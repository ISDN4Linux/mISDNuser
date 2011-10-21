/*
 * mc_buffer.c
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2011  by Karsten Keil <kkeil@linux-pingi.de>
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

#include <stdlib.h>
#include <string.h>
#include "mc_buffer.h"

/*
 * free the message
 */

#ifdef MEMLEAK_DEBUG
void __free_mc_buf(struct mc_buf *mc, const char *file, int lineno, const char *func)
{
	if (mc->l3m)
		__free_l3_msg(mc->l3m, file, lineno, func);
	__mi_free(mc, file, lineno, func);
}

#else
void free_mc_buf(struct mc_buf *mc)
{
	if (mc->l3m)
		free_l3_msg(mc->l3m);
	free(mc);
}
#endif


void mc_clear_cmsg(struct mc_buf *mc)
{
	memset(&mc->cmsg, 0, sizeof(mc->cmsg));
}
