/*
 * G3 decoding 
 *
 * Written by Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright (C) 2011 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE
 * version 2.1 as published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU LESSER GENERAL PUBLIC LICENSE for more details.
 *
 */

#ifndef _G3_MH_H
#define _G3_MH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct g3_mh_code {
	uint16_t	rl;
	uint16_t	val;
	uint16_t	bitm;
	uint8_t		bits;
	uint8_t		type;
};

#define G3_CWTYPE_TERMINATION	0
#define G3_CWTYPE_MAKEUP	1

extern void g3_gen_tables(void);

struct g3_mh_line_s {
	unsigned char	*line;
	unsigned char   *dp;
	unsigned char	*rawline;
	uint16_t	bitpos;
	uint16_t	len;
	uint32_t	sreg;
	uint8_t		nb;
	uint16_t	linelen;
	uint16_t	nb_bits;
	uint16_t	bitcnt;
	int		nr;
};

extern int g3_decode_line(struct g3_mh_line_s *);
extern int g3_encode_line(struct g3_mh_line_s *);

#ifdef __cplusplus
}

#endif

#endif
