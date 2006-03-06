#ifndef ISDN_NET_H
#define ISDN_NET_H

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "mISDNlib.h"
#include "isdn_msg.h"
#include "isdn_debug.h"
#include "ibuffer.h"

#define MSN_LEN		32
#define	SUBADR_LEN	24
#define UUS_LEN		256
#define FAC_LEN		132
#ifndef mISDN_FRAME_MIN
#define mISDN_FRAME_MIN	8
#endif

typedef struct _layer2		layer2_t;
typedef struct _layer3		layer3_t;
typedef struct _layer4		layer4_t;
typedef struct _bchannel	bchannel_t;
typedef struct _mISDNif		mISDNif_t;
typedef struct _mISDNinstance	mISDNinstance_t;
typedef struct _net_stack	net_stack_t;
typedef struct _manager		manager_t;
typedef struct _nr_list		nr_list_t;
typedef int (*ifunc_t)(net_stack_t *, msg_t *);
typedef int (*bfunc_t)(void *, void *);
typedef int (*afunc_t)(manager_t *, int, void *);


#define	MAX_BDATA_SIZE	2048

struct _bchannel {
	sem_t			work;
	msg_queue_t		workq;
	pthread_t		tid;
	manager_t		*manager;
	void			*app;
	int			channel;
	pthread_mutex_t		lock;
	int			cstate;
	int			bstate;
	int			l3id;
	int			b_addr;
	int			Flags;
	int			ttime;
	nr_list_t		*usednr;
	int			l1_prot;
	unsigned char		bc[8];
	unsigned char		uu[UUS_LEN];
	unsigned char		fac[FAC_LEN];
	unsigned char		nr[MSN_LEN];
	unsigned char		msn[MSN_LEN];
	unsigned char		clisub[SUBADR_LEN];
	unsigned char		cldsub[SUBADR_LEN];
	int			cause_loc;
	int			cause_val;
	unsigned char		display[84];
	msg_t			*smsg;
	ibuffer_t		*rbuf;
	ibuffer_t		*sbuf;
	int			rrid;
	int			rsid;
};

struct _manager	{
	manager_t		*prev;
	manager_t		*next;
	bchannel_t		bc[2];
	nr_list_t		*nrlist;
	net_stack_t		*nst;
	bfunc_t			man2stack;
	afunc_t			application;
	afunc_t			app_bc;
	pthread_t		tid;
	sem_t			work;
	msg_queue_t		workq;
};

#define PR_APP_CHECK_NR		1
#define PR_APP_ICALL		2
#define PR_APP_OCHANNEL		3
#define PR_APP_OCALL		4
#define PR_APP_ALERT		5
#define PR_APP_CONNECT		6
#define PR_APP_HANGUP		7
#define PR_APP_CLEAR		8
#define PR_APP_USERUSER		9
#define PR_APP_FACILITY		10
#define PR_APP_OPEN_RECFILES	11
#define PR_APP_CLOSE_RECFILES	12

#define FLG_NST_READER_ABORT	1
#define FLG_NST_TERMINATION	2

#define FEATURE_NET_HOLD	0x00000001
#define FEATURE_NET_PTP		0x00000002
#define FEATURE_NET_CRLEN2	0x00000004
#define FEATURE_NET_EXTCID	0x00000008

struct _net_stack {
	int			device;
	int			cardnr;
	int			d_stid;
	int			l0_id;
	int			l1_id;
	int			l2_id;
	msg_t			*phd_down_msg;
	layer2_t		*layer2;
	layer3_t		*layer3;
	ifunc_t			l1_l2;
	ifunc_t			l2_l3;
	ifunc_t			l3_l2;
	ifunc_t			manager_l3;
	bfunc_t			l3_manager;
	manager_t		*manager;
	msg_queue_t		down_queue;
	msg_queue_t		rqueue;
	msg_queue_t		wqueue;
	sem_t			work;
	pthread_mutex_t		lock;
	pthread_t		reader;
	int			b_stid[2];
	int			b_addr[2];
	int			bcid[2];
	u_long			flag;
	struct _itimer		*tlist;
	void			*l2fsm;
	void			*teifsm;
	u_long			feature;
};

struct _nr_list {
	nr_list_t		*prev;
	nr_list_t		*next;
	unsigned char		len;
	unsigned char		nr[MSN_LEN];
	unsigned char		name[64];
	int			typ;
	int			flags;
};

#define NR_TYPE_INTERN		1
#define NR_TYPE_AUDIO		2
#define NR_TYPE_VOIP		3

typedef struct _itimer {
	struct _itimer		*prev;
	struct _itimer		*next;
	net_stack_t		*nst;
	int			id;
	int			expires;
	u_long			Flags;
	unsigned long		data;
	int			(*function)(unsigned long);
} itimer_t;

#define FLG_TIMER_RUNING	1

#define FLG_BC_USE		0x00000001
#define FLG_BC_SENT_CID		0x00000002
#define FLG_BC_CALL_ORGINATE	0x00000004
#define FLG_BC_PROGRESS		0x00000008
#define	FLG_BC_APPLICATION	0x00000010
#define FLG_BC_TONE_DIAL	0x00000100
#define FLG_BC_TONE_BUSY	0x00000200
#define FLG_BC_TONE_ALERT	0x00000400
#define FLG_BC_TONE_SILENCE	0x00000800
#define FLG_BC_TONE_NONE	0x00000000
#define FLG_BC_TONE		0x00000F00
#define FLG_BC_RECORD		0x00010000
#define FLG_BC_RECORDING	0x00020000
#define FLG_BC_RAWDEVICE	0x01000000
#define FLG_BC_KEEP_SEND	0x02000000
#define FLG_BC_TERMINATE	0x08000000

#define MSG_L1_PRIM		0x010000
#define MSG_L2_PRIM		0x020000
#define MSG_L3_PRIM		0x030000

extern	int		do_net_stack_setup(net_stack_t *);
extern	int		do_net_stack_cleanup(net_stack_t  *nst);
extern	void		*do_netthread(void *);
extern	int		term_netstack(net_stack_t *nst);

extern	int		init_manager(manager_t **mlist, afunc_t application);
extern	int		cleanup_manager(manager_t *mgr);

extern	int		write_dmsg(net_stack_t *, msg_t *);
extern	int		phd_conf(net_stack_t *, iframe_t *, msg_t *);

extern	int		init_timer(itimer_t *, net_stack_t  *);
extern	int		add_timer(itimer_t *);
extern	int		del_timer(itimer_t *);
extern	int		remove_timer(itimer_t *it);
extern	int		timer_pending(itimer_t *);

extern	u_char		*findie(u_char *, int, u_char, int);
extern	u_char		*find_and_copy_ie(u_char *, int, u_char, int, msg_t *);
extern	void		display_NR_IE(u_char *, char *, char *);

extern	int		match_nr(manager_t *mgr, unsigned char *nx, nr_list_t **nrx);

typedef struct _mISDNuser_head {
	u_int	prim;
	int	dinfo;
} mISDNuser_head_t;

#define mISDNUSER_HEAD_SIZE	sizeof(mISDNuser_head_t)

/* interface msg help routines */

static inline void mISDN_newhead(u_int prim, int dinfo, msg_t *msg)
{
	mISDNuser_head_t *hh = (mISDNuser_head_t *)msg->data;

	hh->prim = prim;
	hh->dinfo = dinfo;
}

static inline int if_newhead(void *arg, ifunc_t func, u_int prim, int dinfo,
	msg_t *msg)
{
	if (!msg)
		return(-ENXIO);
	mISDN_newhead(prim, dinfo, msg);
	return(func((net_stack_t *)arg, msg));
}

static inline void mISDN_addhead(u_int prim, int dinfo, msg_t *msg)
{
	mISDNuser_head_t *hh = (mISDNuser_head_t *)msg_push(msg, mISDNUSER_HEAD_SIZE);

	hh->prim = prim;
	hh->dinfo = dinfo;
}


static inline int if_addhead(void *arg, ifunc_t func, u_int prim, int dinfo,
	msg_t *msg)
{
	if (!msg)
		return(-ENXIO);
	mISDN_addhead(prim, dinfo, msg);
	return(func((net_stack_t *)arg, msg));
}


static inline msg_t *create_link_msg(u_int prim, int dinfo,
	int len, void *arg, int reserve)
{
	msg_t	*msg;

	if (!(msg = alloc_msg(len + mISDNUSER_HEAD_SIZE + reserve))) {
		wprint("%s: no msg size %d+%d+%d\n", __FUNCTION__,
			len, mISDNUSER_HEAD_SIZE, reserve);
		return(NULL);
	} else
		msg_reserve(msg, reserve + mISDNUSER_HEAD_SIZE);
	if (len)
		memcpy(msg_put(msg, len), arg, len);
	mISDN_addhead(prim, dinfo, msg);
	return(msg);
}

static inline int if_link(void *farg, ifunc_t func, u_int prim, int dinfo, int len,
	void *arg, int reserve)
{
	msg_t	*msg;
	int	err;

	if (!(msg = create_link_msg(prim, dinfo, len, arg, reserve)))
		return(-ENOMEM);
	err = func((net_stack_t *)farg, msg);
	if (err)
		free_msg(msg);
	return(err);
}

static inline msg_t *prep_l3data_msg(u_int prim, int dinfo, int ssize, int dsize, msg_t *old)
{
	if (!old) {
		old = alloc_msg(ssize + dsize + mISDNUSER_HEAD_SIZE + DEFAULT_HEADROOM);
		if (!old) {
			wprint("%s: no msg size %d+%d+%d\n", __FUNCTION__,
				ssize, dsize, mISDNUSER_HEAD_SIZE + DEFAULT_HEADROOM);
			return(NULL);
		}
	} else {
		old->data = old->head + DEFAULT_HEADROOM;
		old->tail = old->data;
		old->len = 0;
	}
	memset(msg_put(old, ssize + mISDNUSER_HEAD_SIZE), 0,
		ssize + mISDNUSER_HEAD_SIZE);
	mISDN_newhead(prim, dinfo, old);
	return(old);
}

#endif
