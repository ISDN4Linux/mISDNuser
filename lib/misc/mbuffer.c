/* mbuffer.c
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
 * Copyright 2010  by Karsten Keil <kkeil@linux-pingi.de>
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
#include <mISDN/mbuffer.h>
#include "helper.h"
#include "debug.h"

static	struct mqueue	free_queue_l2, free_queue_l3;
static	int	Max_Cache;
#define	MIN_CACHE	4

#define MB_TYP_L2	2
#define MB_TYP_L3	3

#ifdef MEMLEAK_DEBUG
static struct Aqueue {
	struct lhead	Alist;
	pthread_mutex_t	lock;
	int		len;
} AllocQueue;

static inline void
Aqueue_init(struct Aqueue *q)
{
	pthread_mutex_init(&q->lock, NULL);
	q->len = 0;
	q->Alist.prev = &q->Alist;
	q->Alist.next = &q->Alist;
}

static inline int Amqueue_len(struct Aqueue *q)
{
	return q->len ;
}

static inline void __list_add(struct lhead *new, struct lhead *prev, struct lhead *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void __list_del(struct lhead * prev, struct lhead * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void Aqueue_head(struct Aqueue *q, struct mbuffer *newm)
{
	pthread_mutex_lock(&q->lock);
	q->len++;
	__list_add(&newm->Alist, &q->Alist, q->Alist.next);
	pthread_mutex_unlock(&q->lock);
}


static inline void Aqueue_tail(struct Aqueue *q, struct mbuffer *newm)
{
	pthread_mutex_lock(&q->lock);
	q->len++;
	__list_add(&newm->Alist, q->Alist.prev, &q->Alist);
	pthread_mutex_unlock(&q->lock);
}


static inline struct mbuffer *Adequeue(struct Aqueue *q)
{
	struct mbuffer *m;
	struct lhead *le;

	pthread_mutex_lock(&q->lock);
	if (q->len) {
		le = q->Alist.next;
		__list_del(le->prev, le->next);
		q->len--;
		m = container_of(le, struct mbuffer, Alist);
	} else
		m = NULL;
	pthread_mutex_unlock(&q->lock);
	return m;
}
static void Aqueue_remove(struct Aqueue *q, struct mbuffer *mb)
{
	pthread_mutex_lock(&q->lock);
	__list_del(mb->Alist.prev, mb->Alist.next);
	q->len--;
	pthread_mutex_unlock(&q->lock);
}

#endif

void
init_mbuffer(int max_cache)
{
	mqueue_init(&free_queue_l2);
	mqueue_init(&free_queue_l3);
#ifdef MEMLEAK_DEBUG
	Aqueue_init(&AllocQueue);
#endif
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

#ifdef MEMLEAK_DEBUG

static struct mbuffer *
_new_mbuffer(int typ, const char *file, int lineno, const char *func)
{
	struct mbuffer	*m;

	m = __mi_calloc(1, sizeof(struct mbuffer), file, lineno, func);
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

static struct mbuffer *
_alloc_mbuffer(int typ, const char *file, int lineno, const char *func)
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
		m = _new_mbuffer(typ, file, lineno, func);
	else
		__mi_reuse(m, file, lineno, func);
	strncpy(m->d_fn, file, 79);
	m->d_ln = lineno;
	Aqueue_tail(&AllocQueue, m);
	return m;
}

struct mbuffer *
__alloc_mbuffer(const char *file, int lineno, const char *func)
{
	return _alloc_mbuffer(MB_TYP_L2, file, lineno, func);
}


void
__free_mbuffer(struct mbuffer *m, const char *file, int lineno, const char *func) {
	if (!m)
		return;
	if (m->refcnt) {
		m->refcnt--;
		return;
	}
	Aqueue_remove(&AllocQueue, m);
	strncpy(m->d_fn, file, 79);
	m->d_ln = -lineno;
	if (m->list) {
		if (m->list == &free_queue_l3)
			dprint(DBGM_L3BUFFER, "%s l3 buffer %p already freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
		else if (m->list == &free_queue_l2)
			dprint(DBGM_L3BUFFER, "%s l2 buffer %p already freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
		else
			dprint(DBGM_L3BUFFER, "%s buffer %p still in list %p : %lx\n", __FUNCTION__, m, m->list, (unsigned long)__builtin_return_address(0));
		return;
	} else
		dprint(DBGM_L3BUFFER, "%s buffer %p freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
	if (m->chead) {
		if (free_queue_l3.len > Max_Cache) {
			free(m->chead);
			free(m->head);
			__mi_free(m, file, lineno, func);
		} else {
			memset(&m->l3, 0, sizeof(m->l3));
			memset(&m->l3h, 0, sizeof(m->l3h));
			m->data = m->tail = m->head;
			m->len = 0;
			m->ctail = m->chead;
			__mi_reuse(m, file, lineno, "IN_FREE_QUEUE");
			mqueue_head(&free_queue_l3, m);
		}
	} else {
		if (free_queue_l2.len > Max_Cache) {
			free(m->head);
			__mi_free(m, file, lineno, func);
		} else {
			memset(&m->l3, 0, sizeof(m->l3));
			memset(&m->l3h, 0, sizeof(m->l3h));
			m->data = m->tail = m->head;
			m->len = 0;
			__mi_reuse(m, file, lineno, "IN_FREE_QUEUE");
			mqueue_head(&free_queue_l2, m);
		}
	}
}

struct l3_msg	*
__alloc_l3_msg(const char *file, int lineno, const char *func)
{
	struct mbuffer	*m;

	m = _alloc_mbuffer(MB_TYP_L3, file, lineno, func);
	if (m)
		return &m->l3;
	else
		return NULL;
}

void
__free_l3_msg(struct l3_msg *l3m, const char *file, int lineno, const char *func)
{
	struct mbuffer	*m;

	if (!l3m)
		return;
	m = container_of(l3m, struct mbuffer, l3);
	__free_mbuffer(m, file, lineno, func);
}

void
cleanup_mbuffer(void)
{
	struct mbuffer *m;
	int ql;

	__mqueue_purge(&free_queue_l2);
	__mqueue_purge(&free_queue_l3);
	ql = Amqueue_len(&AllocQueue);
	iprint("AllocQueue has %d lost mbuffer\n", ql);
	if (ql) {
		m = Adequeue(&AllocQueue);
		while (m) {
			wprint("Lost mbuffer allocated %s:%d typ=%s len=%d\n",
				m->d_fn, m->d_ln, m->chead ? "L3" : "L2", m->len);
			wprint("             H: prim=%s id=%d L3: prim=%s pid=%d\n",
				_mi_msg_type2str(m->h->prim), m->h->id, _mi_msg_type2str(m->l3.type), m->l3.pid);
			free_mbuffer(m);
			m = Adequeue(&AllocQueue);
		}
	}
}

#else

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

static struct mbuffer *
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
			dprint(DBGM_L3BUFFER, "%s l3 buffer %p already freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
		else if (m->list == &free_queue_l2)
			dprint(DBGM_L3BUFFER, "%s l2 buffer %p already freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
		else
			dprint(DBGM_L3BUFFER, "%s buffer %p still in list %p : %lx\n", __FUNCTION__, m, m->list, (unsigned long)__builtin_return_address(0));
		return;
	} else
		dprint(DBGM_L3BUFFER, "%s buffer %p freed: %lx\n",  __FUNCTION__, m, (unsigned long)__builtin_return_address(0));
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
cleanup_mbuffer(void)
{
	__mqueue_purge(&free_queue_l2);
	__mqueue_purge(&free_queue_l3);
}
#endif

void
l3_msg_increment_refcnt(struct l3_msg *l3m)
{
	struct mbuffer	*m;

	m = container_of(l3m, struct mbuffer, l3);
	m->refcnt++;
}

