/* parse.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 * Parsing of config-files for the Linux-DOSEmulator
 *
 * The parser is a hand-written state machine.
 *
 * $Date: 1994/07/14 23:21:28 $
 * $Source: /home/src/dosemu0.60/init/RCS/parse.c,v $
 * $Revision: 2.6 $
 * $State: Exp $
 *
 * $Log: parse.c,v $
 * Revision 2.6  1994/07/14  23:21:28  root
 * Markkk's patches.
 *
 * Revision 2.5  1994/07/05  22:00:34  root
 * NCURSES IS HERE.
 *
 * Revision 2.3  1994/06/24  14:52:20  root
 * Messing with parse_config.
 *
 * Revision 2.2  1994/06/17  00:14:45  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:18:22  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:18:22  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.7  1994/06/03  00:59:58  root
 * pre51_23 prep, Daniel's fix for scrbuf malloc().
 *
 * Revision 1.6  1994/05/18  00:16:55  root
 * pre15_17
 *
 * Revision 1.5  1994/05/09  23:37:01  root
 * pre51_13.
 *
 * Revision 1.4  1994/05/04  22:18:36  root
 * Patches by Alan to mouse subsystem.
 *
 * Revision 1.3  1994/05/04  21:58:48  root
 * Prior to Alan's patches to mouse code.
 *
 * Revision 1.2  1994/04/29  23:53:26  root
 * Prior to Lutz's latest 94/04/29
 *
 * Revision 1.1  1994/04/25  21:06:39  root
 * Initial revision
 *
 * Revision 1.21  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.20  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.19  1994/04/13  00:07:09  root
 * Jochen Patches.
 *
 * Revision 1.18  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 2.9  1994/04/06  00:56:04  root
 * Made serial config more flexible, and up to 4 ports.
 *
 * Revision 1.1  1994/04/05  16:24:20  root
 * Initial revision
 *
 * Revision 1.16  1994/04/04  22:51:55  root
 * Patches for PS/2 mice.
 *
 * Revision 1.15  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.14  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
 * Revision 1.13  1994/03/18  23:17:51  root
 * Prep for 0.50pl1.
 *
 * Revision 1.12  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.11  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.10  1994/03/04  00:01:58  root
 * Readying for 0.50
 *
 * Revision 1.9  1994/02/21  20:28:19  root
 * Serial patches of Ronnie's
 *
 * Revision 1.8  1994/02/09  20:10:24  root
 * Added dosbanner config option for optionally displaying dosemu bannerinfo.
 * Added allowvideportaccess config option to deal with video ports.
 *
 * Revision 1.7  1994/01/31  21:05:04  root
 * Mouse now works with X for me.
 *
 * Revision 1.6  1994/01/31  18:44:24  root
 * Work on making mouse work
 *
 * Revision 1.5  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.4  1993/12/30  11:18:32  root
 * Theadore T'so's patches to allow booting from a diskimage, and then
 * returning the floopy to dosemu.
 * Also his patches to serial.c
 *
 * Revision 1.3  1993/12/27  19:06:29  root
 * Added Diamond option.
 *
 * Revision 1.2  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.3  1993/07/07  21:42:04  root
 * minor changes for -Wall
 *
 * Revision 1.2  1993/07/07  01:32:28  root
 * added support for ~/.dosrc and parse_config() now takes a filename
 * argument to override .dosrc check.
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.1  1993/05/04  05:29:22  root
 * Initial revision
 *
 */

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

extern int exchange_uids(void);

extern char* strdup(const char *); /* Not defined in string.h :-( */

extern struct config_info config;  /* Configuration information */

int errors;               /* Number of errors detected while parsing */
int warnings;             /* Number of warnings detected while parsing */

jmp_buf exitpar;

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

/***************************************************************/

typedef enum {
  NULLTOK = 0, LBRACE, RBRACE, TOP, SEC_COMMENT, SEC_FLOPPY,
  SEC_DISK, D_HEADS, D_SECTORS, D_CYLINDERS, D_FLOPPY, D_HARD,
  D_PARTITION, D_WHOLEDISK, D_OFFSET, D_FILE, D_FIVEINCH,
  D_READONLY, D_HDIMAGE, D_THREEINCH, VAL_FASTFLOPPY, VAL_CPU,
  VAL_XMS, VAL_EMS, VAL_DOSMEM, 
  EOL_COMMENT, P_COMMAND, P_OPTIONS, P_TIMEOUT,
  P_FILE, SEC_PORTS, PRT_RANGE, PRT_RDONLY, PRT_WRONLY,
  PRT_RDWR, PRT_ANDMASK, PRT_ORMASK, VAL_SPEAKER,
  VAL_PKTFLAGS, VAL_HOGTHRESHOLD, VAL_TIMER,
  VAL_DOSBANNER, VAL_TIMINT, VAL_EMUBAT, VAL_EMUSYS,
  SEC_BOOTDISK, VAL_DPMI, VAL_ALLOWVIDEOPORTACCESS,

  SEC_PRINTER, VAL_MATHCO, VAL_IPXSUP, LCCOM, PRED_BOOTA, PRED_BOOTC,
  
  SEC_DEBUG, DBG_IO, DBG_VIDEO, DBG_SERIAL, DBG_CONFIG, DBG_DISK,
             DBG_READ, DBG_WRITE, DBG_KEYB, DBG_PRINTER, DBG_WARNING,
             DBG_GENERAL, DBG_XMS, DBG_DPMI, DBG_MOUSE, DBG_HARDWARE,
             DBG_IPC, DBG_EMS, DBG_NET,
  
  SEC_KEYBOARD, KEYB_RAWKEYBOARD, KEYB_LAYOUT, KEYB_KEYBINT,

  SEC_TERMINAL, T_METHOD, T_UPDATELINES, T_UPDATEFREQ, T_CHARSET, T_COLOR,
  
  SEC_VIDEO, V_CHIPSET, V_MDA, V_VGA, V_CGA, V_EGA, V_VBIOSF,
             V_VBIOSC, V_CONSOLE, V_FULLREST, V_PARTIALREST, V_GRAPHICS,
             V_MEMSIZE, V_VBIOSM, V_VBIOS_SEG,
  
  SEC_SERIAL, S_DEVICE, S_PORT, S_BASE, S_IRQ, S_MODEM, S_MOUSE,

  SEC_MOUSE, M_INTDRV, M_BAUDRATE, M_DEVICE, M_MICROSOFT, M_PS2, 
             M_MOUSESYSTEMS, M_MMSERIES, M_LOGITECH, M_BUSMOUSE, 
             M_MOUSEMAN, M_HITACHI, M_CLEARDTR
} tok_t;

typedef enum {
  NULLFORM = 0, ODELIM, CDELIM, COMMENT, SECTION, PRED,
  VALUE, VALUE2, VALUEN
} form_t;

typedef int arg_t;

typedef struct word_struct {
  char *name;
  tok_t token;
  form_t form;
  int (*func) (struct word_struct *, arg_t p1, arg_t p2);
  void (*start) (struct word_struct *, arg_t p1, arg_t p2);
  void (*stop) (struct word_struct *, arg_t p1, arg_t p2);
  struct word_struct *sub_words;
  arg_t p1, p2;
} word_t;

typedef struct syn_struct {
  char *syn;
  char *trans;
} syn_t;

/***************************************************************/

int do_top(word_t *, arg_t, arg_t);
int do_num(word_t *, arg_t, arg_t);
void parse_file(word_t *, FILE *);
char *get_name(FILE *);

int do_comment(word_t *, arg_t, arg_t);

int do_disk(word_t *, arg_t, arg_t);
void start_disk(word_t *, arg_t, arg_t);
void stop_disk(word_t *, arg_t, arg_t);

int do_printer(word_t *, arg_t, arg_t);
void start_printer(word_t *, arg_t, arg_t);
void stop_printer(word_t *, arg_t, arg_t);

int do_video(word_t *, arg_t, arg_t);
void stop_video(word_t *, arg_t, arg_t);

int do_terminal(word_t *, arg_t, arg_t);
void stop_terminal(word_t *, arg_t, arg_t);

int do_serial(word_t *, arg_t, arg_t);
void start_serial(word_t *, arg_t, arg_t);
void stop_serial(word_t *, arg_t, arg_t);

int do_mouse(word_t *, arg_t, arg_t);
void start_mouse(word_t *, arg_t, arg_t);
void stop_mouse(word_t *, arg_t, arg_t);

int do_debug(word_t *, arg_t, arg_t);
void start_debug(word_t *, arg_t, arg_t);
void stop_debug(word_t *, arg_t, arg_t);

int do_keyboard(word_t *, arg_t, arg_t);
void start_keyboard(word_t *, arg_t, arg_t);
void stop_keyboard(word_t *, arg_t, arg_t);

int do_ports(word_t *, arg_t, arg_t);

int glob_bad(word_t *, arg_t, arg_t);
int porttok(word_t *, arg_t, arg_t);

extern int allow_io(int, int, int, int, int);

void parse_error(char *s);
void parse_warning(char *s);
int read_until_rtn_previous(FILE * fd, int goal);
int same_name(char *str1, char *str2);
char *synonym(syn_t * syntable, char *str);
char *get_arg(FILE * fd);
word_t *match_name(word_t * words, char *name);
FILE *open_file(char *filename);
void close_file(FILE * file);

#define do_delim 0

#define NULL_WORD	{NULL,NULLTOK,NULLFORM,glob_bad,0,0,}
#define NULL_SYN	{NULL,NULL}

word_t null_words[] =
{
  NULL_WORD,
  NULL_WORD
};

word_t video_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_video},
  {"}", RBRACE, CDELIM, do_video},
  {"chipset", V_CHIPSET, VALUE, do_video},
  {"console", V_CONSOLE, PRED, do_video},
  {"fullrestore", V_FULLREST, PRED, do_video},
  {"partialrestore", V_PARTIALREST, PRED, do_video},
  {"graphics", V_GRAPHICS, PRED, do_video},
  {"memsize", V_MEMSIZE, VALUE, do_video},
  {"vga", V_VGA, PRED, do_video},
  {"ega", V_EGA, PRED, do_video},
  {"cga", V_CGA, PRED, do_video},
  {"mda", V_MDA, PRED, do_video},
  {"vbios_file", V_VBIOSF, VALUE, do_video},
  {"vbios_copy", V_VBIOSC, PRED, do_video},
  {"vbios_mmap", V_VBIOSM, PRED, do_video},
  {"vbios_seg", V_VBIOS_SEG, VALUE, do_video},
  NULL_WORD
};

word_t terminal_words[] = 
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_terminal},
  {"}", RBRACE, CDELIM, do_terminal},
  {"method", T_METHOD, VALUE, do_terminal},
  {"updatelines", T_UPDATELINES, VALUE, do_terminal},
  {"updatefreq", T_UPDATEFREQ, VALUE, do_terminal},
  {"charset", T_CHARSET, VALUE, do_terminal},
  {"color", T_COLOR, VALUE, do_terminal},
  NULL_WORD
};

word_t serial_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_serial},
  {"}", RBRACE, CDELIM, do_serial},
  {"mouse", S_MOUSE, PRED, do_serial},	 		/* Mouse flag */
  {"device", S_DEVICE, VALUE, do_serial}, 		/* Device file */
  {"com", S_PORT, VALUE, do_serial}, 			/* COM port number */
  {"base", S_BASE, VALUE, do_serial},			/* Base port address */
  {"irq", S_IRQ, VALUE, do_serial}, 			/* IRQ line */
  {"interrupt", S_IRQ, VALUE, do_serial}, 		/* XXXXX obsolete */
  NULL_WORD
};

word_t mouse_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_mouse},
  {"}", RBRACE, CDELIM, do_mouse},
  {"internaldriver", M_INTDRV, PRED, do_mouse},
  {"baudrate", M_BAUDRATE, VALUE, do_mouse},
  {"device", M_DEVICE, VALUE, do_mouse},
  {"microsoft", M_MICROSOFT, PRED, do_mouse},
  {"ps2", M_PS2, PRED, do_mouse},
  {"mousesystems", M_MOUSESYSTEMS, PRED, do_mouse},
  {"mmseries", M_MMSERIES, PRED, do_mouse},
  {"logitech", M_LOGITECH, PRED, do_mouse},
  {"busmouse", M_BUSMOUSE, PRED, do_mouse},
  {"mouseman", M_MOUSEMAN, PRED, do_mouse},
  {"hitachi", M_HITACHI, PRED, do_mouse},
  {"cleardtr", M_CLEARDTR, PRED, do_mouse},
  NULL_WORD
};

word_t debug_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_debug},
  {"}", RBRACE, CDELIM, do_debug},
  {"io", DBG_IO, VALUE, do_debug},
  {"port", DBG_IO, VALUE, do_debug},
  {"video", DBG_VIDEO, VALUE, do_debug},
  {"serial", DBG_SERIAL, VALUE, do_debug},
  {"config", DBG_CONFIG, VALUE, do_debug},
  {"disk", DBG_DISK, VALUE, do_debug},
  {"read", DBG_READ, VALUE, do_debug},
  {"write", DBG_WRITE, VALUE, do_debug},
  {"keyboard", DBG_KEYB, VALUE, do_debug},
  {"keyb", DBG_KEYB, VALUE, do_debug},
  {"printer", DBG_PRINTER, VALUE, do_debug},
  {"warning", DBG_WARNING, VALUE, do_debug},
  {"general", DBG_GENERAL, VALUE, do_debug},
  {"xms", DBG_XMS, VALUE, do_debug},
  {"dpmi", DBG_DPMI, VALUE, do_debug},
  {"mouse", DBG_MOUSE, VALUE, do_debug},
  {"hardware", DBG_HARDWARE, VALUE, do_debug},
  {"ipc", DBG_IPC, VALUE, do_debug},
  {"ems", DBG_EMS, VALUE, do_debug},
  {"network", DBG_NET, VALUE, do_debug},
  NULL_WORD
};

word_t keyboard_words[] =
{
  {NULL, NULLTOK, NULLFORM, porttok},
  {"{", LBRACE, ODELIM, do_ports,},
  {"}", RBRACE, CDELIM, do_ports},
  {"rawkeyboard", KEYB_RAWKEYBOARD, VALUE, do_keyboard},
  {"layout", KEYB_LAYOUT, VALUE, do_keyboard},
  {"keybint", KEYB_KEYBINT, VALUE, do_keyboard},
  NULL_WORD
};

word_t comment_words[] =
{
  NULL_WORD,
  {"*/", RBRACE, CDELIM, do_comment},
  NULL_WORD
};

word_t port_words[] =
{
  {NULL, NULLTOK, NULLFORM, porttok},
  {"{", LBRACE, ODELIM, do_ports,},
  {"}", RBRACE, CDELIM, do_ports},
  {"range", PRT_RANGE, VALUE2, do_ports},
  {"readonly", PRT_RDONLY, PRED, do_ports},
  {"writeonly", PRT_WRONLY, PRED, do_ports},
  {"readwrite", PRT_RDWR, PRED, do_ports},
  {"ormask", PRT_ORMASK, VALUE, do_ports},
  {"andmask", PRT_ANDMASK, VALUE, do_ports},
  NULL_WORD
};

word_t disk_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_disk,},
  {"}", RBRACE, CDELIM, do_disk},
  {"device", D_FILE, VALUE, do_disk},
  {"file", D_FILE, VALUE, do_disk},
  {"offset", D_OFFSET, VALUE, do_disk},
  {"header", D_OFFSET, VALUE, do_disk},
  {"heads", D_HEADS, VALUE, do_disk},
  {"sectors", D_SECTORS, VALUE, do_disk},
  {"cylinders", D_CYLINDERS, VALUE, do_disk},
  {"tracks", D_CYLINDERS, VALUE, do_disk},
  {"floppy", D_FLOPPY, PRED, do_disk},
  {"hard", D_HARD, VALUE, do_disk},
  {"hdimage", D_HDIMAGE, VALUE, do_disk},
  {"image", D_HDIMAGE, VALUE, do_disk},
  {"partition", D_PARTITION, VALUE2, do_disk},
  {"wholedisk", D_WHOLEDISK, VALUE, do_disk},
  {"readonly", D_READONLY, PRED, do_disk},
  {"fiveinch", D_FIVEINCH, PRED, do_disk},
  {"threeinch", D_THREEINCH, PRED, do_disk},
  NULL_WORD
};

word_t printer_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_printer},
  {"}", RBRACE, CDELIM, do_printer},
  {"timeout", P_TIMEOUT, VALUE, do_printer},
  {"command", P_COMMAND, VALUE, do_printer},
  {"options", P_OPTIONS, VALUE, do_printer},
  {"file", P_FILE, VALUE, do_printer},
  NULL_WORD
};

word_t top_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_delim, 0, 0, null_words},
  {"}", RBRACE, CDELIM, do_delim, 0, 0, null_words},
  {"/*", SEC_COMMENT, SECTION, do_comment, 0, 0, comment_words},
  {"#", EOL_COMMENT, COMMENT, do_comment, 0, 0, comment_words},
  {";", EOL_COMMENT, COMMENT, do_comment, 0, 0, comment_words},
  {"disk", SEC_DISK, SECTION, do_top, start_disk, stop_disk, disk_words},
  {"floppy", SEC_FLOPPY, SECTION, do_top, start_disk, stop_disk, disk_words},
  {"bootdisk", SEC_BOOTDISK, SECTION, do_top, start_disk, stop_disk, disk_words},
  {"video", SEC_VIDEO, SECTION, do_top, 0, stop_video, video_words},
  {"terminal", SEC_TERMINAL, SECTION, do_top, 0, stop_terminal, terminal_words},
  {"serial", SEC_SERIAL, SECTION, do_top, start_serial, stop_serial, serial_words},
  {"mouse", SEC_MOUSE, SECTION, do_top, start_mouse, stop_mouse, mouse_words},
  {"xms", VAL_XMS, VALUE, do_num, 0, 0, null_words},
  {"ems", VAL_EMS, VALUE, do_num, 0, 0, null_words},
  {"dpmi", VAL_DPMI, VALUE, do_num, 0, 0, null_words},
  {"dosmem", VAL_DOSMEM, VALUE, do_num, 0, 0, null_words},
  {"allowvideoportaccess", VAL_ALLOWVIDEOPORTACCESS, VALUE, do_num, 0, 0, null_words},
  {"printer", SEC_PRINTER, SECTION, do_top, start_printer, stop_printer, printer_words},
  {"mathco", VAL_MATHCO, VALUE, do_num, 0, 0, null_words},
  {"ipxsupport", VAL_IPXSUP, VALUE, do_num, 0, 0, null_words},
  {"pktdriver", VAL_PKTFLAGS, VALUE, do_num, 0, 0, null_words},
  {"speaker", VAL_SPEAKER, VALUE, do_num, 0, 0, null_words},
  {"boota", PRED_BOOTA, PRED, do_num, 0, 0, null_words},
  {"bootc", PRED_BOOTC, PRED, do_num, 0, 0, null_words},
  {"cpu", VAL_CPU, VALUE, do_num, 0, 0, null_words},
  {"ports", SEC_PORTS, SECTION, do_top, 0, 0, port_words},
  {"debug", SEC_DEBUG, SECTION, do_top, start_debug, stop_debug, debug_words},
  {"messages", SEC_DEBUG, SECTION, do_num, start_debug, stop_debug, debug_words},
  {"keyboard", SEC_KEYBOARD, SECTION, do_top, start_keyboard, stop_keyboard, keyboard_words},
  {"hogthreshold", VAL_HOGTHRESHOLD, VALUE, do_num, 0, 0, null_words},
  {"timer", VAL_TIMER, VALUE, do_num, 0, 0, null_words},
  {"dosbanner", VAL_DOSBANNER, VALUE, do_num, 0, 0, null_words},
  {"timint", VAL_TIMINT, VALUE, do_num, 0, 0, null_words},
  {"fastfloppy", VAL_FASTFLOPPY, VALUE, do_num, 0, 0, null_words},
  {"emusys", VAL_EMUSYS, VALUE, do_num, 0, 0, null_words},
  {"emubat", VAL_EMUBAT, VALUE, do_num, 0, 0, null_words},
  NULL_WORD
};

/* really should break this up into groups for each word, but there are
 * no conflicts yet
 */

/* XXX - fix these synonyms! */
syn_t global_syns[] =
{
  {"on",  "1"},
  {"off", "0"},
  {"yes", "1"},
  {"no",  "0"},

  {"8086",  "0"},              /* values from <linux/vm86.h> */
  {"80186", "1"},
  {"80286", "2"},
  {"80386", "3"},
  {"80486", "4"},
  {"80586", "5"},

  {"native",   "1"},		/* for speaker */
  {"emulated", "2"},

  {"plainvga", "0"},
  {"trident",  "1"},
  {"et4000",   "2"},
  {"diamond",  "3"},
  {"s3",       "4"},

  {"novell_hack", "1"},               /* Novell 8137->802.3 hack */

  {"finnish",       "0" },       /* Ugly, must match the defines in emu.h */
  {"finnish-latin1","1" },
  {"us",            "2" },
  {"uk",            "3" },
  {"gr",            "4" },
  {"gr-latin1",     "5" },
  {"fr",            "6" },
  {"fr-latin1",     "7" },
  {"dk",            "8" },
  {"dk-latin1",     "9" },
  {"dvorak",        "10" },
  {"sg",            "11" },
  {"sg-latin1",     "12" },
  {"no",            "13" },
  {"sf",            "15" },
  {"sf-latin1",     "16" },
  {"es",            "17" },
  {"es-latin1",     "18" },
  {"be",            "19" },
  
  {"latin",         "1" },   /* Ugly, must match CHARSET_* defines in video.h */
  {"ibm",           "2" },   
  {"fullibm",       "3" },
  
  {"fast",          "1" },   /* Ugly, must match METHOD_* defines in video.h */
  {"ncurses",       "2" },

  NULL_SYN
};

#define iscomment(c) (c=='#' || c=='!' || c==';')
#define istoken(c) (!isspace(c) && !iscomment(c))
#define read_until_eol(fd) read_until_rtn_previous(fd, '\n')

void die(char *reason) NORETURN;

/****************************** globals ************************/
FILE *globl_file = NULL;
tok_t state = TOP;
char *statestr = "top";

struct disk *dptr = NULL;
int hdiskno = 0;
int diskno = 0;

/***************************** functions ***********************/

/*
 * parse_error - print an error message and count the number of error
 *               that occured while parsing the config-file
 */

void parse_error(char *s)
{
  fprintf(stderr, "ERROR: %s\n",s);
  errors = errors + 1;
}

/*
 * parse_warinig - print a warning message and count the number of warnings
 *                 that occured while parsing the config-file
 */

void parse_warning(char *s)
{
  fprintf(stderr, "WARNING: %s\n",s);
  warnings++;
}

int
read_until_rtn_previous(FILE * fd, int goal)
{
  static int previous = 0;
  int here;

  here = fgetc(fd);

  while (!feof(fd) && here != goal) {
    previous = here;
    here = fgetc(fd);
  }

  if (feof(fd))
    return EOF;
  else
    return previous;
}

/*
 * glob_bad - unknown token read from config-file
 */

int glob_bad(word_t * word, arg_t a1, arg_t a2)
{
  char *str;

  str = malloc(128);

  sprintf(str,"glob_bad called: %s!", word->name);
  parse_error(str);

  free(str);

  return word->token;
}

int ports_permission = IO_RDWR;
unsigned int ports_ormask = 0;
unsigned int ports_andmask = 0xFFFF;

int
porttok(word_t * word, arg_t a1, arg_t a2)
{
  long num;
  char *end;

  num = strtol(word->name, &end, 0);
  allow_io(num, 1, ports_permission, ports_ormask, ports_andmask);
  return 0;
}

int
do_ports(word_t * word, arg_t a1, arg_t a2)
{
  char *arg = NULL, *arg2 = NULL;

  /*  c_printf("do_ports: %s %d\n", word->name, word->token); */

  switch (word->form) {
  case ODELIM:
  case CDELIM:
    ports_permission = IO_RDWR;
    ports_ormask = 0;
    ports_andmask = 0xFFFF;
    break;
  case VALUE2:
    arg = get_name(globl_file);
    arg2 = get_name(globl_file);
    break;
  case VALUE:
    arg = get_name(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
    char *end;

  case PRT_RANGE:{
      long num1, num2;

      num1 = strtol(arg, &end, 0);
      num2 = strtol(arg2, &end, 0);
      c_printf("CONF: range of I/O ports 0x%04x-0x%04x\n",
	       (unsigned short) num1, (unsigned short) num2);
      allow_io(num1, num2 - num1 + 1, ports_permission, ports_ormask, ports_andmask);
      break;
    }
  case PRT_RDONLY:
    ports_permission = IO_READ;
    break;
  case PRT_WRONLY:
    ports_permission = IO_WRITE;
    break;
  case PRT_RDWR:
    ports_permission = IO_RDWR;
    break;
  case PRT_ORMASK:
    ports_ormask = strtol(arg, &end, 0);
    break;
  case PRT_ANDMASK:
    ports_andmask = strtol(arg, &end, 0);
    break;
  default:
    break;
  }
  if (arg)
    free(arg);
  if (arg2)
    free(arg2);
  return word->token;
}

void die(char *reason)
{
  error("ERROR: par dead: %s\n", reason);
  leavedos(0);
}

int
do_comment(word_t * word, arg_t a1, arg_t a2)
{
  if (word->token == EOL_COMMENT)
    read_until_eol(globl_file);
  else {
    int prev;

    do {
      if ((prev = read_until_rtn_previous(globl_file, '/')) == EOF)
	die("unterminated /* comment!\n");
    } while (prev != '*');
  }

  return word->token;
}

char *
get_name(FILE * fd)
{
  char tmps[100], *p = tmps;
  int tmpc;

  *p = 0;

  /* prime the pump */
  tmpc = fgetc(fd);

  while (!istoken(tmpc) && !feof(fd)) {
    if (iscomment(tmpc))
      read_until_eol(globl_file);
    tmpc = fgetc(fd);
  }

#define QUOTE '"'

  /* there is no escaping of quote marks yet...is there a need? */

  if (tmpc == QUOTE) {
    tmpc = fgetc(fd);
    while (tmpc != QUOTE && !feof(fd)) {
      *p++ = tmpc;
      tmpc = fgetc(fd);
    }
  }
  else {
    while (istoken(tmpc) && !feof(fd)) {
      *p++ = tmpc;
      tmpc = fgetc(fd);
    };
    if (iscomment(tmpc))
      read_until_eol(globl_file);
  }

  *p = 0;
  if (strlen(tmps) == 0)
    return 0;

  p = malloc(strlen(tmps) + 1);
  strcpy(p, tmps);
  return p;
}

/* case-insensitive match of two strings */
int
same_name(char *str1, char *str2)
{
  for (; *str1 && *str2 && tolower(*str1) == tolower(*str2);
       str1++, str2++)
    /* empty loop */
    ;

  if (!*str1 && !*str2)
    return 1;
  else
    return 0;
}

char *
synonym(syn_t * syntable, char *str)
{
  int i;

  for (i = 0; syntable[i].trans; i++)
    if (same_name(syntable[i].syn, str))
      break;

  return syntable[i].trans;
}

char *
get_arg(FILE * fd)
{
  char *str, *str2;

  str = get_name(fd);
  if ((str2 = synonym(global_syns, str))) {
    free(str);
    str = strdup(str2);
  }

  return str;
}

/*
 * open_file - opens the configuration-file named *filename and returns
 *             a file-pointer. The error/warning-counters are reset to zero.
 */

FILE *open_file(char *filename)
{
  errors   = 0;                  /* Reset error counter */
  warnings = 0;                  /* Reset counter for warnings */

  return (globl_file = fopen(filename, "r")); /* Open config-file */
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

word_t *
match_name(word_t * words, char *name)
{
  int i;

  for (i = 1; words[i].name; i++)
    if (same_name(words[i].name, name))
      return &words[i];

  return NULL;
}

int
do_num(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;

  switch (word->form) {
  case PRED:
    switch (word->token) {
    case PRED_BOOTA:
      config.hdiskboot = 0;
      break;
    case PRED_BOOTC:
      config.hdiskboot = 1;
      break;
    default:
      c_printf("PAR: set pred %s\n", word->name);
      break;
    }
    break;

  case VALUE:
    arg = get_arg(globl_file);

    switch (word->token) {
    case VAL_EMUBAT:
      config.emubat = strdup(arg);
      c_printf("PAR: config.emubat %s\n", config.emubat);
      break;
    case VAL_EMUSYS:
      config.emusys = strdup(arg);
      c_printf("PAR: config.emusys %s\n", config.emusys);
      break;
    case VAL_FASTFLOPPY:
      config.fastfloppy = atoi(arg);
      break;
    case VAL_CPU:
      vm86s.cpu_type = atoi(arg);
      break;
    case VAL_HOGTHRESHOLD:
      config.hogthreshold = atoi(arg);
      break;
    case VAL_TIMINT:
      config.timers = atoi(arg);
      c_printf("CONF: timers %s!\n", config.timers ? "on" : "off");
      break;
    case VAL_DOSBANNER:
      config.dosbanner = atoi(arg);
      c_printf("CONF: dosbanner %s!\n", config.dosbanner ? "on" : "off");
      break;
    case VAL_ALLOWVIDEOPORTACCESS:
      config.allowvideoportaccess = atoi(arg);
      c_printf("CONF: allowvideoportaccess %s!\n", config.allowvideoportaccess ? "on" : "off");
      break;
    case VAL_TIMER:
      config.freq = atoi(arg);
      config.update = 1000000 / config.freq;
      break;
    case VAL_EMS:
      config.ems_size = atoi(arg);
      break;
    case VAL_XMS:
      config.xms_size = atoi(arg);
      break;
    case VAL_DPMI:
      config.dpmi_size = atoi(arg);
      break;
    case VAL_DOSMEM:
      config.mem_size = atoi(arg);
      break;
    case VAL_MATHCO:
      config.mathco = (atoi(arg) != 0);
      break;
    case VAL_IPXSUP:
      config.ipxsup = (atoi(arg) != 0);
      c_printf("CONF: ipx support is %s\n",
        config.ipxsup?"ON":"OFF");
      break;
    case VAL_PKTFLAGS:
      config.pktflags = atoi(arg);
      break;
    case VAL_SPEAKER:
      config.speaker = atoi(arg);
      if (config.speaker == SPKR_NATIVE) {
	warn("CONF: allowing access to the speaker ports!\n");
	allow_io(0x42, 1, IO_RDWR, 0, 0xFFFF);
	allow_io(0x61, 1, IO_RDWR, 0, 0xFFFF);
      }
      else
	c_printf("CONF: not allowing speaker port access\n");
      break;
    default:
      c_printf("val %s set to %s\n", word->name, arg);
      break;
    }

    if (arg)
      free(arg);
    break;
  default:
    c_printf("non-value in do_num!\n");
  }
 return 0;
}

void
stop_video(word_t * word, arg_t farg1, arg_t farg2)
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

int
do_video(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;
  char *end;
  /* c_printf("do_video: %s %d\n", word->name, word->token); */

  switch (word->form) {
  case VALUE:
    arg = get_arg(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case V_VGA:
    config.cardtype = CARD_VGA;
    break;
  case V_EGA:
  case V_CGA:
    config.cardtype = CARD_CGA;
    break;
  case V_MDA:
    config.cardtype = CARD_MDA;
    break;
  case V_CHIPSET:
    config.chipset = atoi(arg);
    c_printf("CHIPSET: %s %d\n", arg, config.chipset);
    break;
  case V_MEMSIZE:
    config.gfxmemsize = atoi(arg);
    break;
  case V_GRAPHICS:
    config.vga = 1;
    break;
  case V_CONSOLE:
    config.console_video = 1;
    break;
  case V_FULLREST:
    config.fullrestore = 1;
    break;
  case V_PARTIALREST:
    config.fullrestore = 0;
    break;
  case V_VBIOSF:
    config.vbios_file = strdup(arg);
    config.mapped_bios = 1;
    config.vbios_copy = 0;
    break;
  case V_VBIOSM:
    config.vbios_file = NULL;
    config.mapped_bios = 1;
    config.vbios_copy = 0;
    break;
  case V_VBIOSC:
    config.vbios_file = NULL;
    config.vbios_copy = 1;
    config.mapped_bios = 1;
    break;
  case V_VBIOS_SEG:
    config.vbios_seg = strtol(arg,&end,0);
    c_printf("CONF: VGA-BIOS-Segment %x\n", config.vbios_seg);
    if ((config.vbios_seg != 0xe000) && (config.vbios_seg != 0xc000) )
      {
	config.vbios_seg = 0xc000;
	c_printf("CONF: VGA-BIOS-Segment set to %x\n", config.vbios_seg);
      }
    break;
  default:
    error("CONF: unknown video token: %s\n", word->name);
    break;
  }

  if (arg)
    free(arg);
  return (word->token);
}

void
stop_terminal(word_t * word, arg_t farg1, arg_t farg2)
{
  if (config.term_updatefreq > 100) {
    warn("CONF: terminal updatefreq too large (too slow)!");
    config.term_updatefreq = 100;
  } 
}

int
do_terminal(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;
  /* c_printf("do_terminal: %s %d\n", word->name, word->token); */

  switch (word->form) {
  case VALUE:
    arg = get_arg(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case T_METHOD:
    config.term_method = atoi(arg);
    break;
  case T_UPDATELINES:
    config.term_updatelines = atoi(arg);
    break;
  case T_UPDATEFREQ:
    config.term_updatefreq = atoi(arg);
    break;
  case T_CHARSET:
    config.term_charset = atoi(arg);
    break;
  case T_COLOR:
    config.term_color = atoi(arg);
    break;
  default:
    error("CONF: unknown terminal token: %s\n", word->name);
    break;
  }

  if (arg)
    free(arg);
  return (word->token);
}

void
start_mouse(word_t * word, arg_t farg1, arg_t farg2)
{
  if (c_mouse >= MAX_MOUSE)
    mptr = &nullmouse;
  else {
    mptr = &mice[c_mouse];
    mptr->fd = -1;
  }
}

/*
 * Initialize the debug-configuration variables
 */

void
start_debug(word_t * word, arg_t farg1, arg_t farg2)
{
  int flag = 0;                 /* Default is no debugging output at all */

  c_printf("CONF: start_debug\n");
  d.video = flag;               /* For all options */
  d.serial = flag;
  d.config = flag;
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

/*
 * Initialize the keyboard-configuration variables
 */

void
start_keyboard(word_t * word, arg_t farg1, arg_t farg2)
{
  /* Nothing special to do */
  c_printf("CONF: start_keyboard\n");
}

void
start_serial(word_t * word, arg_t farg1, arg_t farg2)
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

void
stop_mouse(word_t * word, arg_t farg1, arg_t farg2)
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

/*
 * End of debugging configuration 
 */

void
stop_debug(word_t * word, arg_t farg1, arg_t farg2)
{
  c_printf("CONF: stop_debug\n");
  /* Nothing to do for now */
}

/*
 * End of keyboard configuration 
 */

void
stop_keyboard(word_t * word, arg_t farg1, arg_t farg2)
{
  c_printf("CONF: stop_keyboard\n");
  /* Nothing to do for now */
}

void
stop_serial(word_t * word, arg_t farg1, arg_t farg2)
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

int
do_mouse(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;

  switch (word->form) {
  case VALUE:
    arg = get_name(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case M_INTDRV:
    mptr->intdrv = TRUE;
    break;
  case M_BAUDRATE:
    mptr->baudRate = atoi(arg);
    break; 
  case M_DEVICE:
    strcpy(mptr->dev, arg);   
    break;
  case M_MICROSOFT:
    mptr->type = MOUSE_MICROSOFT;
    mptr->flags = CS7 | CREAD | CLOCAL | HUPCL;
    break;
  case M_MOUSESYSTEMS:
    mptr->type = MOUSE_MOUSESYSTEMS;
    mptr->flags = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL;
    break;
  case M_MMSERIES:
    mptr->type = MOUSE_MMSERIES;
    mptr->flags = CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL;
    break;
  case M_LOGITECH:
    mptr->type = MOUSE_LOGITECH;
    mptr->flags = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL;
    break;
  case M_PS2:
    mptr->type = MOUSE_PS2;
    mptr->flags = 0;
    break;
  case M_MOUSEMAN:
    mptr->type = MOUSE_MOUSEMAN;
    mptr->flags = CS7 | CREAD | CLOCAL | HUPCL;
    break;
  case M_HITACHI:
    mptr->type = MOUSE_HITACHI;
    mptr->flags = CS8 | CREAD | CLOCAL | HUPCL;
    break;
  case M_BUSMOUSE:
    mptr->type = MOUSE_BUSMOUSE;
    mptr->flags = 0;
    break;
  case M_CLEARDTR:
    if (mptr->type == MOUSE_MOUSESYSTEMS)
      mptr->cleardtr = TRUE;
    else
      parse_error("Option CLEARDTR is only valid for some MouseSystems-Mice");
    break;
  default:
    error("CONF: Unknown mouse token: %s", word->name);
    break;
  }

  if (arg)
    free(arg);
  return (word->token);
}

int
do_debug(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;

  c_printf("CONF: do_debug\n");

  switch (word->form) {
  case VALUE:
    arg = get_arg(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case DBG_IO:
    d.io = (atoi(arg) != 0);
    c_printf("DEBUG: IO-Port %s!\n", d.io ? "on" : "off");
    break;
  case DBG_VIDEO:
    d.video = atoi(arg); 
    c_printf("DEBUG: Video %s!\n", d.video ? "on" : "off");
    break;
  case DBG_SERIAL:
    d.serial = atoi(arg); 
    c_printf("DEBUG: Serial %s!\n", d.serial ? "on" : "off");
    break;
  case DBG_CONFIG:
    d.config = atoi(arg); 
    c_printf("DEBUG: Config %s!\n", d.config ? "on" : "off");
    break;
  case DBG_DISK:
    d.disk = atoi(arg); 
    c_printf("DEBUG: Disk %s!\n", d.disk ? "on" : "off");
    break;
  case DBG_READ:
    d.read = atoi(arg); 
    c_printf("DEBUG: Read %s!\n", d.read ? "on" : "off");
    break;
  case DBG_WRITE:
    d.write = atoi(arg); 
    c_printf("DEBUG: Write %s!\n", d.write ? "on" : "off");
    break;
  case DBG_KEYB:
    d.keyb = atoi(arg); 
    c_printf("DEBUG: Keyboard %s!\n", d.keyb ? "on" : "off");
    break;
  case DBG_PRINTER:
    d.printer = atoi(arg); 
    c_printf("DEBUG: Printer %s!\n", d.printer ? "on" : "off");
    break;
  case DBG_WARNING:
    d.warning = atoi(arg); 
    c_printf("DEBUG: Warning %s!\n", d.warning ? "on" : "off");
    break;
  case DBG_GENERAL:
    d.general = atoi(arg); 
    c_printf("DEBUG: General %s!\n", d.general ? "on" : "off");
    break;
  case DBG_XMS:
    d.xms = atoi(arg); 
    c_printf("DEBUG: XMS %s!\n", d.xms ? "on" : "off");
    break;
  case DBG_DPMI:
    d.dpmi = atoi(arg); 
    c_printf("DEBUG: DPMI %s!\n", d.dpmi ? "on" : "off");
    break;
  case DBG_MOUSE:
    d.mouse = atoi(arg); 
    c_printf("DEBUG: Mouse %s!\n", d.mouse ? "on" : "off");
    break;
  case DBG_HARDWARE:
    d.hardware = atoi(arg); 
    c_printf("DEBUG: Hardware %s!\n", d.hardware ? "on" : "off");
    break;
  case DBG_IPC:
    d.IPC = atoi(arg); 
    c_printf("DEBUG: IPC %s!\n", d.IPC ? "on" : "off");
    break;
  case DBG_EMS:
    d.EMS = atoi(arg); 
    c_printf("DEBUG: EMS %s!\n", d.EMS ? "on" : "off");
    break;
  case DBG_NET:
    d.network = atoi(arg); 
    c_printf("DEBUG: network %s!\n", d.network ? "on" : "off");
    break;
  
  default:
    error("CONF: unknown debug token: %s\n", word->name);
    break;
    break;
  }
  if (arg)
    free(arg);
  return (word->token);
}

int
do_keyboard(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;

  c_printf("CONF: do_keyboard\n");

  switch (word->form) {
  case VALUE:
    arg = get_arg(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case KEYB_LAYOUT:
    switch (atoi(arg)) {
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
    default:
      error("CONF: unknown keyboard layout: %s\n", word->name);
      break;
    }
    break;
  case KEYB_RAWKEYBOARD:
    config.console_keyb = (atoi(arg) != 0);
    c_printf("CONF: RawKeyboard %s!\n", config.console_keyb ? "on" : "off");
    break;
  case KEYB_KEYBINT:
    config.keybint = atoi(arg);
    c_printf("CONF: keybint %s!\n", config.keybint ? "on" : "off");
    break;
  default:
    error("CONF: unknown debug token: %s\n", word->name);
    break;
  }
  if (arg)
    free(arg);
  return (word->token);
}

int
do_serial(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;
  char *end;

  switch (word->form) {
  case VALUE:
    arg = get_name(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case S_DEVICE:
    strcpy(sptr->dev, arg);
    break;
  case S_PORT:
    sptr->real_comport = strtol(arg, &end, 0);
    com_port_used[sptr->real_comport] = 1;
    break;
  case S_BASE:
    sptr->base_port = strtol(arg, &end, 0);
    break;
  case S_IRQ:
    sptr->interrupt = strtol(arg, &end, 0);
    break;
  case S_MOUSE:
    sptr->mouse = 1;
    break;
  default:
    error("CONF: unknown serial token: %s\n", word->name);
    break;
  }
  if (arg)
    free(arg);
  return (word->token);
}

void
start_printer(word_t * word, arg_t farg1, arg_t farg2)
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

void
stop_printer(word_t * word, arg_t farg1, arg_t farg2)
{
  c_printf("CONF(LPT%d) f: %s   c: %s  o: %s  t: %d\n",
	   c_printers, pptr->dev, pptr->prtcmd, pptr->prtopt, pptr->delay);
  c_printers++;
  config.num_lpt = c_printers;
}

int
do_printer(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL;

  /* c_printf("do_printer: %s %d\n", word->name, word->token, farg1, farg2); */

  switch (word->form) {
  case VALUE:
    arg = get_name(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case RBRACE:
  case LBRACE:
    break;
  case P_COMMAND:
    pptr->prtcmd = strdup(arg);
    break;
  case P_OPTIONS:
    pptr->prtopt = strdup(arg);
    break;
  case P_TIMEOUT:
    pptr->delay = atoi(arg);
    break;
  case P_FILE:
    pptr->dev = strdup(arg);
    break;
  default:
    error("CONF: unknown printer token: %s\n", word->name);
    break;
  }

  if (arg)
    free(arg);
  return (word->token);
}

int
do_top(word_t * word, arg_t arg1, arg_t arg2)
{
  int oldstate = state;
  char *oldstatestr = statestr;

  /*  c_printf("do_top: called with %s %d\n", word->name, word->token); */

  state = word->token;
  statestr = word->name;

  switch (word->form) {
  case COMMENT:
    read_until_eol(globl_file);
    break;
  case SECTION:
    switch (word->token) {
    case SEC_PORTS:
    case SEC_COMMENT:
    case SEC_DISK:
    case SEC_FLOPPY:
    case SEC_BOOTDISK:
    case SEC_PRINTER:
    case SEC_VIDEO:
    case SEC_TERMINAL:
    case SEC_SERIAL:
    case SEC_MOUSE:
    case SEC_DEBUG:
    case SEC_KEYBOARD:
      if (word->start)
	(word->start) (word, 0, 0);
      parse_file(word->sub_words, globl_file);
      if (word->stop)
	(word->stop) (word, 0, 0);
      break;
    default:
      c_printf("bad top-level section token: %s %d\n", word->name,
	       word->token);
    }
    break;

  default:
    c_printf("bad top-level form: %s %d\n", word->name,
	     word->form);
  }
  state = oldstate;
  statestr = oldstatestr;
  return NULLFORM;
}

/*
 * start_disk - start parsing one disk-statement
 */

void start_disk(word_t * word, arg_t arg1, arg_t arg2)
{
  switch(word->token)
    {
    case SEC_BOOTDISK:               /* bootdisk-section */
      if (config.bootdisk)           /* Already a bootdisk configured ? */
	parse_error("There is already a bootdisk configured");
      
      dptr = &bootdisk;              /* set pointer do bootdisk-struct */
      
      dptr->sectors = 0;             /* setup default-values           */
      dptr->heads   = 0;
      dptr->tracks  = 0;
      dptr->type    = FLOPPY;
      dptr->default_cmos = THREE_INCH_FLOPPY;
      dptr->timeout = 0;
      break;

    case SEC_FLOPPY:                 /* floppy section */
      if (c_fdisks >= MAX_FDISKS)
	{
	  parse_error("There are too many floppy disks defined");
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
      break;

    case SEC_DISK:                   /* Hard-disk section */
      if (c_hdisks >= MAX_HDISKS) 
	{
	  parse_error("There are too many hard disks defined");
	  dptr = &nulldisk;          /* Dummy-Entry to avoid core-dumps */
	}
      else
	dptr = &hdisktab[c_hdisks];
      
      dptr->type    =  NODISK;
      dptr->sectors = -1;
      dptr->heads   = -1;
      dptr->tracks  = -1;
      dptr->timeout = 0;
      break;

    default:
      parse_error("unknown disk type in parsing a disk-section");
      break;
    }

  dptr->dev_name = NULL;              /* default-values */
  dptr->rdonly = 0;
  dptr->header = 0;
}

/*
 * stop_disk - A disk-option has been parsed, do the last checks
 */

void stop_disk(word_t * word, arg_t arg1, arg_t arg2)
{
  if (dptr == &nulldisk)              /* is there any disk? */
    return;                           /* no, nothing to do */

  if (!dptr->dev_name)                /* Is there a file/device-name? */
    parse_error("disk: no device/file-name given!");
  else                                /* check the file/device for existance */
    {
      struct stat file_status;        /* date for checking that file */

      c_printf("device: %s ", dptr->dev_name);
      if (stat(dptr->dev_name,&file_status) != 0) /* Does this file exist? */
	{
	  char *str;

	  str = malloc(128);             /* No, issue an error-message */
	  sprintf(str,"Disk-device/file %s doesn't exist.",dptr->dev_name);
	  parse_error(str);

	  free(str);
	}
    }

  if (dptr->type == NODISK)    /* Is it one of bootdisk, floppy, harddisk ? */
    parse_error("disk: no device/file-name given!"); /* No, error */
  else
    c_printf("type %d ", dptr->type);

  if (dptr->type == PARTITION)
    {
      struct stat file_status;        /* date for checking that file */

      c_printf("partition# %d ", dptr->part_info.number);
      
      if (stat(dptr->part_info.file,&file_status) != 0) /* check part_info */
	{
	  char *str;
	  
	  str = malloc(128);             /* issue an error-message */
	  sprintf(str,"Partition-Info %s doesn't exist.",dptr->part_info.file);
	  parse_error(str);
	  
	  free(str);
	}
    }

  if (dptr->header)
    c_printf("header_size: %ld ", (long) dptr->header);

  c_printf("h: %d  s: %d   t: %d\n", dptr->heads, dptr->sectors,
	   dptr->tracks);

  if (word->token == SEC_BOOTDISK) {
    config.bootdisk = 1;
    use_bootdisk = 1;
  }
  else if (word->token == SEC_FLOPPY) {
    c_fdisks++;
    config.fdisks = c_fdisks;
  }
  else {
    c_hdisks++;
    config.hdisks = c_hdisks;
  }
}

/*
 * do_disk - parse a disk-section of a config-file.
 *           The sections for bootdisk, floppy and disk(harddisk)
 *           are very similar, so all these sections are parsed here.
 */

int do_disk(word_t * word, arg_t farg1, arg_t farg2)
{
  char *arg = NULL, *arg2 = NULL;

  /* c_printf("do_disk: %s %d\n", word->name, word->token, farg1, farg2); */

  switch (word->form) {
  case VALUE2:
    arg = get_name(globl_file);
    arg2 = get_name(globl_file);
    break;
  case VALUE:
    arg = get_name(globl_file);
    break;
  case PRED:
    break;
  default:
    break;
  }

  switch (word->token) {
  case D_READONLY:
    dptr->rdonly = 1;
    break;
  case D_FIVEINCH:
    dptr->default_cmos = FIVE_INCH_FLOPPY;
    break;
  case D_THREEINCH:
    dptr->default_cmos = THREE_INCH_FLOPPY;
    break;
  case D_SECTORS:
    dptr->sectors = atoi(arg);
    break;
  case D_CYLINDERS:
    dptr->tracks = atoi(arg);
    break;
  case D_HEADS:
    dptr->heads = atoi(arg);
    break;
  case D_FILE:
    if (dptr->dev_name != NULL)
      parse_error("Two names for a disk-image file or device given.");
    dptr->dev_name = arg;
    arg = NULL;
    break;
  case D_HDIMAGE:
    if (dptr->dev_name != NULL)
      parse_error("Two names for a harddisk-image file given.");
    dptr->type = IMAGE;
    dptr->header = HEADER_SIZE;
    dptr->dev_name = arg;
    arg = NULL;
    break;
  case D_WHOLEDISK:
  case D_HARD:
    if (dptr->dev_name != NULL)
      parse_error("Two names for a harddisk device given.");
    dptr->type = HDISK;
    dptr->dev_name = arg;
    arg = NULL;
    break;
  case D_FLOPPY:
    if (dptr->dev_name != NULL)
      parse_error("Two names for a floppy-device given.");
    dptr->type = FLOPPY;
    dptr->dev_name = arg;
    arg = NULL;
    break;
  case D_PARTITION:
    if (dptr->dev_name != NULL)
      parse_error("Two names for a partition given.");
    dptr->type = PARTITION;
    dptr->part_info.number = atoi(arg2);
    dptr->dev_name = arg;

    dptr->part_info.file = malloc(strlen(PARTITION_PATH ".")+10); 
    dptr->part_info.file = strcpy(dptr->part_info.file,PARTITION_PATH "."); 
    dptr->part_info.file = strcat(dptr->part_info.file,dptr->dev_name+5);

    arg = NULL;
    break;
  case D_OFFSET:
    dptr->header = atoi(arg);
    break;
  case RBRACE:
    break;
  default:
    break;
  }

  if (arg)
    free(arg);
  if (arg2)
    free(arg2);
  return word->token;
}

void
parse_file(word_t * words, FILE * fd)
{
  word_t *mword;
  char *gn;
  int funrtn = 0;

  int oldstate = state;
  char *oldstatestr = statestr;

  gn = get_name(fd);

  while (gn && (funrtn != RBRACE)) {
    if ((mword = match_name(words, gn))) {
      if (mword->func) {
	funrtn = (mword->func) (mword, 0, 0);
      }
      else {
	funrtn = mword->token;
      }
    }
    else if (state != SEC_COMMENT) {
      word_t badword;

      badword.name = gn;
      badword.token = NULLTOK;
      badword.form = NULLFORM;
      badword.func = 0;
      badword.start = 0;
      badword.start = 0;

      if (words[0].func)
	(words[0].func) (&badword, 0, 0);
    }

    free(gn);
    if (funrtn != RBRACE)
      gn = get_name(fd);
  }

  state = oldstate;
  statestr = oldstatestr;
}

int
parse_config(char *confname)
{
  FILE *volatile fd;

  c_hdisks = 0;
  c_fdisks = 0;

  if (/* !fd && */ !(fd = open_file(CONFIG_FILE))) {
    die("cannot open standard configuration file!");
  }

  if (!setjmp(exitpar))
    parse_file(top_words, fd);

  close_file(fd);

  if (!exchange_uids()) die("Cannot exchange uids\n");
  if (confname)
    fd = open_file(confname);
  else {
    char *home = getenv("HOME");
    char *name = malloc(strlen(home) + 20);

    sprintf(name, "%s/.dosrc", home);
    fd = open_file(name);
    free(name);
  }
  if (!exchange_uids()) die("Cannot changeback uids\n");

  if (!setjmp(exitpar))
    parse_file(top_words, fd);

  if (fd) close_file(fd);

  return 1;
}

#if 0
int
main(int argc, char **argv)
{
  if (argc != 2)
    die("no filename!");

  if (!parse_config(argv[1]))
    die("parse failed!\n");
}

#endif
