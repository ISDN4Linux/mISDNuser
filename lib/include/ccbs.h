/*
 * $Id$
 *
 * CCBS Supplementary Services ETS 300 359-1
 * CCNR Supplementary Services ETS 301 065-1
 *
 * Generic functional protocol for the support of supplementary services ETS 300 196-1
 *
 * CCBS/CCNR Facility ie encode/decode header
 */

#ifndef __ASN1_CCBS_H
#define __ASN1_CCBS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

	int encodeFacStatusRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacStatusRequest *StatusRequest);
	int ParseStatusRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacStatusRequest *StatusRequest);
	int ParseStatusRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacStatusRequest_RES *StatusRequest);

	int encodeFacCallInfoRetain(__u8 * Dest, const struct asn1_parm *pc, const struct FacCallInfoRetain *CallInfoRetain);
	int ParseCallInfoRetain(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallInfoRetain *CallInfoRetain);

	int encodeFacEraseCallLinkageID(__u8 * Dest, const struct asn1_parm *pc, const struct FacEraseCallLinkageID *EraseCallLinkageID);
	int ParseEraseCallLinkageID(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacEraseCallLinkageID *EraseCallLinkageID);

	int encodeFacCCBSDeactivate(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSDeactivate *CCBSDeactivate);
	int ParseCCBSDeactivate(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSDeactivate *CCBSDeactivate);

	int encodeFacCCBSErase(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSErase *CCBSErase);
	int ParseCCBSErase(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSErase *CCBSErase);

	int encodeFacCCBSRemoteUserFree(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRemoteUserFree *CCBSRemoteUserFree);
	int ParseCCBSRemoteUserFree(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacCCBSRemoteUserFree *CCBSRemoteUserFree);

	int encodeFacCCBSCall(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSCall *CCBSCall);
	int ParseCCBSCall(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSCall *CCBSCall);

	int encodeFacCCBSBFree(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSBFree *CCBSBFree);
	int ParseCCBSBFree(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSBFree *CCBSBFree);

	int encodeFacCCBSStopAlerting(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSStopAlerting *CCBSStopAlerting);
	int ParseCCBSStopAlerting(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSStopAlerting *CCBSStopAlerting);

	int encodeFacCCBSStatusRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSStatusRequest *CCBSStatusRequest);
	int ParseCCBSStatusRequest(struct asn1_parm *pc, u_char * p, u_char * end,
				       struct FacCCBSStatusRequest *CCBSStatusRequest);
	int ParseCCBSStatusRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				       struct FacCCBSStatusRequest_RES *CCBSStatusRequest);

	int encodeFacCCBSRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRequest *CCBSRequest);
	int encodeFacCCNRRequest(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSRequest *CCNRRequest);
	int ParseCCBSRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest *CCBSRequest);
	int ParseCCNRRequest(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest *CCNRRequest);
	int ParseCCBSRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_RES *CCBSRequest);
	int ParseCCNRRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_RES *CCNRRequest);

	int encodeFacCCBSInterrogate(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSInterrogate *CCBSInterrogate);
	int encodeFacCCNRInterrogate(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBSInterrogate *CCNRInterrogate);
	int ParseCCBSInterrogate(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate *CCBSInterrogate);
	int ParseCCNRInterrogate(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate *CCNRInterrogate);
	int ParseCCBSInterrogate_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate_RES *CCBSInterrogate);
	int ParseCCNRInterrogate_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate_RES *CCNRInterrogate);

	int encodeFacCCBS_T_Call(__u8 * Dest, const struct asn1_parm *pc, const void *val);
	int encodeFacCCBS_T_Suspend(__u8 * Dest, const struct asn1_parm *pc, const void *val);
	int encodeFacCCBS_T_Resume(__u8 * Dest, const struct asn1_parm *pc, const void *val);
	int encodeFacCCBS_T_RemoteUserFree(__u8 * Dest, const struct asn1_parm *pc, const void *val);
	int encodeFacCCBS_T_Available(__u8 * Dest, const struct asn1_parm *pc, const void *val);

	int encodeFacCCBS_T_Request(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBS_T_Request *CCBS_T_Request);
	int encodeFacCCNR_T_Request(__u8 * Dest, const struct asn1_parm *pc, const struct FacCCBS_T_Request *CCNR_T_Request);
	int ParseCCBS_T_Request(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request *CCBS_T_Request);
	int ParseCCNR_T_Request(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request *CCNR_T_Request);
	int ParseCCBS_T_Request_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_RES *CCBS_T_Request);
	int ParseCCNR_T_Request_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_RES *CCNR_T_Request);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_CCBS_H */
/* ------------------------------------------------------------------- *//* end asn1_ccbs.h */
