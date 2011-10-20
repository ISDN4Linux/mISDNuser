/*
 * mc_buffer.h
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2011  by Karsten Keil <kkeil@linux-pingi.de>
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

#ifndef _MC_BUFFER_H
#define _MC_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mISDN/mbuffer.h>
#include <capiutils.h>


#define MC_RB_SIZE		2112

struct mc_buf {
	struct l3_msg	*l3m;
	_cmsg		cmsg;
	int		len;
	unsigned char	rb[MC_RB_SIZE];
	unsigned char	*rp;
};

#ifdef MEMLEAK_DEBUG
/*
 * alloc a new mbuffer
 */

extern struct mc_buf	*__alloc_mc_buf(const char *file, int lineno, const char *func);

/*
 * free the message
 */
extern void		__free_mc_buf(struct mc_buf *, const char *file, int lineno, const char *func);

#define alloc_mc_buf()	__alloc_mc_buf(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define free_mc_buf(p)	__free_mc_buf(p, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
/*
 * alloc a new mbuffer
 */
extern struct mc_buf	*alloc_mc_buf(void);

/*
 * free the message 
 */
extern void		free_mc_buf(struct mc_buf *);
#endif


#ifdef __cplusplus
}
#endif

#endif

