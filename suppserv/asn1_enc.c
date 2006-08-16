/* $Id: asn1_enc.c,v 1.2 2006/08/16 14:15:52 nadi Exp $
 *
 */

#include "asn1.h"
#include <string.h>

int encodeNull(__u8 *dest)
{
	dest[0] = 0x05;  // null
	dest[1] = 0;     // length
	return 2;
}

int encodeBoolean(__u8 *dest, __u32 i)
{
	dest[0] = 0x01;  // BOOLEAN
	dest[1] = 1;     // length 1
	dest[2] = i ? 1:0;  // Value
	return 3;
}

int encodeInt(__u8 *dest, __u32 i)
{
	__u8 *p;

	dest[0] = 0x02;  // integer
	dest[1] = 0;     // length
	p = &dest[2];
	do {
		*p++ = i;
		i >>= 8;
	} while (i);

	dest[1] = p - &dest[2];
	return p - dest;
}

int encodeEnum(__u8 *dest, __u32 i)
{
	__u8 *p;

	dest[0] = 0x0a;  // integer
	dest[1] = 0;     // length
	p = &dest[2];
	do {
		*p++ = i;
		i >>= 8;
	} while (i);

	dest[1] = p - &dest[2];
	return p - dest;
}

int encodeNumberDigits(__u8 *dest, __s8 *nd, __u8 len)
{
	__u8 *p;
	int i;

	dest[0] = 0x12;    // numeric string
	dest[1] = 0x0;     // length
	p = &dest[2];
	for (i = 0; i < len; i++)
		*p++ = *nd++;

	dest[1] = p - &dest[2];
	return p - dest;
}

int encodePublicPartyNumber(__u8 *dest, __s8 *facilityPartyNumber)
{
	__u8 *p;

	dest[0] = 0x20;  // sequence
	dest[1] = 0;     // length
	p = &dest[2];
	p += encodeEnum(p, (facilityPartyNumber[2] & 0x70) >> 4);
	p += encodeNumberDigits(p, &facilityPartyNumber[4], facilityPartyNumber[0] - 3);

	dest[1] = p - &dest[2];
	return p - dest;
}

int encodePartyNumber(__u8 *dest, __s8 *facilityPartyNumber)
{
	__u8 *p = dest;

	p += encodeNumberDigits(p, facilityPartyNumber, strlen((char *)facilityPartyNumber));
	dest[0] = 0x80;
#if 0
	switch (facilityPartyNumber[1]) {
	case 0: // unknown
		p += encodeNumberDigits(p, &facilityPartyNumber[4], facilityPartyNumber[0] - 3);
		dest[0] &= 0x20;
		dest[0] |= 0x81;
		break;
	case 1: // publicPartyNumber
		p += encodePublicPartyNumber(p, facilityPartyNumber);
		dest[0] &= 0x20;
		dest[0] |= 0x81;
		break;
	default:
		int_error();
		return -1;
	}
#endif
	return p - dest;
}

int encodeServedUserNumber(__u8 *dest, __s8 *servedUserNumber)
{
	if (servedUserNumber[0])
		return encodePartyNumber(dest, servedUserNumber);
        else
		return encodeNull(dest);
}

int encodeAddress(__u8 *dest, __s8 *facilityPartyNumber, __s8 *calledPartySubaddress)
{
	__u8 *p = dest;

	dest[0] = 0x30;  // invoke id tag, integer
	dest[1] = 0;     // length
	p = &dest[2];

	p += encodePartyNumber(p, facilityPartyNumber);
#if 0 // FIXME
	if (calledPartySubaddress[0])
		p += encodePartySubaddress(p, calledPartySubaddress);
#endif
	dest[1] = p - &dest[2];
	return p - dest;
}

