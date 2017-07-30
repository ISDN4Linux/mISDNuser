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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <mISDN/mISDNif.h>
#include <mISDN/af_isdn.h>
#include <mISDN/mbuffer.h>
#include "call_log.h"

static int dch_echo = 0;
static int running = 0;

static void usage(pname)
char *pname;
{
	fprintf(stderr,"Call with %s [options]\n", pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 0)\n");
	fprintf(stderr,"\n");
}


static int my_lib_debug(const char *file, int line, const char *func, int level, const char *fmt, va_list va)
{
	int ret;
	char log[256];

	ret = vsnprintf(log, 256, fmt, va);
	fprintf(stderr, "%20s:%3d %s\n", file, line, log);
	return ret;
}

static struct mi_ext_fn_s myfn = {
	.prt_debug = my_lib_debug,
};

static void printhex(unsigned char *p, int len, int head)
{
	int	i,j;

	for (i = 1; i <= len; i++) {
		printf(" %02x", *p++);
		if ((i!=len) && !(i % 4) && (i % 16))
			printf(" ");
		if ((i!=len) && !(i % 16)) {
			printf("\n");
			for (j = 0; j < head; j++)
				printf(" ");
		}
	}
	printf("\n");
}

struct ctstamp {
	size_t		cmsg_len;
	int		cmsg_level;
	int		cmsg_type;
	struct timeval	tv;
};

int
main(argc, argv)
int argc;
char *argv[];
{
	int	aidx=1, i, channel;
	int	cardnr = 0;
	int	log_socket;
	struct sockaddr_mISDN  log_addr;
	char	sw;
	int	head = 0;
	char	*pn, pns[32];
	struct msghdr	mh;
	struct iovec	iov[1];
	struct ctstamp	cts;
	struct tm	*mt;
	int	result;
	int	opt;
	u_int	cnt;
	struct mISDN_devinfo	di;
	struct mISDNversion	ver;
	struct mbuffer		*mb;

	while (aidx < argc) {
		if (argv[aidx] && argv[aidx][0]=='-') {
			sw=argv[aidx][1];
			switch (sw) {
				case 'c':
					if (argv[aidx][2]) {
						cardnr=atol(&argv[aidx][2]);
					}
					break;
				case 'e':
					dch_echo = 1;
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
			fprintf(stderr,"Undefined argument %s\n",argv[aidx]);
			usage(argv[0]);
			exit(1);
		}
		aidx++;
	}

	if (cardnr < 0) {
		fprintf(stderr,"card nr may not be negative\n");
		exit(1);
	}

	if ((log_socket = socket(PF_ISDN, SOCK_RAW, 0)) < 0) {
		printf("could not open socket %s\n", strerror(errno));
		exit(1);
	}
	init_layer3(4, &myfn);
	result = ioctl(log_socket, IMGETVERSION, &ver);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	}
	if (ver.release & MISDN_GIT_RELEASE)
		printf("mISDN kernel version %d.%02d.%d (git.misdn.eu) found\n", ver.major, ver.minor, ver.release & ~MISDN_GIT_RELEASE);
	else
		printf("mISDN kernel version %d.%02d.%d found\n", ver.major, ver.minor, ver.release);

	printf("mISDN user   version %d.%02d.%d found\n", MISDN_MAJOR_VERSION, MISDN_MINOR_VERSION, MISDN_RELEASE);

	if (ver.major != MISDN_MAJOR_VERSION) {
		printf("VERSION incompatible please update\n");
		exit(1);
	}

	result = ioctl(log_socket, IMGETCOUNT, &cnt);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	} else
		printf("%d controller%s found\n", cnt, (cnt==1)?"":"s");

	di.id = cardnr;
	result = ioctl(log_socket, IMGETDEVINFO, &di);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
	} else {
		printf("	id:		%d\n", di.id);
		printf("	Dprotocols:	%08x\n", di.Dprotocols);
		printf("	Bprotocols:	%08x\n", di.Bprotocols);
		printf("	protocol:	%d\n", di.protocol);
		printf("	channelmap:	");
		for (i = MISDN_CHMAP_SIZE - 1; i >= 0; i--)
			printf("%02x", di.channelmap[i]);
		printf("\n");
		printf("	nrbchan:	%d\n", di.nrbchan);
		printf("	name:		%s\n", di.name);
	}

	close(log_socket);

	if (di.protocol == ISDN_P_NONE) /* default TE */
		di.protocol = ISDN_P_TE_S0;

	if ((log_socket = socket(PF_ISDN, SOCK_DGRAM, di.protocol)) < 0) {
		printf("could not open log socket %s\n", strerror(errno));
		exit(1);
	}

	log_addr.family = AF_ISDN;
	log_addr.dev = cardnr;

	/* try to bind on D/E channel first, fallback to D channel on error */
	result = -1;
	channel = dch_echo;

	while ((result < 0) && (channel >= 0)) {
		log_addr.channel = (unsigned char)channel;
		result = bind(log_socket, (struct sockaddr *) &log_addr,
			sizeof(log_addr));
		printf("log bind ch(%i) return %d\n", log_addr.channel, result);
		if (result < 0) {
			printf("log bind error %s\n", strerror(errno));
			close(log_socket);
			if (channel == 0) {
				exit(1);
			}
			channel--;
			if ((log_socket = socket(PF_ISDN, SOCK_DGRAM, di.protocol)) < 0) {
				printf("could not open log socket %s\n", strerror(errno));
				exit(1);
			}
		}
	}
	if (dch_echo)
		dch_echo = (log_addr.channel == 1);
	printf("Echo channel logging %s\n", dch_echo ? "yes" : "no");

	opt = 1;
	result = setsockopt(log_socket, SOL_MISDN, MISDN_TIME_STAMP, &opt, sizeof(opt));
	if (result < 0) {
		printf("log  setsockopt error %s\n", strerror(errno));
	}

	running = 1;
	while (running) {
		mh.msg_name = NULL;
		mh.msg_namelen = 0;
		mh.msg_iov = iov;
		mh.msg_iovlen = 1;
		mh.msg_control = &cts;
		mh.msg_controllen = sizeof(cts);
		mh.msg_flags = 0;
		mb = alloc_mbuffer();
		if (!mb) {
			printf("Cannot allocate mbuffer - aborting\n");
			break;
		}
		iov[0].iov_base = mb->head;
		iov[0].iov_len = MBUFFER_DATA_SIZE;
		result = recvmsg(log_socket, &mh, 0);
		if (result < 0) {
			printf("read error %s\n", strerror(errno));
			running = 0;
		} else {
			mb->len = result;
			msg_pull(mb, MISDN_HEADER_LEN);
			if (mh.msg_flags) {
				printf("received message with msg_flags(%x)\n", mh.msg_flags);
			}
			if (cts.cmsg_type == MISDN_TIME_STAMP) {
				mt = localtime((time_t *)&cts.tv.tv_sec);
				head = printf("%02d.%02d.%04d %02d:%02d:%02d.%06ld", mt->tm_mday, mt->tm_mon + 1, mt->tm_year + 1900,
					mt->tm_hour, mt->tm_min, mt->tm_sec, cts.tv.tv_usec);
			} else {
				cts.tv.tv_sec = 0;
				cts.tv.tv_usec = 0;
			}
			switch (mb->h->prim) {
				case PH_DATA_E_IND:
					pn = "ECHO IND";
					break;
				case PH_DATA_IND:
					pn = "DATA IND";
					break;
				case PH_DATA_REQ:
					pn = "DATA REQ";
					break;
				case PH_DATA_CNF:
					pn = "DATA CNF";
					break;
				case PH_ACTIVATE_IND:
					pn = "ACTIVATE IND";
					break;
				case PH_ACTIVATE_REQ:
					pn = "ACTIVATE REQ";
					break;
				case PH_ACTIVATE_CNF:
					pn = "ACTIVATE CNF";
					break;
				case PH_DEACTIVATE_IND:
					pn = "DEACTIVATE IND";
					break;
				case PH_DEACTIVATE_REQ:
					pn = "DEACTIVATE REQ";
					break;
				case PH_DEACTIVATE_CNF:
					pn = "DEACTIVATE CNF";
					break;
				case MPH_ACTIVATE_IND:
					pn = "MPH ACTIVATE IND";
					break;
				case MPH_ACTIVATE_REQ:
					pn = "MPH ACTIVATE REQ";
					break;
				case MPH_INFORMATION_REQ:
					pn = "MPH INFORMATION REQ";
					break;
				case MPH_DEACTIVATE_IND:
					pn = "MPH DEACTIVATE IND";
					break;
				case MPH_DEACTIVATE_REQ:
					pn = "MPH DEACTIVATE REQ";
					break;
				case MPH_INFORMATION_IND:
					pn = "MPH INFORMATION IND";
					break;
				case PH_CONTROL_REQ:
					pn = "PH CONTROL REQ";
					break;
				case PH_CONTROL_IND:
					pn = "PH CONTROL IND";
					break;
				case PH_CONTROL_CNF:
					pn = "PH CONTROL CNF";
					break;
				default:
					sprintf(pns,"Unknown %04x", mb->h->prim);
					pn = pns;
					break;
			}
			head += printf(" %s id=%08x", pn, mb->h->id);

			if (result > MISDN_HEADER_LEN) {
				head += printf(" %3d bytes", mb->len);
				printhex(mb->data, mb->len, head);
			} else
				printf("\n");
		}
		free_mbuffer(mb);
	}
	close(log_socket);
	return (0);
}
