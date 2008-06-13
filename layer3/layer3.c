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
#include "debug.h"

void
l3_debug(layer3_t *l3, char *fmt, ...)
{
	va_list args;
	char buf[256], *p;

	va_start(args, fmt);
	p = buf;
	p += sprintf(p, "l3 ");
	p += vsprintf(p, fmt, args);
	va_end(args);
	dprint(DBGM_L3, l3->l2master.l2addr.dev, "%s\n", buf);
}

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
	l3_process_t	*p, *cp;

	if ((pid & 0xffff7fff) == 0) /* global CR */
		return &l3->global;
	if (pid == MISDN_PID_GLOBAL)
		return &l3->global;
	if (pid == MISDN_PID_DUMMY)
		return &l3->dummy;
	list_for_each_entry(p, &l3->plist, list) {
		if (p->pid == pid)
			return p;
		if (!list_empty(&p->child)) {
			list_for_each_entry(cp, &p->child, list) {
				if (cp->pid == pid)
					return cp;
			}
		}
		if ((p->pid & MISDN_PID_CRVAL_MASK) == (pid & MISDN_PID_CRVAL_MASK)) {
			if ((p->pid & MISDN_PID_CRTYPE_MASK) == MISDN_PID_MASTER)
				return p;
		}
	}
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

l3_process_t *
get_first_l3process4ces(layer3_t *l3, unsigned int ces)
{
	l3_process_t	*p;

	if (ces == MISDN_CES_MASTER)
		return NULL;
	list_for_each_entry(p, &l3->plist, list)
		if ((p->pid >> 16) == ces)
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
		eprint("%s: timer %x reused as %x\n", __FUNCTION__, t->nr, nr);
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
	L3DelTimer(&pc->timer1);
	L3DelTimer(&pc->timer2);
	free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	test_and_clear_bit(FLG_L3P_TIMER303_1, &pc->flags);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->flags);
	test_and_clear_bit(FLG_L3P_TIMER312, &pc->flags);
}

static struct l2l3if *
get_l2if(layer3_t *l3, unsigned int ces)
{
	struct l2l3if	*l2i;

	if (ces == MISDN_CES_MASTER)
		return &l3->l2master;
	if (ces == l3->l2master.l2addr.channel)
		return &l3->l2master;
	list_for_each_entry(l2i, &l3->l2master.list, list) {
		if (ces == l2i->l2addr.channel)
			return l2i;
	}
	return NULL;
}

l3_process_t *
create_new_process(layer3_t *l3, unsigned int ces, unsigned int cr, l3_process_t *master)
{
	l3_process_t	*pc;
	unsigned int	max_cr = 0x7fff;
	int		try;
	

	if ((cr & 0xffff) > 0) { /* remote owned callref */
		pc = get_l3process4pid(l3, ((ces & 0xff) << 16) | cr);
		if (pc && (pc != master)) /* already here */
			return NULL;
	} else {
		if (test_bit(FLG_BASICRATE, &l3->ml3.options))
			max_cr = 0x7f;
		for (try = 0; try <= l3->ml3.nr_bchannel; try++) { /* more tries for more channels */
			cr = l3->next_cr++;
			if (l3->next_cr > max_cr)
				l3->next_cr = 1;
			if (!get_l3process4cref(l3, cr))
				break;
		}
		if (get_l3process4cref(l3, cr))
			return NULL;
		cr |= MISDN_PID_CR_FLAG; /* we own it */
	}
	pc = calloc(1, sizeof(l3_process_t));
	if (!pc) {
		eprint("%s: no memory for layer3 process\n", __FUNCTION__);
		return NULL;
	}
	pc->l2if = get_l2if(l3, ces);
	if (ces == MISDN_CES_MASTER) {
		if (test_bit(FLG_USER, &l3->ml3.options) && !test_bit(MISDN_FLG_PTP, &l3->ml3.options)) {
			if (list_empty(&l3->l2master.list)) {
				eprint("%s: no layer2 assigned\n", __FUNCTION__);
				pc->l2if = NULL;
			} else
				pc->l2if = (struct l2l3if   *)l3->l2master.list.next;
		}
	}
	if (!pc->l2if) {
		eprint("%s: no layer2 if found for ces %x\n", __FUNCTION__, ces);
		free(pc);
		return NULL;
	}
	pc->L3 = l3;
	pc->pid = ((ces & 0xffff) << 16) | cr;
	L3TimerInit(l3, pc->pid, &pc->timer1);
	L3TimerInit(l3, pc->pid, &pc->timer2);
	INIT_LIST_HEAD(&pc->child);
	pc->master = master;
	if (master)
		list_add_tail(&pc->list, &master->child);
	else
		list_add_tail(&pc->list, &l3->plist);
	return pc;
}

void
SendMsg(l3_process_t *pc, struct l3_msg *l3m, int state) {
	int		ret;
	struct mbuffer	*mb = container_of(l3m, struct mbuffer, l3);

	
	ret = assembleQ931(pc, l3m);
	if (ret) {
		eprint("%s assembleQ931 error %x\n", __FUNCTION__, ret);
		free_l3_msg(l3m);
		return;		
	}
	if (state != -1)
		newl3state(pc, state);
	mb->h->id = l3m->pid;
	msg_push(mb, MISDN_HEADER_LEN);
	if ((l3m->type == MT_SETUP) && test_bit(FLG_NETWORK, &pc->l2if->l3->ml3.options) &&
	    !test_bit(MISDN_FLG_PTP, &pc->l2if->l3->ml3.options))
		mb->h->prim = DL_UNITDATA_REQ;
	else
		mb->h->prim = DL_DATA_REQ;
	mb->addr = pc->l2if->l2addr;
	mqueue_tail(&pc->l2if->squeue, mb);
	if (pc->l2if->l3m.state != ST_L3_LC_ESTAB)
		l3_manager(pc->l2if, DL_ESTABLISH_REQ);
}

void
release_l3_process(l3_process_t *pc)
{
	layer3_t	*l3;
	struct l2l3if	*l2i;
	int		ces;

	if (!pc)
		return;
	l2i = pc->l2if;
	l3 = l2i->l3;
	ces = pc->pid >> 16;
	mISDN_l3up(pc, MT_FREE, NULL);
	list_del(&pc->list);
	StopAllL3Timer(pc);
	free(pc);
	pc = get_first_l3process4ces(l3, ces);
	if ((!pc) && !test_bit(MISDN_FLG_L2_HOLD, &l3->ml3.options)) {
		if (!mqueue_len(&l2i->squeue)) {
			FsmEvent(&l2i->l3m, EV_RELEASE_REQ, NULL);
		}
	}
}

static void
l3ml3p(layer3_t *l3, int pr, unsigned int ces)
{
	l3_process_t *p, *np;

	list_for_each_entry_safe(p, np, &l3->plist, list)
		if ((p->pid >> 16) == ces)
			l3->p_mgr(p, pr, NULL);
}

void
mISDN_l3up(l3_process_t *l3p, u_int prim, struct l3_msg *l3m)
{
	int	ret;

	if (!l3p->L3) {
		eprint("%s no L3 for l3p(%p) pid(%x)\n", __FUNCTION__, l3p, l3p->pid);
		return;
	}
	ret = l3p->L3->ml3.from_layer3(&l3p->L3->ml3, prim, l3p->pid, l3m);
	if (ret) {
		eprint("%s cannot deliver mesage %x process %x to application\n", __FUNCTION__, prim, l3p->pid);
		if (l3m)
			free_l3_msg(l3m);
	}
}

static void
l3down(struct l2l3if *l2i, u_int prim, struct mbuffer *mb)
{
	int	ret;

	if (!mb) {
		mb = alloc_mbuffer();
		if (!mb) {
			eprint("%s cannot alloc mbuffer for %x\n", __FUNCTION__, prim);
			return;
		}
		msg_put(mb, MISDN_HEADER_LEN);
		mb->h->prim = prim;
		mb->h->id = 0;
		mb->addr = l2i->l2addr;
	}
	ret = sendto(l2i->l3->l2sock, mb->head, mb->len, 0, (struct sockaddr *)&mb->addr, sizeof(mb->addr));
	if (ret < 0)
		eprint("%s write socket error %s\n", __FUNCTION__, strerror(errno));
	free_mbuffer(mb);
}

#define DREL_TIMER_VALUE 40000

static void
lc_activate(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;

	FsmChangeState(fi, ST_L3_LC_ESTAB_WAIT);
	l3down(l2i, DL_ESTABLISH_REQ, NULL);
}

static void
lc_connect(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;
	struct mbuffer	*mb;
	int dequeued = 0;
	l3_process_t	*pc;

	FsmChangeState(fi, ST_L3_LC_ESTAB);
	while ((mb = mdequeue(&l2i->squeue))) {
		l3down(l2i, DL_DATA_REQ, mb);
		dequeued++;
	}
	pc = get_first_l3process4ces(l2i->l3, l2i->l2addr.channel);
	if ((!pc) && (!test_bit(MISDN_FLG_L2_HOLD, &l2i->l3->ml3.options)) && dequeued) {
		FsmEvent(fi, EV_RELEASE_REQ, NULL);
	} else {
		l3ml3p(l2i->l3, DL_ESTABLISH_IND, l2i->l2addr.channel);
		l2i->l3->ml3.from_layer3(&l2i->l3->ml3, MT_L2ESTABLISH, l2i->l2addr.tei, NULL);
	}
}

static void
lc_connected(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;
	struct mbuffer	*mb;
	int dequeued = 0;
	l3_process_t	*pc;

	FsmDelTimer(&l2i->l3m_timer, 51);
	FsmChangeState(fi, ST_L3_LC_ESTAB);
	while ((mb = mdequeue(&l2i->squeue))) {
		l3down(l2i, DL_DATA_REQ, mb);
		dequeued++;
	}
	pc = get_first_l3process4ces(l2i->l3, l2i->l2addr.channel);
	if ((!pc) && (!test_bit(MISDN_FLG_L2_HOLD, &l2i->l3->ml3.options)) && dequeued) {
		FsmEvent(fi, EV_RELEASE_REQ, NULL);
	} else {
		l3ml3p(l2i->l3, DL_ESTABLISH_IND, l2i->l2addr.channel);
		l2i->l3->ml3.from_layer3(&l2i->l3->ml3, MT_L2ESTABLISH, l2i->l2addr.tei, NULL);
	}
}

static void
lc_start_delay(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;

	FsmChangeState(fi, ST_L3_LC_REL_DELAY);
	FsmAddTimer(&l2i->l3m_timer, DREL_TIMER_VALUE, EV_TIMEOUT, NULL, 50);
}

static void
lc_release_req(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;

	if (test_bit(FLG_L2BLOCK, &l2i->l3->ml3.options)) {
		/* restart release timer */
		FsmAddTimer(&l2i->l3m_timer, DREL_TIMER_VALUE, EV_TIMEOUT, NULL, 51);
	} else {
		FsmChangeState(fi, ST_L3_LC_REL_WAIT);
		l3down(l2i, DL_RELEASE_REQ, NULL);
	}
}

static void
lc_release_ind(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;

	FsmDelTimer(&l2i->l3m_timer, 52);
	FsmChangeState(fi, ST_L3_LC_REL);
	mqueue_purge(&l2i->squeue);
	l3ml3p(l2i->l3, DL_RELEASE_IND, l2i->l2addr.channel);
	l2i->l3->ml3.from_layer3(&l2i->l3->ml3, MT_L2RELEASE, l2i->l2addr.tei, NULL);
}

static void
lc_release_cnf(struct FsmInst *fi, int event, void *arg)
{
	struct l2l3if *l2i = fi->userdata;

	FsmChangeState(fi, ST_L3_LC_REL);
	mqueue_purge(&l2i->squeue);
	l3ml3p(l2i->l3, DL_RELEASE_CNF, l2i->l2addr.channel);
	l2i->l3->ml3.from_layer3(&l2i->l3->ml3, MT_L2RELEASE, l2i->l2addr.tei, NULL);
}


/* *INDENT-OFF* */
static struct FsmNode L3FnList[] =
{
	{ST_L3_LC_REL,		EV_ESTABLISH_REQ,	lc_activate},
	{ST_L3_LC_REL,		EV_ESTABLISH_IND,	lc_connect},
	{ST_L3_LC_REL,		EV_ESTABLISH_CNF,	lc_connect},
	{ST_L3_LC_ESTAB_WAIT,	EV_ESTABLISH_IND,	lc_connected},
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
l3_manager(struct l2l3if *l2i, unsigned int pr)
{
	switch (pr) {
	case DL_ESTABLISH_REQ:
		FsmEvent(&l2i->l3m, EV_ESTABLISH_REQ, NULL);
		break;
	case DL_RELEASE_REQ:
		FsmEvent(&l2i->l3m, EV_RELEASE_REQ, NULL);
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
		proc = create_new_process(l3, MISDN_CES_MASTER, 0, NULL);
		if (!proc)
			return -EBUSY;
		dprint(DBGM_L3, l3->l2master.l2addr.dev, "%s: new procpid(%x)\n",
			__FUNCTION__, proc->pid);
		ml3->from_layer3(ml3, MT_ASSIGN, proc->pid, NULL);
		break;
	case MT_L2ESTABLISH:
	case MT_L2RELEASE:
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
			eprint("%s wake up worker error %s\n", __FUNCTION__, strerror(errno));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void
init_l2if(struct l2l3if *l2i, layer3_t *l3)
{
	INIT_LIST_HEAD(&l2i->list);
	l2i->l3 = l3;
	mqueue_init(&l2i->squeue);
	l2i->l3m.fsm = &l3fsm;
	l2i->l3m.state = ST_L3_LC_REL;
	l2i->l3m.userdata = l2i;
	l2i->l3m.l3 = l3;
	l2i->l3m.userint = 0;
	FsmInitTimer(&l2i->l3m, &l2i->l3m_timer);
//	l2i->l3m.printdebug = l3_debug;
}

static struct l2l3if *
create_l2l3if(layer3_t *l3, struct sockaddr_mISDN *addr)
{
	struct l2l3if	*l2i;

	if (l3->l2master.l2addr.tei == addr->tei)
		l2i = &l3->l2master;
	else
		l2i = get_l2if(l3, addr->channel);
	if (l2i) {
		dprint(DBGM_L3, l2i->l2addr.dev, "%s: already have layer2/3 interface for ces(%x) tei(%x/%x)\n",
			__FUNCTION__, addr->channel, addr->tei, l2i->l2addr.tei);
		l2i->l2addr = *addr;
		return l2i;
	}
	l2i = calloc(1, sizeof(struct l2l3if));
	if (!l2i) {
		eprint("%s: no memory\n", __FUNCTION__);
		return NULL;
	}
	init_l2if(l2i, l3);
	l2i->l2addr = *addr;
	list_add_tail(&l2i->list, &l3->l2master.list);
	return l2i;
}

void
init_l3(layer3_t *l3)
{
	INIT_LIST_HEAD(&l3->plist);
	l3->global.l2if = &l3->l2master;
	l3->global.L3 = l3;
	l3->dummy.l2if = &l3->l2master;
	l3->dummy.L3 = l3;
	L3TimerInit(l3, MISDN_PID_GLOBAL, &l3->global.timer1);
	L3TimerInit(l3, MISDN_PID_GLOBAL, &l3->global.timer2);
	L3TimerInit(l3, MISDN_PID_DUMMY, &l3->dummy.timer1);
	L3TimerInit(l3, MISDN_PID_DUMMY, &l3->dummy.timer2);
	l3->debug = 0xff; // FIXME dynamic
	init_l2if(&l3->l2master, l3);
	/*
	 * Broadcast is always established.
	 * PTP l2master process follows tei 0 process
	 */
	if (!test_bit(MISDN_FLG_PTP, &l3->ml3.options))
		l3->l2master.l3m.state = ST_L3_LC_ESTAB;
	mqueue_init(&l3->app_queue);
	INIT_LIST_HEAD(&l3->pending_timer);
	l3->ml3.to_layer3 = to_layer3;
	pthread_mutex_init(&l3->run, NULL);
	l3->next_cr = 1;
}

static void
release_if(struct l2l3if *l2i)
{
	mqueue_purge(&l2i->squeue);
	FsmDelTimer(&l2i->l3m_timer, 54);
}

void
release_l3(layer3_t *l3)
{
	l3_process_t	*p, *np;
	struct l2l3if	*l2i, *nl2i;

	list_for_each_entry_safe(p, np, &l3->plist, list)
		release_l3_process(p);
	StopAllL3Timer(&l3->global);
	StopAllL3Timer(&l3->dummy);
	release_if(&l3->l2master);
	mqueue_purge(&l3->app_queue);
	list_for_each_entry_safe(l2i, nl2i, &l3->l2master.list, list) {
		release_if(l2i);
		list_del(&l2i->list);
		free(l2i);
	}
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
	struct l2l3if	*l2i;
	int ret;

	switch (mb->h->prim) {
	case DL_DATA_IND:
	case DL_UNITDATA_IND:
		l3->from_l2(l3, mb);
		return;
	case DL_INFORMATION_IND:
		l2i = create_l2l3if(l3, &mb->addr);
		goto free_out;
	case MPH_INFORMATION_IND:
	case MPH_ACTIVATE_IND:
	case MPH_DEACTIVATE_IND:
		ret = l3->ml3.from_layer3(&l3->ml3, mb->h->prim, mb->h->id, &mb->l3);
		if (ret)
			goto free_out;
		return;
	}
	l2i = get_l2if(l3, mb->addr.channel);
	if (!l2i) {
		eprint("%s: cannot find layer2/3 interface for ces(%x)\n", __FUNCTION__,  mb->addr.channel);
		goto free_out;
	}
	switch (mb->h->prim) {
	case DL_ESTABLISH_CNF:
		FsmEvent(&l2i->l3m, EV_ESTABLISH_CNF, NULL);
		break;
	case DL_ESTABLISH_IND:
		FsmEvent(&l2i->l3m, EV_ESTABLISH_IND, NULL);
		break;
	case DL_RELEASE_IND:
		FsmEvent(&l2i->l3m, EV_RELEASE_IND, NULL);
		break;
	case DL_RELEASE_CNF:
		FsmEvent(&l2i->l3m, EV_RELEASE_CNF, NULL);
		break;
	default:
		dprint(DBGM_L3, mb->addr.dev, "%s: unknown prim(%x) ces(%x)\n", __FUNCTION__,  mb->h->prim, mb->addr.channel);
	}
free_out:
	free_mbuffer(mb);
}

static void
to_l2(layer3_t *l3, struct l3_msg *l3m)
{
	struct l2l3if	*l2i;

	list_for_each_entry(l2i, &l3->l2master.list, list) {
		/* given tei or 0=first tei, but not 127 */
		if (l3m->pid == l2i->l2addr.tei
		 || (l3m->pid == 0 && l2i->l2addr.tei != 127)) {
			switch(l3m->type) {
			case MT_L2ESTABLISH:
				FsmEvent(&l2i->l3m, EV_ESTABLISH_REQ, NULL);
				break;
			case MT_L2RELEASE:
				FsmEvent(&l2i->l3m, EV_RELEASE_REQ, NULL);
				break;
			}
			break;
		}
	}
	free_l3_msg(l3m);
}

static void *
layer3_thread(void *arg)
{
	struct _layer3	*l3 = arg;
	fd_set		rfd;
	struct mbuffer	*mb;
	int		ret, id;
	socklen_t	alen;
	struct l2l3if	*l2i;

	while(!test_bit(FLG_ABORT, &l3->ml3.options)) {
		FD_ZERO(&rfd);
		FD_SET(l3->l2sock, &rfd);
		FD_SET(l3->mdev, &rfd);
		ret = select(l3->maxfd +1, &rfd, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			eprint("%s aborted: %s\n", __FUNCTION__, strerror(errno));
			break;
		}
		if (FD_ISSET(l3->mdev, &rfd)) {
			ret = read(l3->mdev, &id, sizeof(id));
			if (ret < 0) {
				eprint("%s read timer error %s\n", __FUNCTION__, strerror(errno));
			} else {
				if (ret == sizeof(id) && id) {
					expire_timer(l3, id);
				}
			}
		}
		if (FD_ISSET(l3->l2sock, &rfd)) {
			mb = alloc_mbuffer();
			if (!mb) {
				eprint("%s no memory for mbuffer\n", __FUNCTION__);
				break;
			}
			alen = sizeof(mb->addr);
			ret = recvfrom(l3->l2sock, mb->head, MBUFFER_DATA_SIZE, 0, (struct sockaddr *) &mb->addr, &alen);
			if (ret < 0) {
				eprint("%s read socket error %s\n", __FUNCTION__, strerror(errno));
			} else if (ret < MISDN_HEADER_LEN) {
					eprint("%s read socket shor frame\n", __FUNCTION__);
			} else {
				mb->len = ret;
				msg_pull(mb, MISDN_HEADER_LEN);
				handle_l2msg(l3, mb);
			}
		}
		while ((mb = mdequeue(&l3->app_queue))) {
			if (mb->l3.type == MT_L2ESTABLISH || mb->l3.type == MT_L2RELEASE)
				to_l2(l3, &mb->l3);
			else {
				ret = l3->to_l3(l3, &mb->l3);
				if (ret < 0)
					free_mbuffer(mb);
			}
		}
		if (l3->l2master.l3m.state == ST_L3_LC_ESTAB) {
			while ((mb = mdequeue(&l3->l2master.squeue))) {
				ret = sendto(l3->l2sock, mb->head, mb->len, 0, (struct sockaddr *)&mb->addr, sizeof(mb->addr));
				if (ret < 0)
					eprint("%s write socket error %s\n", __FUNCTION__, strerror(errno));
				free_mbuffer(mb);
			}
		}
		list_for_each_entry(l2i, &l3->l2master.list, list) {
			if (l2i->l3m.state == ST_L3_LC_ESTAB) {
				while ((mb = mdequeue(&l2i->squeue))) {
					ret = sendto(l3->l2sock, mb->head, mb->len, 0, (struct sockaddr *)&mb->addr, sizeof(mb->addr));
					if (ret < 0)
						eprint("%s write socket error %s\n", __FUNCTION__, strerror(errno));
					free_mbuffer(mb);
				}
			}
		}
		if (test_bit(FLG_RUN_WAIT, &l3->ml3.options)) {
			test_and_clear_bit(FLG_RUN_WAIT, &l3->ml3.options);
			pthread_mutex_unlock(&l3->run);
		}
	}
	return NULL;
}

int
l3_start(struct _layer3 *l3)
{
	int	ret;

	pthread_mutex_lock(&l3->run);
	test_and_set_bit(FLG_RUN_WAIT, &l3->ml3.options);
	ret = pthread_create(&l3->worker, NULL, layer3_thread, (void *)l3);
	if (ret) {
		eprint("%s cannot start worker thread  %s\n", __FUNCTION__, strerror(errno));
	} else
		pthread_mutex_lock(&l3->run); // wait until tread is running
	return ret;
}

void
l3_stop(struct _layer3 *l3)
{
	int	ret;

	test_and_set_bit(FLG_ABORT, &l3->ml3.options);
	ret = pthread_cancel(l3->worker);
	if (ret)
		eprint("%s cannot cancel worker thread  %s\n", __FUNCTION__, strerror(errno));
	ret = pthread_join(l3->worker, NULL);
	if (ret)
		eprint("%s cannot join worker thread  %s\n", __FUNCTION__, strerror(errno));
}

