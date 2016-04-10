%{ /* -*- fundamental -*- */
/* lexer spec. for modemu command mode */
#include "cmdlex.h"	/*Cmdstat*/
#include "defs.h"	/*->atcmd.h (uchar)*/
#include "atcmd.h"	/*(atcmdDAddr)*/

#if 0
static struct {
	const char *ptr;
	const char *end;
} line;
#endif
static const char *ptr;

#if 0
#define yyInput() (*ptr++)
#define yyInput() (*ptr? *ptr++ : 0)
#endif

#if 0
static int
yyInput(void)
{
	if (line.ptr >= line.end) return 0;
	return *line.ptr++;
}
#endif

#ifdef FLEX_SCANNER
/*#undef yywrap*/
static int
flexYyInput(char *buf,int maxsize)
{
	int i;

	for (i = 0; i < maxsize; i++) {
		if (*ptr == 0) break;
		*buf++ = *ptr++;
	}
	return i;
}
# define YY_INPUT(buf,result,maxsize) (result = flexYyInput(buf,maxsize))
#else /*!ifdef FLEX_SCANNER*/
# undef input
# define input() (*ptr++)
# undef unput
# define unput(c) (ptr--) /* can't alter the const */
#endif /*ifdef FLEX_SCANNER*/

static Cmdstat cmdstat;

int
yywrap(void)
{
	return 1;
}

%}

	/*A_NUM	([0-9]{1,3}(\.[0-9]{1,3}){3}) <- meets ATT lex bug*/
	/*A_NUM	(([0-9]{1,3})(\.[0-9]{1,3}){3}) <- meets ATT lex bug*/
	/*A_NUM	(([0-9]{1,3}\.){3}[0-9]{1,3}) <- meets ATT lex bug*/
A_NUM	(([0-9]{1,3}\.){3}([0-9]{1,3}))
A_STR	([a-zA-Z][-a-zA-Z0-9.]*)
P_NUM	([0-9]{1,5})
P_STR	([a-zA-Z][-a-zA-Z0-9]*)
	/* TTYPE ([a-zA-Z][-/a-zA-Z0-9]*[a-zA-Z0-9]?) strict check */
TTYPE	([-/a-zA-Z0-9]*)

%s AT ATD
%k 1500

%%
%{
	BEGIN 0;
%}

<INITIAL>at |
<INITIAL>AT	{ BEGIN AT; cmdstat = CMDST_OK; }
<INITIAL>^#	{ return cmdstat = CMDST_NOAT; }
<INITIAL>.|\n	{ /*ignore preceding garbage*/ }

<AT>[dD]	{ BEGIN ATD; cmdstat = CMDST_ERROR; }
<AT>[eE][0-9]?	{ if (atcmdFake(yytext,"1")) return cmdstat = CMDST_ERROR; }
<AT>[fF][0-9]?	{ if (atcmdFake(yytext,"1")) return cmdstat = CMDST_ERROR; }
<AT>[hH][0-9]?	{ if (atcmdH(yytext)) return cmdstat = CMDST_ERROR; }
<AT>[iI][0-9]?	{ if (atcmdI(yytext)) return cmdstat = CMDST_ERROR; }
<AT>[oO][0-9]?	{ if (atcmdFake(yytext,"0")) return cmdstat = CMDST_ERROR;
		  cmdstat = CMDST_ATO; }
<AT>[pPtT]	{ /* just ignore */ }
<AT>[qQ][0-9]?	{ if (atcmdFake(yytext,"0")) return cmdstat = CMDST_ERROR; }
<AT>[vV][0-9]?	{ if (atcmdFake(yytext,"1")) return cmdstat = CMDST_ERROR; }
<AT>[xX][0-9]?	{ if (atcmdFake(yytext,"0")) return cmdstat = CMDST_ERROR; }
<AT>[sS][0-9]*=[0-9]*	{ /* ATT lex:[0-9]{0,n} n=2:OK n=3:NG */
			  if (atcmdSSet(yytext))
				return cmdstat = CMDST_ERROR; }
<AT>[sS][0-9]*\?	{ if (atcmdSQuery(yytext))
				return cmdstat = CMDST_ERROR; }
<AT>[zZ]		{ atcmdZ(); }
<AT>&[wW]		{ atcmdAW(); }
<AT>%[bB][0-9]?=[0-9]?	{ if (atcmdPB(yytext)) return cmdstat = CMDST_ERROR; }
<AT>%[dD][0-9]?		{ if (atcmdPD(yytext)) return cmdstat = CMDST_ERROR; }
<AT>%[lL][0-9]?		{ if (atcmdPL(yytext)) return cmdstat = CMDST_ERROR; }
<AT>%[qQ]		{ atcmdPQ(); }
<AT>%[rR][0-9]?		{ if (atcmdPR(yytext)) return cmdstat = CMDST_ERROR; }
<AT>%[tT][0-9]?		{ if (atcmdPT(yytext)) return cmdstat = CMDST_ERROR; }
<AT>%[tT]=\"{TTYPE}\"?	{ if (atcmdPTSet(yytext))
				return cmdstat = CMDST_ERROR; }
<AT>%[vV][0-9]*		{ if (atcmdPV(yytext)) return cmdstat = CMDST_ERROR; }
<AT>" "			{ /*ignore spaces*/ }
<AT>.|\n		{ return cmdstat = CMDST_ERROR; }

<ATD>[tTpP] |
<ATD>[0-9][wW,]		{ /* ignore them */ }
<ATD>[sS][0-9]*		{ /* use registered number (not work yet) */ }
<ATD>{A_NUM} |
<ATD>\"{A_NUM}\"?	{ atcmdD(yytext, ATDA_NUM, ATDP_NUL);
				cmdstat = CMDST_ATD; BEGIN AT; }
<ATD>{A_NUM}:{P_NUM} |
<ATD>\"{A_NUM}:{P_NUM}\"?	{ atcmdD(yytext, ATDA_NUM, ATDP_NUM);
					cmdstat = CMDST_ATD; BEGIN AT; }
<ATD>\"{A_NUM}:{P_STR}\"?	{ atcmdD(yytext, ATDA_NUM, ATDP_STR);
					cmdstat = CMDST_ATD; BEGIN AT; }
<ATD>\"{A_STR}\"?		{ atcmdD(yytext, ATDA_STR, ATDP_NUL);
					cmdstat = CMDST_ATD; BEGIN AT; }
<ATD>\"{A_STR}:{P_NUM}\"?	{ atcmdD(yytext, ATDA_STR, ATDP_NUM);
					cmdstat = CMDST_ATD; BEGIN AT; }
<ATD>\"{A_STR}:{P_STR}\"?	{ atcmdD(yytext, ATDA_STR, ATDP_STR);
					cmdstat = CMDST_ATD; BEGIN AT; }
<ATD>" "			{ /*ignore spaces*/ }
<ATD>.|\n			{ return cmdstat = CMDST_ERROR; }

%%

Cmdstat
cmdLex(const char *p)
{
	if (p == NULL) return CMDST_NOAT;
	ptr = p;
#ifdef FLEX_SCANNER
	yyrestart(yyin);
#endif
	cmdstat = CMDST_NOAT;
	yylex();
	return cmdstat;
}
