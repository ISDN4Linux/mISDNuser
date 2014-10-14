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

#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "m_capi.h"
#include "mc_buffer.h"

static struct mCAPIobj AppRoot;

/* not in capi header files yet */
void capi_freeapplid(unsigned);

int mApplication_init(void)
{
	int ret;

	memset(&AppRoot, 0, sizeof(AppRoot));
	ret = init_cobj(&AppRoot, NULL, Cot_Root, 0, 0);
	return ret;
}

static void app_sendcontrol(struct mApplication *appl, int cmd)
{
	int ret;

	if (appl->cpipe[1] > 0) {
		ret = write(appl->cpipe[1], &cmd, sizeof(cmd));
		if (ret < sizeof(cmd))
			eprint("%s: refcount %d cannot write cmd=%x to controlpipe(%d) ret=%d - %s\n",
				CAPIobjIDstr(&appl->cobj), appl->cobj.refcnt, cmd, appl->cpipe[1], ret, strerror(errno));
	} else
		eprint("%s: refcount %d cannot write cmd=%x - control pipe closed\n",
			CAPIobjIDstr(&appl->cobj), appl->cobj.refcnt, cmd);
}

struct mApplication *RegisterApplication(uint16_t ApplId, uint32_t MaxB3Connection, uint32_t MaxB3Blks, uint32_t MaxSizeB3)
{
	struct mApplication *appl;
	int ret;

	appl = calloc(1, sizeof(*appl));
	if (appl) {
		appl->lcl = calloc(mI_ControllerCount, sizeof(void *));
		if (appl->lcl) {
			appl->cobj.id2 = ApplId;
			ret = init_cobj_registered(&appl->cobj, &AppRoot, Cot_Application, 0);
			if (ret) {
				eprint("Appl %d: Error on init CapiObj - %s\n", ApplId, strerror(ret));
				free(appl->lcl);
				free(appl);
				appl = NULL;
			} else {
				if (ret) {
					wprint("Application %d already registered\n", ApplId);
					put_cobj(&AppRoot);
					free(appl->lcl);
					free(appl);
					appl = NULL;
				} else {
					appl->MaxB3Con = MaxB3Connection;
					appl->MaxB3Blk = MaxB3Blks;
					appl->MaxB3Size = MaxSizeB3;
					appl->cpipe[0] = -1;
					appl->cpipe[1] = -1;
				}
			}
		} else {
			eprint("Appl %d: No memory for lController array\n", ApplId);
			free(appl);
			appl = NULL;
		}
	} else {
		eprint("Appl %d: No memory for application (%zd bytes)\n", ApplId, sizeof(*appl));
	}
	return appl;
}

int register_lController(struct mApplication *appl, struct lController *lc)
{
	unsigned int i;

	i = lc->cobj.id - 1;
	if (i >= mI_ControllerCount) {
		eprint("%s: Register invalid controller ID:%d\n", CAPIobjIDstr(&appl->cobj), lc->cobj.id);
		return -EINVAL;
	}
	if (appl->lcl[i]) {
		eprint("%s: controller idx %d ID:%d\already registered\n", CAPIobjIDstr(&appl->cobj), i, lc->cobj.id);
		return -EBUSY;
	}
	if (get_cobj(&lc->cobj)) {
		appl->lcl[i] = lc;
	} else {
		eprint("%s: controller idx %d cannot get controller object %s\n", CAPIobjIDstr(&appl->cobj), i, CAPIobjIDstr(&lc->cobj));
		return -EINVAL;
	}
	return 0;
}

/*
 * Release the Application
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
void ReleaseApplication(struct mApplication *appl, int unregister)
{
	int ret;
	unsigned int i;

	pthread_rwlock_wrlock(&appl->cobj.lock);
	if (appl->cobj.cleaned) {
		pthread_rwlock_unlock(&appl->cobj.lock);
		wprint("%s: already cleaned\n", CAPIobjIDstr(&appl->cobj));
		return;
	} else
		appl->cobj.cleaned = 1;

	appl->unregistered = unregister;

	if (appl->cpipe[0] > -1 && appl->cpipe[1] > -1) {
		wprint("%s appl->cpipe(%d, %d) still open - reuse fds\n", CAPIobjIDstr(&appl->cobj),  appl->cpipe[0], appl->cpipe[1]);
	} else {
		ret = pipe2(appl->cpipe, O_NONBLOCK);
		if (ret)
			eprint("%s: Cannot open control pipe - %s\n", CAPIobjIDstr(&appl->cobj), strerror(errno));
		else
			dprint(MIDEBUG_CONTROLLER, "create appl->cpipe(%d, %d)\n", appl->cpipe[0], appl->cpipe[1]);
	}
	dprint(MIDEBUG_CONTROLLER, "close appl->fd %d\n", appl->fd);
	close(appl->fd);
	appl->fd = -1;

	pthread_rwlock_unlock(&appl->cobj.lock);

	/* Signal assigned logical controllers Application is gone */
	for (i = 0; i < mI_ControllerCount; i++) {
		if (appl->lcl[i]) {
			release_lController(appl->lcl[i]);
			cleanup_lController(appl->lcl[i]);
		}
	}
	dprint(MIDEBUG_CAPIMSG, "%s: cleaning done refcnt:%d\n", CAPIobjIDstr(&appl->cobj), appl->cobj.refcnt);
	app_sendcontrol(appl, MI_PUT_APPLICATION);
}

int ReleaseAllApplications(void)
{
	struct mApplication *appl;
	struct mCAPIobj *co;
	int ret, cnt = 0, fd;

	co = get_next_cobj(&AppRoot, NULL);
	while (co) {
		appl = container_of(co, struct mApplication, cobj);
		fd = appl->fd;
		ReleaseApplication(appl, 0);
		ret = mIcapi_mainpoll_releaseApp(fd, appl->cpipe[0]);
		if (ret < 0)
			eprint("%s mainpoll not released\n", CAPIobjIDstr(&appl->cobj));
		co = get_next_cobj(&AppRoot, co);
		cnt++;
	}
	return cnt;
}

void Free_Application(struct mCAPIobj *co)
{
	unsigned int i;
	struct mApplication *appl = container_of(co, struct mApplication, cobj);
	struct lController *lc;

	delist_cobj(&appl->cobj);
	if (appl->lcl) {
		for (i = 0; i < mI_ControllerCount; i++) {
			lc = appl->lcl[i];
			appl->lcl[i] = NULL;
			if (lc) {
				release_lController(lc);
				Free_lController(&lc->cobj);
			}
		}
	}
	if (!appl->unregistered) /* filedescriptor was closed */
		capi_freeapplid(appl->cobj.id2);
	dprint(MIDEBUG_CONTROLLER, "close appl->fd %d\n", appl->fd);
	if (appl->fd > 0)
		close(appl->fd);
	appl->fd = -1;
	dprint(MIDEBUG_CONTROLLER, "close appl->cpipe(%d, %d)\n", appl->cpipe[0], appl->cpipe[1]);
	if (appl->cpipe[1] > 0)
		close(appl->cpipe[1]);
	appl->cpipe[1] = -1;
	if (appl->cpipe[0] > 0)
		close(appl->cpipe[0]);
	appl->cpipe[0] = -1;

	put_cobj(appl->cobj.parent);
	appl->cobj.parent = NULL;
	iprint("%s: refcnt=%d freed\n", CAPIobjIDstr(&appl->cobj), appl->cobj.refcnt);
	pthread_rwlock_destroy(&appl->cobj.lock);
	if (appl->lcl)
		free(appl->lcl);
	appl->lcl = NULL;
	free_capiobject(&appl->cobj, appl);
}

void dump_applications(void)
{
	struct mApplication *ap;
	struct mCAPIobj *co;
	unsigned int i;

	if (pthread_rwlock_tryrdlock(&AppRoot.lock)) {
		wprint("Cannot read lock application list for dumping\n");
		return;
	}
	co = AppRoot.listhead;
	while (co) {
		ap = container_of(co, struct mApplication, cobj);
		iprint("%s: MaxB3Con:%d MaxB3Blk:%d MaxB3Size:%d\n", CAPIobjIDstr(&ap->cobj),
			ap->MaxB3Con, ap->MaxB3Blk, ap->MaxB3Size);
		iprint("%s: Refs:%d cleaned:%s unregistered:%s cpipe(%d, %d)\n", CAPIobjIDstr(&ap->cobj),
			ap->cobj.refcnt, ap->cobj.cleaned ? "yes" : "no", ap->unregistered ? "yes" : "no",
			ap->cpipe[0], ap->cpipe[1]);
		for (i = 0; i < mI_ControllerCount; i++) {
			if (ap->lcl[i]) {
				dump_lcontroller(ap->lcl[i]);
				ap->lcl[i]->listed = 1;
			}
		}
		co = co->next;
	}
	pthread_rwlock_unlock(&AppRoot.lock);
}

void Put_Application_cleaned(struct mCAPIobj *co)
{
	struct mApplication *appl = container_of(co, struct mApplication, cobj);

	if (appl->cobj.cleaned && appl->cpipe[1] > 0)
		app_sendcontrol(appl, MI_PUT_APPLICATION);
}

void delisten_application(struct lController *lc)
{
	unsigned int i;
	struct mApplication *appl;

	appl = lc->Appl;
	if (!appl) {
		wprint("Appl not linked\n");
		return;
	}
	lc->Appl = NULL;
	i = lc->cobj.id;
	i--;
	if (i <  mI_ControllerCount) {
		if (appl->lcl[i])
			 put_cobj(&lc->cobj);
		appl->lcl[i] = NULL;
	}
	put_cobj(&appl->cobj);
}

struct lController *get_lController(struct mApplication *appl, unsigned int cont)
{
	struct lController *lc;

	if (cont > 0 && cont <= mI_ControllerCount)
		lc = appl->lcl[cont - 1];
	else {
		wprint("%s: wrong controller id %d (max %d)\n", CAPIobjIDstr(&appl->cobj), cont, mI_ControllerCount);
		lc = NULL;
	}
	if (lc) {
		if (!get_cobj(&lc->cobj)) {
			wprint("%s: cannot get controller object %s\n", CAPIobjIDstr(&appl->cobj), CAPIobjIDstr(&lc->cobj));
			lc = NULL;
		}
	}
	return lc;
}

static struct lController *find_lController(struct mApplication *appl, unsigned int cont)
{
	struct lController *lc;

	if (cont > 0 && cont <= mI_ControllerCount)
		lc = appl->lcl[cont - 1];
	else {
		wprint("%s: wrong controller id %d (max %d)\n", CAPIobjIDstr(&appl->cobj), cont, mI_ControllerCount);
		lc = NULL;
	}
	return lc;
}

void SendMessage2Application(struct mApplication *appl, struct mc_buf *mc)
{
	int ret;

	if (mI_debug_mask & MIDEBUG_CAPIMSG)
		 mCapi_message2str(mc);
	ret = send(appl->fd, mc->rb, mc->len, 0);
	if (ret != mc->len)
		wprint("Message send error len=%d ret=%d - %s\n", mc->len, ret, strerror(errno));
}

void SendCmsg2Application(struct mApplication *appl, struct mc_buf *mc)
{
	int ret;

	if (appl->cobj.cleaned || appl->fd < 0) {
		/* Application is gone so we need answer INDICATIONS to avoid blocking the state machine */
		wprint("%s: Cannot send %s to released application\n", CAPIobjIDstr(&appl->cobj),
			capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand));
		if (mc->cmsg.Subcommand != CAPI_IND)
			return;
		switch(mc->cmsg.Command) {
		// for NCCI state machine
		case CAPI_CONNECT_B3:
			mc->cmsg.Reject = 2;
		case CAPI_CONNECT_B3_ACTIVE:
		case CAPI_DISCONNECT_B3:
			break;
		// for PLCI state machine
		case CAPI_CONNECT:
			mc->cmsg.Reject = 2;
		case CAPI_CONNECT_ACTIVE:
		case CAPI_DISCONNECT:
			break;
		case CAPI_FACILITY:
		case CAPI_MANUFACTURER:
		case CAPI_INFO:
			wprint("%s %s ignored\n", CAPIobjIDstr(&appl->cobj),
				capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand));
			return;
		default:
			wprint("%s: %s not handled\n", CAPIobjIDstr(&appl->cobj),
				capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand));
			return;
		}
		capi20_cmsg_answer(&mc->cmsg);
		capi_cmsg2message(&mc->cmsg, mc->rb);
		mc->len = CAPIMSG_LEN(mc->rb);
		mc->refcnt++; /* The message is reused, so increment the refcnt to allow double free */
		dprint(MIDEBUG_CONTROLLER, "%s: sent emulated answer %s to PutMessageApplication\n",
			CAPIobjIDstr(&appl->cobj), capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand));
		ret = PutMessageApplication(appl, mc);
		if (ret)
			dprint(MIDEBUG_CONTROLLER, "%s: sent emulated answer %s to PutMessageApplication returned=%d\n",
				CAPIobjIDstr(&appl->cobj), capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand), ret);
	} else {
		capi_cmsg2message(&mc->cmsg, mc->rb);
		mc->len = CAPIMSG_LEN(mc->rb);
		if (mI_debug_mask & MIDEBUG_CAPIMSG)
			mCapi_message2str(mc);
		ret = send(appl->fd, mc->rb, mc->len, 0);
		if (ret != mc->len)
			eprint("Message send error len=%d ret=%d - %s\n", mc->len, ret, strerror(errno));
	}
}

void SendCmsgAnswer2Application(struct mApplication *appl, struct mc_buf *mc, __u16 Info)
{
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = Info;
	SendCmsg2Application(appl, mc);
}

struct lPLCI *get_lPLCI4plci(struct mApplication *appl, uint32_t id)
{
	struct lPLCI *lp = NULL;;
	struct lController *lc;
	struct mPLCI *plci;

	lc = find_lController(appl, id & 0x7f);
	if (lc) {
		plci = getPLCI4Id(p4lController(lc), id & 0xFFFF);
		if (plci) {
			lp = get_lPLCI4Id(plci, appl->cobj.id2);
			put_cobj(&plci->cobj);
		}
	}
	return lp;
}

#define CapiFacilityNotSupported		0x300b

static int FacilityMessage(struct mApplication *appl, struct pController *pc, struct mc_buf *mc)
{
	int ret = CapiNoError;
	struct mPLCI *plci;
	struct lPLCI *lp;
	struct BInstance *bi;
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
		if (plci)
			put_cobj(&plci->cobj);
		bi = lp ? lp->BIlink : NULL;
		if  (bi) {
			ret = bi->from_up(bi, mc);
		} else {
			wprint("DTMF addr %06x lPLCI not found\n", mc->cmsg.adr.adrNCCI);
			ret = CapiIllController;
		}
		if (lp)
			put_cobj(&lp->cobj);
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
	unsigned int id;
	struct pController *pc;
	struct lController *lc;
	struct mPLCI *plci = NULL;
	struct lPLCI *lp = NULL;
	struct BInstance *bi;
	uint8_t cmd, subcmd;
	int ret = CapiNoError;

	cmd = CAPIMSG_COMMAND(mc->rb);
	subcmd = CAPIMSG_SUBCOMMAND(mc->rb);
	if (cmd != CAPI_DATA_B3 && mI_debug_mask & MIDEBUG_CAPIMSG)
		mCapi_message2str(mc);
	if (mc->len < 12) {
		eprint("message %02x/%02x %s too short (%d)\n", cmd, subcmd, capi20_cmd2str(cmd, subcmd), mc->len);
		ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		SendCmsgAnswer2Application(appl, mc, ret);
		return ret;
	}
	id = CAPIMSG_CONTROL(mc->rb);
	lc = get_lController(appl, id & 0x7f);
	if (lc)
		pc = p4lController(lc);
	else
		pc = get_cController(id & 0x7f);
	if (!pc) {
		eprint("message %x controller for id %06x not found\n", cmd, id);
	}
	dprint(MIDEBUG_CONTROLLER, "%s: ID:%06x cmd %02x/%02x %s\n", CAPIobjIDstr(&appl->cobj),
		id, cmd, subcmd, capi20_cmd2str(cmd, subcmd));
	switch (cmd) {
		// for NCCI state machine
	case CAPI_DATA_B3:
	case CAPI_CONNECT_B3_ACTIVE:
	case CAPI_RESET_B3:
		mcbuf_rb2cmsg(mc);
		if ((subcmd == CAPI_REQ) || (subcmd == CAPI_RESP)) {
			plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI & 0xFFFF);
			lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
			bi = lp ? lp->BIlink : NULL;
			if (bi) {
				ret = bi->from_up(bi, mc);
			} else {
				wprint("%s: cmd %x (%s) %s  %s  BIlink not found\n", CAPIobjIDstr(&appl->cobj), cmd,
					capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand),
					plci ? CAPIobjIDstr(&plci->cobj) : "no plci",
					lp ? CAPIobjIDstr(&lp->cobj) : "no lplci");
				ret = CapiIllController;
			}
		} else
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case CAPI_DISCONNECT_B3:
		mcbuf_rb2cmsg(mc);
		if ((subcmd == CAPI_REQ) || (subcmd == CAPI_RESP)) {
			plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI & 0xFFFF);
			lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
			bi = lp ? lp->BIlink : NULL;
			if (bi) {
				ret = bi->from_up(bi, mc);
			} else if (subcmd == CAPI_RESP) {
				dprint(MIDEBUG_CONTROLLER, "%s: cmd %x (%s) %s  %s  BIlink already gone - OK\n", CAPIobjIDstr(&appl->cobj), cmd,
					capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand),
					plci ? CAPIobjIDstr(&plci->cobj) : "no plci",
					lp ? CAPIobjIDstr(&lp->cobj) : "no lplci");
				ret = 1; /* free msg in calling function main_recv() */
			} else {
				ret = CapiIllController;
			}
		} else
			ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case CAPI_CONNECT_B3:
		mcbuf_rb2cmsg(mc);
		plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI & 0xFFFF);
		lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
		bi = lp ? lp->BIlink : NULL;
		if (bi) {
			ret = bi->from_up(bi, mc);
		} else {
			wprint("%s: cmd %x (%s) %s  %s  BIlink not found\n", CAPIobjIDstr(&appl->cobj), cmd,
				capi20_cmd2str(mc->cmsg.Command, mc->cmsg.Subcommand),
				plci ? CAPIobjIDstr(&plci->cobj) : "no plci",
				lp ? CAPIobjIDstr(&lp->cobj) : "no lplci");
			ret = CapiIllController;
		}
		break;
		// for PLCI state machine
	case CAPI_CONNECT:
	case CAPI_INFO:
		mcbuf_rb2cmsg(mc);
		plci = getPLCI4Id(pc, mc->cmsg.adr.adrPLCI);
		dprint(MIDEBUG_PLCI, "%s adrPLCI %06x plci:%04x ApplId %d\n", capi20_cmd2str(cmd, subcmd), mc->cmsg.adr.adrPLCI,
		       plci ? plci->cobj.id : 0xffff, mc->cmsg.ApplId);
		if (subcmd == CAPI_REQ) {
			if (plci) {
				lp = get_lPLCI4Id(plci, mc->cmsg.ApplId);
				if (lp)
					ret = lPLCISendMessage(lp, mc);
				else {
					wprint("%s adrPLCI %06x plci:%04x ApplId %d no plci found\n", capi20_cmd2str(cmd, subcmd),
					       mc->cmsg.adr.adrPLCI, plci ? plci->cobj.id : 0xffff, mc->cmsg.ApplId);
					ret = CapiIllController;
				}
			} else {
				if (!lc) {
					if (pc) {
						lc = addlController(appl, pc, 1);
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
				       mc->cmsg.adr.adrPLCI, plci ? plci->cobj.id : 0xffff, mc->cmsg.ApplId);
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
			       plci ? plci->cobj.id : 0xffff, mc->cmsg.ApplId, lp);
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
				lc = addlController(appl, pc, 0);
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
	if (lp)
		put_cobj(&lp->cobj);
	if (plci)
		put_cobj(&plci->cobj);
	if (lc)
		put_cobj(&lc->cobj);
	return ret;
}

void mCapi_cmsg2str(struct mc_buf *mc)
{
	char *decmsg, *line;

	if (mI_debug_mask & MIDEBUG_CAPIMSG) {
		decmsg = capi_cmsg2str(&mc->cmsg);
		while (decmsg) {
			line = strsep(&decmsg, "\n");
			if (line)
				dprint(MIDEBUG_CAPIMSG, "%s\n", line);
		}
	}
}

void mCapi_message2str(struct mc_buf *mc)
{
	char *decmsg, *line;

	if (mI_debug_mask & MIDEBUG_CAPIMSG) {
		decmsg = capi_message2str(mc->rb);
		while (decmsg) {
			line = strsep(&decmsg, "\n");
			if (line)
				dprint(MIDEBUG_CAPIMSG, "%s\n", line);
		}
	}
}
