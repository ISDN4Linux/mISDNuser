/* $Id: helper.h,v 1.0 2003/08/27 07:35:32 kkeil Exp $
 *
 *   Basic declarations, defines and prototypes
 *
 * This file is (c) under GNU PUBLIC LICENSE
 *
 */
#ifndef _mISDN_HELPER_H
#define	_mISDN_HELPER_H
#ifdef MEMDBG
#include "memdbg.h"
#endif

#define int_error() \
        fprintf(stderr, "mISDN: INTERNAL ERROR in %s:%d\n", \
                       __FILE__, __LINE__)
                       
#define int_errtxt(fmt, arg...) \
        fprintf(stderr, "mISDN: INTERNAL ERROR in %s:%d " fmt "\n", \
                       __FILE__, __LINE__, ## arg)
                       
#define APPEND_TO_LIST(item,base) \
	if (item->prev || item->next) \
		int_errtxt("APPEND not clean %p<-%p->%p", \
			item->prev, item, item->next); \
	item->next = NULL; \
	item->prev = base; \
	while (item->prev && item->prev->next) \
		item->prev = item->prev->next; \
	if (item->prev == item) { \
		int_errtxt("APPEND DUP %p", item); \
	} else \
		if (base) { \
			item->prev->next = item; \
		} else \
			base = item

#define INSERT_INTO_LIST(newi,nexti,base) \
	newi->next = nexti; \
	newi->prev = nexti->prev; \
	if (newi->prev) \
		newi->prev->next = newi; \
	nexti->prev = newi; \
	if (base == nexti) \
		base = newi

#define REMOVE_FROM_LIST(item) \
	if (item->prev) \
		item->prev->next = item->next; \
	if (item->next) \
		item->next->prev = item->prev

#define REMOVE_FROM_LISTBASE(item,base) \
	REMOVE_FROM_LIST(item); \
	if (item == base) \
		base = item->next

#endif
