/*
 * suppserv.h
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

#ifndef __SUPPSERV_H__
#define __SUPPSERV_H__

#define AST_MISDN_ENHANCEMENTS	1	/* Compile the mISDN user with Asterisk enhancements */

#include <asm/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

/*
 * Structs for Facility Messages
 */

#define FAC_OID_BASE(Base)			((Base) * 0x100)
	enum FacOIDBase {
		FacOIDBase_Local,	/* localValue base (Non-OID) (Must be first) */

		FacOIDBase_CCBS,
		FacOIDBase_CCNR,
		FacOIDBase_StatusRequest,
		FacOIDBase_ECT,
		FacOIDBase_CCBS_T,
		FacOIDBase_CCNR_T,

		/* Must be last in the list */
		FacOIDBase_Last
	};

/* Facility operation-value function code */
	enum FacFunction {
		Fac_None = 0xffff,
		Fac_ERROR = 0xfffe,
		Fac_RESULT = 0xfffd,	/* For when there is no operation-value in the result */
		Fac_REJECT = 0xfffc,	/* There was a problem with the facility ie */

#if defined(MISDN_NO_LONGER_IMPLEMENTED)	/* No longer implemented if they ever were implemented. */
		/* ??? Don't know where these are from. */
		Fac_GetSupportedServices = FAC_OID_BASE(FacOIDBase_Local) + 0,
		Fac_Listen = FAC_OID_BASE(FacOIDBase_Local) + 1,
		Fac_Suspend = FAC_OID_BASE(FacOIDBase_Local) + 4,
		Fac_Resume = FAC_OID_BASE(FacOIDBase_Local) + 5,

		/*
		 * localValue's from Diversion-Operations
		 * {ccitt identified-organization etsi(0) 207 operations-and-errors(1)}
		 *
		 * These names are not even used by the standard document.
		 */
		Fac_CFActivate = FAC_OID_BASE(FacOIDBase_Local) + 7,	/* Use Fac_ActivationDiversion */
		Fac_CFDeactivate = FAC_OID_BASE(FacOIDBase_Local) + 8,	/* Use Fac_DeactivationDiversion */
		Fac_CFInterrogateParameters = FAC_OID_BASE(FacOIDBase_Local) + 11,	/* Use Fac_InterrogationDiversion */
		Fac_CFInterrogateNumbers = FAC_OID_BASE(FacOIDBase_Local) + 12,	/* Use Fac_DiversionInformation */
#endif				/* defined(MISDN_NO_LONGER_IMPLEMENTED) */

		Fac_CD = FAC_OID_BASE(FacOIDBase_Local) + 13,	/* Use Fac_CallDeflection */

		/*
		 * Malicious Call Identification Operation Request
		 */
		Fac_MaliciousCallId = FAC_OID_BASE(FacOIDBase_Local) + 3,
		/*
		 * localValue's from Diversion-Operations
		 * {ccitt identified-organization etsi(0) 207 operations-and-errors(1)}
		 */
		Fac_ActivationDiversion = FAC_OID_BASE(FacOIDBase_Local) + 7,
		Fac_DeactivationDiversion = FAC_OID_BASE(FacOIDBase_Local) + 8,
		Fac_ActivationStatusNotificationDiv = FAC_OID_BASE(FacOIDBase_Local) + 9,
		Fac_DeactivationStatusNotificationDiv = FAC_OID_BASE(FacOIDBase_Local) + 10,
		Fac_InterrogationDiversion = FAC_OID_BASE(FacOIDBase_Local) + 11,
		Fac_DiversionInformation = FAC_OID_BASE(FacOIDBase_Local) + 12,
		Fac_CallDeflection = FAC_OID_BASE(FacOIDBase_Local) + 13,
		Fac_CallRerouteing = FAC_OID_BASE(FacOIDBase_Local) + 14,
		Fac_DivertingLegInformation2 = FAC_OID_BASE(FacOIDBase_Local) + 15,
		Fac_InterrogateServedUserNumbers = FAC_OID_BASE(FacOIDBase_Local) + 17,
		Fac_DivertingLegInformation1 = FAC_OID_BASE(FacOIDBase_Local) + 18,
		Fac_DivertingLegInformation3 = FAC_OID_BASE(FacOIDBase_Local) + 19,

		/*
		 * localValue's from Advice-of-Charge-Operations
		 * {ccitt identified-organization etsi (0) 182 operations-and-errors (1)}
		 *
		 * Advice-Of-Charge-at-call-Setup(AOCS)
		 * Advice-Of-Charge-During-the-call(AOCD)
		 * Advice-Of-Charge-at-the-End-of-the-call(AOCE)
		 */
		Fac_ChargingRequest = FAC_OID_BASE(FacOIDBase_Local) + 30,
		Fac_AOCSCurrency = FAC_OID_BASE(FacOIDBase_Local) + 31,
		Fac_AOCSSpecialArr = FAC_OID_BASE(FacOIDBase_Local) + 32,
		Fac_AOCDCurrency = FAC_OID_BASE(FacOIDBase_Local) + 33,
		Fac_AOCDChargingUnit = FAC_OID_BASE(FacOIDBase_Local) + 34,
		Fac_AOCECurrency = FAC_OID_BASE(FacOIDBase_Local) + 35,
		Fac_AOCEChargingUnit = FAC_OID_BASE(FacOIDBase_Local) + 36,

		/*
		 * localValue's from Explicit-Call-Transfer-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 369 operations-and-errors(1)}
		 */
		Fac_EctExecute = FAC_OID_BASE(FacOIDBase_Local) + 6,

		/*
		 * globalValue's (OIDs) from Explicit-Call-Transfer-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 369 operations-and-errors(1)}
		 */
		Fac_ExplicitEctExecute = FAC_OID_BASE(FacOIDBase_ECT) + 1,
		Fac_RequestSubaddress = FAC_OID_BASE(FacOIDBase_ECT) + 2,
		Fac_SubaddressTransfer = FAC_OID_BASE(FacOIDBase_ECT) + 3,
		Fac_EctLinkIdRequest = FAC_OID_BASE(FacOIDBase_ECT) + 4,
		Fac_EctInform = FAC_OID_BASE(FacOIDBase_ECT) + 5,
		Fac_EctLoopTest = FAC_OID_BASE(FacOIDBase_ECT) + 6,

		/*
		 * globalValue's (OIDs) from Status-Request-Procedure
		 * {itu-t identified-organization etsi(0) 196 status-request-procedure(9)}
		 */
		Fac_StatusRequest = FAC_OID_BASE(FacOIDBase_StatusRequest) + 1,

		/*
		 * globalValue's (OIDs) from CCBS-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 359 operations-and-errors(1)}
		 */
		Fac_CallInfoRetain = FAC_OID_BASE(FacOIDBase_CCBS) + 1,
		Fac_CCBSRequest = FAC_OID_BASE(FacOIDBase_CCBS) + 2,
		Fac_CCBSDeactivate = FAC_OID_BASE(FacOIDBase_CCBS) + 3,
		Fac_CCBSInterrogate = FAC_OID_BASE(FacOIDBase_CCBS) + 4,
		Fac_CCBSErase = FAC_OID_BASE(FacOIDBase_CCBS) + 5,
		Fac_CCBSRemoteUserFree = FAC_OID_BASE(FacOIDBase_CCBS) + 6,
		Fac_CCBSCall = FAC_OID_BASE(FacOIDBase_CCBS) + 7,
		Fac_CCBSStatusRequest = FAC_OID_BASE(FacOIDBase_CCBS) + 8,
		Fac_CCBSBFree = FAC_OID_BASE(FacOIDBase_CCBS) + 9,
		Fac_EraseCallLinkageID = FAC_OID_BASE(FacOIDBase_CCBS) + 10,
		Fac_CCBSStopAlerting = FAC_OID_BASE(FacOIDBase_CCBS) + 11,

		/*
		 * globalValue's (OIDs) from CCBS-private-networks-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 359 private-networks-operations-and-errors(2)}
		 */
		Fac_CCBS_T_Request = FAC_OID_BASE(FacOIDBase_CCBS_T) + 1,
		Fac_CCBS_T_Call = FAC_OID_BASE(FacOIDBase_CCBS_T) + 2,
		Fac_CCBS_T_Suspend = FAC_OID_BASE(FacOIDBase_CCBS_T) + 3,
		Fac_CCBS_T_Resume = FAC_OID_BASE(FacOIDBase_CCBS_T) + 4,
		Fac_CCBS_T_RemoteUserFree = FAC_OID_BASE(FacOIDBase_CCBS_T) + 5,
		Fac_CCBS_T_Available = FAC_OID_BASE(FacOIDBase_CCBS_T) + 6,

		/*
		 * globalValue's (OIDs) from CCNR-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 1065 operations-and-errors(1)}
		 */
		Fac_CCNRRequest = FAC_OID_BASE(FacOIDBase_CCNR) + 1,
		Fac_CCNRInterrogate = FAC_OID_BASE(FacOIDBase_CCNR) + 2,

		/*
		 * globalValue's (OIDs) from CCNR-private-networks-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 1065 private-networks-operations-and-errors(2)}
		 */
		Fac_CCNR_T_Request = FAC_OID_BASE(FacOIDBase_CCNR_T) + 1,
	};

	enum FacErrorCode {
		FacError_None = 0xFFFF,	/* No error occurred */
		FacError_Unknown = 0xFFFE,	/* Unknown OID error code */

		/*
		 * localValue Errors from General-Errors
		 * {ccitt identified-organization etsi(0) 196 general-errors(2)}
		 */
		FacError_Gen_NotSubscribed = FAC_OID_BASE(FacOIDBase_Local) + 0,
		FacError_Gen_NotAvailable = FAC_OID_BASE(FacOIDBase_Local) + 3,
		FacError_Gen_NotImplemented = FAC_OID_BASE(FacOIDBase_Local) + 4,
		FacError_Gen_InvalidServedUserNr = FAC_OID_BASE(FacOIDBase_Local) + 6,
		FacError_Gen_InvalidCallState = FAC_OID_BASE(FacOIDBase_Local) + 7,
		FacError_Gen_BasicServiceNotProvided = FAC_OID_BASE(FacOIDBase_Local) + 8,
		FacError_Gen_NotIncomingCall = FAC_OID_BASE(FacOIDBase_Local) + 9,
		FacError_Gen_SupplementaryServiceInteractionNotAllowed = FAC_OID_BASE(FacOIDBase_Local) + 10,
		FacError_Gen_ResourceUnavailable = FAC_OID_BASE(FacOIDBase_Local) + 11,

		/*
		 * localValue Errors from Diversion-Operations
		 * {ccitt identified-organization etsi(0) 207 operations-and-errors(1)}
		 */
		FacError_Div_InvalidDivertedToNr = FAC_OID_BASE(FacOIDBase_Local) + 12,
		FacError_Div_SpecialServiceNr = FAC_OID_BASE(FacOIDBase_Local) + 14,
		FacError_Div_DiversionToServedUserNr = FAC_OID_BASE(FacOIDBase_Local) + 15,
		FacError_Div_IncomingCallAccepted = FAC_OID_BASE(FacOIDBase_Local) + 23,
		FacError_Div_NumberOfDiversionsExceeded = FAC_OID_BASE(FacOIDBase_Local) + 24,
		FacError_Div_NotActivated = FAC_OID_BASE(FacOIDBase_Local) + 46,
		FacError_Div_RequestAlreadyAccepted = FAC_OID_BASE(FacOIDBase_Local) + 48,

		/*
		 * localValue Errors from Advice-of-Charge-Operations
		 * {ccitt identified-organization etsi (0) 182 operations-and-errors (1)}
		 */
		FacError_AOC_NoChargingInfoAvailable = FAC_OID_BASE(FacOIDBase_Local) + 26,

		/*
		 * globalValue Errors (OIDs) from CCBS-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 359 operations-and-errors(1)}
		 */
		FacError_CCBS_InvalidCallLinkageID = FAC_OID_BASE(FacOIDBase_CCBS) + 20,
		FacError_CCBS_InvalidCCBSReference = FAC_OID_BASE(FacOIDBase_CCBS) + 21,
		FacError_CCBS_LongTermDenial = FAC_OID_BASE(FacOIDBase_CCBS) + 22,
		FacError_CCBS_ShortTermDenial = FAC_OID_BASE(FacOIDBase_CCBS) + 23,
		FacError_CCBS_IsAlreadyActivated = FAC_OID_BASE(FacOIDBase_CCBS) + 24,
		FacError_CCBS_AlreadyAccepted = FAC_OID_BASE(FacOIDBase_CCBS) + 25,
		FacError_CCBS_OutgoingCCBSQueueFull = FAC_OID_BASE(FacOIDBase_CCBS) + 26,
		FacError_CCBS_CallFailureReasonNotBusy = FAC_OID_BASE(FacOIDBase_CCBS) + 27,
		FacError_CCBS_NotReadyForCall = FAC_OID_BASE(FacOIDBase_CCBS) + 28,

		/*
		 * globalValue Errors (OIDs) from CCBS-private-networks-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 359 private-networks-operations-and-errors(2)}
		 */
		FacError_CCBS_T_LongTermDenial = FAC_OID_BASE(FacOIDBase_CCBS_T) + 20,
		FacError_CCBS_T_ShortTermDenial = FAC_OID_BASE(FacOIDBase_CCBS_T) + 21,

		/*
		 * globalValue Errors (OIDs) from Explicit-Call-Transfer-Operations-and-Errors
		 * {ccitt identified-organization etsi(0) 369 operations-and-errors(1)}
		 */
		FacError_ECT_LinkIdNotAssignedByNetwork = FAC_OID_BASE(FacOIDBase_ECT) + 21,
	};

	struct FacERROR {
		__s16 InvokeID;
		/*! \see enum FacErrorCode */
		__u16 errorValue;
	};

	struct FacRESULT {
		__s16 InvokeID;
	};

#define FAC_REJECT_BASE(Base)			((Base) * 0x10)
	enum FacRejectBase {
		FacRejectBase_General,
		FacRejectBase_Invoke,
		FacRejectBase_Result,
		FacRejectBase_Error,

		/* Must be last in the list */
		FacRejectBase_Last
	};

/*
 * From Facility-Information-Element-Components
 * {itu-t identified-organization etsi(0) 196 facility-information-element-component(3)}
 */
	enum FacRejectCode {
		FacReject_None = 0xFFFF,	/* Not rejected */
		FacReject_Unknown = 0xFFFE,	/* Unknown reject code */

		FacReject_Gen_UnrecognizedComponent = FAC_REJECT_BASE(FacRejectBase_General) + 0,
		FacReject_Gen_MistypedComponent = FAC_REJECT_BASE(FacRejectBase_General) + 1,
		FacReject_Gen_BadlyStructuredComponent = FAC_REJECT_BASE(FacRejectBase_General) + 2,

		FacReject_Inv_DuplicateInvocation = FAC_REJECT_BASE(FacRejectBase_Invoke) + 0,
		FacReject_Inv_UnrecognizedOperation = FAC_REJECT_BASE(FacRejectBase_Invoke) + 1,
		FacReject_Inv_MistypedArgument = FAC_REJECT_BASE(FacRejectBase_Invoke) + 2,
		FacReject_Inv_ResourceLimitation = FAC_REJECT_BASE(FacRejectBase_Invoke) + 3,
		FacReject_Inv_InitiatorReleasing = FAC_REJECT_BASE(FacRejectBase_Invoke) + 4,
		FacReject_Inv_UnrecognizedLinkedID = FAC_REJECT_BASE(FacRejectBase_Invoke) + 5,
		FacReject_Inv_LinkedResponseUnexpected = FAC_REJECT_BASE(FacRejectBase_Invoke) + 6,
		FacReject_Inv_UnexpectedChildOperation = FAC_REJECT_BASE(FacRejectBase_Invoke) + 7,

		FacReject_Res_UnrecognizedInvocation = FAC_REJECT_BASE(FacRejectBase_Result) + 0,
		FacReject_Res_ResultResponseUnexpected = FAC_REJECT_BASE(FacRejectBase_Result) + 1,
		FacReject_Res_MistypedResult = FAC_REJECT_BASE(FacRejectBase_Result) + 2,

		FacReject_Err_UnrecognizedInvocation = FAC_REJECT_BASE(FacRejectBase_Error) + 0,
		FacReject_Err_ErrorResponseUnexpected = FAC_REJECT_BASE(FacRejectBase_Error) + 1,
		FacReject_Err_UnrecognizedError = FAC_REJECT_BASE(FacRejectBase_Error) + 2,
		FacReject_Err_UnexpectedError = FAC_REJECT_BASE(FacRejectBase_Error) + 3,
		FacReject_Err_MistypedParameter = FAC_REJECT_BASE(FacRejectBase_Error) + 4,
	};

	struct FacREJECT {
		/* TRUE if InvokeID is valid and was/will-be in the facility ie */
		__u16 InvokeIDPresent;

		__s16 InvokeID;

		/*! \see enum FacRejectCode */
		__u16 Code;
	};

#if defined(MISDN_NO_LONGER_IMPLEMENTED)	/* No longer implemented if they ever were implemented. */
	struct FacListen {
		__u32 NotificationMask;
	};

	struct FacSuspend {
		__s8 CallIdentity[16];
	};

	struct FacResume {
		__s8 CallIdentity[16];
	};

	struct FacCFActivate {
		__u32 Handle;
		__u16 Procedure;
		__u16 BasicService;
		__s8 ServedUserNumber[16];
		__s8 ForwardedToNumber[16];
		__s8 ForwardedToSubaddress[16];
	};

	struct FacCFDeactivate {
		__u32 Handle;
		__u16 Procedure;
		__u16 BasicService;
		__s8 ServedUserNumber[16];
	};

#define FacCFInterrogateParameters FacCFDeactivate

	struct FacCFInterrogateNumbers {
		__u32 Handle;
	};
#endif				/* defined(MISDN_NO_LONGER_IMPLEMENTED) */

	struct FacCDeflection {
		__u16 PresentationAllowed;
		__s8 DeflectedToNumber[16];
		__s8 DeflectedToSubaddress[16];
	};

	struct ChargingAssociation {
		__u8 chargeNumber[30];
		__s32 chargeIdentifier;
	};

/* This structure does not capture all information correctly */
	struct FacAOCChargingUnit {
		__s16 InvokeID;
		__u16 chargeNotAvailable;
		__u16 freeOfCharge;
		__u32 recordedUnits;
		__u32 typeOfChargingInfo;
		__u32 billingId;
		__u8 chargeNumber[50];
		struct ChargingAssociation chargeAssoc;
	};

	struct FacAOCCurrency {
		__s16 InvokeID;
		__u16 chargeNotAvailable;
		__u16 freeOfCharge;
		__u8 currency[10 + 1];
		__u32 currencyAmount;
		__u32 multiplier;
		__u32 typeOfChargingInfo;
		__u32 billingId;
		struct ChargingAssociation chargeAssoc;
	};

	struct Q931_BearerCapability {
		__u8 Length;

		/*
		 * We mostly just need to store the contents so we will defer
		 * decoding/encoding.
		 */
		__u8 Contents[12];
	};

	struct Q931_HighLayerCompatibility {
		/* Compatibility present if length is nonzero */
		__u8 Length;

		/*
		 * We mostly just need to store the contents so we will defer
		 * decoding/encoding.
		 */
		__u8 Contents[5];
	};

	struct Q931_LowLayerCompatibility {
		/* Compatibility present if length is nonzero */
		__u8 Length;

		/*
		 * We mostly just need to store the contents so we will defer
		 * decoding/encoding.
		 */
		__u8 Contents[18];
	};

	struct Q931_UserUserInformation {
		/* User-User information present if length is nonzero */
		__u8 Length;

		/*
		 * The network dependent maximum is either 35 or 131 octets
		 * in non-USER-INFORMATION messages.
		 */
		__u8 Contents[131];
	};

/* The BC, HLC (optional) and LLC (optional) information */
	struct Q931_Bc_Hlc_Llc {
		/* Bearer Capability */
		struct Q931_BearerCapability Bc;

		/* Low Layer Compatibility (Optional) */
		struct Q931_LowLayerCompatibility Llc;

		/* High Layer Compatibility (Optional) */
		struct Q931_HighLayerCompatibility Hlc;
	};

/* The BC, HLC (optional), LLC (optional), and User-user (optional) information */
	struct Q931_Bc_Hlc_Llc_Uu {
		/* Bearer Capability */
		struct Q931_BearerCapability Bc;

		/* Low Layer Compatibility (Optional) */
		struct Q931_LowLayerCompatibility Llc;

		/* High Layer Compatibility (Optional) */
		struct Q931_HighLayerCompatibility Hlc;

		/* User-User Information (Optional) */
		struct Q931_UserUserInformation UserInfo;
	};

	struct FacPartyNumber {
		/*
		 * Party numbering plan
		 * unknown(0),
		 * public(1) - The numbering plan is according to ITU-T E.164,
		 * nsapEncoded(2),
		 * data(3) - Reserved,
		 * telex(4) - Reserved,
		 * private(5),
		 * nationalStandard(8) - Reserved
		 */
		__u8 Type;

		/*
		 * Valid for public and private party number types
		 * public:
		 *  unknown(0),
		 *  internationalNumber(1),
		 *  nationalNumber(2),
		 *  networkSpecificNumber(3) - Reserved
		 *  subscriberNumber(4) - Reserved
		 *  abbreviatedNumber(6)
		 * private:
		 *  unknown(0),
		 *  level2RegionalNumber(1),
		 *  level1RegionalNumber(2)
		 *  pTNSpecificNumber(3)
		 *  localNumber(4),
		 *  abbreviatedNumber(6)
		 */
		__u8 TypeOfNumber;

		/* Number present if length is nonzero */
		__u8 LengthOfNumber;
		__u8 Number[20 + 1];
	};

	struct FacPartySubaddress {
		/* Subaddress type UserSpecified(0), NSAP(1) */
		__u8 Type;

		/* Subaddress present if length is nonzero */
		__u8 Length;

		union {
			/* Specified according to ITU-T Recommendation X.213 */
			__u8 Nsap[20 + 1];

			/* Use of this formatting is not recommended */
			struct {
				/* TRUE if OddCount present */
				__u8 OddCountPresent;

				/*
				 * TRUE if odd number of BCD digits
				 * Used when the coding of subaddress is BCD.
				 */
				__u8 OddCount;
				__u8 Information[20 + 1];
			} UserSpecified;
		} u;
	};

	struct FacAddress {
		struct FacPartyNumber Party;

		/* Subaddress (Optional) */
		struct FacPartySubaddress Subaddress;
	};

	struct FacAddressScreened {
		struct FacPartyNumber Party;

		/* Subaddress (Optional) */
		struct FacPartySubaddress Subaddress;

		/*
		 * userProvidedNotScreened(0),
		 * userProvidedVerifiedAndPassed(1),
		 * userProvidedVerifiedAndFailed(2), -- Not used, value reserved
		 * networkProvided(3)
		 */
		__u8 ScreeningIndicator;
	};

	struct FacPresentedNumberUnscreened {
		/*
		 * Number presentation type:
		 * presentationAllowedNumber(0),
		 * presentationRestricted(1),
		 * numberNotAvailableDueToInterworking(2),
		 * presentationRestrictedNumber(3)
		 */
		__u8 Type;
		struct FacPartyNumber Unscreened;
	};

	struct FacPresentedAddressScreened {
		/*
		 * Address presentation type:
		 * presentationAllowedAddress(0),
		 * presentationRestricted(1),
		 * numberNotAvailableDueToInterworking(2),
		 * presentationRestrictedAddress(3)
		 */
		__u8 Type;
		struct FacAddressScreened Address;
	};

	struct FacCallInformation {
		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* Address of B */
		struct FacAddress AddressOfB;

		/* Subaddress of A (Optional) */
		struct FacPartySubaddress SubaddressOfA;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	enum FacComponentType {
		FacComponent_Invoke,
		FacComponent_Result,
		FacComponent_Error,
		FacComponent_Reject
	};

	struct FacStatusRequest_ARG {
		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* allBasicServices(0), oneOrMoreBasicServices(1) */
		__u8 CompatibilityMode;
	};

	struct FacStatusRequest_RES {
		/* compatibleAndFree(0), compatibleAndBusy(1), incompatible(2) */
		__u8 Status;
	};

	struct FacStatusRequest {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacStatusRequest_ARG Invoke;
			struct FacStatusRequest_RES Result;
		} Component;
	};

	struct FacCallInfoRetain {
		__s16 InvokeID;

		/* Call Linkage Record ID */
		__u8 CallLinkageID;
	};

	struct FacEraseCallLinkageID {
		__s16 InvokeID;

		/* Call Linkage Record ID */
		__u8 CallLinkageID;
	};

	struct FacCCBSRequest_ARG {
		/* Call Linkage Record ID */
		__u8 CallLinkageID;
	};

	struct FacCCBSRequest_RES {
		/* globalRecall(0),     specificRecall(1) */
		__u8 RecallMode;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSRequest {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCCBSRequest_ARG Invoke;
			struct FacCCBSRequest_RES Result;
		} Component;
	};

	struct FacCCBSDeactivate_ARG {
		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSDeactivate {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCCBSDeactivate_ARG Invoke;
		} Component;
	};

	struct FacCCBSInterrogate_ARG {
		/* Party A number (Optional) */
		struct FacPartyNumber AParty;

		/* TRUE if CCBSReference present */
		__u8 CCBSReferencePresent;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSInterrogate_RES {
		struct FacCallInformation CallDetails[5];

		/* Number of CallDetails records present */
		__u8 NumRecords;

		/* globalRecall(0),     specificRecall(1) */
		__u8 RecallMode;
	};

	struct FacCCBSInterrogate {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCCBSInterrogate_ARG Invoke;
			struct FacCCBSInterrogate_RES Result;
		} Component;
	};

	struct FacCCBSErase {
		__s16 InvokeID;

		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* Address of B */
		struct FacAddress AddressOfB;

		/* globalRecall(0),     specificRecall(1) */
		__u8 RecallMode;

		/* CCBS Record ID */
		__u8 CCBSReference;

		/*
		 * CCBS Erase reason
		 * normal-unspecified(0),
		 * t-CCBS2-timeout(1),
		 * t-CCBS3-timeout(2),
		 * basic-call-failed(3)
		 */
		__u8 Reason;
	};

	struct FacCCBSRemoteUserFree {
		__s16 InvokeID;

		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* Address of B */
		struct FacAddress AddressOfB;

		/* globalRecall(0),     specificRecall(1) */
		__u8 RecallMode;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSCall {
		__s16 InvokeID;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSStatusRequest_ARG {
		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* globalRecall(0),     specificRecall(1) */
		__u8 RecallMode;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSStatusRequest_RES {
		/* TRUE if User A is free */
		__u8 Free;
	};

	struct FacCCBSStatusRequest {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCCBSStatusRequest_ARG Invoke;
			struct FacCCBSStatusRequest_RES Result;
		} Component;
	};

	struct FacCCBSBFree {
		__s16 InvokeID;

		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* Address of B */
		struct FacAddress AddressOfB;

		/* globalRecall(0),     specificRecall(1) */
		__u8 RecallMode;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBSStopAlerting {
		__s16 InvokeID;

		/* CCBS Record ID */
		__u8 CCBSReference;
	};

	struct FacCCBS_T_Request_ARG {
		/* The BC, HLC (optional) and LLC (optional) information */
		struct Q931_Bc_Hlc_Llc Q931ie;

		/* Address of B */
		struct FacAddress Destination;

		/* Caller-ID Address (Present if Originating.Party.LengthOfNumber is nonzero) */
		struct FacAddress Originating;

		/* TRUE if the PresentationAllowedIndicator is present */
		__u8 PresentationAllowedIndicatorPresent;

		/* TRUE if presentation is allowed for the originating address */
		__u8 PresentationAllowedIndicator;

		/* TRUE if User A's CCBS request is continued if user B is busy again. */
		__u8 RetentionSupported;
	};

	struct FacCCBS_T_Request_RES {
		/* TRUE if User A's CCBS request is continued if user B is busy again. */
		__u8 RetentionSupported;
	};

	struct FacCCBS_T_Request {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCCBS_T_Request_ARG Invoke;
			struct FacCCBS_T_Request_RES Result;
		} Component;
	};

	struct FacCCBS_T_Event {
		__s16 InvokeID;
	};

	struct FacEctExecute {
		__s16 InvokeID;
	};

	struct FacExplicitEctExecute {
		__s16 InvokeID;

		__s16 LinkID;
	};

	struct FacRequestSubaddress {
		__s16 InvokeID;
	};

	struct FacSubaddressTransfer {
		__s16 InvokeID;

		/* Transferred to subaddress */
		struct FacPartySubaddress Subaddress;
	};

	struct FacEctLinkIdRequest_RES {
		__s16 LinkID;
	};

	struct FacEctLinkIdRequest {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacEctLinkIdRequest_RES Result;
		} Component;
	};

	struct FacEctInform {
		__s16 InvokeID;

		/* Redirection Number (Optional) */
		struct FacPresentedNumberUnscreened Redirection;

		/* TRUE if the Redirection Number is present */
		__u8 RedirectionPresent;

		/* alerting(0), active(1) */
		__u8 Status;
	};

	struct FacEctLoopTest_ARG {
		__s8 CallTransferID;
	};

	struct FacEctLoopTest_RES {
		/*
		 * insufficientInformation(0),
		 * noLoopExists(1),
		 * simultaneousTransfer(2)
		 */
		__u8 LoopResult;
	};

	struct FacEctLoopTest {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacEctLoopTest_ARG Invoke;
			struct FacEctLoopTest_RES Result;
		} Component;
	};

	struct FacActivationDiversion_ARG {
		/* Forwarded to address */
		struct FacAddress ForwardedTo;

		/* Forward all numbers if not present. */
		struct FacPartyNumber ServedUser;

		/* cfu(0), cfb(1), cfnr(2) */
		__u8 Procedure;

		/*
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacActivationDiversion {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacActivationDiversion_ARG Invoke;
		} Component;
	};

	struct FacDeactivationDiversion_ARG {
		/* Forward all numbers if not present. */
		struct FacPartyNumber ServedUser;

		/* cfu(0), cfb(1), cfnr(2) */
		__u8 Procedure;

		/*
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacDeactivationDiversion {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacDeactivationDiversion_ARG Invoke;
		} Component;
	};

	struct FacActivationStatusNotificationDiv {
		__s16 InvokeID;

		/* Forwarded to address */
		struct FacAddress ForwardedTo;

		/* Forward all numbers if not present. */
		struct FacPartyNumber ServedUser;

		/* cfu(0), cfb(1), cfnr(2) */
		__u8 Procedure;

		/*
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacDeactivationStatusNotificationDiv {
		__s16 InvokeID;

		/* Forward all numbers if not present. */
		struct FacPartyNumber ServedUser;

		/* cfu(0), cfb(1), cfnr(2) */
		__u8 Procedure;

		/*
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacInterrogationDiversion_ARG {
		/* Forward all numbers if not present. */
		struct FacPartyNumber ServedUser;

		/* cfu(0), cfb(1), cfnr(2) */
		__u8 Procedure;

		/*
		 * DEFAULT allServices
		 *
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacForwardingRecord {
		/* Forwarded to address */
		struct FacAddress ForwardedTo;

		/* Forward all numbers if not present. */
		struct FacPartyNumber ServedUser;

		/* cfu(0), cfb(1), cfnr(2) */
		__u8 Procedure;

		/*
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacForwardingList {
		/* SET SIZE (0..29) OF Forwarding Records */
		struct FacForwardingRecord List[29];

		/* Number of Forwarding records present */
		__u8 NumRecords;
	};

	struct FacInterrogationDiversion {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacInterrogationDiversion_ARG Invoke;
			struct FacForwardingList Result;
		} Component;
	};

	struct FacDiversionInformation {
		__s16 InvokeID;

		/* Served user subaddress (Optional) */
		struct FacPartySubaddress ServedUserSubaddress;

		/* Calling address (Optional) */
		struct FacPresentedAddressScreened CallingAddress;

		/* Original called number (Optional) */
		struct FacPresentedNumberUnscreened OriginalCalled;

		/* Last diverting number (Optional) */
		struct FacPresentedNumberUnscreened LastDiverting;

		/* User-User information embedded in Q.931 IE (Optional) */
		struct Q931_UserUserInformation UserInfo;

		/*
		 * Last diverting reason (Optional)
		 *
		 * unknown(0),
		 * cfu(1),
		 * cfb(2),
		 * cfnr(3),
		 * cdAlerting(4),
		 * cdImmediate(5)
		 */
		__u8 LastDivertingReason;

		/* TRUE if CallingAddress is present */
		__u8 CallingAddressPresent;

		/* TRUE if OriginalCalled is present */
		__u8 OriginalCalledPresent;

		/* TRUE if LastDiverting is present */
		__u8 LastDivertingPresent;

		/* TRUE if LastDivertingReason is present */
		__u8 LastDivertingReasonPresent;

		/*
		 * unknown(0),
		 * cfu(1),
		 * cfb(2),
		 * cfnr(3),
		 * cdAlerting(4),
		 * cdImmediate(5)
		 */
		__u8 DiversionReason;

		/*
		 * allServices(0),
		 * speech(1),
		 * unrestrictedDigitalInformation(2),
		 * audio3k1Hz(3),
		 * unrestrictedDigitalInformationWithTonesAndAnnouncements(4),
		 * multirate(5),
		 * telephony3k1Hz(32),
		 * teletex(33),
		 * telefaxGroup4Class1(34),
		 * videotexSyntaxBased(35),
		 * videotelephony(36),
		 * telefaxGroup2-3(37),
		 * telephony7kHz(38),
		 * euroFileTransfer(39),
		 * fileTransferAndAccessManagement(40),
		 * videoconference(41),
		 * audioGraphicConference(42)
		 */
		__u8 BasicService;
	};

	struct FacCallDeflection_ARG {
		/* Deflection address (Deflected-To address) */
		struct FacAddress Deflection;

		/* TRUE if PresentationAllowedToDivertedToUser is present */
		__u8 PresentationAllowedToDivertedToUserPresent;

		/* TRUE if presentation is allowed (Optional) */
		__u8 PresentationAllowedToDivertedToUser;
	};

	struct FacCallDeflection {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCallDeflection_ARG Invoke;
		} Component;
	};

	struct FacCallRerouteing_ARG {
		struct FacAddress CalledAddress;

		/* The BC, HLC (optional), LLC (optional), and User-user (optional) information */
		struct Q931_Bc_Hlc_Llc_Uu Q931ie;

		/* Last rerouting number */
		struct FacPresentedNumberUnscreened LastRerouting;

		/* Calling party subaddress (Optional) */
		struct FacPartySubaddress CallingPartySubaddress;

		/*
		 * unknown(0),
		 * cfu(1),
		 * cfb(2),
		 * cfnr(3),
		 * cdAlerting(4),
		 * cdImmediate(5)
		 */
		__u8 ReroutingReason;

		/* Range 1-5 */
		__u8 ReroutingCounter;

		/*
		 * DEFAULT noNotification
		 *
		 * noNotification(0),
		 * notificationWithoutDivertedToNr(1),
		 * notificationWithDivertedToNr(2)
		 */
		__u8 SubscriptionOption;
	};

	struct FacCallRerouteing {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacCallRerouteing_ARG Invoke;
		} Component;
	};

	struct FacServedUserNumberList {
		/* SET SIZE (0..99) OF Served user numbers */
		struct FacPartyNumber List[99];

		/* Number of Served user numbers present */
		__u8 NumRecords;
	};

	struct FacInterrogateServedUserNumbers {
		__s16 InvokeID;

		/*! \see enum FacComponentType */
		__u8 ComponentType;
		union {
			struct FacServedUserNumberList Result;
		} Component;
	};

	struct FacDivertingLegInformation1 {
		__s16 InvokeID;

		/* Diverted to number (Optional) */
		struct FacPresentedNumberUnscreened DivertedTo;

		/* TRUE if DivertedTo is present */
		__u8 DivertedToPresent;

		/*
		 * unknown(0),
		 * cfu(1),
		 * cfb(2),
		 * cfnr(3),
		 * cdAlerting(4),
		 * cdImmediate(5)
		 */
		__u8 DiversionReason;

		/*
		 * noNotification(0),
		 * notificationWithoutDivertedToNr(1),
		 * notificationWithDivertedToNr(2)
		 */
		__u8 SubscriptionOption;
	};

	struct FacDivertingLegInformation2 {
		__s16 InvokeID;

		/* Diverting number (Optional) */
		struct FacPresentedNumberUnscreened Diverting;

		/* Original called number (Optional) */
		struct FacPresentedNumberUnscreened OriginalCalled;

		/* TRUE if Diverting is present */
		__u8 DivertingPresent;

		/* TRUE if OriginalCalled is present */
		__u8 OriginalCalledPresent;

		/*
		 * unknown(0),
		 * cfu(1),
		 * cfb(2),
		 * cfnr(3),
		 * cdAlerting(4),
		 * cdImmediate(5)
		 */
		__u8 DiversionReason;

		/* Range 1-5 */
		__u8 DiversionCounter;
	};

	struct FacDivertingLegInformation3 {
		__s16 InvokeID;

		/* TRUE if presentation is allowed */
		__u8 PresentationAllowedIndicator;
	};

	struct FacParm {
		enum FacFunction Function;
		union {
#if defined(MISDN_NO_LONGER_IMPLEMENTED)	/* No longer implemented if they ever were implemented. */
			struct FacListen Listen;
			struct FacSuspend Suspend;
			struct FacResume Resume;

			/* Original diversion support */
			struct FacCFActivate CFActivate;
			struct FacCFDeactivate CFDeactivate;
			struct FacCFInterrogateParameters CFInterrogateParameters;
			struct FacCFInterrogateNumbers CFInterrogateNumbers;
#endif				/* defined(MISDN_NO_LONGER_IMPLEMENTED) */

			struct FacCDeflection CDeflection;

			/* Original AOC support */
			struct FacAOCChargingUnit AOCchu;
			struct FacAOCCurrency AOCcur;

			struct FacRESULT RESULT;
			struct FacERROR ERROR;
			struct FacREJECT REJECT;

			/* CCBS/CCNR support */
			struct FacStatusRequest StatusRequest;

			/* CCBS/CCNR support */
			struct FacCallInfoRetain CallInfoRetain;
			struct FacEraseCallLinkageID EraseCallLinkageID;
			struct FacCCBSDeactivate CCBSDeactivate;
			struct FacCCBSErase CCBSErase;
			struct FacCCBSRemoteUserFree CCBSRemoteUserFree;
			struct FacCCBSCall CCBSCall;
			struct FacCCBSStatusRequest CCBSStatusRequest;
			struct FacCCBSBFree CCBSBFree;
			struct FacCCBSStopAlerting CCBSStopAlerting;

			/* CCBS support */
			struct FacCCBSRequest CCBSRequest;
			struct FacCCBSInterrogate CCBSInterrogate;

			/* CCNR support */
			struct FacCCBSRequest CCNRRequest;
			struct FacCCBSInterrogate CCNRInterrogate;

			/* CCBS-T/CCNR-T support */
			struct FacCCBS_T_Event CCBS_T_Call;
			struct FacCCBS_T_Event CCBS_T_Suspend;
			struct FacCCBS_T_Event CCBS_T_Resume;
			struct FacCCBS_T_Event CCBS_T_RemoteUserFree;
			struct FacCCBS_T_Event CCBS_T_Available;

			/* CCBS-T support */
			struct FacCCBS_T_Request CCBS_T_Request;

			/* CCNR-T support */
			struct FacCCBS_T_Request CCNR_T_Request;

			/* ECT support */
			struct FacEctExecute EctExecute;
			struct FacExplicitEctExecute ExplicitEctExecute;
			struct FacRequestSubaddress RequestSubaddress;
			struct FacSubaddressTransfer SubaddressTransfer;
			struct FacEctLinkIdRequest EctLinkIdRequest;
			struct FacEctInform EctInform;
			struct FacEctLoopTest EctLoopTest;

			/* Diversion support */
			struct FacActivationDiversion ActivationDiversion;
			struct FacDeactivationDiversion DeactivationDiversion;
			struct FacActivationStatusNotificationDiv ActivationStatusNotificationDiv;
			struct FacDeactivationStatusNotificationDiv DeactivationStatusNotificationDiv;
			struct FacInterrogationDiversion InterrogationDiversion;
			struct FacDiversionInformation DiversionInformation;
			struct FacCallDeflection CallDeflection;
			struct FacCallRerouteing CallRerouteing;
			struct FacInterrogateServedUserNumbers InterrogateServedUserNumbers;
			struct FacDivertingLegInformation1 DivertingLegInformation1;
			struct FacDivertingLegInformation2 DivertingLegInformation2;
			struct FacDivertingLegInformation3 DivertingLegInformation3;
		} u;
		int InvokeID;
	};

/*
 * encodeFac(__u8 *dest, struct FacParm *fac)
 *
 * encode the facility (fac) into the buffer (dest)
 *
 * parameter:
 * dest  - destination buffer
 * fac   - facility to encode
 *
 * returns:
 *    length of the encoded facility, or -1 on error
 */
	int encodeFac(__u8 * dest, struct FacParm *fac);

/*
 * decodeFac(__u8 *src, struct FacParm *fac)
 *
 * decode the facility (src) and write the result to (fac)
 *
 * parameter:
 * src   - encoded facility
 * fac   - where to store the result
 *
 * returns:
 *    0 on success, -1 on error
 */
	int decodeFac(__u8 * src, struct FacParm *fac);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __SUPPSERV_H__ */
/* ------------------------------------------------------------------- *//* end suppserv.h */
