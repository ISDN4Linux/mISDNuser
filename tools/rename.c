/*****************************************************************************\
**                                                                           **
** isdnrename                                                                **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Matthias Urlichs  (GPL)                                        **
**            based on code from Andreas Eversberg                           **
**                                                                           **
** user space utility to rename a mISDN device                               **
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
#include <ctype.h>
#include <mISDNif.h>
#define AF_COMPATIBILITY_FUNC
#include <compat_af_isdn.h>

int main(int argc, char *argv[])
{
	int ret;
	int i, ii;
	struct mISDN_devinfo devinfo;
	struct mISDN_devrename devname;
	int sock;

	if (argc != 3) {
		fprintf(stderr,"Usage: %s <old name or ID> <new name>\n",
		        argv[0]);
		exit(2);
	}
	if (! argv[2][0] || strlen(argv[2]) >= MISDN_MAX_IDLEN) {
		fprintf(stderr,"New device name: must be at most %d bytes long.\n",MISDN_MAX_IDLEN-1);
		exit(2);
	}

	init_af_isdn();
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
		fprintf(stderr, "Cannot get number of mISDN devices: %s\n", strerror(errno));
		goto done;
	}
	if (ii == 0) {
		fprintf(stderr, "No mISDN devices found.\n");
		goto done;
	}

	if(isdigit(argv[1][0])) {
		i = atoi(argv[1]);
		if (i < 0) {
			fprintf(stderr,"Interface number must be >= zero.\n");
			exit(1);
		}
	} else {
		if (! argv[1][0] || strlen(argv[1]) >= MISDN_MAX_IDLEN) {
			fprintf(stderr,"Old device name: may be at most %d bytes long.\n",MISDN_MAX_IDLEN-1);
			exit(2);
		}

		i = 0;
		while(ii && i <= MAX_DEVICE_ID)
		{
			devinfo.id = i;
			ret = ioctl(sock, IMGETDEVINFO, &devinfo);
			if (ret < 0)
			{
				fprintf(stderr, "error getting info for device %d: %s\n", i,strerror(errno));
				goto next_dev;
			}
			if (!strcmp (argv[1], devinfo.name))
				goto found_dev;
			--ii;
		next_dev:
			i++;
		}
		fprintf(stderr,"Interface not found.\n");
		goto done;
	}
found_dev:
	devname.id = i;
	strncpy(devname.name,argv[2],MISDN_MAX_IDLEN);
	ret = ioctl(sock, IMSETDEVNAME, &devname);
	if (ret < 0)
	{
		fprintf(stderr, "Cannot set device name for port %d: %s\n", i, strerror(errno));
		exit(1);
	}

done:
	close(sock);
	return(0);
}

