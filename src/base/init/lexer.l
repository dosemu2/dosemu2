%{
#include <stdlib.h>
#undef ECHO
#include <termios.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <setjmp.h>
#include <sys/stat.h>                    /* structure stat       */
#include <unistd.h>                      /* prototype for stat() */

#include "config.h"
#include "emu.h"
#include "cpu.h"
#include "disks.h"
#include "lpt.h"
#include "video.h"
#include "mouse.h"
#include "serial.h"
#include "timers.h"
#include "keymaps.h"

#include "parser.h"
int line_count;

static int __active__=1, __next_ITEM_for_me__=0;
#define __active_max 15
static int __active_stack[__active_max+1]={1,0};
static int __active_i=1;

static void push__active();
static void pop__active();
static void enter_includefile(char * fname);
extern char *get_config_variable(char *name);

#if 0 /* this just to test the conditional code */
  #define TESTACTIVE (({if(__active__)fprintf(stderr,">>>%s<<<\n", yytext);}),__active__)
#else
  #define TESTACTIVE __active__
#endif

#define RETURN if (TESTACTIVE) return
#define MAY_BE  if (TESTACTIVE)

#define MAY_BEFORME if (__next_ITEM_for_me__) { \
  if (get_config_variable(yytext)) { \
    __active__= (__next_ITEM_for_me__ > 0); \
  } \
  else __active__= (__next_ITEM_for_me__ < 0); \
  __next_ITEM_for_me__=0; \
} \
else if (TESTACTIVE)

#define MAY_BEINCLUDE(other_stuff) if (TESTACTIVE) { \
  if (__next_ITEM_for_me__ == 2) { \
    __next_ITEM_for_me__=0; \
    yytext[strlen(yytext)-1] = '\0'; \
    enter_includefile(&yytext[1]); \
  } \
  else other_stuff \
}



extern char* strdup(const char *); /* Not defined in string.h :-( */
extern void yyerror(char* string, ...);

char *yy_vbuffer=0;

#define YY_INPUT(buf,result,max_size) \
  if (!yyin) { \
    if (yy_vbuffer && yy_vbuffer[0]) { \
      buf[(max_size)-1]=0; \
      strncpy(buf,yy_vbuffer,max_size); \
      if (buf[(max_size)-1]) { \
        yy_vbuffer+=max_size; \
        result=max_size; \
      } \
      else { \
        result=strlen(buf); \
        yy_vbuffer=0; \
      } \
    } \
    else result=0; \
  } \
  else { \
    if ( yy_current_buffer->yy_is_interactive ) { \
      int c = getc( yyin ); \
      result = c == EOF ? 0 : 1; \
      buf[0] = (char) c; \
    } \
    else { \
      if ( ((result = fread( buf, 1, max_size, yyin )) == 0) && ferror( yyin ) ) \
                  YY_FATAL_ERROR( "input in flex scanner failed" ); \
    } \
  }


#undef YY_DECL
#define YY_DECL int yylex YY_PROTO(( YYSTYPE* yylval ))
%}

%%

	/* special characters */

"{"|"}"			RETURN(yytext[0]);


	/* conditionals */

"ifdef"			{ push__active(); if (__active__) __next_ITEM_for_me__=1; }
"ifndef"		{ push__active(); if (__active__) __next_ITEM_for_me__=-1; }
"else"			{ 
			  if (__active__) __active__=0;
			  else if (__active_stack[__active_i-1] ) __active__=1;
			}
"endif"			pop__active();
"include"		MAY_BE __next_ITEM_for_me__=2;

	/* config variable settings */
"define"		RETURN(DEFINE);
"undef"			RETURN(UNDEF);

	/* keywords */

abort			RETURN(ABORT);
warn			RETURN(WARN);
x			RETURN(L_X);
dosbanner		RETURN(DOSBANNER);
fastfloppy		RETURN(FASTFLOPPY);
timint			RETURN(TIMINT);
hogthreshold		RETURN(HOGTHRESH);
speaker			RETURN(SPEAKER);
ipxsupport		RETURN(IPXSUPPORT);
pktdriver		RETURN(PKTDRIVER);
debug			RETURN(DEBUG);
mouse			RETURN(MOUSE);
serial			RETURN(SERIAL);
keyboard		RETURN(KEYBOARD);
keystroke		RETURN(PRESTROKE);
terminal		RETURN(TERMINAL);
video			RETURN(VIDEO);
allowvideoportaccess	RETURN(ALLOWVIDEOPORT);
mathco			RETURN(MATHCO);
cpu			RETURN(CPU);
bootA			RETURN(BOOTA);
bootC			RETURN(BOOTC);
xms			RETURN(L_XMS);
umb_max			RETURN(L_UMB);
secure			RETURN(L_SECURE);
ems			RETURN(L_EMS);
dpmi			RETURN(L_DPMI);
dosmem			RETURN(DOSMEM);
ports			RETURN(PORTS);
sillyint		RETURN(SILLYINT);
irqpassing		RETURN(SILLYINT);
hardware_ram		RETURN(HARDWARE_RAM);
disk			RETURN(DISK);
bootdisk		RETURN(BOOTDISK);
bootfile		RETURN(BOOTFILE);
printer			RETURN(PRINTER);
emusys                  RETURN(EMUSYS);
emubat                  RETURN(EMUBAT);
emuini                  RETURN(EMUINI);
ttylocks		RETURN(TTYLOCKS);
sound_emu               RETURN(L_SOUND);
dosemumap               RETURN(DOSEMUMAP);
logbufsize              RETURN(LOGBUFSIZE);

	/* sillyint values */
use_sigio		RETURN(USE_SIGIO);

	/* ems values */
ems_size		RETURN(EMS_SIZE);
ems_frame		RETURN(EMS_FRAME);

	/* speaker values */

emulated		RETURN(EMULATED);
native			RETURN(NATIVE);

	/* disk keywords */
hdimage			RETURN(HDIMAGE);
image			RETURN(HDIMAGE);
partition		RETURN(L_PARTITION);
wholedisk		RETURN(WHOLEDISK);
readonly		RETURN(READONLY);
threeinch		RETURN(THREEINCH);
fiveinch		RETURN(FIVEINCH);
sectors			RETURN(SECTORS);
cylinders		RETURN(CYLINDERS);
tracks			RETURN(TRACKS);
heads			RETURN(HEADS);
offset			RETURN(OFFSET);
floppy			RETURN(L_FLOPPY);

	/* keyboard - !!conflict between 'no' and boolean 'no' */

layout			RETURN(LAYOUT);
keybint			RETURN(KEYBINT);
rawkeyboard		RETURN(RAWKEYBOARD);

finnish			MAY_BE { yylval->i_value = KEYB_FINNISH;
			  return(KEYB_LAYOUT); }
finnish-latin1		MAY_BE { yylval->i_value = KEYB_FINNISH_LATIN1;
			  return(KEYB_LAYOUT); }
us			MAY_BE { yylval->i_value = KEYB_US; return(KEYB_LAYOUT); }
uk			MAY_BE { yylval->i_value = KEYB_UK; return(KEYB_LAYOUT); }
de			MAY_BE { yylval->i_value = KEYB_DE; return(KEYB_LAYOUT); }
de-latin1		MAY_BE { yylval->i_value = KEYB_DE_LATIN1;
			  return(KEYB_LAYOUT); }
fr			MAY_BE { yylval->i_value = KEYB_FR; return(KEYB_LAYOUT); }
fr-latin1		MAY_BE { yylval->i_value = KEYB_FR_LATIN1;
			  return(KEYB_LAYOUT); }
dk			MAY_BE { yylval->i_value = KEYB_DK; return(KEYB_LAYOUT); }
dk-latin1		MAY_BE { yylval->i_value = KEYB_DK_LATIN1;
			  return(KEYB_LAYOUT); }
no			MAY_BE { yylval->i_value = KEYB_NO; return(KEYB_LAYOUT); }
no-latin1		MAY_BE { yylval->i_value = KEYB_NO_LATIN1;
			  return(KEYB_LAYOUT); }
dvorak			MAY_BE { yylval->i_value = KEYB_DVORAK; return(KEYB_LAYOUT); }
sg			MAY_BE { yylval->i_value = KEYB_SG; return(KEYB_LAYOUT); }
sg-latin1		MAY_BE { yylval->i_value = KEYB_SG_LATIN1;
			  return(KEYB_LAYOUT); }
sf			MAY_BE { yylval->i_value = KEYB_SF; return(KEYB_LAYOUT); }
sf-latin1		MAY_BE { yylval->i_value = KEYB_SF_LATIN1;
			  return(KEYB_LAYOUT); }
es			MAY_BE { yylval->i_value = KEYB_ES; return(KEYB_LAYOUT); }
es-latin1		MAY_BE { yylval->i_value = KEYB_ES_LATIN1;
			  return(KEYB_LAYOUT); }
be			MAY_BE { yylval->i_value = KEYB_BE; return(KEYB_LAYOUT); }
po			MAY_BE { yylval->i_value = KEYB_PO; return(KEYB_LAYOUT); }
it			MAY_BE { yylval->i_value = KEYB_IT; return(KEYB_LAYOUT); }
sw			MAY_BE { yylval->i_value = KEYB_SW; return(KEYB_LAYOUT); }
hu			MAY_BE { yylval->i_value = KEYB_HU; return(KEYB_LAYOUT); }
hu-cwi			MAY_BE { yylval->i_value = KEYB_HU_CWI; return(KEYB_LAYOUT); }
hu-latin2		MAY_BE { yylval->i_value = KEYB_HU_LATIN2;
			  return(KEYB_LAYOUT); }

	/* serial stuff */

base			RETURN(BASE);
irq			RETURN(IRQ);
interrupt		RETURN(INTERRUPT);
baudrate		RETURN(BAUDRATE);
device			RETURN(DEVICE);
com                     RETURN(COM);

	/* lock file stuff */
directory              RETURN(DIRECTORY);
namestub               RETURN(NAMESTUB);
binary                 RETURN(BINARY);

	/* terminal stuff */

charset			RETURN(CHARSET);
updatefreq		RETURN(UPDATEFREQ);
	/* updatelines		RETURN(UPDATELINES); */
color			RETURN(COLOR);
escchar			RETURN(ESCCHAR);
	/* corner			RETURN(CORNER); */
	/* method			RETURN(METHOD); */
	/* normal			RETURN(NORMAL); */
	/* xterm			RETURN(XTERM); */
	/* fast			RETURN(FAST); */
	/* ncurses			RETURN(NCURSES); */
latin			MAY_BE { yylval->i_value = CHARSET_LATIN;
			  return(CHARSET_TYPE); }
ibm			MAY_BE { yylval->i_value = CHARSET_IBM;
			  return(CHARSET_TYPE); }
fullibm			MAY_BE { yylval->i_value = CHARSET_FULLIBM;
			  return(CHARSET_TYPE); }

	/* mouse types */

microsoft		RETURN(MICROSOFT);
ms3button		RETURN(MS3BUTTON);
logitech		RETURN(LOGITECH);
mmseries		RETURN(MMSERIES);
mouseman		RETURN(MOUSEMAN);
hitachi			RETURN(HITACHI);
mousesystems		RETURN(MOUSESYSTEMS);
busmouse		RETURN(BUSMOUSE);
ps2			RETURN(PS2);
internaldriver		RETURN(INTERNALDRIVER);
emulate3buttons		RETURN(EMULATE3BUTTONS);
cleardtr		RETURN(CLEARDTR);

	/* video stuff - sorry for Matrox but MGA was already used */

vga			RETURN(VGA);
ega			RETURN(EGA);
cga			RETURN(CGA);
mga			RETURN(MGA);
mda			RETURN(MGA);
console			RETURN(CONSOLE);
graphics		RETURN(GRAPHICS);
chipset			RETURN(CHIPSET);
memsize			RETURN(MEMSIZE);
fullrestore		RETURN(FULLREST);
partialrestore		RETURN(PARTREST);
vbios_file		RETURN(VBIOS_FILE);
vbios_copy		RETURN(VBIOS_COPY);
vbios_mmap		RETURN(VBIOS_MMAP);
vbios_seg		RETURN(VBIOS_SEG);
vbios_size		RETURN(VBIOS_SIZE_TOK);
dualmon			RETURN(DUALMON);
forcevtswitch		RETURN(FORCE_VT_SWITCH);
plainvga		MAY_BE { yylval->i_value = 0; return(CHIPSET_TYPE); }
trident			MAY_BE { yylval->i_value = 1; return(CHIPSET_TYPE); }
et4000			MAY_BE { yylval->i_value = 2; return(CHIPSET_TYPE); }
diamond			MAY_BE { yylval->i_value = 3; return(CHIPSET_TYPE); }
s3			MAY_BE { yylval->i_value = 4; return(CHIPSET_TYPE); }
avance			MAY_BE { yylval->i_value = 5; return(CHIPSET_TYPE); }
ati			MAY_BE { yylval->i_value = 6; return(CHIPSET_TYPE); }
cirrus			MAY_BE { yylval->i_value = 7; return(CHIPSET_TYPE); }
matrox			MAY_BE { yylval->i_value = 8; return(CHIPSET_TYPE); }

	/* xwindows stuff */

display			RETURN(L_DISPLAY);
title			RETURN(L_TITLE);
icon_name		RETURN(ICON_NAME);
keycode			RETURN(X_KEYCODE);
blinkrate		RETURN(X_BLINKRATE);
sharecmap		RETURN(X_SHARECMAP);
mitshm                  RETURN(X_MITSHM);
font			RETURN(X_FONT);
fixed_aspect		RETURN(X_FIXED_ASPECT);
aspect_43		RETURN(X_ASPECT_43);
lin_filt		RETURN(X_LIN_FILT);
bilin_filt		RETURN(X_BILIN_FILT);
mode13fact		RETURN(X_MODE13FACT);
winsize			RETURN(X_WINSIZE);
gamma			RETURN(X_GAMMA);
vgaemu_memsize		RETURN(VGAEMU_MEMSIZE);

        /* Sound stuff */

sb_base                 RETURN(SB_BASE);
sb_irq                  RETURN(SB_IRQ);
sb_dma                  RETURN(SB_DMA);
sb_dsp                  RETURN(SB_DSP);
sb_mixer                RETURN(SB_MIXER);
mpu_base                RETURN(MPU_BASE);

	/* packet driver */
	/* NOTE: we need 'no' _before_ 'novell_hack', else the rule will not match */
no			MAY_BE { yylval->i_value = 0; return(L_NO); }
novell_hack		RETURN(NOVELLHACK);

	/* debug flags */

io			RETURN(IO);
port			RETURN(PORT);
config			RETURN(CONFIG);
read			RETURN(READ);
write			RETURN(WRITE);
keyb			RETURN(KEYB);
warning			RETURN(WARNING);
general			RETURN(GENERAL);
hardware		RETURN(HARDWARE);
ipc			RETURN(L_IPC);
network			RETURN(NETWORK);
sound			RETURN(SOUND);

	/* printer stuff */

command			RETURN(COMMAND);
timeout			RETURN(TIMEOUT);
options			RETURN(OPTIONS);
file			RETURN(L_FILE);

	/* port/io stuff */

ormask			RETURN(ORMASK);
andmask			RETURN(ANDMASK);
rdonly			RETURN(RDONLY);
wronly			RETURN(WRONLY);
rdwr			RETURN(RDWR);
range			RETURN(RANGE);
fast			RETURN(FAST);

	/* dexe stuff */
dexe			RETURN(DEXE);
allowdisk		RETURN(ALLOWDISK);
forcexdos		RETURN(FORCEXDOS);
xdosonly		RETURN(XDOSONLY);

	/* boolean values */

on			MAY_BE { yylval->i_value = 1; return(L_ON); }
off			MAY_BE { yylval->i_value = 0; return(L_OFF); }
yes			MAY_BE { yylval->i_value = 1; return(L_YES); }
	/*no			MAY_BE { yylval->i_value = 0; return(L_NO); } */

	/* integers */

[0-9]+			MAY_BE { yylval->i_value = atoi(yytext); return(INTEGER); }
0x[0-9a-f]+		MAY_BE { char *end;
			  yylval->i_value = strtol(yytext, &end, 0);
			  return(INTEGER); }

	/* strings */

\"[^\"]*\"		MAY_BEINCLUDE ( { yytext[strlen(yytext)-1] = '\0';
			  yylval->s_value = strdup(&yytext[1]);
			  return(STRING); } )
\'[^\']*\'		MAY_BE { yytext[strlen(yytext)-1] = '\0';
			  yylval->s_value = strdup(&yytext[1]);
			  return(STRING); }
[a-z0-9/\~@-_\+=:,\.]+	MAY_BEFORME { yylval->s_value = strdup(yytext);
			  return(STRING); }
$[a-z0-9/\~@-_\+=:,\.]+	MAY_BE { yylval->s_value = strdup(getenv(&yytext[1]));
			  return(STRING); }

	/* comments & whitespace */

[#\;\!][^\n]*\n		line_count++;   /* comments to (and including) EOLN */
[ \t]+			;   		/* ignore all white space */
\n			line_count++;	/* keep track of lines seen */
	/* NOTE: the below rule will not have any effect, ... except */
	/* printing a warning ;-) */
	/*.			fprintf(stderr, "discarding char '%c'\n", yytext[0]); */

%%

#define MAX_INCLUDE_DEPTH 10
static YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH] ={0};
int include_stack_ptr = 0;
char * include_fnames[MAX_INCLUDE_DEPTH] = {0};
static int include_lines[MAX_INCLUDE_DEPTH] = {0};

static void enter_includefile(char * fname)
{
  FILE * new_yyin;
  char fname_[256];
  if ( include_stack_ptr >= MAX_INCLUDE_DEPTH ) {
    yyerror("Includes nested too deeply" );
    return;
  }
  /* we want to have the include files in the same directory as
   * the main config file, if we have no leading  '/'
   */
  if (fname[0] != '/')  {
    int i;
    strcpy(fname_,include_fnames[0]);
    i=strlen(fname_);
    while (i && (fname_[i] != '/')) i--;
    if (i) {
      i++;
      strcpy(fname_+i,fname);
      fname=fname_;
    }
  }
  if (get_config_variable("c_system")) enter_priv_on();
  else enter_priv_off();
  new_yyin = fopen( fname, "r" );
  leave_priv_setting();
  if ( ! new_yyin ) {
    yyerror("cannot open includefile %s", fname);
    return;
  }
  c_printf("CONF: opened include file %s\n", fname);
  include_lines[include_stack_ptr] = line_count;
  include_stack[include_stack_ptr] = YY_CURRENT_BUFFER;
  include_stack_ptr++;
  include_fnames[include_stack_ptr] = strdup(fname);
  line_count = 1;
  yyin = new_yyin;
  include_stack[include_stack_ptr] = yy_create_buffer( yyin, YY_BUF_SIZE );
  yy_switch_to_buffer(include_stack[include_stack_ptr]);
}


#ifdef yywrap
  error "yywrap defined elsewere, need our own one"
#endif

int yywrap(void)    /* this gets called at EOF of a parsed file */
{
  if (include_stack_ptr <= 0 ) return(1);
  yy_switch_to_buffer(include_stack[include_stack_ptr-1] );
  fclose(include_stack[include_stack_ptr]->yy_input_file);
  yy_delete_buffer(include_stack[include_stack_ptr]);
  c_printf("CONF: closed include file %s\n", include_fnames[include_stack_ptr]);
  free(include_fnames[include_stack_ptr]);
  include_stack_ptr--;
  line_count=include_lines[include_stack_ptr];
  return(0);
}


extern void yyerror(char* string, ...);

static void push__active()
{
  __active_stack[__active_i++]=__active__;
  if (__active_i > __active_max) {
    __active_i = __active_max;
    yyerror("Lexer block stack overflow, unmatching #if..#else..#endif");
  }
}

static void pop__active()
{
  if (__active_i <=0) {
    yyerror("Lexer block stack underflow, unmatching #if..#else..#endif");
    return;
  }
  __active__=__active_stack[--__active_i];
}

