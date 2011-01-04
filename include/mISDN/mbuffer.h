/* mbuffer.h
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
              
#ifndef _MBUFFER_H
#define _MBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <mISDN/mISDNif.h>
#include <mISDN/mlayer3.h>

struct mqueue {
	struct mbuffer	*prev;
	struct mbuffer	*next;
	pthread_mutex_t	lock;
	int		len;
};

struct mbuffer {
	struct mbuffer		*prev;
	struct mbuffer		*next;
	struct mqueue		*list;
	int			refcnt;
	struct mISDNhead	*h;
	struct sockaddr_mISDN	addr;
	unsigned char		*head;
	unsigned char		*data;
	unsigned char		*tail;
	unsigned char		*end;
	unsigned char		*chead;
	unsigned char		*ctail;
	unsigned char		*cend;
	int			len;
	struct l3_head		l3h;
	struct l3_msg		l3;
} __attribute__((__may_alias__));

#define	MBUFFER_DATA_SIZE	280

/*
 * init mbuffer caching
 * @parameter count of cached mbuffers
 */
extern void		init_mbuffer(int);

/*
 * free the cache
 */
extern void		cleanup_mbuffer(void);

/*
 * alloc a new mbuffer
 */

extern struct mbuffer	*alloc_mbuffer(void);

/*
 * free the message 
 */
extern void		free_mbuffer(struct mbuffer *);

static inline void
mqueue_init(struct mqueue *q)
{
	pthread_mutex_init(&q->lock, NULL);
	q->len = 0;
	q->prev = (struct mbuffer *)q;
	q->next = (struct mbuffer *)q;
}

static inline int mqueue_len(struct mqueue *q)
{
	return(q->len);
}

static inline void mqueue_head(struct mqueue *q, struct mbuffer *newm)
{
	pthread_mutex_lock(&q->lock);
	newm->list = q;
	q->len++;
	q->next->prev = newm;
	newm->next = q->next;
	newm->prev = (struct mbuffer *)q;
	q->next = newm;
	pthread_mutex_unlock(&q->lock);
}


static inline void mqueue_tail(struct mqueue *q, struct mbuffer *newm)
{
	pthread_mutex_lock(&q->lock);
	newm->list = q;
	q->len++;
	q->prev->next = newm;
	newm->next = (struct mbuffer *)q;;
	newm->prev = q->prev;
	q->prev = newm;
	pthread_mutex_unlock(&q->lock);
}


static inline struct mbuffer *mdequeue(struct mqueue *q)
{
	struct mbuffer *next, *prev, *result;

	pthread_mutex_lock(&q->lock);
	prev = (struct mbuffer *)q;
	next = prev->next;
	result = NULL;
	if (next != prev) {
		result = next;
		next = next->next;
		q->len--;
		next->prev = prev;
		prev->next = next;
		result->list = NULL;
	}
	pthread_mutex_unlock(&q->lock);
	return result;
}

static __inline__ void mqueue_purge(struct mqueue *q)
{
	struct mbuffer *mb;

	while ((mb = mdequeue(q))!=NULL)
		free_mbuffer(mb);
}

static __inline__ unsigned char *msg_put(struct mbuffer *msg, unsigned int len)
{
	unsigned char *tmp = msg->tail;

	msg->tail += len;
	msg->len += len;
	if (msg->tail > msg->end) {
		fprintf(stderr, "msg_over_panic msg(%p) data(%p) head(%p)\n",
			msg, msg->data, msg->head);
		return NULL;
	}
	return tmp;
}

static __inline__ unsigned char *msg_push(struct mbuffer *msg, unsigned int len)
{
	msg->data -= len;
	msg->len += len;
	if(msg->data < msg->head)
	{
		fprintf(stderr, "msg_under_panic msg(%p) data(%p) head(%p)\n",
			msg, msg->data, msg->head);
		return NULL;
	}
	return msg->data;
}


static __inline__ unsigned char *__msg_pull(struct mbuffer *msg, unsigned int len)
{
	unsigned char *tmp = msg->data;

	msg->len -= len;
	msg->data += len;
	return tmp;
}

static __inline__ unsigned char * msg_pull(struct mbuffer *msg, unsigned int len)
{
	if (len > (unsigned int)msg->len)
		return NULL;
	return (unsigned char *)__msg_pull(msg,len);
}

static __inline__ int msg_headroom(struct mbuffer *msg)
{
	return msg->data - msg->head;
}

static __inline__ int msg_tailroom(struct mbuffer *msg)
{
	return msg->end-msg->tail;
}

static __inline__ void msg_reserve(struct mbuffer *msg, unsigned int len)
{
	msg->data += len;
	msg->tail += len;
}

static __inline__ void __msg_trim(struct mbuffer *msg, unsigned int len)
{
	msg->len = len;
	msg->tail = msg->data + len;
}

static __inline__ void msg_trim(struct mbuffer *msg, unsigned int len)
{
	if ((unsigned int)msg->len > len) {
		__msg_trim(msg, len);
	}
}

#ifdef __cplusplus
}
#endif

#endif
