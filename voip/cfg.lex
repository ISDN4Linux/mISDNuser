WSP	[ \t]
NWSP	[^ \t\n]
VCHR    [A-Za-z_]
VCHRZ   [A-Za-z_0-9]
VCHRX	[A-Za-z\-\._0-9]
VCHRP	[A-Za-z\-\.\/_0-9]
MSN	[Mm][Ss][Nn]
AUDIONR	[Aa][Uu][Dd][Ii][Oo][Nn][Rr]
VOIPNR	[Vv][Oo][Ii][Pp][Nn][Rr]
DEBUG   [Dd][Ee][Bb][Uu][Gg]
PORT	[Pp][Oo][Rr][Tt]
GSM	[Gg][Ss][Mm]
RECORD	[Rr][Ee][Cc][Oo][Rr][Dd]
FILE	[Ff][Ii][Ll][Ee]
PATH	[Pp][Aa][Tt][Hh]
CTRL	[Cc][Tt][Rr][Ll]
ZIF	[0-9]
HZIF	[0-9a-fA-F]
HEX	0[Xx]{HZIF}+
NR	{ZIF}+
NAME	{VCHRX}+
PATHSTR	{VCHRP}+

%START Normal Comment Number Name NumValue PathValue

%%
	int		AktState=0;
	ulong		val=0;
	nr_list_t	*new_nr = NULL;

<Normal>{
^#.*		;
{DEBUG}{WSP}+   {
			BEGIN NumValue;
			AktState = ST_DEB;
		}
{RECORD}{CTRL}{FILE}{WSP}+ {
			BEGIN PathValue;
			AktState = ST_RCF;
}

{RECORD}{FILE}{PATH}{WSP}+ {
			BEGIN PathValue;
			AktState = ST_RFP;
}

{PORT}{WSP}+   {
			BEGIN NumValue;
			AktState = ST_PORT;
		}
{MSN}{WSP}+	{
			BEGIN Number;
			AktState = ST_MSN;
			new_nr = getnewnr(NR_TYPE_INTERN);
		}
{AUDIONR}{WSP}+	{
			BEGIN Number;
			AktState = ST_AUDIO;
			new_nr = getnewnr(NR_TYPE_AUDIO);
		}
{VOIPNR}{WSP}+	{
			BEGIN Number;
			AktState = ST_VNR;
			new_nr = getnewnr(NR_TYPE_VOIP);
		}
{GSM}{WSP}*	{
			add_cfgflag(AktState, new_nr, FLAG_GSM);
		}
{WSP}+		;
[^ \t\n]	{
			yyless(0);
			BEGIN Name; 
		}
\n		{
			new_nr = NULL;
		}
}

<Number>{
{WSP}*		;
{NR}		{
			add_cfgnr(AktState, new_nr, yytext, yyleng);
			BEGIN Normal;
		}
}
<Name>{
{WSP}*		;
{NAME}		{
			add_cfgname(AktState, new_nr, yytext, yyleng);
			BEGIN Normal;
		}
}
<NumValue>{
{WSP}*		;
{HEX}		{
			val = strtol(yytext, NULL, 16);
			add_cfgval(AktState, new_nr, val);
			AktState = ST_NORM;
			BEGIN Normal;
		}
{NR}		{
			val = strtol(yytext, NULL, 0);
			add_cfgval(AktState, new_nr, val);
			AktState = ST_NORM;
			BEGIN Normal;
		}
}
<PathValue>{
{WSP}*		;
{PATHSTR}	{
			add_path(AktState, yytext, yyleng);
			AktState = ST_NORM;
			BEGIN Normal;
		}
}
