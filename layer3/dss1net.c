/* $Id: dss1net.c,v 2.00 2007/07/08 12:24:01 kkeil Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mlayer3.h>
#include <mbuffer.h>
#include <q931.h>
#include "helper.h"
#include "dss1.h"
#include "layer3.h"
#include "debug.h"

const char *l3_revision = "$Revision: 2.00 $";

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
	IMSG_MASTER_L2_DATA,
	IMSG_PROCEEDING_IND,
	IMSG_ALERTING_IND,
	IMSG_CONNECT_IND,
	IMSG_SEL_PROC,
	IMSG_RELEASE_CHILDS,
};

static int send_proc(l3_process_t *proc, int op, void *arg);
static int dss1man(l3_process_t *proc, u_int pr, struct l3_msg *l3m);

static int
l3dss1_message(l3_process_t *pc, u_char mt)
{
	struct l3_msg	*l3m;
	int		ret;

	if (!(l3m = MsgStart(pc, mt)))
		return -ENOMEM;
	SendMsg(pc, l3m, -1);
	return(ret);
}

static void
l3dss1_message_cause(l3_process_t *pc, u_char mt, u_char cause)
{
	struct l3_msg	*l3m;
	unsigned char	c[2];

	if (!(l3m = MsgStart(pc, mt)))
		return;
	c[0] = 0x80 | CAUSE_LOC_USER;
	c[1] = 0x80 | cause;
	add_layer3_ie(l3m, IE_CAUSE, 2, c);
	SendMsg(pc, l3m, -1);
}

static void
l3dss1_status_send(l3_process_t *pc, u_char cause)
{
	struct l3_msg	*l3m;
	unsigned char	c[2];

	if (!(l3m = MsgStart(pc, MT_STATUS)))
		return;
	c[0] = 0x80 | CAUSE_LOC_USER;
	c[1] = 0x80 | cause;
	add_layer3_ie(l3m, IE_CAUSE, 2, c);
	c[0] = pc->state & 0x3f;
	add_layer3_ie(l3m, IE_CALL_STATE, 1, c);
	SendMsg(pc, l3m, -1);
}

static void
l3dss1_msg_without_setup(l3_process_t *pc, u_char cause)
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
	}
	send_proc(pc, IMSG_END_PROC, NULL);
}

static int
l3dss1_check_messagetype_validity(l3_process_t *pc, int mt)
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
		case MT_HOLD_ACKNOWLEDGE:
		case MT_HOLD_REJECT:
		case MT_RETRIEVE:
		case MT_RETRIEVE_ACKNOWLEDGE:
		case MT_RETRIEVE_REJECT:
		case MT_RESUME:
		case MT_SUSPEND:
		default:
			l3dss1_status_send(pc, CAUSE_MT_NOTIMPLEMENTED);
			return 1;
	}
	return 0;
}

static void
l3dss1_std_ie_err(l3_process_t *pc, int ret) {

	switch(ret) {
		case 0:
			break;
		case Q931_ERROR_COMPREH:
			l3dss1_status_send(pc, CAUSE_MANDATORY_IE_MISS);
			break;
		case Q931_ERROR_UNKNOWN:
			l3dss1_status_send(pc, CAUSE_IE_NOTIMPLEMENTED);
			break;
		case Q931_ERROR_IELEN:
			l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
			break;
		default:
			break;
	}
}

static int
l3dss1_get_cid(l3_process_t *pc, struct l3_msg *l3m) {

	memset(pc->cid, 0, 4); /* clear cid */

	if (!l3m->channel_id) {
		dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s no channel id\n", __FUNCTION__);
		return -1;
	}
	if (l3m->channel_id[0] < 1) {
		dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s ERROR: channel id short read\n", __FUNCTION__);
		return -2;
	}
	if (l3m->channel_id[0] > 3) {
		dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s ERROR: channel id too large\n", __FUNCTION__);
		return -3;
	}
	if (l3m->channel_id[1] & 0x40) {
		dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s ERROR: channel id for adjected channels not supported\n", __FUNCTION__);
		return -4;
	}
	if (l3m->channel_id[1] & 0x04) {
		dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s channel id with dchannel\n", __FUNCTION__);
		goto done;
	}
	if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
		if (l3m->channel_id[1] & 0x20) {
			dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s ERROR: channel id not for BRI interface\n", __FUNCTION__);
			return -11;
		}
	} else { /* primary rate */
		if (!(l3m->channel_id[1] & 0x20)) {
			dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s ERROR: channel id not for PRI interface\n", __FUNCTION__);
			return -11;
		}
		if (l3m->channel_id[0] < 3)
			goto done;
		if (l3m->channel_id[2] & 0x10) { /* map not allowed by ETSI */
			dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s ERROR: channel id uses channel map\n", __FUNCTION__);
			return -12;
		}
	}
done:
	memcpy(pc->cid, l3m->channel_id, l3m->channel_id[0] + 1);
	return 0;
}

static int
l3dss1_get_cause(l3_process_t *pc, struct l3_msg *l3m) {
	unsigned char	l;
	unsigned char	*p;

	if (l3m->cause) {
		p = l3m->cause;
		l = *p++;
		if (l>30) {
			return -30;
		}
		if (l)
			l--;
		else {
			return -2;
		}
		if (l && !(*p & 0x80)) {
			l--;
			p++; /* skip recommendation */
		}
		p++;
		if (l) {
			if (!(*p & 0x80)) {
				return(-3);
			}
			pc->rm_cause = *p & 0x7F;
		} else {
			return -4;
		}
	} else
		return -1;
	return 0;
}

static void
l3dss1_status_enq(l3_process_t *proc, unsigned int pr, struct l3_msg *l3m)
{
}

static void
l3dss1_facility(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!l3m->facility) {
		free_l3_msg(l3m);
		return;
	}
	mISDN_l3up(pc, MT_FACILITY, l3m);
}

static void
l3dss1_userinfo(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!l3m->useruser) {
		free_l3_msg(l3m);
		return;
	}
	mISDN_l3up(pc, MT_USER_INFORMATION, l3m);
}

static void
l3dss1_setup(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		err = 0;
	
	/*
	 * Bearer Capabilities
	 */
	/* only the first occurence 'll be detected ! */
	if (l3m->bearer_capability) {
		/*
		 * Application has to analyze and reject
		 */
	} else {
		/* ETS 300-104 1.3.3 */
		l3dss1_msg_without_setup(pc, CAUSE_MANDATORY_IE_MISS);
		free_l3_msg(l3m);
		return;
	}
	/* channel id is optional */
	if (!(err = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if (!test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
				/* no channel  is invalid ??? */
				if (0 == (pc->cid[1] & 3)) {
					l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
					free_l3_msg(l3m);
					return;
				}
			}
		} /* valid test for primary rate ??? */
	} else if (err != -1) {
		l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
		free_l3_msg(l3m);
		return;
	}
	/* Now we are on none mandatory IEs */
	newl3state(pc, 1);
	L3DelTimer(&pc->timer2);
//	dprint(DBGM_L3, pc->l3->nst->cardnr, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
	L3AddTimer(&pc->timer2, T_CTRL, CC_TCTRL);
//	if (err) /* STATUS for none mandatory IE errors after actions are taken */
//		l3dss1_std_ie_err(pc, err);
	mISDN_l3up(pc, MT_SETUP, l3m);
}


static void
l3dss1_disconnect(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (pc->state == 19) {
		//	printf("We're in State 19, receive disconnect, so we stay here\n");
		free_l3_msg(l3m);
		return ;
	}

	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	StopAllL3Timer(pc);
	newl3state(pc, 11);

	ret = l3dss1_get_cause(pc, l3m);
	if (ret) {
		if (pc->L3->debug & L3_DEB_WARN)
			l3_debug(pc->L3, "DISC get_cause ret(%d)", ret);
	} 
	mISDN_l3up(pc, MT_DISCONNECT, l3m);
}

static void
l3dss1_disconnect_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char	cause = 0;

	if (pc->state == 19) {
		//	printf("We're in State 19, receive disconnect, so we stay here\n");
		free_l3_msg(l3m);
		return ;
	}
	StopAllL3Timer(pc);
	ret = l3dss1_get_cause(pc, l3m);
	if (ret) {
		if (pc->L3->debug & L3_DEB_WARN)
			l3_debug(pc->L3, "DISC get_cause ret(%d)", ret);
		if (ret < 0)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
	}
	if (cause)
		l3dss1_message_cause(pc, MT_RELEASE, cause);
	else
		l3dss1_message(pc, MT_RELEASE);
	newl3state(pc, 19);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->flags);
	L3AddTimer(&pc->timer1, T308, CC_T308_1);
	mISDN_l3up(pc, MT_DISCONNECT, l3m);
}

static void
l3dss1_information(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	if (pc->state == 2) { /* overlap receiving */
		L3DelTimer(&pc->timer1);
		L3AddTimer(&pc->timer1, T302, CC_T302);
	}
	 mISDN_l3up(pc, MT_INFORMATION, l3m);
}

static void
l3dss1_release(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char 	cause = 0;

	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	StopAllL3Timer(pc);
	ret = l3dss1_get_cause(pc, l3m);
	if (ret) {
		if (pc->state != 12)
			if (pc->L3->debug & L3_DEB_WARN)
				l3_debug(pc->L3, "REL get_cause ret(%d)", ret);
		if ((ret < 0) && (pc->state != 12))
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (ret > 0)
			cause = CAUSE_INVALID_CONTENTS;
	}
	if (cause)
		l3dss1_message_cause(pc, MT_RELEASE_COMPLETE, cause);
	else
		l3dss1_message(pc, MT_RELEASE_COMPLETE);
	mISDN_l3up(pc, MT_RELEASE, l3m);

	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_release_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	l3dss1_message(pc, MT_RELEASE_COMPLETE);
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_release_cmpl(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	StopAllL3Timer(pc);
	newl3state(pc, 0);
	ret = l3dss1_get_cause(pc, l3m);
	if (ret) {
		if (ret > 0)
			if (pc->L3->debug & L3_DEB_WARN)
				l3_debug(pc->L3, "RELCMPL get_cause err(%d)", ret);
	}
	mISDN_l3up(pc, MT_RELEASE_COMPLETE, l3m);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_release_cmpl_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_setup_acknowledge_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 25);
		free_l3_msg(l3m);
		return;
	}
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	L3DelTimer(&pc->timer1);	/* T304 */
	newl3state(pc, 25);
	mISDN_l3up(pc, MT_SETUP_ACKNOWLEDGE, l3m);
}

static void
l3dss1_proceeding_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 9);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);	/* T304 */
	newl3state(pc, 9);
	mISDN_l3up(pc, MT_CALL_PROCEEDING, l3m);
}

static void
l3dss1_alerting_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 7);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);	/* T304 */
	newl3state(pc, 7);
	mISDN_l3up(pc, MT_ALERTING, l3m);
}

#if 0
static void
l3dss1_call_proc(l3_process_t *pc, unsigned int pr, void *arg)
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
		if (!(pc->L3->nst->feature & FEATURE_NET_EXTCID)) { /* BRI */
			if ((0 == pc->bc) || (3 == pc->bc)) {
				if (pc->L3->debug & L3_DEB_WARN)
					l3_debug(pc->L3, "setup answer with wrong chid %x", pc->bc);
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_msg(umsg);
				return;
			}
		}
	} else if (1 == pc->state) {
		if (pc->L3->debug & L3_DEB_WARN)
			l3_debug(pc->L3, "setup answer wrong chid (ret %d)", pc->err);
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
	L3AddTimer(&pc->timer1, T310, CC_T310);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
	if (mISDN_l3up(pc, umsg))
		free_msg(umsg);
}
#endif

static void
l3dss1_connect_i(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!pc->master) {
		L3DelTimer(&pc->timer1);
		newl3state(pc, 8);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);	/* T310 */
	newl3state(pc, 8);
	if (send_proc(pc, IMSG_CONNECT_IND, l3m))
		free_l3_msg(l3m);
}

static void
l3dss1_hold(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!test_bit(MISDN_FLG_NET_HOLD, &pc->L3->ml3.options)) {
		l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_MT_NOTIMPLEMENTED);
		return;
	}
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	if (pc->aux_state == AUX_HOLD_IND)
		return;
	if (pc->aux_state != AUX_IDLE) {
		l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_NOTCOMPAT_STATE);
		return;
	}
	pc->aux_state = AUX_HOLD_IND; 

	mISDN_l3up(pc, MT_HOLD, l3m);
}

static void
l3dss1_retrieve(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!test_bit(MISDN_FLG_NET_HOLD, &pc->L3->ml3.options)) {
		l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_MT_NOTIMPLEMENTED);
		return;
	}
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	if (pc->aux_state == AUX_RETRIEVE_IND)
		return;
	if (pc->aux_state != AUX_CALL_HELD) {
		l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_NOTCOMPAT_STATE);
		return;
	}
	pc->aux_state = AUX_RETRIEVE_IND;

	mISDN_l3up(pc, MT_RETRIEVE, l3m);
}

static void
l3dss1_suspend(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	newl3state(pc, 15);
	mISDN_l3up(pc, MT_SUSPEND, l3m);
}

static void
l3dss1_resume(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	newl3state(pc, 17);
	 mISDN_l3up(pc, MT_RESUME, l3m);
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
	{SBIT(7) | SBIT(8) | SBIT(9) | SBIT(25),
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

static l3_process_t
*create_child_proc(l3_process_t *pc, struct l3_msg *l3m, int state) {
	l3_process_t	*p3i;
	struct mbuffer	*mb = container_of(l3m, struct mbuffer, l3);

	p3i = create_new_process(pc->L3, mb->addr.channel, pc->pid & MISDN_PID_CRVAL_MASK, pc);
	if (!p3i) {
		l3_debug(pc->L3, "cannot create child\n");
		return(NULL);
	}
	p3i->state = pc->state;
	if (pc->state != -1)
		newl3state(pc, state);
	send_proc(p3i, IMSG_L2_DATA, l3m);
	return(p3i);
}                                                   

static void
l3dss1_setup_acknowledge_m(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	create_child_proc(pc, l3m, 25);
}

static void
l3dss1_proceeding_m(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	create_child_proc(pc, l3m, 9);
}

static void
l3dss1_alerting_m(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	create_child_proc(pc, l3m, 7);
}

static void
l3dss1_connect_m(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s\n", __FUNCTION__);
	L3DelTimer(&pc->timer1);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	create_child_proc(pc, l3m, 8);
}

static void
l3dss1_release_m(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	l3dss1_release_i(pc, pr, l3m);
}

static void
l3dss1_release_mx(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	l3dss1_release(pc, pr, l3m);
}

static void
l3dss1_release_cmpl_m(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (pc->state == 6) {
		ret = l3dss1_get_cause(pc, l3m);
		if (!ret) {
			dprint(DBGM_L3, pc->l2if->l2addr.dev,"%s cause (%d/%d)\n", __FUNCTION__,
				pc->rm_cause, pc->cause);
			switch(pc->rm_cause) {
				case CAUSE_USER_BUSY:
					break;
				case CAUSE_CALL_REJECTED:
					if (pc->rm_cause == CAUSE_USER_BUSY)
						pc->cause = pc->rm_cause;
					break;
				default:
					pc->cause = pc->rm_cause;
			}
		}
		test_and_set_bit(FLG_L3P_GOTRELCOMP, &pc->flags);
	}
}

static void
l3dss1_release_cmpl_mx(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	l3dss1_release_cmpl(pc, pr, l3m);
}

static void
l3dss1_information_mx(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	l3dss1_information(pc, pr, l3m);
}

static struct stateentry mdatastatelist[] =
{
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
l3dss1_setup_ack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc,l3m, 2);
	} else {
		newl3state(pc, 2);
		l3dss1_message(pc, MT_SETUP_ACKNOWLEDGE);
	}
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T302, CC_T302);
}

static void
l3dss1_proceed_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	if (l3m) {
		SendMsg(pc, l3m, 3);
	} else {
		newl3state(pc, 3);
		l3dss1_message(pc, MT_CALL_PROCEEDING);
	}
}

static void
l3dss1_alert_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, 4);
	} else {
		newl3state(pc, 4);
		l3dss1_message(pc, MT_ALERTING);
	}
	L3DelTimer(&pc->timer1);
}

static void
l3dss1_setup_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	l3_msg_increment_refcnt(l3m);
	pc->t303msg = l3m;
	SendMsg(pc, l3m, 6);
	L3DelTimer(&pc->timer1);
	test_and_clear_bit(FLG_L3P_TIMER303_1, &pc->flags);
	L3AddTimer(&pc->timer1, T303, CC_T303);
	L3DelTimer(&pc->timer2);
	if (!test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
		test_and_set_bit(FLG_L3P_TIMER312, &pc->flags);
		L3AddTimer(&pc->timer2, T312, CC_T312);
	}
}

static void
l3dss1_disconnect_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m);

static void
l3dss1_connect_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	if (l3m) {
		SendMsg(pc, l3m, 10);
	} else {
		newl3state(pc, 10);
		l3dss1_message(pc, MT_CONNECT);
	}
}

static void
l3dss1_connect_ack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	unsigned char	cause[2];

	L3DelTimer(&pc->timer1);
	send_proc(pc, IMSG_SEL_PROC, NULL);
	if (l3m) {
		SendMsg(pc, l3m, 10);
	} else {
		newl3state(pc, 10);
		l3dss1_message(pc, MT_CONNECT_ACKNOWLEDGE);
	}
	cause[0] = CAUSE_LOC_PRVN_LOCUSER | 0x80;
	cause[1] = CAUSE_NONSELECTED_USER | 0x80;
	send_proc(pc, IMSG_RELEASE_CHILDS, cause);
}

static void
l3dss1_disconnect_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	if (l3m) {
		SendMsg(pc, l3m, 12);
	} else {
		newl3state(pc, 12);
		l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_NORMALUNSPECIFIED);
	}
	L3AddTimer(&pc->timer1, T305, CC_T305);
}

static void
l3dss1_facility_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_userinfo_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_information_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->state == 25 && !test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options))
		return;
	
	if (l3m) {
//		if (pc->state != 25)
			SendMsg(pc, l3m, -1);
//		else {
//			l = pc->op - &pc->obuf[0];
//			if (!(msg = l3_alloc_msg(l)))
//				return;
//			memcpy(msg_put(msg, l), &pc->obuf[0], l);
//			dhexprint(DBGM_L3DATA, "l3 oframe:", &pc->obuf[0], l);
//			dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s: proc(%p) sending INFORMATION to CES 0 during state 25 (OVERLAP)\n", __FUNCTION__, pc);
//			if (l3_msgXXXXXX(pc->L3, DL_DATA | REQUEST, 0, msg))
//				free_msg(msg);
//		}
	}
}

static void
l3dss1_progress_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_notify_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_disconnect_req_out(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	unsigned char	cause[2];

	if (pc->master) { /* child */
		l3dss1_disconnect_req_out(pc->master, pr, l3m);
		return;
	}
	L3DelTimer(&pc->timer1);
	if (l3m) {
		if (l3m->cause){
			cause[0] = l3m->cause[2] & 0x7f;
			cause[1] = l3m->cause[3] & 0x7f;
		} else {
			cause[0] = CAUSE_LOC_PRVN_LOCUSER | 0x80;
			cause[1] = CAUSE_NORMALUNSPECIFIED | 0x80;
		}
	}
	send_proc(pc, IMSG_RELEASE_CHILDS, cause);
	if (test_bit(FLG_L3P_TIMER312, &pc->flags)) {
		newl3state(pc, 22);
	} else {
//		if_link(pc->L3->nst->manager, (ifunc_t)pc->L3->nst->L3_manager,
//			CC_RELEASE | CONFIRM, pc->ces |
//			(pc->callref << 16), 0, NULL, 0);
//		newl3state(pc, 0);
		if (list_empty(&pc->child))
			send_proc(pc, IMSG_END_PROC_M, NULL);
	}
	if (l3m)
		free_l3_msg(l3m);
}

static void
l3dss1_release_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	if (l3m) {
		SendMsg(pc, l3m, 19);
	} else {
		newl3state(pc, 19);
		l3dss1_message(pc, MT_RELEASE);
	}
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->flags);
	L3AddTimer(&pc->timer1, T308, CC_T308_1);
}

static void
l3dss1_release_cmpl_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	if (l3m) {
		SendMsg(pc, l3m, 0);
	} else {
		newl3state(pc, 0);
		l3dss1_message(pc, MT_RELEASE_COMPLETE);
	}
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
send_timeout(l3_process_t *pc, char *nr)
{
	struct l3_msg	*l3m;
	unsigned char	c[5];

	l3m = alloc_l3_msg();
	if (!l3m) {
		eprint("%s no memory for l3 message\n", __FUNCTION__);
		return;
	}
	c[0] = 0x80 | CAUSE_LOC_USER;
	c[1] = 0x80 | CAUSE_TIMER_EXPIRED;
	c[2] = nr[0];
	c[3] = nr[1];
	c[4] = nr[2];
	add_layer3_ie(l3m, IE_CAUSE, 5, c);
	mISDN_l3up(pc, MT_TIMEOUT, l3m);
}

static void
l3dss1_t302(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	send_timeout(pc, "302");
}

static void
l3dss1_t303(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	unsigned char	c[2];

	L3DelTimer(&pc->timer1);
	if (test_bit(FLG_L3P_GOTRELCOMP, &pc->flags)) {
		StopAllL3Timer(pc);
		l3m = alloc_l3_msg();
		if (!l3m)
			return;
		newl3state(pc, 0);
		if (pc->rm_cause) {
			c[0] = 0x80 | CAUSE_LOC_PRVN_RMTUSER;
			c[1] = 0x80 | pc->rm_cause;
		} else {
			c[0] = 0x80 | CAUSE_LOC_PRVN_RMTUSER;
			c[1] = 0x80 | CAUSE_NORMALUNSPECIFIED;
		}
		add_layer3_ie(l3m, IE_CAUSE, 2, c);
		mISDN_l3up(pc, MT_RELEASE_COMPLETE , l3m);
		send_proc(pc, IMSG_END_PROC_M, NULL);
		return;
	}
	if (!test_and_set_bit(FLG_L3P_TIMER303_1, &pc->flags)) {
		if (pc->t303msg) {
			SendMsg(pc, pc->t303msg, -1);
			pc->t303msg = NULL;
		}
		L3AddTimer(&pc->timer1, T303, CC_T303);
		L3DelTimer(&pc->timer2);
		if (!test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
			L3AddTimer(&pc->timer2, T312, CC_T312);
			test_and_set_bit(FLG_L3P_TIMER312, &pc->flags);
		}
		return;
	}
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	l3m = alloc_l3_msg();
	if (!l3m)
		return;
	c[0] = 0x80 | CAUSE_LOC_PRVN_RMTUSER;
	c[1] = 0x80 | CAUSE_NOUSER_RESPONDING;
	add_layer3_ie(l3m, IE_CAUSE, 2, c);
	mISDN_l3up(pc, MT_RELEASE_COMPLETE , l3m); // indicate CC_REJECT
	if (test_bit(FLG_L3P_TIMER312, &pc->flags)) {
		newl3state(pc, 22);
	} else {
		StopAllL3Timer(pc);
		send_proc(pc, IMSG_END_PROC_M, NULL);
	}
}

static void
l3dss1_t305(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	newl3state(pc, 19);
		l3dss1_message(pc, MT_RELEASE);
	test_and_clear_bit(FLG_L3P_TIMER308_1, &pc->flags);
	L3AddTimer(&pc->timer1, T308, CC_T308_1);
}

static void
l3dss1_t308(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!test_and_set_bit(FLG_L3P_TIMER308_1, &pc->flags)) {
		newl3state(pc, 19);
		L3DelTimer(&pc->timer1);
		l3dss1_message(pc, MT_RELEASE);
		L3AddTimer(&pc->timer1, T308, CC_T308_1);
	} else {
		StopAllL3Timer(pc);
		newl3state(pc, 0);
		send_timeout(pc, "308");
		send_proc(pc, IMSG_END_PROC_M, NULL);
	}
}

#warning hier lesen
#if 0
static void
l3dss1_t312(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	test_and_clear_bit(FLG_L3P_TIMER312, &pc->flags);
	L3DelTimer(&pc->timer2);
	dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
	l3_debug(pc->L3, "%s: state %d", __FUNCTION__, pc->state);
// only action if proc in state 22 ETSI !!!
//	if (pc->state == 22 || pc->state == 25 || pc->state == 9 || pc->state == 7) {
	if (pc->state == 22) {
		StopAllL3Timer(pc);
//		if (list_empty(&pc->child)) {
//			if_link(pc->L3->nst->manager, (ifunc_t)pc->L3->nst->L3_manager,
//				CC_TIMEOUT | INDICATION,pc->ces |
//				(pc->callref << 16), sizeof(int), &t, 0);
			send_proc(pc, IMSG_END_PROC_M, NULL);
//		}
	}
}
// t312 feuert nur im state 22. wenn aber vorher z.b. ein alerting war,
// dann ist der state auf 7, auch wenn der child-process ausgelöst hat.
// (wenn ich also während der 6 sekunde (T312) das gespräch abweise,
//  dann bekomme ich erst nach ende von T312 ein disconnect vom stack.
//  es könnte ja noch ein gerät antworten und auch alerting oder connect
//  senden.)
//
// wir sollten eher prüfen, ob noch ein child da ist. der ist dann in einem
// der states "setup acknowledge", proceeding oder alerting.
// NUR DANN sollte der stack beendet werden und nach oben ein timeout geschickt,
// dann weiss man als applikation, dass der stack aufgrund von T312 beendet
// wurde, kein child mehr da ist und man dann den gesammelten cause an den
// anrufer (innerhalb der applikation) weiterreichen muss.
//
// im call abort state (22) ist ja auch kein child mehr da, da in diesem state
// jede antwort vom user sofort abgewiesen wird.
// hier mein code:
#endif
static void
l3dss1_t312(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	test_and_clear_bit(FLG_L3P_TIMER312, &pc->flags);
	L3DelTimer(&pc->timer2);
	dprint(DBGM_L3, pc->l2if->l2addr.dev, "%s: pc=%p del timer2\n", __FUNCTION__, pc);
	l3_debug(pc->L3, "%s: state %d", __FUNCTION__, pc->state);
	if (list_empty(&pc->child)) {
		StopAllL3Timer(pc);
		newl3state(pc, 0);
		send_timeout(pc, "312");
		send_proc(pc, IMSG_END_PROC_M, NULL);
	}
}

static void
l3dss1_holdack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->aux_state != AUX_HOLD_IND)
		return;
	pc->aux_state = AUX_CALL_HELD; 
	if (l3m) {
		SendMsg(pc, l3m, -1);
	} else {
		l3dss1_message(pc, MT_HOLD_ACKNOWLEDGE);
	}
}

static void
l3dss1_holdrej_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->aux_state != AUX_HOLD_IND)
		return;
	pc->aux_state = AUX_IDLE;
	if (!l3m->cause)
		l3dss1_message_cause(pc,
			MT_HOLD_REJECT, CAUSE_NORMALUNSPECIFIED);
	else
		SendMsg(pc, l3m, -1);
}

static void
l3dss1_retrack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->aux_state != AUX_RETRIEVE_IND)
		return;
	pc->aux_state = AUX_IDLE;
	if (l3m) {
		SendMsg(pc, l3m, -1);
	} else {
		l3dss1_message(pc, MT_RETRIEVE_ACKNOWLEDGE);
	}
}

static void
l3dss1_retrrej_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->aux_state != AUX_RETRIEVE_IND)
		return;
	pc->aux_state = AUX_CALL_HELD; 
	if (!l3m->cause)
		l3dss1_message_cause(pc,
			MT_RETRIEVE_REJECT, CAUSE_NORMALUNSPECIFIED);
	else
		SendMsg(pc, l3m, -1);
}

static void
l3dss1_suspack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (l3m) {
		SendMsg(pc, l3m, 0);
	} else {
		l3dss1_message(pc, MT_SUSPEND_ACKNOWLEDGE);
	}
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_susprej_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!l3m->cause)
		l3dss1_message_cause(pc,
			MT_SUSPEND_REJECT, CAUSE_NORMALUNSPECIFIED);
	else
		SendMsg(pc, l3m, -1);
	newl3state(pc, 10);
}

static void
l3dss1_resack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (l3m) {
		SendMsg(pc, l3m, 10);
	} else {
		l3dss1_message(pc, MT_RESUME_ACKNOWLEDGE);
		newl3state(pc, 10);
	}
}

static void
l3dss1_resrej_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!l3m->cause)
		l3dss1_message_cause(pc,
			MT_RESUME_REJECT, CAUSE_NORMALUNSPECIFIED);
	else
		SendMsg(pc, l3m, -1);
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_reset(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
	send_proc(pc, IMSG_END_PROC_M, NULL);
}

static void
l3dss1_global_restart(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	unsigned char	ri;
	l3_process_t	*up, *n;
	struct l3_msg	*nl3m;
	int		ret;

	L3DelTimer(&pc->timer1);
	if (l3m->restart_ind) {
		ri = l3m->restart_ind[1];
	} else {
		ri = 0x86;
	}
	if (!(ret = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if (0 == (pc->cid[1] & 3)) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
		} /* valid test for primary rate ??? */
	} else if (ret != -1) {
		l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
		free_l3_msg(l3m);
		return;
	}
	newl3state(pc, 2);
	list_for_each_entry_safe(up, n, &pc->L3->plist, list) {
		if ((ri & 6) == 6) 
			dss1man(up, MT_RESTART, NULL);
		else if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if ((up->cid[1] & 3) == (pc->cid[1] & 3))
				dss1man(up, MT_RESTART, NULL);
		} else {
			if ((up->cid[3] & 0x7f) == (pc->cid[3] & 0x7f))
				dss1man(up, MT_RESTART, NULL); 
		}
	}
	nl3m = MsgStart(pc, MT_RESTART_ACKNOWLEDGE);
	if (pc->cid[0])
		/*copy channel ie*/
		add_layer3_ie(nl3m, IE_CHANNEL_ID, pc->cid[0], pc->cid + 1);
	free_l3_msg(l3m);
	add_layer3_ie(nl3m, IE_RESTART_IND, 1, &ri);
	SendMsg(pc, nl3m, -1);
	newl3state(pc, 0);
}


static void
l3dss1_restart_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	eprint("Restart Acknowledge\n");
}

static void
l3dss1_dl_reset(l3_process_t *pc, unsigned int pr, struct l3_msg *arg)
{
	struct l3_msg	*l3m = MsgStart(pc, MT_DISCONNECT);
	unsigned char	c[2];

	if (!l3m)
		return;
	c[0] = 0x80 | CAUSE_LOC_USER;
	c[1] = 0x80 | CAUSE_TEMPORARY_FAILURE;
	add_layer3_ie(l3m, IE_CAUSE, 2, c);
	l3_msg_increment_refcnt(l3m);
	l3dss1_disconnect_req(pc, pr, l3m);
	mISDN_l3up(pc, MT_DISCONNECT, l3m);
}

static void
l3dss1_dl_release(l3_process_t *pc, unsigned int pr, struct l3_msg  *arg)
{
	newl3state(pc, 0);
	send_proc(pc, IMSG_END_PROC, NULL);
}

static void
l3dss1_dl_reestablish(l3_process_t *pc, unsigned int pr, struct l3_msg  *arg)
{
	if (!test_and_set_bit(FLG_L3P_TIMER309, &pc->flags)) {
		L3DelTimer(&pc->timer1);
		L3AddTimer(&pc->timer1, T309, CC_T309);
	}
	l3_manager(pc->l2if, DL_ESTABLISH_REQ);
}

static void
l3dss1_dl_reest_status(l3_process_t *pc, unsigned int pr, struct l3_msg  *arg)
{
	L3DelTimer(&pc->timer1);
	test_and_clear_bit(FLG_L3P_TIMER309, &pc->flags);
}

static void
l3dss1_dl_ignore(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
}

/* *INDENT-OFF* */
static struct stateentry downstatelist[] =
{
	{ALL_STATES,
		MT_RELEASE_COMPLETE, l3dss1_release_cmpl_req},
	{SBIT(0),
	 MT_SETUP, l3dss1_setup_req},
	{SBIT(1),
	 MT_SETUP_ACKNOWLEDGE, l3dss1_setup_ack_req},
	{SBIT(1) | SBIT(2),
	 MT_CALL_PROCEEDING, l3dss1_proceed_req},
	{SBIT(2) | SBIT(3),
	 MT_ALERTING, l3dss1_alert_req},
	{SBIT(2) | SBIT(3) | SBIT(4),
	 MT_CONNECT, l3dss1_connect_req},
	{SBIT(8),
	 MT_CONNECT_ACKNOWLEDGE, l3dss1_connect_ack_req},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10),
	 MT_DISCONNECT, l3dss1_disconnect_req},
	{ SBIT(2) | SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(25),
	 MT_DISCONNECT, l3dss1_disconnect_req_out},
	{ SBIT(2) | SBIT(7) | SBIT(9) | SBIT(11) | SBIT(12) | SBIT(25),
	 MT_RELEASE, l3dss1_release_req},
	{ALL_STATES,
	 MT_FACILITY, l3dss1_facility_req},
	{SBIT(4) | SBIT(7) | SBIT(8) | SBIT(10),
	 MT_USER_INFORMATION, l3dss1_userinfo_req},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(10) | SBIT(11) | SBIT(12) | SBIT(25),
	 MT_INFORMATION, l3dss1_information_req},
	{SBIT(2) | SBIT(3) | SBIT(4),
	 MT_PROGRESS, l3dss1_progress_req},
	{SBIT(10) | SBIT(15),
	 MT_NOTIFY, l3dss1_notify_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 MT_HOLD_ACKNOWLEDGE, l3dss1_holdack_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 MT_HOLD_REJECT, l3dss1_holdrej_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE_ACKNOWLEDGE, l3dss1_retrack_req},
	{SBIT(3) | SBIT(4) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE_REJECT, l3dss1_retrrej_req},
	{SBIT(15),
	 MT_SUSPEND_ACKNOWLEDGE, l3dss1_suspack_req},
	{SBIT(15),
	 MT_SUSPEND_REJECT, l3dss1_susprej_req},
	{SBIT(17),
	 MT_RESUME_ACKNOWLEDGE, l3dss1_resack_req},
	{SBIT(17),
	 MT_RESUME_REJECT, l3dss1_resrej_req},
};

#define DOWNSLLEN \
	(sizeof(downstatelist) / sizeof(struct stateentry))

static struct stateentry globalmes_list[] =
{
#ifdef TODO
	{ALL_STATES,
	 MT_STATUS, l3dss1_status},
#endif
	{SBIT(0),
	 MT_RESTART, l3dss1_global_restart},
	{SBIT(1),
	 MT_RESTART_ACKNOWLEDGE, l3dss1_restart_ack},
};
#define GLOBALM_LEN \
	(sizeof(globalmes_list) / sizeof(struct stateentry))

static struct stateentry manstatelist[] =
{
        {SBIT(2) | SBIT(25),
         DL_ESTABLISH_IND, l3dss1_dl_reset},
        {SBIT(10),
         DL_ESTABLISH_CNF, l3dss1_dl_reest_status},
        {SBIT(10),
         DL_RELEASE_IND, l3dss1_dl_reestablish},
        {ALL_STATES,
         DL_RELEASE_IND, l3dss1_dl_release},
        {ALL_STATES,
         DL_ESTABLISH_CNF, l3dss1_dl_ignore},
	{SBIT(2),
	 CC_T302, l3dss1_t302},
	{SBIT(12),
	 CC_T305, l3dss1_t305},
	{SBIT(6),
	 CC_T303, l3dss1_t303},
	{SBIT(19),
	 CC_T308_1, l3dss1_t308},
	{SBIT(10),
	 CC_T309, l3dss1_dl_release},
	{ALL_STATES, // muss sein, sonst gibt es probleme.
	 CC_T312, l3dss1_t312},
	{SBIT(6),
	 CC_TCTRL, l3dss1_reset},
};

#define MANSLLEN \
        (sizeof(manstatelist) / sizeof(struct stateentry))

static int
imsg_intrelease(l3_process_t *master, l3_process_t *child)
{
	unsigned char	cause[2];

	if ((!master) || (!child))
		return(-EINVAL);
	dprint(DBGM_L3, master->l2if->l2addr.dev, "%s: m/c(%x/%x) state(%d/%d) m->child:%d\n", __FUNCTION__,
		master->pid, child->pid, master->state, child->state,
		!list_empty(&master->child));
	switch (master->state) {
		case 0:
			if (list_empty(&master->child)) {
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
			if ((!list_empty(&master->child)) || test_bit(FLG_L3P_TIMER312, &master->flags)) {
				dprint(DBGM_L3, master->l2if->l2addr.dev, "%s: JOLLY child %d, flg=%d\n", __FUNCTION__,
					!list_empty(&master->child), test_bit(FLG_L3P_TIMER312, &master->flags));
			} else {
				send_proc(master, IMSG_END_PROC, NULL);
			}
			break;
		case 8:
			if (master->selpid == child->pid) {
				cause[0] = CAUSE_LOC_PRVN_LOCUSER | 0x80;
				cause[1] = CAUSE_NONSELECTED_USER | 0x80;
				send_proc(master, IMSG_RELEASE_CHILDS, cause);
				if (test_bit(FLG_L3P_TIMER312, &master->flags)) {
					newl3state(master, 22);
				} else {
					if (list_empty(&master->child))
						send_proc(master, IMSG_END_PROC, NULL);
				}
			}
			break;
		case 22:
			if (list_empty(&master->child)) {
				send_proc(master, IMSG_END_PROC, NULL);
			}
			break;
	}
	return 0;
}

static int
send_proc(l3_process_t *proc, int op, void *arg)
{
	int		i;
	struct l3_msg	*l3m = arg;
	struct l3_msg	*nl3m;
	l3_process_t	*p, *np;

	if (proc->L3 && proc->L3->debug & L3_DEB_PROC)
		l3_debug(proc->L3, "%s: proc(%x) op(%d)", __FUNCTION__,
			proc->pid, op);  
	switch(op) {
		case IMSG_END_PROC:
		case IMSG_END_PROC_M:
			StopAllL3Timer(proc);
			list_del(&proc->list);
			if (!proc->master && !arg) {
				proc->L3->ml3.from_layer3(&proc->L3->ml3, MT_FREE, proc->pid, NULL);
			}
			if (!list_empty(&proc->child)) {
				list_for_each_entry_safe(p, np, &proc->child, list)
					send_proc(p, IMSG_END_PROC, NULL);
			}
			if (proc->master) {
				if (op == IMSG_END_PROC_M)
					imsg_intrelease(proc->master, proc);
			}
			free(proc);
			break;
		case IMSG_L2_DATA:
			for (i = 0; i < DATASLLEN; i++)
				if ((l3m->type == datastatelist[i].primitive) &&
					((1 << proc->state) & datastatelist[i].state))
					break;
			if (i == DATASLLEN) {
				if (proc->L3->debug & L3_DEB_STATE) {
					l3_debug(proc->L3, "dss1 state %d mt %#x unhandled",
						proc->state, l3m->type);
				}
				if ((MT_RELEASE_COMPLETE != l3m->type) && (MT_RELEASE != l3m->type)) {
		//			l3dss1_status_send(proc, CAUSE_NOTCOMPAT_STATE);
				}
				free_l3_msg(l3m);
			} else {
				if (proc->L3->debug & L3_DEB_STATE) {
					l3_debug(proc->L3, "dss1 state %d mt %x",
						proc->state, l3m->type);
				}
				datastatelist[i].rout(proc, l3m->type, l3m);
			}
			break;
		case IMSG_MASTER_L2_DATA:
			for (i = 0; i < MDATASLLEN; i++)
				if ((l3m->type == mdatastatelist[i].primitive) &&
					((1 << proc->state) & mdatastatelist[i].state))
					break;
			if (i == MDATASLLEN) {
				if (proc->L3->debug & L3_DEB_STATE) {
					l3_debug(proc->L3, "dss1 state %d mt %#x unhandled",
						proc->state, l3m->type);
				}
				if ((MT_RELEASE_COMPLETE != l3m->type) && (MT_RELEASE != l3m->type)) {
		//			l3dss1_status_send(proc, CAUSE_NOTCOMPAT_STATE);
				}
				free_l3_msg(l3m);
			} else {
				if (proc->L3->debug & L3_DEB_STATE) {
					l3_debug(proc->L3, "dss1 state %d mt %x",
						proc->state, l3m->type);
				}
				mdatastatelist[i].rout(proc, l3m->type, l3m);
			}
			break;
		case IMSG_L4_DATA:
			for (i = 0; i < DOWNSLLEN; i++)
				if ((l3m->type == downstatelist[i].primitive) &&
					((1 << proc->state) & downstatelist[i].state))
					break;
			if (i == DOWNSLLEN) {
				if (proc->L3->debug & L3_DEB_STATE) {
					l3_debug(proc->L3, "dss1 state %d L4 %#x unhandled",
						proc->state, l3m->type);
				}
				free_l3_msg(l3m);
			} else {
				if (proc->L3->debug & L3_DEB_STATE) {
					l3_debug(proc->L3, "dss1 state %d L4 %x",
						proc->state, l3m->type);
				}
				downstatelist[i].rout(proc, l3m->type, l3m);
			}
			break;
		case IMSG_CONNECT_IND:
			p = proc;
			proc = proc->master;
			if (!proc)
				return -EINVAL;
			proc->selpid = p->pid;
			newl3state(proc, 8);
			mISDN_l3up(proc, l3m->type, l3m);
			return 0;
		case IMSG_SEL_PROC:
			p = get_l3process4pid(proc->L3, proc->selpid);
			if (!p) {
				eprint("%s: did not find selected process %x\n", __FUNCTION__, proc->selpid);
				break;
			}
			proc->L3->ml3.from_layer3(&proc->L3->ml3, MT_ASSIGN, proc->selpid, NULL);
			proc->pid = proc->selpid;
			proc->l2if = p->l2if;
			send_proc(p, IMSG_END_PROC, NULL);
			break;
		case IMSG_RELEASE_CHILDS:
			list_for_each_entry_safe(p, np, &proc->child, list) {
				nl3m = alloc_l3_msg();
				if (!nl3m) {
					eprint("%s: no memory\n", __FUNCTION__);
					return -ENOMEM;
				}
				add_layer3_ie(nl3m, IE_CAUSE, 2, arg);
				nl3m->type = MT_RELEASE;
				nl3m->pid = p->pid;
				send_proc(p, IMSG_L4_DATA, nl3m);
			}
			break;
	}
	return 0;
}

static void
global_handler(layer3_t *l3, u_int mt, struct mbuffer *mb)
{
	u_int		i;
	l3_process_t	*proc = &l3->global;

	proc->pid = mb->l3h.cr; /* cr flag */
	for (i = 0; i < GLOBALM_LEN; i++)
		if ((mt == globalmes_list[i].primitive) &&
		    ((1 << proc->state) & globalmes_list[i].state))
			break;
	if (i == GLOBALM_LEN) {
		l3dss1_status_send(proc, CAUSE_INVALID_CALLREF);
		free_mbuffer(mb);
	} else {
		globalmes_list[i].rout(proc, mt, &mb->l3);
	}
}

static int
dl_data_mux(layer3_t *l3, struct mbuffer *msg)
{
	int		ret;
	l3_process_t	*proc;

	if (msg->len < 3) {
		fprintf(stderr, "dss1up frame too short(%d)\n", msg->len);
		goto freemsg;
	}
	if (msg->data[0] != Q931_PD)
		goto freemsg;
	ret = parseQ931(msg);
	if (ret & Q931_ERROR_FATAL) {
		fprintf(stderr, "dss1up: parse IE error %x\n", ret); 
		goto freemsg;
	}
	dprint(DBGM_L3, msg->addr.dev, "%s: mt(%x) pid(%x) crlen(%d)\n", __FUNCTION__, msg->l3.type, msg->l3.pid, msg->l3h.crlen);
	if (msg->l3h.crlen == 0) {	/* Dummy Callref */
		if (msg->l3h.type == MT_FACILITY) {
			l3dss1_facility(&l3->dummy, msg->h->prim, &msg->l3);
			return 0;
		}
		goto freemsg;
	} else if ((msg->l3h.cr & 0x7fff) == 0) {	/* Global CallRef */
		global_handler(l3, msg->l3h.type, msg);
		return 0;
	}
	proc = get_l3process4pid(l3, msg->l3.pid);
	dprint(DBGM_L3, msg->addr.dev, "%s: proc(%x)\n", __FUNCTION__, proc ? proc->pid : 0);
	if (!proc) {
		if (msg->l3.type == MT_SETUP || msg->l3.type == MT_RESUME) {
			/* Setup/Resume creates a new transaction process */
			
			if (msg->l3.pid & 0x8000) {
				/* Setup/Resume with wrong CREF flag */
				if (l3->debug & L3_DEB_STATE)
					l3_debug(l3, "dss1 wrong CRef flag");
				goto freemsg;
			}
			dprint(DBGM_L3, msg->addr.dev, "%s: %s\n", __FUNCTION__, (msg->l3.type == MT_SETUP) ? "MT_SETUP" : "MT_RESUME");
			if (!(proc = create_new_process(l3, msg->addr.channel,msg->l3h.cr, NULL))) {
				/* May be to answer with RELEASE_COMPLETE and
				 * CAUSE 0x2f "Resource unavailable", but this
				 * need a new_l3_process too ... arghh
				 */
				goto freemsg;
			}
			dprint(DBGM_L3, msg->addr.dev, "%s: proc(%x)\n", __FUNCTION__, proc->pid);
		} else {
			dprint(DBGM_L3, msg->addr.dev, "%s: mt(%x) do not create proc\n", __FUNCTION__,
				msg->l3.type);
			// TODO: it happens that a response to an outgoing setup is 
			// received after connect of another terminal. in this case we must release.
			// Hmm should not happen, if it is a collision and we already sent a RELEASE
			goto freemsg;
		}
	}
	if ((proc->pid & MISDN_PID_CRTYPE_MASK) == MISDN_PID_MASTER) {
		dprint(DBGM_L3, msg->addr.dev, "%s: master state %d found\n", __FUNCTION__,
			proc->state);
		send_proc(proc, IMSG_MASTER_L2_DATA, &msg->l3);
	} else
		send_proc(proc, IMSG_L2_DATA, &msg->l3);
	return 0;
freemsg:
	free_mbuffer(msg);
	return 0;
}

static int
dss1_fromup(layer3_t *l3, struct l3_msg *l3m)
{
	l3_process_t	*proc;

	if (l3m->pid == MISDN_PID_DUMMY) {
		if (l3m->type == MT_FACILITY) {
			l3dss1_facility_req(&l3->dummy, l3m->type, l3m);
			return 0;
		}
		return -EINVAL;
	}

	if (l3m->pid == MISDN_PID_GLOBAL) {
#ifdef NOTYET
		if (l3m->type == MT_RESTART) {
			l3dss1_restart_req(&l3->global, l3m->type, l3m);
			return 0;
		}
#endif
		return -EINVAL;
	}

	proc = get_l3process4pid(l3, l3m->pid);
	if (!proc) {
		eprint("mISDN dss1 fromup without proc pr=%04x dinfo(%x)\n", l3m->type, l3m->pid);
		return -EINVAL;
	}
	send_proc(proc, IMSG_L4_DATA, l3m);
	return 0;
}

static int
dss1man(l3_process_t *proc, u_int pr, struct l3_msg *l3m)
{
	u_int	i;

	if (!proc) {
		eprint("mISDN dss1man without proc pr=%04x\n", pr);
		return -EINVAL;
	}
	for (i = 0; i < MANSLLEN; i++)
		if ((pr == manstatelist[i].primitive) && ((1 << proc->state) & manstatelist[i].state))
			break;
	if (i == MANSLLEN) {
		eprint("cr %x dss1man state %d prim %#x unhandled\n",
			proc->pid & 0x7fff, proc->state, pr);
		if (l3m)
			free_l3_msg(l3m);
	} else {
		manstatelist[i].rout(proc, pr, l3m);
	}
	return 0;
}

static void
dss1_net_init(layer3_t *l3)
{
	l3->from_l2 = dl_data_mux;
	l3->to_l3 = dss1_fromup;
	l3->p_mgr = dss1man;
	test_and_set_bit(FLG_NETWORK, &l3->ml3.options);
}

struct l3protocol	dss1net = {
	.name = "DSS1 Netside",
	.protocol = L3_PROTOCOL_DSS1_NET ,
	.init = dss1_net_init
};

