/* layer3.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <q931.h>
#include "layer3.h"

static
struct Fsm l3fsm = {NULL, 0, 0, NULL, NULL};

enum {
	ST_L3_LC_REL,
	ST_L3_LC_ESTAB_WAIT,
	ST_L3_LC_REL_DELAY,
	ST_L3_LC_REL_WAIT,
	ST_L3_LC_ESTAB,
};

#define L3_STATE_COUNT (ST_L3_LC_ESTAB + 1)

static char *strL3State[] =
{
	"ST_L3_LC_REL",
	"ST_L3_LC_ESTAB_WAIT",
	"ST_L3_LC_REL_DELAY",
	"ST_L3_LC_REL_WAIT",
	"ST_L3_LC_ESTAB",
};

enum {
	EV_ESTABLISH_REQ,
	EV_ESTABLISH_IND,
	EV_ESTABLISH_CNF,
	EV_RELEASE_REQ,
	EV_RELEASE_CNF,
	EV_RELEASE_IND,
	EV_TIMEOUT,
};

#define L3_EVENT_COUNT (EV_TIMEOUT+1)

static char *strL3Event[] =
{
	"EV_ESTABLISH_REQ",
	"EV_ESTABLISH_IND",
	"EV_ESTABLISH_CNF",
	"EV_RELEASE_REQ",
	"EV_RELEASE_CNF",
	"EV_RELEASE_IND",
	"EV_TIMEOUT",
};

l3_process_t *
get_l3process4pid(layer3_t *l3, u_int pid)
{
	l3_process_t	*p;

	if ((pid & 0xffff7fff) == 0) /* global CR */
		return &l3->global;
	if (pid == MISDN_PID_GLOBAL)
		return &l3->global;
	if (pid == MISDN_PID_DUMMY)
		return &l3->dummy;
	list_for_each_entry(p, &l3->plist, list)
		if (p->pid == pid)
			return p;
	return NULL;
}

l3_process_t *
get_l3process4cref(layer3_t *l3, u_int cr)
{
	l3_process_t	*p;

	if ((cr & 0x00007fff) == 0) /* global CR */
		return &l3->global;
	list_for_each_entry(p, &l3->plist, list)
		if ((p->pid & MISDN_PID_CRVAL_MASK) == (cr & MISDN_PID_CRVAL_MASK))
			return p;
	return NULL;
}

static void
L3TimerFunction(void *arg)
{
	struct L3Timer	*l3t = arg;
	l3_process_t	*pc;
	unsigned int	nr = l3t->nr;

	l3t->nr = 0;
	pc = get_l3process4pid(l3t->l3, l3t->pid);
	if (pc)
		l3t->l3->p_mgr(pc, nr, NULL);
}

static void
L3TimerInit(layer3_t *l3, int pid, struct L3Timer *t)
{
	init_timer(&t->tl, l3, t, L3TimerFunction);
	t->pid = pid;
	t->nr = 0;
	t->l3 = l3;
}

void
L3AddTimer(struct L3Timer *t, int delay, unsigned int nr)
{
	if (t->nr) {
		fprintf(stderr, "%s: timer %x reused as %x\n", __FUNCTION__, t->nr, nr);
		del_timer(&t->tl);
	}
	t->nr = nr;
	add_timer(&t->tl, delay);
}

void
L3DelTimer(struct L3Timer *t)
{
	if (t->nr) {
		del_timer(&t->tl);
		t->nr = 0;
	}
}

void
StopAllL3Timer(struct _l3_process *pc)
{
	L3DelTimer(&pc->timer);
	free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
}

static int
new_process_id(layer3_t *l3)
{
	int	i, j;

	for (i = 0; i < MAX_PID_COUNT/8; i++) {
		if (l3->pids[i] == 0xff)
			continue;
		for (j = 1; j <= 8; j++) {
			if (!((1 << (j-1)) & l3->pids[i])) {
				l3->pids[i] |= 1 << (j-1);
				return (8*i) + j;
			}
		}
	}
	return -1; /* overflow */
}

l3_process_t *
create_new_process(layer3_t *l3, unsigned int pid)
{
	int		cr;
	l3_process_t	*pc;


	if ((pid & 0xffff) > 0) { /* remote owned callref */
		if (pid & 0x8000)
			return NULL;
		pc = get_l3process4pid(l3, pid);
		if (pc) /* already here */
			return NULL;
	} else {
		cr = new_process_id(l3); /* we use callrefs global, not per TEI */
		if (cr < 0) /* all callrefs busy */
			return NULL;
		pid |= cr | 0x8000; /* we own it */
	}
	pc = calloc(1, sizeof(l3_process_t));
	if (!pc)
		return NULL;
	pc->pid = pid;
	pc->l3 = l3;
	L3TimerInit(l3, pid, &pc->timer);
	L3TimerInit(l3, pid, &pc->aux_timer);
	list_add_tail(&pc->list, &l3->plist);
	return pc;
}

void
SendMsg(l3_process_t *pc, struct l3_msg *l3m, int state) {
	int		ret;
	unsigned char	ch;
	struct mbuffer	*mb = container_of(l3m, struct mbuffer, l3);

	
	ret = assembleQ931(pc, l3m);
	if (ret) {
		fprintf(stderr, "%s assembleQ931 error %x\n", __FUNCTION__, ret);
		free_l3_msg(l3m);
		return;		
	}
	if (state != -1)
		newl3state(pc, state);
	mb->h->prim = DL_DATA_REQ;
	mb->h->id = l3m->pid;
	mb->h->len = mb->len;
	msg_push(mb, MISDN_HEADER_LEN);
	mb->addr = pc->l3->l2addr;
	ch = (mb->l3.pid >> 16) & 0xff;
	if (ch)
		mb->addr.channel =  ch;
	mqueue_tail(&pc->l3->squeue, mb);
	if (pc->l3->l3m.state != ST_L3_LC_ESTAB)
		l3_manager(pc->l3, DL_ESTABLISH_REQ);
}

void
release_l3_process(l3_process_t *pc)
{
	layer3_t *l3;

	if (!pc)
		return;
	l3 = pc->l3;
	mISDN_l3up(pc, MT_FREE, NULL);
	list_del(&pc->list);
	StopAllL3Timer(pc);
	L3DelTimer(&pc->aux_timer);
	free(pc);
	if (list_empty(&l3->plist) && !test_bit(FLG_PTP, &l3->ml3.options)) {
		if (!mqueue_len(&l3->squeue)) {
			FsmEvent(&l3->l3m, EV_RELEASE_REQ, NULL);
		}
	}
}

static void
l3ml3p(layer3_t *l3, int pr)
{
	l3_process_t *p, *np;

	list_for_each_entry_safe(p, np, &l3->plist, list)
		l3->p_mgr(p, pr, NULL);
}

void
mISDN_l3up(l3_process_t *l3p, u_int prim, struct l3_msg *l3m)
{
	int	ret;

	ret = l3p->l3->ml3.from_layer3(&l3p->l3->ml3, prim, l3p->pid, l3m);
	if (ret) {
		fprintf(stderr, "%s cannot deliver mesage %x process %x to application\n", __FUNCTION__, prim, l3p->pid);
		if (l3m)
			free_l3_msg(l3m);
	}
}

static void
l3down(layer3_t *l3, u_int prim, struct mbuffer *mb)
{
	int	ret;

	if (!mb) {
		mb = alloc_mbuffer();
		if (!mb) {
			fprintf(stderr, "%s cannot alloc mbuffer for %x\n", __FUNCTION__, prim);
			return;
		}
		msg_put(mb, MISDN_HEADER_LEN);
		mb->h->prim = prim;
		mb->h->id = 0;
		mb->h->len = 0;
		mb->addr = l3->l2addr;
	}
	ret = sendto(l3->l2sock, mb->head, mb->len, 0, (struct sockaddr *)&mb->addr, sizeof(mb->addr));
	if (ret < 0)
		fprintf(stderr, "%s write socket error %s\n", __FUNCTION__, strerror(errno));
}

#define DREL_TIMER_VALUE 40000

static void
lc_activate(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	FsmChangeState(fi, ST_L3_LC_ESTAB_WAIT);
	l3down(l3, DL_ESTABLISH_REQ, NULL);
}

static void
lc_connect(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;
	struct mbuffer	*mb;
	int dequeued = 0;

	FsmChangeState(fi, ST_L3_LC_ESTAB);
	while ((mb = mdequeue(&l3->squeue))) {
		l3down(l3, DL_DATA_REQ, mb);
		dequeued++;
	}
	if (list_empty(&l3->plist) && !test_bit(FLG_PTP, &l3->ml3.options) && dequeued) {
		FsmEvent(&l3->l3m, EV_RELEASE_REQ, NULL);
	} else
		l3ml3p(l3, DL_ESTABLISH_IND);
}

static void
lc_connected(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;
	struct mbuffer	*mb;
	int dequeued = 0;

	FsmDelTimer(&l3->l3m_timer, 51);
	FsmChangeState(fi, ST_L3_LC_ESTAB);
	while ((mb = mdequeue(&l3->squeue))) {
		l3down(l3, DL_DATA_REQ, mb);
		dequeued++;
	}
	if (list_empty(&l3->plist) && !test_bit(FLG_PTP, &l3->ml3.options) &&  dequeued) {
		FsmEvent(&l3->l3m, EV_RELEASE_REQ, NULL);
	} else
		l3ml3p(l3, DL_ESTABLISH_CNF);
}

static void
lc_start_delay(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	FsmChangeState(fi, ST_L3_LC_REL_DELAY);
	FsmAddTimer(&l3->l3m_timer, DREL_TIMER_VALUE, EV_TIMEOUT, NULL, 50);
}

static void
lc_release_req(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	if (test_bit(FLG_L2BLOCK, &l3->ml3.options)) {
		/* restart release timer */
		FsmAddTimer(&l3->l3m_timer, DREL_TIMER_VALUE, EV_TIMEOUT, NULL, 51);
	} else {
		FsmChangeState(fi, ST_L3_LC_REL_WAIT);
		l3down(l3, DL_RELEASE_REQ, NULL);
	}
}

static void
lc_release_ind(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	FsmDelTimer(&l3->l3m_timer, 52);
	FsmChangeState(fi, ST_L3_LC_REL);
	mqueue_purge(&l3->squeue);
	l3ml3p(l3, DL_RELEASE_IND);
}

static void
lc_release_cnf(struct FsmInst *fi, int event, void *arg)
{
	layer3_t *l3 = fi->userdata;

	FsmChangeState(fi, ST_L3_LC_REL);
	mqueue_purge(&l3->squeue);
	l3ml3p(l3, DL_RELEASE_CNF);
}


/* *INDENT-OFF* */
static struct FsmNode L3FnList[] =
{
	{ST_L3_LC_REL,		EV_ESTABLISH_REQ,	lc_activate},
	{ST_L3_LC_REL,		EV_ESTABLISH_IND,	lc_connect},
	{ST_L3_LC_REL,		EV_ESTABLISH_CNF,	lc_connect},
	{ST_L3_LC_ESTAB_WAIT,	EV_ESTABLISH_CNF,	lc_connected},
	{ST_L3_LC_ESTAB_WAIT,	EV_RELEASE_REQ,		lc_start_delay},
	{ST_L3_LC_ESTAB_WAIT,	EV_RELEASE_IND,		lc_release_ind},
	{ST_L3_LC_ESTAB,	EV_RELEASE_IND,		lc_release_ind},
	{ST_L3_LC_ESTAB,	EV_RELEASE_REQ,		lc_start_delay},
	{ST_L3_LC_REL_DELAY,	EV_RELEASE_IND,		lc_release_ind},
	{ST_L3_LC_REL_DELAY,	EV_ESTABLISH_REQ,	lc_connected},
	{ST_L3_LC_REL_DELAY,	EV_TIMEOUT,		lc_release_req},
	{ST_L3_LC_REL_WAIT,	EV_RELEASE_CNF,		lc_release_cnf},
	{ST_L3_LC_REL_WAIT,	EV_ESTABLISH_REQ,	lc_activate},
};
/* *INDENT-ON* */

#define L3_FN_COUNT (sizeof(L3FnList)/sizeof(struct FsmNode))


void
l3_manager(layer3_t *l3, unsigned int pr)
{
	switch (pr) {
	case DL_ESTABLISH_REQ:
		FsmEvent(&l3->l3m, EV_ESTABLISH_REQ, NULL);
		break;
	case DL_RELEASE_REQ:
		FsmEvent(&l3->l3m, EV_RELEASE_REQ, NULL);
		break;
	}
}

static int
to_layer3(struct mlayer3 *ml3, unsigned int prim, unsigned int pid, struct l3_msg *l3m)
{
	layer3_t	*l3;
	int		ret, id;
	l3_process_t	*proc;
	struct mbuffer	*mb;

	l3 = container_of(ml3, layer3_t, ml3);
	switch(prim) {
	case MT_ASSIGN:
		proc = create_new_process(l3, pid);
		if (!proc)
			return -EBUSY;
		ml3->from_layer3(ml3, MT_ASSIGN, proc->pid, NULL);
		break;
	case MT_ALERTING:
	case MT_CALL_PROCEEDING:
	case MT_CONNECT:
	case MT_CONNECT_ACKNOWLEDGE:
	case MT_DISCONNECT:
	case MT_INFORMATION:
	case MT_FACILITY:
	case MT_NOTIFY:
	case MT_PROGRESS:
	case MT_RELEASE:
	case MT_RELEASE_COMPLETE:
	case MT_SETUP:
	case MT_SETUP_ACKNOWLEDGE:
	case MT_RESUME_ACKNOWLEDGE:
	case MT_RESUME_REJECT:
	case MT_SUSPEND_ACKNOWLEDGE:
	case MT_SUSPEND_REJECT:
	case MT_USER_INFORMATION:
	case MT_RESTART:
	case MT_RESTART_ACKNOWLEDGE:
	case MT_CONGESTION_CONTROL:
	case MT_STATUS:
	case MT_STATUS_ENQUIRY:
	case MT_HOLD:
	case MT_HOLD_ACKNOWLEDGE:
	case MT_HOLD_REJECT:
	case MT_RETRIEVE:
	case MT_RETRIEVE_ACKNOWLEDGE:
	case MT_RETRIEVE_REJECT:
	case MT_RESUME: /* RESUME only in user->net */
	case MT_SUSPEND: /* SUSPEND only in user->net */
		if (!l3m) {
			l3m = alloc_l3_msg();
			if (!l3m)
				return -ENOMEM;
		}
		mb = container_of(l3m, struct mbuffer, l3);
		l3m->type = prim;
		l3m->pid = pid;
		mqueue_tail(&l3->app_queue, mb);
		id = 0;
		/* wake up worker */
		ret = ioctl(l3->mdev, IMADDTIMER, &id);
		if (ret)
			fprintf(stderr, "%s wake up worker error %s\n", __FUNCTION__, strerror(errno));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

void
init_l3(layer3_t *l3)
{
	INIT_LIST_HEAD(&l3->plist);
	L3TimerInit(l3, MISDN_PID_GLOBAL, &l3->global.timer);
	L3TimerInit(l3, MISDN_PID_DUMMY, &l3->dummy.timer);
	mqueue_init(&l3->squeue);
	mqueue_init(&l3->app_queue);
	l3->l3m.fsm = &l3fsm;
	l3->l3m.state = ST_L3_LC_REL;
	l3->l3m.userdata = l3;
	l3->l3m.l3 = l3;
	l3->l3m.userint = 0;
//	l3->l3m.printdebug = l3m_debug;
	INIT_LIST_HEAD(&l3->pending_timer);
	FsmInitTimer(&l3->l3m, &l3->l3m_timer);
	l3->ml3.to_layer3 = to_layer3;
}


void
release_l3(layer3_t *l3)
{
	l3_process_t *p, *np;

	list_for_each_entry_safe(p, np, &l3->plist, list)
		release_l3_process(p);
	StopAllL3Timer(&l3->global);
	StopAllL3Timer(&l3->dummy);
	FsmDelTimer(&l3->l3m_timer, 54);
	mqueue_purge(&l3->squeue);
	mqueue_purge(&l3->app_queue);
}

void
mISDNl3New(void)
{
	l3fsm.state_count = L3_STATE_COUNT;
	l3fsm.event_count = L3_EVENT_COUNT;
	l3fsm.strEvent = strL3Event;
	l3fsm.strState = strL3State;
	FsmNew(&l3fsm, L3FnList, L3_FN_COUNT);
}

void
mISDNl3Free(void)
{
	FsmFree(&l3fsm);
}


static void
handle_l2msg(struct _layer3 *l3, struct mbuffer *mb)
{
	switch (mb->h->prim) {
	case DL_DATA_IND:
	case DL_UNITDATA_IND:
		l3->from_l2(l3, mb);
		return;
	case DL_INFORMATION_IND:
		l3->l2addr = mb->addr;
		break;
	case DL_ESTABLISH_CNF:
		FsmEvent(&l3->l3m, EV_ESTABLISH_CNF, NULL);
		break;
	case DL_ESTABLISH_IND:
		FsmEvent(&l3->l3m, EV_ESTABLISH_IND, NULL);
		break;
	case DL_RELEASE_IND:
		FsmEvent(&l3->l3m, EV_RELEASE_IND, NULL);
		break;
	case DL_RELEASE_CNF:
		FsmEvent(&l3->l3m, EV_RELEASE_CNF, NULL);
		break;
	}
	free_mbuffer(mb);
}

static void *
layer3_thread(void *arg)
{
	struct _layer3	*l3 = arg;
	fd_set		rfd;
	struct mbuffer	*mb;
	int		ret, id;
	socklen_t	alen;

	while(!test_bit(FLG_ABORT, &l3->ml3.options)) {
		FD_ZERO(&rfd);
		FD_SET(l3->l2sock, &rfd);
		FD_SET(l3->mdev, &rfd);
		ret = select(l3->maxfd, &rfd, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "%s aborted: %s\n", __FUNCTION__, strerror(errno));
			break;
		}
		if (FD_ISSET(l3->mdev, &rfd)) {
			ret = read(l3->mdev, &id, sizeof(id));
			if (ret < 0) {
				fprintf(stderr, "%s read timer error %s\n", __FUNCTION__, strerror(errno));
			} else {
				if (ret == sizeof(id) && id) {
					expire_timer(l3, id);
				}
			}
		}
		if (FD_ISSET(l3->l2sock, &rfd)) {
			mb = alloc_mbuffer();
			if (!mb) {
				fprintf(stderr, "%s no memory for mbuffer\n", __FUNCTION__);
				break;
			}
			alen = sizeof(mb->addr);
			ret = recvfrom(l3->l2sock, mb->head, MBUFFER_DATA_SIZE, 0, (struct sockaddr *) &mb->addr, &alen);
			if (ret < 0) {
				fprintf(stderr, "%s read socket error %s\n", __FUNCTION__, strerror(errno));
			} else if (ret < MISDN_HEADER_LEN) {
					fprintf(stderr, "%s read socket shor frame\n", __FUNCTION__);
			} else {
				mb->len = ret;
				msg_pull(mb, MISDN_HEADER_LEN);
				handle_l2msg(l3, mb);
			}
		}
		while ((mb = mdequeue(&l3->app_queue))) {
			l3->to_l3(l3, &mb->l3);
		}
		if (l3->l3m.state == ST_L3_LC_ESTAB) {
			while ((mb = mdequeue(&l3->squeue))) {
				ret = sendto(l3->l2sock, mb->head, mb->len, 0, (struct sockaddr *)&mb->addr, sizeof(mb->addr));
				if (ret < 0)
					fprintf(stderr, "%s write socket error %s\n", __FUNCTION__, strerror(errno));
			}
		}
	}
	return NULL;
}

int
l3_start(struct _layer3 *l3)
{
	int	ret;

	ret = pthread_create(&l3->worker, NULL, layer3_thread, (void *)l3);
	if (ret) {
		fprintf(stderr, "%s cannot start worker thread  %s\n", __FUNCTION__, strerror(errno));
	}
	return ret;
}

void
l3_stop(struct _layer3 *l3)
{
	int	ret;

	test_and_set_bit(FLG_ABORT, &l3->ml3.options);
	ret = pthread_cancel(l3->worker);
	if (ret)
		fprintf(stderr, "%s cannot cancel worker thread  %s\n", __FUNCTION__, strerror(errno));
	ret = pthread_join(l3->worker, NULL);
	if (ret)
		fprintf(stderr, "%s cannot join worker thread  %s\n", __FUNCTION__, strerror(errno));
}

