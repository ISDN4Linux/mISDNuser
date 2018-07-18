WSP	[ \t]
NWSP	[^ \t\n]
VCHR    [A-Za-z_]
VCHRZ   [A-Za-z_0-9]
PATHCHR [A-Za-z_\/\.0-9]
IDENT   {VCHR}{VCHRZ}*
LEXT	\\\n
LAYER                [lL][aA][yY][eE][rR]
ALL                  [aA][lL][lL]
UNKNOWN              [uU][nN][kK][nN][oO][wW][nN]
ALERTING             [aA][lL][eE][rR][tT][iI][nN][gG]
CALL_PROCEEDING      [cC][aA][lL][lL]_*[pP][rR][oO][cC][eE][eE][dD][iI][nN][gG]
CONNECT              [cC][oO][nN][nN][eE][cC][tT]
CONNECT_ACKNOWLEDGE  [cC][oO][nN][nN][eE][cC][tT]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
PROGRESS             [pP][rR][oO][gG][rR][eE][sS][sS]
SETUP                [sS][eE][tT][uU][pP]
SETUP_ACKNOWLEDGE    [sS][eE][tT][uU][pP]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
RESUME               [rR][eE][sS][uU][mM][eE]
RESUME_ACKNOWLEDGE   [rR][eE][sS][uU][mM][eE]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
RESUME_REJECT        [rR][eE][sS][uU][mM][eE]_*[rR][eE][jJ][eE][cC][tT]
SUSPEND              [sS][uU][sS][pP][eE][nN][dD]
SUSPEND_ACKNOWLEDGE  [sS][uU][sS][pP][eE][nN][dD]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
SUSPEND_REJECT       [sS][uU][sS][pP][eE][nN][dD]_*[rR][eE][jJ][eE][cC][tT]
USER_INFORMATION     [uU][sS][eE][rR]_*[iI][nN][fF][oO][rR][mM][aA][tT][iI][oO][nN]
DISCONNECT           [dD][iI][sS][cC][oO][nN][nN][eE][cC][tT]
RELEASE              [rR][eE][lL][eE][aA][sS][eE]
RELEASE_COMPLETE     [rR][eE][lL][eE][aA][sS][eE]_*[cC][oO][mM][pP][lL][eE][tT][eE]
RESTART              [rR][eE][sS][tT][aA][rR][tT]
RESTART_ACKNOWLEDGE  [rR][eE][sS][tT][aA][rR][tT]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
SEGMENT              [sS][eE][gG][mM][eE][nN][tT]
CONGESTION_CONTROL   [cC][oO][nN][gG][eE][sS][tT][iI][oO][nN]_*[cC][oO][nN][tT][rR][oO][lL]
INFORMATION          [iI][nN][fF][oO][rR][mM][aA][tT][iI][oO][nN]
FACILITY             [fF][aA][cC][iI][lL][iI][tT][yY]
NOTIFY               [nN][oO][tT][iI][fF][yY]
STATUS               [sS][tT][aA][tT][uU][sS]
STATUS_ENQUIRY       [sS][tT][aA][tT][uU][sS]_*[eE][nN][qQ][uU][iI][rR][yY]
HOLD                 [hH][oO][lL][dD]
HOLD_ACKNOWLEDGE     [hH][oO][lL][dD]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
HOLD_REJECT          [hH][oO][lL][dD]_*[rR][eE][jJ][eE][cC][tT]
RETRIEVE             [rR][eE][tT][rR][iI][eE][vV][eE]
RETRIEVE_ACKNOWLEDGE [rR][eE][tT][rR][iI][eE][vV][eE]_*[aA][cC][kK][nN][oO][wW][lL][eE][dD][gG][eE]
RETRIEVE_REJECT      [rR][eE][tT][rR][iI][eE][vV][eE]_*[rR][eE][jJ][eE][cC][tT]
REGISTER             [rR][eE][gG][iI][sS][tT][eE][rR]
ECHO                 [eE][cC][hH][oO]
TRUE                 [tT][rR][uU][eE]
FALSE                [fF][aA][lL][sS][eE]
VERBOSE              [vV][eE][rR][bB][oO][sS][eE]
HEX                  [hH][eE][xX]
HEXDUMP              [hH][eE][xX][dD][uU][mM][pP]
ENABLE               [eE][nN][aA][bB][lL][eE]
DISABLE              [dD][iI][sS][aA][bB][lL][eE]
CONTROLLER           [cC][oO][nN][tT][rR][oO][lL][lL][eE][rR]
KEEPALIVE            [kK][eE][eE][pP][aA][lL][iI][vV][eE]
TEI                  [tT][eE][iI]
FULL                 [fF][uU][lL][lL]
SAPI                 [sS][aA][pP][iI]
ACTIVATE             [aA][cC][tT][iI][vV][aA][tT][eE]
DEACTIVATE           [dD][eE][aA][cC][tT][iI][vV][aA][tT][eE]
CONTROL              [cC][oO][nN][tT][rR][oO][lL]
SYSLOG               [sS][yY][sS][lL][oO][gG]
LOGFILE              [lL][oO][gG][fF][iI][lL][eE]
DUMPFILE             [dD][uU][mM][pP][fF][iI][lL][eE]
FRAMES               [fF][rR][aA][mM][eE][sS]
NOTICE               [nN][oO][tT][iI][cC][eE]
INFO                 [iI][nN][fF][oO]


%option nounput
%option noinput
%option yylineno

%START Start Section EchoVal L1Value L2Value L3Type Level3 SysLog ErrorOut GetPath

%{
#include "logger.h"

static int	controller = -2;
static int	l3typ = -2;
static int	l3level = -2;
static int	filetyp = 0;
static int	l1_val = 0;
static int	l2_val = 0;
static struct mController *currentController = NULL;

static int new_controller(void);
static int setLayer3(int, int);

#define setValue(name, val)	{\
					if (currentController) \
						currentController->name = val; \
				}
%}

%%
	int ret;


<Start>{
#.*			;
\[global\].*		{
				if (controller != -2) {
					fprintf(stderr, "Line %d [global] section need to be the first section\n", yylineno);
					return -1;
				}
				controller = -1;
				new_controller();
				BEGIN Section;
			}
\[{CONTROLLER}{WSP}*[0-9]+\].*	{
					controller = atoi(yytext+11);
					ret = new_controller();
					if (ret < 0) {
						fprintf(stderr, "Line %d: '%s' Controller number %d is not valid\n", yylineno, yytext, controller);
						return -1;
					} else if (!ret) {
						fprintf(stderr, "Line %d: Controller %d was detected - ignored\n", yylineno, controller);
					}
					BEGIN Section;
				}
\[[0-9]+\].*		{
				controller = atoi(yytext+1);
				ret = new_controller();
				if (ret < 0) {
					fprintf(stderr, "Line %d: '%s' Controller number %d is not valid\n", yylineno, yytext, controller);
					return -1;
				} else if (!ret) {
					fprintf(stderr, "Line %d: Controller %d was detected - ignored\n", yylineno, controller);
				}
				BEGIN Section;
			}
{LEXT}			;
\n			{ }
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<Section>{
#.*			;
{WSP}+			;
\[			{
				yyless(yyleng-1);
				BEGIN Start;
			}
{ENABLE}		setValue(enable, 1);
{DISABLE}		setValue(enable, 0);
{ECHO}			{
				BEGIN EchoVal;
			}
{DUMPFILE}/{WSP}	{filetyp = 1; BEGIN GetPath;}
{LOGFILE}/{WSP}		{filetyp = 2; BEGIN GetPath;}
{SYSLOG}/{WSP}		{
				BEGIN SysLog;
			}
{LAYER}1/{WSP}*		{
				BEGIN L1Value;
			}
{LAYER}2/{WSP}		{
				BEGIN L2Value;
			}
{LAYER}3/{WSP}		{
				l3typ =-1 ;
				BEGIN L3Type;
			}
{LEXT}			;
\n			{ }
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<EchoVal>{
{WSP}+			;
#.*			;
{TRUE}			|
{ENABLE}		setValue(echo, 1);
{FALSE}			|
{DISABLE}		setValue(echo, 0);
{LEXT}			;
\n			{
				setValue(layer2, l2_val)
				BEGIN Section;
			}
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<L1Value>{
#.*			;
{WSP}+			;
{TRUE}			|
{ENABLE}		l1_val |= 1;
{FALSE}			|
{DISABLE}		l1_val = 0;
\n			{
				setValue(layer1, l1_val)
				BEGIN Section;
			}
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<L2Value>{
{WSP}+			;
#.*			;
{TRUE}			|
{ENABLE}		l2_val |= l2vENABLE;
{TEI}			l2_val |= l2vTEI;
{SAPI}			l2_val |= l2vSAPI;
{CONTROL}		l2_val |= l2vCONTROL;
{KEEPALIVE}		l2_val |= l2vKEEPALIVE;
{FALSE}			|
{DISABLE}		l2_val = l2vDISABLE;
{LEXT}			;
\n			{
				setValue(layer2, l2_val)
				BEGIN Section;
			}
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<L3Type>{
{WSP}+			;
{ALL}			{
				l3typ = mTall;
				l3level = l3vDISABLE;
				BEGIN Level3;
			}
{UNKNOWN}		{
				l3typ = mTunknown;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{ALERTING}		{
				l3typ = mTalerting;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{CALL_PROCEEDING}	{
				l3typ = mTcall_proceeding;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{CONNECT}		{
				l3typ = mTconnect;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{CONNECT_ACKNOWLEDGE}	{
				l3typ = mTconnect_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{PROGRESS}		{
				l3typ = mTprogress;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{SETUP}			{
				l3typ = mTsetup;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{SETUP_ACKNOWLEDGE}	{
				l3typ = mTsetup_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RESUME}		{
				l3typ = mTresume;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RESUME_ACKNOWLEDGE}	{
				l3typ = mTresume_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RESUME_REJECT}		{
				l3typ = mTresume_reject;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{SUSPEND}		{
				l3typ = mTsuspend;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{SUSPEND_ACKNOWLEDGE}	{
				l3typ = mTsuspend_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{SUSPEND_REJECT}	{
				l3typ = mTsuspend_reject;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{USER_INFORMATION}	{
				l3typ = mTuser_information;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{DISCONNECT}		{
				l3typ = mTdisconnect;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RELEASE}		{
				l3typ = mTrelease;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RELEASE_COMPLETE}	{
				l3typ = mTrelease_complete;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RESTART}		{
				l3typ = mTrestart;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RESTART_ACKNOWLEDGE}	{
				l3typ = mTrestart_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{SEGMENT}		{
				l3typ = mTsegment;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{CONGESTION_CONTROL}	{
				l3typ = mTcongestion_control;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{INFORMATION}		{
				l3typ = mTinformation;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{FACILITY}		{
				l3typ = mTfacility;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{NOTIFY}		{
				l3typ = mTnotify;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{STATUS}		{
				l3typ = mTstatus;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{STATUS_ENQUIRY}	{
				l3typ = mTstatus_enquiry;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{HOLD}			{
				l3typ = mThold;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{HOLD_ACKNOWLEDGE}	{
				l3typ = mThold_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{HOLD_REJECT}		{
				l3typ = mThold_reject;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RETRIEVE}		{
				l3typ = mTretrieve;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RETRIEVE_ACKNOWLEDGE}	{
				l3typ = mTretrieve_acknowledge;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{RETRIEVE_REJECT}	{
				l3typ = mTretrieve_reject;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{REGISTER}		{
				l3typ = mTregister;
				l3level = currentController ? currentController->layer3[l3typ] : 0;
				BEGIN Level3;
			}
{LEXT}			;
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<Level3>{
{WSP}+			;
#.*			;
{DISABLE}		|
{FALSE}			l3level = l3vDISABLE;
{ENABLE}		|
{TRUE}			l3level |= l3vENABLE;
{VERBOSE}		l3level |= l3vVERBOSE;
{HEX}			|
{HEXDUMP}		l3level |= l3vHEX;
[0-7]			l3level = atoi(yytext);
\n			{
				if (setLayer3(l3typ, l3level))
					return -1;
				l3typ = -2;
				l3level = -2;
				BEGIN Section;
			}
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<SysLog>{
{WSP}+			;
#.*			;
{FALSE}			setValue(syslog, 0);
{TRUE}			|
{INFO}			setValue(syslog, LOG_INFO);
{NOTICE}		setValue(syslog, LOG_NOTICE);
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
{LEXT}			;
\n 			BEGIN Section;
}

<GetPath>{
{WSP}+			;
#.*			;
{PATHCHR}+/[ \t#\n]*	{
				if (currentController) {
					if (filetyp == 1) {
						if (defController == currentController)
							fprintf(stderr, "dumpfile ignored for global section\n");
						else
							strncpy(currentController->dumpfile, yytext, MAX_FILE_NAME - 1);
					} else if (filetyp == 2) {
						strncpy(currentController->logfile, yytext, MAX_FILE_NAME - 1);
					} else {
						fprintf(stderr, "Got unhandled filetype %d with %s  - abort\n", filetyp, yytext);
						return -1;
					}
				}
				filetyp = 0;
			}
{LEXT}			;
\n			{
				if (filetyp) {
					fprintf(stderr, "Need file path argument for filetyp=%d\n", filetyp);
					return -1;
				}
				BEGIN Section;
			}
.			{
				yyless(0);
				BEGIN ErrorOut;
			}
}

<ErrorOut>{
.+			{
				fprintf(stderr, "Unknown item '%s' on line %d\n", yytext, yyget_lineno());
				return -1;
			}
}

%%


static int new_controller(void)
{
	if (controller >= mI_ControllerCount) {
		currentController = NULL;
		return 0;
	}
	if (controller == -1) { /* global */
		currentController = defController;
		l1_val = 0;
		l2_val = 0;
		return 1;
	}
	if (controller < 0) {
		currentController = NULL;
		return -1;
	}
	currentController = &defController[controller + 1];
	/* set defaults */
	memcpy(currentController, defController, sizeof(*currentController));
	currentController->mNr = controller;
	l1_val = currentController->layer1;
	l2_val = currentController->layer2;
	return 1;
}

static int setLayer3(int typ, int level)
{
	if (!currentController)
		return 0;
	if (typ == mTall) {
		int i;
		for (i = 0; i < mTall; i++)
			currentController->layer3[i] = level;
	} else if (typ < 0 || typ > mTall) {
		fprintf(stderr, "Line %d: layer3 message type %d out of range\n", yyget_lineno(), typ);
		return -1;
	} else
		currentController->layer3[typ] = level;
	return 0;
}


int yywrap(void)
{
	return 1;
}

int parse_config(FILE *f)
{
	int ret;

	yyin = f;
	BEGIN Start;
	ret = yylex();
	yylex_destroy();
	return ret;
}

