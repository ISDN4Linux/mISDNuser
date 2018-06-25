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

#define _GNU_SOURCE
#include <unistd.h>
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
#include <sys/syscall.h>
#include <sys/types.h>
#include <grp.h>
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

#define MISDNCAPID_VERSION	"1.0"

typedef enum {
	PIT_None = 0,
	PIT_Control,
	PIT_mISDNmain,
	PIT_mISDNtimer,
	PIT_CAPImain,
	PIT_NewConn,
	PIT_Application,
	PIT_ReleasedApp,
	PIT_Bchannel,
} pollInfo_t;

struct pollInfo {
	pollInfo_t	type;
	void		*data;
};

/* give 5 sec periode to allow releasing all applications */
#define MI_SHUTDOWN_DELAY	5000

static struct pollfd *mainpoll = NULL;
static struct pollInfo *pollinfo = NULL;
static int mainpoll_max = 0;
static int mainpoll_size = 0;
#define MAINPOLL_LIMIT 256

static char def_config[] = DEF_CONFIG_FILE;
static char *config_file = NULL;
static int do_daemon = 1;
static int mIsock = -1;
static int mCsock = -1;
static int mIControl[2] = {-1, -1};
static struct pController *mI_Controller = NULL;
int mI_ControllerCount = 0;
static FILE *DebugFile = NULL;
static char *DebugFileName = NULL;
static unsigned int debugmask = 0;
static int DebugUniquePLCI = 0;
int KeepTemporaryFiles = 0;
int WriteWaveFiles = 0;
static char _TempDirectory[80];
char *TempDirectory = NULL;
static struct timer_base _mICAPItimer_base;
struct timer_base *mICAPItimer_base;

#define MISDND_TEMP_DIR	"/tmp"

static char *__pinfonames[] = {
	"None",
	"Control",
	"mISDNmain",
	"mISDNtimer",
	"CAPImain",
	"NewConn",
	"Application",
	"ReleasedApp",
	"Bchannel",
	"unknown"
};

static char *pinfo2str(pollInfo_t pit)
{
	unsigned int i = pit;

	if (pit > PIT_Bchannel)
		i = PIT_Bchannel + 1;
	return __pinfonames[i];
}

pid_t gettid(void)
{
	pid_t tid;
	tid = syscall(SYS_gettid);
	return tid;
}

static void usage(void)
{
	fprintf(stderr, "Usage: mISDNcapid [OPTIONS]\n");
	fprintf(stderr, "  Options are\n");
	fprintf(stderr, "   -?, --help                            this help\n");
	fprintf(stderr, "   -c, --config <file>                   use this config file - default %s\n", def_config);
	fprintf(stderr, "   -d, --debug <debugmask>               enable additional debug (see m_capi.h for the bits)\n");
	fprintf(stderr, "   -D, --debug-file <debug file>         use debug file (default stdout/stderr)\n");
	fprintf(stderr, "   -f, --foreground                      run in forground, not as daemon\n");
	fprintf(stderr, "   -k, --keeptemp                        do not delete temporary files (e.g. TIFF for fax)\n");
	fprintf(stderr, "   -u, --uniquePLCI                      use unique PLCI numbers - easier to follow in the debug log\n");
//	fprintf(stderr, "   -w, --writewavefiles                  write wave files for debug fax\n");
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
			{"uniquePLCI", 0, 0, 'u'},
			{"writewavefiles", 0, 0, 'w'},
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "?c:D:d:fkuw", long_options, &option_index);
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
				debugmask = (unsigned int)strtoul(optarg, NULL, 0);
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
		case 'u':
			DebugUniquePLCI = 1;
			break;
		case 'w':
			WriteWaveFiles++;
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
			if (contr > mI_ControllerCount - 1) {
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
			mI_Controller[contr].cobjLC.id = capicontr;
			mI_Controller[contr].cobjPLCI.id = capicontr;
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

int send_master_control(int event, int len, void *para)
{
	int ret, err, *msg, totlen = sizeof(event);

	if (len > 0) {
		totlen += len;
		msg = malloc(totlen);
		if (!msg) {
			eprint("Cannot alloc %d bytes for maincontrol message\n", totlen);
			return -ENOMEM;
		}
		*msg = event | len;
		msg++;
		memcpy(msg, para, len);
		msg--;
	} else {
		msg = &event;
	}
	ret = write(mIControl[1], msg, totlen);
	if (ret != totlen) {
		if (ret < 0)
			err = errno;
		else
			err = EMSGSIZE;
		eprint("Cannot send maincontrol message %08x return %d (%d) - %s\n", *msg, ret, totlen, strerror(err));
		ret = -err;
	} else {
		ret = 0;
	}
	if (len > 0)
		free(msg);
	return ret;
}

/**********************************************************************
Signal handler for clean shutdown
***********************************************************************/
static void
termHandler(int sig)
{
	send_master_control(MICD_CTRL_SHUTDOWN, 0, NULL);
	return;
}



/**********************************************************************
Signal handler for re-opening log file
***********************************************************************/
static void
hupHandler(int sig)
{
	send_master_control(MICD_CTRL_REOPEN_LOG, 0, NULL);
	return;
}



/**********************************************************************
Signal handler for dumping state info
***********************************************************************/
static void dump_mainpoll(void)
{
	int i;

	for (i = 0; i < mainpoll_max; i++) {
		iprint("Poll[%i] fd:%d mask:%04x/%04x data:%p type:%s\n",
			i, mainpoll[i].fd, mainpoll[i].events, mainpoll[i].revents, pollinfo[i].data, pinfo2str(pollinfo[i].type));
	}
}

static void dump_controller(void)
{
	int i, j;
	struct BInstance *bi;

	for (i = 0; i < mI_ControllerCount; i++) {
		if (mI_Controller[i].enable) {
			iprint("Controller%d mISDN No:%d Bchannels:%d capiNr:%d ApplCnt:%d\n", i, mI_Controller[i].mNr,
				mI_Controller[i].devinfo.nrbchan, mI_Controller[i].profile.ncontroller, mI_Controller[i].appCnt);
			iprint("Controller%d InfoMask:%08x CIPMask:%08x CIP2Mask:%08x\n", i, mI_Controller[i].InfoMask,
				mI_Controller[i].CIPmask, mI_Controller[i].CIPmask2);
			for (j = 0; j < mI_Controller[i].BImax; j++) {
				bi = &mI_Controller[i].BInstances[j];
				iprint("Controller%d Bi[%d] B%d type:%s usecnt:%d proto:%x fd:%d tty:%d%s%s%s%s%s\n", i, j, bi->nr,
					BItype2str(bi->type),  bi->usecnt,  bi->proto,  bi->fd,  bi->tty,
					bi->closed ? " closed" : "", bi->closing ? " closing" : "", bi->running ? " running" : "",
					bi->detached ? " detached" : "", bi->joined ? " joined" : "");
			}
			dump_lControllers(&mI_Controller[i]);
			dump_controller_plci(&mI_Controller[i]);
		} else {
			iprint("Controller%d disabled\n", i);
		}
	}
}

const char *_BTypes[] = {
	"None",
	"Direct",
	"Fax",
	"tty",
	"Wrong value"
};

const char *BItype2str(enum BType bt)
{
	const char *r;
	unsigned int i = (unsigned int)bt;

	if (i <= BType_tty)
		r = _BTypes[i];
	else
		r = _BTypes[4];
	return r;
}

static void
dumpHandler(int sig)
{
	if (sig == SIGUSR1)
		send_master_control(MICD_CTRL_DUMP_1, 0, NULL);  // master control will call do_dump
	else
		send_master_control(MICD_CTRL_DUMP_2, 0, NULL);  // master control will call do_dump
	return;
}

static void do_dump(const int sig)
{
	if (sig == SIGUSR1) {
		iprint("Received  signal %d -- start dumping\n", sig);
		dump_applications();
		dump_controller();
		dump_mainpoll();
		mc_buffer_dump_status();
		dump_cobjects();
	} else {
		dump_cobjects();
#ifdef MISDN_CAPIOBJ_NO_FREE
		dump_cobjects_free();
#endif
	}
	iprint("dumping ends\n");
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
				n = 8;
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
			case PIT_ReleasedApp:	/* not freed here */
			case PIT_Application:	/* not freed here */
			case PIT_Bchannel:	/* Never freed */
			case PIT_mISDNtimer:
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

static int modify_mainpoll(int enable, int fd)
{
	int i;

	for (i = 0; i < mainpoll_max; i++) {
		if (mainpoll[i].fd == fd) {
			if (enable)
				mainpoll[i].events = POLLIN | POLLPRI;
			else
				mainpoll[i].events = POLLPRI;
			dprint(MIDEBUG_CONTROLLER, "Mainpoll: fd=%d now %s\n", fd, enable ? "enabled" : "disabled");
			return 0;
		}
	}
	return  -EINVAL;
}

int mIcapi_mainpoll_releaseApp(int fd, int newfd)
{
	int i;

	for (i = 0; i < mainpoll_max; i++) {
		if (mainpoll[i].fd == fd) {
			if (pollinfo[i].type == PIT_Application || pollinfo[i].type == PIT_ReleasedApp) {
				mainpoll[i].fd = newfd;
				pollinfo[i].type = PIT_ReleasedApp;
				return i;
			} else {
				eprint("Mainpoll fd=%d found but type %s\n", fd, BItype2str(pollinfo[i].type));
			}
		}
	}
	return -1;
}

static int main_control(int idx)
{
	int ret, event, len, *fds;
	void *para = NULL;

	len = sizeof(event);
	ret = read(mainpoll[idx].fd, &event, len);

	if (ret != len) {
		if (ret > 0)
			event = -EMSGSIZE;
		else
			event = -errno;
		eprint("Event for MasterControl read error read return %d (%d) - %s\n", ret, len, strerror(-event));
		return event;
	}
	len = event & MICD_EV_LEN;
	if (len) {
		para = malloc(len);
		if (!para) {
			eprint("Event for MasterControl cannot alloc %d bytes for parameter\n", len);
			return -ENOMEM;
		}
		ret = read(mainpoll[idx].fd, para, len);
		if (ret != len) {
			if (ret > 0)
				event = -EMSGSIZE;
			else
				event = -errno;
			eprint("Event for MasterControl read error read on parameter return %d (%d) - %s\n", ret, len, strerror(-event));
			free(para);
			return event;
		}
	}
	switch (event & MICD_EV_MASK) {
	case MICD_CTRL_SHUTDOWN:
		iprint("Terminating on signal -- request shutdown mISDNcapid\n");
		break;
	case MICD_CTRL_REOPEN_LOG:
		iprint("Re-opening log on signal\n");
		if (DebugFileName)
		{
			iprint("Received SIGHUP, closing this file for immediate re-open\n");
			fflush(DebugFile);
			fclose(DebugFile);
			DebugFile = fopen(DebugFileName, "a");
			if (!DebugFile)
			{
				fprintf(stderr, "Cannot re-open %s - %s\n", DebugFileName, strerror(errno));
				break;
			}
			iprint("Re-opened debug log file after SIGHUP\n");
			fflush(DebugFile);
		}
		else
			iprint("Nothing to do for SIGHUP since no debug file configured\n");
		break;
	case MICD_CTRL_DUMP_1:
		do_dump(SIGUSR1);
		break;
	case MICD_CTRL_DUMP_2:
		do_dump(SIGUSR2);
		break;

	case MICD_CTRL_DISABLE_POLL:
	case MICD_CTRL_ENABLE_POLL:
		len /= sizeof(int);
		fds = para;
		while (len) {
			ret = modify_mainpoll((event & MICD_EV_MASK) == MICD_CTRL_ENABLE_POLL, *fds);
			if (ret)
				wprint("modify_mainpoll for fd=%d failed\n", *fds);
			len--;
			fds++;
		}
		break;
	default:
		eprint("Unknown event for MasterControl %08x - ignored\n", event);
		event = -EINVAL;
	}
	if (para)
		free(para);
	return event;
}

void clean_all(void)
{
	int i, j;
	struct mApplication *ap;
	struct mCAPIobj *co;
	struct lController *lc;

	dprint(MIDEBUG_CONTROLLER, "clean_all controller:%d mainpoll %d/%d\n", mI_ControllerCount, mainpoll_max, mainpoll_size);
	if (!mainpoll_max && !mainpoll_size) {
		wprint("clean_all called twice\n");
		return;
	}
	for (i = 0; i < mainpoll_max; i++) {
		switch (pollinfo[i].type) {
		case PIT_Control:
			dprint(MIDEBUG_CONTROLLER, "close mIControl(%d, %d)\n", mIControl[0], mIControl[1]);
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
			ap = pollinfo[i].data;
			if (ap) {
				eprint("Application %d not released in clean_all freeing now\n", ap->cobj.id2);
				pollinfo[i].data = NULL;
				ReleaseApplication(ap, 0);
				Free_Application(&ap->cobj);
			} else
				eprint("Unlinked Application not released in clean_all\n");
			break;
		case PIT_Bchannel:
			ReleaseBchannel(pollinfo[i].data);
			break;
		case PIT_NewConn:
			dprint(MIDEBUG_CONTROLLER, "close mainpoll[%d].fd %d\n", i, mainpoll[i].fd);
			close(mainpoll[i].fd);
			break;
		case PIT_ReleasedApp:
			ap = pollinfo[i].data;
			if (ap) {
				eprint("Released Application %d  in clean_all freeing now\n", ap->cobj.id2);
				pollinfo[i].data = NULL;
				Free_Application(&ap->cobj);
			}
			break;
		case PIT_mISDNmain:
		case PIT_mISDNtimer:
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
	if (pollinfo)
		free(pollinfo);
	pollinfo = NULL;
	if (mainpoll)
		free(mainpoll);
	mainpoll = NULL;
	for (i = 0; i < mI_ControllerCount; i++) {
		dprint(MIDEBUG_CONTROLLER, "clean_all controller:%d BImax=%d\n", i, mI_Controller[i].BImax);
		if (mI_Controller[i].l3) {
			close_layer3(mI_Controller[i].l3);
			mI_Controller[i].l3 = NULL;
		}
		if (mI_Controller[i].BInstances) {
			for (j = 0; j < mI_Controller[i].BImax; j++) {
				ReleaseBchannel(&mI_Controller[i].BInstances[j]);
			}
			free(mI_Controller[i].BInstances);
			mI_Controller[i].BInstances = NULL;
		}
		co = get_next_cobj(&mI_Controller[i].cobjLC, NULL);
		while (co) {
			lc = container_of(co, struct lController, cobj);
			Free_lController(&lc->cobj);
			co = get_next_cobj(&mI_Controller[i].cobjLC, co);
		}
	}
	mI_ControllerCount = 0;
}

struct pController *get_cController(int cNr)
{
	int i;

	for (i = 0; i < mI_ControllerCount; i++) {
		if (mI_Controller[i].enable && mI_Controller[i].profile.ncontroller == cNr)
			return &mI_Controller[i];
	}
	return NULL;
}

struct pController *get_mController(int mNr)
{
	int i;

	for (i = 0; i < mI_ControllerCount; i++) {
		if (mI_Controller[i].enable && mI_Controller[i].mNr == mNr)
			return &mI_Controller[i];
	}
	return NULL;
}

/* Is called with pc->cobjPLCI.lock !!! */
uint32_t NextFreePLCI(struct mCAPIobj *copc)
{
	uint32_t id = 0, last;
	uint8_t	run;
	struct pController *pc;
	struct mCAPIobj *c;

	pc = container_of(copc, struct pController, cobjPLCI);
	if (DebugUniquePLCI) {
		run = 2; /* one wrap after reaching 0xff00 allowed */
		pc->lastPLCI += 0x0100;
		if (pc->lastPLCI > 0xff00) {
			pc->lastPLCI = 0x0100;
			run--;
		}
		last = pc->lastPLCI;
	} else {
		last = 0x0100;
		run = 1; /* stop after reaching 0xff00 */
	}
	/* check if not used */
	while (run) {
		c = copc->listhead;
		while (c) {
			if ((c->id & 0xff00) == last)
				break;
			c = c->next;
		}
		if (!c) {
			id = last;
			break;
		} else {
			if (last == 0xff00) {
				run--;
				last = 0x0100;
			} else {
				last += 0x0100;
			}
		}
	}
	if (id)
		pc->lastPLCI = id;
	return id;
}

struct BInstance *ControllerSelChannel(struct pController *pc, int nr, int proto)
{
	struct BInstance *bi;
	int pmask, err;

	if (nr >= pc->BImax) {
		wprint("Request for channel number %d but controller %d only has %d channels\n",
		       nr, pc->profile.ncontroller, pc->devinfo.nrbchan);
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
	err = pthread_mutex_lock(&bi->lock);
	if (err == 0) {
		if (bi->usecnt) {
			/* for now only one user allowed - this is not sufficient for X25 */
			wprint("Request for channel number %d on controller %d but channel already in use\n",
				nr, pc->profile.ncontroller);
			pthread_mutex_unlock(&bi->lock);
			bi = NULL;
		} else {
			bi->usecnt++;
			bi->proto = proto;
			pthread_mutex_unlock(&bi->lock);
		}
	} else {
		eprint("Controller%d lock for BI[%d] could not aquired - %s\n",
			pc->profile.ncontroller, bi->nr, strerror(err));
		bi = NULL;
	}
	return bi;
}

int ControllerDeSelChannel(struct BInstance *bi)
{
	int err;

	err = pthread_mutex_lock(&bi->lock);
	if (err == 0) {
		if (bi->usecnt) {
			bi->usecnt--;
			bi->proto = ISDN_P_NONE;
		} else {
			wprint("BI[%d] usage count is already zero\n",
				bi->nr);
			err = -EINVAL;
		}
		pthread_mutex_unlock(&bi->lock);
	} else {
		eprint("Lock for BI[%d] could not be acquired - %s\n",
			bi->nr, strerror(err));
	}
	return err;
}

int check_free_bchannels(struct pController *pc)
{
	int i, cnt = 0;

	if (pc) {
		for (i = 0; i <= pc->BImax; i++) {
			if (test_channelmap(i, pc->devinfo.channelmap)) {
				if (pc->BInstances[i].usecnt == 0)
					cnt++;
			}
		}
	}
	return cnt;
}


static int Create_tty(struct BInstance *bi)
{
	int ret, pmod;

	ret = posix_openpt(O_RDWR | O_NOCTTY);
	if (ret < 0) {
		eprint("Cannot open terminal - %s\n", strerror(errno));
	} else {
		bi->tty = ret;
		dprint(MIDEBUG_CONTROLLER, "create bi[%d]->tty %d\n", bi->nr, bi->tty);
		ret = grantpt(bi->tty);
		if (ret < 0) {
			eprint("Error on grantpt - %s\n", strerror(errno));
			dprint(MIDEBUG_CONTROLLER, "close bi[%d]->tty %d\n", bi->nr, bi->tty);
			close(bi->tty);
			bi->tty = -1;
		} else {
			ret = unlockpt(bi->tty);
			if (ret < 0) {
				eprint("Error on unlockpt - %s\n", strerror(errno));
				dprint(MIDEBUG_CONTROLLER, "close bi[%d]->tty %d\n", bi->nr, bi->tty);
				close(bi->tty);
				bi->tty = -1;
			} else {
				/* packet mode */
				pmod = 1;
				ret = ioctl(bi->tty, TIOCPKT, &pmod);
				if (ret < 0) {
					eprint("Cannot set packet mode - %s\n", strerror(errno));
					dprint(MIDEBUG_CONTROLLER, "close bi[%d]->tty %d\n", bi->nr, bi->tty);
					close(bi->tty);
					bi->tty = -1;
				} else {
					bi->tty_received = 0;
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
		bi->tty_received = 1;
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

static char *_threadmsg1 = "running";
static char *_threadmsg2 = "stopped after poll";
static char *_threadmsg3 = "stopped after receive";
static char *_threadmsg4 = "stopped via command";

static void *BCthread(void *arg)
{
	struct BInstance *bi = arg;
	unsigned char cmd;
	char *msg = _threadmsg1;
	int ret, i;

	bi->running = 1;
	bi->got_timeout = 0;
	bi->detached = 0;
	bi->joined = 0;
	bi->release_pending = 0;
	bi->tid = gettid();
	iprint("Started Bchannel thread=%05d\n", bi->tid);
	while (bi->running) {
		if (bi->pcnt == 0) {
			wprint("Bchannel%d no filedescriptors to poll - abort thread=%05d\n",
				bi->nr, bi->tid);
			break;
		}
		ret = poll(bi->pfd, bi->pcnt, bi->timeout);
		if (!bi->running) {
			msg = _threadmsg2;
			break;
		}
		if (ret < 0) {
			wprint("Bchannel%d Error on poll - %s\n", bi->nr, strerror(errno));
			continue;
		}
		if (ret == 0) { /* timeout */
			wprint("Bchannel%d %stimeout (release %spending) thread=%05d\n", bi->nr,
				bi->got_timeout ? "2. " : "", bi->release_pending ? "" : "not ", bi->tid);
			if (bi->release_pending) {
				if (bi->got_timeout) { /* 2 times */
					bi->detached = 1;
					ret = pthread_detach(bi->thread);
					if (ret)
						wprint("Error on pthread_detach thread=%05d - %s\n", bi->tid, strerror(ret));
					bi->running = 0;
				} else
					bi->got_timeout = 1;
				ret = bi->from_down(bi, NULL);
				if (ret < 0)
					wprint("Bchannel%d from down RELEASE return %d - %s\n", bi->nr, ret, strerror(-ret));
			}
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
		if (!bi->running) {
			msg = _threadmsg3;
			break;
		}
		if (bi->pfd[0].revents & POLLIN) {
			ret = read(bi->pfd[0].fd, &cmd, 1);
			iprint("Got control %d thread=%05d\n", cmd, bi->tid);
			if (cmd == 42) {
				bi->running = 0;
				msg = _threadmsg4;
			}
		}
		bi->got_timeout = 0;
	}
	iprint("thread=%05d terminating with msg %s\n", bi->tid, msg);
	return msg;
}

static int CreateBchannelThread(struct BInstance *bi, int pcnt)
{
	int ret, i;

	if (bi->cpipe[0] > -1 || bi->cpipe[1] > -1)
		wprint("bi[%d]->cpipe(%d, %d) still open - may leaking fds\n", bi->nr, bi->cpipe[0], bi->cpipe[1]);
	ret = pipe(bi->cpipe);
	if (ret) {
		eprint("error - %s\n", strerror(errno));
		return ret;
	} else
		dprint(MIDEBUG_CONTROLLER, "create bi[%d]->cpipe(%d, %d)\n",  bi->nr, bi->cpipe[0], bi->cpipe[1]);
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
	bi->timeout = 500; /* default poll timeout 500ms */
	ret = pthread_create(&bi->thread, NULL, BCthread, bi);
	if (ret) {
		eprint("Cannot create thread error - %s\n", strerror(errno));
		bi->detached = 1;
		dprint(MIDEBUG_CONTROLLER, "close bi[%d]->cpipe(%d, %d)\n",  bi->nr, bi->cpipe[0], bi->cpipe[1]);
		close(bi->cpipe[0]);
		close(bi->cpipe[1]);
		bi->cpipe[0] = -1;
		bi->cpipe[1] = -1;
		bi->pfd[0].fd = -1;
		for (i = 1; i < bi->pcnt; i++)
			bi->pfd[i].fd = -1;
	}
	return ret;
}

static int StopBchannelThread(struct BInstance *bi)
{
	int ret, i, result = 0;
	unsigned char cmd;
	char *msg = NULL;
	pthread_t self = pthread_self();

	if (bi->running) {
		if (bi->waiting)
			sem_post(&bi->wait);
		if (pthread_equal(self, bi->thread)) {
			/* we cannot join our self but we will terminate on return */
			bi->detached = 1;
			ret = pthread_detach(bi->thread);
			if (ret) {
				wprint("Error on pthread_detach thread=%05d - %s\n", bi->tid, strerror(ret));
				result = -ret;
			}
			bi->running = 0;
			dprint(MIDEBUG_CONTROLLER, "thread=%05d detached\n", bi->tid);
		} else if (!bi->joined) {
			cmd = 42;
			ret = write(bi->cpipe[1], &cmd, 1);
			if (ret != 1)
				wprint("Error on write control ret=%d - %s\n", ret, strerror(errno));
			bi->joined = 1;
			ret = pthread_join(bi->thread, (void **)&msg);
			if (ret) {
				wprint("Error on pthread_join - %s\n", strerror(ret));
				result = -ret;
			} else
				dprint(MIDEBUG_CONTROLLER, "thread=%05d joined to %05d %s\n", bi->tid, gettid(), msg);
		} else {
			wprint("Running but already joined in thread=%05d ???\n", bi->tid);
		}
		dprint(MIDEBUG_CONTROLLER, "close bi[%d]->cpipe(%d, %d)\n",  bi->nr, bi->cpipe[0], bi->cpipe[1]);
		close(bi->cpipe[0]);
		close(bi->cpipe[1]);
		bi->pfd[0].fd = -1;
		for (i = 1; i < bi->pcnt; i++)
			bi->pfd[i].fd = -1;
		bi->pcnt = 0;
		bi->cpipe[0] = -1;
		bi->cpipe[1] = -1;
	} else {
		if (!bi->detached) {
			if (!bi->joined) {
				if (pthread_equal(self, bi->thread)) {
					/* we cannot join our self but we will terminate on return */
					wprint("Not running but still in thread=%05d - possible memleak\n", bi->tid);
					result = -EDEADLOCK;
				} else {
					bi->joined = 1;
					ret = pthread_join(bi->thread, (void **)&msg);
					if (ret) {
						wprint("Error on pthread_join thread=%05d - %s\n", bi->tid, strerror(ret));
						result = -ret;
					} else
						dprint(MIDEBUG_CONTROLLER, "thread=%05d joined to %05d %s\n", bi->tid, gettid(), msg);
				}
			}
		}
	}
	return result;
}

static int dummy_btrans(struct BInstance *bi, struct mc_buf *mc)
{
	struct mISDNhead *hh;

	if (!mc) {
		wprint("Controller%d ch%d: Got timeout RELEASE - but %s called thread=%x\n", bi->pc->profile.ncontroller, bi->nr,
			__func__, (unsigned int)pthread_self());
		return 0;
	} else {
		hh = (struct mISDNhead *)mc->rb;
		wprint("Controller%d ch%d: Got %s id %x - but %s called thread=%x\n", bi->pc->profile.ncontroller, bi->nr,
			_mi_msg_type2str(hh->prim), hh->id, __func__, (unsigned int)pthread_self());
		return -EINVAL;
	}
}

int OpenBInstance(struct BInstance *bi, struct lPLCI *lp)
{
	int sk;
	int ret;
	struct sockaddr_mISDN addr;
	struct mISDN_ctrl_req creq;

	sk = socket(PF_ISDN, SOCK_DGRAM, bi->proto);
	if (sk < 0) {
		wprint("Cannot open socket for BInstance %d on controller %d protocol 0x%02x - %s\n",
		       bi->nr, bi->pc->profile.ncontroller, bi->proto, strerror(errno));
		return -errno;
	} else
		dprint(MIDEBUG_CONTROLLER, "create socket bi[%d]->fd %d\n", bi->nr, sk);

	ret = fcntl(sk, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		ret = -errno;
		wprint("fcntl error %s\n", strerror(errno));
		dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
		close(sk);
		return ret;
	}

	if (get_cobj(&lp->cobj)) {
		bi->lp = lp;
	} else {
		bi->lp = NULL;
		ret = -EINVAL;
		wprint("Cannot get logical controller object\n");
		close(sk);
		return ret;
	}

	switch (lp->btype) {
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
		eprint("Error unnkown BType %d\n",  lp->btype);
		dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
		close(sk);
		bi->lp = NULL;
		put_cobj(&lp->cobj);
		return -EINVAL;
	}
	bi->type = lp->btype;

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
		dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
		close(sk);
		bi->from_down = dummy_btrans;
		bi->from_up = dummy_btrans;
		bi->type = BType_None;
		bi->lp = NULL;
		put_cobj(&lp->cobj);
	} else {
		bi->closed = 0;
		bi->fd = sk;
		bi->org_rx_min = MISDN_CTRL_RX_SIZE_IGNORE;
		bi->rx_min = MISDN_CTRL_RX_SIZE_IGNORE;
		bi->org_rx_max = MISDN_CTRL_RX_SIZE_IGNORE;
		bi->rx_max = MISDN_CTRL_RX_SIZE_IGNORE;
		if ((bi->proto == ISDN_P_B_L2DSP) || (bi->proto == ISDN_P_B_RAW)) {
			/* Get buffersize */
			creq.op = MISDN_CTRL_RX_BUFFER;
			creq.channel = bi->nr;
			creq.p1 = MISDN_CTRL_RX_SIZE_IGNORE; // do not change min yet
			creq.p2 = MISDN_CTRL_RX_SIZE_IGNORE; // do not change max yet
			creq.unused = 0;
			ret = ioctl(bi->fd, IMCTRLREQ, &creq);
			/* MISDN_CTRL_RX_BUFFER is not mandatory warn if not supported */
			if (ret < 0) {
				wprint("%s: Error on MISDN_CTRL_RX_BUFFER  ioctl maybe kernel do not support it  - %s\n",
					CAPIobjIDstr(&lp->cobj), strerror(errno));
			} else {
				bi->org_rx_min = creq.p1;
				bi->org_rx_max = creq.p2;
				dprint(MIDEBUG_NCCI, "%s: MISDN_CTRL_RX_BUFFER  original values: min=%d max=%d\n",
					CAPIobjIDstr(&lp->cobj), creq.p1, creq.p2);
				if (bi->org_rx_max > lp->Appl->MaxB3Size)
					bi->rx_max = lp->Appl->MaxB3Size;
				else
					bi->rx_max = bi->org_rx_max;
				if (bi->type == BType_Fax) {
					bi->rx_min = DEFAULT_FAX_PKT_SIZE;
				} else {
					bi->rx_min = bi->rx_max  - (bi->org_rx_min - 1);
					if (bi->rx_min < bi->org_rx_min)
						bi->rx_min = bi->org_rx_min;
				}
				if (bi->rx_min > bi->rx_max)
					bi->rx_min = bi->rx_max;
				/* Set buffersize */
				creq.op = MISDN_CTRL_RX_BUFFER;
				creq.channel = bi->nr;
				creq.p1 = bi->rx_min;
				creq.p2 = bi->rx_max;
				creq.unused = 0;
				ret = ioctl(bi->fd, IMCTRLREQ, &creq);
				if (ret < 0) {
					wprint("%s: Error setting  MISDN_CTRL_RX_BUFFER min=%d max=%d ioctl - %s\n",
						CAPIobjIDstr(&lp->cobj), bi->rx_min, bi->rx_max, strerror(errno));
				} else {
					dprint(MIDEBUG_NCCI, "%s: set rxbuffer to min=%d max=%d\n",
						CAPIobjIDstr(&lp->cobj), bi->rx_min, bi->rx_max);
				}
			}
		}
		if (bi->type == BType_Direct) {
			ret = add_mainpoll(sk, PIT_Bchannel);
			if (ret < 0) {
				eprint("Error while adding mIsock to mainpoll (mainpoll_max %d)\n", mainpoll_max);
				dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
				put_cobj(&lp->cobj);
			} else {
				dprint(MIDEBUG_CONTROLLER, "Controller%d: Bchannel %d socket %d added to poll idx %d\n",
				       bi->pc->profile.ncontroller, bi->nr, sk, ret);
				pollinfo[ret].data = bi;
				ret = 0;
				bi->UpId = 0;
				bi->DownId = 0;
			}
		} else if (bi->type == BType_Fax) {
			ret = CreateBchannelThread(bi, 2);
			if (ret < 0) {
				eprint("Error while creating B%d-channel thread\n", bi->nr);
				dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
				put_cobj(&lp->cobj);
			} else {
				ret = 0;
				bi->UpId = 0;
				bi->DownId = 0;
			}
		} else if (bi->type == BType_tty) {
			ret = Create_tty(bi);
			if (ret < 0) {
				eprint("Error while creating B%d-channel tty\n", bi->nr);
				dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
				put_cobj(&lp->cobj);
				return ret;
			} else {
				bi->pfd[2].fd = bi->tty;
				bi->pfd[2].events = POLLIN | POLLPRI;
				ret = CreateBchannelThread(bi, 3);
			}
			if (ret < 0) {
				eprint("Error while creating B%d-channel thread\n", bi->nr);
				dprint(MIDEBUG_CONTROLLER, "close socket bi[%d]->fd %d\n", bi->nr, sk);
				close(sk);
				bi->fd = -1;
				bi->lp = NULL;
				put_cobj(&lp->cobj);
			} else {
				ret = 0;
				bi->UpId = 0;
				bi->DownId = 0;
			}
		}
		if (ret)
			 bi->type = BType_None;
	}
	return ret;
}

int CloseBInstance(struct BInstance *bi)
{
	int err, ret = 0;

	dprint(MIDEBUG_CONTROLLER, "Closing bchannel type:%d tid=%05d usecnt:%d%s%s%s%s%s\n",
		bi->type, bi->tid, bi->usecnt, bi->closed ? " closed" : "",
		bi->closing ? " closing" : "", bi->running ? " running" : "",
		bi->detached ? " detached" : "", bi->joined ? " joined" : "");
	err = pthread_mutex_lock(&bi->lock);
	if (err == 0) {
		if (!bi->closed) {
			if (!bi->closing) {
				bi->closing = 1;
				if (bi->usecnt) {
					bi->closed = 1;
					pthread_mutex_unlock(&bi->lock);
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
						dprint(MIDEBUG_CONTROLLER, "close bi[%d]->tty %d\n", bi->nr, bi->tty);
						if (bi->tty > -1)
							close(bi->tty);
						bi->tty = -1;
					default:
						break;
					}
					dprint(MIDEBUG_CONTROLLER, "Closing fd=%d usecnt %d\n", bi->fd, bi->usecnt);
					if (bi->fd >= 0)
						close(bi->fd);
					bi->fd = -1;
					bi->proto = ISDN_P_NONE;
					if (bi->b3data && bi->lp)
						B3ReleaseLink(bi->lp, bi);
					if (bi->lp) {
						put_cobj(&bi->lp->cobj);
						bi->lp = NULL;
					}
					bi->from_up(bi, NULL);
					bi->b3data = NULL;
					bi->lp = NULL;
					bi->type = BType_None;
					bi->from_down = dummy_btrans;
					bi->from_up = dummy_btrans;
					err = pthread_mutex_lock(&bi->lock);
					if (err == 0) {
						bi->usecnt--;
						bi->closing = 0;
						if (bi->usecnt)
							wprint("BI[%d] still in use (%d)\n", bi->nr, bi->usecnt);
						pthread_mutex_unlock(&bi->lock);
						ret = 0;
					} else {
						eprint("Lock for BI[%d] could not aquired - %s\n",
							bi->nr, strerror(err));
						ret = -err;
					}
				} else {
					bi->closing = 0;
					pthread_mutex_unlock(&bi->lock);
					wprint("BI[%d] not active\n", bi->nr);
					ret = -1;
				}
			} else {
				pthread_mutex_unlock(&bi->lock);
				wprint("Closing  BI[%d] already in progress\n", bi->nr);
				ret = -1;
			}
		} else {
			pthread_mutex_unlock(&bi->lock);
			wprint("Closing  BI[%d] already closed\n", bi->nr);
			ret = -1;
		}
	} else {
		eprint("Lock for BI[%d] could not aquired - %s\n",
			bi->nr, strerror(err));
		ret = -err;
	}
	return ret;
};

int activate_bchannel(struct BInstance *bi)
{
	int ret, err;
	struct mISDNhead mh;

	mh.id = 1;
	if (bi->proto == ISDN_P_NONE)
		return -EINVAL;
	else
		mh.prim = PH_ACTIVATE_REQ;

	ret = send(bi->fd, &mh, sizeof(mh), 0);
	if (ret != sizeof(mh)) {
		err = errno;
		wprint("BI[%d] cannot send activation ret %d - %s\n", bi->nr, ret, strerror(err));
		if (ret == -1)
			ret = -err;
		else
			ret = -EMSGSIZE;
	} else
		ret = 0;
	return ret;
}

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
		dprint(MIDEBUG_CONTROLLER, "close bi[%d]->fd %d\n", bi->nr, bi->fd);
		close(bi->fd);
		bi->fd = -1;
	}
	if (bi->tty >= 0) {
		dprint(MIDEBUG_CONTROLLER, "close bi[%d]->tty %d\n", bi->nr, bi->tty);
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
		plci = new_mPLCI(pc, pid);
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
		if (!plci) /* Normal on release */
			dprint(MIDEBUG_CONTROLLER, "Controller %d - got %s but no PLCI found\n",
				pc->profile.ncontroller, _mi_msg_type2str(cmd));
		else
			plci->cobj.id2 = MISDN_PID_NONE;
		break;
	case MPH_ACTIVATE_IND:
	case MPH_DEACTIVATE_IND:
	case MPH_INFORMATION_IND:
	case MT_L2ESTABLISH:
	case MT_L2RELEASE:
	case MT_L2IDLE:
		break;
	case MT_TIMEOUT:
		iprint("Controller %d - got %s from layer3 pid(%x) msg(%p) plci(%04x)\n",
		       pc->profile.ncontroller, _mi_msg_type2str(cmd), pid, l3m, plci ? plci->cobj.id : 0xffff);
		break;
	case MT_ERROR:
		wprint("Controller %d - got %s from layer3 pid(%x) msg(%p) plci(%04x)\n",
		       pc->profile.ncontroller, _mi_msg_type2str(cmd), pid, l3m, plci ? plci->cobj.id : 0xffff);
		break;
	default:
		wprint("Controller %d - got %s (%x) from layer3 pid(%x) msg(%p) plci(%04x) - not handled\n",
		       pc->profile.ncontroller, _mi_msg_type2str(cmd), cmd, pid, l3m, plci ? plci->cobj.id : 0xffff);
		ret = -EINVAL;
	}
	if (!ret) {
		if (plci)
			ret = plci_l3l4(plci, cmd, l3m);
		else if (l3m)
			ret = 1; /* message need to be freed */
	}
	if (plci)
		put_cobj(&plci->cobj);
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
	struct mCAPIobj *co;
	struct lController *lc;
	uint32_t InfoMask = 0, CIPMask = 0, CIPMask2 = 0;
	int ret = 0;

	pthread_rwlock_rdlock(&pc->cobjLC.lock);
	co = pc->cobjLC.listhead;
	while (co) {
		lc = container_of(co, struct lController, cobj);
		dprint(MIDEBUG_CONTROLLER, "Lc %p %08x/%08x/%08x\n", lc, lc->InfoMask, lc->CIPmask, lc->CIPmask2);
		InfoMask |= lc->InfoMask;
		CIPMask |= lc->CIPmask;
		CIPMask2 |= lc->CIPmask2;
		co = co->next;
	}
	pthread_rwlock_unlock(&pc->cobjLC.lock);
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

static void ShutdownAppl(int idx, int unregister)
{
	struct mApplication *appl = pollinfo[idx].data;
	int ret;

	if (!appl) {
		eprint("Application not assigned\n");
		return;
	}
	if (appl->cpipe[0] > -1 || appl->cpipe[1] > -1)
		wprint("%s appl->cpipe(%d, %d) still open - leaking fds\n", CAPIobjIDstr(&appl->cobj),  appl->cpipe[0], appl->cpipe[1]);
	ret = pipe2(appl->cpipe, O_NONBLOCK);
	if (ret) {
		eprint("Cannot open application %d control pipe - %s\n", appl->cobj.id2, strerror(errno));
		mainpoll[idx].fd = -1;
	} else {
		dprint(MIDEBUG_CONTROLLER, "create appl->cpipe(%d, %d)\n", appl->cpipe[0], appl->cpipe[1]);
	}
	ReleaseApplication(appl, unregister);
	pollinfo[idx].type = PIT_ReleasedApp;
	mainpoll[idx].fd = appl->cpipe[0];
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
		for (i = 0; i < mI_ControllerCount; i++) {
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

static void mIcapi_release(int fd, int idx, struct mc_buf *mc)
{
	int ret;
	uint16_t aid, info = CapiIllAppNr;
	struct mApplication *appl = pollinfo[idx].data;

	aid = CAPIMSG_APPID(mc->rb);
	CAPIMSG_SETSUBCOMMAND(mc->rb, CAPI_CONF);
	iprint("Unregister application %d (%d)\n", aid, appl ? appl->cobj.id2 : -1);
	if (appl) {
		if (aid == appl->cobj.id2) {
			ShutdownAppl(idx, 1);
			info = CapiNoError;
		} else {
			eprint("Application Id mismatch Got %d have %d\n", aid, appl->cobj.id2);
		}
	} else
		eprint("Application not assigined\n");
	capimsg_setu16(mc->rb, 8, info);
	ret = send(fd, mc->rb, 10, 0);
	if (ret != 10)
		eprint("error send %d/%d  - %s\n", ret, 10, strerror(errno));
	if (info == CapiNoError) {
		close(fd);
		dprint(MIDEBUG_CONTROLLER, "close mIcapi_release fd %d\n", fd);
	}
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
			if (lp && lp->BIlink && lp->BIlink->tty > -1) {
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
		}
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
		mIcapi_release(fd, idx, mc);
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
	int res, ret, i, idx, error = -1, timerId;
	int running = 1, ShutDown = 0, polldelay;
	int fd;
	int nconn = -1;
	struct sockaddr_un caddr;
	char buf[4096];
	socklen_t alen;
	struct mApplication *appl;

	ret = pipe(mIControl);
	if (ret) {
		eprint("error setup MasterControl pipe - %s\n", strerror(errno));
		return errno;
	} else
		dprint(MIDEBUG_CONTROLLER, "create mIControl(%d, %d)\n", mIControl[0], mIControl[1]);
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

	ret = add_mainpoll(mICAPItimer_base->tdev, PIT_mISDNtimer);
	if (ret < 0) {
		eprint("Error while adding mICAPItimer to mainpoll (mainpoll_max %d)\n", mainpoll_max);
		return -1;
	} else
		iprint("mICAPItimer added to idx %d\n", ret);
	pollinfo[ret].data = mICAPItimer_base;

	ret = add_mainpoll(mCsock, PIT_CAPImain);
	if (ret < 0) {
		eprint("Error while adding mCsock to mainpoll (mainpoll_max %d)\n", mainpoll_max);
		return -1;
	} else
		iprint("mCsock added to idx %d\n", ret);

	polldelay = -1;
	while (running) {
		ret = poll(mainpoll, mainpoll_max, polldelay);
		if (ret < 0) {
			if (errno == EINTR)
				iprint("Received signal - continue\n");
			else {
				wprint("Error on poll errno=%d - %s\n", errno, strerror(errno));
				error = -errno;
			}
			continue;
		}
		if (ret == 0) {	/* timeout */
			if (ShutDown) {
				eprint("Shutdown mainpoll timeout reached - force shutdown\n");
				running = 0;
				error = -EBUSY;
				continue;
			}
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
						ShutdownAppl(i, 0);
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
						if (pollinfo[i].type == PIT_Application) {
							ShutdownAppl(i, 0);
							break;
						}
						fd = mainpoll[i].fd;
						dprint(MIDEBUG_POLL, "socket connection %s - fd %d idx %d type %d closed\n",
							res == -ECONNABORTED ? "abort" : "reset", fd, i, pollinfo[i].type);
						close(fd);
						res = del_mainpoll(fd);
						if (res < 0) {
							eprint("Cannot delete fd=%d from mainpoll result %d\n", fd, res);
						} else
							dprint(MIDEBUG_POLL, "Deleted fd=%d from mainpoll\n", fd);
					}
					break;
				case PIT_Control:
					res = main_control(i);
					if (res == MICD_CTRL_SHUTDOWN) {
						iprint("Pollevent ShutdownRequest\n");
						polldelay = MI_SHUTDOWN_DELAY;
						ShutDown = 1;
						res = ReleaseAllApplications();
						if (res <= 0) { /* No Apllication or error shutdown now */
							running = 0;
							error = res;
						}
					}
					break;
				case PIT_ReleasedApp:
					res = read(mainpoll[i].fd, &error, sizeof(error));
					if (res == sizeof(error)) {
						if (error == MI_PUT_APPLICATION) {
							appl = pollinfo[i].data;
							iprint("AppId %d Pollevent MI_PUT_APPLICATION refcnt:%d\n",
								appl->cobj.id2, appl->cobj.refcnt);
							if (appl->cobj.refcnt < 3) {
								res = del_mainpoll(mainpoll[i].fd);
								if (res < 0) {
									eprint("Cannot delete fd=%d from mainpoll result %d\n",
										mainpoll[i].fd, res);
								}
								Free_Application(&appl->cobj);
							}
						}
					}
					break;
				case PIT_mISDNtimer:
					res = read(mainpoll[i].fd, &timerId, sizeof(timerId));
					if (res < 0) {
						eprint("mISDN read timer error %s\n", strerror(errno));
					} else {
						if (res == sizeof(timerId) && timerId) {
							expire_timer(pollinfo[i].data, timerId);
						}
					}
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
	char date[64], log[2048];
	struct tm tm;
	struct timeval tv;
	pid_t tid = gettid();
	char *LC = "UEWID";

	ret = vsnprintf(log, 2000, fmt, va);
	l = gettimeofday(&tv, NULL);
	if (tv.tv_usec > 999999) {
		tv.tv_sec++;
		tv.tv_usec -= 1000000;
	}
	localtime_r(&tv.tv_sec, &tm);
	l = strftime(date, sizeof(date), "%b %e %T", &tm);
	snprintf(&date[l], sizeof(date) - l, ".%06d", (int)tv.tv_usec);

	if (DebugFile) {
		fprintf(DebugFile, "%s %c %16s:%4d %22s(%05d):%s", date, LC[level], file, line, func, tid, log);
		fflush(DebugFile);
	} else if (level > MISDN_LIBDEBUG_WARN)
		fprintf(stdout, "%s %c %20s:%4d %22s(%05d):%s", date, LC[level], file, line, func, tid, log);
	else
		fprintf(stderr, "%s %c %20s:%4d %22s(%05d):%s", date, LC[level], file, line, func, tid, log);
	return ret;
}

static int my_capilib_dbg(const char *file, int line, const char *func, const char *fmt, va_list va)
{
	return my_lib_debug(file, line, func, 1, fmt, va);
}

static struct mi_ext_fn_s l3dbg = {
	.prt_debug = my_lib_debug,
};

static struct sigaction mysig_term, mysig_dump, mysig_hup;

int main(int argc, char *argv[])
{
	int ret, i, j, nb, c, exitcode = 1, ver, libdebug;
	struct sockaddr_un mcaddr;
	struct pController *pc;
	struct group *grp;

	mICAPItimer_base = &_mICAPItimer_base;
	mICAPItimer_base->tdev = -1;
	INIT_LIST_HEAD(&_mICAPItimer_base.pending_timer);
	KeepTemporaryFiles = 0;
	config_file = def_config;
	ret = opt_parse(argc, argv);
	if (ret)
		exit(1);

	memset(&mcaddr, 0, sizeof(mcaddr));

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

	grp = getgrnam(MISDN_GROUP);
	if (!grp) {
		fprintf(stderr, "Cannot get %s group ID - %s\n", MISDN_GROUP, strerror(errno));
		return 1;
	} else {
		if (setegid(grp->gr_gid) < 0) {
			fprintf(stderr, "Cannot set effective group to %s gid:%d - %s\n",
				MISDN_GROUP, (int)grp->gr_gid, strerror(errno));
			return 1;
		}
		iprint("Did set eGID to %s gid:%d\n", MISDN_GROUP, (int)grp->gr_gid);
	}

	ret = mApplication_init();
	if (ret) {
		fprintf(stderr, "Cannot init mApplication -%s\n", strerror(ret));
		return 1;
	}
	CAPIobj_init();

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
	mICAPItimer_base->tdev = ret;
	
	mIsock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (mIsock < 0) {
		fprintf(stderr, "mISDNv2 not installed - %s\n", strerror(errno));
		return 1;
	}

	/* get number of stacks */
	ret = ioctl(mIsock, IMGETCOUNT, &mI_ControllerCount);
	if (ret < 0) {
		fprintf(stderr, "mISDNv2 IMGETCOUNT error - %s\n", strerror(errno));
		goto errout;
	}
	if (mI_ControllerCount < 1) {
		fprintf(stderr, "mISDNv2 no controllers found\n");
		goto errout;
	}
	mI_Controller = calloc(mI_ControllerCount, sizeof(*mI_Controller));
	if (!mI_Controller) {
		fprintf(stderr, "no memory to allocate %d controller struct (a %zd)\n",
			mI_ControllerCount, sizeof(*mI_Controller));
		goto errout;
	}
	for (i = 0; i < mI_ControllerCount; i++) {
		pc = &mI_Controller[i];
		pc->mNr = i;
		pc->devinfo.id = i;
		pc->enable = 1;	/* default all controllers are enabled */
		pc->L3Proto = L3_PROTOCOL_DSS1_USER;
		pc->L3Flags = 0;
		ret = init_cobj(&pc->cobjLC, NULL, Cot_Root, i, 0);
		if (ret) {
			fprintf(stderr, "Cannot init LC lock for controller %d ret:%d - %s\n", i + 1, ret, strerror(ret));
			goto errout;
		}
		ret = init_cobj(&pc->cobjPLCI, NULL, Cot_Root, i, 0);
		if (ret) {
			fprintf(stderr, "Cannot init PLCI lock for controller %d ret:%d - %s\n", i + 1, ret, strerror(ret));
			goto errout;
		}
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
			pc->BInstances[j].cpipe[0] = -1;
			pc->BInstances[j].cpipe[1] = -1;
			pthread_mutex_init(&pc->BInstances[j].lock, NULL);
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
	ret = mkdir(MISDN_CAPI_SOCKET_DIR, S_IRWXU | S_IRWXG);
	if (ret < 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "cannot create socket directory %s - %s\n", MISDN_CAPI_SOCKET_DIR, strerror(errno));
			goto errout;
		} else
			iprint("directory %s already exist - reusing it\n", MISDN_CAPI_SOCKET_DIR);
	}
	sprintf(mcaddr.sun_path, "%s/%s", MISDN_CAPI_SOCKET_DIR, MISDN_CAPI_SOCKET_NAME);
	ret = bind(mCsock, (struct sockaddr *)&mcaddr, sizeof(mcaddr));
	if (ret < 0) {
		fprintf(stderr, "cannot bind socket to %s - %s\n", mcaddr.sun_path, strerror(errno));
		if (errno == EADDRINUSE) {	/* old socket file exist */
			ret = connect(mCsock, (struct sockaddr *)&mcaddr, sizeof(mcaddr));
			if (ret < 0) {
				/* seems the socket file is not in use */
				ret = unlink(mcaddr.sun_path);
				if (ret < 0) {
					fprintf(stderr, "cannot remove old socket file %s - %s\n",
						mcaddr.sun_path, strerror(errno));
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
	ret = chmod(mcaddr.sun_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (ret < 0) {
		fprintf(stderr, "cannot change permissions on unix socket:%s - %s\n", mcaddr.sun_path, strerror(errno));
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
	memset(&mysig_term, 0, sizeof(mysig_term));
	mysig_term.sa_handler = termHandler;
	ret = sigaction(SIGTERM, &mysig_term, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGTERM - %s\n", strerror(errno));
	}
	ret = sigaction(SIGINT, &mysig_term, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGINT - %s\n", strerror(errno));
	}

	memset(&mysig_dump, 0, sizeof(mysig_dump));
	mysig_dump.sa_handler = dumpHandler;
	ret = sigaction(SIGUSR1, &mysig_dump, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGUSR1 - %s\n", strerror(errno));
	}
	ret = sigaction(SIGUSR2, &mysig_dump, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGUSR2 - %s\n", strerror(errno));
	}

	memset(&mysig_hup, 0, sizeof(mysig_hup));
	mysig_hup.sa_handler = hupHandler;
	ret = sigaction(SIGHUP, &mysig_hup, NULL);
	if (ret) {
		wprint("Error to setup signal handler for SIGHUP - %s\n", strerror(errno));
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
	CAPIobj_exit();
	if (mCsock > -1)
		close(mCsock);
	close(mIsock);
	if (mI_Controller)
		free(mI_Controller);
	if (mICAPItimer_base->tdev > 0)
		close(mICAPItimer_base->tdev);
	mICAPItimer_base->tdev = -1;
	if (mcaddr.sun_path[0])
		unlink(mcaddr.sun_path);
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
