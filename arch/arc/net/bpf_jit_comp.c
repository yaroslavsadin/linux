#include <linux/filter.h>

/* ARC core registers. */
enum {
	ARC_R_0 , ARC_R_1 , ARC_R_2 , ARC_R_3 , ARC_R_4 , ARC_R_5,
	ARC_R_6 , ARC_R_7 , ARC_R_8 , ARC_R_9 , ARC_R_10, ARC_R_11,
	ARC_R_12, ARC_R_13, ARC_R_14, ARC_R_15, ARC_R_16, ARC_R_17,
	ARC_R_18, ARC_R_19, ARC_R_20, ARC_R_21, ARC_R_22, ARC_R_23,
	ARC_R_24, ARC_R_25, ARC_R_26, ARC_R_FP, ARC_R_SP, ARC_R_ILINK,
	ARC_R_30, ARC_R_BLINK,
	ARC_R_IMM = 62
};

/*
 * bpf2arc array maps BPF registers to ARC registers. However, that is not
 * all and in some cases we need an extra temporary register to perform
 * the operations. This temporary register is added as yet another index
 * in the bpf2arc array, so it will unfold like the rest of registers into
 * the final JIT.
 */
#define JIT_REG_TMP MAX_BPF_JIT_REG

static const u8 bpf2arc[][2] = {
	/* Return value from in-kernel function, and exit value from eBPF */
	[BPF_REG_0] = {ARC_R_0 , ARC_R_1},
	/* Arguments from eBPF program to in-kernel function */
	[BPF_REG_1] = {ARC_R_2 , ARC_R_3},
	[BPF_REG_2] = {ARC_R_4 , ARC_R_5},
	/* Remaining arguments, to be passed on the stack per O32 ABI */
	[BPF_REG_3] = {ARC_R_6 , ARC_R_7},
	[BPF_REG_4] = {ARC_R_8 , ARC_R_9},
	[BPF_REG_5] = {ARC_R_10, ARC_R_11},
	/* Callee-saved registers that in-kernel function will preserve */
	[BPF_REG_6] = {ARC_R_12, ARC_R_13},
	[BPF_REG_7] = {ARC_R_14, ARC_R_15},
	[BPF_REG_8] = {ARC_R_16, ARC_R_17},
	[BPF_REG_9] = {ARC_R_18, ARC_R_19},
	/* Read-only frame pointer to access the eBPF stack. 32-bit only. */
	[BPF_REG_FP] = {ARC_R_FP, },
	/* Register for blinding constants */
	[BPF_REG_AX] = {ARC_R_22, ARC_R_23},
	/* Temporary registers for internal use */
	[JIT_REG_TMP] = {ARC_R_20, ARC_R_21},
};

#define REG_LO(r) (bpf2arc[(r)][0])
#define REG_HI(r) (bpf2arc[(r)][1])

/* Bytes. */
enum {
	INSN_len_short = 2,	/* Short instructions length. */
	INSN_len_normal = 4,	/* Normal instructions length. */
	INSN_len_imm = 4	/* Length of an extra 32-bit immediate. */
};

/* ZZ defines the size of operation in encodings that it is used. */
enum {
	ZZ_1_byte = 1,
	ZZ_2_byte = 2,
	ZZ_4_byte = 0,
	ZZ_8_byte = 3
};

/*
 * AA is mostly about address write back mode. It determines if the
 * address in question should be updated before usage or after:
 *   addr += offset; data = *addr;
 *   data = *addr; addr += offset;
 *
 * In "scaling" mode, the effective address will become the sum
 * of "address" + "index"*"size". The "size" is specified by the
 * "ZZ" field. There is no write back when AA is set for scaling:
 *   data = *(addr + offset<<zz)
 */
enum {
	AA_none  = 0,
	AA_pre   = 1,	/* in assembly known as "a/aw". */
	AA_post  = 2,	/* in assembly known as "ab". */
	AA_scale = 3	/* in assembly known as "as". */
};

/* D flag specifies how a memory access should be done. */
enum {
	D_cached = 0,
	D_direct = 1
};

/* X flag determines the mode of extension. */
enum {
	X_zero = 0,
	X_sign = 1
};

/* Condition codes. */
enum {
	CC_always = 0,		/* condition is true all the time. */
	CC_equal = 1,		/* if status32.z flag is set. */
	CC_unequal = 2,		/* if status32.z flag is clear. */
	CC_positive = 3,	/* if status32.n flag is clear. */
	CC_negative = 4,	/* if status32.n flag is set. */
};

#define IN_S9_RANGE(x)	((x) <= 255 && (x) >= -256)

/* Operands in most of the encodings. */
#define OP_A(x)	((x) & 0x03f)
#define OP_B(x)	((((x) & 0x07) << 24) | (((x) & 0x38) <<  9))
#define OP_C(x)	(((x) & 0x03f) << 6)

/*
 * The 4-byte encoding of "add a,b,c":
 *
 * 0010_0bbb 0000_0000 fBBB_cccc ccaa_aaaa
 *
 * f:                   indicates if flags (carry, etc.) should be updated
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define ADD_OPCODE	0x20000000
#define ADD_F(x)	(((x) & 1) << 15)
/* Addition with updating the pertinent flags in "status32" register. */
#define OPC_ADD_F \
	ADD_OPCODE | ADD_F(1)

/*
 * The 4-byte encoding of "adc a,b,c" (addition with carry):
 *
 * 0010_0bbb 0000_0001 0BBB_cccc ccaa_aaaa
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define ADC_OPCODE	0x20010000

/*
 * The 4-byte encoding of "sub a,b,c":
 *
 * 0010_0bbb 0000_0010 fBBB_cccc ccaa_aaaa
 *
 * f:                   indicates if flags (carry, etc.) should be updated
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define SUB_OPCODE	0x20020000
#define SUB_F(x)	(((x) & 1) << 15)
/* Subtraction with updating the pertinent flags in "status32" register. */
#define OPC_SUB_F \
	SUB_OPCODE | SUB_F(1)

/*
 * The 4-byte encoding of "sbc a,b,c" (subtraction with carry):
 *
 * 0010_0bbb 0000_0011 0BBB_cccc ccaa_aaaa
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define SBC_OPCODE	0x20030000

/*
 * The 4-byte encoding of "xor a,b,c":
 *
 * 0010_0bbb 0000_0111 0BBB_cccc ccaa_aaaa
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define XOR_OPCODE	0x20070000

/*
 * The 4-byte encoding of "asl a,b,c":
 *
 * 0010_1bbb 0i00_0000 0BBB_cccc ccaa_aaaa
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		number to be shifted
 * c:  cccccc		amount to be shifted
 * i:			if set, c is considered a 6-bit immediate, else a reg.
 */
#define ASL_OPCODE	0x28000000
#define ASL_I(x)	(((x) & 1) << 22)
#define OPC_ASLI	ASL_OPCODE | ASL_I(1)

/*
 * The 4-byte encoding of "mov b,c":
 *
 * 0010_0bbb 0000_1010 0BBB_cccc cc00_0000
 *
 * b:  BBBbbb		destination register
 * c:  cccccc		source register
 */
#define MOVE_OPCODE	0x200a0000

/*
 * The 4-byte encoding of "ld[zz][.x][.aa][.di] c,[b,s9]":
 *
 * 0001_0bbb ssss_ssss SBBB_daaz zxcc_cccc
 *
 * zz:			size mode
 * aa:			address write back mode
 * d:			memory access mode
 * x:			extension mode
 *
 * s9: S_ssss_ssss	9-bit signed number
 * b:  BBBbbb		source reg for address
 * c:  cccccc		destination register
 */
#define LOAD_OPCODE	0x10000000
#define LOAD_X(x)	((x) << 6)
#define LOAD_ZZ(x)	((x) << 7)
#define LOAD_AA(x)	((x) << 9)
#define LOAD_D(x)	((x) << 11)
#define LOAD_S9(x)	((((x) & 0x0ff) << 16) | (((x) & 0x100) <<  7))
#define LOAD_C(x)	((x) & 0x03f)
/* Generic load. */
#define OPC_LD		LOAD_OPCODE | LOAD_D(D_cached) | LOAD_X(X_zero)
/* 32-bit load. */
#define OPC_LD32	OPC_LD | LOAD_ZZ(ZZ_4_byte)
/* "pop reg" is merely a "ld.ab reg,[sp,4]". */
#define OPC_POP		\
	OPC_LD32 | LOAD_AA(AA_post) | LOAD_S9(4) | OP_B(ARC_R_SP)

/*
 * The 4-byte encoding of "st[zz][.aa][.di] c,[b,s9]":
 *
 * 0001_1bbb ssss_ssss SBBB_cccc ccda_azz0
 *
 * zz: zz		size mode
 * aa: aa		address write back mode
 * d:  d		memory access mode
 *
 * s9: S_ssss_ssss	9-bit signed number
 * b:  BBBbbb		source reg for address
 * c:  cccccc		source reg to be stored
 */
#define STORE_OPCODE	0x18000000
#define STORE_ZZ(x)	((x) << 1)
#define STORE_AA(x)	((x) << 3)
#define STORE_D(x)	((x) << 5)
#define STORE_S9(x)	((((x) & 0x0ff) << 16) | (((x) & 0x100) <<  7))
/* Generic store. */
#define OPC_ST		STORE_OPCODE | STORE_D(D_cached)
/* 32-bit store. */
#define OPC_ST32	OPC_ST | STORE_ZZ(ZZ_4_byte)
/* "push reg" is merely a "st.aw reg,[sp,-4]". */
#define OPC_PUSH	\
	OPC_ST32 | STORE_AA(AA_pre) | STORE_S9(-4) | OP_B(ARC_R_SP)

/*
 * Encoding for jump to an address in register:
 * j reg_c
 *
 * 0010_0000 1110_0000 0000_cccc cc00_0000
 *
 * c:  cccccc		register holding the destination address
 */
#define JMP_OPCODE	0x20e00000
/* Jump to "branch-and-link" register, which effectively is a "return". */
#define OPC_J_BLINK	JMP_OPCODE | OP_C(ARC_R_BLINK)

/*
 * Encoding for jump-and-link to an address in register:
 * jl reg_c
 *
 * 0010_0000 0010_0010 0000_cccc cc00_0000
 *
 * c:  cccccc		register holding the destination address
 */
#define JL_OPCODE	0x20220000

/*
 * TODO: remove me.
 * Dumps bytes in /var/log/messages at KERN_INFO level (4).
 */
static void dump_bytes(const u8 *buf, u32 len, bool jit)
{
	u8 line[256];
	size_t i, j;

	if (jit)
		pr_info("-----------------[ jited ]-----------------\n");
	else
		pr_info("-----------------[  VM   ]-----------------\n");

	for (i = 0, j = 0; i < len; i++) {
		if (i == len-1) {
			sprintf(line+j, "0x%02x" , buf[i]);
			pr_info("%s\n", line);
			break;
		}
		else if (i % 8 == 7) {
			sprintf(line+j, "0x%02x", buf[i]);
			pr_info("%s\n", line);
			j = 0;
		} else {
			j += sprintf(line+j, "0x%02x, ", buf[i]);
		}
	}

	if (jit)
		pr_info("\n");
}

/*
 * If "emit" is true, the instructions are actually generated. Else, the
 * generation part will be skipped and only the length of instruction is
 * returned by the responsible functions.
 */
static bool emit = false;

static inline void emit_2_bytes(u8 *buf, u16 bytes)
{
	*((u16 *) buf) = bytes;
}

static inline void emit_4_bytes(u8 *buf, u32 bytes)
{
	emit_2_bytes(buf+0, bytes >>     16);
	emit_2_bytes(buf+2, bytes  & 0xffff);
}

static inline u8 bpf_to_arc_size(u8 size)
{
	switch (size) {
	case BPF_B:
		return ZZ_1_byte;
	case BPF_H:
		return ZZ_2_byte;
	case BPF_W:
		return ZZ_4_byte;
	case BPF_DW:
		return ZZ_8_byte;
	default:
		return ZZ_4_byte;
	}
}

/*********************** Encoders ************************/

static u8 arc_add_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = ADD_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_add_f_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = OPC_ADD_F | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_add_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (emit) {
		u32 insn = ADD_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

static u8 arc_adc_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = ADC_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_sub_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = SUB_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_sub_f_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = OPC_SUB_F | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_sub_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (emit) {
		u32 insn = SUB_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

static u8 arc_sbc_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = SBC_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_xor_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = XOR_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_xor_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (emit) {
		u32 insn = XOR_OPCODE | OP_A(reg_dst) | OP_B(reg_dst) |
			   OP_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

/* "mov r, 0" in fact is a "xor r, r, r". */
static u8 arc_mov_0(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = XOR_OPCODE | OP_A(reg) | OP_B(reg) | OP_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_mov_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = MOVE_OPCODE | OP_B(reg_dst) | OP_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_mov_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (imm == 0)
		return arc_mov_0(buf, reg_dst);

	if (emit) {
		u32 insn = MOVE_OPCODE | OP_B(reg_dst) | OP_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

/* st.as reg_c, [reg_b, off] */
static u8 arc_st_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 zz)
{
	if (emit) {
		u32 insn = OPC_ST | STORE_AA(AA_none) | STORE_ZZ(zz) |
			   OP_C(reg) | OP_B(reg_mem) | STORE_S9(off);
		emit_4_bytes(buf, insn);
	}

	return INSN_len_normal;
}

/* "push reg" is merely a "st.aw reg_c, [sp, -4]". */
static u8 arc_push_r(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = OPC_PUSH | OP_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

/* ld.aw reg_c, [reg_b, off] */
static u8 arc_ld_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 zz)
{
	if (emit) {
		u32 insn = OPC_LD | LOAD_AA(AA_none) | LOAD_ZZ(zz) |
			   LOAD_C(reg) | OP_B(reg_mem) | LOAD_S9(off);
		emit_4_bytes(buf, insn);
	}

	return INSN_len_normal;
}

static u8 arc_pop_r(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = OPC_POP | LOAD_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_jmp_return(u8 *buf)
{
	if (emit)
		emit_4_bytes(buf, OPC_J_BLINK);
	return INSN_len_normal;
}

static u8 arc_jl(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = JL_OPCODE | OP_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

/*********************** Packers *************************/

static u8 add_r32(u8 *buf, u8 reg_dst, u8 reg_src)
{
	return arc_add_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
}

static u8 add_r32_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	return arc_add_i(buf, REG_LO(reg_dst), imm);
}

static u8 add_r64(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len;
	len  = arc_add_f_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
	len += arc_adc_r(buf+len, REG_HI(reg_dst), REG_HI(reg_src));
	return len;
}

static u8 mov_r64_i32(u8 *, u8, s32);

static u8 add_r64_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	u8 len;
	len  = mov_r64_i32(buf, JIT_REG_TMP, imm);
	len += add_r64(buf+len, reg_dst, JIT_REG_TMP);
	return len;
}

static u8 sub_r32(u8 *buf, u8 reg_dst, u8 reg_src)
{
	return arc_sub_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
}

static u8 sub_r32_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	return arc_sub_i(buf, REG_LO(reg_dst), imm);
}

static u8 sub_r64(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len;
	len  = arc_sub_f_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
	len += arc_sbc_r(buf+len, REG_HI(reg_dst), REG_HI(reg_src));
	return len;
}

static u8 sub_r64_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	u8 len;
	len  = mov_r64_i32(buf, JIT_REG_TMP, imm);
	len += sub_r64(buf+len, reg_dst, JIT_REG_TMP);
	return len;
}

static u8 xor_r32(u8 *buf, u8 reg_dst, u8 reg_src)
{
	return arc_xor_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
}

static u8 xor_r32_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	return arc_xor_i(buf, REG_LO(reg_dst), imm);
}

static u8 xor_r64(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len;
	len  = arc_xor_r(buf    , REG_LO(reg_dst), REG_LO(reg_src));
	len += arc_xor_r(buf+len, REG_HI(reg_dst), REG_HI(reg_src));
	return len;
}

static u8 xor_r64_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	u8 len;
	len  = mov_r64_i32(buf, JIT_REG_TMP, imm);
	len += xor_r64(buf+len, reg_dst, JIT_REG_TMP);
	return len;
}

static u8 mov_r64(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len;

	if (reg_dst == reg_src)
		return 0;

	len  = arc_mov_r(buf, REG_LO(reg_dst), REG_LO(reg_src));

	if (reg_src != BPF_REG_FP)
		len += arc_mov_r(buf+len, REG_HI(reg_dst), REG_HI(reg_src));
	/* BPF_REG_FP is mapped to 32-bit "fp" register. */
	else
		len += arc_mov_0(buf+len, REG_HI(reg_dst));

	return len;
}

/* sign extend the 32-bit immediate into 64-bit register pair. */
static u8 mov_r64_i32(u8 *buf, u8 reg, s32 imm)
{
	u8 len = 0;

	len = arc_mov_i(buf, REG_LO(reg), imm);
	if (imm >= 0)
		len += arc_mov_0(buf+len, REG_HI(reg));
	else
		len += arc_mov_i(buf+len, REG_HI(reg), -1);

	return len;
}

static u8 mov_r64_i64(u8 *buf, u8 reg, u32 lo, u32 hi)
{
	u8 len;

	len  = arc_mov_i(buf, REG_LO(reg), lo);
	len += arc_mov_i(buf+len, REG_HI(reg), hi);

	return len;
}

/*
 * If the offset is too big to fit in s9, emit:
 *   mov r20, reg
 *   add r20, r20, off
 * and make sure that r20 will be the effective address for store:
 *   st  r, [r20, 0]
 */
static u8 correct_for_offset(u8 *buf, s16 *off, u8 size,
			     u8 reg_mem, u8 *arc_reg_mem)
{
	u8 len = 0;

	if (!IN_S9_RANGE(*off) ||
	    (size == BPF_DW && !IN_S9_RANGE(*off + 4))) {
		*arc_reg_mem = REG_LO(JIT_REG_TMP);
		len  = arc_mov_r(buf    , *arc_reg_mem, reg_mem);
		len += arc_add_i(buf+len, *arc_reg_mem, (u32) off);
		*off = 0;
	}

	return len;
}

static u8 store_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 size)
{
	u8 arc_reg_mem = REG_LO(reg_mem);
	u8 len;

	len = correct_for_offset(buf, &off, size, reg_mem, &arc_reg_mem);

	if (size == BPF_DW) {
		len += arc_st_r(buf+len, REG_LO(reg), arc_reg_mem, off,
				ZZ_4_byte);
		len += arc_st_r(buf+len, REG_HI(reg), arc_reg_mem, off+4,
				ZZ_4_byte);
	} else {
		u8 zz = bpf_to_arc_size(size);
		len += arc_st_r(buf+len, REG_LO(reg), arc_reg_mem, off, zz);
	}

	return len;
}

/*
 * For {8,16,32}-bit stores:
 *   mov r21, imm
 *   st  r21, [...]
 * For 64-bit stores:
 *   mov r21, imm
 *   st  r21, [...]
 *   mov r21, {0,-1}
 *   st  r21, [...+4]
 */
static u8 store_i(u8 *buf, s32 imm, u8 reg_mem, s16 off, u8 size)
{
	/* REG_LO(JIT_REG_TMP) might be used by "correct_for_offset()". */
	const u8 arc_reg_src = REG_HI(JIT_REG_TMP);
	u8 arc_reg_mem = REG_LO(reg_mem);
	u8 len;

	len = correct_for_offset(buf, &off, size, reg_mem, &arc_reg_mem);

	if (size == BPF_DW) {
		len += arc_mov_i(buf+len, arc_reg_src, imm);
		len += arc_st_r(buf+len, arc_reg_src, arc_reg_mem, off,
				ZZ_4_byte);
		imm = (imm >= 0 ? 0 : -1);
		len += arc_mov_i(buf+len, arc_reg_src, imm);
		len += arc_st_r(buf+len, arc_reg_src, arc_reg_mem, off+4,
				ZZ_4_byte);
	} else {
		u8 zz = bpf_to_arc_size(size);
		len += arc_mov_i(buf+len, arc_reg_src, imm);
		len += arc_st_r(buf+len, arc_reg_src, arc_reg_mem, off, zz);
	}

	return len;
}

static u8 push_r64(u8 *buf, u8 reg)
{
	u8 len;

	/* BPF_REG_FP is mapped to 32-bit "fp" register. */
	if (reg == BPF_REG_FP)
		return arc_push_r(buf, REG_LO(reg));

	len  = arc_push_r(buf    , REG_LO(reg));
	len += arc_push_r(buf+len, REG_HI(reg));

	return len;
}

static u8 push_blink(u8 *buf)
{
	return arc_push_r(buf, ARC_R_BLINK);
}

static u8 load_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 size)
{
	u8 len = 0;
	u8 arc_reg_mem = REG_LO(reg_mem);

	/*
	 * If the offset is too big to fit in s9, emit:
	 *   mov r20, reg
	 *   add r20, r20, off
	 * and make sure that r20 will be the effective address for the "load".
	 *   ld  r, [r20, 0]
	 */
	if (!IN_S9_RANGE(off) ||
	    (size == BPF_DW && !IN_S9_RANGE(off + 4))) {
		arc_reg_mem = REG_LO(JIT_REG_TMP);
		len  = arc_mov_r(buf    , arc_reg_mem, reg_mem);
		len += arc_add_i(buf+len, arc_reg_mem, (u32) off);
		off = 0;
	}

	if (size == BPF_B || size == BPF_H || size == BPF_W) {
		u8 zz = bpf_to_arc_size(size);
		len += arc_ld_r(buf+len, REG_LO(reg), arc_reg_mem, off, zz);
		len += arc_mov_0(buf+len, REG_HI(reg));
	} else if (size == BPF_DW) {
		/*
		 * We are about to issue 2 consecutive loads:
		 *
		 *   ld rx, [rb, off+0]
		 *   ld ry, [rb, off+4]
		 *
		 * If "rx" and "rb" are the same registers, then the order
		 * should change to guarantee that "rb" remains intact
		 * during these 2 operations:
		 *
		 *   ld ry, [rb, off+4]
		 *   ld rx, [rb, off+0]
		 */
		if (REG_LO(reg) != arc_reg_mem) {
			len += arc_ld_r(buf+len, REG_LO(reg), arc_reg_mem,
					off+0, ZZ_4_byte);
			len += arc_ld_r(buf+len, REG_HI(reg), arc_reg_mem,
					off+4, ZZ_4_byte);
		} else {
			len += arc_ld_r(buf+len, REG_HI(reg), arc_reg_mem,
					off+4, ZZ_4_byte);
			len += arc_ld_r(buf+len, REG_LO(reg), arc_reg_mem,
					off+0, ZZ_4_byte);
		}
	}

	return len;
}

static u8 pop_r64(u8 *buf, u8 reg)
{
	u8 len;

	/* BPF_REG_FP is mapped to 32-bit "fp" register. */
	if (reg == BPF_REG_FP)
		return arc_pop_r(buf, REG_LO(reg));

	len  = arc_pop_r(buf    , REG_LO(reg));
	len += arc_pop_r(buf+len, REG_HI(reg));

	return len;
}

static u8 pop_blink(u8 *buf)
{
	return arc_pop_r(buf, ARC_R_BLINK);
}

/*
 * To create a frame, all that is needed is:
 *
 *  push fp
 *  mov  fp, sp
 *  sub  sp, <frame_size>
 *
 * "push fp" is taken care of separately while saving the clobbered registers.
 * All that remains is copying SP value to FP and shrinking SP's address space
 * for any possible function call to come.
 */
static u8 enter_frame(u8 *buf, u16 size)
{
	u8 len;
	len  = arc_mov_r(buf, ARC_R_FP, ARC_R_SP);
	len += arc_sub_i(buf+len, ARC_R_SP, size);
	return len;
}

/* The value of SP upon entering was copied to FP. */
static u8 exit_frame(u8 *buf)
{
	return arc_mov_r(buf, ARC_R_SP, ARC_R_FP);
}

static u8 jump_return(u8 *buf)
{
	return arc_jmp_return(buf);
}

/*
 * This translation leads to:
 *
 *   mov r20, addr
 *   jl  r20
 *
 * Here "addr" is u32, but for arc_mov_i(), it is s32.
 * The lack of an explicit conversion is OK.
 */
static u8 jump_and_link(u8 *buf, u32 addr)
{
	u8 len;
	len  = arc_mov_i(buf, REG_LO(JIT_REG_TMP), addr);
	len += arc_jl(buf+len, REG_LO(JIT_REG_TMP));
	return len;
}

/********************* JIT context ***********************/

/*
 * buf:		Translated instructions end up here.
 * len:		The length of whole block in bytes.
 * index:	The offset at which the _next_ instruction may be put.
 */
struct jit_buffer
{
	u8	*buf;
	u32	len;
	u32	index;
};

/*
 * This is a subset of "struct jit_context" that its information is deemed
 * necessary for the next extra pass to come.
 *
 * bpf_header:	Needed to finally lock the region.
 * bpf2insn:	Used to find the translation of "call" instructions.
 *
 * Things like "jit.buf" and "jit.len" can be retrieved respectively from
 * "prog->bpf_func" and "prog->jited_len".
 */
struct arc_jit_data
{
	struct bpf_binary_header *bpf_header;
	u32                      *bpf2insn;
};

/*
 * The JIT pertinent context that is used by different functions.
 *
 * prog:		The current eBPF program being handled.
 * orig_prog:		The original eBPF program before any possible change.
 * jit:			The JIT buffer and its length.
 * bpf_header:		The JITed program header. "jit.buf" points inside it.
 * bpf2insn:		Maps BPF insn indices to their counterparts in jit.buf.
 * jit_data:		A piece of memory to transfer data to the next pass.
 * regs_clobbered:	Each bit status determines if that BPF reg is clobbered.
 * save_blink:		If ARC's "blink" register needs to be saved.
 * stack_depth:		Derived from FP accesses (fp-4, fp-8, ...).
 * epilogue_offset:	Used by early "return"s in the code to jump here.
 * need_extra_pass:	A forecast if an "extra_pass" will occur.
 * blinded:		True if "constant blinding" step returned a new "prog".
 * success:		Indicates if the whole JIT went OK.
 */
struct jit_context
{
	struct bpf_prog			*prog;
	struct bpf_prog			*orig_prog;
	struct jit_buffer		jit;
	struct bpf_binary_header	*bpf_header;
	u32				*bpf2insn;
	struct arc_jit_data		*jit_data;
	u16				regs_clobbered;
	bool				save_blink;
	u16				stack_depth;
	u32				epilogue_offset;
	bool				need_extra_pass;
	bool				blinded;
	bool				success;
};

static int jit_ctx_init(struct jit_context *ctx, struct bpf_prog *prog)
{
	ctx->orig_prog = prog;

	/* If constant blinding was requested but failed, scram. */
	ctx->prog = bpf_jit_blind_constants(prog);
	if (IS_ERR(ctx->prog))
		return PTR_ERR(ctx->prog);
	ctx->blinded = (ctx->prog == ctx->orig_prog ? false : true);

	ctx->jit.buf         = NULL;
	ctx->jit.len         = 0;
	ctx->jit.index       = 0;
	ctx->bpf_header      = NULL;
	ctx->bpf2insn        = NULL;
	ctx->regs_clobbered  = 0;
	ctx->save_blink      = false;
	ctx->stack_depth     = 0;
	ctx->epilogue_offset = 0;
	ctx->need_extra_pass = false;
	ctx->success         = false;

	return 0;
}

/*
 * "*mem" should be freed when there is no "extra pass" to come,
 * or the compilation terminated abruptly. A few of such memory
 * allocations are: ctx->jit_data and ctx->bpf2insn.
 */
static inline void maybe_free(struct jit_context *ctx, void **mem)
{
	if (*mem) {
		if (!ctx->success || !ctx->need_extra_pass) {
			kfree(*mem);
			*mem = NULL;
		}
	}
}

/*
 * Free memories based on the status of the context.
 *
 * A note about "bpf_header": On successful runs, "bpf_header" is
 * not freed, because "jit.buf", a sub-array of it, is returned as
 * the "bpf_func". However, "bpf_header" is lost and nothing points
 * to it. This should not cause a leakage, because apparently
 * "bpf_header" can be revived by "bpf_jit_binary_hdr()". This is
 * how "bpf_jit_free()" in "kernel/bpf/core.c" releases the memory.
 */
static void jit_ctx_cleanup(struct jit_context *ctx)
{
	if (ctx->blinded) {
		/* if all went well, release the orig_prog. */
		if (ctx->success)
			bpf_jit_prog_release_other(ctx->prog, ctx->orig_prog);
		else
			bpf_jit_prog_release_other(ctx->orig_prog, ctx->prog);
	}

	maybe_free(ctx, (void **) &ctx->bpf2insn);
	maybe_free(ctx, (void **) &ctx->jit_data);

	/* Freeing "bpf_header" is enough. "jit.buf" is a sub-array of it. */
	if (!ctx->success && ctx->bpf_header) {
		bpf_jit_binary_free(ctx->bpf_header);
		ctx->bpf_header = NULL;
		ctx->jit.buf    = NULL;
		ctx->jit.index  = 0;
		ctx->jit.len    = 0;
	}
}

/*
 * Goes through all the instructions and checks if any of the callee-saved
 * registers are clobbered. If yes, the corresponding bit position of that
 * register is set to true.
 */
static void analyze_reg_usage(struct jit_context *ctx)
{
	u16 usage = 0;
	s16 depth = 0;	/* Will be "min()"ed against negative numbers. */
	bool call_exists = false;
	size_t i;
	const struct bpf_insn *insn = ctx->prog->insnsi;

	for (i = 0; i < ctx->prog->len; i++) {
		if (insn[i].dst_reg == BPF_REG_6)
			usage |= BIT(BPF_REG_6);
		else if (insn[i].dst_reg == BPF_REG_7)
			usage |= BIT(BPF_REG_7);
		else if (insn[i].dst_reg == BPF_REG_8)
			usage |= BIT(BPF_REG_8);
		else if (insn[i].dst_reg == BPF_REG_9)
			usage |= BIT(BPF_REG_9);

		/*
		 * Reading the frame pointer register implies that it should
		 * be saved and reinitialised with the current frame data.
		 */
		if (insn[i].dst_reg == BPF_REG_FP) {
			const u8 store_mem_mask = 0x67;
			const u8 code_mask = insn[i].code & store_mem_mask;
			usage |= BIT(BPF_REG_FP);
			/* Is FP usage in the form of "*(FP + off) = data"? */
			if (code_mask == (BPF_STX | BPF_MEM)) {
				/* Then, record the deepest depth. */
				depth = min(depth, insn[i].off);
			}
		}

		/* A "call" indicates that ARC's "blink" reg must be saved. */
		call_exists |= (insn[i].code == (BPF_JMP | BPF_CALL));
	}

	ctx->regs_clobbered = usage;
	ctx->save_blink     = call_exists;
	ctx->stack_depth    = abs(depth);
}

/* Verify that no instruction will be emitted when there is no buffer. */
static inline int jit_buffer_check(const struct jit_buffer *jbuf)
{
	if (emit == true) {
		if (jbuf->buf == NULL) {
			pr_err("bpf-jit: inconsistence state; no "
			       "buffer to emit instructions.\n");
			return -EINVAL;
		} else if (jbuf->index > jbuf->len) {
			pr_err("bpf-jit: estimated JIT length is less "
			       "than the emitted instructions.\n");
			return -EFAULT;
		}
	}
	return 0;
}

/* On a dry run (emit=false), "jit.len" is growing gradually. */
static inline void jit_buffer_update(struct jit_buffer *jbuf, u32 n)
{
	if (!emit)
		jbuf->len += n;
	else
		jbuf->index += n;
}

/* Based on "emit", determine the address where instructions are emitted. */
static inline u8 *effective_jit_buf(const struct jit_buffer *jbuf)
{
	return (emit ? jbuf->buf + jbuf->index : NULL);
}

/*
 * If "emit" is true, all the necessary "push"s are generated. Else, it acts
 * as a dry run and only updates the length of would-have-been instructions.
 */
static int handle_prologue(struct jit_context *ctx)
{
	int ret;
	u16 push_mask = ctx->regs_clobbered;
	u8 *buf = effective_jit_buf(&ctx->jit);
	u32 len = 0;

	if ((ret = jit_buffer_check(&ctx->jit)))
	    return ret;

	while (push_mask) {
		u8 reg = __builtin_ffs(push_mask) - 1;

		if (reg < BPF_REG_6 || reg > BPF_REG_FP) {
			pr_err("bpf-jit: invalid register for prologue %u\n",
			       reg);
			return -EINVAL;
		}

		len += push_r64(buf+len, reg);
		push_mask &= ~BIT(reg);
	}

	if (ctx->save_blink)
		len += push_blink(buf+len);

	if (ctx->stack_depth)
		len += enter_frame(buf+len, ctx->stack_depth);

	jit_buffer_update(&ctx->jit, len);

	return 0;
}

/*
 * The counter part for "handle_prologue()". If this function is asked to emit
 * instructions then it continues with "jit.index". If no instruction is
 * supposed to be emitted, it means it should contribute to the calculation of
 * "jit.len", and therefore it begins with that.
 */
static int handle_epilogue(struct jit_context *ctx)
{
	int ret;
	u32 pop_mask = ctx->regs_clobbered;
	u8 *buf = effective_jit_buf(&ctx->jit);
	u32 len = 0;

	if ((ret = jit_buffer_check(&ctx->jit)))
	    return ret;

	if (ctx->stack_depth)
		len += exit_frame(buf+len);

	if (ctx->save_blink)
		len += pop_blink(buf+len);

	while (pop_mask) {
		u8 reg = 31 - __builtin_clz(pop_mask);

		if (reg < BPF_REG_6 || reg > BPF_REG_FP) {
			pr_err("bpf-jit: invalid register for epilogue %u\n",
			       reg);
			return -EINVAL;
		}

		len += pop_r64(buf+len, reg);
		pop_mask &= ~BIT(reg);
	}

	/* At last, issue the "return". */
	len += jump_return(buf+len);

	jit_buffer_update(&ctx->jit, len);

	return 0;
}

/* Try to get the resolved the address and generate the instructions. */
static int gen_call(struct jit_context *ctx, const struct bpf_insn *insn,
		       bool extra_pass, u8 *len)
{
	int  ret;
	bool fixed = false;
	u64  addr = 0;
	u8  *buf = effective_jit_buf(&ctx->jit);

	ret = bpf_jit_get_func_addr(ctx->prog, insn, extra_pass, &addr, &fixed);
	if (ret < 0) {
		pr_err("bpf-jit: can't get the address for call.");
		return ret;
	}

	/* No valuble address retrieved (yet). */
	if (!fixed && !addr)
		ctx->need_extra_pass = true;

	*len = jump_and_link(buf, (u32) addr);
	return 0;
}

static inline bool is_last_insn(const struct bpf_prog *prog, u32 idx)
{
	return (idx == (prog->len - 1));
}

/*
 * Handles one eBPF instruction at a time. To make this function faster,
 * it does not call "jit_buffer_check()". Else, it would call it for every
 * instruction. As a result, it should not be invoked directly. Only
 * "handle_body()", that has already executed the verification, may call
 * this function.
 *
 * If the "ret" value is negative, something has went wrong. Else,
 * it mostly holds the value 0 and rarely 1. Number 1 signals
 * the loop in "handle_body()" to skip the next instruction, because
 * it has been consumed as part of a 64-bit immediate value.
 */
static int handle_insn(struct jit_context *ctx, u32 idx)
{
	const struct bpf_insn *insn = &ctx->prog->insnsi[idx];
	u8   code = insn->code;
	u8   dst  = insn->dst_reg;
	u8   src  = insn->src_reg;
	s16  off  = insn->off;
	s32  imm  = insn->imm;
	u8  *buf  = effective_jit_buf(&ctx->jit);
	u8   len  = 0;
	int  ret  = 0;

	switch (code) {
	/* dst += src (32-bit) */
	case BPF_ALU | BPF_ADD | BPF_X:
		len = add_r32(buf, dst, src);
		break;
	/* dst += imm (32-bit) */
	case BPF_ALU | BPF_ADD | BPF_K:
		len = add_r32_i32(buf, dst, imm);
		break;
	/* dst -= src (32-bit) */
	case BPF_ALU | BPF_SUB | BPF_X:
		len = sub_r32(buf, dst, src);
		break;
	/* dst -= imm (32-bit) */
	case BPF_ALU | BPF_SUB | BPF_K:
		len = sub_r32_i32(buf, dst, imm);
		break;
	/* dst ^= src (32-bit) */
	case BPF_ALU | BPF_XOR | BPF_X:
		len = xor_r32(buf, dst, src);
		break;
	/* dst ^= imm (32-bit) */
	case BPF_ALU | BPF_XOR | BPF_K:
		len = xor_r32_i32(buf, dst, imm);
		break;
	/* dst += src (64-bit) */
	case BPF_ALU64 | BPF_ADD | BPF_X:
		len = add_r64(buf, dst, src);
		break;
	/* dst += imm32 (64-bit) */
	case BPF_ALU64 | BPF_ADD | BPF_K:
		len = add_r64_i32(buf, dst, imm);
		break;
	/* dst -= src (64-bit) */
	case BPF_ALU64 | BPF_SUB | BPF_X:
		len = sub_r64(buf, dst, src);
		break;
	/* dst -= imm32 (64-bit) */
	case BPF_ALU64 | BPF_SUB | BPF_K:
		len = sub_r64_i32(buf, dst, imm);
		break;
	/* dst ^= imm32 (64-bit) */
	case BPF_ALU64 | BPF_XOR | BPF_K:
		len = xor_r64_i32(buf, dst, imm);
		break;
	/* dst = src (64-bit) */
	case BPF_ALU64 | BPF_MOV | BPF_X:
		len = mov_r64(buf, dst, src);
		break;
	/* dst = imm32 (sign extend to 64-bit) */
	case BPF_ALU64 | BPF_MOV | BPF_K:
		len = mov_r64_i32(buf, dst, imm);
		break;
	/* dst = imm64 */
	case BPF_LD | BPF_DW | BPF_IMM:
		/* We're about to consume 2 VM instructions. */
		if (is_last_insn(ctx->prog, idx)) {
			pr_err("bpf-jit: need more data for 64-bit immediate.");
			return -EINVAL;
		}
		len = mov_r64_i64(buf, dst, imm, (insn+1)->imm);
		/* Tell the loop to skip the next instruction. */
		ret = 1;
		break;
	/* dst = *(size *)(src + off) */
	case BPF_LDX | BPF_MEM | BPF_W:
	case BPF_LDX | BPF_MEM | BPF_H:
	case BPF_LDX | BPF_MEM | BPF_B:
	case BPF_LDX | BPF_MEM | BPF_DW:
		len = load_r(buf, dst, src, off, BPF_SIZE(code));
		break;
	/* *(size *)(dst + off) = src */
	case BPF_STX | BPF_MEM | BPF_W:
	case BPF_STX | BPF_MEM | BPF_H:
	case BPF_STX | BPF_MEM | BPF_B:
	case BPF_STX | BPF_MEM | BPF_DW:
		len = store_r(buf, src, dst, off, BPF_SIZE(code));
		break;
	case BPF_ST | BPF_MEM | BPF_W:
	case BPF_ST | BPF_MEM | BPF_H:
	case BPF_ST | BPF_MEM | BPF_B:
	case BPF_ST | BPF_MEM | BPF_DW:
		len = store_i(buf, imm, dst, off, BPF_SIZE(code));
		break;
	case BPF_JMP | BPF_CALL:
		/*
		 * If we're here, then "extra_pass" is definitely "false".
		 * When "extra_pass" is true, it leads to a different code
		 * execution than here. In that case, "do_extra_pass()"
		 * takes care of the situation.
		 */
		if ((ret = gen_call(ctx, insn, false, &len)) < 0)
			return ret;
		break;

	case BPF_JMP | BPF_EXIT:
		/* If this is the last instruction, epilogue will follow. */
		if (is_last_insn(ctx->prog, idx))
			break;
		/* TODO: jump to epilogue AND add "break". */
		fallthrough;
	default:
		pr_err("bpf-jit: can't handle instruction code 0x%02X\n", code);
		return -ENOTSUPP;
	}

	jit_buffer_update(&ctx->jit, len);

	return ret;
}

static int handle_body(struct jit_context *ctx)
{
	int ret;
	const struct bpf_prog *prog = ctx->prog;

	if ((ret = jit_buffer_check(&ctx->jit)))
	    return ret;

	for (u32 i = 0; i < prog->len; i++) {

		/*
		 * Record the mapping for the instructions during the
		 * dry-run, as "jit.len" grows gradually per instruction.
		 *
		 * Doing it this way allows us to have the mapping ready
		 * for the jump instructions during the real compilation
		 * phase.
		 */
		if (!emit)
			ctx->bpf2insn[i] = ctx->jit.len;

		if ((ret = handle_insn(ctx, i)) < 0)
			return ret;

		/* "ret" holds 1 if two (64-bit) chunks were consumed. */
		i += ret;
	}

	return 0;
}

/*
 * Initialize the memory with "unimp_s" which is the mnemonic for
 * "unimplemented" instruction and always raises an exception.
 *
 * The instruction is 2 bytes. If "size" is odd, there is not much
 * that can be done about the last byte in "area". Because, the
 * CPU always fetches instructions in two bytes. Therefore, the
 * byte beyond the last one is going to accompany it during a
 * possible fetch. In the most likely case of a little endian
 * system, that beyond-byte will become the major opcode and
 * we have no control over its initialisation.
 */
static void fill_ill_insn(void *area, unsigned int size)
{
	const u16 unimp_s = 0x79e0;

	if (size & 1) {
		*((u8 *) area + (size - 1)) = 0xff;
		size -= 1;
	}

	memset16(area, unimp_s, size >> 1);
}

/* Piece of memory that can be allocated at the begining of jit_prepare(). */
static int jit_prepare_early_mem_alloc(struct jit_context *ctx)
{
	ctx->bpf2insn = kcalloc(ctx->prog->len, sizeof(ctx->jit.len),
				GFP_KERNEL);

	if (!ctx->bpf2insn) {
		pr_err("bpf-jit: could not allocate memory for "
		       "mapping of the instructions.\n");
		return -ENOMEM;
	}

	return 0;
}

/*
 * Memory allocations that rely on parameters known at the
 * end of jit_prepare().
 */
static int jit_prepare_final_mem_alloc(struct jit_context *ctx)
{
	ctx->bpf_header = bpf_jit_binary_alloc(ctx->jit.len, &ctx->jit.buf,
					       INSN_len_normal, fill_ill_insn);

	if (!ctx->bpf_header) {
		pr_err("bpf-jit: could not allocate memory for translation.\n");
		return -ENOMEM;
	}

	if (ctx->need_extra_pass) {
		ctx->jit_data = kzalloc(sizeof(struct arc_jit_data),
					GFP_KERNEL);
		if (!ctx->jit_data) {
			pr_err("bpf-jit: could not allocate memory for "
			       "the next pass's data.\n");
			return -ENOMEM;
		}
	}

	return 0;
}

/*
 * The first phase of the translation without actually emitting any
 * instruction. It helps in getting a forecast on some aspects, such
 * as the length of the whole program or where the epilogue starts.
 *
 * Whenever the necessary parameters are known, memories are allocated.
 */
static int jit_prepare(struct jit_context *ctx)
{
	int ret;

	/* Dry run. */
	emit = false;

	if ((ret = jit_prepare_early_mem_alloc(ctx)))
		return ret;

	/* Get the length of prologue section after some register analysis. */
	analyze_reg_usage(ctx);
	if ((ret = handle_prologue(ctx)))
		return ret;

	if ((ret = handle_body(ctx)))
		return ret;

	/* Record at which offset epilogue begins. */
	ctx->epilogue_offset = ctx->jit.len;

	/* Add the epilogue's length as well. */
	if ((ret = handle_epilogue(ctx)))
		return ret;

	if ((ret = jit_prepare_final_mem_alloc(ctx)))
		return ret;

	return 0;
}

/*
 * All the "handle_*()" functions have been called before by the
 * "jit_prepare()". If there was an error, we would know by now.
 * Therefore, no extra error checking at this point, other than
 * a sanity check at the end that expects the calculated length
 * (jit.len) to be equal to the length of generated instructions
 * (jit.index).
 */
static int jit_compile(struct jit_context *ctx)
{
	emit = true;

	(void) handle_prologue(ctx);

	(void) handle_body(ctx);

	(void) handle_epilogue(ctx);

	if (ctx->jit.index != ctx->jit.len) {
		pr_err("bpf-jit: divergence between the phases; "
		       "%u vs. %u (bytes).\n",
		       ctx->jit.len, ctx->jit.index);
		return -EFAULT;
	}

	return 0;
}

/*
 * Calling this function implies a successful JIT. A successful
 * translation is signaled by setting the right parameters:
 *
 * prog->jited=1, prog->jited_len=..., prog->bpf_func=...
 */
static void jit_finalize(struct jit_context *ctx)
{
	struct bpf_prog *prog = ctx->prog;

	ctx->success    = true;
	prog->bpf_func  = (void *) ctx->jit.buf;
	prog->jited_len = ctx->jit.len;
	prog->jited     = 1;

	/* We're going to need this information for the "do_extra_pass()". */
	if (ctx->need_extra_pass) {
		ctx->jit_data->bpf_header = ctx->bpf_header;
		ctx->jit_data->bpf2insn   = ctx->bpf2insn;
		prog->aux->jit_data       = (void *) ctx->jit_data;
	} else {
		/*
		 * If things seem finalised, then mark the JITed memory
		 * as R-X and flush the memory.
		 */
		bpf_jit_binary_lock_ro(ctx->bpf_header);
		flush_icache_range((unsigned long) ctx->bpf_header,
				   (unsigned long) ctx->jit.buf + ctx->jit.len);
		prog->aux->jit_data = NULL;
	}

	jit_ctx_cleanup(ctx);

	if (bpf_jit_enable > 1)
		bpf_jit_dump(ctx->prog->len, ctx->jit.len, 2, ctx->jit.buf);

	/* TODO: Debugging */
	dump_bytes(ctx->jit.buf, ctx->jit.len, true);
}

/* Reuse the previous pass's data. */
static int jit_resume_context(struct jit_context *ctx)
{
	struct arc_jit_data *jdata =
		(struct arc_jit_data *) ctx->prog->aux->jit_data;

	if (!jdata) {
		pr_err("bpf-jit: no jit data for the extra pass.");
		return -EINVAL;
	}

	ctx->jit.buf    = (u8 *) ctx->prog->bpf_func;
	ctx->jit.len    = ctx->prog->jited_len;
	ctx->bpf_header = jdata->bpf_header;
	ctx->bpf2insn   = (u32 *) jdata->bpf2insn;
	ctx->jit_data   = jdata;

	return 0;
}

/*
 * Goes after all of the "call" instructions and gets new "address"es
 * for them. Then uses this information to re-emit them.
 */
static int jit_patch_calls(struct jit_context *ctx)
{
	const struct bpf_prog *prog = ctx->prog;
	int ret;

	emit = true;
	for (u32 i = 0; i < prog->len; i++) {
		const struct bpf_insn *insn = &prog->insnsi[i];
		/*
		 * Adjust "ctx.jit.index", so "handle_call()" can use it
		 * for its output address.
		 */
		ctx->jit.index = ctx->bpf2insn[i];

		if (insn->code == (BPF_JMP | BPF_CALL)) {
			u8 dummy;
			if ((ret = gen_call(ctx, insn, true, &dummy)) < 0)
				return ret;
		}
	}
	return 0;
}

/*
 * A normal pass that involves a "dry-run" phase, jit_prepare(),
 * to get the necessary data for the real compilation phase,
 * jit_compile().
 */
struct bpf_prog *do_normal_pass(struct bpf_prog *prog)
{
	struct jit_context ctx;

	/* Bail out if JIT is disabled. */
	if (!prog->jit_requested)
		return prog;

	if (jit_ctx_init(&ctx, prog)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	/* Get the lenghts and allocate buffer. */
	if (jit_prepare(&ctx)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	if (jit_compile(&ctx)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	jit_finalize(&ctx);

	return ctx.prog;
}

/*
 * If there are multi-function BPF programs that call each other,
 * their translated addresses are not known all at once. Therefore,
 * an extra pass is needed to consult the bpf_jit_get_func_addr()
 * again to get the newly translated addresses in order to resolve
 * the "call"s.
 */
struct bpf_prog *do_extra_pass(struct bpf_prog *prog)
{
	struct jit_context ctx;

	if (jit_ctx_init(&ctx, prog)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	if (jit_resume_context(&ctx)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	if (jit_patch_calls(&ctx)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	return ctx.prog;
}

/*
 * This function may be invoked twice for the same stream of BPF
 * instructions. The "extra pass" happens, when there are "call"s
 * involved that their addresses are not known during the first
 * invocation.
 */
struct bpf_prog *bpf_int_jit_compile(struct bpf_prog *prog)
{
	/* TODO: Debugging */
	dump_bytes((u8 *) prog->insns, 8*prog->len, false);

	/* Was this program already translated? */
	if (!prog->jited)
		return do_normal_pass(prog);
	else
		return do_extra_pass(prog);
}
