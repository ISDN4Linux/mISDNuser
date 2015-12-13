/*
 *
 * Copyright 2015 Karsten Keil <keil@b1-systems.de>
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
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <mISDN/q931.h>
#include <mISDN/suppserv.h>
#include "logger.h"
#include "../lib/include/debug.h"

#ifndef DEF_CONFIG_FILE
#define DEF_CONFIG_FILE	"/etc/misdnlogger.conf"
#endif

#define MISDNLOGGER_VERSION	"0.9"

static int running = 0;
static int do_daemon = 1;
static unsigned int ml_debugmask = 0;

static char def_config[] = DEF_CONFIG_FILE;
static char *config_file = NULL;

int mI_ControllerCount;

static struct mController *mI_Controller;

static struct sigaction mysig_term, mysig_dump;

struct mController *defController;


const char *mTypesStr[] = {
	"UNKNOWN",
	"ALERTING",
	"CALL_PROCEEDING",
	"CONNECT",
	"CONNECT_ACKNOWLEDGE",
	"PROGRESS",
	"SETUP",
	"SETUP_ACKNOWLEDGE",
	"RESUME",
	"RESUME_ACKNOWLEDGE",
	"RESUME_REJECT",
	"SUSPEND",
	"SUSPEND_ACKNOWLEDGE",
	"SUSPEND_REJECT",
	"USER_INFORMATION",
	"DISCONNECT",
	"RELEASE",
	"RELEASE_COMPLETE",
	"RESTART",
	"RESTART_ACKNOWLEDGE",
	"SEGMENT",
	"CONGESTION_CONTROL",
	"INFORMATION",
	"FACILITY",
	"NOTIFY",
	"STATUS",
	"STATUS_ENQUIRY",
	"HOLD",
	"HOLD_ACKNOWLEDGE",
	"HOLD_REJECT",
	"RETRIEVE",
	"RETRIEVE_ACKNOWLEDGE",
	"RETRIEVE_REJECT",
	"REGISTER",
	"ALL"
};



/**********************************************************************
Signal handler for clean shutdown
***********************************************************************/
static void
termHandler(int sig)
{
	iprint("Terminating on signal %d -- request shutdown mISDNlogger\n", sig);
	running = 0;
	return;
}


/**********************************************************************
Signal handler for dumping state info
***********************************************************************/
static void
dumpHandler(int sig)
{
	iprint("Received  signal %d -- start dumping\n", sig);
}



static void usage(void)
{
	fprintf(stderr, "Usage: mISDNlogger [OPTIONS]\n");
	fprintf(stderr, "  Options are\n");
	fprintf(stderr, "   -?, --help                     this help\n");
	fprintf(stderr, "   -c, --config <file>            use this config file - default %s\n", def_config);
	fprintf(stderr, "   -d, --debug <level>            extra debug\n");
	fprintf(stderr, "   -f, --foreground               run in forground, not as daemon\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "mISDNlogger Version %s\n", MISDNLOGGER_VERSION);
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
			{"debug", 1, 0, 'd'},
			{"foreground", 0, 0, 'f'},
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "?c:f", long_options, &option_index);
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
		case 'd':
			if (optarg) {
				errno = 0;
				ml_debugmask = (unsigned int)strtoul(optarg, NULL, 0);
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

extern int parse_config(FILE *);

static int read_config_file(char *name)
{
	FILE *f;
	int ret;

	f = fopen(name, "r");
	if (!f) {
		fprintf(stderr, "cannot open %s - %s\n", name, strerror(errno));
		return -1;
	} else {
		ret = parse_config(f);
	}
	fclose(f);
	return ret;
}

static int my_lib_debug(const char *file, int line, const char *func, int level, const char *fmt, va_list va)
{
	int ret, ll;
	char log[512];

	ret = vsnprintf(log, 512, fmt, va);
	if (do_daemon) {
		switch(level) {
		case 1:
			ll = LOG_ERR;
			break;
		case 2:
			ll = LOG_WARNING;
			break;
		case 3:
			ll = LOG_INFO;
			break;
		default:
			ll = LOG_DEBUG;
			break;
		}
		syslog(ll, "%s:%d %s\n", file, line, log);
	} else {
		fprintf(stderr, "%s:%d %s\n", file, line, log);
	}
	return ret;
}

static struct mi_ext_fn_s myfn = {
	.prt_debug = my_lib_debug,
};



static int log_output(struct mController *mc)
{
	struct tm *mt;
	char log[32];

	if (mc->lp == mc->logtxt) /* nothing to log */
		return 0;
	if (mc->tv) {
		mt = localtime((time_t *) &mc->tv->tv_sec);
		snprintf(log, 32, "Contr%d: %02d:%02d:%02d.%06ld",
			          mc->mNr, mt->tm_hour, mt->tm_min, mt->tm_sec, mc->tv->tv_usec);
	} else
		snprintf(log, 32, "Contr%d:", mc->mNr);
	if (do_daemon) {
		if (mc->syslog > 0)
			syslog(mc->syslog, "%s %s\n", log, mc->logtxt);
	} else if (mc->syslog > 0) {
		fprintf(stderr, "%s %s\n", log, mc->logtxt);
		fflush(stderr);
	}
	if (mc->log) {
		fprintf(mc->log, "%s %s\n", log, mc->logtxt);
		fflush(mc->log);
	}
	return 0;
}

static int open_logfile(void)
{
	int i, err;
	struct mController *mc;

	for (i = 0; i < mI_ControllerCount; i++) {
		mc = &mI_Controller[i];
		if (mc->logfile[0]) { /* logfilename set */
			if (strcmp(mc->logfile, defController->logfile)) {
				/* own name */
				mc->log = fopen(mc->logfile, "w");
				if (!mc->log) {
					err = errno;
					eprint("Cannot open log file %s - %s\n", mc->logfile, strerror(err));
					return -1;
				}
			} else { /* same name as default - only use one logfile */
				if (!defController->log) {
					mc->log = fopen(mc->logfile, "w");
					if (!mc->log) {
						err = errno;
						eprint("Cannot open log file %s - %s\n", mc->logfile, strerror(err));
						return -1;
					} else
						defController->log = mc->log;
				} else
					mc->log = defController->log;
			}
		}
	}
	return 0;
}

static void close_logfile(void)
{
	int i;
	FILE *log;

	for (i = 0; i < mI_ControllerCount; i++) {
		log = mI_Controller[i].log;
		if (log) {
			mI_Controller[i].log = NULL;
			if (defController->log == log)
				continue;
			fclose(log);
		}
	}
	if (defController->log) {
		fclose(defController->log);
		defController->log = NULL;
	}
}

static int logprint(struct mController *mc, const char *fmt, ...)
{
	int rest, ret = 0;
	va_list	args;

	va_start(args, fmt);
	rest = LOGTXT_SIZE - (mc->lp - mc->logtxt);
	ret = vsnprintf(mc->lp, rest, fmt, args);
	if (ret > rest) {
		ret = -1;
		mc->lp += rest;
		mc->logtxt[LOGTXT_SIZE - 1] = 0;
	} else
		mc->lp += ret;
	va_end(args);
	return ret;
}

/*
 * fmt 0 log each octet without space
 * fmt 1 log each octet with one space before
 * fmt n log each octet with one space before and a extra space every n octets
 */
static void loghex(struct mController *mc, const char *head, unsigned char *p, int len, int fmt, int ie_idx)
{
	int i, rest, n = 1;

	if (head)
		if (0 > logprint(mc, "%s", head))
			return;

	if ((ie_idx >= 0) && (mc->layer3[ie_idx] & l3vHEX)) {
		/* do not dump if hexdump was reqeusted too */
		mc->lp--; /* remove ':' */
		return;
	}
	rest = LOGTXT_SIZE - (mc->lp - mc->logtxt);
	while (rest > 3 && len) {
		if (fmt)
			i = snprintf(mc->lp, rest, " %02x", *p++);
		else
			i = snprintf(mc->lp, rest, "%02x", *p++);
		rest -= i;
		mc->lp += i;
		len--;
		if (fmt > 1) {
			if (rest && len && !(n % fmt)) {
				*mc->lp++ = ' ';
				*mc->lp = 0;
				rest--;
			}
			n++;
		}
	}
}

struct ctstamp {
	size_t cmsg_len;
	int cmsg_level;
	int cmsg_type;
	struct timeval tv;
};

#define RR	0x01
#define RNR	0x05
#define REJ	0x09
#define SABME	0x6f
#define SABM	0x2f
#define DM	0x0f
#define UI	0x03
#define DISC	0x43
#define UA	0x63
#define FRMR	0x87
#define XID	0xaf

#define CMD	0
#define RSP	1

static int IsUI(unsigned char *data)
{
	return (data[0] & 0xef) == UI;
}

static int IsUA(unsigned char *data)
{
	return (data[0] & 0xef) == UA;
}

static int IsDM(unsigned char *data)
{
	return (data[0] & 0xef) == DM;
}

static int IsDISC(unsigned char *data)
{
	return (data[0] & 0xef) == DISC;
}

static int IsRR(unsigned char *data)
{
	return data[0]  == RR;
}


static int IsSABME(unsigned char *data)
{
	return (data[0] & 0xef) == SABME;
}

static int IsREJ(unsigned char *data)
{
	return data[0] == REJ;
}

static int IsFRMR(unsigned char *data)
{
	return data[0] == FRMR;
}

static int IsRNR(unsigned char *data)
{
	return data[0] == RNR;
}


static const char MTidx[] = {
	MT_ALERTING,
	MT_CALL_PROCEEDING,
	MT_CONNECT,
	MT_CONNECT_ACKNOWLEDGE,
	MT_PROGRESS,
	MT_SETUP,
	MT_SETUP_ACKNOWLEDGE,
	MT_RESUME,
	MT_RESUME_ACKNOWLEDGE,
	MT_RESUME_REJECT,
	MT_SUSPEND,
	MT_SUSPEND_ACKNOWLEDGE,
	MT_SUSPEND_REJECT,
	MT_USER_INFORMATION,
	MT_DISCONNECT,
	MT_RELEASE,
	MT_RELEASE_COMPLETE,
	MT_RESTART,
	MT_RESTART_ACKNOWLEDGE,
	MT_SEGMENT,
	MT_CONGESTION_CONTROL,
	MT_INFORMATION,
	MT_FACILITY,
	MT_NOTIFY,
	MT_STATUS,
	MT_STATUS_ENQUIRY,
	MT_HOLD,
	MT_HOLD_ACKNOWLEDGE,
	MT_HOLD_REJECT,
	MT_RETRIEVE,
	MT_RETRIEVE_ACKNOWLEDGE,
	MT_RETRIEVE_REJECT,
	MT_REGISTER,
	0
};


static int log_coding(struct mController *mc, const char *head, int cstd, int loc, int typ, int plan)
{
	int ret = 0;
	if (head) {
		ret = logprint(mc, " %s:", head);
		 if (ret < 0)
		 	return -1;
	}
	if (cstd >= 0) {
		switch(cstd & 3) {
		case 0:
			ret = logprint(mc, " Std:CCITT");
			break;
		case 1:
			ret = logprint(mc, " Std:other Intnational");
			break;
		case 2:
			ret = logprint(mc, " Std:national");
			break;
		case 3:
			ret = logprint(mc, " Std:other");
			break;
		}
	}
	switch(loc) {
	case -1: /* skip */
		break;
	case 0:
		ret = logprint(mc, " Loc:user,");
		break;
	case 1:
		ret = logprint(mc, " Loc:private network serving the local User,");
		break;
	case 2:
		ret = logprint(mc, " Loc:public network serving the local user,");
		break;
	case 3:
		ret = logprint(mc, " Loc:transit network,");
		break;
	case 4:
		ret = logprint(mc, " Loc:public network serving the remote user,");
		break;
	case 5:
		ret = logprint(mc, " Loc:private network serving the remote user,");
		break;
	case 7:
		ret = logprint(mc, " Loc:international network,");
		break;
	case 10:
		ret = logprint(mc, " Loc:network beyond interworking point,");
		break;
	default:
		ret = logprint(mc, " Loc:reserved(%d),", loc);
		break;
	}
	switch(typ) {
	case -1: /* skip */
		break;
	case 0:
		ret = logprint(mc, " Type:user");
		break;
	case 2:
		ret = logprint(mc, " Type:national");
		break;
	case 3:
		ret = logprint(mc, " Type:international");
		break;
	default:
		ret = logprint(mc, " Type:reserved(%d),", typ);
		break;
	}
	switch(plan) {
	case -1: /* skip */
		break;
	case 0:
		ret = logprint(mc, " Plan:unknown,");
		break;
	case 2:
		ret = logprint(mc, " Plan:Carrier identification code");
		break;
	case 3:
		ret = logprint(mc, " Plan:Data network identification code(X.121)");
		break;
	default:
		ret = logprint(mc, " Plan:reserved(%d),", plan);
		break;
	}
	return ret;
}

static void log_bearer_capability(struct mController *mc, unsigned char *p)
{
	int ret, r, m, l, i, o;

	l = *p++;
	if (0 > log_coding(mc, "bearerCap", (*p & 0x60) >> 5, -1, -1, -1))
		return;
	if (0 > logprint(mc, " %s", mi_bearer2str(*p & 0x1f)))
		return;

	if (l < 2) {
		logprint(mc, " bearerCap too short");
		return;
	}
	l -= 2;
	/* octet 4 */
	p++;
	m = *p & 0x60;
	r = *p & 0x1f;
	if (m == 0x40) {
		if (0 > logprint(mc, " packet mode"))
			return;
	} else if (m == 0) {
		if (0 > logprint(mc, " circuit mode rate:"))
			return;
		switch(r) {
		case 0x10:
			ret = logprint(mc, "64Kbit/s");
			break;
		case 0x11:
			ret = logprint(mc, "2x64Kbit/s");
			break;
		case 0x13:
			ret = logprint(mc, "384Kbit/s");
			break;
		case 0x15:
			ret = logprint(mc, "1536Kbit/s");
			break;
		case 0x17:
			ret = logprint(mc, "1920Kbit/s");
			break;
		default:
			ret = logprint(mc, "reserved");
			break;
		}
		if (ret < 0)
			return;
	} else {
		if (0 > logprint(mc, " undefined mode"))
			return;
	}
	if (l && !(*p & 0x80)) {
		/* octet 4a */
		l--;
		p++;
		if (0 > logprint(mc, " octet4a=0x%02x"))
			return;
		if (l && !(*p & 0x80)) {
			/* octet 4b */
			l--;
			p++;
			if (0 > logprint(mc, " octet4a=0x%02x"))
				return;
		}
	}
	/* octet 5 */
	while (l > 0) {
		l--;
		p++;
		i = (*p & 0x60) >> 5;
		m = *p & 0x1f;
		if (0 > logprint(mc, " L%d ", i))
			return;
		if (i == 1) {
			switch(m) {
			case 1:
				ret = logprint(mc, "protocol V.110/X.30");
				break;
			case 2:
				ret = logprint(mc, "protocol G.711 ulaw");
				break;
			case 3:
				ret = logprint(mc, "protocol G.711 Alaw");
				break;
			case 4:
				ret = logprint(mc, "protocol G.721 32kbit/s ADPCM");
				break;
			case 5:
				ret = logprint(mc, "protocol G.722 7kHz audio");
				break;
			case 6:
				ret = logprint(mc, "protocol G.7xx 384 kbit/s video");
				break;
			case 7:
				ret = logprint(mc, "none CCITT rate adaption");
				break;
			case 9:
				ret = logprint(mc, "protocol G.721 32kbit/s ADPCM");
				break;
			default:
				ret = logprint(mc, "protocol reserved (%d)", m);
				break;
			}
			if (ret < 0)
				return;
			o = 'a';
			while((l > 0) && !(*p & 0x80)) {
				l--;
				p++;
				if (0 > logprint(mc, " octet5%c=0x%02x", o, *p))
					return;
				o++;
			}
		} else if (i == 2) {
			switch(m) {
			case 2:
				logprint(mc, "protocol Q.921");
				break;
			case 6:
				logprint(mc, "protocol X.25");
				break;
			default:
				logprint(mc, "protocol reserved (%d)", m);
				break;
			}
		} else if (i == 3) {
			switch(m) {
			case 2:
				logprint(mc, "protocol Q.931");
				break;
			case 6:
				logprint(mc, "protocol X.25");
				break;
			default:
				logprint(mc, "protocol reserved (%d)", m);
				break;
			}
		} else {
			logprint(mc, "invalid");
		}
	}
}

static void log_cause(struct mController *mc, unsigned char *p)
{
	int ret, c, loc, l = *p++;

	c = (*p & 0x60) >> 5;
	loc = *p & 0xf;
	l--;
	ret = log_coding(mc, "cause", c, loc, -1, -1);
	if (ret < 0)
		return;
	if (ret < 0)
		return;
	if (l && !(*p & 0x80)) {
		/* octet 3a */
		l--;
		p++;
		c = *p & 0x7f;
		switch(c) {
		case 0:
			ret = logprint(mc, " Q.931");
			break;
		case 3:
			ret = logprint(mc, " X.21");
			break;
		case 4:
			ret = logprint(mc, " X.25");
			break;
		default:
			ret = logprint(mc, " reserved recommendation(%d)", c);
			break;
		}
		if (ret < 0)
			return;
	}
	if (l) {
		p++;
		l--;
		c = *p++ & 0x7f;
		switch(c) {
		case 0x01: ret = logprint(mc, " 'Unallocated (unassigned) number'"); break;
		case 0x02: ret = logprint(mc, " 'No route to specified transit network'"); break;
		case 0x03: ret = logprint(mc, " 'No route to destination'"); break;
		case 0x06: ret = logprint(mc, " 'Channel unacceptable'"); break;
		case 0x07: ret = logprint(mc, " 'Call awarded and being delivered in an established channel'"); break;
		case 0x10: ret = logprint(mc, " 'Normal call clearing'"); break;
		case 0x11: ret = logprint(mc, " 'User busy'"); break;
		case 0x12: ret = logprint(mc, " 'No user responding'"); break;
		case 0x13: ret = logprint(mc, " 'No answer from user (user alerted)'"); break;
		case 0x15: ret = logprint(mc, " 'Call rejected'"); break;
		case 0x16: ret = logprint(mc, " 'Number changed'"); break;
		case 0x1A: ret = logprint(mc, " 'Non-selected user clearing'"); break;
		case 0x1B: ret = logprint(mc, " 'Destination out of order'"); break;
		case 0x1C: ret = logprint(mc, " 'Invalid number format'"); break;
		case 0x1D: ret = logprint(mc, " 'Facility rejected'"); break;
		case 0x1E: ret = logprint(mc, " 'Response to STATUS ENQUIRY'"); break;
		case 0x1F: ret = logprint(mc, " 'Normal, unspecified'"); break;
		case 0x22: ret = logprint(mc, " 'No circuit / channel available'"); break;
		case 0x26: ret = logprint(mc, " 'Network out of order'"); break;
		case 0x29: ret = logprint(mc, " 'Temporary failure'"); break;
		case 0x2A: ret = logprint(mc, " 'Switching equipment congestion'"); break;
		case 0x2B: ret = logprint(mc, " 'Access information discarded'"); break;
		case 0x2C: ret = logprint(mc, " 'Requested circuit / channel not available'"); break;
		case 0x2F: ret = logprint(mc, " 'Resources unavailable, unspecified'"); break;
		case 0x31: ret = logprint(mc, " 'Quality of service unavailable'"); break;
		case 0x32: ret = logprint(mc, " 'Requested facility not subscribed'"); break;
		case 0x39: ret = logprint(mc, " 'Bearer capability not authorized'"); break;
		case 0x3A: ret = logprint(mc, " 'Bearer capability not presently available'"); break;
		case 0x3F: ret = logprint(mc, " 'Service or option not available, unspecified'"); break;
		case 0x41: ret = logprint(mc, " 'Bearer capability not implemented'"); break;
		case 0x42: ret = logprint(mc, " 'Channel type not implemented'"); break;
		case 0x45: ret = logprint(mc, " 'Requested facility not implemented'"); break;
		case 0x46: ret = logprint(mc, " 'Only restricted digital information bearer capability is available'"); break;
		case 0x4F: ret = logprint(mc, " 'Service or option not implemented, unspecified'"); break;
		case 0x51: ret = logprint(mc, " 'Invalid call reference value'"); break;
		case 0x52: ret = logprint(mc, " 'Identified channel does not exist'"); break;
		case 0x53: ret = logprint(mc, " 'A suspended call exists, but this call identity does not'"); break;
		case 0x54: ret = logprint(mc, " 'Call identity in use'"); break;
		case 0x55: ret = logprint(mc, " 'No call suspended'"); break;
		case 0x56: ret = logprint(mc, " 'Call having the requested call identity has been cleared'"); break;
		case 0x58: ret = logprint(mc, " 'Incompatible destination'"); break;
		case 0x5B: ret = logprint(mc, " 'Invalid transit network selection'"); break;
		case 0x5F: ret = logprint(mc, " 'Invalid message, unspecified'"); break;
		case 0x60: ret = logprint(mc, " 'Mandatory information element is missing'"); break;
		case 0x61: ret = logprint(mc, " 'Message type non-existent or not implemented'"); break;
		case 0x62: ret = logprint(mc, " 'Message not compatible with call state or message type non-existent or not implemented'"); break;
		case 0x63: ret = logprint(mc, " 'Information element non-existent or not implemented'"); break;
		case 0x64: ret = logprint(mc, " 'Invalid information element contents'"); break;
		case 0x65: ret = logprint(mc, " 'Message not compatible with call state'"); break;
		case 0x66: ret = logprint(mc, " 'Recovery on timer expiry'"); break;
		case 0x6F: ret = logprint(mc, " 'Protocol error, unspecified'"); break;
		case 0x7F: ret = logprint(mc, " 'Interworking, unspecified'"); break;
		default:
			ret = logprint(mc, " 'unknown cause (%d)'", c);
			break;
		}
		if (ret < 0)
			return;
	} else {
		logprint(mc, " no Value");
		return;
	}
	if (l)
		loghex(mc, " diagnostic:", p, l, 0, -1);
}

static void log_call_state(struct mController *mc, unsigned char *p, struct l3_msg *l3m)
{
	char s;
	int ret, c, st;

	p++;
	c = (*p & 0x60) >> 5;
	st = *p & 0x1f;
	ret = log_coding(mc, "callState", c, -1, -1, -1);
	if (ret < 0)
		return;
	if (mc->protocol == ISDN_P_NT_S0 || mc->protocol == ISDN_P_NT_E1)
		s = 'N';
	else
		s = 'U';
	if (st > 0 && st < 26)
		logprint(mc, " %c%d", s, st);
	else if (st == 0) {
		if (l3m->pid == MISDN_PID_GLOBAL)
			logprint(mc, " REST 0");
		else
			logprint(mc, " %c0", s);
	} else if (l3m->pid != MISDN_PID_GLOBAL)
		logprint(mc, " unknown state %c%d", s, st);
	else if (st == 0x1d || st == 0x1e)
		logprint(mc, " REST %d", st & 3);
	else
		logprint(mc, " unknown state REST %d", st);
}

static void log_channel_id(struct mController *mc, unsigned char *p)
{
	unsigned char ift, excl, dch;
	const char *cu;
	int ret, ch, st, l = *p++;

	ift = *p & 0x20;
	excl = *p & 0x08;
	dch = *p & 0x04;
	ch = *p & 0x3;
	l--;
	ret = log_coding(mc, "channelID", -1, -1, -1, -1);
	if (ret < 0)
		return;
	ret = logprint(mc, " %s,%s", ift ? "primary" : "basic", excl ? "exclusiv" : "prefered");
	if (ret < 0)
		return;
	if (dch) {
		ret = logprint(mc, ",D channel");
	} else if (!ift) {
		switch(ch) {
		case 0:
			ret = logprint(mc, ",no channel");
			break;
		case 1:
		case 2:
			ret = logprint(mc, ",B%d channel", ch);
			break;
		case 3:
			ret = logprint(mc, ",any channel");
			break;
		}
	} else if (l < 2) {
		p++;
		l--;
		st = *p++;
		ch = *p;
		ret = log_coding(mc, NULL, (*p & 0x60) >> 5, -1, -1, -1);
		if (ret < 0)
			return;
		switch(st & 0xf) {
		case 3:
			cu = ",B";
			break;
		case 6:
			cu = ",H0";
			break;
		case 8:
			cu = ",H11";
			break;
		case 9:
			cu = ",H12";
			break;
		default:
			cu = ",unknown channel units";
			 break;
		}
		logprint(mc, " %s %d", cu, ch & 0x7f);
	} else
		logprint(mc, " channelIE too short");
}

struct _lookup_table {
	int v;
	const char *s;
};

static struct _lookup_table ets_asn1_operations[] = {
	{Fac_MaliciousCallId, "MaliciousCallId"},
	{Fac_Begin3PTY, "Begin3PTY"},
	{Fac_End3PTY, "End3PTY"},
	{Fac_ActivationDiversion, "ActivationDiversion"},
	{Fac_DeactivationDiversion, "DeactivationDiversion"},
	{Fac_ActivationStatusNotificationDiv, "ActivationStatusNotificationDiv"},
	{Fac_DeactivationStatusNotificationDiv, "DeactivationStatusNotificationDiv"},
	{Fac_InterrogationDiversion, "InterrogationDiversion"},
	{Fac_DiversionInformation, "DiversionInformation"},
	{Fac_CallDeflection, "CallDeflection"},
	{Fac_CallRerouteing, "CallRerouteing"},
	{Fac_DivertingLegInformation2, "DivertingLegInformation2"},
	{Fac_InterrogateServedUserNumbers, "InterrogateServedUserNumbers"},
	{Fac_DivertingLegInformation1, "DivertingLegInformation1"},
	{Fac_DivertingLegInformation3, "DivertingLegInformation3"},
	{Fac_ChargingRequest, "ChargingRequest"},
	{Fac_AOCSCurrency, "AOCSCurrency"},
	{Fac_AOCSSpecialArr, "AOCSSpecialArr"},
	{Fac_AOCDCurrency, "AOCDCurrency"},
	{Fac_AOCDChargingUnit, "AOCDChargingUnit"},
	{Fac_AOCECurrency, "AOCECurrency"},
	{Fac_AOCEChargingUnit, "AOCEChargingUnit"},
	{Fac_EctExecute, "EctExecute"},
	{Fac_ExplicitEctExecute, "ExplicitEctExecute"},
	{Fac_RequestSubaddress, "RequestSubaddress"},
	{Fac_SubaddressTransfer, "SubaddressTransfer"},
	{Fac_EctLinkIdRequest, "EctLinkIdRequest"},
	{Fac_EctInform, "EctInform"},
	{Fac_EctLoopTest, "EctLoopTest"},
	{Fac_StatusRequest, "StatusRequest"},
	{Fac_CallInfoRetain, "CallInfoRetain"},
	{Fac_CCBSRequest, "CCBSRequest"},
	{Fac_CCBSDeactivate, "CCBSDeactivate"},
	{Fac_CCBSInterrogate, "CCBSInterrogate"},
	{Fac_CCBSErase, "CCBSErase"},
	{Fac_CCBSRemoteUserFree, "CCBSRemoteUserFree"},
	{Fac_CCBSCall, "CCBSCall"},
	{Fac_CCBSStatusRequest, "CCBSStatusRequest"},
	{Fac_CCBSBFree, "CCBSBFree"},
	{Fac_EraseCallLinkageID, "EraseCallLinkageID"},
	{Fac_CCBSStopAlerting, "CCBSStopAlerting"},
	{Fac_CCBS_T_Request, "CCBS_T_Request"},
	{Fac_CCBS_T_Call, "CCBS_T_Call"},
	{Fac_CCBS_T_Suspend, "CCBS_T_Suspend"},
	{Fac_CCBS_T_Resume, "CCBS_T_Resume"},
	{Fac_CCBS_T_RemoteUserFree, "CCBS_T_RemoteUserFree"},
	{Fac_CCBS_T_Available, "CCBS_T_Available"},
	{Fac_CCNRRequest, "CCNRRequest"},
	{Fac_CCNRInterrogate, "CCNRInterrogate"},
	{Fac_CCNR_T_Request, "CCNR_T_Request"},
	{-1, "unknown operation"}
};

static const char *get_lookup_str(int val, struct _lookup_table *tab)
{
	struct _lookup_table *item = tab;

	while(item->v != -1) {
		if (item->v == val)
			break;
		item++;
	}
	return item->s;
}

static void log_facility(struct mController *mc, unsigned char *p, int idx)
{
	const char *cp, *ops;
	const char *opstr;
	int id, op, ret = 0, l = *p;
	struct asn1_parm ap;

	if (0 > logprint(mc, " facility:"))
		return;
	ret = decodeFac(p, &ap);
	p++;
	if (ret < 0) {
		if (0 > logprint(mc, " error %d on decode", ret))
			return;
		if (mc->layer3[idx] & l3vHEX)
			ret = logprint(mc, " not dumped");
		else
			loghex(mc, "hex:", p, l, 4, -1);
	} else {
		ret = logprint(mc, " decode OK %s", ap.Valid ? "and valid": "but invalid");
		if (ret < 0)
			return;
		if (ap.Valid) {
			cp = "unknown";
			ops = cp;
			id = -1;
			op = -1;
			opstr = "";
			switch(ap.comp) {
			case CompInvoke:
				cp = "Invoke";
				id = ap.u.inv.invokeId;
				op = ap.u.inv.operationValue;
				ops = "operation";
				opstr = get_lookup_str(op, ets_asn1_operations);
				break;
			case CompReturnResult:
				cp = "ReturnResult";
				id = ap.u.retResult.invokeId;
				op = ap.u.retResult.operationValue;
				opstr = get_lookup_str(op, ets_asn1_operations);
				ops = "operation";
				break;
			case CompReturnError:
				cp = "ReturnError";
				id = ap.u.retError.invokeId;
				op = ap.u.retError.errorValue;
				ops = "error";
				break;
			case CompReject:
				cp = "Reject";
				id = ap.u.reject.invokeId;
				op = ap.u.reject.problemValue;
				ops = "problem";
				break;
			}
			logprint(mc, " %s id=%d %s %x %s", cp, id, ops, op, opstr);
		}
	}
}

static void log_progress(struct mController *mc, unsigned char *p)
{
	int ret, c, loc, l = *p++;

	c = (*p & 0x60) >> 5;
	loc = *p & 0xf;
	ret = log_coding(mc, "progress", c, loc, -1, -1);
	if (ret < 0)
		return;
	if (!l)
		logprint(mc, " too short");
	else {
		p++;
		switch(*p & 0x7f) {
		case 1:
			logprint(mc, "'Call is not end-to-end ISDN: further progress information may be available in-band'");
			break;
		case 2:
			logprint(mc, "'Destination address is non-ISDN'");
			break;
		case 3:
			logprint(mc, "'Origination address is non-ISDN'");
			break;
		case 4:
			logprint(mc, "'Call has returned to the ISDN'");
			break;
		case 5:
			logprint(mc, "'Interworking has occurred and has resulted in a telecommunication service change'");
			break;
		case 8:
			logprint(mc, "'In-band information or appropriate pattern now available'");
			break;
		default:
			logprint(mc, "'unknown progress description: %02x'", *p & 0x7f);
			break;
		}
	}
}

static void log_net_fac(struct mController *mc, unsigned char *p)
{
	int ret = 0, t, pl, il, l = *p++;

	il = *p++;
	l--;
	if (il) {
		t = (*p & 70) >> 4;
		pl = *p & 0xf;
		il--;
		l--;
		ret = log_coding(mc, "networkSpecificFacility", -1, -1, t, pl);
		if (ret < 0)
			return;
		p++;
		while (il && l) {
			ret = logprint(mc, "%c", *p++ & 0x7f);
			if (ret < 0)
				return;
			il--;
			l--;
		}
	} else {
		ret = logprint(mc, " networkSpecificFacility: ");
		if (ret < 0)
			return;
	}
	if (l)
		loghex(mc, "content:", p, l, 0, -1);
}

static void log_notify(struct mController *mc, unsigned char *p)
{
	p++;
	switch(*p & 0x7f) {
	case 0:
		logprint(mc, " notify:'user suspended'");
		break;
	case 1:
		logprint(mc, " notify:'user resumed'");
		break;
	case 2:
		logprint(mc, " notify:'bearer service changed'");
		break;
	case 3:
		logprint(mc, " notify:'BER encoded extension'");
		break;
	case 4:
		logprint(mc, " notify:'Call completion delay'");
		break;
	case 0x42:
		logprint(mc, " notify:'Conference established'");
		break;
	case 0x43:
		logprint(mc, " notify:'Conference disconnected'");
		break;
	case 0x44:
		logprint(mc, " notify:'Other party added'");
		break;
	case 0x45:
		logprint(mc, " notify:'Isolated'");
		break;
	case 0x46:
		logprint(mc, " notify:'Reattached'");
		break;
	case 0x47:
		logprint(mc, " notify:'Other party isolated'");
		break;
	case 0x48:
		logprint(mc, " notify:'Other party reattached'");
		break;
	case 0x49:
		logprint(mc, " notify:'Other party split'");
		break;
	case 0x4a:
		logprint(mc, " notify:'Other party disconnected'");
		break;
	case 0x60:
		logprint(mc, " notify:'Call is a waiting call'");
		break;
	case 0x68:
		logprint(mc, " notify:'Diversion activated'");
		break;
	case 0x69:
		logprint(mc, " notify:'Call transferred, alerting'");
		break;
	case 0x6a:
		logprint(mc, " notify:'Call transferred, active'");
		break;
	case 0x79:
		logprint(mc, " notify:'Remote hold'");
		break;
	case 0x7a:
		logprint(mc, " notify:'Remote retrieval'");
		break;
	case 0x7b:
		logprint(mc, " notify:'Call is diverting'");
		break;
	default:
		logprint(mc, " notify:'unknown description: %02x'", *p & 0x7f);
		break;
	}
}

static void log_ia5_chars(struct mController *mc, const char *head, unsigned char *p, int len)
{
	int ret;

	if (head) {
		ret = logprint(mc, "%s", head);
		if (ret < 0)
			return;
	}
	while (len) {
		ret = logprint(mc, "%c", *p++ & 0x7f);
		if (ret < 0)
			return;
		len--;
	}
}

static void log_date(struct mController *mc, unsigned char *p)
{
	int l = *p++;

	if  (l > 5)
		logprint(mc, " date:'%02d.%02d.%02d %02d:%02d:%02d'", p[0], p[1], p[2], p[3], p[4], p[5]);
	else
		logprint(mc, " date:'%02d.%02d.%02d %02d:%02d'", p[0], p[1], p[2], p[3], p[4]);
	return;
}

static void log_number(struct mController *mc, int ie, const char *head, unsigned char *p)
{
	const char *ts, *ps;
	int ret = 0, t, pl, len = *p++;

	if (head) {
		ret = logprint(mc, "%s", head);
		if (ret < 0)
			return;
	}
	t = (*p & 0x70) >> 4;
	pl = *p & 0x0f;
	switch(t) {
	case 0:
		ts = "Unknown";
		break;
	case 1:
		ts = "International";
		break;
	case 2:
		ts = "National";
		break;
	case 3:
		ts = "Network specific";
		break;
	case 4:
		ts = "Subscriber";
		break;
	case 6:
		ts = "Abbreviated";
		break;
	case 7:
		ts = "reserved for ext.";
		break;
	default:
		ts = "reserved";
		break;
	}
	
	switch(pl) {
	case 0:
		ps = "Unknown";
		break;
	case 1:
		ps = "ISDN/Telephony";
		break;
	case 3:
		ps = "Data";
		break;
	case 4:
		ps = "Telex";
		break;
	case 8:
		ps = "National";
		break;
	case 9:
		ps = "Private";
		break;
	case 15:
		ps = "reserved for ext.";
		break;
	default:
		ps = "reserved";
		break;
	}
	ret = logprint(mc, "Type:%s Plan:%s ", ts, ps);
	if (ret < 0)
		return;
	len--;
	if (len && !(*p & 0x80)) {
		len--;
		p++;
		t = (*p & 0x60) >> 5;
		pl = *p & 3;
		switch(t) {
		case 0:
			ts = "presentation:allowed";
			break;
		case 1:
			ts = "presentation:restricted";
			break;
		case 2:
			ts = "number not available";
			break;
		case 3:
			ts = "reserved";
			break;
		}
		if ((ie == IE_REDIRECTION_NR) || (ie == IE_REDIRECTING_NR)) /* no screening */
			ret = logprint(mc, "%s ", ts);
		else {
			switch(pl) {
			case 0:
				ps = "user provided,not screened";
				break;
			case 1:
				ps = "user provided,verified and passed";
				break;
			case 2:
				ps = "user provided,verified and failed";
				break;
			case 3:
				ps = "network provided";
				break;
			}
			ret = logprint(mc, "%s,%s ", ts, ps);
		}
		if (ret < 0)
			return;
		if (len && !(*p & 0x80)) {
			/* reason in IE_REDIRECTING_NR */
			len--;
			p++;
			switch(*p & 0xf){
			case 0:
				ps = "Unknown";
				break;
			case 1:
				ps = "Call forwarding busy";
				break;
			case 2:
				ps = "Call forwarding no reply";
				break;
			case 10:
				ps = "Call deflection";
				break;
			case 15:
				ps = "Call forwarding unconditional";
				break;
			default:
				ps = "reserved";
			}
			ret = logprint(mc, "reason:%s ", ps);
			if (ret < 0)
				return;
		}
	}
	p++;
	while (len > 0) {
		ret = logprint(mc, "%c", *p++ & 0x7f);
		if (ret < 0)
			return;
		len--;
	}
}

static void log_subaddress(struct mController *mc, const char *head, unsigned char *p)
{
	int ret = 0, t, odd, len = *p++;

	if (head) {
		ret = logprint(mc, "%s", head);
		if (ret < 0)
			return;
	}
	t = (*p & 0x70) >> 4;
	odd = *p & 0x08;
	switch(t) {
	case 0:
		ret = logprint(mc, "Type:NSAP ");
		break;
	case 2:
		ret = logprint(mc, "Type:user specified, %s ", odd ? "odd" : "even");
		break;
	default:
		ret = logprint(mc, "Type:reserved(%d) ", t);
		break;
	}
	if (ret < 0)
		return;
	len--;
	p++;
	if (len) {
		ret = logprint(mc, "%c", *p++ & 0x7f);
		if (ret < 0)
			return;
		len--;
	}
}

static void log_signal(struct mController *mc, unsigned char *p)
{
	p++;
	switch(*p) {
	case 0:
		logprint(mc, " signal:'dial tone on'");
		break;
	case 1:
		logprint(mc, " signal:'ringback tone on'");
		break;
	case 2:
		logprint(mc, " signal:'intercept tone on'");
		break;
	case 3:
		logprint(mc, " signal:'network congestion tone on'");
		break;
	case 4:
		logprint(mc, " signal:'busy tone on'");
		break;
	case 5:
		logprint(mc, " signal:'confirm tone on'");
		break;
	case 6:
		logprint(mc, " signal:'answer tone on'");
		break;
	case 7:
		logprint(mc, " signal:'call waiting tone on'");
		break;
	case 8:
		logprint(mc, " signal:'off-hook warning tone on'");
		break;
	case 0x3f:
		logprint(mc, " signal:'tones off'");
		break;
	case 0x40:
		logprint(mc, " signal:'alerting on - pattern 0'");
		break;
	case 0x41:
		logprint(mc, " signal:'alerting on - pattern 1'");
		break;
	case 0x42:
		logprint(mc, " signal:'alerting on - pattern 2'");
		break;
	case 0x43:
		logprint(mc, " signal:'alerting on - pattern 3'");
		break;
	case 0x44:
		logprint(mc, " signal:'alerting on - pattern 4'");
		break;
	case 0x45:
		logprint(mc, " signal:'alerting on - pattern 5'");
		break;
	case 0x46:
		logprint(mc, " signal:'alerting on - pattern 6'");
		break;
	case 0x47:
		logprint(mc, " signal:'alerting on - pattern 7'");
		break;
	case 0x4f:
		logprint(mc, " signal:'alerting off'");
		break;
	default:
		logprint(mc, " signal:'unknown code: %02x'", *p);
		break;
	}
}

static void log_transit_net_sel(struct mController *mc, unsigned char *p)
{
	int ret = 0, t, pl, l = *p++;

	t = (*p & 70) >> 4;
	pl = *p & 0xf;
	l--;
	ret = log_coding(mc, "transitNetworkSelection", -1, -1, t, pl);
	if (ret < 0)
		return;
	p++;
	while (l) {
		ret = logprint(mc, "%c", *p++ & 0x7f);
		if (ret < 0)
			return;
		l--;
	}
}

static void log_restart_ind(struct mController *mc, unsigned char *p)
{
	p++;
	switch(*p & 0x7f) {
	case 0:
		logprint(mc, " Restart: indicated interfaces");
		break;
	case 1:
		logprint(mc, " Restart: single interface");
		break;
	case 2:
		logprint(mc, " Restart: all interfaces");
		break;
	default:
		logprint(mc, " Restart:'reserved class( %02x)", *p & 0x7f);
		break;
	}
}

static struct _lookup_table hlc_ids[] = {
	{0x001, "Telephony"},
	{0x004, "Facsimile Group 2/3"},
	{0x021, "Facsimile Group 4 Class 1"},
	{0x024, "Teletex service, basic and mixed mode of operation"},
	{0x028, "Teletex service, basic and processable mode of operation"},
	{0x031, "Teletex service, basic mode of operation"},
	{0x032, "Syntax based Videotex"},
	{0x033, "International Videotex interworking"},
	{0x035, "Telex service"},
	{0x038, "Message Handling Systems"},
	{0x041, "OSI application"},
	{0x05e, "Reserved for Maintenance"},
	{0x05f, "Reserved for Management"},
	{0x060, "Videotelephony"},
	{0x101, "capability set of initial channel of H.221"},
	{0x102, "capability set of subsequent channel of  H.221"},
	{-1, "reserved"}
};

static void log_hlc(struct mController *mc, unsigned char *p)
{
	int ret, cod, ecod, l;
	const char *cs;

	l = *p++;
	if (0 > log_coding(mc, "HLC", (*p & 0x60) >> 5, -1, -1, -1))
		return;

	l--;
	p++;
	cod = *p & 0x7f;
	cs = get_lookup_str(cod, hlc_ids);
	ret = logprint(mc, " HL characteristics:%s", cs);
	if (ret < 0)
		return;
	if (l && !(*p & 0x80)) {
		p++;
		ecod = *p & 0x7f;
		if (cod == 0x060)
			ecod |= 0x100;
		cs = get_lookup_str(ecod, hlc_ids);
		logprint(mc, ",%s", cs);
	}
}

static void log_llc(struct mController *mc, unsigned char *p)
{
	int l = *p++;

	if (0 > log_coding(mc, "LLC", (*p & 0x60) >> 5, -1, -1, -1))
		return;
	if (0 > logprint(mc, " %s", mi_bearer2str(*p & 0x1f)))
		return;

	if (!(*p & 0x80)) { /* 3a present */
		if ( l < 2) {
			logprint(mc, " LLC too short");
			return;
		}
		l--;
		p++;
		logprint(mc, " out-band negotiation%spossible", (*p & 0x40) ? " " : " not ");
	}
	l--;
	p++;
	/* rest will be printed as hex, because decoding is very complex and I never saw this IE in a log */
	if (l)
		loghex(mc, "content octet4...:", p, l, 0, -1);
}

static void logIE(struct mController *mc, struct l3_msg *l3m, int ie, unsigned char *iep, int mtIndex)
{
	switch(ie) {
	case IE_BEARER:
		log_bearer_capability(mc, iep);
		break;
	case IE_CAUSE:
		log_cause(mc, iep);
		break;
	case IE_CALL_ID:
		loghex(mc, " callID: ", iep + 1, *iep, 0, -1);
		break;
	case IE_CALL_STATE:
		log_call_state(mc, iep, l3m);
		break;
	case IE_CHANNEL_ID:
		log_channel_id(mc, iep);
		break;
	case IE_FACILITY:
		log_facility(mc, iep, mtIndex);
		break;
	case IE_PROGRESS:
		log_progress(mc, iep);
		break;
	case IE_NET_FAC:
		log_net_fac(mc, iep);
		break;
	case IE_NOTIFY:
		log_notify(mc, iep);
		break;
	case IE_DISPLAY:
		log_ia5_chars(mc, " display:", iep + 1, *iep);
		break;
	case IE_DATE:
		log_date(mc, iep);
		break;
	case IE_KEYPAD:
		log_ia5_chars(mc, " keypad:", iep + 1, *iep);
		break;
	case IE_SIGNAL:
		log_signal(mc, iep);
		break;
	case IE_INFORATE:
		loghex(mc, " infoRate:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_E2E_TDELAY:
		loghex(mc, " end2endTransitDelay:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_TDELAY_SEL:
		loghex(mc, " transiDelaySelection:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_PACK_BINPARA:
		loghex(mc, " pktBinPara:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_PACK_WINSIZE:
		loghex(mc, " pktWindowSize:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_PACK_SIZE:
		loghex(mc, " pktSize:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_CUG:
		loghex(mc, " closedUsergroup:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_REV_CHARGE:
		loghex(mc, " reverse_charge:", iep + 1, *iep, 0, mtIndex);
		break;
	case IE_CONNECT_PN:
		log_number(mc, IE_CONNECT_PN, " connected number:", iep);
		break;
	case IE_CONNECT_SUB:
		log_subaddress(mc, " connected subaddress:", iep);
		break;
	case IE_CALLING_PN:
		log_number(mc, IE_CALLING_PN, " calling number:",  iep);
		break;
	case IE_CALLING_SUB:
		log_subaddress(mc, " calling subaddress:", iep);
		break;
	case IE_CALLED_PN:
		log_number(mc, IE_CALLED_PN, " called number:", iep);
		break;
	case IE_CALLED_SUB:
		log_subaddress(mc, " called subaddress:", iep);
		break;
	case IE_REDIRECTING_NR:
		log_number(mc, IE_REDIRECTING_NR, " redirecting number:", iep);
		break;
	case IE_REDIRECTION_NR:
		log_number(mc, IE_REDIRECTION_NR, " redirection number:", iep);
		break;
	case IE_TRANS_SEL:
		log_transit_net_sel(mc, iep);
		break;
	case IE_RESTART_IND:
		log_restart_ind(mc, iep);
		break;
	case IE_LLC:
		log_llc(mc, iep);
		break;
	case IE_HLC:
		log_hlc(mc, iep);
		break;
	case IE_USER_USER:
		loghex(mc, " useruser:", iep + 1, *iep, 4, mtIndex);
		break;
	}
}

static void logIEs(struct mController *mc, struct mbuffer *mb, int mtIndex)
{
	struct l3_msg *l3m = &mb->l3;
	unsigned char *iep, **v_ie = &l3m->bearer_capability;
	int pos = 0, ie;

	if (l3m->comprehension_req)
		if (0 > logprint(mc, " comprehension Required detected ie=%02x", l3m->comprehension_req))
			return;
	if (l3m->more_data)
		if (0 > logprint(mc, " moreData"))
			return;
	if (l3m->sending_complete)
		if (0 > logprint(mc, " sendingComplete"))
			return;
	if (l3m->congestion_level)
		if (0 > logprint(mc, " congestionLevel=%d", l3m->congestion_level & 0xf))
			return;

	while(1) {
		iep = *v_ie;
		ie = l3_pos2ie(pos);
		if (iep && *iep)
			logIE(mc, l3m, ie, iep, mtIndex);
		if (v_ie == &l3m->useruser) /* last one */
			break;
		pos++;
		v_ie++;
	}
	/* handle repeated IEs and different codset parts */
	for (pos = 0; pos < 8; pos++) {
		if (l3m->extra[pos].val) {
			if (l3m->extra[pos].codeset != 0) {
				logprint(mc, " codeSet(%d)", l3m->extra[pos].codeset);
				loghex(mc, " items:", l3m->extra[pos].val, l3m->extra[pos].len, 4, mtIndex);
			} else if (*l3m->extra[pos].val) {
				logIE(mc, l3m, l3m->extra[pos].ie, l3m->extra[pos].val, mtIndex);
			}
		}
	}
}



static int log_layer3(struct mController *mc, struct mbuffer *mb)
{
	unsigned char *dp;
	char *ip, *sp = mc->lp;
	int len, ret, idx = 0;

	dp = mb->data;
	len = mb->len;
	if (*dp != Q931_PD) {
		logprint(mc, " Unknown L3 protocol found 0x%02x\n", *dp);
		return 0;
	}
	ret = parseQ931(mb);
	if (ret) {
		logprint(mc, " Error %x parsing L3 message", ret);
		return 0;
	}
	if (mb->l3h.crlen == 0)
		logprint(mc, " CRef=Dummy");
	else if ((mb->l3h.cr & 0x7fff) == 0)
		logprint(mc, " CRef(%c)=Global", mb->l3h.cr & MISDN_PID_CR_FLAG ? 'A' : 'O');
	else
		logprint(mc, " CRef(%c)=(%d)", mb->l3h.cr & MISDN_PID_CR_FLAG ? 'A' : 'O', mb->l3h.cr & 0x7fff);
	ip = strchr(MTidx, mb->l3.type);
	if (ip)
		idx = 1 + (ip - MTidx);
		
	if (mc->layer3[idx]) {
		if (ip)
			logprint(mc, " %s", mTypesStr[idx]);
		else
			logprint(mc, " %s(%02x)", mTypesStr[idx], mb->l3.type);
		if (mc->layer3[idx] & l3vVERBOSE)
			logIEs(mc, mb, idx);
		if (mc->layer3[idx] & l3vHEX)
			loghex(mc, "L3hex:", dp, len, 4, -1);
	} else {
		mc->lp = sp;
		*sp = 0;
	}
	return 0;
}

static int log_teimanagment(struct mController *mc, struct mbuffer *mb)
{
	int ri, mt, ai;
	unsigned char *dp;

	dp = msg_pull(mb, 1);
	if (!IsUI(dp)) {
		logprint(mc, " not UI but %02x frame for TEI managment procedures", *dp);
	} else {
		if (mb->len < 5) {
			logprint(mc, " TEI managment UI frame too short only %d from 5 bytes", mb->len);
		} else {
			dp = msg_pull(mb, 5);
			if (*dp != 0xf) {
				logprint(mc, " wrong layer managment entity %02x for TEI managment procedures", *dp);
			} else {
				dp++;
				ri = *dp++;
				ri <<= 8;
				ri |= *dp++;
				mt = *dp++;
				ai = *dp >> 1;
				switch (mt) {
				case 1:
					logprint(mc, " TEI Identity request ri=%d ai=%d", ri, ai);
					break;
				case 2:
					logprint(mc, " TEI Identity assigned ri=%d TEI=%d", ri, ai);
					break;
				case 3:
					if (ai == 127)
						logprint(mc, " TEI Identity denied ri=%d - no TEI available", ri);
					else
						logprint(mc, " TEI Identity denied ri=%d TEI=%d", ri, ai);
					break;
				case 4:
					if (ai == 127)
						logprint(mc, " TEI Identity check request for all TEI");
					else
						logprint(mc, " TEI Identity check request for TEI %d", ai);
					break;
				case 5: /* TODO multi tei report ? */
					logprint(mc, " TEI Identity check response ri=%d TEI %d in use", ri, ai);
					break;
				case 6:
					if (ai == 127)
						logprint(mc, " TEI Identity remove for all TEI");
					else
						logprint(mc, " TEI Identity remove for TEI %d", ai);
					break;
				case 7:
					logprint(mc, " TEI Identity verify for TEI %d", ai);
					break;
				default:
					logprint(mc, " Invalid TEI managment type %d ri=%d ai=%d", mt, ri, ai);
					return 0;
				}
				if (!(mc->layer2 & l2vTEI)) /* only report TEI assignmen if requested */
					mc->lp = mc->logtxt;
			}
		}
	}
	return 0;
}

static int log_layer2(struct mController *mc, struct mbuffer *mb)
{
	unsigned char *dp, *savep;
	int ret = 0, savelen, psapi, ptei, cr, nr, ns, origin, cmd;
	char *sp, pf;

	if (mb->len < 3) {
		logprint(mc, "L2 frame too short only %d bytes\n", mb->len);
		return 0;
	}
	sp = mc->lp;
	savep = mb->data;
	savelen = mb->len;
	dp = msg_pull(mb, 2);
	psapi = *dp++;
	ptei = *dp++;
	if ((psapi & 1) || !(ptei & 1)) {
		logprint(mc, "L2 frame wrong EA0/EA1 %02x/%02x\n", psapi, ptei);
		return 0;
	}
	cr = psapi & 2; 
	psapi >>= 2;
	ptei >>= 1;
	if (mc->protocol == ISDN_P_NT_S0 || mc->protocol == ISDN_P_NT_E1)
		origin = mb->h->prim == PH_DATA_REQ ? 0 : 1;
	else
		origin = ((mb->h->prim == PH_DATA_REQ) || (mb->h->prim == PH_DATA_E_IND)) ? 1 : 0;
	switch ((origin | cr) & 3) {
	case 0:
	case 3:
		cmd = 0;
		break;
	case 1:
	case 2:
		cmd = 1;
		break;
	} 
	if (origin)
		logprint(mc, "User>Net %s", cmd ? "Cmd" : "Rsp");
	else
		logprint(mc, "Net>User %s", cmd ? "Cmd" : "Rsp");
	if (psapi == TEI_SAPI) {
		if (ptei == 127) {
			if (cmd)
				ret = log_teimanagment(mc, mb);
			else
				logprint(mc, " TEI managment frame is not a command frame");
		} else
			logprint(mc, " Wrong TEI %d for TEI managment procedures", ptei);
		return ret;
	} else if (psapi == CTRL_SAPI) {
		logprint(mc, " TEI=%d", ptei);
		sp = mc->lp;
	} else {
		if (mc->layer2 & l2vSAPI)
			wprint("Controller%d: L2 frame for SAPI %d TEI %d\n", mc->mNr, psapi, ptei);
		return 0;
	}
	dp = msg_pull(mb, 1);

	if (!(*dp & 1)) {	/* I-Frame */
		nr = *dp++ >> 1;
		ns =  *dp >> 1;
		pf =  *dp & 1 ? 'P' : ' ';
		logprint(mc, " %c  I N(R)=%d N(S)=%d", pf, nr, ns);
		msg_pull(mb, 1);
		sp = mc->lp;
		ret = log_layer3(mc, mb);
	} else if (IsUI(dp)) {
		pf = *dp & 0x10 ? 'P' : ' ';
		logprint(mc, " %c UI", pf);
		sp = mc->lp;
		ret = log_layer3(mc, mb);
	} else {
		if (IsRR(dp)) {
			if (mc->layer2 & l2vKEEPALIVE) {
				dp++;
				nr = *dp >> 1;
				if (*dp & 1)
					pf = cmd ? 'P' : 'F';
				else
					pf = ' ';
				logprint(mc, " %c RR N(R)=%d", pf, nr);
			}
		} else if (IsRNR(dp)) {
			if (mc->layer2 & l2vKEEPALIVE) {
				dp++;
				nr = *dp >> 1;
				if (*dp & 1)
					pf = cmd ? 'P' : 'F';
				else
					pf = ' ';
				logprint(mc, " %c RNR N(R)=%d", pf, nr);
			}
		} else if (IsREJ(dp)) {
			if (mc->layer2 & l2vKEEPALIVE) {
				dp++;
				nr = *dp >> 1;
				if (*dp & 1)
					pf = cmd ? 'P' : 'F';
				else
					pf = ' ';
				logprint(mc, " %c REJ N(R)=%d", pf, nr);
			}
		} else if (IsSABME(dp)) {
			if (mc->layer2 & (l2vCONTROL | l2vKEEPALIVE)) {
				pf = *dp & 0x10 ? 'P' : ' ';
				logprint(mc, " %c SABME", pf);
			}
		} else if (IsUA(dp)) {
			if (mc->layer2 & (l2vCONTROL | l2vKEEPALIVE)) {
				pf = *dp & 0x10 ? 'F' : ' ';
				logprint(mc, " %c UA", pf);
			}
		} else if (IsDISC(dp)) {
			if (mc->layer2 & (l2vCONTROL | l2vKEEPALIVE)) {
				pf = *dp & 0x10 ? 'P' : ' ';
				logprint(mc, " %c DISC", pf);
			}
		} else if (IsDM(dp)) {
			if (mc->layer2 & (l2vCONTROL | l2vKEEPALIVE)) {
				pf = *dp & 0x10 ? 'F' : ' ';
				logprint(mc, " %c DM", pf);
			}
		} else if (IsFRMR(dp)) {
			if (mc->layer2) {
				logprint(mc, " FRMR");
			}
		} else {
			if (mc->layer2) {
				logprint(mc, " Unknown L2 type %02x", *dp);
			}
		}
	}
	if (mc->lp == sp)
		mc->lp = &mc->logtxt[0];
	mb->data = savep;
	mb->len = savelen;
	return ret;
}

static int log_layer1(struct mController *mc, struct mbuffer *mb)
{
	int ret = 0;
	const char *pn = NULL;
	
	switch (mb->h->prim) {
	case PH_DATA_E_IND:
		if (!mc->echo) { /* skip PH_DATA_E_IND if not expected */
			dprint(mlDEBUG_MESSAGE, "Controller%d unexpected PH_DATA_E_IND recived\n", mc->mNr);
		} else
			ret = log_layer2(mc, mb);
		break;
	case PH_DATA_IND:
		ret = log_layer2(mc, mb);
		break;
	case PH_DATA_REQ:
		if (!mc->echo) /* skip PH_DATA_REQ if PH_DATA_E_IND are expected */
			ret = log_layer2(mc, mb);
		break;
	case PH_ACTIVATE_IND:
		pn = "ACTIVATE IND";
		break;
	case PH_DEACTIVATE_IND:
		pn = "DEACTIVATE IND";
		break;
	case PH_DATA_CNF:
	case PH_DEACTIVATE_CNF:
	case PH_ACTIVATE_REQ:
	case PH_ACTIVATE_CNF:
	case PH_DEACTIVATE_REQ:
	case MPH_ACTIVATE_IND:
	case MPH_ACTIVATE_REQ:
	case MPH_INFORMATION_REQ:
	case MPH_DEACTIVATE_IND:
	case MPH_DEACTIVATE_REQ:
	case MPH_INFORMATION_IND:
	case PH_CONTROL_REQ:
	case PH_CONTROL_IND:
	case PH_CONTROL_CNF:
		/* ignored */
		break;
	default:
		logprint(mc, "Unknown L1 message len=%d %04x received\n", mb->len, mb->h->prim);
		break;
	}
	if (pn && mc->layer1) {
		logprint(mc, "Layer1 %s", pn);
	}
	return ret;
}

static int read_socket(struct mController *mc)
{
	int result;
	struct msghdr mh;
	struct iovec iov[1];
	struct ctstamp cts;
	struct mbuffer *mb;

	mh.msg_name = NULL;
	mh.msg_namelen = 0;
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_control = &cts;
	mh.msg_controllen = sizeof(cts);
	mh.msg_flags = 0;
	mb = alloc_mbuffer();
	if (!mb) {
		eprint("Cannot allocate mbuffer - aborting\n");
		return -1;
	}
	iov[0].iov_base = mb->head;
	iov[0].iov_len = MBUFFER_DATA_SIZE;
	result = recvmsg(mc->socket, &mh, 0);
	if (result < 0) {
		eprint("read error %s\n", strerror(errno));
		free_mbuffer(mb);
	} else {
		mc->lp = &mc->logtxt[0];
		mb->len = result;
		msg_pull(mb, MISDN_HEADER_LEN);
		if (mh.msg_flags) {
			dprint(mlDEBUG_MESSAGE, "received message with msg_flags(%x)\n", mh.msg_flags);
		}
		if (cts.cmsg_type == MISDN_TIME_STAMP)
			mc->tv = &cts.tv;
		else
			mc->tv = NULL;
		result = log_layer1(mc, mb);
		log_output(mc);
		if (!result)
			free_mbuffer(mb);
	}
	return result;
}


static int main_loop(int enabled)
{
	int ret, rc, result = 0;
	int i, idx = 0;
	int polldelay;
	struct mController *pc, **pollContr = alloca(enabled * sizeof(struct mController *));
	struct pollfd  *polls = alloca(enabled * sizeof(struct pollfd));

	memset(polls, 0, enabled * sizeof(struct pollfd));
	for (i = 0; i < mI_ControllerCount; i++) {
		pc = &mI_Controller[i];
		if (!pc->enable)
			continue;
		polls[idx].fd = pc->socket;
		polls[idx].events = POLLIN | POLLPRI;
		pollContr[idx] = pc;
		dprint(mlDEBUG_POLL, "poll setup %d fd %d events %x\n", idx, polls[idx].fd, polls[idx].events);
		idx++;
		if (idx >= enabled)
			break;
	}
	if (idx != enabled) {
		eprint("internal error %d controller enabled, but %d added to poll -aborted\n", enabled, idx);
		return -1;
	}
	polldelay = -1;
	running = 1;
	while (running) {
		ret = poll(polls, enabled, polldelay);
		dprint(mlDEBUG_POLL, "poll ret=%d\n", ret);
		if (ret < 0) {
			if (errno == EINTR)
				dprint(mlDEBUG_POLL, "Received signal - continue\n");
			else {
				wprint("Error on poll errno=%d - %s\n", errno, strerror(errno));
			}
			continue;
		}
		if (ret == 0) {	/* timeout never */
			running = 0;
			wprint("Error poll timeout - should never happen\n");
			continue;
		}
		for (i = 0; i < enabled; i++) {
			if (ret && polls[i].revents) {
				pc = pollContr[i];
				rc = read_socket(pc);
				if (rc < 0) {
					running = 0;
					break;
				}
				ret--;
			}
			if (ret == 0)
				break;
		}
	}
	return result;
}


int main(int argc, char *argv[])
{
	int result;
	int opt;
	struct mISDNversion ver;
	int i, j;
	int exitcode = -1;
	int _L3init = 0;
	int mIsock = -1;
	struct mController *pc;
	int channel;
	int nrEnabled;

	config_file = def_config;
	result = opt_parse(argc, argv);
	if (result)
		exit(1);

	if (ml_debugmask)
		mISDN_set_debug_level(ml_debugmask << 24);

	mI_ControllerCount = 0;
	mIsock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (mIsock < 0) {
		fprintf(stderr, "mISDNv2 not installed - %s\n", strerror(errno));
		return 1;
	}

	result = ioctl(mIsock, IMGETVERSION, &ver);
	if (result < 0) {
		fprintf(stderr, "ioctl error %s\n", strerror(errno));
		return 1;
	}
	if (ver.release & MISDN_GIT_RELEASE)
		printf("mISDN kernel version %d.%02d.%d (git.misdn.eu) found\n", ver.major, ver.minor,
		       ver.release & ~MISDN_GIT_RELEASE);
	else
		printf("mISDN kernel version %d.%02d.%d found\n", ver.major, ver.minor, ver.release);

	printf("mISDN user   version %d.%02d.%d found\n", MISDN_MAJOR_VERSION, MISDN_MINOR_VERSION, MISDN_RELEASE);

	if (ver.major != MISDN_MAJOR_VERSION) {
		fprintf(stderr, "VERSION incompatible please update\n");
		goto errout;
	}

	/* get number of stacks */
	result = ioctl(mIsock, IMGETCOUNT, &mI_ControllerCount);
	if (result < 0) {
		fprintf(stderr, "mISDNv2 IMGETCOUNT error - %s\n", strerror(errno));
		goto errout;
	}
	if (mI_ControllerCount < 1) {
		fprintf(stderr, "mISDNv2 no controllers found\n");
		goto errout;
	}
	defController = calloc(mI_ControllerCount + 1, sizeof(*mI_Controller));
	if (!defController) {
		fprintf(stderr, "no memory to allocate %d controller struct (a %zd)\n",
			mI_ControllerCount, sizeof(*mI_Controller));
		goto errout;
	}
	mI_Controller = &defController[1];
	result = read_config_file(config_file);
	if (result < 0) {
		fprintf(stderr, "Error while proccessing config file %s - aborting\n", config_file);
		goto errout;
	}
	for (i = 0; i < mI_ControllerCount; i++) {
		pc = &mI_Controller[i];
		pc->devinfo.id = i;
		pc->socket = -1;
		result = ioctl(mIsock, IMGETDEVINFO, &pc->devinfo);
		if (result < 0) {
			fprintf(stderr, "mISDNv2 IMGETDEVINFO error controller %d - %s\n", i + 1, strerror(errno));
			goto errout;
		}
		printf("	id:		%d\n", pc->devinfo.id);
		printf("	Dprotocols:	%08x\n", pc->devinfo.Dprotocols);
		printf("	Bprotocols:	%08x\n", pc->devinfo.Bprotocols);
		printf("	protocol:	%d\n", pc->devinfo.protocol);
		printf("	channelmap:	");
		for (j = MISDN_CHMAP_SIZE - 1; j >= 0; j--)
			printf("%02x", pc->devinfo.channelmap[j]);
		printf("\n");
		printf("	nrbchan:	%d\n", pc->devinfo.nrbchan);
		printf("	name:		%s\n", pc->devinfo.name);
		if (pc->devinfo.protocol == ISDN_P_NONE)
			pc->protocol = ISDN_P_TE_S0;
		else
			pc->protocol = pc->devinfo.protocol;
	}
	nrEnabled = 0;
	for (i = 0; i < mI_ControllerCount; i++) {
		pc = &mI_Controller[i];
		if (!pc->enable)
			continue;
		pc->socket = socket(PF_ISDN, SOCK_DGRAM, pc->protocol);
		if (pc->socket < 0) {
			fprintf(stderr, "mISDNv2 cannot open a mISDN socket for controller %d protocol %d\n", i + 1, pc->protocol);
			goto errout;
		}
		pc->addr.family = AF_ISDN;
		pc->addr.dev = pc->mNr;
		/* try echo channel first if enabled */
		channel = pc->echo;
		while (channel >= 0) {
			pc->addr.channel = (unsigned char)channel;
			result = bind(pc->socket, (struct sockaddr *)&pc->addr, sizeof(pc->addr));
			if (result < 0) {
				if (channel == 0) {
					fprintf(stderr, "mISDNv2 controller %d cannot bind to socket\n", i + 1);
					goto errout;
				}
				close(pc->socket);
				pc->socket = socket(PF_ISDN, SOCK_DGRAM, pc->protocol);
				if (pc->socket < 0) {
					fprintf(stderr, "mISDNv2 cannot open socket again for controller %d protocol %d\n",
						i + 1, pc->protocol);
					goto errout;
				}
				channel--;
			} else
				break;
		}
		pc->echo = (pc->addr.channel == 1);
		opt = 1;
		result = setsockopt(pc->socket, SOL_MISDN, MISDN_TIME_STAMP, &opt, sizeof(opt));
		if (result < 0) {
			/* none fatal */
			fprintf(stderr, "mISDNv2 controller %d setsockopt error %s\n", i + 1, strerror(errno));
		}
		nrEnabled++;
	}
	init_layer3(2 * (1 + mI_ControllerCount), &myfn);
	_L3init = 1;
	iprint("mISDNv2 logger enabled %d/%d controller", nrEnabled, mI_ControllerCount);

	if (!nrEnabled) {
		fprintf(stderr, "No controllers are enabled for logging - abort\n");
		goto errout;
	}
	dprint(mlDEBUG_BASIC, "do_daemon=%d config=%s\n", do_daemon, config_file);
	if (do_daemon) {
		result = daemon(0, 0);
		if (result < 0) {
			fprintf(stderr, "cannot run as daemon\n");
			goto errout;
		}
		openlog("mISDNlogger", LOG_ODELAY, LOG_DAEMON);
	}
	if (0 > open_logfile())
		goto errout;
	/* setup signal handler */
	memset(&mysig_term, 0, sizeof(mysig_term));
	mysig_term.sa_handler = termHandler;
	result = sigaction(SIGTERM, &mysig_term, NULL);
	if (result) {
		wprint("Error to setup signal handler for SIGTERM - %s\n", strerror(errno));
	}
	result = sigaction(SIGHUP, &mysig_term, NULL);
	if (result) {
		wprint("Error to setup signal handler for SIGHUP - %s\n", strerror(errno));
	}
	result = sigaction(SIGINT, &mysig_term, NULL);
	if (result) {
		wprint("Error to setup signal handler for SIGINT - %s\n", strerror(errno));
	}

	memset(&mysig_dump, 0, sizeof(mysig_dump));
	mysig_dump.sa_handler = dumpHandler;
	result = sigaction(SIGUSR1, &mysig_dump, NULL);
	if (result) {
		wprint("Error to setup signal handler for SIGUSR1 - %s\n", strerror(errno));
	}
	result = sigaction(SIGUSR2, &mysig_dump, NULL);
	if (result) {
		wprint("Error to setup signal handler for SIGUSR2 - %s\n", strerror(errno));
	}

	exitcode = main_loop(nrEnabled);

	iprint("logger mainloop ended exitcode %d", exitcode);
errout:
	if (mIsock > 0)
		close(mIsock);
	if (mI_Controller) {
		for (i = 0; i < mI_ControllerCount; i++) {
			pc = &mI_Controller[i];
			if (pc->socket > 0) {
				close(pc->socket);
			}
		}
	}
	if (_L3init)
		cleanup_layer3();
	if (defController) {
		close_logfile();
		free(defController);
	}
	return exitcode;
}
