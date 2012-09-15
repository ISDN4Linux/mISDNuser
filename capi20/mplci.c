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

struct mPLCI *new_mPLCI(struct pController *pc, unsigned int pid)
{
	struct mPLCI *plci;
	int ret;

	plci = calloc(1, sizeof(*plci));
	if (!plci) {
		eprint("Controller:%x PID:%x no memory for PLCI\n", pc->mNr, pid);
		return NULL;
	}
	ret = init_cobj_registered(&plci->cobj, &pc->cobjPLCI, Cot_PLCI, 0x01ff);
	if (ret) {
		eprint("Controller:%x PID:%x Error on init - no IDs left\n", pc->mNr, pid);
		free(plci);
		plci = NULL;
	} else {
		plci->cobj.id2 = pid;
		plci->pc = pc;
	}
	return plci;
}

void dump_controller_plci(struct pController *pc)
{
	struct mCAPIobj *co;
	struct mPLCI *plci;

	pthread_rwlock_rdlock(&pc->cobjPLCI.lock);
	co = pc->cobjPLCI.listhead;
	while (co) {
		plci = container_of(co, struct mPLCI, cobj);
		iprint("%s refcnt:%d number lPLCI:%d %s%s\n", CAPIobjIDstr(co), co->refcnt, co->itemcnt,
			plci->alerting ? "alerting " : "", plci->outgoing ? "outgoing" : "incoming");
		pthread_rwlock_rdlock(&co->lock);
		if (co->listhead)
			dump_Lplcis(container_of(co->listhead, struct lPLCI, cobj));
		pthread_rwlock_unlock(&co->lock);
		co = co->next;
	}
	pthread_rwlock_unlock(&pc->cobjPLCI.lock);
}

void Free_PLCI(struct mCAPIobj *co)
{
	struct mPLCI *plci = container_of(co, struct mPLCI, cobj);

	if (!co->cleaned) {
		delist_cobj(co);
		co->cleaned = 1;
	}
	plci->pc = NULL;
	if (co->parent) {
		put_cobj(co->parent);
		co->parent = NULL;
	}
	dprint(MIDEBUG_PLCI, "%s: freeing done\n", CAPIobjIDstr(co));
	free_capiobject(co, plci);
}

static void cleanup_mPLCI(struct mPLCI *plci)
{
	struct pController *pc = plci->pc;
	struct mCAPIobj *co;
	struct lPLCI *lp;

	plci->cobj.cleaned = 1;
	co = get_next_cobj(&plci->cobj, NULL);
	while (co) {
		lp = container_of(co, struct lPLCI, cobj);
		cleanup_lPLCI(lp);
		co = get_next_cobj(&plci->cobj, co);
	}
	if (plci->cobj.itemcnt) {
		wprint("%s: lPLCI count %d not zero\n", CAPIobjIDstr(&plci->cobj), plci->cobj.itemcnt);
	}
	if (pc) {
		plci->pc = NULL;
		delist_cobj(&plci->cobj);
	} else
		eprint("%s: no pcontroller assigned\n", CAPIobjIDstr(&plci->cobj));
}

void plciDetachlPLCI(struct lPLCI *lp)
{
	struct mPLCI *p;
	int mt;
	struct l3_msg *l3m;

	p = p4lPLCI(lp);
	if (!p) {
		eprint("%s: detach no PLCI\n", CAPIobjIDstr(&lp->cobj));
		return;
	}
	if (lp->rel_req) {
		/* need to store cause for final answer */
		if (lp->cause_loc == CAUSE_LOC_USER) {
			dprint(MIDEBUG_PLCI, "%s: set final cause plci:#%d lplci:#%d\n",
				CAPIobjIDstr(&lp->cobj), p->cause, lp->cause);
			if (p->cause <= 0) {
				p->cause = lp->cause;
			} else if (lp->cause < p->cause) {
				/* for now we prefer lower values, maybe need changed */
				p->cause = lp->cause;
			}
			p->cause_loc = CAUSE_LOC_USER;
		} else
			wprint("%s: cause got owerwritten loc:#%d cause #%d - not stored\n",
				CAPIobjIDstr(&lp->cobj), lp->cause_loc, lp->cause);

	}
	delist_cobj(&lp->cobj);
	if (p->cobj.itemcnt == 0) {
		if (p->cause > 0) {
			dprint(MIDEBUG_PLCI, "%s: last lPLCI gone clear call cause #%d\n",
				CAPIobjIDstr(&p->cobj), p->cause);
			l3m = alloc_l3_msg();
			if (!l3m) {
				eprint("%s: disconnect not send no l3m\n", CAPIobjIDstr(&p->cobj));
			} else {
				if (p->alerting)
					mt = MT_DISCONNECT;
				else
					mt = MT_RELEASE_COMPLETE;
				mi_encode_cause(l3m, p->cause, p->cause_loc, 0, NULL);
				plciL4L3(p, mt, l3m);
			}
		}
		dprint(MIDEBUG_PLCI, "%s: All lPLCIs are gone remove PLCI now\n", CAPIobjIDstr(&p->cobj));
		cleanup_mPLCI(p);
	}
}

static void plciHandleSetupInd(struct mPLCI *plci, int pr, struct mc_buf *mc)
{
	uint16_t CIPValue;
	uint32_t CIPmask;
	struct pController *pc;
	struct mCAPIobj *co;
	struct lController *lc;
	struct lPLCI *lp;
	uint8_t found = 0;
	int cause = CAUSE_INCOMPATIBLE_DEST;
	int ret;

	if (!mc || !mc->l3m) {
		eprint("%s: SETUP without message\n", CAPIobjIDstr(&plci->cobj));
		return;
	}
	CIPValue = q931CIPValue(mc, &CIPmask);
	pc = plci->pc;
	dprint(MIDEBUG_PLCI, "%s: check CIPvalue %d (%08x) with CIPmask %08x chanIE:%s\n",
		CAPIobjIDstr(&plci->cobj), CIPValue, CIPmask, pc->CIPmask, mc->l3m->channel_id ? "yes" : "no");
	if (CIPValue && ((CIPmask & pc->CIPmask) || (pc->CIPmask & 1))) {
		/* at least one Application is listen for this service */
		co = get_next_cobj(&pc->cobjLC, NULL);
		while (co) {
			lc = container_of(co, struct lController, cobj);
			if ((lc->CIPmask & CIPmask) || (lc->CIPmask & 1)) {
				ret = lPLCICreate(&lp, lc, plci);
				if (!ret) {
					found++; 
					put_cobj(&lp->cobj);
				} else {
					wprint("%s: cannot create lPLCI\n", CAPIobjIDstr(&plci->cobj));
				}
			}
			co = get_next_cobj(&pc->cobjLC, co);
		}
		if (plci->cobj.itemcnt) {
			/* at least one lplci was created */
			co = get_next_cobj(&plci->cobj, NULL);
			while (co) {
				lp = container_of(co, struct lPLCI, cobj);
				lPLCI_l3l4(lp, pr, mc);
				dprint(MIDEBUG_PLCI, "%s: SETUP %s\n",
					CAPIobjIDstr(&lp->cobj), lp->ignored ? "ignored - no B-channel" : "delivered");
				if (lp->ignored)
					cleanup_lPLCI(lp);
				co = get_next_cobj(&plci->cobj, co);
			}
		}
	}
	if (found == 0) {
		struct l3_msg *l3m;

		l3m = alloc_l3_msg();
		if (l3m) {
			dprint(MIDEBUG_PLCI, "%s: send %s cause #%d (0x%02x) to layer3\n",
				CAPIobjIDstr(&plci->cobj), _mi_msg_type2str(MT_RELEASE_COMPLETE), cause, cause);
			if (!mi_encode_cause(l3m, cause, CAUSE_LOC_USER, 0, NULL)) {
				ret = pc->l3->to_layer3(pc->l3, MT_RELEASE_COMPLETE, plci->cobj.id2, l3m);
				if (ret) {
					wprint("%s: Error %d -  %s on sending %s to pid %x\n", CAPIobjIDstr(&plci->cobj), ret,
						strerror(-ret), _mi_msg_type2str(MT_RELEASE_COMPLETE), plci->cobj.id2);
					free_l3_msg(l3m);
				}
			}
		} else
			eprint("%s: cannot allocate l3 message plci\n", CAPIobjIDstr(&plci->cobj));

		cleanup_mPLCI(plci);
	}
}

int plci_l3l4(struct mPLCI *plci, int pr, struct l3_msg *l3m)
{
	struct mc_buf *mc;
	struct mCAPIobj *co;

	mc = alloc_mc_buf();
	if (!mc) {
		wprint("%s: Cannot allocate mc_buf for %s\n", CAPIobjIDstr(&plci->cobj), _mi_msg_type2str(pr));
		return -ENOMEM;
	}
	mc->l3m = l3m;
	switch (pr) {
	case MT_SETUP:
		plciHandleSetupInd(plci, pr, mc);
		break;
	default:
		co = get_next_cobj(&plci->cobj, NULL);
		while (co) {
			lPLCI_l3l4(container_of(co, struct lPLCI, cobj), pr, mc);
			co = get_next_cobj(&plci->cobj, co);
		}
		break;
	}
	free_mc_buf(mc);
	return 0;
}

int mPLCISendMessage(struct lController *lc, struct mc_buf *mc)
{
	struct mPLCI *plci;
	struct lPLCI *lp;
	int ret;
	struct pController *pc;

	pc = p4lController(lc);
	switch (mc->cmsg.Command) {
	case CAPI_CONNECT:
		plci = new_mPLCI(pc, 0);
		if (plci) {
			ret = lPLCICreate(&lp, lc, plci);
			if (!ret) {
				ret = lPLCISendMessage(lp, mc);
				put_cobj(&lp->cobj);
			} else {
				wprint("%s: cannot create lPLCI Appl-%03d", CAPIobjIDstr(&plci->cobj), lc->cobj.id2);
				ret = CapiMsgOSResourceErr;
			}
			put_cobj(&plci->cobj);
		} else {
			wprint("Cannot create PLCI for controller %d\n", pc->profile.ncontroller);
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
	struct mCAPIobj *co;

	if (!plci)
		return NULL;
	co = get_next_cobj(&plci->cobj, NULL);
	while (co) {
		if (appId == co->id2)
			break;
		co = get_next_cobj(&plci->cobj, co);
	}
	return co ? container_of(co, struct lPLCI, cobj) : NULL;
}

struct mPLCI *getPLCI4pid(struct pController *pc, int pid)
{
	struct mCAPIobj *co;

	co = get_next_cobj(&pc->cobjPLCI, NULL);
	while (co) {
		if (co->id2 == pid)
			break;
		co = get_next_cobj(&pc->cobjPLCI, co);
	}
	return co ? container_of(co, struct mPLCI, cobj) : NULL;
}

struct mPLCI *getPLCI4Id(struct pController *pc, uint32_t id)
{
	struct mCAPIobj *co;

	co = get_next_cobj(&pc->cobjPLCI, NULL);
	while (co) {
		if (co->id == id)
			break;
		co = get_next_cobj(&pc->cobjPLCI, co);
	}
	return co ? container_of(co, struct mPLCI, cobj) : NULL;
}

int plciL4L3(struct mPLCI *plci, int mt, struct l3_msg *l3m)
{
	int ret;

	ret = plci->pc->l3->to_layer3(plci->pc->l3, mt, plci->cobj.id2, l3m);
	if (ret < 0) {
		wprint("%s: Error sending %s to controller %d pid %x %s msg\n", CAPIobjIDstr(&plci->cobj), _mi_msg_type2str(mt),
			plci->pc->profile.ncontroller, plci->cobj.id2, l3m ? "with" : "no");
		if (l3m)
			free_l3_msg(l3m);
	}
	dprint(MIDEBUG_PLCI, "%s: Sending %s to layer3 %s msg\n",
		CAPIobjIDstr(&plci->cobj), _mi_msg_type2str(mt), l3m ? "with" : "no");
	return ret;
}

unsigned int plci_new_pid(struct mPLCI *plci)
{
	return request_new_pid(plci->pc->l3);
}

void release_lController(struct lController *lc)
{
	struct mCAPIobj *cop, *colp;
	struct lPLCI *lp;
	struct pController *pc = p4lController(lc);

	if (pc) {
		cop = get_next_cobj(&pc->cobjPLCI, NULL);
		while (cop) {
			colp = get_next_cobj(cop, NULL);
			while (colp) {
				lp = container_of(colp, struct lPLCI, cobj);
				if (lc == lp->lc) {
					if (!lp->rel_req) {
						dprint(MIDEBUG_PLCI, "%s do release\n", CAPIobjIDstr(colp));
						lPLCIRelease(lp);
					} else
						dprint(MIDEBUG_PLCI, "%s: release already done\n", CAPIobjIDstr(colp));
				}
				colp = get_next_cobj(cop, colp);
			}
			cop = get_next_cobj(&pc->cobjPLCI, cop);
		}
	}
}
