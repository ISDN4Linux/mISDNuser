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
#include <linux/mISDNif.h>

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


void printhex(unsigned char *p, int len)
{
	int	i;

	for (i = 1; i <= len; i++) {
		printf(" %02x", *p++);
		if (!(i % 16))
			printf("\n");
	}
	printf("\n");
}

int
main(argc, argv)
int argc;
char *argv[];
{
	int aidx=1,para=1, idx;
	int cardnr = 1;
	int log_socket;
	struct sockaddr_mISDN  log_addr;
	int buflen = 512;
	char sw, buffer[buflen];
	int result, ret;
	int alen;
	u_int cnt;
	struct mISDN_devinfo	di;
	struct mISDNhead 	*hh;


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

	if (cardnr < 1) {
		fprintf(stderr,"card nr %d wrong it should be 1 ... nr of installed cards\n", cardnr);
		exit(1);
	}
	if ((log_socket = socket(AF_ISDN, SOCK_RAW, 0)) < 0) {
		printf("could not open socket %s\n", strerror(errno));
		exit(1);
	}
	result = ioctl(log_socket, IMGETCOUNT, &cnt);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	} else
		printf("%d controller found\n", cnt);

	if (cardnr > cnt) {
		fprintf(stderr,"card nr %d wrong it should be 1 ... nr of installed cards (%d)\n", cardnr, cnt);
	}
	di.id = cardnr - 1;
	result = ioctl(log_socket, IMGETDEVINFO, &di);
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

	close(log_socket);

	if (di.protocol == ISDN_P_NONE) /* default TE */
		di.protocol = ISDN_P_TE_S0;

	if ((log_socket = socket(AF_ISDN, SOCK_DGRAM, di.protocol)) < 0) {
		printf("could not open log socket %s\n", strerror(errno));
		exit(1);
	}

	log_addr.family = AF_ISDN;
	log_addr.dev = cardnr - 1;
	log_addr.channel = 0;

	result = bind(log_socket, (struct sockaddr *) &log_addr,
		 sizeof(log_addr));
	printf("log bind return %d\n", result);

	if (result < 0) {
		printf("log bind error %s\n", strerror(errno));
	}

	hh = (struct mISDNhead *)buffer;

	while (1) {
		result = recvfrom(log_socket, buffer, 512, 0, NULL, NULL);
		if (result < 0) {
			printf("read error %s\n", strerror(errno));
			break;
		} else {
			printf("received %d bytes prim = %x id=%x len=%d\n",
				result, hh->prim, hh->id, hh->len);
			if (result > MISDN_HEADER_LEN)
				printhex(&buffer[MISDN_HEADER_LEN], result - MISDN_HEADER_LEN);
		}
	}
	close(log_socket);
	return (0);
}
