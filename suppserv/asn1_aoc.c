/* $Id: asn1_aoc.c,v 1.3 2006/08/16 14:15:52 nadi Exp $
 *
 */

#include "asn1.h"
#include <string.h>

// ======================================================================
// AOC EN 300 182-1 V1.3.3

int
ParseAOCDCurrency(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDCurrency *cur)
{
	INIT;

	cur->chargeNotAvailable = 1;
	cur->freeOfCharge = 0;
	memset(cur->currency, 0 , sizeof(cur->currency));
	cur->currencyAmount = 0;
	cur->multiplier = 0;
	cur->typeOfChargingInfo = -1;
	cur->billingId = -1;
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED); // chargeNotAvail
	cur->chargeNotAvailable = 0;
	XCHOICE_1(ParseAOCDCurrencyInfo, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, cur);
	XCHOICE_DEFAULT;
}

int
ParseAOCDChargingUnit(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDChargingUnit *chu)
{
	INIT;

	chu->chargeNotAvailable = 1;
	chu->freeOfCharge = 0;
	chu->recordedUnits = 0;
	chu->typeOfChargingInfo = -1;
	chu->billingId = -1;
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED); // chargeNotAvail
	chu->chargeNotAvailable = 0;
	XCHOICE_1(ParseAOCDChargingUnitInfo, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, chu);
	XCHOICE_DEFAULT;
}

#if 0
// AOCECurrency

int
ParseAOCECurrency(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	INIT;

	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED); // chargeNotAvail
	XCHOICE(ParseAOCECurrencyInfo, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED);
	XCHOICE_DEFAULT;
}
#endif

// AOCEChargingUnit

int
ParseAOCEChargingUnit(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	INIT;

	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED); // chargeNotAvail
	XCHOICE(ParseAOCEChargingUnitInfo, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED);
	XCHOICE_DEFAULT;
}

// AOCDCurrencyInfo

int
ParseAOCDSpecificCurrency(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseRecordedCurrency, ASN1_TAG_SEQUENCE, 1, cur);
	XSEQUENCE_1(ParseTypeOfChargingInfo, ASN1_TAG_ENUM, 2, &cur->typeOfChargingInfo);
	XSEQUENCE_OPT_1(ParseAOCDBillingId, ASN1_TAG_ENUM, 3, &cur->billingId);

	return p - beg;
}

int
ParseAOCDCurrencyInfo(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDCurrency *cur)
{
	INIT;

	XCHOICE_1(ParseAOCDSpecificCurrency, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, cur);

	cur->freeOfCharge = 1;
	XCHOICE(ParseNull, ASN1_TAG_NULL, 1); // freeOfCharge
	cur->freeOfCharge = 0;
	XCHOICE_DEFAULT;
}

// AOCDChargingUnitInfo

int
ParseAOCDSpecificChargingUnits(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDChargingUnit *chu)
{
	INIT;

	XSEQUENCE_1(ParseRecordedUnitsList, ASN1_TAG_SEQUENCE, 1, &chu->recordedUnits);
	XSEQUENCE_1(ParseTypeOfChargingInfo, ASN1_TAG_ENUM, 2, &chu->typeOfChargingInfo);
	XSEQUENCE_OPT_1(ParseAOCDBillingId, ASN1_TAG_ENUM, 3, &chu->billingId);

//	p_L3L4(pc, CC_CHARGE | INDICATION, &recordedUnits);

	return p - beg;
}

int
ParseAOCDChargingUnitInfo(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDChargingUnit *chu)
{
	INIT;

	XCHOICE_1(ParseAOCDSpecificChargingUnits, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, chu);

	chu->freeOfCharge = 1;
	XCHOICE(ParseNull, ASN1_TAG_NULL, 1); // freeOfCharge
	chu->freeOfCharge = 0;

	XCHOICE_DEFAULT;
}

// RecordedCurrency

int
ParseRecordedCurrency(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseCurrency, ASN1_TAG_IA5_STRING, 1, (char *)cur->currency);
	XSEQUENCE_1(ParseAmount, ASN1_TAG_SEQUENCE, 2, cur);

	return p - beg;
}

// RecordedUnitsList

int
ParseRecordedUnitsList(struct asn1_parm *pc, u_char *p, u_char *end, int *recordedUnits)
{
	int i;
	int units;
	INIT;

	*recordedUnits = 0;
	XSEQUENCE_1(ParseRecordedUnits, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, recordedUnits);
	for (i = 0; i < 31; i++) {
		units = 0;
		XSEQUENCE_OPT_1(ParseRecordedUnits, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, &units);
		*recordedUnits += units;
	}

	return p - beg;
}

// TypeOfChargingInfo

int
ParseTypeOfChargingInfo(struct asn1_parm *pc, u_char *p, u_char *end, int *typeOfChargingInfo)
{
	return ParseEnum(pc, p, end, typeOfChargingInfo);
}

// RecordedUnits

int
ParseRecordedUnitsChoice(struct asn1_parm *pc, u_char *p, u_char *end, int *recordedUnits)
{
	INIT;

	XCHOICE_1(ParseNumberOfUnits, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, recordedUnits);
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED); // not available
	XCHOICE_DEFAULT;
}

int
ParseRecordedUnits(struct asn1_parm *pc, u_char *p, u_char *end, int *recordedUnits)
{
	int typeOfUnit;
	INIT;

	XSEQUENCE_1(ParseRecordedUnitsChoice, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, recordedUnits);
	XSEQUENCE_OPT_1(ParseTypeOfUnit, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &typeOfUnit);

	return p - beg;
}

// AOCDBillingId

int
ParseAOCDBillingId(struct asn1_parm *pc, u_char *p, u_char *end, int *billingId)
{
	return ParseEnum(pc, p, end, billingId);
}

#if 0
// AOCECurrencyInfo

int
ParseAOCESpecificCurrency(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	int billingId;
	INIT;

	XSEQUENCE(ParseRecordedCurrency, ASN1_TAG_SEQUENCE, 1);
	XSEQUENCE_OPT_1(ParseAOCEBillingId, ASN1_TAG_ENUM, 2, &billingId);

	return p - beg;
}

int
ParseAOCECurrencyInfoChoice(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	INIT;

	XCHOICE(ParseAOCESpecificCurrency, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED);
	XCHOICE(ParseNull, ASN1_TAG_NULL, 1); // freeOfCharge
	XCHOICE_DEFAULT;
}

int
ParseAOCECurrencyInfo(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	INIT;

	XSEQUENCE(ParseAOCECurrencyInfoChoice, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED);
	XSEQUENCE_OPT(ParseChargingAssociation, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED);
	XCHOICE_DEFAULT;
}
#endif

// AOCEChargingUnitInfo

int
ParseAOCESpecificChargingUnits(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	int recordedUnits;
	int billingId;
	INIT;

	XSEQUENCE_1(ParseRecordedUnitsList, ASN1_TAG_SEQUENCE, 1, &recordedUnits);
	XSEQUENCE_OPT_1(ParseAOCEBillingId, ASN1_TAG_ENUM, 2, &billingId);

//	p_L3L4(pc, CC_CHARGE | INDICATION, &recordedUnits);

	return p - beg;
}

int
ParseAOCEChargingUnitInfoChoice(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	INIT;

	XCHOICE(ParseAOCESpecificChargingUnits, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED);
	XCHOICE(ParseNull, ASN1_TAG_NULL, 1); // freeOfCharge
	XCHOICE_DEFAULT;
}

int
ParseAOCEChargingUnitInfo(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	INIT;

	XSEQUENCE(ParseAOCEChargingUnitInfoChoice, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED);
	XSEQUENCE_OPT(ParseChargingAssociation, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED);

	return p - beg;
}

// AOCEBillingId

int
ParseAOCEBillingId(struct asn1_parm *pc, u_char *p, u_char *end, int *billingId)
{
	return ParseEnum(pc, p, end, billingId);
}

// Currency

int
ParseCurrency(struct asn1_parm *pc, u_char *p, u_char *end, char *currency)
{
	return ParseIA5String(pc, p, end, currency);
}

// Amount

int
ParseAmount(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseCurrencyAmount, ASN1_TAG_INTEGER, 1, &cur->currencyAmount);
	XSEQUENCE_1(ParseMultiplier, ASN1_TAG_INTEGER, 2, &cur->multiplier);

	return p - beg;
}

// CurrencyAmount

int
ParseCurrencyAmount(struct asn1_parm *pc, u_char *p, u_char *end, int *currencyAmount)
{
	return ParseInteger(pc, p, end, currencyAmount);
}

// Multiplier

int
ParseMultiplier(struct asn1_parm *pc, u_char *p, u_char *end, int *multiplier)
{
	return ParseEnum(pc, p, end, multiplier);
}

// TypeOfUnit

int
ParseTypeOfUnit(struct asn1_parm *pc, u_char *p, u_char *end, int *typeOfUnit)
{
	return ParseInteger(pc, p, end, typeOfUnit);
}

// NumberOfUnits

int
ParseNumberOfUnits(struct asn1_parm *pc, u_char *p, u_char *end, int *numberOfUnits)
{
	return ParseInteger(pc, p, end, numberOfUnits);
}

// Charging Association

int
ParseChargingAssociation(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
//	char partyNumber[30];
	INIT;

//	XCHOICE_1(ParsePartyNumber, ASN1_TAG_SEQUENCE, 0, partyNumber);
	XCHOICE(ParseChargeIdentifier, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED);
	XCHOICE_DEFAULT;
}

// ChargeIdentifier

int
ParseChargeIdentifier(struct asn1_parm *pc, u_char *p, u_char *end, int dummy)
{
	int chargeIdentifier;

	return ParseInteger(pc, p, end, &chargeIdentifier);
}

