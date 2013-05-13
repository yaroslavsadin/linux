/*
 * Copyright (C) 2014 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_IRQFLAGS_H
#define __ASM_ARC_IRQFLAGS_H

#ifdef CONFIG_ISA_ARCV2
#include <asm/irqflags-arcv2.h>
#else
#include <asm/irqflags-arcompact.h>
#endif

#endif
