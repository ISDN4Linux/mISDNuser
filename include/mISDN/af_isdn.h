/*af_isdn.h
 *
 * Author       Karsten Keil <kkeil@novell.com>
 *
 * Copyright 2010  by Karsten Keil <kkeil@novell.com>
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
 *
 * simple implementation to allow different values for
 * AF_ISDN/PF_ISDN
 *
 * Important notes:
 *
 * This file is C code, not only declarations, so a program should only
 * include it in one code file, best in the file containing main() as well.
 * Programs using libmisdn do not need that file, since libmisdn already have
 * it.
 *
 */


int __af_isdn = MISDN_AF_ISDN;

int set_af_isdn(int af)
{
	if (af < 0)
		return -1;
	if (af >= AF_MAX)
		return -1;
	__af_isdn = af;
	return 0;
}
