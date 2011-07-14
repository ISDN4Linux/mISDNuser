/*
 * fac.c
 *
 * Copyright (C) 2006, Nadi Sarrar
 * Nadi Sarrar <nadi@beronet.com>
 *
 * Portions of this file are based on the mISDN sources
 * by Karsten Keil.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "asn1.h"
#include "asn1_diversion.h"
#include <mISDN/q931.h>
#include "asn1_ccbs.h"
#include "asn1_ect.h"
#include <string.h>

#define AOC_MAX_NUM_CHARGE_UNITS   0xFFFFFF
#define AOCE_CHARGE_UNIT_IE_LENGTH 17
#define AOCD_CHARGE_UNIT_IE_LENGTH 20
#define MISDN_FACILITY_DEBUG       1

enum {
	SUPPLEMENTARY_SERVICE = 0x91,	/* remote operations protocol */
} SERVICE_DISCRIMINATOR;

/* ******************************************************************* */
/*!
 * \brief Encode the head of the Facility component message.
 *
 * \param p Start of the Facility message
 * \param componentType Component-Type value
 *
 * \return Position for the next octet in the message
 */
__u8 *encodeComponent_Head(__u8 * p, enum asn1ComponentTag componentTag)
{
	*p++ = IE_FACILITY;	/* Facility IE identifier */
	*p++ = 0;		/* length -- not known yet */
	*p++ = SUPPLEMENTARY_SERVICE;	/* remote operations protocol */
	*p++ = componentTag;
	*p++ = 0;		/* length -- not known yet */
	return p;
}				/* end encodeComponent_Head() */

/* ******************************************************************* */
/*!
 * \brief Encode the head of the Facility component message
 * with a length that can take a full __u8 value.
 *
 * \param p Start of the Facility message
 * \param componentType Component-Type value
 *
 * \return Position for the next octet in the message
 */
__u8 *encodeComponent_Head_Long_u8(__u8 * p, enum asn1ComponentTag componentTag)
{
	*p++ = IE_FACILITY;	/* Facility IE identifier */
	*p++ = 0;		/* length -- not known yet */
	*p++ = SUPPLEMENTARY_SERVICE;	/* remote operations protocol */
	*p++ = componentTag;
	p += ASN1_NUM_OCTETS_LONG_LENGTH_u8;	/* length -- not known yet */
	return p;
}				/* end encodeComponent_Head_Long_u8() */

/* ******************************************************************* */
/*!
 * \brief Fill in the final length for the Facility IE contents.
 *
 * \param msg Beginning of the Facility IE contents.
 * \param p Pointer to the next octet of the Facility IE
 *            contents if there were more.
 *
 * \return Length of the Facility IE message
 */
int encodeComponent_Length(__u8 * msg, __u8 * p)
{
	msg[4] = p - &msg[5];
	msg[1] = p - &msg[2];
	return msg[1] + 2;
}				/* end encodeComponent_Length() */

/* ******************************************************************* */
/*!
 * \brief Fill in the final length for the Facility IE contents
 * with a length that can take a full __u8 value.
 *
 * \param msg Beginning of the Facility IE contents.
 * \param p Pointer to the next octet of the Facility IE
 *            contents if there were more.
 *
 * \return Length of the Facility IE message
 */
int encodeComponent_Length_Long_u8(__u8 * msg, __u8 * p)
{
	encodeLen_Long_u8(&msg[4], p - &msg[4 + ASN1_NUM_OCTETS_LONG_LENGTH_u8]);
	msg[1] = p - &msg[2];
	return msg[1] + 2;
}				/* end encodeComponent_Length_Long_u8() */

/* ******************************************************************* */
/*!
 * \brief Encode the Facility ie component operation-value.
 *
 * \param dest Where to put the operation-value
 * \param operationValue enum operation-value to encode
 *
 * \return Length of the operation-value
 */
int encodeOperationValue(__u8 * dest, int operationValue)
{
	struct asn1Oid operationOid;

	if (IsEnumOid(operationValue)
	    && ConvertEnumToOid(&operationOid, operationValue)) {
		return encodeOid(dest, ASN1_TAG_OBJECT_IDENTIFIER, &operationOid);
	} else {
		return encodeInt(dest, ASN1_TAG_INTEGER, operationValue);
	}
}				/* end encodeOperationValue() */

/* ******************************************************************* */
/*!
 * \brief Encode the Facility ie component error-value.
 *
 * \param dest Where to put the error-value
 * \param errorValue enum error-value to encode
 *
 * \return Length of the error-value
 */
int encodeErrorValue(__u8 * dest, int errorValue)
{
	return encodeOperationValue(dest, errorValue);
}				/* end encodeErrorValue() */

/* ******************************************************************* */
/*!
 * \brief Encode the common invoke component beginning
 *
 * \param Dest Where to put the encoding
 * \param InvokeID
 * \param OperationValue
 *
 * \return Position for the next octet in the message
 */
__u8 *encodeComponentInvoke_Head(__u8 * Dest, int InvokeID, enum FacFunction OperationValue)
{
	__u8 *p;

	p = encodeComponent_Head(Dest, asn1ComponentTag_Invoke);
	p += encodeInt(p, ASN1_TAG_INTEGER, InvokeID);
	//p += encodeInt(p, ASN1_TAG_CONTEXT_SPECIFIC | 0, LinkedID); /* Optional */
	p += encodeOperationValue(p, OperationValue);
	return p;
}				/* end encodeComponentInvoke_Head() */

/* ******************************************************************* */
/*!
 * \brief Encode the common invoke component beginning
 * with a length that can take a full __u8 value.
 *
 * \param Dest Where to put the encoding
 * \param InvokeID
 * \param OperationValue
 *
 * \return Position for the next octet in the message
 */
__u8 *encodeComponentInvoke_Head_Long_u8(__u8 * Dest, int InvokeID, enum FacFunction OperationValue)
{
	__u8 *p;

	p = encodeComponent_Head_Long_u8(Dest, asn1ComponentTag_Invoke);
	p += encodeInt(p, ASN1_TAG_INTEGER, InvokeID);
	//p += encodeInt(p, ASN1_TAG_CONTEXT_SPECIFIC | 0, LinkedID); /* Optional */
	p += encodeOperationValue(p, OperationValue);
	return p;
}				/* end encodeComponentInvoke_Head_Long_u8() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the anonymous RESULT response.
 *
 * \param dest Where to put the encoding
 * \param result RESULT parameters
 *
 * \return Length of the encoding
 */
static int encodeFacRESULT(__u8 * dest, const struct FacRESULT *result)
{
	__u8 *p;

	p = encodeComponent_Head(dest, asn1ComponentTag_Result);
	p += encodeInt(p, ASN1_TAG_INTEGER, result->InvokeID);
	return encodeComponent_Length(dest, p);
}				/* end encodeFacRESULT() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the ERROR response.
 *
 * \param dest Where to put the encoding
 * \param error ERROR parameters
 *
 * \return Length of the encoding
 */
static int encodeFacERROR(__u8 * dest, const struct FacERROR *error)
{
	__u8 *p;

	p = encodeComponent_Head(dest, asn1ComponentTag_Error);
	p += encodeInt(p, ASN1_TAG_INTEGER, error->InvokeID);
	p += encodeErrorValue(p, error->errorValue);
	return encodeComponent_Length(dest, p);
}				/* end encodeFacERROR() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the REJECT response.
 *
 * \param dest Where to put the encoding
 * \param error REJECT parameters
 *
 * \return Length of the encoding
 */
static int encodeFacREJECT(__u8 * dest, const struct FacREJECT *reject)
{
	int code;
	enum FacRejectBase base;
	__u8 *p;

	p = encodeComponent_Head(dest, asn1ComponentTag_Reject);

	if (reject->InvokeIDPresent) {
		p += encodeInt(p, ASN1_TAG_INTEGER, reject->InvokeID);
	} else {
		p += encodeNull(p, ASN1_TAG_NULL);
	}

	for (base = (enum FacRejectBase)0; base < FacRejectBase_Last; ++base) {
		if (FAC_REJECT_BASE(base) <= reject->Code && reject->Code < FAC_REJECT_BASE(base + 1)) {
			break;
		}
	}			/* end for */
	if (base == FacRejectBase_Last) {
		/* We were given a bad reject code */
		code = -1;
		base = FacRejectBase_General;
	} else {
		code = reject->Code - FAC_REJECT_BASE(base);
	}
	p += encodeInt(p, ASN1_TAG_CONTEXT_SPECIFIC | base, code);

	return encodeComponent_Length(dest, p);
}				/* end encodeFacREJECT() */

unsigned char encode_invoke_id(void)
{
	static int id = 1;
	int max_id = 255;

	if ((id > max_id) || (id == 0)) {
		id = 1;
	}
	return (unsigned char)id++;
}

static int encodeAOCDChargingUnitOperation(__u8 * dest, int numberOfUnits)
{
	/* Manually encoded ASN.1 */

	unsigned char len = 0;
	int i = 0;
	int result = -1;

	__u8 *p = dest;

	if ((p != NULL) && (numberOfUnits <= AOC_MAX_NUM_CHARGE_UNITS)) {
		if (numberOfUnits > 0xFFFF)
			len = 3;
		else if (numberOfUnits > 0xFF)
			len = 2;
		else
			len = 1;

		p[i++] = IE_FACILITY;	// IE identifier
		p[i++] = 0x00;	// length -- not known yet
		p[i++] = SUPPLEMENTARY_SERVICE;	// remote operations protocol
		p[i++] = 0xa1;	// invoke component
		p[i++] = 0x00;	// length -- not known yet
		p[i++] = 0x02;	// Invoke ID
		p[i++] = 0x01;	// InvokeId Length
		p[i++] = encode_invoke_id();	// Invoke value
		p[i++] = 0x02;	// Operation Tag
		p[i++] = 0x01;	// Tag Length
		p[i++] = Fac_AOCDChargingUnit;	// Operation Value
		p[i++] = 0x30;
		p[i++] = (0x09 + len);	// Length
		p[i++] = 0xa1;	// APDU
		p[i++] = (0x04 + len);	// Length
		p[i++] = 0x30;
		p[i++] = (0x02 + len);
		p[i++] = 0x02;	// Operation Tag
		p[i++] = 0x01;	// AOC-D so Sub-Total

		// Recorded units could take up as much as 3 bytes (0xFFFFFF)
		p[i] = (unsigned char)numberOfUnits;
		i += len;

		p[i++] = 0x82;
		p[i++] = 0x01;
		p[i++] = 0x01;

		p[1] = (AOCD_CHARGE_UNIT_IE_LENGTH + len);	// IE Payload Length
		p[4] = p[1] - 3;	// Invoke Component Length
		result = p[1] + 2;	// Total Length of IE
	}
	return result;
}

static int encodeAOCEChargingUnitOperation(__u8 * dest, int numberOfUnits)
{
	/* Manually encoded ASN.1 */

	unsigned char len = 0;

	int i = 0;
	int result = -1;

	__u8 *p = dest;

	if ((p != NULL) && (numberOfUnits <= AOC_MAX_NUM_CHARGE_UNITS)) {
		if (numberOfUnits > 0xFFFF)
			len = 3;
		else if (numberOfUnits > 0xFF)
			len = 2;
		else
			len = 1;

		p[i++] = IE_FACILITY;	// IE identifier
		p[i++] = 0x00;	// length -- not known yet
		p[i++] = SUPPLEMENTARY_SERVICE;	// remote operations protocol
		p[i++] = 0xa1;	// invoke component
		p[i++] = 0x00;	// length -- not known yet
		p[i++] = 0x02;	// Invoke ID
		p[i++] = 0x01;	// InvokeId Length
		p[i++] = encode_invoke_id();	// Invoke value
		p[i++] = 0x02;	// Operation Tag
		p[i++] = 0x01;	// Tag Length
		p[i++] = Fac_AOCEChargingUnit;	// Operation Value
		p[i++] = 0xa1;	// APDU
		p[i++] = 0x30;
		p[i++] = (0x09 + len);	// Length
		p[i++] = 0xa1;	// APDU
		p[i++] = (0x04 + len);	// Length
		p[i++] = 0x30;
		p[i++] = (0x01 + len);
		p[i++] = 0x02;
		p[i++] = 0x02;	// AOC-E so Total

		// Recorded units could take up as much as 3 bytes (0xFFFFFF)
		p[i] = (unsigned char)numberOfUnits;
		i += len;

		p[1] = (AOCE_CHARGE_UNIT_IE_LENGTH + len);	// IE Payload Length
		p[4] = p[1] - 3;	// Invoke Component Length

		result = p[1] + 2;	// Total Length of IE
	}
	return result;
}

static int encodeFacMcidIvokeRequest(__u8 * dest, int *fac_invokeId)
{
	int i = 0;
	int result = -1;
	unsigned char invokeId = 0;

	__u8 *p = dest;

	if ((p != NULL) && (fac_invokeId)) {
		invokeId = encode_invoke_id();

		p[i++] = IE_FACILITY;	// IE identifier
		p[i++] = 0x09;	// length
		p[i++] = SUPPLEMENTARY_SERVICE;	// remote operations protocol
		p[i++] = 0xa1;	// invoke component
		p[i++] = 0x06;	// Manually Encoding the MCID
		p[i++] = 0x02;	// Invoke ID
		p[i++] = 0x01;	// InvokeId Length
		p[i++] = invokeId;	// Invoke value
		p[i++] = 0x02;	// Operation Tag
		p[i++] = 0x01;	// Tag Length
		p[i++] = Fac_MaliciousCallId;	// Operation Value

		result = p[1] + 2;	// Total IE Length

		*fac_invokeId = (int)invokeId;
	}
	return result;
}

/*
 * Facility IE Encoding
 */
int encodeFac(__u8 * dest, struct FacParm *fac)
{
	int len = -1;

	switch (fac->Function) {
	case Fac_None:
		break;

		/* Diversion support */
	case Fac_ActivationDiversion:
		len = encodeFacActivationDiversion(dest, &fac->u.ActivationDiversion);
		break;
	case Fac_DeactivationDiversion:
		len = encodeFacDeactivationDiversion(dest, &fac->u.DeactivationDiversion);
		break;
	case Fac_ActivationStatusNotificationDiv:
		len = encodeFacActivationStatusNotificationDiv(dest, &fac->u.ActivationStatusNotificationDiv);
		break;
	case Fac_DeactivationStatusNotificationDiv:
		len = encodeFacDeactivationStatusNotificationDiv(dest, &fac->u.DeactivationStatusNotificationDiv);
		break;
	case Fac_InterrogationDiversion:
		len = encodeFacInterrogationDiversion(dest, &fac->u.InterrogationDiversion);
		break;
	case Fac_DiversionInformation:
		len = encodeFacDiversionInformation(dest, &fac->u.DiversionInformation);
		break;
	case Fac_CallDeflection:
		len = encodeFacCallDeflection(dest, &fac->u.CallDeflection);
		break;
	case Fac_CallRerouteing:
		len = encodeFacCallRerouteing(dest, &fac->u.CallRerouteing);
		break;
	case Fac_InterrogateServedUserNumbers:
		len = encodeFacInterrogateServedUserNumbers(dest, &fac->u.InterrogateServedUserNumbers);
		break;
	case Fac_DivertingLegInformation1:
		len = encodeFacDivertingLegInformation1(dest, &fac->u.DivertingLegInformation1);
		break;
	case Fac_DivertingLegInformation2:
		len = encodeFacDivertingLegInformation2(dest, &fac->u.DivertingLegInformation2);
		break;
	case Fac_DivertingLegInformation3:
		len = encodeFacDivertingLegInformation3(dest, &fac->u.DivertingLegInformation3);
		break;

		/* ECT support */
	case Fac_EctExecute:
		len = encodeFacEctExecute(dest, &fac->u.EctExecute);
		break;
	case Fac_ExplicitEctExecute:
		len = encodeFacExplicitEctExecute(dest, &fac->u.ExplicitEctExecute);
		break;
	case Fac_RequestSubaddress:
		len = encodeFacRequestSubaddress(dest, &fac->u.RequestSubaddress);
		break;
	case Fac_SubaddressTransfer:
		len = encodeFacSubaddressTransfer(dest, &fac->u.SubaddressTransfer);
		break;
	case Fac_EctLinkIdRequest:
		len = encodeFacEctLinkIdRequest(dest, &fac->u.EctLinkIdRequest);
		break;
	case Fac_EctInform:
		len = encodeFacEctInform(dest, &fac->u.EctInform);
		break;
	case Fac_EctLoopTest:
		len = encodeFacEctLoopTest(dest, &fac->u.EctLoopTest);
		break;

		/* AOC support */
	case Fac_ChargingRequest:
	case Fac_AOCSCurrency:
	case Fac_AOCSSpecialArr:
	case Fac_AOCDCurrency:
	case Fac_AOCECurrency:
		break;
	case Fac_AOCDChargingUnit:
		len = encodeAOCDChargingUnitOperation(dest, fac->u.AOCchu.recordedUnits);
		break;
	case Fac_AOCEChargingUnit:
		len = encodeAOCEChargingUnitOperation(dest, fac->u.AOCchu.recordedUnits);
		break;
/* Malicious Call Tag Support */
	case Fac_MaliciousCallId:
		len = encodeFacMcidIvokeRequest(dest, &fac->InvokeID);
		break;

	case Fac_RESULT:
		len = encodeFacRESULT(dest, &fac->u.RESULT);
		break;
	case Fac_ERROR:
		len = encodeFacERROR(dest, &fac->u.ERROR);
		break;
	case Fac_REJECT:
		len = encodeFacREJECT(dest, &fac->u.REJECT);
		break;

	case Fac_StatusRequest:
		len = encodeFacStatusRequest(dest, &fac->u.StatusRequest);
		break;

		/* CCBS/CCNR support */
	case Fac_CallInfoRetain:
		len = encodeFacCallInfoRetain(dest, &fac->u.CallInfoRetain);
		break;
	case Fac_EraseCallLinkageID:
		len = encodeFacEraseCallLinkageID(dest, &fac->u.EraseCallLinkageID);
		break;
	case Fac_CCBSDeactivate:
		len = encodeFacCCBSDeactivate(dest, &fac->u.CCBSDeactivate);
		break;
	case Fac_CCBSErase:
		len = encodeFacCCBSErase(dest, &fac->u.CCBSErase);
		break;
	case Fac_CCBSRemoteUserFree:
		len = encodeFacCCBSRemoteUserFree(dest, &fac->u.CCBSRemoteUserFree);
		break;
	case Fac_CCBSCall:
		len = encodeFacCCBSCall(dest, &fac->u.CCBSCall);
		break;
	case Fac_CCBSStatusRequest:
		len = encodeFacCCBSStatusRequest(dest, &fac->u.CCBSStatusRequest);
		break;
	case Fac_CCBSBFree:
		len = encodeFacCCBSBFree(dest, &fac->u.CCBSBFree);
		break;
	case Fac_CCBSStopAlerting:
		len = encodeFacCCBSStopAlerting(dest, &fac->u.CCBSStopAlerting);
		break;
	case Fac_CCBSRequest:
		len = encodeFacCCBSRequest(dest, &fac->u.CCBSRequest);
		break;
	case Fac_CCBSInterrogate:
		len = encodeFacCCBSInterrogate(dest, &fac->u.CCBSInterrogate);
		break;
	case Fac_CCNRRequest:
		len = encodeFacCCNRRequest(dest, &fac->u.CCNRRequest);
		break;
	case Fac_CCNRInterrogate:
		len = encodeFacCCNRInterrogate(dest, &fac->u.CCNRInterrogate);
		break;

		/* CCBS-T/CCNR-T support */
	case Fac_CCBS_T_Call:
		len = encodeFacCCBS_T_Call(dest, &fac->u.CCBS_T_Call);
		break;
	case Fac_CCBS_T_Suspend:
		len = encodeFacCCBS_T_Suspend(dest, &fac->u.CCBS_T_Suspend);
		break;
	case Fac_CCBS_T_Resume:
		len = encodeFacCCBS_T_Resume(dest, &fac->u.CCBS_T_Resume);
		break;
	case Fac_CCBS_T_RemoteUserFree:
		len = encodeFacCCBS_T_RemoteUserFree(dest, &fac->u.CCBS_T_RemoteUserFree);
		break;
	case Fac_CCBS_T_Available:
		len = encodeFacCCBS_T_Available(dest, &fac->u.CCBS_T_Available);
		break;
	case Fac_CCBS_T_Request:
		len = encodeFacCCBS_T_Request(dest, &fac->u.CCBS_T_Request);
		break;
	case Fac_CCNR_T_Request:
		len = encodeFacCCNR_T_Request(dest, &fac->u.CCNR_T_Request);
		break;

	default:
		break;
	}
	return len;
}				/* end encodeFac() */

/*
 * Facility IE Decoding
 */
int decodeFac(__u8 * src, struct FacParm *fac)
{
	struct asn1_parm pc;
	unsigned fac_len;
	__u8 *end;
	__u8 *p = src;

	if (!p) {
		goto _dec_err;
	}

	fac_len = *p++;
	end = p + fac_len;

	ParseASN1(p + 1, end, 0);

	if (*p++ != SUPPLEMENTARY_SERVICE) {
		goto _dec_err;
	}

	if (ParseComponent(&pc, p, end) == -1) {
		goto _dec_err;
	}

	switch (pc.comp) {
	case invoke:
		fac->Function = pc.u.inv.operationValue;
		switch (pc.u.inv.operationValue) {
			/* Diversion support */
		case Fac_ActivationDiversion:
			fac->u.ActivationDiversion.InvokeID = pc.u.inv.invokeId;
			fac->u.ActivationDiversion.ComponentType = FacComponent_Invoke;
			fac->u.ActivationDiversion.Component.Invoke = pc.u.inv.o.ActivationDiversion;
			return 0;
		case Fac_DeactivationDiversion:
			fac->u.DeactivationDiversion.InvokeID = pc.u.inv.invokeId;
			fac->u.DeactivationDiversion.ComponentType = FacComponent_Invoke;
			fac->u.DeactivationDiversion.Component.Invoke = pc.u.inv.o.DeactivationDiversion;
			return 0;
		case Fac_ActivationStatusNotificationDiv:
			fac->u.ActivationStatusNotificationDiv = pc.u.inv.o.ActivationStatusNotificationDiv;
			return 0;
		case Fac_DeactivationStatusNotificationDiv:
			fac->u.DeactivationStatusNotificationDiv = pc.u.inv.o.DeactivationStatusNotificationDiv;
			return 0;
		case Fac_InterrogationDiversion:
			fac->u.InterrogationDiversion.InvokeID = pc.u.inv.invokeId;
			fac->u.InterrogationDiversion.ComponentType = FacComponent_Invoke;
			fac->u.InterrogationDiversion.Component.Invoke = pc.u.inv.o.InterrogationDiversion;
			return 0;
		case Fac_DiversionInformation:
			fac->u.DiversionInformation = pc.u.inv.o.DiversionInformation;
			return 0;
		case Fac_CallDeflection:
			fac->u.CallDeflection.InvokeID = pc.u.inv.invokeId;
			fac->u.CallDeflection.ComponentType = FacComponent_Invoke;
			fac->u.CallDeflection.Component.Invoke = pc.u.inv.o.CallDeflection;
			return 0;
		case Fac_CallRerouteing:
			fac->u.CallRerouteing.InvokeID = pc.u.inv.invokeId;
			fac->u.CallRerouteing.ComponentType = FacComponent_Invoke;
			fac->u.CallRerouteing.Component.Invoke = pc.u.inv.o.CallRerouteing;
			return 0;
		case Fac_InterrogateServedUserNumbers:
			fac->u.InterrogateServedUserNumbers.InvokeID = pc.u.inv.invokeId;
			fac->u.InterrogateServedUserNumbers.ComponentType = FacComponent_Invoke;
			return 0;
		case Fac_DivertingLegInformation1:
			fac->u.DivertingLegInformation1 = pc.u.inv.o.DivertingLegInformation1;
			return 0;
		case Fac_DivertingLegInformation2:
			fac->u.DivertingLegInformation2 = pc.u.inv.o.DivertingLegInformation2;
			return 0;
		case Fac_DivertingLegInformation3:
			fac->u.DivertingLegInformation3 = pc.u.inv.o.DivertingLegInformation3;
			return 0;

			/* ECT support */
		case Fac_EctExecute:
			fac->u.EctExecute.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_ExplicitEctExecute:
			fac->u.ExplicitEctExecute = pc.u.inv.o.ExplicitEctExecute;
			return 0;
		case Fac_RequestSubaddress:
			fac->u.RequestSubaddress.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_SubaddressTransfer:
			fac->u.SubaddressTransfer = pc.u.inv.o.SubaddressTransfer;
			return 0;
		case Fac_EctLinkIdRequest:
			fac->u.EctLinkIdRequest.InvokeID = pc.u.inv.invokeId;
			fac->u.EctLinkIdRequest.ComponentType = FacComponent_Invoke;
			return 0;
		case Fac_EctInform:
			fac->u.EctInform = pc.u.inv.o.EctInform;
			return 0;
		case Fac_EctLoopTest:
			fac->u.EctLoopTest.InvokeID = pc.u.inv.invokeId;
			fac->u.EctLoopTest.ComponentType = FacComponent_Invoke;
			fac->u.EctLoopTest.Component.Invoke = pc.u.inv.o.EctLoopTest;
			return 0;

			/* AOC support */
		case Fac_ChargingRequest:
		case Fac_AOCSCurrency:
		case Fac_AOCSSpecialArr:
			break;
		case Fac_AOCDCurrency:
		case Fac_AOCECurrency:
			fac->Function = pc.u.inv.operationValue;
			memcpy(&(fac->u.AOCcur), &(pc.u.inv.o.AOCcur), sizeof(struct FacAOCCurrency));
			return 0;
		case Fac_AOCDChargingUnit:
		case Fac_AOCEChargingUnit:
			fac->Function = pc.u.inv.operationValue;
			memcpy(&(fac->u.AOCchu), &(pc.u.inv.o.AOCchu), sizeof(struct FacAOCChargingUnit));
			return 0;

		case Fac_StatusRequest:
			fac->u.StatusRequest.InvokeID = pc.u.inv.invokeId;
			fac->u.StatusRequest.ComponentType = FacComponent_Invoke;
			fac->u.StatusRequest.Component.Invoke = pc.u.inv.o.StatusRequest;
			return 0;

			/* CCBS/CCNR support */
		case Fac_CallInfoRetain:
			fac->u.CallInfoRetain = pc.u.inv.o.CallInfoRetain;
			return 0;
		case Fac_EraseCallLinkageID:
			fac->u.EraseCallLinkageID = pc.u.inv.o.EraseCallLinkageID;
			return 0;
		case Fac_CCBSDeactivate:
			fac->u.CCBSDeactivate.InvokeID = pc.u.inv.invokeId;
			fac->u.CCBSDeactivate.ComponentType = FacComponent_Invoke;
			fac->u.CCBSDeactivate.Component.Invoke = pc.u.inv.o.CCBSDeactivate;
			return 0;
		case Fac_CCBSErase:
			fac->u.CCBSErase = pc.u.inv.o.CCBSErase;
			return 0;
		case Fac_CCBSRemoteUserFree:
			fac->u.CCBSRemoteUserFree = pc.u.inv.o.CCBSRemoteUserFree;
			return 0;
		case Fac_CCBSCall:
			fac->u.CCBSCall = pc.u.inv.o.CCBSCall;
			return 0;
		case Fac_CCBSStatusRequest:
			fac->u.CCBSStatusRequest.InvokeID = pc.u.inv.invokeId;
			fac->u.CCBSStatusRequest.ComponentType = FacComponent_Invoke;
			fac->u.CCBSStatusRequest.Component.Invoke = pc.u.inv.o.CCBSStatusRequest;
			return 0;
		case Fac_CCBSBFree:
			fac->u.CCBSBFree = pc.u.inv.o.CCBSBFree;
			return 0;
		case Fac_CCBSStopAlerting:
			fac->u.CCBSStopAlerting = pc.u.inv.o.CCBSStopAlerting;
			return 0;
		case Fac_CCBSRequest:
			fac->u.CCBSRequest.InvokeID = pc.u.inv.invokeId;
			fac->u.CCBSRequest.ComponentType = FacComponent_Invoke;
			fac->u.CCBSRequest.Component.Invoke = pc.u.inv.o.CCBSRequest;
			return 0;
		case Fac_CCBSInterrogate:
			fac->u.CCBSInterrogate.InvokeID = pc.u.inv.invokeId;
			fac->u.CCBSInterrogate.ComponentType = FacComponent_Invoke;
			fac->u.CCBSInterrogate.Component.Invoke = pc.u.inv.o.CCBSInterrogate;
			return 0;
		case Fac_CCNRRequest:
			fac->u.CCNRRequest.InvokeID = pc.u.inv.invokeId;
			fac->u.CCNRRequest.ComponentType = FacComponent_Invoke;
			fac->u.CCNRRequest.Component.Invoke = pc.u.inv.o.CCNRRequest;
			return 0;
		case Fac_CCNRInterrogate:
			fac->u.CCNRInterrogate.InvokeID = pc.u.inv.invokeId;
			fac->u.CCNRInterrogate.ComponentType = FacComponent_Invoke;
			fac->u.CCNRInterrogate.Component.Invoke = pc.u.inv.o.CCNRInterrogate;
			return 0;

			/* CCBS-T/CCNR-T support */
		case Fac_CCBS_T_Call:
			fac->u.CCBS_T_Call.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_CCBS_T_Suspend:
			fac->u.CCBS_T_Suspend.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_CCBS_T_Resume:
			fac->u.CCBS_T_Resume.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_CCBS_T_RemoteUserFree:
			fac->u.CCBS_T_RemoteUserFree.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_CCBS_T_Available:
			fac->u.CCBS_T_Available.InvokeID = pc.u.inv.invokeId;
			return 0;
		case Fac_CCBS_T_Request:
			fac->u.CCBS_T_Request.InvokeID = pc.u.inv.invokeId;
			fac->u.CCBS_T_Request.ComponentType = FacComponent_Invoke;
			fac->u.CCBS_T_Request.Component.Invoke = pc.u.inv.o.CCBS_T_Request;
			return 0;
		case Fac_CCNR_T_Request:
			fac->u.CCNR_T_Request.InvokeID = pc.u.inv.invokeId;
			fac->u.CCNR_T_Request.ComponentType = FacComponent_Invoke;
			fac->u.CCNR_T_Request.Component.Invoke = pc.u.inv.o.CCNR_T_Request;
			return 0;

		default:
			fac->Function = pc.u.inv.operationValue;
			break;
		}		/* end switch */
		break;

/* ------------------------------------------------------------------- */

	case returnResult:
		if (!pc.u.retResult.operationValuePresent) {
			fac->Function = Fac_RESULT;
			fac->u.RESULT.InvokeID = pc.u.retResult.invokeId;
			return 0;
		}
		fac->Function = pc.u.retResult.operationValue;
		switch (pc.u.retResult.operationValue) {
			/* Diversion support */
		case Fac_ActivationDiversion:
			fac->u.ActivationDiversion.InvokeID = pc.u.retResult.invokeId;
			fac->u.ActivationDiversion.ComponentType = FacComponent_Result;
			return 0;
		case Fac_DeactivationDiversion:
			fac->u.DeactivationDiversion.InvokeID = pc.u.retResult.invokeId;
			fac->u.DeactivationDiversion.ComponentType = FacComponent_Result;
			return 0;
		case Fac_ActivationStatusNotificationDiv:
		case Fac_DeactivationStatusNotificationDiv:
			break;
		case Fac_InterrogationDiversion:
			fac->u.InterrogationDiversion.InvokeID = pc.u.retResult.invokeId;
			fac->u.InterrogationDiversion.ComponentType = FacComponent_Result;
			fac->u.InterrogationDiversion.Component.Result = pc.u.retResult.o.InterrogationDiversion;
			return 0;
		case Fac_DiversionInformation:
			break;
		case Fac_CallDeflection:
			fac->u.CallDeflection.InvokeID = pc.u.retResult.invokeId;
			fac->u.CallDeflection.ComponentType = FacComponent_Result;
			return 0;
		case Fac_CallRerouteing:
			fac->u.CallRerouteing.InvokeID = pc.u.retResult.invokeId;
			fac->u.CallRerouteing.ComponentType = FacComponent_Result;
			return 0;
		case Fac_InterrogateServedUserNumbers:
			fac->u.InterrogateServedUserNumbers.InvokeID = pc.u.retResult.invokeId;
			fac->u.InterrogateServedUserNumbers.ComponentType = FacComponent_Result;
			fac->u.InterrogateServedUserNumbers.Component.Result = pc.u.retResult.o.InterrogateServedUserNumbers;
			return 0;
		case Fac_DivertingLegInformation1:
		case Fac_DivertingLegInformation2:
		case Fac_DivertingLegInformation3:
			break;

			/* ECT support */
		case Fac_EctExecute:
		case Fac_ExplicitEctExecute:
		case Fac_RequestSubaddress:
		case Fac_SubaddressTransfer:
			fac->Function = pc.u.inv.operationValue;
			break;
		case Fac_EctLinkIdRequest:
			fac->u.EctLinkIdRequest.InvokeID = pc.u.retResult.invokeId;
			fac->u.EctLinkIdRequest.ComponentType = FacComponent_Result;
			fac->u.EctLinkIdRequest.Component.Result = pc.u.retResult.o.EctLinkIdRequest;
			return 0;
		case Fac_EctInform:
			break;
		case Fac_EctLoopTest:
			fac->u.EctLoopTest.InvokeID = pc.u.retResult.invokeId;
			fac->u.EctLoopTest.ComponentType = FacComponent_Result;
			fac->u.EctLoopTest.Component.Result = pc.u.retResult.o.EctLoopTest;
			return 0;

			/* AOC support */
		case Fac_ChargingRequest:
		case Fac_AOCSCurrency:
		case Fac_AOCSSpecialArr:
		case Fac_AOCDCurrency:
		case Fac_AOCDChargingUnit:
		case Fac_AOCECurrency:
		case Fac_AOCEChargingUnit:
			break;

		case Fac_StatusRequest:
			fac->u.StatusRequest.InvokeID = pc.u.retResult.invokeId;
			fac->u.StatusRequest.ComponentType = FacComponent_Result;
			fac->u.StatusRequest.Component.Result = pc.u.retResult.o.StatusRequest;
			return 0;

			/* CCBS/CCNR support */
		case Fac_CallInfoRetain:
		case Fac_EraseCallLinkageID:
			break;
		case Fac_CCBSDeactivate:
			fac->u.CCBSDeactivate.InvokeID = pc.u.retResult.invokeId;
			fac->u.CCBSDeactivate.ComponentType = FacComponent_Result;
			return 0;
		case Fac_CCBSErase:
		case Fac_CCBSRemoteUserFree:
		case Fac_CCBSCall:
			break;
		case Fac_CCBSStatusRequest:
			fac->u.CCBSStatusRequest.InvokeID = pc.u.retResult.invokeId;
			fac->u.CCBSStatusRequest.ComponentType = FacComponent_Result;
			fac->u.CCBSStatusRequest.Component.Result = pc.u.retResult.o.CCBSStatusRequest;
			return 0;
		case Fac_CCBSBFree:
		case Fac_CCBSStopAlerting:
			break;
		case Fac_CCBSRequest:
			fac->u.CCBSRequest.InvokeID = pc.u.retResult.invokeId;
			fac->u.CCBSRequest.ComponentType = FacComponent_Result;
			fac->u.CCBSRequest.Component.Result = pc.u.retResult.o.CCBSRequest;
			return 0;
		case Fac_CCBSInterrogate:
			fac->u.CCBSInterrogate.InvokeID = pc.u.retResult.invokeId;
			fac->u.CCBSInterrogate.ComponentType = FacComponent_Result;
			fac->u.CCBSInterrogate.Component.Result = pc.u.retResult.o.CCBSInterrogate;
			return 0;
		case Fac_CCNRRequest:
			fac->u.CCNRRequest.InvokeID = pc.u.retResult.invokeId;
			fac->u.CCNRRequest.ComponentType = FacComponent_Result;
			fac->u.CCNRRequest.Component.Result = pc.u.retResult.o.CCNRRequest;
			return 0;
		case Fac_CCNRInterrogate:
			fac->u.CCNRInterrogate.InvokeID = pc.u.retResult.invokeId;
			fac->u.CCNRInterrogate.ComponentType = FacComponent_Result;
			fac->u.CCNRInterrogate.Component.Result = pc.u.retResult.o.CCNRInterrogate;
			return 0;

			/* CCBS-T/CCNR-T support */
		case Fac_CCBS_T_Call:
		case Fac_CCBS_T_Suspend:
		case Fac_CCBS_T_Resume:
		case Fac_CCBS_T_RemoteUserFree:
		case Fac_CCBS_T_Available:
			break;
		case Fac_CCBS_T_Request:
			fac->u.CCBS_T_Request.InvokeID = pc.u.inv.invokeId;
			fac->u.CCBS_T_Request.ComponentType = FacComponent_Result;
			fac->u.CCBS_T_Request.Component.Result = pc.u.retResult.o.CCBS_T_Request;
			return 0;
		case Fac_CCNR_T_Request:
			fac->u.CCNR_T_Request.InvokeID = pc.u.inv.invokeId;
			fac->u.CCNR_T_Request.ComponentType = FacComponent_Result;
			fac->u.CCNR_T_Request.Component.Result = pc.u.retResult.o.CCNR_T_Request;
			return 0;

		default:
			break;
		}		/* end switch */
		break;

/* ------------------------------------------------------------------- */

	case returnError:
		fac->Function = Fac_ERROR;
		fac->u.ERROR.InvokeID = pc.u.retError.invokeId;
		fac->u.ERROR.errorValue = pc.u.retError.errorValue;
		return 0;

/* ------------------------------------------------------------------- */

	case reject:
		fac->Function = Fac_REJECT;
		fac->u.REJECT.InvokeIDPresent = pc.u.reject.invokeIdPresent;
		fac->u.REJECT.InvokeID = pc.u.reject.invokeId;
		fac->u.REJECT.Code = FAC_REJECT_BASE(pc.u.reject.problem) + pc.u.reject.problemValue;
		return 0;
	}			/* end switch */

 _dec_err:
	fac->Function = Fac_None;
	return -1;
}				/* end decodeFac() */
