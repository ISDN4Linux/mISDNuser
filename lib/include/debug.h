/* debug.h
 *
 * Basic Layer3 functions
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2007 by Karsten Keil <kkeil@novell.com>
 * Copyright 2009, 2010 by Karsten Keil <kkeil@linux-pingi.de>
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
#include <mISDN/mISDNcompat.h>

#undef MEMLEAK_DEBUG
#if MEMLEAKDEBUG_COMPILE
#define MEMLEAK_DEBUG 1
#endif

#define L3_DEB_WARN	0x00000001
#define L3_DEB_PROTERR	0x00000002
#define L3_DEB_STATE	0x00000004
#define L3_DEB_PROC	0x00000008
#define L3_DEB_CHECK	0x00000010
#define DBGM_L3		0x00000040
#define DBGM_L3DATA	0x00000080
#define DBGM_L3BUFFER	0x00000100
#define DBGM_L2		0x00000200
#define DBGM_L2_STATE	0x00000400
#define DBGM_ASN1_WARN	0x00010000
#define DBGM_ASN1_ENC	0x00020000
#define DBGM_ASN1_DEC	0x00040000
#define DBGM_ASN1_DATA	0x00080000
#define DBGM_ALL	0xffffffff

extern unsigned int	mI_debug_mask;

#define mi_thread_create(a, b, c, d) _mi_thread_create(a, b, c, d, __FILE__, __PRETTY_FUNCTION__, __LINE__, #c)
extern int _mi_thread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine) (void *),
			void *arg, const char *file, const char *caller, int line, const char *start_fn);


#define eprint(fmt, ...)	mi_printf(__FILE__, __LINE__, __PRETTY_FUNCTION__, 1, fmt, ##__VA_ARGS__)
#define wprint(fmt, ...)	mi_printf(__FILE__, __LINE__, __PRETTY_FUNCTION__, 2, fmt, ##__VA_ARGS__)
#define iprint(fmt, ...)	mi_printf(__FILE__, __LINE__, __PRETTY_FUNCTION__, 3, fmt, ##__VA_ARGS__)
#define dprint(m, fmt, ...)	do { if (m & mI_debug_mask) mi_printf(__FILE__, __LINE__, __PRETTY_FUNCTION__, 4, fmt, ##__VA_ARGS__);} while(0)

extern int mi_printf(const char *file, int line, const char *func, int lev, const char *fmt, ...) __attribute__ ((format (printf,5,6)));

#define dhexprint(m, h, d, l)	do { if (m & mI_debug_mask) mi_dhexprint(__FILE__, __LINE__, __PRETTY_FUNCTION__, h, d, l);} while(0)

extern void mi_dhexprint(const char *file, int line, const char *func, char *head, unsigned char *buf, int len);
extern void mi_shexprint(char *dest, unsigned char *buf, int len);

#ifdef MEMLEAK_DEBUG
extern void *__mi_alloc(size_t size, const char *file, int lineno, const char *func);
extern void *__mi_calloc(size_t nmemb, size_t size, const char *file, int lineno, const char *func);
extern void __mi_free(void *ptr, const char *file, int lineno, const char *func);
extern void __mi_reuse(void *ptr, const char *file, int lineno, const char *func);

#undef malloc
#undef calloc
#undef free

#define malloc(s)	__mi_alloc(s, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define calloc(n, s)	__mi_calloc(n, s, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define free(p)		__mi_free(p, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

#endif
