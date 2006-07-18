#ifndef ISDN_DEBUG_H
#define ISDN_DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define	DBGM_NET	0x00000001
#define DBGM_MSG	0x00000002
#define DBGM_FSM	0x00000004
#define DBGM_TEI	0x00000010
#define DBGM_L2		0x00000020
#define DBGM_L3		0x00000040
#define DBGM_L3DATA	0x00000080
#define DBGM_BC		0x00000100
#define DBGM_TONE	0x00000200
#define	DBGM_BCDATA	0x00000400
#define DBGM_MAN	0x00001000
#define DBGM_APPL	0x00002000
#define DBGM_ISDN	0x00004000
#define DBGM_SOCK	0x00010000
#define DBGM_CONN	0x00020000
#define DBGM_CDATA	0x00040000
#define DBGM_DDATA	0x00080000
#define DBGM_SOUND	0x00100000
#define DBGM_SDATA	0x00200000
#define DBGM_TOPLEVEL	0x40000000
#define DBGM_ALL	0xffffffff

extern	int		dprint(unsigned int mask, int port, const char *fmt, ...);
extern	int		eprint(const char *fmt, ...);
extern	int		wprint(const char *fmt, ...);
extern	int		debug_init(unsigned int, char *, char *, char *);
extern	void		debug_close(void);
extern	int		dhexprint(unsigned int, char *, unsigned char *, int);
#endif
