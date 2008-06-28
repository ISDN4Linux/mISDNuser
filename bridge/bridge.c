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
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mlayer3.h>
#include <mbuffer.h>
#include <errno.h>

int portcount = 0; /* counts all open ports for finding pair */
int mISDNsocket = -1;

/* quit flag */
int quit = 0;

/* option stuff */
int nooutput = 0;
int traffic = 0;
int debug = 0;

pid_t dsp_pid = 0;

/* mISDN port structure list */
struct mISDNport {
	struct mISDNport *next, *prev;
	int count; /* port count */
	int portnum; /* port number */
	int l1link; /* if l1 is available (only works with nt-mode) */
	time_t l1establish; /* time until establishing after link failure */
	int ntmode; /* is TRUE if port is nt mode */
	int pri; /* is TRUE if port is a primary rate interface */
	int d_sock;
	int b_num; /* number of ports */
	int b_sock[256];
	int b_state[256]; /* state 0 = IDLE */
	unsigned char que_frm[2048]; /* queue while layer 1 is down */
	int que_len;
};

struct mISDNport *mISDNport_first = NULL;

enum { B_STATE_IDLE, B_STATE_ACTIVATING, B_STATE_ACTIVE, B_STATE_DEACTIVATING };

/*
 * show state
 */
static void show_state(struct mISDNport *m1)
{
	struct mISDNport *m2 = m1;

	if (nooutput)
		return;
	
	if (m1->count & 1)
		m1 = m1->prev;
	else
		m2 = m2->next;

	printf("Port %2d %s  <->  Port %2d %s\n", m1->portnum, (m1->l1link)?"ACTIVE":"inactive", m2->portnum, (m2->l1link)?"ACTIVE":"inactive");
}
/*
 * show traffic
 */
static void show_traffic(struct mISDNport *m1, unsigned char *data, int len)
{
	struct mISDNport *m2 = m1;
	int right, i;

	if (nooutput)
		return;
	
	if (m1->count & 1)
	{
		m1 = m1->prev;
		right = 0;
	} else
	{
		m2 = m2->next;
		right = 1;
	}

	printf("Port %2d  %s  Port %2d :", m1->portnum, right?"-->":"<--", m2->portnum);
	i = 0;
	while(i < len)
	{
		printf(" %02x", data[i]);
		i++;
	}
	printf("\n");
}

/*
 * debug output
 */
#define PDEBUG(fmt, arg...) _printdebug(__FUNCTION__, __LINE__, fmt, ## arg)
static void _printdebug(const char *function, int line, const char *fmt, ...)
{
	char buffer[4096];
	va_list args;

	if (!debug || nooutput)
		return;

	va_start(args,fmt);
	vsnprintf(buffer, sizeof(buffer)-1, fmt, args);
	buffer[sizeof(buffer)-1]=0;
	va_end(args);

	printf("%s, line %d: %s", function, line, buffer);
}

/*
 * signal handler to interrupt main loop
 */
static void sighandler(int sigset)
{
	if (sigset == SIGHUP)
		return;
	if (sigset == SIGPIPE)
		return;
	fprintf(stderr, "Signal received: %d\n", sigset);
	if (!quit)
		quit = 1;

}


/*
 * send control information to the channel (dsp-module)
 */
static void ph_control(int sock, int c1, int c2)
{
	unsigned char data[MISDN_HEADER_LEN+sizeof(int)+sizeof(int)];
	struct mISDNhead *hh = (struct mISDNhead *)data;
	int len;
	unsigned long *d = (unsigned long *)(data + MISDN_HEADER_LEN);

	hh->prim = PH_CONTROL_REQ;
	hh->id = 0;
	len = MISDN_HEADER_LEN + sizeof(unsigned long)*2;
	*d++ = c1;
	*d++ = c2;
	len = sendto(sock, hh, len, 0, NULL, 0);
	if (len <= 0)
		fprintf(stderr, "Failed to send to socket %d\n", sock);
}

void ph_control_block(int sock, int c1, void *c2, int c2_len)
{
	unsigned char data[MISDN_HEADER_LEN+sizeof(int)+c2_len];
	struct mISDNhead *hh = (struct mISDNhead *)data;
	int len;
	unsigned long *d = (unsigned long *)(data + MISDN_HEADER_LEN);

	hh->prim = PH_CONTROL_REQ;
	hh->id = 0;
	len = MISDN_HEADER_LEN + sizeof(unsigned long) + c2_len;
	*d++ = c1;
	memcpy(d, c2, c2_len);
	len = sendto(sock, hh, len, 0, NULL, 0);
	if (len <= 0)
		fprintf(stderr, "Failed to send to socket %d\n", sock);
}


/*
 * activate / deactivate bchannel
 */
static void bchannel_activate(struct mISDNport *mISDNport, int i)
{
	struct mISDNhead hh;
	int len;

	/* we must activate if we are deactivated */
	if (mISDNport->b_state[i] == B_STATE_IDLE)
	{
		/* activate bchannel */
		PDEBUG("activating bchannel (index %d), because currently idle.\n", i);
		hh.prim = PH_ACTIVATE_REQ; 
		hh.id = 0;
		len = sendto(mISDNport->b_sock[i], &hh, MISDN_HEADER_LEN, 0, NULL, 0);
		if (len <= 0)
			fprintf(stderr, "Failed to send to socket %d of port %d channel index %d\n", mISDNport->b_sock[i], mISDNport->portnum, i);
		mISDNport->b_state[i] = B_STATE_ACTIVATING;
		return;
	}

	/* if we are active, we configure our channel */
	if (mISDNport->b_state[i] == B_STATE_ACTIVE)
	{
		/* it is an error if this channel is not associated with a port object */
		PDEBUG("during activation, we set rxoff.\n");
		ph_control(mISDNport->b_sock[i], DSP_RECEIVE_OFF, 0);
		PDEBUG("during activation, we add conference to %d.\n", ((mISDNport->count&(~1)) << 23) + (i<<16) + dsp_pid);
		ph_control(mISDNport->b_sock[i], DSP_CONF_JOIN, ((mISDNport->count&(~1)) << 23) + (i<<16) + dsp_pid);
#if 0
		if (sadks->crypt)
		{
			PDEBUG("during activation, we set crypt to crypt=%d.\n", mISDNport->b_port[i]->p_m_crypt);
			ph_control_block(mISDNport->b_addr[i], BF_ENABLE_KEY, mISDNport->b_port[i]->p_m_crypt_key, mISDNport->b_port[i]->p_m_crypt_key_len);
		}
#endif
	}
}


static void bchannel_deactivate(struct mISDNport *mISDNport, int i)
{
	struct mISDNhead hh;
	int len;

	if (mISDNport->b_state[i] == B_STATE_ACTIVE)
	{
		ph_control(mISDNport->b_sock[i], DSP_CONF_SPLIT, 0);
		ph_control(mISDNport->b_sock[i], DSP_RECEIVE_ON, 0);
		/* deactivate bchannel */
		PDEBUG("deactivating bchannel (index %d), because currently active.\n", i);
		hh.prim = PH_DEACTIVATE_REQ; 
		hh.id = 0;
		len = sendto(mISDNport->b_sock[i], &hh, MISDN_HEADER_LEN, 0, NULL, 0);
		if (len <= 0)
			fprintf(stderr, "Failed to send to socket %d of port %d channel index %d\n", mISDNport->b_sock[i], mISDNport->portnum, i);
		mISDNport->b_state[i] = B_STATE_DEACTIVATING;
		return;
	}
}

/*
 * main loop for processing messages from mISDN device
 */
int mISDN_handler(void)
{
	struct mISDNport *mISDNport;
	int i;
	unsigned char data[2048];
	struct mISDNhead *hh = (struct mISDNhead *)data;
	int len;
	int work = 0;

	/* handle all ports */
	mISDNport = mISDNport_first;
	while(mISDNport)
	{
		len = recv(mISDNport->d_sock, data, sizeof(data), 0);
		if (len >= (int)MISDN_HEADER_LEN)
		{
			work = 1;
			/* d-message */
			switch(hh->prim)
			{
				case PH_ACTIVATE_CNF:
				case PH_ACTIVATE_IND:
				PDEBUG("Received PH_ACTIVATE for port %d.\n", mISDNport->portnum);
				if (!mISDNport->l1link)
				{
					mISDNport->l1link = 1;
					show_state(mISDNport);
				}
				if (mISDNport->que_len)
				{
					PDEBUG("Data in que, due to inactive link on port %d.\n", mISDNport->portnum);
					len = sendto(mISDNport->d_sock, mISDNport->que_frm, mISDNport->que_len, 0, NULL, 0);
					if (len <= 0)
						fprintf(stderr, "Failed to send to socket %d of port %d\n", mISDNport->d_sock, mISDNport->portnum);
					mISDNport->que_len = 0;
				}
				break;

				case PH_DEACTIVATE_CNF:
				case PH_DEACTIVATE_IND:
				PDEBUG("Received PH_DEACTIVATE for port %d.\n", mISDNport->portnum);
				if (mISDNport->l1link)
				{
					mISDNport->l1link = 0;
					show_state(mISDNport);
				}
				mISDNport->que_len = 0;
				break;

				case PH_CONTROL_CNF:
				case PH_CONTROL_IND:
				PDEBUG("Received PH_CONTROL for port %d.\n", mISDNport->portnum);
				break;

				case PH_DATA_IND:
				if (traffic)
					show_traffic(mISDNport, data + MISDN_HEADER_LEN, len-MISDN_HEADER_LEN);
				PDEBUG("GOT data from %s port %d prim 0x%x id 0x%x\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, hh->prim, hh->id);
				if (mISDNport->count & 1)
				{
					if (mISDNport->prev == NULL)
					{
						printf("soft error, no prev where expected.\n");
						exit (0);
					}
					/* sending to previous port */
					PDEBUG("sending to %s port %d prim 0x%x id 0x%x\n", (mISDNport->prev->ntmode)?"NT":"TE", mISDNport->prev->portnum, hh->prim, hh->id);
					hh->prim = PH_DATA_REQ; 
					if (mISDNport->prev->l1link)
					{
						len = sendto(mISDNport->prev->d_sock, data, len, 0, NULL, 0);
						if (len <= 0)
							fprintf(stderr, "Failed to send to socket %d of port %d\n", mISDNport->d_sock, mISDNport->portnum);
					} else {
						PDEBUG("layer 1 is down, so we queue and activate link.\n");
						memcpy(mISDNport->prev->que_frm, data, len);
						mISDNport->prev->que_len = len;
						hh->prim = PH_ACTIVATE_REQ; 
						hh->id = 0;
						len = sendto(mISDNport->d_sock, data, MISDN_HEADER_LEN, 0, NULL, 0);
						if (len <= 0)
							fprintf(stderr, "Failed to send to socket %d of port %d\n", mISDNport->d_sock, mISDNport->portnum);
					}
				} else {

					if (mISDNport->next == NULL)
					{
						printf("soft error, no next where expected.\n");
						exit (0);
					}
					/* sending to next port */
					PDEBUG("sending to %s port %d prim 0x%x id 0x%x\n", (mISDNport->next->ntmode)?"NT":"TE", mISDNport->next->portnum, hh->prim, hh->id);
					hh->prim = PH_DATA_REQ; 
					if (mISDNport->next->l1link)
					{
						len = sendto(mISDNport->next->d_sock, data, len, 0, NULL, 0);
						if (len <= 0)
							fprintf(stderr, "Failed to send to socket %d of port %d\n", mISDNport->d_sock, mISDNport->portnum);
					} else {
						PDEBUG("layer 1 is down, so we queue and activate link.\n");
						memcpy(mISDNport->next->que_frm, data, len);
						mISDNport->next->que_len = len;
						hh->prim = PH_ACTIVATE_REQ; 
						hh->id = 0;
						len = sendto(mISDNport->d_sock, data, MISDN_HEADER_LEN, 0, NULL, 0);
						if (len <= 0)
							fprintf(stderr, "Failed to send to socket %d of port %d\n", mISDNport->d_sock, mISDNport->portnum);
					}
				}
				break;

				case PH_DATA_CNF:
				//PDEBUG("GOT confirm from %s port %d prim 0x%x id 0x%x\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, hh->prim, hh->id);
				break;

				case PH_DATA_REQ:
				//PDEBUG("GOT strange PH_DATA REQUEST from %s port %d prim 0x%x id 0x%x 0x%x\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, hh->prim, hh->id);
				break;

				default:
				break;
			}
		}
		i = 0;
		while(i < mISDNport->b_num)
		{
			len = recv(mISDNport->b_sock[i], data, sizeof(data), 0);
			if (len >= (int)MISDN_HEADER_LEN)
			{
				work = 1;
				/* b-message */
				switch(hh->prim)
				{
					/* we don't care about confirms, we use rx data to sync tx */
					case PH_DATA_CNF:
					//case DL_DATA_CNF:
					break;

					/* we receive audio data, we respond to it AND we send tones */
					case PH_DATA_IND:
					case DL_DATA_IND:
					PDEBUG("got B-channel data, this should not happen all the time. (just a few until DSP release tx-data are ok)\n");
					break;

					case PH_CONTROL_IND:
					break;

					case PH_ACTIVATE_IND:
					case DL_ESTABLISH_IND:
					case PH_ACTIVATE_CNF:
					case DL_ESTABLISH_CNF:
					PDEBUG("DL_ESTABLISH confirm: bchannel is now activated (port %d index %i).\n", mISDNport->portnum, i);
					mISDNport->b_state[i] = B_STATE_ACTIVE;
					bchannel_activate(mISDNport, i);
					break;

					case PH_DEACTIVATE_IND:
					case DL_RELEASE_IND:
					case PH_DEACTIVATE_CNF:
					case DL_RELEASE_CNF:
					PDEBUG("DL_RELEASE confirm: bchannel is now de-activated (port %d index %i).\n", mISDNport->portnum, i);
					mISDNport->b_state[i] = B_STATE_IDLE;
					break;

					default:
					PDEBUG("unknown bchannel data (port %d index %i).\n", mISDNport->portnum, i);
				}
			}
			i++;
		}

		mISDNport = mISDNport->next;
	} 
	return(work);
}


/*
 * global function to add a new card (port)
 */
struct mISDNport *mISDN_port_open(int port, int nt_mode)
{
	int ret;
	struct mISDNhead hh;
	struct mISDNport *mISDNport, **mISDNportp, *mISDNport_prev;
	int i, cnt;
	int bri = 0, pri = 0, pots = 0;
	int nt = 0, te = 0;

	struct mISDN_devinfo devinfo;
	unsigned long on = 1;
	struct sockaddr_mISDN addr;

	/* query port's requirements */
	ret = ioctl(mISDNsocket, IMGETCOUNT, &cnt);
	if (ret < 0)
	{
		fprintf(stderr, "Cannot get number of mISDN devices. (ioctl IMGETCOUNT failed ret=%d)\n", ret);
		return(NULL);
	}
	if (cnt <= 0)
	{
		fprintf(stderr, "Found no card. Please be sure to load card drivers.\n");
		return(NULL);
	}
	if (port>cnt || port<1)
	{
		fprintf(stderr, "Port (%d) given is out of existing port range (%d-%d)\n", port, 1, cnt);
		return(NULL);
	}
	devinfo.id = port - 1;
	ret = ioctl(mISDNsocket, IMGETDEVINFO, &devinfo);
	if (ret < 0)
	{
		fprintf(stderr, "Cannot get device information for port %d. (ioctl IMGETDEVINFO failed ret=%d)\n", i, ret);
		return(NULL);
	}
	/* output the port info */
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
	if (bri && pri)
	{
		fprintf(stderr, "Port %d supports BRI and PRI?? What kind of controller is that?. (Can't use this!)\n", port);
		return(NULL);
	}
	if (pots && !bri && !pri)
	{
		fprintf(stderr, "Port %d supports POTS, LCR does not!\n", port);
		return(NULL);
	}
	if (!bri && !pri)
	{
		fprintf(stderr, "Port %d does not support BRI nor PRI!\n", port);
		return(NULL);
	}
	if (!nt && !te)
	{
		fprintf(stderr, "Port %d does not support NT-mode nor TE-mode!\n", port);
		return(NULL);
	}
	if (!nt && nt_mode)
	{
		fprintf(stderr, "Port %d does not support NT-mode as requested!\n", port);
		return(NULL);
	}
	if (!te && !nt_mode)
	{
		fprintf(stderr, "Port %d does not support TE-mode as requested!\n", port);
		return(NULL);
	}

	/* add mISDNport structure */
	mISDNport = mISDNport_first;
	mISDNportp = &mISDNport_first;
	mISDNport_prev = NULL;
	while(mISDNport)
	{
		mISDNport_prev=mISDNport;
		mISDNportp = &mISDNport->next;
		mISDNport = mISDNport->next;
	}
	mISDNport = (struct mISDNport *)calloc(1, sizeof(struct mISDNport));
	if (!mISDNport)
	{
		fprintf(stderr, "Cannot alloc mISDNport structure\n");
		return(NULL);
	}
	memset(mISDNport, 0, sizeof(mISDNport));
	*mISDNportp = mISDNport;
	mISDNport->prev = mISDNport_prev;

	/* allocate ressources of port */
	mISDNport->count = portcount++;
	mISDNport->portnum = port;
	mISDNport->b_num = devinfo.nrbchan;
	mISDNport->ntmode = nt_mode;
	mISDNport->pri = pri;

	/* open dchannel */
	if (nt_mode)
		mISDNport->d_sock = socket(PF_ISDN, SOCK_DGRAM, pri?ISDN_P_NT_E1:ISDN_P_NT_S0);
	else
		mISDNport->d_sock = socket(PF_ISDN, SOCK_DGRAM, pri?ISDN_P_TE_E1:ISDN_P_TE_S0);
	if (mISDNport->d_sock < 0)
	{
		return(NULL);
	}
	/* set nonblocking io */
	ret = ioctl(mISDNport->d_sock, FIONBIO, &on);
	if (ret < 0)
	{
		fprintf(stderr, "Error: Failed to set dchannel-socket into nonblocking IO\n");
		return(NULL);
	}
	/* bind socket to dchannel */
	memset(&addr, 0, sizeof(addr));
	addr.family = AF_ISDN;
	addr.dev = mISDNport->portnum-1;
	addr.channel = 0;
	ret = bind(mISDNport->d_sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		fprintf(stderr, "Error: Failed to bind dchannel-socket.\n");
		return(NULL);
	}
	PDEBUG("Port %d (%s) opened with %d b-channels.\n", port, devinfo.name, mISDNport->b_num);

	/* open bchannels */
	i = 0;
	while(i < mISDNport->b_num)
	{
		mISDNport->b_sock[i] = -1;
		i++;
	}
	i = 0;
	while(i < mISDNport->b_num)
	{
		mISDNport->b_sock[i] = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_B_L2DSP);
		if (mISDNport->b_sock[i] < 0)
		{
			fprintf(stderr, "Error: Failed to open bchannel-socket for index %d with mISDN-DSP layer. Did you load mISDNdsp.ko?\n", i);
			return(NULL);
		}
		/* set nonblocking io */
		ret = ioctl(mISDNport->b_sock[i], FIONBIO, &on);
		if (ret < 0)
		{
			fprintf(stderr, "Error: Failed to set bchannel-socket index %d into nonblocking IO\n", i);
			return(NULL);
		}
		/* bind socket to bchannel */
		memset(&addr, 0, sizeof(addr));
		addr.family = AF_ISDN;
		addr.dev = mISDNport->portnum-1;
		addr.channel = i+1+(i>=15);
		ret = bind(mISDNport->b_sock[i], (struct sockaddr *)&addr, sizeof(addr));
		if (ret < 0)
		{
			fprintf(stderr, "Error: Failed to bind bchannel-socket for index %d with mISDN-DSP layer. Did you load mISDNdsp.ko?\n", i);
			return(NULL);
		}
		i++;
	}

	/* try to activate link layer 1 */
	hh.prim = PH_ACTIVATE_REQ; 
	hh.id = 0;
	ret = sendto(mISDNport->d_sock, &hh, MISDN_HEADER_LEN, 0, NULL, 0);
	if (ret <= 0)
		fprintf(stderr, "Failed to send to socket %d of port %d\n", mISDNport->d_sock, mISDNport->portnum);
	/* initially, we assume that the link is down */
	mISDNport->l1link = 0;

	PDEBUG("using 'mISDN_dsp.o' module\n");
	printf("Port %d (%s) %s %s %d b-channels\n", mISDNport->portnum, devinfo.name, (mISDNport->ntmode)?"NT-mode":"TE-mode", pri?"PRI":"BRI", mISDNport->b_num);
	return(mISDNport);
}


/*
 * global function to free ALL cards (ports)
 */
void mISDN_port_close(void)
{
	struct mISDNport *mISDNport, *mISDNporttemp;
	int i;

	/* free all ports */
	mISDNport = mISDNport_first;
	while(mISDNport)
	{
		i = 0;
		while(i < mISDNport->b_num)
		{
			bchannel_deactivate(mISDNport, i);
			PDEBUG("freeing %s port %d bchannel (index %d).\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, i);
			if (mISDNport->b_sock[i])
				close(mISDNport->b_sock[i]);
			i++;
		}

		PDEBUG("freeing d-stack.\n");
		if (mISDNport->d_sock)
			close(mISDNport->d_sock);

		mISDNporttemp = mISDNport;
		mISDNport = mISDNport->next;
		memset(mISDNporttemp, 0, sizeof(struct mISDNport));
		free(mISDNporttemp);
	}
	mISDNport_first = NULL;
}


/*
 * main routine and loop
 */
int main(int argc, char *argv[])
{
	struct mISDNport *mISDNport_a, *mISDNport_b;
	int i, j, nt_a, nt_b;
	int forking = 0;

	dsp_pid = getpid();

	if (argc <= 1)
	{
		usage:
		printf("Usage: %s [--<option> [...]] te|nt <port a> te|nt <port b> \\\n"
			" [te|nt <port c> te|nt <port d> [...]]\n\n", argv[0]);
		printf("Example: %s --traffic te 1 nt 2 # bridges port 1 (TE-mode) with port 2 (NT-mode)\n", argv[0]);
		printf("Bridges given pairs of ports. The number of given ports must be even.\n");
		printf("Each pair of ports must be the same interface size (equal channel number).\n");
		printf("Both ports may have same mode, e.g. 'te', to bridge ISDN leased line.\n");
		printf("Also bridging a card to ISDN over IP tunnel is possible. (L1oIP)\n");
		printf("Note: .\n");
		printf("--fork will make a daemon fork.\n");
		printf("--traffic will show D-channel traffic.\n");
		printf("--debug will show debug info.\n");
		return(0);
	}
	if (strstr("help", argv[1]))
		goto usage;

	/* try to open raw socket to check kernel */
	mISDNsocket = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (mISDNsocket < 0)
	{
		fprintf(stderr, "Cannot open mISDN due to '%s'. (Does your Kernel support socket based mISDN?)\n", strerror(errno));
		return(mISDNsocket);
	}

	/* read options and ports */
	i = 1;
	while (i < argc)
	{
		usleep(200000);
		if (!strcmp(argv[i], "--traffic"))
		{
			traffic = 1;
			i++;
			continue;
		}
		if (!strcmp(argv[i], "--fork"))
		{
			forking = 1;
			i++;
			continue;
		}
		if (!strcmp(argv[i], "--debug"))
		{
			debug = 1;
			i++;
			continue;
		}

		/* get mode a */
		if (!strcasecmp(argv[i], "te"))
			nt_a = 0;
		else if (!strcasecmp(argv[i], "nt"))
			nt_a = 1;
		else {
			fprintf(stderr, "Expecting 'te' or 'nt' keyword for argument #%d\n\n", i);
			goto error;
		}
		i++; // first port
		if (i == argc)
		{
			fprintf(stderr, "Expecting port a number after given mode.\n\n");
			goto error;
		}

		/* open port a */
		mISDNport_a = mISDN_port_open(strtol(argv[i], NULL, 0), nt_a);
		if (!mISDNport_a)
			goto error;
		printf("port A: #%d %s, %d b-channels\n", mISDNport_a->portnum, (mISDNport_a->ntmode)?"NT-mode":"TE-mode", mISDNport_a->b_num);
		i++; // second mode
		if (i == argc)
		{
			fprintf(stderr, "The number of ports given are not even.\nYou may only bridge two or more pairs of ports.\n\n");
			goto error;
		}
		
		/* get mode b */
		if (!strcasecmp(argv[i], "te"))
			nt_b = 0;
		else if (!strcasecmp(argv[i], "nt"))
			nt_b = 1;
		else {
			fprintf(stderr, "Expecting 'te' or 'nt' keyword for argument #%d\n\n", i);
			goto error;
		}
		i++; // second port
		if (i == argc)
		{
			fprintf(stderr, "Expecting port b number after given mode.\n\n");
			goto error;
		}

		/* open port b */
		mISDNport_b = mISDN_port_open(strtol(argv[i], NULL, 0), nt_b);
		if (!mISDNport_b)
			goto error;
		printf("port B: #%d %s, %d b-channels\n", mISDNport_b->portnum, (mISDNport_b->ntmode)?"NT-mode":"TE-mode", mISDNport_b->b_num);
		i++; // next port / arg

		if (mISDNport_a->b_num != mISDNport_b->b_num)
		{
			fprintf(stderr, "The pair of ports are not compatible for bridging.\n");
			fprintf(stderr, "The number ob B-channels are different: port(%d)=%d, port(%d)=%d\n", mISDNport_a->portnum, mISDNport_a->b_num, mISDNport_b->portnum, mISDNport_b->b_num);

			mISDN_port_close();
			goto error;
		}

		/* opening and bridge each pair of bchannels */
		j = 0;
		while(j < mISDNport_a->b_num)
		{
			bchannel_activate(mISDNport_a, j);
			while(mISDN_handler())
				;
			j++;
		}
		j = 0;
		while(j < mISDNport_b->b_num)
		{
			bchannel_activate(mISDNport_b, j);
			while(mISDN_handler())
				;
			j++;
		}
	}

	printf("%s now started\n",argv[0]);

	/* forking */
	if (forking) {
		pid_t pid;

		/* do daemon fork */
		pid = fork();

		if (pid < 0)
		{
			fprintf(stderr, "Cannot fork!\n");
			goto free;
		}
		if (pid != 0)
		{
			exit(0);
		}
		usleep(200000);
		printf("\n");
		
		/* do second fork */
		pid = fork();

		if (pid < 0)
		{
			fprintf(stderr, "Cannot fork!\n");
			goto free;
		}
		if (pid != 0)
		{
			printf("PBX: Starting daemon.\n");
			exit(0);
		}
		nooutput = 1;
	}
	
	/* signal handlers */	
	signal(SIGINT,sighandler);
	signal(SIGHUP,sighandler);
	signal(SIGTERM,sighandler);
	signal(SIGPIPE,sighandler);

	while(!quit)
	{
		//mISDNport_a->l1link = 1;
		if (!mISDN_handler())
			usleep(30000);
	}

	/* remove signal handler */
	signal(SIGINT,SIG_DFL);
	signal(SIGHUP,SIG_DFL);
	signal(SIGTERM,SIG_DFL);
	signal(SIGPIPE,SIG_DFL);

free:
	mISDN_port_close();
	close(mISDNsocket);
	return(0);

	error:
	mISDN_port_close();
	close(mISDNsocket);
	return(-1);
}


