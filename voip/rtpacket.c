/*

	      RTP input packet construction and parsing

*/

#include <pwd.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "g711.h"
#include "rtpacket.h"

#ifndef FALSE
#define	FALSE	0
#endif

#ifndef TRUE
#define	TRUE	1
#endif

audio_descr_t adt[] = {
/* enc	   sample ch */
  {AE_PCMU,  8000, 1},	/*  0 PCMU */
  {AE_MAX,   8000, 1},	/*  1 1016 */
  {AE_G721,  8000, 1},	/*  2 G721 */
  {AE_GSM,   8000, 1},	/*  3 GSM */
  {AE_G723,  8000, 1},	/*  4 Unassigned */
  {AE_IDVI,  8000, 1},	/*  5 DVI4 */
  {AE_IDVI, 16000, 1},	/*  6 DVI4 */
  {AE_LPC,   8000, 1},	/*  7 LPC */
  {AE_PCMA,  8000, 1},	/*  8 PCMA */
  {AE_MAX,	0, 1},	/*  9 G722 */
  {AE_L16,  44100, 2},	/* 10 L16 */
  {AE_L16,  44100, 1},	/* 11 L16 */
  {AE_MAX,	0, 1},	/* 12 */
};

#define MAX_MISORDER 100
#define MAX_DROPOUT  3000

/*  ISRTP  --  Determine if a packet is RTP or not.  If so, convert
	       in place into a sound buffer.  */

int isrtp(pkt, len)
  unsigned char *pkt;
  int len;
{
#ifdef RationalWorld
    rtp_hdr_t *rh = (rtp_hdr_t *) pkt;
#endif

    unsigned int r_version, r_p, r_x, r_cc, r_m, r_pt,
		 r_seq, r_ts;

    /* Tear apart the header in a byte- and bit field-order
       independent fashion. */

    r_version = (pkt[0] >> 6) & 3;
    r_p = !!(pkt[0] & 0x20);
    r_x = !!(pkt[0] & 0x10);
    r_cc = pkt[0] & 0xF;
    r_m = !!(pkt[1] & 0x80);
    r_pt = pkt[1] & 0x1F;
    r_seq = ntohs(*((short *) (pkt + 2)));
    r_ts = ntohl(*((long *) (pkt + 4)));

    if (
#ifdef RationalWorld
	rh->version == RTP_VERSION && /* Version ID correct */
	rh->pt < ELEMENTS(adt) &&     /* Payload type credible */
	adt[rh->pt].sample_rate != 0 && /* Defined payload type */
				      /* Padding, if present, is plausible */
	(!rh->p || (pkt[len - 1] < (len - (12 + 4 * rh->cc))))
#else
	r_version == RTP_VERSION &&   /* Version ID correct */
	r_pt < ELEMENTS(adt) &&       /* Payload type credible */
	adt[r_pt].sample_rate != 0 && /* Defined payload type */
				      /* Padding, if present, is plausible */
	(!r_p || (pkt[len - 1] < (len - (12 + 4 * r_cc))))
#endif
       ) {
	unsigned char *payload;
	int lex, paylen;

			      /* Length of fixed header extension, if any */
	lex = r_x ? (ntohs(*((short *) (pkt + 2 + 12 + 4 * r_cc))) + 1) * 4 : 0;
	payload = pkt + (12 + 4 * r_cc) + lex; /* Start of payload */
	paylen = len - ((12 + 4 * r_cc) +      /* Length of payload */
		    lex + (r_p ? pkt[len - 1] : 0));
	return TRUE;
    }
    return FALSE;
}

/*  ISVALIDRTCPPACKET  --  Consistency check a packet to see if
			   is a compliant RTCP packet.	Note that
			   since this must also accept Speak Freely
			   SDES packets, the test on the protocol is
			   not as tight as were it exclusively for
			   RTP.  */

int isValidRTCPpacket(p, len)
  unsigned char *p;
  int len;
{
    unsigned char *end;

    if (((((p[0] >> 6) & 3) != RTP_VERSION) &&	   /* Version incorrect ? */
	((((p[0] >> 6) & 3) != 1))) ||		   /* Allow Speak Freely too */
	((p[0] & 0x20) != 0) || 		   /* Padding in first packet ? */
	((p[1] != RTCP_SR) && (p[1] != RTCP_RR))) { /* First item not SR or RR ? */
	return FALSE;
    }
    end = p + len;

    do {
	/* Advance to next subpacket */
	p += (ntohs(*((short *) (p + 2))) + 1) * 4;
    } while (p < end && (((p[0] >> 6) & 3) == RTP_VERSION));

    return p == end;
}

/*  ISRTCPBYEPACKET  --  Test if this RTCP packet contains a BYE.  */

int isRTCPByepacket(p, len)
  unsigned char *p;
  int len;
{
    unsigned char *end;
    int sawbye = FALSE;
						   /* Version incorrect ? */
    if ((((p[0] >> 6) & 3) != RTP_VERSION && ((p[0] >> 6) & 3) != 1) ||
	((p[0] & 0x20) != 0) || 		   /* Padding in first packet ? */
	((p[1] != RTCP_SR) && (p[1] != RTCP_RR))) { /* First item not SR or RR ? */
	return FALSE;
    }
    end = p + len;

    do {
	if (p[1] == RTCP_BYE) {
	    sawbye = TRUE;
	}
	/* Advance to next subpacket */
	p += (ntohs(*((short *) (p + 2))) + 1) * 4;
    } while (p < end && (((p[0] >> 6) & 3) == RTP_VERSION));

    return (p == end) && sawbye;
}

/*  ISRTCPAPPPACKET  --  Test if this RTCP packet contains a APP item
			 with a given name.  If so, returns a pointer
			 to the APP sub-packet in app_ptr.  */

int isRTCPAPPpacket(p, len, name, app_ptr)
  unsigned char *p;
  int len;
  char *name;
  unsigned char **app_ptr;
{
    unsigned char *end;

    *app_ptr = NULL;
						   /* Version incorrect ? */
    if ((((p[0] >> 6) & 3) != RTP_VERSION && ((p[0] >> 6) & 3) != 1) ||
	((p[0] & 0x20) != 0) || 		   /* Padding in first packet ? */
	((p[1] != RTCP_SR) && (p[1] != RTCP_RR))) { /* First item not SR or RR ? */
	return FALSE;
    }
    end = p + len;

    do {
	if ((p[1] == RTCP_APP) && (memcmp(p + 8, name, 4) == 0)) {
	    *app_ptr = p;
	    return TRUE;
	}
	/* Advance to next subpacket */
	p += (ntohs(*((short *) (p + 2))) + 1) * 4;
    } while (p < end && (((p[0] >> 6) & 3) == RTP_VERSION));

    return FALSE;
}

#if 0
/*  RTP_MAKE_SDES  --  Generate a source description for this
		       user, based either on information obtained
		       from the password file or supplied by
		       environment variables.  Strict construction
		       of the RTP specification requires every
		       SDES packet to be a composite which begins
		       with a sender or receiver report.  If the
                       "strict" argument is true, we'll comply with
                       this.  Unfortunately, Look Who's Listening
		       Server code was not aware of this little
		       twist when originally implemented, so it will
		       take some time to transition all the running
		       servers to composite packet aware code.	*/

int rtp_make_sdes(pkt, ssrc_i, port, strict)
  char **pkt;
  unsigned long ssrc_i;
  int port, strict;
{
    unsigned char zp[1500];
    unsigned char *p = zp;
    rtcp_t *rp;
    unsigned char *ap;
    char *sp, *ep;
    int l, hl;
    struct passwd *pw;
    char s[256], ev[1024];

#define addSDES(item, text) *ap++ = item; *ap++ = l = strlen(text); \
			    bcopy(text, ap, l); ap += l

    hl = 0;
    if (strict) {
	*p++ = RTP_VERSION << 6;
	*p++ = RTCP_RR;
	*p++ = 0;
	*p++ = 1;
	*((long *) p) = htonl(ssrc_i);
	p += 4;
	hl = 8;
    }

    rp = (rtcp_t *) p;
#ifdef RationalWorld
    rp->common.version = RTP_VERSION;
    rp->common.p = 0;
    rp->common.count = 1;
    rp->common.pt = RTCP_SDES;
#else
    *((short *) p) = htons((RTP_VERSION << 14) | RTCP_SDES | (1 << 8));
#endif	
    rp->r.sdes.src = htonl(ssrc_i);

    ap = (unsigned char *) rp->r.sdes.item;

    ep = getenv("SPEAKFREE_ID");
    if (ep != NULL) {
	if (strlen(ep) == 0) {
	    ep = NULL;
	} else {
	    strcpy(ev, ep);
	    ep = ev;
	}
    }

    /* Build canonical name for this user.  This is generally
       a name which can be used for "talk" and "finger", and
       is not necessarily the user's E-mail address. */

    if ((sp = getenv("SPEAKFREE_CNAME")) != NULL && strlen(sp) > 0) {
        /* If strict, drop leading asterisk that's used to request an
	   unlisted entry on an LWL server. */
        if (strict && sp[0] == '*') {
	    sp++;
	}
	addSDES(RTCP_SDES_CNAME, sp);
    } else {
	pw = getpwuid(getuid());
	if (pw != NULL) {
	    char dn[64];
	    char hn[MAXHOSTNAMELEN];

	    dn[0] = hn[0] = 0;
	    getdomainname(dn, sizeof dn);
	    gethostname(hn, sizeof hn);
	    if (dn[0] != 0) {
                sprintf(s, "%s@%s", pw->pw_name, dn);
	    } else {
		struct hostent *h;
		struct in_addr naddr;

		h = gethostbyname(hn);
                if (strchr(h->h_name, '.') != NULL) {
                    sprintf(s, "%s@%s", pw->pw_name, h->h_name);
		} else {
		    bcopy(h->h_addr, &naddr, sizeof naddr);
                    sprintf(s, "%s@%s", pw->pw_name, inet_ntoa(naddr));
		}
	    }
	    addSDES(RTCP_SDES_CNAME, s);
	    if (ep == NULL && pw->pw_gecos != NULL) {
                char *gc = strchr(pw->pw_gecos, ',');

		if (gc != NULL) {
		    *gc = 0;
		}
		addSDES(RTCP_SDES_NAME, pw->pw_gecos);
	    }
	} else {
#ifdef Solaris
	    {	char s[12];
		sysinfo(SI_HW_SERIAL, s, 12);
                sprintf(s, "Unknown@%s.hostid.net", s);
	    }
#else
            sprintf(s, "Unknown@%lu.hostid.net", gethostid());
#endif
	    addSDES(RTCP_SDES_CNAME, s);
	}
    }

    /* If a SPEAKFREE_ID environment variable is present,
       parse the items it contains.  Format:

       SPEAKFREE_ID=<full name>:<E-mail address>:<phone number>:<location>

    */

    if (ep != NULL) {
	int i;
	static int items[] = { RTCP_SDES_NAME, RTCP_SDES_EMAIL,
			       RTCP_SDES_PHONE, RTCP_SDES_LOC };
	char *np;

	for (i = 0; i < ELEMENTS(items); i++) {
	    while (*ep && isspace(*ep)) {
		ep++;
	    }
	    if (*ep == 0) {
		break;
	    }
            if ((np = strchr(ep, ':')) != NULL) {
		*np++ = 0;
	    } else {
		np = NULL;
	    }
	    if (strlen(ep) > 0) {
                /* If strict, drop leading asterisk that's used to request an
		   unlisted entry on an LWL server. */
                if (strict && items[i] == RTCP_SDES_EMAIL && ep[0] == '*') {
		    ep++;
		}
		addSDES(items[i], ep);
	    }
	    if (np == NULL) {
		break;
	    }
	    ep = np;
	}
    }

    addSDES(RTCP_SDES_TOOL, "Speak Freely for Unix");

    if (!strict) {

	/* If a port number is specified, add a PRIV item indicating
           the port we're communicating on. */

	if (port >= 0) {
	    char s[20];

            sprintf(s, "\001P%d", port);
	    addSDES(RTCP_SDES_PRIV, s);
	}
    }
    
    *ap++ = RTCP_SDES_END;
    *ap++ = 0;

    l = ap - p;

    rp->common.length = htons(((l + 3) / 4) - 1);
    l = hl + ((ntohs(rp->common.length) + 1) * 4);

    /* Okay, if the total length of this packet is not an odd
       multiple of 4 bytes, we're going to put a pad at the
       end of it.  Why?  Because we may encrypt the packet
       later and that requires it be a multiple of 8 bytes,
       and we don't want the encryption code to have to
       know all about our weird composite packet structure.
       Oh yes, there's no reason to do this if strict isn't
       set, since we never encrypt packets sent to a Look
       Who's Listening server.

       Why an odd multiple of 4 bytes, I head you ask?
       Because when we encrypt an RTCP packet, we're required
       to prefix it with four random bytes to deter a known
       plaintext attack, and since the total buffer we
       encrypt, including the random bytes, has to be a
       multiple of 8 bytes, the message needs to be an odd
       multiple of 4. */

    if (strict) {
	int pl = (l & 4) ? l : l + 4;

	if (pl > l) {
	    int pad = pl - l;

	    bzero(zp + l, pad);       /* Clear pad area to zero */
	    zp[pl - 1] = pad;	      /* Put pad byte count at end of packet */
            p[0] |= 0x20;             /* Set the "P" bit in the header of the
					 SDES (last in message) packet */
                                      /* If we've added an additional word to
					 the packet, adjust the length in the
					 SDES message, which must include the
					 pad */
	    rp->common.length = htons(ntohs(rp->common.length) + ((pad) / 4));
	    l = pl;		      /* Include pad in length of packet */
	}
    }

    *pkt = (char *) malloc(l);
    if (*pkt != NULL) {
	bcopy(zp, *pkt, l);
	return l;
    }
    return 0;
}

/*  RTP_MAKE_SDES_S--  Generate a source description for this
		       servers to composite packet aware code.	*/

int rtp_make_sdes_s(pkt, strict, r)
  char **pkt;
  struct rtcp_sdes_request *r;
  int strict;
{
    unsigned char zp[1500];
    unsigned char *p = zp;
    unsigned long *ssrc;
    rtcp_t *rp;
    unsigned char *ap;
    int l, hl, i;

#define addSDES(item, text) *ap++ = item; *ap++ = l = strlen(text); \
			    bcopy(text, ap, l); ap += l

    ssrc = (unsigned long *)r->ssrc;
    hl = 0;
    if (strict) {
	*p++ = RTP_VERSION << 6;
	*p++ = RTCP_RR;
	*p++ = 0;
	*p++ = 1;
	*((long *) p) = htonl(*ssrc);
	p += 4;
	hl = 8;
    }

    rp = (rtcp_t *) p;
#ifdef RationalWorld
    rp->common.version = RTP_VERSION;
    rp->common.p = 0;
    rp->common.count = 1;
    rp->common.pt = RTCP_SDES;
#else
    *((short *) p) = htons((RTP_VERSION << 14) | RTCP_SDES | (1 << 8));
#endif	
    rp->r.sdes.src = htonl(*ssrc);

    ap = (unsigned char *) rp->r.sdes.item;

    for (i = 0; i < r->nitems; i++) {
    	addSDES(r->item[i].r_item, r->item[i].r_text);
    }
    
    *ap++ = RTCP_SDES_END;
    *ap++ = 0;

    l = ap - p;

    rp->common.length = htons(((l + 3) / 4) - 1);
    l = hl + ((ntohs(rp->common.length) + 1) * 4);

    /* Okay, if the total length of this packet is not an odd
       multiple of 4 bytes, we're going to put a pad at the
       end of it.  Why?  Because we may encrypt the packet
       later and that requires it be a multiple of 8 bytes,
       and we don't want the encryption code to have to
       know all about our weird composite packet structure.
       Oh yes, there's no reason to do this if strict isn't
       set, since we never encrypt packets sent to a Look
       Who's Listening server.

       Why an odd multiple of 4 bytes, I head you ask?
       Because when we encrypt an RTCP packet, we're required
       to prefix it with four random bytes to deter a known
       plaintext attack, and since the total buffer we
       encrypt, including the random bytes, has to be a
       multiple of 8 bytes, the message needs to be an odd
       multiple of 4. */

    if (strict) {
	int pl = (l & 4) ? l : l + 4;

	if (pl > l) {
	    int pad = pl - l;

	    bzero(zp + l, pad);       /* Clear pad area to zero */
	    zp[pl - 1] = pad;	      /* Put pad byte count at end of packet */
            p[0] |= 0x20;             /* Set the "P" bit in the header of the
					 SDES (last in message) packet */
                                      /* If we've added an additional word to
					 the packet, adjust the length in the
					 SDES message, which must include the
					 pad */
	    rp->common.length = htons(ntohs(rp->common.length) + ((pad) / 4));
	    l = pl;		      /* Include pad in length of packet */
	}
    }

    if (*pkt == NULL)
	    *pkt = (char *) malloc(l);
    if (*pkt != NULL) {
	bcopy(zp, *pkt, l);
	return l;
    }
    return 0;
}

#endif

/*  RTP_MAKE_BYE  --  Create a "BYE" RTCP packet for this connection.  */

int rtp_make_bye(p, ssrc_i, raison, strict)
  unsigned char *p;
  unsigned long ssrc_i;
  char *raison;
  int strict;
{
    rtcp_t *rp;
    unsigned char *ap, *zp;
    int l, hl;

    /* If requested, prefix the packet with a null receiver
       report.	This is required by the RTP spec, but is not
       required in packets sent only to the Look Who's Listening
       server. */

    zp = p;
    hl = 0;
    if (strict) {
	*p++ = RTP_VERSION << 6;
	*p++ = RTCP_RR;
	*p++ = 0;
	*p++ = 1;
	*((long *) p) = htonl(ssrc_i);
	p += 4;
	hl = 8;
    }

    rp = (rtcp_t *) p;
#ifdef RationalWorld
    rp->common.version = RTP_VERSION;
    rp->common.p = 0;
    rp->common.count = 1;
    rp->common.pt = RTCP_BYE;
#else
    *((short *) p) = htons((RTP_VERSION << 14) | RTCP_BYE | (1 << 8));
#endif	
    rp->r.bye.src[0] = htonl(ssrc_i);

    ap = (unsigned char *) rp->r.sdes.item;

    l = 0;
    if (raison != NULL) {
	l = strlen(raison);
	if (l > 0) {
	    *ap++ = l;
	    bcopy(raison, ap, l);
	    ap += l;
	}
    }

    while ((ap - p) & 3) {
	*ap++ = 0;
    }
    l = ap - p;

    rp->common.length = htons((l / 4) - 1);

    l = hl + ((ntohs(rp->common.length) + 1) * 4);

    /* If strict, pad the composite packet to an odd multiple of 4
       bytes so that if we decide to encrypt it we don't have to worry
       about padding at that point. */

    if (strict) {
	int pl = (l & 4) ? l : l + 4;

	if (pl > l) {
	    int pad = pl - l;

	    bzero(zp + l, pad);       /* Clear pad area to zero */
	    zp[pl - 1] = pad;	      /* Put pad byte count at end of packet */
            p[0] |= 0x20;             /* Set the "P" bit in the header of the
					 SDES (last in message) packet */
                                      /* If we've added an additional word to
					 the packet, adjust the length in the
					 SDES message, which must include the
					 pad */
	    rp->common.length = htons(ntohs(rp->common.length) + ((pad) / 4));
	    l = pl;		      /* Include pad in length of packet */
	}
    }

    return l;
}

/*  RTP_MAKE_APP  --  Create a "APP" (application-defined) RTCP packet
		      for this connection with the given type (name)
		      and content. */

int rtp_make_app(p, ssrc_i, strict, type, content, len)
  unsigned char	*p, *content;
  unsigned long	ssrc_i;
  int		strict, len;
  char		*type;
{
    rtcp_t *rp;
    unsigned char *ap, *zp;
    int l, hl;

    /* If requested, prefix the packet with a null receiver
       report.	This is required by the RTP spec, but is not
       required in packets sent only to other copies of Speak
       Freely. */

    zp = p;
    hl = 0;
    if (strict) {
	*p++ = RTP_VERSION << 6;
	*p++ = RTCP_RR;
	*p++ = 0;
	*p++ = 1;
	*((long *) p) = htonl(ssrc_i);
	p += 4;
	hl = 8;
    }

    rp = (rtcp_t *) p;
    *((short *) p) = htons((RTP_VERSION << 14) | RTCP_APP | (1 << 8));
    rp->r.bye.src[0] = htonl(ssrc_i);

    ap = p + 8;
    bcopy(type, ap, 4);
    ap += 4;

    bcopy(content, ap, len);
    ap += len;

    while ((ap - p) & 3) {
	*ap++ = 0;
    }
    l = ap - p;

    rp->common.length = htons((l / 4) - 1);

    l = hl + ((ntohs(rp->common.length) + 1) * 4);

    /* If strict, pad the composite packet to an odd multiple of 4
       bytes so that if we decide to encrypt it we don't have to worry
       about padding at that point. */

    if (strict) {
	int pl = (l & 4) ? l : l + 4;

	if (pl > l) {
	    int pad = pl - l;

	    bzero(zp + l, pad);       /* Clear pad area to zero */
	    zp[pl - 1] = pad;	      /* Put pad byte count at end of packet */
            p[0] |= 0x20;             /* Set the "P" bit in the header of the
					 SDES (last in message) packet */
                                      /* If we've added an additional word to
					 the packet, adjust the length in the
					 SDES message, which must include the
					 pad */
	    rp->common.length = htons(ntohs(rp->common.length) + ((pad) / 4));
	    l = pl;		      /* Include pad in length of packet */
	}
    }

    return l;
}

#if 0
/*  RTPOUT  --	Convert a sound buffer into an RTP packet, given the
		SSRC, timestamp, and sequence number appropriate for the
		next packet sent to this connection.  */

int rtpout(sb, ssrc_i, timestamp_i, seq_i, spurt)
  soundbuf *sb;
  unsigned long ssrc_i, timestamp_i;
  unsigned short seq_i;
  int spurt;
{
    soundbuf rp;
    rtp_hdr_t *rh = (rtp_hdr_t *) &rp;
    int pl = 0;

#ifdef RationalWorld
    rh->version = RTP_VERSION;
    rh->p = 0;
    rh->x = 0;
    rh->cc = 0;
    rh->m = !!spurt;
#else
    *((short *) rh) = htons((RTP_VERSION << 14) | (spurt ? 0x80 : 0));
#endif
    rh->seq = htons(seq_i);
    rh->ts = htonl(timestamp_i);
    rh->ssrc = htonl(ssrc_i);

    /* GSM */

    if (sb->compression & fCompGSM) {
#ifdef RationalWorld
	rh->pt = 3;
#else
	((char *) rh)[1] = 3;
#endif
	bcopy(sb->buffer.buffer_val + 2, ((char *) &rp) + 12,
		  (int) sb->buffer.buffer_len - 2);
	pl = (sb->buffer.buffer_len - 2) + 12;

    /* ADPCM */

    } else if (sb->compression & fCompADPCM) {
#ifdef RationalWorld
	rh->pt = 5;
#else
	((char *) rh)[1] = 5;
#endif
	bcopy(sb->buffer.buffer_val, ((char *) &rp) + 12 + 4,
		  (int) sb->buffer.buffer_len - 3);
	bcopy(sb->buffer.buffer_val + ((int) sb->buffer.buffer_len - 3),
		  ((char *) &rp) + 12, 3);
	((char *) &rp)[15] = 0;
	pl = (sb->buffer.buffer_len + 1) + 12;

    /* LPC */

    } else if (sb->compression & fCompLPC) {
	int i, n = (sb->buffer.buffer_len - 2) / 14;
	char *ip = (char *) (sb->buffer.buffer_val + 2),
	     *op = (char *) &rp + 12;
#ifdef RationalWorld
	rh->pt = 7;
#else
	((char *) rh)[1] = 7;
#endif
	for (i = 0; i < n; i++) {
	    bcopy(ip, op, 3);
	    bcopy(ip + 4, op + 3, 10);
	    op[13] = 0;
	    ip += 14;
	    op += 14;
	}
	pl = 12 + 14 * n;

    } else if (sb->compression & fEnculaw) {
#ifdef RationalWorld
    	rh->pt = 0;
#else
	((char *) rh)[1] = 0;
#endif
    	bcopy(sb->buffer.buffer_val, ((char *) &rp) + 12,
    		(int) sb->buffer.buffer_len);
    	pl = (int) sb->buffer.buffer_len + 12;
    } else {	/* default Uncompressed PCMA samples fEncAlaw */
#ifdef RationalWorld
	rh->pt = 8;
#else
	((char *) rh)[1] = 8;
#endif
	bcopy(sb->buffer.buffer_val, ((char *) &rp) + 12,
		(int) sb->buffer.buffer_len);
	pl = (int) sb->buffer.buffer_len + 12;
    }
    if (pl > 0) {
	bcopy((char *) &rp, (char *) sb, pl);
    }
    return pl;
}

#endif

/*  PARSESDES  --  Look for an SDES message in a possibly composite
		   RTCP packet and extract pointers to selected items
                   into the caller's structure.  */

int parseSDES(packet, r)
  unsigned char *packet;
  struct rtcp_sdes_request *r;
{
    int i, success = FALSE;
    unsigned char *p = packet;

    /* Initialise all the results in the request packet to NULL. */

    for (i = 0; i < r->nitems; i++) {
	r->item[i].r_text = NULL;
    }

    /* Walk through the individual items in a possibly composite
       packet until we locate an SDES. This allows us to accept
       packets that comply with the RTP standard that all RTCP packets
       begin with an SR or RR. */

    while ((p[0] >> 6 & 3) == RTP_VERSION || (p[0] >> 6 & 3) == 1) {
	if ((p[1] == RTCP_SDES) && ((p[0] & 0x1F) > 0)) {
	    unsigned char *cp = p + 8,
			  *lp = cp + (ntohs(*((short *) (p + 2))) + 1) * 4;

	    bcopy(p + 4, r->ssrc, 4);
	    while (cp < lp) {
		unsigned char itype = *cp;

		if (itype == RTCP_SDES_END) {
		    break;
		}

		/* Search for a match in the request and fill the
		   first unused matching item.	We do it this way to
		   permit retrieval of multiple PRIV items in the same
		   packet. */

		for (i = 0; i < r->nitems; i++) {
		    if (r->item[i].r_item == itype &&
			r->item[i].r_text == NULL) {
			r->item[i].r_text = (char *) cp;
			success = TRUE;
			break;
		    }
		}
		cp += cp[1] + 2;
	    }
	    break;
	}
	/* If not of interest to us, skip to next subpacket. */
	p += (ntohs(*((short *) (p + 2))) + 1) * 4;
    }
    return success;
}

/*  COPYSDESITEM  --  Copy an SDES item to a zero-terminated user
		      string.  */

void copySDESitem(s, d)
  char *s, *d;
{
    int len = s[1] & 0xFF;

    bcopy(s + 2, d, len);
    d[len] = 0;
}

