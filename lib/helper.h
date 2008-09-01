/* helper.h
 *
 * some helper functions 
 *
 * Author       Karsten Keil <kkeil@novell.com>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
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

#ifndef _HELPER_H
#define _HELPER_H

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
                      

/* Simple replacement for the NON-ATOMIC routines which asm/bitops.h
   was providing. */

static inline int test_bit(int bit, unsigned long *word)
{
	return !!((*word) & (1<<bit));
}

static inline int test_and_clear_bit(int bit, unsigned long *word)
{
	int ret = !!((*word) & (1<<bit));

	*word &= ~(1<<bit);
	return ret;
}

static inline int test_and_set_bit(int bit, unsigned long *word)
{
	int ret = !!((*word) & (1<<bit));

	*word |= 1<<bit;
	return ret;
}

static inline void clear_bit(int bit, unsigned long *word)
{
	*word &= ~(1<<bit);
}

static inline void set_bit(int bit, unsigned long *word)
{
	*word |= 1<<bit;
}

#endif
