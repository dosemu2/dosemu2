/* parse.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 * rudimentary attempt at config file parsing for dosemu
 *
 * $Date: 1993/11/30 21:26:44 $
 * $Source: /home/src/dosemu0.49pl3/RCS/parse.c,v $
 * $Revision: 1.2 $
 * $State: Exp $
 *
 * $Log: parse.c,v $
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

#include "config.h"
#include "emu.h"
#include "cpu.h"
#include "disks.h"
#include "lpt.h"
#include "dosvga.h"
#include "serial.h"

extern struct config_info config;
extern struct CPU cpu;

jmp_buf exitpar;

#define MAX_SER 2
extern serial_t com[MAX_SER];
serial_t *sptr;
serial_t nullser;
int c_ser=0;

#define MAX_FDISKS 4
#define MAX_HDISKS 4
extern struct disk disktab[MAX_FDISKS];
extern struct disk hdisktab[MAX_HDISKS];
struct disk *dptr;
struct disk nulldisk;
int c_hdisks=0;
int c_fdisks=0;

extern struct printer lpt[NUM_PRINTERS];
struct printer *pptr;
struct printer nullptr;
int c_printers=0;


/***************************************************************/

typedef enum { NULLTOK=0, LBRACE, RBRACE, TOP, SEC_COMMENT, SEC_FLOPPY,
		 SEC_DISK, D_HEADS, D_SECTORS, D_CYLINDERS, D_FLOPPY, D_HARD, 
		 D_PARTITION, D_WHOLEDISK, D_OFFSET, D_FILE, D_FIVEINCH,
		 D_READONLY, D_HDIMAGE, D_THREEINCH, VAL_FASTFLOPPY, VAL_CPU,
		 SEC_VIDEO, VAL_XMS, VAL_EMS, VAL_DOSMEM, PRED_RAWKEY,
		 SEC_PRINTER, VAL_MATHCO, LCCOM, PRED_BOOTA, PRED_BOOTC,
		 VAL_DEBUG, EOL_COMMENT, P_COMMAND, P_OPTIONS, P_TIMEOUT,
		 P_FILE, SEC_PORTS, PRT_RANGE, PRT_RDONLY, PRT_WRONLY,
		 PRT_RDWR, PRT_ANDMASK, PRT_ORMASK, VAL_SPEAKER, 
		 V_CHUNKS, V_CHIPSET, V_MDA, V_VGA, V_CGA, V_EGA, V_VBIOSF,
		 V_VBIOSC, V_CONSOLE, V_FULLREST, V_PARTIALREST, V_GRAPHICS,
		 V_MEMSIZE, V_VBIOSM,  VAL_HOGTHRESHOLD, VAL_TIMER,
		 SEC_SERIAL, S_DEVICE, S_BASE, S_INTERRUPT, S_MODEM, S_MOUSE,
		 VAL_KEYBINT, VAL_TIMINT, VAL_EMUBAT, VAL_EMUSYS
	       } tok_t;

typedef enum { NULLFORM=0, ODELIM, CDELIM, COMMENT, SECTION, PRED,
		 VALUE, VALUE2, VALUEN } form_t;

typedef int arg_t;

typedef struct word_struct {
  char *name;
  tok_t token;
  form_t form;
  int (*func)(struct word_struct *, arg_t p1, arg_t p2);
  int (*start)(struct word_struct *, arg_t p1, arg_t p2);
  int (*stop)(struct word_struct *, arg_t p1, arg_t p2);
  struct word_struct *sub_words;
  arg_t p1, p2;
} word_t;

typedef struct syn_struct {
  char *syn;
  char *trans;
} syn_t;

int do_top(word_t *, arg_t, arg_t);
int do_num(word_t *, arg_t, arg_t);
void parse_file(word_t *, FILE *);
char *get_name(FILE *);

int do_comment(word_t *, arg_t, arg_t);

int do_disk(word_t *, arg_t, arg_t);
int start_disk(word_t *, arg_t, arg_t);
int stop_disk(word_t *, arg_t, arg_t);

int do_printer(word_t *, arg_t, arg_t);
int start_printer(word_t *, arg_t, arg_t);
int stop_printer(word_t *, arg_t, arg_t);

int do_video(word_t *, arg_t, arg_t);
int stop_video(word_t *, arg_t, arg_t);

int do_serial(word_t *, arg_t, arg_t);
int start_serial(word_t *, arg_t, arg_t);
int stop_serial(word_t *, arg_t, arg_t);

int do_ports(word_t *, arg_t, arg_t);

int glob_bad(word_t *, arg_t, arg_t);
int porttok(word_t *, arg_t, arg_t);

#define do_delim nullf

int nullf(word_t *, arg_t, arg_t);

#define NULL_WORD	{NULL,NULLTOK,NULLFORM,glob_bad,0,0,}
#define NULL_SYN	{NULL,NULL}

int
nullf(word_t *word, arg_t a1, arg_t a2)
{
  return word->token;
}


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
  {"chunks", V_CHUNKS, VALUE, do_video},
  NULL_WORD
};

word_t serial_words[] =
{
  NULL_WORD,
  {"{", LBRACE, ODELIM, do_serial},
  {"}", RBRACE, CDELIM, do_serial},
  {"device", S_DEVICE, VALUE, do_serial},
  {"base", S_BASE, VALUE, do_serial},
  {"interrupt", S_INTERRUPT, VALUE, do_serial},
  {"modem", S_MODEM, PRED, do_serial},
  {"mouse", S_MOUSE, PRED, do_serial},
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
  {"{", LBRACE, ODELIM, do_delim, nullf, nullf, null_words},
  {"}", RBRACE, CDELIM, do_delim, nullf, nullf, null_words},
  {"/*", SEC_COMMENT, SECTION, do_comment, nullf, nullf, comment_words}, 
  {"#", EOL_COMMENT, COMMENT, do_comment, nullf, nullf, comment_words},
  {";", EOL_COMMENT, COMMENT, do_comment, nullf, nullf, comment_words},
  {"disk", SEC_DISK, SECTION, do_top, start_disk, stop_disk, disk_words},
  {"floppy", SEC_FLOPPY, SECTION, do_top, start_disk, stop_disk, disk_words},
  {"video", SEC_VIDEO, SECTION, do_top, nullf, stop_video, video_words},
  {"serial", SEC_SERIAL, SECTION, do_top, start_serial, stop_serial, serial_words},
  {"xms", VAL_XMS, VALUE, do_num, nullf, nullf, null_words},
  {"ems", VAL_EMS, VALUE, do_num, nullf, nullf, null_words},
  {"dosmem", VAL_DOSMEM, VALUE, do_num, nullf, nullf, null_words},
  {"printer", SEC_PRINTER, SECTION, do_top, start_printer, stop_printer, printer_words},
  {"mathco", VAL_MATHCO, VALUE, do_num, nullf, nullf, null_words},
  {"speaker", VAL_SPEAKER, VALUE, do_num, nullf, nullf, null_words},
  {"rawkeyboard", PRED_RAWKEY, PRED, do_num, nullf, nullf, null_words},
  {"boota", PRED_BOOTA, PRED, do_num, nullf, nullf, null_words},
  {"bootc", PRED_BOOTC, PRED, do_num, nullf, nullf, null_words},
  {"cpu", VAL_CPU, VALUE, do_num, nullf, nullf, null_words},
  {"ports", SEC_PORTS, SECTION, do_top, nullf, nullf, port_words},
  {"debug", VAL_DEBUG, VALUE, do_num, nullf, nullf, null_words},
  {"messages", VAL_DEBUG, VALUE, do_num, nullf, nullf, null_words},
  {"hogthreshold", VAL_HOGTHRESHOLD, VALUE, do_num, nullf, nullf, null_words},
  {"timer", VAL_TIMER, VALUE, do_num, nullf, nullf, null_words},
  {"keybint", VAL_KEYBINT, VALUE, do_num, nullf, nullf, null_words},
  {"timint", VAL_TIMINT, VALUE, do_num, nullf, nullf, null_words},
  {"fastfloppy", VAL_FASTFLOPPY, VALUE, do_num, nullf, nullf, null_words},
  {"emusys", VAL_EMUSYS, VALUE, do_num, nullf, nullf, null_words},
  {"emubat", VAL_EMUBAT, VALUE, do_num, nullf, nullf, null_words},
  NULL_WORD
};

/* really should break this up into groups for each word, but there are
 * no conflicts yet
 */

/* XXX - fix these synonyms! */
syn_t global_syns[] =
{
  {"on" , "1"}, {"off", "0"},
  {"yes", "1"}, {"no" , "0"},
  {"80286", "2"}, {"80386", "3"},
  {"80486", "4"}, 
  {"emulated","2"}, {"native","1"},  /* for speaker */
  {"et4000", "2"}, {"trident", "1"},
  {"plainvga", "0"},
  NULL_SYN
};

#define iscomment(c) (c=='#' || c=='!' || c==';')
#define istoken(c) (!isspace(c) && !iscomment(c))
#define read_until_eol(fd) read_until_rtn_previous(fd, '\n')

#define die(reason) do { error("ERROR: par dead: %s\n", reason); \
			      longjmp(exitpar,1); } while(0)


/****************************** globals ************************/
FILE *globl_file = NULL;
tok_t state = TOP;
char *statestr = "top";

struct disk *dptr=NULL;
int hdiskno=0;
int diskno=0;


/***************************** functions ***********************/
int 
read_until_rtn_previous(FILE *fd, int goal)
{
  static int previous=0;
  int here, tmp;

  here = fgetc(fd);
  
  while ( !feof(fd) && here != goal) {
    previous=here;
    here = fgetc(fd);
  }

  if (feof(fd)) return EOF;
  else return previous;
}

int
glob_bad(word_t *word, arg_t a1, arg_t a2)
{
  c_printf("glob_bad called: %s!\n", word->name);
  return word->token;
}

int ports_permission = IO_RDWR;
unsigned int ports_ormask = 0;
unsigned int ports_andmask = 0xFFFF;

int
porttok(word_t *word, arg_t a1, arg_t a2)
{
  long num;
  char *end;
  num = strtol(word->name, &end, 0);
  allow_io(num, 1, ports_permission,ports_ormask,ports_andmask);
}


int
do_ports(word_t *word, arg_t a1, arg_t a2)
{
  char *arg=NULL, *arg2=NULL;

/*  c_printf("do_ports: %s %d\n", word->name, word->token); */

  switch (word->form) 
    {
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

  switch (word->token)
    {
      char *end;
    case PRT_RANGE: {
      long num1, num2;

      num1 = strtol(arg, &end, 0);
      num2 = strtol(arg2, &end, 0);
      c_printf("CONF: range of I/O ports 0x%04x-0x%04x\n",
        (unsigned short) num1, (unsigned short) num2);
      allow_io(num1, num2-num1+1, ports_permission,ports_ormask,ports_andmask);
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
      ports_ormask = strtol(arg,&end,0);
      break;
    case PRT_ANDMASK:
      ports_andmask = strtol(arg,&end,0);
      break;
    default:
      break;
    }
  if (arg) free(arg);
  if (arg2) free(arg2);
  return word->token;
}


int
do_comment(word_t *word, arg_t a1, arg_t a2)
{
  if (word->token == EOL_COMMENT)
    read_until_eol(globl_file);
  else {
    int prev;

    do {
      if ((prev=read_until_rtn_previous(globl_file,'/')) == EOF)
	die("unterminated /* comment!\n");
    } while (prev != '*');
  }

  return word->token;
}

char *
get_name(FILE *fd)
{
  char tmps[100], *p=tmps;
  int tmpc;

  *p = 0;

  /* prime the pump */
  tmpc = fgetc(fd);

  while (!istoken(tmpc) && !feof(fd)) 
    {
      if (iscomment(tmpc)) read_until_eol(globl_file);
      tmpc = fgetc(fd);
    }

#define QUOTE '"' 

  /* there is no escaping of quote marks yet...is there a need? */

  if (tmpc == QUOTE) {
    tmpc = fgetc(fd);
    while ( tmpc != QUOTE && !feof(fd) )
    { 
      *p++ = tmpc; 
      tmpc = fgetc(fd); 
    } 
  }
  else 
    {
      while ( istoken(tmpc) && !feof(fd))
	{
	  *p++ = tmpc;
	  tmpc = fgetc(fd);
	};
      if (iscomment(tmpc)) read_until_eol(globl_file);
    }
    
  *p=0;
  if (strlen(tmps) == 0) return 0;

  p = malloc(strlen(tmps) + 1);
  strcpy(p, tmps);
  return p;
}


/* case-insensitive match of two strings */
int
same_name(char *str1, char *str2)
{
  for ( ; *str1 && *str2 && tolower(*str1) == tolower(*str2) ; 
       str1++, str2++)
    /* empty loop */    
    ;

  if (!*str1 && !*str2) return 1;
  else return 0;
}


char *
synonym(syn_t *syntable, char * str)
{
  int i;

  for (i = 0; syntable[i].trans; i++) 
      if (same_name(syntable[i].syn, str)) break;

  return syntable[i].trans;
}


char *
get_arg(FILE *fd)
{
  char *str, *str2;

  str=get_name(fd);
  if (str2=synonym(global_syns, str)) 
    { 
      free(str); 
      str=strdup(str2); 
    }

  return str;
}


FILE *
open_file(char *filename)
{
  return( globl_file=fopen(filename, "r") );
}


void
close_file(FILE *file)
{
  fclose(file);
}


word_t *
match_name(word_t *words, char *name)
{
  int i;

  for (i = 1; words[i].name; i++) 
      if (same_name(words[i].name, name)) return &words[i];

  return NULL;
}


int
do_num (word_t *word, arg_t farg1, arg_t farg2)
{
  char *arg=NULL;

  switch (word->form)
    {
    case PRED:
      switch (word->token) 
	{
	case PRED_BOOTA:
	  config.hdiskboot = 0;
	  break;
	case PRED_BOOTC:
	  config.hdiskboot = 1;
	  break;
	case PRED_RAWKEY:
	  config.console_keyb = 1;
	  break;
	default:
	  c_printf("PAR: set pred %s\n", word->name);
	  break;
	}
      break;

    case VALUE:
      arg = get_arg(globl_file);

      switch (word->token) 
	{
	case VAL_EMUBAT:
	  config.emubat = strdup(arg);
	  break;
	case VAL_EMUSYS:
	  config.emusys = strdup(arg);
	  break;
	case VAL_FASTFLOPPY:
	  config.fastfloppy = atoi(arg);
	  break;
	case VAL_DEBUG:
	  parse_debugflags(arg);
	  break;
	case VAL_CPU:
	  cpu.type = atoi(arg);
	  break;
	case VAL_HOGTHRESHOLD:
	  config.hogthreshold = atoi(arg);
	  break;
	case VAL_TIMINT:
	  config.timers = atoi(arg);
	  c_printf("CONF: timers %s!\n", config.timers ? "on" : "off");
	  break;
	case VAL_KEYBINT:
	  config.keybint = atoi(arg);
	  c_printf("CONF: keybint %s!\n", config.keybint ? "on" : "off");
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
	case VAL_DOSMEM:
	  config.mem_size = atoi(arg);
	  break;
	case VAL_MATHCO:
	  config.mathco = (atoi(arg) != 0);
	  break;
	case VAL_SPEAKER:
	  config.speaker = atoi(arg);
	  if (config.speaker == SPKR_NATIVE) 
	    {
	      warn("CONF: allowing access to the speaker ports!\n");
	      allow_io(0x42, 1, IO_RDWR, 0, 0xFFFF);
	      allow_io(0x61, 1, IO_RDWR, 0, 0xFFFF);
	    }
	  else c_printf("CONF: not allowing speaker port access\n");
	  break;
	default:
	  c_printf("val %s set to %s\n", word->name, arg);
	  break;
	}

      if (arg) free(arg);
      break;
    default:
      c_printf("non-value in do_val!\n");
    }
}


int
stop_video (word_t *word, arg_t farg1, arg_t farg2)
{
  if ((config.cardtype != CARD_VGA) || !config.console_video)
    {
      config.graphics=0;
      config.vga=0;
    }

  if (config.vga)
    {
      if (config.mem_size > 640) config.mem_size = 640;
      config.mapped_bios = 1;
      config.console_video = 1;
    }
}


int
do_video (word_t *word, arg_t farg1, arg_t farg2)
{
  char *arg=NULL;

/* c_printf("do_printer: %s %d\n", word->name, word->token, farg1, farg2); */

  switch (word->form) 
    {
    case VALUE:
      arg = get_arg(globl_file);
      break;
    case PRED:
      break;
    default:
      break;
  }

  switch (word->token)
    {
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
    case V_CHUNKS:
      config.redraw_chunks = atoi(arg);
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
    default:
      error("CONF: unknown video token: %s\n", word->name);
      break;
    }

  if (arg) free(arg);
  return (word->token);
}



int
start_serial (word_t *word, arg_t farg1, arg_t farg2)
{
  if (c_ser >= MAX_SER) sptr = &nullser;
  else {
    sptr = &com[c_ser];
    sptr->mouse = 0;
    /* default first port */
    if (c_ser == 0) {
      strcpy(sptr->dev,"/dev/cua0"); 
      sptr->base_port=0x3f8;
      sptr->interrupt=0xc;
      sptr->fd=-1;
    }
    else {
      strcpy(sptr->dev,"/dev/cua1"); 
      sptr->base_port=0x2f8;
      sptr->interrupt=0xb;
      sptr->fd=-1;
    }
  }
}


int
stop_serial (word_t *word, arg_t farg1, arg_t farg2)
{
  c_ser++;
  config.num_ser = c_ser;
  c_printf("SER: %s port %x int %x\n", sptr->dev, sptr->base_port, 
	   sptr->interrupt);
}


int
do_serial (word_t *word, arg_t farg1, arg_t farg2)
{
  char *arg=NULL;
  char *end;

  switch (word->form) 
    {
    case VALUE:
      arg = get_name(globl_file);
      break;
    case PRED:
      break;
    default:
      break;
  }

  switch (word->token)
    {
    case RBRACE:
    case LBRACE:
      break;
    case S_DEVICE:
      strcpy(sptr->dev,arg);
      break;
    case S_BASE:
      sptr->base_port = strtol(arg, &end, 0);
      break;
    case S_INTERRUPT:
      sptr->interrupt = strtol(arg, &end, 0);
      break;
    case S_MODEM:
      sptr->mouse = 0;
      break;
    case S_MOUSE:
      sptr->mouse = 1;
      break;
    default:
      error("CONF: unknown serial token: %s\n", word->name);
      break;
    }

  if (arg) free(arg);
  return (word->token);
}



int
start_printer (word_t *word, arg_t farg1, arg_t farg2)
{
  if (c_printers >= NUM_PRINTERS) pptr = &nullptr;
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
    pptr->dev=NULL;
    pptr->file=NULL;
    pptr->remaining=-1;
    pptr->delay=10;
  }
}


int
stop_printer (word_t *word, arg_t farg1, arg_t farg2)
{
  c_printf("CONF(LPT%d) f: %s   c: %s  o: %s  t: %d\n", 
	   c_printers, pptr->dev, pptr->prtcmd, pptr->prtopt, pptr->delay);
  c_printers++;
  config.num_lpt = c_printers;
}


int
do_printer (word_t *word, arg_t farg1, arg_t farg2)
{
  char *arg=NULL;

/* c_printf("do_printer: %s %d\n", word->name, word->token, farg1, farg2); */

  switch (word->form) 
    {
    case VALUE:
      arg = get_name(globl_file);
      break;
    case PRED:
      break;
    default:
      break;
  }

  switch (word->token)
    {
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

  if (arg) free(arg);
  return (word->token);
}

int
do_top (word_t *word, arg_t arg1, arg_t arg2)
{
  int oldstate=state;
  char *oldstatestr=statestr;

/*  c_printf("do_top: called with %s %d\n", word->name, word->token); */

  state = word->token;
  statestr = word->name;

  switch (word->form) 
    {
    case COMMENT:
      read_until_eol(globl_file);
      break;
    case SECTION: 
      switch(word->token)
	{
	case SEC_PORTS:
	case SEC_COMMENT:
	case SEC_DISK:
	case SEC_FLOPPY:
	case SEC_PRINTER:
	case SEC_VIDEO:
	case SEC_SERIAL:
	  (word->start)(word, 0, 0);
	  parse_file(word->sub_words, globl_file);
	  (word->stop)(word, 0, 0);
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


int
start_disk (word_t *word, arg_t arg1, arg_t arg2)
{
  if (word->token == SEC_FLOPPY) {
    if (c_fdisks >= MAX_FDISKS) {
      error("ERROR: config: too many floppy disks defined!\n");
      dptr = &nulldisk;
    }
    else dptr = &disktab[c_fdisks];

    dptr->dev_name=NULL;
    dptr->rdonly=0;
    dptr->sectors = dptr->heads = dptr->tracks = 0;
    dptr->type = FLOPPY;
    dptr->default_cmos = THREE_INCH_FLOPPY;
    dptr->header = 0;
    dptr->timeout = 0;
  } else {
    if (c_hdisks >= MAX_HDISKS) {
      error("ERROR: config: too many hard disks defined!\n");
      dptr = &nulldisk;
    }
    else dptr = &hdisktab[c_hdisks];

    dptr->dev_name=NULL;
    dptr->rdonly=0;
    dptr->sectors = dptr->heads = dptr->tracks = -1;
    dptr->type = NODISK;
    dptr->header = 0;
  }
}


int
stop_disk (word_t *word, arg_t arg1, arg_t arg2)
{
  if (word->token == SEC_FLOPPY) {

    if (!dptr->dev_name) die("no device name!");
    else c_printf("device: %s ", dptr->dev_name);
    
    if (dptr->type == NODISK) die("no device type!");
    else c_printf("type %d ", dptr->type);
    
    if (dptr->type == PARTITION) c_printf("partition# %d ",
					  dptr->part_info.number); 
    
    if (dptr->header) c_printf("header_size: %ld ", (long) dptr->header);
    
    c_printf("h: %d  s: %d   t: %d\n", dptr->heads, dptr->sectors,
	     dptr->tracks);

    c_fdisks++;
    config.fdisks = c_fdisks;
    
  } else {
    if (!dptr->dev_name) die("no device name!");
    else c_printf("device: %s ", dptr->dev_name);
    
    if (dptr->type == NODISK) die("no device type!");
    else c_printf("type %d ", dptr->type);
    
    if (dptr->type == PARTITION) c_printf("partition# %d ",
					  dptr->part_info.number); 
    
    if (dptr->header) c_printf("header_size: %ld ", (long) dptr->header);
    
    c_printf("h: %d  s: %d   t: %d\n", dptr->heads, dptr->sectors,
	     dptr->tracks);

    c_hdisks++;
    config.hdisks = c_hdisks;
  }
}


int
do_disk (word_t *word, arg_t farg1, arg_t farg2)
{
  char *arg=NULL, *arg2=NULL;

/* c_printf("do_disk: %s %d\n", word->name, word->token, farg1, farg2); */

  switch (word->form) 
    {
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

  switch (word->token)
    {
    case D_READONLY:
      dptr->rdonly=1;
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
      dptr->dev_name = arg;
      arg = NULL;
      break;
    case D_HDIMAGE:
      dptr->type = IMAGE;
      dptr->header = HEADER_SIZE;
      dptr->dev_name = arg;
      arg = NULL;
      break;
    case D_WHOLEDISK:
    case D_HARD:
      dptr->type = HDISK;
      dptr->dev_name = arg;
      arg = NULL;
      break;
    case D_FLOPPY:
      dptr->type = FLOPPY;
      dptr->dev_name = arg;
      arg = NULL;
      break;
    case D_PARTITION:
      dptr->type = PARTITION;
      dptr->part_info.number = atoi(arg2);
      dptr->dev_name = arg;
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

  if (arg) free(arg);
  if (arg2) free(arg2);
  return word->token;
}


void
parse_file(word_t *words, FILE *fd)
{
  word_t *mword;
  char *gn;
  int funrtn=0;

  int oldstate=state;
  char *oldstatestr=statestr;

  gn=get_name(fd);

  while ( gn && (funrtn != RBRACE) )
    {
      if (mword=match_name(words, gn))
	  funrtn = (mword->func)(mword, 0, 0);

      else if (state != SEC_COMMENT) {
	word_t badword;

	badword.name=gn;
	badword.token=NULLTOK;
	badword.form=NULLFORM;
	badword.func=nullf;
	badword.start=nullf;
	badword.start=nullf;

	(words[0].func)(&badword, 0, 0);
      }

      free(gn);
      if (funrtn != RBRACE) gn=get_name(fd);
    }

  state=oldstate;
  statestr=oldstatestr;
}

int
parse_config(char *confname)
{
  FILE * volatile fd;

  if (confname)
    fd = open_file(confname);
  else {
    char *home = getenv("HOME");
    char *name = malloc(strlen(home) + 20);
    sprintf(name,"%s/.dosrc", home);
    fd = open_file(name);
    free(name);
  }

  if ( !fd && !(fd = open_file(CONFIG_FILE)) )  {
    die("cannot open configuration files!");
    return 0;
  }

  c_hdisks=0;
  c_fdisks=0;

  if (! setjmp(exitpar))
    parse_file(top_words, fd);

  close_file(fd);
  return 1;
}


#if 0
int
main(int argc, char **argv)
{
 if (argc != 2) die("no filename!");

 if ( ! parse_config(argv[1]))
   die("parse failed!\n");
}
#endif
