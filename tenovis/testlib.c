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


int			tid;
int			hid;
int			dl3id;
int			l2activ = 0;

#define BUFFER_SIZE	2048

u_char			buffer[BUFFER_SIZE];
u_char			msg[BUFFER_SIZE];
iframe_t		*frame;


int
init_lowdchannel(void)
{
	stack_info_t		*stinf;
	layer_info_t		linf;
	int			ret;
#ifdef OBSOLETE
	interface_info_t	iinf;
#endif
	int			dstid, dl2id;

	frame = (iframe_t *)buffer;
	hid = mISDN_open();
	if (hid < 0)
		return(-1);
	
	ret = mISDN_write_frame(hid, buffer, 0,
		MGR_SETDEVOPT | REQUEST, FLG_mISDNPORT_ONEFRAME,
		0, NULL, TIMEOUT_1SEC);
	fprintf(stdout, "MGR_SETDEVOPT ret(%d)\n", ret);
	ret = mISDN_read(hid, buffer, BUFFER_SIZE, TIMEOUT_10SEC);
	fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
		ret, frame->prim, frame->dinfo, frame->len);
	ret = mISDN_get_stack_count(hid);
	fprintf(stdout, "stackcnt %d\n", ret);
	if (ret <= 0) {
		mISDN_close(hid);
		return(-1);
	}
	ret = mISDN_get_stack_info(hid, 1, buffer, BUFFER_SIZE);
	stinf = (stack_info_t *)&frame->data.p;
	mISDNprint_stack_info(stdout, stinf);
	fprintf(stdout, "ext(%x) instcnt(%d) childcnt(%d)\n",
		stinf->extentions, stinf->instcnt, stinf->childcnt);
	dstid = stinf->id;
	dl2id = mISDN_get_layerid(hid, dstid, 2);
	fprintf(stdout, " dl2id = %08x\n", dl2id);
#ifdef OBSOLETE
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_get_interface_info(hid, &iinf);
	fprintf(stdout, "l2 up   own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(hid, &iinf);
	fprintf(stdout, "l2 down own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);

#endif
	memset(&linf, 0, sizeof(layer_info_t));
	strcpy(&linf.name[0], "tst L3");
	linf.object_id = -1;
	linf.extentions = EXT_INST_MIDDLE;
	linf.pid.protocol[stinf->instcnt] = ISDN_PID_ANY;
	linf.pid.layermask = ISDN_LAYER(stinf->instcnt);
	linf.st = dstid;
	dl3id = mISDN_new_layer(hid, &linf);
	fprintf(stdout, "mISDN_new_layer ret(%x)\n", dl3id);

	ret = mISDN_get_stack_info(hid, 1, buffer, BUFFER_SIZE);
	stinf = (stack_info_t *)&frame->data.p;
	mISDNprint_stack_info(stdout, stinf);

#ifdef OBSOLETE
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.extentions = EXT_INST_MIDDLE;
	iinf.owner = dl3id;
	iinf.peer = dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_write_frame(hid, buffer, dl3id,
		MGR_SETIF | REQUEST, 0, sizeof(interface_info_t),
		&iinf, TIMEOUT_1SEC);
	fprintf(stdout, "mISDN_write_frame ret(%d)\n", ret);
	ret = mISDN_read(hid, buffer, 1024, TIMEOUT_10SEC);
	fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
		ret, frame->prim, frame->dinfo,
		frame->len);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_get_interface_info(hid, &iinf);
	fprintf(stdout, "l2 up   own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(hid, &iinf);
	fprintf(stdout, "l2 down own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
#endif
	return(0);
}


int
close_lowdchannel(void)
{
	int	ret;

	ret = mISDN_write_frame(hid, buffer, dl3id, MGR_DELLAYER | REQUEST,
		0, 0, NULL, TIMEOUT_1SEC);
	fprintf(stdout, "MGR_DELLAYER ret(%d)\n", ret);
	ret = mISDN_read(hid, buffer, 1024, TIMEOUT_10SEC);
	fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
		ret, frame->prim, frame->dinfo, frame->len);
	return(0);
}

int
process_lowdchannel(int len)
{
	int	ret;

	fprintf(stderr, "mISDN_read pr(%x) di(%x) l(%d)\n",
		frame->prim, frame->dinfo, frame->len);
	if (frame->prim == (DL_DATA | INDICATION)) {
		ret = DL3write(tid, &frame->data.p, frame->len);
		fprintf(stderr, "DL3write ret(%d)\n", ret);
	} else if (frame->prim == (DL_UNITDATA | INDICATION)) {
		ret = DL3write(tid, &frame->data.p, frame->len);
		fprintf(stderr, "DL3write ret(%d)\n", ret);
	} else if (frame->prim == (DL_ESTABLISH | CONFIRM)) {
		fprintf(stderr, "estab cnf\n");
		l2activ = 1;
	} else if (frame->prim == (DL_ESTABLISH | INDICATION)) {
		fprintf(stderr, "estab ind\n");
		l2activ = 1;
	} else if (frame->prim == (DL_RELEASE | CONFIRM)) {
		fprintf(stderr, "release cnf\n");
		l2activ = 0;
	} else if (frame->prim == (DL_RELEASE | INDICATION)) {
		fprintf(stderr, "release ind\n");
		l2activ = 0;
	}
	return(0);
}

int
send_lowdchannel(int len)
{
	int	ret;

	if (!l2activ) {
		ret = mISDN_write_frame(hid, buffer, dl3id | FLG_MSG_TARGET | FLG_MSG_DOWN,
			DL_ESTABLISH | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
		fprintf(stderr, "estab req ret(%d)\n", ret);
		ret = mISDN_read(hid, buffer, 1024, TIMEOUT_10SEC);
		fprintf(stderr, "estab read ret(%d)\n", ret);
		if (ret >= 16) {
			if (frame->prim == (DL_ESTABLISH | CONFIRM)) {
				fprintf(stderr, "estab cnf\n");
				l2activ = 1;
			}
			if (frame->prim == (DL_ESTABLISH | INDICATION)) {
				fprintf(stderr, "estab ind\n");
				l2activ = 1;
			}
		}
	}
	ret = mISDN_write_frame(hid, buffer, dl3id | FLG_MSG_TARGET | FLG_MSG_DOWN,
		DL_DATA | REQUEST, 0, len, msg, TIMEOUT_1SEC);
	return(ret);
}


static void
term_handler(int sig)
{
	int	err;

	fprintf(stderr,"signal %d received\n", sig);
	close_lowdchannel();
	err = DL3close(tid);
	fprintf(stderr,"DL3close returns %d\n", err);
	exit(0);		
}


int main(argc,argv)
int argc;
char *argv[];
{
	int		ret, n;
	int		err;
	fd_set		in;

	fprintf(stderr,"%s\n", argv[0]);
	tid = DL3open();
	fprintf(stderr,"DL3open returns %d\n", tid);
	if (tid<0) {
		fprintf(stderr,"DL3open error %s\n", strerror(errno));
		exit(1);
	}
	if (init_lowdchannel()) {
		DL3close(tid);
		exit(1);
	}
	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	signal(SIGPIPE, term_handler);
	while(1) {
		FD_ZERO(&in);
		n = fileno(stdin);
		FD_SET(n, &in);
		if (n<tid)
			n = tid;
		FD_SET(tid, &in);
		if (n<hid)
			n = hid;
		FD_SET(hid, &in);
		n++;
		ret = select(n, &in, NULL, NULL, NULL);
		if (ret<0)
			continue;
		if (FD_ISSET(fileno(stdin), &in))
			break;
		if (FD_ISSET(tid, &in)) {
			ret = DL3read(tid, msg, 2048);
			if (ret == -2) {
				fprintf(stderr,"DL3read internal processing\n");
			} else if (ret == -1) {
				fprintf(stderr,"DL3read errno %d: %s\n",
					errno, strerror(errno)); 
			} else if (ret == 0) {
				fprintf(stderr,"DL3read empty frame ?\n");
			} else {
				fprintf(stderr,"DL3read %d bytes\n", ret);
				send_lowdchannel(ret);
			}
		}
		if (FD_ISSET(hid, &in)) {
			ret = mISDN_read(hid, buffer, 1024, TIMEOUT_1SEC);
			if (ret < 0) {
				fprintf(stderr,"mISDN_read errno %d: %s\n",
					errno, strerror(errno));
			} else if (ret == 0) {
				fprintf(stderr,"mISDN_read empty frame ?\n");
			} else {
				fprintf(stderr,"mISDN_read %d bytes\n", ret);
				if (ret < 16) {
					fprintf(stderr,"mISDN_read incomplete frame\n");
					continue;
				}
				process_lowdchannel(ret);
			}
		}
	}
	err = close_lowdchannel();
	fprintf(stderr,"close_lowdchannel returns %d\n", err);
	err = DL3close(tid);
	fprintf(stderr,"DL3close returns %d\n", err);
	return(0);
}
