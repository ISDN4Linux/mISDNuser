/*
 * testlayer1.c
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <mISDNif.h>

#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>


void usage(char *pname)
{
	printf("Call with %s [options]\n",pname);
	printf("\n");
	printf("\n     Valid options are:\n");
	printf("\n");
	printf("  -?              Usage ; printout this information\n");
	printf("  -c<n>           use card number n (default 1)\n");
	printf("  -m<n>           binary channel mask (7 = run on B1,B2,D)\n");
	printf("  -vn             Printing debug info level n\n");
	printf("  -b1=N           use N bytes packet size for B1 frames\n");
	printf("  -b2=N           use N bytes packet size for B2 frames\n");
	printf("  -d=N            use N bytes packet size for D frames\n");
	printf("\n");
}

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
	int	activated;
	unsigned long long t_start; // time of day first TX
	data_stats_t rx, tx;    // contains data statistics
	unsigned long seq_num;
} channel_data_t;

typedef struct _devinfo {
	int			device;
	int			cardnr;
	int			layer1; // layer1 ID
	struct sockaddr_mISDN	l1addr;
	int			nds;

	channel_data_t		ch[4]; // data channel info for D,B2,B2,(E)
	unsigned char 		channel_mask; // enable channel data pump
} devinfo_t;

static int VerifyOn=1;
static devinfo_t mISDN;
static int usleep_val=200;


void sig_handler(int sig)
{
	fprintf(stdout, "exiting...\n");
	fflush(stdout);
	fflush(stderr);
	if (mISDN.layer1 > 0)
		close(mISDN.layer1);
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

/*
 * opens NT-mode layer1, send PH_ACTIVATE_REQ amd wait for PH_ACTIVATE_IND
 * returns 0 if PH_ACTIVATE_IND received within timeout interval
 *
 */
int do_setup(devinfo_t *di)
{
	int 			cnt, ret = 0;
	int			sk;
	struct mISDN_devinfo	devinfo;
	struct sockaddr_mISDN	l2addr;
	socklen_t		alen;
	struct mISDNhead	*hh;
	struct timeval		tout;
	fd_set			rds;
	unsigned char 		buffer[2048];
	struct mISDN_ctrl_req	creq;

	sk = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sk < 1) {
		fprintf(stderr, "could not open socket 'ISDN_P_BASE' %s\n", strerror(errno));
		return 2;
	}
	ret = ioctl(sk, IMGETCOUNT, &cnt);
	if (ret) {
		fprintf(stderr, "ioctl error %s\n", strerror(errno));
		close(sk);
		return 3;
	}

	if (VerifyOn>1)
		fprintf(stdout, "%d devices found\n", cnt);
	if (cnt < di->cardnr) {
		fprintf(stderr, "cannot config card nr %d only %d cards\n",
			di->cardnr, cnt);
		return 4;
	}

	devinfo.id = di->cardnr - 1;
	ret = ioctl(sk, IMGETDEVINFO, &devinfo);
	if (ret < 0) {
		fprintf(stdout, "ioctl error %s\n", strerror(errno));
	} else if (VerifyOn>1) {
		fprintf(stdout, "        id:             %d\n", devinfo.id);
		fprintf(stdout, "        Dprotocols:     %08x\n", devinfo.Dprotocols);
		fprintf(stdout, "        Bprotocols:     %08x\n", devinfo.Bprotocols);
		fprintf(stdout, "        protocol:       %d\n", devinfo.protocol);
		fprintf(stdout, "        nrbchan:        %d\n", devinfo.nrbchan);
		fprintf(stdout, "        name:           %s\n", devinfo.name);
	}
	close(sk);

	mISDN.layer1 = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_NT_S0);
	if (mISDN.layer1 < 1) {
		fprintf(stderr, "could not open socket 'ISDN_P_NT_S0': %s\n", strerror(errno));
		return 5;
	}
	
	di->nds = di->layer1 + 1;
	ret = fcntl(di->layer1, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stdout, "fcntl error %s\n", strerror(errno));
		return 6;
	}
	
	di->l1addr.family = AF_ISDN;
	di->l1addr.dev = di->cardnr - 1;
	di->l1addr.channel = 0;
	ret = bind(di->layer1, (struct sockaddr *) &di->l1addr, sizeof(di->l1addr));

	if (ret < 0) {
		fprintf(stdout, "could not bind l1 socket %s\n", strerror(errno));
		return 7;
	}

	hh = (struct mISDNhead *)buffer;
	hh->prim = PH_ACTIVATE_REQ;
	hh->id   = MISDN_ID_ANY;
	ret = sendto(di->layer1, buffer, MISDN_HEADER_LEN, 0, NULL, 0);

	while (1) {
		tout.tv_usec = 0;
		tout.tv_sec = 5;
		FD_ZERO(&rds);
		FD_SET(di->layer1, &rds);

		ret = select(di->nds, &rds, NULL, NULL, &tout);
		if (VerifyOn>3)
			fprintf(stdout,"select ret=%d\n", ret);
		if (ret < 0) {
			fprintf(stdout, "select error %s\n", strerror(errno));
			return 9;
		}
		if (ret == 0) {
			fprintf(stdout, "select timeeout\n");
			return 10;
		}

		if (FD_ISSET(di->layer1, &rds)) {
			alen = sizeof(di->l1addr);
			ret = recvfrom(di->layer1, buffer, 300, 0, (struct sockaddr *) &di->l1addr, &alen);
			if (ret < 0) {
				fprintf(stdout, "recvfrom error %s\n", strerror(errno));
				return 11;
			}
			if (VerifyOn>3) {
				fprintf(stdout, "alen =%d, dev(%d) channel(%d)\n",
					alen, di->l1addr.dev, di->l1addr.channel);
			}
			if (hh->prim == PH_ACTIVATE_IND) {
				if (VerifyOn>2)
					fprintf(stdout, "got PH_ACTIVATE_IND\n");
				return 0;
			} else {
				if (VerifyOn>2)
					fprintf(stdout, "got unhandled prim 0x%x\n", hh->prim);
			}
		}
	}

	return 666;
}


int main(int argc, char *argv[])
{
	int aidx=1;
	char sw;
	int err;
	unsigned char ch_idx;
	devinfo_t *di;

	fprintf(stdout,"\n\ntestlayer1 $Revision: 1.5 $\n");
	memset(&mISDN, 0, sizeof(mISDN));

	// default stack no (starts with 1!)
	mISDN.cardnr = 1;

	di = &mISDN;
	di->channel_mask = 0;

	if (argc<1) {
		fprintf(stderr, "Error: Not enough arguments please check\n");
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

	init_af_isdn();
	err = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (err < 0) {
		fprintf (stderr, "cannot open mISDN due to %s\n",
		    strerror(errno));
		return 1;
	}
	close(err);

	set_signals();

	// init Data burst values
	for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
	{
		if (di->channel_mask & (1 << ch_idx))
		{
			if (!di->ch[ch_idx].tx_size)
				di->ch[ch_idx].tx_size = CHAN_DFLT_PKT_SZ[ch_idx];

			if (di->ch[ch_idx].tx_size > CHAN_MAX_PKT_SZ[ch_idx])
				di->ch[ch_idx].tx_size = CHAN_MAX_PKT_SZ[ch_idx];

			fprintf (stdout, "channel_mask 0x%x enables %s, packet size: %d bytes\n",
			    di->channel_mask, CHAN_NAMES[ch_idx], di->ch[ch_idx].tx_size);
			di->ch[ch_idx].tx_ack = 1;
		}
	}

	err = do_setup(&mISDN);
	if (err) {
		fprintf(stdout, "do_setup error %d\n", err);
		return(0);
	}

	if (mISDN.layer1 > 0) {
		fprintf (stdout, "using fCard-%d, usleep_val %d\n", mISDN.cardnr, usleep_val);
		// main_data_loop(&mISDN);

		fprintf (stdout, "closing socket mISDN.layer1\n");
		close(mISDN.layer1);
	}

	fflush(stdout);
	fflush(stderr);
	return(0);
}
