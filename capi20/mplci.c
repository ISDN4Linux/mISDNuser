/* 
 * mplci.c
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
#include <mISDN/q931.h>


struct mPLCI *new_mPLCI(struct pController *pc, int pid, struct lPLCI *lp)
{
	struct mPLCI	*plci;
	int plci_nr = 1;

	plci = pc->plciL;
	if (plci) {
		plci_nr = (plci->plci >> 8) & 0xff;
		plci_nr++;
		if (plci_nr > 255)
			return NULL;
	}
	plci = calloc(1, sizeof(*plci));
	if (plci) {
		plci->plci = pc->profile.ncontroller | (plci_nr << 8);
		plci->pid = pid;
		plci->pc = pc;
		plci->lPLCIs = lp;
		plci->next = pc->plciL;
		if (pc->plciL)
			pc->plciL->prev = plci;
		pc->plciL = plci;
	}
	return plci;
}

int free_mPLCI(struct mPLCI *plci)
{
	struct mPLCI *p;
	struct pController *pc;

	while (plci->lPLCIs) {
		lPLCI_free(plci->lPLCIs);
	}
	if (plci->nAppl) {
		wprint("Application count (%d) mismatch for PLCI %04x\n", plci->nAppl, plci->plci);
	}
	pc = plci->pc;
	p = pc->plciL;
	if (p == plci)
		pc->plciL = p->next;
	else {
		while(p) {
			if (p->next == plci)
				p->next = plci->next;
			if (plci->prev)
				plci->next->prev = p;
			
			p = p->next;
		}
	}
	free(plci);
	return 0;
}

int plciAttach_lPLCI(struct mPLCI *plci, struct lPLCI *lp)
{
	struct lPLCI *test = get_lPLCI4Id(plci, lp->lc->Appl->AppId);

	if (test) {
		eprint("lPLCI for application %d already attached\n", lp->lc->Appl->AppId);
		return -1;
	}
	lp->PLCI = plci;
	lp->next = plci->lPLCIs;
	if (plci->lPLCIs)
		plci->lPLCIs->prev = lp;
	plci->lPLCIs = lp;
	plci->nAppl++;
	return plci->nAppl;
}

void plciDetachlPLCI(struct lPLCI *lp)
{
	struct lPLCI *n;
	struct mPLCI *p;

	p = lp->PLCI;

	if (p->lPLCIs == lp) {
		p->lPLCIs = lp->next;
		if (p->lPLCIs)
			p->lPLCIs->prev = NULL;
		p->nAppl--;
	} else {
		n = p->lPLCIs;
		while (n->next) {
			if (n->next == lp) {
				n->next = lp->next;
				if (n->next)
					n->next->prev = n;
				p->nAppl--;
				break;
			}
			n = n->next;
		}
	}
	if (p->nAppl == 0 && p->lPLCIs == NULL) {
		dprint(MIDEBUG_PLCI, "All lPLCIs are gone remove PLCI %04x now\n", p->plci);
		free_mPLCI(p);
	}
}

static void plciHandleSetupInd(struct mPLCI *plci, int pr, struct mc_buf *mc)
{
	uint16_t		CIPValue;
	uint32_t		CIPmask;
	struct pController	*pc;
	struct lController	*lc;
	struct lPLCI		*lp;
	uint8_t			found = 0;
	int			ret;

	CIPValue = q931CIPValue(mc);
	pc = plci->pc;
	CIPmask = 1 << CIPValue;
	dprint(MIDEBUG_PLCI, "PLCI %04x: Check CIPvalue %d (%08x) with CIPmask %08x\n",
		plci->plci, CIPValue, CIPmask, pc->CIPmask);
	if (CIPValue && ((CIPmask & pc->CIPmask) || (pc->CIPmask & 1))) {
		/* at least one Application is listen for this service */
		lc = pc->lClist;
		while (lc) {
			if ((lc->CIPmask & CIPmask) || (lc->CIPmask & 1)) {
				ret = lPLCICreate(&lp, lc, plci);
				if (!ret) {
					ret = plciAttach_lPLCI(plci, lp);
					if (0 < ret) {
						found++;
						lPLCI_l3l4(lp, pr, mc);
					} else {
						wprint("Cannot attach lPLCI on PLCI %04x return %d\n", plci->plci, ret);
						lPLCI_free(lp);
					}
				} else {
					wprint("Cannot create lPLCI for PLCI %04x\n", plci->plci);
				}
			}
			lc = lc->nextA;
		}
	}
	if (found == 0) {
		struct l3_msg *l3m;

		l3m = alloc_l3_msg();
		if (l3m) {
			if (!mi_encode_cause(l3m, CAUSE_INCOMPATIBLE_DEST, CAUSE_LOC_USER, 0, NULL)) {
				ret = pc->l3->to_layer3(pc->l3, MT_RELEASE_COMPLETE, plci->pid, l3m);
				if (ret) {
					wprint("Error %d -  %s on sending %s to pid %x\n", ret, strerror(-ret),
						_mi_msg_type2str(MT_RELEASE_COMPLETE), plci->pid);
					free_l3_msg(l3m);
				}
			}
		} else
			eprint("Cannot allocate l3 message plci %x\n", plci->plci);
		free_mPLCI(plci);
	}
}

int plci_l3l4(struct mPLCI *plci, int pr, struct l3_msg *l3m)
{
	struct lPLCI	*lp;
	struct mc_buf	*mc;

	mc = alloc_mc_buf();
	if (!mc) {
		wprint("PLCI %04x: Cannot allocate mc_buf for %s\n",
			plci->plci, _mi_msg_type2str(pr));
		return -ENOMEM;
	}
	mc->l3m = l3m;
	switch (pr) {
	case MT_SETUP:
		plciHandleSetupInd(plci, pr, mc);
		break;
	default:
		lp = plci->lPLCIs;
		while(lp) {
			lPLCI_l3l4(lp, pr, mc);
			lp = lp->next;
		}
		break;
	}
	free_mc_buf(mc);
	return 0;
}

int mPLCISendMessage(struct lController *lc, struct mc_buf *mc)
{
	struct mPLCI		*plci;
	struct lPLCI		*lp;
	int			ret;

	switch (mc->cmsg.Command) {
	case CAPI_CONNECT:
		plci = new_mPLCI(lc->Contr, 0, NULL);
		if (plci) {
			ret = lPLCICreate(&lp, lc, plci);
			if (!ret) {
				ret = plciAttach_lPLCI(plci, lp);
				if (0 < ret)
					ret = lPLCISendMessage(lp, mc);
				else {
					wprint("Cannot attach lPLCI on PLCI %04x return %d\n", plci->plci, ret);
					ret = CapiMsgOSResourceErr;
				}
			} else {
				wprint("Cannot create lPLCI for PLCI %04x\n", plci->plci);
				ret = CapiMsgOSResourceErr;
			}
		} else {
			wprint("Cannot create PLCI for controller %d\n", lc->Contr->profile.ncontroller);
			ret = CapiMsgOSResourceErr;
		}
		break;
	default:
		wprint("Message %s not handled yet\n", capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand));
		ret = CapiMessageNotSupportedInCurrentState;
		break;
	}
	return ret;
}

struct lPLCI *get_lPLCI4Id(struct mPLCI *plci, uint16_t appId)
{
	struct lPLCI *lp;

	if (!plci)
		return NULL;
	lp = plci->lPLCIs;
	while(lp) {
		if (appId == lp->lc->Appl->AppId)
			break;
		lp = lp->next;
	}
	return lp;
}

struct mPLCI *getPLCI4pid(struct pController *pc, int pid)
{
	struct mPLCI *plci = pc->plciL;

	while(plci) {
		if (plci->pid == pid)
			break;
		plci = plci->next;
	}
	return plci;
}

struct mPLCI *getPLCI4Id(struct pController *pc, uint32_t id)
{
	struct mPLCI *plci = pc->plciL;

	while(plci) {
		if (plci->plci == id)
			break;
		plci = plci->next;
	}
	return plci;
}

int
plciL4L3(struct mPLCI *plci, int mt, struct l3_msg *l3m)
{
	int ret;

	ret = plci->pc->l3->to_layer3(plci->pc->l3, mt, plci->pid, l3m);
	if (ret < 0) {
		wprint("Error sending %s to controller %d pid %x %s msg\n", _mi_msg_type2str(mt),
			plci->pc->profile.ncontroller, plci->pid, l3m ? "with" : "no");
		if (l3m)
			free_l3_msg(l3m);
	}
	dprint(MIDEBUG_PLCI, "Sending %s to layer3 pid %x controller %d %s msg\n", _mi_msg_type2str(mt),
		plci->pc->profile.ncontroller, plci->pid, l3m ? "with" : "no");
	return ret;
}
