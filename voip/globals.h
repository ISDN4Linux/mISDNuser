#ifndef FLAG_GSM
#define FLAG_GSM	0x0020
#endif

extern	int		parse_cfg(char *, manager_t *);

#ifdef INIT_GLOBALS
	int	global_debug = 0;
	int	rtp_port = Internet_Port;
	int	default_flags = 0;
	char	RecordCtrlFile[1024] = {0,};
	char	RecordFilePath[1024] = {0,};
#else
extern	int	global_debug;
extern	int	rtp_port;
extern	int	default_flags;
extern	char	RecordCtrlFile[1024];
extern  char	RecordFilePath[1024];
#endif
