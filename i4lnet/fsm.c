/* $Id: fsm.c,v 1.0 2003/08/27 07:35:31 kkeil Exp $
 *
 * Author       Karsten Keil (keil@isdn4linux.de)
 *
 * Thanks to    Jan den Ouden
 *              Fritz Elfert
 *
 * This file is (c) under GNU PUBLIC LICENSE
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "fsm.h"

#define FSM_TIMER_DEBUG 0

void
FsmNew(struct Fsm *fsm,
       struct FsmNode *fnlist, int fncount)
{
	int i;

	fsm->jumpmatrix = (FSMFNPTR *)
		malloc(sizeof (FSMFNPTR) * fsm->state_count * fsm->event_count);
	if (!fsm->jumpmatrix)
		return;
	memset(fsm->jumpmatrix, 0, sizeof (FSMFNPTR) * fsm->state_count * fsm->event_count);
	for (i = 0; i < fncount; i++) 
		if ((fnlist[i].state>=fsm->state_count) || (fnlist[i].event>=fsm->event_count)) {
			eprint("FsmNew Error line %d st(%ld/%ld) ev(%ld/%ld)\n",
				i,(long)fnlist[i].state,(long)fsm->state_count,
				(long)fnlist[i].event,(long)fsm->event_count);
		} else		
			fsm->jumpmatrix[fsm->state_count * fnlist[i].event +
				fnlist[i].state] = (FSMFNPTR) fnlist[i].routine;
}

void
FsmFree(struct Fsm *fsm)
{
	free(fsm->jumpmatrix);
}

int
FsmEvent(struct FsmInst *fi, int event, void *arg)
{
	FSMFNPTR r;

	if ((fi->state>=fi->fsm->state_count) || (event >= fi->fsm->event_count)) {
		eprint("FsmEvent Error st(%ld/%ld) ev(%d/%ld)\n",
			(long)fi->state,(long)fi->fsm->state_count,event,(long)fi->fsm->event_count);
		return(1);
	}
	r = fi->fsm->jumpmatrix[fi->fsm->state_count * event + fi->state];
	if (r) {
		if (fi->debug)
			fi->printdebug(fi, "State %s Event %s",
				fi->fsm->strState[fi->state],
				fi->fsm->strEvent[event]);
		r(fi, event, arg);
		return (0);
	} else {
		if (fi->debug)
			fi->printdebug(fi, "State %s Event %s no action",
				fi->fsm->strState[fi->state],
				fi->fsm->strEvent[event]);
		return (!0);
	}
}

void
FsmChangeState(struct FsmInst *fi, int newstate)
{
	fi->state = newstate;
	if (fi->debug)
		fi->printdebug(fi, "ChangeState %s",
			fi->fsm->strState[newstate]);
}

static int
FsmExpireTimer(struct FsmTimer *ft)
{
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmExpireTimer %lx", (long) ft);
#endif
	FsmEvent(ft->fi, ft->event, ft->arg);
	return(0);
}

void
FsmInitTimer(struct FsmInst *fi, struct FsmTimer *ft)
{
	ft->fi = fi;
	ft->tl.function = (void *)FsmExpireTimer;
	ft->tl.data = (long) ft;
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmInitTimer %lx", (long) ft);
#endif
	init_timer(&ft->tl, ft->fi->nst);
}

void
FsmDelTimer(struct FsmTimer *ft, int where)
{
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmDelTimer %lx %d", (long) ft, where);
#endif
	del_timer(&ft->tl);
}

void
FsmRemoveTimer(struct FsmTimer *ft)
{
	remove_timer(&ft->tl);
}

int
FsmAddTimer(struct FsmTimer *ft,
	    int millisec, int event, void *arg, int where)
{

#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmAddTimer %lx %d %d",
			(long) ft, millisec, where);
#endif

	if (timer_pending(&ft->tl)) {
		wprint("FsmAddTimer: timer already active!\n");
		ft->fi->printdebug(ft->fi, "FsmAddTimer already active!");
		return -1;
	}
	init_timer(&ft->tl, ft->fi->nst);
	ft->event = event;
	ft->arg = arg;
	ft->tl.expires = millisec;
	add_timer(&ft->tl);
	return 0;
}

void
FsmRestartTimer(struct FsmTimer *ft,
	    int millisec, int event, void *arg, int where)
{

#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmRestartTimer %lx %d %d",
			(long) ft, millisec, where);
#endif

	if (timer_pending(&ft->tl))
		del_timer(&ft->tl);
	init_timer(&ft->tl, ft->fi->nst);
	ft->event = event;
	ft->arg = arg;
	ft->tl.expires = millisec;
	add_timer(&ft->tl);
}
