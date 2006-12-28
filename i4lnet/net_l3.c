/* $Id: net_l3.c,v 1.15 2006/12/28 12:24:01 jolly Exp $
 *
 * Author       Karsten Keil (keil@isdn4linux.de)
 *
 *		This file is (c) under GNU PUBLIC LICENSE
 *		For changes and modifications please read
 *		../../../Documentation/isdn/mISDN.cert
 *
 */

#include <stdlib.h>
#include "mISDNlib.h"
#include "net_l2.h"
#include "net_l3.h"
#include "l3dss1.h"
#include "helper.h"
// #include "debug.h"

const char *l3_revision = "$Revision: 1.15 $";

#define PROTO_DIS_EURO	8

#define L3_DEB_WARN	1
#define L3_DEB_PROTERR	2
#define	L3_DEB_STATE	4
#define L3_DEB_PROC	8
#define L3_DEB_CHECK	16

enum {
	ST_L3_LC_REL,
	ST_L3_LC_ESTAB_WAIT,
	ST_L3_LC_REL_DELAY, 
	ST_L3_LC_REL_WAIT,
	ST_L3_LC_ESTAB,
};

enum {
	IMSG_END_PROC,
	IMSG_END_PROC_M,
	IMSG_L2_DATA,
	IMSG_L4_DATA,
	IMSG_TIMER_EXPIRED,
	IMSG_MASTER_L2_DATA,
	IMSG_PROCEEDING_IND,
	IMSG_ALERTING_IND,
	IMSG_CONNECT_IND,
	IMSG_SEL_PROC,
	IMSG_RELEASE_CHILDS,
};

static int send_proc(layer3_proc_t *proc, int op, void *arg);
static int l3_msg(layer3_t *l3, u_int pr, int dinfo, void *arg);
static int mISDN_l3up(layer3_proc_t *, msg_t *);
static int l3down(layer3_t *l3, u_int prim, int dinfo, msg_t *msg);

struct _l3_msg {
	int	mt;
	msg_t	*msg;
};

struct stateentry {
	int state;
	int primitive;
	void (*rout) (layer3_proc_t *, int, void *);
};

#define SBIT(state)	(1<<state)
#define ALL_STATES	0x03ffffff

void
display_NR_IE(u_char *p, char *head1, char *head2)
{
	int len;
	char	txt[128];
	char	*tp = txt;

	len = *p++;
	tp += sprintf(tp, "len(%d)", len);
	if (len) {
		len--;
		tp += sprintf(tp, " plan(%x)", *p);
		if (len && !(*p & 0x80)) {
			len--;
			p++;
			tp += sprintf(tp, " pres(%x)", *p);
		}
		p++;
		tp += sprintf(tp, " ");
		while(len--)
			tp += sprintf(tp, "%c", *p++);
	}
	dprint(DBGM_L3, -1, "%s%s %s\n", head1, head2, txt);
}

static void
l3_debug(layer3_t *l3, char *fmt, ...)
{
	va_list args;
	char buf[256], *p;

	va_start(args, fmt);
	p = buf;
	p += sprintf(p, "l3 ");
	p += vsprintf(p, fmt, args);
	va_end(args);
	dprint(DBGM_L3, l3->nst->cardnr, "%s\n", buf);
}

static int
getcallref(u_char *p)
{
	int l, cr = 0;

	p++;			/* prot discr */
	l = 0xf & *p++;		/* callref length */
	if (l > 2)		/* wrong callref only 1 or 2 octet*/
		return(-2);
	if (!l)			/* dummy CallRef */
		return(-1);
	if (l == 1) {		/* BRI */
		cr = *p & 0x7f;
		cr += (*p & 0x80) << 8;
	} else {		/* PRI */
		cr = *p++ << 8;
		cr += *p;
	}

	return (cr);
}
                                                                                        
void
newl3state(layer3_proc_t *pc, int state)
{
	if (pc->l3 && pc->l3->debug & L3_DEB_STATE)
		l3_debug(pc->l3, "newstate cr %d %d%s --> %d%s", 
			 pc->callref & 0x7FFF,
			 pc->state, pc->master ? "i" : "",
			 state, pc->master ? "i" : "");
	pc->state = state;
}

static void
L3ExpireTimer(L3Timer_t *t)
{
	if (t->pc->l3->debug & L3_DEB_STATE)
		l3_debug(t->pc->l3, "timer %p nr %x expired", t, t->nr);
	send_proc(t->pc, IMSG_TIMER_EXPIRED, &t->nr);
}

void
L3InitTimer(layer3_proc_t *pc, L3Timer_t *t)
{
	t->pc = pc;
	t->tl.function = (void *) L3ExpireTimer;
	t->tl.data = (long) t;
	init_timer(&t->tl, pc->l3->nst);
}

void
L3DelTimer(L3Timer_t *t)
{
	del_timer(&t->tl);
}

int
L3AddTimer(L3Timer_t *t, int millisec, int timer_nr)
{
	if (timer_pending(&t->tl)) {
		if (t->pc && t->pc->l3)
			dprint(DBGM_L3, t->pc->l3->nst->cardnr, "L3AddTimer: timer already active!\n");
		else
			dprint(DBGM_L3, 0, "L3AddTimer: timer already active!\n");
			
		return -1;
	}
	init_timer(&t->tl, t->pc->l3->nst);
	t->nr = timer_nr;
	t->tl.expires = millisec;
	add_timer(&t->tl);
	return 0;
}

void
StopAllL3Timer(layer3_proc_t *pc)
{
	L3DelTimer(&pc->timer1);
	L3DelTimer(&pc->timer2);
		
	dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
#warning also remove flags:
	test_and_clear_bit(FLG_L3P_TIMER303_1, &pc->Flags);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->Flags);
	test_and_clear_bit(FLG_L3P_TIMER312, &pc->Flags);
}

void
RemoveAllL3Timer(layer3_proc_t *pc)
{
	int ret;
	
	ret = remove_timer(&pc->timer1.tl);
	if (ret)
		dprint(DBGM_L3, pc->l3?pc->l3->nst->cardnr:0, "RemoveL3Timer1: ret %d\n", ret);
	ret = remove_timer(&pc->timer2.tl);
	dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
	if (ret)
		dprint(DBGM_L3, pc->l3->nst->cardnr, "RemoveL3Timer2: ret %d\n", ret);
#warning also remove flags:
	test_and_clear_bit(FLG_L3P_TIMER303_1, &pc->Flags);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->Flags);
	test_and_clear_bit(FLG_L3P_TIMER312, &pc->Flags);
}

static layer3_proc_t *
create_proc(layer3_t *l3, int ces, int cr, layer3_proc_t *master)
{
	layer3_proc_t	*l3p;

	l3p = malloc(sizeof(layer3_proc_t));
	if (l3p) {
		memset(l3p, 0, sizeof(layer3_proc_t));
		l3p->l3 = l3;
		l3p->ces = ces;
		l3p->callref = cr;
		l3p->master = master;
		L3InitTimer(l3p, &l3p->timer1);
		L3InitTimer(l3p, &l3p->timer2);
		if (master) {
			APPEND_TO_LIST(l3p, master->child);
		}
	}	
	return(l3p);
}

static layer3_proc_t *
find_proc(layer3_proc_t *master, int ces, int cr)
{
	layer3_proc_t	*p = master;
	layer3_proc_t	*cp;

	dprint(DBGM_L3, master?master->l3->nst->cardnr:0, "%s: ces(%x) cr(%x)\n", __FUNCTION__,
		ces, cr);
	while(p) {
		dprint(DBGM_L3, p->l3->nst->cardnr, "%s: proc %p ces(%x) cr(%x)\n", __FUNCTION__,
			p, p->ces, p->callref);
		if ((p->ces == ces) && (p->callref == cr))
			break;
		if (p->child) {
			cp = find_proc(p->child, ces, cr);
			if (cp)
				return(cp);
		}
		if (((p->ces & 0xffffff00) == 0xff00) && (p->callref == cr))
			break;
		p = p->next;
	}
	return(p);
}

u_char *
findie(u_char * p, int size, u_char ie, int wanted_set)
{
	int l, codeset, maincodeset;
	u_char *pend = p + size;

	/* skip protocol discriminator, callref and message type */
	p++;
	l = (*p++) & 0xf;
	p += l;
	p++;
	codeset = 0;
	maincodeset = 0;
	/* while there are bytes left... */
	while (p < pend) {
		if ((*p & 0xf0) == 0x90) {
			codeset = *p & 0x07;
			if (!(*p & 0x08))
				maincodeset = codeset;
		}
		if (codeset == wanted_set) {
			if (*p == ie) {
				/* improved length check (Werner Cornelius) */
				if (!(*p & 0x80)) {
					if ((pend - p) < 2)
						return(NULL);
					if (*(p+1) > (pend - (p+2)))
						return(NULL);
					p++; /* points to len */
				}
				return (p);
			} else if ((*p > ie) && !(*p & 0x80))
				return (NULL);
		}
		if (!(*p & 0x80)) {
			p++;
			l = *p;
			p += l;
			codeset = maincodeset;
		}
		p++;
	}
	return (NULL);
}

u_char *
find_and_copy_ie(u_char * p, int size, u_char ie, int wanted_set, msg_t *msg)
{
	u_char *iep, *mp;
	int	l;

	iep = findie(p, size, ie, wanted_set);
	if (iep) {
		l = 1;
		if (!(ie & 0x80))
			l += *iep;
		mp = msg_put(msg, l);
		memcpy(mp, iep, l);
		iep = mp;
	}
	return(iep);
}

static void MsgStart(layer3_proc_t *pc, u_char mt) {
	pc->op = &pc->obuf[0];
	*pc->op++ = 8;
	if (pc->callref == -1) { /* dummy cr */
		*pc->op++ = 0;
	} else {
		if (pc->l3->nst->feature & FEATURE_NET_CRLEN2) {
			*pc->op++ = 2;
			*pc->op++ = (pc->callref >> 8) ^ 0x80;
			*pc->op++ = pc->callref & 0xff;
		} else {
			*pc->op++ = 1;
			*pc->op = pc->callref & 0x7f;
			if (!(pc->callref & 0x8000))
				*pc->op |= 0x80;
			pc->op++;
		}
	}
	*pc->op++ = mt;
}

static void AddvarIE(layer3_proc_t *pc, u_char ie, u_char *iep) {
	u_char len = *iep;

	*pc->op++ = ie;
	*pc->op++ = *iep++;
	while(len--)
		*pc->op++ = *iep++;	
}

static int SendMsg(layer3_proc_t *pc, int state) {
	int l;
	int ret;
	msg_t *msg;

	l = pc->op - &pc->obuf[0];
	if (!(msg = l3_alloc_msg(l)))
		return(-ENOMEM);
	memcpy(msg_put(msg, l), &pc->obuf[0], l);
	dhexprint(DBGM_L3DATA, "l3 oframe:", &pc->obuf[0], l);
	if (state != -1)
		newl3state(pc, state);
	if ((ret = l3_msg(pc->l3, DL_DATA | REQUEST, pc->ces, msg)))
		free_msg(msg);
	return(ret);
}

static int
l3dss1_message(layer3_proc_t *pc, u_char mt)
{
	msg_t *msg;
	u_char *p;
	int ret;

	if (!(msg = l3_alloc_msg(4)))
		return(-ENOMEM);
	p = msg_put(msg, 4);
	*p++ = 8;
	if (pc->l3->nst->feature & FEATURE_NET_CRLEN2) {
		*p++ = 2;
		*p++ = (pc->callref >> 8) ^ 0x80;
		*p++ = pc->callref & 0xff;
	} else {
		*p++ = 1;
		*p = pc->callref & 0x7f;
		if (!(pc->callref & 0x8000))
			*p |= 0x80;
		p++;
	}
	*p++ = mt;
	dhexprint(DBGM_L3DATA, "l3 oframe:", msg->data, 4);
	if ((ret=l3_msg(pc->l3, DL_DATA | REQUEST, pc->ces, msg)))
		free_msg(msg);
	return(ret);
}

static void
l3dss1_message_cause(layer3_proc_t *pc, u_char mt, u_char cause)
{
	MsgStart(pc, mt);
	if (cause) {
		*pc->op++ = IE_CAUSE;
		*pc->op++ = 0x2;
		*pc->op++ = 0x80 | CAUSE_LOC_PNET_LOCUSER;
		*pc->op++ = 0x80 | cause;
	}
	SendMsg(pc, -1); 
}

static void
l3dss1_status_send(layer3_proc_t *pc, u_char cause)
{

	MsgStart(pc, MT_STATUS);
	*pc->op++ = IE_CAUSE;
	*pc->op++ = 2;
	*pc->op++ = 0x80 | CAUSE_LOC_USER;
	*pc->op++ = 0x80 | cause;

	*pc->op++ = IE_CALL_STATE;
	*pc->op++ = 1;
	*pc->op++ = pc->state & 0x3f;
	SendMsg(pc, -1); 
}

static void
l3dss1_msg_without_setup(layer3_proc_t *pc, u_char cause)
{
	/* This routine is called if here was no SETUP made (checks in dss1up and in
	 * l3dss1_setup) and a RELEASE_COMPLETE have to be sent with an error code
	 * MT_STATUS_ENQUIRE in the NULL state is handled too
	 */
	switch (cause) {
		case 81:	/* invalid callreference */
		case 88:	/* incomp destination */
		case 96:	/* mandory IE missing */
		case 100:       /* invalid IE contents */
		case 101:	/* incompatible Callstate */
			l3dss1_message_cause(pc, MT_RELEASE_COMPLETE, cause);
			break;
		default:
			dprint(DBGM_L3, pc->l3->nst->cardnr, "mISDN l3dss1_msg_without_setup wrong cause %d\n",
				cause);
	}
	send_proc(pc, IMSG_END_PROC, NULL);
}

#if 0
static int
l3dss1_check_messagetype_validity(layer3_proc_t *pc, int mt, void *arg)
{
	switch (mt) {
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
		case MT_RETRIEVE:
		case MT_RESUME: /* RESUME only in user->net */
		case MT_SUSPEND: /* SUSPEND only in user->net */
			if (pc->l3->debug & L3_DEB_CHECK)
				l3_debug(pc->l3, "l3dss1_check_messagetype_validity mt(%x) OK", mt);
			break;
		default:
			if (pc->l3->debug & (L3_DEB_CHECK | L3_DEB_WARN))
				l3_debug(pc->l3, "l3dss1_check_messagetype_validity mt(%x) fail", mt);
			l3dss1_status_send(pc, CAUSE_MT_NOTIMPLEMENTED);
			return(1);
	}
	return(0);
}
#endif

static void
l3dss1_std_ie_err(layer3_proc_t *pc, int ret) {

	if (pc->l3->debug & L3_DEB_CHECK)
		l3_debug(pc->l3, "check_infoelements ret %d", ret);
	switch(ret) {
		case 0: 
			break;
		case ERR_IE_COMPREHENSION:
			l3dss1_status_send(pc, CAUSE_MANDATORY_IE_MISS);
			break;
		case ERR_IE_UNRECOGNIZED:
			l3dss1_status_send(pc, CAUSE_IE_NOTIMPLEMENTED);
			break;
		case ERR_IE_LENGTH:
			l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
			break;
		case ERR_IE_SEQUENCE:
		default:
			break;
	}
}

static u_char *
l3dss1_get_channel_id(layer3_proc_t *pc, msg_t *omsg, msg_t *nmsg) {
	u_char	*sp, *p;
	int	l;

	if ((sp = p = findie(omsg->data, omsg->len, IE_CHANNEL_ID, 0))) {
		l = *p++;
		if (pc->l3->nst->feature & FEATURE_NET_EXTCID) { /* PRI */
			if (l < 3) {
				if (pc->l3->debug & L3_DEB_WARN)
					l3_debug(pc->l3, "wrong chid len %d", *p);
				pc->err = -2;
				return (NULL);
			}
			if ((*p & 0x60) != 0x20) {
				if (pc->l3->debug & L3_DEB_WARN)
					l3_debug(pc->l3, "wrong chid %x (for PRI interface)", *p);
				pc->err = -3;
				return (NULL);
			}
			p++;
			if (*p & 0x10) {
				if (pc->l3->debug & L3_DEB_WARN)
					l3_debug(pc->l3, "wrong chid %x (channel map not supported)", *p);
				pc->err = -4;
				return (NULL);
			}
			p++;
			pc->bc = *p & 0x7f;
		} else { /* BRI */
			if (l < 1) {
				if (pc->l3->debug & L3_DEB_WARN)
					l3_debug(pc->l3, "wrong chid len %d", *p);
				pc->err = -2;
				return (NULL);
			}
			if (*p & 0x60) {
				if (pc->l3->debug & L3_DEB_WARN)
					l3_debug(pc->l3, "wrong chid %x", *p);
				pc->err = -3;
				return (NULL);
			}
			pc->bc = *p & 3;
		}
		p = sp;
		sp = msg_put(nmsg, 1 + *p);
		memcpy(sp, p, 1 + *p);
	} else
		pc->err = -1;
	return(sp);
}

static u_char *
l3dss1_get_cause(layer3_proc_t *pc, msg_t *omsg, msg_t *nmsg) {
	u_char l;
	u_char *p, *sp;

	if ((sp = p = findie(omsg->data, omsg->len, IE_CAUSE, 0))) {
		l = *p++;
		if (l>30) {
			pc->err = 1;
			return(NULL);
		}
		if (l)
			l--;
		else {
			pc->err = 2;
			return(NULL);
		}
		if (l && !(*p & 0x80)) {
			l--;
			p++; /* skip recommendation */
		}
		p++;
		if (l) {
			if (!(*p & 0x80)) {
				pc->err = 3;
				return(NULL);
			}
			pc->err = *p & 0x7F;
		} else {
			pc->err = 4;
			return(NULL);
		}
		if (nmsg) {
			p = sp;
			sp = msg_put(nmsg, 1 + *p);
			memcpy(sp, p, 1 + *p);
		}
	} else
		pc->err = -1;
	return(sp);
}

static void
l3dss1_status_enq(layer3_proc_t *proc, int pr, void *arg)
{
}

static void
l3dss1_facility(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg,*msg = arg;
	FACILITY_t	*fac;

	umsg = prep_l3data_msg(CC_FACILITY | INDICATION, 
	pc->callref>0?pc->ces | (pc->callref << 16):-1, 
	sizeof(FACILITY_t), msg->len, NULL);
	if (!umsg)
		return;
	fac = (FACILITY_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	fac->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_userinfo(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t			*umsg,*msg = arg;
	USER_INFORMATION_t	*ui;

	umsg = prep_l3data_msg(CC_USER_INFORMATION | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(USER_INFORMATION_t), msg->len, NULL);
	if (!umsg)
		return;
	ui = (USER_INFORMATION_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	ui->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_setup(layer3_proc_t *pc, int pr, void *arg)
{
	u_char	*p;
	int	bcfound = 0;
	msg_t	*umsg,*msg = arg;
	int	err = 0;
	SETUP_t	*setup;
	
	umsg = prep_l3data_msg(CC_SETUP | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(SETUP_t), msg->len, NULL);
	if (!umsg)
		return;
	setup = (SETUP_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	/*
	 * Bearer Capabilities
	 */
	/* only the first occurence 'll be detected ! */
	if ((p = setup->BEARER = find_and_copy_ie(msg->data, msg->len,
		IE_BEARER, 0, umsg))) {
		if ((p[0] < 2) || (p[0] > 11))
			err = 1;
		else {
			switch (p[1] & 0x7f) {
				case 0x00: /* Speech */
				case 0x10: /* 3.1 Khz audio */
				case 0x08: /* Unrestricted digital information */
				case 0x09: /* Restricted digital information */
				case 0x11:
					/* Unrestr. digital information  with 
					 * tones/announcements ( or 7 kHz audio
					 */
				case 0x18: /* Video */
					break;
				default:
					err = 2;
					break;
			}
			switch (p[2] & 0x7f) {
				case 0x40: /* packed mode */
				case 0x10: /* 64 kbit */
				case 0x11: /* 2*64 kbit */
				case 0x13: /* 384 kbit */
				case 0x15: /* 1536 kbit */
				case 0x17: /* 1920 kbit */
					break;
				default:
					err = 3;
					break;
			}
		}
		if (err) {
			if (pc->l3->debug & L3_DEB_WARN)
				l3_debug(pc->l3, "setup with wrong bearer(l=%d:%x,%x)",
					p[0], p[1], p[2]);
			l3dss1_msg_without_setup(pc, CAUSE_INVALID_CONTENTS);
			free_msg(umsg);
			return;
		} 
	} else {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "setup without bearer capabilities");
		/* ETS 300-104 1.3.3 */
		l3dss1_msg_without_setup(pc, CAUSE_MANDATORY_IE_MISS);
		free_msg(umsg);
		return;
	}
	/*
	 * Channel Identification
	 */
	if ((setup->CHANNEL_ID = l3dss1_get_channel_id(pc, msg, umsg))) {
		if (pc->bc) {
			bcfound++;
		} else {
			if (pc->l3->debug & L3_DEB_WARN)
				l3_debug(pc->l3, "setup without bchannel, call waiting");
			bcfound++;
		} 
	} else if (pc->err != -1) {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "setup with wrong chid ret %d", pc->err);
	}
	/* Now we are on none mandatory IEs */

	setup->COMPLETE =
		find_and_copy_ie(msg->data, msg->len, IE_COMPLETE, 0, umsg);
	setup->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	setup->PROGRESS =
		find_and_copy_ie(msg->data, msg->len, IE_PROGRESS, 0, umsg);
	setup->NET_FAC =
		find_and_copy_ie(msg->data, msg->len, IE_NET_FAC, 0, umsg);
	setup->KEYPAD =
		find_and_copy_ie(msg->data, msg->len, IE_KEYPAD, 0, umsg);
	setup->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	setup->CALLED_PN =
		find_and_copy_ie(msg->data, msg->len, IE_CALLED_PN, 0, umsg);
	setup->CALLED_SUB =
		find_and_copy_ie(msg->data, msg->len, IE_CALLED_SUB, 0, umsg);
	setup->CALLING_PN =
		find_and_copy_ie(msg->data, msg->len, IE_CALLING_PN, 0, umsg);
	setup->CALLING_SUB =
		find_and_copy_ie(msg->data, msg->len, IE_CALLING_SUB, 0, umsg);
	setup->REDIR_NR =
		find_and_copy_ie(msg->data, msg->len, IE_REDIR_NR, 0, umsg);
	setup->LLC =
		find_and_copy_ie(msg->data, msg->len, IE_LLC, 0, umsg);
	setup->HLC =
		find_and_copy_ie(msg->data, msg->len, IE_HLC, 0, umsg);
	setup->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	setup->ces = pc->ces;
	newl3state(pc, 1);
	L3DelTimer(&pc->timer2);
	dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
	L3AddTimer(&pc->timer2, T_CTRL, 0x31f);
	if (err) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, err);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}


static void
l3dss1_disconnect(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg,*msg = arg;
	DISCONNECT_t	*disc;

	if (pc->state == 19) {
	//	printf("We're in State 19, receive disconnect, so we stay here\n");
		return ;
	}


	umsg = prep_l3data_msg(CC_DISCONNECT | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(DISCONNECT_t), msg->len, NULL);
	if (!umsg)
		return;
	disc = (DISCONNECT_t *)(umsg->data + mISDNUSER_HEAD_SIZE);


	StopAllL3Timer(pc);
	newl3state(pc, 11);

	if (!(disc->CAUSE = l3dss1_get_cause(pc, msg, umsg))) {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "DISC get_cause ret(%d)", pc->err);
	} 
	disc->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	disc->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	disc->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_disconnect_i(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg,*msg = arg;
	DISCONNECT_t	*disc;
	u_char		cause = 0;

	umsg = prep_l3data_msg(CC_DISCONNECT | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(DISCONNECT_t), msg->len, NULL);
	if (!umsg)
		return;
	disc = (DISCONNECT_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	StopAllL3Timer(pc);
	if (!(disc->CAUSE = l3dss1_get_cause(pc, msg, umsg))) {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "DISC get_cause ret(%d)", pc->err);
		if (pc->err<0)
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (pc->err>0)
			cause = CAUSE_INVALID_CONTENTS;
	}
	disc->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	disc->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	disc->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	if (cause)
		l3dss1_message_cause(pc, MT_RELEASE, cause);
	else
		l3dss1_message(pc, MT_RELEASE);
	newl3state(pc, 19);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->Flags);
	L3AddTimer(&pc->timer1, T308, 0x308);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_information(layer3_proc_t *pc, int pr, void *arg) {
	msg_t 		*umsg, *msg = arg;
	INFORMATION_t	*info;

	umsg = prep_l3data_msg(CC_INFORMATION | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(INFORMATION_t), msg->len, NULL);
	if (!umsg)
		return;
	info = (INFORMATION_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	info->COMPLETE =
		find_and_copy_ie(msg->data, msg->len, IE_COMPLETE, 0, umsg);
	info->KEYPAD =
		find_and_copy_ie(msg->data, msg->len, IE_KEYPAD, 0, umsg);
	info->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	info->CALLED_PN =
		find_and_copy_ie(msg->data, msg->len, IE_CALLED_PN, 0, umsg);
	if (pc->state == 2) { /* overlap receiving */
		L3DelTimer(&pc->timer1);
		L3AddTimer(&pc->timer1, T302, 0x302);
	}
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_release(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	RELEASE_t	*rel;
	int 		cause=0;

	umsg = prep_l3data_msg(CC_RELEASE | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(RELEASE_t), msg->len, NULL);
	if (!umsg)
		return;
	rel = (RELEASE_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	StopAllL3Timer(pc);
	if (!(rel->CAUSE = l3dss1_get_cause(pc, msg, umsg))) {
		if (pc->state != 12)
			if (pc->l3->debug & L3_DEB_WARN)
				l3_debug(pc->l3, "REL get_cause ret(%d)",
					pc->err);
		if ((pc->err<0) && (pc->state != 12))
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (pc->err>0)
			cause = CAUSE_INVALID_CONTENTS;
	}
	rel->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	rel->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	rel->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	if (cause)
		l3dss1_message_cause(pc, MT_RELEASE_COMPLETE, cause);
	else
		l3dss1_message(pc, MT_RELEASE_COMPLETE);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);

/*	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
*/
}

static void
l3dss1_release_i(layer3_proc_t *pc, int pr, void *arg)
{

	l3dss1_message(pc, MT_RELEASE_COMPLETE);
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_release_cmpl(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t			*umsg, *msg = arg;
	RELEASE_COMPLETE_t	*relc;

	umsg = prep_l3data_msg(CC_RELEASE_COMPLETE | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(RELEASE_COMPLETE_t), msg->len, NULL);
	if (!umsg)
		return;
	relc = (RELEASE_COMPLETE_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	StopAllL3Timer(pc);
	newl3state(pc, 0);
	if (!(relc->CAUSE = l3dss1_get_cause(pc, msg, umsg))) {
		if (pc->err > 0)
			if (pc->l3->debug & L3_DEB_WARN)
				l3_debug(pc->l3, "RELCMPL get_cause err(%d)",
					pc->err);
	}
	relc->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	relc->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	relc->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_release_cmpl_i(layer3_proc_t *pc, int pr, void *arg)
{
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_setup_acknowledge_i(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t			*umsg, *msg = arg;
	SETUP_ACKNOWLEDGE_t	*sa;

	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 25);
		return;
	}
	umsg = prep_l3data_msg(CC_SETUP_ACKNOWLEDGE | INDICATION, pc->master->ces |
		(pc->master->callref << 16), sizeof(SETUP_ACKNOWLEDGE_t), msg->len, NULL);
	if (!umsg)
		return;
	sa = (SETUP_ACKNOWLEDGE_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	L3DelTimer(&pc->timer1);	/* T304 */
	newl3state(pc, 25);
	sa->CHANNEL_ID =
		find_and_copy_ie(msg->data, msg->len, IE_CHANNEL_ID, 0, umsg);
	sa->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	sa->PROGRESS =
		find_and_copy_ie(msg->data, msg->len, IE_PROGRESS, 0, umsg);
	if (!mISDN_l3up(pc->master, umsg))
		return;
	free_msg(umsg);
}

static void
l3dss1_proceeding_i(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t			*umsg, *msg = arg;
	CALL_PROCEEDING_t	*proc;

	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 9);
		return;
	}
	umsg = prep_l3data_msg(CC_PROCEEDING | INDICATION, pc->master->ces |
		(pc->master->callref << 16), sizeof(CALL_PROCEEDING_t), msg->len, NULL);
	if (!umsg)
		return;
	proc = (CALL_PROCEEDING_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	L3DelTimer(&pc->timer1);	/* T304 */
	newl3state(pc, 9);
	proc->CHANNEL_ID =
		find_and_copy_ie(msg->data, msg->len, IE_CHANNEL_ID, 0, umsg);
	proc->BEARER =
		find_and_copy_ie(msg->data, msg->len, IE_BEARER, 0, umsg);
	proc->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	proc->PROGRESS =
		find_and_copy_ie(msg->data, msg->len, IE_PROGRESS, 0, umsg);
	proc->HLC =
		find_and_copy_ie(msg->data, msg->len, IE_HLC, 0, umsg);
	if (!mISDN_l3up(pc->master, umsg))
		return;
	free_msg(umsg);
}

static void
l3dss1_alerting_i(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	ALERTING_t	*al;

	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 7);
		return;
	}
	umsg = prep_l3data_msg(CC_ALERTING | INDICATION, pc->master->ces |
		(pc->master->callref << 16), sizeof(ALERTING_t), msg->len, NULL);
	if (!umsg)
		return;
	al = (ALERTING_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	L3DelTimer(&pc->timer1);	/* T304 */
	newl3state(pc, 7);
	al->CHANNEL_ID =
		find_and_copy_ie(msg->data, msg->len, IE_CHANNEL_ID, 0, umsg);
	al->BEARER =
		find_and_copy_ie(msg->data, msg->len, IE_BEARER, 0, umsg);
	al->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	al->PROGRESS =
		find_and_copy_ie(msg->data, msg->len, IE_PROGRESS, 0, umsg);
	al->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	al->HLC =
		find_and_copy_ie(msg->data, msg->len, IE_HLC, 0, umsg);
	al->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	al->REDIR_DN =
		find_and_copy_ie(msg->data, msg->len, IE_REDIR_DN, 0, umsg);
	if (!mISDN_l3up(pc->master, umsg))
		return;
	free_msg(umsg);
}

#if 0
static void
l3dss1_call_proc(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t			*umsg, *msg = arg;
	int			ret = 0;
	u_char			cause;
	CALL_PROCEEDING_t	*cp;

	umsg = prep_l3data_msg(CC_PROCEEDING | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(CALL_PROCEEDING_t), msg->len, NULL);
	if (!umsg)
		return;
	cp = (CALL_PROCEEDING_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	if ((cp->CHANNEL_ID = l3dss1_get_channel_id(pc, msg, umsg))) {
		if (!(pc->l3->nst->feature & FEATURE_NET_EXTCID)) { /* BRI */
			if ((0 == pc->bc) || (3 == pc->bc)) {
				if (pc->l3->debug & L3_DEB_WARN)
					l3_debug(pc->l3, "setup answer with wrong chid %x", pc->bc);
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_msg(umsg);
				return;
			}
		}
	} else if (1 == pc->state) {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "setup answer wrong chid (ret %d)", pc->err);
		if (pc->err == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_msg(umsg);
		return;
	}
	/* Now we are on none mandatory IEs */
	cp->BEARER =
		find_and_copy_ie(msg->data, msg->len, IE_BEARER, 0, umsg);
	cp->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	cp->PROGRESS =
		find_and_copy_ie(msg->data, msg->len, IE_PROGRESS, 0, umsg);
	cp->DISPLAY =
		find_and_copy_ie(msg->data, msg->len, IE_DISPLAY, 0, umsg);
	cp->REDIR_DN =
		find_and_copy_ie(msg->data, msg->len, IE_REDIR_DN, 0, umsg);
	cp->HLC =
		find_and_copy_ie(msg->data, msg->len, IE_HLC, 0, umsg);
	L3DelTimer(&pc->timer1);
	newl3state(pc, 3);
	L3AddTimer(&pc->timer1, T310, 0x310);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}
#endif

static void
l3dss1_connect_i(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	CONNECT_t	*conn;

	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 8);
		return;
	}
	umsg = prep_l3data_msg(CC_CONNECT | INDICATION, pc->master->ces |
		(pc->master->callref << 16), sizeof(CONNECT_t), msg->len, NULL);
	if (!umsg)
		return;
	conn = (CONNECT_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	L3DelTimer(&pc->timer1);	/* T310 */
	newl3state(pc, 8);
	conn->BEARER =
		find_and_copy_ie(msg->data, msg->len, IE_BEARER, 0, umsg);
	conn->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	conn->PROGRESS =
		find_and_copy_ie(msg->data, msg->len, IE_PROGRESS, 0, umsg);
	conn->DISPLAY =
		find_and_copy_ie(msg->data, msg->len, IE_DISPLAY, 0, umsg);
	conn->DATE =
		find_and_copy_ie(msg->data, msg->len, IE_DATE, 0, umsg);
	conn->SIGNAL =
		find_and_copy_ie(msg->data, msg->len, IE_SIGNAL, 0, umsg);
	conn->CONNECT_PN =
		find_and_copy_ie(msg->data, msg->len, IE_CONNECT_PN, 0, umsg);
	conn->CONNECT_SUB =
		find_and_copy_ie(msg->data, msg->len, IE_CONNECT_SUB, 0, umsg);
	conn->HLC =
		find_and_copy_ie(msg->data, msg->len, IE_HLC, 0, umsg);
	conn->LLC =
		find_and_copy_ie(msg->data, msg->len, IE_LLC, 0, umsg);
	conn->USER_USER =
		find_and_copy_ie(msg->data, msg->len, IE_USER_USER, 0, umsg);
	conn->ces = pc->ces;
	if (send_proc(pc, IMSG_CONNECT_IND, umsg))
		free_msg(umsg); 
}

static void
l3dss1_hold(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	HOLD_t		*hold;

	if (!(pc->l3->nst->feature & FEATURE_NET_HOLD)) {
		l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_MT_NOTIMPLEMENTED);
		return;
	}
	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
#warning TODO: global mask for supported none mandatory services, like HOLD
	if (pc->hold_state == HOLDAUX_HOLD_IND)
		return;
	if (pc->hold_state != HOLDAUX_IDLE) {
		l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_NOTCOMPAT_STATE);
		return;
	}
	pc->hold_state = HOLDAUX_HOLD_IND; 

	umsg = prep_l3data_msg(CC_HOLD | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(HOLD_t), msg->len, NULL);
	if (!umsg)
		return;
	hold = (HOLD_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_retrieve(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	RETRIEVE_t	*retr;

	if (!(pc->l3->nst->feature & FEATURE_NET_HOLD)) {
		l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_MT_NOTIMPLEMENTED);
		return;
	}
	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	if (pc->hold_state == HOLDAUX_RETR_IND)
		return;
	if (pc->hold_state != HOLDAUX_HOLD) {
		l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_NOTCOMPAT_STATE);
		return;
	}
	pc->hold_state = HOLDAUX_RETR_IND;

	umsg = prep_l3data_msg(CC_RETRIEVE | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(RETRIEVE_t), msg->len, NULL);
	if (!umsg)
		return;
	retr = (RETRIEVE_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	retr->CHANNEL_ID =
		find_and_copy_ie(msg->data, msg->len, IE_CHANNEL_ID, 0, umsg);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_suspend(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	SUSPEND_t	*susp;

	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	umsg = prep_l3data_msg(CC_SUSPEND | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(SUSPEND_t), msg->len, NULL);
	if (!umsg)
		return;
	susp = (SUSPEND_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	susp->CALL_ID =
		find_and_copy_ie(msg->data, msg->len, IE_CALL_ID, 0, umsg);
	susp->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	newl3state(pc, 15);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static void
l3dss1_resume(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t		*umsg, *msg = arg;
	RESUME_t	*res;

	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	umsg = prep_l3data_msg(CC_RESUME | INDICATION, pc->ces |
		(pc->callref << 16), sizeof(RESUME_t), msg->len, NULL);
	if (!umsg)
		return;
	res = (RESUME_t *)(umsg->data + mISDNUSER_HEAD_SIZE);
	res->CALL_ID =
		find_and_copy_ie(msg->data, msg->len, IE_CALL_ID, 0, umsg);
	res->FACILITY =
		find_and_copy_ie(msg->data, msg->len, IE_FACILITY, 0, umsg);
	res->ces = pc->ces;
	newl3state(pc, 17);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}

static struct stateentry datastatelist[] =
{
	{ALL_STATES,
		MT_STATUS_ENQUIRY, l3dss1_status_enq},
	{ALL_STATES,
		MT_FACILITY, l3dss1_facility},
	{SBIT(19),
		MT_STATUS, l3dss1_release_cmpl},
	{SBIT(0),
		MT_SETUP, l3dss1_setup},
#warning setup ack
	{SBIT(6) | SBIT(7)  | SBIT(9) | SBIT(25),
		MT_SETUP_ACKNOWLEDGE, l3dss1_setup_acknowledge_i},
	{SBIT(6) | SBIT(7)  | SBIT(9) | SBIT(25),
		MT_CALL_PROCEEDING, l3dss1_proceeding_i},
	{SBIT(6) | SBIT(7)  | SBIT(9) | SBIT(25),
		MT_ALERTING, l3dss1_alerting_i},
	{SBIT(6) | SBIT(7)  | SBIT(9) | SBIT(25),
		MT_CONNECT, l3dss1_connect_i},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) | SBIT(19),
		MT_DISCONNECT, l3dss1_disconnect},
	{SBIT(7) | SBIT(8) | SBIT(9),
		MT_DISCONNECT, l3dss1_disconnect_i},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
		MT_INFORMATION, l3dss1_information},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17),
		MT_RELEASE, l3dss1_release},
	{SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(19) | SBIT(25),
		MT_RELEASE, l3dss1_release_i},
	{SBIT(0) | SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19),
		MT_RELEASE_COMPLETE, l3dss1_release_cmpl},
	{SBIT(4) | SBIT(7) | SBIT(10),
		MT_USER_INFORMATION, l3dss1_userinfo},
	{SBIT(7) | SBIT(8) | SBIT(9) | SBIT(25),
		MT_RELEASE_COMPLETE, l3dss1_release_cmpl_i},
	{SBIT(3) | SBIT(4) | SBIT(10),
		MT_HOLD, l3dss1_hold},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
		MT_RETRIEVE, l3dss1_retrieve},
	{SBIT(10),
		MT_SUSPEND, l3dss1_suspend},
	{SBIT(0),
		MT_RESUME, l3dss1_resume},
};

#define DATASLLEN \
	(sizeof(datastatelist) / sizeof(struct stateentry))

static layer3_proc_t
*create_child_proc(layer3_proc_t *pc, int mt, msg_t *msg, int state) {
	mISDNuser_head_t	*hh;
	struct _l3_msg	l3m;
	layer3_proc_t	*p3i;

	hh = (mISDNuser_head_t *)msg->data;
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	p3i = create_proc(pc->l3, hh->dinfo, pc->callref, pc);
	if (!p3i) {
		l3_debug(pc->l3, "cannot create child\n");
		return(NULL);
	}
	p3i->state = pc->state;
	if (pc->state != -1)
		newl3state(pc, state);
	l3m.mt = mt;
	l3m.msg = msg;
	send_proc(p3i, IMSG_L2_DATA, &l3m);
	return(p3i);
}                                                   

static void
l3dss1_setup_acknowledge_m(layer3_proc_t *pc, int pr, void *arg)
{
	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	create_child_proc(pc, pr, arg, 25);
}

static void
l3dss1_proceeding_m(layer3_proc_t *pc, int pr, void *arg)
{
	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	create_child_proc(pc, pr, arg, 9);
}

static void
l3dss1_alerting_m(layer3_proc_t *pc, int pr, void *arg)
{
	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	create_child_proc(pc, pr, arg, 7);
}

static void
l3dss1_connect_m(layer3_proc_t *pc, int pr, void *arg)
{
	dprint(DBGM_L3, pc->l3->nst->cardnr,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	create_child_proc(pc, pr, arg, 8);
}

static void
l3dss1_release_m(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t	*msg = arg;
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	l3dss1_release_i(pc, pr, msg);
}

static void
l3dss1_release_mx(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t	*msg = arg;

	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	l3dss1_release(pc, pr, msg);
}

static void
l3dss1_release_cmpl_m(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t	*msg = arg;
	u_char	*p;

	if (pc->state == 6) {
		msg_pull(msg, mISDNUSER_HEAD_SIZE);
		if ((p = l3dss1_get_cause(pc, msg, NULL))) {
			dprint(DBGM_L3, pc->l3->nst->cardnr,"%s cause (%d/%d)\n", __FUNCTION__,
				pc->cause, pc->err);
			switch(pc->cause) {
				case CAUSE_USER_BUSY:
					break;
				case CAUSE_CALL_REJECTED:
					if (pc->err == CAUSE_USER_BUSY)
						pc->cause = pc->err;
					break;
				default:
					pc->cause = pc->err;
			}
		}
		test_and_set_bit(FLG_L3P_GOTRELCOMP, &pc->Flags);
	}
}

static void
l3dss1_release_cmpl_mx(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t	*msg = arg;

	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	l3dss1_release_cmpl(pc, pr, msg);
}

static void
l3dss1_information_mx(layer3_proc_t *pc, int pr, void *arg)
{
	msg_t	*msg = arg;

	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	l3dss1_information(pc, pr, msg);
}

static struct stateentry mdatastatelist[] =
{
#warning setup acknowledge
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
		MT_SETUP_ACKNOWLEDGE, l3dss1_setup_acknowledge_m},
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
		MT_CALL_PROCEEDING, l3dss1_proceeding_m},
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
		MT_ALERTING, l3dss1_alerting_m},
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
		MT_CONNECT, l3dss1_connect_m},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
		MT_INFORMATION, l3dss1_information_mx},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) | SBIT(11) |
	 SBIT(12) | SBIT(15) | SBIT(17),
		MT_RELEASE, l3dss1_release_mx},
	{SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(22) | SBIT(25),
		MT_RELEASE, l3dss1_release_m},
	{SBIT(19),  MT_RELEASE, l3dss1_release_cmpl},
	{SBIT(0) | SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19),
		MT_RELEASE_COMPLETE, l3dss1_release_cmpl_mx},
	{SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(22) | SBIT(25),
		MT_RELEASE_COMPLETE, l3dss1_release_cmpl_m},
};

#define MDATASLLEN \
	(sizeof(mdatastatelist) / sizeof(struct stateentry))
                                                   
static void
l3dss1_setup_ack_req(layer3_proc_t *pc, int pr, void *arg)
{
	SETUP_ACKNOWLEDGE_t *setup_ack = arg;

	if (setup_ack) {
		MsgStart(pc, MT_SETUP_ACKNOWLEDGE);
		if (setup_ack->CHANNEL_ID) {
			if (setup_ack->CHANNEL_ID[0] == 1)
				pc->bc = setup_ack->CHANNEL_ID[1] & 3;
			AddvarIE(pc, IE_CHANNEL_ID, setup_ack->CHANNEL_ID);
		}
		if (setup_ack->FACILITY)
			AddvarIE(pc, IE_FACILITY, setup_ack->FACILITY);
		if (setup_ack->PROGRESS)
			AddvarIE(pc, IE_PROGRESS, setup_ack->PROGRESS);
		if (setup_ack->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, setup_ack->DISPLAY);
		SendMsg(pc, 2);
	} else {
		newl3state(pc, 2);
		l3dss1_message(pc, MT_SETUP_ACKNOWLEDGE);
	}
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T302, 0x302);
}

static void
l3dss1_proceed_req(layer3_proc_t *pc, int pr, void *arg)
{
	CALL_PROCEEDING_t *cproc = arg;

	L3DelTimer(&pc->timer1);
	if (cproc) {
		MsgStart(pc, MT_CALL_PROCEEDING);
		if (cproc->BEARER)
			AddvarIE(pc, IE_BEARER, cproc->BEARER);
		if (cproc->CHANNEL_ID) {
			if (cproc->CHANNEL_ID[0] == 1)
				pc->bc = cproc->CHANNEL_ID[1] & 3;
			AddvarIE(pc, IE_CHANNEL_ID, cproc->CHANNEL_ID);
		}
		if (cproc->FACILITY)
			AddvarIE(pc, IE_FACILITY, cproc->FACILITY);
		if (cproc->PROGRESS)
			AddvarIE(pc, IE_PROGRESS, cproc->PROGRESS);
		if (cproc->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, cproc->DISPLAY);
		if (cproc->REDIR_DN)
			AddvarIE(pc, IE_REDIR_DN, cproc->REDIR_DN);
		if (cproc->HLC)
			AddvarIE(pc, IE_HLC, cproc->HLC);
		SendMsg(pc, 3);
	} else {
		newl3state(pc, 3);
		l3dss1_message(pc, MT_CALL_PROCEEDING);
	}
}

static void
l3dss1_alert_req(layer3_proc_t *pc, int pr, void *arg)
{
	ALERTING_t *alert = arg;

	if (alert) {
		MsgStart(pc, MT_ALERTING);
		if (alert->BEARER)
			AddvarIE(pc, IE_BEARER, alert->BEARER);
		if (alert->CHANNEL_ID) {
			if (alert->CHANNEL_ID[0] == 1)
				pc->bc = alert->CHANNEL_ID[1] & 3;
			AddvarIE(pc, IE_CHANNEL_ID, alert->CHANNEL_ID);
		}
		if (alert->FACILITY)
			AddvarIE(pc, IE_FACILITY, alert->FACILITY);
		if (alert->PROGRESS)
			AddvarIE(pc, IE_PROGRESS, alert->PROGRESS);
		if (alert->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, alert->DISPLAY);
		if (alert->HLC)
			AddvarIE(pc, IE_HLC, alert->HLC);
		if (alert->USER_USER)
			AddvarIE(pc, IE_USER_USER, alert->USER_USER);
		if (alert->REDIR_DN)
			AddvarIE(pc, IE_REDIR_DN, alert->REDIR_DN);
		SendMsg(pc, 4);
	} else {
		newl3state(pc, 4);
		l3dss1_message(pc, MT_ALERTING);
	}
	L3DelTimer(&pc->timer1);
}

static void
l3dss1_setup_req(layer3_proc_t *pc, int pr, void *arg)
{
	SETUP_t	*setup = arg;
	msg_t	*msg;
	int	l;

	MsgStart(pc, MT_SETUP);
	if (setup->COMPLETE)
		*pc->op++ = IE_COMPLETE;
	if (setup->BEARER)
		AddvarIE(pc, IE_BEARER, setup->BEARER);
	if (setup->CHANNEL_ID) {
		if (setup->CHANNEL_ID[0] == 1)
			pc->bc = setup->CHANNEL_ID[1] & 3;
		AddvarIE(pc, IE_CHANNEL_ID, setup->CHANNEL_ID);
	}
	if (setup->FACILITY)
		AddvarIE(pc, IE_FACILITY, setup->FACILITY);
	if (setup->PROGRESS)
		AddvarIE(pc, IE_PROGRESS, setup->PROGRESS);
	if (setup->NET_FAC)
		AddvarIE(pc, IE_NET_FAC, setup->NET_FAC);
	if (setup->DISPLAY)
		AddvarIE(pc, IE_DISPLAY, setup->DISPLAY);
	if (setup->KEYPAD)
		AddvarIE(pc, IE_KEYPAD, setup->KEYPAD);
	if (setup->CALLING_PN)
		AddvarIE(pc, IE_CALLING_PN, setup->CALLING_PN);
	if (setup->CALLING_SUB)
		AddvarIE(pc, IE_CALLING_SUB, setup->CALLING_SUB);
	if (setup->CALLED_PN)
		AddvarIE(pc, IE_CALLED_PN, setup->CALLED_PN);
	if (setup->CALLED_SUB)
		AddvarIE(pc, IE_CALLED_SUB, setup->CALLED_SUB);
	if (setup->LLC)
		AddvarIE(pc, IE_LLC, setup->LLC);
	if (setup->HLC)
		AddvarIE(pc, IE_HLC, setup->HLC);
	if (setup->USER_USER)
		AddvarIE(pc, IE_USER_USER, setup->USER_USER);
	
	l = pc->op - &pc->obuf[0];
	if (!(msg = l3_alloc_msg(l)))
		return;
	memcpy(msg_put(msg, l), &pc->obuf[0], l);
	newl3state(pc, 6);
	dhexprint(DBGM_L3DATA, "l3 oframe:", &pc->obuf[0], l);
#warning testing
	if (pc->l3->nst->feature & FEATURE_NET_PTP) {
		dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: proc(%p) sending SETUP to CES 0\n", __FUNCTION__, pc);
		if (l3_msg(pc->l3, DL_DATA | REQUEST, 0, msg))
			free_msg(msg);
	} else {
		dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: proc(%p) sending SETUP to broadcast CES\n", __FUNCTION__, pc);
		if (l3_msg(pc->l3, DL_UNITDATA | REQUEST, 127, msg))
			free_msg(msg);
	}
	L3DelTimer(&pc->timer1);
	test_and_clear_bit(FLG_L3P_TIMER303_1, &pc->Flags);
	L3AddTimer(&pc->timer1, T303, 0x303);
	test_and_set_bit(FLG_L3P_TIMER312, &pc->Flags);
	L3DelTimer(&pc->timer2);
	L3AddTimer(&pc->timer2, T312, 0x312);
}

static void
l3dss1_disconnect_req(layer3_proc_t *pc, int pr, void *arg);

static void
l3dss1_connect_req(layer3_proc_t *pc, int pr, void *arg)
{
	CONNECT_t *conn = arg;

	L3DelTimer(&pc->timer1);
	if (conn && conn->CHANNEL_ID) {
		if (conn->CHANNEL_ID[0] == 1)
			pc->bc = conn->CHANNEL_ID[1] & 3;
	}
#warning pc->bc is nice, but a task of the application. if you change anything, please let me know. 
#if 0
	if (!pc->bc) {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "D-chan connect for waiting call");
		l3dss1_disconnect_req(pc, pr, NULL);
		return;
	}
#endif
	if (conn) {
		MsgStart(pc, MT_CONNECT);
		if (conn->BEARER)
			AddvarIE(pc, IE_BEARER, conn->BEARER);
		if (conn->CHANNEL_ID)
			AddvarIE(pc, IE_CHANNEL_ID, conn->CHANNEL_ID);
		if (conn->FACILITY)
			AddvarIE(pc, IE_FACILITY, conn->FACILITY);
		if (conn->PROGRESS)
			AddvarIE(pc, IE_PROGRESS, conn->PROGRESS);
		if (conn->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, conn->DISPLAY);
		if (conn->DATE)
			AddvarIE(pc, IE_DATE, conn->DATE);
		if (conn->CONNECT_PN)
			AddvarIE(pc, IE_CONNECT_PN, conn->CONNECT_PN);
		if (conn->CONNECT_SUB)
			AddvarIE(pc, IE_CONNECT_SUB, conn->CONNECT_SUB);
		if (conn->LLC)
			AddvarIE(pc, IE_LLC, conn->LLC);
		if (conn->HLC)
			AddvarIE(pc, IE_HLC, conn->HLC);
		if (conn->USER_USER)
			AddvarIE(pc, IE_USER_USER, conn->USER_USER);
		SendMsg(pc, 10);
	} else {
		newl3state(pc, 10);
		l3dss1_message(pc, MT_CONNECT);
	}
}

static void
l3dss1_connect_res(layer3_proc_t *pc, int pr, void *arg)
{
	CONNECT_ACKNOWLEDGE_t	*connack = arg;
	int			cause;

	L3DelTimer(&pc->timer1);
	send_proc(pc, IMSG_SEL_PROC, NULL);
	if (connack && connack->CHANNEL_ID) {
		if (connack->CHANNEL_ID[0] == 1)
			pc->bc = connack->CHANNEL_ID[1] & 3;
	}
#if 0
	if (!pc->bc) {
		if (pc->l3->debug & L3_DEB_WARN)
			l3_debug(pc->l3, "D-chan connect for waiting call");
		l3dss1_disconnect_req(pc, pr, NULL);
		return;
	}
#endif
	if (connack) {
		MsgStart(pc, MT_CONNECT_ACKNOWLEDGE);
		if (connack->CHANNEL_ID)
			AddvarIE(pc, IE_CHANNEL_ID, connack->CHANNEL_ID);
		if (connack->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, connack->DISPLAY);
		if (connack->SIGNAL)
			AddvarIE(pc, IE_SIGNAL, connack->SIGNAL);
		SendMsg(pc, 10);
	} else {
		newl3state(pc, 10);
		l3dss1_message(pc, MT_CONNECT_ACKNOWLEDGE);
	}
	cause = CAUSE_NONSELECTED_USER;
	send_proc(pc, IMSG_RELEASE_CHILDS, &cause);
}

static void
l3dss1_disconnect_req(layer3_proc_t *pc, int pr, void *arg)
{
	DISCONNECT_t *disc = arg;

	StopAllL3Timer(pc);
	if (disc) {
		MsgStart(pc, MT_DISCONNECT);
		if (disc->CAUSE){ 
			AddvarIE(pc, IE_CAUSE, disc->CAUSE);
		} else {
			*pc->op++ = IE_CAUSE;
			*pc->op++ = 2;
			*pc->op++ = 0x80 | CAUSE_LOC_PNET_LOCUSER;
			*pc->op++ = 0x80 | CAUSE_NORMALUNSPECIFIED;
		}
		if (disc->FACILITY)
			AddvarIE(pc, IE_FACILITY, disc->FACILITY);
		if (disc->PROGRESS)
			AddvarIE(pc, IE_PROGRESS, disc->PROGRESS);
		if (disc->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, disc->DISPLAY);
		if (disc->USER_USER)
			AddvarIE(pc, IE_USER_USER, disc->USER_USER);
		SendMsg(pc, 12);
	} else {
		newl3state(pc, 12);
		l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_NORMALUNSPECIFIED);
	}
	L3AddTimer(&pc->timer1, T305, 0x305);
}

static void
l3dss1_facility_req(layer3_proc_t *pc, int pr, void *arg)
{
	FACILITY_t *fac = arg;

	if (fac) {
		MsgStart(pc, MT_FACILITY);
		if (fac->FACILITY)
			AddvarIE(pc, IE_FACILITY, fac->FACILITY);
		else
			return;
		if (fac->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, fac->DISPLAY);
		SendMsg(pc, -1);
	}
}

static void
l3dss1_userinfo_req(layer3_proc_t *pc, int pr, void *arg)
{
	USER_INFORMATION_t *ui = arg;

	if (ui) {
		MsgStart(pc, MT_USER_INFORMATION);
 		if (ui->USER_USER)
			AddvarIE(pc, IE_USER_USER, ui->USER_USER);
		else
			return;
		SendMsg(pc, -1);
	}
}

static void
l3dss1_information_req(layer3_proc_t *pc, int pr, void *arg)
{
	INFORMATION_t *info = arg;

	if (info) {
		MsgStart(pc, MT_INFORMATION);
 		if (info->COMPLETE)
			*pc->op++ = IE_COMPLETE;
		if (info->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, info->DISPLAY);
		if (info->KEYPAD)
			AddvarIE(pc, IE_KEYPAD, info->KEYPAD);
		if (info->SIGNAL)
			AddvarIE(pc, IE_SIGNAL, info->SIGNAL);
		if (info->CALLED_PN)
			AddvarIE(pc, IE_CALLED_PN, info->CALLED_PN);
		SendMsg(pc, -1);
	}
}

static void
l3dss1_progress_req(layer3_proc_t *pc, int pr, void *arg)
{
	PROGRESS_t *prog = arg;

	if (prog) {
		MsgStart(pc, MT_INFORMATION);
		if (prog->BEARER)
			AddvarIE(pc, IE_BEARER, prog->BEARER);
		if (prog->CAUSE)
			AddvarIE(pc, IE_CAUSE, prog->CAUSE);
		if (prog->FACILITY)
			AddvarIE(pc, IE_FACILITY, prog->FACILITY);
		if (prog->PROGRESS)
			AddvarIE(pc, IE_PROGRESS, prog->PROGRESS);
		else
			return;
		if (prog->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, prog->DISPLAY);
		if (prog->HLC)
			AddvarIE(pc, IE_HLC, prog->HLC);
//#warning ETSI 300286-1 only define USER_USER for USER_INFORMATION SETUP ALERTING PROGRESS CONNECT DISCONNECT RELEASE*
//		if (prog->USER_USER)
//			AddvarIE(pc, IE_USER_USER, prog->USER_USER);
		SendMsg(pc, -1);
	}
}

static void
l3dss1_notify_req(layer3_proc_t *pc, int pr, void *arg)
{
	NOTIFY_t *noti = arg;

	if (noti) {
		MsgStart(pc, MT_INFORMATION);
		if (noti->BEARER)
			AddvarIE(pc, IE_BEARER, noti->BEARER);
		if (noti->NOTIFY)
			AddvarIE(pc, IE_NOTIFY, noti->NOTIFY);
		else
			return;
		if (noti->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, noti->DISPLAY);
		if (noti->REDIR_DN)
			AddvarIE(pc, IE_REDIR_DN, noti->REDIR_DN);
		SendMsg(pc, -1);
	}
}

static void
l3dss1_disconnect_req_out(layer3_proc_t *pc, int pr, void *arg)
{
	DISCONNECT_t	*disc = arg;
	int		cause;

	if (pc->master) { /* child */
		l3dss1_disconnect_req_out(pc->master, pr, arg);
		return;
	}
	L3DelTimer(&pc->timer1);
	if (disc) {
		if (disc->CAUSE){
			cause = disc->CAUSE[2] & 0x7f;
		} else {
			cause = CAUSE_NORMALUNSPECIFIED;
		}
	}
	send_proc(pc, IMSG_RELEASE_CHILDS, &cause);
	if (test_bit(FLG_L3P_TIMER312, &pc->Flags)) {
		newl3state(pc, 22);
	} else {
		if_link(pc->l3->nst->manager, (ifunc_t)pc->l3->nst->l3_manager,
			CC_RELEASE | CONFIRM, pc->ces |
			(pc->callref << 16), 0, NULL, 0);
		newl3state(pc, 0);
		if (!pc->child)
			send_proc(pc, IMSG_END_PROC_M, NULL);
	}
}

static void
l3dss1_release_req(layer3_proc_t *pc, int pr, void *arg)
{
	RELEASE_t *rel = arg;
	
	StopAllL3Timer(pc);
	if (rel) {
		MsgStart(pc, MT_RELEASE);
		if (rel->CAUSE)
			AddvarIE(pc, IE_CAUSE, rel->CAUSE);
		if (rel->FACILITY)
			AddvarIE(pc, IE_FACILITY, rel->FACILITY);
		if (rel->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, rel->DISPLAY);
		if (rel->USER_USER)
			AddvarIE(pc, IE_USER_USER, rel->USER_USER);
		SendMsg(pc, 19);
	} else {
		newl3state(pc, 19);
		l3dss1_message(pc, MT_RELEASE);
	}
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->Flags);
	L3AddTimer(&pc->timer1, T308, 0x308);
}

static void
l3dss1_release_cmpl_req(layer3_proc_t *pc, int pr, void *arg)
{
	RELEASE_COMPLETE_t *rcmpl = arg;

	StopAllL3Timer(pc);
	if (rcmpl) {
		MsgStart(pc, MT_RELEASE_COMPLETE);
		if (rcmpl->CAUSE)
			AddvarIE(pc, IE_CAUSE, rcmpl->CAUSE);
		if (rcmpl->FACILITY)
			AddvarIE(pc, IE_FACILITY, rcmpl->FACILITY);
		if (rcmpl->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, rcmpl->DISPLAY);
		if (rcmpl->USER_USER) 
			AddvarIE(pc, IE_USER_USER, rcmpl->USER_USER);
		SendMsg(pc, 0);
	} else {
		newl3state(pc, 0);
		l3dss1_message(pc, MT_RELEASE_COMPLETE);
	}
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_t302(layer3_proc_t *pc, int pr, void *arg)
{
	{
		int t = 0x302;

		StopAllL3Timer(pc);
		if_link(pc->l3->nst->manager, (ifunc_t)pc->l3->nst->l3_manager,
			CC_TIMEOUT | INDICATION,pc->ces | (pc->callref << 16),
			sizeof(int), &t, 0);
	}
}

static void
l3dss1_t303(layer3_proc_t *pc, int pr, void *arg)
{
	int			l;
	msg_t			*msg;
	RELEASE_COMPLETE_t	*relc;

	L3DelTimer(&pc->timer1);
	if (test_bit(FLG_L3P_GOTRELCOMP, &pc->Flags)) {
		StopAllL3Timer(pc);
		msg = prep_l3data_msg(CC_RELEASE_COMPLETE | INDICATION,
			pc->ces | (pc->callref << 16),
			sizeof(RELEASE_COMPLETE_t), 3, NULL);
		if (!msg)
			return;
		relc = (RELEASE_COMPLETE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
		newl3state(pc, 0);
		relc->CAUSE = msg_put(msg, 3);
		relc->CAUSE[0] = 2;
		relc->CAUSE[1] = 0x80;
		if (pc->cause)
			relc->CAUSE[2] = pc->cause | 0x80;
		else
			relc->CAUSE[2] = CAUSE_NORMALUNSPECIFIED | 0x80;
		if (mISDN_l3up(pc, msg))
			free_msg(msg);
		send_proc(pc, IMSG_END_PROC_M, NULL);
		return;
	}
	if (!test_and_set_bit(FLG_L3P_TIMER303_1, &pc->Flags)) {
		if (pc->obuf[3] == MT_SETUP) {
			l = pc->op - &pc->obuf[0];
			dhexprint(DBGM_L3DATA, "l3 oframe:", &pc->obuf[0], l);
			if ((msg = l3_alloc_msg(l))) {
				memcpy(msg_put(msg, l), &pc->obuf[0], l);

#warning testing as well
				if (pc->l3->nst->feature & FEATURE_NET_PTP) {
					dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: proc(%p) sending SETUP to CES 0\n", __FUNCTION__, pc);
					if (l3_msg(pc->l3, DL_DATA | REQUEST, 0, msg))
						free_msg(msg);
				} else {
					dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: proc(%p) sending SETUP to broadcast CES\n", __FUNCTION__, pc);
					if (l3_msg(pc->l3, DL_UNITDATA | REQUEST, 127, msg))
						free_msg(msg);
				}
			}

			L3DelTimer(&pc->timer2);
dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
			if ( ! pc->l3->nst->feature & FEATURE_NET_PTP) {
				L3AddTimer(&pc->timer2, T312, 0x312);
				test_and_set_bit(FLG_L3P_TIMER312,
					&pc->Flags);
			}

			L3AddTimer(&pc->timer1, T303, 0x303);
			return;
		}
	}
	msg = prep_l3data_msg(CC_RELEASE_COMPLETE | INDICATION,
		pc->ces | (pc->callref << 16),
		sizeof(RELEASE_COMPLETE_t), 3, NULL);
	if (!msg)
		return;
	relc = (RELEASE_COMPLETE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	relc->CAUSE = msg_put(msg, 3);
	relc->CAUSE[0] = 2;
	relc->CAUSE[1] = 0x85;
	relc->CAUSE[2] = CAUSE_NOUSER_RESPONDING | 0x80;
	if (mISDN_l3up(pc, msg))
		free_msg(msg);
	newl3state(pc, 22);
}

static void
l3dss1_t305(layer3_proc_t *pc, int pr, void *arg)
{
	int t = 0x305;

	StopAllL3Timer(pc);


	newl3state(pc, 19);
		l3dss1_message(pc, MT_RELEASE);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->Flags);
	L3AddTimer(&pc->timer1, T308, 0x308);
}

static void
l3dss1_t308(layer3_proc_t *pc, int pr, void *arg)
{
	if (!test_and_set_bit(FLG_L3P_TIMER308_1, &pc->Flags)) {
		newl3state(pc, 19);
		L3DelTimer(&pc->timer1);
		l3dss1_message(pc, MT_RELEASE);
		L3AddTimer(&pc->timer1, T308, 0x308);
	} else {
		int t = 0x308;

		StopAllL3Timer(pc);
		newl3state(pc, 0);
		if_link(pc->l3->nst->manager, (ifunc_t)pc->l3->nst->l3_manager,
			CC_TIMEOUT | INDICATION,pc->ces | (pc->callref << 16),
			sizeof(int), &t, 0);
		send_proc(pc, IMSG_END_PROC_M, NULL);
	}
}

static void
l3dss1_t312(layer3_proc_t *pc, int pr, void *arg)
{
	int t = 0x312;

	test_and_clear_bit(FLG_L3P_TIMER312, &pc->Flags);
	L3DelTimer(&pc->timer2);
dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
	l3_debug(pc->l3, "%s: state %d", __FUNCTION__, pc->state);
	if (pc->state == 22) {
		StopAllL3Timer(pc);
		if (!pc->child) {
			if_link(pc->l3->nst->manager, (ifunc_t)pc->l3->nst->l3_manager,
				CC_TIMEOUT | INDICATION,pc->ces |
				(pc->callref << 16), sizeof(int), &t, 0);
			send_proc(pc, IMSG_END_PROC_M, NULL);
		}
	}
}

static void
l3dss1_holdack_req(layer3_proc_t *pc, int pr, void *arg)
{
	HOLD_ACKNOWLEDGE_t *hack = arg;

	if (pc->hold_state != HOLDAUX_HOLD_IND)
		return;
	pc->hold_state = HOLDAUX_HOLD; 
	if (hack) {
		MsgStart(pc, MT_HOLD_ACKNOWLEDGE);
		if (hack->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, hack->DISPLAY);
		SendMsg(pc, -1);
	} else {
		l3dss1_message(pc, MT_HOLD_ACKNOWLEDGE);
	}
}

static void
l3dss1_holdrej_req(layer3_proc_t *pc, int pr, void *arg)
{
	HOLD_REJECT_t *hrej = arg;

	if (pc->hold_state != HOLDAUX_HOLD_IND)
		return;
	pc->hold_state = HOLDAUX_IDLE; 
	MsgStart(pc, MT_HOLD_REJECT);
	if (hrej) {
		if (hrej->CAUSE)
			AddvarIE(pc, IE_CAUSE, hrej->CAUSE);
		else {
			*pc->op++ = IE_CAUSE;
			*pc->op++ = 2;
			*pc->op++ = 0x80;
			*pc->op++ = 0x80 | 0x47;
		}
		if (hrej->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, hrej->DISPLAY);
	} else {
		*pc->op++ = IE_CAUSE;
		*pc->op++ = 2;
		*pc->op++ = 0x80;
		*pc->op++ = 0x80 | 0x47;
	}
	SendMsg(pc, -1);
}

static void
l3dss1_retrack_req(layer3_proc_t *pc, int pr, void *arg)
{
	RETRIEVE_ACKNOWLEDGE_t *rack = arg;

	if (pc->hold_state != HOLDAUX_RETR_IND)
		return;
	pc->hold_state = HOLDAUX_IDLE;
	if (rack) {
		MsgStart(pc, MT_RETRIEVE_ACKNOWLEDGE);
		if (rack->CHANNEL_ID)
			AddvarIE(pc, IE_CHANNEL_ID, rack->CHANNEL_ID);
		if (rack->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, rack->DISPLAY);
		SendMsg(pc, -1);
	} else {
		l3dss1_message(pc, MT_RETRIEVE_ACKNOWLEDGE);
	}
}

static void
l3dss1_retrrej_req(layer3_proc_t *pc, int pr, void *arg)
{
	RETRIEVE_REJECT_t *rrej = arg;

	if (pc->hold_state != HOLDAUX_RETR_IND)
		return;
	pc->hold_state = HOLDAUX_HOLD; 
	MsgStart(pc, MT_RETRIEVE_REJECT);
	if (rrej) {
		if (rrej->CAUSE)
			AddvarIE(pc, IE_CAUSE, rrej->CAUSE);
		else {
			*pc->op++ = IE_CAUSE;
			*pc->op++ = 2;
			*pc->op++ = 0x80;
			*pc->op++ = 0x80 | 0x47;
		}
		if (rrej->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, rrej->DISPLAY);
	} else {
		*pc->op++ = IE_CAUSE;
		*pc->op++ = 2;
		*pc->op++ = 0x80;
		*pc->op++ = 0x80 | 0x47;
	}
	SendMsg(pc, -1);
}

static void
l3dss1_suspack_req(layer3_proc_t *pc, int pr, void *arg)
{
	SUSPEND_ACKNOWLEDGE_t *sack = arg;

	StopAllL3Timer(pc);
	if (sack) {
		MsgStart(pc, MT_SUSPEND_ACKNOWLEDGE);
		if (sack->FACILITY)
			AddvarIE(pc, IE_FACILITY, sack->FACILITY);
		if (sack->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, sack->DISPLAY);
		SendMsg(pc, 0);
	} else {
		l3dss1_message(pc, MT_SUSPEND_ACKNOWLEDGE);
	}
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_susprej_req(layer3_proc_t *pc, int pr, void *arg)
{
	SUSPEND_REJECT_t *srej = arg;

	MsgStart(pc, MT_SUSPEND_REJECT);
	if (srej) {
		if (srej->CAUSE)
			AddvarIE(pc, IE_CAUSE, srej->CAUSE);
		else {
			*pc->op++ = IE_CAUSE;
			*pc->op++ = 2;
			*pc->op++ = 0x80;
			*pc->op++ = 0x80 | 0x47;
		}
		if (srej->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, srej->DISPLAY);
	} else {
		*pc->op++ = IE_CAUSE;
		*pc->op++ = 2;
		*pc->op++ = 0x80;
		*pc->op++ = 0x80 | 0x47;
	}
	SendMsg(pc, -1);
	newl3state(pc, 10);
}

static void
l3dss1_resack_req(layer3_proc_t *pc, int pr, void *arg)
{
	RESUME_ACKNOWLEDGE_t *rack = arg;

	StopAllL3Timer(pc);
	if (rack) {
		MsgStart(pc, MT_RESUME_ACKNOWLEDGE);
		if (rack->CHANNEL_ID)
			AddvarIE(pc, IE_CHANNEL_ID, rack->CHANNEL_ID);
		if (rack->FACILITY)
			AddvarIE(pc, IE_FACILITY, rack->FACILITY);
		if (rack->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, rack->DISPLAY);
		SendMsg(pc, 0);
	} else {
		l3dss1_message(pc, MT_RESUME_ACKNOWLEDGE);
	}
	newl3state(pc, 10);
}

static void
l3dss1_resrej_req(layer3_proc_t *pc, int pr, void *arg)
{
	RESUME_REJECT_t *rrej = arg;

	MsgStart(pc, MT_RESUME_REJECT);
	if (rrej) {
		if (rrej->CAUSE)
			AddvarIE(pc, IE_CAUSE, rrej->CAUSE);
		else {
			*pc->op++ = IE_CAUSE;
			*pc->op++ = 2;
			*pc->op++ = 0x80;
			*pc->op++ = 0x80 | 0x47;
		}
		if (rrej->DISPLAY)
			AddvarIE(pc, IE_DISPLAY, rrej->DISPLAY);
	} else {
		*pc->op++ = IE_CAUSE;
		*pc->op++ = 2;
		*pc->op++ = 0x80;
		*pc->op++ = 0x80 | 0x47;
	}
	SendMsg(pc, -1);
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

/* *INDENT-OFF* */
static struct stateentry downstatelist[] =
{
#if 0
	{SBIT(0),
	 CC_RESUME | REQUEST, l3dss1_resume_req},
	{SBIT(12),
	 CC_RELEASE | REQUEST, l3dss1_release_req},
	{ALL_STATES,
	 CC_RESTART | REQUEST, l3dss1_restart},
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
	 CC_CONNECT | REQUEST, l3dss1_connect_req},
	{SBIT(10),
	 CC_SUSPEND | REQUEST, l3dss1_suspend_req},
#endif
	{ALL_STATES,
		CC_RELEASE_COMPLETE | REQUEST, l3dss1_release_cmpl_req},
	{SBIT(0),
	 CC_SETUP | REQUEST, l3dss1_setup_req},
	{SBIT(1),
	 CC_SETUP_ACKNOWLEDGE | REQUEST, l3dss1_setup_ack_req},
	{SBIT(1) | SBIT(2),
	 CC_PROCEEDING | REQUEST, l3dss1_proceed_req},
	{SBIT(2) | SBIT(3),
	 CC_ALERTING | REQUEST, l3dss1_alert_req},
	{SBIT(2) | SBIT(3) | SBIT(4),
	 CC_CONNECT | REQUEST, l3dss1_connect_req},
	{SBIT(8),
	 CC_CONNECT | RESPONSE, l3dss1_connect_res},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10),
	 CC_DISCONNECT | REQUEST, l3dss1_disconnect_req},
	{ SBIT(2) | SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(25),
	 CC_DISCONNECT | REQUEST, l3dss1_disconnect_req_out},
	{ SBIT(2) | SBIT(11)
#warning bitte beachte folgendes:
/*
es ist nur erlaubt, im state 11 einen release zu schicken!
dennoch verwende der stack den release scheinbar, um einen prozess
zu releasen, wie es z.b. in l3dss1_disconnect_req_out geschieht.
der process befindet sich zu diesem zeitpunk noch im state 7, 9 oder 25.
wenn man den (Layer 4) state auf 11 ändern würde, braucht mann die folgende
zeile nicht: (bitte nachdenken, ob dies korrekt ist)
Nein glaube ich nicht. CC_RELEASE |= CC_RELEASE_CR muss aber mal ein paar Tests
machen
*/
	| SBIT(12) | SBIT(7) | SBIT(9) | SBIT(25)
	 ,CC_RELEASE | REQUEST, l3dss1_release_req},
#warning noch ein bug: wenn ein CC_DISCONNECT gesendet wird (state 7 = klingeling), dann bekommt man nur einen RELEASE_CR, aber keinen vorherigen RELEASE 
/* muss ich auch testen, keine Zeit */
	{ALL_STATES,
	 CC_FACILITY | REQUEST, l3dss1_facility_req},
	{SBIT(4) | SBIT(7) | SBIT(8) | SBIT(10),
	 CC_USER_INFORMATION | REQUEST, l3dss1_userinfo_req},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) | SBIT(11) | SBIT(12) | SBIT(25),
	 CC_INFORMATION | REQUEST, l3dss1_information_req},
	{SBIT(2) | SBIT(3) | SBIT(4),
	 CC_PROGRESS | REQUEST, l3dss1_progress_req},
	{SBIT(10) | SBIT(15),
	 CC_NOTIFY | REQUEST, l3dss1_notify_req},
	{SBIT(2),
	 CC_T302, l3dss1_t302},
	{SBIT(12),
	 CC_T305, l3dss1_t305},
	{SBIT(6),
	 CC_T303, l3dss1_t303},
	{SBIT(19),
	 CC_T308, l3dss1_t308},
	{ALL_STATES,
	 CC_T312, l3dss1_t312},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 CC_HOLD_ACKNOWLEDGE | REQUEST, l3dss1_holdack_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 CC_HOLD_REJECT | REQUEST, l3dss1_holdrej_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 CC_RETRIEVE_ACKNOWLEDGE | REQUEST, l3dss1_retrack_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 CC_RETRIEVE_REJECT | REQUEST, l3dss1_retrrej_req},
	{SBIT(15),
	 CC_SUSPEND_ACKNOWLEDGE | REQUEST, l3dss1_suspack_req},
	{SBIT(15),
	 CC_SUSPEND_REJECT | REQUEST, l3dss1_susprej_req},
	{SBIT(17),
	 CC_RESUME_ACKNOWLEDGE | REQUEST, l3dss1_resack_req},
	{SBIT(17),
	 CC_RESUME_REJECT | REQUEST, l3dss1_resrej_req},
};

#define DOWNSLLEN \
	(sizeof(downstatelist) / sizeof(struct stateentry))

static int
imsg_intrelease(layer3_proc_t *master, layer3_proc_t *child)
{
	int	cause;

	if ((!master) || (!child))
		return(-EINVAL);
	dprint(DBGM_L3, master->l3->nst->cardnr, "%s: m/c(%x/%x) state(%d/%d) m->c(%p)\n", __FUNCTION__,
		master->ces, child->ces, master->state, child->state,
		master->child);
	switch (master->state) {
		case 0:
			if (!master->child) {
				send_proc(master, IMSG_END_PROC, master);
			}
			break;
		case 6:
		case 10:
			break;
		case 19:
			send_proc(master, IMSG_END_PROC, NULL);
			break;
		case 7:
		case 9:
		case 25:
			if (master->child ||
				test_bit(FLG_L3P_TIMER312, &master->Flags)) {
	dprint(DBGM_L3, master->l3->nst->cardnr, "%s: JOLLY child=%p, flg=%d\n", __FUNCTION__, master->child, test_bit(FLG_L3P_TIMER312, &master->Flags));
#warning TODO: save cause
#warning bedenke auch, dass vielleicht overlap sending mit information-messages praktisch wäre (später PTP)
			} else {
				send_proc(master, IMSG_END_PROC, NULL);
			}
			break;
		case 8:
			if (master->selces == child->ces) {
				cause = CAUSE_NONSELECTED_USER;
				send_proc(master, IMSG_RELEASE_CHILDS, &cause);
				if (test_bit(FLG_L3P_TIMER312, &master->Flags)) {
					newl3state(master, 22);
				} else {
					if (!master->child)
						send_proc(master,
							IMSG_END_PROC, NULL);
				}
			}
			break;
		case 22:
			if (!master->child) {
				send_proc(master, IMSG_END_PROC, NULL);
			}
			break;
	}
	return(0);
}

static int
send_proc(layer3_proc_t *proc, int op, void *arg)
{
	int		i;
	layer3_proc_t	*selp;
	struct _l3_msg	*l3m = arg;
	struct _l3_msg	l3msg;

	if (proc->l3 && proc->l3->debug & L3_DEB_PROC)
		l3_debug(proc->l3, "%s: proc(%x,%d) op(%d)", __FUNCTION__,
			proc->ces, proc->callref, op);  
	switch(op) {
		case IMSG_END_PROC:
		case IMSG_END_PROC_M:
			RemoveAllL3Timer(proc);
			if (!proc->master && !arg) {
				if_link(proc->l3->nst->manager,
					(ifunc_t)proc->l3->nst->l3_manager,
					CC_RELEASE_CR | INDICATION,
					proc->ces | (proc->callref << 16),
					sizeof(int), &proc->err, 0);
			}
			while (proc->child)
				send_proc(proc->child, IMSG_END_PROC, NULL);
			if (proc->next)
				proc->next->prev = proc->prev;
			if (proc->prev)
				proc->prev->next = proc->next;
			if (proc->l3 && (proc == proc->l3->proc) )
				proc->l3->proc = proc->next;
			if (proc->master) {
				if (proc->master->child == proc)
					proc->master->child = proc->next;
				if (op == IMSG_END_PROC_M)
					imsg_intrelease(proc->master, proc);
			}
			free(proc);
			break;
		case IMSG_L2_DATA:
			for (i = 0; i < DATASLLEN; i++)
				if ((l3m->mt == datastatelist[i].primitive) &&
					((1 << proc->state) & datastatelist[i].state))
				break;
			if (i == DATASLLEN) {
				if (proc->l3->debug & L3_DEB_STATE) {
					l3_debug(proc->l3, "dss1 state %d mt %#x unhandled",
						proc->state, l3m->mt);
				}
				if ((MT_RELEASE_COMPLETE != l3m->mt) && (MT_RELEASE != l3m->mt)) {
		//			l3dss1_status_send(proc, CAUSE_NOTCOMPAT_STATE);
				}
			} else {
				if (proc->l3->debug & L3_DEB_STATE) {
					l3_debug(proc->l3, "dss1 state %d mt %x",
						proc->state, l3m->mt);
				}
				datastatelist[i].rout(proc, l3m->mt, l3m->msg);
			}
			break;
		case IMSG_MASTER_L2_DATA:
			for (i = 0; i < MDATASLLEN; i++)
				if ((l3m->mt == mdatastatelist[i].primitive) &&
					((1 << proc->state) & mdatastatelist[i].state))
				break;
			if (i == MDATASLLEN) {
				if (proc->l3->debug & L3_DEB_STATE) {
					l3_debug(proc->l3, "dss1 state %d mt %#x unhandled",
						proc->state, l3m->mt);
				}
				if ((MT_RELEASE_COMPLETE != l3m->mt) && (MT_RELEASE != l3m->mt)) {
		//			l3dss1_status_send(proc, CAUSE_NOTCOMPAT_STATE);
				}
			} else {
				if (proc->l3->debug & L3_DEB_STATE) {
					l3_debug(proc->l3, "dss1 state %d mt %x",
						proc->state, l3m->mt);
				}
				mdatastatelist[i].rout(proc, l3m->mt, l3m->msg);
			}
			break;
		case IMSG_TIMER_EXPIRED:
			i = *((int *)arg);
			l3_debug(proc->l3, "%s: timer %x", __FUNCTION__, i);
			l3m = &l3msg;
			l3m->mt = CC_TIMER | (i<<8);
			l3m->msg = NULL;
		case IMSG_L4_DATA:
			for (i = 0; i < DOWNSLLEN; i++)
				if ((l3m->mt == downstatelist[i].primitive) &&
					((1 << proc->state) & downstatelist[i].state))
				break;
			if (i == DOWNSLLEN) {
				if (proc->l3->debug & L3_DEB_STATE) {
					l3_debug(proc->l3, "dss1 state %d L4 %#x unhandled",
						proc->state, l3m->mt);
				}
			} else {
				if (proc->l3->debug & L3_DEB_STATE) {
					l3_debug(proc->l3, "dss1 state %d L4 %x",
						proc->state, l3m->mt);
				}
				if (l3m->msg)
					downstatelist[i].rout(proc, l3m->mt,
						l3m->msg->data);
				else
					downstatelist[i].rout(proc, l3m->mt,
						NULL);
			}
			break;
		case IMSG_CONNECT_IND:
			selp = proc;
			proc = proc->master;
			if (!proc)
				return(-EINVAL);
			proc->selces = selp->ces;
			newl3state(proc, 8);
			return(mISDN_l3up(proc, arg));
		case IMSG_SEL_PROC:
			selp = find_proc(proc->child, proc->selces,
				proc->callref);
			i = proc->selces | (proc->callref << 16);
			if_link(proc->l3->nst->manager, 
				(ifunc_t)proc->l3->nst->l3_manager,
				CC_NEW_CR | INDICATION, proc->ces |
				(proc->callref << 16), sizeof(int), &i, 0);
			proc->ces = proc->selces;
			send_proc(selp, IMSG_END_PROC, NULL);
			break;
		case IMSG_RELEASE_CHILDS:
			{
				RELEASE_t	*rel;
				char		cause[3];

				cause[0] = 2;
				cause[1] = CAUSE_LOC_PNET_LOCUSER | 0x80;
				cause[2] = *((int *)arg) | 0x80;
				l3msg.mt = CC_RELEASE | REQUEST;
				l3msg.msg = alloc_msg(sizeof(RELEASE_t));
				if (!l3msg.msg)
					return(-ENOMEM);
				rel = (RELEASE_t *)msg_put(l3msg.msg,
					sizeof(RELEASE_t));
				memset(rel, 0, sizeof(RELEASE_t));
				rel->CAUSE = cause;
				selp = proc->child;
				while(selp) {
					layer3_proc_t *next = selp->next;

					send_proc(selp, IMSG_L4_DATA, &l3msg);
					selp = next;
				}
				free_msg(l3msg.msg);
			}
			break;
	}
	return(0);
}

static int
dl_data_mux(layer3_t *l3, mISDNuser_head_t *hh, msg_t *msg)
{
	layer3_proc_t	*proc;
	int		ret = -EINVAL;
	int		cr;
	struct _l3_msg	l3m;

	if (!l3)
		return(ret);
	dprint(DBGM_L3, l3->nst->cardnr, "%s: len(%d)\n", __FUNCTION__, msg->len);
	dhexprint(DBGM_L3DATA, "l3 iframe:", msg->data, msg->len);
	if (msg->len < 3) {
		l3_debug(l3, "dss1 frame too short(%d)", msg->len);
		free_msg(msg);
		return(0);
	}
	if (msg->data[0] != PROTO_DIS_EURO) { 
		if (l3->debug & L3_DEB_PROTERR) {
			l3_debug(l3, "dss1%sunexpected discriminator %x message len %d",
				(hh->prim == (DL_DATA | INDICATION)) ? " " : "(broadcast) ",
				msg->data[0], msg->len);
		}
		free_msg(msg); 
		return(0);
	}
	dprint(DBGM_L3, l3->nst->cardnr, "%s: dis(%x)\n", __FUNCTION__, msg->data[0]);
	cr = getcallref(msg->data);
	dprint(DBGM_L3, l3->nst->cardnr, "%s: cr(%x)\n", __FUNCTION__, cr);
	if (msg->len < ((msg->data[1] & 0x0f) + 3)) {
		l3_debug(l3, "dss1 frame too short(%d)", msg->len);
		free_msg(msg);
		return(0);
	}
	l3m.msg = msg;
	l3m.mt = msg->data[msg->data[1] + 2];
	if (cr == -2) {  /* wrong Callref */
		if (l3->debug & L3_DEB_WARN)
			l3_debug(l3, "dss1 wrong Callref");
		free_msg(msg);
		return(0);
	} else if (cr == -1) {  /* Dummy Callref */
		if (l3m.mt == MT_FACILITY) {
			layer3_proc_t dummy;
			memset( &dummy, 0, sizeof(layer3_proc_t));
			dummy.l3 = l3;
			dummy.ces = 0;
			dummy.callref = -1;
			l3dss1_facility(&dummy, hh->prim, msg);
		}
		else if (l3->debug & L3_DEB_WARN)
			l3_debug(l3, "dss1 dummy Callref (no facility msg)");
		free_msg(msg);
		return(0);
	} else if ((((msg->data[1] & 0x0f) == 1) && (0==(cr & 0x7f))) ||
		(((msg->data[1] & 0x0f) == 2) && (0==(cr & 0x7fff)))) {
		/* Global CallRef */
		if (l3->debug & L3_DEB_STATE)
			l3_debug(l3, "dss1 Global CallRef");
//		global_handler(l3, l3m.mt, msg);
		free_msg(msg);
		return(0);
	}
	dprint(DBGM_L3, l3->nst->cardnr, "%s: mt(%x)\n", __FUNCTION__, l3m.mt);
	proc = find_proc(l3->proc, hh->dinfo, cr);
	dprint(DBGM_L3, l3->nst->cardnr, "%s: proc(%p)\n", __FUNCTION__, proc);
	if (!proc) {
		if (l3m.mt == MT_SETUP || l3m.mt == MT_RESUME) {
			/* Setup/Resume creates a new transaction process */
			if (msg->data[2] & 0x80) {
				/* Setup/Resume with wrong CREF flag */
				if (l3->debug & L3_DEB_STATE)
					l3_debug(l3, "dss1 wrong CRef flag");
				free_msg(msg);
				return(0);
			}
			dprint(DBGM_L3, l3->nst->cardnr, "%s: %s\n", __FUNCTION__, (l3m.mt==MT_SETUP)?"MT_SETUP":"MT_RESUME");
			if (!(proc = create_proc(l3, hh->dinfo, cr, NULL))) {
				/* May be to answer with RELEASE_COMPLETE and
				 * CAUSE 0x2f "Resource unavailable", but this
				 * need a new_l3_process too ... arghh
				 */
				free_msg(msg);
				return(0);
			}
			dprint(DBGM_L3, l3->nst->cardnr, "%s: proc(%p)\n", __FUNCTION__, proc);
			APPEND_TO_LIST(proc, l3->proc);
		} else {
			dprint(DBGM_L3, l3->nst->cardnr, "%s: mt(%x) do not create proc\n", __FUNCTION__,
				l3m.mt);
#warning TODO: it happens that a response to an outgoing setup is received after connect of another terminal. in this case we must release.
			free_msg(msg);
			return(0);
		}
	}
	if ((proc->ces & 0xffffff00) == 0xff00) {
		dprint(DBGM_L3, l3->nst->cardnr, "%s: master state %d found\n", __FUNCTION__,
			proc->state);
		msg_push(msg, mISDNUSER_HEAD_SIZE);
		send_proc(proc, IMSG_MASTER_L2_DATA, &l3m);
	} else
		send_proc(proc, IMSG_L2_DATA, &l3m);
	free_msg(msg);
	return(0);
}

int
l3_muxer(net_stack_t *nst, msg_t *msg)
{
	mISDNuser_head_t	*hh;
	int		ret = -EINVAL;

	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_L3, nst->cardnr, "%s: msg len(%d)\n", __FUNCTION__, msg->len);
	dprint(DBGM_L3, nst->cardnr, "%s: pr(%x) di(%x)\n", __FUNCTION__,
		hh->prim, hh->dinfo);
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	if (hh->prim == (DL_DATA | INDICATION)) {
		ret = dl_data_mux(nst->layer3, hh, msg); 
	} else {
		ret = l3_msg(nst->layer3, hh->prim, hh->dinfo, msg);
	}
	if (ret)
		free_msg(msg);
	ret = 0;
	return(ret);
}

static int
manager_l3(net_stack_t *nst, msg_t *msg)
{
	mISDNuser_head_t	*hh;
	layer3_proc_t	*proc;
	struct _l3_msg	l3m;

	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_L3, nst->cardnr, "%s: msg len(%d)\n", __FUNCTION__, msg->len);
	dprint(DBGM_L3, nst->cardnr, "%s: pr(%x) di(%x)\n", __FUNCTION__,
		hh->prim, hh->dinfo);
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	proc = find_proc(nst->layer3->proc, hh->dinfo & 0xffff,
		(hh->dinfo>>16)& 0xffff);
	if (!proc) {
		switch (hh->prim) {
			case CC_SETUP | REQUEST:
			{
				int l4id;
				nst->layer3->next_cr++;
				if (nst->feature & FEATURE_NET_CRLEN2) {
					if (nst->layer3->next_cr>32766)
						nst->layer3->next_cr = 1;
				} else {
					if (nst->layer3->next_cr>126)
						nst->layer3->next_cr = 1;
				}
				proc = create_proc(nst->layer3, hh->dinfo & 0xffff,
					nst->layer3->next_cr | 0x8000, NULL);
				if (!proc) {
					dprint(DBGM_L3, nst->cardnr, "%s: pr(%x) failed to create proc.\n",
						__FUNCTION__, hh->prim);
					free_msg(msg);
					return(0);
				}
				dprint(DBGM_L3, nst->cardnr, "%s: proc(%p)\n", __FUNCTION__, proc);
#warning testing
#if 0
printf("check for tei 0 active\n");
		l2 = nst->layer2;
		while(l2) {
			if (l2->tei == 0 && l2->sapi == 0)
				break;
			l2 = l2->next;
		}
		if (l2) if (l2->state == ST_L2_4) {
			p3i = create_proc(proc->l3, 0, proc->callref, proc);
			if (!p3i) {
				l3_debug(proc->l3, "cannot create child\n");
				return(NULL);
			}
			proc = p3i;
			
			dprint(DBGM_L3, nst->cardnr, "%s: TEI 0 is active, so we created proc(%p)\n", __FUNCTION__, proc);
			
		}
#endif
			

				APPEND_TO_LIST(proc, nst->layer3->proc);
				l4id = proc->ces | (proc->callref << 16);
				if_link(nst->manager, (ifunc_t)nst->l3_manager, CC_SETUP | CONFIRM, hh->dinfo, sizeof(int), &l4id, 0);
				}
			break;
			case DL_ESTABLISH | REQUEST: 
				if (nst->feature & FEATURE_NET_PTP) {
					l3down(nst->layer3, DL_ESTABLISH | REQUEST, 0, NULL);
					free_msg(msg);
					return 0;
				}
			break;
			default:
			break;
		}
	}
	if (!proc) {
		dprint(DBGM_L3, nst->cardnr, "%s: pr(%x) no proc id %x found\n", __FUNCTION__,
			hh->prim, hh->dinfo);
		free_msg(msg);
		return(0);
	}
	l3m.mt = hh->prim;
	if (msg->len)
		l3m.msg = msg;
	else {
		dprint(DBGM_L3, nst->cardnr, "%s: pr(%x) id(%x) zero param\n", __FUNCTION__,
			hh->prim, hh->dinfo);
		l3m.msg = NULL;
	}
	send_proc(proc, IMSG_L4_DATA, &l3m);
	free_msg(msg);
	return(0);
}

static void
release_l3(layer3_t *l3) {
	dprint(DBGM_L3, l3->nst->cardnr, "%s(%p)\n", __FUNCTION__, l3);
	while(l3->proc) {
		dprint(DBGM_L3, l3->nst->cardnr, "%s: rel_proc ces(%x)\n", __FUNCTION__,
			l3->proc->ces);
		send_proc(l3->proc, IMSG_END_PROC, NULL);
	}
	msg_queue_purge(&l3->squeue0);
	REMOVE_FROM_LISTBASE(l3, l3->nst->layer3);
	free(l3);
}

static int
mISDN_l3up(layer3_proc_t *l3p, msg_t *msg)
{
	int err = -EINVAL;

	if (!l3p || !l3p->l3 || !l3p->l3->nst)
		return(-EINVAL);
	if (l3p->l3->nst->l3_manager)
		err = l3p->l3->nst->l3_manager(l3p->l3->nst->manager, msg);
	if (err)
		dprint(DBGM_L3, l3p->l3->nst->cardnr, "%s: error %d\n", __FUNCTION__, err);
	return(err);
}

static int
l3down(layer3_t *l3, u_int prim, int dinfo, msg_t *msg) {
	int err = -EINVAL;

	if (!msg)
		err = if_link(l3->nst, l3->nst->l3_l2, prim, dinfo, 0, NULL, 0);
	else
		err = if_addhead(l3->nst, l3->nst->l3_l2, prim, dinfo, msg);
	return(err);
}

static void
send_squeue(layer3_t *l3, msg_queue_t *squeue)
{
	msg_t	*msg;

	while((msg = msg_dequeue(&l3->squeue0))) {
		if (l3->nst->l3_l2(l3->nst, msg))
			free_msg(msg);
	}
}

#warning testing
static int
remove_proc(layer3_proc_t **procp, int ces)
{
	int found = 1;
	int any = 0;
	layer3_proc_t *proc;

	if (ces > 126)
		return(0);

	while(found) {
		found = 0;
		proc = *procp;
		while(proc) {
			dprint(DBGM_L3, proc->l3->nst->cardnr, "%s: comparing %s proc(%x) ces(%x)\n", __FUNCTION__,
				(proc->master)?"child":"master", proc, proc->ces);
			if (proc->ces == ces) {
				dprint(DBGM_L3, proc->l3->nst->cardnr, "%s: found proc(%x)\n", __FUNCTION__,
					 proc);
				if (proc->master)
					send_proc(proc, IMSG_END_PROC_M, NULL);
				else
					send_proc(proc, IMSG_END_PROC, NULL);
				any = 1;
				found = 1;
				break;
			}
			if (proc->child) {
				if (remove_proc(&proc->child, ces)) {
					any = 1;
					found = 1;
					break;
				}
			}
			proc = proc->next;
		}
	}
	return(any);
}

#warning l2_state makes no sense in multipoint environment. shouldnt we use something like l2_state[ces] ?
static int
l3_msg(layer3_t *l3, u_int pr, int dinfo, void *arg)
{
	msg_t	*msg = arg, *lmsg = NULL;
#warning testing
	int	ces = dinfo & 0xffff;
	dprint(DBGM_L3, l3->nst->cardnr, "%s: pr(%x) di(%x) arg(%p)\n", __FUNCTION__,
		pr, dinfo, arg);

	if (l3->nst->feature & FEATURE_NET_PTP) dinfo=0;

	switch (pr) {
		case (DL_UNITDATA | REQUEST):
			return(l3down(l3, pr, dinfo, arg));
		case (DL_DATA | REQUEST):
			if (l3->l2_state0 == ST_L3_LC_ESTAB || ces > 0) {
				return(l3down(l3, pr, dinfo, arg));
			} else {
				if (ces == 0) {
					mISDN_addhead(pr, dinfo, msg);
					msg_queue_tail(&l3->squeue0, msg);
					l3->l2_state0 = ST_L3_LC_ESTAB_WAIT;
					l3down(l3, DL_ESTABLISH | REQUEST, dinfo, NULL);
					return(0);
				}
			}
			break;
		case (DL_DATA | CONFIRM):
			break;
		case (DL_ESTABLISH | REQUEST):
			if (ces == 0) {
				if (l3->l2_state0 != ST_L3_LC_ESTAB) {
					l3down(l3, pr, dinfo, NULL);
					l3->l2_state0 = ST_L3_LC_ESTAB_WAIT;
				}
			}
			break;
		case (DL_ESTABLISH | CONFIRM):
			if (ces == 0) {
				if (l3->l2_state0 != ST_L3_LC_REL_WAIT) {
					l3->l2_state0 = ST_L3_LC_ESTAB;
					send_squeue(l3, &l3->squeue0);
				}
			}
			if (!l3->nst->l3_manager)
				break;
			if (!(lmsg = create_link_msg(pr, dinfo, 0, NULL, 0)))
				break;
			if (l3->nst->l3_manager(l3->nst->manager, lmsg))
				free_msg(lmsg);
			break;
		case (DL_ESTABLISH | INDICATION):
			if (ces == 0) {
				if (l3->l2_state0 == ST_L3_LC_REL) {
					l3->l2_state0 = ST_L3_LC_ESTAB;
					send_squeue(l3, &l3->squeue0);
				}
			}
			if (!l3->nst->l3_manager)
				break;
			if (!(lmsg = create_link_msg(pr, dinfo, 0, NULL, 0)))
				break;
			if (l3->nst->l3_manager(l3->nst->manager, lmsg))
				free_msg(lmsg);
			break;
		case (DL_RELEASE | INDICATION):
#warning du musst alle processe releasen CC_RELEASE!!! dies geschieht z.b. wenn man das telefon vom s0-bus abnimmt und der layer-2 dadurch zusammen bricht.
#warning geschieht dies auch im TE-mode?
#warning TODO DL_RELEASE | INDICATION handling; inclusiv special state 10 (T309)
			if (ces == 0) {
				if (l3->l2_state0 == ST_L3_LC_ESTAB) {
					l3->l2_state0 = ST_L3_LC_REL;
				}
			}
			if (!l3->nst->l3_manager)
				break;
			if (!(lmsg = create_link_msg(pr, dinfo, 0, NULL, 0)))
				break;
			if (l3->nst->l3_manager(l3->nst->manager, lmsg))
				free_msg(lmsg);
			remove_proc(&l3->proc, dinfo);
			break;
		case (DL_RELEASE | CONFIRM):
			if (ces == 0) {
				if (l3->l2_state0 == ST_L3_LC_REL_WAIT) {
					l3->l2_state0 = ST_L3_LC_REL;
				}
			}
			if (!l3->nst->l3_manager)
				break;
			if (!(lmsg = create_link_msg(pr, dinfo, 0, NULL, 0)))
				break;
			if (l3->nst->l3_manager(l3->nst->manager, lmsg))
				free_msg(lmsg);
			remove_proc(&l3->proc, dinfo);
			break;
		case (DL_RELEASE | REQUEST):
			if (ces == 0) {
				if (l3->l2_state0 == ST_L3_LC_ESTAB) {
					l3down(l3, pr, dinfo, NULL);
					l3->l2_state0 = ST_L3_LC_REL_WAIT;
				}
			}
			break;
	}
	if (msg)
		free_msg(msg);
	return(0);
}

int Isdnl3Init(net_stack_t *nst)
{
	layer3_t *l3;

	l3 = malloc(sizeof(layer3_t));
	if (!l3)
		return(-ENOMEM);
	memset(l3, 0, sizeof(layer3_t));
	l3->nst = nst;
	nst->l2_l3 = l3_muxer;
	nst->manager_l3 = manager_l3;
	l3->debug = 0xff;
#warning testing
	msg_queue_init(&l3->squeue0);
	l3->l2_state0 = ST_L3_LC_REL;
	APPEND_TO_LIST(l3, nst->layer3);
	return(0);
}

void cleanup_Isdnl3(net_stack_t *nst)
{
	if (nst->layer3) {
		dprint(DBGM_L3, nst->cardnr, "%s: l3 list not empty\n", __FUNCTION__);
		while(nst->layer3)
			release_l3(nst->layer3);
	}
}
