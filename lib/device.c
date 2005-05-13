#include "mISDNlib.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>



/* API library to use with /dev/mISDN */

typedef struct _mISDNdev {
	struct _mISDNdev	*prev;
	struct _mISDNdev	*next;
	pthread_mutex_t		rmutex;
	pthread_mutex_t		wmutex;
	int			Flags;
	int			fid;
	int			isize;
	unsigned char		*inbuf;
	unsigned char           *irp;
	unsigned char		*iend;
} mISDNdev_t;

#define FLG_mISDN_WRRD_ATOMIC	1

mISDNdev_t *devlist = NULL;

static	pthread_mutex_t	devlist_lock = PTHREAD_MUTEX_INITIALIZER;

#define mISDN_DEVICE		"/dev/mISDN"

#if 0
void
xxxxxxxxxx(void) {
if (devlist)
	fprintf(stderr, "xxxxxxxxxx dev %p prev %p next %p\n", devlist, devlist->prev, devlist->next);
else
	fprintf(stderr, "xxxxxxxxxx devlist %p\n", devlist);
}
#endif

int
mISDN_open(void)
{
	int		fid;
	mISDNdev_t	*dev;

	
	if (0>(fid = open(mISDN_DEVICE, O_RDWR | O_NONBLOCK)))
		return(fid);
	pthread_mutex_lock(&devlist_lock);
	dev = devlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&devlist_lock);
	if (dev) {
		fprintf(stderr, "%s: device %d (%p) has allready fid(%d)\n",
			__FUNCTION__, dev->fid, dev, fid);
		close(fid);
		errno = EBUSY;
		return(-1); 
	}
	dev = malloc(sizeof(mISDNdev_t));
	if (!dev) {
		close(fid);
		errno = ENOMEM;
		return(-1);
	}
	memset(dev, 0, sizeof(mISDNdev_t));
	dev->fid = fid;
	dev->isize = mISDN_INBUFFER_SIZE;
	dev->inbuf = malloc(dev->isize);
	if (!dev->inbuf) {
		free(dev);
		close(fid);
		errno = ENOMEM;
		return(-1);
	}
	dev->irp = dev->inbuf;
	dev->iend = dev->inbuf;
	pthread_mutex_init(&dev->rmutex, NULL);
	pthread_mutex_init(&dev->wmutex, NULL);
	
	pthread_mutex_lock(&devlist_lock);
	dev->prev = devlist;
	while(dev->prev && dev->prev->next)
		dev->prev = dev->prev->next;
	if (devlist)
		dev->prev->next = dev;
	else
		devlist = dev;
	pthread_mutex_unlock(&devlist_lock);
	return(fid);
}

int
mISDN_close(int fid)
{
	mISDNdev_t	*dev = devlist;
	int		ret;

	pthread_mutex_lock(&devlist_lock);
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}

	if (!dev) {
		pthread_mutex_unlock(&devlist_lock);
		errno = ENODEV;
		return(-1);
	}
	if (dev->prev)
		dev->prev->next = dev->next;
	if (dev->next)
		dev->next->prev = dev->prev;
	if (devlist==dev)
		devlist=dev->next;
	pthread_mutex_lock(&dev->rmutex);
#ifdef CLOSE_REPORT
	fprintf(stderr, "%s: fid(%d) isize(%d) inbuf(%p) irp(%p) iend(%p)\n",
		__FUNCTION__, fid, dev->isize, dev->inbuf, dev->irp, dev->iend);	
#endif
	if (dev->inbuf)
		free(dev->inbuf);
	dev->inbuf = NULL;
	pthread_mutex_unlock(&dev->rmutex);
	ret = pthread_mutex_destroy(&dev->rmutex);
	if (ret)
		fprintf(stderr, "%s: rmutex destroy returns %d\n",
			__FUNCTION__, ret);
	pthread_mutex_lock(&dev->wmutex);
	pthread_mutex_unlock(&dev->wmutex);
	ret = pthread_mutex_destroy(&dev->wmutex);
	if (ret)
		fprintf(stderr, "%s: wmutex destroy returns %d\n",
			__FUNCTION__, ret);
	pthread_mutex_unlock(&devlist_lock);
	free(dev);
	return(close(fid));
}

static int
mISDN_remove_iframe(mISDNdev_t *dev, iframe_t *frm)
{
	u_char	*ep;
	int	len;

	if (frm->len > 0)
		len = mISDN_HEADER_LEN + frm->len;
	else
		len = mISDN_HEADER_LEN;
	ep = (u_char *)frm;
	ep += len;
	if (ep >= dev->iend)
		dev->iend = (u_char *)frm;
	else {
		memcpy(frm, ep, dev->iend - ep);
		dev->iend -= len; 
	}
	
	return(dev->iend - dev->irp);
}

int
mISDN_read(int fid, void *buf, size_t count, int utimeout) {
	mISDNdev_t	*dev;
	int		ret = 0, len, sel;
	fd_set		in;
	iframe_t	*ifr;
	struct timeval	tout;
#ifdef MUTEX_TIMELOCK
	struct timespec	ts;
#endif
	pthread_mutex_lock(&devlist_lock);
	dev = devlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&devlist_lock);
	if (!dev) {
		errno = ENODEV;
		return(-1);
	}
	if (utimeout != -1) {
		tout.tv_sec = utimeout/1000000;
		tout.tv_usec = utimeout%1000000;
#ifdef MUTEX_TIMELOCK
		if (utimeout == 0) {
			ret = pthread_mutex_trylock(&dev->rmutex);
			if (ret) {
				fprintf(stderr, "%s: mutex_trylock (%d)\n",
					__FUNCTION__, ret);
				errno = ret;
				return(-1);
			}
		} else {
#ifdef CLOCK_REALTIME
			clock_gettime(CLOCK_REALTIME, &ts);
#else
			{
				struct timeval  tv;
				struct timezone tz;

				gettimeofday(&tv,&tz);
				TIMEVAL_TO_TIMESPEC(&tv,&ts);
			}
#endif
			ts.tv_sec += tout.tv_sec;
			ts.tv_nsec += 1000*tout.tv_usec;
			if (ts.tv_nsec > 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			ret = pthread_mutex_timedlock(&dev->rmutex, &ts);
			if (ret) {
				fprintf(stderr, "%s: mutex_timedlock (%d)\n",
					__FUNCTION__, ret);
				errno = ret;
				return(-1);
			}
		}
#else
		pthread_mutex_lock(&dev->rmutex);
#endif
	} else
		pthread_mutex_lock(&dev->rmutex);

	if (dev->Flags & FLG_mISDN_WRRD_ATOMIC) {
//		fprintf(stderr, "%s: WRRD_ATOMIC try again\n", __FUNCTION__);
		errno = EAGAIN;
		ret = -1;
		goto out;
	}
	len = dev->iend - dev->irp;
	if (!len) {
		dev->irp = dev->iend = dev->inbuf;
		pthread_mutex_unlock(&dev->rmutex);
		FD_ZERO(&in);
		FD_SET(fid, &in);
		if (utimeout != -1) {
			sel = select(fid + 1, &in, NULL, NULL, &tout);
		} else
			sel = select(fid + 1, &in, NULL, NULL, NULL);
		if (sel<0) {
//			fprintf(stderr, "%s: select err(%d)\n", __FUNCTION__, errno);
			return(sel);
		} else if (sel==0) {
//			fprintf(stderr, "%s: select timed out\n", __FUNCTION__);
			return(0);
		}
		if (FD_ISSET(fid, &in)) {
#ifdef MUTEX_TIMELOCK
			if (!utimeout) {
				ret = pthread_mutex_trylock(&dev->rmutex);
				if (ret) {
					fprintf(stderr, "%s: mutex_trylock (%d)\n",
						__FUNCTION__, ret);
					errno = ret;
					return(-1);
				}
			} else
#endif
				pthread_mutex_lock(&dev->rmutex);
			len = dev->isize  - (dev->iend - dev->irp);
			if (len<=0) {
				errno = ENOSPC;
				ret = -1;
				goto out;
			}
			if (dev->Flags & FLG_mISDN_WRRD_ATOMIC) {
//				fprintf(stderr, "%s: WRRD_ATOMIC try again\n", __FUNCTION__);
				errno = EAGAIN;
				ret = -1;
				goto out;
			}
			len = read(fid, dev->iend, len);
//			fprintf(stderr, "%s: read %d\n", __FUNCTION__, len);
			if (len <= 0) {
				ret = len;
				goto out;
			}
			dev->iend += len;
			len = dev->iend - dev->irp;
		} else {
			return(0);
		}
	}
	if (len < mISDN_HEADER_LEN) {
		dev->iend = dev->irp;
		fprintf(stderr, "%s: frame too short:%d\n",
			__FUNCTION__, len);
		errno = EINVAL;
		ret = -1;
		goto out;
	}
	ifr = (iframe_t *)dev->irp;
	if (ifr->len > 0) {
		if ((ifr->len + mISDN_HEADER_LEN) > len) {
			dev->iend = dev->irp;
			errno = EINVAL;
			ret = -1;
			goto out;
		}
		len = ifr->len + mISDN_HEADER_LEN;
	} else
		len = mISDN_HEADER_LEN;
	if (len>count) {
		errno = ENOSPC;
		ret = -1;
		goto out;
	}
	memcpy(buf, dev->irp, len);
	dev->irp += len;
	ret = len;
out:
	pthread_mutex_unlock(&dev->rmutex);
	return(ret);
}

static iframe_t *
mISDN_find_iframe(mISDNdev_t *dev, u_int addr, u_int prim) {
	iframe_t	*frm;
	u_char		*rp;
	
	rp = dev->irp;
	while(rp<dev->iend) {
		if ((dev->iend - rp) < mISDN_HEADER_LEN) {
			return(NULL);
		}
		frm = (iframe_t *)rp;
		if ((frm->addr == addr) && (frm->prim == prim))
			return(frm);
		if (frm->len > 0)
			rp += mISDN_HEADER_LEN + frm->len;
		else
			rp += mISDN_HEADER_LEN;
	}
	return(NULL);
}


int
mISDN_read_frame(int fid, void *buf, size_t count, u_int addr, u_int msgtype,
	int utimeout)
{
	mISDNdev_t	*dev;
	int		len, sel, first, ret = 0;
	fd_set		in;
	iframe_t	*ifr;
	struct timeval	tout;
#ifdef MUTEX_TIMELOCK
	struct timespec	ts;
#endif

	pthread_mutex_lock(&devlist_lock);
	dev = devlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&devlist_lock);
	if (!dev) {
		errno = ENODEV;
		return(-1);
	}
	if (utimeout != -1) {
		tout.tv_sec = utimeout/1000000;
		tout.tv_usec = utimeout%1000000;
#ifdef MUTEX_TIMELOCK
		if (utimeout == 0) {
			ret = pthread_mutex_trylock(&dev->rmutex);
			if (ret) {
				fprintf(stderr, "%s: mutex_trylock (%d)\n",
					__FUNCTION__, ret);
				errno = ret;
				return(-1);
			}
		} else {
#ifdef CLOCK_REALTIME
			clock_gettime(CLOCK_REALTIME, &ts);
#else
			{
				struct timeval  tv;
				struct timezone tz;

				gettimeofday(&tv,&tz);
				TIMEVAL_TO_TIMESPEC(&tv,&ts);
			}
#endif
			ts.tv_sec += tout.tv_sec;
			ts.tv_nsec += 1000*tout.tv_usec;
			if (ts.tv_nsec > 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			ret = pthread_mutex_timedlock(&dev->rmutex, &ts);
			if (ret) {
				fprintf(stderr, "%s: mutex_timedlock (%d)\n",
					__FUNCTION__, ret);
				errno = ret;
				return(-1);
			}
		}
#else
		pthread_mutex_lock(&dev->rmutex);
#endif
	} else
		pthread_mutex_lock(&dev->rmutex);

	first = 1;
	while((utimeout == -1) || tout.tv_sec || tout.tv_usec || first) {
		if (!first || !(dev->iend - dev->irp)) {
			FD_ZERO(&in);
			FD_SET(fid, &in);
			if (utimeout != -1)
				sel = select(fid + 1, &in, NULL, NULL, &tout);
			else
				sel = select(fid + 1, &in, NULL, NULL, NULL);
			if (sel<0) {
//				fprintf(stderr, "%s: select err(%d)\n", __FUNCTION__, errno);
				ret = sel;
				goto out;
			} else if (sel==0) {
//				fprintf(stderr, "%s: select timed out\n", __FUNCTION__);
				ret = 0;
				goto out;
			}
			if (FD_ISSET(fid, &in)) {
				len = dev->isize - (dev->iend - dev->irp);
				if (len<=0) {
					errno = ENOSPC;
					ret = -1;
					goto out;
				}
				len = read(fid, dev->iend, len);
//				fprintf(stderr, "%s: read %d\n", __FUNCTION__, len);
				if (len <= 0) {
					ret = len;
					goto out;
				}
				dev->iend += len;
			} else
				continue;
		}
		if (dev->iend - dev->irp) {
			ifr = mISDN_find_iframe(dev, addr, msgtype);
			if (ifr) {
				if (ifr->len > 0) {
#if 0
					if ((ifr->len + mISDN_HEADER_LEN) > len) {
						dev->irp = dev->iend;
						errno = EINVAL;
						ret = -1;
						goto out;
					}
#endif
					len = ifr->len + mISDN_HEADER_LEN;
				} else
					len = mISDN_HEADER_LEN;
				if (len > count) {
					errno = ENOSPC;
					ret = -1;
					goto out;
				}
				memcpy(buf, ifr, len);
				mISDN_remove_iframe(dev, ifr);
				ret = len;
				goto out;
			}
		}
		first = 0;
	}
out:
	pthread_mutex_unlock(&dev->rmutex);
	return(ret);
}

int
mISDN_write(int fid, void *buf, size_t count, int utimeout) {
	mISDNdev_t	*dev;
	int		len, sel;
	fd_set		out;
	struct timeval	tout;
#ifdef MUTEX_TIMELOCK
	struct timespec	ts;
	int		ret;
#endif

	pthread_mutex_lock(&devlist_lock);
	dev = devlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&devlist_lock);
	if (!dev) {
		errno = ENODEV;
		return(-1);
	}
	FD_ZERO(&out);
	FD_SET(fid, &out);
	if (utimeout != -1) {
		tout.tv_sec = utimeout/1000000;
		tout.tv_usec = utimeout%1000000;
		sel = select(fid + 1, NULL, &out, NULL, &tout);
	} else
		sel = select(fid + 1, NULL, &out, NULL, NULL);
	if (sel<=0)
		return(sel);
	if (!FD_ISSET(fid, &out))
		return(0);
	if (utimeout != -1) {
#ifdef MUTEX_TIMELOCK
		if (utimeout == 0) {
			ret = pthread_mutex_trylock(&dev->wmutex);
			if (ret) {
				fprintf(stderr, "%s: mutex_trylock (%d)\n",
					__FUNCTION__, ret);
				errno = ret;
				return(-1);
			}
		} else {
#ifdef CLOCK_REALTIME
			clock_gettime(CLOCK_REALTIME, &ts);
#else
			{
				struct timeval  tv;
				struct timezone tz;

				gettimeofday(&tv,&tz);
				TIMEVAL_TO_TIMESPEC(&tv,&ts);
			}
#endif
			ts.tv_sec += tout.tv_sec;
			ts.tv_nsec += 1000*tout.tv_usec;
			if (ts.tv_nsec > 1000000000L) {
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000L;
			}
			ret = pthread_mutex_timedlock(&dev->wmutex, &ts);
			if (ret) {
				fprintf(stderr, "%s: mutex_timedlock (%d)\n",
					__FUNCTION__, ret);
				errno = ret;
				return(-1);
			}
		}
#else
		pthread_mutex_lock(&dev->wmutex);
#endif
	} else
		pthread_mutex_lock(&dev->wmutex);
	len = write(fid, buf, count);
	pthread_mutex_unlock(&dev->wmutex);
	return(len);
}

int
mISDN_write_frame(int fid, void *fbuf, u_int addr, u_int msgtype,
		int dinfo, int dlen, void *dbuf, int utimeout)
{
	iframe_t        *ifr = fbuf;
	int		len = mISDN_HEADER_LEN;
	int		ret;

	if (!fbuf) {
		errno = EINVAL;
		return(-1);
	}
	if ((dlen > 0) && !dbuf) {
		errno = EINVAL;
		return(-1);
	}
	ifr->addr = addr;
	ifr->prim = msgtype;
	ifr->dinfo= dinfo;
	ifr->len  = dlen;
	if (dlen>0) {
		len += dlen;
		memcpy(&ifr->data.i, dbuf, dlen);
	}
	ret = mISDN_write(fid, ifr, len, utimeout);
	if (ret == len)
		ret = 0;
	else if (ret>=0) {
		errno = ENOSPC;
		ret = -1;
	}
	return(ret);
}

int
mISDN_select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *timeout)
{
	mISDNdev_t	*dev = devlist;

	if (readfds) {
		pthread_mutex_lock(&devlist_lock);
		while(dev) {
			if (FD_ISSET(dev->fid, readfds)) {
				if (dev->iend - dev->irp) {
					pthread_mutex_unlock(&devlist_lock);
					FD_ZERO(readfds);
					FD_SET(dev->fid, readfds);
					if (writefds)
						FD_ZERO(writefds);
					if (exceptfds)
						FD_ZERO(exceptfds);
					return(1);
				}
			}
			dev = dev->next;
		}
		pthread_mutex_unlock(&devlist_lock);
	}
	
	return(select(n, readfds, writefds, exceptfds, timeout));
}

int
set_wrrd_atomic(int fid)
{
	mISDNdev_t	*dev;
	int		ret;

	pthread_mutex_lock(&devlist_lock);
	dev = devlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&devlist_lock);
	if (!dev) {
		return(-1);
	}
	pthread_mutex_lock(&dev->rmutex);
	if (dev->Flags & FLG_mISDN_WRRD_ATOMIC)
		ret = 1;
	else {
		ret = 0;
		dev->Flags |= FLG_mISDN_WRRD_ATOMIC;
	}
	pthread_mutex_unlock(&dev->rmutex);
	return(ret);
}

int
clear_wrrd_atomic(int fid)
{
	mISDNdev_t	*dev;
	int		ret;

	pthread_mutex_lock(&devlist_lock);
	dev = devlist;
	while(dev) {
		if (dev->fid==fid)
			break;
		dev = dev->next;
	}
	pthread_mutex_unlock(&devlist_lock);
	if (!dev) {
		return(-1);
	}
	if (dev->Flags & FLG_mISDN_WRRD_ATOMIC) {
		dev->Flags &= ~FLG_mISDN_WRRD_ATOMIC;
		ret = 0;
	} else {
		ret = 1;
	}
	return(ret);
}
