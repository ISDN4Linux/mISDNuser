/* Interface for Tenovis */

/*
 * int DL3open(void);
 *
 * DL3open() opens a device through which the D channel can be accessed.
 *
 * Returns a file descriptor on success or -1 in case of an error.
 *   The file descriptor is used in all other DL3* calls and for select().
 *
 */

extern	int	DL3open(void);

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
       
extern	int	DL3close(int DL3fd);

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

extern	int	DL3write(int DL3fd, const void *buf, size_t count);

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

extern	size_t	DL3read(int DL3fd, void *buf, size_t count);

