/* mtimer.h
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

#ifndef MTIMER_H
#define MTIMER_H

#include "mlist.h"

struct mlayer3;

typedef	void (mtimer_func_t)(void *);

struct mtimer {
	struct list_head	list;
	struct _layer3		*l3;
	int			id;
	int			timeout;
	void			*data;
	mtimer_func_t		*function;
};

extern	int		init_timer(struct mtimer *, struct _layer3  *, void *, mtimer_func_t *);
extern	int		add_timer(struct mtimer *, int);
extern	int		del_timer(struct mtimer *);
extern	int		timer_pending(struct mtimer *);
extern	void		expire_timer(struct _layer3 *, int);

#endif
