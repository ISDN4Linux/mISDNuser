#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "net_l2.h"
#include "isdn_net.h"
#include "bchannel.h"
#include "helper.h"


int
do_net_stack_setup(net_stack_t	*nst)
{
	int		ret;
	unsigned char	buf[1024];
	int		i,cnt;
	iframe_t	*frm = (iframe_t *)buf;
	stack_info_t	*stinf;
	layer_info_t	li;
#ifdef OBSOLETE
	interface_info_t ii;
#endif
	

	if (!nst)
		return(-EINVAL);
	if (nst->device)
		return(-EBUSY);
	ret = mISDN_open();
	if (0 > ret) {
		wprint("cannot open mISDN due to %s\n",
			strerror(errno));
		return(ret);
	}
	nst->device = ret;
	cnt = mISDN_get_stack_count(nst->device);
	if (cnt < 1) {
		mISDN_close(nst->device);
		wprint("no cards found ret(%d)\n", cnt);
		return(-ENODEV);
	}
	for (i=1; i<=cnt; i++) {
		ret = mISDN_get_stack_info(nst->device, i, buf, 1024);
		if (ret<=0)
			dprint(DBGM_NET, "cannot get stackinfo err: %d\n", ret);
		stinf = (stack_info_t *)&frm->data.p;
//		mISDNprint_stack_info(stdout, stinf);
		if ((stinf->pid.protocol[0] == ISDN_PID_L0_NT_S0) &&
			(stinf->pid.protocol[1] == ISDN_PID_L1_NT_S0)) {
			if (stinf->instcnt == 1) {
				nst->cardnr = i;
				nst->d_stid = stinf->id;
				nst->b_stid[0] = stinf->child[0];
				nst->b_stid[1] = stinf->child[1];
				dprint(DBGM_NET, "bst1 %x bst2 %x\n",
					nst->b_stid[0], nst->b_stid[1]);
				break;
			} else
				dprint(DBGM_NET, "stack %d instcnt is %d\n",
					i, stinf->instcnt);
		} else
			dprint(DBGM_NET, "stack %d protocol %x\n",
				i, stinf->pid.protocol[0]);
	}
	if (i>cnt) {
		mISDN_close(nst->device);
		wprint("no NT cards found\n");
		return(-ENODEV);
	}
	nst->l1_id = mISDN_get_layerid(nst->device, nst->d_stid, 1);
	if (nst->l1_id < 0) {
		mISDN_close(nst->device);
		eprint("no layer1 id found\n");
		return(-EINVAL);
	}
	dprint(DBGM_NET, "found NT card stack card%d dst(%x) l1(%x)\n",
		nst->cardnr, nst->d_stid, nst->l1_id);
	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "net l2");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[2] = ISDN_PID_L2_LAPD_NET;
	li.pid.layermask = ISDN_LAYER(2);
	li.st = nst->d_stid;
	nst->l2_id = mISDN_new_layer(nst->device, &li);
	if (nst->l2_id<=0) {
		eprint("cannot add layer2 error %d %s\n",
			nst->l2_id, strerror(-nst->l2_id));
		mISDN_close(nst->device);
		return(nst->l2_id);
	}
#ifdef OBSOLETE
	ii.extentions = EXT_IF_EXCLUSIV;
	ii.owner = nst->l2_id;
	ii.peer = nst->l1_id;
	ii.stat = IF_DOWN;
	ret = mISDN_connect(nst->device, &ii);
	if (ret) {
		eprint("cannot connect layer1 error %d %s\n",
			ret, strerror(-ret));
		mISDN_close(nst->device);
		return(ret);
	}
#endif
	dprint(DBGM_NET, "add nt net layer2  %x\n",
		nst->l2_id);
	msg_queue_init(&nst->down_queue);
	msg_queue_init(&nst->rqueue);
	msg_queue_init(&nst->wqueue);
	pthread_mutex_init(&nst->lock, NULL);
	ret = sem_init (&nst->work, 0, 0);
	if (ret) {
		eprint("cannot init semaphore ret(%d) %d %s\n",
			ret, errno, strerror(errno));
		return(ret);
	}
	return(0);
}

int
do_net_stack_cleanup(net_stack_t  *nst)
{
	int ret;

	msg_queue_purge(&nst->down_queue);
	msg_queue_purge(&nst->rqueue);
	msg_queue_purge(&nst->wqueue);
	if (nst->phd_down_msg)
		free_msg(nst->phd_down_msg);
	nst->phd_down_msg = NULL;
	mISDN_close(nst->device);
	ret = sem_destroy(&nst->work);
	if (ret) {
		eprint("cannot destroy semaphore ret(%d) %d %s\n",
			ret, errno, strerror(errno));
		return(ret);
	}
	ret = pthread_mutex_destroy(&nst->lock);
	if (ret) {
		eprint("cannot destroy mutex ret(%d) %s\n",
			ret, strerror(ret));
		return(ret);
	}
	return(0);
}

static itimer_t
*get_timer(net_stack_t *nst, int id)
{
	itimer_t	*it = nst->tlist;

	while(it) {
		if (it->id == id)
			break;
		it = it->next;
	}
	return(it);
}

int
init_timer(itimer_t *it, net_stack_t *nst)
{
	iframe_t	frm;
	int		ret;

	if (!nst)
		return(-ENODEV);
	if (!get_timer(nst, it->id)) {
		it->id = (int)it;
		it->Flags = 0;
		it->nst = nst;
		it->prev = NULL;
		if (nst->tlist) {
			nst->tlist->prev = it;
			it->next = nst->tlist;
		}
		nst->tlist = it;
	}
	dprint(DBGM_NET, "init timer(%x)\n", it->id);
	if (test_bit(FLG_TIMER_RUNING, &it->Flags))
		dprint(DBGM_NET, "init timer(%x) while running\n", it->id);
	ret = mISDN_write_frame(it->nst->device, &frm, it->id,
		MGR_INITTIMER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (ret)
		wprint("cannot init timer %p err(%d) %s\n",
			it, errno, strerror(errno));
	return(ret);
}

int
remove_timer(itimer_t *it)
{
	iframe_t	frm;
	int		ret;

	
	if (!it->nst)
		return(-ENODEV);
	if (!get_timer(it->nst, it->id))
		return(-ENODEV);
	
	ret = mISDN_write_frame(it->nst->device, &frm, it->id,
		MGR_REMOVETIMER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (ret)
		wprint("cannot remove timer %p err(%d) %s\n",
			it, errno, strerror(errno));
	REMOVE_FROM_LISTBASE(it, it->nst->tlist);
	return(ret);
}

int
add_timer(itimer_t *it)
{
	iframe_t	frm;
	int		ret;

	if (!it->nst)
		return(-ENODEV);
	if (!get_timer(it->nst, it->id))
		return(-ENODEV);
	if (timer_pending(it))
		return(-EBUSY);
	dprint(DBGM_NET, "add timer(%x)\n", it->id);
	test_and_set_bit(FLG_TIMER_RUNING, &it->Flags);
	ret = mISDN_write_frame(it->nst->device, &frm, it->id,
		MGR_ADDTIMER | REQUEST, it->expires, 0, NULL, TIMEOUT_1SEC);
	if (ret)
		wprint("cannot add timer %p (%d ms) err(%d) %s\n",
			it, it->expires, errno, strerror(errno));
	return(ret);
}

int
del_timer(itimer_t *it)
{
	iframe_t	frm;
	int		ret;

	if (!it->nst)
		return(-ENODEV);
	if (!get_timer(it->nst, it->id))
		return(-ENODEV);
	dprint(DBGM_NET, "del timer(%x)\n", it->id);
	test_and_clear_bit(FLG_TIMER_RUNING, &it->Flags);
	ret = mISDN_write_frame(it->nst->device, &frm, it->id,
		MGR_DELTIMER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (ret)
		wprint("cannot del timer %p (%d ms) err(%d) %s\n",
			it, it->expires, errno, strerror(errno));
	return(ret);
}

int
timer_pending(itimer_t *it)
{
	return(test_bit(FLG_TIMER_RUNING, &it->Flags));
}

static int
handle_timer(net_stack_t *nst, int id)
{
	itimer_t	*it;
	int		ret = 0;

	it = get_timer(nst, id);
	if (!it)
		return(-ENODEV);
//	dprint(DBGM_NET, "handle timer(%x)\n", it->id);
	test_and_clear_bit(FLG_TIMER_RUNING, &it->Flags);
	if (it->function)
		ret = it->function(it->data);
	return(ret);
}

int
write_dmsg(net_stack_t *nst, msg_t *msg)
{
	iframe_t	*frm;
	mISDNuser_head_t	*hh;

	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_NET, "%s: msg(%p) len(%d) pr(%x) di(%x)\n", __FUNCTION__,
		msg, msg->len, hh->prim, hh->dinfo);
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	frm = (iframe_t *)msg_push(msg, mISDN_HEADER_LEN);
	frm->prim = hh->prim;
	frm->dinfo = hh->dinfo;
	frm->addr = nst->l2_id | FLG_MSG_DOWN;
	frm->len = msg->len - mISDN_HEADER_LEN;
	if (frm->prim == PH_DATA_REQ) {
		frm->dinfo = (int)msg;
		if (nst->phd_down_msg) {
			msg_queue_tail(&nst->down_queue, msg);
			return(0);
		}
		nst->phd_down_msg = msg;
	}
	mISDN_write(nst->device, msg->data, msg->len, -1);
	free_msg(msg);
	return(0);
}

int
phd_conf(net_stack_t *nst, iframe_t *frm, msg_t *msg)
{
	dprint(DBGM_NET, "%s: di(%x)\n", __FUNCTION__, frm->dinfo);
	if (frm->dinfo == (int)nst->phd_down_msg) {
		free_msg(msg);
		nst->phd_down_msg = msg_dequeue(&nst->down_queue);
		if (nst->phd_down_msg) {
			mISDN_write(nst->device, nst->phd_down_msg->data,
				nst->phd_down_msg->len, -1);
			free_msg(nst->phd_down_msg);
		}
		return(0);
	} else {
		wprint("%s: not matching %p/%#x\n", __FUNCTION__,
			nst->phd_down_msg, frm->dinfo);
		return(-EINVAL);
	}
}

static int
do_net_read(net_stack_t *nst)
{
	msg_t		*msg;
	iframe_t	*frm;
	int		ret;
	
	msg = alloc_msg(MAX_MSG_SIZE);
	if (!msg)
		return(-ENOMEM);
	ret = mISDN_read(nst->device, msg->data, MAX_MSG_SIZE, -1);
	if (ret<0) {
		free_msg(msg);
		if (errno == EAGAIN)
			return(0);
		else
			return(-errno);
	}
	if (!ret) {
		wprint("do_net_read read nothing\n");
		free_msg(msg);
		return(-EINVAL);
	}
	__msg_trim(msg, ret);
	frm = (iframe_t *)msg->data;
	
	dprint(DBGM_NET,"%s: prim(%x) addr(%x)\n", __FUNCTION__,
		frm->prim, frm->addr);
	switch (frm->prim) {
		case MGR_INITTIMER | CONFIRM:
		case MGR_ADDTIMER | CONFIRM:
		case MGR_DELTIMER | CONFIRM:
		case MGR_REMOVETIMER | CONFIRM:
//			dprint(DBGM_NET, "timer(%x) cnf(%x)\n",
//				frm->addr, frm->prim);
			free_msg(msg);
			return(0);
	}
	msg_queue_tail(&nst->rqueue, msg);
	sem_post(&nst->work);
	return(0);
}

static int
b_message(net_stack_t *nst, int ch, iframe_t *frm, msg_t *msg)
{
	mISDNuser_head_t	*hh;

	msg_pull(msg, mISDN_HEADER_LEN);
	hh = (mISDNuser_head_t *)msg_push(msg, mISDNUSER_HEAD_SIZE);
	hh->prim = frm->prim;
	hh->dinfo = nst->bcid[ch];
	if (nst->l3_manager)
		return(nst->l3_manager(nst->manager, msg));
	return(-EINVAL);
	
}

static int
do_readmsg(net_stack_t *nst, msg_t *msg)
{
	iframe_t	*frm;
	int		ret = -EINVAL;

	if (!nst || !msg)
		return(-EINVAL);
	frm = (iframe_t *)msg->data;
	
	dprint(DBGM_NET,"%s: prim(%x) addr(%x)\n", __FUNCTION__,
		frm->prim, frm->addr);
	if (frm->prim == (MGR_TIMER | INDICATION)) {
		mISDN_write_frame(nst->device, msg->data, frm->addr,
			MGR_TIMER | RESPONSE, 0, 0, NULL, TIMEOUT_1SEC);
		ret = handle_timer(nst, frm->addr);
		free_msg(msg);
		return(0);
	}
	if ((frm->addr & INST_ID_MASK) == nst->l2_id) {
		if (nst->l1_l2) {
			ret = nst->l1_l2(nst, msg);
		}
	} else if (nst->b_addr[0] &&
		((frm->addr & INST_ID_MASK) == nst->b_addr[0])) {
		ret = b_message(nst, 0, frm, msg);
	} else if (nst->b_addr[1] &&
		((frm->addr & INST_ID_MASK) == nst->b_addr[1])) {
		ret = b_message(nst, 1, frm, msg);
	} else if (nst->b_stid[0] == frm->addr) {
		ret = b_message(nst, 0, frm, msg);
	} else if (nst->b_stid[1] == frm->addr) {
		ret = b_message(nst, 1, frm, msg);
	} else if (frm->prim == (MGR_DELLAYER | CONFIRM)) {
		dprint(DBGM_NET,"%s: MGR_DELLAYER CONFIRM addr(%x)\n", __FUNCTION__,
			frm->addr);
		free_msg(msg);
		return(0);
	} else {
		wprint("%s: unhandled msg(%d) prim(%x) addr(%x) dinfo(%x)\n", __FUNCTION__,
			frm->len, frm->prim, frm->addr, frm->dinfo);
	}
	return(ret);
}

static int 
setup_bchannel(net_stack_t *nst, mISDNuser_head_t *hh, msg_t *msg) {
	mISDN_pid_t	*pid;
	int		ret, ch, *id;
	layer_info_t	li;
	unsigned char	buf[32];

	if ((hh->dinfo < 1) || (hh->dinfo > 2)) {
		eprint("wrong channel %d\n", hh->dinfo);
		return(-EINVAL);
	}
	ch = hh->dinfo -1;
	dprint(DBGM_NET,"%s:ch%d\n", __FUNCTION__, hh->dinfo);
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	id = (int *)msg->data;
	nst->bcid[ch] = *id;
	msg_pull(msg, sizeof(int));
	pid = (mISDN_pid_t *)msg->data;
	memset(&li, 0, sizeof(layer_info_t));
	li.object_id = -1;
	li.extentions = 0;
	li.st = nst->b_stid[ch];
	if (pid->protocol[2] == ISDN_PID_L2_B_USER) {
		strcpy(&li.name[0], "B L2");
		li.pid.protocol[2] = ISDN_PID_L2_B_USER;
		li.pid.layermask = ISDN_LAYER(2);
	} else {
		strcpy(&li.name[0], "B L3");
		li.pid.protocol[3] = pid->protocol[3];
		li.pid.layermask = ISDN_LAYER(3);
	}
	if (nst->b_addr[ch])
		wprint("%s: b_addr[%d] %x in use\n", __FUNCTION__,
			ch, nst->b_addr[ch]);
	ret = mISDN_new_layer(nst->device, &li);
	if (ret<=0) {
		wprint("%s: new_layer ret(%d)\n", __FUNCTION__, ret);
		goto error;
	}
	if (ret) {
		nst->b_addr[ch] = ret;
		dprint(DBGM_NET,"%s: b_address%d %08x\n", __FUNCTION__,
			hh->dinfo, ret);
		ret = mISDN_set_stack(nst->device, nst->b_stid[ch],
			pid);
		if (ret) {
			wprint("set_stack ret(%d)\n", ret);
			mISDN_write_frame(nst->device, buf,
				nst->b_addr[ch], MGR_DELLAYER | REQUEST,
				0, 0, NULL, TIMEOUT_1SEC);
			nst->b_addr[ch] = 0;
			goto error;
		}
		if_link(nst->manager, (ifunc_t)nst->l3_manager,
			BC_SETUP | CONFIRM, nst->bcid[ch], sizeof(int),
			&nst->b_addr[ch], 0);
		free_msg(msg);
		return(0);
	} 
error:
	if_link(nst->manager, (ifunc_t)nst->l3_manager, BC_SETUP | SUB_ERROR,
		nst->bcid[ch], sizeof(int), &ret, 0);
	free_msg(msg);
	return(0);
}

static int 
cleanup_bc(net_stack_t *nst, mISDNuser_head_t *hh, msg_t *msg)
{
	unsigned char	buf[32];
	int 		ch;

	if (hh->dinfo == nst->bcid[0])
		ch = 0;
	else if (hh->dinfo == nst->bcid[1])
		ch = 1;
	else {
		wprint("%s:not channel match %x %x/%x\n", __FUNCTION__,
			hh->dinfo, nst->bcid[0], nst->bcid[1]);
		
		if_link(nst->manager, (ifunc_t)nst->l3_manager,
			BC_CLEANUP | SUB_ERROR, hh->dinfo, 0, NULL, 0);
		free_msg(msg);
		return(0);
	}
	dprint(DBGM_NET,"%s:ch%d\n", __FUNCTION__, ch + 1);
	mISDN_clear_stack(nst->device, nst->b_stid[ch]);
	if (nst->b_addr[ch])
		mISDN_write_frame(nst->device, buf, nst->b_addr[ch],
			MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if_link(nst->manager, (ifunc_t)nst->l3_manager,
		BC_CLEANUP | CONFIRM, hh->dinfo, 0, NULL, 0);
	nst->b_addr[ch] = 0;
	free_msg(msg);
	return(0);
}

static int
l1_request(net_stack_t *nst, mISDNuser_head_t *hh, msg_t *msg)
{
	iframe_t	*frm;

	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_NET, "%s: msg(%p) len(%d) pr(%x) di(%x)\n", __FUNCTION__,
		msg, msg->len, hh->prim, hh->dinfo);
	msg_pull(msg, mISDNUSER_HEAD_SIZE);
	frm = (iframe_t *)msg_push(msg, mISDN_HEADER_LEN);
	frm->prim = hh->prim;
	frm->addr = hh->dinfo;
	if (frm->prim == PH_DATA_REQ)
		frm->dinfo = (int)msg;
	else
		frm->dinfo = 0;
	frm->len = msg->len - mISDN_HEADER_LEN;
	mISDN_write(nst->device, msg->data, msg->len, -1);
	free_msg(msg);
	return(0);
}

static int
do_writemsg(net_stack_t *nst, msg_t *msg)
{
	mISDNuser_head_t	*hh;
	int		ret = -EINVAL;

	if (!nst || !msg)
		return(-EINVAL);
	hh = (mISDNuser_head_t *)msg->data;
	dprint(DBGM_NET,"%s: prim(%x) dinfo(%x)\n", __FUNCTION__,
		hh->prim, hh->dinfo);
	if ((hh->prim & LAYER_MASK) == MSG_L1_PRIM) {
		ret = l1_request(nst, hh, msg);
	} else if (hh->prim == (BC_SETUP | REQUEST)) {
		ret = setup_bchannel(nst, hh, msg);
	} else if (hh->prim == (BC_CLEANUP | REQUEST)) {
		ret = cleanup_bc(nst, hh, msg);
	} else if (hh->prim == (CC_NEW_CR | INDICATION)) {
		msg_pull(msg, mISDNUSER_HEAD_SIZE);
		if (hh->dinfo == nst->bcid[0]) {
			nst->bcid[0] = *((int *)msg->data);
			free_msg(msg);
			ret = 0;
		} else if (hh->dinfo == nst->bcid[1]) {
			nst->bcid[1] = *((int *)msg->data);
			free_msg(msg);
			ret = 0;
		} else
			ret = -ENXIO;
	} else if ((hh->prim & LAYER_MASK) == MSG_L3_PRIM) {
		if (nst->manager_l3)
			ret = nst->manager_l3(nst, msg);
	} else {
		wprint("%s: prim(%x) dinfo(%x) unhandled msg(%d)\n", __FUNCTION__,
			hh->prim, hh->dinfo, msg->len);
	}
	return(ret);
}

static void *
main_readloop(void *arg)
{
	net_stack_t	*nst = arg;
	int		lp = 1;
	int		sel, ret;
	int		maxfd;
	fd_set		rfd;
	fd_set		efd; 
	pthread_t	tid;


	tid = pthread_self();
	dprint(DBGM_NET, "%s: tid %ld\n", __FUNCTION__, tid);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	while(lp) {
//		dprint(DBGM_NET, "%s: begin dev %d\n", __FUNCTION__, nst->device);
		maxfd = nst->device;
		FD_ZERO(&rfd);
		FD_SET(nst->device, &rfd);
		FD_ZERO(&efd);
		FD_SET(nst->device, &efd);
		maxfd++;
restart:
		sel = mISDN_select(maxfd, &rfd, NULL, &efd, NULL);
		if (sel < 0) {
			if (errno == EINTR) {
				if (test_bit(FLG_NST_TERMINATION, &nst->flag))
					break;
				dprint(DBGM_NET, "%s: select restart\n", __FUNCTION__);
				goto restart;
			}
			wprint("%s: error(%d) in select %s\n", __FUNCTION__,
				errno, strerror(errno));
			break;
		}
		if (sel) {
			if (FD_ISSET(nst->device, &rfd)) {
				ret = do_net_read(nst);
				if (ret) {
					dprint(DBGM_NET, "%s: rdfunc ret(%d)\n", __FUNCTION__, ret);
				}
			}
			if (FD_ISSET(nst->device, &efd)) {
				dprint(DBGM_NET, "%s: exception\n", __FUNCTION__);
			}
		}
	}
	dprint(DBGM_NET,"%s: fall trough, abort\n", __FUNCTION__);
	pthread_mutex_lock(&nst->lock);
	test_and_set_bit(FLG_NST_READER_ABORT, &nst->flag);
	pthread_mutex_unlock(&nst->lock);
	sem_post(&nst->work);
	return(NULL);
}

void *
do_netthread(void *arg) {
	net_stack_t	*nst = arg;
	int	ret;
	pthread_t	tid;
	void	*retval = NULL;

	/* create reader thread */
	tid = pthread_self();
	dprint(DBGM_NET, "%s: tid %ld\n", __FUNCTION__, tid);
	ret = pthread_create(&nst->reader, NULL, main_readloop, (void *)nst);
	tid = pthread_self();
	dprint(DBGM_NET, "%s: tid %ld crated %ld\n", __FUNCTION__, tid, nst->reader);
	if (ret) {
		eprint("%s: cannot create reader %d\n", __FUNCTION__,
			ret);
		return(NULL);
	}
	while(1) {
		msg_t	*msg;

		sem_wait(&nst->work);
		msg = msg_dequeue(&nst->wqueue);
		if (msg) {
			ret = do_writemsg(nst, msg);
			if (ret) {
				wprint("%s: do_writemsg return %d\n", __FUNCTION__,
					ret);
				free_msg(msg);
			}
		}
		
		msg = msg_dequeue(&nst->rqueue);
		if (msg) {
			ret = do_readmsg(nst, msg);
			if (ret) {
				wprint("%s: do_readmsg return %d\n", __FUNCTION__,
					ret);
				free_msg(msg);
			}
		}
		pthread_mutex_lock(&nst->lock);
		if (test_and_clear_bit(FLG_NST_READER_ABORT, &nst->flag)) {
			pthread_mutex_unlock(&nst->lock);
			dprint(DBGM_NET,"%s: reader aborted\n", __FUNCTION__);
			ret = pthread_join(nst->reader, &retval);
			dprint(DBGM_NET,"%s: join ret(%d) reader retval %p\n", __FUNCTION__,
				ret, retval);
			break;
		}
		if (test_bit(FLG_NST_TERMINATION, &nst->flag)) {
			pthread_mutex_unlock(&nst->lock);
			dprint(DBGM_NET,"%s: reader cancel\n", __FUNCTION__);
			ret = pthread_cancel(nst->reader);
			dprint(DBGM_NET,"%s: cancel reader ret(%d)\n", __FUNCTION__,
				ret);
			ret = pthread_join(nst->reader, &retval);
			dprint(DBGM_NET,"%s: join ret(%d) reader retval %p\n", __FUNCTION__,
				ret, retval);
			break;
		}
		pthread_mutex_unlock(&nst->lock);
	}
	return(retval);
}

int
term_netstack(net_stack_t *nst)
{
	test_and_set_bit(FLG_NST_TERMINATION, &nst->flag);
	sem_post(&nst->work);
	return(0);
}
