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
	fprintf(stderr,"Call with %s [options] <prim> <dinfo>\n",pname);
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?               Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>            use card number n (default 1)\n"); 
	fprintf(stderr,"  -v<n>            Printing debug info level n\n");
	fprintf(stderr,"        0          only received messages are printed\n");
	fprintf(stderr,"        1          send count\n");
	fprintf(stderr,"        2          send status\n");
	fprintf(stderr,"        3          send contens\n");
	fprintf(stderr,"        4          device read count\n");
	fprintf(stderr,"        5          stdin line parsing\n");
	fprintf(stderr,"        6          stdin line raw contens\n");
	fprintf(stderr,"        7          filenumber/select status\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"<prim>             primitiv code e.g. 0x000280 PH_CONTROL REQUEST\n");
	fprintf(stderr,"<dinfo>            dinfo code e.g. 0xFF01 to loop B1\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int 		device;
	unsigned int  d_stid;
	unsigned int  d_adress;
} devinfo_t;


static int VerifyOn=0;

int send_primitiv(devinfo_t *di, unsigned int prim, unsigned int dinfo) {
	unsigned char *p, buf[2048], rbuf[128];
	iframe_t *frm = (iframe_t *)buf;
	int ret = 0;

	frm->prim = prim;
	frm->dinfo = dinfo;
	frm->addr = di->d_adress | FLG_MSG_TARGET | FLG_MSG_DOWN;
	frm->len = 0;

	ret = mISDN_write(di->device, buf, frm->len+mISDN_HEADER_LEN, 100000);
	if (ret < 0)
		fprintf(stdout,"send_data write error %d %s\n", errno,
			strerror(errno));
	else if (VerifyOn>3)
		fprintf(stdout,"send_data write ret=%d\n", ret);

	ret = mISDN_read(di->device, rbuf, 128, TIMEOUT_1SEC); 	
	if (VerifyOn>3)
		fprintf(stdout,"read ret=%d\n", ret);
	return(0);
}

int do_setup(devinfo_t *di, int cardnr) {
	unsigned char buf[2048];
	iframe_t *frm = (iframe_t *)buf;
	int ret = 0;
	stack_info_t *stinf;

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
	if (!stinf->inst[0]) {
		fprintf(stdout,"cannot get hw d-channel address\n");
		return(4);
	}
	di->d_adress = stinf->inst[0];
	return(0);
}

unsigned int
get_value(char *s)
{
	unsigned int v;
	
	if (strlen(s) >2) {
		if (s[0] == '0') {
			if ((s[1] == 'x') || (s[1] == 'X')) {
				sscanf(s, "%x", &v);
				return(v);
			}
		}
	}
	v = atol(s);
	return(v);
}

int main(argc,argv)
int argc;
char *argv[];

{
	int aidx=1,para=1;
	char sw;
	int err;
	devinfo_t mISDN;
	int cardnr =1;
	unsigned int prim = 0;
	unsigned int dinfo = 0;

	fprintf(stderr,"sendhwctrl 1.0\n");
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
						prim = get_value(argv[aidx]);
					para++;
				} else if (para==2) {
					if (argc > 1)
						dinfo = get_value(argv[aidx]);
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
	if (para < 3) {
		fprintf(stderr,"Error: Not enough arguments please check\n");
		usage(argv[0]);
		exit(1);
	}
	memset(&mISDN, 0, sizeof(mISDN));
	if (0>(mISDN.device = mISDN_open())) {
		printf("TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return(1);
	}

	err = do_setup(&mISDN, cardnr);
	if (err)
		fprintf(stdout,"do_setup error %d\n", err);
	else
		send_primitiv(&mISDN, prim, dinfo);
	if (err)
		fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
			strerror(err));
	
	return(0);
}
