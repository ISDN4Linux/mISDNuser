#include <errno.h>
#include <string.h>
#include "mISDNlib.h"

int
mISDN_get_layerid(int fid, int stack, int layer)
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, stack, MGR_GETLAYERID | REQUEST,
		layer, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		stack, MGR_GETLAYERID | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret != mISDN_HEADER_LEN) {
		if (ret>0)
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
mISDN_new_layer(int fid, layer_info_t *l_info)
{
	unsigned char	buf[sizeof(layer_info_t) + mISDN_HEADER_LEN];
	iframe_t	*ifr = (iframe_t *)buf;
	int		ret;
	u_int		*ip;
	
	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, buf, 0, MGR_NEWLAYER | REQUEST,
		0, sizeof(layer_info_t), l_info, TIMEOUT_1SEC);
//	fprintf(stderr, "%s: wret %d\n", __FUNCTION__, ret);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, ifr, sizeof(layer_info_t) + mISDN_HEADER_LEN,
		0, MGR_NEWLAYER | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
//	fprintf(stderr, "%s: rret %d\n", __FUNCTION__, ret);
	if (ret<0)
		return(ret);
	if (ret < (mISDN_HEADER_LEN + 2*sizeof(int))) {
		if (ret == mISDN_HEADER_LEN)
			ret = ifr->len;
		else if (ret>0)
			ret = -EINVAL;
	} else {
		ret = 0;
		ip = &ifr->data.p;
		l_info->id = *ip++;
		l_info->clone = *ip;
	}
//	fprintf(stderr, "%s: ret %x\n", __FUNCTION__, ret);
	return(ret);
}

int
mISDN_register_layer(int fid, u_int sid, u_int lid) 
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, sid, MGR_REGLAYER | REQUEST, lid,
		0, NULL, TIMEOUT_1SEC);
//	fprintf(stderr, "%s: wret %d\n", __FUNCTION__, ret); 
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		sid, MGR_REGLAYER | CONFIRM, TIMEOUT_1SEC);
//	fprintf(stderr, "%s: rret %d\n", __FUNCTION__, ret);
	if (ret != mISDN_HEADER_LEN) {
		if (ret >= 0)
			ret = -1;
	} else {
		ret = ifr.len;
	}
	return(ret);
}

int
mISDN_unregister_layer(int fid, u_int sid, u_int lid) 
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, sid, MGR_UNREGLAYER | REQUEST, lid,
		0, NULL, TIMEOUT_1SEC);
//	fprintf(stderr, "%s: wret %d\n", __FUNCTION__, ret); 
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		sid, MGR_UNREGLAYER | CONFIRM, TIMEOUT_1SEC);
//	fprintf(stderr, "%s: rret %d\n", __FUNCTION__, ret);
	if (ret != mISDN_HEADER_LEN) {
		if (ret >= 0)
			ret = -1;
	} else {
		ret = ifr.len;
	}
	return(ret);
}

int
mISDN_get_setstack_ind(int fid, u_int lid)
{
	iframe_t	ifr;
	int		ret;

	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		lid, MGR_SETSTACK | INDICATION, TIMEOUT_5SEC);
//	fprintf(stderr, "%s: rret %d\n", __FUNCTION__, ret);
	if (ret != mISDN_HEADER_LEN) {
		if (ret >= 0)
			ret = -1;
	} else {
		ret = ifr.len;
	}
	return(ret);
}

#ifdef OBSOLATE
int
mISDN_connect(int fid, interface_info_t *i_info)
{
	unsigned char	buf[sizeof(interface_info_t) + mISDN_HEADER_LEN];
	iframe_t	ifr;
	int		ret;
	
	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, buf, 0, MGR_CONNECT | REQUEST,
		0, sizeof(interface_info_t), i_info, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, &ifr, sizeof(iframe_t),
		0, MGR_CONNECT | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret != mISDN_HEADER_LEN) {
		if (ret > 0)
			ret = -1; 
	} else {
		if (ifr.len)
			ret = ifr.len;
		else
			ret = ifr.data.i;
	}
	return(ret);
}
#endif

int
mISDN_get_layer_info(int fid, int lid, void *info, size_t max_len)
{
	iframe_t	ifr;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, &ifr, lid, MGR_GETLAYER | REQUEST,
		0, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, info, max_len,
		lid, MGR_GETLAYER | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	return(ret);
}

void
mISDNprint_layer_info(FILE *file, layer_info_t *l_info)
{
	int i;

	fprintf(file, "instance id %08x\n", l_info->id);
	fprintf(file, "       name %s\n", l_info->name);
	fprintf(file, "        obj %08x\n", l_info->object_id);
	fprintf(file, "        ext %08x\n", l_info->extentions);
	fprintf(file, "      stack %08x\n", l_info->st);
	fprintf(file, "      clone %08x\n", l_info->clone);
	fprintf(file, "     parent %08x\n", l_info->parent);
	for(i=0;i<=MAX_LAYER_NR;i++)
		fprintf(file, "   prot%d %08x\n", i, l_info->pid.protocol[i]);
}

#ifdef OBSOLATE
int
mISDN_get_interface_info(int fid, interface_info_t *i_info)
{
	unsigned char   buf[sizeof(interface_info_t) + mISDN_HEADER_LEN];
	iframe_t	*ifr = (iframe_t *)buf;
	int		ret;

	set_wrrd_atomic(fid);
	ret = mISDN_write_frame(fid, ifr, i_info->owner, MGR_GETIF | REQUEST,
		i_info->stat, 0, NULL, TIMEOUT_1SEC);
	if (ret) {
		clear_wrrd_atomic(fid);
		return(ret);
	}
	ret = mISDN_read_frame(fid, ifr, sizeof(interface_info_t) + mISDN_HEADER_LEN,
		i_info->owner, MGR_GETIF | CONFIRM, TIMEOUT_1SEC);
	clear_wrrd_atomic(fid);
	if (ret==mISDN_HEADER_LEN) {
		ret = ifr->data.i;
	} else if (ret == (sizeof(interface_info_t) + mISDN_HEADER_LEN)) {
		ret = 0;
		memcpy(i_info, &ifr->data.p, sizeof(interface_info_t));
	}
	return(ret);
}
#endif
