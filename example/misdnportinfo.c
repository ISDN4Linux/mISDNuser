/*
 *
 * Copyright 2008 Karsten Keil <kkeil@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
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

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <mISDNif.h>
#include <q931.h>

#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>

int main()
{
	int 			cnt, ret = 0, i = 0;
	int			sk;
	struct mISDN_devinfo	devinfo;

	init_af_isdn();
	sk = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sk < 1) {
		fprintf(stdout, "could not open socket %s\n", strerror(errno));
		return 2;
	}
	ret = ioctl(sk, IMGETCOUNT, &cnt);
	if (ret) {
		fprintf(stdout, "error getting interface count: %s\n", strerror(errno));
		close(sk);
		return 3;
	}

	printf("Found %d device%s\n",cnt,(cnt==1)?"":"s");

	while(cnt && i <= MAX_DEVICE_ID) {
		devinfo.id = i;
		ret = ioctl(sk, IMGETDEVINFO, &devinfo);
		if (ret < 0) {
			fprintf(stdout, "error getting info for device %d: %s\n", i, strerror(errno));
		} else {
			fprintf(stdout, "        id:             %d\n", devinfo.id);
			fprintf(stdout, "        Dprotocols:     %08x\n", devinfo.Dprotocols);
			fprintf(stdout, "        Bprotocols:     %08x\n", devinfo.Bprotocols);
			fprintf(stdout, "        protocol:       %d\n", devinfo.protocol);
			fprintf(stdout, "        nrbchan:        %d\n", devinfo.nrbchan);
			fprintf(stdout, "        name:           %s\n", devinfo.name);
			cnt--;
		}
		i++;
	}

	close(sk);
	return 0;
}
