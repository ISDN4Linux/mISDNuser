#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n"); 
	fprintf(stderr,"  -F<n>           use function n (default 0)\n"); 
	fprintf(stderr,"                    0 normal logging\n"); 
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int	device;
	int	cardnr;
	int	func;
	char	phonenr[32];
	int	d_stid;
	int	layer1;
	int	layer2;
	int	layer3;
	int	b_stid[2];
	int	b_adress[2];
	int	used_bchannel;
	int	save;
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
#define FLG_BCHANNEL_EARLY	0x0200


#define MAX_REC_BUF		4000
#define MAX_DATA_BUF		1024

static int VerifyOn=0;

static devinfo_t *init_di = NULL;

#define	MsgHead(ptr, cref, mty) \
	*ptr++ = 0x8; \
	if (cref == -1) { \
		*ptr++ = 0x0; \
	} else { \
		*ptr++ = 0x1; \
		*ptr++ = cref^0x80; \
	} \
	*ptr++ = mty
	
int printhexdata(FILE *f, int len, u_char *p)
{
	while(len--) {
		fprintf(f, "%02x", *p++);
		if (len)
			fprintf(f, " ");
	}
	fprintf(f, "\n");
	return(0);
}

int process_dchannel(devinfo_t *di, int len, iframe_t *frm)
{
	write(di->save, frm, len);
	if (frm->prim == (PH_DATA | INDICATION) && (frm->len >0)) {
		if (VerifyOn>5)
			printhexdata(stdout, frm->len, (void *)&frm->data.i);
	} 
	return(0);
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

int activate_bchan(devinfo_t *di) {
	unsigned char buf[128];
	iframe_t *rfrm;
	int ret;

	ret = mISDN_write_frame(di->device, buf,
		di->b_adress[di->used_bchannel] | IF_DOWN,
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
		di->b_adress[di->used_bchannel] | IF_DOWN,
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

int read_mutiplexer(devinfo_t *di) {
	unsigned char	buf[MAX_REC_BUF];
	iframe_t	*rfrm;
	int		timeout = TIMEOUT_10SEC;
	int		ret = 0;

	rfrm = (iframe_t *)buf;
	/* Main loop */
	while (1){
		ret = mISDN_read(di->device, buf, MAX_REC_BUF, timeout);
		if (VerifyOn>3)
			fprintf(stdout,"readloop ret=%d\n", ret);
		if (ret == -1) {
			fprintf(stdout,"readloop read error\n");
			break;
		}
		if (ret >= 16) {
			if (VerifyOn>4)
				fprintf(stdout,"readloop addr(%x) prim(%x) len(%d)\n",
					rfrm->addr, rfrm->prim, rfrm->len);
			if (rfrm->addr == (di->b_adress[di->used_bchannel] | IF_DOWN)) {
				/* B-Channel related messages */
				if (rfrm->prim == (DL_DATA | INDICATION)) {
					/* received data, save it */
					write(di->save, &rfrm->data.i, rfrm->len);
				} 
			/* D-Channel related messages */  
			} else if (rfrm->addr == (di->layer2 | IF_DOWN)) {
				if (VerifyOn>4)
					fprintf(stdout,"readloop addr(%x) prim(%x)len(%d)\n",
						rfrm->addr, rfrm->prim, rfrm->len);
				process_dchannel(di, ret, rfrm);
			} else {
				if (VerifyOn)
					fprintf(stdout,"readloop unknown addr(%x) prim((%x)len(%d)\n",
						rfrm->addr, rfrm->prim, rfrm->len);
			}
		}
	}
	return(0);
}


int
add_dlayer2(devinfo_t *di, int prot)
{
	layer_info_t li;
	interface_info_t ii;
	int lid, ret;

	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "user L2");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[2] = prot;
	li.pid.layermask = ISDN_LAYER(2);
	li.st = di->d_stid;
	lid = mISDN_new_layer(di->device, &li);
	if (lid<0)
		return(12);
	di->layer2 = lid;
	if (!di->layer2)
		return(13);
	
	/* 
	 * EXT_IF_CREATE | EXT_IF_EXCLUSIV sorgen dafuer, das wenn die L3
	 * Schnittstelle schon benutzt ist, eine neue L2 Instanz erzeugt
	 * wird
	 */
   	
	ii.extentions = EXT_IF_CREATE | EXT_IF_EXCLUSIV;
	ii.owner = di->layer2;
	ii.peer = di->layer1;
	ii.stat = IF_DOWN;
	ret = mISDN_connect(di->device, &ii);
	if (ret)
		return(13);
	ii.owner = di->layer2;
	ii.stat = IF_DOWN;
	ret = mISDN_get_interface_info(di->device, &ii);
	if (ret != 0)
		return(14);
	if (ii.peer == di->layer1)
		fprintf(stdout, "Layer 1 not cloned\n");
	else
		fprintf(stdout, "Layer 1 %08x cloned from %08x\n",
			ii.peer, di->layer1);
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
	switch (di->func) {
		case 0:
			di->bl1_prot = ISDN_PID_L1_B_64TRANS;
			di->si = 1;
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
	if (VerifyOn>1)
		fprintf(stdout,"layer2 id %08x\n", di->layer2);

	if (di->layer2) {
		fprintf(stdout,"layer 2 allready present\n");
		return(5);
	}

	ret = add_dlayer2(di, ISDN_PID_L2_LAPD);
	if (ret)
		return(ret);

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
	init_di = di;
	return(0);
}

void
close_di(devinfo_t *di) {
	unsigned char buf[1024];
	int ret = 0;

	init_di = NULL;
	ret = mISDN_write_frame(di->device, buf, di->layer3 | IF_DOWN,
		MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
	if (VerifyOn>3)
		fprintf(stdout,"ret=%d\n", ret);
	ret = mISDN_read(di->device, buf, 1024, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
}

static void
term_handler(int sig)
{
	if (init_di)
		close_di(init_di);
}

int main(argc,argv)
int argc;
char *argv[];

{
	char FileName[200],FileNameOut[200];
	int aidx=1,para=1;
	char sw;
	devinfo_t mISDN;
	int err;

	fprintf(stderr,"TestmISDN 1.0\n");
	strcpy(FileName, "test_file");
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 1;
	mISDN.func = 0;
	mISDN.phonenr[0] = 0;

	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	signal(SIGPIPE, term_handler);

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
	sprintf(FileNameOut,"%s",FileName);
	if (0>(mISDN.save = open(FileName, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU))) {
		printf("TestmISDN cannot open %s due to %s\n",FileName,
			strerror(errno));
		close(mISDN.device);
		return(1);
	}
	if (VerifyOn>8)
		fprintf(stdout,"fileno %d/%d\n",mISDN.save, mISDN.device);
	err = do_setup(&mISDN);
	if (err)
		fprintf(stdout,"do_setup error %d\n", err);
	else
		read_mutiplexer(&mISDN);
	close(mISDN.save);
	err=mISDN_close(mISDN.device);
	if (err)
		fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
			strerror(err));
	
	return(0);
}
