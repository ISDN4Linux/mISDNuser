#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helper.h"
#include "g711.h"
#include "isdn_net.h"
#include "isound.h"
#include "rtpacket.h"
#include "iapplication.h"


#define RANDOM_DEVICE	"/dev/urandom"

#define RTP_PAD_FLAG	0x20

static int
init_voipsocks(vapplication_t *v)
{
	v->dsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (v->dsock < 0) {
            perror("opening data socket");
	    return 1;
	}
	v->csock = socket(AF_INET, SOCK_DGRAM, 0);
	if (v->csock < 0) {
            perror("opening control socket");
	    return 1;
	}

	if (v->csock < v->dsock) {
		int tmpsock = v->dsock;
		v->dsock = v->csock;
		v->csock = tmpsock;
	}
	
	/* Create caddr/daddr with wildcards. */

	v->daddr.sin_family = v->caddr.sin_family = AF_INET;
	v->daddr.sin_addr.s_addr = v->caddr.sin_addr.s_addr = INADDR_ANY;
	v->daddr.sin_port = htons(v->port);
	if (bind(v->dsock, (struct sockaddr *) &v->daddr,
		sizeof(struct sockaddr_in)) < 0) {
		perror("binding data socket");
		return 1;
	}
	v->caddr.sin_port = htons(v->port + 1);
	if (bind(v->csock, (struct sockaddr *) &v->caddr,
		sizeof(struct sockaddr_in)) < 0) {
		perror("binding control socket");
		return 1;
	}
	dprint(DBGM_SOCK, "socket ports #%d/#%d\n",
		ntohs(v->daddr.sin_port), ntohs(v->caddr.sin_port));
	return(0);
}

int
SendBye(iapplication_t *ia, char *bye)
{
	vconnection_t	*c;
	int		len;

	c = ia->con;
	if (!c)
		return(-EINVAL);
	if (ia->Flags & (AP_FLG_VOIP_SENT_BYE | AP_FLG_VOIP_PEER_BYE))
		return(0);
	len = rtp_make_bye(c->cbuf, c->own_ssrc, bye, 1);
	dprint(DBGM_SOCK, "C socket send %d bytes to %s\n",
		len, inet_ntoa(c->cpeer.sin_addr));
	ia->Flags |= AP_FLG_VOIP_SENT_BYE;
	if (len) {
		dhexprint(DBGM_CDATA, "send ctrl packet:", c->cbuf, len);
		len = sendto(c->sock, c->cbuf, len, 0,
			(struct sockaddr *)&c->cpeer, sizeof(c->cpeer));
		if (len < 0) {
			eprint("cannot send ctrl msg errno(%d)\n", errno);
			return(-errno);
		}
	} else {
		eprint("cannot compose Bye message\n");
		return(-EINVAL);
	}
	return(0);
}

void
clear_connection(iapplication_t *ia)
{
	vconnection_t	*c = ia->con;

	if (c) {
		dprint(DBGM_SOCK, "clear connection to %s ssrc(%lx/%lx)\n",
			c->rmtname, c->own_ssrc, c->peer_ssrc);
		if (ia->Flags & AP_FLG_VOIP_PEER_VALID) {
			msg_queue_purge(&c->aqueue);
			if (c->amsg)
				free_msg(c->amsg);
			SendBye(ia, "closing");
		}
		if (c->sock)
			close(c->sock);
#ifdef GSM_COMPRESSION
		if (c->r_gsm)
			gsm_destroy(c->r_gsm);
		if (c->s_gsm)
			gsm_destroy(c->s_gsm);
#endif
		free(c);
		ia->con = NULL;
	}
}

void
free_application(iapplication_t *ia)
{
	vconnection_t	*c = ia->con;

	if (!ia)
		return;
	if (c)
		clear_connection(ia);
	REMOVE_FROM_LISTBASE(ia, ia->vapp->iapp_lst);
	free(ia);
}

static void
close_voipsocks(vapplication_t *v)
{
	dprint(DBGM_SOCK, "socket closed\n");
	while(v->iapp_lst) {
		free_application(v->iapp_lst);
	}
	close(v->csock);
	close(v->dsock);
	v->csock = 0;
	v->dsock = 0;
}

static iapplication_t *
get_connection(vapplication_t *v,  unsigned long ssrc, int exact)
{
	iapplication_t	*ip;

	ip = v->iapp_lst;
	while (ip) {
		if (ip->con) {
			dprint(DBGM_SOCK, "ip(%p) %x %s/%s %x/%x\n",
				ip, ip->Flags,
				inet_ntoa(v->from.sin_addr),
				inet_ntoa(ip->con->cpeer.sin_addr),
				ssrc, ip->con->peer_ssrc);
			if (memcmp(&v->from.sin_addr, &(ip->con->cpeer.sin_addr),
				sizeof(struct in_addr)) == 0) {
				if (ip->Flags & AP_FLG_VOIP_PEER_VALID) {
					if (ip->Flags & AP_FLG_VOIP_PEER_SF) {
						return(ip);
					} else if (ip->con->peer_ssrc == ssrc) {
						return(ip);
					}
				} else if (!exact) {
					if (!get_connection(v, ssrc, 1)) {
						ip->con->peer_ssrc = ssrc;
						ip->Flags |= AP_FLG_VOIP_PEER_VALID;
						return(ip);
					}
				}
			}
		}
		ip = ip->next;
	}
	return(NULL);
}

unsigned long
my_random_ul(void) {
	int		rd;
	unsigned long	r;

	rd = open(RANDOM_DEVICE, O_RDONLY);
	if (rd<0)
		return(random());
	read(rd, &r, sizeof(r));
	close(rd);
	return(r);
}
 
unsigned long
getnew_ssrc(vapplication_t *v) 
/* this excludes 0 as value, but I think its OK */
{
	unsigned long	ssrc = 0;
	iapplication_t	*ip;
	
	while(!ssrc) {
		ssrc = my_random_ul();		
		ip = v->iapp_lst;
		while (ip) {
			if (ip->con) {
				if (ip->con->peer_ssrc == ssrc) {
					ssrc = 0;
					break;
				}
				if (ip->con->own_ssrc == ssrc) {
					ssrc = 0;
					break;
				}
			}
			ip = ip->next;
		}
	}
	return(ssrc);
}

vconnection_t *
new_connection(iapplication_t *ia, struct in_addr *addr)
{
	vconnection_t	*c;
	
	c = malloc(sizeof(vconnection_t));
	if (!c)
		return(NULL);
	memset(c, 0, sizeof(vconnection_t));
	msg_queue_init(&c->aqueue);
	c->sock = socket(AF_INET, SOCK_DGRAM, 0);
	c->cpeer.sin_family = c->dpeer.sin_family = AF_INET;
	memcpy(&c->cpeer.sin_addr, addr, sizeof(struct in_addr));
	memcpy(&c->dpeer.sin_addr, addr, sizeof(struct in_addr));
	c->dpeer.sin_port = htons(ia->vapp->port);
	c->cpeer.sin_port = htons(ia->vapp->port + 1);
#if 0
	/* Compute the number of sound samples needed to fill a
	   packet of TINY_PACKETS bytes. */

	if (rtp) {
		sound_packet = (gsmcompress | lpccompress | adpcmcompress) ? (160 * 4)
					   : 320;
	} else if (vat) {
		sound_packet = (gsmcompress | lpccompress | adpcmcompress) ? (160 * 4)
					   : 320;
	} else {
		sound_packet = ((TINY_PACKETS - ((sizeof(soundbuf) - BUFL))) *
						(compressing ? 2 : 1));
		if (gsmcompress) {
			sound_packet = compressing ? 3200 : 1600;
		} else if (adpcmcompress) {
			sound_packet *= 2;
			sound_packet -= 4;				  /* Leave room for state at the end */
		} else if (lpccompress) {
			sound_packet = 10 * LPC_FRAME_SIZE;
		} else if (lpc10compress) {
			sound_packet = compressing ? 3600 : 1800;
		}
	}

#ifdef SHOW_PACKET_SIZE
	printf("Samples per packet = %d\n", sound_packet);
#endif
	lread = sound_packet;
#endif
	/* default size */
	c->pkt_size = 320;
	return(c);
}

iapplication_t *
new_application(vapplication_t *v)
{
	iapplication_t  *ip;

	ip = malloc(sizeof(iapplication_t));
	if (!ip)
		return(NULL);
	memset(ip, 0, sizeof(iapplication_t));
	ip->vapp = v;
	APPEND_TO_LIST(ip, v->iapp_lst);
	return(ip);
}

static iapplication_t *
new_peer_connection(vapplication_t *v, unsigned long ssrc, int version)
{
	iapplication_t	*ip;
	vconnection_t	*c;
	struct hostent	*h;

	ip = new_application(v);
	if (!ip)
		return(NULL);
	c = new_connection(ip, &v->from.sin_addr);
	if (!c) {
		free_application(ip);
		return(NULL);
	}
	h = gethostbyaddr((char *) &v->from.sin_addr, sizeof(struct in_addr),
		AF_INET);
	if (h == NULL) {
		strcpy(c->rmtname, inet_ntoa(v->from.sin_addr));
	} else {
		strcpy(c->rmtname, h->h_name);
	}
	strncpy(c->con_hostname, c->rmtname, 31);
	c->peer_ssrc = ssrc;
	c->own_ssrc = getnew_ssrc(v);
	ip->Flags |= AP_FLG_VOIP_PEER_VALID;
	if (version == 1) /* speakfreely without RTP */
		ip->Flags |= AP_FLG_VOIP_PEER_SF;
	ip->con = c;
	return(ip);
}

static int
play_data(iapplication_t *ip)
{
	isound_t	*is = ip->data2;
	int		maxlen;
	unsigned char	*buf = ip->con->rbuf;

	if (!is || !is->rbuf) {
		wprint("ip->data2 %p\n", is);
		return(-1);
	}
	maxlen = ibuf_freecount(is->rbuf);
	dprint(DBGM_SOUND, "%s: %d data max(%d)\n", __FUNCTION__,
		ip->con->rlen, maxlen);
	if (maxlen > ip->con->rlen)
		maxlen = ip->con->rlen;
	else if (maxlen < ip->con->rlen) {
		dprint(DBGM_SOUND, "pb shortage, skip %d bytes (%d/%d/%d/%d/%d)\n",
			ip->con->rlen - maxlen, ip->con->rlen, maxlen,
			is->rbuf->ridx, is->rbuf->widx, is->rbuf->size);
		wprint("playbuffer shortage, skip %d bytes\n",
			ip->con->rlen - maxlen);
		buf += (ip->con->rlen - maxlen);
	}
	if (maxlen)
		ibuf_memcpy_w(is->rbuf, buf, maxlen);
	if (is->rbuf->rsem)
		sem_post(is->rbuf->rsem);
	return(maxlen);
}

static int
receive_data(vapplication_t *v) {
	iapplication_t	*iap;
	unsigned long	ssrc, ts;
	unsigned short	seq;
	unsigned char	cc, payl;
	unsigned char	ver;
	rtp_hdr_t	*rh;

	if (v->rlen < 12)
		return(-2);
	rh = (rtp_hdr_t *)&v->buf.d;
	ver = (v->buf.d[0] >> 6) & 3;
	if (ver != RTP_VERSION) {
		dprint(DBGM_SOCK, "rtp version %d\n",
			ver);
		return(-3);
	}
	ssrc = ntohl(rh->ssrc);
	iap = get_connection(v, ssrc, 1);
	if (!iap) { /* data from not known source ignored before a CTRL packet */
		dprint(DBGM_SOCK, "rtp data ignored from %s ssrc(%lx)\n",
			inet_ntoa(v->from.sin_addr), ssrc);
		return(-4);
	}
	cc = v->buf.d[0] & 0xf;
	payl = v->buf.d[1] & 0x7f;
	seq = ntohs(rh->seq);
	ts = ntohl(rh->ts);
	iap->con->rlen = v->rlen - 4*cc - 12;
	if (v->buf.d[0] & RTP_PAD_FLAG) {
		iap->con->rlen -= v->buf.d[v->rlen-1]; 
	}
	dprint(DBGM_SOCK, "rtp data len(%d) pl(%x) seq(%d) ts(%lx)\n",
		iap->con->rlen, payl, seq, ts);
	if (iap->con->rlen <= 0) {
		dprint(DBGM_SOCK, "rtp data len error %d\n", iap->con->rlen);
		return(-5);
	}
	iap->con->rbuf = v->buf.d + 4*cc + 12;
	if (payl == 8) { /* alaw */
		/* default */
	} else if (payl == 0) { /* ulaw */
		int	i;

		for (i=0;i<iap->con->rlen;i++)
			iap->con->rbuf[i] = ulaw2alaw(iap->con->rbuf[i]);
#ifdef GSM_COMPRESSION
	} else if (payl == 3) { /* GSM */
		int	i;
		gsm_signal	gs[640], *gp;
		u_char		buf[640], *p;

		if (!(iap->con->sndflags & SNDFLG_COMPR_GSM)) {
			iap->con->sndflags |= SNDFLG_COMPR_GSM;
			iap->con->pkt_size = 640;
		}
		if (!iap->con->r_gsm)
			iap->con->r_gsm = gsm_create();
		if (iap->con->rlen != 4*33) {
			eprint("%s wrong GSM Data size %d/%d\n", __FUNCTION__,
				iap->con->rlen, 4*33);
			return(-6);
		}
		gp = gs;
		p = iap->con->rbuf;
		for (i=0;i<4; i++) {
			gsm_decode(iap->con->r_gsm, p, gp);
			p += 33;
			gp += 160;
		}
		gp = gs;
		p = iap->con->rbuf = buf;
		iap->con->rlen = 640;
		for (i=0;i<640;i++)
			*p++ = linear2alaw(*gp++);
#endif
	} else {
		dprint(DBGM_SOCK, "rtp data payload %x not supported\n", payl);
		return(-7);
	}
	return(play_data(iap));
}

static int
receive_ctrl(vapplication_t *v) {
	iapplication_t	*iap;
	unsigned char	*app;
	unsigned long	ssrc;
	int		ver;

	if (v->rlen < 8)
		return(1);
	ver = (v->buf.d[0] >> 6) & 3;
	if ((ver != RTP_VERSION) && (ver != 1)) {
		dprint(DBGM_SOCK, "rtp version %d\n",
			ver);
		return(2);
	}
	ssrc = ntohl(*((unsigned long *)&v->buf.d[4]));
	iap = get_connection(v, ssrc, 0);
	if (!iap) { /* New connection */
		if (isRTCPByepacket(v->buf.d, v->rlen)) {
			dprint(DBGM_CONN, "bye in new connection from %s ignored\n",
				inet_ntoa(v->from.sin_addr));
			return(3);
		}
		dprint(DBGM_CONN, "new connection from %s ssrc(%x)\n",
			inet_ntoa(v->from.sin_addr), ssrc);
		iap = new_peer_connection(v, ssrc, ver);
		if (!iap) {
			return(4);
		}
		voip_application_handler(iap, AP_PR_VOIP_NEW, NULL);
	} else {
		if (isRTCPByepacket(v->buf.d, v->rlen)) {
			iap->Flags |= AP_FLG_VOIP_PEER_BYE;
			dprint(DBGM_CONN, "connection from %s bye\n",
				inet_ntoa(v->from.sin_addr));
		}
	}
	if (isRTCPAPPpacket(v->buf.d, v->rlen, "ISDN", &app)) {
		return(voip_application_handler(iap, AP_PR_VOIP_ISDN, app));
	}
	if (iap->Flags & AP_FLG_VOIP_PEER_BYE) {
		clear_connection(iap); 
		voip_application_handler(iap, AP_PR_VOIP_BYE, NULL);
	}
	return(0);
#if 0
	/* See if this connection is active.  If not, initialise a new
	   connection. */

	busyreject = FALSE;
	newconn = FALSE;
	c = conn;
	while (c != NULL) {
		if (memcmp(&from.sin_addr, &(c->con_addr),
			   sizeof(struct in_addr)) == 0) {
		break;
		}
		c = c->con_next;
	}
	/* Initialise fields in connection.  Only fields which need to
	   be reinitialised when a previously idle host resumes activity
	   need be set here. */

	if (newconn) {
		c->face_file = NULL;
		c->face_filename[0] = 0;
		c->face_viewer = 0;
		c->face_stat = FSinit;
		c->face_address = 0L;
		c->face_retry = 0;
		c->con_compmodes = -1;
		c->con_protocol = PROTOCOL_UNKNOWN;
		c->con_rseq = -1;
		c->con_reply_current = FALSE;
		c->con_busy = 0;
			bcopy("\221\007\311\201", c->con_session, 4);
		lpc_init(&c->lpcc);
		busyreject = isBusy();
	}

	if (c != NULL) {
		/* Reset connection timeout. */
		c->con_timeout = 0;

		if (newconn) {
		if (showhosts) {
			fprintf(stdout, "%s: %s %s %s\n", prog, etime(), c->con_hostname,
				busyreject ? "sending busy signal" : "connect");
		}
		}
		if (busyreject) {
		continue;
		}

		/* Request face data from the other end, starting with
		   block zero.  If the connection was created itself
			   by a face data request, don't request the face from
		   the other end; wait, instead, for some sound to
		   arrive.	We use face_stat to decide when to make the
		   request rather than newconn, since a connection may
		   have been created by a face request from the other
			   end, which didn't trigger a reciprocal request by
		   us. */

		if (!control && (c->con_protocol == PROTOCOL_SPEAKFREE) &&
			(c->face_stat == FSinit) &&
			isSoundPacket(ntohl(sb.compression)) &&
			(ntohl(sb.compression) & fFaceOffer)) {
		c->face_address = 0;
		c->face_timeout = 0;
		c->face_retry = 0;
		c->face_stat = FSreply;   /* Activate request from timeout */
		faceTransferActive++;	  /* Mark face transfer underway */
		if (faceTransferActive == 1) {
			windtimer();	  /* Set timer to fast cadence */
		}
		}

	} else {
		continue;
	}

	wasrtp = FALSE;

#ifdef CRYPTO

		/* If a DES key is present and we're talking RTP or VAT
	   protocol we must decrypt the packet at this point.
	   We decrypt the packet if:

		1.  A DES key was given on the command line, and
			either:

			a)	The packet arrived on the control port
			(and hence must be from an RTP/VAT client), or

			b)	The protocol has already been detected as
			RTP or VAT by reception of a valid control
			port message.  */

	if ((control || (c->con_protocol == PROTOCOL_RTP) ||
			(c->con_protocol == PROTOCOL_VAT)) &&
		 rtpdeskey[0]) {

		/* One more little twist.  If this packet arrived on the
		   control channel, see if it passes all the tests for a
			   valid RTCP packet.  If so, we'll assume it isn't
		   encrypted.  RTP utilities have the option of either
		   encrypting their control packets or sending them in
		   the clear, so a hack like this is the only way we have
		   to guess whether something we receive is encrypted. */

		if (!isValidRTCPpacket((unsigned char *) &sb, rll)) {
		des_key_schedule sched;
		des_cblock ivec;
		int drll = rll;
		char *whichkey;
		static int toggle = 0;

		bzero(ivec, 8);
		drll = (rll + 7) & (~7);
		if (Debug) {
					fprintf(stdout, "Decrypting %d VAT/RTP bytes with DES key.\r\n",
				drll);
		}
		if (drll > rll) {
			/* Should only happen for VAT protocol.  Zero the rest of
			   the DES encryption block to guarantee consistency. */
			bzero(((char *) &sb) + rll, drll - rll);
		}

		/* If the protocol is unknown, toggle back and forth
		   between the RTP and VAT DES keys until we crack the
		   packet and set the protocol. */

		if (c->con_protocol == PROTOCOL_UNKNOWN || 
			c->con_protocol == PROTOCOL_VATRTP_CRYPT ||
			c->con_protocol == PROTOCOL_SPEAKFREE) {
			whichkey = toggle == 0 ? vatdeskey :
				   (toggle == 1 ? rtpdeskey : NULL);
			toggle = (toggle + 1) % 3;
		} else {
			whichkey = c->con_protocol == PROTOCOL_VAT ?
				vatdeskey : rtpdeskey;
		}
		if (whichkey != NULL) {
			des_set_key((des_cblock *) (whichkey + 1), sched);
			des_ncbc_encrypt((des_cblock *) &sb,
			(des_cblock *) &sb, rll, sched,
			(des_cblock *) ivec, DES_DECRYPT);

			/* Just one more thing.  In RTP (unlike VAT), when
			   an RTCP control packet is encrypted, 4 bytes of
			   random data are prefixed to the packet to prevent
			   known-plaintext attacks.  We have to strip this
			   prefix after decrypting. */

			if (control && ((*(((char *) &sb) + 4) & 0xC0) == 0x80)) {
			rll -= 4;
			bcopy(((char *) &sb) + 4, (char *) &sb, rll);
			}
		}
#ifdef HEXDUMP
		if (hexdump) {
			xd(stdout, (unsigned char *)&sb, rll, TRUE);
		}
#endif
		}
	}
#endif

	/* If this packet arrived on the session control port, dispatch
	   it to the appropriate handler for its protocol. */

	if (control) {
		short protocol = PROTOCOL_VATRTP_CRYPT;
		unsigned char *p = (unsigned char *) &sb;
		unsigned char *apkt;
		int proto = (p[0] >> 6) & 3;

		if (proto == 0) {		  /* VAT */
		/* To avoid spoofing by bad encryption keys, require
				   a proper ID message be seen before we'll flip into
		   VAT protocol. */
		if (((p[1] == 1) || (p[1] == 3)) ||
			((c->con_protocol == PROTOCOL_VAT) && (p[1] == 2))) {
			protocol = PROTOCOL_VAT;
			bcopy(p + 2, c->con_session, 2);  /* Save conference ID */

			if (p[1] == 1 && showhosts) {
			char uname[256];

			bcopy(p + 4, uname, rll - 4);
			uname[rll - 4] = 0;
			if (strcmp(uname, c->con_uname) != 0) {
				strcpy(c->con_uname, uname);
				fprintf(stdout, "%s: %s sending from %s.\n", prog,
					c->con_uname, c->con_hostname);
			}
			}

			/* Handling of VAT IDLIST could be a lot more elegant
			   than this. */

			if (p[1] == 3 && showhosts) {
			char *uname;

			uname = (char *) malloc(rll);
			if (uname != NULL) {
				unsigned char *bp = p, *ep = p + rll;
				int i = bp[4];

				bp += 8;
				uname[0] = 0;
				*ep = 0;
				while (--i >= 0 && bp < ep) {
				bp += 4;
								strcat(uname, "\t");
				strcat(uname, (char *) bp);
				while (isspace(uname[strlen(uname) - 1])) {
					uname[strlen(uname) - 1] = 0;
				}
								strcat(uname, "\n");
				bp += (strlen((char *) bp) + 3) & ~3;
				}
				if (strncmp(uname, c->con_uname, (sizeof c->con_uname - 1)) != 0) {
				strncpy(c->con_uname, uname, sizeof c->con_uname);
				if (strlen(uname) > ((sizeof c->con_uname) - 1)) {
					c->con_uname[((sizeof c->con_uname) - 1)] = 0;
				}
				fprintf(stdout, "%s: now in conference at %s:\n%s", prog,
					c->con_hostname, uname);
				}
				free(uname);
			}
			}

			/* If it's a DONE packet, reset protocol to unknown. */

			if (p[1] == 2) {
				c->con_protocol = protocol = PROTOCOL_UNKNOWN;
				c->con_timeout = hosttimeout - 1;
				c->con_uname[0] = c->con_email[0] = 0;
				if (showhosts) {
					fprintf(stdout, "%s: %s VAT connection closed.\n",
						prog, c->con_hostname);
				}
			}
		}

		} else if (proto == RTP_VERSION || proto == 1) { /* RTP */
		if (isValidRTCPpacket((unsigned char *) &sb, rll)) {
			protocol = (proto == 1) ? PROTOCOL_SPEAKFREE : PROTOCOL_RTP;
			bcopy(p + 4, c->con_session, 4);  /* Save SSRC */

					/* If it's a BYE packet, reset protocol to unknown. */

			if (isRTCPAPPpacket((unsigned char *) &sb, rll,
					RTCP_APP_TEXT_CHAT, &apkt) && apkt != NULL) {
			char *ident = c->con_hostname;

			/* To identify the sender, get successively more
			   personal depending on the information we have at
			   hand, working down from hostname (which may just
						   be an IP address if we couldn't resolve the host,
			   through E-mail address, to user name. */

			if (c->con_email[0] != 0) {
				ident = c->con_email;
			}
			if (c->con_uname[0] != 0) {
				ident = ident = c->con_uname;
			}

						printf("%s: %s\n", ident, (char *) (apkt + 12));

					/* Otherwise, it's presumably an SDES, from which we
			   should update the user identity information for the
			   connection. */

			} else {
			struct rtcp_sdes_request rp;

			rp.nitems = 5;
			rp.item[0].r_item = RTCP_SDES_CNAME;
			rp.item[1].r_item = RTCP_SDES_NAME;
			rp.item[2].r_item = RTCP_SDES_EMAIL;
			rp.item[3].r_item = RTCP_SDES_TOOL;
			rp.item[4].r_item = RTCP_SDES_NOTE;
			if (parseSDES((unsigned char *) &sb, &rp)) {
				char uname[256], email[256];

				uname[0] = email[0] = 0;
				if (rp.item[1].r_text != NULL) {
				copySDESitem(rp.item[1].r_text, uname);
				if (rp.item[2].r_text != NULL) {
					copySDESitem(rp.item[2].r_text, email);
				} else if (rp.item[2].r_text != NULL) {
					copySDESitem(rp.item[0].r_text, email);
				}
				} else if (rp.item[2].r_text != NULL) {
				copySDESitem(rp.item[2].r_text, uname);
				} else if (rp.item[0].r_text != NULL) {
				copySDESitem(rp.item[0].r_text, uname);
				}
				if (rp.item[4].r_text != NULL) {
				copySDESitem(rp.item[4].r_text, hm_note);
				h_appl_mgr(1, hm_note, c->con_hostname);
				}
				if (strcmp(uname, c->con_uname) != 0 ||
				strcmp(email, c->con_email) != 0) {
				strcpy(c->con_uname, uname);
				strcpy(c->con_email, email);
				if (showhosts && uname[0]) {
									fprintf(stdout, "%s: %s", prog, uname);
					if (email[0]) {
									  fprintf(stdout, " (%s)", email);
					}
									fprintf(stdout, " sending from %s.\n",
						c->con_hostname);
				}
				}
			}
			}
		} else {
			if (Debug) {
						fprintf(stdout, "Invalid RTCP packet received.\n");
			}
		}
		} else {
		if (Debug) {
					fprintf(stdout, "Bogus protocol 3 control message.\n");
		}
		}

		/* If protocol changed, update in connection and, if appropriate,
		   update the reply command. */

		if (protocol != c->con_protocol) {
		static char *pname[] = {
					"Speak Freely",
					"VAT",
					"RTP",
					"VAT/RTP encrypted",
					"Unknown"
		};

		c->con_protocol = protocol;
		if (showhosts) {
					fprintf(stdout, "%s: %s sending in %s protocol.\n",
				prog, c->con_hostname, pname[protocol]);
		}
		c->con_reply_current = FALSE;
		}
		continue;
	}

	/* If this message is tagged with our Speak Freely protocol
	   bit, force protocol back to Speak Freely.  This allows us
	   to switch back to Speak Freely after receiving packets in
	   VAT.  We can still get confused if we receive a packet from
		   an older version of Speak Freely that doesn't tag. */

	if (c->con_protocol == PROTOCOL_VAT ||
		c->con_protocol == PROTOCOL_VATRTP_CRYPT) {
		unsigned char *p = (unsigned char *) &sb;

		if (((p[0] >> 6) & 3) == 1) {
		c->con_protocol = PROTOCOL_SPEAKFREE;
		}
	}

	/* If this is a VAT packet, translate it into a sound buffer. */

	if (((c->con_protocol == PROTOCOL_VAT)) &&
		(bcmp(((unsigned char *) &sb) + 2, c->con_session, 2) == 0) &&
		isvat((unsigned char *) &sb, rll)) {
		if (sb.buffer.buffer_len == 0) {
		if (Debug) {
					fprintf(stdout, "Ignoring unparseable VAT packet.\n");
		}
		continue;
		}
		wasrtp = TRUE;

	/* If this is an RTP packet, transmogrify it into a sound
	   buffer we can understand. */

	} else if ((c->con_protocol == PROTOCOL_RTP) &&
		 (bcmp(((unsigned char *) &sb) + 8, c->con_session, 4) == 0) &&
		 isrtp((unsigned char *) &sb, rll)) {
		if (sb.buffer.buffer_len == 0) {
		if (Debug) {
					fprintf(stdout, "Ignoring unparseable RTP packet.\n");
		}
		continue;
		}
		wasrtp = TRUE;
	}

	if (!wasrtp) {
		int xbl;

		/* Convert relevant fields from network to host
		   byte order, if necessary. */

		sb.compression = ntohl(sb.compression);
		sb.buffer.buffer_len = ntohl(sb.buffer.buffer_len);

		if (sb.compression & fCompRobust) {
		int aseq = (sb.buffer.buffer_len >> 24) & 0xFF;

		if (aseq == c->con_rseq) {
			continue;
		}
		c->con_rseq = aseq;
		sb.buffer.buffer_len &= 0xFFFFFFL;
		}

		/* Now if this is a valid Speak Freely packet (as
		   opposed to a VAT packet masquerading as one, or
		   an encrypted VAT or RTP packet we don't have the
		   proper key to decode), the length received from the
		   socket will exactly equal the buffer length plus
		   the size of the header. This is a reasonably
		   good validity check, well worth it considering the
		   horrors treating undecipherable garbage as a sound
		   buffer could inflict on us. */

		xbl = sb.buffer.buffer_len + (sizeof(struct soundbuf) - BUFL);

		/* If this packet is encrypted with an algorithm which requires
		   padding the packet to an 8-byte boundary, adjust the actual
		   content length to account for the padding. */

		if ((sb.compression & (fEncDES | fEncIDEA | fEncBF | fEncPGP)) != 0) {
			xbl = ((sb.buffer.buffer_len + 7) & (~7)) +
				(sizeof(struct soundbuf) - BUFL);
		}

		/* If packet is compressed with LPC-10, compensate for "packet
		   stuffing". */

		if ((sb.compression & fCompLPC10) && (sb.buffer.buffer_len >= 16)) {
			xbl -= 16;
		}
		if (xbl != rll) {
			if (Debug) {
				fprintf(stdout,
					"Sound buffer length %d doesn't match %d byte packet. Hdr=%08lX\n",
					xbl, rll, sb.compression);
			}
			if (showhosts && c->con_protocol != PROTOCOL_UNKNOWN) {
				fprintf(stdout, "%s: %s sending in unknown protocol or encryption.\n",
					prog, c->con_hostname);
			}
			c->con_protocol = PROTOCOL_UNKNOWN;
			continue;
		}

		/* It does appear to be a genuine Speak Freely sound
		   buffer. On that basis, set the protocol to Speak Freely
		   even if the buffer isn't explicitly tagged. */

		if (c->con_protocol != PROTOCOL_SPEAKFREE) {
			c->con_protocol = PROTOCOL_SPEAKFREE;
			if (showhosts) {
				fprintf(stdout, "%s: %s sending in Speak Freely protocol.\n", prog, c->con_hostname);
			}
			c->con_reply_current = FALSE;
		}
	}

	/* If this is a face request and we have a face file open,
	   respond to it.  Note that servicing of face file data requests
	   is stateless. */

	if (sb.compression & fFaceData) {
		if (sb.compression & faceRequest) {
		long l;

		/* Request for face data. */

		if (facefile != NULL) {
			fseek(facefile, sb.buffer.buffer_len, 0);
			*((long *) sb.buffer.buffer_val) = htonl(sb.buffer.buffer_len);
			l = fread(sb.buffer.buffer_val + sizeof(long),
			1, 512 - (sizeof(long) + (sizeof(soundbuf) - BUFL)), facefile);
			sb.compression = fProtocol | fFaceData | faceReply;
			if (Debug) {
						fprintf(stdout, "%s: sending %ld bytes of face data at %d to %s\n",
				prog, l, ntohl(*((long *) sb.buffer.buffer_val)),
				c->con_hostname);
			}
			l += sizeof(long);
		} else {
			/* No face file.  Shut down requestor. */
			sb.compression = fProtocol | fFaceData | faceLess;
			l = 0;
		}
		bcopy((char *) &(from.sin_addr), (char *) &(name.sin_addr),
			sizeof(struct in_addr));

		sb.compression = htonl(sb.compression);
		sb.buffer.buffer_len = htonl(l);
		if (sendto(sock, (char *) &sb,
			(int) ((sizeof(struct soundbuf) - BUFL) + l),
			0, (struct sockaddr *) &(name), sizeof name) < 0) {
					perror("sending face image data");
		}
		} else if (sb.compression & faceReply) {

		/* Face data packet received from remote server. */

		if ((c->face_file == NULL) && (sb.buffer.buffer_len > 0)) {
					sprintf(c->face_filename, "%sSF-%s.bmp", FACEDIR, c->con_hostname);
					c->face_file = fopen(c->face_filename, "w");
		}
		if (c->face_file != NULL) {
			if (sb.buffer.buffer_len > sizeof(long)) {
			long lp =  ntohl(*((long *) sb.buffer.buffer_val));

			if (lp == c->face_address) {
				fseek(c->face_file, lp, 0);
				fwrite(sb.buffer.buffer_val + sizeof(long),
					sb.buffer.buffer_len - sizeof(long), 1,
					c->face_file);
				if (Debug) {
								fprintf(stdout, "%s: writing %ld bytes at %ld in face file %s\n",
					prog, sb.buffer.buffer_len - sizeof(long),
					lp, c->face_filename);
				}
				c->face_address += sb.buffer.buffer_len - sizeof(long);
				/* Timeout will make next request after the
				   configured interval. */
				c->face_stat = FSreply;
				c->face_retry = 0;
			} else {
				if (Debug) {
								fprintf(stdout, "%s: discarded %ld bytes for %ld in face file %s, expected data for %ld\n",
					prog, sb.buffer.buffer_len - sizeof(long),
					lp, c->face_filename, c->face_address);
				}
			}
			} else {
			pid_t cpid;

			if (Debug) {
							fprintf(stdout, "%s: closing face file %s\n",
				prog, c->face_filename);
			}
			fclose(c->face_file);
			c->face_file = NULL;
			c->face_stat = FScomplete;
			faceTransferActive--;

			/* Start viewer to display face.  We terminate
			   audio output (if active) before doing this since
						   we don't know the nature of the audio output
						   resource.  If it's an open file handle which
			   would be inherited by the child process, that
			   would hang the audio device as long as the
			   viewer is active.  */

			if (audiok) {
				audiok = FALSE;
				if (Debug) {
								fprintf(stdout, "%s: releasing audio before viewer fork().\n", prog);
				}
			}
			cpid = fork();
			if (cpid == 0) {
				char geom[30], *gp1 = NULL, *gp2 = NULL;

#ifdef NEEDED
				/* These should be reset by the execlp(). */
				signal(SIGHUP, SIG_DFL);
				signal(SIGINT, SIG_DFL);
				signal(SIGTERM, SIG_DFL);
				signal(SIGALRM, SIG_DFL);
				signal(SIGCHLD, SIG_DFL);
#endif

				/* Now we need to close any shared resources
				   that might have been inherited from the parent
				   process to avoid their being locked up for the
							   duration of the viewer's execution. */

				close(sock);
				if (record != NULL) {
				fclose(record);
				}
				if (facefile != NULL) {
				fclose(facefile);
				}
#ifdef FACE_SET_GEOMETRY
				/* Attempt to reasonably place successive face windows
							   on the screen to avoid the user's having to place
				   them individually (for window managers with
				   interactivePlacement enabled). */

#define faceInterval 120	/* Interval, in pixels, between successive faces */
							sprintf(geom, "-0+%d", facesDisplayed * faceInterval);
							gp1 = "-geometry";
				gp2 = geom;
#endif
							execlp("xv",  "xv", c->face_filename, gp1, gp2, (char *) 0);
							perror("launching face image viewer");
				facesDisplayed--;
				exit(0);
				/* Leave face image around, for the moment, so the user can
				   try to view it manually. */
			} else if (cpid == (pid_t) -1) {
							perror("creating face image viewer process");
			} else {
				c->face_viewer = cpid;
				facesDisplayed++;
			}
			}
		}
		} else if (sb.compression & faceLess) {
		if (c->face_file != NULL) {
			fclose(c->face_file);
			unlink(c->face_filename);
		}
		c->face_stat = FSabandoned;
		faceTransferActive--;
		if (Debug) {
					fprintf(stdout, "%s: no face image available for %s\n",
			prog, c->con_hostname);
		}
		}
		continue;			  /* Done with packet */
	}

	/* If the packet requests loop-back, immediately dispatch it
	   back to the host who sent it to us.	To prevent an infinite
	   loop-back cycle, we clear the loop-back bit in the header
	   before sending the message.	We leave the host of origin
	   unchanged, allowing the sender to identify the packet as
	   one he originated. */

	if (sb.compression & fLoopBack) {
		bcopy((char *) &(from.sin_addr), (char *) &(name.sin_addr),
		sizeof(struct in_addr));
		sb.compression &= ~fLoopBack;	/* Prevent infinite loopback */

		sb.compression = htonl(sb.compression);
		sb.buffer.buffer_len = htonl(sb.buffer.buffer_len);
		if (sendto(sock, (char *) &sb, rll,
		0, (struct sockaddr *) &(name), sizeof name) < 0) {
				perror("sending datagram message");
		}
		sb.compression = ntohl(sb.compression);
		sb.buffer.buffer_len = ntohl(sb.buffer.buffer_len);
	}


	/* If this packet has been "stuffed" for maximum efficiency,
	   un-stuff it at this point. */

	if ((sb.compression & fCompLPC10) && (sb.buffer.buffer_len >= 16)) {
		bcopy(sb.sendinghost, (char *) &sb + rll,
		  sizeof sb.sendinghost);
		rll += sizeof sb.sendinghost;
	}


#ifdef CRYPTO
	if ((sb.compression & fKeyPGP)) {
		char cmd[256], f[40], kmd[16];
		FILE *kfile;
		FILE *pipe;
		struct MD5Context md5c;

		MD5Init(&md5c);
		MD5Update(&md5c, sb.buffer.buffer_val, sb.buffer.buffer_len);
		MD5Final(kmd, &md5c);

		if (memcmp(c->keymd5, kmd, 16) != 0) {
		bcopy(kmd, c->keymd5, 16);
				sprintf(f, "/tmp/.SF_SKEY%d", getpid());

				kfile = fopen(f, "w");
		if (kfile == NULL) {
					fprintf(stdout, "Cannot open encrypted session key file %s\n", f);
		} else {
			fwrite(sb.buffer.buffer_val, sb.buffer.buffer_len, 1, kfile);
			fclose(kfile);
#ifdef ZZZ
			if (pgppass == NULL) {
			static char s[256]; 

						fprintf(stdout, "Enter PGP pass phrase: ");
			if (fgets(s, sizeof s, stdin) != NULL) {
				s[strlen(s) - 1] = 0;
				pgppass = s;
			}
			}
#endif
					sprintf(cmd, "pgp -f +nomanual +verbose=0 +armor=off %s%s%s <%s",
						pgppass ? "-z\"" : "", pgppass ? pgppass : "",
						pgppass ? "\" " : "", f);
#ifdef PGP_DEBUG
					fprintf(stdout, "Decoding session key with: %s\n", cmd);
#else
			if (Debug) {
					   fprintf(stdout, "%s: decoding PGP session key.\n", prog);
			}
#endif
					pipe = popen(cmd, "r");
			if (pipe == NULL) {
						fprintf(stdout, "Unable to open pipe to: %s\n", cmd);
			} else {
			int lr;

			/* Okay, explanation time again.  On some systems
			   (Silicon Graphics, for example), the timer tick
			   alarm signal can cause the pending read from the
						   PGP key pipe to return an "Interrupted system
						   call" status (EINTR) with (as far as I've ever
						   seen and I sincerely hope it's always) zero bytes
			   read.  This happens frequently when the timer is
			   running and the user takes longer to enter the
			   secret key pass phrase than the timer tick.	So,
			   if this happens we keep on re-issuing the pipe
			   read until the phrase allows PGP to finish the
			   job. */

			while ((lr = fread(c->pgpkey, 1, 17, pipe)) != 17 &&
				   (errno == EINTR)) ;
			if (lr == 17) {
				c->pgpkey[0] = TRUE;
#ifdef PGP_DEBUG
				{	
				int i;

								fprintf(stdout, "Session key for %s:", c->con_hostname);
				for (i = 0; i < 16; i++) {
									fprintf(stdout, " %02X", c->pgpkey[i + 1] & 0xFF);
				}
								fprintf(stdout, "\n");
				}
#else
				if (Debug) {
							   fprintf(stdout, "%s: PGP session key decoded.\n", prog);
				}
#endif
			} else {
				c->pgpkey[0] = FALSE;
							fprintf(stdout, "%s: Error decoding PGP session key.\n", prog);
#ifdef PGP_DEBUG
							fprintf(stdout, "Read status from pipe: %d\n", lr);
							perror("reading decoded PGP key from pipe");
#endif
			}
			pclose(pipe);
			}
			unlink(f);
		}
		}
	} else
#endif
	{
		playbuffer(&sb, c);
	}
#endif
}

int
SendCtrl(iapplication_t *ia)
{
	vconnection_t	*c;
	unsigned char	*p;
	int		len;

	c = ia->con;
	if (!c)
		return(-EINVAL);
	if (ia->Flags & (AP_FLG_VOIP_SENT_BYE | AP_FLG_VOIP_PEER_BYE))
		return(-EBUSY);
	if (!c->amsg) {
		c->amsg = msg_dequeue(&c->aqueue);
		if (c->amsg)
			c->oc++;
	}
	if (!c->amsg) { /* make RR */
		c->amsg = alloc_msg(4);
		if (!c->amsg)
			return(-ENOMEM);
		p = msg_put(c->amsg, 4);
		*p++ = c->pc;
		*p++ = c->oc;
		*p++ = 0;
		*p++ = 0x81; /* RR */
		len = rtp_make_app(c->cbuf, c->own_ssrc, 1, "ISDN",
			c->amsg->data, c->amsg->len);
		free_msg(c->amsg);
		c->amsg = NULL;
	} else {
		p = c->amsg->data;
		*p++ = c->pc;
		*p = c->oc;
		len = rtp_make_app(c->cbuf, c->own_ssrc, 1, "ISDN",
			c->amsg->data, c->amsg->len);
	}
	dprint(DBGM_SOCK, "C socket send %d bytes to %s\n",
		len, inet_ntoa(c->cpeer.sin_addr));
	if (len) {
		dhexprint(DBGM_CDATA, "send ctrl packet:", c->cbuf, len);
		len = sendto(c->sock, c->cbuf, len, 0,
			(struct sockaddr *)&c->cpeer, sizeof(c->cpeer));
		if (len < 0) {
			eprint("cannot send ctrl msg errno(%d)\n", errno);
			return(-errno);
		}
	} else {
		eprint("cannot compose APP message\n");
		return(-EINVAL);
	}
	return(0);
}

static void *
voipscan(void *arg) {
	int		ret;
	vapplication_t	*v = arg;
	fd_set		fdset;
	struct timeval	timeout;

	init_voipsocks(v);
	while (!(v->flags & AP_FLG_VOIP_ABORT)) {
		run_vitimer();
		FD_ZERO(&fdset);
		FD_SET(v->dsock, &fdset);
		FD_SET(v->csock, &fdset);
		if (get_next_vitimer_dist(&timeout)) {
			timeout = v->tout;
		}
		ret = select(v->csock + 1, &fdset, NULL, NULL, &timeout);
		if (ret < 0) { /* error */
			dprint(DBGM_SOCK, "socket select errno %d: %s\n",
				errno, strerror(errno));
			continue;
		}
		if (ret == 0) { /* timeout */
			dprint(DBGM_SOCK, "socket select timeout\n");
			continue;
		}
		if (FD_ISSET(v->dsock, &fdset)) { /* data packet */
			v->fromlen = sizeof(struct sockaddr_in);
			v->rlen = recvfrom(v->dsock, v->buf.d, MAX_NETBUFFER_SIZE,
				0, (struct sockaddr *) &v->from, &v->fromlen);
			if (v->rlen <= 0) {
				dprint(DBGM_SOCK, "D socket rlen(%d)\n",
					v->rlen); 
				continue;
			}
			dhexprint(DBGM_DDATA, "data packet:", v->buf.d, v->rlen);
			ret = receive_data(v);
			if (ret<0)
				dprint(DBGM_SOCK, "receive_data ret(%d)\n", ret);
		}
		if (FD_ISSET(v->csock, &fdset)) { /* ctrl packet */
			v->fromlen = sizeof(struct sockaddr_in);
			v->rlen = recvfrom(v->csock, v->buf.d, MAX_NETBUFFER_SIZE,
				0, (struct sockaddr *) &v->from, &v->fromlen);
			if (v->rlen <= 0) {
				dprint(DBGM_SOCK, "C socket rlen(%d)\n",
					v->rlen); 
				continue;
			}
			dhexprint(DBGM_CDATA, "ctrl packet:", v->buf.d, v->rlen);
			ret = receive_ctrl(v);
			if (ret)
				dprint(DBGM_SOCK, "receive_ctrl ret(%d)\n", ret);
		}
	}
	close_voipsocks(v);
	return(NULL);
}

pthread_t
run_voip(vapplication_t *v) {
	int		ret;
	pthread_t	tid;

	ret = pthread_create(&tid, NULL, voipscan, (void *)v);
	return(tid);
}

static int
rtpout_ap(iapplication_t *iap)
{
	int		len, r;
	rtp_hdr_t	*rh;

	rh = (rtp_hdr_t *)iap->con->dbuf;
	iap->con->dbuf[0] = RTP_VERSION << 6;
	rh->seq = htons(iap->con->seq);
	rh->ts = htonl(iap->con->timestamp);
	rh->ssrc = htonl(iap->con->own_ssrc);

#ifdef GSM_COMPRESSION
	if (iap->con->sndflags & SNDFLG_COMPR_GSM) { /* GSM */
		int	i;
		gsm_signal	gs[640], *gp;
		u_char		*p;

		if (!iap->con->s_gsm)
			iap->con->s_gsm = gsm_create();
		if (iap->con->slen != 4*160) {
			eprint("%s wrong GSM Data size %d/%d\n", __FUNCTION__,
				iap->con->rlen, 4*160);
			return(0);
		}
		gp = gs;
		p = iap->con->sbuf;
		for (i=0;i<640;i++)
			*gp++ = alaw2linear(*p++);
		p = &iap->con->dbuf[12];
		gp = gs;
		for (i=0;i<4; i++) {
			gsm_encode(iap->con->s_gsm, gp, p);
			p += 33;
			gp += 160;
		}
		len = 4*33 + 12;
		iap->con->dbuf[1] = 3;
	} else
#endif
	{
		iap->con->dbuf[1] = 8;
		memcpy(&iap->con->dbuf[12], iap->con->sbuf, iap->con->slen);
		len = 12 + iap->con->slen;
	}
	r = len % 4;
	if (r) {
		int i;
		r = 4 - r;
		for (i=0; i<r; i++)
			iap->con->dbuf[len++] = 0;
		iap->con->dbuf[len-1] = r;
		iap->con->dbuf[0] |= RTP_PAD_FLAG; 
	}
	return(len);
}

static int
send_sdata(iapplication_t *ap)
{
	int	len, ret;	

	len = rtpout_ap(ap);
	ap->con->seq++;
	ap->con->timestamp += ap->con->slen;
	if (len)
		ret = sendto(ap->con->sock, ap->con->dbuf, len, 0,
			(struct sockaddr *) &ap->con->dpeer,
			sizeof(struct sockaddr_in));
	else
		ret = 0;
	return(ret);
}

void *
voip_sender(void *arg)
{
	iapplication_t	*ap = arg;
	isound_t 	*is;
	int		avail, ret = 0;


	while (!(ap->Flags & AP_FLG_VOIP_ABORT)) {
		is = ap->data2;
		if (!is || !is->sbuf) {
			dprint(DBGM_SOUND, "application data2 NULL\n");
			break;
		}
		if (!ap->con) {
			dprint(DBGM_SOUND, "application ap->con NULL\n");
			break;
		}
		avail = ibuf_usedcount(is->sbuf);
		if (avail >= ap->con->pkt_size) {
			ap->con->slen = ap->con->pkt_size;
			ibuf_memcpy_r(ap->con->sbuf, is->sbuf, ap->con->slen);
			if (is->sbuf->wsem)
				sem_post(is->sbuf->wsem);
#if 0
			register unsigned char *start = bs;
			register int j;
			int squelched = (squelch > 0), osl = soundel;

			/* If entire buffer is less than squelch, ditch it.  If
			   we haven't received sqdelay samples since the last
			   squelch event, continue to transmit. */

			if (sqdelay > 0 && sqwait > 0) {
				if (debugging) {
					printf("Squelch countdown: %d samples left.\n",
						sqwait);
				}
				sqwait -= soundel;
				squelched = FALSE;
			} else if (squelch > 0) {
				for (j = 0; j < soundel; j++) {
#ifdef USQUELCH
					if (((*start++ & 0x7F) ^ 0x7F) > squelch)
#else
					int samp = alaw2linear(*start++);

					if (samp < 0) {
						samp = -samp;
					}
					if (samp > squelch)
#endif
					{
						squelched = FALSE;
						sqwait = sqdelay;
						break;
					}
				}
			}

			if (squelched) {
				if (debugging) {
					printf("Entire buffer squelched.\n");
				}
				spurt = TRUE;
			} else {
				netbuf.compression = fProtocol | (ring ? (fSetDest | fDestSpkr) : 0);
				netbuf.compression |= debugging ? fDebug : 0;
				netbuf.compression |= loopback ? fLoopBack : 0;
				ring = FALSE;
				if (compressing) {
					int is = soundel, os = soundel / 2;

					rate_flow(buf, buf, &is, &os);
					soundel = os;
					netbuf.compression |= fComp2X;
				}
				netbuf.buffer.buffer_len = soundel;
				if (!sendpkt(&netbuf)) {
					exiting();
					return(2);
				}
				if (debugging && !vat && !rtp) {
					fprintf(stdout, "Sent %d audio samples in %d bytes.\r\n",
						osl, soundel);
				}
			}
#endif
			ret = send_sdata(ap);
			dprint(DBGM_SOUND, "send_sdata ret %d\n", ret);
			if (ret<=0) {
				dprint(DBGM_SOUND, "send_sdata ret %d\n", ret);
				ap->Flags |= AP_FLG_VOIP_ABORT;
			}
		} else {
			sem_wait(&is->work);
		}
	}
	return((void *)ret);
}

