%{
#include <stdlib.h>
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

#undef YY_DECL
#define YY_DECL int yylex YY_PROTO(( YYSTYPE* yylval ))
%}

%%

	/* special characters */

"{"|"}"			return(yytext[0]);

	/* keywords */

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
ems			return(L_EMS);
dpmi			return(L_DPMI);
dosmem			return(DOSMEM);
ports			return(PORTS);
disk			return(DISK);
bootdisk		return(BOOTDISK);
printer			return(PRINTER);

	/* integers */

[0-9]+			{ yylval->i_value = atoi(yytext); return(INTEGER); }
0x[0-9a-f]+		{ char *end;
			  yylval->i_value = strtol(yytext, &end, 0);
			  return(INTEGER); }

	/* strings */

\/dev\/[a-z0-9]+	{ yylval->s_value = yytext; return(STRING); }
\"[^\"]*\"		{ yylval->s_value = &yytext[1];
			  yytext[strlen(yytext)-1] = '\0';
			  return(STRING); }

	/* boolean values */

on			{ yylval->i_value = 1; return(L_ON); }
off			{ yylval->i_value = 0; return(L_OFF); }
yes			{ yylval->i_value = 1; return(L_YES); }
no			{ yylval->i_value = 0; return(L_NO); }

	/* speaker values */

emulated		{ yylval->i_value = 2; return(L_SPEAKER); }
native			{ yylval->i_value = 1; return(L_SPEAKER); }

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
heads			return(HEADS);
offset			return(OFFSET);
floppy			return(L_FLOPPY);

	/* keyboard */

layout			return(LAYOUT);
keybint			return(KEYBINT);
rawkeyboard		return(RAWKEYBOARD);

finnish			{ yylval->i_value = KEYB_FINNISH;
			  return(KEYB_LAYOUT); }
finnish-latin1\,1	{ yylval->i_value = KEYB_FINNISH_LATIN1;
			  return(KEYB_LAYOUT); }
us			{ yylval->i_value = KEYB_US; return(KEYB_LAYOUT); }
uk			{ yylval->i_value = KEYB_UK; return(KEYB_LAYOUT); }
gr			{ yylval->i_value = KEYB_GR; return(KEYB_LAYOUT); }
gr-latin1		{ yylval->i_value = KEYB_GR_LATIN1;
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

	/* serial stuff */

base			return(BASE);
irq			return(IRQ);
interrupt		return(INTERRUPT);
baudrate		return(BAUDRATE);
device			return(DEVICE);

	/* terminal stuff */

charset			return(CHARSET);
updatefreq		return(UPDATEFREQ);
updatelines		return(UPDATELINES);
color			return(COLOR);
corner			return(CORNER);
method			return(METHOD);
fast			{ yylval->i_value = 1; return(METHOD_TYPE); }
ncurses			{ yylval->i_value = 2; return(METHOD_TYPE); }
latin			{ yylval->i_value = 1; return(CHARSET_TYPE); }
ibm			{ yylval->i_value = 2; return(CHARSET_TYPE); }
fullibm			{ yylval->i_value = 3; return(CHARSET_TYPE); }

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
cleardtr		return(CLEARDTR);

	/* video stuff */

vga			return(VGA);
ega			return(EGA);
cga			return(CGA);
mga			return(MGA);
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
plainvga		{ yylval->i_value = 0; return(CHIPSET_TYPE); }
trident			{ yylval->i_value = 1; return(CHIPSET_TYPE); }
et4000			{ yylval->i_value = 2; return(CHIPSET_TYPE); }
diamond			{ yylval->i_value = 3; return(CHIPSET_TYPE); }
s3			{ yylval->i_value = 4; return(CHIPSET_TYPE); }

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

	/* comments & whitespace */

[#\;\!][^\n]*\n		line_count++;	/* comments to EOLN */
[ \t]+			;   		/* ignore all white space */
\n			line_count++;	/* keep track of lines seen */
.			;

%%
