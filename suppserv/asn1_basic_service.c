/* $Id: asn1_basic_service.c,v 1.1 2006/08/15 16:29:13 nadi Exp $
 *
 */

#include "suppserv.h"

// ======================================================================
// Basic Service Elements EN 300 196-1 D.6

int ParseBasicService(struct asn1_parm *pc, u_char *p, u_char *end, int *basicService)
{
	return ParseEnum(pc, p, end, basicService);
}

