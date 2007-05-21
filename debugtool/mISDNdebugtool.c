#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <linux/mISDNdebugtool.h>

#define BUFLEN 512

static FILE *file;
static char *self;

struct header {
    int usecs;
    unsigned long secs;
    int channel;
    int origin;
    int size;
};

static void init_file (char *fn)
{
	if (!fn) {
		fprintf(stderr, "Usage: %s <filename>\n", self);
		exit(1);
	}

	file = fopen(fn, "w");
	if (file && !ferror(file))
		printf("Writing trace to: %s\n", fn);
	else {
		fprintf(stderr, "Failed to open %s for writing!\n", fn);
		exit(1);
	}

	fprintf(file, "EyeSDN");
}

static void write_esc(unsigned char *buf, int len)
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

static void write_header(struct header *hp)
{
    unsigned char buf[12];
    
    buf[0] = (unsigned char)(0xff & (hp->usecs >> 16));
    buf[1] = (unsigned char)(0xff & (hp->usecs >> 8));
    buf[2] = (unsigned char)(0xff & (hp->usecs >> 0));
    buf[3] = (unsigned char)0;
    buf[4] = (unsigned char)(0xff & (hp->secs >> 24));
    buf[5] = (unsigned char)(0xff & (hp->secs >> 16));
    buf[6] = (unsigned char)(0xff & (hp->secs >> 8));
    buf[7] = (unsigned char)(0xff & (hp->secs >> 0));
    buf[8] = (unsigned char) hp->channel;
    buf[9] = (unsigned char) hp->origin;
    buf[10]= (unsigned char)(0xff &(hp->size >> 8));
    buf[11]= (unsigned char)(0xff &(hp->size >> 0));
    
    return write_esc(buf, 12);
}

static void write_packet (struct header *hdr, unsigned char *buf)
{
	fputc(0xff, file);
	write_header(hdr);
	write_esc(buf, hdr->size);
	fflush(file);
}

static void sigint (int signo)
{
	printf("Exiting ...\n");
	fflush(file);
	fclose(file);
	exit(0);
}

int main (int argc, char *argv[])
{
	struct sockaddr_in sock_server;
	struct sockaddr_in sock_client;
	int s;
	socklen_t socklen = sizeof(struct sockaddr_in);
	char buf[BUFLEN];
	size_t size;
	int i;

	self = argv[0];

	signal(SIGINT, sigint);

	init_file(argc > 1 ? argv[1] : NULL);

	if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket()");
		exit(-1);
	}

	sock_server.sin_family = AF_INET;
	sock_server.sin_port = htons(PORT);
	sock_server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(s, (struct sockaddr *) &sock_server, socklen) < 0) {
		perror("bind()");
		exit(-1);
	}

	for (;;) {
		size = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &sock_client, &socklen);
		if (size < 0) {
			perror("recvfrom()");
			exit(-1);
		}
		if (size < sizeof(mISDN_dt_header_t)) {
			printf("Invalid Packet! (size(%d) < %d)\n", size, sizeof(mISDN_dt_header_t));
			continue;
		}
		
		mISDN_dt_header_t *hdr = (mISDN_dt_header_t *)buf;
		if (hdr->plength + sizeof(mISDN_dt_header_t) != size) {
			printf("Invalid Packet! (plen:%d, but size:%d)\n", hdr->plength, size);
			continue;
		}

#ifdef VERBOSE
		printf("Received packet from %s:%d (vers:%d type:%s,%s id:%08x plen:%d)\n%ld.%ld: ", 
			   inet_ntoa(sock_client.sin_addr),
			   ntohs(sock_client.sin_port),
			   hdr->version, hdr->stack_protocol & 0x10 ? "NT" : "TE",
			   hdr->type == D_RX ? "D_RX" : hdr->type == D_TX ? "D_TX" : "??", hdr->stack_id, hdr->plength,
			   hdr->time.tv_sec, hdr->time.tv_nsec);
		for(i = 0; i < hdr->plength; ++i)
			printf("%.2hhx ", *(buf + sizeof(mISDN_dt_header_t) + i));
		printf("\n\n");
#endif

		{
			struct header dump_hdr;
			dump_hdr.channel = 0;
			if (hdr->stack_protocol & 0x10)
				dump_hdr.origin = hdr->type == D_TX ? 0 : 1;
			else
				dump_hdr.origin = hdr->type == D_TX ? 1 : 0;
			dump_hdr.size = hdr->plength;
			dump_hdr.secs = hdr->time.tv_sec;
			dump_hdr.usecs = hdr->time.tv_nsec / 1000;

			write_packet(&dump_hdr, buf + sizeof(mISDN_dt_header_t));
		}
	}

	printf("\nFailed!\n");

	return 0;
}
