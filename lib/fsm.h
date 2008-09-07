/* $Id: fsm.h,v 2.0 2007/06/29 14:35:31 kkeil Exp $
 *
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


/* Statemachine */

#include "mtimer.h"

struct FsmInst;

typedef void (* FSMFNPTR)(struct FsmInst *, int, void *);

struct Fsm {
	FSMFNPTR *jumpmatrix;
	int state_count, event_count;
	char **strEvent, **strState;
};

struct FsmInst {
	struct Fsm	*fsm;
	struct _layer3	*l3;
	int		state;
	int		debug;
	void		*userdata;
	int		userint;
	void		(*printdebug) (struct FsmInst *, char *, ...);
};

struct FsmNode {
	int state, event;
	void (*routine) (struct FsmInst *, int, void *);
};

struct FsmTimer {
	struct FsmInst	*fi;
	struct mtimer	tl;
	int		event;
	void		*arg;
};

extern void FsmNew(struct Fsm *, struct FsmNode *, int);
extern void FsmFree(struct Fsm *);
extern int FsmEvent(struct FsmInst *, int , void *);
extern void FsmChangeState(struct FsmInst *, int);
extern void FsmInitTimer(struct FsmInst *, struct FsmTimer *);
extern int FsmAddTimer(struct FsmTimer *, int, int, void *, int);
extern void FsmRestartTimer(struct FsmTimer *, int, int, void *, int);
extern void FsmDelTimer(struct FsmTimer *, int);

