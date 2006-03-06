#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "isdn_net.h"
#include "net_l2.h"
#include "net_l3.h"
#include "net_l4.h"
#include "l3dss1.h"
#include "helper.h"
#include "bchannel.h"
#include "tone.h"

net_stack_t	kern_if;

itimer_t	timer1;

static void
do_cleanup(net_stack_t *nst)
{
	fprintf(stderr, "%s\n", __FUNCTION__);
	cleanup_Isdnl4(nst);
	cleanup_Isdnl3(nst);
	cleanup_Isdnl2(nst);
	do_net_stack_cleanup(nst);
}

static void
term_handler(int sig)
{
	pthread_t	tid;

	tid = pthread_self();
	fprintf(stderr,"signal %d received from thread %ld\n", sig, tid);
	test_and_set_bit(FLG_KIF_TERMINATION, &kern_if.flag);
	sem_post(&kern_if.network);
}

static int
man_down(net_stack_t *nst, msg_t *msg)
{
	msg_queue_tail(&nst->wqueue, msg);
	sem_post(&nst->network);
	return(0);
}

static int
do_disconnect(layer4_t *l4)
{
	l4->cause_loc = CAUSE_LOC_PNET_LOCUSER;
	l4->cause_val = CAUSE_NORMAL_CLEARING;
	l4->progress = PROGRESS_TONE;
	if_link(l4->nst, man_down, MAN_CLEAR_CALL | REQUEST,
		l4->channel, 0, NULL, 0);
	return(0);
}

static int
do_connect(layer4_t *l4)
{
	if_link(l4->nst, man_down, MAN_CONNECT | REQUEST,
		l4->channel, 0, NULL, 0);
	return(0);
}

static int
do_alert(layer4_t *l4)
{
	if_link(l4->nst, man_down, MAN_ALERT | REQUEST,
		l4->channel, 0, NULL, 0);
	return(0);
}

static int
clear_call(layer4_t *l4)
{
	if (l4->sdata) {
		layer4_t *peer = l4->sdata;
		
		if (l4->cause_val) {
			peer->cause_loc = l4->cause_loc;
			peer->cause_val = l4->cause_val;
		} else {
			peer->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			peer->cause_val = CAUSE_NORMALUNSPECIFIED;
		}
		peer->progress = PROGRESS_TONE;
		peer->sbuf = NULL;
		peer->sdata = NULL;
		peer->rdata = NULL;
		if (peer->nst)
			if_link(peer->nst, man_down, MAN_CLEAR_CALL |
				REQUEST, peer->channel, 0, NULL, 0);
	}
	l4->sdata = NULL;
	l4->rdata = NULL;
	l4->sbuf = NULL;
	return(0);
}

static int
alert_call(layer4_t *l4)
{
	if (l4->sdata)
		do_alert(l4->sdata);
	return(0);
}

static int
connect_call(layer4_t *l4)
{
	strcpy(l4->display,"connect ack");
	if_link(l4->nst, man_down, MAN_CONNECT | RESPONSE,
		l4->channel, 0, NULL, 0);
	del_timer(&timer1);
	if (l4->sdata)
		do_connect(l4->sdata);
	return(0);
}

static int
route_call(layer4_t *l4)
{
	layer4_t	*newl4;

	fprintf(stderr, "%s: msn ", __FUNCTION__);
	display_NR_IE(l4->msn);
	fprintf(stderr, "%s:  nr ", __FUNCTION__);
	display_NR_IE(l4->nr);
	if (l4->usednr->typ == NR_TYPE_INTERN) {
		newl4 = get_free_channel(&kern_if, -1, NULL);
		if (!newl4) {
			l4->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			l4->cause_val = CAUSE_USER_BUSY;
			l4->progress = PROGRESS_TONE;
			if_link(l4->nst, man_down, MAN_CLEAR_CALL | REQUEST,
				l4->channel, 0, NULL, 0);
			return(0);
		}
		l4->sdata = newl4;
		l4->rdata = newl4;
		newl4->sdata = l4;
		newl4->rdata = l4;
		l4->sbuf = &newl4->rbuf;
		newl4->sbuf = &l4->rbuf;
		newl4->msn[0] = l4->usednr->len +1;
		newl4->msn[1] = 0x81;
		memcpy(&newl4->msn[2], l4->usednr->nr, l4->usednr->len);
		if (l4->msn[0])
			memcpy(newl4->nr, l4->msn, l4->msn[0] + 1);
		newl4->l1_prot = ISDN_PID_L1_B_64TRANS;
		if_link(newl4->nst, man_down, MAN_SETUP | REQUEST,
			newl4->channel, 0, NULL, 0);
	} else if (l4->usednr->typ == NR_TYPE_AUDIO) {
		l4->sdata = NULL;
		l4->rdata = NULL;
		strcpy(l4->display,"connect to AUDIO");
		do_connect(l4);
		l4->display[0] = 0;
		deactivate_bchannel(l4);
		setup_bchannel_rawdev(l4);
		activate_bchannel(l4);
	} else if (l4->usednr->typ == NR_TYPE_VOIP) {
		l4->sdata = NULL;
		l4->rdata = NULL;
		sprintf(l4->display,"calling %s", l4->usednr->name);
		do_alert(l4);
		sprintf(l4->display,"connect to %s", l4->usednr->name);
		do_connect(l4);
		l4->display[0] = 0;
		deactivate_bchannel(l4);
		setup_bchannel_rawdev(l4);
		activate_bchannel(l4);
	}
	return(0);
}

static int
manager(net_stack_t *nst, msg_t *msg) {
	mISDNuser_head_t	*hh;
	layer4_t	*l4;

	if (!msg)
		return(-EINVAL);
	hh = (mISDNuser_head_t *)msg->data;
	msg_pull(msg, mISDN_HEAD_SIZE);
	fprintf(stderr, "%s: prim(%x) msg->len(%d)\n", __FUNCTION__,
		hh->prim, msg->len);
	if (hh->dinfo == 1) {
		l4 = &nst->layer4[0];
	} else if (hh->dinfo == 2) {
		l4 = &nst->layer4[1];
	} else {
		return(-EINVAL);
	}
	switch(hh->prim) {
		case MAN_SETUP | INDICATION:
			fprintf(stderr, "%s: setup id(%x)\n", __FUNCTION__,
				hh->dinfo);
			route_call(l4);
			break;
		case MAN_ALERT | INDICATION:
			fprintf(stderr, "%s: connect id(%x)\n", __FUNCTION__,
				hh->dinfo);
			alert_call(l4);
			break;
		case MAN_CONNECT | INDICATION:
			fprintf(stderr, "%s: connect id(%x)\n", __FUNCTION__,
				hh->dinfo);
			connect_call(l4);
			break;
		case MAN_CLEAR_CALL | INDICATION:
			fprintf(stderr, "%s: clear call id(%x)\n", __FUNCTION__,
				hh->dinfo);
			clear_call(l4);
			break;
		default:
			fprintf(stderr, "%s: unhandled prim(%x) msg->len(%d)\n", __FUNCTION__,
				hh->prim, msg->len);
			break;
	}
	free_msg(msg);
	return(0);
}


int main(argc,argv)
int argc;
char *argv[];

{
	int		ret, *retp;
	nr_list_t	*nr1,*nr2,*nr3,*nr4,*nr5;
	layer4_t	*l4;
	if_action_t	mISDNrd,*hrd;

	
	nr1 = malloc(sizeof(nr_list_t));
	nr2 = malloc(sizeof(nr_list_t));
	nr3 = malloc(sizeof(nr_list_t));
	nr4 = malloc(sizeof(nr_list_t));
	nr5 = malloc(sizeof(nr_list_t));
	memset(nr1, 0, sizeof(nr_list_t));
	memset(nr2, 0, sizeof(nr_list_t));
	memset(nr3, 0, sizeof(nr_list_t));
	memset(nr4, 0, sizeof(nr_list_t));
	memset(nr5, 0, sizeof(nr_list_t));
	nr1->len = 5;
	strcpy(nr1->nr,"12345");
	nr1->typ = NR_TYPE_INTERN;
	nr2->len = 4;
	strcpy(nr2->nr,"4566");
	nr2->typ = NR_TYPE_INTERN;
	nr3->len = 3;
	strcpy(nr3->nr,"789");
	nr3->typ = NR_TYPE_AUDIO;
	nr4->len = 3;
	strcpy(nr4->nr,"147");
	strcpy(nr4->name, "pingi2");
	nr4->typ = NR_TYPE_VOIP;
	nr5->len = 3;
	strcpy(nr5->nr,"258");
	strcpy(nr5->name, "pingi2");
	nr5->typ = NR_TYPE_VOIP;
	msg_init();
	ret = do_net_stack_setup(&kern_if);
	if (ret) {
		fprintf(stderr, "error in do_net_stack_setup %d\n", ret);
		return(0);
	}
	APPEND_TO_LIST(nr1, kern_if.nrlist);
	APPEND_TO_LIST(nr2, kern_if.nrlist);
	APPEND_TO_LIST(nr3, kern_if.nrlist);
	APPEND_TO_LIST(nr4, kern_if.nrlist);
	APPEND_TO_LIST(nr5, kern_if.nrlist);
	Isdnl2Init(&kern_if);
	Isdnl3Init(&kern_if);
	Isdnl4Init(&kern_if);
	kern_if.l4_mgr = manager;
	init_bhandler(&kern_if);
	memset(&timer1, 0, sizeof(itimer_t));
	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	signal(SIGPIPE, term_handler);
	if (argc>1) {
		l4 = get_free_channel(&kern_if, -1, NULL);
		if (l4) {
			l4->msn[0] = 4;
			l4->msn[1] = 0x81;
			l4->msn[2] = '8';
			l4->msn[3] = '8';
			l4->msn[4] = '8';
			
			l4->nr[0] = 4;
			l4->nr[1] = 0x81;
			l4->nr[2] = '1';
			l4->nr[3] = '2';
			l4->nr[4] = '3';
			l4->l1_prot = ISDN_PID_L1_B_64TRANS;
			if_link(l4->nst, man_down, MAN_SETUP | REQUEST,
				l4->channel, 0, NULL, 0);
			del_timer(&timer1);
			timer1.function = (void *)do_disconnect;
			timer1.data = (long)l4;
			init_timer(&timer1, &kern_if);
			timer1.expires = 8000;
			add_timer(&timer1);
		}
	}
	hrd = &mISDNrd;
	memset(hrd, 0, sizeof(if_action_t));
	hrd->nst = &kern_if;
	hrd->fd = kern_if.device;
	hrd->function = do_net_read;
	APPEND_TO_LIST(hrd, kern_if.rd);
	retp = do_netthread(&kern_if);
	fprintf(stderr, "do_main_loop returns(%p)\n", retp);
	do_cleanup(&kern_if);
	return(0);
}
