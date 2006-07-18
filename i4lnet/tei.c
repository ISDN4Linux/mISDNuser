/* $Id: tei.c,v 1.5 2006/07/18 13:50:03 crich Exp $
 *
 * Author       Karsten Keil (keil@isdn4linux.de)
 *
 *		This file is (c) under GNU PUBLIC LICENSE
 *		For changes and modifications please read
 *		../../../Documentation/isdn/mISDN.cert
 *
 */
#define __NO_VERSION__
#include <stdlib.h>
#include "net_l2.h"
// #include "helper.h"
// #include "debug.h"
// #include <linux/random.h>

const char *tei_revision = "$Revision: 1.5 $";

#define ID_REQUEST	1
#define ID_ASSIGNED	2
#define ID_DENIED	3
#define ID_CHK_REQ	4
#define ID_CHK_RES	5
#define ID_REMOVE	6
#define ID_VERIFY	7

#define TEI_ENTITY_ID	0xf

enum {
	ST_TEI_NOP,
	ST_TEI_REMOVE,
	ST_TEI_IDVERIFY,
};

#define TEI_STATE_COUNT (ST_TEI_IDVERIFY+1)

static char *strTeiState[] =
{
	"ST_TEI_NOP",
	"ST_TEI_REMOVE",
	"ST_TEI_IDVERIFY",
};

enum {
	EV_IDREQ,
	EV_ASSIGN,
	EV_ASSIGN_REQ,
	EV_CHECK_RES,
	EV_CHECK_REQ,
	EV_REMOVE,
	EV_VERIFY,
	EV_T201,
};

#define TEI_EVENT_COUNT (EV_T201+1)

static char *strTeiEvent[] =
{
	"EV_IDREQ",
	"EV_ASSIGN",
	"EV_ASSIGN_REQ",
	"EV_CHECK_RES",
	"EV_CHECK_REQ",
	"EV_REMOVE",
	"EV_VERIFY",
	"EV_T201",
};

static layer2_t
*new_tei_req(net_stack_t *nst)
{
	layer2_t	*l2;
	int		tei;

	for (tei=64;tei<127;tei++) {
		l2 = nst->layer2;
		while(l2) {
			if (l2->tei == tei)
				break;
			l2 = l2->next;
		}
		if (!l2)
			break;
	}
	if (tei==127) /* all tei in use */
		return(NULL);
	l2 = new_dl2(nst, tei);
	return(l2);
}

unsigned int
random_ri(void)
{
	long int x;

	x = random();
	return (x & 0xffff);
}

static layer2_t *
find_tei(net_stack_t *nst, int tei)
{
	layer2_t	*l2;

	l2 = nst->layer2;
	while(l2) {
		if (l2->tei == tei)
			break;
		l2 = l2->next;
	}
	return(l2);
}

static void
put_tei_msg(teimgr_t *tm, u_char m_id, unsigned int ri, u_char tei)
{
	msg_t *msg;
	u_char bp[8];

	bp[0] = (TEI_SAPI << 2);
	if (test_bit(FLG_LAPD_NET, &tm->l2->flag))
		bp[0] |= 2; /* CR:=1 for net command */
	bp[1] = (GROUP_TEI << 1) | 0x1;
	bp[2] = UI;
	bp[3] = TEI_ENTITY_ID;
	bp[4] = ri >> 8;
	bp[5] = ri & 0xff;
	bp[6] = m_id;
	bp[7] = (tei << 1) | 1;
	msg = create_link_msg(MDL_UNITDATA | REQUEST, DINFO_SKB, 8, bp, 0);
	if (!msg) {
		dprint(DBGM_TEI, -1, "mISDN: No msg for TEI manager\n");
		return;
	}
	if (tei_l2(tm->l2, msg))
		free_msg(msg);
}

static void
tei_assign_req(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	u_char *dp = arg;

	if (tm->l2->tei == -1) {
		tm->tei_m.printdebug(&tm->tei_m,
			"net tei assign request without tei");
		return;
	}
	tm->ri = ((unsigned int) *dp++ << 8);
	tm->ri += *dp++;
	if (tm->debug)
		tm->tei_m.printdebug(&tm->tei_m,
			"net assign request ri %d teim %d", tm->ri, *dp);
	put_tei_msg(tm, ID_ASSIGNED, tm->ri, tm->l2->tei);
	FsmChangeState(fi, ST_TEI_NOP);
}

static void
tei_id_chk_res(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;
	int	*ri = arg;

	
	if (tm->debug)
		tm->tei_m.printdebug(fi, "identity %d check response ri %x/%x",
			tm->l2->tei, *ri, tm->ri);
	if (tm->ri != -1) {
		FsmDelTimer(&tm->t201, 4);
		tm->tei_m.printdebug(fi, "duplicat %d response", tm->l2->tei);
		tm->val = tm->l2->tei;
		put_tei_msg(tm, ID_REMOVE, 0, tm->val);
		FsmAddTimer(&tm->t201, tm->T201, EV_T201, NULL, 2);
		FsmChangeState(&tm->tei_m, ST_TEI_REMOVE);
	} else
		tm->ri = *ri;
}

static void
tei_id_remove(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t	*tm = fi->userdata;
	int		*tei = arg;

	if (tm->debug)
		tm->tei_m.printdebug(fi, "identity remove tei %d/%d", *tei, tm->l2->tei);
	tm->val = *tei;
	put_tei_msg(tm, ID_REMOVE, 0, tm->val);
	FsmAddTimer(&tm->t201, tm->T201, EV_T201, NULL, 2);
	FsmChangeState(&tm->tei_m, ST_TEI_REMOVE);
}

static void
tei_id_verify(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;

	if (tm->debug)
		tm->tei_m.printdebug(fi, "id verify request for tei %d",
			tm->l2->tei);
	tm->ri = -1;
	put_tei_msg(tm, ID_CHK_REQ, 0, tm->l2->tei);
	FsmChangeState(&tm->tei_m, ST_TEI_IDVERIFY);
	test_and_set_bit(FLG_TEI_T201_1, &tm->l2->flag);
	FsmAddTimer(&tm->t201, tm->T201, EV_T201, NULL, 2);
}

static void
tei_id_remove_tout(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;

	if (tm->debug)
		tm->tei_m.printdebug(fi, "remove req(2) tei %d",
			tm->l2->tei);
	put_tei_msg(tm, ID_REMOVE, 0, tm->val);
	FsmChangeState(fi, ST_TEI_NOP);
}

static void
tei_id_ver_tout(struct FsmInst *fi, int event, void *arg)
{
	teimgr_t *tm = fi->userdata;

	if (tm->debug)
		tm->tei_m.printdebug(fi, "verify tout tei %d",
			tm->l2->tei);
	if (test_and_clear_bit(FLG_TEI_T201_1, &tm->l2->flag)) {
		put_tei_msg(tm, ID_CHK_REQ, 0, tm->l2->tei);
		tm->ri = -1;
		FsmAddTimer(&tm->t201, tm->T201, EV_T201, NULL, 3);
	} else {
		FsmChangeState(fi, ST_TEI_NOP);
		if (tm->ri == -1) {
			tm->tei_m.printdebug(fi, "tei %d check no response",
				tm->l2->tei);
			// remove tei
		} else
			tm->tei_m.printdebug(fi, "tei %d check ok",
				tm->l2->tei);
	}
}

int
l2_tei(teimgr_t *tm, msg_t *msg)
{
	mISDNuser_head_t	*hh;
	int		ret = -EINVAL;

	if (!tm || !msg)
		return(ret);
	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_TEI, -1, "%s: prim(%x)\n", __FUNCTION__, hh->prim);
	if (msg->len < mISDN_FRAME_MIN)
		return(ret);
	switch(hh->prim) {
	    case (MDL_REMOVE | INDICATION):
		FsmEvent(&tm->tei_m, EV_REMOVE, &hh->dinfo);
		break;
	    case (MDL_ERROR | REQUEST):
	    	if (!test_bit(FLG_FIXED_TEI, &tm->l2->flag))
			FsmEvent(&tm->tei_m, EV_VERIFY, NULL);
		break;
	}
	free_msg(msg);
	return(0);
}

static void
tei_debug(struct FsmInst *fi, char *fmt, ...)
{
	teimgr_t	*tm = fi->userdata;
	char		tbuf[128];
	va_list 	args;

	va_start(args, fmt);
	vsprintf(tbuf, fmt, args);
	dprint(DBGM_L2, -1, "tei%d %s\n", tm->l2->tei, tbuf);
	va_end(args);
}

static struct FsmNode TeiFnList[] =
{
	{ST_TEI_NOP, EV_ASSIGN_REQ, tei_assign_req},
	{ST_TEI_NOP, EV_VERIFY, tei_id_verify},
	{ST_TEI_NOP, EV_REMOVE, tei_id_remove},
	{ST_TEI_REMOVE, EV_T201, tei_id_remove_tout},
	{ST_TEI_IDVERIFY, EV_T201, tei_id_ver_tout},
	{ST_TEI_IDVERIFY, EV_REMOVE, tei_id_remove},
	{ST_TEI_IDVERIFY, EV_CHECK_RES, tei_id_chk_res},
};

#define TEI_FN_COUNT (sizeof(TeiFnList)/sizeof(struct FsmNode))

void
release_tei(teimgr_t *tm)
{
	FsmDelTimer(&tm->t201, 1);
	free(tm);
}

int
create_teimgr(layer2_t *l2) {
	teimgr_t *ntei;

	if (!l2) {
		eprint("create_tei no layer2\n");
		return(-EINVAL);
	}
	if (!(ntei = malloc(sizeof(teimgr_t)))) {
		eprint("kmalloc teimgr failed\n");
		return(-ENOMEM);
	}
	memset(ntei, 0, sizeof(teimgr_t));
	ntei->l2 = l2;
	ntei->T201 = 1000;	/* T201  1000 milliseconds */
	ntei->debug = l2->debug;
	ntei->tei_m.nst = l2->nst;
	ntei->tei_m.debug = l2->debug;
	ntei->tei_m.userdata = ntei;
	ntei->tei_m.printdebug = tei_debug;
	ntei->tei_m.fsm = l2->nst->teifsm;
	ntei->tei_m.state = ST_TEI_NOP;
	FsmInitTimer(&ntei->tei_m, &ntei->t201);
	l2->tm = ntei;
	return(0);
}

int
tei_mux(net_stack_t *nst, msg_t *msg)
{
	mISDNuser_head_t	*hh;
	u_char		*dp;
	int 		mt;
	layer2_t	*l2;
	unsigned int	ri, ai;

	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_TEI, -1, "%s: prim(%x) len(%d)\n", __FUNCTION__,
		hh->prim, msg->len);
	if (msg->len < mISDN_FRAME_MIN)
		return(-EINVAL);
	if (hh->prim != (MDL_UNITDATA | INDICATION)) {
		wprint("%s: prim(%x) unhandled\n", __FUNCTION__,
			hh->prim);
		return(-EINVAL);
	}
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	if (msg->len < 8) {
		wprint("short tei mgr frame %d/8\n", msg->len);
		return(-EINVAL);
	}
	dp = msg->data + 2;
	if ((*dp & 0xef) != UI) {
		wprint("tei mgr frame is not ui %x\n", *dp);
		return(-EINVAL);
	}
	dp++;
	if (*dp++ != TEI_ENTITY_ID) {
		/* wrong management entity identifier, ignore */
		dp--;
		wprint("tei handler wrong entity id %x\n", *dp);
		return(-EINVAL);
	} else {
		mt = *(dp+2);
		ri = ((unsigned int) *dp++ << 8);
		ri += *dp++;
		dp++;
		ai = (unsigned int) *dp++;
		ai >>= 1;
		dprint(DBGM_TEI, -1, "tei handler mt %x ri(%x) ai(%d)\n",
			mt, ri, ai);
		if (mt == ID_REQUEST) {
			if (ai != 127) {
				wprint("%s: ID_REQUEST ai(%d) not 127\n", __FUNCTION__,
					ai);
				return(-EINVAL);
			}
			l2 = new_tei_req(nst);
			if (!l2) {
				wprint("%s: no free tei\n", __FUNCTION__);
				return(-EBUSY);
			}
			l2->tm->ri = ri;
			put_tei_msg(l2->tm, ID_ASSIGNED, ri, l2->tei);
			free_msg(msg);
			return(0);
		}
		l2 = find_tei(nst, ai);
		if (mt == ID_VERIFY) {
			if (l2) {
				FsmEvent(&l2->tm->tei_m, EV_VERIFY, &ai);
			} else {
				l2 = find_tei(nst, 127);
				if (!l2) {
					wprint("%s: no 127 manager\n", __FUNCTION__);
					return(-EINVAL);
				}
				FsmEvent(&l2->tm->tei_m, EV_REMOVE, &ai);
			}
		} else if (mt == ID_CHK_RES) {
			if (l2) {
				FsmEvent(&l2->tm->tei_m, EV_CHECK_RES, &ri);
			} else {
				l2 = find_tei(nst, 127);
				if (!l2) {
					wprint("%s: no 127 manager\n", __FUNCTION__);
					return(-EINVAL);
				}
				FsmEvent(&l2->tm->tei_m, EV_REMOVE, &ai);
			}
		} else {
			wprint("%s: wrong mt %x", __FUNCTION__, mt);
			return(-EINVAL);
		}
	}
	free_msg(msg);
	return(0);
}



int TEIInit(net_stack_t *nst)
{
	struct Fsm *teif;

	if (!(teif = malloc(sizeof(struct Fsm))))
		return(1);
	nst->teifsm = teif;
	memset(teif, 0, sizeof(struct Fsm));
	teif->state_count = TEI_STATE_COUNT;
	teif->event_count = TEI_EVENT_COUNT;
	teif->strEvent = strTeiEvent;
	teif->strState = strTeiState;
	FsmNew(teif, TeiFnList, TEI_FN_COUNT);
	return(0);
}

void TEIFree(net_stack_t *nst)
{
	FsmFree(nst->teifsm);
}
