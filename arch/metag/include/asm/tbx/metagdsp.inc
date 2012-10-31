/*
 * metagdsp.inc
 *
 * Copyright (C) 2003, 2004, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * A standardized set of macros for DSP function entry, exit and extended
 * context management.
 */

#ifndef _metagdsp_inc_
#define _metagdsp_inc_ 1

INCLUDEC "metagtbi.h"

;
; _DSP_SAFE is used to enable extra code to check for common errors
;    (It should be defined for DEBUG builds; but this can be controlled
;     because it does not directly use the DEBUG define to become enabled.)
;
; _DSP_SAFETY_NET uses D1.5 and D1.6 so it can be used where these
;    registers are scratch, it checks that key CT registers are not left
;    set. It is used in the implementation of each of the standard exit
;    stubs referenced below.
;
#ifdef _DSP_SAFE
#define _DSP_SAFETY_NET                                                \
	MOV	D1.6,TXMODE                                            \
	MOV	D1.5,TXL1COUNT                                         \
	OR	D1.6,D1.6,D1.5                                         \
	MOV	D1.5,TXL2COUNT                                         \
	ORS	D1.6,D1.6,D1.5                                         \
	BNZ	\#(0)
#else
#define _DSP_SAFETY_NET
#endif

;
; DSP_ENTRY Standard DSP function entry macro
;       (i)   Declare public function name
;       (ii)  Declare function entry label
;       (iii) Set up new frame pointer
;       (iv)  Push register arguments
;       (v)   Push return address and previous frame pointer
;       (vi)  Push callee-saved registers
;
; The standard call-stack layout defines for general purpose code (see
;    machine.inc) for routines that use this entry sequence are-
;
;       #define	ARS	(3*8)     ; Register arg bytes saved on stack
;       #define FRS     (4*8)     ; Frame save area size in bytes
;
; It is optional as to whether the routine has local space (LCS), and
;   of because of this ambiguity in the general case we choose to create
;   a call frame and use this to access the incoming arguments. Hence
;   the standard defines LCS and AR0 would not be relevant in the general
;   case of code that uses these macros.
;
#ifndef _GMON_ASM
#define DSP_ENTRY(NAME)                                                \
FUNCTION PUBLIC NAME                                                   \
NAME:                                                                  \
        MOV     D0FrT,A0FrP                                            \
	ADD     A0FrP,A0StP,\#(3*8)                                    \
        MSETL   [A0StP],D0Ar6,D0Ar4,D0Ar2,D0FrT,D0.5,D0.6,D0.7
#else
#define DSP_ENTRY(NAME)                                                \
FUNCTION PUBLIC NAME                                                   \
NAME:                                                                  \
        MOV     D0FrT,A0FrP                                            \
        ADD     A0FrP,A0StP,\#(3*8)                                    \
        MSETL   [A0StP],D0Ar6,D0Ar4,D0Ar2,D0FrT,D0.5,D0.6,D0.7         \
        CALLR   D0FrT,_mcount_wrapper
#endif


;
; DSP_EXIT Standard DSP function exit macro.
;       (i)   Pop old frame pointer and return address
;       (ii)  Pop callee-saved registers
;       (iii) Restore stack pointer to value it had on entry
;       (iv)  Return to caller
;
; Reverses DSP_ENTRY
;
#define DSP_EXIT                                                       \
	_DSP_SAFETY_NET                                                \
        MGETL   D0FrT,D0.5,D0.6,D0.7,[A0FrP]                           \
        SUB     A0StP,A0FrP,\#(7*8)                                    \
        MOV     A0FrP,D0FrT                                            \
        MOV     PC,D1RtP

;
; _DSP_B helper - returns value of bit number BP in N
;
#define _DSP_B(N,BP) (((N)>>(BP)) & 1)

;
; _DSP_NB helper - returns negated value of bit number BP in N
;
#define _DSP_NB(N,BP) (_DSP_B(N,BP) ^ 1)

;
; _DSP_MB helper returns maximum non-zero bit number in N
;         (N assumed to be in range 0...0x0ffff)
;
#define _DSP_MB(N)                                                     \
(_DSP_B((N),15) * 15) + _DSP_NB((N),15) * (                            \
(_DSP_B((N),14) * 14) + _DSP_NB((N),14) * (                            \
(_DSP_B((N),13) * 13) + _DSP_NB((N),13) * (                            \
(_DSP_B((N),12) * 12) + _DSP_NB((N),12) * (                            \
(_DSP_B((N),11) * 11) + _DSP_NB((N),11) * (                            \
(_DSP_B((N),10) * 10) + _DSP_NB((N),10) * (                            \
(_DSP_B((N),9) * 9) + _DSP_NB((N),9) * (                               \
(_DSP_B((N),8) * 8) + _DSP_NB((N),8) * (                               \
(_DSP_B((N),7) * 7) + _DSP_NB((N),7) * (                               \
(_DSP_B((N),6) * 6) + _DSP_NB((N),6) * (                               \
(_DSP_B((N),5) * 5) + _DSP_NB((N),5) * (                               \
(_DSP_B((N),4) * 4) + _DSP_NB((N),4) * (                               \
(_DSP_B((N),3) * 3) + _DSP_NB((N),3) * (                               \
(_DSP_B((N),2) * 2) + _DSP_NB((N),2) * (                               \
(_DSP_B((N),1) * 1) + _DSP_NB((N),1) * (                               \
(_DSP_B((N),0) * 0)                                                    \
)))))))))))))))

;
; _DSP_MAX helper returns maximum of A,B
;
#define _DSP_MAX(A,B)                                                  \
(((A)>(B))*(A) + ((A)<=(B))*(B))

;
; DSP_RESTORE_REGARGS - restore all register arguments from stack
;                       (assuming DSP_ENTRY was used)
;
#define DSP_RESTORE_REGARGS                                            \
        SUB     A0FrP,A0FrP,\#(3*8)                                    \
        MGETL   D0Ar6,D0Ar4,D0Ar2,[A0FrP]

;
; DSP_ARG - access a stacked argument
;           (assuming DSP_ENTRY was used)
;
; typical use:
;        GETD  D0Re0,DSP_ARG(3) ; retrieve third argument
;
#define DSP_ARG(N) [A0FrP+\#-(4*(N))]

;
; _DSP_CODED_ABITS helper return <number of address bits - 1>
;                  for N locations.
;                  Enforces coding rule that N must be 0 or >= 8
;
#define _DSP_CODED_ABITS(N) ((N) > 0)*(_DSP_MB(_DSP_MAX((N),8)-1))

;
; _DSP_RAMREQ helper constructs a DSP RAM request word from
;             a single (constant) value.
;
#define _DSP_RAMREQ(DSPRAMSIZE)                                        \
_DSP_CODED_ABITS(DSPRAMSIZE) |                                         \
(_DSP_CODED_ABITS(DSPRAMSIZE) << 4) |                                  \
(_DSP_CODED_ABITS(DSPRAMSIZE) << 8) |                                  \
(_DSP_CODED_ABITS(DSPRAMSIZE) << 12)

;
; _DSP_ECTXSIZE helper calculates size of extended context save area
;
#define _DSP_ECTXSIZE(FLAGS)                                           \
(((((FLAGS) & TBICTX_XEXT_BIT) != 0) * TBICTXEXT_BYTES) +              \
 ((((FLAGS) & TBICTX_XDX8_BIT) != 0) * TBICTXEXTBB8_BYTES) +           \
 ((((FLAGS) & TBICTX_XAXX_BIT) != 0) * TBICTXEXTAXX_BYTES) +           \
 ((((FLAGS) & TBICTX_XHL2_BIT) != 0) * TBICTXEXTHL2_BYTES) +           \
 ((((FLAGS) & TBICTX_XTDP_BIT) != 0) * TBICTXEXTTDP_BYTES))

;
; DSPECTX_LCP - request extended context,with save area allocated on
;               local stack. Forces TBICTX_XEXT_BIT in case user forgot.
;
#ifndef _NOEXTERNS
EXTERN ___TBICtxAlloc
#endif

#define DSPECTX_LCP(FLAGS)                                             \
        MOV     D1Ar1,A0StP                                            \
        ADD     A0StP,A0StP,\#_DSP_ECTXSIZE((FLAGS) | TBICTX_XEXT_BIT) \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT)                    \
        CALLR   D1RtP,___TBICtxAlloc

;
; DSPECTX_NCP - request extended context but defer space allocation.
;     (typically invokes OS assistance or forces non-swapping behaviour)
;               Forces TBICTX_XEXT_BIT in case user forgot.
;
#define DSPECTX_NCP(FLAGS)                                             \
	MOV     D1Ar1,\#0                                              \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT)                    \
        CALLR   D1RtP,___TBICtxAlloc

;
; DSPECTX_CCP - request extended context with caller supplied save area.
;                  Assumes caller has provided a pointer to the save area
;                  in argument number ARGNUM (arguments numbered 1...n)
;                  Forces TBICTX_XEXT_BIT in case user forgot.
;
#define DSPECTX_CCP(FLAGS,ARGNUM)                                      \
        GETD    D1Ar1,DSP_ARG(ARGNUM)                                  \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT)                    \
        CALLR   D1RtP,___TBICtxAlloc

;
; DSPECTX_NCP_DRS -  request extended context including static
;                    DSP RAM allocation. Context save space is
;                    NULL,thus requiring OS intervention for
;                    preemptive scheduling.
;
;                    Forces TBICTX_XEXT_BIT and TBICTX_XTDP_BIT
;                    in case user forgot
;
#define DSPECTX_NCP_DRS(FLAGS,DSPRAMSIZE)                              \
        MOV     D1Ar1,\#0                                              \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT | TBICTX_XTDP_BIT)  \
        OR      D0Ar2,D0Ar2,\#_DSP_RAMREQ(DSPRAMSIZE)                  \
        CALLR   D1RtP,___TBICtxAlloc

;
; DSPECTX_CCP_DRS -  request extended context including static
;                    DSP RAM allocation. Context save space is
;                    caller provided.
;
;                       Forces TBICTX_XEXT_BIT and TBICTX_XTDP_BIT
;                    in case user forgot
;
#define DSPECTX_CCP_DRS(FLAGS,ARGNUM,DSPRAMSIZE)                       \
        GETD    D1Ar1,DSP_ARG(ARGNUM)                                  \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT | TBICTX_XTDP_BIT)  \
        OR      D0Ar2,D0Ar2,\#_DSP_RAMREQ(DSPRAMSIZE)                  \
        CALLR   D1RtP,___TBICtxAlloc

;
; DSPECTX_NCP_DRD
; DSPECTX_CCP_DRD
;                   - request extended context including
;                     runtime calculation of the DSPRAM request bits.
;                     Context save space is NULL or caller provided
;                     depending on which version you use.
;
;                        Forces TBICTX_XEXT_BIT and TBICTX_XTDP_BIT
;                     in case user forgot.
;
;
;                     On entry,D1Ar1 must contain minimum DSPRAM requirement
;
#define DSPECTX_NCP_DRD(FLAGS)                                         \
	MOV     D1Re0,\#8                                              \
        MAX     D1Ar1,D1Ar1,D1Re0                                      \
        SUB     D1Ar1,D1Ar1,\#1                                        \
        FFB     D1Ar1,D1Ar1                                            \
        LSL     D1Re0,D1Ar1,\#4                                        \
        OR      D1Ar1,D1Ar1,D1Re0                                      \
        LSL     D1Re0,D1Ar1,\#8                                        \
        OR      D1Re0,D1Ar1,D1Re0                                      \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT | TBICTX_XTDP_BIT)  \
        MOV     D1Ar1,\#0                                              \
        OR      D0Ar2,D0Ar2,D1Re0                                      \
        CALLR   D1RtP,___TBICtxAlloc

#define DSPECTX_CCP_DRD(FLAGS,CTXARGNUM)                               \
        MOV     D1Re0,\#8                                              \
        MAX     D1Ar1,D1Ar1,D1Re0                                      \
        SUB     D1Ar1,D1Ar1,\#1                                        \
        FFB     D1Ar1,D1Ar1                                            \
        LSL     D1Re0,D1Ar1,\#4                                        \
        OR      D1Ar1,D1Ar1,D1Re0                                      \
        LSL     D1Re0,D1Ar1,\#8                                        \
        OR      D1Re0,D1Ar1,D1Re0                                      \
        MOVT    D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT | TBICTX_XTDP_BIT)  \
        GETD    D1Ar1,DSP_ARG(CTXARGNUM)                               \
        OR      D0Ar2,D0Ar2,D1Re0                                      \
        CALLR   D1RtP,___TBICtxAlloc

;
; DSPECTX_FREE release extended context
;              (one macro suits all the allocation variants)
;
; WARNING: since this is only intended for use immediately prior to
;          function return,scratch registers other than DnRe0 are not
;          necessarily preserved
;
#ifndef _NOEXTERNS
EXTERN ___TBICtxFree
#endif

#define DSPECTX_FREE                                                   \
 DL      MOV     D0.5,D0Re0                                            \
 DL      XOR     D0Ar2,D0Ar2,D0Ar2                                     \
         CALLR   D1RtP,___TBICtxFree                                   \
 DL      MOV     D0Re0,D0.5

;
; DSPECTX_EXIT release extended context and return via stub
;
; This is equivalent to-
;
;               DSPECTX_FREE
;               DSP_EXIT
;
#ifndef _DSP_SAFE
#ifndef _NOEXTERNS
EXTERN ___TBICtxExitFrP
#endif

#define DSPECTX_EXIT                                                   \
         B        ___TBICtxExitFrP
#else
#ifndef _NOEXTERNS
EXTERN ___TBICtxExitFrPSafe
#endif

#define DSPECTX_EXIT                                                   \
         B        ___TBICtxExitFrPSafe
#endif

;
; DSP_QENTRY_* macros
;
; These combine a specific sub-set of the features of DSP_ENTRY and DSPECTX_
; macros to generate a minimal set of entry macro options for use when
; overheads of entry/exit are absolutely critical.
;
; The macros assume that the routines do not declare any local space (LCS),
; hence no local context storage options ala LCP above are supported.
;
; It is also assumed that they do not benifit from the creation of a frame
; pointer to access arguments which is generally true if no local space is
; allocated.
;
; DSP_QENTRY_* Quick DSP function entry macros
;       (i)   Declare public function name
;       (ii)  Declare function entry label
;       (iii) Push arguments, return address, and callee-saved registers
;       (iv)  Setup args including DSP ram size requirement and Flags
;       (v)   Call ___TBICtxAlloc function
;
; The standard call-stack layout defines for general purpose code (see
;    machine.inc) for routines that use this entry sequence are-
;
;       #define	ARS	(3*8)     ; Register arg bytes saved on stack
;       #define FRS     (4*8)     ; Frame save area size in bytes
;

#ifndef _GMON_ASM
;
; DSP_QENTRY_CCP Quick DSP function entry macro for caller context
;
;    Is equivalent to-
;
;               DSP_ENTRY(NAME)
;               DSPECTX_CCP_DRS(FLAGS,ARGNUM,DSPRAMSIZE)
;
;        ... with the exception that a frame pointer is not normally
;      created and hence the DSP_QARG macro rather than DSP_ARG _MUST_ be
;      used to access args.
;
;    DSPRAMSIZE can be specified as zero if no DSP RAM is used.
;
#define DSP_QENTRY_CCP_DRS(NAME,FLAGS,ARGNUM,DSPRAMSIZE)               \
FUNCTION PUBLIC NAME                                                   \
NAME:                                                                  \
        MSETL    [A0StP],D0Ar2,D0Ar4,D0Ar6,D0FrT,D0.5,D0.6,D0.7        \
        GETD     D1Ar1,DSP_QARG( ARGNUM )                              \
	MOVT     D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT                    \
                                   | ((DSPRAMSIZE>0)*TBICTX_XTDP_BIT)) \
        OR       D0Ar2,D0Ar2,\#_DSP_RAMREQ(DSPRAMSIZE)                 \
        CALLR    D1RtP,___TBICtxAlloc
#else
#define DSP_QENTRY_CCP_DRS(NAME,FLAGS,ARGNUM,DSPRAMSIZE)               \
FUNCTION PUBLIC NAME                                                   \
NAME:                                                                  \
        MOV      D0FrT,A0FrP                                           \
        ADD      A0FrP,A0StP,\#(3*8)                                   \
        MSETL    [A0StP],D0Ar2,D0Ar4,D0Ar6,D0FrT,D0.5,D0.6,D0.7        \
        CALLR    D0FrT,_mcount_wrapper                                 \
        GETD     D1Ar1,DSP_QARG( ARGNUM )                              \
        MOVT     D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT                    \
                                  | ((DSPRAMSIZE>0)*TBICTX_XTDP_BIT))  \
        OR       D0Ar2,D0Ar2,\#_DSP_RAMREQ(DSPRAMSIZE)                 \
        CALLR    D1RtP,___TBICtxAlloc
#endif

#ifndef _GMON_ASM
;
; DSP_QENTRY_NCP_DRS Quick DSP function entry macro for NULL context
;
;    This is equivalent to-
;
;               DSP_ENTRY(NAME)
;               DSPECTX_NCP_DRS(FLAGS,DSPRAMSIZE)
;
;        ... with the exception that a frame pointer is not normally
;      created and hence the DSP_QARG macro rather than DSP_ARG _MUST_ be
;      used to access args.
;
;    DSPRAMSIZE can be specified as zero if no DSP RAM is used.
;
#define DSP_QENTRY_NCP_DRS(NAME,FLAGS,DSPRAMSIZE)                      \
FUNCTION PUBLIC NAME                                                   \
NAME:                                                                  \
        MSETL    [A0StP],D0Ar2,D0Ar4,D0Ar6,D0FrT,D0.5,D0.6,D0.7        \
        XOR      D1Ar1,D1Ar1,D1Ar1                                     \
        MOVT     D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT                    \
                                  | ((DSPRAMSIZE>0)*TBICTX_XTDP_BIT))  \
        OR       D0Ar2,D0Ar2,\#_DSP_RAMREQ(DSPRAMSIZE)                 \
        CALLR    D1RtP,___TBICtxAlloc
#else
#define DSP_QENTRY_NCP_DRS(NAME,FLAGS,DSPRAMSIZE)                      \
FUNCTION PUBLIC NAME                                                   \
NAME:                                                                  \
        MOV      D0FrT,A0FrP                                           \
        ADD      A0FrP,A0StP,\#(3*8)                                   \
        MSETL    [A0StP],D0Ar2,D0Ar4,D0Ar6,D0FrT,D0.5,D0.6,D0.7        \
        CALLR    D0FrT,_mcount_wrapper                                 \
        XOR      D1Ar1,D1Ar1,D1Ar1                                     \
        MOVT     D0Ar2,\#((FLAGS) | TBICTX_XEXT_BIT                    \
                                  | ((DSPRAMSIZE>0)*TBICTX_XTDP_BIT))  \
        OR       D0Ar2,D0Ar2,\#_DSP_RAMREQ(DSPRAMSIZE)                 \
        CALLR    D1RtP,___TBICtxAlloc
#endif

;
; DSP_QARG - access a stacked argument using only the stack pointer
;           (assuming DSP_QENTRY_* was used)
;
; typical use:
;        GETD  D0Re0,DSP_QARG(3) ; get third argument
;
; Note that the routine must not declare local storage (LCS) if this macro
;   is to be used. If local space is required please consider _NOT_ using
;   the quick entry/exit macros at all - use the standard DSP_ENTRY based
;   ones instead (see above) or customise your own system.
;
#define DSP_QARG(N) [A0StP+\#-((4*8)+(4*(N)))]

;
; DSPECTX_QEXIT Quick DSP function exit macro.
;           (i)   Save result to be returned later
;           (ii)  Call ___TBICtxFree
;           (iii) Restore stack pointer and callee saved registers
;           (iv)  Return to caller
;
;    Reverses DSP_QENTRY_*
;
;    This is equivalent to-
;
;               DSPECTX_FREE
;               DSP_EXIT
;
;       ... with the exception that a local frame pointer is not
;       assumed and the parent frame pointer is hence implicitly preserved.
;
#ifndef _DSP_SAFE
#ifndef _GMON_ASM
#ifndef _NOEXTERNS
EXTERN ___TBICtxQExit
#endif

#define DSPECTX_QEXIT                                                  \
        B        ___TBICtxQExit
#else
#define DSPECTX_QEXIT                                                  \
        B        ___TBICtxExitFrP
#endif
#else
#ifndef _GMON_ASM
#ifndef _NOEXTERNS
EXTERN ___TBICtxQExitSafe
#endif

#define DSPECTX_QEXIT                                                  \
        B        ___TBICtxQExitSafe
#else
#define DSPECTX_QEXIT                                                  \
        B        ___TBICtxExitFrPSafe
#endif
#endif

;
; DSP_QENTRY_CALLER - old deprecated variant of DSP_QENTRY_CCP_DRS
;
#define DSP_QENTRY_CALLER(NAME, FLAGS, ARGNUM, D0MIN, D0MAX, D1MIN, D1MAX) \
	DSP_QENTRY_CCP_DRS(NAME, FLAGS, ARGNUM, D0MAX + 0*(D0MIN+D1MIN+D1MAX) )

;
; DSP_QENTRY_NULL - old deprecated variant of DSP_QENTRY_NCP_DRS
;
#define DSP_QENTRY_NULL(NAME, FLAGS, D0MIN, D0MAX, D1MIN, D1MAX)           \
	DSP_QENTRY_NCP_DRS(NAME, FLAGS, D0MAX + 0*(D0MIN+D1MIN+D1MAX) )

#endif /* ifndef _metagdsp_inc_ */

;
; End of metagdsp.inc
;
