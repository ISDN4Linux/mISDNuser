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
 * Structs for Facility Requests
 */

enum FacReqFunction {
	FacReq_None 					= 0xffff,
	FacReq_GetSupportedServices 	= 0x0000,
	FacReq_Listen 					= 0x0001,
	FacReq_Suspend 					= 0x0004,
	FacReq_Resume 					= 0x0005,
	FacReq_CFActivate 				= 0x0009,
	FacReq_CFDeactivate 			= 0x000a,
	FacReq_CFInterrogateParameters 	= 0x000b,
	FacReq_CFInterrogateNumbers 	= 0x000c,
	FacReq_CD 						= 0x000d,
};

struct FacReqListen {
	__u32 NotificationMask;
};

struct FacReqSuspend {
	__s8  CallIdentity[16];
};

struct FacReqResume {
	__s8  CallIdentity[16];
};

struct FacReqCFActivate {
	__u32 Handle;
	__u16 Procedure;
	__u16 BasicService;
	__s8  ServedUserNumber[16];
	__s8  ForwardedToNumber[16];
	__s8  ForwardedToSubaddress[16];
};

struct FacReqCFDeactivate {
	__u32 Handle;
	__u16 Procedure;
	__u16 BasicService;
	__s8  ServedUserNumber[16];
};

struct FacReqCDeflection {
	__u16 PresentationAllowed;
	__s8  DeflectedToNumber[16];
	__s8  DeflectedToSubaddress[16];
};

#define FacReqCFInterrogateParameters FacReqCFDeactivate

struct FacReqCFInterrogateNumbers {
	__u32 Handle;
};

struct FacReqParm {
	enum FacReqFunction Function;
	union {
		struct FacReqListen Listen;
		struct FacReqSuspend Suspend;
		struct FacReqResume Resume;
		struct FacReqCFActivate CFActivate;
		struct FacReqCFDeactivate CFDeactivate;
		struct FacReqCFInterrogateParameters CFInterrogateParameters;
		struct FacReqCFInterrogateNumbers CFInterrogateNumbers;
		struct FacReqCDeflection CDeflection;
	} u;
};

/*
 * Structs for Facility Confirms
 */

enum FacConfFunction {
	FacConf_GetSupportedServices 		= 0x0000,
	FacConf_Listen 						= 0x0001,
	FacConf_Hold 						= 0x0002,
	FacConf_Retrieve 					= 0x0003,
	FacConf_Suspend 					= 0x0004,
	FacConf_Resume 						= 0x0005,
	FacConf_CFActivate 					= 0x0009,
	FacConf_CFDeactivate 				= 0x000a,
	FacConf_CFInterrogateParameters 	= 0x000b,
	FacConf_CFInterrogateNumbers 		= 0x000c,
	FacConf_CD 							= 0x000d,
};

struct FacConfGetSupportedServices {
	__u16 SupplementaryServiceInfo;
	__u32 SupportedServices;
};

struct FacConfInfo {
	__u16 SupplementaryServiceInfo;
};

struct FacConfParm {
	enum FacConfFunction Function;
	union {
		struct FacConfGetSupportedServices GetSupportedServices;
		struct FacConfInfo Info;
	} u;
};

/*
 * encodeFacReq (__u8 *dest, struct FacReqParm *fac)
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
int encodeFacReq (__u8 *dest, struct FacReqParm *fac);

/*
 * decodeFacReq (__u8 *src, struct FacReqParm *fac)
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
int decodeFacReq (__u8 *src, struct FacReqParm *fac);

#endif
