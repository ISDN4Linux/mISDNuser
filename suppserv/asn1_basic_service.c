/* $Id: asn1_basic_service.c,v 1.2 2006/08/16 14:15:52 nadi Exp $
 *
 */

#include "asn1.h"

// ======================================================================
// Basic Service Elements EN 300 196-1 D.6

int ParseBasicService(struct asn1_parm *pc, u_char *p, u_char *end, int *basicService)
{
	return ParseEnum(pc, p, end, basicService);
}

