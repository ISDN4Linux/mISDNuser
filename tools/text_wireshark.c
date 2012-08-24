/*
 *
 * Copyright 2012 Karsten Keil <kkeil@suse.de>
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


/*
 * This tool is not specific to mISDN it does read a D-channel trace from a hex/text
 * file into binary wireshark format
 *
 * Text line format:
 *
 * > FCFF030F01FF01FF     05.06. 13:23:33
 * < FEFF030F01FF0285     05.06. 13:23:33
 *
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <time.h>


static void usage(pname)
char *pname;
{
	fprintf(stderr,"\n\nCall with %s [options] <infile> <outfile>\n",pname);
	fprintf(stderr,"\n");
	fprintf(stderr,"\n     Valid options are:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -?              Usage ; printout this information\n");
	fprintf(stderr,"\n");
}


static void write_esc (FILE *file, unsigned char *buf, int len)
{
    int i, byte;

    for (i = 0; i < len; ++i) {
		byte = buf[i];
		if (byte == 0xff || byte == 0xfe) {
			fputc(0xfe, file);
			byte -= 2;
		}
		fputc(byte, file);
	}

	if (ferror(file)) {
		fprintf(stderr, "Error on writing to file!\nAborting...");
		exit(1);
	}
}

static void write_wfile(FILE *f, unsigned char *buf, int len, struct timeval *tv, int origin)
{
	u_char			head[12];

	fputc(0xff, f);
	head[0] = (unsigned char)(0xff & (tv->tv_usec >> 16));
	head[1] = (unsigned char)(0xff & (tv->tv_usec >> 8));
	head[2] = (unsigned char)(0xff & tv->tv_usec);
	head[3] = (unsigned char)0;
	head[4] = (unsigned char)(0xff & (tv->tv_sec >> 24));
	head[5] = (unsigned char)(0xff & (tv->tv_sec >> 16));
	head[6] = (unsigned char)(0xff & (tv->tv_sec >> 8));
	head[7] = (unsigned char)(0xff & tv->tv_sec);
	head[8] = (unsigned char) 0;
	head[9] = (unsigned char) origin & 0xff;
	head[10]= (unsigned char)(0xff & (len >> 8));
	head[11]= (unsigned char)(0xff & len);

	write_esc(f, head, 12);
	write_esc(f, buf, len);
	fflush(f);
}

static char *skip_space(char *buf) {
	while (*buf) {
		switch (*buf) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			buf++;
			break;
		default:
			return buf;
		}
	}
	return buf;
}

static int getvalue(char *p)
{
	char tmp[4];
	int v = -1;

	if (strlen(p) < 2)
		return -1;
	tmp[0] = p[0];
	tmp[1] = p[1];
	tmp[2] = 0;
	sscanf(tmp, "%x", &v);
	return v;
}

static int analyse_line(char *line, u_char *buf, int *org, struct tm *tm)
{
	char *p = line;
	int len = 0, val;

	p = skip_space(p);
	if (*p == 0)
		return len;
	if (*p == '<')
		*org = 0;
	else if (*p == '>')
		*org = 1;
	p++;
	p = skip_space(p);
	if (*p == 0)
		return len;
	while(*p) {
		if (isspace(*p))
			break;
		val = getvalue(p);
		if (val < 0)
			break;
		buf[len] = val;
		len++;
		p += 2;
	}
	p = skip_space(p);
	if (*p) {
		sscanf(p, "%2d.%2d. %2d:%2d:%2d", &tm->tm_mday, &tm->tm_mon, &tm->tm_hour, &tm->tm_min, &tm->tm_sec);
	}
	return len;
}

static void normalize_tv(struct timeval *tv)
{
	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

int
main(argc, argv)
int argc;
char *argv[];
{
	char	*infilename = NULL;
	char	*outfilename = NULL;
	char sw;
	char	line[4096];
	u_char  buffer[512];
	struct timeval main_tv, cur_tv;
	struct tm main_tm, cur_tm;
	time_t sec, last_sec;
	suseconds_t usec;
	int	len, org, aidx = 1;
	int	param = 0;
	FILE	*ifile, *ofile;

	while (aidx < argc) {
		if (argv[aidx] && argv[aidx][0]=='-') {
			sw=argv[aidx][1];
			switch (sw) {
			case '?':
				usage(argv[0]);
				exit(1);
				break;
			default:
				fprintf(stderr,"Unknown Switch %c\n",sw);
				usage(argv[0]);
				exit(1);
				break;
			}
		}  else {
			if (strlen(argv[aidx]) >= 512) {
				fprintf(stderr,"%s filename too long\n", param ? "out" : "in");
				exit(1);
			}
			if (param == 0) {
				infilename = argv[aidx];
			} else if (param == 1) {
				outfilename = argv[aidx];
			} else {
				fprintf(stderr,"Too many parameter (%d)  item (%s)\n", argc - 1,  argv[aidx]);
				usage(argv[0]);
				exit(1);
			}
			param++;
		}
		aidx++;
	}

	if (param < 2) {
		fprintf(stderr,"Only %d parameter given but need <infile> and <outfile>\n", param);
		exit(1);
	}

	ifile = fopen(infilename, "rt");
	if (!ifile) {
		fprintf(stderr,"cannot open %s for input - %s\n", infilename, strerror(errno));
		exit(1);
	} 

	ofile = fopen(outfilename, "w");
	if (!ofile) {
		fprintf(stderr,"cannot open %s for output - %s\n", outfilename, strerror(errno));
		fclose(ifile);
		exit(1);
	} 
	fprintf(ofile, "EyeSDN");
	fflush(ofile);

	gettimeofday(&main_tv, NULL);
	main_tv.tv_usec = 0;
	localtime_r(&main_tv.tv_sec, &main_tm);
	last_sec = 0;
	usec = 0;
	while (1) {
		if (!fgets(line, 4096, ifile)) {
			fprintf(stderr,"EOF or error reading file %s\n", infilename);
			break;
		}
		org = 0;
		cur_tm = main_tm;
		len = analyse_line(line, buffer, &org, &cur_tm);
		if (len) {
			sec = mktime(&cur_tm);
			if (sec != last_sec) {
				usec = 0;
				last_sec = sec;
			}
			cur_tv.tv_sec = sec;
			cur_tv.tv_usec = usec;
			usec += 1000 + 500 * len;
			normalize_tv(&cur_tv);
			main_tv = cur_tv;
			localtime_r(&main_tv.tv_sec, &main_tm);
			write_wfile(ofile, buffer, len, &cur_tv, org);
		}
	}
	fclose(ifile);
	fclose(ofile);
	return 0;
}
