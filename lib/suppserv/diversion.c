/*
 *
 * Diversion Supplementary Services ETS 300 207-1 Table 3
 *
 * Diversion Facility ie encode/decode
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
#include "diversion.h"
#include <string.h>

/* ------------------------------------------------------------------- */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the Q.931 ie UserUser value used for Diversion.
 *
 * \param Dest Where to put the encoding
 * \param Q931ie Q931 ie information to encode.
 *
 * \retval length
 */
static int encodeQ931_Div_UserInfo(__u8 * Dest, const struct Q931_UserUserInformation *Q931ie)
{
	int Length;
	__u8 *p;

	/* [APPLICATION 0] IMPLICIT OCTET STRING */
	Dest[0] = ASN1_TAG_APPLICATION_WIDE | 0;

	/* Pre-calculate content length */
	if (Q931ie->Length) {
		Length = 2 + Q931ie->Length;
	} else {
		Length = 0;
	}

	/* Store length */
	if (Length < 0x80) {
		Dest[1] = Length;
		p = &Dest[2];
	} else {
		encodeLen_Long_u8(&Dest[1], Length);
		p = &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];
	}

	/* Store value */
	if (Q931ie->Length) {
		*p++ = 0x7E;	/* Q.931 User-User-Information ie tag */
		*p++ = Q931ie->Length;
		memcpy(p, Q931ie->Contents, Q931ie->Length);
		p += Q931ie->Length;
	}

	return p - Dest;
}				/* end encodeQ931_Div_UserInfo() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the Q.931 argument contents for Diversion.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Q931ie Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseQ931_Div_UserInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct Q931_UserUserInformation *Q931ie)
{
	__u8 Ie_type;
	size_t Length;
	INIT;

	memset(Q931ie, 0, sizeof(*Q931ie));
	do {
		if (end < p + 2) {
			/* Q.931 ie length error */
			return -1;
		}
		Ie_type = *p++;
		Length = *p++;
		if (end < p + Length) {
			/* Q.931 ie length error */
			return -1;
		}
		switch (Ie_type) {
		case 0x7E:	/* Q.931 User-User-Information ie tag */
			if (Length <= sizeof(Q931ie->Contents)) {
				Q931ie->Length = Length;
				memcpy(Q931ie->Contents, p, Length);
			}
			break;
		default:
			/* Unknown Q.931 ie in Diversion message */
			break;
		}		/* end switch */
		p += Length;
	} while (p < end);

	return p - beg;
}				/* end ParseQ931_Div_UserInfo() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the Q.931 ie value used for Diversion.
 *
 * \param Dest Where to put the encoding
 * \param Q931ie Q931 ie information to encode.
 *
 * \retval length
 */
static int encodeQ931_Div(__u8 * Dest, const struct Q931_Bc_Hlc_Llc_Uu *Q931ie)
{
	int Length;
	__u8 *p;

	/* [APPLICATION 0] IMPLICIT OCTET STRING */
	Dest[0] = ASN1_TAG_APPLICATION_WIDE | 0;

	/* Pre-calculate content length */
	Length = 0;
	if (Q931ie->Bc.Length) {
		Length += 2 + Q931ie->Bc.Length;
	}
	if (Q931ie->Hlc.Length) {
		Length += 2 + Q931ie->Hlc.Length;
	}
	if (Q931ie->Llc.Length) {
		Length += 2 + Q931ie->Llc.Length;
	}
	if (Q931ie->UserInfo.Length) {
		Length += 2 + Q931ie->UserInfo.Length;
	}

	/* Store length */
	if (Length < 0x80) {
		Dest[1] = Length;
		p = &Dest[2];
	} else {
		encodeLen_Long_u8(&Dest[1], Length);
		p = &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];
	}

	/* Store value */
	if (Q931ie->Bc.Length) {
		*p++ = 0x04;	/* Q.931 Bearer Capability ie tag */
		*p++ = Q931ie->Bc.Length;
		memcpy(p, Q931ie->Bc.Contents, Q931ie->Bc.Length);
		p += Q931ie->Bc.Length;
	}
	if (Q931ie->Llc.Length) {
		*p++ = 0x7C;	/* Q.931 Low Layer Compatibility ie tag */
		*p++ = Q931ie->Llc.Length;
		memcpy(p, Q931ie->Llc.Contents, Q931ie->Llc.Length);
		p += Q931ie->Llc.Length;
	}
	if (Q931ie->Hlc.Length) {
		*p++ = 0x7D;	/* Q.931 High Layer Compatibility ie tag */
		*p++ = Q931ie->Hlc.Length;
		memcpy(p, Q931ie->Hlc.Contents, Q931ie->Hlc.Length);
		p += Q931ie->Hlc.Length;
	}
	if (Q931ie->UserInfo.Length) {
		*p++ = 0x7E;	/* Q.931 User-User-Information ie tag */
		*p++ = Q931ie->UserInfo.Length;
		memcpy(p, Q931ie->UserInfo.Contents, Q931ie->UserInfo.Length);
		p += Q931ie->UserInfo.Length;
	}

	return p - Dest;
}				/* end encodeQ931_Div() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the Q.931 argument contents for Diversion.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Q931ie Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseQ931_Div(struct asn1_parm *pc, u_char * p, u_char * end, struct Q931_Bc_Hlc_Llc_Uu *Q931ie)
{
	__u8 Ie_type;
	size_t Length;
	INIT;

	memset(Q931ie, 0, sizeof(*Q931ie));
	do {
		if (end < p + 2) {
			/* Q.931 ie length error */
			return -1;
		}
		Ie_type = *p++;
		Length = *p++;
		if (end < p + Length) {
			/* Q.931 ie length error */
			return -1;
		}
		switch (Ie_type) {
		case 0x04:	/* Q.931 Bearer Capability ie tag */
			if (Length <= sizeof(Q931ie->Bc.Contents)) {
				Q931ie->Bc.Length = Length;
				memcpy(Q931ie->Bc.Contents, p, Length);
			}
			break;
		case 0x7C:	/* Q.931 Low Layer Compatibility ie tag */
			if (Length <= sizeof(Q931ie->Llc.Contents)) {
				Q931ie->Llc.Length = Length;
				memcpy(Q931ie->Llc.Contents, p, Length);
			}
			break;
		case 0x7D:	/* Q.931 High Layer Compatibility ie tag */
			if (Length <= sizeof(Q931ie->Hlc.Contents)) {
				Q931ie->Hlc.Length = Length;
				memcpy(Q931ie->Hlc.Contents, p, Length);
			}
			break;
		case 0x7E:	/* Q.931 User-User-Information ie tag */
			if (Length <= sizeof(Q931ie->UserInfo.Contents)) {
				Q931ie->UserInfo.Length = Length;
				memcpy(Q931ie->UserInfo.Contents, p, Length);
			}
			break;
		default:
			/* Unknown Q.931 ie in Diversion message */
			break;
		}		/* end switch */
		p += Length;
	} while (p < end);

	return p - beg;
}				/* end ParseQ931_Div() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the ServedUserNr type.
 *
 * \param Dest Where to put the encoding
 * \param ServedUser Served user number information to encode.
 *
 * \retval length
 */
static int encodeServedUserNumber_Full(__u8 * Dest, const struct FacPartyNumber *ServedUser)
{
	int Length;

	if (ServedUser->LengthOfNumber) {
		/* Forward this number */
		Length = encodePartyNumber_Full(Dest, ServedUser);
	} else {
		/* Forward all numbers */
		Length = encodeNull(Dest, ASN1_TAG_NULL);
	}

	return Length;
}				/* end encodeServedUserNumber_Full() */

/* ******************************************************************* */
/*!
 * \brief Parse the ServedUserNr argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param ServedUser Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseServedUserNumber_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartyNumber *ServedUser)
{
	INIT;

	ServedUser->LengthOfNumber = 0;
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);

	/* Must be a PartyNumber (Which is itself a CHOICE) */
	return ParsePartyNumber_Full(pc, beg, end, ServedUser);
}				/* end ParseServedUserNumber_Full() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the IntResult type.
 *
 * \param Dest Where to put the encoding
 * \param IntResult Forwarding record information to encode.
 *
 * \retval length
 */
static int encodeIntResult(__u8 * Dest, const struct FacForwardingRecord *IntResult)
{
	__u8 *p;

	Dest[0] = ASN1_TAG_SEQUENCE;
	p = &Dest[2];

	p += encodeServedUserNumber_Full(p, &IntResult->ServedUser);
	p += encodeEnum(p, ASN1_TAG_ENUM, IntResult->BasicService);
	p += encodeEnum(p, ASN1_TAG_ENUM, IntResult->Procedure);
	p += encodeAddress_Full(p, &IntResult->ForwardedTo);

	/* length */
	Dest[1] = p - &Dest[2];

	return p - Dest;
}				/* end encodeIntResult() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the IntResult argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param IntResult Forwarding record storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseIntResult(struct asn1_parm *pc, u_char * p, u_char * end, struct FacForwardingRecord *IntResult)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseServedUserNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &IntResult->ServedUser);
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	IntResult->BasicService = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	IntResult->Procedure = Value;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &IntResult->ForwardedTo);

	return p - beg;
}				/* end ParseIntResult() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the IntResultList type.
 *
 * \param Dest Where to put the encoding
 * \param IntResultList Forwarding record list information to encode.
 *
 * \retval length
 */
static int encodeIntResultList(__u8 * Dest, const struct FacForwardingList *IntResultList)
{
	unsigned Index;
	__u8 *p;

	Dest[0] = ASN1_TAG_SET;
	p = &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

	for (Index = 0; Index < IntResultList->NumRecords; ++Index) {
		p += encodeIntResult(p, &IntResultList->List[Index]);
	}			/* end for */

	/* length */
	encodeLen_Long_u8(&Dest[1], p - &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

	return p - Dest;
}				/* end encodeIntResultList() */

/* ******************************************************************* */
/*!
 * \brief Parse the IntResultList argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param IntResultList Forwarding record list storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseIntResultList(struct asn1_parm *pc, u_char * p, u_char * end, struct FacForwardingList *IntResultList)
{
	INIT;

	IntResultList->NumRecords = 0;
	while (p < end) {
		if (IntResultList->NumRecords < sizeof(IntResultList->List) / sizeof(IntResultList->List[0])) {
			XSEQUENCE_1(ParseIntResult, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
				    &IntResultList->List[IntResultList->NumRecords]);
			++IntResultList->NumRecords;
		} else {
			/* Too many records */
			return -1;
		}
	}			/* end while */

	return p - beg;
}				/* end ParseIntResultList() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the ServedUserNumberList type.
 *
 * \param Dest Where to put the encoding
 * \param ServedUserNumberList Served user record list information to encode.
 *
 * \retval length
 */
static int encodeServedUserNumberList(__u8 * Dest, const struct FacServedUserNumberList *ServedUserNumberList)
{
	unsigned Index;
	__u8 *p;

	Dest[0] = ASN1_TAG_SET;
	p = &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

	for (Index = 0; Index < ServedUserNumberList->NumRecords; ++Index) {
		p += encodePartyNumber_Full(p, &ServedUserNumberList->List[Index]);
	}			/* end for */

	/* length */
	encodeLen_Long_u8(&Dest[1], p - &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

	return p - Dest;
}				/* end encodeServedUserNumberList() */

/* ******************************************************************* */
/*!
 * \brief Parse the ServedUserNumberList argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param ServedUserNumberList Served user record list storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseServedUserNumberList(struct asn1_parm *pc, u_char * p, u_char * end, struct FacServedUserNumberList *ServedUserNumberList)
{
	INIT;

	ServedUserNumberList->NumRecords = 0;
	while (p < end) {
		if (ServedUserNumberList->NumRecords < sizeof(ServedUserNumberList->List) / sizeof(ServedUserNumberList->List[0])) {
			XSEQUENCE_1(ParsePartyNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED,
				    &ServedUserNumberList->List[ServedUserNumberList->NumRecords]);
			++ServedUserNumberList->NumRecords;
		} else {
			/* Too many records */
			return -1;
		}
	}			/* end while */

	return p - beg;
}				/* end ParseServedUserNumberList() */

/* ******************************************************************* */
/*!
 * \brief Encode the ActivationDiversion facility ie.
 *
 * \param Dest Where to put the encoding
 * \param ActivationDiversion Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacActivationDiversion(__u8 * Dest, const struct asn1_parm *pc, const struct FacActivationDiversion *ActivationDiversion)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_ActivationDiversion);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeEnum(p, ASN1_TAG_ENUM, ActivationDiversion->Procedure);
		p += encodeEnum(p, ASN1_TAG_ENUM, ActivationDiversion->BasicService);
		p += encodeAddress_Full(p, &ActivationDiversion->ForwardedTo);
		p += encodeServedUserNumber_Full(p, &ActivationDiversion->ServedUser);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_ActivationDiversion);

		/* No arguments */

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacActivationDiversion() */

/* ******************************************************************* */
/*!
 * \brief Parse the ActivationDiversion invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param ActivationDiversion Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseActivationDiversion(struct asn1_parm *pc, u_char * p, u_char * end,
				 struct FacActivationDiversion *ActivationDiversion)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	ActivationDiversion->Procedure = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	ActivationDiversion->BasicService = Value;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &ActivationDiversion->ForwardedTo);
	XSEQUENCE_1(ParseServedUserNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &ActivationDiversion->ServedUser);

	return p - beg;
}				/* end ParseActivationDiversion() */

/* ******************************************************************* */
/*!
 * \brief Encode the DeactivationDiversion facility ie.
 *
 * \param Dest Where to put the encoding
 * \param DeactivationDiversion Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacDeactivationDiversion(__u8 * Dest, const struct asn1_parm *pc, const struct FacDeactivationDiversion *DeactivationDiversion)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_DeactivationDiversion);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeEnum(p, ASN1_TAG_ENUM, DeactivationDiversion->Procedure);
		p += encodeEnum(p, ASN1_TAG_ENUM, DeactivationDiversion->BasicService);
		p += encodeServedUserNumber_Full(p, &DeactivationDiversion->ServedUser);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_DeactivationDiversion);

		/* No arguments */

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacDeactivationDiversion() */

/* ******************************************************************* */
/*!
 * \brief Parse the DeactivationDiversion invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param DeactivationDiversion Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseDeactivationDiversion(struct asn1_parm *pc, u_char * p, u_char * end, struct FacDeactivationDiversion *DeactivationDiversion)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DeactivationDiversion->Procedure = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DeactivationDiversion->BasicService = Value;
	XSEQUENCE_1(ParseServedUserNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &DeactivationDiversion->ServedUser);

	return p - beg;
}				/* end ParseDeactivationDiversion() */

/* ******************************************************************* */
/*!
 * \brief Encode the ActivationStatusNotificationDiv facility ie.
 *
 * \param Dest Where to put the encoding
 * \param ActivationStatusNotificationDiv Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacActivationStatusNotificationDiv(__u8 * Dest, const struct asn1_parm *pc,
					     const struct FacActivationStatusNotificationDiv *ActivationStatusNotificationDiv)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_ActivationStatusNotificationDiv);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, ActivationStatusNotificationDiv->Procedure);
	p += encodeEnum(p, ASN1_TAG_ENUM, ActivationStatusNotificationDiv->BasicService);
	p += encodeAddress_Full(p, &ActivationStatusNotificationDiv->ForwardedTo);
	p += encodeServedUserNumber_Full(p, &ActivationStatusNotificationDiv->ServedUser);

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacActivationStatusNotificationDiv() */

/* ******************************************************************* */
/*!
 * \brief Parse the ActivationStatusNotificationDiv invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param ActivationStatusNotificationDiv Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseActivationStatusNotificationDiv(struct asn1_parm *pc, u_char * p, u_char * end,
					     struct FacActivationStatusNotificationDiv *ActivationStatusNotificationDiv)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	ActivationStatusNotificationDiv->Procedure = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	ActivationStatusNotificationDiv->BasicService = Value;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &ActivationStatusNotificationDiv->ForwardedTo);
	XSEQUENCE_1(ParseServedUserNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &ActivationStatusNotificationDiv->ServedUser);

	return p - beg;
}				/* end ParseActivationStatusNotificationDiv() */

/* ******************************************************************* */
/*!
 * \brief Encode the DeactivationStatusNotificationDiv facility ie.
 *
 * \param Dest Where to put the encoding
 * \param DeactivationStatusNotificationDiv Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacDeactivationStatusNotificationDiv(__u8 * Dest, const struct asn1_parm *pc,
					       const struct FacDeactivationStatusNotificationDiv
					       *DeactivationStatusNotificationDiv)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_DeactivationStatusNotificationDiv);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, DeactivationStatusNotificationDiv->Procedure);
	p += encodeEnum(p, ASN1_TAG_ENUM, DeactivationStatusNotificationDiv->BasicService);
	p += encodeServedUserNumber_Full(p, &DeactivationStatusNotificationDiv->ServedUser);

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacDeactivationStatusNotificationDiv() */

/* ******************************************************************* */
/*!
 * \brief Parse the DeactivationStatusNotificationDiv invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param DeactivationStatusNotificationDiv Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseDeactivationStatusNotificationDiv(struct asn1_parm *pc, u_char * p, u_char * end,
					       struct FacDeactivationStatusNotificationDiv *DeactivationStatusNotificationDiv)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DeactivationStatusNotificationDiv->Procedure = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DeactivationStatusNotificationDiv->BasicService = Value;
	XSEQUENCE_1(ParseServedUserNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &DeactivationStatusNotificationDiv->ServedUser);


	return p - beg;
}				/* end ParseDeactivationStatusNotificationDiv() */

/* ******************************************************************* */
/*!
 * \brief Encode the InterrogationDiversion facility ie.
 *
 * \param Dest Where to put the encoding
 * \param InterrogationDiversion Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacInterrogationDiversion(__u8 * Dest, const const struct asn1_parm *pc, const struct FacInterrogationDiversion *InterrogationDiversion)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_InterrogationDiversion);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeEnum(p, ASN1_TAG_ENUM, InterrogationDiversion->Procedure);
		if (InterrogationDiversion->BasicService) {
			/* Not the DEFAULT value */
			p += encodeEnum(p, ASN1_TAG_ENUM, InterrogationDiversion->BasicService);
		}
		p += encodeServedUserNumber_Full(p, &InterrogationDiversion->ServedUser);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head_Long_u8(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

		p += encodeOperationValue(p, Fac_InterrogationDiversion);

		p += encodeIntResultList(p, &pc->u.retResult.o.InterrogationDiversion);

		/* sequence Length */
		encodeLen_Long_u8(&SeqStart[1], p - &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

		Length = encodeComponent_Length_Long_u8(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacInterrogationDiversion() */

/* ******************************************************************* */
/*!
 * \brief Parse the InterrogationDiversion invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param InterrogationDiversion Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseInterrogationDiversion(struct asn1_parm *pc, u_char * p, u_char * end,
				    struct FacInterrogationDiversion *InterrogationDiversion)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	InterrogationDiversion->Procedure = Value;
	Value = 0;		/* DEFAULT BasicService value (allServices) */
	XSEQUENCE_OPT_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	InterrogationDiversion->BasicService = Value;
	XSEQUENCE_1(ParseServedUserNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &InterrogationDiversion->ServedUser);

	return p - beg;
}				/* end ParseInterrogationDiversion() */

/* ******************************************************************* */
#if 0
/*!
 * \brief Parse the InterrogationDiversion result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param InterrogationDiversion Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseInterrogationDiversion_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				    struct FacForwardingList *InterrogationDiversion)
{
	return ParseIntResultList(pc, p, end, InterrogationDiversion);
}				/* end ParseInterrogationDiversion_RES() */
#endif

/* ******************************************************************* */
/*!
 * \brief Encode the DiversionInformation facility ie.
 *
 * \param Dest Where to put the encoding
 * \param DiversionInformation Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacDiversionInformation(__u8 * Dest, const const struct asn1_parm *pc, const struct FacDiversionInformation *DiversionInformation)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;
	__u8 *TagStart;

	p = encodeComponentInvoke_Head_Long_u8(Dest, pc->u.inv.invokeId, Fac_DiversionInformation);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

	p += encodeEnum(p, ASN1_TAG_ENUM, DiversionInformation->DiversionReason);
	p += encodeEnum(p, ASN1_TAG_ENUM, DiversionInformation->BasicService);
	if (DiversionInformation->ServedUserSubaddress.Length) {
		p += encodePartySubaddress_Full(p, &DiversionInformation->ServedUserSubaddress);
	}
	if (DiversionInformation->CallingAddressPresent) {
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 0;
		p = &TagStart[2];

		p += encodePresentedAddressScreened_Full(p, &DiversionInformation->CallingAddress);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];
	}
	if (DiversionInformation->OriginalCalledPresent) {
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 1;
		p = &TagStart[2];

		p += encodePresentedNumberUnscreened_Full(p, &DiversionInformation->OriginalCalled);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];
	}
	if (DiversionInformation->LastDivertingPresent) {
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 2;
		p = &TagStart[2];

		p += encodePresentedNumberUnscreened_Full(p, &DiversionInformation->LastDiverting);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];
	}
	if (DiversionInformation->LastDivertingReasonPresent) {
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 3;
		p = &TagStart[2];

		p += encodeEnum(p, ASN1_TAG_ENUM, DiversionInformation->LastDivertingReason);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];
	}
	if (DiversionInformation->UserInfo.Length) {
		p += encodeQ931_Div_UserInfo(p, &DiversionInformation->UserInfo);
	}

	/* sequence Length */
	encodeLen_Long_u8(&SeqStart[1], p - &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

	Length = encodeComponent_Length_Long_u8(Dest, p);

	return Length;
}				/* end encodeFacDiversionInformation() */

/* ******************************************************************* */
/*!
 * \brief Parse the DiversionInformation invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param DiversionInformation Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseDiversionInformation(struct asn1_parm *pc, u_char * p, u_char * end,
				  struct FacDiversionInformation *DiversionInformation)
{
	int xtag;
	int xlen;
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DiversionInformation->DiversionReason = Value;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DiversionInformation->BasicService = Value;

	if (p < end && (*p == ASN1_TAG_SEQUENCE || *p == ASN1_TAG_OCTET_STRING)) {
		CallASN1(ret, p, end, ParsePartySubaddress_Full(pc, p, end, &DiversionInformation->ServedUserSubaddress));
	} else {
		DiversionInformation->ServedUserSubaddress.Length = 0;
	}

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 0)) {
		/* Remove EXPLICIT tag */
		CallASN1(ret, p, end, ParseTag(p, end, &xtag));
		CallASN1(ret, p, end, ParseLen(p, end, &xlen));
		CallASN1(ret, p, end, ParsePresentedAddressScreened_Full(pc, p, end, &DiversionInformation->CallingAddress));
		DiversionInformation->CallingAddressPresent = 1;
	} else {
		DiversionInformation->CallingAddressPresent = 0;
	}

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 1)) {
		/* Remove EXPLICIT tag */
		CallASN1(ret, p, end, ParseTag(p, end, &xtag));
		CallASN1(ret, p, end, ParseLen(p, end, &xlen));
		CallASN1(ret, p, end, ParsePresentedNumberUnscreened_Full(pc, p, end, &DiversionInformation->OriginalCalled));
		DiversionInformation->OriginalCalledPresent = 1;
	} else {
		DiversionInformation->OriginalCalledPresent = 0;
	}

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 2)) {
		/* Remove EXPLICIT tag */
		CallASN1(ret, p, end, ParseTag(p, end, &xtag));
		CallASN1(ret, p, end, ParseLen(p, end, &xlen));
		CallASN1(ret, p, end, ParsePresentedNumberUnscreened_Full(pc, p, end, &DiversionInformation->LastDiverting));
		DiversionInformation->LastDivertingPresent = 1;
	} else {
		DiversionInformation->LastDivertingPresent = 0;
	}

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 3)) {
		/* Remove EXPLICIT tag */
		CallASN1(ret, p, end, ParseTag(p, end, &xtag));
		CallASN1(ret, p, end, ParseLen(p, end, &xlen));
		CallASN1(ret, p, end, ParseEnum(pc, p, end, &Value));
		DiversionInformation->LastDivertingReason = Value;
		DiversionInformation->LastDivertingReasonPresent = 1;
	} else {
		DiversionInformation->LastDivertingReasonPresent = 0;
	}

	DiversionInformation->UserInfo.Length = 0;
	XSEQUENCE_OPT_1(ParseQ931_Div_UserInfo, ASN1_TAG_APPLICATION_WIDE, 0, &DiversionInformation->UserInfo);

	return p - beg;
}				/* end ParseDiversionInformation() */

/* ******************************************************************* */
/*!
 * \brief Encode the CallDeflection facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CallDeflection Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCallDeflection(__u8 * Dest, const const struct asn1_parm *pc, const struct FacCallDeflection *CallDeflection)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CallDeflection);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeAddress_Full(p, &CallDeflection->Deflection);
		if (CallDeflection->PresentationAllowedToDivertedToUserPresent) {
			p += encodeBoolean(p, ASN1_TAG_BOOLEAN,
					   CallDeflection->PresentationAllowedToDivertedToUser);
		}

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_CallDeflection);

		/* No arguments */

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCallDeflection() */

/* ******************************************************************* */
/*!
 * \brief Parse the CallDeflection invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CallDeflection Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCallDeflection(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallDeflection *CallDeflection)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CallDeflection->Deflection);

	if (p < end) {
		CallDeflection->PresentationAllowedToDivertedToUserPresent = 1;
		XSEQUENCE_1(ParseBoolean, ASN1_TAG_BOOLEAN, ASN1_NOT_TAGGED, &Value);
		CallDeflection->PresentationAllowedToDivertedToUser = Value;
	} else {
		CallDeflection->PresentationAllowedToDivertedToUserPresent = 0;
	}

	return p - beg;
}				/* end ParseCallDeflection() */

/* ******************************************************************* */
/*!
 * \brief Encode the CallRerouteing facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CallRerouteing Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCallRerouteing(__u8 * Dest, const const struct asn1_parm *pc, const struct FacCallRerouteing *CallRerouteing)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;
	__u8 *TagStart;
	__u8 big = 0;

start:
	switch (pc->comp) {
	case CompInvoke:
		if (big)
			p = encodeComponentInvoke_Head_Long_u8(Dest, pc->u.inv.invokeId, Fac_CallRerouteing);
		else
			p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CallRerouteing);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2 + big];

		p += encodeEnum(p, ASN1_TAG_ENUM, CallRerouteing->ReroutingReason);
		p += encodeAddress_Full(p, &CallRerouteing->CalledAddress);
		p += encodeInt(p, ASN1_TAG_INTEGER, CallRerouteing->ReroutingCounter);
		p += encodeQ931_Div(p, &CallRerouteing->Q931ie);

		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 1;
		p = &TagStart[2];

		p += encodePresentedNumberUnscreened_Full(p, &CallRerouteing->LastRerouting);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];

		if (CallRerouteing->SubscriptionOption) {
			/* Not the DEFAULT value */
			TagStart = p;
			TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 2;
			p = &TagStart[2];

			p += encodeEnum(p, ASN1_TAG_ENUM, CallRerouteing->SubscriptionOption);

			/* Tag Length */
			TagStart[1] = p - &TagStart[2];
		}

		if (CallRerouteing->CallingPartySubaddress.Length) {
			TagStart = p;
			TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 3;
			p = &TagStart[2];

			p += encodePartySubaddress_Full(p, &CallRerouteing->CallingPartySubaddress);

			/* Tag Length */
			TagStart[1] = p - &TagStart[2];
		}

		/* sequence Length */
		if (big) {
			encodeLen_Long_u8(&SeqStart[1], p - &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);
			Length = encodeComponent_Length_Long_u8(Dest, p);
		} else {
			Length = p - &SeqStart[2];
			if (Length > 127) {
				big = 1; /* == ASN1_NUM_OCTETS_LONG_LENGTH_u8 - 1 */
				/* start again with full byte length element */
				goto start;
			}
			SeqStart[1] = Length & 0x7f;
			Length = encodeComponent_Length(Dest, p);
		}
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_CallRerouteing);

		/* No arguments */

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCallRerouteing() */

/* ******************************************************************* */
/*!
 * \brief Parse the CallRerouteing invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CallRerouteing Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCallRerouteing(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallRerouteing *CallRerouteing)
{
	unsigned int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	CallRerouteing->ReroutingReason = Value;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CallRerouteing->CalledAddress);
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &Value);
	CallRerouteing->ReroutingCounter = Value;
	XSEQUENCE_1(ParseQ931_Div, ASN1_TAG_APPLICATION_WIDE, 0, &CallRerouteing->Q931ie);
	XSEQUENCE_1(ParsePresentedNumberUnscreened_Full, ASN1_TAG_CONTEXT_SPECIFIC, ASN1_TAG_EXPLICIT | 1,
		    &CallRerouteing->LastRerouting);
	Value = 0;		/* DEFAULT value noNotification */
	XSEQUENCE_OPT_1(ParseEnum, ASN1_TAG_CONTEXT_SPECIFIC, ASN1_TAG_EXPLICIT | 2, &Value);
	CallRerouteing->SubscriptionOption = Value;
	CallRerouteing->CallingPartySubaddress.Length = 0;
	XSEQUENCE_OPT_1(ParsePartySubaddress_Full, ASN1_TAG_CONTEXT_SPECIFIC, ASN1_TAG_EXPLICIT | 3,
			&CallRerouteing->CallingPartySubaddress);

	return p - beg;
}				/* end ParseCallRerouteing() */

/* ******************************************************************* */
/*!
 * \brief Encode the InterrogateServedUserNumbers facility ie.
 *
 * \param Dest Where to put the encoding
 * \param InterrogateServedUserNumbers Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacInterrogateServedUserNumbers(__u8 * Dest, const const struct asn1_parm *pc, const struct FacServedUserNumberList *InterrogateServedUserNumbers)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_InterrogateServedUserNumbers);

		/* No arguments */

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head_Long_u8(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

		p += encodeOperationValue(p, Fac_InterrogateServedUserNumbers);

		p += encodeServedUserNumberList(p, InterrogateServedUserNumbers);

		/* sequence Length */
		encodeLen_Long_u8(&SeqStart[1], p - &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

		Length = encodeComponent_Length_Long_u8(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacInterrogateServedUserNumbers() */

/* ******************************************************************* */
#if 0
/*!
 * \brief Parse the InterrogateServedUserNumbers result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param InterrogateServedUserNumbers Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseInterrogateServedUserNumbers_RES(struct asn1_parm *pc, u_char * p, u_char * end,
					  struct FacServedUserNumberList *InterrogateServedUserNumbers)
{
	return ParseServedUserNumberList(pc, p, end, InterrogationDiversion);
}				/* end ParseInterrogateServedUserNumbers_RES() */
#endif

/* ******************************************************************* */
/*!
 * \brief Encode the DivertingLegInformation1 facility ie.
 *
 * \param Dest Where to put the encoding
 * \param DivertingLegInformation1 Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacDivertingLegInformation1(__u8 * Dest, const const struct asn1_parm *pc, const struct FacDivertingLegInformation1 *DivertingLegInformation1)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_DivertingLegInformation1);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, DivertingLegInformation1->DiversionReason);
	p += encodeEnum(p, ASN1_TAG_ENUM, DivertingLegInformation1->SubscriptionOption);
	if (DivertingLegInformation1->DivertedToPresent) {
		p += encodePresentedNumberUnscreened_Full(p, &DivertingLegInformation1->DivertedTo);
	}

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacDivertingLegInformation1() */

/* ******************************************************************* */
/*!
 * \brief Parse the DivertingLegInformation1 invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param DivertingLegInformation1 Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseDivertingLegInformation1(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacDivertingLegInformation1 *DivertingLegInformation1)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DivertingLegInformation1->DiversionReason = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DivertingLegInformation1->SubscriptionOption = Value;
	if (p < end) {
		XSEQUENCE_1(ParsePresentedNumberUnscreened_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED,
			    &DivertingLegInformation1->DivertedTo);
		DivertingLegInformation1->DivertedToPresent = 1;
	} else {
		DivertingLegInformation1->DivertedToPresent = 0;
	}

	return p - beg;
}				/* end ParseDivertingLegInformation1() */

/* ******************************************************************* */
/*!
 * \brief Encode the DivertingLegInformation2 facility ie.
 *
 * \param Dest Where to put the encoding
 * \param DivertingLegInformation2 Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacDivertingLegInformation2(__u8 * Dest, const const struct asn1_parm *pc, const struct FacDivertingLegInformation2 *DivertingLegInformation2)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;
	__u8 *TagStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_DivertingLegInformation2);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeInt(p, ASN1_TAG_INTEGER, DivertingLegInformation2->DiversionCounter);
	p += encodeEnum(p, ASN1_TAG_ENUM, DivertingLegInformation2->DiversionReason);

	if (DivertingLegInformation2->DivertingPresent) {
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 1;
		p = &TagStart[2];

		p += encodePresentedNumberUnscreened_Full(p, &DivertingLegInformation2->Diverting);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];
	}

	if (DivertingLegInformation2->OriginalCalledPresent) {
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 2;
		p = &TagStart[2];

		p += encodePresentedNumberUnscreened_Full(p, &DivertingLegInformation2->OriginalCalled);

		/* Tag Length */
		TagStart[1] = p - &TagStart[2];
	}

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacDivertingLegInformation2() */

/* ******************************************************************* */
/*!
 * \brief Parse the DivertingLegInformation2 invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param DivertingLegInformation2 Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseDivertingLegInformation2(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacDivertingLegInformation2 *DivertingLegInformation2)
{
	int xtag;
	int xlen;
	unsigned int Value;
	INIT;

	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &Value);
	DivertingLegInformation2->DiversionCounter = Value;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	DivertingLegInformation2->DiversionReason = Value;

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 1)) {
		DivertingLegInformation2->DivertingPresent = 1;
		CallASN1(ret, p, end, ParseTag(p, end, &xtag));
		CallASN1(ret, p, end, ParseLen(p, end, &xlen));
		CallASN1(ret, p, end, ParsePresentedNumberUnscreened_Full(pc, p, end, &DivertingLegInformation2->Diverting));
	} else {
		DivertingLegInformation2->DivertingPresent = 0;
	}

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 2)) {
		DivertingLegInformation2->OriginalCalledPresent = 1;
		CallASN1(ret, p, end, ParseTag(p, end, &xtag));
		CallASN1(ret, p, end, ParseLen(p, end, &xlen));
		CallASN1(ret, p, end, ParsePresentedNumberUnscreened_Full(pc, p, end, &DivertingLegInformation2->OriginalCalled));
	} else {
		DivertingLegInformation2->OriginalCalledPresent = 0;
	}

	return p - beg;
}				/* end ParseDivertingLegInformation2() */

/* ******************************************************************* */
/*!
 * \brief Encode the DivertingLegInformation3 facility ie.
 *
 * \param Dest Where to put the encoding
 * \param DivertingLegInformation3 Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacDivertingLegInformation3(__u8 * Dest, const const struct asn1_parm *pc, const struct FacDivertingLegInformation3 *DivertingLegInformation3)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_DivertingLegInformation3);

	p += encodeBoolean(p, ASN1_TAG_BOOLEAN, DivertingLegInformation3->PresentationAllowedIndicator);

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacDivertingLegInformation3() */

/* ******************************************************************* */
/*!
 * \brief Parse the DivertingLegInformation3 invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param DivertingLegInformation3 Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseDivertingLegInformation3(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacDivertingLegInformation3 *DivertingLegInformation3)
{
	int Value;
	int ret;
	u_char *beg;

	beg = p;

	XSEQUENCE_1(ParseBoolean, ASN1_TAG_BOOLEAN, ASN1_NOT_TAGGED, &Value);
	DivertingLegInformation3->PresentationAllowedIndicator = Value;

	return p - beg;
}				/* end ParseDivertingLegInformation3() */

/* ------------------------------------------------------------------- */
/* end asn1_diversion.c */
