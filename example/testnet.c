#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "mISDNlib.h"
#include "l3dss1.h"


unsigned char ulaw_to_Alaw[256] = {
     0x2a, 0x2b, 0x28, 0x29, 0x2e, 0x2f, 0x2c, 0x2d,
     0x22, 0x23, 0x20, 0x21, 0x26, 0x27, 0x24, 0x25,
     0x3a, 0x3b, 0x38, 0x39, 0x3e, 0x3f, 0x3c, 0x3d,
     0x32, 0x33, 0x30, 0x31, 0x36, 0x37, 0x34, 0x35,
     0x0b, 0x08, 0x09, 0x0e, 0x0f, 0x0c, 0x0d, 0x02,
     0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 0x1a,
     0x1b, 0x18, 0x19, 0x1e, 0x1f, 0x1c, 0x1d, 0x12,
     0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 0x6b,
     0x68, 0x69, 0x6e, 0x6f, 0x6c, 0x6d, 0x62, 0x63,
     0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 0x7b, 0x79,
     0x7e, 0x7f, 0x7c, 0x7d, 0x72, 0x73, 0x70, 0x71,
     0x76, 0x77, 0x74, 0x75, 0x4b, 0x49, 0x4f, 0x4d,
     0x42, 0x43, 0x40, 0x41, 0x46, 0x47, 0x44, 0x45,
     0x5a, 0x5b, 0x58, 0x59, 0x5e, 0x5f, 0x5c, 0x5d,
     0x52, 0x52, 0x53, 0x53, 0x50, 0x50, 0x51, 0x51,
     0x56, 0x56, 0x57, 0x57, 0x54, 0x54, 0x55, 0xd5,
     0xaa, 0xab, 0xa8, 0xa9, 0xae, 0xaf, 0xac, 0xad,
     0xa2, 0xa3, 0xa0, 0xa1, 0xa6, 0xa7, 0xa4, 0xa5,
     0xba, 0xbb, 0xb8, 0xb9, 0xbe, 0xbf, 0xbc, 0xbd,
     0xb2, 0xb3, 0xb0, 0xb1, 0xb6, 0xb7, 0xb4, 0xb5,
     0x8b, 0x88, 0x89, 0x8e, 0x8f, 0x8c, 0x8d, 0x82,
     0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 0x9a,
     0x9b, 0x98, 0x99, 0x9e, 0x9f, 0x9c, 0x9d, 0x92,
     0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 0xeb,
     0xe8, 0xe9, 0xee, 0xef, 0xec, 0xed, 0xe2, 0xe3,
     0xe0, 0xe1, 0xe6, 0xe7, 0xe4, 0xe5, 0xfb, 0xf9,
     0xfe, 0xff, 0xfc, 0xfd, 0xf2, 0xf3, 0xf0, 0xf1,
     0xf6, 0xf7, 0xf4, 0xf5, 0xcb, 0xc9, 0xcf, 0xcd,
     0xc2, 0xc3, 0xc0, 0xc1, 0xc6, 0xc7, 0xc4, 0xc5,
     0xda, 0xdb, 0xd8, 0xd9, 0xde, 0xdf, 0xdc, 0xdd,
     0xd2, 0xd2, 0xd3, 0xd3, 0xd0, 0xd0, 0xd1, 0xd1,
     0xd6, 0xd6, 0xd7, 0xd7, 0xd4, 0xd4, 0xd5, 0xd5,
    
};

unsigned char Alaw_to_ulaw[256] = {
     0x29, 0x2a, 0x27, 0x28, 0x2d, 0x2e, 0x2b, 0x2c,
     0x21, 0x22, 0x1f, 0x20, 0x25, 0x26, 0x23, 0x24,
     0x39, 0x3a, 0x37, 0x38, 0x3d, 0x3e, 0x3b, 0x3c,
     0x31, 0x32, 0x2f, 0x30, 0x35, 0x36, 0x33, 0x34,
     0x0a, 0x0b, 0x08, 0x09, 0x0e, 0x0f, 0x0c, 0x0d,
     0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05,
     0x1a, 0x1b, 0x18, 0x19, 0x1e, 0x1f, 0x1c, 0x1d,
     0x12, 0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15,
     0x62, 0x63, 0x60, 0x61, 0x66, 0x67, 0x64, 0x65,
     0x5d, 0x5d, 0x5c, 0x5c, 0x5f, 0x5f, 0x5e, 0x5e,
     0x74, 0x76, 0x70, 0x72, 0x7c, 0x7e, 0x78, 0x7a,
     0x6a, 0x6b, 0x68, 0x69, 0x6e, 0x6f, 0x6c, 0x6d,
     0x48, 0x49, 0x46, 0x47, 0x4c, 0x4d, 0x4a, 0x4b,
     0x40, 0x41, 0x3f, 0x3f, 0x44, 0x45, 0x42, 0x43,
     0x56, 0x57, 0x54, 0x55, 0x5a, 0x5b, 0x58, 0x59,
     0x4f, 0x4f, 0x4e, 0x4e, 0x52, 0x53, 0x50, 0x51,
     0xa9, 0xaa, 0xa7, 0xa8, 0xad, 0xae, 0xab, 0xac,
     0xa1, 0xa2, 0x9f, 0xa0, 0xa5, 0xa6, 0xa3, 0xa4,
     0xb9, 0xba, 0xb7, 0xb8, 0xbd, 0xbe, 0xbb, 0xbc,
     0xb1, 0xb2, 0xaf, 0xb0, 0xb5, 0xb6, 0xb3, 0xb4,
     0x8a, 0x8b, 0x88, 0x89, 0x8e, 0x8f, 0x8c, 0x8d,
     0x82, 0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85,
     0x9a, 0x9b, 0x98, 0x99, 0x9e, 0x9f, 0x9c, 0x9d,
     0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95,
     0xe2, 0xe3, 0xe0, 0xe1, 0xe6, 0xe7, 0xe4, 0xe5,
     0xdd, 0xdd, 0xdc, 0xdc, 0xdf, 0xdf, 0xde, 0xde,
     0xf4, 0xf6, 0xf0, 0xf2, 0xfc, 0xfe, 0xf8, 0xfa,
     0xea, 0xeb, 0xe8, 0xe9, 0xee, 0xef, 0xec, 0xed,
     0xc8, 0xc9, 0xc6, 0xc7, 0xcc, 0xcd, 0xca, 0xcb,
     0xc0, 0xc1, 0xbf, 0xbf, 0xc4, 0xc5, 0xc2, 0xc3,
     0xd6, 0xd7, 0xd4, 0xd5, 0xda, 0xdb, 0xd8, 0xd9,
     0xcf, 0xcf, 0xce, 0xce, 0xd2, 0xd3, 0xd0, 0xd1,
    
};

void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options] [filename]\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"       filename   filename.in  incoming data\n");
	fprintf(stderr,"                  filename.out outgoing data\n");
	fprintf(stderr,"                  data is sun audio 8khz 8bi for voice\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n");
	fprintf(stderr,"  -d <text>       Display text (default \"Test Display\")\n");
	fprintf(stderr,"  -m <number>     Called PN (default 789)\n");
	fprintf(stderr,"  -n <number>     Calling PN (default keine)\n");
	fprintf(stderr,"  -F<n>           use function n (default 0)\n"); 
	fprintf(stderr,"                    0 outgoing call\n"); 
	fprintf(stderr,"                    1 incomming call\n"); 
	fprintf(stderr,"  -vn             Printing debug info level n\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int	device;
	int	cardnr;
	int	func;
	char	phonenr[32];
	char	display[32];
	char	msn[32];
	int	d_stid;
	int	layer1;
	int	layer2;
	int	layer3;
	int	b_stid[2];
	int	b_adress[2];
	int	used_bchannel;
	int	save;
	int	play;
	FILE	*fplay;
	int	flag;
	int	val;
	int	cr;
	int	si;
	int	bl1_prot;
	int	bl2_prot;
	int	bl3_prot;
} devinfo_t;

#define FLG_SEND_TONE		0x0001
#define FLG_SEND_DATA		0x0002
#define FLG_BCHANNEL_SETUP	0x0010
#define FLG_BCHANNEL_DOACTIVE	0x0020
#define FLG_BCHANNEL_ACTIVE	0x0040
#define FLG_BCHANNEL_ACTDELAYED	0x0080
#define FLG_CALL_ORGINATE	0x0100


#define MAX_REC_BUF		4000
#define MAX_DATA_BUF		1024

static int VerifyOn=0;

char tt_char[]="0123456789ABCD*#";

#define PLAY_SIZE 64

#define	MsgHead(ptr, cref, mty) \
	*ptr++ = 0x8; \
	if (cref == -1) { \
		*ptr++ = 0x0; \
	} else { \
		*ptr++ = 0x1; \
		*ptr++ = cref^0x80; \
	} \
	*ptr++ = mty

int save_alaw(devinfo_t *di, unsigned char *buf, int len) {
	int i;
	unsigned char *p = buf;
	
	for (i=0; i<len; i++) {
		*p = ulaw_to_Alaw[*p];
		p++;
	}
	write(di->save, buf, len);
	return(len);		
}

int play_msg(devinfo_t *di) {
	unsigned char buf[PLAY_SIZE+mISDN_HEADER_LEN], *p;
	iframe_t *frm = (iframe_t *)buf;
	int len, ret, i;
	
	if (di->play<0)
		return(0);
	len = read(di->play, buf + mISDN_HEADER_LEN, PLAY_SIZE);
	if (len<0) {
		printf("play_msg err %d: \"%s\"\n", errno, strerror(errno));
		close(di->play);
		di->play = -1;
	}
	p = buf + mISDN_HEADER_LEN;
	for (i=0; i<len; i++) {
		*p = Alaw_to_ulaw[*p];
		p++;
	}
	frm->addr = di->b_adress[di->used_bchannel] | FLG_MSG_TARGET | FLG_MSG_DOWN;
	frm->prim = DL_DATA | REQUEST;
	frm->dinfo = 0;
	frm->len = len;
	ret = mISDN_write(di->device, buf, len+mISDN_HEADER_LEN, 8000);
	if (ret < 0)
		fprintf(stdout,"play write error %d %s\n", errno, strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"play write ret=%d\n", ret);
	return(ret);
}

int send_data(devinfo_t *di) {
	unsigned char buf[MAX_DATA_BUF+mISDN_HEADER_LEN];
	iframe_t *frm = (iframe_t *)buf;
	unsigned char *data;
	int len, ret;
	
	if (di->play<0 || !di->fplay)
		return(0);
	if (!(data = fgets(buf + mISDN_HEADER_LEN, MAX_DATA_BUF, di->fplay))) {
		close(di->play);
		di->play = -1;
		data = buf + mISDN_HEADER_LEN;
		data[0] = 4; /* ctrl-D */
		data[1] = 0;
	}
	len = strlen(data);
	if (len==0) {
		close(di->play);
		di->play = -1;
		data[0] = 4; /* ctrl-D */
		len = 1;
	}
	
	frm->addr = di->b_adress[di->used_bchannel] | FLG_MSG_TARGET | FLG_MSG_DOWN;
	frm->prim = DL_DATA | REQUEST;
	frm->dinfo = 0;
	frm->len = len;
	ret = mISDN_write(di->device, buf, len+mISDN_HEADER_LEN, 100000);
	if (ret < 0)
		fprintf(stdout,"send_data write error %d %s\n", errno, strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"send_data write ret=%d\n", ret);
	return(ret);
}

int setup_bchannel(devinfo_t *di) {
	mISDN_pid_t pid;
	int ret;
	layer_info_t li;


	if ((di->used_bchannel<0) || (di->used_bchannel>1)) {
		fprintf(stdout, "wrong channel %d\n", di->used_bchannel);
		return(0);
	}
	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "B L3");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[3] = di->bl3_prot;
	li.pid.layermask = ISDN_LAYER(3);
	li.st = di->b_stid[di->used_bchannel];
	ret = mISDN_new_layer(di->device, &li);
	if (ret<0) {
		fprintf(stdout, "new_layer ret(%d)\n", ret);
		return(0);
	}
	if (ret) {
		di->b_adress[di->used_bchannel] = ret;
		if (VerifyOn>2)
			fprintf(stdout,"b_adress%d %08x\n",
				di->used_bchannel+1, ret);
		memset(&pid, 0, sizeof(mISDN_pid_t));
		pid.protocol[1] = di->bl1_prot;
		pid.protocol[2] = di->bl2_prot;
		pid.protocol[3] = di->bl3_prot;
		pid.layermask = ISDN_LAYER(1) | ISDN_LAYER(2)| ISDN_LAYER(3);
		if (di->flag & FLG_CALL_ORGINATE)
			pid.global = 1;
		ret = mISDN_set_stack(di->device,
			di->b_stid[di->used_bchannel], &pid);
		if (ret) {
			fprintf(stdout, "set_stack ret(%d)\n", ret);
			return(0);
		}
		ret = di->b_adress[di->used_bchannel];
	}
	return(ret);
}

int send_SETUP(devinfo_t *di, int SI, char *PNr) {
	unsigned char *np, *p, *msg, buf[1024];
	int len, ret;

	p = msg = buf + mISDN_HEADER_LEN;
	MsgHead(p, di->cr, MT_SETUP);
	*p++ = 0xa1; /* complete indicator */
	*p++ = IE_BEARER;
	if (SI == 1) { /* Audio */
		*p++ = 0x3;	/* Length                               */
		*p++ = 0x90;	/* Coding Std. CCITT, 3.1 kHz audio     */
		*p++ = 0x90;	/* Circuit-Mode 64kbps                  */
		*p++ = 0xa3;	/* A-Law Audio                          */
	} else { /* default Datatransmission 64k */
		*p++ = 0x2;	/* Length                               */
		*p++ = 0x88;	/* Coding Std. CCITT, unrestr. dig. Inf */
		*p++ = 0x90;	/* Circuit-Mode 64kbps                  */
	}
	*p++ = IE_CHANNEL_ID;
	*p++ = 0x1;	/* Length */
	*p++ = 0x80 | (1 + di->used_bchannel);
	if (strlen(di->display)) {
		*p++ = IE_DISPLAY;
		*p++ = strlen(di->display);
		np = di->display;
		while(*np)
			*p++ = *np++ & 0x7f;
	}
	if (strlen(di->msn)) {
		*p++ = IE_CALLING_PN;
		*p++ = strlen(PNr) +2;
		*p++ = 0x01;
		*p++ = 0x80;
		np = PNr;
		while(*np)
			*p++ = *np++ & 0x7f;
	}
	if (PNr && strlen(PNr)) {
		*p++ = IE_CALLED_PN;
		np = di->msn;
		*p++ = strlen(np) + 1;
		/* Classify as AnyPref. */
		*p++ = 0x81;		/* Ext = '1'B, Type = '000'B, Plan = '0001'B. */
		while (*np)
			*p++ = *np++ & 0x7f;
	}
	len = p - msg;
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN,
		DL_UNITDATA | REQUEST, 0, len, msg, TIMEOUT_1SEC);
	return(ret);
}

int activate_bchan(devinfo_t *di) {
	unsigned char buf[128];
	iframe_t *rfrm;
	int ret;

	ret = mISDN_write_frame(di->device, buf,
		di->b_adress[di->used_bchannel] | FLG_MSG_TARGET | FLG_MSG_DOWN,
		DL_ESTABLISH | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"DL_ESTABLISH write ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 128, TIMEOUT_10SEC); 	
	if (VerifyOn>3)
		fprintf(stdout,"DL_ESTABLISH read ret=%d\n", ret);
	rfrm = (iframe_t *)buf;
	if (ret>0) {
		if (rfrm->prim == (DL_ESTABLISH | CONFIRM)) {
			di->flag |= FLG_BCHANNEL_ACTIVE;
		}
	}
	return(ret);
}

int deactivate_bchan(devinfo_t *di) {
	unsigned char buf[128];
	int ret;

	ret = mISDN_write_frame(di->device, buf,
		di->b_adress[di->used_bchannel] | FLG_MSG_TARGET | FLG_MSG_DOWN,
		DL_RELEASE | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"DL_RELEASE write ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 128, TIMEOUT_10SEC); 	
	if (VerifyOn>3)
		fprintf(stdout,"DL_RELEASE read ret=%d\n", ret);
	di->flag &= ~FLG_BCHANNEL_ACTIVE;
	di->flag &= ~FLG_BCHANNEL_SETUP;
	ret = mISDN_clear_stack(di->device, di->b_stid[di->used_bchannel]);
	if (VerifyOn>3)
		fprintf(stdout,"clear_stack ret=%d\n", ret);
	return(ret);
}

int send_touchtone(devinfo_t *di, int tone) {
	iframe_t frm;
	int tval, ret;

	if (VerifyOn>1)
		fprintf(stdout,"send_touchtone %c\n", DTMF_TONE_MASK & tone);
	tval = DTMF_TONE_VAL | tone;
	ret = mISDN_write_frame(di->device, &frm,
		di->b_adress[di->used_bchannel] | FLG_MSG_TARGET | FLG_MSG_DOWN,
		PH_CONTROL | REQUEST, 0, 4, &tval, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"tt send ret=%d\n", ret);
	return(ret);
}

int read_mutiplexer(devinfo_t *di) {
	unsigned char	*p, *msg, buf[MAX_REC_BUF];
	iframe_t	*rfrm;
	int		timeout = TIMEOUT_10SEC;
	int		ret = 0;
	int		len;

	rfrm = (iframe_t *)buf;
	/* Main loop */

start_again:
	while ((ret = mISDN_read(di->device, buf, MAX_REC_BUF, timeout))) {
		if (VerifyOn>3)
			fprintf(stdout,"readloop ret=%d\n", ret);
		if (ret >= 16) {
			if (VerifyOn>4)
				fprintf(stdout,"readloop addr(%x) prim(%x) len(%d)\n",
					rfrm->addr, rfrm->prim, rfrm->len);
			if (rfrm->addr == (di->b_adress[di->used_bchannel] | FLG_MSG_TARGET | FLG_MSG_DOWN)) {
				/* B-Channel related messages */
				if (rfrm->prim == (DL_DATA | INDICATION)) {
					/* received data, save it */
					save_alaw(di, (unsigned char *)&rfrm->data.i,
						rfrm->len);
				} else if (rfrm->prim == (DL_DATA | CONFIRM)) {
					/* get ACK of send data, so we can
					 * send more
					 */
					if (VerifyOn>5)
						fprintf(stdout,"DL_DATA_CNF\n");
					switch (di->func) {
						case 0:
						case 1:
							if (di->play > -1)
								play_msg(di);
							break;
					}
				} else if (rfrm->prim == (PH_CONTROL | INDICATION)) {
					if ((rfrm->len == 4) &&
						((rfrm->data.i & ~DTMF_TONE_MASK)
						== DTMF_TONE_VAL)) {
						fprintf(stdout,"GOT TT %c\n",
							DTMF_TONE_MASK & rfrm->data.i);
					} else
						fprintf(stdout,"unknown PH_CONTROL len %d/val %x\n",
							rfrm->len, rfrm->data.i);
				}
			/* D-Channel related messages */  
			} else if ((ret > 19) && (buf[19] == MT_CONNECT) &&
				(di->flag & FLG_CALL_ORGINATE)) {
				/* We got connect, so bring B-channel up */
				if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
					activate_bchan(di);
				else
					di->flag |= FLG_BCHANNEL_DOACTIVE;
				/* send a CONNECT_ACKNOWLEDGE */
				p = msg = buf + mISDN_HEADER_LEN;
				MsgHead(p, di->cr, MT_CONNECT_ACKNOWLEDGE);
				len = p - msg;
				ret = mISDN_write_frame(di->device, buf,
					di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN, DL_DATA | REQUEST,
					0, len, msg, TIMEOUT_1SEC);
				/* if here is outgoing data, send first part */
				switch (di->func) {
					case 0:
					case 1:
						if (di->play > -1)
							play_msg(di);
						break;
				}
			} else if ((ret > 19) && (buf[19] == MT_CONNECT_ACKNOWLEDGE) &&
				(!(di->flag & FLG_CALL_ORGINATE))) {
				/* We got connect ack, so bring B-channel up */
				if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
					activate_bchan(di);
				else
					di->flag |= FLG_BCHANNEL_DOACTIVE;
				/* if here is outgoing data, send first part */
				switch (di->func) {
					case 0:
					case 1:
						if (di->play > -1)
							play_msg(di);
						break;
				}
			} else if ((ret > 19) && (buf[19] == MT_DISCONNECT)) {
				/* send a RELEASE */
				p = msg = buf + mISDN_HEADER_LEN;
				MsgHead(p, di->cr, MT_RELEASE);
				len = p - msg;
				ret = mISDN_write_frame(di->device, buf,
					di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN, DL_DATA | REQUEST,
					0, len, msg, TIMEOUT_1SEC);
			} else if ((ret > 19) && (buf[19] == MT_RELEASE)) {
				/* on a disconnecting msg leave loop */
				/* send a RELEASE_COMPLETE */
				p = msg = buf + mISDN_HEADER_LEN;
				MsgHead(p, di->cr, MT_RELEASE_COMPLETE);
				len = p - msg;
				ret = mISDN_write_frame(di->device, buf,
					di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN, DL_DATA | REQUEST,
					0, len, msg, TIMEOUT_1SEC);
				return(2);
			} else if ((ret > 19) && (buf[19] == MT_RELEASE_COMPLETE)) {
				/* on a disconnecting msg leave loop */
				return(1);
			}
		}
	}
	if (di->flag & FLG_SEND_TONE) {
		if (di->val) {
			di->val--;
			send_touchtone(di, tt_char[di->val]);
		} else {
			/* After last tone disconnect */
			p = msg = buf + mISDN_HEADER_LEN;
			MsgHead(p, di->cr, MT_DISCONNECT);
			len = p - msg;
			ret = mISDN_write_frame(di->device, buf,
				di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN, DL_DATA | REQUEST,
				0, len, msg, TIMEOUT_1SEC);
			di->flag &= ~FLG_SEND_TONE;
		}
		goto start_again;
	} else if (di->flag & FLG_SEND_DATA) {
		if (di->play > -1)
			send_data(di);
		else
			di->flag &= ~FLG_SEND_DATA;
		goto start_again;
	} else if (di->flag & FLG_BCHANNEL_DOACTIVE) {
		ret = activate_bchan(di);
		if (!ret) {
			fprintf(stdout,"error on activate_bchan\n");
			return(0);
		}
		di->flag &= ~FLG_BCHANNEL_DOACTIVE;
		/* send next after 1 sec */
		timeout = 1*TIMEOUT_1SEC;
		di->flag |= FLG_SEND_DATA;
		goto start_again;
	}
	return(0);
}

int do_connection(devinfo_t *di) {
	unsigned char *p, *msg, buf[1024];
	iframe_t *rfrm;
	int len, idx, ret = 0;
	int bchannel;
	time_t		tim;
	struct tm	*ts;

	rfrm = (iframe_t *)buf;

	if (di->func == 0) {
		di->flag |= FLG_CALL_ORGINATE;
		di->cr = 0x81;
		send_SETUP(di, di->si, di->phonenr);
	}
	bchannel= di->used_bchannel + 1;
	/* Wait for a SETUP message or a CALL_PROCEEDING */
	while ((ret = mISDN_read(di->device, buf, 1024, 3*TIMEOUT_10SEC))) {
		if (VerifyOn>3)
			fprintf(stdout,"readloop ret=%d\n", ret);
		if (ret >= 20) {
			if (((!(di->flag & FLG_CALL_ORGINATE)) &&
				(buf[19] == MT_SETUP)) ||
				((di->flag & FLG_CALL_ORGINATE) &&
				(buf[19] == MT_ALERTING))) {
				
				if (!(di->flag & FLG_CALL_ORGINATE))
					di->cr = buf[18];
	 			idx = 20;
				break;
			}
		}
	}
	fprintf(stdout,"bchannel %d\n", bchannel);
	if (bchannel > 0) {
		/* setup a B-channel stack */
		switch (di->func) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				ret = setup_bchannel(di);
				if (ret)
					di->flag |= FLG_BCHANNEL_SETUP;
				else {
					fprintf(stdout,"error on setup_bchannel\n");
					goto clean_up;
				}
				break;
		}
		if (!(di->flag & FLG_CALL_ORGINATE)) {
			p = msg = buf + mISDN_HEADER_LEN;
			MsgHead(p, di->cr, MT_CONNECT);
			time(&tim);
			ts = localtime(&tim);
			if (ts->tm_year > 99)
				ts->tm_year -=100;
			printf("Time %d:%d %d/%d/%d\n",
				ts->tm_hour, ts->tm_min,
				ts->tm_mday, ts->tm_mon+1, ts->tm_year);
			*p++ = IE_CHANNEL_ID;
			*p++ = 0x1;	/* Length */
			*p++ = 0x80 | (1 + di->used_bchannel);
			*p++ = IE_DISPLAY;
			*p++ = strlen(di->display);
			for (idx=0; idx < strlen(di->display); idx++)
				*p++ = di->display[idx];
			*p++ = IE_DATE;
			*p++ = 5;
			*p++ = ts->tm_year;
			*p++ = ts->tm_mon+1;
			*p++ = ts->tm_mday;
			*p++ = ts->tm_hour;
			*p++ = ts->tm_min;
			len = p - msg;
			ret = mISDN_write_frame(di->device, buf,
				di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN, DL_DATA | REQUEST,
				0, len, msg, TIMEOUT_1SEC);
		}
		if (!read_mutiplexer(di)) { /* timed out */
			/* send a RELEASE_COMPLETE */
			fprintf(stdout,"read_mutiplexer timed out sending RELEASE_COMPLETE\n");
			p = msg = buf + mISDN_HEADER_LEN;;
			MsgHead(p, di->cr, MT_RELEASE_COMPLETE);
			len = p - msg;
			ret = mISDN_write_frame(di->device, buf,
				di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN, DL_DATA | REQUEST,
				0, len, msg, TIMEOUT_1SEC);
		}
		deactivate_bchan(di);
	} else {
		fprintf(stdout,"no channel or no connection\n");
	}
clean_up:
	sleep(1);
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN,
		DL_RELEASE | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	sleep(1);
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN,
		MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	return(0);
}



int
add_dlayer3(devinfo_t *di, int prot)
{
	layer_info_t li;
	stack_info_t si;
#ifdef OBSOLETE
	interface_info_t ii;
#endif
	int lid, ret;

	if (di->layer3) {
		memset(&si, 0, sizeof(stack_info_t));
		si.extentions = EXT_STACK_CLONE;
		si.mgr = -1;
		si.id = di->d_stid;
		ret = mISDN_new_stack(di->device, &si);
		if (ret <= 0) {
			fprintf(stdout, "clone stack failed ret(%d)\n", ret);
			return(11);
		}
		di->d_stid = ret;
	}
	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "user L3");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[3] = prot;
	li.pid.layermask = ISDN_LAYER(3);
	li.st = di->d_stid;
	lid = mISDN_new_layer(di->device, &li);
	if (lid<0)
		return(12);
	di->layer3 = lid;
	if (!di->layer3)
		return(13);
	
#ifdef OBSOLETE
	/* 
	 * EXT_IF_CREATE | EXT_IF_EXCLUSIV sorgen dafuer, das wenn die L3
	 * Schnittstelle schon benutzt ist, eine neue L2 Instanz erzeugt
	 * wird
	 */
   	
	ii.extentions = EXT_IF_CREATE | EXT_IF_EXCLUSIV;
	ii.owner = di->layer3;
	ii.peer = di->layer2;
	ii.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_connect(di->device, &ii);
	if (ret)
		return(13);
	ii.owner = di->layer3;
	ii.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(di->device, &ii);
	if (ret != 0)
		return(14);
	if (ii.peer == di->layer2)
		fprintf(stdout, "Layer 2 not cloned\n");
	else
		fprintf(stdout, "Layer 2 %08x cloned from %08x\n",
			ii.peer, di->layer2);
	di->layer2 = ii.peer;
#endif
	return(0);
}

int do_setup(devinfo_t *di) {
	unsigned char buf[1024];
	iframe_t *frm = (iframe_t *)buf;
	int i, ret = 0;
	stack_info_t *stinf;
	status_info_t *si;

	di->bl2_prot = ISDN_PID_L2_B_TRANS;
	di->bl3_prot = ISDN_PID_L3_B_TRANS;
	di->bl1_prot = ISDN_PID_L1_B_64TRANS;
	di->si = 1;

	ret = mISDN_get_stack_count(di->device);
	if (VerifyOn>1)
		fprintf(stdout,"%d stacks found\n", ret);
	if (ret < di->cardnr) {
		fprintf(stdout,"cannot config card nr %d only %d cards\n",
			di->cardnr, ret);
		return(2);
	}
	ret = mISDN_get_stack_info(di->device, di->cardnr, buf, 1024);
	if (ret<=0) {
		fprintf(stdout,"cannot get stackinfo err: %d\n", ret);
		return(3);
	}
	stinf = (stack_info_t *)&frm->data.p;
	if (VerifyOn>1)
		mISDNprint_stack_info(stdout, stinf);
	di->d_stid = stinf->id;
	for (i=0;i<2;i++) {
		if (stinf->childcnt>i)
			di->b_stid[i] = stinf->child[i];
		else
			di->b_stid[i] = 0;
	}

	di->layer1 = mISDN_get_layerid(di->device, di->d_stid, 1);
	if (di->layer1<0) {
		fprintf(stdout,"cannot get layer1\n");
		return(4);
	}
	if (VerifyOn>1)
		fprintf(stdout,"layer1 id %08x\n", di->layer1);

	di->layer2 = mISDN_get_layerid(di->device, di->d_stid, 2);
	if (di->layer2<0) {
		fprintf(stdout,"cannot get layer2\n");
		return(5);
	}
	if (VerifyOn>1)
		fprintf(stdout,"layer2 id %08x\n", di->layer2);

	di->layer3 = mISDN_get_layerid(di->device, di->d_stid, 3);
	if (di->layer3<0) {
		fprintf(stdout,"cannot get layer3\n");
		di->layer3 = 0;
	}
	if (VerifyOn>1)
		fprintf(stdout,"layer3 id %08x\n", di->layer3);


	ret = add_dlayer3(di, ISDN_PID_L3_DSS1NET);
	if (ret)
		return(ret);
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_TARGET | FLG_MSG_DOWN,
		DL_ESTABLISH | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"dl_etablish write ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"dl_etablish read ret=%d\n", ret);
	if (ret>0) {
		if (frm->prim != (DL_ESTABLISH | CONFIRM)) {
			fprintf(stdout,"DL_ESTABLISH | REQUEST return prim:%x\n",
				frm->prim);
//			return(6);
		}
	} else {
		fprintf(stdout,"DL_ESTABLISH | REQUEST return(%d)\n", ret);
//		return(7);
	}
	ret = mISDN_get_status_info(di->device, di->layer1, buf, 1024);
	if (ret > mISDN_HEADER_LEN) {
		si = (status_info_t *)&frm->data.p;
		mISDNprint_status(stdout, si);
	} else
		fprintf(stdout,"mISDN_get_status_info ret(%d)\n", ret);
	ret = mISDN_get_status_info(di->device, di->layer2, buf, 1024);
	if (ret > mISDN_HEADER_LEN) {
		si = (status_info_t *)&frm->data.p;
		mISDNprint_status(stdout, si);
	} else
		fprintf(stdout,"mISDN_get_status_info ret(%d)\n", ret);
	sleep(1);
	return(0);
}

int main(argc,argv)
int argc;
char *argv[];

{
	char FileName[200],FileNameOut[200];
	int aidx=1,para=1, idx;
	char sw;
	devinfo_t mISDN;
	int err;

	fprintf(stderr,"Test HFCNet 1.0\n");
	strcpy(FileName, "test_file");
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 1;
	mISDN.func = 0;
	strcpy(mISDN.display, "Test Display");
	strcpy(mISDN.msn, "789");
	mISDN.phonenr[0] = 0;
	if (argc<1) {
		fprintf(stderr,"Error: Not enough arguments please check\n");
		usage(argv[0]);
		exit(1);
	} else {
		do {
			if (argv[aidx] && argv[aidx][0]=='-') {
				sw=argv[aidx][1];
				switch (sw) {
					 case 'v':
					 case 'V':
						VerifyOn=1;
						if (argv[aidx][2]) {
							VerifyOn=atol(&argv[aidx][2]);
						}
						break;
					case 'c':
						if (argv[aidx][2]) {
							mISDN.cardnr=atol(&argv[aidx][2]);
						}
						break;
					case 'F':
						if (argv[aidx][2]) {
							mISDN.func=atol(&argv[aidx][2]);
						}
						break;
					case 'd':
					        if (!argv[aidx][2]) {
					        	idx = 0;
							aidx++;
						} else {
							idx=2;
						}
						if (aidx<=argc) {
							strcpy(mISDN.display, &argv[aidx][idx]);
						} else {
							fprintf(stderr," Switch %c without value\n",sw);
							exit(1);
						}
						break;
					case 'm':
					        if (!argv[aidx][2]) {
					        	idx = 0;
							aidx++;
						} else {
							idx=2;
						}
						if (aidx<=argc) {
							strcpy(mISDN.msn, &argv[aidx][idx]);
						} else {
							fprintf(stderr," Switch %c without value\n",sw);
							exit(1);
						}
						break;
					case 'n':
					        if (!argv[aidx][2]) {
					        	idx = 0;
							aidx++;
						} else {
							idx=2;
						}
						if (aidx<=argc) {
							strcpy(mISDN.phonenr, &argv[aidx][idx]);
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
				if (para==1) {
					if (argc > 1)
						strcpy(FileName,argv[aidx]);
					para++;
				} else {
					fprintf(stderr,"Undefined argument %s\n",argv[aidx]);
					usage(argv[0]);
					exit(1);
				}
			}
                                   aidx++;
		} while (aidx<argc);
	}
	if (0>(mISDN.device = mISDN_open())) {
		printf("TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return(1);
	}
	sprintf(FileNameOut,"%s.out",FileName);
	sprintf(FileName,"%s.in",FileName);
	printf("TestmISDN open in %s\n",FileName);
	if (0>(mISDN.save = open(FileName, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU))) {
		printf("TestmISDN cannot open %s due to %s\n",FileName,
			strerror(errno));
		close(mISDN.device);
		return(1);
	}
	printf("TestmISDN open out %s\n",FileNameOut);
	if (0>(mISDN.play = open(FileNameOut, O_RDONLY))) {
		printf("TestmISDN cannot open %s due to %s\n",FileNameOut,
			strerror(errno));
		mISDN.play = -1;
	} else 
		mISDN.fplay = fdopen(mISDN.play, "r");
	printf("TestmISDN files open\n");
	if (VerifyOn>8)
		fprintf(stdout,"fileno %d/%d/%d\n",mISDN.save, mISDN.play,
			mISDN.device);
	err = do_setup(&mISDN);
	if (err)
		fprintf(stdout,"do_setup error %d\n", err);
	else
		do_connection(&mISDN);	
	close(mISDN.save);
	if (mISDN.play>=0)
		close(mISDN.play);
	err=mISDN_close(mISDN.device);
	if (err)
		fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
			strerror(err));
	
	return(0);
}
