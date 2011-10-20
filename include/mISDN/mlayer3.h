/* mlayer3.h
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
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
              
#ifndef _MLAYER3_H
#define _MLAYER3_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdarg.h>
#include <pthread.h>
#include <mISDN/mISDNcompat.h>

#undef MEMLEAK_DEBUG
#if MEMLEAKDEBUG_COMPILE
#define MEMLEAK_DEBUG 1
#endif

struct l3_head {
	unsigned char	type;
	unsigned char	crlen;
	unsigned short	cr;
};

struct m_extie {
	unsigned char	ie;
	unsigned char	codeset;
	unsigned char	len;
	unsigned char	*val;
};

struct l3_msg {
	unsigned int	type;
	unsigned int	pid;
	unsigned char	*bearer_capability;	/* ie 0x04 pos  0 */
	unsigned char	*cause;			/* ie 0x08 pos  1 */
	unsigned char	*call_id;		/* ie 0x10 pos  2 */
	unsigned char	*call_state;		/* ie 0x14 pos  3 */
	unsigned char	*channel_id;		/* ie 0x18 pos  4 */
	unsigned char	*facility;		/* ie 0x1c pos  5 */
	unsigned char	*progress;		/* ie 0x1e pos  6 */
	unsigned char	*net_fac;		/* ie 0x20 pos  7 */
	unsigned char	*notify;		/* ie 0x27 pos  8 */
	unsigned char	*display;		/* ie 0x28 pos  9 */
	unsigned char	*date;			/* ie 0x29 pos 10 */
	unsigned char	*keypad;		/* ie 0x2c pos 11 */
	unsigned char	*signal;		/* ie 0x34 pos 12 */
	unsigned char	*info_rate;		/* ie 0x40 pos 13 */
	unsigned char	*end2end_transit;	/* ie 0x42 pos 14 */
	unsigned char	*transit_delay_sel;	/* ie 0x43 pos 15 */
	unsigned char	*pktl_bin_para;		/* ie 0x44 pos 16 */
	unsigned char	*pktl_window;		/* ie 0x45 pos 17 */
	unsigned char	*pkt_size;		/* ie 0x46 pos 18 */
	unsigned char	*closed_userg;		/* ie 0x47 pos 19 */
	unsigned char	*reverse_charge;	/* ie 0x4a pos 20 */
	unsigned char	*connected_nr;		/* ie 0x4c pos 21 */
	unsigned char	*connected_sub;		/* ie 0x4d pos 22 */
	unsigned char	*calling_nr;		/* ie 0x6c pos 23 */
	unsigned char	*calling_sub;		/* ie 0x6d pos 24 */
	unsigned char	*called_nr;		/* ie 0x70 pos 25 */
	unsigned char	*called_sub;		/* ie 0x71 pos 26 */
	unsigned char	*redirecting_nr;	/* ie 0x74 pos 27 */
	unsigned char	*redirection_nr;	/* ie 0x76 pos 28 */
	unsigned char	*transit_net_sel;	/* ie 0x78 pos 29 */
	unsigned char	*restart_ind;		/* ie 0x79 pos 30 */
	unsigned char	*llc;			/* ie 0x7c pos 31 */
	unsigned char	*hlc;			/* ie 0x7d pos 32 */
	unsigned char	*useruser;		/* ie 0x7e pos 33 */
	unsigned char	comprehension_req;
	unsigned char	more_data;
	unsigned char	sending_complete;
	unsigned char	congestion_level;
	struct m_extie	extra[8];
};

struct mlayer3;

/*
 * callback function to send and receive messages from and to layer3
 * @parameter1 struct mlayer3 - identfy the layer3
 * @parameter2 message type MT_ constants (Q931 and some private)
 * @parameter3 PID (process identification) value to identify the target process
 * @parameter4 optional layer3 message, if here are no special IE to deliver, use NULL
 */
typedef int (mlayer3_cb_t)(struct mlayer3 *, unsigned int, unsigned int, struct l3_msg *);


/* debug helper */
#define MISDN_LIBDEBUG_ERROR	1
#define MISDN_LIBDEBUG_WARN	2
#define MISDN_LIBDEBUG_INFO	3
#define MISDN_LIBDEBUG_DEBUG	4

typedef int (*mi_thread_create_t)(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine) (void *),
			void *arg, const char *file, const char *caller, int line, const char *start_fn);
typedef int (*mi_debug_t)(const char *file, int line, const char *func, int level, const char *fmt, va_list va);
typedef void * (*mi_malloc_t)(size_t size, const char *file, int line, const char *func);
typedef void * (*mi_calloc_t)(size_t nmemb, size_t size, const char *file, int line, const char *func);
typedef void (*mi_free_t)(void *ptr, const char *file, int line, const char *func);


struct mi_ext_fn_s {
	mi_thread_create_t	thread_create;
	mi_debug_t		prt_debug;
	mi_malloc_t		malloc;
	mi_calloc_t		calloc;
	mi_free_t		reuse;
	mi_free_t		free;
};

extern struct mi_ext_fn_s *mi_extern_func;

/*
 * To avoid to include always all headers needed for mISDNif.h we redefine MISDN_CHMAP_SIZE here
 * Please make sure to keep it in sync with mISDNif.h (but changes are very unlikely)
 */
#ifndef MISDN_CHMAP_SIZE
#define MISDN_MAX_CHANNEL	127
#define MISDN_CHMAP_SIZE	((MISDN_MAX_CHANNEL + 1) >> 3)
#endif

struct mISDN_devinfo;

struct mlayer3 {
	unsigned int		device;
	unsigned int		nr_bchannel;
	unsigned long		options;
	mlayer3_cb_t		*to_layer3;
	mlayer3_cb_t		*from_layer3;
	void			*priv; /* free use for applications */
	struct mISDN_devinfo	*devinfo;
};

/*
 * Layer3 protocols
 */
#define	L3_PROTOCOL_DSS1_USER	0x101
#define L3_PROTOCOL_DSS1_NET	0x102

/*
 * Layer3 property Flags
 *
 * 16...31 reserved for internal use
 *
 */
 
#define MISDN_FLG_PTP		1
#define MISDN_FLG_NET_HOLD	2
#define	MISDN_FLG_L2_HOLD	3
#define	MISDN_FLG_L2_CLEAN	4
#define	MISDN_FLG_L1_HOLD	5

/* 
 * Layer3 <----> Application additional message types
 * Basic messages are coded like Q931 MT_ from q931.h
 */
/* Application <---> L3 */
#define MT_ASSIGN		0x11000
/* L3 ---> Application */
#define MT_FREE			0x11001
#define MT_L2ESTABLISH		0x12000
#define MT_L2RELEASE		0x12001
#define MT_L2IDLE		0x12002
#define MT_ERROR		0x18000
#define MT_TIMEOUT		0x18001

/* 
 * process IDs
 *
 *    
 */
#define MISDN_PID_DUMMY		0x81000000
#define MISDN_PID_GLOBAL	0x82000000
#define MISDN_PID_NONE		0xFFFFFFFF
#define MISDN_PID_MASTER	0xFF000000
#define MISDN_PID_CRTYPE_MASK	0xFF000000
#define MISDN_PID_CID_MASK	0x00FF0000
#define MISDN_PID_CR_MASK	0xFF00FFFF
#define MISDN_PID_CRVAL_MASK	0x0000FFFF
#define MISDN_PID_CR_FLAG	0x00008000
#define MISDN_CES_MASTER	0x0000FF00

#define MISDN_LIB_VERSION	2
#define MISDN_LIB_RELEASE	5

#define MISDN_LIB_INTERFACE	((MISDN_LIB_VERSION << 16) | MISDN_LIB_RELEASE)

/*
 * init layer3 statemachines and caches
 * must be called before first open
 * @parameter count of cached mbuffers
 * @parameter optional block of external functions for debug
 * @return: interface version
 */
extern unsigned int	init_layer3(int, struct mi_ext_fn_s *);

/*
 * cleanup layer3 statemachines and chaches
 * must be called after all layer3 are closed
 */
extern void             cleanup_layer3(void);

/*
 * open a layer3 stack
 * @parameter1 - device id
 * @parameter2 - protocol
 * @parameter3 - layer3 additional properties
 * @parameter4 - callback function to deliver messages
 * @parameter5 - pointer for private application use
 */
extern struct mlayer3	*open_layer3(unsigned int, unsigned int, unsigned int, mlayer3_cb_t *, void *);

/*
 * close a layer3 stack
 * parameter1 - stack struct
 */
extern void		close_layer3(struct mlayer3 *);

extern unsigned int	request_new_pid(struct mlayer3 *);
extern int		mISDN_get_pcm_slots(struct mlayer3 *, int, int *, int *);
extern int		mISDN_set_pcm_slots(struct mlayer3 *, int, int, int);
extern int		add_layer3_ie(struct l3_msg *, unsigned char, int, unsigned char *);
extern void		l3_msg_increment_refcnt(struct l3_msg *);

#ifdef MEMLEAK_DEBUG
extern struct l3_msg    *__alloc_l3_msg(const char *file, int lineno, const char *func);
extern void             __free_l3_msg(struct l3_msg *, const char *file, int lineno, const char *func);

#define alloc_l3_msg()  __alloc_l3_msg(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define free_l3_msg(p)  __free_l3_msg(p, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#else
extern struct l3_msg    *alloc_l3_msg(void);
extern void             free_l3_msg(struct l3_msg *);
#endif

extern  void		mISDN_set_debug_level(unsigned int);

#ifdef __cplusplus
}
#endif

#endif
