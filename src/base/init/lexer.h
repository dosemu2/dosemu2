/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * We are intercepting the yylex() function calls from the parser
 */ 
#define OUR_YY_DECL int yylex (YYSTYPE* yylval)
OUR_YY_DECL;

extern void tell_lexer_if(int value);
extern void tell_lexer_loop(int cfile, int value);

#ifndef LEXER
extern void yyrestart(FILE *input_file);
extern FILE* yyin;
#endif

extern void yyerror(char *, ...);
extern void yywarn(char *, ...);
extern char *yy_vbuffer;
extern int include_stack_ptr;
extern char *include_fnames[];
extern int include_lines[];
extern int line_count;
extern int last_include;

extern int yyget_lineno(void);
extern void yyset_lineno(int line_number);
extern void yyset_out(FILE * out_str);
extern void yyset_in(FILE * out_str);
extern void yyset_debug(int bdebug );
extern int yyget_debug(void);
extern int yylex_destroy(void);
extern FILE* yyget_in(void);
extern FILE* yyget_out(void);
extern int yyget_leng(void);
extern char *yyget_text(void);
