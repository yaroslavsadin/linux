#include <linux/filter.h>

/* ARC core registers. */
enum {
	ARC_R_0 , ARC_R_1 , ARC_R_2 , ARC_R_3 , ARC_R_4 , ARC_R_5,
	ARC_R_6 , ARC_R_7 , ARC_R_8 , ARC_R_9 , ARC_R_10, ARC_R_11,
	ARC_R_12, ARC_R_13, ARC_R_14, ARC_R_15, ARC_R_16, ARC_R_17,
	ARC_R_18, ARC_R_19, ARC_R_20, ARC_R_21, ARC_R_22, ARC_R_23,
	ARC_R_24, ARC_R_25, ARC_R_GP, ARC_R_FP, ARC_R_SP, ARC_R_ILINK,
	ARC_R_30, ARC_R_BLINK,
	ARC_R_IMM = 62
};

/*
 * Intermediary register for some operations. Its life span is only during that
 * specific operation, and no more.
 */
#define REG_TEMP ARC_R_20

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
	/* Temporary register for blinding constants */
	[BPF_REG_AX] = {ARC_R_22, ARC_R_23},
};

#define REG_LO(r) (bpf2arc[(r)][0])
#define REG_HI(r) (bpf2arc[(r)][1])

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

/* Bytes. */
enum {
	INSN_len_short = 2,	/* Short instructions length. */
	INSN_len_normal = 4,	/* Normal instructions length. */
	INSN_len_imm = 4	/* Length of an extra 32-bit immediate. */
};

#define IN_S9_RANGE(x)	((x) <= 255 && (x) >= -256)

/*
 * The 4-byte encoding of "add a, b, c":
 *
 * 0010_0bbb 0000_0000 0BBB_cccc ccaa_aaaa
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define ADD_OPCODE	0x20000000
#define ADD_A(x)	((x) & 0x03f)
#define ADD_B(x)	((((x) & 0x07) << 24) | (((x) & 0x38) <<  9))
#define ADD_C(x)	(((x) & 0x03f) << 6)

/*
 * The 4-byte encoding of "xor a, b, c":
 *
 * 0010_0bbb 0000_0111 0BBB_cccc ccaa_aaaa
 *
 * a:  aaaaaa		result
 * b:  BBBbbb		the 1st input operand
 * c:  cccccc		the 2nd input operand
 */
#define XOR_OPCODE	0x20070000
#define XOR_A(x)	((x) & 0x03f)
#define XOR_B(x)	((((x) & 0x07) << 24) | (((x) & 0x38) <<  9))
#define XOR_C(x)	(((x) & 0x03f) << 6)

/*
 * The 4-byte encoding of "mov b, c":
 *
 * 0010_0bbb 0000_1010 0BBB_cccc cc00_0000
 *
 * b:  BBBbbb		destination register
 * c:  cccccc		source register
 */
#define MOVE_OPCODE	0x200a0000
#define MOVE_B(x)	((((x) & 0x07) << 24) | (((x) & 0x38) <<  9))
#define MOVE_C(x)	(((x) & 0x03f) << 6)

/*
 * The 4-byte encoding of "ld[zz][.x][.aa][.di] c, [b,s9]":
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
#define LOAD_B(x)	((((x) &  0x07) << 24) | (((x) &  0x38) <<  9))
#define LOAD_C(x)	((x) & 0x03f)
/* Generic load. */
#define OP_LD \
	LOAD_OPCODE | LOAD_D(D_cached) | LOAD_X(X_zero)
/* 32-bit load. */
#define OP_LD32 \
	OP_LD | LOAD_ZZ(ZZ_4_byte)
/* "pop reg" is merely a "ld.ab reg, [sp, 4]". */
#define OP_POP \
	OP_LD32 | LOAD_AA(AA_post) | LOAD_S9(4) | LOAD_B(ARC_R_SP)

/*
 * The 4-byte encoding of "st[zz][.aa][.di] c, [b,s9]":
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
#define STORE_B(x)	((((x) &  0x07) << 24) | (((x) &  0x38) <<  9))
#define STORE_C(x)	(((x) & 0x03f) << 6)
/* Generic store. */
#define OP_ST \
	STORE_OPCODE | STORE_D(D_cached)
/* 32-bit store. */
#define OP_ST32 \
	OP_ST | STORE_ZZ(ZZ_4_byte)
/* "push reg" is merely a "st.aw reg, [sp, -4]". */
#define OP_PUSH \
	OP_ST32 | STORE_AA(AA_pre) | STORE_S9(-4) | STORE_B(ARC_R_SP)

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
		u32 insn = ADD_OPCODE | ADD_A(reg_dst) | ADD_B(reg_dst) |
			   ADD_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_add_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (emit) {
		u32 insn = ADD_OPCODE | ADD_A(reg_dst) | ADD_B(reg_dst) |
			   ADD_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

static u8 arc_xor_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = XOR_OPCODE | XOR_A(reg_dst) | XOR_B(reg_dst) |
			   XOR_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_xor_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (emit) {
		u32 insn = XOR_OPCODE | XOR_A(reg_dst) | XOR_B(reg_dst) |
			   XOR_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

/* "mov r, 0" in fact is a "xor r, r, r". */
static u8 arc_mov_0(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = XOR_OPCODE | XOR_A(reg) | XOR_B(reg) | XOR_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_mov_r(u8 *buf, u8 reg_dst, u8 reg_src)
{
	if (emit) {
		u32 insn = MOVE_OPCODE | MOVE_B(reg_dst) | MOVE_C(reg_src);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

static u8 arc_mov_i(u8 *buf, u8 reg_dst, s32 imm)
{
	if (emit) {
		u32 insn = MOVE_OPCODE | MOVE_B(reg_dst) | MOVE_C(ARC_R_IMM);
		emit_4_bytes(buf                , insn);
		emit_4_bytes(buf+INSN_len_normal, imm);
	}
	return INSN_len_normal + INSN_len_imm;
}

/* st.as reg_c, [reg_b, off] */
static u8 arc_st_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 zz)
{
	if (emit) {
		u32 insn = OP_ST | STORE_AA(AA_none) | STORE_ZZ(zz) |
			   STORE_C(reg) | STORE_B(reg_mem) | STORE_S9(off);
		emit_4_bytes(buf, insn);
	}

	return INSN_len_normal;
}

/* "push reg" is merely a "st.aw reg_c, [sp, -4]". */
static u8 arc_push_r(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = OP_PUSH | STORE_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

/* ld.aw reg_c, [reg_b, off] */
static u8 arc_ld_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 zz)
{
	if (emit) {
		u32 insn = OP_LD | LOAD_AA(AA_none) | LOAD_ZZ(zz) |
			   LOAD_C(reg) | LOAD_B(reg_mem) | LOAD_S9(off);
		emit_4_bytes(buf, insn);
	}

	return INSN_len_normal;
}

static u8 arc_pop_r(u8 *buf, u8 reg)
{
	if (emit) {
		u32 insn = OP_POP | LOAD_C(reg);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

/*********************** Packers *************************/

static u8 add_r32(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len = 0;
	len = arc_add_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
	return len;
}

static u8 add_r32_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	u8 len = 0;
	len = arc_add_i(buf, REG_LO(reg_dst), imm);
	return len;
}

static u8 xor_r32(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len = 0;
	len = arc_xor_r(buf, REG_LO(reg_dst), REG_LO(reg_src));
	return len;
}

static u8 xor_r32_i32(u8 *buf, u8 reg_dst, s32 imm)
{
	u8 len = 0;
	len = arc_xor_i(buf, REG_LO(reg_dst), imm);
	return len;
}

static u8 mov_r64(u8 *buf, u8 reg_dst, u8 reg_src)
{
	u8 len = 0;

	if (reg_dst == reg_src)
		return len;

	len  = arc_mov_r(buf    , REG_LO(reg_dst), REG_LO(reg_src));
	len += arc_mov_r(buf+len, REG_HI(reg_dst), REG_HI(reg_src));

	return len;
}

static u8 mov_r64_i32(u8 *buf, u8 reg, s32 imm)
{
	u8 len = 0;

	if (imm == 0) {
		len  = arc_mov_0(buf    , REG_LO(reg));
		len += arc_mov_0(buf+len, REG_HI(reg));
		return len;
	}

	len = arc_mov_i(buf, REG_LO(reg), imm);
	if (imm >= 0)
		len += arc_mov_0(buf+len, REG_HI(reg));
	else
		len += arc_mov_i(buf+len, REG_HI(reg), -1);

	return len;
}

static u8 mov_r64_i64(u8 *buf, u8 reg, u32 hi, u32 lo)
{
	u8 len = 0;

	len  = arc_mov_i(buf    , REG_LO(reg), lo);
	len += arc_mov_i(buf+len, REG_HI(reg), hi);

	return len;
}

static u8 store_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 size)
{
	u8 len = 0;
	u8 arc_reg_mem = REG_LO(reg_mem);

	/*
	 * If the offset is too big to fit in s9, emit:
	 *   mov r20, off
	 *   add r20, r20, reg
	 * and make sure that r20 will be the effective address for store.
	 *   st  r, [r20, 0]
	 */
	if (!IN_S9_RANGE(off) ||
	    (size == BPF_DW && !IN_S9_RANGE(off + 4))) {
		len  = arc_mov_i(buf    , REG_TEMP, (u32) off);
		len += arc_add_r(buf+len, REG_TEMP, reg_mem);
		arc_reg_mem = REG_TEMP;
		off = 0;
	}

	if (size == BPF_B || size == BPF_H || size == BPF_W) {
		u8 zz = bpf_to_arc_size(size);
		len += arc_st_r(buf+len, REG_LO(reg), arc_reg_mem, off, zz);
	} else if (size == BPF_DW) {
		len += arc_st_r(buf+len, REG_LO(reg), arc_reg_mem, off+0,
				ZZ_4_byte);
		len += arc_st_r(buf+len, REG_HI(reg), arc_reg_mem, off+4,
				ZZ_4_byte);
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

static u8 load_r(u8 *buf, u8 reg, u8 reg_mem, s16 off, u8 size)
{
	u8 len = 0;
	u8 arc_reg_mem = REG_LO(reg_mem);

	/*
	 * If the offset is too big to fit in s9, emit:
	 *   mov r20, off
	 *   add r20, r20, reg
	 * and make sure that r20 will be the effective address for load.
	 *   ld  r, [r20, 0]
	 */
	if (!IN_S9_RANGE(off) ||
	    (size == BPF_DW && !IN_S9_RANGE(off + 4))) {
		len  = arc_mov_i(buf    , REG_TEMP, (u32) off);
		len += arc_add_r(buf+len, REG_TEMP, reg_mem);
		arc_reg_mem = REG_TEMP;
		off = 0;
	}

	if (size == BPF_B || size == BPF_H || size == BPF_W) {
		u8 zz = bpf_to_arc_size(size);
		len += arc_ld_r(buf+len, REG_LO(reg), arc_reg_mem, off, zz);
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
 * The JIT pertinent context that is used by different functions.
 *
 * prog:		The current eBPF program being handled.
 * orig_prog:		The original eBPF program before any possible change.
 * jit:			The jit buffer and its length.
 * blinded:		True if "constant blinding" step returned a new "prog".
 * success:		Indicates if the whole JIT went OK.
 * regs_clobbered:	Each bit status determines if that BPF reg is clobbered.
 * epilogue_offset:	Used by early "return"s in the code to jump here.
 */
struct jit_context
{
	struct bpf_prog		*prog;
	struct bpf_prog		*orig_prog;
	struct jit_buffer	jit;
	bool			blinded;
	bool			success;
	u32			regs_clobbered;
	u32			epilogue_offset;
};

static int jit_ctx_init(struct jit_context *ctx, struct bpf_prog *prog)
{
	ctx->orig_prog = prog;

	/* If constant blinding was requested but failed, scram. */
	ctx->prog = bpf_jit_blind_constants(prog);
	if (IS_ERR(ctx->prog))
		return PTR_ERR(ctx->prog);
	ctx->blinded = (ctx->prog == ctx->orig_prog ? false : true);

	ctx->jit.buf   = NULL;
	ctx->jit.len   = 0;
	ctx->jit.index = 0;
	ctx->success   = false;

	return 0;
}

static void jit_ctx_cleanup(struct jit_context *ctx)
{
	if (ctx->blinded) {
		/* if all went well, release the orig_prog. */
		if (ctx->success)
			bpf_jit_prog_release_other(ctx->prog, ctx->orig_prog);
		else
			bpf_jit_prog_release_other(ctx->orig_prog, ctx->prog);
	}

	if (ctx->jit.buf && !ctx->success)
		kfree(ctx->jit.buf);
}

/*
 * Goes through all the instructions and checks if any of the callee-saved
 * registers are clobbered. If yes, the corresponding bit position of that
 * register is set to true.
 */
static void detect_regs_clobbered(struct jit_context *ctx)
{
	u32 usage = 0;
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
		if (insn[i].src_reg == BPF_REG_FP)
			usage |= BIT(BPF_REG_FP);
	}

	ctx->regs_clobbered = usage;
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

/*
 * If "emit" is true, all the necessary "push"s are generated. Else, it acts
 * as a dry run and only updates the length of would-have-been instructions.
 */
static int handle_prologue(struct jit_context *ctx)
{
	int ret;
	u32 push_mask = ctx->regs_clobbered;
	u8 *buf = (emit ? ctx->jit.buf : NULL);	/* Prologue starts at buf[0]. */
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
	u8 *buf = (emit ? ctx->jit.buf + ctx->jit.index : NULL);
	u32 len = 0;

	if ((ret = jit_buffer_check(&ctx->jit)))
	    return ret;

	/* TODO: Frame pointer analysis (in a separate func() and put in ctx)
	 * and allocate frame based on that. */

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

	jit_buffer_update(&ctx->jit, len);

	return 0;
}

/* TODO: fill me in. */
/*
 * Handles one eBPF instruction at a time. To make this function faster,
 * it does not call "jit_buffer_check()". Else, it would call it for every
 * instruction. As a result, it should not be invoked directly. Only
 * "handle_body()", that has already executed the verification, may call
 * this function.
 */
static int handle_insn(const struct bpf_insn *insn, bool last_insn,
		       struct jit_buffer *jbuf)
{
	u8  code = insn->code;
	u8  dst  = insn->dst_reg;
	u8  src  = insn->src_reg;
	s16 off  = insn->off;
	s32 imm  = insn->imm;
	u8 *buf  = (emit ? jbuf->buf + jbuf->index : NULL);
	u8  len  = 0;

	switch (code) {
	/* dst += src (32-bit) */
	case BPF_ALU | BPF_ADD | BPF_X:
		len = add_r32(buf, dst, src);
		break;
	/* dst += imm (32-bit) */
	case BPF_ALU | BPF_ADD | BPF_K:
		len = add_r32_i32(buf, dst, imm);
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
		/* TODO
		len = add_r64(buf, dst, src); */
		break;
	/* dst += imm (64-bit) */
	case BPF_ALU64 | BPF_ADD | BPF_K:
		/* TODO
		len = add_r64_i32(buf, dst, imm); */
		break;
	/* dst = src (64-bit) */
	case BPF_ALU64 | BPF_MOV | BPF_X:
		len = mov_r64(buf, dst, src);
		break;
	/* dst = imm32 (sign extend to 64-bit) */
	case BPF_ALU64 | BPF_MOV | BPF_K:
		len = mov_r64_i32(buf, dst, imm);
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
	/* TODO: add store_i().
	case BPF_ST | BPF_MEM | BPF_W:
	case BPF_ST | BPF_MEM | BPF_H:
	case BPF_ST | BPF_MEM | BPF_B:
	case BPF_ST | BPF_MEM | BPF_DW:
	*/
	case BPF_JMP | BPF_EXIT:
		/* If the last instruction, epilogue will follow. */
		if (last_insn)
			break;
		/* TODO: jump to epilogue AND add "break". */
		fallthrough;
	default:
		pr_err("bpf-jit: can't handle instruction code 0x%02X\n", code);
		return -ENOTSUPP;
	}

	jit_buffer_update(jbuf, len);

	return 0;
}

static int handle_body(struct jit_context *ctx)
{
	int ret;
	const struct bpf_prog *prog = ctx->prog;

	if ((ret = jit_buffer_check(&ctx->jit)))
	    return ret;

	for (u32 i = 0; i < prog->len; i++) {
		bool last = (i == (prog->len - 1) ? true : false);

		if ((ret = handle_insn(&prog->insnsi[i], last, &ctx->jit)))
			return ret;
	}

	return 0;
}

/*
 * The first pass of the translation without actually emitting any
 * instruction. It helps in getting a forecast on some aspects, such
 * as the length of the whole program or where the epilogue starts.
 *
 * In the end, when the whole length is known, a piece of buffer
 * is allocated.
 */
static int jit_prepare(struct jit_context *ctx)
{
	int ret;

	/* Dry run. */
	emit = false;

	/* Get the length of prologue section. */
	detect_regs_clobbered(ctx);
	if ((ret = handle_prologue(ctx)))
		return ret;

	if ((ret = handle_body(ctx)))
		return ret;

	/* Record at which offset prologue may begin. */
	ctx->epilogue_offset = ctx->jit.len;

	/* Add the epilogue's length as well. */
	if ((ret = handle_epilogue(ctx)))
		return ret;

	ctx->jit.buf = kcalloc(ctx->jit.len, sizeof(*ctx->jit.buf),
			       GFP_KERNEL);

	if (!ctx->jit.buf) {
		pr_err("bpf-jit: could not allocate memory for translation.\n");
		return -ENOMEM;
	}

	/* TODO: j blink */

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
		pr_err("bpf-jit: divergence between the passes; "
		       "%u vs. %u (bytes).\n",
		       ctx->jit.len, ctx->jit.index);
		return -EFAULT;
	}

	/* Debugging */
	dump_bytes((u8 *) ctx->prog->insns, 8*ctx->prog->len, false);
	dump_bytes(ctx->jit.buf, ctx->jit.len, true);

	return 0;
}

/* TODO: fill me in. */
static int jit_finalize(struct jit_context *ctx)
{
	/* set the right permissions for jit.buf */
	/* if ctx->success ... */
	/*   prog->jited = 1 */
	/*   prog->jited_len = jit.len */
	/*   prog->bpf_func = jit.buf */
	return 0;
}

/*
 * A successful translation is signaled by setting the right parameters:
 *
 * prog->jited=1, prog->jited_len=..., prog->bpf_func=...
 *
 * And then returning. Any other sort of return is an act of falling back
 * to the interpreter.
 */
struct bpf_prog *bpf_int_jit_compile(struct bpf_prog *prog)
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

	if (jit_finalize(&ctx)) {
		jit_ctx_cleanup(&ctx);
		return prog;
	}

	jit_ctx_cleanup(&ctx);

	return ctx.prog;
}
