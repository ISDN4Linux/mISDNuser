/* mtimer.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "mtimer.h"
#include "layer3.h"
#include "helper.h"

int
init_timer(struct mtimer *mt, struct timer_base  *tb, void *data, mtimer_func_t *f)
{
	mt->tb = tb;
	mt->id = 0;
	mt->data = data;
	mt->function = f;
	return 0;
}

int
add_timer(struct mtimer *mt, int tout)
{
	int	ret, para;

	mt->timeout = tout;
	para = tout;
	ret = ioctl(mt->tb->tdev, IMADDTIMER, (long)&para);
	if (ret < 0)
		return ret;
	mt->id = para;
	list_add_tail(&mt->list, &mt->tb->pending_timer);
	return ret;
}

int
del_timer(struct mtimer *mt)
{
	int		ret = 0;

	if (mt->id) {
		list_del(&mt->list);
		ret = ioctl(mt->tb->tdev, IMDELTIMER, (long)&mt->id);
		mt->id = 0;
	}
	return ret;
}

int
timer_pending(struct mtimer *mt)
{
	return mt->id;
}

void expire_timer(struct timer_base *tb, int id)
{
	struct mtimer	*mt;

	list_for_each_entry(mt, &tb->pending_timer, list) {
		if (mt->id == id) {
			list_del(&mt->list);
			mt->id = 0;
			mt->function(mt->data);
			return;
		}
	}
}
