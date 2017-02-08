/* m_capi.h
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

#ifndef _M_CAPI_H
#define _M_CAPI_H

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <semaphore.h>
#include <mISDN/mISDNif.h>
#include <mISDN/mlayer3.h>
#include <mISDN/q931.h>
#include <capi20.h>
#include "mc_buffer.h"
#include "../lib/include/fsm.h"
#include "../lib/include/debug.h"
//#include "../lib/include/mlist.h"
#include "../lib/include/helper.h"

/* Some DEBUG features defines */
/* Refcounting functions will log, if debug mask bit 31 is set */
#define MISDN_CAPI_REFCOUNT_DEBUG	1

/* this define is only for developing if double frees are detected, it never free any object !!! - big memory leak */
/* #define MISDN_CAPIOBJ_NO_FREE 1 */

/* Some globals */
extern int	KeepTemporaryFiles;
extern int	WriteWaveFiles;
extern char	*TempDirectory;
extern pid_t	gettid(void);

/* Master control defines */

#define MICD_EV_MASK		0xffff0000
#define MICD_EV_LEN		0x0000ffff
#define MICD_CTRL_SHUTDOWN	0x42010000
#define MICD_CTRL_DISABLE_POLL	0x42020000
#define MICD_CTRL_ENABLE_POLL	0x42030000

int send_master_control(int, int, void *);


struct mCAPIobj;
struct mApplication;
struct mPLCI;
struct lPLCI;
struct mNCCI;
struct pController;
struct lController;
struct BInstance;

extern int mI_ControllerCount;
extern struct timer_base *mICAPItimer_base;

struct Bprotocol {
	uint16_t	B1;
	uint16_t	B2;
	uint16_t	B3;
	unsigned char	B1cfg[16];
	unsigned char	B2cfg[16];
	unsigned char	B3cfg[132];
};

typedef int (BDataTrans_t)(struct BInstance *, struct mc_buf *);

enum BType {
	BType_None = 0,
	BType_Direct = 1,
	BType_Fax = 2,
	BType_tty = 3
};

const char *BItype2str(enum BType);

enum eCAPIobjtype {
	Cot_None = 0,
	Cot_Root,
	Cot_Application,
	Cot_lController,
	Cot_PLCI,
	Cot_lPLCI,
	Cot_NCCI,
	Cot_FAX
};
#define Cot_Last	Cot_FAX

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
#define CAPIobj_IDSIZE		48
#else
#define CAPIobj_IDSIZE		32
#endif

struct mCAPIobj	{
	struct mCAPIobj		*next;
	struct mCAPIobj		*nextD;
	enum eCAPIobjtype	type;
	int			refcnt;
	struct mCAPIobj		*parent;
	pthread_rwlock_t	lock;
	struct mCAPIobj		*listhead;
	int			itemcnt;
	unsigned int		id;
	unsigned int		id2;
	unsigned int		uid;
	unsigned int		cleaned:1;
	unsigned int		unlisted:1;
	unsigned int		freeing:1;
	unsigned int		freed:1;
	char			idstr[CAPIobj_IDSIZE];
#ifdef MISDN_CAPIOBJ_NO_FREE
	void			*freep;
#endif
};

#if __GNUC_PREREQ (3,4)
# define __WUR  __attribute__ ((__warn_unused_result__))
#else
# define __WUR
#endif


#ifdef MISDN_CAPI_REFCOUNT_DEBUG
struct mCAPIobj *__get_cobj(struct mCAPIobj *, const char *, int) __WUR;
int __put_cobj(struct mCAPIobj *, const char *, int);
struct mCAPIobj *__get_next_cobj(struct mCAPIobj *, struct mCAPIobj *, const char *, int) __WUR;
int __delist_cobj(struct mCAPIobj *, const char *, int);
#define get_cobj(co)		__get_cobj(co, __FILE__, __LINE__)
#define put_cobj(co)		__put_cobj(co, __FILE__, __LINE__)
#define get_next_cobj(pa, co)	__get_next_cobj(pa, co, __FILE__, __LINE__)
#define delist_cobj(co)		__delist_cobj(co, __FILE__, __LINE__)
#else
struct mCAPIobj *get_cobj(struct mCAPIobj *) __WUR;
int put_cobj(struct mCAPIobj *);
struct mCAPIobj *get_next_cobj(struct mCAPIobj *, struct mCAPIobj *) __WUR;
int delist_cobj(struct mCAPIobj *);
#endif

void dump_cobjects(void);
void CAPIobj_init(void);
void CAPIobj_exit(void);

void free_capiobject(struct mCAPIobj *, void *);
#ifdef MISDN_CAPIOBJ_NO_FREE
void dump_cobjects_free(void);
#endif

int init_cobj(struct mCAPIobj *, struct mCAPIobj *, enum eCAPIobjtype, unsigned int, unsigned int);
int init_cobj_registered(struct mCAPIobj *, struct mCAPIobj *, enum eCAPIobjtype cot, unsigned int);
const char *CAPIobjt2str(struct mCAPIobj *);
const char *CAPIobjIDstr(struct mCAPIobj *);

struct BInstance {
	int 			nr;
	int			usecnt;
	int			proto;
	int			fd;
	int			tty;
	int			tty_received;
	int			rx_min;
	int			rx_max;
	int			org_rx_min;
	int			org_rx_max;
	enum BType		type;
	uint16_t		DownId;	/* Ids for send down messages */
	uint16_t		UpId;	/* Ids for send up messages */
	struct pController	*pc;
	struct lPLCI		*lp;
	void			*b3data;
	pthread_mutex_t		lock;
	BDataTrans_t		*from_down;
	BDataTrans_t		*from_up;
	pthread_t		thread;
	pid_t			tid;
	struct pollfd		pfd[4];
	int			pcnt;
	int			timeout;
	int			cpipe[2];
	sem_t			wait;
	unsigned int		running:1;
	unsigned int		waiting:1;
	unsigned int		release_pending:1;
	unsigned int		got_timeout:1;
	unsigned int		closing:1;
	unsigned int		detached:1;
	unsigned int		joined:1;
	unsigned int		closed:1;
};


#define DEFAULT_FAX_PKT_SIZE	512


int OpenBInstance(struct BInstance *, struct lPLCI *);
int CloseBInstance(struct BInstance *);
int ReleaseBchannel(struct BInstance *);
int activate_bchannel(struct BInstance *);

struct capi_profile {
	uint16_t ncontroller;	/* number of installed controller */
	uint16_t nbchannel;	/* number of B-Channels */
	uint32_t goptions;	/* global options */
	uint32_t support1;	/* B1 protocols support */
	uint32_t support2;	/* B2 protocols support */
	uint32_t support3;	/* B3 protocols support */
	uint32_t reserved[6];	/* reserved */
	uint32_t manu[5];	/* manufacturer specific information */
};

/* physical controller access */
struct pController {
	struct mCAPIobj         cobjLC;
	struct mCAPIobj		cobjPLCI;
	int			mNr;
	int			enable;
	struct mISDN_devinfo	devinfo;
	struct capi_profile	profile;
	uint32_t		L3Proto;
	uint32_t		L3Flags;
	struct mlayer3		*l3;
	uint32_t		lastPLCI;	/* used only in unique PLCI debugmode */
	int			appCnt;
	int			BImax;		/* Nr of BInstances */
	struct BInstance 	*BInstances;	/* Array of BInstances [0 ... BImax - 1] */
	pthread_rwlock_t	Block;
	uint32_t		InfoMask;	/* Listen info mask all active applications */
	uint32_t		CIPmask;	/* Listen CIP mask all active applications */
	uint32_t		CIPmask2;	/* Listen CIP mask 2 all active applications */
};

struct pController *get_mController(int);
struct pController *get_cController(int);
struct BInstance *ControllerSelChannel(struct pController *, int, int);
int ControllerDeSelChannel(struct BInstance *);
uint32_t NextFreePLCI(struct mCAPIobj *);
int OpenLayer3(struct pController *);
int check_free_bchannels(struct pController *);
void dump_controller_plci(struct pController *);

/* This is a struct for the logical controller per application, also has the listen statemachine */
struct lController {
	struct mCAPIobj		cobj;
	struct mApplication	*Appl;		/* pointer to the CAPI application */
	struct FsmInst		listen_m;	/* Listen state machine */
	uint32_t		InfoMask;	/* Listen info mask */
	uint32_t		CIPmask;	/* Listen CIP mask */
	uint32_t		CIPmask2;	/* Listen CIP mask 2 */
	unsigned int		listed;
};

#define p4lController(l)	((l) ? container_of((l)->cobj.parent, struct pController, cobjLC) : NULL)

/* listen.c */
struct lController *get_lController(struct mApplication *, unsigned int);
void init_listen(void);
void free_listen(void);
struct lController *addlController(struct mApplication *, struct pController *, int);
void cleanup_lController(struct lController *);
void Free_lController(struct mCAPIobj *);
int listenRequest(struct lController *, struct mc_buf *);
void dump_lControllers(struct pController *);
void dump_lcontroller(struct lController *);

struct mApplication {
	struct mCAPIobj		cobj;
	int			fd;	/* Filedescriptor for CAPI messages */
	uint16_t		MsgId;	/* next message number */
	int			MaxB3Con;
	int			MaxB3Blk;
	int			MaxB3Size;
	struct lController	**lcl;
	uint32_t		UserFlags;
	int			cpipe[2];
	unsigned int		unregistered:1;
};

int mApplication_init(void);
struct mApplication *RegisterApplication(uint16_t, uint32_t, uint32_t, uint32_t);
void ReleaseApplication(struct mApplication *, int);
void Free_Application(struct mCAPIobj *);
int ReleaseAllApplications(void);
int register_lController(struct mApplication *, struct lController *);
void delisten_application(struct lController *);
int PutMessageApplication(struct mApplication *, struct mc_buf *);
void SendMessage2Application(struct mApplication *, struct mc_buf *);
void SendCmsg2Application(struct mApplication *, struct mc_buf *);
void SendCmsgAnswer2Application(struct mApplication *, struct mc_buf *, uint16_t);
int ListenController(struct pController *);
void Put_Application_cleaned(struct mCAPIobj *);
void dump_applications(void);

struct mPLCI {
	struct mCAPIobj		cobj;
	struct pController	*pc;
	unsigned int		alerting:1;
	unsigned int		outgoing:1;
	int			cause;
	int			cause_loc;
};

/* PLCI state flags */

struct mPLCI *new_mPLCI(struct pController *, unsigned int);
void plciDetachlPLCI(struct lPLCI *);
void Free_PLCI(struct mCAPIobj *);
unsigned int plci_new_pid(struct mPLCI *);
struct mPLCI *getPLCI4Id(struct pController *, uint32_t);
struct lPLCI *get_lPLCI4Id(struct mPLCI *, uint16_t);
struct mPLCI *getPLCI4pid(struct pController *, int);
int mPLCISendMessage(struct lController *, struct mc_buf *);
int plciL4L3(struct mPLCI *, int, struct l3_msg *);
int plci_l3l4(struct mPLCI *, int, struct l3_msg *);
void release_lController(struct lController *);

struct lPLCI {
	struct mCAPIobj			cobj;
	int				pid;		/* L3 pid */
	struct lController		*lc;
	struct mApplication		*Appl;
	struct FsmInst			plci_m;
	int				proto;
	enum BType			btype;
	struct BInstance		*BIlink;
	struct mtimer			atimer;
	int				cause;
	int				cause_loc;
	struct misdn_channel_info	chid;
	struct Bprotocol		Bprotocol;
	uint32_t			cipmask;
	unsigned int			l1dtmf:1;
	unsigned int			autohangup:1;
	unsigned int			disc_req:1;
	unsigned int			rel_req:1;
	unsigned int			ignored:1;
	unsigned int			req_relcomplete:1;
};

#define p4lPLCI(l)	((l) ? container_of(((struct lPLCI *)(l))->cobj.parent, struct mPLCI, cobj) : NULL)
#define pc4lPLCI(l)	((struct mPLCI *)p4lPLCI(l))->pc

void init_lPLCI_fsm(void);
void free_lPLCI_fsm(void);
int lPLCICreate(struct lPLCI **, struct lController *, struct mPLCI *, uint32_t);
void cleanup_lPLCI(struct lPLCI *);
void Free_lPLCI(struct mCAPIobj *);
void lPLCIRelease(struct lPLCI *);
void lPLCI_l3l4(struct lPLCI *, int, struct mc_buf *);
uint16_t lPLCISendMessage(struct lPLCI *, struct mc_buf *);
uint32_t q931CIPMask(struct mc_buf *);
uint16_t CIPMask2CIPValue(uint32_t);
void lPLCIDelNCCI(struct mNCCI *);
struct mNCCI *ConnectB3Request(struct lPLCI *, struct mc_buf *);
void B3ReleaseLink(struct lPLCI *, struct BInstance *);
struct lPLCI *get_lPLCI4plci(struct mApplication *, uint32_t);
void dump_Lplcis(struct lPLCI *);

struct _ConfQueue {
	uint32_t	PktId;
	uint16_t	DataHandle;
	uint16_t	MsgId;
	uint16_t	dlen;
	uint16_t	sent;
	struct mc_buf	*pkt;
	unsigned char	*sp;
};

#define CAPI_MAXDATAWINDOW	8

enum _flowmode {
	flmNone = 0,
	flmPHDATA = 1,
	flmIndication = 2,
};

struct mNCCI {
	struct mCAPIobj		cobj;
	struct mApplication	*appl;
	struct BInstance	*BIlink;
	int			window;
	struct FsmInst		ncci_m;
	pthread_mutex_t		lock;
	struct _ConfQueue	xmit_handles[CAPI_MAXDATAWINDOW];
	uint32_t		recv_handles[CAPI_MAXDATAWINDOW];
	enum _flowmode		flowmode;
	_cbyte			*ncpi;
	uint16_t		Reason_B3;
	uint16_t		isize;
	uint16_t		osize;
	uint16_t		iidx;
	uint16_t		oidx;
	int			ridx;
	struct msghdr		down_msg;
	struct iovec		down_iv[3];
	struct mISDNhead	down_header;
	struct msghdr		up_msg;
	struct iovec		up_iv[2];
	unsigned char		up_header[30];
	unsigned int		dtmflisten:1;
	unsigned int		l1direct:1;
	unsigned int		l1trans:1;
	unsigned int		l2trans:1;
	unsigned int		l3trans:1;
	unsigned int		dlbusy:1;
};

#define lPLCI4NCCI(n)	((n) ? container_of(((struct mNCCI *)(n))->cobj.parent, struct lPLCI, cobj) : NULL)
void init_ncci_fsm(void);
void free_ncci_fsm(void);
struct mNCCI *ncciCreate(struct lPLCI *);
void Free_NCCI(struct mCAPIobj *);
int recvBdirect(struct BInstance *, struct mc_buf *);
int ncciB3Data(struct BInstance *, struct mc_buf *);
void ncciReleaseLink(struct mNCCI *);
void cleanup_ncci(struct mNCCI *);
int ncciL4L3(struct mNCCI *, uint32_t, int, int, void *, struct mc_buf *);
void dump_ncci(struct lPLCI *);

#ifdef USE_SOFTFAX
int FaxRecvBData(struct BInstance *, struct mc_buf *);
int FaxB3Message(struct BInstance *, struct mc_buf *);
void FaxReleaseLink(struct BInstance *);
void Free_Faxobject(struct mCAPIobj *);

void dump_fax_status(struct BInstance *);
#else
static inline void Free_Faxobject(struct mCAPIobj *co) {};
static inline void dump_fax_status(struct BInstance *bi) {};
#endif

#define MC_BUF_ALLOC(a) if (!(a = alloc_mc_buf())) {eprint("Cannot allocate mc_buff\n");return;}

#define CMSGCMD(cm)		CAPICMD((cm)->Command, (cm)->Subcommand)

/* Debug MASK */
#define MC_DEBUG_POLL		0x01
#define MC_DEBUG_CONTROLLER	0x02
#define MC_DEBUG_CAPIMSG	0x04
#define MC_DEBUG_STATES		0x08
#define MC_DEBUG_PLCI		0x10
#define MC_DEBUG_NCCI		0x20
#define MC_DEBUG_NCCI_DATA	0x40
#define MC_DEBUG_CAPIOBJ	0x80

#define MIDEBUG_POLL		(MC_DEBUG_POLL << 24)
#define MIDEBUG_CONTROLLER	(MC_DEBUG_CONTROLLER << 24)
#define MIDEBUG_CAPIMSG		(MC_DEBUG_CAPIMSG << 24)
#define MIDEBUG_STATES		(MC_DEBUG_STATES << 24)
#define MIDEBUG_PLCI		(MC_DEBUG_PLCI << 24)
#define MIDEBUG_NCCI		(MC_DEBUG_NCCI << 24)
#define MIDEBUG_NCCI_DATA	(MC_DEBUG_NCCI_DATA << 24)
#define MIDEBUG_CAPIOBJ		(MC_DEBUG_CAPIOBJ << 24)

#define MI_PUT_APPLICATION	0x42000000

int mIcapi_mainpoll_releaseApp(int, int);

void mCapi_cmsg2str(struct mc_buf *);
void mCapi_message2str(struct mc_buf *);

/* missing capi errors */
#define CapiMessageNotSupportedInCurrentState	0x2001
#define CapiIllController			0x2002
#define CapiNoPLCIAvailable			0x2003
#define CapiNoNCCIAvailable			0x2004
#define CapiIllMessageParmCoding		0x2007

#define CapiB1ProtocolNotSupported		0x3001
#define CapiB2ProtocolNotSupported		0x3002
#define CapiB3ProtocolNotSupported		0x3003
#define CapiB1ProtocolParameterNotSupported	0x3004
#define CapiB2ProtocolParameterNotSupported	0x3005
#define CapiB3ProtocolParameterNotSupported	0x3006
#define CapiProtocolCombinationNotSupported	0x3007
#define CapiNCPINotSupported			0x3008

#define CapiProtocolErrorLayer1			0x3301
#define CapiProtocolErrorLayer2			0x3302
#define CapiProtocolErrorLayer3			0x3303
#define CapiConnectionNoSuccess_noG3		0x3311
#define CapiConnectionNoSuccess_TrainingErr	0x3312
#define CapiDisconnectBeforeTrans_Unsuppoted	0x3313
#define CapiDisconnectDuringTrans_RemoteAbort	0x3314
#define CapiDisconnectDuringTrans_ProcedureErr	0x3315
#define CapiDisconnectDuringTrans_TXunderflow	0x3316
#define CapiDisconnectDuringTrans_RXoverflow	0x3317
#define CapiDisconnectDuringTrans_LocalAbort	0x3318
#define CapiIllegalParameterCoding		0x3319

/* internal used errors */
#define CapiBchannelNotAvailable		0x3f01


#define FAX_B3_FORMAT_SFF	0
#define FAX_B3_FORMAT_PLAIN	1
#define FAX_B3_FORMAT_PCX	2
#define FAX_B3_FORMAT_DCX	3
#define FAX_B3_FORMAT_TIFF	4
#define FAX_B3_FORMAT_ASCII	5
#define FAX_B3_FORMAT_EXT_ANSI	6
#define FAX_B3_FORMAT_BINARY	7


/* Info mask bits */
#define CAPI_INFOMASK_CAUSE	0x0001
#define CAPI_INFOMASK_DATETIME	0x0002
#define CAPI_INFOMASK_DISPLAY	0x0004
#define CAPI_INFOMASK_USERUSER	0x0008
#define CAPI_INFOMASK_PROGRESS	0x0010
#define CAPI_INFOMASK_FACILITY	0x0020
#define CAPI_INFOMASK_CHARGE	0x0040
#define CAPI_INFOMASK_CALLEDPN	0x0080
#define CAPI_INFOMASK_CHANNELID	0x0100
#define CAPI_INFOMASK_EARLYB3	0x0200
#define CAPI_INFOMASK_REDIRECT	0x0400
#define CAPI_INFOMASK_COMPLETE	0x1000

#define CAPIMSG_REQ_DATAHANDLE(m)	(m[18] | (m[19]<<8))
#define CAPIMSG_RESP_DATAHANDLE(m)	(m[12] | (m[13]<<8))
#define CAPIMSG_REQ_FLAGS(m)		(m[20] | (m[21]<<8))

#define CAPI_B3_DATA_IND_HEADER_SIZE	((4 == sizeof(void *)) ? 22 : 30)


#define CAPIFLAG_HIGHJACKING	1

#define CAPI_DATA_TTY		0xe0

/* some helper */
static inline int capiEncodeWord(unsigned char *p, uint16_t i)
{
	*p++ = i;
	*p++ = i >> 8;
	return 2;
}

static inline int capiEncodeDWord(unsigned char *p, uint32_t i)
{
	*p++ = i;
	*p++ = i >> 8;
	*p++ = i >> 16;
	*p++ = i >> 24;
	return 4;
}

int capiEncodeFacIndSuspend(unsigned char *, uint16_t);

#endif
