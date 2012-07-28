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
#include "diversion.h"
#include <mISDN/q931.h>
#include "ccbs.h"
#include "ect.h"
#include <string.h>

#define AOC_MAX_NUM_CHARGE_UNITS   0xFFFFFF
#define AOCE_CHARGE_UNIT_IE_LENGTH 17
#define AOCD_CHARGE_UNIT_IE_LENGTH 20

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
__u8 *encodeComponentInvoke_Head(__u8 * Dest, int InvokeID, enum Operation OperationValue)
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
__u8 *encodeComponentInvoke_Head_Long_u8(__u8 * Dest, int InvokeID, enum Operation OperationValue)
{
	__u8 *p;

	p = encodeComponent_Head_Long_u8(Dest, asn1ComponentTag_Invoke);
	p += encodeInt(p, ASN1_TAG_INTEGER, InvokeID);
	//p += encodeInt(p, ASN1_TAG_CONTEXT_SPECIFIC | 0, LinkedID); /* Optional */
	p += encodeOperationValue(p, OperationValue);
	return p;
}				/* end encodeComponentInvoke_Head_Long_u8() */

static int encodeAOCDChargingUnitOperation(__u8 * dest, const struct asn1_parm *pc, int numberOfUnits)
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
		p[i++] = pc->u.inv.invokeId;
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
		p[i] = numberOfUnits & 0xFF;
		if (len > 1)
			p[i+1] = (numberOfUnits >> 8) & 0xFF;
		if (len > 2)
			p[i+2] = (numberOfUnits >> 16) & 0xFF;
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

static int encodeAOCEChargingUnitOperation(__u8 * dest, const struct asn1_parm *pc, int numberOfUnits)
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
		p[i++] = pc->u.inv.invokeId;
		p[i++] = 0x02;	// Operation Tag
		p[i++] = 0x01;	// Tag Length
		p[i++] = Fac_AOCEChargingUnit;	// Operation Value
		p[i++] = 0xa1;	// APDU
		p[i++] = 0x30;
		p[i++] = (0x09 + len);	// Length
		p[i++] = 0xa1;	// APDU
		p[i++] = (0x04 + len);	// Length
		p[i++] = 0x30;
		p[i++] = (0x02 + len);
		p[i++] = 0x02;
		p[i++] = 0x02;	// AOC-E so Total

		// Recorded units could take up as much as 3 bytes (0xFFFFFF)
		p[i] = numberOfUnits & 0xFF;
		if (len > 1)
			p[i+1] = (numberOfUnits >> 8) & 0xFF;
		if (len > 2)
			p[i+2] = (numberOfUnits >> 16) & 0xFF;
		i += len;

		p[1] = (AOCE_CHARGE_UNIT_IE_LENGTH + len);	// IE Payload Length
		p[4] = p[1] - 3;	// Invoke Component Length

		result = p[1] + 2;	// Total Length of IE
	}
	return result;
}

static int encodeFacMcidIvokeRequest(__u8 * dest, const struct asn1_parm *pc)
{
	int i = 0;
	int result = -1;
	unsigned char invokeId = 0;

	__u8 *p = dest;

	if (p != NULL) {
		invokeId = pc->u.inv.invokeId;

		p[i++] = IE_FACILITY;	// IE identifier
		p[i++] = 0x09;	// length
		p[i++] = SUPPLEMENTARY_SERVICE;	// remote operations protocol
		p[i++] = 0xa1;	// invoke component
		p[i++] = 0x06;	// Manually Encoding the MCID
		p[i++] = 0x02;	// Invoke ID
		p[i++] = 0x01;	// InvokeId Length
		p[i++] = pc->u.inv.invokeId;	// Invoke value
		p[i++] = 0x02;	// Operation Tag
		p[i++] = 0x01;	// Tag Length
		p[i++] = Fac_MaliciousCallId;	// Operation Value

		result = p[1] + 2;	// Total IE Length
	}
	return result;
}

#if 0
	case Fac_RESULT:
		len = encodeFacRESULT(dest, pc, &inv->o.RESULT);
		break;
	case Fac_ERROR:
		len = encodeFacERROR(dest, pc, &inv->o.ERROR);
		break;
	case Fac_REJECT:
		len = encodeFacREJECT(dest, pc, &inv->o.REJECT);
		break;
#endif
/*
 * Facility IE Encoding
 */
static int encodeFacReturnResult(__u8 * dest, const struct asn1_parm *pc)
{
        const struct asn1ReturnResult *res = &pc->u.retResult;
        int len = -1;
        char ops[20];
        __u8 *p;

        if (res->operationValuePresent)
                sprintf(ops,"operation 0x%04x", res->operationValue);
        else
                sprintf(ops, "no operation value");

        dprint(DBGM_ASN1_ENC, "Return result %s Id:%d start encoding\n", ops, res->invokeId);
        if (res->operationValuePresent) {
                switch (res->operationValue) {
		case Fac_Begin3PTY:
			len = encodeFacBegin3PTY(dest, pc, NULL);
			break;
		case Fac_End3PTY:
			len = encodeFacEnd3PTY(dest, pc, NULL);
			break;
                case Fac_StatusRequest:
			len = encodeFacStatusRequest(dest, pc, NULL);
			break;
		case Fac_CCBSStatusRequest:
			len = encodeFacCCBSStatusRequest(dest, pc, NULL);
			break;
		case Fac_CCBSRequest:
			len = encodeFacCCBSRequest(dest, pc, NULL);
			break;
		case Fac_CCBSInterrogate:
			len = encodeFacCCBSInterrogate(dest, pc, NULL);
			break;
		case Fac_CCNRRequest:
			len = encodeFacCCNRRequest(dest, pc, NULL);
			break;
		case Fac_CCNRInterrogate:
			len = encodeFacCCNRInterrogate(dest, pc, NULL);
			break;
                case Fac_CCBS_T_Request:
			len = encodeFacCCBS_T_Request(dest, pc, NULL);
			break;
                case Fac_CCNR_T_Request:
			len = encodeFacCCNR_T_Request(dest, pc, NULL);
			break;
                case Fac_EctLinkIdRequest:
			len = encodeFacEctLinkIdRequest(dest, pc, NULL);
			break;
                case Fac_EctLoopTest:
			len = encodeFacEctLoopTest(dest, pc, NULL);
			break;
                case Fac_InterrogationDiversion:
			len = encodeFacInterrogationDiversion(dest, pc, NULL);
			break;
                case Fac_InterrogateServedUserNumbers:
			len = encodeFacInterrogateServedUserNumbers(dest, pc, &res->o.InterrogateServedUserNumbers);
			break;

                default:
		        eprint("ReturnResult Function %s not supported yet\n", ops);
		        return -1;
                }
	} else {
		p = encodeComponent_Head(dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, res->invokeId);
		len = encodeComponent_Length(dest, p);
	}

	if (len < 0)
		eprint("Error on encoding ReturnResult %s\n", ops);
	else {
		dprint(DBGM_ASN1_ENC, "ReturnResult %s encoded in %d bytes\n", ops, len);
		dhexprint(DBGM_ASN1_DATA, "Facility:", dest, len);
	}
	return len;
}

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
static int encodeFacReturnError(__u8 * dest, const struct asn1_parm *pc)
{
        __u8 *p;
        const struct asn1ReturnError *err = &pc->u.retError;
        int len = -1;

        dprint(DBGM_ASN1_ENC, "Return error Id:%d errorValue 0x%x start encoding\n", err->invokeId, err->errorValue);


	p = encodeComponent_Head(dest, asn1ComponentTag_Error);
	p += encodeInt(p, ASN1_TAG_INTEGER, err->invokeId);
	p += encodeErrorValue(p, err->errorValue);
	len = encodeComponent_Length(dest, p);

	dprint(DBGM_ASN1_ENC, "ReturnError encoded in %d bytes\n", len);
	dhexprint(DBGM_ASN1_DATA, "Facility:", dest, len);
	return len;
}

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
static int encodeFacReject(__u8 * dest, const struct asn1_parm *pc)
{
        struct asn1Reject *rej = &pc->u.reject;
        int len = -1;
        char ids[16];
	__u8 *p;

        if (rej->invokeIdPresent)
                sprintf(ids,"ID 0x%04x", rej->invokeId);
        else
                sprintf(ids, "no invoke ID");

        dprint(DBGM_ASN1_ENC, "Reject %s problem %d value 0x%x start encoding\n", ids, rej->problem, rej->problemValue);

	p = encodeComponent_Head(dest, asn1ComponentTag_Reject);

	if (rej->invokeIdPresent)
		p += encodeInt(p, ASN1_TAG_INTEGER, rej->invokeId);
	else
		p += encodeNull(p, ASN1_TAG_NULL);

	p += encodeInt(p, ASN1_TAG_CONTEXT_SPECIFIC | rej->problem, rej->problemValue);

	len = encodeComponent_Length(dest, p);

	dprint(DBGM_ASN1_ENC, "Reject encoded in %d bytes\n", len);
	dhexprint(DBGM_ASN1_DATA, "Facility:", dest, len);
	return len;
}

static int encodeFacInvoke(__u8 * dest, const struct asn1_parm *pc)
{
        const struct asn1Invoke *inv = &pc->u.inv;
	int len = -1;

	dprint(DBGM_ASN1_ENC, "Invoke operation 0x%x Id:%d start encoding\n", inv->operationValue, inv->invokeId);
	switch (inv->operationValue) {
	case Fac_None:
		break;

	/* Diversion support */
	case Fac_ActivationDiversion:
		len = encodeFacActivationDiversion(dest, pc, &inv->o.ActivationDiversion);
		break;
	case Fac_Begin3PTY:
		len = encodeFacBegin3PTY(dest, pc, NULL);
		break;
	case Fac_End3PTY:
		len = encodeFacEnd3PTY(dest, pc, NULL);
		break;
	case Fac_DeactivationDiversion:
		len = encodeFacDeactivationDiversion(dest, pc, &inv->o.DeactivationDiversion);
		break;
	case Fac_ActivationStatusNotificationDiv:
		len = encodeFacActivationStatusNotificationDiv(dest, pc, &inv->o.ActivationStatusNotificationDiv);
		break;
	case Fac_DeactivationStatusNotificationDiv:
		len = encodeFacDeactivationStatusNotificationDiv(dest, pc, &inv->o.DeactivationStatusNotificationDiv);
		break;
	case Fac_InterrogationDiversion:
		len = encodeFacInterrogationDiversion(dest, pc, &inv->o.InterrogationDiversion);
		break;
	case Fac_DiversionInformation:
		len = encodeFacDiversionInformation(dest, pc, &inv->o.DiversionInformation);
		break;
	case Fac_CallDeflection:
		len = encodeFacCallDeflection(dest, pc, &inv->o.CallDeflection);
		break;
	case Fac_CallRerouteing:
		len = encodeFacCallRerouteing(dest, pc, &inv->o.CallRerouteing);
		break;
	case Fac_InterrogateServedUserNumbers:
		len = encodeFacInterrogateServedUserNumbers(dest, pc, NULL);
		break;
	case Fac_DivertingLegInformation1:
		len = encodeFacDivertingLegInformation1(dest, pc, &inv->o.DivertingLegInformation1);
		break;
	case Fac_DivertingLegInformation2:
		len = encodeFacDivertingLegInformation2(dest, pc, &inv->o.DivertingLegInformation2);
		break;
	case Fac_DivertingLegInformation3:
		len = encodeFacDivertingLegInformation3(dest, pc, &inv->o.DivertingLegInformation3);
		break;

		/* ECT support */
	case Fac_EctExecute:
		len = encodeFacEctExecute(dest, pc, NULL);
		break;
	case Fac_ExplicitEctExecute:
		len = encodeFacExplicitEctExecute(dest, pc, &inv->o.ExplicitEctExecute);
		break;
	case Fac_RequestSubaddress:
		len = encodeFacRequestSubaddress(dest, pc, NULL);
		break;
	case Fac_SubaddressTransfer:
		len = encodeFacSubaddressTransfer(dest, pc, &inv->o.SubaddressTransfer);
		break;
	case Fac_EctLinkIdRequest:
		len = encodeFacEctLinkIdRequest(dest, pc, NULL);
		break;
	case Fac_EctInform:
		len = encodeFacEctInform(dest, pc, &inv->o.EctInform);
		break;
	case Fac_EctLoopTest:
		len = encodeFacEctLoopTest(dest, pc, &inv->o.EctLoopTest);
		break;

		/* AOC support */
	case Fac_ChargingRequest:
	case Fac_AOCSCurrency:
	case Fac_AOCSSpecialArr:
	case Fac_AOCDCurrency:
	case Fac_AOCECurrency:
		break;
	case Fac_AOCDChargingUnit:
		len = encodeAOCDChargingUnitOperation(dest, pc, inv->o.AOCchu.recordedUnits);
		break;
	case Fac_AOCEChargingUnit:
		len = encodeAOCEChargingUnitOperation(dest, pc, inv->o.AOCchu.recordedUnits);
		break;
/* Malicious Call Tag Support */
	case Fac_MaliciousCallId:
		len = encodeFacMcidIvokeRequest(dest, pc);
		break;
	case Fac_StatusRequest:
		len = encodeFacStatusRequest(dest, pc, &inv->o.StatusRequest);
		break;

		/* CCBS/CCNR support */
	case Fac_CallInfoRetain:
		len = encodeFacCallInfoRetain(dest, pc, &inv->o.CallInfoRetain);
		break;
	case Fac_EraseCallLinkageID:
		len = encodeFacEraseCallLinkageID(dest, pc, &inv->o.EraseCallLinkageID);
		break;
	case Fac_CCBSDeactivate:
		len = encodeFacCCBSDeactivate(dest, pc, &inv->o.CCBSDeactivate);
		break;
	case Fac_CCBSErase:
		len = encodeFacCCBSErase(dest, pc, &inv->o.CCBSErase);
		break;
	case Fac_CCBSRemoteUserFree:
		len = encodeFacCCBSRemoteUserFree(dest, pc, &inv->o.CCBSRemoteUserFree);
		break;
	case Fac_CCBSCall:
		len = encodeFacCCBSCall(dest, pc, &inv->o.CCBSCall);
		break;
	case Fac_CCBSStatusRequest:
		len = encodeFacCCBSStatusRequest(dest, pc, &inv->o.CCBSStatusRequest);
		break;
	case Fac_CCBSBFree:
		len = encodeFacCCBSBFree(dest, pc, &inv->o.CCBSBFree);
		break;
	case Fac_CCBSStopAlerting:
		len = encodeFacCCBSStopAlerting(dest, pc, &inv->o.CCBSStopAlerting);
		break;
	case Fac_CCBSRequest:
		len = encodeFacCCBSRequest(dest, pc, &inv->o.CCBSRequest);
		break;
	case Fac_CCBSInterrogate:
		len = encodeFacCCBSInterrogate(dest, pc, &inv->o.CCBSInterrogate);
		break;
	case Fac_CCNRRequest:
		len = encodeFacCCNRRequest(dest, pc, &inv->o.CCNRRequest);
		break;
	case Fac_CCNRInterrogate:
		len = encodeFacCCNRInterrogate(dest, pc, &inv->o.CCNRInterrogate);
		break;

		/* CCBS-T/CCNR-T support */
	case Fac_CCBS_T_Call:
		len = encodeFacCCBS_T_Call(dest, pc, NULL);
		break;
	case Fac_CCBS_T_Suspend:
		len = encodeFacCCBS_T_Suspend(dest, pc, NULL);
		break;
	case Fac_CCBS_T_Resume:
		len = encodeFacCCBS_T_Resume(dest, pc, NULL);
		break;
	case Fac_CCBS_T_RemoteUserFree:
		len = encodeFacCCBS_T_RemoteUserFree(dest, pc, NULL);
		break;
	case Fac_CCBS_T_Available:
		len = encodeFacCCBS_T_Available(dest, pc, NULL);
		break;
	case Fac_CCBS_T_Request:
		len = encodeFacCCBS_T_Request(dest, pc, &inv->o.CCBS_T_Request);
		break;
	case Fac_CCNR_T_Request:
		len = encodeFacCCNR_T_Request(dest, pc, &inv->o.CCNR_T_Request);
		break;

	default:
		eprint("Function %d not supported yet\n", inv->operationValue);
		return -1;
	}
	if (len < 0)
		eprint("Error on encoding function 0x%x\n", inv->operationValue);
	else {
		dprint(DBGM_ASN1_ENC, "Function 0x%x encoded in %d bytes\n", inv->operationValue, len);
		dhexprint(DBGM_ASN1_DATA, "Facility:", dest, len);
	}
	return len;
}

int encodeFac(__u8 * dest, struct asn1_parm *ap)
{
        int len = -1;

        if (ap->Valid) {
                switch (ap->comp) {
                case CompInvoke:
                        len = encodeFacInvoke(dest, ap);
                        break;
                case CompReturnResult:
                        len = encodeFacReturnResult(dest, ap);
                        break;
                case CompReturnError:
                        len = encodeFacReturnError(dest, ap);
                        break;
                case CompReject:
                        len = encodeFacReject(dest, ap);
                        break;
                default:
                        eprint("Unknown component 0x%x\n", ap->comp);
                        break;
                }
        } else
                eprint("Facility struct component(%d) not marked as valid\n", ap->comp);
        return len;
}

/*
 * Facility IE Decoding
 */
int decodeFac(__u8 * src, struct asn1_parm *ap)
{
	unsigned fac_len;
	__u8 *end;
	__u8 *p = src;

	if (!p) {
		goto _dec_err;
	}

	fac_len = *p++;
	end = p + fac_len;

	if (*p++ != SUPPLEMENTARY_SERVICE) {
		goto _dec_err;
	}

	memset(ap, 0, sizeof(*ap));
	if (ParseComponent(ap, p, end) == -1) {
		goto _dec_err;
	}

	switch (ap->comp) {
	case CompInvoke:
		switch (ap->u.inv.operationValue) {
                /* Diversion support */
		case Fac_ActivationDiversion:
		case Fac_DeactivationDiversion:
		case Fac_ActivationStatusNotificationDiv:
		case Fac_DeactivationStatusNotificationDiv:
		case Fac_InterrogationDiversion:
		case Fac_DiversionInformation:
		case Fac_CallDeflection:
		case Fac_CallRerouteing:
		case Fac_InterrogateServedUserNumbers:
		case Fac_DivertingLegInformation1:
		case Fac_DivertingLegInformation2:
		case Fac_DivertingLegInformation3:
		/* ECT support */
		case Fac_EctExecute:
		case Fac_ExplicitEctExecute:
		case Fac_RequestSubaddress:
		case Fac_SubaddressTransfer:
		case Fac_EctLinkIdRequest:
		case Fac_EctInform:
		case Fac_EctLoopTest:
		/* AOC support */
		case Fac_AOCDCurrency:
		case Fac_AOCECurrency:
		case Fac_AOCDChargingUnit:
		case Fac_AOCEChargingUnit:
		case Fac_StatusRequest:
		/* CCBS/CCNR support */
		case Fac_CallInfoRetain:
		case Fac_EraseCallLinkageID:
		case Fac_CCBSDeactivate:
		case Fac_CCBSErase:
		case Fac_CCBSRemoteUserFree:
		case Fac_CCBSCall:
		case Fac_CCBSStatusRequest:
		case Fac_CCBSBFree:
		case Fac_CCBSStopAlerting:
		case Fac_CCBSRequest:
		case Fac_CCBSInterrogate:
		case Fac_CCNRRequest:
		case Fac_CCNRInterrogate:
		/* CCBS-T/CCNR-T support */
		case Fac_CCBS_T_Call:
		case Fac_CCBS_T_Suspend:
		case Fac_CCBS_T_Resume:
		case Fac_CCBS_T_RemoteUserFree:
		case Fac_CCBS_T_Available:
		case Fac_CCBS_T_Request:
		case Fac_CCNR_T_Request:
		        ap->Valid = 1;
		        return 0;
		default:
		        eprint("Unknown invoke operation %x\n", ap->u.inv.operationValue);
		        break;
		}
		break;

/* ------------------------------------------------------------------- */

	case CompReturnResult:
		if (!ap->u.retResult.operationValuePresent) {
			return 0;
		}
		switch (ap->u.retResult.operationValue) {
		/* Diversion support */
		case Fac_ActivationDiversion:
		case Fac_DeactivationDiversion:
		case Fac_InterrogationDiversion:
		case Fac_CallDeflection:
		case Fac_CallRerouteing:
		case Fac_InterrogateServedUserNumbers:
		        ap->Valid = 1;
			return 0;
		case Fac_ActivationStatusNotificationDiv:
		case Fac_DeactivationStatusNotificationDiv:
		case Fac_DiversionInformation:
		case Fac_DivertingLegInformation1:
		case Fac_DivertingLegInformation2:
		case Fac_DivertingLegInformation3:
			break;

		/* ECT support */
		case Fac_EctExecute:
		case Fac_ExplicitEctExecute:
		case Fac_RequestSubaddress:
		case Fac_SubaddressTransfer:
			break;
		case Fac_EctLinkIdRequest:
			return 0;
		case Fac_EctInform:
			break;
		case Fac_EctLoopTest:
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
			return 0;

			/* CCBS/CCNR support */
		case Fac_CallInfoRetain:
		case Fac_EraseCallLinkageID:
			break;
		case Fac_CCBSDeactivate:
			return 0;
		case Fac_CCBSErase:
		case Fac_CCBSRemoteUserFree:
		case Fac_CCBSCall:
			break;
		case Fac_CCBSStatusRequest:
			return 0;
		case Fac_CCBSBFree:
		case Fac_CCBSStopAlerting:
			break;
		case Fac_CCBSRequest:
		case Fac_CCBSInterrogate:
		case Fac_CCNRRequest:
		case Fac_CCNRInterrogate:
			return 0;

			/* CCBS-T/CCNR-T support */
		case Fac_CCBS_T_Call:
		case Fac_CCBS_T_Suspend:
		case Fac_CCBS_T_Resume:
		case Fac_CCBS_T_RemoteUserFree:
		case Fac_CCBS_T_Available:
			break;
		case Fac_CCBS_T_Request:
		case Fac_CCNR_T_Request:
			return 0;

		default:
			break;
		}		/* end switch */
		break;

/* ------------------------------------------------------------------- */

	case CompReturnError:
	        ap->Valid = 1;
		return 0;

/* ------------------------------------------------------------------- */

	case CompReject:
	        ap->Valid = 1;
		return 0;
	}			/* end switch */

 _dec_err:
	return -1;
}				/* end decodeFac() */
