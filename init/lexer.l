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

extern char* strdup(const char *); /* Not defined in string.h :-( */

#undef YY_DECL
#define YY_DECL int yylex YY_PROTO(( YYSTYPE* yylval ))
%}

%%

	/* special characters */

"{"|"}"			return(yytext[0]);

	/* keywords */

x			return(L_X);
dosbanner		return(DOSBANNER);
fastfloppy		return(FASTFLOPPY);
timint			return(TIMINT);
hogthreshold		return(HOGTHRESH);
speaker			return(SPEAKER);
ipxsupport		return(IPXSUPPORT);
pktdriver		return(PKTDRIVER);
debug			return(DEBUG);
mouse			return(MOUSE);
serial			return(SERIAL);
keyboard		return(KEYBOARD);
terminal		return(TERMINAL);
video			return(VIDEO);
allowvideoportaccess	return(ALLOWVIDEOPORT);
mathco			return(MATHCO);
cpu			return(CPU);
bootA			return(BOOTA);
bootC			return(BOOTC);
xms			return(L_XMS);
umb_max			return(L_UMB);
ems			return(L_EMS);
dpmi			return(L_DPMI);
dosmem			return(DOSMEM);
ports			return(PORTS);
sillyint		return(SILLYINT);
hardware_ram		return(HARDWARE_RAM);
disk			return(DISK);
bootdisk		return(BOOTDISK);
printer			return(PRINTER);
emusys                  return(EMUSYS);
emubat                  return(EMUBAT);
ttylocks		return(TTYLOCKS);

	/* sillyint values */
use_sigio		return(USE_SIGIO);

	/* ems values */
ems_size		return(EMS_SIZE);
ems_frame		return(EMS_FRAME);

	/* speaker values */

emulated		return(EMULATED);
native			return(NATIVE);

	/* disk keywords */
hdimage			return(HDIMAGE);
image			return(HDIMAGE);
partition		return(L_PARTITION);
wholedisk		return(WHOLEDISK);
readonly		return(READONLY);
threeinch		return(THREEINCH);
fiveinch		return(FIVEINCH);
sectors			return(SECTORS);
cylinders		return(CYLINDERS);
tracks			return(TRACKS);
heads			return(HEADS);
offset			return(OFFSET);
floppy			return(L_FLOPPY);

	/* keyboard */

layout			return(LAYOUT);
keybint			return(KEYBINT);
rawkeyboard		return(RAWKEYBOARD);

finnish			{ yylval->i_value = KEYB_FINNISH;
			  return(KEYB_LAYOUT); }
finnish-latin1		{ yylval->i_value = KEYB_FINNISH_LATIN1;
			  return(KEYB_LAYOUT); }
us			{ yylval->i_value = KEYB_US; return(KEYB_LAYOUT); }
uk			{ yylval->i_value = KEYB_UK; return(KEYB_LAYOUT); }
de			{ yylval->i_value = KEYB_DE; return(KEYB_LAYOUT); }
de-latin1		{ yylval->i_value = KEYB_DE_LATIN1;
			  return(KEYB_LAYOUT); }
fr			{ yylval->i_value = KEYB_FR; return(KEYB_LAYOUT); }
fr-latin1		{ yylval->i_value = KEYB_FR_LATIN1;
			  return(KEYB_LAYOUT); }
dk			{ yylval->i_value = KEYB_DK; return(KEYB_LAYOUT); }
dk-latin1		{ yylval->i_value = KEYB_DK_LATIN1;
			  return(KEYB_LAYOUT); }
dvorak			{ yylval->i_value = KEYB_DVORAK; return(KEYB_LAYOUT); }
sg			{ yylval->i_value = KEYB_SG; return(KEYB_LAYOUT); }
sg-latin1		{ yylval->i_value = KEYB_SG_LATIN1;
			  return(KEYB_LAYOUT); }
sf			{ yylval->i_value = KEYB_SF; return(KEYB_LAYOUT); }
sf-latin1		{ yylval->i_value = KEYB_SF_LATIN1;
			  return(KEYB_LAYOUT); }
es			{ yylval->i_value = KEYB_ES; return(KEYB_LAYOUT); }
es-latin1		{ yylval->i_value = KEYB_ES_LATIN1;
			  return(KEYB_LAYOUT); }
be			{ yylval->i_value = KEYB_BE; return(KEYB_LAYOUT); }
po                      { yylval->i_value = KEYB_PO; return(KEYB_LAYOUT); }
it                      { yylval->i_value = KEYB_IT; return(KEYB_LAYOUT); }
sw			{ yylval->i_value = KEYB_SW; return(KEYB_LAYOUT); }

	/* serial stuff */

base			return(BASE);
irq			return(IRQ);
interrupt		return(INTERRUPT);
baudrate		return(BAUDRATE);
device			return(DEVICE);
com                     return(COM);

	/* lock file stuff */
directory              return(DIRECTORY);
namestub               return(NAMESTUB);
binary                 return(BINARY);

	/* terminal stuff */

charset			return(CHARSET);
updatefreq		return(UPDATEFREQ);
	/* updatelines		return(UPDATELINES); */
color			return(COLOR);
escchar			return(ESCCHAR);
	/* corner			return(CORNER); */
	/* method			return(METHOD); */
	/* normal			return(NORMAL); */
	/* xterm			return(XTERM); */
	/* fast			return(FAST); */
	/* ncurses			return(NCURSES); */
latin			{ yylval->i_value = CHARSET_LATIN;
			  return(CHARSET_TYPE); }
ibm			{ yylval->i_value = CHARSET_IBM;
			  return(CHARSET_TYPE); }
fullibm			{ yylval->i_value = CHARSET_FULLIBM;
			  return(CHARSET_TYPE); }

	/* mouse types */

microsoft		return(MICROSOFT);
logitech		return(LOGITECH);
mmseries		return(MMSERIES);
mouseman		return(MOUSEMAN);
hitachi			return(HITACHI);
mousesystems		return(MOUSESYSTEMS);
busmouse		return(BUSMOUSE);
ps2			return(PS2);
internaldriver		return(INTERNALDRIVER);
emulate3buttons		return(EMULATE3BUTTONS);
cleardtr		return(CLEARDTR);

	/* video stuff */

vga			return(VGA);
ega			return(EGA);
cga			return(CGA);
mga			return(MGA);
mda			return(MGA);
console			return(CONSOLE);
graphics		return(GRAPHICS);
chipset			return(CHIPSET);
memsize			return(MEMSIZE);
fullrestore		return(FULLREST);
partialrestore		return(PARTREST);
vbios_file		return(VBIOS_FILE);
vbios_copy		return(VBIOS_COPY);
vbios_mmap		return(VBIOS_MMAP);
vbios_seg		return(VBIOS_SEG);
vbios_size		return(VBIOS_SIZE);
dualmon			return(DUALMON);
plainvga		{ yylval->i_value = 0; return(CHIPSET_TYPE); }
trident			{ yylval->i_value = 1; return(CHIPSET_TYPE); }
et4000			{ yylval->i_value = 2; return(CHIPSET_TYPE); }
diamond			{ yylval->i_value = 3; return(CHIPSET_TYPE); }
s3			{ yylval->i_value = 4; return(CHIPSET_TYPE); }

	/* xwindows stuff */

display			return(L_DISPLAY);
title			return(L_TITLE);
icon_name		return(ICON_NAME);
keycode			return(X_KEYCODE);
blinkrate		return(X_BLINKRATE);
font			return(X_FONT);

	/* packet driver */

novell_hack		return(NOVELLHACK);

	/* debug flags */

io			return(IO);
port			return(PORT);
config			return(CONFIG);
read			return(READ);
write			return(WRITE);
keyb			return(KEYB);
warning			return(WARNING);
general			return(GENERAL);
hardware		return(HARDWARE);
ipc			return(L_IPC);
network			return(NETWORK);

	/* printer stuff */

command			return(COMMAND);
timeout			return(TIMEOUT);
options			return(OPTIONS);
file			return(L_FILE);

	/* port/io stuff */

ormask			return(ORMASK);
andmask			return(ANDMASK);
rdonly			return(RDONLY);
wronly			return(WRONLY);
rdwr			return(RDWR);
range			return(RANGE);

	/* boolean values */

on			{ yylval->i_value = 1; return(L_ON); }
off			{ yylval->i_value = 0; return(L_OFF); }
yes			{ yylval->i_value = 1; return(L_YES); }
no			{ yylval->i_value = 0; return(L_NO); }

	/* integers */

[0-9]+			{ yylval->i_value = atoi(yytext); return(INTEGER); }
0x[0-9a-f]+		{ char *end;
			  yylval->i_value = strtol(yytext, &end, 0);
			  return(INTEGER); }

	/* strings */

\"[^\"]*\"		{ yytext[strlen(yytext)-1] = '\0';
			  yylval->s_value = strdup(&yytext[1]);
			  return(STRING); }
\'[^\']*\'		{ yytext[strlen(yytext)-1] = '\0';
			  yylval->s_value = strdup(&yytext[1]);
			  return(STRING); }
[a-z0-9/\~@-_\+=:,\.]+  { yylval->s_value = strdup(yytext);
			  return(STRING); }
$[a-z0-9/\~@-_\+=:,\.]+ { yylval->s_value = strdup(getenv(&yytext[1]));
			  return(STRING); }

	/* comments & whitespace */

[#\;\!][^\n]*\n		line_count++;   /* comments to (and including) EOLN */
[ \t]+			;   		/* ignore all white space */
\n			line_count++;	/* keep track of lines seen */
.			fprintf(stderr, "discarding char '%c'\n", yytext[0]);

%%

#ifndef yywrap
  int yywrap(void)    /* do this to avoid needing to do -lfl */
     {
     return(1);
     }
#endif
