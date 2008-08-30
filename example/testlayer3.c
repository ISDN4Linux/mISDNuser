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
#include <pthread.h>
#include <sys/ioctl.h>
#include <mISDNif.h>
#include <q931.h>
#include <mlayer3.h>
#include <compat_af_isdn.h>

void usage(pname) 
char *pname;
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
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n"); 
	fprintf(stderr,"  -F<n>           use function n (default 0)\n"); 
	fprintf(stderr,"                    0 send and recive voice\n"); 
	fprintf(stderr,"                    1 send touchtones\n"); 
	fprintf(stderr,"                    2 recive touchtones\n"); 
	fprintf(stderr,"                    3 send and recive hdlc data\n"); 
	fprintf(stderr,"                    4 send and recive X75 data\n"); 
	fprintf(stderr,"                    5 send and recive voice early B connect\n");
	fprintf(stderr,"                    6 loop back voice\n");
	fprintf(stderr,"  -n <phone nr>   Phonenumber to dial\n");
	fprintf(stderr,"  -N              NT Mode\n");
	fprintf(stderr,"  -m <msn>	  MSN (default 123) NT mode only\n");
	fprintf(stderr,"  -vn             Printing debug info level n\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int			cardnr;
	int			func;
	char			phonenr[32];
	char			msn[32];
	char			net_nr[32];
	pthread_mutex_t		wait;
	pthread_t		bworker;
	struct mlayer3		*layer3;
	unsigned int		pid;
	int			bchan;
	int			nds;
	int			bproto;
	int			used_bchannel;
	int			save;
	int			play;
	FILE			*fplay;
	int			si;
	int			flag;
	int			timeout;
} devinfo_t;

#define FLG_SEND_TONE		0x0001
#define FLG_SEND_DATA		0x0002
#define FLG_NT_MODE		0x0004
#define FLG_BCHANNEL_SETUP	0x0010
#define FLG_BCHANNEL_DOACTIVE	0x0020
#define FLG_BCHANNEL_ACTIVE	0x0040
#define FLG_BCHANNEL_ACTDELAYED	0x0080
#define FLG_CALL_ORGINATE	0x0100
#define FLG_BCHANNEL_EARLY	0x0200
#define FLG_CALL_ACTIVE		0x0400
#define FLG_BCHANNEL_LOOP	0x0800
#define FLG_BCHANNEL_LOOPSET	0x1000
#define FLG_STOP_BCHANNEL	0x4000
#define FLG_STOP		0x8000

#define MAX_REC_BUF		4000
#define MAX_DATA_BUF		1024

static int VerifyOn=0;
#ifdef NOTYET
static char tt_char[] = "0123456789ABCD*#";
#endif

#define PLAY_SIZE 64

int play_msg(devinfo_t *di) {
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
	else if (VerifyOn>4)
		fprintf(stdout,"play send ret=%d\n", ret);
	return ret;
}

int send_data(devinfo_t *di) {
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
	else if (VerifyOn>4)
		fprintf(stdout,"send_data ret=%d\n", ret);
	return ret;
}

void do_bconnection(devinfo_t *);

int setup_bchannel(devinfo_t *di) {
	int			ret;
	struct sockaddr_mISDN	addr;

	if (di->flag & FLG_BCHANNEL_SETUP)
		return 1;
	if ((di->used_bchannel < 1) || (di->used_bchannel > 2)) {
		fprintf(stdout, "wrong channel %d\n", di->used_bchannel);
		return 2;
	}
	di->bchan = socket(PF_ISDN, SOCK_DGRAM, di->bproto);
	if (di->bchan < 0) {
		fprintf(stdout, "could not open bchannel socket %s\n", strerror(errno));
		return 3;
	}

	if (di->bchan > di->nds - 1)
		di->nds = di->bchan + 1;

	ret = fcntl(di->bchan, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stdout, "fcntl error %s\n", strerror(errno));
		return 4;
	}

	addr.family = AF_ISDN;
	addr.dev = di->cardnr - 1;
	addr.channel = di->used_bchannel;

	ret = bind(di->bchan, (struct sockaddr *) &addr, sizeof(addr));

	if (ret < 0) {
		fprintf(stdout, "could not bind bchannel socket %s\n", strerror(errno));
		return 5;
	}
	ret = pthread_create(&di->bworker, NULL, (void *)do_bconnection, (void *)di);
	if (ret) {
		fprintf(stdout, "%s cannot start bworker thread  %s\n", __FUNCTION__, strerror(errno));
	} else
		di->flag |= FLG_BCHANNEL_SETUP;
		
	return ret;
}

int send_SETUP(devinfo_t *di, int SI, char *PNr) {
	unsigned char		nr[64], bearer[4], cid[4];
	unsigned char		*np,*p;
	int			ret, l;
	struct	l3_msg		*l3m;

	ret = di->layer3->to_layer3(di->layer3, MT_ASSIGN, 0, NULL);
	if (ret < 0) {
		fprintf(stdout, "cannot get a new pid\n");
		return ret;
	}
		
	l3m =  alloc_l3_msg();
	if (!l3m) {
		fprintf(stdout, "cannot allocate l3 msg struct\n");
		return -ENOMEM;
	}
	if (SI == 1) { /* Audio */
		l = 3;			/* Length                               */
		bearer[0] = 0x90;	/* Coding Std. CCITT, 3.1 kHz audio     */
		bearer[1] = 0x90;	/* Circuit-Mode 64kbps                  */
		bearer[2] = 0xa3;	/* A-Law Audio                          */
	} else { /* default Datatransmission 64k */
		l = 2;			/* Length                               */
		bearer[0] = 0x88;	/* Coding Std. CCITT, unrestr. dig. Inf */
		bearer[1] = 0x90;	/* Circuit-Mode 64kbps                  */
	}
	add_layer3_ie(l3m, IE_BEARER, l, bearer);
	if (di->flag & FLG_NT_MODE) {
		di->used_bchannel = 1;
		cid[0] = 0x80; /* channel prefered */
		cid[0] |= di->used_bchannel;
		add_layer3_ie(l3m, IE_CHANNEL_ID, 1, cid);
	}
	np = (unsigned char *)PNr;
	l = strlen(PNr) + 1;
	p = nr;
	/* Classify as AnyPref. */
	*p++ = 0x81;		/* Ext = '1'B, Type = '000'B, Plan = '0001'B. */
	while (*np)
		*p++ = *np++ & 0x7f;
	add_layer3_ie(l3m, IE_CALLED_PN, l, nr);
	ret = di->layer3->to_layer3(di->layer3, MT_SETUP, di->pid, l3m);
	if (ret < 0) {
		fprintf(stdout, "send to_layer3  error  %s\n", strerror(errno));
	}
	return ret;
}

int activate_bchan(devinfo_t *di) {
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

int deactivate_bchan(devinfo_t *di) {
	unsigned char 		buf[64];
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;
	int ret;

	if (!(di->flag &FLG_BCHANNEL_ACTIVE))
		return 0;
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
	di->flag &= ~FLG_BCHANNEL_ACTIVE;
	if (VerifyOn>3)
		fprintf(stdout,"DEACTIVATE_REQ sendto ret=%d\n", ret);

	return ret;
}

#ifdef NOTYET
int send_touchtone(devinfo_t *di, int tone) {
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

#ifdef NOTYET
void
do_hw_loop(devinfo_t *di)
{
	struct mISDN_ctrl_req	creq;
	int ret;

	creq.op = MISDN_CTRL_LOOP;
	creq.channel = di->used_bchannel;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0)
		fprintf(stdout,"do_hw_loop ioctl error %s\n", strerror(errno));
	else
		di->flag  |= FLG_BCHANNEL_LOOPSET;
}

void
del_hw_loop(devinfo_t *di)
{
	struct mISDN_ctrl_req	creq;
	int ret;

	if (!(di->flag & FLG_BCHANNEL_LOOPSET))
		return;
	creq.op = MISDN_CTRL_LOOP;
	creq.channel = 0;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0)
		fprintf(stdout,"del_hw_loop ioctl error %s\n", strerror(errno));
	di->flag &= ~FLG_BCHANNEL_LOOPSET;
}
#endif

int do_bchannel(devinfo_t *di, int len, unsigned char *buf)
{
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;

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
		write(di->save, buf + MISDN_HEADER_LEN, len - MISDN_HEADER_LEN);
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
		write(di->save, buf + MISDN_HEADER_LEN, len - MISDN_HEADER_LEN);
	} else if (hh->prim == (PH_CONTROL_IND)) {
		unsigned int	*tone = (unsigned int *)(buf + MISDN_HEADER_LEN);

		if ((len == (4 + MISDN_HEADER_LEN)) && ((*tone & ~DTMF_TONE_MASK) == DTMF_TONE_VAL)) {
			fprintf(stdout,"GOT TT %c\n", DTMF_TONE_MASK & *tone);
		} else
			fprintf(stdout,"unknown PH_CONTROL len %d/val %x\n", len, *tone);
	} else if ((hh->prim == DL_RELEASE_CNF) || (hh->prim == PH_DEACTIVATE_IND)) {
		if (VerifyOn)
			fprintf(stdout,"readloop B got termination request\n");
		close(di->bchan);
		di->bchan = 0;
		di->flag &= ~FLG_BCHANNEL_ACTIVE;
		di->flag &= ~FLG_BCHANNEL_SETUP;
		return 1;
	} else {
		if (VerifyOn)
			fprintf(stdout,"got unexpected B frame prim(%x) id(%x) len(%d)\n",
				hh->prim, hh->id, len);
	}
	return 0;
}

void do_bconnection(devinfo_t *di) {
	unsigned char		buf[MAX_REC_BUF];
	struct  mISDNhead	*hh;
	struct timeval		tout;
	fd_set			rds;
	int			ret = 0;

	hh = (struct  mISDNhead *)buf;
	/* Main loop */
	while (1) {
		tout.tv_usec = 0;
		tout.tv_sec = di->timeout;
		FD_ZERO(&rds);
		FD_SET(di->bchan, &rds);
		ret = select(di->bchan +1 , &rds, NULL, NULL, &tout);
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
					p = msg = buf + mISDN_HEADER_LEN;
					MsgHead(p, di->cr, MT_DISCONNECT);
					l = p - msg;
					hh->prim = DL_DATA_REQ;
					hh->id = MISDN_ID_ANY;
					ret = sendto(di->layer2, buf, l + MISDN_HEADER_LEN, 0, (struct sockaddr *)&di->l2addr, sizeof(di->l2addr));
					if (ret < 0) {
						fprintf(stdout, "sendto error  %s\n", strerror(errno));
					}
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
					return;
				}
				di->flag &= ~FLG_BCHANNEL_DOACTIVE;
				/* send next after 1 sec */
				di->timeout = 1;
				di->flag |= FLG_SEND_DATA;
				continue;
			}
			/* hangup */
			fprintf(stdout,"timed out sending hangup\n");
			if (di->flag & FLG_CALL_ACTIVE)
				ret = di->layer3->to_layer3(di->layer3, MT_DISCONNECT, di->pid, NULL);
			else
				ret = di->layer3->to_layer3(di->layer3, MT_RELEASE_COMPLETE, di->pid, NULL);
			if (ret < 0) {
				fprintf(stdout, "to_layer3 error  %s\n", strerror(-ret));
			}
			if (di->flag & FLG_CALL_ACTIVE)
				di->flag &= ~FLG_CALL_ACTIVE;
			else
				break;
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
			ret = do_bchannel(di, ret, buf);
			if (ret)
				break;
		}
	}
	if (di->bchan) {
#ifdef NOTYET
		if (di->flag & FLG_BCHANNEL_LOOP)
			del_hw_loop(di);
		else
#endif
			deactivate_bchan(di);
	}
	return;
}

int
number_match(char *cur, char *msn, int complete) {

	if (VerifyOn > 2)
		fprintf(stdout,"numbers: %s/%s\n", cur, msn);
	if (complete) {
		if (!strcmp(cur, msn))
			return 0;
		else
			return 2;
	}
	if (strlen(cur) >= strlen(msn)) {
		if (!strncmp(cur, msn, strlen(msn)))
			return 0;
		else
			return 2;
	}
	while(*cur) {
		if (*cur++ != *msn++)
			return 2;
	}
	return 1;
}

int send_connect(devinfo_t *di) {
	time_t		sec;
	struct tm	*t;
	unsigned char	c[5];
	struct l3_msg	*l3m;
	int		ret;

	sec = time(NULL);
	t = localtime(&sec);
	l3m =  alloc_l3_msg();
	if (!l3m) {
		fprintf(stdout, "cannot allocate l3 msg struct\n");
		return -ENOMEM;
	}
	c[0] = t->tm_year;
	c[1] = t->tm_mon + 1;
	c[2] = t->tm_mday;
	c[3] = t->tm_hour;
	c[4] = t->tm_min;
	add_layer3_ie(l3m, IE_DATE, 5, c);
	ret = di->layer3->to_layer3(di->layer3, MT_CONNECT, di->pid, l3m);
	return ret;
}

int answer_call(devinfo_t *di, struct l3_msg *l3m) {
	unsigned char		c[4];
	unsigned char		*np,*p;
	int			ret, l, mt;
	struct	l3_msg		*nl3m;

	nl3m =  alloc_l3_msg();
	if (!nl3m) {
		fprintf(stdout, "cannot allocate l3 msg struct\n");
		return -ENOMEM;
	}
	if (l3m->called_nr) {
		p = l3m->called_nr;
		l = *p++;
		np = (unsigned char *)di->net_nr;
		if (l > 1) {
			p++;
			l--;
			while(l--)
				*np++ = *p++;
			*np = 0;
		}
		ret = number_match(di->net_nr, di->msn, l3m->sending_complete);
		if (ret == 0) {
			mt = MT_CALL_PROCEEDING;
		} else if (ret == 1) {
			mt = MT_SETUP_ACKNOWLEDGE;
		} else {
			/* Number cannot match */
			mt = MT_RELEASE_COMPLETE;
		}
	} else {
		mt = MT_SETUP_ACKNOWLEDGE;
	}
	if (mt == MT_RELEASE_COMPLETE) {
		c[0] = 0x80 | CAUSE_LOC_PRVN_RMTUSER;
		c[1] = 0x80 | CAUSE_CALL_REJECTED;
		add_layer3_ie(nl3m, IE_CAUSE, 2, c);
	} else {
		c[0] = 0x80; /* channel prefered */
		c[0] |= di->used_bchannel;
		add_layer3_ie(nl3m, IE_CHANNEL_ID, 1, c);
	}
	ret = di->layer3->to_layer3(di->layer3, mt, di->pid, nl3m);
	if (ret < 0) {
		fprintf(stdout, "send to_layer3  error  %s\n", strerror(errno));
		return ret;
	}
	if (mt == MT_CALL_PROCEEDING)
		return 1;
	else
		return 0;
}

int
setup_channel(devinfo_t *di, struct l3_msg *l3m)
{
	int	ret;

	if (di->flag & FLG_CALL_ACTIVE)
		return 0;
	if (di->flag & FLG_NT_MODE) {
		di->used_bchannel = 1;
		ret = answer_call(di, l3m);
		if (ret < 0) {
			fprintf(stdout, "answer_call return %d\n", ret);
			return 1;
		}
		di->flag |= FLG_CALL_ACTIVE;
		if (!ret) {
			return 0;
		}
	} else {
		if (l3m->channel_id) {
			if (l3m->channel_id[0] == 1 && !(l3m->channel_id[1] & 0x60)) {
				di->used_bchannel = l3m->channel_id[1]  & 0x3;
			} else {
				fprintf(stdout,"wrong channel id IE %02x %02x\n", l3m->channel_id[0], l3m->channel_id[1]);
				return 2;
			}
			di->flag |= FLG_CALL_ACTIVE;
			if (di->used_bchannel < 1 || di->used_bchannel > 2) {
				fprintf(stdout,"got no valid bchannel nr %d\n", di->used_bchannel);
				return 3;
			}
		} else {
			fprintf(stdout,"got no channel_id IE\n");
			return 4;
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
			if (ret) {
				fprintf(stdout,"error %d on setup_bchannel\n", ret);
				return 5;
			}
			if (di->flag & FLG_BCHANNEL_EARLY) {
				ret = activate_bchan(di);
				if (!ret) {
					fprintf(stdout,"error on activate_bchan\n");
					return 6;
				}
			}
			break;
		case 6:
			di->flag |= FLG_BCHANNEL_SETUP;
			break;
	}
	if (di->flag & FLG_NT_MODE) {
		ret = send_connect(di);
		if (ret) {
			fprintf(stdout, "send to_layer3  error  %s\n", strerror(errno));
			return 7;
		}
		if (!(di->flag & FLG_BCHANNEL_EARLY)) {
			ret = activate_bchan(di);
			if (!ret) {
				fprintf(stdout,"error on activate_bchan\n");
				return 8;
			}
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
#ifdef NOTYET
		case 6:
			do_hw_loop(di);
			break;
#endif
		}
	} else {
		if (!(di->flag & FLG_CALL_ORGINATE)) {
			ret = di->layer3->to_layer3(di->layer3, MT_CONNECT, di->pid, NULL);
			if (ret < 0) {
				fprintf(stdout, "sendto error  %s\n", strerror(errno));
				return 9;
			}
		}
	}
	
	return 0;
}

int check_number(devinfo_t *di, struct l3_msg *l3m) {
	unsigned char		*np,*p, c[2];
	int			ret, l, mt;
	struct	l3_msg		*nl3m;

	if (l3m->called_nr) {
		p = l3m->called_nr;
		l = *p++;
		np = (unsigned char *)di->net_nr;
		np += strlen(di->net_nr);
		if (l > 1) {
			p++;
			l--;
			while(l--)
				*np++ = *p++;
			*np = 0;
		}
		ret = number_match(di->net_nr, di->msn, l3m->sending_complete);
		if (ret == 0) {
			mt = MT_CALL_PROCEEDING;
		} else if (ret == 1) {
			return 0;
		} else {
			/* Number cannot match */
			mt = MT_DISCONNECT;
			fprintf(stdout, "call rejected\n");
		}
	} else {
		return 0;
	}
	nl3m =  alloc_l3_msg();
	if (!nl3m) {
		fprintf(stdout, "cannot allocate l3 msg struct\n");
		return -ENOMEM;
	}
	if (mt == MT_DISCONNECT) {
		c[0] = 0x80 | CAUSE_LOC_PRVN_RMTUSER;
		c[1] = 0x80 | CAUSE_CALL_REJECTED;
		add_layer3_ie(nl3m, IE_CAUSE, 2, c);
	}
	ret = di->layer3->to_layer3(di->layer3, mt, di->pid, nl3m);
	if (ret < 0) {
		fprintf(stdout, "send to_layer3  error  %s\n", strerror(errno));
		return ret;
	}
	if (mt == MT_DISCONNECT)
		return 0;
	switch (di->func) {
		case 5:
			di->flag |= FLG_BCHANNEL_EARLY;
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			ret = setup_bchannel(di);
			if (ret) {
				fprintf(stdout,"error %d on setup_bchannel\n", ret);
				return 5;
			}
			if (di->flag & FLG_BCHANNEL_EARLY) {
				ret = activate_bchan(di);
				if (!ret) {
					fprintf(stdout,"error on activate_bchan\n");
					return 6;
				}
			}
			break;
		case 6:
			di->flag |= FLG_BCHANNEL_SETUP;
			break;
	}
	ret = send_connect(di);
	if (ret) {
		fprintf(stdout, "send to_layer3  error  %s\n", strerror(errno));
			return 1;
		}
	if (!(di->flag & FLG_BCHANNEL_EARLY)) {
		ret = activate_bchan(di);
		if (!ret) {
			fprintf(stdout,"error on activate_bchan\n");
			return 2;
		}
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
#ifdef NOTYET
	case 6:
		do_hw_loop(di);
		break;
#endif
	}
	return 0;
}

int do_dchannel(struct mlayer3 *l3, unsigned int cmd, unsigned int pid, struct l3_msg *l3m)
{
	devinfo_t		*di = l3->priv;
	int			ret;

	if (VerifyOn>3)
		fprintf(stdout,"do_dchannel: L3 cmd(%x) pid(%x)\n",
			cmd, pid);
	switch (cmd) {
	case MT_ASSIGN:
		if (!di->pid) {
			di->pid = pid;
			if (VerifyOn > 2)
				fprintf(stdout,"register pid %x\n", pid);
		} else if ((di->flag & FLG_NT_MODE) && (di->flag & FLG_CALL_ORGINATE)) {
			if (((di->pid & MISDN_PID_CRTYPE_MASK) == MISDN_PID_MASTER) &&
			    ((di->pid & MISDN_PID_CRVAL_MASK) == (pid & MISDN_PID_CRVAL_MASK))) {
				if (VerifyOn > 2)
					fprintf(stdout,"select pid %x for %x\n", pid, di->pid);
				di->pid = pid;
			} else
				fprintf(stdout,"got MT_ASSIGN for pid %x but already have pid %x\n", pid, di->pid);
		} else
			fprintf(stdout,"got MT_ASSIGN for pid %x but already have pid %x\n", pid, di->pid);
		return 0;
	case MT_SETUP:
		if (!(di->flag & FLG_CALL_ORGINATE)) {
			if (VerifyOn)
				fprintf(stdout,"got setup pid(%x)\n", pid);
			di->pid = pid;
			if (!(di->flag & FLG_CALL_ACTIVE)) {
				ret = setup_channel(di, l3m);
				if (ret) {
					fprintf(stdout,"setup_channel returned error %d\n", ret);
					di->layer3->to_layer3(di->layer3, MT_RELEASE_COMPLETE, di->pid, NULL);
					di->flag |= FLG_STOP;
				}
				pthread_mutex_unlock(&di->wait);
			}
		}
		goto free_msg;
	}
	if (pid != di->pid) {
		if (di->flag & FLG_NT_MODE) {
			if ((di->flag & FLG_CALL_ORGINATE) && ((di->pid & MISDN_PID_CRTYPE_MASK) == MISDN_PID_MASTER) &&
			    ((di->pid & MISDN_PID_CRVAL_MASK) == (pid & MISDN_PID_CRVAL_MASK))) {
				if (VerifyOn > 2)
					fprintf(stdout,"got message (%x) with pid(%x) for %x\n", cmd, pid, di->pid);
			} else {
				fprintf(stdout,"got message (%x) with pid(%x) but not %x\n", cmd, pid, di->pid);
				goto free_msg;
			}
		} else {
			fprintf(stdout,"got message (%x) with pid(%x) but not %x\n", cmd, pid, di->pid);
			goto free_msg;
		}
	}
	switch (cmd) {
	case MT_FREE:
		if (di->pid == pid) {
			di->pid = 0;
			di->flag |= FLG_STOP;
			pthread_mutex_unlock(&di->wait);
		}
		break;
	case MT_SETUP_ACKNOWLEDGE:
	case MT_CALL_PROCEEDING:
	case MT_ALERTING:
		if (di->flag & FLG_NT_MODE) {
			if (di->flag & FLG_CALL_ORGINATE) {
				ret = setup_bchannel(di);
				if (ret == 1) {
					pthread_mutex_unlock(&di->wait);
				} else if (ret) {
					fprintf(stdout, "setup Bchannel error %d\n", ret);
				}
			}
		} else {
			if (di->flag & FLG_CALL_ORGINATE) {
				if (!(di->flag & FLG_CALL_ACTIVE)) {
					ret = setup_channel(di, l3m);
					if (ret) {
						fprintf(stdout,"setup_channel returned error %d\n", ret);
						di->layer3->to_layer3(di->layer3, MT_RELEASE_COMPLETE, di->pid, NULL);
						di->flag |= FLG_STOP;
					}
					pthread_mutex_unlock(&di->wait);
				}
			}
		}
		break;
	case MT_INFORMATION:
		if (di->flag & FLG_NT_MODE) {
			ret = check_number(di, l3m);
			if (ret) {
				fprintf(stdout,"setup_channel returned error %d\n", ret);
			}
		}
		break;
	case MT_CONNECT:
		if (di->flag & FLG_CALL_ORGINATE) {
			/* We got connect, so bring B-channel up */
			if (!(di->flag & FLG_BCHANNEL_SETUP)) {
				fprintf(stdout,"CONNECT but no bchannel selected\n");
				di->layer3->to_layer3(di->layer3, MT_RELEASE_COMPLETE, di->pid, NULL);
				di->flag |= FLG_STOP;
				pthread_mutex_unlock(&di->wait);
				break;
			}
			if (!(di->flag & FLG_BCHANNEL_EARLY) && !(di->flag & FLG_BCHANNEL_LOOP)) {
				if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
					activate_bchan(di);
			} else
				di->flag |= FLG_BCHANNEL_DOACTIVE;
			/* send a CONNECT_ACKNOWLEDGE */
			ret = di->layer3->to_layer3(di->layer3, MT_CONNECT_ACKNOWLEDGE, di->pid, NULL);
			if (ret < 0) {
				fprintf(stdout, "to_layer3 error  %s\n", strerror(-ret));
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
#ifdef NOTYET
			case 6:
				do_hw_loop(di);
				break;
#endif
			}
		}
		break;
	case MT_CONNECT_ACKNOWLEDGE:
		if (!(di->flag & FLG_CALL_ORGINATE)) {
			/* We got connect ack, so bring B-channel up */
			if (!(di->flag & FLG_BCHANNEL_SETUP)) {
				fprintf(stdout,"CONNECT but no bchannel selected\n");
				di->layer3->to_layer3(di->layer3, MT_RELEASE_COMPLETE, di->pid, NULL);
				di->flag |= FLG_STOP;
				pthread_mutex_unlock(&di->wait);
				break;
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
#ifdef NOTYET
			case 6:
				do_hw_loop(di);
				break;
#endif
			}
		}
		break;
	case MT_DISCONNECT:
		/* send a RELEASE */
		ret = di->layer3->to_layer3(di->layer3, MT_RELEASE, di->pid, NULL);
		if (ret < 0) {
			fprintf(stdout, "to_layer3 error  %s\n", strerror(-ret));
		}
		break;
	case MT_RELEASE:
	case MT_RELEASE_COMPLETE:
		switch (di->func) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			deactivate_bchan(di);
			break;
		}
		break;
	default:
		if (VerifyOn) {
			fprintf(stdout,"got unexpected D message cmd(%x) id(%x)\n",
				cmd, pid);
		}
		break;
	}
free_msg:
	if (l3m)
		free_l3_msg(l3m);
	return 0;
}

int do_setup(devinfo_t *di) {
	int		ret;
	struct timeval	tv;
	struct timespec	ts;
	unsigned int	prop, protocol;

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
#ifdef NOTYET
		case 6:
			di->bproto = ISDN_P_B_RAW;
			di->si = 1;
			di->flag |= FLG_BCHANNEL_LOOP;
			break;
#endif
		default:
			fprintf(stdout,"unknown program function %d\n",
				di->func);
			return 1;
	}
	if (VerifyOn > 8)
		fprintf(stdout, "init_layer3(4)\n");
	if (VerifyOn > 8)
		fprintf(stdout, "done\n");
	if (di->flag & FLG_NT_MODE) {
		protocol = L3_PROTOCOL_DSS1_NET;
	} else {
		protocol = L3_PROTOCOL_DSS1_USER;
	}
	prop = 0;
	if (VerifyOn > 8)
		fprintf(stdout, "open_layer3(%d, %x, %x, %p, %p)\n",
			di->cardnr - 1, protocol, prop , do_dchannel, di);
	di->layer3 = open_layer3(di->cardnr - 1, protocol, prop , do_dchannel, di);
	if (VerifyOn > 8)
		fprintf(stdout, "done\n");
	if (!di->layer3) {
		fprintf(stdout, "cannot open layer3\n");
		return 2;
	}
	if (VerifyOn)
		fprintf(stdout, "open card %d with %d channels options(%lx)\n", di->cardnr, di->layer3->nr_bchannel, di->layer3->options);
	
	pthread_mutex_init(&di->wait, NULL);
	pthread_mutex_lock(&di->wait);
	
	
	if (strlen(di->phonenr)) {
		di->flag |= FLG_CALL_ORGINATE;
		send_SETUP(di, di->si, di->phonenr);
	}
	di->timeout = 60;
	ret = gettimeofday(&tv, NULL);
	if (ret) {
		fprintf(stdout, "cannot get time\n");
		return 3;
	}
	ts.tv_sec = tv.tv_sec + di->timeout;
	ts.tv_nsec = tv.tv_usec * 1000;
	di->timeout = 60;
	ret = pthread_mutex_timedlock(&di->wait, &ts);
	if (ret) {
		if (ret == ETIMEDOUT)
			fprintf(stdout, "timed out, exit\n");
		else
			fprintf(stdout, "pthread_mutex_timedlock error %s\n", strerror(ret));
		close_layer3(di->layer3 );
		cleanup_layer3();
		return 4;
	}
	if (VerifyOn)
		fprintf(stdout, "got first answer continue\n");
	if (di->flag & FLG_STOP) {
		fprintf(stdout, "stop requested\n");
		close_layer3(di->layer3 );
		cleanup_layer3();
		return 5;
	}
	/* wait for final hangup */
	pthread_mutex_lock(&di->wait);
	/* wait to clean up */
	sleep(1);
	close_layer3(di->layer3 );
	cleanup_layer3();
	return 0;
}

int main(argc,argv)
int argc;
char *argv[];

{
	char FileName[200],FileNameOut[200];
	int aidx=1,para=1, idx;
	char sw;
	devinfo_t mISDN;
	int err;

	fprintf(stderr,"TestmISDN 1.0\n");
	strcpy(FileName, "test_file");
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 1;
	strcpy(mISDN.msn, "123");
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
					case 'N':
						mISDN.flag |= FLG_NT_MODE;
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
					case 'm':
					        if (!argv[aidx][2]) {
					        	idx = 0;
							aidx++;
						} else {
							idx=2;
						}
						if (aidx<=argc) {
							strcpy(mISDN.msn, &argv[aidx][idx]);
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
						strcpy(FileName,argv[aidx]);
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

	init_layer3(4);

	err = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (err < 0) {
		printf("TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return 1;
	}
	close(err);
	sprintf(FileNameOut,"%s.out",FileName);
	sprintf(FileName,"%s.in",FileName);
	if (VerifyOn > 2)
		mISDN_debug_init(0xffffffff, "testlayer3.debug", "testlayer3.warn", "testlayer3.error");
	if (0>(mISDN.save = open(FileName, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU))) {
		printf("TestmISDN cannot open %s due to %s\n",FileName,
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

	close(mISDN.save);
	if (mISDN.play>=0)
		close(mISDN.play);
	if (mISDN.bchan > 0)
		close(mISDN.bchan);
	mISDN_debug_close();
	return 0;
}
