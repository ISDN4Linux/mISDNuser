/* $Id: net_l2.h,v 0.9.1.1 2003/08/27 07:33:03 kkeil Exp $
 *
 * Layer 2 defines
 *
 * This file is (c) under GNU PUBLIC LICENSE
 *
 */

#ifndef NET_L2_H
#define NET_L2_H

#include <asm/bitops.h>
#include "mISDNlib.h"
#include "isdn_net.h"
#include "fsm.h"
#ifdef MEMDBG
#include "memdbg.h"
#endif

#define MAX_WINDOW	8

typedef struct _teimgr {
	int		ri;
	struct FsmInst	tei_m;
	struct FsmTimer	t201;
	int		T201;
	int		debug;
	int		val;
	struct _layer2	*l2;
} teimgr_t;

struct _layer2 {
	struct _layer2	*prev;
	struct _layer2	*next;
	int		sapi;
	int		tei;
	laddr_t		addr;
	int		maxlen;
	teimgr_t	*tm;
	u_int		flag;
	u_int		vs, va, vr;
	int		rc;
	u_int		window;
	u_int		sow;
	struct FsmInst	l2m;
	struct FsmTimer	t200, t203;
	int		T200, N200, T203;
	int		debug;
	msg_t	*windowar[MAX_WINDOW];
	net_stack_t	*nst;
	msg_queue_t i_queue;
	msg_queue_t ui_queue;
};

#define SAPITEI(ces)	(ces>>8)&0xff, ces&0xff

static inline int CES(layer2_t *l2) {
	return(l2->tei | (l2->sapi << 8));
}

/* from mISDN_l2.c */
extern layer2_t	*new_dl2(net_stack_t *nst, int tei);
extern int	tei_l2(layer2_t *l2, msg_t *msg);
extern int	Isdnl2Init(net_stack_t *nst);
extern void	cleanup_Isdnl2(net_stack_t *nst);


/* from tei.c */
extern int tei_mux(net_stack_t *nst, msg_t *msg);
extern int l2_tei(teimgr_t *tm, msg_t *msg);
extern int create_teimgr(layer2_t *l2);
extern void release_tei(teimgr_t *tm);
extern int TEIInit(void);
extern void TEIFree(void);

#define GROUP_TEI	127
#define TEI_SAPI	63
#define CTRL_SAPI	0

#define RR	0x01
#define RNR	0x05
#define REJ	0x09
#define SABME	0x6f
#define SABM	0x2f
#define DM	0x0f
#define UI	0x03
#define DISC	0x43
#define UA	0x63
#define FRMR	0x87
#define XID	0xaf

#define CMD	0
#define RSP	1

#define LC_FLUSH_WAIT 1

#define FLG_LAPB	0
#define FLG_LAPD	1
#define FLG_ORIG	2
#define FLG_MOD128	3
#define FLG_PEND_REL	4
#define FLG_L3_INIT	5
#define FLG_T200_RUN	6
#define FLG_ACK_PEND	7
#define FLG_REJEXC	8
#define FLG_OWN_BUSY	9
#define FLG_PEER_BUSY	10
#define FLG_DCHAN_BUSY	11
#define FLG_L1_ACTIV	12
#define FLG_ESTAB_PEND	13
#define FLG_PTP		14
#define FLG_FIXED_TEI	15
#define FLG_L2BLOCK	16
#define FLG_L1_BUSY	17
#define FLG_LAPD_NET	18
#define FLG_TEI_T201_1	19

#endif
