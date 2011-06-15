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

	int encodeFacEctExecute(__u8 * Dest, const struct FacEctExecute *EctExecute);

	int encodeFacExplicitEctExecute(__u8 * Dest, const struct FacExplicitEctExecute *ExplicitEctExecute);
	int ParseExplicitEctExecute_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacExplicitEctExecute *ExplicitEctExecute);

	int encodeFacRequestSubaddress(__u8 * Dest, const struct FacRequestSubaddress *RequestSubaddress);

	int encodeFacSubaddressTransfer(__u8 * Dest, const struct FacSubaddressTransfer *SubaddressTransfer);
	int ParseSubaddressTransfer_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacSubaddressTransfer *SubaddressTransfer);

	int encodeFacEctLinkIdRequest(__u8 * Dest, const struct FacEctLinkIdRequest *EctLinkIdRequest);
	int ParseEctLinkIdRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				      struct FacEctLinkIdRequest_RES *EctLinkIdRequest);

	int encodeFacEctInform(__u8 * Dest, const struct FacEctInform *EctInform);
	int ParseEctInform_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctInform *EctInform);

	int encodeFacEctLoopTest(__u8 * Dest, const struct FacEctLoopTest *EctLoopTest);
	int ParseEctLoopTest_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLoopTest_ARG *EctLoopTest);
	int ParseEctLoopTest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacEctLoopTest_RES *EctLoopTest);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_ECT_H */
/* ------------------------------------------------------------------- *//* end asn1_ect.h */
