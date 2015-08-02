/*
 *
 * Copyright 2015 Karsten Keil <keil@b1-systems.de>
 * Copyright 2011 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <getopt.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <mISDN/mISDNif.h>
#include <mISDN/af_isdn.h>

/* We do not have the all the ioctl controls mainstream yet so define it here.
 * It should still work then with the old standalone driver
 */

#ifndef MISDN_CTRL_L1_TESTS
#define MISDN_CTRL_L1_TESTS		0x00010000
#define MISDN_CTRL_L1_STATE_TEST	0x00010001
#define MISDN_CTRL_L1_AIS_TEST		0x00010002
#define MISDN_CTRL_L1_TS0_MODE		0x00010003
#define MISDN_CTRL_L1_GET_SYNC_INFO	0x00010004
#endif

#define	FRAME_SIZE	32
#define	SMFR_SIZE	(8 * FRAME_SIZE)
#define MFR_SIZE	(2 * SMFR_SIZE)
#define DATA_SIZE_125US	32
#define DATA_SIZE_1MS	(8 * 32)
#define DATA_SIZE_1S	(8000 * 32)

#define MFR_SYNC_VALUE	0x2c
#define MFR_SYNC_MASK	0xfc
#define MFR_SYNC_BIT	0x01
#define MFR_SYNC_BITS	8
#define MFR_SYNC_OFFSET	15

#define FR_FAS_VAL	0xd8
#define FR_FAS_VAL_FAIL	0xc0
#define FR_FAS_MASK	0xfe

#define FR_NFAS_B2	0xfa
#define FR_NFAS_B2_FAIL	0xf8

enum FrameTypes {
	ftNone,
	/* send out AIS (all TS 0xff) */
	ftAIS,
	/* Basic Frames */
	ftFAS,
	ftBIT2,
	/* Sub multi frames */
	ftSMFA,
	ftSMFB,
	/* Multi frames */
	ftMFA,
	ftMFB,
	/* Frames */
	ftFRAME_A,
	ftFRAME_B,
	ftFRAME_C,
	/* Control items */
	ftCtrl_Start,
	ftCtrl_Repeat,
	ftCtrl_End,
	ftCtrl_Stop
};

struct fr_cdesc {
	enum FrameTypes type;
	uint8_t prop;
	uint8_t subcnt;
	uint16_t count;
};

struct fr_flatdesc {
	enum FrameTypes type;	/* only basic frame types */
	uint8_t prop;
	enum FrameTypes otype;	/* original type from generation */
	uint8_t mf_pos;		/* 0 - 15 */
	uint32_t pos;
	uint8_t *data;
};

#define ftPROP_NONE	0x00
#define ftPROP_FAIL	0x01	/* frame type failure (/FAS or /BIT2) */
#define ftPROP_CRC4	0x02	/* CRC4 failure in submultiframe */
#define ftPROP_MFAS	0x04	/* MFAS failure in multiframe */
#define ftPROP_TS31	0x08	/* simulate BIT2 (FAS in TS 0) and FAS (BIT2 in TS0) in TS 31 */
#define ftPROP_START	0x10	/* Start test */
#define ftPROP_AIS	0x80	/* AIS */

struct fr_data {
	int count;
	struct fr_flatdesc *desc;
	size_t data_size;
	uint8_t *data;
};

static struct fr_data *TestData;

struct fr_cdesc preamble[] = {
	{ftAIS, ftPROP_AIS, 1, 16000},
	{ftSMFA, ftPROP_NONE, 8, 1100},
	{ftCtrl_Stop, ftPROP_NONE, 0, 0}
};

struct fr_cdesc test0[] = {
	{ftSMFA, ftPROP_NONE, 8, 10000},
	{ftCtrl_Stop, ftPROP_NONE, 0, 0}
};

struct fr_cdesc test1[] = {
	{ftSMFA, ftPROP_NONE, 8, 1500},
	{ftCtrl_Start, ftPROP_START, 0, 0},
	{ftSMFA, ftPROP_NONE, 8, 500},
	{ftSMFB, ftPROP_CRC4, 8, 1},
	{ftSMFA, ftPROP_NONE, 8, 1000},
	{ftSMFB, ftPROP_CRC4, 8, 2},
	{ftSMFA, ftPROP_NONE, 8, 1500},
	{ftSMFB, ftPROP_CRC4, 8, 914},
	{ftSMFA, ftPROP_NONE, 8, 86},
	{ftSMFB, ftPROP_CRC4, 8, 914},
	{ftSMFA, ftPROP_NONE, 8, 2000},
	{ftSMFB, ftPROP_CRC4, 8, 915},
	{ftSMFA, ftPROP_NONE, 8, 85},
	{ftSMFB, ftPROP_CRC4, 8, 915},
	{ftSMFA, ftPROP_NONE, 8, 1500},
	{ftCtrl_Repeat, ftPROP_NONE, 0, 4000},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},
	{ftCtrl_Stop, ftPROP_NONE, 0, 0}
};

struct fr_cdesc test2[] = {
	{ftCtrl_Repeat, ftPROP_NONE, 0, 9000},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Start, ftPROP_START, 0, 0},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 1000},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 20},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftBIT2, ftPROP_FAIL, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 200},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_NONE, 1, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFRAME_B, ftPROP_TS31, 2, 16000},
	{ftFRAME_C, ftPROP_TS31, 2, 6},
	{ftFRAME_B, ftPROP_TS31, 2, 16000},

	{ftCtrl_Stop, ftPROP_NONE, 0, 0}
};

struct fr_cdesc test3[] = {
	{ftFRAME_B, ftPROP_TS31, 2, 15000},

	{ftCtrl_Start, ftPROP_START, 0, 0},

	{ftFRAME_B, ftPROP_TS31, 2, 1000},

	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 4},
	{ftMFA, ftPROP_NONE, 16, 1},

	{ftMFB, ftPROP_NONE, 16, 37},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 251},

	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftMFB, ftPROP_NONE, 16, 250},

	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 4},

	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 2},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 2},
	{ftMFA, ftPROP_NONE, 16, 1},

	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 2},
	{ftMFA, ftPROP_NONE, 16, 2},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 500},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Stop, ftPROP_NONE, 0, 0}
};

struct fr_cdesc test4[] = {
	{ftFRAME_B, ftPROP_TS31, 2, 16000},

	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 4},
	{ftMFA, ftPROP_NONE, 16, 1},

	{ftMFB, ftPROP_NONE, 16, 37},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 251},

	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftFAS, ftPROP_FAIL, 1, 1},
	{ftBIT2, ftPROP_NONE, 1, 1},
	{ftMFB, ftPROP_NONE, 16, 250},

	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 4},

	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 2},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 2},
	{ftMFA, ftPROP_NONE, 16, 1},

	{ftMFA, ftPROP_NONE, 16, 1},
	{ftMFB, ftPROP_NONE, 16, 2},
	{ftMFA, ftPROP_NONE, 16, 2},

	{ftCtrl_Repeat, ftPROP_NONE, 0, 500},
	{ftMFB, ftPROP_NONE, 16, 1},
	{ftMFA, ftPROP_NONE, 16, 1},
	{ftCtrl_End, ftPROP_NONE, 0, 0},

	{ftCtrl_Stop, ftPROP_NONE, 0, 0}
};

static uint8_t mfr_sync;
static uint8_t startcnt = MFR_SYNC_BITS;

static uint8_t *inbuf, *ib_p, *ib_end;
static int ib_size, ib_pos;
static uint8_t *outbuf, *ob_p, *ob_end;
static int ob_size, ob_pos;

enum FSyncState {
	FSync_None,
	FSync_FAS,
	FSync_NFAS,
	FSync_AIS,
	FSync_LOS
};
enum MFRSyncState {
	MFR_State_NotSync,
	MFR_State_Sync
};

static enum FSyncState fsync_state = FSync_None;
static enum MFRSyncState mfr_state = MFR_State_NotSync;

static uint8_t *last_mfrs;

static int good_mfr, bad_mfr;

static int debuglevel = 0;
static int RawReadMode = 0;
static int cardnr = 0;
static char *WriteFileName = NULL;
static int ListMode = 0;
struct fr_cdesc *Test = test0;
static void usage(char *pname)
{
	fprintf(stderr, "Call with %s [options]\n", pname);
	fprintf(stderr, "\n");
	fprintf(stderr, "\n     Valid options are:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  --help    -?              Usage ; printout this information\n");
	fprintf(stderr, "  --card   -c <number>      use card number # (default 0)\n");
	fprintf(stderr, "  --flat   -f               list flat test frame description\n");
	fprintf(stderr, "  --list   -l               list test frame description\n");
	fprintf(stderr, "  --raw    -r               rawread only mode\n");
	fprintf(stderr, "  --debug  -d <level>       debuglevel\n");
	fprintf(stderr, "  --test   -t <test #>      generate data for test # (default 0)\n");
	fprintf(stderr, "  --write  -w <file>        write <file>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Tests:\n");
	fprintf(stderr, "      0  - 10 seconds normal CRC4 framing\n");
	fprintf(stderr, "      1  - TBR4 B.4.2 (table B.1)\n");
	fprintf(stderr, "      2  - TBR4 B.5.2 (table B.2)\n");
	fprintf(stderr, "      3  - TBR4 B.5.3 (table B.3)\n");
	fprintf(stderr, "\n");
}

static int opt_parse(int ac, char *av[])
{
	int c;

	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, '?'},
			{"card", 1, 0, 'c'},
			{"debug", 1, 0, 'd'},
			{"flat", 0, 0, 'f'},
			{"list", 0, 0, 'l'},
			{"raw", 0, 0, 'r'},
			{"test", 1, 0, 't'},
			{"write", 1, 0, 'w'},
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "?c:d:flrt:w:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			fprintf(stderr, "option %s", long_options[option_index].name);
			if (optarg)
				fprintf(stderr, " with arg %s", optarg);
			fprintf(stderr, "\n");
			break;
		case 'c':
			if (optarg)
				cardnr = atoi(optarg);
			else {
				fprintf(stderr, "option -c but no card number\n");
				return -2;
			}
			break;
		case 'w':
			if (optarg)
				WriteFileName = strdup(optarg);
			else {
				fprintf(stderr, "option -w but no filename\n");
				return -2;
			}
			break;
		case 'd':
			if (optarg) {
				debuglevel = atoi(optarg);
			} else {
				fprintf(stderr, "option -d but no value for debug level\n");
				return -3;
			}
			break;
		case 'l':
			ListMode = 1;
			break;
		case 'f':
			ListMode = 2;
			break;
		case 'r':
			RawReadMode = 1;
			break;
		case 't':
			if (optarg) {
				switch (*optarg) {
				case '0':
					Test = test0;
					break;
				case '1':
					Test = test1;
					break;
				case '2':
					Test = test2;
					break;
				case '3':
					Test = test3;
					break;
				default:
					fprintf(stderr, "Unknown test %s\n", optarg);
					return -4;
				}
			} else {
				fprintf(stderr, "option -t but no value for test\n");
				return -3;
			}
			break;
		case '?':
			usage(av[0]);
			return -1;
		}
	}
	c = ac - optind;
	if (c != 0) {
		fprintf(stderr, "unknown options: %s\n", av[optind]);
		return -2;
	}
	return 0;
}

static void printbinary(char *name, uint32_t val, uint8_t bits)
{
	uint32_t m;

	if (bits > 32)
		bits = 32;

	printf("%s: ", name);

	m = 1 << (bits - 1);
	while (m) {
		printf("%c", val & m ? '1' : '0');
		m >>= 1;
	}
}

static void printhex(unsigned char *p, uint16_t idx, int len, int head)
{
	int i, j;

	for (i = 1; i <= len; i++) {
		printf(" %02x", p[idx++]);
		if ((i != len) && !(i % 4) && (i % 16))
			printf(" ");
		if ((i != len) && !(i % 16)) {
			printf("\n");
			for (j = 0; j < head; j++)
				printf(" ");
		}
	}
	printf("\n");
}

/* table for bit swap */
uint8_t bitswap_tbl[256];

/* table for CRC4 (x^4 + x + 1, Generator poly 0x13) */
uint8_t crc4_tbl[16] = { 0x0, 0x3, 0x6, 0x5, 0xc, 0xf, 0xa, 0x9, 0xb, 0x8, 0xd, 0xe, 0x7, 0x4, 0x1, 0x2 };

void calc_bitswap_table(void)
{
	uint8_t v = 0;

	do {
		bitswap_tbl[v] = (((v & 0x01) << 7) | ((v & 0x02) << 5) | ((v & 0x04) << 3) | ((v & 0x08) << 1) |
				  ((v & 0x10) >> 1) | ((v & 0x20) >> 3) | ((v & 0x40) >> 5) | ((v & 0x80) >> 7));
		v++;
	} while (v != 0);
}

static uint8_t calc_crc4(uint8_t * data, uint16_t start, int len)
{
	uint16_t i, idx = start;
	uint8_t b, crc4 = 0;

	for (i = 0; i < len; i++) {
		b = bitswap_tbl[data[idx]];
		crc4 ^= ((b >> 4) & 0xf);
		crc4 = crc4_tbl[crc4];
		crc4 ^= (b & 0xf);
		crc4 = crc4_tbl[crc4];
		idx++;
	}
	return crc4;
};

static uint8_t calc_and_set_crc4_smf(uint8_t * smf, uint8_t crc)
{
	uint16_t i;
	uint8_t d, b, crc4 = 0;

	for (i = 0; i < SMFR_SIZE; i++) {
		d = smf[i];
		if (!(i % 64)) {	/* Cx bit */
			d &= 0xfe;
			b = bitswap_tbl[d];
			if (crc != 0xff) {	/* no set if ff */
				if (crc & 8)
					d |= 1;
				crc <<= 1;
				smf[i] = d;
			}
		} else
			b = bitswap_tbl[d];
		crc4 ^= ((b >> 4) & 0xf);
		crc4 = crc4_tbl[crc4];
		crc4 ^= (b & 0xf);
		crc4 = crc4_tbl[crc4];
	}
	return crc4 & 0xf;
}

#ifdef NOT_USED_YET
static uint8_t get_crc4_smf(uint8_t * smf)
{
	uint16_t i;
	uint8_t crc4 = 0;

	for (i = 0; i < 4; i++) {
		crc4 <<= 1;
		crc4 |= (smf[64 * i] & 1);
	}
	return crc4;
}
#endif

static int cur_Err = 0;		// cur_RAI = 0;
static int analyse_mfr(uint8_t * p)
{
	uint16_t i, t0idx;
	uint8_t d, crcfr[16];
	uint8_t A = 0;
	uint8_t E = 0;
	uint8_t C = 0;
	uint8_t cr0, cr1;
	const char *stim;
	struct fr_flatdesc *dsc;

	size_t ipos;

	ob_pos = ob_p - outbuf;
	ipos = p - inbuf;
	if (ipos < ((MFR_SYNC_OFFSET + 12) * FRAME_SIZE)) {
		stim = "VOID";
	} else {
		ipos -= ((MFR_SYNC_OFFSET + 12) * FRAME_SIZE);
		ipos /= FRAME_SIZE;
		dsc = &TestData->desc[ipos];
		switch (dsc->otype) {
		case ftAIS:
			stim = "AIS";
			break;
			/* Basic Frames */
		case ftFAS:
			if (dsc->prop & ftPROP_FAIL)
				stim = "/FAS";
			else
				stim = "FAS";
			break;
		case ftBIT2:
			if (dsc->prop & ftPROP_FAIL)
				stim = "/BIT 2";
			else
				stim = "BIT 2";
			break;
			/* Sub multi frames */
		case ftSMFA:
			stim = "SMF A";
			break;
		case ftSMFB:
			stim = "SMF B";
			break;
			/* Multi frames */
		case ftMFA:
			stim = "MF A";
			break;
		case ftMFB:
			stim = "MF B";
			break;
			/* Frames */
		case ftFRAME_A:
			stim = "FRAME A";
			break;
		case ftFRAME_B:
			stim = "FRAME B";
			break;
		case ftFRAME_C:
			stim = "FRAME C";
			break;
		default:
			stim = "VOID";
			break;
		}
	}

	printf("Time:%6d ms {TX: %-7s} MFR %5d Error %3d ", ob_pos / DATA_SIZE_1MS, stim, good_mfr, bad_mfr);
	last_mfrs = p;
	p -= MFR_SYNC_OFFSET * FRAME_SIZE;
	for (i = 0; i < 16; i++) {
		t0idx = i * 32;
		d = p[t0idx];
		crcfr[i] = d;
		if (i & 1) {
			A <<= 1;
			if (d & 0x04)
				A |= 1;
			if (i >= 13) {
				E <<= 1;
				if (d & 0x01)
					E |= 1;
			}
		} else {
			C <<= 1;
			C |= (0x1 & d);
		}
	}
	cr0 = calc_and_set_crc4_smf(p, 0xff);
	cr1 = calc_and_set_crc4_smf(p + 256, 0xff);

	switch (E) {
	case 0:
		cur_Err += 2;
		break;
	case 1:
		cur_Err++;
		break;
	case 2:
		cur_Err = 1;
		break;
	case 3:
		cur_Err = 0;
		break;
	}
	printbinary("A", A, 8);
	printbinary(" C", C, 8);
	printbinary(" CR0", cr0, 4);
	printbinary(" CR1", cr1, 4);
	printbinary(" E", E, 2);
	printf(" Cerr %4d", cur_Err);
	printf("    ");
	printhex(crcfr, 0, 16, 0);
	if (E == 1)
		cur_Err = 0;
	return 0;
}

/*
 * Write one multiframe 16 * 2048 bit ; 512 bytes
 * returns the CRC4 for the next sub multiframe
 */
uint8_t fill_outframe(uint8_t * of, uint8_t A, uint8_t E, uint8_t S, uint8_t F, uint8_t cr0, uint8_t MFRSW, uint8_t * dch,
		      uint8_t def)
{
	uint16_t i, tidx;
	static unsigned char p;
	uint8_t c0, c1, j;

	tidx = 0;
	for (i = 0; i < 16; i++) {
		if (i & 1) {	/* NFAS */
			p = 0xf8 & S;
			if (A & 0x80)
				p |= 4;
			A <<= 1;
			p |= 2;
			if (i >= 13) {
				if (E & 0x2)
					p |= 1;
				E <<= 1;
			} else {
				if (MFRSW & 0x80)
					p |= 1;
				MFRSW <<= 1;
			}
		} else {
			p = F & 0xfe;
		}
		of[tidx] = p;
		tidx++;
		for (j = 1; j < 32; j++) {
			if (j == 16) {	/* D channel */
				of[tidx] = *dch;
				dch++;
			} else
				of[tidx] = def;
			tidx++;
		}
	}
	c0 = calc_crc4(of, 0, 256);
	c1 = calc_crc4(of, 256, 256);
	for (i = 0; i < 16; i += 2) {
		tidx = 32 * i;
		p = of[tidx];
		if (i < 8) {
			if (cr0 & 0x8)
				p |= 1;
			cr0 <<= 1;
		} else {
			if (c0 & 0x8)
				p |= 1;
			c0 <<= 1;
		}
		of[tidx] = p;
	}
	return c1;
}

static uint8_t *ib_cp;

static int process_data(int cnt)
{
	uint16_t dist, nidx;
	uint8_t i;
	static uint8_t fr[5];
	static enum FSyncState frT[5];

	if (debuglevel == 2)
		printf("Start %d  %p/%p ", cnt, ib_cp, ib_p);
	while (1) {
		dist = ib_p - ib_cp;
		if (dist <= 400)
			break;
		switch (fsync_state) {
		case FSync_None:
		case FSync_AIS:
		case FSync_LOS:
			/* hunt for basic sync */
			fr[0] = ib_cp[0];
			if ((fr[0] & FR_FAS_MASK) != FR_FAS_VAL) {
				ib_cp++;
				continue;
			} else {
				frT[0] = FSync_FAS;
				if (debuglevel == 2)
					printf("F");
				nidx = 32;
				fr[1] = ib_cp[nidx];
				if (!(fr[1] & 0x2)) {
					ib_cp++;
					continue;
				} else
					frT[1] = FSync_NFAS;
				if (debuglevel == 2)
					printf("N");
				nidx = 64;
				fr[2] = ib_cp[nidx];
				if ((fr[2] & FR_FAS_MASK) != FR_FAS_VAL) {
					ib_cp++;
					continue;
				} else
					frT[2] = FSync_FAS;
				if (debuglevel == 2)
					printf("f %p/%d", ib_cp, dist);
				for (i = 3; i < 5; i++) {
					nidx = i * 32;
					fr[i] = ib_cp[nidx];
					if ((fr[i] & FR_FAS_MASK) == FR_FAS_VAL) {
						frT[i] = FSync_FAS;
					} else if (fr[i] & 0x2) {
						frT[i] = FSync_NFAS;
					} else
						frT[i] = FSync_None;
				}
			}
			break;
		default:
			for (i = 0; i < 4; i++) {
				fr[i] = fr[i + 1];
				frT[i] = frT[i + 1];
			}
			/* i = 4 */
			nidx = i * 32;
			fr[i] = ib_cp[nidx];
			if ((fr[i] & FR_FAS_MASK) == FR_FAS_VAL) {
				frT[i] = FSync_FAS;
			} else if (fr[i] & 0x2) {
				frT[i] = FSync_NFAS;
			} else
				frT[i] = FSync_None;
			break;
		}
		switch (frT[0]) {
		case FSync_FAS:
			switch (fsync_state) {
			case FSync_None:
			case FSync_AIS:
			case FSync_LOS:
				fsync_state = FSync_FAS;
				startcnt = MFR_SYNC_BITS;
				mfr_sync = 0;
				break;
			case FSync_NFAS:
				fsync_state = FSync_FAS;
				break;
			case FSync_FAS:
				if ((frT[2] != FSync_NFAS) && (frT[4] != FSync_NFAS)) {
					/* 3 wrong NFAS received - sync lost */
					fsync_state = FSync_LOS;
				} else {
					/* assume we did receive a NFAS, with wrong Bit 2 */
					fsync_state = FSync_NFAS;
				}
				break;
			}
			break;
		case FSync_NFAS:
			switch (fsync_state) {
			case FSync_None:
			case FSync_AIS:
			case FSync_LOS:
				/* cannot happen, since sa new sync always starts with FAS */
				fprintf(stderr, "Line %d: Wrong state %d while processing NFAS\n", __LINE__, fsync_state);
				exit(1);
				break;
			case FSync_FAS:
				fsync_state = FSync_NFAS;
				break;
			case FSync_NFAS:
				if ((frT[2] != FSync_FAS) && (frT[4] != FSync_FAS)) {
					/* 3 wrong FAS received - sync lost */
					fsync_state = FSync_LOS;
				} else {
					/* assume we did receive a wrong coded FAS */
					fsync_state = FSync_FAS;
				}
				break;
			}
			break;
		case FSync_None:
			switch (fsync_state) {
			case FSync_None:
			case FSync_AIS:
			case FSync_LOS:
				/* cannot happen, since a new sync always starts with FAS */
				fprintf(stderr, "Line %d: Wrong state %d\n", __LINE__, fsync_state);
				exit(1);
				break;
			case FSync_FAS:
				if ((frT[2] != FSync_NFAS) && (frT[4] != FSync_NFAS)) {
					/* 3 wrong NFAS received - sync lost */
					fsync_state = FSync_LOS;
				} else {
					/* assume we did receive a NFAS, with wrong Bit 2 */
					fsync_state = FSync_NFAS;
				}
				break;
			case FSync_NFAS:
				if ((frT[2] != FSync_FAS) && (frT[4] != FSync_FAS)) {
					/* 3 wrong FAS received - sync lost */
					fsync_state = FSync_LOS;
				} else {
					/* assume we did receive a wrong coded FAS */
					fsync_state = FSync_FAS;
				}
				break;
			}
			break;
		default:	/* not possible here */
			fprintf(stderr, "Line %d: Wrong state %d\n", __LINE__, frT[0]);
			exit(1);
			break;
		}
		/* check for multi frame sync now */
		switch (fsync_state) {
		case FSync_LOS:
			/* start new sync hunting */
			continue;
		case FSync_NFAS:
			mfr_sync <<= 1;
			mfr_sync |= (fr[0] & MFR_SYNC_BIT);
			if (startcnt == 0) {
				dist = ib_cp - last_mfrs;
				if ((mfr_sync & MFR_SYNC_MASK) == MFR_SYNC_VALUE) {
					/* sync found */
					if (mfr_state == MFR_State_NotSync) {
						mfr_state = MFR_State_Sync;
						good_mfr = 1;
						analyse_mfr(ib_cp);
					} else if (dist == 16 * 32) {
						good_mfr++;
						analyse_mfr(ib_cp);
					}
				} else {
					if (mfr_state == MFR_State_Sync) {
						if (dist == 16 * 32) {
							bad_mfr++;
						} else if (dist == 32 * 32) {
							mfr_state = MFR_State_NotSync;
						}
					}
				}
			} else
				startcnt--;
			break;
		case FSync_FAS:
			break;
		default:
			fprintf(stderr, "Wrong state %d\n", fsync_state);
			exit(1);
			break;
		}
		ib_cp += 32;
	}
	if (debuglevel == 2)
		printf("\n");
	return 0;
}

static int fill_buffer(unsigned char *p, int len)
{

	ib_pos = ib_p - inbuf;
	if ((ib_pos + len) >= ib_size)
		len = ib_size - ib_pos;
	memcpy(ib_p, p, len);
	ib_p += len;
	return len;
}

/* returns next start frame */
static uint8_t *fill_timeslot(uint8_t * p0, int ts, uint8_t * data, int datalen, int repeat)
{
	uint8_t *p = p0;
	int i, cnt = repeat;

	while (cnt) {
		for (i = 0; i < datalen; i++) {
			p[ts] = data[i];
			p += FRAME_SIZE;
		}
		cnt--;
	}
	return p;
}

uint8_t *fill_timeslot_ts0_smf(uint8_t * p0, int ts, int smf, uint8_t fas_err, uint8_t nfas_err, uint8_t mfsw,
			       uint8_t S, uint8_t A, uint8_t C, uint8_t E, int repeat)
{
	uint8_t *p = p0;
	uint8_t i, d[8], a, c, e, n, f, m;

	while (repeat) {
		e = E;
		if (smf & 1) {
			a = A & 0xf;
			c = C & 0xf;
			n = nfas_err & 0xf;
			f = fas_err & 0xf;
			m = mfsw & 0xf;
		} else {
			a = A >> 4;
			c = C >> 4;
			n = nfas_err >> 4;
			f = fas_err >> 4;
			m = mfsw >> 4;
		}
		for (i = 0; i < 8; i++) {
			if (i & 1) {	/* NFAS */
				if (n & 8)	/* nfas error */
					d[i] = 0;
				else
					d[i] = 2;
				n <<= 1;
				if (i > 4 && (smf & 1)) {
					if (e & 2)
						d[i] |= 1;
					e <<= 1;
				} else {
					if (m & 8)
						d[i] |= 1;
					m <<= 1;
				}
				d[i] |= (S & 0xF8);
				if (a & 8)
					d[i] |= 4;
				a <<= 1;
			} else {	/* FAS */
				if (f & 8)	/* FAS error */
					d[i] = 0xc8;
				else
					d[i] = FR_FAS_VAL;
				f <<= 1;
				if (c & 8)
					d[i] |= 1;
				c <<= 1;
			}
		}
		p = fill_timeslot(p, ts, d, 8, 1);
		smf++;
		repeat--;
	}
	return p;
}

static uint8_t *calc_and_set_crc4_ts(uint8_t * start, uint8_t ts, uint8_t * first_last_crc, int smf_count)
{
	uint8_t crc = *first_last_crc & 0xf;
	uint8_t wrong_crc = *first_last_crc & 0xf0;
	uint8_t *p = start;

	while (smf_count) {
		if (wrong_crc)
			crc = crc + 2;
		crc = calc_and_set_crc4_smf(&p[ts], crc & 0xf);
		p += SMFR_SIZE;
		smf_count--;
	}
	*first_last_crc = crc;
	return p;
}

static int transmit(int sock, uint8_t * ob, uint16_t len)
{
	struct msghdr mh;
	struct iovec iov[2];
	int ret;
	struct mISDNhead hh = { PH_DATA_REQ, 1 };

	mh.msg_name = NULL;
	mh.msg_namelen = 0;
	mh.msg_iov = iov;
	mh.msg_iovlen = 2;
	mh.msg_control = NULL;
	mh.msg_controllen = 0;
	mh.msg_flags = 0;
	iov[0].iov_base = &hh;
	iov[0].iov_len = MISDN_HEADER_LEN;
	iov[1].iov_base = ob;
	iov[1].iov_len = len;
	ret = sendmsg(sock, &mh, 0);
	if (ret != (len + MISDN_HEADER_LEN)) {
		fprintf(stderr, "Send error %d (%d + %d) - %s\n", ret, (int)MISDN_HEADER_LEN, len, strerror(errno));
		ret = 0;
	} else
		ret = len;
	return ret;
}

#define MAX_RECV_BUFFER_SIZE	4200
static int start_transmit = 1;
static int last_dlen = 64;

static int receive_ts0dch(int socket)
{
	int ret, cnt, head;
	uint8_t buffer[MAX_RECV_BUFFER_SIZE];
	struct mISDNhead *hh = (struct mISDNhead *)buffer;

	ret = recv(socket, buffer, MAX_RECV_BUFFER_SIZE, 0);
	if (ret < 0) {
		fprintf(stderr, "read error %s\n", strerror(errno));
		return ret;
	}
#if 0
	if (cts.cmsg_type == MISDN_TIME_STAMP) {
		mt = localtime((time_t *) & cts.tv.tv_sec);
		head = printf("%02d.%02d.%04d %02d:%02d:%02d.%06ld", mt->tm_mday, mt->tm_mon + 1, mt->tm_year + 1900,
			      mt->tm_hour, mt->tm_min, mt->tm_sec, cts.tv.tv_usec);
	} else {
		cts.tv.tv_sec = 0;
		cts.tv.tv_usec = 0;
	}
#endif
	head = 0;
	switch (hh->prim) {
	case PH_DATA_IND:
		last_dlen = ret - MISDN_HEADER_LEN;
		if (!RawReadMode) {
			if (start_transmit && RawReadMode == 0) {
				ob_p += transmit(socket, ob_p, last_dlen);
				start_transmit = 0;
			}
			cnt = fill_buffer(&buffer[MISDN_HEADER_LEN], last_dlen);
			process_data(cnt);
			if ((mfr_state == MFR_State_NotSync && debuglevel == 3) || debuglevel == 4) {
				head = printf("Got %3d bytes ", last_dlen);
				printhex(buffer, MISDN_HEADER_LEN, cnt, head);
			}
		} else {
			head = printf("Got %3d bytes ", last_dlen);
			printhex(buffer, MISDN_HEADER_LEN, last_dlen, head);
		}
		break;
	case PH_DATA_CNF:
		if (!RawReadMode) {
			ob_pos = ob_p - outbuf;
			if ((ob_pos + last_dlen) >= ob_size) {
				ret = -1;	/* stop */
				last_dlen = ob_size - ob_pos;
			}
			if (debuglevel == 5) {
				head = printf("Send %3d bytes ", last_dlen);
				printhex(ob_p, 0, last_dlen, head);
			}
			ob_p += transmit(socket, ob_p, last_dlen);
		}
		break;
	}
	return ret;
}

/* test description sub routines */
static const char *printhead[] = {
	"",
	"    ",
	"        ",
	"            ",
	"                ",
	"                    ",
	"                        ",
	"                            ",
	"                                "
};

static void print_testdescription(struct fr_cdesc *arg)
{
	struct fr_cdesc *dsc = arg;
	int head = 0;

	while (dsc) {
		if (dsc->type == ftNone) {
			fprintf(stdout, "unexpected end of description\n");
			break;
		} else if (dsc->type == ftCtrl_Start) {
			fprintf(stdout, "Start of test sequence\n");
		} else if (dsc->type == ftCtrl_Stop) {
			fprintf(stdout, "End of test\n");
			break;
		} else if (dsc->type == ftCtrl_Repeat) {
			fprintf(stdout, "%sdo %d times {\n", printhead[head], dsc->count);
			head++;
		} else if (dsc->type == ftCtrl_End) {
			head--;
			fprintf(stdout, "%s}\n", printhead[head]);
		} else {
			fprintf(stdout, "%s%4d ", printhead[head], dsc->count);
			switch (dsc->type) {
			case ftAIS:
				fprintf(stdout, "%-8s", "AIS");
				break;
				/* Basic Frames */
			case ftFAS:
				if (dsc->prop & ftPROP_FAIL)
					fprintf(stdout, "%-8s", "/FAS");
				else
					fprintf(stdout, "%-8s", "FAS");
				break;
			case ftBIT2:
				if (dsc->prop & ftPROP_FAIL)
					fprintf(stdout, "%-8s", "/BIT 2");
				else
					fprintf(stdout, "%-8s", "BIT 2");
				break;
				/* Sub multi frames */
			case ftSMFA:
				fprintf(stdout, "%-8s", "SMF A");
				break;
			case ftSMFB:
				fprintf(stdout, "%-8s", "SMF B");
				break;
				/* Multi frames */
			case ftMFA:
				fprintf(stdout, "%-8s", "MF A");
				break;
			case ftMFB:
				fprintf(stdout, "%-8s", "MF B");
				break;
				/* Frames */
			case ftFRAME_A:
				fprintf(stdout, "%-8s", "FRAME A");
				break;
			case ftFRAME_B:
				fprintf(stdout, "%-8s", "FRAME B");
				break;
			case ftFRAME_C:
				fprintf(stdout, "%-8s", "FRAME C");
				break;
			default:
				fprintf(stdout, "%-8s", "UNKNOWN");
				break;
			}
			if (dsc->prop & ftPROP_CRC4)
				fprintf(stdout, " (CRC4 Error)");
			if (dsc->prop & ftPROP_MFAS)
				fprintf(stdout, " (MFAS Error)");
			if (dsc->prop & ftPROP_TS31)
				fprintf(stdout, " (TS 31)");
			if (dsc->prop & ftPROP_START)
				fprintf(stdout, " (Start Test)");
			fprintf(stdout, "\n");
		}
		dsc++;
	}
}

static void print_flatdescription(struct fr_flatdesc *dsc)
{
	fprintf(stdout, "Start test\n");
	while (dsc) {
		if (dsc->type == ftNone) {
			fprintf(stdout, "unexpected end of description\n");
			break;
		} else if (dsc->type == ftCtrl_Stop) {
			fprintf(stdout, "End of test\n");
			break;
		} else if (dsc->type == ftAIS) {
			fprintf(stdout, "%7d %2d %-6s", dsc->pos, dsc->mf_pos, "AIS");
		} else if (dsc->type == ftFAS) {
			if (dsc->prop & ftPROP_FAIL)
				fprintf(stdout, "%7d %2d %-6s", dsc->pos, dsc->mf_pos, "/FAS");
			else
				fprintf(stdout, "%7d %2d %-6s", dsc->pos, dsc->mf_pos, "FAS");
		} else if (dsc->type == ftBIT2) {
			if (dsc->prop & ftPROP_FAIL)
				fprintf(stdout, "%7d %2d %-6s", dsc->pos, dsc->mf_pos, "/BIT 2");
			else
				fprintf(stdout, "%7d %2d %-6s", dsc->pos, dsc->mf_pos, "BIT 2");
		}
		switch (dsc->otype) {
		case ftAIS:
			fprintf(stdout, "[%-7s]", "AIS");
			break;
			/* Basic Frames */
		case ftFAS:
			if (dsc->prop & ftPROP_FAIL)
				fprintf(stdout, "[%-7s]", "/FAS");
			else
				fprintf(stdout, "[%-7s]", "FAS");
			break;
		case ftBIT2:
			if (dsc->prop & ftPROP_FAIL)
				fprintf(stdout, "[%-7s]", "/BIT 2");
			else
				fprintf(stdout, "[%-7s]", "BIT 2");
			break;
			/* Sub multi frames */
		case ftSMFA:
			fprintf(stdout, "[%-7s]", "SMF A");
			break;
		case ftSMFB:
			fprintf(stdout, "[%-7s]", "SMF B");
			break;
			/* Multi frames */
		case ftMFA:
			fprintf(stdout, "[%-7s]", "MF A");
			break;
		case ftMFB:
			fprintf(stdout, "[%-7s]", "MF B");
			break;
			/* Frames */
		case ftFRAME_A:
			fprintf(stdout, "[%-7s]", "FRAME A");
			break;
		case ftFRAME_B:
			fprintf(stdout, "[%-7s]", "FRAME B");
			break;
		case ftFRAME_C:
			fprintf(stdout, "[%-7s]", "FRAME C");
			break;
		default:
			fprintf(stdout, "[%-7s]", "VOID");
			break;
		}
		if (dsc->prop & ftPROP_CRC4)
			fprintf(stdout, " (CRC4 Error)");
		if (dsc->prop & ftPROP_MFAS)
			fprintf(stdout, " (MFAS Error)");
		if (dsc->prop & ftPROP_TS31)
			fprintf(stdout, " (TS 31)");
		if (dsc->prop & ftPROP_START)
			fprintf(stdout, " (Start Test)");
		fprintf(stdout, "\n");
		dsc++;
	}
}

static int calc_flatcount(struct fr_cdesc *arg)
{
	struct fr_cdesc *dsc = arg;
	int count[8], repeat[8];
	int cnt, csp = 0;

	count[0] = 0;
	while (dsc) {
		if (dsc->type == ftNone) {
			fprintf(stderr, "unexpected end of description\n");
			csp = 0;
			count[0] = -1;
			break;
		}
		if (dsc->type == ftCtrl_Stop)
			break;
		if (dsc->type == ftCtrl_Start) {
			/* nothing to do */
		} else if (dsc->type == ftCtrl_Repeat) {
			csp++;
			if (csp >= 8) {
				fprintf(stderr, "To deep nesting of loops\n");
				csp = 0;
				count[0] = -1;
				break;
			}
			repeat[csp] = dsc->count;
			count[csp] = 0;
		} else if (dsc->type == ftCtrl_End) {
			if (csp < 1) {
				fprintf(stderr, "End of description, but no Repeatblock\n");
				csp = 0;
				count[0] = -1;
				break;
			}
			cnt = repeat[csp] * count[csp];
			csp--;
			count[csp] += cnt;
		} else {
			count[csp] += (dsc->subcnt * dsc->count);
		}
		dsc++;
	}
	if (csp != 0) {
		fprintf(stderr, "End of description, but repeat level %d (not 0)\n", csp);
		csp = 0;
		count[0] = -1;
	}
	return count[0];
}

static struct fr_flatdesc *make_flat_description(struct fr_cdesc *test, struct fr_flatdesc *begin)
{
	struct fr_cdesc *dsc = test;
	struct fr_flatdesc *flat = begin;
	int repeat[8];
	struct fr_flatdesc *seq, *end, *start[8];
	int cnt, i, csp = 0, pos, mfpos;

	pos = flat->pos;
	mfpos = flat->mf_pos;
	while (dsc) {
		if (dsc->type == ftNone) {
			fprintf(stderr, "unexpected end of description\n");
			flat = NULL;
			break;
		}
		if (dsc->type == ftCtrl_Stop) {
			flat->pos = pos;
			flat->type = dsc->type;
			break;
		}
		if (dsc->type == ftCtrl_Start) {
			/* nothing to do */
			flat->prop = ftPROP_START;
		} else if (dsc->type == ftCtrl_Repeat) {
			csp++;
			if (csp >= 8) {
				fprintf(stderr, "To deep nesting of loops\n");
				csp = 0;
				flat = NULL;
				break;
			}
			repeat[csp] = dsc->count;
			start[csp] = flat;
		} else if (dsc->type == ftCtrl_End) {
			if (csp < 1) {
				fprintf(stderr, "End of description, but no Repeatblock\n");
				flat = NULL;
				break;
			}
			end = flat;
			repeat[csp]--;	/* one already done */
			while (repeat[csp]) {
				seq = start[csp];
				while (seq != end) {
					*flat = *seq;
					flat->pos = pos;
					flat->prop &= ~ftPROP_START;
					seq++;
					flat++;
					pos++;
				}
				--repeat[csp];
			}
			csp--;
		} else {
			for (cnt = 0; cnt < dsc->count; cnt++) {
				switch (dsc->type) {
				case ftAIS:
					/* Basic Frames */
				case ftFAS:
				case ftBIT2:
					flat->pos = pos;
					flat->type = dsc->type;
					flat->otype = dsc->type;
					flat->prop |= dsc->prop;
					flat++;
					pos++;
					break;
					/* Sub multi frames */
				case ftSMFA:
					for (i = 0; i < 8; i++) {
						flat->pos = pos;
						flat->otype = dsc->type;
						if (i & 1) {
							flat->type = ftBIT2;
						} else {
							flat->type = ftFAS;
						}
						flat++;
						pos++;
					}
					break;
				case ftSMFB:
					for (i = 0; i < 8; i++) {
						flat->pos = pos;
						flat->otype = dsc->type;
						if (i & 1) {
							flat->type = ftBIT2;
						} else {
							flat->type = ftFAS;
						}
						flat->prop |= dsc->prop;
						flat++;
						pos++;
					}
					break;
					/* Multi frames */
				case ftMFA:
					for (i = 0; i < 16; i++) {
						flat->pos = pos;
						flat->otype = dsc->type;
						if (i & 1) {
							flat->type = ftBIT2;
						} else {
							flat->type = ftFAS;
						}
						flat++;
						pos++;
					}
					break;
				case ftMFB:
					for (i = 0; i < 16; i++) {
						flat->pos = pos;
						flat->otype = dsc->type;
						if (i & 1) {
							flat->type = ftBIT2;
						} else {
							flat->type = ftFAS;
						}
						flat->prop |= ftPROP_MFAS;
						flat++;
						pos++;
					}
					break;
					/* Frames */
				case ftFRAME_A:
					flat->pos = pos;
					flat->otype = dsc->type;
					flat->type = ftFAS;
					flat++;
					pos++;
					flat->pos = pos;
					flat->otype = dsc->type;
					flat->type = ftBIT2;
					flat++;
					pos++;
					break;
				case ftFRAME_B:
					flat->pos = pos;
					flat->otype = dsc->type;
					flat->type = ftFAS;
					flat->prop |= ftPROP_TS31;
					flat++;
					pos++;
					flat->pos = pos;
					flat->otype = dsc->type;
					flat->type = ftBIT2;
					flat->prop |= ftPROP_TS31;
					flat++;
					pos++;
					break;
				case ftFRAME_C:
					flat->pos = pos;
					flat->otype = dsc->type;
					flat->type = ftFAS;
					flat->prop |= ftPROP_FAIL | ftPROP_TS31;
					flat++;
					pos++;
					flat->pos = pos;
					flat->otype = dsc->type;
					flat->type = ftBIT2;
					flat->prop |= ftPROP_TS31;
					flat++;
					pos++;
					break;
				default:
					/* never */
					fprintf(stderr, "Got wrong type %x\n", dsc->type);
					exit(1);
				}
			}
		}
		dsc++;
	}
	if (!flat)
		return flat;
	flat = begin;
	mfpos = flat->mf_pos;
	while (flat) {
		if (flat->type == ftNone) {
			fprintf(stdout, "unexpected end of description\n");
			flat = NULL;
			break;
		} else if (flat->type == ftCtrl_Stop) {
			flat->mf_pos = mfpos;
			break;
		} else if (flat->type == ftAIS) {
			flat->mf_pos = 0xff;
			flat->mf_pos = 14;
		} else if (flat->type == ftFAS) {
			if (mfpos & 1)	/* reset */
				mfpos = 0;
		} else if (flat->type == ftBIT2) {
			if (!(mfpos & 1))
				mfpos = 1;
		}
		flat->mf_pos = mfpos;
		mfpos++;
		if (mfpos > 15)
			mfpos = 0;
		flat++;
	}
	return flat;
}

static struct fr_data *gen_flat_frame_desc(struct fr_cdesc *dsc)
{
	struct fr_data loc_frd, *frd;
	struct fr_flatdesc *flat_dsc;
	int cnt, sum, pre;

	pre = calc_flatcount(preamble);
	cnt = calc_flatcount(dsc);
	sum = pre + cnt;
	loc_frd.count = sum;
	flat_dsc = calloc(sum + 2, sizeof(*flat_dsc));	/* reserve for stop */
	if (!flat_dsc) {
		fprintf(stderr, "No memory to allocate %d * %zd bytes for flat description\n", loc_frd.count, sizeof(*flat_dsc));
		return NULL;
	}
	loc_frd.desc = flat_dsc;
	flat_dsc = make_flat_description(preamble, flat_dsc);
	if (!flat_dsc) {
		fprintf(stderr, "Error while generating flat description of preamble\n");
		free(flat_dsc);
		return NULL;
	}
	flat_dsc = make_flat_description(dsc, flat_dsc);
	if (!flat_dsc) {
		fprintf(stderr, "Error while generating flat description\n");
		free(flat_dsc);
		return NULL;
	}
	frd = malloc(sizeof(*frd));
	if (!frd) {
		fprintf(stderr, "No memory to allocate %zd bytes\n", sizeof(*frd));
		free(flat_dsc);
		return NULL;
	}
	*frd = loc_frd;
	return frd;
}

static int gen_flat_frame_data(struct fr_data *frd)
{
	struct fr_flatdesc *flat;
	size_t size;
	int ret = 0;
	uint8_t crc, *p, dch_flags = 0x7e;

	flat = frd->desc;
	size = frd->count;
	size *= FRAME_SIZE;
	frd->data_size = size;
	frd->data = malloc(size + 100 * FRAME_SIZE);	/* Reserve to avoid crash when manipulate frames on the end */
	if (!frd->data) {
		fprintf(stderr, "No memory to allocate %zd + %d bytes\n", size, 100 * FRAME_SIZE);
		flat = NULL;
		ret = -ENOMEM;
	}
	memset(frd->data, 0xff, size + 100 * FRAME_SIZE);
	p = frd->data;
	fill_timeslot(p, 16, &dch_flags, 1, frd->count);
	while (flat) {
		flat->data = p;
		if (flat->type == ftNone) {
			fprintf(stdout, "unexpected end of description\n");
			free(frd->data);
			frd->data = NULL;
			ret = -EINVAL;
			break;
		} else if (flat->type == ftCtrl_Stop) {
			fprintf(stdout, "End of test\n");
			break;
		} else if (flat->type == ftAIS) {
			memset(p, 0xff, FRAME_SIZE);
		} else if (flat->type == ftFAS) {
			if (flat->prop & ftPROP_FAIL)
				*p = FR_FAS_VAL_FAIL;
			else
				*p = FR_FAS_VAL;
			if (flat->prop & ftPROP_TS31)
				p[31] = FR_NFAS_B2;
		} else if (flat->type == ftBIT2) {
			if (flat->prop & ftPROP_FAIL)
				*p = FR_NFAS_B2_FAIL;
			else
				*p = FR_NFAS_B2;
			if ((flat->mf_pos & 0xf) == 5) {
				if (!(flat->prop & ftPROP_MFAS))
					*p |= 1;
			} else if ((flat->mf_pos & 0xf) > 7) {
				*p |= 1;
			}
			if (flat->prop & ftPROP_TS31)
				p[31] = FR_FAS_VAL;
		}
		p += FRAME_SIZE;
		flat++;
	}
	p = frd->data;
	crc = 0;		/* start value */
	flat = frd->desc;
	while (flat) {
		if (flat->type == ftNone) {
			fprintf(stdout, "unexpected end of description\n");
			free(frd->data);
			frd->data = NULL;
			ret = -EINVAL;
			break;
		} else if (flat->type == ftCtrl_Stop) {
			fprintf(stdout, "End of test\n");
			break;
		} else if (flat->type == ftAIS) {
			crc = 0;
		} else {
			if ((flat->mf_pos & 0x7) == 0) {
				calc_and_set_crc4_ts(flat->data, 0, &crc, 1);
			}
			if ((flat->mf_pos & 0x7) == 2) {
				if (flat->prop & ftPROP_CRC4) {
					/* insert a bit failure in TS 3 (0xFF -> 0xEF) */
					flat->data[3] = 0xef;
				}
			}
		}
		flat++;
	}
	return ret;
}

int main(argc, argv)
int argc;
char *argv[];
{
	int i, channel;
	int log_socket;
	struct sockaddr_mISDN log_addr;
	int buflen = 4104;
	u_char buffer[buflen];
	int result;
	int cnt, dlen;
	struct mISDN_devinfo di;
	struct mISDNhead *hh;
	struct mISDNversion ver;
	struct pollfd pfd[8];
	int pfd_nr;
	struct mISDN_ctrl_req creq;

	result = opt_parse(argc, argv);
	if (result) {
		exit(1);
	}
	calc_bitswap_table();

	cnt = calc_flatcount(preamble);
	fprintf(stdout, "preamble: %d test: %d\n", cnt, calc_flatcount(Test));

	if (ListMode == 1) {
		print_testdescription(Test);
		return 0;
	}
	TestData = gen_flat_frame_desc(Test);
	if (!TestData) {
		fprintf(stdout, "Did not generate flat description\n");
		exit(1);
	}
	if (ListMode == 2) {
		print_flatdescription(TestData->desc);
		return 0;
	}
	result = gen_flat_frame_data(TestData);
	if (result) {
		fprintf(stderr, "Error generating test data\n");
		exit(1);
	}

	if (cardnr < 0) {
		fprintf(stderr, "card nr may not be negative\n");
		exit(1);
	}

	if ((log_socket = socket(PF_ISDN, SOCK_RAW, 0)) < 0) {
		printf("could not open socket %s\n", strerror(errno));
		exit(1);
	}

	result = ioctl(log_socket, IMGETVERSION, &ver);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	}
	if (ver.release & MISDN_GIT_RELEASE)
		printf("mISDN kernel version %d.%02d.%d (git.misdn.eu) found\n", ver.major, ver.minor,
		       ver.release & ~MISDN_GIT_RELEASE);
	else
		printf("mISDN kernel version %d.%02d.%d found\n", ver.major, ver.minor, ver.release);

	printf("mISDN user   version %d.%02d.%d found\n", MISDN_MAJOR_VERSION, MISDN_MINOR_VERSION, MISDN_RELEASE);

	if (ver.major != MISDN_MAJOR_VERSION) {
		printf("VERSION incompatible please update\n");
		exit(1);
	}

	result = ioctl(log_socket, IMGETCOUNT, &cnt);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
		exit(1);
	} else
		printf("%d controller%s found\n", cnt, (cnt == 1) ? "" : "s");

	di.id = cardnr;
	result = ioctl(log_socket, IMGETDEVINFO, &di);
	if (result < 0) {
		printf("ioctl error %s\n", strerror(errno));
	} else {
		printf("	id:		%d\n", di.id);
		printf("	Dprotocols:	%08x\n", di.Dprotocols);
		printf("	Bprotocols:	%08x\n", di.Bprotocols);
		printf("	protocol:	%d\n", di.protocol);
		printf("	channelmap:	");
		for (i = MISDN_CHMAP_SIZE - 1; i >= 0; i--)
			printf("%02x", di.channelmap[i]);
		printf("\n");
		printf("	nrbchan:	%d\n", di.nrbchan);
		printf("	name:		%s\n", di.name);
	}

	close(log_socket);
	outbuf = TestData->data;
	ob_size = TestData->data_size;
	ob_end = outbuf + ob_size;
	ib_size = ob_size;
	ib_size += (5 * MFR_SIZE);
	inbuf = malloc(ib_size + (5 * MFR_SIZE));
	if (!inbuf) {
		fprintf(stderr, "Could not allocate in buffer (%d bytes)\n", ib_size + (5 * MFR_SIZE));
		exit(1);
	} else if (debuglevel)
		printf("Allocated in buffer %d bytes (Reserve:%d)\n", ib_size, 10 * MFR_SIZE);

	ib_p = inbuf;
	ib_cp = inbuf;
	ib_end = outbuf + ib_size;

	/* Ready for transmit */
	ob_p = outbuf;

	if ((log_socket = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_B_RAW)) < 0) {
		printf("could not open log socket %s\n", strerror(errno));
		exit(1);
	}

	log_addr.family = AF_ISDN;
	log_addr.dev = cardnr;

	result = -1;
	channel = 0;

	log_addr.channel = (unsigned char)channel;
	result = bind(log_socket, (struct sockaddr *)&log_addr, sizeof(log_addr));
	printf("log bind ch(%i) return %d\n", log_addr.channel, result);
	if (result < 0) {
		printf("log bind error %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}

	result = fcntl(log_socket, F_SETFL, O_NONBLOCK);
	if (result < 0) {
		printf("log F_SETFL O_NONBLOCK error %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}
	pfd[0].fd = log_socket;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	pfd_nr = 1;

	hh = (struct mISDNhead *)buffer;

	creq.op = MISDN_CTRL_GETOP;
	creq.channel = 0;
	creq.p1 = 0;
	creq.p2 = 0;
	creq.unused = 0;

	result = ioctl(log_socket, IMCTRLREQ, &creq);
	if (result < 0) {
		fprintf(stdout, "Error on MISDN_CTRL_L1_TS0_MODE ioctl - %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}
	if (!(creq.op & MISDN_CTRL_L1_TESTS)) {
		fprintf(stdout, "MISDN_CTRL_L1_TESTS controls are not supported on this card/driver\n");
		close(log_socket);
		exit(1);
	}

	if (debuglevel)
		fprintf(stdout, "MISDN_CTRL_GETOP ioctl supported operations %x\n", creq.op);

	/* This set the register values in the card so, that the TS0  is not in full transparent mode,
	 * so the receiver can syncronize - this allows to get the TS0 data on byte boundaries, so bit shifting all
	 * incoming data is not needed. After we did detect syncron state for 3 times, will will switch off
	 * autosync and put TS0 into full transparent mode */

	creq.op = MISDN_CTRL_L1_TS0_MODE;
	creq.channel = 0;
	creq.p1 = 0x06;		/* R_RX_SL0_CFG0 =  (V_AUTOSYNC | V_AUTO_RECO) */
	creq.p2 = 0x31;		/* R_TX_SL0_CFG1 = (V_TX_MF | V_TX_E | V_INV_E) */
	creq.unused = 0;
	if (debuglevel)
		fprintf(stdout, "L1 TS0  ioctl R_RX_SL0_CFG0=%02x R_TX_SL0_CFG1=%02x\n", creq.p1, creq.p2);
	result = ioctl(log_socket, IMCTRLREQ, &creq);
	if (debuglevel)
		fprintf(stdout, "L1 TS0  ioctl old register values R_RX_SL0_CFG0=%02x R_TX_SL0_CFG1=%02x\n", creq.p1, creq.p2);
	if (result < 0) {
		fprintf(stdout, "Error on MISDN_CTRL_L1_TS0_MODE ioctl - %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}

	hh->prim = PH_ACTIVATE_REQ;
	hh->id = MISDN_ID_ANY;
	result = sendto(log_socket, buffer, MISDN_HEADER_LEN, 0, NULL, 0);

	if (result < 0) {
		fprintf(stdout, "could not send ACTIVATE_REQ %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}

	/* Disable RX for the sync search time */
	creq.op = MISDN_CTRL_RX_OFF;
	creq.channel = 0;
	creq.p1 = 1;
	creq.p2 = 0;
	creq.unused = 0;
	result = ioctl(log_socket, IMCTRLREQ, &creq);
	if (result < 0) {
		fprintf(stdout, "Error on MISDN_CTRL_RX_OFF ioctl - %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}
	if (debuglevel)
		fprintf(stdout, "RX_OFF  result %d ioctl p1=%02x p2=%02x p3=%02x\n", result, creq.p1, creq.p2, creq.unused);

	creq.op = MISDN_CTRL_L1_GET_SYNC_INFO;
	creq.channel = 0;
	cnt = 0;
	/* Wait for sync - this will make sure that we do not need bit shifting incomming data */
	for (i = 0; i < 5000; i++) {
		result = ioctl(log_socket, IMCTRLREQ, &creq);
		if (result < 0) {
			fprintf(stdout, "Error on MISDN_CTRL_L1_GET_SYNC_INFO ioctl - %s\n", strerror(errno));
			close(log_socket);
			exit(1);
		}
		if (debuglevel)
			fprintf(stdout, "L1 GET_SYNC_INFO ioctl p1=%02x p2=%02x p3=%02x\n", creq.p1, creq.p2, creq.unused);
		if ((creq.p1 & 0xff07) == 0x2701) {
			cnt++;
			if (cnt == 3)
				break;
			i--;
		} else
			cnt = 0;
		usleep(100000);
	};

	if (cnt != 3) {
		fprintf(stdout, "L1 ts0 sync state not reached\n");
		close(log_socket);
		exit(1);
	} else
		fprintf(stdout, "L1 ts0 sync state reached (need %d iterations)\n", i);

	/* reenable receive */
	creq.op = MISDN_CTRL_RX_OFF;
	creq.channel = 0;
	creq.p1 = 0;
	creq.p2 = 0;
	creq.unused = 0;
	result = ioctl(log_socket, IMCTRLREQ, &creq);
	if (result < 0) {
		fprintf(stdout, "Error on MISDN_CTRL_RX_OFF ioctl - %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}
	if (debuglevel)
		fprintf(stdout, "RX_OFF  ioctl p1=%02x p2=%02x p3=%02x\n", creq.p1, creq.p2, creq.unused);

	creq.op = MISDN_CTRL_L1_TS0_MODE;
	creq.channel = 0;
	creq.p1 = 0x01;		/* R_RX_SL0_CFG0 = (V_NO_INSYNC)  */
	creq.p2 = 0x03;		/* R_TX_SL0_CFG1 = (V_TX_MF | V_TRP_SL0) */
	creq.unused = 0;
	if (debuglevel)
		fprintf(stdout, "L1 TS0  ioctl R_RX_SL0_CFG0=%02x R_TX_SL0_CFG1=%02x\n", creq.p1, creq.p2);
	result = ioctl(log_socket, IMCTRLREQ, &creq);
	if (debuglevel)
		fprintf(stdout, "L1 TS0  ioctl old register values R_RX_SL0_CFG0=%02x R_TX_SL0_CFG1=%02x\n", creq.p1, creq.p2);
	if (result < 0) {
		fprintf(stdout, "Error on MISDN_CTRL_L1_TS0_MODE ioctl - %s\n", strerror(errno));
		close(log_socket);
		exit(1);
	}

	dlen = 64;
	while (1) {
		result = poll(pfd, pfd_nr, 1000);
		if (result < 0) {
			fprintf(stderr, "Poll error %s\n", strerror(errno));
			continue;
		} else if (result == 0) {
			fprintf(stderr, "Poll timeout restart\n");
			continue;
		}
		if (pfd[0].revents) {
			dlen = receive_ts0dch(log_socket);
			if (dlen < 1)	/* end of data */
				break;
		}
	}
	close(log_socket);
	return 0;
}
