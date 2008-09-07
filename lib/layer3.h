/* layer3.h
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

#ifndef LAYER3_H
#define LAYER3_H

#include <mbuffer.h>
#include "helper.h"
#include "mlist.h"
#include "mtimer.h"
#include "fsm.h"

typedef struct _l3_process	l3_process_t;

struct l3protocol {
	char			*name;
	unsigned int		protocol;
	void			(*init)(struct _layer3 *);
};

struct L3Timer {
	struct mtimer		tl;
	struct _layer3		*l3;
	unsigned int		pid;
	unsigned int		nr;
};

struct l2l3if;

struct _l3_process {
	struct list_head	list;
	struct _layer3		*L3;
	struct l2l3if		*l2if;
	l3_process_t		*master;
	struct list_head	child;
	unsigned long		flags;
	int			pid;
	int			selpid;
	int			state;
	struct L3Timer		timer1;
	struct L3Timer		timer2;
	int			n303;
	struct l3_msg		*t303msg;
	unsigned char		cid[4];
	int			cause;
	int			rm_cause;
	int			aux_state;
};

#define FLG_L3P_TIMER312	1
#define FLG_L3P_TIMER303_1	2
#define FLG_L3P_TIMER308_1	3
#define FLG_L3P_TIMER309	4
#define FLG_L3P_GOTRELCOMP	5

/*
 * maximum orginated callrefs
 * have to be a multiple of 8
 */
#define	MAX_PID_COUNT	128

struct l2l3if {
	struct list_head	list;
	struct _layer3		*l3;
	struct sockaddr_mISDN 	l2addr;
	struct	FsmInst		l3m;
	struct	FsmTimer	l3m_timer;
	struct mqueue		squeue;
};

typedef struct _layer3 {
	struct mlayer3		ml3;
	pthread_t		worker;
	int			l2sock;
	int			mdev;
	int			maxfd;
	struct l2l3if		l2master;
	struct list_head	pending_timer;
	int			next_cr;
	struct list_head	plist;
	l3_process_t		global;
	l3_process_t		dummy;
	int			(*p_mgr)(l3_process_t *, u_int, struct l3_msg *);
	int			(*from_l2)(struct _layer3 *, struct mbuffer *);
	int			(*to_l3)(struct _layer3 *, struct l3_msg *);
	int			debug;
	struct mqueue		app_queue;
	pthread_mutex_t		run;
} layer3_t;

struct stateentry {
	unsigned int	state;
	unsigned int	primitive;
	void		(*rout) (l3_process_t *, unsigned int, struct l3_msg *);
};

extern l3_process_t	*get_l3process4pid(layer3_t *, unsigned int);
extern l3_process_t	*get_l3process4cref(layer3_t *, unsigned int);
extern l3_process_t	*create_new_process(layer3_t *, unsigned int, unsigned int, l3_process_t *);
extern void 		release_l3_process(struct _l3_process *);
extern void		SendMsg(struct _l3_process *, struct l3_msg *, int);
extern void 		mISDN_l3up(l3_process_t *, unsigned int, struct l3_msg *);
extern void		l3_debug(layer3_t *, char *, ...);

static inline void
newl3state(l3_process_t *pc, int state)
{
	pc->state = state;
}

static inline struct l3_msg *
MsgStart(l3_process_t *pc, unsigned char mt)
{
	struct l3_msg	*l3m;
	struct mbuffer	*mb;

	l3m = alloc_l3_msg();
	if (!l3m)
		return NULL;
	mb = container_of(l3m, struct mbuffer, l3);
	l3m->type = mt;
	mb->l3h.type = mt;
	l3m->pid = pc->pid;
	return(l3m);
}

/* L3 timer functions */
extern void		StopAllL3Timer(struct _l3_process *);
extern void		L3AddTimer(struct L3Timer *, int, unsigned int);
extern void		L3DelTimer(struct L3Timer *);
extern void		l3_manager(struct l2l3if *, unsigned int);

extern int		parseQ931(struct mbuffer *);
extern int		assembleQ931(struct _l3_process *, struct l3_msg *);
extern int		l3_ie2pos(u_char);
extern unsigned char	l3_pos2ie(int);

#define SBIT(state)	(1 << state)
#define ALL_STATES	0x03ffffff

/*
 * internal used property flags
 * 0...15 reserved for set properties
 */

#define FLG_USER	16
#define FLG_NETWORK	17
#define FLG_BASICRATE	18
#define FLG_L2BLOCK	19
#define FLG_RUN_WAIT	30
#define FLG_ABORT	31

extern void		init_l3(layer3_t *);
extern void		release_l3(layer3_t *);
extern int		l3_start(struct _layer3 *);
extern void 		l3_stop(struct _layer3 *);
extern void		mISDNl3New(void);
extern void		mISDNl3Free(void);

#endif
