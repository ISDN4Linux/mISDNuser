/*
 * Basic ASN1 parser functions
 *
 * Copyright 2009,2010  by Karsten Keil <kkeil@linux-pingi.de>
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

#include "asn1.h"

int ParseTag(u_char * p, u_char * end, int *tag)
{
	if (p >= end) {
		eprint("ParseTag underflow %p/%p\n", p, end);
		return -1;
	}
	*tag = *p;
	return 1;
}

int ParseLen(u_char * p, u_char * end, int *len)
{
	int l, i;

	if (p >= end) {
		eprint("ParseLen underflow %p/%p\n", p, end);
		return -1;
	}
	if (*p == 0x80) {	// indefinite
		*len = -1;
		return 1;
	}
	if (!(*p & 0x80)) {	// one byte
		*len = *p;
		return 1;
	}
	*len = 0;
	l = *p & ~0x80;
	p++;
	if (p + l >= end) {
		eprint("ParseLen underflow %p/%p\n", p, end);
		return -1;
	}
	for (i = 0; i < l; i++) {
		*len = (*len << 8) + *p;
		p++;
	}
	return l + 1;
}
