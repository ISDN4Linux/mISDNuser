/*
 * fac.c
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

#include "suppserv.h"
#include "asn1_diversion.h"
#include "l3dss1.h"
#include <string.h>

enum {
	SUPPLEMENTARY_SERVICE 	= 0x91,
} SERVICE_DISCRIMINATOR;

enum {
	INVOKE 					= 0xa1,
	RETURN_RESULT 			= 0xa2,
	RETURN_ERROR 			= 0xa3,
	REJECT 					= 0xa4,
} COMPONENT_TYPE_TAG;

enum {
	CALL_DEFLECT 			= 0x0d,
	AOC 					= 0x22,
} OPERATION_CODE;

/*
 * Facility IE Encoding
 */

static __u8* encodeInvokeComponentHead (__u8 *p, __u8 ie_id)
{
	*p++ = ie_id; // IE identifier
	*p++ = 0;     // length -- not known yet
	*p++ = 0x91;  // remote operations protocol
	*p++ = 0xa1;  // invoke component
	*p++ = 0;     // length -- not known yet
	return p;
}

static int encodeInvokeComponentLength (__u8 *msg, __u8 *p)
{
	msg[4] = p - &msg[5];
	msg[1] = p - &msg[2];
	return msg[1] + 2;
}

static int encodeFacCDeflection (__u8 *dest, struct FacReqCDeflection *CD)
{
	__u8 *p;
	p = encodeInvokeComponentHead(dest, IE_FACILITY);
	p += encodeInt(p, 0x02);
	p += encodeInt(p, 13); // Calldefection
	p += encodeInvokeDeflection(p, CD);
	return encodeInvokeComponentLength(dest, p);
}

int encodeFacReq (__u8 *dest, struct FacReqParm *fac)
{
	int len = -1;

	switch (fac->Function) {
	case FacReq_None:
	case FacReq_GetSupportedServices:
	case FacReq_Listen:
	case FacReq_Suspend:
	case FacReq_Resume:
	case FacReq_CFActivate:
	case FacReq_CFDeactivate:
	case FacReq_CFInterrogateParameters:
	case FacReq_CFInterrogateNumbers:
		break;
	case FacReq_CD:
		len = encodeFacCDeflection(dest, &(fac->u.CDeflection));
	}
	return len;
}

/*
 * Facility IE Decoding
 */

int decodeFacReq (__u8 *src, struct FacReqParm *fac)
{
	struct asn1_parm pc;
	int 	fac_len,
			offset;
	__u8 	*end,
		 	*p = src;

	if (!p)
		goto _dec_err;

	offset = ParseLen(p, p + 3, &fac_len);
	if (offset < 0)
		goto _dec_err;
	p += offset;
	end = p + fac_len;

/* 	ParseASN1(p + 1, end, 0); */

	if (*p++ != SUPPLEMENTARY_SERVICE)
		goto _dec_err;

	if (ParseComponent(&pc, p, end) == -1)
		goto _dec_err;

	switch (pc.comp) {
	case invoke:
		switch (pc.u.inv.operationValue) {
		case CALL_DEFLECT:
			fac->Function = FacReq_CD;
			if (pc.u.inv.o.reqCD.address.partyNumber.type == 0)
				strncpy((char *)fac->u.CDeflection.DeflectedToNumber,
						pc.u.inv.o.reqCD.address.partyNumber.p.unknown,
						sizeof(fac->u.CDeflection.DeflectedToNumber));
			else
				strncpy((char *)fac->u.CDeflection.DeflectedToNumber,
						pc.u.inv.o.reqCD.address.partyNumber.p.publicPartyNumber.numberDigits,
						sizeof(fac->u.CDeflection.DeflectedToNumber));
			fac->u.CDeflection.PresentationAllowed = pc.u.inv.o.reqCD.pres;
			*(fac->u.CDeflection.DeflectedToSubaddress) = 0;
			return 0;
		}
		break;
	case returnResult:
	case returnError:
	case reject:
		goto _dec_err;
	}

_dec_err:
	fac->Function = FacReq_None;
	return -1;
} 

