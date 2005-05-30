#include <errno.h>
#include <string.h>
#include "mISDNlib.h"
// #include <stdio.h>

int
mISDN_get_stack_count(int fid)
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, 0, MGR_GETSTACK | REQUEST,
		0, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t), 0,
		MGR_GETSTACK | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret != mISDN_HEADER_LEN) {
		if (ret > 0)
			ret = -EINVAL; 
	} else {
		if (ifr.len)
			ret = ifr.len;
		else
			ret = ifr.dinfo;
	}
	return(ret);
}

int
mISDN_new_stack(int fid, stack_info_t *s_info)
{
	u_char		buf[sizeof(stack_info_t) + mISDN_HEADER_LEN];
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, buf, 0, MGR_NEWSTACK | REQUEST,
		0, sizeof(stack_info_t), s_info, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t), 0, 
		MGR_NEWSTACK | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret == mISDN_HEADER_LEN) {
		if (ifr.len)
			ret = ifr.len;
		else
			ret = ifr.dinfo;
	}
	return(ret);
}

int
mISDN_set_stack(int fid, int stack, mISDN_pid_t *pid)
{
	u_char		buf[sizeof(mISDN_pid_t) + mISDN_HEADER_LEN];
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, buf, stack, MGR_SETSTACK | REQUEST,
		0, sizeof(mISDN_pid_t), pid, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		stack, MGR_SETSTACK | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret == mISDN_HEADER_LEN)
		ret = ifr.len;
	else if (ret>0)
		ret = -EINVAL;
	return(ret);
}

int
mISDN_clear_stack(int fid, int stack)
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, stack, MGR_CLEARSTACK | REQUEST,
		0, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		stack, MGR_CLEARSTACK | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret == mISDN_HEADER_LEN)
		ret = ifr.len;
	else if (ret>0)
		ret = -EINVAL;
	return(ret);
}

int
mISDN_get_stack_info(int fid, int stack, void *info, size_t max_len)
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);

	ret = mISDN_write_frame(fid, &ifr, stack, MGR_GETSTACK | REQUEST,
		0, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, info, max_len,
		stack, MGR_GETSTACK | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret == mISDN_HEADER_LEN)
		ret = ((iframe_t *)info)->len;
	return(ret);
}

void
mISDNprint_stack_info(FILE *file, stack_info_t *s_info)
{
	int i;

	fprintf(file, "stack id %08x\n", s_info->id);
	fprintf(file, "     ext %08x\n", s_info->extentions);
	for(i=0;i<=MAX_LAYER_NR;i++)
		fprintf(file, "   prot%d %08x\n", i, s_info->pid.protocol[i]);
	for(i=0;i<s_info->instcnt;i++)
		fprintf(file, "   inst%d %08x\n", i, s_info->inst[i]);
	fprintf(file, "     mgr %08x\n", s_info->mgr);
	fprintf(file, "  master %08x\n", s_info->master);
	fprintf(file, "   clone %08x\n", s_info->clone);
	for(i=0;i<s_info->childcnt;i++)
		fprintf(file, "  child%d %08x\n", i, s_info->child[i]);
}
