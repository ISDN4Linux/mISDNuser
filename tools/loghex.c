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
#include <mISDNif.h>
#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>

static void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options]\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n");
	fprintf(stderr,"  -w <file>       write wiresharkdump <file>\n");
	fprintf(stderr,"\n");
}


static void write_esc (FILE *file, unsigned char *buf, int len)
{
    int i, byte;
    
    for (i = 0; i < len; ++i) {
		byte = buf[i];
		if (byte == 0xff || byte == 0xfe) {
			fputc(0xfe, file);
			byte -= 2;
		}
		fputc(byte, file);
	}

	if (ferror(file)) {
		fprintf(stderr, "Error on writing to file!\nAborting...");
		exit(1);
	}
}

static void write_wfile(FILE *f, unsigned char *buf, int len, struct timeval *tv, int protocol)
{
	struct mISDNhead	*hh = (struct mISDNhead *)buf;
	u_char			head[12], origin;

	if ((hh->prim != PH_DATA_REQ) && (hh->prim != PH_DATA_IND))
		return;
	if (protocol == ISDN_P_NT_S0 || protocol == ISDN_P_NT_E1)
		origin = hh->prim == PH_DATA_REQ ? 0 : 1;
	else
		origin = hh->prim == PH_DATA_REQ ? 1 : 0;

	len -= MISDN_HEADER_LEN;

	fputc(0xff, f);

	head[0] = (unsigned char)(0xff & (tv->tv_usec >> 16));
	head[1] = (unsigned char)(0xff & (tv->tv_usec >> 8));
	head[2] = (unsigned char)(0xff & tv->tv_usec);
	head[3] = (unsigned char)0;
	head[4] = (unsigned char)(0xff & (tv->tv_sec >> 24));
	head[5] = (unsigned char)(0xff & (tv->tv_sec >> 16));
	head[6] = (unsigned char)(0xff & (tv->tv_sec >> 8));
	head[7] = (unsigned char)(0xff & tv->tv_sec);
	head[8] = (unsigned char) 0;
	head[9] = (unsigned char) origin;
	head[10]= (unsigned char)(0xff & (len >> 8));
	head[11]= (unsigned char)(0xff & len);

	write_esc(f, head, 12);
	write_esc(f, &buf[MISDN_HEADER_LEN], len);
	fflush(f);
}


static void printhex(unsigned char *p, int len)
{
	int	i;

	for (i = 1; i <= len; i++) {
		printf(" %02x", *p++);
		if ((i!=len) && !(i % 16))
			printf("\n                                 ");
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
	int	aidx=1, idx, i;
	int	cardnr = 0;
	int	log_socket;
	struct sockaddr_mISDN  log_addr;
	int	buflen = 512;
	char	sw;
	char	wfilename[512];
	u_char	buffer[buflen];
	struct msghdr	mh;
	struct iovec	iov[1];
	struct ctstamp	cts;
	struct tm	*mt;
	int	result;
	int	opt;
	u_int	cnt;
	struct mISDN_devinfo	di;
	struct mISDNhead 	*hh;
	struct mISDNversion	ver;
	FILE	*wfile = NULL;
 
	*wfilename = 0;
	while (aidx < argc) {
		if (argv[aidx] && argv[aidx][0]=='-') {
			sw=argv[aidx][1];
			switch (sw) {
				case 'c':
					if (argv[aidx][2]) {
						cardnr=atol(&argv[aidx][2]);
					}
					break;
				case 'w':
					if (!argv[aidx][2]) {
						idx = 0;
						aidx++;
					} else {
						idx=2;
					}
					if (aidx<=argc) {
						if (512 <= strlen(&argv[aidx][idx])) {
							fprintf(stderr," -w filename too long\n");
							exit(1);
						}
						strcpy(wfilename, &argv[aidx][idx]);
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

	init_af_isdn();

	if ((log_socket = socket(PF_ISDN, SOCK_RAW, 0)) < 0) {
		printf("could not open socket %s\n", strerror(errno));
		exit(1);
	}
	
	result = ioctl(log_socket, IMGETVERSION, &ver);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	}
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
	log_addr.channel = 0;

	result = bind(log_socket, (struct sockaddr *) &log_addr,
		 sizeof(log_addr));
	printf("log bind return %d\n", result);

	if (result < 0) {
		printf("log bind error %s\n", strerror(errno));
	}
	
	opt = 1;
	result = setsockopt(log_socket, SOL_MISDN, MISDN_TIME_STAMP, &opt, sizeof(opt));
	
	if (result < 0) {
		printf("log  setsockopt error %s\n", strerror(errno));
	}
	
	if (strlen(wfilename)) {
		wfile = fopen(wfilename, "w");
		if (wfile) {
			fprintf(wfile, "EyeSDN");
			fflush(wfile);
		} else 
			printf("cannot open wireshark dump file(%s)\n", wfilename);
	}

	hh = (struct mISDNhead *)buffer;

	while (1) {
		mh.msg_name = NULL;
		mh.msg_namelen = 0;
		mh.msg_iov = iov;
		mh.msg_iovlen = 1;
		mh.msg_control = &cts;
		mh.msg_controllen = sizeof(cts);
		mh.msg_flags = 0;
		iov[0].iov_base = buffer;
		iov[0].iov_len = buflen;
		result = recvmsg(log_socket, &mh, 0);
		if (result < 0) {
			printf("read error %s\n", strerror(errno));
			break;
		} else {
			if (mh.msg_flags) {
				printf("received message with msg_flags(%x)\n", mh.msg_flags);
			}
			if (cts.cmsg_type == MISDN_TIME_STAMP) {
				mt = localtime((time_t *)&cts.tv.tv_sec);
				printf("%02d.%02d.%04d %02d:%02d:%02d.%06ld", mt->tm_mday, mt->tm_mon + 1, mt->tm_year + 1900,
					mt->tm_hour, mt->tm_min, mt->tm_sec, cts.tv.tv_usec);
			} else {
				cts.tv.tv_sec = 0;
				cts.tv.tv_usec = 0;
			}
			if (wfile && (result > MISDN_HEADER_LEN))
				write_wfile(wfile, buffer, result, &cts.tv, di.protocol);	
			printf(" received %3d bytes prim = %04x id=%08x",
				result, hh->prim, hh->id);
			if (result > MISDN_HEADER_LEN)
				printhex(&buffer[MISDN_HEADER_LEN], result - MISDN_HEADER_LEN);
			else
				printf("\n");
		}
	}
	close(log_socket);
	if (wfile)
		fclose(wfile);
	return (0);
}
