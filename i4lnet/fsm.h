/* $Id: fsm.h,v 1.0 2003/08/27 07:35:31 kkeil Exp $
 *
 * This file is (c) under GNU PUBLIC LICENSE
 *
 */


/* Statemachine */

#include "isdn_net.h"

struct FsmInst;

typedef void (* FSMFNPTR)(struct FsmInst *, int, void *);

struct Fsm {
	FSMFNPTR *jumpmatrix;
	int state_count, event_count;
	char **strEvent, **strState;
};

struct FsmInst {
	struct Fsm	*fsm;
	net_stack_t	*nst;
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
	itimer_t	tl;
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
extern void FsmRemoveTimer(struct FsmTimer *);

