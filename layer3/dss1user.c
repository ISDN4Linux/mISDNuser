/* dss1user.c
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

static int dss1man(l3_process_t *, u_int, struct l3_msg *);

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
	release_l3_process(pc);
}

static int ie_ALERTING[] = {IE_BEARER, IE_CHANNEL_ID | IE_MANDATORY_1,
		IE_FACILITY, IE_PROGRESS, IE_DISPLAY, IE_SIGNAL, IE_REDIR_DN,
		IE_HLC, IE_USER_USER, -1};
static int ie_CALL_PROCEEDING[] = {IE_BEARER, IE_CHANNEL_ID | IE_MANDATORY_1,
		IE_FACILITY, IE_PROGRESS, IE_DISPLAY, IE_REDIR_DN, IE_HLC, -1};
static int ie_CONNECT[] = {IE_BEARER, IE_CHANNEL_ID | IE_MANDATORY_1,
		IE_FACILITY, IE_PROGRESS, IE_DISPLAY, IE_DATE, IE_SIGNAL,
		IE_CONNECT_PN, IE_CONNECT_SUB, IE_LLC, IE_HLC, IE_USER_USER, -1};
static int ie_CONNECT_ACKNOWLEDGE[] = {IE_CHANNEL_ID, IE_DISPLAY, IE_SIGNAL, -1};
static int ie_DISCONNECT[] = {IE_CAUSE | IE_MANDATORY, IE_FACILITY,
		IE_PROGRESS, IE_DISPLAY, IE_SIGNAL, IE_USER_USER, -1};
static int ie_INFORMATION[] = {IE_COMPLETE, IE_DISPLAY, IE_KEYPAD, IE_SIGNAL,
		IE_CALLED_PN, -1};
static int ie_NOTIFY[] = {IE_BEARER, IE_NOTIFY | IE_MANDATORY, IE_DISPLAY, -1};
static int ie_PROGRESS[] = {IE_BEARER, IE_CAUSE, IE_FACILITY, IE_PROGRESS |
		IE_MANDATORY, IE_DISPLAY, IE_HLC, IE_USER_USER, -1};
static int ie_RELEASE[] = {IE_CAUSE | IE_MANDATORY_1, IE_FACILITY, IE_DISPLAY,
		IE_SIGNAL, IE_USER_USER, -1};
/* a RELEASE_COMPLETE with errors don't require special actions
static int ie_RELEASE_COMPLETE[] = {IE_CAUSE | IE_MANDATORY_1, IE_FACILITY,
		IE_DISPLAY, IE_SIGNAL, IE_USER_USER, -1};
*/
static int ie_RESUME_ACKNOWLEDGE[] = {IE_CHANNEL_ID| IE_MANDATORY, IE_FACILITY,
		IE_DISPLAY, -1};
static int ie_RESUME_REJECT[] = {IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
static int ie_SETUP[] = {IE_COMPLETE, IE_BEARER  | IE_MANDATORY,
		IE_CHANNEL_ID| IE_MANDATORY, IE_FACILITY, IE_PROGRESS,
		IE_NET_FAC, IE_DISPLAY, IE_KEYPAD, IE_SIGNAL, IE_CALLING_PN,
		IE_CALLING_SUB, IE_CALLED_PN, IE_CALLED_SUB, IE_REDIR_NR,
		IE_LLC, IE_HLC, IE_USER_USER, -1};
static int ie_SETUP_ACKNOWLEDGE[] = {IE_CHANNEL_ID | IE_MANDATORY, IE_FACILITY,
		IE_PROGRESS, IE_DISPLAY, IE_SIGNAL, -1};
static int ie_STATUS[] = {IE_CAUSE | IE_MANDATORY, IE_CALL_STATE |
		IE_MANDATORY, IE_DISPLAY, -1};
static int ie_STATUS_ENQUIRY[] = {IE_DISPLAY, -1};
static int ie_SUSPEND_ACKNOWLEDGE[] = {IE_FACILITY, IE_DISPLAY, -1};
static int ie_SUSPEND_REJECT[] = {IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
static int ie_HOLD[] = {IE_DISPLAY, -1};
static int ie_HOLD_ACKNOWLEDGE[] = {IE_DISPLAY, -1};
static int ie_HOLD_REJECT[] = {IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
static int ie_RETRIEVE[] = {IE_CHANNEL_ID| IE_MANDATORY, IE_DISPLAY, -1};
static int ie_RETRIEVE_ACKNOWLEDGE[] = {IE_CHANNEL_ID| IE_MANDATORY, IE_DISPLAY, -1};
static int ie_RETRIEVE_REJECT[] = {IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
/* not used
 * static int ie_CONGESTION_CONTROL[] = {IE_CONGESTION | IE_MANDATORY,
 *		IE_CAUSE | IE_MANDATORY, IE_DISPLAY, -1};
 * static int ie_USER_INFORMATION[] = {IE_MORE_DATA, IE_USER_USER | IE_MANDATORY, -1};
 * static int ie_RESTART[] = {IE_CHANNEL_ID, IE_DISPLAY, IE_RESTART_IND |
 *		IE_MANDATORY, -1};
 */
static int ie_FACILITY[] = {IE_FACILITY | IE_MANDATORY, IE_DISPLAY, -1};

static int l3_valid_states[] = {0,1,2,3,4,6,7,8,9,10,11,12,15,17,19,25,-1};

struct ie_len {
	int ie;
	int len;
};

static
struct ie_len max_ie_len[] = {
// not implemented	{IE_SEGMENT, 4},
	{IE_BEARER, 12},
	{IE_CAUSE, 32},
	{IE_CALL_ID, 10},
	{IE_CALL_STATE, 3},
	{IE_CHANNEL_ID,	34},
	{IE_FACILITY, 255},
	{IE_PROGRESS, 4},
	{IE_NET_FAC, 255},
	{IE_NOTIFY, 255}, /* 3-* Q.932 Section 9 */
	{IE_DISPLAY, 82},
	{IE_DATE, 8},
	{IE_KEYPAD, 34},
	{IE_SIGNAL, 3},
	{IE_INFORATE, 6},
	{IE_E2E_TDELAY, 11},
	{IE_TDELAY_SEL, 5},
	{IE_PACK_BINPARA, 3},
	{IE_PACK_WINSIZE, 4},
	{IE_PACK_SIZE, 4},
	{IE_CUG, 7},
	{IE_REV_CHARGE, 3},
	{IE_CONNECT_PN, 24},
	{IE_CONNECT_SUB, 23},
	{IE_CALLING_PN, 24},
	{IE_CALLING_SUB, 23},
	{IE_CALLED_PN, 24},
	{IE_CALLED_SUB, 23},
	{IE_REDIR_NR, 255},
	{IE_REDIR_DN, 255},
	{IE_TRANS_SEL, 255},
	{IE_RESTART_IND, 3},
	{IE_LLC, 18},
	{IE_HLC, 5},
	{IE_USER_USER, 131},
	{-1,0},
};

static int
ie_in_set(l3_process_t *pc, u_char ie, int *checklist) {
	int ret = 1;

	while (*checklist != -1) {
		if ((*checklist & 0xff) == ie) {
			if (ie & 0x80)
				return(-ret);
			else
				return(ret);
		}
		ret++;
		checklist++;
	}
	return 0;
}

static int
check_infoelements(l3_process_t *pc, struct l3_msg *l3m, int *checklist, int mt)
{
	unsigned char	**v_ie, ie;
	int		i, l, pos;
	int		err_len = 0, err_compr = 0, err_ureg = 0;

	v_ie = &l3m->bearer_capability;

	for (i = 0; i < IE_COUNT; i++) {
		if (v_ie[i]) {
			ie = l3_pos2ie(i);
			if (!(pos = ie_in_set(pc, ie, checklist))) {
				eprint("Received IE %x not allowed (mt=%x)\n", mt);
				err_ureg++;
			}
			l = *v_ie[i];
			if (l > max_ie_len[i].len)
				err_len++;
		}
	}

	if (l3m->comprehension_req) {
		err_compr++;
	}
	
	if (err_compr)
		return Q931_ERROR_COMPREH;
	if (err_ureg)
		return Q931_ERROR_UNKNOWN;
	if (err_len)
		return Q931_ERROR_IELEN;
	return 0;
}

/* verify if a message type exists and contain no IE error */
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
			break;
		case MT_RESUME: /* RESUME only in user->net */
		case MT_SUSPEND: /* SUSPEND only in user->net */
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
l3dss1_release_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (l3m) {
		SendMsg(pc, l3m, 19);
	} else {
		newl3state(pc, 19);
		l3dss1_message(pc, MT_RELEASE);
	}
	L3AddTimer(&pc->timer1, T308, CC_T308_1);
}

static void
l3dss1_setup_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	l3_msg_increment_refcnt(l3m);
	pc->t303msg = l3m;
	SendMsg(pc, l3m, 1);
	pc->n303 = N303;
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T303, CC_T303);
}

static void
l3dss1_disconnect_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	unsigned char	c[2];

	if (pc->t303msg) {
		free_l3_msg(pc->t303msg);
		pc->t303msg = NULL;
	}
	StopAllL3Timer(pc);
	if (l3m) {
		if (!l3m->cause) {
			c[0] = 0x80 | CAUSE_LOC_USER;
			c[1] = 0x80 | CAUSE_NORMALUNSPECIFIED;
			add_layer3_ie(l3m, IE_CAUSE, 2, c);
		} else
			pc->cause = *(l3m->cause + 3) & 0x7f;
		SendMsg(pc, l3m, 11);
	} else {
		newl3state(pc, 11);
		l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_NORMALUNSPECIFIED);
		pc->cause=CAUSE_NORMALUNSPECIFIED;
	}
	L3AddTimer(&pc->timer1, T305, CC_T305);
}

static void
l3dss1_connect_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!pc->cid[0]) { /* no channel was selected */
		l3dss1_disconnect_req(pc, pr, NULL);
		if (l3m)
			free_l3_msg(l3m);
		return;
	}
	if (l3m) {
		SendMsg(pc, l3m, 8);
	} else {
		newl3state(pc, 8);
		l3dss1_message(pc, MT_CONNECT);
	}
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T313, CC_T313);
}

static void
l3dss1_release_cmpl_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	if (l3m) {
		SendMsg(pc, l3m, 0);
	} else {
		newl3state(pc, 0);
		l3dss1_message(pc, MT_RELEASE_COMPLETE);
	}
	release_l3_process(pc);
}

static void
l3dss1_alert_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, 7);
	} else {
		newl3state(pc, 7);
		l3dss1_message(pc, MT_ALERTING);
	}
	L3DelTimer(&pc->timer1);
}

static void
l3dss1_proceed_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, 9);
	} else {
		newl3state(pc, 9);
		l3dss1_message(pc, MT_CALL_PROCEEDING);
	}
	L3DelTimer(&pc->timer1);
}

static void
l3dss1_setup_ack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, 25);
	} else {
		newl3state(pc, 25);
		l3dss1_message(pc, MT_SETUP_ACKNOWLEDGE);
	}
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T302, CC_T302);
}

static void
l3dss1_suspend_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, 15);
	} else {
		newl3state(pc, 15);
		l3dss1_message(pc, MT_SUSPEND);
	}
	L3AddTimer(&pc->timer1, T319, CC_T319);
}

static void
l3dss1_resume_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, 17);
	} else {
		newl3state(pc, 17);
		l3dss1_message(pc, MT_RESUME);
	}
	L3AddTimer(&pc->timer1, T318, CC_T318);
}

static void
l3dss1_status_enq_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
	l3dss1_message(pc, MT_STATUS_ENQUIRY);
}

static void
l3dss1_information_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (pc->state == 2) {
		L3DelTimer(&pc->timer1);
		L3AddTimer(&pc->timer1, T304, CC_T304);
	}

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
l3dss1_progress_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_facility_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_restart_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m) {
		SendMsg(pc, l3m, -1);
	}
}

static void
l3dss1_release_cmpl(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	StopAllL3Timer(pc);
	newl3state(pc, 0);
	mISDN_l3up(pc, MT_RELEASE_COMPLETE, l3m);
	release_l3_process(pc);
}

static void
l3dss1_alerting(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (!(ret = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if ((0 == (pc->cid[1] & 3)) || (3 == (pc->cid[1] & 3))) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
		} /* valid test for primary rate ??? */
	} else if (1 == pc->state) {
		unsigned char	cause;
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_ALERTING, MT_ALERTING);
	if (Q931_ERROR_COMPREH  == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);	/* T304 */
	if (pc->t303msg) {
		free_l3_msg(pc->t303msg);
		pc->t303msg = NULL;
	}
	newl3state(pc, 4);
	if (ret)
		l3dss1_std_ie_err(pc, ret);
	mISDN_l3up(pc, MT_ALERTING, l3m);
}

static void
l3dss1_call_proc(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (!(ret = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if ((0 == (pc->cid[1] & 3)) || (3 == (pc->cid[1] & 3))) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
		} /* valid test for primary rate ??? */
	} else if (1 == pc->state) {
		unsigned char	cause;
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	/* Now we are on none mandatory IEs */
	ret = check_infoelements(pc, l3m, ie_CALL_PROCEEDING, MT_CALL_PROCEEDING);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);
	if (pc->t303msg) {
		free_l3_msg(pc->t303msg);
		pc->t303msg = NULL;
	}
	newl3state(pc, 3);
	L3AddTimer(&pc->timer1, T310, CC_T310);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
	mISDN_l3up(pc, MT_CALL_PROCEEDING, l3m);
}

static void
l3dss1_connect(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (!(ret = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if ((0 == (pc->cid[1] & 3)) || (3 == (pc->cid[1] & 3))) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
		} /* valid test for primary rate ??? */
	} else if (1 == pc->state) {
		unsigned char	cause;
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_CONNECT, MT_CONNECT);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);	/* T303 or T310 */
	if (pc->t303msg) {
		free_l3_msg(pc->t303msg);
		pc->t303msg = NULL;
	}
	l3dss1_message(pc, MT_CONNECT_ACKNOWLEDGE);
	newl3state(pc, 10);
	if (ret)
		l3dss1_std_ie_err(pc, ret);
	mISDN_l3up(pc, MT_CONNECT, l3m);
}

static void
l3dss1_connect_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;

	ret = check_infoelements(pc, l3m, ie_CONNECT_ACKNOWLEDGE, MT_CONNECT_ACKNOWLEDGE);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	newl3state(pc, 10);
	L3DelTimer(&pc->timer1);
	if (ret)
		l3dss1_std_ie_err(pc, ret);
	mISDN_l3up(pc, MT_CONNECT_ACKNOWLEDGE, l3m);
}

static void
l3dss1_disconnect(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char	cause = 0;

	StopAllL3Timer(pc);
	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
	}
	else if (pc->state == 7) /* Call Received*/
		cause = pc->rm_cause;
	ret = check_infoelements(pc, l3m, ie_DISCONNECT, MT_DISCONNECT);
	if (Q931_ERROR_COMPREH == ret)
		cause = CAUSE_MANDATORY_IE_MISS;
	else if ((!cause) && (Q931_ERROR_UNKNOWN == ret))
		cause = CAUSE_IE_NOTIMPLEMENTED;
	ret = pc->state;
	if (cause)
		newl3state(pc, 19);
	else
		newl3state(pc, 12);
       	if (11 != ret) {
		mISDN_l3up(pc, MT_DISCONNECT, l3m);
	} else if (!cause) {
		l3dss1_release_req(pc, pr, NULL);
		free_l3_msg(l3m);
	} else
		free_l3_msg(l3m);
	if (cause) {
		l3dss1_message_cause(pc, MT_RELEASE, cause);
		L3AddTimer(&pc->timer1, T308, CC_T308_1);
	}
}

static void
l3dss1_setup_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (!(ret = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if ((0 == (pc->cid[1] & 3)) || (3 == (pc->cid[1] & 3))) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
		} /* valid test for primary rate ??? */
	} else if (1 == pc->state) {
		unsigned char	cause;
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	/* Now we are on none mandatory IEs */
	ret = check_infoelements(pc, l3m, ie_SETUP_ACKNOWLEDGE, MT_SETUP_ACKNOWLEDGE);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);
	if (pc->t303msg) {
		free_l3_msg(pc->t303msg);
		pc->t303msg = NULL;
	}
	newl3state(pc, 2);
	L3AddTimer(&pc->timer1, T304, CC_T304);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
	mISDN_l3up(pc, MT_SETUP_ACKNOWLEDGE, l3m);
}

static void
l3dss1_setup(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	err = 0;

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
	/*
	 * Channel Identification
	 */

	if (!(err = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if (!test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
				if (3 == (pc->cid[1] & 3)) {
					l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
					free_l3_msg(l3m);
					return;
				}
			}
		} /* valid test for primary rate ??? */
	} else {
		unsigned char	cause;
		if (err == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	/* Now we are on none mandatory IEs */
	err = check_infoelements(pc, l3m, ie_SETUP, MT_SETUP);
	if (Q931_ERROR_COMPREH == err) {
		l3dss1_msg_without_setup(pc, CAUSE_MANDATORY_IE_MISS);
		free_l3_msg(l3m);
		return;
	}
	newl3state(pc, 6);
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T_CTRL, CC_TCTRL);
	if (err) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, err);
	mISDN_l3up(pc, MT_SETUP, l3m);
}

static void
l3dss1_reset(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
	release_l3_process(pc);
}

static void
l3dss1_release(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret, cause=0;

	StopAllL3Timer(pc);
	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if ((ret == -1) && (pc->state != 11))
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (ret != -1)
			cause = CAUSE_INVALID_CONTENTS;
	}
	ret = check_infoelements(pc, l3m, ie_RELEASE, MT_RELEASE);
	if (Q931_ERROR_COMPREH == ret)
		cause = CAUSE_MANDATORY_IE_MISS;
	else if ((Q931_ERROR_UNKNOWN == ret) && (!cause))
		cause = CAUSE_IE_NOTIMPLEMENTED;
	if (cause)
		l3dss1_message_cause(pc, MT_RELEASE_COMPLETE, cause);
	else
		l3dss1_message(pc, MT_RELEASE_COMPLETE);
	mISDN_l3up(pc, MT_RELEASE, l3m);
	newl3state(pc, 0);
	release_l3_process(pc);
}

static void
l3dss1_progress(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	int		err = 0;
	unsigned char	cause = CAUSE_INVALID_CONTENTS;

	if (l3m->progress) {
		if (l3m->progress[0] != 2) {
			err = 1;
		} else if (!(l3m->progress[1] & 0x70)) {
			switch (l3m->progress[1]) {
			case 0x80:
			case 0x81:
			case 0x82:
			case 0x84:
			case 0x85:
			case 0x87:
			case 0x8a:
				switch (l3m->progress[2]) {
				case 0x81:
				case 0x82:
				case 0x83:
				case 0x84:
				case 0x88:
					break;
				default:
					err = 2;
					break;
				}
				break;
			default:
				err = 3;
				break;
			}
		}
	} else {
		cause = CAUSE_MANDATORY_IE_MISS;
		err = 4;
	}
	if (err) {
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	/* Now we are on none mandatory IEs */
	err = check_infoelements(pc, l3m, ie_PROGRESS, MT_PROGRESS);
	if (err)
		l3dss1_std_ie_err(pc, err);
	/* 
	 * clear T310 if running (should be cleared by a Progress 
	 * Message, according to ETSI). 
	 * 
	 */
	L3DelTimer(&pc->timer1);
	if (pc->t303msg) {
		free_l3_msg(pc->t303msg);
		pc->t303msg = NULL;
	}
	if (Q931_ERROR_COMPREH != err) {
		mISDN_l3up(pc, MT_PROGRESS, l3m);
	} else
		free_l3_msg(l3m);
}

static void
l3dss1_notify(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	int		err = 0;
	unsigned char	cause = CAUSE_INVALID_CONTENTS;

	if (l3m->notify) {
		if (l3m->notify[0] != 1) {
			err = 1;
#if 0 // Why
		} else {
			switch (l3m->notify[1]) {
				case 0x80:
				case 0x81:
				case 0x82:
					break;
				default:
					err = 2;
					break;
			}
#endif
		}
	} else {
		cause = CAUSE_MANDATORY_IE_MISS;
		err = 3;
	}
	if (err) {
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	/* Now we are on none mandatory IEs */
	err = check_infoelements(pc, l3m, ie_NOTIFY, MT_NOTIFY);
	if (err)
		l3dss1_std_ie_err(pc, err);
	if (Q931_ERROR_COMPREH != err) {
		mISDN_l3up(pc, MT_NOTIFY, l3m);
	} else
		free_l3_msg(l3m);
}

static void
l3dss1_status_enq(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	int		ret;

	ret = check_infoelements(pc, l3m, ie_STATUS_ENQUIRY, MT_STATUS_ENQUIRY);
	l3dss1_std_ie_err(pc, ret);
	l3dss1_status_send(pc, CAUSE_STATUS_RESPONSE);
	mISDN_l3up(pc, MT_STATUS_ENQUIRY, l3m);
}

static void
l3dss1_information(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	int		ret;

	ret = check_infoelements(pc, l3m, ie_INFORMATION, MT_INFORMATION);
	if (ret)
		l3dss1_std_ie_err(pc, ret);
	if (pc->state == 25) { /* overlap receiving */
		L3DelTimer(&pc->timer1);
		L3AddTimer(&pc->timer1, T302, CC_T302);
	}
	mISDN_l3up(pc, MT_INFORMATION, l3m);
}

static void
l3dss1_release_ind(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	callState = -1;

	if (pc->state == 19) {
		/* ETS 300-102 5.3.5 */
		newl3state(pc, 0);
		mISDN_l3up(pc, MT_RELEASE, l3m);
		release_l3_process(pc);
	} else {
		if (l3m->call_state) {
			if (1 == l3m->call_state[0])
				callState = l3m->call_state[1];
		}
		if (callState == 0) {
			/* ETS 300-104 7.6.1, 8.6.1, 10.6.1... and 16.1
			 * set down layer 3 without sending any message
			 */
			newl3state(pc, 0);
			mISDN_l3up(pc, MT_RELEASE, l3m);
			release_l3_process(pc);
		} else {
			mISDN_l3up(pc, MT_RELEASE, l3m);
		}
	}
}

static void
l3dss1_restart(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	L3DelTimer(&pc->timer1);
	release_l3_process(pc);
	if (l3m)
		free_l3_msg(l3m);
}

static void
l3dss1_status(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	int		ret = 0;
	unsigned char	cause = 0, callState = 0xff;

	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
	}
	if (l3m->call_state) {
		if (1 == l3m->call_state[0]) {
			callState = l3m->call_state[1];
			if (!ie_in_set(pc, callState, l3_valid_states))
				cause = CAUSE_INVALID_CONTENTS;
		} else
			cause = CAUSE_INVALID_CONTENTS;
	} else
		cause = CAUSE_MANDATORY_IE_MISS;
	if (!cause) { /*  no error before */
		ret = check_infoelements(pc, l3m, ie_STATUS, MT_STATUS);
		if (Q931_ERROR_COMPREH == ret)
			cause = CAUSE_MANDATORY_IE_MISS;
		else if (Q931_ERROR_UNKNOWN == ret)
			cause = CAUSE_IE_NOTIMPLEMENTED;
	}
	if (cause) {
		l3dss1_status_send(pc, cause);
		if (cause != CAUSE_IE_NOTIMPLEMENTED) {
			free_l3_msg(l3m);
			return;
		}
	}
	if (l3m->cause)
		cause = pc->rm_cause & 0x7f;
	if ((cause == CAUSE_PROTOCOL_ERROR) && (callState == 0)) {
		/* ETS 300-104 7.6.1, 8.6.1, 10.6.1...
		 * if received MT_STATUS with cause == 111 and call
		 * state == 0, then we must set down layer 3
		 */
		newl3state(pc, 0);
		mISDN_l3up(pc, MT_STATUS, l3m);
		release_l3_process(pc);
	} else
		mISDN_l3up(pc, MT_STATUS, l3m);
}

static void
l3dss1_facility(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;

	ret = check_infoelements(pc, l3m, ie_FACILITY, MT_FACILITY);
	l3dss1_std_ie_err(pc, ret);
	if (!l3m->facility) {
		free_l3_msg(l3m);
		return;
	}
	mISDN_l3up(pc, MT_FACILITY, l3m);
}

static void
l3dss1_suspend_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m) {
	int	ret;

	L3DelTimer(&pc->timer1);
	newl3state(pc, 0);
	/* We don't handle suspend_ack for IE errors now */
	ret = check_infoelements(pc, l3m, ie_SUSPEND_ACKNOWLEDGE, MT_SUSPEND_ACKNOWLEDGE);
	mISDN_l3up(pc, MT_SUSPEND_ACKNOWLEDGE, l3m);
	release_l3_process(pc);
}

static void
l3dss1_suspend_rej(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char	cause;

	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_SUSPEND_REJECT, MT_SUSPEND_REJECT);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);
	mISDN_l3up(pc, MT_SUSPEND_REJECT, l3m);
	newl3state(pc, 10);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
}

static void
l3dss1_resume_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (!(ret = l3dss1_get_cid(pc, l3m))) {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options)) {
			if ((0 == (pc->cid[1] & 3)) || (3 == (pc->cid[1] & 3))) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
		} /* valid test for primary rate ??? */
	} else if (1 == pc->state) {
		unsigned char	cause;
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_RESUME_ACKNOWLEDGE, MT_RESUME_ACKNOWLEDGE);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);
	mISDN_l3up(pc, MT_RESUME_ACKNOWLEDGE, l3m);
	newl3state(pc, 10);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
}

static void
l3dss1_resume_rej(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char	cause;

	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_RESUME_REJECT, MT_RESUME_REJECT);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	L3DelTimer(&pc->timer1);
	mISDN_l3up(pc, MT_RESUME_REJECT, l3m);
	newl3state(pc, 0);
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
	release_l3_process(pc);
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
	memset(pc->cid, 0, 4); /* clear cid */
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
l3dss1_dummy(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
}

static void
l3dss1_hold_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
		if ((pc->state & VALID_HOLD_STATES_PTMP) == 0) { /* not a valid HOLD state for PtMP */
			return;
		}
	}
	switch(pc->aux_state) {
		case AUX_IDLE:
			break;
		default:
			eprint("RETRIEVE_REQ in wrong aux state %d\n", pc->aux_state);
		case AUX_HOLD_IND: /* maybe collition, ignored */
			return;
	}
	if (l3m)
		SendMsg(pc, l3m, -1);
	else
		l3dss1_message(pc, MT_HOLD);
	pc->aux_state = AUX_HOLD_REQ;
	L3AddTimer(&pc->timer2, THOLD, CC_THOLD);
}

static void
l3dss1_hold_ack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	switch(pc->aux_state) {
		case AUX_HOLD_IND:
			break;
		default:
			eprint("HOLD_ACK in wrong aux state %d\n", pc->aux_state);
			return;
	}
	if (l3m)
		SendMsg(pc, l3m, -1);
	else
		l3dss1_message(pc, MT_HOLD_ACKNOWLEDGE);
	pc->aux_state = AUX_CALL_HELD;
}

static void
l3dss1_hold_rej_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	switch(pc->aux_state) {
		case AUX_HOLD_IND:
			break;
		default:
			eprint("HOLD_REJ in wrong aux state %d\n", pc->aux_state);
			return;
	}
	if (l3m)
		SendMsg(pc, l3m, -1);
	else
		l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_RESOURCES_UNAVAIL); // FIXME
	pc->aux_state = AUX_IDLE;
}

static void
l3dss1_hold_ind(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	ret = check_infoelements(pc, l3m, ie_HOLD, MT_HOLD);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	if (test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
		if ((pc->state & VALID_HOLD_STATES_PTP) == 0) { /* not a valid HOLD state for PtP */
			l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_NOTCOMPAT_STATE);
			free_l3_msg(l3m);
			return;
		}
	} else {
		if ((pc->state & VALID_HOLD_STATES_PTMP) == 0) { /* not a valid HOLD state for PtMP */
			l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_NOTCOMPAT_STATE);
			free_l3_msg(l3m);
			return;
		}
	}
	switch(pc->aux_state) {
		case AUX_HOLD_REQ:
			L3DelTimer(&pc->timer2);
		case AUX_IDLE:
			mISDN_l3up(pc, MT_HOLD, l3m);
			pc->aux_state = AUX_HOLD_IND;
			break;
		default:
			l3dss1_message_cause(pc, MT_HOLD_REJECT, CAUSE_NOTCOMPAT_STATE);
			free_l3_msg(l3m);
			return;
	}
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
}

static void
l3dss1_hold_rej(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char	cause;

	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_HOLD_REJECT, MT_HOLD_REJECT);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	switch(pc->aux_state) {
		case AUX_HOLD_REQ:
			L3DelTimer(&pc->timer2);
			break;
		default:
			eprint("HOLD_REJ in wrong aux state %d\n", pc->aux_state);
	}
	pc->aux_state = AUX_IDLE;
	mISDN_l3up(pc, MT_HOLD_REJECT, l3m);
}

static void
l3dss1_hold_ignore(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
}

static void
l3dss1_hold_req_ignore(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (l3m)
		free_l3_msg(l3m);
}

static void
l3dss1_hold_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	ret = check_infoelements(pc, l3m, ie_HOLD_ACKNOWLEDGE, MT_HOLD_ACKNOWLEDGE);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	switch(pc->aux_state) {
		case AUX_HOLD_REQ:
			L3DelTimer(&pc->timer2);
			mISDN_l3up(pc, MT_HOLD_ACKNOWLEDGE, l3m);
			pc->aux_state = AUX_CALL_HELD;
			break;
		default:
			eprint("HOLD_ACK in wrong aux state %d\n", pc->aux_state);
			free_l3_msg(l3m);
	}
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
}

static void
l3dss1_retrieve_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	if (!test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
		if ((pc->state & (VALID_HOLD_STATES_PTMP | SBIT(12))) == 0) { /* not a valid RETRIEVE state for PtMP */
			if (l3m)
				free_l3_msg(l3m);
			return;
		}
	}
	switch(pc->aux_state) {
		case AUX_CALL_HELD:
			break;
		default:
			eprint("RETRIEVE_REQ in wrong aux state %d\n", pc->aux_state);
		case AUX_RETRIEVE_IND: /* maybe collition, ignored */
			if (l3m)
				free_l3_msg(l3m);
			return;
	}
	if (l3m) {
		SendMsg(pc, l3m, -1);
	} else {
		newl3state(pc, -1);
		l3dss1_message(pc, MT_RETRIEVE);
	}
	pc->aux_state = AUX_RETRIEVE_REQ;
	L3AddTimer(&pc->timer2, TRETRIEVE, CC_TRETRIEVE);
}

static void
l3dss1_retrieve_ack_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	switch(pc->aux_state) {
		case AUX_RETRIEVE_IND:
			break;
		default:
			eprint("HOLD_REJ in wrong aux state %d\n", pc->aux_state);
			if (l3m)
				free_l3_msg(l3m);
			return;
	}
	if (l3m)
		SendMsg(pc, l3m, -1);
	else
		l3dss1_message(pc, MT_RETRIEVE_ACKNOWLEDGE);
	pc->aux_state = AUX_IDLE;
}

static void
l3dss1_retrieve_rej_req(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	switch(pc->aux_state) {
		case AUX_RETRIEVE_IND:
			break;
		default:
			eprint("HOLD_REJ in wrong aux state %d\n", pc->aux_state);
			if (l3m)
				free_l3_msg(l3m);
			return;
	}
	if (l3m)
		SendMsg(pc, l3m, -1);
	else
		l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_RESOURCES_UNAVAIL); // FIXME
	pc->aux_state = AUX_CALL_HELD;
}


static void
l3dss1_retrieve_ind(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	if (test_bit(MISDN_FLG_PTP, &pc->L3->ml3.options)) {
		if ((pc->state & (VALID_HOLD_STATES_PTP | SBIT(12))) == 0) { /* not a valid RETRIEVE state for PtP */
			l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_NOTCOMPAT_STATE);
			free_l3_msg(l3m);
			return;
		}
	} else {
		if ((pc->state & (VALID_HOLD_STATES_PTMP | SBIT(12))) == 0) { /* not a valid RETRIEVE state for PtMP */
			l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_NOTCOMPAT_STATE);
			free_l3_msg(l3m);
			return;
		}
	}
	ret = check_infoelements(pc, l3m, ie_RETRIEVE, MT_RETRIEVE);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	switch(pc->aux_state) {
		case AUX_RETRIEVE_REQ:
			L3DelTimer(&pc->timer2);
		case AUX_CALL_HELD:
			if (!l3m->channel_id) {
				l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_MANDATORY_IE_MISS);
				free_l3_msg(l3m);
				return;
			}
			mISDN_l3up(pc, MT_RETRIEVE, l3m);
			pc->aux_state = AUX_RETRIEVE_IND;
			break;
		default:
			l3dss1_message_cause(pc, MT_RETRIEVE_REJECT, CAUSE_NOTCOMPAT_STATE);
			free_l3_msg(l3m);
			return;
	}
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
}

static void
l3dss1_retrieve_ack(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int	ret;

	ret = check_infoelements(pc, l3m, ie_RETRIEVE_ACKNOWLEDGE, MT_RETRIEVE_ACKNOWLEDGE);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	switch(pc->aux_state) {
		case AUX_RETRIEVE_REQ:
			L3DelTimer(&pc->timer2);
			if (!l3m->channel_id) {
				l3dss1_status_send(pc, CAUSE_INVALID_CONTENTS);
				free_l3_msg(l3m);
				return;
			}
			mISDN_l3up(pc, MT_RETRIEVE_ACKNOWLEDGE, l3m);
			pc->aux_state = AUX_IDLE;
			break;
		default:
			eprint("RETRIEVE_ACK in wrong aux state %d\n", pc->aux_state);
			free_l3_msg(l3m);
	}
	if (ret) /* STATUS for none mandatory IE errors after actions are taken */
		l3dss1_std_ie_err(pc, ret);
}

static void
l3dss1_retrieve_rej(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int		ret;
	unsigned char	cause;

	if ((ret = l3dss1_get_cause(pc, l3m))) {
		if (ret == -1)
			cause = CAUSE_MANDATORY_IE_MISS;
		else
			cause = CAUSE_INVALID_CONTENTS;
		l3dss1_status_send(pc, cause);
		free_l3_msg(l3m);
		return;
	}
	ret = check_infoelements(pc, l3m, ie_RETRIEVE_REJECT, MT_RETRIEVE_REJECT);
	if (Q931_ERROR_COMPREH == ret) {
		l3dss1_std_ie_err(pc, ret);
		free_l3_msg(l3m);
		return;
	}
	switch(pc->aux_state) {
		case AUX_RETRIEVE_REQ:
			L3DelTimer(&pc->timer2);
			pc->aux_state = AUX_CALL_HELD;
			break;
		default:
			eprint("RETRIEVE_REJ in wrong aux state %d\n", pc->aux_state);
	}
	pc->aux_state = AUX_IDLE;
	mISDN_l3up(pc, MT_RETRIEVE_REJECT, l3m);
}

static void
l3dss1_thold(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer2);
#if 0
	pc->cause = 102;	/* Timer expiry */
	pc->para.loc = 0;	/* local */
#endif
	mISDN_l3up(pc, MT_HOLD_REJECT, NULL);
	pc->aux_state = AUX_IDLE;
}

static void
l3dss1_tretrieve(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer2);
#if 0
	pc->cause = 102;	/* Timer expiry */
	pc->para.loc = 0;	/* local */
#endif
	mISDN_l3up(pc, MT_RETRIEVE_REJECT, NULL);
	pc->aux_state = AUX_CALL_HELD;
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
	L3DelTimer(&pc->timer1);
	newl3state(pc, 11);
	l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_INVALID_NUMBER);
	send_timeout(pc, "302");
}

static void
l3dss1_t303(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	if (pc->n303 > 0) {
		pc->n303--;
		if (pc->t303msg) {
			struct l3_msg *nl3m = pc->t303msg;

			if (pc->n303 > 0)
				l3_msg_increment_refcnt(pc->t303msg);
			else
				pc->t303msg = NULL;
			SendMsg(pc, nl3m, -1);
		}
		L3AddTimer(&pc->timer1, T303, CC_T303);
		return;
	}
	if (pc->t303msg)
		free_l3_msg(pc->t303msg);
	pc->t303msg = NULL;
	l3dss1_message_cause(pc, MT_RELEASE_COMPLETE, CAUSE_TIMER_EXPIRED);
	send_timeout(pc, "303");
	release_l3_process(pc);
}

static void
l3dss1_t304(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	newl3state(pc, 11);
	l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_TIMER_EXPIRED);
	send_timeout(pc, "304");
}

static void
l3dss1_t305(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	int cause;
	
	L3DelTimer(&pc->timer1);
	if (pc->cause != NO_CAUSE) {
		cause = pc->cause;
	} else {
		cause = CAUSE_NORMAL_CLEARING;
	}
	
	newl3state(pc, 19);
	l3dss1_message_cause(pc, MT_RELEASE, cause);
	L3AddTimer(&pc->timer1, T308, CC_T308_1);
}

static void
l3dss1_t310(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	newl3state(pc, 11);
	l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_TIMER_EXPIRED);
	send_timeout(pc, "310");
}

static void
l3dss1_t313(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	newl3state(pc, 11);
	l3dss1_message_cause(pc, MT_DISCONNECT, CAUSE_TIMER_EXPIRED);
	send_timeout(pc, "313");
}

static void
l3dss1_t308_1(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	newl3state(pc, 19);
	L3DelTimer(&pc->timer1);
	l3dss1_message(pc, MT_RELEASE);
	L3AddTimer(&pc->timer1, T308, CC_T308_2);
}

static void
l3dss1_t308_2(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
	send_timeout(pc, "308");
	release_l3_process(pc);
}

static void
l3dss1_t318(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
#if 0
	pc->cause = 102;	/* Timer expiry */
	pc->para.loc = 0;	/* local */
#endif
	mISDN_l3up(pc, MT_RESUME_REJECT, NULL);
	newl3state(pc, 19);
	l3dss1_message(pc, MT_RELEASE);
	L3AddTimer(&pc->timer1, T308, CC_T308_1);
}

static void
l3dss1_t319(l3_process_t *pc, unsigned int pr, struct l3_msg *l3m)
{
	L3DelTimer(&pc->timer1);
#if 0
	pc->cause = 102;	/* Timer expiry */
	pc->para.loc = 0;	/* local */
#endif
	mISDN_l3up(pc, MT_SUSPEND_REJECT, NULL);
	newl3state(pc, 10);
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
	l3dss1_disconnect_req(pc, pr, l3m);
}

static void
l3dss1_dl_release(l3_process_t *pc, unsigned int pr, struct l3_msg  *arg)
{
	newl3state(pc, 0);
#if 0
        pc->cause = 0x1b;          /* Destination out of order */
        pc->para.loc = 0;
#endif
	release_l3_process(pc);
}

static void
l3dss1_dl_reestablish(l3_process_t *pc, unsigned int pr, struct l3_msg  *arg)
{
	L3DelTimer(&pc->timer1);
	L3AddTimer(&pc->timer1, T309, CC_T309);
	l3_manager(pc->l2if, DL_ESTABLISH_REQ);
}

static void
l3dss1_dl_reest_status(l3_process_t *pc, unsigned int pr, struct l3_msg  *arg)
{
	L3DelTimer(&pc->timer1);

	l3dss1_status_send(pc, CAUSE_NORMALUNSPECIFIED);
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
	{SBIT(0),
	 MT_SETUP, l3dss1_setup_req},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) |
		SBIT(10) | SBIT(11) | SBIT(12) | SBIT(15) | SBIT(25),
	 MT_INFORMATION, l3dss1_information_req},
	{ALL_STATES,
	 MT_NOTIFY, l3dss1_notify_req},
	{SBIT(10),
	 MT_PROGRESS, l3dss1_progress_req},
	{SBIT(0),
	 MT_RESUME, l3dss1_resume_req},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(6) | SBIT(7) |
		SBIT(8) | SBIT(9) | SBIT(10) | SBIT(25),
	 MT_DISCONNECT, l3dss1_disconnect_req},
	{SBIT(6) | SBIT(11) | SBIT(12),
	 MT_RELEASE, l3dss1_release_req},
	{ALL_STATES,
	 MT_RESTART, l3dss1_restart},
	{ALL_STATES,
	 MT_RELEASE_COMPLETE, l3dss1_release_cmpl_req},
	{SBIT(6) | SBIT(25),
	 MT_CALL_PROCEEDING, l3dss1_proceed_req},
	{SBIT(6),
	 MT_SETUP_ACKNOWLEDGE, l3dss1_setup_ack_req},
	{SBIT(25),
	 MT_SETUP_ACKNOWLEDGE, l3dss1_dummy},
	{SBIT(6) | SBIT(9) | SBIT(25),
	 MT_ALERTING, l3dss1_alert_req},
	{SBIT(6) | SBIT(7) | SBIT(9) | SBIT(25),
	 MT_CONNECT, l3dss1_connect_req},
	{SBIT(10),
	 MT_SUSPEND, l3dss1_suspend_req},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10),
	 MT_HOLD, l3dss1_hold_req},
	{ALL_STATES,
	 MT_HOLD, l3dss1_hold_req_ignore},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10),
	 MT_HOLD_ACKNOWLEDGE, l3dss1_hold_ack_req},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10),
	 MT_HOLD_REJECT, l3dss1_hold_rej_req},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE, l3dss1_retrieve_req},
	{ALL_STATES,
	 MT_RETRIEVE, l3dss1_hold_req_ignore},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE_ACKNOWLEDGE, l3dss1_retrieve_ack_req},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE_REJECT, l3dss1_retrieve_rej_req},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10),
	 MT_HOLD , l3dss1_hold_req},
	{ALL_STATES,
	 MT_FACILITY, l3dss1_facility_req},
	{ALL_STATES,
	 MT_STATUS_ENQUIRY, l3dss1_status_enq_req},
};

#define DOWNSLLEN \
	(sizeof(downstatelist) / sizeof(struct stateentry))

static struct stateentry datastatelist[] =
{
	{ALL_STATES,
	 MT_STATUS_ENQUIRY, l3dss1_status_enq},
	{ALL_STATES,
	 MT_FACILITY, l3dss1_facility},
	{SBIT(19),
	 MT_STATUS, l3dss1_release_ind},
	{ALL_STATES,
	 MT_STATUS, l3dss1_status},
	{SBIT(0),
	 MT_SETUP, l3dss1_setup},
	{SBIT(6) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(11) | SBIT(12) |
	 SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
	 MT_SETUP, l3dss1_dummy},
	{SBIT(1) | SBIT(2),
	 MT_CALL_PROCEEDING, l3dss1_call_proc},
	{SBIT(1),
	 MT_SETUP_ACKNOWLEDGE, l3dss1_setup_ack},
	{SBIT(2) | SBIT(3),
	 MT_ALERTING, l3dss1_alerting},
	{SBIT(2) | SBIT(3),
	 MT_PROGRESS, l3dss1_progress},
	{SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
	 MT_INFORMATION, l3dss1_information},
	{ALL_STATES,
	 MT_NOTIFY, l3dss1_notify},
	{SBIT(0) | SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(10) |
	 SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(19) | SBIT(25),
	 MT_RELEASE_COMPLETE, l3dss1_release_cmpl},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(11) | SBIT(12) | SBIT(15) | SBIT(17) | SBIT(25),
	 MT_RELEASE, l3dss1_release},
	{SBIT(19),  MT_RELEASE, l3dss1_release_ind},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(11) | SBIT(15) | SBIT(17) | SBIT(25),
	 MT_DISCONNECT, l3dss1_disconnect},
	{SBIT(19),
	 MT_DISCONNECT, l3dss1_dummy},
	{SBIT(1) | SBIT(2) | SBIT(3) | SBIT(4),
	 MT_CONNECT, l3dss1_connect},
	{SBIT(8),
	 MT_CONNECT_ACKNOWLEDGE, l3dss1_connect_ack},
	{SBIT(15),
	 MT_SUSPEND_ACKNOWLEDGE, l3dss1_suspend_ack},
	{SBIT(15),
	 MT_SUSPEND_REJECT, l3dss1_suspend_rej},
	{SBIT(17),
	 MT_RESUME_ACKNOWLEDGE, l3dss1_resume_ack},
	{SBIT(17),
	 MT_RESUME_REJECT, l3dss1_resume_rej},
	{SBIT(12) | SBIT(19),
	 MT_HOLD, l3dss1_hold_ignore},
	{ALL_STATES,
	 MT_HOLD, l3dss1_hold_ind},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10),
	 MT_HOLD_REJECT, l3dss1_hold_rej},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10),
	 MT_HOLD_ACKNOWLEDGE, l3dss1_hold_ack},
	{ALL_STATES,
	 MT_RETRIEVE, l3dss1_retrieve_ind},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE_REJECT, l3dss1_retrieve_rej},
	{SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10) | SBIT(12),
	 MT_RETRIEVE_ACKNOWLEDGE, l3dss1_retrieve_ack},
};

#define DATASLLEN \
	(sizeof(datastatelist) / sizeof(struct stateentry))

static struct stateentry globalmes_list[] =
{
	{ALL_STATES,
	 MT_STATUS, l3dss1_status},
	{SBIT(0),
	 MT_RESTART, l3dss1_global_restart},
	{SBIT(1),
	 MT_RESTART_ACKNOWLEDGE, l3dss1_restart_ack},
};
#define GLOBALM_LEN \
	(sizeof(globalmes_list) / sizeof(struct stateentry))

static struct stateentry manstatelist[] =
{
        {SBIT(2),
         DL_ESTABLISH_IND, l3dss1_dl_reset},
        {SBIT(10),
         DL_ESTABLISH_CNF, l3dss1_dl_reest_status},
        {SBIT(10),
         DL_RELEASE_IND, l3dss1_dl_reestablish},
        {ALL_STATES,
         DL_RELEASE_IND, l3dss1_dl_release},
        {ALL_STATES,
         DL_ESTABLISH_IND, l3dss1_dl_ignore},
        {ALL_STATES,
         DL_ESTABLISH_CNF, l3dss1_dl_ignore},
	{SBIT(25),
	 CC_T302, l3dss1_t302},
	{SBIT(1),
	 CC_T303, l3dss1_t303},
	{SBIT(2),
	 CC_T304, l3dss1_t304},
	{SBIT(3),
	 CC_T310, l3dss1_t310},
	{SBIT(8),
	 CC_T313, l3dss1_t313},
	{SBIT(11),
	 CC_T305, l3dss1_t305},
	{SBIT(15),
	 CC_T319, l3dss1_t319},
	{SBIT(17),
	 CC_T318, l3dss1_t318},
	{SBIT(19),
	 CC_T308_1, l3dss1_t308_1},
	{SBIT(19),
	 CC_T308_2, l3dss1_t308_2},
	{SBIT(10),
	 CC_T309, l3dss1_dl_release},
	{SBIT(6),
	 CC_TCTRL, l3dss1_reset},
	{ALL_STATES,
	 CC_THOLD, l3dss1_thold},
	{ALL_STATES,
	 CC_TRETRIEVE, l3dss1_tretrieve},
	{ALL_STATES,
	 MT_RESTART, l3dss1_restart},
};

#define MANSLLEN \
        (sizeof(manstatelist) / sizeof(struct stateentry))
/* *INDENT-ON* */


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
dss1_fromdown(layer3_t *l3, struct mbuffer *msg)
{
	u_int		i;
	int		cause, callState, ret;
	l3_process_t	*proc;

	if (msg->len < 3) {
		eprint("dss1up frame too short(%d)\n", msg->len);
		goto freemsg;
	}
	if (msg->data[0] != Q931_PD)
		goto freemsg;
	ret = parseQ931(msg);
	if (ret & Q931_ERROR_FATAL) {
		eprint("dss1up: parse IE error %x\n", ret); 
		goto freemsg;
	}
	if (msg->l3h.crlen == 0) {	/* Dummy Callref */
		if (msg->l3h.type == MT_FACILITY) {
			l3dss1_facility(&l3->dummy, msg->h->prim, &msg->l3);
			return 0;
		}
		goto freemsg;
	} else if ((msg->l3h.cr & 0x7fff) == 0) {	/* Global CallRef */
		global_handler(l3, msg->l3h.type, msg);
		return 0;
	} else if (!(proc = get_l3process4cref(l3, msg->l3.pid))) {
		/* No transaction process exist, that means no call with
		 * this callreference is active
		 */
		if (msg->l3h.type == MT_SETUP) {
			/* Setup creates a new l3 process */
			if (msg->l3h.cr & 0x8000) {
				/* Setup with wrong CREF flag */
				goto freemsg;
			}
			if (!(proc = create_new_process(l3, msg->addr.channel,msg->l3h.cr, NULL))) {
				/* Maybe should answer with RELEASE_COMPLETE and
				 * CAUSE 0x2f "Resource unavailable", but this
				 * need a new_l3_process too ... arghh
				 */
				goto freemsg;
			}
			/* register this ID in L4 */
			// we will try without this for now
			// ret = mISDN_l3up(proc, MT_NEW_CR, NULL);
#if 0
			if (ret) {
				eprint("dss1up: cannot register ID(%x)\n",
					proc->id);
				release_l3_process(proc);
				goto freemsg;
			}
#endif
		} else if (msg->l3h.type == MT_STATUS) {
			cause = 0;
			if (msg->l3.cause) {
				if (msg->l3.cause[0] >= 2)
					cause = msg->l3.cause[2] & 0x7f;
				else
					cause = msg->l3.cause[1] & 0x7f;
			}
			callState = 0;
			if (msg->l3.call_state) {
				callState = msg->l3.call_state[1];
			}
			/* ETS 300-104 part 2.4.1
			 * if setup has not been made and a message type
			 * MT_STATUS is received with call state == 0,
			 * we must send nothing
			 */
			if (callState != 0) {
				/* ETS 300-104 part 2.4.2
				 * if setup has not been made and a message type
				 * MT_STATUS is received with call state != 0,
				 * we must send MT_RELEASE_COMPLETE cause 101
				 */
				if ((proc = create_new_process(l3, msg->addr.channel,msg->l3h.cr, NULL))) {
					l3dss1_msg_without_setup(proc, CAUSE_NOTCOMPAT_STATE);
				}
			}
			goto freemsg;
		} else if (msg->l3h.type == MT_RELEASE_COMPLETE) {
			goto freemsg;
		} else {
			/* ETS 300-104 part 2
			 * if setup has not been made and a message type
			 * (except MT_SETUP and RELEASE_COMPLETE) is received,
			 * we must send MT_RELEASE_COMPLETE cause 81 */
			
			eprint("We got Message with Invalid Callref\n");
			if ((proc = create_new_process(l3, msg->addr.channel,msg->l3h.cr, NULL))) {
				l3dss1_msg_without_setup(proc, CAUSE_INVALID_CALLREF);
			}
			goto freemsg;
		}
	}
	if (l3dss1_check_messagetype_validity(proc, msg->l3h.type))
			goto freemsg;
	for (i = 0; i < DATASLLEN; i++)
		if ((msg->l3h.type == datastatelist[i].primitive) &&
		    ((1 << proc->state) & datastatelist[i].state))
			break;
	if (i == DATASLLEN) {
		if ((MT_RELEASE_COMPLETE != msg->l3h.type) && (MT_RELEASE != msg->l3h.type)) {
			l3dss1_status_send(proc, CAUSE_NOTCOMPAT_STATE);
		}
	} else {
		datastatelist[i].rout(proc, msg->h->prim, &msg->l3);
		return 0;
	}
freemsg:
	free_mbuffer(msg);
	return 0;
}

static int
dss1_fromup(layer3_t *l3, struct l3_msg *l3m)
{
	u_int		i;
	l3_process_t	*proc;

	if (l3m->pid == MISDN_PID_DUMMY) {
		if (l3m->type == MT_FACILITY) {
			l3dss1_facility_req(&l3->dummy, l3m->type, l3m);
			return 0;
		}
		return -EINVAL;
	}

	if (l3m->pid == MISDN_PID_GLOBAL) {
		if (l3m->type == MT_RESTART) {
			l3dss1_restart_req(&l3->global, l3m->type, l3m);
			return 0;
		}
		return -EINVAL;
	}

	proc = get_l3process4pid(l3, l3m->pid);
	if (!proc) {
		eprint("mISDN dss1 fromup without proc pr=%04x dinfo(%x)\n", l3m->type, l3m->pid);
		return -EINVAL;
	}
	for (i = 0; i < DOWNSLLEN; i++)
		if ((l3m->type == downstatelist[i].primitive) && ((1 << proc->state) & downstatelist[i].state))
			break;
	if (i == DOWNSLLEN) {
		free_l3_msg(l3m);
	} else {
		downstatelist[i].rout(proc, l3m->type, l3m);
	}
	return 0;
}

static int
dss1man(l3_process_t *proc, u_int pr, struct l3_msg *l3m)
{
	u_int	i;
#warning	hier kommt an dsl_estab und dl_release

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
dss1_user_init(layer3_t *l3)
{
	l3->from_l2 = dss1_fromdown;
	l3->to_l3 = dss1_fromup;
	l3->p_mgr = dss1man;
	test_and_set_bit(FLG_USER, &l3->ml3.options);
}

struct l3protocol	dss1user = {
	.name = "DSS1 User",
	.protocol = L3_PROTOCOL_DSS1_USER ,
	.init = dss1_user_init
};

