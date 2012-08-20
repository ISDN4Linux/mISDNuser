/* q931.c
 *
 * Basic functions for all Q931 based protocols
 *
 * Author       Karsten Keil <kkeil@novell.com>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
 * Copyright 2010  by Karsten Keil <kkeil@linux-pingi.de>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mISDN/mbuffer.h>
#include <mISDN/q931.h>
#include <mISDN/suppserv.h>
#include "layer3.h"
#include "dss1.h"

static signed char __l3pos[128] = {
			-1,-2,-2,-2, 0,-2,-2,-2, 1,-2,-2,-2,-2,-2,-2,-2,
			 2,-1,-1,-1, 3,-1,-1,-1, 4,-1,-1,-1, 5,-1, 6,-1,
			 7,-1,-1,-1,-1,-1,-1, 8, 9,10,-1,-1,11,-1,-1,-1,
			-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			13,-1,14,15,16,17,18,19,-1,-1,20,-1,21,22,-1,-1,
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,23,24,-1,-1,
			25,26,-1,-1,27,-1,28,-1,29,30,-1,-1,31,32,33,-1
};

static unsigned char __l3ie[IE_COUNT] = {
			0x04, 0x08, 0x10, 0x14, 0x18, 0x1c, 0x1e, 0x20,
			0x27, 0x28, 0x29, 0x2c, 0x34, 0x40, 0x42, 0x43,
			0x44, 0x45, 0x46, 0x47, 0x4a, 0x4c, 0x4d, 0x6c,
			0x6d, 0x70, 0x71, 0x74, 0x76, 0x78, 0x79, 0x7c,
			0x7d, 0x7e
};

int
l3_ie2pos(u_char ie)
{
	if (ie > 127) /* only variable IE */
		return -3;
	return __l3pos[ie];
}

unsigned char
l3_pos2ie(int pos)
{
	if (pos < 0 || pos > IE_COUNT)
		return 0;
	return __l3ie[pos];
}

static int
__get_free_extra(struct l3_msg *m)
{
	int	i;

	for (i = 0; i < 8; i++)
		if (!m->extra[i].val)
			return i;
	fprintf(stderr, "%s: internal overflow\n", __FUNCTION__);
	return -1;
}

int
parseQ931(struct mbuffer *mb) {
	int		codeset, maincodeset;
	int		iep, err = 0, eidx = -1;
	unsigned char	*p, ie, **v_ie = &mb->l3.bearer_capability;

	__msg_pull(mb, 1);
	mb->l3h.crlen = *__msg_pull(mb, 1);
	if (mb->l3h.crlen > 2)
		return Q931_ERROR_CREF;
	if (mb->l3h.crlen)
		mb->l3h.cr = *__msg_pull(mb, 1);
	if (mb->l3h.crlen == 2) {
		mb->l3h.cr <<= 8;
		mb->l3h.cr |= *__msg_pull(mb, 1);
	} else if (mb->l3h.crlen == 1)
		if (mb->l3h.cr & 0x80) {
			mb->l3h.cr |= MISDN_PID_CR_FLAG;
			mb->l3h.cr &= 0x807F;
		}
	mb->l3.pid = mb->addr.channel << 16;
	if (mb->l3h.crlen == 0)
		mb->l3.pid |= MISDN_PID_DUMMY;
	else if ((mb->l3h.cr & 0x7fff) == 0)
		mb->l3.pid |= MISDN_PID_GLOBAL;
	else
		mb->l3.pid |= mb->l3h.cr;
	if (mb->len < 1)
		return Q931_ERROR_LEN;

	mb->l3h.type = *__msg_pull(mb, 1);
	mb->l3.type = mb->l3h.type;
	codeset = maincodeset = 0;
	while (mb->len) {
		p = __msg_pull(mb, 1);
		ie = *p;
		if ((ie & 0xf0) == 0x90) {
			codeset = ie & 0x07;
			if (!(ie & 0x08))
				maincodeset = codeset;
			if (eidx >= 0) {
				mb->l3.extra[eidx].len = mb->data - mb->l3.extra[eidx].val -1;
				eidx = -1;
			}
			if (codeset != 0) {
				eidx = __get_free_extra(&mb->l3);
				if (eidx < 0)
					return Q931_ERROR_OVERFLOW;
				mb->l3.extra[eidx].ie = ie;
				mb->l3.extra[eidx].codeset = codeset;
				mb->l3.extra[eidx].val = mb->data;
			}
			continue;
		}
		if (codeset == 0) {
			if (ie & 0x80) { /* single octett IE */
				if (ie == IE_MORE_DATA)
					mb->l3.more_data++;
				else if (ie == IE_COMPLETE) {
					mb->l3.sending_complete++;
				}
				else if ((ie & 0xf0) == IE_CONGESTION)
					mb->l3.congestion_level = ie;
				else {
					err |= Q931_ERROR_UNKNOWN;
				}
			} else {
				iep = __l3pos[ie];
				if (mb->len < 1)
					return Q931_ERROR_LEN;
				p = __msg_pull(mb, 1);
				if (mb->len < *p)
					return Q931_ERROR_LEN;
				if (iep >= 0) {
					if (!v_ie[iep]) { /* IE not detected before */
						v_ie[iep] = p;
					} else { /* IE is repeated */
						eidx = __get_free_extra(&mb->l3);
						if (eidx < 0)
							return Q931_ERROR_OVERFLOW;
						mb->l3.extra[eidx].ie = ie;
						mb->l3.extra[eidx].val = p;
						eidx = -1;
					}
				} else {
					if (iep == -2) {
						err |= Q931_ERROR_COMPREH;
						mb->l3.comprehension_req = ie;
					}
					err |= Q931_ERROR_UNKNOWN;
				}
				__msg_pull(mb, *p);
			}
		} else { /* codeset != 0 */
			if (!(ie & 0x80)) { /* not single octett IE */
				if (mb->len < 1)
					return Q931_ERROR_LEN;
				p = __msg_pull(mb, 1);
				if (mb->len < *p)
					return Q931_ERROR_LEN;
				__msg_pull(mb, *p);
			}
			if (codeset != maincodeset) { /* not locked shift */
				mb->l3.extra[eidx].len = mb->data - mb->l3.extra[eidx].val;
				eidx = -1;
			}
		}
		codeset = maincodeset;
	}
	if (eidx >= 0)
		mb->l3.extra[eidx].len = mb->data - mb->l3.extra[eidx].val;
	return(err);
}

static int
__get_next_extra(struct l3_msg *m, int i, unsigned char ie)
{
	while(++i < 8) {
		if (m->extra[i].codeset)
			continue;
		if (!m->extra[i].val)
			break; /* assume that extra is filled in order without holes */
		if (m->extra[i].ie == ie)
			return i;
	}
	return -1;
}

int
assembleQ931(l3_process_t *pc, struct l3_msg *l3m)
{
	struct mbuffer	*mb = container_of(l3m, struct mbuffer, l3);
	unsigned char	ie, **v_ie = &l3m->bearer_capability;
	int		i, l, eidx = -1;

	mb->data = mb->tail = mb->head;
	mb->len = 0;
	msg_reserve(mb, MISDN_HEADER_LEN);
	if (pc->pid == MISDN_PID_DUMMY) {
		mb->l3h.crlen = 0;
	} else {
		if (test_bit(FLG_BASICRATE, &pc->L3->ml3.options))
			mb->l3h.crlen = 1;
		else
			mb->l3h.crlen = 2;
		mb->l3h.cr = pc->pid & 0xffff;
	}
	mb->l3h.type = l3m->type & 0xff;
	*msg_put(mb, 1) = Q931_PD; /* TODO: be flexible for national */
	*msg_put(mb, 1) = mb->l3h.crlen;
	if (mb->l3h.crlen == 2) {
		*msg_put(mb, 1) = 0x80 ^ ((mb->l3h.cr >> 8) & 0xff);
		*msg_put(mb, 1) = mb->l3h.cr & 0xff;
	} else if (mb->l3h.crlen == 1) {
		if (mb->l3h.cr & MISDN_PID_CR_FLAG)
			*msg_put(mb, 1) = mb->l3h.cr & 0x7f;
		else
			*msg_put(mb, 1) = 0x80 | (mb->l3h.cr & 0x7f);
	}
	*msg_put(mb, 1) = mb->l3h.type;

	/* single octett IE */
	if (l3m->more_data)
		*msg_put(mb, 1) = IE_MORE_DATA;
	if (l3m->sending_complete)
		*msg_put(mb, 1) = IE_COMPLETE;
	if (l3m->congestion_level)
		*msg_put(mb, 1) = l3m->congestion_level;

	for (i=0; i< IE_COUNT; i++) {
		if (v_ie[i]) {
			ie = __l3ie[i];
			*msg_put(mb, 1) = ie;
			l = *v_ie[i] + 1;
			memcpy(msg_put(mb, l), v_ie[i], l);
			while (0 <= (eidx = __get_next_extra(l3m, eidx, ie))) {
				*msg_put(mb, 1) = l3m->extra[eidx].ie;
				l = *l3m->extra[eidx].val + 1;
				memcpy(msg_put(mb, l), l3m->extra[eidx].val, l);
			}
		}
	}
	for (i=0; i<8; i++) {
		/* handle other codeset elements */
		if (l3m->extra[i].codeset) {
			ie = 0x98 | l3m->extra[i].codeset;
			*msg_put(mb, 1) = ie; /* shift codeset IE */
			memcpy(msg_put(mb, l3m->extra[i].len), l3m->extra[i].val, l3m->extra[i].len);
		}
	}
	return 0;
}

int
add_layer3_ie(struct l3_msg *l3m, unsigned char ie, int len, unsigned char *data)
{
	struct mbuffer	*mb = container_of(l3m, struct mbuffer, l3);
	int i;
	unsigned char **v_ie;

	if (ie & 0x80) { /* single octett IE */
		if (ie == IE_MORE_DATA)
			l3m->more_data++;
		else if (ie == IE_COMPLETE)
			l3m->sending_complete++;
		else if ((ie & 0xf0) == IE_CONGESTION)
			l3m->congestion_level = ie;
		else
			return Q931_ERROR_UNKNOWN;
	} else { /*variable lenght IE */
		v_ie = &l3m->bearer_capability;
		i = __l3pos[ie];
		if (i < 0)
			return Q931_ERROR_UNKNOWN;
		if (len > 255)
			return Q931_ERROR_IELEN;
		if (len + 1 + mb->ctail > mb->cend)
			return Q931_ERROR_OVERFLOW;
		*mb->ctail = len & 0xFF;
		memcpy(mb->ctail + 1, data, len);
		if (!v_ie[i])
			v_ie[i] = mb->ctail;
		else {
			i = __get_free_extra(l3m);
			if (i < 0)
				return Q931_ERROR_OVERFLOW;
			l3m->extra[i].ie = ie;
			l3m->extra[i].val = mb->ctail;
		}
		mb->ctail += len + 1;
	}
	return 0;
}

/* helper functions to encode common Q931 info elements */

int
mi_encode_bearer(struct l3_msg *l3m, unsigned int capability, unsigned int l1user, unsigned int mode, unsigned int rate)
{
	unsigned char ie[8];
	int l = 2, multi = -1, user;

	user = l1user;

	switch (capability) {
	case Q931_CAP_UNRES_DIGITAL:
		user = -1;
		break;
	case Q931_CAP_RES_DIGITAL:
		user = -1;
		break;
	default:
		rate = 0x10;
		mode = 0;
		break;
	}

	ie[0] = 0x80 |  capability;
	ie[1] = 0x80 | (mode << 5) | rate;
	if (multi >= 0) {
		ie[2] = 0x80 | multi;
		l++;
	}
	if (user >= 0) {
		ie[l] = 0xa0 | user;
		l++;
	}

	return add_layer3_ie(l3m, IE_BEARER, l, ie);
}

int
mi_encode_hlc(struct l3_msg *l3m, int hlc, int ehlc)
{
	unsigned char ie[3];
	int l = 2;

	if (hlc < 0) /* no hlc to include */
		return 0;

	ie[0] = 0x91;
	ie[1] = hlc & 0x7f;
	
	if (ehlc < 0)
		ie[1] | 0x80;
	else {
		l = 3;
		ie[2] = 0x80 | (ehlc & 0x7f);
	}
	return add_layer3_ie(l3m, IE_HLC, l, ie);
}

int
mi_encode_channel_id(struct l3_msg *l3m, struct misdn_channel_info *ci)
{
	unsigned char ie[3];
	int ret, l, excl = 1;

	if (ci->ctrl & MI_CHAN_CTRL_SENT) /* already sent */
		return 0;

	if (!(ci->ctrl & MI_CHAN_CTRL_NEEDSEND)) /* we do not need send the ie */
		return 0;

	if (ci->nr == MI_CHAN_ANY || ci->nr == MI_CHAN_NONE ||
		(ci->flags & MI_CHAN_FLG_NONE) || (ci->flags & MI_CHAN_FLG_ANY) ||
		(!(ci->flags & MI_CHAN_FLG_EXCLUSIVE)))
		excl = 0;

	if (ci->flags & MI_CHAN_FLG_OTHER_IF) { /* PRI */
		if (ci->nr == MI_CHAN_NONE)
			return 0; /* IE is not included */
		else if (ci->nr == MI_CHAN_ANY || (ci->flags & MI_CHAN_FLG_ANY)) {
			l = 1;
			ie[0] = 0x80 | 0x20 | 0x03;
		} else {
			l = 3;
			ie[0] = 0x80 | 0x20 | (excl << 3) | 1;
			if (ci->type & MI_CHAN_TYP_H0)
				ie[1] = 0x86;
			else if (ci->type & MI_CHAN_TYP_H11)
				ie[1] = 0x88;
			else if (ci->type & MI_CHAN_TYP_H12)
				ie[1] = 0x89;
			else /* default B-channel */
				ie[1] = 0x83;
			ie[2] = 0x80 | ci->nr;
		}
	} else { /* BRI */
		l = 1;
		if (ci->nr == MI_CHAN_DCHANNEL || ci->type == MI_CHAN_TYP_D || (ci->flags & MI_CHAN_FLG_DCHANNEL))
			ie[0] = MI_CHAN_FLG_DCHANNEL;
		else if (ci->nr == MI_CHAN_ANY || (ci->flags & MI_CHAN_FLG_ANY))
			ie[0] = 3;
		else if (ci->nr == MI_CHAN_NONE || (ci->flags & MI_CHAN_FLG_NONE))
			ie[0] = 0;
		else
			ie[0] = ci->nr & 3;
		ie[0] |= 0x80 | (excl << 3);
	}
	ret = add_layer3_ie(l3m, IE_CHANNEL_ID, l, ie);
	if (!ret) {
		ci->ctrl |= MI_CHAN_CTRL_SENT;
		ci->ctrl &= ~MI_CHAN_CTRL_SENT;
	}
	return ret;
}

int
mi_encode_calling_nr(struct l3_msg *l3m, char *nr, int pres, unsigned int screen, unsigned int type, unsigned int plan)
{
	unsigned char ie[32];
	int l;

	if (pres < 0 && (nr == NULL || *nr == 0)) /* defaults, no number provided */
		return 0;
	if (nr && strlen(nr) > 30)
		return -EINVAL;

	if (pres >= 0) {
		l = 2;
		ie[0] = (type << 4) | plan;
		ie[1] = 0x80 | (pres << 5) | screen;
	} else {
		l = 1;
		ie[0] = 0x80 | (type << 4) | plan;
	}
	if (nr && *nr) {
		strncpy((char *)&ie[l], nr, 30);
		l += strlen(nr);
	}
	return add_layer3_ie(l3m, IE_CALLING_PN, l, ie);
}

int
mi_encode_connected_nr(struct l3_msg *l3m, char *nr, int pres, unsigned int screen, unsigned int type, unsigned int plan)
{
	unsigned char ie[32];
	int l;

	if (pres < 0 && (nr == NULL || *nr == 0)) /* defaults, no number provided */
		return 0;
	if (nr && strlen(nr) > 30)
		return -EINVAL;

	if (pres >= 0) {
		l = 2;
		ie[0] = (type << 4) | plan;
		ie[1] = 0x80 | (pres << 5) | screen;
	} else {
		l = 1;
		ie[0] = 0x80 | (type << 4) | plan;
	}
	if (pres > 0) /* restricted or no available, no number should be sent */
		nr = NULL;
	if (nr && *nr) {
		strncpy((char *)&ie[l], nr, 30);
		l += strlen(nr);
	}
	return add_layer3_ie(l3m, IE_CONNECT_PN, l, ie);
}

int
mi_encode_called_nr(struct l3_msg *l3m, char *nr, unsigned int type, unsigned int plan)
{
	unsigned char ie[32];
	int l;

	if (nr == NULL || *nr == 0) /* not provided */
		return 0;
	if (nr && strlen(nr) > 30)
		return -EINVAL;

	l = 1;
	ie[0] = 0x80 | (type << 4) | plan;
	if (nr && *nr) {
		strncpy((char *)&ie[l], nr, 30);
		l += strlen(nr);
	}
	return add_layer3_ie(l3m, IE_CALLED_PN, l, ie);
}


int
mi_encode_redirecting_nr(struct l3_msg *l3m, char *nr, int pres, unsigned int type, unsigned int plan, int reason)
{
	unsigned char ie[24];
	int l;

	if (nr == NULL || *nr == 0) /* not provided */
		return 0;
	if (nr && strlen(nr) > 20)
		return -EINVAL;

	if (pres >= 0) {
		l = 2;
		ie[0] = (type << 4) | plan;
		ie[1] = (pres << 5);
		if (reason >= 0) {
			l++;
			ie[2] = 0x80 | reason;
		} else
			ie[1] |= 0x80;
	} else {
		l = 1;
		ie[0] = 0x80 | (type << 4) | plan;
	}
	if (nr && *nr) {
		strncpy((char *)&ie[l], nr, 30);
		l += strlen(nr);
	}
	return add_layer3_ie(l3m, IE_REDIRECTING_NR, l, ie);
}

int
mi_encode_redirection_nr(struct l3_msg *l3m, char *nr, int pres, unsigned int type, unsigned int plan)
{
	unsigned char ie[24];
	int l;

	if (nr == NULL || *nr == 0) /* not provided */
		return 0;
	if (nr && strlen(nr) > 20)
		return -EINVAL;

	if (pres >= 0) {
		l = 2;
		ie[0] = (type << 4) | plan;
		ie[1] = 0x80 | (pres << 5);
	} else {
		l = 1;
		ie[0] = 0x80 | (type << 4) | plan;
	}
	if (nr && *nr) {
		strncpy((char *)&ie[l], nr, 30);
		l += strlen(nr);
	}
	return add_layer3_ie(l3m, IE_REDIRECTION_NR, l, ie);
}

int
mi_encode_useruser(struct l3_msg *l3m, int protocol, int len, char *data)
{
	unsigned char ie[256];

	if (len <= 0) /* not included */
		return 0;
	if (len > 250 || !data)
		return -EINVAL;
	if (protocol<0 || protocol>127)
		return -EINVAL;
	ie[0] = protocol & 0xff;
	memcpy(&ie[1], data, len);
	return add_layer3_ie(l3m, IE_USER_USER, len + 1, ie);
}

int
mi_encode_cause(struct l3_msg *l3m, int cause, int loc, int dlen, unsigned char *diag)
{
	unsigned char ie[32];
	int l;

	if (cause < 0 || cause == NO_CAUSE) /* not included */
		return 0;
	if (cause > 127)
		return -EINVAL;
	if (loc < 0 || loc > 7)
		return -EINVAL;
	if (dlen > 30)
		return -EINVAL;
	if (dlen && !diag)
		return -EINVAL;
	l = 2 + dlen;
	ie[0] = 0x80 | loc;
	ie[1] = 0x80 | cause;
	if (dlen)
		memcpy(&ie[2], diag, dlen);
	return add_layer3_ie(l3m, IE_CAUSE, l, ie);
}

int
mi_encode_progress(struct l3_msg *l3m, struct misdn_progress_info *prg)
{
	unsigned char ie[2];
	int ret;

	if (prg->ctrl & MI_PROG_CTRL_SENT)
		return 0;

	if (!(prg->ctrl & MI_PROG_CTRL_NEEDSEND))
		return 0;

	ie[0] = 0x80 | prg->loc;
	ie[1] = 0x80 | prg->desc;

	ret = add_layer3_ie(l3m, IE_PROGRESS, 2, ie);
	if (!ret) {
		prg->ctrl |= MI_PROG_CTRL_SENT;
		prg->ctrl &= ~MI_PROG_CTRL_SENT;
	}
	return ret;
}

int
mi_encode_date(struct l3_msg *l3m, struct tm *tm)
{
	unsigned char ie[5];

	ie[0] = tm->tm_year % 100;
	ie[1] = tm->tm_mon + 1;
	ie[2] = tm->tm_mday;
	ie[3] = tm->tm_hour;
	ie[4] = tm->tm_min;
	return add_layer3_ie(l3m, IE_DATE, 5, ie);
}

int
mi_encode_restart_ind(struct l3_msg *l3m, unsigned char _class)
{
	switch(_class) {
	case RESTART_CLASS_CHANNEL:
	case RESTART_CLASS_SINGLE:
	case RESTART_CLASS_ALL:
		break;
	default:
		return -EINVAL;
	}
	_class |= 0x80;
	return add_layer3_ie(l3m, IE_RESTART_IND, 1, &_class);
}

int
mi_encode_facility(struct l3_msg *l3m, struct asn1_parm *fac)
{
	struct mbuffer  *mb = container_of(l3m, struct mbuffer, l3);
	int i, len;

	len = encodeFac(mb->ctail, fac);
	if (len <= 0)
		return -EINVAL;
	if (mb->ctail + len >= mb->cend) {
		eprint("Msg buffer overflow %d needed %d available\n", len + 1, (int)(mb->cend - mb->ctail));
		return Q931_ERROR_OVERFLOW;
	}
	if (l3m->facility) {
		i = __get_free_extra(l3m);
		if (i < 0) {
			eprint("To many Facility IEs\n");
			return Q931_ERROR_OVERFLOW;
		}
		l3m->extra[i].ie = IE_FACILITY;
		l3m->extra[i].val = mb->ctail + 1;
	} else
		l3m->facility = mb->ctail + 1;
	mb->ctail += len + 1;
	return 0;
}

/* helper functions to decode common IE */

#define _ASSIGN_PVAL(p, v)	if (p) *p = (v)

int
mi_decode_progress(struct l3_msg *l3m, struct misdn_progress_info *progress)
{
	struct misdn_progress_info prg;

	if (l3m == NULL || !l3m->progress)
		return 0;

	if (l3m->progress[0] < 2)
		return -EINVAL;
	prg.loc = l3m->progress[1] &  0x7f;
	prg.desc = l3m->progress[2] & 0x7f;
	prg.resv = 0;
	prg.ctrl = MI_PROG_CTRL_UPDATED;
	_ASSIGN_PVAL(progress, prg);
	return 0;
}

int
mi_decode_bearer_capability(struct l3_msg *l3m, int *coding, int *capability, int *mode, int *rate,
	int *oct_4a, int *oct_4b, int *oct_5, int *oct_5a, int *oct_5b1,
	int *oct_5b2, int *oct_5c, int *oct_5d, int *oct_6, int *oct_7)
{
	int opt[10];
	int i,j;
	enum {
		octet_4a  = 0,
		octet_4b  = 1,
		octet_5   = 2,
		octet_5a  = 3,
		octet_5b1 = 4,
		octet_5b2 = 5,
		octet_5c  = 6,
		octet_5d  = 7,
		octet_6   = 8,
		octet_7   = 9
	};

	if (!l3m->bearer_capability || *l3m->bearer_capability < 2)
		return -EINVAL;
	_ASSIGN_PVAL(coding, (l3m->bearer_capability[1] & 0x60) >> 5);
	_ASSIGN_PVAL(capability, l3m->bearer_capability[1] &  0x1f);
	_ASSIGN_PVAL(mode, (l3m->bearer_capability[2] & 0x60) >> 5);
	_ASSIGN_PVAL(rate, l3m->bearer_capability[2] & 0x1f);

	/* Now the optional octets */
	for (j = 0; j < 10; j++)
		opt[j] = -1;
	i = 2;
	if (*l3m->bearer_capability <= i)
		goto done;
	if (!(l3m->bearer_capability[i] & 0x80)) {
		i++;
		j = octet_4a;
		opt[j] = l3m->bearer_capability[i];
		if (*l3m->bearer_capability <= i)
			goto done;
		j++;  /* octet4b */
		if (!(l3m->bearer_capability[i] & 0x80)) {
			i++;
			opt[j] = l3m->bearer_capability[i];
		}
	}
	i++;
	if (*l3m->bearer_capability < i)
		goto done;
	opt[octet_5] = l3m->bearer_capability[i];
	if (*l3m->bearer_capability <= i)
		goto done;
	j = octet_5a;
	while (j <= octet_5d  && *l3m->bearer_capability > i) {
		if (l3m->bearer_capability[i] & 0x80)
			break;
		i++;
		opt[j] = l3m->bearer_capability[i];
		j++;
	}
	i++;
	if (*l3m->bearer_capability < i)
		goto done;
	opt[octet_6] = l3m->bearer_capability[i];
	i++;
	if (*l3m->bearer_capability < i)
		goto done;
	opt[octet_7] = l3m->bearer_capability[i];
done:
	_ASSIGN_PVAL(oct_4a, opt[octet_4a]);
	_ASSIGN_PVAL(oct_4b, opt[octet_4b]);
	_ASSIGN_PVAL(oct_5, opt[octet_5]);
	_ASSIGN_PVAL(oct_5a, opt[octet_5a]);
	_ASSIGN_PVAL(oct_5b1, opt[octet_5b1]);
	_ASSIGN_PVAL(oct_5b2, opt[octet_5b2]);
	_ASSIGN_PVAL(oct_5c, opt[octet_5c]);
	_ASSIGN_PVAL(oct_5d, opt[octet_5d]);
	_ASSIGN_PVAL(oct_6, opt[octet_6]);
	_ASSIGN_PVAL(oct_7, opt[octet_7]);
	return 0;
};

int
mi_decode_hlc(struct l3_msg *l3m, int *hlchar, int *exthcl)
{
	int hlc = -1, exthlc = -1;

	if (l3m == NULL || l3m->hlc == NULL)
		goto done;
	if (*l3m->hlc < 2)
		return -EINVAL;
	if (l3m->hlc[1] != 0x91)
		return -EINVAL;
	hlc = l3m->hlc[2] & 0x7f;
	if (*l3m->hlc > 2 && !(l3m->hlc[2] & 0x80))
		exthlc = l3m->hlc[2] & 0x7f;
done:
	_ASSIGN_PVAL(hlchar, hlc);
	_ASSIGN_PVAL(exthcl, exthlc);
	return 0;
}

int
mi_decode_cause(struct l3_msg *l3m, int *coding, int *loc, int *rec, int *val, int *dialen, unsigned char *dia)
{
	int r = 0, l = 2, dl = 0;

	if (l3m == NULL || !l3m->cause)
		return 0;
	if (*l3m->cause < 2)
		return -EINVAL;
	_ASSIGN_PVAL(coding, (l3m->cause[1] & 0x60) >> 5);
	_ASSIGN_PVAL(loc, l3m->cause[1] & 0x0f);
	if (!(l3m->cause[1] & 0x80)) {
		r = l3m->cause[2] & 0x7f;
		l++;
	} else
		r = 0;
	_ASSIGN_PVAL(rec, r);
	_ASSIGN_PVAL(val, l3m->cause[l] & 0x7f);
	dl = *l3m->cause - l;
	if (dl > 0 && dl < 30 && dia)
		memcpy(dia, &l3m->cause[l + 1], dl);
	else
		dl = 0;
	_ASSIGN_PVAL(dialen, dl);
	return 0;
}

int
mi_decode_channel_id(struct l3_msg *l3m, struct misdn_channel_info *cip)
{
	struct misdn_channel_info c = {MI_CHAN_NONE, 0, 0, 0};

	if (l3m == NULL || !l3m->channel_id)
		return 0;
	if (*l3m->channel_id == 0)
		return 0;
	if (!cip)
		return 0;
	c.ctrl = cip->ctrl & (MI_CHAN_CTRL_ALLOCATED | MI_CHAN_CTRL_NEEDSEND | MI_CHAN_CTRL_SENT);
	/* We ignore explicit interface settings */
	c.flags = l3m->channel_id[1] & 0x2c;

	switch (l3m->channel_id[1] & 3) {
	case 0:
		c.nr = MI_CHAN_NONE;
		c.flags |= MI_CHAN_FLG_NONE;
		break;
	case 3:
		c.nr = MI_CHAN_ANY;
		c.flags |= MI_CHAN_FLG_ANY;
		break;
	default:
		c.nr = l3m->channel_id[1] & 3;
	}
	if ((c.flags & MI_CHAN_FLG_OTHER_IF) == 0) { /* BRI */
		if (c.flags & MI_CHAN_FLG_DCHANNEL) {
			c.nr = MI_CHAN_DCHANNEL;
			c.flags &= 0xfc; /* clear NONE,ANY */
			c.type = MI_CHAN_TYP_D;
		} else
			c.type = MI_CHAN_TYP_B;
	} else { /* PRI */
		if (*l3m->channel_id < 2)
			return -EINVAL;
		/* We ignore coding so CCITT/ETSI is assumed, no support for multiple channels or channelmaps */
		switch (l3m->channel_id[2] & 0x7f) {
			default:
			case 3:
				c.type = MI_CHAN_TYP_B;
				break;
			case 6:
				c.type = MI_CHAN_TYP_H0;
				break;
			case 8:
				c.type = MI_CHAN_TYP_H11;
				break;
			case 9:
				c.type = MI_CHAN_TYP_H12;
				break;
		}
		if (*l3m->channel_id > 2 && ((c.flags & (MI_CHAN_FLG_ANY | MI_CHAN_FLG_NONE)) == 0))
			c.nr = l3m->channel_id[3] & 0x7f;
	}
	if (cip->nr != c.nr || cip->type != c.type || cip->flags != c.flags)
		c.ctrl |= MI_CHAN_CTRL_UPDATED;
	_ASSIGN_PVAL(cip, c);
	return 0;
}

int
mi_decode_calling_nr(struct l3_msg *l3m, int *type, int *plan, int *pres, int *screen, char *nr)
{
	int _pres = 0, _screen = 0, i = 2, l;

	if (l3m == NULL || !l3m->calling_nr)
		return 0;
	if (*l3m->calling_nr < 2)
		return -EINVAL;
	if (*l3m->calling_nr > 32)
		return -EINVAL;
	_ASSIGN_PVAL(type, (l3m->calling_nr[1] & 0x70) >> 4);
	_ASSIGN_PVAL(plan, l3m->calling_nr[1] & 0x0f);
	if ((l3m->calling_nr[1] & 0x80) == 0 && *l3m->calling_nr >= 2) {
		_pres = (l3m->calling_nr[2] & 0x60) >> 5;
		_screen = l3m->calling_nr[2] & 0x03;
		i++;
	}
	l = *l3m->calling_nr + 1 - i;
	if (nr) {
		memcpy(nr, &l3m->calling_nr[i], l);
		nr[l] = 0;
	}
	_ASSIGN_PVAL(pres, _pres);
	_ASSIGN_PVAL(screen, _screen);
	return 0;
}

int
mi_decode_connected_nr(struct l3_msg *l3m, int *type, int *plan, int *pres, int *screen, char *nr)
{
	int _pres = 0, _screen = 0, i = 2, l;

	if (l3m == NULL || !l3m->connected_nr)
		return 0;
	if (*l3m->connected_nr < 2)
		return -EINVAL;
	if (*l3m->connected_nr > 32)
		return -EINVAL;
	_ASSIGN_PVAL(type, (l3m->connected_nr[1] & 0x70) >> 4);
	_ASSIGN_PVAL(plan, l3m->connected_nr[1] & 0x0f);
	if ((l3m->connected_nr[1] & 0x80) == 0 && *l3m->connected_nr >= 2) {
		_pres = (l3m->connected_nr[2] & 0x60) >> 5;
		_screen = l3m->connected_nr[2] & 0x03;
		i++;
	}
	l = *l3m->connected_nr + 1 - i;
	if (nr) {
		memcpy(nr, &l3m->connected_nr[i], l);
		nr[l] = 0;
	}
	_ASSIGN_PVAL(pres, _pres);
	_ASSIGN_PVAL(screen, _screen);
	return 0;
}

int
mi_decode_called_nr(struct l3_msg *l3m, int *type, int *plan, char *nr)
{
	int l;
	if (l3m == NULL || !l3m->called_nr)
		return 0;
	if (*l3m->called_nr < 1)
		return -EINVAL;
	if (*l3m->called_nr > 32)
		return -EINVAL;
	_ASSIGN_PVAL(type, (l3m->called_nr[1] & 0x70) >> 4);
	_ASSIGN_PVAL(plan, l3m->called_nr[1] & 0x0f);
	l = *l3m->called_nr - 1;
	if (nr) {
		memcpy(nr, &l3m->called_nr[2], l);
		nr[l] = 0;
	}
	return 0;
}

int
mi_decode_redirecting_nr(struct l3_msg *l3m, int *count, int *type, int *plan, int *pres, int *reason, char *nr)
{
	int _pres = 0, _count = 0, _reason = 0, i, l;
	unsigned char *rdnr;

	_ASSIGN_PVAL(count, _count);
	if (l3m == NULL || !l3m->redirecting_nr)
		return 0;
	rdnr = l3m->redirecting_nr;
	_count++;
	/* IE could be repeated, use the last one and calculate the number of redirects */
	for (i = 0; i < 8; i++) {
		if (!l3m->extra[i].val)
			break;
		if (l3m->extra[i].ie == IE_REDIRECTING_NR) {
			_count++;
			rdnr = l3m->extra[i].val;
		}
	}
	_ASSIGN_PVAL(count, _count);
	if (*rdnr < 2)
		return -EINVAL;
	if (*rdnr > 23)
		return -EINVAL;
	i = 2;
	_ASSIGN_PVAL(type, (rdnr[1] & 0x70) >> 4);
	_ASSIGN_PVAL(plan, rdnr[1] & 0x0f);
	if ((rdnr[1] & 0x80) == 0 && *rdnr >= 2) {
		_pres = (rdnr[2] & 0x60) >> 5;
		i++;
		if ((rdnr[2] & 0x80) == 0 && *rdnr >= 3) {
			_reason = rdnr[3] & 0x0f;
			i++;
		}
	}
	l = *rdnr + 1 - i;
	if (nr) {
		memcpy(nr, &rdnr[i], l);
		nr[l] = 0;
	}
	_ASSIGN_PVAL(pres, _pres);
	_ASSIGN_PVAL(reason, _reason);
	return 0;
}

int
mi_decode_redirection_nr(struct l3_msg *l3m, int *type, int *plan, int *pres, char *nr)
{
	int _pres = 0, i = 2, l;

	if (l3m == NULL || !l3m->redirection_nr)
		return 0;
	if (*l3m->redirection_nr < 2)
		return -EINVAL;
	if (*l3m->redirection_nr > 23)
		return -EINVAL;
	_ASSIGN_PVAL(type, (l3m->redirection_nr[1] & 0x70) >> 4);
	_ASSIGN_PVAL(plan, l3m->redirection_nr[1] & 0x0f);
	if ((l3m->redirection_nr[1] & 0x80) == 0 && *l3m->redirection_nr >= 2) {
		_pres = (l3m->redirection_nr[2] & 0x60) >> 5;
		i++;
	}
	l = *l3m->redirection_nr + 1 - i;
	if (nr) {
		memcpy(nr, &l3m->redirection_nr[i], l);
		nr[l] = 0;
	}
	_ASSIGN_PVAL(pres, _pres);
	return 0;
}

int
mi_decode_display(struct l3_msg *l3m, char *display, int maxlen)
{
	if (l3m == NULL || !l3m->display)
		return 0;
	if (!display)
		return 0;
	maxlen--;
	if (*l3m->display < maxlen)
		maxlen = *l3m->display;
	memcpy(display, &l3m->display[1], maxlen);
	display[maxlen] = 0;
	return 0;
}

int
mi_decode_useruser(struct l3_msg *l3m, int *pd, int *uulen, char *uu, int maxlen)
{
	int l;
	if (l3m == NULL || !l3m->useruser)
		return 0;
	if (!uu)
		return 0;
	if (*l3m->useruser < 1)
		return 0;
	if (*l3m->useruser < maxlen)
		l = *l3m->useruser - 1;
	else
		l = maxlen;
	if (l > 0)
		memcpy(uu, &l3m->useruser[2], l);
	_ASSIGN_PVAL(uulen, l);
	_ASSIGN_PVAL(pd, l3m->useruser[1]);
	return 0;
}

int
mi_decode_date(struct l3_msg *l3m, struct tm *dat)
{
	struct tm tm;

	if (!dat)
		return 0;

	if (l3m == NULL || !l3m->date)
		return 0;

	if (*l3m->date < 5)
		return 0;

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = l3m->date[1];
	if (tm.tm_year < 70)
		tm.tm_year += 100;
	tm.tm_mon = l3m->date[2] - 1;
	tm.tm_mday = l3m->date[3];
	tm.tm_hour = l3m->date[4];
	tm.tm_min = l3m->date[5];
	_ASSIGN_PVAL(dat, tm);
	return 0;
}

int
mi_decode_restart_ind(struct l3_msg *l3m, unsigned char *_class)
{
	if (l3m == NULL || !l3m->restart_ind)
		return 0;
	if (!_class)
		return 0;
	if (*l3m->restart_ind < 1)
		return 0;
	_ASSIGN_PVAL(_class, 0x7f & l3m->restart_ind[1]);
	return 0;
}

int
mi_decode_facility(struct l3_msg *l3m, struct asn1_parm *fac)
{
	if (l3m == NULL || !l3m->facility)
		return 0;
	if (!fac)
		return 0;
	return decodeFac(l3m->facility, fac);
}

const char *
mi_bearer2str(int cap)
{
	static char *bearers[] = {
		"Speech",
		"Audio 3.1 kHz",
		"Audio 7 kHz",
		"Unrestricted Digital",
		"Restricted Digital",
		"Video",
		"Unknown Bearer"
	};

	switch (cap) {
	case Q931_CAP_SPEECH:
		return bearers[0];
		break;
	case Q931_CAP_3KHZ_AUDIO:
		return bearers[1];
		break;
	case Q931_CAP_7KHZ_AUDIO:
		return bearers[2];
		break;
	case Q931_CAP_UNRES_DIGITAL:
		return bearers[3];
		break;
	case Q931_CAP_RES_DIGITAL:
		return bearers[4];
		break;
	case Q931_CAP_VIDEO:
		return bearers[5];
		break;
	default:
		return bearers[6];
		break;
	}
}

static const struct _cmdtab {
	unsigned int cmd;
	const char *name;
} cmdtab[] = {
	{MT_ALERTING		, "ALERTING"},
	{MT_CALL_PROCEEDING	, "CALL_PROCEEDING"},
	{MT_CONNECT		, "CONNECT"},
	{MT_CONNECT_ACKNOWLEDGE	, "CONNECT_ACKNOWLEDGE"},
	{MT_PROGRESS		, "PROGRESS"},
	{MT_SETUP		, "SETUP"},
	{MT_SETUP_ACKNOWLEDGE	, "SETUP_ACKNOWLEDGE"},
	{MT_RESUME		, "RESUME"},
	{MT_RESUME_ACKNOWLEDGE	, "RESUME_ACKNOWLEDGE"},
	{MT_RESUME_REJECT	, "RESUME_REJECT"},
	{MT_SUSPEND		, "SUSPEND"},
	{MT_SUSPEND_ACKNOWLEDGE	, "SUSPEND_ACKNOWLEDGE"},
	{MT_SUSPEND_REJECT	, "SUSPEND_REJECT"},
	{MT_USER_INFORMATION	, "USER_INFORMATION"},
	{MT_DISCONNECT		, "DISCONNECT"},
	{MT_RELEASE		, "RELEASE"},
	{MT_RELEASE_COMPLETE	, "RELEASE_COMPLETE"},
	{MT_RESTART		, "RESTART"},
	{MT_RESTART_ACKNOWLEDGE	, "RESTART_ACKNOWLEDGE"},
	{MT_SEGMENT		, "SEGMENT"},
	{MT_CONGESTION_CONTROL	, "CONGESTION_CONTROL"},
	{MT_INFORMATION		, "INFORMATION"},
	{MT_FACILITY		, "FACILITY"},
	{MT_NOTIFY		, "NOTIFY"},
	{MT_STATUS		, "STATUS"},
	{MT_STATUS_ENQUIRY	, "STATUS_ENQUIRY"},
	{MT_HOLD		, "HOLD"},
	{MT_HOLD_ACKNOWLEDGE	, "HOLD_ACKNOWLEDGE"},
	{MT_HOLD_REJECT		, "HOLD_REJECT"},
	{MT_RETRIEVE		, "RETRIEVE"},
	{MT_RETRIEVE_ACKNOWLEDGE, "RETRIEVE_ACKNOWLEDGE"},
	{MT_RETRIEVE_REJECT	, "RETRIEVE_REJECT"},
	{MT_REGISTER		, "REGISTER"},
	{MT_ASSIGN		, "ASSIGN_PID"},
	{MT_FREE		, "FREE_PID"},
	{MT_L2ESTABLISH		, "L2ESTABLISH"},
	{MT_L2RELEASE		, "L2RELEASE"},
	{MT_L2IDLE		, "L2IDLE"},
	{MT_ERROR		, "ERROR"},
	{MT_TIMEOUT		, "TIMEOUT"},
	{PH_ACTIVATE_REQ	, "PH_ACTIVATE_REQ"},
	{PH_DEACTIVATE_REQ	, "PH_DEACTIVATE_REQ"},
	{PH_DATA_REQ		, "PH_DATA_REQ"},
	{MPH_ACTIVATE_REQ	, "MPH_ACTIVATE_REQ"},
	{MPH_DEACTIVATE_REQ	, "MPH_DEACTIVATE_REQ"},
	{MPH_INFORMATION_REQ	, "MPH_INFORMATION_REQ"},
	{PH_CONTROL_REQ		, "PH_CONTROL_REQ"},
/* layer 1 -> layer 2 */
	{PH_ACTIVATE_IND	, "PH_ACTIVATE_IND"},
	{PH_ACTIVATE_CNF	, "PH_ACTIVATE_CNF"},
	{PH_DEACTIVATE_IND	, "PH_DEACTIVATE_IND"},
	{PH_DEACTIVATE_CNF	, "PH_DEACTIVATE_CNF"},
	{PH_DATA_IND		, "PH_DATA_IND"},
	{PH_DATA_E_IND		, "PH_DATA_E_IND"},
	{MPH_ACTIVATE_IND	, "MPH_ACTIVATE_IND"},
	{MPH_DEACTIVATE_IND	, "MPH_DEACTIVATE_IND"},
	{MPH_INFORMATION_IND	, "MPH_INFORMATION_IND"},
	{PH_DATA_CNF		, "PH_DATA_CNF"},
	{PH_CONTROL_IND		, "PH_CONTROL_IND"},
	{PH_CONTROL_CNF		, "PH_CONTROL_CNF"},

/* layer 3 -> layer 2 */
	{DL_ESTABLISH_REQ	, "DL_ESTABLISH_REQ"},
	{DL_RELEASE_REQ		, "DL_RELEASE_REQ"},
	{DL_DATA_REQ		, "DL_DATA_REQ"},
	{DL_UNITDATA_REQ	, "DL_UNITDATA_REQ"},
	{DL_INFORMATION_REQ	, "DL_INFORMATION_REQ"},

/* layer 2 -> layer 3 */
	{DL_ESTABLISH_IND	, "DL_ESTABLISH_IND"},
	{DL_ESTABLISH_CNF	, "DL_ESTABLISH_CNF"},
	{DL_RELEASE_IND		, "DL_RELEASE_IND"},
	{DL_RELEASE_CNF		, "DL_RELEASE_CNF"},
	{DL_DATA_IND		, "DL_DATA_IND"},
	{DL_UNITDATA_IND	, "DL_UNITDATA_IND"},
	{DL_INFORMATION_IND	, "DL_INFORMATION_IND"},

/* intern layer 2 management */
	{MDL_ASSIGN_REQ		, "MDL_ASSIGN_REQ"},
	{MDL_ASSIGN_IND		, "MDL_ASSIGN_IND"},
	{MDL_REMOVE_REQ		, "MDL_REMOVE_REQ"},
	{MDL_REMOVE_IND		, "MDL_REMOVE_IND"},
	{MDL_STATUS_UP_IND	, "MDL_STATUS_UP_IND"},
	{MDL_STATUS_DOWN_IND	, "MDL_STATUS_DOWN_IND"},
	{MDL_STATUS_UI_IND	, "MDL_STATUS_UI_IND"},
	{MDL_ERROR_IND		, "MDL_ERROR_IND"},
	{MDL_ERROR_RSP		, "MDL_ERROR_RSP"},

/* intern layer 2 */
	{DL_TIMER200_IND	, "DL_TIMER200_IND"},
	{DL_TIMER203_IND	, "DL_TIMER203_IND"},
	{DL_INTERN_MSG		, "DL_INTERN_MSG"},

/* L3 timer */
	{CC_T301		, "Timer 301"},
	{CC_T302		, "Timer 302"},
	{CC_T303		, "Timer 303"},
	{CC_T304		, "Timer 304"},
	{CC_T305		, "Timer 305"},
	{CC_T308_1		, "Timer 308(1)"},
	{CC_T308_2		, "Timer 308(2)"},
	{CC_T309		, "Timer 309"},
	{CC_T310		, "Timer 310"},
	{CC_T312		, "Timer 312"},
	{CC_T313		, "Timer 313"},
	{CC_T318		, "Timer 318"},
	{CC_T319		, "Timer 319"},
	{CC_TCTRL		, "Timer CTRL"},
	{CC_THOLD		, "Timer HOLD"},
	{CC_TRETRIEVE		, "Timer RETRIEVE"},
	{0xFFFFFFFF		, "UNKNOWN"}
};


const char *
mi_msg_type2str(unsigned int cmd)
{
	const struct _cmdtab *ct = cmdtab;

	while (ct->cmd != 0xFFFFFFFF) {
		if (ct->cmd == cmd)
			break;
		ct++;
	}
	if (ct->cmd == 0xFFFFFFFF)
		return NULL;
	else
		return ct->name;
}

static const char _unknown_mt[] = {"UNKNOWN"};

const char *
_mi_msg_type2str(unsigned int cmd)
{
	const char *t = mi_msg_type2str(cmd);

	if (!t)
		t = _unknown_mt;
	return t;
}
