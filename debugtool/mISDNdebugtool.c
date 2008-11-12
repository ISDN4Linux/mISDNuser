/*
 * mISDNdebugtool: Userspace counterpart of the mISDN_debugtool kernel module.
 *
 * Copyright (C) 2007, Nadi Sarrar
 *
 * Nadi Sarrar <nadi@beronet.com>
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/mISDNdebugtool.h>

#define BUFLEN 1024

static int arg_daemon = 0;
static int arg_verbose = 0;
static int arg_udp_port = 50501;
static int arg_dontenable = 0;
static int arg_size = 0;
static char *arg_ports = NULL;
static char *arg_dfile = NULL;
static char *arg_lfile = NULL;

static char usage[] =
"Usage: %s [-p <mISDN-port>,..] [-f <prefix>] [-l <prefix>] [-b <UDP-port>] [-d] [-n] [-v] [-h]\n"
"\n"
"Arguments:\n"
"  -p <mISDN-port>,..  mISDN ports to care for, default: care for all\n"
"  -f <prefix>         enable dumpfile mode, use this prefix for filenames\n"
"  -l <prefix>         enable logfile mode, use this prefix for filenames\n"
"  -s <size in kB>     max number of kB consumed by dump and logfiles combined\n"
"  -b <UDP-port>       UDP port to bind to, default: 50501\n"
"  -d                  daemon mode\n"
"  -n                  do not enable mISDN_debugtool kernel module\n"
"  -v                  print packets to stdout\n"
"  -h                  print this help text and exit\n";

struct port {
	int pnum;

	char dfn[256];
	FILE *df;
	int d_unclean_bytes;
	
	char lfn[256];
	FILE *lf;

	struct port *next;
};

struct port *ports = NULL;

static char *self;

static int disable_kernel_debugtool = 0;

static int max_bytes = 0;

static void kernel_debugtool_disable (void);

static void clean_exit (int code)
{
	struct port* p;
	struct stat st;

	for (p = ports; p; p = p->next) {
		if (p->df) {
			fflush(p->df);
			fclose(p->df);
			if (p->d_unclean_bytes) {
				if (!stat(p->dfn, &st)) {
//					printf("truncating file: %s from %ld to %ld bytes\n", p->dfn,
//							st.st_size, st.st_size - p->d_unclean_bytes);
					truncate(p->dfn, st.st_size - p->d_unclean_bytes);
				} else
					printf("failed to truncate file, so it may be unusable: %s\n", p->dfn);
			}
		}
		if (p->lf) {
			fflush(p->lf);
			fclose(p->lf);
		}
	}

	kernel_debugtool_disable();

	exit(code);
}

static void fail (char *err)
{
	fprintf(stderr, "ERROR: %s\n", err);
	clean_exit(1);
}

static void fail_perr (char *err)
{
	perror(err);
	clean_exit(1);
}

/* file helper */
static void _init_file (FILE **file, char *fn)
{
	*file = fopen(fn, "w");
	if (!*file || ferror(*file)) {
		fprintf(stderr, "ERROR: failed to open %s for writing!\n", fn);
		clean_exit(1);
	}
}

static void _consume_bytes (int bytes)
{
	if (!max_bytes)
		// we have no limit
		return;

	max_bytes -= bytes;
	if (max_bytes < 1) {
		printf("Exiting (max size reached)...\n");
		clean_exit(0);
	}
}

static void init_dfile (FILE **file, char *fn)
{
	_init_file(file, fn);
	_consume_bytes(6);
	fprintf(*file, "EyeSDN");
}

static void init_lfile (FILE **file, char *fn)
{
	_init_file(file, fn);
}

/* port filter */
static inline struct port* _get_port (int pnum)
{
	struct port *p = ports;

	for (; p; p = p->next)
		if (p->pnum == pnum)
			return p;

	return NULL;
}

static struct port* new_port (int pnum)
{
	struct port *p;

	if ((p = _get_port(pnum)))
		return p;

	p = calloc(1, sizeof(struct port));
	if (!p)
		fail_perr("calloc()");
	if (arg_dfile) {
		if (snprintf(p->dfn, sizeof(p->dfn), "%s-%d", arg_dfile, pnum) >= sizeof(p->dfn))
			fail("dumpfile prefix too long");
		init_dfile(&p->df, p->dfn);
	}

	if (arg_lfile) {
		if (snprintf(p->lfn, sizeof(p->lfn), "%s-%d", arg_lfile, pnum) >= sizeof(p->lfn))
			fail("logfile prefix too long");
		init_lfile(&p->lf, p->lfn);
	}

	p->pnum = pnum;
	p->next = ports;
	ports = p;

	return p;
}

static void init_ports (void)
{
	char *tok, *dup;
	int pnum;

	if (!arg_ports)
		return;

	dup = strdup(arg_ports);
	if (!dup)
		fail_perr("strdup()");
	
	while ((tok = strsep(&dup, ","))) {
		if (sscanf(tok, "%d", &pnum) == 1 && pnum > 0)
			new_port(pnum);
		else
			fail("port value incorrect");
	}
}

static struct port* get_port (int pnum)
{
	struct port *p = ports;

	p = _get_port(pnum);
	if (p)
		return p;

	if (!arg_ports)
		return new_port(pnum);
	
	return NULL;
}

static char *typestr (unsigned char type)
{
	static char *str[] = {
		"??",
		"D_RX",
		"D_TX",
		"L1_UP",
		"L1_DOWN",
		"CRC_ERR",
		"NEWSTATE",
	};

	if (type <= NEWSTATE)
		return str[type];
	return str[0];
}

static int write_esc (FILE *file, unsigned char *buf, int len)
{
    int i, byte;
	int tmplen = 0;
	unsigned char tmpbuf[2 * len];
    
    for (i = 0; i < len; ++i) {
		byte = buf[i];
		if (byte == 0xff || byte == 0xfe) {
			tmpbuf[tmplen++] = 0xfe;
			byte -= 2;
		}
		tmpbuf[tmplen++] = byte;
	}

	_consume_bytes(tmplen);
	fwrite(tmpbuf, tmplen, 1, file);
	if (ferror(file)) {
		fprintf(stderr, "Error on writing to file!\nAborting...");
		clean_exit(1);
	}

	return tmplen;
}

static int write_header (FILE *file, mISDN_dt_header_t *hdr)
{
    unsigned char buf[12];
	int usecs;
	unsigned long secs;
	int origin;
	
	if (hdr->stack_protocol & 0x10)
		origin = hdr->type == D_TX ? 0 : 1;
	else
		origin = hdr->type == D_TX ? 1 : 0;
	secs = hdr->time.tv_sec;
	usecs = hdr->time.tv_nsec / 1000;

    buf[0] = (unsigned char)(0xff & (usecs >> 16));
    buf[1] = (unsigned char)(0xff & (usecs >> 8));
    buf[2] = (unsigned char)(0xff & (usecs >> 0));
    buf[3] = (unsigned char)0;
    buf[4] = (unsigned char)(0xff & (secs >> 24));
    buf[5] = (unsigned char)(0xff & (secs >> 16));
    buf[6] = (unsigned char)(0xff & (secs >> 8));
    buf[7] = (unsigned char)(0xff & (secs >> 0));
    buf[8] = (unsigned char) 0;
    buf[9] = (unsigned char) origin;
    buf[10]= (unsigned char)(0xff & (hdr->plength >> 8));
    buf[11]= (unsigned char)(0xff & (hdr->plength >> 0));
    
    return write_esc(file, buf, 12);
}

static void log_packet (FILE *file, struct sockaddr_in *sock_client, mISDN_dt_header_t *hdr, unsigned char *buf)
{
	int i;
	int tmplen = 0;
	unsigned char tmpbuf[256 + 3 * hdr->plength];

	tmplen = sprintf(tmpbuf, "Received packet from %s:%d (vers:%d protocol:%s type:%s id:%08x plen:%d)\n%ld.%ld: ", 
		   inet_ntoa(sock_client->sin_addr),
		   ntohs(sock_client->sin_port),
		   hdr->version, hdr->stack_protocol & 0x10 ? "NT" : "TE",
		   typestr(hdr->type),
		   hdr->stack_id,
		   hdr->plength,
		   hdr->time.tv_sec,
		   hdr->time.tv_nsec);

	switch (hdr->type) {
	case D_RX:
	case D_TX:
		for (i = 0; i < hdr->plength; ++i)
			tmplen += sprintf(tmpbuf + tmplen, "%.2hhx ", *(buf + i));
		break;
	case NEWSTATE:
		tmplen += sprintf(tmpbuf + tmplen, "%u %s", *(unsigned int *)buf, buf + 4);
		break;
	default:
		break;
	}
	
	tmplen += sprintf(tmpbuf + tmplen, "\n\n");
	
	_consume_bytes(tmplen + 1);
	fwrite(tmpbuf, tmplen + 1, 1, file);
}

static inline void handle_packet (struct sockaddr_in *sock_client, mISDN_dt_header_t *hdr, unsigned char *buf)
{
	struct port *p = get_port(hdr->stack_id >> 8); 

	if (!p)
		return;

	if (arg_verbose)
		log_packet(stdout, sock_client, hdr, buf);

	if (p->lf) {
		log_packet(p->lf, sock_client, hdr, buf);
		fflush(p->lf);
	}

	if (p->df && (hdr->type == D_RX || hdr->type == D_TX)) {
		_consume_bytes(1);
		fputc(0xff, p->df);
		p->d_unclean_bytes = 1;
		p->d_unclean_bytes += write_header(p->df, hdr);
		write_esc(p->df, buf, hdr->plength);
		p->d_unclean_bytes = 0;
		fflush(p->df);
	}
}

static int kernel_debugtool_disabled (void)
{
	int e;

	FILE *enabled = fopen("/sys/class/mISDN-debugtool/enabled", "r");
	if (!enabled)
		fail_perr("fopen(\"/sys/class/mISDN-debugtool/enabled\")");

	if (fscanf(enabled, "%d", &e) != 1)
		fail("Could not get enabled status");

	fclose(enabled);

	return !e;
}

static void kernel_debugtool_echo (char *str)
{
	FILE *enabled = fopen("/sys/class/mISDN-debugtool/enabled", "w");
	if (!enabled)
		fail_perr("fopen(\"/sys/class/mISDN-debugtool/enabled\")");
	fprintf(enabled, str);
	fclose(enabled);
}

static void kernel_debugtool_enable (void)
{
	if (kernel_debugtool_disabled()) {
		disable_kernel_debugtool = 1;
		kernel_debugtool_echo("1");
	}
}

static void kernel_debugtool_disable (void)
{
	if (disable_kernel_debugtool)
		kernel_debugtool_echo("0");
}

static void sighandler (int signo)
{
	printf("Exiting (signal received)...\n");

	clean_exit(0);
}

int main (int argc, char *argv[])
{
	struct sockaddr_in sock_server;
	struct sockaddr_in sock_client;
	int s;
	socklen_t socklen = sizeof(struct sockaddr_in);
	char buf[BUFLEN];
	size_t size;
	int c;
	int failed = 0;
	int noargs = 1;

	self = argv[0];

	signal(SIGINT, sighandler);
	signal(SIGKILL, sighandler);
	signal(SIGTERM, sighandler);

	/* parse args */
	while ((c = getopt(argc, argv, "p:f:l:s:b:dnvh")) != -1) {
		noargs = 0;
		switch (c) {
		case 'p':
			if (!arg_ports)
				arg_ports = optarg;
			else {
				fprintf(stderr, "%s: argument given more than once -- p\n", self);
				failed = 1;
			}
			break;
		case 'f':
			if (!arg_dfile)
				arg_dfile = optarg;
			else {
				fprintf(stderr, "%s: argument given more than once -- f\n", self);
				failed = 1;
			}
			break;
		case 'l':
			if (!arg_lfile)
				arg_lfile = optarg;
			else {
				fprintf(stderr, "%s: argument given more than once -- l\n", self);
				failed = 1;
			}
			break;
		case 's':
			if (!optarg || sscanf(optarg, "%d", &arg_size) != 1 || arg_size < 1)
				fail("Invalid value for parameter -s");
			max_bytes = arg_size * 1024;
			break;
		case 'b':
			if (!optarg || sscanf(optarg, "%d", &arg_udp_port) != 1 || arg_udp_port < 1)
				fail("UDP port value incorrect");
			break;
		case 'd':
			arg_daemon = 1;
			break;
		case 'v':
			arg_verbose = 1;
			break;
		case 'n':
			arg_dontenable = 1;
			break;
		case 'h':
			printf(usage, self);
			exit(0);
			break;
		default:
			failed = 1;
		}
	}

	if (noargs || failed) {
		if (failed)
			fprintf(stderr, "\n");
		fprintf(failed ? stderr : stdout, usage, self);
		exit(failed ? -1 : 0);
	}

	if (!arg_verbose && !arg_dfile && !arg_lfile) {
		fprintf(stderr, "I ain't got things to do!\nPlease give me a job with -f, -l or -v.\n");
		exit(1);
	}

	if (arg_daemon && arg_verbose) {
		fprintf(stderr, "Option -v in combination with -d makes no sense.\n");
		exit(1);
	}

	if (!arg_dontenable)
		kernel_debugtool_enable();

	if (arg_daemon && daemon(1, 0))
		fail("daemon()");

	init_ports();

	if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		fail_perr("socket()");

	sock_server.sin_family = AF_INET;
	sock_server.sin_port = htons(arg_udp_port);
	sock_server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(s, (struct sockaddr *) &sock_server, socklen) < 0)
		fail_perr("bind()");

	for (;;) {
		size = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &sock_client, &socklen);
		if (size < 0)
			fail_perr("recvfrom()");
		if (size < sizeof(mISDN_dt_header_t)) {
			printf("Invalid Packet! (size(%d) < %d)\n", size, sizeof(mISDN_dt_header_t));
			continue;
		}
		
		mISDN_dt_header_t *hdr = (mISDN_dt_header_t *)buf;
		if (hdr->plength + sizeof(mISDN_dt_header_t) != size) {
			printf("Invalid Packet! (plen:%d, but size:%d)\n", hdr->plength, size);
			continue;
		}

		handle_packet(&sock_client, hdr, buf + sizeof(mISDN_dt_header_t));
	}

	printf("\nFailed!\n");

	return 0;
}
