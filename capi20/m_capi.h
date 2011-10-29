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
#include <mISDN/mISDNif.h>
#include <mISDN/mlayer3.h>
#include <mISDN/q931.h>
#include <capi20.h>
#include "mc_buffer.h"
#include "../lib/include/fsm.h"
#include "../lib/include/debug.h"

struct mApplication;
struct mPLCI;
struct lPLCI;
struct mNCCI;
struct pController;
struct lController;
struct BInstance;

struct Bprotocol {
	uint16_t	B1;
	uint16_t	B2;
	uint16_t	B3;
	unsigned char	B1cfg[16];
	unsigned char	B2cfg[16];
	unsigned char	B3cfg[80];
};

typedef int (BDataTrans_t)(struct BInstance *, struct mc_buf *);

struct BInstance {
	int 			nr;
	int			usecnt;
	int			proto;
	int			fd;
	uint16_t		DownId;	/* Ids for send down messages */
	uint16_t		UpId;	/* Ids for send up messages */
	struct pController	*pc;
	struct lPLCI		*lp;
	struct mNCCI		*nc;
	BDataTrans_t		*from_down;
};

int OpenBInstance(struct BInstance *, struct lPLCI *, BDataTrans_t *);
int CloseBInstance(struct BInstance *);

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
	int			mNr;
	int			enable;
	struct mISDN_devinfo	devinfo;
	struct capi_profile	profile;
	uint32_t		L3Proto;
	uint32_t		L3Flags;
	struct mlayer3		*l3;
	struct lController	*lClist;
	struct mPLCI		*plciL;		/* List of PLCIs */
	int			appCnt;
	int			BImax;		/* Nr of BInstances */
	struct BInstance 	*BInstances;	/* Array of BInstances [0 ... BImax - 1] */
	uint32_t		InfoMask;	/* Listen info mask all active applications */
	uint32_t		CIPmask;	/* Listen CIP mask all active applications */
	uint32_t		CIPmask2;	/* Listen CIP mask 2 all active applications */
};

struct pController *get_mController(int);
struct pController *get_cController(int);
struct BInstance *ControllerSelChannel(struct pController *, int, int);

/* This is a struct for the logical controller per application, also has the listen statemachine */
struct lController {
	struct lController	*nextC;
	struct lController	*nextA;
	int			refc;		/* refcount */
	struct pController	*Contr;		/* pointer to the physical controler */
	struct mApplication	*Appl;		/* pointer to the CAPI application */
	struct FsmInst		listen_m;	/* Listen state machine */
	uint32_t		InfoMask;	/* Listen info mask */
	uint32_t		CIPmask;	/* Listen CIP mask */
	uint32_t		CIPmask2;	/* Listen CIP mask 2 */
};

/* listen.c */
struct lController *get_lController(struct mApplication *, int);
void init_listen(void);
void free_listen(void);
struct lController *addlController(struct mApplication *, struct pController *);
void rm_lController(struct lController *lc);
int listenRequest(struct lController *, struct mc_buf *);

struct mApplication {
	struct mApplication	*next;
	int			refc;	/* refcount */
	int			fd;	/* Filedescriptor for CAPI messages */
	struct lController	*contL;	/* list of controllers */
	uint16_t		AppId;
	uint16_t		MsgId;	/* next message number */
	int			MaxB3Con;
	int			MaxB3Blk;
	int			MaxB3Size;
};

struct mApplication *RegisterApplication(uint16_t, uint32_t, uint32_t, uint32_t);
void ReleaseApplication(struct mApplication *);
int PutMessageApplication(struct mApplication *, struct mc_buf *);
void SendMessage2Application(struct mApplication *, struct mc_buf *);
void SendCmsg2Application(struct mApplication *, struct mc_buf *);
void SendCmsgAnswer2Application(struct mApplication *, struct mc_buf *, uint16_t);
int ListenController(struct pController *);

struct mPLCI {
	struct mPLCI		*next;
	uint32_t		plci;	/* PLCI ID */
	int			pid;	/* L3 pid */
	struct pController	*pc;
	int nAppl;
	struct lPLCI		*lPLCIs;
	unsigned int		alerting:1;
	unsigned int		outgoing:1;
};

/* PLCI state flags */

struct mPLCI *new_mPLCI(struct pController *, int, struct lPLCI *);
int free_mPLCI(struct mPLCI *);
void plciDetachlPLCI(struct lPLCI *);
struct mPLCI *getPLCI4Id(struct pController *, uint32_t);
struct lPLCI *get_lPLCI4Id(struct mPLCI *, uint16_t);
struct mPLCI *getPLCI4pid(struct pController *, int);
int mPLCISendMessage(struct lController *, struct mc_buf *);
int plciL4L3(struct mPLCI *, int, struct l3_msg *);
int plci_l3l4(struct mPLCI *, int, struct l3_msg *);

struct lPLCI {
	struct lPLCI			*next;
	uint32_t			plci;		/* PLCI ID */
	int				pid;		/* L3 pid */
	struct lController		*lc;
	struct mPLCI			*PLCI;
	struct FsmInst			plci_m;
	struct BInstance		*BIlink;
	int NcciCnt;
	struct mNCCI			*Nccis;
	int				cause;
	int				cause_loc;
	struct misdn_channel_info	chid;
	struct Bprotocol		Bprotocol;
	unsigned int			l1dtmf:1;
};

void init_lPLCI_fsm(void);
void free_lPLCI_fsm(void);
int lPLCICreate(struct lPLCI **, struct lController *, struct mPLCI *);
void lPLCI_free(struct lPLCI *);
void lPLCI_l3l4(struct lPLCI *, int, struct mc_buf *);
uint16_t lPLCISendMessage(struct lPLCI *, struct mc_buf *);
uint16_t q931CIPValue(struct mc_buf *);
struct mNCCI *getNCCI4addr(struct lPLCI *, uint32_t, int);
void lPLCIDelNCCI(struct mNCCI *);
struct mNCCI *ConnectB3Request(struct lPLCI *, struct mc_buf *);

#define GET_NCCI_EXACT		1
#define GET_NCCI_ONLY_PLCI	2
#define GET_NCCI_PLCI		3

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
	struct mNCCI		*next;
	uint32_t		ncci;
	struct lPLCI		*lp;
	struct mApplication	*appl;
	struct BInstance	*BIlink;
	int			window;
	struct FsmInst		ncci_m;
	pthread_mutex_t		lock;
	struct _ConfQueue	xmit_handles[CAPI_MAXDATAWINDOW];
	uint32_t		recv_handles[CAPI_MAXDATAWINDOW];
	enum _flowmode		flowmode;
	uint16_t isize;
	uint16_t osize;
	uint16_t iidx;
	uint16_t oidx;
	struct msghdr		down_msg;
	struct iovec		down_iv[3];
	struct mISDNhead	down_header;
	struct msghdr		up_msg;
	struct iovec		up_iv[2];
	unsigned char		up_header[30];
	unsigned int		dtmflisten:1;
	unsigned int		l1direct:1;
	unsigned int		l2trans:1;
	unsigned int		l3trans:1;
	unsigned int		dlbusy:1;
};

void init_ncci_fsm(void);
void free_ncci_fsm(void);
struct mNCCI *ncciCreate(struct lPLCI *);
void ncciFree(struct mNCCI *);
int ncciSendMessage(struct mNCCI *, uint8_t, uint8_t, struct mc_buf *);
int recvBdirect(struct BInstance *, struct mc_buf *);
void ncciReleaseLink(struct mNCCI *);
void ncciDel_lPlci(struct mNCCI *);

#define MC_BUF_ALLOC(a) if (!(a = alloc_mc_buf())) {eprint("Cannot allocate mc_buff\n");return;}

#define CMSGCMD(cm)		CAPICMD((cm)->Command, (cm)->Subcommand)

/* Debug MASK */
#define MC_DEBUG_POLL		0x01
#define MC_DEBUG_CONTROLLER	0x02
#define MC_DEBUG_STATES		0x04
#define MC_DEBUG_PLCI		0x08
#define MC_DEBUG_NCCI		0x10
#define MC_DEBUG_NCCI_DATA	0x20

#define MIDEBUG_POLL		(MC_DEBUG_POLL << 24)
#define MIDEBUG_CONTROLLER	(MC_DEBUG_CONTROLLER << 24)
#define MIDEBUG_STATES		(MC_DEBUG_STATES << 24)
#define MIDEBUG_PLCI		(MC_DEBUG_PLCI << 24)
#define MIDEBUG_NCCI		(MC_DEBUG_NCCI << 24)
#define MIDEBUG_NCCI_DATA	(MC_DEBUG_NCCI_DATA << 24)

/* missing capi errors */
#define CapiMessageNotSupportedInCurrentState	0x2001
#define CapiIllController			0x2002
#define CapiNoPlciAvailable			0x2003
#define CapiIllMessageParmCoding		0x2007

#define CapiB1ProtocolNotSupported		0x3001
#define CapiB2ProtocolNotSupported		0x3002
#define CapiB3ProtocolNotSupported		0x3003
#define CapiB1ProtocolParameterNotSupported	0x3004
#define CapiB2ProtocolParameterNotSupported	0x3005
#define CapiB3ProtocolParameterNotSupported	0x3006

#define CapiProtocolErrorLayer1			0x3301
#define CapiProtocolErrorLayer2			0x3302
#define CapiProtocolErrorLayer3			0x3303

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

#define CAPI_B3_DATA_IND_HEADER_SIZE	((4 == sizeof(void *)) ? 22 : 30)

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
