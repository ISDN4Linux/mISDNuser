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

void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options] [firmware filename]\n",pname);
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?               Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>            use card number n (default 1)\n"); 
	fprintf(stderr,"  -v<n>            Printing debug info level n\n");
	fprintf(stderr,"        0          only recived messages are printed\n");
	fprintf(stderr,"        1          send count\n");
	fprintf(stderr,"        2          send status\n");
	fprintf(stderr,"        3          send contens\n");
	fprintf(stderr,"        4          device read count\n");
	fprintf(stderr,"        5          stdin line parsing\n");
	fprintf(stderr,"        6          stdin line raw contens\n");
	fprintf(stderr,"        7          filenumber/select status\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int  device;
	int  mode;
	int  d_stid;
	int  d_adress;
	int  b_stid[2];
	int  b_adress[2];
	int  used_bchannel;
} devinfo_t;

#define MAX_SIZE 0x10000

static int VerifyOn=0;
unsigned char *firmware;

int download_firmware(devinfo_t *di, int size) {
	unsigned char *p, buf[2048], rbuf[128];
	iframe_t *frm = (iframe_t *)buf;
	//iframe_t *rfrm = (iframe_t *)rbuf;
	int cnt, ret = 0;
	int *adr, l;

	frm->prim = PH_CONTROL | REQUEST;
	frm->dinfo = 0;
	frm->addr = di->b_adress[di->used_bchannel] | IF_DOWN;
	frm->len = 8;
	adr = (int *)&frm->data.i;
	*adr++ = HW_FIRM_START;
	*adr++ = size;

	ret = mISDN_write(di->device, buf, frm->len+mISDN_HEADER_LEN, 100000);
	if (ret < 0)
		fprintf(stdout,"send_data write error %d %s\n", errno,
			strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"send_data write ret=%d\n", ret);

	ret = mISDN_read(di->device, rbuf, 128, TIMEOUT_10SEC); 	
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	cnt = 0;
	p = firmware;
	while (cnt<size) {
		l = 1024;
		if (l+cnt >=size)
			l = size - cnt;
		frm->prim = PH_CONTROL | REQUEST;
		frm->dinfo = 0;
		frm->addr = di->b_adress[di->used_bchannel] | IF_DOWN;
		frm->len = l + 8;
		adr = (int *)&frm->data.i;
		*adr++ = HW_FIRM_DATA;
		*adr++ = l;
		memcpy(adr, firmware + cnt, l);
		ret = mISDN_write(di->device, buf, frm->len+mISDN_HEADER_LEN, 100000);
		if (ret < 0)
			fprintf(stdout,"send_data write error %d %s\n", errno,
				strerror(errno));
		else if (VerifyOn>3)
			fprintf(stdout,"send_data write ret=%d\n", ret);
		ret = mISDN_read(di->device, rbuf, 128, TIMEOUT_10SEC);
		if (VerifyOn>3)
			fprintf(stdout,"read ret=%d\n", ret);
		cnt += l;
	}
	frm->prim = PH_CONTROL | REQUEST;
	frm->dinfo = 0;
	frm->addr = di->b_adress[di->used_bchannel] | IF_DOWN;
	frm->len = 4;
	adr = (int *)&frm->data.i;
	*adr++ = HW_FIRM_END;
	ret = mISDN_write(di->device, buf, frm->len+mISDN_HEADER_LEN, 100000);
	if (ret < 0)
		fprintf(stdout,"send_data write error %d %s\n", errno,
			strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"send_data write ret=%d\n", ret);
	ret = mISDN_read(di->device, rbuf, 128, TIMEOUT_10SEC);
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	
	ret = mISDN_clear_stack(di->device, di->b_stid[di->used_bchannel]);
	if (VerifyOn>3)
		fprintf(stdout,"clear_stack ret=%d\n", ret);
	return(0);
}

int do_setup(devinfo_t *di, int cardnr) {
	unsigned char buf[2048];
	iframe_t *frm = (iframe_t *)buf;
	int ret = 0;
	int i;
	stack_info_t *stinf;
	mISDN_pid_t pid;
	layer_info_t li;

	ret = mISDN_get_stack_count(di->device);
	if (VerifyOn>1)
		fprintf(stdout,"%d stacks found(%d)\n", ret, cardnr);
	if (ret < cardnr) {
		fprintf(stdout,"cannot config card nr %d only %d cards\n",
			cardnr, ret);
		return(2);
	}
	ret = mISDN_get_stack_info(di->device, cardnr, buf, 1024);
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

	di->used_bchannel = 0;

	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "B L3");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[3] = ISDN_PID_L3_B_TRANS;
	li.pid.layermask = ISDN_LAYER(3);
	li.st = di->b_stid[di->used_bchannel];
	ret = mISDN_new_layer(di->device, &li);
	if (ret<=0) {
		fprintf(stdout, "new_layer ret(%d)\n", ret);
		return(4);
	}
	di->b_adress[di->used_bchannel] = ret;
	if (VerifyOn>2)
		fprintf(stdout,"b_adress%d %08x\n",
			di->used_bchannel+1, ret);
	memset(&pid, 0, sizeof(mISDN_pid_t));
	pid.protocol[1] = ISDN_PID_L1_B_64TRANS;
	pid.protocol[2] = ISDN_PID_L2_B_TRANS;
	pid.protocol[3] = ISDN_PID_L3_B_TRANS;
	pid.layermask = ISDN_LAYER(1) | ISDN_LAYER(2)| ISDN_LAYER(3);
	ret = mISDN_set_stack(di->device,
		di->b_stid[di->used_bchannel], &pid);
	if (ret) {
		fprintf(stdout, "set_stack ret(%d)\n", ret);
		return(5);
	}
	return(0);
}

int
read_firmware(unsigned char *fname)
{
	FILE *infile;
	int  cnt;
	
	if (!(infile = fopen(fname, "rb"))) {
		fprintf(stderr, "cannot open file %s\n", fname);
		exit(-1);
	}
	firmware = (unsigned char *) malloc(MAX_SIZE);
	if (!firmware) {
		fprintf(stderr, "cannot get %d byte memory\n", MAX_SIZE+4);
		exit(-1);
	}
	cnt = fread(firmware, 1, MAX_SIZE, infile);
	fclose(infile);
	if (cnt==MAX_SIZE) {
		fprintf(stderr, "wrong filesize\n");
		exit(-1);
	}
	return(cnt);
}

int main(argc,argv)
int argc;
char *argv[];

{
	char FileName[200];
	int aidx=1,para=1;
	char sw;
	int len,err;
	devinfo_t mISDN;
	int cardnr =1;

	fprintf(stderr,"loadfirm 1.0\n");
	strcpy(FileName,"ISAR.BIN");
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
	memset(&mISDN, 0, sizeof(mISDN));
	if (0>(mISDN.device = mISDN_open())) {
		printf("TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return(1);
	}

	len = read_firmware(FileName);
	if (VerifyOn)
		fprintf(stdout,"read firmware from %s %d bytes\n", FileName, len);
	err = do_setup(&mISDN, cardnr);
	if (err)
		fprintf(stdout,"do_setup error %d\n", err);
	else
		download_firmware(&mISDN, len);
	free(firmware);
	err=mISDN_close(mISDN.device);
	if (err)
		fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
			strerror(err));
	
	return(0);
}
