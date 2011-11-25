/*
 * G3 decoding 
 *
 * Written by Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright (C) 2011 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this package for more details.
 */
#ifdef USE_SOFTFAX

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include "m_capi.h"
#include "g3_mh.h"

struct g3_mh_code tr_white[64] = {
	{  0, 0x0ac, 0x0ff,  8, 0},
	{  1, 0x038, 0x03f,  6, 0},
	{  2, 0x00e, 0x00f,  4, 0},
	{  3, 0x001, 0x00f,  4, 0},
	{  4, 0x00d, 0x00f,  4, 0},
	{  5, 0x003, 0x00f,  4, 0},
	{  6, 0x007, 0x00f,  4, 0},
	{  7, 0x00f, 0x00f,  4, 0},
	{  8, 0x019, 0x01f,  5, 0},
	{  9, 0x005, 0x01f,  5, 0},
	{ 10, 0x01c, 0x01f,  5, 0},
	{ 11, 0x002, 0x01f,  5, 0},
	{ 12, 0x004, 0x03f,  6, 0},
	{ 13, 0x030, 0x03f,  6, 0},
	{ 14, 0x00b, 0x03f,  6, 0},
	{ 15, 0x02b, 0x03f,  6, 0},
	{ 16, 0x015, 0x03f,  6, 0},
	{ 17, 0x035, 0x03f,  6, 0},
	{ 18, 0x072, 0x07f,  7, 0},
	{ 19, 0x018, 0x07f,  7, 0},
	{ 20, 0x008, 0x07f,  7, 0},
	{ 21, 0x074, 0x07f,  7, 0},
	{ 22, 0x060, 0x07f,  7, 0},
	{ 23, 0x010, 0x07f,  7, 0},
	{ 24, 0x00a, 0x07f,  7, 0},
	{ 25, 0x06a, 0x07f,  7, 0},
	{ 26, 0x064, 0x07f,  7, 0},
	{ 27, 0x012, 0x07f,  7, 0},
	{ 28, 0x00c, 0x07f,  7, 0},
	{ 29, 0x040, 0x0ff,  8, 0},
	{ 30, 0x0c0, 0x0ff,  8, 0},
	{ 31, 0x058, 0x0ff,  8, 0},
	{ 32, 0x0d8, 0x0ff,  8, 0},
	{ 33, 0x048, 0x0ff,  8, 0},
	{ 34, 0x0c8, 0x0ff,  8, 0},
	{ 35, 0x028, 0x0ff,  8, 0},
	{ 36, 0x0a8, 0x0ff,  8, 0},
	{ 37, 0x068, 0x0ff,  8, 0},
	{ 38, 0x0e8, 0x0ff,  8, 0},
	{ 39, 0x014, 0x0ff,  8, 0},
	{ 40, 0x094, 0x0ff,  8, 0},
	{ 41, 0x054, 0x0ff,  8, 0},
	{ 42, 0x0d4, 0x0ff,  8, 0},
	{ 43, 0x034, 0x0ff,  8, 0},
	{ 44, 0x0b4, 0x0ff,  8, 0},
	{ 45, 0x020, 0x0ff,  8, 0},
	{ 46, 0x0a0, 0x0ff,  8, 0},
	{ 47, 0x050, 0x0ff,  8, 0},
	{ 48, 0x0d0, 0x0ff,  8, 0},
	{ 49, 0x04a, 0x0ff,  8, 0},
	{ 50, 0x0ca, 0x0ff,  8, 0},
	{ 51, 0x02a, 0x0ff,  8, 0},
	{ 52, 0x0aa, 0x0ff,  8, 0},
	{ 53, 0x024, 0x0ff,  8, 0},
	{ 54, 0x0a4, 0x0ff,  8, 0},
	{ 55, 0x01a, 0x0ff,  8, 0},
	{ 56, 0x09a, 0x0ff,  8, 0},
	{ 57, 0x05a, 0x0ff,  8, 0},
	{ 58, 0x0da, 0x0ff,  8, 0},
	{ 59, 0x052, 0x0ff,  8, 0},
	{ 60, 0x0d2, 0x0ff,  8, 0},
	{ 61, 0x04c, 0x0ff,  8, 0},
	{ 62, 0x0cc, 0x0ff,  8, 0},
	{ 63, 0x02c, 0x0ff,  8, 0}
};

/* make-up codes white */
struct g3_mh_code mk_white[27] = {
	{   64, 0x01b, 0x01f,  5, 1},
	{  128, 0x009, 0x01f,  5, 1},
	{  192, 0x03a, 0x03f,  6, 1},
	{  256, 0x076, 0x07f,  7, 1},
	{  320, 0x06c, 0x0ff,  8, 1},
	{  384, 0x0ec, 0x0ff,  8, 1},
	{  448, 0x026, 0x0ff,  8, 1},
	{  512, 0x0a6, 0x0ff,  8, 1},
	{  576, 0x016, 0x0ff,  8, 1},
	{  640, 0x0e6, 0x0ff,  8, 1},
	{  704, 0x066, 0x1ff,  9, 1},
	{  768, 0x166, 0x1ff,  9, 1},
	{  832, 0x096, 0x1ff,  9, 1},
	{  896, 0x196, 0x1ff,  9, 1},
	{  960, 0x056, 0x1ff,  9, 1},
	{ 1024, 0x156, 0x1ff,  9, 1},
	{ 1088, 0x0d6, 0x1ff,  9, 1},
	{ 1152, 0x1d6, 0x1ff,  9, 1},
	{ 1216, 0x036, 0x1ff,  9, 1},
	{ 1280, 0x136, 0x1ff,  9, 1},
	{ 1344, 0x0b6, 0x1ff,  9, 1},
	{ 1408, 0x1b6, 0x1ff,  9, 1},
	{ 1472, 0x032, 0x1ff,  9, 1},
	{ 1536, 0x132, 0x1ff,  9, 1},
	{ 1600, 0x0b2, 0x1ff,  9, 1},
	{ 1664, 0x006, 0x03f,  6, 1},
	{ 1728, 0x1b2, 0x1ff,  9, 1}
};
                 
                 
struct g3_mh_code tr_black[64] = {
	{   0, 0x3b0, 0x3ff, 10, 0},
	{   1, 0x002, 0x007,  3, 0},
	{   2, 0x003, 0x003,  2, 0},
	{   3, 0x001, 0x003,  2, 0},
	{   4, 0x006, 0x007,  3, 0},
	{   5, 0x00c, 0x00f,  4, 0},
	{   6, 0x004, 0x00f,  4, 0},
	{   7, 0x018, 0x01f,  5, 0},
	{   8, 0x028, 0x03f,  6, 0},
	{   9, 0x008, 0x03f,  6, 0},
	{  10, 0x010, 0x07f,  7, 0},
	{  11, 0x050, 0x07f,  7, 0},
	{  12, 0x070, 0x07f,  7, 0},
	{  13, 0x020, 0x0ff,  8, 0},
	{  14, 0x0e0, 0x0ff,  8, 0},
	{  15, 0x030, 0x1ff,  9, 0},
	{  16, 0x3a0, 0x3ff, 10, 0},
	{  17, 0x060, 0x3ff, 10, 0},
	{  18, 0x040, 0x3ff, 10, 0},
	{  19, 0x730, 0x7ff, 11, 0},
	{  20, 0x0b0, 0x7ff, 11, 0},
	{  21, 0x1b0, 0x7ff, 11, 0},
	{  22, 0x760, 0x7ff, 11, 0},
	{  23, 0x0a0, 0x7ff, 11, 0},
	{  24, 0x740, 0x7ff, 11, 0},
	{  25, 0x0c0, 0x7ff, 11, 0},
	{  26, 0x530, 0xfff, 12, 0},
	{  27, 0xd30, 0xfff, 12, 0},
	{  28, 0x330, 0xfff, 12, 0},
	{  29, 0xb30, 0xfff, 12, 0},
	{  30, 0x160, 0xfff, 12, 0},
	{  31, 0x960, 0xfff, 12, 0},
	{  32, 0x560, 0xfff, 12, 0},
	{  33, 0xd60, 0xfff, 12, 0},
	{  34, 0x4b0, 0xfff, 12, 0},
	{  35, 0xcb0, 0xfff, 12, 0},
	{  36, 0x2b0, 0xfff, 12, 0},
	{  37, 0xab0, 0xfff, 12, 0},
	{  38, 0x6b0, 0xfff, 12, 0},
	{  39, 0xeb0, 0xfff, 12, 0},
	{  40, 0x360, 0xfff, 12, 0},
	{  41, 0xb60, 0xfff, 12, 0},
	{  42, 0x5b0, 0xfff, 12, 0},
	{  43, 0xdb0, 0xfff, 12, 0},
	{  44, 0x2a0, 0xfff, 12, 0},
	{  45, 0xaa0, 0xfff, 12, 0},
	{  46, 0x6a0, 0xfff, 12, 0},
	{  47, 0xea0, 0xfff, 12, 0},
	{  48, 0x260, 0xfff, 12, 0},
	{  49, 0xa60, 0xfff, 12, 0},
	{  50, 0x4a0, 0xfff, 12, 0},
	{  51, 0xca0, 0xfff, 12, 0},
	{  52, 0x240, 0xfff, 12, 0},
	{  53, 0xec0, 0xfff, 12, 0},
	{  54, 0x1c0, 0xfff, 12, 0},
	{  55, 0xe40, 0xfff, 12, 0},
	{  56, 0x140, 0xfff, 12, 0},
	{  57, 0x1a0, 0xfff, 12, 0},
	{  58, 0x9a0, 0xfff, 12, 0},
	{  59, 0xd40, 0xfff, 12, 0},
	{  60, 0x340, 0xfff, 12, 0},
	{  61, 0x5a0, 0xfff, 12, 0},
	{  62, 0x660, 0xfff, 12, 0},
	{  63, 0xe60, 0xfff, 12, 0}
};

struct g3_mh_code mk_black[27] = {
	{   64,  0x3c0, 0x03ff, 10, 1},
	{  128,  0x130, 0x0fff, 12, 1},
	{  192,  0x930, 0x0fff, 12, 1},
	{  256,  0xda0, 0x0fff, 12, 1},
	{  320,  0xcc0, 0x0fff, 12, 1},
	{  384,  0x2c0, 0x0fff, 12, 1},
	{  448,  0xac0, 0x0fff, 12, 1},
	{  512,  0x6c0, 0x1fff, 13, 1},
	{  576, 0x16c0, 0x1fff, 13, 1},
	{  640,  0xa40, 0x1fff, 13, 1},
	{  704, 0x1a40, 0x1fff, 13, 1},
	{  768,  0x640, 0x1fff, 13, 1},
	{  832, 0x1640, 0x1fff, 13, 1},
	{  896,  0x9c0, 0x1fff, 13, 1},
	{  960, 0x19c0, 0x1fff, 13, 1},
	{ 1024,  0x5c0, 0x1fff, 13, 1},
	{ 1088, 0x15c0, 0x1fff, 13, 1},
	{ 1152,  0xdc0, 0x1fff, 13, 1},
	{ 1216, 0x1dc0, 0x1fff, 13, 1},
	{ 1280,  0x940, 0x1fff, 13, 1},
	{ 1344, 0x1940, 0x1fff, 13, 1},
	{ 1408,  0x540, 0x1fff, 13, 1},
	{ 1472, 0x1540, 0x1fff, 13, 1},
	{ 1536,  0xb40, 0x1fff, 13, 1},
	{ 1600, 0x1b40, 0x1fff, 13, 1},
	{ 1664,  0x4c0, 0x1fff, 13, 1},
	{ 1728, 0x14c0, 0x1fff, 13, 1}
};

struct g3_mh_code special_cw[2] = {
	{ 0, 0x0000, 0x0fff, 12, 3},
	{ 0, 0x0800, 0x0fff, 12, 2}
};

struct g3_mh_code **white_rev;
struct g3_mh_code **black_rev;

/* white run length table wrunlen_tbl[bitpos to start][val of byte] */
static uint8_t wrunlen_tbl[8][256];

static uint8_t bit_swap[256];

/* we use 13 bit tables */
#define G3_REVERSE_TBL_SIZE	8192
#define G3_REVERSE_MASK		0x1fff
#define G3_REVERSE_BITS		13



static int fill_rev_tbl(struct g3_mh_code **tbl, struct g3_mh_code *code)
{
	uint16_t i_cnt, shift, idx;

	i_cnt = 0;
	shift = code->bits;
	idx = code->val | (i_cnt << shift);
	while (idx < G3_REVERSE_TBL_SIZE) {
		if (!tbl[idx]) {
			tbl[idx] = code;
		} else {
			wprint("Error during fill %x/%d  idx %x already filled with %x/%d\n",
				code->val, shift, idx, tbl[idx]->val, tbl[idx]->bits);
			return -1;
		}
		i_cnt++;
		idx = code->val | (i_cnt << shift);
	};
	return 0;
}

static inline uint8_t calc_byte_runlen(uint8_t val, uint8_t sb)
{
	uint8_t i = sb, bm = 1 << sb;

	while (i < 8) {
		if ( val & bm)
			break;
		i++;
		bm <<= 1;
	}
	return i - sb;
}


void g3_gen_tables(void)
{
	uint8_t i, v;

	white_rev = calloc(G3_REVERSE_TBL_SIZE, sizeof(*white_rev));
	black_rev = calloc(G3_REVERSE_TBL_SIZE, sizeof(*black_rev));
	
	for (i = 0; i < 64; i++) {
		if (fill_rev_tbl(white_rev, &tr_white[i]))
			wprint("Error during white term fill\n");
	}
	for (i = 0; i < 27; i++) {
		if (fill_rev_tbl(white_rev, &mk_white[i]))
			wprint("Error during white makeup fill\n");
	}
	if (fill_rev_tbl(white_rev, &special_cw[0]))
		wprint("Error during white skip fill\n");
	if (fill_rev_tbl(white_rev, &special_cw[1]))
		wprint("Error during white EOL fill\n");
	for (i = 0; i < 64; i++) {
		if (fill_rev_tbl(black_rev, &tr_black[i]))
			wprint("Error during black term fill\n");
	}
	for (i = 0; i < 27; i++) {
		if (fill_rev_tbl(black_rev, &mk_black[i]))
			wprint("Error during black makeup fill\n");
	}
	if (fill_rev_tbl(black_rev, &special_cw[0]))
		wprint("Error during black skip fill\n");
	if (fill_rev_tbl(black_rev, &special_cw[1]))
		wprint("Error during black EOL fill\n");

	special_cw[0].bits = 1; /* skip only one bit a time */

	/* run length table */  
	for (i = 0; i < 8; i++) {
		v = 0;
		do {
			wrunlen_tbl[i][v] = calc_byte_runlen(v, i);
			v++;
			
		} while (v != 0);
	}
	/* bit order tab */
	v = 0;
	do {
		bit_swap[v] =  (((v & 0x01) << 7) | ((v & 0x02) << 5) | ((v & 0x04) << 3) | ((v & 0x08) << 1) |
				((v & 0x10) >> 1) | ((v & 0x20) >> 3) | ((v & 0x40) >> 5) | ((v & 0x80) >> 7));
		v++;
	} while (v != 0);
	                                                                                       
}

static struct g3_mh_code *g3_lookup_code(uint16_t val, int black)
{
	struct g3_mh_code *code;

	if (black)
		code = black_rev[val & G3_REVERSE_MASK];
	else
		code = white_rev[val & G3_REVERSE_MASK];
	return code;
}

static inline void _update_nb(struct g3_mh_line_s *ls)
{
	if (ls->dp) {
		ls->nb = *ls->dp++;
		if (ls->dp >= (ls->line + ls->len))
			ls->dp = NULL; /* EOL */
	} else
		ls->nb = 0; /* fill with 0 bits */
	ls->nb_bits = 8;
	ls->sreg |= ls->nb << 16;
}

static void advance_sreg(struct g3_mh_line_s *ls, uint8_t bits)
{
	uint8_t b;

	while (bits) {
		if (!ls->nb_bits)
			_update_nb(ls);
		b = bits;
		if (b > ls->nb_bits)
			b = ls->nb_bits;
		ls->sreg >>= b;
		ls->nb_bits -= b;
		bits -= b;
	} 
}

static void put_black_run(struct g3_mh_line_s *ls, int bits)
{
	uint8_t bitp, bm, b, bb;
	uint16_t bytep;

	while (bits > 0) {
		b = 0xff;
		bb = 8;
		bitp = ls->bitpos & 7;
		bytep = ls->bitpos >> 3;
		if (bitp) {
			b <<= bitp;
			bb -= bitp;
		}
		if (bits < bb) {
			bb = bits;
			bm = b;
			bm <<= bb;
			bm = ~bm;
			b &= bm;
		}
		ls->rawline[bytep] |= bit_swap[b];
		ls->bitpos += bb;
		bits -= bb;
	}
}


const char *code_type_str[] = {
	"termination",
	"markup",
	"EOL",
	"FillBit",
};

int g3_decode_line(struct g3_mh_line_s *ls)
{
	struct g3_mh_code *cc;
	int col = 0; /* start white */
	int bits = 0;
	// int wlen, ret;

	ls->dp = ls->line;
	ls->sreg = *ls->dp++;
	ls->nb = *ls->dp++;
	ls->sreg |= (ls->nb << 8);
	ls->nb_bits = 0;
	ls->bitcnt = 0;
	ls->bitpos = 0;
	memset(ls->rawline, 0, ((ls->linelen + 7) >> 3));
	dprint(MIDEBUG_NCCI_DATA, "Start decoding line %d, len=%d\n", ls->nr, ls->len);
	while (ls->bitcnt <= ls->linelen) {
		cc = g3_lookup_code(ls->sreg, col);
		if (cc) {
			ls->bitcnt += cc->rl;
			dprint(MIDEBUG_NCCI_DATA, "sreg = %04x %s %s code %x %d bits runlen %d  sum %4d\n",
				ls->sreg, col ? "black" : "white", code_type_str[cc->type],
				cc->val, cc->bits, cc->rl, ls->bitcnt);
			advance_sreg(ls, cc->bits);
			switch(cc->type) {
			case G3_CWTYPE_TERMINATION:
				bits += cc->rl;
				if (col)
					put_black_run(ls, bits);
				else
					ls->bitpos += bits;
				bits = 0;
				col = !col;
				break;
			case G3_CWTYPE_MAKEUP:
				bits += cc->rl;
				break;
			default:
				break;
			}
		} else {
			wprint("sreg = %04x no code found\n", ls->sreg);
			break;
		}
		if (ls->bitcnt == ls->linelen && ls->sreg == 0)
			break;
	}
	dprint(MIDEBUG_NCCI_DATA, "Stop decoding line %d, len=%d\n", ls->nr, ls->bitcnt);
	// g3_print_hex(stdout, ls->rawline, (ls->bitcnt +7)>>3);
#if 0
	if (ls->fd >= 0) {
		/* Write raw linedata */
		wlen = (ls->linelen + 7) >> 3;
		ret = write(ls->fd, ls->rawline, wlen);
		if (ret != wlen) {
			wprint("Cannot write %d bytes (ret = %d) to plain file - %s\n", wlen, ret, strerror(errno));
		}
	}
#endif
	return ls->bitcnt;
}

static int calc_current_runlen(struct g3_mh_line_s *ls, int col)
{
	uint16_t sval = ls->bitcnt;
	uint16_t bitsleft, crl, rl = 0;
	uint8_t val, sbit, bitcnt;

	bitsleft = ls->linelen - ls->bitcnt;
	while (bitsleft) {
		sbit = sval & 7;
		val = bit_swap[ls->rawline[sval >> 3]];
		if (col)
			val ^= 0xff;
		bitcnt = 8 - sbit;
		if (bitsleft < bitcnt)
			bitcnt = bitsleft;
		crl = wrunlen_tbl[sbit][val];
		if (crl > bitcnt)
			crl = bitcnt;
		rl += crl;
		if (crl < bitcnt) {
			/* found new color */
			sval += crl;
			break;
		}
		sval += bitcnt;
		bitsleft -= bitcnt;
	}
	ls->bitcnt = sval;
	return rl;
}

static void write_mh_code(struct g3_mh_line_s *ls, struct g3_mh_code *cc, int col)
{
	uint32_t creg;
	uint16_t bits, bitoff, idx;

	creg = cc->val;
	bitoff = ls->bitpos & 7;
	idx = ls->bitpos >> 3;
	creg <<= bitoff;
	bits = cc->bits;
	ls->sreg |= creg;
	ls->line[idx] = ls->sreg & 0xff;
	ls->bitpos += bits;
	bits += bitoff;
	dprint(MIDEBUG_NCCI_DATA, "sreg = %04x %s %s code %x %d bits runlen %d  sum %4d\n",
		ls->sreg, col ? "black" : "white", code_type_str[cc->type],
		cc->val, cc->bits, cc->rl, ls->bitpos);
	while (bits > 7) {
		ls->line[idx] = ls->sreg & 0xff;
		idx++;
		bits -= 8;
		ls->sreg >>= 8;
	}
	ls->line[idx] = ls->sreg & 0xff;
}

static int put_runlen(struct g3_mh_line_s *ls, uint16_t rl, int col)
{
	uint16_t tidx, midx;
	struct g3_mh_code *cc;
	
	tidx = rl & 0x3f;
	midx = rl >> 6;
	
	if (midx) {
		if (midx > 28) {
			wprint("runlen too big - not supported yet\n");
			return -1;
		}
		midx--;
		cc = col ? &mk_black[midx] : &mk_white[midx];
		write_mh_code(ls, cc, col);
	}
	cc = col ? &tr_black[tidx] : &tr_white[tidx];
	write_mh_code(ls, cc, col);
	return 0;
}

int g3_encode_line(struct g3_mh_line_s *ls)
{
	int col = 0;
	uint16_t rl;

	ls->dp = ls->line;
	ls->bitpos = 0;
	ls->bitcnt = 0;
	ls->sreg = 0;
	dprint(MIDEBUG_NCCI_DATA, "Start encoding line %d, len=%d\n", ls->nr, ls->linelen);
	while (ls->bitcnt < ls->linelen) {
		rl = calc_current_runlen(ls, col);
		put_runlen(ls, rl, col);
		col = !col;
	}
	ls->len = (ls->bitpos + 7) >> 3;
	dprint(MIDEBUG_NCCI_DATA, "Stop encoding line %d, compressed bits: %d len %d\n", ls->nr, ls->bitpos, ls->len);
	return ls->len;
}


/* USE_SOFTFAX */
#endif
