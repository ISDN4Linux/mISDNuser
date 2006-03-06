#ifndef _mISDN_LIB_H
#define _mISDN_LIB_H

/* API library to use with /dev/mISDN */

/* we need somme extentions */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

typedef unsigned short u16;

#include <sys/types.h>
#include <stdio.h>
#include "linux/mISDNif.h"

#define mISDN_INBUFFER_SIZE	0x20000

typedef struct _iframe {
	u_int	addr __attribute__((packed));
	u_int	prim __attribute__((packed));
	int	dinfo __attribute__((packed));
	int	len __attribute__((packed));
	union {
		u_char  b[4];
		void    *p;
		int     i;
		u_int     ui;
	} data;
} iframe_t;


#define TIMEOUT_1SEC	1000000
#define TIMEOUT_5SEC	5000000
#define TIMEOUT_10SEC	10000000
#define TIMEOUT_INFINIT -1

// extern	void xxxxxxxxxx(void);

/* Prototypes from device.c */

/* mISDN_open(void)
 *
 * opens a mISDN device and allocate buffers for it
 *
 * parameter:
 *    none
 *
 * return:
 *    the file descriptor or -1 on error and the error cause in errno
 */
extern int mISDN_open(void);

/* mISDN_close(int fid)
 *
 * close the mISDN device and frees related memory.
 *
 * parameter:
 *    fid - file descriptor returned by mISDN_open
 *
 * return:
 *    0 on success or -1 on error and the error cause in errno
 *
 */
extern int mISDN_close(int fid);

/* mISDN_read(int fid, void *buf, size_t count, int utimeout)
 *
 * read one message from device or buffer
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * buf      - pointer to readbuffer
 * count    - maximum length of read data
 * utimeout - maximum time in microseconds to wait for data, if -1
 *            wait until some data is ready
 *
 * return:
 *    length of read data or -1 on error and the error cause in errno
 *
 */
extern int mISDN_read(int fid, void *buf, size_t count, int utimeout);

/* mISDN_read_frame(int fid, void *buf, size_t count, u_int addr,
 *                  u_int msgtype, int utimeout)
 *
 * read one message for address (addr) and message type (msgtype)
 * from device or buffer
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * buf      - pointer to readbuffer
 * count    - maximum length of read data
 * addr     - address of frame
 * msgtype  - message type of frame
 * utimeout - maximum time in microseconds to wait for data, if -1
 *            wait until some data is ready
 *
 * return:
 *    length of read data or -1 on error and the error cause in errno
 *
 */
extern int mISDN_read_frame(int fid, void *buf, size_t count, u_int addr,
	u_int msgtype, int utimeout);

/* mISDN_write(int fid, void *buf, size_t count, int utimeout)
 *
 * write a message to device
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * buf      - pointer to data to write
 * count    - length of data
 * utimeout - maximum time in microseconds to wait for device ready to
 *            accept new data, if -1 wait until device is ready
 *
 * return:
 *    length of written data or -1 on error and the error cause in errno
 *
 */
extern int mISDN_write(int fid, void *buf, size_t count, int utimeout);

/* mISDN_write_frame(int fid, void *fbuf, u_int addr, u_int msgtype,
 *                   int dinfo, int dlen, void *dbuf, int utimeout)
 *
 * write a frame to device
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * fbuf     - buffer for frame, caller has to provide a big enougth buffer
 * count    - length of data
 * addr     - address for frame
 * msgtype  - message type of frame
 * dinfo    - data info parameter
 * dlen     - len of dbuf data, if negativ it is an error code (dbuf len=0)
 * dbuf     - pointer to frame payload data
 * utimeout - maximum time in microseconds to wait for device ready to
 *            accept new data, if -1 wait until device is ready
 *
 * return:
 *    0 if successfull or -1 on error and the error cause in errno
 *
 */
extern int mISDN_write_frame(int fid, void *fbuf, u_int addr, u_int msgtype,
		int dinfo, int dlen, void *dbuf, int utimeout);

/* int mISDN_select(int n, fd_set *readfds, fd_set *writefds,
 *                  fd_set *exceptfds, struct timeval *timeout)
 *
 * select call which handles mISDN readbuffer
 *
 * parameters and use see man select
 *
 */

extern int mISDN_select(int n, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, struct timeval *timeout);


/* Prototypes from stack.c */

/* mISDN_get_stack_count(int fid)
 *
 * get number of ISDN stacks
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 *
 * return:
 *    count of ISDN stacks, negativ values for error
 *
 */
extern int mISDN_get_stack_count(int fid);

/* mISDN_get_stack_info(int fid, int stack, void *info, size_t max_len)
 *
 * get the info about ISDN stack
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * stack    - ID of the stack
 * info     - buffer to store info
 * max_len  - size of above buffer
 *
 * return:
 *    length of stored info, negativ values are errors
 *
 */
extern int mISDN_get_stack_info(int fid, int stack, void *info, size_t max_len);

/* mISDN_new_stack(int fid, stack_info_t *s_info)
 *
 * create a new stack
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * s_info   - info for the new stack
 *
 * return:
 *    stack id or negativ error code
 *
 */
extern int mISDN_new_stack(int fid, stack_info_t *s_info);

/* mISDN_set_stack(int fid, int stack, mISDN_pid_t *pid)
 *
 * setup a stack for the given protocol
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * stack    - stack id
 * pid      - description of the stack protocol
 *
 * return:
 *    0 on sucess other values are errors
 *
 */
extern int mISDN_set_stack(int fid, int stack, mISDN_pid_t *pid);

/* mISDN_clear_stack(int fid, int stack)
 *
 *  clear the protocol stack
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * stack    - stack id
 *
 * return:
 *    0 on sucess other values are errors
 *
 */
extern int mISDN_clear_stack(int fid, int stack);

/* mISDNprint_stack_info(FILE *file, stack_info_t *s_info)
 *
 * print out the stack_info in readable output
 *
 * parameter:
 * file     - stream to print to
 * s_info   - stack_info
 *
 * return:
 *    nothing
 *
 */
extern void mISDNprint_stack_info(FILE *file, stack_info_t *s_info);

/* Prototypes from layer.c */

/* mISDN_get_layerid(int fid, int stack, int layer)
 *
 * get the id of the layer given by stack and layer number
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * stack    - ID of the stack
 * layer    - layer number
 *
 * return:
 *    layer id or negativ error code
 *
 */
extern int mISDN_get_layerid(int fid, int stack, int layer);

/* mISDN_new_layer(int fid, layer_info_t *l_info)
 *
 * create a new layer
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * l_info   - info for the layer
 *
 * return:
 *    0 on success or error code
 *    l_info->id the id of the new layer
 *    l_info->clone the id of a cloned layer
 *
 */
extern int mISDN_new_layer(int fid, layer_info_t *l_info);

/* mISDN_preregister_layer(int fid, u_int sid, u_int lid)
 *
 * preregister a layer on a stack
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * sid      - stack id
 * lid      - layer (instance) id
 *
 * return:
 *    0 on success or error code
 *
 */
extern int mISDN_register_layer(int, u_int, u_int);
extern int mISDN_unregister_layer(int, u_int, u_int);
extern int mISDN_get_setstack_ind(int fid, u_int lid);

/* mISDN_connect(int fid, interface_info_t *i_info)
 *
 * create a new connection
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * i_info   - info for the connection
 *
 * return:
 *    0 on success or error code
 *
 */
//extern int mISDN_connect(int fid, interface_info_t *i_info);

/* mISDN_get_layer_info(int fid, int lid, void *info, size_t max_len)
 *
 * get the info about ISDN layer
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * lid      - ID of the layer
 * info     - buffer to store info
 * max_len  - size of above buffer
 *
 * return:
 *    length of stored info, negativ values are errors
 *
 */
extern int mISDN_get_layer_info(int fid, int lid, void *info, size_t max_len);

/* mISDNprint_layer_info(FILE *file, layer_info_t *l_info)
 *
 * print out the layer_info in readable output
 *
 * parameter:
 * file     - stream to print to
 * l_info   - layer_info
 *
 * return:
 *    nothing
 *
 */
extern void mISDNprint_layer_info(FILE *file, layer_info_t *l_info);

/* mISDN_get_interface_info(int fid, interface_info_t *i_info)
 *
 * get the info about ISDN layer interface
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * i_info   - contains the info about layer (i_info->owner) and
 *            which interface (i_info->stat) and gives requested info back
 *
 * return:
 *    0 on sucess other values are errors
 *
 */
//extern int mISDN_get_interface_info(int fid, interface_info_t *i_info);

/* Prototypes and defines for status.c */

/* l1 status_info */
typedef struct _status_info_l1 {
	int	len;
	int	typ;
	int	protocol;
	int	status;
	int	state;
	int	Flags;
	int	T3;
	int	delay;
	int	debug;
} status_info_l1_t;

/* State values for l1 state machine (status_info_l1_t state field) */
extern char *strL1SState[];


/* l2 status_info */
typedef struct _laddr {
	u_char  A;
	u_char  B;
} laddr_t;
                
                
typedef struct _status_info_l2 {
	int	len;
	int	typ;
	int	protocol;
	int	state;
	int	sapi;
	int	tei;
	laddr_t addr;
	int	maxlen;
	u_int	flag;
	u_int	vs;
	u_int	va;
	u_int	vr;
	int	rc;
	u_int	window;
	u_int	sow;
	int	T200;
	int	N200;
	int	T203;
	int	len_i_queue;
	int	len_ui_queue;
	int	len_d_queue;
	int	debug;
	int	tei_state;
	int	tei_ri;
	int	T202;
	int	N202;
	int	tei_debug;
} status_info_l2_t;

/* State values for l2 state machine (status_info_l2_t state field) */
extern char *strL2State[];

/* mISDN_get_status_info(int fid, int id, void *info, size_t max_len)
 *
 * get status infos about layer instances in the ISDN stack
 *
 * parameter:
 * fid      - FILE descriptor returned by mISDN_open
 * id       - ID of the instance
 * info     - buffer to store info
 * max_len  - size of above buffer
 *
 * return:
 *    length of stored info, negativ values are errors
 *
 */
extern int mISDN_get_status_info(int fid, int id, void *info, size_t max_len);

/* mISDNprint_status(FILE *file, status_info_t *si)
 *
 * print out the status in readable output
 *
 * parameter:
 * file     - stream to print to
 * s_info   - status_info
 *
 * return:
 *    0 on success or -1 if no known status_info
 *
 */
extern int mISDNprint_status(FILE *file, status_info_t *si);

/* private functions */

extern int set_wrrd_atomic(int fid);
extern int clear_wrrd_atomic(int fid);

#endif
