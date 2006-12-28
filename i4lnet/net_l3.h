/* $Id: net_l3.h,v 1.4 2006/12/28 12:24:01 jolly Exp $
 *
 * Layer 3 defines
 *
 * This file is (c) under GNU PUBLIC LICENSE
 *
 */

#ifndef NET_L3_H
#define NET_L3_H

#include "isdn_net.h"

typedef struct _layer3_proc	layer3_proc_t;
typedef struct _L3Timer L3Timer_t;

struct _L3Timer {
	layer3_proc_t		*pc;
	itimer_t		tl;
	int			nr;
};

struct _layer3_proc {
	layer3_proc_t	*prev;
	layer3_proc_t	*next;
	layer3_proc_t	*child;
	layer3_proc_t	*master;
	layer3_t	*l3;
	int		callref;
	int		ces;
	int		selces;
	int		state;
	u_long		Flags;
	L3Timer_t	timer1;
	L3Timer_t	timer2;
	int		bc;
	int             err;
	int		cause;
	int		hold_state;
	u_char		obuf[MAX_DFRAME_LEN];
	u_char		*op;
};

#define FLG_L3P_TIMER312	1
#define FLG_L3P_TIMER303_1	2
#define FLG_L3P_TIMER308_1	3
#define FLG_L3P_GOTRELCOMP	4

struct _layer3 {
	layer3_t	*prev;
	layer3_t	*next;
	msg_queue_t	squeue0;
	int		l2_state0;
	int		next_cr;
	int		debug;
	net_stack_t	*nst;
	layer3_proc_t	*proc;
};

static inline msg_t *l3_alloc_msg(int size)
{
	msg_t	*msg;

	msg = alloc_msg(size+MAX_HEADER_LEN);
	if (msg)
		msg_reserve(msg, MAX_HEADER_LEN);
	return(msg);
}

extern	int	Isdnl3Init(net_stack_t *);
extern	void	cleanup_Isdnl3(net_stack_t *);
extern	void	display_NR_IE(u_char *, char *, char *);

/* l3 pointer arrays */

typedef struct _ALERTING {
	u_char *BEARER;
	u_char *CHANNEL_ID;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *DISPLAY;
	u_char *SIGNAL;
	u_char *HLC;
	u_char *USER_USER;
	u_char *REDIR_DN;
} ALERTING_t;

typedef struct _CALL_PROCEEDING {
	u_char *BEARER;
	u_char *CHANNEL_ID;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *DISPLAY;
	u_char *REDIR_DN;
	u_char *HLC;
} CALL_PROCEEDING_t;

typedef struct _CONNECT {
	u_char *BEARER;
	u_char *CHANNEL_ID;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *DISPLAY;
	u_char *DATE;
	u_char *SIGNAL;
	u_char *CONNECT_PN;
	u_char *CONNECT_SUB;
	u_char *LLC;
	u_char *HLC;
	u_char *USER_USER;
	int ces;
} CONNECT_t;

typedef struct _CONNECT_ACKNOWLEDGE {
	u_char *CHANNEL_ID;
	u_char *DISPLAY;
	u_char *SIGNAL;
} CONNECT_ACKNOWLEDGE_t;

typedef struct _DISCONNECT {
	u_char *CAUSE;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *DISPLAY;
	u_char *SIGNAL;
	u_char *USER_USER;
} DISCONNECT_t;

typedef struct _INFORMATION {
	u_char *COMPLETE;
	u_char *DISPLAY;
	u_char *KEYPAD;
	u_char *SIGNAL;
	u_char *CALLED_PN;
} INFORMATION_t;

typedef struct _NOTIFY {
	u_char *BEARER;
	u_char *NOTIFY;
	u_char *DISPLAY;
	u_char *REDIR_DN;
} NOTIFY_t;

typedef struct _PROGRESS {
	u_char *BEARER;
	u_char *CAUSE;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *DISPLAY;
	u_char *HLC;
} PROGRESS_t;

typedef struct _RELEASE {
	u_char *CAUSE;
	u_char *FACILITY;
	u_char *DISPLAY;
	u_char *SIGNAL;
	u_char *USER_USER;
} RELEASE_t;

typedef struct _RELEASE_COMPLETE {
	u_char *CAUSE;
	u_char *FACILITY;
	u_char *DISPLAY;
	u_char *SIGNAL;
	u_char *USER_USER;
} RELEASE_COMPLETE_t;

typedef struct _RESUME {
	u_char *CALL_ID;
	u_char *FACILITY;
	int ces;
} RESUME_t;

typedef struct _RESUME_ACKNOWLEDGE {
	u_char *CHANNEL_ID;
	u_char *FACILITY;
	u_char *DISPLAY;
} RESUME_ACKNOWLEDGE_t;

typedef struct _RESUME_REJECT {
	u_char *CAUSE;
	u_char *DISPLAY;
} RESUME_REJECT_t;

typedef struct _SETUP {
	u_char *COMPLETE;
	u_char *BEARER;
	u_char *CHANNEL_ID;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *NET_FAC;
	u_char *DISPLAY;
	u_char *KEYPAD;
	u_char *SIGNAL;
	u_char *CALLING_PN;
	u_char *CALLING_SUB;
	u_char *CALLED_PN;
	u_char *CALLED_SUB;
	u_char *REDIR_NR;
	u_char *LLC;
	u_char *HLC;
	u_char *USER_USER;
	int ces;
} SETUP_t;

typedef struct _SETUP_ACKNOWLEDGE {
	u_char *CHANNEL_ID;
	u_char *FACILITY;
	u_char *PROGRESS;
	u_char *DISPLAY;
	u_char *SIGNAL;
} SETUP_ACKNOWLEDGE_t;

typedef struct _STATUS {
	u_char *CAUSE;
	u_char *CALL_STATE;
	u_char *DISPLAY;
} STATUS_t;

typedef struct _STATUS_ENQUIRY {
	u_char *DISPLAY;
} STATUS_ENQUIRY_t;

typedef struct _SUSPEND {
	u_char *CALL_ID;
	u_char *FACILITY;
} SUSPEND_t;

typedef struct _SUSPEND_ACKNOWLEDGE {
	u_char *FACILITY;
	u_char *DISPLAY;
} SUSPEND_ACKNOWLEDGE_t;

typedef struct _SUSPEND_REJECT {
	u_char *CAUSE;
	u_char *DISPLAY;
} SUSPEND_REJECT_t;

typedef struct _CONGESTION_CONTROL {
	u_char *CONGESTION;
	u_char *CAUSE;
	u_char *DISPLAY;
} CONGESTION_CONTROL_t;

typedef struct _USER_INFORMATION {
	u_char *MORE_DATA;
	u_char *USER_USER;
} USER_INFORMATION_t;

typedef struct _RESTART {
	u_char *CHANNEL_ID;
	u_char *DISPLAY;
	u_char *RESTART_IND;
} RESTART_t;

typedef struct _FACILITY {
	u_char *FACILITY;
	u_char *DISPLAY;
} FACILITY_t;

typedef struct _HOLD {
	u_char *DISPLAY;
} HOLD_t;

typedef struct _HOLD_ACKNOWLEDGE {
	u_char *CHANNEL_ID;
	u_char *DISPLAY;
} HOLD_ACKNOWLEDGE_t;

typedef struct _HOLD_REJECT {
	u_char *CAUSE;
	u_char *DISPLAY;
} HOLD_REJECT_t;

typedef struct _RETRIEVE {
	u_char *CHANNEL_ID;
} RETRIEVE_t;

typedef struct _RETRIEVE_ACKNOWLEDGE {
	u_char *CHANNEL_ID;
	u_char *DISPLAY;
} RETRIEVE_ACKNOWLEDGE_t;

typedef struct _RETRIEVE_REJECT {
	u_char *CAUSE;
	u_char *DISPLAY;
} RETRIEVE_REJECT_t;

#endif
