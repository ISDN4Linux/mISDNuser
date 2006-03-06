#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "mISDNlib.h"
#include "l3dss1.h"

void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options] [filename]\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"       filename   filename.in  incoming data\n");
	fprintf(stderr,"                  filename.out outgoing data\n");
	fprintf(stderr,"                  data is alaw for voice\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n"); 
	fprintf(stderr,"  -F<n>           use function n (default 0)\n"); 
	fprintf(stderr,"                    0 send and recive voice\n"); 
	fprintf(stderr,"                    1 send touchtones\n"); 
	fprintf(stderr,"                    2 recive touchtones\n"); 
	fprintf(stderr,"                    3 send and recive hdlc data\n"); 
	fprintf(stderr,"                    4 send and recive X75 data\n"); 
	fprintf(stderr,"                    5 send and recive voice early B connect\n");
	fprintf(stderr,"  -n <phone nr>   Phonenumber to dial\n");
	fprintf(stderr,"  -vn             Printing debug info level n\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int	device;
	int	cardnr;
	int	func;
	char	phonenr[32];
	u_int	d_stid;
	u_int	layer1;
	u_int	layer2;
	u_int	layer3;
	u_int	layer4;
	u_int	l3_id;
	u_int	idcnt;
	u_int	entity;
	u_int	b_stid[2];
	u_int	b_adress[2];
	u_int	b_l2[2];
	int	used_bchannel;
	int	save;
	int	play;
	FILE	*fplay;
	int	flag;
	int	val;
	int	si;
	u_int	bl1_prot;
	u_int	bl2_prot;
	u_int	bl3_prot;
} devinfo_t;

#define FLG_SEND_TONE		0x0001
#define FLG_SEND_DATA		0x0002
#define FLG_BCHANNEL_SETUP	0x0010
#define FLG_BCHANNEL_DOACTIVE	0x0020
#define FLG_BCHANNEL_ACTIVE	0x0040
#define FLG_BCHANNEL_ACTDELAYED	0x0080
#define FLG_CALL_ORGINATE	0x0100
#define FLG_BCHANNEL_EARLY	0x0200


#define MAX_REC_BUF		4000
#define MAX_DATA_BUF		1024

static int VerifyOn=0;
static char tt_char[] = "0123456789ABCD*#";

#define PLAY_SIZE 64

/* this should be moved into library context */
#ifndef MOVED_TO_LIB
static signed char _mISDN_l3_ie2pos[128] = {
			-1,-1,-1,-1, 0,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,
			 2,-1,-1,-1, 3,-1,-1,-1, 4,-1,-1,-1, 5,-1, 6,-1,
			 7,-1,-1,-1,-1,-1,-1, 8, 9,10,-1,-1,11,-1,-1,-1,
			-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			13,-1,14,15,16,17,18,19,-1,-1,-1,-1,20,21,-1,-1,
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,22,23,-1,-1,
			24,25,-1,-1,26,-1,-1,-1,27,28,-1,-1,29,30,31,-1
};
			
static unsigned char _mISDN_l3_pos2ie[32] = {
			0x04, 0x08, 0x10, 0x14, 0x18, 0x1c, 0x1e, 0x20,
			0x27, 0x28, 0x29, 0x2c, 0x34, 0x40, 0x42, 0x43,
			0x44, 0x45, 0x46, 0x47, 0x4c, 0x4d, 0x6c, 0x6d,
			0x70, 0x71, 0x74, 0x78, 0x79, 0x7c, 0x7d, 0x7e
};

signed int
mISDN_l3_ie2pos(u_char c)
{
	if (c>0x7f)
		return(-1);
	return(_mISDN_l3_ie2pos[c]);
}

unsigned char
mISDN_l3_pos2ie(int pos)
{
	return(_mISDN_l3_pos2ie[pos]);
}

void
mISDN_initQ931_info(Q931_info_t *qi) {
	memset(qi, 0, sizeof(Q931_info_t));
};

int mISDN_get_free_ext_ie(Q931_info_t *qi)
{
	int	i;

	for (i = 0; i < 8; i++) {
		if (qi->ext[i].ie.off == 0)
			return(i);
	}
	return (-1);
}

int
mISDN_AddvarIE(Q931_info_t *qi, u_char *p, u_char *ie)
{
	u_char		*ps;
	ie_info_t	*ies;
	int		l;

	ies = &qi->bearer_capability;
	ps = (u_char *) qi;
	ps += L3_EXTRA_SIZE;
	if (*ie & 0x80) { /* one octett IE */
		if (*ie == IE_MORE_DATA)
			ies = &qi->more_data;
		else if (*ie == IE_COMPLETE)
			ies = &qi->sending_complete;
		else if ((*ie & 0xf0) == IE_CONGESTION)
			ies = &qi->congestion_level;
		else {
			return(-1);
		}
		l = 1;
	} else {
		if (_mISDN_l3_ie2pos[*ie]<0) {
			return(-2);
		}
		ies += _mISDN_l3_ie2pos[*ie];
		if (ies->off) {
			while (ies->repeated)
				ies = &qi->ext[ies->ridx].ie;;
			l = mISDN_get_free_ext_ie(qi);
			if (l < 0) { // overflow
				return(-3);
			}
			ies->ridx = l;
			ies->repeated = 1;
			ies = &qi->ext[l].ie;
			ies->cs_flg = 0;
			qi->ext[l].v.codeset = 0;
			qi->ext[l].v.val = *ie;
		}
		l = ie[1] + 2;
	}
	ies->off = (u16)(p - ps);
	memcpy(p, ie, l);
	return(l);
}

int
mISDN_AddIE(Q931_info_t *qi, u_char *p, u_char ie, u_char *iep)
{
	u_char		*ps;
	ie_info_t	*ies;
	int		l;

	if (ie & 0x80) { /* one octett IE */
		if (ie == IE_MORE_DATA)
			ies = &qi->more_data;
		else if (ie == IE_COMPLETE)
			ies = &qi->sending_complete;
		else if ((ie & 0xf0) == IE_CONGESTION)
			ies = &qi->congestion_level;
		else {
			return(-1);
		}
		l = 0;
	} else {
		if (!iep || !iep[0])
			return(-3);
		ies = &qi->bearer_capability;
		if (_mISDN_l3_ie2pos[ie]<0) {
			return(-2);
		}
		ies += _mISDN_l3_ie2pos[ie];
		if (ies->off) {
			while (ies->repeated)
				ies = &qi->ext[ies->ridx].ie;;
			l = mISDN_get_free_ext_ie(qi);
			if (l < 0) { // overflow
				return(-3);
			}
			ies->ridx = l;
			ies->repeated = 1;
			ies = &qi->ext[l].ie;
			ies->cs_flg = 0;
			qi->ext[l].v.codeset = 0;
			qi->ext[l].v.val = ie;
		}
		l = iep[0] + 1;
	}
	ps = (u_char *) qi;
	ps += L3_EXTRA_SIZE;
	ies->off = (u16)(p - ps);
	*p++ = ie;
	if (l)
		memcpy(p, iep, l);
	return(l+1);
}
#endif /* MOVED_TO_LIB */

int play_msg(devinfo_t *di) {
	unsigned char buf[PLAY_SIZE+mISDN_HEADER_LEN];
	iframe_t *frm = (iframe_t *)buf;
	int len, ret;
	
	if (di->play<0)
		return(0);
	len = read(di->play, buf + mISDN_HEADER_LEN, PLAY_SIZE);
	if (len<0) {
		printf("play_msg err %d: \"%s\"\n", errno, strerror(errno));
		close(di->play);
		di->play = -1;
	}
	
	frm->addr = di->b_adress[di->used_bchannel] | FLG_MSG_DOWN;
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
	char buf[MAX_DATA_BUF+mISDN_HEADER_LEN];
	iframe_t *frm = (iframe_t *)buf;
	char *data;
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
	
	frm->addr = di->b_adress[di->used_bchannel] | FLG_MSG_DOWN;
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
	if (ret) {
		fprintf(stdout, "new_layer ret(%d)\n", ret);
		return(0);
	}
	if (li.id) {
		di->b_adress[di->used_bchannel] = li.id;
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
		ret = mISDN_set_stack(di->device, di->b_stid[di->used_bchannel], &pid);
		if (ret) {
			fprintf(stdout, "set_stack ret(%d)\n", ret);
			return(0);
		}
		/* Wait until the stack is really available */
		ret = mISDN_get_setstack_ind(di->device, di->b_adress[di->used_bchannel]);
		if (ret) {
			fprintf(stdout, "get_setstack_ind ret (%d)\n", ret);
			return(0);
		}
		/* get the registered id of the b3 layer */
		ret = mISDN_get_layerid(di->device, di->b_stid[di->used_bchannel], 3);
		if (ret <= 0) {
			fprintf(stdout, "get_layerid b3 ret(%d)\n", ret);
			return(0);
		}
		di->b_adress[di->used_bchannel] = ret;
		/* get the registered id of the b2  layer */
		ret = mISDN_get_layerid(di->device, di->b_stid[di->used_bchannel], 2);
		if (ret > 0)
			di->b_l2[di->used_bchannel] = ret;
		fprintf(stdout, "b_l2 id %08x\n", ret);
	} else
		ret = 0;
	return(ret);
}

int send_SETUP(devinfo_t *di, int SI, char *PNr) {
	unsigned char *np, *p, *msg, buf[1024],ie[64];
	Q931_info_t *qi;
	int len, ret;

	di->idcnt++;
	if (di->idcnt >0x7fff)
		di->idcnt = 1;
	di->l3_id = (di->entity << 16) | di->idcnt;
	ret = mISDN_write_frame(di->device, buf, di->layer4 | FLG_MSG_DOWN,
		CC_NEW_CR | REQUEST, di->l3_id, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"CC_NEW_CR | REQUEST l3_id %x ret=%d\n", di->l3_id, ret);
	p = msg = buf + mISDN_HEADER_LEN;
	qi = (Q931_info_t *)p;
	mISDN_initQ931_info(qi);
	qi->type = MT_SETUP;
	p += L3_EXTRA_SIZE;
	p++; /* needed to avoid offset 0 in IE array */
	len = mISDN_AddIE(qi, p, IE_COMPLETE, NULL);
	if (len<0) {
		fprintf(stdout,"Add IE_COMPLETE error %d\n", len);
		return(len);
	}
	p += len;
	if (SI == 1) { /* Audio */
		len = mISDN_AddIE(qi, p, IE_BEARER, (unsigned char*)"\x3\x90\x90\xa3");
	} else { /* default Datatransmission 64k */
		len = mISDN_AddIE(qi, p, IE_BEARER, (unsigned char*)"\x2\x88\x90");
	}
	if (len<0) {
		fprintf(stdout,"Add IE_BEARER error %d\n", len);
		return(len);
	}
	p += len;
	np = ie;
	*np++ = strlen(PNr) + 1;
	/* Classify as AnyPref. */
	*np++ = 0x81;            /* Ext = '1'B, Type = '000'B, Plan = '0001'B. */
	while (*PNr)
		*np++ = *PNr++ & 0x7f;
	len = mISDN_AddIE(qi, p, IE_CALLED_PN, ie);
	if (len<0) {
		fprintf(stdout,"Add IE_CALLED_PNerror %d\n", len);
		return(len);
	}
	p += len;
	len = p - msg;
	ret = mISDN_write_frame(di->device, buf, di->layer4 | FLG_MSG_DOWN,
		CC_SETUP | REQUEST, di->l3_id, len, msg, TIMEOUT_1SEC);
	return(ret);
}

int activate_bchan(devinfo_t *di) {
	unsigned char buf[128];
	iframe_t *rfrm;
	int ret;

	ret = mISDN_write_frame(di->device, buf,
		di->b_adress[di->used_bchannel] | FLG_MSG_DOWN,
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
		di->b_adress[di->used_bchannel] | FLG_MSG_DOWN,
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
	ret = mISDN_write_frame(di->device, buf, di->b_adress[di->used_bchannel] | FLG_MSG_DOWN,
		MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	ret = mISDN_write_frame(di->device, buf, di->b_l2[di->used_bchannel] | FLG_MSG_DOWN,
		MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	return(ret);
}

int send_touchtone(devinfo_t *di, int tone) {
	iframe_t frm;
	int tval, ret;

	if (VerifyOn>1)
		fprintf(stdout,"send_touchtone %c\n", DTMF_TONE_MASK & tone);
	tval = DTMF_TONE_VAL | tone;
	ret = mISDN_write_frame(di->device, &frm,
		di->b_adress[di->used_bchannel] | FLG_MSG_DOWN,
		PH_CONTROL | REQUEST, 0, 4, &tval, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"tt send ret=%d\n", ret);
	return(ret);
}

int read_mutiplexer(devinfo_t *di) {
	unsigned char	buf[MAX_REC_BUF];
	iframe_t	*rfrm;
	int		timeout = TIMEOUT_10SEC;
	int		ret = 0;

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
			if (rfrm->addr == (di->b_l2[di->used_bchannel] | FLG_MSG_UP)) {
				/* B-Channel related messages */
				if (rfrm->prim == (DL_DATA | INDICATION)) {
					/* received data, save it */
					write(di->save, &rfrm->data.i, rfrm->len);
				} else if (rfrm->prim == (DL_DATA | CONFIRM)) {
					/* get ACK of send data, so we can
					 * send more
					 */
					if (VerifyOn>5)
						fprintf(stdout,"DL_DATA_CNF\n");
					switch (di->func) {
						case 0:
						case 2:
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
			} else if ((rfrm->prim == (CC_CONNECT | INDICATION)) &&
				(di->flag & FLG_CALL_ORGINATE)) {
				/* We got connect, so bring B-channel up */
				if (!(di->flag & FLG_BCHANNEL_EARLY)) {
					if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
						activate_bchan(di);
					else
						di->flag |= FLG_BCHANNEL_DOACTIVE;
				}
				/* send a CONNECT_ACKNOWLEDGE */
				switch (di->func) {
					case 0:
					case 2:
					case 5:
						if (di->play > -1)
							play_msg(di);
						break;
					case 1:
						/* send next after 2 sec */
						timeout = 2*TIMEOUT_1SEC;
						di->flag |= FLG_SEND_TONE;
						break;
					case 3:
					case 4:
						/* setup B after 1 sec */
						timeout = 1*TIMEOUT_1SEC;
						break;
				}
			} else if ((rfrm->prim == (CC_CONNECT_ACKNOWLEDGE | INDICATION)) &&
				(!(di->flag & FLG_CALL_ORGINATE))) {
				/* We got connect ack, so bring B-channel up */
				if (!(di->flag & FLG_BCHANNEL_EARLY)) {
					if (!(di->flag & FLG_BCHANNEL_ACTDELAYED))
						activate_bchan(di);
					else
						di->flag |= FLG_BCHANNEL_DOACTIVE;
				}
				/* if here is outgoing data, send first part */
				switch (di->func) {
					case 0:
					case 2:
					case 5:
						if (di->play > -1)
							play_msg(di);
						break;
					case 1:
						/* send next after 2 sec */
						timeout = 2*TIMEOUT_1SEC;
						di->flag |= FLG_SEND_TONE;
						break;
					case 3:
					case 4:
						/* setup B after 1 sec */
						timeout = 1*TIMEOUT_1SEC;
						break;
				}
			} else if (rfrm->prim == (CC_DISCONNECT | INDICATION)) {
				/* send a RELEASE */
				ret = mISDN_write_frame(di->device, buf,
					di->layer4 | FLG_MSG_DOWN, CC_RELEASE | REQUEST,
					di->l3_id, 0, NULL, TIMEOUT_1SEC);
			} else if (rfrm->prim == (CC_RELEASE | INDICATION)) {
				/* on a disconnecting msg leave loop */
				return(2);
			} else if (rfrm->prim == (CC_RELEASE_COMPLETE | INDICATION)) {
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
			ret = mISDN_write_frame(di->device, buf,
				di->layer4 | FLG_MSG_DOWN, CC_DISCONNECT | REQUEST,
				di->l3_id, 0, NULL, TIMEOUT_1SEC);
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
	unsigned char *p, buf[1024];
	iframe_t *rfrm;
	Q931_info_t	*qi;
	int ret = 0;
	int bchannel;

	rfrm = (iframe_t *)buf;

	if (strlen(di->phonenr)) {
		di->flag |= FLG_CALL_ORGINATE;
//		di->cr = 0x81;
		ret = send_SETUP(di, di->si, di->phonenr);
		if (ret) {
			return(1);
		}
	}
	bchannel= -1;
	/* Wait for a SETUP message or a CALL_PROCEEDING */
	while ((ret = mISDN_read(di->device, buf, 1024, 3*TIMEOUT_10SEC))) {
		if (VerifyOn>3)
			fprintf(stdout,"readloop ret=%d addr(%08x) prim=%x dinfo=%x\n",
				ret, rfrm->addr, rfrm->prim, rfrm->dinfo);
		if (ret >= 16) {
			if ((!(di->flag & FLG_CALL_ORGINATE)) &&
				(rfrm->prim == (CC_NEW_CR | INDICATION))) {
				di->l3_id = rfrm->dinfo;
				if (VerifyOn>3)
					fprintf(stdout,"new layer3 process id %x\n", di->l3_id);
			} else if (((!(di->flag & FLG_CALL_ORGINATE)) && (rfrm->prim == (CC_SETUP | INDICATION))) ||
				((di->flag & FLG_CALL_ORGINATE) && (rfrm->prim == (CC_PROCEEDING | INDICATION)))) {
				p = buf + mISDN_HEADER_LEN;
				qi = (Q931_info_t *)p;
				p += L3_EXTRA_SIZE;
				if (qi->channel_id.off > 0) {
					bchannel = p[qi->channel_id.off + 2] & 0x3;
				} else {
					fprintf(stdout,"no bchannel IE found\n");
					return(2);
				}
				break;
			}
		}
	}
	fprintf(stdout,"bchannel %d\n", bchannel);
	if (bchannel > 0) {
		/* setup a B-channel stack */
		di->used_bchannel = bchannel -1;
		switch (di->func) {
			case 5:
				di->flag |= FLG_BCHANNEL_EARLY;
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
				if (di->flag & FLG_BCHANNEL_EARLY) {
					ret = activate_bchan(di);
					if (!ret) {
						fprintf(stdout,"error on activate_bchan\n");
						goto clean_up;
					}
				}
				break;
		}
		if (!(di->flag & FLG_CALL_ORGINATE)) {
			ret = mISDN_write_frame(di->device, buf,
				di->layer4 | FLG_MSG_DOWN, CC_CONNECT | REQUEST,
				di->l3_id, 0, NULL, TIMEOUT_1SEC);
		}
		if (!read_mutiplexer(di)) { /* timed out */
			/* send a RELEASE_COMPLETE */
			fprintf(stdout,"read_mutiplexer timed out sending RELEASE_COMPLETE\n");
			ret = mISDN_write_frame(di->device, buf,
				di->layer4 | FLG_MSG_DOWN, CC_RELEASE_COMPLETE | REQUEST,
				di->l3_id, 0, NULL, TIMEOUT_1SEC);
		}
		deactivate_bchan(di);
	} else {
		fprintf(stdout,"no channel or no connection\n");
	}
clean_up:
	sleep(1);
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_DOWN,
		DL_RELEASE | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	sleep(1);
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_DOWN,
		MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	ret = mISDN_write_frame(di->device, buf, di->layer3,
		MGR_DELENTITY | REQUEST, di->entity, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"MGR_DELENTITY | REQUEST ret=%d\n", ret);
	while((ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC))) {
		if (VerifyOn>3)
			fprintf(stdout,"MGR_DELENTITY | REQUEST read ret=%d\n", ret);
		if (rfrm->prim == (MGR_DELENTITY | CONFIRM)) {
			if (VerifyOn>4)
				fprintf(stdout,"entity(%x) removed\n", di->entity);
			break;
		} else {
			if (VerifyOn)
				fprintf(stdout,"read prim %x instead of MGR_DELENTITY | CONFIRM (%x)\n",
					rfrm->prim, (MGR_DELENTITY | CONFIRM));
		}
	}
	if (!ret) {
		fprintf(stdout,"MGR_DELENTITY | REQUEST read timed out\n");
		return(6);
	}
	return(0);
}



int
add_dlayer4(devinfo_t *di, int prot)
{
	layer_info_t li;
	stack_info_t si;
	int lid, stid, ret;

	if (di->layer4) {
		memset(&si, 0, sizeof(stack_info_t));
		si.extentions = EXT_STACK_CLONE;
		si.mgr = -1;
		si.id = di->d_stid;
		stid = mISDN_new_stack(di->device, &si);
		if (stid <= 0) {
			fprintf(stdout, "clone stack failed ret(%d)\n", stid);
			return(11);
		}
		memset(&li, 0, sizeof(layer_info_t));
		li.object_id = -1;
		li.extentions = EXT_INST_CLONE;
		li.parent = di->layer2;
		li.st = stid;
		ret = mISDN_new_layer(di->device, &li);
		if (ret) {
			fprintf(stdout, "clone layer failed ret(%d)\n", ret);
			return(11);
		}
		if (!li.clone) {
			fprintf(stdout, "no cloned id\n");
			return(11);
		}
		di->layer2 = li.clone;
		di->layer3 = 0;
		di->d_stid = stid;
	}
	if (!di->layer3) {
		/* search for DSS1 */
		fprintf(stdout, "currently L3 must already exist\n");
		return (12); /* currently not implemented */
	}
	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "user L4");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[4] = prot;
	li.pid.layermask = ISDN_LAYER(4);
	li.st = di->d_stid;
	ret = mISDN_new_layer(di->device, &li);
	if (ret)
		return(12);
	di->layer4 = li.id;
	if (VerifyOn>1)
		fprintf(stdout,"new layer3 id %08x\n", di->layer4);
	if (!di->layer4)
		return(13);
	
	ret = mISDN_register_layer(di->device, di->d_stid, di->layer4);
	if (ret) {
		fprintf(stdout, "register_layer ret(%d)\n", ret);
		return(14);
	}
	lid = mISDN_get_layerid(di->device, di->d_stid, 3);
	if (lid<0) {
		fprintf(stdout,"cannot get layer3 (%d)\n", lid);
		return(15);
	}
	di->layer3 = lid;
	lid = mISDN_get_layerid(di->device, di->d_stid, 4);
	if (lid<0) {
		fprintf(stdout,"cannot get layer4 (%d)\n", lid);
		return(16);
	}
	di->layer4 = lid;
	return(0);
}

int do_setup(devinfo_t *di) {
	unsigned char buf[1024];
	iframe_t *frm = (iframe_t *)buf;
	int i, ret = 0;
	stack_info_t *stinf;
//	status_info_t *si;

	di->bl2_prot = ISDN_PID_L2_B_TRANS;
	di->bl3_prot = ISDN_PID_L3_B_TRANS;
	switch (di->func) {
		case 0:
		case 5:
			di->bl1_prot = ISDN_PID_L1_B_64TRANS;
			di->si = 1;
			break;
		case 1:
			di->bl1_prot = ISDN_PID_L1_B_64TRANS;
			di->bl2_prot = ISDN_PID_L2_B_TRANSDTMF;
			di->si = 1;
			di->val= 8; /* send  8 touch tons (7 ... 0) */
			break;
		case 2:
			di->bl1_prot = ISDN_PID_L1_B_64TRANS;
			di->bl2_prot = ISDN_PID_L2_B_TRANSDTMF;
			di->si = 1;
			break;
		case 3:
			di->bl1_prot = ISDN_PID_L1_B_64HDLC;
			di->si = 7;
			break;
		case 4:
			di->bl1_prot = ISDN_PID_L1_B_64HDLC;
			di->bl2_prot = ISDN_PID_L2_B_X75SLP;
			di->si = 7;
			di->flag |= FLG_BCHANNEL_ACTDELAYED;
			break;
		default:
			fprintf(stdout,"unknown program function %d\n",
				di->func);
			return(1);
	}

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


	di->layer4 = mISDN_get_layerid(di->device, di->d_stid, 4);
	if (di->layer4<0) {
		fprintf(stdout,"cannot get layer4\n");
		di->layer4 = 0;
	}
	if (VerifyOn>1)
		fprintf(stdout,"layer4 id %08x\n", di->layer4);


	ret = add_dlayer4(di, ISDN_PID_L4_CAPI20);
	if (ret)
		return(ret);

	ret = mISDN_get_stack_info(di->device, di->d_stid, buf, 1024);
	if (ret<=0) {
		fprintf(stdout,"cannot get stackinfo err: %d\n", ret);
		return(3);
	}
	stinf = (stack_info_t *)&frm->data.p;
	if (VerifyOn>1)
		mISDNprint_stack_info(stdout, stinf);

	ret = mISDN_write_frame(di->device, buf, di->layer3,
		MGR_NEWENTITY | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"MGR_NEWENTITY | REQUEST ret=%d\n", ret);
	while((ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC))) {
		if (VerifyOn>3)
			fprintf(stdout,"MGR_NEWENTITY | REQUEST read ret=%d\n", ret);
		if (frm->prim == (MGR_NEWENTITY | CONFIRM)) {
			di->entity = frm->dinfo;
			if (VerifyOn>4)
				fprintf(stdout,"entity = %x\n", di->entity);
			break;
		} else {
			if (VerifyOn)
				fprintf(stdout,"read prim %x instead of MGR_NEWENTITY | CONFIRM (%x)\n",
					frm->prim, (MGR_NEWENTITY | CONFIRM));
		}
	}
	if (!ret) {
		fprintf(stdout,"MGR_NEWENTITY | REQUEST read timed out\n");
		return(6);
	}
#ifdef OBSOLATE
	ret = mISDN_write_frame(di->device, buf, di->layer3 | FLG_MSG_DOWN,
		DL_ESTABLISH | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"dl_etablish write ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"dl_etablish read ret=%d\n", ret);
	if (ret>0) {
		if (frm->prim != (DL_ESTABLISH | CONFIRM))
			return(6);
	} else {
		fprintf(stdout,"DL_ESTABLISH | REQUEST return(%d)\n", ret);
		return(7);
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
#endif
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

	fprintf(stderr,"TestmISDN 1.0\n");
	strcpy(FileName, "test_file");
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 1;
	mISDN.func = 0;
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
	if (0>(mISDN.save = open(FileName, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU))) {
		printf("TestmISDN cannot open %s due to %s\n",FileName,
			strerror(errno));
		close(mISDN.device);
		return(1);
	}
	if (0>(mISDN.play = open(FileNameOut, O_RDONLY))) {
		printf("TestmISDN cannot open %s due to %s\n",FileNameOut,
			strerror(errno));
		mISDN.play = -1;
	} else 
		mISDN.fplay = fdopen(mISDN.play, "r");
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
