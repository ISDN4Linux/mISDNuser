/* debug.c
 *
 * mISDN lib debug functions
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
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "debug.h"
#include <mISDN/mlayer3.h>

unsigned int		mI_debug_mask;


#ifdef MEMLEAK_DEBUG

#undef malloc
#undef calloc
#undef free

void *
__mi_alloc(size_t size, const char *file, int lineno, const char *func)
{
	void *p;

	if (mi_extern_func && mi_extern_func->malloc)
		p = mi_extern_func->malloc(size, file, lineno, func);
	else
		p = malloc(size);
	return p;
}

void *
__mi_calloc(size_t nmemb, size_t size, const char *file, int lineno, const char *func)
{
	void *p;

	if (mi_extern_func && mi_extern_func->calloc)
		p = mi_extern_func->calloc(nmemb, size, file, lineno, func);
	else
		p = calloc(nmemb, size);
	return p;
}

void
__mi_free(void *ptr, const char *file, int lineno, const char *func)
{
	if (mi_extern_func && mi_extern_func->free)
		mi_extern_func->free(ptr, file, lineno, func);
	else
		free(ptr);
}

void __mi_reuse(void *ptr, const char *file, int lineno, const char *func)
{
	if (mi_extern_func && mi_extern_func->reuse)
		mi_extern_func->reuse(ptr, file, lineno, func);
}

#endif



void
mISDN_set_debug_level(unsigned int mask)
{
	mI_debug_mask = mask;
}

int
mi_printf(const char *file, int line, const char *func, int lev, const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	if (mi_extern_func && mi_extern_func->prt_debug) {
		ret = mi_extern_func->prt_debug(file, line, func, lev, fmt, args);
	} else {
		FILE *f = stderr;

		if (lev == MISDN_LIBDEBUG_DEBUG || lev == MISDN_LIBDEBUG_INFO)
			f = stdout;
		ret = vfprintf(f, fmt, args);
		fflush(f);
	}
	va_end(args);
	return ret;
}

int
_mi_thread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine) (void *),
			void *arg, const char *file, const char *caller, int line, const char *start_fn)
{
	int ret;

	if (mi_extern_func && mi_extern_func->thread_create)
		ret = mi_extern_func->thread_create(thread, attr, start_routine, arg, file, caller, line, start_fn);
	else
		ret = pthread_create(thread, attr, start_routine, arg);
	return ret;
}

void
mi_dhexprint(const char *file, int line, const char *func, char *head, unsigned char *buf, int len)
{
	int	ret = 0,i;
	char	*p,*obuf;


	obuf = malloc(100);
	if (!obuf)
		return;
	p = obuf;
	*p = 0;
	for (i = 1; i <= len; i++) {
		p += sprintf(p, "%02x ", *buf);
		buf++;
		if (!(i % 32)) {
			p--;
			*p = 0;
			mi_printf(file, line, func, MISDN_LIBDEBUG_DEBUG, "%s %s\n", head, obuf);
			p = obuf;
			*p = 0;
		}
	}
	if (*obuf) {
		p--;
		*p = 0;
		mi_printf(file, line, func, MISDN_LIBDEBUG_DEBUG, "%s %s\n", head, obuf);
	}
	free(obuf);
}

void
mi_shexprint(char *dest, unsigned char *buf, int len)
{
	char *p = dest;

	while (len) {
		p += sprintf(p, "%02x ", *buf);
		buf++;
		len--;
	}
	p--;
	*p = 0;
}
