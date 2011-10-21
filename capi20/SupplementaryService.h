/*
 * SupplementaryService.h
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2011  by Karsten Keil <kkeil@linux-pingi.de>
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

#ifndef _SUPPLEMENTARYSERVICE_H
#define _SUPPLEMENTARYSERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Supplementary Services
// ---------------------------------------------------------------------------

#define SuppServiceHR			0x00000001
#define SuppServiceTP			0x00000002
#define SuppServiceECT			0x00000004
#define SuppService3PTY			0x00000008
#define SuppServiceCF			0x00000010
#define SuppServiceCD			0x00000020
#define SuppServiceMCID			0x00000040
#define SuppServiceCCBS			0x00000080

#define mISDNSupportedServices		(SuppServiceCD | \
					 SuppServiceCF | \
					 SuppServiceTP | \
					 SuppServiceHR)

#define CapiSupplementaryServiceNotSupported	0x300e
#define CapiRequestNotAllowedInThisState	0x3010


// ---------------------------------------------------------------------------
// structs for Facillity requests
// ---------------------------------------------------------------------------

struct FacReqListen {
	uint32_t NotificationMask;
};

struct FacReqSuspend {
	unsigned char *CallIdentity;
};

struct FacReqResume {
	unsigned char *CallIdentity;
};

struct FacReqCFActivate {
	uint32_t Handle;
	uint16_t Procedure;
	uint16_t BasicService;
	unsigned char  *ServedUserNumber;
	unsigned char  *ForwardedToNumber;
	unsigned char  *ForwardedToSubaddress;
};

struct FacReqCFDeactivate {
	uint32_t Handle;
	uint16_t Procedure;
	uint16_t BasicService;
	unsigned char  *ServedUserNumber;
};

struct FacReqCDeflection {
	uint16_t PresentationAllowed;
	unsigned char  *DeflectedToNumber;
	unsigned char  *DeflectedToSubaddress;
};

#define FacReqCFInterrogateParameters FacReqCFDeactivate

struct FacReqCFInterrogateNumbers {
	uint32_t Handle;
};

struct FacReqParm {
	uint16_t Function;
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

// ---------------------------------------------------------------------------
// structs for Facillity confirms
// ---------------------------------------------------------------------------

struct FacConfGetSupportedServices {
	uint16_t SupplementaryServiceInfo;
	uint32_t SupportedServices;
};

struct FacConfInfo {
	uint16_t SupplementaryServiceInfo;
};

struct FacConfParm {
	uint16_t Function;
	union {
		struct FacConfGetSupportedServices GetSupportedServices;
		struct FacConfInfo Info;
	} u;
};

int SendSSNotificationEvent(struct lPLCI *, uint16_t);

#ifdef __cplusplus
}
#endif

#endif

