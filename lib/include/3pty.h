/*
 * $Id$
 *
 * Three Party (3PTY) Supplementary Services ETS 300 188-2
 *
 * 3PTY Facility ie encode/decode header
 */

#ifndef __ASN1_3PTY_H
#define __ASN1_3PTY_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- */

	int encodeBegin3PTY(__u8 * Dest, const struct asn1_parm *pc, const void *val);
	int encodeEnd3PTY(__u8 * Dest, const struct asn1_parm *pc, const void *val);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/* __ASN1_3PTY_H */
/* ------------------------------------------------------------------- *//* end asn1_3pty.h */
