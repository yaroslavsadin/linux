/*
 * compasm.inc
 *
 * Copyright (C) 2000, 2001, 2002, 2005, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * Sets up the varies defines depending on compiler/assembler.
 *
 *              __GNUC__ - implies inclusion in GNU C compile
 *         __ASSEMBLER__ - implies inclusion in assembly not C compile
 *
 *              __ANYC__ - implies inclusion in C compile not assembly
 *    __ASSEMBLER_META__ - implies assembly using META assembler
 *     __ASSEMBLER_GNU__ - implies assembly using GNU assembler
 */

#ifndef _COMPASM_INC_
#define _COMPASM_INC_

#ifdef __GNUC__
#ifndef __ANYC__
/* Define __ANYC__ if being compiled by GNU C */
#define __ANYC__
#endif
#endif /* ifdef __GNUC__ */

#ifndef __ANYC__
#ifdef __STDC__
#define __ANYC__ 1
#endif
#endif /* ifndef __ANYC__*/ 

#ifndef __ANYC__
#ifdef _WIN32
#define __ANYC__ 1
#endif
#endif /* ifndef __ANYC__*/ 

/* Determine if we are assembling/pre-processing assembler
   sources as opposed to compiling/pre-processing C/C++  */

/* If we are assembling/pre-processing assembly sources
   on metag-linux either __ASSEMBLY__ and/or __ASSEMBLER__
   will be defined.  */

#if defined (__ASSEMBLY__) || defined (__ASSEMBLER__)

/* Ensure that __ASSEMBLER__ is defined and that __ANYC__ is
   undefined so that we can correctly determine the appropriate
   assembler. */
#ifndef __ASSEMBLER__
#define __ASSEMBLER__
#endif
#undef __ANYC__
#endif /* defined (__ASSEMBLY__) || defined (__ASSEMBLER__) */

/* __ANYC__ should now be defined if we are compiling and
   undefined if we are assembling.  */

/* If we are not compiling/pre-processing C/C++ sources
   determine which assembler is being used.  */

#ifndef __ANYC__
#ifndef __ASSEMBLER__
#define __ASSEMBLER__		/* Indicate we are assembling */
#define __ASSEMBLER__META__	/* Assembling with Meta as */
#else
#define __ASSEMBLER_GNU__	/* Assembling with GNU as */
#endif
#endif /* ifndef __ANYC__ */

/* __ASSEMBLER__ should now be defined if we are assembling. */

#ifdef __ASSEMBLER__
/* We're assembling so undefine __ANYC__ */
#undef __ANYC__
#endif /* __ASSEMBLER__ */

#endif /* ifndef _COMPASM_INC_ */

/* End of compasm.inc */
