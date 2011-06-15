/*
 * $Id$
 *
 * Diversion Supplementary Services ETS 300 207-1 Table 3
 *
 * Diversion Facility ie encode/decode header
 */

#ifndef __ASN1_DIVERSION_H__
#define __ASN1_DIVERSION_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

	int encodeFacActivationDiversion(__u8 * Dest, const struct FacActivationDiversion *ActivationDiversion);
	int ParseActivationDiversion_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					 struct FacActivationDiversion_ARG *ActivationDiversion);

	int encodeFacDeactivationDiversion(__u8 * Dest, const struct FacDeactivationDiversion *DeactivationDiversion);
	int ParseDeactivationDiversion_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					   struct FacDeactivationDiversion_ARG *DeactivationDiversion);

	int encodeFacActivationStatusNotificationDiv(__u8 * Dest,
						     const struct FacActivationStatusNotificationDiv
						     *ActivationStatusNotificationDiv);
	int ParseActivationStatusNotificationDiv_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
						     struct FacActivationStatusNotificationDiv *ActivationStatusNotificationDiv);

	int encodeFacDeactivationStatusNotificationDiv(__u8 * Dest,
						       const struct FacDeactivationStatusNotificationDiv
						       *DeactivationStatusNotificationDiv);
	int ParseDeactivationStatusNotificationDiv_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
						       struct FacDeactivationStatusNotificationDiv
						       *DeactivationStatusNotificationDiv);

	int encodeFacInterrogationDiversion(__u8 * Dest, const struct FacInterrogationDiversion *InterrogationDiversion);
	int ParseInterrogationDiversion_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					    struct FacInterrogationDiversion_ARG *InterrogationDiversion);
#define ParseInterrogationDiversion_RES		ParseIntResultList
	int ParseIntResultList(struct asn1_parm *pc, u_char * p, u_char * end, struct FacForwardingList *IntResultList);

	int encodeFacDiversionInformation(__u8 * Dest, const struct FacDiversionInformation *DiversionInformation);
	int ParseDiversionInformation_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					  struct FacDiversionInformation *DiversionInformation);

	int encodeFacCallDeflection(__u8 * Dest, const struct FacCallDeflection *CallDeflection);
	int ParseCallDeflection_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallDeflection_ARG *CallDeflection);

	int encodeFacCallRerouteing(__u8 * Dest, const struct FacCallRerouteing *CallRerouteing);
	int ParseCallRerouteing_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallRerouteing_ARG *CallRerouteing);

	int encodeFacInterrogateServedUserNumbers(__u8 * Dest,
						  const struct FacInterrogateServedUserNumbers *InterrogateServedUserNumbers);
#define ParseInterrogateServedUserNumbers_RES		ParseServedUserNumberList
	int ParseServedUserNumberList(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacServedUserNumberList *ServedUserNumberList);

	int encodeFacDivertingLegInformation1(__u8 * Dest, const struct FacDivertingLegInformation1 *DivertingLegInformation1);
	int ParseDivertingLegInformation1_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					      struct FacDivertingLegInformation1 *DivertingLegInformation1);

	int encodeFacDivertingLegInformation2(__u8 * Dest, const struct FacDivertingLegInformation2 *DivertingLegInformation2);
	int ParseDivertingLegInformation2_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					      struct FacDivertingLegInformation2 *DivertingLegInformation2);

	int encodeFacDivertingLegInformation3(__u8 * Dest, const struct FacDivertingLegInformation3 *DivertingLegInformation3);
	int ParseDivertingLegInformation3_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					      struct FacDivertingLegInformation3 *DivertingLegInformation3);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_DIVERSION_H__ */
/* ------------------------------------------------------------------- *//* end asn1_diversion.h */
