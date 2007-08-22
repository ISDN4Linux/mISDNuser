/* $Id: mISDNif.h,v 2.0 2007/06/06 15:39:31 kkeil Exp $
 *
 * Author	Karsten Keil <kkeil@novell.com>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
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

#ifndef mISDNIF_H
#define mISDNIF_H

#include <stdarg.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/socket.h>

/*
 * ABI Version 32 bit
 *
 * <8 bit> Major version
 *		- changed if any interface become backwards incompatible
 *
 * <8 bit> Minor version
 *              - changed if any interface is extended but backwards compatible
 *
 * <16 bit> Release number
 *              - should be incremented on every checkin
 */
#define	MISDN_MAJOR_VERSION	1
#define	MISDN_MINOR_VERSION	0
#define MISDN_RELEASE		15

#define MISDN_REVISION		"$Revision: 2.0 $"
#define MISDN_DATE		"$Date: 2007/06/05 15:39:31 $"


#define MISDN_MSG_STATS

/* collect some statistics about the message queues */
//#define MISDN_MSG_STATS

/* primitives for information exchange
 * generell format
 * <16  bit  0 >
 * <8  bit command>
 *    BIT 8 = 1 LAYER private
 *    BIT 7 = 1 answer
 *    BIT 6 = 1 DATA 
 * <8  bit target layer mask>
 *
 * Layer = 00 is reserved for general commands
   Layer = 01  L2 -> HW
   Layer = 02  HW -> L2
   Layer = 04  L3 -> L2
   Layer = 08  L2 -> L3
 * Layer = FF is reserved for broadcast commands
 */

#define MISDN_CMDMASK		0xff00
#define MISDN_LAYERMASK		0x00ff

/* generell commands */
#define OPEN_CHANNEL		0x0100
#define CLOSE_CHANNEL		0x0200
#define CONTROL_CHANNEL		0x0300
#define CHECK_DATA		0x0400

/* layer 2 -> layer 1 */
#define PH_ACTIVATE_REQ		0x0101
#define PH_DEACTIVATE_REQ	0x0201
#define PH_DATA_REQ		0x2001
#define MPH_ACTIVATE_REQ	0x0501
#define MPH_DEACTIVATE_REQ	0x0601
#define MPH_INFORMATION_REQ	0x0701
#define PH_CONTROL_REQ		0x0801

/* layer 1 -> layer 2 */
#define PH_ACTIVATE_IND		0x0102
#define PH_DEACTIVATE_IND	0x0202
#define PH_DATA_IND		0x2002
#define MPH_ACTIVATE_IND	0x0502
#define MPH_DEACTIVATE_IND	0x0602
#define MPH_INFORMATION_IND	0x0702
#define PH_DATA_CNF		0x6002
#define PH_CONTROL_IND		0x0802

/* layer 3 -> layer 2 */
#define DL_ESTABLISH_REQ	0x1004
#define DL_RELEASE_REQ		0x1104
#define DL_DATA_REQ		0x3004
#define DL_UNITDATA_REQ		0x3104
#define DL_INFORMATION_REQ	0x0004

/* layer 2 -> layer 3 */
#define DL_ESTABLISH_IND	0x1008
#define DL_ESTABLISH_CNF	0x5008
#define DL_RELEASE_IND		0x1108
#define DL_RELEASE_CNF		0x5108
#define DL_DATA_IND		0x3008
#define DL_UNITDATA_IND		0x3108
#define DL_INFORMATION_IND	0x0008

/* intern layer 2 managment */
#define MDL_ASSIGN_REQ		0x1804
#define MDL_ASSIGN_IND		0x1904
#define MDL_REMOVE_REQ		0x1A04
#define MDL_REMOVE_IND		0x1B04
#define MDL_STATUS_UP_IND	0x1C04
#define MDL_STATUS_DOWN_IND	0x1D04
#define MDL_STATUS_UI_IND	0x1E04
#define MDL_ERROR_IND		0x1F04
#define MDL_ERROR_RSP		0x5F04

/* DL_INFORMATION_IND types */
#define DL_INFO_L2_CONNECT	0x0001
#define DL_INFO_L2_REMOVED	0x0002

/* PH_CONTROL types */
/* TOUCH TONE IS 0x20XX  XX "0"..."9", "A","B","C","D","*","#" */
#define DTMF_TONE_VAL		0x2000
#define DTMF_TONE_MASK		0x007F
#define DTMF_TONE_START		0x2100
#define DTMF_TONE_STOP		0x2200
#define DTMF_COEF		0x4000

/* MPH_INFORMATION_IND */
#define L1_SIGNAL_LOS_OFF	0x0010
#define L1_SIGNAL_LOS_ON	0x0011
#define L1_SIGNAL_AIS_OFF	0x0012
#define L1_SIGNAL_AIS_ON	0x0013
#define L1_SIGNAL_SLIP_RX	0x0020
#define L1_SIGNAL_SLIP_TX	0x0021

/* 
 * protocol ids
 * D channel 1-31
 * B channel 33 - 63
 */

#define ISDN_P_NONE		0
#define ISDN_P_BASE		0
#define ISDN_P_TE_S0		0x01
#define ISDN_P_NT_S0  		0x02
#define ISDN_P_LAPD_TE		0x10
#define	ISDN_P_LAPD_NT		0x11	

#define ISDN_P_B_MASK		0x1f
#define ISDN_P_B_START		0x20

#define ISDN_P_B_RAW		0x21
#define ISDN_P_B_HDLC		0x22
#define ISDN_P_B_X75SLP		0x23
#define ISDN_P_B_L2DTMF		0x24
#define ISDN_P_B_L2DSP		0x25

#define OPTION_L2_PMX		1
#define OPTION_L2_PTP		2
#define OPTION_L2_FIXEDTEI	3
#define OPTION_L2_CLEANUP	4

/* should be in sync with linux/kobject.h:KOBJ_NAME_LEN */
#define MISDN_MAX_IDLEN		20

struct mISDNhead {
	unsigned int	prim;
	unsigned int	id;
}  __attribute__((packed));

#define MISDN_HEADER_LEN	sizeof(struct mISDNhead)
#define MAX_DATA_SIZE		2048
#define MAX_DATA_MEM		(MAX_DATA_SIZE + MISDN_HEADER_LEN)
#define MAX_DFRAME_LEN		260

#define MISDN_ID_ADDR_MASK	0xFFFF
#define MISDN_ID_TEI_MASK	0xFF00
#define MISDN_ID_SAPI_MASK	0x00FF
#define MISDN_ID_TEI_ANY	0x7F00

#define MISDN_ID_ANY		0xFFFF
#define MISDN_ID_NONE		0xFFFE

#define GROUP_TEI		127
#define TEI_SAPI		63
#define CTRL_SAPI		0

#define MISDN_CHMAP_SIZE	16

/* socket */
#ifndef	AF_ISDN
#define AF_ISDN		27
#define PF_ISDN		AF_ISDN
#endif

#define SOL_MISDN	0

struct sockaddr_mISDN {
	sa_family_t    family;
	unsigned char	dev;
	unsigned char	channel;
	unsigned char	sapi;
	unsigned char	tei;
};

/* timer device ioctl */
#define IMADDTIMER	_IOR('I', 64, int)
#define IMDELTIMER	_IOR('I', 65, int)
/* socket ioctls */
#define	IMGETVERSION	_IOR('I', 66, int)
#define	IMGETCOUNT	_IOR('I', 67, int)
#define IMGETDEVINFO	_IOR('I', 68, int)
#define IMCTRLREQ	_IOR('I', 69, int)
#define IMCLEAR_L2	_IOR('I', 70, int)

struct mISDNversion {
	unsigned char	major;
	unsigned char	minor;
	unsigned short	release;
};

struct mISDN_devinfo {
	u_int			id;
	u_int			Dprotocols;
	u_int			Bprotocols;
	u_int			protocol;
	u_char			channelmap[MISDN_CHMAP_SIZE];
	u_int			nrbchan;
	char			name[MISDN_MAX_IDLEN];
};

#define MISDN_CTRL_GETOP		0x0000
#define MISDN_CTRL_LOOP			0x0001
#define MISDN_CTRL_CONNECT		0x0002
#define MISDN_CTRL_DISCONNECT		0x0004
#define MISDN_CTRL_PCMCONNECT		0x0010
#define MISDN_CTRL_PCMDISCONNECT	0x0020


/* socket options */
#define MISDN_TIME_STAMP		0x0001

struct mISDN_ctrl_req {
	int		op;
	int		channel;
	int		p1;
	int		p2;
};

/* muxer options */
#define MISDN_OPT_ALL		1
#define MISDN_OPT_TEIMGR	2

#endif /* mISDNIF_H */
