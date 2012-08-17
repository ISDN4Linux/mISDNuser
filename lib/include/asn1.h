/*
 *
 * Abstract Syntax Notation.1 (ASN.1) ITU-T X.208
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

#ifndef __ASN1_H__
#define __ASN1_H__

#include <mISDN/suppserv.h>
#include <asm/types.h>
#include <sys/types.h>
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

int ParseTag(u_char * p, u_char * end, int *tag);
int ParseLen(u_char * p, u_char * end, int *len);

#define ASN1_TAG_BOOLEAN           1
#define ASN1_TAG_INTEGER           2
#define ASN1_TAG_BIT_STRING        3
#define ASN1_TAG_OCTET_STRING      4
#define ASN1_TAG_NULL              5
#define ASN1_TAG_OBJECT_IDENTIFIER 6
#define ASN1_TAG_ENUM              10
#define ASN1_TAG_SEQUENCE          (ASN1_TAG_CONSTRUCTED | 16)
#define ASN1_TAG_SET               (ASN1_TAG_CONSTRUCTED | 17)
#define ASN1_TAG_NUMERIC_STRING    18
#define ASN1_TAG_PRINTABLE_STRING  19
#define ASN1_TAG_IA5_STRING        22
#define ASN1_TAG_UTC_TIME          23

#define ASN1_TAG_CONSTRUCTED       0x20

#define ASN1_TAG_TYPE_MASK         0xC0
#define ASN1_TAG_UNIVERSAL         0x00
#define ASN1_TAG_APPLICATION_WIDE  0x40
#define ASN1_TAG_CONTEXT_SPECIFIC  0x80
#define ASN1_TAG_PRIVATE           0xC0

#define ASN1_TAG_EXPLICIT          0x100
#define ASN1_TAG_OPT               0x200
#define ASN1_NOT_TAGGED            0x400

#define CallASN1(ret, p, end, todo) \
	do { \
		ret = todo; \
		if (ret < 0) { \
			eprint("CallASN1 internal error\n"); \
			return -1; \
		} \
		p += ret; \
	} while (0)

/* INIT must be placed after the last variable declared */
#define INIT \
	int tag, len; \
	int ret; \
	u_char *beg __attribute__((unused)); \
	\
	beg = p; \
	CallASN1(ret, p, end, ParseTag(p, end, &tag)); \
	CallASN1(ret, p, end, ParseLen(p, end, &len)); \
	if (len >= 0) { \
		if (p + len > end) { \
			eprint("ASN1 parser underflow %p/%p in %s\n", p + len, end, __func__); \
			return -1; \
		} \
		dprint(DBGM_ASN1_DEC, "Start p=%p end=%p-->%p len=%d\n", p, end, p + len, len); \
		end = p + len; \
	}

#define XSEQUENCE_1(todo, act_tag, the_tag, arg1) \
	do { \
		if (p < end) { \
			if (((the_tag) & ~ASN1_TAG_OPT) == ASN1_NOT_TAGGED) { \
				if (((act_tag) == ASN1_NOT_TAGGED) || ((u_char) (act_tag) == *p)) { \
					CallASN1(ret, p, end, todo(pc, p, end, arg1)); \
				} else { \
					if (!((the_tag) & ASN1_TAG_OPT)) { \
						eprint("ASN1 XSEQUENCE_1 internal error 1\n"); \
						return -1; \
					} \
				} \
			} else if ((the_tag) & ASN1_TAG_EXPLICIT) { \
				/* EXPLICIT tags are always constructed */ \
				if ((u_char) (((the_tag) & 0xff) | (((act_tag) & ASN1_TAG_TYPE_MASK) | ASN1_TAG_CONSTRUCTED)) == *p) { \
					int xtag, xlen; \
					CallASN1(ret, p, end, ParseTag(p, end, &xtag)); \
					CallASN1(ret, p, end, ParseLen(p, end, &xlen)); \
					CallASN1(ret, p, end, todo(pc, p, end, arg1)); \
				} else { \
					if (!((the_tag) & ASN1_TAG_OPT)) { \
						eprint("ASN1 XSEQUENCE_1 internal error 2\n"); \
						return -1; \
					} \
				} \
			} else { /* IMPLICIT */ \
				if ((u_char) (((the_tag) & 0xff) | ((act_tag) & (ASN1_TAG_TYPE_MASK | ASN1_TAG_CONSTRUCTED))) == *p) { \
					CallASN1(ret, p, end, todo(pc, p, end, arg1)); \
				} else { \
					if (!((the_tag) & ASN1_TAG_OPT)) { \
						eprint("ASN1 XSEQUENCE_1 internal error 3\n"); \
						return -1; \
					} \
				} \
			} \
		} else { \
			if (!((the_tag) & ASN1_TAG_OPT)) { \
				eprint("ASN1 XSEQUENCE_1 internal error 4\n"); \
				return -1; \
			} \
		} \
	} while (0)

#define XSEQUENCE_OPT_1(todo, act_tag, the_tag, arg1) \
        XSEQUENCE_1(todo, act_tag, (the_tag | ASN1_TAG_OPT), arg1)

#define XSEQUENCE(todo, act_tag, the_tag) XSEQUENCE_1(todo, act_tag, the_tag, -1)
#define XSEQUENCE_OPT(todo, act_tag, the_tag) XSEQUENCE_OPT_1(todo, act_tag, the_tag, -1)

#define XCHOICE_1(todo, act_tag, the_tag, arg1) \
	do { \
		if ((act_tag) == ASN1_NOT_TAGGED) { \
			return todo(pc, beg, end, arg1); \
		} else if ((the_tag) == ASN1_NOT_TAGGED) { \
			if ((act_tag) == tag) { \
				return todo(pc, beg, end, arg1); \
			} \
		} else if ((act_tag) & ASN1_TAG_EXPLICIT) { \
			/* EXPLICIT tags are always constructed */ \
			if (((the_tag) | (((act_tag) & ASN1_TAG_TYPE_MASK) | ASN1_TAG_CONSTRUCTED)) == tag) { \
				return todo(pc, p, end, arg1); \
			} \
		} else { \
			if (((the_tag) | ((act_tag) & (ASN1_TAG_TYPE_MASK | ASN1_TAG_CONSTRUCTED))) == tag) { \
				return todo(pc, beg, end, arg1); \
			} \
		} \
	} while (0)

#define XCHOICE(todo, act_tag, the_tag) XCHOICE_1(todo, act_tag, the_tag, -1)

#define XCHOICE_DEFAULT \
	do { \
		eprint("ASN1 XCHOICE_DEFAULT internal error in %s\n", __func__); \
		return -1; \
	} while (0)

#define CHECK_P \
	do { \
		if (p >= end) { \
			eprint("ASN1 parser underflow %p/%p in %s\n", p, end, __func__); \
			return -1; \
		} \
	} while (0)

	const struct asn1OidConvert *FindOidByOidValue(int length, const __u16 oidValues[]);
	const struct asn1OidConvert *FindOidByEnum(__u16 value);
	__u16 ConvertOidToEnum(const struct asn1Oid *oid, __u16 errorValue);
	int ConvertEnumToOid(struct asn1Oid *oid, __u16 enumValue);
#define IsEnumOid(enumValue)	\
	((FAC_OID_BASE(1) <= (enumValue) \
		&& (enumValue) < FAC_OID_BASE(FacOIDBase_Last)) ? 1 : 0)

/*
** ASN.1 Encoding
*/

/* Facility-Information-Element-Components prototypes */
	enum asn1ComponentTag {
		asn1ComponentTag_Invoke = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 1,
		asn1ComponentTag_Result = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 2,
		asn1ComponentTag_Error = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 3,
		asn1ComponentTag_Reject = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 4,
	};
	__u8 *encodeComponent_Head(__u8 * p, enum asn1ComponentTag componentTag);
	__u8 *encodeComponent_Head_Long_u8(__u8 * p, enum asn1ComponentTag componentTag);
	int encodeComponent_Length(__u8 * msg, __u8 * end);
	int encodeComponent_Length_Long_u8(__u8 * msg, __u8 * p);

	__u8 *encodeComponentInvoke_Head(__u8 * Dest, int InvokeID, enum Operation OperationValue);
	__u8 *encodeComponentInvoke_Head_Long_u8(__u8 * Dest, int InvokeID, enum Operation OperationValue);

	int encodeOperationValue(__u8 * dest, int operationValue);
	int encodeErrorValue(__u8 * dest, int errorValue);

/* Primitive ASN.1 prototypes */
#define ASN1_NUM_OCTETS_LONG_LENGTH_u8	2
#define ASN1_NUM_OCTETS_LONG_LENGTH_u16	3
	int encodeLen_Long_u8(__u8 * dest, __u8 length);
	int encodeLen_Long_u16(__u8 * dest, __u16 length);

	int encodeNull(__u8 * dest, __u8 tagType);
	int encodeBoolean(__u8 * dest, __u8 tagType, __u32 i);
	int encodeInt(__u8 * dest, __u8 tagType, __s32 i);
	int encodeEnum(__u8 * dest, __u8 tagType, __s32 i);
	int encodeOctetString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len);
	int encodeNumericString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len);
	int encodePrintableString(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len);
	int encodeIA5String(__u8 * dest, __u8 tagType, const __s8 * str, __u8 len);
	int encodeOid(__u8 * dest, __u8 tagType, const struct asn1Oid *oid);

/* Addressing-Data-Elements prototypes */
	int encodePartyNumber_Full(__u8 * Dest, const struct FacPartyNumber *PartyNumber);
	int encodePartySubaddress_Full(__u8 * Dest, const struct FacPartySubaddress *PartySubaddress);
	int encodeAddress_Full(__u8 * Dest, const struct FacAddress *Address);
	int encodePresentedNumberUnscreened_Full(__u8 * Dest, const struct FacPresentedNumberUnscreened *Presented);
	int encodePresentedAddressScreened_Full(__u8 * Dest, const struct FacPresentedAddressScreened *Presented);

/*
** ASN.1 Parsing
*/

/* Facility-Information-Element-Components prototypes */
	int ParseComponent(struct asn1_parm *parm, u_char * p, u_char * end);

/* Primitive ASN.1 prototypes */
	int ParseBoolean(struct asn1_parm *pc, u_char * p, u_char * end, int *i);
	int ParseNull(struct asn1_parm *pc, u_char * p, u_char * end, int dummy);
	int ParseUnsignedInteger(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *i);
	int ParseSignedInteger(struct asn1_parm *pc, u_char * p, u_char * end, signed int *i);
	int ParseEnum(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *i);

	struct asn1ParseString {
		char *buf;	/* Where to put the parsed string characters */
		size_t maxSize;	/* sizeof string buffer (Including an ASCIIz terminator) */
		size_t length;	/* length of string put into the string buffer (Without the terminator) */
	};
	int ParseIA5String(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1ParseString *str);
	int ParseNumericString(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1ParseString *str);
	int ParseOctetString(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1ParseString *str);
	int ParseOid(struct asn1_parm *pc, u_char * p, u_char * end, struct asn1Oid *oid);

/* Addressing-Data-Elements prototypes */
	int ParsePartyNumber_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartyNumber *PartyNumber);
	int ParsePartySubaddress_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartySubaddress *PartySubaddress);
	int ParseAddress_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAddress *Address);
	int ParsePresentedNumberUnscreened_Full(struct asn1_parm *pc, u_char * p, u_char * end,
						struct FacPresentedNumberUnscreened *Presented);
	int ParsePresentedAddressScreened_Full(struct asn1_parm *pc, u_char * p, u_char * end,
					       struct FacPresentedAddressScreened *Presented);

/* Advice Of Charge (AOC) prototypes */
	int ParseAOCECurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur);
	int ParseAOCDChargingUnit(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu);
	int ParseAOCDCurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur);
	int ParseAOCDCurrencyInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur);
	int ParseAOCDChargingUnitInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu);
	int ParseRecordedCurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur);
	int ParseRecordedUnitsList(struct asn1_parm *pc, u_char * p, u_char * end, __u32 *recordedUnits);
	int ParseTypeOfChargingInfo(struct asn1_parm *pc, u_char * p, u_char * end, __u32 *typeOfChargingInfo);
	int ParseRecordedUnits(struct asn1_parm *pc, u_char * p, u_char * end, __u32 *recordedUnits);
	int ParseAOCDBillingId(struct asn1_parm *pc, u_char * p, u_char * end, __u32 *billingId);
	int ParseAOCECurrencyInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur);
	int ParseAOCEChargingUnit(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu);
	int ParseAOCEChargingUnitInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu);
	int ParseAOCEBillingId(struct asn1_parm *pc, u_char * p, u_char * end, __u32 *billingId);
	int ParseCurrency(struct asn1_parm *pc, u_char * p, u_char * end, char *currency);
	int ParseAmount(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur);
	int ParseCurrencyAmount(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *currencyAmount);
	int ParseMultiplier(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *multiplier);
	int ParseTypeOfUnit(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *typeOfUnit);
	int ParseNumberOfUnits(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *numberOfUnits);
	int ParseChargingAssociation(struct asn1_parm *pc, u_char * p, u_char * end, struct ChargingAssociation *chargeAssoc);
	int ParseChargeIdentifier(struct asn1_parm *pc, u_char * p, u_char * end, int *chargeIdentifier);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_H__ */
/* ------------------------------------------------------------------- *//* end asn1.h */
