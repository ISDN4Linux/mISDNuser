/* debug.c
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
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "debug.h"


static unsigned int	debug_mask = 0;
static FILE		*debug_file = NULL;
static FILE		*warn_file = NULL;
static FILE		*error_file = NULL;

int
mISDN_debug_init(unsigned int mask, char *dfile, char *wfile, char *efile)
{
	if (dfile) {
		if (debug_file && (debug_file != stderr))
			debug_file = freopen(dfile, "a", debug_file);
		else
			debug_file = fopen(dfile, "a");
		if (!debug_file) {
			debug_file = stderr;
			fprintf(debug_file,
				"%s: cannot open %s for debug log, using stderr\n",
				__FUNCTION__, dfile);
		}
	} else {
		if (!debug_file) {
			debug_file = stderr;
		}
	}
	if (wfile) {
		if (warn_file && (warn_file != stderr))
			warn_file = freopen(wfile, "a", warn_file);
		else
			warn_file = fopen(wfile, "a");
		if (!warn_file) {
			warn_file = stderr;
			fprintf(warn_file,
				"%s: cannot open %s for warning log, using stderr\n",
				__FUNCTION__, wfile);
		}
	} else {
		if (!warn_file) {
			warn_file = stderr;
		}
	}
	if (efile) {
		if (error_file && (error_file != stderr))
			error_file = freopen(efile, "a", error_file);
		else
			error_file = fopen(efile, "a");
		if (!error_file) {
			error_file = stderr;
			fprintf(error_file,
				"%s: cannot open %s for error log, using stderr\n",
				__FUNCTION__, efile);
		}
	} else {
		if (!error_file) {
			error_file = stderr;
		}
	}
	debug_mask = mask;
	return 0;
}

void
mISDN_debug_close(void)
{
	if (debug_file && (debug_file != stderr))
		fclose(debug_file);
	if (warn_file && (warn_file != stderr))
		fclose(warn_file);
	if (error_file && (error_file != stderr))
		fclose(error_file);
}

int
dprint(unsigned int mask, int port, const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;
	time_t tm = time(NULL);
	char *tmp=ctime(&tm),*p;
	
	p=strchr(tmp,'\n');
	if (p) *p=':';

	va_start(args, fmt);
	if (debug_mask & mask) {
		if (debug_file != stderr)
			fprintf(debug_file, "%s P(%02d): L(0x%02x):",tmp, port,mask);
		ret = vfprintf(debug_file, fmt, args);
		if (debug_file != stderr)
			fflush(debug_file);
	}
	va_end(args);
	return ret;
}

int
iprint(const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	if (debug_file != stderr) {
		ret = vfprintf(debug_file, fmt, args);
		fflush(debug_file);
	}
	va_end(args);
	return(ret);
}


int
wprint(const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	ret = vfprintf(warn_file, fmt, args);
	fflush(warn_file);
	va_end(args);
	return(ret);
}


int
eprint(const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	ret = vfprintf(error_file, fmt, args);
	fflush(error_file);
	va_end(args);
	return(ret);
}

int
dhexprint(unsigned int mask, char *head, unsigned char *buf, int len)
{
	int	ret = 0;
	char	*p,*obuf;

	if (debug_mask & mask) {
		obuf = malloc(3*(len+1));
		if (!obuf)
			return(-ENOMEM);
		p = obuf;
		while (len) {
			p += sprintf(p,"%02x ", *buf);
			buf++;
			len--;
		}
		p--;
		*p=0;
		ret = fprintf(debug_file, "%s %s\n", head, obuf);
		free(obuf);
	}
	return(ret);
}
