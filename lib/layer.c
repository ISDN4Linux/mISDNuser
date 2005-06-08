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
	if (ret < (mISDN_HEADER_LEN + sizeof(int))) {
		if (ret == mISDN_HEADER_LEN)
			ret = ifr->len;
		else if (ret>0)
			ret = -EINVAL;
	} else
		ret = ifr->data.i;
//	fprintf(stderr, "%s: ret %x\n", __FUNCTION__, ret);
	return(ret);
}

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
