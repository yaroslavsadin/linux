/*
 * machine.inc
 *
 * Copyright (C) 2000, 2001, 2002, 2005, 2007, 2012 Imagination Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * Global assembler header for defining target specifics
 *
 *            defines : See metag.inc for details
 */

#if defined(METAG) || defined(METAC_1_2) || defined(METAC_2_1)
#include "metag.inc"
#endif

/*
 * Customers may make additions to this file to describe additional hardware
 * added around or inside the instance of the META Core concerned
 */

/* End of machine.inc */
