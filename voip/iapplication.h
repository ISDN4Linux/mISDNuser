#ifndef IAPPLICATION_H
#define IAPPLICATION_H

#include "vitimer.h"
#ifdef GSM_COMPRESSION
#include <gsm.h>
#endif

#define AP_MODE_IDLE		0
#define	AP_MODE_INTERN_CALL	1
#define AP_MODE_AUDIO_CALL	2
#define AP_MODE_VOIP_OCALL	3
#define AP_MODE_VOIP_ICALL	4

#define AP_FLG_AUDIO_ACTIV	1
#define AP_FLG_VOIP_ALERTING	2
#define AP_FLG_VOIP_ACTIV	4

#define AP_FLG_VOIP_NEW_CONN	0x01000000
#define AP_FLG_VOIP_PEER_VALID	0x02000000
#define	AP_FLG_VOIP_SENT_BYE	0x04000000
#define AP_FLG_VOIP_PEER_BYE	0x08000000
#define AP_FLG_VOIP_PEER_SF	0x10000000

#define AP_FLG_AUDIO_USED	0x00000100
#define	AP_FLG_VOIP_ABORT	0x80000000

#define AP_PR_VOIP_ISDN		1
#define AP_PR_VOIP_NEW		2
#define AP_PR_VOIP_SPEAKFREE	3
#define AP_PR_VOIP_BYE		4

#define MAX_HOST_SIZE		64
#define MAX_NETBUFFER_SIZE	8040

#define SLOW_TIMEOUT_s		10
#define SLOW_TIMEOUT_us		0
#define NORMAL_TIMEOUT_s	0
#define NORMAL_TIMEOUT_us	(320*125)

#define SNDFLG_ULAW		0x00000001
#define SNDFLG_ALAW		0x00000002
#define SNDFLG_LINEAR16		0x00000004
#define SNDFLG_COMPR_GSM	0x00000100

typedef struct _iapplication	iapplication_t;
typedef struct _vapplication	vapplication_t;
typedef struct _vconnection	vconnection_t;

struct _iapplication {
	iapplication_t	*prev;
	iapplication_t	*next;
	manager_t	*mgr;
	vapplication_t	*vapp;
	void		*data1;
	void		*data2;
	vconnection_t	*con;
	void		*para;
	vi_timer_t	timer;
	pthread_t	tid;
	int		Flags;
	int		mode;
};

struct _vapplication {
	manager_t		*mgr_lst;
	char			hostname[MAX_HOST_SIZE];
	unsigned int		flags;
	struct timeval		tout;
	int			debug;
	int			port;
	int			dsock;
	int			csock;
	struct sockaddr_in	daddr;
	struct sockaddr_in	caddr;
	struct sockaddr_in	from;
	int			fromlen;
	iapplication_t		*iapp_lst;
	int			rlen;
	union {
		unsigned char		d[MAX_NETBUFFER_SIZE];
	}			buf;
};

struct _vconnection {
	int			sock;
	struct sockaddr_in	cpeer;
	struct sockaddr_in	dpeer;
	char			rmtname[256];
	char			con_hostname[32];
	unsigned int		own_ssrc;
	unsigned int		peer_ssrc;
	unsigned int		timestamp;
	unsigned short		seq;
	unsigned short		lastseq;
	unsigned char		oc;
	unsigned char		pc;
	msg_queue_t		aqueue;
	msg_t			*amsg;
	int			rlen;
	unsigned char		*rbuf;
	unsigned int		sndflags;
	int			pkt_size;
	int			slen;
#ifdef GSM_COMPRESSION
	gsm			r_gsm;
	gsm			s_gsm;
#endif
	unsigned char		sbuf[1024];
	unsigned char		dbuf[1152];
	unsigned char		cbuf[1024];
};

extern	pthread_t	run_voip(vapplication_t *v);
extern	void		*voip_sender(void *arg);

extern	void		clear_connection(iapplication_t *);
extern	void		free_application(iapplication_t *);
extern	unsigned long	getnew_ssrc(vapplication_t *);
extern	iapplication_t	*new_application(vapplication_t *);
extern	vconnection_t	*new_connection(iapplication_t *, struct in_addr *);
extern	int		SendCtrl(iapplication_t *);

extern	int		voip_application_handler(iapplication_t *, int,
				unsigned char *);
extern	int		setup_voip(iapplication_t *, bchannel_t *);
extern	int		close_voip(iapplication_t *, bchannel_t *);

extern	int		setup_voip_ocall(iapplication_t *, bchannel_t *);
extern	int		alert_voip(iapplication_t *, bchannel_t *);
extern	int		facility_voip(iapplication_t *, bchannel_t *);
extern	int		useruser_voip(iapplication_t *, bchannel_t *);
extern	int		connect_voip(iapplication_t *, bchannel_t *);
extern	int		disconnect_voip(iapplication_t *, bchannel_t *);
extern	int		release_voip(iapplication_t *, bchannel_t *);

#endif
