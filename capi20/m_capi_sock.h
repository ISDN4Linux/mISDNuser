/*
 * m_capi_sock.h
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

#ifndef _M_CAPI_SOCK_H
#define _M_CAPI_SOCK_H

#include <capiutils.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MISDN_CAPI_SOCKET_PATH	"/var/run/mISDNcapid/sock"

#define MIC_INFO_CODING_ERROR	1

/* mISDN CAPI commands */
#define MIC_GET_PROFILE_REQ		CAPICMD(0xf0, 0xff)
#define MIC_REGISTER_REQ		CAPICMD(0xf1, 0xff)
#define MIC_RELEASE_REQ			CAPICMD(0xf2, 0xff)
#define MIC_SERIAL_NUMBER_REQ		CAPICMD(0xf3, 0xff)
#define MIC_VERSION_REQ			CAPICMD(0xf4, 0xff)
#define MIC_GET_MANUFACTURER_REQ	CAPICMD(0xf5, 0xff)
#define MIC_MANUFACTURER_REQ		CAPICMD(0xf6, 0xff)
#define MIC_USERFLAG_REQ		CAPICMD(0xf7, 0xff)
#define MIC_TTYNAME_REQ			CAPICMD(0xf8, 0xff)


#ifdef __cplusplus
}
#endif

#endif

