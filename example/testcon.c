/*
 *
 * Copyright 2008 Karsten Keil <kkeil@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <mISDN/mISDNif.h>
#include <mISDN/q931.h>
#include <mISDN/mlayer3.h>

/* We do not have the all the ioctl controls mainstream yet so define it here.
 * It should still work then with the old standalone driver
 */

#ifndef MISDN_CTRL_L1_TESTS
#define MISDN_CTRL_L1_TESTS		0x00010000
#define MISDN_CTRL_L1_STATE_TEST	0x00010001
#define MISDN_CTRL_L1_AIS_TEST		0x00010002
#define MISDN_CTRL_L1_TS0_MODE		0x00010003
#define MISDN_CTRL_L1_GET_SYNC_INFO	0x00010004
#endif

void usage(char *pname)
{
	fprintf(stderr,"Call with %s [options] [filename]\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"       filename   filename.in  incoming data\n");
	fprintf(stderr,"                  filename.out outgoing data\n");
	fprintf(stderr,"                  data is alaw for voice\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 0)\n"); 
	fprintf(stderr,"  -F<n>           use function n (default 0)\n"); 
	fprintf(stderr,"                    0 send and receive voice\n"); 
	fprintf(stderr,"                    1 send touchtones\n"); 
	fprintf(stderr,"                    2 receive touchtones\n"); 
	fprintf(stderr,"                    3 send and receive hdlc data\n"); 
	fprintf(stderr,"                    4 send and receive X75 data\n"); 
	fprintf(stderr,"                    5 send and receive voice early B connect\n");
	fprintf(stderr,"                    6 loop back voice - autohangup after 30 sec\n");
	fprintf(stderr,"                    7 loop back voice - do not auto hangup\n");
	fprintf(stderr,"                    8 set loop back B1 permanent\n");
	fprintf(stderr,"                    9 set loop back B2 permanent\n");
	fprintf(stderr,"                   10 set loop back D permanent\n");
	fprintf(stderr,"                   11 clear all loopbacks\n");
	fprintf(stderr,"                   12 L1 Timer3 value to given -n value (allowed values 5-30)\n");
	fprintf(stderr,"                   13 L1 Test AIS set)\n");
	fprintf(stderr,"                   14 L1 Test AIS cleared)\n");
	fprintf(stderr,"                   15 L1 Test set state machine  to given -n value (allowed values 0-7, 7 - auto state enabled)\n");
	fprintf(stderr,"                   16 Reject calls with cause values (given via -n)\n");
	fprintf(stderr,"  -n <phone nr>   Phonenumber to dial or on -F12 T3 value\n");
	fprintf(stderr,"  -vn             Printing debug info level n\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int			cardnr;
	int			func;
	char			phonenr[32];
	unsigned char		cause;
	int			layer2;
	struct sockaddr_mISDN	l2addr;
	int			bchan;
	struct sockaddr_mISDN	baddr;
	int			nds;
	int			dproto;
	int			bproto;
	int			used_bchannel;
	int			save;
	int			play;
	FILE			*fplay;
	int			flag;
	int			val;
	int			cr;
	int			si;
	int			timeout;
	int			setloopback;
} devinfo_t;

#define FLG_SEND_TONE		0x0001
#define FLG_SEND_DATA		0x0002
#define FLG_BCHANNEL_SETUP	0x0010
#define FLG_BCHANNEL_DOACTIVE	0x0020
#define FLG_BCHANNEL_ACTIVE	0x0040
#define FLG_BCHANNEL_ACTDELAYED	0x0080
#define FLG_CALL_ORGINATE	0x0100
#define FLG_BCHANNEL_EARLY	0x0200
#define FLG_CALL_ACTIVE		0x0400
#define FLG_BCHANNEL_LOOP	0x0800
#define FLG_BCHANNEL_LOOPSET	0x1000

#define MAX_REC_BUF		4000
#define MAX_DATA_BUF		1024

static int VerifyOn=0;
#ifdef NOTYET
static char tt_char[] = "0123456789ABCD*#";
#endif

#define PLAY_SIZE 64

#define	MsgHead(ptr, cref, mty) do { \
	*ptr++ = 0x8; \
	if (cref == -1) { \
		*ptr++ = 0x0; \
	} else { \
		*ptr++ = 0x1; \
		*ptr++ = cref^0x80; \
	} \
	*ptr++ = mty; \
} while(0)

static int play_msg(devinfo_t *di) {
	unsigned char buf[PLAY_SIZE + MISDN_HEADER_LEN];
	struct  mISDNhead *hh = (struct  mISDNhead *)buf;
	int len, ret;
	
	if (di->play < 0)
		return 0;
	len = read(di->play, buf + MISDN_HEADER_LEN, PLAY_SIZE);
	if (len<1) {
		if (len<0)
			printf("play_msg err %d: \"%s\"\n", errno, strerror(errno));
		close(di->play);
		di->play = -1;
		return 0;
	}
	
	hh->prim = PH_DATA_REQ;
	hh->id = 0;
	ret = sendto(di->bchan, buf, len + MISDN_HEADER_LEN, 0, NULL, 0);
	if (ret < 0)
		fprintf(stdout,"play send error %d %s\n", errno, strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"play send ret=%d\n", ret);
	return ret;
}

static int send_data(devinfo_t *di) {
	char buf[MAX_DATA_BUF + MISDN_HEADER_LEN];
	struct  mISDNhead *hh = (struct  mISDNhead *)buf;
	char *data;
	int len, ret;
	
	if (di->play<0 || !di->fplay)
		return 0;
	if (!(data = fgets(buf + MISDN_HEADER_LEN, MAX_DATA_BUF, di->fplay))) {
		close(di->play);
		di->play = -1;
		data = buf + MISDN_HEADER_LEN;
		data[0] = 4; /* ctrl-D */
		data[1] = 0;
	}
	len = strlen(data);
	if (len==0) {
		close(di->play);
		di->play = -1;
		data[0] = 4; /* ctrl-D */
		len = 1;
	}
	
	hh->prim = DL_DATA_REQ;
	hh->id = 0;
	ret = sendto(di->bchan, buf, len + MISDN_HEADER_LEN, 0, NULL, 0);
	if (ret < 0)
		fprintf(stdout,"send_data error %d %s\n", errno, strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"send_data ret=%d\n", ret);
	return ret;
}

static int setup_bchannel(devinfo_t *di) {
	int			ret;
	struct sockaddr_mISDN	addr;

	if (VerifyOn>3)
		fprintf(stdout,"%s used_bchannel %d\n", __FUNCTION__, di->used_bchannel);
	if (di->flag & FLG_BCHANNEL_SETUP)
		return 0;
	if ((di->used_bchannel < 1) || (di->used_bchannel > 2)) {
		fprintf(stdout, "wrong channel %d\n", di->used_bchannel);
		return 1;
	}
	di->bchan = socket(PF_ISDN, SOCK_DGRAM, di->bproto);
	if (di->bchan < 0) {
		fprintf(stdout, "could not open bchannel socket %s\n", strerror(errno));
		return 2;
	}

	if (di->bchan > di->nds - 1)
		di->nds = di->bchan + 1;

	ret = fcntl(di->bchan, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stdout, "fcntl error %s\n", strerror(errno));
		return 3;
	}

	addr.family = AF_ISDN;
	addr.dev = di->cardnr;
	addr.channel = di->used_bchannel;
	
	ret = bind(di->bchan, (struct sockaddr *) &addr, sizeof(addr));

	if (ret < 0) {
		fprintf(stdout, "could not bind bchannel socket %s\n", strerror(errno));
		return 4;
	}
	return ret;
}

static int send_SETUP(devinfo_t *di, int SI, char *PNr) {
	char			*msg, buf[64];
	char			*np,*p;
	struct  mISDNhead 	*hh = (struct  mISDNhead *)buf;
	int			ret, len;

	p = msg = buf + MISDN_HEADER_LEN;
	MsgHead(p, di->cr, MT_SETUP);
	*p++ = 0xa1; /* complete indicator */
	*p++ = IE_BEARER;
	if (SI == 1) { /* Audio */
		*p++ = 0x3;	/* Length                               */
		*p++ = 0x90;	/* Coding Std. CCITT, 3.1 kHz audio     */
		*p++ = 0x90;	/* Circuit-Mode 64kbps                  */
		*p++ = 0xa3;	/* A-Law Audio                          */
	} else { /* default Datatransmission 64k */
		*p++ = 0x2;	/* Length                               */
		*p++ = 0x88;	/* Coding Std. CCITT, unrestr. dig. Inf */
		*p++ = 0x90;	/* Circuit-Mode 64kbps                  */
	}
	*p++ = IE_CALLED_PN;
	np = PNr;
	*p++ = strlen(np) + 1;
	/* Classify as AnyPref. */
	*p++ = 0x81;		/* Ext = '1'B, Type = '000'B, Plan = '0001'B. */
	while (*np)
		*p++ = *np++ & 0x7f;
	len = p - msg;
	hh->prim = DL_DATA_REQ;
	hh->id = MISDN_ID_ANY;
	ret = sendto(di->layer2, buf, len + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
	if (ret < 0) {
		fprintf(stdout, "sendto error  %s\n", strerror(errno));
	}
	return ret;
}

static int send_msg_with_cause(devinfo_t *di, unsigned char mt) {
	char			*msg, buf[64];
	char			*p;
	struct  mISDNhead 	*hh = (struct  mISDNhead *)buf;
	int			ret, len;

	p = msg = buf + MISDN_HEADER_LEN;
	MsgHead(p, di->cr, mt);
	*p++ = IE_CAUSE;
	*p++ = 0x2;	/* Length */
	*p++ = 0x82;	/* Coding Std. CCITT, public network*/
	*p++ = 0x80 | (di->cause & 0x7f);

	len = p - msg;
	hh->prim = DL_DATA_REQ;
	hh->id = MISDN_ID_ANY;
	ret = sendto(di->layer2, buf, len + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
	if (ret < 0)
		fprintf(stdout, "sendto error  %s\n", strerror(errno));
	else if (VerifyOn > 2)
		fprintf(stdout,"send %s with %d bytes cause #%d\n", mi_msg_type2str(mt), ret, di->cause);
	return ret;
}

static int send_answer(devinfo_t *di, unsigned char mt, unsigned char channel_id) {
	char			*msg, buf[64];
	char			*p;
	struct  mISDNhead 	*hh = (struct  mISDNhead *)buf;
	int			ret, len;

	p = msg = buf + MISDN_HEADER_LEN;
	MsgHead(p, di->cr, mt);
	*p++ = IE_CHANNEL_ID;
	*p++ = 0x1;	/* Length */
	*p++ = channel_id;

	len = p - msg;
	hh->prim = DL_DATA_REQ;
	hh->id = MISDN_ID_ANY;
	ret = sendto(di->layer2, buf, len + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
	if (ret < 0)
		fprintf(stdout, "sendto error  %s\n", strerror(errno));
	else if (VerifyOn > 2)
		fprintf(stdout,"send %s with %d bytes cause #%d\n", mi_msg_type2str(mt), ret, di->cause);
	return ret;
}

static int send_status(devinfo_t *di, unsigned char state) {
	char			*msg, buf[64];
	char			*p;
	struct  mISDNhead 	*hh = (struct  mISDNhead *)buf;
	int			ret, len;

	p = msg = buf + MISDN_HEADER_LEN;
	MsgHead(p, di->cr, MT_STATUS);
	*p++ = IE_CAUSE;
	*p++ = 0x2;	/* Length */
	*p++ = 0x82;	/* Coding Std. CCITT, public network*/
	*p++ = 0x80 | (di->cause & 0x7f);

	*p++ = IE_CALL_STATE;
	*p++ = 1;
	*p++ = state;

	len = p - msg;
	hh->prim = DL_DATA_REQ;
	hh->id = MISDN_ID_ANY;
	ret = sendto(di->layer2, buf, len + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
	if (ret < 0)
		fprintf(stdout, "sendto error  %s\n", strerror(errno));
	else if (VerifyOn > 2)
		fprintf(stdout,"send %s with %d bytes cause #%d state %d\n", mi_msg_type2str(MT_STATUS), ret, di->cause, state);
	return ret;
}

static int activate_bchan(devinfo_t *di) {
	unsigned char 		buf[2048];
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;
	struct timeval		tout;
	fd_set			rds;
	int ret, rval;

	if (di->bproto == ISDN_P_B_X75SLP)
		hh->prim = DL_ESTABLISH_REQ;
	else
		hh->prim = PH_ACTIVATE_REQ;
	hh->id   = MISDN_ID_ANY;
	ret = sendto(di->bchan, buf, MISDN_HEADER_LEN, 0, NULL, 0);
	
	if (ret < 0) {
		fprintf(stdout, "could not send ACTIVATE_REQ %s\n", strerror(errno));
		return 0;
	}
	
	if (VerifyOn>3)
		fprintf(stdout,"ACTIVATE_REQ sendto ret=%d\n", ret);

	tout.tv_usec = 0;
	tout.tv_sec = 10;
	FD_ZERO(&rds);
	FD_SET(di->bchan, &rds);

	ret = select(di->nds, &rds, NULL, NULL, &tout);
	if (VerifyOn>3)
		fprintf(stdout,"select ret=%d\n", ret);
	if (ret < 0) {
		fprintf(stdout, "select error  %s\n", strerror(errno));
		return 0;
	}
	if (ret == 0) {
		fprintf(stdout, "select timeeout\n");
		return 0;
	}
	
	if (di->bproto == ISDN_P_B_X75SLP)
		rval = DL_ESTABLISH_CNF;
	else
		rval = PH_ACTIVATE_IND;
	if (FD_ISSET(di->bchan, &rds)) {
		ret = recv(di->bchan, buf, 2048, 0);
		if (ret < 0) {
			fprintf(stdout, "recv error  %s\n", strerror(errno));
			return 0;
		}
		if (hh->prim == rval) {
			di->flag |= FLG_BCHANNEL_ACTIVE;
		} else {
			fprintf(stdout, "recv not  %x but %x\n", rval, hh->prim);
			return 0;
		}
	} else {
		fprintf(stdout, "bchan fd not in set\n");
		return 0;
	}
	return ret;
}

static int deactivate_bchan(devinfo_t *di) {
	unsigned char 		buf[2048];
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;
	struct timeval		tout;
	fd_set			rds;
	int ret, rval;

	if (di->bchan < 1)
		return 0;
	if (di->bproto == ISDN_P_B_X75SLP)
		hh->prim = DL_RELEASE_REQ;
	else
		hh->prim = PH_DEACTIVATE_REQ;
	hh->id   = MISDN_ID_ANY;
	ret = sendto(di->bchan, buf, MISDN_HEADER_LEN, 0, NULL, 0);
	
	if (ret < 0) {
		fprintf(stdout, "could not send DEACTIVATE_REQ %s\n", strerror(errno));
		return 9;
	}
	
	if (VerifyOn>3)
		fprintf(stdout,"DEACTIVATE_REQ sendto ret=%d\n", ret);

	tout.tv_usec = 0;
	tout.tv_sec = 10;
	FD_ZERO(&rds);
	FD_SET(di->bchan, &rds);

	ret = select(di->nds, &rds, NULL, NULL, &tout);
	if (VerifyOn>3)
		fprintf(stdout,"select ret=%d\n", ret);
	if (ret < 0) {
		fprintf(stdout, "select error  %s\n", strerror(errno));
		return 0;
	}
	if (ret == 0) {
		fprintf(stdout, "select timeeout\n");
		return 0;
	}
	
	if (di->bproto == ISDN_P_B_X75SLP)
		rval = DL_RELEASE_CNF;
	else
		rval = PH_DEACTIVATE_IND;
	if (FD_ISSET(di->bchan, &rds)) {
		ret = recv(di->bchan, buf, 2048, 0);
		if (ret < 0) {
			fprintf(stdout, "recv error  %s\n", strerror(errno));
			return 0;
		}
		if (hh->prim != rval) {
			fprintf(stdout, "recv not %x but %x\n", rval, hh->prim);
			return 0;
		}
	} else {
		fprintf(stdout, "bchan fd not in set\n");
		return 0;
	}
	close(di->bchan);
	di->bchan = 0;
	di->flag &= ~FLG_BCHANNEL_ACTIVE;
	di->flag &= ~FLG_BCHANNEL_SETUP;
	return ret;
}

#ifdef NOTYET
static int send_touchtone(devinfo_t *di, int tone) {
	struct  mISDNhead frm;
	int tval, ret;

	if (VerifyOn>1)
		fprintf(stdout,"send_touchtone %c\n", DTMF_TONE_MASK & tone);
	tval = DTMF_TONE_VAL | tone;
	ret = mISDN_write_frame(di->device, &frm,
		di->b_adress[di->used_bchannel] | FLG_MSG_DOWN,
		PH_CONTROL | REQUEST, 0, 4, &tval, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"tt send ret=%d\n", ret);
	return ret;
}
#endif

static void
do_hw_loop(devinfo_t *di)
{
	struct mISDN_ctrl_req	creq;
	int ret;

	memset(&creq, 0, sizeof(creq));
	creq.op = MISDN_CTRL_LOOP;
	creq.channel = di->used_bchannel;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0)
		fprintf(stdout,"do_hw_loop ioctl error %s\n", strerror(errno));
	else
		di->flag  |= FLG_BCHANNEL_LOOPSET;
}

static void
del_hw_loop(devinfo_t *di)
{
	struct mISDN_ctrl_req	creq;
	int ret;

	if (!(di->flag & FLG_BCHANNEL_LOOPSET))
		return;
	memset(&creq, 0, sizeof(creq));
	creq.op = MISDN_CTRL_LOOP;
	creq.channel = 0;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0)
		fprintf(stdout,"del_hw_loop ioctl error %s\n", strerror(errno));
	di->flag &= ~FLG_BCHANNEL_LOOPSET;
}

static int do_bchannel(devinfo_t *di, int len, unsigned char *buf)
{
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;
	int ret;

	if (len < MISDN_HEADER_LEN) {
		if (VerifyOn)
			fprintf(stdout,"got short B frame %d\n", len);
		return 1;
	}

	if (VerifyOn>7)
		fprintf(stdout,"readloop B prim(%x) id(%x) len(%d)\n",
			hh->prim, hh->id, len);
	if (hh->prim == PH_DATA_IND) {
		/* received data, save it */
		ret = write(di->save, buf + MISDN_HEADER_LEN, len - MISDN_HEADER_LEN);
		if (ret < 0)
			fprintf(stderr,"got error on write %s\n", strerror(errno));
		        
	} else if (hh->prim == PH_DATA_CNF) {
		/* get ACK of send data, so we can
		 * send more
		 */
		if (VerifyOn>5)
			fprintf(stdout,"PH_DATA_CNF\n");
		switch (di->func) {
			case 0:
			case 2:
				if (di->play > -1)
					play_msg(di);
				break;
		}
	} else if (hh->prim == DL_DATA_IND) {
		/* received data, save it */
		ret = write(di->save, buf + MISDN_HEADER_LEN, len - MISDN_HEADER_LEN);
		if (ret < 0)
			fprintf(stderr,"got error on write %s\n", strerror(errno));
	} else if (hh->prim == (PH_CONTROL_IND)) {
		if ((len == MISDN_HEADER_LEN) && ((hh->id & ~DTMF_TONE_MASK) == DTMF_TONE_VAL)) {
			fprintf(stdout,"GOT TT %c\n", DTMF_TONE_MASK & hh->id);
		} else
			fprintf(stdout,"unknown PH_CONTROL len %d/val %x\n", len, hh->id);
	} else {
		if (VerifyOn)
			fprintf(stdout,"got unexpected B frame prim %s (%x) id(%x) len(%d)\n",
				mi_msg_type2str(hh->prim), hh->prim, hh->id, len);
	}
	return 0;
}

#define L3_MT_OFF	(MISDN_HEADER_LEN + 3)
#define L3_CR_VAL	(MISDN_HEADER_LEN + 2)

static int do_dchannel(devinfo_t *di, int len, unsigned char *buf)
{
	struct mISDNhead	*hh = (struct  mISDNhead *)buf;
	unsigned char		*p, *msg;
	int			ret, l, result = 0;

	if (len < MISDN_HEADER_LEN) {
		if (VerifyOn)
			fprintf(stdout,"got short D frame %d\n", len);
		return 0;
	}
	if (VerifyOn>4)
		fprintf(stdout,"readloop L2 prim(%x) id(%x) len(%d)\n",
			hh->prim, hh->id, len);
	if (hh->prim != DL_DATA_IND && hh->prim != DL_UNITDATA_IND) {
		if (VerifyOn)
			fprintf(stdout,"got unexpected D frame prim %s (%x) id(%x) len(%d)\n",
				mi_msg_type2str(hh->prim), hh->prim, hh->id, len);
		return 0;
	}
	if (len > (L3_MT_OFF +1) && (((!(di->flag & FLG_CALL_ORGINATE)) && (buf[L3_MT_OFF] == MT_SETUP)) ||
	    ((di->flag & FLG_CALL_ORGINATE) && (buf[L3_MT_OFF] == MT_CALL_PROCEEDING)) ||
	    ((di->flag & FLG_CALL_ORGINATE) && (buf[L3_MT_OFF] == MT_ALERTING)))) {
		int	idx = L3_MT_OFF + 1;

		di->flag |= FLG_CALL_ACTIVE;
		if (!(di->flag & FLG_CALL_ORGINATE))
			di->cr = buf[L3_CR_VAL];
		while (idx<len) {
			if (buf[idx] == IE_CHANNEL_ID) {
				di->used_bchannel=buf[idx+2] & 0x3;
				break;
			} else if (!(buf[idx] & 0x80)) {
				/* variable len IE */
				idx++;
				idx += buf[idx];
			}
			idx++;
		}
		switch (di->func) {
		case 16:
		case 17:
			break;
		default:
			if (di->used_bchannel < 1 || di->used_bchannel > 2) {
				fprintf(stdout,"got no valid bchannel nr %d\n", di->used_bchannel);
				return 1;
			}
		}
		switch (di->func) {
			case 5:
				di->flag |= FLG_BCHANNEL_EARLY;
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				ret = setup_bchannel(di);
				if (!ret)
					di->flag |= FLG_BCHANNEL_SETUP;
				else {
					fprintf(stdout,"error %d on setup_bchannel\n", ret);
					return 2;
				}
				if (di->flag & FLG_BCHANNEL_EARLY) {
					ret = activate_bchan(di);
					if (!ret) {
						fprintf(stdout,"error on activate_bchan\n");
						return 3;
					}
				}
				break;
			case 6:
			case 7:
				di->flag |= FLG_BCHANNEL_SETUP;
				break;
			case 16:
			case 17:
				send_answer(di, MT_SETUP_ACKNOWLEDGE, 0x81); /* B1 channel */
				send_msg_with_cause(di, MT_DISCONNECT);
				return 0;
		}
		if (!(di->flag & FLG_CALL_ORGINATE)) {
			p = msg = buf + MISDN_HEADER_LEN;
			MsgHead(p, di->cr, MT_CONNECT);
			l = p - msg;
			hh->prim = DL_DATA_REQ;
			hh->id = MISDN_ID_ANY;
			ret = sendto(di->layer2, buf, l + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
			if (ret < 0) {
				fprintf(stdout, "sendto error  %s\n", strerror(errno));
				result = 4;
			}
		}
	} else if ((len > L3_MT_OFF) && (buf[L3_MT_OFF] == MT_CONNECT) && (di->flag & FLG_CALL_ORGINATE)) {
		/* We got connect, so bring B-channel up */
		if (!(di->flag & FLG_BCHANNEL_SETUP)) {
			fprintf(stdout,"CONNECT but no bchannel selected\n");
			return 5;
		}
		if (!(di->flag & FLG_BCHANNEL_EARLY) && !(di->flag & FLG_BCHANNEL_LOOP)) {
			if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
				activate_bchan(di);
			else
				di->flag |= FLG_BCHANNEL_DOACTIVE;
		}
		/* send a CONNECT_ACKNOWLEDGE */
		p = msg = buf + MISDN_HEADER_LEN;
		MsgHead(p, di->cr, MT_CONNECT_ACKNOWLEDGE);
		l = p - msg;
		hh->prim = DL_DATA_REQ;
		hh->id = MISDN_ID_ANY;
			ret = sendto(di->layer2, buf, l + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
		if (ret < 0) {
			fprintf(stdout, "sendto error  %s\n", strerror(errno));
		}
		switch (di->func) {
		case 0:
		case 2:
		case 5:
			if (di->play > -1)
				play_msg(di);
			break;
		case 1:
			/* send next after 2 sec */
			di->timeout = 2;
			di->flag |= FLG_SEND_TONE;
			break;
		case 3:
		case 4:
			/* setup B after 1 sec */
			di->timeout = 1;
			break;
		case 6:
		case 7:
			do_hw_loop(di);
			break;
		}
	} else if ((len > L3_MT_OFF) && (buf[L3_MT_OFF] == MT_CONNECT_ACKNOWLEDGE) && (!(di->flag & FLG_CALL_ORGINATE))) {
		/* We got connect ack, so bring B-channel up */
		if (!(di->flag & FLG_BCHANNEL_SETUP)) {
			fprintf(stdout,"CONNECT but no bchannel selected\n");
			return 6;
		}
		if (!(di->flag & FLG_BCHANNEL_EARLY) && !(di->flag & FLG_BCHANNEL_LOOP)) {
			if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
				activate_bchan(di);
			else
				di->flag |= FLG_BCHANNEL_DOACTIVE;
		}
		/* if here is outgoing data, send first part */
		switch (di->func) {
		case 0:
		case 2:
		case 5:
			if (di->play > -1)
				play_msg(di);
			break;
		case 1:
			/* send next after 2 sec */
			di->timeout = 2;
			di->flag |= FLG_SEND_TONE;
			break;
		case 3:
		case 4:
			/* setup B after 1 sec */
			di->timeout = 1;
			break;
		case 6:
		case 7:
			do_hw_loop(di);
			break;
		}
	} else if ((len > L3_MT_OFF) && (buf[L3_MT_OFF] == MT_DISCONNECT)) {
		/* send a RELEASE */
		p = msg = buf + MISDN_HEADER_LEN;
		MsgHead(p, di->cr, MT_RELEASE);
		l = p - msg;
		hh->prim = DL_DATA_REQ;
		hh->id = MISDN_ID_ANY;
		ret = sendto(di->layer2, buf, l + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
		if (ret < 0) {
			fprintf(stdout, "sendto error  %s\n", strerror(errno));
		}
	} else if ((len > L3_MT_OFF) && (buf[L3_MT_OFF] == MT_RELEASE)) {
		/* on a disconnecting msg leave loop */
		/* send a RELEASE_COMPLETE */
		p = msg = buf + MISDN_HEADER_LEN;
		MsgHead(p, di->cr, MT_RELEASE_COMPLETE);
		l = p - msg;
		hh->prim = DL_DATA_REQ;
		hh->id = MISDN_ID_ANY;
		ret = sendto(di->layer2, buf, l + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
		if (ret < 0) {
			fprintf(stdout, "sendto error  %s\n", strerror(errno));
		}
		switch (di->func) {
		case 16:
		case 17:
			result = 0;
			break;
		default:
			result = 7;
			break;
		}
	} else if ((len > L3_MT_OFF) && (buf[L3_MT_OFF] == MT_RELEASE_COMPLETE)) {
		result = 8;
	} else if ((len > L3_MT_OFF) && (buf[L3_MT_OFF] == MT_RESTART)) {
		l = di->cr;
		di->cr = buf[L3_CR_VAL];
		switch (di->func) {
		case 17:
			di->cause = CAUSE_NOTCOMPAT_STATE_OR_MT_NOTIMPLEMENTED;
			send_status(di, 0);
			break;
		default:
			di->cr = buf[L3_CR_VAL];
			
			p = msg = buf + MISDN_HEADER_LEN;
			MsgHead(p, di->cr, MT_RESTART_ACKNOWLEDGE);
			/* we use the same content as recaived for the answer */
			hh->prim = DL_DATA_REQ;
			hh->id = MISDN_ID_ANY;
			ret = sendto(di->layer2, buf, len, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
			if (ret < 0)
				fprintf(stdout, "sendto error  %s\n", strerror(errno));
			break;
		}
		di->cr = l;
		result = 9;
	} else {
		if (VerifyOn) {
			fprintf(stdout,"got unexpected D frame prim %s (%x) id(%x) len(%d)\n",
				mi_msg_type2str(hh->prim), hh->prim, hh->id, len);
			if (len > MISDN_HEADER_LEN) {
				int	i;
				for (i = MISDN_HEADER_LEN; i < len; i++)
					fprintf(stdout," %02x", buf[i]);
				fprintf(stdout,"\n");
			}
		}
	}
	return result;
}

static int do_connection(devinfo_t *di) {
	unsigned char		buf[MAX_REC_BUF];
	struct  mISDNhead	*hh;
	struct timeval		tout;
	struct sockaddr_mISDN	l2addr;
	socklen_t		alen;
	fd_set			rds;
	int			ret = 0;

	if (di->setloopback)
		return 0;
	if (di->func > 12 && di->func < 16)
		return 0;
	hh = (struct  mISDNhead *)buf;

	if (strlen(di->phonenr)) {
		di->flag |= FLG_CALL_ORGINATE;
		di->cr = 0x81;
		send_SETUP(di, di->si, di->phonenr);
	}
	di->timeout = 30;

	/* Main loop */
	while (1) {
		tout.tv_usec = 0;
		tout.tv_sec = di->timeout;
		FD_ZERO(&rds);
		FD_SET(di->layer2, &rds);
		if (di->bchan)
			FD_SET(di->bchan, &rds);
		ret = select(di->nds, &rds, NULL, NULL, &tout);
		if (VerifyOn>7)
			fprintf(stdout,"selectloop ret=%d\n", ret);
		if (ret < 0) {
			fprintf(stdout,"select error %s\n", strerror(errno));
			continue;
		}
		if (ret == 0) { /* time out */
#ifdef NOTYET
			if (di->flag & FLG_SEND_TONE) {
				if (di->val) {
					di->val--;
					send_touchtone(di, tt_char[di->val]);
				} else {
					/* After last tone disconnect */
					di->cause = CAUSE_NORMAL_CLEARING;
					send_msg_with_cause(di, MT_DISCONNECT);
					di->flag &= ~FLG_SEND_TONE;
				}
				continue;
			} else 
#endif	
			if (di->flag & FLG_SEND_DATA) {
				if (di->play > -1)
					send_data(di);
				else
					di->flag &= ~FLG_SEND_DATA;
				continue;;
			} else if (di->flag & FLG_BCHANNEL_DOACTIVE) {
				ret = activate_bchan(di);
				if (!ret) {
					fprintf(stdout,"error on activate_bchan\n");
					return 0;
				}
				di->flag &= ~FLG_BCHANNEL_DOACTIVE;
				/* send next after 1 sec */
				di->timeout = 1;
				di->flag |= FLG_SEND_DATA;
				continue;
			}
			if (di->flag & FLG_BCHANNEL_LOOPSET) {
				if (di->func == 7) { /* we never end on timeout */
					if (VerifyOn>3)
						fprintf(stdout,"timed out but continue\n");
					continue;
				}
			}
			if (di->func == 16 || di->func == 17) {
				break; /* we are cleared */
			} else {
				/* hangup */
				fprintf(stdout,"timed out sending hangup\n");
				di->cause = CAUSE_NORMAL_CLEARING;
				if (di->flag & FLG_CALL_ACTIVE)
					send_msg_with_cause(di, MT_DISCONNECT);
				else
					send_msg_with_cause(di, MT_RELEASE_COMPLETE);
				if (di->flag & FLG_CALL_ACTIVE)
					di->flag &= ~FLG_CALL_ACTIVE;
				else
					break;
			}
		}
		if (FD_ISSET(di->bchan, &rds)) {
			/* B-Channel related messages */
			ret = recv(di->bchan, buf, MAX_REC_BUF, 0);
			if (VerifyOn>6)
				fprintf(stdout,"recvloop ret=%d\n", ret);
			if (ret < 0) {
				fprintf(stdout, "recv error  %s\n", strerror(errno));
				continue;
			}
			do_bchannel(di, ret, buf);		
		}
		if (FD_ISSET(di->layer2, &rds)) {
			alen = sizeof(l2addr);
			ret = recvfrom(di->layer2, buf, 300, 0, (struct sockaddr *) &l2addr, &alen);
			if (VerifyOn>3)
				fprintf(stdout," recvfrom loop ret=%d\n", ret);
			if (ret < 0) {
				fprintf(stdout, "recvfrom error  %s\n", strerror(errno));
				continue;
			}
			ret = do_dchannel(di, ret, buf);
			if (ret)
				break;
		}
	}
	if (di->bchan) {
		if (di->flag & FLG_BCHANNEL_LOOP)
			del_hw_loop(di);
		else
			deactivate_bchan(di);
	}
	sleep(1);
	hh->prim = DL_RELEASE_REQ;
	hh->id   = MISDN_ID_ANY;
	ret = sendto(di->layer2, buf, MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
	
	if (ret < 0) {
		fprintf(stdout, "could not send DL_RELEASE_REQ %s\n", strerror(errno));
	} else {
		if (VerifyOn>3)
			fprintf(stdout,"dl_release sendto ret=%d\n", ret);

		tout.tv_usec = 0;
		tout.tv_sec = 10;
		FD_ZERO(&rds);
		FD_SET(di->layer2, &rds);

		ret = select(di->nds, &rds, NULL, NULL, &tout);
		if (VerifyOn>3)
			fprintf(stdout,"select ret=%d\n", ret);
		if (ret < 0) {
			fprintf(stdout, "select error  %s\n", strerror(errno));
		} else if (ret == 0) {
			fprintf(stdout, "select timeeout\n");
		} else if (FD_ISSET(di->layer2, &rds)) {
			alen = sizeof(l2addr);
			ret = recvfrom(di->layer2, buf, 300, 0, (struct sockaddr *) &l2addr, &alen);
			if (ret < 0) {
				fprintf(stdout, "recvfrom error  %s\n", strerror(errno));
			}
			if (hh->prim != DL_RELEASE_CNF) {
				fprintf(stdout, "got not  DL_RELEASE_CNF but %x\n", hh->prim);
			}
		} else {
			fprintf(stdout, "layer2 fd not in set\n");
		}
	}
	sleep(1);
	return 0;
}

static int do_setup(devinfo_t *di) {
	int 			cnt, ret = 0;
	int			sk;
	struct mISDN_devinfo	devinfo;
	struct sockaddr_mISDN	l2addr;
	socklen_t		alen;
	struct mISDNhead	*hh;
	struct timeval		tout;
	fd_set			rds;
	unsigned char		buffer[300];
	struct mISDN_ctrl_req	creq;

	di->dproto = ISDN_P_LAPD_TE;
	di->l2addr.family = AF_ISDN;
	di->l2addr.dev = di->cardnr;
	di->l2addr.channel = 0;
	di->l2addr.sapi = 0;
	di->l2addr.tei = 127;

	switch (di->func) {
		case 0:
		case 5:
			di->bproto = ISDN_P_B_RAW;
			di->si = 1;
			break;
#ifdef NOTYET
		case 1:
			di->bproto = ISDN_P_B_RAW;
			di->si = 1;
			di->val= 8; /* send  8 touch tones (7 ... 0) */
			break;
#endif
		case 2:
			di->bproto = ISDN_P_B_L2DTMF;
			di->si = 1;
			break;
		case 3:
			di->bproto = ISDN_P_B_HDLC;
			di->si = 7;
			break;
		case 4:
			di->bproto = ISDN_P_B_X75SLP;
			di->si = 7;
			di->flag |= FLG_BCHANNEL_ACTDELAYED;
			break;
		case 6:
		case 7:
			di->bproto = ISDN_P_B_RAW;
			di->si = 1;
			di->flag |= FLG_BCHANNEL_LOOP;
			break;
		case 8:
		case 9:
		case 10:
		case 11:
			di->setloopback = di->func - 7;
			break;
		case 12:
		case 13:
		case 14:
		case 15:
			di->dproto = ISDN_P_LAPD_NT;
			break;
		case 17:
		case 16:
			di->dproto = ISDN_P_LAPD_NT;
			di->cause = atol(di->phonenr);
			di->l2addr.tei = 0; /* P2P */
			di->phonenr[0] = 0;
			break;
		default:
			fprintf(stdout,"unknown program function %d\n",
				di->func);
			return 1;
	}
	sk = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sk < 1) {
		fprintf(stdout, "could not open socket %s\n", strerror(errno));
		return 2;
	}
	ret = ioctl(sk, IMGETCOUNT, &cnt);
	if (ret) {
		fprintf(stdout, "error getting interface count: %s\n", strerror(errno));
		close(sk);
		return 3;
	}

	if (VerifyOn>1)
		fprintf(stdout,"%d device%s found\n", cnt, (cnt==1)?"":"s");

	devinfo.id = di->cardnr;
	ret = ioctl(sk, IMGETDEVINFO, &devinfo);
	if (ret < 0) {
		fprintf(stdout, "error getting info for device %d: %s\n", di->cardnr, strerror(errno));
	} else if (VerifyOn>1) {  
		fprintf(stdout, "        id:             %d\n", devinfo.id);
		fprintf(stdout, "        Dprotocols:     %08x\n", devinfo.Dprotocols);
		fprintf(stdout, "        Bprotocols:     %08x\n", devinfo.Bprotocols);
		fprintf(stdout, "        protocol:       %d\n", devinfo.protocol);
		fprintf(stdout, "        nrbchan:        %d\n", devinfo.nrbchan);
		fprintf(stdout, "        name:           %s\n", devinfo.name);
	}

	close(sk);

	di->layer2 = socket(PF_ISDN, SOCK_DGRAM, di->dproto);
	if (di->layer2 < 0) {
		fprintf(stdout, "could not open layer2 socket %s\n", strerror(errno));
		return 5;
	}

	di->nds = di->layer2 + 1;

	ret = fcntl(di->layer2, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stdout, "fcntl error %s\n", strerror(errno));
		return 6;
	}

	ret = bind(di->layer2, (struct sockaddr *) &di->l2addr, sizeof(di->l2addr));

	if (ret < 0) {
		fprintf(stdout, "could not bind l2 socket %s\n", strerror(errno));
		return 7;
	}

	memset(&creq, 0, sizeof(creq));
	creq.op = MISDN_CTRL_GETOP;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0) {
		fprintf(stdout,"IMCTRLREQ ioctl error %s\n", strerror(errno));
		return 8;
	}
	if (VerifyOn > 1)
		fprintf(stdout, "supported IMCTRLREQ operations: %x\n", creq.op);

	if (di->func == 6 || di->func == 7 || (di->func <= 11 && di->func >= 8)) {
		if (!(creq.op & MISDN_CTRL_LOOP)) {
			fprintf(stdout," hw loop not supported\n");
			return 8;
		}
	}

	if (di->func == 13 || di->func == 14 || di->func == 15) {
		if (!(creq.op & MISDN_CTRL_L1_TESTS)) {
			fprintf(stdout,"MISDN_CTRL_L1_TESTS not supported\n");
			return 8;
		}
	}

	if (di->func == 12) {
		if (!(creq.op & MISDN_CTRL_L1_TIMER3)) {
			fprintf(stdout,"Setting Timer 3 value - not supported\n");
			return 8;
		}
		if (!di->phonenr[0]) {
			fprintf(stdout,"Setting Timer 3 value - need to give value with -n\n");
			return 8;
		}
	}

	if (di->setloopback) {
		creq.op = MISDN_CTRL_LOOP;
		creq.channel = di->setloopback;
		ret = ioctl(di->layer2, IMCTRLREQ, &creq);
		if (ret < 0)
			fprintf(stdout,"hw_loop ioctl error %s\n", strerror(errno));
		else
			fprintf(stdout,"dhw_loop ioctl (%d) successful\n", di->setloopback);
		close(di->layer2);
		return ret;
	}
	if (di->func == 12) {
		creq.op = MISDN_CTRL_L1_TIMER3;
		creq.channel = 0;
		creq.p1 = atol(di->phonenr);
		ret = ioctl(di->layer2, IMCTRLREQ, &creq);
		if (ret < 0)
			fprintf(stdout,"Timer3 ioctl error %s\n", strerror(errno));
		else
			fprintf(stdout,"Timer3 ioctl (%d) successful\n", di->setloopback);
		close(di->layer2);
		return ret;
	}
	if (di->func == 13) {

		creq.op = MISDN_CTRL_L1_AIS_TEST;
		creq.channel = 0;
		creq.p1 = 1;
		ret = ioctl(di->layer2, IMCTRLREQ, &creq);
		if (ret < 0)
			fprintf(stdout,"AIS ioctl error %s\n", strerror(errno));
		else
			fprintf(stdout,"AIS ioctl enable successful\n");
		close(di->layer2);
		return ret;
	}
	if (di->func == 14) {
		creq.op = MISDN_CTRL_L1_AIS_TEST;
		creq.channel = 0;
		creq.p1 = 0;
		ret = ioctl(di->layer2, IMCTRLREQ, &creq);
		if (ret < 0)
			fprintf(stdout,"AIS ioctl error %s\n", strerror(errno));
		else
			fprintf(stdout,"AIS ioctl disable successful\n");
		close(di->layer2);
		return ret;
	}
	if (di->func == 15) {
		creq.op = MISDN_CTRL_L1_STATE_TEST;
		creq.channel = 0;
		creq.p1 = atol(di->phonenr);
		ret = ioctl(di->layer2, IMCTRLREQ, &creq);
		if (ret < 0)
			fprintf(stdout,"L1 state set ioctl error %s\n", strerror(errno));
		else
			fprintf(stdout,"L1 set state(%ld) ioctl successful\n", atol(di->phonenr));
		close(di->layer2);
		return ret;
	}

	hh = (struct mISDNhead *)buffer;

	while (1) {
		tout.tv_usec = 0;
		tout.tv_sec = 10;
		FD_ZERO(&rds);
		FD_SET(di->layer2, &rds);

		ret = select(di->nds, &rds, NULL, NULL, &tout);
		if (VerifyOn>3)
			fprintf(stdout,"select ret=%d\n", ret);
		if (ret < 0) {
			fprintf(stdout, "select error  %s\n", strerror(errno));
			return 9;
		}
		if (ret == 0) {
			fprintf(stdout, "select timeeout\n");
			return 10;
		}
	
		if (FD_ISSET(di->layer2, &rds)) {
			alen = sizeof(l2addr);
			ret = recvfrom(di->layer2, buffer, 300, 0, (struct sockaddr *) &l2addr, &alen);
			if (ret < 0) {
				fprintf(stdout, "recvfrom error  %s\n", strerror(errno));
				return 11;
			}
			if (VerifyOn>3) {
				fprintf(stdout, "alen =%d, dev(%d) channel(%d) sapi(%d) tei(%d)\n", alen, l2addr.dev, l2addr.channel, l2addr.sapi, l2addr.tei);
			}
			if (hh->prim == DL_INFORMATION_IND) {
				fprintf(stdout, "got DL_INFORMATION_IND\n");
				if (alen == sizeof(l2addr)) {
					if (VerifyOn)
						fprintf(stdout, "use channel(%d) sapi(%d) tei(%d) for now\n", l2addr.channel, l2addr.sapi, l2addr.tei);
					di->l2addr = l2addr;
				}
				if (l2addr.tei != 127)
					continue;
				
				hh->prim = DL_ESTABLISH_REQ;
				hh->id   = MISDN_ID_ANY;
				ret = sendto(di->layer2, buffer, MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
				
				if (ret < 0) {
					fprintf(stdout, "could not send DL_ESTABLISH_REQ %s\n", strerror(errno));
					return 12;
				}
				
				if (VerifyOn>3)
					fprintf(stdout,"dl_etablish send ret=%d\n", ret);

			} else if (hh->prim == MPH_ACTIVATE_IND) {
				fprintf(stdout, "got MPH_ACTIVATE_IND\n");
				if (alen == sizeof(l2addr)) {
					if (VerifyOn)
						fprintf(stdout, "use channel(%d) sapi(%d) tei(%d) for now\n", l2addr.channel, l2addr.sapi, l2addr.tei);
					di->l2addr = l2addr;
				}
				if (di->func == 16 || di->func == 17) {
					fprintf(stdout, "NT Mode SAPI:%d TEI:%d activated\n", l2addr.sapi, l2addr.tei);
				} else {
					if (l2addr.tei != 127)
						continue;
				}
				hh->prim = DL_ESTABLISH_REQ;
				hh->id   = MISDN_ID_ANY;
				ret = sendto(di->layer2, buffer, MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
				
				if (ret < 0) {
					fprintf(stdout, "could not send DL_ESTABLISH_REQ %s\n", strerror(errno));
					return 12;
				}
				
				if (VerifyOn>3)
					fprintf(stdout,"dl_etablish send ret=%d\n", ret);
			} else if (hh->prim == DL_ESTABLISH_CNF) {
				fprintf(stdout, "got DL_ESTABLISH_CNF\n");
				break;
			} else {
				fprintf(stdout, "got unexpected %s (%x) message\n", mi_msg_type2str(hh->prim), hh->prim );
			}
		} else {
			fprintf(stdout, "layer2 fd not in set\n");
			return 13;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char FileName[200],FileNameOut[200], FileNameIn[200];
	int aidx=1,para=1, idx;
	char sw;
	devinfo_t mISDN;
	int err;

	fprintf(stderr,"TestmISDN 1.0\n");
	strcpy(FileName, "test_file");
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 0;
	mISDN.func = 0;
	mISDN.phonenr[0] = 0;
	if (argc<1) {
		fprintf(stderr,"Error: Not enough arguments please check\n");
		usage(argv[0]);
		exit(1);
	} else {
		do {
			if (argv[aidx] && argv[aidx][0]=='-') {
				sw=argv[aidx][1];
				switch (sw) {
					 case 'v':
					 case 'V':
						VerifyOn=1;
						if (argv[aidx][2]) {
							VerifyOn=atol(&argv[aidx][2]);
						}
						break;
					case 'c':
						if (argv[aidx][2]) {
							mISDN.cardnr=atol(&argv[aidx][2]);
						}
						break;
					case 'F':
						if (argv[aidx][2]) {
							mISDN.func=atol(&argv[aidx][2]);
						}
						break;
					case 'n':
						if (!argv[aidx][2]) {
							idx = 0;
							aidx++;
						} else {
							idx=2;
						}
						if (aidx<=argc) {
							strcpy(mISDN.phonenr, &argv[aidx][idx]);
						} else {
							fprintf(stderr," Switch %c without value\n",sw);
							exit(1);
						}
						break;
					case '?' :
						usage(argv[0]);
						exit(1);
						break;
					default  : fprintf(stderr,"Unknown Switch %c\n",sw);
						usage(argv[0]);
						exit(1);
						break;
				}
			}  else {
				if (para==1) {
					if (argc > 1)
						strncpy(FileName, argv[aidx], 199);
					para++;
				} else {
					fprintf(stderr,"Undefined argument %s\n",argv[aidx]);
					usage(argv[0]);
					exit(1);
				}
			}
			aidx++;
		} while (aidx<argc);
	}
	err = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (err < 0) {
		printf("TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return 1;
	}
	close(err);
	sprintf(FileNameOut,"%s.out", FileName);
	sprintf(FileNameIn,"%s.in", FileName);
	if (0>(mISDN.save = open(FileNameIn, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU))) {
		printf("TestmISDN cannot open %s due to %s\n",FileNameIn,
			strerror(errno));
		return 1;
	}
	if (0>(mISDN.play = open(FileNameOut, O_RDONLY))) {
		printf("TestmISDN cannot open %s due to %s\n",FileNameOut,
			strerror(errno));
		mISDN.play = -1;
	} else 
		mISDN.fplay = fdopen(mISDN.play, "r");
	if (VerifyOn>8)
		fprintf(stdout,"fileno %d/%d\n",mISDN.save, mISDN.play);
	
	err = do_setup(&mISDN);
	if (err)
		fprintf(stdout,"do_setup error %d\n", err);
	else
		do_connection(&mISDN);	
	close(mISDN.save);
	if (mISDN.play>=0)
		close(mISDN.play);
	if (mISDN.layer2 > 0)
		close(mISDN.layer2);
	if (mISDN.bchan > 0)
		close(mISDN.bchan);
	return 0;
}
