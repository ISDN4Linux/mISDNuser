#ifndef H_RTPACKET_H
#define H_RTPACKET_H

#include "rtp.h"

#define ELEMENTS(array)	(sizeof(array)/sizeof((array)[0]))
#define	fEnculaw	0x100000
#define	fEncAlaw	0x200000

struct rtcp_sdes_request_item {
    unsigned char r_item;
    char *r_text;
};

struct rtcp_sdes_request {
    int nitems; 		      /* Number of items requested */
    unsigned char ssrc[4];	      /* Source identifier */
    struct rtcp_sdes_request_item item[10]; /* Request items */
};

extern	int	isrtp(unsigned char *, int);
extern	int	isValidRTCPpacket(unsigned char *, int);
extern	int	isRTCPByepacket(unsigned char *, int);
extern	int	isRTCPAPPpacket(unsigned char *, int, char *, unsigned char **);
extern	int	rtp_make_sdes(char **, unsigned long, int, int);
extern	int	rtp_make_sdes_s(char **, int, struct rtcp_sdes_request *);
extern	int	rtp_make_bye(unsigned char *, unsigned long, char *, int);
extern	int	rtp_make_app(unsigned char *, unsigned long, int, char *,
			unsigned char *, int);
extern	int	parseSDES(unsigned char *, struct rtcp_sdes_request *);
extern	void	copySDESitem(char *, char *);
#endif
