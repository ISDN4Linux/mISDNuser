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
#include <sys/time.h>
#include "mISDNlib.h"

void usage(pname) 
char *pname;
{
	fprintf(stderr, "Call with %s [options]\n",pname);
	fprintf(stderr, "\n");
	fprintf(stderr, "\n     Valid options are:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -?              Usage ; printout this information\n");
	fprintf(stderr, "  -c<n>           use card number n (default 1)\n"); 
	fprintf(stderr, "  -vn             Printing debug info level n\n");
	fprintf(stderr, "\n");
}

typedef struct _devinfo {
	int	device;
	int	cardnr;
	int	func;
	int	d_stid;
	int	b_stid[2];
	int	layer1;
	int	layer2;
	int	unconfirmed;
} devinfo_t;


static int VerifyOn=1;
static devinfo_t mISDN;


void sig_handler(int sig)
{
	int err;
	fprintf(stdout, "exiting...\n");
        err = mISDN_close(mISDN.device);
        fprintf(stdout, "mISDN_close: error(%d): %s\n", err,
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
 
#define TICKS_PER_SEC 1000000
long get_tick_count(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);
	return ((unsigned)tp.tv_sec)*TICKS_PER_SEC+((unsigned)tp.tv_usec);
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
		fprintf(stdout, "cannot get layer2 (%d)\n", lid);
		return(15);
	}
	di->layer2 = lid;
                                                     
	if (!di->layer2)
		return(13);
	
	return(0);
}

int do_setup(devinfo_t *di)
{
	unsigned char buf[2048];
	iframe_t *frm = (iframe_t *)buf;
	int i, ret = 0;
	stack_info_t *stinf;
	long t1;

	ret = mISDN_get_stack_count(di->device);
	if (VerifyOn>1)
		fprintf(stdout, "%d stacks found\n", ret);
	if (ret < di->cardnr) {
		fprintf(stdout, "cannot config card nr %d only %d cards\n",
			di->cardnr, ret);
		return(2);
	}
	ret = mISDN_get_stack_info(di->device, di->cardnr, buf, 2048);
	if (ret<=0) {
		fprintf(stdout, "cannot get stackinfo err: %d\n", ret);
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
		fprintf(stdout, "cannot get layer1\n");
		return(4);
	}
	if (VerifyOn>1)
		fprintf(stdout, "layer1 id %08x\n", di->layer1);

	/* make sure this stack just consists of layer0 + layer1 */
	di->layer2 = mISDN_get_layerid(di->device, di->d_stid, 2);
	if (di->layer2 > 0) {
		fprintf(stdout, "layer2 found (%i), use layermask=3\n", di->layer2);
		return(5);
	}

	ret = add_dlayer2(di);
	if (ret) {
		fprintf(stdout, "add_dlayer2 failed\n");
		return(ret);
	} else {
		fprintf(stdout, "created new User-Layer2: 0x%x\n", di->layer2);
	}

	fprintf(stdout, "sending PH_ACTIVATE | REQUEST to layer1...\n");
	ret = mISDN_write_frame(di->device, buf, di->layer2 | FLG_MSG_DOWN,
		PH_ACTIVATE | REQUEST, 0, 0, NULL, 0);

	// wait for PH_ACTIVATE | INDICATION or CONFIRM
	t1 = get_tick_count();
	while (1) { 

		ret = mISDN_read(di->device, buf, 2048, 0);
		
		if (ret > 0) {
			if ((frm->prim == (PH_ACTIVATE | CONFIRM)) ||
			    (frm->prim == (PH_ACTIVATE | INDICATION))) {
				fprintf(stdout, "layer1 activated (0x%x)\n", frm->prim);
				return(0);
			}
			
			/*
			if (frm->prim == (MGR_SHORTSTATUS | INDICATION)) {
				fprintf(stdout, "got MGR_SHORTSTATUS | INDICATION\n");
			}
			fprintf(stdout, "got (0x%x), still waiting for PH_ACTIVATE | CONFIRM/INDICATION...\n", frm->prim);
			*/
		}
		
		if ((get_tick_count() - t1)  > (TICKS_PER_SEC * 5)) {
			printf(stdout, "unable to activate layer1 (TIMEOUT)\n");
			return(6);
		}
	}

	return(7);
}
	

int printhexdata(FILE *f, int len, u_char *p)
{
        while(len--) {
        	fprintf(f, "0x%02x", *p++);
        	if (len)
        		fprintf(f, " ");
	}
	fprintf(f, "\n");
	return(0);
}


                                

void main_data_loop(devinfo_t *di)
{
	long t1;
	unsigned char buf[2048];
	unsigned char tx_buf[2048];
		
        iframe_t *frm = (iframe_t *)buf;
        int ret, i;
        
        t1 = get_tick_count();
	
        printf ("waiting for data (use CTRL-C to cancel)...\n");
        while (1)  {
        	/* read data */
        	ret = mISDN_read(di->device, buf, 2048, 0);
        	if (ret >= mISDN_HEADER_LEN) {
        		
        		switch(frm->prim) {
        			case (PH_DATA | INDICATION) :
        				// layer1 gives rx frame
        				printf ("(PH_DATA | INDICATION) : \n\t");
					printhexdata(stdout, frm->len, buf + mISDN_HEADER_LEN);
					break;
					
				case (PH_DATA | CONFIRM) :
					// layer1 confirmes tx frame
					di->unconfirmed--;
					break;
				default:
					printf ("unhandled prim(0x%x) len(0x%x)\n", frm->prim, frm->len);
        		}
        	}
        	
        	/* write data */
        	if ((get_tick_count()-t1) > TICKS_PER_SEC) {
        		printf(".\n");

        		i=0;

        		/* send fake d-channel frame, here Tei request */
			tx_buf[i++] = 0xfC;
			tx_buf[i++] = 0xff;
			tx_buf[i++] = 0x03;
			tx_buf[i++] = 0x0f;
			tx_buf[i++] = 0xe9;
			tx_buf[i++] = 0x69;
			tx_buf[i++] = 0x01;
			tx_buf[i++] = 0xff;

			ret = mISDN_write_frame(di->device, buf, di->layer2 | FLG_MSG_DOWN,
				PH_DATA | REQUEST, 0, 8, tx_buf, 10);
			if (ret>0) {
				fprintf(stdout,"unable to send data (0x%x)\n", ret);
			} else {
				di->unconfirmed++;
			}
        		
        		t1 = get_tick_count();
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

	fprintf(stdout,"\n\nTest-L1 $Revision: 1.3 $\n");
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
	if (err) {
		fprintf(stdout, "do_setup error %d\n", err);
		return(0);
	}

	main_data_loop(&mISDN);

	err=mISDN_close(mISDN.device);
	if (err)
		fprintf(stdout,"mISDN_close: error(%d): %s\n", err,
			strerror(err));
	
	return(0);
}
