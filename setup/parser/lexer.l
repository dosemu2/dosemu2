/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

%{

#if 0
  #define HANDLE_COMMENTS
#endif

#define YY_NO_UNPUT		1
#if 0
#define YY_NEVER_INTERACTIVE	1
#endif

#include <stdlib.h>
#undef ECHO
#include <string.h>


#include "parser.h"
int line_count;
struct comment *comment_head = 0;
struct comment *comment_tail = 0;
int token_serial = 0;

#ifdef HANDLE_COMMENTS
  static void shift_comment(char *s);
  char *reduce_comment(int serial);
  #define SERIAL yylval->u.serial = token_serial++
#else
  #define SERIAL
  #define shift_comment(s)
#endif

#undef YY_DECL
#define YY_DECL int yylex YY_PROTO(( YYSTYPE* yylval ))


%}

DIGIT		[0-9]
HEXDIGIT 	{DIGIT}|[a-fA-F]
LETTER		[a-zA-Z]
ALPHNUM		{LETTER}|{DIGIT}
IDENT		_*{LETTER}({ALPHNUM}|_)*
STRQUOTELESS	({LETTER}|[/\.\~])({ALPHNUM}|[_\~@\-\+:\./])*

%%

	/* special characters */
[()<>=,\-+\*;]		{SERIAL; return(yytext[0]);}


	/* operators */
"/"			return('/');
"div"			return('/');
"|"			return(OR_OP);
"^"			return(XOR_OP);
">>"			return(SHR_OP);
"<<"			return(SHL_OP);
"!"			return(NOT_OP);
"=="			return(EQ_OP);
">="			return(GE_OP);
"<="			return(LE_OP);
"!="			return(NEQ_OP);
"&&"			return(L_AND_OP);
"&"			return(AND_OP);
"||"			return(L_OR_OP);
"~"			return(BIT_NOT_OP);
	/*
	"eq"			return(STR_EQ_OP);
	"ne"			return(STR_NEQ_OP);
	*/

	/* numbers */

{DIGIT}+		{SERIAL; yylval->s_value = strdup(yytext);
			return(INTEGER); }

0x{HEXDIGIT}+		{SERIAL; yylval->s_value = strdup(yytext);
			return(INTEGER); }

0b[01]+			{SERIAL; yylval->s_value = strdup(yytext);
			return(INTEGER); }

{DIGIT}+\.{DIGIT}*([eE]\-?{DIGIT}+)?  {SERIAL; yylval->s_value = strdup(yytext);
			return(REAL); }

on			{SERIAL; yylval->s_value = strdup(yytext); return(INTEGER); }
off			{SERIAL; yylval->s_value = strdup(yytext); return(INTEGER); }
auto			{SERIAL; yylval->s_value = strdup(yytext); return(INTEGER); }

${IDENT}		{SERIAL; yylval->s_value = strdup(&yytext[1]);
			return(VARIABLE);}

(\"[^\"]*\")|(\'[^\']*\') {SERIAL;
			yytext[strlen(yytext)-1] = '\0';
			yylval->s_value = strdup(&yytext[1]);
			if (yytext[0] == '"') return(STRING);
			else return(STRING1);
			}

	/* comments & whitespace */

				/* comments to (and including) EOLN */
[ \t]*[#][^\n]*\n		{line_count++; SERIAL; shift_comment(yytext);}

				/* ignore 'in between' white spaces */
[ \t]+			{SERIAL; shift_comment(yytext);}

				/* keep track of lines seen */
\n			{line_count++; SERIAL; shift_comment(yytext);}
.			; /* fprintf(stderr, "discarding char '%c'\n", yytext[0]); */


%%

#ifdef HANDLE_COMMENTS
struct comment {
	struct comment *next;
	int serial;
	char *s;
};

static void shift_comment(char *s)
{
	struct comment *new = malloc(sizeof(struct comment));
	if (!comment_tail) {
		comment_tail = comment_head = new;
	}
	else {
		comment_tail->next = new;
		comment_tail = new;
	}
	new->next = 0;
	new->s = strdup(s);
	new->serial = token_serial;
}

char *reduce_comment(int serial)
{
	struct comment *next;
	char *s;
	if (!comment_head) return 0;
	if (comment_head->serial > serial) return 0;
	next = comment_head->next;
	s = comment_head->s;
	free(comment_head);
	comment_head = next;
	return s;
}
#endif /* HANDLE_COMMENTS */


#ifndef yywrap
int yywrap(void)	/* do this to avoid needing to do -lfl */
{
	return(1);
}
#endif
