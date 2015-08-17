/*
 *
 * Copyright 2014 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 *  Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 */

#ifndef _LOGGER_H
#define _LOGGER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <sys/syslog.h>
#include <mISDN/mISDNif.h>
#include <mISDN/mbuffer.h>

#define L3MT_SIZE	40
#define LOGTXT_SIZE	1024
#define MAX_FILE_NAME	256

struct mController {
	int			mNr;
	int			enable;
	int			syslog;
	char			logfile[MAX_FILE_NAME];
	char			dumpfile[MAX_FILE_NAME];
	FILE			*log;
	FILE			*dump;
	int			layer1;
	int			layer2;
	unsigned char		layer3[L3MT_SIZE];
	int			protocol;
	int			echo;
	int			socket;
	struct sockaddr_mISDN	addr;
	struct mISDN_devinfo	devinfo;
	struct timeval		*tv;
	char			*lp;
	char			logtxt[LOGTXT_SIZE];
};

extern struct mController *defController;
extern int mI_ControllerCount;

enum l3Values {
	l3vDISABLE	= 0,
	l3vENABLE	= 1,
	l3vVERBOSE	= 2,
	l3vHEX		= 4
};

enum l2Values {
	l2vDISABLE	= 0,
	l2vENABLE	= 0x0001,
	l2vTEI		= 0x0002,
	l2vSAPI		= 0x0004,
	l2vCONTROL	= 0x0008,
	l2vKEEPALIVE	= 0x0010
};

enum mTypes {
	mTunknown = 0,
	mTalerting,
	mTcall_proceeding,
	mTconnect,
	mTconnect_acknowledge,
	mTprogress,
	mTsetup,
	mTsetup_acknowledge,
	mTresume,
	mTresume_acknowledge,
	mTresume_reject,
	mTsuspend,
	mTsuspend_acknowledge,
	mTsuspend_reject,
	mTuser_information,
	mTdisconnect,
	mTrelease,
	mTrelease_complete,
	mTrestart,
	mTrestart_acknowledge,
	mTsegment,
	mTcongestion_control,
	mTinformation,
	mTfacility,
	mTnotify,
	mTstatus,
	mTstatus_enquiry,
	mThold,
	mThold_acknowledge,
	mThold_reject,
	mTretrieve,
	mTretrieve_acknowledge,
	mTretrieve_reject,
	mTregister,
	mTall
};

extern const char *mTypesStr[];


#define mlDEBUG_BASIC	0x01000000
#define mlDEBUG_MESSAGE	0x02000000
#define mlDEBUG_POLL	0x04000000

#endif /* _CALL_LOG_H */
