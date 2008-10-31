/* $Id: misdn.cpp 11 2008-10-31 18:35:50Z daxtar $
 * (c) 2008 Martin Bachem, m.bachem@gmx.de
 *
 * This file is part of qmisdnwatch
 *
 * Project's Home
 *     http://www.misdn.org/index.php/Qmisdnwatch
 *
 * qmisdnwatch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2
 *
 * qmisdnwatch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with qmisdnwatch.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "misdn.h"

#include <QApplication>
#define AF_COMPATIBILITY_FUNC
#define MISDN_OLD_AF_COMPATIBILITY
#include <mISDNuser/compat_af_isdn.h>



mISDN::mISDN(void) {
	numdevices = -1;
	memset(&kver, 0, sizeof(struct mISDNversion));
	connectCore();
}

mISDN::~mISDN(void) {
	if (isCoreConnected())
		close(sock);
}

int mISDN::connectCore(void) {
	init_af_isdn();
	sock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	qDebug("mISDN::connectCore %d", sock);
	if (isCoreConnected())
		queryVersion();
	return sock;
}

bool mISDN::isCoreConnected(void) {
	return(sock >= 0);
}

struct mISDNversion mISDN::getVersion(void) {
	return(kver);
}

void mISDN::queryVersion(void) {
	struct mISDNversion v;
	int ret;
	if (isCoreConnected()) {
		ret = ioctl(sock, IMGETVERSION, &v);
		if (ret >= 0)
			memcpy(&kver, &v, sizeof(struct mISDNversion));
	}
}

int mISDN::getNumDevices(void) {
	int cnt, ret = 0;
	if (isCoreConnected()) {
		ret = ioctl(sock, IMGETCOUNT, &cnt);
		if (!ret) {
			numdevices = cnt;
			return(cnt);
		}
	}
	return(-1);
}

int mISDN::getDeviceInfo(struct mISDN_devinfo *devinfo, int id) {
	int ret;
	if (isCoreConnected()) {
		devinfo->id = id;
		try {
			ret = ioctl(sock, IMGETDEVINFO, devinfo);
			if (!ret)
				return 0;
		}
		catch (...) {
			return -1;
		}
	}
	return -2;
}

/*
 * returns
 *   <0 on error
 *   =0 socket open with no E-Channel logging available
 *   =1 socket open with E-Channel logging enabled
 */
int mISDN::openl1Log(int id, int protocol, int * log_socket,
		     struct sockaddr_mISDN * log_addr)
{
	int ret, channel;
	if (!(isCoreConnected()))
		return -2;

	if ((*log_socket = socket(PF_ISDN, SOCK_DGRAM, protocol)) >= 0)
	{
		log_addr->family = AF_ISDN;
		log_addr->dev = id;
		ret = -1;
		channel = 1;
		while ((ret < 0) && (channel >= 0)) {
			log_addr->channel = (unsigned char)channel;
			ret = bind(*log_socket, (struct sockaddr *)log_addr,
				      sizeof(struct sockaddr_mISDN));
			if (ret < 0)
				channel--;
		}
		int opt=1;
		setsockopt(*log_socket, SOL_MISDN, MISDN_TIME_STAMP,
			    &opt, sizeof(opt));
		return (log_addr->channel == 1);
	}

	return -1;
}


int mISDN::getLastNumDevs(void) {
	return(numdevices);
}

int mISDN::renameLayer1(unsigned int id, char * name) {
	struct mISDN_devrename devrename;
	int ret;
	if (isCoreConnected()) {
		devrename.id = id;
		strncpy(devrename.name, name, MISDN_MAX_IDLEN);
		ret = ioctl(sock, IMSETDEVNAME, &devrename);
		return ret;
	}
	return -1;
}

int mISDN::cleanl2(unsigned int id) {
	int ret;
	if (isCoreConnected()) {
		struct mISDN_devinfo devinfo;
		if (getDeviceInfo(&devinfo, id) >= 0) {
			if (!devinfo.protocol)
				return -4;

			int l2sock;
			if (IS_ISDN_P_TE(devinfo.protocol))
				l2sock = socket(PF_ISDN, SOCK_DGRAM,
					ISDN_P_LAPD_TE);
			else
				l2sock = socket(PF_ISDN, SOCK_DGRAM,
					ISDN_P_LAPD_NT);
			if (l2sock >= 0) {
				struct sockaddr_mISDN addr;
				addr.family = AF_ISDN;
				addr.dev = id;
				addr.channel = 0;
				addr.sapi = 0;
				addr.tei = 127;
				ret = bind(l2sock, (struct sockaddr *) &addr,
					      sizeof(addr));
				if (ret >= 0) {
					int clean = 1;
					ret = ioctl(l2sock, IMCLEAR_L2, &clean);
					if (ret < 0)
						return -1;
					return 0;
				} else
					return -2;
			} else
				return -3;
		} else
			return -6;
	}
	return -7;
}
