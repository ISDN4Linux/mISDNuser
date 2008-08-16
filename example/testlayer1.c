/* $Id$
 * (c) 2008 Martin Bachem, info@colognechip.com
 *
 * This file is part of mISDNuser/example
 *
 * 'testlayer1' is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2
 *
 * 'testlayer1' is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 'testlayer1', If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * TODO:
 *   - activate and service B channels if requested by --b1 or --b2
 */


#include <stdio.h>
#include <getopt.h>
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
	printf("Call with %s [options]\n", pname);
	printf("\n");
	printf("\n     Valid options are:\n");
	printf("\n");
	printf("  --card=<n>         use card number n (default 1)\n");
	printf("  --d                enable D channel stream with <n> packet sz\n");
	printf("  --b1, --b1=<n>     enable B channel stream with <n> packet sz\n");
	printf("  --b2, --b2=<n>     enable B channel stream with <n> packet sz\n");
	printf("  -v, --verbose=<n>  set debug verbose level\n");
	printf("  --help             Usage ; printout this information\n");
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
	unsigned char 		channel_mask; // enable channel streams
} devinfo_t;



static int debug=0;
static int usleep_val=200;
static int te_mode=0;

static devinfo_t mISDN;

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


/*
 * opens NT-mode layer1, send PH_ACTIVATE_REQ and wait for PH_ACTIVATE_IND
 * returns 0 if PH_ACTIVATE_IND received within timeout interval
 *
 */
int do_setup(devinfo_t *di)
{
	int 			cnt, ret=0;
	int			sk;
	struct mISDN_devinfo	devinfo;
	socklen_t		alen;
	struct mISDNhead	*hh;
	struct timeval		tout;
	fd_set			rds;
	unsigned char 		buffer[2048];

	sk = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sk < 1) {
		fprintf(stderr, "could not open socket 'ISDN_P_BASE' %s\n",
			strerror(errno));
		return 2;
	}
	ret = ioctl(sk, IMGETCOUNT, &cnt);
	if (ret) {
		fprintf(stderr, "ioctl error %s\n", strerror(errno));
		close(sk);
		return 3;
	}

	if (debug>1)
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
	} else if (debug>1) {
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
		fprintf(stderr, "could not open socket 'ISDN_P_NT_S0': %s\n",
			strerror(errno));
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
	fprintf(stdout, "--> PH_ACTIVATE_REQ\n");
	ret = sendto(di->layer1, buffer, MISDN_HEADER_LEN, 0, NULL, 0);

	while (1) {
		tout.tv_usec = 0;
		tout.tv_sec = 1;
		FD_ZERO(&rds);
		FD_SET(di->layer1, &rds);

		ret = select(di->nds, &rds, NULL, NULL, &tout);
		if (debug>3)
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
			ret = recvfrom(di->layer1, buffer, 300, 0,
				       (struct sockaddr *) &di->l1addr, &alen);
			if (ret < 0) {
				fprintf(stdout, "recvfrom error %s\n",
					strerror(errno));
				return 11;
			}
			if (debug>3) {
				fprintf(stdout, "alen =%d, dev(%d) channel(%d)\n",
					alen, di->l1addr.dev, di->l1addr.channel);
			}
			if (hh->prim == PH_ACTIVATE_IND) {
				fprintf(stdout, "<-- PH_ACTIVATE_IND\n");
				di->ch[CHAN_D].activated = 1;
				
				// TODO: acivate b channels if requested by --b1 or --b2
				return 0;
			} else {
				if (debug)
					fprintf(stdout, "<-- unhandled prim 0x%x\n", hh->prim);
			}
		}
	}

	return 666;
}


int main_data_loop(devinfo_t *di)
{
	unsigned long long t1, t2;
	unsigned char rx_buf[MISDN_BUF_SZ];
	unsigned char tx_buf[MISDN_BUF_SZ];
	struct  mISDNhead *hhtx = (struct  mISDNhead *)tx_buf;
	struct  mISDNhead *hhrx = (struct  mISDNhead *)rx_buf;
	unsigned char *p, *msg;
	int ret, i, l, ch_idx;
	struct timeval tout;
	socklen_t alen;
	fd_set rds;
	unsigned long rx_seq_num;
	unsigned char rx_error;


	t1 = get_tick_count();

	tout.tv_usec = 0;
	tout.tv_sec = 1;

	printf ("\nwaiting for data (use CTRL-C to cancel)...\n");
	while (1)
	{
		/* write data */
		for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
		{
			if ((di->ch[ch_idx].tx_ack) && (di->ch[ch_idx].activated))
			{
	        		// start timer tick at first TX packet
				if (!di->ch[ch_idx].t_start) {
					di->ch[ch_idx].t_start = get_tick_count();
					di->ch[ch_idx].seq_num = di->ch[ch_idx].tx.pkt_cnt;
				}

				p = msg = tx_buf + MISDN_HEADER_LEN;

				/*
				* TX Frame
				*   - 1 byte ch_idx (CHAN_D=0, ...)
				*   - 4 bytes big endian pckt counter
				*   - n bytes data
				*/
				*p++ = ch_idx;
				for (i=0; i<4; i++)
					*p++ = ((di->ch[ch_idx].tx.pkt_cnt >> (8*(3-i))) & 0xFF);
				di->ch[ch_idx].tx.pkt_cnt++;

	        		// random data, here: incremental
				for (i=0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++)
					*p++ = i;

				if (debug > 2) {
					printf("%s-TX size(%d) : ",
					       CHAN_NAMES[ch_idx], i);
					printhexdata(stdout, i, tx_buf + MISDN_HEADER_LEN);
				}

				l = p - msg;
				hhtx->prim = PH_DATA_REQ;
				hhtx->id = MISDN_ID_ANY;
				ret = sendto(di->layer1, tx_buf, l + MISDN_HEADER_LEN,
					     0, (struct sockaddr *)&di->l1addr,
					     sizeof(di->l1addr));

				di->ch[ch_idx].tx_ack--;
			}
		}

		/* read data */
		// TODO: also read B1 and B2 if required, for now only D Channel is handled
		FD_ZERO(&rds);
		FD_SET(di->layer1, &rds);

		ret = select(di->nds, &rds, NULL, NULL, &tout);
		if (ret < 0)
			fprintf(stdout, "select error %s\n", strerror(errno));

		if ((ret > 0) && (FD_ISSET(di->layer1, &rds))) {
			alen = sizeof(di->l1addr);
			ret = recvfrom(di->layer1, rx_buf, MISDN_BUF_SZ, 0,
				       (struct sockaddr *) &di->l1addr, &alen);
			if (ret < 0)
				fprintf(stdout, "recvfrom error %s\n",
					strerror(errno));
			if (debug>3)
				fprintf(stdout, "alen =%d, dev(%d) channel(%d)\n",
					alen, di->l1addr.dev, di->l1addr.channel);

			if (hhrx->prim == PH_DATA_IND) {
				if (debug) 
					fprintf(stdout, "<-- PH_DATA_IND\n");
				if (debug > 2)
					printhexdata(stdout, ret-MISDN_HEADER_LEN, rx_buf + MISDN_HEADER_LEN);

				ch_idx = CHAN_D;

				di->ch[ch_idx].tx_ack++;
				di->ch[ch_idx].rx.total += ret + 2; // line rate means 2 bytes crc overhead here
				di->ch[ch_idx].rx.pkt_cnt++;

			        // validate RX data
				rx_error = 0;
				if ((ret-MISDN_HEADER_LEN) == di->ch[ch_idx].tx_size) {
					// check first byte to be ch_idx
					if (rx_buf[MISDN_HEADER_LEN + 0] != ch_idx) {
						if (debug > 1)
							printf ("RX DATA ERROR: channel index %s\n", CHAN_NAMES[ch_idx]);
						rx_error++;
			        		// return;
					}

					// check sequence number
					rx_seq_num = (rx_buf[MISDN_HEADER_LEN + 1] << 24) +
					    (rx_buf[MISDN_HEADER_LEN + 2] << 16) +
					    (rx_buf[MISDN_HEADER_LEN + 3] << 8) +
					    rx_buf[MISDN_HEADER_LEN + 4];

					if (rx_seq_num == di->ch[ch_idx].seq_num)
			        		// expect next seq no at next rx
						di->ch[ch_idx].seq_num++;
					else {
						if (debug > 1)
							printf ("RX DATA ERROR: sequence no %s\n", CHAN_NAMES[ch_idx]);
       						// either return crit error, or resync req no
						di->ch[ch_idx].seq_num = rx_seq_num+1;
						rx_error++;
					}

					// check data
					for (i=0; i<(di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
						if (rx_buf[MISDN_HEADER_LEN + TX_BURST_HEADER_SZ + i] != (i & 0xFF)) {
							if (debug > 1)
								printf ("RX DATA ERROR: packet data error %s\n", CHAN_NAMES[ch_idx]);
							rx_error++;
						}
					}

				} else {
					if (debug > 1)
						printf ("RX DATA ERROR: packet size %s (%i,%i,%i)\n",
							CHAN_NAMES[ch_idx],
							ret, di->ch[ch_idx].tx_size,
							MISDN_HEADER_LEN);
					rx_error++;
				}

				if (rx_error)
					di->ch[ch_idx].rx.err_pkt++;
			} else {
				if (debug>1)
					fprintf(stdout, "<-- unhandled prim 0x%x\n", hhrx->prim);
			}
		}
		
		/* relax cpu usage */
		usleep(usleep_val);

        	// print out data rate stats:
		t2 = get_tick_count();
		if ((t2-t1) > (TICKS_PER_SEC / 1))
		{
        		// printf ("%llu - %llu = %llu\n", t2, t1, t2-t1);
			t1 = get_tick_count();

			for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
			{
				printf ("%s rate/s: %lu, rate-avg: %4.3f, rx total: %lu kb since %llu secs, pkt(rx/tx): %lu/%lu, rx-err:%lu\n",
					CHAN_NAMES[ch_idx],
					(di->ch[ch_idx].rx.total - di->ch[ch_idx].rx.delta),
					(double)((double)((unsigned long long)di->ch[ch_idx].rx.total * TICKS_PER_SEC) / (double)(t2 - di->ch[ch_idx].t_start)),
					(di->ch[ch_idx].rx.total),
					di->ch[ch_idx].t_start?((t2 - di->ch[ch_idx].t_start) / TICKS_PER_SEC):0,
					di->ch[ch_idx].rx.pkt_cnt,
					di->ch[ch_idx].tx.pkt_cnt,
					di->ch[ch_idx].rx.err_pkt);
				di->ch[ch_idx].rx.delta = di->ch[ch_idx].rx.total;
			}
			printf ("\n");
		}
	}
}

int main(int argc, char *argv[])
{
	int c, err;
	unsigned char ch_idx;
	devinfo_t *di;

	static struct option testlayer1_opts[] = {
		{"verbose",	optional_argument,	0,		'v'},
		{"card",     	optional_argument,	0,		'c'},
		{"sleep",     	optional_argument,	0,		's'},
		{"te",     	no_argument,		&te_mode,	1},
		{"d",     	optional_argument,	0,		'x'},
		{"b1",     	optional_argument,	0,		'y'},
		{"b2",     	optional_argument,	0,		'z'},
		{"help",	no_argument,		0,		'h'},
	};

	di = &mISDN;
	memset(&mISDN, 0, sizeof(mISDN));
	mISDN.cardnr = 1;

	for (;;) {
		int option_index = 0;

		c = getopt_long (argc, argv, "vcsxyz", testlayer1_opts,
				 &option_index);
		if (c == -1)
			break;
		
		switch (c) {
			case 'v':
				debug=1;
				if (optarg)
					debug = atoi(optarg);
				break;
			case 'c':
				if (optarg)
					mISDN.cardnr = atoi(optarg);
				break;
			case 'x':
				mISDN.channel_mask |= 4;
				if (optarg)
					mISDN.ch[CHAN_D].tx_size = atoi(optarg);
				break;
			case 'y':
				mISDN.channel_mask |= 1;
				if (optarg)
					mISDN.ch[CHAN_B1].tx_size = atoi(optarg);
				break;
			case 'z':
				mISDN.channel_mask |= 2;
				if (optarg)
					mISDN.ch[CHAN_B2].tx_size = atoi(optarg);
				break;
		}
	}

	fprintf(stdout,"\n\ntestlayer1 $Revision: 2.? $, card(%i) debug(%i)\n",
		mISDN.cardnr, debug);

	// init Data burst values
	for (ch_idx=0; ch_idx<MAX_CHAN; ch_idx++)
	{
		if (mISDN.channel_mask & (1 << ch_idx))
		{
			if (!mISDN.ch[ch_idx].tx_size)
				mISDN.ch[ch_idx].tx_size = CHAN_DFLT_PKT_SZ[ch_idx];

			if (mISDN.ch[ch_idx].tx_size > CHAN_MAX_PKT_SZ[ch_idx])
				mISDN.ch[ch_idx].tx_size = CHAN_MAX_PKT_SZ[ch_idx];

			di->ch[ch_idx].tx_ack = 1;

			fprintf (stdout, "chan %s stream enabled with packet sz %d bytes\n",
				 CHAN_NAMES[ch_idx], di->ch[ch_idx].tx_size);
		}
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
	err = do_setup(&mISDN);
	if (err) {
		fprintf(stdout, "do_setup error %d\n", err);
		return(0);
	}

	if (mISDN.channel_mask) {
		 main_data_loop(&mISDN);
	} else {
		fprintf (stdout, "no channels request, try [--d, --b1, --b2]\n");
		
	}

	close(mISDN.layer1);

	fflush(stdout);
	fflush(stderr);
	return(0);
}
