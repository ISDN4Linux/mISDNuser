#ifndef ISOUND_H
#define ISOUND_H

#include "ibuffer.h"

typedef	struct _isound	isound_t;

#define MAX_AUDIO_READ	2048

struct _isound {
	ibuffer_t	*sbuf;
	ibuffer_t	*rbuf;
	int		Flag;
	int		data;
	unsigned char	rtmp[MAX_AUDIO_READ];
	unsigned char	wtmp[MAX_AUDIO_READ];
	int		wlen;
	int             widx;
	sem_t		work;
	pthread_t	rd_t;
	pthread_t	wr_t;
};

#endif
