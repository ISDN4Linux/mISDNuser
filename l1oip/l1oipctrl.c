/*****************************************************************************\
**                                                                           **
** isdnbridge                                                                **
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

int main(int argc, char *argv[])
{
	int ret;
	int count;
	int sock;
	struct hostent *hostent;
	struct servent *servent;
	struct mISDN_ctrl_req crq;

	memset(&crq, 0, sizeof(crq));

	/* usage */
	if (argc <= 1) {
usage:
		printf(
			"Usage: %s <port> [param]\n"
			"Param is:\n"
			"  unsetpeer -> releases current peer IP\n"
			"  setpeer <remote host> <remote port> [<local port>]\n"
			, argv[0]);
		return(0);
	}
	if (argc <= 2) {
		fprintf(stderr, "Missing parameter.\n");
		goto usage;
	}
	crq.dev = atoi(argv[1]);
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
		servent = getservbyname(argv[3], "udp");
		if (!servent) {
			fprintf(stderr, "Given port %s not found.\n", argv[4]);
			return(0);
		}
		crq.p2 = ntohs(servent->s_port);
		if (argc > 4) {
			servent = getservbyname(argv[4], "udp");
			if (!servent) {
				fprintf(stderr, "Given port %s not found.\n", argv[5]);
				return(0);
			}
			crq.p2 |= ntohs(servent->s_port) << 16;
		}
	} else
	if (!strcmp(argv[2], "unsetpeer")) {
		crq.op = MISDN_CTRL_UNSETPEER;
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

	ret = ioctl(sock, IMCTRLREQ, &crq);
	if (ret < 0) {
		fprintf(stderr, "IMCTRLREQ failed for port %d. (ret=%d)\n", crq.dev, ret);
		goto done;
	}

	/* output the port info */
	printf("Done.");

done:
	close(sock);
	return(0);
}

