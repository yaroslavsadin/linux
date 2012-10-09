/*
 * ppp-comp.h - Definitions for doing PPP packet compression.
 *
 * Copyright 1994-1998 Paul Mackerras.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 */
#ifndef _NET_PPP_COMP_H
#define _NET_PPP_COMP_H

#include <uapi/linux/ppp-comp.h>

extern int ppp_register_compressor(struct compressor *);
extern void ppp_unregister_compressor(struct compressor *);
#endif /* _NET_PPP_COMP_H */
