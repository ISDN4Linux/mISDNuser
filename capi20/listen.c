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

static const char *str_st_listen[] = {
	"ST_LISTEN_L_0",
	"ST_LISTEN_L_0_1",
	"ST_LISTEN_L_1",
	"ST_LISTEN_L_1_1",
};

enum {
	EV_LISTEN_REQ,
	EV_LISTEN_CONF,
} const EV_LISTEN_COUNT = EV_LISTEN_CONF + 1;

static const char *str_ev_listen[] = {
	"EV_LISTEN_REQ",
	"EV_LISTEN_CONF",
};

static struct Fsm listen_fsm = { 0, 0, 0, 0, 0 };

static void listen_debug(struct FsmInst *fi, const char *fmt, ...)
{
	char tmp[128];
	va_list args;
	struct lController *lc = fi->userdata;

	if (!fi->debug)
		return;
	va_start(args, fmt);
	vsnprintf(tmp, 128, fmt, args);
	dprint(MIDEBUG_STATES, "%s: listen %s\n", CAPIobjIDstr(&lc->cobj), tmp);
	va_end(args);
}

static void listen_req_l_x(struct FsmInst *fi, int event, void *arg, int state)
{
	struct lController *lc = fi->userdata;
	struct mc_buf *mc = arg;
	const char *ids;

	FsmChangeState(fi, state);
	ids = CAPIobjIDstr(&lc->cobj);
	dprint(MIDEBUG_CONTROLLER, "%s: set InfoMask %08x -> %08x\n", ids, lc->InfoMask, mc->cmsg.InfoMask);
	dprint(MIDEBUG_CONTROLLER, "%s: set CIPmask %08x -> %08x\n", ids, lc->CIPmask, mc->cmsg.CIPmask);
	dprint(MIDEBUG_CONTROLLER, "%s: set CIPmask2 %08x -> %08x\n", ids, lc->CIPmask2, mc->cmsg.CIPmask2);
	lc->InfoMask = mc->cmsg.InfoMask;
	lc->CIPmask = mc->cmsg.CIPmask;
	lc->CIPmask2 = mc->cmsg.CIPmask2;
	ListenController(p4lController(lc));
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
	struct lController *lc;
	int ret;

	if (openl3) {
		if (OpenLayer3(pc)) {
			eprint("Controller%d: Application %d - cannot open L3 instance\n", pc->profile.ncontroller, app->cobj.id2);
			return NULL;
		}
	}
	lc = calloc(1, sizeof(*lc));
	if (lc) {
		lc->cobj.id2 = app->cobj.id2;
		if (!get_cobj(&app->cobj)) {
			eprint("Cannot get application object\n");
			free(lc);
			return NULL;
		}
		ret = init_cobj_registered(&lc->cobj, &pc->cobjLC, Cot_lController, 0x0000ff);
		if (ret) {
			eprint("Controller%d: Application %d - cannot init\n", pc->profile.ncontroller, app->cobj.id2);
			put_cobj(&app->cobj);
			free(lc);
			lc = NULL;
		} else {
			lc->Appl = app;
			lc->listen_m.fsm = &listen_fsm;
			lc->listen_m.state = ST_LISTEN_L_0;
			lc->listen_m.debug = MIDEBUG_CONTROLLER & mI_debug_mask;
			lc->listen_m.userdata = lc;
			lc->listen_m.printdebug = listen_debug;
			lc->InfoMask = 0;
			lc->CIPmask = 0;
			lc->CIPmask2 = 0;
			ret = register_lController(app, lc);
			if (ret) {
				lc->Appl = NULL;
				put_cobj(&app->cobj);
				eprint("Controller%d: - cannot register LC on Application %d - %s\n", pc->profile.ncontroller,
					app->cobj.id2, strerror(-ret));
				lc->cobj.cleaned = 1;
				delist_cobj(&lc->cobj);
				put_cobj(&lc->cobj);
				lc = NULL;
			}
		}
	} else
		eprint("Controller%d: Application %d - no memory for lController\n", pc->profile.ncontroller, app->cobj.id2);
	return lc;
}

void dump_lControllers(struct pController *pc)
{
	struct mCAPIobj *co;
	struct lController *lc;

	if (pthread_rwlock_tryrdlock(&pc->cobjLC.lock)) {
		wprint("Cannot read lock LC list for dumping\n");
		return;
	}
	co = pc->cobjLC.listhead;
	while (co) {
		lc = container_of(co, struct lController, cobj);
		if (lc->listed)
			lc->listed = 0;
		else
			dump_lcontroller(lc);
		co = co->next;
	}
	pthread_rwlock_unlock(&pc->cobjLC.lock);
}

void cleanup_lController(struct lController *lc)
{
	struct pController *pc = p4lController(lc);

	dprint(MIDEBUG_CONTROLLER, "%s: cleaning now refcnt %d (%scleaned)\n", CAPIobjIDstr(&lc->cobj),
		lc->cobj.refcnt, lc->cobj.cleaned ? "" : "not ");
	if (lc->cobj.cleaned) {
		return;
	}
	lc->cobj.cleaned = 1;
	delisten_application(lc);
	delist_cobj(&lc->cobj);
	if (pc)
		ListenController(pc);
}

void Free_lController(struct mCAPIobj *co)
{
	struct lController *lc = container_of(co, struct lController, cobj);
	struct pController *pc = p4lController(lc);

	if (lc->Appl)
		delisten_application(lc);
	if (pc) {
		co->cleaned = 1;
		if (co->parent) {
			delist_cobj(co);
			put_cobj(co->parent);
			co->parent = NULL;
		}
		/* update controller masks */
		ListenController(pc);
	}
	dprint(MIDEBUG_CONTROLLER, "%s: freeing done\n", CAPIobjIDstr(co));
	free_capiobject(co, lc);
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

void dump_lcontroller(struct lController *lc)
{
	iprint("%s: Refs:%d state:%s Info:%08x CIP:%08x CIP2:%08x\n", CAPIobjIDstr(&lc->cobj), lc->cobj.refcnt,
		str_st_listen[lc->listen_m.state], lc->InfoMask, lc->CIPmask, lc->CIPmask2);
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
