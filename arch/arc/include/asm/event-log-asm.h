/*
 *  Low level Event Capture API callable from Assembly Code
 *  vineetg: Feb 2008
 *
 *  TBD: SMP Safe
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_EVENT_LOG_ASM_H
#define __ASM_ARC_EVENT_LOG_ASM_H

#include <asm/event-log.h>

#ifdef __ASSEMBLY__

#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

.macro TAKE_SNAP_ASM reg_scratch, reg_ptr, type
.endm

.macro TAKE_SNAP_C_FROM_ASM type
.endm

#else

#include <asm/asm-offsets.h>

/*
 * Macro to invoke the "C" event logger routine from assmebly code
 */
.macro TAKE_SNAP_C_FROM_ASM type
	mov r0, \type
	mov r1, sp
	bl take_snap3
.endm

#endif	/* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif	/* __ASSEMBLY__ */

#endif
