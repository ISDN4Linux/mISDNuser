/*
 * ncci.h
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

#ifndef _NCCI_H
#define _NCCI_H

// --------------------------------------------------------------------
// NCCI state machine
//
// Some rules:
//   *  EV_AP_*  events come from CAPI Application
//   *  EV_DL_*  events come from the ISDN stack
//   *  EV_NC_*  events generated in NCCI handling
//   *  messages are send in the routine that handle the event
//
// --------------------------------------------------------------------

enum st_ncci_e {
	ST_NCCI_N_0,
	ST_NCCI_N_0_1,
	ST_NCCI_N_1,
	ST_NCCI_N_2,
	ST_NCCI_N_ACT,
	ST_NCCI_N_3,
	ST_NCCI_N_4,
	ST_NCCI_N_5,
};

enum ev_ncci_e {
	EV_AP_CONNECT_B3_REQ,
	EV_NC_CONNECT_B3_CONF,
	EV_NC_CONNECT_B3_IND,
	EV_AP_CONNECT_B3_RESP,
	EV_NC_CONNECT_B3_ACTIVE_IND,
	EV_AP_CONNECT_B3_ACTIVE_RESP,
	EV_AP_RESET_B3_REQ,
	EV_NC_RESET_B3_IND,
	EV_NC_RESET_B3_CONF,
	EV_AP_RESET_B3_RESP,
	EV_NC_CONNECT_B3_T90_ACTIVE_IND,
	EV_AP_DISCONNECT_B3_REQ,
	EV_NC_DISCONNECT_B3_IND,
	EV_NC_DISCONNECT_B3_CONF,
	EV_AP_DISCONNECT_B3_RESP,
	EV_L3_DISCONNECT_IND,
	EV_AP_FACILITY_REQ,
	EV_AP_MANUFACTURER_REQ,
	EV_DL_ESTABLISH_IND,
	EV_DL_ESTABLISH_CONF,
	EV_DL_RELEASE_IND,
	EV_DL_RELEASE_CONF,
	EV_DL_DOWN_IND,
	EV_NC_LINKDOWN,
	EV_AP_RELEASE,
};

struct mNCCI;

const char *_mi_ncci_st2str(struct mNCCI *);
const char *_mi_ncci_ev2str(enum ev_ncci_e);

void ncciCmsgHeader(struct mNCCI *, struct mc_buf *, uint8_t, uint8_t);
int ncciB3Message(struct mNCCI *, struct mc_buf *);
void AnswerDataB3Req(struct mNCCI *, struct mc_buf *, uint16_t);

#endif
