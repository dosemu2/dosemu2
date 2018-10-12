/*
 * We are intercepting the yylex() function calls from the parser
 */
#ifndef LEXER_H
#define LEXER_H
#define OUR_YY_DECL int yylex (YYSTYPE* yylval)
OUR_YY_DECL;

extern void tell_lexer_if(int value);
extern void tell_lexer_loop(int cfile, int value);

#ifndef LEXER
extern void yyrestart(FILE *input_file);
extern FILE* yyin;
#endif

extern void yyerror(const char *, ...);
extern void yywarn(const char *, ...);
extern char *yy_vbuffer;
extern int include_stack_ptr;
extern char *include_fnames[];
extern int include_lines[];
extern int line_count;
extern int last_include;

#endif
