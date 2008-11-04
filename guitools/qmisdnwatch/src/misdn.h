/* $Id: misdn.h 11 2008-10-31 18:35:50Z daxtar $
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

#ifndef _MISDN_H_
#define _MISDN_H_

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <linux/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <mISDNuser/mISDNif.h>
#include <mISDNuser/q931.h>


#ifndef ISDN_P_TE_UP0
#define ISDN_P_TE_UP0 0x05
#endif

#ifndef ISDN_P_NT_UP0
#define ISDN_P_NT_UP0 0x06
#endif

#ifndef IS_ISDN_P_TE
#define IS_ISDN_P_TE(p) ((p == ISDN_P_TE_S0) || (p == ISDN_P_TE_E1) || \
			(p == ISDN_P_TE_UP0) || (p == ISDN_P_LAPD_TE))
#endif
#ifndef IS_ISDN_P_NT
#define IS_ISDN_P_NT(p) ((p == ISDN_P_NT_S0) || (p == ISDN_P_NT_E1) || \
			(p == ISDN_P_NT_UP0) || (p == ISDN_P_LAPD_NT))
#endif
#ifndef IS_ISDN_P_S0
#define IS_ISDN_P_S0(p) ((p == ISDN_P_TE_S0) || (p == ISDN_P_NT_S0))
#endif
#ifndef IS_ISDN_P_E1
#define IS_ISDN_P_E1(p) ((p == ISDN_P_TE_E1) || (p == ISDN_P_NT_E1))
#endif
#ifndef IS_ISDN_P_UP0
#define IS_ISDN_P_UP0(p) ((p == ISDN_P_TE_UP0) || (p == ISDN_P_NT_UP0))
#endif


class mISDN {
	public:
		mISDN(void);
		~mISDN(void);
		int connectCore(void);
		bool isCoreConnected(void);
		int getNumDevices(void);
		int getDeviceInfo(struct mISDN_devinfo *devinfo, int id);
		int getLastNumDevs(void);
		struct mISDNversion getVersion(void);

		/* socket helper */
		int openl1Log(int id, int protocol, int * log_socket,
			      struct sockaddr_mISDN * log_addr);

		/* tools / examples */
		int renameLayer1(unsigned int id, char * name);
		int cleanl2(unsigned int id);

	private:
		void queryVersion(void);
		int numdevices; // devicesNumber
		int sock; // base socket handles
		struct mISDNversion kver; // mISDN kernel version
};

struct ctstamp {
	size_t		cmsg_len;
	int		cmsg_level;
	int		cmsg_type;
	struct timeval	tv;
};


#endif // _MISDN_H_
