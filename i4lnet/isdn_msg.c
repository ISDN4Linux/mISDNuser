#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isdn_msg.h"
#include "isdn_debug.h"

static	msg_queue_t	_free_queue;
msg_queue_t		*free_queue;

void
msg_init(void)
{
	free_queue = & _free_queue;
	msg_queue_init(free_queue);
	free_queue->maxlen = 200;
}


static  int alloc_msg_cnt = 0;

msg_t *
_new_msg(int size)
{
	msg_t *m;

	if (size <= MAX_MSG_SIZE)
		size = MAX_MSG_SIZE;
	else
		goto err;
	m = malloc(sizeof(msg_t));
	if (!m)
		goto err;
	m->size = size;
	alloc_msg_cnt++;
	return(m);
err:
	eprint("%s: no mem for size %d msg\n", __FUNCTION__,
		size);
	return(NULL);
}

msg_t *
alloc_msg(int size)
{
	msg_t *m;
	
	if (size > MAX_MSG_SIZE)
		return(NULL);
	if (msg_queue_len(free_queue))
		m = msg_dequeue(free_queue);
	else
		m = _new_msg(size);
	if (!m) {
		eprint("%s: no mem for msg len (%d)\n", __FUNCTION__,
			size);
		return(NULL);
	}
	m->list = NULL;
	m->prev = NULL;
	m->next = NULL;
	m->head = &m->__data[0];
	m->data = m->head + DEFAULT_HEADROOM;
	m->tail = m->data;
	m->end  = m->head + m->size;
	m->len = 0;
	dprint(DBGM_MSG, -1,"%s: %d msg(%p)\n", __FUNCTION__,
		alloc_msg_cnt, m);
	return(m);
}


void
free_msg(msg_t *msg) {
	if (!msg) {
		wprint("free NULL msg\n");
		return;
	}
	dprint(DBGM_MSG, -1,"%s: %d/%d msg(%p) \n", __FUNCTION__,
		alloc_msg_cnt, free_queue->len, msg);

	if (msg->list) {
		if  (msg->list == free_queue)
			wprint("%s: free twice msg(%p)\n", __FUNCTION__,
				msg);
		else
			wprint("%s: msg(%p) in queue(%p)\n", __FUNCTION__,
				msg, msg->list);
		return;
	}
	if (free_queue->len>=free_queue->maxlen) {
		alloc_msg_cnt--;
		dprint(DBGM_MSG, -1, "free msg no free_queue %d/%d\n",
			free_queue->len, free_queue->maxlen);
		free(msg);
		return;
	}
	msg_queue_head(free_queue, msg);
}

msg_t *
msg_copy(msg_t *msg) {
	msg_t	*nmsg;

	dprint(DBGM_MSG, -1,"%s: old(%p)\n", __FUNCTION__, msg);
	nmsg = alloc_msg(msg->size);
	if (!nmsg)
		return(NULL);
	dprint(DBGM_MSG, -1,"%s: new(%p) size(%d)\n", __FUNCTION__,
		nmsg, msg->size);
	memcpy(nmsg, msg, sizeof(msg_t));
	return(nmsg);
}
