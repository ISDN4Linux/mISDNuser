/*****************************************************************************\
**                                                                           **
** l1oipctrl                                                                **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg  (GPL)                                       **
**                                                                           **
** control l1oip virtual cards                                               **
**                                                                           **
\*****************************************************************************/ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <mlayer3.h>
#include <mbuffer.h>
#include <errno.h>
#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>

int main(int argc, char *argv[])
{
	int			ret;
	int			count;
	int			sock;
	int			port;
	int			try = 0, pri = 0;
	struct hostent		*hostent;
	struct servent		*servent;
	struct mISDN_ctrl_req	crq;
	struct sockaddr_mISDN	addr;
	struct mISDN_devinfo	devinfo;

	memset(&crq, 0, sizeof(crq));

	init_af_isdn();

	/* usage */
	if (argc <= 1) {
usage:
		printf(
			"Usage: %s <port> [param]\n"
			"Param is:\n"
			"  getpeer -> gets current peer IP setting\n"
			"  unsetpeer -> releases current peer IP\n"
			"  setpeer <remote host> <remote port> [<local port>]\n"
			, argv[0]);
		return(0);
	}
	if (argc <= 2) {
		fprintf(stderr, "Missing parameter.\n");
		goto usage;
	}
	port = atoi(argv[1]);
	if (!strcmp(argv[2], "setpeer")) {
		crq.op = MISDN_CTRL_SETPEER;
		if (argc <= 3) {
			fprintf(stderr, "Missing remote host.\n");
			goto usage;
		}
		hostent = gethostbyname(argv[3]);
		if (!hostent) {
			fprintf(stderr, "Given host %s not found.\n", argv[3]);
			return(0);
		}
		if (hostent->h_addrtype != AF_INET || hostent->h_length != 4) {
			fprintf(stderr, "Given host %s is not IPv4 address.\n", argv[3]);
			return(0);
		}
		crq.p1 = ntohl(*(unsigned int *)hostent->h_addr);
		if (argc <= 4) {
			fprintf(stderr, "Missing remote port.\n");
			goto usage;
		}
		if (argv[4][0] < '0' || argv[4][0] > '9') {
			servent = getservbyname(argv[4], "udp");
			if (!servent) {
				fprintf(stderr, "Given port %s not found.\n", argv[4]);
				return(0);
			}
			crq.p2 = ntohs(servent->s_port);
		} else
			crq.p2 = atoi(argv[4]);
		if (argc > 5) {
			if (argv[5][0] < '0' || argv[5][0] > '9') {
				servent = getservbyname(argv[5], "udp");
				if (!servent) {
					fprintf(stderr, "Given port %s not found.\n", argv[5]);
					return(0);
				}
				crq.p2 |= ntohs(servent->s_port) << 16;
			} else
				crq.p2 |= atoi(argv[5]) << 16;
		}
	} else
	if (!strcmp(argv[2], "unsetpeer")) {
		crq.op = MISDN_CTRL_UNSETPEER;
		crq.p1 = 0;
	} else
	if (!strcmp(argv[2], "getpeer")) {
		crq.op = MISDN_CTRL_GETPEER;
	} else {
		fprintf(stderr, "Unknown parameter.\n");
		goto usage;
	}
	
	/* open mISDN */
	sock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sock < 0) {
		fprintf(stderr, "Cannot open mISDN due to %s. (Does your Kernel support socket based mISDN?)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* get number of stacks */
	ret = ioctl(sock, IMGETCOUNT, &count);
	if (ret < 0) {
		fprintf(stderr, "Cannot get number of mISDN devices. (ioctl IMGETCOUNT failed ret=%d)\n", ret);
		goto done;
	}
	if (count <= 0) {
		printf("Found no card. Please be sure to load card drivers.\n");
		goto done;
	}
	if (count <= port) {
		printf("Given port %d is out of range.\n", port);
		goto done;
	}

	devinfo.id = port;
	ret = ioctl(sock, IMGETDEVINFO, &devinfo);
	if (ret < 0) {
		fprintf(stderr, "Cannot get devinfo of mISDN devices. (ioctl IMGETDEVINFO failed ret=%d)\n", ret);
		goto done;
	}
	if (devinfo.Dprotocols & (1 << ISDN_P_TE_E1))
		pri = 1;
	if (devinfo.Dprotocols & (1 << ISDN_P_NT_E1))
		pri = 1;

	close(sock);
	
	/* open device */
again:
	if (try == 0)
		sock = socket(PF_ISDN, SOCK_DGRAM, pri?ISDN_P_TE_E1:ISDN_P_TE_S0);
	else
		sock = socket(PF_ISDN, SOCK_DGRAM, pri?ISDN_P_NT_E1:ISDN_P_NT_S0);
	if (sock < 0) {
		fprintf(stderr, "Cannot open mISDN device due to %s.\n", strerror(errno));
		goto done;
	}
	addr.family = AF_ISDN;
	addr.dev = port;
	addr.channel = 0;
	ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0 && errno == EBUSY && try == 0) {
		close(sock);
		try = 1;
		goto again;
	}
	if (ret < 0) {
		fprintf(stderr, "Bind failed due to %s.\n", strerror(errno));
		goto done;
	}
	ret = ioctl(sock, IMCTRLREQ, &crq);
	if (ret < 0) {
		fprintf(stderr, "IMCTRLREQ failed for port %d. Port seems not to be a layer1-over-ip device.\n", port);
		goto done;
	}

	/* output the port info */
	printf("Peer settings for port %d (%s)\n", port, devinfo.name);
	if (crq.p1 == 0)
		printf("No peer set.\n");
	else {
		printf("remote IP  : %d.%d.%d.%d\n", (unsigned int)crq.p1 >> 24, (crq.p1 >> 16) & 0xff, (crq.p1 >> 8) & 0xff, crq.p1 & 0xff);
		printf("remote port: %d\n", crq.p2 & 0xffff);
		if (crq.p2 >> 16)
			printf("local port : %d\n", (unsigned int)crq.p2 >> 16);
		else
			printf("local port : set automatically\n");
	}

done:
	close(sock);
	return(0);
}

