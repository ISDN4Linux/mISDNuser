/*
 *
 * CCBS Supplementary Services ETS 300 359-1
 * CCNR Supplementary Services ETS 301 065-1
 *
 * CCBS/CCNR Facility ie encode/decode
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
#include "ccbs.h"
#include <string.h>

/* ------------------------------------------------------------------- */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the Q.931 ie values used for CCBS/CCNR.
 *
 * \param Dest Where to put the encoding
 * \param Q931ie Q931 ie information to encode.
 *
 * \retval length
 */
static int encodeQ931ie_CCBS(__u8 * Dest, const struct Q931_Bc_Hlc_Llc *Q931ie)
{
	__u8 *p;

	/* [APPLICATION 0] IMPLICIT OCTET STRING */
	Dest[0] = ASN1_TAG_APPLICATION_WIDE | 0;
	p = &Dest[2];

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

	/* length */
	Dest[1] = p - &Dest[2];

	return p - Dest;
}				/* end encodeQ931ie_CCBS() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the Q.931 argument contents for CCBS/CCNR.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Q931ie Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseQ931ie_CCBS(struct asn1_parm *pc, u_char * p, u_char * end, struct Q931_Bc_Hlc_Llc *Q931ie)
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
		default:
			/* Unknown Q.931 ie in CCBS message */
			break;
		}		/* end switch */
		p += Length;
	} while (p < end);

	return p - beg;
}				/* end ParseQ931ie_CCBS() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the array of call information details type.
 *
 * \param Dest Where to put the encoding
 * \param CallDetails Array of call detail information to encode.
 *
 * \retval length
 */
static int encodeCallInformation(__u8 * Dest, const struct FacCallInformation *CallInformation)
{
	__u8 *p;

	Dest[0] = ASN1_TAG_SEQUENCE;
	p = &Dest[2];

	p += encodeAddress_Full(p, &CallInformation->AddressOfB);
	p += encodeQ931ie_CCBS(p, &CallInformation->Q931ie);
	p += encodeInt(p, ASN1_TAG_INTEGER, CallInformation->CCBSReference);
	if (CallInformation->SubaddressOfA.Length) {
		p += encodePartySubaddress_Full(p, &CallInformation->SubaddressOfA);
	}

	/* length */
	Dest[1] = p - &Dest[2];

	return p - Dest;
}				/* end encodeCallInformation() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the CallInformation argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CallInformation Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseCallInformation(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallInformation *CallInformation)
{
	unsigned int CCBSReference;
	INIT;

	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CallInformation->AddressOfB);
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &CallInformation->Q931ie);
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CallInformation->CCBSReference = CCBSReference;
	CallInformation->SubaddressOfA.Length = 0;	/* Assume subaddress not present */
	if (p < end) {
		/* The optional subaddress must be present since there is something left. */
		XSEQUENCE_1(ParsePartySubaddress_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &CallInformation->SubaddressOfA);
	} else {
		CallInformation->SubaddressOfA.Length = 0;	/* Subaddress not present */
	}

	return p - beg;
}				/* end ParseCallInformation() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the array of call information details type.
 *
 * \param Dest Where to put the encoding
 * \param NumRecords How many records to encode.
 * \param CallDetails Array of call detail information to encode.
 *
 * \retval length
 */
static int encodeCallDetails(__u8 * Dest, unsigned NumRecords, const struct FacCallInformation CallDetails[])
{
	unsigned Index;
	__u8 *p;

	Dest[0] = ASN1_TAG_SEQUENCE;
	p = &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

	for (Index = 0; Index < NumRecords; ++Index) {
		p += encodeCallInformation(p, &CallDetails[Index]);
	}			/* end for */

	/* length */
	encodeLen_Long_u8(&Dest[1], p - &Dest[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

	return p - Dest;
}				/* end encodeCallDetails() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the array of call information details argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSInterrogate Array of call information details storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseCallDetails(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSInterrogate_RES *CCBSInterrogate)
{
	INIT;

	while (p < end) {
		if (CCBSInterrogate->NumRecords < sizeof(CCBSInterrogate->CallDetails) / sizeof(CCBSInterrogate->CallDetails[0])) {
			XSEQUENCE_1(ParseCallInformation, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
				    &CCBSInterrogate->CallDetails[CCBSInterrogate->NumRecords]);
			++CCBSInterrogate->NumRecords;
		} else {
			/* Too many records */
			return -1;
		}
	}			/* end while */

	return p - beg;
}				/* end ParseCallDetails() */

/* ******************************************************************* */
/*!
 * \brief Encode the StatusRequest facility ie.
 *
 * \param Dest Where to put the encoding
 * \param StatusRequest Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacStatusRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacStatusRequest *StatusRequest)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_StatusRequest);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;

		p = &SeqStart[2];
		p += encodeEnum(p, ASN1_TAG_ENUM, StatusRequest->CompatibilityMode);
		p += encodeQ931ie_CCBS(p, &StatusRequest->Q931ie);

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

		p += encodeOperationValue(p, Fac_StatusRequest);

		p += encodeEnum(p, ASN1_TAG_ENUM, pc->u.retResult.o.StatusRequest.Status);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacStatusRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the StatusRequest invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param StatusRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseStatusRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacStatusRequest *StatusRequest)
{
	int CompatibilityMode;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &CompatibilityMode);
	StatusRequest->CompatibilityMode = CompatibilityMode;
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &StatusRequest->Q931ie);

	return p - beg;
}				/* end ParseStatusRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the StatusRequest result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param StatusRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseStatusRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacStatusRequest_RES *StatusRequest)
{
	int Status;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Status);
	StatusRequest->Status = Status;

	return p - beg;
}				/* end ParseStatusRequest_RES() */

/* ******************************************************************* */
/*!
 * \brief Encode the CallInfoRetain facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CallInfoRetain Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCallInfoRetain(__u8 * Dest, const struct asn1_parm *pc, const struct FacCallInfoRetain *CallInfoRetain)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CallInfoRetain);
	p += encodeInt(p, ASN1_TAG_INTEGER, CallInfoRetain->CallLinkageID);
	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCallInfoRetain() */

/* ******************************************************************* */
/*!
 * \brief Parse the CallInfoRetain invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CallInfoRetain Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCallInfoRetain(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallInfoRetain *CallInfoRetain)
{
	unsigned int CallLinkageID;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CallLinkageID);
	CallInfoRetain->CallLinkageID = CallLinkageID;

	return p - beg;
}				/* end ParseCallInfoRetain() */

/* ******************************************************************* */
/*!
 * \brief Encode the EraseCallLinkageID facility ie.
 *
 * \param Dest Where to put the encoding
 * \param EraseCallLinkageID Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacEraseCallLinkageID(__u8 * Dest, const struct asn1_parm *pc, const struct FacEraseCallLinkageID *EraseCallLinkageID)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_EraseCallLinkageID);
	p += encodeInt(p, ASN1_TAG_INTEGER, EraseCallLinkageID->CallLinkageID);
	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacEraseCallLinkageID() */

/* ******************************************************************* */
/*!
 * \brief Parse the EraseCallLinkageID invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param EraseCallLinkageID Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseEraseCallLinkageID(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEraseCallLinkageID *EraseCallLinkageID)
{
	unsigned int CallLinkageID;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CallLinkageID);
	EraseCallLinkageID->CallLinkageID = CallLinkageID;

	return p - beg;
}				/* end ParseEraseCallLinkageID() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSDeactivate facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSDeactivate Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSDeactivate(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSDeactivate *CCBSDeactivate)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSDeactivate);
		p += encodeInt(p, ASN1_TAG_INTEGER, CCBSDeactivate->CCBSReference);
		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_CCBSDeactivate);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCCBSDeactivate() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSDeactivate invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSDeactivate Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSDeactivate(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSDeactivate *CCBSDeactivate)
{
	unsigned int CCBSReference;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSDeactivate->CCBSReference = CCBSReference;

	return p - beg;
}				/* end ParseCCBSDeactivate() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSErase facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSErase Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSErase(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSErase *CCBSErase)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSErase);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, CCBSErase->RecallMode);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSErase->CCBSReference);
	p += encodeAddress_Full(p, &CCBSErase->AddressOfB);
	p += encodeQ931ie_CCBS(p, &CCBSErase->Q931ie);
	p += encodeEnum(p, ASN1_TAG_ENUM, CCBSErase->Reason);

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSErase() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSErase invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSErase Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSErase(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSErase *CCBSErase)
{
	int RecallMode;
	unsigned int CCBSReference;
	int EraseReason;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &RecallMode);
	CCBSErase->RecallMode = RecallMode;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSErase->CCBSReference = CCBSReference;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CCBSErase->AddressOfB);
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &CCBSErase->Q931ie);
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &EraseReason);
	CCBSErase->Reason = EraseReason;

	return p - beg;
}				/* end ParseCCBSErase() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSRemoteUserFree facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSRemoteUserFree Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSRemoteUserFree(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRemoteUserFree *CCBSRemoteUserFree)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSRemoteUserFree);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, CCBSRemoteUserFree->RecallMode);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSRemoteUserFree->CCBSReference);
	p += encodeAddress_Full(p, &CCBSRemoteUserFree->AddressOfB);
	p += encodeQ931ie_CCBS(p, &CCBSRemoteUserFree->Q931ie);

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSRemoteUserFree() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSRemoteUserFree invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSRemoteUserFree Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSRemoteUserFree(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRemoteUserFree *CCBSRemoteUserFree)
{
	int RecallMode;
	unsigned int CCBSReference;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &RecallMode);
	CCBSRemoteUserFree->RecallMode = RecallMode;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSRemoteUserFree->CCBSReference = CCBSReference;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CCBSRemoteUserFree->AddressOfB);
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &CCBSRemoteUserFree->Q931ie);

	return p - beg;
}				/* end ParseCCBSRemoteUserFree() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSCall facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSCall Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSCall(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSCall *CCBSCall)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSCall);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSCall->CCBSReference);
	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSCall() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSCall invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSCall Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSCall(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSCall *CCBSCall)
{
	unsigned int CCBSReference;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSCall->CCBSReference = CCBSReference;

	return p - beg;
}				/* end ParseCCBSCall() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSBFree facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSBFree Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSBFree(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSBFree *CCBSBFree)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSBFree);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, CCBSBFree->RecallMode);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSBFree->CCBSReference);
	p += encodeAddress_Full(p, &CCBSBFree->AddressOfB);
	p += encodeQ931ie_CCBS(p, &CCBSBFree->Q931ie);

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSBFree() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSBFree invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSBFree Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSBFree(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSBFree *CCBSBFree)
{
	int RecallMode;
	unsigned int CCBSReference;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &RecallMode);
	CCBSBFree->RecallMode = RecallMode;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSBFree->CCBSReference = CCBSReference;
	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CCBSBFree->AddressOfB);
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &CCBSBFree->Q931ie);

	return p - beg;
}				/* end ParseCCBSBFree() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSStopAlerting facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSStopAlerting Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSStopAlerting(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSStopAlerting *CCBSStopAlerting)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSStopAlerting);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSStopAlerting->CCBSReference);
	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSStopAlerting() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSStopAlerting invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSStopAlerting Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSStopAlerting(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSStopAlerting *CCBSStopAlerting)
{
	unsigned int CCBSReference;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSStopAlerting->CCBSReference = CCBSReference;

	return p - beg;
}				/* end ParseCCBSStopAlerting() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSStatusRequest facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSStatusRequest Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSStatusRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSStatusRequest *CCBSStatusRequest)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_CCBSStatusRequest);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeEnum(p, ASN1_TAG_ENUM, CCBSStatusRequest->RecallMode);
		p += encodeInt(p, ASN1_TAG_INTEGER, CCBSStatusRequest->CCBSReference);
		p += encodeQ931ie_CCBS(p, &CCBSStatusRequest->Q931ie);

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

		p += encodeOperationValue(p, Fac_CCBSStatusRequest);

		p += encodeBoolean(p, ASN1_TAG_BOOLEAN, pc->u.retResult.o.CCBSStatusRequest.Free);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCCBSStatusRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSStatusRequest invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSStatusRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSStatusRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSStatusRequest *CCBSStatusRequest)
{
	int RecallMode;
	unsigned int CCBSReference;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &RecallMode);
	CCBSStatusRequest->RecallMode = RecallMode;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSStatusRequest->CCBSReference = CCBSReference;
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &CCBSStatusRequest->Q931ie);

	return p - beg;
}				/* end ParseCCBSStatusRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSStatusRequest result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCBSStatusRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSStatusRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSStatusRequest_RES *CCBSStatusRequest)
{
	int Free;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseBoolean, ASN1_TAG_BOOLEAN, ASN1_NOT_TAGGED, &Free);
	CCBSStatusRequest->Free = Free;

	return p - beg;
}				/* end ParseCCBSStatusRequest_RES() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the CCBS/CCNR-Request facility invoke ie
 *
 * \param Dest Where to put the encoding
 * \param CCBSRequest Information needed to encode in ie.
 * \param MsgType Which facility type to generate
 *
 * \retval length on success.
 * \retval -1 on error.
 */
static int encodeFacCCBSRequest_Invoke(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRequest *CCBSRequest, enum Operation MsgType)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, MsgType);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSRequest->CallLinkageID);
	Length = encodeComponent_Length(Dest, p);
	return Length;
}				/* end encodeFacCCBSRequest_Invoke() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the CCBS/CCNR-Request facility result ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSRequest Information needed to encode in ie.
 * \param MsgType Which facility type to generate
 *
 * \retval length on success.
 * \retval -1 on error.
 */
static int encodeFacCCBSRequest_Result(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRequest_RES *CCBSRequest, enum Operation MsgType)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;
	__u8 *ResultSeqStart;

	p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
	p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeOperationValue(p, MsgType);

	ResultSeqStart = p;
	ResultSeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &ResultSeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, CCBSRequest->RecallMode);
	p += encodeInt(p, ASN1_TAG_INTEGER, CCBSRequest->CCBSReference);

	/* sequence Length */
	ResultSeqStart[1] = p - &ResultSeqStart[2];

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSRequest_Result() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSRequest facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSRequest Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRequest *CCBSRequest)
{
	int Length;

	switch (pc->comp) {
	case CompInvoke:
		Length = encodeFacCCBSRequest_Invoke(Dest, pc, CCBSRequest, Fac_CCBSRequest);
		break;
	case CompReturnResult:
		Length = encodeFacCCBSRequest_Result(Dest, pc, &pc->u.retResult.o.CCBSRequest, Fac_CCBSRequest);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCCBSRequest() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCNRRequest facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCNRRequest Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCNRRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRequest *CCNRRequest)
{
	int Length;

	switch (pc->comp) {
	case CompInvoke:
		Length = encodeFacCCBSRequest_Invoke(Dest, pc, CCNRRequest, Fac_CCNRRequest);
		break;
	case CompReturnResult:
		Length = encodeFacCCBSRequest_Result(Dest, pc, &pc->u.retResult.o.CCNRRequest, Fac_CCNRRequest);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCCNRRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSRequest invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest *CCBSRequest)
{
	unsigned int CallLinkageID;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CallLinkageID);
	CCBSRequest->CallLinkageID = CallLinkageID;

	return p - beg;
}				/* end ParseCCBSRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCNRRequest invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCNRRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCNRRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest *CCNRRequest)
{
	return ParseCCBSRequest(pc, p, end, CCNRRequest);
}				/* end ParseCCNRRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSRequest result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCBSRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_RES *CCBSRequest)
{
	int RecallMode;
	unsigned int CCBSReference;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &RecallMode);
	CCBSRequest->RecallMode = RecallMode;
	XSEQUENCE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CCBSReference);
	CCBSRequest->CCBSReference = CCBSReference;

	return p - beg;
}				/* end ParseCCBSRequest_RES() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCNRRequest result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCNRRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCNRRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_RES *CCNRRequest)
{
	return ParseCCBSRequest_RES(pc, p, end, CCNRRequest);
}				/* end ParseCCNRRequest_RES() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the CCBS/CCNR-Interrogate facility invoke ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSInterrogate Information needed to encode in ie.
 * \param MsgType Which facility type to generate
 *
 * \retval length on success.
 * \retval -1 on error.
 */
static int encodeFacCCBSInterrogate_Invoke(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSInterrogate *CCBSInterrogate, enum Operation MsgType)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;
	__u8 *ResultSeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, MsgType);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	if (CCBSInterrogate->CCBSReferencePresent) {
		p += encodeInt(p, ASN1_TAG_INTEGER, CCBSInterrogate->CCBSReference);
	}
	if (CCBSInterrogate->AParty.LengthOfNumber) {
		p += encodePartyNumber_Full(p, &CCBSInterrogate->AParty);
	}

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBSInterrogate_Backend() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the CCBS/CCNR-Interrogate facility result ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSInterrogate Information needed to encode in ie.
 * \param MsgType Which facility type to generate
 *
 * \retval length on success.
 * \retval -1 on error.
 */
static int encodeFacCCBSInterrogate_Result(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSInterrogate_RES *CCBSInterrogate, enum Operation MsgType)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;
	__u8 *ResultSeqStart;

	p = encodeComponent_Head_Long_u8(Dest, asn1ComponentTag_Result);
	p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

	p += encodeOperationValue(p, MsgType);

	ResultSeqStart = p;
	ResultSeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &ResultSeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8];

	p += encodeEnum(p, ASN1_TAG_ENUM, CCBSInterrogate->RecallMode);
	if (CCBSInterrogate->NumRecords) {
		p += encodeCallDetails(p, CCBSInterrogate->NumRecords, CCBSInterrogate->CallDetails);
	}

	/* sequence Length */
	encodeLen_Long_u8(&ResultSeqStart[1], p - &ResultSeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

	/* sequence Length */
	encodeLen_Long_u8(&SeqStart[1], p - &SeqStart[1 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);

	Length = encodeComponent_Length_Long_u8(Dest, p);

	return Length;
}				/* end encodeFacCCBSInterrogate_Backend() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBSInterrogate facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBSInterrogate Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBSInterrogate(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSInterrogate *CCBSInterrogate)
{
	int Length;

	switch (pc->comp) {
	case CompInvoke:
		Length = encodeFacCCBSInterrogate_Invoke(Dest, pc, CCBSInterrogate, Fac_CCBSInterrogate);
		break;
	case CompReturnResult:
		Length = encodeFacCCBSInterrogate_Result(Dest, pc, &pc->u.retResult.o.CCBSInterrogate, Fac_CCBSInterrogate);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */
	return Length;
}				/* end encodeFacCCBSInterrogate() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCNRInterrogate facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCNRInterrogate Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCNRInterrogate(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSInterrogate *CCNRInterrogate)
{
	int Length;

	switch (pc->comp) {
	case CompInvoke:
		Length = encodeFacCCBSInterrogate_Invoke(Dest, pc, CCNRInterrogate, Fac_CCNRInterrogate);
		break;
	case CompReturnResult:
		Length = encodeFacCCBSInterrogate_Result(Dest, pc, &pc->u.retResult.o.CCNRInterrogate, Fac_CCNRInterrogate);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */
	return Length;
}				/* end encodeFacCCNRInterrogate() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSInterrogate invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBSInterrogate Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSInterrogate(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSInterrogate *CCBSInterrogate)
{
	unsigned int CCBSReference;
	INIT;

	if (p < end && *p == ASN1_TAG_INTEGER) {
		CallASN1(ret, p, end, ParseUnsignedInteger(pc, p, end, &CCBSReference));
		CCBSInterrogate->CCBSReference = CCBSReference;
		CCBSInterrogate->CCBSReferencePresent = 1;
	} else {
		CCBSInterrogate->CCBSReferencePresent = 0;
		CCBSInterrogate->CCBSReference = 0;
	}
	CCBSInterrogate->AParty.LengthOfNumber = 0;	/* Assume A party number not present */
	XSEQUENCE_OPT_1(ParsePartyNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &CCBSInterrogate->AParty);

	return p - beg;
}				/* end ParseCCBSInterrogate() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCNRInterrogate invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCNRInterrogate Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCNRInterrogate(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSInterrogate *CCNRInterrogate)
{
	return ParseCCBSInterrogate(pc, p, end, CCNRInterrogate);
}				/* end ParseCCNRInterrogate() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBSInterrogate result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCBSInterrogate Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBSInterrogate_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSInterrogate_RES *CCBSInterrogate)
{
	int RecallMode;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &RecallMode);
	CCBSInterrogate->RecallMode = RecallMode;
	CCBSInterrogate->NumRecords = 0;
	XSEQUENCE_OPT_1(ParseCallDetails, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, CCBSInterrogate);

	return p - beg;
}				/* end ParseCCBSInterrogate_RES() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCNRInterrogate result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCNRInterrogate Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCNRInterrogate_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSInterrogate_RES *CCNRInterrogate)
{
	return ParseCCBSInterrogate_RES(pc, p, end, CCNRInterrogate);
}				/* end ParseCCNRInterrogate_RES() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the CCBS_T_xxx event facility ie.
 *
 * \param Dest Where to put the encoding
 * \param InvokeID
 * \param OperationValue CCBS_T_xxx message type
 *
 * \retval length on success.
 * \retval -1 on error.
 */
static int encodeFacCCBS_T_Event(__u8 * Dest, int InvokeID, enum Operation OperationValue)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, InvokeID, OperationValue);

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacCCBS_T_Event() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBS_T_Call facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_Call Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBS_T_Call(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	return encodeFacCCBS_T_Event(Dest, pc->u.inv.invokeId, Fac_CCBS_T_Call);
}				/* end encodeFacCCBS_T_Call() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBS_T_Suspend facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_Suspend Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBS_T_Suspend(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	return encodeFacCCBS_T_Event(Dest, pc->u.inv.invokeId, Fac_CCBS_T_Suspend);
}				/* end encodeFacCCBS_T_Suspend() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBS_T_Resume facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_Resume Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBS_T_Resume(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	return encodeFacCCBS_T_Event(Dest, pc->u.inv.invokeId, Fac_CCBS_T_Resume);
}				/* end encodeFacCCBS_T_Resume() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBS_T_RemoteUserFree facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_RemoteUserFree Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBS_T_RemoteUserFree(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	return encodeFacCCBS_T_Event(Dest, pc->u.inv.invokeId, Fac_CCBS_T_RemoteUserFree);
}				/* end encodeFacCCBS_T_RemoteUserFree() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBS_T_Available facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_Available Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBS_T_Available(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	return encodeFacCCBS_T_Event(Dest, pc->u.inv.invokeId, Fac_CCBS_T_Available);
}				/* end encodeFacCCBS_T_Available() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the CCBS-T/CCNR-T-Request facility ie backend.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_Request Information needed to encode in ie.
 * \param MsgType Which facility type to generate
 *
 * \retval length on success.
 * \retval -1 on error.
 */
static int encodeFacCCBS_T_Request_Backend(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBS_T_Request *CCBS_T_Request, __u8 RetentionSupported, enum Operation MsgType)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, MsgType);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeAddress_Full(p, &CCBS_T_Request->Destination);
		p += encodeQ931ie_CCBS(p, &CCBS_T_Request->Q931ie);
		if (CCBS_T_Request->RetentionSupported) {
			/* Not the DEFAULT value */
			p += encodeBoolean(p, ASN1_TAG_CONTEXT_SPECIFIC | 1, CCBS_T_Request->RetentionSupported);
		}
		if (CCBS_T_Request->PresentationAllowedIndicatorPresent) {
			p += encodeBoolean(p, ASN1_TAG_CONTEXT_SPECIFIC | 2,
					   CCBS_T_Request->PresentationAllowedIndicator);
		}
		if (CCBS_T_Request->Originating.Party.LengthOfNumber) {
			p += encodeAddress_Full(p, &CCBS_T_Request->Originating);
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

		p += encodeOperationValue(p, MsgType);

		p += encodeBoolean(p, ASN1_TAG_BOOLEAN, RetentionSupported);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacCCBS_T_Request_Backend() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCBS_T_Request facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCBS_T_Request Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCBS_T_Request(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBS_T_Request *CCBS_T_Request)
{
	return encodeFacCCBS_T_Request_Backend(Dest, pc, CCBS_T_Request,
		pc->u.retResult.o.CCBS_T_Request.RetentionSupported, Fac_CCBS_T_Request);
}				/* end encodeFacCCBS_T_Request() */

/* ******************************************************************* */
/*!
 * \brief Encode the CCNR_T_Request facility ie.
 *
 * \param Dest Where to put the encoding
 * \param CCNR_T_Request Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacCCNR_T_Request(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBS_T_Request *CCNR_T_Request)
{
	return encodeFacCCBS_T_Request_Backend(Dest, pc, CCNR_T_Request,
		pc->u.retResult.o.CCNR_T_Request.RetentionSupported, Fac_CCNR_T_Request);
}				/* end encodeFacCCNR_T_Request() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBS_T_Request invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCBS_T_Request Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBS_T_Request(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request *CCBS_T_Request)
{
	int Value;
	INIT;

	XSEQUENCE_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CCBS_T_Request->Destination);
	XSEQUENCE_1(ParseQ931ie_CCBS, ASN1_TAG_APPLICATION_WIDE, 0, &CCBS_T_Request->Q931ie);

	Value = 0;		/* DEFAULT RetentionSupported value (FALSE) */
	XSEQUENCE_OPT_1(ParseBoolean, ASN1_TAG_CONTEXT_SPECIFIC, 1, &Value);
	CCBS_T_Request->RetentionSupported = Value;

	if (p < end && *p == (ASN1_TAG_CONTEXT_SPECIFIC | 2)) {
		CallASN1(ret, p, end, ParseBoolean(pc, p, end, &Value));
		CCBS_T_Request->PresentationAllowedIndicator = Value;
		CCBS_T_Request->PresentationAllowedIndicatorPresent = 1;
	} else {
		CCBS_T_Request->PresentationAllowedIndicator = 0;
		CCBS_T_Request->PresentationAllowedIndicatorPresent = 0;
	}

	CCBS_T_Request->Originating.Party.LengthOfNumber = 0;	/* Assume Originating party number not present */
	XSEQUENCE_OPT_1(ParseAddress_Full, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &CCBS_T_Request->Originating);

	return p - beg;
}				/* end ParseCCBS_T_Request() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCNR_T_Request invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param CCNR_T_Request Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCNR_T_Request(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request *CCNR_T_Request)
{
	return ParseCCBS_T_Request(pc, p, end, CCNR_T_Request);
}				/* end ParseCCNR_T_Request() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCBS_T_Request result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCBS_T_Request Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCBS_T_Request_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_RES *CCBS_T_Request)
{
	int RetentionSupported;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseBoolean, ASN1_TAG_BOOLEAN, ASN1_NOT_TAGGED, &RetentionSupported);
	CCBS_T_Request->RetentionSupported = RetentionSupported;

	return p - beg;
}				/* end ParseCCBS_T_Request_RES() */

/* ******************************************************************* */
/*!
 * \brief Parse the CCNR_T_Request result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param CCNR_T_Request Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseCCNR_T_Request_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_RES *CCNR_T_Request)
{
	return ParseCCBS_T_Request_RES(pc, p, end, CCNR_T_Request);
}				/* end ParseCCNR_T_Request_RES() */

/* ------------------------------------------------------------------- */
/* end asn1_ccbs.c */
