/* q931.c
 *
 * Basic functions for all Q931 based protocols
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mbuffer.h>
#include <q931.h>
#include "layer3.h"

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
