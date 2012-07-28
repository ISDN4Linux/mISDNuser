/*
 *
 * Three Party (3PTY) Supplementary Services ETS 300 188-2
 *
 * ECT Facility ie encode/decode
 *
 * Copyright 2009,2010  by Karsten Keil <kkeil@linux-pingi.de>
 * Copyright 2012  by Andreas Eversberg <jolly@eversberg.eu>
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

#include "asn1.h"
#include "3pty.h"

/* ******************************************************************* */
/*!
 * \brief Encode the Begin3PTY facility ie.
 *
 * \param Dest Where to put the encoding
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacBegin3PTY(__u8 * Dest, const struct asn1_parm *pc, void *unused)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_Begin3PTY);

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_Begin3PTY);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacEctLoopTest() */

/* ******************************************************************* */
/*!
 * \brief Encode the End3PTY facility ie.
 *
 * \param Dest Where to put the encoding
 *
 * \retval length on success.
 * \retval -1 on error.
 */
int encodeFacEnd3PTY(__u8 * Dest, const struct asn1_parm *pc, void *unused)
{
	int Length;
	__u8 *p;
	__u8 *SeqStart;

	switch (pc->comp) {
	case CompInvoke:
		p = encodeComponentInvoke_Head(Dest, pc->u.inv.invokeId, Fac_End3PTY);

		Length = encodeComponent_Length(Dest, p);
		break;
	case CompReturnResult:
		p = encodeComponent_Head(Dest, asn1ComponentTag_Result);
		p += encodeInt(p, ASN1_TAG_INTEGER, pc->u.retResult.invokeId);

		SeqStart = p;
		SeqStart[0] = ASN1_TAG_SEQUENCE;
		p = &SeqStart[2];

		p += encodeOperationValue(p, Fac_End3PTY);

		/* sequence Length */
		SeqStart[1] = p - &SeqStart[2];

		Length = encodeComponent_Length(Dest, p);
		break;
	default:
		Length = -1;
		break;
	}			/* end switch */

	return Length;
}				/* end encodeFacEctLoopTest() */
