/*
 * tbicache.c
 *
 * Copyright (C) 2001, 2002, 2005, 2007, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * Cache control code shared as part of TBI
 */

#define METAC_ALL_VALUES
#define METAG_ALL_VALUES
#include "machine.inc"
#include "metagtbx.h"

#define DEFAULT_CACHE_WAYS_LOG2	2

/* Size of a set in the caches. Initialised for default 16K stride, adjusted
 * according to values passed through TBI global heap segment via LDLK (on ATP)
 * or config registers (on HTP/MTP)
 */
static int __TBICacheDSetShift = METAG_TBI_CACHE_SIZE_BASE_LOG2-DEFAULT_CACHE_WAYS_LOG2;
static int __TBICacheISetShift = METAG_TBI_CACHE_SIZE_BASE_LOG2-DEFAULT_CACHE_WAYS_LOG2;
/* The number of sets in the caches. Initialised for HTP/ATP, adjusted
 * according to NOMMU setting in config registers
 */
static unsigned char __TBICacheDSetsLog2 = DEFAULT_CACHE_WAYS_LOG2;
static unsigned char __TBICacheISetsLog2 = DEFAULT_CACHE_WAYS_LOG2;

static int __TBICacheDoneInit = 0;

static void __TBICacheInit( void )
{

	int version = *(volatile int*)METAC_ID;
	
	if ((version & METAC_ID_MAJOR_BITS) >= (0x02 << METAC_ID_MAJOR_S))
	{
		int coreid = *(volatile int*)METAC_CORE_ID;
		int config = *(volatile int*)METAC_CORE_CONFIG2;
		int cfgcache = coreid & METAC_COREID_CFGCACHE_BITS;

		if (   cfgcache == METAC_COREID_CFGCACHE_TYPE0
		    || cfgcache == METAC_COREID_CFGCACHE_PRIVNOMMU
				)
		{
			__TBICacheISetsLog2 = 1;
			__TBICacheDSetsLog2 = 1;
		}

		/* For normal size caches, the smallest size is 4Kb.
		   For small caches, the smallest size is 64b */
		__TBICacheISetShift = (config & METAC_CORECFG2_ICSMALL_BIT) ? 6 : 12;
		__TBICacheISetShift += ((config & METAC_CORE_C2ICSZ_BITS) >> METAC_CORE_C2ICSZ_S);
		__TBICacheISetShift -= __TBICacheISetsLog2;

		__TBICacheDSetShift = (config & METAC_CORECFG2_DCSMALL_BIT) ? 6 : 12;
		__TBICacheDSetShift += ((config & METAC_CORECFG2_DCSZ_BITS) >> METAC_CORECFG2_DCSZ_S);
		__TBICacheDSetShift -= __TBICacheDSetsLog2;
	}
	else
	{
		/* Extract cache sizes from global heap segment */
		unsigned long val, u;
		int width, shift, addend;

		PTBISEG pSeg = __TBIFindSeg( NULL, TBID_SEG( TBID_THREAD_GLOBAL,
		                             TBID_SEGSCOPE_GLOBAL, TBID_SEGTYPE_HEAP) );
		if ( pSeg != NULL )
		{
			val = pSeg->Data[1];

			/* Work out width of I-cache size bit-field */
			u = ((unsigned long) METAG_TBI_ICACHE_SIZE_BITS)
			       >> METAG_TBI_ICACHE_SIZE_S;
			width = 0;
			while ( u & 1 )
			{
				width++;
				u >>= 1;
			}
			/* Extract sign-extended size addend value */
			shift = 32 - (METAG_TBI_ICACHE_SIZE_S + width);
			addend = (long) ((val & METAG_TBI_ICACHE_SIZE_BITS) << shift)
			           >> (shift + METAG_TBI_ICACHE_SIZE_S);
			/* Now calculate I-cache set size */
			__TBICacheISetShift = (METAG_TBI_CACHE_SIZE_BASE_LOG2-DEFAULT_CACHE_WAYS_LOG2)
			                        + addend;

			/* Similarly for D-cache */
			u = ((unsigned long) METAG_TBI_DCACHE_SIZE_BITS)
			       >> METAG_TBI_DCACHE_SIZE_S;
			width = 0;
			while ( u & 1 )
			{
				width++;
				u >>= 1;
			}
			shift = 32 - (METAG_TBI_DCACHE_SIZE_S + width);
			addend = (long) ((val & METAG_TBI_DCACHE_SIZE_BITS) << shift)
			           >> (shift + METAG_TBI_DCACHE_SIZE_S);
			__TBICacheDSetShift = (METAG_TBI_CACHE_SIZE_BASE_LOG2-DEFAULT_CACHE_WAYS_LOG2)
			                        + addend;
		}
	}

	__TBICacheDoneInit = 1;
}

void __TBIDataCacheFlush( const void *pStart, int Bytes )
{
	volatile int *pCtrl = (volatile int *) SYSC_CACHE_MMU_CONFIG;

	if ( !__TBICacheDoneInit )
		__TBICacheInit();

	if ( (pCtrl[0] & SYSC_CMMUCFG_DC_ON_BIT) == 0 )
	{
		/* No need to flush the data cache it's not actually enabled */
		return;
	} 

	{
		/* Use a sequence of writes to flush the cache region requested */
		volatile char *pFlush0;
		int Loops, Step;
		int Thread = (TBI_GETREG(TXENABLE) & TXENABLE_THREAD_BITS)>>
		                                     TXENABLE_THREAD_S      ;
	
		if ( Bytes >= 4096 )
		{
			volatile char *pFlush1;
			volatile char *pFlush2;
			volatile char *pFlush3;

			/* Cache is broken into sets which lie in contiguous RAMs */
			int SetShift = __TBICacheDSetShift;
			
			/* Move to the base of the physical cache flush region */
			pFlush0 = (volatile char *) LINSYSCFLUSH_DCACHE_LINE;
			Step    = 64;

#ifdef METAC_1_1
			/* Set size */
			Loops = 1<<SetShift;
			
			if ( ( (Mode & MMCU_xCACHE_CTRL_PARTITION_BIT) != 0 ) &&
				 ( ((int) pStart) >= 0 )                             )
			{
				/* Only flush the 4K quarter that can contain local addrs */
				Loops >>= 2;
				pFlush0 += Loops * Thread;
			}
#else
			{
				/* Get partition data for this thread */
				int Part = ((volatile int *) (SYSC_DCPART0 +
				              (SYSC_xCPARTn_STRIDE*Thread)   ))[0];
				int Offset;
				
				if ( ((int) pStart) < 0 )
				{
					/* Access Global vs Local partition */ 
					Part >>= SYSC_xCPARTG_AND_S-SYSC_xCPARTL_AND_S;
				}
				
				/* Extract Offset and move SetOff */
				Offset = (Part & SYSC_xCPARTL_OR_BITS) >> SYSC_xCPARTL_OR_S;
				pFlush0 += (Offset << (SetShift - 4));

				/* Shrink size */
				Part = (Part & SYSC_xCPARTL_AND_BITS) >> SYSC_xCPARTL_AND_S;
				Loops = ((Part + 1) << (SetShift - 4));
			}
#endif
			/* Reduce Loops by step of cache line size */
			Loops /= Step;

			pFlush1 = pFlush0 + (1 << SetShift);
			pFlush2 = pFlush0 + (2 << SetShift);
			pFlush3 = pFlush0 + (3 << SetShift);

			if (__TBICacheDSetsLog2 == 1)
			{
				pFlush2 = pFlush1;
				pFlush3 = pFlush1 + Step;
				pFlush1 = pFlush0 + Step;
				Step  <<= 1;
				Loops >>= 1;
			}
			
			/* Clear Loops ways in cache */
			while ( Loops-- != 0 )
			{
				/* Clear the ways. */
#if 0
				/* GCC 2.95 doesnt generate very good code for this so we
				 * provide inline assembly instead.
				 */
				*pFlush0 = 0;
				*pFlush1 = 0;
				*pFlush2 = 0;
				*pFlush3 = 0;

				pFlush0 += Step;
				pFlush1 += Step;
				pFlush2 += Step;
				pFlush3 += Step;
#else
				asm volatile (
					"SETB\t[%0+%4++],%5\n"
					"SETB\t[%1+%4++],%5\n"
					"SETB\t[%2+%4++],%5\n"
					"SETB\t[%3+%4++],%5\n"
					: "+e" (pFlush0), "+e" (pFlush1), "+e" (pFlush2), "+e" (pFlush3) 
					: "e" (Step), "a" (0));
#endif
			}
		}
		else
		{
#ifdef METAC_1_1
			/* Use linear cache flush mechanism */
			pFlush0 = (volatile char *) (((unsigned int) pStart) >>
			                            LINSYSLFLUSH_S            );
			Loops  = (((int) pStart) & ((1<<LINSYSLFLUSH_S)-1)) + Bytes + 
			                                ((1<<LINSYSLFLUSH_S)-1)   ;
			Loops  >>= LINSYSLFLUSH_S;

#define PRIM_FLUSH( _addr, _offset ) _addr[_offset] = 0

#define LOOP_INC 4
#else
			/* Use linear cache flush mechanism on META IP */
			pFlush0 = (volatile char *) ((int) pStart);
			Loops  = (((int) pStart) & (DCACHE_LINE_BYTES-1)) + Bytes + 
			                                (DCACHE_LINE_BYTES-1)   ;
			Loops  >>= DCACHE_LINE_S;

#define PRIM_FLUSH( Addr, Offset )                                          {\
	int __Addr = ((int) (Addr)) + ((Offset) * 64);                           \
	TBIDCACHE_FLUSH( __Addr );                                               }

#define LOOP_INC (4*64)
#endif

			do
			{
				/* By default stop */
				Step = 0;
				
				switch ( Loops )
				{
					/* Drop Thru Cases! */
					default: PRIM_FLUSH( pFlush0, 3 );
					         Loops -= 4;
					         Step = 1;
					case 3:  PRIM_FLUSH( pFlush0, 2 );
					case 2:  PRIM_FLUSH( pFlush0, 1 );
					case 1:  PRIM_FLUSH( pFlush0, 0 );
					         pFlush0 += LOOP_INC;
					case 0:	 break;
				}
			}
			while ( Step ); 
		}
	}
}

void __TBICodeCacheFlush( const void *pStart, int Bytes )
{
	volatile int *pCtrl = (volatile int *) SYSC_CACHE_MMU_CONFIG;

	if ( !__TBICacheDoneInit )
		__TBICacheInit();

	if ( (pCtrl[0] & SYSC_CMMUCFG_IC_ON_BIT) == 0 )
	{
		/* No need to flush the code cache it's not actually enabled */
		return;
	} 

	{
		/* Use a sequence of writes to flush the cache region requested */
		int Loops, Step;
#ifdef TBI_1_3
		int TXEnable = TBI_GETREG(TXENABLE);
		int Thread = (TXEnable & TXENABLE_THREAD_BITS)>> TXENABLE_THREAD_S;
#else /* !TBI_1_3 */
		int Thread = (TBI_GETREG(TXENABLE) & TXENABLE_THREAD_BITS)>>
		                                     TXENABLE_THREAD_S      ;
#endif /* !TBI_1_3 */
		int SetShift = __TBICacheISetShift, SetSize;

#ifdef TBI_1_3
		/* If large size or not HTP/MTP */
		if ( ( Bytes >= 4096 ) || ( (TXEnable & TXENABLE_REV_STEP_BITS) <
		                            (3<<TXENABLE_REV_STEP_S)              ) )
#endif /* TBI_1_3 */
		{
			volatile char *pFlush0;
			volatile char *pFlush1;
			volatile char *pFlush2;
			volatile char *pFlush3;
			volatile char *pEndSet;

			/* Move to the base of the physical cache flush region */
			pFlush0 = (volatile char *) LINSYSCFLUSH_ICACHE_LINE;
			Step   = 64;

#ifdef METAC_1_1
			/* Set size */
			Loops = 1<<SetShift;
			
			if ( ( (Mode & MMCU_xCACHE_CTRL_PARTITION_BIT) != 0 ) &&
			     ( ((int) pStart) >= 0 )                             )
			{
				/* Only flush the 4K quarter that can contain local addrs */
				Loops >>= 2;
				pFlush0 += Loops * Thread;
			}
#else
			{
				/* Get partition code for this thread */
				int Part = ((volatile int *) (SYSC_ICPART0 +
				              (SYSC_xCPARTn_STRIDE*Thread)   ))[0];
				int Offset;
				
				if ( ((int) pStart) < 0 )
				{
					/* Access Global vs Local partition */ 
					Part >>= SYSC_xCPARTG_AND_S-SYSC_xCPARTL_AND_S;
				}
				
				/* Extract Offset and move SetOff */
				Offset = (Part & SYSC_xCPARTL_OR_BITS) >> SYSC_xCPARTL_OR_S;
				pFlush0 += (Offset << (SetShift - 4));
	
				/* Shrink size */
				Part = (Part & SYSC_xCPARTL_AND_BITS) >> SYSC_xCPARTL_AND_S;
				Loops = ((Part + 1) << (SetShift - 4));
			}
#endif
			/* Where does the Set end? */
			pEndSet = pFlush0 + Loops;
			SetSize = Loops;
			
			if ( ( Bytes < 4096 ) && ( Bytes < Loops ) )
			{
				/* Unreachable on HTP/MTP */
				/* Only target the sets that could be relavent */
				pFlush0 += (Loops - Step) & ((int) pStart);
				Loops    = (((int) pStart) & (Step-1)) + Bytes + Step - 1;
			}
  
			/* Reduce Loops by step of cache line size */
			Loops /= Step;

			pFlush1 = pFlush0 + (1<<SetShift);
			pFlush2 = pFlush0 + (2<<SetShift);
			pFlush3 = pFlush0 + (3<<SetShift);
  
			if (__TBICacheISetsLog2 == 1)
			{
				pFlush2 = pFlush1;
				pFlush3 = pFlush1 + Step;
				pFlush1 = pFlush0 + Step;
#if 0
				/* pFlush0 will stop one line early in this case
				 * (pFlush1 will do the final line).
				 * However we don't correct pEndSet here at the moment
				 * because it will never wrap on HTP/MTP
				 */
				pEndSet -= Step;
#endif
				Step  <<= 1;
				Loops >>= 1;
			}
				
			/* Clear Loops ways in cache */
			while ( Loops-- != 0 )
			{
#if 0
				/* GCC 2.95 doesnt generate very good code for this so we
				 * provide inline assembly instead.
				 */
				/* Clear the ways */
				*pFlush0 = 0;
				*pFlush1 = 0;
				*pFlush2 = 0;
				*pFlush3 = 0;

				pFlush0 += Step;
				pFlush1 += Step;
				pFlush2 += Step;
				pFlush3 += Step;
#else
				asm volatile (
					"SETB\t[%0+%4++],%5\n"
					"SETB\t[%1+%4++],%5\n"
					"SETB\t[%2+%4++],%5\n"
					"SETB\t[%3+%4++],%5\n"
					: "+e" (pFlush0), "+e" (pFlush1), "+e" (pFlush2), "+e" (pFlush3) 
					: "e" (Step), "a" (0));
#endif
  
				if ( pFlush0 == pEndSet )
				{
					/* Wrap within Set 0 */
					pFlush0 -= SetSize;
					pFlush1 -= SetSize;
					pFlush2 -= SetSize;
					pFlush3 -= SetSize;
				}
			}
		}
#ifdef TBI_1_3
		else
		{
			volatile char *pFlush;

			/* Use linear cache flush mechanism on META IP */
			pFlush = (volatile char *) (((int) pStart) & ~(ICACHE_LINE_BYTES-1));
			Loops  = (((int) pStart) & (ICACHE_LINE_BYTES-1)) + Bytes + 
			                                (ICACHE_LINE_BYTES-1)   ;
			Loops  >>= ICACHE_LINE_S;


#define PRIM_IFLUSH( Addr, Offset ) \
  TBIXCACHE_WD(( (Addr) + ( (Offset) * 64) ), CACHEW_ICACHE_BIT)

#define LOOP_INC (4*64)

			do
			{
				/* By default stop */
				Step = 0;
				
				switch ( Loops )
				{
					/* Drop Thru Cases! */
					default: PRIM_IFLUSH( pFlush, 3 );
					         Loops -= 4;
					         Step = 1;
					case 3:  PRIM_IFLUSH( pFlush, 2 );
					case 2:  PRIM_IFLUSH( pFlush, 1 );
					case 1:  PRIM_IFLUSH( pFlush, 0 );
					         pFlush += LOOP_INC;
					case 0:  break;
				}
			}
			while ( Step ); 
		}
#endif /* TBI_1_3 */
	}
}


/* End of tbicache.c */
