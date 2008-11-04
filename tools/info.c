/*****************************************************************************\
**                                                                           **
** isdninfo                                                                  **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg  (GPL)                                       **
**                                                                           **
** user space utility to list mISDN devices                                  **
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
#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>

int main(int argc, char *argv[])
{
	int ret;
	int i, ii, c, start_c;
	int useable, nt, te, pri, bri, pots, s0;
	struct mISDN_devinfo devinfo;
	int sock;

	init_af_isdn();
	/* open mISDN */
	sock = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (sock < 0)
	{
		fprintf(stderr, "Cannot open mISDN due to %s. (Does your Kernel support socket based mISDN?)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* get number of stacks */
	i = 0;
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
	} else 
		printf("Found %i port%s\n", ii, (ii>1)?"s":"");

	/* loop the number of cards and get their info */
	while(ii && i <= MAX_DEVICE_ID)
	{
		nt = te = bri = pri = pots = s0 = 0;
		useable = 0;

		devinfo.id = i;
		ret = ioctl(sock, IMGETDEVINFO, &devinfo);
		if (ret < 0)
		{
			fprintf(stderr, "error getting info for device %d: %s\n", i,strerror(errno));
			goto next_dev;
		}

		/* output the port info */
		printf("  Port %2d: '%s': ", i, devinfo.name);
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_S0))
		{
			bri = 1;
			te = 1;
			s0 = 1;
		}
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_S0))
		{
			bri = 1;
			nt = 1;
			s0 = 1;
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
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_UP0))
		{
			bri = 1;
			te = 1;
		}
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_UP0))
		{
			bri = 1;
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
			printf("\tTE/NT-mode BRI %s (for phone lines & phones)",
				(s0) ? "S/T" : "UP0");
		if (te && !nt && bri)
			printf("\tTE-mode    BRI %s (for phone lines)",
				(s0) ? "S/T" : "UP0");
		if (nt && !te && bri)
			printf("\tNT-mode    BRI %s (for phones)",
				(s0) ? "S/T" : "UP0");
		if (te && nt && pri)
			printf("\tTE/NT-mode PRI E1  (for phone lines & E1 devices)");
		if (te && !nt && pri)
			printf("\tTE-mode    PRI E1  (for phone lines)");
		if (nt && !te && pri)
			printf("\tNT-mode    PRI E1  (for E1 devices)");
		if (te && nt && pots)
			printf("\tFXS/FXO    POTS    (for analog lines & phones)");
		if (te && !nt && pots)
			printf("\tFXS        POTS    (for analog lines)");
		if (nt && !te && pots)
			printf("\tFXO        POTS    (for analog phones)");
		if (pots)
		{
			useable = 0;
			printf("\n -> Analog interfaces are not supported.");
		} else
		if (!useable)
		{
			printf("unsupported interface protocol bits 0x%016x", devinfo.Dprotocols);
		}
		printf("\n\t\t\t\t%d B-channels:", devinfo.nrbchan);
		c = 0;
		start_c = -1;
		while(c <= MISDN_MAX_CHANNEL + 1)
		{
			if (c <= MISDN_MAX_CHANNEL && test_channelmap(c, devinfo.channelmap))
			{
				if (start_c < 0)
					start_c = c;
			} else
			{
				if (start_c >= 0)
				{
					if (c-1 == start_c)
						printf(" %d", start_c);
					else
						printf(" %d-%d", start_c, c-1);
					start_c = -1;
				}
			}
			c++;
		}
		printf("\n");

		if (!useable)
			printf(" * Port NOT useable for LCR\n");

		if (ii > 1)
			printf("  --------\n");
		ii--;

	next_dev:
		i++;
	}
	printf("\n");

done:
	close(sock);
	return(0);
}

