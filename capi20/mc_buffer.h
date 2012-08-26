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

#define MI_MCBUFFER_DEBUG	1
#ifdef __cplusplus
extern "C" {
#endif

#include <mISDN/mbuffer.h>
#include <capiutils.h>


#define MC_RB_SIZE		2112

#ifdef MI_MCBUFFER_DEBUG
enum mstate {
	Mst_NoAlloc	= 0,
	MSt_fresh	= 0x5555aaaa,
	MSt_free	= 0x42424242,
	MSt_reused	= 0x55aa55aa
};
#endif

struct mc_buf {
	struct l3_msg	*l3m;
	_cmsg		cmsg;
	int		len;
	unsigned char	rb[MC_RB_SIZE];
	unsigned char	*rp;
#ifdef MI_MCBUFFER_DEBUG
	enum mstate	state;
	char		filename[80];
	int		line;
	struct mc_buf	*next;
	struct mc_buf	*prev;
#endif
};

extern void mc_buffer_init(void);
extern void mc_buffer_cleanup(void);
extern void mc_buffer_dump_status(void);

#ifdef MI_MCBUFFER_DEBUG
extern void __free_mc_buf(struct mc_buf *, const char *file, int lineno, const char *func);

#define free_mc_buf(p)  __free_mc_buf(p, __FILE__, __LINE__, __PRETTY_FUNCTION__)

extern struct mc_buf	*__alloc_mc_buf(const char *file, int lineno, const char *func);
#define alloc_mc_buf()	__alloc_mc_buf( __FILE__, __LINE__, __PRETTY_FUNCTION__)

#else
#ifdef MEMLEAK_DEBUG
/*
 * alloc a new mbuffer
 */

#define	alloc_mc_buf()	__mi_calloc(1, sizeof(struct mc_buf), __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*
 * free the message
 */
extern void		__free_mc_buf(struct mc_buf *, const char *file, int lineno, const char *func);

#define free_mc_buf(p)	__free_mc_buf(p, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
/*
 * alloc a new mbuffer
 */
#define alloc_mc_buf()	calloc(1, sizeof(struct mc_buf))
/*
 * free the message 
 */
extern void		free_mc_buf(struct mc_buf *);
#endif
#endif

#define	mcbuf_rb2cmsg(m)	capi_message2cmsg(&(m)->cmsg, (m)->rb)

extern void			mc_clear_cmsg(struct mc_buf *);

#ifdef __cplusplus
}
#endif

#endif

