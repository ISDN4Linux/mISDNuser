/*
 * mISDNv2 CAPI 2.0 daemon
 *
 * Written by Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright (C) 2011 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this package for more details.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/un.h>
#include <errno.h>
#include <poll.h>
#include <mISDN/q931.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include "m_capi.h"
#include "mc_buffer.h"
#include "m_capi_sock.h"
#include "alaw.h"
#ifdef USE_SOFTFAX
#include "g3_mh.h"
#endif
#include <capi_debug.h>

/* for some reasons when _XOPEN_SOURCE 600 was defined lot of other things are not longer
 * defined so use this as workaround
 */

extern int posix_openpt(int flags);
extern int grantpt(int fd);
extern int unlockpt(int fd);
extern int ptsname_r(int fd, char *buf, size_t buflen);

#ifndef DEF_CONFIG_FILE
#define DEF_CONFIG_FILE	"/etc/capi20.conf"
#endif

#define MISDNCAPID_VERSION	"0.9"

typedef enum {
	PIT_None = 0,
	PIT_Control,
	PIT_mISDNmain,
	PIT_CAPImain,
	PIT_NewConn,
	PIT_Application,
	PIT_Bchannel,
} pollInfo_t;

struct pollInfo {
	pollInfo_t	type;
	void		*data;
};

#define MI_CONTROL_SHUTDOWN	0x01000000

static struct pollfd *mainpoll = NULL;
static struct pollInfo *pollinfo = NULL;
static int mainpoll_max = 0;
static int mainpoll_size = 0;
#define MAINPOLL_LIMIT 256

static char def_config[] = DEF_CONFIG_FILE;
static char *config_file = NULL;
static unsigned int debugmask = 0;
static int do_daemon = 1;
static int mIsock = 0;
static int mCsock = 0;
static int mIControl[2] = {-1, -1};
static struct pController *mI_Controller = NULL;
static int mI_count= 0;
static FILE *DebugFile = NULL;
static char *DebugFileName = NULL;
int KeepTemporaryFiles = 0;
static char _TempDirectory[80];
char *TempDirectory = NULL;

#define MISDND_TEMP_DIR	"/tmp"

static void usage(void)
{
	fprintf(stderr, "Usage: mISDNcapid [OPTIONS]\n");
	fprintf(stderr, "  Options are\n");
	fprintf(stderr, "   -?, --help                            this help\n");
	fprintf(stderr, "   -c, --config <file>                   use this config file - default %s\n", def_config);
	fprintf(stderr, "   -d, --debug <level>                   set debug level\n");
	fprintf(stderr, "   -D, --debug-file <debug file>         use debug file (default stdout/stderr)\n");
	fprintf(stderr, "   -f, --foreground                      run in forground, not as daemon\n");
	fprintf(stderr, "   -k, --keeptemp                        do not delete temporary files (e.g. TIFF for fax)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "mISDNcapid Version %s\n", MISDNCAPID_VERSION);
	fprintf(stderr, "\n");
}

static int opt_parse(int ac, char *av[])
{
	int c;

	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, '?'},
			{"config", 1, 0, 'c'},
			{"debug-file", 1, 0, 'D'},
			{"debug", 1, 0, 'd'},
			{"foreground", 0, 0, 'f'},
			{"keeptemp", 0, 0, 'k'},
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "?c:D:d:fk", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			fprintf(stderr, "option %s", long_options[option_index].name);
			if (optarg)
				fprintf(stderr, " with arg %s", optarg);
			fprintf(stderr, "\n");
			break;
		case 'c':
			if (optarg)
				config_file = optarg;
			else {
				fprintf(stderr, "option -c but no filename\n");
				return -2;
			}
			break;
		case 'D':
			if (optarg)
				DebugFileName = optarg;
			else {
				fprintf(stderr, "option -D but no filename\n");
				return -2;
			}
			break;
		case 'd':
			if (optarg) {
				errno = 0;
				debugmask = (unsigned int)strtol(optarg, NULL, 0);
				if (errno) {
					fprintf(stderr, "cannot read debuglevel from %s - %s\n", optarg, strerror(errno));
					return -3;
				}
			} else {
				fprintf(stderr, "option -d but no value for debugmask\n");
				return -3;
			}
			break;
		case 'f':
			do_daemon = 0;
			break;
		case 'k':
			KeepTemporaryFiles = 1;
			break;
		case '?':
			usage();
			return -1;
		}
	}
	c = ac - optind;
	if (c != 0) {
		fprintf(stderr, "unknown options: %s\n", av[optind]);
		return -2;
	}
	return 0;
}

static int read_config_file(char *name)
{
	int nr_controller = 0, lnr = 0;
	int contr = 0, capicontr, ena, n;
	FILE *f;
	char line[256], *s;

	f = fopen(name, "r");
	if (!f) {
		fprintf(stderr, "cannot open %s - %s\n", name, strerror(errno));
		return 0;
	}
	while ((s = fgets(line, 256, f))) {
		lnr++;
		switch (*s) {
		case '\n':
		case '#':
		case ';':
		case '!':
			/* comment or empty lines */
			continue;
			break;
		default:
			break;
		}
		if (!strncasecmp("mISDN", s, 5)) {
			n = sscanf(&s[5], "%d %d %d", &contr, &capicontr, &ena);
			switch (n) {
			case 0:
				nr_controller = -1;
				fprintf(stderr, "error in config file %s:%d:%s\n", name, lnr, line);
				goto err;
			case 1:
				capicontr = contr + 1;
			case 2:
				ena = 1;
				break;
			}
			if (contr > mI_count - 1) {
				fprintf(stderr, "Controller %d not detected - ignored\n", contr + 1);
				continue;
			}
			if (contr < 0 || contr > 126) {
				fprintf(stderr, "Invalid controller nr (%d)  in config file %s, line %d: %s\n", contr + 1, name,
					lnr, line);
				nr_controller = -2;
				goto err;
			}
			if (capicontr < 1 || capicontr > 127) {
				fprintf(stderr, "Invalid capi controller nr (%d)  in config file %s, line %d: %s\n", capicontr,
					name, lnr, line);
				nr_controller = -3;
				goto err;
			}
			mI_Controller[contr].mNr = contr;
			mI_Controller[contr].profile.ncontroller = capicontr;
			mI_Controller[contr].enable = ena;
			nr_controller++;
		}
		if (!strncasecmp("debugmask", s, 9)) {
			debugmask |= (unsigned int)strtol(&s[9], NULL, 0);
		}
		/* all other is ignored */
		if (feof(f))
			break;
	}
err:
	fclose(f);
	return nr_controller;
}

/**********************************************************************
Signal handler for clean shutdown
***********************************************************************/
static void
termHandler(int sig)
{
	int ret, contr = MI_CONTROL_SHUTDOWN | sig;

	iprint("Terminating on signal %d -- shutdown mISDNcapid\n", sig);

	ret = write(mIControl[1], &contr, sizeof(contr));
	if (ret != sizeof(contr))
		eprint("Error sending shutdown to mainloop after signal %d - %s\n", sig, strerror(errno));
	return;
}

static int add_mainpoll(int fd, pollInfo_t pit)
{
	struct pollfd *newmp;
	struct pollInfo *newinfo;
	int i, n;

	if (mainpoll_max) {
		for (i = 0; i < mainpoll_max; i++) {
			if (mainpoll[i].fd == -1)
				break;
		}
	} else
		i = 0;
	if (i == mainpoll_max) {
		if (mainpoll_max < mainpoll_size)
			mainpoll_max++;
		else if (mainpoll_size < MAINPOLL_LIMIT) {
			if (mainpoll_size)
				n = mainpoll_size * 2;
			else
				n = 4;
			newmp = calloc(n, sizeof(struct pollfd));
			if (!newmp) {
				eprint("no memory for %d mainpoll\n", n);
				return -1;
			}
			newinfo = calloc(n, sizeof(struct pollInfo));
			if (!newinfo) {
				free(newmp);
				eprint("no memory for %d pollinfo\n", n);
				return -1;
			}
			if (mainpoll) {
				memcpy(newmp, mainpoll, mainpoll_size * sizeof(struct pollfd));
				free(mainpoll);
			}
			if (pollinfo) {
				memcpy(newinfo, pollinfo, mainpoll_size * sizeof(struct pollInfo));
				free(pollinfo);
			}
			mainpoll_size = n;
			mainpoll = newmp;
			pollinfo = newinfo;
			mainpoll_max++;
		} else {
			eprint("mainpoll full %d fds\n", mainpoll_size);
			return -1;
		}
	}
	mainpoll[i].fd = fd;
	pollinfo[i].type = pit;
	mainpoll[i].events = POLLIN | POLLPRI;
	return i;
}

static int del_mainpoll(int fd)
{
	int i, j;

	for (i = 0; i < mainpoll_max; i++) {
		if (mainpoll[i].fd == fd) {
			mainpoll[i].events = 0;
			mainpoll[i].revents = 0;
			mainpoll[i].fd = -1;
			j = i;
			switch (pollinfo[i].type) {
			default:
				if (pollinfo[i].data)
					free(pollinfo[i].data);
			case PIT_Application:	/* already freed */
			case PIT_Bchannel:	/* Never freed */
				pollinfo[i].data = NULL;
				pollinfo[i].type = PIT_None;
				break;
			}
			while (mainpoll_max && j == (mainpoll_max - 1) && mainpoll[j].fd == -1) {
				mainpoll_max--;
				j--;
			}
			return i;
		}
	}
	return -1;
}

void clean_all(void)
{
	int i, j;

	for (i = 0; i < mainpoll_max; i++) {
		switch (pollinfo[i].type) {
		case PIT_Control:
			if (mIControl[0] >= 0) {
				close(mIControl[0]);
				mIControl[0] = -1;
			}
			if (mIControl[1] >= 0) {
				close(mIControl[1]);
				mIControl[1] = -1;
			}
			break;
		case PIT_Application:
			ReleaseApplication(pollinfo[i].data);
			pollinfo[i].data = NULL;
			break;
		case PIT_Bchannel:
			ReleaseBchannel(pollinfo[i].data);
			break;
		case PIT_NewConn:
			close(mainpoll[i].fd);
			break;
		case PIT_mISDNmain:
		case PIT_CAPImain:
		case PIT_None:
			break;
		}
		mainpoll[i].fd = -1;
		mainpoll[i].events = 0;
		mainpoll[i].revents = 0;
	}
	mainpoll_max = 0;
	mainpoll_size = 0;
	free(pollinfo);
	pollinfo = NULL;
	free(mainpoll);
	mainpoll = NULL;
	for (i = 0; i < mI_count; i++) {
		if (mI_Controller[i].l3) {
			close_layer3(mI_Controller[i].l3);
			mI_Controller[i].l3 = NULL;
		}
		for (j = 0; j < mI_Controller[i].BImax; j++) {
			ReleaseBchannel(&mI_Controller[i].BInstances[j]);
		}
		free(mI_Controller[i].BInstances);
		mI_Controller[i].BInstances = NULL;
		while (mI_Controller[i].lClist)
			rm_lController(mI_Controller[i].lClist);
	}
	mI_count = 0;
}

struct pController *get_cController(int cNr)
{
	int i;

	for (i = 0; i < mI_count; i++) {
		if (mI_Controller[i].enable && mI_Controller[i].profile.ncontroller == cNr)
			return &mI_Controller[i];
	}
	return NULL;
}

struct pController *get_mController(int mNr)
{
	int i;

	for (i = 0; i < mI_count; i++) {
		if (mI_Controller[i].enable && mI_Controller[i].mNr == mNr)
			return &mI_Controller[i];
	}
	return NULL;
}

struct BInstance *ControllerSelChannel(struct pController *pc, int nr, int proto)
{
	struct BInstance *bi;
	int pmask;

	if (nr >= pc->BImax) {
		wprint("Request for channel number %d but controller %d only has %d channels\n",
		       nr, pc->profile.ncontroller, pc->BImax);
		return NULL;
	}
	if (ISDN_P_B_START <= proto) {
		pmask = 1 << (proto & ISDN_P_B_MASK);
		if (!(pmask & pc->devinfo.Bprotocols)) {
			wprint("Request for channel number %d on controller %d protocol 0x%02x not supported\n",
			       nr, pc->profile.ncontroller, proto);
			return NULL;
		}
	} else {
		pmask = 1 << proto;
		if (!(pmask & pc->devinfo.Dprotocols)) {
			wprint("Request for channel number %d on controller %d protocol 0x%02x not supported\n",
			       nr, pc->profile.ncontroller, proto);
			return NULL;
		}
	}
	bi = pc->BInstances + nr;
	if (bi->usecnt) {
		/* for now only one user allowed - this is not sufficient for X25 */
		wprint("Request for channel number %d on controller %d but channel already in use\n", nr, pc->profile.ncontroller);
		return NULL;
	} else {
		bi->usecnt++;
		bi->proto = proto;
	}
	return bi;
}


static int Create_tty(struct BInstance *bi)
{
	int ret, pmod;

	ret = posix_openpt(O_RDWR | O_NOCTTY);
	if (ret < 0) {
		eprint("Cannot open terminal - %s\n", strerror(errno));
	} else {
		bi->tty = ret;
		ret = grantpt(bi->tty);
		if (ret < 0) {
			eprint("Error on grantpt - %s\n", strerror(errno));
			close(bi->tty);
			bi->tty = -1;
		} else {
			ret = unlockpt(bi->tty);
			if (ret < 0) {
				eprint("Error on unlockpt - %s\n", strerror(errno));
				close(bi->tty);
				bi->tty = -1;
			} else {
				/* packet mode */
				pmod = 1;
				ret = ioctl(bi->tty, TIOCPKT, &pmod);
				if (ret < 0) {
					eprint("Cannot set packet mode - %s\n", strerror(errno));
					close(bi->tty);
					bi->tty = -1;
				}
			}
			
		}
	}
	return  ret;
}

static int recvBchannel(struct BInstance *);


static int recv_tty(struct BInstance *bi)
{
	int ret, maxl;
	struct mc_buf *mc;

	mc = alloc_mc_buf();
	if (!mc)
		return -ENOMEM;
	if (!bi)
		return -EINVAL;
	mc->rp  = mc->rb + 8;
	maxl = MC_RB_SIZE - 8;
	ret = read(bi->tty, mc->rp, maxl);
	if (ret < 0) {
		wprint("Error on reading from tty %d errno %d - %s\n", bi->tty, errno, strerror(errno));
		ret = -errno;
	} else if (ret == 0) {
		/* closed */
		wprint("Read 0 bytes from tty %d\n", bi->tty);
		ret = -ECONNABORTED;
	} else if (ret == maxl) {
		eprint("Message too big %d ctrl %02x (%02x%02x%02x%02x%02x%02x%02x%02x)\n",
		       ret, mc->rp[0], mc->rp[1], mc->rp[2], mc->rp[3], mc->rp[4], mc->rp[5], mc->rp[6], mc->rp[7], mc->rp[8]);
		ret = -EMSGSIZE;
	}
	if (ret > 0) {
		mc->len = ret;
		/* Fake length of DATA_B3 REQ to pass offset check */
		capimsg_setu16(mc->rb, 0, 22);
		mc->cmsg.Command = CAPI_DATA_TTY;
		mc->cmsg.Subcommand = CAPI_REQ;
		ret = bi->from_up(bi, mc);
	}

	if (ret != 0)		/* if message is not queued or freed */
		free_mc_buf(mc);
	return ret;
}

static void *BCthread(void *arg)
{
	struct BInstance *bi = arg;
	unsigned char cmd;
	int ret, i;

	bi->running = 1;
	while (bi->running) {
		ret = poll(bi->pfd, bi->pcnt, -1);
		if (ret < 0) {
			wprint("Bchannel%d Error on poll - %s\n", bi->nr, strerror(errno));
			continue;
		}
		for (i = 1; i < bi->pcnt; i++) {
			if (bi->pfd[i].revents & POLLIN) {
				switch(i) {
				case 1:
					ret = recvBchannel(bi);
					break;
				case 2:
					ret = recv_tty(bi);
					break;
				default:
					wprint("Bchannel%d poll idx %d not handled\n", bi->nr, i);
				}
			}
		}

		if (bi->pfd[0].revents & POLLIN) {
			ret = read(bi->pfd[0].fd, &cmd, 1);
			if (cmd == 42) {
				bi->running = 0;
			}
		}
	}
	return NULL;
}

static int CreateBchannelThread(struct BInstance *bi, int pcnt)
{
	int ret, i;

	ret = pipe(bi->cpipe);
	if (ret) {
		eprint("error - %s\n", strerror(errno));
		return ret;
	}
	ret = fcntl(bi->cpipe[0], F_SETFL, O_NONBLOCK);
	if (ret) {
		eprint("error - %s\n", strerror(errno));
		return ret;
	}
	ret = fcntl(bi->cpipe[1], F_SETFL, O_NONBLOCK);
	if (ret) {
		eprint("error - %s\n", strerror(errno));
		return ret;
	}
	bi->pfd[0].fd = bi->cpipe[0];
	bi->pfd[0].events = POLLIN | POLLPRI;
	bi->pfd[1].fd = bi->fd;
	bi->pfd[1].events = POLLIN | POLLPRI;
	bi->pcnt = pcnt;
	ret = pthread_create(&bi->tid, NULL, BCthread, bi);
	if (ret) {
		eprint("Cannot create thread error - %s\n", strerror(errno));
		close(bi->cpipe[0]);
		close(bi->cpipe[1]);
		bi->cpipe[0] = -1;
		bi->cpipe[1] = -1;
		bi->pfd[0].fd = -1;
		for (i = 1; i < bi->pcnt; i++)
			bi->pfd[i].fd = -1;
	} else
		iprint("Created Bchannel tread %d\n", (int)bi->tid);
	return ret;
}

static int StopBchannelThread(struct BInstance *bi)
{
	int ret, i;
	unsigned char cmd;

	if (bi->running) {
		cmd = 42;
		if (bi->waiting)
			sem_post(&bi->wait);
		ret = write(bi->cpipe[1], &cmd, 1);
		if (ret != 1)
			wprint("Error on write control ret=%d - %s\n", ret, strerror(errno));
		ret = pthread_join(bi->tid, NULL);
		if (ret < 0)
			wprint("Error on pthread_join - %s\n", strerror(errno));
		else
			iprint("Thread %d joined\n", (int)bi->tid);
		close(bi->cpipe[0]);
		close(bi->cpipe[1]);
		bi->pfd[0].fd = -1;
		for (i = 1; i < bi->pcnt; i++)
			bi->pfd[i].fd = -1;
		bi->pcnt = 0;
		bi->cpipe[0] = -1;
		bi->cpipe[1] = -1;
	}
	return 0;
}

static int dummy_btrans(struct BInstance *bi, struct mc_buf *mc)
{
	struct mISDNhead *hh = (struct mISDNhead *)mc->rb;

	wprint("Controller%d ch%d: Got %s id %x - but %s called\n", bi->pc->profile.ncontroller, bi->nr,
		_mi_msg_type2str(hh->prim), hh->id, __func__);
	return -EINVAL;
}

int OpenBInstance(struct BInstance *bi, struct lPLCI *lp, enum BType btype)
{
	int sk;
	int ret;
	struct sockaddr_mISDN addr;

	sk = socket(PF_ISDN, SOCK_DGRAM, bi->proto);
	if (sk < 0) {
		wprint("Cannot open socket for BInstance %d on controller %d protocol 0x%02x - %s\n",
		       bi->nr, bi->pc->profile.ncontroller, bi->proto, strerror(errno));
		return -errno;
	}

	ret = fcntl(sk, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		ret = -errno;
		wprint("fcntl error %s\n", strerror(errno));
		close(sk);
		return ret;
	}

	switch (btype) {
	case BType_Direct:
		bi->from_down = recvBdirect;
		bi->from_up = ncciB3Data;
		break;
#ifdef USE_SOFTFAX
	case BType_Fax:
		bi->from_down = FaxRecvBData;
		bi->from_up = FaxB3Message;
		break;
#endif
	case BType_tty:
		bi->from_down = recvBdirect;
		bi->from_up = ncciB3Data;
		break;
	default:
		eprint("Error unnkown BType %d\n",  btype);
		close(sk);
		return -EINVAL;
	}
	bi->type = btype;

	addr.family = AF_ISDN;
	addr.dev = bi->pc->mNr;
	addr.channel = bi->nr;
	/* not used but make valgrind happy */
	addr.sapi = 0;
	addr.tei = 0;

	ret = bind(sk, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		ret = -errno;
		wprint("Cannot bind socket for BInstance %d on controller %d (mISDN nr %d) protocol 0x%02x - %s\n",
		       bi->nr, bi->pc->profile.ncontroller, bi->pc->mNr, bi->proto, strerror(errno));
		close(sk);
		bi->from_down = dummy_btrans;
		bi->from_up = dummy_btrans;
	} else {
		bi->fd = sk;
		bi->lp = lp;
		if (btype == BType_Direct) {
			ret = add_mainpoll(sk, PIT_Bchannel);
			if (ret < 0) {
				eprint("Error while adding mIsock to mainpoll (mainpoll_max %d)\n", mainpoll_max);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
			} else {
				dprint(MIDEBUG_CONTROLLER, "Controller%d: Bchannel %d socket %d added to poll idx %d\n",
				       bi->pc->profile.ncontroller, bi->nr, sk, ret);
				pollinfo[ret].data = bi;
				ret = 0;
				bi->UpId = 0;
				bi->DownId = 0;
			}
		} else if (btype == BType_Fax) {
			ret = CreateBchannelThread(bi, 2);
			if (ret < 0) {
				eprint("Error while creating B%d-channel thread\n", bi->nr);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
			} else {
				ret = 0;
				bi->UpId = 0;
				bi->DownId = 0;
			}
		} else if (btype == BType_tty) {
			ret = Create_tty(bi);
			if (ret < 0) {
				eprint("Error while creating B%d-channel tty\n", bi->nr);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
				return ret;
			} else {
				bi->pfd[2].fd = bi->tty;
				bi->pfd[2].events = POLLIN | POLLPRI;
				ret = CreateBchannelThread(bi, 3);
			}
			if (ret < 0) {
				eprint("Error while creating B%d-channel thread\n", bi->nr);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
			} else {
				ret = 0;
				bi->UpId = 0;
				bi->DownId = 0;
			}
		}
	}
	return ret;
}

int CloseBInstance(struct BInstance *bi)
{
	int ret = 0;

	if (bi->usecnt) {
		switch (bi->type) {
		case BType_Direct:
			if (bi->fd >= 0)
				del_mainpoll(bi->fd);
			break;
#ifdef USE_SOFTFAX
		case  BType_Fax:
			StopBchannelThread(bi);
			break;
#endif
		case BType_tty:
			StopBchannelThread(bi);
			if (bi->tty > -1)
				close(bi->tty);
			bi->tty = -1;
		default:
			break;
		}
		if (bi->fd >= 0)
			close(bi->fd);
		bi->fd = -1;
		bi->proto = ISDN_P_NONE;
		bi->usecnt--;
		if (bi->b3data && bi->lp)
			B3ReleaseLink(bi->lp, bi);
		bi->b3data = NULL;
		bi->lp = NULL;
		bi->type = BType_None;
		bi->from_down = dummy_btrans;
		bi->from_up = dummy_btrans;
	} else {
		wprint("BInstance %d not active\n", bi->nr);
		ret = -1;
	}
	return ret;
};

static int recvBchannel(struct BInstance *bi)
{
	int ret;
	struct mc_buf *mc;

	mc = alloc_mc_buf();
	if (!mc)
		return -ENOMEM;
	if (!bi) {
		free_mc_buf(mc);
		eprint("recvBchannel: but no BInstance assigned\n");
		return -EINVAL;
	}
	ret = recv(bi->fd, mc->rb, MC_RB_SIZE, MSG_DONTWAIT);
	if (ret < 0) {
		wprint("Error on reading from %d errno %d - %s\n", bi->fd, errno, strerror(errno));
		ret = -errno;
	} else if (ret == 0) {
		/* closed */
		wprint("Nothing read - connection closed ?\n");
		ret = -ECONNABORTED;
	} else if (ret < 8) {
		eprint("Short message read len %d (%02x%02x%02x%02x%02x%02x%02x%02x)\n",
		       ret, mc->rb[0], mc->rb[1], mc->rb[2], mc->rb[3], mc->rb[4], mc->rb[5], mc->rb[6], mc->rb[7]);
		ret = -EBADMSG;
	} else if (ret == MC_RB_SIZE) {
		eprint("Message too big %d (%02x%02x%02x%02x%02x%02x%02x%02x)\n",
		       ret, mc->rb[0], mc->rb[1], mc->rb[2], mc->rb[3], mc->rb[4], mc->rb[5], mc->rb[6], mc->rb[7]);
		ret = -EMSGSIZE;
	}
	if (ret > 0) {
		mc->len = ret;
		ret = bi->from_down(bi, mc);
	}

	if (ret != 0)		/* if message is not queued or freed */
		free_mc_buf(mc);
	return ret;
}

int ReleaseBchannel(struct BInstance *bi)
{
	if (!bi)
		return -1;
	if (bi->fd >= 0) {
		del_mainpoll(bi->fd);
		close(bi->fd);
		bi->fd = -1;
	}
	if (bi->tty >= 0) {
		close(bi->tty);
		bi->tty = -1;
	}
	return 0;
}

static int l3_callback(struct mlayer3 *l3, unsigned int cmd, unsigned int pid, struct l3_msg *l3m)
{
	struct pController *pc = l3->priv;
	struct mPLCI *plci;
	int ret = 0;

	dprint(MIDEBUG_CONTROLLER, "Controller %d - got %s (%x) from layer3 pid(%x) msg(%p)\n",
	       pc->profile.ncontroller, _mi_msg_type2str(cmd), cmd, pid, l3m);

	plci = getPLCI4pid(pc, pid);

	switch (cmd) {
	case MT_SETUP:
		if (plci) {
			iprint("Controller %d - got %s but pid(%x) already in use\n",
			       pc->profile.ncontroller, _mi_msg_type2str(cmd), pid);
			break;
		}
		plci = new_mPLCI(pc, pid, NULL);
		if (!plci) {
			wprint("Controller %d - got %s but could not allocate new PLCI\n",
			       pc->profile.ncontroller, _mi_msg_type2str(cmd));
			ret = -ENOMEM;
		}
		break;
	case MT_SETUP_ACKNOWLEDGE:
	case MT_CALL_PROCEEDING:
	case MT_ALERTING:
	case MT_PROGRESS:
	case MT_CONNECT:
	case MT_CONNECT_ACKNOWLEDGE:
	case MT_DISCONNECT:
	case MT_RELEASE:
	case MT_RELEASE_COMPLETE:
	case MT_HOLD:
	case MT_HOLD_ACKNOWLEDGE:
	case MT_HOLD_REJECT:
	case MT_RETRIEVE:
	case MT_RETRIEVE_ACKNOWLEDGE:
	case MT_RETRIEVE_REJECT:
	case MT_SUSPEND_ACKNOWLEDGE:
	case MT_SUSPEND_REJECT:
	case MT_RESUME_ACKNOWLEDGE:
	case MT_RESUME_REJECT:
	case MT_NOTIFY:
		if (!plci)
			wprint("Controller %d - got %s but no PLCI found\n", pc->profile.ncontroller, _mi_msg_type2str(cmd));
		break;
	case MT_FREE:
		if (!plci)
			wprint("Controller %d - got %s but no PLCI found\n", pc->profile.ncontroller, _mi_msg_type2str(cmd));
		else
			plci->pid = MISDN_PID_NONE;
		break;
	case MPH_ACTIVATE_IND:
	case MT_L2ESTABLISH:
	case MT_L2RELEASE:
	case MT_L2IDLE:
		break;
	case MT_TIMEOUT:
		iprint("Controller %d - got %s from layer3 pid(%x) msg(%p) plci(%04x)\n",
		       pc->profile.ncontroller, _mi_msg_type2str(cmd), pid, l3m, plci ? plci->plci : 0xffff);
		break;
	case MT_ERROR:
		wprint("Controller %d - got %s from layer3 pid(%x) msg(%p) plci(%04x)\n",
		       pc->profile.ncontroller, _mi_msg_type2str(cmd), pid, l3m, plci ? plci->plci : 0xffff);
		break;
	default:
		wprint("Controller %d - got %s (%x) from layer3 pid(%x) msg(%p) plci(%04x) - not handled\n",
		       pc->profile.ncontroller, _mi_msg_type2str(cmd), cmd, pid, l3m, plci ? plci->plci : 0xffff);
		ret = -EINVAL;
	}
	if (!ret) {
		if (plci)
			ret = plci_l3l4(plci, cmd, l3m);
		else
			ret = 1; /* message need to be freed */
	}
	return ret;
}

int OpenLayer3(struct pController *pc)
{
	int ret = 0;

	if (!pc->l3) {
		pc->l3 = open_layer3(pc->mNr, pc->L3Proto, pc->L3Flags, l3_callback, pc);
		if (!pc->l3) {
			eprint("Cannot open L3 for controller %d L3 protocol %x L3 flags %x\n", pc->mNr, pc->L3Proto,
				pc->L3Flags);
			ret = -EINVAL;
		} else
			dprint(MIDEBUG_CONTROLLER, "Controller %d l3 open for protocol %x L3 flags %x\n", pc->mNr,
			       pc->L3Proto, pc->L3Flags);
	}
	return ret;
}

int ListenController(struct pController *pc)
{
	struct lController *lc;
	uint32_t InfoMask = 0, CIPMask = 0, CIPMask2 = 0;
	int ret = 0;

	lc = pc->lClist;
	while (lc) {
		InfoMask |= lc->InfoMask;
		CIPMask |= lc->CIPmask;
		CIPMask2 |= lc->CIPmask2;
		lc = lc->nextA;
	}
	dprint(MIDEBUG_CONTROLLER, "Controller %d change InfoMask %08x -> %08x\n", pc->profile.ncontroller, pc->InfoMask, InfoMask);
	dprint(MIDEBUG_CONTROLLER, "Controller %d change CIPMask  %08x -> %08x\n", pc->profile.ncontroller, pc->CIPmask, CIPMask);
	dprint(MIDEBUG_CONTROLLER, "Controller %d change CIPMask2 %08x -> %08x\n", pc->profile.ncontroller, pc->CIPmask2, CIPMask2);
	pc->InfoMask = InfoMask;
	pc->CIPmask = CIPMask;
	pc->CIPmask2 = CIPMask2;
	if ((pc->CIPmask || pc->InfoMask) && !pc->l3) {
		ret = OpenLayer3(pc);
	}
	return ret;
}

void capi_dump_shared(void);

static void get_profile(int fd, struct mc_buf *mc)
{
	int ret, cnt, i, contr;
	struct pController *pc;

	contr = CAPIMSG_U16(mc->rb, 8);
	memset(&mc->rb[8], 0, 66);
	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	CAPIMSG_SETLEN(mc->rb, 74);
	if (mc->len < 10 || contr < 0) {
		capimsg_setu16(mc->rb, 8, MIC_INFO_CODING_ERROR);
	} else if (contr == 0) {
		cnt = 0;
		for (i = 0; i < mI_count; i++) {
			if (mI_Controller[i].enable)
				cnt++;
		}
		capimsg_setu16(mc->rb, 8, CapiNoError);
		capimsg_setu16(mc->rb, 10, cnt);
	} else {
		pc = get_cController(contr);
		if (!pc) {
			capimsg_setu16(mc->rb, 8, 0x2002);	/* Illegal controller */
		} else {
			capimsg_setu16(mc->rb, 8, CapiNoError);
			capimsg_setu16(mc->rb, 10, contr);
			capimsg_setu16(mc->rb, 12, pc->profile.nbchannel);
			capimsg_setu32(mc->rb, 14, pc->profile.goptions);
			capimsg_setu32(mc->rb, 18, pc->profile.support1);
			capimsg_setu32(mc->rb, 22, pc->profile.support2);
			capimsg_setu32(mc->rb, 26, pc->profile.support3);
			/* TODO reserved, manu */
		}
	}
	ret = send(fd, mc->rb, 74, 0);
	if (ret != 74)
		eprint("error send %d/%d  - %s\n", ret, 74, strerror(errno));
	capi_dump_shared();
}

static void mIcapi_register(int fd, int idx, struct mc_buf *mc)
{
	int ret;
	uint16_t aid;
	struct mApplication *appl;
	uint32_t b3c, b3b, b3s;

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	aid = CAPIMSG_APPID(mc->rb);
	if (mc->len == 20) {
		iprint("register application id %d fd=%d\n", aid, fd);
		b3c = CAPIMSG_U32(mc->rb, 8);
		b3b = CAPIMSG_U32(mc->rb, 12);
		b3s = CAPIMSG_U32(mc->rb, 16);
		appl = RegisterApplication(aid, b3c, b3b, b3s);
		if (appl) {
			capimsg_setu16(mc->rb, 8, CapiNoError);
			pollinfo[idx].type = PIT_Application;
			pollinfo[idx].data = appl;
			appl->fd = fd;
		} else {
			eprint("register application id %d fd %d failed\n", aid, fd);
			capimsg_setu16(mc->rb, 8, CapiMsgOSResourceErr);
		}
	} else
		capimsg_setu16(mc->rb, 8, MIC_INFO_CODING_ERROR);
	ret = send(fd, mc->rb, 10, 0);
	if (ret != 10)
		eprint("error send %d/%d  - %s\n", ret, 10, strerror(errno));
}

static void mIcapi_release(int fd, struct mApplication *appl, struct mc_buf *mc)
{
	int ret, idx;
	uint16_t aid, info = CapiIllAppNr;

	aid = CAPIMSG_APPID(mc->rb);
	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	iprint("Unregister application %d (%d)\n", aid, appl ? appl->AppId : -1);
	if (appl) {
		if (aid == appl->AppId) {
			idx = del_mainpoll(appl->fd);
			if (idx < 0)
				wprint("Application %d not found in poll array\n", aid);
			else
				pollinfo[idx].data = NULL;
			ReleaseApplication(appl);
			info = CapiNoError;
		}
	}
	capimsg_setu16(mc->rb, 8, info);
	ret = send(fd, mc->rb, 10, 0);
	if (ret != 10)
		eprint("error send %d/%d  - %s\n", ret, 10, strerror(errno));
	if (info == CapiNoError)
		close(fd);
}

static void get_serial_number(int fd, struct mc_buf *mc)
{
	int ret, contr = CAPIMSG_U16(mc->rb, 8);

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	capimsg_setu16(mc->rb, 8, CapiNoError);
	/* only fake for now since we do not have serial numbers on most HW */
	sprintf((char *)&mc->rb[10], "%07d", contr);
	ret = send(fd, mc->rb, 18, 0);
	if (ret != 18)
		eprint("error send %d/%d  - %s\n", ret, 18, strerror(errno));
}

static void get_capi_version(int fd, struct mc_buf *mc)
{
	/* all the same for all cards */
	int ret;

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	capimsg_setu16(mc->rb, 8, CapiNoError);
	capimsg_setu32(mc->rb, 10, 2);	/* major */
	capimsg_setu32(mc->rb, 14, 0);	/* minor */
	capimsg_setu32(mc->rb, 18, 0);	/* manu major */
	capimsg_setu32(mc->rb, 22, 1);	/* manu minor */
	ret = send(fd, mc->rb, 26, 0);
	if (ret != 26)
		eprint("error send %d/%d  - %s\n", ret, 26, strerror(errno));
}

static void get_manufacturer(int fd, struct mc_buf *mc)
{
	/* We us a generic mISDN for now */
	int ret;

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	memset(&mc->rb[8], 0, 66);
	capimsg_setu16(mc->rb, 8, CapiNoError);
	sprintf((char *)&mc->rb[10], "mISDN");
	ret = send(fd, mc->rb, 74, 0);
	if (ret != 74)
		eprint("error send %d/%d  - %s\n", ret, 74, strerror(errno));
}

static void misdn_manufacturer_req(int fd, struct mc_buf *mc)
{
	/* nothing implemented, only return no error */
	int ret;

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	capimsg_setu16(mc->rb, 8, CapiNoError);
	ret = send(fd, mc->rb, 10, 0);
	if (ret != 10)
		eprint("error send %d/%d  - %s\n", ret, 10, strerror(errno));
}

static void mIcapi_userflag(int fd, int idx, struct mc_buf *mc)
{
	int ret;
	struct mApplication *appl = pollinfo[idx].data;
	uint32_t sf, cf, uf = 0xffffffff;

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	capimsg_setu16(mc->rb, 0, 12);
	if (appl) {
		uf = appl->UserFlags;
		sf = CAPIMSG_U32(mc->rb, 8);
		cf = CAPIMSG_U32(mc->rb, 12);
		if (cf)
			uf &= ~cf;
		if (sf)
			uf |= sf;
		iprint("UserFlags old=%x new=%x\n", appl->UserFlags, uf);
		appl->UserFlags = uf;
	}
	capimsg_setu32(mc->rb, 8, uf);
	ret = send(fd, mc->rb, 12, 0);
	if (ret != 12)
		eprint("error send %d/%d  - %s\n", ret, 12, strerror(errno));
}

static void mIcapi_ttyname(int fd, int idx, struct mc_buf *mc)
{
	int ret, ml = 0, l = 0;
	struct mApplication *appl = pollinfo[idx].data;
	struct lPLCI *lp;
	uint32_t ncci = 0;
	char name[80];

	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	if (appl) {
		ncci = CAPIMSG_U32(mc->rb, 8);
		ml = CAPIMSG_U32(mc->rb, 12);
		if (appl->UserFlags & CAPIFLAG_HIGHJACKING) {
			lp = get_lPLCI4plci(appl, ncci);
			if (lp->BIlink && lp->BIlink->tty > -1) {
				ret = ptsname_r(lp->BIlink->tty, name, 80);
				if (ret)
					eprint("NCCI %06x: error to get ptsname for %d - %s\n",
						ncci, lp->BIlink->tty, strerror(errno));
				else
					l = strlen(name);
			} else
				eprint("NCCI %06x: do not find lPLCI for NCCI\n", ncci);
		}
	}
	if (l >= ml)
		l = 0;
	if (l) {
		iprint("NCCI %06x: ttyname set to %s\n", ncci, name);
		memcpy(&mc->rb[8], name, l);
	} else
		wprint("NCCI %06x: ttyname requested but not available\n", ncci);
	mc->rb[8 + l] = 0;
	capimsg_setu16(mc->rb, 0, l + 8);
	ret = send(fd, mc->rb, l + 8, 0);
	if (ret != l + 8)
		eprint("error send %d/%d  - %s\n", ret, 12, strerror(errno));
}

static int main_recv(int fd, int idx)
{
	int ret, len, cmd, dl;
	struct mc_buf *mc;

	mc = alloc_mc_buf();
	if (!mc)
		return -ENOMEM;
	ret = recv(fd, mc->rb, MC_RB_SIZE, MSG_DONTWAIT);
	if (ret < 0) {
		wprint("Error on reading from %d errno %d - %s\n", fd, errno, strerror(errno));
		ret = -errno;
	} else if (ret == 0) {
		/* closed */
		ret = -ECONNABORTED;
	} else if (ret < 8) {
		eprint("Short message read len %d (%02x%02x%02x%02x%02x%02x%02x%02x)\n",
		       ret, mc->rb[0], mc->rb[1], mc->rb[2], mc->rb[3], mc->rb[4], mc->rb[5], mc->rb[6], mc->rb[7]);
		ret = -EBADMSG;
	} else if (ret == MC_RB_SIZE) {
		eprint("Message too big %d (%02x%02x%02x%02x%02x%02x%02x%02x)\n",
		       ret, mc->rb[0], mc->rb[1], mc->rb[2], mc->rb[3], mc->rb[4], mc->rb[5], mc->rb[6], mc->rb[7]);
		ret = -EMSGSIZE;
	}
	if (ret < 0)
		goto end;

	len = CAPIMSG_LEN(mc->rb);
	cmd = CAPIMSG_CMD(mc->rb);
	if (cmd != CAPI_DATA_B3_REQ) {
		if (len != ret) {
			wprint("Msg len error on %04x read %d but indicated %d bytes\n", cmd, ret, len);
			if (ret < len)
				mc->len = ret;
		} else
			mc->len = len;
	} else {
		dl = CAPIMSG_DATALEN(mc->rb);
		if ((len + dl) != ret) {
			wprint("Msg len error on DATA_B3_REQ msg len %d data len %d but read %d\n", len, dl, ret);
		}
		mc->len = ret;
	}
	switch (cmd) {
	case MIC_GET_PROFILE_REQ:
		get_profile(fd, mc);
		break;
	case MIC_REGISTER_REQ:
		mIcapi_register(fd, idx, mc);
		break;
	case MIC_RELEASE_REQ:
		mIcapi_release(fd, pollinfo[idx].data, mc);
		break;
	case MIC_SERIAL_NUMBER_REQ:
		get_serial_number(fd, mc);
		break;
	case MIC_VERSION_REQ:
		get_capi_version(fd, mc);
		break;
	case MIC_GET_MANUFACTURER_REQ:
		get_manufacturer(fd, mc);
		break;
	case MIC_MANUFACTURER_REQ:
		misdn_manufacturer_req(fd, mc);
		break;
	case MIC_USERFLAG_REQ:
		mIcapi_userflag(fd, idx, mc);
		break;
	case MIC_TTYNAME_REQ:
		mIcapi_ttyname(fd, idx, mc);
		break;
	default:
		if (pollinfo[idx].type == PIT_Application)
			ret = PutMessageApplication(pollinfo[idx].data, mc);
		else {
			wprint("CMD %x for fd=%d type %d not handled\n", cmd, fd, pollinfo[idx].type);
		}
		break;
	}
end:
	if (ret != 0)		/* if message is not queued or freed */
		free_mc_buf(mc);
	return ret;
}

int main_loop(void)
{
	int res, ret, i, idx, error = -1;
	int running = 1;
	int fd;
	int nconn = -1;
	struct sockaddr_un caddr;
	char buf[4096];
	socklen_t alen;

	ret = pipe(mIControl);
	if (ret) {
		eprint("error setup MasterControl pipe - %s\n", strerror(errno));
		return errno;
	}
	ret = add_mainpoll(mIControl[0], PIT_Control);
	if (ret < 0) {
		eprint("Error while adding mIControl to mainpoll (mainpoll_max %d)\n", mainpoll_max);
		return -1;
	} else
		iprint("mIControl added to idx %d\n", ret);
	ret = add_mainpoll(mIsock, PIT_mISDNmain);
	if (ret < 0) {
		eprint("Error while adding mIsock to mainpoll (mainpoll_max %d)\n", mainpoll_max);
		return -1;
	} else
		iprint("mIsock added to idx %d\n", ret);
	ret = add_mainpoll(mCsock, PIT_CAPImain);
	if (ret < 0) {
		eprint("Error while adding mCsock to mainpoll (mainpoll_max %d)\n", mainpoll_max);
		return -1;
	} else
		iprint("mCsock added to idx %d\n", ret);

	while (running) {
		ret = poll(mainpoll, mainpoll_max, -1);
		if (ret < 0) {
			if (errno == EINTR)
				iprint("Received signal - continue\n");
			else {
				wprint("Error on poll errno=%d - %s\n", errno, strerror(errno));
				error = -errno;
			}
			continue;
		}
		for (i = 0; i < mainpoll_max; i++) {
			if (ret && mainpoll[i].revents) {
				dprint(MIDEBUG_POLL, "Poll return %d for idx %d ev %x fd %d\n", ret,
				       i, mainpoll[i].revents, mainpoll[i].fd);
				switch (pollinfo[i].type) {
				case PIT_Bchannel:
					if (mainpoll[i].revents & POLLIN) {
						res = recvBchannel(pollinfo[i].data);
					} else if (mainpoll[i].revents == POLLHUP) {
						dprint(MIDEBUG_POLL, "Bchannel socket %d closed\n", mainpoll[i].fd);
						ReleaseBchannel(pollinfo[i].data);
					}
					break;
				case PIT_CAPImain:	/* new connect */
					if (mainpoll[i].revents & POLLIN) {
						caddr.sun_family = AF_UNIX;
						caddr.sun_path[0] = 0;
						alen = sizeof(caddr);
						nconn = accept(mCsock, (struct sockaddr *)&caddr, &alen);
						if (nconn < 0) {
							eprint("Error on accept - %s\n", strerror(errno));
						} else {
							dprint(MIDEBUG_POLL, "New connection on capisocket\n");
							idx = add_mainpoll(nconn, PIT_NewConn);
							if (idx < 0)
								eprint("Cannot add fd=%d to mainpoll\n", nconn);
							else
								dprint(MIDEBUG_POLL, "nconn added to idx %d\n", idx);
						}
					}
					break;
				case PIT_mISDNmain:
					res = read(mIsock, buf, 4096);
					dprint(MIDEBUG_POLL, "Read %d from mIsock (%d)\n", res, mIsock);
					break;
				case PIT_Application:
					if (mainpoll[i].revents == POLLHUP) {
						dprint(MIDEBUG_POLL, "socket %d closed\n", mainpoll[i].fd);
						ReleaseApplication(pollinfo[i].data);
						res = del_mainpoll(mainpoll[i].fd);
						break;
					}
					/* no break to handle POLLIN */
				case PIT_NewConn:
					if (mainpoll[i].revents == POLLHUP) {
						dprint(MIDEBUG_POLL, "socket %d closed\n", mainpoll[i].fd);
						close(mainpoll[i].fd);
						res = del_mainpoll(mainpoll[i].fd);
						if (res < 0) {
							eprint("Cannot delete fd=%d from mainpoll result %d\n",
							       mainpoll[i].fd, res);
						}
					}
					res = main_recv(mainpoll[i].fd, i);
					if (res == -ECONNABORTED || res == -ECONNRESET) {
						if (pollinfo[i].type == PIT_Application)
							ReleaseApplication(pollinfo[i].data);
						fd = mainpoll[i].fd;
						dprint(MIDEBUG_POLL, "read 0 socket %d closed\n", fd);
						close(mainpoll[i].fd);
						res = del_mainpoll(mainpoll[i].fd);
						if (res < 0) {
							eprint("Cannot delete fd=%d from mainpoll result %d\n", fd, res);
						} else
							dprint(MIDEBUG_POLL, "Deleted fd=%d from mainpoll\n", fd);
					}
					break;
				case PIT_Control:
					res = read(mainpoll[i].fd, &error, sizeof(error));
					if (res == sizeof(error))
						iprint("Pollevent for MasterControl read %x (%d bytes)\n", error, res);
					else
						eprint("Pollevent for MasterControl read error - %s\n", strerror(errno));
					running = 0;
					error = 0;
					break;
				default:
					wprint("Unexpected poll event %x on fd %d type %d\n", mainpoll[i].revents,
					       mainpoll[i].fd, pollinfo[i].type);
					break;
				}
				ret--;
			}
			if (ret == 0)
				break;
		}
	}
	clean_all();
	return error;
}

static int my_lib_debug(const char *file, int line, const char *func, int level, const char *fmt, va_list va)
{
	int ret, l;
	char date[64], log[1024];
	struct tm tm;
	struct timeval tv;

	ret = vsnprintf(log, 256, fmt, va);
	l = gettimeofday(&tv, NULL);
	if (tv.tv_usec > 999999) {
		tv.tv_sec++;
		tv.tv_usec -= 1000000;
	}
	localtime_r(&tv.tv_sec, &tm);
	l = strftime(date, sizeof(date), "%b %e %T", &tm);
	snprintf(&date[l], sizeof(date) - l, ".%06d", (int)tv.tv_usec);

	if (DebugFile) {
		fprintf(DebugFile, "%s %20s:%4d %22s():%s", date, file, line, func, log);
		fflush(DebugFile);
	} else if (level > MISDN_LIBDEBUG_WARN)
		fprintf(stdout, "%s %20s:%4d %22s():%s", date, file, line, func, log);
	else
		fprintf(stderr, "%s %20s:%4d %22s():%s", date, file, line, func, log);
	return ret;
}

static int my_capilib_dbg(const char *file, int line, const char *func, const char *fmt, va_list va)
{
	return my_lib_debug(file, line, func, 1, fmt, va);
}

static struct mi_ext_fn_s l3dbg = {
	.prt_debug = my_lib_debug,
};

static struct sigaction mysig_act;

int main(int argc, char *argv[])
{
	int ret, i, j, nb, c, exitcode = 1, ver, libdebug;
	struct sockaddr_un mcaddr;
	struct pController *pc;

	KeepTemporaryFiles = 0;
	config_file = def_config;
	ret = opt_parse(argc, argv);
	if (ret)
		exit(1);

	if (DebugFileName) {
		DebugFile = fopen(DebugFileName, "w");
		if (!DebugFile) {
			fprintf(stderr, "Cannot open %s - %s\n", DebugFileName, strerror(errno));
			exit(1);
		}
	}
	libdebug = (debugmask & 0xff) << 24;
	if (debugmask & 0x100)
		libdebug |= 0xfffff;

	register_dbg_vprintf(my_capilib_dbg);
	ver = init_layer3(4, &l3dbg);
	mISDN_set_debug_level(libdebug);
	iprint("Init mISDN lib version %x, debug = %x (%x)\n", ver, debugmask, libdebug);

	mc_buffer_init();

	snprintf(_TempDirectory, 80, "%s/mISDNd_XXXXXX", MISDND_TEMP_DIR);
	TempDirectory = mkdtemp(_TempDirectory);
	if (!TempDirectory) {
		fprintf(stderr, "Cannot create temporary directory %s - %s\n", _TempDirectory, strerror(errno));
		return 1;
	}
	/* open mISDN */
	/* test if /dev/mISDNtimer is accessible */
	ret = open("/dev/mISDNtimer", O_RDWR);
	if (ret < 0) {
		fprintf(stderr, "Cannot access /dev/mISDNtimer - %s\n", strerror(errno));
		return 1;
	}
	close(ret);
	
	mIsock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (mIsock < 0) {
		fprintf(stderr, "mISDNv2 not installed - %s\n", strerror(errno));
		return 1;
	}

	/* get number of stacks */
	ret = ioctl(mIsock, IMGETCOUNT, &mI_count);
	if (ret < 0) {
		fprintf(stderr, "mISDNv2 IMGETCOUNT error - %s\n", strerror(errno));
		goto errout;
	}
	if (mI_count < 1) {
		fprintf(stderr, "mISDNv2 no controllers found\n");
		goto errout;
	}
	mI_Controller = calloc(mI_count, sizeof(*mI_Controller));
	if (!mI_Controller) {
		fprintf(stderr, "no memory to allocate %d controller struct (a %zd)\n", mI_count, sizeof(*mI_Controller));
		goto errout;
	}
	for (i = 0; i < mI_count; i++) {
		pc = &mI_Controller[i];
		pc->mNr = i;
		pc->devinfo.id = i;
		pc->enable = 1;	/* default all controllers are enabled */
		pc->L3Proto = L3_PROTOCOL_DSS1_USER;
		pc->L3Flags = 0;
		ret = ioctl(mIsock, IMGETDEVINFO, &pc->devinfo);
		if (ret < 0) {
			fprintf(stderr, "mISDNv2 IMGETDEVINFO error controller %d - %s\n", i + 1, strerror(errno));
			goto errout;
		}
		c = 0;
		nb = 0;
		while (c <= MISDN_MAX_CHANNEL + 1) {
			if (c <= MISDN_MAX_CHANNEL && test_channelmap(c, pc->devinfo.channelmap)) {
				nb++;
				pc->BImax = c;
			}
			c++;
		}
		pc->BImax++;
		pc->BInstances = calloc(pc->BImax, sizeof(*pc->BInstances));
		if (!pc->BInstances) {
			fprintf(stderr, "no memory to allocate %d Bchannel instances for controller %d\n", pc->BImax, i + 1);
			goto errout;
		}
		for (j = 0; j < pc->BImax; j++) {
			pc->BInstances[j].nr = j;
			pc->BInstances[j].pc = pc;
			pc->BInstances[j].fd = -1;
			pc->BInstances[j].tty = -1;
			sem_init(&pc->BInstances[j].wait, 0, 0);
		}
		pc->profile.ncontroller = i + 1;
		pc->profile.nbchannel = nb;
		pc->profile.goptions = 1;	/* internal controller */
		pc->profile.support1 = 0;
		pc->profile.support2 = 0;
		pc->profile.support3 = 0x01;
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_RAW & ISDN_P_B_MASK))) {
			pc->profile.support1 |= 0x02;
			pc->profile.support2 |= 0x02;
#ifdef USE_SOFTFAX
			pc->profile.support1 |= 0x10;
			pc->profile.support2 |= 0x10;
			pc->profile.support3 |= 0x30;
#endif
		}
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_HDLC & ISDN_P_B_MASK))) {
			pc->profile.support1 |= 0x01;
		}
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_X75SLP & ISDN_P_B_MASK))) {
			pc->profile.support2 |= 0x01;
		}
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_L2DTMF & ISDN_P_B_MASK))) {
			pc->profile.goptions |= 0x08;
		}
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_L2DSP & ISDN_P_B_MASK))) {
			pc->profile.goptions |= 0x08;
		}
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_T30_FAX & ISDN_P_B_MASK))) {
			pc->profile.support1 |= 0x10;
			pc->profile.support2 |= 0x10;
			pc->profile.support3 |= 0x10;
		}
		if (pc->devinfo.Bprotocols & (1 << (ISDN_P_B_MODEM_ASYNC & ISDN_P_B_MASK))) {
			pc->profile.support1 |= 0x180;
			pc->profile.support3 |= 0x80;
		}
	}
	i = read_config_file(config_file);
	if (i < 0)
		goto errout;
retry_Csock:
	mCsock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (mCsock < 0) {
		fprintf(stderr, "cannot create socket - %s\n", strerror(errno));
		goto errout;
	}
	mcaddr.sun_family = AF_UNIX;
	sprintf(mcaddr.sun_path, MISDN_CAPI_SOCKET_PATH);
	ret = bind(mCsock, (struct sockaddr *)&mcaddr, sizeof(mcaddr));
	if (ret < 0) {
		fprintf(stderr, "cannot bind socket to %s - %s\n", mcaddr.sun_path, strerror(errno));
		if (errno == EADDRINUSE) {	/* old socket file exist */
			ret = connect(mCsock, (struct sockaddr *)&mcaddr, sizeof(mcaddr));
			if (ret < 0) {
				/* seems the socket file is not in use */
				ret = unlink(MISDN_CAPI_SOCKET_PATH);
				if (ret < 0) {
					fprintf(stderr, "cannot remove old socket file %s - %s\n",
						MISDN_CAPI_SOCKET_PATH, strerror(errno));
					goto errout;
				}
				close(mCsock);
				goto retry_Csock;
			} else {
				fprintf(stderr, "mISDNcapid is already running - only one instance can be used\n");
				goto errout;
			}

		}
	}
	ret = chmod(MISDN_CAPI_SOCKET_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (ret < 0) {
		fprintf(stderr, "cannot change permissions on unix socket:%s - %s\n", MISDN_CAPI_SOCKET_PATH, strerror(errno));
		goto errout;
	}

	if (listen(mCsock, 15)) {
		fprintf(stderr, "cannot set listen mode for socket %s - %s\n", mcaddr.sun_path, strerror(errno));
		goto errout;
	}
	fprintf(stderr, "debug=%#x do_daemon=%d config=%s\n", debugmask, do_daemon, config_file);
	if (do_daemon) {
		ret = daemon(0, 0);
		if (ret < 0) {
			fprintf(stderr, "cannot run as daemon\n");
			goto errout;
		}
	}
	init_listen();
	init_lPLCI_fsm();
	init_ncci_fsm();
#ifdef USE_SOFTFAX
	create_lin2alaw_table();
	g3_gen_tables();
#endif
	/* setup signal handler */
	memset(&mysig_act, 0, sizeof(mysig_act));
	mysig_act.sa_handler = termHandler;
	ret = sigaction(SIGTERM, &mysig_act, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGTERM - %s\n", strerror(errno));
	}
	ret = sigaction(SIGHUP, &mysig_act, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGHUP - %s\n", strerror(errno));
	}
	ret = sigaction(SIGINT, &mysig_act, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGINT - %s\n", strerror(errno));
	}
	exitcode = main_loop();
	free_ncci_fsm();
	free_lPLCI_fsm();
	free_listen();
#ifdef USE_SOFTFAX
	g3_destroy_tables();
#endif
errout:
	mc_buffer_cleanup();
	cleanup_layer3();
	if (mCsock > -1)
		close(mCsock);
	close(mIsock);
	if (mI_Controller)
		free(mI_Controller);

	unlink(MISDN_CAPI_SOCKET_PATH);
	if (TempDirectory && !KeepTemporaryFiles) {
		ret = rmdir(TempDirectory);
		if (ret)
			wprint("Error to remove TempDirectory:%s - %s\n", TempDirectory, strerror(errno));
		else
			iprint("Removed TempDirectory:%s\n", TempDirectory);
	}
	if (DebugFile)
		fclose(DebugFile);
	return exitcode;
}
