/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

#define SERIAL(v) (((YYSTYPE *)(&(v)))->u.serial)

static char *parsedfile = 0;
static char *updatefile = 0;
static char *activefile = 0;

static int errors = 0;
static int updatemode = 0;
static int option_t = 0;
static int option_rc = 0;

static char *dosemurc_varlist[] = {
	"_debug", "_timint", "_mathco", "_cpu", "_pci", "_xms", "_ems", "_ems_frame",
	"_features", "_mapping",
	"_emusys", "_emuini",
	"_term_char_set", "_term_color", "_term_updfreq", "_escchar", "_layout",
	"_X_updfreq", "_X_title", "_X_icon_name", "_X_keycode", "_X_blinkrate",
	"_X_font", "_X_mitshm", "_X_sharecmap", "_X_fixed_aspect", "_X_aspect_43",
	"_X_lin_filt", "_X_bilin_filt", "_X_mode13fact", "_X_winsize", "_X_gamma",
	"_X_vgaemu_memsize", "_X_lfb", "_X_pm_interface", "_X_mgrab_key", "_X_vesamode",
	"_speaker",
	"_com1", "_com2", "_com3", "_com4", "_mouse", "_mouse_dev", "_mouse_flags", "_mouse_baud",
	"_printer", "_printer_timeout", "_ports",
	"_ipxsupport", "_novell_hack", "_vnet",
	"_sound", "_sb_base", "_sb_irq", "_sb_dma", "_sb_hdma", "_sb_dsp", "_sb_mixer", "_mpu_base",
	"_hogthreshold", "_dpmi", "_vbootfloppy", "_hdimage", "_ttylocks",
	"_aspi",
	NULL
};

	/* external procedures */
extern int yylex(); /* exact argument types depend on the way you call bison */

	/* variables in lexer.l */
extern int line_count;
extern FILE* yyin;
extern void yyrestart(FILE *input_file);

	/* local procedures */
static void yyerror(char *, ...);
static void handle_assignment(char *typ, char *var, char *s);
static char *joinstrings(char *frmt, ...);

%}

%start lines

%union {
	char *s_value;
	struct {
		int dummy;
		int serial;
	} __attribute__((packed)) u;
};

%token <s_value> INTEGER L_OFF L_ON L_AUTO
%token <s_value> REAL
%token <s_value> STRING STRING1 VARIABLE
%type  <s_value> expression strarglist strarglist_item
%left AND_OP OR_OP XOR_OP SHR_OP SHL_OP
%left NOT_OP /* logical NOT */
%left EQ_OP GE_OP LE_OP '=' '<' '>' NEQ_OP
	/*
	%left STR_EQ_OP STR_NEQ_OP
	*/
%left L_AND_OP L_OR_OP
%left '+' '-'
%left '*' '/'
%left UMINUS UPLUS BIT_NOT_OP

%%


lines:		  line
		| lines line
		;

line:		  VARIABLE '=' '(' expression ')' {handle_assignment("N ", $1, joinstrings("(%s)",$4,0));}
		| VARIABLE '=' strarglist  {handle_assignment("S ", $1, $3);}
		;

expression:	  INTEGER
		| REAL
		| VARIABLE		{$$ = joinstrings("$%s",$1,0);}
		| expression '+' expression {$$ = joinstrings("%s + %%s",$1,$3,0);}
		| expression '-' expression {$$ = joinstrings("%s - %%s",$1,$3,0);}
		| expression '*' expression {$$ = joinstrings("%s * %%s",$1,$3,0);}
		| expression '/' expression {$$ = joinstrings("%s / %%s",$1,$3,0);}
		| '-' expression %prec UMINUS {$$ = joinstrings("-%s",$2,0);}
		| '+' expression %prec UPLUS {$$ = joinstrings("+%s",$2,0);}
		| expression AND_OP expression {$$ = joinstrings("%s & %%s",$1,$3,0);}
		| expression OR_OP expression {$$ = joinstrings("%s | %%s",$1,$3,0);}
		| expression XOR_OP expression {$$ = joinstrings("%s ^ %%s",$1,$3,0);}
		| BIT_NOT_OP expression %prec BIT_NOT_OP {$$ = joinstrings("~%s",$2,0);}
		| expression SHR_OP expression {$$ = joinstrings("%s >> %%s",$1,$3,0);}
		| expression SHL_OP expression {$$ = joinstrings("%s << %%s",$1,$3,0);}
		| expression EQ_OP expression {$$ = joinstrings("%s == %%s",$1,$3,0);}
		| expression NEQ_OP expression {$$ = joinstrings("%s != %%s",$1,$3,0);}
		| expression GE_OP expression {$$ = joinstrings("%s >= %%s",$1,$3,0);}
		| expression LE_OP expression {$$ = joinstrings("%s <= %%s",$1,$3,0);}
		| expression '<' expression {$$ = joinstrings("%s < %%s",$1,$3,0);}
		| expression '>' expression {$$ = joinstrings("%s > %%s",$1,$3,0);}
		| expression L_AND_OP expression {$$ = joinstrings("%s && %%s",$1,$3,0);}
		| expression L_OR_OP expression {$$ = joinstrings("%s || %%s",$1,$3,0);}
		| NOT_OP expression {$$ = joinstrings("!%s",$2,0);}
		| '(' expression ')'	{$$ = joinstrings("(%s)",$2,0);}
		;

strarglist:	  strarglist_item
		| strarglist ',' strarglist_item {$$ = joinstrings("%s, %s", $1,$3,0);}

strarglist_item:  STRING		{$$ = joinstrings("\"%s\"",$1,0);}
		| STRING1		{$$ = joinstrings("'%s'",$1,0);}
		| VARIABLE		{$$ = joinstrings("$%s",$1,0);}
		;

%%

struct updaterec {
	struct updaterec *next;
	char *var;
	char *s;
	int taken;
};

static struct updaterec *update_head = 0;

static struct updaterec *lookup_update(char *var)
{
	struct updaterec *rec = update_head;
	while (rec) {
		if (!strcmp(rec->var,var) && !rec->taken) {
			rec->taken = 1;
			return rec;
		}
		rec = rec->next;
	}
	return 0;
}

static int valid_var(char *var)
{
	char **s = dosemurc_varlist;
	if (!option_rc || !var) return 1;
	while (*s) {
		if (!strcmp(*s, var)) return 1;
		s++;
	}
	return 0;
}

static void handle_assignment(char *typ, char *var, char *s)
{
	if (!option_t || !typ) typ = "";
	if (!valid_var(var)) {
		if (var) {
			free(var);
			free(s);
		}
		return;
	}
	if (!var) {
		/* flush not yet taken updates */
		struct updaterec *rec = update_head;
		if (!updatemode) return;
		while (rec) {
			if (!rec->taken) {
				printf("%s$%s = %s\n", typ, rec->var, rec->s);
			}
			rec = rec->next;
		}
		return;
	}
	switch (updatemode) {
		case 0: {
			printf("%s$%s = %s\n", typ, var, s);
			free(var), free(s);
			break;
		}
		case 1: {
			struct updaterec *rec = malloc(sizeof(struct updaterec));
			rec->var = var;
			rec->s = s;
			rec->taken = 0;
			rec->next = update_head;
			update_head = rec;
			break;
		}
		case 2: {
			char *s_ = s;
			struct updaterec *rec = lookup_update(var);
			if (rec) s_ = rec->s;
			printf("%s$%s = %s\n", typ, var, s_);
			free(var), free(s);
			break;
		}
	}
}


static char *joinstrings(char *frmt, ...)
{
	va_list ap;
	char buf[4096];
	char *s;
	va_start(ap, frmt);

	vsprintf(buf, frmt, ap);
	while ((s = va_arg(ap, char *)) !=0) {
		free(s);
	}
	va_end(ap);
	return s = strdup(buf);
}

static void yyerror(char* string, ...)
{
  va_list vars;
  va_start(vars, string);
  fprintf(stderr, "Error in %s: (line %.3d) ", activefile, line_count);
  vfprintf(stderr, string, vars);
  fprintf(stderr, "\n");
  va_end(vars);
  errors++;
}

static void usage(void)
{
	fprintf(stderr,"
USAGE:
  parser [-r] [-t] -p filetoparse [-u updatesfile]

  It works as follows: First step is to parse a given /etc/dosemu.conf or
  .dosemurc (-r) and get all variables extracted, each variable at one line.
  For this you use

    parser [-r] -p /etc/dosemu.conf >tmpfile

  If -t is also set, then 'N ' for numeric and 'S ' for string prefixes each
  variable assigment.

  In the second (or any number of additional steps) you merge any changed/new
  variable line in to the config file via

    parser [-r] -p /etc/dosemu.conf -u vars_to_update >tmpfile.out
    mv -f tmpfile.out /etc/dosemu.conf

  You need to supply the changed/added variables in vars_to_update, and this
  in the same format you got them in 'tmpfile', but without the -t generated
  type-prefixes ('N ' or 'S ').

  The parser currently cannot handle comments, hence those are lost :-(
"
	);
	exit(1);
}

static int filesize(char *name)
{
	struct stat s;
	if (stat(name, &s)) return -1;
	return (int)s.st_size;
}

static int valid_file(char *name)
{
	return ((name[0] == '-') || (filesize(name) > 0));
}

int main(int argc, char **argv)
{
	int ret = 0;
	int stdin_in_use = 0;
	int valid_parsedfile;
	char c;
#if YYDEBUG != 0
	extern int yydebug;
	yydebug  = 1;
#endif

	optind = 0;
	while ((c = getopt(argc, argv, "p:u:rt")) !=EOF) {
		switch (c) {
			case 'p': parsedfile = optarg; break;
			case 'u': updatefile = optarg; break;
			case 't': option_t = 1; break;
			case 'r': option_rc = 1; break;
			default: usage();
		}
	}

	if (!parsedfile) usage();
	valid_parsedfile = valid_file(parsedfile);
	if (updatefile && valid_file(updatefile)) {
		activefile = updatefile;
		if (updatefile[0] == '-') {
			yyin = stdin;
			stdin_in_use = 1;
			activefile = "STDIN";
		}
		else yyin = fopen(updatefile, "r");
		if (yyin < 0) {
			fprintf(stderr, "cannot open file %s\n", updatefile);
			exit(1);
		}
		if (valid_parsedfile) updatemode = 1;
		ret = yyparse();
		if (yyin != stdin) fclose(yyin);
		if (ret) {
			yyerror("finished parsing updates with %d errors\n", errors);
			exit(ret);
		}
		if (valid_parsedfile) updatemode = 2;
	}
	if (valid_parsedfile) {
		activefile = parsedfile;
		if (parsedfile[0] == '-') {
			if (stdin_in_use) {
				fprintf(stderr, "stdin already in use by -u\n");
				exit(1);
			}
			activefile = "STDIN";
			yyin = stdin;
		}
		else yyin = fopen(parsedfile, "r");
		if (yyin < 0) {
			fprintf(stderr, "cannot open file %s\n", parsedfile);
			exit(1);
		}
		if (updatemode) yyrestart(yyin);
		ret = yyparse();
		if (yyin != stdin) fclose(yyin);
		if (ret) {
			yyerror("finished parsing with %d errors\n", errors);
			exit(ret);
		}
		handle_assignment(0,0,0);
	}
	return ret;
}

