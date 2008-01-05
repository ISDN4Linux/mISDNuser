/*****************************************************************************\
**                                                                           **
** isdnbridge                                                                **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg  (GPL)                                       **
**                                                                           **
** user space utility to bridge two mISDN ports.                             **
**                                                                           **
\*****************************************************************************/ 


#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
//#include <poll.h>
#include <errno.h>
//#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mISDNlib.h>

#define ISDN_PID_L4_B_USER 0x440000ff

/* used for udevice */
int entity = 0;

/* the device handler and port list */
int mISDNdevice = -1;
int portcount = 0; /* counts all open ports for finding pair */

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
	int upper_id; /* id to transfer data down */
	int lower_id; /* id to transfer data up */
	int d_stid;
	int b_num; /* number of ports */
	int b_stid[256];
	int b_addr[256];
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
static void ph_control(unsigned long b_addr, int c1, int c2)
{
	unsigned char buffer[mISDN_HEADER_LEN+sizeof(int)+sizeof(int)];
	iframe_t *ctrl = (iframe_t *)buffer; 
	unsigned long *d = (unsigned long *)&ctrl->data.p;

	ctrl->prim = PH_CONTROL | REQUEST;
	ctrl->addr = b_addr | FLG_MSG_DOWN;
	ctrl->dinfo = 0;
	ctrl->len = sizeof(unsigned long)*2;
	*d++ = c1;
	*d++ = c2;
	mISDN_write(mISDNdevice, ctrl, mISDN_HEADER_LEN+ctrl->len, TIMEOUT_1SEC);
}

void ph_control_block(unsigned long b_addr, int c1, void *c2, int c2_len)
{
	unsigned char buffer[mISDN_HEADER_LEN+sizeof(int)+c2_len];
	iframe_t *ctrl = (iframe_t *)buffer;
	unsigned long *d = (unsigned long *)&ctrl->data.p;

	ctrl->prim = PH_CONTROL | REQUEST;
	ctrl->addr = b_addr | FLG_MSG_DOWN;
	ctrl->dinfo = 0;
	ctrl->len = sizeof(unsigned long)*2;
	*d++ = c1;
	memcpy(d, c2, c2_len);
	mISDN_write(mISDNdevice, ctrl, mISDN_HEADER_LEN+ctrl->len, TIMEOUT_1SEC);
}


/*
 * activate / deactivate bchannel
 */
static void bchannel_activate(struct mISDNport *mISDNport, int i)
{
	iframe_t act;

	/* we must activate if we are deactivated */
	if (mISDNport->b_state[i] == B_STATE_IDLE)
	{
		/* activate bchannel */
		PDEBUG("activating bchannel (index %d), because currently idle (address 0x%x).\n", i, mISDNport->b_addr[i]);
		act.prim = DL_ESTABLISH | REQUEST; 
		act.addr = mISDNport->b_addr[i] | FLG_MSG_DOWN;
		act.dinfo = 0;
		act.len = 0;
		mISDN_write(mISDNdevice, &act, mISDN_HEADER_LEN+act.len, TIMEOUT_1SEC);
		mISDNport->b_state[i] = B_STATE_ACTIVATING;
		return;
	}

	/* if we are active, we configure our channel */
	if (mISDNport->b_state[i] == B_STATE_ACTIVE)
	{
		/* it is an error if this channel is not associated with a port object */
		PDEBUG("during activation, we add conference to %d.\n", ((mISDNport->count&(~1)) << 23) + (i<<16) + dsp_pid);
		ph_control(mISDNport->b_addr[i], CMX_CONF_JOIN, ((mISDNport->count&(~1)) << 23) + (i<<16) + dsp_pid);
		PDEBUG("during activation, we set rxoff.\n");
		ph_control(mISDNport->b_addr[i], CMX_RECEIVE_OFF, 0);
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
	iframe_t dact;

	if (mISDNport->b_state[i] == B_STATE_ACTIVE)
	{
		ph_control(mISDNport->b_addr[i], CMX_CONF_SPLIT, 0);
		ph_control(mISDNport->b_addr[i], CMX_RECEIVE_ON, 0);
		/* deactivate bchannel */
		PDEBUG("deactivating bchannel (index %d), because currently active.\n", i);
		dact.prim = DL_RELEASE | REQUEST; 
		dact.addr = mISDNport->b_addr[i] | FLG_MSG_DOWN;
		dact.dinfo = 0;
		dact.len = 0;
		mISDN_write(mISDNdevice, &dact, mISDN_HEADER_LEN+dact.len, TIMEOUT_1SEC);
		mISDNport->b_state[i] = B_STATE_DEACTIVATING;
		return;
	}
}

/*
 * main loop for processing messages from mISDN device
 */
int mISDN_handler(void)
{
	iframe_t *frm;
	struct mISDNport *mISDNport;
	int i;
	unsigned char data[2048];
	int len;

	/* get message from kernel */
	len = mISDN_read(mISDNdevice, data, sizeof(data), 0);
	if (len < 0)
	{
		if (errno == EAGAIN)
			return(0);
		fprintf(stderr, "FATAL ERROR: failed to do mISDN_read()\n");
		exit(-1);
	}
	if (!len)
	{
		return(0);
	}
	frm = (iframe_t *)data;

	/* global prim */
	switch(frm->prim)
	{
		case MGR_INITTIMER | CONFIRM:
		case MGR_ADDTIMER | CONFIRM:
		case MGR_DELTIMER | CONFIRM:
		case MGR_REMOVETIMER | CONFIRM:
		return(1);
	}

	/* find the port */
	mISDNport = mISDNport_first;
	while(mISDNport)
	{
		if ((frm->addr&STACK_ID_MASK) == (unsigned int)(mISDNport->upper_id&STACK_ID_MASK))
		{
			/* d-message */
			switch(frm->prim)
			{
				case MGR_SHORTSTATUS | INDICATION:
				case MGR_SHORTSTATUS | CONFIRM:
				switch(frm->dinfo) {
					case SSTATUS_L1_ACTIVATED:
					PDEBUG("Received SSTATUS_L1_ACTIVATED for port %d.\n", mISDNport->portnum);
					goto ss_act;
					case SSTATUS_L1_DEACTIVATED:
					PDEBUG("Received SSTATUS_L1_DEACTIVATED for port %d.\n", mISDNport->portnum);
					goto ss_deact;
				}
				break;

				case PH_ACTIVATE | CONFIRM:
				case PH_ACTIVATE | INDICATION:
				PDEBUG("Received PH_ACTIVATE for port %d.\n", mISDNport->portnum);
				ss_act:
				if (!mISDNport->l1link)
				{
					mISDNport->l1link = 1;
					show_state(mISDNport);
				}
				if (mISDNport->que_len)
				{
					PDEBUG("Data in que, due to inactive link on port %d.\n", mISDNport->portnum);
					mISDN_write(mISDNdevice, mISDNport->que_frm, mISDNport->que_len, TIMEOUT_1SEC);
					mISDNport->que_len = 0;
				}
				break;

				case PH_DEACTIVATE | CONFIRM:
				case PH_DEACTIVATE | INDICATION:
				PDEBUG("Received PH_DEACTIVATE for port %d.\n", mISDNport->portnum);
				ss_deact:
				if (mISDNport->l1link)
				{
					mISDNport->l1link = 0;
					show_state(mISDNport);
				}
				mISDNport->que_len = 0;
				break;

				case PH_CONTROL | CONFIRM:
				case PH_CONTROL | INDICATION:
				PDEBUG("Received PH_CONTROL for port %d.\n", mISDNport->portnum);
				break;

				case PH_DATA | INDICATION:
				if (traffic)
					show_traffic(mISDNport, data + mISDN_HEADER_LEN, frm->len);
				PDEBUG("GOT data from %s port %d prim 0x%x dinfo 0x%x addr 0x%x\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, frm->prim, frm->dinfo, frm->addr);
				if (mISDNport->count & 1)
				{
					if (mISDNport->prev == NULL)
					{
						printf("soft error, no prev where expected.\n");
						exit (0);
					}
					/* sending to previous port */
					frm->prim = PH_DATA | REQUEST;
					frm->addr = mISDNport->prev->upper_id | FLG_MSG_DOWN;
				PDEBUG("sending to %s port %d prim 0x%x dinfo 0x%x addr 0x%x\n", (mISDNport->prev->ntmode)?"NT":"TE", mISDNport->prev->portnum, frm->prim, frm->dinfo, frm->addr);
					if (mISDNport->prev->l1link)
						mISDN_write(mISDNdevice, frm, mISDN_HEADER_LEN+frm->len, TIMEOUT_1SEC);
					else {
						PDEBUG("layer 1 is down, so we queue and activate link.\n");
						memcpy(mISDNport->prev->que_frm, frm, len);
						mISDNport->prev->que_len = len;
						frm->prim = PH_ACTIVATE | REQUEST; 
						frm->dinfo = 0;
						frm->len = 0;
						mISDN_write(mISDNdevice, frm, mISDN_HEADER_LEN+frm->len, TIMEOUT_1SEC);
					}
				} else {

					if (mISDNport->next == NULL)
					{
						printf("soft error, no next where expected.\n");
						exit (0);
					}
					/* sending to next port */
					frm->prim = PH_DATA | REQUEST; 
					frm->addr = mISDNport->next->upper_id | FLG_MSG_DOWN;
				PDEBUG("sending to %s port %d prim 0x%x dinfo 0x%x addr 0x%x\n", (mISDNport->next->ntmode)?"NT":"TE", mISDNport->next->portnum, frm->prim, frm->dinfo, frm->addr);
					if (mISDNport->next->l1link)
						mISDN_write(mISDNdevice, frm, mISDN_HEADER_LEN+frm->len, TIMEOUT_1SEC);
					else {
						PDEBUG("layer 1 is down, so we queue and activate link.\n");
						memcpy(mISDNport->next->que_frm, frm, len);
						mISDNport->next->que_len = len;
						frm->prim = PH_ACTIVATE | REQUEST; 
						frm->dinfo = 0;
						frm->len = 0;
						mISDN_write(mISDNdevice, frm, mISDN_HEADER_LEN+frm->len, TIMEOUT_1SEC);
					}
				}
				break;

				case PH_DATA | CONFIRM:
				//PDEBUG("GOT confirm from %s port %d prim 0x%x dinfo 0x%x addr 0x%x\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, frm->prim, frm->dinfo, frm->addr);
				break;

				case PH_DATA | REQUEST:
				PDEBUG("GOT strange PH_DATA REQUEST from %s port %d prim 0x%x dinfo 0x%x addr 0x%x\n", (mISDNport->ntmode)?"NT":"TE", mISDNport->portnum, frm->prim, frm->dinfo, frm->addr);
				break;

				default:
				break;
			}
			break;
		}
//PDEBUG("flg:%d upper_id=%x addr=%x\n", (frm->addr&FLG_CHILD_STACK), (mISDNport->b_addr[0])&(~IF_CHILDMASK), (frm->addr)&(~IF_CHILDMASK));
		/* check if child, and if parent stack match */
		if ((frm->addr&FLG_CHILD_STACK) && (((unsigned int)(mISDNport->b_addr[0])&(~CHILD_ID_MASK)&STACK_ID_MASK) == ((frm->addr)&(~CHILD_ID_MASK)&STACK_ID_MASK)))
		{
			/* b-message */
			switch(frm->prim)
			{
				/* we don't care about confirms, we use rx data to sync tx */
				case PH_DATA | CONFIRM:
				case DL_DATA | CONFIRM:
				break;

				/* we receive audio data, we respond to it AND we send tones */
				case PH_DATA | INDICATION:
				case DL_DATA | INDICATION:
				case PH_CONTROL | INDICATION:
				i = 0;
				while(i < mISDNport->b_num)
				{
					if ((unsigned int)(mISDNport->b_addr[i]&STACK_ID_MASK) == (frm->addr&STACK_ID_MASK))
						break;
					i++;
				}
				if (i == mISDNport->b_num)
				{
					fprintf(stderr, "unhandled b-message (address 0x%x).\n", frm->addr);
					break;
				}
				PDEBUG("got B-channel data, this should not happen all the time. (just a few until cmx release tx-data are ok)\n");
				break;

				case PH_ACTIVATE | INDICATION:
				case DL_ESTABLISH | INDICATION:
				case PH_ACTIVATE | CONFIRM:
				case DL_ESTABLISH | CONFIRM:
				PDEBUG("DL_ESTABLISH confirm: bchannel is now activated (address 0x%x).\n", frm->addr);
				i = 0;
				while(i < mISDNport->b_num)
				{
					if ((unsigned int)(mISDNport->b_addr[i]&STACK_ID_MASK) == (frm->addr&STACK_ID_MASK))
						break;
					i++;
				}
				if (i == mISDNport->b_num)
				{
					fprintf(stderr, "unhandled b-establish (address 0x%x).\n", frm->addr);
					break;
				}
				mISDNport->b_state[i] = B_STATE_ACTIVE;
				bchannel_activate(mISDNport, i);
				break;

				case PH_DEACTIVATE | INDICATION:
				case DL_RELEASE | INDICATION:
				case PH_DEACTIVATE | CONFIRM:
				case DL_RELEASE | CONFIRM:
				PDEBUG("DL_RELEASE confirm: bchannel is now de-activated (address 0x%x).\n", frm->addr);
				i = 0;
				while(i < mISDNport->b_num)
				{
					if ((unsigned int)(mISDNport->b_addr[i]&STACK_ID_MASK) == (frm->addr&STACK_ID_MASK))
						break;
					i++;
				}
				if (i == mISDNport->b_num)
				{
					fprintf(stderr, "unhandled b-release (address 0x%x).\n", frm->addr);
					break;
				}
				mISDNport->b_state[i] = B_STATE_IDLE;
				break;
			}
			break;
		}

		mISDNport = mISDNport->next;
	} 
	if (!mISDNport)
	{
		if (frm->prim == (MGR_TIMER | INDICATION))
			fprintf(stderr, "unhandled timer indication message: prim(0x%x) addr(0x%x) len(%d)\n", frm->prim, frm->addr, len);
		else
			fprintf(stderr, "unhandled message: prim(0x%x) addr(0x%x) len(%d)\n", frm->prim, frm->addr, len);
	}

	return(1);
}


/*
 * global function to add a new card (port)
 */
struct mISDNport *mISDN_port_open(int port)
{
	int ret;
	unsigned char buff[1025];
	iframe_t *frm = (iframe_t *)buff;
	stack_info_t *stinf;
	struct mISDNport *mISDNport, **mISDNportp, *mISDNport_prev;
	int i, cnt;
	layer_info_t li;
//	interface_info_t ii;
	mISDN_pid_t pid;
	int pri = 0;
	int nt = 0;
	iframe_t dact;

	/* open mISDNdevice if not already open */
	if (mISDNdevice < 0)
	{
		ret = mISDN_open();
		if (ret < 0)
		{
			fprintf(stderr, "cannot open mISDN device ret=%d errno=%d (%s) Check for mISDN modules!\nAlso did you create \"/dev/mISDN\"? Do: \"mknod /dev/mISDN c 46 0\"\n", ret, errno, strerror(errno));
			return(0);
		}
		mISDNdevice = ret;
		PDEBUG("mISDN device opened.\n");

		/* create entity for layer 3 TE-mode */
		mISDN_write_frame(mISDNdevice, buff, 0, MGR_NEWENTITY | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
		ret = mISDN_read_frame(mISDNdevice, frm, sizeof(iframe_t), 0, MGR_NEWENTITY | CONFIRM, TIMEOUT_1SEC);
		if (ret < (int)mISDN_HEADER_LEN)
		{
			noentity:
			fprintf(stderr, "cannot request MGR_NEWENTITY from mISDN. Exitting due to software bug.");
			exit(-1);
		}
		entity = frm->dinfo & 0xffff;
		if (!entity)
			goto noentity;
		PDEBUG("our entity for l3-processes is %d.\n", entity);
	}

	/* query port's requirements */
	cnt = mISDN_get_stack_count(mISDNdevice);
	if (cnt <= 0)
	{
		fprintf(stderr, "Found no card. Please be sure to load card drivers.\n");
		return(0);
	}
	if (port>cnt || port<1)
	{
		fprintf(stderr, "Port (%d) given is out of existing port range (%d-%d)\n", port, 1, cnt);
		return(0);
	}
	ret = mISDN_get_stack_info(mISDNdevice, port, buff, sizeof(buff));
	if (ret < 0)
	{
		fprintf(stderr, "Cannot get stack info for port %d (ret=%d)\n", port, ret);
		return(0);
	}
	stinf = (stack_info_t *)&frm->data.p;
	switch(stinf->pid.protocol[0] & ~ISDN_PID_FEATURE_MASK)
	{
		case ISDN_PID_L0_TE_S0:
		PDEBUG("TE-mode BRI S/T interface line\n");
		break;
		case ISDN_PID_L0_NT_S0:
		PDEBUG("NT-mode BRI S/T interface port\n");
		nt = 1;
		break;
		case ISDN_PID_L0_TE_E1:
		PDEBUG("TE-mode PRI E1  interface line\n");
		pri = 1;
		break;
		case ISDN_PID_L0_NT_E1:
		PDEBUG("LT-mode PRI E1  interface port\n");
		pri = 1;
		nt = 1;
		break;
		default:
		fprintf(stderr, "unknown port(%d) type 0x%08x\n", port, stinf->pid.protocol[0]);
		return(0);
	}
	if (stinf->pid.protocol[1] == 0)
	{
		fprintf(stderr, "Given port %d: Missing layer 1 protocol.\n", port);
		return(0);
	}
	if (stinf->pid.protocol[2])
	{
		fprintf(stderr, "Given port %d: Layer 2 protocol 0x%08x is detected, but not allowed for bridging layer 1.\n", port, stinf->pid.protocol[2]);
		return(0);
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
		return(0);
	}
	memset(mISDNport, 0, sizeof(mISDNport));
	*mISDNportp = mISDNport;
	mISDNport->prev = mISDNport_prev;

	/* allocate ressources of port */
	mISDNport->count = portcount++;
	mISDNport->portnum = port;
	mISDNport->ntmode = nt;
	mISDNport->pri = pri;
	mISDNport->d_stid = stinf->id;
	PDEBUG("d_stid = 0x%x.\n", mISDNport->d_stid);
	mISDNport->b_num = stinf->childcnt;
	PDEBUG("Port has %d b-channels.\n", mISDNport->b_num);
	i = 0;
	while(i < stinf->childcnt)
	{
		mISDNport->b_stid[i] = stinf->child[i];
		PDEBUG("b_stid[%d] = 0x%x.\n", i, mISDNport->b_stid[i]);
		i++;
	}
	memset(&li, 0, sizeof(li));
	strcpy(&li.name[0], "bridge l2");
	li.object_id = -1;
	li.extentions = 0;
	li.pid.protocol[2] = (nt)?ISDN_PID_L2_LAPD_NET:ISDN_PID_L2_LAPD;
	li.pid.layermask = ISDN_LAYER(2);
	li.st = mISDNport->d_stid;
	ret = mISDN_new_layer(mISDNdevice, &li);
	if (ret)
	{
		fprintf(stderr, "Cannot add layer 2 of port %d (ret %d)\n", port, ret);
		return(0);
	}
	mISDNport->upper_id = li.id;
	ret = mISDN_register_layer(mISDNdevice, mISDNport->d_stid, mISDNport->upper_id);
	if (ret)
	{
		fprintf(stderr, "Cannot register layer 2 of port %d\n", port);
		return(0);
	}
	mISDNport->lower_id = mISDN_get_layerid(mISDNdevice, mISDNport->d_stid, 1);
	if (mISDNport->lower_id < 0)
	{
		fprintf(stderr, "Cannot get layer(1) id of port %d\n", port);
		return(0);
	}
	mISDNport->upper_id = mISDN_get_layerid(mISDNdevice, mISDNport->d_stid, 2);
	if (mISDNport->upper_id < 0)
	{
		fprintf(stderr, "Cannot get layer(2) id of port %d\n", port);
		return(0);
	}
	PDEBUG("Layer 2 of port %d added.\n", port);

	/* try to activate link layer 1 */
	{
		iframe_t act;
		/* L1 */
		PDEBUG("sending PH_ACTIVATE to port %d.\n", port);
		act.prim = PH_ACTIVATE | REQUEST; 
		act.addr = mISDNport->upper_id | FLG_MSG_DOWN;
		act.dinfo = 0;
		act.len = 0;
		mISDN_write(mISDNdevice, &act, mISDN_HEADER_LEN+act.len, TIMEOUT_1SEC);
	}
	/* initially, we assume that the link is down */
	mISDNport->l1link = 0;

	PDEBUG("using 'mISDN_dsp.o' module\n");

	/* add all bchannel layers */
	i = 0;
	while(i < mISDNport->b_num)
	{
		mISDNport->b_state[i] = B_STATE_IDLE;
		/* create new layer */
		PDEBUG("creating bchannel %d (index %d).\n" , i+1+(i>=15), i);
		memset(&li, 0, sizeof(li));
		memset(&pid, 0, sizeof(pid));
		li.object_id = -1;
		li.extentions = 0;
		li.st = mISDNport->b_stid[i];
		strcpy(li.name, "B L4");
		li.pid.layermask = ISDN_LAYER((4));
		li.pid.protocol[4] = ISDN_PID_L4_B_USER;
		ret = mISDN_new_layer(mISDNdevice, &li);
		if (ret)
		{
			failed_new_layer:
			fprintf(stderr, "mISDN_new_layer() failed to add bchannel %d (index %d)\n", i+1+(i>=15), i);
			return(0);
		}
		mISDNport->b_addr[i] = li.id;
		if (!li.id)
		{
			goto failed_new_layer;
		}
		PDEBUG("new layer (b_addr=0x%x)\n", mISDNport->b_addr[i]);

		/* create new stack */
		pid.protocol[1] = ISDN_PID_L1_B_64TRANS;
		pid.protocol[2] = ISDN_PID_L2_B_TRANS;
		pid.protocol[3] = ISDN_PID_L3_B_DSP;
		pid.protocol[4] = ISDN_PID_L4_B_USER;
		pid.layermask = ISDN_LAYER((1)) | ISDN_LAYER((2)) | ISDN_LAYER((3)) | ISDN_LAYER((4));
		ret = mISDN_set_stack(mISDNdevice, mISDNport->b_stid[i], &pid);
		if (ret)
		{
			stack_error:
			fprintf(stderr, "mISDN_set_stack() failed (ret=%d) to add bchannel (index %d) stid=0x%x\n", ret, i, mISDNport->b_stid[i]);
			mISDN_write_frame(mISDNdevice, buff, mISDNport->b_addr[i], MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
			mISDNport->b_addr[i] = 0;
			return(0);
		}
		ret = mISDN_get_setstack_ind(mISDNdevice, mISDNport->b_addr[i]);
		if (ret)
			goto stack_error;

		/* get layer id */
		mISDNport->b_addr[i] = mISDN_get_layerid(mISDNdevice, mISDNport->b_stid[i], 4);
		if (!mISDNport->b_addr[i])
			goto stack_error;
		/* deactivate bchannel if already enabled due to crash */
		PDEBUG("deactivating bchannel (index %d) as a precaution.\n", i);
		dact.prim = DL_RELEASE | REQUEST; 
		dact.addr = mISDNport->b_addr[i] | FLG_MSG_DOWN;
		dact.dinfo = 0;
		dact.len = 0;
		mISDN_write(mISDNdevice, &dact, mISDN_HEADER_LEN+dact.len, TIMEOUT_1SEC);

		i++;
	}
	PDEBUG("using port %d %s %d b-channels\n", mISDNport->portnum, (mISDNport->ntmode)?"NT-mode":"TE-mode", mISDNport->b_num);
	return(mISDNport);
}


/*
 * global function to free ALL cards (ports)
 */
void mISDN_port_close(void)
{
	struct mISDNport *mISDNport, *mISDNporttemp;
	unsigned char buf[32];
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
			if (mISDNport->b_stid[i])
			{
				mISDN_clear_stack(mISDNdevice, mISDNport->b_stid[i]);
				if (mISDNport->b_addr[i])
					mISDN_write_frame(mISDNdevice, buf, mISDNport->b_addr[i] | FLG_MSG_DOWN, MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
			}
			i++;
		}

		PDEBUG("freeing d-stack.\n");
		if (mISDNport->d_stid)
		{
//			mISDN_clear_stack(mISDNdevice, mISDNport->d_stid);
			if (mISDNport->lower_id)
				mISDN_write_frame(mISDNdevice, buf, mISDNport->lower_id, MGR_DELLAYER | REQUEST, 0, 0, NULL, TIMEOUT_1SEC);
		}

		mISDNporttemp = mISDNport;
		mISDNport = mISDNport->next;
		memset(mISDNporttemp, 0, sizeof(struct mISDNport));
		free(mISDNporttemp);
	}
	mISDNport_first = NULL;
	
	/* close mISDNdevice */
	if (mISDNdevice >= 0)
	{
		/* free entity */
		mISDN_write_frame(mISDNdevice, buf, 0, MGR_DELENTITY | REQUEST, entity, 0, NULL, TIMEOUT_1SEC);
		/* close device */
		mISDN_close(mISDNdevice);
		mISDNdevice = -1;
		PDEBUG("mISDN device closed.\n");
	}
}


/*
 * main routine and loop
 */
int main(int argc, char *argv[])
{
	struct mISDNport *mISDNport_a, *mISDNport_b;
	int i, j;
	int forking = 0;

	if (argc <= 1)
	{
		usage:
		printf("Usage: %s [--<option> [...]] <port a> <port b> [<port a> <port b> [...]]\n\n", argv[0]);
		printf("Bridges given pairs of ports. The number of given ports must be even.\n");
		printf("Each pair of ports must be the same interface size (equal channel number).\n");
		printf("Both ports may have same mode, e.g. TE-mode, to bridge ISDN to leased line.\n");
		printf("Also bridging a card to ISDN over IP tunnel is possible. (L1oIP)\n");
		printf("Note: Ports must have layer 1 only, so layermask must be 0x3 to make it work.\n");
		printf("--fork will make a daemon fork.\n");
		printf("--traffic will show D-channel traffic.\n");
		printf("--debug will show debug info.\n");
		return(0);
	}
	if (strstr("help", argv[1]))
		goto usage;

	/* open mISDN */
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

		/* open port a */
		mISDNport_a = mISDN_port_open(strtol(argv[i], NULL, 0));
		if (!mISDNport_a)
			goto error;
		printf("port A: #%d %s, %d b-channels\n", mISDNport_a->portnum, (mISDNport_a->ntmode)?"NT-mode":"TE-mode", mISDNport_a->b_num);
		i++; // two ports at the same time
		if (i == argc)
		{
			fprintf(stderr, "The number of ports given are not even.\nYou may only bridge two or more pairs of ports.\n\n");
			goto error;
		}
		mISDNport_b = mISDN_port_open(strtol(argv[i], NULL, 0));
		if (!mISDNport_b)
			goto error;
		/* open port b */
		printf("port B: #%d %s, %d b-channels\n", mISDNport_b->portnum, (mISDNport_b->ntmode)?"NT-mode":"TE-mode", mISDNport_b->b_num);
		i++; // two ports at the same time

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
			printf("%s: Starting daemon.\n", argv[0]);
			exit(0);
		}
		nooutput = 1;
	}
	dsp_pid = getpid();
	
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
	return(0);

	error:
	return(-1);
}


