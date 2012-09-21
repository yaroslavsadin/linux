/*
 * metag.inc
 *
 * Copyright (C) 2000-2007, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * Global header for defining META Core specifics
 *
 *            defines :
 *
 *              __ANYC__ - implies inclusion in any C compile (i.e. not assembly)
 *              __STDC__ - implies inclusion in any ANSI C compile
 *              __GNUC__ - implies inclusion in GNU C compile
 *         __ASSEMBLER__ - implies inclusion in assembly not C compile
 *                 METAG - set to disable automatic assembler setup
 *      METAG_..._VALUES - set if various common META Core defines needed
 *      METAG_ALL_VALUES - set if all data available is to be defined
 *             METAC_X_Y - set to identify the specific core being targeted
 */

#ifndef _METAG_INC_
#define _METAG_INC_

/* Setup defines to indicate which compiler/assembler is being used. */
#include "compasm.inc"

#ifndef __ANYC__
/*
 * Only if assembling
 */
#ifndef METAG
#define METAG 1

ENABLE_META    /* Fall-back has no variant restrictions */
#endif /* ifndef METAG */
#endif /* ifndef __ANYC__ */

/* -------------------------------------------------------------------------
;                          DATA AND ADDRESS UNIT REGISTERS
;  -----------------------------------------------------------------------*/
/* 
  Thread local D0 registers
 */ 
/*   D0.0    ; Holds 32-bit result, can be used as scratch */
#define D0Re0 D0.0
/*   D0.1    ; Used to pass Arg6_32 */
#define D0Ar6 D0.1
/*   D0.2    ; Used to pass Arg4_32 */
#define D0Ar4 D0.2
/*   D0.3    ; Used to pass Arg2_32 to a called routine (see D1.3 below) */
#define D0Ar2 D0.3
/*   D0.4    ; Can be used as scratch; used to save A0FrP in entry sequences */
#define D0FrT D0.4
/*   D0.5    ; C compiler assumes preservation, save with D1.5 if used */
/*   D0.6    ; C compiler assumes preservation, save with D1.6 if used */
/*   D0.7    ; C compiler assumes preservation, save with D1.7 if used */
/*   D0.8    ; Use of D0.8 and above is not encouraged */
/*   D0.9  */
/*   D0.10 */
/*   D0.11 */
/*   D0.12 */
/*   D0.13 */
/*   D0.14 */
/*   D0.15 */
/*
   Thread local D1 registers
 */
/*   D1.0    ; Holds top 32-bits of 64-bit result, can be used as scratch */
#define D1Re0 D1.0
/*   D1.1    ; Used to pass Arg5_32 */
#define D1Ar5 D1.1
/*   D1.2    ; Used to pass Arg3_32 */
#define D1Ar3 D1.2
/*   D1.3    ; Used to pass Arg1_32 (first 32-bit argument) to a called routine */
#define D1Ar1 D1.3
/*   D1.4    ; Used for Return Pointer, save during entry with A0FrP (via D0.4) */
#define D1RtP D1.4
/*   D1.5    ; C compiler assumes preservation, save if used */
/*   D1.6    ; C compiler assumes preservation, save if used */
/*   D1.7    ; C compiler assumes preservation, save if used */
/*   D1.8    ; Use of D1.8 and above is not encouraged */
/*   D1.9  */
/*   D1.10 */
/*   D1.11 */
/*   D1.12 */
/*   D1.13 */
/*   D1.14 */
/*   D1.15 */
/*
   Thread local A0 registers
 */
/*   A0.0    ; Primary stack pointer */
#define A0StP A0.0
/*   A0.1    ; Used as local frame pointer in C, save if used (via D0.4) */
#define A0FrP A0.1
/*   A0.2  */
/*   A0.3  */
/*   A0.4    ; Use of A0.4 and above is not encouraged */
/*   A0.5  */
/*   A0.6  */
/*   A0.7  */
/*
   Thread local A1 registers
 */
/*   A1.0    ; Global static chain pointer - do not modify */
#define A1GbP A1.0
/*   A1.1    ; Local static chain pointer in C, can be used as scratch */
#define A1LbP A1.1
/*   A1.2  */
/*   A1.3  */
/*   A1.4    ; Use of A1.4 and above is not encouraged */
/*   A1.5  */
/*   A1.6  */
/*   A1.7  */
#ifdef METAC_2_1
/* Renameable registers for use with Fast Interrupts */
/* The interrupt stack pointer (usually a global register) */
#define A0IStP A0IReg
/* The interrupt global pointer (usually a global register) */
#define A1IGbP A1IReg
#endif
/*
   Further registers may be globally allocated via linkage/loading tools,
   normally they are not used.
 */
/*-------------------------------------------------------------------------
;                    STACK STRUCTURE and CALLING CONVENTION
; -----------------------------------------------------------------------*/
/*
; Calling convention indicates that the following is the state of the
; stack frame at the start of a routine-
;
;       Arg9_32 [A0StP+#-12]
;       Arg8_32 [A0StP+#- 8]
;       Arg7_32 [A0StP+#- 4]
;   A0StP-> 
;
; Registers D1.3, D0.3, ..., to D0.1 are used to pass Arg1_32 to Arg6_32
;   respectively. If a routine needs to store them on the stack in order
;   to make sub-calls or because of the general complexity of the routine it
;   is best to dump these registers immediately at the start of a routine
;   using a MSETL or SETL instruction-
;
;   MSETL   [A0StP],D0Ar6,D0Ar4,D0Ar2; Only dump argments expected
;or SETL    [A0StP+#8++],D0Ar2       ; Up to two 32-bit args expected
;
; For non-leaf routines it is always necessary to save and restore at least
; the return address value D1RtP on the stack. Also by convention if the
; frame is saved then a new A0FrP value must be set-up. So for non-leaf
; routines at this point both these registers must be saved onto the stack
; using a SETL instruction and the new A0FrP value is then set-up-
;
;   MOV     D0FrT,A0FrP
;   ADD     A0FrP,A0StP,#0
;   SETL    [A0StP+#8++],D0FrT,D1RtP
;
; Registers D0.5, D1.5, to D1.7 are assumed to be preserved across calls so
;   a SETL or MSETL instruction can be used to save the current state
;   of these registers if they are modified by the current routine-
;
;   MSETL   [A0StP],D0.5,D0.6,D0.7   ; Only save registers modified
;or SETL    [A0StP+#8++],D0.5        ; Only D0.5 and/or D1.5 modified
;
; All of the above sequences can be combined into one maximal case-
;
;   MOV     D0FrT,A0FrP              ; Save and calculate new frame pointer
;   ADD     A0FrP,A0StP,#(ARS)
;   MSETL   [A0StP],D0Ar6,D0Ar4,D0Ar2,D0FrT,D0.5,D0.6,D0.7
;
; Having completed the above sequence the only remaining task on routine
; entry is to reserve any local and outgoing argment storage space on the
; stack. This instruction may be omitted if the size of this region is zero-
;
;   ADD     A0StP,A0StP,#(LCS)
;
; LCS is the first example use of one of a number of standard local defined
; values that can be created to make assembler code more readable and
; potentially more robust-
;
; #define ARS   0x18                 ; Register arg bytes saved on stack
; #define FRS   0x20                 ; Frame save area size in bytes
; #define LCS   0x00                 ; Locals and Outgoing arg size
; #define ARO   (LCS+FRS)            ; Stack offset to access args
; 
; All of the above defines should be undefined (#undef) at the end of each
; routine to avoid accidental use in the next routine.
;
; Given all of the above the following stack structure is expected during
; the body of a routine if all args passed in registers are saved during
; entry-
;
                                     ; 'Incoming args area'
;         Arg10_32 [A0StP+#-((10*4)+ARO)]       Arg9_32  [A0StP+#-(( 9*4)+ARO)]
;         Arg8_32  [A0StP+#-(( 8*4)+ARO)]       Arg7_32  [A0StP+#-(( 7*4)+ARO)]
;--- Call point
; D0Ar6=  Arg6_32  [A0StP+#-(( 6*4)+ARO)] D1Ar5=Arg5_32  [A0StP+#-(( 5*4)+ARO)]
; D0Ar4=  Arg4_32  [A0StP+#-(( 4*4)+ARO)] D1Ar3=Arg3_32  [A0StP+#-(( 3*4)+ARO)]
; D0Ar2=  Arg2_32  [A0StP+#-(( 2*4)+ARO)] D1Ar2=Arg1_32  [A0StP+#-(( 1*4)+ARO)]
                                     ; 'Frame area'
; A0FrP-> D0FrT, D1RtP,
;         D0.5, D1.5,
;         D0.6, D1.6,
;         D0.7, D1.7,
                                     ; 'Locals area'
;         Loc0_32  [A0StP+# (( 0*4)-LCS)],      Loc1_32 [A0StP+# (( 1*4)-LCS)]
;               .... other locals
;         Locn_32  [A0StP+# (( n*4)-LCS)]
                                     ; 'Outgoing args area'
;         Outm_32  [A0StP+#- ( m*4)]            .... other outgoing args
;         Out8_32  [A0StP+#- ( 1*4)]            Out7_32  [A0StP+#- ( 1*4)]
; A0StP-> (Out1_32-Out6_32 in regs D1Ar1-D0Ar6)
;
; The exit sequence for a non-leaf routine can use the frame pointer created
; in the entry sequence to optimise the recovery of the full state-
;
;   MGETL   D0FrT,D0.5,D0.6,D0.7,[A0FrP]
;   SUB     A0StP,A0FrP,#(ARS+FRS)
;   MOV     A0FrP,D0FrT
;   MOV     PC,D1RtP
;
; Having described the most complex non-leaf case above, it is worth noting
; that if a routine is a leaf and does not use any of the caller-preserved
; state. The routine can be implemented as-
;
;   ADD     A0StP,A0StP,#LCS
;   .... body of routine
;   SUB     A0StP,A0StP,#LCS
;   MOV     PC,D1RtP
;
; The stack adjustments can also be omitted if no local storage is required.
;
; Another exit sequence structure is more applicable if for a leaf routine
; with no local frame pointer saved/generated in which the call saved
; registers need to be saved and restored-
;
;   MSETL   [A0StP],D0.5,D0.6,D0.7   ; Hence FRS is 0x18, ARS is 0x00
;   ADD     A0StP,A0StP,#LCS
;   .... body of routine
;   GETL    D0.5,D1.5,[A0StP+#((0*8)-(FRS+LCS))]
;   GETL    D0.6,D1.6,[A0StP+#((1*8)-(FRS+LCS))]
;   GETL    D0.7,D1.7,[A0StP+#((2*8)-(FRS+LCS))]
;   SUB     A0StP,A0StP,#(ARS+FRS+LCS)
;   MOV     PC,D1RtP
;
; Lastly, to support profiling assembler code should use a fixed entry/exit
; sequence if the trigger define _GMON_ASM is defined-
;
;   #ifndef _GMON_ASM
;   ... optimised entry code
;   #else
;   ; Profiling entry case
;   MOV     D0FrT,A0FrP              ; Save and calculate new frame pointer
;   ADD     A0FrP,A0StP,#(ARS)
;   MSETL   [A0StP],...,D0FrT,... or SETL    [A0FrP],D0FrT,D1RtP
;   CALLR   D0FrT,_mcount_wrapper
;   #endif
;   ... body of routine
;   #ifndef _GMON_ASM
;   ... optimised exit code
;   #else
;   ; Profiling exit case
;   MGETL   D0FrT,...,[A0FrP]     or GETL    D0FrT,D1RtP,[A0FrP++]
;   SUB     A0StP,A0FrP,#(ARS+FRS)
;   MOV     A0FrP,D0FrT
;   MOV     PC,D1RtP
;   #endif


; -------------------------------------------------------------------------
;                         CONTROL UNIT REGISTERS
; -------------------------------------------------------------------------
;
; See the assembler guide, hardware documentation, or the field values
; defined below for some details of the use of these registers.
*/
#define TXENABLE    CT.0    /* Need to define bit-field values in these */
#define TXMODE      CT.1
#define TXSTATUS    CT.2    /* DEFAULT 0x00020000 */
#define TXRPT       CT.3
#define TXTIMER     CT.4
#ifndef METAC_0_1
#define TXL1START   CT.5
#define TXL1END     CT.6
#define TXL1COUNT   CT.7
#define TXL2START   CT.8
#define TXL2END     CT.9
#define TXL2COUNT   CT.10
#define TXBPOBITS   CT.11
#define TXMRSIZE    CT.12
#define TXTIMERI    CT.13
#define TXDRCTRL    CT.14  /* DEFAULT 0x0XXXF0F0 */
#define TXDRSIZE    CT.15
#define TXCATCH0    CT.16
#define TXCATCH1    CT.17
#define TXCATCH2    CT.18
#define TXCATCH3    CT.19

#ifdef METAC_2_1
#define TXDEFR      CT.20
#define TXCPRS      CT.21
#endif

#define TXINTERN0   CT.23
#define TXAMAREG0   CT.24
#define TXAMAREG1   CT.25
#define TXAMAREG2   CT.26
#define TXAMAREG3   CT.27
#define TXDIVTIME   CT.28   /* DEFAULT 0x00000001 */
#define TXPRIVEXT   CT.29   /* DEFAULT 0x003B0000 */
#define TXTACTCYC   CT.30
#define TXIDLECYC   CT.31
#else
#define TXTACTCYC   CT.6
#define TXIDLECYC   CT.7
#endif

/*****************************************************************************
                          CONTROL UNIT REGISTER BITS
 ****************************************************************************/
/*
 * The following registers and where appropriate the sub-fields of those
 * registers are defined for pervasive use in controlling program flow.
 */

/*
 * TXENABLE register fields - only the thread id is routinely useful
 */
#define TXENABLE_REGNUM 0
#define TXENABLE_THREAD_BITS       0x00000700
#define TXENABLE_THREAD_S          8
#define TXENABLE_REV_STEP_BITS     0x000000F0
#define TXENABLE_REV_STEP_S        4

/*
 * TXMODE register - controls extensions of the instruction set
 */
#define TXMODE_REGNUM 1
#define     TXMODE_DEFAULT  0   /* All fields default to zero */

/*
 * TXSTATUS register - contains a couple of stable bits that can be used
 *      to determine the privilege processing level and interrupt
 *      processing level of the current thread.
 */
#define TXSTATUS_REGNUM 2
#define TXSTATUS_PSTAT_BIT         0x00020000   /* -> Privilege active      */
#define TXSTATUS_PSTAT_S           17
#define TXSTATUS_ISTAT_BIT         0x00010000   /* -> In interrupt state    */
#define TXSTATUS_ISTAT_S           16

/*
 * These are all relatively boring registers, mostly full 32-bit
 */
#define TXRPT_REGNUM     3  /* Repeat counter for XFR... instructions   */
#define TXTIMER_REGNUM   4  /* Timer-- causes timer trigger on overflow */
#define TXL1START_REGNUM 5  /* Hardware Loop 1 Start-PC/End-PC/Count    */
#define TXL1END_REGNUM   6
#define TXL1COUNT_REGNUM 7
#define TXL2START_REGNUM 8  /* Hardware Loop 2 Start-PC/End-PC/Count    */
#define TXL2END_REGNUM   9
#define TXL2COUNT_REGNUM 10
#define TXBPOBITS_REGNUM 11 /* Branch predict override bits - tune perf */
#define TXTIMERI_REGNUM  13 /* Timer-- time based interrupt trigger     */

/*
 * TXDIVTIME register is routinely read to calculate the time-base for
 * the TXTIMER register.
 */
#define TXDIVTIME_REGNUM 28
#define     TXDIVTIME_DIV_BITS 0x000000FF
#define     TXDIVTIME_DIV_S    0
#define     TXDIVTIME_DIV_MIN  0x00000001   /* Maximum resolution       */
#define     TXDIVTIME_DIV_MAX  0x00000100   /* 1/1 -> 1/256 resolution  */
#define     TXDIVTIME_BASE_HZ  1000000      /* Timers run at 1Mhz @1/1  */

/*
 * TXPRIVEXT register can be consulted to decide if write access to a
 *    part of the threads register set is not permitted when in
 *    unprivileged mode (PSTAT == 0).
 */
#define TXPRIVEXT_REGNUM 29
#define     TXPRIVEXT_COPRO_BITS    0xFF000000 /* Co-processor 0-7 */
#define     TXPRIVEXT_COPRO_S       24
#define     TXPRIVEXT_TXTRIGGER_BIT 0x00020000 /* TXSTAT|TXMASK|TXPOLL */
#define     TXPRIVEXT_TXGBLCREG_BIT 0x00010000 /* Global common regs */
#define     TXPRIVEXT_CBPRIV_BIT    0x00008000 /* Mem i/f dump priv */
#define     TXPRIVEXT_ILOCK_BIT     0x00004000 /* LOCK inst priv */
#define     TXPRIVEXT_TXITACCYC_BIT 0x00002000 /* TXIDLECYC|TXTACTCYC */
#define     TXPRIVEXT_TXDIVTIME_BIT 0x00001000 /* TXDIVTIME priv */
#define     TXPRIVEXT_TXAMAREGX_BIT 0x00000800 /* TXAMAREGX priv */
#define     TXPRIVEXT_TXTIMERI_BIT  0x00000400 /* TXTIMERI  priv */
#define     TXPRIVEXT_TXSTATUS_BIT  0x00000200 /* TXSTATUS  priv */
#define     TXPRIVEXT_TXDISABLE_BIT 0x00000100 /* TXENABLE  priv */
#ifndef METAC_1_2
#define     TXPRIVEXT_MINIMON_BIT   0x00000080 /* Enable Minim features */
#define     TXPRIVEXT_OLDBCCON_BIT  0x00000020 /* Restore Static predictions */
#define     TXPRIVEXT_ALIGNREW_BIT  0x00000010 /* Align & precise checks */
#endif
#define     TXPRIVEXT_KEEPPRI_BIT   0x00000008 /* Use AMA_Priority if ISTAT=1*/
#define     TXPRIVEXT_TXTOGGLEI_BIT 0x00000001 /* TX.....I  priv */

/*
 * TXTACTCYC register - counts instructions issued for this thread
 */
#define TXTACTCYC_REGNUM  30
#define     TXTACTCYC_COUNT_MASK    0x00FFFFFF

/*
 * TXIDLECYC register - counts idle cycles
 */
#define TXIDLECYC_REGNUM  31
#define     TXIDLECYC_COUNT_MASK    0x00FFFFFF

#ifdef METAC_0_1
/* Alternate layout */
#undef TXTACTCYC_REGNUM
#undef TXIDLECYC_REGNUM
#define TXTACTCYC_REGNUM  6
#define TXIDLECYC_REGNUM  7
#endif

/*****************************************************************************
                               DSP EXTENSIONS
 ****************************************************************************/
/*
 * The following values relate to fields and controls that only a program
 * using the DSP extensions of the META instruction set need to know.
 */


#ifndef METAC_1_2
/*
 * Allow co-processor hardware to replace the read pipeline data source in
 * appropriate cases.
 */
#define TXMODE_RDCPEN_BIT       0x00800000
#endif

/*
 * Address unit addressing modes
 */
#define TXMODE_A1ADDR_BITS  0x00007000
#define TXMODE_A1ADDR_S     12
#define TXMODE_A0ADDR_BITS  0x00000700
#define TXMODE_A0ADDR_S     8
#define     TXMODE_AXADDR_MODULO 3
#define     TXMODE_AXADDR_REVB   4
#define     TXMODE_AXADDR_REVW   5
#define     TXMODE_AXADDR_REVD   6
#define     TXMODE_AXADDR_REVL   7

/*
 * Data unit OverScale select (default 0 -> normal, 1 -> top 16 bits)
 */
#define TXMODE_DXOVERSCALE_BIT	0x00000080

/*
 * Data unit MX mode select (default 0 -> MX16, 1 -> MX8)
 */
#define TXMODE_M8_BIT         0x00000040

/*
 * Data unit accumulator saturation point (default -> 40 bit accumulator)
 */
#define TXMODE_DXACCSAT_BIT 0x00000020 /* Set for 32-bit accumulator */

/*
 * Data unit accumulator saturation enable (default 0 -> no saturation)
 */
#define TXMODE_DXSAT_BIT    0x00000010

/*
 * Data unit master rounding control (default 0 -> normal, 1 -> convergent)
 */
#define TXMODE_DXROUNDING_BIT   0x00000008

/*
 * Data unit product shift for fractional arithmetic (default off)
 */
#define TXMODE_DXPRODSHIFT_BIT  0x00000004

/*
 * Select the arithmetic mode (multiply mostly) for both data units
 */
#define TXMODE_DXARITH_BITS 0x00000003
#define     TXMODE_DXARITH_32  3
#ifdef METAC_1_0
#define     TXMODE_DXARITH_24  2
#else
#ifdef METAC_1_1
#define     TXMODE_DXARITH_24  2
#else
#define     TXMODE_DXARITH_32H 2
#endif
#endif
#define     TXMODE_DXARITH_S16 1
#define     TXMODE_DXARITH_16  0

/*
 * TXMRSIZE register value only relevant when DSP modulo addressing active
 */
#define TXMRSIZE_REGNUM 12
#define     TXMRSIZE_MIN    0x0002  /* 0, 1 -> normal addressing logic */
#define     TXMRSIZE_MAX    0xFFFF

/*
 * TXDRCTRL register can be used to detect the actaul size of the DSP RAM
 * partitions allocated to this thread.
 */
#define TXDRCTRL_REGNUM 14
#define     TXDRCTRL_SINESIZE_BITS  0x0F000000
#define     TXDRCTRL_SINESIZE_S     24
#define     TXDRCTRL_RAMSZPOW_BITS  0x001F0000  /* Limit = (1<<RAMSZPOW)-1 */
#define     TXDRCTRL_RAMSZPOW_S     16
#define     TXDRCTRL_D1RSZAND_BITS  0x0000F000  /* Mask top 4 bits - D1 */
#define     TXDRCTRL_D1RSZAND_S     12
#define     TXDRCTRL_D0RSZAND_BITS  0x000000F0  /* Mask top 4 bits - D0 */
#define     TXDRCTRL_D0RSZAND_S     4
/* Given extracted RAMSZPOW and DnRSZAND fields this returns the size */
#define     TXDRCTRL_DXSIZE( Pow, AndBits )                              (\
                                  (((~(AndBits)) & 0x0f) + 1) << ((Pow)-4))

/*
 * TXDRSIZE register provides modulo addressing options for each DSP RAM
 */
#define TXDRSIZE_REGNUM 15
#define     TXDRSIZE_R1MOD_BITS       0xFFFF0000
#define     TXDRSIZE_R1MOD_S          16
#define     TXDRSIZE_R0MOD_BITS       0x0000FFFF
#define     TXDRSIZE_R0MOD_S          0

#define     TXDRSIZE_RBRAD_SCALE_BITS 0x70000000
#define     TXDRSIZE_RBRAD_SCALE_S    28
#define     TXDRSIZE_RBMODSIZE_BITS   0x0FFF0000
#define     TXDRSIZE_RBMODSIZE_S      16
#define     TXDRSIZE_RARAD_SCALE_BITS 0x00007000
#define     TXDRSIZE_RARAD_SCALE_S    12
#define     TXDRSIZE_RAMODSIZE_BITS   0x00000FFF
#define     TXDRSIZE_RAMODSIZE_S      0

/*****************************************************************************
                         DEFERRED and BUS ERROR EXTENSION
 ****************************************************************************/

/*
 * TXDEFR register - Deferred exception control
 */
#define TXDEFR_REGNUM 20
#define     TXDEFR_DEFAULT  0   /* All fields default to zero */

/*
 * Bus error state is a multi-bit positive/negative event notification from
 * the bus infrastructure. 
 */
#define     TXDEFR_BUS_ERR_BIT    0x80000000  /* Set if error (LSB STATE) */
#define     TXDEFR_BUS_ERRI_BIT   0x40000000  /* Fetch returned error */
#define     TXDEFR_BUS_STATE_BITS 0x3F000000  /* Bus event/state data */
#define     TXDEFR_BUS_STATE_S    24
#define     TXDEFR_BUS_TRIG_BIT   0x00800000  /* Set when bus error seen */

/*
 * Bus events are collected by background code in a deferred manner unless
 * selected to trigger an extended interrupt HALT trigger when they occur.
 */
#define     TXDEFR_BUS_ICTRL_BIT  0x00000080  /* Enable interrupt trigger */

/*****************************************************************************
                                  FPU EXTENSIONS
 ****************************************************************************/
/*
 * The following registers only exist in FPU enabled cores, see later section
 * enabled by defining METAG_FPU_VALUES or METAG_ALL_VALUES for details.
 */

/*
 * TXMODE register - FPU rounding mode control/status fields
 */
#define     TXMODE_FPURMODE_BITS     0x00030000
#define     TXMODE_FPURMODE_S        16
#define     TXMODE_FPURMODEWRITE_BIT 0x00040000  /* Set to change FPURMODE */

/*
 * TXDEFR register - FPU exception handling/state is a significant source
 *   of deferrable errors. Run-time S/W can move handling to interrupt level
 *   using DEFR instruction to collect state. 
 */
#define     TXDEFR_FPE_FE_BITS       0x003F0000  /* Set by FPU_FE events */
#define     TXDEFR_FPE_FE_S          16

#define     TXDEFR_FPE_INEXACT_FE_BIT   0x010000
#define     TXDEFR_FPE_UNDERFLOW_FE_BIT 0x020000
#define     TXDEFR_FPE_OVERFLOW_FE_BIT  0x040000
#define     TXDEFR_FPE_DIVBYZERO_FE_BIT 0x080000
#define     TXDEFR_FPE_INVALID_FE_BIT   0x100000
#define     TXDEFR_FPE_DENORMAL_FE_BIT  0x200000

#define     TXDEFR_FPE_ICTRL_BITS    0x000003F   /* Route to interrupts */
#define     TXDEFR_FPE_ICTRL_S       0

#define     TXDEFR_FPE_INEXACT_ICTRL_BIT   0x01
#define     TXDEFR_FPE_UNDERFLOW_ICTRL_BIT 0x02
#define     TXDEFR_FPE_OVERFLOW_ICTRL_BIT  0x04
#define     TXDEFR_FPE_DIVBYZERO_ICTRL_BIT 0x08
#define     TXDEFR_FPE_INVALID_ICTRL_BIT   0x10
#define     TXDEFR_FPE_DENORMAL_ICTRL_BIT  0x20

/*****************************************************************************
               THREAD CONTROL, ERROR, OR INTERRUPT STATE EXTENSIONS
 ****************************************************************************/
/*
 * The following values are only relevant to code that externally controls
 * threads, handles errors/interrupts, and/or set-up interrupt/error handlers
 * for subsequent use.
 */

/*
 * TXENABLE register fields - only ENABLE_BIT is potentially read/write
 */
#define TXENABLE_MAJOR_REV_BITS    0xFF000000
#define TXENABLE_MAJOR_REV_S       24
#define TXENABLE_MINOR_REV_BITS    0x00FF0000
#define TXENABLE_MINOR_REV_S       16
#define TXENABLE_CLASS_BITS        0x0000F000
#define TXENABLE_CLASS_S           12
#define TXENABLE_CLASS_DSP             0x0 /* -> DSP Thread */
#define TXENABLE_CLASS_LDSP            0x8 /* -> DSP LITE Thread */
#define TXENABLE_CLASS_GP              0xC /* -> General Purpose Thread */
#define     TXENABLE_CLASSALT_LFPU       0x2 /*  Set to indicate LITE FPU */
#define     TXENABLE_CLASSALT_FPUR8      0x1 /*  Set to indicate 8xFPU regs */
#define TXENABLE_MTXARCH_BIT       0x00000800
#define TXENABLE_STEP_REV_BITS     0x000000F0
#define TXENABLE_STEP_REV_S        4
#define TXENABLE_STOPPED_BIT       0x00000004   /* TXOFF due to ENABLE->0 */
#define TXENABLE_OFF_BIT           0x00000002   /* Thread is in off state */
#define TXENABLE_ENABLE_BIT        0x00000001   /* Set if running */

/*
 * TXSTATUS register - used by external/internal interrupt/error handler
 *    further values defined in METAC_CATCH_VALUES section of metac_x_y.inc
 */
#define TXSTATUS_CB1MARKER_BIT     0x00800000   /* -> int level mem state */
#define TXSTATUS_CBMARKER_BIT      0x00400000   /* -> mem i/f state dumped */
#define TXSTATUS_MEM_FAULT_BITS    0x00300000
#define TXSTATUS_MEM_FAULT_S       20
#define     TXSTATUS_MEMFAULT_NONE  0x0 /* -> No memory fault       */
#define     TXSTATUS_MEMFAULT_GEN   0x1 /* -> General fault         */
#define     TXSTATUS_MEMFAULT_PF    0x2 /* -> Page fault            */
#define     TXSTATUS_MEMFAULT_RO    0x3 /* -> Read only fault       */
#define TXSTATUS_MAJOR_HALT_BITS   0x000C0000
#define TXSTATUS_MAJOR_HALT_S      18
#define     TXSTATUS_MAJHALT_TRAP 0x0   /* -> SWITCH inst used      */
#define     TXSTATUS_MAJHALT_INST 0x1   /* -> Unknown inst or fetch */
#define     TXSTATUS_MAJHALT_PRIV 0x2   /* -> Internal privilege    */
#define     TXSTATUS_MAJHALT_MEM  0x3   /* -> Memory i/f fault      */
#define TXSTATUS_L_STEP_BITS       0x00000800   /* -> Progress of L oper    */
#define TXSTATUS_LSM_STEP_BITS     0x00000700   /* -> Progress of L/S mult  */
#define TXSTATUS_LSM_STEP_S        8
#define TXSTATUS_FLAG_BITS         0x0000001F   /* -> All the flags         */
#define TXSTATUS_SCC_BIT           0x00000010   /* -> Split-16 flags ...    */
#define TXSTATUS_SCF_LZ_BIT        0x00000008   /* -> Split-16 Low  Z flag  */
#define TXSTATUS_SCF_HZ_BIT        0x00000004   /* -> Split-16 High Z flag  */
#define TXSTATUS_SCF_HC_BIT        0x00000002   /* -> Split-16 High C flag  */
#define TXSTATUS_SCF_LC_BIT        0x00000001   /* -> Split-16 Low  C flag  */
#define TXSTATUS_CF_Z_BIT          0x00000008   /* -> Condition Z flag      */
#define TXSTATUS_CF_N_BIT          0x00000004   /* -> Condition N flag      */
#define TXSTATUS_CF_O_BIT          0x00000002   /* -> Condition O flag      */
#define TXSTATUS_CF_C_BIT          0x00000001   /* -> Condition C flag      */

/*
 * TXCATCH0-3 register contents may store information on a memory operation
 * that has failed if the bit TXSTATUS_CBMARKER_BIT is set.
 */
#define TXCATCH0_REGNUM 16
   /* Details in METAC_CATCH_VALUES section of metac_x_y.inc */
#define TXCATCH1_REGNUM 17
#define     TXCATCH1_ADDR_BITS   0xFFFFFFFF   /* TXCATCH1 is Addr 0-31 */
#define     TXCATCH1_ADDR_S      0
#define TXCATCH2_REGNUM 18
#define     TXCATCH2_DATA0_BITS  0xFFFFFFFF   /* TXCATCH2 is Data 0-31 */
#define     TXCATCH2_DATA0_S     0
#define TXCATCH3_REGNUM 19
#define     TXCATCH3_DATA1_BITS  0xFFFFFFFF   /* TXCATCH3 is Data 32-63 */
#define     TXCATCH3_DATA1_S     0

/*
 * TXDIVTIME register used to hold the partial base address of memory i/f
 * state dump area. Now deprecated.
 */
#define     TXDIVTIME_CBBASE_MASK    0x03FFFE00
#define     TXDIVTIME_CBBASE_LINBASE 0x80000000
#define     TXDIVTIME_CBBASE_LINBOFF 0x00000000 /* BGnd state */
#define     TXDIVTIME_CBBASE_LINIOFF 0x00000100 /* Int  state */

/*
 * TXDIVTIME register used to indicate if the read pipeline was dirty when a
 * thread was interrupted, halted, or generated an exception. It is invalid
 * to attempt to issue a further pipeline read address while the read
 * pipeline is in the dirty state.
 */
#define     TXDIVTIME_RPDIRTY_BIT   0x80000000

/*
 * Further bits in the TXDIVTIME register allow interrupt handling code to
 * short-cut the discovery the most significant bit last read from TXSTATI.
 *
 * This is the bit number of the trigger line that a low level interrupt
 * handler should acknowledge and then perhaps the index of a corresponding
 * handler function.
 */
#define     TXDIVTIME_IRQENC_BITS   0x0F000000
#define     TXDIVTIME_IRQENC_S      24

/*
 * TXPRIVEXT register can be used to single step thread execution and
 * enforce synchronous memory i/f address checking for debugging purposes.
 */
#define     TXPRIVEXT_TXSTEP_BIT    0x00000004
#define     TXPRIVEXT_MEMCHECK_BIT  0x00000002

/*
 * TXINTERNx registers holds internal state information for H/W debugging only
 */
#define TXINTERN0_REGNUM 23
#define     TXINTERN0_LOCK2_BITS  0xF0000000
#define     TXINTERN0_LOCK2_S     28
#define     TXINTERN0_LOCK1_BITS  0x0F000000
#define     TXINTERN0_LOCK1_S     24
#define     TXINTERN0_TIFDF_BITS  0x0000F000
#define     TXINTERN0_TIFDF_S     12
#define     TXINTERN0_TIFIB_BITS  0x00000F00
#define     TXINTERN0_TIFIB_S     8
#define     TXINTERN0_TIFAF_BITS  0x000000F0
#define     TXINTERN0_TIFAF_S     4
#define     TXINTERN0_MSTATE_BITS 0x0000000F
#define     TXINTERN0_MSTATE_S    0

/*
 * TXSTAT, TXMASK, TXPOLL, TXSTATI, TXMASKI, TXPOLLI registers from trigger
 * bank all have similar contents (upper kick count bits not in MASK regs)
 */
#define TXSTAT_REGNUM  0
#define     TXSTAT_TIMER_BIT    0x00000001
#define     TXSTAT_TIMER_S      0
#define     TXSTAT_KICK_BIT     0x00000002
#define     TXSTAT_KICK_S       1
#define     TXSTAT_DEFER_BIT    0x00000008
#define     TXSTAT_DEFER_S      3
#define     TXSTAT_EXTTRIG_BITS 0x0000FFF0
#define     TXSTAT_EXTTRIG_S    4
#define     TXSTAT_FPE_BITS     0x003F0000
#define     TXSTAT_FPE_S        16
#define     TXSTAT_FPE_DENORMAL_BIT    0x00200000
#define     TXSTAT_FPE_DENORMAL_S      21
#define     TXSTAT_FPE_INVALID_BIT     0x00100000
#define     TXSTAT_FPE_INVALID_S       20
#define     TXSTAT_FPE_DIVBYZERO_BIT   0x00080000
#define     TXSTAT_FPE_DIVBYZERO_S     19
#define     TXSTAT_FPE_OVERFLOW_BIT    0x00040000
#define     TXSTAT_FPE_OVERFLOW_S      18
#define     TXSTAT_FPE_UNDERFLOW_BIT   0x00020000
#define     TXSTAT_FPE_UNDERFLOW_S     17
#define     TXSTAT_FPE_INEXACT_BIT     0x00010000
#define     TXSTAT_FPE_INEXACT_S       16
#define     TXSTAT_BUSERR_BIT          0x00800000   /* Set if bus error/ack state */
#define     TXSTAT_BUSERR_S            23
#define         TXSTAT_BUSSTATE_BITS     0xFF000000	/* Read only */
#define         TXSTAT_BUSSTATE_S        24
#define     TXSTAT_KICKCNT_BITS 0xFFFF0000
#define     TXSTAT_KICKCNT_S    16
#define TXMASK_REGNUM  1
#define TXSTATI_REGNUM 2
#define     TXSTATI_BGNDHALT_BIT    0x00000004
#define TXMASKI_REGNUM 3
#define TXPOLL_REGNUM  4
#define TXPOLLI_REGNUM 6

/*
 * TXDRCTRL register can be used to partition the DSP RAM space available to
 * this thread at startup. This is achieved by offsetting the region allocated
 * to each thread.
 */
#define     TXDRCTRL_D1PARTOR_BITS  0x00000F00  /* OR's into top 4 bits */
#define     TXDRCTRL_D1PARTOR_S     8
#define     TXDRCTRL_D0PARTOR_BITS  0x0000000F  /* OR's into top 4 bits */
#define     TXDRCTRL_D0PARTOR_S     0
/* Given extracted Pow and Or fields this is threads base within DSP RAM */
#define     TXDRCTRL_DXBASE( Pow, Or )  ((Or)<<((Pow)-4))

/*****************************************************************************
                   META GENERIC OR CHIP SPECIFIC VALUE CONTROLS
 ****************************************************************************/
/*
 * Further generic META or chip specific values can be conditionally
 * defined by defining one of the following values to enable related
 * definitions.
 */

#ifdef METAG_ALL_VALUES
#ifndef _METAG_ALL_VALUES_
#define _METAG_ALL_VALUES_
#define METAG_LIN_VALUES    /* we want to understand META memory map   */
#define METAG_TRIG_VALUES   /* we want to understand META triggers     */
#define METAG_PRIV_VALUES   /* we want to understand META privilege   */
#define METAG_MMCU_VALUES   /* we want to understand MMCU tables       */
#define METAG_KICK_VALUES   /* we want to understand KICK registers    */
#define METAG_TXUXXRX_VALUES    /* we want to understand GLOBAL reg access */
#define METAG_DATAW_VALUES  /* we want to understand DATAW watchpoints */
#define METAG_CODEB_VALUES  /* we want to understand CODEB breakpoints */
#define METAG_FPU_VALUES    /* we want to understand the FPU */
#endif
#endif

/*****************************************************************************
                     META MEMORY MAP LINEAR ADDRESS VALUES
 ****************************************************************************/
/*
 * COMMON MEMORY MAP
 * -----------------
 */

/* #ifdef METAG_LIN_VALUES */
#ifndef _METAG_LIN_VALUES_
#define _METAG_LIN_VALUES_

#define LINSYSTEM_BASE  0x00200000
#define LINSYSTEM_LIMIT 0x07FFFFFF

#ifdef METAG_LIN_VALUES
/* Linear cache flush now implemented via DCACHE instruction. These defines
   related to a special region that used to exist for achieving cache flushes.
 */
#define         LINSYSLFLUSH_S 0
#endif

#define     LINSYSRES0_BASE     0x00200000
#define     LINSYSRES0_LIMIT    0x01FFFFFF

#define     LINSYSCUSTOM_BASE 0x02000000
#define     LINSYSCUSTOM_LIMIT   0x02FFFFFF

#define     LINSYSEXPAND_BASE 0x03000000
#define     LINSYSEXPAND_LIMIT   0x03FFFFFF

#define     LINSYSEVENT_BASE  0x04000000
#define         LINSYSEVENT_WR_ATOMIC_UNLOCK    0x04000000
#define         LINSYSEVENT_WR_ATOMIC_LOCK      0x04000040
#define         LINSYSEVENT_WR_CACHE_DISABLE    0x04000080
#define         LINSYSEVENT_WR_CACHE_ENABLE     0x040000C0
#define         LINSYSEVENT_WR_COMBINE_FLUSH    0x04000100 
#define         LINSYSEVENT_WR_FENCE            0x04000140
#define     LINSYSEVENT_LIMIT   0x04000FFF

#define     LINSYSCFLUSH_BASE   0x04400000
#define         LINSYSCFLUSH_DCACHE_LINE    0x04400000
#define         LINSYSCFLUSH_ICACHE_LINE    0x04500000
#define         LINSYSCFLUSH_MMCU           0x04700000
#ifndef METAC_1_2
#define         LINSYSCFLUSH_TxMMCU_BASE    0x04700020
#define         LINSYSCFLUSH_TxMMCU_STRIDE  0x00000008
#endif
#define         LINSYSCFLUSH_ADDR_BITS      0x000FFFFF
#define         LINSYSCFLUSH_ADDR_S         0
#define     LINSYSCFLUSH_LIMIT  0x047FFFFF

#define     LINSYSCTRL_BASE     0x04800000
#define     LINSYSCTRL_LIMIT    0x04FFFFFF

#define     LINSYSMTABLE_BASE   0x05000000
#define     LINSYSMTABLE_LIMIT  0x05FFFFFF

#define     LINSYSDIRECT_BASE   0x06000000
#define     LINSYSDIRECT_LIMIT  0x07FFFFFF

#define LINLOCAL_BASE   0x08000000
#define LINLOCAL_LIMIT  0x7FFFFFFF

#define LINCORE_BASE    0x80000000
#define LINCORE_LIMIT   0x87FFFFFF

#define LINCORE_CODE_BASE  0x80000000
#define LINCORE_CODE_LIMIT 0x81FFFFFF

#define LINCORE_DATA_BASE  0x82000000
#define LINCORE_DATA_LIMIT 0x83FFFFFF


/* The core can support locked icache lines in this region */
#define LINCORE_ICACHE_BASE  0x84000000
#define LINCORE_ICACHE_LIMIT 0x85FFFFFF

/* The core can support locked dcache lines in this region */
#define LINCORE_DCACHE_BASE  0x86000000
#define LINCORE_DCACHE_LIMIT 0x87FFFFFF

#define LINGLOBAL_BASE  0x88000000
#define LINGLOBAL_LIMIT 0xFFFDFFFF
#endif
/* #endif */

/*****************************************************************************
           INTER-THREAD KICK REGISTERS FOR SOFTWARE EVENT GENERATION
 ****************************************************************************/
/*
 * These values define memory mapped registers that can be used to supply
 * kicks to threads that service arbitrary software events.
 */

#ifdef METAG_KICK_VALUES
#ifndef _METAG_KICK_VALUES_
#define _METAG_KICK_VALUES_
#define T0KICK     0x04800800   /* Background kick 0     */
#define     TXXKICK_MAX 0xFFFF  /* Maximum kicks */
#define     TnXKICK_STRIDE      0x00001000  /* Thread scale value    */
#define     TnXKICK_STRIDE_S    12
#define T0KICKI    0x04800808   /* Interrupt kick 0      */
#define     TXIKICK_OFFSET  0x00000008  /* Int level offset value */
#define T1KICK     0x04801800   /* Background kick 1     */
#define T1KICKI    0x04801808   /* Interrupt kick 1      */
#define T2KICK     0x04802800   /* Background kick 2     */
#define T2KICKI    0x04802808   /* Interrupt kick 2      */
#define T3KICK     0x04803800   /* Background kick 3     */
#define T3KICKI    0x04803808   /* Interrupt kick 3      */
#endif
#endif

/*****************************************************************************
                  GLOBAL REGISTER ACCESS RESOURCES
 ****************************************************************************/
/*
 * These values define memory mapped registers that allow access to the
 * internal state of all threads in order to allow global set-up of thread
 * state and external handling of thread events, errors, or debugging.
 *
 * The actual unit and register index values needed to access individul
 * registers are chip specific see - METAC_TXUXX_VALUES in metac_x_y.inc.
 * However two C array initialisers TXUXX_MASKS and TGUXX_MASKS will always be
 * defined to allow arbitrary loading, display, and saving of all valid
 * register states without detailed knowledge of their purpose - TXUXX sets
 * bits for all valid registers and TGUXX sets bits for the sub-set which are
 * global.
 */

#ifdef METAG_TXUXXRX_VALUES
#ifndef _METAG_TXUXXRX_VALUES_
#define _METAG_TXUXXRX_VALUES_
#define T0UCTREG0   0x04800000  /* Access to all CT regs */
#define TnUCTRX_STRIDE      0x00001000  /* Thread scale value    */
#define TXUCTREGn_STRIDE    0x00000008  /* Register scale value  */

#define TXUXXRXDT  0x0480FFF0   /* Data to/from any threads reg */
#define TXUXXRXRQ  0x0480FFF8   
#define     TXUXXRXRQ_DREADY_BIT 0x80000000  /* Poll for done */
#define     TXUXXRXRQ_DSPEXT_BIT 0x00020000  /* Addr DSP Regs */
#define     TXUXXRXRQ_RDnWR_BIT  0x00010000  /* Set for read  */
#define     TXUXXRXRQ_TX_BITS    0x00003000  /* Thread number */
#define     TXUXXRXRQ_TX_S       12
#define     TXUXXRXRQ_RX_BITS    0x000001F0  /* Register num  */
#define     TXUXXRXRQ_RX_S       4
#define         TXUXXRXRQ_DSPRARD0    0      /* DSP RAM A Read Pointer 0 */
#define         TXUXXRXRQ_DSPRARD1    1      /* DSP RAM A Read Pointer 1 */
#define         TXUXXRXRQ_DSPRAWR0    2      /* DSP RAM A Write Pointer 0 */
#define         TXUXXRXRQ_DSPRAWR2    3      /* DSP RAM A Write Pointer 1 */
#define         TXUXXRXRQ_DSPRBRD0    4      /* DSP RAM B Read Pointer 0 */
#define         TXUXXRXRQ_DSPRBRD1    5      /* DSP RAM B Read Pointer 1 */
#define         TXUXXRXRQ_DSPRBWR0    6      /* DSP RAM B Write Pointer 0 */
#define         TXUXXRXRQ_DSPRBWR1    7      /* DSP RAM B Write Pointer 1 */
#define         TXUXXRXRQ_DSPRARINC0  8      /* DSP RAM A Read Increment 0 */
#define         TXUXXRXRQ_DSPRARINC1  9      /* DSP RAM A Read Increment 1 */
#define         TXUXXRXRQ_DSPRAWINC0 10      /* DSP RAM A Write Increment 0 */
#define         TXUXXRXRQ_DSPRAWINC1 11      /* DSP RAM A Write Increment 1 */
#define         TXUXXRXRQ_DSPRBRINC0 12      /* DSP RAM B Read Increment 0 */
#define         TXUXXRXRQ_DSPRBRINC1 13      /* DSP RAM B Read Increment 1 */
#define         TXUXXRXRQ_DSPRBWINC0 14      /* DSP RAM B Write Increment 0 */
#define         TXUXXRXRQ_DSPRBWINC1 15      /* DSP RAM B Write Increment 1 */

#define         TXUXXRXRQ_ACC0L0     16      /* Accumulator 0 bottom 32-bits */
#define         TXUXXRXRQ_ACC1L0     17      /* Accumulator 1 bottom 32-bits */
#define         TXUXXRXRQ_ACC2L0     18      /* Accumulator 2 bottom 32-bits */
#define         TXUXXRXRQ_ACC3L0     19      /* Accumulator 3 bottom 32-bits */
#define         TXUXXRXRQ_ACC0HI     20      /* Accumulator 0 top 8-bits */
#define         TXUXXRXRQ_ACC1HI     21      /* Accumulator 1 top 8-bits */
#define         TXUXXRXRQ_ACC2HI     22      /* Accumulator 2 top 8-bits */
#define         TXUXXRXRQ_ACC3HI     23      /* Accumulator 3 top 8-bits */
#define     TXUXXRXRQ_UXX_BITS   0x0000000F  /* Unit number   */
#define     TXUXXRXRQ_UXX_S      0
#endif
#endif

/*****************************************************************************
            PRIVILEGE CONTROL VALUES FOR MEMORY MAPPED RESOURCES
 ****************************************************************************/
/*
 * These values define memory mapped registers that give control over and
 * the privilege required to access other memory mapped resources. These
 * registers themselves always require privilege to update them.
 */

#ifdef METAG_PRIV_VALUES
#ifndef _METAG_PRIV_VALUES_
#define _METAG_PRIV_VALUES_
#define TXPRIVREG_STRIDE    0x8 /* Delta between per-thread regs */
#define TXPRIVREG_STRIDE_S  3

/*
 * Each bit 0 to 15 defines privilege required to access internal register
 * regions 0x04800000 to 0x048FFFFF in 64k chunks
 */
#define T0PIOREG    0x04810100
#define T1PIOREG    0x04810108
#define T2PIOREG    0x04810110
#define T3PIOREG    0x04810118

/*
 * Each bit 0 to 31 defines privilege required to use the pair of
 * system events implemented as writee in the regions 0x04000000 to
 * 0x04000FFF in 2*64 byte chunks.
 */
#define T0PSYREG    0x04810180
#define T1PSYREG    0x04810188
#define T2PSYREG    0x04810190
#define T3PSYREG    0x04810198
#endif
#endif

/*****************************************************************************
            H/W TRIGGER STATE/LEVEL REGISTERS AND H/W TRIGGER VECTORS
 ****************************************************************************/
/*
 * These values define memory mapped registers that give control over and
 * the state of hardware trigger sources both external to the META processor
 * and internal to it.
 */

#ifdef METAG_TRIG_VALUES
#ifndef _METAG_TRIG_VALUES_
#define _METAG_TRIG_VALUES_
#define HWSTATMETA  0x04820000  /* Hardware status/clear META trig */
#define         HWSTATMETA_T0HALT_BITS 0xF
#define         HWSTATMETA_T0HALT_S    0
#define     HWSTATMETA_T0BHALT_BIT 0x1  /* Background HALT */
#define     HWSTATMETA_T0IHALT_BIT 0x2  /* Interrupt HALT  */
#define     HWSTATMETA_T0PHALT_BIT 0x4  /* PF/RO Memory HALT */
#define     HWSTATMETA_T0AMATR_BIT 0x8  /* AMA trigger */
#define     HWSTATMETA_TnINT_S     4    /* Shift by (thread*4) */
#define HWSTATEXT   0x04820010  /* H/W status/clear external trigs  0-31 */
#define HWSTATEXT2  0x04820018  /* H/W status/clear external trigs 32-63 */
#define HWSTATEXT4  0x04820020  /* H/W status/clear external trigs 64-95 */
#define HWSTATEXT6  0x04820028  /* H/W status/clear external trigs 96-128 */
#define HWLEVELEXT  0x04820030  /* Edge/Level type of external trigs  0-31 */
#define HWLEVELEXT2 0x04820038  /* Edge/Level type of external trigs 32-63 */
#define HWLEVELEXT4 0x04820040  /* Edge/Level type of external trigs 64-95 */
#define HWLEVELEXT6 0x04820048  /* Edge/Level type of external trigs 96-128 */
#define     HWLEVELEXT_XXX_LEVEL 1  /* Level sense logic in HWSTATEXTn */
#define     HWLEVELEXT_XXX_EDGE  0
#define HWMASKEXT   0x04820050	/* Enable/disable of external trigs  0-31 */
#define HWMASKEXT2  0x04820058	/* Enable/disable of external trigs 32-63 */
#define HWMASKEXT4  0x04820060	/* Enable/disable of external trigs 64-95 */
#define HWMASKEXT6  0x04820068	/* Enable/disable of external trigs 96-128 */
#define T0VECINT_BHALT  0x04820500  /* Background HALT trigger vector */
#define     TXVECXXX_BITS   0xF       /* Per-trigger vector vals 0,1,4-15 */
#define     TXVECXXX_S  0
#define T0VECINT_IHALT  0x04820508  /* Interrupt HALT */
#define T0VECINT_PHALT  0x04820510  /* PF/RO memory fault */
#define T0VECINT_AMATR  0x04820518  /* AMA trigger */
#define     TnVECINT_STRIDE 0x00000020  /* Per thread stride */
#define HWVEC0EXT   0x04820700  /* Vectors for external triggers  0-31 */
#define HWVEC20EXT  0x04821700  /* Vectors for external triggers 32-63 */
#define HWVEC40EXT  0x04822700  /* Vectors for external triggers 64-95 */
#define HWVEC60EXT  0x04823700  /* Vectors for external triggers 96-127 */
#define     HWVECnEXT_STRIDE 0x00000008 /* Per trigger stride */
#define HWVECnEXT_DEBUG 0x1         /* Redirect trigger to debug i/f */
#endif
#endif

/*
 * CORE HWCODE-BREAKPOINT REGISTERS/VALUES
 * ---------------------------------------
 */
#ifdef METAG_CODEB_VALUES
#ifndef _METAG_CODEB_VALUES_
#define _METAG_CODEB_VALUES_
#define CODEB0ADDR         0x0480FF00  /* Address specifier */
#define     CODEBXADDR_MATCHX_BITS 0xFFFFFFFC
#define     CODEBXADDR_MATCHX_S    2
#define CODEB0CTRL         0x0480FF08  /* Control */
#define     CODEBXCTRL_MATEN_BIT   0x80000000   /* Match 'Enable'  */
#define     CODEBXCTRL_MATTXEN_BIT 0x10000000   /* Match threadn enable */
#define     CODEBXCTRL_HITC_BITS   0x00FF0000   /* Hit counter   */
#define     CODEBXCTRL_HITC_S      16
#define           CODEBXHITC_NEXT  0xFF     /* Next 'hit' will trigger */
#define           CODEBXHITC_HIT1  0x00     /* No 'hits' after trigger */
#define     CODEBXCTRL_MMASK_BITS  0x0000FFFC   /* Mask ADDR_MATCH bits */
#define     CODEBXCTRL_MMASK_S     2
#define     CODEBXCTRL_MATLTX_BITS 0x00000003   /* Match threadn LOCAL addr */
#define     CODEBXCTRL_MATLTX_S    0            /* Match threadn LOCAL addr */
#define CODEBnXXXX_STRIDE      0x00000010  /* Stride between CODEB reg sets */
#define CODEBnXXXX_STRIDE_S    4
#define CODEBnXXXX_LIMIT       3           /* Sets 0-3 */
#endif
#endif

/*
 * CORE DATA-WATCHPOINT REGISTERS/VALUES
 * -------------------------------------
 */
#ifdef METAG_DATAW_VALUES
#ifndef _METAG_DATAW_VALUES_
#define _METAG_DATAW_VALUES_
#define DATAW0ADDR         0x0480FF40  /* Address specifier */
#define     DATAWXADDR_MATCHR_BITS 0xFFFFFFF8
#define     DATAWXADDR_MATCHR_S    3
#define     DATAWXADDR_MATCHW_BITS 0xFFFFFFFF
#define     DATAWXADDR_MATCHW_S    0
#define DATAW0CTRL         0x0480FF48  /* Control */
#define     DATAWXCTRL_MATRD_BIT   0x80000000   /* Match 'Read'  */
#ifndef METAC_1_2
#define     DATAWXCTRL_MATNOTTX_BIT 0x20000000  /* Invert threadn enable */
#endif
#define     DATAWXCTRL_MATWR_BIT   0x40000000   /* Match 'Write' */
#define     DATAWXCTRL_MATTXEN_BIT 0x10000000   /* Match threadn enable */
#define     DATAWXCTRL_WRSIZE_BITS 0x0F000000   /* Write Match Size */
#define     DATAWXCTRL_WRSIZE_S    24
#define         DATAWWRSIZE_ANY   0         /* Any size transaction matches */ 
#define         DATAWWRSIZE_8BIT  1     /* Specific sizes ... */
#define         DATAWWRSIZE_16BIT 2
#define         DATAWWRSIZE_32BIT 3
#define         DATAWWRSIZE_64BIT 4
#define     DATAWXCTRL_HITC_BITS   0x00FF0000   /* Hit counter   */
#define     DATAWXCTRL_HITC_S      16
#define           DATAWXHITC_NEXT  0xFF     /* Next 'hit' will trigger */
#define           DATAWXHITC_HIT1  0x00     /* No 'hits' after trigger */
#define     DATAWXCTRL_MMASK_BITS 0x0000FFF8    /* Mask ADDR_MATCH bits */
#define     DATAWXCTRL_MMASK_S    3
#define     DATAWXCTRL_MATLTX_BITS 0x00000003   /* Match threadn LOCAL addr */
#define     DATAWXCTRL_MATLTX_S    0            /* Match threadn LOCAL addr */
#define DATAW0DMATCH0       0x0480FF50 /* Write match data */
#define DATAW0DMATCH1       0x0480FF58
#define DATAW0DMASK0        0x0480FF60 /* Write match data mask */
#define DATAW0DMASK1        0x0480FF68
#define DATAWnXXXX_STRIDE      0x00000040  /* Stride between DATAW reg sets */
#define DATAWnXXXX_STRIDE_S    6
#define DATAWnXXXX_LIMIT       1           /* Sets 0,1 */
#endif
#endif

/*
 * DETAILED FPU RELATED VALUES
 * ---------------------------
 */
#ifdef METAG_FPU_VALUES
#ifndef _METAG_FPU_VALUES_
#define _METAG_FPU_VALUES_
/*
 * Rounding mode field in TXMODE can hold a number of logical values
 */
#define METAG_FPURMODE_TONEAREST  0x0      /* Default */
#define METAG_FPURMODE_TOWARDZERO 0x1
#define METAG_FPURMODE_UPWARD     0x2
#define METAG_FPURMODE_DOWNWARD   0x3

/*
 * In order to set the TXMODE register field that controls the rounding mode
 * an extra bit must be set in the value written versus that read in order
 * to gate writes to the rounding mode field. This allows other non-FPU code
 * to modify TXMODE without knowledge of the FPU units presence and not
 * influence the FPU rounding mode. This macro adds the required bit so new
 * rounding modes are accepted.
 */
#define TXMODE_FPURMODE_SET( FPURMode )                           (\
        TXMODE_FPURMODEWRITE_BIT + ((FPURMode)<<TXMODE_FPURMODE_S) )

/*
 * To successfully restore TXMODE to zero at the end of the function the
 * following value (rather than zero) must be used.
 */
#define TXMODE_FPURMODE_RESET (TXMODE_FPURMODEWRITE_BIT)

/*
 * In TXSTATUS a special bit exists to indicate if FPU H/W has been accessed
 * since it was last reset.
 */
#define TXSTATUS_FPACTIVE_BIT  0x01000000

/*
 * Exception state (see TXDEFR_FPU_FE_*) and enabling (for interrupt
 * level processing (see TXDEFR_FPU_ICTRL_*) are controlled by similar
 * bit mask locations within each field.
 */
#define METAG_FPU_FE_INEXACT   0x01
#define METAG_FPU_FE_UNDERFLOW 0x02
#define METAG_FPU_FE_OVERFLOW  0x04
#define METAG_FPU_FE_DIVBYZERO 0x08
#define METAG_FPU_FE_INVALID   0x10
#define METAG_FPU_FE_DENORMAL  0x20
#define METAG_FPU_FE_ALL_EXCEPT (METAG_FPU_FE_INEXACT   | \
                                 METAG_FPU_FE_UNDERFLOW | \
                                 METAG_FPU_FE_OVERFLOW  | \
                                 METAG_FPU_FE_DIVBYZERO | \
                                 METAG_FPU_FE_INVALID   | \
                                 METAG_FPU_FE_DENORMAL)
#endif
#endif

/*****************************************************************************
                            CORE SPECIFIC VALUES
 ****************************************************************************/

#ifdef METAC_1_2
#include "metac_1_2.inc"
#endif
#ifdef METAC_2_1
#include "metac_2_1.inc"
#endif

/*
 * Target dependent physical memory map
 * ------------------------------------
 */
#ifdef METAC_PHYSMEM_VALUES
#ifndef _METAC_PHYSMEM_VALUES_
#define _METAC_PHYSMEM_VALUES_
/*
 * These are effectively example values as they describe what the core
 * simulator implements. Other values can be used in real target systems
 * define these in machine.inc after defining _METAC_PHYSMEM_VALUES_ to
 * disable these defaults.
 */

/* The core simulator can support 1MB of internal ROM/RAM */
#define PHYSMEM_INTMEM_BASE             0x80000000
#define PHYSMEM_INTMEM_LIMIT            0x800FFFFF

/* The core simulator can support 32MBytes of SDRAM */
#define PHYSMEM_SDRAM_BASE              0xB0000000
#define PHYSMEM_SDRAM_LIMIT             0xB1FFFFFF

/* The core simulator can support 8MB of Local Bus memory */
/* Local Bus CS0 */
#define PHYSMEM_MEM0_BASE               0xC0000000
#define PHYSMEM_MEM0_LIMIT              0xC0FFFFFF

/* Local Bus CS1 */
#define PHYSMEM_MEM1_BASE               0xC1000000
#define PHYSMEM_MEM1_LIMIT              0xC1FFFFFF

/* 128KB of Core Code ROM */
#define PHYSMEM_CODEROMMEM_BASE         0x80000000
#define PHYSMEM_CODEROMMEM_LIMIT        0x8001FFFF

/* 32KB of Core Code RAM */
#define PHYSMEM_CODERAMMEM_BASE         0x80020000
#define PHYSMEM_CODERAMMEM_LIMIT        0x80027FFF

/* 64KB of Core Data RAM */
#define PHYSMEM_DATARAMMEM_BASE         0x82000000
#define PHYSMEM_DATARAMMEM_LIMIT        0x8200FFFF

#endif
#endif

/*
 * These values are historical but allow code to be built to work with all
 * META Cores.
 */
#ifdef METAC_CATCHX_VALUES
#ifndef _METAC_CATCHX_VALUES_
#define _METAC_CATCHX_VALUES_
#define CATCH_ENTRIES      12       /* Twelve catch buffer slots */
#define CATCH_ENTRY_BYTES  16       /* Overall size of each entry */
#define CATCH_ENTRY_S      4
#define CATCH_E0VALID_BIT  0x80000000   /* Set for Read, Load, or Write     */
#define CATCH_E0WATCHS_BIT 0x00040000   /* Set if Data Watch set fault      */
#define CATCH_E0WATCH1_BIT 0x00020000   /* Set if Data Watch 1 would match  */
#define CATCH_E0WATCH0_BIT 0x00010000   /* Set if Data Watch 0 would match  */
#define CATCH_E0FAULT_BITS 0x0000C000   /* See TXSTATUS_MEMFAULT_* values   */
#define CATCH_E0FAULT_S    14
#define CATCH_E0PRIV_BIT   0x00001000   /* Privilege of each transaction   */

/* Load operations are indicated by the LOAD and READ bits being set */
#define CATCH_E0READ_BIT   0x00002000   /* Set for Read, or Load cases      */
#define CATCH_E0LOAD_BIT   0x00000800   /* Indicates Load operation vs Read */
#define CATCH_E0LM16_BIT   0x00000400      /* Load M16 flag */
#define CATCH_E0L2L1_BITS  0x00000300      /* Load data size L2,L1 */
#define CATCH_E0L2L1_S     8
#define CATCH_E0DRXX_BITS  0x000000F8      /* Load destination reg 0-31 */
#define CATCH_E0DRXX_S     3
#define CATCH_E0DUX_BITS   0x00000007      /* Load destination unit 0-7 */
#define CATCH_E0DUX_S      0

/* Read operations are indicated by the READ bit being set by itself */
#define CATCH_E0RAXX_BITS  0x0000000F  /* Set for Read operations only     */
#define CATCH_E0RAXX_S     0

/* Write operations are all that remain if READ is not set */
#define CATCH_E0WMASK_BITS 0x000000FF      /* Write byte lane mask */
#define CATCH_E0WMASK_S    0

/* For all VALID slots the three remaining 32-bit words are similar */
#define CATCH_E1ADDR_BITS  0xFFFFFFFF   /* Second 32-bit word is Addr 0-31  */
#define CATCH_E1ADDR_S     0
#define CATCH_E2DATA0_BITS 0xFFFFFFFF   /* Third 32-bit word is Data 0-31   */
#define CATCH_E2DATA0_S    0
#define CATCH_E3DATA1_BITS 0xFFFFFFFF   /* Fourth 32-bit word is Data 32-63 */
#define CATCH_E3DATA1_S    0
#endif
#endif

/* Support for INCLUDEC feature, some C features need workarounds */
#ifndef __STDC__
#ifndef VOLATILE
#define VOLATILE
#endif
#endif /* ifndef __STDC__ */

#ifndef __ANYC__
#ifndef lword
#define lword __extension__ long long
#endif
#endif /* ifndef __ANYC__ */

#ifdef METAC_MINIM_ENC
/* Calculate program counter offsets for MINIM sections.
 * When using MINIM, program counter offsets need to account for long
 * and short encodings used in MINIM. The toolkit will ensure that these
 * offsets remain correct if the MINIM is later converted to META.
 */
#define METAC_PC_OFFSET(LONG, SHORT) ((LONG) * 8 + (SHORT) * 4)
#else
/* For internal use only.
 * Calculate the program counter offsets for META
 */
#define METAC_PC_OFFSET(LONG, SHORT) ((LONG) * 4 + (SHORT) * 4)
#endif

#endif /* ifndef _METAG_INC_ */

/* End of metag.inc */
