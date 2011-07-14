/*
 * $Id$
 *
 * Explicit Call Transfer (ECT) Supplementary Services ETS 300 369-1
 *
 * ECT Facility ie encode/decode header
 */

#ifndef __ASN1_ECT_H
#define __ASN1_ECT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

	int encodeFacEctExecute(__u8 * Dest, const struct asn1_parm *pc, const void *val);

	int encodeFacExplicitEctExecute(__u8 * Dest, const struct asn1_parm *pc, const struct FacExplicitEctExecute *ExplicitEctExecute);
	int ParseExplicitEctExecute(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacExplicitEctExecute *ExplicitEctExecute);

	int encodeFacRequestSubaddress(__u8 * Dest, const struct asn1_parm *pc, const void *val);

	int encodeFacSubaddressTransfer(__u8 * Dest, const struct asn1_parm *pc, const struct FacSubaddressTransfer *SubaddressTransfer);
	int ParseSubaddressTransfer(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacSubaddressTransfer *SubaddressTransfer);

	int encodeFacEctLinkIdRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacEctLinkIdRequest_RES *EctLinkIdRequest);
	int ParseEctLinkIdRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacEctLinkIdRequest_RES *EctLinkIdRequest);

	int encodeFacEctInform(__u8 * Dest, const struct asn1_parm *pc, const struct FacEctInform *EctInform);
	int ParseEctInform(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctInform *EctInform);

	int encodeFacEctLoopTest(__u8 * Dest, const struct asn1_parm *pc, const struct FacEctLoopTest *EctLoopTest);
	int ParseEctLoopTest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLoopTest *EctLoopTest);
	int ParseEctLoopTest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLoopTest_RES *EctLoopTest);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_ECT_H */
/* ------------------------------------------------------------------- *//* end asn1_ect.h */
