/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * The backend-agnostic part of Just-In-Time compiler for eBPF bytecode.
 *
 * Copyright (c) 2023 Synopsys Inc.
 * Author: Shahab Vahedi <shahab@synopsys.com>
 */

#ifndef _BPF_JIT_CORE_H
#define _BPF_JIT_CORE_H

/*************** Functions that the backend must provide **************/
/* Addition */
extern u8 add_r32(u8 *buf, u8 rd, u8 rs);
extern u8 add_r32_i32(u8 *buf, u8 rd, s32 imm);
extern u8 add_r64(u8 *buf, u8 rd, u8 rs);
extern u8 add_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Subtraction */
extern u8 sub_r32(u8 *buf, u8 rd, u8 rs);
extern u8 sub_r32_i32(u8 *buf, u8 rd, s32 imm);
extern u8 sub_r64(u8 *buf, u8 rd, u8 rs);
extern u8 sub_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Multiplication */
extern u8 mul_r32(u8 *buf, u8 rd, u8 rs);
extern u8 mul_r32_i32(u8 *buf, u8 rd, s32 imm);
extern u8 mul_r64(u8 *buf, u8 rd, u8 rs);
extern u8 mul_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Division */
extern u8 div_r32(u8 *buf, u8 rd, u8 rs);
extern u8 div_r32_i32(u8 *buf, u8 rd, s32 imm);
/* Remainder */
extern u8 mod_r32(u8 *buf, u8 rd, u8 rs);
extern u8 mod_r32_i32(u8 *buf, u8 rd, s32 imm);
/* Bitwise AND */
extern u8 and_r32(u8 *buf, u8 rd, u8 rs);
extern u8 and_r32_i32(u8 *buf, u8 rd, s32 imm);
extern u8 and_r64(u8 *buf, u8 rd, u8 rs);
extern u8 and_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Bitwise OR */
extern u8 or_r32(u8 *buf, u8 rd, u8 rs);
extern u8 or_r32_i32(u8 *buf, u8 rd, s32 imm);
extern u8 or_r64(u8 *buf, u8 rd, u8 rs);
extern u8 or_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Bitwise XOR */
extern u8 xor_r32(u8 *buf, u8 rd, u8 rs);
extern u8 xor_r32_i32(u8 *buf, u8 rd, s32 imm);
extern u8 xor_r64(u8 *buf, u8 rd, u8 rs);
extern u8 xor_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Bitwise Negate */
extern u8 neg_r32(u8 *buf, u8 r);
extern u8 neg_r64(u8 *buf, u8 r);
/* Bitwise left shift */
extern u8 lsh_r32(u8 *buf, u8 rd, u8 rs);
extern u8 lsh_r32_i32(u8 *buf, u8 rd, u8 imm);
extern u8 lsh_r64(u8 *buf, u8 rd, u8 rs);
extern u8 lsh_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Bitwise right shift (logical) */
extern u8 rsh_r32(u8 *buf, u8 rd, u8 rs);
extern u8 rsh_r32_i32(u8 *buf, u8 rd, u8 imm);
extern u8 rsh_r64(u8 *buf, u8 rd, u8 rs);
extern u8 rsh_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Bitwise right shift (arithmetic) */
extern u8 arsh_r32(u8 *buf, u8 rd, u8 rs);
extern u8 arsh_r32_i32(u8 *buf, u8 rd, u8 imm);
extern u8 arsh_r64(u8 *buf, u8 rd, u8 rs);
extern u8 arsh_r64_i32(u8 *buf, u8 rd, s32 imm);
/* Moves */
extern u8 mov_r32(u8 *buf, u8 rd, u8 rs);
extern u8 mov_r32_i32(u8 *buf, u8 reg, s32 imm);
extern u8 mov_r64(u8 *buf, u8 rd, u8 rs);
extern u8 mov_r64_i32(u8 *buf, u8 reg, s32 imm);

#endif /* _BPF_JIT_CORE_H */
