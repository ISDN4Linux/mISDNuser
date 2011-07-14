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

	int encodeFacStatusRequest(__u8 * Dest, const struct FacStatusRequest *StatusRequest);
	int ParseStatusRequest_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacStatusRequest_ARG *StatusRequest);
	int ParseStatusRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacStatusRequest_RES *StatusRequest);

	int encodeFacCallInfoRetain(__u8 * Dest, const struct FacCallInfoRetain *CallInfoRetain);
	int ParseCallInfoRetain_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCallInfoRetain *CallInfoRetain);

	int encodeFacEraseCallLinkageID(__u8 * Dest, const struct FacEraseCallLinkageID *EraseCallLinkageID);
	int ParseEraseCallLinkageID_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacEraseCallLinkageID *EraseCallLinkageID);

	int encodeFacCCBSDeactivate(__u8 * Dest, const struct FacCCBSDeactivate *CCBSDeactivate);
	int ParseCCBSDeactivate_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSDeactivate_ARG *CCBSDeactivate);

	int encodeFacCCBSErase(__u8 * Dest, const struct FacCCBSErase *CCBSErase);
	int ParseCCBSErase_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSErase *CCBSErase);

	int encodeFacCCBSRemoteUserFree(__u8 * Dest, const struct FacCCBSRemoteUserFree *CCBSRemoteUserFree);
	int ParseCCBSRemoteUserFree_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
					struct FacCCBSRemoteUserFree *CCBSRemoteUserFree);

	int encodeFacCCBSCall(__u8 * Dest, const struct FacCCBSCall *CCBSCall);
	int ParseCCBSCall_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSCall *CCBSCall);

	int encodeFacCCBSBFree(__u8 * Dest, const struct FacCCBSBFree *CCBSBFree);
	int ParseCCBSBFree_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSBFree *CCBSBFree);

	int encodeFacCCBSStopAlerting(__u8 * Dest, const struct FacCCBSStopAlerting *CCBSStopAlerting);
	int ParseCCBSStopAlerting_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSStopAlerting *CCBSStopAlerting);

	int encodeFacCCBSStatusRequest(__u8 * Dest, const struct FacCCBSStatusRequest *CCBSStatusRequest);
	int ParseCCBSStatusRequest_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
				       struct FacCCBSStatusRequest_ARG *CCBSStatusRequest);
	int ParseCCBSStatusRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				       struct FacCCBSStatusRequest_RES *CCBSStatusRequest);

	int encodeFacCCBSRequest(__u8 * Dest, const struct FacCCBSRequest *CCBSRequest);
	int encodeFacCCNRRequest(__u8 * Dest, const struct FacCCBSRequest *CCNRRequest);
	int ParseCCBSRequest_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_ARG *CCBSRequest);
	int ParseCCNRRequest_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_ARG *CCNRRequest);
	int ParseCCBSRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_RES *CCBSRequest);
	int ParseCCNRRequest_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBSRequest_RES *CCNRRequest);

	int encodeFacCCBSInterrogate(__u8 * Dest, const struct FacCCBSInterrogate *CCBSInterrogate);
	int encodeFacCCNRInterrogate(__u8 * Dest, const struct FacCCBSInterrogate *CCNRInterrogate);
	int ParseCCBSInterrogate_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate_ARG *CCBSInterrogate);
	int ParseCCNRInterrogate_ARG(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate_ARG *CCNRInterrogate);
	int ParseCCBSInterrogate_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate_RES *CCBSInterrogate);
	int ParseCCNRInterrogate_RES(struct asn1_parm *pc, u_char * p, u_char * end,
				     struct FacCCBSInterrogate_RES *CCNRInterrogate);

	int encodeFacCCBS_T_Call(__u8 * Dest, const struct FacCCBS_T_Event *CCBS_T_Call);
	int encodeFacCCBS_T_Suspend(__u8 * Dest, const struct FacCCBS_T_Event *CCBS_T_Suspend);
	int encodeFacCCBS_T_Resume(__u8 * Dest, const struct FacCCBS_T_Event *CCBS_T_Resume);
	int encodeFacCCBS_T_RemoteUserFree(__u8 * Dest, const struct FacCCBS_T_Event *CCBS_T_RemoteUserFree);
	int encodeFacCCBS_T_Available(__u8 * Dest, const struct FacCCBS_T_Event *CCBS_T_Available);

	int encodeFacCCBS_T_Request(__u8 * Dest, const struct FacCCBS_T_Request *CCBS_T_Request);
	int encodeFacCCNR_T_Request(__u8 * Dest, const struct FacCCBS_T_Request *CCNR_T_Request);
	int ParseCCBS_T_Request_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_ARG *CCBS_T_Request);
	int ParseCCNR_T_Request_ARG(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_ARG *CCNR_T_Request);
	int ParseCCBS_T_Request_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_RES *CCBS_T_Request);
	int ParseCCNR_T_Request_RES(struct asn1_parm *pc, u_char * p, u_char * end, struct FacCCBS_T_Request_RES *CCNR_T_Request);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_CCBS_H */
/* ------------------------------------------------------------------- *//* end asn1_ccbs.h */
