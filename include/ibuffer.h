#ifndef IBUFFER_H
#define IBUFFER_H
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

/* ibuffer stuff */

typedef struct _ibuffer		ibuffer_t;

struct _ibuffer {
	int		size;
	unsigned char	*buffer;
	int		ridx;
	int		widx;
	sem_t		*rsem;
	sem_t		*wsem;	
};

static inline void
clear_ibuffer(ibuffer_t *ib)
{
	if (!ib)
		return;
	ib->ridx = 0;
	ib->widx = 0;
}

static inline ibuffer_t *
init_ibuffer(int size)
{
	ibuffer_t	*ib;

	ib = (ibuffer_t *)malloc(sizeof(ibuffer_t));
	if (!ib)
		return(NULL);
	memset(ib, 0, sizeof(ibuffer_t));
	ib->buffer = (unsigned char *)malloc(size);
	if (!ib->buffer) {
		free(ib);
		return(NULL);
	}
	ib->size = size;
	return(ib);
}

static inline void
free_ibuffer(ibuffer_t *ib)
{
	if (!ib)
		return;
	if (ib->buffer)
		free(ib->buffer);
	free(ib);
}

static inline int
ibuf_usedcount(ibuffer_t *ib)
{
	int l;

	if (!ib)
		return(0);
	l = ib->widx - ib->ridx;
	if (l<0)
		l += ib->size;
	return(l);
}


static inline int
ibuf_freecount(ibuffer_t *ib)
{
	if (!ib)
		return(0);
	return(ib->size - ibuf_usedcount(ib));
}

static inline void
ibuf_memcpy_w(ibuffer_t *ib, void *data, int len)
{
	unsigned char *p = (unsigned char *)data;
	int	frag;

	frag = ib->size - ib->widx;
	if (frag < len) {
		memcpy(&ib->buffer[ib->widx], p, frag);
		p += frag;
		frag = len - frag;
		ib->widx = 0;
	} else
		frag = len;
	memcpy(&ib->buffer[ib->widx], p, frag);
	ib->widx += frag;
	ib->widx %= ib->size;
}

static inline void
ibuf_memcpy_r(void *data, ibuffer_t *ib, int len)
{
	unsigned char *p = (unsigned char *)data;
	int	frag;

	frag = ib->size - ib->ridx;
	if (frag < len) {
		memcpy(p, &ib->buffer[ib->ridx], frag);
		p += frag;
		frag = len - frag;
		ib->ridx = 0;
	} else
		frag = len;
	memcpy(p, &ib->buffer[ib->ridx], frag);
	ib->ridx += frag;
	ib->ridx %= ib->size;
}

#endif

