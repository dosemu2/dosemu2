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
#include <stdarg.h>
#include <pwd.h>

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

serial_t *sptr;
serial_t nullser;
mouse_t *mptr;
mouse_t nullmouse;
int c_ser = 0;
int c_mouse = 0;

struct disk *dptr;
struct disk nulldisk;
int c_hdisks = 0;
int c_fdisks = 0;

extern struct printer lpt[NUM_PRINTERS];
struct printer *pptr;
struct printer nullptr;
int c_printers = 0;

int ports_permission = IO_RDWR;
unsigned int ports_ormask = 0;
unsigned int ports_andmask = 0xFFFF;

int errors = 0;
int warnings = 0;

extern struct config_info config;  /* Configuration information */

	/* external procedures */

void die(char *reason) NORETURN;
extern int allow_io(int, int, int, int, int);
extern int exchange_uids(void);
extern char* strdup(const char *); /* Not defined in string.h :-( */
extern int yylex(); /* exact argument types depend on the way you call bison */

	/* local procedures */

void yyerror(char *, ...);
void yywarn(char *, ...);
void keyb_layout(int value);
void start_ports(void);
void start_mouse(void);
void stop_mouse(void);
void start_debug(void);
void stop_video(void);
void start_serial(void);
void stop_serial(void);
void start_printer(void);
void stop_printer(void);
void stop_terminal(void);
void start_disk(void);
void do_part(char *);
void start_bootdisk(void);
void start_floppy(void);
void stop_disk(int token);
FILE* open_file(char* filename);
void close_file(FILE* file);
void parse_dosemu_users(void);
static int set_hardware_ram(int addr);
static void set_irq_value(int bits, int i1);
static void set_irq_range(int bits, int i1, int i2);

	/* variables in lexer.l */

extern int line_count;
extern FILE* yyin;
extern void yyrestart(FILE *input_file);
%}

%start lines

%union {
  int   i_value;
  char *s_value;
  };

%token <i_value> INTEGER L_OFF L_ON L_YES L_NO CHIPSET_TYPE
%token <i_value> CHARSET_TYPE KEYB_LAYOUT
%token <s_value> STRING
	/* main options */
%token DOSBANNER FASTFLOPPY TIMINT HOGTHRESH SPEAKER IPXSUPPORT NOVELLHACK
%token DEBUG MOUSE SERIAL COM KEYBOARD TERMINAL VIDEO ALLOWVIDEOPORT TIMER
%token MATHCO CPU BOOTA BOOTB BOOTC L_XMS L_DPMI PORTS DISK DOSMEM PRINTER
%token L_EMS EMS_SIZE EMS_FRAME
%token BOOTDISK L_FLOPPY EMUSYS EMUBAT L_X
	/* speaker */
%token EMULATED NATIVE
	/* keyboard */
%token KEYBINT RAWKEYBOARD
	/* ipx */
%token NETWORK PKTDRIVER
	/* serial */
%token BASE IRQ INTERRUPT DEVICE CHARSET  BAUDRATE
	/* mouse */
%token MICROSOFT LOGITECH MMSERIES MOUSEMAN HITACHI MOUSESYSTEMS BUSMOUSE PS2
%token INTERNALDRIVER CLEARDTR
	/* x-windows */
%token L_DISPLAY L_TITLE ICON_NAME X_KEYCODE X_BLINKRATE
	/* video */
%token VGA MGA CGA EGA CONSOLE GRAPHICS CHIPSET FULLREST PARTREST
%token MEMSIZE VBIOS_SIZE VBIOS_SEG VBIOS_FILE VBIOS_COPY VBIOS_MMAP
	/* terminal */
%token UPDATEFREQ UPDATELINES COLOR CORNER METHOD NORMAL XTERM NCURSES FAST
	/* debug */
%token IO PORT CONFIG READ WRITE KEYB PRINTER WARNING GENERAL HARDWARE
%token L_IPC
	/* printer */
%token COMMAND TIMEOUT OPTIONS L_FILE
	/* disk */
%token L_PARTITION WHOLEDISK THREEINCH FIVEINCH READONLY LAYOUT
%token SECTORS CYLINDERS TRACKS HEADS OFFSET HDIMAGE
	/* ports/io */
%token RDONLY WRONLY RDWR ORMASK ANDMASK RANGE
	/* Silly interrupts */
%token SILLYINT USE_SIGIO
	/* hardware ram mapping */
%token HARDWARE_RAM

%type <i_value> mem_bool irq_bool bool speaker method_val color_val

%%

lines		: line
		| lines line
		;

line		: HOGTHRESH INTEGER	{ config.hogthreshold = $2; }
 		| EMUSYS STRING
		    {
		    config.emusys = $2;
		    c_printf("CONF: config.emusys = '%s'\n", $2);
		    }
		| EMUSYS '{' STRING '}'
		    {
		    config.emusys = $3;
		    c_printf("CONF: config.emusys = '%s'\n", $3);
		    }
 		| EMUBAT STRING
		    {
		    config.emubat = $2;
		    c_printf("CONF: config.emubat = '%s'\n", $2);
		    }
		| EMUBAT '{' STRING '}'
		    {
		    config.emubat = $3;
		    c_printf("CONF: config.emubat = '%s'\n", $3);
		    }
		| FASTFLOPPY INTEGER
			{ 
			config.fastfloppy = $2;
			c_printf("CONF: fastfloppy = %d\n", config.fastfloppy);
			}
		| FASTFLOPPY bool
			{
			config.fastfloppy = ($2) ? 2 : 0;
			c_printf("CONF: fastfloppy = %d", config.fastfloppy);
			}
		| CPU INTEGER		{ vm86s.cpu_type = ($2/100)%10; }
		| BOOTA			{ config.hdiskboot = 0; }
		| BOOTC			{ config.hdiskboot = 1; }
		| BOOTB			{ config.hdiskboot = 2; }
		| TIMINT bool
		    {
		    config.timers = $2;
		    c_printf("CONF: timint %s\n", ($2) ? "on" : "off");
		    }
		| TIMER INTEGER
		    {
		    config.freq = $2;
		    config.update = 1000000 / $2;
		    }
		| DOSBANNER bool
		    {
		    config.dosbanner = $2;
		    c_printf("CONF: dosbanner %s\n", ($2) ? "on" : "off");
		    }
		| ALLOWVIDEOPORT bool
		    {
		    config.allowvideoportaccess = $2;
		    c_printf("CONF: allowvideoportaccess %s\n", ($2) ? "on" : "off");
		    }
		| L_EMS { config.ems_size=0; }  '{' ems_flags '}'
		| L_EMS mem_bool
		    {
		    config.ems_size = $2;
		    if ($2 > 0) c_printf("CONF: %dk bytes EMS memory\n", $2);
		    }
		| L_XMS mem_bool
		    {
		    config.xms_size = $2;
		    if ($2 > 0) c_printf("CONF: %dk bytes XMS memory\n", $2);
		    }
		| L_DPMI mem_bool
		    {
		    config.dpmi_size = $2;
		    if ($2 > 0) c_printf("CONF: %dk bytes DPMI memory\n", $2);
		    }
		| DOSMEM mem_bool	{ config.mem_size = $2; }
		| MATHCO bool		{ config.mathco = $2; }
		| IPXSUPPORT bool	{ config.ipxsup = $2; }
		    {
		    config.ipxsup = $2;
		    c_printf("CONF: IPX support %s\n", ($2) ? "on" : "off");
		    }
		| PKTDRIVER NOVELLHACK	{ config.pktflags = 1; }
		| SPEAKER speaker
		    {
		    if ($2 == SPKR_NATIVE) {
                      yywarn("CONF: allowing access to the speaker ports!");
		      allow_io(0x42, 1, IO_RDWR, 0, 0xFFFF);
		      allow_io(0x61, 1, IO_RDWR, 0, 0xFFFF);
		      }
		    else
                      c_printf("CONF: not allowing speaker port access\n");
		    config.speaker = $2;
		    }
		| VIDEO	'{' video_flags '}'
		    { stop_video(); }
		| TERMINAL '{' term_flags '}'
		    { stop_terminal(); }
		| DEBUG
		    { start_debug(); }
		  '{' debug_flags '}'
		| MOUSE
		    { start_mouse(); }
		  '{' mouse_flags '}'
		    { stop_mouse(); }
		| SERIAL
		    { start_serial(); }
		  '{' serial_flags '}'
		  { stop_serial(); }
		| KEYBOARD '{' keyboard_flags '}'
		| PORTS
		    { start_ports(); }
		  '{' port_flags '}'
		| DISK
		    { start_disk(); }
		  '{' disk_flags '}'
		    { stop_disk(DISK); }
		| BOOTDISK
		    { start_bootdisk(); }
		  '{' disk_flags '}'
		    { stop_disk(BOOTDISK); }
		| L_FLOPPY
		    { start_floppy(); }
		  '{' disk_flags '}'
		    { stop_disk(L_FLOPPY); }
		| PRINTER
		    { start_printer(); }
		  '{' printer_flags '}'
		    { stop_printer(); }
		| L_X '{' x_flags '}'
		| SILLYINT { config.sillyint=0; }  '{' sillyint_flags '}'
		| SILLYINT irq_bool	{ config.sillyint = 1 << $2; }
		| HARDWARE_RAM 
                   { 
                     config.must_spare_hardware_ram=0;
                      memset(config.hardware_pages,0,sizeof(config.hardware_pages)); 
                   }  
                   '{' hardware_ram_flags '}'
		| STRING
		    { yyerror("unrecognized command '%s'", $1); free($1); }
		| error
		;

	/* x-windows */

x_flags		: x_flag
		| x_flags x_flag
		;
x_flag		: UPDATELINES INTEGER	{ config.X_updatelines = $2; }
		| UPDATEFREQ INTEGER	{ config.X_updatefreq = $2; }
		| L_DISPLAY STRING	{ config.X_display = $2; }
		| L_TITLE STRING	{ config.X_title = $2; }
		| ICON_NAME STRING	{ config.X_icon_name = $2; }
		| X_KEYCODE		{ config.X_keycode = 1; }
		| X_BLINKRATE INTEGER	{ config.X_blinkrate = $2; }
		;

	/* video */

video_flags	: video_flag
		| video_flags video_flag
		;
video_flag	: VGA			{ config.cardtype = CARD_VGA; }
		| MGA			{ config.cardtype = CARD_MDA; }
		| CGA			{ config.cardtype = CARD_CGA; }
		| EGA			{ config.cardtype = CARD_CGA; }
		| CHIPSET CHIPSET_TYPE
		    {
		    config.chipset = $2;
                    c_printf("CHIPSET: %d\n", $2);
		    }
		| MEMSIZE INTEGER	{ config.gfxmemsize = $2; }
		| GRAPHICS		{ config.vga = 1; }
		| CONSOLE		{ config.console_video = 1; }
		| FULLREST		{ config.fullrestore = 1; }
		| PARTREST		{ config.fullrestore = 0; }
		| VBIOS_FILE STRING	{ config.vbios_file = $2;
					  config.mapped_bios = 1;
					  config.vbios_copy = 0; }
		| VBIOS_COPY		{ config.vbios_file = NULL;
					  config.mapped_bios = 1;
					  config.vbios_copy = 1; }
		| VBIOS_MMAP		{ config.vbios_file = NULL;
					  config.mapped_bios = 1;
					  config.vbios_copy = 1; }

		| VBIOS_SEG INTEGER
		   {
		   config.vbios_seg = $2;
		   c_printf("CONF: VGA-BIOS-Segment %x\n", $2);
		   if (($2 != 0xe000) && ($2 != 0xc000))
		      {
		      config.vbios_seg = 0xc000;
		      c_printf("CONF: VGA-BIOS-Segment set to 0xc000\n");
		      }
		   }
		| VBIOS_SIZE INTEGER
		   {
		   config.vbios_size = $2;
		   c_printf("CONF: VGA-BIOS-Size %x\n", $2);
		   if (($2 != 0x8000) && ($2 != 0x10000))
		      {
		      config.vbios_size = 0x10000;
		      c_printf("CONF: VGA-BIOS-Size set to 0x10000\n");
		      }
		   }
		| STRING
		    { yyerror("unrecognized video option '%s'", $1);
		      free($1); }
		| error
		;

	/* terminal */

term_flags	: term_flag
		| term_flags term_flag
		;
term_flag	: METHOD method_val	{ config.term_method = $2; }
		| UPDATELINES INTEGER	{ config.term_updatelines = $2; }
		| UPDATEFREQ INTEGER	{ config.term_updatefreq = $2; }
		| CHARSET CHARSET_TYPE	{ config.term_charset = $2; }
		| COLOR color_val	{ config.term_color = $2; }
		| CORNER bool		{ config.term_corner = $2; }
		| STRING
		    { yyerror("unrecognized terminal option '%s'", $1);
		      free($1); }
		| error
		;

color_val	: L_OFF			{ $$ = 0; }
		| L_ON			{ $$ = COLOR_NORMAL; }
		| NORMAL		{ $$ = COLOR_NORMAL; }
		| XTERM			{ $$ = COLOR_XTERM; }
		;

method_val	: FAST			{ $$ = METHOD_FAST; }
		| NCURSES		{ $$ = METHOD_NCURSES; }
		;

	/* debugging */

debug_flags	: debug_flag
		| debug_flags debug_flag
		;
debug_flag	: VIDEO bool		{ d.video = $2; }
		| SERIAL bool		{ d.serial = $2; }
		| CONFIG bool		{ if (!d.config) d.config = $2; }
		| DISK bool		{ d.disk = $2; }
		| READ bool		{ d.read = $2; }
		| WRITE bool		{ d.write = $2; }
		| KEYB bool		{ d.keyb = $2; }
		| KEYBOARD bool		{ d.keyb = $2; }
		| PRINTER bool		{ d.printer = $2; }
		| IO bool		{ d.io = $2; }
		| PORT bool 		{ d.io = $2; }
		| WARNING bool		{ d.warning = $2; }
		| GENERAL bool		{ d.general = $2; }
		| L_XMS bool		{ d.xms = $2; }
		| L_DPMI bool		{ d.dpmi = $2; }
		| MOUSE bool		{ d.mouse = $2; }
		| HARDWARE bool		{ d.hardware = $2; }
		| L_IPC bool		{ d.IPC = $2; }
		| L_EMS bool		{ d.EMS = $2; }
		| NETWORK bool		{ d.network = $2; }
		| L_X bool		{ d.X = $2; }
		| STRING
		    { yyerror("unrecognized debug flag '%s'", $1); free($1); }
		| error
		;

	/* mouse */

mouse_flags	: mouse_flag
		| mouse_flags mouse_flag
		;
mouse_flag	: DEVICE STRING		{ strcpy(mptr->dev, $2); free($2); }
		| INTERNALDRIVER	{ mptr->intdrv = TRUE; }
		| BAUDRATE INTEGER	{ mptr->baudRate = $2; }
		| CLEARDTR
		    { if (mptr->type == MOUSE_MOUSESYSTEMS)
			 mptr->cleardtr = TRUE;
		      else
			 yyerror("option CLEARDTR is only valid for MicroSystems-Mice");
		    }
		| MICROSOFT
		  {
		  mptr->type = MOUSE_MICROSOFT;
		  mptr->flags = CS7 | CREAD | CLOCAL | HUPCL;
		  }
		| MOUSESYSTEMS
		  {
		  mptr->type = MOUSE_MOUSESYSTEMS;
		  mptr->flags = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL;
		  }
		| MMSERIES
		  {
		  mptr->type = MOUSE_MMSERIES;
		  mptr->flags = CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL;
		  }
		| LOGITECH
		  {
		  mptr->type = MOUSE_LOGITECH;
		  mptr->flags = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL;
		  }
		| PS2
		  {
		  mptr->type = MOUSE_PS2;
		  mptr->flags = 0;
		  }
		| MOUSEMAN
		  {
		  mptr->type = MOUSE_MOUSEMAN;
		  mptr->flags = CS7 | CREAD | CLOCAL | HUPCL;
		  }
		| HITACHI
		  {
		  mptr->type = MOUSE_HITACHI;
		  mptr->flags = CS8 | CREAD | CLOCAL | HUPCL;
		  }
		| BUSMOUSE
		  {
		  mptr->type = MOUSE_BUSMOUSE;
		  mptr->flags = 0;
		  }
		| STRING
		    { yyerror("unrecognized mouse flag '%s'", $1); free($1); }
		| error
		;

	/* keyboard */

keyboard_flags	: keyboard_flag
		| keyboard_flags keyboard_flag
		;
keyboard_flag	: LAYOUT KEYB_LAYOUT	{ keyb_layout($2); }
		| LAYOUT L_NO		{ keyb_layout(KEYB_NO); }
		| RAWKEYBOARD bool	{ config.console_keyb = $2; }
		| KEYBINT bool		{ config.keybint = $2; }
		| STRING
		    { yyerror("unrecognized keyboard flag '%s'", $1);
		      free($1);}
		| error
		;

	/* serial ports */

serial_flags	: serial_flag
		| serial_flags serial_flag
		;
serial_flag	: DEVICE STRING		{ strcpy(sptr->dev, $2); free($2); }
		| COM INTEGER		{ sptr->real_comport = $2;
					  com_port_used[$2] = 1; }
		| BASE INTEGER		{ sptr->base_port = $2; }
		| IRQ INTEGER		{ sptr->interrupt = $2; }
		| INTERRUPT INTEGER	{ sptr->interrupt = $2; }
		| MOUSE			{ sptr->mouse = 1; }
		| STRING
		    { yyerror("unrecognized serial flag '%s'", $1); free($1); }
		| error
		;

	/* printer */

printer_flags	: printer_flag
		| printer_flags printer_flag
		;
printer_flag	: COMMAND STRING	{ pptr->prtcmd = $2; }
		| TIMEOUT INTEGER	{ pptr->delay = $2; }
		| OPTIONS STRING	{ pptr->prtopt = $2; }
		| L_FILE STRING		{ pptr->dev = $2; }
		| STRING
		    { yyerror("unrecognized printer flag %s", $1); free($1); }
		| error
		;

	/* disks */

disk_flags	: disk_flag
		| disk_flags disk_flag
		;
disk_flag	: READONLY		{ dptr->wantrdonly = 1; }
		| THREEINCH	{ dptr->default_cmos = THREE_INCH_FLOPPY; }
		| FIVEINCH	{ dptr->default_cmos = FIVE_INCH_FLOPPY; }
		| SECTORS INTEGER	{ dptr->sectors = $2; }
		| CYLINDERS INTEGER	{ dptr->tracks = $2; }
		| TRACKS INTEGER	{ dptr->tracks = $2; }
		| HEADS INTEGER		{ dptr->heads = $2; }
		| OFFSET INTEGER	{ dptr->header = $2; }
		| DEVICE STRING
		  {
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a disk-image file or device given.");
		  dptr->dev_name = $2;
		  }
		| L_FILE STRING
		  {
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a disk-image file or device given.");
		  dptr->dev_name = $2;
		  }
		| HDIMAGE STRING
		  {
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a harddisk-image file given.");
		  dptr->type = IMAGE;
		  dptr->header = HEADER_SIZE;
		  dptr->dev_name = $2;
		  }
		| WHOLEDISK STRING
		  {
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a harddisk given.");
		  dptr->type = HDISK;
		  dptr->dev_name = $2;
		  }
		| L_FLOPPY STRING
		  {
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a floppy-device given.");
		  dptr->type = FLOPPY;
		  dptr->dev_name = $2;
		  }
		| L_PARTITION STRING INTEGER
		  {
                  yywarn("CONF: { partition \"%s\" %d } the"
			 " token '%d' is ignored and can be removed.",
			 $2,$3,$3);
		  do_part($2);
		  }
		| L_PARTITION STRING 
		  { do_part($2); }
		| STRING
		    { yyerror("unrecognized disk flag '%s'\n", $1); free($1); }
		| error
		;

	/* i/o ports */

port_flags	: port_flag
		| port_flags port_flag
		;
port_flag	: INTEGER
	           {
	           allow_io($1, 1, ports_permission, ports_ormask,
	                    ports_andmask);
	           }
		| RANGE INTEGER INTEGER
		   {
		   c_printf("CONF: range of I/O ports 0x%04x-0x%04x\n",
			    (unsigned short)$2, (unsigned short)$3);
		   allow_io($2, $3 - $2 + 1, ports_permission, ports_ormask,
			    ports_andmask);
		   }
		| RDONLY		{ ports_permission = IO_READ; }
		| WRONLY		{ ports_permission = IO_WRITE; }
		| RDWR			{ ports_permission = IO_RDWR; }
		| ORMASK INTEGER	{ ports_ormask = $2; }
		| ANDMASK INTEGER	{ ports_andmask = $2; }
		| STRING
		    { yyerror("unrecognized port command '%s'", $1);
		      free($1); }
		| error
		;

	/* IRQ definition for Silly Interrupt Generator */

sillyint_flags	: sillyint_flag
		| sillyint_flags sillyint_flag
		;
sillyint_flag	: INTEGER { set_irq_value(1, $1); }
		| USE_SIGIO INTEGER { set_irq_value(0x10001, $2); }
		| RANGE INTEGER INTEGER { set_irq_range(1, $2, $3); }
		| USE_SIGIO RANGE INTEGER INTEGER { set_irq_range(0x10001, $3, $4); }
		| STRING
		    { yyerror("unrecognized sillyint command '%s'", $1);
		      free($1); }
		| error
		;

	/* EMS definitions  */

ems_flags	: ems_flag
		| ems_flags ems_flag
		;
ems_flag	: INTEGER
	           {
		     config.ems_size = $1;
		     if ($1 > 0) c_printf("CONF: %dk bytes EMS memory\n", $1);
	           }
		| EMS_SIZE INTEGER
		   {
		     config.ems_size = $2;
		     if ($2 > 0) c_printf("CONF: %dk bytes EMS memory\n", $2);
		   }
		| EMS_FRAME INTEGER
		   {
		     if ( (($2 & 0xfc00)>0xc800) && (($2 & 0xfc00)<=0xe000) ) {
		       config.ems_frame = $2 & 0xfc00;
		       c_printf("CONF: EMS-frame = 0x%04x\n", config.ems_frame);
		     }
		     else yyerror("wrong EMS-frame: 0x%04x\n", $2);
		   }
		| STRING
		    { yyerror("unrecognized ems command '%s'", $1);
		      free($1); }
		| error
		;

	/* memory areas to spare for hardware (adapter) ram */

hardware_ram_flags : hardware_ram_flag
		| hardware_ram_flags hardware_ram_flag
		;
hardware_ram_flag : INTEGER
	           {
                     if (!set_hardware_ram($1)) {
                       yyerror("wrong hardware ram address : 0x%05x", $1);
                     }
	           }
		| RANGE INTEGER INTEGER
		   {
                     int i;
                     for (i=$2; i<= $3; i+=0x1000) {
                       if (set_hardware_ram(i))
	                   c_printf("CONF: hardware ram page at 0x%05x\n", i);
                       else {
                         yyerror("wrong hardware ram address : 0x%05x", i);
                         break;
                       }
                     }
		   }
		| STRING
		    { yyerror("unrecognized sillyint command '%s'", $1);
		      free($1); }
		| error
		;

	/* booleans */

bool		: L_YES		{ $$ = 1; }
		| L_NO		{ $$ = 0; }
		| L_ON		{ $$ = 1; }
		| L_OFF		{ $$ = 0; }
		| INTEGER	{ $$ = $1 != 0; }
		| STRING        { yyerror("got '%s', expected 'on' or 'off'", $1);
				  free($1); }
                | error         { yyerror("expected 'on' or 'off'"); }
		;

mem_bool	: L_OFF		{ $$ = 0; }
		| INTEGER
		| STRING        { yyerror("got '%s', expected 'off' or an integer", $1);
				  free($1); }
		| error         { yyerror("expected 'off' or an integer"); }
		;

irq_bool	: L_OFF		{ $$ = 0; }
		| INTEGER       { if ( ($1 < 2) || ($1 > 15) ) {
                                    yyerror("got '%d', expected 'off' or an integer 2..15", $1);
                                  } 
                                }
		| STRING        { yyerror("got '%s', expected 'off' or an integer 2..15", $1);
				  free($1); }
		| error         { yyerror("expected 'off' or an integer 2..15"); }
		;

	/* speaker values */

speaker		: L_OFF		{ $$ = SPKR_OFF; }
		| NATIVE	{ $$ = SPKR_NATIVE; }
		| EMULATED	{ $$ = SPKR_EMULATED; }
		| STRING        { yyerror("got '%s', expected 'emulated' or 'native'", $1);
				  free($1); }
		| error         { yyerror("expected 'emulated' or 'native'"); }
		;

%%

	/* mouse */

void start_mouse(void)
{
  if (c_mouse >= MAX_MOUSE)
    mptr = &nullmouse;
  else {
    mptr = &mice[c_mouse];
    mptr->fd = -1;
  }
}

void stop_mouse(void)
{
  if (c_mouse >= MAX_MOUSE) {
    c_printf("MOUSE: too many mice, ignoring %s\n", mptr->dev);
    return;
  }
  c_mouse++;
  config.num_mice = c_mouse;
  c_printf("MOUSE: %s type %x using internaldriver: %s, baudrate: %d\n", 
        mptr->dev, mptr->type, mptr->intdrv ? "yes" : "no", mptr->baudRate);
}

	/* debug */

void start_ports(void)
{
  ports_permission = IO_RDWR;
  ports_ormask = 0;
  ports_andmask = 0xFFFF;
}

	/* debug */

void start_debug(void)
{
  int flag = 0;                 /* Default is no debugging output at all */

  d.video = flag;               /* For all options */
  d.serial = flag;
#if 0
  d.config = flag;
#endif
  d.disk = flag;
  d.read = flag;
  d.write = flag;
  d.keyb = flag;
  d.printer = flag;
  d.io = flag;
  d.warning = flag;
  d.general = flag;
  d.xms = flag;
  d.dpmi = flag;
  d.mouse = flag;
  d.hardware = flag;
  d.IPC = flag;
  d.EMS = flag;
  d.network = flag;
}

	/* video */

void stop_video(void)
{
  if ((config.cardtype != CARD_VGA) || !config.console_video) {
    config.graphics = 0;
    config.vga = 0;
  }

  if (config.vga) {
    if (config.mem_size > 640)
      config.mem_size = 640;
    config.mapped_bios = 1;
    config.console_video = 1;
  }
}

	/* serial */

void start_serial(void)
{
  if (c_ser >= MAX_SER)
    sptr = &nullser;
  else {
    /* The defaults for interrupt, base_port, real_comport and dev are 
    ** automatically filled in inside the do_ser_init routine of serial.c
    */
    sptr = &com[c_ser];
    sptr->dev[0] = 0;
    sptr->interrupt = 0; 
    sptr->base_port = 0;
    sptr->real_comport = 0;
    sptr->fd = -1;
    sptr->mouse = 0;
  }
}


void stop_serial(void)
{
  if (c_ser >= MAX_SER) {
    c_printf("SER: too many ports, ignoring %s\n", sptr->dev);
    return;
  }
  c_ser++;
  config.num_ser = c_ser;
  c_printf("SER: %s port %x int %x\n", sptr->dev, sptr->base_port,
	   sptr->interrupt);
}

	/* terminal */

void stop_terminal(void)
{
  if (config.term_updatefreq > 100) {
    yywarn("CONF: terminal updatefreq too large (too slow)!");
    config.term_updatefreq = 100;
  } 
}

	/* printer */

void start_printer(void)
{
  if (c_printers >= NUM_PRINTERS)
    pptr = &nullptr;
  else {
    pptr = &lpt[c_printers];
    /* this causes crashes ?? */
#if 0
    pptr->prtcmd = strdup("lpr");
    pptr->prtopt = strdup("%s");
#else
    pptr->prtcmd = NULL;
    pptr->prtopt = NULL;
#endif
    pptr->dev = NULL;
    pptr->file = NULL;
    pptr->remaining = -1;
    pptr->delay = 10;
  }
}

void stop_printer(void)
{
  c_printf("CONF(LPT%d) f: %s   c: %s  o: %s  t: %d\n",
	   c_printers, pptr->dev, pptr->prtcmd, pptr->prtopt, pptr->delay);
  c_printers++;
  config.num_lpt = c_printers;
}

	/* disk */

void start_bootdisk(void)
{
  if (config.bootdisk)           /* Already a bootdisk configured ? */
    yyerror("There is already a bootdisk configured");
      
  dptr = &bootdisk;              /* set pointer do bootdisk-struct */
      
  dptr->sectors = 0;             /* setup default-values           */
  dptr->heads   = 0;
  dptr->tracks  = 0;
  dptr->type    = FLOPPY;
  dptr->default_cmos = THREE_INCH_FLOPPY;
  dptr->timeout = 0;
  dptr->dev_name = NULL;              /* default-values */
  dptr->wantrdonly = 0;
  dptr->header = 0;
}

void start_floppy(void)
{
  if (c_fdisks >= MAX_FDISKS)
    {
    yyerror("There are too many floppy disks defined");
    dptr = &nulldisk;          /* Dummy-Entry to avoid core-dumps */
    }
  else
    dptr = &disktab[c_fdisks];

  dptr->sectors = 0;             /* setup default values */
  dptr->heads   = 0;
  dptr->tracks  = 0;
  dptr->type    = FLOPPY;
  dptr->default_cmos = THREE_INCH_FLOPPY;
  dptr->timeout = 0;
  dptr->dev_name = NULL;              /* default-values */
  dptr->wantrdonly = 0;
  dptr->header = 0;
}

void start_disk(void)
{
  if (c_hdisks >= MAX_HDISKS) 
    {
    yyerror("There are too many hard disks defined");
    dptr = &nulldisk;          /* Dummy-Entry to avoid core-dumps */
    }
  else
    dptr = &hdisktab[c_hdisks];
      
  dptr->type    =  NODISK;
  dptr->sectors = -1;
  dptr->heads   = -1;
  dptr->tracks  = -1;
  dptr->timeout = 0;
  dptr->dev_name = NULL;              /* default-values */
  dptr->wantrdonly = 0;
  dptr->header = 0;
}

void do_part(char *dev)
{
  if (dptr->dev_name != NULL)
    yyerror("Two names for a partition given.");
  dptr->type = PARTITION;
  dptr->dev_name = dev;
  dptr->part_info.number = atoi(dptr->dev_name+8);
  if (dptr->part_info.number == 0) 
    yyerror("ERROR: %s must be a PARTITION, can't find number suffix!\n",
   	    dptr->dev_name);
}

void stop_disk(int token)
{
  if (dptr == &nulldisk)              /* is there any disk? */
    return;                           /* no, nothing to do */

  if (!dptr->dev_name)                /* Is there a file/device-name? */
    yyerror("disk: no device/file-name given!");
  else                                /* check the file/device for existance */
    {
      struct stat file_status;        /* date for checking that file */

      c_printf("device: %s ", dptr->dev_name);
      if (stat(dptr->dev_name,&file_status) != 0) /* Does this file exist? */
	 yyerror("Disk-device/file %s doesn't exist.",dptr->dev_name);
    }

  if (dptr->type == NODISK)    /* Is it one of bootdisk, floppy, harddisk ? */
    yyerror("disk: no device/file-name given!"); /* No, error */
  else
    c_printf("type %d ", dptr->type);

  if (dptr->type == PARTITION)
    {
      c_printf("partition# %d ", dptr->part_info.number);
    }

  if (dptr->header)
    c_printf("header_size: %ld ", (long) dptr->header);

  c_printf("h: %d  s: %d   t: %d\n", dptr->heads, dptr->sectors,
	   dptr->tracks);

  if (token == BOOTDISK) {
    config.bootdisk = 1;
    use_bootdisk = 1;
  }
  else if (token == L_FLOPPY) {
    c_fdisks++;
    config.fdisks = c_fdisks;
  }
  else {
    c_hdisks++;
    config.hdisks = c_hdisks;
  }
}

	/* keyboard */

void keyb_layout(int layout)
{
  switch (layout) {
  case KEYB_FINNISH:
    c_printf("CONF: Keyboard-layout finnish\n");
    config.keyboard  = KEYB_FINNISH;
    config.key_map   = key_map_finnish;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_finnish;
    config.alt_map   = alt_map_finnish;
    config.num_table = num_table_comma;
    break;
  case KEYB_FINNISH_LATIN1:
    c_printf("CONF: Keyboard-layout finnish-latin1\n");
    config.keyboard  = KEYB_FINNISH_LATIN1;
    config.key_map   = key_map_finnish_latin1;
    config.shift_map = shift_map_finnish_latin1;
    config.alt_map   = alt_map_finnish_latin1;
    config.num_table = num_table_comma;
    break;
  case KEYB_US:
    c_printf("CONF: Keyboard-layout us\n");
    config.keyboard  = KEYB_US;
    config.key_map   = key_map_us;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_us;
    config.alt_map   = alt_map_us;
    config.num_table = num_table_dot;
    break;
  case KEYB_UK:
    c_printf("CONF: Keyboard-layout uk\n");
    config.keyboard  = KEYB_UK;
    config.key_map   = key_map_uk;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_uk;
    config.alt_map   = alt_map_uk;
    config.num_table = num_table_dot;
    break;
  case KEYB_GR:
    c_printf("CONF: Keyboard-layout gr\n");
    config.keyboard  = KEYB_GR;
    config.key_map   = key_map_gr;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_gr;
    config.alt_map   = alt_map_gr;
    config.num_table = num_table_comma;
    break;
  case KEYB_GR_LATIN1:
    c_printf("CONF: Keyboard-layout gr-latin1\n");
    config.keyboard  = KEYB_GR_LATIN1;
    config.key_map   = key_map_gr_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_gr_latin1;
    config.alt_map   = alt_map_gr_latin1;
    config.num_table = num_table_comma;
    break;
  case KEYB_FR:
    c_printf("CONF: Keyboard-layout fr\n");
    config.keyboard  = KEYB_FR;
    config.key_map   = key_map_fr;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_fr;
    config.alt_map   = alt_map_fr;
    config.num_table = num_table_dot;
    break;
  case KEYB_FR_LATIN1:
    c_printf("CONF: Keyboard-layout fr-latin1\n");
    config.keyboard  = KEYB_FR_LATIN1;
    config.key_map   = key_map_fr_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_fr_latin1;
    config.alt_map   = alt_map_fr_latin1;
    config.num_table = num_table_dot;
    break;
  case KEYB_DK:
    c_printf("CONF: Keyboard-layout dk\n");
    config.keyboard  = KEYB_DK;
    config.key_map   = key_map_dk;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_dk;
    config.alt_map   = alt_map_dk;
    config.num_table = num_table_comma;
    break;
  case KEYB_DK_LATIN1:
    c_printf("CONF: Keyboard-layout dk-latin1\n");
    config.keyboard  = KEYB_DK_LATIN1;
    config.key_map   = key_map_dk_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_dk_latin1;
    config.alt_map   = alt_map_dk_latin1;
    config.num_table = num_table_comma;
    break;
  case KEYB_DVORAK:
    c_printf("CONF: Keyboard-layout dvorak\n");
    config.keyboard  = KEYB_DVORAK;
    config.key_map   = key_map_dvorak;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_dvorak;
    config.alt_map   = alt_map_dvorak;
    config.num_table = num_table_comma;
    break;
  case KEYB_SG:
    c_printf("CONF: Keyboard-layout sg\n");
    config.keyboard  = KEYB_SG;
    config.key_map   = key_map_sg;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_sg;
    config.alt_map   = alt_map_sg;
    config.num_table = num_table_comma;
    break;
  case KEYB_SG_LATIN1:
    c_printf("CONF: Keyboard-layout sg-latin1\n");
    config.keyboard  = KEYB_SG_LATIN1;
    config.key_map   = key_map_sg_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_sg_latin1;
    config.alt_map   = alt_map_sg_latin1;
    config.num_table = num_table_comma;
    break;
  case KEYB_NO:
    c_printf("CONF: Keyboard-layout no\n");
    config.keyboard  = KEYB_NO;
    config.key_map   = key_map_no;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_no;
    config.alt_map   = alt_map_no;
    config.num_table = num_table_comma;
    break;
  case KEYB_SF:
    c_printf("CONF: Keyboard-layout sf\n");
    config.keyboard  = KEYB_SF;
    config.key_map   = key_map_sf;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_sf;
    config.alt_map   = alt_map_sf;
    config.num_table = num_table_comma;
    break;
  case KEYB_SF_LATIN1:
    c_printf("CONF: Keyboard-layout sf-latin1\n");
    config.keyboard  = KEYB_SF_LATIN1;
    config.key_map   = key_map_sf_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_sf_latin1;
    config.alt_map   = alt_map_sf_latin1;
    config.num_table = num_table_comma;
    break;
  case KEYB_ES:
    c_printf("CONF: Keyboard-layout es\n");
    config.keyboard  = KEYB_ES;
    config.key_map   = key_map_es;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_es;
    config.alt_map   = alt_map_es;
    config.num_table = num_table_comma;
    break;
  case KEYB_ES_LATIN1:
    c_printf("CONF: Keyboard-layout es-latin1\n");
    config.keyboard  = KEYB_ES_LATIN1;
    config.key_map   = key_map_es_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_es_latin1;
    config.alt_map   = alt_map_es_latin1;
    config.num_table = num_table_comma;
    break;
  case KEYB_BE:
    c_printf("CONF: Keyboard-layout be\n");
    config.keyboard  = KEYB_BE;
    config.key_map   = key_map_be;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_be;
    config.alt_map   = alt_map_be;
    config.num_table = num_table_dot;
    break;
  case KEYB_PO:
    c_printf("CONF: Keyboard-layout po\n");
    config.keyboard  = KEYB_PO;
    config.key_map   = key_map_po;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_po;
    config.alt_map   = alt_map_po;
    config.num_table = num_table_dot;
    break;
  case KEYB_IT:
    c_printf("CONF: Keyboard-layout it\n");
    config.keyboard  = KEYB_IT;
    config.key_map   = key_map_it;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_it;
    config.alt_map   = alt_map_it;
    config.num_table = num_table_dot;
    break;
  default:
    c_printf("CONF: ERROR -- Keyboard has incorrect number!!!\n");
  }
}

static int set_hardware_ram(int addr)
{
  if ((addr<0xc8000) || (addr>=0xf0000)) return 0;
  config.must_spare_hardware_ram=1;
  config.hardware_pages[(addr-0xc8000) >> 12]=1;
  return 1;
}


static void set_irq_value(int bits, int i1)
{
  if ((i1>2) && (i1<=15)) {
    config.sillyint |= (bits << i1);
    c_printf("CONF: IRQ %d for sillyint", i1);
    if (bits & 0x10000)  c_printf(" uses SIGIO\n");
    else c_printf("\n");
  }
  else yyerror("wrong IRQ for sillyint command: %d", i1);
}

static void set_irq_range(int bits, int i1, int i2) {
  int i;
  if ( (i1<3) || (i1>15) || (i2<3) || (i2>15) || (i1 > i2 ) ) {
    yyerror("wrong IRQ range for sillyint command: %d .. %d", i1, i2);
  }
  else {
    for (i=i1; i<=i2; i++) config.sillyint |= (bits << i);
    c_printf("CONF: range of IRQs for sillyint %d .. %d", i1, i2);
    if (bits & 0x10000)  c_printf(" uses SIGIO\n");
    else c_printf("\n");
  }
}


	/* errors & warnings */

void yywarn(char* string, ...)
{
  va_list vars;
  va_start(vars, string);
  vfprintf(stderr, string, vars);
  fprintf(stderr, "\n");
  va_end(vars);
  warnings++;
}

void yyerror(char* string, ...)
{
  va_list vars;
  va_start(vars, string);
  fprintf(stderr, "PAR: (line %.3d) ", line_count);
  vfprintf(stderr, string, vars);
  fprintf(stderr, "\n");
  va_end(vars);
  errors++;
}

void die(char *reason)
{
  error("ERROR: par dead: %s\n", reason);
  leavedos(0);
}

/*
 * open_file - opens the configuration-file named *filename and returns
 *             a file-pointer. The error/warning-counters are reset to zero.
 */

FILE *open_file(char *filename)
{
  errors   = 0;                  /* Reset error counter */
  warnings = 0;                  /* Reset counter for warnings */

  return fopen(filename, "r"); /* Open config-file */
}

/*
 * close_file - Close the configuration file and issue a message about
 *              errors/warnings that occured. If there were errors, the
 *              flag early-exit is that, so dosemu won't really.
 */

void close_file(FILE * file)
{
  fclose(file);                  /* Close the config-file */

  fprintf(stderr, "%d error(s) detected while parsing the configuration-file\n",
	 errors);
  fprintf(stderr, "%d warning(s) detected while parsing the configuration-file\n",
	 warnings);

  if (errors != 0)               /* Exit dosemu on errors */
    {
      config.exitearly = TRUE;
    }
}

/* Parse Users for DOSEMU, by Alan Hourihane, alanh@fairlite.demon.co.uk */
void
parse_dosemu_users(void)
{
  FILE *volatile fp;
  struct passwd *pwd;
  char buf[80];
  int userok = 0;

  /* Sanity Check, Shouldn't be anyone logged in without a userid */
  if ((pwd = getpwuid(getuid())) == (struct passwd *)0) {
    fprintf(stderr, "Illegal User!!!\n");
    exit(1);
  }

  if (getuid() != 0) {
        if ((fp = open_file(DOSEMU_USERS_FILE))) {
                while(fgets(buf, 79, fp) != NULL && !userok) {
			if (!(strncmp(buf, pwd->pw_name, strlen(buf)-1))) userok = 1;
                }
        } else {
		fprintf(stderr,
   "Cannot open %s, Please check installation via System Admin.\n",
				DOSEMU_USERS_FILE);
		exit(1);
        }
        fclose(fp);
        if (userok == 0) {
           	fprintf(stderr,
   "Sorry %s. You are not allowed to use DOSEMU. Contact System Admin.\n",
                                pwd->pw_name);
		exit(1);
        }
   } 
}

int
parse_config(char *confname)
{
  FILE *volatile fd;
#if YYDEBUG != 0
  extern int yydebug;

  yydebug  = 1;
#endif

  c_hdisks = 0;
  c_fdisks = 0;

  /* Parse valid users who can execute DOSEMU */
  parse_dosemu_users();

  /* Let's try confname if not null, and fail if not found */
  /* Else try the user's own .dosrc */
  /* If that doesn't exist we will default to CONFIG_FILE */

  { 
    char *home = getenv("HOME");
    char *name = malloc(strlen(home) + 20);
    sprintf(name, "%s/.dosrc", home);

    if (getuid() != 0) {
      if (!exchange_uids()) die("Cannot exchange uids\n");
      if (!exchange_uids()) die("Cannot changeback uids\n");
    }

    if(confname) {
      if(!(fd = open_file(confname))) {
        fprintf(stderr, "Cannot open -F config file %s, Aborting DOSEMU.\n",confname);
           exit(1);
      }
    } else {

      if (!(fd = open_file(name))) {
        fprintf(stderr, "Cannot open user config file %s, Trying default.\n",name);
        if (!(fd = open_file(CONFIG_FILE))) {
          fprintf(stderr, "Cannot open default config file %s, Aborting DOSEMU.\n",CONFIG_FILE);
          exit(1);
        }
      }
    }

    yyin = fd;
    line_count = 1;
    if (yyparse())
      yyerror("error in user's configuration file");
    close_file(fd);
  }

#ifdef TESTING
  error("TESTING: parser is terminating program\n");
  leavedos(0);
#endif

  return 1;
}

#ifdef TESTING_MAIN
int
main(int argc, char **argv)
{
  if (argc != 2)
    die("no filename!");

  if (!parse_config(argv[1]))
    die("parse failed!\n");
}

#endif
