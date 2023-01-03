#include <linux/filter.h>

/* ARC core registers. */
enum {
	ARC_R_0 , ARC_R_1 , ARC_R_2 , ARC_R_3 , ARC_R_4 , ARC_R_5,
	ARC_R_6 , ARC_R_7 , ARC_R_8 , ARC_R_9 , ARC_R_10, ARC_R_11,
	ARC_R_12, ARC_R_13, ARC_R_14, ARC_R_15, ARC_R_16, ARC_R_17,
	ARC_R_18, ARC_R_19, ARC_R_20, ARC_R_21, ARC_R_22, ARC_R_23,
	ARC_R_24, ARC_R_25, ARC_R_GP, ARC_R_FP, ARC_R_SP, ARC_R_ILINK,
	ARC_R_30, ARC_R_BLINK
};

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
	/* Read-only frame pointer to access the eBPF stack */
	[BPF_REG_FP] = {ARC_R_FP, ARC_R_GP},
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
#define STORE_S9(x)	((((x) & 0x0ff) << 16) | \
			 (((x) & 0x100) <<  7))
#define STORE_B(x)	((((x) & 0x07) << 24) | \
			 (((x) & 0x38) <<  9))
#define STORE_C(x)	(((x) & 0x03f) <<  6)

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
 * c:  cccccc		source reg to be stored
 */
#define LOAD_OPCODE	0x10000000
#define LOAD_X(x)	((x) << 6)
#define LOAD_ZZ(x)	((x) << 7)
#define LOAD_AA(x)	((x) << 9)
#define LOAD_D(x)	((x) << 11)
#define LOAD_S9(x)	((((x) & 0x0ff) << 16) | \
			 (((x) & 0x100) <<  7))
#define LOAD_B(x)	((((x) & 0x07) << 24) | \
			 (((x) & 0x38) <<  9))
#define LOAD_C(x)	((x) & 0x03f)

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
		if (i != len-1) {
			j += sprintf(line+j, "0x%02x, ", buf[i]);
		}
		else {
			sprintf(line+j, "0x%02x" , buf[i]);
			pr_info("%s\n", line);
			break;
		}
		if (i % 8 == 7) {
			pr_info("%s\n", line);
			j = 0;
		}
	}

	if (jit)
		pr_info("\n");
}

static inline void emit_2_bytes(u8 *buf, u16 bytes)
{
	*((u16 *) buf) = bytes;
}

static inline void emit_4_bytes(u8 *buf, u32 bytes)
{
	emit_2_bytes(buf+0, bytes >>     16);
	emit_2_bytes(buf+2, bytes  & 0xffff);
}

/* "push reg" is merely a "st.aw reg, [sp, -4]". */
static u8 emit_push_reg(u8 *buf, u8 reg_num, bool emit)
{
	/* The hardcoded bits of a push instruction. */
	u32 insn = STORE_OPCODE | STORE_ZZ(ZZ_4_byte) | STORE_AA(AA_pre) | \
		   STORE_D(D_cached) | STORE_S9(-4) | STORE_B(ARC_R_SP);

	if (emit) {
		insn |= STORE_C(reg_num);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
}

/* "pop reg" is merely a "ld.ab reg, [sp, 4]". */
static u8 emit_pop_reg(u8 *buf, u8 reg_num, bool emit)
{
	/* The hardcoded bits of a pop instruction. */
	u32 insn = LOAD_OPCODE | LOAD_ZZ(ZZ_4_byte) | LOAD_AA(AA_post) | \
		   LOAD_D(D_cached) | LOAD_X(X_zero) | LOAD_S9(4) | \
		   LOAD_B(ARC_R_SP);

	if (emit) {
		insn |= LOAD_C(reg_num);
		emit_4_bytes(buf, insn);
	}
	return INSN_len_normal;
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

/*
 * If "emit" is true, all the necessary "push"s are generated. Else, it acts
 * as a dry run and only updates the length of would-have-been instructions.
 */
static int handle_prologue(struct jit_context *ctx, bool emit)
{
	u32 push_mask = ctx->regs_clobbered;
	u8 *buf = ctx->jit.buf;
	u32 idx = 0;			/* A prologue always starts at 0. */

	while (push_mask) {
		u8 reg = __builtin_ffs(push_mask) - 1;

		if (reg < BPF_REG_6 || reg > BPF_REG_FP) {
			pr_err("bpf-jit: invalid register for prologue %u\n",
			       reg);
			return -EINVAL;
		}

		idx += emit_push_reg(buf+idx, REG_LO(reg), emit);
		idx += emit_push_reg(buf+idx, REG_HI(reg), emit);
		push_mask &= ~BIT(reg);
	}

	/* If no instruction is emitted, only update the total length. */
	if (!emit)
		ctx->jit.len = idx;
	else
		ctx->jit.index = idx;

	return 0;
}

/*
 * The counter part for "handle_prologue()". If this function is asked to emit
 * instructions then it continues with "jit.index". If no instruction is
 * supposed to be emitted, it means it should contribute to the calculation of
 * "jit.len", and therefore it begins with that.
 */
static int handle_epilogue(struct jit_context *ctx, bool emit)
{
	u32 pop_mask = ctx->regs_clobbered;
	u8 *buf = ctx->jit.buf;
	u32 idx = (emit ? ctx->jit.index : ctx->jit.len);

	while (pop_mask) {
		u8 reg = 31 - __builtin_clz(pop_mask);

		if (reg < BPF_REG_6 || reg > BPF_REG_FP) {
			pr_err("bpf-jit: invalid register for epilogue %u\n",
			       reg);
			return -EINVAL;
		}

		idx += emit_pop_reg(buf+idx, REG_HI(reg), emit);
		idx += emit_pop_reg(buf+idx, REG_LO(reg), emit);
		pop_mask &= ~BIT(reg);
	}

	/* If no instruction is emitted, only update the total length. */
	if (!emit)
		ctx->jit.len = idx;
	else
		ctx->jit.index = idx;

	return 0;
}

/* TODO: fill me in. */
static int handle_body(struct jit_context *ctx, bool emit)
{
	const struct bpf_prog *prog = ctx->prog;

	for (u32 i = 0; i < prog->len; i++) {
		const struct bpf_insn *insn = &prog->insnsi[i];
		switch (insn->code) {
		case BPF_ALU   | BPF_ADD:
		case BPF_ALU64 | BPF_ADD:
			//if (BPF_K | BPF_X)
			//gen_addition();
			break;
		case BPF_JMP | BPF_EXIT:
			/* If the last instruction, epilogue will follow. */
			if (i == prog->len - 1)
				break;
			/* TODO: jump to epilogue AND add "break". */
			fallthrough;
		default:
			pr_err("bpf-jit: Can't handle instruction code %u\n",
			       insn->code);
			return -EFAULT;
		}
	}

	/* Debugging */
	dump_bytes((u8 *) prog->insns, 4*prog->len, false);
	dump_bytes(ctx->jit.buf, ctx->jit.len, true);

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

	/* Get the length of prologue section. */
	detect_regs_clobbered(ctx);
	if ((ret = handle_prologue(ctx, false)))
		return ret;

	if ((ret = handle_body(ctx, false)))
		return ret;

	/* Record at which offset prologue may begin. */
	ctx->epilogue_offset = ctx->jit.len;

	/* Add the epilogue's length as well. */
	if ((ret = handle_epilogue(ctx, false)))
		return ret;

	ctx->jit.buf = kcalloc(ctx->jit.len, sizeof(*ctx->jit.buf),
			       GFP_KERNEL);

	if (!ctx->jit.buf) {
		pr_err("bpf-jit: could not allocate memory for translation.\n");
		return -ENOMEM;
	}

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
	(void) handle_prologue(ctx, true);

	(void) handle_body(ctx, true);

	(void) handle_epilogue(ctx, true);

	if (ctx->jit.index != ctx->jit.len) {
		pr_err("bpf-jit: divergence between the passes; "
		       "%u vs. %u (bytes).\n",
		       ctx->jit.len, ctx->jit.index);
		return -EFAULT;
	}

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
