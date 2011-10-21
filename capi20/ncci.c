/* 
 * ncci.c
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
#include <mISDN/q931.h>

// --------------------------------------------------------------------
// NCCI state machine
//
// Some rules:
//   *  EV_AP_*  events come from CAPI Application
//   *  EV_DL_*  events come from the ISDN stack
//   *  EV_NC_*  events generated in NCCI handling
//   *  messages are send in the routine that handle the event
//
// --------------------------------------------------------------------
enum {
	ST_NCCI_N_0,
	ST_NCCI_N_0_1,
	ST_NCCI_N_1,
	ST_NCCI_N_2,
	ST_NCCI_N_ACT,
	ST_NCCI_N_3,
	ST_NCCI_N_4,
	ST_NCCI_N_5,
}

const ST_NCCI_COUNT = ST_NCCI_N_5 + 1;

static char *str_st_ncci[] = {
	"ST_NCCI_N_0",
	"ST_NCCI_N_0_1",
	"ST_NCCI_N_1",
	"ST_NCCI_N_2",
	"ST_NCCI_N_ACT",
	"ST_NCCI_N_3",
	"ST_NCCI_N_4",
	"ST_NCCI_N_5",
}; 

enum {
	EV_AP_CONNECT_B3_REQ,
	EV_NC_CONNECT_B3_CONF,
	EV_NC_CONNECT_B3_IND,
	EV_AP_CONNECT_B3_RESP,
	EV_NC_CONNECT_B3_ACTIVE_IND,
	EV_AP_CONNECT_B3_ACTIVE_RESP,
	EV_AP_RESET_B3_REQ,
	EV_NC_RESET_B3_IND,
	EV_NC_RESET_B3_CONF,
	EV_AP_RESET_B3_RESP,
	EV_NC_CONNECT_B3_T90_ACTIVE_IND,
	EV_AP_DISCONNECT_B3_REQ,
	EV_NC_DISCONNECT_B3_IND,
	EV_NC_DISCONNECT_B3_CONF,
	EV_AP_DISCONNECT_B3_RESP,
	EV_AP_FACILITY_REQ,
	EV_AP_MANUFACTURER_REQ,
	EV_DL_ESTABLISH_IND,
	EV_DL_ESTABLISH_CONF,
	EV_DL_RELEASE_IND,
	EV_DL_RELEASE_CONF,
	EV_DL_DOWN_IND,
	EV_NC_LINKDOWN,
	EV_AP_RELEASE,
}

const EV_NCCI_COUNT = EV_AP_RELEASE + 1;

static char* str_ev_ncci[] = {
	"EV_AP_CONNECT_B3_REQ",
	"EV_NC_CONNECT_B3_CONF",
	"EV_NC_CONNECT_B3_IND",
	"EV_AP_CONNECT_B3_RESP",
	"EV_NC_CONNECT_B3_ACTIVE_IND",
	"EV_AP_CONNECT_B3_ACTIVE_RESP",
	"EV_AP_RESET_B3_REQ",
	"EV_NC_RESET_B3_IND",
	"EV_NC_RESET_B3_CONF",
	"EV_AP_RESET_B3_RESP",
	"EV_NC_CONNECT_B3_T90_ACTIVE_IND",
	"EV_AP_DISCONNECT_B3_REQ",
	"EV_NC_DISCONNECT_B3_IND",
	"EV_NC_DISCONNECT_B3_CONF",
	"EV_AP_DISCONNECT_B3_RESP",
	"EV_AP_FACILITY_REQ",
	"EV_AP_MANUFACTURER_REQ",
	"EV_DL_ESTABLISH_IND",
	"EV_DL_ESTABLISH_CONF",
	"EV_DL_RELEASE_IND",
	"EV_DL_RELEASE_CONF",
	"EV_DL_DOWN_IND",
	"EV_NC_LINKDOWN",
	"EV_AP_RELEASE",
};

static struct Fsm ncci_fsm = { 0, 0, 0, 0, 0 };

static int ncciL4L3(struct mNCCI *ncci, uint32_t, int, int, void *, struct mc_buf *);

static void
ncci_debug(struct FsmInst *fi, char *fmt, ...)
{
	char tmp[128];
	char *p = tmp;
	va_list args;
	struct mNCCI *ncci = fi->userdata;
	
	if (!ncci->ncci_m.debug)
		return;
	va_start(args, fmt);
	p += sprintf(p, "NCCI %08x: ", ncci->ncci);
	p += vsprintf(p, fmt, args);
	*p = 0;
	dprint(MIDEBUG_STATES, "%s\n", tmp);
	va_end(args);
}

static inline void
Send2Application(struct mNCCI *ncci, struct mc_buf *mc)
{
	SendCmsg2Application(ncci->appl, mc);
}

static inline void
ncciCmsgHeader(struct mNCCI *ncci, struct mc_buf *mc, __u8 cmd, __u8 subcmd)
{
	capi_cmsg_header(&mc->cmsg, ncci->appl->AppId, cmd, subcmd, 
			 ncci->appl->MsgId++, ncci->ncci);
}

static void
ncci_connect_b3_req(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;

	FsmChangeState(fi, ST_NCCI_N_0_1);
	capi_cmsg_answer(&mc->cmsg);

	// TODO: NCPI handling
	mc->cmsg.Info = 0;
	mc->cmsg.adr.adrNCCI = ncci->ncci;
	FsmEvent(fi, EV_NC_CONNECT_B3_CONF, mc);
}

static void
ncci_connect_b3_ind(struct FsmInst *fi, int event, void *arg)
{
	// from DL_ESTABLISH
	FsmChangeState(fi, ST_NCCI_N_1);
	Send2Application(fi->userdata, arg);
}

static void
ncci_connect_b3_resp(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;

	if (mc->cmsg.Info == 0) {
		FsmChangeState(fi, ST_NCCI_N_2);
		ncciCmsgHeader(ncci, mc, CAPI_CONNECT_B3_ACTIVE, CAPI_IND);
		event = EV_NC_CONNECT_B3_ACTIVE_IND;
	} else {
		FsmChangeState(fi, ST_NCCI_N_4);
		mc->cmsg.Info = 0;
		ncciCmsgHeader(ncci, mc, CAPI_DISCONNECT_B3, CAPI_IND);
		event = EV_NC_DISCONNECT_B3_IND;
	}
	FsmEvent(&ncci->ncci_m, event, mc);
}

static void
ncci_connect_b3_conf(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;
	unsigned int	pr;

	if (mc->cmsg.Info == 0) {
		FsmChangeState(fi, ST_NCCI_N_2);
		Send2Application(ncci, mc);
		pr = ncci->l1direct ? PH_ACTIVATE_REQ : DL_ESTABLISH_REQ;
		ncciL4L3(ncci, pr, 0, 0, NULL, NULL);
	} else {
		FsmChangeState(fi, ST_NCCI_N_0);
		Send2Application(ncci, mc);
		ncciFree(ncci);
	}
}

static void
ncci_disconnect_b3_req(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;
	uint16_t	Info = 0;
	int		prim;

	if (ncci->appl) { //FIXME
		/* TODO: handle NCPI and wait for all DATA_B3_REQ confirmed on
		 * related protocols (voice, T30)
		 */ 
		capi_cmsg_answer(&mc->cmsg);
		mc->cmsg.Info = Info;
		FsmEvent(fi, EV_NC_DISCONNECT_B3_CONF, mc);
	} else {
		FsmChangeState(fi, ST_NCCI_N_4);
	}
	prim = ncci->l1direct ? PH_DEACTIVATE_REQ : DL_RELEASE_REQ;
	ncciL4L3(ncci, prim, 0, 0, NULL, NULL);
}

static void
ncci_disconnect_b3_conf(struct FsmInst *fi, int event, void *arg)
{
	struct mc_buf *mc = arg;

	if (mc->cmsg.Info == 0) {
		FsmChangeState(fi, ST_NCCI_N_4);
	}
	Send2Application(fi->userdata, mc);
}

static void
ncci_disconnect_b3_ind(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;

	FsmChangeState(fi, ST_NCCI_N_5);
	Send2Application(ncci, arg);
}

static void
ncci_disconnect_b3_resp(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_NCCI_N_0);
	ncciFree(fi->userdata);
}

static void
ncci_facility_req(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;
	u_char		*p = mc->cmsg.FacilityRequestParameter;
	uint16_t	func;
	int		op;

	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = CapiNoError;
	if (mc->cmsg.FacilitySelector == 0) { // Handset
#ifdef HANDSET_SERVICE
		int err = ncciL4L3(ncci, PH_CONTROL_REQ, HW_POTS_ON, 0, NULL, NULL);
		if (err)
#endif
			mc->cmsg.Info = CapiSupplementaryServiceNotSupported;
	} else if (mc->cmsg.FacilitySelector != 1) { // not DTMF
		mc->cmsg.Info = CapiIllMessageParmCoding;
	} else if (p && p[0]) {
		func = CAPIMSG_U16(p, 1);
		ncci_debug(fi, "%s: p %02x %02x %02x func(%x)",
			__FUNCTION__, p[0], p[1], p[2], func);
		switch (func) {
			case 1:
				op = DTMF_TONE_START;
				ncciL4L3(ncci, PH_CONTROL_REQ, 0, sizeof(int), &op, NULL);
				break;
			case 2:
				op = DTMF_TONE_STOP;
				ncciL4L3(ncci, PH_CONTROL_REQ, 0, sizeof(int), &op, NULL);
				break;
			default:
				mc->cmsg.Info = CapiSupplementaryServiceNotSupported;
				break;
		}
	} else
		mc->cmsg.Info = CapiIllMessageParmCoding;

	dprintf(MIDEBUG_NCCI, "NCCI %06x: fac\n", ncci->ncci);		
	Send2Application(ncci, mc);
}

static void
ncci_manufacturer_req(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;
#ifdef HANDSET_SERVICE
	int	err, op;
	struct  _manu_conf_para {
			uint8_t		len;
			uint16_t	Info;
			uint16_t	vol;
		}  __attribute__((packed)) mcp = {2, CapiNoError, 0};

	struct  _manu_req_para {
			uint8_t		len;
			uint16_t	vol;
		} __attribute__((packed)) *mrp;

	mrp = (struct  _manu_req_para *)cmsg->ManuData;
	capi_cmsg_answer(cmsg);
	if (cmsg->Class == mISDN_MF_CLASS_HANDSET) { // Handset
		switch(cmsg->Function) {
			case mISDN_MF_HANDSET_ENABLE:
				err = ncciL4L3(ncci, PH_CONTROL | REQUEST, HW_POTS_ON, 0, NULL, NULL);
				if (err)
					mcp.Info = CapiFacilityNotSupported;
				break;
			case mISDN_MF_HANDSET_DISABLE:
				err = ncciL4L3(ncci, PH_CONTROL | REQUEST, HW_POTS_OFF, 0, NULL, NULL);
				if (err)
					mcp.Info = CapiSupplementaryServiceNotSupported;
				break;
			case mISDN_MF_HANDSET_SETMICVOLUME:
			case mISDN_MF_HANDSET_SETSPKVOLUME:
				if (!mrp || mrp->len != 2) {
					mcp.Info = CapiIllMessageParmCoding;
					break;
				}
				op = (cmsg->Function == mISDN_MF_HANDSET_SETSPKVOLUME) ?
					HW_POTS_SETSPKVOL : HW_POTS_SETMICVOL;
				err = ncciL4L3(ncci, PH_CONTROL | REQUEST, op, 2, &mrp->vol, NULL);
				if (err == -ENODEV)
					mcp.Info = CapiSupplementaryServiceNotSupported;
				else if (err)
					mcp.Info = CapiIllMessageParmCoding;
				break;
			/* not handled yet */
			case mISDN_MF_HANDSET_GETMICVOLUME:
			case mISDN_MF_HANDSET_GETSPKVOLUME:
			default:
				mcp.Info = CapiSupplementaryServiceNotSupported;
				break;
		}
	} else
		mcp.Info = CapiIllMessageParmCoding;

	cmsg->ManuData = (_cstruct)&mcp;
#else
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = CapiSupplementaryServiceNotSupported;
#endif
	Send2Application(ncci, mc);
}

static void
ncci_connect_b3_active_ind(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	int	i;

	FsmChangeState(fi, ST_NCCI_N_ACT);
	for (i = 0; i < CAPI_MAXDATAWINDOW; i++) {
		ncci->xmit_handles[i].PktId = 0;
		ncci->recv_handles[i] = 0;
	}
	Send2Application(ncci, arg);
}

static void
ncci_connect_b3_active_resp(struct FsmInst *fi, int event, void *arg)
{
}

static void
ncci_n0_dl_establish_ind_conf(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;

	ncciCmsgHeader(ncci, arg, CAPI_CONNECT_B3, CAPI_IND);
	FsmEvent(&ncci->ncci_m, EV_NC_CONNECT_B3_IND, arg);
}

static void
ncci_dl_establish_conf(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;

	ncciCmsgHeader(ncci, mc, CAPI_CONNECT_B3_ACTIVE, CAPI_IND);
	FsmEvent(&ncci->ncci_m, EV_NC_CONNECT_B3_ACTIVE_IND, mc);
}

static void
ncci_dl_release_ind_conf(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;

	ncciCmsgHeader(ncci, mc, CAPI_DISCONNECT_B3, CAPI_IND);
	FsmEvent(&ncci->ncci_m, EV_NC_DISCONNECT_B3_IND, mc);
}

static void
ncci_linkdown(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;

	ncciCmsgHeader(ncci, mc, CAPI_DISCONNECT_B3, CAPI_IND);
	FsmEvent(&ncci->ncci_m, EV_NC_DISCONNECT_B3_IND, mc);
}

static void
ncci_dl_down_ind(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI	*ncci = fi->userdata;
	struct mc_buf	*mc = arg;

	ncciCmsgHeader(ncci, mc, CAPI_DISCONNECT_B3, CAPI_IND);
	mc->cmsg.Reason_B3 = CapiProtocolErrorLayer1;
	FsmEvent(&ncci->ncci_m, EV_NC_DISCONNECT_B3_IND, mc);
}

static void
ncci_appl_release(struct FsmInst *fi, int event, void *arg)
{
	FsmChangeState(fi, ST_NCCI_N_0);
	ncciFree(fi->userdata);
}

static void
ncci_appl_release_disc(struct FsmInst *fi, int event, void *arg)
{
	struct mNCCI *ncci = fi->userdata;
	int prim;

	prim = ncci->l1direct ? PH_DEACTIVATE_REQ : DL_RELEASE_REQ;
	ncciL4L3(ncci, prim, 0, 0, NULL, NULL);
}

static struct FsmNode fn_ncci_list[] =
{
  {ST_NCCI_N_0,		EV_AP_CONNECT_B3_REQ,		ncci_connect_b3_req},
  {ST_NCCI_N_0,		EV_NC_CONNECT_B3_IND,		ncci_connect_b3_ind},
  {ST_NCCI_N_0,		EV_DL_ESTABLISH_CONF,		ncci_n0_dl_establish_ind_conf},
  {ST_NCCI_N_0,		EV_DL_ESTABLISH_IND,		ncci_n0_dl_establish_ind_conf},
  {ST_NCCI_N_0,		EV_AP_RELEASE,			ncci_appl_release},

  {ST_NCCI_N_0_1,	EV_NC_CONNECT_B3_CONF,		ncci_connect_b3_conf},
  {ST_NCCI_N_0_1,	EV_AP_MANUFACTURER_REQ,		ncci_manufacturer_req},
  {ST_NCCI_N_0_1,	EV_AP_RELEASE,			ncci_appl_release},

  {ST_NCCI_N_1,		EV_AP_CONNECT_B3_RESP,		ncci_connect_b3_resp},
  {ST_NCCI_N_1,		EV_AP_DISCONNECT_B3_REQ,	ncci_disconnect_b3_req},
  {ST_NCCI_N_1,		EV_NC_DISCONNECT_B3_IND,	ncci_disconnect_b3_ind},
  {ST_NCCI_N_1,		EV_AP_MANUFACTURER_REQ,		ncci_manufacturer_req},
  {ST_NCCI_N_1,		EV_AP_RELEASE,			ncci_appl_release_disc},
  {ST_NCCI_N_1,		EV_NC_LINKDOWN,			ncci_linkdown},

  {ST_NCCI_N_2,		EV_NC_CONNECT_B3_ACTIVE_IND,	ncci_connect_b3_active_ind},
  {ST_NCCI_N_2,		EV_AP_DISCONNECT_B3_REQ,	ncci_disconnect_b3_req},
  {ST_NCCI_N_2,		EV_NC_DISCONNECT_B3_IND,	ncci_disconnect_b3_ind},
  {ST_NCCI_N_2,		EV_DL_ESTABLISH_CONF,		ncci_dl_establish_conf},
  {ST_NCCI_N_2,		EV_DL_RELEASE_IND,		ncci_dl_release_ind_conf},
  {ST_NCCI_N_2,		EV_AP_MANUFACTURER_REQ,		ncci_manufacturer_req},
  {ST_NCCI_N_2,		EV_AP_RELEASE,			ncci_appl_release_disc},
  {ST_NCCI_N_2,		EV_NC_LINKDOWN,			ncci_linkdown},
     
#if 0
  {ST_NCCI_N_3,		EV_NC_RESET_B3_IND,		ncci_reset_b3_ind},
  {ST_NCCI_N_3,		EV_DL_DOWN_IND,			ncci_dl_down_ind},
  {ST_NCCI_N_3,		EV_AP_DISCONNECT_B3_REQ,	ncci_disconnect_b3_req},
  {ST_NCCI_N_3,		EV_NC_DISCONNECT_B3_IND,	ncci_disconnect_b3_ind},
  {ST_NCCI_N_3,		EV_AP_RELEASE,			ncci_appl_release_disc},
  {ST_NCCI_N_3,		EV_NC_LINKDOWN,			ncci_linkdown},
#endif

  {ST_NCCI_N_ACT,	EV_AP_CONNECT_B3_ACTIVE_RESP,	ncci_connect_b3_active_resp},
  {ST_NCCI_N_ACT,	EV_AP_DISCONNECT_B3_REQ,	ncci_disconnect_b3_req},
  {ST_NCCI_N_ACT,	EV_NC_DISCONNECT_B3_IND,	ncci_disconnect_b3_ind},
  {ST_NCCI_N_ACT,	EV_DL_RELEASE_IND,		ncci_dl_release_ind_conf},
  {ST_NCCI_N_ACT,	EV_DL_RELEASE_CONF,		ncci_dl_release_ind_conf},
  {ST_NCCI_N_ACT,	EV_DL_DOWN_IND,			ncci_dl_down_ind},
  {ST_NCCI_N_ACT,	EV_AP_FACILITY_REQ,		ncci_facility_req},
  {ST_NCCI_N_ACT,	EV_AP_MANUFACTURER_REQ,		ncci_manufacturer_req},
  {ST_NCCI_N_ACT,	EV_AP_RELEASE,			ncci_appl_release_disc},
  {ST_NCCI_N_ACT,	EV_NC_LINKDOWN,			ncci_linkdown},
#if 0
  {ST_NCCI_N_ACT,	EV_AP_RESET_B3_REQ,		ncci_reset_b3_req},
  {ST_NCCI_N_ACT,	EV_NC_RESET_B3_IND,		ncci_reset_b3_ind},
  {ST_NCCI_N_ACT,	EV_NC_CONNECT_B3_T90_ACTIVE_IND,ncci_connect_b3_t90_active_ind},
#endif

  {ST_NCCI_N_4,		EV_NC_DISCONNECT_B3_CONF,	ncci_disconnect_b3_conf},
  {ST_NCCI_N_4,		EV_NC_DISCONNECT_B3_IND,	ncci_disconnect_b3_ind},
  {ST_NCCI_N_4,		EV_DL_RELEASE_CONF,		ncci_dl_release_ind_conf},
  {ST_NCCI_N_4,		EV_DL_DOWN_IND,			ncci_dl_down_ind},
  {ST_NCCI_N_4,		EV_AP_MANUFACTURER_REQ,		ncci_manufacturer_req},
  {ST_NCCI_N_4,		EV_NC_LINKDOWN,			ncci_linkdown},

  {ST_NCCI_N_5,		EV_AP_DISCONNECT_B3_RESP,	ncci_disconnect_b3_resp},
  {ST_NCCI_N_5,		EV_AP_RELEASE,			ncci_appl_release},
};
const int FN_NCCI_COUNT = sizeof(fn_ncci_list)/sizeof(struct FsmNode);


static void initNCCIHeaders(struct mNCCI *nc)
{
	CAPIMSG_SETLEN(nc->up_header, CAPI_B3_DATA_IND_HEADER_SIZE);
	CAPIMSG_SETAPPID(nc->up_header, nc->appl->AppId);
	CAPIMSG_SETCOMMAND(nc->up_header, CAPI_DATA_B3);
	CAPIMSG_SETSUBCOMMAND(nc->up_header, CAPI_IND);
	CAPIMSG_SETCONTROL(nc->up_header, nc->ncci);
	nc->up_msg.msg_name = NULL;
	nc->up_msg.msg_namelen = 0;
	nc->up_msg.msg_iov = nc->up_iv;
	nc->up_msg.msg_iovlen = 2;
	nc->up_msg.msg_control = NULL;
	nc->up_msg.msg_controllen = 0;
	nc->up_msg.msg_flags = 0;
	nc->up_iv[0].iov_base = nc->up_header;
	nc->up_iv[0].iov_len = CAPI_B3_DATA_IND_HEADER_SIZE;
	
	nc->down_header.prim = nc->l1direct ? PH_DATA_REQ : DL_DATA_REQ;
	nc->down_msg.msg_name = NULL;
	nc->down_msg.msg_namelen = 0;
	nc->down_msg.msg_iov = nc->down_iv;
	nc->down_msg.msg_iovlen = 2;
	nc->down_msg.msg_control = NULL;
	nc->down_msg.msg_controllen = 0;
	nc->down_msg.msg_flags = 0;
	nc->down_iv[0].iov_base = &nc->down_header;
	nc->down_iv[0].iov_len = sizeof(nc->down_header);
}                                                                                

struct mNCCI *
ncciCreate(struct lPLCI *lp)
{
	struct mNCCI *nc, *old;

	nc = calloc(1, sizeof(*nc));
	if (!nc) {
		eprint("No memory for NCCI on PLCI:%04x\n", lp->plci);
		return NULL;
	}
	nc->ncci_m.state      = ST_NCCI_N_0;
//	nc->ncci_m.debug      = aplci->plci->contr->debug & CAPI_DBG_NCCI_STATE;
	nc->ncci_m.debug      = MIDEBUG_NCCI & mI_debug_mask;
	nc->ncci_m.userdata   = nc;
	nc->ncci_m.printdebug = ncci_debug;
	/* unused NCCI */
	lp->NcciCnt++;
	nc->ncci = lp->plci;
	nc->ncci |= (lp->NcciCnt <<16) & 0xFFFF0000;
	nc->lp = lp;
	nc->appl = lp->lc->Appl;
	nc->BIlink = lp->BIlink;
	nc->window = lp->lc->Appl->MaxB3Blk;
	pthread_mutex_init(&nc->lock, NULL);
	if (lp->Bprotocol.B1 == 1) {
		if (!lp->l1dtmf) {
			nc->flowmode = flmPHDATA;
			nc->l1direct = 1;
		} else {
			nc->flowmode = flmIndication;
		}
	}
	if (lp->Bprotocol.B2 == 0) { /* X.75 has own flowctrl */
		nc->l2trans = 1;
		if (lp->Bprotocol.B1 == 0) {
			nc->l1direct = 1;
			nc->flowmode = flmPHDATA;
		}
	}
	if (lp->Bprotocol.B3 == 0) {
		nc->l3trans = 1;
		nc->ncci_m.fsm = &ncci_fsm;
	} else
		nc->ncci_m.fsm = &ncci_fsm;
		// nc->ncci_m.fsm = &ncciD_fsm;

	if (nc->window > CAPI_MAXDATAWINDOW) {
		wprint("NCCI %06x: Datawindow too big (%d) reduced to (%d)\n", nc->ncci, nc->window, CAPI_MAXDATAWINDOW);
		nc->window = CAPI_MAXDATAWINDOW;
	}
	initNCCIHeaders(nc);
	nc->osize = 256;
	if (lp->Nccis) {
		old = lp->Nccis;
		while (old->next)
			old = old->next;
		old->next = nc;
	} else
		lp->Nccis = nc;
	dprintf(MIDEBUG_NCCI, "NCCI %06x: created\n", nc->ncci);
	return nc;
}

void
ncciFree(struct mNCCI *ncci)
{
	int	i;

	dprintf(MIDEBUG_NCCI, "NCCI %06x: free\n", ncci->ncci);
	
	/* cleanup data queues */
	for (i = 0; i < CAPI_MAXDATAWINDOW; i++) {
		if (ncci->xmit_handles[i].pkt)
			free_mc_buf(ncci->xmit_handles[i].pkt);
	}
	lPLCIDelNCCI(ncci);
	free(ncci);
}

void
ncciDel_lPlci(struct mNCCI *ncci)
{
	ncci->lp = NULL;
	/* maybe we should release the NCCI here */
}

void
ncciReleaseLink(struct mNCCI *ncci)
{
	/* this is normal shutdown on speech and other transparent protocols */
	struct mc_buf *mc = alloc_mc_buf();

	if (mc) {
		FsmEvent(&ncci->ncci_m, EV_NC_LINKDOWN, mc);
		free_mc_buf(mc);
	}
}


static void AnswerDataB3Req(struct mNCCI *ncci, struct mc_buf *mc, uint16_t Info)
{
	uint16_t dh = CAPIMSG_U16(mc->rb, 18);

	mc->len = 16;
	CAPIMSG_SETLEN(mc->rb, 16);
	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	capimsg_setu16(mc->rb, 12, dh);
	capimsg_setu16(mc->rb, 14, Info);
	SendMessage2Application(ncci->appl, mc);
	free_mc_buf(mc);
}

static void SendDataB3Down(struct mNCCI *ncci, uint16_t len)
{
	int pktid, l, tot, ret, nidx;
	struct mc_buf *mc;

	pthread_mutex_lock(&ncci->lock);
	if (!ncci->xmit_handles[ncci->oidx].pkt) {
		ncci->dlbusy = 0;
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	mc = ncci->xmit_handles[ncci->oidx].pkt;
	if (!ncci->BIlink) {
		wprint("NCCI %06x: BInstance is gone - packet ignored\n", ncci->ncci);
		AnswerDataB3Req(ncci, mc, CapiMessageNotSupportedInCurrentState);
		pthread_mutex_unlock(&ncci->lock);
		return;
	}
	pktid = ++ncci->BIlink->DownId;
	if (0 == pktid || pktid > 0x7fff) {
		pktid = 1;
		ncci->BIlink->DownId = 1;
	}
	ncci->xmit_handles[ncci->oidx].PktId = pktid;
	if (ncci->flowmode == flmIndication) {
		tot = ncci->down_iv[0].iov_len;
		l = ncci->xmit_handles[ncci->oidx].dlen - ncci->xmit_handles[ncci->oidx].sent;
		if (l >= len) {
			l = len;
			len = 0;
		} else
			len -= l;
		ncci->down_iv[1].iov_len = l;
		ncci->down_iv[1].iov_base = ncci->xmit_handles[ncci->oidx].sp;
		ncci->xmit_handles[ncci->oidx].sp += l;
		ncci->xmit_handles[ncci->oidx].sent += l;
		ncci->down_msg.msg_iovlen = 2;
		tot += l;
		if (len) {
			nidx = ncci->oidx + 1;
			if (nidx == ncci->window)
				nidx = 0;
			if (ncci->xmit_handles[nidx].pkt) {
				l = ncci->xmit_handles[nidx].dlen - ncci->xmit_handles[nidx].sent;
				if (l >= len) {
					l = len;
					len = 0;
				} else
					len -= l;
				ncci->down_iv[2].iov_len = l;
				ncci->down_iv[2].iov_base = ncci->xmit_handles[nidx].sp;
				ncci->xmit_handles[nidx].sp += l;
				ncci->xmit_handles[ncci->oidx].sent += l;
				ncci->down_msg.msg_iovlen = 3;
				tot += l;
			}
		}
		ncci->down_header.id = pktid;
	} else {
		if (ncci->flowmode == flmPHDATA) {
			if (ncci->dlbusy) {
				wprint("NCCI %06x: dlbusy set\n", ncci->ncci);
				pthread_mutex_unlock(&ncci->lock);
				return;
			} else
				ncci->dlbusy = 1;
		}
		/* complete paket */
		l = ncci->xmit_handles[ncci->oidx].dlen;
		ncci->down_iv[1].iov_len = l;
		ncci->down_iv[1].iov_base = mc->rp;
		ncci->xmit_handles[ncci->oidx].sent = l;
		ncci->down_header.id = pktid;
		ncci->down_msg.msg_iovlen = 2;
		tot = l + ncci->down_iv[0].iov_len;
	}
	ret = sendmsg(ncci->BIlink->fd, &ncci->down_msg, MSG_DONTWAIT);
	if (ret != tot) {
		wprint("NCCI %06x: send returned %d while sending %d bytes type %s id %d - %s\n", ncci->ncci, ret, tot,
			_mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, strerror(errno));
		if (ncci->flowmode != flmIndication) {
			ncci->dlbusy = 0;
			ncci->xmit_handles[ncci->oidx].pkt = NULL;
			ncci->xmit_handles[ncci->oidx].PktId = 0;
			ncci->oidx++;
			if (ncci->oidx == ncci->window)
				ncci->oidx = 0;
			AnswerDataB3Req(ncci, mc, CapiMsgOSResourceErr);
		}
		pthread_mutex_unlock(&ncci->lock);
		return;
	} else
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: send down %d bytes type %s id %d current oidx[%d] sent %d/%d\n", ncci->ncci, ret,
			_mi_msg_type2str(ncci->down_header.prim), ncci->down_header.id, ncci->oidx,
			ncci->xmit_handles[ncci->oidx].sent, ncci->xmit_handles[ncci->oidx].dlen);
	ncci->dlbusy = 1;
	if (ncci->flowmode == flmIndication) {
		if (ncci->xmit_handles[ncci->oidx].sent == ncci->xmit_handles[ncci->oidx].dlen) {
			ncci->xmit_handles[ncci->oidx].pkt = NULL;
			ncci->oidx++;
			if (ncci->oidx == ncci->window)
				ncci->oidx = 0;
			AnswerDataB3Req(ncci, mc, CapiNoError);
			mc = ncci->xmit_handles[ncci->oidx].pkt;
			if (mc && (ncci->xmit_handles[ncci->oidx].sent == ncci->xmit_handles[ncci->oidx].dlen)) {
				ncci->xmit_handles[ncci->oidx].pkt = NULL;
				ncci->oidx++;
				if (ncci->oidx == ncci->window)
					ncci->oidx = 0;
				AnswerDataB3Req(ncci, mc, CapiNoError);
			}
			if (!ncci->xmit_handles[ncci->oidx].pkt)
				ncci->dlbusy = 0;
		}
	} else {
		if (ncci->flowmode == flmNone) {
			ncci->dlbusy = 0;
			ncci->xmit_handles[ncci->oidx].pkt = NULL;
			ncci->xmit_handles[ncci->oidx].PktId = 0;
			AnswerDataB3Req(ncci, mc, CapiNoError);
		}
		ncci->oidx++;
		if (ncci->oidx == ncci->window)
			ncci->oidx = 0;
	}
	pthread_mutex_unlock(&ncci->lock);
}

static uint16_t ncciDataReq(struct mNCCI *ncci, struct mc_buf *mc)
{
	uint16_t	len, off;

	off = CAPIMSG_LEN(mc->rb);
	if (off != 22 && off != 30) {
		wprint("NCCI %06x: Illegal message len %d\n", ncci->ncci, off);
		AnswerDataB3Req(ncci, mc, CapiIllMessageParmCoding);
		return 0;
	}
	pthread_mutex_lock(&ncci->lock);
	if (!ncci->BIlink) {
		pthread_mutex_unlock(&ncci->lock);
		wprint("NCCI %06x: BInstance is gone - packet ignored\n", ncci->ncci);
		AnswerDataB3Req(ncci, mc, CapiMessageNotSupportedInCurrentState);
		return 0;
	}
	if (ncci->xmit_handles[ncci->iidx].pkt) {
		wprint("NCCI %06x: CapiSendQueueFull\n", ncci->ncci);
		pthread_mutex_unlock(&ncci->lock);
		AnswerDataB3Req(ncci, mc, CapiSendQueueFull);
		return 0;
	}
	len = CAPIMSG_DATALEN(mc->rb);
	ncci->xmit_handles[ncci->iidx].pkt = mc;
	ncci->xmit_handles[ncci->iidx].DataHandle = CAPIMSG_REQ_DATAHANDLE(mc->rb);
	ncci->xmit_handles[ncci->iidx].MsgId = CAPIMSG_MSGID(mc->rb);
	ncci->xmit_handles[ncci->iidx].dlen = len;
	ncci->xmit_handles[ncci->iidx].sent = 0;
	mc->rp = mc->rb + off;
	mc->len = len;
	ncci->xmit_handles[ncci->iidx].sp = mc->rp;
	
	dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: handle = %d data offset %d totlen %d datalen %d flowmode:%d ncci->dlbusy:%d\n",
		ncci->ncci, ncci->xmit_handles[ncci->iidx].DataHandle, off, mc->len, len, ncci->flowmode, ncci->dlbusy);
	ncci->iidx++;
	if (ncci->iidx == ncci->window)
		ncci->iidx = 0;
	if (!ncci->dlbusy)
		len = ncci->osize;
	else
		len = 0;
	pthread_mutex_unlock(&ncci->lock);
	if (len)
		SendDataB3Down(ncci, len);
	return 0;
}

static int ncciDataInd(struct mNCCI *ncci, int pr, struct mc_buf *mc)
{
	int i, ret, tot;
	uint16_t dlen, dh;
	struct mISDNhead *hh;

	hh = (struct mISDNhead *)mc->rb;
	dlen = mc->len - sizeof(*hh);
	pthread_mutex_lock(&ncci->lock);
	if (ncci->isize != dlen && (ncci->flowmode == flmIndication)) {
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: New isize (%d --> %d) set\n", ncci->ncci, ncci->isize, dlen);
		ncci->isize =  dlen;
		ncci->osize =  dlen;
	}
	if (!ncci->BIlink) {
		pthread_mutex_unlock(&ncci->lock);
		wprint("NCCI %06x: : frame with %d bytes dropped BIlink gone\n", ncci->ncci, dlen);
		return -EINVAL;
	}
	for (i = 0; i < ncci->window; i++) {
		if (ncci->recv_handles[i] == 0)
			break;
	}

	if (i == ncci->window) {
		// FIXME: trigger flow control if supported by L2 protocol
		wprint("NCCI %06x: : frame with %d bytes discarded\n", ncci->ncci, dlen);
		pthread_mutex_unlock(&ncci->lock);
		return -EBUSY;
	}

	dh = ++ncci->BIlink->UpId;
	if (dh == 0)
		dh = ++ncci->BIlink->UpId;

	ncci->recv_handles[i] = dh;
	
	CAPIMSG_SETMSGID(ncci->up_header, ncci->appl->MsgId++);
	CAPIMSG_SETDATALEN(ncci->up_header, dlen);
	capimsg_setu16(ncci->up_header, 18, dh);
	// FIXME FLAGS
	// capimsg_setu16(cm, 20, 0);

	ncci->up_iv[1].iov_len = dlen;
	hh++;
	ncci->up_iv[1].iov_base = hh;
	tot = dlen + CAPI_B3_DATA_IND_HEADER_SIZE;

	ret = sendmsg(ncci->appl->fd, &ncci->up_msg, MSG_DONTWAIT);

	pthread_mutex_unlock(&ncci->lock);
	if (ret != tot) {
		wprint("NCCI %06x: : frame with %d + %d bytes only %d bytes are sent - %s\n",
			ncci->ncci, dlen, CAPI_B3_DATA_IND_HEADER_SIZE, ret, strerror(errno));
		ret = -EINVAL;
	} else
		dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: frame with %d + %d bytes handle %d was sent ret %d\n",
			ncci->ncci, CAPI_B3_DATA_IND_HEADER_SIZE, dlen, dh, ret);
	if (ncci->flowmode == flmIndication)
		SendDataB3Down(ncci, dlen);
	return ret;
}

static void ncciDataConf(struct mNCCI *ncci, struct mc_buf *mc)
{
	int i;
	struct mISDNhead *hh;

	hh = (struct mISDNhead *)mc->rb;
	if (ncci->flowmode != flmPHDATA) {
		wprint("NCCI %06x: Got DATA confirm for %x - but flow mode(%d)\n", ncci->ncci, hh->id, ncci->flowmode);
		free_mc_buf(mc);
		return;
	}

	pthread_mutex_lock(&ncci->lock);
	for (i = 0; i < ncci->window; i++) {
		if (ncci->xmit_handles[i].PktId == hh->id) {
			if (ncci->xmit_handles[i].pkt)
				break;
		}
	}
	if (i == ncci->window) {
		wprint("NCCI %06x: Got DATA confirm for %x - but ID not found\n", ncci->ncci, hh->id);
		for (i = 0; i < ncci->window; i++)
			wprint("NCCI %06x: PktId[%d] %x\n", ncci->ncci, i, ncci->xmit_handles[i].PktId);
		pthread_mutex_unlock(&ncci->lock);
		free_mc_buf(mc);
		return;
	}
	dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: confirm xmit_handles[%d] pktid=%x handle=%d\n", ncci->ncci, i, hh->id, ncci->xmit_handles[i].DataHandle);
	free_mc_buf(mc);
	mc = ncci->xmit_handles[i].pkt;
	ncci->xmit_handles[i].pkt = NULL;
	ncci->xmit_handles[i].PktId = 0;
	ncci->dlbusy = 0;
	pthread_mutex_unlock(&ncci->lock);
	AnswerDataB3Req(ncci, mc, CapiNoError);
	SendDataB3Down(ncci, 0);
	return;
}

static void
ncciDataResp(struct mNCCI *ncci, struct mc_buf *mc)
{
	int i;
	uint16_t dh = CAPIMSG_RESP_DATAHANDLE(mc->rb);

	pthread_mutex_lock(&ncci->lock);
	for (i = 0; i < ncci->window; i++) {
		if (ncci->recv_handles[i] == dh) {
			dprint(MIDEBUG_NCCI_DATA, "NCCI %06x: data handle %d acked at pos %d\n", ncci->ncci, dh, i);
			ncci->recv_handles[i] = 0;
			break;
		}
	}
	if (i == ncci->window) {
		char deb[128], *dp;

		dp = deb;
		for (i = 0; i < ncci->window; i++)
			dp += sprintf(dp, " [%d]=%d", i, ncci->recv_handles[i]);
		wprint("NCCI %06x: data handle %d not in%s\n", ncci->ncci, dh, deb);
	}
	pthread_mutex_unlock(&ncci->lock);
	free_mc_buf(mc);
}

static int ncciGetCmsg(struct mNCCI *ncci, uint8_t cmd, uint8_t subcmd, struct mc_buf *mc)
{
	int	retval = CapiNoError;

	if (!ncci->l3trans) {
		eprint("NCCI %06x: Error L3 not transparent", ncci->ncci);
		return -EINVAL;
	}
	switch (CAPICMD(cmd, subcmd)) {
		case CAPI_CONNECT_B3_REQ:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_CONNECT_B3_REQ, mc);
			break;
		case CAPI_CONNECT_B3_RESP:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_CONNECT_B3_RESP, mc);
			break;
		case CAPI_CONNECT_B3_ACTIVE_RESP:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_CONNECT_B3_ACTIVE_RESP, mc);
			break;
		case CAPI_DISCONNECT_B3_REQ:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_DISCONNECT_B3_REQ, mc);
			break;
		case CAPI_DISCONNECT_B3_RESP:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_DISCONNECT_B3_RESP, mc);
			break;
		case CAPI_FACILITY_REQ:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_FACILITY_REQ, mc);
			break;
		case CAPI_MANUFACTURER_REQ:
			retval = FsmEvent(&ncci->ncci_m, EV_AP_MANUFACTURER_REQ, mc);
			break;
		default:
			eprint("NCCI %06x: Error Unhandled command %02x/%02x\n", ncci->ncci, cmd, subcmd);
			retval = CapiMessageNotSupportedInCurrentState;
	}
	if (retval) { 
		if (subcmd == CAPI_REQ)
			retval = CapiMessageNotSupportedInCurrentState;
		else { /* RESP */
			wprint("NCCI %06x: Error Message %02x/%02x not supported in state %s\n", ncci->ncci, cmd, subcmd, str_st_ncci[ncci->ncci_m.state]);
			retval = CapiNoError;
		}
	}
	if (retval == CapiNoError)
		free_mc_buf(mc);
	return retval;
}


int ncciSendMessage(struct mNCCI *ncci, uint8_t cmd, uint8_t subcmd, struct mc_buf *mc)
{
	int ret = CapiNoError;
#if 0
	if (!ncci->l3trans) {
		ret = ncci_l4l3_direct(ncci, mc);
		switch(ret) {
			case 0:
				break;
			case -EINVAL:
			case -ENXIO:
				int_error();
				break;	/* (CAPI_MSGBUSY) */
			case -EXFULL:
				int_error();
				break;	/* (CAPI_SENDQUEUEFULL) */
			default:
				int_errtxt("ncci_l4l3_direct return(%d)", ret);
				dev_kfree_skb(skb);
				break;
		}
		return;
	}
#endif
	// we're not using the cmsg for DATA_B3 for performance reasons
	if (cmd == CAPI_DATA_B3) {
		if (subcmd == CAPI_REQ) {
			if (ncci->ncci_m.state == ST_NCCI_N_ACT) {
				ret = ncciDataReq(ncci, mc);
			} else {
				ret = CapiMessageNotSupportedInCurrentState;
				wprint("NCCI %06x: DATA_B3_REQ - but but NCCI state %s\n", ncci->ncci, str_st_ncci[ncci->ncci_m.state]);
			}
		} else if (subcmd == CAPI_RESP) {
			ncciDataResp(ncci, mc);
			ret = CapiNoError;
		} else {
			wprint("NCCI %06x: Unknown subcommand %02x\n", ncci->ncci, subcmd);
			free_mc_buf(mc);
			ret = CapiNoError;
		}
	} else {
		// capi_message2cmsg(cmsg, skb->data);
		ret = ncciGetCmsg(ncci, cmd, subcmd, mc);
	}
	return ret;
}

static int
ncciL4L3(struct mNCCI *ncci, uint32_t prim, int id, int len, void *data, struct mc_buf *mc)
{
	struct mc_buf *loc = mc;
	struct mISDNhead *hh;
	int ret, l = sizeof(*hh);

	dprint(MIDEBUG_NCCI, "NCCI %06x: prim %s id %x len %d\n", ncci->ncci, _mi_msg_type2str(prim), id, len);
	if (!mc) {
		loc = alloc_mc_buf();
		if (!loc) {
			eprint("NCCI %06x: prim %s id %x len %d cannot allocate buffer\n", ncci->ncci, _mi_msg_type2str(prim), id, len);
			return -ENOMEM;
		}
	}
	if (!len) {
		hh = (struct mISDNhead *)loc->rb;
	} else {
		if (data == loc->rp) {
			hh = (struct mISDNhead *)loc->rp;
			hh--;
		} else {
			hh = (struct mISDNhead *)loc->rb;
			hh++;
			memcpy(hh, data, len);
		}
		hh--;
		l += len;
	}
	hh->prim = prim;
	hh->id = id;
	ret = send(ncci->BIlink->fd, hh, l, 0);
	if (!mc)
		free_mc_buf(loc);
	return ret;
}

int recvB_L12(struct BInstance *bi, int pr, struct mc_buf *mc)
{
	struct mISDNhead *hh;
	int ret = 0;

	hh = (struct mISDNhead *)mc->rb;
	switch(pr) {
	// we're not using the Fsm for DL_DATA for performance reasons
	case PH_DATA_IND:
	case DL_DATA_IND:
		if (!bi->nc) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr));
			ret = -EINVAL;
			break;
		}
		if (bi->nc->ncci_m.state == ST_NCCI_N_ACT) {
			ret = ncciDataInd(bi->nc, hh->prim, mc);
		} else {
			wprint("Controller%d ch%d: Got %s but but NCCI state %s\n",
				bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr), str_st_ncci[bi->nc->ncci_m.state]);
			ret = 1;
		}
		break;
	case PH_DATA_CNF:
		if (!bi->nc) {
			wprint("Controller%d ch%d: Got %s but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr));
			ret = -EINVAL;
		} else {
			ncciDataConf(bi->nc, mc);
			ret = 0;
		}
		break;
	case PH_ACTIVATE_CNF:
	case DL_ESTABLISH_CNF:
		if (!bi->nc) {
			bi->nc = ncciCreate(bi->lp);
			if (!bi->nc) {
				eprint("Cannot create NCCI for PLCI %04x\n", bi->lp ? bi->lp->plci : 0xffff);
				return -ENOMEM;
			}
		}
		FsmEvent(&bi->nc->ncci_m, EV_DL_ESTABLISH_CONF, mc);
		ret = 1;
		break;
	case PH_ACTIVATE_IND:
	case DL_ESTABLISH_IND:
		if (!bi->nc) {
			bi->nc = ncciCreate(bi->lp);
			if (!bi->nc) {
				eprint("Cannot create NCCI for PLCI %04x\n", bi->lp ? bi->lp->plci : 0xffff);
				return -ENOMEM;
			}
		}
		FsmEvent(&bi->nc->ncci_m, EV_DL_ESTABLISH_IND, mc);
		ret = 1;
		break;
	case DL_RELEASE_IND:
	case PH_DEACTIVATE_IND:
		if (!bi->nc) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr));
			ret = -EINVAL;
			break;
		}
		FsmEvent(&bi->nc->ncci_m, EV_DL_RELEASE_IND, mc);
		ret = 1;
		break;
	case DL_RELEASE_CNF:
	case PH_DEACTIVATE_CNF:
		if (!bi->nc) {
			wprint("Controller%d ch%d: Got %s but but no NCCI set\n", bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr));
			ret = -EINVAL;
			break;
		}
		FsmEvent(&bi->nc->ncci_m, EV_DL_RELEASE_CONF, mc);
		ret = 1;
		break;
	case PH_CONTROL_IND: /* e.g touch tones */
	case PH_CONTROL_CNF:
		/* handled by AppPlci */
		if (!bi->lp) {
			wprint("Controller%d ch%d: Got %s but but no lPLCI set\n", bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr));
			ret = -EINVAL;
		} else {
			lPLCI_l3l4(bi->lp, pr, mc);
			ret = 1;
		}
		break;
	default:
		wprint("Controller%d ch%d: Got %s (%x) id=%x len %d\n",
			bi->pc->profile.ncontroller, bi->nr, _mi_msg_type2str(pr), pr, hh->id, mc->len);
		ret = -EINVAL;
	}
	return ret;
}

void
init_ncci_fsm(void)
{
	ncci_fsm.state_count = ST_NCCI_COUNT;
	ncci_fsm.event_count = EV_NCCI_COUNT;
	ncci_fsm.strEvent = str_ev_ncci;
	ncci_fsm.strState = str_st_ncci;
	FsmNew(&ncci_fsm, fn_ncci_list, FN_NCCI_COUNT);
}

void
free_ncci_fsm(void)
{
	FsmFree(&ncci_fsm);
}
