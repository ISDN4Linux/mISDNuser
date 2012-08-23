/* mlayer3.c
 *
 * Author       Karsten Keil <kkeil@linux-pingi.de>
 *
 * Copyright 2007  by Karsten Keil <kkeil@novell.com>
 * Copyright 2010  by Karsten Keil <kkeil@linux-pingi.de>
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
#include <mISDN/mlayer3.h>
#include <mISDN/mbuffer.h>
#include <errno.h>
#include "helper.h"
#include "layer3.h"
#include "debug.h"
#include <mISDN/af_isdn.h>

static	int	__init_done = 0;

struct mi_ext_fn_s *mi_extern_func;

unsigned int
init_layer3(int nr, struct mi_ext_fn_s *fn)
{
	init_mbuffer(nr);
	mISDNl3New();
	__init_done = 1;
	mI_debug_mask = 0;
	if (fn)
	        mi_extern_func = fn;
        else
                mi_extern_func = NULL;
	return MISDN_LIB_INTERFACE;
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
	int			fd, ret;
	struct mISDNversion	ver;
	int			set = 1;

	if (__init_done == 0) {
		eprint("You should call init_layer3(nr of message cache entres) first\n");
		init_layer3(10, NULL);
	}
	fd = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (fd < 0) {
		eprint("could not open socket %s\n", strerror(errno));
		return NULL;
	}
	ret = ioctl(fd, IMGETVERSION, &ver);
	if (ret < 0) {
		eprint("could not send IOCTL IMGETVERSION %s\n", strerror(errno));
		close(fd);
		return NULL;
	}

	if (ver.release & MISDN_GIT_RELEASE)
		iprint("mISDN kernel version %d.%02d.%d (git.misdn.eu) found\n", ver.major, ver.minor, ver.release & ~MISDN_GIT_RELEASE);
	else
		iprint("mISDN kernel version %d.%02d.%d found\n", ver.major, ver.minor, ver.release);
	iprint("mISDN user   version %d.%02d.%d found\n", MISDN_MAJOR_VERSION, MISDN_MINOR_VERSION, MISDN_RELEASE);
	iprint("mISDN library interface version %d release %d\n", MISDN_LIB_VERSION, MISDN_LIB_RELEASE);

	if (ver.major != MISDN_MAJOR_VERSION) {
		eprint("VERSION incompatible please update\n");
		close(fd);
		return NULL;
	}
	/* handle version backward compatibility specific  stuff here */

	l3 = calloc(1, sizeof(struct _layer3));
	if (!l3)
		return NULL;
	l3->ml3.devinfo = calloc(1, sizeof(*l3->ml3.devinfo));
	if (!l3->ml3.devinfo) {
		free(l3);
		return NULL;
	}
	l3->ml3.options = prop;
	l3->ml3.from_layer3 = f;
	l3->ml3.priv = p;

	init_l3(l3);
	l3->ml3.devinfo->id = dev;
	ret = ioctl(fd, IMGETDEVINFO, l3->ml3.devinfo);
	if (ret < 0) {
		eprint("could not send IOCTL IMGETCOUNT %s\n", strerror(errno));
		goto fail;
	}
	close(fd);
	l3->ml3.nr_bchannel = l3->ml3.devinfo->nrbchan;
	if (!(l3->ml3.devinfo->Dprotocols & (1 << ISDN_P_TE_E1))
	 && !(l3->ml3.devinfo->Dprotocols & (1 << ISDN_P_NT_E1)))
		test_and_set_bit(FLG_BASICRATE, &l3->ml3.options);
	switch(proto) {
	case L3_PROTOCOL_DSS1_USER:
		if (!(l3->ml3.devinfo->Dprotocols & (1 << ISDN_P_TE_S0))
		 && !(l3->ml3.devinfo->Dprotocols & (1 << ISDN_P_TE_E1))) {
			eprint("protocol L3_PROTOCOL_DSS1_USER device do not support ISDN_P_TE_S0 / ISDN_P_TE_E1\n");
			goto fail;
		}
		fd = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_LAPD_TE);
		if (fd < 0) {
			eprint("could not open ISDN_P_LAPD_TE socket %s\n", strerror(errno));
			goto fail;
		}
		dss1user.init(l3);
		break;
	case L3_PROTOCOL_DSS1_NET:
		if (!(l3->ml3.devinfo->Dprotocols & (1 << ISDN_P_NT_S0))
		 && !(l3->ml3.devinfo->Dprotocols & (1 << ISDN_P_NT_E1))) {
			eprint("protocol L3_PROTOCOL_DSS1_NET device do not support ISDN_P_NT_S0 / ISDN_P_NT_E1\n");
			goto fail;
		}
		fd = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_LAPD_NT);
		if (fd < 0) {
			eprint("could not open ISDN_P_LAPD_NT socket %s\n", strerror(errno));
			goto fail;
		}
		dss1net.init(l3);
		break;
	default:
		eprint("protocol %x not supported\n", proto);
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
		eprint("could not bind socket for device %d:%s\n", dev, strerror(errno));
		goto fail;
	}
	if (test_bit(MISDN_FLG_L2_CLEAN, &l3->ml3.options)
		&& proto == L3_PROTOCOL_DSS1_NET) {
		ret = ioctl(fd, IMCLEAR_L2, &set);
		if (ret < 0) {
			eprint("could not send IOCTL IMCLEAN_L2 %s\n", strerror(errno));
			goto fail;
		}
	}
	if (test_bit(MISDN_FLG_L1_HOLD, &l3->ml3.options)) {
		ret = ioctl(fd, IMHOLD_L1, &set);
		if (ret < 0) {
			eprint("could not send IOCTL IMHOLD_L1 %s\n", strerror(errno));
			goto fail;
		}
	}
	l3->l2sock = fd;
	l3->tbase.tdev = open("/dev/mISDNtimer", O_RDWR);
	if (l3->tbase.tdev < 0) {
		eprint("could not open /dev/mISDNtimer %s\n", strerror(errno));
		eprint("It seems that you don't use udev filesystem. You may use this workarround:\n\n");
		eprint("Do 'cat /proc/misc' and see the number in front of 'mISDNtimer'.\n");
		eprint("Do 'mknod /dev/mISDNtimer c 10 xx', where xx is the number you saw.\n");
		eprint("Note: This number changes if you load modules in different order, that use misc device.\n");
		goto fail;
	}
	if (l3->l2sock < l3->tbase.tdev)
		l3->maxfd = l3->tbase.tdev;
	else
		l3->maxfd = l3->l2sock;
	ret = l3_start(l3);
	if (ret < 0) {
		eprint("could not start layer3 thread for device %d\n", dev);
		close(l3->tbase.tdev);
		goto fail;
	}

	return(&l3->ml3);
fail:
	close(fd);
	release_l3(l3);
	free(l3->ml3.devinfo);
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
	close(l3->tbase.tdev);
	release_l3(l3);
	if (ml3->devinfo)
		free(ml3->devinfo);
	free(l3);
}

int
mISDN_set_pcm_slots(struct mlayer3 *ml3, int channel, int tx, int rx)
{
/* need to be reimplemented */
#ifdef MISDN_CTRL_SET_PCM_SLOTS
	struct _layer3	*l3;
	struct mISDN_ctrl_req ctrlrq;
	int ret;

	ctrlrq.op = MISDN_CTRL_SET_PCM_SLOTS;
	ctrlrq.channel = 0; /* via D-channel */
	ctrlrq.p1 = channel;
	ctrlrq.p2 = tx;
	ctrlrq.p3 = rx;
	l3 = container_of(ml3, struct _layer3, ml3);
	ret = ioctl(l3->l2sock, IMCTRLREQ, &ctrlrq);
	if (ret < 0)
		eprint("could not send IOCTL IMCTRLREQ %s\n", strerror(errno));
#else
	int ret = -EOPNOTSUPP;

	eprint("%s not supported in this version\n", __func__);
#endif
	return ret;
}

int
mISDN_get_pcm_slots(struct mlayer3 *ml3, int channel, int *txp, int *rxp)
{
#ifdef MISDN_CTRL_GET_PCM_SLOTS
	struct _layer3	*l3;
	struct mISDN_ctrl_req ctrlrq;
	int ret;

	ctrlrq.op = MISDN_CTRL_GET_PCM_SLOTS;
	ctrlrq.channel = 0; /* via D-channel */
	ctrlrq.p1 = channel;
	l3 = container_of(ml3, struct _layer3, ml3);
	ret = ioctl(l3->l2sock, IMCTRLREQ, &ctrlrq);
	if (ret < 0)
		eprint("could not send IOCTL IMCTRLREQ %s\n", strerror(errno));
	else {
		if (txp)
			*txp = ctrlrq.p2;
		if (rxp)
			*rxp = ctrlrq.p3;
	}
#else
	int ret = -EOPNOTSUPP;

	eprint("%s not supported in this version\n", __func__);
#endif
	return ret;
}
