/* 
 * Internal functions for Tenovis lib
 *
 */

#include "tenovis_int.h"
#include "tenovis.h"

static	tenovisdev_t	*tdevlist = NULL;
static	pthread_mutex_t	tdevlist_lock = PTHREAD_MUTEX_INITIALIZER;

tenovisdev_t *
alloc_tdevice(int fid)
{
	tenovisdev_t	*dev;

	dev = malloc(sizeof(tenovisdev_t));
	if (!dev) {
		close(fid);
		errno = ENODEV;
		return(NULL);
	}
	memset(dev, 0, sizeof(tenovisdev_t));
	dev->fid = fid;
	dev->size = TN_INBUFFER_SIZE;
	dev->buf.p = malloc(dev->size);
	if (!dev->buf.p) {
		close(fid);
		free(dev);
		errno = ENODEV;
		return(NULL);
	}
	pthread_mutex_init(&dev->mutex, NULL);
	pthread_mutex_lock(&tdevlist_lock);
	dev->prev = tdevlist;
	while(dev->prev && dev->prev->next)
		dev->prev = dev->prev->next;
	if (tdevlist)
		dev->prev->next = dev;
	else
		tdevlist = dev;
	pthread_mutex_unlock(&tdevlist_lock);
	return(dev);
}

tenovisdev_t *
get_tdevice(int fid)
{
	tenovisdev_t	*dev;

	pthread_mutex_lock(&tdevlist_lock);
	dev = tdevlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&tdevlist_lock);
	return(dev);
}

int
free_tdevice(tenovisdev_t *dev)
{
	int	ret;

	if (dev->prev)
		dev->prev->next = dev->next;
	if (dev->next)
		dev->next->prev = dev->prev;
	if (tdevlist==dev)
		tdevlist=dev->next;
	pthread_mutex_lock(&dev->mutex);
	if (dev->buf.p)
		free(dev->buf.p);
	dev->buf.p = NULL;
	pthread_mutex_unlock(&dev->mutex);
	ret = pthread_mutex_destroy(&dev->mutex);
	if (ret)
		fprintf(stderr, "%s mutex destroy returns %d\n",
			__FUNCTION__, ret);
	ret = mISDN_close(dev->fid);
	free(dev);
	return(ret);
}

int
setup_tdevice(tenovisdev_t *dev)
{
	int			ret;
	stack_info_t		*stinf;
	layer_info_t		linf;
#ifdef OBSOLETE
	interface_info_t	iinf;
#endif

	ret = mISDN_write_frame(dev->fid, dev->buf.p, 0,
		MGR_SETDEVOPT | REQUEST, FLG_mISDNPORT_ONEFRAME,
		0, NULL, TIMEOUT_1SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, "MGR_SETDEVOPT ret(%d)\n", ret);
#endif			
	ret = mISDN_read(dev->fid, dev->buf.p, 1024, TIMEOUT_10SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
		ret, dev->buf.f->prim, dev->buf.f->dinfo, dev->buf.f->len);
#endif
	ret = mISDN_get_stack_count(dev->fid);
#ifdef PRINTDEBUG
	fprintf(stdout, "stackcnt %d\n", ret);
#endif
	if (ret <= 0) {
		free_tdevice(dev);
		errno = ENODEV;
		return(-1);
	}
	ret = mISDN_get_stack_info(dev->fid, 1, dev->buf.p, dev->size);
	stinf = (stack_info_t *)&dev->buf.f->data.p;
#ifdef PRINTDEBUG
	mISDNprint_stack_info(stdout, stinf);
	fprintf(stdout, "ext(%x) instcnt(%d) childcnt(%d)\n",
		stinf->extentions, stinf->instcnt, stinf->childcnt);
#endif
	dev->dstid = stinf->id;
	dev->dl0id = mISDN_get_layerid(dev->fid, dev->dstid, 0);
	dev->dl1id = mISDN_get_layerid(dev->fid, dev->dstid, 1);
	dev->dl2id = mISDN_get_layerid(dev->fid, dev->dstid, 2);
	dev->dl3id = mISDN_get_layerid(dev->fid, dev->dstid, 3);
	dev->dl4id = mISDN_get_layerid(dev->fid, dev->dstid, 4);
#ifdef PRINTDEBUG
	fprintf(stdout, " dl0id = %08x\n", dev->dl0id);
	fprintf(stdout, " dl1id = %08x\n", dev->dl1id);
	fprintf(stdout, " dl2id = %08x\n", dev->dl2id);
	fprintf(stdout, " dl3id = %08x\n", dev->dl3id);
	fprintf(stdout, " dl4id = %08x\n", dev->dl4id);
#ifdef #OBSOLETE
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l2 up   own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l2 down own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl3id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l3 up   own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl3id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l3 down own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
#endif
#endif
	memset(&linf, 0, sizeof(layer_info_t));
	strcpy(&linf.name[0], "tenovis L2");
	linf.object_id = -1;
	linf.extentions = EXT_INST_MIDDLE;
	linf.pid.protocol[stinf->instcnt] = ISDN_PID_ANY;
	linf.pid.layermask = ISDN_LAYER(stinf->instcnt);
	linf.st = dev->dstid;
	dev->tlid = mISDN_new_layer(dev->fid, &linf);
#ifdef PRINTDEBUG
	fprintf(stdout, "mISDN_new_layer ret(%x)\n", dev->tlid);
	ret = mISDN_get_stack_info(dev->fid, 1, dev->buf.p, dev->size);
	stinf = (stack_info_t *)&dev->buf.f->data.p;
	mISDNprint_stack_info(stdout, stinf);
#endif
#ifdef OBSOLETE
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.extentions = EXT_INST_MIDDLE;
	iinf.owner = dev->tlid;
	iinf.peer = dev->dl3id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_write_frame(dev->fid, dev->buf.p, dev->tlid,
		MGR_SETIF | REQUEST, 0, sizeof(interface_info_t),
		&iinf, TIMEOUT_1SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, "mISDN_write_frame ret(%d)\n", ret);
#endif
	ret = mISDN_read(dev->fid, dev->buf.p, 1024, TIMEOUT_10SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
		ret, dev->buf.f->prim, dev->buf.f->dinfo,
		dev->buf.f->len);
#endif
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.extentions = EXT_INST_MIDDLE;
	iinf.owner = dev->tlid;
	iinf.peer = dev->dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_write_frame(dev->fid, dev->buf.p, dev->tlid,
		MGR_SETIF | REQUEST, 0, sizeof(interface_info_t),
		&iinf, TIMEOUT_1SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, "mISDN_write_frame ret(%d)\n", ret);
#endif
	ret = mISDN_read(dev->fid, dev->buf.p, 1024, TIMEOUT_10SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
		ret, dev->buf.f->prim, dev->buf.f->dinfo,
		dev->buf.f->len);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l2 up   own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl2id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l2 down own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl3id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_UP;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l3 up   own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
	memset(&iinf, 0, sizeof(interface_info_t));
	iinf.owner = dev->dl3id;
	iinf.stat = FLG_MSG_TARGET | FLG_MSG_DOWN;
	ret = mISDN_get_interface_info(dev->fid, &iinf);
	fprintf(stdout, "l3 down own(%x) -> peer(%x)\n",
		iinf.owner, iinf.peer);
#endif
#endif
	return(0);
}

int
shutdown_tdevice(tenovisdev_t *dev)
{
	int	ret;

	pthread_mutex_lock(&dev->mutex);
	if (dev->buf.p) {
		if (dev->tlid) {
			ret = mISDN_write_frame(dev->fid, dev->buf.p,
				dev->tlid, MGR_DELLAYER | REQUEST,
				0, 0, NULL, TIMEOUT_1SEC);
#ifdef PRINTDEBUG
			fprintf(stdout, "MGR_DELLAYER ret(%d)\n", ret);
#endif			
			ret = mISDN_read(dev->fid, dev->buf.p, 1024,
				TIMEOUT_10SEC);
#ifdef PRINTDEBUG
			fprintf(stdout, "mISDN_read ret(%d) pr(%x) di(%x) l(%d)\n",
				ret, dev->buf.f->prim, dev->buf.f->dinfo,
				dev->buf.f->len);
#endif
		}
	}
	pthread_mutex_unlock(&dev->mutex);
	return(0);
}

int
intern_read(tenovisdev_t *dev)
{
	int	ret;

#ifdef PRINTDEBUG
	fprintf(stdout, __FUNCTION__" addr(%x) prim(%x)\n",
		dev->buf.f->addr, dev->buf.f->prim);
#endif
	if (dev->buf.f->addr == (dev->tlid | FLG_MSG_TARGET | FLG_MSG_UP)) {
		if (dev->buf.f->prim == (DL_ESTABLISH | REQUEST)) {
			dev->Flags |= TN_FLG_L2_ACTIV;
			ret = mISDN_write_frame(dev->fid, dev->buf.p,
				dev->tlid | FLG_MSG_TARGET | FLG_MSG_UP, DL_ESTABLISH | CONFIRM,
				0, 0, NULL, TIMEOUT_1SEC);
#ifdef PRINTDEBUG
			fprintf(stdout, __FUNCTION__": estab cnf ret(%d)\n",
				ret);
#endif
		} else if (dev->buf.f->prim == (DL_RELEASE | REQUEST)) {
			dev->Flags &= ~TN_FLG_L2_ACTIV;
			ret = mISDN_write_frame(dev->fid, dev->buf.p,
				dev->tlid | FLG_MSG_TARGET | FLG_MSG_UP, DL_RELEASE | CONFIRM,
				0, 0, NULL, TIMEOUT_1SEC);
#ifdef PRINTDEBUG
			fprintf(stdout, __FUNCTION__": rel cnf ret(%d)\n",
				ret);
#endif
		}
	}
	return(-2);
}

