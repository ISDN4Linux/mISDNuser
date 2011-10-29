/* 
 * application.c
 *
 * Written by Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright (C) 2011 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this package for more details.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "m_capi.h"
#include "mc_buffer.h"

static struct mApplication *mApplications;

struct mApplication *RegisterApplication(uint16_t ApplId, uint32_t MaxB3Connection, uint32_t MaxB3Blks, uint32_t MaxSizeB3)
{
	struct mApplication *appl, *old = mApplications;

	while (old) {
		if (old->AppId == ApplId) {
			wprint("Application %d already registered\n", ApplId);
			return NULL;
		}
		if (!old->next)
			break;
		old = old->next;
	}
	appl = calloc(1, sizeof(*appl));
	if (!appl) {
		eprint("No memory for application (%zd bytes)\n", sizeof(*appl));
		return NULL;
	}
	appl->AppId = ApplId;
	appl->MaxB3Con = MaxB3Connection;
	appl->MaxB3Blk = MaxB3Blks;
	appl->MaxB3Size = MaxSizeB3;
	if (old)
		old->next = appl;
	else
		mApplications = appl;
	return appl;
}

/*
 * Destroy the Application
 *
 * depending who initiate this we cannot release imediatly, if
 * any AppPlci is still in use.
 *
 * @who:   0 - a AppPlci is released in state APPL_STATE_RELEASE
 *         1 - Application is released from CAPI application
 *         2 - the controller is resetted
 *         3 - the controller is removed
 *         4 - the CAPI module will be unload
 */
void ReleaseApplication(struct mApplication *appl)
{
	struct mApplication *ma = mApplications;

	/* first remove from list */
	while (ma) {
		if (ma == appl) {
			mApplications = appl->next;
			appl->next = NULL;
			break;
		} else if (ma->next == appl) {
			ma->next = appl->next;
			appl->next = NULL;
			break;
		}
		ma = ma->next;
	}
	/* TODO clean controller refs */
	close(appl->fd);
	free(appl);
#if 0
	int i, used = 0;
	AppPlci_t **aplci_p = appl->AppPlcis;

	if (test_and_set_bit(APPL_STATE_DESTRUCTOR, &appl->state)) {
		// we are allready in this function
		return (-EBUSY);
	}
	test_and_set_bit(APPL_STATE_RELEASE, &appl->state);
	test_and_clear_bit(APPL_STATE_ACTIV, &appl->state);
	listenDestr(appl);
	if (who > 2) {
		appl->contr = NULL;
	}
	if (aplci_p) {
		for (i = 0; i < appl->maxplci; i++) {
			if (*aplci_p) {
				switch (who) {
				case 4:
					AppPlciDestr(*aplci_p);
					*aplci_p = NULL;
					break;
				case 1:
				case 2:
				case 3:
					AppPlciRelease(*aplci_p);
				case 0:
					if ((volatile AppPlci_t *)(*aplci_p))
						used++;
					break;
				}
			}
			aplci_p++;
		}
	}
	if (used) {
		if (who == 3) {
			list_del_init(&appl->head);
			list_add(&appl->head, &garbage_applications);
		}
		test_and_clear_bit(APPL_STATE_DESTRUCTOR, &appl->state);
		return (-EBUSY);
	}
	list_del_init(&appl->head);
	appl->maxplci = 0;
	kfree(appl->AppPlcis);
	appl->AppPlcis = NULL;
	kfree(appl);
	return (0);
#endif
}

struct lController *get_lController(struct mApplication *app, int cont)
{
	struct lController *lc = app->contL;

	while (lc) {
		if (lc->Contr->profile.ncontroller == cont)
			break;
		lc = lc->nextC;
	}
	return lc;
}

void SendMessage2Application(struct mApplication *appl, struct mc_buf *mc)
{
	int ret;

	ret = send(appl->fd, mc->rb, mc->len, 0);
	if (ret != mc->len)
		wprint("Message send error len=%d ret=%d - %s\n", mc->len, ret, strerror(errno));
}

void SendCmsg2Application(struct mApplication *appl, struct mc_buf *mc)
{
	int ret;

	capi_cmsg2message(&mc->cmsg, mc->rb);
	mc->len = CAPIMSG_LEN(mc->rb);
	ret = send(appl->fd, mc->rb, mc->len, 0);
	if (ret != mc->len)
		eprint("Message send error len=%d ret=%d - %s\n", mc->len, ret, strerror(errno));
	else
		dprint(MIDEBUG_NCCI_DATA, "Msg: %02x/%02x %s addr %x len %d/%d\n", mc->rb[4], mc->rb[5],
		       capi20_cmd2str(mc->rb[4], mc->rb[5]), CAPIMSG_CONTROL(mc->rb), mc->len, ret);
}

void SendCmsgAnswer2Application(struct mApplication *appl, struct mc_buf *mc, __u16 Info)
{
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = Info;
	SendCmsg2Application(appl, mc);
}

#define CapiFacilityNotSupported		0x300b

static int FacilityMessage(struct mApplication *appl, struct pController *pc, struct mc_buf *mc)
{
	int ret = CapiNoError;
	struct mPLCI *plci;
	struct lPLCI *lp;
	struct mNCCI *ncci;
	unsigned char tmp[64], *p;

	p = tmp;
	switch (mc->cmsg.FacilitySelector) {
#if 0
	case 0x0000:		// Handset
#endif
	case 0x0001:		// DTMF
		dprint(MIDEBUG_CONTROLLER, "DTMF addr %06x\n", mc->cmsg.adr.adrNCCI);
		plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI & 0xFFFF);
		lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
		if (lp) {
			ncci = getNCCI4addr(lp, mc->cmsg.adr.adrNCCI, GET_NCCI_PLCI);
			if (ncci) {
				ret = ncciSendMessage(ncci, mc->cmsg.Command, mc->cmsg.Subcommand, mc);
			} else {
				wprint("DTMF addr %06x NCCI not found\n", mc->cmsg.adr.adrNCCI);
				ret = CapiIllController;
			}
		} else {
			wprint("DTMF addr %06x lPLCI not found\n", mc->cmsg.adr.adrNCCI);
			ret = CapiIllController;
		}
		break;
	case 0x0003:		// SupplementaryServices
		// ret = SupplementaryFacilityReq(appl, mc);
		capimsg_setu8(p, 0, 9);
		capimsg_setu16(p, 1, 0);
		capimsg_setu8(p, 3, 6);
		capimsg_setu16(p, 4, 0);
		capimsg_setu32(p, 6, 0);
		mc->cmsg.FacilityConfirmationParameter = tmp;
		SendCmsgAnswer2Application(appl, mc, ret);
		free_mc_buf(mc);
		ret = CapiNoError;
		break;
	default:
		ret = CapiFacilityNotSupported;
		break;
	}
	return ret;
}

int PutMessageApplication(struct mApplication *appl, struct mc_buf *mc)
{
	int id;
	struct pController *pc;
	struct lController *lc;
	struct mPLCI *plci;
	struct lPLCI *lp;
	struct mNCCI *ncci;
	uint8_t cmd, subcmd;
	int ret = CapiNoError;

	cmd = CAPIMSG_COMMAND(mc->rb);
	subcmd = CAPIMSG_SUBCOMMAND(mc->rb);
	if (mc->len < 12) {
		eprint("message %02x/%02x %s too short (%d)\n", cmd, subcmd, capi20_cmd2str(cmd, subcmd), mc->len);
		ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		SendCmsgAnswer2Application(appl, mc, ret);
		return ret;
	}
	id = CAPIMSG_CONTROL(mc->rb);
	lc = get_lController(appl, id & 0x7f);
	if (lc)
		pc = lc->Contr;
	else
		pc = get_cController(id & 0x7f);
	if (!pc) {
		eprint("message %x controller for id %06x not found\n", cmd, id);
	}
	dprint(MIDEBUG_CONTROLLER, "ID: %06x cmd %02x/%02x %s\n", id, cmd, subcmd, capi20_cmd2str(cmd, subcmd));
	switch (cmd) {
		// for NCCI state machine
	case CAPI_DATA_B3:
	case CAPI_CONNECT_B3_ACTIVE:
	case CAPI_DISCONNECT_B3:
	case CAPI_RESET_B3:
		mcbuf_rb2cmsg(mc);
		if ((subcmd == CAPI_REQ) || (subcmd == CAPI_RESP)) {
			plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI & 0xFFFF);
			lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
			ncci = getNCCI4addr(lp, mc->cmsg.adr.adrNCCI, GET_NCCI_EXACT);
			if (ncci)
				ret = ncciSendMessage(ncci, cmd, subcmd, mc);
			else {
				wprint("Application%d: cmd %x (%s) plci %04x lplci %04x NCCI not found\n", appl->AppId, cmd,
				       capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand),
				       plci ? plci->plci : 0xffff, lp ? lp->plci : 0xffff);
				ret = CapiIllController;
			}
		} else
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case CAPI_CONNECT_B3:
		mcbuf_rb2cmsg(mc);
		plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI & 0xFFFF);
		lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
		if (subcmd == CAPI_REQ) {
			if (lp) {
				ncci = ConnectB3Request(lp, mc);
			} else
				ncci = NULL;
		} else if (subcmd != CAPI_RESP) {
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
			break;
		} else
			ncci = getNCCI4addr(lp, mc->cmsg.adr.adrNCCI, GET_NCCI_EXACT);
		if (ncci)
			ret = ncciSendMessage(ncci, cmd, subcmd, mc);
		else {
			wprint("Application%d: cmd %x (%s) plci %04x lplci %04x NCCI not found\n", appl->AppId, cmd,
			       capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand),
			       plci ? plci->plci : 0xffff, lp ? lp->plci : 0xffff);
			ret = CapiIllController;
		}
		break;
		// for PLCI state machine
	case CAPI_CONNECT:
	case CAPI_INFO:
		mcbuf_rb2cmsg(mc);
		plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI);
		dprint(MIDEBUG_PLCI, "%s adrPLCI %06x plci:%04x ApplId %d\n", capi20_cmd2str(cmd, subcmd), mc->cmsg.adr.adrPLCI,
		       plci ? plci->plci : 0xffff, mc->cmsg.ApplId);
		if (subcmd == CAPI_REQ) {
			if (plci) {
				lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
				if (lp)
					ret = lPLCISendMessage(lp, mc);
				else {
					wprint("%s adrPLCI %06x plci:%04x ApplId %d no plci found\n", capi20_cmd2str(cmd, subcmd),
					       mc->cmsg.adr.adrPLCI, plci ? plci->plci : 0xffff, mc->cmsg.ApplId);
					ret = CapiIllController;
				}
			} else {
				if (!lc) {
					if (pc) {
						lc = addlController(appl, pc);
						if (!lc) {
							ret = CapiMsgOSResourceErr;
							break;
						}
					} else {
						ret = CapiIllController;
						break;
					}
				}
				ret = mPLCISendMessage(lc, mc);
			}
		} else if (subcmd == CAPI_RESP) {
			lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
			if (lp)
				ret = lPLCISendMessage(lp, mc);
			else {
				wprint("%s adrPLCI %06x plci:%04x ApplId %d no plci found\n", capi20_cmd2str(cmd, subcmd),
				       mc->cmsg.adr.adrPLCI, plci ? plci->plci : 0xffff, mc->cmsg.ApplId);
				ret = CapiIllController;
			}
		} else
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case CAPI_ALERT:
	case CAPI_CONNECT_ACTIVE:
	case CAPI_DISCONNECT:
	case CAPI_SELECT_B_PROTOCOL:
		mcbuf_rb2cmsg(mc);
		if ((subcmd == CAPI_REQ) || (subcmd == CAPI_RESP)) {
			plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI);
			lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
			dprint(MIDEBUG_PLCI, "adrPLCI %06x plci:%04x ApplId %d lp %p\n", mc->cmsg.adr.adrPLCI,
			       plci ? plci->plci : 0xffff, mc->cmsg.ApplId, lp);
			if (lp)
				ret = lPLCISendMessage(lp, mc);
			else
				ret = CapiIllController;
		} else
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case CAPI_LISTEN:
		if (subcmd != CAPI_REQ) {
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
			break;
		}
		mcbuf_rb2cmsg(mc);
		if (!lc) {
			if (pc) {
				lc = addlController(appl, pc);
				if (!lc) {
					ret = CapiMsgOSResourceErr;
					break;
				}
			} else {
				ret = CapiIllController;
				break;
			}
		}
		if (!ret)
			ret = listenRequest(lc, mc);
		break;
	case CAPI_FACILITY:
		mcbuf_rb2cmsg(mc);
		ret = FacilityMessage(appl, pc, mc);
		break;
	default:
		ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		wprint("message %x (%s)for controller id %06x not supported yet\n", cmd, capi20_cmd2str(cmd, subcmd), id);
		break;
	}
	if (ret && subcmd != CAPI_RESP)
		SendCmsgAnswer2Application(appl, mc, ret);
	return ret;
}
