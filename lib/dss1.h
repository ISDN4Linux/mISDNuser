/* dss1.h
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

#ifndef DSS1_H
#define DSS1_H

/*
 * Timer values
 */ 
#define T302	15000
#define T303	4000
#define T304	30000
#define T305	30000
#define T308	4000
/* for layer 1 certification T309 < layer1 T3 (e.g. 4000) */
/* This makes some tests easier and quicker */
#define T309	40000
/* T310 can be between 30-120 Seconds. We use 120 Seconds so the user can hear
   the inband messages */
#define T310	120000
#define T312	6000
#define T313	4000
#define T318	4000
#define T319	4000
#define N303	1
#define T_CTRL	180000

#define THOLD		4000
#define TRETRIEVE	4000

/* private TIMER events */
#define CC_T302		0x030201
#define CC_T303		0x030301
#define CC_T304		0x030401
#define CC_T305		0x030501
#define CC_T308_1	0x030801
#define CC_T308_2	0x030802
#define CC_T309		0x030901
#define CC_T310		0x031001
#define CC_T312		0x031201
#define CC_T313		0x031301
#define CC_T318		0x031801
#define CC_T319		0x031901
#define CC_TCTRL	0x031f01
#define CC_THOLD	0x03a001
#define CC_TRETRIEVE	0x03a101

#define AUX_IDLE		0
#define AUX_HOLD_REQ		1
#define AUX_CALL_HELD		2
#define AUX_RETRIEVE_REQ	3
#define AUX_HOLD_IND		4
#define AUX_RETRIEVE_IND	5

#define VALID_HOLD_STATES_PTMP	(SBIT(3) | SBIT(4) | SBIT(10))
#define VALID_HOLD_STATES_PTP	(SBIT(3) | SBIT(4) | SBIT(7) | SBIT(8) | SBIT(9) | SBIT(10))

#endif
