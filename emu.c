/* dos emulator, Matthias Lautner */

#define EMU_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/11/17 22:29:33 $
 * $Source: /home/src/dosemu0.49pl2/RCS/emu.c,v $
 * $Revision: 1.4 $
 * $State: Exp $
 *
 * $Log: emu.c,v $
 * Revision 1.4  1993/11/17  22:29:33  root
 * *** empty log message ***
 *
 * Revision 1.3  1993/11/15  19:56:49  root
 * Fixed sp -> ssp overflow, it is a hack at this time, but it works.
 *
 * Revision 1.2  1993/11/12  13:00:04  root
 * Keybuffer updates. REAL_INT16 addition. Link List for Hard INTs.
 *
 * Revision 1.7  1993/07/21  01:52:19  rsanders
 * uses new ems.sys for EMS emulation
 *
 * Revision 1.6  1993/07/19  18:44:01  rsanders
 * removed all "wait on ext. event" messages
 *
 * Revision 1.5  1993/07/14  04:34:06  rsanders
 * changed printing of "wait on external event" warnings.
 *
 * Revision 1.4  1993/07/13  19:18:38  root
 * changes for using the new (0.99pl10) signal stacks
 *
 * Revision 1.3  1993/07/07  21:42:04  root
 * minor changes for -Wall
 *
 * Revision 1.2  1993/07/07  01:33:10  root
 * hook for parse_config(name);
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.27  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.26  1993/04/07  21:04:26  root
 * big move
 *
 * Revision 1.25  1993/04/05  17:25:13  root
 * big pre-49 checkin; EMS, new MFS redirector, etc.
 *
 * Revision 1.24  1993/03/04  22:35:12  root
 * put in perfect shared memory, HMA and everything.  added PROPER_STI.
 *
 * Revision 1.23  1993/03/02  03:06:42  root
 * somewhere between 0.48pl1 and 0.49 (with IPC).  added virtual IOPL
 * and AC support (for 386/486 tests), -3 and -4 flags for choosing.
 * Split dosemu into 2 processes; the child select()s on the keyboard,
 * and signals the parent when a key is received (also sends it on a
 * UNIX domain socket...this might not work well for non-console keyb).
 *
 */


#if STATIC
        __asm__(".org 0x110000");
#endif

/* make sure that this line ist the first of emu.c
   and link emu.o as the first object file to the lib */
	__asm__("___START___: jmp _emulate\n");

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <termio.h>
#include <termcap.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <sys/vm86.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "bios.h"
#include "termio.h"
#include "dosvga.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "dosipc.h"
#include "disks.h"
#include "xms.h"


#define OUTBUFSIZE	3000

/* thanks to Andrew Haylett (ajh@gec-mrc.co.uk) for catching character loss
 * in CHOUT */

#define CHOUT(c)   if (outp == &outbuf[OUTBUFSIZE-2]) { CHFLUSH } \
			*(outp++) = (c);

#define CHFLUSH    if (outp - outbuf) { v_write(2, outbuf, outp - outbuf); \
						outp = outbuf; }

void v_write(int, unsigned char *, int);

extern struct disk disktab[],
                  hdisktab[];

/* the default configuration is two floppies and two hard disks. You
 * can change it with the -H and -F flags, or by editing the numbers
 * below.  For example, you may wish to have only the hdimage hard drive,
 * plus your LINUX.EXE drive (i.e. no direct-access to your DOS
 * partition.
 * !!!!! You do this in the Makefile now! Don't tamper with it here! !!!!
 */

int exitearly=0;

struct config_info config;
#define FDISKS config.fdisks
#define HDISKS config.hdisks
#define MEM_SIZE	config.mem_size   /* 1k blocks of normal RAM */

extern struct vm86_struct vm86s;
extern struct CPU cpu;
int fatalerr;

struct timeval scr_tv;
struct itimerval itv;
unsigned char outbuf[OUTBUFSIZE], *outp = outbuf;


#define IQUEUE_LEN 400
int int_queue_start=0;
int int_queue_end=0;
struct int_queue_struct
{
  int interrupt;
  int (*callstart)();
  int (*callend)();
} int_queue[IQUEUE_LEN];

/* This is currently a KLUDGE at best, but it works, any one who
   wants to do it their way would get my thanks.
   I have made the INT16 run in real mode - Thnxs Tim Bird :-)
   if you find something that may not be working because of this,
   what I'm not sure, you should remove the following #define 
                                          - JES jmaclean@fox.nstn.ns.ca
*/
#define REAL_INT16 1

/* This is here to allow multiple hard_int's to be running concurrently.
   I know this is wrong, why I don't know, but has to be here for any 
   programs that steal INT9 away from DOSEMU.
*/
struct int_queue_list_struct
{
	struct int_queue_list_struct *prev;
	struct int_queue_list_struct *next;
	struct int_queue_struct *int_queue_ptr;
  	int int_queue_return_addr;
  	struct vm86_regs saved_regs;
} *int_queue_head, *int_queue_tail, *int_queue_temp;

u_char in16=0;

int scrtest_bitmap, update_screen;
unsigned char *scrbuf;  /* the previously updated screen */

long start_time;
unsigned long last_ticks;

int card_init=0;
unsigned long precard_eip, precard_cs;

/* XXX - the mem size of 734 is much more dangerous than 704.
 * 704 is the bottom of 0xb0000 memory.  use that instead?  
 */
#ifdef EXPERIMENTAL_GFX
#define MAX_MEM_SIZE    640
#else
#define MAX_MEM_SIZE    734	   /* up close to the 0xB8000 mark */
#endif

/* this holds all the configuration information, set in config_init() */
unsigned int configuration;
void config_init(void);

void boot(struct disk *);

extern void map_bios(void);
extern int open_kmem();

void leavedos(int),
  usage(void),
  robert_irq(int),
  hardware_init(void),
  do_int(int);

int dos_helper(void);
void init_vga_card(void);
extern void sigser(int);

/* Programmable Interrupt Controller, 8259 */
struct pic
{
  int stage;	/* where in init. , 0=ICW1 */
  	/* the seq. is ICW1 to 0x20, ICW2 to 0x21
         * if ICW1:D1=0, ICW3 to 0x20h, 
	 * ICW4 to 0x21, OCWs any order
	 */
  unsigned char
    ICW1,	/* Input Control Words */
    ICW2,
    ICW3,
    ICW4,
    OCW1,	/* Output Control Words */
    OCW2,
    OCW3;
} pics[2];

int port61=0xd0;  /* the pseudo-8255 device on AT's */

int special_nowait=0;

struct ioctlq iq = {0,0,0,0};	/* one-entry queue :-( for ioctl's */
int in_sighandler=0;		/* so I know to not use non-reentrant
				 * syscalls like ioctl() :-( */
int in_ioctl=0;
struct ioctlq curi = {0,0,0,0};				 

/* this is DEBUGGING code! */
int sizes=0;

struct debug_flags d = {0,0,0,0,1,0,1,1,0,0,0,1,1,1,0,1,1,0,1,1};

int poll_io=1;			/* polling io, default on */
int ignore_segv=0;		/* ignore sigsegv's */
int in_sigsegv=0;
int terminal_pipe;
int terminal_fd=-1;

#define kbd_flags *KBDFLAG_ADDR
#define key_flags *KEYFLAG_ADDR

int in_interrupt=0;		/* for unimplemented interrupt code */

/* for use by cli() and sti() */
sigset_t oldset;

unsigned char trans[] = /* LATIN CHAR SET */
{
	"\0\0\0\0\0\0\0\0\00\00\00\00\00\00\00\00"
	"\0\0\0\0\266\247\0\0\0\0\0\0\0\0\0\0"
	" !\"#$%&'()*+,-./"
	"0123456789:;<=>?"
	"@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmno"
	"pqrstuvwxyz{|}~ "
	"\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
	"\311\346\306\364\366\362\373\371\377\326\334\242\243\245\0\0"
	"\341\355\363\372\361\321\252\272\277\0\254\275\274\241\253\273"
	"\0\0\0|++++++|+++++"
	"++++-++++++++-++"
	"+++++++++++\0\0\0\0\0"
	"\0\337\0\0\0\0\265\0\0\0\0\0\0\0\0\0"
	"\0\261\0\0\0\0\367\0\260\267\0\0\0\262\244\0" 
};


void 
signal_init(void)
{
  sigset_t trashset;

  /* block no additional signals (i.e. get the current signal mask) */
  sigemptyset(&trashset);
  sigprocmask(SIG_BLOCK, &trashset, &oldset);

/*  g_printf("CLI/STI initialized\n"); */
}

void 
cli(void)
{
  sigset_t blockset;

  sigfillset(&blockset);
  DOS_SYSCALL( sigprocmask(SIG_SETMASK, &blockset, &oldset) );
}

void 
sti(void)
{
  sigset_t blockset;

  DOS_SYSCALL( sigprocmask(SIG_SETMASK, &oldset, &blockset) );
}


inline void 
run_vm86(void)
{
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */

  in_vm86 = 1;
  (void)vm86(&vm86s);
  in_vm86 = 0;
  /* this is here because ioctl() is non-reentrant, and signal handlers
   * may have to use ioctl().  This results in a possible (probable) time 
   * lag of indeterminate length (and a bad return value).  
   * Ah, life isn't perfect.
   *
   * I really need to clean up the queue functions to use real queues.
   */
  if (iq.queued) 
      do_queued_ioctl();
}


int 
outcbuf(int c)
{
  CHOUT(c);
  return 1;
}


void
dostputs(char *a, int b, outfuntype c)
{
  /* discard c right now */
  /* CHFLUSH; */
/* was "CHFLUSH; tputs(a,b,outcbuf);" */
  tputs(a,b,c);
}


inline void
poscur(int x, int y)
{
  /* were "co" and "li" */
  if ((unsigned)x >= CO || (unsigned)y >= LI) return;
  tputs(tgoto(cm, x, y), 1, outc);
}


#ifndef OLD_SCROLL
/* 
This is a better scroll routine, mostly for aesthetic reasons. It was
just too horrible to contemplate a scroll that worked 1 character at a
time :-) 

It may give some performance improvement on some systems (it does 
on mine)
(Andrew Tridgell) 
*/

void 
scroll(int x0, int y0 , int x1, int y1, int l, int att)
{
  int dx= x1-x0+1;
  int dy= y1-y0+1;
  int x, y;
  us *sadr, blank = ' ' | (att << 8);
  us *tbuf;
  
  if (dx<=0 || dy<=0 || x0<0 || x1>=CO || y0<0 || y1>=LI)
    return;
  
  update_screen = 0;
  
  /* make a blank line */
  tbuf = (us *)malloc(sizeof(us)*dx);
  if (!tbuf) 
    {
      error("failed to malloc temp buf in scroll!");
      return;
    }	
  for (x=0;x<dx;x++) tbuf[x] = blank;
  
  sadr=SCREEN_ADR(SCREEN);
  
  if (l>=dy || l<=-dy) l=0;
  
  if(l==0)       /* Wipe mode */
    {
      for(y=y0;y<=y1;y++)
	memcpy(&sadr[y*CO+x0],tbuf,dx*sizeof(us));
      free(tbuf);
      return;
    }
  
  if (l>0)
    {
      if (dx == CO)
	memcpy(&sadr[y0*CO],
	       &sadr[(y0+l)*CO],
	       (dy-l)*dx*sizeof(us));
      else
	for (y=y0;y<=(y1-l);y++)
	  memcpy(&sadr[y*CO + x0],&sadr[(y+l)*CO+x0],dx*sizeof(us));
      
      for (y=y1-l+1;y<=y1;y++)
	memcpy(&sadr[y*CO + x0],tbuf,dx*sizeof(us));
    }
  else
    {
      for (y=y1;y>=(y0-l);y--)
	memcpy(&sadr[y*CO + x0],&sadr[(y+l)*CO+x0],dx*sizeof(us));
      
      for (y=y0-l-1;y>=y0;y--)
	memcpy(&sadr[y*CO + x0],tbuf,dx*sizeof(us));
    }
  
  update_screen=0; 
  memcpy(scrbuf, sadr, CO*LI*2);
  free(tbuf);	
}


#else

void 
scrollup(int x0, int y0 , int x1, int y1, int l, int att)
{
  int dx, dy, x, y, ofs;
  us *sadr, *p, *q, blank = ' ' | (att << 8);
  
  if(l==0)		/* Wipe mode */
    {
      sadr=SCREEN_ADR(SCREEN);
      for(dy=y0;dy<=y1;dy++)
	for(dx=x0;dx<=x1;dx++)
	  sadr[dy*CO+dx]=blank;
      return;
    }
  
  sadr = SCREEN_ADR(SCREEN);
  sadr += x0 + CO * (y0 + l);
  
  dx = x1 - x0 +1;
  dy = y1 - y0 - l +1;
  ofs = -CO * l;
  for (y=0; y<dy; y++) {
    p = sadr;
    if (l != 0) for (x=0; x<dx; x++, p++) p[ofs] = p[0];
    else        for (x=0; x<dx; x++, p++) p[0] = blank;
    sadr += CO;
	}
  for (y=0; y<l; y++) {
    sadr -= CO;
    p = sadr;
    for (x=0; x<dx; x++, p++) *p = blank;
  }
  
  update_screen=1; 
}


void 
scrolldn(int x0, int y0 , int x1, int y1, int l, int att)
{
	int dx, dy, x, y, ofs;
	us *sadr, *p, blank = ' ' | (att << 8);

	if(l==0)
	{
		l=LI-1;		/* Clear whole if l=0 */
	}

	sadr = SCREEN_ADR(SCREEN);
	sadr += x0 + CO * (y1 -l);

	dx = x1 - x0 +1;
	dy = y1 - y0 - l +1;
	ofs = CO * l;
	for (y=0; y<dy; y++) {
		p = sadr;
		if (l != 0) for (x=0; x<dx; x++, p++) p[ofs] = p[0];
		else        for (x=0; x<dx; x++, p++) p[0] = blank;
		sadr -= CO;
	}
	for (y=0; y<l; y++) {
		sadr += CO;
		p = sadr;
		for (x=0; x<dx; x++, p++) *p = blank;
	}

	update_screen=1; 
}
#endif OLD_SCROLL


void
v_write(int fd, unsigned char *ch, int len)
{
  if (! config.console_video) DOS_SYSCALL( write(fd, ch, len) );
  else error("ERROR: (video) v_write deferred for console_video\n");
}


void 
char_out_att(unsigned char ch, unsigned char att, int s, int advflag)
{
	u_short *sadr;
	int xpos=XPOS(s), ypos=YPOS(s);

/*	if (s > max_page) return; */

	if (config.console_video)
	  {
	    if (ch >= ' ') {
	      sadr = SCREEN_ADR(s);
	      sadr[YPOS(s)*CO + XPOS(s)++] = ch | (att << 8);
	    } else if (ch == '\r') {
	      XPOS(s) = 0;
	    } else if (ch == '\n') {
	      YPOS(s)++;
	      XPOS(s)=0;  /* EDLIN needs this behavior */
	    } else if (ch == '\010' && XPOS(s) > 0) {
	      XPOS(s)--;
	    } else if (ch == '\t') {
	      v_printf("tab\n");
	      do {
		char_out(' ', s, advflag);
	      } while (XPOS(s) % 8 != 0); 
	    }

	    if (!advflag) 
	      {
		XPOS(s) = xpos;
		YPOS(s) = ypos;
	      }
	    else poscur(XPOS(s), YPOS(s));
	  }

	else {
	  unsigned short *wscrbuf=(unsigned short *)scrbuf;

	  /* update_screen not set to 1 because we do the outputting
	   * ourselves...scrollup() and scrolldn() should also work
	   * this way, when I get around to it.
	   */
	  update_screen=0;

	  /* this will need some fixing to work with advflag, so that extra
	   * characters won't cause wraparound...
	   */

	  if (ch >= ' ') {
	    sadr = SCREEN_ADR(s);
	    sadr[YPOS(s)*CO + XPOS(s)] = ch | (att << 8);
	    wscrbuf[YPOS(s)*CO + XPOS(s)] = ch | (att << 8); 
	    XPOS(s)++;
	    if (s == SCREEN) outc(trans[ch]);
	  } else if (ch == '\r') {
	    XPOS(s) = 0;
	    if (s == SCREEN) write(2, &ch, 1);
	  } else if (ch == '\n') {
	    YPOS(s)++;
	    XPOS(s)=0;  /* EDLIN needs this behavior */
	    if (s == SCREEN) write(2, &ch, 1);
	  } else if (ch == '\010' && XPOS(s) > 0) {
	    XPOS(s)--;
	    if ((s == SCREEN) && advflag) write(2, &ch, 1);
	  } else if (ch == '\t') {
		do {
		    char_out(' ', s, advflag);
	        } while (XPOS(s) % 8 != 0);
	  } else if (ch == '\010' && XPOS(s) > 0) {
	    XPOS(s)--;
	    if ((s == SCREEN) && advflag) write(2, &ch, 1);
	  }
	}

	if (advflag)
	  {
	    if (XPOS(s) == CO) {
	      XPOS(s) = 0;
	      YPOS(s)++;
	    }
	    if (YPOS(s) == LI) {
	      YPOS(s)--;
	      scrollup(0, 0, CO-1, LI-1, 1, 7);
	      update_screen=0 /* 1 */;
	    }
	  }
}


/* temporary hack... we'll merge this back into char_out_att later.
 * right now, I need to play with it.
 */
void 
char_out(unsigned char ch, int s, int advflag)
{
	us *sadr;
	int xpos=XPOS(s), ypos=YPOS(s);
	int newline_att = 7;

	if (config.console_video)
	  {
	    if (ch >= ' ') {
	      sadr = SCREEN_ADR(s);
	      sadr[YPOS(s)*CO + XPOS(s)] &= 0xff00;
	      sadr[YPOS(s)*CO + XPOS(s)++] |= ch;
	    } else if (ch == '\r') {
	      XPOS(s) = 0;
	    } else if (ch == '\n') {
	      YPOS(s)++;
	      XPOS(s)=0;  /* EDLIN needs this behavior */

	      /* color new line */
	      sadr = SCREEN_ADR(s);
	      newline_att = sadr[YPOS(s)*CO + XPOS(s) - 1] >> 8;

	    } else if (ch == '\010' && XPOS(s) > 0) {
	      XPOS(s)--;
	    } else if (ch == '\t') {
	      v_printf("tab\n");
	      do {
		char_out(' ', s, advflag);
	      } while (XPOS(s) % 8 != 0); 
	    }

	    if (!advflag) 
	      {
		XPOS(s) = xpos;
		YPOS(s) = ypos;
	      }
	    else poscur(XPOS(s), YPOS(s));
	  }

	else {
	  unsigned short *wscrbuf=(unsigned short *)scrbuf;

	  /* this will need some fixing to work with advflag, so that extra
	   * characters won't cause wraparound...
	   */

	  update_screen = 0;

	  if (ch >= ' ') {
	    sadr = SCREEN_ADR(s);
	    sadr[YPOS(s)*CO + XPOS(s)] &= 0xff00;
	    sadr[YPOS(s)*CO + XPOS(s)] |= ch;
	    wscrbuf[YPOS(s)*CO + XPOS(s)] &= 0xff00;
	    wscrbuf[YPOS(s)*CO + XPOS(s)] |= ch;
	    XPOS(s)++;
	    if (s == SCREEN) outc(trans[ch]);
	  } else if (ch == '\r') {
	    XPOS(s) = 0;
	    if (s == SCREEN) write(2, &ch, 1);
	  } else if (ch == '\n') {
	    YPOS(s)++;
	    XPOS(s)=0;  /* EDLIN needs this behavior */
	    if (s == SCREEN) write(2, &ch, 1);
	  } else if (ch == '\010' && XPOS(s) > 0) {
	    XPOS(s)--;
	    if ((s == SCREEN) && advflag) write(2, &ch, 1);
	  } else if (ch == '\t') {
		do {
		    char_out(' ', s, advflag);
	        } while (XPOS(s) % 8 != 0);
	  } else if (ch == '\010' && XPOS(s) > 0) {
	    XPOS(s)--;
	    if ((s == SCREEN) && advflag) write(2, &ch, 1);
	  }
	}

	if (advflag)
	  {
	    if (XPOS(s) == CO) {
	      XPOS(s) = 0;
	      YPOS(s)++;
	    }
	    if (YPOS(s) == LI) {
	      YPOS(s)--;
	      scrollup(0, 0, CO-1, LI-1, 1, newline_att);
	      update_screen=0 /* 1 */;
	    }
	  }
}


void 
clear_screen(int s, int att)
{
  us *sadr, *p, blank = ' ' | (att << 8);
  int lx;
  
  update_screen=0;
  v_printf("VID: cleared screen\n");
  if (s > max_page) return;
  sadr = SCREEN_ADR(s);
  cli();

  for (p = sadr, lx=0; lx < (CO*LI); *(p++) = blank, lx++); 
  if (! config.console_video)
    {
      memcpy (scrbuf, sadr, CO*LI*2);
      if (s == SCREEN) tputs(cl, 1, outc);
    }

  XPOS(s) = YPOS(s) = 0;
  poscur(0,0);
  sti();
  update_screen=0;
}


/* there's a bug in here somewhere */
#define CHUNKY_RESTORE 1
#ifdef CHUNKY_RESTORE
/*
This version of restore_screen works in chunks across the screen,
instead of only with whole lines. This can be MUCH faster in some
cases, like when running across a 2400 baud modem :-)
Andrew Tridgell
*/
void 
restore_screen(void)
{
  us *sadr, *p; 
  unsigned char c, a;
  int x, y, oa;
  int Xchunk = CO/config.redraw_chunks;
  int Xstart;

  update_screen=0;
  CHFLUSH;
  
  v_printf("RESTORE SCREEN: scrbuf at 0x%08x\n", scrbuf);
  
  if (config.console_video) 
    {
      v_printf("restore cancelled for console_video\n");
      return;
    }

  sadr = SCREEN_ADR(SCREEN);
  oa = 7; 
  p = sadr;
  for (y=0; y<LI; y++) 
    for (Xstart=0; Xstart<CO; Xstart += Xchunk)
      {
	int chunk = ((Xstart+Xchunk) > CO) ? (CO-Xstart) : Xchunk;
	
	/* only update if this chunk of line has changed
	 * ..note that sadr is an unsigned
	 * short ptr, so CO is not multiplied by 2...I'll clean this up
	 * later.
	 */	      
	if (! memcmp(scrbuf + y*CO*2 + Xstart*2, sadr + y*CO + Xstart, 
		     chunk*2) )
	  {
	    p += chunk;  /* p is an unsigned short pointer */
	    continue; /* go to the next chunk */
	  }
	else 
	  memcpy(scrbuf + y*CO*2 + Xstart*2, p, chunk*2); 

	/* This chunk must have changed - we'll have to redraw it */

	/* go to the start of the chunk */
	dostputs(tgoto(cm, Xstart, y), 1, outcbuf);

	/* do chars one at a time */
	for (x=Xstart; x<Xstart+chunk; x++) 
	  {
	    c = *(unsigned char *)p;
	    if ((a = ((unsigned char *)p)[1]) != oa) 
	      {
		/* do fore/back-ground colors */      
		if (!(a & 7) || (a & 0x70)) 
		  dostputs(mr, 1, outcbuf);
		else 
		  dostputs(me, 1, outcbuf);
		
		/* do high intensity */
		if (a & 0x8) 
		  dostputs(md, 1, outcbuf);
		else if (oa & 0x8)
		  {
		    dostputs(me, 1, outcbuf);
		    if (!(a & 7) || (a & 0x70))
		      dostputs(mr, 1, outcbuf);
		  }
		
		/* do underline/blink */
		if (a & 0x80) 
		  dostputs(so, 1, outcbuf);
		else if (oa & 0x80) 
		  dostputs(se, 1, outcbuf);
		
		oa = a;   /* save old attr as current */
	      }
	    CHOUT(trans[c] ? trans[c] : '_');
	    p++;
	  }
      }

  dostputs(me, 1, outcbuf);
  CHFLUSH;
  poscur(XPOS(SCREEN),YPOS(SCREEN));
}

#else

void 
restore_screen(void)
{
  us *sadr, *p; 
  unsigned char c, a;
  int x, y, oa;
  
  update_screen=0;
  
  v_printf("RESTORE SCREEN: scrbuf at 0x%08x\n", scrbuf);
  
  if (config.console_video) {
    v_printf("restore cancelled for console_video\n");
    return;
  }
  
  sadr = SCREEN_ADR(SCREEN);
  oa = 7; 
  p = sadr;
  for (y=0; y<LI; y++) 
    {
      dostputs(tgoto(cm, 0, y), 1, outcbuf);
      for (x=0; x<CO; x++) {
	c = *(unsigned char *)p;
	if ((a = ((unsigned char *)p)[1]) != oa) {
#ifndef OLD_RESTORE
	  /* do fore/back-ground colors */      
	  if (!(a & 7) || (a & 0x70)) dostputs(mr, 1, outcbuf);
	  else dostputs(me, 1, outcbuf);
	  
	  /* do high intensity */
	  if (a & 0x8) dostputs(md, 1, outcbuf);
	  else if (oa & 0x8)
	    {
	      dostputs(me, 1, outcbuf);
	      if (!(a & 7) || (a & 0x70))
		dostputs(mr, 1, outcbuf);
	    }
	  
	  /* do underline/blink */
	  if (a & 0x80) dostputs(so, 1, outcbuf);
	  else if (oa & 0x80) dostputs(se, 1, outcbuf);
	  
	  oa = a;   /* save old attr as current */
#else
	  if ((a & 7) == 0) dostputs(mr, 1, outcbuf);
	  else dostputs(me, 1, outcbuf);
	  if ((a & 0x88)) dostputs(md, 1, outcbuf);
	  oa = a;
#endif
	}
	CHOUT(trans[c] ? trans[c] : '_');
	p++;
      }
    }
  dostputs(me, 1, outcbuf);
  CHFLUSH;
  poscur(XPOS(SCREEN),YPOS(SCREEN));
}
#endif CHUNKY_RESTORE


int 
inb(int port)
{
/* for scanseq */
#define NEWCODE      1
#define BREAKCODE    2


	static unsigned int cga_r=0;
	static unsigned int tmp=0;

	port &= 0xffff;
#if AJT
	if (port_readable(port))
	  return(read_port(port) & 0xFF);
#endif

	/* graphic status - many programs will use this port to sync with
	 * the vert & horz retrace so as not to cause CGA snow */
	if ((port == 0x3da) || (port == 0x3ba))
	  return (cga_r ^= 1) ? 0xcf : 0xc6;
	else
	  switch (port)
	    {
	    case 0x60:
/* #define new8042 */
#ifndef new8042
	      k_printf("direct 8042 read1: 0x%02x\n", lastscan);
	      tmp=lastscan;
	      lastscan=0;
	      return tmp;
#else
  /* this code can't send non-ascii scancodes like alt/ctrl/scrollock,
     nor keyups, etc. */
	      if (new_scanseq == BREAKCODE) {
		k_printf("KBD: doing keyup for 0x%02x->0x%02x\n", 
			 new_lastscan, new_lastscan|0x80);
		new_scanseq=NEWCODE;
		return new_lastscan|0x80;
	      }

	      if (!CReadKeyboard(&tmpkeycode, NOWAIT)) 
		{
		  k_printf("failed direct 8042 read\n");
		  return 0;
		}
	      else {
		new_lastscan=tmpkeycode >> 8;
		new_scanseq=BREAKCODE;
		k_printf("KBD: direct 8042 read: 0x%02x\n", new_lastscan);
		return new_lastscan;
	      }
	      break;
#endif
	    case 0x64:
	      tmp=0x10 | (lastscan ? 1 : 0);  /* low bit set = sc ready */
	      /* lastscan=0; */
	      k_printf("direct 8042 status check: 0x%02x\n", tmp);
	      return tmp;

	    case 0x61:
	      i_printf("inb [0x61] =  0x%x (8255 chip)\n", port61);
	      return port61;

	    case 0x70:
	    case 0x71:
	      return cmos_read(port);

	    case 0x40:
	      i_printf("inb [0x41] = 0x%x  1st timer inb\n",
			  pit.CNTR0-=20);
	      return pit.CNTR0;
	    case 0x41:
	      i_printf("inb [0x41] = 0x%x  2nd timer inb\n",
			  pit.CNTR1--);
	      return pit.CNTR1;
	    case 0x42:
	      i_printf("inb [0x42] = 0x%x  3rd timer inb\n",
			  pit.CNTR2--);
	      return pit.CNTR2;
	    case 0x3bc:
	      i_printf("printer port inb [0x3bc] = 0\n");
	      return 0;
	    case 0x3db:  /* light pen strobe reset */
	      return 0;
	    default: {
	      int num;

	      if ( (num=is_serial_io(port)) != -1 )
		return( do_serial_in(num, port) );
	      i_printf("inb [0x%x] = 0x%x\n", port, _regs.eax);
	      return 0;
	    }
	    }
	return 0;
}


void 
outb(int port, int byte)
{
  static int timer_beep=0;
  static int lastport=0;
  int num;

  port &= 0xffff;
  byte &= 0xff;

#if AJT
  if (port_writeable(port))
    {
      write_port(byte,port);
      return;
    }
#endif


  if ((port == 0x60) || (port == 0x64)) i_printf("keyboard outb\n");
  else if (port == 0x61) 
    {
      port61=byte;
      i_printf("8255 outb\n");
      if (((byte & 3) == 3) && (timer_beep == 1) && 
	  (config.speaker == SPKR_EMULATED))
	{
	  i_printf("beep!\n");
	  fprintf(stderr,"\007");
	  timer_beep=0;
	} 
      else timer_beep=1;
    }

  else if ((port == 0x70) || (port == 0x71))
    cmos_write(port, byte);

  else if (port >= 0x40 && port <= 0x43)
    {
      i_printf("timer outb 0x%02x\n", byte);
      if ((port == 0x42) && (lastport == 0x42))
	{
	  if ((timer_beep == 1) && 
	      (config.speaker == SPKR_EMULATED))
	    {
	      fprintf(stderr,"\007");
	      timer_beep=0;
	    }
	  else timer_beep=1;
	}
    }

  else if ( (num=is_serial_io(port)) != -1)
    do_serial_out(num, port, byte);


  else i_printf("outb [0x%x] 0x%x\n", port, byte);

  lastport = port;
}


void 
config_init(void)
{
  int b; 

  configuration=0;

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, and the
   * configured number of floppy disks
   */
  CONF_NFLOP(configuration, config.fdisks);
  CONF_NSER(configuration, config.num_ser);
  CONF_NLPT(configuration, config.num_lpt);
  if (config.mouse_flag) configuration |= CONF_MOUSE;  /* XXX - is this PS/2 only? */

  if (config.mathco) configuration |= CONF_MATHCO;
  configuration |= (CONF_SCRMODE);   /* XXX -  CONF_DMA - ?? DMA? */

  g_printf("CONFIG: 0x%04x    binary: ", configuration);
  for (b=15; b >= 0; b--)
      dbug_printf( "%s%s", (configuration & (1<<b)) ? "1" : "0",
		           (b % 4)                  ? ""  : " ");

  dbug_printf("\n");
}

void dos_ctrl_alt_del(void)
{
  dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
  disk_close();
  clear_screen(SCREEN,7);
  show_cursor();
  special_nowait=0;
  p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
  boot(config.hdiskboot? hdisktab : disktab);
}

void dos_ctrlc(void)
{
  k_printf("DOS ctrl-c!\n");
  p_dos_str("^C\n\r");   /* print ctrl-c message */
  keybuf_clear();

  do_soft_int(0x23);
}


void 
dosemu_banner(void)
{
  u_short *ssp = SEG_ADR((u_short *), ss, sp);

  *--ssp = _regs.cs;
  *--ssp = _regs.eip;
  _regs.esp -= 4;
  _regs.cs = Banner_SEG;
  _regs.eip = Banner_OFF;
}


void boot(struct disk *dp)
{
  char *buffer;
  unsigned int i;
  unsigned char *ptr;
  
  /* I took this out because I didn't seem to need it anymore.  if you
   * have any problems booting, you might want to put it back in
   */

  config_init();
  cpu_init();
  cmos_init();

  ignore_segv++;
  
  disk_close();
  disk_open(dp);
  buffer = (char *)0x7c00;

  /* fill the last page w/HLT, except leave the BIOS date & machine
   * type there if BIOS mapped in... */
  if (! config.mapped_sbios) {
    memset((char *)0xffff0, 0xF4, 16); 
    
    strncpy((char *)0xffff5, "02/25/93", 8);  /* set our BIOS date */
    *(char *)0xffffe = 0xfc;                  /* model byte = IBM AT */
  }

  /* init trapped interrupts called via jump */
  for (i=0; i<256; i++) {
    if ((i & 0xf8) == 0x60) continue; /* user interrupts */
    SETIVEC(i, BIOSSEG, 16*i);
    ptr=(unsigned char *)(BIOSSEG*16 + 16*i);
    *ptr++=0xcd;     /* 0xcd = INT */
    *ptr++=i;
    *ptr++=0xcf;     /* 0xcf = IRET */
  }
  
  SETIVEC(0x1c,BIOSSEG,2);  /* user timer tick, should be an IRET */

  /* XMS has it's handler just after the interrupt dummy segment */
  ptr=(unsigned char *)(XMSControl_ADD);
  *ptr++ = 0xeb;       /* jmp short forward 3 */
  *ptr++ = 3;
  *ptr++ = 0x90;       /* nop */
  *ptr++ = 0x90;       /* nop */
  *ptr++ = 0x90;       /* nop */
  *ptr++ = 0xf4;       /* HLT...the current emulator trap */
  *ptr++ = INT2F_XMS_MAGIC;  /* just an info byte. reserved for later */
  *ptr++ = 0xcb;       /* FAR RET */
/*  xms_init(); */

  /* show EMS as disabled */
  SETIVEC(0x67, 0, 0);

  /* this is the mouse handler */
  if (config.mouse_flag)
    {
      u_short *seg, *off;
      ptr=(unsigned char *)(Mouse_ADD);

      /* mouse routine simulates the stack frame of an int, then does a
       * "pushad" before here...so we just "popad; iret" to get back out
       */
      *ptr++=0xff;
      *ptr++=0x1e;
      *( ((us *)ptr)++ )=Mouse_OFF + 7;  /* uses ptr[3] as well */

      *ptr++=0x61;       /* popa */
      *ptr++=0xcf;       /* iret */

      *ptr++=0x27;       /* placeholder(offset) */
      *ptr++=0x02;       /* placeholder */
      off = (u_short *)(ptr - 2);
      *ptr++=0x81;       /* placeholder(segment) */
      *ptr++=0x1c;      /* placeholder */
      seg = (u_short *)(ptr - 2);
      
      /* tell the mouse driver where we are...exec add, seg, offset */
      mouse_sethandler(ptr, seg, off);
    }
#if 0
  else SETIVEC(0x33,BIOSSEG,2);  /* point to IRET */
#endif

  ptr = (u_char *)Banner_ADD;
  *ptr++ = 0xb0;  	/* mov al, 5 */
  *ptr++ = 0x05;
  *ptr++ = 0xcd;  	/* int 0xe5 */
  *ptr++ = 0xe5;
  *ptr++ = 0xcb;  	/* far ret */

#ifdef REAL_INT16
  ptr = (u_char *)INT16_ADD;
	*ptr++=0xFB;
	*ptr++=0x1E;
	*ptr++=0x53;
	*ptr++=0xBB;
	*ptr++=0x40;
	*ptr++=0x00;
	*ptr++=0x8E;
	*ptr++=0xDB;
	*ptr++=0x0A;
	*ptr++=0xE4;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x00;
	*ptr++=0x74;
	*ptr++=0x2B;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x10;
	*ptr++=0x74;
	*ptr++=0x26;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x01;
	*ptr++=0x74;
	*ptr++=0x47;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x11;
	*ptr++=0x74;
	*ptr++=0x42;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x02;
	*ptr++=0x74;
	*ptr++=0x4E;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x03;
	*ptr++=0x74;
	*ptr++=0x0F;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x04;
	*ptr++=0x74;
	*ptr++=0x0A;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x05;
	*ptr++=0x74;
	*ptr++=0x48;
	*ptr++=0x80;
	*ptr++=0xFC;
	*ptr++=0x12;
	*ptr++=0x74;
	*ptr++=0x3A;
	*ptr++=0x5B;
	*ptr++=0x1F;
	*ptr++=0xCF;
	*ptr++=0xFA;
	*ptr++=0x8B;
	*ptr++=0x1E;
	*ptr++=0x1A;
	*ptr++=0x00;
	*ptr++=0x3B;
	*ptr++=0x1E;
	*ptr++=0x1C;
	*ptr++=0x00;
	*ptr++=0x75;
	*ptr++=0x03;
	*ptr++=0xFB;
	*ptr++=0xEB;
	*ptr++=0xF2;
	*ptr++=0x8B;
	*ptr++=0x07;
	*ptr++=0x43;
	*ptr++=0x43;
	*ptr++=0x89;
	*ptr++=0x1E;
	*ptr++=0x1A;
	*ptr++=0x00;
	*ptr++=0x3B;
	*ptr++=0x1E;
	*ptr++=0x82;
	*ptr++=0x00;
	*ptr++=0x75;
	*ptr++=0xE1;
	*ptr++=0x8B;
	*ptr++=0x1E;
	*ptr++=0x80;
	*ptr++=0x00;
	*ptr++=0x89;
	*ptr++=0x1E;
	*ptr++=0x1A;
	*ptr++=0x00;
	*ptr++=0xEB;
	*ptr++=0xD7;
	*ptr++=0xFA;
	*ptr++=0x8B;
	*ptr++=0x1E;
	*ptr++=0x1A;
	*ptr++=0x00;
	*ptr++=0x3B;
	*ptr++=0x1E;
	*ptr++=0x1C;
	*ptr++=0x00;
	*ptr++=0x8B;
	*ptr++=0x07;
	*ptr++=0xFB;
	*ptr++=0x5B;
	*ptr++=0x1F;
	*ptr++=0xCA;
	*ptr++=0x02;
	*ptr++=0x00;
	*ptr++=0xA0;
	*ptr++=0x17;
	*ptr++=0x00;
	*ptr++=0x8A;
	*ptr++=0x26;
	*ptr++=0x18;
	*ptr++=0x00;
	*ptr++=0xEB;
	*ptr++=0xBD;
	*ptr++=0xFA;
	*ptr++=0x8B;
	*ptr++=0x1E;
	*ptr++=0x1C;
	*ptr++=0x00;
	*ptr++=0x43;
	*ptr++=0x43;
	*ptr++=0x3B;
	*ptr++=0x1E;
	*ptr++=0x82;
	*ptr++=0x00;
	*ptr++=0x75;
	*ptr++=0x04;
	*ptr++=0x8B;
	*ptr++=0x1E;
	*ptr++=0x80;
	*ptr++=0x00;
	*ptr++=0x3B;
	*ptr++=0x1E;
	*ptr++=0x1A;
	*ptr++=0x00;
	*ptr++=0x75;
	*ptr++=0x05;
	*ptr++=0xB0;
	*ptr++=0x01;
	*ptr++=0xFB;
	*ptr++=0xEB;
	*ptr++=0xA1;
	*ptr++=0x53;
	*ptr++=0x8B;
	*ptr++=0x1E;
	*ptr++=0x1C;
	*ptr++=0x00;
	*ptr++=0x89;
	*ptr++=0x0F;
	*ptr++=0xB0;
	*ptr++=0x00;
	*ptr++=0x5B;
	*ptr++=0x89;
	*ptr++=0x1E;
	*ptr++=0x1C;
	*ptr++=0x00;
	*ptr++=0xFB;
	*ptr++=0xEB;
	*ptr++=0x90;
    SETIVEC(0x16, BIOSSEG, INT16_OFF); 
/* End of REAL MODE interrupt 16 */
#endif

  /* set up BIOS exit routine (we have *just* enough room for this) */
  ptr = (u_char *)0xffff0;
  *ptr++ = 0xb8;	/* mov ax, 0xffff */
  *ptr++ = 0xff;
  *ptr++ = 0xff;
  *ptr++ = 0xcd;	/* int 0xe5 */
  *ptr++ = 0xe5;

  /* set up relocated video handler (interrupt 0x42) */
  *(u_char *)0xff065 = 0xcf;  /* IRET */


  *(us *)0x410 = configuration;
  *(us *)0x413 = MEM_SIZE;	/* size of memory */
  VIDMODE = screen_mode;	/* screen mode */
  COLS = CO;			/* chars per line */
  ROWSM1 = LI-1;    		/* lines on screen - 1 */
  REGEN_SIZE = TEXT_SIZE;  	/* XXX - size of video regen area in bytes */
  PAGE_OFFSET = 0;       	/* offset of current page in buffer */

  /* The 16-word BIOS key buffer starts at 0x41e */
  *(us *)0x41a = 0x1e;		/* key buf start ofs */
  *(us *)0x41c = 0x1e;		/* key buf end ofs */
  *(us *)0x480 = 0x1e;		/* keyboard queue start... */
  *(us *)0x482 = 0x3e;		/* ...and end offsets from 0x400 */
  
  keybuf_clear();
  
  *(us *)0x463 = 0x3d4;           /* base port of CRTC - IMPORTANT! */
  *(char *)0x465 = 9;		  /* current 3x8 (x=b or d) value */
  *(char *)0x487 = 0x61;
  *(char *)0x488 = 0x81;          /* video display data */
  *(char *)0x48a = VID_COMBO;     /* video type */

  *(char *)0x496 = 16;		  /* 102-key keyboard */
  *(long *)0x4a8 = 0;             /* pointer to video table */

  
  /* from Alan Cox's mods */
  /* we need somewhere for the bios equipment. Put it at d0000 */
  /* XXX - this fails bad with XMS; put into BIOS area... */
  *(char *)0xd0000=0x09;
  *(char *)0xd0001=0x00;	/* 9 byte table */
  *(char *)0xd0002=0xFC;        /* PC AT */
  *(char *)0xd0003=0x01;
  *(char *)0xd0004=0x04;	/* bios revision 4 */
  *(char *)0xd0005=0x20;	/* no mca no ebios no wat no keybint
				   rtc no slave 8259 no dma 3 */
  *(char *)0xd0006=0x00;
  *(char *)0xd0007=0x00;
  *(char *)0xd0008=0x00;
  *(char *)0xd0009=0x00;

  if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
    error("ERROR: can't boot from %s, using harddisk\n", dp->dev_name);
    dp = hdisktab;
    if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
      error("ERROR: can't boot from hard disk\n");
      leavedos(1);
    }
  }
  disk_close();

  _regs.eax = _regs.ebx = _regs.edx = 0;
  _regs.ecx = 0;
  _regs.ebp = _regs.esi = _regs.edi;
  _regs.esp = 0xfff0;  /* give it some stack..is this legal? */
  _regs.cs = _regs.ss = _regs.ds = _regs.es = _regs.fs = _regs.gs = 0x7c0;
  _regs.eip = 0;
  /* _regs.eflags = 0; */
  update_flags(&_regs.eflags);

  /* the banner helper actually get called *after* the VGA card
   * is initialized (if it is) because we set up a return chain:
   *      init_vga_card -> dosemu_banner -> 7c00:0000 (boot block)
   */

  dosemu_banner();

  if (config.vga) {
    g_printf("INITIALIZING VGA CARD BIOS!\n");
    init_vga_card();
  }

  if (exitearly)
    {
      dbug_printf("Leaving DOS before booting\n");
      leavedos(0);
    }
  ignore_segv--;
}


void 
int15(void)
{
  struct timeval wait_time;
  int num;
  NOCARRY;

  switch(HI(ax)) 
    {
    case 0x41: /* wait on external event */
      break;
    case 0x4f:   /* Keyboard intercept */
      HI(ax) = 0x86;
      NOCARRY;  /* original scancode (no translation) */
      break;
    case 0x80:	/* default BIOS device open event */
      _regs.eax&=0x00FF;
      return;
    case 0x81:
      _regs.eax&=0x00FF;
      return;
    case 0x82:
      _regs.eax&=0x00FF;
      return;
    case 0x83:
      h_printf("int 15h event wait:\n");
      show_regs();
      CARRY;
      return;	/* no event wait */
    case 0x84:
      CARRY;
      return;	/* no joystick */
    case 0x85:
      num=_regs.eax&0xFF;	/* default bios handler for sysreq key */
      if(num==0||num==1)
	{
	  _regs.eax&=0x00FF;
	  return;
	}
      _regs.eax&=0xFF00;
      _regs.eax|=1;
      CARRY;
      return;
    case 0x86:
      /* wait...cx:dx=time in usecs */
      g_printf("doing int15 wait...ah=0x86\n");
      show_regs();
      wait_time.tv_sec = 0;
      wait_time.tv_usec = (LWORD(ecx) << 16) | LWORD(edx);
      RPT_SYSCALL( select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv) );
      NOCARRY;
      return;
      
    case 0x87:
      if (config.xms_size) xms_int15();
      else
	{
	  _regs.eax&=0xFF;
	  _regs.eax|=0x0300;	/* say A20 gate failed - a lie but enough */
	  CARRY;
	}
      return;
      
    case 0x88:
      if (config.xms_size) xms_int15();
      else
	{
	  _regs.eax &= ~0xffff;   /* we don't have extended ram if it's not XMS */
	  NOCARRY;
	}
    return;
      
    case 0x89:			/* enter protected mode : kind of tricky! */
      _regs.eax|=0xFF00;	/* failed */
      CARRY;
      return;
    case 0x90:			/* no device post/wait stuff */
      CARRY;
      return;
    case 0x91:
      CARRY;
      return;
    case 0xc0:
      _regs.es=0xf000;
      LWORD(ebx)=0xe6f5;
      return;
    case 0xc1:
      CARRY;
      return;			/* no ebios area */
    case 0xc2:
      _regs.eax&=0x00FF;
      _regs.eax|=0x0300;	/* interface error if use a ps2 mouse */
      CARRY;
      return;
    case 0xc3:
      /* no watchdog */
      CARRY;
      return;
    case 0xc4:
      /* no post */
      CARRY;
      return;
    default:
      g_printf("int 15h error: ax=0x%04x\n", LWORD(eax));
      CARRY;
      return;
    }
}



/* the famous interrupt 0x2f
 *     returns 1 if handled by dosemu, else 0 
 *
 * note that it switches upon both AH and AX
 */
inline int 
int2f(void)
{
  /* this is the DOS give up time slice call...*/
  if ( LWORD(eax) == INT2F_IDLE_MAGIC )
    {
      usleep(INT2F_IDLE_USECS);
      return 1;
    }

  /* is it a redirector call ? */
  else if (HI(ax) == 0x11 && mfs_redirector())
    return 1;

  else if ( HI(ax) == INT2F_XMS_MAGIC )
    {
      if (!config.xms_size) return 0;
      switch(LO(ax))
	{
	case 0:  /* check for XMS */
	  x_printf("Check for XMS\n");
	  LO(ax)=0x80;
	  break;
	case 0x10:
	  x_printf("Get XMSControl address\n");
	  _regs.es = XMSControl_SEG;
	  LWORD(ebx) = XMSControl_OFF;
	  break;
	default:
	  x_printf("BAD int 0x2f XMS function\n");
	}
      return 1;
    }

  return 0;
}


void 
int16(void)
{
  unsigned int key;
  
  static struct timeval tp1;
  static struct timeval tp2;
  static int time_count=0;
  static u_char hiax;
  key=0;
  if (in16) HI(ax) = hiax;
  in16=0;

  if (time_count==0)
    gettimeofday(&tp1,NULL);
  else
    {
      gettimeofday(&tp2,NULL);
      if ((tp2.tv_sec-tp1.tv_sec)*1000000 +
	  ((int)tp2.tv_usec-(int)tp1.tv_usec) < config.hogthreshold)
	usleep(100);
    }
  time_count = (time_count+1)%10;

  switch( HI(ax) ) 
    {
    case 0x10: 		/* read ext. key code */
    case 0:		/* read key code, wait */
      if (special_nowait)
	{
	  /* k_printf("doing special_nowait\n"); */
	  if (CReadKeyboard(&key, NOWAIT) == 1)
	    {
	      LWORD(eax)=key;
	      _regs.eflags &= ~(ZF|CF);
	    } else {
	      LWORD(eax)=0;
	      _regs.eflags |= ZF | CF;
	    }
	  return;
	}

    for (;;) {
	if (CReadKeyboard(&key, NOWAIT)) {
      		if ((key & 0xff) == 3) k_printf("CTRL-C!\n");
      	        LWORD(eax) = key;
 		return;
	}
        if (int_queue_start != int_queue_end) {
          hiax=HI(ax);
          in16=1;
	  LWORD(eax)=0;
	  _regs.eip -= 2;
	  return;
	}

      }
      break;

    case 0x11:		/* check for ext. key press */
    case 1:		/* test key code */
      if ( CReadKeyboard(&key, TEST) ) 
	{
	  _regs.eflags &= ~(ZF | CF); /* key pressed */
	  LWORD(eax) = key;
	} 
      else 
	{
	  _regs.eflags |= ZF | CF; /* no key */
	  LWORD(eax) = 0;
	}
      break;

    case 2: /* read key state */
      if (config.console_keyb) LO(ax)=kbd_flags & 0xff;
      else _regs.eax &= ~0xff; 
      break;

    case 3: /* set typematic rate -- not supported */
      return;

    case 5:	/* insert key code */
      k_printf("ins ext key 0x%04x\n", LWORD(eax));
      _regs.eax &= ~0xff;
      _regs.eax |= ((InsKeyboard((unsigned short)
				 LWORD(ecx))) ? 0 : 1);
      break;

    case 0x12: /* get extended shift states */
      if (config.console_keyb) LWORD(eax)=kbd_flags;
      else _regs.eax &= ~0xff; 
      return;


    case 0x55: /* MS word coop w/TSR? */
      special_nowait ^= 1;
      if (d.keyb)
	{
	  dbug_printf("weird int16 ax=%04x call: %d!\n",
		      LWORD(eax),special_nowait);
	  show_regs();
	  show_ints(0,0x16);
	}
      LWORD(eax)=0;  /* tell Microsoft Word that keyboard is not
		      * being handled */

      return;

    default:
      error("ERROR: keyboard (int 16h, ah=0x%x)\n", HI(ax));
      show_regs();
      /* fatalerr = 7; */
      return;
    }
}


void 
int1a(void)
{
	unsigned long ticks;
	long akt_time, elapsed;
	struct timeval tp;
	struct timezone tzp;
	struct tm *tm;


	switch(HI(ax)) {
		case 0: /* read time counter */
			time(&akt_time);
			elapsed = akt_time - start_time;
			ticks = (elapsed *182) / 10 + last_ticks;
			LWORD(eax) &= ~0xff; /* 0 hours */
			LWORD(ecx) = ticks >> 16;
			LWORD(edx) = ticks & 0xffff;
			/* dbug_printf("read timer st: %ud %ud t=%d\n",
				    start_time, ticks, akt_time); */
			break;
		case 1: /* write time counter */
			last_ticks=(LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);
			set_ticks(last_ticks);
			time(&start_time);
			g_printf("set timer to %ud \n", last_ticks);
			break;
		case 2: /* get time */
			gettimeofday(&tp, &tzp);
			ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
			tm = localtime((time_t *)&ticks);
        		/* g_printf("get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec); */
			HI(cx) = tm->tm_hour % 10;
			tm->tm_hour /= 10;
			HI(cx) |= tm->tm_hour << 4;
			LO(cx) = tm->tm_min % 10;
			tm->tm_min /= 10;
			LO(cx) |= tm->tm_min << 4;
			HI(dx) = tm->tm_sec % 10;
			tm->tm_sec /= 10;
			HI(dx) |= tm->tm_sec << 4;
			/* LO(dx) = tm->tm_isdst; */
			_regs.eflags &= ~CF;
			break;
		case 4: /* get date */
			gettimeofday(&tp, &tzp);
			ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
			tm = localtime((time_t *)&ticks);
			tm->tm_year += 1900;
			tm->tm_mon ++;
        		/* g_printf("get date %d.%d.%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year); */
			LWORD(ecx) = tm->tm_year % 10;
			tm->tm_year /= 10;
			LWORD(ecx) |= (tm->tm_year % 10) << 4;
			tm->tm_year /= 10;
		        LWORD(ecx) |= (tm->tm_year % 10) << 8;
			tm->tm_year /= 10;
			LWORD(ecx) |= (tm->tm_year) << 12;
			LO(dx) = tm->tm_mday % 10;
			tm->tm_mday /= 10;
			LO(dx) |= tm->tm_mday << 4;
			HI(dx) = tm->tm_mon % 10;
			tm->tm_mon /= 10;
			HI(dx) |= tm->tm_mon << 4;
			_regs.eflags &= ~CF;
			break;
		case 3: /* set time */
		case 5: /* set date */
			error("ERROR: timer: can't set time/date\n");
			break;
		default:
			error("ERROR: timer error AX=0x%04x\n", LWORD(eax));
			/* show_regs(); */
			/* fatalerr = 9; */
			return;
	}
}

/* note that the emulation herein may cause problems with programs
 * that like to take control of certain int 21h functions, or that
 * change functions that the true int 21h functions use.  An example
 * of the latter is ANSI.SYS, which changes int 10h, and int 21h
 * uses int 10h.  for the moment, ANSI.SYS won't work anyway, so it's
 * no problem.
 */
int ms_dos(int nr) /* returns 1 if emulated, 0 if internal handling */
{
  unsigned int c;

/* int 21, ah=1,7,or 8:  return 0 for an extended keystroke, but the
   next call returns the scancode */
/* if I press and hold a key like page down, it causes the emulator to
   exit! */


Restart: 
	/* dbug_printf("DOSINT 0x%x\n", nr); */
	/* emulate keyboard io to avoid DOS' busy wait */
	switch(nr) {

/* define NO_FIX_STDOUT if you want faster dos screen output, at the 
 * expense of being able to redirect things.  I don't seriously consider
 * leaving this in for real, but I just don't feel like taking it out
 * yet...I'd forget it if I did.
 */
#if DOS_FLUSH
	case 7: /* read char, do not check <ctrl> C */
	case 1: /* read and echo char */
	case 8: /* read char */
	  /* k_printf("KBD/DOS: int 21h ah=%d\n", nr); */
	  disk_close();   /* DISK */
	  return 0;  /* say not emulated */
	  break;

	case 10: /* read string */
	  disk_close(); /* DISK */
	  return 0;
	  break;
#endif

	case 12: /* clear key buffer, do int AL */
	  while (CReadKeyboard(&c, NOWAIT) == 1);
	  nr = LO(ax);
	  if (nr == 0) break; /* thanx to R Michael McMahon for the hint */
	  HI(ax) = LO(ax);
	  NOCARRY;
	  goto Restart;

#define DOS_HANDLE_OPEN		0x3d
#define DOS_HANDLE_CLOSE	0x3e
#define DOS_IOCTL		0x44
#define IOCTL_GET_DEV_INFO	0
#define IOCTL_CHECK_OUTPUT_STS	7

/* XXX - MAJOR HACK!!! this is bad bad wrong.  But it'll probably work, unless
 * someone puts "files=200" in his/her config.sys
 */
#define EMM_FILE_HANDLE 200  

#define FALSE 0
#define TRUE 1

/* lowercase and truncate to 3 letters the replacement extension */
#define ext_fix(s) { char *r=(s); \
		     while (*r) { *r=toupper(*r); r++; } \
		     if ((r - s) > 3) s[3]=0; }

	/* we trap this for two functions: simulating the EMMXXXX0
	 * device, and fudging the CONFIG.XXX and AUTOEXEC.XXX
	 * bootup files
	 */
	case DOS_HANDLE_OPEN: {
	  char * ptr = SEG_ADR((char *),ds,dx);

#ifdef INTERNAL_EMS 
	  if ( !strncmp(ptr, "EMMXXXX0", 8) && config.ems_size) {
	    E_printf("EMS: opened EMM file!\n");
	    LWORD(eax) = EMM_FILE_HANDLE;
	    NOCARRY;
	    show_regs();
	    return(1);
	  } 
	  else
#endif
	    if ( !strncmp(ptr, "\\CONFIG.SYS", 11) && config.emusys) {
	    ext_fix(config.emusys);
	    sprintf(ptr, "\\CONFIG.%-3s", config.emusys);
	    d_printf("DISK: Substituted %s for CONFIG.SYS\n", ptr);
	  }
	  /* ignore explicitly selected drive by incrementing ptr by 1 */
	  else if ( !strncmp(ptr+1, ":\\AUTOEXEC.BAT", 14) && config.emubat) {
	    ext_fix(config.emubat);
	    sprintf(ptr+1, ":\\AUTOEXEC.%-3s", config.emubat);
	    d_printf("DISK: Substituted %s for AUTOEXEC.BAT\n", ptr+1);
	  }

	  return(0);
	}

#ifdef INTERNAL_EMS
	case DOS_HANDLE_CLOSE: 
	  if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
	    return(0);
	  else {
	    E_printf("EMS: closed EMM file!\n");
	    NOCARRY;
	    show_regs();
	    return(1);
	  }

	case DOS_IOCTL: 
	  {
	    
	    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
	      return(FALSE);
	    
	    switch(LO(ax)) {
	    case IOCTL_GET_DEV_INFO:
	      E_printf("EMS: dos_ioctl getdevinfo emm.\n");
	      LWORD(edx) = 0x80;
	      NOCARRY;
	      show_regs();
	      return(TRUE);
	      break;
	    case IOCTL_CHECK_OUTPUT_STS:
	      E_printf("EMS: dos_ioctl chkoutsts emm.\n");
	      LO(ax) = 0xff;
	      NOCARRY;
	      show_regs();
	      return(TRUE);
	      break;
	    }
	    error("ERROR: dos_ioctl shouldn't get here. XXX\n");
	    return(FALSE);
	  }
#endif	  

	default:
	  /* taking this if/printf out will speed things up a bit...*/
	  if (d.dos)
	    dbug_printf(" dos interrupt 0x%x, ax=0x%x, bx=0x%x\n",
			nr, LWORD(eax), LWORD(ebx)); 
	  return 0;
	}
	return 1;
}


inline int
run_int(int i)
{
  us *ssp;
  static struct timeval tp1;
  static struct timeval tp2;
  static int time_count=0;
  
#ifdef REAL_INT16
if (i == 0x16) {

  if (time_count==0)
    gettimeofday(&tp1,NULL);
  else
    {
      gettimeofday(&tp2,NULL);
      if ((tp2.tv_sec-tp1.tv_sec)*1000000 +
	  ((int)tp2.tv_usec-(int)tp1.tv_usec) < config.hogthreshold)
	usleep(100);
    }
  time_count = (time_count+1)%10;
}
#endif

  if (! (_regs.eip && _regs.cs))
    {
      error("run_int: not running NULL interrupt 0x%x handler\n",i);
      show_regs();
      return 0;
    }

  /* make sure that the pushed flags match out "virtual" flags */
  update_flags(&_regs.eflags);

  if (!_regs.esp)
    ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
  else
    ssp = SEG_ADR((us *), ss, sp);
  *--ssp = _regs.eflags;
  *--ssp = _regs.cs;
  *--ssp = _regs.eip;
  _regs.esp -= 6;
  _regs.cs =  ((us *)0)[ (i<<1) +1];
  _regs.eip = ((us *)0)[  i<<1    ];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
  _regs.eflags &= ~(TF|IF|NT);
  return 0;
}

inline int can_revector (int i)
{
/* here's sort of a guideline:
 * if we emulate it completely, but there is a good reason to stick
 * something in front of it, and it seems to work, by all means revector it.
 * if we emulate none of it, say yes, as that is a little bit faster.
 * if we emulate it, but things don't seem to work when it's revectored,
 * (like int15h and LINUX.EXE), then don't let it be revectored.
 *
 */

  switch(i)
    {
      /* some things, like 0x29, need to be unrevectored if the vectors
       * are the DOS vectors...but revectored otherwise
       */
#if MOUSE
    case 0x33: /* mouse...we're testing */
#endif
    case 0x21: /* we want it first...then we'll pass it on */
    case 0x28: /* same here */
    case 0x2a: 
    case 0x15: /* need this for LINUX.EXE...why??  */
#undef FAST_BUT_WRONG29
#ifdef FAST_BUT_WRONG29
    case 0x29: /* DOS fast char output... */
#endif
    case 0x2f: /* needed for XMS, redirector, and idle interrupt */
    case 0x67: /* EMS */
    case 0xe5:  /* for redirector and helper (was 0xfe) */
      return 0;

#if !MOUSE
    case 0x33:
#endif
#ifndef FAST_BUT_WRONG29
    case 0x29:
#endif
    case 0: 
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x25: /* absolute disk read, calls int 13h */
    case 0x26: /* absolute disk write, calls int 13h */
    case 0x1b: /* ctrl-break handler */
    case 0x1c: /* user timer tick */
    case 0x16: /* BIOS keyboard */
    case 0x17: /* BIOS printer */
    case 0x10: /* BIOS video */
    case 0x13: /* BIOS disk */
    case 0x27: /* TSR */
    case 0x20: /* exit program */
    case 0xfe: /* Turbo Debugger and others... */
      return 1;
    default:
      g_printf("revectoring 0x%02x\n", i);
      return 1;
    }
}


void do_int(int i)
{
  int highint=0;
/* printf("Do INT0x%02x\n", i); */
#if AJT
  int_start(i);
#endif
  in_interrupt++;

#ifdef REAL_INT16
if (in16) {
  printf("Returning to 0x16\n");
  int16();
  return;
}
#endif

  if ((_regs.cs&0xffff) == BIOSSEG)
    highint=1;
  else
    if (IS_REDIRECTED(i) && can_revector(i) /* && !IS_IRET(i) */)
      {
	run_int(i);
	return;
      }

  switch(i) {
      case 0x5   :
	g_printf("BOUNDS exception\n");
	goto default_handling;
	return;
      case 0x0a  :
      case 0x0b  : /* com2 interrupt */
      case 0x0c  : /* com1 interrupt */
      case 0x0d  :
      case 0x0e  :
      case 0x0f  :
      case 0x09  : /* IRQ1, keyb data ready */
	g_printf("IRQ->interrupt %x\n", i);
      case 0x08  :
	goto default_handling;
	return;
      case 0x10 : /* VIDEO */
	int10();
	return;
      case 0x11 : /* CONFIGURATION */
	LWORD(eax) = configuration;
	/* if (FDISKS > 0) LWORD(eax) |= 1 | ((FDISKS -1)<<6); */
	return;
      case 0x12 : /* MEMORY */
	LWORD(eax) = MEM_SIZE;
	return;
      case 0x13 : /* DISK IO */
	int13();
	return;
      case 0x14 : /* COM IO */
	int14();
	return;
      case 0x15 : /* Cassette */
	int15();
	return;
      case 0x16 : /* KEYBOARD */
/*
printf("Goinf IN\n");
printf("*DS:1A=0x%05x\n",*(us *)((0x40 << 4) + 0x1a));
printf("*DS:1C=0x%05x\n",*(us *)((0x40 << 4) + 0x1c));
printf("*DS:82=0x%05x\n",*(us *)((0x40 << 4) + 0x82));
printf("*DS:80=0x%05x\n",*(us *)((0x40 << 4) + 0x80));
*/
#ifdef REAL_INT16
	run_int(0x16); 
#else
	int16(); 
#endif
	return;
      case 0x17 : /* PRINTER */
	int17();
	return;
      case 0x18 : /* BASIC */
	break;
      case 0x19 : /* LOAD SYSTEM */
	if (config.hdiskboot != 2)
	  boot(config.hdiskboot? hdisktab : disktab);
	else boot(&disktab[1]);
	return;
      case 0x1a : /* CLOCK */
	int1a();
	return;
#if 0
      case 0x1b : /* BREAK */
      case 0x1c : /* TIMER */
      case 0x1d : /* SCREEN INIT */
      case 0x1e : /* DEF DISK PARMS */
      case 0x1f : /* DEF GRAPHIC */
      case 0x20 : /* EXIT */
      case 0x27 : /* TSR */
#endif

      case 0x21 : /* MS-DOS */
	if (ms_dos(HI(ax))) return;
	/* else do default handling in vm86 mode */
	goto default_handling;
	
      case 0x28 : /* KEYBOARD BUSY LOOP */
#if AJT
	{
	  static int first=1;
	  if (first && config.keybint && config.console_keyb)
	    {
	      first = 0;
	      /* revector int9, so dos doesn't play with the keybuffer */
	      k_printf("revectoring int9 away from dos\n");
	      SETIVEC(0x9,BIOSSEG,2);  /* point to IRET */
	    }
	}
#endif	  
	if (int28()) return;
	goto default_handling; 
	return;
	
      case 0x29 : /* FAST CONSOLE OUTPUT */
	char_out(*(char *)&_regs.eax, SCREEN, ADVANCE);    /* char in AL */
	return;

      case 0x2a : /* CRITICAL SECTION */
	goto default_handling;

      case 0x2f : /* Multiplex */
	if ( int2f() ) return;
        goto default_handling;

      case 0x33: /* mouse */
#if MOUSE
	warning("doing dosemu's mouse interrupt!\n");
	if (config.mouse_flag) mouse_int();
#else
	goto default_handling;
#endif
	return;
	break;

      case 0x67: /* EMS */
	goto default_handling;

      case 0xe5: /* dos helper and mfs startup (was 0xfe) */
	if (dos_helper()) return;
	return;

      default :
	if (d.defint)
	  dbug_printf("int 0x%x, ax=0x%x\n", i, LWORD(eax));
	  /* fall through */

      default_handling:

	if (highint) {
	    in_interrupt--;
	    return;
	  }

	if (! IS_REDIRECTED(i))  {
	    g_printf("DEFIVEC: int 0x%x  SG: 0x%04x  OF: 0x%04x\n",
			i, ISEG(i), IOFF(i));
	    in_interrupt--;
	    return;
	  }

 	if (IS_IRET(i)) {
	    if ((i != 0x2a) && (i != 0x28))
	      g_printf("just an iret 0x%02x\n", i);
	    in_interrupt--;
	    return; 
	  }

#ifdef OLD_DO_INT
	if (i == 0x08) run_int(0x1c);
	update_flags(&_regs.eflags);

  	if (!_regs.esp)
  	  ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
  	else
  	  ssp = SEG_ADR((us *), ss, sp);
	*--ssp = _regs.eflags;
	*--ssp = _regs.cs;
	*--ssp = _regs.eip;
	_regs.esp -= 6;
	_regs.cs =  ((us *)0)[ (i<<1) +1];
	_regs.eip = ((us *)0)[  i<<1    ];
	if ((_regs.eip == 0) && (_regs.cs == 0) && d.warning)
	  {
	    warn("NULL interrupt 0x%x handler\n",i);
	    show_regs();
	  }
	/* clear TF (trap flag, singlestep), IF (interrupt flag), and
	 * NT (nested task) bits of EFLAGS
	 */
	_regs.eflags &= ~(TF|IF|NT);
#else
	run_int(i);
#endif

	return;
  }
  error("\nERROR: int 0x%x not implemented\n", i);
  show_regs();
  fatalerr = 1;
  return;
}


void 
sigalrm(int sig)
{
    static int running=0;
    static inalrm=0;
    static int partials=0;

    in_vm86 = 0;

    if (inalrm)
      error("ERROR: Reentering SIGALRM!\n");

    inalrm=1;
    in_sighandler=1;

    if ( ((vm86s.screen_bitmap & scrtest_bitmap) || 
	  (update_screen && !config.console_video)) && !running) {
      running = 1;
      restore_screen();
      vm86s.screen_bitmap = 0;
      running = 0;
    }

    setitimer(TIMER_TIME, &itv, NULL);

    /* if (config.mouse_flag) mouse_curtick(); */

    /* update the Bios Data Area timer dword if interrupts enabled */
    if (cpu.iflag) timer_tick();

    /* this is severely broken */
    if (config.timers)
      {
	h_printf("starting timer int 8...\n");
	if (! do_hard_int(8)) h_printf("CAN'T DO TIMER INT 8...IF CLEAR\n");
      }
    else error("NOT CONFIG.TIMERS\n");

    /* this is for per-second activities */
    partials++;
    if (partials == FREQ) {
      partials=0;
      printer_tick((u_long)0);
      if (config.fastfloppy) floppy_tick();
    }

    in_sighandler=0;
    inalrm=0;
}


extern pid_t ipc_pid, parent_pid;
void sigchld(int sig)
{
  int chld_pid, returncode;

  in_vm86 = 0;

  chld_pid=wait(&returncode);

  if (chld_pid == ipc_pid)
    {
      char *exitmethod;
      int exitnum;

      if (WIFEXITED(returncode)) {
	exitmethod = "exited with return value";
	exitnum = WEXITSTATUS(returncode);
      }
      else if (WIFSIGNALED(returncode)) {
	exitmethod = "died from signal";
	exitnum = WTERMSIG(returncode);
      }
      else if (WIFSTOPPED(returncode)) {
	exitmethod = "was stopped by signal";
	exitnum = WSTOPSIG(returncode);
      }
      else {
	exitmethod = "unknown termination method";
	exitnum = returncode;
      }
      error("ERROR: main IPC process pid %d %s %d! dosemu terminating...\n",
	    chld_pid, exitmethod, exitnum);
      leavedos(1);
    }
}

void 
sigquit(int sig)
{
  in_vm86 = 0;
  in_sighandler=1;

  error("ERROR: sigquit called\n");
  show_ints(0,0x33);
  show_regs();

  ignore_segv++;
  *(unsigned char *)0x471 = 0x80;  /* ctrl-break flag */
  ignore_segv--;

  do_soft_int(0x1b);
  in_sighandler=0;
}


void 
timint(int sig)
{
  in_vm86 = 0;
  in_sighandler=1;

  warn("timint called: %04x:%04x -> %05x\n", ISEG(8), IOFF(8), IVEC(8));
  warn("(vec 0x1c)     %04x:%04x -> %05x\n", ISEG(0x1c), IOFF(0x1c),
       IVEC(0x1c));
  show_regs();

  do_hard_int(0x8);

  in_sighandler=0;
}


void
open_terminal_pipe(char *path)
{
  terminal_fd =  DOS_SYSCALL( open(path, O_RDWR) );
  if (terminal_fd == -1)
    {
      terminal_pipe=0;
      error("ERROR: open_terminal_pipe failed - cannot open %s!\n", path);
      return;
    } 
  else terminal_pipe=1;
}

/* this part is fairly flexible...you specify the debugging flags you wish
 * with -D string.  The string consists of the following characters:
 *   +   turns the following options on (initial state)
 *   -   turns the following options off
 *   a   turns all the options on/off, depending on whether +/- is set
 *   0-9 sets debug levels (0 is off, 9 is most verbose)
 *   #   where # is a letter from the valid option list (see docs), turns
 *       that option off/on depending on the +/- state.
 *
 * Any option letter can occur in any place.  Even meaningless combinations,
 * such as "01-a-1+0vk" will be parsed without error, so be careful.
 * Some options are set by default, some are clear. This is subject to my 
 * whim.  You can ensure which are set by explicitly specifying.
 */

void
parse_debugflags(const char *s)
{
  char c;
  unsigned char flag=1;
  const char allopts[]="vsdRWkpiwghxmIEc";

/* if you add new classes of debug messages, make sure to add the
 * letter to the allopts string above so that "1" and "a" can work
 * correctly.
 */

  dbug_printf("debug flags: %s\n", s);
  while ( (c=*(s++)) )
    switch (c)
      {
      case '+':	/* begin options to turn on */
	if (!flag) flag=1;
	break;
      case '-':	/* begin options to turn off */
	flag=0;
	break;
      case 'v': /* video */
	d.video=flag;
	break;
      case 's': /* serial */
	d.serial=flag;
	break;
      case 'c': /* disk */
	d.config=flag;
	break;
      case 'd': /* disk */
	d.disk=flag;
	break;
      case 'R': /* disk READ */
	d.read=flag;
	break;
      case 'W': /* disk WRITE */
	d.write=flag;
	break;
      case 'k': /* keyboard */
	d.keyb=flag;
	break;
      case 'p': /* printer */
	d.printer=flag;
	break;
      case 'i': /* i/o instructions (in/out) */
	d.io=flag;
	break;
      case 'w': /* warnings */
	d.warning=flag;
	break;
      case 'g': /* general messages */
	d.general=flag;
	break;
      case 'x': /* XMS */
	d.xms=flag;
	break;
      case 'm': /* mouse */
	d.mouse=flag;
	break;
      case 'a':	{ /* turn all on/off depending on flag */
	char *newopts=(char *)malloc(strlen(allopts)+2);
	d.all=flag;
	newopts[0]=flag ? '+' : '-';
	newopts[1]=0;
	strcat(newopts, allopts);
	parse_debugflags(newopts);	
	free(newopts);
	break;
      }
      case 'h': /* hardware */
	d.hardware=flag;
	break;
      case 'I': /* IPC */
	d.IPC=flag;
	break;
      case 'E': /* EMS */
	d.EMS=flag;
	break;
      case '0' ... '9':	/* set debug level, 0 is off, 9 is most verbose */
	flag = c - '0';
	break;
      default:
	p_dos_str("Unknown debug-msg mask: %c\n\r", c);
	dbug_printf("Unknown debug-msg mask: %c\n", c);
      }
}

/* XXX - takes a string in the form "1,2" 
 *       this functions should go away or be fixed...but I'd rather it
 *       went away
 */
void flip_disks(char *flipstr)
{
  struct disk temp;
  int i1, i2;
  
  if ((strlen(flipstr) != 3) || !isdigit(flipstr[0]) || !isdigit(flipstr[2]))
    {
      error("ERROR: flips string (%s) incorrect format\n");
      return;
    }

  /* shouldn't rely on the ASCII character set here :-) 
   * this makes disks go as 1,2,3...
   */
  i1=flipstr[0]-'1';
  i2=flipstr[2]-'1';

  g_printf("FLIPPED disks %s and %s\n", disktab[i1].dev_name, 
	   disktab[i2].dev_name);

  /* no range checking! bad Robert */
  temp=disktab[i1];
  disktab[i1]=disktab[i2];
  disktab[i2]=temp;
}

void 
config_defaults(void)
{
  config.hdiskboot = 1;   /* default hard disk boot */

  config.mem_size = 640;
  config.ems_size = 0;
  config.xms_size = 0;
  config.mathco=1;
  config.mouse_flag = 0;
  config.mapped_bios = 0;
  config.mapped_sbios = 0;
  config.vbios_file = NULL;
  config.vbios_copy = 0;
  config.console_keyb = 0;
  config.console_video = 0;
  config.fdisks = 0 /*DEF_FDISKS*/;
  config.hdisks = 0 /*DEF_HDISKS*/;
  config.exitearly = 0;
  config.redraw_chunks = 1;
  config.hogthreshold = 20000; /* in usecs */
  config.chipset = PLAINVGA;
  config.cardtype = CARD_VGA;
  config.fullrestore = 0;
  config.graphics = 0;
  config.gfxmemsize = 256;
  config.vga = 0;  /* this flags BIOS graphics */

  config.speaker = SPKR_EMULATED;

  config.update = 54945;
  config.freq = 18;  /* rough frequency */

  config.timers=1;   /* deliver timer ints */
  config.keybint=0;  /* no keyboard interrupt */

  config.num_ser = 0;
  config.num_lpt = 0;
  cpu.type=CPU_386;
  config.fastfloppy=1;

  config.emusys = (char *)NULL;
  config.emubat = (char *)NULL;
}

/*
 * Called to queue a hardware interrupt - will call "callstart"
 * just before the interrupt occurs and "callend" when it finishes
*/
void queue_hard_int(int i,void (*callstart),void (*callend))
{
cli();

int_queue[int_queue_end].interrupt = i;
int_queue[int_queue_end].callstart = callstart;
int_queue[int_queue_end].callend = callend;
int_queue_end = (int_queue_end+1)%IQUEUE_LEN;

h_printf("int_queue: (%d,%d) ",int_queue_start,int_queue_end);

i = int_queue_start;
while (i!=int_queue_end)
  {
    h_printf("%d ",int_queue[i]);
    i = (i+1)%IQUEUE_LEN;
  }
h_printf("\n");
sti();
}


/* Called by vm86() loop to handle queueing of interrupts */
void int_queue_run()
{
/*  static int int_queue_return_addr=0; */
  static int int_queue_running=0;
/*  static struct vm86_regs saved_regs; */

  int i;
  us *ssp;

  if (int_queue_running) 
    {
      int endval;

      /* check if current int is finished */
      if ((int)(SEG_ADR((us *),cs,ip)) != int_queue_head->int_queue_return_addr ){
/*	printf("Not finished!\n");
	return; */
      }
      else {
      /* it's finished - clean up */
      /* call user cleanup function */
      if (int_queue_head->int_queue_ptr->callend)
	endval = int_queue_head->int_queue_ptr->callend(int_queue_head->int_queue_ptr->interrupt);
/*      else endval = IQUEUE_END_NORMAL; */

      /* restore registers */
      _regs = int_queue_head->saved_regs;
      
      int_queue_running--;

      h_printf("int_queue: finished %x\n", int_queue_head->int_queue_return_addr);
      int_queue_temp = int_queue_head->prev;
      free(int_queue_head);
      int_queue_head = int_queue_temp;
      int_queue_head->next = NULL;
      }

/*      int_queue_start = (int_queue_start+1)%IQUEUE_LEN; */
    }

/* printf("Queue start %d, end %d, running %d\n", int_queue_start, int_queue_end, int_queue_running); */

  if (int_queue_start == int_queue_end || !(_regs.eflags & IF))
    return;

  i = int_queue[int_queue_start].interrupt;

  /* call user startup function...don't run interrupt if returns -1 */
  if (int_queue[int_queue_start].callstart)
    if (int_queue[int_queue_start].callstart(i) == -1) {
	printf("Callstart NOWORK\n");
	return;
    }

  if (int_queue_head == NULL) {
	int_queue_head = int_queue_tail = (struct int_queue_list_struct *)malloc(sizeof(struct int_queue_list_struct));
	int_queue_head->prev = NULL;
	int_queue_head->next = NULL;
	int_queue_head->int_queue_ptr = &int_queue[int_queue_start];
  }
  else {
	int_queue_temp = (struct int_queue_list_struct *)malloc(sizeof(struct int_queue_list_struct));
	int_queue_head->next = int_queue_temp;
	int_queue_temp->prev = int_queue_head;
	int_queue_head = int_queue_temp;
	int_queue_head->next = NULL;
	int_queue_head->int_queue_ptr = &int_queue[int_queue_start];
  }

  int_queue_running++;

  /* save our regs */
  int_queue_head->saved_regs = _regs;

  if (!_regs.esp)
    ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
  else
    ssp = SEG_ADR((us *), ss, sp);

  /* push an illegal instruction onto the stack */
  *--ssp = 0xffff;
  
  /* this is where we're going to return to */
  int_queue_head->int_queue_return_addr  = (int)ssp;

  *--ssp = _regs.eflags;
  *--ssp = (int_queue_head->int_queue_return_addr>>4); /* the code segment of our illegal opcode */
  *--ssp = (int_queue_head->int_queue_return_addr&0xf); /* and the instruction pointer */
  _regs.esp -= 8;
  _regs.cs =  ((us *)0)[ (i<<1) +1];
  _regs.eip = ((us *)0)[  i<<1    ];
  if (i==0x9) {
    *--ssp = _regs.eflags;
    *--ssp = (_regs.cs); 
    *--ssp = (_regs.eip);
    _regs.esp -= 6;
    _regs.cs =  ((us *)0)[ (0x15<<1) +1];
    _regs.eip = ((us *)0)[  0x15<<1    ];
    _regs.eax = 0x4f00;
  }

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
  _regs.eflags &= ~(TF|IF|NT);	

/*
printf("int_queue_head->int_queue_return_addr=%x\n", int_queue_head->int_queue_return_addr);
printf("int_queue_head->prev=%x\n", int_queue_head->prev);
printf("int_queue_head->next=%x\n", int_queue_head->next);
printf("int_queue_head->int_queue_ptr->callend=%x\n", int_queue_head->int_queue_ptr->callend);
printf("int_queue[int_queue_start].callend=%x\n", int_queue[int_queue_start].callend);
printf("int_queue_head->int_queue_ptr->interrupt=%x\n", int_queue_head->int_queue_ptr->interrupt);
printf("int_queue[int_queue_start].interrupt=%x\n", int_queue[int_queue_start].interrupt);
*/

  h_printf("int_queue: running int %d return_addr=%x\n",i,int_queue_head->int_queue_return_addr);
  int_queue_start = (int_queue_start+1)%IQUEUE_LEN;
  return;
}


/* load <msize> bytes of file <name> starting at offset <foffset> 
 * into memory at <mstart>
 */
int
load_file(char *name, int foffset, char *mstart, int msize)
{
  int fd=open(name, O_RDONLY);
  DOS_SYSCALL( lseek(fd, foffset, SEEK_SET) );
  RPT_SYSCALL( read(fd, mstart, msize) );
  return 0;
}


void
emulate(int argc, char **argv)
{
  struct sigaction sa;
  int c;
  char *confname = NULL;

  config_defaults();
  
/* set queue head and tail to NULL */
  int_queue_head=int_queue_tail=NULL;

  /* initialize cli() and sti() */
  signal_init();
  
  iq.queued=0;
  in_sighandler=0;
  sync(); /* for safety */
  setbuf(stdout, NULL);
  

  
  /* allocate screen buffer for non-console video compare speedup */
  scrbuf=malloc(CO*LI*2);
 
  opterr=0;
  confname = NULL;
  while ( (c=getopt(argc, argv, "ABCf:cF:kM:D:P:VNtsgx:Km234e:")) != EOF)
    if (c == 'F') confname = optarg;
  parse_config(confname);

  optind=0;
  opterr=0;
  while ( (c=getopt(argc, argv, "ABCf:cF:kM:D:P:VNtsgx:Km234e:")) != EOF) {
    switch(c) 
      {
      case 'F':  /* previously parsed config file argument */
	break;
      case 'A':
	config.hdiskboot=0;
	break;
      case 'B':
	config.hdiskboot=2;
	break;	    
      case 'C':
	config.hdiskboot=1;
	break;
      case 'f':  /* flip floppies: argument is string */
	flip_disks(optarg);
	break;
      case 'c':
	config.console_video=1;
	break;
      case 'k':
	config.console_keyb=1;
	break;
      case 'K':
	warn("Keyboard interrupt enabled...this is still buggy!\n");
	config.keybint=1;
	break;
      case 'M': {
	int max_mem = config.vga ? 640 : MAX_MEM_SIZE;

	MEM_SIZE=atoi(optarg);
	if (MEM_SIZE > max_mem)
	  MEM_SIZE = max_mem;
	break;
      }
      case 'D':
	parse_debugflags(optarg);
	break;
      case 'P':
	if (terminal_fd == -1) open_terminal_pipe(optarg);
	else error("ERROR: terminal pipe already open\n");
	break;
      case 'V':
	g_printf("Assuming VGA video card & mapped ROM\n");
	config.vga=1;
	config.mapped_bios=1;
	if (MEM_SIZE > 640) MEM_SIZE = 640;
	break;
      case 'N':
	warn("DOS will not be started\n");
	exitearly=1;
	break;
      case 't':
	g_printf("doing timer emulation\n");
	config.timers=1;
	break;
      case 's':
	g_printf("using new scrn size code\n");
	sizes=1;
	break;
      case 'g':
#ifdef EXPERIMENTAL_GFX
	g_printf("turning graphics option on\n");
	config.graphics=1;
#else
	error("Graphics support not compiled in!\n");
#endif
	break;

      case 'x':
	config.xms_size = atoi(optarg);
	x_printf("enabling %dK XMS memory\n",config.xms_size);
	break;
	
      case 'e':
	config.ems_size=atoi(optarg);
	g_printf("enabling %dK EMS memory\n",config.ems_size);
	break;

      case 'm':
	g_printf("turning MOUSE support on\n");
	config.mouse_flag=1;
	break;
	
      case '2':
	g_printf("CPU set to 286\n");
	cpu.type=CPU_286;
	break;
	
      case '3':
	g_printf("CPU set to 386\n");
	cpu.type=CPU_386;
	break;
	
      case '4':
	g_printf("CPU set to 486\n");
	cpu.type=CPU_486;
	break;

      case '?':
      default:
	p_dos_str("unrecognized option: -%c\n\r", c /*optopt*/);
	usage();
	fflush(stdout);
	fflush(stderr);
	_exit(1);
      }
  }
  
  /* setup DOS memory, whether shared or not */
  memory_setup();

  if (config.mapped_bios || config.console_video) open_kmem();
  if (config.mapped_bios) 
    {
      if (config.vbios_file) {
	warn("WARN: loading VBIOS %s into mem at 0x%X (0x%X bytes)\n",
	     config.vbios_file, VBIOS_START, VBIOS_SIZE);
	load_file(config.vbios_file, 0, (char *)VBIOS_START, VBIOS_SIZE);
      }
      else if (config.vbios_copy) {
	warn("WARN: copying VBIOS file from /dev/mem\n");
	load_file("/dev/mem", VBIOS_START, (char *)VBIOS_START, VBIOS_SIZE);
      }
      else map_bios();
    }

  /* copy graphics characters from system BIOS */
  load_file("/dev/mem", GFX_CHARS, (char *)GFX_CHARS, GFXCHAR_SIZE);

  g_printf("EMULATE\n");


/* we assume system call restarting... under linux 0.99pl8 and earlier,
 * this was the default.  SA_RESTART was defined in 0.99pl8 to explicitly
 * request restarting (and thus does nothing).  However, if this ever
 * changes, I want to be safe
 */
#ifndef SA_RESTART
#define SA_RESTART 0
#endif

#define SETSIG(sig, fun)	sa.sa_handler = (SignalHandler)fun; \
				sa.sa_flags = SA_RESTART; \
				sigemptyset(&sa.sa_mask); \
				sigaddset(&sa.sa_mask, SIG_TIME); \
				sigaction(sig, &sa, NULL);

  /* init signal handlers */
  SETSIG(SIGSEGV, sigsegv);
  SETSIG(SIGILL, sigill);
  SETSIG(SIG_TIME, sigalrm);
  SETSIG(SIGFPE, sigfpe);
  SETSIG(SIGTRAP, sigtrap);
  SETSIG(SIGIPC, sigipc);
  SETSIG(SIG_SER, sigser);
  
  
  SETSIG(SIGHUP, leavedos);    /* for "graceful" shutdown */
  SETSIG(SIGTERM, leavedos);
  SETSIG(SIGKILL, leavedos);
  SETSIG(SIGCHLD, sigchld);
  SETSIG(SIGQUIT, sigquit);
  SETSIG(SIGUNUSED, timint);
  
  /* do time stuff - necessary for initial time setting */
  {
    struct timeval tp;
    struct timezone tzp;
    struct tm *tm;
    unsigned long ticks;
    
    time(&start_time);
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
    tm = localtime((time_t *)&ticks);
    last_ticks=(tm->tm_hour*60*60 + tm->tm_min*60 + tm->tm_sec)*18.206;
    set_ticks(last_ticks);
  };
  
  disk_init();
  termioInit();
  hardware_init();
  if (config.vga) vga_initialize();
  clear_screen(SCREEN, 7);

  dbug_printf("$Header: /home/src/dosemu0.49pl2/RCS/emu.c,v 1.4 1993/11/17 22:29:33 root Exp root $\n");

  boot(config.hdiskboot? hdisktab : disktab); 

  fflush(stdout);
  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = UPDATE;
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = UPDATE;
  setitimer(TIMER_TIME, &itv, NULL);
  
  if (!config.console_video)
    vm86s.flags = VM86_SCREEN_BITMAP;
  else
    vm86s.flags = 0;
  vm86s.screen_bitmap = 0;
  scrtest_bitmap = 1 << (24 + SCREEN);
  update_screen=1;
  
  /* start up the IPC process...stopped in leavedos() */
  start_dosipc();
  post_dosipc();

  for(;!fatalerr;) {
    run_vm86();
    serial_run(); 
    int_queue_run();
  }
  error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	fatalerr, fatalerr, in_sigsegv, ignore_segv);
  
  leavedos(0);
}

void ign_sigs(int sig)
{
  static int timerints=0;
  static int otherints=0;

  error("ERROR: signal %d received in leavedos()\n",sig);
  if (sig == SIG_TIME) timerints++;
  else otherints++;

#define LEAVEDOS_TIMEOUT (3 * FREQ)
#define LEAVEDOS_SIGOUT  5
/* XXX - why do I need this? */
  if ((timerints >= LEAVEDOS_TIMEOUT) || (otherints >= LEAVEDOS_SIGOUT))
    {
      error("ERROR: timed/signalled out in leavedos()\n");
      fclose(stderr);
      fclose(stdout);
      _exit(1);
    }
}

/* "graceful" shutdown */
void leavedos(int sig)
{
  struct sigaction sa;

  in_vm86 = 0;

  SETSIG(SIG_TIME, ign_sigs);
  SETSIG(SIGSEGV, ign_sigs);
  SETSIG(SIGILL, ign_sigs);
  SETSIG(SIGFPE, ign_sigs);
  SETSIG(SIGTRAP, ign_sigs);
  p_dos_str("\n\rDOS killed!\n\r"); 
  error("leavedos(%d) called - shutting down\n", sig);  

  close_all_printers();

  serial_close();

  show_ints(0,0x33);
  show_regs();
  fflush(stderr);
  fflush(stdout);

  termioClose();
  disk_close_all();

  /* close down the IPC process & kill all shared mem */
  SETSIG(SIGCHLD, SIG_IGN);              /* ignore its death */

  stop_dosipc();

  _exit(0);
}



void hardware_init(void)
{
  int i;

  /* do PIT init here */
  serial_init();
  init_all_printers();

  /* PIC init */
  for (i=0; i < 2; i++)
    {
      pics[i].OCW1 = 0;   /* no IRQ's serviced */
      pics[i].OCW2 = 0;   /* no EOI's received */
      pics[i].OCW3 = 8;   /* just marks this as OCW3 */
    }

  g_printf("Hardware initialized\n"); 
}

/* check the fd for data ready for reading */
int d_ready(int fd)
{
   struct timeval w_time;
   fd_set checkset;

   w_time.tv_sec=0;
   w_time.tv_usec=200000;

   FD_ZERO(&checkset);
   FD_SET(fd, &checkset);
   
   if ( RPT_SYSCALL(select(fd+1, &checkset, NULL, NULL, &w_time)) == 1)
     {
       if (FD_ISSET(fd, &checkset)) return(1);
       else return(0);
     }
   else return(0);
 }


void 
usage(void)
{
   fprintf(stderr, "$Header: /home/src/dosemu0.49pl2/RCS/emu.c,v 1.4 1993/11/17 22:29:33 root Exp root $\n");
   fprintf(stderr,"usage: dos [-ABCckbVtsgxKm234e] [-D flags] [-M SIZE] [-P FILE] [-H|F #disks] [-f FLIPSTR] > doserr\n");
   fprintf(stderr,"    -A boot from first defined floppy disk (A)\n");
   fprintf(stderr,"    -B boot from second defined floppy disk (B) (#)\n");
   fprintf(stderr,"    -C boot from first defined hard disk (C)\n");
   fprintf(stderr,"    -f flip disks, argument \"X,Y\" where 1 <= X&Y <= 3\n");
   fprintf(stderr,"    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
   fprintf(stderr,"    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
   fprintf(stderr,"    -D set debug-msg mask to flags (+-)(avkdRWspwgxhi01)\n");
   fprintf(stderr,"    -M set memory size to SIZE kilobytes (!)\n");
   fprintf(stderr,"    -P copy debugging output to FILE\n");
   fprintf(stderr,"    -b map BIOS into emulator RAM (%%)\n");
   fprintf(stderr,"    -H specify number of hard disks (1 or 2)\n");
   fprintf(stderr,"    -F specify number of floppy disks (1-4)\n");
   fprintf(stderr,"    -V use BIOS-VGA video modes (!#%%)\n");
   fprintf(stderr,"    -N No boot of DOS\n");
   fprintf(stderr,"    -t try new timer code (#)\n");
   fprintf(stderr,"    -s try new screen size code (COMPLETELY BROKEN)(#)\n");
   fprintf(stderr,"    -g enable graphics modes (COMPLETELY BROKEN) (!%%#)\n");
   fprintf(stderr,"    -x SIZE enable SIZE K XMS RAM\n");
   fprintf(stderr,"    -e SIZE enable SIZE K EMS RAM\n");
   fprintf(stderr,"    -m enable mouse support (!#)\n");
   fprintf(stderr,"    -2,3,4 choose 286, 386 or 486 CPU\n");
   fprintf(stderr,"    -K Do int9 within PollKeyboard (!#)\n");
   fprintf(stderr,"\n     (!) means BE CAREFUL! READ THE DOCS FIRST!\n");
   fprintf(stderr,"     (%%) marks those options which require dos be run as root (i.e. suid)\n");
   fprintf(stderr,"     (#) marks options which do not fully work yet\n");
}

int
ifprintf(unsigned char flg,const char *fmt, ...)
{
  va_list args;
  char buf[1025];
  int i;
  
  if (! flg) return 0;

  va_start(args, fmt);
  i=vsprintf(buf, fmt, args);
  va_end(args);

#if 0
  printf(buf);
  if (terminal_pipe) fprintf(terminal, buf);
#else
  write(STDOUT_FILENO, buf, strlen(buf));
  if (terminal_pipe) write(terminal_fd, buf, strlen(buf));
#endif
  return i;
}  

void
p_dos_str(char *fmt, ...)
{
  va_list args;
  char buf[1025], *s;
  int i;
  
  va_start(args, fmt);
  i=vsprintf(buf, fmt, args);
  va_end(args);

  s=buf;
  while (*s) char_out(*s++, SCREEN, ADVANCE);
}


void
init_vga_card(void)
{
  u_short *ssp;
  
  if (!config.mapped_bios) {
    error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
    return;
  }
  if (set_ioperm(0x3b0,0x3db-0x3b0, 1)) 
    warn("couldn't get range!\n");
  config.vga=1;
  set_vc_screen_page(SCREEN);
  ssp = SEG_ADR((us *), ss, sp);
  *--ssp = _regs.cs;
  *--ssp = _regs.eip;
  _regs.esp -= 4;
  _regs.cs = 0xc000;
  _regs.eip = 3;
}
  


void
ems_helper(void)
{
  u_char *rhptr;  /* request header pointer */

  switch(LWORD(ebx))
    {
    case 0:
      error("EMS Init called!\n");
      break;
    case 3:
      error("EMS IOCTL called!\n");
      break;
    case 4:
      error("EMS READ called!\n");
      break;
    case 8:
      error("EMS WRITE called!\n");
      break;
    case 10:
      error("EMS Output Status called!\n");
      break;
    case 12:
      error("EMS IOCTL-WRITE called!\n");
      break;
    case 13:
      error("EMS OPENDEV called!\n");
      break;
    case 14:
      error("EMS CLOSEDEV called!\n");
      break;
    case 0x20:
      error("EMS INT 0x67 called!\n");
      break;
    default:
      error("UNKNOWN EMS HELPER FUNCTION %d\n", LWORD(ebx));
    }
  rhptr = SEG_ADR((u_char *), es, di);
  error("EMS RHDR: len %d, command %d\n", *rhptr, *(u_short *)(rhptr+2));
}


/* returns 1 if dos_helper() handles it, 0 otherwise */
int
dos_helper(void)
{
  us *ssp;

  switch (LO(ax)) 
    {
    case 0x20:
      mfs_inte5();
      return 1;
      break;
    case 0x21:
      ems_helper();
      return 1;
      break;
    case 0x22: {
      error("EMS: in 0xe5,0x22 handler!\n");
      LWORD(eax) = pop_word(&_regs);
      if (config.ems_size)
	bios_emm_fn(&_regs);
      else
	error("EMS: not running bios_em_fn!\n");
      break;
    }
    case 0:     /* Linux dosemu installation test */
      LWORD(eax)=0xaa55;
      LWORD(ebx)=VERNUM;  /* major version 0.49 -> 0x0049 */
      warn("WARNING: dosemu installation check\n");
      show_regs();
      break;
    case 1:	/* SHOW_REGS */
      show_regs();
      break;
    case 2:	/* SHOW INTS, BH-BL */
      show_ints(HI(bx), LO(bx));
      break;
    case 3:	/* SET IOPERMS: bx=start, cx=range,
		   carry set for get, clear for release */
      {
	int cflag=_regs.eflags&CF ? 1 : 0;
	
	i_printf("I/O perms: 0x%x 0x%x %d\n",
		 LWORD(ebx), LWORD(ecx), cflag);
	if (set_ioperm(LWORD(ebx), LWORD(ecx), cflag))
	  {
	    error("ERROR: SET_IOPERMS request failed!!\n");
	    CARRY;  /* failure */
	  }
	else
	  {
	    if (cflag) warn("WARNING! DOS can now access I/O ports 0x%x to 0x%x\n", LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
	    else warn("Access to ports 0x%x to 0x%x clear\n",
		      LWORD(ebx),LWORD(ebx) + LWORD(ecx) - 1);
	    NOCARRY;   /* success */
	  }
      }
      break;
    case 4:  /* initialize video card */
      if (LO(bx) == 0)
	{
	  if (set_ioperm(0x3b0, 0x3db-0x3b0, 0))
	    warn("couldn't shut off ioperms\n");
	  SETIVEC(0x10, BIOSSEG, 0x10*0x10);  /* restore our old vector */
	  config.vga=0;
	}
      else
	{
	  if (!config.mapped_bios) {
	    error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
	    return 1;
	  }
	  if (set_ioperm(0x3b0,0x3db-0x3b0, 1)) 
	    warn("couldn't get range!\n");
	  config.vga=1;
	  set_vc_screen_page(SCREEN);
	  warn("WARNING: jumping to 0c000:0003\n");
	  ssp = SEG_ADR((us *), ss, sp);
	  *--ssp = _regs.cs;
	  *--ssp = _regs.eip;
	  precard_eip=_regs.eip;
	  precard_cs=_regs.cs;
	  _regs.esp -= 4;
	  _regs.cs = 0xc000;
	  _regs.eip = 3;
	  show_regs();
	  card_init=1;
	}

    case 5:  /* show banner */
      p_dos_str("\n\nLinux DOS emulator "VERSTR" $Date: 1993/11/17 22:29:33 $\n");
      p_dos_str("Last configured at %s\n", CONFIG_TIME);
      p_dos_str("                on %s\n", CONFIG_HOST);
      p_dos_str("maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n");
      break;
      
    case 0xff:
      if (LWORD(eax) == 0xffff)
	{
	  dbug_printf("DOS termination requested\n");
	  p_dos_str("\n\rLeaving DOS...\n\r");
	  leavedos(0);
	}
      break;
      
    default:
      error("ERROR: bad dos helper function: AX=0x%04x\n", LWORD(eax));
      return 0;
    }

  return 1;
}



inline uid_t
be(uid_t who)
{
  if (getuid() != who)
    return setreuid(geteuid(),getuid());
  else return 0;
}


inline uid_t
be_me()
{
  if (geteuid() == 0) {
    return setreuid(geteuid(),getuid());
    return 0;
  }
  else return geteuid();
}


inline uid_t
be_root()
{
  if (geteuid() != 0) {
    setreuid(geteuid(),getuid());
    return getuid();
    }
  else return 0;
}


int
set_ioperm(int start, int size, int flag)
{
  int tmp, s_errno;
  uid_t last_me;

#if DO_BE
  last_me = be_root();
  warn("IOP: was %d, now %d\n", last_me, 0);
#endif
  tmp=ioperm(start,size,flag);
  s_errno=errno;
#if DO_BE
  be(last_me);
  warn("IOP: was %d, now %d\n", 0, last_me);
#endif

  errno=s_errno;
  return tmp;
}

#undef EMU_C

