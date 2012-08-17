/*
 * ASN1 parser for standard elements
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

// ======================================================================
// general ASN.1

int ParseBoolean(struct asn1_parm *pc, u_char * p, u_char * end, int *i)
{
	INIT;

	*i = *p ? 1 : 0;
	dprint(DBGM_ASN1_DEC, " DEBUG> BOOL = %d %#x\n", *i, *i);
	return end - beg;
}

int ParseNull(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	INIT;

	return p - beg;
}

static int ParseUInt(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *i)
{
	INIT;

	*i = 0;
	while (len--) {
		*i = (*i << 8) | *p;
		p++;
	}
	return p - beg;
}				/* end ParseInt() */

static int ParseSInt(struct asn1_parm *pc, u_char * p, u_char * end, signed int *i)
{
	INIT;

	/* Read value as signed */
	if (*p & 0x80) {
		/* The value is negative */
		*i = -1;
	} else {
		*i = 0;
	}
	while (len--) {
		*i = (*i << 8) | *p;
		p++;
	}
	return p - beg;
}				/* end ParseInt() */

int ParseUnsignedInteger(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *i)
{
	int length;

	length = ParseUInt(pc, p, end, i);
	dprint(DBGM_ASN1_DEC, " DEBUG> INT = %d %#x\n", *i, *i);
	return length;
}

int ParseSignedInteger(struct asn1_parm *pc, u_char * p, u_char * end, signed int *i)
{
	int length;

	length = ParseSInt(pc, p, end, i);

	dprint(DBGM_ASN1_DEC, " DEBUG> INT = %d %#x\n", *i, *i);
	return length;
}

int ParseEnum(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *i)
{
	int length;

	length = ParseUInt(pc, p, end, i);
	dprint(DBGM_ASN1_DEC, " DEBUG> ENUM = %d %#x\n", *i, *i);
	return length;
}

int ParseIA5String(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1ParseString *str)
{
	char *buf;
	int numChars;
	INIT;

	if (len < 0) {
		/* We do not handle indefinite length strings */
		return -1;
	}

	if (str->maxSize < len + 1) {
		numChars = str->maxSize - 1;
	} else {
		numChars = len;
	}
	len -= numChars;
	str->length = numChars;
	buf = str->buf;
	while (numChars--)
		*buf++ = *p++;
	*buf = 0;
	dprint(DBGM_ASN1_DEC, " DEBUG> IA5 = %s\n", str->buf);
	if (0 < len) {
		wprint("Discard %d IA5 max %zd\n", len, str->maxSize);
		/* Discard the remainder of the string.  We have no room left. */
		p += len;
	}
	return p - beg;
}

int ParseNumericString(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1ParseString *str)
{
	char *buf;
	int numChars;
	INIT;

	if (len < 0) {
		/* We do not handle indefinite length strings */
		return -1;
	}

	if (str->maxSize < len + 1) {
		numChars = str->maxSize - 1;
	} else {
		numChars = len;
	}
	len -= numChars;
	str->length = numChars;
	buf = str->buf;
	while (numChars--)
		*buf++ = *p++;
	*buf = 0;
	dprint(DBGM_ASN1_DEC, " DEBUG> NumStr = %s\n", str->buf);
	if (0 < len) {
		wprint("Discard %d NumStr max %zd\n", len, str->maxSize);
		/* Discard the remainder of the string.  We have no room left. */
		p += len;
	}
	return p - beg;
}

int ParseOctetString(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1ParseString *str)
{
	char *buf;
	INIT;

	if (len < 0) {
		/* We do not handle indefinite length strings */
		return -1;
	}
	if (str->maxSize < len + 1) {
		/*
		 * The octet string will not fit in the available buffer
		 * and truncating it is not a good idea in all cases.
		 */
		eprint("OctetString does not fit %d max %zd\n", len, str->maxSize);
		return -1;
	}

	str->length = len;
	buf = str->buf;
	while (len--)
		*buf++ = *p++;
	*buf = 0;
	if (DBGM_ASN1_DEC & mI_debug_mask) {
		char *tmp = malloc((3 * len) + 1);

		if (tmp) {
			mi_shexprint(tmp, p, len);
			dprint(DBGM_ASN1_DEC, " DEBUG> Octets = %s\n", tmp);
			free(tmp);
		} else
			eprint("MALLOC for %d bytes failed\n", (3 * len) + 1);
	}
	return p - beg;
}

int ParseOid(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1Oid *oid)
{
	int numValues;
	int value;
	INIT;

	numValues = 0;
	while (len) {
		value = 0;
		for (;;) {
			--len;
			value = (value << 7) | (*p & 0x7F);
			if (!(*p++ & 0x80)) {
				/* Last octet in the OID subidentifier value */
				if (numValues < sizeof(oid->value) / sizeof(oid->value[0])) {
					oid->value[numValues] = value;
				} else {
					dprint(DBGM_ASN1_WARN, "Too many OID subidentifier %d\n", value);
				}
				++numValues;
				break;
			}
			if (!len) {
				oid->numValues = 0;
				wprint("Last OID subidentifier value (%d) not terminated!\n", value);
				return -1;
			}
		}		/* end for */
	}			/* end while */

	if (numValues <= sizeof(oid->value) / sizeof(oid->value[0])) {
		oid->numValues = numValues;
		for (numValues = 0; numValues < oid->numValues; numValues++)
			dprint(DBGM_ASN1_DEC, "OID->value[%d] = %d\n", numValues, oid->value[numValues]);
		return p - beg;
	} else {
		/* Need to increase the size of the OID subidentifier list. */
		wprint("Too many OID values (%d)!\n", numValues);
		return -1;
	}
}
