/*
 * metac_1_2.inc
 *
 * Copyright (C) 2000, 2001, 2002, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * Assembler header for defining target specifics (v1.2)
 *
 *            defines :
 *
 *              __ANYC__ - implies inclusion in any C compile (i.e. not assembly)
 *              __STDC__ - implies inclusion in any ANSI C compile
 *             METAC_1_2 - set if this specific chip is being targeted
 *      METAC_..._VALUES - set if various chip specific META defines needed
 *      METAC_ALL_VALUES - set if you want all data possible to be defined
 */

#ifdef METAC_1_2
#ifndef _METAC_1_2_H_
#define _METAC_1_2_H_

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

#define     TXPIOREG_WRC_BIT          0x00080000  /* Wr combiner reg priv */
#define     TXPIOREG_LOCALBUS_RW_BIT  0x00040000  /* Local bus rd/wr priv */
#define     TXPIOREG_SYSREGBUS_RD_BIT 0x00020000  /* Sys reg bus write priv */
#define     TXPIOREG_SYSREGBUS_WR_BIT 0x00010000  /* Sys reg bus read priv */

/* CORE region privilege controls */
#define T0PRIVCORE 0x04800828
#define         TXPRIVCORE_TXBKICK_BIT   0x001  /* Background kick priv */
#define         TXPRIVCORE_TXIKICK_BIT   0x002  /* Interrupt kick priv  */
#define         TXPRIVCORE_TXAMAREGX_BIT 0x004  /* TXAMAREG4|5|6 priv   */
#define TnPRIVCORE_STRIDE 0x00001000

#define T0PRIVSYSR 0x04810000
#define     TnPRIVSYSR_STRIDE   0x00000008
#define     TnPRIVSYSR_STRIDE_S 3
#define     TXPRIVSYSR_ALL_BITS       0x07
#define     TXPRIVSYSR_CFLUSH_BIT     0x01
#define     TXPRIVSYSR_MTABLE_BIT     0x02
#define     TXPRIVSYSR_DIRECT_BIT     0x04
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
#define     TXCATCH0_LDRXX_BITS  0xF8000000  /* Load destination reg 0-31 */
#define     TXCATCH0_LDRXX_S     27
#define     TXCATCH0_LDDST_BITS  0x07FF0000  /* Load destination bits */
#define     TXCATCH0_LDDST_S     16
#define         TXCATCH0_LDDST_D1DSP 0x400   /* One bit set if it's a LOAD */
#define         TXCATCH0_LDDST_D0DSP 0x200
#define         TXCATCH0_LDDST_TMPLT 0x100
#define         TXCATCH0_LDDST_TR    0x080
#define         TXCATCH0_LDDST_PC    0x020
#define         TXCATCH0_LDDST_A1    0x010
#define         TXCATCH0_LDDST_A0    0x008
#define         TXCATCH0_LDDST_D1    0x004
#define         TXCATCH0_LDDST_D0    0x002
#define         TXCATCH0_LDDST_CT    0x001
#define     TXCATCH0_WATCHS_BIT  0x00004000  /* Set if Data Watch set fault */
#define     TXCATCH0_WATCH1_BIT  0x00002000  /* Set if Data Watch 1 matches */
#define     TXCATCH0_WATCH0_BIT  0x00001000  /* Set if Data Watch 0 matches */
#define     TXCATCH0_FAULT_BITS  0x00000C00  /* See TXSTATUS_MEMFAULT_*     */
#define     TXCATCH0_FAULT_S     10
#define     TXCATCH0_PRIV_BIT    0x00000200  /* Privilege of transaction    */
#define     TXCATCH0_READ_BIT    0x00000100  /* Set for Read or Load cases  */

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
#define     TXAMAREG0_CTRL_BITS 0x07000000
#define     TXAMAREG0_CTRL_S    24
#define     TXAMAREG0_MASK_BITS 0x00700000
#define     TXAMAREG0_MASK_S    20
#define     TXAMAREG0_STATUS_BITS   0x00070000
#define     TXAMAREG0_STATUS_S  16
#define     TXAMAREG0_PRIORITY_BITS 0x0000FF00
#define     TXAMAREG0_PRIORITY_S    8
#define     TXAMAREG0_BVALUE_BITS   0x000000FF
#define     TXAMAREG0_BVALUE_S  0
#define TXAMAREG1_REGNUM 25
#define     TXAMAREG1_DELAYC_BITS   0x07FFFFFF
#define     TXAMAREG1_DELAYC_S  0
#define TXAMAREG2_REGNUM 26
#define     TXAMAREG2_DLINEC_BITS   0x00FFFFFF
#define     TXAMAREG2_DLINEC_S  0
#define TXAMAREG3_REGNUM 27
#define     TXAMAREG2_AMABLOCK_BIT  0x00080000
#define     TXAMAREG2_AMAC_BITS 0x0000FFFF
#define     TXAMAREG2_AMAC_S    0

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
#define     TXUCT_MASK  0xFF0FFFFF  /* Valid regs 0..31  */
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

#define TXUXX_MASKS { TXUCT_MASK, TXUDX_MASK, TXUDX_MASK, TXUAX_MASK, \
                      TXUAX_MASK, TXUPC_MASK,          0, TXUTR_MASK, \
                      0, 0, 0, 0, 0, 0, 0, 0                          }
#define TGUXX_MASKS { TGUCT_MASK, TGUDX_MASK, TGUDX_MASK, TGUAX_MASK, \
                      TGUAX_MASK, TGUPC_MASK,          0, TGUTR_MASK, \
                      0, 0, 0, 0, 0, 0, 0, 0                          }
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
#define     MMCU_CWIN_BURST     1 /* Is memory, but uncached */
#define     MMCU_CWIN_C1SET     2 /* Cached in 1 set only    */
#define     MMCU_CWIN_CACHED    3 /* Fully cached            */
#define MMCU_ENTRY_CACHE_BIT 0x00000080   /* Set for cached region         */
#define     MMCU_ECACHE1_FULL_BIT  0x00000040 /* Use all the sets */
#define     MMCU_ECACHE0_BURST_BIT 0x00000040 /* Match bursts     */
#define MMCU_ENTRY_SYS_BIT   0x00000010   /* Sys-coherent access required  */
#define MMCU_ENTRY_WRC_BIT   0x00000008   /* Write combining allowed       */
#define MMCU_ENTRY_PRIV_BIT  0x00000004   /* Privilege required            */
#define MMCU_ENTRY_WR_BIT    0x00000002   /* Writes allowed                */
#define MMCU_ENTRY_VAL_BIT   0x00000001   /* Entry is valid                */

/* MMCU control register in SYSC region */
#define MMCU_TABLE_PHYS_ADDR        0x04830010
#define     MMCU_TABLE_PHYS_ADDR_BITS   0xFFFFFFFC
#define MMCU_DCACHE_CTRL_ADDR       0x04830018
#define     MMCU_xCACHE_CTRL_ENABLE_BIT     0x00000001
#define     MMCU_xCACHE_CTRL_PARTITION_BIT  0x00000000 /* See xCPART below */
#define MMCU_ICACHE_CTRL_ADDR       0x04830020
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
#define     METAC_ID_CHIPID_BITS    0x000000FF
#define     METAC_ID_CHIPID_S       0

#define SYSC_CACHE_MMU_CONFIG       0x04830028
#define     SYSC_CMMUCFG_ON             0x7
/* Historical deprecated values (Turn all three on - no sub-sets) */
#define     SYSC_CMMUCFG_MMU_ON_BIT     0x1
#define     SYSC_CMMUCFG_DC_ON_BIT      0x2
#define     SYSC_CMMUCFG_IC_ON_BIT      0x4
#define SYSC_JTAG_THREAD            0x04830030
#define     SYSC_JTAG_TX_BITS           0x000003 /* Read only bits! */
#define     SYSC_JTAG_TX_S              0
#define     SYSC_JTAG_PRIV_BIT          0x000004
#define SYSC_DCACHE_FLUSH           0x04830038
#define SYSC_ICACHE_FLUSH           0x04830040
#define  SYSC_xCACHE_FLUSH_INIT     0x1
#define MMCU_DIRECTMAP0_ADDR        0x04830080 /* LINSYSDIRECT_BASE -> */
#define     MMCU_DIRECTMAPn_STRIDE      0x00000010 /* 4 Region settings */
#define     MMCU_DIRECTMAPn_S               4
#define         MMCU_DIRECTMAPn_ADDR_BITS       0xFF800000
#define         MMCU_DIRECTMAPn_ADDR_S          23
#define         MMCU_DIRECTMAPn_ADDR_SCALE      0x00800000 /* 8M Regions */
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
#define         SYSC_MCMGID_NODEV       0xFF
#define         SYSC_MCMGID_DSPRAM0A    0x04 /* DSP RAM access */
#define         SYSC_MCMGID_DSPRAM0B    0x05
#define         SYSC_MCMGID_DSPRAM1A    0x06
#define         SYSC_MCMGID_DSPRAM1B    0x07
#define         SYSC_MCMGID_DCACHEL     0x08 /* DCACHE lines, tags, LRU */
#define         SYSC_MCMGID_DCACHET     0x0A
#define         SYSC_MCMGID_DCACHELRU   0x0B
#define         SYSC_MCMGID_ICACHEL     0x0C /* ICACHE lines, tags, LRU */
#define         SYSC_MCMGID_ICACHET     0x0E
#define         SYSC_MCMGID_ICACHELRU   0x0F
#define         SYSC_MCMGID_COREIRAM0   0x10 /* Core code mem id 0 */
#define         SYSC_MCMGID_COREIRAMn   0x17
#define         SYSC_MCMGID_COREDRAM0   0x18 /* Core data mem id 0 */
#define         SYSC_MCMGID_COREDRAMn   0x1F
#define     SYSC_MCMGCTRL_TR31_BIT  0x80000000 /* Trigger 31 on completion */
#define SYSC_MCMSTATUS 0x04830318   /* Status read only */
#define     SYSC_MCMSTATUS_IDLE_BIT 0x00000001

/* META System Events */
#define SYSC_SYS_EVENT       0x04830400
#define     SYSC_SYSEVT_ATOMIC_BIT      0x00000001
#define     SYSC_SYSEVT_CACHEX_BIT      0x00000002
#define SYSC_ATOMIC_LOCK     0x04830408
#define     SYSC_ATOMIC_STATE_TX_BITS 0x0F
#define     SYSC_ATOMIC_STATE_TX_S    0
#define     SYSC_ATOMIC_STATE_DX_BITS 0xF0
#define     SYSC_ATOMIC_STATE_DX_S    4
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

#define    PERFCTRL_DHITS   8  /* Dcache Hits */
#define    PERFCTRL_IHITS   9  /* Icache Hits */
#define    PERFCTRL_IMISS   10 /* Icache Misses */
#endif
#endif

/*
 * Expansion area registers
 * --------------------------------------
 */
#ifdef METAC_EXPAND_VALUES
#ifndef _METAC_EXPAND_VALUES_
#define _METAC_EXPAND_VALUES_

#define EXPAND_T0WRCOMBINE       0x03000000
#define     EXPWRC_ENABLE_BIT       0x00002000  /* Enable write combiner */
#define  EXPWRC_TIMEOUT_ENABLE_BIT  0x00001000  /* Timeout flush enable */
#define  EXPWRC_TIMEOUT_COUNT_BITS  0x000003FF  
#define  EXPWRC_TIMEOUT_COUNT_S     0
#define EXPAND_TnWRCOMBINE_STRIDE      0x00000008

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

#define EXPAND_TIMER_DIV   0x03000040
#define     EXPTIM_DIV_BITS      0x000000FF
#define     EXPTIM_DIV_S         0

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
#define         EXPPERF_XBAR_HOLDLBUS 0x7   /*    local bus               */
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
#define     DBGPORT_MDBGCTRL1_LOCK2_INTERLOCK_BIT   0x20000000
#define     DBGPORT_MDBGCTRL1_ATOMIC_INTERLOCK_BIT  0x10000000
#define     DBGPORT_MDBGCTRL1_TRIGSTATUS_BIT        0x08000000
#define     DBGPORT_MDBGCTRL1_GBLPORT_IDLE_BIT      0x04000000
#define     DBGPORT_MDBGCTRL1_COREMEM_IDLE_BIT      0x02000000
#define     DBGPORT_MDBGCTRL1_READY_BIT             0x01000000
#define     DBGPORT_MDBGCTRL1_WR_ACTIVE_BIT         0x00040000
#define     DBGPORT_MDBGCTRL1_COND_LOCK2_BIT        0x00020000
#define     DBGPORT_MDBGCTRL1_LOCK2_BIT             0x00010000
#define     DBGPORT_MDBGCTRL1_DIAGNOSE_BIT          0x00008000
#define     DBGPORT_MDBGCTRL1_FORCEDIAG_BIT         0x00004000
#define     DBGPORT_MDBGCTRL1_MEMFAULT_BITS         0x00003000
#define     DBGPORT_MDBGCTRL1_MEMFAULT_S            12
#define     DBGPORT_MDBGCTRL1_TRIGGER_BIT           0x00000100
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

#endif  /* #ifndef _METAC_1_2_H_ */
#endif   /* #ifdef METAC_1_2 */

/* End of metac_1_2.inc */
