%{
/*
 * $Date: 1993/11/12 12:41:40 $
 * $Source: /home/src/dosemu0.50/parse/RCS/scan.l,v $
 * $Revision: 1.1 $
 * $State: Exp $
 */


#include <stdio.h>
#include <stdlib.h>

#include "parse.tab.h"
%}

DIGIT		[0-9]
HEXDIGIT	[A-Za-z0-9]
OCTDIGIT	[0-7]
LET		[A-Za-z_]
ALNUM		[A-Za-z_0-9]

%%

{DIGIT}+|"0x"{HEXDIGIT}+|"0"{OCTDIGIT}+ {
	yylval.val = strtoul(yytext, NULL, 0);
	printf("\nNUM: %d 0x%x\n", yylval.val, yylval.val);
	}

ports	{ printf("...%s...\n", yytext); yylval.name = yytext; return PORTS; }
disk	{ return DISK; }

quit			{ exit(1); }

{LET}{ALNUM}+	{ yylval.name = yytext; return WORD; }

"#".*		{ printf("COM_EOL #\n"); }
"!".*		{ printf("COM_EOL !\n"); }

[ \t\n]			;

"/*".*"*/"		{ printf("\nCOMMENT\n"); }

"{"|"}"|"#"		return yytext[0];

.			return yytext[0];

%%

