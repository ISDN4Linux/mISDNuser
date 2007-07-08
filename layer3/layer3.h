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

struct _l3_process {
	struct list_head	list;
	u_int			id;
	struct _layer3		*l3;
	int			pid; /* callref */
	int			state;
	struct L3Timer		timer;
	int			n303;
	struct l3_msg		*t303msg;
	unsigned char		cid[4];
	int			cause;
	int			rm_cause;
	int			aux_state;
	struct L3Timer		aux_timer;
};

/*
 * maximum orginated callrefs
 * have to be a multiple of 8
 */
#define	MAX_PID_COUNT	128

typedef struct _layer3 {
	struct mlayer3		ml3;
	pthread_t		worker;
	struct sockaddr_mISDN	l2addr;
	int			l2sock;
	int			mdev;
	int			maxfd;
	struct list_head	pending_timer;
	struct FsmInst		l3m;
	struct FsmTimer		l3m_timer;
	int			pid_cnt;
	int			next_id;
	unsigned char		pids[MAX_PID_COUNT/8];
	struct list_head	plist;
	l3_process_t		global;
	l3_process_t		dummy;
	int			(*p_mgr)(l3_process_t *, u_int, struct l3_msg *);
	int			(*from_l2)(struct _layer3 *, struct mbuffer *);
	int			(*to_l3)(struct _layer3 *, struct l3_msg *);
	int			debug;
	struct mqueue		squeue;
	struct mqueue		app_queue;
} layer3_t;

struct stateentry {
	unsigned int	state;
	unsigned int	primitive;
	void		(*rout) (l3_process_t *, u_char, struct l3_msg *);
};

extern l3_process_t	*get_l3process4pid(layer3_t *, unsigned int);
extern l3_process_t	*get_l3process4cref(layer3_t *, unsigned int);
extern l3_process_t	*create_new_process(layer3_t *, unsigned int);
extern void 		release_l3_process(struct _l3_process *);
extern void		SendMsg(struct _l3_process *, struct l3_msg *, int);
extern void 		mISDN_l3up(l3_process_t *, unsigned int, struct l3_msg *);

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
extern void		l3_manager(layer3_t *, unsigned int);

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
 
#define FLG_BASICRATE	16
#define FLG_L2BLOCK	17
#define FLG_ABORT	31

extern void		init_l3(layer3_t *);
extern void		release_l3(layer3_t *);
extern int		l3_start(struct _layer3 *);
extern void 		l3_stop(struct _layer3 *);
extern void		mISDNl3New(void);
extern void		mISDNl3Free(void);

/* TEMORARY */
extern int		dss1_user_fromup(struct mlayer3 *, unsigned int, unsigned int, struct l3_msg *l3m);

#endif
