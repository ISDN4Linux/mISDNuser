/* $Id$
 *
 */

#include "asn1.h"
#include <string.h>

int encodeLen_Long_u8(__u8 * dest, __u8 length)
{
	dest[0] = 0x80 + ASN1_NUM_OCTETS_LONG_LENGTH_u8 - 1;
	dest[1] = length;
	return ASN1_NUM_OCTETS_LONG_LENGTH_u8;
}				/* end encodeLen_Long_u8() */

int encodeLen_Long_u16(__u8 * dest, __u16 length)
{
	dest[0] = 0x80 + ASN1_NUM_OCTETS_LONG_LENGTH_u16 - 1;
	dest[1] = (length >> 8) & 0xFF;
	dest[2] = length & 0xFF;
	return ASN1_NUM_OCTETS_LONG_LENGTH_u16;
}				/* end encodeLen_Long_u16() */

int encodeNull(__u8 * dest, __u8 tagType)
{
	dest[0] = tagType;
	dest[1] = 0;		/* length */
	return 2;
}

int encodeBoolean(__u8 * dest, __u8 tagType, __u32 i)
{
	dest[0] = tagType;
	dest[1] = 1;		/* length */
	dest[2] = i ? 1 : 0;	/* value */
	return 3;
}

int encodeInt(__u8 * dest, __u8 tagType, __s32 i)
{
	unsigned count;
	__u32 test_mask;
	__u32 value;
	__u8 *p;

	dest[0] = tagType;

	/* Find most significant octet of 32 bit integer that carries meaning. */
	test_mask = 0xFF800000;
	value = (__u32) i;
	for (count = 4; --count;) {
		if ((value & test_mask) != test_mask && (value & test_mask) != 0) {
			/*
			 * The first 9 bits of a multiple octet integer is not
			 * all ones or zeroes.
			 */
			break;
		}
		test_mask >>= 8;
	}			/* end for */

	/* length */
	dest[1] = count + 1;

	/* Store value */
	p = &dest[2];
	do {
		value = (__u32) i;
		value >>= (8 * count);
		*p++ = value & 0xFF;
	} while (count--);

	return p - dest;
}

int encodeEnum(__u8 * dest, __u8 tagType, __s32 i)
{
	return encodeInt(dest, tagType, i);
}

/*
 * Use to encode the following string types:
 * ASN1_TAG_OCTET_STRING
 * ASN1_TAG_NUMERIC_STRING
 * ASN1_TAG_PRINTABLE_STRING
 * ASN1_TAG_IA5_STRING
 *
 * Note The string length MUST be less than 128 characters.
 */
static int encodeString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len)
{
	__u8 *p;
	int i;

	dest[0] = tagType;

	/* Store value */
	p = &dest[2];
	for (i = 0; i < len; i++)
		*p++ = *str++;

	/* length */
	dest[1] = p - &dest[2];

	return p - dest;
}

int encodeOctetString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len)
{
	return encodeString(dest, tagType, str, len);
}				/* end encodeOctetString() */

int encodeNumericString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len)
{
	return encodeString(dest, tagType, str, len);
}				/* end encodeNumericString() */

int encodePrintableString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len)
{
	return encodeString(dest, tagType, str, len);
}				/* end encodePrintableString() */

int encodeIA5String(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len)
{
	return encodeString(dest, tagType, str, len);
}				/* end encodeIA5String() */

int encodeOid(__u8 * dest, __u8 tagType, const struct asn1Oid *oid)
{
	unsigned numValues;
	unsigned count;
	__u32 value;
	__u8 *p;

	dest[0] = tagType;

	/* For all OID subidentifer values */
	p = &dest[2];
	for (numValues = 0; numValues < oid->numValues; ++numValues) {
		/*
		 * Count the number of 7 bit chunks that are needed
		 * to encode the integer.
		 */
		value = oid->value[numValues] >> 7;
		for (count = 0; value; ++count) {
			/* There are bits still set */
			value >>= 7;
		}		/* end for */

		/* Store OID subidentifier value */
		do {
			value = oid->value[numValues];
			value >>= (7 * count);
			*p++ = (value & 0x7F) | (count ? 0x80 : 0);
		} while (count--);
	}			/* end for */

	/* length */
	dest[1] = p - &dest[2];

	return p - dest;
}				/* end encodeOid() */
