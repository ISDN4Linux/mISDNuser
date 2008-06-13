/*****************************************************************************\
**                                                                           **
** isdnbridge                                                                **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg  (GPL)                                       **
**                                                                           **
** user space utility to bridge two mISDN ports via layer 1                  **
**                                                                           **
\*****************************************************************************/ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <mlayer3.h>
#include <mbuffer.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	int ret;
	int i, ii;
	int useable, nt, te, pri, bri, pots;
	struct mISDN_devinfo devinfo;
	int sock;

	/* open mISDN */
	sock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sock < 0)
	{
		fprintf(stderr, "Cannot open mISDN due to %s. (Does your Kernel support socket based mISDN?)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* get number of stacks */
	i = 1;
	ret = ioctl(sock, IMGETCOUNT, &ii);
	if (ret < 0)
	{
		fprintf(stderr, "Cannot get number of mISDN devices. (ioctl IMGETCOUNT failed ret=%d)\n", ret);
		goto done;
	}
	printf("\n");
	if (ii <= 0)
	{
		printf("Found no card. Please be sure to load card drivers.\n");
		goto done;
	}

	/* loop the number of cards and get their info */
	while(i <= ii)
	{
		nt = te = bri = pri = pots = 0;
		useable = 0;

		devinfo.id = i - 1;
		ret = ioctl(sock, IMGETDEVINFO, &devinfo);
		if (ret < 0)
		{
			fprintf(stderr, "Cannot get device information for port %d. (ioctl IMGETDEVINFO failed ret=%d)\n", i, ret);
			break;
		}

		/* output the port info */
		printf("Port %2d name='%s': ", i, devinfo.name);
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_S0))
		{
			bri = 1;
			te = 1;
		}
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_S0))
		{
			bri = 1;
			nt = 1;
		}
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_E1))
		{
			pri = 1;
			te = 1;
		}
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_E1))
		{
			pri = 1;
			nt = 1;
		}
#ifdef ISDN_P_FXS
		if (devinfo.Dprotocols & (1 << ISDN_P_FXS))
		{
			pots = 1;
			te = 1;
		}
#endif
#ifdef ISDN_P_FXO
		if (devinfo.Dprotocols & (1 << ISDN_P_FXO))
		{
			pots = 1;
			nt = 1;
		}
#endif
		if ((te || nt) && (bri || pri || pots))
			useable = 1;

		if (te && nt && bri)
			printf("TE/NT-mode BRI S/T (for phone lines & phones)");
		if (te && !nt && bri)
			printf("TE-mode    BRI S/T (for phone lines)");
		if (nt && !te && bri)
			printf("NT-mode    BRI S/T (for phones)");
		if (te && nt && pri)
			printf("TE/NT-mode PRI E1  (for phone lines & E1 devices)");
		if (te && !nt && pri)
			printf("TE-mode    PRI E1  (for phone lines)");
		if (nt && !te && pri)
			printf("NT-mode    PRI E1  (for E1 devices)");
		if (te && nt && pots)
			printf("FXS/FXO    POTS    (for analog lines & phones)");
		if (te && !nt && pots)
			printf("FXS        POTS    (for analog lines)");
		if (nt && !te && pots)
			printf("FXO        POTS    (for analog phones)");
		if (pots)
		{
			useable = 0;
			printf("\n -> Analog interfaces are not supported.");
		} else
		if (!useable)
		{
			printf("unsupported interface protocol bits 0x%016x", devinfo.Dprotocols);
		}
		printf("\n");

		printf("  - %d B-channels\n", devinfo.nrbchan);

		if (!useable)
			printf(" * Port NOT useable for LCR\n");

		printf("--------\n");

		i++;
	}
	printf("\n");

done:
	close(sock);
	return(0);
}

