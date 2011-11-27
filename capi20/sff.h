/*
 * sff.h
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2011  by Karsten Keil <kkeil@linux-pingi.de>
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

#ifndef _SFF_H
#define _SFF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mc_buffer.h"

#define SFF_MAGIC_HEAD		0x66666653
#define SFF_VERSION		1
#define SFF_RT_PAGEHEAD		0xfe

#define SFF_FILE_HEADER_SIZE	20
struct sff_file_head {
	uint32_t	magic;
	uint8_t		version;
	uint8_t		reserved;
	uint16_t	userinfo;
	uint16_t	pagecount;
	uint16_t	o_firstpageheader;
	uint32_t	o_lastpageheader;
	uint32_t	o_docend;
};

#define SFF_PAGE_HEADER_SIZE	18
struct sff_page_head {
	uint8_t		rectype;
	uint8_t		pageheaderlen;
	uint8_t		res_vertical;
	uint8_t		res_horizontal;
	uint8_t		coding;
	uint8_t		reserved;
	uint16_t	linelen;
	uint16_t	pagelen;
	uint32_t	o_previous_page;
	uint32_t	o_next_page;	
};

enum SFFState {
	SFF_Begin = 0,
	SFF_Header,
	SFF_PageHeader,
	SFF_PageData,
	SFF_PageDone,
	SFF_DocEnd,
};

struct sff_page;

struct sff_state {
	enum SFFState		state;
	struct sff_file_head	fh;
	int			page_cnt;
	struct sff_page		*firstpage;
	struct sff_page		*lastpage;
	size_t			data_size;
	size_t			size;
	unsigned char		*data;
	unsigned char		*dp;	/* current byte */
	unsigned char		*ep;	/* last byte */
};

int SFF_Put_Data(struct sff_state *, unsigned char *, int);
int SFF_WriteTiff(struct sff_state *, char *);
int SFF_ReadTiff(struct sff_state *, char *);
#ifdef __cplusplus
}

#endif

#endif
