/* mlayer3.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mlayer3.h>
#include <mbuffer.h>
#include <errno.h>
#include "helper.h"
#include "layer3.h"
#include "debug.h"

static	int	__init_done = 0;

void
init_layer3(int nr)
{
	init_mbuffer(nr);
	mISDNl3New();
	__init_done = 1;
	mISDN_debug_init(0, NULL, NULL, NULL);
}

void
cleanup_layer3(void)
{
	cleanup_mbuffer();
	mISDNl3Free();
	__init_done = 0;
}

/* for now, maybe get this via some register prortocol in future */
extern struct l3protocol	dss1user;
extern struct l3protocol	dss1net;
/*
 * open a layer3 stack
 * parameter1 - device id
 * parameter2 - protocol
 * parameter3 - layer3 additional properties
 * parameter4 - callback function to deliver messages
 * parameter5 - pointer for private application use
 */
struct mlayer3	*
open_layer3(unsigned int dev, unsigned int proto, unsigned int prop, mlayer3_cb_t *f, void *p)
{
	struct _layer3		*l3;
	int			fd, cnt, ret;
	struct mISDNversion	ver;
	struct mISDN_devinfo	devinfo;
	int			clean = 1;

	if (__init_done == 0) {
		fprintf(stderr, "You should call init_layer3(nr of message cache entres) first\n"); 
		init_layer3(10);
	}
	fd = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (fd < 0) {
		fprintf(stderr, "could not open socket %s\n", strerror(errno));
		return NULL;
	}
	ret = ioctl(fd, IMGETVERSION, &ver);
	if (ret < 0) {
		fprintf(stderr, "could not send IOCTL IMGETVERSION %s\n", strerror(errno));
		close(fd);
		return NULL;
	}
	iprint("mISDN kernel version %d.%02d.%d found\n", ver.major, ver.minor, ver.release);
	iprint("mISDN user   version %d.%02d.%d found\n", MISDN_MAJOR_VERSION, MISDN_MINOR_VERSION, MISDN_RELEASE);
	
	if (ver.major != MISDN_MAJOR_VERSION) {
		fprintf(stderr, "VERSION incompatible please update\n");
		close(fd);
		return NULL;
	}
	/* handle version backward compatibility specific  stuff here */
	
	/* nothing yet */

	ret = ioctl(fd, IMGETCOUNT, &cnt);
	if (ret < 0) {
		fprintf(stderr,"could not send IOCTL IMGETCOUNT %s\n", strerror(errno));
		close(fd);
		return NULL;
	}
	if (cnt < dev) {
		fprintf(stderr,"device %d do not exist\n", dev);
		close(fd);
		return NULL;
	}

	l3 = calloc(1, sizeof(struct _layer3));
	if (!l3)
		return NULL;
	l3->ml3.options = prop;
	l3->ml3.from_layer3 = f;
	l3->ml3.priv = p;

	init_l3(l3);
	devinfo.id = dev;
	ret = ioctl(fd, IMGETDEVINFO, &devinfo);
	if (ret < 0) {
		fprintf(stderr,"could not send IOCTL IMGETCOUNT %s\n", strerror(errno));
		goto fail;
	}
	close(fd);
	l3->ml3.nr_bchannel = devinfo.nrbchan;
	if (!(devinfo.Dprotocols & (1 << ISDN_P_TE_E1))
	 && !(devinfo.Dprotocols & (1 << ISDN_P_NT_E1)))
		test_and_set_bit(FLG_BASICRATE, &l3->ml3.options);
	switch(proto) {
	case L3_PROTOCOL_DSS1_USER:
		if (!(devinfo.Dprotocols & (1 << ISDN_P_TE_S0))
		 && !(devinfo.Dprotocols & (1 << ISDN_P_TE_E1))) {
			fprintf(stderr,"protocol L3_PROTOCOL_DSS1_USER device do not support ISDN_P_TE_S0 / ISDN_P_TE_E1\n");
			goto fail;
		}
		fd = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_LAPD_TE);
		if (fd < 0) {
			fprintf(stderr,"could not open ISDN_P_LAPD_TE socket %s\n", strerror(errno));
			goto fail;
		}
		dss1user.init(l3);
		break;
	case L3_PROTOCOL_DSS1_NET:
		if (!(devinfo.Dprotocols & (1 << ISDN_P_NT_S0)) 
		 && !(devinfo.Dprotocols & (1 << ISDN_P_NT_E1))) {
			fprintf(stderr,"protocol L3_PROTOCOL_DSS1_NET device do not support ISDN_P_NT_S0 / ISDN_P_NT_E1\n");
			goto fail;
		}
		fd = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_LAPD_NT);
		if (fd < 0) {
			fprintf(stderr,"could not open ISDN_P_LAPD_NT socket %s\n", strerror(errno));
			goto fail;
		}
		dss1net.init(l3);
		break;
	default:
		fprintf(stderr,"protocol %x not supported\n", proto);
		goto fail;
	}

	l3->l2master.l2addr.family = AF_ISDN;
	l3->l2master.l2addr.dev = dev;
	l3->l2master.l2addr.channel = 0;
	l3->l2master.l2addr.sapi = 0;
	if (test_bit(MISDN_FLG_PTP, &l3->ml3.options))
		l3->l2master.l2addr.tei = 0;
	else
		l3->l2master.l2addr.tei = 127;
	ret = bind(fd, (struct sockaddr *)&l3->l2master.l2addr, sizeof(l3->l2master.l2addr));
	if (ret < 0) {
		fprintf(stderr,"could not bind socket for device %d:%s\n", dev, strerror(errno));
		goto fail;
	}
	if (test_bit(MISDN_FLG_L2_CLEAN, &l3->ml3.options)
		&& proto == L3_PROTOCOL_DSS1_NET) {
		ret = ioctl(fd, IMCLEAR_L2, &clean);
       		if (ret < 0) {
			fprintf(stderr, "could not send IOCTL IMCLEAN_L2 %s\n", strerror(errno));
			goto fail;
		}
	}
	l3->l2sock = fd;
	l3->mdev = open("/dev/mISDNtimer", O_RDWR);
	if (l3->mdev < 0) {
		fprintf(stderr,"could not open /dev/mISDNtimer %s\n", strerror(errno));
		fprintf(stderr,"It seems that you don't use udev filesystem. You may use this workarround:\n\n");
		fprintf(stderr,"Do 'cat /proc/misc' and see the number in front of 'mISDNtimer'.\n");
		fprintf(stderr,"Do 'mknod /dev/mISDNtimer c 10 xx', where xx is the number you saw.\n");
		fprintf(stderr,"Note: This number changes if you load modules in different order, that use misc device.\n");
		goto fail;
	}
	if (l3->l2sock < l3->mdev)
		l3->maxfd = l3->mdev;
	else
		l3->maxfd = l3->l2sock;
	ret = l3_start(l3);
	if (ret < 0) {
		fprintf(stderr,"could not start layer3 thread for device %d\n", dev);
		close(l3->mdev);
		goto fail;
	}

	return(&l3->ml3);
fail:
	close(fd);
	release_l3(l3);
	free(l3);
	return NULL;
}

/*
 * close a layer3 stack
 * parameter1 - stack struct
 */
void
close_layer3(struct mlayer3 *ml3)
{
	struct _layer3	*l3;

	l3 = container_of(ml3, struct _layer3, ml3);
	l3_stop(l3);
	close(l3->l2sock);
	close(l3->mdev);
	release_l3(l3);
	free(l3);
}

