/* $Id: parser.y,v 1.13 1995/05/06 16:26:13 root Exp root $ 
 *
 * $Log: parser.y,v $
 * Revision 1.13  1995/05/06  16:26:13  root
 * Prep for 0.60.2.
 *
 * Revision 1.12  1995/04/08  22:34:19  root
 * Release dosemu0.60.0
 *
 * Revision 1.11  1995/02/05  16:53:16  root
 * Prep for Scotts patches.
 *
 */
%{
#include <stdlib.h>
#include <termios.h>
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
#include <syslog.h>
#include <string.h>
#ifdef __linux__
#include <mntent.h>
#endif
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "config.h"
#include "emu.h"
#if 0
#include "cpu.h"
#endif /* WHY */
#include "disks.h"
#include "lpt.h"
#include "video.h"
#include "mouse.h"
#include "serial.h"
#include "timers.h"
#include "keymaps.h"
#include "memory.h"

static serial_t *sptr;
static serial_t nullser;
static mouse_t *mptr;
static mouse_t nullmouse;
static int c_ser = 0;
static int c_mouse = 0;

static struct disk *dptr;
static struct disk nulldisk;
static int c_hdisks = 0;
static int c_fdisks = 0;

#define DEXE_LOAD_PATH "/var/lib/dosemu"
static int dexe_running = 0;
static int dexe_forbid_disk = 1;
static int dexe_secure = 1;

extern struct printer lpt[NUM_PRINTERS];
static struct printer *pptr;
static struct printer nullptr;
static int c_printers = 0;

static int ports_permission = IO_RDWR;
static unsigned int ports_ormask = 0;
static unsigned int ports_andmask = 0xFFFF;
static unsigned int portspeed = 0;
static char dev_name[255]= "";

static int errors = 0;
static int warnings = 0;

static int priv_lvl = 0;

static char *file_being_parsed;

	/* external procedures */

extern int allow_io(int, int, int, int, int, int, char *);
extern int exchange_uids(void);
extern char* strdup(const char *); /* Not defined in string.h :-( */
extern int yylex(); /* exact argument types depend on the way you call bison */

	/* local procedures */

void yyerror(char *, ...);
static void yywarn(char *, ...);
void keyb_layout(int value);
static void start_ports(void);
static void start_mouse(void);
static void stop_mouse(void);
static void start_debug(void);
static void start_video(void);
static void stop_video(void);
static void start_ttylocks(void);
static void stop_ttylocks(void);
static void start_serial(void);
static void stop_serial(void);
static void start_printer(void);
static void stop_printer(void);
static void start_keyboard(void);
static void start_terminal(void);
static void stop_terminal(void);
static void start_disk(void);
static void do_part(char *);
static void start_bootdisk(void);
static void start_floppy(void);
static void stop_disk(int token);
static FILE* open_file(char* filename);
static void close_file(FILE* file);
static void write_to_syslog(char *message);
static void mail_to_root(char *subject, char *message);
static void parse_dosemu_users(void);
static int set_hardware_ram(int addr);
static void set_irq_value(int bits, int i1);
static void set_irq_range(int bits, int i1, int i2);
extern void append_pre_strokes(unsigned char *s);
char *get_config_variable(char *name);
int define_config_variable(char *name);
static int undefine_config_variable(char *name);

	/* class stuff */
#define IFCLASS(m) if (is_in_allowed_classes(m))

#define CL_ALL			-1
#define CL_NICE		    0x100
#define CL_VAR		    0x200
#define CL_FILEEXT	    0x400
#define CL_FLOPPY	    0x800
#define CL_BOOT		   0x1000
#define CL_VPORT	   0x2000
#define CL_SECURE	   0x4000
#define CL_DPMI		   0x8000
#define CL_VIDEO	  0x10000
#define CL_PORT		  0x20000
#define CL_DISK		  0x40000
#define CL_X		  0x80000
#define CL_SOUND	 0x100000
#define CL_IRQ		 0x200000
#define CL_DEXE		 0x400000
#define CL_PRINTER	 0x800000
#define CL_HARDRAM	0x1000000

static int is_in_allowed_classes(int mask);

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
%token DEFINE UNDEF
%token DOSBANNER FASTFLOPPY TIMINT HOGTHRESH SPEAKER IPXSUPPORT NOVELLHACK
%token DEBUG MOUSE SERIAL COM KEYBOARD TERMINAL VIDEO ALLOWVIDEOPORT TIMER
%token MATHCO CPU BOOTA BOOTB BOOTC L_XMS L_DPMI PORTS DISK DOSMEM PRINTER
%token L_EMS L_UMB EMS_SIZE EMS_FRAME TTYLOCKS L_SOUND
%token L_SECURE
%token DEXE ALLOWDISK FORCEXDOS
%token BOOTDISK L_FLOPPY EMUSYS EMUBAT EMUINI L_X
%token DOSEMUMAP LOGBUFSIZE
	/* speaker */
%token EMULATED NATIVE
	/* keyboard */
%token KEYBINT RAWKEYBOARD
%token PRESTROKE
	/* ipx */
%token NETWORK PKTDRIVER
        /* lock files */
%token DIRECTORY NAMESTUB BINARY
	/* serial */
%token BASE IRQ INTERRUPT DEVICE CHARSET  BAUDRATE
	/* mouse */
%token MICROSOFT MS3BUTTON LOGITECH MMSERIES MOUSEMAN HITACHI MOUSESYSTEMS BUSMOUSE PS2
%token INTERNALDRIVER EMULATE3BUTTONS CLEARDTR
	/* x-windows */
%token L_DISPLAY L_TITLE ICON_NAME X_KEYCODE X_BLINKRATE X_SHARECMAP X_MITSHM X_FONT
%token X_FIXED_ASPECT X_ASPECT_43 X_LIN_FILT X_BILIN_FILT X_MODE13FACT X_WINSIZE
	/* video */
%token VGA MGA CGA EGA CONSOLE GRAPHICS CHIPSET FULLREST PARTREST
%token MEMSIZE VBIOS_SIZE_TOK VBIOS_SEG VBIOS_FILE VBIOS_COPY VBIOS_MMAP DUALMON
%token FORCE_VT_SWITCH
	/* terminal */
%token UPDATEFREQ UPDATELINES COLOR ESCCHAR
/* %token UPDATEFREQ UPDATELINES COLOR CORNER METHOD NORMAL XTERM NCURSES FAST */
	/* debug */
%token IO PORT CONFIG READ WRITE KEYB PRINTER WARNING GENERAL HARDWARE
%token L_IPC SOUND
	/* printer */
%token COMMAND TIMEOUT OPTIONS L_FILE
	/* disk */
%token L_PARTITION BOOTFILE WHOLEDISK THREEINCH FIVEINCH READONLY LAYOUT
%token SECTORS CYLINDERS TRACKS HEADS OFFSET HDIMAGE
	/* ports/io */
%token RDONLY WRONLY RDWR ORMASK ANDMASK RANGE FAST DEV_NAME
	/* Silly interrupts */
%token SILLYINT USE_SIGIO
	/* hardware ram mapping */
%token HARDWARE_RAM
        /* Sound Emulation */
%token SB_BASE SB_IRQ SB_DMA SB_MIXER SB_DSP MPU_BASE

/* %type <i_value> mem_bool irq_bool bool speaker method_val color_val floppy_bool */
%type <i_value> mem_bool irq_bool bool speaker color_val floppy_bool

%%

lines		: line
		| lines line
		;

line		: HOGTHRESH INTEGER	{ IFCLASS(CL_NICE) config.hogthreshold = $2; }
		| DEFINE STRING		{ IFCLASS(CL_VAR){ define_config_variable($2); free($2); }}
		| UNDEF STRING		{ IFCLASS(CL_VAR){ undefine_config_variable($2); free($2); }}
 		| EMUSYS STRING
		    { IFCLASS(CL_FILEEXT){
		    config.emusys = $2;
		    c_printf("CONF: config.emusys = '%s'\n", $2);
		    }}
		| EMUSYS '{' STRING '}'
		    { IFCLASS(CL_FILEEXT){
		    config.emusys = $3;
		    c_printf("CONF: config.emusys = '%s'\n", $3);
		    }}
		| DOSEMUMAP STRING
		    {
#ifdef USE_MHPDBG
		    extern char dosemu_map_file_name[];
		    strcpy(dosemu_map_file_name, $2);
		    c_printf("CONF: dosemu.map path = '%s'\n", $2);
#endif
		    free($2);
		    }
 		| EMUBAT STRING
		    { IFCLASS(CL_FILEEXT){
		    config.emubat = $2;
		    c_printf("CONF: config.emubat = '%s'\n", $2);
                    }}
                | EMUINI STRING
                    { IFCLASS(CL_FILEEXT){
                    config.emuini = $2;
                    c_printf("CONF: config.emuini = '%s'\n", $2);
                    }}
                | EMUINI '{' STRING '}'
                    { IFCLASS(CL_FILEEXT){
                    config.emuini = $3;
                    c_printf("CONF: config.emuini = '%s'\n", $3);
		    }}
		| EMUBAT '{' STRING '}'
		    { IFCLASS(CL_FILEEXT){
		    config.emubat = $3;
		    c_printf("CONF: config.emubat = '%s'\n", $3);
		    }}
		| FASTFLOPPY floppy_bool
			{ IFCLASS(CL_FLOPPY){ 
			config.fastfloppy = $2;
			c_printf("CONF: fastfloppy = %d\n", config.fastfloppy);
			}}
		| CPU INTEGER		{ vm86s.cpu_type = ($2/100)%10; }
		| BOOTA
                    { IFCLASS(CL_BOOT){
		      if (priv_lvl)
			yyerror("bootA is illegal in user config file");
		      config.hdiskboot = 0;
		    }}
		| BOOTC
                    { IFCLASS(CL_BOOT){
		      if (priv_lvl)
			yyerror("bootC is illegal in user config file");
		      config.hdiskboot = 1;
		    }}
		| BOOTB
                    { IFCLASS(CL_BOOT){
		      if (priv_lvl)
			yyerror("bootB is illegal in user config file\n");
		      config.hdiskboot = 2;
		    }}
		| TIMINT bool
		    {
		    config.timers = $2;
		    c_printf("CONF: timint %s\n", ($2) ? "on" : "off");
		    }
		| TIMER INTEGER
		    {
		    config.freq = $2;
		    config.update = 1000000 / $2;
		    c_printf("CONF: timer freq=%d, update=%d\n",config.freq,config.update);
		    }
		| LOGBUFSIZE INTEGER
		    {
		      extern int logbuf_size;
		      extern char *logbuf, *logptr;
		      char *b;
		      flush_log();
		      b = malloc($2+1024);
		      if (!b) {
			error("cannot get logbuffer\n");
			exit(1);
		      }
		      logptr = logbuf = b;
		      logbuf_size = $2;
		    }
		| DOSBANNER bool
		    {
		    config.dosbanner = $2;
		    c_printf("CONF: dosbanner %s\n", ($2) ? "on" : "off");
		    }
		| ALLOWVIDEOPORT bool
		    { IFCLASS(CL_VPORT){
		    if ($2 && !config.allowvideoportaccess && priv_lvl)
		      yyerror("Can not enable video port access in use config file");
		    config.allowvideoportaccess = $2;
		    c_printf("CONF: allowvideoportaccess %s\n", ($2) ? "on" : "off");
		    }}
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
		| L_UMB bool
		    {
		    config.max_umb = $2;
		    if ($2 > 0) c_printf("CONF: maximize umb's %s\n", ($2) ? "on" : "off");
		    }
		| L_SECURE bool
		    { IFCLASS(CL_SECURE){
		    if (priv_lvl && config.secure && (! $2))
			yyerror("secure option is illegal in user config file");
		    config.secure = $2;
		    if (config.secure > 0) c_printf("CONF: security %s\n", ($2) ? "on" : "off");
		    }}
		| L_DPMI mem_bool
		    { IFCLASS(CL_DPMI){
		    config.dpmi = $2;
		    c_printf("CONF: DPMI-Server %s\n", ($2) ? "on" : "off");
		    }}
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
                      c_printf("CONF: allowing speaker port access!");
#if 0  /* this is now handled in timers.c */
		      allow_io(0x42, 1, IO_RDWR, 0, 0xFFFF, 1, NULL);
		      allow_io(0x61, 1, IO_RDWR, 0, 0xFFFF, 1, NULL);
#endif
		      }
		    else
                      c_printf("CONF: not allowing speaker port access\n");
		    config.speaker = $2;
		    }
		| VIDEO
		    { IFCLASS(CL_VIDEO) start_video(); }
		  '{' video_flags '}'
		    { stop_video(); }
		| TERMINAL
		    { start_terminal(); }
                  '{' term_flags '}'
		    { stop_terminal(); }
		| DEBUG
		    { start_debug(); }
		  '{' debug_flags '}'
		| MOUSE
		    { start_mouse(); }
		  '{' mouse_flags '}'
		    { stop_mouse(); }
                | TTYLOCKS
                    { start_ttylocks(); }
                  '{' ttylocks_flags '}'
                    { stop_ttylocks(); }
		| SERIAL
		    { start_serial(); }
		  '{' serial_flags '}'
		    { stop_serial(); }
		| KEYBOARD
		    { start_keyboard(); }
	          '{' keyboard_flags '}'
 		| PRESTROKE STRING
		    {
		    append_pre_strokes($2);
		    free($2);
		    c_printf("CONF: appending pre-strokes '%s'\n", $2);
		    }
		| PORTS
		    { IFCLASS(CL_PORT) start_ports(); }
		  '{' port_flags '}'
		| DISK
		    { IFCLASS(CL_DISK) start_disk(); }
		  '{' disk_flags '}'
		    { stop_disk(DISK); }
		| BOOTDISK
		    { IFCLASS(CL_BOOT) start_bootdisk(); }
		  '{' disk_flags '}'
		    { stop_disk(BOOTDISK); }
		| L_FLOPPY
		    { IFCLASS(CL_FLOPPY) start_floppy(); }
		  '{' disk_flags '}'
		    { stop_disk(L_FLOPPY); }
		| PRINTER
		    { IFCLASS(CL_PRINTER) start_printer(); }
		  '{' printer_flags '}'
		    { stop_printer(); }
		| L_X { IFCLASS(CL_X); } '{' x_flags '}'
		| L_SOUND bool	{ if (! $2) config.sb_irq = 0; }
                | L_SOUND { IFCLASS(CL_SOUND); } '{' sound_flags '}'
		| SILLYINT
                    { IFCLASS(CL_IRQ) config.sillyint=0; }
                  '{' sillyint_flags '}'
		| SILLYINT irq_bool
                    { IFCLASS(CL_IRQ)
		      config.sillyint = 1 << $2;
		      c_printf("CONF: IRQ %d for irqpassing\n", $2); 
		    }
		| DEXE { IFCLASS(CL_DEXE); } '{' dexeflags '}'
		| HARDWARE_RAM
                    { IFCLASS(CL_HARDRAM);
		    if (priv_lvl)
		      yyerror("Can not change hardware ram access settings in user config file");
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
		| X_SHARECMAP		{ config.X_sharecmap = 1; }
		| X_MITSHM              { config.X_mitshm = 1; }
		| X_FONT STRING		{ config.X_font = $2; }
		| X_FIXED_ASPECT bool   { config.X_fixed_aspect = $2; }
		| X_ASPECT_43           { config.X_aspect_43 = 1; }
		| X_LIN_FILT            { config.X_lin_filt = 1; }
		| X_BILIN_FILT          { config.X_bilin_filt = 1; }
		| X_MODE13FACT INTEGER  { config.X_mode13fact = $2; }
		| X_WINSIZE INTEGER INTEGER
                   {
                     config.X_winsize_x = $2;
                     config.X_winsize_y = $3;
                   }
		;

dexeflags	: dexeflag
		| dexeflags dexeflag
		;

dexeflag	: ALLOWDISK	{ if (!priv_lvl) dexe_forbid_disk = 0; }
		| L_SECURE	{ dexe_secure = 1; }
		| FORCEXDOS	{
			char *env = getenv("DISPLAY");
			if (env && env[0]) config.X = 1;
		}
		;

	/* sb emulation */
 
sound_flags	: sound_flag
		| sound_flags sound_flag
		;
sound_flag	: SB_BASE INTEGER	{ config.sb_base = $2; }
		| SB_DMA INTEGER	{ config.sb_dma = $2; }
		| SB_IRQ INTEGER	{ config.sb_irq = $2; }
                | SB_MIXER STRING       { config.sb_mixer = $2; }
                | SB_DSP STRING         { config.sb_dsp = $2; }
		| MPU_BASE INTEGER	{ config.mpu401_base = $2; }
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
		| VBIOS_SIZE_TOK INTEGER
		   {
		   config.vbios_size = $2;
		   c_printf("CONF: VGA-BIOS-Size %x\n", $2);
		   if (($2 != 0x8000) && ($2 != 0x10000))
		      {
		      config.vbios_size = 0x10000;
		      c_printf("CONF: VGA-BIOS-Size set to 0x10000\n");
		      }
		   }
		| DUALMON		{ config.dualmon = 1; }
		| FORCE_VT_SWITCH	{ config.force_vt_switch = 1; }
		| STRING
		    { yyerror("unrecognized video option '%s'", $1);
		      free($1); }
		| error
		;

	/* terminal */

term_flags	: term_flag
		| term_flags term_flag
		;
term_flag	/* : METHOD method_val	{ config.term_method = $2; } */
		/* | UPDATELINES INTEGER	{ config.term_updatelines = $2; } */
                : ESCCHAR INTEGER       { config.term_esc_char = $2; }
		| UPDATEFREQ INTEGER	{ config.term_updatefreq = $2; }
		| CHARSET CHARSET_TYPE	{ config.term_charset = $2; }
		| COLOR color_val	{ config.term_color = $2; }
		/* | CORNER bool		{ config.term_corner = $2; } */
		| STRING
		    { yyerror("unrecognized terminal option '%s'", $1);
		      free($1); }
		| error
		;

color_val	: L_OFF			{ $$ = 0; }
		| L_ON			{ $$ = 1; }
		/* | NORMAL		{ $$ = COLOR_NORMAL; } */
		/* | XTERM			{ $$ = COLOR_XTERM; } */
		;

/* method_val	: FAST			{ $$ = METHOD_FAST; } */
/* 		| NCURSES		{ $$ = METHOD_NCURSES; } */
/* 		; */

	/* debugging */

debug_flags	: debug_flag
		| debug_flags debug_flag
		;
debug_flag	: VIDEO bool		{ d.video = $2; }
		| L_OFF			{ memset(&d, 0, sizeof(d)); }
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
		| SOUND	bool		{ d.sound = $2; }
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
		| EMULATE3BUTTONS	{ mptr->emulate3buttons = TRUE; }
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
		| MS3BUTTON
		  {
		  mptr->type = MOUSE_MS3BUTTON;
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

	/* lock files */

ttylocks_flags	: ttylocks_flag
		| ttylocks_flags ttylocks_flag
		;
ttylocks_flag	: DIRECTORY STRING	{ config.tty_lockdir = $2; }
		| NAMESTUB STRING	{ config.tty_lockfile = $2; }
		| BINARY		{ config.tty_lockbinary = TRUE; }
		| STRING
		    { yyerror("unrecognized ttylocks flag '%s'", $1); free($1); }
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
		| BASE INTEGER		{ pptr->base_port = $2; }
		| STRING
		    { yyerror("unrecognized printer flag %s", $1); free($1); }
		| error
		;

	/* disks */

optbootfile	: BOOTFILE STRING
		  {
                  if (priv_lvl)
                    yyerror("Can not use BOOTFILE in the user config file\n");
		  if (dptr->boot_name != NULL)
		    yyerror("Two names for a boot-image file or device given.");
		  dptr->boot_name = $2;
                  }
		| /* empty */
		;

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
		| DEVICE STRING optbootfile
		  {
                  if (priv_lvl)
                    yyerror("Can not use DISK/DEVICE in the user config file\n");
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
		| WHOLEDISK STRING optbootfile
		  {
                  if (priv_lvl)
                    yyerror("Can not use DISK/WHOLEDISK in the user config file\n");
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a harddisk given.");
		  dptr->type = HDISK;
		  dptr->dev_name = $2;
		  }
		| L_FLOPPY STRING
		  {
                  if (priv_lvl)
                    yyerror("Can not use DISK/FLOPPY in the user config file\n");
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a floppy-device given.");
		  dptr->type = FLOPPY;
		  dptr->dev_name = $2;
		  }
		| L_PARTITION STRING INTEGER optbootfile
		  {
                  yywarn("{ partition \"%s\" %d } the"
			 " token '%d' is ignored and can be removed.",
			 $2,$3,$3);
		  do_part($2);
		  }
		| L_PARTITION STRING optbootfile
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
	                    ports_andmask,portspeed, (char*)dev_name);
	           }
		| RANGE INTEGER INTEGER
		   {
		   c_printf("CONF: range of I/O ports 0x%04x-0x%04x\n",
			    (unsigned short)$2, (unsigned short)$3);
		   allow_io($2, $3 - $2 + 1, ports_permission, ports_ormask,
			    ports_andmask, portspeed, (char*)dev_name);
		   portspeed=0;
		   strcpy(dev_name,"");
		   }
		| RDONLY		{ ports_permission = IO_READ; }
		| WRONLY		{ ports_permission = IO_WRITE; }
		| RDWR			{ ports_permission = IO_RDWR; }
		| ORMASK INTEGER	{ ports_ormask = $2; }
		| ANDMASK INTEGER	{ ports_andmask = $2; }
                | FAST	                { portspeed = 1; }
                | DEVICE STRING         { strcpy(dev_name,$2); free($2); } 
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
/* is there a technical reason why the EMS frame can't be at 0xC0000 or
   0xA0000 if there's space? */
#if 0
		     if ( (($2 & 0xfc00)>=0xc800) && (($2 & 0xfc00)<=0xe000) ) {
		       config.ems_frame = $2 & 0xfc00;
		       c_printf("CONF: EMS-frame = 0x%04x\n", config.ems_frame);
		     }
		     else yyerror("wrong EMS-frame: 0x%04x", $2);
#endif
	             config.ems_frame = $2 & 0xfc00;
		     c_printf("CONF: EMS-frame = 0x%04x\n", config.ems_frame);
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
		    { yyerror("unrecognized hardware ram command '%s'", $1);
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

floppy_bool	: L_YES		{ $$ = 2; }
		| L_NO		{ $$ = 0; }
		| L_ON		{ $$ = 2; }
		| L_OFF		{ $$ = 0; }
		| INTEGER	{ $$ = $1; }
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

static void start_mouse(void)
{
  if (c_mouse >= MAX_MOUSE)
    mptr = &nullmouse;
  else {
    mptr = &mice[c_mouse];
    mptr->fd = -1;
  }
}

static void stop_mouse(void)
{
  if (c_mouse >= MAX_MOUSE) {
    c_printf("MOUSE: too many mice, ignoring %s\n", mptr->dev);
    return;
  }
  c_mouse++;
  config.num_mice = c_mouse;
  c_printf("MOUSE: %s type %x using internaldriver: %s, emulate3buttons: %s baudrate: %d\n", 
        mptr->dev, mptr->type, mptr->intdrv ? "yes" : "no", mptr->emulate3buttons ? "yes" : "no", mptr->baudRate);
}

	/* debug */

static void start_ports(void)
{
  if (priv_lvl)
    yyerror("Can not change port privileges in user config file");
  ports_permission = IO_RDWR;
  ports_ormask = 0;
  ports_andmask = 0xFFFF;
}

	/* debug */

static void start_debug(void)
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
  d.sound = flag;
}

	/* video */

static void start_video(void)
{
  config.vbios_file = NULL;
  config.vbios_copy = 0;
  config.vbios_seg  = 0xc000;
  config.vbios_size = 0x10000;
  config.console_video = 0;
  config.cardtype = CARD_VGA;
  config.chipset = PLAINVGA;
  config.mapped_bios = 0;
  config.graphics = 0;
  config.vga = 0;
  config.gfxmemsize = 256;
  config.fullrestore = 0;
  config.dualmon = 0;
  config.force_vt_switch = 0;
}

static void stop_video(void)
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


        /* tty lock files */
static void start_ttylocks(void)
{
  if (priv_lvl)
    yyerror("Can not change lock file settings in user config file");
}


static void stop_ttylocks(void)
{
  c_printf("SER: directory %s namestub %s binary %s\n", config.tty_lockdir,
	   config.tty_lockfile,(config.tty_lockbinary?"Yes":"No"));
}

	/* serial */

static void start_serial(void)
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


static void stop_serial(void)
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

	/* keyboard */

static void start_keyboard(void)
{
  keyb_layout(-1);
  config.console_keyb = 0;
  config.keybint = 0;
}

	/* terminal */

static void start_terminal(void)
{
   /* config.term_method = METHOD_FAST; */
   /* config.term_updatelines = 25; */
  config.term_updatefreq = 4;
  config.term_charset = CHARSET_LATIN;
  config.term_color = 1;
   config.term_esc_char = 30;	       /* Ctrl-^ */
   /* config.term_corner = 1; */
}

static void stop_terminal(void)
{
  if (config.term_updatefreq > 100) {
    yywarn("terminal updatefreq too large (too slow)!");
    config.term_updatefreq = 100;
  } 
}

	/* printer */

static void start_printer(void)
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

static void stop_printer(void)
{
  c_printf("CONF(LPT%d) f: %s   c: %s  o: %s  t: %d  port: %x\n",
	   c_printers, pptr->dev, pptr->prtcmd, pptr->prtopt,
           pptr->delay, pptr->base_port);
  c_printers++;
  config.num_lpt = c_printers;
}

	/* disk */

static void start_bootdisk(void)
{
  if (priv_lvl)
    yyerror("Can not change disk settings in user config file");

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
  dptr->boot_name = NULL;
  dptr->wantrdonly = 0;
  dptr->header = 0;
}

static void start_floppy(void)
{
  if (priv_lvl)
    yyerror("Can not change disk settings in user config file");

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
  dptr->boot_name = NULL;
  dptr->wantrdonly = 0;
  dptr->header = 0;
}

static void start_disk(void)
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
  dptr->boot_name = NULL;
  dptr->wantrdonly = 0;
  dptr->header = 0;
}

static void do_part(char *dev)
{
  if (priv_lvl)
    yyerror("Can not use DISK/PARTITION in the user config file\n");

  if (dptr->dev_name != NULL)
    yyerror("Two names for a partition given.");
  dptr->type = PARTITION;
  dptr->dev_name = dev;
#ifdef __NetBSD__
  dptr->part_info.number = dptr->dev_name[strlen(dptr->dev_name)-1] - 'a' + 1;
#endif
#ifdef __linux__
  dptr->part_info.number = atoi(dptr->dev_name+8);
#endif
  if (dptr->part_info.number == 0) 
    yyerror("%s must be a PARTITION, can't find number suffix!\n",
   	    dptr->dev_name);
}

static void stop_disk(int token)
{
#ifdef __linux__
  FILE   *f;
  struct mntent *mtab;
#endif
#ifdef __NetBSD__
  struct statfs *statfs, *rstatfs;
  register int i, j;
#endif
  int    mounted_rw;

  if (dexe_running && dexe_forbid_disk)
    return;
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

  if (dptr->type == PARTITION) {
    c_printf("partition# %d ", dptr->part_info.number);
#ifdef __linux__
    mtab = NULL;
    if ((f = setmntent(MOUNTED, "r")) != NULL) {
      while ((mtab = getmntent(f)))
        if (!strcmp(dptr->dev_name, mtab->mnt_fsname)) break;
      endmntent(f);
    }
    if (mtab) {
      mounted_rw = ( hasmntopt(mtab, MNTOPT_RW) != NULL );
      if (mounted_rw && !dptr->wantrdonly) 
        yyerror("\n\nYou specified '%s' for read-write Direct Partition Access,"
                "\nit is currently mounted read-write on '%s' !!!\n",
                dptr->dev_name, mtab->mnt_dir);
      else if (mounted_rw) 
        yywarn("You specified '%s' for read-only Direct Partition Access,"
               "\n         it is currently mounted read-write on '%s'.\n",
               dptr->dev_name, mtab->mnt_dir);
      else if (!dptr->wantrdonly) 
        yywarn("You specified '%s' for read-write Direct Partition Access,"
               "\n         it is currently mounted read-only on '%s'.\n",
               dptr->dev_name, mtab->mnt_dir);
    }
#endif
#ifdef __NetBSD__
    i = getmntinfo(&statfs, 0);
    rstatfs = NULL;
    if (i > 0) for (j = 0; j < i; j++) {
	char *cp1, *cp2;
	if (!strcmp(statfs[j].f_mntfromname, dptr->dev_name)) {
	    rstatfs = &statfs[j];
	    break;
	}
	cp1 = strrchr(statfs[j].f_mntfromname, '/');
	cp2 = strrchr(dptr->dev_name, '/');
	if (cp1 && cp2) {
	    /* lop off leading 'r' for raw device on dptr->dev_name */
	    if (!strcmp(cp1+1, cp2+2)) {
		rstatfs = &statfs[j];
		break;
	    }
	}
    }
    if (rstatfs) {
      mounted_rw = ((rstatfs->f_flags & MNT_RDONLY) == 0);
      if (mounted_rw && !dptr->wantrdonly) 
        yyerror("\n\nYou specified '%s' for read-write Direct Partition Access,"
                "\nit is currently mounted read-write on '%s' !!!\n",
                dptr->dev_name, rstatfs->f_mntonname);
      else if (mounted_rw) 
        yywarn("You specified '%s' for read-only Direct Partition Access,"
               "\n         it is currently mounted read-write on '%s'.\n",
               dptr->dev_name, rstatfs->f_mntonname);
      else if (!dptr->wantrdonly) 
        yywarn("You specified '%s' for read-write Direct Partition Access,"
               "\n         it is currently mounted read-only on '%s'.\n",
               dptr->dev_name, rstatfs->f_mntonname);
    }
#endif
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
  if (layout == -1)
    layout = KEYB_US;
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
  case KEYB_DE:
    c_printf("CONF: Keyboard-layout de\n");
    config.keyboard  = KEYB_DE;
    config.key_map   = key_map_de;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_de;
    config.alt_map   = alt_map_de;
    config.num_table = num_table_comma;
    break;
  case KEYB_DE_LATIN1:
    c_printf("CONF: Keyboard-layout de-latin1\n");
    config.keyboard  = KEYB_DE_LATIN1;
    config.key_map   = key_map_de_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_de_latin1;
    config.alt_map   = alt_map_de_latin1;
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
  case KEYB_NO_LATIN1:
    c_printf("CONF: Keyboard-layout no-latin1\n");
    config.keyboard  = KEYB_NO_LATIN1;
    config.key_map   = key_map_no_latin1;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_no_latin1;
    config.alt_map   = alt_map_no_latin1;
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
  case KEYB_SW:
    c_printf("CONF: Keyboard-layout sw\n");
    config.keyboard  = KEYB_SW;
    config.key_map   = key_map_sw;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_sw;
    config.alt_map   = alt_map_sw;
    config.num_table = num_table_comma;
    break;
  case KEYB_HU:
    c_printf("CONF: Keyboard-layout hu\n");
    config.keyboard  = KEYB_HU;
    config.key_map   = key_map_hu;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_hu;
    config.alt_map   = alt_map_hu;
    config.num_table = num_table_comma;
    break;
  case KEYB_HU_CWI:
    c_printf("CONF: Keyboard-layout hu-cwi\n");
    config.keyboard  = KEYB_HU_CWI;
    config.key_map   = key_map_hu_cwi;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_hu_cwi;
    config.alt_map   = alt_map_hu_cwi;
    config.num_table = num_table_comma;
    break;
  case KEYB_HU_LATIN2:
    c_printf("CONF: Keyboard-layout hu-latin2\n");
    config.keyboard  = KEYB_HU_LATIN2;
    config.key_map   = key_map_hu_latin2;  /* pointer to the keyboard-map */
    config.shift_map = shift_map_hu_latin2;
    config.alt_map   = alt_map_hu_latin2;
    config.num_table = num_table_comma;
    break;
  default:
    c_printf("CONF: ERROR -- Keyboard has incorrect number!!!\n");
  }
}

static int set_hardware_ram(int addr)
{
  if ((addr>=0xc800) && (addr<0xf000))   addr *= 0x10;
  if ((addr<0xc8000) || (addr>=0xf0000)) return 0;
  memcheck_reserve('h', addr, 4096);
  config.must_spare_hardware_ram=1;
  config.hardware_pages[(addr-0xc8000) >> 12]=1;
  return 1;
}


static void set_irq_value(int bits, int i1)
{
  if ((i1>2) && (i1<=15)) {
    config.sillyint |= (bits << i1);
    c_printf("CONF: IRQ %d for irqpassing", i1);
    if (bits & 0x10000)  c_printf(" uses SIGIO\n");
    else c_printf("\n");
  }
  else yyerror("wrong IRQ for irqpassing command: %d", i1);
}

static void set_irq_range(int bits, int i1, int i2) {
  int i;
  if ( (i1<3) || (i1>15) || (i2<3) || (i2>15) || (i1 > i2 ) ) {
    yyerror("wrong IRQ range for irqpassing command: %d .. %d", i1, i2);
  }
  else {
    for (i=i1; i<=i2; i++) config.sillyint |= (bits << i);
    c_printf("CONF: range of IRQs for irqpassing %d .. %d", i1, i2);
    if (bits & 0x10000)  c_printf(" uses SIGIO\n");
    else c_printf("\n");
  }
}


	/* errors & warnings */

static void yywarn(char* string, ...)
{
  va_list vars;
  va_start(vars, string);
  fprintf(stderr, "Warning: ");
  vfprintf(stderr, string, vars);
  fprintf(stderr, "\n");
  va_end(vars);
  warnings++;
}

void yyerror(char* string, ...)
{
  va_list vars;
  va_start(vars, string);
  fprintf(stderr, "Error in %s: (line %.3d) ", file_being_parsed, line_count);
  vfprintf(stderr, string, vars);
  fprintf(stderr, "\n");
  va_end(vars);
  errors++;
}


/*
 * open_file - opens the configuration-file named *filename and returns
 *             a file-pointer. The error/warning-counters are reset to zero.
 */

static FILE *open_file(char *filename)
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

static void close_file(FILE * file)
{
  fclose(file);                  /* Close the config-file */

  if(errors)
    fprintf(stderr, "%d error(s) detected while parsing the configuration-file\n",
	    errors);
  if(warnings)
    fprintf(stderr, "%d warning(s) detected while parsing the configuration-file\n",
	    warnings);

  if (errors != 0)               /* Exit dosemu on errors */
    {
      config.exitearly = TRUE;
    }
}

/* write_to_syslog */
static void write_to_syslog(char *message)
{
  openlog("dosemu", LOG_PID, LOG_USER | LOG_NOTICE);
  syslog(LOG_PID | LOG_USER | LOG_NOTICE, message);
  closelog();
}

/* mail_to_root */
static void mail_to_root(char *subject, char *message)
{
  char buf[256];
  sprintf(buf, "echo \"%s\" | mail -s \"%s\" root", message, subject);
  system(buf);
}

/* Parse Users for DOSEMU, by Alan Hourihane, alanh@fairlite.demon.co.uk */
/* Jan-17-1996: Erik Mouw (J.A.K.Mouw@et.tudelft.nl)
 *  - added logging facilities
 */
static void
parse_dosemu_users(void)
{
#define ALL_USERS "all"
#define PBUFLEN 80

  FILE *volatile fp;
  struct passwd *pwd;
  char buf[PBUFLEN];
  int userok = 0;
  char *ustr;
  int log_mail=0;
  int log_syslog=0;
  int uid;
  int have_vars=0;

  /* Get the log level*/
  if((fp = open_file(DOSEMU_LOGLEVEL_FILE)))
     {
       while (!feof(fp))
	 {
	   fgets(buf, PBUFLEN, fp);
	   if(buf[0]!='#')
	     {
	       if(strstr(buf, "mail_error")!=NULL)
		 log_mail=1;
	       else if(strstr(buf, "mail_always")!=NULL)
		 log_mail=2;
	       else if(strstr(buf, "syslog_error")!=NULL)
		 log_syslog=1;
	       else if(strstr(buf, "syslog_always")!=NULL)
		 log_syslog=2;
	     }
	 }
       fclose(fp);
     }

  /* We want to test if the _user_ is allowed to run Dosemu,             */
  /* so check if the username connected to the get_orig_uid() is in the  */
  /* DOSEMU_USERS_FILE file (usually /etc/dosemu.users).                  */
   
  uid = get_orig_uid();

  pwd = getpwuid(uid);

  /* Sanity Check, Shouldn't be anyone logged in without a userid */
  if (pwd  == (struct passwd *)0) 
     {
       fprintf(stderr, "Illegal User!!!\n");
       sprintf(buf, "Illegal DOSEMU user: uid=%i", uid);
       mail_to_root("Illegal user", buf);
       write_to_syslog(buf);
       exit(1);
     }

     {
       enter_priv_on();
       fp = open_file(DOSEMU_USERS_FILE);
       leave_priv_setting();
       if (fp)
	 {
	   for(userok=0; fgets(buf, PBUFLEN, fp) != NULL && !userok; ) {
	     if (buf[0] != '#') {
	       int l = strlen(buf);
	       if (l && (buf[l-1] == '\n')) buf[l-1] = 0;
		ustr = strtok(buf, " \t\n,;:");
	       if (strcmp(ustr, pwd->pw_name)== 0) 
		 userok = 1;
	       else if (strcmp(ustr, ALL_USERS)== 0)
		 userok = 1;
	       if (userok) {
		 while ((ustr=strtok(0, " \t,;:")) !=0) {
		   define_config_variable(ustr);
		   have_vars = 1;
		 }
		 if (!have_vars) {
		   if (uid) define_config_variable("c_normal");
		   else define_config_variable("c_all");
		   have_vars = 1;
		 }
	       }
	     }
	   }
	 } 
       else 
	 {
	   fprintf(stderr,
		   "Cannot open %s, Please check installation via System Admin.\n",
		   DOSEMU_USERS_FILE);
	   fprintf(stdout,
		   "Cannot open %s, Please check installation via System Admin.\n",
		   DOSEMU_USERS_FILE);
	 }
       fclose(fp);
     }
  if (uid == 0) {
     if (!have_vars) define_config_variable("c_all");
     userok=1;  /* This must be root, so always allow DOSEMU start */
  }

  if(userok==0) 
     {
       fprintf(stderr,
	       "Sorry %s. You are not allowed to use DOSEMU. Contact System Admin.\n",
	       pwd->pw_name);

       sprintf(buf, "Illegal DOSEMU start attempt by %s (uid=%i)", 
       pwd->pw_name, uid);
            
       if(log_syslog>=1)
         {
	   fprintf(stderr, "This event will be logged!\n");
	   write_to_syslog(buf);
	 }
	 
       if(log_mail>=1)
         {
	   fprintf(stderr, "This event will be reported to root!\n");
	   mail_to_root("Illegal DOSEMU start", buf);
	 }
            
       exit(1);
     }
  else
     {
       sprintf(buf, "DOSEMU start by %s (uid=%i)", pwd->pw_name, uid);
            
       if(log_syslog>=2)
         write_to_syslog(buf);

       if(log_mail>=2)
	 mail_to_root("DOSEMU start", buf);
     }
}


char *commandline_statements=0;

static int has_dexe_magic(char *name)
{
  int fd, magic, ret;
  enter_priv_off();
  fd = open(name, O_RDONLY);
  leave_priv_setting();
  if (fd <0) return 0;
  ret = (read(fd, &magic, 4) == 4) && (magic == DEXE_MAGIC);
  close(fd);
  return ret;
}

static int stat_dexe(char *name)
{
  struct stat s;
  extern int is_in_groups(gid_t);
  if (stat(name, &s)) return 0;
  if ( ! S_ISREG(s.st_mode)) return 0;
  if ((s.st_mode & S_IXUSR) && (s.st_uid == get_orig_uid()))
    return has_dexe_magic(name);
  if ((s.st_mode & S_IXGRP) && (is_in_groups(s.st_gid)))
    return  has_dexe_magic(name); 
  if (s.st_mode & S_IXOTH) return has_dexe_magic(name);
  return 0;
}

static char *resolve_exec_path(char *dexename, char *ext)
{
  static char n[256];
  static char name[256];
  char *p, *path=getenv("PATH");

  p = rindex(dexename, '.');
  if ( ext && ((p && strcmp(p, ext)) || !p) )
    sprintf(name,"%s%s",dexename,ext);
  else
    strcpy(name,dexename);


  /* first try the pure file name */
  if (!path || (name[0] == '/') || (!strncmp(name, "./", 2))) {
    if (stat_dexe(name)) return name;
    return 0;
  }

  /* next try the standard path for DEXE files */
  sprintf(n, DEXE_LOAD_PATH "/%s", name);
  if (stat_dexe(n)) return n;

  /* now search in the users normal PATH */
  path = strdup(path);
  p= strtok(path,":");
  while (p) {
    sprintf(n, "%s/%s", p, name);
    if (stat_dexe(n)) {
      free(path);
      return n;
    }
    p=strtok(0,":");
  }
  free(path);
  return 0;
}

void prepare_dexe_load(char *name)
{
  char *n, *cbuf;
  int fd, csize;

  enter_priv_on(); /* we need search rights for 'stat' */
  n = resolve_exec_path(name, ".dexe");
  if (!n) {
    n = resolve_exec_path(name, 0);
    if (!n) {
      leave_priv_setting();
      error("DEXE file not found or not executable\n");
      exit(1);
    }
  }
  leave_priv_setting();

  /* now we extract the configuration file */
  fd = open(n, O_RDONLY);

  lseek(fd, HEADER_SIZE + 0x200, SEEK_SET); /* just behind the MBR */
  if ((read(fd, &csize, 4) != 4) || (csize > 0x2000)) {
    error("broken DEXE format, configuration not found\n");
    close(fd);
    exit(1);
  }
  
  /* we use the -I option to feed in the configuration,
   * and we put ours in front of eventually existing options
   */
  if (commandline_statements) {
    cbuf = malloc(csize+1+strlen(commandline_statements)+1);
    read(fd, cbuf, csize);
    cbuf[csize] = '\n';
    strcpy(cbuf+csize+1, commandline_statements);
  }
  else {
    cbuf = malloc(csize+1);
    read(fd, cbuf, csize);
    cbuf[csize] = 0;
  }
  commandline_statements = cbuf;
  close(fd);

  start_disk();
  dptr->type = IMAGE;
  dptr->header = HEADER_SIZE;
  dptr->dev_name = n;
  stop_disk(DISK);
  dexe_running = 1;
}


int
parse_config(char *confname)
{
  FILE *fd;
#if YYDEBUG != 0
  extern int yydebug;

  yydebug  = 1;
#endif

  {
    /* preset the 'include-stack', so files without '/' can be found */
    extern char * include_fnames[];
    include_fnames[0]=strdup("/etc/dosemu.conf");
  }

  /* Parse valid users who can execute DOSEMU */
  parse_dosemu_users();
  if (get_config_variable("c_strict") && strcmp(confname, CONFIG_FILE)) {
     c_printf("CONF: use of option -F %s forbidden by /etc/dosemu.users\n",confname);
     c_printf("CONF: using " CONFIG_FILE " instead\n");
     confname = CONFIG_FILE;
  }

  /* Let's try confname if not null, and fail if not found */
  /* Else try the user's own .dosrc */
  /* If that doesn't exist we will default to CONFIG_FILE */

  { 
    uid_t uid = get_orig_uid();

    char *home = getenv("HOME");
    char *name = malloc(strlen(home) + 20);
    sprintf(name, "%s/.dosrc", home);

    /* privileged options allowed? */
    priv_lvl = uid != 0 && strcmp(confname, CONFIG_FILE);

    /* DEXE together with option F ? */
    if (priv_lvl && dexe_running) {
      /* for security reasons we cannot allow this */
      fprintf(stderr, "user cannot load DEXE file together with option -F\n");
      exit(1);
    }

    if (priv_lvl) define_config_variable("c_user");
    else define_config_variable("c_system");
    if (dexe_running) define_config_variable("c_dexerun");

    if (strcmp(confname, "none")) {
      enter_priv_on();
      fd = open_file(confname);
      leave_priv_setting();
      if (!fd) {
        if (!dexe_running) {
          fprintf(stderr, "Cannot open base config file %s, Aborting DOSEMU.\n",confname);
          exit(1);
        }
      }
      else {
        yyin = fd;
        line_count = 1;
        c_printf("CONF: Parsing %s file.\n", confname);
        file_being_parsed = malloc(strlen(confname) + 1);
        strcpy(file_being_parsed, confname);
        if (yyparse())
          yyerror("error in configuration file %s", confname);
        close_file(fd);
      }
    }
    if (priv_lvl) undefine_config_variable("c_user");
    else undefine_config_variable("c_system");

    /* privileged options allowed for user's config? */
    priv_lvl = uid != 0;
    if (priv_lvl) define_config_variable("c_user");
    define_config_variable("c_dosrc");
    if ((fd = open_file(name)) != 0) {
      c_printf("Parsing %s file.\n", name);
      free(file_being_parsed);
      file_being_parsed = malloc(strlen(name) + 1);
      strcpy(file_being_parsed, name);
      yyin = fd;
      line_count = 1;
      yyrestart(fd);
      if (yyparse())
	yyerror("error in user's configuration file %s", name);
      close_file(fd);
    }
    free(file_being_parsed);
    undefine_config_variable("c_dosrc");

    /* Now we parse any commandline statements from option '-I'
     * We do this under priv_lvl set above, so we have the same secure level
     * as with .dosrc
     */

    if (commandline_statements) {
      #define XX_NAME "commandline"
      extern char *yy_vbuffer;
      define_config_variable("c_comline");
      c_printf("Parsing " XX_NAME  " statements.\n");
      file_being_parsed = malloc(strlen(XX_NAME) + 1);
      strcpy(file_being_parsed, XX_NAME);
      yyin=0;				 /* say: we have no input file */
      yy_vbuffer=commandline_statements; /* this is the input to scan */
      line_count = 1;
      yyrestart(yyin);
      if (yyparse())
	yyerror("error in user's %s statement", XX_NAME);
      free(file_being_parsed);
      undefine_config_variable("c_comline");
    }
  }

  /* check for global settings, that should know the whole settings
   * This we only can do, after having parsed all statements
   */

  if (dexe_running) {
    if (dexe_secure && get_orig_uid())
      config.secure = 1;
  }
  else {
    if (get_config_variable("c_dexeonly")) {
       c_printf("CONF: only execution of DEXE files allowed\n");
       fprintf(stderr, "only execution of DEXE files allowed\n");
       leavedos(99);
    }
  }


#ifdef TESTING
  error("TESTING: parser is terminating program\n");
  leavedos(0);
#endif

  return 1;
}

#define MAX_CONFIGVARIABLES 128
char *config_variables[MAX_CONFIGVARIABLES+1] = {0};
static int config_variables_count = 0;
static int config_variables_last = 0;
static int allowed_classes = -1;



static int is_in_allowed_classes(int mask)
{
  if (!(allowed_classes & mask)) {
    yyerror("unsufficient class privilege to use this configuration option\n");
    leavedos(99);
  }
  return 1;
}

struct config_classes {
	char *class;
	int mask;
} config_classes[] = {
	{"c_all", CL_ALL},
	{"c_normal", CL_ALL & (~(CL_VAR | CL_BOOT | CL_VPORT | CL_SECURE | CL_IRQ | CL_HARDRAM))},
	{"c_fileext", CL_FILEEXT},
	{"c_var", CL_VAR},
	{"c_system", CL_VAR},
	{"c_nice", CL_NICE},
	{"c_floppy", CL_FLOPPY},
	{"c_boot", CL_BOOT},
	{"c_secure", CL_SECURE},
	{"c_vport", CL_VPORT},
	{"c_dpmi", CL_DPMI},
	{"c_video", CL_VIDEO},
	{"c_port", CL_PORT},
	{"c_disk", CL_DISK},
	{"c_x", CL_X},
	{"c_sound", CL_SOUND},
	{"c_irq", CL_IRQ},
	{"c_dexe", CL_DEXE},
	{"c_printer", CL_PRINTER},
	{"c_hardram", CL_HARDRAM},
	{0,0}
};

static int get_class_mask(char *name)
{
  struct config_classes *p = &config_classes[0];
  while (p->class) {
    if (!strcmp(p->class,name)) return p->mask;
    p++;
  }
  return 0;
}

static void update_class_mask(void)
{
  int i, m;
  allowed_classes = 0;
  for (i=0; i< config_variables_count; i++) {
    if ((m=get_class_mask(config_variables[i])) != 0) {
      allowed_classes |= m;
    }
  }
}

char *get_config_variable(char *name)
{
  int i;
  for (i=0; i< config_variables_count; i++) {
    if (!strcmp(name, config_variables[i])) {
      config_variables_last = i;
      return config_variables[i];
    }
  }
  return 0;
}

int define_config_variable(char *name)
{
  if (priv_lvl) {
    if (strncmp(name, "u_", 2)) {
      c_printf("CONF: not enough privilege to define config variable %s\n", name);
      return 0;
    }
  }
  if (!get_config_variable(name)) {
    if (config_variables_count < MAX_CONFIGVARIABLES) {
      config_variables[config_variables_count++] = strdup(name);
      update_class_mask();
    }
    else {
      c_printf("CONF: overflow on config variable list\n");
      return 0;
    }
  }
  c_printf("CONF: config variable %s set\n", name);
  return 1;
}

static int undefine_config_variable(char *name)
{
  if (priv_lvl) {
    if (strncmp(name, "u_", 2)) {
      c_printf("CONF: not enough privilege to undefine config variable %s\n", name);
      return 0;
    }
  }
  if (get_config_variable(name)) {
    int i;
    free(config_variables[config_variables_last]);
    for (i=config_variables_last; i<(config_variables_count-1); i++) {
      config_variables[i] = config_variables[i+1];
    }
    config_variables_count--;
    update_class_mask();
    c_printf("CONF: config variable %s unset\n", name);
    return 1;
  }
  return 0;
}



#ifdef TESTING_MAIN

static void die(char *reason)
{
  error("ERROR: par dead: %s\n", reason);
  leavedos(0);
}

int
main(int argc, char **argv)
{
  if (argc != 2)
    die("no filename!");

  if (!parse_config(argv[1]))
    die("parse failed!\n");
}

#endif
