#ifndef __ASN1_H__
#define __ASN1_H__

#include "suppserv.h"
#include <asm/types.h>
#include <sys/types.h>
#include <stdio.h>

typedef enum {
	invoke       = 1,
	returnResult = 2,
	returnError  = 3,
	reject       = 4,
} asn1Component;

typedef enum {
	GeneralP     = 0,
	InvokeP      = 1,
	ReturnResultP= 2,
	ReturnErrorP = 3,
} asn1Problem;

struct PublicPartyNumber {
	int publicTypeOfNumber;
	char numberDigits[30];
};

struct PartyNumber {
	int type;
	union {
		char unknown[30];
		struct PublicPartyNumber publicPartyNumber;
	} p;
};

struct Address {
	struct PartyNumber partyNumber;
	char partySubaddress[30];
};

struct ServedUserNr {
	int all;
	struct PartyNumber partyNumber;
};

struct ActDivNotification {
	int procedure;
	int basicService;
	struct ServedUserNr servedUserNr;
	struct Address address;
};

struct DeactDivNotification {
	int procedure;
	int basicService;
	struct ServedUserNr servedUserNr;
};

struct ReqCallDeflection {
	struct Address address;
	int pres;
};

struct ServedUserNumberList {
	struct PartyNumber partyNumber[10];
};

struct IntResult {
	struct ServedUserNr servedUserNr;
	int procedure;
	int basicService;
	struct Address address;
};

struct IntResultList {
	struct IntResult intResult[10];
};

struct asn1Invoke {
	__u16 invokeId;
	__u16 operationValue;
	union {
		struct ActDivNotification actNot;
		struct DeactDivNotification deactNot;
		struct ReqCallDeflection reqCD;
		struct FacAOCDChargingUnit AOCDchu;
		struct FacAOCDCurrency AOCDcur;
	} o;
};

struct asn1ReturnResult {
	__u16 invokeId;
	union {
		struct ServedUserNumberList list;
		struct IntResultList resultList;
	} o;
};

struct asn1ReturnError {
	__u16 invokeId;
	__u16 errorValue;
	__u8 error[32];
};

struct asn1Reject {
	int invokeId;
	asn1Problem problem;
	__u16 problemValue;
};

struct asn1_parm {
	asn1Component comp;
	union {
		struct asn1Invoke       inv;
		struct asn1ReturnResult retResult;
		struct asn1ReturnError  retError;
		struct asn1Reject	reject;
	} u;
};


#ifdef ASN1_DEBUG
#define print_asn1msg(dummy, fmt, args...) printf(fmt, ## args)
int ParseASN1(u_char *p, u_char *end, int level);
#else
#define print_asn1msg(dummy, fmt, args...) 
#define ParseASN1(p,end,level)
#endif

#define int_error() \
	printf("mISDN: INTERNAL ERROR in %s:%d\n", \
		   __FILE__, __LINE__)

int ParseTag(u_char *p, u_char *end, int *tag);
int ParseLen(u_char *p, u_char *end, int *len);

#define ASN1_TAG_BOOLEAN           (0x01) // is that true?
#define ASN1_TAG_INTEGER           (0x02)
#define ASN1_TAG_BIT_STRING        (0x03)
#define ASN1_TAG_OCTET_STRING      (0x04)
#define ASN1_TAG_NULL              (0x05)
#define ASN1_TAG_OBJECT_IDENTIFIER (0x06)
#define ASN1_TAG_ENUM              (0x0a)
#define ASN1_TAG_SEQUENCE          (0x30)
#define ASN1_TAG_SET               (0x31)
#define ASN1_TAG_NUMERIC_STRING    (0x12)
#define ASN1_TAG_PRINTABLE_STRING  (0x13)
#define ASN1_TAG_IA5_STRING        (0x16)
#define ASN1_TAG_UTC_TIME          (0x17)

#define ASN1_TAG_CONSTRUCTED       (0x20)
#define ASN1_TAG_CONTEXT_SPECIFIC  (0x80)

#define ASN1_TAG_EXPLICIT          (0x100)
#define ASN1_TAG_OPT               (0x200)
#define ASN1_NOT_TAGGED            (0x400)

#define CallASN1(ret, p, end, todo) do { \
        ret = todo; \
	if (ret < 0) { \
                int_error(); \
                return -1; \
        } \
        p += ret; \
} while (0)

#define INIT \
	int tag, len; \
	int ret; \
	u_char *beg; \
        \
        print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> %s\n", __FUNCTION__); \
	beg = p; \
	CallASN1(ret, p, end, ParseTag(p, end, &tag)); \
	CallASN1(ret, p, end, ParseLen(p, end, &len)); \
        if (len >= 0) { \
                if (p + len > end) \
                        return -1; \
                end = p + len; \
        }

#define XSEQUENCE_1(todo, act_tag, the_tag, arg1) do { \
	if (p < end) { \
  	        if (((the_tag) &~ ASN1_TAG_OPT) == ASN1_NOT_TAGGED) { \
		        if (((u_char)act_tag == *p) || ((act_tag) == ASN1_NOT_TAGGED)) { \
			        CallASN1(ret, p, end, todo(pc, p, end, arg1)); \
                        } else { \
                                if (!((the_tag) & ASN1_TAG_OPT)) { \
                                        print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> err 1 %s:%d\n", __FUNCTION__, __LINE__); \
                	    	        return -1; \
                                } \
                        } \
	        } else { \
                        if ((the_tag) & ASN1_TAG_EXPLICIT) { \
		                if ((u_char)(((the_tag) & 0xff) | (ASN1_TAG_CONTEXT_SPECIFIC | ASN1_TAG_CONSTRUCTED)) == *p) { \
                                        int xtag, xlen; \
	                                CallASN1(ret, p, end, ParseTag(p, end, &xtag)); \
			                CallASN1(ret, p, end, ParseLen(p, end, &xlen)); \
  	                                CallASN1(ret, p, end, todo(pc, p, end, arg1)); \
                                } else { \
                                        if (!(the_tag) & ASN1_TAG_OPT) { \
                                                print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> err 2 %s:%d\n", __FUNCTION__, __LINE__); \
                        	    	        return -1; \
                                        } \
                                } \
                        } else { \
		                if ((u_char)(((the_tag) & 0xff) | (ASN1_TAG_CONTEXT_SPECIFIC | (act_tag & ASN1_TAG_CONSTRUCTED))) == *p) { \
  	                                CallASN1(ret, p, end, todo(pc, p, end, arg1)); \
                                } else { \
                                        if (!(the_tag) & ASN1_TAG_OPT) { \
                                                print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> err 3 %s:%d\n", __FUNCTION__, __LINE__); \
                        	    	        return -1; \
                                        } \
                                } \
		        } \
		} \
        } else { \
                if (!(the_tag) & ASN1_TAG_OPT) { \
                        print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> err 4 %s:%d\n", __FUNCTION__, __LINE__); \
			return -1; \
                } \
        } \
} while (0)

#define XSEQUENCE_OPT_1(todo, act_tag, the_tag, arg1) \
        XSEQUENCE_1(todo, act_tag, (the_tag | ASN1_TAG_OPT), arg1)

#define XSEQUENCE(todo, act_tag, the_tag) XSEQUENCE_1(todo, act_tag, the_tag, -1)
#define XSEQUENCE_OPT(todo, act_tag, the_tag) XSEQUENCE_OPT_1(todo, act_tag, the_tag, -1)

#define XCHOICE_1(todo, act_tag, the_tag, arg1) \
	if (act_tag == ASN1_NOT_TAGGED) { \
		return todo(pc, beg, end, arg1); \
        } \
        if (the_tag == ASN1_NOT_TAGGED) { \
		  if (act_tag == tag) { \
                            return todo(pc, beg, end, arg1); \
                  } \
         } else { \
		  if ((the_tag | (0x80 | (act_tag & 0x20))) == tag) { \
                            return todo(pc, beg, end, arg1); \
                  } \
	 }

#define XCHOICE(todo, act_tag, the_tag) XCHOICE_1(todo, act_tag, the_tag, -1)

#define XCHOICE_DEFAULT do {\
          print_asn1msg(PRT_DEBUG_DECODE, " DEBUG> err 5 %s:%d\n", __FUNCTION__, __LINE__); \
          return -1; \
	  } while (0)

#define CHECK_P do { \
        if (p >= end) \
                 return -1; \
        } while (0) 

/*
** ASN.1 Encoding
*/

int encodeNull(__u8 *dest);
int encodeBoolean(__u8 *dest, __u32 i);
int encodeInt(__u8 *dest, __u32 i);
int encodeEnum(__u8 *dest, __u32 i);
int encodeNumberDigits(__u8 *dest, __s8 *nd, __u8 len);
int encodePublicPartyNumber(__u8 *dest, __s8 *facilityPartyNumber);
int encodePartyNumber(__u8 *dest, __s8 *facilityPartyNumber);
int encodeServedUserNumber(__u8 *dest, __s8 *servedUserNumber);
int encodeAddress(__u8 *dest, __s8 *facilityPartyNumber, __s8 *calledPartySubaddress);

/*
** ASN.1 Parsing
*/

int ParseBoolean(struct asn1_parm *pc, u_char *p, u_char *end, int *i);
int ParseNull(struct asn1_parm *pc, u_char *p, u_char *end, int dummy);
int ParseInteger(struct asn1_parm *pc, u_char *p, u_char *end, int *i);
int ParseEnum(struct asn1_parm *pc, u_char *p, u_char *end, int *i);
int ParseIA5String(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseNumericString(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseOctetString(struct asn1_parm *pc, u_char *p, u_char *end, char *str);

int ParseARGReqCallDeflection(struct asn1_parm *pc, u_char *p, u_char *end, struct ReqCallDeflection *reqCD);
int ParseARGActivationStatusNotificationDiv(struct asn1_parm *pc, u_char *p, u_char *end, struct ActDivNotification *actNot);
int ParseARGDeactivationStatusNotificationDiv(struct asn1_parm *pc, u_char *p, u_char *end, struct DeactDivNotification *deactNot);
int ParseARGInterrogationDiversion(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseRESInterrogationDiversion(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseARGInterrogateServedUserNumbers(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseRESInterrogateServedUserNumbers(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseARGDiversionInformation(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseIntResult(struct asn1_parm *parm, u_char *p, u_char *end, struct IntResult *intResult);
int ParseIntResultList(struct asn1_parm *parm, u_char *p, u_char *end, struct IntResultList *intResultList);
int ParseServedUserNr(struct asn1_parm *parm, u_char *p, u_char *end, struct ServedUserNr *servedUserNr);
int ParseProcedure(struct asn1_parm *pc, u_char *p, u_char *end, int *procedure);
int ParseServedUserNumberList(struct asn1_parm *parm, u_char *p, u_char *end, struct ServedUserNumberList *list);
int ParseDiversionReason(struct asn1_parm *parm, u_char *p, u_char *end, char *str);

int ParsePresentedAddressScreened(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParsePresentedNumberScreened(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParsePresentedNumberUnscreened(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseAddressScreened(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseNumberScreened(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseAddress(struct asn1_parm *pc, u_char *p, u_char *end, struct Address *address);
int ParsePartyNumber(struct asn1_parm *pc, u_char *p, u_char *end, struct PartyNumber *partyNumber);
int ParsePublicPartyNumber(struct asn1_parm *pc, u_char *p, u_char *end, struct PublicPartyNumber *publicPartyNumber);
int ParsePrivatePartyNumber(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParsePublicTypeOfNumber(struct asn1_parm *pc, u_char *p, u_char *end, int *publicTypeOfNumber);
int ParsePrivateTypeOfNumber(struct asn1_parm *pc, u_char *p, u_char *end, int *privateTypeOfNumber);
int ParsePartySubaddress(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseUserSpecifiedSubaddress(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseNSAPSubaddress(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseSubaddressInformation(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseScreeningIndicator(struct asn1_parm *pc, u_char *p, u_char *end, char *str);
int ParseNumberDigits(struct asn1_parm *pc, u_char *p, u_char *end, char *str);

int ParseInvokeId(struct asn1_parm *parm, u_char *p, u_char *end, int *invokeId);
int ParseOperationValue(struct asn1_parm *parm, u_char *p, u_char *end, int *operationValue);
int ParseInvokeComponent(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseReturnResultComponent(struct asn1_parm *parm, u_char *p, u_char *end, int dummy);
int ParseComponent(struct asn1_parm *parm, u_char *p, u_char *end);
int XParseComponent(struct asn1_parm *parm, u_char *p, u_char *end);

int ParseAOCECurrency(struct asn1_parm *pc, u_char *p, u_char *end, int dummy);
int ParseAOCDChargingUnit(struct asn1_parm *pc,u_char *p, u_char *end, struct FacAOCDChargingUnit *chu);
int ParseAOCDCurrency(struct asn1_parm *pc, u_char *p, u_char *end, struct FacAOCDCurrency *cur);
int ParseAOCDCurrencyInfo(struct asn1_parm *pc,u_char *p, u_char *end, struct FacAOCDCurrency *cur);
int ParseAOCDChargingUnitInfo(struct asn1_parm *pc,u_char *p, u_char *end, struct FacAOCDChargingUnit *chu);
int ParseRecordedCurrency(struct asn1_parm *pc,u_char *p, u_char *end, struct FacAOCDCurrency *cur);
int ParseRecordedUnitsList(struct asn1_parm *pc,u_char *p, u_char *end, int *recordedUnits);
int ParseTypeOfChargingInfo(struct asn1_parm *pc,u_char *p, u_char *end, int *typeOfChargingInfo);
int ParseRecordedUnits(struct asn1_parm *pc,u_char *p, u_char *end, int *recordedUnits);
int ParseAOCDBillingId(struct asn1_parm *pc, u_char *p, u_char *end, int *billingId);
int ParseAOCECurrencyInfo(struct asn1_parm *pc, u_char *p, u_char *end, int dummy);
int ParseAOCEChargingUnitInfo(struct asn1_parm *pc,u_char *p, u_char *end, int dummy);
int ParseAOCEBillingId(struct asn1_parm *pc,u_char *p, u_char *end, int *billingId);
int ParseCurrency(struct asn1_parm *pc,u_char *p, u_char *end, char *currency);
int ParseAmount(struct asn1_parm *pc,u_char *p, u_char *end, struct FacAOCDCurrency *cur);
int ParseCurrencyAmount(struct asn1_parm *pc,u_char *p, u_char *end, int *currencyAmount);
int ParseMultiplier(struct asn1_parm *pc,u_char *p, u_char *end, int *multiplier);
int ParseTypeOfUnit(struct asn1_parm *pc,u_char *p, u_char *end, int *typeOfUnit);
int ParseNumberOfUnits(struct asn1_parm *pc,u_char *p, u_char *end, int *numberOfUnits);
int ParseChargingAssociation(struct asn1_parm *pc,u_char *p, u_char *end, int dummy);
int ParseChargeIdentifier(struct asn1_parm *pc,u_char *p, u_char *end, int dummy);

int ParseBasicService(struct asn1_parm *pc, u_char *p, u_char *end, int *basicService);

#endif
