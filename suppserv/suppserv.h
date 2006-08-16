/*
 * suppserv.h
 *
 * Copyright (C) 2006, Nadi Sarrar
 * Nadi Sarrar <nadi@beronet.com>
 *
 * Portions of this file are based on the mISDN sources
 * by Karsten Keil.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License, Version 2.
 *
 */

#ifndef __SUPPSERV_H__
#define __SUPPSERV_H__

#include "asn1.h"
#include <asm/types.h>

/*
 * Structs for Facility Messages
 */

enum FacFunction {
	Fac_None 					= 0xffff,
	Fac_GetSupportedServices 	= 0x0000,
	Fac_Listen 					= 0x0001,
	Fac_Suspend 				= 0x0004,
	Fac_Resume 					= 0x0005,
	Fac_CFActivate 				= 0x0009,
	Fac_CFDeactivate 			= 0x000a,
	Fac_CFInterrogateParameters = 0x000b,
	Fac_CFInterrogateNumbers 	= 0x000c,
	Fac_CD 						= 0x000d,
	Fac_AOCDCurrency 			= 0x0021,
	Fac_AOCDChargingUnit 		= 0x0022,
};

struct FacListen {
	__u32 NotificationMask;
};

struct FacSuspend {
	__s8  CallIdentity[16];
};

struct FacResume {
	__s8  CallIdentity[16];
};

struct FacCFActivate {
	__u32 Handle;
	__u16 Procedure;
	__u16 BasicService;
	__s8  ServedUserNumber[16];
	__s8  ForwardedToNumber[16];
	__s8  ForwardedToSubaddress[16];
};

struct FacCFDeactivate {
	__u32 Handle;
	__u16 Procedure;
	__u16 BasicService;
	__s8  ServedUserNumber[16];
};

struct FacCDeflection {
	__u16 PresentationAllowed;
	__s8  DeflectedToNumber[16];
	__s8  DeflectedToSubaddress[16];
};

#define FacCFInterrogateParameters FacCFDeactivate

struct FacCFInterrogateNumbers {
	__u32 Handle;
};

struct FacAOCDChargingUnit {
	__u16 chargeNotAvailable;
	__u16 freeOfCharge;
	__s32 recordedUnits;
	__s32 typeOfChargingInfo;
	__s32 billingId;
};

struct FacAOCDCurrency {
	__u16 chargeNotAvailable;
	__u16 freeOfCharge;
	__u8  currency[11];
	__s32 currencyAmount;
	__s32 multiplier;
	__s32 typeOfChargingInfo;
	__s32 billingId;
};

struct FacParm {
	enum FacFunction Function;
	union {
		struct FacListen Listen;
		struct FacSuspend Suspend;
		struct FacResume Resume;
		struct FacCFActivate CFActivate;
		struct FacCFDeactivate CFDeactivate;
		struct FacCFInterrogateParameters CFInterrogateParameters;
		struct FacCFInterrogateNumbers CFInterrogateNumbers;
		struct FacCDeflection CDeflection;
		struct FacAOCDChargingUnit AOCDchu;
		struct FacAOCDCurrency AOCDcur;
	} u;
};

/*
 * encodeFac (__u8 *dest, struct FacParm *fac)
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
int encodeFac (__u8 *dest, struct FacParm *fac);

/*
 * decodeFac (__u8 *src, struct FacParm *fac)
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
int decodeFac (__u8 *src, struct FacParm *fac);

#endif
