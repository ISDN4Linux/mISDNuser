/* $Id: asn1.c,v 1.2 2006/08/16 13:14:54 nadi Exp $
 *
 */

#include "asn1.h"

int ParseTag(u_char *p, u_char *end, int *tag)
{
	*tag = *p;
	return 1;
}

int ParseLen(u_char *p, u_char *end, int *len)
{
	int l, i;

	if (*p == 0x80) { // indefinite
		*len = -1;
		return 1;
	}
	if (!(*p & 0x80)) { // one byte
		*len = *p;
		return 1;
	}
	*len = 0;
	l = *p & ~0x80;
	p++;
	for (i = 0; i < l; i++) {
		*len = (*len << 8) + *p; 
		p++;
	}
	return l+1;
}

#ifdef ASN1_DEBUG
int
ParseASN1(u_char *p, u_char *end, int level)
{
	int tag, len;
	int ret;
	int j;
	u_char *tag_end, *beg;

	beg = p;

	CallASN1(ret, p, end, ParseTag(p, end, &tag));
	CallASN1(ret, p, end, ParseLen(p, end, &len));
#ifdef ASN1_DEBUG
	for (j = 0; j < level*5; j++) print_asn1msg(PRT_DEBUG_DECODE, " ");
	print_asn1msg(PRT_DEBUG_DECODE, "TAG 0x%02x LEN %3d\n", tag, len);
#endif
	
	if (tag & ASN1_TAG_CONSTRUCTED) {
		if (len == -1) { // indefinite
			while (*p) {
				CallASN1(ret, p, end, ParseASN1(p, end, level + 1));
			}
			p++;
			if (*p) 
				return -1;
			p++;
		} else {
			tag_end = p + len;
			while (p < tag_end) {
				CallASN1(ret, p, end, ParseASN1(p, end, level +1));
			}
		}
	} else {
		for (j = 0; j < level*5; j++) print_asn1msg(PRT_DEBUG_DECODE, " ");
		while (len--) {
			print_asn1msg(PRT_DEBUG_DECODE, "%02x ", *p);
			p++;
		}
		print_asn1msg(PRT_DEBUG_DECODE, "\n");
	}
	for (j = 0; j < level*5; j++) print_asn1msg(PRT_DEBUG_DECODE, " ");
	print_asn1msg(PRT_DEBUG_DECODE, "END (%d)\n", p - beg - 2);
	return p - beg;
}
#endif
