/* $Id: l3dss1.h,v 2.0 2007/06/29 14:08:15 kkeil Exp $
 *
 *  DSS1 (Euro) D-channel protocol defines
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
#ifndef _L3DSS1_H
#define _L3DSS1_H

#define T301	180000
#define T302	15000
#define T303	4000
#define T304	30000
#define T305	30000
#define T308	4000
/* for layer 1 certification T309 < layer1 T3 (e.g. 4000) */
/* This makes some tests easier and quicker */
#define T309	40000
#define T310	30000
#define T312	6000
#define T313	4000
#define T318	4000
#define T319	4000
#define N303	1
#define T_CTRL	180000

/* private TIMER events */
#define CC_TIMER	0x000001
#define CC_T301		0x030101
#define CC_T302		0x030201
#define CC_T303		0x030301
#define CC_T304		0x030401
#define CC_T305		0x030501
#define CC_T308		0x030801
#define CC_T309         0x030901
#define CC_T310		0x031001
#define CC_T312		0x031201
#define CC_T313		0x031301
#define CC_T318		0x031801
#define CC_T319		0x031901
#define CC_TCTRL	0x031f01
#define HOLDAUX_IDLE		0
#define HOLDAUX_HOLD_REQ	1
#define HOLDAUX_HOLD		2
#define HOLDAUX_RETR_REQ	3
#define HOLDAUX_HOLD_IND	4
#define HOLDAUX_RETR_IND	5


/* l3dss1 specific data in l3 process */
typedef struct
  { unsigned char invoke_id; /* used invoke id in remote ops, 0 = not active */
    ulong ll_id; /* remebered ll id */
    u_char remote_operation; /* handled remote operation, 0 = not active */ 
    int proc; /* rememered procedure */  
    ulong remote_result; /* result of remote operation for statcallb */
    char uus1_data[35]; /* data send during alerting or disconnect */
  } dss1_proc_priv;

/* l3dss1 specific data in protocol stack */
typedef struct
  { unsigned char last_invoke_id; /* last used value for invoking */
    unsigned char invoke_used[32]; /* 256 bits for 256 values */
  } dss1_stk_priv;        

#endif
