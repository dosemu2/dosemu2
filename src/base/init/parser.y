/* parser.y
 *
 * Parser version 1     ... before 0.66.5
 * Parser version 2     at state of 0.66.5   97/05/30
 * Parser version 3     at state of 0.97.0.1 98/01/03
 *
 * Note:  starting with version 2, you may protect against version 3 via
 *
 *   ifdef parser_version_3
 *     # version 3 style parser
 *   else
 *     # old style parser
 *   endif
 *
 * Note2: starting with version 3 you _need_ atleast _one_ statement such as
 *
 *   $XYZ = "something"
 *
 * to make the 'new version style check' happy, else dosemu will abort.
 */

%{

#define PARSER_VERSION_STRING "parser_version_3"

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
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>
#include <pwd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#ifdef __linux__
#include <mntent.h>
#endif
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "config.h"
#include "emu.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "disks.h"
#include "port.h"
#ifdef NEW_PORT_CODE
#define allow_io	port_allow_io
#else
int allow_io(unsigned int start, int size, int permission, int ormask,
	 int andmask, unsigned int portspeed, char *device);
#endif
#include "lpt.h"
#include "video.h"
#include "mouse.h"
#include "serial.h"
#include "timers.h"
#include "keymaps.h"
#include "memory.h"
#include "utilities.h"

#include "parsglob.h"
#include "lexer.h"


#define USERVAR_PREF	"dosemu_"
static int user_scope_level;

static int after_secure_check = 0;
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
int dexe_running = 0;
static int dexe_forbid_disk = 1;
static int dexe_secure = 1;
char own_hostname[128];

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
static int saved_priv_lvl = 0; 


static char *file_being_parsed;

			/* this to ensure we are parsing a new style */
static int parser_version_3_style_used = 0;
#define CONFNAME_V3USED "version_3_style_used"

	/* external procedures */

extern int exchange_uids(void);
extern char* strdup(const char *); /* Not defined in string.h :-( */
extern int yylex(); /* exact argument types depend on the way you call bison */
extern void tell_lexer_if(int value);
extern void tell_lexer_loop(int cfile, int value);
extern int parse_debugflags(const char *s, unsigned char flag);

	/* local procedures */

void yyerror(char *, ...);
void yywarn(char *, ...);
void keyb_layout(int value);
static void start_ports(void);
static void start_mouse(void);
static void stop_mouse(void);
static void start_debug(void);
static void start_video(void);
static void stop_video(void);
static void set_vesamodes(int width, int height, int color_bits);
static void start_ttylocks(void);
static void stop_ttylocks(void);
static void start_serial(void);
static void stop_serial(void);
static void start_printer(void);
static void stop_printer(void);
static void start_keyboard(void);
static void keytable_start(int layout);
static void keytable_stop(void);
static void keyb_mod(int wich, int keynum);
static void dump_keytables_to_file(char *name);
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
static int set_hardware_ram(int addr);
static void set_irq_value(int bits, int i1);
static void set_irq_range(int bits, int i1, int i2);
extern void append_pre_strokes(unsigned char *s);
char *get_config_variable(char *name);
int define_config_variable(char *name);
static int undefine_config_variable(char *name);
static void check_user_var(char *name);
static char *run_shell(char *command);
static int for_each_handling(int loopid, char *varname, char *delim, char *list);
char *checked_getenv(const char *name);
static void enter_user_scope(int incstackptr);
static void leave_user_scope(int incstackptr);

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
#define CL_SHELL	0x2000000

static int is_in_allowed_classes(int mask);

	/* variables in lexer.l */

extern FILE* yyin;
extern void yyrestart(FILE *input_file);
%}

%start lines

%union {
	int i_value;
	char *s_value;
	float r_value;
	long long all_value;
	ExprType t_value;
};


%token <i_value> INTEGER L_OFF L_ON L_YES L_NO CHIPSET_TYPE
%token <i_value> CHARSET_TYPE KEYB_LAYOUT
%token <r_value> REAL
%token <s_value> STRING VARIABLE

	/* needed for expressions */
%token EXPRTEST
%token INTCAST REALCAST
%left AND_OP OR_OP XOR_OP SHR_OP SHL_OP
%left NOT_OP /* logical NOT */
%left EQ_OP GE_OP LE_OP '=' '<' '>' NEQ_OP
%left STR_EQ_OP STR_NEQ_OP
%left L_AND_OP L_OR_OP
%left '+' '-'
%left '*' '/'
%left UMINUS UPLUS BIT_NOT_OP

%token	STRLEN STRTOL STRNCMP STRCAT STRPBRK STRSPLIT STRCHR STRRCHR STRSTR
%token	STRDEL STRSPN STRCSPN SHELL
%token	DEFINED
%type	<i_value> expression
%type	<s_value> string_expr variable_content strarglist strarglist_item

	/* flow control */
%token DEFINE UNDEF IFSTATEMENT WHILESTATEMENT FOREACHSTATEMENT
%token <i_value> ENTER_USER_SPACE LEAVE_USER_SPACE

	/* variable handling */
%token CHECKUSERVAR

	/* main options */
%token DOSBANNER FASTFLOPPY TIMINT HOGTHRESH SPEAKER IPXSUPPORT NOVELLHACK
%token VNET
%token DEBUG MOUSE SERIAL COM KEYBOARD TERMINAL VIDEO ALLOWVIDEOPORT TIMER
%token MATHCO CPU CPUSPEED RDTSC BOOTA BOOTB BOOTC L_XMS L_DPMI PORTS DISK DOSMEM PRINTER
%token L_EMS L_UMB EMS_SIZE EMS_FRAME TTYLOCKS L_SOUND
%token L_SECURE
%token DEXE ALLOWDISK FORCEXDOS XDOSONLY
%token ABORT WARN
%token BOOTDISK L_FLOPPY EMUSYS EMUBAT EMUINI L_X
%token DOSEMUMAP LOGBUFSIZE LOGFILESIZE
	/* speaker */
%token EMULATED NATIVE
	/* keyboard */
%token KEYBINT RAWKEYBOARD
%token PRESTROKE
%token KEYTABLE SHIFT ALT NUMPAD DUMP
%token DGRAVE DACUTE DCIRCUM DTILDE DBREVE DABOVED DDIARES DABOVER DDACUTE DCEDILLA DIOTA
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
%token X_GAMMA VGAEMU_MEMSIZE VESAMODE X_LFB X_PM_INTERFACE X_MGRAB_KEY
	/* video */
%token VGA MGA CGA EGA NONE CONSOLE GRAPHICS CHIPSET FULLREST PARTREST
%token MEMSIZE VBIOS_SIZE_TOK VBIOS_SEG VBIOS_FILE VBIOS_COPY VBIOS_MMAP DUALMON
%token FORCE_VT_SWITCH PCI
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
%token SECTORS CYLINDERS TRACKS HEADS OFFSET HDIMAGE DISKCYL4096
	/* ports/io */
%token RDONLY WRONLY RDWR ORMASK ANDMASK RANGE FAST
	/* Silly interrupts */
%token SILLYINT USE_SIGIO
	/* hardware ram mapping */
%token HARDWARE_RAM
        /* Sound Emulation */
%token SB_BASE SB_IRQ SB_DMA SB_MIXER SB_DSP MPU_BASE
	/* CD-ROM */
%token CDROM

	/* we know we have 1 shift/reduce conflict :-( 
	 * and tell the parser to ignore that */
	/* %expect 1 */

/* %type <i_value> mem_bool irq_bool bool speaker method_val color_val floppy_bool */
%type <i_value> mem_bool irq_bool bool speaker color_val floppy_bool

%%

lines		: line
		| lines line
		| lines optdelim line
		;

optdelim	: ';'
		| optdelim ';'
		;

line		: HOGTHRESH expression	{ IFCLASS(CL_NICE) config.hogthreshold = $2; }
		| DEFINE STRING		{ IFCLASS(CL_VAR){ define_config_variable($2);} free($2); }
		| UNDEF STRING		{ IFCLASS(CL_VAR){ undefine_config_variable($2);} free($2); }
		| IFSTATEMENT '(' expression ')' {
			/* NOTE:
			 * We _need_ absolutely to return to state stack 0
			 * because we 'backward' modify the input stream for
			 * the parser (in lexer). Hence, _if_ the parser needs
			 * to read one token more than needed, we are lost,
			 * because we can't discard it. So please, don't
			 * play with the grammar without knowing what you do.
			 * The ')' _will_ return to state stack 0, but fiddling
			 * with brackets '()' in the underlaying 'expression'
			 * rules may distroy this.
			 *                          -- Hans 971231
			 */
			tell_lexer_if($3);
		}
		/* Note: the below syntax of the while__yy__ statement
		 * is internal and _not_ visible out side.
		 * The visible syntax is:
		 *	while ( expression )
		 *	   <loop contents>
		 *	done
		 */
		| WHILESTATEMENT INTEGER ',' '(' expression ')' {
			tell_lexer_loop($2, $5);
		}
		| FOREACHSTATEMENT INTEGER ',' VARIABLE '(' string_expr ',' strarglist ')' {
			tell_lexer_loop($2, for_each_handling($2,$4,$6,$8));
			free($4); free($6); free($8);
		}
		| SHELL '(' strarglist ')' {
			IFCLASS(CL_SHELL) {
				char *s = run_shell($3);
				if (s) free(s);
			}
			else {
				yyerror("not in allowed class to execute SHELL(%s)",$3);
			}
			free($3);
		}
		| ENTER_USER_SPACE { enter_user_scope($1); }
		| LEAVE_USER_SPACE { leave_user_scope($1); }
		| VARIABLE '=' strarglist {
		    if (!parser_version_3_style_used) {
			parser_version_3_style_used = 1;
			define_config_variable(CONFNAME_V3USED);
		    }
		    if ((strpbrk($1, "uhc") == $1) && ($1[1] == '_'))
			yyerror("reserved variable %s can't be set\n", $1);
		    else {
			if (user_scope_level) {
			    char *s = malloc(strlen($1)+sizeof(USERVAR_PREF));
			    strcpy(s, USERVAR_PREF);
			    strcat(s,$1);
			    setenv(s, $3, 1);
			    free(s);
			}
			else IFCLASS(CL_VAR) setenv($1, $3, 1);
		    }
		    free($1); free($3);
		}
		| CHECKUSERVAR check_user_var_list
		| EXPRTEST expression {
		    if (EXPRTYPE($2) == TYPE_REAL)
			c_printf("CONF TESTING: exprtest real %f %08x\n", VAL_R($2), $2);
		    else
			c_printf("CONF TESTING: exprtest int %d %08x\n", VAL_I($2), $2);
		}
		/* abandoning 'single' abort due to shift/reduce conflicts
		   Just use ' abort "" '
		| ABORT			{ leavedos(99); }
		*/
		| ABORT strarglist
		    { if ($2[0]) fprintf(stderr,"CONF aborted with: %s\n", $2);
			exit(99);
		      leavedos(99);
		    }
		| WARN strarglist	{ c_printf("CONF: %s\n", $2); free($2); }
 		| EMUSYS string_expr
		    { IFCLASS(CL_FILEEXT){
		    config.emusys = $2;
		    c_printf("CONF: config.emusys = '%s'\n", $2);
		    }}
		| EMUSYS '{' string_expr '}'
		    { IFCLASS(CL_FILEEXT){
		    config.emusys = $3;
		    c_printf("CONF: config.emusys = '%s'\n", $3);
		    }}
		| DOSEMUMAP string_expr
		    {
#ifdef USE_MHPDBG
		    extern char dosemu_map_file_name[];
		    strcpy(dosemu_map_file_name, $2);
		    c_printf("CONF: dosemu.map path = '%s'\n", $2);
#endif
		    free($2);
		    }
 		| EMUBAT string_expr
		    { IFCLASS(CL_FILEEXT){
		    config.emubat = $2;
		    c_printf("CONF: config.emubat = '%s'\n", $2);
                    }}
                | EMUINI string_expr
                    { IFCLASS(CL_FILEEXT){
                    config.emuini = $2;
                    c_printf("CONF: config.emuini = '%s'\n", $2);
                    }}
                | EMUINI '{' string_expr '}'
                    { IFCLASS(CL_FILEEXT){
                    config.emuini = $3;
                    c_printf("CONF: config.emuini = '%s'\n", $3);
		    }}
		| EMUBAT '{' string_expr '}'
		    { IFCLASS(CL_FILEEXT){
		    config.emubat = $3;
		    c_printf("CONF: config.emubat = '%s'\n", $3);
		    }}
		| FASTFLOPPY floppy_bool
			{ IFCLASS(CL_FLOPPY){ 
			config.fastfloppy = $2;
			c_printf("CONF: fastfloppy = %d\n", config.fastfloppy);
			}}
		| CPU expression
			{
			extern int cpu_override(int);
			int cpu = cpu_override (($2%100)==86?($2/100)%10:0);
			if (cpu > 0) {
				c_printf("CONF: CPU set to %d86\n",cpu);
				vm86s.cpu_type = cpu;
			}
			else
				yyerror("error in CPU user override\n");
			}
		| CPU EMULATED
			{
#ifdef X86_EMULATOR
#ifdef DONT_START_EMU
			config.cpuemu = 1;
#else
			config.cpuemu = 3;
#endif
#endif
			}
		| CPUSPEED expression
			{ 
			if (config.realcpu >= CPU_586) {
			  config.cpu_spd = ((double)LLF_US)/TOF($2);
			  config.cpu_tick_spd = ((double)LLF_TICKS)/TOF($2);
			  c_printf("CONF: CPU speed = %g\n", ((double)TOF($2)));
			}
			}
		| CPUSPEED INTEGER INTEGER
			{ 
			if (config.realcpu >= CPU_586) {
			  config.cpu_spd = (LLF_US*$3)/$2;
			  config.cpu_tick_spd = (LLF_TICKS*$3)/$2;
			  c_printf("CONF: CPU speed = %d/%d\n", $2, $3);
			}
			}
		| RDTSC bool
		    {
		    if (config.smp && ($2!=0)) {
			c_printf("CONF: Denying use of pentium timer on SMP machine\n");
			config.rdtsc = 0;
		    }
		    else if (config.realcpu>=CPU_586) {
			config.rdtsc = ($2!=0);
			c_printf("CONF: %sabling use of pentium timer\n",
				(config.rdtsc?"En":"Dis"));
		    }
		    else
			c_printf("CONF: Ignoring 'rdtsc' statement\n");
		    }
		| PCI bool
		    {
		    config.pci = ($2!=0);
		    }
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
		| TIMER expression
		    {
		    config.freq = $2;
		    config.update = 1000000 / $2;
		    c_printf("CONF: timer freq=%d, update=%d\n",config.freq,config.update);
		    }
		| LOGBUFSIZE expression
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
		| LOGFILESIZE expression
		    {
		      extern int logfile_limit;
		      logfile_limit = $2;
		    }
		| DOSBANNER bool
		    {
		    config.dosbanner = $2;
		    c_printf("CONF: dosbanner %s\n", ($2) ? "on" : "off");
		    }
		| ALLOWVIDEOPORT bool
		    { IFCLASS(CL_VPORT){
		    if ($2 && !config.allowvideoportaccess && priv_lvl)
		      yyerror("Can not enable video port access in user config file");
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
		| VNET bool	{ config.vnet = $2; }
		| SPEAKER speaker
		    {
		    if ($2 == SPKR_NATIVE) {
                      c_printf("CONF: allowing speaker port access!\n");
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
		| DEBUG strarglist {
			parse_debugflags($2, 1);
			free($2);
		}
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
		| KEYTABLE KEYB_LAYOUT
			{keytable_start($2);}
		  '{' keyboard_mods '}'
		  	{keytable_stop();}
 		| PRESTROKE string_expr
		    {
		    append_pre_strokes($2);
		    c_printf("CONF: appending pre-strokes '%s'\n", $2);
		    free($2);
		    }
		| KEYTABLE DUMP string_expr {
			dump_keytables_to_file($3);
			free($3);
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
                | CDROM '{' string_expr '}'
                    { IFCLASS(CL_DISK){
		    strncpy(path_cdrom, $3, 30);
                    c_printf("CONF: cdrom on %s\n", $3);
		    }}
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
                    { IFCLASS(CL_IRQ) if ($2) {
		        config.sillyint = 1 << $2;
		        c_printf("CONF: IRQ %d for irqpassing\n", $2);
		      }
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

	/* expressions */
expression:
		  INTEGER {TYPINT($$);}
		| REAL {TYPREAL($$);}
		| expression '+' expression
			{V_VAL($$,$1) = TOF($1) + TOF($3); }
		| expression '-' expression
			{V_VAL($$,$1) = TOF($1) - TOF($3); }
		| expression '*' expression
			{V_VAL($$,$1) = TOF($1) * TOF($3); }
		| expression '/' expression {
			if ($3)	V_VAL($$,$1) = TOF($1) / TOF($3);
			else V_VAL($$,$1) = TOF($3);
		}
		| '-' expression %prec UMINUS 
			{I_VAL($$) = -TOF($2); }
		| '+' expression %prec UPLUS
			{V_VAL($$,$2) = TOF($2); }
		| expression AND_OP expression
			{I_VAL($$) = VAL_I($1) & VAL_I($3); }
		| expression OR_OP expression
			{I_VAL($$) = VAL_I($1) | VAL_I($3); }
		| expression XOR_OP expression
			{I_VAL($$) = VAL_I($1) ^ VAL_I($3); }
		| BIT_NOT_OP expression %prec BIT_NOT_OP
			{I_VAL($$) = VAL_I($2) ^ (-1); }
		| expression SHR_OP expression
			{ unsigned int shift = (1 << VAL_I($3));
			if (!shift) ALL($$) = ALL($1);
			else V_VAL($$, $1) =  TOF($1) / shift;}
		| expression SHL_OP expression
			{ unsigned int shift = (1 << VAL_I($3));
			if (!shift) ALL($$) = ALL($1);
			else V_VAL($$, $1) =  TOF($1) * shift;}
		| expression EQ_OP expression
			{B_VAL($$) = TOF($1) == TOF($3); }
		| expression NEQ_OP expression
			{B_VAL($$) = TOF($1) != TOF($3); }
		| expression GE_OP expression
			{B_VAL($$) = TOF($1) >= TOF($3); }
		| expression LE_OP expression
			{B_VAL($$) = TOF($1) <= TOF($3); }
		| expression '<' expression
			{B_VAL($$) = TOF($1) < TOF($3); }
		| expression '>' expression
			{B_VAL($$) = TOF($1) > TOF($3); }
		| expression L_AND_OP expression
			{B_VAL($$) = TOF($1) && TOF($3); }
		| expression L_OR_OP expression
			{B_VAL($$) = TOF($1) || TOF($3); }
		| string_expr STR_EQ_OP string_expr
			{B_VAL($$) = strcmp($1,$3) == 0; }
		| string_expr STR_NEQ_OP string_expr
			{B_VAL($$) = strcmp($1,$3) != 0; }
		| NOT_OP expression {B_VAL($$) = (TOF($2) ? 0:1); }
		| L_YES		{B_VAL($$) = 1; }
		| L_NO		{B_VAL($$) = 0; }
		| L_ON		{B_VAL($$) = 1; }
		| L_OFF		{B_VAL($$) = 0; }
		| INTCAST '(' expression ')' {I_VAL($$) = TOF($3);}
		| REALCAST '(' expression ')' {R_VAL($$) = TOF($3);}
		| STRTOL '(' string_expr ')' {
			I_VAL($$) = strtol($3,0,0);
			free($3);
		}
		| STRLEN '(' string_expr ')' {
			I_VAL($$) = strlen($3);
			free($3);
		}
		| STRNCMP '(' string_expr ',' string_expr ',' expression ')' {
			I_VAL($$) = strncmp($3,$5,$7);
			free($3); free($5);
		}
		| STRPBRK '(' string_expr ',' string_expr ')' {
			char *s = strpbrk($3,$5);
			if (s) I_VAL($$) = (int)s - (int)$3;
			else I_VAL($$) = -1;
			free($3); free($5);
		}
		| STRCHR '(' string_expr ',' string_expr ')' {
			char *s = strchr($3,$5[0]);
			if (s) I_VAL($$) = (int)s - (int)$3;
			else I_VAL($$) = -1;
			free($3); free($5);
		}
		| STRRCHR '(' string_expr ',' string_expr ')' {
			char *s = strrchr($3,$5[0]);
			if (s) I_VAL($$) = (int)s - (int)$3;
			else I_VAL($$) = -1;
			free($3); free($5);
		}
		| STRSTR '(' string_expr ',' string_expr ')' {
			char *s = strstr($3,$5);
			if (s) I_VAL($$) = (int)s - (int)$3;
			else I_VAL($$) = -1;
			free($3); free($5);
		}
		| STRSPN '(' string_expr ',' string_expr ')' {
			I_VAL($$) = strspn($3,$5);
			free($3); free($5);
		}
		| STRCSPN '(' string_expr ',' string_expr ')' {
			I_VAL($$) = strcspn($3,$5);
			free($3); free($5);
		}
		| DEFINED '(' STRING ')' {
			B_VAL($$) = get_config_variable($3) !=0;
			free($3);
		}
		| variable_content {
			char *s;
			I_VAL($$) = strtol($1,&s,0);
			switch (*s) {
				case '.':  case 'e':  case 'E':
				/* we assume a real number */
				R_VAL($$) = strtod($1,0);
			}
			free($1);
		}
		| '(' expression ')' {ALL($$) = ALL($2);}
		| STRING {
			if ( $1[0] && !$1[1] && (EXPRTYPE($1) == TYPE_STRING1) )
				I_VAL($$) = $1[0];
			else	yyerror("unrecognized expression '%s'", $1);
			free($1);
		}
		;

variable_content:
		VARIABLE {
			char *s = $1;
			if (get_config_variable(s))
				s = "1";
			else if (strncmp("c_",s,2)
					&& strncmp("u_",s,2)
					&& strncmp("h_",s,2) ) {
				s = checked_getenv(s);
				if (!s) s = "";
			}
			else
				s = "0";
			S_VAL($$) = strdup(s);
			free($1);
		}
		;

string_expr:	STRING {S_VAL($$) = $1;}
		| STRCAT '(' strarglist ')' { S_VAL($$) = $3; }
		| STRSPLIT '(' string_expr ',' expression ',' expression ')' {
			int i = (int)TOF($5);
			int len = (int)TOF($7);
			int slen = strlen($3);
			if ((i >=0) && (i < slen) && (len > 0)) {
				if ((i+len) > slen) len = slen - i;
				$3[i+len] = 0;
				S_VAL($$) = strdup($3 + i);
			}
			else
				S_VAL($$) = strdup("");
			free($3);
		}
		| STRDEL '(' string_expr ',' expression ',' expression ')' {
			int i = (int)TOF($5);
			int len = (int)TOF($7);
			int slen = strlen($3);
			char *s = strdup($3);
			if ((i >=0) && (i < slen) && (len > 0)) {
				if ((i+len) > slen) s[i] = 0;
				else memcpy(s+i, s+i+len, slen-i-len+1);
			}
			free($3);
			S_VAL($$) = s;
		}
		| SHELL '(' strarglist ')' {
			IFCLASS(CL_SHELL) {
				S_VAL($$) = run_shell($3);
			}
			else {
				S_VAL($$) = strdup("");
				yyerror("not in allowed class to execute SHELL(%s)",$3);
			}
			free($3);
		}
		| variable_content {ALL($$) = ALL($1);}
		;

strarglist:	strarglist_item
		| strarglist ',' strarglist_item {
			char *s = malloc(strlen($1)+strlen($3)+1);
			strcpy(s, $1);
			strcat(s, $3);
			S_VAL($$) = s;
			free($1); free($3);
		}
		;

strarglist_item: string_expr
		| '(' expression ')' {
			char buf[128];
			if (EXPRTYPE($2) == TYPE_REAL)
				sprintf(buf, "%g", VAL_R($2));
			else
				sprintf(buf, "%d", VAL_I($2));
			S_VAL($$) = strdup(buf);
		}
		;

check_user_var_list:
		VARIABLE {
			check_user_var($1);
			free($1);
		}
		| check_user_var_list ',' VARIABLE {
			check_user_var($3);
			free($3);
		}
		;

	/* x-windows */

x_flags		: x_flag
		| x_flags x_flag
		;
x_flag		: UPDATELINES expression	{ config.X_updatelines = $2; }
		| UPDATEFREQ expression	{ config.X_updatefreq = $2; }
		| L_DISPLAY string_expr	{ config.X_display = $2; }
		| L_TITLE string_expr	{ config.X_title = $2; }
		| ICON_NAME string_expr	{ config.X_icon_name = $2; }
		| X_KEYCODE		{ config.X_keycode = 1; }
		| X_BLINKRATE expression	{ config.X_blinkrate = $2; }
		| X_SHARECMAP		{ config.X_sharecmap = 1; }
		| X_MITSHM              { config.X_mitshm = 1; }
		| X_MITSHM bool         { config.X_mitshm = $2; }
		| X_FONT string_expr		{ config.X_font = $2; }
		| X_FIXED_ASPECT bool   { config.X_fixed_aspect = $2; }
		| X_ASPECT_43           { config.X_aspect_43 = 1; }
		| X_LIN_FILT            { config.X_lin_filt = 1; }
		| X_BILIN_FILT          { config.X_bilin_filt = 1; }
		| X_MODE13FACT expression  { config.X_mode13fact = $2; }
		| X_WINSIZE INTEGER INTEGER
                   {
                     config.X_winsize_x = $2;
                     config.X_winsize_y = $3;
                   }
		| X_WINSIZE expression ',' expression
                   {
                     config.X_winsize_x = $2;
                     config.X_winsize_y = $4;
                   }
		| X_GAMMA expression  { config.X_gamma = $2; }
		| VGAEMU_MEMSIZE expression	{ config.vgaemu_memsize = $2; }
		| VESAMODE INTEGER INTEGER { set_vesamodes($2,$3,0);}
		| VESAMODE INTEGER INTEGER INTEGER { set_vesamodes($2,$3,$4);}
		| VESAMODE expression ',' expression { set_vesamodes($2,$4,0);}
		| VESAMODE expression ',' expression ',' expression
			{ set_vesamodes($2,$4,$6);}
		| X_LFB bool            { config.X_lfb = $2; }
		| X_PM_INTERFACE bool   { config.X_pm_interface = $2; }
		| X_MGRAB_KEY string_expr { config.X_mgrab_key = $2; }
		;

dexeflags	: dexeflag
		| dexeflags dexeflag
		;

dexeflag	: ALLOWDISK	{ if (!priv_lvl) dexe_forbid_disk = 0; }
		| L_SECURE	{ dexe_secure = 1; }
		| FORCEXDOS	{
			char *env = getenv("DISPLAY");
			if (env && env[0] && dexe_running) config.X = 1;
		}
		| XDOSONLY	{
			char *env = getenv("DISPLAY");
			if (env && env[0] && dexe_running) config.X = 1;
			else if (dexe_running) {
			  yyerror("this DEXE requires X, giving up");
			  leavedos(99);
			}
		}
		;

	/* sb emulation */
 
sound_flags	: sound_flag
		| sound_flags sound_flag
		;
sound_flag	: SB_BASE expression	{ config.sb_base = $2; }
		| SB_DMA expression	{ config.sb_dma = $2; }
		| SB_IRQ expression	{ config.sb_irq = $2; }
                | SB_MIXER string_expr       { config.sb_mixer = $2; }
                | SB_DSP string_expr         { config.sb_dsp = $2; }
		| MPU_BASE expression	{ config.mpu401_base = $2; }
		;

	/* video */

video_flags	: video_flag
		| video_flags video_flag
		;
video_flag	: VGA			{ config.cardtype = CARD_VGA; }
		| MGA			{ config.cardtype = CARD_MDA; }
		| CGA			{ config.cardtype = CARD_CGA; }
		| EGA			{ config.cardtype = CARD_EGA; }
		| NONE			{ config.cardtype = CARD_NONE; }
		| CHIPSET CHIPSET_TYPE
		    {
		    config.chipset = $2;
                    c_printf("CHIPSET: %d\n", $2);
		    if ($2==MATROX) config.pci=config.pci_video=1;
		    }
		| MEMSIZE expression	{ config.gfxmemsize = $2; }
		| GRAPHICS
		    { config.vga =
#if defined(X86_EMULATOR)&&defined(VT_EMU_ONLY)
			!config.cpuemu;
#else
			1;
#endif
		    }
		| CONSOLE
		    { config.console_video =
#if defined(X86_EMULATOR)&&defined(VT_EMU_ONLY)
			!config.cpuemu;
#else
			1;
#endif
		    }
		| FULLREST		{ config.fullrestore = 1; }
		| PARTREST		{ config.fullrestore = 0; }
		| VBIOS_FILE string_expr	{ config.vbios_file = $2;
					  config.mapped_bios = 1;
					  config.vbios_copy = 0; }
		| VBIOS_COPY		{ config.vbios_file = NULL;
					  config.mapped_bios = 1;
					  config.vbios_copy = 1; }
		| VBIOS_MMAP		{ config.vbios_file = NULL;
					  config.mapped_bios = 1;
					  config.vbios_copy = 1; }
		| VBIOS_SEG expression
		   {
		   config.vbios_seg = $2;
		   c_printf("CONF: VGA-BIOS-Segment %x\n", $2);
		   if (($2 != 0xe000) && ($2 != 0xc000))
		      {
		      config.vbios_seg = 0xc000;
		      c_printf("CONF: VGA-BIOS-Segment set to 0xc000\n");
		      }
		   }
		| VBIOS_SIZE_TOK expression
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
		| PCI			{ config.pci_video = 1; }
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
                : ESCCHAR expression       { config.term_esc_char = $2; }
		| UPDATEFREQ expression	{ config.term_updatefreq = $2; }
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
mouse_flag	: DEVICE string_expr		{ strcpy(mptr->dev, $2); free($2); }
		| INTERNALDRIVER	{ mptr->intdrv = TRUE; }
		| EMULATE3BUTTONS	{ mptr->emulate3buttons = TRUE; }
		| BAUDRATE expression	{ mptr->baudRate = $2; }
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
		  mptr->flags = CS8 | CREAD | CLOCAL | HUPCL;
/* is cstopb needed?  mptr->flags = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL; */
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
		| LAYOUT KEYB_LAYOUT {keyb_layout($2);} '{' keyboard_mods '}'
		| LAYOUT L_NO		{ keyb_layout(KEYB_NO); }
		| RAWKEYBOARD bool	{ config.console_keyb = $2; }
		| KEYBINT bool		{ config.keybint = $2; }
		| STRING
		    { yyerror("unrecognized keyboard flag '%s'", $1);
		      free($1);}
		| error
		;

keyboard_mods	: keyboard_mod
		| keyboard_mods keyboard_mod
		;

keyboard_mod	: expression '=' { keyb_mod(' ', $1); } keyboard_modvals
		| SHIFT expression '=' { keyb_mod('S', $2); } keyboard_modvals
		| ALT expression '=' { keyb_mod('A', $2); } keyboard_modvals
		| NUMPAD expression '=' { keyb_mod('N', $2); } keyboard_modvals
		;

keyboard_modvals: keyboard_modval
		| keyboard_modvals ',' keyboard_modval
		;

keyboard_modval : INTEGER { keyb_mod(0, $1); }
		| '(' expression ')' { keyb_mod(0, $2); }
		| DGRAVE { keyb_mod(0, DEAD_GRAVE); }
		| DACUTE { keyb_mod(0, DEAD_ACUTE); }
		| DCIRCUM { keyb_mod(0, DEAD_CIRCUMFLEX); }
		| DTILDE { keyb_mod(0, DEAD_TILDE); }
		| DBREVE { keyb_mod(0, DEAD_BREVE); }
		| DABOVED { keyb_mod(0, DEAD_ABOVEDOT); }
		| DDIARES { keyb_mod(0, DEAD_DIAERESIS); }
		| DABOVER { keyb_mod(0, DEAD_ABOVERING); }
		| DDACUTE { keyb_mod(0, DEAD_DOUBLEACUTE); }
		| DCEDILLA { keyb_mod(0, DEAD_CEDILLA); }
		| DIOTA { keyb_mod(0,DEAD_IOTA); }
		| STRING {
			char *p = $1;
			while (*p) keyb_mod(0, *p++);
			free($1);
		}
		;

	/* lock files */

ttylocks_flags	: ttylocks_flag
		| ttylocks_flags ttylocks_flag
		;
ttylocks_flag	: DIRECTORY string_expr	{ config.tty_lockdir = $2; }
		| NAMESTUB string_expr	{ config.tty_lockfile = $2; }
		| BINARY		{ config.tty_lockbinary = TRUE; }
		| STRING
		    { yyerror("unrecognized ttylocks flag '%s'", $1); free($1); }
		| error
		;

	/* serial ports */

serial_flags	: serial_flag
		| serial_flags serial_flag
		;
serial_flag	: DEVICE string_expr		{ strcpy(sptr->dev, $2);
					  if (!strcmp("/dev/mouse",sptr->dev)) sptr->mouse=1;
					  free($2); }
		| COM expression		{ sptr->real_comport = $2;
					  com_port_used[$2] = 1; }
		| BASE expression		{ sptr->base_port = $2; }
		| IRQ expression		{ sptr->interrupt = $2; }
		| INTERRUPT expression	{ sptr->interrupt = $2; }
		| MOUSE			{ sptr->mouse = 1; }
		| STRING
		    { yyerror("unrecognized serial flag '%s'", $1); free($1); }
		| error
		;

	/* printer */

printer_flags	: printer_flag
		| printer_flags printer_flag
		;
printer_flag	: COMMAND string_expr	{ pptr->prtcmd = $2; }
		| TIMEOUT expression	{ pptr->delay = $2; }
		| OPTIONS string_expr	{ pptr->prtopt = $2; }
		| L_FILE string_expr		{ pptr->dev = $2; }
		| BASE expression		{ pptr->base_port = $2; }
		| STRING
		    { yyerror("unrecognized printer flag %s", $1); free($1); }
		| error
		;

	/* disks */

optbootfile	: BOOTFILE string_expr
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
		| DISKCYL4096	{ dptr->diskcyl4096 = 1; }
		| SECTORS expression	{ dptr->sectors = $2; }
		| CYLINDERS expression	{ dptr->tracks = $2; }
		| TRACKS expression	{ dptr->tracks = $2; }
		| HEADS expression		{ dptr->heads = $2; }
		| OFFSET expression	{ dptr->header = $2; }
		| DEVICE string_expr optbootfile
		  {
                  if (priv_lvl)
                    yyerror("Can not use DISK/DEVICE in the user config file\n");
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a disk-image file or device given.");
		  dptr->dev_name = $2;
		  }
		| L_FILE string_expr
		  {
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a disk-image file or device given.");
		  dptr->dev_name = $2;
		  }
		| HDIMAGE string_expr
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
		| L_FLOPPY string_expr
		  {
                  if (priv_lvl)
                    yyerror("Can not use DISK/FLOPPY in the user config file\n");
		  if (dptr->dev_name != NULL)
		    yyerror("Two names for a floppy-device given.");
		  dptr->type = FLOPPY;
		  dptr->dev_name = $2;
		  }
		| L_PARTITION string_expr INTEGER optbootfile
		  {
                  yywarn("{ partition \"%s\" %d } the"
			 " token '%d' is ignored and can be removed.",
			 $2,$3,$3);
		  do_part($2);
		  }
		| L_PARTITION string_expr optbootfile
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
		| '(' expression ')'
	           {
	           allow_io($2, 1, ports_permission, ports_ormask,
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
		| RANGE expression ',' expression
		   {
		   c_printf("CONF: range of I/O ports 0x%04x-0x%04x\n",
			    (unsigned short)$2, (unsigned short)$4);
		   allow_io($2, $4 - $2 + 1, ports_permission, ports_ormask,
			    ports_andmask, portspeed, (char*)dev_name);
		   portspeed=0;
		   strcpy(dev_name,"");
		   }
		| RDONLY		{ ports_permission = IO_READ; }
		| WRONLY		{ ports_permission = IO_WRITE; }
		| RDWR			{ ports_permission = IO_RDWR; }
		| ORMASK expression	{ ports_ormask = $2; }
		| ANDMASK expression	{ ports_andmask = $2; }
                | FAST	                { portspeed = 1; }
                | DEVICE string_expr         { strcpy(dev_name,$2); free($2); } 
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
		| '(' expression ')' { set_irq_value(1, $2); }
		| USE_SIGIO expression { set_irq_value(0x10001, $2); }
		| RANGE INTEGER INTEGER { set_irq_range(1, $2, $3); }
		| RANGE expression ',' expression { set_irq_range(1, $2, $4); }
		| USE_SIGIO RANGE INTEGER INTEGER { set_irq_range(0x10001, $3, $4); }
		| USE_SIGIO RANGE expression ',' expression { set_irq_range(0x10001, $3, $5); }
		| STRING
		    { yyerror("unrecognized irqpassing command '%s'", $1);
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
		| '(' expression ')'
	           {
		     config.ems_size = $2;
		     if ($2 > 0) c_printf("CONF: %dk bytes EMS memory\n", $2);
	           }
		| EMS_SIZE expression
		   {
		     config.ems_size = $2;
		     if ($2 > 0) c_printf("CONF: %dk bytes EMS memory\n", $2);
		   }
		| EMS_FRAME expression
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
		| '(' expression ')'
	           {
                     if (!set_hardware_ram($2)) {
                       yyerror("wrong hardware ram address : 0x%05x", $2);
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
		| RANGE expression ',' expression
		   {
                     int i;
                     for (i=$2; i<= $4; i+=0x1000) {
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

bool:		expression
		;

floppy_bool:	expression
		;

mem_bool:	expression {
			if ($1 == 1) {
				yyerror("got 'on', expected 'off' or an integer");
			}
		}
		;

irq_bool:	expression {
			if ( $1 && (($1 < 2) || ($1 > 15)) ) {
				yyerror("got '%d', expected 'off' or an integer 2..15", $1);
			} 
		}
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

	/* vesa modes */
static void set_vesamodes(int width, int height, int color_bits)
{
  vesamode_type *vmt = malloc(sizeof *vmt);
  if(vmt != NULL) {
    vmt->width = width;
    vmt->height = height;
    vmt->color_bits = color_bits;
    vmt->next = config.vesamode_list;
    config.vesamode_list = vmt;
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

static int keyboard_statement_already = 0;

static void start_keyboard(void)
{
  keyb_layout(KEYB_USER); /* NOTE: the default has changed, --Hans, 971204 */
  config.console_keyb = 0;
  config.keybint = 0;
  keyboard_statement_already = 1;
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
      
  dptr->diskcyl4096 = 0;
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
  dptr->dexeflags = 0;
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

  c_printf("h: %d  s: %d   t: %d", dptr->heads, dptr->sectors,
	   dptr->tracks);

  if (token == BOOTDISK) {
    config.bootdisk = 1;
    use_bootdisk = 1;
    c_printf(" bootdisk\n");
  }
  else if (token == L_FLOPPY) {
    c_printf(" floppy %c:\n", 'A'+c_fdisks);
    c_fdisks++;
    config.fdisks = c_fdisks;
  }
  else {
    c_printf(" drive %c:\n", 'C'+c_hdisks);
    c_hdisks++;
    config.hdisks = c_hdisks;
  }
}

	/* keyboard */

void keyb_layout(int layout)
{
  struct keytable_entry *kt = keytable_list;
  if (layout == -1)
    layout = KEYB_US;
  while (kt->name) {
    if (kt->keyboard == layout) {
      c_printf("CONF: Keyboard-layout %s\n", kt->name);
      config.keytable = kt;
      return;
    }
    kt++;
  }
  c_printf("CONF: ERROR -- Keyboard has incorrect number!!!\n");
}

static void keytable_start(int layout)
{
  static struct keytable_entry *saved_kt = 0;
  if (layout == -1) {
    if (keyboard_statement_already) {
      if (config.keytable != saved_kt) {
        yywarn("keytable changed to %s table, but previously was defined %s\n",
                config.keytable->name, saved_kt->name);
      }
    }
  }
  else {
    saved_kt = config.keytable;
    keyb_layout(layout);
  }
}

static void keytable_stop(void)
{
  keytable_start(-1);
}

static void keyb_mod(int wich, int keynum)
{
  static unsigned char *table = 0;
  static int count = 0;
  static int in_altmap = 0;

  switch (wich) {
    case ' ': {
      in_altmap = 0;
      switch (keynum & 0x300) {
        case 0: table = config.keytable->key_map;
        	count=config.keytable->sizemap;
        	break;
        case 0x100: table = config.keytable->shift_map;
        	count=config.keytable->sizemap;
        	break;
        case 0x200: table = config.keytable->alt_map;
        	count=config.keytable->sizemap;
        	in_altmap = 1;
        	break;
        case 0x300: table = config.keytable->num_table;
        	count=config.keytable->sizepad;
        	break;
      }
      break;
    }
    case 'S': table = config.keytable->shift_map;
    	count=config.keytable->sizemap;
	in_altmap = 0;
    	break;
    case 'A': table = config.keytable->alt_map;
    	count=config.keytable->sizemap;
    	in_altmap = 1;
    	break;
    case 'N': table = config.keytable->num_table;
    	count=config.keytable->sizepad;
	in_altmap = 0;
    	break;
  }

  keynum &= 0xff;
  if (wich) {
    if (keynum >= count) return;
    table += keynum;
    count -= keynum;
    return;
  }
  if (count > 0) {
    *table++ = keynum;
    if (in_altmap && keynum)
      config.keytable->flags |= KT_USES_ALTMAP;
    count--;
  }
}


static void dump_keytable_part(FILE *f, unsigned char *map, int size)
{
  int i, in_string=0, is_deadkey;
  unsigned char c, *cc, comma=' ', buf[16];
  static unsigned char dead_key_list[] = {FULL_DEADKEY_LIST, 0};
  static char *dead_key_names[] = {
    "dgrave", "dacute", "dcircum", "dtilde", "dbreve", "daboved", "ddiares",
    "dabover", "ddacute", "dcedilla", "diota"
  };

  size--;
  for (i=0; i<=size; i++) {
    c = map[i];
    if (!(i & 15)) fprintf(f,"    ");
    is_deadkey = c ? (int)strchr(dead_key_list, c) : 0;
    if (!is_deadkey && isprint(c) && !strchr("\\\"\'`", c)) {
      if (in_string) fputc(c,f);
      else {
        fprintf(f, "%c\"%c", comma, c);
        in_string = 1;
      }
    }
    else {
      if (is_deadkey) cc = dead_key_names[is_deadkey - (int)(&dead_key_list)];
      else {
        sprintf(buf, "%d", c);
        cc = buf;
      }
      if (!in_string) fprintf(f, "%c%s", comma, cc);
      else {
        fprintf(f, "\",%s", cc);
        in_string = 0;
      }
    }
    if ((i & 15) == 15) {
      if (in_string) fputc('"', f);
      if (i < size) fputc(',', f);
      fputc('\n', f);
      in_string = 0;
      comma = ' ';
    }
    else comma = ',';
  }
  if (in_string) fputc('"', f);
  fputc('\n', f);
}

void dump_keytable(FILE *f, struct keytable_entry *kt)
{
    fprintf(f, "keytable %s {\n", kt->name);
    fprintf(f, "  0=\n");
    dump_keytable_part(f, kt->key_map, kt->sizemap);
    fprintf(f, "  shift 0=\n");
    dump_keytable_part(f, kt->shift_map, kt->sizemap);
    fprintf(f, "  alt 0=\n");
    dump_keytable_part(f, kt->alt_map, kt->sizemap);
    fprintf(f, "  numpad 0=\n");
    dump_keytable_part(f, kt->num_table, kt->sizepad-1);
    fprintf(f, "}\n\n\n");
}

static void dump_keytables_to_file(char *name)
{
  PRIV_SAVE_AREA
  FILE * f;
  struct keytable_entry *kt = keytable_list;

  enter_priv_off();
  f = fopen(name, "w");
  leave_priv_setting();
  if (!f) {
    error("cannot create keytable file %s\n", name);
    exit(1);
  }
  
  while (kt->name) {
    dump_keytable(f, kt);
    kt++;
  }
  fclose(f);
  exit(0);
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

void yywarn(char* string, ...)
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
  if (include_stack_ptr != 0 && !last_include) {
	  int i;
	  fprintf(stderr, "In file included from %s:%d\n",
		  include_fnames[0], include_lines[0]);
	  for(i = 1; i < include_stack_ptr; i++) {
		  fprintf(stderr, "                 from %s:%d\n",
			  include_fnames[i], include_lines[i]);
	  }
	  last_include = 1;
  }
  fprintf(stderr, "Error in %s: (line %.3d) ", 
	  include_fnames[include_stack_ptr], line_count);
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

  if (!filename) return 0;
  return fopen(filename, "r"); /* Open config-file */
}

/*
 * close_file - Close the configuration file and issue a message about
 *              errors/warnings that occured. If there were errors, the
 *              flag early-exit is that, so dosemu won't really.
 */

static void close_file(FILE * file)
{
  if (file) fclose(file);                  /* Close the config-file */

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

/* Parse Users for DOSEMU, by Alan Hourihane, alanh@fairlite.demon.co.uk */
/* Jan-17-1996: Erik Mouw (J.A.K.Mouw@et.tudelft.nl)
 *  - added logging facilities
 */
void
parse_dosemu_users(void)
{
#define ALL_USERS "all"
#define PBUFLEN 256

  PRIV_SAVE_AREA
  FILE *volatile fp;
  struct passwd *pwd;
  char buf[PBUFLEN];
  int userok = 0;
  char *ustr;
  int log_syslog=0;
  int uid;
  int have_vars=0;

  /* We come here _very_ early (at top of main()) to avoid security conflicts.
   * priv_init() has already been called, but nothing more.
   *
   * We will exit, if /etc/dosemu.users says that the user has no right
   * to run a suid root dosemu (and we are on), but will continue, if the user
   * is running a non-suid copy _and_ is mentioned in dosemu users.
   * The dosemu.users entry for such a user is:
   *
   *      joeodd  ... nosuidroot
   *
   * The functions we call rely on the following setting:
   */
  after_secure_check = 0;
  priv_lvl = 0;

  /* get our own hostname and define it as 'h_hostname'
   * and in $DOSEMU_HOST
   */
  {
    char buf [128];
    int l;
    /* preset DOSEMU_HOST env, such that the user may not fake an other */
    setenv("DOSEMU_HOST", "unknown", 1);
    if (!gethostname(buf+2, sizeof(buf)-1-2)) {
      buf[0]='h';
      buf[1]='_';
      l=strlen(buf+2);
      if (!getdomainname(buf+2+1+l, sizeof(buf)-1-2-l)) {
        if (buf[2+1+l] && strncmp(buf+2+1+l, "(none)", 6)) buf[2+l]='.';
        define_config_variable(buf);
        setenv("DOSEMU_HOST", buf+2, 1);
      }
    }
  }

  /* Get the log level*/
  if((fp = open_file(DOSEMU_LOGLEVEL_FILE)))
     {
       while (!feof(fp))
	 {
	   fgets(buf, PBUFLEN, fp);
	   if(buf[0]!='#')
	     {
	       if(strstr(buf, "syslog_error")!=NULL)
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
       write_to_syslog(buf);
       exit(1);
     }

  /* preset DOSEMU_*USER env, such that a user can't fake it */
  setenv("DOSEMU_USER", "unknown", 1);
  setenv("DOSEMU_REAL_USER", pwd->pw_name, 1);

  {
       enter_priv_on();
       fp = open_file(DOSEMU_USERS_FILE);
       leave_priv_setting();
       if (fp)
	 {
	   for(userok=0; fgets(buf, PBUFLEN, fp) != NULL && !userok; ) {
	     int l = strlen(buf);
	     if (l && (buf[l-1] == '\n')) buf[l-1] = 0;
	     ustr = strtok(buf, " \t\n,;:");
	     if (ustr && (ustr[0] != '#')) {
	       if (strcmp(ustr, pwd->pw_name)== 0) 
		 userok = 1;
	       else if (strcmp(ustr, ALL_USERS)== 0)
		 userok = 1;
	       if (userok) {
		 setenv("DOSEMU_USER", ustr, 1);
		 while ((ustr=strtok(0, " \t,;:")) !=0) {
		   if (ustr[0] == '#') break;
		   define_config_variable(ustr);
		   have_vars = 1;
                   if (!under_root_login && can_do_root_stuff && !strcmp(ustr,"nosuidroot")) {
                     fprintf(stderr,
                       "\nSorry, you are not allowed to run this suid root binary\n"
                       "but you may run a non-suid root copy of it\n\n");
                     exit(1);
                   }
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

  /* now we setup up our local DOSEMU home directory, where we
   * have (among other things) all temporary stuff in
   * (since 0.97.10.2)
   */
  LOCALDIR = get_dosemu_local_home();
  TMPDIR = mkdir_under(LOCALDIR, "tmp", 0);
  TMPDIR_PROCESS = mkdir_under(TMPDIR, "temp.", 1);
  RUNDIR = mkdir_under(LOCALDIR, "run", 0);
  TMPFILE = assemble_path(RUNDIR, "dosemu.", 0);
  
  /* check wether we are we are running non-suid root
   * If so eventually rearange some file locations.
   */
#if 0 /* we currently need not, but will need later for an enhanced non-suid */
  if (!can_do_root_stuff) {
	/* maybe ~/.dosemu/etc, ~/.dosemu/lib, e.t.c. */
  }
#endif

  if(userok==0) {
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
	 
       exit(1);
  }
  else {
       sprintf(buf, "DOSEMU started%s by %s (uid/euid=%i/%i)",
            (can_do_root_stuff && !under_root_login)? " suid root" : "",
            pwd->pw_name, uid, get_orig_euid());
            
       if(log_syslog>=2)
         write_to_syslog(buf);

  }
  after_secure_check = 1;
}


char *commandline_statements=0;

static int has_dexe_magic(char *name)
{
  PRIV_SAVE_AREA
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
  enum { maxn=0x255 };
  static char n[maxn+1];
  static char name[maxn+1];
  char *p, *path=getenv("PATH");

  strncpy(name,dexename,maxn);
  name[maxn] = 0;
  n[maxn] = 0;
  p = rindex(name, '.');
  if ( ext && ((p && strcmp(p, ext)) || !p) )
    strncat(name,ext,maxn);


  /* first try the pure file name */
  if (!path || (name[0] == '/') || (!strncmp(name, "./", 2))) {
    if (stat_dexe(name)) return name;
    return 0;
  }

  /* next try the standard path for DEXE files */
  snprintf(n, maxn, DEXE_LOAD_PATH "/%s", name);
  if (stat_dexe(n)) return n;

  /* now search in the users normal PATH */
  path = strdup(path);
  p= strtok(path,":");
  while (p) {
    snprintf(n, maxn, "%s/%s", p, name);
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
  PRIV_SAVE_AREA
  char *n, *cbuf;
  int fd, csize;
  struct image_header ihdr;

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

  /* now we extract the configuration file and the access flags */
  fd = open(n, O_RDONLY);
  if (read(fd, &ihdr, sizeof(struct image_header)) != sizeof(struct image_header)) {
    error("broken DEXE format, can't read image header\n");
    close(fd);
    exit(1);
  }

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
  dptr->dexeflags = ihdr.dexeflags | DISK_IS_DEXE;
  stop_disk(DISK);
  dexe_running = 1;
}


static void do_parse(FILE* fp, char *confname, char *errtx)
{
        yyin = fp;
        line_count = 1;
	include_stack_ptr = 0;
        c_printf("CONF: Parsing %s file.\n", confname);
	file_being_parsed = strdup(confname);
	include_fnames[include_stack_ptr] = file_being_parsed;
	yyrestart(fp);
        if (yyparse()) yyerror(errtx, confname);
        close_file(fp);
	include_stack_ptr = 0;
	include_fnames[include_stack_ptr] = 0;
	free(file_being_parsed);
}

int parse_config(char *confname, char *dosrcname)
{
  FILE *fd;
  int is_user_config;
#if YYDEBUG != 0
  extern int yydebug;

  yydebug  = 1;
#endif

  define_config_variable(PARSER_VERSION_STRING);

  if (get_config_variable("c_strict") && strcmp(confname, CONFIG_SCRIPT)) {
     c_printf("CONF: use of option -F %s forbidden by /etc/dosemu.users\n",confname);
     c_printf("CONF: using %s instead\n", CONFIG_SCRIPT);
     confname = CONFIG_SCRIPT;
  }

  /* Let's try confname if not null, and fail if not found */
  /* Else try the user's own .dosrc (old) or .dosemurc (new) */
  /* If that doesn't exist we will default to CONFIG_FILE */

  { 
    PRIV_SAVE_AREA
    uid_t uid = get_orig_uid();

    char *home = getenv("HOME");
    char *name;
    int skip_dosrc = 0;

    if (!dosrcname) {
      name = malloc(strlen(home) + 20);
      sprintf(name, "%s/.dosemurc", home);
      setenv("DOSEMU_RC",name,1);
      sprintf(name, "%s/.dosrc", home);
    }
    else {
      name = strdup(dosrcname);
      setenv("DOSEMU_RC",name,1);
      skip_dosrc = 1;
    }

    /* privileged options allowed? */
    is_user_config = strcmp(confname, CONFIG_SCRIPT);
    priv_lvl = uid != 0 && is_user_config;

    /* DEXE together with option F ? */
    if (priv_lvl && dexe_running) {
      /* for security reasons we cannot allow this */
      fprintf(stderr, "user cannot load DEXE file together with option -F\n");
      exit(1);
    }

    if (is_user_config) define_config_variable("c_user");
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
	do_parse(fd, confname, "error in configuration file %s");
      }
    }
    if (priv_lvl) undefine_config_variable("c_user");
    else undefine_config_variable("c_system");

    if (!get_config_variable(CONFNAME_V3USED)) {
	/* we obviously have an old configuration file
         * ( or a too simple one )
	 * giving up
	 */
	yyerror("\nYour %s script or %s configuration file is obviously\n"
		"an old style or a too simple one\n"
		"Please read README.txt on how to upgrade\n", confname, CONFIG_FILE);
	exit(1);
    }

    /* privileged options allowed for user's config? */
    priv_lvl = uid != 0;
    if (priv_lvl) define_config_variable("c_user");
    define_config_variable("c_dosrc");
    if (!skip_dosrc && !get_config_variable("skip_dosrc")
                    && ((fd = open_file(name)) != 0)) {
      do_parse(fd, name, "error in user's configuration file %s");
    }
    undefine_config_variable("c_dosrc");

    /* Now we parse any commandline statements from option '-I'
     * We do this under priv_lvl set above, so we have the same secure level
     * as with .dosrc
     */

    if (commandline_statements) {
      #define XX_NAME "commandline"
      extern char *yy_vbuffer;

      open_file(0);
      define_config_variable("c_comline");
      c_printf("Parsing " XX_NAME  " statements.\n");
      yyin=0;				 /* say: we have no input file */
      yy_vbuffer=commandline_statements; /* this is the input to scan */
      do_parse(0, XX_NAME, "error in user's %s statement");
      undefine_config_variable("c_comline");
    }
  }

  /* check for global settings, that should know the whole settings
   * This we only can do, after having parsed all statements
   */

  if (dexe_running) {
    if (dexe_secure && get_orig_uid())
      config.secure = 1;
    /* force a BootC,
     * regardless what ever was set in the config files
     */
    config.hdiskboot = 1;
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
static int saved_allowed_classes = -1;



static int is_in_allowed_classes(int mask)
{
  if (!(allowed_classes & mask)) {
    yyerror("insufficient class privilege to use this configuration option\n");
    leavedos(99);
  }
  return 1;
}

struct config_classes {
	char *class;
	int mask;
} config_classes[] = {
	{"c_all", CL_ALL},
	{"c_normal", CL_ALL & (~(CL_SHELL | CL_VAR | CL_BOOT | CL_VPORT | CL_SECURE | CL_IRQ | CL_HARDRAM))},
	{"c_fileext", CL_FILEEXT},
	{"c_var", CL_VAR},
	{"c_system", CL_ALL},
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
	{"c_shell", CL_SHELL},
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

static void enter_user_scope(int incstackptr)
{
  if (user_scope_level) return;
  saved_priv_lvl = priv_lvl;
  priv_lvl = 1;
  saved_allowed_classes = allowed_classes;
  allowed_classes = 0;
  user_scope_level = incstackptr;
  c_printf("CONF: entered user scope, includelevel %d\n", incstackptr-1);
}

static void leave_user_scope(int incstackptr)
{
  if (user_scope_level != incstackptr) return;
  priv_lvl = saved_priv_lvl;
  allowed_classes = saved_allowed_classes;
  user_scope_level = 0;
  c_printf("CONF: left user scope, includelevel %d\n", incstackptr-1);
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
    if (strcmp(name, CONFNAME_V3USED) && strncmp(name, "u_", 2)) {
      c_printf("CONF: not enough privilege to define config variable %s\n", name);
      return 0;
    }
  }
  if (!get_config_variable(name)) {
    if (config_variables_count < MAX_CONFIGVARIABLES) {
      config_variables[config_variables_count++] = strdup(name);
      if (!priv_lvl) update_class_mask();
    }
    else {
      if (after_secure_check)
        c_printf("CONF: overflow on config variable list\n");
      return 0;
    }
  }
  if (after_secure_check)
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
    if (!strcmp(name, CONFNAME_V3USED)) parser_version_3_style_used = 0;
    free(config_variables[config_variables_last]);
    for (i=config_variables_last; i<(config_variables_count-1); i++) {
      config_variables[i] = config_variables[i+1];
    }
    config_variables_count--;
    if (!priv_lvl) update_class_mask();
    c_printf("CONF: config variable %s unset\n", name);
    return 1;
  }
  return 0;
}

char *checked_getenv(const char *name)
{
  if (user_scope_level) {
     char *s, *name_ = malloc(strlen(name)+sizeof(USERVAR_PREF));
     strcpy(name_, USERVAR_PREF);
     strcat(name_, name);
     s = getenv(name_);
     free(name_);
     if (s) return s;
  }
  return getenv(name);
}

static void check_user_var(char *name)
{
	char *name_;
	char *s;

	if (user_scope_level) return;
	name_ = malloc(strlen(name)+sizeof(USERVAR_PREF));
	strcpy(name_, USERVAR_PREF);
	strcat(name_, name);
	s = getenv(name_);
	if (s) {
		if (getenv(name))
			c_printf("CONF: variable %s replaced by user\n", name);
		setenv(name, s, 1);
		unsetenv(name_);
	}
	free(name_);
}


static char *run_shell(char *command)
{
	int pipefds[2];
	pid_t pid;
	char excode[16] = "1";

	setenv("DOSEMU_SHELL_RETURN", excode, 1);
	if (pipe(pipefds)) return strdup("");
	pid = fork();
	if (pid == -1) return strdup("");
	if (!pid) {
		/* child */
		int ret;
		close(pipefds[0]);	/* we won't read from the pipe */
		dup2(pipefds[1], 1);	/* make the pipe child's stdout */
		priv_drop();	/* drop any priviledges */
		ret = system(command);
			/* tell the parent: "we have finished",
			 * this way we need not to play games with select()
			 */
		if (ret == -1) ret = errno;
		else ret >>=8;
		write(pipefds[1],"\0\0\0",4);
		close(pipefds[1]);
		_exit(ret);
	}
	else {
		/* parent */
		char *buf = 0;
		int recsize = 128;
		int bufsize = 0;
		int ptr =0;
		int ret;
		int status;

		close(pipefds[1]);	/* we won't write to the pipe */
		do {
			bufsize = ptr + recsize;
			if (!buf) buf = malloc(bufsize);
			else buf = realloc(buf, bufsize);
			ret = read(pipefds[0], buf+ptr, recsize -1);
			if (ret > 0) {
				ptr += ret;
			}
		} while (ret >0 && *((int *)(buf+ptr-4)) );
		close(pipefds[0]);
		waitpid(pid, &status, 0);
		buf[ptr] = 0;
		if (!buf[0]) {
			free(buf);
			buf = strdup("");
		}
		sprintf(excode, "%d", WEXITSTATUS(status));
		setenv("DOSEMU_SHELL_RETURN", excode, 1);
		return buf;
	}
}


struct for_each_entry {
	char *list;
	char *ptr;
};

#define FOR_EACH_DEPTH	16
static struct for_each_entry *for_each_list = 0;

static int for_each_handling(int loopid, char *varname, char *delim, char *list)
{
	struct for_each_entry *fe;
	char * new;
	char saved;
	if (!for_each_list) {
		int size = FOR_EACH_DEPTH * sizeof(struct for_each_entry);
		for_each_list = malloc(size);
		memset(for_each_list, 0, size);
	}
	if (loopid > FOR_EACH_DEPTH) {
		yyerror("too deeply nested foreach\n");
		return 0;
	}
	fe = for_each_list + loopid;
	if (!fe->list) {
		/* starting the loop */
		fe->ptr = fe->list = strdup(list);
	}
	while (fe->ptr[0] && strchr(delim,fe->ptr[0])) fe->ptr++;
	/* subsequent call */
	if (!fe->ptr[0]) {
		/* loop end */
		free(fe->list);
		fe->list = 0;
		return (0);
	}
	new = strpbrk(fe->ptr, delim);
	if (!new) new = strchr(fe->ptr,0);
	saved = *new;
	*new = 0;
	setenv(varname,fe->ptr,1);
	if (saved) new++;
	fe->ptr = new;
	return (1);
}

#ifdef TESTING_MAIN

static void die(char *reason)
{
  error("par dead: %s\n", reason);
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
