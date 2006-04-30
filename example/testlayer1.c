/*
	Sample code to act as a userland dummy layer2
	and communicate with Layer1
	
	(use layermask=3 to ensure your stack just 
	consists of Layer0+Layer1)
*/

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

void usage(pname) 
char *pname;
{
	fprintf(stderr,"Call with %s [options]\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"  -c<n>           use card number n (default 1)\n"); 
	fprintf(stderr,"  -vn             Printing debug info level n\n");
	fprintf(stderr,"\n");
}

typedef struct _devinfo {
	int	device;
	int	cardnr;
	int	func;
	int	d_stid;
	int	b_stid[2];
	int	layer1;
	int	layer2;
} devinfo_t;


static int VerifyOn=0;
static devinfo_t mISDN;


void sig_handler(int sig)
{
	int err;
	fprintf(stdout,"exiting...\n");
        err = mISDN_close(mISDN.device);
        fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
        	strerror(err));
	exit(0);
}
                        
                        
void set_signals()
{
        /* Set up the signal handler */
        signal(SIGHUP, sig_handler);
        signal(SIGINT, sig_handler);
        signal(SIGTERM, sig_handler);
}
                                                        

/* create userland layer2 */
int
add_dlayer2(devinfo_t *di)
{
	layer_info_t li;
	int ret, lid;

	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "user L2");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[2] = ISDN_PID_L2_LAPD;
	li.pid.layermask = ISDN_LAYER(2);
	li.st = di->d_stid;
	ret = mISDN_new_layer(di->device, &li);
	if (ret<0)
		return(12);
	
	di->layer2 = li.id;
	
	ret = mISDN_register_layer(di->device, di->d_stid, di->layer2);
        if (ret) {
                fprintf(stdout, "register_layer ret(%d)\n", ret);
                return(14);
        }	
        
        lid = mISDN_get_layerid(di->device, di->d_stid, 2);
	if (lid<0) {
		fprintf(stdout,"cannot get layer2 (%d)\n", lid);
		return(15);
	}
	di->layer2 = lid;
                                                     
	if (!di->layer2)
		return(13);
	
	return(0);
}

int do_setup(devinfo_t *di) {
	unsigned char buf[1024];
	iframe_t *frm = (iframe_t *)buf;
	int i, ret = 0;
	stack_info_t *stinf;

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

	/* make sure this stack just consists of layer0 + layer1 */
	di->layer2 = mISDN_get_layerid(di->device, di->d_stid, 2);
	if (di->layer2 > 0) {
		fprintf(stdout,"layer2 found (%i), use layermask=3\n", di->layer2);
		return(5);
	}

	ret = add_dlayer2(di);
	if (ret) {
		fprintf(stdout,"add_dlayer2 failed\n");
		return(ret);
	} else {
		fprintf(stdout,"created new User-Layer2: 0x%x\n", di->layer2);
	}

	ret = mISDN_write_frame(di->device, buf, di->layer2 | FLG_MSG_DOWN,
		PH_ACTIVATE | REQUEST, 0, 0, NULL, 0);

	/* wait for at least timer3 for */
        ret = mISDN_read(di->device, buf, 128, TIMEOUT_10SEC);
        if (ret>0) {
                if ((frm->prim == (PH_ACTIVATE | CONFIRM)) ||
                    (frm->prim == (PH_ACTIVATE | INDICATION))) {
                        fprintf(stdout,"layer1 activated (0x%x)\n", frm->prim);
                        return(0);
                } else {
                	fprintf(stdout,"unable to activate layer1 (0x%x)\n", frm->prim);
                	return(6);
                }
        }

	return(10);
}

void main_data_loop(devinfo_t *di)
{
	unsigned char buf[1024];
	
        iframe_t *frm = (iframe_t *)buf;
        int ret, i;
        unsigned char *p;
                
	/* test sending a trash frame... */ 
	/*
	unsigned char tx_buf[1024];
	
	msg[0] = 0x01;
	msg[1] = 0x02;
	msg[2] = 0x03;
	msg[3] = 0x04;
	msg[4] = 0x05;

	ret = mISDN_write_frame(di->device, buf, di->layer2 | FLG_MSG_DOWN,
        	PH_DATA | REQUEST, 0, 5, msg, 0);
	if (ret>0) {
		fprintf(stdout,"unable to send data (0x%x)\n", ret);
	}
	*/

        printf ("waiting for data (use CTRL-C to cancel) ...\n");
        while (1)  {
        	ret = mISDN_read(di->device, buf, 1024, 10);
        	if (ret >= mISDN_HEADER_LEN) {
        		p = buf + mISDN_HEADER_LEN;
        		
        		switch(frm->prim) {
        			case (PH_DATA | INDICATION) :
        				printf ("(PH_DATA | INDICATION) : \n\t");
		        		for (i=0; i < frm->len; i++) {
        					printf ("0x%02x ", *(p+i));
						if ((!((i+1) % 8)) && (i<frm->len))
							printf ("\n\t");
					}
					printf ("\n\n");
					break;
				default:
					printf ("unhandled prim(0x%x) len(0x%x)\n", frm->prim, frm->len);
        		}
        	}
        }
}
        


int main(argc,argv)
int argc;
char *argv[]; 
{
	int aidx=1;
	char sw;
	int err;

	fprintf(stdout,"\n\nTest-L1 $Revision: 1.1 $\n");
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 1;
	mISDN.func = 0;
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
			} 
                        aidx++;
		} while (aidx<argc);
	}
	if (0>(mISDN.device = mISDN_open())) {
		printf("TestmISDN cannot open mISDN due to %s\n",
			strerror(errno));
		return(1);
	}
	
	set_signals();
	err = do_setup(&mISDN);
	if (err)
		fprintf(stdout,"do_setup error %d\n", err);
		
	main_data_loop(&mISDN);
		
	err=mISDN_close(mISDN.device);
	if (err)
		fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
			strerror(err));
	
	return(0);
}
