#include <errno.h>
#include <string.h>
#include "mISDNlib.h"

/* State values for l1 state machine (status_info_l1_t state field) */
char *strL1SState[] =
{
	"ST_L1_F2",
	"ST_L1_F3",
	"ST_L1_F4",
	"ST_L1_F5",
	"ST_L1_F6",
	"ST_L1_F7",
	"ST_L1_F8",
};


/* State values for l2 state machine (status_info_l2_t state field) */
char *strL2State[] =
{
	"ST_L2_1",
	"ST_L2_2",
	"ST_L2_3",
	"ST_L2_4",
	"ST_L2_5",
	"ST_L2_6",
	"ST_L2_7",
	"ST_L2_8",
};

int
mISDN_get_status_info(int fid, int id, void *info, size_t max_len)
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, id, MGR_STATUS | REQUEST,
		0, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, info, max_len,
		id, MGR_STATUS | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	return(ret);
}

/* not complete now */

int 
mISDNprint_status(FILE *file, status_info_t *si)
{
	int ret=0;
	status_info_l1_t *si1;
	status_info_l2_t *si2;

	switch(si->typ) {
		case STATUS_INFO_L1:
			si1 = (status_info_l1_t *)si;
			fprintf(file," prot:%x status:%d state:%s Flags:%x\n",
				si1->protocol, si1->status,
				strL1SState[si1->state], si1->Flags);
			break;
		case STATUS_INFO_L2:
			si2 = (status_info_l2_t *)si;
			fprintf(file," prot:%x tei:%d state:%s flag:%x\n",
				si2->protocol, si2->tei,
				strL2State[si2->state], si2->flag);
			break;
		default:
			fprintf(file, "unknown status type %d\n", si->typ);
			break;
	}
	return(ret);
}


