/*
 * SFF to TIFF and TIFF to SFF 
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

#include <tiffio.h>
#include "m_capi.h"
#include "mc_buffer.h"
#include "sff.h"
#include "g3_mh.h"

/* This debug will generate huge amount of data and slow down the process a lot */
//#define SFF_VERBOSE_DEBUG 1

#define SFF_DATA_BLOCK_SIZE	(64 * 1024)

struct sff_page {
	enum SFFState		state;
	int			nr;
	struct sff_page_head	head;
	unsigned char		*headp;
	unsigned char		*start;
	unsigned char		*ep;	/* last byte of page */
	unsigned char		*dp;
	unsigned char		*CStrip;
	int			CStrip_size;
	unsigned char		*sp;
	unsigned char		*RStrip;
	int			lines;
	int			line_size; /* size in bytes to hold 1 line - linelen is pixel ! */
	int			size;
	TIFF			*tiff;
	struct sff_page		*next;
};

static int sff_read_page_header(struct sff_state *sf)
{
	struct sff_page *pg;

	pg = calloc(1, sizeof(*pg));
	if (!pg) {
		eprint("No memory for page data\n");
		return -ENOMEM;
	}
	if (!sf->firstpage)
		sf->firstpage = pg;

	pg->nr =  sf->page_cnt + 1;
	pg->headp = sf->dp;
	pg->dp = pg->headp;
	pg->head.rectype = *pg->dp;
	pg->dp++;
	pg->head.pageheaderlen = *pg->dp;
	pg->dp++;
	pg->head.res_vertical = *pg->dp;
	pg->dp++;
	pg->head.res_horizontal = *pg->dp;
	pg->dp++;
	pg->head.coding = *pg->dp;
	pg->dp++;
	pg->head.reserved = *pg->dp;
	pg->dp++;
	pg->head.linelen = CAPIMSG_U16(pg->dp, 0);
	pg->dp += 2;
	pg->line_size = (pg->head.linelen + 7) >> 3;
	pg->head.pagelen = CAPIMSG_U16(pg->dp, 0);
	pg->dp += 2;
	pg->head.o_previous_page = CAPIMSG_U32(pg->dp, 0);
	pg->dp += 4;
	pg->head.o_next_page = CAPIMSG_U32(pg->dp, 0);	
	pg->dp += 4;
	sf->state = SFF_PageHeader;
	pg->state = SFF_PageHeader;
	pg->start = pg->headp + 2 + pg->head.pageheaderlen;
	pg->dp = pg->start;
	sf->dp = pg->start;
	if (sf->lastpage)
		sf->lastpage->next = pg;
	sf->lastpage = pg;
	sf->page_cnt++;
	return 0;
}

static int sff_read_file_header(struct sff_state *sf)
{
	sf->dp = sf->data;
	if (sf->size < SFF_FILE_HEADER_SIZE) {
		wprint("SFF too small(%zu/%zu\n", sf->size, sizeof(sf->fh));
		return 1;
	}
	sf->fh.magic = CAPIMSG_U32(sf->dp, 0);
	sf->fh.version = CAPIMSG_U8(sf->dp, 4);
	sf->fh.reserved = CAPIMSG_U8(sf->dp, 5);
	sf->fh.userinfo = CAPIMSG_U16(sf->dp, 6);
	sf->fh.pagecount = CAPIMSG_U16(sf->dp, 8);
	sf->fh.o_firstpageheader = CAPIMSG_U16(sf->dp, 10);
	sf->fh.o_lastpageheader  = CAPIMSG_U32(sf->dp, 12);
	sf->fh.o_docend = CAPIMSG_U32(sf->dp, 16);
	sf->dp = sf->data + sf->fh.o_firstpageheader;
	sf->state = SFF_Header;
	return 0;
}

#if 0
static int add_eol(unsigned char *p)
{
	*p++ = 0x00;
	*p++ = 0x80;
	return 2;
}

static int add_empty_line(unsigned char *tp, struct g3_mh_line_s *ls)
{
	unsigned char *p = tp;

	*p++ = 0xB2;
	*p++ = 0x59;
	*p++ = 0x01;
	*p++ = 0x80;

	ls->len = (uint16_t)(p - tp);
	ls->line = tp;
	g3_decode_line(ls);
	return ls->len;
}

static int add_empty_lines(unsigned char *p, int cnt, struct g3_mh_line_s *ls)
{
	int i,l, ret = 0;

	for (i = 0; i < cnt; i++) {
		l = add_empty_line(p, ls);
		p += l;
		ret += l;
	}
	return ret;
}

static int add_line(unsigned char *p, unsigned char *s, int len, struct g3_mh_line_s *ls)
{
	struct g3_mh_line_s lo;

	lo.nr = ls->nr;
	lo.rawline = ls->rawline;
	lo.linelen = ls->linelen;
	lo.line = calloc(1, ((lo.linelen + 7) >> 3));
	memcpy(p, s, len);
	ls->line = p;
	ls->len = len;
	g3_decode_line(ls);
#if 0
	if (Out_TiffF) {
		if (TIFFWriteScanline(Out_TiffF, ls->rawline, ls->nr - 1, 0) < 0) {
			wprint("%s: Write error at row %d.\n",
			    tiff_file, ls->nr);
		}
	}
#endif
	g3_encode_line(&lo);
	if (ls->len == lo.len) {
		if (memcmp(ls->line, lo.line, ls->len))
			wprint("SFF encoded line %d do not match\n", ls->nr);
	} else
		wprint("SFF encoded line %d len %d do not match %d\n", ls->nr, lo.len, ls->len);
	free(lo.line);
	return len + add_eol(p + len);
}
#endif

static int add_raw_line(struct sff_page *pg, unsigned char *s, int len, int nr)
{
	struct g3_mh_line_s ls;

	ls.nr = nr;
	ls.rawline = pg->sp;
	ls.linelen = pg->head.linelen;
	ls.line = s;
	ls.len = len;
	g3_decode_line(&ls);
	if (pg->tiff) {
		if (TIFFWriteScanline(pg->tiff, ls.rawline, ls.nr, 0) < 0) {
			wprint("Write error at row %d.\n", ls.nr);
		}
	}
	pg->sp += pg->line_size;
	return 0;
}

static int add_raw_empty_line(struct sff_page *pg, int nr)
{
	unsigned char buf[8];
	unsigned char *p = buf;
	int len;

	*p++ = 0xB2;
	*p++ = 0x59;
	*p++ = 0x01;
	*p++ = 0x80;

	len = (uint16_t)(p - buf);
	return add_raw_line(pg, buf, len, nr);
}

static int add_raw_empty_lines(struct sff_page *pg, int cnt, int start)
{
	int i;

	for (i = 0; i < cnt; i++) {
		add_raw_empty_line(pg, start + i);
	}
	return 0;
}

int  sff_decode_page_data(struct sff_page *pg)
{
	unsigned char rt;
	int l, nr;

	pg->RStrip = calloc(pg->lines, pg->line_size);
	pg->sp = pg->RStrip;
	pg->dp = pg->start;
	rt = *pg->dp;
	pg->dp++;
	nr = 0;
	while (pg->dp < pg->ep) {
		l = rt;
		if (rt == 0) {
			/* escape 2 byte len */
			l = CAPIMSG_U16(pg->dp, 0);
			pg->dp += 2;
#ifdef SFF_VERBOSE_DEBUG 
			dprint(MIDEBUG_NCCI_DATA, "Line %d with %d bytes found in page %d\n", nr, l, pg->nr);
#endif
			add_raw_line(pg, pg->dp, l, nr);
			nr++;
		} else if (rt < 217) {
			l = rt;
#ifdef SFF_VERBOSE_DEBUG 
			dprint(MIDEBUG_NCCI_DATA, "Line %d with %d bytes found in page %d\n", nr, l, pg->nr);
#endif
			add_raw_line(pg, pg->dp, l, nr);
			nr++;
		} else if (rt < 254) {
			l = 0;
#ifdef SFF_VERBOSE_DEBUG 
			dprint(MIDEBUG_NCCI_DATA, "Skip %d empty lines (line %d)  in page %d\n", rt - 216, nr, pg->nr);
#endif
			add_raw_empty_lines(pg,  rt - 216, nr);
			nr += (rt - 216);
		} else if (rt == 254) {
			/* Page header should be never occur */
			wprint("Pageheader inside page %d \n", pg->nr);
		} else { /* 255 */
			l = *pg->dp;
			pg->dp++;
			if (!l) {
				add_raw_empty_line(pg, nr);
				nr++;
				dprint(MIDEBUG_NCCI_DATA, "Illegal Line %d in page %d found\n", nr, pg->nr);
			} else
				dprint(MIDEBUG_NCCI_DATA, "%d bytes userdata found in page %d\n", l, pg->nr);
			
		}
		pg->dp += l;
		rt = *pg->dp;
		pg->dp++;
	}
	return 0;
}

static int sff_read_data(struct sff_state *sff)
{
	struct sff_page *pg;
	uint8_t rt;
	uint16_t l;
	int ret;

	if (sff->state < SFF_Header) {
		ret = sff_read_file_header(sff);
		if (ret < 0)
			return ret;
		if (sff->dp == sff->ep)
			return 1;
	}
	pg = sff->lastpage;
	while ((sff->dp + 1) < sff->ep) {
		rt = *sff->dp;
		if (rt == 0) {
			/* escape 2 byte len */
			if ((sff->dp + 3) < sff->ep) { 
				l = 3 + CAPIMSG_U16(sff->dp, 1);
				if ((sff->dp + l) >= sff->ep)
					break;
				pg->lines++;
			} else
				break;;
		} else if (rt < 217) {
			l = 1 + rt;
			if ((sff->dp + l) >= sff->ep)
				break;
			pg->lines++;
		} else if (rt < 254) {
			l = 1;
			pg->lines += rt - 216;
		} else if (rt == 254) {
			/* New Page header */
			if (pg)
				pg->ep = sff->dp;
			if (sff->dp[1] == 0) {
				/* EOF reached */
				return 0;
			}
			l = 2 + sff->dp[1];
			if ((sff->dp + l) >= sff->ep)
				break;
			ret = sff_read_page_header(sff);
			if (ret)
				return ret;
			pg = sff->lastpage;
			l = 0; /* read read_page_header did change sff->dp */
		} else { /* 255 */
			l = 1 + sff->dp[1];
			if (l == 1) {
				pg->lines++;
			} else {
				if ((sff->dp + l) >= sff->ep)
					break;
			}
		}
		sff->dp += l;
	}
	return 1;
}

static void adjust_memory(struct sff_state *sff, unsigned char *np)
{
	size_t offset;
	struct sff_page *pg;

	offset = np - sff->data;
	dprint(MIDEBUG_NCCI_DATA, "Adjust data offset old=%p new=%p offset=%zu data_size=%zu\n",
		sff->data, np, offset, sff->data_size);
	if (sff->dp)
		sff->dp += offset;
	pg = sff->firstpage;
	while (pg) {
		if (pg->headp)
			pg->headp += offset;
		if (pg->start)
			pg->start += offset;
		if (pg->dp)
			pg->dp += offset;
		if (pg->ep)
			pg->ep += offset;
		pg = pg->next;
	}	
}

int SFF_Put_Data(struct sff_state *sff, unsigned char *data, int len)
{
	unsigned char *dp;
	size_t inc;

	if ((sff->size + len) >= sff->data_size) {
		inc = SFF_DATA_BLOCK_SIZE;
		while (len > inc)
			inc += SFF_DATA_BLOCK_SIZE;
		dp = realloc(sff->data, sff->data_size + inc);
		if (!dp) {
			eprint("No memory for sff data block (%zd)\n", sff->data_size + inc);
			return -ENOMEM;
		}
		sff->data_size += inc;
		if (sff->data && sff->data != dp) {
			adjust_memory(sff, dp);
		} else
			dprint(MIDEBUG_NCCI_DATA, "Adjust data_size=%zu data=%p dp=%p\n", sff->data_size, sff->data, dp);
		sff->data = dp;
		if (!sff->dp)
			sff->dp = dp;
	}
	memcpy(sff->data + sff->size, data, len);
	sff->size += len;
	sff->ep = sff->data + sff->size;
	return sff_read_data(sff);
}

static int sff_copy_data(struct sff_state *sff, unsigned char *data, int len)
{
	unsigned char *dp;
	size_t inc;

	if ((sff->size + len) >= sff->data_size) {
		inc = SFF_DATA_BLOCK_SIZE;
		while (len > inc)
			inc += SFF_DATA_BLOCK_SIZE;
		dp = realloc(sff->data, sff->data_size + inc);
		if (!dp) {
			eprint("No memory for sff data block (%zd)\n", sff->data_size + inc);
			return -ENOMEM;
		}
		sff->data_size += inc;
		if (sff->data && sff->data != dp) {
			adjust_memory(sff, dp);
		} else
			dprint(MIDEBUG_NCCI_DATA, "Adjust data_size=%zu data=%p dp=%p\n", sff->data_size, sff->data, dp);
		sff->data = dp;
		if (!sff->dp)
			sff->dp = dp;
	}
	/* if data is NULL reserve len bytes */
	if (data)
		memcpy(sff->dp, data, len);
	sff->size += len;
	sff->dp += len;
	return len;
}

static int sff_put_encoded_line(struct sff_state *sff, struct sff_page *pg, struct g3_mh_line_s *ls)
{
	int ret = 0;
	unsigned char tmp[4];

	if (ls->len < 217) {
		tmp[0] = ls->len;
		ret = sff_copy_data(sff, tmp, 1);
		if (ret < 0)
			return ret;
		ret = sff_copy_data(sff, ls->line, ls->len);
	} else {
		tmp[0] = 0;
		capimsg_setu16(tmp, 1, ls->len);
		ret = sff_copy_data(sff, tmp, 3);
		if (ret < 0)
			return ret;
		ret = sff_copy_data(sff, ls->line, ls->len);
	}
	if (ret >= 0) {
		pg->dp = sff->dp;
		pg->ep = pg->dp;
	}
	return ret;
}

static int sff_put_page(struct sff_state *sff, struct sff_page *pg)
{
	int ret = 0;

	capimsg_setu8(pg->headp, 0, SFF_RT_PAGEHEAD);
	capimsg_setu8(pg->headp, 1, 0x10);
	capimsg_setu8(pg->headp, 2, pg->head.res_vertical);
	capimsg_setu8(pg->headp, 3, pg->head.res_horizontal);
	capimsg_setu8(pg->headp, 4, pg->head.coding);
	capimsg_setu8(pg->headp, 5, pg->head.reserved);
	capimsg_setu16(pg->headp, 6, pg->head.linelen);
	capimsg_setu16(pg->headp, 8, pg->head.pagelen);
	capimsg_setu32(pg->headp, 10, pg->head.o_previous_page);
	capimsg_setu32(pg->headp, 14, pg->head.o_next_page);
	return ret;
}

static int sff_put_eof(struct sff_state *sff)
{
	unsigned char eof[2] = {SFF_RT_PAGEHEAD, 0};

	return sff_copy_data(sff, eof, 2);
}

static int sff_put_header(struct sff_state *sff)
{
	int ret = 0;

	if (sff->size < SFF_FILE_HEADER_SIZE) {
		ret = sff_copy_data(sff, NULL, SFF_FILE_HEADER_SIZE);
		if (ret < 0)
			return ret;
	}
	sff->fh.o_docend = sff->size;
	sff->fh.o_firstpageheader = 0x14;
	sff->fh.pagecount = sff->page_cnt;
	if (sff->lastpage)
		sff->fh.o_lastpageheader = sff->lastpage->headp - sff->data;
	capimsg_setu32(sff->data, 0, sff->fh.magic);
	capimsg_setu8(sff->data, 4, sff->fh.version);
	capimsg_setu8(sff->data, 5, sff->fh.reserved);
	capimsg_setu16(sff->data, 6, sff->fh.userinfo);
	capimsg_setu16(sff->data, 8, sff->fh.pagecount);
	capimsg_setu16(sff->data, 10, sff->fh.o_firstpageheader);
	capimsg_setu32(sff->data, 12, sff->fh.o_lastpageheader);
	capimsg_setu32(sff->data, 16, sff->fh.o_docend);
	return 0;
}

int SFF_WriteTiff(struct sff_state *sff, char *name)
{
	struct sff_page *next, *pg = sff->firstpage;
	TIFF *tf;
	int compression_out = COMPRESSION_CCITTFAX3;
	int fillorder_out = FILLORDER_MSB2LSB;
	uint32 group3options_out = GROUP3OPT_FILLBITS|GROUP3OPT_2DENCODING;
	uint32 group4options_out = 0;	/* compressed */
	uint32 defrowsperstrip = (uint32) 0;
	uint32 rowsperstrip;
	int photometric_out = PHOTOMETRIC_MINISWHITE;
	float resY;

	tf = TIFFOpen(name, "w");
	if (tf == NULL) {
		wprint("Can not create Tiff file %s\n", name);
		return -1;
	}
	while(pg) {
		if (pg->lines) {
			if (pg->head.pagelen == 0) {
				pg->head.pagelen = pg->lines;
			} else if (pg->head.pagelen < pg->lines) {
				wprint("found more lines %d as in header %d\n", pg->lines, pg->head.pagelen);
				pg->head.pagelen = pg->lines;
			} else if (pg->head.pagelen > pg->lines) {
				wprint("found less lines %d as in header %d\n", pg->lines, pg->head.pagelen);
				pg->head.pagelen = pg->lines;
			}
		} else {
			wprint("SFF page %d no lines detected\n", pg->nr);
		}
		if (pg->head.res_vertical)
			resY = 196.0;
		else
			resY = 98.0;

		TIFFSetField(tf, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
		TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, pg->head.linelen);
		TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 1);
		TIFFSetField(tf, TIFFTAG_COMPRESSION, compression_out);
		TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, photometric_out);
		TIFFSetField(tf, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, 1);
		switch (compression_out) {
			/* g3 */
			case COMPRESSION_CCITTFAX3:
				TIFFSetField(tf, TIFFTAG_GROUP3OPTIONS, group3options_out);
				TIFFSetField(tf, TIFFTAG_FAXMODE, FAXMODE_CLASSIC);
				rowsperstrip = (defrowsperstrip) ? defrowsperstrip : (uint32)-1;
				break;

			/* g4 */
			case COMPRESSION_CCITTFAX4:
				TIFFSetField(tf, TIFFTAG_GROUP4OPTIONS, group4options_out);
				TIFFSetField(tf, TIFFTAG_FAXMODE, FAXMODE_CLASSIC);
				rowsperstrip = (defrowsperstrip) ? defrowsperstrip : (uint32)-1;
				break;

			default:
				rowsperstrip = (defrowsperstrip) ? defrowsperstrip : TIFFDefaultStripSize(tf, 0);
				break;
		}
		TIFFSetField(tf, TIFFTAG_ROWSPERSTRIP, rowsperstrip);
		TIFFSetField(tf, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(tf, TIFFTAG_FILLORDER, fillorder_out);
		TIFFSetField(tf, TIFFTAG_SOFTWARE, "mISDNcapid");
		TIFFSetField(tf, TIFFTAG_XRESOLUTION, 203.0);

		TIFFSetField(tf, TIFFTAG_YRESOLUTION, resY);

		TIFFSetField(tf, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
		TIFFSetField(tf, TIFFTAG_PAGENUMBER, sff->page_cnt, pg->nr);
		pg->tiff = tf;
		sff_decode_page_data(pg);
		dprint(MIDEBUG_NCCI, "SFF wrote page %d with %d lines\n", pg->nr, pg->lines);
		TIFFSetField(tf, TIFFTAG_IMAGELENGTH, pg->lines);
		TIFFWriteDirectory(tf);
		pg = pg->next;
	}
	TIFFClose(tf);
	pg = sff->firstpage;
	while (pg) {
		if (pg->RStrip)
			free(pg->RStrip);
		next = pg->next;
		free(pg);
		pg = next;
	}
	return 0;
}

int SFF_ReadTiff(struct sff_state *sff, char *name)
{
	TIFF *tf;
	int linelen = 0, retval = 0, newpg = 1, lines;
	int i, ret;
	unsigned char *rawbuf = NULL;
	unsigned char *cbuf = NULL;
	struct sff_page *pg, *prevpg;
	struct g3_mh_line_s ls;
	float resV;

	tf = TIFFOpen(name, "r");
	if (tf == NULL) {
		wprint("Can not open Tiff file %s\n", name);
		return -1;
	}
	rawbuf = calloc(1, 4096);
	if (!rawbuf) {
		eprint("Cannot allocate SFF rawbuf\n");
		goto end;
	}
	cbuf = calloc(1, 4096);
	if (!cbuf) {
		eprint("Cannot allocate SFF cbuf\n");
		goto end;
	}
	sff->fh.magic = SFF_MAGIC_HEAD;
	sff->fh.version = SFF_VERSION;
	sff->fh.reserved = 0;
	sff->fh.userinfo = 0;
	sff->fh.pagecount = 0;
	sff->fh.o_firstpageheader = 0x14;
	sff->fh.o_lastpageheader  = 0;
	sff->fh.o_docend = 0;
	sff->state = SFF_Header;
	ret = sff_copy_data(sff, NULL, SFF_FILE_HEADER_SIZE);
	sff->page_cnt = 0;
	if (ret < 0) {
		eprint("Cannot reserve the file header\n");
		retval = -ENOMEM;
		goto end;
	}
	while (newpg) {
		pg = calloc(1, sizeof(*pg));
		if (!pg) {
			eprint("Cannot allocate SFF page header\n");
			retval = -ENOMEM;
			break;
		}
		if (!sff->firstpage)
			sff->firstpage = pg;
		prevpg = sff->lastpage;
		if (sff->lastpage)
			sff->lastpage->next = pg;
		sff->lastpage = pg;
		sff->page_cnt++;
		if (!TIFFGetField(tf, TIFFTAG_IMAGEWIDTH, &linelen))
			wprint("TIFFTAG_IMAGEWIDTH not set\n");
		if (!linelen) {
			wprint("Using default linelength 1728\n");
			linelen = 1728;
		}
		pg->head.linelen = linelen;
		if (!TIFFGetField(tf, TIFFTAG_IMAGELENGTH, &lines)) {
			wprint("TIFFTAG_IMAGELENGTH not set\n");
			retval = -EINVAL;
			break;
		}
		pg->headp = sff->dp;
		/* reserve the page header */
		ret = sff_copy_data(sff, NULL, SFF_PAGE_HEADER_SIZE);
		if (ret < 0) {
			eprint("Cannot reserve the page header\n");
			retval = -ENOMEM;
			break;
		}
		pg->start = sff->dp;
		pg->dp = sff->dp;
		pg->head.pagelen = lines;
		ls.rawline = rawbuf;
		ls.line = cbuf;
		ls.linelen = linelen;
		if (!TIFFGetField(tf, TIFFTAG_YRESOLUTION, &resV)) {
			wprint("TIFFTAG_YRESOLUTION not set\n");
			resV = 96.0;
		}
		if (resV > 97.0)
			pg->head.res_vertical = 1;
		else
			pg->head.res_vertical = 0;
		pg->head.res_horizontal = 0;
		for (i = 0; i < lines; i++) {
			ret = TIFFReadScanline(tf, rawbuf, i, 0);
			if (ret < 1) {
				wprint("TIFFReadScanline %d returned %d\n", i, ret);
				retval = -EINVAL;
				goto end;
			}
			ls.nr = i + 1;
			ret = g3_encode_line(&ls);
			if (ret < 0) {
				wprint("encoding line %d gaves error %d\n", i + 1, ret);
				retval = -EINVAL;
				goto end;
			} else {
				ret = sff_put_encoded_line(sff, pg, &ls);
				if (ret < 0) {
					wprint("putting encoded_line %d gaves error %d\n", i + 1, ret);
					retval = ret;
					goto end;
				}
			}
#ifdef SFF_VERBOSE_DEBUG 
			dprint(MIDEBUG_NCCI_DATA, "Line %d encode %d bytes %x %x %x %x %x %x %x %x\n", i, ret,
				cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7]);
#endif
		}
		if (prevpg) {
			prevpg->head.o_next_page = pg->headp - sff->data;
			pg->head.o_previous_page = prevpg->headp - sff->data;
			sff_put_page(sff, prevpg);
		} else
			pg->head.o_previous_page = 1;
		pg->head.o_next_page = 1;
		sff_put_page(sff, pg);
		newpg = TIFFReadDirectory(tf);
		if (!newpg) {
			/* write EOF tag */
			ret = sff_put_eof(sff);
			if (ret < 0) {
				eprint("Cannot put EOF\n");
				retval = -ENOMEM;
				break;
			}
			/* write header */
			sff_put_header(sff);
		}
	}
end:
	if (rawbuf)
		free(rawbuf);
	if (cbuf)
		free(cbuf);
	while (sff->firstpage) {
		pg = sff->firstpage->next;
		free(sff->firstpage);
		sff->firstpage = pg;
	}
	TIFFClose(tf);
	dprint(MIDEBUG_NCCI, "Recoded %d pages as SFF size %zu (allocated %zu)\n", sff->page_cnt, sff->size, sff->data_size);
	return retval;
}
/* USE_SOFTFAX */
#endif
