#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "isdn_net.h"
#include "globals.h"

extern	int		parse_cfg(char *, manager_t *);

int main(argc,argv)
int argc;
char *argv[];
{
	manager_t	mgr;
	nr_list_t	*nr;

	memset(&mgr, 0, sizeof(manager_t));
	printf("Start: Debug %x Port %d Flags %x\n",
		global_debug, rtp_port, default_flags);
	parse_cfg("tstcfg", &mgr);
	printf("AfterParse: Debug %x Port %d Flags %x\n",
		global_debug, rtp_port, default_flags);
	nr = mgr.nrlist;
	while(nr) {
		printf("nr(%s) len(%d) flg(%x) typ(%d) name(%s)\n",
			nr->nr, nr->len, nr->flags, nr->typ, nr->name);
		nr = nr->next;
	}
	return(0);
}