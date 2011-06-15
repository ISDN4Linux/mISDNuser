/* $Id$
 *
 */

#include "asn1.h"

// ======================================================================
// general ASN.1

int ParseBoolean(struct asn1_parm *pc, u_char * p, u_char * end, int *i)
{
	INIT;

	CHECK_P;
	*i = *p ? 1 : 0;
	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> BOOL = %d %#x\n", *i, *i);
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

	CHECK_P;
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

	CHECK_P;
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
	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> INT = %d %#x\n", *i, *i);
	return length;
}

int ParseSignedInteger(struct asn1_parm *pc, u_char * p, u_char * end, signed int *i)
{
	int length;

	length = ParseSInt(pc, p, end, i);
	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> INT = %d %#x\n", *i, *i);
	return length;
}

int ParseEnum(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *i)
{
	int length;

	length = ParseUInt(pc, p, end, i);
	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> ENUM = %d %#x\n", *i, *i);
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

	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> IA5 = ");
	if (str->maxSize < len + 1) {
		numChars = str->maxSize - 1;
	} else {
		numChars = len;
	}
	len -= numChars;
	str->length = numChars;
	buf = str->buf;
	while (numChars--) {
		print_asn1msg(PRT_DEBUG_DECODE, "%c", *p);
		*buf++ = *p++;
	}			/* end while */
	print_asn1msg(PRT_DEBUG_DECODE, "\n");
	*buf = 0;
	if (0 < len) {
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

	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> NumStr = ");
	if (str->maxSize < len + 1) {
		numChars = str->maxSize - 1;
	} else {
		numChars = len;
	}
	len -= numChars;
	str->length = numChars;
	buf = str->buf;
	while (numChars--) {
		print_asn1msg(PRT_DEBUG_DECODE, "%c", *p);
		*buf++ = *p++;
	}			/* end while */
	print_asn1msg(PRT_DEBUG_DECODE, "\n");
	*buf = 0;
	if (0 < len) {
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
		return -1;
	}

	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> Octets = ");
	str->length = len;
	buf = str->buf;
	while (len--) {
		print_asn1msg(PRT_DEBUG_DECODE, " %02x", *p);
		*buf++ = *p++;
	}			/* end while */
	print_asn1msg(PRT_DEBUG_DECODE, "\n");
	*buf = 0;
	return p - beg;
}

int ParseOid(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1Oid *oid)
{
	int numValues;
	int value;
#if defined(ASN1_DEBUG)
	int delimiter;
#endif				/* defined(ASN1_DEBUG) */
	INIT;

	print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> OID =");
#if defined(ASN1_DEBUG)
	delimiter = ' ';
#endif				/* defined(ASN1_DEBUG) */
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
#if defined(ASN1_DEBUG)
					print_asn1msg(PRT_DEBUG_DECODE, "%c%d", delimiter, value);
					delimiter = '.';
#endif				/* defined(ASN1_DEBUG) */
				} else {
					/* Too many OID subidentifier values */
#if defined(ASN1_DEBUG)
					delimiter = '~';
					print_asn1msg(PRT_DEBUG_DECODE, "%c%d", delimiter, value);
#endif				/* defined(ASN1_DEBUG) */
				}
				++numValues;
				break;
			}
			if (!len) {
				oid->numValues = 0;
				print_asn1msg(PRT_DEBUG_DECODE, "\n");
				print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> Last OID subidentifier value not terminated!\n");
				return -1;
			}
		}		/* end for */
	}			/* end while */
	print_asn1msg(PRT_DEBUG_DECODE, "\n");

	if (numValues <= sizeof(oid->value) / sizeof(oid->value[0])) {
		oid->numValues = numValues;
		return p - beg;
	} else {
		/* Need to increase the size of the OID subidentifier list. */
		oid->numValues = 0;
		print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> Too many OID values!\n");
		return -1;
	}
}				/* end ParseOid() */
