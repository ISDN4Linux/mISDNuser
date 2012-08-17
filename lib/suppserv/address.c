/* $Id$
 *
 * Addressing-Data-Elements ETS 300 196-1 D.3
 *
 * Addressing-Data-Elements encode/decode
 */

#include "asn1.h"
#include <string.h>

/* ------------------------------------------------------------------- */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the NumberDigits PartyNumber argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartyNumber Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseNumberDigits_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartyNumber *PartyNumber)
{
	struct asn1ParseString Number;
	int LengthConsumed;

	Number.buf = (char *)PartyNumber->Number;
	Number.maxSize = sizeof(PartyNumber->Number);
	Number.length = 0;
	LengthConsumed = ParseNumericString(pc, p, end, &Number);
	PartyNumber->LengthOfNumber = Number.length;
	return LengthConsumed;
}				/* end ParseNumberDigits_Full() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the NSAP PartyNumber argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartyNumber Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseNSAPPartyNumber(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartyNumber *PartyNumber)
{
	struct asn1ParseString Number;
	int LengthConsumed;

	Number.buf = (char *)PartyNumber->Number;
	Number.maxSize = sizeof(PartyNumber->Number);
	Number.length = 0;
	LengthConsumed = ParseOctetString(pc, p, end, &Number);
	PartyNumber->LengthOfNumber = Number.length;
	return LengthConsumed;
}				/* end ParseNSAPPartyNumber() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the public or private network PartyNumber type.
 *
 * \param Dest Where to put the encoding
 * \param Number
 * \param LengthOfNumber
 * \param TypeOfNumber
 *
 * \retval length
 */
static int encodeNetworkPartyNumber(__u8 * Dest, const __s8 * Number, __u8 LengthOfNumber, __u8 TypeOfNumber)
{
	__u8 *p;

	Dest[0] = ASN1_TAG_SEQUENCE;
	p = &Dest[2];

	p += encodeEnum(p, ASN1_TAG_ENUM, TypeOfNumber);
	p += encodeNumericString(p, ASN1_TAG_NUMERIC_STRING, Number, LengthOfNumber);

	/* length */
	Dest[1] = p - &Dest[2];

	return p - Dest;
}				/* end encodeNetworkPartyNumber() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the public or private network PartyNumber argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartyNumber Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseNetworkPartyNumber(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartyNumber *PartyNumber)
{
	unsigned int TypeOfNumber;
	INIT;

	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &TypeOfNumber);
	PartyNumber->TypeOfNumber = TypeOfNumber;
	XSEQUENCE_1(ParseNumberDigits_Full, ASN1_TAG_NUMERIC_STRING, ASN1_NOT_TAGGED, PartyNumber);

	return p - beg;
}				/* end ParseNetworkPartyNumber() */

/* ******************************************************************* */
/*!
 * \brief Encode the PartyNumber type.
 *
 * \param Dest Where to put the encoding
 * \param PartyNumber Number information to encode.
 *
 * \retval length
 */
int encodePartyNumber_Full(__u8 * Dest, const struct FacPartyNumber *PartyNumber)
{
	int Length;

	switch (PartyNumber->Type) {
	case 0:		/* Unknown PartyNumber */
		Length =
		    encodeNumericString(Dest, ASN1_TAG_CONTEXT_SPECIFIC | 0, (const __s8 *)PartyNumber->Number,
					PartyNumber->LengthOfNumber);
		break;
	case 1:		/* Public PartyNumber */
		Length =
		    encodeNetworkPartyNumber(Dest, (const __s8 *)PartyNumber->Number, PartyNumber->LengthOfNumber,
					     PartyNumber->TypeOfNumber);
		Dest[0] &= ASN1_TAG_CONSTRUCTED;
		Dest[0] |= ASN1_TAG_CONTEXT_SPECIFIC | 1;
		break;
	case 2:		/* NSAP encoded PartyNumber */
		Length =
		    encodeOctetString(Dest, ASN1_TAG_CONTEXT_SPECIFIC | 2, (const __s8 *)PartyNumber->Number,
				      PartyNumber->LengthOfNumber);
		break;
	case 3:		/* Data PartyNumber (Not used) */
		Length =
		    encodeNumericString(Dest, ASN1_TAG_CONTEXT_SPECIFIC | 3, (const __s8 *)PartyNumber->Number,
					PartyNumber->LengthOfNumber);
		break;
	case 4:		/* Telex PartyNumber (Not used) */
		Length =
		    encodeNumericString(Dest, ASN1_TAG_CONTEXT_SPECIFIC | 4, (const __s8 *)PartyNumber->Number,
					PartyNumber->LengthOfNumber);
		break;
	case 5:		/* Private PartyNumber */
		Length =
		    encodeNetworkPartyNumber(Dest, (const __s8 *)PartyNumber->Number, PartyNumber->LengthOfNumber,
					     PartyNumber->TypeOfNumber);
		Dest[0] &= ASN1_TAG_CONSTRUCTED;
		Dest[0] |= ASN1_TAG_CONTEXT_SPECIFIC | 5;
		break;
	case 8:		/* National Standard PartyNumber (Not used) */
		Length =
		    encodeNumericString(Dest, ASN1_TAG_CONTEXT_SPECIFIC | 8, (const __s8 *)PartyNumber->Number,
					PartyNumber->LengthOfNumber);
		break;
	default:
		Length = 0;
		break;
	}			/* end switch */

	return Length;
}				/* end encodePartyNumber_Full() */

/* ******************************************************************* */
/*!
 * \brief Parse the PartyNumber argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartyNumber Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParsePartyNumber_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartyNumber *PartyNumber)
{
	INIT;

	PartyNumber->Type = 0;	/* Unknown PartyNumber */
	XCHOICE_1(ParseNumberDigits_Full, ASN1_TAG_CONTEXT_SPECIFIC, 0, PartyNumber);
	PartyNumber->Type = 1;	/* Public PartyNumber */
	XCHOICE_1(ParseNetworkPartyNumber, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 1, PartyNumber);
	PartyNumber->Type = 2;	/* NSAP encoded PartyNumber */
	XCHOICE_1(ParseNSAPPartyNumber, ASN1_TAG_CONTEXT_SPECIFIC, 2, PartyNumber);
	PartyNumber->Type = 3;	/* Data PartyNumber (Not used) */
	XCHOICE_1(ParseNumberDigits_Full, ASN1_TAG_CONTEXT_SPECIFIC, 3, PartyNumber);
	PartyNumber->Type = 4;	/* Telex PartyNumber (Not used) */
	XCHOICE_1(ParseNumberDigits_Full, ASN1_TAG_CONTEXT_SPECIFIC, 4, PartyNumber);
	PartyNumber->Type = 5;	/* Private PartyNumber */
	XCHOICE_1(ParseNetworkPartyNumber, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 5, PartyNumber);
	PartyNumber->Type = 8;	/* National Standard PartyNumber (Not used) */
	XCHOICE_1(ParseNumberDigits_Full, ASN1_TAG_CONTEXT_SPECIFIC, 8, PartyNumber);

	XCHOICE_DEFAULT;
}				/* end ParsePartyNumber_Full() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the User information string PartySubaddress argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartySubaddress Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseUserSubaddressInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartySubaddress *PartySubaddress)
{
	struct asn1ParseString Subaddress;
	int LengthConsumed;

	Subaddress.buf = (char *)PartySubaddress->u.UserSpecified.Information;
	Subaddress.maxSize = sizeof(PartySubaddress->u.UserSpecified.Information);
	Subaddress.length = 0;
	LengthConsumed = ParseOctetString(pc, p, end, &Subaddress);
	PartySubaddress->Length = Subaddress.length;
	return LengthConsumed;
}				/* end ParseUserSubaddressInfo() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the User PartySubaddress argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartySubaddress Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseUserSubaddress(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartySubaddress *PartySubaddress)
{
	int OddCount;
	INIT;

	PartySubaddress->Type = 0;	/* UserSpecified */

	XSEQUENCE_1(ParseUserSubaddressInfo, ASN1_TAG_OCTET_STRING, ASN1_NOT_TAGGED, PartySubaddress);
	if (p < end) {
		XSEQUENCE_1(ParseBoolean, ASN1_TAG_BOOLEAN, ASN1_NOT_TAGGED, &OddCount);
		PartySubaddress->u.UserSpecified.OddCount = OddCount;
		PartySubaddress->u.UserSpecified.OddCountPresent = 1;
	} else {
		PartySubaddress->u.UserSpecified.OddCount = 0;
		PartySubaddress->u.UserSpecified.OddCountPresent = 0;
	}
	return p - beg;
}				/* end ParseUserSubaddress() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the NSAP PartySubaddress argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartySubaddress Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseNSAPSubaddress_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartySubaddress *PartySubaddress)
{
	struct asn1ParseString Subaddress;
	int LengthConsumed;

	PartySubaddress->Type = 1;	/* NSAP */

	Subaddress.buf = (char *)PartySubaddress->u.Nsap;
	Subaddress.maxSize = sizeof(PartySubaddress->u.Nsap);
	Subaddress.length = 0;
	LengthConsumed = ParseOctetString(pc, p, end, &Subaddress);
	PartySubaddress->Length = Subaddress.length;
	return LengthConsumed;
}				/* end ParseNSAPSubaddress_Full() */

/* ******************************************************************* */
/*!
 * \brief Encode the PartySubaddress type.
 *
 * \param Dest Where to put the encoding
 * \param PartySubaddress Subaddress information to encode.
 *
 * \retval length
 */
int encodePartySubaddress_Full(__u8 * Dest, const struct FacPartySubaddress *PartySubaddress)
{
	__u8 *p;
	int Length;

	switch (PartySubaddress->Type) {
	case 0:		/* UserSpecified */
		Dest[0] = ASN1_TAG_SEQUENCE;
		p = &Dest[2];

		p += encodeOctetString(p, ASN1_TAG_OCTET_STRING, (const __s8 *)PartySubaddress->u.UserSpecified.Information,
				       PartySubaddress->Length);
		if (PartySubaddress->u.UserSpecified.OddCountPresent) {
			p += encodeBoolean(p, ASN1_TAG_BOOLEAN, PartySubaddress->u.UserSpecified.OddCount);
		}

		/* length */
		Dest[1] = p - &Dest[2];

		Length = p - Dest;
		break;
	case 1:		/* NSAP */
		Length =
		    encodeOctetString(Dest, ASN1_TAG_OCTET_STRING, (const __s8 *)PartySubaddress->u.Nsap, PartySubaddress->Length);
		break;
	default:
		Length = 0;
		break;
	}			/* end switch */

	return Length;
}				/* end encodePartySubaddress_Full() */

/* ******************************************************************* */
/*!
 * \brief Parse the PartySubaddress argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param PartySubaddress Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParsePartySubaddress_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacPartySubaddress *PartySubaddress)
{
	INIT;

	XCHOICE_1(ParseUserSubaddress, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, PartySubaddress);
	XCHOICE_1(ParseNSAPSubaddress_Full, ASN1_TAG_OCTET_STRING, ASN1_NOT_TAGGED, PartySubaddress);

	XCHOICE_DEFAULT;
}				/* end ParsePartySubaddress_Full() */

/* ******************************************************************* */
/*!
 * \brief Encode the Address type.
 *
 * \param Dest Where to put the encoding
 * \param Address Address information to encode.
 *
 * \retval length
 */
int encodeAddress_Full(__u8 * Dest, const struct FacAddress *Address)
{
	__u8 *p;

	Dest[0] = ASN1_TAG_SEQUENCE;
	p = &Dest[2];

	p += encodePartyNumber_Full(p, &Address->Party);
	if (Address->Subaddress.Length) {
		p += encodePartySubaddress_Full(p, &Address->Subaddress);
	}

	/* length */
	Dest[1] = p - &Dest[2];

	return p - Dest;
}				/* end encodeAddress_Full() */

/* ******************************************************************* */
/*!
 * \brief Parse the Address argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Address Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParseAddress_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAddress *Address)
{
	INIT;

	XSEQUENCE_1(ParsePartyNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &Address->Party);
	if (p < end) {
		/* The optional subaddress must be present since there is something left. */
		XSEQUENCE_1(ParsePartySubaddress_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &Address->Subaddress);
	} else {
		Address->Subaddress.Length = 0;	/* Subaddress not present */
	}

	return p - beg;
}				/* end ParseAddress_Full() */

/* ******************************************************************* */
/*!
 * \brief Encode the PresentedNumberUnscreened type.
 *
 * \param Dest Where to put the encoding
 * \param Presented Number information to encode.
 *
 * \retval length
 */
int encodePresentedNumberUnscreened_Full(__u8 * Dest, const struct FacPresentedNumberUnscreened *Presented)
{
	__u8 *p;
	__u8 *TagStart;

	p = Dest;
	switch (Presented->Type) {
	case 0:		/* presentationAllowedNumber */
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 0;
		p = &TagStart[2];

		p += encodePartyNumber_Full(p, &Presented->Unscreened);

		/* tag Length */
		TagStart[1] = p - &TagStart[2];
		break;
	case 1:		/* presentationRestricted */
		p += encodeNull(p, ASN1_TAG_CONTEXT_SPECIFIC | 1);
		break;
	case 2:		/* numberNotAvailableDueToInterworking */
		p += encodeNull(p, ASN1_TAG_CONTEXT_SPECIFIC | 2);
		break;
	case 3:		/* presentationRestrictedNumber */
		TagStart = p;
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 3;
		p = &TagStart[2];

		p += encodePartyNumber_Full(p, &Presented->Unscreened);

		/* tag Length */
		TagStart[1] = p - &TagStart[2];
		break;
	default:
		break;
	}			/* end switch */

	return p - Dest;
}				/* end encodePresentedNumberUnscreened_Full() */

/* ******************************************************************* */
/*!
 * \brief Parse the PresentedNumberUnscreened argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Presented Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParsePresentedNumberUnscreened_Full(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacPresentedNumberUnscreened *Presented)
{
	INIT;

	Presented->Type = 0;
	XCHOICE_1(ParsePartyNumber_Full, ASN1_TAG_EXPLICIT | ASN1_TAG_CONTEXT_SPECIFIC, 0, &Presented->Unscreened);
	Presented->Type = 1;
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 1);
	Presented->Type = 2;
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 2);
	Presented->Type = 3;
	XCHOICE_1(ParsePartyNumber_Full, ASN1_TAG_EXPLICIT | ASN1_TAG_CONTEXT_SPECIFIC, 3, &Presented->Unscreened);

	XCHOICE_DEFAULT;
}				/* end ParsePresentedNumberUnscreened_Full() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Encode the AddressScreened type.
 *
 * \param Dest Where to put the encoding
 * \param Address Address information to encode.
 *
 * \retval length
 */
static int encodeAddressScreened_Full(__u8 * Dest, const struct FacAddressScreened *Address)
{
	__u8 *p;

	Dest[0] = ASN1_TAG_SEQUENCE;
	p = &Dest[2];

	p += encodePartyNumber_Full(p, &Address->Party);
	p += encodeEnum(p, ASN1_TAG_ENUM, Address->ScreeningIndicator);
	if (Address->Subaddress.Length) {
		p += encodePartySubaddress_Full(p, &Address->Subaddress);
	}

	/* length */
	Dest[1] = p - &Dest[2];

	return p - Dest;
}				/* end encodeAddressScreened_Full() */

/* ******************************************************************* */
/*!
 * \internal
 * \brief Parse the AddressScreened argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Address Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
static int ParseAddressScreened_Full(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAddressScreened *Address)
{
	unsigned int Value;
	INIT;

	XSEQUENCE_1(ParsePartyNumber_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &Address->Party);
	XSEQUENCE_1(ParseEnum, ASN1_TAG_ENUM, ASN1_NOT_TAGGED, &Value);
	Address->ScreeningIndicator = Value;
	if (p < end) {
		/* The optional subaddress must be present since there is something left. */
		XSEQUENCE_1(ParsePartySubaddress_Full, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &Address->Subaddress);
	} else {
		Address->Subaddress.Length = 0;	/* Subaddress not present */
	}

	return p - beg;
}				/* end ParseAddressScreened_Full() */

/* ******************************************************************* */
/*!
 * \brief Encode the PresentedAddressScreened type.
 *
 * \param Dest Where to put the encoding
 * \param Presented Address information to encode.
 *
 * \retval length
 */
int encodePresentedAddressScreened_Full(__u8 * Dest, const struct FacPresentedAddressScreened *Presented)
{
	__u8 *p;
	__u8 *TagStart;

	p = Dest;
	switch (Presented->Type) {
	case 0:		/* presentationAllowedAddress */
		TagStart = p;
		p += encodeAddressScreened_Full(p, &Presented->Address);
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 0;
		break;
	case 1:		/* presentationRestricted */
		p += encodeNull(p, ASN1_TAG_CONTEXT_SPECIFIC | 1);
		break;
	case 2:		/* numberNotAvailableDueToInterworking */
		p += encodeNull(p, ASN1_TAG_CONTEXT_SPECIFIC | 2);
		break;
	case 3:		/* presentationRestrictedAddress */
		TagStart = p;
		p += encodeAddressScreened_Full(p, &Presented->Address);
		TagStart[0] = ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED | 3;
		break;
	default:
		break;
	}			/* end switch */

	return p - Dest;
}				/* end encodePresentedAddressScreened_Full() */

/* ******************************************************************* */
/*!
 * \brief Parse the PresentedAddressScreened argument parameters.
 *
 * \param pc Complete component message storage data.
 * \param p Starting buffer position to parse arguments
 * \param end End buffer position that must not go past.
 * \param Presented Parameter storage to fill.
 *
 * \retval length of buffer consumed
 * \retval -1 on error.
 */
int ParsePresentedAddressScreened_Full(struct asn1_parm *pc, u_char * p, u_char * end,
				       struct FacPresentedAddressScreened *Presented)
{
	INIT;

	Presented->Type = 0;
	XCHOICE_1(ParseAddressScreened_Full, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 0, &Presented->Address);
	Presented->Type = 1;
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 1);
	Presented->Type = 2;
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 2);
	Presented->Type = 3;
	XCHOICE_1(ParseAddressScreened_Full, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 3, &Presented->Address);

	XCHOICE_DEFAULT;
}				/* end ParsePresentedAddressScreened_Full() */

/* ------------------------------------------------------------------- */
/* end asn1_address.c */
