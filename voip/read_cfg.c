#include <stdio.h>
#include <stdlib.h>

#include "isdn_net.h"
#include "helper.h"
#define INIT_GLOBALS
#include "globals.h"

static	manager_t *akt_mgr;

int		add_cfgnr(int  state, nr_list_t *nr, char *t, int l);
int		add_cfgname(int  state, nr_list_t *nr, char *t, int l);
int		add_cfgval(int  state, nr_list_t *nr, ulong val);
nr_list_t	*getnewnr(int typ);
int		add_cfgflag(int  state, nr_list_t *nr, int flag);
int		add_path(int  state, char *t, int l);

enum {
	ST_NORM,
	ST_MSN,
	ST_VNR,
	ST_AUDIO,
	ST_DEB,
	ST_PORT,
	ST_RFP,
	ST_RCF,
};

#include "cfg_lex.c"

int yywrap(void)
{
	return(1);
}

int parse_cfg(char *FName, manager_t *mgr) {
	int ret;

	yyin = fopen(FName, "r");
	if (!yyin) {
		fprintf(stderr,"cannot open cfg file %s\n", FName);
		return(1);
	} else
		fprintf(stderr,"parsing cfg file %s\n", FName);
	akt_mgr = mgr;
	BEGIN Normal;
	ret = yylex();
	fclose(yyin);
	return(ret);
}

nr_list_t *
getnewnr(int typ)
{
	nr_list_t	*nr;
	
	nr = malloc(sizeof(nr_list_t));
	memset(nr, 0, sizeof(nr_list_t));
	nr->typ = typ;
	return(nr);
}

int
add_cfgnr(int  state, nr_list_t *nr, char *t, int l)
{
//	printf("%s(%d,%p,%s)\n", __FUNCTION__, state, nr, t);
	if (nr) {
		switch(state) {
			default:
				strcpy(nr->nr, t);
				nr->len = l;
				APPEND_TO_LIST(nr, akt_mgr->nrlist);
				break;
		}
	}
	return(0);
}

int
add_cfgname(int  state, nr_list_t *nr, char *t, int l)
{
//	printf("%s(%d,%p,%s)\n", __FUNCTION__, state, nr, t);
	if (nr) {
		switch(state) {
			default:
				strcpy(nr->name, t);
				break;
		}
	}
	return(0);
}

int
add_cfgval(int  state, nr_list_t *nr, ulong val)
{
	if (nr) {
	} else {
		switch(state) {
			case ST_DEB:
				global_debug = val;
				break;
			case ST_PORT:
				rtp_port  = val;
				break;
		}
	}
	return(0);
}

int
add_cfgflag(int  state, nr_list_t *nr, int flag)
{
	if (nr) {
		nr->flags ^= flag;
	} else {
		default_flags ^= flag;
	}
	return(0);
}

int
add_path(int  state, char *t, int l)
{
//	printf("%s(%d,%s)\n", __FUNCTION__, state, t);
	if (l<1)
		return(0);
	switch(state) {
		default:
			fprintf(stderr, "%s: Unknown state(%d) text(%s)\n", __FUNCTION__,
				state, t);
		case ST_RCF:
			strcpy(RecordCtrlFile, t);
			break;
		case ST_RFP:
			strcpy(RecordFilePath, t);
			if (RecordFilePath[l-1] != '/') {
				RecordFilePath[l] = '/';
				RecordFilePath[l+1] = 0;
			}
			break;
	}
	return(0);
}
