#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include "mISDNlib.h"
#include "lib/tenovis.h"
#include "l3dss1.h"

#define	make_dss1_head(f, cr, mt) \
		*f++ = 8;\
		*f++ = 1;\
		*f++ = cr;\
		*f++ = mt

int	tid;
u_char	callref;

static void
term_handler(int sig)
{
	int	err;

	fprintf(stderr,"signal %d received\n", sig);
	err = DL3close(tid);
	fprintf(stderr,"DL3close returns %d\n", err);
	exit(0);		
}


int
handle_msg(u_char *imsg, int len)
{
	u_char	mt,cr;
	u_char	omsg[2048], *p;
	int 	ret;

	cr = imsg[2];
	callref = cr;
	mt = imsg[3];
	p = omsg;
	switch(mt) {
		case MT_SETUP:
			make_dss1_head(p, (cr ^ 0x80), MT_CALL_PROCEEDING);
			*p++ = IE_CHANNEL_ID;
			*p++ = 3;
			*p++ = 0xa1;
			*p++ = 0x83;
			*p++ = 0x05;
			ret = DL3write(tid, omsg, p - omsg);
			fprintf(stderr,"CALLP write ret %d\n", ret);
			p = omsg;
			make_dss1_head(p, (cr ^ 0x80), MT_ALERTING);
			ret = DL3write(tid, omsg, p - omsg);
			fprintf(stderr,"ALERT write ret %d\n", ret);
			p = omsg;
			make_dss1_head(p, (cr ^ 0x80), MT_CONNECT);
			ret = DL3write(tid, omsg, p - omsg);
			fprintf(stderr,"CONN write ret %d\n", ret);
			break;
		case MT_DISCONNECT:
			make_dss1_head(p, (cr ^= 0x80), MT_RELEASE);
			ret = DL3write(tid, omsg, p - omsg);
			fprintf(stderr,"REL write ret %d\n", ret);
			break;
		case MT_RELEASE:
			make_dss1_head(p, (cr ^= 0x80), MT_RELEASE_COMPLETE);
			ret = DL3write(tid, omsg, p - omsg);
			fprintf(stderr,"RELC write ret %d\n", ret);
			break;
		case MT_RELEASE_COMPLETE:
			fprintf(stderr,"got RELC len(%d)\n", len);
			break;
		case MT_STATUS:
			fprintf(stderr,"got STATUS cr(%x) len(%d)\n",
				cr, len);
			break;
		default:
			fprintf(stderr,"got mt(%x) cr(%x) len(%d)\n",
				mt, cr, len);
			break;
	}
	return(0);
}

int main(argc,argv)
int argc;
char *argv[];

{
	u_char		imsg[2048];
	u_char		omsg[2048], *p;
	int		ret, n, loop = 1;
	int		err;
	fd_set		in;

	fprintf(stderr,"%s\n", argv[0]);
	tid = DL3open();
	fprintf(stderr,"DL3open returns %d\n", tid);
	if (tid<0) {
		fprintf(stderr,"DL3open error %s\n", strerror(errno));
		exit(1);
	}
	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	signal(SIGPIPE, term_handler);
	while(loop) {
		FD_ZERO(&in);
		n = fileno(stdin);
		FD_SET(n, &in);
		if (n<tid)
			n = tid;
		FD_SET(tid, &in);
		n++;
		ret = select(n, &in, NULL, NULL, NULL);
		if (ret<0)
			continue;
		if (FD_ISSET(fileno(stdin), &in)) {
			fgets(imsg, 2048, stdin);
			switch(imsg[0]) {
				case 'q':
				case 'Q':
					loop = 0;
					break;
				case 'h':
				case 'H':
					p = omsg;
					make_dss1_head(p, (callref ^ 0x80),
						MT_DISCONNECT);
					*p++ = IE_CAUSE;
					*p++ = 2;
					*p++ = 0x80;
					*p++ = 0x90;
					ret = DL3write(tid, omsg, p - omsg);
					fprintf(stderr,"DISC write ret %d\n", ret);
					break;
				default:
					fprintf(stderr,"commands are (h)angup and (q)uit\n");
					break;
			}
		}
		if (FD_ISSET(tid, &in)) {
			ret = DL3read(tid, imsg, 2048);
			if (ret>0)
				handle_msg(imsg, ret);
		}
	}
	err = DL3close(tid);
	fprintf(stderr,"DL3close returns %d\n", err);
	return(0);
}
