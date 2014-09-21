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

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <mISDN/q931.h>
#include <mISDN/mISDNif.h>
#include <mISDN/mlayer3.h>

void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options] [filename]\n", pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"    filename     filename.in  incoming data\n");
	fprintf(stderr,"                 filename.out outgoing data\n");
	fprintf(stderr,"                 data is alaw for voice\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr, "   -?, --help                     this help\n");
	fprintf(stderr, "   -a  --af <n>                   use address family number n (default %d)\n", MISDN_AF_ISDN);
	fprintf(stderr, "   -C  --complete                 SETUP sending complete\n");
	fprintf(stderr, "   -c, --controller <n>           use controller n (default 0)\n");
	fprintf(stderr, "   -f, --function <n>             do function n (default 0)\n");
	fprintf(stderr, "   -l, --libdebug <mask>          use library debugmask\n");
	fprintf(stderr, "   -m  --msn <number>             use as calling number\n");
	fprintf(stderr, "   -N  --nt                       NT Mode\n");
	fprintf(stderr, "   -n  --number <phonenumber>     use phone number\n");
	fprintf(stderr, "   -P  --PtP                      PtP Mode\n");
	fprintf(stderr, "   -p  --progress  <LLDD>         add progress (LL 2 digit loc DD 2 digit description\n");
	fprintf(stderr, "   -T, --timeout <n>              use timeout (default 30) -1 no timeout\n");
	fprintf(stderr, "   -v, --verbose <level>          set verbose level\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   Available functions:\n");
	fprintf(stderr, "       0 send and recive voice\n");
	fprintf(stderr, "       1 send touchtones\n");
	fprintf(stderr, "       2 receive touchtones\n");
	fprintf(stderr, "       3 send and receive hdlc data\n");
	fprintf(stderr, "       4 send and receive X75 data\n");
	fprintf(stderr, "       5 send and receive voice early B connect\n");
	fprintf(stderr, "       6 loop back voice\n");
	fprintf(stderr, "       7 send and recive voice extra FACILITY\n");
	fprintf(stderr, "\n");
}

typedef struct _devinfo {
	int				contr;
	int				func;
	char				phonenr[32];
	char				msn[32];
	char				net_nr[32];
	struct mlayer3			*l3;
	struct pollfd			pfd[2];
	int				npfd;
	unsigned int			pid;
	unsigned int			progress;
	struct misdn_channel_info	cid;
	int				bchan;
	int				bproto;
	int				used_bchannel;
	int				pipe[2];
	unsigned int			nt:1;
	unsigned int			PtP:1;
	unsigned int			sending_complete:1;
	unsigned int			orginate:1;
	unsigned int			send_tone:1;
	unsigned int			cid_sent:1;
	unsigned int			active:1;
	unsigned int			hangup:1;
	unsigned int			send_data:1;
	unsigned int			bch_setup:1;
	unsigned int			bch_OK:1;
	unsigned int			bch_doactive:1;
	unsigned int			bch_active:1;
	unsigned int			bch_actdelayed:1;
	unsigned int			bch_early:1;
	unsigned int			bch_loop:1;
	unsigned int			bch_loop_set:1;
	unsigned int			bch_stop:1;
	unsigned int			stop:1;
	int				save;
	int				play;
	FILE				*fplay;
	int				si;
	struct timeval			read_timeout;
	int				worker_tick;
	time_t				timeout;
	time_t				lasttime;
	int				cfg_timeout;
} devinfo_t;

#define MAX_REC_BUF		4000
#define MAX_DATA_BUF		1024

#define ReadPipe		pipe[0]
#define WritePipe		pipe[1]

#define PRIV_ADDBC_FD		0x01010002
#define PRIV_RMBC_FD		0x01020002


#ifdef NOTYET
static char tt_char[] = "0123456789ABCD*#";
#endif

#define PLAY_SIZE 64

static int VerboseLevel, LibDebug;
static char *DataFileName;

#define TEST_FAC_LEN		39
static unsigned char test_facility[] =  {0x91, 0xA1, 0x24, 0x02, 0x02, 0x40, 0x00, 0x02,
					 0x01, 0x0F, 0x30, 0x1B, 0x02, 0x01, 0x01, 0x0A,
					 0x01, 0x00, 0xA1, 0x13, 0xA0, 0x11, 0xA1, 0x0F,
					 0x0A, 0x01, 0x02, 0x12, 0x0A, 0x31, 0x34, 0x31,
					 0x35, 0x35, 0x32, 0x34, 0x39, 0x36, 0x36 };

static int opt_parse(devinfo_t *di, int ac, char *av[])
{
	int c, af;

	memset(di, 0, sizeof(*di));
	DataFileName = NULL;
	di->pid = MISDN_PID_NONE;
	di->cfg_timeout = 30;

	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, '?'},
			{"af", 1, 0, 'a'},
			{"complete", 0, 0, 'C'},
			{"controller", 1, 0, 'c'},
			{"function", 1, 0, 'f'},
			{"libdebug", 1, 0, 'l'},
			{"msn", 1, 0, 'm'},
			{"nt", 0, 0, 'N'},
			{"number", 1, 0, 'n'},
			{"PtP", 0, 0, 'P'},
			{"progress", 1, 0, 'p'},
			{"timeout", 1, 0, 'T'},
			{"verbose", 1, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "?a:Cc:f:l:m:Nn:Pp:T:v:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			fprintf(stderr, "option %s", long_options[option_index].name);
			if (optarg)
				fprintf(stderr, " with arg %s", optarg);
			fprintf(stderr, "\n");
			usage(av[0]);
			return -1;
		case 'a':
			if (optarg) {
				af = atol(optarg);
				if (set_af_isdn(af) < 0) {
					fprintf(stderr, "Wrong address family number %s\n", optarg);
					return -2;
				}
			} else {
				fprintf(stderr, "option -a but no AF number\n");
				return -2;
			}
			break;
		case 'C':
			di->sending_complete = 1;
			break;
		case 'c':
			if (optarg)
				di->contr = atoi(optarg);
			else {
				fprintf(stderr, "option -c but no controller number\n");
				return -2;
			}
			break;
		case 'f':
			if (optarg)
				di->func = atoi(optarg);
			else {
				fprintf(stderr, "option -c but no function number\n");
				return -2;
			}
			break;
		case 'l':
			if (optarg)
				LibDebug = strtol(optarg, NULL, 0);
			else {
				fprintf(stderr, "option -c but no mask\n");
				return -2;
			}
			break;
		case 'm':
			if (optarg) {
				strncpy(di->msn, optarg, 31);
			} else {
				fprintf(stderr, "option -m but no number\n");
				return -2;
			}
			break;
		case 'N':
			di->nt = 1;
			break;
		case 'n':
			if (optarg) {
				strncpy(di->phonenr, optarg, 31);
			} else {
				fprintf(stderr, "option -n but no number\n");
				return -2;
			}
			break;
		case 'P':
			di->PtP = 1;
			break;
		case 'p':
			if (optarg)
				di->progress = strtol(optarg, NULL, 16);
			else {
				fprintf(stderr, "option -p but no progress value\n");
				return -2;
			}
			break;
		case 'T':
			if (optarg)
				di->cfg_timeout = atoi(optarg);
			else {
				fprintf(stderr, "option -T but no timeout value\n");
				return -2;
			}
			break;
		case 'v':
			if (optarg) {
				errno = 0;
				VerboseLevel = (unsigned int)strtol(optarg, NULL, 0);
				if (errno) {
					fprintf(stderr, "cannot read verbose level from %s - %s\n", optarg, strerror(errno));
					return -3;
				}
			} else {
				fprintf(stderr, "option -v but no value for verbose\n");
				return -3;
			}
			break;
		case '?':
			usage(av[0]);
			return -1;
		}
	}
	c = ac - optind;
	if (c != 0) {
		DataFileName = strdup(av[optind]);
	}
	return 0;
}

static int mylog(int level, const char *fmt, ...)
{
	char		logbuf[512];
	struct timeval	tv;
	struct tm	tm;
	int		l;
	va_list	args;

	if (level > VerboseLevel)
		return 0;
	va_start(args, fmt);
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	l = sprintf(logbuf, "%02d.%02d.%04d %02d:%02d:%02d.%06d  ",
		    tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, (int)tv.tv_usec);
	vsnprintf(&logbuf[l], 512 - l, fmt, args);
	va_end(args);
	return fprintf(stdout, "%s", logbuf);
}

static void send_ctrl(devinfo_t *di, unsigned int cmd)
{
	int ret;

	ret = write(di->WritePipe, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd)) {
		mylog(1, "Cannot write control %x - %s\n", cmd, strerror(errno));
	}
}

static int play_msg(devinfo_t *di) {
	unsigned char buf[PLAY_SIZE + MISDN_HEADER_LEN];
	struct  mISDNhead *hh = (struct  mISDNhead *)buf;
	int len, ret;
	
	if (di->play < 0)
		return 0;
	len = read(di->play, buf + MISDN_HEADER_LEN, PLAY_SIZE);
	if (len < 1) {
		if (len < 0)
			mylog(0, "play_msg err %d: \"%s\"\n", errno, strerror(errno));
		close(di->play);
		di->play = -1;
		return 0;
	}
	
	hh->prim = PH_DATA_REQ;
	hh->id = 42;
	ret = sendto(di->bchan, buf, len + MISDN_HEADER_LEN, 0, NULL, 0);
	if (ret < 0)
		mylog(0, "play send error %d %s\n", errno, strerror(errno));
	else
		mylog(7, "play send ret=%d\n", ret);
	return ret;
}

static int send_data(devinfo_t *di) {
	char buf[MAX_DATA_BUF + MISDN_HEADER_LEN];
	struct  mISDNhead *hh = (struct  mISDNhead *)buf;
	char *data;
	int len, ret;
	
	if (di->play < 0 || !di->fplay)
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
		mylog(0, "send_data error %d %s\n", errno, strerror(errno));
	else
		mylog(7, "send_data ret=%d\n", ret);
	return ret;
}

static int setup_bchannel(devinfo_t *di) {
	int			ret;
	struct sockaddr_mISDN	addr;

	if (di->bch_setup)
		return 1;
	if ((di->used_bchannel < 1) || (di->used_bchannel > 2)) {
		mylog(0, "wrong channel %d\n", di->used_bchannel);
		return 2;
	}
	di->bchan = socket(PF_ISDN, SOCK_DGRAM, di->bproto);
	if (di->bchan < 0) {
		mylog(0, "could not open bchannel socket %s\n", strerror(errno));
		return 3;
	}

	ret = fcntl(di->bchan, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		mylog(0, "fcntl error %s\n", strerror(errno));
		return 4;
	}

	addr.family = AF_ISDN;
	addr.dev = di->contr;
	addr.channel = di->used_bchannel;

	ret = bind(di->bchan, (struct sockaddr *) &addr, sizeof(addr));

	if (ret < 0) {
		mylog(0, "could not bind bchannel socket %s\n", strerror(errno));
		return 5;
	}
	di->pfd[1].fd = di->bchan;
	di->pfd[1].events= POLLIN;

	send_ctrl(di, PRIV_ADDBC_FD);

	di->bch_setup = 1;
	return ret;
}

static int activate_bchan(devinfo_t *di) {
	unsigned char 		buf[2048];
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;
	int ret;

	if (di->bproto == ISDN_P_B_X75SLP)
		hh->prim = DL_ESTABLISH_REQ;
	else
		hh->prim = PH_ACTIVATE_REQ;
	hh->id   = MISDN_ID_ANY;
	ret = sendto(di->bchan, buf, MISDN_HEADER_LEN, 0, NULL, 0);
	
	if (ret < 0) {
		mylog(0, "could not send ACTIVATE_REQ %s\n", strerror(errno));
		return 0;
	}
	
	mylog(3, "ACTIVATE_REQ sendto ret=%d\n", ret);

	return ret;
}

static int deactivate_bchan(devinfo_t *di) {
	unsigned char 		buf[64];
	struct  mISDNhead	*hh = (struct  mISDNhead *)buf;
	int ret;

	if (!di->bch_active)
		return 0;
	if (di->bchan < 1)
		return 0;
	if (di->bproto == ISDN_P_B_X75SLP)
		hh->prim = DL_RELEASE_REQ;
	else
		hh->prim = PH_DEACTIVATE_REQ;
	hh->id = MISDN_ID_ANY;
	ret = sendto(di->bchan, buf, MISDN_HEADER_LEN, 0, NULL, 0);
	
	if (ret < 0) {
		mylog(0, "could not send DEACTIVATE_REQ %s\n", strerror(errno));
		return 9;
	}
	di->bch_active = 0;
	mylog(3, "DEACTIVATE_REQ sendto ret=%d\n", ret);

	return ret;
}

#ifdef NOTYET
static int send_touchtone(devinfo_t *di, int tone) {
	struct  mISDNhead frm;
	int tval, ret;

	mylog(1, "send_touchtone %c\n", DTMF_TONE_MASK & tone);
	tval = DTMF_TONE_VAL | tone;
	ret = mISDN_write_frame(di->device, &frm,
		di->b_adress[di->used_bchannel] | FLG_MSG_DOWN,
		PH_CONTROL | REQUEST, 0, 4, &tval, TIMEOUT_1SEC);
	mylog(3, "tt send ret=%d\n", ret);
	return ret;
}
#endif

#ifdef NOTYET
static void do_hw_loop(devinfo_t *di)
{
	struct mISDN_ctrl_req	creq;
	int ret;

	creq.op = MISDN_CTRL_LOOP;
	creq.channel = di->used_bchannel;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0)
		mylog(0, "do_hw_loop ioctl error %s\n", strerror(errno));
	else
		di->bch_loopset = 1;
}

static void del_hw_loop(devinfo_t *di)
{
	struct mISDN_ctrl_req	creq;
	int ret;

	if (!di->bch_loopset)
		return;
	creq.op = MISDN_CTRL_LOOP;
	creq.channel = 0;
	ret = ioctl(di->layer2, IMCTRLREQ, &creq);
	if (ret < 0)
		mylog(0, "del_hw_loop ioctl error %s\n", strerror(errno));
	di->bch_loopset = 0;
}
#endif

static int number_match(char *cur, char *msn, int complete) {

	mylog(0, "numbers: %s/%s\n", cur, msn);
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

static void Dsend_connect(devinfo_t *di)
{
	time_t		t;
	struct tm	tm;
	struct l3_msg	*l3m;
	int		ret;

	l3m = alloc_l3_msg();
	if (!l3m) {
		mylog(0, "cannot allocate l3 msg struct\n");
		return;
	}
	if (di->nt) {
		t = time(NULL);
		localtime_r(&t, &tm);
		mi_encode_date(l3m, &tm);
	}
	ret = di->l3->to_layer3(di->l3, MT_CONNECT, di->pid, l3m);
	if (ret) {
		mylog(0, "send to_layer3  error  %s\n", strerror(-ret));
		free_l3_msg(l3m);
	}
}

static void Dsend_disconnect(devinfo_t *di, int cause)
{
	struct l3_msg	*l3m;
	int		ret, loc;

	l3m = alloc_l3_msg();
	if (!l3m) {
		mylog(0, "cannot allocate l3 msg struct\n");
		return;
	}
	if (di->nt)
		loc = CAUSE_LOC_PRVN_LOCUSER;
	else
		loc =CAUSE_LOC_USER;
	mi_encode_cause(l3m, cause, loc, 0, NULL);
	ret = di->l3->to_layer3(di->l3, MT_DISCONNECT, di->pid, l3m);
	if (ret) {
		mylog(0, "send to_layer3  error  %s\n", strerror(-ret));
		free_l3_msg(l3m);
	} else
		mylog(4, "send Disconnect cause #%d\n", cause);
}

static void Dsend_release(devinfo_t *di, int cause)
{
	struct l3_msg	*l3m;
	int		ret, loc;

	l3m = alloc_l3_msg();
	if (!l3m) {
		mylog(0, "cannot allocate l3 msg struct\n");
		return;
	}
	if (di->nt)
		loc = CAUSE_LOC_PRVN_LOCUSER;
	else
		loc =CAUSE_LOC_USER;
	if (cause != NO_CAUSE)
		mi_encode_cause(l3m, cause, loc, 0, NULL);
	ret = di->l3->to_layer3(di->l3, MT_RELEASE, di->pid, l3m);
	if (ret) {
		mylog(0, "send to_layer3  error  %s\n", strerror(-ret));
		free_l3_msg(l3m);
	} else {
		if (cause != NO_CAUSE)
			mylog(4, "send Release cause #%d\n", cause);
		else
			mylog(4, "send Release without cause\n");
	}
}

static int Dsend_SETUP(devinfo_t *di)
{
	int			ret;
	struct	l3_msg		*l3m;

	if (di->pid != MISDN_PID_NONE) {
		mylog(0, "Already have PID=%x assigned\n", di->pid);
		return -1;
	}
	di->pid = request_new_pid(di->l3);
	if (di->pid == MISDN_PID_NONE) {
		mylog(0, "cannot get a new pid\n");
		return -1;
	}

	l3m = alloc_l3_msg();
	if (!l3m) {
		mylog(0, "cannot allocate l3 msg struct\n");
		return -ENOMEM;
	}

	if (di->sending_complete)
		l3m->sending_complete = 1;

	if (di->si == 1) { /* Audio */
		ret = mi_encode_bearer(l3m, Q931_CAP_3KHZ_AUDIO, Q931_L1INFO_ALAW, 0, 0x10);
	} else { /* default Datatransmission 64k */
		ret = mi_encode_bearer(l3m, Q931_CAP_UNRES_DIGITAL, 0, 0, 0x10);
	}
	if (ret)
		mylog(0, "cannot add Bearer\n");

	if (di->nt) {
		di->used_bchannel = 1;
		di->cid.nr = di->used_bchannel;
		di->cid.type = MI_CHAN_TYP_B;
		di->cid.flags = MI_CHAN_FLG_EXCLUSIVE;
		di->cid.ctrl = MI_CHAN_CTRL_NEEDSEND;
		di->bch_OK = 1;
		di->cid_sent = 1;
	} else {
		di->cid.nr = MI_CHAN_ANY;
		di->cid.type = MI_CHAN_TYP_B;
		di->cid.flags = MI_CHAN_FLG_ANY;
		di->cid.ctrl = MI_CHAN_CTRL_NEEDSEND;
	}
	ret = mi_encode_channel_id(l3m, &di->cid);

	if (ret) {
		mylog(0, "cannot add CID\n");
		return -1;
	}

	if (di->func == 7) {
		/* Add FAC for rerouting info */
		ret = add_layer3_ie(l3m, IE_FACILITY, TEST_FAC_LEN, test_facility);
		if (ret) {
			mylog(0, "cannot add FAC\n");
		}
	}

	if (di->progress) {
		struct misdn_progress_info prg;

		prg.loc = (di->progress >> 8) & 0x7f;
		prg.desc = di->progress & 0x7f;
		prg.ctrl = MI_PROG_CTRL_NEEDSEND;
		ret = mi_encode_progress(l3m, &prg);
		if (ret) {
			mylog(0, "cannot add CID\n");
			return -1;
		}
	}

	if (di->msn) {
		if (di->nt)
			ret = mi_encode_calling_nr(l3m, di->msn, Q931_NPRESENTATION_ALLOWED, Q931_NSCREEN_NETWORK, Q931_NTYPE_NATIONAL, Q931_NPLAN_ISDN);
		else
			ret = mi_encode_calling_nr(l3m, di->msn, Q931_NPRESENTATION_ALLOWED, Q931_NSCREEN_USER_NOT, Q931_NTYPE_UNKNOWN, Q931_NPLAN_ISDN);
		if (ret) {
			mylog(0, "cannot add calling number\n");
			return -1;
		}
	}
	if (di->phonenr) {
		ret = mi_encode_called_nr(l3m, di->phonenr, Q931_NTYPE_UNKNOWN, Q931_NPLAN_ISDN);
		if (ret) {
			mylog(0, "cannot add called number\n");
			return -1;
		}
	}
	ret = di->l3->to_layer3(di->l3, MT_SETUP, di->pid, l3m);
	if (ret < 0) {
		mylog(0, "send to_layer3  error  %s\n", strerror(-ret));
		free_l3_msg(l3m);
	}
	return ret;
}

static void Dsend_answer(devinfo_t *di, unsigned int mt, int cause)
{
	struct l3_msg	*l3m;
	int		ret, loc;

	l3m = alloc_l3_msg();
	if (!l3m) {
		mylog(0, "cannot allocate l3 msg struct\n");
		return;
	}
	/* channel is only added if cid.ctrl has MI_CHAN_CTRL_NEEDSEND */
	ret = mi_encode_channel_id(l3m, &di->cid);
	if (ret) {
		mylog(0, "cannot add CID\n");
		return;
	}
	if (di->nt)
		loc = CAUSE_LOC_PRVN_LOCUSER;
	else
		loc =CAUSE_LOC_USER;
	if (cause != NO_CAUSE)
		mi_encode_cause(l3m, cause, loc, 0, NULL);
	ret = di->l3->to_layer3(di->l3, mt, di->pid, l3m);
	if (ret) {
		mylog(0, "send %s to_layer3  error  %s\n", mi_msg_type2str(mt), strerror(-ret));
		free_l3_msg(l3m);
	} else
		mylog(4, "send %s\n", mi_msg_type2str(mt));
}

static int answer_call(devinfo_t *di, struct l3_msg *l3m) {
	unsigned char	*np, *p;
	int		cause, ret, l, mt;

	cause = NO_CAUSE;
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
			cause = CAUSE_CALL_REJECTED;
		}
	} else {
		mt = MT_SETUP_ACKNOWLEDGE;
	}

	if (mt != MT_RELEASE_COMPLETE) {
		if (di->nt && !di->cid_sent) {
			di->cid.nr = di->used_bchannel;
			di->cid.type = MI_CHAN_TYP_B;
			di->cid.flags = MI_CHAN_FLG_EXCLUSIVE;
			di->cid.ctrl = MI_CHAN_CTRL_NEEDSEND;
			di->cid_sent = 1;
		}
	}
	Dsend_answer(di, mt, cause);
	if (mt == MT_CALL_PROCEEDING)
		return 1;
	else
		return 0;
}

static int setup_channel(devinfo_t *di, struct l3_msg *l3m)
{
	int	ret;

	if (di->active)
		return 0;
	if (di->nt) {
		di->used_bchannel = 1;
		ret = answer_call(di, l3m);
		if (ret < 0) {
			mylog(0, "answer_call return %d\n", ret);
			return 1;
		}
		di->active = 1;
		if (!ret) {
			return 0;
		}
	} else {
		if (l3m->channel_id) {
			if (l3m->channel_id[0] == 1 && !(l3m->channel_id[1] & 0x60)) {
				di->used_bchannel = l3m->channel_id[1]  & 0x3;
			} else {
				mylog(0, "wrong channel id IE %02x %02x\n", l3m->channel_id[0], l3m->channel_id[1]);
				return 2;
			}
			di->active = 1;
			if (di->used_bchannel < 1 || di->used_bchannel > 2) {
				mylog(0, "got no valid bchannel nr %d\n", di->used_bchannel);
				return 3;
			}
		} else {
			mylog(0, "got no channel_id IE\n");
			return 4;
		}
	}
	switch (di->func) {
		case 5:
			di->bch_early = 1;
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 7:
			ret = setup_bchannel(di);
			if (ret) {
				mylog(0, "error %d on setup_bchannel\n", ret);
				return 5;
			}
			if (di->bch_early) {
				ret = activate_bchan(di);
				if (!ret) {
					mylog(0, "error on activate_bchan\n");
					return 6;
				}
			}
			break;
		case 6:
			di->bch_setup = 1;
			break;
	}
	if (di->nt) {
		Dsend_connect(di);

		if (!di->bch_early) {
			ret = activate_bchan(di);
			if (!ret) {
				mylog(0, "error on activate_bchan\n");
				return 8;
			}
		}
		switch (di->func) {
		case 0:
		case 2:
		case 5:
		case 7:
			if (di->play > -1)
				play_msg(di);
			break;
		case 1:
			/* send next after 2 sec */
			di->timeout = 2;
			di->send_tone = 1;
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
		if (!di->orginate) {
			Dsend_connect(di);
		}
	}
	
	return 0;
}

static int check_number(devinfo_t *di, struct l3_msg *l3m) {
	unsigned char		*np,*p;
	int			ret, l, mt, cause;

	cause = NO_CAUSE;
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
			cause = CAUSE_CALL_REJECTED;
		}
	} else {
		return 0;
	}

	Dsend_answer(di, mt, cause);

	if (mt == MT_DISCONNECT)
		return 0;

	switch (di->func) {
		case 5:
			di->bch_early = 1;
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 7:
			ret = setup_bchannel(di);
			if (ret) {
				mylog(0, "error %d on setup_bchannel\n", ret);
				return 5;
			}
			if (di->bch_early) {
				ret = activate_bchan(di);
				if (!ret) {
					mylog(0, "error on activate_bchan\n");
					return 6;
				}
			}
			break;
		case 6:
			di->bch_setup = 1;
			break;
	}

	Dsend_connect(di);

	if (!di->bch_early) {
		ret = activate_bchan(di);
		if (!ret) {
			mylog(0, "error on activate_bchan\n");
			return 2;
		}
	}
	switch (di->func) {
	case 0:
	case 2:
	case 5:
	case 7:
		if (di->play > -1)
			play_msg(di);
		break;
	case 1:
		/* send next after 2 sec */
		di->timeout = 2;
		di->send_tone = 1;
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

static void get_bchannel(devinfo_t *di, struct l3_msg *l3m)
{
	int ret;
	struct misdn_channel_info cid = {MI_CHAN_NONE, 0, 0, 0};

	ret = mi_decode_channel_id(l3m, &cid);
	if (ret)
		mylog(0, "error decoding B-channel %s\n", strerror(-ret));

	if (cid.nr < MI_CHAN_DCHANNEL) {
		/* valid channel number */
		mylog(3, "Got B%d assigned\n", cid.nr);
		di->cid = cid;
		di->bch_OK = 1;
		di->used_bchannel = cid.nr;
	}
}

static int from_layer3(struct mlayer3 *l3, unsigned int cmd, unsigned int pid, struct l3_msg *l3m)
{
	devinfo_t		*di = l3->priv;
	int			ret;

	mylog(4, "from L3 cmd %s (%x) pid(%x)\n", mi_msg_type2str(cmd), cmd, pid);
	switch (cmd) {
	case MT_ASSIGN:
		if (di->pid == MISDN_PID_NONE) {
			di->pid = pid;
			mylog(2, "register pid %x\n", pid);
		} else if (di->nt && di->orginate) {
			if (((di->pid & MISDN_PID_CRTYPE_MASK) == MISDN_PID_MASTER) &&
			    ((di->pid & MISDN_PID_CRVAL_MASK) == (pid & MISDN_PID_CRVAL_MASK))) {
				mylog(2, "select pid %x for %x\n", pid, di->pid);
				di->pid = pid;
			} else
				mylog(0, "got MT_ASSIGN for pid %x but already have pid %x\n", pid, di->pid);
		} else
			mylog(0, "got MT_ASSIGN for pid %x but already have pid %x\n", pid, di->pid);
		goto free_msg;
	case MT_SETUP:
		if (!di->orginate) {
			mylog(1, "got setup pid(%x)\n", pid);
			di->pid = pid;
			if (!di->active) {
				ret = setup_channel(di, l3m);
				if (ret) {
					mylog(0, "setup_channel returned error %d\n", ret);
					di->l3->to_layer3(di->l3, MT_RELEASE_COMPLETE, di->pid, NULL);
					di->stop = 1;
				}
				send_ctrl(di, cmd);
			}
		}
		goto free_msg;
	}
	if (pid != di->pid) {
		if (di->nt) {
			if (di->orginate && ((di->pid & MISDN_PID_CRTYPE_MASK) == MISDN_PID_MASTER) &&
			    ((di->pid & MISDN_PID_CRVAL_MASK) == (pid & MISDN_PID_CRVAL_MASK))) {
				mylog(2, "got message (%x) with pid(%x) for %x\n", cmd, pid, di->pid);
			} else {
				mylog(0, "got message (%x) with pid(%x) but not %x\n", cmd, pid, di->pid);
				goto free_msg;
			}
		} else {
			mylog(0, "got message (%x) with pid(%x) but not %x\n", cmd, pid, di->pid);
			goto free_msg;
		}
	}
	switch (cmd) {
	case MT_FREE:
		if (di->pid == pid) {
			di->pid = MISDN_PID_NONE;
			di->stop = 1;
			send_ctrl(di, cmd);
		}
		break;
	case MT_SETUP_ACKNOWLEDGE:
	case MT_CALL_PROCEEDING:
	case MT_ALERTING:
		mylog(3, "got %s pid(%x)\n", mi_msg_type2str(cmd), pid);
		if (!di->bch_OK)
			get_bchannel(di, l3m);
		if (di->nt) {
			if (di->orginate) {
				ret = setup_bchannel(di);
				if (ret) {
					mylog(0, "setup Bchannel returned %d\n", ret);
				}
			}
		} else {
			if (di->orginate) {
				if (!di->active) {
					ret = setup_channel(di, l3m);
					if (ret) {
						mylog(0, "setup_channel returned error %d\n", ret);
						Dsend_answer(di, MT_RELEASE_COMPLETE, CAUSE_NORMAL_CLEARING);
						di->stop= 1;
					}
				}
			}
		}
		send_ctrl(di, cmd);
		break;
	case MT_INFORMATION:
		if (di->nt) {
			ret = check_number(di, l3m);
			if (ret) {
				mylog(0, "setup_channel returned error %d\n", ret);
			}
		}
		break;
	case MT_CONNECT:
		if (di->orginate) {
			if (!di->bch_OK)
				get_bchannel(di, l3m);
			send_ctrl(di, cmd);
		}
		break;
	case MT_CONNECT_ACKNOWLEDGE:
		if (!di->orginate) {
			if (!di->bch_OK)
				get_bchannel(di, l3m);
			send_ctrl(di, cmd);
		}
		break;
	case MT_DISCONNECT:
		send_ctrl(di, cmd);
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
		case 7:
			deactivate_bchan(di);
			break;
		}
		send_ctrl(di, cmd);
		break;
	default:
		mylog(0, "got unexpected D message cmd %s id(%x)\n", mi_msg_type2str(cmd), pid);
		break;
	}
free_msg:
	if (l3m)
		free_l3_msg(l3m);
	return 0;
}

static int do_control_worker(devinfo_t *di)
{
	unsigned int	cmd;
	int		ret;

	ret = read(di->ReadPipe, &cmd, sizeof(cmd));
	if (ret < sizeof(cmd)) {
		mylog(0, "read control pipe error ret=%d - %s\n", ret, strerror(errno));
	} else {
		switch (cmd) {
		case MT_SETUP:
			mylog(3, "Got setup - accept call\n");
			break;
		case MT_SETUP_ACKNOWLEDGE:
		case MT_CALL_PROCEEDING:
		case MT_ALERTING:
			mylog(3, "got %s\n", mi_msg_type2str(cmd));
			break;
		case MT_CONNECT:
			mylog(3, "got %s\n", mi_msg_type2str(cmd));
			/* We got connect, so bring B-channel up */
			if (!di->bch_setup) {
				mylog(0, "CONNECT but no bchannel selected\n");
				Dsend_answer(di, MT_RELEASE_COMPLETE, CAUSE_NORMAL_CLEARING);
				di->stop = 1;
				break;
			}
			if (!di->bch_early && !di->bch_loop) {
				if (!di->bch_actdelayed)
					activate_bchan(di);
			} else
				di->bch_doactive = 1;
			/* send a CONNECT_ACKNOWLEDGE is done in library*/
			switch (di->func) {
			case 0:
			case 2:
			case 5:
			case 7:
				if (di->play > -1)
					play_msg(di);
				break;
			case 1:
				/* send next after 2 sec */
				di->timeout = 2;
				di->send_tone = 1;
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
		case MT_CONNECT_ACKNOWLEDGE:
			mylog(3, "got %s\n", mi_msg_type2str(cmd));
			/* We got connect ack, so bring B-channel up */
			if (!di->bch_setup) {
				mylog(0, "CONNECT but no bchannel selected\n");
				di->l3->to_layer3(di->l3, MT_RELEASE_COMPLETE, di->pid, NULL);
				di->stop = 1;
				break;
			}
			if (!di->bch_early && !di->bch_loop) {
				if (!di->bch_actdelayed)
					activate_bchan(di);
				else
					di->bch_doactive = 1;
			}
			/* if here is outgoing data, send first part */
			switch (di->func) {
			case 0:
			case 2:
			case 5:
			case 7:
				if (di->play > -1)
					play_msg(di);
				break;
			case 1:
				/* send next after 2 sec */
				di->timeout = 2;
				di->send_tone = 1;
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
			break;
		case MT_DISCONNECT:
			mylog(3, "got %s\n", mi_msg_type2str(cmd));
			Dsend_release(di, NO_CAUSE);
			break;
		case MT_RELEASE:
		case MT_RELEASE_COMPLETE:
			mylog(3, "got %s\n", mi_msg_type2str(cmd));
			di->stop = 1;
			break;
		case PRIV_ADDBC_FD:
			mylog(3, "Add BC fd to pollset nfds %d->%d\n", di->npfd, 2);
			di->npfd = 2; /* include pfd[1] */
			break;
		default:
			mylog(0, "control pipe unhandeld cmd %x - %s\n", cmd, mi_msg_type2str(cmd));
			break;
		}
	}
	return 0;
}


static int bch_worker(devinfo_t *di)
{
	unsigned char		buf[4104];
	int			len, ret;
	unsigned int		*tone;
	struct mISDNhead	*hh;

	hh = (struct  mISDNhead *)buf;

	len = recv(di->bchan, buf, MAX_REC_BUF, 0);
	if (len < 0) {
		ret = -errno;
		mylog(0, "recv error  %s\n", strerror(-ret));
	} else {
		if (len < MISDN_HEADER_LEN) {
			mylog(1, "got short B frame %d\n", len);
			ret = -EINVAL;
		} else
			ret = 0;

		mylog(8, "read B%d prim %s  id(%x) len(%d)\n", di->used_bchannel, mi_msg_type2str(hh->prim), hh->id, len);

		switch (hh->prim) {
		case PH_DATA_IND:
		case DL_DATA_IND:
			/* received data, save it */
			ret = write(di->save, buf + MISDN_HEADER_LEN, len - MISDN_HEADER_LEN);
			if (ret < 0)
				fprintf(stderr,"got error on write %s\n", strerror(errno));
			break;
		case PH_DATA_CNF:
			/* get ACK of send data, so we can
			 * send more
			 */
			mylog(7, "PH_DATA_CNF B%d id=%x\n", di->used_bchannel, hh->id);
			switch (di->func) {
				case 0:
				case 2:
				case 7:
					if (di->play > -1)
						play_msg(di);
					break;
			}
			break;
		case PH_CONTROL_IND:
			tone = (unsigned int *)(buf + MISDN_HEADER_LEN);
			if ((len == (4 + MISDN_HEADER_LEN)) && ((*tone & ~DTMF_TONE_MASK) == DTMF_TONE_VAL)) {
				mylog(0, "GOT TT %c\n", DTMF_TONE_MASK & *tone);
			} else
				mylog(0, "unknown PH_CONTROL len %d/val %x\n", len, *tone);
			break;
		case PH_ACTIVATE_IND:
		case PH_ACTIVATE_CNF:
		case DL_ESTABLISH_IND:
		case DL_ESTABLISH_CNF:
			mylog(5, "Got %s B%d now active\n", mi_msg_type2str(hh->prim), di->used_bchannel);
			di->bch_active = 1;
			break;
		case DL_RELEASE_CNF:
		case PH_DEACTIVATE_IND:
		case PH_DEACTIVATE_CNF:
			mylog(1, "B%d got termination request\n", di->used_bchannel);
			ret = 1;
			break;
		default:
			mylog(1, "got unexpected B%d frame %s id(%x) len(%d)\n", di->used_bchannel, mi_msg_type2str(hh->prim),
			      hh->id, len);
			break;
		}
	}
	return ret;
}


static int main_worker(devinfo_t *di)
{
	int ret, running = 1;
	int retcode = 1;
	time_t t, td;


	while(running) {
		ret = poll(di->pfd, di->npfd, di->worker_tick);
		t = time(NULL);
		td = t - di->lasttime;
		if (ret < 0) {
			mylog(0, "poll error(%d) - %s\n", errno, strerror(errno));
		} else if (ret == 0) {
			/* timeout */
			mylog(6, "Timertick %d ms %li sec to go\n", di->worker_tick, di->timeout - td);
			if (di->bch_active) {
#ifdef NOTYET
				if (di->send_tone) {
					if (di->val) {
						di->val--;
						send_touchtone(di, tt_char[di->val]);
					} else {
						di->send_tone = 0;
						/* After last tone disconnect */
						Dsend_disconnect(di, CAUSE_NORMAL_CLEARING);
					}
				}
#endif
				if (di->send_data) {
					if (di->play > -1)
						send_data(di);
					else
						di->send_data = 0;
				}
			}
			if (di->bch_doactive) {
				ret = activate_bchan(di);
				di->bch_doactive = 0;
				if (!ret) {
					mylog(0, "error on activate_bchan\n");
					Dsend_disconnect(di, CAUSE_NORMAL_CLEARING);
				}
				di->send_data = 1;
			}
		} else {
			/* some event */
			mylog(9, "Poll ready for %d events\n", ret);
			if (di->pfd[0].revents & POLLIN) {
				do_control_worker(di);
				if (di->stop) {
					retcode = 0;
					break;
				}
			}
			if (di->pfd[1].revents & POLLIN) {
				ret = bch_worker(di);
				if (ret) {
					mylog(1, "Bch worker returned %d\n", ret);
					close(di->bchan);
					di->bchan = -1;
					di->bch_active = 0;
					di->bch_setup = 0;
					if (di->active)
						Dsend_disconnect(di, CAUSE_NORMAL_CLEARING);
					else
						Dsend_answer(di, MT_RELEASE_COMPLETE, CAUSE_NORMAL_CLEARING);
					di->active = 0;
				}
			}
		}
		if (td >= di->timeout) {
			if (di->bchan > 0) {
#ifdef NOTYET
				if (di->bch_loop)
					del_hw_loop(di);
				else
#endif
				if (di->bch_active)
					deactivate_bchan(di);
				di->timeout += 2;
				continue;
			}
			if (!di->hangup) {
				di->hangup = 1;
				if (di->active)
					Dsend_disconnect(di, CAUSE_NORMAL_CLEARING);
				else
					Dsend_answer(di, MT_RELEASE_COMPLETE, CAUSE_NORMAL_CLEARING);
				di->active = 0;
				di->timeout += 2;
				continue;
			}
			retcode = 3;
			break;
		}
	}
	return retcode;
}

static int do_setup(devinfo_t *di) {
	int		ret;
	unsigned int	prop, protocol;

	switch (di->func) {
		case 0:
		case 5:
		case 7:
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
			di->bch_actdelayed = 1;
			break;
#ifdef NOTYET
		case 6:
			di->bproto = ISDN_P_B_RAW;
			di->si = 1;
			di->bch_loop = 1;
			break;
#endif
		default:
			mylog(0, "unknown program function %d\n",
				di->func);
			return 1;
	}
	if (di->nt) {
		protocol = L3_PROTOCOL_DSS1_NET;
	} else {
		protocol = L3_PROTOCOL_DSS1_USER;
	}
	if (di->PtP)
		prop = MISDN_FLG_PTP | MISDN_FLG_L2_HOLD;
	else
		prop = 0;
	mylog(6, "open_layer3(%d, %x, %x, %p, %p)\n",
			di->contr, protocol, prop, from_layer3, di);
	di->l3 = open_layer3(di->contr, protocol, prop, from_layer3, di);
	mylog(6, "open_layer3 done\n");
	if (!di->l3) {
		mylog(0, "cannot open layer3\n");
		return 2;
	}
	mylog(1, "open card %d with %d channels options(%lx)\n", di->contr, di->l3->nr_bchannel, di->l3->options);
	
	
	if (strlen(di->phonenr)) {
		di->orginate = 1;
		Dsend_SETUP(di);
	}
	/* we run the worker as main thread here */

	ret = pipe2(di->pipe, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stderr, "Cannot create control pipe - %s\n", strerror(errno));
		return 4;
	}
	di->npfd = 1;
	di->pfd[0].fd = di->ReadPipe;
	di->pfd[0].events = POLLIN;

	di->worker_tick = 1000; /* 1 sec */
	di->lasttime = time(NULL);
	di->timeout = di->cfg_timeout;
	ret = gettimeofday(&di->read_timeout, NULL);
	if (ret) {
		mylog(0, "cannot get time\n");
		return 3;
	}
	ret = main_worker(di);

	mylog(0, "Worker terminated with %d\n", ret);

	/* wait to clean up */
	sleep(1);
	close_layer3(di->l3 );
	cleanup_layer3();
	return ret;
}

static int my_lib_debug(const char *file, int line, const char *func, int level, const char *fmt, va_list va)
{
	int ret;
	char log[256];

	ret = vsnprintf(log, 256, fmt, va);
	if (VerboseLevel > 6) {
		if (level > MISDN_LIBDEBUG_WARN)
			mylog(6, "%20s:%4d %20s %s", file, line, func, log);
		else
			mylog(0, "%20s:%4d %20s %s", file, line, func, log);
	} else {
		mylog(0, "%s", log);
	}
	return ret;
}

static struct mi_ext_fn_s myfn = {
	.prt_debug = my_lib_debug,
};

int main(argc,argv)
int argc;
char *argv[];
{
	char FileNameIn[200],FileNameOut[200];
	devinfo_t DevInfo;
	int err;

	fprintf(stderr,"mISDN Test L3 2.0\n");

	if (argc < 1) {
		fprintf(stderr,"Error: Not enough arguments please check\n");
		usage(argv[0]);
		exit(1);
	} else {
		err = opt_parse(&DevInfo, argc, argv);
		if (err)
			exit(1);
	}
	if (!DataFileName)
		DataFileName = strdup("test_file");
	if (!DevInfo.msn[0])
		strcpy(DevInfo.msn, "123");

	init_layer3(4, &myfn);
	mISDN_set_debug_level(LibDebug);

	err = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (err < 0) {
		mylog(0, "TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return 1;
	}
	close(err);

	sprintf(FileNameOut, "%s.out", DataFileName);
	sprintf(FileNameIn, "%s.in", DataFileName);

	if (0>(DevInfo.save = open(FileNameIn, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU))) {
		mylog(0, "TestmISDN cannot open %s due to %s\n",FileNameIn, strerror(errno));
		return 1;
	}
	if (0>(DevInfo.play = open(FileNameOut, O_RDONLY))) {
		mylog(0, "TestmISDN cannot open %s due to %s\n",FileNameOut, strerror(errno));
		DevInfo.play = -1;
	} else 
		DevInfo.fplay = fdopen(DevInfo.play, "r");

	mylog(8, "fileno %d/%d\n",DevInfo.save, DevInfo.play);
	
	err = do_setup(&DevInfo);

	close(DevInfo.save);
	if (DevInfo.play>=0)
		close(DevInfo.play);
	if (DevInfo.bchan > 0)
		close(DevInfo.bchan);
	return 0;
}
