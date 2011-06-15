/* $Id$
 *
 */

#include "asn1.h"
#include <string.h>

// ======================================================================
// AOC EN 300 182-1 V1.3.3

int ParseAOCDCurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	cur->InvokeID = pc->u.inv.invokeId;

	cur->chargeNotAvailable = 1;
	cur->freeOfCharge = 0;
	memset(cur->currency, 0, sizeof(cur->currency));
	cur->currencyAmount = 0;
	cur->multiplier = 0;
	cur->typeOfChargingInfo = -1;
	cur->billingId = -1;
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);	// chargeNotAvail
	cur->chargeNotAvailable = 0;
	XCHOICE_1(ParseAOCDCurrencyInfo, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, cur);
	XCHOICE_DEFAULT;
}

int ParseAOCDChargingUnit(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{
	INIT;

	chu->InvokeID = pc->u.inv.invokeId;

	chu->chargeNotAvailable = 1;
	chu->freeOfCharge = 0;
	chu->recordedUnits = 0;
	chu->typeOfChargingInfo = -1;
	chu->billingId = -1;
	memset(chu->chargeNumber, 0, sizeof(chu->chargeNumber));
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);	// chargeNotAvail
	chu->chargeNotAvailable = 0;
	XCHOICE_1(ParseAOCDChargingUnitInfo, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, chu);
	XCHOICE_DEFAULT;
}

// AOCECurrency

int ParseAOCECurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	cur->InvokeID = pc->u.inv.invokeId;

	cur->chargeNotAvailable = 1;
	cur->freeOfCharge = 0;
	memset(cur->currency, 0, sizeof(cur->currency));
	cur->currencyAmount = 0;
	cur->multiplier = 0;
	cur->typeOfChargingInfo = -1;
	cur->billingId = -1;

	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);	// chargeNotAvail
	cur->chargeNotAvailable = 0;
	XCHOICE_1(ParseAOCECurrencyInfo, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, cur);
	XCHOICE_DEFAULT;
}

// AOCEChargingUnit

int ParseAOCEChargingUnit(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{
	INIT;

	chu->InvokeID = pc->u.inv.invokeId;

	chu->chargeNotAvailable = 1;
	chu->freeOfCharge = 0;
	chu->recordedUnits = 0;
	chu->typeOfChargingInfo = -1;
	chu->billingId = -1;
	memset(chu->chargeNumber, 0, sizeof(chu->chargeNumber));

	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);	// chargeNotAvail
	XCHOICE_1(ParseAOCEChargingUnitInfo, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, chu);
	XCHOICE_DEFAULT;
}

// AOCDCurrencyInfo

int ParseAOCDSpecificCurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseRecordedCurrency, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 1, cur);
	XSEQUENCE_1(ParseTypeOfChargingInfo, ASN1_TAG_CONTEXT_SPECIFIC, 2, &cur->typeOfChargingInfo);
	XSEQUENCE_OPT_1(ParseAOCDBillingId, ASN1_TAG_CONTEXT_SPECIFIC, 3, &cur->billingId);

	return p - beg;
}

int ParseAOCDCurrencyInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XCHOICE_1(ParseAOCDSpecificCurrency, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, cur);

	cur->freeOfCharge = 1;
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 1);	// freeOfCharge
	cur->freeOfCharge = 0;
	XCHOICE_DEFAULT;
}

// AOCDChargingUnitInfo

int ParseAOCDSpecificChargingUnits(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{
	INIT;

	XSEQUENCE_1(ParseRecordedUnitsList, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 1, &chu->recordedUnits);
	XSEQUENCE_1(ParseTypeOfChargingInfo, ASN1_TAG_CONTEXT_SPECIFIC, 2, &chu->typeOfChargingInfo);
	XSEQUENCE_OPT_1(ParseAOCDBillingId, ASN1_TAG_CONTEXT_SPECIFIC, 3, &chu->billingId);

//      p_L3L4(pc, CC_CHARGE | INDICATION, &recordedUnits);

	return p - beg;
}

int ParseAOCDChargingUnitInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{
	INIT;

	XCHOICE_1(ParseAOCDSpecificChargingUnits, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, chu);

	chu->freeOfCharge = 1;
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 1);	// freeOfCharge
	chu->freeOfCharge = 0;

	XCHOICE_DEFAULT;
}

// RecordedCurrency

int ParseRecordedCurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseCurrency, ASN1_TAG_CONTEXT_SPECIFIC, 1, (char *)cur->currency);
	XSEQUENCE_1(ParseAmount, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 2, cur);

	return p - beg;
}

// RecordedUnitsList

int ParseRecordedUnitsList(struct asn1_parm *pc, u_char * p, u_char * end, int *recordedUnits)
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

int ParseTypeOfChargingInfo(struct asn1_parm *pc, u_char * p, u_char * end, int *typeOfChargingInfo)
{
	return ParseEnum(pc, p, end, typeOfChargingInfo);
}

// RecordedUnits

int ParseRecordedUnitsChoice(struct asn1_parm *pc, u_char * p, u_char * end, int *recordedUnits)
{
	INIT;

	XCHOICE_1(ParseNumberOfUnits, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, recordedUnits);
	XCHOICE(ParseNull, ASN1_TAG_NULL, ASN1_NOT_TAGGED);	// not available
	XCHOICE_DEFAULT;
}

int ParseRecordedUnits(struct asn1_parm *pc, u_char * p, u_char * end, int *recordedUnits)
{
	int typeOfUnit;
	INIT;

	XSEQUENCE_1(ParseRecordedUnitsChoice, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, recordedUnits);
	XSEQUENCE_OPT_1(ParseTypeOfUnit, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &typeOfUnit);

	return p - beg;
}

// AOCDBillingId

int ParseAOCDBillingId(struct asn1_parm *pc, u_char * p, u_char * end, int *billingId)
{
	return ParseEnum(pc, p, end, billingId);
}

/* #if 0 */
// AOCECurrencyInfo

int ParseAOCESpecificCurrency(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseRecordedCurrency, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 1, cur);
	XSEQUENCE_OPT_1(ParseAOCEBillingId, ASN1_TAG_CONTEXT_SPECIFIC, 2, &cur->billingId);

	return p - beg;
}

int ParseAOCECurrencyInfoChoice(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XCHOICE_1(ParseAOCESpecificCurrency, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, cur);
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 1);	// freeOfCharge
	XCHOICE_DEFAULT;
}

int ParseAOCECurrencyInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseAOCECurrencyInfoChoice, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, cur);
	XSEQUENCE_OPT_1(ParseChargingAssociation, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &cur->chargeAssoc);
	XCHOICE_DEFAULT;
}

// AOCEChargingUnitInfo

int ParseAOCESpecificChargingUnits(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{

	INIT;

	XSEQUENCE_1(ParseRecordedUnitsList, ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED, 1, &chu->recordedUnits);
	XSEQUENCE_OPT_1(ParseAOCEBillingId, ASN1_TAG_CONTEXT_SPECIFIC, 2, &chu->billingId);

	return p - beg;
}

int ParseAOCEChargingUnitInfoChoice(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{
	INIT;

	XCHOICE_1(ParseAOCESpecificChargingUnits, ASN1_TAG_SEQUENCE, ASN1_NOT_TAGGED, chu);
	XCHOICE(ParseNull, ASN1_TAG_CONTEXT_SPECIFIC, 1);	// freeOfCharge
	XCHOICE_DEFAULT;
}

int ParseAOCEChargingUnitInfo(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCChargingUnit *chu)
{
	INIT;

	XSEQUENCE_1(ParseAOCEChargingUnitInfoChoice, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, chu);
	XSEQUENCE_OPT_1(ParseChargingAssociation, ASN1_NOT_TAGGED, ASN1_NOT_TAGGED, &chu->chargeAssoc);
}

// AOCEBillingId

int ParseAOCEBillingId(struct asn1_parm *pc, u_char * p, u_char * end, int *billingId)
{
	return ParseEnum(pc, p, end, billingId);
}

// Currency

int ParseCurrency(struct asn1_parm *pc, u_char * p, u_char * end, char *currency)
{
	struct asn1ParseString str;

	str.buf = currency;
	str.maxSize = 11;	/* sizeof(struct FacAOCCurrency.currency) */
	return ParseIA5String(pc, p, end, &str);
}

// Amount

int ParseAmount(struct asn1_parm *pc, u_char * p, u_char * end, struct FacAOCCurrency *cur)
{
	INIT;

	XSEQUENCE_1(ParseCurrencyAmount, ASN1_TAG_CONTEXT_SPECIFIC, 1, &cur->currencyAmount);
	XSEQUENCE_1(ParseMultiplier, ASN1_TAG_CONTEXT_SPECIFIC, 2, &cur->multiplier);

	return p - beg;
}

// CurrencyAmount

int ParseCurrencyAmount(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *currencyAmount)
{
	return ParseUnsignedInteger(pc, p, end, currencyAmount);
}

// Multiplier

int ParseMultiplier(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *multiplier)
{
	return ParseEnum(pc, p, end, multiplier);
}

// TypeOfUnit

int ParseTypeOfUnit(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *typeOfUnit)
{
	return ParseUnsignedInteger(pc, p, end, typeOfUnit);
}

// NumberOfUnits

int ParseNumberOfUnits(struct asn1_parm *pc, u_char * p, u_char * end, unsigned int *numberOfUnits)
{
	return ParseUnsignedInteger(pc, p, end, numberOfUnits);
}

// Charging Association

int ParseChargingAssociation(struct asn1_parm *pc, u_char * p, u_char * end, struct ChargingAssociation *chargeAssoc)
{
	INIT;
	struct FacPartyNumber partyNumber;

	partyNumber.LengthOfNumber = 0;
	partyNumber.Number[0] = '\0';

	XCHOICE_1(ParsePartyNumber_Full, ASN1_TAG_SEQUENCE, 0, &partyNumber);

	if ((partyNumber.LengthOfNumber) && (partyNumber.LengthOfNumber <= 30)
	    && (partyNumber.Number[0] != '\0'))
		strcpy(chargeAssoc->chargeNumber, partyNumber.Number);

	XCHOICE_1(ParseChargeIdentifier, ASN1_TAG_INTEGER, ASN1_NOT_TAGGED, &chargeAssoc->chargeIdentifier);

	XCHOICE_DEFAULT;
}

// ChargeIdentifier

int ParseChargeIdentifier(struct asn1_parm *pc, u_char * p, u_char * end, int *chargeIdentifier)
{
	return ParseSignedInteger(pc, p, end, chargeIdentifier);
}
