/*
 * metac_2_1.inc
 *
 * Copyright (C) 2007, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * Assembler header for defining target specifics (v2.0)
 *
 *            defines :
 *
 *              __ANYC__ - implies inclusion in any C compile (i.e. not assembly)
 *              __STDC__ - implies inclusion in any ANSI C compile
 *             METAC_2_1 - set if this specific core is being targeted
 *      METAC_..._VALUES - set if various core specific META defines needed
 *      METAC_ALL_VALUES - set if you want all data possible to be defined
 */

#ifdef METAC_2_1
#ifndef _METAC_2_1_H_
#define _METAC_2_1_H_

/* This enables all core specific data to be included */
#ifdef METAC_ALL_VALUES
#ifndef _METAC_ALL_VALUES_
#define _METAC_ALL_VALUES_
#define METAC_HWBASE_VALUES   /* we want to access a H/W register block  */
#define METAC_PRIV_VALUES     /* we want to understand H/W privileges    */
#define METAC_CATCH_VALUES    /* we want to understand CATCH registers   */
#define METAC_AMA_VALUES      /* we want to understand AMA registers     */
#define METAC_TXUXX_VALUES    /* we want to understand UNIT/REG layout   */
#define METAC_SYSC_VALUES     /* we want to understand SYSC regs         */
#define METAC_MMCU_VALUES     /* we want to understand MMCU tables       */
#define METAC_PHYSMEM_VALUES  /* we want to understand PHYSICAL memory   */
#define METAC_CACHE_VALUES    /* we want to understand CACHE layout      */
#define METAC_PERF_VALUES     /* we want to understand performance cntrs */
#define METAC_EXPAND_VALUES   /* we want to understand EXPAND registers  */
#define METAC_CUSTOM_VALUES   /* we want to understand CUSTOM registers  */
#define METAC_TRACE_VALUES    /* we want to understand TRACE registers   */
#define METAC_CLKCTRL_VALUES  /* we want to understand CLKCTRL values    */
#define METAC_WRCOMB_VALUES   /* we want to understand WRCOMB values     */
#define METAC_ARBITER_VALUES  /* we want to understand ARBITER values    */
#define METAC_L2CACHE_VALUES  /* we want to understand L2CACHE values    */
#endif
#endif

/*
 * CHIP Core Register Map
 * ----------------------
 */
#ifdef METAC_HWBASE_VALUES
#ifndef _METAC_HWBASE_VALUES_
#define _METAC_HWBASE_VALUES_
#define CORE_HWBASE     0x04800000
#define PRIV_HWBASE     0x04810000
#define TRIG_HWBASE     0x04820000
#define SYSC_HWBASE     0x04830000
#endif
#endif

/*
 * Fast interrupt new bits
 * ------------------------------------
 */
#ifdef METAC_FASTINT_VALUES
#ifndef _METAC_FASTINT_VALUES_
#define _METAC_FASTINT_VALUES_
#define TXSTATUS_IPTOGGLE_BIT           0x80000000 /* Prev PToggle of TXPRIVEXT */
#define TXSTATUS_ISTATE_BIT             0x40000000 /* IState bit */
#define TXSTATUS_IWAIT_BIT              0x20000000 /* wait indefinitely in decision step*/
#define TXSTATUS_IEXCEPT_BIT            0x10000000 /* Indicate an exception occured */
#define TXSTATUS_IRPCOUNT_BITS          0x0E000000 /* Number of 'dirty' date entries*/
#define TXSTATUS_IRPCOUNT_S             25
#define TXSTATUS_IRQSTAT_BITS           0x0000F000 /* IRQEnc bits, trigger or interrupts */
#define TXSTATUS_IRQSTAT_S              12
#define TXSTATUS_LNKSETOK_BIT           0x00000020 /* LNKSetOK bit, successful LNKSET */

/* New fields in TXDE for fast interrupt system */
#define TXDIVTIME_IACTIVE_BIT           0x00008000 /* Enable new interrupt system */
#define TXDIVTIME_INONEST_BIT           0x00004000 /* Gate nested interrupt */
#define TXDIVTIME_IREGIDXGATE_BIT       0x00002000 /* gate of the IRegIdex field */
#define TXDIVTIME_IREGIDX_BITS          0x00001E00 /* Index of A0.0/1 replaces */
#define TXDIVTIME_IREGIDX_S             9
#define TXDIVTIME_NOST_BIT              0x00000100 /* disable superthreading bit */

#endif
#endif

/*
 * CHIP PRIV CONTROLS
 * ------------------
 */
#ifdef METAC_PRIV_VALUES
#ifndef _METAC_PRIV_VALUES_
#define _METAC_PRIV_VALUES_

/* The TXPIOREG register holds a bit mask directly mappable to
   corresponding addresses in the range 0x04800000 to 049FFFFF */
#define     TXPIOREG_ADDR_BITS  0x1F0000 /* Up to 32x64K bytes */
#define     TXPIOREG_ADDR_S     16

/* Hence based on the _HWBASE values ... */
#define     TXPIOREG_CORE_BIT       (1<<((0x04800000>>16)&0x1F))
#define     TXPIOREG_PRIV_BIT       (1<<((0x04810000>>16)&0x1F))
#define     TXPIOREG_TRIG_BIT       (1<<((0x04820000>>16)&0x1F))
#define     TXPIOREG_SYSC_BIT       (1<<((0x04830000>>16)&0x1F))

#define     TXPIOREG_WRC_BIT            0x00080000  /* Wr combiner reg priv */
#define     TXPIOREG_LOCALBUS_RW_BIT    0x00040000  /* Local bus rd/wr priv */
#define     TXPIOREG_SYSREGBUS_RD_BIT   0x00020000  /* Sys reg bus write priv */
#define     TXPIOREG_SYSREGBUS_WR_BIT   0x00010000  /* Sys reg bus read priv */

/* CORE region privilege controls */
#define T0PRIVCORE 0x04800828
#define         TXPRIVCORE_TXBKICK_BIT   0x001  /* Background kick priv */
#define         TXPRIVCORE_TXIKICK_BIT   0x002  /* Interrupt kick priv  */
#define         TXPRIVCORE_TXAMAREGX_BIT 0x004  /* TXAMAREG4|5|6 priv   */
#define TnPRIVCORE_STRIDE 0x00001000

#define T0PRIVSYSR 0x04810000
#define     TnPRIVSYSR_STRIDE   0x00000008
#define     TnPRIVSYSR_STRIDE_S 3
#define     TXPRIVSYSR_ALL_BITS       0x1F
#define     TXPRIVSYSR_CFLUSH_BIT     0x01
#define     TXPRIVSYSR_MTABLE_BIT     0x02
#define     TXPRIVSYSR_DIRECT_BIT     0x04
#define     TXPRIVSYSR_CORE_BIT       0x08
#define     TXPRIVSYSR_CORECODE_BIT   0x10
#define T1PRIVSYSR 0x04810008
#define T2PRIVSYSR 0x04810010
#define T3PRIVSYSR 0x04810018
#endif
#endif

/*
 * Detailed catch state information
 * --------------------------------
 */
#ifdef METAC_CATCH_VALUES
#ifndef _METAC_CATCH_VALUES_
#define _METAC_CATCH_VALUES_

/* Contents of TXCATCH0 register */
#define     TXCATCH0_LDRXX_BITS    0xF8000000  /* Load destination reg 0-31 */
#define     TXCATCH0_LDRXX_S       27
#define     TXCATCH0_LDDST_BITS    0x07FF0000  /* Load destination bits */
#define     TXCATCH0_LDDST_S       16
#define         TXCATCH0_LDDST_D1DSP  0x400   /* One bit set if it's a LOAD */
#define         TXCATCH0_LDDST_D0DSP  0x200
#define         TXCATCH0_LDDST_TMPLT  0x100
#define         TXCATCH0_LDDST_TR     0x080
#define         TXCATCH0_LDDST_FPU    0x040
#define         TXCATCH0_LDDST_PC     0x020
#define         TXCATCH0_LDDST_A1     0x010
#define         TXCATCH0_LDDST_A0     0x008
#define         TXCATCH0_LDDST_D1     0x004
#define         TXCATCH0_LDDST_D0     0x002
#define         TXCATCH0_LDDST_CT     0x001
#define     TXCATCH0_WATCHSTOP_BIT 0x00004000  /* Set if Data Watch set fault */
#define     TXCATCH0_WATCHS_BIT    0x00004000  /* Set if Data Watch set fault */
#define     TXCATCH0_WATCH1_BIT    0x00002000  /* Set if Data Watch 1 matches */
#define     TXCATCH0_WATCH0_BIT    0x00001000  /* Set if Data Watch 0 matches */
#define     TXCATCH0_FAULT_BITS    0x00000C00  /* See TXSTATUS_MEMFAULT_*     */
#define     TXCATCH0_FAULT_S       10
#define     TXCATCH0_PRIV_BIT      0x00000200  /* Privilege of transaction   */
#define     TXCATCH0_READ_BIT      0x00000100  /* Set for Read or Load cases  */

/* LNKGET Marker bit in TXCATCH0 */
#define	  TXCATCH0_LNKGET_MARKER_BIT 0x00000008
#define 	  TXCATCH0_PREPROC_BIT	0x00000004

/* Loads are indicated by one of the LDDST bits being set */
#define     TXCATCH0_LDM16_BIT   0x00000004  /* Load M16 flag */
#define     TXCATCH0_LDL2L1_BITS 0x00000003  /* Load data size L2,L1 */
#define     TXCATCH0_LDL2L1_S    0

/* Reads are indicated by the READ bit being set without LDDST bits */
#define     TXCATCH0_RAXX_BITS   0x0000001F  /* RAXX issue port for read */
#define     TXCATCH0_RAXX_S      0

/* Write operations are all that remain if READ bit is not set */
#define     TXCATCH0_WMASK_BITS  0x000000FF  /* Write byte lane mask */
#define     TXCATCH0_WMASK_S     0

/* When a FPU exception is signalled then FPUSPEC == FPUSPEC_TAG */
#define     TXCATCH0_FPURDREG_BITS    0xF8000000
#define     TXCATCH0_FPURDREG_S       27
#define     TXCATCH0_FPUR1REG_BITS    0x07C00000
#define     TXCATCH0_FPUR1REG_S       22
#define     TXCATCH0_FPUSPEC_BITS     0x000F0000
#define     TXCATCH0_FPUSPEC_S        16
#define         TXCATCH0_FPUSPEC_TAG      0xF
#define     TXCATCH0_FPUINSTA_BIT     0x00001000
#define     TXCATCH0_FPUINSTQ_BIT     0x00000800
#define     TXCATCH0_FPUINSTZ_BIT     0x00000400
#define     TXCATCH0_FPUINSTN_BIT     0x00000200
#define     TXCATCH0_FPUINSTO3O_BIT   0x00000100
#define     TXCATCH0_FPUWIDTH_BITS    0x000000C0
#define     TXCATCH0_FPUWIDTH_S       6
#define         TXCATCH0_FPUWIDTH_FLOAT   0
#define         TXCATCH0_FPUWIDTH_DOUBLE  1
#define         TXCATCH0_FPUWIDTH_PAIRED  2
#define     TXCATCH0_FPUOPENC_BITS    0x0000003F
#define     TXCATCH0_FPUOPENC_S       0
#define         TXCATCH0_FPUOPENC_ADD     0  /* rop1=Rs1, rop3=Rs2 */
#define         TXCATCH0_FPUOPENC_SUB     1  /* rop1=Rs1, rop3=Rs2 */
#define         TXCATCH0_FPUOPENC_MUL     2  /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_ATOI    3  /* rop3=Rs */
#define         TXCATCH0_FPUOPENC_ATOX    4  /* rop3=Rs, uses #Imm */
#define         TXCATCH0_FPUOPENC_ITOA    5  /* rop3=Rs */
#define         TXCATCH0_FPUOPENC_XTOA    6  /* rop3=Rs, uses #Imm */
#define         TXCATCH0_FPUOPENC_ATOH    7  /* rop2=Rs */
#define         TXCATCH0_FPUOPENC_HTOA    8  /* rop2=Rs */
#define         TXCATCH0_FPUOPENC_DTOF    9  /* rop3=Rs */
#define         TXCATCH0_FPUOPENC_FTOD    10 /* rop3=Rs */
#define         TXCATCH0_FPUOPENC_DTOL    11 /* rop3=Rs */
#define         TXCATCH0_FPUOPENC_LTOD    12 /* rop3=Rs */
#define         TXCATCH0_FPUOPENC_DTOXL   13 /* rop3=Rs, uses #imm */
#define         TXCATCH0_FPUOPENC_XLTOD   14 /* rop3=Rs, uses #imm */
#define         TXCATCH0_FPUOPENC_CMP     15 /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_MIN     16 /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_MAX     17 /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_ADDRE   18 /* rop1=Rs1, rop3=Rs2 */
#define         TXCATCH0_FPUOPENC_SUBRE   19 /* rop1=Rs1, rop3=Rs2 */
#define         TXCATCH0_FPUOPENC_MULRE   20 /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_MXA     21 /* rop1=Rs1, rop2=Rs2, rop3=Rs3*/
#define         TXCATCH0_FPUOPENC_MXAS    22 /* rop1=Rs1, rop2=Rs2, rop3=Rs3*/
#define         TXCATCH0_FPUOPENC_MAR     23 /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_MARS    24 /* rop1=Rs1, rop2=Rs2 */
#define         TXCATCH0_FPUOPENC_MUZ     25 /* rop1=Rs1, rop2=Rs2, rop3=Rs3*/
#define         TXCATCH0_FPUOPENC_MUZS    26 /* rop1=Rs1, rop2=Rs2, rop3=Rs3*/
#define         TXCATCH0_FPUOPENC_RCP     27 /* rop2=Rs */
#define         TXCATCH0_FPUOPENC_RSQ     28 /* rop2=Rs */

/* For floating point exceptions TXCATCH1 is used to carry extra data */ 
#define     TXCATCH1_FPUR2REG_BITS    0xF8000000
#define     TXCATCH1_FPUR2REG_S       27
#define     TXCATCH1_FPUR3REG_BITS    0x07C00000  /* Undefined if O3O set */
#define     TXCATCH1_FPUR3REG_S       22
#define     TXCATCH1_FPUIMM16_BITS    0x0000FFFF
#define     TXCATCH1_FPUIMM16_S       0

/*
 * If TXDIVTIME_RPVALID_BIT is set the read pipeline contained significant
 * information when the thread was interrupted|halted|exceptioned. Each slot
 * containing data is indicated by a one bit in the corresponding
 * TXDIVTIME_RPMASK_BITS bit (least significance bit relates to first
 * location in read pipeline - most likely to have the 1 state). Empty slots
 * contain zeroes with no interlock applied on reads if RPDIRTY is currently
 * set with RPMASK itself being read-only state.
 */
#define     TXDIVTIME_RPMASK_BITS 0x003F0000   /* -> Full (1) Empty (0) */
#define     TXDIVTIME_RPMASK_S    16
#endif
#endif

/*
 * CHIP Automatic Mips Allocation control registers
 * ------------------------------------------------
 */
#ifdef METAC_AMA_VALUES
#ifndef _METAC_AMA_VALUES_
#define _METAC_AMA_VALUES_
/* CT Bank AMA Registers */
#define TXAMAREG0_REGNUM 24
#define     TXAMAREG0_RCOFF_BIT       0x08000000
#define     TXAMAREG0_DLINEHLT_BIT    0x04000000
#define     TXAMAREG0_DLINEDIS_BIT    0x02000000
#define     TXAMAREG0_CYCSTRICT_BIT   0x01000000
#define     TXAMAREG0_CTRL_BITS       (TXAMAREG0_RCOFF_BIT |    \
                                       TXAMAREG0_DLINEHLT_BIT | \
                                       TXAMAREG0_DLINEDIS_BIT | \
                                       TXAMAREG0_CYCSTRICT_BIT)
#define     TXAMAREG0_CTRL_S           24
#define     TXAMAREG0_MDM_BIT         0x00400000
#define     TXAMAREG0_MPF_BIT         0x00200000
#define     TXAMAREG0_MPE_BIT         0x00100000
#define     TXAMAREG0_MASK_BITS       (TXAMAREG0_MDM_BIT | \
                                       TXAMAREG0_MPF_BIT | \
                                       TXAMAREG0_MPE_BIT)
#define     TXAMAREG0_MASK_S          20
#define     TXAMAREG0_SDM_BIT         0x00040000
#define     TXAMAREG0_SPF_BIT         0x00020000
#define     TXAMAREG0_SPE_BIT         0x00010000
#define     TXAMAREG0_STATUS_BITS     (TXAMAREG0_SDM_BIT | \
									   TXAMAREG0_SPF_BIT | \
									   TXAMAREG0_SPE_BIT)
#define     TXAMAREG0_STATUS_S        16
#define     TXAMAREG0_PRIORITY_BITS   0x0000FF00
#define     TXAMAREG0_PRIORITY_S      8
#define     TXAMAREG0_BVALUE_BITS     0x000000FF
#define     TXAMAREG0_BVALUE_S  0

#define TXAMAREG1_REGNUM 25
#define     TXAMAREG1_DELAYC_BITS     0x07FFFFFF
#define     TXAMAREG1_DELAYC_S  0

#define TXAMAREG2_REGNUM 26
#define     TXAMAREG2_IRQPRIORITY_BIT 0xFF000000
#define     TXAMAREG2_IRQPRIORITY_S   24
#define     TXAMAREG2_DLINEC_BITS     0x00FFFFF0
#define     TXAMAREG2_DLINEC_S        4

#define TXAMAREG3_REGNUM 27
#define     TXAMAREG2_AMABLOCK_BIT    0x00080000
#define     TXAMAREG2_AMAC_BITS       0x0000FFFF
#define     TXAMAREG2_AMAC_S          0

/* CORE memory mapped AMA registers */
#define T0AMAREG4   0x04800810
#define     TXAMAREG4_POOLSIZE_BITS 0x3FFFFF00
#define     TXAMAREG4_POOLSIZE_S    8
#define     TXAMAREG4_AVALUE_BITS   0x000000FF
#define     TXAMAREG4_AVALUE_S  0
#define T0AMAREG5   0x04800818
#define     TXAMAREG5_POOLC_BITS    0x07FFFFFF
#define         TXAMAREG5_POOLC_S       0
#define T0AMAREG6   0x04800820
#define     TXAMAREG6_DLINEDEF_BITS 0x00FFFFF0
#define         TXAMAREG6_DLINEDEF_S    0
#define TnAMAREGX_STRIDE    0x00001000
#endif
#endif

/*
 * CHIP Unit Identifiers and Valid/Global register number masks
 * ------------------------------------------------------------
 */
#ifdef METAC_TXUXX_VALUES
#ifndef _METAC_TXUXX_VALUES_
#define _METAC_TXUXX_VALUES_
#define TXUCT_ID    0x0     /* Control unit regs */
#define     TXUCT_MASK  0xFF1FFFFF  /* Valid regs 0..31  */
#define     TGUCT_MASK  0x00000000  /* No global regs    */
#define TXUD0_ID    0x1     /* Data unit regs */
#define TXUD1_ID    0x2
#define     TXUDX_MASK  0xFFFFFFFF  /* Valid regs 0..31 */
#define     TGUDX_MASK  0xFFFF0000  /* Global regs for base inst */
#define     TXUDXDSP_MASK   0x0F0FFFFF  /* Valid DSP regs */
#define     TGUDXDSP_MASK   0x0E0E0000  /* Global DSP ACC regs */
#define TXUA0_ID    0x3     /* Address unit regs */
#define TXUA1_ID    0x4
#define     TXUAX_MASK  0x0000FFFF  /* Valid regs   0-15 */
#define     TGUAX_MASK  0x0000FF00  /* Global regs  8-15 */
#define TXUPC_ID    0x5     /* PC registers */
#define     TXUPC_MASK  0x00000003  /* Valid regs   0- 1 */
#define     TGUPC_MASK  0x00000000  /* No global regs    */
#define TXUPORT_ID  0x6     /* Ports are not registers */
#define TXUTR_ID    0x7
#define     TXUTR_MASK  0x0000005F  /* Valid regs   0-3,4,6 */
#define     TGUTR_MASK  0x00000000  /* No global regs    */
#define TXUTT_ID    0x8
#define     TXUTT_MASK  0x0000000F  /* Valid regs   0-3 */
#define     TGUTT_MASK  0x00000010  /* Global reg   4   */
#define TXUFP_ID    0x9     /* FPU regs */
#define     TXUFP_MASK  0x0000FFFF  /* Valid regs   0-15 */
#define     TGUFP_MASK  0x00000000  /* No global regs    */

#define TXUXX_MASKS { TXUCT_MASK, TXUDX_MASK, TXUDX_MASK, TXUAX_MASK, \
                      TXUAX_MASK, TXUPC_MASK,          0, TXUTR_MASK, \
                      TXUTT_MASK, TXUFP_MASK,          0,          0, \
                               0,          0,          0,          0  }
#define TGUXX_MASKS { TGUCT_MASK, TGUDX_MASK, TGUDX_MASK, TGUAX_MASK, \
                      TGUAX_MASK, TGUPC_MASK,          0, TGUTR_MASK, \
                      TGUTT_MASK, TGUFP_MASK,          0,          0, \
                               0,          0,          0,          0  }

#define TXUXXDSP_MASKS { 0, TXUDXDSP_MASK, TXUDXDSP_MASK, 0, 0, 0, 0, 0, \
                         0, 0, 0, 0, 0, 0, 0, 0                          }
#define TGUXXDSP_MASKS { 0, TGUDXDSP_MASK, TGUDXDSP_MASK, 0, 0, 0, 0, 0, \
                         0, 0, 0, 0, 0, 0, 0, 0                          }
#endif
#endif

#ifdef METAC_MMCU_VALUES
#ifndef _METAC_MMCU_VALUES_
#define _METAC_MMCU_VALUES_

/*
 * Memory Management Control Unit Table Entries
 * --------------------------------------------
 */
#define MMCU_ENTRY_S         4            /* -> Entry size                */
#define MMCU_ENTRY_ADDR_BITS 0xFFFFF000   /* Physical address             */
#define MMCU_ENTRY_ADDR_S    12           /* -> Page size                 */
#define MMCU_ENTRY_CWIN_BITS 0x000000C0   /* Caching 'window' selection   */
#define MMCU_ENTRY_CWIN_S    6
#define     MMCU_CWIN_UNCACHED  0 /* May not be memory etc.  */
#define     MMCU_CWIN_BURST     1 /* Cached but LRU unset */
#define     MMCU_CWIN_C1SET     2 /* Cached in 1 set only */
#define     MMCU_CWIN_CACHED    3 /* Fully cached            */
#define MMCU_ENTRY_CACHE_BIT 0x00000080   /* Set for cached region         */
#define     MMCU_ECACHE1_FULL_BIT  0x00000040 /* Use all the sets */
#define     MMCU_ECACHE0_BURST_BIT 0x00000040 /* Match bursts     */
#define MMCU_ENTRY_SYS_BIT   0x00000010   /* Sys-coherent access required  */
#define MMCU_ENTRY_WRC_BIT   0x00000008   /* Write combining allowed       */
#define MMCU_ENTRY_PRIV_BIT  0x00000004   /* Privilege required           */
#define MMCU_ENTRY_WR_BIT    0x00000002   /* Writes allowed                */
#define MMCU_ENTRY_VAL_BIT   0x00000001   /* Entry is valid                */

/*
 * Extended first-level/top table entries have extra/larger fields in later
 * cores as bits 11:0 previously had no effect in such table entries.
 */
#define MMCU_E1ENT_ADDR_BITS 0xFFFFFFC0   /* Physical address             */
#define MMCU_E1ENT_ADDR_S    6            /*   -> resolution < page size  */
#define MMCU_E1ENT_PGSZ_BITS 0x0000001E   /* Page size for 2nd level      */
#define MMCU_E1ENT_PGSZ_S    1
#define     MMCU_E1ENT_PGSZ0_POWER   12   /* PgSz  0 -> 4K */
#define     MMCU_E1ENT_PGSZ_MAX      10   /* PgSz 10 -> 4M maximum */
#define MMCU_E1ENT_MINIM_BIT 0x00000020

/* MMCU control register in SYSC region */
#define MMCU_TABLE_PHYS_ADDR        0x04830010
#define     MMCU_TABLE_PHYS_ADDR_BITS   0xFFFFFFFC
#define     MMCU_TABLE_PHYS_EXTEND      0x00000001     /* See below */
#define MMCU_DCACHE_CTRL_ADDR       0x04830018
#define     MMCU_xCACHE_CTRL_ENABLE_BIT     0x00000001
#define     MMCU_xCACHE_CTRL_PARTITION_BIT  0x00000000 /* See xCPART below */
#define MMCU_ICACHE_CTRL_ADDR       0x04830020

/*
 * Allow direct access to physical memory used to implement MMU table.
 *
 * Each is based on a corresponding MMCU_TnLOCAL_TABLE_PHYSn or similar
 *    MMCU_TnGLOBAL_TABLE_PHYSn register pair (see next).
 */
#define LINSYSMEMT0L_BASE   0x05000000
#define LINSYSMEMT0L_LIMIT  0x051FFFFF
#define     LINSYSMEMTnX_STRIDE     0x00200000  /*  2MB Local per thread */
#define     LINSYSMEMTnX_STRIDE_S   21
#define     LINSYSMEMTXG_OFFSET     0x00800000  /* +2MB Global per thread */
#define     LINSYSMEMTXG_OFFSET_S   23
#define LINSYSMEMT1L_BASE   0x05200000
#define LINSYSMEMT1L_LIMIT  0x053FFFFF
#define LINSYSMEMT2L_BASE   0x05400000
#define LINSYSMEMT2L_LIMIT  0x055FFFFF
#define LINSYSMEMT3L_BASE   0x05600000
#define LINSYSMEMT3L_LIMIT  0x057FFFFF
#define LINSYSMEMT0G_BASE   0x05800000
#define LINSYSMEMT0G_LIMIT  0x059FFFFF
#define LINSYSMEMT1G_BASE   0x05A00000
#define LINSYSMEMT1G_LIMIT  0x05BFFFFF
#define LINSYSMEMT2G_BASE   0x05C00000
#define LINSYSMEMT2G_LIMIT  0x05DFFFFF
#define LINSYSMEMT3G_BASE   0x05E00000
#define LINSYSMEMT3G_LIMIT  0x05FFFFFF

/*
 * Extended MMU table functionality allows a sparse or flat table to be
 * described much more efficiently than before.
 */
#define MMCU_T0LOCAL_TABLE_PHYS0    0x04830700
#define   MMCU_TnX_TABLE_PHYSX_STRIDE    0x20   /* Offset per thread */
#define   MMCU_TnX_TABLE_PHYSX_STRIDE_S  5
#define   MMCU_TXG_TABLE_PHYSX_OFFSET    0x10   /* Global versus local */
#define   MMCU_TXG_TABLE_PHYSX_OFFSET_S  4
#define     MMCU_TBLPHYS0_DCCTRL_BITS       0x000000DF  /* DC controls  */
#define     MMCU_TBLPHYS0_ENTLB_BIT         0x00000020  /* Cache in TLB */
#define     MMCU_TBLPHYS0_TBLSZ_BITS        0x00000F00  /* Area supported */
#define     MMCU_TBLPHYS0_TBLSZ_S           8
#define         MMCU_TBLPHYS0_TBLSZ0_POWER      22  /* 0 -> 4M */
#define         MMCU_TBLPHYS0_TBLSZ_MAX         9   /* 9 -> 2G */
#define     MMCU_TBLPHYS0_LINBASE_BITS      0xFFC00000  /* Linear base */
#define     MMCU_TBLPHYS0_LINBASE_S         22

#define MMCU_T0LOCAL_TABLE_PHYS1    0x04830708
#define     MMCU_TBLPHYS1_ADDR_BITS         0xFFFFFFFC  /* Physical base */
#define     MMCU_TBLPHYS1_ADDR_S            2

#define MMCU_T0GLOBAL_TABLE_PHYS0   0x04830710
#define MMCU_T0GLOBAL_TABLE_PHYS1   0x04830718
#define MMCU_T1LOCAL_TABLE_PHYS0    0x04830720
#define MMCU_T1LOCAL_TABLE_PHYS1    0x04830728
#define MMCU_T1GLOBAL_TABLE_PHYS0   0x04830730
#define MMCU_T1GLOBAL_TABLE_PHYS1   0x04830738
#define MMCU_T2LOCAL_TABLE_PHYS0    0x04830740
#define MMCU_T2LOCAL_TABLE_PHYS1    0x04830748
#define MMCU_T2GLOBAL_TABLE_PHYS0   0x04830750
#define MMCU_T2GLOBAL_TABLE_PHYS1   0x04830758
#define MMCU_T3LOCAL_TABLE_PHYS0    0x04830760
#define MMCU_T3LOCAL_TABLE_PHYS1    0x04830768
#define MMCU_T3GLOBAL_TABLE_PHYS0   0x04830770
#define MMCU_T3GLOBAL_TABLE_PHYS1   0x04830778

#define MMCU_T0EBWCCTRL             0x04830640
#define     MMCU_TnEBWCCTRL_BITS    0x00000007
#define     MMCU_TnEBWCCTRL_S       0
#define         MMCU_TnEBWCCCTRL_DISABLE_ALL 0
#define         MMCU_TnEBWCCCTRL_ABIT25      1
#define         MMCU_TnEBWCCCTRL_ABIT26      2
#define         MMCU_TnEBWCCCTRL_ABIT27      3
#define         MMCU_TnEBWCCCTRL_ABIT28      4
#define         MMCU_TnEBWCCCTRL_ABIT29      5
#define         MMCU_TnEBWCCCTRL_ABIT30      6
#define         MMCU_TnEBWCCCTRL_ENABLE_ALL  7
#define MMCU_TnEBWCCTRL_STRIDE      8

/*
 * CACHEWD/CACHEWL instructions use the bottom 8 bits of the data presented to
 * control the operation actually achieved.
 */
/* Use of these two bits should be discouraged since the bits dont have
 * consistent meanings
 */
#define CACHEW_ICACHE_BIT           0x01
#define CACHEW_TLBFLUSH_BIT         0x02

#define CACHEW_FLUSH_L1D_L2         0x0
#define CACHEW_INVALIDATE_L1I       0x1
#define CACHEW_INVALIDATE_L1DTLB    0x2
#define CACHEW_INVALIDATE_L1ITLB    0x3
#define CACHEW_WRITEBACK_L1D_L2     0x4
#define CACHEW_INVALIDATE_L1D       0x8
#define CACHEW_INVALIDATE_L1D_L2    0xC

/*
 * CACHERD/CACHERL instructions use bits 3:5 of the address presented to
 * control the operation achieved and hence the specific result.
 */
#define CACHER_ADDR_BITS            0xFFFFFFC0
#define CACHER_OPER_BITS            0x00000030
#define CACHER_OPER_S               4
#define     CACHER_OPER_LINPHY          0
#define CACHER_ICACHE_BIT           0x00000008   
#define CACHER_ICACHE_S             3

/*
 * CACHERD/CACHERL LINPHY Oper result is one/two 32-bit words
 *
 *  If CRLINPHY0_VAL_BIT (Bit 0) set then,
 *      Lower 32-bits corresponds to MMCU_ENTRY_* above.
 *      Upper 32-bits corresponds to CRLINPHY1_* values below (if requested).
 *  else
 *      Lower 32-bits corresponds to CRLINPHY0_* values below.
 *      Upper 32-bits undefined.
 */
#define CRLINPHY0_VAL_BIT      0x00000001
#define CRLINPHY0_FIRST_BIT    0x00000004 /* Set if VAL=0 due to first level */

#define CRLINPHY1_READ_BIT     0x00000001 /* Set if reads permitted          */
#define CRLINPHY1_SINGLE_BIT   0x00000004 /* Set if TLB does not cache entry */
#define CRLINPHY1_PAGEMSK_BITS 0x0000FFF0 /* Set to ((2^n-1)>>12) value      */
#define CRLINPHY1_PAGEMSK_S    4
#endif
#endif

#ifdef METAC_SYSC_VALUES
#ifndef _METAC_SYSC_VALUES_
#define _METAC_SYSC_VALUES_

/* Registers within the SYSC register region */
#define METAC_ID                0x04830000
#define     METAC_ID_MAJOR_BITS     0xFF000000
#define     METAC_ID_MAJOR_S        24
#define     METAC_ID_MINOR_BITS     0x00FF0000
#define     METAC_ID_MINOR_S        16
#define     METAC_ID_REV_BITS       0x0000FF00
#define     METAC_ID_REV_S          8
#define     METAC_ID_MAINT_BITS     0x000000FF
#define     METAC_ID_MAINT_S        0

/* Old name for MAINT field - deprecated */
#define     METAC_ID_CHIPID_BITS    0x000000FF
#define     METAC_ID_CHIPID_S       0

#ifdef METAC_ID2_NEEDED
/* Use of this section is strongly deprecated */
#define METAC_ID2               0x04830008
#define     METAC_ID2_DESIGNER_BITS 0xFFFF0000  /* Modified by customer */
#define     METAC_ID2_DESIGNER_S    16
#define     METAC_ID2_MINOR2_BITS   0x00000F00  /* 3rd digit of prod rev */
#define     METAC_ID2_MINOR2_S      8
#define     METAC_ID2_CONFIG_BITS   0x000000FF  /* Wrapper configuration */
#define     METAC_ID2_CONFIG_S      0
#endif

/* Primary core identification and configuration information */
#define METAC_CORE_ID           0x04831000
#define     METAC_COREID_GROUP_BITS   0xFF000000
#define     METAC_COREID_GROUP_S      24
#define         METAC_COREID_GROUP_METAG  0x14
#define     METAC_COREID_ID_BITS      0x00FF0000
#define     METAC_COREID_ID_S         16
#define         METAC_COREID_ID_W32       0x10   /* >= for 32-bit pipeline */
#define     METAC_COREID_CONFIG_BITS  0x0000FFFF
#define     METAC_COREID_CONFIG_S     0
#define       METAC_COREID_CFGCACHE_BITS    0x0007
#define       METAC_COREID_CFGCACHE_S       0
#define           METAC_COREID_CFGCACHE_NOM       0
#define           METAC_COREID_CFGCACHE_TYPE0     1
#define           METAC_COREID_CFGCACHE_NOMMU     1 /* Alias for TYPE0 */
#define           METAC_COREID_CFGCACHE_NOCACHE   2
#define           METAC_COREID_CFGCACHE_PRIVNOMMU 3
#define       METAC_COREID_CFGDSP_BITS      0x0038
#define       METAC_COREID_CFGDSP_S         3
#define           METAC_COREID_CFGDSP_NOM       0
#define           METAC_COREID_CFGDSP_MIN       1
#define       METAC_COREID_NOFPACC_BIT      0x0040 /* Set if no FPU accum */
#define       METAC_COREID_CFGFPU_BITS      0x0180
#define       METAC_COREID_CFGFPU_S         7
#define           METAC_COREID_CFGFPU_NOM       0
#define           METAC_COREID_CFGFPU_SNGL      1
#define           METAC_COREID_CFGFPU_DBL       2
#define       METAC_COREID_NOAMA_BIT        0x0200 /* Set if no AMA present */
#define       METAC_COREID_NOCOH_BIT        0x0400 /* Set if no Gbl coherency */

/* Core revision information */
#define METAC_CORE_REV          0x04831008
#define     METAC_COREREV_DESIGN_BITS   0xFF000000
#define     METAC_COREREV_DESIGN_S      24
#define     METAC_COREREV_MAJOR_BITS    0x00FF0000
#define     METAC_COREREV_MAJOR_S       16
#define     METAC_COREREV_MINOR_BITS    0x0000FF00
#define     METAC_COREREV_MINOR_S       8
#define     METAC_COREREV_MAINT_BITS    0x000000FF
#define     METAC_COREREV_MAINT_S       0

/* Configuration information control outside the core */
#define METAC_CORE_DESIGNER1    0x04831010      /* Arbitrary value */
#define METAC_CORE_DESIGNER2    0x04831018      /* Arbitrary value */

/* Configuration information covering presence/number of various features */
#define METAC_CORE_CONFIG2      0x04831020
#define     METAC_CORECFG2_COREDBGTYPE_BITS 0x60000000   /* Core debug type */
#define     METAC_CORECFG2_COREDBGTYPE_S    29
#define     METAC_CORECFG2_DCSMALL_BIT      0x04000000   /* Data cache small */
#define     METAC_CORECFG2_ICSMALL_BIT      0x02000000   /* Inst cache small */
#define     METAC_CORECFG2_DCSZNP_BITS      0x01C00000   /* Data cache size np */
#define     METAC_CORECFG2_DCSZNP_S         22
#define     METAC_CORECFG2_ICSZNP_BITS      0x00380000  /* Inst cache size np */
#define     METAC_CORECFG2_ICSZNP_S         19
#define     METAC_CORECFG2_DCSZ_BITS        0x00070000   /* Data cache size */
#define     METAC_CORECFG2_DCSZ_S           16
#define         METAC_CORECFG2_xCSZ_4K          0        /* Allocated values */
#define         METAC_CORECFG2_xCSZ_8K          1
#define         METAC_CORECFG2_xCSZ_16K         2
#define         METAC_CORECFG2_xCSZ_32K         3
#define         METAC_CORECFG2_xCSZ_64K         4
#define     METAC_CORE_C2ICSZ_BITS          0x0000E000   /* Inst cache size */
#define     METAC_CORE_C2ICSZ_S             13
#define     METAC_CORE_GBLACC_BITS          0x00001800   /* Number of Global Acc */
#define     METAC_CORE_GBLACC_S             11
#define     METAC_CORE_GBLDXR_BITS          0x00000700   /* 0 -> 0, R -> 2^(R-1) */
#define     METAC_CORE_GBLDXR_S             8
#define     METAC_CORE_GBLAXR_BITS          0x000000E0   /* 0 -> 0, R -> 2^(R-1) */
#define     METAC_CORE_GBLAXR_S             5
#define     METAC_CORE_RTTRACE_BIT          0x00000010
#define     METAC_CORE_WATCHN_BITS          0x0000000C   /* 0 -> 0, N -> 2^N */
#define     METAC_CORE_WATCHN_S             2
#define     METAC_CORE_BREAKN_BITS          0x00000003   /* 0 -> 0, N -> 2^N */
#define     METAC_CORE_BREAKN_S             0

/* Configuration information covering presence/number of various features */
#define METAC_CORE_CONFIG3      0x04831028
#define     METAC_CORECFG3_L2C_REV_ID_BITS          0x000F0000   /* Revision of L2 cache */
#define     METAC_CORECFG3_L2C_REV_ID_S             16
#define     METAC_CORECFG3_L2C_LINE_SIZE_BITS       0x00003000   /* L2 line size */
#define     METAC_CORECFG3_L2C_LINE_SIZE_S          12
#define         METAC_CORECFG3_L2C_LINE_SIZE_64B    0x0          /* 64 bytes */
#define     METAC_CORECFG3_L2C_NUM_WAYS_BITS        0x00000F00   /* L2 number of ways (2^n) */
#define     METAC_CORECFG3_L2C_NUM_WAYS_S           8
#define     METAC_CORECFG3_L2C_SIZE_BITS            0x000000F0   /* L2 size (2^n) */
#define     METAC_CORECFG3_L2C_SIZE_S               4
#define     METAC_CORECFG3_L2C_UNIFIED_BIT          0x00000004   /* Unified cache: */
#define     METAC_CORECFG3_L2C_UNIFIED_S            2
#define       METAC_CORECFG3_L2C_UNIFIED_UNIFIED    1            /* - Unified D/I cache */
#define       METAC_CORECFG3_L2C_UNIFIED_SEPARATE   0            /* - Separate D/I cache */
#define     METAC_CORECFG3_L2C_MODE_BIT             0x00000002   /* Cache Mode: */
#define     METAC_CORECFG3_L2C_MODE_S               1
#define       METAC_CORECFG3_L2C_MODE_WRITE_BACK    1            /* - Write back */
#define       METAC_CORECFG3_L2C_MODE_WRITE_THROUGH 0            /* - Write through */
#define     METAC_CORECFG3_L2C_HAVE_L2C_BIT         0x00000001   /* Have L2C */
#define     METAC_CORECFG3_L2C_HAVE_L2C_S           0

#define SYSC_CACHE_MMU_CONFIG       0x04830028
#define     SYSC_CMMUCFG_DCSKEWABLE_BIT 0x00000040
#define     SYSC_CMMUCFG_ICSKEWABLE_BIT 0x00000020
#define     SYSC_CMMUCFG_DCSKEWOFF_BIT  0x00000010  /* Skew association override  */
#define     SYSC_CMMUCFG_ICSKEWOFF_BIT  0x00000008  /* -> default 0 on if present */
#define     SYSC_CMMUCFG_MODE_BITS      0x00000007  /* Access to old state */
#define     SYSC_CMMUCFG_MODE_S         0
#define         SYSC_CMMUCFG_ON             0x7
#define         SYSC_CMMUCFG_EBYPASS        0x6   /* Enhanced by-pass mode */
#define         SYSC_CMMUCFG_EBYPASSIC      0x4   /* EB just inst cache */
#define         SYSC_CMMUCFG_EBYPASSDC      0x2   /* EB just data cache */
/* Old definitions, Keep them for now */
#define         SYSC_CMMUCFG_MMU_ON_BIT     0x1
#define         SYSC_CMMUCFG_DC_ON_BIT      0x2
#define         SYSC_CMMUCFG_IC_ON_BIT      0x4

#define SYSC_JTAG_THREAD            0x04830030
#define     SYSC_JTAG_TX_BITS           0x00000003 /* Read only bits! */
#define     SYSC_JTAG_TX_S              0
#define     SYSC_JTAG_PRIV_BIT          0x00000004
#define     SYSC_JTAG_SLAVETX_BITS      0x00000018
#define     SYSC_JTAG_SLAVETX_S         3
#define SYSC_DCACHE_FLUSH           0x04830038
#define SYSC_ICACHE_FLUSH           0x04830040
#define  SYSC_xCACHE_FLUSH_INIT     0x1
#define MMCU_DIRECTMAP0_ADDR        0x04830080 /* LINSYSDIRECT_BASE -> */
#define     MMCU_DIRECTMAPn_STRIDE      0x00000010 /* 4 Region settings */
#define     MMCU_DIRECTMAPn_S           4
#define         MMCU_DIRECTMAPn_ADDR_BITS       0xFF800000
#define         MMCU_DIRECTMAPn_ADDR_S          23
#define         MMCU_DIRECTMAPn_ADDR_SCALE      0x00800000 /* 8M Regions */
/*
 * These fields in the above registers provide MMCU_ENTRY_* values
 *   for each direct mapped region to enable optimisation of these areas.
 *       (LSB similar to VALID must be set for enhancments to be active)
 */
#define         MMCU_DIRECTMAPn_ENHANCE_BIT     0x00000001 /* 0 = no optim */
#define         MMCU_DIRECTMAPn_DCCTRL_BITS     0x000000DF /* Get DC Ctrl */
#define         MMCU_DIRECTMAPn_DCCTRL_S        0
#define         MMCU_DIRECTMAPn_ICCTRL_BITS     0x0000C000 /* Get IC Ctrl */
#define         MMCU_DIRECTMAPn_ICCTRL_S        8 
#define         MMCU_DIRECTMAPn_ENTLB_BIT       0x00000020 /* Cache in TLB */
#define         MMCU_DIRECTMAPn_ICCWIN_BITS     0x0000C000 /* Get IC Win Bits */
#define         MMCU_DIRECTMAPn_ICCWIN_S        14
#define MMCU_DIRECTMAP1_ADDR        0x04830090
#define MMCU_DIRECTMAP2_ADDR        0x048300a0
#define MMCU_DIRECTMAP3_ADDR        0x048300b0

/*
 * These bits partion each threads use of data cache or instruction cache
 * resource by modifying the top 4 bits of the address within the cache
 * storage area.
 */
#define SYSC_DCPART0 0x04830200
#define     SYSC_xCPARTn_STRIDE   0x00000008
#define     SYSC_xCPARTL_AND_BITS 0x0000000F /* Masks top 4 bits */
#define     SYSC_xCPARTL_AND_S    0
#define     SYSC_xCPARTG_AND_BITS 0x00000F00 /* Masks top 4 bits */
#define     SYSC_xCPARTG_AND_S    8
#define     SYSC_xCPARTL_OR_BITS  0x000F0000 /* Ors into top 4 bits */
#define     SYSC_xCPARTL_OR_S     16
#define     SYSC_xCPARTG_OR_BITS  0x0F000000 /* Ors into top 4 bits */
#define     SYSC_xCPARTG_OR_S     24
#define     SYSC_CWRMODE_BIT      0x80000000 /* Write cache mode bit */

#define SYSC_DCPART1 0x04830208
#define SYSC_DCPART2 0x04830210
#define SYSC_DCPART3 0x04830218
#define SYSC_ICPART0 0x04830220
#define SYSC_ICPART1 0x04830228
#define SYSC_ICPART2 0x04830230
#define SYSC_ICPART3 0x04830238

/*
 * META Core Memory and Cache Update registers
 */
#define SYSC_MCMDATAX  0x04830300   /* 32-bit read/write data register */
#define SYSC_MCMDATAT  0x04830308   /* Read or write data triggers oper */
#define SYSC_MCMGCTRL  0x04830310   /* Control register */
#define     SYSC_MCMGCTRL_READ_BIT  0x00000001 /* Set to issue 1st read */
#define     SYSC_MCMGCTRL_AINC_BIT  0x00000002 /* Set for auto-increment */
#define     SYSC_MCMGCTRL_ADDR_BITS 0x000FFFFC /* Address or index */
#define     SYSC_MCMGCTRL_ADDR_S    2
#define     SYSC_MCMGCTRL_ID_BITS   0x0FF00000 /* Internal memory block Id */
#define     SYSC_MCMGCTRL_ID_S      20
#define         SYSC_MCMGID_NODEV       0xFF /* No Device Selected */
#define         SYSC_MCMGID_DSPRAM0A    0x04 /* DSP RAM D0 block A access */
#define         SYSC_MCMGID_DSPRAM0B    0x05 /* DSP RAM D0 block B access */
#define         SYSC_MCMGID_DSPRAM1A    0x06 /* DSP RAM D1 block A access */
#define         SYSC_MCMGID_DSPRAM1B    0x07 /* DSP RAM D1 block B access */
#define         SYSC_MCMGID_DCACHEL     0x08 /* DCACHE lines (64-bytes/line) */
#define         SYSC_MCMGID_DCACHETLB   0x09 /* DCACHE TLB ( Read Only )     */
#define         SYSC_MCMGID_DCACHET     0x0A /* DCACHE tags (32-bits/line)   */
#define         SYSC_MCMGID_DCACHELRU   0x0B /* DCACHE LRU (8-bits/line)     */
#define         SYSC_MCMGID_ICACHEL     0x0C /* ICACHE lines (64-bytes/line  */
#define         SYSC_MCMGID_ICACHETLB   0x0D /* ICACHE TLB (Read Only )     */
#define         SYSC_MCMGID_ICACHET     0x0E /* ICACHE Tags (32-bits/line)   */
#define         SYSC_MCMGID_ICACHELRU   0x0F /* ICACHE LRU (8-bits/line )    */
#define         SYSC_MCMGID_COREIRAM0   0x10 /* Core code mem id 0 */
#define         SYSC_MCMGID_COREIRAMn   0x17
#define         SYSC_MCMGID_COREDRAM0   0x18 /* Core data mem id 0 */
#define         SYSC_MCMGID_COREDRAMn   0x1F
#define         SYSC_MCMGID_DCACHEST    0x20 /* DCACHE ST ( Read Only )      */
#define         SYSC_MCMGID_ICACHEST    0x21 /* ICACHE ST ( Read Only )      */
#define         SYSC_MCMGID_DCACHETLBLRU 0x22 /* DCACHE TLB LRU ( Read Only )*/
#define         SYSC_MCMGID_ICACHETLBLRU 0x23 /* ICACHE TLB LRU( Read Only ) */
#define         SYSC_MCMGID_DCACHESTLRU 0x24 /* DCACHE ST LRU ( Read Only )  */
#define         SYSC_MCMGID_ICACHESTLRU 0x25 /* ICACHE ST LRU ( Read Only )  */
#define         SYSC_MCMGID_DEBUGTLB    0x26 /* DEBUG TLB ( Read Only )      */
#define         SYSC_MCMGID_DEBUGST     0x27 /* DEBUG ST ( Read Only )       */
#define         SYSC_MCMGID_L2CACHEL    0x30 /* L2 Cache Lines (64-bytes/line) */
#define         SYSC_MCMGID_L2CACHET    0x31 /* L2 Cache Tags (32-bits/line) */
#define         SYSC_MCMGID_COPROX0     0x70 /* Coprocessor port id 0 */
#define         SYSC_MCMGID_COPROXn     0x77

#define     SYSC_MCMGCTRL_TR31_BIT  0x80000000 /* Trigger 31 on completion */
#define SYSC_MCMSTATUS 0x04830318   /* Status read only */
#define     SYSC_MCMSTATUS_IDLE_BIT 0x00000001

/* META System Events */
#define SYSC_SYS_EVENT            0x04830400
#define     SYSC_SYSEVT_ATOMIC_BIT      0x00000001
#define     SYSC_SYSEVT_CACHEX_BIT      0x00000002
#define SYSC_ATOMIC_LOCK          0x04830408
#define     SYSC_ATOMIC_STATE_TX_BITS 0x0000000F
#define     SYSC_ATOMIC_STATE_TX_S    0
#define     SYSC_ATOMIC_SOURCE_BIT    0x00000010

#ifdef METAC_1_2
#define     SYSC_ATOMIC_STATE_DX_BITS 0x000000F0
#define     SYSC_ATOMIC_STATE_DX_S    4
#endif

/* These definitions replace the EXPAND_TIMER_DIV register defines which are to
 * be deprecated.
 */
#define SYSC_TIMER_DIV            0x04830140
#define     SYSC_TIMDIV_BITS      0x000000FF
#define     SYSC_TIMDIV_S         0

/* META Enhanced by-pass control for local and global region */
#define MMCU_LOCAL_EBCTRL   0x04830600
#define MMCU_GLOBAL_EBCTRL  0x04830608
#define     MMCU_EBCTRL_SINGLE_BIT      0x00000020 /* TLB Uncached */
/*
 * These fields in the above registers provide MMCU_ENTRY_* values
 *   for each direct mapped region to enable optimisation of these areas.
 */
#define     MMCU_EBCTRL_DCCTRL_BITS     0x000000C0 /* Get DC Ctrl */
#define     MMCU_EBCTRL_DCCTRL_S        0
#define     MMCU_EBCTRL_ICCTRL_BITS     0x0000C000 /* Get DC Ctrl */
#define     MMCU_EBCTRL_ICCTRL_S        8

/* META Cached Core Mode Registers */
#define MMCU_T0CCM_ICCTRL   0x04830680     /* Core cached code control */
#define     MMCU_TnCCM_xxCTRL_STRIDE    8
#define     MMCU_TnCCM_xxCTRL_STRIDE_S  3
#define MMCU_T1CCM_ICCTRL   0x04830688
#define MMCU_T2CCM_ICCTRL   0x04830690
#define MMCU_T3CCM_ICCTRL   0x04830698
#define MMCU_T0CCM_DCCTRL   0x048306C0     /* Core cached data control */
#define MMCU_T1CCM_DCCTRL   0x048306C8
#define MMCU_T2CCM_DCCTRL   0x048306D0
#define MMCU_T3CCM_DCCTRL   0x048306D8
#define     MMCU_TnCCM_ENABLE_BIT       0x00000001
#define     MMCU_TnCCM_WIN3_BIT         0x00000002
#define     MMCU_TnCCM_DCWRITE_BIT      0x00000004  /* In DCCTRL only */
#define     MMCU_TnCCM_REGSZ_BITS       0x00000F00
#define     MMCU_TnCCM_REGSZ_S          8
#define         MMCU_TnCCM_REGSZ0_POWER      12     /* RegSz 0 -> 4K */
#define         MMCU_TnCCM_REGSZ_MAXBYTES    0x00080000  /* 512K max */
#define     MMCU_TnCCM_ADDR_BITS        0xFFFFF000
#define     MMCU_TnCCM_ADDR_S           12
#endif
#endif

#ifdef METAC_CACHE_VALUES
#ifndef _METAC_CACHE_VALUES_
#define _METAC_CACHE_VALUES_

/* Data cache line size as bytes and shift */
#define DCACHE_LINE_BYTES 64
#define DCACHE_LINE_S     6

/* Number of ways in the data cache */
#define DCACHE_WAYS       4

/* Instruction cache line size as bytes and shift */
#define ICACHE_LINE_BYTES 64
#define ICACHE_LINE_S     6

/* Number of ways in the instruction cache */
#define ICACHE_WAYS       4
#endif
#endif


/*
 * Hardware performance counter registers
 * --------------------------------------
 */
#ifdef METAC_PERF_VALUES
#ifndef _METAC_PERF_VALUES_
#define _METAC_PERF_VALUES_

/* Two Performance Counter Internal Core Events Control registers */
#define PERF_ICORE0   0x0480FFD0
#define PERF_ICORE1   0x0480FFD8
#define     PERFI_CTRL_BITS    0x0000000F
#define     PERFI_CTRL_S       0
#define         PERFI_CAH_DMISS    0x0  /* Dcache Misses in cache (TLB Hit) */
#define         PERFI_CAH_IMISS    0x1  /* Icache Misses in cache (TLB Hit) */
#define         PERFI_TLB_DMISS    0x2  /* Dcache Misses in per-thread TLB */
#define         PERFI_TLB_IMISS    0x3  /* Icache Misses in per-thread TLB */
#define         PERFI_TLB_DWRHITS  0x4  /* DC Write-Hits in per-thread TLB */
#define         PERFI_TLB_DWRMISS  0x5  /* DC Write-Miss in per-thread TLB */
#define         PERFI_CAH_DLFETCH  0x8  /* DC Read cache line fetch */
#define         PERFI_CAH_ILFETCH  0x9  /* DC Read cache line fetch */
#define         PERFI_CAH_DWFETCH  0xA  /* DC Read cache word fetch */
#define         PERFI_CAH_IWFETCH  0xB  /* DC Read cache word fetch */

/* Two memory-mapped hardware performance counter registers */
#define PERF_COUNT0 0x0480FFE0
#define PERF_COUNT1 0x0480FFE8

/* Fields in PERF_COUNTn registers */
#define PERF_COUNT_BITS  0x00ffffff /* Event count value */

#define PERF_THREAD_BITS 0x0f000000 /* Thread mask selects threads */
#define PERF_THREAD_S    24

#define PERF_CTRL_BITS   0xf0000000 /* Event filter control */
#define PERF_CTRL_S      28

#define    PERFCTRL_SUPER   0  /* Superthread cycles */
#define    PERFCTRL_REWIND  1  /* Rewinds due to Dcache Misses */
#define    PERFCTRL_SUPREW  2  /* Rewinds of superthreaded cycles (no mask) */

#define    PERFCTRL_CYCLES  3  /* Counts all cycles (no mask) */

#define    PERFCTRL_PREDBC  4  /* Conditional branch predictions */
#define    PERFCTRL_MISPBC  5  /* Conditional branch mispredictions */
#define    PERFCTRL_PREDRT  6  /* Return predictions */
#define    PERFCTRL_MISPRT  7  /* Return mispredictions */

#define    PERFCTRL_DHITS   8  /* Dcache Hits */
#define    PERFCTRL_IHITS   9  /* Icache Hits */
#define    PERFCTRL_IMISS   10 /* Icache Misses in cache or TLB */
#define    PERFCTRL_DCSTALL 11 /* Dcache+TLB o/p delayed (per-thread) */
#define    PERFCTRL_ICSTALL 12 /* Icache+TLB o/p delayed (per-thread) */

#define    PERFCTRL_INT     13 /* Internal core delailed events (see next) */
#define    PERFCTRL_EXT     15 /* External source in core periphery */

/* These definitions replace the EXPAND_PERFCHANx register defines which are to
 * be deprecated.
 */
#define PERF_CHAN0 0x04830150
#define PERF_CHAN1 0x04830158
#define     PERF_CHAN_BITS    0x0000000F
#define     PERF_CHAN_S       0
#define         PERFCHAN_WRC_WRBURST   0x0   /* Write combiner write burst */
#define         PERFCHAN_WRC_WRITE     0x1   /* Write combiner write       */
#define         PERFCHAN_WRC_RDBURST   0x2   /* Write combiner read burst  */
#define         PERFCHAN_WRC_READ      0x3   /* Write combiner read        */
#define         PERFCHAN_PREARB_DELAY  0x4   /* Pre-arbiter delay cycle    */
                                             /* Cross-bar hold-off cycle:  */
#define         PERFCHAN_XBAR_HOLDWRAP 0x5   /*    wrapper register        */
#define         PERFCHAN_XBAR_HOLDSBUS 0x6   /*    system bus (ATP only)   */
#define         PERFCHAN_XBAR_HOLDCREG 0x9   /*    core registers          */
#define         PERFCHAN_L2C_MISS      0x6   /* L2 Cache miss              */
#define         PERFCHAN_L2C_HIT       0x7   /* L2 Cache hit               */
#define         PERFCHAN_L2C_WRITEBACK 0x8   /* L2 Cache writeback         */
                                             /* Admission delay cycle:     */ 
#define         PERFCHAN_INPUT_CREG    0xB   /*    core registers          */
#define         PERFCHAN_INPUT_INTR    0xC   /*    internal ram            */
#define         PERFCHAN_INPUT_WRC     0xD   /*    write combiners(memory) */

/* Should following be removed as not in TRM anywhere? */
#define         PERFCHAN_XBAR_HOLDINTR 0x8   /*    internal ram            */
#define         PERFCHAN_INPUT_SBUS    0xA   /*    register port           */
/* End of remove section. */

#define         PERFCHAN_MAINARB_DELAY 0xF   /* Main arbiter delay cycle   */

#endif
#endif

/*
 * Write combiner registers
 * ------------------------
 *
 * These replace the EXPAND_T0WRCOMBINE register defines, which will be
 * deprecated.
 */
#ifdef METAC_WRCOMB_VALUES
#ifndef _METAC_WRCOMB_VALUES
#define _METAC_WRCOMB_VALUES

#define WRCOMB_CONFIG0             0x04830100
#define     WRCOMB_LFFEn_BIT           0x00004000  /* Enable auto line full flush */
#define     WRCOMB_ENABLE_BIT          0x00002000  /* Enable write combiner */
#define     WRCOMB_TIMEOUT_ENABLE_BIT  0x00001000  /* Timeout flush enable */
#define     WRCOMB_TIMEOUT_COUNT_BITS  0x000003FF  
#define     WRCOMB_TIMEOUT_COUNT_S     0
#define WRCOMB_CONFIG4             0x04830180
#define     WRCOMB_PARTALLOC_BITS      0x000000C0
#define     WRCOMB_PARTALLOC_S         64
#define     WRCOMB_PARTSIZE_BITS       0x00000030
#define     WRCOMB_PARTSIZE_S          4
#define     WRCOMB_PARTOFFSET_BITS     0x0000000F
#define     WRCOMB_PARTOFFSET_S        0
#define WRCOMB_CONFIG_STRIDE       8

#endif /* _METAC_WRCOMB_VALUES */
#endif /* METAC_WRCOMB_VALUES */

/*
 * Thread arbiter registers
 * ------------------------
 *
 * These replace the EXPAND_T0ARBITER register defines, which will be
 * deprecated.
 */
#ifdef METAC_ARBITER_VALUES
#ifndef _METAC_ARBITER_VALUES
#define _METAC_ARBITER_VALUES

#define ARBITER_ARBCONFIG0       0x04830120
#define     ARBCFG_BPRIORITY_BIT     0x02000000
#define     ARBCFG_IPRIORITY_BIT     0x01000000
#define     ARBCFG_PAGE_BITS         0x00FF0000
#define     ARBCFG_PAGE_S            16
#define     ARBCFG_BBASE_BITS        0x0000FF00
#define     ARGCFG_BBASE_S           8
#define     ARBCFG_IBASE_BITS        0x000000FF
#define     ARBCFG_IBASE_S           0
#define ARBITER_TTECONFIG0       0x04820160
#define     ARBTTE_IUPPER_BITS       0xFF000000
#define     ARBTTE_IUPPER_S          24
#define     ARBTTE_ILOWER_BITS       0x00FF0000
#define     ARBTTE_ILOWER_S          16
#define     ARBTTE_BUPPER_BITS       0x0000FF00
#define     ARBTTE_BUPPER_S          8
#define     ARBTTE_BLOWER_BITS       0x000000FF
#define     ARBTTE_BLOWER_S          0
#define ARBITER_STRIDE           8

#endif /* _METAC_ARBITER_VALUES */
#endif /* METAC_ARBITER_VALUES */

/*
 * Expansion area registers
 * --------------------------------------
 */
#ifdef METAC_EXPAND_VALUES
#ifndef _METAC_EXPAND_VALUES_
#define _METAC_EXPAND_VALUES_

/* These defines are to be deprecated. Define METAC_WRCOMB_VALUES instead. */
#define EXPAND_T0WRCOMBINE         0x03000000
#define     EXPWRC_LFFEn_BIT           0x00004000  /* Enable auto line full flush */
#define     EXPWRC_ENABLE_BIT          0x00002000  /* Enable write combiner */
#define     EXPWRC_TIMEOUT_ENABLE_BIT  0x00001000  /* Timeout flush enable */
#define     EXPWRC_TIMEOUT_COUNT_BITS  0x000003FF  
#define     EXPWRC_TIMEOUT_COUNT_S     0
#define EXPAND_TnWRCOMBINE_STRIDE  0x00000008

/* These defines are to be deprecated. Define METAC_ARBITER_VALUES instead. */
#define EXPAND_T0ARBITER         0x03000020
#define     EXPARB_BPRIORITY_BIT 0x02000000
#define     EXPARB_IPRIORITY_BIT 0x01000000
#define     EXPARB_PAGE_BITS     0x00FF0000
#define     EXPARB_PAGE_S        16
#define     EXPARB_BBASE_BITS    0x0000FF00
#define     EXPARB_BBASE_S       8
#define     EXPARB_IBASE_BITS    0x000000FF
#define     EXPARB_IBASE_S       0
#define EXPAND_TnARBITER_STRIDE  0x00000008

/* These definitions are to be deprecated. Define METAC_SYSC_VALUES instead. */
#define EXPAND_TIMER_DIV   0x03000040
#define     EXPTIM_DIV_BITS      0x000000FF
#define     EXPTIM_DIV_S         0

/* These definitions are to be deprecated. Define METAC_PERF_VALUES instead. */
#define EXPAND_PERFCHAN0   0x03000050
#define EXPAND_PERFCHAN1   0x03000058
#define     EXPPERF_CTRL_BITS    0x0000000F
#define     EXPPERF_CTRL_S       0
#define         EXPPERF_WRC_WRBURST   0x0   /* Write combiner write burst */
#define         EXPPERF_WRC_WRITE     0x1   /* Write combiner write       */
#define         EXPPERF_WRC_RDBURST   0x2   /* Write combiner read burst  */
#define         EXPPERF_WRC_READ      0x3   /* Write combiner read        */
#define         EXPPERF_PREARB_DELAY  0x4   /* Pre-arbiter delay cycle    */
                                            /* Cross-bar hold-off cycle:  */
#define         EXPPERF_XBAR_HOLDWRAP 0x5   /*    wrapper register        */
#define         EXPPERF_XBAR_HOLDSBUS 0x6   /*    system bus              */
#define         EXPPERF_XBAR_HOLDINTR 0x8   /*    internal ram            */
#define         EXPPERF_XBAR_HOLDCREG 0x9   /*    core registers          */
                                            /* Admission delay cycle:     */ 
#define         EXPPERF_INPUT_SBUS    0xA   /*    register port           */
#define         EXPPERF_INPUT_CREG    0xB   /*    core registers          */
#define         EXPPERF_INPUT_INTR    0xC   /*    internal ram            */
#define         EXPPERF_INPUT_WRC     0xD   /*    write combiners(memory) */
#define         EXPPERF_MAINARB_DELAY 0xF   /* Main arbiter delay cycle   */

#endif
#endif

/*
 * Debug port registers
 * --------------------------------------
 */
#ifdef METAC_DEBUGPORT_VALUES
#ifndef _METAC_DEBUGPORT_VALUES_
#define _METAC_DEBUGPORT_VALUES_

/* Data Exchange Register */
#define DBGPORT_MDBGDATAX                    0x0

/* Data Transfer register */
#define DBGPORT_MDBGDATAT                    0x4

/* Control Register 0 */
#define DBGPORT_MDBGCTRL0                    0x8
#define     DBGPORT_MDBGCTRL0_ADDR_BITS      0xFFFFFFFC
#define     DBGPORT_MDBGCTRL0_ADDR_S         2
#define     DBGPORT_MDBGCTRL0_AUTOINCR_BIT   0x00000002
#define     DBGPORT_MDBGCTRL0_RD_BIT         0x00000001

/* Control Register 1 */
#define DBGPORT_MDBGCTRL1                    0xC
#define    DBGPORT_MDBGCTRL1_DEFERRTHREAD_BITS      0xC0000000
#define    DBGPORT_MDBGCTRL1_DEFERRTHREAD_S         30
#define     DBGPORT_MDBGCTRL1_LOCK2_INTERLOCK_BIT   0x20000000
#define     DBGPORT_MDBGCTRL1_ATOMIC_INTERLOCK_BIT  0x10000000
#define     DBGPORT_MDBGCTRL1_TRIGSTATUS_BIT        0x08000000
#define     DBGPORT_MDBGCTRL1_GBLPORT_IDLE_BIT      0x04000000
#define     DBGPORT_MDBGCTRL1_COREMEM_IDLE_BIT      0x02000000
#define     DBGPORT_MDBGCTRL1_READY_BIT             0x01000000
#define     DBGPORT_MDBGCTRL1_DEFERRID_BITS         0x00E00000
#define     DBGPORT_MDBGCTRL1_DEFERRID_S            21
#define     DBGPORT_MDBGCTRL1_DEFERR_BIT            0x00100000
#define     DBGPORT_MDBGCTRL1_WR_ACTIVE_BIT         0x00040000
#define     DBGPORT_MDBGCTRL1_COND_LOCK2_BIT        0x00020000
#define     DBGPORT_MDBGCTRL1_LOCK2_BIT             0x00010000
#define     DBGPORT_MDBGCTRL1_DIAGNOSE_BIT          0x00008000
#define     DBGPORT_MDBGCTRL1_FORCEDIAG_BIT         0x00004000
#define     DBGPORT_MDBGCTRL1_MEMFAULT_BITS         0x00003000
#define     DBGPORT_MDBGCTRL1_MEMFAULT_S            12
#define     DBGPORT_MDBGCTRL1_TRIGGER_BIT           0x00000100
#define     DBGPORT_MDBGCTRL1_INTSPECIAL_BIT        0x00000080
#define     DBGPORT_MDBGCTRL1_INTRUSIVE_BIT         0x00000040
#define     DBGPORT_MDBGCTRL1_THREAD_BITS           0x00000030 /* Thread mask selects threads */
#define     DBGPORT_MDBGCTRL1_THREAD_S              4
#define     DBGPORT_MDBGCTRL1_TRANS_SIZE_BITS       0x0000000C
#define     DBGPORT_MDBGCTRL1_TRANS_SIZE_S          2
#define         DBGPORT_MDBGCTRL1_TRANS_SIZE_32_BIT 0x00000000
#define         DBGPORT_MDBGCTRL1_TRANS_SIZE_16_BIT 0x00000004
#define         DBGPORT_MDBGCTRL1_TRANS_SIZE_8_BIT  0x00000008
#define     DBGPORT_MDBGCTRL1_BYTE_ROUND_BITS       0x00000003
#define     DBGPORT_MDBGCTRL1_BYTE_ROUND_S          0
#define         DBGPORT_MDBGCTRL1_BYTE_ROUND_8_BIT  0x00000001
#define         DBGPORT_MDBGCTRL1_BYTE_ROUND_16_BIT 0x00000002

#endif
#endif

#ifdef METAC_TRACE_VALUES
#ifndef _METAC_TRACE_VALUES_
#define _METAC_TRACE_VALUES_

/*****************************************************************************
                        RUN TIME TRACE CONTROL REGISTERS
 ****************************************************************************/
/*
 * The following values are only relevant to code that implements run-time 
 *  trace features within the META Core
 */
#define TTEXEC      TT.0
#define TTCTRL      TT.1
#define TTMARK      TT.2
#define TTREC       TT.3
#define GTEXEC      TT.4

#define TTEXEC_REGNUM               0
#define     TTEXEC_EXTTRIGAND_BITS      0x7F000000  
#define     TTEXEC_EXTTRIGAND_S         24      
#define     TTEXEC_EXTTRIGEN_BIT        0x00008000  
#define     TTEXEC_EXTTRIGMATCH_BITS    0x00007F00  
#define     TTEXEC_EXTTRIGMATCH_S       8       
#define     TTEXEC_TCMODE_BITS          0x00000003  
#define     TTEXEC_TCMODE_S             0       

#define TTCTRL_REGNUM               1
#define     TTCTRL_TRACETT_BITS         0x00008000
#define     TTCTRL_TRACETT_S            15
#define     TTCTRL_TRACEALL_BITS        0x00002000
#define     TTCTRL_TRACEALL_S           13
#define     TTCTRL_TRACEALLTAG_BITS     0x00000400
#define     TTCTRL_TRACEALLTAG_S        10
#define     TTCTRL_TRACETAG_BITS        0x00000200
#define     TTCTRL_TRACETAG_S           9
#define     TTCTRL_TRACETTPC_BITS       0x00000080
#define     TTCTRL_TRACETTPC_S          7
#define     TTCTRL_TRACEMPC_BITS        0x00000020
#define     TTCTRL_TRACEMPC_S           5
#define     TTCTRL_TRACEEN_BITS         0x00000008
#define     TTCTRL_TRACEEN_S            3
#define     TTCTRL_TRACEEN1_BITS        0x00000004
#define     TTCTRL_TRACEEN1_S           2
#define     TTCTRL_TRACEPC_BITS         0x00000002
#define     TTCTRL_TRACEPC_S            1

#define TTMARK_REGNUM   2
#define TTMARK_BITS                 0xFFFFFFFF
#define TTMARK_S                    0x0

#define TTREC_REGNUM    3
#define TTREC_BITS                  0xFFFFFFFFFFFFFFFF
#define TTREC_S                     0x0

#define GTEXEC_REGNUM               4
#define     GTEXEC_DCRUN_BITS           0x80000000
#define     GTEXEC_DCRUN_S              31
#define     GTEXEC_ICMODE_BITS          0x0C000000
#define     GTEXEC_ICMODE_S             26
#define     GTEXEC_TCMODE_BITS          0x03000000
#define     GTEXEC_TCMODE_S             24
#define     GTEXEC_PERF1CMODE_BITS      0x00040000
#define     GTEXEC_PERF1CMODE_S         18
#define     GTEXEC_PERF0CMODE_BITS      0x00010000
#define     GTEXEC_PERF0CMODE_S         16
#define     GTEXEC_REFMSEL_BITS         0x0000F000
#define     GTEXEC_REFMSEL_S            12
#define     GTEXEC_METRICTH_BITS        0x000003FF
#define     GTEXEC_METRICTH_S           0

#endif
#endif

/*
 * Clock Control registers
 * -----------------------
 */
#ifdef METAC_CLKCTRL_VALUES
#ifndef _METAC_CLKCTRL_VALUES_
#define _METAC_CLKCTRL_VALUES_
#define TXCLKCTRL_REGNUM        22

/*
 * Default setting is with clocks always on (DEFON), turning all clocks off
 * can only be done from external devices (OFF), enabling automatic clock
 * gating will allow clocks to stop as units fall idle.
 */
#define TXCLKCTRL_ALL_OFF       0x02222222
#define TXCLKCTRL_ALL_DEFON     0x01111111
#define TXCLKCTRL_ALL_AUTO      0x02222222

/*
 * Individual fields control caches, floating point and main data/addr units
 */
#define TXCLKCTRL_CLOCKIC_BITS  0x03000000
#define TXCLKCTRL_CLOCKIC_S     24
#define TXCLKCTRL_CLOCKDC_BITS  0x00300000
#define TXCLKCTRL_CLOCKDC_S     20
#define TXCLKCTRL_CLOCKFP_BITS  0x00030000
#define TXCLKCTRL_CLOCKFP_S     16
#define TXCLKCTRL_CLOCKD1_BITS  0x00003000
#define TXCLKCTRL_CLOCKD1_S     12
#define TXCLKCTRL_CLOCKD0_BITS  0x00000300
#define TXCLKCTRL_CLOCKD0_S     8
#define TXCLKCTRL_CLOCKA1_BITS  0x00000030
#define TXCLKCTRL_CLOCKA1_S     4
#define TXCLKCTRL_CLOCKA0_BITS  0x00000003
#define TXCLKCTRL_CLOCKA0_S     0

/*
 * Individual settings for each field are common
 */
#define TXCLKCTRL_CLOCKxx_OFF   0
#define TXCLKCTRL_CLOCKxx_DEFON 1
#define TXCLKCTRL_CLOCKxx_AUTO  2
#endif
#endif

/* L2 Cache registers */
#ifdef METAC_L2CACHE_VALUES
#ifndef _METAC_L2CACHE_VALUES_
#define _METAC_L2CACHE_VALUES_

#define SYSC_L2C_INIT              0x048300C0
#define SYSC_L2C_INIT_INIT                  1
#define SYSC_L2C_INIT_IN_PROGRESS           0
#define SYSC_L2C_INIT_COMPLETE              1

#define SYSC_L2C_ENABLE            0x048300D0
#define SYSC_L2C_ENABLE_ENABLE_BIT     0x00000001
#define SYSC_L2C_ENABLE_PFENABLE_BIT   0x00000002

#define SYSC_L2C_PURGE             0x048300C8
#define SYSC_L2C_PURGE_PURGE                1
#define SYSC_L2C_PURGE_IN_PROGRESS          0
#define SYSC_L2C_PURGE_COMPLETE             1

#endif
#endif

#endif  /* #ifndef _METAC_2_1_H_ */
#endif   /* #ifdef METAC_2_1 */

/* End of metac_2_1.inc */
