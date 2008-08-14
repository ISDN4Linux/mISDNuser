/*
	$Id: testlayer1.c,v 1.5 2007/08/28 11:19:45 martinb1 Exp $

	Sample code to act as a userland dummy layer2
	and communicate with NT Mode Layer1

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
	printf("Call with %s [options]\n",pname);
	printf("\n");
	printf("\n     Valid options are:\n");
	printf("\n");
	printf("  -?              Usage ; printout this information\n");
	printf("  -c<n>           use card number n (default 1)\n"); 
	printf("  -m<n>           binary channel mask (7 = run with B1,B2,D)\n"); 
	printf("  -vn             Printing debug info level n\n");
	printf("  -b1=N           use N bytes packet size for B1 frames\n");
	printf("  -b2=N           use N bytes packet size for B2 frames\n");
	printf("  -d=N            use N bytes packet size for D frames\n");
	printf("\n");
}

#define ISDN_PID_L2_B_USER	0x420000ff
#define ISDN_PID_L3_B_USER	0x430000ff


#define MISDN_BUF_SZ	2048 // data buffer for message mISDNcore message Q

#define CHAN_B1		0
#define CHAN_B2		1
#define CHAN_D  	2
#define CHAN_E		3
#define MAX_CHAN	4

#define TX_BURST_HEADER_SZ 5



static char * CHAN_NAMES[MAX_CHAN] = {
	"B1", "B2", "D ", "E "
};

/*
 * default Paket sizes for each chan if not modified
 * by -b1=x -b2=x or -d=x
 * (!) and enabled by -mX
 */
static int CHAN_DFLT_PKT_SZ[MAX_CHAN] = {
	1800,	// default B1 pkt sz
	1800,	// default B2 pkt sz
	128,	// default D pkt sz
	0	// default E pkt sz irrelevant
};

static int CHAN_MAX_PKT_SZ[MAX_CHAN] = {
	2048,	// max B1 pkt sz
	2048,	// max B2 pkt sz
	260,	// max D pkt sz
	0	// max E pkt sz irrelevant
};


typedef struct {
	unsigned long total;	// total bytes 
	unsigned long delta;	// delta bytes to last measure point
	unsigned long pkt_cnt;	// total number of packets
	unsigned long err_pkt;
} data_stats_t;


/* channel data test stream */
typedef struct {
	int	tx_size;
	int	rx_size;
	int	tx_ack;

	int	frm_addr;
	int 	stid;		// stack ID
	int	layer2;		// layer2 ID

	int	activated;

	unsigned long long t_start; // time of day first TX

	data_stats_t rx, tx;    // contains data statistics

	unsigned long seq_num;

} channel_data_t;



typedef struct _devinfo {
	int	device;
	int	cardnr;
	int	layer1; // layer1 ID

	channel_data_t	ch[4]; // data channel info for D,B2,B2,(E)
	unsigned char channel_mask; // enable channel data pump
} devinfo_t;


static int VerifyOn=1;
static devinfo_t mISDN;
static int usleep_val=200;


void sig_handler(int sig)
{
	int err;
	printf("exiting...\n");
        err = mISDN_close(mISDN.device);
        printf("mISDN_close: error(%d): %s\n", err, strerror(err));
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
unsigned long long get_tick_count(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);
	return ((unsigned long long)((unsigned)tp.tv_sec)*TICKS_PER_SEC+((unsigned)tp.tv_usec));
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
	li.st = di->ch[CHAN_D].stid;

	ret = mISDN_new_layer(di->device, &li);
	if (ret<0)
		return(12);

	di->ch[CHAN_D].frm_addr = ret;
	printf("registered D layer2 addr(0x%x), stack_id(0x%x)\n",
	       di->ch[CHAN_D].frm_addr, di->ch[CHAN_D].stid);

	di->ch[CHAN_D].layer2 = li.id;

	ret = mISDN_register_layer(di->device, di->ch[CHAN_D].stid, di->ch[CHAN_D].layer2);
        if (ret) {
                printf("register_layer ret(%d)\n", ret);
                return(14);
        }	

        lid = mISDN_get_layerid(di->device, di->ch[CHAN_D].stid, 2);
	if (lid<0) {
		printf("cannot get layer2 (%d)\n", lid);
		return(15);
	}
	di->ch[CHAN_D].layer2 = lid;

	if (!di->ch[CHAN_D].layer2)
		return(13);
	
	return(0);
}


int setup_bchannel(devinfo_t *di, unsigned char bch)
{
	mISDN_pid_t pid;
	int ret;
	layer_info_t li;

	if ((bch != CHAN_B1) && (bch != CHAN_B2)) {
		printf ("setup_bchannel: wrong channel, use [CHAN_B1,CHAN_B2] !\n");
		return(-1);
	}

	memset(&li, 0, sizeof(layer_info_t));
	strcpy(&li.name[0], "B L2");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[2] = ISDN_PID_L2_B_USER;
	li.pid.layermask = ISDN_LAYER(2);
	li.st = di->ch[bch].stid;
	ret = mISDN_new_layer(di->device, &li);
	if (ret) {
		fprintf(stdout, "new_layer ret(%d)\n", ret);
		return(0);
	}
	if (li.id) {
		di->ch[bch].frm_addr = li.id;

		memset(&pid, 0, sizeof(mISDN_pid_t));
		pid.protocol[1] = ISDN_PID_L1_B_64HDLC;
		pid.protocol[2] = ISDN_PID_L2_B_USER;

		pid.layermask = ISDN_LAYER(1) | ISDN_LAYER(2);

		ret = mISDN_set_stack(di->device, di->ch[bch].stid, &pid);
		if (ret) {
			fprintf(stdout, "set_stack ret(%d)\n", ret);
			return(0);
		}
		/* Wait until the stack is really available */
		ret = mISDN_get_setstack_ind(di->device, di->ch[bch].frm_addr);
		if (ret) {
			fprintf(stdout, "get_setstack_ind ret (%d)\n", ret);
			return(0);
		}
		/* get the registered id of the  B layer2 */
		ret = mISDN_get_layerid(di->device, di->ch[bch].stid, 2);
		if (ret <= 0) {
			fprintf(stdout, "get_layerid b3 ret(%d)\n", ret);
			return(0);
		}
		di->ch[bch].frm_addr = ret;
		/* get the registered id of the b2  layer */
		ret = mISDN_get_layerid(di->device, di->ch[bch].stid, 2);
		if (ret > 0)
			di->ch[bch].layer2 = ret;

		printf("B%d b_l2 id %08x\n", bch+1, di->ch[bch].layer2);
	} else
		ret = 0;
	return(ret);
}


/* find stack with layermask=3, and try to activate layer1 */
int do_setup(devinfo_t *di)
{
	unsigned char buf[MISDN_BUF_SZ];
	iframe_t *frm = (iframe_t *)buf;
	int i, ret = 0;
	stack_info_t *stinf;
	long long t1;

	ret = mISDN_get_stack_count(di->device);
	if (VerifyOn>1)
		printf("%d stacks found\n", ret);
	if (ret < di->cardnr) {
		printf("cannot config card nr %d only %d cards\n",
			di->cardnr, ret);
		return(2);
	}
	ret = mISDN_get_stack_info(di->device, di->cardnr, buf, 2048);
	if (ret<=0) {
		printf("cannot get stackinfo err: %d\n", ret);
		return(3);
	}
	stinf = (stack_info_t *)&frm->data.p;
	if (VerifyOn>1)
		mISDNprint_stack_info(stdout, stinf);
	di->ch[CHAN_D].stid = stinf->id;
	for (i=0;i<2;i++) {
		if (stinf->childcnt>i)
			di->ch[i].stid = stinf->child[i];
		else
			di->ch[i].stid = 0;
	}

	di->layer1 = mISDN_get_layerid(di->device, di->ch[CHAN_D].stid, 1);
	if (di->layer1<0) {
		printf("cannot get layer1\n");
		return(4);
	}
	if (VerifyOn>1)
		printf("layer1 id %08x\n", di->layer1);

	/* make sure this stack just consists of layer0 + layer1 */
	if (mISDN_get_layerid(di->device, di->ch[CHAN_D].stid, 2) > 0) {
		printf("D layer2 found (%i), use layermask=3\n", di->ch[CHAN_D].layer2);
		return(5);
	}

	ret = add_dlayer2(di);
	if (ret) {
		printf("add_dlayer2 failed\n");
		return(ret);
	} else {
		printf("created new User-Layer2: 0x%x\n", di->ch[CHAN_D].layer2);
	}

	printf("sending PH_ACTIVATE | REQUEST to layer1...\n");
	ret = mISDN_write_frame(di->device, buf, di->ch[CHAN_D].layer2 | FLG_MSG_DOWN,
		PH_ACTIVATE | REQUEST, 0, 0, NULL, 0);

	// wait for PH_ACTIVATE | INDICATION or CONFIRM
	t1 = get_tick_count();
	while (1)
	{ 
		ret = mISDN_read(di->device, buf, 2048, 0);

		if (ret > 0) {
			if ((frm->prim == (PH_ACTIVATE | CONFIRM)) ||
			    (frm->prim == (PH_ACTIVATE | INDICATION))) {
				printf("layer1 activated prim(0x%x) addr(0x%x)\n",
				        frm->prim, frm->addr);

				di->ch[CHAN_D].activated = 1;

				if (di->channel_mask & (1 << CHAN_B1)) {
					// resgistert B1 layer and activate
					setup_bchannel(di, CHAN_B1);
					printf("sending PH_ACTIVATE | REQUEST to B1 layer1...\n");
					ret = mISDN_write_frame(di->device, buf, di->ch[CHAN_B1].layer2 | FLG_MSG_DOWN,
						PH_ACTIVATE | REQUEST, 0, 0, NULL, 0);
				}

				if (di->channel_mask & (1 << CHAN_B2)) {
					// resgistert B2 layer and activate
					setup_bchannel(di, CHAN_B2);
					printf("sending PH_ACTIVATE | REQUEST to B2 layer1...\n");
					ret = mISDN_write_frame(di->device, buf, di->ch[CHAN_B2].layer2 | FLG_MSG_DOWN,
						PH_ACTIVATE | REQUEST, 0, 0, NULL, 0);
				}

				// the B Channel Activate Inidications will drop in later in main_data_loop
				return(0);
			}

			if (frm->prim == (MGR_SHORTSTATUS | INDICATION)) {
				fprintf(stdout, "got MGR_SHORTSTATUS | INDICATION\n");
			} else {
				printf("got (0x%x) addr(0x%x), still waiting for PH_ACTIVATE | [CONFIRM,INDICATION]\n", frm->prim, frm->addr);
			}
		}

		if ((get_tick_count() - t1)  > (TICKS_PER_SEC * 5)) {
			printf("unable to activate layer1 (TIMEOUT)\n");
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
	unsigned long long t1, t2;
	unsigned char rx_buf[MISDN_BUF_SZ];
	unsigned char tx_buf[MISDN_BUF_SZ];

        iframe_t *frm = (iframe_t *)rx_buf;
        int ret, i;

        int ch_idx;
        unsigned long rx_seq_num;
        unsigned char rx_error;

	t1 = get_tick_count();

        printf ("waiting for data (use CTRL-C to cancel)...\n");
        while (1) 
        {
        	/* relax cpu usage */
        	usleep(usleep_val);

        	/* write data */
        	for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
        	{
			// if ((get_tick_count()-t1) > (TICKS_PER_SEC / 1))
			if ((di->ch[ch_idx].tx_ack) && (di->ch[ch_idx].activated))
	        	{
	        		// start timer tick at first TX packet
	        		if (!di->ch[ch_idx].t_start) {
	        			di->ch[ch_idx].t_start = get_tick_count();
	        			di->ch[ch_idx].seq_num = di->ch[ch_idx].tx.pkt_cnt;
	        		}

	        		di->ch[ch_idx].tx_ack--;

				/* 
				 * TX Frame
				 *   - 1 byte ch_idx (CHAN_D=0, ...)
				 *   - 4 bytes big endian pckt counter
				 *   - n bytes data
				 */
				tx_buf[0] = ch_idx;
				for (i=0; i<4; i++) 
					tx_buf[i+1] = ((di->ch[ch_idx].tx.pkt_cnt >> (8*(3-i))) & 0xFF);
				di->ch[ch_idx].tx.pkt_cnt++;

	        		// random data, here: incremental
	        		for (i=0; i < di->ch[ch_idx].tx_size; i++) {
	        			tx_buf[TX_BURST_HEADER_SZ+i] = i;
	        		}

				if (VerifyOn > 2) {
					printf("%s-TX size(%d), addr(0x%08x) : ",
				       	       CHAN_NAMES[ch_idx], i, di->ch[ch_idx].frm_addr);
					printhexdata(stdout, i, tx_buf);
				}

				ret = mISDN_write_frame(di->device, rx_buf, di->ch[ch_idx].layer2 | FLG_MSG_DOWN,
					PH_DATA | REQUEST, 0, i, tx_buf, 10);

				if (ret>0) {
					fprintf(stdout,"unable to send data (0x%x)\n", ret);
				}
	       		}
	        }

        	/* read data */
        	ret = mISDN_read(di->device, rx_buf, MISDN_BUF_SZ, 0);
        	if (ret >= mISDN_HEADER_LEN)
        	{
	       		switch (frm->addr & MSG_DIR_MASK)
        		{
        			case FLG_MSG_UP:
        				ch_idx = -1;

					if ((frm->addr & LAYER_ID_MASK) == 0)
					{
						if ((frm->addr & CHILD_ID_MASK) == 0x000000)
							ch_idx = CHAN_D;

						if ((frm->addr & CHILD_ID_MASK) == 0x010000)
							ch_idx = CHAN_B1;

						if ((frm->addr & CHILD_ID_MASK) == 0x020000)
							ch_idx = CHAN_B2;
					}

        				if (ch_idx >= 0)
        				{
			        		switch(frm->prim)
			        		{
			        			case (PH_ACTIVATE | CONFIRM):
			        			case (PH_ACTIVATE | INDICATION):
			        				printf ("%s activated (0x%x)\n",
			        				        CHAN_NAMES[ch_idx], frm->addr);
			        				di->ch[ch_idx].activated = 1;
			        				break;

			        			case (PH_DATA | INDICATION) :
			        				// layer1 gives rx frame
			        				if (VerifyOn > 2) {
			        					printf ("%s-RX size(%d), addr(0x%x) : ",
				        				        CHAN_NAMES[ch_idx], frm->len, frm->addr);

									printhexdata(stdout, frm->len, rx_buf + mISDN_HEADER_LEN);
								}

								di->ch[ch_idx].rx.pkt_cnt++;

								di->ch[ch_idx].rx.total += (frm->len + 2); // accumulate date incl. CRC overhead

								rx_error = 0;
			        				// analyze RX data
			        				if (frm->len == di->ch[ch_idx].tx_size)
			        				{

			        					// check first byte to be ch_idx
			        					if (rx_buf[mISDN_HEADER_LEN + 0] != ch_idx) {
			        						if (VerifyOn > 1)
				        						printf ("RX DATA ERROR: channel index %s\n", CHAN_NAMES[ch_idx]);
										rx_error++;
			        						// return;
			        					}

			        					// check sequence number
			        					rx_seq_num = (rx_buf[mISDN_HEADER_LEN + 1] << 24) + 
			        					             (rx_buf[mISDN_HEADER_LEN + 2] << 16) + 
			        					             (rx_buf[mISDN_HEADER_LEN + 3] << 8) +
			        					              rx_buf[mISDN_HEADER_LEN + 4];

			        					if (rx_seq_num == di->ch[ch_idx].seq_num)
			        					{
			        						// expect next seq no at next rx
			        						di->ch[ch_idx].seq_num++;
			        					} else {
			        						if (VerifyOn > 1) {
				        						printf ("RX DATA ERROR: sequence no %s\n", CHAN_NAMES[ch_idx]);
										}
			        						// either return crit error, or resync req no
			        						di->ch[ch_idx].seq_num = rx_seq_num+1;
			        						// return;
			        						rx_error++;
			        					}

			        					// check data
			        					for (i=0; i<(di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
			        						if (rx_buf[mISDN_HEADER_LEN + TX_BURST_HEADER_SZ + i] != (i & 0xFF)) {
			        							if (VerifyOn > 1) {
				        							printf ("RX DATA ERROR: packet data error %s\n", CHAN_NAMES[ch_idx]);
											}
											rx_error++;
			        							// return;
			        						}
			        					}

			        				} else {
			        					if (VerifyOn > 1) {
				        					printf ("RX DATA ERROR: packet size %s\n", CHAN_NAMES[ch_idx]);
									}
			        					// return;
			        					rx_error++;
			        				}

								if (rx_error)
									di->ch[ch_idx].rx.err_pkt++;
								break;

							case (PH_DATA | CONFIRM) :
								di->ch[ch_idx].tx_ack++;
								if (VerifyOn > 2)
			        					printf ("%s-TX [PH_DATA | CONFIRM]\n", CHAN_NAMES[ch_idx]);
								break;

							default:
								printf ("%s unhandled prim(0x%x) len(0x%x) addr(0x%x)\n",
								        CHAN_NAMES[ch_idx], frm->prim, frm->len, frm->addr);
								break;

			        		}
			        	} else {
			        		printf ("unhandled message from layer1 addr(0x%x)\n", frm->addr);
			        	}

		        		break; // case FLG_MSG_UP

        			default:
        				printf ("unhandled message from layer1 addr(0x%x)\n", frm->addr);
        		}
        	}

        	// print out data rate stats:
        	t2 = get_tick_count();
        	if ((t2-t1) > (TICKS_PER_SEC / 1)) {

        		// printf ("%llu - %llu = %llu\n", t2, t1, t2-t1);
	        	t1 = get_tick_count();

	        	for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
	        	{
	        		printf ("%s rate/s: %8lu, rate-avg: %8.3f, rx total: %8lu kb since %6llu secs, pkt(rx/tx): %8lu/%8lu, rx-err:%8lu\n",
	        		        CHAN_NAMES[ch_idx],
	        		        (di->ch[ch_idx].rx.total - di->ch[ch_idx].rx.delta),
	        		        (double)((double)((unsigned long long)di->ch[ch_idx].rx.total * TICKS_PER_SEC) / (double)(t2 - di->ch[ch_idx].t_start)),
	        		        (di->ch[ch_idx].rx.total / 1024),
	        		        di->ch[ch_idx].t_start?((t2 - di->ch[ch_idx].t_start) / TICKS_PER_SEC):0,
	        		        di->ch[ch_idx].rx.pkt_cnt,
	        		        di->ch[ch_idx].tx.pkt_cnt,
	        		        di->ch[ch_idx].rx.err_pkt
	        		);

	        		di->ch[ch_idx].rx.delta = di->ch[ch_idx].rx.total;
	        	}
	        	printf ("\n");
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
	unsigned char ch_idx;
	devinfo_t *di;

	fprintf(stdout,"\n\nTest-L1 $Revision: 1.5 $\n");
	memset(&mISDN, 0, sizeof(mISDN));

	// default stack no (starts with 1!)
	mISDN.cardnr = 1;

	di = &mISDN;
	di->channel_mask = 0;

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
						if (argv[aidx][2])
							VerifyOn=atol(&argv[aidx][2]);
						break;

					case 'c':
						if (argv[aidx][2])
							mISDN.cardnr=atol(&argv[aidx][2]);
						break;

					case 'm':
						if (argv[aidx][2])
							di->channel_mask = atol(&argv[aidx][2]);
						break;

					case 's':
						if (argv[aidx][2])
							usleep_val = atol(&argv[aidx][2]);
						break;

					case 'b':
						if (argv[aidx][2] == '1') {
							if ((argv[aidx][3] == '=') && (argv[aidx][4])) {
								di->ch[0].tx_size = atol(&argv[aidx][4]);
								di->channel_mask |= CHAN_B1;
							}
						} else if (argv[aidx][2] == '2') {
							if ((argv[aidx][3] == '=') && (argv[aidx][4])) {
								di->ch[1].tx_size = atol(&argv[aidx][4]);
								di->channel_mask |= CHAN_B2;
							}
						}
						break;

					case 'd':
						if ((argv[aidx][2] == '=') && (argv[aidx][3])) {
							di->ch[2].tx_size = atol(&argv[aidx][3]);
							di->channel_mask |= CHAN_D;
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
	printf ("using Card-%d, usleep_val %d\n",
	        mISDN.cardnr,
	        usleep_val);

	// init Data burst values
	for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
	{
		if (di->channel_mask & (1 << ch_idx))
		{
			if (!di->ch[ch_idx].tx_size)
				di->ch[ch_idx].tx_size = CHAN_DFLT_PKT_SZ[ch_idx];

			if (di->ch[ch_idx].tx_size > CHAN_MAX_PKT_SZ[ch_idx])
				di->ch[ch_idx].tx_size = CHAN_MAX_PKT_SZ[ch_idx];

			printf ("channel_mask 0x%x enables %s, packet size: %d bytes\n",
			        di->channel_mask,
			        CHAN_NAMES[ch_idx],
			        di->ch[ch_idx].tx_size);
	        	di->ch[ch_idx].tx_ack = 1;
	        }
	}

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
