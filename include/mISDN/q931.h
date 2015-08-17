/* q931.h
 *
 * Basic Q931 defines
 *
 * Author       Karsten Keil <kkeil@novell.com>
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

#ifndef _Q931_H
#define _Q931_H

#include <time.h>

/*
 * Q931 protocol discriminator
 */
#define Q931_PD			0x08
/*
 * Q931 Message-Types
 */

#define MT_ALERTING		0x01
#define MT_CALL_PROCEEDING	0x02
#define MT_CONNECT		0x07
#define MT_CONNECT_ACKNOWLEDGE	0x0f
#define MT_PROGRESS		0x03
#define MT_SETUP		0x05
#define MT_SETUP_ACKNOWLEDGE	0x0d
#define MT_RESUME		0x26
#define MT_RESUME_ACKNOWLEDGE	0x2e
#define MT_RESUME_REJECT	0x22
#define MT_SUSPEND		0x25
#define MT_SUSPEND_ACKNOWLEDGE	0x2d
#define MT_SUSPEND_REJECT	0x21
#define MT_USER_INFORMATION	0x20
#define MT_DISCONNECT		0x45
#define MT_RELEASE		0x4d
#define MT_RELEASE_COMPLETE	0x5a
#define MT_RESTART		0x46
#define MT_RESTART_ACKNOWLEDGE	0x4e
#define MT_SEGMENT		0x60
#define MT_CONGESTION_CONTROL	0x79
#define MT_INFORMATION		0x7b
#define MT_FACILITY		0x62
#define MT_NOTIFY		0x6e
#define MT_STATUS		0x7d
#define MT_STATUS_ENQUIRY	0x75
#define MT_HOLD			0x24
#define MT_HOLD_ACKNOWLEDGE	0x28
#define MT_HOLD_REJECT		0x30
#define MT_RETRIEVE		0x31
#define MT_RETRIEVE_ACKNOWLEDGE	0x33
#define MT_RETRIEVE_REJECT	0x37
#define MT_REGISTER		0x64


/*
 * Info Elements
 */
// not implemented 
// #define IE_SEGMENT		0x00
#define IE_BEARER		0x04
#define IE_CAUSE		0x08
#define IE_CALL_ID		0x10
#define IE_CALL_STATE		0x14
#define IE_CHANNEL_ID		0x18
#define IE_FACILITY		0x1c
#define IE_PROGRESS		0x1e
#define IE_NET_FAC		0x20
#define IE_NOTIFY		0x27
#define IE_DISPLAY		0x28
#define IE_DATE			0x29
#define IE_KEYPAD		0x2c
#define IE_SIGNAL		0x34
#define IE_INFORATE		0x40
#define IE_E2E_TDELAY		0x42
#define IE_TDELAY_SEL		0x43
#define IE_PACK_BINPARA		0x44
#define IE_PACK_WINSIZE		0x45
#define IE_PACK_SIZE		0x46
#define IE_CUG			0x47
#define	IE_REV_CHARGE		0x4a
#define IE_CONNECT_PN		0x4c
#define IE_CONNECT_SUB		0x4d
#define IE_CALLING_PN		0x6c
#define IE_CALLING_SUB		0x6d
#define IE_CALLED_PN		0x70
#define IE_CALLED_SUB		0x71
#define IE_REDIRECTING_NR	0x74
#define IE_REDIRECTION_NR	0x76
#define IE_TRANS_SEL		0x78
#define IE_RESTART_IND		0x79
#define IE_LLC			0x7c
#define IE_HLC			0x7d
#define IE_USER_USER		0x7e

#define	IE_COUNT		34

// not implemented
#define IE_ESCAPE		0x7f
// one octet IE
#define IE_SHIFT		0x90
#define IE_MORE_DATA		0xa0
#define IE_COMPLETE		0xa1
#define IE_CONGESTION		0xb0
// not allowed for ETSI
#define IE_REPEAT		0xd0

/*
 * weight for IE in check lists
 */
#define IE_MANDATORY		0x0100
/* mandatory not in every case */
#define IE_MANDATORY_1		0x0200

/*
 * Cause location
 */
#define CAUSE_LOC_USER		0
#define CAUSE_LOC_PRVN_LOCUSER	1
#define CAUSE_LOC_PUBN_LOCUSER	2
#define CAUSE_LOC_TRANSIT	3
#define CAUSE_LOC_PUBN_RMTUSER	4
#define CAUSE_LOC_PRVN_RMTUSER	5

/*
 * Cause values
 */
#define CAUSE_UNASSIGNED_NUMBER		1
#define CAUSE_NO_TRANSIT_ROUTE		2
#define CAUSE_NO_ROUTE			3
#define CAUSE_CHANNEL_UNACCEPT		6
#define CAUSE_NORMAL_CLEARING		16
#define CAUSE_USER_BUSY			17
#define CAUSE_NOUSER_RESPONDING		18
#define CAUSE_ALERTED_NO_ANSWER		19
#define CAUSE_CALL_REJECTED		21
#define CAUSE_NONSELECTED_USER		26
#define CAUSE_DEST_OUT_OF_ORDER		27
#define CAUSE_INVALID_NUMBER		28
#define CAUSE_FACILITY_REJECTED		29
#define CAUSE_STATUS_RESPONSE		30
#define CAUSE_NORMALUNSPECIFIED		31
#define CAUSE_NO_CHANNEL		34
#define CAUSE_NET_OUT_OF_ORDER 		28
#define CAUSE_TEMPORARY_FAILURE		41
#define CAUSE_SEQ_CONGESTION 		42
#define CAUSE_REQUESTED_CHANNEL		44
#define CAUSE_RESOURCES_UNAVAIL		47
#define CAUSE_FACILITY_NOTSUBSCRIBED	50
#define CAUSE_FACILITY_NOTIMPLEMENTED	69
#define CAUSE_INVALID_CALLREF		81
#define CAUSE_INCOMPATIBLE_DEST		88
#define CAUSE_MANDATORY_IE_MISS		96
#define CAUSE_MT_NOTIMPLEMENTED		97
#define CAUSE_NOTCOMPAT_STATE_OR_MT_NOTIMPLEMENTED 98
#define CAUSE_IE_NOTIMPLEMENTED		99
#define CAUSE_INVALID_CONTENTS		100
#define CAUSE_NOTCOMPAT_STATE		101
#define CAUSE_TIMER_EXPIRED		102
#define CAUSE_PROTOCOL_ERROR		111

#define NO_CAUSE			254

/*
 * Restart indication class values
 */
#define RESTART_CLASS_CHANNEL	0
#define RESTART_CLASS_SINGLE	6
#define RESTART_CLASS_ALL	7

/*
 * Parser error codes
 */
#define	Q931_ERROR_LEN		0x010000
#define Q931_ERROR_OVERFLOW	0x020000
#define Q931_ERROR_CREF		0x040000
#define Q931_ERROR_FATAL	0x0F0000
#define Q931_ERROR_IELEN	0x100000
#define Q931_ERROR_UNKNOWN	0x200000
#define Q931_ERROR_COMPREH	0x400000
#define Q931_ERROR_IESEQ	0x800000	/* do not care in our implementation */

/* Bearer capability */
#define Q931_CAP_SPEECH		0x00
#define Q931_CAP_UNRES_DIGITAL	0x08
#define Q931_CAP_RES_DIGITAL	0x09
#define Q931_CAP_3KHZ_AUDIO	0x10
#define Q931_CAP_7KHZ_AUDIO	0x11
#define Q931_CAP_VIDEO		0x18

/* Bearer L1 user info */
#define Q931_L1INFO_V110	0x01
#define Q931_L1INFO_ULAW	0x02
#define Q931_L1INFO_ALAW	0x03
#define Q931_L1INFO_G721	0x04
#define Q931_L1INFO_G722_5	0x05
#define Q931_L1INFO_G7XX_VIDEO	0x06
#define Q931_L1INFO_NONE_CCITT	0x07
#define Q931_L1INFO_V120	0x08
#define Q931_L1INFO_X31		0x09

struct misdn_channel_info {
	unsigned char	nr;	/* channel number/slot or special */
	unsigned char	flags;	/* exclusiv, not Basic, ANY, NONE */
	unsigned char	type;   /* B-channel, D-channel, H0, H11, H12 */
	unsigned char	ctrl;	/* Allocated, updated, needsend, sent */
} __attribute__((packed));

/*
 * special channel number defines
 */
#define MI_CHAN_ANY		0xff
#define MI_CHAN_NONE		0xfe
#define MI_CHAN_DCHANNEL	0xfd

#define MI_CHAN_FLG_NONE	0x01
#define MI_CHAN_FLG_ANY		0x02
#define MI_CHAN_FLG_DCHANNEL	0x04
#define MI_CHAN_FLG_EXCLUSIVE	0x08
#define MI_CHAN_FLG_OTHER_IF	0x20

#define MI_CHAN_TYP_NONE	0
#define MI_CHAN_TYP_B		1
#define MI_CHAN_TYP_D		2
#define MI_CHAN_TYP_H0		3
#define MI_CHAN_TYP_H11		4
#define MI_CHAN_TYP_H12		5

#define MI_CHAN_CTRL_UPDATED	0x01
#define MI_CHAN_CTRL_NEEDSEND	0x02
#define MI_CHAN_CTRL_SENT	0x04
#define MI_CHAN_CTRL_ALLOCATED	0x10
#define MI_CHAN_CTRL_DOWN	0x20

/* progress info */
struct misdn_progress_info {
	unsigned char	loc;	/* location, octet 3 */
	unsigned char	desc;	/* description, octet 3 */
	unsigned char	resv;	/* reserved */
	unsigned char	ctrl;	/* ctrl info flags */
} __attribute__((packed));

/*
 * Q931 location
 */
#define Q931_LOC_USER		0
#define Q931_LOC_PRVN_LOCUSER	1
#define Q931_LOC_PUBN_LOCUSER	2
#define Q931_LOC_PUBN_RMTUSER	4
#define Q931_LOC_PRVN_RMTUSER	5
#define Q931_LOC_INTERNATIONAL	7

/*
 * Progress values
 */
#define PROGRESS_NOT_E2E_ISDN	1
#define PROGRESS_DEST_NOT_ISDN	2
#define PROGRESS_ORIG_NOT_ISDN	3
#define PROGRESS_RET_TO_ISDN	4
#define PROGRESS_INBAND		8

/* Progress control flags */
#define MI_PROG_CTRL_UPDATED	0x01
#define MI_PROG_CTRL_NEEDSEND	0x02
#define MI_PROG_CTRL_SENT	0x04

/* Reason for diversion */
#define MI_DIV_REASON_UNKNOWN	0x00
#define MI_DIV_REASON_CFB	0x01
#define MI_DIV_REASON_CFNR	0x02
#define MI_DIV_REASON_CD	0x0a
#define MI_DIV_REASON_CFU	0x0f

/* Number qualifiers */
#define Q931_NTYPE_UNKNOWN		0
#define Q931_NTYPE_INTERNATIONAL	1
#define Q931_NTYPE_NATIONAL		2
#define Q931_NTYPE_NETWORKSPECIFIC	3
#define Q931_NTYPE_SUBSCRIBER		4
#define Q931_NTYPE_ABBREVIATED		6
#define Q931_NTYPE_RESERVED		7

#define Q931_NPLAN_UNKNOWN		0x0
#define Q931_NPLAN_ISDN			0x1
#define Q931_NPLAN_DATA			0x3
#define Q931_NPLAN_TELEX		0x4
#define Q931_NPLAN_NATIONAL		0x8
#define Q931_NPLAN_PRIVATE		0x9
#define Q931_NPLAN_RESERVED		0xf

#define Q931_NPRESENTATION_ALLOWED	0
#define Q931_NPRESENTATION_RESTRICTED	1
#define Q931_NPRESENTATION_NOTAVAILABLE	2
#define Q931_NPRESENTATION_RESERVED	3

#define Q931_NSCREEN_USER_NOT		0
#define Q931_NSCREEN_USER_PASSED	1
#define Q931_NSCREEN_USER_FAILED	2
#define Q931_NSCREEN_NETWORK		3

/* Common IE encode helpers */
struct l3_msg;
struct asn1_parm;

extern int l3_ie2pos(unsigned char);

extern int mi_encode_bearer(struct l3_msg *, unsigned int, unsigned int, unsigned int, unsigned int);
extern int mi_encode_hlc(struct l3_msg *, int, int);
extern int mi_encode_channel_id(struct l3_msg *, struct misdn_channel_info *);
extern int mi_encode_calling_nr(struct l3_msg *, char *, int, unsigned int, unsigned int, unsigned int);
extern int mi_encode_connected_nr(struct l3_msg *, char *, int, unsigned int, unsigned int, unsigned int);
extern int mi_encode_called_nr(struct l3_msg *, char *, unsigned int, unsigned int);
extern int mi_encode_redirecting_nr(struct l3_msg *, char *, int, unsigned int, unsigned int, int);
extern int mi_encode_redirection_nr(struct l3_msg *, char *, int, unsigned int, unsigned int);
extern int mi_encode_useruser(struct l3_msg *, int, int, char *);
extern int mi_encode_cause(struct l3_msg *l, int cause, int, int, unsigned char *);
extern int mi_encode_progress(struct l3_msg *, struct misdn_progress_info *);
extern int mi_encode_date(struct l3_msg *, struct tm *);
extern int mi_encode_restart_ind(struct l3_msg *, unsigned char);
extern int mi_encode_facility(struct l3_msg *, struct asn1_parm *);
extern int mi_encode_notification_ind(struct l3_msg *, int);

/* Common IE decode helpers */
struct mbuffer;
extern int parseQ931(struct mbuffer *);
extern int l3_ie2pos(u_char);
extern unsigned char l3_pos2ie(int);

extern int mi_decode_progress(struct l3_msg *, struct misdn_progress_info *);
extern int mi_decode_bearer_capability(struct l3_msg *, int *, int *, int *, int *, int *,
	int *, int *, int *, int *, int *, int *, int *, int *);
extern int mi_decode_hlc(struct l3_msg *l3m, int *, int *);
extern int mi_decode_cause(struct l3_msg *, int *, int *, int *, int *, int *, unsigned char *);
extern int mi_decode_channel_id(struct l3_msg *, struct misdn_channel_info *);
extern int mi_decode_calling_nr(struct l3_msg *, int *, int *, int *, int *, char *);
extern int mi_decode_connected_nr(struct l3_msg *, int *, int *, int *, int *, char *);
extern int mi_decode_called_nr(struct l3_msg *, int *, int *, char *);
extern int mi_decode_redirecting_nr(struct l3_msg *, int *, int *, int *, int *, int *, char *);
extern int mi_decode_redirection_nr(struct l3_msg *, int *, int *, int *, char *);
extern int mi_decode_display(struct l3_msg *, char *, int);
extern int mi_decode_useruser(struct l3_msg *, int *, int *, char *, int);
extern int mi_decode_date(struct l3_msg *, struct tm *);
extern int mi_decode_restart_ind(struct l3_msg *, unsigned char *);
extern int mi_decode_facility(struct l3_msg *, struct asn1_parm *);
extern int mi_decode_notification_ind(struct l3_msg *, int*);

/* some print helpers */
extern const char *mi_bearer2str(int);
extern const char *mi_msg_type2str(unsigned int);
extern const char *_mi_msg_type2str(unsigned int);
#endif
