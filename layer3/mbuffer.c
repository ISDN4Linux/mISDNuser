/* mbuffer.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mbuffer.h"
#include "helper.h"
#include "debug.h"

static	struct mqueue	free_queue_l2, free_queue_l3;
static	int	Max_Cache;
#define	MIN_CACHE	4

#define MB_TYP_L2	2
#define MB_TYP_L3	3

void
init_mbuffer(int max_cache)
{
	mqueue_init(&free_queue_l2);
	mqueue_init(&free_queue_l3);
	if (max_cache < MIN_CACHE)
		max_cache = MIN_CACHE;
	Max_Cache = max_cache;
}

static void
__mqueue_purge(struct mqueue *q) {
	struct mbuffer *mb;

	while ((mb = mdequeue(q))!=NULL) {
		if (mb->head)
			free(mb->head);
		if (mb->chead)
			free(mb->chead);
		free(mb);
	}
}

void
cleanup_mbuffer(void)
{
	__mqueue_purge(&free_queue_l2);
	__mqueue_purge(&free_queue_l3);
}


static struct mbuffer *
_new_mbuffer(int typ)
{
	struct mbuffer	*m;

	m = calloc(1, sizeof(struct mbuffer));
	if (!m)
		goto err;
	switch(typ) {
	case MB_TYP_L3:
		m->chead = malloc(MBUFFER_DATA_SIZE);
		if (!m->chead) {
			free(m);
			goto err;
		}
		m->cend = m->chead + MBUFFER_DATA_SIZE;
		m->ctail = m->chead;
	case MB_TYP_L2:
		m->head = malloc(MBUFFER_DATA_SIZE);
		if (!m->head) {
			if (m->chead)
				free(m->chead);
			free(m);
			goto err;
		}
		m->end = m->head + MBUFFER_DATA_SIZE;
		m->data = m->tail = m->head;
		m->h = (struct mISDNhead *) m->head;
		break;
	}
	return(m);
err:
	eprint("%s: no mem for mbuffer\n", __FUNCTION__);
	return(NULL);
}

struct mbuffer *
_alloc_mbuffer(int typ)
{
	struct mbuffer	*m;

	switch(typ) {
	case MB_TYP_L3:
		m =  mdequeue(&free_queue_l3);
		break;
	case MB_TYP_L2:
		m =  mdequeue(&free_queue_l2);
		break;
	}
	if (!m)
		m = _new_mbuffer(typ);
	return m;
}

struct mbuffer *
alloc_mbuffer(void)
{
	return _alloc_mbuffer(MB_TYP_L2);
}


void
free_mbuffer(struct mbuffer *m) {
	if (!m)
		return;
	if (m->refcnt) {
		m->refcnt--;
		return;
	}
	if (m->list) {
		if (m->list == &free_queue_l3)
			dprint(DBGM_L3BUFFER, 0,"%s l3 buffer %p already freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
		else if (m->list == &free_queue_l2)
			dprint(DBGM_L3BUFFER, 0,"%s l2 buffer %p already freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
		else
			dprint(DBGM_L3BUFFER, 0,"%s buffer %p still in list %p : %lx\n", __FUNCTION__, m, m->list, (unsigned long)__builtin_return_address(0));
		return;
	} else
		dprint(DBGM_L3BUFFER, 0,"%s buffer %p freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
	if (m->chead) {
		if (free_queue_l3.len > Max_Cache) {
			free(m->chead);
			free(m->head);
			free(m);
		} else {
			memset(&m->l3, 0, sizeof(m->l3));
			memset(&m->l3h, 0, sizeof(m->l3h));
			m->data = m->tail = m->head;
			m->len = 0;
			m->ctail = m->chead;
			mqueue_head(&free_queue_l3, m);
		}
	} else {
		if (free_queue_l2.len > Max_Cache) {
			free(m->head);
			free(m);
		} else {
			memset(&m->l3, 0, sizeof(m->l3));
			memset(&m->l3h, 0, sizeof(m->l3h));
			m->data = m->tail = m->head;
			m->len = 0;
			mqueue_head(&free_queue_l2, m);
		}
	}
}

struct l3_msg	*
alloc_l3_msg(void)
{
	struct mbuffer	*m;

	m = _alloc_mbuffer(MB_TYP_L3);
	if (m)
		return &m->l3;
	else
		return NULL;
}

void
free_l3_msg(struct l3_msg *l3m)
{
	struct mbuffer	*m;

	if (!l3m)
		return;
	m = container_of(l3m, struct mbuffer, l3);
	free_mbuffer(m);
}

void
l3_msg_increment_refcnt(struct l3_msg *l3m)
{
	struct mbuffer	*m;

	m = container_of(l3m, struct mbuffer, l3);
	m->refcnt++;
}
