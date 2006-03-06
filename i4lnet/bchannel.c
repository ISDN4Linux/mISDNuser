#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "isdn_net.h"
#include "helper.h"
#include "tone.h"
#include "bchannel.h"
#include "net_l3.h"
#include "l3dss1.h"

static int
open_record_files(bchannel_t *bc)
{
	int	ret = -EINVAL;

	if (bc->manager->application)
		ret = bc->manager->application(bc->manager,
			PR_APP_OPEN_RECFILES, bc);
	return(ret);
}

static int
close_record_files(bchannel_t *bc)
{
	int	ret = -EINVAL;

	if (bc->manager->application)
		ret = bc->manager->application(bc->manager,
			PR_APP_CLOSE_RECFILES, bc);
	return(ret);
}

static int
setup_bchannel(bchannel_t *bc) {
	struct {
		int		id;
		mISDN_pid_t	pid;
	}	para;
		
	if ((bc->channel<1) || (bc->channel>2)) {
		eprint("wrong channel %d\n", bc->channel);
		return(-EINVAL);
	}
	dprint(DBGM_BC,"%s:ch%d bst(%d)\n", __FUNCTION__,
		bc->channel, bc->bstate);
	if ((bc->bstate != BC_BSTATE_NULL) &&
		(bc->bstate != BC_BSTATE_CLEANUP))
		return(-EBUSY); 
	memset(&para.pid, 0, sizeof(mISDN_pid_t));
	para.pid.protocol[1] = bc->l1_prot;
	if (FLG_BC_RAWDEVICE & bc->Flags) {
		para.pid.protocol[2] = ISDN_PID_L2_B_RAWDEV;
		para.pid.protocol[3] = ISDN_PID_L3_B_USER;
		para.pid.layermask = ISDN_LAYER(1) | ISDN_LAYER(2) |
					ISDN_LAYER(3);
	} else {
		para.pid.protocol[2] = ISDN_PID_L2_B_USER;
		para.pid.layermask = ISDN_LAYER(1) | ISDN_LAYER(2);
	}
	if (bc->Flags & FLG_BC_CALL_ORGINATE)
		para.pid.global = 1;
	para.id = bc->l3id;
	bc->bstate = BC_BSTATE_SETUP;
	if (!bc->sbuf) {
		bc->sbuf = init_ibuffer(2048);
		if (bc->sbuf) {
			bc->sbuf->rsem = &bc->work;
			bc->sbuf->wsem = &bc->work;
		}
	}
	if_link(bc->manager->nst, (ifunc_t)bc->manager->man2stack,
		BC_SETUP | REQUEST, bc->channel, sizeof(para), &para, 0);
	return(0);
}

static int
activate_bchannel(bchannel_t *bc)
{
	dprint(DBGM_BC,"%s:ch%d bst(%d)\n", __FUNCTION__,
		bc->channel, bc->bstate);
	if (!bc->b_addr) {
		wprint("%s:ch%d not setup\n", __FUNCTION__,
			bc->channel);
		return(-EINVAL);
	}
	if ((bc->bstate == BC_BSTATE_SETUP) ||
		(bc->bstate == BC_BSTATE_DEACTIVATE)) {
		bc->bstate = BC_BSTATE_ACTIVATE;
		return(if_link(bc->manager->nst,
			(ifunc_t)bc->manager->man2stack,
			PH_ACTIVATE | REQUEST, bc->b_addr | FLG_MSG_DOWN,
			0, NULL, 0));
	} else
		return(-EBUSY);
}

static int
deactivate_bchannel(bchannel_t *bc)
{
	dprint(DBGM_BC,"%s:ch%d bst(%d)\n", __FUNCTION__,
		bc->channel, bc->bstate);
	if (!bc->b_addr) {
		wprint("%s:ch%d not setup\n", __FUNCTION__,
			bc->channel);
		return(-EINVAL);
	}
	if ((bc->bstate == BC_BSTATE_ACTIVATE) ||
		(bc->bstate == BC_BSTATE_ACTIV)) {
		bc->bstate = BC_BSTATE_DEACTIVATE;
		return(if_link(bc->manager->nst,
			(ifunc_t)bc->manager->man2stack,
			PH_DEACTIVATE | REQUEST, bc->b_addr | FLG_MSG_DOWN,
			0, NULL, 0));
	} else
		return(-EBUSY);
}

static int
bc_cleanup(bchannel_t *bc)
{
	int	ret;

	dprint(DBGM_BC,"%s:ch%d bst(%d)\n", __FUNCTION__,
		bc->channel, bc->bstate);
	if (!bc->b_addr) {
		wprint("%s:ch%d not setup\n", __FUNCTION__,
			bc->channel);
	}
	if (!bc->l3id) {
		wprint("%s:ch%d no l3id\n", __FUNCTION__,
			bc->channel);
		return(-EINVAL);
	}
	if ((bc->bstate == BC_BSTATE_DEACTIVATE) ||
		(bc->bstate == BC_BSTATE_SETUP)) {
		bc->bstate = BC_BSTATE_CLEANUP;
		ret = if_link(bc->manager->nst,
			(ifunc_t)bc->manager->man2stack, BC_CLEANUP | REQUEST,
			bc->l3id, 0, NULL, 0);
	} else
		ret = EBUSY;
	return(ret);
}

static int
clear_bc(bchannel_t *bc)
{
	pthread_mutex_lock(&bc->lock);
	free_ibuffer(bc->sbuf);
	bc->sbuf = NULL;
	free_ibuffer(bc->rbuf);
	bc->rbuf = NULL;
	if (bc->Flags & FLG_BC_RECORDING)
		close_record_files(bc);
	bc->Flags = 0;
	bc->nr[0] = 0;
	bc->msn[0] = 0;
	bc->display[0] = 0;
	bc->usednr = NULL;
	bc->smsg = NULL;
	pthread_mutex_unlock(&bc->lock);
	if ((bc->bstate == BC_BSTATE_ACTIV) ||
		(bc->bstate == BC_BSTATE_ACTIVATE))
			deactivate_bchannel(bc);
	return(0);
}

static int
do_b_activated(bchannel_t *bc, mISDNuser_head_t *hh, msg_t *msg) {
	dprint(DBGM_BC,"%s:ch%d state(%d/%d) Flags(%x) smsg(%p)\n", __FUNCTION__,
		bc->channel, bc->cstate, bc->bstate, bc->Flags, bc->smsg);
	clear_ibuffer(bc->rbuf);
	if (!(bc->Flags & FLG_BC_KEEP_SEND))
		clear_ibuffer(bc->sbuf);
	if (bc->sbuf && bc->sbuf->wsem)
		sem_post(bc->sbuf->wsem);
	if (bc->bstate == BC_BSTATE_ACTIVATE)
		bc->bstate = BC_BSTATE_ACTIV;
	free_msg(msg);
	return(0);
}

static int
do_b_deactivated(bchannel_t *bc, mISDNuser_head_t *hh, msg_t *msg) {
	dprint(DBGM_BC,"%s:ch%d Flags(%x) smsg(%p)\n", __FUNCTION__,
		bc->channel, bc->Flags, bc->smsg);
	bc_cleanup(bc);
	free_msg(msg);
	return(0);
}

static int
do_b_setup_conf(bchannel_t *bc, mISDNuser_head_t *hh, msg_t *msg)
{
	int	*addr;

	addr = (int *)msg->data;
	bc->b_addr = *addr;
	activate_bchannel(bc);
	free_msg(msg);
	return(0);
}


static int
do_b_cleanup_conf(bchannel_t *bc, mISDNuser_head_t *hh, msg_t *msg)
{
	dprint(DBGM_BC,"%s:ch%d bst(%d)\n", __FUNCTION__,
		bc->channel, bc->bstate);
	bc->b_addr = 0;
	if (bc->cstate == BC_CSTATE_NULL) {
		bc->l3id = 0;
		bc->cstate = BC_CSTATE_NULL;
	}
	bc->bstate = BC_BSTATE_NULL;
	free_msg(msg);
	return(0);
}


static int
do_b_data_cnf(bchannel_t *bc, mISDNuser_head_t *hh, msg_t *msg)
{
	bc->smsg = NULL;
	if (bc->sbuf && bc->sbuf->rsem)
		sem_post(bc->sbuf->rsem);
	free_msg(msg);
	return(0);
}

static int
do_b_data_ind(bchannel_t *bc, mISDNuser_head_t *hh, msg_t *msg)
{
	int		len;
	int		ret = 0;

	if (bc->bstate != BC_BSTATE_ACTIV)
		return(-EBUSY);
	dprint(DBGM_BCDATA, "%s:ch%d get %d bytes\n", __FUNCTION__,
		bc->channel, msg->len);
	if (bc->rbuf) {
		len = ibuf_freecount(bc->rbuf);
		if (msg->len > len)
			ret = -ENOSPC;
		else {
			ibuf_memcpy_w(bc->rbuf, msg->data, msg->len);
		}
		if (bc->rbuf->rsem)
			sem_post(bc->rbuf->rsem);
	} else
		ret = -EINVAL;
	dprint(DBGM_BCDATA, "%s: finish ret %d\n", __FUNCTION__, ret);
	if (bc->Flags & FLG_BC_RECORD) {
		if (bc->Flags & FLG_BC_RECORDING) {
			write(bc->rrid, msg->data, msg->len);
		} else {
			if (!open_record_files(bc))
				write(bc->rrid, msg->data, msg->len);
		}
	} else if (bc->Flags & FLG_BC_RECORDING) {
		close_record_files(bc);
	}
	if (!ret)
		free_msg(msg);
	return(ret);
}

static int
b_send(bchannel_t *bc)
{
	int	len = 0, ret = -EINVAL;
	u_char	*p;

	if (bc->smsg)
		goto out;
	if (bc->bstate != BC_BSTATE_ACTIV)
		goto out;
	len = ibuf_usedcount(bc->sbuf);
	if (!len)
		goto out;
	if (len > MAX_DATA_SIZE)
		len = MAX_DATA_SIZE;
	dprint(DBGM_BCDATA, "%s:ch%d %d bytes\n", __FUNCTION__, bc->channel, len);
	bc->smsg = prep_l3data_msg(PH_DATA | REQUEST, bc->b_addr | FLG_MSG_DOWN,
		0, len, NULL);
	if (!bc->smsg) {
		len = -ENOMEM;
		goto out;
	}
	p = msg_put(bc->smsg, len);
	ibuf_memcpy_r(p, bc->sbuf, len);
	if (bc->Flags & FLG_BC_RECORD) {
		if (bc->Flags & FLG_BC_RECORDING) {
			write(bc->rsid, p, len);
		} else {
			if (!open_record_files(bc))
				write(bc->rsid, p, len);
		}
	} else if (bc->Flags & FLG_BC_RECORDING) {
		close_record_files(bc);
	}
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, bc->smsg);
	if (ret) {
		free_msg(bc->smsg);
		bc->smsg = NULL;
		len = ret;
	}
	if (bc->sbuf->wsem)
		sem_post(bc->sbuf->wsem);
out:
	return(len);
}

/* call handling */

static int
add_nr(bchannel_t *bc, unsigned char *cpn)
{
	if (bc->nr[0]) {
		if (*cpn>1) {
			memcpy(bc->nr + bc->nr[0] + 1, cpn + 2, *cpn -1);
			bc->nr[0] += *cpn -1;
		} else
			dprint(DBGM_BC,"%s: cpn len %d\n", __FUNCTION__, *cpn);
	} else if (*cpn)
		memcpy(bc->nr, cpn, *cpn + 1);
	dprint(DBGM_BC,"%s: nr:%s\n", __FUNCTION__, &bc->nr[2]);
	return(0);
}

static int
send_setup_ack(bchannel_t *bc)
{
	SETUP_ACKNOWLEDGE_t	*sa;
	msg_t			*msg;
	int			len, ret;
	unsigned char		*p;
	
	dprint(DBGM_BC,"%s: bc%d l3id(%x)\n", __FUNCTION__,
		bc->channel, bc->l3id);
	msg = prep_l3data_msg(CC_SETUP_ACKNOWLEDGE | REQUEST, bc->l3id,
		sizeof(SETUP_ACKNOWLEDGE_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	sa = (SETUP_ACKNOWLEDGE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_OVERLAP_REC;
	if (!(bc->Flags & FLG_BC_SENT_CID)) {
		bc->Flags |= FLG_BC_SENT_CID;
		sa->CHANNEL_ID = msg_put(msg, 2);
		sa->CHANNEL_ID[0] = 1;
		sa->CHANNEL_ID[1] = 0x88 | bc->channel;
	}
	pthread_mutex_unlock(&bc->lock);
	if (bc->Flags & FLG_BC_PROGRESS) {
		sa->PROGRESS = p = msg_put(msg, 3);;
		*p++ = 2;
		*p++ = 0x80 | CAUSE_LOC_PNET_LOCUSER;
		*p++ = 0x80 | PROGRESS_TONE;
		setup_bchannel(bc);
	}
	if (bc->display[0]) {
		len = strlen(bc->display);
		sa->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_setup(bchannel_t *bc)
{
	SETUP_t		*setup;
	msg_t		*msg;
	int		len, ret;
	unsigned char	*p;
	
	if (bc->cstate != BC_CSTATE_OCALL) {
		dprint(DBGM_BC,"%s: bc%d state(%d/%d) not OCALL\n", __FUNCTION__,
			bc->channel, bc->cstate, bc->bstate);
		return(-EINVAL);
	}
#warning testing: more crefs for S2M
	bc->l3id = 0xff00 | bc->channel;	
	msg = prep_l3data_msg(CC_SETUP | REQUEST, bc->l3id,
		sizeof(SETUP_t), 256, NULL);
	if (!msg)
		return(-ENOMEM);
	setup = (SETUP_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	switch (bc->l1_prot) {
		case ISDN_PID_L1_B_64TRANS:
			bc->bc[0] = 3;
			bc->bc[1] = 0x80;
			bc->bc[2] = 0x90;
			bc->bc[3] = 0xa3;
			break;
		default:
			dprint(DBGM_BC,"%s: no protocol %x\n", __FUNCTION__,
				bc->l1_prot);
			free_msg(msg);
			return(-ENOPROTOOPT);
	}
	setup->BEARER = p = msg_put(msg, bc->bc[0] + 1);
	memcpy(p, bc->bc, bc->bc[0] + 1);
	bc->Flags |= FLG_BC_SENT_CID;
	setup->CHANNEL_ID = msg_put(msg, 2);
	setup->CHANNEL_ID[0] = 1;
	setup->CHANNEL_ID[1] = 0x88 | bc->channel;
	if (bc->display[0]) {
		len = strlen(bc->display);
		setup->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->nr[0]) {
		setup->CALLING_PN = p = msg_put(msg, bc->nr[0] + 1);
		memcpy(p, bc->nr, bc->nr[0] + 1);
	}
	if (bc->clisub[0]) {
		setup->CALLING_SUB = p = msg_put(msg, bc->clisub[0] + 1);
		memcpy(p, bc->clisub, bc->clisub[0] + 1);
		bc->clisub[0] = 0;
	}
	if (bc->msn[0]) {
		setup->CALLED_PN = p = msg_put(msg, bc->msn[0] + 1);;
		memcpy(p, bc->msn, bc->msn[0] + 1);
	}
	if (bc->cldsub[0]) {
		setup->CALLED_SUB = p = msg_put(msg, bc->cldsub[0] + 1);
		memcpy(p, bc->cldsub, bc->cldsub[0] + 1);
		bc->cldsub[0] = 0;
	}
	if (bc->fac[0]) {
		setup->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	if (bc->uu[0]) {
		setup->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_proceeding(bchannel_t *bc)
{
	CALL_PROCEEDING_t	*proc;
	msg_t			*msg;
	int			len, ret;
	unsigned char		*p;
	
	msg = prep_l3data_msg(CC_PROCEEDING | REQUEST, bc->l3id,
		sizeof(CALL_PROCEEDING_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	proc = (CALL_PROCEEDING_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_PROCEED;
	if (!(bc->Flags & FLG_BC_SENT_CID)) {
		bc->Flags |= FLG_BC_SENT_CID;
		proc->CHANNEL_ID = msg_put(msg, 2);
		proc->CHANNEL_ID[0] = 1;
		proc->CHANNEL_ID[1] = 0x88 | bc->channel;
	}
	pthread_mutex_unlock(&bc->lock);
	if (bc->display[0]) {
		len = strlen(bc->display);
		proc->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	if (bc->manager->application) {
		bc->Flags |= FLG_BC_APPLICATION;
		len = bc->manager->application(bc->manager, PR_APP_ICALL, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, len);
	}	
	return(ret);
}

static int
send_alert(bchannel_t *bc)
{
	ALERTING_t	*at;
	msg_t		*msg;
	int		len, ret;
	unsigned char	*p;
	
	dprint(DBGM_BC, "%s: bc%d flg(%x) display(%s)\n", __FUNCTION__,
		bc->channel, bc->Flags, bc->display);
	msg = prep_l3data_msg(CC_ALERTING | REQUEST, bc->l3id,
		sizeof(ALERTING_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	at = (ALERTING_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_PROCEED;
	if (!(bc->Flags & FLG_BC_SENT_CID)) {
		bc->Flags |= FLG_BC_SENT_CID;
		at->CHANNEL_ID = msg_put(msg, 2);
		at->CHANNEL_ID[0] = 1;
		at->CHANNEL_ID[1] = 0x88 | bc->channel;
	}
	if (bc->Flags & FLG_BC_PROGRESS) {
		bc->Flags &= ~FLG_BC_PROGRESS;
		set_tone(bc, FLG_BC_TONE_ALERT);
		at->PROGRESS = p = msg_put(msg, 3);;
		*p++ = 2;
		*p++ = 0x80 | CAUSE_LOC_PNET_LOCUSER;
		*p++ = 0x80 | PROGRESS_TONE;
		setup_bchannel(bc);
	}
	pthread_mutex_unlock(&bc->lock);
	if (bc->display[0]) {
		len = strlen(bc->display);
		at->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->fac[0]) {
		at->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	if (bc->uu[0]) {
		at->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_connect(bchannel_t *bc)
{
	CONNECT_t	*conn;
	time_t		tim;
	struct tm	*ts;
	msg_t		*msg;
	int		len, ret;
	unsigned char	*p;
	
	msg = prep_l3data_msg(CC_CONNECT | REQUEST, bc->l3id,
		sizeof(CONNECT_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	conn = (CONNECT_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_PROCEED;
	bc->Flags &= ~FLG_BC_TONE;
	if (!(bc->Flags & FLG_BC_SENT_CID)) {
		bc->Flags |= FLG_BC_SENT_CID;
		conn->CHANNEL_ID = msg_put(msg, 2);
		conn->CHANNEL_ID[0] = 1;
		conn->CHANNEL_ID[1] = 0x88 | bc->channel;
	}
	pthread_mutex_unlock(&bc->lock);
	if (bc->display[0]) {
		len = strlen(bc->display);
		conn->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->fac[0]) {
		conn->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	if (bc->uu[0]) {
		conn->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	time(&tim);
	ts = localtime(&tim);
	if (ts->tm_year > 99)
		ts->tm_year -=100;
	conn->DATE = p = msg_put(msg, 6);
	*p++ = 5;
	*p++ = ts->tm_year;
	*p++ = ts->tm_mon+1;
	*p++ = ts->tm_mday;
	*p++ = ts->tm_hour;
	*p++ = ts->tm_min;

	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_connect_ack(bchannel_t *bc)
{
	CONNECT_ACKNOWLEDGE_t	*ca;
	msg_t			*msg;
	int			len, ret;
	unsigned char		*p;
	
	msg = prep_l3data_msg(CC_CONNECT | RESPONSE, bc->l3id,
		sizeof(CONNECT_ACKNOWLEDGE_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	setup_bchannel(bc);
	ca = (CONNECT_ACKNOWLEDGE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_ACTIV;
	bc->Flags &= ~FLG_BC_TONE; 
	if (!(bc->Flags & FLG_BC_SENT_CID)) {
		bc->Flags |= FLG_BC_SENT_CID;
		ca->CHANNEL_ID = msg_put(msg, 2);
		ca->CHANNEL_ID[0] = 1;
		ca->CHANNEL_ID[1] = 0x88 | bc->channel;
	}
	pthread_mutex_unlock(&bc->lock);
	if (bc->display[0]) {
		len = strlen(bc->display);
		ca->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_disc(bchannel_t *bc)
{
	DISCONNECT_t	*disc;
	msg_t		*msg;
	int		len, ret;
	unsigned char	*p;
	
	msg = prep_l3data_msg(CC_DISCONNECT | REQUEST, bc->l3id,
		sizeof(DISCONNECT_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	disc = (DISCONNECT_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_DISCONNECT;
	pthread_mutex_unlock(&bc->lock);
	if (bc->cause_val) {
		disc->CAUSE = p = msg_put(msg, 3);
		*p++ = 2;
		*p++ = 0x80 | bc->cause_loc;
		*p++ = 0x80 | bc->cause_val;
	}
	if (bc->Flags & FLG_BC_PROGRESS) {
		set_tone(bc, FLG_BC_TONE_BUSY);
		disc->PROGRESS = p = msg_put(msg, 3);;
		*p++ = 2;
		*p++ = 0x80 | CAUSE_LOC_PNET_LOCUSER;
		*p++ = 0x80 | PROGRESS_TONE;
		setup_bchannel(bc);
	}
	if (bc->display[0]) {
		len = strlen(bc->display);
		disc->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->fac[0]) {
		disc->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	if (bc->uu[0]) {
		disc->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_facility(bchannel_t *bc)
{
	FACILITY_t	*fac;
	msg_t		*msg;
	int		len, ret;
	unsigned char	*p;
	
	msg = prep_l3data_msg(CC_FACILITY | REQUEST, bc->l3id,
		sizeof(FACILITY_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	fac = (FACILITY_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	if (bc->display[0]) {
		len = strlen(bc->display);
		fac->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->fac[0]) {
		fac->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_userinfo(bchannel_t *bc)
{
	USER_INFORMATION_t	*ui;
	msg_t			*msg;
	int			ret;
	unsigned char		*p;
	
	msg = prep_l3data_msg(CC_USER_INFORMATION | REQUEST, bc->l3id,
		sizeof(USER_INFORMATION_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	ui = (USER_INFORMATION_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	if (bc->uu[0]) {
		ui->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_rel(bchannel_t *bc)
{
	RELEASE_t	*rel;
	msg_t		*msg;
	int		len, ret;
	unsigned char	*p;
	
	msg = prep_l3data_msg(CC_RELEASE | REQUEST, bc->l3id,
		sizeof(RELEASE_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	rel = (RELEASE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_RELEASE;
	pthread_mutex_unlock(&bc->lock);
	if (bc->cause_val) {
		rel->CAUSE = p = msg_put(msg, 3);
		*p++ = 2;
		*p++ = 0x80 | bc->cause_loc;
		*p++ = 0x80 | bc->cause_val;
	}
	if (bc->display[0]) {
		len = strlen(bc->display);
		rel->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->fac[0]) {
		rel->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	if (bc->uu[0]) {
		rel->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
send_relcomp(bchannel_t *bc, int l3id, int cause) {
	RELEASE_COMPLETE_t	*rc;
	msg_t			*msg;
	int			ret, len;
	unsigned char		*p;

	msg = prep_l3data_msg(CC_RELEASE_COMPLETE | REQUEST, l3id,
		sizeof(RELEASE_COMPLETE_t), 128, NULL);
	if (!msg)
		return(-ENOMEM);
	rc = (RELEASE_COMPLETE_t *)(msg->data + mISDNUSER_HEAD_SIZE);
	clear_bc(bc);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_NULL;
	pthread_mutex_unlock(&bc->lock);
	if (cause) {
		bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
		bc->cause_val = cause;
		rc->CAUSE = msg_put(msg, 3);
		rc->CAUSE[0] = 2;
		rc->CAUSE[1] = 0x80 | CAUSE_LOC_PNET_LOCUSER;
		rc->CAUSE[2] = 0x80 | cause;
	}
	if (bc->display[0]) {
		len = strlen(bc->display);
		rc->DISPLAY = p = msg_put(msg, len+1);
		*p++ = len;
		strcpy(p, bc->display);
		bc->display[0] = 0;
	}
	if (bc->fac[0]) {
		rc->FACILITY = p = msg_put(msg, bc->fac[0] + 1);
		memcpy(p, bc->fac, bc->fac[0] + 1);
		bc->fac[0] = 0;
	}
	if (bc->uu[0]) {
		rc->USER_USER = p = msg_put(msg, bc->uu[0] + 1);
		memcpy(p, bc->uu, bc->uu[0] + 1);
		bc->uu[0] = 0;
	}
	ret = -EINVAL;
	if (bc->manager->man2stack)
		ret = bc->manager->man2stack(bc->manager->nst, msg);
	if (ret)
		free_msg(msg);
	return(ret);
}

static int
info_ind(bchannel_t *bc, void *arg)
{
	INFORMATION_t	*info = arg;
	int		ret;

	if (info->CALLED_PN) {
		set_tone(bc, FLG_BC_TONE_SILENCE);
		add_nr(bc, info->CALLED_PN);
		ret = match_nr(bc->manager, bc->nr, &bc->usednr);
		dprint(DBGM_BC, "%s: match_nr ret(%d)\n", __FUNCTION__,
			ret);
		if (!ret) {
			send_proceeding(bc); 
		} else if (ret == 2 || info->COMPLETE) {
			bc->Flags |= FLG_BC_PROGRESS;
			set_tone(bc, FLG_BC_TONE_BUSY);
			bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			bc->cause_val = CAUSE_UNASSIGNED_NUMBER;
			send_disc(bc);
		}
	}
	return(0);
}

static int
setup_ind(bchannel_t *bc, int l3id, void *arg)
{
	SETUP_t		*setup = arg;
	int		cause,ret;

	if (bc->cstate != BC_CSTATE_ICALL)
		return(send_relcomp(bc, l3id, CAUSE_NOTCOMPAT_STATE));
	bc->l3id = l3id;
	cause = CAUSE_INCOMPATIBLE_DEST;
	if (setup->BEARER) {
		memcpy(bc->bc, setup->BEARER, setup->BEARER[0] +1);
		if (setup->BEARER[0] == 3) {
			if ((setup->BEARER[1] == 0x80) &&
				(setup->BEARER[2] == 0x90) &&
				(setup->BEARER[3] == 0xa3)) {
				cause = 0;
				bc->l1_prot = ISDN_PID_L1_B_64TRANS;		
			}
		}
	} else
		cause = CAUSE_MANDATORY_IE_MISS;
	if (cause)
		return(send_relcomp(bc, bc->l3id, cause));
	if (setup->CALLING_PN)
		memcpy(bc->msn, setup->CALLING_PN, setup->CALLING_PN[0] + 1);
	else
		bc->msn[0] = 0;
	if (setup->CALLING_SUB)
		memcpy(bc->clisub, setup->CALLING_SUB,
			setup->CALLING_SUB[0] + 1);
	else
		bc->clisub[0] = 0;
	if (setup->CALLED_SUB)
		memcpy(bc->cldsub, setup->CALLED_SUB,
			setup->CALLED_SUB[0] + 1);
	else
		bc->cldsub[0] = 0;
	if (setup->FACILITY)
		memcpy(bc->fac, setup->FACILITY, setup->FACILITY[0] + 1);
	else
		bc->fac[0] = 0;
	if (setup->USER_USER)
		memcpy(bc->uu, setup->USER_USER, setup->USER_USER[0] + 1);
	else
		bc->uu[0] = 0;
	if (!bc->sbuf)
		bc->sbuf = init_ibuffer(2048);
	set_tone(bc, FLG_BC_TONE_DIAL);
	if (!setup->CALLED_PN) {
		bc->Flags |= FLG_BC_PROGRESS;
		send_setup_ack(bc);
	} else {
		set_tone(bc, FLG_BC_TONE_SILENCE);
		bc->Flags |= FLG_BC_PROGRESS;
		add_nr(bc, setup->CALLED_PN);
		ret = match_nr(bc->manager, bc->nr, &bc->usednr);
		dprint(DBGM_BC, "%s: match_nr ret(%d)\n", __FUNCTION__,
			ret);
		if (!ret) {
			send_proceeding(bc); 
		} else if (ret == 2 || setup->COMPLETE) {
			return(send_relcomp(bc, bc->l3id, CAUSE_UNASSIGNED_NUMBER));
		} else {
			send_setup_ack(bc);
		}
	}
	return(0);
}

static int
conn_ind(bchannel_t *bc, void *arg)
{
	CONNECT_t	*conn = arg;
	int		ret;

	if (conn) {
		if (conn->FACILITY)
			memcpy(bc->fac, conn->FACILITY, conn->FACILITY[0] + 1);
		else
			bc->fac[0] = 0;
		if (conn->USER_USER)
			memcpy(bc->uu, conn->USER_USER, conn->USER_USER[0] + 1);
		else
			bc->uu[0] = 0;
	}
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		setup_bchannel(bc);
		ret = bc->manager->application(bc->manager, PR_APP_CONNECT, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
		if (!ret) {
			send_connect_ack(bc);
		}
	}	
	return(0);
}

static int
alert_ind(bchannel_t *bc, void *arg)
{
	ALERTING_t	*alert = arg;
	int		ret;

	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_ALERTING;
	pthread_mutex_unlock(&bc->lock);
	if (alert->FACILITY)
		memcpy(bc->fac, alert->FACILITY, alert->FACILITY[0] + 1);
	else
		bc->fac[0] = 0;
	if (alert->USER_USER)
		memcpy(bc->uu, alert->USER_USER, alert->USER_USER[0] + 1);
	else
		bc->uu[0] = 0;
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_ALERT, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}	
	return(0);
}

static int
facility_ind(bchannel_t *bc, void *arg)
{
	FACILITY_t	*fac = arg;
	int		ret;

	if (fac) {
		if (fac->FACILITY)
			memcpy(bc->fac, fac->FACILITY, fac->FACILITY[0] + 1);
		else
			bc->fac[0] = 0;
	}
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_FACILITY, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}	
	return(0);
}

static int
userinfo_ind(bchannel_t *bc, void *arg)
{
	USER_INFORMATION_t	*ui = arg;
	int			ret;

	if (ui) {
		if (ui->USER_USER)
			memcpy(bc->uu, ui->USER_USER, ui->USER_USER[0] + 1);
		else
			bc->uu[0] = 0;
	}
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_USERUSER, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}	
	return(0);
}

static int
disc_ind(bchannel_t *bc, void *arg)
{
	DISCONNECT_t	*disc = arg;
	int		cause = 0;
	int		ret;

	if (disc->CAUSE) {
		if (disc->CAUSE[0] >1) {
			dprint(DBGM_BC, "%s: loc(%d) cause(%d)\n", __FUNCTION__,
				disc->CAUSE[1] & 0xf, disc->CAUSE[2] & 0x7f);
			bc->cause_loc = disc->CAUSE[1] & 0xf;
			bc->cause_val = disc->CAUSE[2] & 0x7f;
		} else {
			dprint(DBGM_BC, "%s: cause len %d\n", __FUNCTION__,
				disc->CAUSE[0]);
			cause = CAUSE_INVALID_CONTENTS;
		}
	} else {
		cause = CAUSE_MANDATORY_IE_MISS;
	}
	if (cause) {
		bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
		bc->cause_val = cause;
	}
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_DISCONNECT;
	pthread_mutex_unlock(&bc->lock);
	send_rel(bc);
	if (disc->FACILITY)
		memcpy(bc->fac, disc->FACILITY, disc->FACILITY[0] + 1);
	else
		bc->fac[0] = 0;
	if (disc->USER_USER)
		memcpy(bc->uu, disc->USER_USER, disc->USER_USER[0] + 1);
	else
		bc->uu[0] = 0;
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_HANGUP, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}	
	return(0);
}

static int
rel_ind(bchannel_t *bc, void *arg)
{
	RELEASE_t	*rel = arg;
	int		ret;

	if (rel) {
		if (rel->FACILITY)
			memcpy(bc->fac, rel->FACILITY, rel->FACILITY[0] + 1);
		else
			bc->fac[0] = 0;
		if (rel->USER_USER)
			memcpy(bc->uu, rel->USER_USER, rel->USER_USER[0] + 1);
		else
			bc->uu[0] = 0;
		if (rel->CAUSE) {
			if (rel->CAUSE[0] > 1) {
				dprint(DBGM_BC, "%s: loc(%d) cause(%d)\n", __FUNCTION__,
					rel->CAUSE[1] & 0xf, rel->CAUSE[2] & 0x7f);
				bc->cause_loc = rel->CAUSE[1] & 0xf;
				bc->cause_val = rel->CAUSE[2] & 0x7f;
			}
		}
	}
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_CLEAR, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}	
	clear_bc(bc);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_NULL;
	pthread_mutex_unlock(&bc->lock);
	return(0);
}

static int
relcmpl_ind(bchannel_t *bc, void *arg)
{
	RELEASE_COMPLETE_t	*rc = arg;
	int			ret;

	if (rc) {
		if (rc->FACILITY)
			memcpy(bc->fac, rc->FACILITY, rc->FACILITY[0] + 1);
		else
			bc->fac[0] = 0;
		if (rc->USER_USER)
			memcpy(bc->uu, rc->USER_USER, rc->USER_USER[0] + 1);
		else
			bc->uu[0] = 0;
		if (rc->CAUSE) {
			if (rc->CAUSE[0] > 1) {
				dprint(DBGM_BC, "%s: loc(%d) cause(%d)\n", __FUNCTION__,
					rc->CAUSE[1] & 0xf, rc->CAUSE[2] & 0x7f);
				bc->cause_loc = rc->CAUSE[1] & 0xf;
				bc->cause_val = rc->CAUSE[2] & 0x7f;
			}
		}
	}
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_CLEAR, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}	
	clear_bc(bc);
	pthread_mutex_lock(&bc->lock);
	bc->cstate = BC_CSTATE_NULL;
	pthread_mutex_unlock(&bc->lock);
	return(0);
}

static int
relcr_ind(bchannel_t *bc, void *arg)
{
	int	ret, *err = arg;
	
	dprint(DBGM_BC, "%s: bc%d cause(%x)\n", __FUNCTION__,
		bc->channel, *err);
	if ((bc->Flags & FLG_BC_APPLICATION) && bc->manager->application) {
		ret = bc->manager->application(bc->manager, PR_APP_CLEAR, bc);
		dprint(DBGM_BC, "%s: bc%d application ret(%d)\n", __FUNCTION__,
			bc->channel, ret);
	}
	if (bc->cstate != BC_CSTATE_NULL) {
		clear_bc(bc);
		pthread_mutex_lock(&bc->lock);
		bc->cstate = BC_CSTATE_NULL;
		pthread_mutex_unlock(&bc->lock);
	}
	return(0);
}

static void
cleanup_bchannel(void *arg)
{
	bchannel_t	*bc = arg;

	dprint(DBGM_BC,"%s: bc %d\n", __FUNCTION__, bc->channel);
	pthread_mutex_lock(&bc->lock);
	msg_queue_purge(&bc->workq);
	bc->smsg = NULL;
	free_ibuffer(bc->sbuf);
	bc->sbuf = NULL;
	free_ibuffer(bc->rbuf);
	bc->rbuf = NULL;
	bc->cstate = BC_CSTATE_NULL;
	while(1)
		if (!sem_trywait(&bc->work))
			break;
	pthread_mutex_unlock(&bc->lock);
	dprint(DBGM_BC,"%s: bc %d end\n", __FUNCTION__, bc->channel);
}

static void *
main_bc_task(void *arg)
{
	bchannel_t	*bc = arg;
	msg_t		*msg;
	int		ret, id;
	mISDNuser_head_t	*hh;

	pthread_cleanup_push(cleanup_bchannel, (void *)bc);
	dprint(DBGM_BC,"%s bc %d\n", __FUNCTION__, bc->channel);
	while(1) {
		
		sem_wait(&bc->work);
		if (bc->Flags & FLG_BC_TERMINATE)
			pthread_exit(NULL);
		if (!bc->smsg) { 
			if (bc->Flags & FLG_BC_TONE)
				tone_handler(bc); 
			if (ibuf_usedcount(bc->sbuf))
				b_send(bc);
		}
		msg = msg_dequeue(&bc->workq);
		if (msg) {
			hh = (mISDNuser_head_t *)msg->data;
			msg_pull(msg, mISDNUSER_HEAD_SIZE);
			dprint(DBGM_BC,"%s: bc%d st(%d/%d) prim(%x) dinfo(%x) len(%d)\n", __FUNCTION__,
				bc->channel, bc->cstate, bc->bstate, hh->prim, hh->dinfo, msg->len);
			ret = -EINVAL;
			switch(hh->prim) {
				case PH_DATA | INDICATION:
					ret = do_b_data_ind(bc, hh, msg);
					break;
				case PH_DATA | CONFIRM:
					ret = do_b_data_cnf(bc, hh, msg);
					break;
				case PH_ACTIVATE | INDICATION:
				case PH_ACTIVATE | CONFIRM:
					ret = do_b_activated(bc, hh, msg);
					break;
				case PH_DEACTIVATE | INDICATION:
				case PH_DEACTIVATE | CONFIRM:
					ret = do_b_deactivated(bc, hh, msg);
					break;
				case BC_SETUP | CONFIRM:
					ret = do_b_setup_conf(bc, hh, msg);
					break;
				case BC_SETUP | SUB_ERROR:
				case BC_CLEANUP | SUB_ERROR:
					wprint("%s:ch%d %x error %x\n", __FUNCTION__,
						bc->channel, hh->prim, *((int *)msg->data));
				case BC_CLEANUP | CONFIRM:
					ret = do_b_cleanup_conf(bc, hh, msg);
					break;

				case CC_SETUP | INDICATION:
					setup_ind(bc, hh->dinfo, msg->data);
					break;
				case CC_SETUP | CONFIRM:
					bc->l3id = *((int *)msg->data);
					break;
				case CC_NEW_CR | INDICATION:
					pthread_mutex_lock(&bc->lock);
					id = *((int *)msg->data);
					msg_push(msg, mISDNUSER_HEAD_SIZE);
					if (bc->manager && bc->manager->man2stack)
						ret = bc->manager->man2stack(
							bc->manager->nst, msg);
					bc->l3id = id;
					pthread_mutex_unlock(&bc->lock);
					break;
				case CC_RELEASE_CR | INDICATION:
					relcr_ind(bc, msg->data);
					break;
				case CC_INFORMATION | INDICATION:
					info_ind(bc, msg->data);
					break;
				case CC_ALERTING | INDICATION:
					alert_ind(bc, msg->data);
					break;
				case CC_CONNECT | INDICATION:
					conn_ind(bc, msg->data);
					break;
				case CC_FACILITY | INDICATION:
					facility_ind(bc, msg->data);
					break;
				case CC_USER_INFORMATION | INDICATION:
					userinfo_ind(bc, msg->data);
					break;
				case CC_DISCONNECT | INDICATION:
					disc_ind(bc, msg->data);
					break;
				case CC_RELEASE | INDICATION:
					rel_ind(bc, msg->data);
					break;
				case CC_RELEASE | CONFIRM:
					rel_ind(bc, NULL);
					break;
				case CC_RELEASE_COMPLETE | INDICATION:
					relcmpl_ind(bc, msg->data);
					break;

				case CC_SETUP | REQUEST:
					send_setup(bc);
					break;
				case CC_ALERTING | REQUEST:
					send_alert(bc);
					break;
				case CC_CONNECT | REQUEST:
					send_connect(bc);
					break;
				case CC_DISCONNECT | REQUEST:
					send_disc(bc);
					break;
				case CC_FACILITY | REQUEST:
					send_facility(bc);
					break;
				case CC_USER_INFORMATION | REQUEST:
					send_userinfo(bc);
					break;
				case CC_TIMEOUT | INDICATION:
					dprint(DBGM_MAN,"%s: bc%d got CC_TIMEOUT\n", __FUNCTION__,
						bc->channel);
					break;
				default:
					wprint("%s:ch%d unhandled prim(%x) di(%x)\n", __FUNCTION__,
						bc->channel, hh->prim, hh->dinfo);
					break;
			}
			if (ret)
				free_msg(msg);
		}
	}
	pthread_cleanup_pop(1);
	return(NULL);
}


int
init_bchannel(bchannel_t *bc, int channel)
{
	int	ret;

	bc->channel = channel;
	msg_queue_init(&bc->workq);
	bc->cstate = BC_CSTATE_NULL;
	bc->bstate = BC_BSTATE_NULL;
	pthread_mutex_init(&bc->lock, NULL);
	sem_init (&bc->work, 0, 0);
	ret = pthread_create(&bc->tid, NULL, main_bc_task, (void *)bc);
	dprint(DBGM_BC, "%s: create bc%d thread %ld ret %d\n", __FUNCTION__,
		channel, bc->tid, ret);
	return(0);
}

int
term_bchannel(bchannel_t *bc)
{
	dprint(DBGM_BC, "%s: bc%d\n", __FUNCTION__, bc->channel);
	bc->Flags |= FLG_BC_TERMINATE;
	sem_post(&bc->work);
	return(0);
}
