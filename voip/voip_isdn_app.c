#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "isdn_net.h"
#include "l3dss1.h"
#include "globals.h"
#include "iapplication.h"


static void MsgAddIE(msg_t *msg, u_char ie, u_char *iep, int reset) {
	int	l;
	u_char	*p;

	if (ie & 0x80)
		l = 1;
	else {
		if (iep && *iep)
			l = 2 + *iep;
		else
			return;
	}
	p = msg_put(msg, l);
	*p++ = ie;
	l--;
	if (l) {
		memcpy(p, iep, l);
		if (reset)
			*iep = 0;
	}
}

static msg_t *
make_msg_head(int size, u_char mt) {
	u_char	*p;
	msg_t	*msg;

	msg = alloc_msg(size);
	if (msg) {
		p = msg_put(msg, 3);
		p++;
		p++;
		*p++ = mt;
	}
	return(msg);
}

int
alert_voip(iapplication_t *ap, bchannel_t *bc)
{
	msg_t	*msg;

	if (!ap->con)
		return(-EBUSY);
	msg = make_msg_head(1024, MT_ALERTING);
	if (msg) {
		MsgAddIE(msg, IE_FACILITY, bc->fac, 1);
		MsgAddIE(msg, IE_DISPLAY, bc->display, 1);
		MsgAddIE(msg, IE_USER_USER, bc->uu, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}

int
facility_voip(iapplication_t *ap, bchannel_t *bc)
{
	msg_t	*msg;

	if (!ap->con)
		return(-EBUSY);
	msg = make_msg_head(1024, MT_FACILITY);
	if (msg) {
		MsgAddIE(msg, IE_FACILITY, bc->fac, 1);
		MsgAddIE(msg, IE_DISPLAY, bc->display, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}


int
useruser_voip(iapplication_t *ap, bchannel_t *bc)
{
	msg_t	*msg;

	if (!ap->con)
		return(-EBUSY);
	msg = make_msg_head(1024, MT_USER_INFORMATION);
	if (msg) {
		MsgAddIE(msg, IE_USER_USER, bc->uu, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}


int
connect_voip(iapplication_t *ap, bchannel_t *bc)
{
	int	ret;
	msg_t	*msg;

	if (!ap->con)
		return(-EBUSY);
	ret = setup_voip(ap, bc);
	if (ret) {
		bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
		bc->cause_val = CAUSE_NO_ROUTE;
		ap->mgr->app_bc(ap->mgr, PR_APP_HANGUP, bc);
		return(-EBUSY);
	}
	msg = make_msg_head(1024, MT_CONNECT);
	if (msg) {
		MsgAddIE(msg, IE_FACILITY, bc->fac, 1);
		MsgAddIE(msg, IE_DISPLAY, bc->display, 1);
		MsgAddIE(msg, IE_USER_USER, bc->uu, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}

int
disconnect_voip(iapplication_t *ap, bchannel_t *bc)
{
	msg_t   *msg;
	u_char	cause[8];

	if (!ap->con)
		return(-EBUSY);
	msg = make_msg_head(1024, MT_DISCONNECT);
	if (msg) {
		cause[0] = 2;
		cause[1] = 0x80 | bc->cause_loc;
		cause[2] = 0x80 | bc->cause_val;
		MsgAddIE(msg, IE_CAUSE, cause, 0);
		MsgAddIE(msg, IE_FACILITY, bc->fac, 1);
		MsgAddIE(msg, IE_DISPLAY, bc->display, 1);
		MsgAddIE(msg, IE_USER_USER, bc->uu, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}

int
release_voip(iapplication_t *ap, bchannel_t *bc)
{
	msg_t   *msg;
	u_char	cause[8];

	if (!ap->con)
		return(-EBUSY);
	msg = make_msg_head(1024, MT_RELEASE);
	if (msg) {
		cause[0] = 2;
		cause[1] = 0x80 | bc->cause_loc;
		cause[2] = 0x80 | bc->cause_val;
		MsgAddIE(msg, IE_CAUSE, cause, 0);
		MsgAddIE(msg, IE_FACILITY, bc->fac, 1);
		MsgAddIE(msg, IE_DISPLAY, bc->display, 1);
		MsgAddIE(msg, IE_USER_USER, bc->uu, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}

int
setup_voip_ocall(iapplication_t *ap, bchannel_t *bc) {
	msg_t		*msg;
	struct in_addr	addr;
	struct hostent	*h;

	if (!ap->con) {
		if ((addr.s_addr = inet_addr(bc->usednr->name)) == -1) {
			h = gethostbyname(bc->usednr->name);
			if (!h) {
				return(-ENXIO);
			}
			memcpy(&addr.s_addr, h->h_addr, sizeof(addr.s_addr));
		}
		ap->con = new_connection(ap, &addr);
		if (!ap->con) {
			return(-ENOMEM);
		}
		if (bc->usednr->flags & FLAG_GSM) {
			ap->con->pkt_size = 640;
			ap->con->sndflags |= SNDFLG_COMPR_GSM;
		}
		ap->con->own_ssrc = getnew_ssrc(ap->vapp);
	}
	msg = make_msg_head(1024, MT_SETUP);
	if (msg) {
		MsgAddIE(msg, IE_FACILITY, bc->fac, 1);
		MsgAddIE(msg, IE_BEARER, bc->bc, 0);
		MsgAddIE(msg, IE_DISPLAY, bc->display, 1);
		MsgAddIE(msg, IE_CALLING_PN, bc->msn, 0);
		MsgAddIE(msg, IE_CALLING_SUB, bc->clisub, 1);
		MsgAddIE(msg, IE_CALLED_PN, bc->nr, 0);
		MsgAddIE(msg, IE_CALLED_SUB, bc->cldsub, 1);
		MsgAddIE(msg, IE_USER_USER, bc->uu, 1);
		msg_queue_tail(&ap->con->aqueue, msg);
		return(SendCtrl(ap));
	}
	return(-ENOMEM);
}

static int
parse_isdn_extra(unsigned char *arg, int len, bchannel_t *bc)
{
	unsigned char	*p;

	p = findie(arg, len, IE_DISPLAY, 0);
	if (p) {
		memcpy(bc->display, p + 1, *p);
		bc->display[*p] = 0;
	}
	p = findie(arg, len, IE_USER_USER, 0);
	if (p)
		memcpy(bc->uu, p, *p + 1);
	p = findie(arg, len, IE_FACILITY, 0);
	if (p)
		memcpy(bc->fac, p, *p + 1);
	return(0);
}

static int
parse_isdn_setup(iapplication_t *appl, unsigned char *arg, int len)
{
	manager_t	*mgr = appl->vapp->mgr_lst;
	unsigned char	*own, *p;
	nr_list_t	*nrx;
	bchannel_t	*bc = NULL;
	int		ret;

	if (appl->mgr) { /* allready setup */
		SendCtrl(appl);
		return(-EINVAL);
	}
	own = findie(arg, len, IE_CALLED_PN, 0);
	if (!own) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	while(mgr) {
		nrx = NULL;
		if (!match_nr(mgr, own, &nrx)) {
			ret = mgr->app_bc(mgr, PR_APP_OCHANNEL, &bc);
			if (0 >= ret)
				eprint( "%s: no free channel ret(%d)\n", __FUNCTION__,
					ret);
			if (!bc) {
				eprint( "%s: no free channel\n", __FUNCTION__);
			} else {
				appl->mgr = mgr;
				appl->data1 = bc;
				appl->mode = AP_MODE_VOIP_ICALL;
				bc->app = appl;
				bc->usednr = nrx;
				break;
			}
		}
		mgr = mgr->next;
	}
	if (!mgr) {
	} else {
		p = findie(arg, len, IE_CALLING_PN, 0);
		if (p)
			memcpy(bc->nr, p, *p + 1);
		p = findie(arg, len, IE_CALLING_SUB, 0);
		if (p)
			memcpy(bc->clisub, p, *p + 1);
		p = findie(arg, len, IE_CALLED_SUB, 0);
		if (p)
			memcpy(bc->cldsub, p, *p + 1);
		parse_isdn_extra(arg, len, bc);
		bc->Flags |= FLG_BC_APPLICATION;
		memcpy(bc->msn, own, own[0] + 1);
		bc->l1_prot = ISDN_PID_L1_B_64TRANS;
		if (!bc->display[0] && appl->con) {
			strcpy(bc->display, appl->con->con_hostname);
		}
		mgr->app_bc(mgr, PR_APP_OCALL, bc);
	}
	SendCtrl(appl);
	return(0);
}

static int
parse_isdn_alert(iapplication_t *appl, unsigned char *arg, int len)
{
	bchannel_t	*bc = appl->data1;

	if (!appl->mgr || !bc) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	parse_isdn_extra(arg, len, bc);
	if (!bc->display[0] && bc->usednr) {
		strcpy(bc->display, bc->usednr->name);
	}
	bc->Flags |= FLG_BC_PROGRESS;
	appl->mgr->app_bc(appl->mgr, PR_APP_ALERT, bc);
	SendCtrl(appl);
	return(0);
}

int
parse_isdn_connect(iapplication_t *appl, unsigned char *arg, int len)
{
	bchannel_t	*bc = appl->data1;
	int		ret;

	if (!appl->mgr || !bc) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	parse_isdn_extra(arg, len, bc);
	ret = setup_voip(appl, bc);
	if (ret) {
		bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
		bc->cause_val = CAUSE_NO_ROUTE;
		appl->mgr->app_bc(appl->mgr, PR_APP_HANGUP, bc);
		return(0);
	}
	appl->Flags |= AP_FLG_VOIP_ACTIV;
	appl->mgr->app_bc(appl->mgr, PR_APP_CONNECT, bc);
	SendCtrl(appl);
	return(0);
}

static int
parse_isdn_disc(iapplication_t *appl, unsigned char *arg, int len)
{
	bchannel_t	*bc = appl->data1;
	unsigned char	*p;

	if (!appl->mgr || !bc) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	p = findie(arg, len, IE_CAUSE, 0);
	if (p) {
		if (*p++ > 1) {
			bc->cause_loc = *p++ & 0xf;
			bc->cause_val = *p++ & 0x7f;
		} else {
			bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			bc->cause_val = CAUSE_NORMAL_CLEARING;
		}
	} else {
		bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
		bc->cause_val = CAUSE_NORMAL_CLEARING;
	}
	parse_isdn_extra(arg, len, bc);
	if (appl->Flags & AP_FLG_VOIP_ACTIV) {
		close_voip(appl, bc);
	}
	bc->Flags |= FLG_BC_PROGRESS;
	appl->mgr->app_bc(appl->mgr, PR_APP_HANGUP, bc);
	SendCtrl(appl);
	return(0);
}

static int
parse_isdn_release(iapplication_t *appl, unsigned char *arg, int len)
{
	bchannel_t	*bc = appl->data1;
	unsigned char	*p;

	if (!appl->mgr || !bc) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	p = findie(arg, len, IE_CAUSE, 0);
	if (p) {
		if (*p++ > 1) {
			bc->cause_loc = *p++ & 0xf;
			bc->cause_val = *p++ & 0x7f;
		} else {
			bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
			bc->cause_val = CAUSE_NORMAL_CLEARING;
		}
	} else {
		bc->cause_loc = CAUSE_LOC_PNET_LOCUSER;
		bc->cause_val = CAUSE_NORMAL_CLEARING;
	}
	parse_isdn_extra(arg, len, bc);
	if (appl->Flags & AP_FLG_VOIP_ACTIV) {
		close_voip(appl, bc);
	}
	bc->Flags |= FLG_BC_PROGRESS;
	appl->mgr->app_bc(appl->mgr, PR_APP_HANGUP, bc);
	SendCtrl(appl);
	return(0);
}

static int
parse_isdn_uinfo(iapplication_t *appl, unsigned char *arg, int len)
{
	bchannel_t	*bc = appl->data1;

	if (!appl->mgr || !bc) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	parse_isdn_extra(arg, len, bc);
	appl->mgr->app_bc(appl->mgr, PR_APP_USERUSER, bc);
	SendCtrl(appl);
	return(0);
}

static int
parse_isdn_fac(iapplication_t *appl, unsigned char *arg, int len)
{
	bchannel_t	*bc = appl->data1;

	if (!appl->mgr || !bc) {
		SendCtrl(appl);
		return(-EINVAL);
	}
	parse_isdn_extra(arg, len, bc);
	appl->mgr->app_bc(appl->mgr, PR_APP_FACILITY, bc);
	SendCtrl(appl);
	return(0);
}

static int
parse_isdn_packet(iapplication_t *appl, unsigned char *arg) {
	unsigned char	*p, oc, pc, pr, a_pc;
	int		len;
	vconnection_t	*con;

	con = appl->con;
	if (!con)
		return(-EINVAL);
	p = arg + 2;
	len = ntohs(*((unsigned short *)p));
	len *= 4;
	p = arg + 12;
	len -= 8;
	if (len<=0)
		return(-EINVAL);
	
	oc = *p++;
	pc = *p;
	*p++ = 0; /* to use L3 findie, fake a dummy CR L3 frame */
	pr = *p++;
	dprint(DBGM_ISDN, -1,  "%s: pr(%02x) own(%d/%d) peer(%d/%d)\n", __FUNCTION__,
		pr, oc, con->oc, pc, con->pc); 
	a_pc = con->pc;
	a_pc++;
	if (con->oc == oc) {
		if (con->amsg) {
			free_msg(con->amsg);
			con->amsg = NULL;
		}
	}
	if (a_pc != pc) {
	} else
		con->pc = pc;

	if (pr == 0) { /* escape to private MTs */
		pr = *p++;
		if (pr == 0x81) { /* RR */
			return(0);
		}
		return(-EINVAL);
	} else if (pr == MT_SETUP) {
		return(parse_isdn_setup(appl, arg + 12, len));
	} else if (pr == MT_ALERTING) {
		return(parse_isdn_alert(appl, arg + 12, len));
	} else if (pr == MT_CONNECT) {
		return(parse_isdn_connect(appl, arg + 12, len));
	} else if (pr == MT_DISCONNECT) {
		return(parse_isdn_disc(appl, arg + 12, len));
	} else if (pr == MT_RELEASE) {
		return(parse_isdn_release(appl, arg + 12, len));
	} else if (pr == MT_USER_INFORMATION) {
		return(parse_isdn_uinfo(appl, arg + 12, len));
	} else if (pr == MT_FACILITY) {
		return(parse_isdn_fac(appl, arg + 12, len));
	}
	return(0);
}

int
voip_application_handler(iapplication_t *appl, int prim, unsigned char *arg) {

	dprint(DBGM_APPL, -1,  "%s(%p, %x, %p)\n", __FUNCTION__,
		appl, prim, arg);

	if (prim == AP_PR_VOIP_NEW) {
		
	} else if (prim == AP_PR_VOIP_ISDN) {
		return(parse_isdn_packet(appl, arg));
	}
	return(-EINVAL);
}

