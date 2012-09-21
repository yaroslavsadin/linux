/*
 * tbistring.c
 *
 * Copyright (C) 2001, 2002, 2003, 2005, 2007, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * String table functions provided as part of the thread binary interface for
 * META processors
 */

#define METAC_ALL_VALUES
#define METAG_ALL_VALUES
#include "machine.inc"

#include "metagtbx.h"

/* There are not any functions to modify the string table currently, if
   these are required at some later point I suggest having a seperate module
   and ensuring that creating new entries does not interfere with reading old
   entries in any way. */

const char *__TBICmpStr( const char *pSrc1, const char *pSrc2 )
{
	/* Compare two strings until we reach the end or get a difference */
	for (pSrc1--;;)
	{
		char Ch = *++pSrc1;

		/* Compare against *pSrc2 */
		if ( Ch != *pSrc2++ ) break;

		if ( Ch == 0 )
		{
			/* Return NULL for precise match! */
			pSrc1 = NULL;
			break;
		}
	}
	
	return ( pSrc1 );
}

const TBISTR *__TBIFindStr( const TBISTR *pStart,
                            const char *pStr, int MatchLen )
{
	const TBISTR *pSearch = pStart;
	int Exact = 1;
	
	if ( MatchLen < 0 )
	{
		/* Make MatchLen always positive for the inner loop */
		MatchLen = -MatchLen;
		Exact = 0;
	}
	else
	{
		/* Also support historic behaviour, which expected 
		   MatchLen to include null terminator */
		if ( pStr[MatchLen-1] == '\0' )
			MatchLen--;
	}
	
	if ( pSearch == NULL )
	{
		/* Find global string table segment */
		PTBISEG pSeg = __TBIFindSeg( NULL, TBID_SEG( TBID_THREAD_GLOBAL,
		                        TBID_SEGSCOPE_GLOBAL, TBID_SEGTYPE_STRING) );
		
		if ( (pSeg == NULL) || (pSeg->Bytes < sizeof(TBISTR)) )
		{
			/* No string table! */
			return (NULL);
		}

		/* Start of string table */
		pSearch = pSeg->pGAddr;
	}
		
	for (;;)
	{
		while ( pSearch->Tag == 0 )
		{
			/* Allow simple gaps which are just zero initialised */
			pSearch = (const TBISTR *) (((const char *) pSearch) + 8);
		}

		if ( pSearch->Tag == METAG_TBI_STRE )
		{
			/* Reached the end of the table */
			pSearch = NULL;
			break;
		}
		
		if ( ( pSearch->Len >= MatchLen )               &&
		     ( (!Exact) || (pSearch->Len == MatchLen+1) ) &&
		     ( pSearch->Tag != METAG_TBI_STRG )            )
		{
			/* Worth searching */
			const char *pMatch = __TBICmpStr( pStr,
			                        (const char *) pSearch->String );

			/* Exact match? */
			if ( pMatch == NULL ) break;
			
			/* Sufficient match? */
			if ( (!Exact) && ((pMatch - pStr) >= MatchLen) ) break;
		}

		/* Next entry */
		pSearch = (const TBISTR *) (((const char *) pSearch) + pSearch->Bytes);
	}

	return ( pSearch );
}

const void *__TBITransStr( const char *pStr, int Len )
{
	const TBISTR *pSearch = NULL;
	const void *pRes = NULL;

	for (;;)
	{
		/* Search onwards */
		pSearch = __TBIFindStr( pSearch, pStr, Len );
		
		/* No translation returns NULL */
		if ( pSearch == NULL ) break;

		/* Skip matching entries with no translation data */
		if ( pSearch->TransLen != METAG_TBI_STRX )
		{
			/* Calculate base of translation string */
			pRes = ((const char *) pSearch->String) + ((pSearch->Len + 7) & (~7));
			break;
		}
		
		/* Next entry */
		pSearch = (const TBISTR *) (((const char *) pSearch) + pSearch->Bytes);
	}

	/* Return base address of translation data or NULL */
	return ( pRes );
}

/* End of tbistring.c */
