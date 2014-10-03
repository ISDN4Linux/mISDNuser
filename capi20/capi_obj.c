/*
 * capi_obj.c
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2012  by Karsten Keil <kkeil@linux-pingi.de>
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

#include "m_capi.h"
#include "mc_buffer.h"
#include "../lib/include/helper.h"

static pthread_rwlock_t	danglinglock;
static struct mCAPIobj *danglinglist;

static pthread_mutex_t uniqLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int uniqID = 1;

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
#define cobj_dbg(fmt, ...)	do {\
					if (file && (MIDEBUG_CAPIOBJ & mI_debug_mask))\
						mi_printf(file, lineno, __func__, MISDN_LIBDEBUG_DEBUG, fmt, ##__VA_ARGS__);\
				} while (0)
#define coref_dbg(fmt, ...)	do {\
					if (file && (MIDEBUG_CAPIOBJ & mI_debug_mask))\
						mi_printf(file, lineno, __func__, MISDN_LIBDEBUG_DEBUG, fmt, ##__VA_ARGS__);\
				} while (0)
#define cobj_err(fmt, ...)	mi_printf(file, lineno, __func__, MISDN_LIBDEBUG_ERROR, fmt, ##__VA_ARGS__)
#define cobj_warn(fmt, ...)	mi_printf(file, lineno, __func__, MISDN_LIBDEBUG_WARN, fmt, ##__VA_ARGS__)
#else
#define cobj_dbg(fmt, ...)	dprint(MIDEBUG_CAPIOBJ, fmt, ##__VA_ARGS__)
#define coref_dbg(fmt, ...)	do {} while (0)
#define cobj_err(fmt, ...)	eprint(fmt, ##__VA_ARGS__)
#define cobj_warn(fmt, ...)	wprint(fmt, ##__VA_ARGS__)
#endif

static int ShutdownNow = 0;

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
#define cobj_free(c)	__cobj_free(c, __FILE__, __LINE__)
static void __cobj_free(struct mCAPIobj *, const char *, int);
#else
static void cobj_free(struct mCAPIobj *);
#endif

#ifdef MISDN_CAPIOBJ_NO_FREE
static struct mCAPIobj *freelist;
static int freelistCnt = 0;
#endif

void free_capiobject(struct mCAPIobj *co, void *ptr)
{
	struct mCAPIobj *c;

	if (ShutdownNow) {
		free(ptr);
		return;
	}
	if (co->freed) {
		eprint("%s: uid=%i double free\n", CAPIobjIDstr(co), co->uid);
		return;
	}
	co->freed = 1;
#ifdef MISDN_CAPIOBJ_NO_FREE
	co->freep = ptr;
#endif
	if (co->unlisted) {
		pthread_rwlock_wrlock(&danglinglock);
		c = danglinglist;
		while(c) {
			if (c == co) {
				danglinglist = co->nextD;
				break;
			}
			if (c->nextD == co) {
				c->nextD = co->nextD;
				break;
			}
			c = c->nextD;
		}
		if (!c) {
			eprint("%s: not in dangling list corrupted ?\n", CAPIobjIDstr(co));
		}
#ifdef MISDN_CAPIOBJ_NO_FREE
		co->nextD = freelist;
		freelist = co;
		freelistCnt++;
#endif
		pthread_rwlock_unlock(&danglinglock);
	} else {
		iprint("%s: not unlisted\n", CAPIobjIDstr(co));
#ifdef MISDN_CAPIOBJ_NO_FREE
		pthread_rwlock_wrlock(&danglinglock);
		co->nextD = freelist;
		freelist = co;
		freelistCnt++;
		pthread_rwlock_unlock(&danglinglock);
#endif
	}
#ifndef MISDN_CAPIOBJ_NO_FREE
	free(ptr);
#endif
}

void CAPIobj_init(void)
{
	pthread_rwlock_init(&danglinglock, NULL);
	danglinglist = NULL;
#ifdef MISDN_CAPIOBJ_NO_FREE
	freelist = NULL;
#endif
}

void CAPIobj_exit(void)
{
	struct mCAPIobj *co, *cn;
	pthread_rwlock_wrlock(&danglinglock);
	ShutdownNow = 1;
	co = danglinglist;
	while (co) {
		cn = co->nextD;
		eprint("%s: uid=%i refcnt %d in dangling list - freeing now\n", CAPIobjIDstr(co), co->uid, co->refcnt);
		cobj_free(co);
		co = cn;
	}
#ifdef MISDN_CAPIOBJ_NO_FREE
	co = freelist;
	while (co) {
		cn = co->nextD;
		eprint("%s: uid=%d refcnt %d in free list - freeing now\n", CAPIobjIDstr(co), co->uid, co->refcnt);
		free(co->freep);
		co = cn;
	}
#endif
	pthread_rwlock_unlock(&danglinglock);
}

void dump_cobjects(void)
{
	struct mCAPIobj *co;

	if (pthread_rwlock_tryrdlock(&danglinglock)) {
		wprint("Cannot read lock dangling list for dumping\n");
		return;
	}
	co = danglinglist;
	iprint("Next unique ID=%i\n", uniqID);
	if (!co)
		iprint("No items in dangling list\n");
	while (co) {
		iprint("%s: uid=%i refcnt %d in dangling list\n", CAPIobjIDstr(co), co->uid, co->refcnt);
		co = co->nextD;
	}
	pthread_rwlock_unlock(&danglinglock);
#ifdef MISDN_CAPIOBJ_NO_FREE
	iprint("%d items in freelist\n", freelistCnt);
#endif
}

#ifdef MISDN_CAPIOBJ_NO_FREE
void dump_cobjects_free(void)
{
	struct mCAPIobj *co;

	if (pthread_rwlock_tryrdlock(&danglinglock)) {
		wprint("Cannot read lock dangling list for dumping\n");
		return;
	}
	co = freelist;
	if (!co)
		iprint("No items in free list\n");
	while (co) {
		iprint("%s: uid=%i refcnt %d in free list\n", CAPIobjIDstr(co), co->uid, co->refcnt);
		co = co->nextD;
	}
	pthread_rwlock_unlock(&danglinglock);
}
#endif

void cobj_unlisted(struct mCAPIobj *co)
{
	if (ShutdownNow)
		return;
	if (co->unlisted) {
		eprint("%s: refcnt %d double unlist\n", CAPIobjIDstr(co), co->refcnt);
		return;
	}
	co->unlisted = 1;
	pthread_rwlock_wrlock(&danglinglock);
	co->nextD = danglinglist;
	danglinglist = co;
	pthread_rwlock_unlock(&danglinglock);
}

const char *__eCAPIobjtype_s[] = {
	"None",
	"Root",
	"Application",
	"lController",
	"PLCI",
	"lPLCI",
	"NCCI",
	"Fax",
	"Undef",
	"Null object"
};


const char *CAPIobjt2str(struct mCAPIobj *co)
{
	unsigned int i;

	if (co) {
		i = co->type;
		if (i > Cot_Last)
			i = 1 + Cot_Last;
	} else
		i = 2 + Cot_Last;
	return __eCAPIobjtype_s[i];
}


static const char *__CAPIobjt2str(enum eCAPIobjtype cot)
{
	unsigned int i;

	i = cot;
	if (i > Cot_Last)
		i = 1 + Cot_Last;
	return __eCAPIobjtype_s[i];
}


const char *CAPIobjIDstr(struct mCAPIobj *co)
{
	int used;
	char stat[8], *p, *s;

	if (co) {
		s = co->idstr;
		if (co->type > Cot_Last) {
			used = snprintf(s, CAPIobj_IDSIZE, "%s%d id:%x id2:%x", CAPIobjt2str(co), co->type, co->id, co->id2);
		} else {
			switch (co->type) {
			case Cot_None:
				used = snprintf(s, CAPIobj_IDSIZE, "NoneObj id:%x-%x", co->id, co->id2);
				break;
			case Cot_Root:
				used = snprintf(s, CAPIobj_IDSIZE, "RootObj id:%x-%x", co->id, co->id2);
				break;
			case Cot_Application:
				used = snprintf(s, CAPIobj_IDSIZE, "Appl-id:%d", co->id2);
				break;
			case Cot_lController:
				used = snprintf(s, CAPIobj_IDSIZE, "LContr-%02d Appl-%03d", co->id, co->id2);
				break;
			case Cot_PLCI:
				used = snprintf(s, CAPIobj_IDSIZE, "  PLCI%04x PID:%08x", co->id, co->id2);
				break;
			case Cot_lPLCI:
				used = snprintf(s, CAPIobj_IDSIZE, " LPLCI%04x Appl-%03d", co->id, co->id2);
				break;
			case Cot_NCCI:
				used = snprintf(s, CAPIobj_IDSIZE, "NCCI%06x Appl-%03d", co->id, co->id2);
				break;
			case Cot_FAX:
				used = snprintf(s, CAPIobj_IDSIZE, " FAX%06x  B%d", co->id, co->id2);
				break;
			}
#ifdef MISDN_CAPI_REFCOUNT_DEBUG
			used += snprintf(&s[used], CAPIobj_IDSIZE - used, " *%d", co->refcnt);
#endif
			p = stat;
			if (co->cleaned)
				*p++ = 'C';
			if (co->unlisted)
				*p++ = co->parent ? 'u' : 'U';
			if (co->freeing)
				*p++ = (co->freed) ? 'F' : 'f';
			*p = 0;
			if (p != stat) {
				used += snprintf(&s[used], CAPIobj_IDSIZE - used, " %s", stat);
				if (used >= CAPIobj_IDSIZE)
					wprint("Overflow %d >= %d\n", used, CAPIobj_IDSIZE);
			}
		}
	} else {
		s = (char *)CAPIobjt2str(NULL);
	}
	return s;
}

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
#undef 	cobj_free
#define cobj_free(c)	__cobj_free(c, file, lineno)
static void __cobj_free(struct mCAPIobj *co, const char *file, int lineno)
{
	if (!file)
		file = "(none)";
#else
static void cobj_free(struct mCAPIobj *co)
{
#endif
	if (co->freeing) {
		cobj_err("%s: uid=%i refcnt %d called again -- double free attempt\n", CAPIobjIDstr(co), co->uid, co->refcnt);
		return;
	}
	co->freeing = 1;
	cobj_dbg("%s: uid=%i freeing now\n", CAPIobjIDstr(co), co->uid);

	/* sanity check */
	if (co->itemcnt || co->listhead) {
		cobj_err("%s: Still %d items in %slist\n", CAPIobjIDstr(co), co->itemcnt, co->listhead ? "" : "NULL ");
	}
	switch(co->type) {
		case Cot_None:
		case Cot_Root:
			/* we never free these */
			wprint("%s: try to free\n", CAPIobjIDstr(co));
			break;
		case Cot_Application:
			Free_Application(co);
			break;
		case Cot_lController:
			Free_lController(co);
			break;
		case Cot_PLCI:
			Free_PLCI(co);
			break;
		case Cot_lPLCI:
			Free_lPLCI(co);
			break;
		case Cot_NCCI:
			Free_NCCI(co);
			break;
		case Cot_FAX:
			Free_Faxobject(co);
			break;
	}
};

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
struct mCAPIobj *__get_cobj(struct mCAPIobj *co, const char *file, int lineno)
#else
struct mCAPIobj *get_cobj(struct mCAPIobj *co)
#endif
{
	if (co) {
		if (co->parent) {
			pthread_rwlock_wrlock(&co->parent->lock);
		} else {
			if (co->type != Cot_Root) { /* has no parent */
				cobj_warn("%s: parent not assigned\n", CAPIobjIDstr(co));
				return NULL;
			}
		}
		if (co->freeing)
			cobj_err("Currently freeing %s refcnt: %d\n", CAPIobjIDstr(co), co->refcnt);
		co->refcnt++;
		coref_dbg("%s\n", CAPIobjIDstr(co));
		if (co->parent)
			pthread_rwlock_unlock(&co->parent->lock);
	} else
		coref_dbg("No CAPIobj\n");
	return co;
}

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
int __put_cobj(struct mCAPIobj *co, const char *file, int lineno)
#else
int put_cobj(struct mCAPIobj *co)
#endif
{
	struct mCAPIobj *p;
	int ret = -ENODEV;

	if (co) {
		if (co->freeing)
			cobj_err("Currently freeing %s refcnt: %d\n", CAPIobjIDstr(co), co->refcnt);
		p = co->parent;
		if (p) {
			pthread_rwlock_wrlock(&p->lock);
			coref_dbg("%s\n", CAPIobjIDstr(co));
			co->refcnt--;
			if (co->refcnt < 0) {
				cobj_err("%s: refcnt reached %d - list items:%d\n", CAPIobjIDstr(co), co->refcnt, co->itemcnt);
			}
			ret = co->refcnt;
			if (co->cleaned && co->refcnt <= 0) { /* last ref */
				pthread_rwlock_unlock(&p->lock);
				cobj_free(co);
			} else {
				pthread_rwlock_unlock(&p->lock);
				if (co->cleaned) {
					switch (co->type) {
					case Cot_Application: /* Application has a special put handler */
						Put_Application_cleaned(co);
						break;
					default:
						break;
					}
				}
			}
		} else {
			if (co->type == Cot_Root) { /* has no parent */
				coref_dbg("%s\n", CAPIobjIDstr(co));
				co->refcnt--;
				ret = co->refcnt;
			} else
				cobj_warn("%s: parent not assigned\n", CAPIobjIDstr(co));
		}
	} else
		cobj_dbg("No CAPIobj\n");
	return ret;
}


#ifdef MISDN_CAPI_REFCOUNT_DEBUG
struct mCAPIobj *__get_next_cobj(struct mCAPIobj *parent, struct mCAPIobj *cur, const char *file, int lineno)
#else
struct mCAPIobj *get_next_cobj(struct mCAPIobj *parent, struct mCAPIobj *cur)
#endif
{
	struct mCAPIobj *next;

	if (parent) {
		pthread_rwlock_wrlock(&parent->lock);
		if (cur)
			next = cur->next;
		else
			next = parent->listhead;
		if (next) {
			next->refcnt++;
			coref_dbg("%s: next %s\n", CAPIobjIDstr(cur), CAPIobjIDstr(next));
		}
		pthread_rwlock_unlock(&parent->lock);
		if (parent->freeing)
			cobj_err("Currently freeing %s refcnt: %d Next: %s\n", CAPIobjIDstr(parent), parent->refcnt, CAPIobjIDstr(next));
	} else
		next = NULL;
	if (cur) {
#ifdef MISDN_CAPI_REFCOUNT_DEBUG
		if (next) {
			__put_cobj(cur, NULL, lineno);
		} else {
			__put_cobj(cur, file, lineno);
		}

#else
		put_cobj(cur);
#endif
	}
	return next;
}

static unsigned int get_uniqID(void)
{
	unsigned int uid;

	pthread_mutex_lock(&uniqLock);
	uid = uniqID++;
	pthread_mutex_unlock(&uniqLock);
	return uid;
}

int init_cobj(struct mCAPIobj *co, struct mCAPIobj *parent, enum eCAPIobjtype cot, unsigned int id, unsigned int id2)
{
	int ret;

	ret = pthread_rwlock_init(&co->lock, NULL);
	co->type = cot;
	co->uid = get_uniqID();
	if (parent)
		co->parent = get_cobj(parent);
	else
		co->parent = NULL;
	co->id = id;
	co->id2 = id2;
	co->refcnt = 1;
	dprint(MIDEBUG_CAPIOBJ, "%s: uid=%i initialized\n", CAPIobjIDstr(co), co->uid);
	return ret;
}

int init_cobj_registered(struct mCAPIobj *co, struct mCAPIobj *parent, enum eCAPIobjtype cot, unsigned int idmask)
{
	unsigned int id, m = 0, lastid;
	int ret = 0;

	pthread_rwlock_wrlock(&parent->lock);
	co->type = cot;
	co->uid = get_uniqID();
	id = 0;
	if (parent->listhead)
		lastid = parent->listhead->id;
	else
		lastid = 0;
	if (cot == Cot_PLCI) {
		id = NextFreePLCI(parent);
		if (!id)
			ret = -EBUSY;
	} else {
		if (idmask) {
			for (m = 0xff; m < 0xff000000; m <<= 8) {
				if ((m & idmask) == m) {
					id &= idmask;
				} else {
					id += (lastid & m) + (idmask & m);
					if ((id & m) == m) {
						ret = -EBUSY; /* overflow */
						break;
					}
					idmask &= ~m;
				}
			}
		}
	}
	if (ret) {
		pthread_rwlock_unlock(&parent->lock);
		wprint("%s: uid=%i new id overflow lastid %x id %x test mask %x\n", CAPIobjt2str(co), co->uid, lastid, id, m);
		return ret;
	}
	co->id = id | (idmask & parent->id);
	ret = pthread_rwlock_init(&co->lock, NULL);
	if (ret == 0) {
		co->next = parent->listhead;
		parent->listhead = co;
		parent->itemcnt++;
		co->refcnt = 2;
	}
	pthread_rwlock_unlock(&parent->lock);
	if (ret == 0)
		co->parent = get_cobj(parent);
	dprint(MIDEBUG_CAPIOBJ, "%s: uid=%i initialized\n", CAPIobjIDstr(co), co->uid);
	return ret;
}

#ifdef MISDN_CAPI_REFCOUNT_DEBUG
int __delist_cobj(struct mCAPIobj *co, const char *file, int lineno)
#else
int delist_cobj(struct mCAPIobj *co)
#endif
{
	int r, ret = -EINVAL;
	int old __attribute__((unused));
	enum eCAPIobjtype pt __attribute__((unused));
	struct mCAPIobj *p, *c;
	unsigned int uid;

	if (co) {
		p = co->parent;
		if (p) {
			pthread_rwlock_wrlock(&p->lock);
			c = p->listhead;
			old = p->itemcnt;
			while (c) {
				if (c == co) {
					p->listhead = co->next;
					break;
				}
				if (c->next == co) {
					c->next = co->next;
					break;
				}
				c = c->next;
			}
			if (c) {
				p->itemcnt--;
				cobj_unlisted(co);
			}
			ret = p->itemcnt;
			r = co->refcnt;
			uid = co->uid;
			pt = p->type;
			pthread_rwlock_unlock(&p->lock);
			if (c)
				r = put_cobj(co);
			if (r > 0)
				coref_dbg("%s: parent %s items %d->%d\n", CAPIobjIDstr(co), __CAPIobjt2str(pt), old, ret);
			else
				coref_dbg("Object uid=%i delisted and freed parent %s items %d->%d\n", uid, __CAPIobjt2str(pt), old, ret);
		} else
			cobj_warn("%s: no  parent assigned\n", CAPIobjt2str(co));
	}
	return ret;
}
