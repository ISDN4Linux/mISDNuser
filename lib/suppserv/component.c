/*
 * ASN1 components for supplementary services
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
#include "ccbs.h"
#include "ect.h"
#include <string.h>

// ======================================================================
// Component EN 300 196-1 D.1

static const struct asn1OidConvert OIDConversion[] = {
/* *INDENT-OFF* */
	/*
	 * Note the first value in oid.values[] is really the first two
	 * OID subidentifiers.  They are compressed using this formula:
	 * First_Value = (First_Subidentifier * 40) + Second_Subidentifier
	 */

	/* {ccitt(0) identified-organization(4) etsi(0) 359 operations-and-errors(1)} */
	{ FacOIDBase_CCBS,			{ 4, { 4, 0, 359, 1 } } },

	/* {ccitt(0) identified-organization(4) etsi(0) 359 private-networks-operations-and-errors(2)} */
	{ FacOIDBase_CCBS_T,		{ 4, { 4, 0, 359, 2 } } },

	/* {ccitt(0) identified-organization(4) etsi(0) 1065 operations-and-errors(1)} */
	{ FacOIDBase_CCNR,			{ 4, { 4, 0, 1065, 1 } } },

	/* {ccitt(0) identified-organization(4) etsi(0) 1065 private-networks-operations-and-errors(2)} */
	{ FacOIDBase_CCNR_T,		{ 4, { 4, 0, 1065, 2 } } },

	/* {itu-t(0) identified-organization(4) etsi(0) 196 status-request-procedure(9)} */
	{ FacOIDBase_StatusRequest,	{ 4, { 4, 0, 196, 9 } } },

	/* {ccitt(0) identified-organization(4) etsi(0) 369 operations-and-errors(1)} */
	{ FacOIDBase_ECT,			{ 4, { 4, 0, 369, 1 } } },
/* *INDENT-ON* */
};

const struct asn1OidConvert *FindOidByOidValue(int length, const __u16 oidValues[])
{
	int index;

	for (index = 0; index < sizeof(OIDConversion) / sizeof(OIDConversion[0]); ++index) {
		if (OIDConversion[index].oid.numValues == length
		    && memcmp(OIDConversion[index].oid.value, oidValues, length * sizeof(oidValues[0])) == 0) {
			return &OIDConversion[index];
		}
	}			/* end for */

	return NULL;
}				/* end FindOidByOidValue() */

const struct asn1OidConvert *FindOidByEnum(__u16 value)
{
	int index;

	for (index = 0; index < sizeof(OIDConversion) / sizeof(OIDConversion[0]); ++index) {
		if (FAC_OID_BASE(OIDConversion[index].baseCode) <= value && value < FAC_OID_BASE(OIDConversion[index].baseCode + 1)) {
			return &OIDConversion[index];
		}
	}			/* end for */

	return NULL;
}				/* end FindOidByEnum() */

__u16 ConvertOidToEnum(const struct asn1Oid * oid, __u16 errorValue)
{
	const struct asn1OidConvert *convert;
	__u16 enumValue;

	enumValue = errorValue;
	if (oid->numValues) {
		convert = FindOidByOidValue(oid->numValues - 1, oid->value);
		if (convert) {
			enumValue = FAC_OID_BASE(convert->baseCode) + oid->value[oid->numValues - 1];
		}
	}

	return enumValue;
}				/* end ConvertOidToEnum() */

int ConvertEnumToOid(struct asn1Oid *oid, __u16 enumValue)
{
	int status;
	const struct asn1OidConvert *convert;

	status = 0;		/* Assume failure */
	convert = FindOidByEnum(enumValue);
	if (convert) {
		*oid = convert->oid;
		if (oid->numValues < sizeof(oid->value) / sizeof(oid->value[0])) {
			oid->value[oid->numValues] = enumValue - FAC_OID_BASE(convert->baseCode);
			++oid->numValues;
			status = 1;	/* successful */
		}
	}

	return status;
}				/* end ConvertEnumToOid() */

static int ParseOperationOid(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *operationValue)
{
	struct asn1Oid operationOid;
	int rval;

	CallASN1(rval, p, end, ParseOid(pc, p, end, &operationOid));
	*operationValue = ConvertOidToEnum(&operationOid, Fac_None);

	return rval;
}				/* end ParseOperationOid() */

static int ParseOperationValue(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *operationValue)
{
	INIT;

	XCHOICE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, operationValue);
	XCHOICE_1(ParseOperationOid, ASN1_TAG_OBJECT_IDENTIFIER, ASN1_NOT_TAGGED, operationValue);
	XCHOICE_DEFAULT;
}

static int ParseInvokeComponent(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	signed int invokeId;
	signed int linkedId;
	unsigned int operationValue;
	INIT;

	pc->comp = CompInvoke;
	XSEQUENCE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &invokeId);
	pc->u.inv.invokeId = invokeId;
	XSEQUENCE_OPT_1(ParseSignedInteger, ASN1_TAG_CONTEXT_SPECIFIC, 0, &linkedId);
	XSEQUENCE_1(ParseOperationValue, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &operationValue);
	pc->u.inv.operationValue = operationValue;

	switch (operationValue) {
		/* Diversion support */
	case Fac_Begin3PTY:
	case Fac_End3PTY:
		/* No additional invoke parameters */
		break;
	case Fac_ActivationDiversion:
		XSEQUENCE_1(ParseActivationDiversion, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.ActivationDiversion);
		break;
	case Fac_DeactivationDiversion:
		XSEQUENCE_1(ParseDeactivationDiversion, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.DeactivationDiversion);
		break;
	case Fac_ActivationStatusNotificationDiv:
		XSEQUENCE_1(ParseActivationStatusNotificationDiv, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
			    &pc->u.inv.o.ActivationStatusNotificationDiv);
		break;
	case Fac_DeactivationStatusNotificationDiv:
		XSEQUENCE_1(ParseDeactivationStatusNotificationDiv, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
			    &pc->u.inv.o.DeactivationStatusNotificationDiv);
		break;
	case Fac_InterrogationDiversion:
		XSEQUENCE_1(ParseInterrogationDiversion, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
			    &pc->u.inv.o.InterrogationDiversion);
		break;
	case Fac_DiversionInformation:
		XSEQUENCE_1(ParseDiversionInformation, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.DiversionInformation);
		break;
	case Fac_CallDeflection:
		XSEQUENCE_1(ParseCallDeflection, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CallDeflection);
		break;
	case Fac_CallRerouteing:
		XSEQUENCE_1(ParseCallRerouteing, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CallRerouteing);
		break;
	case Fac_InterrogateServedUserNumbers:
		/* No additional invoke parameters */
		break;
	case Fac_DivertingLegInformation1:
		XSEQUENCE_1(ParseDivertingLegInformation1, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
			    &pc->u.inv.o.DivertingLegInformation1);
		break;
	case Fac_DivertingLegInformation2:
		XSEQUENCE_1(ParseDivertingLegInformation2, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED,
			    &pc->u.inv.o.DivertingLegInformation2);
		break;
	case Fac_DivertingLegInformation3:
		XSEQUENCE_1(ParseDivertingLegInformation3, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED,
			    &pc->u.inv.o.DivertingLegInformation3);
		break;

		/* ECT support */
	case Fac_EctExecute:
		/* No additional invoke parameters */
		break;
	case Fac_ExplicitEctExecute:
		XSEQUENCE_1(ParseExplicitEctExecute, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.ExplicitEctExecute);
		break;
	case Fac_RequestSubaddress:
		/* No additional invoke parameters */
		break;
	case Fac_SubaddressTransfer:
		XSEQUENCE_1(ParseSubaddressTransfer, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.SubaddressTransfer);
		break;
	case Fac_EctLinkIdRequest:
		/* No additional invoke parameters */
		break;
	case Fac_EctInform:
		XSEQUENCE_1(ParseEctInform, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.EctInform);
		break;
	case Fac_EctLoopTest:
		XSEQUENCE_1(ParseEctLoopTest, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.EctLoopTest);
		break;

		/* AOC support */
#if 0
	case Fac_ChargingRequest:
	case Fac_AOCSCurrency:
	case Fac_AOCSSpecialArr:
		break;
#endif
	case Fac_AOCDCurrency:
		XSEQUENCE_1(ParseAOCDCurrency, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.AOCcur);
		break;
	case Fac_AOCDChargingUnit:
		XSEQUENCE_1(ParseAOCDChargingUnit, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.AOCchu);
		break;
	case Fac_AOCECurrency:
		XSEQUENCE_1(ParseAOCECurrency, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.AOCcur);
		break;
	case Fac_AOCEChargingUnit:
		XSEQUENCE_1(ParseAOCEChargingUnit, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.AOCchu);
		break;
	case Fac_StatusRequest:
		XSEQUENCE_1(ParseStatusRequest, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.StatusRequest);
		break;

		/* CCBS/CCNR support */
	case Fac_CallInfoRetain:
		XSEQUENCE_1(ParseCallInfoRetain, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.CallInfoRetain);
		break;
	case Fac_CCBSRequest:
		XSEQUENCE_1(ParseCCBSRequest, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSRequest);
		break;
	case Fac_CCBSDeactivate:
		XSEQUENCE_1(ParseCCBSDeactivate, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSDeactivate);
		break;
	case Fac_CCBSInterrogate:
		XSEQUENCE_1(ParseCCBSInterrogate, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSInterrogate);
		break;
	case Fac_CCBSErase:
		XSEQUENCE_1(ParseCCBSErase, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSErase);
		break;
	case Fac_CCBSRemoteUserFree:
		XSEQUENCE_1(ParseCCBSRemoteUserFree, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSRemoteUserFree);
		break;
	case Fac_CCBSCall:
		XSEQUENCE_1(ParseCCBSCall, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSCall);
		break;
	case Fac_CCBSStatusRequest:
		XSEQUENCE_1(ParseCCBSStatusRequest, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSStatusRequest);
		break;
	case Fac_CCBSBFree:
		XSEQUENCE_1(ParseCCBSBFree, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSBFree);
		break;
	case Fac_EraseCallLinkageID:
		XSEQUENCE_1(ParseEraseCallLinkageID, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.EraseCallLinkageID);
		break;
	case Fac_CCBSStopAlerting:
		XSEQUENCE_1(ParseCCBSStopAlerting, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBSStopAlerting);
		break;
	case Fac_CCNRRequest:
		XSEQUENCE_1(ParseCCNRRequest, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.inv.o.CCNRRequest);
		break;
	case Fac_CCNRInterrogate:
		XSEQUENCE_1(ParseCCNRInterrogate, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCNRInterrogate);
		break;

		/* CCBS-T/CCNR-T support */
	case Fac_CCBS_T_Call:
	case Fac_CCBS_T_Suspend:
	case Fac_CCBS_T_Resume:
	case Fac_CCBS_T_RemoteUserFree:
	case Fac_CCBS_T_Available:
		/* No additional invoke parameters */
		break;
	case Fac_CCBS_T_Request:
		XSEQUENCE_1(ParseCCBS_T_Request, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCBS_T_Request);
		break;
	case Fac_CCNR_T_Request:
		XSEQUENCE_1(ParseCCNR_T_Request, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.inv.o.CCNR_T_Request);
		break;

	default:
		return -1;
	}

	return p - beg;
}

int ParseReturnResultComponentSequence(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	INIT;

	XSEQUENCE_1(ParseOperationValue, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.operationValue);
	pc->u.retResult.operationValuePresent = 1;

	switch (pc->u.retResult.operationValue) {
		/* 3PTY */
	case Fac_Begin3PTY:
	case Fac_End3PTY:
		/* No additional result parameters */
		break;
		/* Diversion support */
	case Fac_ActivationDiversion:
	case Fac_DeactivationDiversion:
		/* No additional result parameters */
		break;
	case Fac_InterrogationDiversion:
		XSEQUENCE_1(ParseInterrogationDiversion_RES, ASN1_TAG_SET, ASN1_NOT_TAGGED,
			    &pc->u.retResult.o.InterrogationDiversion);
		break;
	case Fac_CallDeflection:
	case Fac_CallRerouteing:
		/* No additional result parameters */
		break;
	case Fac_InterrogateServedUserNumbers:
		XSEQUENCE_1(ParseInterrogateServedUserNumbers_RES, ASN1_TAG_SET, ASN1_NOT_TAGGED,
			    &pc->u.retResult.o.InterrogateServedUserNumbers);
		break;

		/* ECT support */
	case Fac_EctLinkIdRequest:
		XSEQUENCE_1(ParseEctLinkIdRequest_RES, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.o.EctLinkIdRequest);
		break;
	case Fac_EctLoopTest:
		XSEQUENCE_1(ParseEctLoopTest_RES, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.o.EctLoopTest);
		break;

	case Fac_StatusRequest:
		XSEQUENCE_1(ParseStatusRequest_RES, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.o.StatusRequest);
		break;

		/* CCBS/CCNR support */
	case Fac_CCBSDeactivate:
		/* No additional result parameters */
		break;
	case Fac_CCBSStatusRequest:
		XSEQUENCE_1(ParseCCBSStatusRequest_RES, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCBSStatusRequest);
		break;
	case Fac_CCBSRequest:
		XSEQUENCE_1(ParseCCBSRequest_RES, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCBSRequest);
		break;
	case Fac_CCBSInterrogate:
		XSEQUENCE_1(ParseCCBSInterrogate_RES, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCBSInterrogate);
		break;
	case Fac_CCNRRequest:
		XSEQUENCE_1(ParseCCNRRequest_RES, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCNRRequest);
		break;
	case Fac_CCNRInterrogate:
		XSEQUENCE_1(ParseCCNRInterrogate_RES, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCNRInterrogate);
		break;

		/* CCBS-T/CCNR-T support */
	case Fac_CCBS_T_Request:
		XSEQUENCE_1(ParseCCBS_T_Request_RES, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCBS_T_Request);
		break;
	case Fac_CCNR_T_Request:
		XSEQUENCE_1(ParseCCNR_T_Request_RES, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &pc->u.retResult.o.CCNR_T_Request);
		break;

	default:
		return -1;
	}

	return p - beg;
}

static int ParseReturnResultComponent(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	signed int invokeId;
	INIT;

	pc->comp = CompReturnResult;
	XSEQUENCE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &invokeId);
	pc->u.retResult.invokeId = invokeId;
	pc->u.retResult.operationValuePresent = 0;
	XSEQUENCE_OPT(ParseReturnResultComponentSequence, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED);

	return p - beg;
}

static int ParseErrorOid(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *errorValue)
{
	struct asn1Oid errorOid;
	int rval;

	CallASN1(rval, p, end, ParseOid(pc, p, end, &errorOid));
	*errorValue = ConvertOidToEnum(&errorOid, FacError_Unknown);

	return rval;
}				/* end ParseErrorOid() */

static int ParseErrorValue(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *errorValue)
{
	INIT;

	XCHOICE_1(ParseUnsignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, errorValue);
	XCHOICE_1(ParseErrorOid, ASN1_TAG_OBJECT_IDENTIFIER, ASN1_NOT_TAGGED, errorValue);
	XCHOICE_DEFAULT;
}

int ParseReturnErrorComponent(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	int invokeId;
	unsigned int errorValue;
	const char *error;
	char msg[20];
	INIT;

	pc->comp = CompReturnError;

	XSEQUENCE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &invokeId);
	XSEQUENCE_1(ParseErrorValue, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &errorValue);

	pc->u.retError.invokeId = invokeId;
	pc->u.retError.errorValue = errorValue;

	switch (errorValue) {
	case FacError_Gen_NotSubscribed:
		error = "not subscribed";
		break;
	case FacError_Gen_NotAvailable:
		error = "not available";
		break;
	case FacError_Gen_NotImplemented:
		error = "not implemented";
		break;
	case FacError_Gen_InvalidServedUserNr:
		error = "invalid served user nr";
		break;
	case FacError_Gen_InvalidCallState:
		error = "invalid call state";
		break;
	case FacError_Gen_BasicServiceNotProvided:
		error = "basic service not provided";
		break;
	case FacError_Gen_NotIncomingCall:
		error = "not incoming call";
		break;
	case FacError_Gen_SupplementaryServiceInteractionNotAllowed:
		error = "supplementary service interaction not allowed";
		break;
	case FacError_Gen_ResourceUnavailable:
		error = "resource unavailable";
		break;
	case FacError_Div_InvalidDivertedToNr:
		error = "invalid diverted-to nr";
		break;
	case FacError_Div_SpecialServiceNr:
		error = "special service nr";
		break;
	case FacError_Div_DiversionToServedUserNr:
		error = "diversion to served user nr";
		break;
	case FacError_Div_IncomingCallAccepted:
		error = "incoming call accepted";
		break;
	case FacError_Div_NumberOfDiversionsExceeded:
		error = "number of diversions exceeded";
		break;
	case FacError_Div_NotActivated:
		error = "not activated";
		break;
	case FacError_Div_RequestAlreadyAccepted:
		error = "request already accepted";
		break;
	case FacError_AOC_NoChargingInfoAvailable:
		error = "no charging info available";
		break;
	case FacError_CCBS_InvalidCallLinkageID:
		error = "invalid call linkage id";
		break;
	case FacError_CCBS_InvalidCCBSReference:
		error = "invalid ccbs reference";
		break;
	case FacError_CCBS_T_LongTermDenial:
	case FacError_CCBS_LongTermDenial:
		error = "long term denial";
		break;
	case FacError_CCBS_T_ShortTermDenial:
	case FacError_CCBS_ShortTermDenial:
		error = "short term denial";
		break;
	case FacError_CCBS_IsAlreadyActivated:
		error = "ccbs is already activated";
		break;
	case FacError_CCBS_AlreadyAccepted:
		error = "already accepted";
		break;
	case FacError_CCBS_OutgoingCCBSQueueFull:
		error = "outgoing ccbs queue full";
		break;
	case FacError_CCBS_CallFailureReasonNotBusy:
		error = "call failure reason not busy";
		break;
	case FacError_CCBS_NotReadyForCall:
		error = "not ready for call";
		break;
	case FacError_Unknown:
		error = "unknown OID error code";
		break;
	default:
		sprintf(msg, "(%d)", errorValue);
		error = msg;
		break;
	}
	dprint(DBGM_ASN1_DEC, "Decoded-Error: %s\n", error);
	return p - beg;
}				/* end of ParseReturnErrorComponent() */

int ParseProblemValue(struct asn1_parm *pc, u_char * p, u_char * end, asn1Problem prob)
{
	int rval;

	pc->u.reject.problem = prob;
	rval = ParseUnsignedInteger(pc, p, end, &pc->u.reject.problemValue);
	dprint(DBGM_ASN1_DEC, "ParseProblemValue: %d %d, rval:%d\n", prob, pc->u.reject.problemValue, rval);

	return rval;
}

int ParseRejectProblem(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	INIT;

	XCHOICE_1(ParseProblemValue, ASN1_TAG_CONTEXT_SPECIFIC, 0, GeneralP);
	XCHOICE_1(ParseProblemValue, ASN1_TAG_CONTEXT_SPECIFIC, 1, InvokeP);
	XCHOICE_1(ParseProblemValue, ASN1_TAG_CONTEXT_SPECIFIC, 2, ReturnResultP);
	XCHOICE_1(ParseProblemValue, ASN1_TAG_CONTEXT_SPECIFIC, 3, ReturnErrorP);
	XCHOICE_DEFAULT;
}

static int ParseRejectInvokeId(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	INIT;

	pc->u.reject.invokeIdPresent = 1;
	XCHOICE_1(ParseSignedInteger, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &pc->u.reject.invokeId);

	pc->u.reject.invokeIdPresent = 0;
	pc->u.reject.invokeId = 0;
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);

	XCHOICE_DEFAULT;
}

int ParseRejectComponent(struct asn1_parm *pc, u_char * p, u_char * end, int dummy)
{
	INIT;

	pc->comp = CompReject;

	XSEQUENCE(ParseRejectInvokeId, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED);
	if (pc->u.reject.invokeIdPresent) {
		dprint(DBGM_ASN1_DEC, "ParseRejectComponent: invokeId %d\n", pc->u.reject.invokeId);
	}

	XSEQUENCE(ParseRejectProblem, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED);

	return p - beg;
}

int ParseComponent(struct asn1_parm *pc, u_char * p, u_char * end)
{
	INIT;

	XCHOICE(ParseInvokeComponent, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 1);
	XCHOICE(ParseReturnResultComponent, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 2);
	XCHOICE(ParseReturnErrorComponent, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 3);
	XCHOICE(ParseRejectComponent, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 4);
	XCHOICE_DEFAULT;
}
