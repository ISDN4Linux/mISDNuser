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
#include <sys/ioctl.h>
#include <mISDNif.h>

#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>


void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options]\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n"); 
	fprintf(stderr,"\n");
}


int
main(argc, argv)
int argc;
char *argv[];
{
	int aidx=1;
	int cardnr = 0;
	int sock;
	struct sockaddr_mISDN  addr;
	int result;
	int clean;
	char sw;
	u_int cnt, protocol;
	struct mISDN_devinfo	di;


	while (aidx < argc) {
		if (argv[aidx] && argv[aidx][0]=='-') {
			sw=argv[aidx][1];
			switch (sw) {
				case 'c':
					if (argv[aidx][2]) {
						cardnr=atol(&argv[aidx][2]);
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
			fprintf(stderr,"Undefined argument %s\n",argv[aidx]);
			usage(argv[0]);
			exit(1);
		}
		aidx++;
	} 

	if (cardnr < 0) {
		fprintf(stderr,"card nr cannot be negative\n");
		exit(1);
	}
	init_af_isdn();
	if ((sock = socket(PF_ISDN, SOCK_RAW, 0)) < 0) {
		printf("could not open socket %s\n", strerror(errno));
		exit(1);
	}
	result = ioctl(sock, IMGETCOUNT, &cnt);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	} else
		printf("%d controller%s found\n", cnt, (cnt==1)?"":"s");

	di.id = cardnr;
	result = ioctl(sock, IMGETDEVINFO, &di);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
	} else {
		printf("	id:		%d\n", di.id);
		printf("	Dprotocols:	%08x\n", di.Dprotocols);
		printf("	Bprotocols:	%08x\n", di.Bprotocols);
		printf("	protocol:	%d\n", di.protocol);
		printf("	nrbchan:	%d\n", di.nrbchan);
		printf("	name:		%s\n", di.name);
	}

	close(sock);

	if (di.protocol == ISDN_P_NONE) {
		printf("protocol ISDN_P_TE_S0 stack need no cleanup\n");
		exit(0);
	}

	if (di.protocol == ISDN_P_TE_S0 || di.protocol == ISDN_P_TE_E1) {
		protocol = ISDN_P_LAPD_TE;
	} else {
		protocol = ISDN_P_LAPD_NT;
	}

	if ((sock = socket(PF_ISDN, SOCK_DGRAM, protocol)) < 0) {
		printf("could not open socket %s\n", strerror(errno));
		exit(1);
	}

	addr.family = AF_ISDN;
	addr.dev = cardnr;
	addr.channel = 0;
	addr.sapi = 0;
	addr.tei = 127;
	result = bind(sock, (struct sockaddr *) &addr,
		 sizeof(addr));
	printf("bind return %d\n", result);

	if (result < 0) {
		printf("bind error %s\n", strerror(errno));
	}
	clean = 1;
	result = ioctl(sock, IMCLEAR_L2, &clean);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	}
	close(sock);
	return (0);
}
