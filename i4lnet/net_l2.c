/* $Id: net_l2.c,v 1.10 2006/08/09 10:29:44 crich Exp $
 *
 * Author       Karsten Keil (keil@isdn4linux.de)
 *
 *		This file is (c) under GNU PUBLIC LICENSE
 *		For changes and modifications please read
 *		../../../Documentation/isdn/mISDN.cert
 *
 */

#include <stdlib.h>
#include "net_l2.h"
#include "helper.h"
// #include "debug.h"

const char *l2_revision = "$Revision: 1.10 $";

static void l2m_debug(struct FsmInst *fi, char *fmt, ...);

static int debug = 0xff;

enum {
	ST_L2_1,
	ST_L2_2,
	ST_L2_3,
	ST_L2_4,
	ST_L2_5,
	ST_L2_6,
	ST_L2_7,
	ST_L2_8,
};

#define L2_STATE_COUNT (ST_L2_8+1)

enum {
	EV_L2_UI,
	EV_L2_SABME,
	EV_L2_DISC,
	EV_L2_DM,
	EV_L2_UA,
	EV_L2_FRMR,
	EV_L2_SUPER,
	EV_L2_I,
	EV_L2_DL_DATA,
	EV_L2_ACK_PULL,
	EV_L2_DL_UNITDATA,
	EV_L2_DL_ESTABLISH_REQ,
	EV_L2_DL_RELEASE_REQ,
	EV_L2_MDL_ASSIGN,
	EV_L2_MDL_REMOVE,
	EV_L2_MDL_ERROR,
	EV_L1_DEACTIVATE,
	EV_L2_T200,
	EV_L2_T203,
	EV_L2_SET_OWN_BUSY,
	EV_L2_CLEAR_OWN_BUSY,
	EV_L2_FRAME_ERROR,
};

#define L2_EVENT_COUNT (EV_L2_FRAME_ERROR+1)

static char *strL2Event[] =
{
	"EV_L2_UI",
	"EV_L2_SABME",
	"EV_L2_DISC",
	"EV_L2_DM",
	"EV_L2_UA",
	"EV_L2_FRMR",
	"EV_L2_SUPER",
	"EV_L2_I",
	"EV_L2_DL_DATA",
	"EV_L2_ACK_PULL",
	"EV_L2_DL_UNITDATA",
	"EV_L2_DL_ESTABLISH_REQ",
	"EV_L2_DL_RELEASE_REQ",
	"EV_L2_MDL_ASSIGN",
	"EV_L2_MDL_REMOVE",
	"EV_L2_MDL_ERROR",
	"EV_L1_DEACTIVATE",
	"EV_L2_T200",
	"EV_L2_T203",
	"EV_L2_SET_OWN_BUSY",
	"EV_L2_CLEAR_OWN_BUSY",
	"EV_L2_FRAME_ERROR",
};

static int l2addrsize(layer2_t *l2);

static int
l2up(layer2_t *l2, u_int prim, int dinfo, msg_t *msg)
{
	return(if_newhead(l2->nst, l2->nst->l2_l3, prim, dinfo, msg));
}

static int
l2up_create(layer2_t *l2, u_int prim, int dinfo, int len, void *arg)
{
	return(if_link(l2->nst, l2->nst->l2_l3, prim, dinfo, len, arg, 0));
}

static int
l2down_msg(layer2_t *l2, msg_t *msg) {
	int ret;

	ret = write_dmsg(l2->nst, msg);
	if (ret)
		dprint(DBGM_L2, l2->nst->cardnr, "l2down_msg: error %d\n", ret);
	return(ret);
}

static int
l2down(layer2_t *l2, u_int prim, int dinfo, msg_t *msg)
{
	mISDN_newhead(prim, dinfo, msg);
	return(l2down_msg(l2, msg));
}

static int
l2down_create(layer2_t *l2, u_int prim, int dinfo, int len, void *arg)
{
	msg_t	*msg;
	int		err;

	msg = create_link_msg(prim, dinfo, len, arg, 0);
	if (!msg)
		return(-ENOMEM);
	err = l2down_msg(l2, msg);
	if (err)
		free_msg(msg);
	return(err);
}

static int
l2mgr(layer2_t *l2, u_int prim, void *arg) {
	long c = (long)arg;

	dprint(DBGM_L2, l2->nst->cardnr, "l2mgr: prim %x %c\n", prim, (char)c);
      l2->nst->phd_down_msg=NULL;
      msg_queue_purge(&l2->nst->down_queue);
	return(0);
}

static void
set_peer_busy(layer2_t *l2) {
	test_and_set_bit(FLG_PEER_BUSY, &l2->flag);
	dprint(DBGM_L2, l2->nst->cardnr, "Peer Busy\n");
	if (msg_queue_len(&l2->i_queue) || msg_queue_len(&l2->ui_queue))
		test_and_set_bit(FLG_L2BLOCK, &l2->flag);
}

static void
clear_peer_busy(layer2_t *l2) {
	dprint(DBGM_L2, l2->nst->cardnr, "Clear Peer Busy\n");
	if (test_and_clear_bit(FLG_PEER_BUSY, &l2->flag))
		test_and_clear_bit(FLG_L2BLOCK, &l2->flag);
}

static void
InitWin(layer2_t *l2)
{
	int i;

	for (i = 0; i < MAX_WINDOW; i++)
		l2->windowar[i] = NULL;
}

static int
freewin(layer2_t *l2)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_WINDOW; i++) {
		if (l2->windowar[i]) {
			cnt++;
			free_msg(l2->windowar[i]);
			l2->windowar[i] = NULL;
		}
	}
	return cnt;
}

static void
ReleaseWin(layer2_t *l2)
{
	int cnt;

	if((cnt = freewin(l2)))
		dprint(DBGM_L2, l2->nst->cardnr, "isdnl2 freed %d msguffs in release\n", cnt);
}

inline unsigned int
cansend(layer2_t *l2)
{
	unsigned int p1;

	if(test_bit(FLG_MOD128, &l2->flag))
		p1 = (l2->vs - l2->va) % 128;
	else
		p1 = (l2->vs - l2->va) % 8;
	return ((p1 < l2->window) && !test_bit(FLG_PEER_BUSY, &l2->flag));
}

inline void
clear_exception(layer2_t *l2)
{
	test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
	test_and_clear_bit(FLG_REJEXC, &l2->flag);
	test_and_clear_bit(FLG_OWN_BUSY, &l2->flag);
	clear_peer_busy(l2);
}

inline int
l2headersize(layer2_t *l2, int ui)
{
	return (((test_bit(FLG_MOD128, &l2->flag) && (!ui)) ? 2 : 1) +
		(test_bit(FLG_LAPD, &l2->flag) ? 2 : 1));
}

inline int
l2addrsize(layer2_t *l2)
{
	return (test_bit(FLG_LAPD, &l2->flag) ? 2 : 1);
}

static int
sethdraddr(layer2_t *l2, u_char *header, int rsp)
{
	u_char *ptr = header;
	int crbit = rsp;

	if (test_bit(FLG_LAPD, &l2->flag)) {
		if (test_bit(FLG_LAPD_NET, &l2->flag))
			crbit = !crbit;
		*ptr++ = (l2->sapi << 2) | (crbit ? 2 : 0);
		*ptr++ = (l2->tei << 1) | 1;
		return (2);
	} else {
		if (test_bit(FLG_ORIG, &l2->flag))
			crbit = !crbit;
		if (crbit)
			*ptr++ = l2->addr.B;
		else
			*ptr++ = l2->addr.A;
		return (1);
	}
}

inline static void
enqueue_super(layer2_t *l2, msg_t *msg)
{
	if (l2down(l2, PH_DATA | REQUEST, DINFO_SKB, msg))
		free_msg(msg);
}

#define enqueue_ui(a, b) enqueue_super(a, b)

inline int
IsUI(u_char * data, layer2_t *l2)
{
	return ((data[0] & 0xef) == UI);
}

inline int
IsUA(u_char * data, layer2_t *l2)
{
	return ((data[0] & 0xef) == UA);
}

inline int
IsDM(u_char * data, layer2_t *l2)
{
	return ((data[0] & 0xef) == DM);
}

inline int
IsDISC(u_char * data, layer2_t *l2)
{
	return ((data[0] & 0xef) == DISC);
}

inline int
IsRR(u_char * data, layer2_t *l2)
{
	if (test_bit(FLG_MOD128, &l2->flag))
		return (data[0] == RR);
	else
		return ((data[0] & 0xf) == 1);
}

inline int
IsSFrame(u_char * data, layer2_t *l2)
{
	register u_char d = *data;
	
	if (!test_bit(FLG_MOD128, &l2->flag))
		d &= 0xf;
	return(((d & 0xf3) == 1) && ((d & 0x0c) != 0x0c));
}

inline int
IsSABME(u_char * data, layer2_t *l2)
{
	u_char d = data[0] & ~0x10;
	return (test_bit(FLG_MOD128, &l2->flag) ? d == SABME : d == SABM);
}

inline int
IsREJ(u_char * data, layer2_t *l2)
{
	return (test_bit(FLG_MOD128, &l2->flag) ? data[0] == REJ : (data[0] & 0xf) == REJ);
}

inline int
IsFRMR(u_char * data, layer2_t *l2)
{
	return ((data[0] & 0xef) == FRMR);
}

inline int
IsRNR(u_char * data, layer2_t *l2)
{
	return (test_bit(FLG_MOD128, &l2->flag) ? data[0] == RNR : (data[0] & 0xf) == RNR);
}

int
iframe_error(layer2_t *l2, msg_t *msg)
{
	int i = l2addrsize(l2) + (test_bit(FLG_MOD128, &l2->flag) ? 2 : 1);
	int rsp = *msg->data & 0x2;
	
	if (test_bit(FLG_ORIG, &l2->flag))
		rsp = !rsp;
	if (rsp)
		return 'L';
	if (msg->len < i)
		return 'N';
	if ((msg->len - i) > l2->maxlen)
		return 'O';
	return 0;
}

int
super_error(layer2_t *l2, msg_t *msg)
{
	if (msg->len != l2addrsize(l2) +
	    (test_bit(FLG_MOD128, &l2->flag) ? 2 : 1))
		return 'N';
	return 0;
}

int
unnum_error(layer2_t *l2, msg_t *msg, int wantrsp)
{
	int rsp = (*msg->data & 0x2) >> 1;
	if (test_bit(FLG_ORIG, &l2->flag))
		rsp = !rsp;
	if (rsp != wantrsp)
		return 'L';
	if (msg->len != l2addrsize(l2) + 1)
		return 'N';
	return 0;
}

int
UI_error(layer2_t *l2, msg_t *msg)
{
	int rsp = *msg->data & 0x2;
	if (test_bit(FLG_ORIG, &l2->flag))
		rsp = !rsp;
	if (rsp)
		return 'L';
	if (msg->len > l2->maxlen + l2addrsize(l2) + 1)
		return 'O';
	return 0;
}

int
FRMR_error(layer2_t *l2, msg_t *msg)
{
	int headers = l2addrsize(l2) + 1;
	u_char *datap = msg->data + headers;
	int rsp = *msg->data & 0x2;

	if (test_bit(FLG_ORIG, &l2->flag))
		rsp = !rsp;
	if (!rsp)
		return 'L';
	if (test_bit(FLG_MOD128, &l2->flag)) {
		if (msg->len < headers + 5)
			return 'N';
		else
			l2m_debug(&l2->l2m, "FRMR information %2x %2x %2x %2x %2x",
				datap[0], datap[1], datap[2],
				datap[3], datap[4]);
	} else {
		if (msg->len < headers + 3)
			return 'N';
		else
			l2m_debug(&l2->l2m, "FRMR information %2x %2x %2x",
				datap[0], datap[1], datap[2]);
	}
	return 0;
}

static unsigned int
legalnr(layer2_t *l2, unsigned int nr)
{
	if(test_bit(FLG_MOD128, &l2->flag))
		return ((nr - l2->va) % 128) <= ((l2->vs - l2->va) % 128);
	else
		return ((nr - l2->va) % 8) <= ((l2->vs - l2->va) % 8);
}

static void
setva(layer2_t *l2, unsigned int nr)
{
	while (l2->va != nr) {
		(l2->va)++;
		if(test_bit(FLG_MOD128, &l2->flag))
			l2->va %= 128;
		else
			l2->va %= 8;
		l2up(l2, DL_DATA | CONFIRM, (int)l2->windowar[l2->sow], NULL);
		free_msg(l2->windowar[l2->sow]);
		l2->windowar[l2->sow] = NULL;
		l2->sow = (l2->sow + 1) % l2->window;
	}
}

static void
send_uframe(layer2_t *l2, msg_t *msg, u_char cmd, u_char cr)
{
	u_char tmp[MAX_HEADER_LEN];
	int i;

	i = sethdraddr(l2, tmp, cr);
	tmp[i++] = cmd;
	if (msg)
		msg_trim(msg, 0);
	else if ((msg = alloc_msg(i + mISDNUSER_HEAD_SIZE)))
		msg_reserve(msg, mISDNUSER_HEAD_SIZE);
	else {
		dprint(DBGM_L2, l2->nst->cardnr,"%s: can't alloc msguff\n", __FUNCTION__);
		return;
	}
	memcpy(msg_put(msg, i), tmp, i);
	msg_push(msg, mISDNUSER_HEAD_SIZE);
	enqueue_super(l2, msg);
}


inline u_char
get_PollFlag(layer2_t *l2, msg_t * msg)
{
	return (msg->data[l2addrsize(l2)] & 0x10);
}

inline u_char
get_PollFlagFree(layer2_t *l2, msg_t *msg)
{
	u_char PF;

	PF = get_PollFlag(l2, msg);
	free_msg(msg);
	return (PF);
}

inline void
start_t200(layer2_t *l2, int i)
{
	FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, i);
	test_and_set_bit(FLG_T200_RUN, &l2->flag);
}

inline void
restart_t200(layer2_t *l2, int i)
{
	FsmRestartTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, i);
	test_and_set_bit(FLG_T200_RUN, &l2->flag);
}

inline void
stop_t200(layer2_t *l2, int i)
{
	if(test_and_clear_bit(FLG_T200_RUN, &l2->flag))
		FsmDelTimer(&l2->t200, i);
}

inline void
st5_dl_release_l2l3(layer2_t *l2)
{
	int pr;

	if (test_and_clear_bit(FLG_PEND_REL, &l2->flag)) {
		pr = DL_RELEASE | CONFIRM;
	} else {
		pr = DL_RELEASE | INDICATION;
	}
	l2up_create(l2, pr, CES(l2), 0, NULL);
}

inline void
lapb_dl_release_l2l3(layer2_t *l2, int f)
{
	if (test_bit(FLG_LAPB, &l2->flag))
		l2down_create(l2, PH_DEACTIVATE | REQUEST, 0, 0, NULL);
	l2up_create(l2, DL_RELEASE | f, CES(l2), 0, NULL);
}

static void
establishlink(struct FsmInst *fi)
{
	layer2_t *l2 = fi->userdata;
	u_char cmd;

	clear_exception(l2);
	l2->rc = 0;
	cmd = (test_bit(FLG_MOD128, &l2->flag) ? SABME : SABM) | 0x10;
	send_uframe(l2, NULL, cmd, CMD);
	FsmDelTimer(&l2->t203, 1);
	restart_t200(l2, 1);
	test_and_clear_bit(FLG_PEND_REL, &l2->flag);
	freewin(l2);
	FsmChangeState(fi, ST_L2_5);
}

static void
l2_mdl_error_ua(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	if (get_PollFlagFree(l2, msg))
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'C');
	else
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'D');
	
}

static void
l2_mdl_error_dm(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	if (get_PollFlagFree(l2, msg))
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'B');
	else {
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'E');
		establishlink(fi);
		test_and_clear_bit(FLG_L3_INIT, &l2->flag);
	}
}

static void
l2_st8_mdl_error_dm(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	if (get_PollFlagFree(l2, msg))
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'B');
	else {
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'E');
	}
	establishlink(fi);
	test_and_clear_bit(FLG_L3_INIT, &l2->flag);
}

static void
l2_go_st3(struct FsmInst *fi, int event, void *arg)
{
	free_msg((msg_t *)arg);
	FsmChangeState(fi, ST_L2_3); 
}

static void
l2_mdl_assign(struct FsmInst *fi, int event, void *arg)
{
	layer2_t	*l2 = fi->userdata;
	msg_t	*msg = arg;
	mISDNuser_head_t	*hh;

	FsmChangeState(fi, ST_L2_3);
	msg_trim(msg, 0);
	hh = (mISDNuser_head_t *)msg_put(msg, mISDNUSER_HEAD_SIZE);
	hh->prim = MDL_ASSIGN | INDICATION;
	hh->dinfo = 0;
	if (l2_tei(l2->tm, msg))
		free_msg(msg);
}

static void
l2_queue_ui_assign(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_tail(&l2->ui_queue, msg);
	FsmChangeState(fi, ST_L2_2);
	if ((msg = create_link_msg(MDL_ASSIGN | INDICATION, 0, 0, NULL, 0))) {
		if (l2_tei(l2->tm, msg))
			free_msg(msg);
	}
}

static void
l2_queue_ui(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_tail(&l2->ui_queue, msg);
}

static void
tx_ui(layer2_t *l2)
{
	msg_t *msg;
	u_char header[MAX_HEADER_LEN];
	int i;

	i = sethdraddr(l2, header, CMD);
	if (test_bit(FLG_LAPD_NET, &l2->flag))
		header[1] = 0xff; /* tei 127 */
	header[i++] = UI;
	while ((msg = msg_dequeue(&l2->ui_queue))) {
		msg_pull(msg, mISDNUSER_HEAD_SIZE);
		memcpy(msg_push(msg, i), header, i);
		msg_push(msg, mISDNUSER_HEAD_SIZE);
		enqueue_ui(l2, msg);
	}
}

static void
l2_send_ui(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_tail(&l2->ui_queue, msg);
	tx_ui(l2);
}

static void
l2_got_ui(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_pull(msg, l2headersize(l2, 1));
/*
 *		in states 1-3 for broadcast
 */
	msg_push(msg, mISDNUSER_HEAD_SIZE);
	if (l2up(l2, DL_UNITDATA | INDICATION, CES(l2), msg))
		free_msg(msg);
}

static void
l2_establish(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	establishlink(fi);
	test_and_set_bit(FLG_L3_INIT, &l2->flag);
	free_msg(msg);
}

static void
l2_discard_i_setl3(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	msg_queue_purge(&l2->i_queue);
	test_and_set_bit(FLG_L3_INIT, &l2->flag);
	test_and_clear_bit(FLG_PEND_REL, &l2->flag);
	free_msg(msg);
} 

static void
l2_l3_reestablish(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	msg_queue_purge(&l2->i_queue);
	establishlink(fi);
	test_and_set_bit(FLG_L3_INIT, &l2->flag);
	free_msg(msg);
}

static void
l2_release(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_trim(msg, mISDNUSER_HEAD_SIZE);
	if (l2up(l2, DL_RELEASE | CONFIRM, CES(l2), msg))
		free_msg(msg);
}

static void
l2_pend_rel(struct FsmInst *fi, int event, void *arg)
{
	msg_t *msg = arg;
	layer2_t *l2 = fi->userdata;

	test_and_set_bit(FLG_PEND_REL, &l2->flag);
	free_msg(msg);
}

static void
l2_disconnect(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->i_queue);
	freewin(l2);
	FsmChangeState(fi, ST_L2_6);
	l2->rc = 0;
	send_uframe(l2, NULL, DISC | 0x10, CMD);
	FsmDelTimer(&l2->t203, 1);
	restart_t200(l2, 2);
	if (msg)
		free_msg(msg);
}

static void
l2_start_multi(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	send_uframe(l2, NULL, UA | get_PollFlag(l2, msg), RSP);

	clear_exception(l2);
	l2->vs = 0;
	l2->va = 0;
	l2->vr = 0;
	l2->sow = 0;
	FsmChangeState(fi, ST_L2_7);
	FsmAddTimer(&l2->t203, l2->T203, EV_L2_T203, NULL, 3);
	msg_trim(msg, 0);
	msg_push(msg, mISDNUSER_HEAD_SIZE);
	if (l2up(l2, DL_ESTABLISH | INDICATION, CES(l2), msg))
		free_msg(msg);
}

static void
l2_send_UA(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	send_uframe(l2, msg, UA | get_PollFlag(l2, msg), RSP);
}

static void
l2_send_DM(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	send_uframe(l2, msg, DM | get_PollFlag(l2, msg), RSP);
}

static void
l2_restart_multi(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;
	int est = 0;

	send_uframe(l2, msg, UA | get_PollFlag(l2, msg), RSP);

	l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'F');

	if (l2->vs != l2->va) {
		msg_queue_purge(&l2->i_queue);
		est = 1;
	}

	clear_exception(l2);
	l2->vs = 0;
	l2->va = 0;
	l2->vr = 0;
	l2->sow = 0;
	FsmChangeState(fi, ST_L2_7);
	stop_t200(l2, 3);
	FsmRestartTimer(&l2->t203, l2->T203, EV_L2_T203, NULL, 3);

	if (est)
		l2up_create(l2, DL_ESTABLISH | INDICATION, CES(l2), 0, NULL);

	if (msg_queue_len(&l2->i_queue) && cansend(l2))
		FsmEvent(fi, EV_L2_ACK_PULL, NULL);
}

static void
l2_stop_multi(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	FsmChangeState(fi, ST_L2_4);
	FsmDelTimer(&l2->t203, 3);
	stop_t200(l2, 4);

	send_uframe(l2, msg, UA | get_PollFlag(l2, msg), RSP);
	msg_queue_purge(&l2->i_queue);
	freewin(l2);
	lapb_dl_release_l2l3(l2, INDICATION);
}

static void
l2_connected(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;
	int pr=-1;

	if (!get_PollFlag(l2, msg)) {
		l2_mdl_error_ua(fi, event, arg);
		return;
	}
	free_msg(msg);
	if (test_and_clear_bit(FLG_PEND_REL, &l2->flag))
		l2_disconnect(fi, event, NULL);
	if (test_and_clear_bit(FLG_L3_INIT, &l2->flag)) {
		pr = DL_ESTABLISH | CONFIRM;
	} else if (l2->vs != l2->va) {
		msg_queue_purge(&l2->i_queue);
		pr = DL_ESTABLISH | INDICATION;
	}
	stop_t200(l2, 5);

	l2->vr = 0;
	l2->vs = 0;
	l2->va = 0;
	l2->sow = 0;
	FsmChangeState(fi, ST_L2_7);
	FsmAddTimer(&l2->t203, l2->T203, EV_L2_T203, NULL, 4);
	if (pr != -1)
		l2up_create(l2, pr, CES(l2), 0, NULL);

	if (msg_queue_len(&l2->i_queue) && cansend(l2))
		FsmEvent(fi, EV_L2_ACK_PULL, NULL);
}

static void
l2_released(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	if (!get_PollFlag(l2, msg)) {
		l2_mdl_error_ua(fi, event, arg);
		return;
	}
	free_msg(msg);
	stop_t200(l2, 6);
	lapb_dl_release_l2l3(l2, CONFIRM);
	FsmChangeState(fi, ST_L2_4);
}

static void
l2_reestablish(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	if (!get_PollFlagFree(l2, msg)) {
		establishlink(fi);
		test_and_set_bit(FLG_L3_INIT, &l2->flag);
	}
}

static void
l2_st5_dm_release(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	if (get_PollFlagFree(l2, msg)) {
		stop_t200(l2, 7);
	 	if (!test_bit(FLG_L3_INIT, &l2->flag))
			msg_queue_purge(&l2->i_queue);
		if (test_bit(FLG_LAPB, &l2->flag))
			l2down_create(l2, PH_DEACTIVATE | REQUEST, 0, 0, NULL);
		st5_dl_release_l2l3(l2);
		FsmChangeState(fi, ST_L2_4);
	}
}

static void
l2_st6_dm_release(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	if (get_PollFlagFree(l2, msg)) {
		stop_t200(l2, 8);
		lapb_dl_release_l2l3(l2, CONFIRM);
		FsmChangeState(fi, ST_L2_4);
	}
}

void
enquiry_cr(layer2_t *l2, u_char typ, u_char cr, u_char pf)
{
	msg_t *msg;
	u_char tmp[MAX_HEADER_LEN];
	int i;

	i = sethdraddr(l2, tmp, cr);
	if (test_bit(FLG_MOD128, &l2->flag)) {
		tmp[i++] = typ;
		tmp[i++] = (l2->vr << 1) | (pf ? 1 : 0);
	} else
		tmp[i++] = (l2->vr << 5) | typ | (pf ? 0x10 : 0);
	if (!(msg = alloc_msg(i + mISDNUSER_HEAD_SIZE))) {
		dprint(DBGM_L2, l2->nst->cardnr, "isdnl2 can't alloc sbbuff for enquiry_cr\n");
		return;
	} else
		msg_reserve(msg, mISDNUSER_HEAD_SIZE);
	memcpy(msg_put(msg, i), tmp, i);
	msg_push(msg, mISDNUSER_HEAD_SIZE);
	enqueue_super(l2, msg);
}

inline void
enquiry_response(layer2_t *l2)
{
	if (test_bit(FLG_OWN_BUSY, &l2->flag))
		enquiry_cr(l2, RNR, RSP, 1);
	else
		enquiry_cr(l2, RR, RSP, 1);
	test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
}

inline void
transmit_enquiry(layer2_t *l2)
{
	if (test_bit(FLG_OWN_BUSY, &l2->flag))
		enquiry_cr(l2, RNR, CMD, 1);
	else
		enquiry_cr(l2, RR, CMD, 1);
	test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
	start_t200(l2, 9);
}


static void
nrerrorrecovery(struct FsmInst *fi)
{
	layer2_t *l2 = fi->userdata;

	l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'J');
	establishlink(fi);
	test_and_clear_bit(FLG_L3_INIT, &l2->flag);
}

static void
invoke_retransmission(layer2_t *l2, unsigned int nr)
{
	unsigned int p1;

	if (l2->vs != nr) {
		while (l2->vs != nr) {
			(l2->vs)--;
			if(test_bit(FLG_MOD128, &l2->flag)) {
				l2->vs %= 128;
				p1 = (l2->vs - l2->va) % 128;
			} else {
				l2->vs %= 8;
				p1 = (l2->vs - l2->va) % 8;
			}
			p1 = (p1 + l2->sow) % l2->window;
//			if (test_bit(FLG_LAPB, &l2->flag))
//				st->l1.bcs->tx_cnt += l2->windowar[p1]->len + l2headersize(l2, 0);
			msg_queue_head(&l2->i_queue, l2->windowar[p1]);
			l2->windowar[p1] = NULL;
		}
		FsmEvent(&l2->l2m, EV_L2_ACK_PULL, NULL);
	}
}

static void
l2_st7_got_super(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;
	int PollFlag, rsp, typ = RR;
	unsigned int nr;

	rsp = *msg->data & 0x2;
	if (test_bit(FLG_ORIG, &l2->flag))
		rsp = !rsp;

	msg_pull(msg, l2addrsize(l2));
	if (IsRNR(msg->data, l2)) {
		set_peer_busy(l2);
		typ = RNR;
	} else
		clear_peer_busy(l2);
	if (IsREJ(msg->data, l2))
		typ = REJ;

	if (test_bit(FLG_MOD128, &l2->flag)) {
		PollFlag = (msg->data[1] & 0x1) == 0x1;
		nr = msg->data[1] >> 1;
	} else {
		PollFlag = (msg->data[0] & 0x10);
		nr = (msg->data[0] >> 5) & 0x7;
	}
	free_msg(msg);

	if (PollFlag) {
		if (rsp)
			l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'A');
		else
			enquiry_response(l2);
	}
	if (legalnr(l2, nr)) {
		if (typ == REJ) {
			setva(l2, nr);
			invoke_retransmission(l2, nr);
			stop_t200(l2, 10);
			if (FsmAddTimer(&l2->t203, l2->T203,
					EV_L2_T203, NULL, 6))
				l2m_debug(&l2->l2m, "Restart T203 ST7 REJ");
		} else if ((nr == l2->vs) && (typ == RR)) {
			setva(l2, nr);
			stop_t200(l2, 11);
			FsmRestartTimer(&l2->t203, l2->T203,
					EV_L2_T203, NULL, 7);
		} else if ((l2->va != nr) || (typ == RNR)) {
			setva(l2, nr);
			if (typ != RR)
				FsmDelTimer(&l2->t203, 9);
			restart_t200(l2, 12);
		}
		if (msg_queue_len(&l2->i_queue) && (typ == RR))
			FsmEvent(fi, EV_L2_ACK_PULL, NULL);
	} else
		nrerrorrecovery(fi);
}

static void
l2_feed_i_if_reest(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

//	if (test_bit(FLG_LAPB, &l2->flag))
//		st->l1.bcs->tx_cnt += msg->len + l2headersize(l2, 0);
	if (!test_bit(FLG_L3_INIT, &l2->flag))
		msg_queue_tail(&l2->i_queue, msg);
	else
		free_msg(msg);
}

static void
l2_feed_i_pull(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

//	if (test_bit(FLG_LAPB, &l2->flag))
//		st->l1.bcs->tx_cnt += msg->len + l2headersize(l2, 0);
	msg_queue_tail(&l2->i_queue, msg);
	FsmEvent(fi, EV_L2_ACK_PULL, NULL);
}

static void
l2_feed_iqueue(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

//	if (test_bit(FLG_LAPB, &l2->flag))
//		st->l1.bcs->tx_cnt += msg->len + l2headersize(l2, 0);
	msg_queue_tail(&l2->i_queue, msg);
}

static void
l2_got_iframe(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;
	int PollFlag, ns, i;
	unsigned int nr;

	i = l2addrsize(l2);
	if (test_bit(FLG_MOD128, &l2->flag)) {
		PollFlag = ((msg->data[i + 1] & 0x1) == 0x1);
		ns = msg->data[i] >> 1;
		nr = (msg->data[i + 1] >> 1) & 0x7f;
	} else {
		PollFlag = (msg->data[i] & 0x10);
		ns = (msg->data[i] >> 1) & 0x7;
		nr = (msg->data[i] >> 5) & 0x7;
	}
	if (test_bit(FLG_OWN_BUSY, &l2->flag)) {
		free_msg(msg);
		if (PollFlag)
			enquiry_response(l2);
	} else if (l2->vr == ns) {
		(l2->vr)++;
		if(test_bit(FLG_MOD128, &l2->flag))
			l2->vr %= 128;
		else
			l2->vr %= 8;
		test_and_clear_bit(FLG_REJEXC, &l2->flag);
		if (PollFlag)
			enquiry_response(l2);
		else
			test_and_set_bit(FLG_ACK_PEND, &l2->flag);
		msg_pull(msg, l2headersize(l2, 0));
		msg_push(msg, mISDNUSER_HEAD_SIZE);
		if (l2up(l2, DL_DATA | INDICATION, CES(l2), msg))
			free_msg(msg);
	} else {
		/* n(s)!=v(r) */
		free_msg(msg);
		if (test_and_set_bit(FLG_REJEXC, &l2->flag)) {
			if (PollFlag)
				enquiry_response(l2);
		} else {
			enquiry_cr(l2, REJ, RSP, PollFlag);
			test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
		}
	}
	if (legalnr(l2, nr)) {
		if (!test_bit(FLG_PEER_BUSY, &l2->flag) && (fi->state == ST_L2_7)) {
			if (nr == l2->vs) {
				stop_t200(l2, 13);
				FsmRestartTimer(&l2->t203, l2->T203,
						EV_L2_T203, NULL, 7);
			} else if (nr != l2->va)
				restart_t200(l2, 14);
		}
		setva(l2, nr);
	} else {
		nrerrorrecovery(fi);
		return;
	}
	if (msg_queue_len(&l2->i_queue) && (fi->state == ST_L2_7))
		FsmEvent(fi, EV_L2_ACK_PULL, NULL);
	if (test_and_clear_bit(FLG_ACK_PEND, &l2->flag))
		enquiry_cr(l2, RR, RSP, 0);
}

static void
l2_got_tei(struct FsmInst *fi, int event, void *arg)
{
	layer2_t	*l2 = fi->userdata;
	msg_t	*msg = arg;
	mISDNuser_head_t	*hh = (mISDNuser_head_t *)msg->data;

	l2->tei = hh->dinfo;
	free_msg(msg);
	if (fi->state == ST_L2_3) {
		establishlink(fi);
		test_and_set_bit(FLG_L3_INIT, &l2->flag);
	} else
		FsmChangeState(fi, ST_L2_4);
	if (msg_queue_len(&l2->ui_queue))
		tx_ui(l2);
}

static void
l2_st5_tout_200(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	if (test_bit(FLG_LAPD, &l2->flag) &&
		test_bit(FLG_DCHAN_BUSY, &l2->flag)) {
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, 9);
	} else if (l2->rc == l2->N200) {
		FsmChangeState(fi, ST_L2_4);
		test_and_clear_bit(FLG_T200_RUN, &l2->flag);
		msg_queue_purge(&l2->i_queue);
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'G');
		if (test_bit(FLG_LAPB, &l2->flag))
			l2down_create(l2, PH_DEACTIVATE | REQUEST, 0, 0, NULL);
		st5_dl_release_l2l3(l2);
	} else {
		l2->rc++;
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, 9);
		send_uframe(l2, NULL, (test_bit(FLG_MOD128, &l2->flag) ?
			SABME : SABM) | 0x10, CMD);
	}
}

static void
l2_st6_tout_200(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	if (test_bit(FLG_LAPD, &l2->flag) &&
		test_bit(FLG_DCHAN_BUSY, &l2->flag)) {
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, 9);
	} else if (l2->rc == l2->N200) {
		FsmChangeState(fi, ST_L2_4);
		test_and_clear_bit(FLG_T200_RUN, &l2->flag);
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'H');
		lapb_dl_release_l2l3(l2, CONFIRM);
	} else {
		l2->rc++;
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200,
			    NULL, 9);
		send_uframe(l2, NULL, DISC | 0x10, CMD);
	}
}

static void
l2_st7_tout_200(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	if (test_bit(FLG_LAPD, &l2->flag) &&
		test_bit(FLG_DCHAN_BUSY, &l2->flag)) {
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, 9);
		return;
	}
	test_and_clear_bit(FLG_T200_RUN, &l2->flag);
	l2->rc = 0;
	FsmChangeState(fi, ST_L2_8);
	transmit_enquiry(l2);
	l2->rc++;
}

static void
l2_st8_tout_200(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	if (test_bit(FLG_LAPD, &l2->flag) &&
		test_bit(FLG_DCHAN_BUSY, &l2->flag)) {
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, 9);
		return;
	}
	test_and_clear_bit(FLG_T200_RUN, &l2->flag);
	if (l2->rc == l2->N200) {
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'I');
		establishlink(fi);
		test_and_clear_bit(FLG_L3_INIT, &l2->flag);
	} else {
		transmit_enquiry(l2);
		l2->rc++;
	}
}

static void
l2_st7_tout_203(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	if (test_bit(FLG_LAPD, &l2->flag) &&
		test_bit(FLG_DCHAN_BUSY, &l2->flag)) {
		FsmAddTimer(&l2->t203, l2->T203, EV_L2_T203, NULL, 9);
		return;
	}
	FsmChangeState(fi, ST_L2_8);
	transmit_enquiry(l2);
	l2->rc = 0;
}

static void
l2_pull_iqueue(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg, *omsg;
	u_char header[MAX_HEADER_LEN];
	int i;
	int unsigned p1;

	if (!cansend(l2))
		return;

	msg = msg_dequeue(&l2->i_queue);
	if (!msg)
		return;

	if(test_bit(FLG_MOD128, &l2->flag))
		p1 = (l2->vs - l2->va) % 128;
	else
		p1 = (l2->vs - l2->va) % 8;
	p1 = (p1 + l2->sow) % l2->window;
	if (l2->windowar[p1]) {
		dprint(DBGM_L2, l2->nst->cardnr, "isdnl2 try overwrite ack queue entry %d\n",
		       p1);
		free_msg(l2->windowar[p1]);
	}
	l2->windowar[p1] = msg;
	msg = msg_clone(msg);

	if (!msg) {
		free_msg(l2->windowar[p1]);
		dprint(DBGM_L2, l2->nst->cardnr,"%s: no msg mem\n", __FUNCTION__);
		return;
	}

	i = sethdraddr(l2, header, CMD);

	if (test_bit(FLG_MOD128, &l2->flag)) {
		header[i++] = l2->vs << 1;
		header[i++] = l2->vr << 1;
		l2->vs = (l2->vs + 1) % 128;
	} else {
		header[i++] = (l2->vr << 5) | (l2->vs << 1);
		l2->vs = (l2->vs + 1) % 8;
	}

	p1 = msg_headroom(msg);
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	if (p1 >= i)
		memcpy(msg_push(msg, i), header, i);
	else {
		dprint(DBGM_L2, l2->nst->cardnr,
		"isdnl2 pull_iqueue msg header(%d/%d) too short\n", i, p1);
		omsg = msg;
		msg = alloc_msg(omsg->len + i + mISDNUSER_HEAD_SIZE);
		if (!msg) {
			free_msg(omsg);
			dprint(DBGM_L2, l2->nst->cardnr,"%s: no msg mem\n", __FUNCTION__);
			return;
		}
		msg_reserve(msg, mISDNUSER_HEAD_SIZE);
		memcpy(msg_put(msg, i), header, i);
		memcpy(msg_put(msg, omsg->len), omsg->data, omsg->len);
		free_msg(omsg);
	}
	msg_push(msg, mISDNUSER_HEAD_SIZE);
	l2down(l2, PH_DATA_REQ, DINFO_SKB, msg);
	test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
	if (!test_and_set_bit(FLG_T200_RUN, &l2->flag)) {
		FsmDelTimer(&l2->t203, 13);
		FsmAddTimer(&l2->t200, l2->T200, EV_L2_T200, NULL, 11);
	}
}

static void
l2_st8_got_super(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;
	int PollFlag, rsp, rnr = 0;
	unsigned int nr;

	rsp = *msg->data & 0x2;
	if (test_bit(FLG_ORIG, &l2->flag))
		rsp = !rsp;

	msg_pull(msg, l2addrsize(l2));

	if (IsRNR(msg->data, l2)) {
		set_peer_busy(l2);
		rnr = 1;
	} else
		clear_peer_busy(l2);

	if (test_bit(FLG_MOD128, &l2->flag)) {
		PollFlag = (msg->data[1] & 0x1) == 0x1;
		nr = msg->data[1] >> 1;
	} else {
		PollFlag = (msg->data[0] & 0x10);
		nr = (msg->data[0] >> 5) & 0x7;
	}
	free_msg(msg);
	if (rsp && PollFlag) {
		if (legalnr(l2, nr)) {
			if (rnr) {
				restart_t200(l2, 15);
			} else {
				stop_t200(l2, 16);
				FsmAddTimer(&l2->t203, l2->T203,
					    EV_L2_T203, NULL, 5);
				setva(l2, nr);
			}
			invoke_retransmission(l2, nr);
			FsmChangeState(fi, ST_L2_7);
			if (msg_queue_len(&l2->i_queue) && cansend(l2))
				FsmEvent(fi, EV_L2_ACK_PULL, NULL);
		} else
			nrerrorrecovery(fi);
	} else {
		if (!rsp && PollFlag)
			enquiry_response(l2);
		if (legalnr(l2, nr)) {
			setva(l2, nr);
		} else
			nrerrorrecovery(fi);
	}
}

static void
l2_got_FRMR(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_pull(msg, l2addrsize(l2) + 1);

	if (!(msg->data[0] & 1) || ((msg->data[0] & 3) == 1) ||		/* I or S */
	    (IsUA(msg->data, l2) && (fi->state == ST_L2_7))) {
		l2mgr(l2, MDL_ERROR | INDICATION, (void *) 'K');
		establishlink(fi);
		test_and_clear_bit(FLG_L3_INIT, &l2->flag);
	}
	free_msg(msg);
}

static void
l2_st24_tei_remove(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->ui_queue);
	l2->tei = -1;
	FsmChangeState(fi, ST_L2_1);
	free_msg(msg);
}

static void
l2_st3_tei_remove(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->ui_queue);
	l2->tei = -1;
	msg_trim(msg, mISDNUSER_HEAD_SIZE);
	if (l2up(l2, DL_RELEASE | INDICATION, CES(l2), msg))
		free_msg(msg);
	FsmChangeState(fi, ST_L2_1);
}

static void
l2_st5_tei_remove(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->i_queue);
	msg_queue_purge(&l2->ui_queue);
	freewin(l2);
	l2->tei = -1;
	stop_t200(l2, 17);
	st5_dl_release_l2l3(l2);
	FsmChangeState(fi, ST_L2_1);
	free_msg(msg);
}

static void
l2_st6_tei_remove(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->ui_queue);
	l2->tei = -1;
	stop_t200(l2, 18);
	if (l2up(l2, DL_RELEASE | CONFIRM, CES(l2), msg))
		free_msg(msg);
	FsmChangeState(fi, ST_L2_1);
}

static void
l2_tei_remove(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->i_queue);
	msg_queue_purge(&l2->ui_queue);
	freewin(l2);
	l2->tei = -1;
	stop_t200(l2, 17);
	FsmDelTimer(&l2->t203, 19);
	if (l2up(l2, DL_RELEASE | INDICATION, CES(l2), msg))
		free_msg(msg);
	FsmChangeState(fi, ST_L2_1);
}

static void
l2_st14_persistant_da(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;
	
	msg_queue_purge(&l2->i_queue);
	msg_queue_purge(&l2->ui_queue);
	if (test_and_clear_bit(FLG_ESTAB_PEND, &l2->flag))
		if (!l2up(l2, DL_RELEASE | INDICATION, CES(l2), msg))
			return;
	free_msg(msg);
}

static void
l2_st5_persistant_da(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->i_queue);
	msg_queue_purge(&l2->ui_queue);
	freewin(l2);
	stop_t200(l2, 19);
	st5_dl_release_l2l3(l2);
	FsmChangeState(fi, ST_L2_4);
	free_msg(msg);
}

static void
l2_st6_persistant_da(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->ui_queue);
	stop_t200(l2, 20);
	if (l2up(l2, DL_RELEASE | CONFIRM, CES(l2), msg))
		free_msg(msg);
	FsmChangeState(fi, ST_L2_4);
}

static void
l2_persistant_da(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	msg_queue_purge(&l2->i_queue);
	msg_queue_purge(&l2->ui_queue);
	freewin(l2);
	stop_t200(l2, 19);
	FsmDelTimer(&l2->t203, 19);
	if (l2up(l2, DL_RELEASE | INDICATION, CES(l2), msg))
		free_msg(msg);
	FsmChangeState(fi, ST_L2_4);
}

static void
l2_set_own_busy(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	if(!test_and_set_bit(FLG_OWN_BUSY, &l2->flag)) {
		enquiry_cr(l2, RNR, RSP, 0);
		test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
	}
	if (msg)
		free_msg(msg);
}

static void
l2_clear_own_busy(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;
	msg_t *msg = arg;

	if(!test_and_clear_bit(FLG_OWN_BUSY, &l2->flag)) {
		enquiry_cr(l2, RR, RSP, 0);
		test_and_clear_bit(FLG_ACK_PEND, &l2->flag);
	}
	if (msg)
		free_msg(msg);
}

static void
l2_frame_error(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	l2mgr(l2, MDL_ERROR | INDICATION, arg);
}

static void
l2_frame_error_reest(struct FsmInst *fi, int event, void *arg)
{
	layer2_t *l2 = fi->userdata;

	l2mgr(l2, MDL_ERROR | INDICATION, arg);
	establishlink(fi);
	test_and_clear_bit(FLG_L3_INIT, &l2->flag);
}

static struct FsmNode L2FnList[] =
{
	{ST_L2_1, EV_L2_DL_ESTABLISH_REQ, l2_mdl_assign},
	{ST_L2_2, EV_L2_DL_ESTABLISH_REQ, l2_go_st3},
	{ST_L2_4, EV_L2_DL_ESTABLISH_REQ, l2_establish},
	{ST_L2_5, EV_L2_DL_ESTABLISH_REQ, l2_discard_i_setl3},
	{ST_L2_7, EV_L2_DL_ESTABLISH_REQ, l2_l3_reestablish},
	{ST_L2_8, EV_L2_DL_ESTABLISH_REQ, l2_l3_reestablish},
	{ST_L2_4, EV_L2_DL_RELEASE_REQ, l2_release},
	{ST_L2_5, EV_L2_DL_RELEASE_REQ, l2_pend_rel},
	{ST_L2_7, EV_L2_DL_RELEASE_REQ, l2_disconnect},
	{ST_L2_8, EV_L2_DL_RELEASE_REQ, l2_disconnect},
	{ST_L2_5, EV_L2_DL_DATA, l2_feed_i_if_reest},
	{ST_L2_7, EV_L2_DL_DATA, l2_feed_i_pull},
	{ST_L2_8, EV_L2_DL_DATA, l2_feed_iqueue},
	{ST_L2_1, EV_L2_DL_UNITDATA, l2_queue_ui_assign},
	{ST_L2_2, EV_L2_DL_UNITDATA, l2_queue_ui},
	{ST_L2_3, EV_L2_DL_UNITDATA, l2_queue_ui},
	{ST_L2_4, EV_L2_DL_UNITDATA, l2_send_ui},
	{ST_L2_5, EV_L2_DL_UNITDATA, l2_send_ui},
	{ST_L2_6, EV_L2_DL_UNITDATA, l2_send_ui},
	{ST_L2_7, EV_L2_DL_UNITDATA, l2_send_ui},
	{ST_L2_8, EV_L2_DL_UNITDATA, l2_send_ui},
	{ST_L2_1, EV_L2_MDL_ASSIGN, l2_got_tei},
	{ST_L2_2, EV_L2_MDL_ASSIGN, l2_got_tei},
	{ST_L2_3, EV_L2_MDL_ASSIGN, l2_got_tei},
	{ST_L2_2, EV_L2_MDL_ERROR, l2_st24_tei_remove},
	{ST_L2_3, EV_L2_MDL_ERROR, l2_st3_tei_remove},
	{ST_L2_4, EV_L2_MDL_REMOVE, l2_st24_tei_remove},
	{ST_L2_5, EV_L2_MDL_REMOVE, l2_st5_tei_remove},
	{ST_L2_6, EV_L2_MDL_REMOVE, l2_st6_tei_remove},
	{ST_L2_7, EV_L2_MDL_REMOVE, l2_tei_remove},
	{ST_L2_8, EV_L2_MDL_REMOVE, l2_tei_remove},
	{ST_L2_4, EV_L2_SABME, l2_start_multi},
	{ST_L2_5, EV_L2_SABME, l2_send_UA},
	{ST_L2_6, EV_L2_SABME, l2_send_DM},
	{ST_L2_7, EV_L2_SABME, l2_restart_multi},
	{ST_L2_8, EV_L2_SABME, l2_restart_multi},
	{ST_L2_4, EV_L2_DISC, l2_send_DM},
	{ST_L2_5, EV_L2_DISC, l2_send_DM},
	{ST_L2_6, EV_L2_DISC, l2_send_UA},
	{ST_L2_7, EV_L2_DISC, l2_stop_multi},
	{ST_L2_8, EV_L2_DISC, l2_stop_multi},
	{ST_L2_4, EV_L2_UA, l2_mdl_error_ua},
	{ST_L2_5, EV_L2_UA, l2_connected},
	{ST_L2_6, EV_L2_UA, l2_released},
	{ST_L2_7, EV_L2_UA, l2_mdl_error_ua},
	{ST_L2_8, EV_L2_UA, l2_mdl_error_ua},
	{ST_L2_4, EV_L2_DM, l2_reestablish},
	{ST_L2_5, EV_L2_DM, l2_st5_dm_release},
	{ST_L2_6, EV_L2_DM, l2_st6_dm_release},
	{ST_L2_7, EV_L2_DM, l2_mdl_error_dm},
	{ST_L2_8, EV_L2_DM, l2_st8_mdl_error_dm},
	{ST_L2_1, EV_L2_UI, l2_got_ui},
	{ST_L2_2, EV_L2_UI, l2_got_ui},
	{ST_L2_3, EV_L2_UI, l2_got_ui},
	{ST_L2_4, EV_L2_UI, l2_got_ui},
	{ST_L2_5, EV_L2_UI, l2_got_ui},
	{ST_L2_6, EV_L2_UI, l2_got_ui},
	{ST_L2_7, EV_L2_UI, l2_got_ui},
	{ST_L2_8, EV_L2_UI, l2_got_ui},
	{ST_L2_7, EV_L2_FRMR, l2_got_FRMR},
	{ST_L2_8, EV_L2_FRMR, l2_got_FRMR},
	{ST_L2_7, EV_L2_SUPER, l2_st7_got_super},
	{ST_L2_8, EV_L2_SUPER, l2_st8_got_super},
	{ST_L2_7, EV_L2_I, l2_got_iframe},
	{ST_L2_8, EV_L2_I, l2_got_iframe},
	{ST_L2_5, EV_L2_T200, l2_st5_tout_200},
	{ST_L2_6, EV_L2_T200, l2_st6_tout_200},
	{ST_L2_7, EV_L2_T200, l2_st7_tout_200},
	{ST_L2_8, EV_L2_T200, l2_st8_tout_200},
	{ST_L2_7, EV_L2_T203, l2_st7_tout_203},
	{ST_L2_7, EV_L2_ACK_PULL, l2_pull_iqueue},
	{ST_L2_7, EV_L2_SET_OWN_BUSY, l2_set_own_busy},
	{ST_L2_8, EV_L2_SET_OWN_BUSY, l2_set_own_busy},
	{ST_L2_7, EV_L2_CLEAR_OWN_BUSY, l2_clear_own_busy},
	{ST_L2_8, EV_L2_CLEAR_OWN_BUSY, l2_clear_own_busy},
	{ST_L2_4, EV_L2_FRAME_ERROR, l2_frame_error},
	{ST_L2_5, EV_L2_FRAME_ERROR, l2_frame_error},
	{ST_L2_6, EV_L2_FRAME_ERROR, l2_frame_error},
	{ST_L2_7, EV_L2_FRAME_ERROR, l2_frame_error_reest},
	{ST_L2_8, EV_L2_FRAME_ERROR, l2_frame_error_reest},
	{ST_L2_1, EV_L1_DEACTIVATE, l2_st14_persistant_da},
	{ST_L2_2, EV_L1_DEACTIVATE, l2_st24_tei_remove},
	{ST_L2_3, EV_L1_DEACTIVATE, l2_st3_tei_remove},
	{ST_L2_4, EV_L1_DEACTIVATE, l2_st14_persistant_da},
	{ST_L2_5, EV_L1_DEACTIVATE, l2_st5_persistant_da},
	{ST_L2_6, EV_L1_DEACTIVATE, l2_st6_persistant_da},
	{ST_L2_7, EV_L1_DEACTIVATE, l2_persistant_da},
	{ST_L2_8, EV_L1_DEACTIVATE, l2_persistant_da},
};

#define L2_FN_COUNT (sizeof(L2FnList)/sizeof(struct FsmNode))

static layer2_t *
select_l2(net_stack_t *nst, int sapi, int tei) {
	layer2_t	*l2;

	l2 = nst->layer2;
	while (l2) {
		if ((l2->sapi == sapi) && (l2->tei == tei))
			break;
		l2 = l2->next;
	}
	return(l2);
}

static int
ph_data_mux(net_stack_t *nst, iframe_t *frm, msg_t *msg)
{
	u_char		*datap;
	layer2_t	*l2;
	int		ret = -EINVAL;
	int		psapi, ptei;
	mISDNuser_head_t	*hh;
	int		c = 0;

	datap = msg_pull(msg, mISDN_HEADER_LEN);
	if (msg->len <= 2) {
		dprint(DBGM_L2, nst->cardnr, "%s: msg (%d) too short\n", __FUNCTION__,
			msg->len);
		msg_push(msg, mISDN_HEADER_LEN);
		return(ret);
	}
	psapi = *datap++;
	ptei = *datap++;
	if ((psapi & 1) || !(ptei & 1)) {
		dprint(DBGM_L2, nst->cardnr, "l2 D-channel frame wrong EA0/EA1\n");
		msg_push(msg, mISDN_HEADER_LEN);
		return(ret);
	}
	psapi >>= 2;
	ptei >>= 1;
	dprint(DBGM_L2, nst->cardnr, "%s: sapi(%d) tei(%d)\n", __FUNCTION__, psapi, ptei);
	if (ptei == GROUP_TEI) {
		if (psapi == TEI_SAPI) {
			hh = (mISDNuser_head_t *)msg_push(msg, mISDNUSER_HEAD_SIZE);
			hh->prim = MDL_UNITDATA | INDICATION;
			if (nst->feature & FEATURE_NET_PTP) {
				dprint(DBGM_L2, nst->cardnr, "%s: tei management not enabled for PTP\n", __FUNCTION__);
				return(-EINVAL);
			}
			return(tei_mux(nst, msg));
		} else {
			dprint(DBGM_L2, nst->cardnr, "%s: unknown tei(%d) msg\n", __FUNCTION__,
				ptei);
		}
	}
	l2 = select_l2(nst, psapi, ptei);
	if (!l2) {
		dprint(DBGM_L2, nst->cardnr, "%s: no l2 for sapi(%d) tei(%d)\n", __FUNCTION__,
			psapi, ptei);
		return(-ENXIO);
	}
	if (!(*datap & 1)) {	/* I-Frame */
		if(!(c = iframe_error(l2, msg)))
			ret = FsmEvent(&l2->l2m, EV_L2_I, msg);
	} else if (IsSFrame(datap, l2)) {	/* S-Frame */
		if(!(c = super_error(l2, msg)))
			ret = FsmEvent(&l2->l2m, EV_L2_SUPER, msg);
	} else if (IsUI(datap,l2)) {
		if(!(c = UI_error(l2, msg)))
			ret = FsmEvent(&l2->l2m, EV_L2_UI, msg);
	} else if (IsSABME(datap, l2)) {
		if(!(c = unnum_error(l2, msg, CMD)))
			ret = FsmEvent(&l2->l2m, EV_L2_SABME, msg);
	} else if (IsUA(datap,l2)) {
		if(!(c = unnum_error(l2, msg, RSP)))
			ret = FsmEvent(&l2->l2m, EV_L2_UA, msg);
	} else if (IsDISC(datap,l2)) {
		if(!(c = unnum_error(l2, msg, CMD)))
			ret = FsmEvent(&l2->l2m, EV_L2_DISC, msg);
	} else if (IsDM(datap,l2)) {
		if(!(c = unnum_error(l2, msg, RSP)))
			ret = FsmEvent(&l2->l2m, EV_L2_DM, msg);
	} else if (IsFRMR(datap,l2)) {
		if(!(c = FRMR_error(l2, msg)))
			ret = FsmEvent(&l2->l2m, EV_L2_FRMR, msg);
	} else {
		c = 'L';
	}
	if (c) {
		dprint(DBGM_L2, l2->nst->cardnr, "l2 D-channel frame error %c\n",c);
		FsmEvent(&l2->l2m, EV_L2_FRAME_ERROR, (void *)(long)c);
	}
	if (ret)
		free_msg(msg);
	return(0);
}

int
msg_mux(net_stack_t *nst, iframe_t *frm, msg_t *msg)
{
	layer2_t	*l2;
	int		ret = -EINVAL;
	msg_t		*nmsg;

	dprint(DBGM_L2, nst->cardnr, "%s: msg len(%d)\n", __FUNCTION__, msg->len);
	dprint(DBGM_L2, nst->cardnr, "%s: adr(%x) pr(%x) di(%x) len(%d)\n", __FUNCTION__,
		frm->addr, frm->prim, frm->dinfo, frm->len);
	l2 = nst->layer2;
	while(l2) {
		if (frm->prim == (PH_CONTROL | INDICATION)) {
			if (frm->dinfo == HW_D_BLOCKED)
				test_and_set_bit(FLG_DCHAN_BUSY, &l2->flag);
			else if (frm->dinfo == HW_D_NOBLOCKED)
				test_and_clear_bit(FLG_DCHAN_BUSY, &l2->flag);
			l2 = l2->next;
			continue;
		}
		if (l2->next) {
			nmsg = msg_copy(msg);
		} else
			nmsg = msg;
		ret = -EINVAL;
		switch (frm->prim) {
			case (PH_ACTIVATE | CONFIRM):
			case (PH_ACTIVATE | INDICATION):
				test_and_set_bit(FLG_L1_ACTIV, &l2->flag);
				if (test_and_clear_bit(FLG_ESTAB_PEND, &l2->flag))
					ret = FsmEvent(&l2->l2m,
						EV_L2_DL_ESTABLISH_REQ, nmsg);
				break;
			case (PH_DEACTIVATE | INDICATION):
			case (PH_DEACTIVATE | CONFIRM):
				test_and_clear_bit(FLG_L1_ACTIV, &l2->flag);
				ret = FsmEvent(&l2->l2m, EV_L1_DEACTIVATE, nmsg);
				break;
			default:
				l2m_debug(&l2->l2m, "l2 unknown pr %x", frm->prim);
				break;
		}
		if (ret)
			free_msg(nmsg);
		ret = 0;
		l2 = l2->next;
	}
	if (ret)
		free_msg(msg);
	return(0);
}

int
l2muxer(net_stack_t *nst, msg_t *msg)
{
	iframe_t	*frm;
	int		ret = -EINVAL;

	frm = (iframe_t *)msg->data;
	switch(frm->prim) {
		case (PH_DATA_IND):
			ret = ph_data_mux(nst, frm, msg);
			break;
		case (PH_DATA | CONFIRM):
			ret = phd_conf(nst, frm, msg);
			break;
		case (PH_ACTIVATE | CONFIRM):
		case (PH_ACTIVATE | INDICATION):
		case (PH_CONTROL | INDICATION):
		case (PH_DEACTIVATE | INDICATION):
		case (PH_DEACTIVATE | CONFIRM):
			ret = msg_mux(nst, frm, msg);
			break;
		default:
			dprint(DBGM_L2, nst->cardnr, "%s: pr %x\n", __FUNCTION__, frm->prim);
			break;
	}
	return(ret);
}

static int
l2from_up(net_stack_t *nst, msg_t *msg) {
	layer2_t	*l2;
	mISDNuser_head_t	*hh;
	int		ret = -EINVAL;

	if (!msg)
		return(ret);
	hh = (mISDNuser_head_t *)msg->data;
	if (msg->len < mISDN_FRAME_MIN)
		return(ret);
	dprint(DBGM_L2, nst->cardnr, "%s: prim(%x) dinfo(%x)\n", __FUNCTION__,
		hh->prim, hh->dinfo);
	l2 = select_l2(nst, SAPITEI(hh->dinfo));
	if (!l2) {
		dprint(DBGM_L2, nst->cardnr, "%s: no l2 for sapi(%d) tei(%d)\n", __FUNCTION__,
			SAPITEI(hh->dinfo));
		return(-ENXIO);
	}
	switch (hh->prim) {
		case (DL_DATA | REQUEST):
			ret = FsmEvent(&l2->l2m, EV_L2_DL_DATA, msg);
			break;
		case (DL_UNITDATA | REQUEST):
			ret = FsmEvent(&l2->l2m, EV_L2_DL_UNITDATA, msg);
			break;
		case (DL_ESTABLISH | REQUEST):
			if (test_bit(FLG_L1_ACTIV, &l2->flag)) {
				if (test_bit(FLG_LAPD, &l2->flag) ||
					test_bit(FLG_ORIG, &l2->flag)) {
					ret = FsmEvent(&l2->l2m,
						EV_L2_DL_ESTABLISH_REQ, msg);
				}
			} else {
				if (test_bit(FLG_LAPD, &l2->flag) ||
					test_bit(FLG_ORIG, &l2->flag)) {
					test_and_set_bit(FLG_ESTAB_PEND,
						&l2->flag);
				}
				ret = l2down(l2, PH_ACTIVATE | REQUEST, 0, msg);
			}
			break;
		case (DL_RELEASE | REQUEST):
			if (test_bit(FLG_LAPB, &l2->flag))
				l2down_create(l2, PH_DEACTIVATE | REQUEST,
					0, 0, NULL);
			ret = FsmEvent(&l2->l2m, EV_L2_DL_RELEASE_REQ, msg);
			break;
		case (MDL_ASSIGN | REQUEST):
			ret = FsmEvent(&l2->l2m, EV_L2_MDL_ASSIGN, msg);
			break;
		case (MDL_REMOVE | REQUEST):
			ret = FsmEvent(&l2->l2m, EV_L2_MDL_REMOVE, msg);
			break;
		case (MDL_ERROR | RESPONSE):
			ret = FsmEvent(&l2->l2m, EV_L2_MDL_ERROR, msg);
		case (MDL_STATUS | REQUEST):
			l2up_create(l2, MDL_STATUS | CONFIRM, hh->dinfo, 1,
				(void *)l2->tei);
			break;
		default:
			l2m_debug(&l2->l2m, "l2 unknown pr %04x", hh->prim);
	}
	return(ret);
}

int
tei_l2(layer2_t *l2, msg_t *msg)
{
	mISDNuser_head_t	*hh = (mISDNuser_head_t *)msg->data;
	int		ret = -EINVAL;

	if (!l2 || !msg)
		return(ret);
	dprint(DBGM_L2, l2->nst->cardnr, "%s: prim(%x)\n", __FUNCTION__, hh->prim);
	if (msg->len < mISDN_FRAME_MIN)
		return(ret);
	switch(hh->prim) {
	    case (MDL_UNITDATA | REQUEST):
		ret = l2down(l2, PH_DATA_REQ, hh->dinfo, msg);
		break;
	    case (MDL_ASSIGN | REQUEST):
		ret = FsmEvent(&l2->l2m, EV_L2_MDL_ASSIGN, msg);
		break;
	    case (MDL_REMOVE | REQUEST):
		ret = FsmEvent(&l2->l2m, EV_L2_MDL_REMOVE, msg);
		break;
	    case (MDL_ERROR | RESPONSE):
		ret = FsmEvent(&l2->l2m, EV_L2_MDL_ERROR, msg);
		break;
	    case (MDL_FINDTEI | REQUEST):
	    	ret = l2down_msg(l2, msg);
		break;
	}
	return(ret);
}

static void
l2m_debug(struct FsmInst *fi, char *fmt, ...)
{
	char	tbuf[128];
	va_list args;

	va_start(args, fmt);
	vsprintf(tbuf, fmt, args);
	dprint(DBGM_L2, fi->nst->cardnr, "L2 %s\n", tbuf);
	va_end(args);
}

static void
release_l2(layer2_t *l2)
{
	dprint(DBGM_L2, l2->nst->cardnr, "%s: sapi(%d) tei(%d) state(%d)\n", __FUNCTION__,
		l2->sapi, l2->tei, l2->l2m.state);
	FsmRemoveTimer(&l2->t200);
	FsmRemoveTimer(&l2->t203);
	msg_queue_purge(&l2->i_queue);
	msg_queue_purge(&l2->ui_queue);
	ReleaseWin(l2);
	if (test_bit(FLG_LAPD, &l2->flag))
		release_tei(l2->tm);
	REMOVE_FROM_LISTBASE(l2, l2->nst->layer2);
	free(l2);
}

#warning testing
int
tei0_active(layer2_t *l2)
{
	while(l2) {
		dprint(DBGM_L2, l2->nst->cardnr, "checking l2 with tei=%d, sapi=%d\n", l2->tei, l2->sapi);
		if (l2->tei == 0 && l2->sapi == 0)
			break;
		l2 = l2->next;
	}
	if (!l2)
		return(0);
	dprint(DBGM_L2, l2->nst->cardnr, "checking l2 with state=%d\n", l2->l2m.state);
	if (l2->l2m.state >= ST_L2_7)
		return(1);
	return(0);

}

layer2_t *
new_dl2(net_stack_t *nst, int tei) {
	layer2_t *nl2;

	if (!(nl2 = malloc(sizeof(layer2_t)))) {
		dprint(DBGM_L2, nst->cardnr, "malloc layer2 failed\n");
		return(NULL);
	}
	memset(nl2, 0, sizeof(layer2_t));
	nl2->nst = nst;
	nl2->debug = debug;
	test_and_set_bit(FLG_LAPD, &nl2->flag);
	test_and_set_bit(FLG_LAPD_NET, &nl2->flag);
	test_and_set_bit(FLG_FIXED_TEI, &nl2->flag);
	test_and_set_bit(FLG_MOD128, &nl2->flag);
	nl2->sapi = 0;
	nl2->tei = tei;
	nl2->maxlen = MAX_DFRAME_LEN;
	nl2->window = 1;
	nl2->T200 = 1000;
	nl2->N200 = 3;
	nl2->T203 = 10000;
	if (create_teimgr(nl2)) {
		free(nl2);
		return(NULL);
	}
	msg_queue_init(&nl2->i_queue);
	msg_queue_init(&nl2->ui_queue);
	InitWin(nl2);
	nl2->l2m.fsm = nst->l2fsm;
	nl2->l2m.state = ST_L2_4;
	nl2->l2m.debug = debug;
	nl2->l2m.nst = nl2->nst;
	nl2->l2m.userdata = nl2;
	nl2->l2m.userint = 0;
	nl2->l2m.printdebug = l2m_debug;
	FsmInitTimer(&nl2->l2m, &nl2->t200);
	FsmInitTimer(&nl2->l2m, &nl2->t203);
	APPEND_TO_LIST(nl2, nst->layer2);
	return(nl2);
}

int Isdnl2Init(net_stack_t *nst)
{
	layer2_t	*l2;
	msg_t		*msg;
	struct		Fsm *l2f;

	if (!(l2f = malloc(sizeof(struct Fsm))))
		return(-ENOMEM);
	nst->l2fsm = l2f;
	memset(l2f, 0, sizeof(struct Fsm));
	l2f->state_count = L2_STATE_COUNT;
	l2f->event_count = L2_EVENT_COUNT;
	l2f->strEvent = strL2Event;
	l2f->strState = strL2State;
	FsmNew(l2f, L2FnList, L2_FN_COUNT);
	TEIInit(nst);
	nst->l1_l2 = l2muxer;
	nst->l3_l2 = l2from_up;
	l2 = new_dl2(nst, 127);
	if (!l2) {
		dprint(DBGM_L2, l2->nst->cardnr, "%s: failed to create L2-instance with TEI 127\n", __FUNCTION__);
		cleanup:
		cleanup_Isdnl2(nst);
		return(-ENOMEM);
	}
	l2 = new_dl2(nst, 0);
	if (!l2) {
		dprint(DBGM_L2, l2->nst->cardnr, "%s: failed to create L2-instance with TEI 0\n", __FUNCTION__);
		goto cleanup;
	}
	if (!(nst->feature & FEATURE_NET_PTP)) {
		if ((msg = create_link_msg(MDL_REMOVE | INDICATION, 127,
			0, NULL, 0))) {
			if (l2_tei(l2->tm, msg))
				free_msg(msg);
		}
	}
	return(0);
}

void cleanup_Isdnl2(net_stack_t *nst)
{
	if(nst->layer2) {
		dprint(DBGM_L2, nst->cardnr, "%s: l2 list not empty\n", __FUNCTION__);
		while(nst->layer2)
			release_l2(nst->layer2);
	}
	TEIFree(nst);
	FsmFree(nst->l2fsm);
	free(nst->l2fsm);
}
