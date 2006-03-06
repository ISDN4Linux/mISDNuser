/* 
 * Interface for Tenovis
 * device functions
 *
 */

#include "tenovis_int.h"
#include "tenovis.h"

/*
 * int DL3open(void);
 *
 * DL3open() opens a device through which the D channel can be accessed.
 *
 * Returns a file descriptor on success or -1 in case of an error.
 *   The file descriptor is used in all other DL3* calls and for select().
 *
 */

int
DL3open(void)
{
	int			fid, ret;
	tenovisdev_t		*dev;
	
	if (0>(fid = mISDN_open()))
		return(fid);
	dev = get_tdevice(fid);
	if (dev) {
		fprintf(stderr, "%s device %d (%p) has allready fid(%d)\n",
			__FUNCTION__, dev->fid, dev, fid);
		close(fid);
		errno = EBUSY;
		return(-1); 
	}
	dev = alloc_tdevice(fid);
	if (!dev) {
		return(-1);
	}
	ret = setup_tdevice(dev);
	if (ret)
		ret  = -1;
	else
		ret = fid;
	return(ret);
}

/*
 *
 * int DL3close(int DL3fd)
 *
 *  DL3close(int DL3fd) closes the DL3fd previously opened DL3open().
 *
 *  The file descriptor DL3fd must not be used after DL3close() was called !
 *
 *  Parameter:
 *     DL3fd : file descriptor assigned by DL3open
 *
 *  Returnvalue:
 *     0 on success or -1 if the file descriptor was already closed or
 *     is not valid.
 *
 */
       
int
DL3close(int DL3fd)
{
	tenovisdev_t	*dev;
	int		ret;

	dev = get_tdevice(DL3fd);
	if (!dev) {
		errno = ENODEV;
		return(-1);
	}
	shutdown_tdevice(dev);
	ret = free_tdevice(dev);
	return(ret);
}


/*
 * int DL3write(int DL3fd, const void *buf, size_t count);
 *
 * Sends a message to the layer 3 of the D channel stack.
 *
 *  Parameter:
 *     DL3fd : file descriptor assigned by DL3open
 *       buf : pointer to the message buffer
 *     count : the length of the message in bytes 
 *
 *  Returnvalue:
 *     0 on success or -1 on error in which case errno is set.
 *
 *
 */

extern	int	DL3write(int DL3fd, const void *buf, size_t count)
{
	tenovisdev_t	*dev;
	int		ret;

	dev = get_tdevice(DL3fd);
	if (!dev) {
		errno = ENODEV;
		return(-1);
	}
	if (!count)
		return(0);
	ret = mISDN_write_frame(dev->fid, dev->buf.p, dev->tlid | FLG_MSG_TARGET | FLG_MSG_UP,
		DL_DATA | INDICATION, 0, count, (void *)buf, TIMEOUT_1SEC);
	return(ret);
}

/*
 * size_t DL3read(int DL3fd, void *buf, size_t count);
 *
 * Reads a message from the Layer 3 of the D channel stack.
 *
 *  Parameter:
 *     DL3fd : file descriptor assigned by DL3open
 *       buf : pointer to the message buffer
 *     count : the maximum message size which can read
 *
 *  Returnvalue:
 *     the length of the message in bytes or -1 in case of an error
 *     -2 if it was an internal (not L3) message
 */

extern	size_t	DL3read(int DL3fd, void *buf, size_t count)
{
	tenovisdev_t	*dev;
	int		ret;

	dev = get_tdevice(DL3fd);
	if (!dev) {
		errno = ENODEV;
		return(-1);
	}
	if (!buf) {
		errno = EINVAL;
		return(-1);
	}
	if (!count)
		return(0);
	if (count > dev->size - mISDN_HEADER_LEN)
		count = dev->size - mISDN_HEADER_LEN;
	ret = mISDN_read(dev->fid, dev->buf.p, count + mISDN_HEADER_LEN,
		TIMEOUT_10SEC);
#ifdef PRINTDEBUG
	fprintf(stdout, __FUNCTION__": mISDN_read ret(%d) adr(%x) pr(%x) di(%x) l(%d)\n",
				ret, dev->buf.f->addr, dev->buf.f->prim,
				dev->buf.f->dinfo, dev->buf.f->len);
#endif
	if (ret <= 0)
		return(ret);
	if (dev->buf.f->addr == (dev->tlid | FLG_MSG_TARGET | FLG_MSG_UP)) {
		if (dev->buf.f->prim == (DL_DATA | REQUEST)) {
			if (dev->buf.f->len > count) {
				errno = ENOSPC;
				return(-1);
			}
			ret = dev->buf.f->len;
			memcpy(buf, &dev->buf.f->data.p, ret);
			return(ret);
		}
	}
	ret = intern_read(dev);
	return(ret);
}

