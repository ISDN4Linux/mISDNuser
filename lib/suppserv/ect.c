/*
 *
 * Explicit Call Transfer (ECT) Supplementary Services ETS 300 369-1
 *
 * ECT Facility ie encode/decode
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
#include "ect.h"

/* ------------------------------------------------------------------- */

/* ******************************************************************* */
/*!
 * \brief Encode the EctExecute facility ie.
 *
 * \param Dest Where to put the encoding
 * \param EctExecute Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacEctExecute(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_EctExecute);

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacEctExecute() */

/* ******************************************************************* */
/*!
 * \brief Encode the ExplicitEctExecute facility ie.
 *
 * \param Dest Where to put the encoding
 * \param ExplicitEctExecute Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacExplicitEctExecute(__u8 * Dest, const struct asn1_parm *pc, const struct FacExplicitEctExecute *ExplicitEctExecute)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_ExplicitEctExecute);

	p += encodeInt(p, ASN1_TAG_INTEGER, ExplicitEctExecute->LinkID);

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacExplicitEctExecute() */

/* ******************************************************************* */
/*!
 * \brief Parse the ExplicitEctExecute invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param ExplicitEctExecute Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseExplicitEctExecute(struct asn1_parm *pc, u_char * p, u_char * end, struct FacExplicitEctExecute *ExplicitEctExecute)
{
	int LinkID;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &LinkID);
	ExplicitEctExecute->LinkID = LinkID;

	return p - beg;
}				/* end ParseExplicitEctExecute() */

/* ******************************************************************* */
/*!
 * \brief Encode the RequestSubaddress facility ie.
 *
 * \param Dest Where to put the encoding
 * \param RequestSubaddress Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacRequestSubaddress(__u8 * Dest, const struct asn1_parm *pc, const void *val)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_RequestSubaddress);

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacRequestSubaddress() */

/* ******************************************************************* */
/*!
 * \brief Encode the SubaddressTransfer facility ie.
 *
 * \param Dest Where to put the encoding
 * \param SubaddressTransfer Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacSubaddressTransfer(__u8 * Dest, const struct asn1_parm *pc, const struct FacSubaddressTransfer *SubaddressTransfer)
{
	int Length;
	__u8 *p;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_SubaddressTransfer);

	p += encodePartySubaddress_Full(p, &SubaddressTransfer->Subaddress);

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacSubaddressTransfer() */

/* ******************************************************************* */
/*!
 * \brief Parse the SubaddressTransfer invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param SubaddressTransfer Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseSubaddressTransfer(struct asn1_parm *pc, u_char * p, u_char * end, struct FacSubaddressTransfer *SubaddressTransfer)
{
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParsePartySubaddress_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &SubaddressTransfer->Subaddress);

	return p - beg;
}				/* end ParseSubaddressTransfer() */

/* ******************************************************************* */
/*!
 * \brief Encode the EctLinkIdRequest facility ie.
 *
 * \param Dest Where to put the encoding
 * \param EctLinkIdRequest Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacEctLinkIdRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacEctLinkIdRequest_RES *EctLinkIdRequest)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_EctLinkIdRequest);

		/* No arguments */

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_EctLinkIdRequest);

		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.o.EctLinkIdRequest.LinkID);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacEctLinkIdRequest() */

/* ******************************************************************* */
/*!
 * \brief Parse the EctLinkIdRequest result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param EctLinkIdRequest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseEctLinkIdRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLinkIdRequest_RES *EctLinkIdRequest)
{
	int LinkID;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &LinkID);
	EctLinkIdRequest->LinkID = LinkID;

	return p - beg;
}				/* end ParseEctLinkIdRequest_RES() */

/* ******************************************************************* */
/*!
 * \brief Encode the EctInform facility ie.
 *
 * \param Dest Where to put the encoding
 * \param EctInform Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacEctInform(__u8 * Dest, const struct asn1_parm *pc, const struct FacEctInform *EctInform)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_EctInform);

	SeqStart = p;
	SeqStart[0] = ASN1_TAG_SEQUENCE;
	p = &SeqStart[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, EctInform->Status);
	if (EctInform->RedirectionPresent) {
		p += encodePresentedNumberUnscreened_Full(p, &EctInform->Redirection);
	}

	/* sequence Length */
	SeqStart[1] = p - &SeqStart[2];

	Length = encodeComponent_Length(Dest, p);

	return Length;
}				/* end encodeFacEctInform() */

/* ******************************************************************* */
/*!
 * \brief Parse the EctInform invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param EctInform Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseEctInform(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctInform *EctInform)
{
	unsigned int Status;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Status);
	EctInform->Status = Status;
	if (p < end) {
		XSEQUENCE_1(ParsePresentedNumberUnscreened_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &EctInform->Redirection);
		EctInform->RedirectionPresent = 1;
	} else {
		EctInform->RedirectionPresent = 0;
	}

	return p - beg;
}				/* end ParseEctInform() */

/* ******************************************************************* */
/*!
 * \brief Encode the EctLoopTest facility ie.
 *
 * \param Dest Where to put the encoding
 * \param EctLoopTest Information needed to encode in ie.
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacEctLoopTest(__u8 * Dest, const struct asn1_parm *pc, const struct FacEctLoopTest *EctLoopTest)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_EctLoopTest);

		p += encodeInt(p, ASN1_TAG_INTEGER, EctLoopTest->CallTransferID);

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_EctLoopTest);

		p += encodeEnum(p, ASN1_TAG_ENUM, pc->u.retResult.o.EctLoopTest.LoopResult);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacEctLoopTest() */

/* ******************************************************************* */
/*!
 * \brief Parse the EctLoopTest invoke argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param EctLoopTest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseEctLoopTest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLoopTest *EctLoopTest)
{
	int CallTransferID;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &CallTransferID);
	EctLoopTest->CallTransferID = CallTransferID;

	return p - beg;
}				/* end ParseEctLoopTest() */

/* ******************************************************************* */
/*!
 * \brief Parse the EctLoopTest result parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse parameters
 * \param end End buffer position that must not go past.
 * \param EctLoopTest Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseEctLoopTest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLoopTest_RES *EctLoopTest)
{
	unsigned int LoopResult;
	int ret;
	u_char *beg;

	beg = p;
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &LoopResult);
	EctLoopTest->LoopResult = LoopResult;

	return p - beg;
}				/* end ParseEctLoopTest_RES() */

/* ------------------------------------------------------------------- */
/* end asn1_ect.c */
