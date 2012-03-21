/* $Id: listen.c,v 1.8 2004/01/26 22:21:30 keil Exp $
 *
 */

#include "m_capi.h"
#include "../lib/include/helper.h"

// --------------------------------------------------------------------
// LISTEN state machine

enum {
	ST_LISTEN_L_0,
	ST_LISTEN_L_0_1,
	ST_LISTEN_L_1,
	ST_LISTEN_L_1_1,
} const ST_LISTEN_COUNT = ST_LISTEN_L_1_1 + 1;

static char *str_st_listen[] = {
	"ST_LISTEN_L_0",
	"ST_LISTEN_L_0_1",
	"ST_LISTEN_L_1",
	"ST_LISTEN_L_1_1",
};

enum {
	EV_LISTEN_REQ,
	EV_LISTEN_CONF,
} const EV_LISTEN_COUNT = EV_LISTEN_CONF + 1;

static char *str_ev_listen[] = {
	"EV_LISTEN_REQ",
	"EV_LISTEN_CONF",
};

static struct Fsm listen_fsm = { 0, 0, 0, 0, 0 };

static void listen_debug(struct FsmInst *fi, char *fmt, ...)
{
	char tmp[128];
	char *p = tmp;
	va_list args;
	struct lController *lc = fi->userdata;

	if (!fi->debug)
		return;
	va_start(args, fmt);
	p += sprintf(p, "Controller%d ApplId %d listen ", lc->Contr->profile.ncontroller, lc->Appl->AppId);
	p += vsprintf(p, fmt, args);
	*p = 0;
	dprint(MIDEBUG_STATES, "%s\n", tmp);
	va_end(args);
}

static void listen_req_l_x(struct FsmInst *fi, int event, void *arg, int state)
{
	struct lController *lc = fi->userdata;
	struct mc_buf *mc = arg;

	FsmChangeState(fi, state);

	dprint(MIDEBUG_CONTROLLER, "Controller%d: lc=%p nC=%p nA=%p\n",
		lc->Contr->profile.ncontroller, lc, lc->nextC, lc->nextA);
	dprint(MIDEBUG_CONTROLLER, "Controller%d: set InfoMask %08x -> %08x\n",
	       lc->Contr->profile.ncontroller, lc->InfoMask, mc->cmsg.InfoMask);
	dprint(MIDEBUG_CONTROLLER, "Controller%d: set CIPmask %08x -> %08x\n",
	       lc->Contr->profile.ncontroller, lc->CIPmask, mc->cmsg.CIPmask);
	dprint(MIDEBUG_CONTROLLER, "Controller%d: set CIPmask2 %08x -> %08x\n",
	       lc->Contr->profile.ncontroller, lc->CIPmask2, mc->cmsg.CIPmask2);
	lc->InfoMask = mc->cmsg.InfoMask;
	lc->CIPmask = mc->cmsg.CIPmask;
	lc->CIPmask2 = mc->cmsg.CIPmask2;
	ListenController(lc->Contr);
	capi_cmsg_answer(&mc->cmsg);
	mc->cmsg.Info = CapiNoError;
	FsmEvent(&lc->listen_m, EV_LISTEN_CONF, mc);
}

static void listen_req_l_0(struct FsmInst *fi, int event, void *arg)
{
	listen_req_l_x(fi, event, arg, ST_LISTEN_L_0_1);
}

static void listen_req_l_1(struct FsmInst *fi, int event, void *arg)
{
	listen_req_l_x(fi, event, arg, ST_LISTEN_L_1_1);
}

static void listen_conf_l_x_1(struct FsmInst *fi, int event, void *arg, int state)
{
	struct lController *lc = fi->userdata;
	struct mc_buf *mc = arg;

	if (mc->cmsg.Info != CapiNoError) {
		FsmChangeState(fi, state);
	} else {		// Info == 0
		if (lc->CIPmask == 0) {
			FsmChangeState(fi, ST_LISTEN_L_0);
		} else {
			FsmChangeState(fi, ST_LISTEN_L_1);
		}
	}
	SendCmsg2Application(lc->Appl, mc);
}

static void listen_conf_l_0_1(struct FsmInst *fi, int event, void *arg)
{
	listen_conf_l_x_1(fi, event, arg, ST_LISTEN_L_0);
}

static void listen_conf_l_1_1(struct FsmInst *fi, int event, void *arg)
{
	listen_conf_l_x_1(fi, event, arg, ST_LISTEN_L_1);
}

static struct FsmNode fn_listen_list[] = {
	{ST_LISTEN_L_0, EV_LISTEN_REQ, listen_req_l_0},
	{ST_LISTEN_L_0_1, EV_LISTEN_CONF, listen_conf_l_0_1},
	{ST_LISTEN_L_1, EV_LISTEN_REQ, listen_req_l_1},
	{ST_LISTEN_L_1_1, EV_LISTEN_CONF, listen_conf_l_1_1},
};

const int FN_LISTEN_COUNT = sizeof(fn_listen_list) / sizeof(struct FsmNode);

struct lController *addlController(struct mApplication *app, struct pController *pc, int openl3)
{
	struct lController *lc, *old;

	if (openl3) {
		if (OpenLayer3(pc)) {
			eprint("Controller%d: Application %d - cannot open L3 instance\n", pc->profile.ncontroller, app->AppId);
			return NULL;
		}
	}
	lc = calloc(1, sizeof(*lc));
	if (lc) {
		lc->Appl = app;
		lc->Contr = pc;
		lc->listen_m.fsm = &listen_fsm;
		lc->listen_m.state = ST_LISTEN_L_0;
		lc->listen_m.debug = MIDEBUG_CONTROLLER & mI_debug_mask;
		lc->listen_m.userdata = lc;
		lc->listen_m.printdebug = listen_debug;
		lc->InfoMask = 0;
		lc->CIPmask = 0;
		lc->CIPmask2 = 0;
		old = pc->lClist;
		while (old && old->nextC)
			old = old->nextC;
		if (old)
			old->nextC = lc;
		else
			pc->lClist = lc;
		old = app->contL;
		while (old && old->nextA)
			old = old->nextA;
		if (old)
			old->nextA = lc;
		else
			app->contL = lc;
	} else
		eprint("Controller%d: Application %d - no memory for lController\n", pc->profile.ncontroller, app->AppId);
	return lc;
}

void rm_lController(struct lController *lc)
{
	struct lController *cur, *old;

	if (lc->Contr) {
		cur = lc->Contr->lClist;
		old = cur;
		while (cur) {
			if (cur == lc) {
				old->nextC = cur->nextC;
				break;
			}
			old = cur;
			cur = cur->nextC;
		}
		if (lc == lc->Contr->lClist)
			lc->Contr->lClist = lc->nextC;
	}
	free(lc);
}

int listenRequest(struct lController *lc, struct mc_buf *mc)
{
	FsmEvent(&lc->listen_m, EV_LISTEN_REQ, mc);
	free_mc_buf(mc);
	return CapiNoError;
}

int listenHandle(struct lController *lc, uint16_t CIPValue)
{
	if ((lc->CIPmask & 1) || (lc->CIPmask & (1 << CIPValue)))
		return 1;
	return 0;
}

void init_listen(void)
{
	listen_fsm.state_count = ST_LISTEN_COUNT;
	listen_fsm.event_count = EV_LISTEN_COUNT;
	listen_fsm.strEvent = str_ev_listen;
	listen_fsm.strState = str_st_listen;

	FsmNew(&listen_fsm, fn_listen_list, FN_LISTEN_COUNT);
}

void free_listen(void)
{
	FsmFree(&listen_fsm);
}
