
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

//#define PRINTDEBUG
#undef	PRINTDEBUG

#define	TN_INBUFFER_SIZE	4000

typedef struct _tenovisdev {
	struct _tenovisdev	*prev;
	struct _tenovisdev	*next;
	int			fid;
	unsigned int		dstid;
	unsigned int		dl0id;
	unsigned int		dl1id;
	unsigned int		dl2id;
	unsigned int		dl3id;
	unsigned int		dl4id;
	unsigned int		tlid;
	unsigned int		hwid;
	unsigned int		Flags;
	pthread_mutex_t		mutex;
	int			size;
	union {
		unsigned char	*p;
		iframe_t	*f;
	} buf;
} tenovisdev_t;

#define TN_FLG_L2_ACTIV		0x0001

extern	tenovisdev_t		*get_tdevice(int fid);
extern	tenovisdev_t		*alloc_tdevice(int fid);
extern	int			free_tdevice(tenovisdev_t *dev);
extern	int			setup_tdevice(tenovisdev_t *dev);
extern	int			shutdown_tdevice(tenovisdev_t *dev);
extern	int			intern_read(tenovisdev_t *dev);
