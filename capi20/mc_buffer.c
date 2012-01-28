/*
 * mc_buffer.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mc_buffer.h"

#ifdef MI_MCBUFFER_DEBUG
#include "m_capi.h"
#define MI_MCBUFFER_DEBUG_BACKLOG	100	/* number of buffers before reuse */
static pthread_mutex_t mcb_lock;

static struct mc_buf *mcb_lost_start = NULL;
static struct mc_buf *mcb_lost_last = NULL;
static struct mc_buf *mcb_free_start = NULL;
static struct mc_buf *mcb_free_last = NULL;
static int mcb_alloc_count = 0;
static int mcb_free_count = 0;
static int *crash = NULL;

void mc_buffer_init(void)
{
	pthread_mutex_init(&mcb_lock, NULL);
	iprint("Setup mc buffer MI_MCBUFFER_DEBUG\n");
}

void mc_buffer_cleanup(void)
{
	struct mc_buf *mc;
	const char   *deb;
	struct mISDNhead *hh;

	iprint("Clean up mc buffers %d (min backlog %d) buffers alloc %d lost %d\n",
		mcb_free_count, MI_MCBUFFER_DEBUG_BACKLOG, mcb_alloc_count,
		mcb_alloc_count - mcb_free_count);
	pthread_mutex_lock(&mcb_lock);
	mI_debug_mask |= MIDEBUG_CAPIMSG;
	while (mcb_lost_start) {
		mc = mcb_lost_start;
		mcb_lost_start = mc->next;
		eprint("Buffer %p state %x len:%d allocated at %s:%d not freed\n",
			mc, mc->state, mc->len, mc->filename, mc->line);
		hh = (struct mISDNhead *)mc->rb;
		deb = mi_msg_type2str(hh->prim);
		if (deb)
			eprint("Buffer: prim %s (%x) pid = %x\n", deb, hh->prim, hh->id);
		if (mc->cmsg.Command != 0) /* it may crash if Command is undefined */
			mCapi_cmsg2str(mc);
		if (mc->l3m) {
			deb = mi_msg_type2str(mc->l3m->type);
			eprint("l3m: prim %s pid: %x\n", deb, mc->l3m->pid);
			free_l3_msg(mc->l3m);
		}
		free(mc);
		mcb_alloc_count--;
	}
	while (mcb_free_start) {
		mc = mcb_free_start;
		mcb_free_start = mc->next;
		free(mc);
		mcb_free_count--;
	}
	mcb_free_last = NULL;
	pthread_mutex_unlock(&mcb_lock);
	iprint("Clean up mc buffers finished count %d\n", mcb_free_count);
}

struct mc_buf *__alloc_mc_buf(const char *file, int lineno, const char *func)
{
	struct mc_buf *mc;

	pthread_mutex_lock(&mcb_lock);
	if (mcb_free_count > MI_MCBUFFER_DEBUG_BACKLOG) {
		mc = mcb_free_start;
		mcb_free_start = mc->next;
		mcb_free_count--;
		memset(mc, 0, sizeof(*mc));
		mc->state = MSt_reused;
	} else {
#ifdef MEMLEAK_DEBUG
		mc = __mi_calloc(1, sizeof(struct mc_buf), __FILE__, __LINE__, __PRETTY_FUNCTION__);
#else
		mc = calloc(1, sizeof(struct mc_buf));
#endif
		mc->state = MSt_fresh;
		mcb_alloc_count++;
	}
	strncpy(mc->filename, file, 79);
	mc->line = lineno;
	if (mcb_lost_last)
		mcb_lost_last->next = mc;
	mc->prev = mcb_lost_last;
	mc->next = NULL;
	mcb_lost_last = mc;
	if (!mcb_lost_start)
		mcb_lost_start = mc;
	pthread_mutex_unlock(&mcb_lock);
	return mc;
}

void __free_mc_buf(struct mc_buf *mc, const char *file, int lineno, const char *func)
{
	/* Best we can do on free error is crash (dump core) to analyse via debugger */
	if (!mc)
		*crash = 99; /* crash */
	else if (mc->state == MSt_free) /* double free */
		*crash = 100; /* crash */
	else if (mc->state == Mst_NoAlloc) /* free a not allocated buffer */
		*crash = 101; /* crash */
	if (mc->l3m) {
#ifdef MEMLEAK_DEBUG
		__free_l3_msg(mc->l3m, file, lineno, func);
#else
		free_l3_msg(mc->l3m);
#endif
		mc->l3m = NULL;
	}
	strncpy(mc->filename, file, 79);
	mc->line = lineno;
	mc->state = MSt_free;
	pthread_mutex_lock(&mcb_lock);
	if (mc->prev)
		mc->prev->next = mc->next;
	if (mc->next)
		mc->next->prev = mc->prev;
	if (mcb_lost_last == mc)
		mcb_lost_last = mc->prev;
	if (mcb_lost_start == mc)
		mcb_lost_start = mc->next;

	if (mcb_free_last)
		mcb_free_last->next = mc;
	mc->prev = mcb_free_last;
	mc->next = NULL;
	mcb_free_last = mc;
	if (!mcb_free_start)
		mcb_free_start = mc;
	mcb_free_count++;
	pthread_mutex_unlock(&mcb_lock);
}

#else

void mc_buffer_init(void)
{
}

void mc_buffer_cleanup(void)
{
}

#ifdef MEMLEAK_DEBUG
/*
 * free the message
 */
void __free_mc_buf(struct mc_buf *mc, const char *file, int lineno, const char *func)
{
	if (mc->l3m)
		__free_l3_msg(mc->l3m, file, lineno, func);
	__mi_free(mc, file, lineno, func);
}

#else
/*
 * free the message
 */
void free_mc_buf(struct mc_buf *mc)
{
	if (mc->l3m)
		free_l3_msg(mc->l3m);
	free(mc);
}
#endif
#endif

void mc_clear_cmsg(struct mc_buf *mc)
{
	memset(&mc->cmsg, 0, sizeof(mc->cmsg));
}
