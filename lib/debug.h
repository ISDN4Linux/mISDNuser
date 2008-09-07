/* debug.h
 * 
 * Basic Layer3 functions
 *
 * Author       Karsten Keil <kkeil@novell.com>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE
 * version 2.1 as published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU LESSER GENERAL PUBLIC LICENSE for more details.
 *
 */
#ifndef ISDN_DEBUG_H
#define ISDN_DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define L3_DEB_WARN	0x00000001
#define L3_DEB_PROTERR	0x00000002
#define L3_DEB_STATE	0x00000004
#define L3_DEB_PROC	0x00000008
#define L3_DEB_CHECK	0x00000010
#define DBGM_L3		0x00000040
#define DBGM_L3DATA	0x00000080
#define DBGM_L3BUFFER	0x00000100
#define DBGM_ALL	0xffffffff

extern	int		dprint(unsigned int mask, int port, const char *fmt, ...);
extern	int		iprint(const char *fmt, ...);
extern	int		eprint(const char *fmt, ...);
extern	int		wprint(const char *fmt, ...);
extern	int		dhexprint(unsigned int, char *, unsigned char *, int);
#endif
