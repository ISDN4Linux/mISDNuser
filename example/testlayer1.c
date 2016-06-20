/*
 * Copyright 2008 Martin Bachem <info@colognechip.com>
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
 * HOWTO:
 * ------
 *
 * testlayer1 expects every frame it sends down to layer1 is immediatly looped
 * back. An easy way to do this is using a cross-plug with terminiation:
 *
 *    3 RX+ 2a ---+--------+          /           / /
 *    4 TX+ 1a --/         |          ---------- / /
 *                     [100 Ohm]     | 87654321 | /
 *    5 TX- 1b ----+       |         |__      __|/
 *    6 RX- 2b -----\------+            |____|
 *
 * well, you could also make layer1 be opened as TE (using '--te') and connect
 * the TE with an NT running a layer1 testloop. Using TE mode without
 * requesting any data (by calling './testlayer1 --te')
 * is a simple S0 bus activatation test.
 * BE CAREFULL: if you request testing data pipes, please make sure you did
 * not connect your ISDN TA with your Telco's NT ;)
 * BY DEFAULT testlayer1 opens your ISDN TA as NT !
 *
 *
 *
 * Examples:
 * --------
 *
 *    testlayer1 --d -v     : using NT mode, verbose output,
 *                            stress test D Channel data with default
 *                            packet size
 *
 *    testlayer1 --d=20 -v  : using NT mode, verbose output,
 *                            stress test D Channel data with 20 bytes
 *                            packet size
 *
 *    testlayer1 -v         : only check if S0 bus can be activated as NT
 *    testlayer1 -v --te    : only check if S0 bus activates using your
 *                            ISDN TA in TE mode
 *
 *
 *    testlayer1 --testloop=[x] : control testloops (line RX -> line TX)
 *                                0 disable all
 *                                bit0 : 1: set B1 loop, 0: unset B1 loop
 *                                bit1 : 1: set B2 loop, 0: unset B2 loop
 *                                bit2 : 1: set D  loop, 0: unset D  loop
 *
 *                                e.g. --testloop=3 enables B1+B2 and
 *                                disables D loop
 *
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
#include <mISDN/mISDNif.h>
#include <mISDN/af_isdn.h>

void usage(void) {
	printf("\nvalid options are:\n");
	printf("\n");
	printf("  --card=<n>         use card number n (default 0)\n");
	printf("  --d,  --d=<n>      enable D channel stream with <n> packet sz\n");
	printf("  --b1, --b1=<n>     enable B channel stream with <n> packet sz\n");
	printf("  --b2, --b2=<n>     enable B channel stream with <n> packet sz\n");
	printf("  --te               use TA in TE mode (default is NT)\n");
	printf("  --testloop=<x>     set up hardware testloops, 7 activates B1+B2+D:\n");
	printf("                            0: deactivate testloops\n");
	printf("                            1: activate B1 testloop rx -> line tx\n");
	printf("                            2: activate B2 testloop rx -> line tx\n");
	printf("                            4: activate  D testloop rx -> line tx\n");
	printf("  --playload=<x>     hdlc package payload types:\n");
	printf("                            0: always 0x00\n");
	printf("                            1: incremental playload (default)\n");
	printf("                         0xFF: always 0xFF\n");
	printf("  --btrans           use bchannels in transparant mode\n");
	printf("  --stop=<n>         stop testlayer1 after <n> seconds\n");
	printf("  --sleep=<n>        tweak usleep() duration in mail data loop\n");
	printf("  -v, --verbose=<n>  set debug verbose level\n");
	printf("  --help             Usage ; printout this information\n");
	printf("\n");
}

#define MISDN_BUF_SZ	2048 // data buffer for message mISDNcore message Q

#define CHAN_B1		0
#define CHAN_B2		1
#define CHAN_D  	2
#define MAX_CHAN	3

#define CHAN_ACTIVATE   1
#define CHAN_DEACTIVATE 0

#define TX_BURST_HEADER_SZ 5


static char * CHAN_NAMES[MAX_CHAN] = {
	"B1", "B2", "D "
};

/*
 * default Paket sizes for each chan if not modified
 * by -b1=x -b2=x or -d=x
 * (!) and enabled by -mX
 */
static int CHAN_DFLT_PKT_SZ[MAX_CHAN] = {
	1800, // default B1 pkt sz
	1800, // default B2 pkt sz
	64, // default D pkt sz
};

static int CHAN_MAX_PKT_SZ[MAX_CHAN] = {
	2048, // max B1 pkt sz
	2048, // max B2 pkt sz
	260, // max D pkt sz
};

typedef struct {
	unsigned long total; // total bytes
	unsigned long delta; // delta bytes to last measure point
	unsigned long pkt_cnt; // total number of packets
	unsigned long err_pkt;
} data_stats_t;

/* channel data test stream */
typedef struct {
	int tx_size;
	int rx_size;
	int tx_ack;
	int transp_rx;
	int activated;
	unsigned long long t_start; // timestamp of first channel activate CNF
	data_stats_t rx, tx; // contains data statistics
	unsigned long seq_num;
	unsigned char idle_cnt; // cnt seconds if channel is acivated by idle
	unsigned char res_cnt; // cnt channel ressurections
	unsigned char hdlc;
	unsigned char toggle;    // ACTIVATE/DEACTIVATE channel during testloop
	unsigned int toggle_cnt; // toggle count
} channel_data_t;

typedef struct _devinfo {
	int device;
	int cardnr;

	int layerid[4]; // layer1 ID (sockets)
	struct sockaddr_mISDN laddr[4];

	channel_data_t ch[4]; // data channel info for D,B2,B2,(E)
	unsigned char channel_mask; // enable channel streams
} devinfo_t;



// cmd line opts
static int debug = 0;
static int usleep_val = 200;
static int te_mode = 0;
static int stop = 0; // stop after x seconds
static int exit_app = 0; // stop after x seconds
static unsigned char payload = 1;
static int btrans = 0;
static int testloop = 0;

// globals
static devinfo_t mISDN;
static unsigned char trans_tx_val[MAX_CHAN] = {0, 0, 0};
static unsigned char trans_rx_val[MAX_CHAN] = {0, 0, 0};


// function header
int control_channel(devinfo_t *di, unsigned char chan_id, unsigned char activate);


void sig_handler(int sig) {
	switch (sig) {
		case 2:
			exit_app = 1; // request graceful stop of mainloop
			return;
		default:
			exit(0);
	}
}

void set_signals() {
	/* Set up the signal handler */
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
}

#define TICKS_PER_SEC 1000000

unsigned long long get_tick_count(void) {
	struct timeval tp;

	gettimeofday(&tp, 0);
	return ((unsigned long long) ((unsigned) tp.tv_sec) * TICKS_PER_SEC + ((unsigned) tp.tv_usec));
}

int printhexdata(FILE *f, int len, u_char *p) {
	while (len--) {
		fprintf(f, "0x%02x", *p++);
		if (len) {
			fprintf(f, " ");
		}
	}
	fprintf(f, "\n");
	return (0);
}

int setup_bchannel(devinfo_t *di, unsigned char bch) {
	int ret;

	if (di->ch[bch].hdlc) {
		di->layerid[bch] = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_B_HDLC);
	} else {
		di->layerid[bch] = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_B_RAW); // transparent
	}

	if (di->layerid[bch] < 0) {
		fprintf(stdout, "could not open bchannel socket %s\n", strerror(errno));
		return 2;
	}

	ret = fcntl(di->layerid[bch], F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stdout, "fcntl error %s\n", strerror(errno));
		return 3;
	}

	di->laddr[bch].family = AF_ISDN;
	di->laddr[bch].dev = di->cardnr;
	di->laddr[bch].channel = bch + 1;

	ret = bind(di->layerid[bch], (struct sockaddr *) &di->laddr[bch], sizeof (di->laddr[bch]));

	if (ret < 0) {
		fprintf(stdout, "could not bind bchannel socket %s\n", strerror(errno));
		return 4;
	}

	return ret;
}

/*
 * activate / deactivate B-Channel
 */
int control_channel(devinfo_t *di, unsigned char chan_id, unsigned char activate)
{
	unsigned char buffer[2048];
	struct mISDNhead *hh = (struct mISDNhead *) buffer;
	struct timeval tout;
	fd_set rds;
	int ret;
	socklen_t alen;
	unsigned char done = 0;

	hh->prim = activate ? PH_ACTIVATE_REQ : PH_DEACTIVATE_REQ;
	hh->id = MISDN_ID_ANY;
	ret = sendto(di->layerid[chan_id], buffer, MISDN_HEADER_LEN, 0, NULL, 0);

	if (ret < 0) {
		fprintf(stdout, "could not send %s %s\n",
			(activate ? "PH_ACTIVATE_REQ" : "PH_DEACTIVATE_REQ"),
			strerror(errno));
		return 0;
	}

	if (!di->ch[chan_id].toggle_cnt || exit_app) {
		fprintf(stdout, "--> %s - %s\n", CHAN_NAMES[chan_id], (activate ? "PH_ACTIVATE_REQ" : "PH_DEACTIVATE_REQ"));
	}

	int max_retry = 128;

	do {
		tout.tv_usec = 1000 * 100;
		tout.tv_sec = 0;
		FD_ZERO(&rds);
		FD_SET(di->layerid[chan_id], &rds);

		ret = select(di->layerid[chan_id]+1, &rds, NULL, NULL, &tout);
		if (debug > 3) {
			fprintf(stdout, "select ret=%d\n", ret);
		}
		if (ret < 0) {
			fprintf(stdout, "select error  %s\n", strerror(errno));
			return 0;
		}
		if (ret == 0) {
			fprintf(stdout, "select timeeout\n");
			return 0;
		}

		if (FD_ISSET(di->layerid[chan_id], &rds)) {
			alen = sizeof (di->laddr[chan_id]);
			ret = recvfrom(di->layerid[chan_id], buffer, 2048, 0,
				(struct sockaddr *) &di->laddr[chan_id], &alen);
			if (ret < 0) {
				fprintf(stdout, "recvfrom error %s\n",
					strerror(errno));
				return 0;
			}
			if (debug > 3) {
				fprintf(stdout, "alen =%d, dev(%d) channel(%d)\n",
					alen, di->laddr[chan_id].dev, di->laddr[chan_id].channel);
			}
			if (activate && ((hh->prim == PH_ACTIVATE_IND) || (hh->prim == PH_ACTIVATE_CNF))) {
				if (!di->ch[chan_id].toggle_cnt || exit_app) {
					fprintf(stdout,
						"<-- %s - PH_ACTIVATE_%s\n",
						CHAN_NAMES[chan_id],
						(hh->prim == PH_ACTIVATE_IND) ? "IND" : "CNF");
				}
				di->ch[chan_id].toggle_cnt++;
				di->ch[chan_id].activated = 1;
				done = 1;
			} else
			if (!activate && ((hh->prim == PH_DEACTIVATE_IND) || (hh->prim == PH_DEACTIVATE_CNF))) {
				if (!di->ch[chan_id].toggle_cnt || exit_app) {
					fprintf(stdout,
						"<-- %s - PH_DEACTIVATE_%s\n",
						CHAN_NAMES[chan_id],
						(hh->prim == PH_DEACTIVATE_IND) ? "IND" : "CNF");
				}
				di->ch[chan_id].toggle_cnt++;
				di->ch[chan_id].activated = 0;
				done = 1;
			} else {
				if (debug)
					fprintf(stdout, "<-- %s -  unhandled prim 0x%x\n",
					CHAN_NAMES[chan_id], hh->prim);
			}
		} else {
			fprintf(stdout, "chan fd not in set\n");
			return 0;
		}
	} while (!done && (max_retry-- > 0));

	return (done==1);
}

/*
 * send PH_ACTIVATE_REQ and wait for PH_ACTIVATE_IND
 * returns 0 if PH_ACTIVATE_IND received within timeout interval
 */
int do_setup(devinfo_t *di) {
	if (!control_channel(di, CHAN_D, CHAN_ACTIVATE)) {
		fprintf(stdout, "error activateing D Channel\n");
		return 0;
	}

	if (di->ch[CHAN_D].activated) {
		if ((di->ch[CHAN_B1].tx_ack) && (!setup_bchannel(di, CHAN_B1))) {
			if (!control_channel(di, CHAN_B1, CHAN_ACTIVATE)) {
				fprintf(stdout, "error activateing B1 Channel\n");
				return 0;
			}
		}
		if ((di->ch[CHAN_B2].tx_ack) && (!setup_bchannel(di, CHAN_B2))) {
			if (!control_channel(di, CHAN_B2, CHAN_ACTIVATE)) {
				fprintf(stdout, "error activateing B2 Channel\n");
				return 0;
			}
		}
	}

	return (di->ch[CHAN_D].activated ? 0 : 1);
}

int check_rx_data_hdlc(devinfo_t *di, int ch_idx, int ret, unsigned char *rx_buf) {
	int rx_error = 0;
	unsigned long rx_seq_num;
	int i;

	if ((ret - MISDN_HEADER_LEN) == di->ch[ch_idx].tx_size) {
		// check first byte to be ch_idx
		if (rx_buf[MISDN_HEADER_LEN + 0] != ch_idx) {
			if (debug > 1) {
				printf("RX DATA ERROR: channel index %s\n",
				CHAN_NAMES[ch_idx]);
			}
			rx_error++;
		}

		// check sequence number
		rx_seq_num = (rx_buf[MISDN_HEADER_LEN + 1] << 24) +
			(rx_buf[MISDN_HEADER_LEN + 2] << 16) +
			(rx_buf[MISDN_HEADER_LEN + 3] << 8) +
			rx_buf[MISDN_HEADER_LEN + 4];

		if (rx_seq_num == di->ch[ch_idx].seq_num) {
			// expect next seq no at next rx
			di->ch[ch_idx].seq_num++;
		} else {
			if (debug > 1) {
				printf("RX DATA ERROR: sequence no %s\n",
				CHAN_NAMES[ch_idx]);
			}
			// either return crit error, or resync req no
			di->ch[ch_idx].seq_num = rx_seq_num + 1;
			rx_error++;
		}

		// check data
		switch (payload) {
			case 0:
				for (i = 0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
					if (rx_buf[MISDN_HEADER_LEN + TX_BURST_HEADER_SZ + i]) {
						printf("RX DATA ERROR: packet data error %s\n",
							CHAN_NAMES[ch_idx]);
						rx_error++;
						break;
					}
				}
				break;
			case 0xFF:
				for (i = 0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
					if (rx_buf[MISDN_HEADER_LEN + TX_BURST_HEADER_SZ + i] != 0xFF) {
						printf("RX DATA ERROR: packet data error %s\n",
							CHAN_NAMES[ch_idx]);
						rx_error++;
						break;
					}
				}
				break;
			case 1:
			default:
				for (i = 0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
					if (rx_buf[MISDN_HEADER_LEN + TX_BURST_HEADER_SZ + i] != (i & 0xFF)) {
						if (debug > 1) {
							printf("RX DATA ERROR: packet data error %s\n",
							CHAN_NAMES[ch_idx]);
						}
						rx_error++;
					}
				}
				break;
		}
	} else {
		if (debug > 1) {
			printf("RX DATA ERROR: packet size %s (%i,%i)\n",
				CHAN_NAMES[ch_idx], ret, di->ch[ch_idx].tx_size);
			printhexdata(stdout, ret - MISDN_HEADER_LEN, rx_buf + MISDN_HEADER_LEN);
		}
		rx_error++;
	}

	return rx_error;
}

int check_rx_data_trans(devinfo_t *di, int ch_idx, int ret, unsigned char *rx_buf) {
	int i;
	int rx_err = 0;

	if (((trans_rx_val[ch_idx] + 1) & 0xFF) != (rx_buf[MISDN_HEADER_LEN] & 0xFF)) {
		rx_err++;
	}

	for (i = MISDN_HEADER_LEN; i < ret - 1; i++) {
		rx_err += (int)(((rx_buf[i] + 1) & 0xFF) != (rx_buf[i + 1] & 0xFF));
	}

	trans_rx_val[ch_idx] = rx_buf[i];
	// printf ("%i ", rx_err);
	// printhexdata(stdout, ret-MISDN_HEADER_LEN, rx_buf + MISDN_HEADER_LEN);

	return rx_err;
}

int build_tx_data(devinfo_t *di, int ch_idx, unsigned char *p) {
	unsigned char *tmp = p;
	int i;

	if (di->ch[ch_idx].hdlc) {
		// 5 bytes package header
		*p++ = ch_idx;
		for (i = 0; i < 4; i++) {
			*p++ = ((di->ch[ch_idx].tx.pkt_cnt >> (8 * (3 - i))) & 0xFF);
		}

		// data
		switch (payload) {
			case 0:
				for (i = 0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
					*p++ = 0;
				}
				break;
			case 0xff:
				for (i = 0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
					*p++ = 0xFF;
				}
				break;

			case 1:
			default:
				for (i = 0; i < (di->ch[ch_idx].tx_size - TX_BURST_HEADER_SZ); i++) {
					*p++ = i;
				}
		}
	} else {
		// incremental data in transparent mode
		for (i = 0; i < di->ch[ch_idx].tx_size; i++) {
			*p++ = trans_tx_val[ch_idx]++;
		}
	}

	di->ch[ch_idx].tx.pkt_cnt++;

	return (p - tmp);
}

int main_data_loop(devinfo_t *di) {
	unsigned long long t1, t2;
	unsigned char rx_buf[MISDN_BUF_SZ];
	unsigned char tx_buf[MISDN_BUF_SZ];
	struct mISDNhead *hhtx = (struct mISDNhead *) tx_buf;
	struct mISDNhead *hhrx = (struct mISDNhead *) rx_buf;
	int ret, l, ch_idx;
	struct timeval tout;
	socklen_t alen;
	fd_set rds;
	unsigned char rx_error;
	unsigned long rx_delta;
	unsigned int running_since = 0;

	t1 = get_tick_count();

	tout.tv_usec = 0;
	tout.tv_sec = 1;


	for (ch_idx = 0; ch_idx < MAX_CHAN; ch_idx++) {
		// start timer tick for first TX packet
		if (!di->ch[ch_idx].t_start) {
			di->ch[ch_idx].t_start = t1;
			di->ch[ch_idx].seq_num = di->ch[ch_idx].tx.pkt_cnt;
		}
	}

	printf("\nwaiting for data (use CTRL-C to cancel) stop(%i) sleep(%i)...\n", stop, usleep_val);
	while (1) {
		for (ch_idx = 0; ch_idx < MAX_CHAN; ch_idx++) {
			if (!di->ch[ch_idx].activated) {
				if (di->ch[ch_idx].toggle) {
					if (control_channel(di, ch_idx, CHAN_ACTIVATE)) {
						di->ch[ch_idx].tx_ack = 1;
					}
				}
				continue;
			}

			/* write data */
			if (di->ch[ch_idx].tx_ack) {
				l = build_tx_data(di, ch_idx, tx_buf + MISDN_HEADER_LEN);
				if (debug > 4) {
					printf("%s-TX size(%d) : ", CHAN_NAMES[ch_idx], l);
					printhexdata(stdout, l, tx_buf + MISDN_HEADER_LEN);
				}

				hhtx->prim = PH_DATA_REQ;
				hhtx->id = MISDN_ID_ANY;
				ret = sendto(di->layerid[ch_idx], tx_buf, l + MISDN_HEADER_LEN,
					0, (struct sockaddr *) &di->laddr[ch_idx],
					sizeof (di->laddr[ch_idx]));

				di->ch[ch_idx].tx_ack--;
			}

			/* read data */
			FD_ZERO(&rds);
			FD_SET(di->layerid[ch_idx], &rds);

			ret = select(di->layerid[ch_idx] + 1, &rds, NULL, NULL, &tout);
			if (ret < 0) {
				fprintf(stdout, "select error %s\n", strerror(errno));
			}

			if ((ret > 0) && (FD_ISSET(di->layerid[ch_idx], &rds))) {
				alen = sizeof (di->laddr[ch_idx]);
				ret = recvfrom(di->layerid[ch_idx], rx_buf, MISDN_BUF_SZ, 0,
					(struct sockaddr *) &di->laddr[ch_idx], &alen);
				if (ret < 0) {
					fprintf(stdout, "recvfrom error %s\n",
					strerror(errno));
				}
				if (debug > 3) {
					fprintf(stdout, "alen(%d) dev(%d) channel(%d)\n",
					alen, di->laddr[ch_idx].dev, di->laddr[ch_idx].channel);
				}

				if (hhrx->prim == PH_DATA_IND) {
					if (debug > 2) {
						fprintf(stdout, "<-- %s - PH_DATA_IND\n",
						CHAN_NAMES[ch_idx]);
					}
					if (debug > 3) {
						printhexdata(stdout, ret - MISDN_HEADER_LEN,
						rx_buf + MISDN_HEADER_LEN);
					}

					di->ch[ch_idx].rx.pkt_cnt++;

					/* line rate means 2 bytes crc
					 * and 2 bytes HDLC flags overhead each packet
					 */
					if (di->ch[ch_idx].hdlc) {
						di->ch[ch_idx].rx.total += 4;
					}
					di->ch[ch_idx].rx.total += ret - MISDN_HEADER_LEN;

					// validate RX data
					if (di->ch[ch_idx].hdlc) {
						rx_error = check_rx_data_hdlc(di, ch_idx, ret, rx_buf);
					} else {
						rx_error = check_rx_data_trans(di, ch_idx, ret, rx_buf);
					}

					if (rx_error) {
						di->ch[ch_idx].rx.err_pkt++;
					}

					if (di->ch[ch_idx].toggle) {
						control_channel(di, ch_idx, CHAN_DEACTIVATE);
					}
				} else if (hhrx->prim == PH_DATA_CNF) {
					if (!di->ch[ch_idx].toggle) {
						// ready to send next TX package in next mainloop
						di->ch[ch_idx].tx_ack++;
					}
				} else {
					if (debug > 2) {
						fprintf(stdout, "<-- %s - unhandled prim 0x%x\n",
						CHAN_NAMES[ch_idx], hhrx->prim);
					}
				}
			}
		}

		/* relax cpu usage */
		usleep(usleep_val);

		// print out data rate stats:
		t2 = get_tick_count();
		if ((t2 - t1) > (TICKS_PER_SEC / 1) || exit_app) {
			t1 = t2;
			running_since++;

			for (ch_idx = 0; ch_idx < MAX_CHAN; ch_idx++) {
				rx_delta = (di->ch[ch_idx].rx.total - di->ch[ch_idx].rx.delta);
				printf("%s rate/s: %lu, rate-avg: %4.3f,"
					" rx total: %lu kb since %llu secs,"
					" pkt(tx/rx): %lu/%lu, rx-err:%lu,%i DEACT/ACT(%d)\n",
					CHAN_NAMES[ch_idx], rx_delta,
					(double) ((double) ((unsigned long long) di->ch[ch_idx].rx.total * TICKS_PER_SEC)
					/ (double) (t2 - di->ch[ch_idx].t_start)),
					(di->ch[ch_idx].rx.total),
					di->ch[ch_idx].t_start ? ((t2 - di->ch[ch_idx].t_start) / TICKS_PER_SEC) : 0,
					di->ch[ch_idx].tx.pkt_cnt,
					di->ch[ch_idx].rx.pkt_cnt,
					di->ch[ch_idx].rx.err_pkt,
					di->ch[ch_idx].res_cnt,
					di->ch[ch_idx].toggle_cnt);

				/*
				 * care for idle but 'active' channels, what happens
				 * e.g. on CRC errors down in layer1
				 */
				if ((di->ch[ch_idx].activated) && (!rx_delta)) {
					di->ch[ch_idx].idle_cnt++;
					if ((di->ch[ch_idx].idle_cnt > 2) && !di->ch[ch_idx].toggle) {
						// resurrect data pipe
						di->ch[ch_idx].seq_num++;
						di->ch[ch_idx].res_cnt++;
						di->ch[ch_idx].tx_ack = 1;
						di->ch[ch_idx].idle_cnt = 0;
					}
				} else {
					di->ch[ch_idx].idle_cnt = 0;
				}

				di->ch[ch_idx].rx.delta = di->ch[ch_idx].rx.total;
			}
			printf("\n");

			// closing all channels before exit
			if (exit_app || (stop && (running_since >= stop))) {
				for (ch_idx = 0; ch_idx < MAX_CHAN; ch_idx++) {
					if (di->ch[ch_idx].activated) {
						di->ch[ch_idx].activated = 0;
						control_channel(di, ch_idx, CHAN_DEACTIVATE);
					}

					if (mISDN.layerid[ch_idx] > 0) {
						fprintf(stdout, "closing socket '%s'\n", CHAN_NAMES[ch_idx]);
						close(mISDN.layerid[ch_idx]);
					}
				}
				return 0;
			}
		}
	}
}


int
connect_layer1_d(devinfo_t *di) {
	int cnt, ret = 0;
	int sk;
	struct mISDN_devinfo devinfo;

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

	if (debug > 1) {
		fprintf(stdout, "%d devices found\n", cnt);
	}
	if (cnt < di->cardnr + 1) {
		fprintf(stderr, "cannot config card nr %d only %d cards\n",
			di->cardnr, cnt);
		return 4;
	}

	devinfo.id = di->cardnr;
	ret = ioctl(sk, IMGETDEVINFO, &devinfo);
	if (ret < 0) {
		fprintf(stdout, "ioctl error %s\n", strerror(errno));
	} else if (debug > 1) {
		fprintf(stdout, "        id:             %d\n", devinfo.id);
		fprintf(stdout, "        Dprotocols:     %08x\n", devinfo.Dprotocols);
		fprintf(stdout, "        Bprotocols:     %08x\n", devinfo.Bprotocols);
		fprintf(stdout, "        protocol:       %d\n", devinfo.protocol);
		fprintf(stdout, "        nrbchan:        %d\n", devinfo.nrbchan);
		fprintf(stdout, "        name:           %s\n", devinfo.name);
	}
	close(sk);

	if (te_mode) {
		mISDN.layerid[CHAN_D] = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_TE_S0);
	} else {
		mISDN.layerid[CHAN_D] = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_NT_S0);
	}
	if (mISDN.layerid[CHAN_D] < 1) {
		fprintf(stderr, "could not open socket '%s': %s\n",
			strerror(errno),
			(te_mode) ? "ISDN_P_TE_S0" : "ISDN_P_NT_S0");
		return 5;
	}

	ret = fcntl(di->layerid[CHAN_D], F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		fprintf(stdout, "fcntl error %s\n", strerror(errno));
		return 6;
	}

	di->laddr[CHAN_D].family = AF_ISDN;
	di->laddr[CHAN_D].dev = di->cardnr;
	di->laddr[CHAN_D].channel = 0;
	ret = bind(di->layerid[CHAN_D], (struct sockaddr *) &di->laddr[CHAN_D], sizeof (di->laddr[CHAN_D]));

	if (ret < 0) {
		fprintf(stdout, "could not bind l1 socket %s\n", strerror(errno));
		return 7;
	}

	return 0;
}

int
set_hw_loop(devinfo_t *di)
{
	int ret;
	struct mISDN_ctrl_req creq;

	creq.op = MISDN_CTRL_LOOP;
	creq.channel = di->channel_mask;
	ret = ioctl(mISDN.layerid[CHAN_D], IMCTRLREQ, &creq);
	if (ret < 0) {
		fprintf(stdout, "set_hw_loop ioctl error %s\n", strerror(errno));
	}
	return ret;
}


int main(int argc, char *argv[]) {
	int c, err;
	unsigned char ch_idx;
	devinfo_t *di;

	static struct option testlayer1_opts[] = {
		{"verbose", optional_argument, 0, 'v'},
		{"card", optional_argument, 0, 'c'},
		{"sleep", optional_argument, 0, 's'},
		{"payload", required_argument, 0, 'p'},
		{"btrans", no_argument, &btrans, 1},
		{"stop", required_argument, 0, 't'},
		{"te", no_argument, &te_mode, 1},
		{"d", optional_argument, 0, 'x'},
		{"b1", optional_argument, 0, 'y'},
		{"b2", optional_argument, 0, 'z'},
		{"testloop", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
	};

	di = &mISDN;
	memset(&mISDN, 0, sizeof (mISDN));
	mISDN.cardnr = 0;

	for (;;) {
		int option_index = 0;

		c = getopt_long(argc, argv, "vcsxyz", testlayer1_opts,
			&option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'v':
				debug = 1;
				if (optarg) {
					debug = atoi(optarg);
				}
				break;
			case 'c':
				if (optarg) {
					mISDN.cardnr = atoi(optarg);
				}
				break;
			case 's':
				if (optarg)
					usleep_val = atoi(optarg);
				break;
			case 'p':
				if (optarg) {
					payload = atoi(optarg);
				}
				break;
			case 't':
				if (optarg) {
					stop = atoi(optarg);
				}
				break;
			case 'x':
				mISDN.channel_mask |= 4;
				if (optarg) {
					mISDN.ch[CHAN_D].tx_size = atoi(optarg);
				}
				break;
			case 'y':
				mISDN.channel_mask |= 1;
				if (optarg) {
					mISDN.ch[CHAN_B1].tx_size = atoi(optarg);
				}
				break;
			case 'z':
				mISDN.channel_mask |= 2;
				if (optarg) {
					mISDN.ch[CHAN_B2].tx_size = atoi(optarg);
				}
				break;
			case 'l':
				mISDN.channel_mask = atoi(optarg) & 0x7;
				testloop = 1;
				break;
			case 'h':
				usage();
				return 0;
		}
	}

	fprintf(stdout, "\n\ntestlayer1 - card(%i) debug(%i) playload(%i) btrans(%i) testloop(%i)\n",
		mISDN.cardnr, debug, payload, btrans, testloop);

	// init Data burst values
	for (ch_idx = 0; ch_idx < MAX_CHAN; ch_idx++) {
		if (mISDN.channel_mask & (1 << ch_idx)) {
			if (mISDN.ch[ch_idx].tx_size < 0) {
				mISDN.ch[ch_idx].toggle = 1;
				mISDN.ch[ch_idx].tx_size *= -1;
			}

			if (!mISDN.ch[ch_idx].tx_size) {
				mISDN.ch[ch_idx].tx_size = CHAN_DFLT_PKT_SZ[ch_idx];
			}

			if (mISDN.ch[ch_idx].tx_size > CHAN_MAX_PKT_SZ[ch_idx]) {
				mISDN.ch[ch_idx].tx_size = CHAN_MAX_PKT_SZ[ch_idx];
			}

			mISDN.ch[ch_idx].hdlc = (!(((ch_idx == CHAN_B1) || (ch_idx == CHAN_B2)) && btrans));
			mISDN.ch[ch_idx].tx_ack = 1;

			fprintf(stdout, "chan %s stream enabled with packet sz %d bytes\n",
				CHAN_NAMES[ch_idx], di->ch[ch_idx].tx_size);
		}
	}

	err = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (err < 0) {
		fprintf(stderr, "cannot open mISDN due to %s\n",
			strerror(errno));
		return 1;
	}
	close(err);

	err = connect_layer1_d(&mISDN);
	if (err) {
		fprintf(stdout, "error(%d) connecting layer1\n", err);
		return err;
	}


	if (testloop) {
		set_hw_loop(&mISDN);
	} else {
		set_signals();
		err = do_setup(&mISDN);
		if (err) {
			fprintf(stdout, "do_setup error %d\n", err); return
			(0);
		}
		if (mISDN.channel_mask) {
			main_data_loop(&mISDN);
		} else {
			fprintf(stdout, "no channels request, try [--d, --b1, --b2]\n");
		}

		sig_handler(9); // abuse as cleanup
	}

	return (0);
}
