/*
 * $Id$
 *
 * Diversion Supplementary Services ETS 300 207-1 Table 3
 *
 * Diversion Facility ie encode/decode header
 *
 * Copyright 2009,2010  by Karsten Keil <kkeil@linux-pingi.de>
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

#ifndef __ASN1_DIVERSION_H__
#define __ASN1_DIVERSION_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

	int encodeFacActivationDiversion(__u8 * Dest, const struct asn1_parm *pc, const struct FacActivationDiversion *ActivationDiversion);
	int ParseActivationDiversion(struct asn1_parm *pc, u_char * p, u_char * end,
					 struct FacActivationDiversion *ActivationDiversion);

	int encodeFacDeactivationDiversion(__u8 * Dest, const struct asn1_parm *pc, const struct FacDeactivationDiversion *DeactivationDiversion);
	int ParseDeactivationDiversion(struct asn1_parm *pc, u_char * p, u_char * end,
					   struct FacDeactivationDiversion *DeactivationDiversion);

	int encodeFacActivationStatusNotificationDiv(__u8 * Dest, const struct asn1_parm *pc,
						     const struct FacActivationStatusNotificationDiv
						     *ActivationStatusNotificationDiv);
	int ParseActivationStatusNotificationDiv(struct asn1_parm *pc, u_char * p, u_char * end,
						     struct FacActivationStatusNotificationDiv *ActivationStatusNotificationDiv);

	int encodeFacDeactivationStatusNotificationDiv(__u8 * Dest, const struct asn1_parm *pc,
						       const struct FacDeactivationStatusNotificationDiv
						       *DeactivationStatusNotificationDiv);
	int ParseDeactivationStatusNotificationDiv(struct asn1_parm *pc, u_char * p, u_char * end,
						       struct FacDeactivationStatusNotificationDiv
						       *DeactivationStatusNotificationDiv);

	int encodeFacInterrogationDiversion(__u8 * Dest, const struct asn1_parm *pc, const struct FacInterrogationDiversion *InterrogationDiversion);
	int ParseInterrogationDiversion(struct asn1_parm *pc, u_char * p, u_char * end,
					    struct FacInterrogationDiversion *InterrogationDiversion);
#define ParseInterrogationDiversion_RES		ParseIntResultList
	int ParseIntResultList(struct asn1_parm *pc, u_char * p, u_char * end, struct FacForwardingList *IntResultList);

	int encodeFacDiversionInformation(__u8 * Dest, const struct asn1_parm *pc, const struct FacDiversionInformation *DiversionInformation);
	int ParseDiversionInformation(struct asn1_parm *pc, u_char * p, u_char * end,
					  struct FacDiversionInformation *DiversionInformation);

	int encodeFacCallDeflection(__u8 * Dest, const struct asn1_parm *pc, const struct FacCallDeflection *CallDeflection);
	int ParseCallDeflection(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallDeflection *CallDeflection);

	int encodeFacCallRerouteing(__u8 * Dest, const struct asn1_parm *pc, const struct FacCallRerouteing *CallRerouteing);
	int ParseCallRerouteing(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallRerouteing *CallRerouteing);

	int encodeFacInterrogateServedUserNumbers(__u8 * Dest, const struct asn1_parm *pc,
						  const struct FacServedUserNumberList *InterrogateServedUserNumbers);
#define ParseInterrogateServedUserNumbers_RES		ParseServedUserNumberList
	int ParseServedUserNumberList(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacServedUserNumberList *ServedUserNumberList);

	int encodeFacDivertingLegInformation1(__u8 * Dest, const struct asn1_parm *pc, const struct FacDivertingLegInformation1 *DivertingLegInformation1);
	int ParseDivertingLegInformation1(struct asn1_parm *pc, u_char * p, u_char * end,
					      struct FacDivertingLegInformation1 *DivertingLegInformation1);

	int encodeFacDivertingLegInformation2(__u8 * Dest, const struct asn1_parm *pc, const struct FacDivertingLegInformation2 *DivertingLegInformation2);
	int ParseDivertingLegInformation2(struct asn1_parm *pc, u_char * p, u_char * end,
					      struct FacDivertingLegInformation2 *DivertingLegInformation2);

	int encodeFacDivertingLegInformation3(__u8 * Dest, const struct asn1_parm *pc, const struct FacDivertingLegInformation3 *DivertingLegInformation3);
	int ParseDivertingLegInformation3(struct asn1_parm *pc, u_char * p, u_char * end,
					      struct FacDivertingLegInformation3 *DivertingLegInformation3);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_DIVERSION_H__ */
/* ------------------------------------------------------------------- *//* end asn1_diversion.h */
