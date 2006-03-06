#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "isdn_net.h"
#include "l3dss1.h"
#include "net_l2.h"
#include "net_l3.h"
#include "bchannel.h"
#include "helper.h"

int
match_nr(manager_t *mgr, unsigned char *nx, nr_list_t **nrx)
{
	int		l,i,ret = 2;
	unsigned char	*p;
	nr_list_t	*nr = mgr->nrlist;

	if (!nrx)
		return(3);
	l = nx[0] - 1;
	if (l<=0)
		return(3);
	while(nr) {
		p = nx + 2;
		dprint(DBGM_MAN,"%s: cpn(%s) nr(%s)\n", __FUNCTION__,
			p, nr->nr);
		for(i=0;i<nr->len;i++) {
			if (*p != nr->nr[i])
				break;
			if ((i+1) == nr->len) {
				*nrx = nr;
				return(0);
			}
			if (l == (i+1)) {
				ret = 1;
				break;
			}
			p++;	
		}
		nr = nr->next;
	}
	return(ret);
}

static int
manager2stack(void *dat, void *arg)
{
	net_stack_t	*nst = dat;
	msg_t		*msg = arg;
	mISDNuser_head_t	*hh;

	dprint(DBGM_MAN, "%s:dat(%p) arg(%p)\n", __FUNCTION__,
		dat, arg);
	if (!nst | !arg)
		return(-EINVAL);
	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_MAN, "%s: prim(%x) dinfo(%x) msg->len(%d)\n", __FUNCTION__,
		hh->prim, hh->dinfo, msg->len);
	if (hh->prim == (CC_NEW_CR | INDICATION)) /* high prio */
		msg_queue_head(&nst->wqueue, arg);
	else
		msg_queue_tail(&nst->wqueue, arg);
	sem_post(&nst->work);
	return(0);
}

static int
stack2manager(void *dat, void *arg) {
	manager_t 	*mgr = dat;
	msg_t		*msg = arg;
	mISDNuser_head_t	*hh;

	if (!msg || !mgr)
		return(-EINVAL);
	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_MAN, "%s: prim(%x) dinfo(%x) msg->len(%d) bid(%x/%x)\n", __FUNCTION__,
		hh->prim, hh->dinfo, msg->len, mgr->bc[0].l3id, mgr->bc[1].l3id);
	if (hh->prim == (CC_SETUP | INDICATION)) {
		SETUP_t			*setup;
		RELEASE_COMPLETE_t	*rc;
		unsigned char		cause[4];

		setup = (SETUP_t*)(msg->data + mISDNUSER_HEAD_SIZE);
		pthread_mutex_lock(&mgr->bc[0].lock);
		if (mgr->bc[0].cstate == BC_CSTATE_NULL) {
			mgr->bc[0].cstate = BC_CSTATE_ICALL;
			msg_queue_tail(&mgr->bc[0].workq, msg);
			pthread_mutex_unlock(&mgr->bc[0].lock);
			sem_post(&mgr->bc[0].work);
			return(0);
		}
		pthread_mutex_unlock(&mgr->bc[0].lock);
		pthread_mutex_lock(&mgr->bc[1].lock);
		if (mgr->bc[1].cstate == BC_CSTATE_NULL) {
			mgr->bc[1].cstate = BC_CSTATE_ICALL;
			msg_queue_tail(&mgr->bc[1].workq, msg);
			pthread_mutex_unlock(&mgr->bc[1].lock);
			sem_post(&mgr->bc[1].work);
			return(0);
		}
		pthread_mutex_unlock(&mgr->bc[1].lock);
		/* No channel available */
		cause[0] = 2;
		cause[1] = 0x80 | CAUSE_LOC_PNET_LOCUSER;
		if (setup->CHANNEL_ID)
			cause[2] = 0x80 | CAUSE_CHANNEL_UNACCEPT;
		else
			cause[2] = 0x80 | CAUSE_NO_CHANNEL;
		prep_l3data_msg(CC_RELEASE_COMPLETE | REQUEST, hh->dinfo,
			sizeof(RELEASE_COMPLETE_t), 3, msg);
		rc = (RELEASE_COMPLETE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
		rc->CAUSE = msg_put(msg, 3);
		memcpy(rc->CAUSE, &cause, 3);
		if (manager2stack(mgr->nst, msg))
			free_msg(msg);
	} else if (hh->dinfo == mgr->bc[0].l3id) {
		msg_queue_tail(&mgr->bc[0].workq, msg);
		sem_post(&mgr->bc[0].work);
	} else if (hh->dinfo == mgr->bc[1].l3id) {
		msg_queue_tail(&mgr->bc[1].workq, msg);
		sem_post(&mgr->bc[1].work);
	} else {
		wprint("%s: prim(%x) dinfo(%x) msg->len(%d) not handled\n", __FUNCTION__,
			hh->prim, hh->dinfo, msg->len);
		return(-ESRCH);
	}
	return(0);
}

static int
appl2bc(manager_t *mgr, int prim, void *arg)
{
	bchannel_t	*bc = arg;
	msg_t		*msg;

	dprint(DBGM_MAN, "%s(%p,%x,%p)\n", __FUNCTION__,
		 mgr, prim, arg);
	if (!mgr || !bc)
		return(-EINVAL);
	if (prim == PR_APP_OCHANNEL) {
		bchannel_t **bcp = arg;

		pthread_mutex_lock(&mgr->bc[0].lock);
		if (mgr->bc[0].cstate == BC_CSTATE_NULL) {
			mgr->bc[0].cstate = BC_CSTATE_OCALL;
			pthread_mutex_unlock(&mgr->bc[0].lock);
			*bcp = &mgr->bc[0];
			return(1);
		}
		pthread_mutex_unlock(&mgr->bc[0].lock);
		pthread_mutex_lock(&mgr->bc[1].lock);
		if (mgr->bc[1].cstate == BC_CSTATE_NULL) {
			mgr->bc[1].cstate = BC_CSTATE_OCALL;
			pthread_mutex_unlock(&mgr->bc[1].lock);
			*bcp = &mgr->bc[1];
			return(2);
		}
		pthread_mutex_unlock(&mgr->bc[1].lock);
		/* No channel available */
		return(-EBUSY);
	} else if (prim == PR_APP_OCALL) {
		pthread_mutex_lock(&bc->lock);
		msg = create_link_msg(CC_SETUP | REQUEST, bc->l3id, 0,
			NULL, 0);
		if (!msg)
			return(-ENOMEM);
		msg_queue_tail(&bc->workq, msg);
		sem_post(&bc->work);
		pthread_mutex_unlock(&bc->lock);
	} else if (prim == PR_APP_ALERT) {
		pthread_mutex_lock(&bc->lock);
		msg = create_link_msg(CC_ALERTING | REQUEST, bc->l3id, 0,
			NULL, 0);
		if (!msg)
			return(-ENOMEM);
		msg_queue_tail(&bc->workq, msg);
		sem_post(&bc->work);
		pthread_mutex_unlock(&bc->lock);
	} else if (prim == PR_APP_CONNECT) {
		pthread_mutex_lock(&bc->lock);
		msg = create_link_msg(CC_CONNECT | REQUEST, bc->l3id, 0,
			NULL, 0);
		if (!msg)
			return(-ENOMEM);
		msg_queue_tail(&bc->workq, msg);
		sem_post(&bc->work);
		pthread_mutex_unlock(&bc->lock);
	} else if (prim == PR_APP_HANGUP) {
		pthread_mutex_lock(&bc->lock);
		msg = create_link_msg(CC_DISCONNECT | REQUEST, bc->l3id, 0,
			NULL, 0);
		if (!msg)
			return(-ENOMEM);
		msg_queue_tail(&bc->workq, msg);
		sem_post(&bc->work);
		pthread_mutex_unlock(&bc->lock);
	} else if (prim == PR_APP_FACILITY) {
		pthread_mutex_lock(&bc->lock);
		msg = create_link_msg(CC_FACILITY | REQUEST, bc->l3id,
			0, NULL, 0);
		if (!msg)
			return(-ENOMEM);
		msg_queue_tail(&bc->workq, msg);
		sem_post(&bc->work);
		pthread_mutex_unlock(&bc->lock);
	} else if (prim == PR_APP_USERUSER) {
		pthread_mutex_lock(&bc->lock);
		msg = create_link_msg(CC_USER_INFORMATION | REQUEST, bc->l3id,
			0, NULL, 0);
		if (!msg)
			return(-ENOMEM);
		msg_queue_tail(&bc->workq, msg);
		sem_post(&bc->work);
		pthread_mutex_unlock(&bc->lock);
	} else {
		wprint("%s(%p,%x,%p) unhandled\n", __FUNCTION__,
			mgr, prim, arg);
	}
	return(0);
}

int
init_manager(manager_t **mlist, afunc_t application)
{
	manager_t	*mgr;
	int		ret;

	*mlist = NULL;
	mgr = malloc(sizeof(manager_t));
	if (!mgr)
		return(-ENOMEM);
	memset(mgr, 0, sizeof(manager_t));
	mgr->nst = malloc(sizeof(net_stack_t));
	if (!mgr->nst) {
		free(mgr);
		return(-ENOMEM);
	}
	memset(mgr->nst, 0, sizeof(net_stack_t));
	ret = do_net_stack_setup(mgr->nst);
	if (ret) {
		free(mgr->nst);
		free(mgr);
		return(ret);
	}
	mgr->application = application;
	mgr->app_bc = appl2bc;
	mgr->man2stack = manager2stack;
	mgr->nst->l3_manager = stack2manager;
	mgr->nst->manager = mgr;
	Isdnl2Init(mgr->nst);
	Isdnl3Init(mgr->nst);
	mgr->bc[0].manager = mgr;
	mgr->bc[1].manager = mgr;
	init_bchannel(&mgr->bc[0], 1); 
	init_bchannel(&mgr->bc[1], 2);
	*mlist = mgr;
	return(0);
} 

int
cleanup_manager(manager_t *mgr)
{
	int	ret, *retv;

	dprint(DBGM_MAN,"%s\n", __FUNCTION__);
	term_bchannel(&mgr->bc[0]);
	term_bchannel(&mgr->bc[1]);
	cleanup_Isdnl3(mgr->nst);
	cleanup_Isdnl2(mgr->nst);
	do_net_stack_cleanup(mgr->nst);
	ret = pthread_join(mgr->bc[0].tid, (void *)&retv);
	dprint(DBGM_MAN,"%s: join ret(%d) bc1 retv(%p)\n", __FUNCTION__,
		ret, retv);
	ret = pthread_join(mgr->bc[1].tid, (void *)&retv);
	dprint(DBGM_MAN,"%s: join ret(%d) bc2 retv(%p)\n", __FUNCTION__,
		ret, retv);
	while(mgr->nrlist) {
		nr_list_t *nr = mgr->nrlist;

		REMOVE_FROM_LISTBASE(nr, mgr->nrlist);
		free(nr);
	}
	free(mgr->nst);
	free(mgr);
	return(0);
}
