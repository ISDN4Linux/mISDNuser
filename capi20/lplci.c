/* 
 * lplci.c
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2011  by Karsten Keil <kkeil@linux-pingi.de>
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

#include "m_capi.h"
#include "mc_buffer.h"
#include "SupplementaryService.h"
#include "../lib/include/helper.h"
#include <mISDN/q931.h>

static int lPLCILinkUp(struct lPLCI *);
static int lPLCILinkDown(struct lPLCI *);

// --------------------------------------------------------------------
// PLCI state machine
//
// Some rules:
//   *  EV_AP_*  events come from CAPI Application
//   *  EV_L3_*  events come from the ISDN stack
//   *  EV_PI_*  events generated in PLCI handling
//   *  messages are send in the routine that handle the event
//
// --------------------------------------------------------------------

enum {
	ST_PLCI_P_0,
	ST_PLCI_P_0_1,
	ST_PLCI_P_1,
	ST_PLCI_P_2,
	ST_PLCI_P_3,
	ST_PLCI_P_4,
	ST_PLCI_P_ACT,
	ST_PLCI_P_HELD,
	ST_PLCI_P_5,
	ST_PLCI_P_6,
	ST_PLCI_P_RES,
} const ST_PLCI_COUNT = ST_PLCI_P_RES + 1;

static char *str_st_plci[] = {
	"ST_PLCI_P_0",
	"ST_PLCI_P_0_1",
	"ST_PLCI_P_1",
	"ST_PLCI_P_2",
	"ST_PLCI_P_3",
	"ST_PLCI_P_4",
	"ST_PLCI_P_ACT",
	"ST_PLCI_P_HELD",
	"ST_PLCI_P_5",
	"ST_PLCI_P_6",
	"ST_PLCI_P_RES",
};

enum {
	EV_AP_CONNECT_REQ,
	EV_PI_CONNECT_CONF,
	EV_PI_CONNECT_IND,
	EV_AP_CONNECT_RESP,
	EV_PI_CONNECT_ACTIVE_IND,
	EV_AP_CONNECT_ACTIVE_RESP,
	EV_AP_ALERT_REQ,
	EV_AP_INFO_REQ,
	EV_PI_INFO_IND,
	EV_PI_FACILITY_IND,
	EV_AP_SELECT_B_PROTOCOL_REQ,
	EV_AP_DISCONNECT_REQ,
	EV_PI_DISCONNECT_IND,
	EV_AP_DISCONNECT_RESP,
	EV_AP_HOLD_REQ,
	EV_AP_RETRIEVE_REQ,
	EV_PI_HOLD_CONF,
	EV_PI_RETRIEVE_CONF,
	EV_AP_SUSPEND_REQ,
	EV_PI_SUSPEND_CONF,
	EV_AP_RESUME_REQ,
	EV_PI_RESUME_CONF,
	EV_PI_CHANNEL_ERR,
	EV_L3_SETUP_IND,
	EV_L3_SETUP_CONF_ERR,
	EV_L3_SETUP_CONF,
	EV_L3_SETUP_COMPL_IND,
	EV_L3_DISCONNECT_IND,
	EV_L3_RELEASE_IND,
	EV_L3_RELEASE_PROC_IND,
	EV_L3_NOTIFY_IND,
	EV_L3_HOLD_IND,
	EV_L3_HOLD_ACKNOWLEDGE,
	EV_L3_HOLD_REJECT,
	EV_L3_RETRIEVE_IND,
	EV_L3_RETRIEVE_ACKNOWLEDGE,
	EV_L3_RETRIEVE_REJECT,
	EV_L3_SUSPEND_ERR,
	EV_L3_SUSPEND_CONF,
	EV_L3_RESUME_ERR,
	EV_L3_RESUME_CONF,
	EV_L3_REJECT_IND,
	EV_PH_CONTROL_IND,
	EV_AP_RELEASE,
} const EV_PLCI_COUNT = EV_AP_RELEASE + 1;

static char *str_ev_plci[] = {
	"EV_AP_CONNECT_REQ",
	"EV_PI_CONNECT_CONF",
	"EV_PI_CONNECT_IND",
	"EV_AP_CONNECT_RESP",
	"EV_PI_CONNECT_ACTIVE_IND",
	"EV_AP_CONNECT_ACTIVE_RESP",
	"EV_AP_ALERT_REQ",
	"EV_AP_INFO_REQ",
	"EV_PI_INFO_IND",
	"EV_PI_FACILITY_IND",
	"EV_AP_SELECT_B_PROTOCOL_REQ",
	"EV_AP_DISCONNECT_REQ",
	"EV_PI_DISCONNECT_IND",
	"EV_AP_DISCONNECT_RESP",
	"EV_AP_HOLD_REQ",
	"EV_AP_RETRIEVE_REQ",
	"EV_PI_HOLD_CONF",
	"EV_PI_RETRIEVE_CONF",
	"EV_AP_SUSPEND_REQ",
	"EV_PI_SUSPEND_CONF",
	"EV_AP_RESUME_REQ",
	"EV_PI_RESUME_CONF",
	"EV_PI_CHANNEL_ERR",
	"EV_L3_SETUP_IND",
	"EV_L3_SETUP_CONF_ERR",
	"EV_L3_SETUP_CONF",
	"EV_L3_SETUP_COMPL_IND",
	"EV_L3_DISCONNECT_IND",
	"EV_L3_RELEASE_IND",
	"EV_L3_RELEASE_PROC_IND",
	"EV_L3_NOTIFY_IND",
	"EV_L3_HOLD_IND",
	"EV_L3_HOLD_ACKNOWLEDGE",
	"EV_L3_HOLD_REJECT",
	"EV_L3_RETRIEVE_IND",
	"EV_L3_RETRIEVE_ACKNOWLEDGE",
	"EV_L3_RETRIEVE_REJECT",
	"EV_L3_SUSPEND_ERR",
	"EV_L3_SUSPEND_CONF",
	"EV_L3_RESUME_ERR",
	"EV_L3_RESUME_CONF",
	"EV_L3_REJECT_IND",
	"EV_PH_CONTROL_IND",
	"EV_AP_RELEASE",
};

static struct Fsm plci_fsm = { 0, 0, 0, 0, 0 };

static void lPLCI_debug(struct FsmInst *fi, char *fmt, ...)
{
	char tmp[128];
	char *p = tmp;
	va_list args;
	struct lPLCI *lp = fi->userdata;

	if (!(MIDEBUG_STATES & mI_debug_mask))
		return;
	va_start(args, fmt);
	p += sprintf(p, "lPLCI 0x%x: ", lp->plci);
	p += vsprintf(p, fmt, args);
	*p = 0;
	dprint(MIDEBUG_STATES, "%s\n", tmp);
	va_end(args);
}

static inline void Send2Application(struct lPLCI *lp, struct mc_buf *mc)
{
	SendCmsg2Application(lp->lc->Appl, mc);
}

static inline void lPLCICmsgHeader(struct lPLCI *lp, _cmsg * cmsg, __u8 cmd, __u8 subcmd)
{
	capi_cmsg_header(cmsg, lp->lc->Appl->AppId, cmd, subcmd, lp->lc->Appl->MsgId++, lp->plci);
}

static void lPLCIClearOtherApps(struct lPLCI *lp)
{
	struct lPLCI *o_lp;
	struct mc_buf *mc = NULL;

	if (!lp->PLCI)
		return;
	o_lp = lp->PLCI->lPLCIs;
	while (o_lp) {
		if (o_lp != lp) {
			MC_BUF_ALLOC(mc);
			lPLCICmsgHeader(o_lp, &mc->cmsg, CAPI_DISCONNECT, CAPI_IND);
			mc->cmsg.Reason = 0x3304;	// other application got the call
			FsmEvent(&o_lp->plci_m, EV_PI_DISCONNECT_IND, mc);
		}
		o_lp = o_lp->next;
	}
	if (mc)
		free_mc_buf(mc);
}

static void lPLCIInfoIndMsg(struct lPLCI *lp, uint32_t mask, unsigned char mt, struct mc_buf *arg)
{
	struct mc_buf *mc = arg;

	if ((!(lp->lc->InfoMask & mask)))
		return;

	if (!mc) {
		MC_BUF_ALLOC(mc);
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_INFO, CAPI_IND);
	mc->cmsg.InfoNumber = 0x8000 | mt;
	mc->cmsg.InfoElement = 0;
	Send2Application(lp, mc);
	if (!arg)
		free_mc_buf(mc);
	else
		mc_clear_cmsg(mc);
}

static void lPLCIInfoIndIE(struct lPLCI *lp, unsigned char ie, uint32_t mask, struct mc_buf *mc)
{
	unsigned char **v_ie, *iep;
	int pos;

	if (!mc || mc->l3m)
		return;

	if (!(lp->lc->InfoMask & mask))	/* not requested by application */
		return;

	if (ie & 0x80) {	/* Single octet */
		if (ie != IE_COMPLETE)
			return;
		iep = &mc->l3m->sending_complete;
	} else {
		v_ie = &mc->l3m->bearer_capability;
		pos = l3_ie2pos(ie);
		if (pos < 0)	/* not supported IE */
			return;
		iep = v_ie[pos];
	}
	if ((iep == NULL) || (*iep == 0))	/* not available in message */
		return;

	mc_clear_cmsg(mc);
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_INFO, CAPI_IND);
	mc->cmsg.InfoNumber = ie;
	mc->cmsg.InfoElement = iep;
#ifdef HANDLE_EARLYB3
	if (ie == IE_PROGRESS && lp->lc->InfoMask & CAPI_INFOMASK_EARLYB3) {
		if (iep[0] == 0x02 && iep[2] == 0x88) {	// in-band information available
			lPLCILinkUp(lp);
			if (!test_bit(PLCI_STATE_STACKREADY, &lp->plci->state)) {
				Send2ApplicationDelayed(lp, cmsg);
				return;
			}
		}
	}
#endif
	Send2Application(lp, mc);
}

uint16_t q931CIPValue(struct mc_buf * mc)
{
	uint16_t CIPValue = 0;
	int capability, mode, rate, oct5;
	int hlc = -1, ehlc = -1;
	int ret, l;
	char bdebug[48];

	if (!mc->l3m)
		return 0;
	if (!mc->l3m->bearer_capability)
		return 0;

	/* mi_decode_bearer_capability(struct l3_msg *l3m,
	   int *coding, int *capability, int *mode, int *rate,
	   int *oct_4a, int *oct_4b, int *oct_5, int *oct_5a, int *oct_5b1,
	   int *oct_5b2, int *oct_5c, int *oct_5d, int *oct_6, int *oct_7)
	 */

	ret = mi_decode_bearer_capability(mc->l3m, NULL, &capability, &mode, &rate,
					  NULL, NULL, &oct5, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	l = *mc->l3m->bearer_capability;
	if (l > 13)
		l = 14;
	else
		l++;
	mi_shexprint(bdebug, mc->l3m->bearer_capability, l);
	if (ret) {
		wprint("Error decoding bearer %s return %d - %s\n", bdebug, ret, strerror(-ret));
		return 0;
	}

	if (mode == 0) {
		switch (capability) {
		case Q931_CAP_SPEECH:
			CIPValue = 1;
			break;
		case Q931_CAP_UNRES_DIGITAL:
			CIPValue = 2;
			break;
		case Q931_CAP_RES_DIGITAL:
			CIPValue = 3;
			break;
		case Q931_CAP_3KHZ_AUDIO:
			CIPValue = 4;
			break;
		case Q931_CAP_7KHZ_AUDIO:
			if (oct5 == 0xA5)
				CIPValue = 9;
			else
				CIPValue = 5;
			break;
		case Q931_CAP_VIDEO:
			CIPValue = 6;
			break;
		default:
			wprint("No valid capability %x in bearer %s\n", capability, bdebug);
			return 0;
		}
	} else if (mode == 2) {
		CIPValue = 7;
	} else {
		wprint("Invalid mode %d in bearer %s\n", mode, bdebug);
		return 0;
	}

	if (mc->l3m->hlc) {
		ret = mi_decode_hlc(mc->l3m, &hlc, &ehlc);
		l = *mc->l3m->hlc;
		if (l > 5)
			l = 6;
		else
			l++;
		mi_shexprint(bdebug, mc->l3m->hlc, l);
		if (!ret) {
			switch (hlc) {
			case 0x01:
				if (CIPValue == 1)
					CIPValue = 16;
				else if (CIPValue == 9)
					CIPValue = 26;
				break;
			case 0x04:
				if (CIPValue == 4)
					CIPValue = 17;
				break;
			case 0x21:
				if (CIPValue == 2)
					CIPValue = 18;
				break;
			case 0x24:
				if (CIPValue == 2)
					CIPValue = 19;
				break;
			case 0x28:
				if (CIPValue == 2)
					CIPValue = 20;
				break;
			case 0x31:
				if (CIPValue == 2)
					CIPValue = 21;
				break;
			case 0x32:
				if (CIPValue == 2)
					CIPValue = 22;
				break;
			case 0x35:
				if (CIPValue == 2)
					CIPValue = 23;
				break;
			case 0x38:
				if (CIPValue == 2)
					CIPValue = 24;
				break;
			case 0x41:
				if (CIPValue == 2)
					CIPValue = 25;
				break;
			case 0x60:
				if (CIPValue == 2 && ehlc == 2)
					CIPValue = 28;
				else if (CIPValue == 9 && ehlc == 1)
					CIPValue = 27;
				break;
			default:
				break;
			}
		} else
			wprint("Cannot decode HLC %s return %d - %s\n", bdebug, ret, strerror(-ret));
	}
	return CIPValue;
}

uint16_t CIPValue2setup(uint16_t CIPValue, struct l3_msg * l3m)
{
	switch (CIPValue) {
	case 16:
		mi_encode_bearer(l3m, Q931_CAP_SPEECH, Q931_L1INFO_ALAW, 0, 0x10);
		mi_encode_hlc(l3m, 1, -1);
		break;
	case 17:
		mi_encode_bearer(l3m, Q931_CAP_3KHZ_AUDIO, Q931_L1INFO_ALAW, 0, 0x10);
		// mi_encode_hlc(l3m, 4, -1);
		break;
	case 1:
		mi_encode_bearer(l3m, Q931_CAP_SPEECH, Q931_L1INFO_ALAW, 0, 0x10);
		break;
	case 2:
		mi_encode_bearer(l3m, Q931_CAP_UNRES_DIGITAL, -1, 0, 0x10);
		break;
	case 3:
		mi_encode_bearer(l3m, Q931_CAP_RES_DIGITAL, -1, 0, 0x10);
		break;
	case 4:
		mi_encode_bearer(l3m, Q931_CAP_3KHZ_AUDIO, Q931_L1INFO_ALAW, 0, 0x10);
		break;
	default:
		return CapiIllMessageParmCoding;
	}
	return 0;
}

uint16_t cmsg2setup_req(_cmsg * cmsg, struct l3_msg * l3m)
{
	if (CIPValue2setup(cmsg->CIPValue, l3m))
		goto err;
	if (cmsg->CallingPartyNumber && cmsg->CallingPartyNumber[0])
		add_layer3_ie(l3m, IE_CALLING_PN, cmsg->CallingPartyNumber[0], &cmsg->CallingPartyNumber[1]);
	if (cmsg->CallingPartySubaddress && cmsg->CallingPartySubaddress[0])
		add_layer3_ie(l3m, IE_CALLING_SUB, cmsg->CallingPartySubaddress[0], &cmsg->CallingPartySubaddress[1]);
	if (cmsg->CalledPartyNumber && cmsg->CalledPartyNumber[0])
		add_layer3_ie(l3m, IE_CALLED_PN, cmsg->CalledPartyNumber[0], &cmsg->CalledPartyNumber[1]);
	if (cmsg->CalledPartySubaddress && cmsg->CalledPartySubaddress[0])
		add_layer3_ie(l3m, IE_CALLED_SUB, cmsg->CalledPartySubaddress[0], &cmsg->CalledPartySubaddress[1]);
	if (cmsg->LLC && cmsg->LLC[0])
		add_layer3_ie(l3m, IE_LLC, cmsg->LLC[0], &cmsg->LLC[1]);
	if (cmsg->HLC && cmsg->HLC[0]) {
		l3m->hlc = NULL;
		add_layer3_ie(l3m, IE_HLC, cmsg->HLC[0], &cmsg->HLC[1]);
	}
	return 0;
err:
	return CapiIllMessageParmCoding;
}

uint16_t cmsg2info_req(_cmsg * cmsg, struct l3_msg * l3m)
{
	if (cmsg->Keypadfacility && cmsg->Keypadfacility[0])
		add_layer3_ie(l3m, IE_KEYPAD, cmsg->Keypadfacility[0], &cmsg->Keypadfacility[1]);
	if (cmsg->CalledPartyNumber && cmsg->CalledPartyNumber[0])
		add_layer3_ie(l3m, IE_CALLED_PN, cmsg->CalledPartyNumber[0], &cmsg->CalledPartyNumber[1]);
	return 0;
}

uint16_t cmsg2alerting_req(_cmsg * cmsg, struct l3_msg * l3m)
{
	if (cmsg->Useruserdata && cmsg->Useruserdata[0])
		add_layer3_ie(l3m, IE_USER_USER, cmsg->Useruserdata[0], &cmsg->Useruserdata[1]);
	return 0;
}

uint16_t lPLCICheckBprotocol(struct lPLCI * lp, _cmsg * cmsg)
{
	struct pController *pc = lp->PLCI->pc;
	unsigned long sprot;
	int val;

	/* no endian translation */
	sprot = pc->profile.support1;
	if (!test_bit(cmsg->B1protocol, &sprot))
		return CapiB1ProtocolNotSupported;
	sprot = pc->profile.support2;
	if (!test_bit(cmsg->B2protocol, &sprot))
		return CapiB2ProtocolNotSupported;
	sprot = pc->profile.support3;
	if (!test_bit(cmsg->B3protocol, &sprot))
		return CapiB3ProtocolNotSupported;
	lp->Bprotocol.B1 = cmsg->B1protocol;
	lp->Bprotocol.B2 = cmsg->B2protocol;
	lp->Bprotocol.B3 = cmsg->B3protocol;
	if (cmsg->B1configuration && cmsg->B1configuration[0]) {
		if (cmsg->B1configuration[0] > 15) {
			wprint("B1cfg too large(%d)\n", cmsg->B1configuration[0]);
			return CapiB1ProtocolParameterNotSupported;
		}
		memcpy(&lp->Bprotocol.B1cfg[0], cmsg->B1configuration, cmsg->B1configuration[0] + 1);
	} else
		lp->Bprotocol.B1cfg[0] = 0;
	if (cmsg->B2configuration && cmsg->B2configuration[0]) {
		if (cmsg->B2configuration[0] > 15) {
			wprint("B2cfg too large(%d)\n", cmsg->B2configuration[0]);
			return CapiB2ProtocolParameterNotSupported;
		}
		memcpy(&lp->Bprotocol.B2cfg[0], cmsg->B2configuration, cmsg->B2configuration[0] + 1);
	} else
		lp->Bprotocol.B2cfg[0] = 0;
	if (cmsg->B3configuration && cmsg->B3configuration[0]) {
		if (cmsg->B3configuration[0] > 79) {
			wprint("B3cfg too large(%d)\n", cmsg->B3configuration[0]);
			return CapiB3ProtocolParameterNotSupported;
		}
		memcpy(&lp->Bprotocol.B3cfg[0], cmsg->B3configuration, cmsg->B3configuration[0] + 1);
	} else
		lp->Bprotocol.B3cfg[0] = 0;
	if (lp->Bprotocol.B1 == 4) {
		if (lp->Bprotocol.B2 != 4 || ((lp->Bprotocol.B3 != 4 && lp->Bprotocol.B3 != 5))) {
			wprint("B1 Fax but B2(%d) or B3(%d) not\n", lp->Bprotocol.B2, lp->Bprotocol.B3);
			return CapiProtocolCombinationNotSupported;
		}
		/* valid Fax combination */
		if (lp->Bprotocol.B3cfg[0]) {
			val = CAPIMSG_U16(lp->Bprotocol.B3cfg, 3);
			switch (val) {
			case FAX_B3_FORMAT_SFF:
			case FAX_B3_FORMAT_TIFF:
				break;
			default: /* Others not suported yet */
				wprint("B3cfg Fax format %d not supported\n", val);
				return CapiB3ProtocolParameterNotSupported;
			}
		}
		if (lp->Bprotocol.B1cfg[0]) {
			val = CAPIMSG_U16(lp->Bprotocol.B3cfg, 1);
			switch (val) {
			case 0: /* Adaptive */
			case 4800:
			case 7200:
			case 9600:
			case 12000:
			case 14400:
				break;
			default: /* Others not suported */
				wprint("B1cfg Fax bitrate %d not supported\n", val);
				return CapiB1ProtocolParameterNotSupported;
			}
		}
	}
	return 0;
}

/*
 * return  1 channel was set modified now
 * return  0 channel was set but not modified by this message
 * return -1 channel is not set or not a physical channel (e.g ANY)
 * return -2 decoding error
 */
static int plci_parse_channel_id(struct lPLCI *lp, struct mc_buf *mc)
{
	int ret;

	if (mc) {
		ret = mi_decode_channel_id(mc->l3m, &lp->chid);
		if (ret < 0) {
			wprint("Channel ID IE decoding error - %s\n", strerror(-ret));
		}
		return -2;
	}
	if (lp->chid.ctrl & MI_CHAN_CTRL_UPDATED) {
		ret = 1;
		lp->chid.ctrl &= ~MI_CHAN_CTRL_UPDATED;
	} else
		ret = 0;

	if (lp->chid.nr == MI_CHAN_NONE || lp->chid.nr == MI_CHAN_ANY)
		ret = -1;

	return ret;
}

static void plci_connect_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;
	struct mc_buf *mc = arg;
	struct l3_msg *l3m;
	uint16_t Info = 0;

	FsmChangeState(fi, ST_PLCI_P_0_1);
	plci->outgoing = 1;

	l3m = alloc_l3_msg();
	if (!l3m) {
		Info = CapiNoPlciAvailable;
		goto answer;
	}
	if ((Info = cmsg2setup_req(&mc->cmsg, l3m))) {
		goto answer;
	}
	if ((Info = lPLCICheckBprotocol(lp, &mc->cmsg))) {
		goto answer;
	}

	plci->pid = request_new_pid(plci->pc->l3);
	plciL4L3(plci, MT_SETUP, l3m);
answer:
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = Info;
	if (mc->cmsg.Info == 0)
		mc->cmsg.adr.adrPLCI = lp->plci;
	FsmEvent(fi, EV_PI_CONNECT_CONF, mc);
}

static void plci_connect_conf(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	if (mc->cmsg.Info == 0) {
		Send2Application(lp, mc);
		FsmChangeState(fi, ST_PLCI_P_1);
	} else {
		Send2Application(lp, mc);
		FsmChangeState(fi, ST_PLCI_P_0);
		lPLCI_free(lp);
	}
}

static void plci_connect_ind(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_PLCI_P_2);
	Send2Application(fi->userdata, arg);
}

static void plci_hold_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;

	plciL4L3(plci, MT_HOLD, arg);
}

static void plci_retrieve_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;

	plciL4L3(plci, MT_RETRIEVE, arg);
}

static void plci_suspend_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;

	plciL4L3(plci, MT_SUSPEND, arg);
}

static void plci_resume_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;

	// we already sent CONF with Info = SuppInfo = 0
	FsmChangeState(fi, ST_PLCI_P_RES);
	plci->pid = request_new_pid(plci->pc->l3);
	plciL4L3(plci, MT_RESUME, arg);
}

static void plci_alert_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;
	struct mc_buf *mc = arg;
	uint16_t Info = 0;

	if (plci->alerting) {
		Info = 0x0003;	// other app is already alerting
	} else {
		if (!mc->l3m)
			mc->l3m = alloc_l3_msg();
		if (!mc->l3m) {
			wprint("alerting not send no l3m\n");
			goto answer;
		}
		Info = cmsg2alerting_req(&mc->cmsg, mc->l3m);
		if (Info == 0) {
			plciL4L3(plci, MT_ALERTING, mc->l3m);
			plci->alerting = 1;
		}
	}
answer:
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = Info;
	Send2Application(lp, mc);
}

static void plci_connect_resp(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;
	struct mc_buf *mc = arg;
	struct l3_msg *l3m;
	int cause;

	if (mc->cmsg.Reject == 0) {	// accept
		if (lPLCICheckBprotocol(lp, &mc->cmsg)) {
			wprint("Bprotocol mismatch\n");
		}
		lPLCIClearOtherApps(lp);
		l3m = alloc_l3_msg();
		/* TODO connect number, sub,llc, addie... */
		plciL4L3(plci, MT_CONNECT, l3m);
		FsmChangeState(fi, ST_PLCI_P_4);
		return;
	}
	// ignore, reject 
	switch (mc->cmsg.Reject) {
	case 1:
		break;		// Ignore - no message should be sent
	case 2:
		cause = 0x90;
		break;		// normal call clearing
	case 3:
		cause = 0x91;
		break;		// user busy
	case 4:
		cause = 0xac;
		break;		// req circuit/channel not avail
	case 5:
		cause = 0x9d;
		break;		// fac rejected
	case 6:
		cause = 0x86;
		break;		// channel unacceptable
	case 7:
		cause = 0xd8;
		break;		// incompatible dest
	case 8:
		cause = 0x9b;
		break;		// dest out of order
	default:
		if ((mc->cmsg.Reject & 0xff00) == 0x3400) {
			cause = mc->cmsg.Reject & 0xff;
		} else {
			cause = 0x90;	// normal call clearing
		}
	}
	// FIXME
	// WHY ???
	// if (cmsg->Reject != 1) {
	// ignore
	//      lPLCIClearOtherApps(lp);
	// }
	// plciDetachlPLCI(plci, lp);
	if (mc->cmsg.Reject != 1) {
		if (plci->nAppl == 1) {
			int mt;

			l3m = alloc_l3_msg();
			if (!l3m) {
				wprint("disconnect not send no l3m\n");
			} else {
				if (plci->alerting)
					mt = MT_DISCONNECT;
				else
					mt = MT_RELEASE_COMPLETE;
				mi_encode_cause(l3m, cause, CAUSE_LOC_USER, 0, NULL);
				plciL4L3(plci, mt, l3m);
			}
		}
	}
	mc->cmsg.Command = CAPI_DISCONNECT;
	mc->cmsg.Subcommand = CAPI_IND;
	mc->cmsg.Messagenumber = lp->lc->Appl->MsgId++;
	mc->cmsg.Reason = 0;
	FsmEvent(&lp->plci_m, EV_PI_DISCONNECT_IND, mc);
}

static void plci_connect_active_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;

	FsmChangeState(fi, ST_PLCI_P_ACT);
	lPLCILinkUp(lp);
	Send2Application(lp, arg);
}

static void plci_connect_active_resp(struct FsmInst *fi, int event, void *arg)
{
}

static void plci_disconnect_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;
	struct mc_buf *mc = arg;
	struct l3_msg *l3m;
	int cause;

	FsmChangeState(fi, ST_PLCI_P_5);

	// FIXME handle additional Info
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Reason = 0;	// disconnect initiated
	Send2Application(lp, mc);

	lPLCILinkDown(lp);

	lp->disc_req = 1;
	if (lp->cause == 0) {
		dprint(MIDEBUG_PLCI, "Disconnect send to card\n");
		l3m = alloc_l3_msg();
		if (!l3m) {
			wprint("disconnect not send no l3m\n");
		} else {
			cause = CAUSE_NORMAL_CLEARING;
			mi_encode_cause(l3m, cause, CAUSE_LOC_USER, 0, NULL);
			plciL4L3(plci, MT_DISCONNECT, l3m);
		}
	} else {
		/* release physical link */
		// FIXME
		dprint(MIDEBUG_PLCI, "Connection was disconnected with cause %02x - send RELEASE\n", lp->cause);
		plciL4L3(plci, MT_RELEASE, NULL);
	}
}

static void plci_suspend_conf(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_PLCI_P_5);
}

static void plci_resume_conf(struct FsmInst *fi, int event, void *arg)
{
	// facility_ind Resume: Reason = 0
	struct lPLCI *lp = fi->userdata;

	FsmChangeState(fi, ST_PLCI_P_ACT);
	lPLCILinkUp(lp);
	Send2Application(lp, arg);
}

static void plci_disconnect_ind(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_PLCI_P_6);
	Send2Application(fi->userdata, arg);
}

static void plci_disconnect_resp(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_PLCI_P_0);
	lPLCI_free(fi->userdata);
}

static void plci_appl_release(struct FsmInst *fi, int event, void *arg)
{
	lPLCI_free(fi->userdata);
}

static void plci_appl_release_disc(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mPLCI *plci = lp->PLCI;
	int cause;
	struct l3_msg *l3m;

	FsmChangeState(fi, ST_PLCI_P_5);

	lPLCILinkDown(lp);

	if (lp->cause == 0) {
		l3m = alloc_l3_msg();
		if (!l3m) {
			wprint("disconnect not send no l3m\n");
		} else {
			cause = CAUSE_NORMALUNSPECIFIED;
			mi_encode_cause(l3m, cause, CAUSE_LOC_USER, 0, NULL);
			plciL4L3(plci, MT_DISCONNECT, l3m);
		}
	} else {
		/* release physical link */
		// FIXME
		plciL4L3(plci, MT_RELEASE, NULL);
	}
}

static void plci_cc_setup_conf(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	if (lp->chid.nr == MI_CHAN_NONE || lp->chid.nr == MI_CHAN_ANY) {
		/* no valid channel set */
		FsmEvent(fi, EV_PI_CHANNEL_ERR, mc);
		return;
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_CONNECT_ACTIVE, CAPI_IND);
	if (mc->l3m) {
		if (mc->l3m->connected_nr && *mc->l3m->connected_nr)
			mc->cmsg.ConnectedNumber = mc->l3m->connected_nr;
		if (mc->l3m->connected_sub && *mc->l3m->connected_sub)
			mc->cmsg.ConnectedSubaddress = mc->l3m->connected_sub;
		if (mc->l3m->llc && *mc->l3m->llc)
			mc->cmsg.LLC = mc->l3m->llc;
	}
	FsmEvent(fi, EV_PI_CONNECT_ACTIVE_IND, mc);
}

static void plci_cc_setup_conf_err(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_DISCONNECT, CAPI_IND);
	mc->cmsg.Reason = CapiProtocolErrorLayer3;
	FsmEvent(&lp->plci_m, EV_PI_DISCONNECT_IND, mc);
	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_channel_err(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	int cause;
	struct l3_msg *l3m;
	struct mc_buf *mc = arg;

	l3m = alloc_l3_msg();
	if (l3m) {
		cause = CAUSE_CHANNEL_UNACCEPT;
		mi_encode_cause(l3m, cause, CAUSE_LOC_USER, 0, NULL);
		plciL4L3(lp->PLCI, MT_RELEASE_COMPLETE, l3m);
	} else
		eprint("no l3_msg\n");
	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_DISCONNECT, CAPI_IND);
	mc->cmsg.Reason = CapiProtocolErrorLayer3;
	FsmEvent(&lp->plci_m, EV_PI_DISCONNECT_IND, mc);
	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_setup_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_CONNECT, CAPI_IND);

	// FIXME: CW
	if (mc->l3m) {
		mc->cmsg.CIPValue = q931CIPValue(mc);
		if (mc->l3m->called_nr && *mc->l3m->called_nr)
			mc->cmsg.CalledPartyNumber = mc->l3m->called_nr;
		if (mc->l3m->called_sub && *mc->l3m->called_sub)
			mc->cmsg.CalledPartySubaddress = mc->l3m->called_sub;
		if (mc->l3m->calling_nr && *mc->l3m->calling_nr)
			mc->cmsg.CallingPartyNumber = mc->l3m->calling_nr;
		if (mc->l3m->calling_sub && *mc->l3m->calling_sub)
			mc->cmsg.CallingPartySubaddress = mc->l3m->calling_sub;
		if (mc->l3m->bearer_capability && *mc->l3m->bearer_capability)
			mc->cmsg.BC = mc->l3m->bearer_capability;
		if (mc->l3m->llc && *mc->l3m->llc)
			mc->cmsg.LLC = mc->l3m->llc;
		if (mc->l3m->hlc && *mc->l3m->hlc)
			mc->cmsg.HLC = mc->l3m->hlc;
#if CAPIUTIL_VERSION > 1
		{
			struct m_extie *eie;
			int i;
			/* ETS 300 092 Annex B */
			eie = mc->l3m->extra;
			for (i = 0; i < 8; i++) {
				if (!eie->ie)	/* stop if no additional ie */
					break;
				if (eie->ie == IE_CALLING_PN && eie->codeset == 0) {
					if (eie->val && *eie->val)
						mc->cmsg.CallingPartyNumber2 = eie->val;
					break;
				}
				eie++;
			}
		}
#endif
		// all else set to default
	}
	FsmEvent(&lp->plci_m, EV_PI_CONNECT_IND, mc);
}

static void plci_cc_setup_compl_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_CONNECT_ACTIVE, CAPI_IND);
	FsmEvent(&lp->plci_m, EV_PI_CONNECT_ACTIVE_IND, mc);
	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_disconnect_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	int ret;

	lp->cause = -1;
	if (mc->l3m) {
		ret = mi_decode_cause(mc->l3m, NULL, &lp->cause_loc, NULL, &lp->cause, NULL, NULL);
		if (ret) {
			wprint("Error decoding cause %d - %s\n", ret, strerror(-ret));
		} else
			iprint("PLCI %04x: Got Disconnect with cause %02d (0x%02x) loc %02x\n",
			       lp->plci, lp->cause, lp->cause, lp->cause_loc);
	} else
		iprint("PLCI %04x: Got Disconnect without cause info\n", lp->plci);

	if (lp->autohangup) {
		if (lp->lc->InfoMask & CAPI_INFOMASK_EARLYB3)
			return;

		lPLCILinkDown(lp);
		plciL4L3(lp->PLCI, MT_RELEASE, NULL);
	}
}

static void plci_cc_release_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	int cause = -1, loc = -1, ret;

	lPLCILinkDown(lp);
	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_DISCONNECT, CAPI_IND);
	if (mc->l3m) {
		if (mc->l3m->cause && *mc->l3m->cause) {
			ret = mi_decode_cause(mc->l3m, NULL, &loc, NULL, &cause, NULL, NULL);
			if (ret) {
				wprint("Error decoding cause %d - %s\n", ret, strerror(-ret));
			} else {
				iprint("Release with cause %d loc %d main cause %d\n", cause, loc, lp->cause);
			}
		}
	}
	if (!lp->disc_req) {
		/* We only should send causes from RELEASE when we did not request the RELEASE 
		 * Some applications will see Reasons not 00 as error - maybe thats a bug
		 */
		if (lp->cause > 0)
			cause = lp->cause;
		if (cause > 0)
			mc->cmsg.Reason = 0x3400 | (cause & 0x7f);
		else
			mc->cmsg.Reason = 0;
	} else
		mc->cmsg.Reason = 0;
	FsmEvent(&lp->plci_m, EV_PI_DISCONNECT_IND, mc);
	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_notify_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	if (!mc || !mc->l3m || !mc->l3m->notify)
		return;
	if (mc->l3m->notify[0] != 1)	// len != 1
		return;
	switch (mc->l3m->notify[1]) {
	case 0xF9:		// user hold
		SendSSNotificationEvent(lp, 0x8000);
		break;
	case 0xFA:		// user retrieve
		SendSSNotificationEvent(lp, 0x8001);
		break;
	case 0x80:		// user suspended
		SendSSNotificationEvent(lp, 0x8002);
		break;
	case 0x81:		// user resumed
		SendSSNotificationEvent(lp, 0x8003);
		break;
	case 0xFB:		// call is diverting
		SendSSNotificationEvent(lp, 0x8004);
		break;
	case 0xE8:		// diversion activated
		SendSSNotificationEvent(lp, 0x8005);
		break;
	default:
		eprint("unhandled notification %x\n", mc->l3m->notify[1]);
	}
}

static void plci_hold_conf(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_PLCI_P_HELD);
}

static void lPLCI_hold_reply(struct lPLCI *lp, uint16_t SuppServiceReason, void *arg)
{
	unsigned char tmp[10], *p;
	struct mc_buf *mc = arg;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_FACILITY, CAPI_IND);
	p = &tmp[1];
	p += capiEncodeWord(p, 0x0002);	// Hold
	p += capiEncodeFacIndSuspend(p, SuppServiceReason);
	tmp[0] = p - &tmp[1];
	mc->cmsg.FacilitySelector = 0x0003;
	mc->cmsg.FacilityIndicationParameter = tmp;
	Send2Application(lp, mc);
	if (SuppServiceReason == CapiNoError)
		FsmEvent(&lp->plci_m, EV_PI_HOLD_CONF, NULL);
	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_hold_rej(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	uint16_t SuppServiceReason;
	struct mc_buf *mc = arg;
	int cause = 0, ret;

	if (mc && mc->l3m) {
		if (mc->l3m->cause && *mc->l3m->cause) {
			ret = mi_decode_cause(mc->l3m, NULL, NULL, NULL, &cause, NULL, NULL);
			if (ret)
				wprint("Error decoding cause %d - %s\n", ret, strerror(-ret));
			SuppServiceReason = 0x3400 | (cause & 0x7f);
		} else
			SuppServiceReason = CapiProtocolErrorLayer3;
	} else {		// timeout
		SuppServiceReason = CapiProtocolErrorLayer3;
	}
	lPLCI_hold_reply(lp, SuppServiceReason, arg);
}

static void plci_cc_hold_ack(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;

	lPLCI_hold_reply(lp, CapiNoError, arg);
	lPLCILinkDown(lp);
}

static void plci_cc_hold_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;

	lPLCI_hold_reply(lp, CapiNoError, arg);
	lPLCILinkDown(lp);
	plciL4L3(lp->PLCI, MT_HOLD_ACKNOWLEDGE, NULL);
}

static void plci_retrieve_conf(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;

	FsmChangeState(fi, ST_PLCI_P_ACT);
	lPLCILinkUp(lp);
	Send2Application(lp, arg);
}

static void lPLCI_retrieve_reply(struct lPLCI *lp, uint16_t SuppServiceReason, void *arg)
{
	unsigned char tmp[10], *p;
	struct mc_buf *mc = arg;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_FACILITY, CAPI_IND);
	p = &tmp[1];
	p += capiEncodeWord(p, 0x0003);	// Retrieve
	p += capiEncodeFacIndSuspend(p, SuppServiceReason);
	tmp[0] = p - &tmp[1];
	mc->cmsg.FacilitySelector = 0x0003;
	mc->cmsg.FacilityIndicationParameter = tmp;

	if (SuppServiceReason != CapiNoError)
		Send2Application(lp, mc);
	else
		FsmEvent(&lp->plci_m, EV_PI_RETRIEVE_CONF, mc);

	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_retrieve_rej(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	uint16_t SuppServiceReason;
	struct mc_buf *mc = arg;
	int cause = 0, ret;

	if (mc && mc->l3m) {
		if (mc->l3m->cause && *mc->l3m->cause) {
			ret = mi_decode_cause(mc->l3m, NULL, NULL, NULL, &cause, NULL, NULL);
			if (ret)
				wprint("Error decoding cause %d - %s\n", ret, strerror(-ret));
			SuppServiceReason = 0x3400 | (cause & 0x7f);
		} else
			SuppServiceReason = CapiProtocolErrorLayer3;
	} else {		// timeout
		SuppServiceReason = CapiProtocolErrorLayer3;
	}
	lPLCI_retrieve_reply(lp, SuppServiceReason, arg);
}

static void plci_cc_retrieve_ack(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	int ret;

	ret = plci_parse_channel_id(lp, mc);
	if (ret >= 0) {
		lPLCI_retrieve_reply(lp, CapiNoError, arg);
	} else {
		wprint("No valid channel for retrieve (%d)\n", ret);
		lPLCI_retrieve_reply(lp, 0x3711, arg);	/* resource Error */
	}
}

static void plci_cc_retrieve_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	int mt;
	int ret;

	ret = plci_parse_channel_id(lp, mc);
	if (ret >= 0) {
		lPLCI_retrieve_reply(lp, CapiNoError, arg);
		mt = MT_RETRIEVE_ACKNOWLEDGE;
	} else {
		wprint("No valid channel for retrieve (%d)\n", ret);
		lPLCI_retrieve_reply(lp, 0x3711, arg);	/* resource Error */
		mt = MT_RETRIEVE_REJECT;
	}
	plciL4L3(lp->PLCI, mt, NULL);
}

static void lPLCI_suspend_reply(struct lPLCI *lp, uint16_t SuppServiceReason, void *arg)
{
	unsigned char tmp[10], *p;
	struct mc_buf *mc = arg;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_FACILITY, CAPI_IND);
	p = &tmp[1];
	p += capiEncodeWord(p, 0x0004);	// Suspend
	p += capiEncodeFacIndSuspend(p, SuppServiceReason);
	tmp[0] = p - &tmp[1];
	mc->cmsg.FacilitySelector = 0x0003;
	mc->cmsg.FacilityIndicationParameter = tmp;
	Send2Application(lp, mc);

	if (SuppServiceReason == CapiNoError)
		FsmEvent(&lp->plci_m, EV_PI_SUSPEND_CONF, NULL);

	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_suspend_err(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	uint16_t SuppServiceReason;
	struct mc_buf *mc = arg;
	int cause = 0, ret;

	if (mc && mc->l3m) {
		if (mc->l3m->cause && *mc->l3m->cause) {
			ret = mi_decode_cause(mc->l3m, NULL, NULL, NULL, &cause, NULL, NULL);
			if (ret)
				wprint("Error decoding cause %d - %s\n", ret, strerror(-ret));
			SuppServiceReason = 0x3400 | (cause & 0x7f);
		} else
			SuppServiceReason = CapiProtocolErrorLayer3;
	} else {		// timeout
		SuppServiceReason = CapiProtocolErrorLayer3;
	}
	lPLCI_suspend_reply(lp, SuppServiceReason, arg);
}

static void plci_cc_suspend_conf(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCILinkDown(lp);

	lPLCI_suspend_reply(lp, CapiNoError, arg);

	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_DISCONNECT, CAPI_IND);
	FsmEvent(&lp->plci_m, EV_PI_DISCONNECT_IND, mc);

	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_resume_err(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	int cause = 0, ret;

	if (!mc) {
		mc = alloc_mc_buf();
		if (!mc) {
			eprint("No mc buffers\n");
			return;
		}
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_DISCONNECT, CAPI_IND);
	if (mc->l3m) {
		if (mc->l3m->cause && *mc->l3m->cause) {
			ret = mi_decode_cause(mc->l3m, NULL, NULL, NULL, &cause, NULL, NULL);
			if (ret)
				wprint("Error decoding cause %d - %s\n", ret, strerror(-ret));
			mc->cmsg.Reason = 0x3400 | (cause & 0x7f);
		} else
			mc->cmsg.Reason = 0;
	} else			// timeout
		mc->cmsg.Reason = CapiProtocolErrorLayer1;

	FsmEvent(&lp->plci_m, EV_PI_DISCONNECT_IND, mc);
	if (!arg)		/* if allocated local */
		free_mc_buf(mc);
}

static void plci_cc_resume_conf(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	unsigned char tmp[10], *p;
	int ret;

	ret = plci_parse_channel_id(lp, mc);
	if (ret < 0) {
		wprint("No valid channel for resume (%d)\n", ret);
		return;
	}
	lPLCICmsgHeader(lp, &mc->cmsg, CAPI_FACILITY, CAPI_IND);
	p = &tmp[1];
	p += capiEncodeWord(p, 0x0005);	// Suspend
	p += capiEncodeFacIndSuspend(p, CapiNoError);
	tmp[0] = p - &tmp[1];
	mc->cmsg.FacilitySelector = 0x0003;
	mc->cmsg.FacilityIndicationParameter = tmp;
	FsmEvent(&lp->plci_m, EV_PI_RESUME_CONF, mc);
}

static void plci_select_b_protocol_req(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	uint16_t Info;
	int ret;

	Info = lPLCICheckBprotocol(lp, &mc->cmsg);
	if (Info)
		goto answer;

	ret = lPLCILinkDown(lp);
	if (ret) {
		Info = CapiMessageNotSupportedInCurrentState;
		goto answer;
	}
	ret = lPLCILinkUp(lp);
	if (ret < 0)
		Info = CapiMessageNotSupportedInCurrentState;
	else
		Info = ret;
answer:
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = Info;
	Send2Application(lp, arg);
}

static void plci_info_req_overlap(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	uint16_t Info = 0;
	struct l3_msg *l3m;

	l3m = alloc_l3_msg();
	if (l3m) {
		Info = cmsg2info_req(&mc->cmsg, l3m);
		if (Info == CapiNoError)
			plciL4L3(lp->PLCI, MT_INFORMATION, l3m);
		else
			free_l3_msg(l3m);
	}
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = Info;
	Send2Application(lp, mc);
}

static void plci_cc_ph_control_ind(struct FsmInst *fi, int event, void *arg)
{
	struct lPLCI *lp = fi->userdata;
	struct mc_buf *mc = arg;
	unsigned int *tt;
	__u8 tmp[2];

	tt = (unsigned int *)(mc->rb + sizeof(struct mISDNhead));
	dprint(MIDEBUG_PLCI, "PLCI:%04x tt(%x)\n", lp->plci, *tt);
	if ((*tt & ~DTMF_TONE_MASK) != DTMF_TONE_VAL) {
		wprint("PLCI:%04x PH_CONTROL but not a touchtone (%x) ?\n", lp->plci, *tt);
	} else {
		lPLCICmsgHeader(lp, &mc->cmsg, CAPI_FACILITY, CAPI_IND);
		tmp[0] = 1;
		tmp[1] = *tt & DTMF_TONE_MASK;
		mc->cmsg.FacilitySelector = 0x0001;
		mc->cmsg.FacilityIndicationParameter = tmp;
		Send2Application(lp, mc);
	}
}

static void plci_info_req(struct FsmInst *fi, int event, void *arg)
{
	// FIXME handle INFO CONF
}

static struct FsmNode fn_plci_list[] = {
	{ST_PLCI_P_0, EV_AP_CONNECT_REQ, plci_connect_req},
	{ST_PLCI_P_0, EV_PI_CONNECT_IND, plci_connect_ind},
	{ST_PLCI_P_0, EV_AP_RESUME_REQ, plci_resume_req},
	{ST_PLCI_P_0, EV_L3_SETUP_IND, plci_cc_setup_ind},
	{ST_PLCI_P_0, EV_AP_RELEASE, plci_appl_release},

	{ST_PLCI_P_0_1, EV_PI_CONNECT_CONF, plci_connect_conf},
	{ST_PLCI_P_0_1, EV_AP_RELEASE, plci_appl_release},

	{ST_PLCI_P_1, EV_PI_CONNECT_ACTIVE_IND, plci_connect_active_ind},
	{ST_PLCI_P_1, EV_AP_DISCONNECT_REQ, plci_disconnect_req},
	{ST_PLCI_P_1, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_1, EV_AP_INFO_REQ, plci_info_req_overlap},
	{ST_PLCI_P_1, EV_L3_SETUP_CONF, plci_cc_setup_conf},
	{ST_PLCI_P_1, EV_L3_SETUP_CONF_ERR, plci_cc_setup_conf_err},
	{ST_PLCI_P_1, EV_L3_DISCONNECT_IND, plci_cc_disconnect_ind},
	{ST_PLCI_P_1, EV_L3_RELEASE_PROC_IND, plci_cc_setup_conf_err},
	{ST_PLCI_P_1, EV_L3_RELEASE_IND, plci_cc_release_ind},
	{ST_PLCI_P_1, EV_L3_REJECT_IND, plci_cc_release_ind},
	{ST_PLCI_P_1, EV_PI_CHANNEL_ERR, plci_channel_err},
	{ST_PLCI_P_1, EV_AP_RELEASE, plci_appl_release_disc},

	{ST_PLCI_P_2, EV_AP_ALERT_REQ, plci_alert_req},
	{ST_PLCI_P_2, EV_AP_CONNECT_RESP, plci_connect_resp},
	{ST_PLCI_P_2, EV_AP_DISCONNECT_REQ, plci_disconnect_req},
	{ST_PLCI_P_2, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_2, EV_L3_RELEASE_PROC_IND, plci_cc_release_ind},
	{ST_PLCI_P_2, EV_AP_INFO_REQ, plci_info_req},
	{ST_PLCI_P_2, EV_L3_RELEASE_IND, plci_cc_release_ind},
	{ST_PLCI_P_2, EV_AP_RELEASE, plci_appl_release_disc},

	{ST_PLCI_P_4, EV_PI_CONNECT_ACTIVE_IND, plci_connect_active_ind},
	{ST_PLCI_P_4, EV_AP_DISCONNECT_REQ, plci_disconnect_req},
	{ST_PLCI_P_4, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_4, EV_AP_INFO_REQ, plci_info_req},
	{ST_PLCI_P_4, EV_L3_SETUP_COMPL_IND, plci_cc_setup_compl_ind},
	{ST_PLCI_P_4, EV_L3_RELEASE_IND, plci_cc_release_ind},
	{ST_PLCI_P_4, EV_L3_RELEASE_PROC_IND, plci_cc_release_ind},
	{ST_PLCI_P_4, EV_PI_CHANNEL_ERR, plci_channel_err},
	{ST_PLCI_P_4, EV_AP_RELEASE, plci_appl_release_disc},

	{ST_PLCI_P_ACT, EV_AP_CONNECT_ACTIVE_RESP, plci_connect_active_resp},
	{ST_PLCI_P_ACT, EV_AP_DISCONNECT_REQ, plci_disconnect_req},
	{ST_PLCI_P_ACT, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_ACT, EV_AP_INFO_REQ, plci_info_req},
	{ST_PLCI_P_ACT, EV_AP_SELECT_B_PROTOCOL_REQ, plci_select_b_protocol_req},
	{ST_PLCI_P_ACT, EV_AP_HOLD_REQ, plci_hold_req},
	{ST_PLCI_P_ACT, EV_AP_SUSPEND_REQ, plci_suspend_req},
	{ST_PLCI_P_ACT, EV_PI_SUSPEND_CONF, plci_suspend_conf},
	{ST_PLCI_P_ACT, EV_L3_DISCONNECT_IND, plci_cc_disconnect_ind},
	{ST_PLCI_P_ACT, EV_L3_RELEASE_IND, plci_cc_release_ind},
	{ST_PLCI_P_ACT, EV_L3_RELEASE_PROC_IND, plci_cc_release_ind},
	{ST_PLCI_P_ACT, EV_L3_NOTIFY_IND, plci_cc_notify_ind},
	{ST_PLCI_P_ACT, EV_L3_HOLD_IND, plci_cc_hold_ind},
	{ST_PLCI_P_ACT, EV_L3_HOLD_ACKNOWLEDGE, plci_cc_hold_ack},
	{ST_PLCI_P_ACT, EV_L3_HOLD_REJECT, plci_cc_hold_rej},
	{ST_PLCI_P_ACT, EV_PI_HOLD_CONF, plci_hold_conf},
	{ST_PLCI_P_ACT, EV_L3_SUSPEND_ERR, plci_cc_suspend_err},
	{ST_PLCI_P_ACT, EV_L3_SUSPEND_CONF, plci_cc_suspend_conf},
	{ST_PLCI_P_ACT, EV_PH_CONTROL_IND, plci_cc_ph_control_ind},
	{ST_PLCI_P_ACT, EV_AP_RELEASE, plci_appl_release_disc},

	{ST_PLCI_P_HELD, EV_AP_RETRIEVE_REQ, plci_retrieve_req},
	{ST_PLCI_P_HELD, EV_L3_RETRIEVE_ACKNOWLEDGE, plci_cc_retrieve_ack},
	{ST_PLCI_P_HELD, EV_L3_RETRIEVE_REJECT, plci_cc_retrieve_rej},
	{ST_PLCI_P_HELD, EV_PI_RETRIEVE_CONF, plci_retrieve_conf},
	{ST_PLCI_P_HELD, EV_AP_DISCONNECT_REQ, plci_disconnect_req},
	{ST_PLCI_P_HELD, EV_AP_INFO_REQ, plci_info_req},
	{ST_PLCI_P_HELD, EV_L3_RETRIEVE_IND, plci_cc_retrieve_ind},
	{ST_PLCI_P_HELD, EV_L3_DISCONNECT_IND, plci_cc_disconnect_ind},
	{ST_PLCI_P_HELD, EV_L3_RELEASE_IND, plci_cc_release_ind},
	{ST_PLCI_P_HELD, EV_L3_RELEASE_PROC_IND, plci_cc_release_ind},
	{ST_PLCI_P_HELD, EV_L3_NOTIFY_IND, plci_cc_notify_ind},
	{ST_PLCI_P_HELD, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_HELD, EV_AP_RELEASE, plci_appl_release_disc},

	{ST_PLCI_P_5, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_5, EV_L3_RELEASE_IND, plci_cc_release_ind},
	{ST_PLCI_P_5, EV_L3_RELEASE_PROC_IND, plci_cc_release_ind},
	{ST_PLCI_P_5, EV_AP_RELEASE, plci_appl_release},

	{ST_PLCI_P_6, EV_AP_DISCONNECT_RESP, plci_disconnect_resp},
	{ST_PLCI_P_6, EV_AP_RELEASE, plci_disconnect_resp},

	{ST_PLCI_P_RES, EV_PI_RESUME_CONF, plci_resume_conf},
	{ST_PLCI_P_RES, EV_PI_DISCONNECT_IND, plci_disconnect_ind},
	{ST_PLCI_P_RES, EV_L3_RESUME_ERR, plci_cc_resume_err},
	{ST_PLCI_P_RES, EV_L3_RESUME_CONF, plci_cc_resume_conf},
	{ST_PLCI_P_RES, EV_AP_RELEASE, plci_appl_release_disc},
};

const int FN_PLCI_COUNT = sizeof(fn_plci_list) / sizeof(struct FsmNode);

int lPLCICreate(struct lPLCI **lpp, struct lController *lc, struct mPLCI *plci)
{
	struct lPLCI *lp;

	lp = calloc(1, sizeof(*lp));
	if (!lp)
		return -ENOMEM;
	lp->plci = plci->plci;
	lp->lc = lc;
	lp->PLCI = plci;
	if (lc->Contr->profile.goptions & 0x0008) {
		/* DTMF */
		lp->l1dtmf = 0;
	}
	lp->plci_m.fsm = &plci_fsm;
	lp->plci_m.state = ST_PLCI_P_0;
//      lp->plci_m.debug      = plci->contr->debug & CAPI_DBG_PLCI_STATE;
	lp->plci_m.debug = MIDEBUG_PLCI & mI_debug_mask;
	lp->plci_m.userdata = lp;
	lp->plci_m.printdebug = lPLCI_debug;
	lp->chid.nr = MI_CHAN_NONE;
	lp->autohangup = 1;
	*lpp = lp;
	return 0;
}

void lPLCI_free(struct lPLCI *lp)
{
	struct mNCCI *nc, *nn;

	if (lp->BIlink)
		CloseBInstance(lp->BIlink);
	if (lp->PLCI) {
		dprint(MIDEBUG_PLCI, "PLCI:%04x plci state %s\n", lp->plci, str_st_plci[lp->plci_m.state]);
		if (lp->plci_m.state != ST_PLCI_P_0) {
			struct l3_msg *l3m = alloc_l3_msg();

			if (l3m) {
				mi_encode_cause(l3m, CAUSE_RESOURCES_UNAVAIL, CAUSE_LOC_USER, 0, NULL);
				plciL4L3(lp->PLCI, MT_RELEASE_COMPLETE, l3m);
			} else
				eprint("No l3m for RELEASE_COMPLETE for PLCI:%04x\n", lp->plci);
		}
		plciDetachlPLCI(lp);
	}
	nc = lp->Nccis;
	while (nc) {
		ncciDel_lPlci(nc);
		nn = nc->next;
		nc->next = NULL;
		nc = nn;
	}
	free(lp);
}

void lPLCIRelease(struct lPLCI *lp)
{
	/* TODO clean NCCIs */
	FsmEvent(&lp->plci_m, EV_AP_RELEASE, NULL);
}

static int lPLCILinkUp(struct lPLCI *lp)
{
	int proto = -1, ret = 0, act_l1;
	struct mISDNhead mh;
	enum BType btype = BType_None;
	struct mApplication *ap = lp->lc->Appl;

	mh.id = 1;
	mh.prim = 0;
	act_l1 = !lp->PLCI->outgoing;

	if (lp->chid.nr == MI_CHAN_NONE || lp->chid.nr == MI_CHAN_ANY) {
		/* no valid channel set */
		wprint("PLCI:%04x no channel selected (0x%x)\n", lp->plci, lp->chid.nr);
		return -EINVAL;
	}

	switch (lp->Bprotocol.B1) {
	case 0:		/* HDLC */
		proto = ISDN_P_B_HDLC;
		mh.prim = PH_ACTIVATE_REQ;
		break;
	case 1:		/* trans */
		if (lp->l1dtmf) {
			proto = ISDN_P_B_L2DSP;
			mh.prim = DL_ESTABLISH_REQ;
		} else {
			proto = ISDN_P_B_RAW;
			mh.prim = PH_ACTIVATE_REQ;
		}
		break;
#ifdef USE_SOFTFAX
	case 4:
		proto = ISDN_P_B_RAW;
		mh.prim = PH_ACTIVATE_REQ;
		act_l1 = 1;
		lp->autohangup = 0;
		break;
#endif
	default:
		wprint("Unsupported B1 prot %x\n", lp->Bprotocol.B1);
		ret = 0x3001;
		break;
	}

	switch (lp->Bprotocol.B2) {
	case 0:		/* HDLC */
		proto = ISDN_P_B_X75SLP;
		mh.prim = DL_ESTABLISH_REQ;
		break;
	case 1:		/* trans */
#ifdef USE_SOFTFAX
	case 4:
#endif
		break;
	default:
		wprint("Unsupported B2 prot %x\n", lp->Bprotocol.B2);
		ret = 0x3002;
		break;
	}

	switch (lp->Bprotocol.B3) {
	case 0:	/* trans */
		btype = BType_Direct;
		break;
#ifdef USE_SOFTFAX
	case 4:
	case 5:
		btype = BType_Fax;
		break;
#endif
	default:
		wprint("Unsupported B3 prot %x\n", lp->Bprotocol.B3);
		ret = 0x3003;
		break;
	}

	if (ret)
		return ret;

	if (ap->UserFlags & CAPIFLAG_HIGHJACKING) {
		btype = BType_tty;
	}
	dprint(MIDEBUG_PLCI, "lPLCILinkUp B1(%x) B2(%x) B3(%x) ch(%d) proto(%x)\n",
	       lp->Bprotocol.B1, lp->Bprotocol.B2, lp->Bprotocol.B3, lp->chid.nr, proto);

	lp->BIlink = ControllerSelChannel(lp->lc->Contr, lp->chid.nr, proto);
	if (!lp->BIlink) {
		wprint("PLCI:%04x channel %d busy\n", lp->plci, lp->chid.nr);
		return CapiMsgOSResourceErr;
	}
	dprint(MIDEBUG_PLCI, "lPLCILinkUp lp->link(%p)\n", lp->BIlink);
	if (!OpenBInstance(lp->BIlink, lp, btype)) {
		if (act_l1) {
			ret = send(lp->BIlink->fd, &mh, 8, 0);
			if (ret < 0) {
				wprint("Cannot send to Bchannel socket\n");
				ret = CapiMsgOSResourceErr;
			} else {
				dprint(MIDEBUG_NCCI, "Sent %d bytes to controller\n", ret);
			}
		}
	} else {
		wprint("OpenBInstance PLCI:%04x failed\n", lp->plci);
		ret = CapiMsgOSResourceErr;
	}
	return ret;
}

struct mNCCI *getNCCI4addr(struct lPLCI *lp, uint32_t addr, int mode)
{
	struct mNCCI *ncci = NULL;
	int cnt = 0;

	if (!lp)
		return NULL;
	ncci = lp->Nccis;
	while (ncci) {
		cnt++;
		if (ncci->ncci == addr)
			return ncci;
		if (mode == GET_NCCI_ONLY_PLCI) {
			if (ncci->ncci == (addr & 0xffff))
				return ncci;
		}
		ncci = ncci->next;
	}
	if (!cnt)
		return NULL;
	if (mode != GET_NCCI_PLCI)
		return NULL;
	if (1 == cnt) {
		if (!(addr & 0xffff0000))
			return lp->Nccis;
	}
	return NULL;
}

void lPLCIDelNCCI(struct mNCCI *ncci)
{
	struct lPLCI *lp = ncci->lp;

	if (lp->Nccis != ncci) {
		eprint("lPLCI %04x do not cover NCCI %06x\n", lp->plci, ncci->ncci);
		return;
	}
	lp->Nccis = NULL;
	lp->NcciCnt = 0;

	if (ncci->BIlink) {
		ncci->BIlink->b3data = NULL;
		if (ncci->BIlink->fd > 0)
			CloseBInstance(ncci->BIlink);
		ncci->BIlink = NULL;
		lp->BIlink = NULL;
	}
}

void B3ReleaseLink(struct lPLCI *lp, struct BInstance *bi)
{
	switch(bi->type) {
	case BType_Direct:
		ncciReleaseLink(bi->b3data);
		break;
#ifdef USE_SOFTFAX
	case BType_Fax:
		FaxReleaseLink(bi);
		break;
#endif
	default:
		eprint("Unknown BType %d\n", bi->type);
	}
}

void lPLCI_l3l4(struct lPLCI *lp, int pr, struct mc_buf *mc)
{
	int ret;

	dprint(MIDEBUG_PLCI, "lp(%x) %s %s arg\n", lp->plci, _mi_msg_type2str(pr), mc ? "with" : "no");
	switch (pr) {
	case MT_SETUP:
		if (!mc || !mc->l3m)
			return;
		plci_parse_channel_id(lp, mc);
		FsmEvent(&lp->plci_m, EV_L3_SETUP_IND, mc);
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS, mc);
			lPLCIInfoIndIE(lp, IE_FACILITY, CAPI_INFOMASK_FACILITY, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
			lPLCIInfoIndIE(lp, IE_CALLED_PN, CAPI_INFOMASK_CALLEDPN, mc);
			lPLCIInfoIndIE(lp, IE_COMPLETE, CAPI_INFOMASK_COMPLETE, mc);
		}
		break;
	case MT_TIMEOUT:
		FsmEvent(&lp->plci_m, EV_L3_SETUP_CONF_ERR, mc);
		break;
	case MT_CONNECT:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_DATE, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS, mc);
			lPLCIInfoIndIE(lp, IE_FACILITY, CAPI_INFOMASK_FACILITY, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
			ret = plci_parse_channel_id(lp, mc);
			if (ret < 0) {
				dprint(MIDEBUG_PLCI, "Got no valid channel on %s (%d)\n", _mi_msg_type2str(pr), ret);
			}
		}
		FsmEvent(&lp->plci_m, EV_L3_SETUP_CONF, mc);
		break;
	case MT_CONNECT_ACKNOWLEDGE:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
			ret = plci_parse_channel_id(lp, mc);
			if (ret < 0) {
				dprint(MIDEBUG_PLCI, "Got no valid channel on %s (%d)\n", _mi_msg_type2str(pr), ret);
			}
		}
		FsmEvent(&lp->plci_m, EV_L3_SETUP_COMPL_IND, mc);
		break;
	case MT_DISCONNECT:
		if (mc->l3m) {
			lPLCIInfoIndMsg(lp, CAPI_INFOMASK_EARLYB3, MT_DISCONNECT, mc);
			lPLCIInfoIndIE(lp, IE_CAUSE, CAPI_INFOMASK_CAUSE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS, mc);
			lPLCIInfoIndIE(lp, IE_FACILITY, CAPI_INFOMASK_FACILITY, mc);
		}
		FsmEvent(&lp->plci_m, EV_L3_DISCONNECT_IND, mc);
		break;
	case MT_RELEASE:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_CAUSE, CAPI_INFOMASK_CAUSE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_FACILITY, CAPI_INFOMASK_FACILITY, mc);
		}
		FsmEvent(&lp->plci_m, EV_L3_RELEASE_IND, mc);
		break;
	case MT_RELEASE_COMPLETE:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_CAUSE, CAPI_INFOMASK_CAUSE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_FACILITY, CAPI_INFOMASK_FACILITY, mc);
		}
		FsmEvent(&lp->plci_m, EV_L3_RELEASE_IND, mc);
		break;
	case MT_FREE:
		FsmEvent(&lp->plci_m, EV_L3_RELEASE_PROC_IND, mc);
		break;
	case MT_SETUP_ACKNOWLEDGE:
		if (mc->l3m) {
			lPLCIInfoIndMsg(lp, CAPI_INFOMASK_PROGRESS, MT_SETUP_ACKNOWLEDGE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS | CAPI_INFOMASK_EARLYB3, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
			ret = plci_parse_channel_id(lp, mc);
			if (ret < -1) {
				wprint("Got channel coding error in %s (%d)\n", _mi_msg_type2str(pr), ret);
			}
		}
		break;
	case MT_CALL_PROCEEDING:
		if (mc->l3m) {
			lPLCIInfoIndMsg(lp, CAPI_INFOMASK_PROGRESS, MT_CALL_PROCEEDING, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS | CAPI_INFOMASK_EARLYB3, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
		}
		break;
	case MT_ALERTING:
		if (mc->l3m) {
			lPLCIInfoIndMsg(lp, CAPI_INFOMASK_PROGRESS, MT_ALERTING, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS | CAPI_INFOMASK_EARLYB3, mc);
			lPLCIInfoIndIE(lp, IE_FACILITY, CAPI_INFOMASK_FACILITY, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
			ret = plci_parse_channel_id(lp, mc);
			if (ret < -1) {
				wprint("Got channel coding error in %s (%d)\n", _mi_msg_type2str(pr), ret);
			}
		}
		break;
	case MT_PROGRESS:
		if (mc->l3m) {
			lPLCIInfoIndMsg(lp, CAPI_INFOMASK_PROGRESS, MT_PROGRESS, mc);
			lPLCIInfoIndIE(lp, IE_CAUSE, CAPI_INFOMASK_CAUSE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_USER_USER, CAPI_INFOMASK_USERUSER, mc);
			lPLCIInfoIndIE(lp, IE_PROGRESS, CAPI_INFOMASK_PROGRESS | CAPI_INFOMASK_EARLYB3, mc);
		}
		break;
	case MT_HOLD:
		if (mc->l3m)
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
		if (FsmEvent(&lp->plci_m, EV_L3_HOLD_IND, mc)) {
			/* no routine reject L3 */
			plciL4L3(lp->PLCI, MT_HOLD_REJECT, NULL);
		}
		break;
	case MT_HOLD_ACKNOWLEDGE:
		if (mc->l3m)
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
		FsmEvent(&lp->plci_m, EV_L3_HOLD_ACKNOWLEDGE, mc);
		break;
	case MT_HOLD_REJECT:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_CAUSE, CAPI_INFOMASK_CAUSE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
		}
		FsmEvent(&lp->plci_m, EV_L3_HOLD_REJECT, mc);
		break;
	case MT_RETRIEVE:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
		}
		if (FsmEvent(&lp->plci_m, EV_L3_RETRIEVE_IND, mc)) {
			/* no routine reject L3 */
			plciL4L3(lp->PLCI, MT_RETRIEVE_REJECT, NULL);
		}
		break;
	case MT_RETRIEVE_ACKNOWLEDGE:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
			lPLCIInfoIndIE(lp, IE_CHANNEL_ID, CAPI_INFOMASK_CHANNELID, mc);
		}
		FsmEvent(&lp->plci_m, EV_L3_RETRIEVE_ACKNOWLEDGE, mc);
		break;
	case MT_RETRIEVE_REJECT:
		if (mc->l3m) {
			lPLCIInfoIndIE(lp, IE_CAUSE, CAPI_INFOMASK_CAUSE, mc);
			lPLCIInfoIndIE(lp, IE_DISPLAY, CAPI_INFOMASK_DISPLAY, mc);
		}
		FsmEvent(&lp->plci_m, EV_L3_RETRIEVE_REJECT, mc);
		break;
	case MT_SUSPEND_ACKNOWLEDGE:
		FsmEvent(&lp->plci_m, EV_L3_SUSPEND_CONF, mc);
		break;
	case MT_SUSPEND_REJECT:
		FsmEvent(&lp->plci_m, EV_L3_SUSPEND_ERR, mc);
		break;
	case MT_RESUME_ACKNOWLEDGE:
		FsmEvent(&lp->plci_m, EV_L3_RESUME_CONF, mc);
		break;
	case MT_RESUME_REJECT:
		FsmEvent(&lp->plci_m, EV_L3_RESUME_ERR, mc);
		break;
	case MT_NOTIFY:
		FsmEvent(&lp->plci_m, EV_L3_NOTIFY_IND, mc);
		break;
	case PH_CONTROL_IND:
		/* TOUCH TONE */
		FsmEvent(&lp->plci_m, EV_PH_CONTROL_IND, mc);
		break;
	default:
		wprint("PLCI %x pr 0x%x not handled\n", lp->plci, pr);
		break;
	}
}

void lPLCIGetCmsg(struct lPLCI *lp, struct mc_buf *mc)
{
	int retval = 0;

	switch (CMSGCMD(&mc->cmsg)) {
	case CAPI_INFO_REQ:
		retval = FsmEvent(&lp->plci_m, EV_AP_INFO_REQ, mc);
		break;
	case CAPI_ALERT_REQ:
		retval = FsmEvent(&lp->plci_m, EV_AP_ALERT_REQ, mc);
		break;
	case CAPI_CONNECT_REQ:
		retval = FsmEvent(&lp->plci_m, EV_AP_CONNECT_REQ, mc);
		break;
	case CAPI_CONNECT_RESP:
		retval = FsmEvent(&lp->plci_m, EV_AP_CONNECT_RESP, mc);
		break;
	case CAPI_DISCONNECT_REQ:
		retval = FsmEvent(&lp->plci_m, EV_AP_DISCONNECT_REQ, mc);
		break;
	case CAPI_DISCONNECT_RESP:
		retval = FsmEvent(&lp->plci_m, EV_AP_DISCONNECT_RESP, mc);
		break;
	case CAPI_CONNECT_ACTIVE_RESP:
		retval = FsmEvent(&lp->plci_m, EV_AP_CONNECT_ACTIVE_RESP, mc);
		break;
	case CAPI_SELECT_B_PROTOCOL_REQ:
		retval = FsmEvent(&lp->plci_m, EV_AP_SELECT_B_PROTOCOL_REQ, mc);
		break;
	default:
		wprint("PLCI:%x command %02x/%02x not handled\n", lp->plci, mc->cmsg.Command, mc->cmsg.Subcommand);
		retval = -1;
	}
	if (retval) {
		if (mc->cmsg.Subcommand == CAPI_REQ) {
			capi_cmsg_answer(&mc->cmsg);
			mc->cmsg.Info = CapiMessageNotSupportedInCurrentState;
			Send2Application(lp, mc);
		}
	}
}

uint16_t lPLCISendMessage(struct lPLCI *lp, struct mc_buf *mc)
{
	uint16_t ret;

	lPLCIGetCmsg(lp, mc);
	ret = CapiNoError;
	return ret;
}

struct mNCCI *ConnectB3Request(struct lPLCI *lp, struct mc_buf *mc)
{
	struct mNCCI *ncci = ncciCreate(lp);

	if (!ncci) {
		wprint("Could not create NCCI for PCLI %04x\n", lp->plci);
	} else if (!ncci->BIlink) {
		wprint("No channel instance assigned for PCLI %04x\n", lp->plci);
	}
	return ncci;
}

static int lPLCILinkDown(struct lPLCI *lp)
{
	struct mNCCI *nc, *nn;
	struct BInstance *bi;

	nc = lp->Nccis;
	if (nc) {
		while (nc) {
			nn = nc->next;
			ncciReleaseLink(nc);
			nc = nn;
		}
	} else if (lp->BIlink) {
		bi = lp->BIlink;
		lp->BIlink = NULL;
		CloseBInstance(bi);
	}
	return 0;
}

int lPLCIFacHoldReq(struct lPLCI *lp, struct FacReqParm *facReqParm, struct FacConfParm *facConfParm)
{
	/* no parameter needed so we do not need a l3m */
	if (FsmEvent(&lp->plci_m, EV_AP_HOLD_REQ, NULL)) {
		// no routine
		facConfParm->u.Info.SupplementaryServiceInfo = CapiRequestNotAllowedInThisState;
		return CapiMessageNotSupportedInCurrentState;
	} else {
		facConfParm->u.Info.SupplementaryServiceInfo = CapiNoError;
	}
	return CapiNoError;
}

int lPLCIFacRetrieveReq(struct lPLCI *lp, struct FacReqParm *facReqParm, struct FacConfParm *facConfParm)
{
	/* no parameter needed so we do not need a l3m */
	if (FsmEvent(&lp->plci_m, EV_AP_RETRIEVE_REQ, NULL)) {
		// no routine
		facConfParm->u.Info.SupplementaryServiceInfo = CapiRequestNotAllowedInThisState;
		return CapiMessageNotSupportedInCurrentState;
	} else {
		facConfParm->u.Info.SupplementaryServiceInfo = CapiNoError;
	}
	return CapiNoError;
}

int lPLCIFacSuspendReq(struct lPLCI *lp, struct FacReqParm *facReqParm, struct FacConfParm *facConfParm)
{
	unsigned char *CallId;
	struct l3_msg *l3m;

	CallId = facReqParm->u.Suspend.CallIdentity;
	if (CallId && CallId[0] > 8)
		return CapiIllMessageParmCoding;
	l3m = alloc_l3_msg();
	if (!l3m) {
		wprint("Could not allocate l3 message for PLCI %04x\n", lp->plci);
		return CapiMsgOSResourceErr;
	}
	if (CallId && CallId[0])
		add_layer3_ie(l3m, IE_CALL_ID, CallId[0], &CallId[1]);

	if (FsmEvent(&lp->plci_m, EV_AP_SUSPEND_REQ, l3m)) {
		// no routine
		facConfParm->u.Info.SupplementaryServiceInfo = CapiRequestNotAllowedInThisState;
		free_l3_msg(l3m);
		return CapiMessageNotSupportedInCurrentState;
	} else {
		facConfParm->u.Info.SupplementaryServiceInfo = CapiNoError;
	}
	return CapiNoError;
}

int lPLCIFacResumeReq(struct lPLCI *lp, struct FacReqParm *facReqParm, struct FacConfParm *facConfParm)
{
	__u8 *CallId;
	struct l3_msg *l3m;

	CallId = facReqParm->u.Resume.CallIdentity;
	if (CallId && CallId[0] > 8) {
		lPLCI_free(lp);
		return CapiIllMessageParmCoding;
	}
	l3m = alloc_l3_msg();
	if (!l3m) {
		wprint("Could not allocate l3 message for PLCI %04x\n", lp->plci);
		lPLCI_free(lp);
		return CapiIllMessageParmCoding;
	}
	if (CallId && CallId[0])
		add_layer3_ie(l3m, IE_CALL_ID, CallId[0], &CallId[1]);

	if (FsmEvent(&lp->plci_m, EV_AP_RESUME_REQ, l3m)) {
		// no routine
		facConfParm->u.Info.SupplementaryServiceInfo = CapiRequestNotAllowedInThisState;
		free_l3_msg(l3m);
		lPLCI_free(lp);
		return CapiMessageNotSupportedInCurrentState;
	}
	facConfParm->u.Info.SupplementaryServiceInfo = CapiNoError;
	return CapiNoError;
}

void init_lPLCI_fsm(void)
{
	plci_fsm.state_count = ST_PLCI_COUNT;
	plci_fsm.event_count = EV_PLCI_COUNT;
	plci_fsm.strEvent = str_ev_plci;
	plci_fsm.strState = str_st_plci;

	FsmNew(&plci_fsm, fn_plci_list, FN_PLCI_COUNT);
}

void free_lPLCI_fsm(void)
{
	FsmFree(&plci_fsm);
}
