/* dos emulator, Matthias Lautner */

#define EMU_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1994/03/04 15:23:54 $
 * $Source: /home/src/dosemu0.50/RCS/emu.c,v $
 * $Revision: 1.45 $
 * $State: Exp $
 *
 * $Log: emu.c,v $
 * Revision 1.45  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.44  1994/03/04  14:46:13  root
 * Jochen patches.
 *
 * Revision 1.43  1994/03/04  00:01:58  root
 * Readying for 0.50
 *
 * Revision 1.42  1994/02/21  20:28:19  root
 * DPMI update
 *
 * Revision 1.41  1994/02/20  10:55:25  root
 * Added set_leds to int08 :-(.
 *
 * Revision 1.40  1994/02/20  10:00:16  root
 * More keyboard work :-(.
 *
 * Revision 1.39  1994/02/15  19:04:46  root
 * Roonie's cleaning up of inb/outb.
 *
 * Revision 1.38  1994/02/10  20:41:14  root
 * Last cleanup prior to release of pl4.
 *
 * Revision 1.37  1994/02/09  20:10:24  root
 * Added dosbanner config option for optionally displaying dosemu bannerinfo.
 * Added allowvideportaccess config option to deal with video ports.
 *
 * Revision 1.36  1994/02/05  21:45:55  root
 * Fixing Keyboard int15 4f to return AH=0x86.
 *
 * Revision 1.35  1994/02/02  21:12:56  root
 * Bringing the pktdrvr up to speed.
 *
 * Revision 1.34  1994/02/01  20:57:31  root
 * With unlimited thanks to gorden@jegnixa.hsc.missouri.edu (Jason Gorden),
 * here's a packet driver  to compliment Tim_R_Bird@Novell.COM's IPX work.
 *
 * Revision 1.33  1994/02/01  19:25:49  root
 * Fix to allow multiple graphics DOS sessions with my Trident card.
 *
 * Revision 1.32  1994/01/31  18:44:24  root
 * Work on making mouse work
 *
 * Revision 1.30  1994/01/30  14:29:51  root
 * Changed FCB callout for redirector, now inline and works with Create|O_TRUNC.
 *
 * Revision 1.29  1994/01/30  12:30:23  root
 * Changed dos_helper to int 0xe6.
 *
 * Revision 1.28  1994/01/28  20:04:07  root
 * Tim's IPX is ready to go.
 * Modified mmap strategy.
 *
 * Revision 1.27  1994/01/28  18:52:58  root
 * Fix int15 0xc0 call.
 *
 * Revision 1.26  1994/01/27  22:09:20  root
 * Allow USAGE to display before default stderr redirection.
 *
 * Revision 1.25  1994/01/27  21:47:09  root
 * Introducing IPX from Tim_R_Bird@Novell.COM.
 *
 * Revision 1.24  1994/01/27  19:43:54  root
 * Preparing for Tim's IPX.
 * Added dos auto-redirect to stderr.
 * Started saytime() function
 *
 * Revision 1.23  1994/01/25  20:02:44  root
 * Modified hard_int routine again.
 * Exchange stderr <-> stdout.
 * Made stderr redirect to /dev/null if user does not redirect it.
 *
 * Revision 1.22  1994/01/20  21:14:24  root
 * Indent, more work serially handling multiple interrupts.
 *
 * Revision 1.21  1994/01/19  20:27:20  root
 * Deleted comment about int16 to inline.
 *
 * Revision 1.20  1994/01/19  17:51:14  root
 * Added dpmi/dpmi.h include for interrupts.
 * Added code to allow int09 (keyboard) to allow another interrupt by using
 * outb(20) called by dos programs.
 * Added code to allow FCB callbacks when mfs.c does an FCB open, kinda kludgy
 * at this time.
 * Modified inline int09 to pass ALL keys to int15-4f function.
 * Added a far return for DPMI call to go protected.
 * Removed old int16 function.
 * Changed dos_helper int e5 to not be revectored. Still no good for my
 * Direct Access, but I'll fix that next :-).
 * Changed int08 inside of do_int() to return after being called.
 * Allowed dos_helper to pass through if redirected, after being called.
 *
 * Revision 1.19  1994/01/12  21:27:15  root
 * Some more EMS fixups
 *
 * Revision 1.18  1994/01/03  22:19:25  root
 * Debugging
 *
 * Revision 1.17  1994/01/01  17:06:19  root
 * Hack to fix EMS not having ax on top of stack. Needs to be debugged !
 *
 * Revision 1.16  1993/12/31  09:29:01  root
 * Added dos_helper hook to Theadore T'so's booton bootoff patch. Now a
 * user can boot from a bootdisk a: type diskimage, and then return control
 * of a: to /dev/fd0. Alright :-).
 *
 * Revision 1.15  1993/12/30  15:11:50  root
 * Fixing multiple emufs.sys problem.
 *
 * Revision 1.14  1993/12/30  11:18:32  root
 * Updates for Diamond Card.
 *
 * Revision 1.13  1993/12/27  19:06:29  root
 * Small fixes for EMS
 *
 * Revision 1.12  1993/12/22  11:45:36  root
 * Keyboard enhancements, and more debug for EMS
 *
 * Revision 1.11  1993/12/05  20:59:03  root
 * Dimond card work.
 *
 * Revision 1.10  1993/11/30  22:21:03  root
 * Final Freeze for release pl3
 *
 * Revision 1.9  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.8  1993/11/29  22:44:11  root
 * Prepare for release of pl3
 *
 * Revision 1.7  1993/11/29  00:05:32  root
 * Overhauling keyboard and some timing stuff.
 *
 * Revision 1.6  1993/11/25  22:45:21  root
 * About to destroy keybaord routines.
 *
 * Revision 1.5  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
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

/*
   DOSEMU must not work within the 1 meg DOS limit, so start of code
   is loaded at a higher address, at some time this could conflict with
   other shared libs
*/
#if STATIC
__asm__(".org 0x110000");
#endif

/*
   Here DOSEMU jumps to the emulate function which was loaded above
   the 1 meg DOS area

   make sure that this line ist the first of emu.c
   and link emu.o as the first object file to the lib
*/
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
#ifdef USE_NCURSES
#include <ncurses.h>
#else
#include <termio.h>
#include <termcap.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <sys/vm86.h>
#include <syscall.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "bios.h"
#include "termio.h"
#include "video.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "dosipc.h"
#include "disks.h"
#include "xms.h"
#include "timers.h"
#ifdef DPMI
#include "dpmi/dpmi.h"
#endif
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"

char *cstack[4096];

int int_queue_running = 0;

/*
   Allow checks via inport 0x64 for available scan codes
*/
extern u_char keys_ready;

void video_config_init();

/*
   In normal keybaord mode (XLATE), some concerns are delt with to
   efficiently write characters to the STDOUT output. These macros
   attempt to speed up this screen updating
*/
/* thanks to Andrew Haylett (ajh@gec-mrc.co.uk) for catching character loss
 * in CHOUT */
#define OUTBUFSIZE	3000
#define CHOUT(c)   if (outp == &outbuf[OUTBUFSIZE-2]) { CHFLUSH } \
			*(outp++) = (c);
#define CHFLUSH    if (outp - outbuf) { v_write(1, outbuf, outp - outbuf); \
						outp = outbuf; }
unsigned char outbuf[OUTBUFSIZE], *outp = outbuf;

/* Need some way to kill hard interrupt calls that wish to look
   like they end before returning to previous code, so I'll take
   the outb 0x20 ack interrupt port output to be O.K. */
u_char outb20 = 1;

/*
   This flag will be set when doing video routines so that special
   access can be given
*/
u_char in_video = 0;

/* Video write */
void v_write(int, unsigned char *, int);

/*
   Tables that hold information of currently specified storage
   devices.
*/
extern struct disk disktab[], hdisktab[];

/* Used to keep track of when xms calls are made */
extern int xms_grab_int15;

/* If -N option used at start up, allow DOSEMU to quit before starting */
int exitearly = 0;

/* Structure to hold all current configuration information */
struct config_info config;

/*
   Structure to hold current state of all VM86 registers used in
   VM86 mode
*/
extern struct vm86_struct vm86s;

/* Small structure of CPU related params (like 80386) */
extern struct CPU cpu;

/* Keep track of error that causes DOSEMU to exit */
int fatalerr;

/* Time structures for translating UNIX <-> DOS times */
struct timeval scr_tv;
struct itimerval itv;

/*
   Do to timing problems, scanned lets keyboard port reads know
   if they should return the current key on the queue, or the
   next one
*/
extern u_char scanned;

/* Keep track of the current depth of hard interrupts */
u_char int_count[8] =
{0, 0, 0, 0, 0, 0, 0, 0};

/*
   Queue to hold all pending hard-interrupts. When an interrupt is
   placed into this queue, it can include a function to be run
   prior to the actuall interrupt being placed onto the DOS stack,
   as well as include a function to be called after the interrupt
   finishes.
*/
#define IQUEUE_LEN 1000
int int_queue_start = 0;
int int_queue_end = 0;
struct int_queue_struct {
  int interrupt;
  int (*callstart) ();
  int (*callend) ();
}

int_queue[IQUEUE_LEN];

/*
   This is here to allow multiple hard_int's to be running concurrently.
   I know this is wrong, why I don't know, but has to be here for any
   programs that steal INT9 away from DOSEMU.
*/
struct int_queue_list_struct {
  struct int_queue_list_struct *prev;
  struct int_queue_list_struct *next;
  struct int_queue_struct *int_queue_ptr;
  int int_queue_return_addr;
  struct vm86_regs saved_regs;
} *int_queue_head = NULL, *int_queue_tail = NULL, *int_queue_temp = NULL;

int scrtest_bitmap, update_screen;	/* Flags to test if screen to be updated */
unsigned char *scrbuf;		/* the previously updated screen */

long start_time;		/* Keep track of times for DOS calls */
unsigned long last_ticks;

int card_init = 0;		/* VGAon exectuted flag */
unsigned long precard_eip, precard_cs;	/* Save state at VGAon */

/* XXX - the mem size of 734 is much more dangerous than 704.
 * 704 is the bottom of 0xb0000 memory.  use that instead?
 */
#ifdef EXPERIMENTAL_GFX
#define MAX_MEM_SIZE    640
#else
#define MAX_MEM_SIZE    734	/* up close to the 0xB8000 mark */
#endif

/* this holds all the configuration information, set in config_init() */
unsigned int configuration;
void config_init(void);

/* Function to set up all memory area for DOS, as well as load boot block */
void boot();

extern void map_bios(void);	/* map in VIDEO bios */
extern int open_kmem();		/* Get access to physical memory */

/* FD for the packet driver */
extern int pd_sock;

void leavedos(int),		/* function to stop DOSEMU */
 usage(void),			/* Print parameters of DOSEMU */
 hardware_init(void),		/* Initialize info on hardware */
 do_int(int);			/* Called by sigsegv() in cpu.c to handle ints */

int dos_helper(void);		/* handle int's created especially for DOSEMU */
void init_vga_card(void);	/* Function to set VM86 regs to run VGA initialation */

/* Programmable Interrupt Controller, 8259 */
struct pic {
  int stage;			/* where in init. , 0=ICW1 */
  /* the seq. is ICW1 to 0x20, ICW2 to 0x21
         * if ICW1:D1=0, ICW3 to 0x20h,
	 * ICW4 to 0x21, OCWs any order
	 */
  unsigned char
   ICW1,			/* Input Control Words */
   ICW2, ICW3, ICW4, OCW1,	/* Output Control Words */
   OCW2, OCW3;
}

pics[2];

/* int port61 = 0xd0;		the pseudo-8255 device on AT's */
int port61 = 0x0e;		/* the pseudo-8255 device on AT's */

int special_nowait = 0;

struct ioctlq iq =
{0, 0, 0, 0};			/* one-entry queue :-( for ioctl's */
char *tmpdir;
int in_sighandler = 0;		/* so I know to not use non-reentrant
				 * syscalls like ioctl() :-( */
int in_ioctl = 0;
struct ioctlq curi =
{0, 0, 0, 0};

/* this is DEBUGGING code! */
int sizes = 0;

struct debug_flags d =
{0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0};

int poll_io = 1;		/* polling io, default on */
int ignore_segv = 0;		/* ignore sigsegv's */
int in_sigsegv = 0;
int terminal_pipe;
int terminal_fd = -1;

#define kbd_flags *KBDFLAG_ADDR
#define key_flags *KEYFLAG_ADDR

extern move_kbd_key_flags;

int in_interrupt = 0;		/* for unimplemented interrupt code */

/* for use by cli() and sti() */
sigset_t oldset;

unsigned char trans[] =		/* LATIN CHAR SET */
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

/* Similar to the sigaction function in libc, except it leaves alone the
   restorer field
   stolen from the wine-project */

static int
dosemu_sigaction(int sig, struct sigaction *new, struct sigaction *old)
{
  __asm__("int $0x80":"=a"(sig)
	  :"0"(SYS_sigaction), "b"(sig), "c"(new), "d"(old));
  if (sig >= 0)
    return 0;
  errno = -sig;
  return -1;
}

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
  DOS_SYSCALL(sigprocmask(SIG_SETMASK, &blockset, &oldset));
}

void
sti(void)
{
  sigset_t blockset;

  DOS_SYSCALL(sigprocmask(SIG_SETMASK, &oldset, &blockset));
}

/*
   Here is where DOSEMU runs VM86 mode with the vm86() call which
   also has the registers that it will be called with. It will stop
   vm86 mode for many reasons, like trying to execute an interrupt,
   doing port I/O to ports not opened for I/O, etc ...
*/
inline void
run_vm86(void)
{
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */
#ifdef DPMI
  if (!in_dpmi || int_queue_running || in_dpmi_dos_int) {
#endif
    in_vm86 = 1;
    (void) vm86(&vm86s);
    in_vm86 = 0;
#ifdef DPMI
  }
  else {
    dpmi_control();
  }
#endif

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

/* Put a character on the output fd of this DOSEMU */
int
outcbuf(int c)
{
  CHOUT(c);
  return 1;
}

#ifndef USE_NCURSES
/* Put a string on the output fd */
void
dostputs(char *a, int b, int (*c) (int) /* outfuntype c */ )
{
  /* discard c right now */
  /* CHFLUSH; */
  /* was "CHFLUSH; tputs(a,b,outcbuf);" */
  tputs(a, b, c);
}

#endif

/* position the cursor on the output fd */
inline void
poscur(int x, int y)
{
#ifdef USE_NCURSES
  move(y, x);
  refresh();			/* may not be necessary */
#else
  /* were "co" and "li" */
  if ((unsigned) x >= CO || (unsigned) y >= LI)
    return;
  tputs(tgoto(cm, x, y), 1, outch);
#endif
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
Scroll(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx = x1 - x0 + 1;
  int dy = y1 - y0 + 1;
  int x, y;
  us *sadr, blank = ' ' | (att << 8);
  us *tbuf;

  if (dx <= 0 || dy <= 0 || x0 < 0 || x1 >= CO || y0 < 0 || y1 >= LI)
    return;

  update_screen = 0;

  /* make a blank line */
  tbuf = (us *) malloc(sizeof(us) * dx);
  if (!tbuf) {
    error("failed to malloc temp buf in scroll!");
    return;
  }
  for (x = 0; x < dx; x++)
    tbuf[x] = blank;

  sadr = SCREEN_ADR(SCREEN);

  if (l >= dy || l <= -dy)
    l = 0;

  if (l == 0) {			/* Wipe mode */
    for (y = y0; y <= y1; y++)
      memcpy(&sadr[y * CO + x0], tbuf, dx * sizeof(us));
    free(tbuf);
    return;
  }

  if (l > 0) {
    if (dx == CO)
      memcpy(&sadr[y0 * CO],
	     &sadr[(y0 + l) * CO],
	     (dy - l) * dx * sizeof(us));
    else
      for (y = y0; y <= (y1 - l); y++)
	memcpy(&sadr[y * CO + x0], &sadr[(y + l) * CO + x0], dx * sizeof(us));

    for (y = y1 - l + 1; y <= y1; y++)
      memcpy(&sadr[y * CO + x0], tbuf, dx * sizeof(us));
  }
  else {
    for (y = y1; y >= (y0 - l); y--)
      memcpy(&sadr[y * CO + x0], &sadr[(y + l) * CO + x0], dx * sizeof(us));

    for (y = y0 - l - 1; y >= y0; y--)
      memcpy(&sadr[y * CO + x0], tbuf, dx * sizeof(us));
  }

  update_screen = 0;
  memcpy(scrbuf, sadr, CO * LI * 2);
  free(tbuf);
}

#else

void
scrollup(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx, dy, x, y, ofs;
  us *sadr, *p, *q, blank = ' ' | (att << 8);

  if (l == 0) {			/* Wipe mode */
    sadr = SCREEN_ADR(SCREEN);
    for (dy = y0; dy <= y1; dy++)
      for (dx = x0; dx <= x1; dx++)
	sadr[dy * CO + dx] = blank;
    return;
  }

  sadr = SCREEN_ADR(SCREEN);
  sadr += x0 + CO * (y0 + l);

  dx = x1 - x0 + 1;
  dy = y1 - y0 - l + 1;
  ofs = -CO * l;
  for (y = 0; y < dy; y++) {
    p = sadr;
    if (l != 0)
      for (x = 0; x < dx; x++, p++)
	p[ofs] = p[0];
    else
      for (x = 0; x < dx; x++, p++)
	p[0] = blank;
    sadr += CO;
  }
  for (y = 0; y < l; y++) {
    sadr -= CO;
    p = sadr;
    for (x = 0; x < dx; x++, p++)
      *p = blank;
  }

  update_screen = 1;
}

void
scrolldn(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx, dy, x, y, ofs;
  us *sadr, *p, blank = ' ' | (att << 8);

  if (l == 0) {
    l = LI - 1;			/* Clear whole if l=0 */
  }

  sadr = SCREEN_ADR(SCREEN);
  sadr += x0 + CO * (y1 - l);

  dx = x1 - x0 + 1;
  dy = y1 - y0 - l + 1;
  ofs = CO * l;
  for (y = 0; y < dy; y++) {
    p = sadr;
    if (l != 0)
      for (x = 0; x < dx; x++, p++)
	p[ofs] = p[0];
    else
      for (x = 0; x < dx; x++, p++)
	p[0] = blank;
    sadr -= CO;
  }
  for (y = 0; y < l; y++) {
    sadr += CO;
    p = sadr;
    for (x = 0; x < dx; x++, p++)
      *p = blank;
  }

  update_screen = 1;
}

#endif /* OLD_SCROLL */

void
v_write(int fd, unsigned char *ch, int len)
{
  if (!config.console_video)
    DOS_SYSCALL(write(fd, ch, len));
  else
    error("ERROR: (video) v_write deferred for console_video\n");
}

#ifdef USE_NCURSES
void 
char_out(unsigned char ch, int s, int advflag)
{
  addch(ch);
  refresh();
}

#else
void
char_out_att(unsigned char ch, unsigned char att, int s, int advflag)
{
  u_short *sadr;
  int xpos = XPOS(s), ypos = YPOS(s);

  /*	if (s > max_page) return; */

  if (config.console_video) {
    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[YPOS(s) * CO + XPOS(s)++] = ch | (att << 8);
    }
    else if (ch == '\r') {
      XPOS(s) = 0;
    }
    else if (ch == '\n') {
      YPOS(s)++;
      XPOS(s) = 0;		/* EDLIN needs this behavior */
    }
    else if (ch == '\010' && XPOS(s) > 0) {
      XPOS(s)--;
    }
    else if (ch == '\t') {
      v_printf("tab\n");
      do {
	char_out(' ', s, advflag);
      } while (XPOS(s) % 8 != 0);
    }

    if (!advflag) {
      XPOS(s) = xpos;
      YPOS(s) = ypos;
    }
    else
      poscur(XPOS(s), YPOS(s));
  }

  else {
    unsigned short *wscrbuf = (unsigned short *) scrbuf;

    /* update_screen not set to 1 because we do the outputting
	   * ourselves...scrollup() and scrolldn() should also work
	   * this way, when I get around to it.
	   */
    update_screen = 0;

    /* this will need some fixing to work with advflag, so that extra
	   * characters won't cause wraparound...
	   */

    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[YPOS(s) * CO + XPOS(s)] = ch | (att << 8);
      wscrbuf[YPOS(s) * CO + XPOS(s)] = ch | (att << 8);
      XPOS(s)++;
      if (s == SCREEN)
	outch(trans[ch]);
    }
    else if (ch == '\r') {
      XPOS(s) = 0;
      if (s == SCREEN)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\n') {
      YPOS(s)++;
      XPOS(s) = 0;		/* EDLIN needs this behavior */
      if (s == SCREEN)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\010' && XPOS(s) > 0) {
      XPOS(s)--;
      if ((s == SCREEN) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\t') {
      do {
	char_out(' ', s, advflag);
      } while (XPOS(s) % 8 != 0);
    }
    else if (ch == '\010' && XPOS(s) > 0) {
      XPOS(s)--;
      if ((s == SCREEN) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
  }

  if (advflag) {
    if (XPOS(s) == CO) {
      XPOS(s) = 0;
      YPOS(s)++;
    }
    if (YPOS(s) == LI) {
      YPOS(s)--;
      scrollup(0, 0, CO - 1, LI - 1, 1, 7);
      update_screen = 0 /* 1 */ ;
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
  int xpos = XPOS(s), ypos = YPOS(s);
  int newline_att = 7;

  if (config.console_video) {
    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[YPOS(s) * CO + XPOS(s)] &= 0xff00;
      sadr[YPOS(s) * CO + XPOS(s)++] |= ch;
    }
    else if (ch == '\r') {
      XPOS(s) = 0;
    }
    else if (ch == '\n') {
      YPOS(s)++;
      XPOS(s) = 0;		/* EDLIN needs this behavior */

      /* color new line */
      sadr = SCREEN_ADR(s);
      newline_att = sadr[YPOS(s) * CO + XPOS(s) - 1] >> 8;

    }
    else if (ch == '\010' && XPOS(s) > 0) {
      XPOS(s)--;
    }
    else if (ch == '\t') {
      v_printf("tab\n");
      do {
	char_out(' ', s, advflag);
      } while (XPOS(s) % 8 != 0);
    }

    if (!advflag) {
      XPOS(s) = xpos;
      YPOS(s) = ypos;
    }
    else
      poscur(XPOS(s), YPOS(s));
  }

  else {
    unsigned short *wscrbuf = (unsigned short *) scrbuf;

    /* this will need some fixing to work with advflag, so that extra
	   * characters won't cause wraparound...
	   */

    update_screen = 0;

    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[YPOS(s) * CO + XPOS(s)] &= 0xff00;
      sadr[YPOS(s) * CO + XPOS(s)] |= ch;
      wscrbuf[YPOS(s) * CO + XPOS(s)] &= 0xff00;
      wscrbuf[YPOS(s) * CO + XPOS(s)] |= ch;
      XPOS(s)++;
      if (s == SCREEN)
	outch(trans[ch]);
    }
    else if (ch == '\r') {
      XPOS(s) = 0;
      if (s == SCREEN)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\n') {
      YPOS(s)++;
      XPOS(s) = 0;		/* EDLIN needs this behavior */
      if (s == SCREEN)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\010' && XPOS(s) > 0) {
      XPOS(s)--;
      if ((s == SCREEN) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\t') {
      do {
	char_out(' ', s, advflag);
      } while (XPOS(s) % 8 != 0);
    }
    else if (ch == '\010' && XPOS(s) > 0) {
      XPOS(s)--;
      if ((s == SCREEN) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
  }

  if (advflag) {
    if (XPOS(s) == CO) {
      XPOS(s) = 0;
      YPOS(s)++;
    }
    if (YPOS(s) == LI) {
      YPOS(s)--;
      scrollup(0, 0, CO - 1, LI - 1, 1, newline_att);
      update_screen = 0 /* 1 */ ;
    }
  }
}

#endif /* USE_NCURSES */

void
clear_screen(int s, int att)
{
#ifdef USE_NCURSES
  attrset(A_NORMAL);		/* should set attribute according to 'att' */
  clear();
  move(0, 0);
  refresh();
#else
  us *sadr, *p, blank = ' ' | (att << 8);
  int lx;

  update_screen = 0;
  v_printf("VID: cleared screen\n");
  if (s > max_page)
    return;
  sadr = SCREEN_ADR(s);
  cli();

  for (p = sadr, lx = 0; lx < (CO * LI); *(p++) = blank, lx++) ;
  if (!config.console_video) {
    memcpy(scrbuf, sadr, CO * LI * 2);
    if (s == SCREEN)
      tputs(cl, 1, outch);
  }

  XPOS(s) = YPOS(s) = 0;
  poscur(0, 0);
  sti();
  update_screen = 0;
#endif
}

#ifdef USE_NCURSES
void 
restore_screen()
{
  refresh();
}

#else

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
  int Xchunk = CO / config.redraw_chunks;
  int Xstart;

  v_printf("RESTORE SCREEN: scrbuf at %p\n", scrbuf);

  if (config.console_video) {
    v_printf("restore cancelled for console_video\n");
    return;
  }

  update_screen = 0;
  CHFLUSH;

  sadr = SCREEN_ADR(SCREEN);
  oa = 7;
  p = sadr;
  for (y = 0; y < LI; y++)
    for (Xstart = 0; Xstart < CO; Xstart += Xchunk) {
      int chunk = ((Xstart + Xchunk) > CO) ? (CO - Xstart) : Xchunk;

      /* only update if this chunk of line has changed
	 * ..note that sadr is an unsigned
	 * short ptr, so CO is not multiplied by 2...I'll clean this up
	 * later.
	 */
      if (!memcmp(scrbuf + y * CO * 2 + Xstart * 2, sadr + y * CO + Xstart,
		  chunk * 2)) {
	p += chunk;		/* p is an unsigned short pointer */
	continue;		/* go to the next chunk */
      }
      else
	memcpy(scrbuf + y * CO * 2 + Xstart * 2, p, chunk * 2);

      /* This chunk must have changed - we'll have to redraw it */

      /* go to the start of the chunk */
      dostputs(tgoto(cm, Xstart, y), 1, outcbuf);

      /* do chars one at a time */
      for (x = Xstart; x < Xstart + chunk; x++) {
	c = *(unsigned char *) p;
	if ((a = ((unsigned char *) p)[1]) != oa) {
	  /* do fore/back-ground colors */
	  if (!(a & 7) || (a & 0x70))
	    dostputs(mr, 1, outcbuf);
	  else
	    dostputs(me, 1, outcbuf);

	  /* do high intensity */
	  if (a & 0x8)
	    dostputs(md, 1, outcbuf);
	  else if (oa & 0x8) {
	    dostputs(me, 1, outcbuf);
	    if (!(a & 7) || (a & 0x70))
	      dostputs(mr, 1, outcbuf);
	  }

	  /* do underline/blink */
	  if (a & 0x80)
	    dostputs(so, 1, outcbuf);
	  else if (oa & 0x80)
	    dostputs(se, 1, outcbuf);

	  oa = a;		/* save old attr as current */
	}
	CHOUT(trans[c] ? trans[c] : '_');
	p++;
      }
    }

  dostputs(me, 1, outcbuf);
  CHFLUSH;
  poscur(XPOS(SCREEN), YPOS(SCREEN));
}

#else

void
restore_screen(void)
{
  us *sadr, *p;
  unsigned char c, a;
  int x, y, oa;

  update_screen = 0;

  v_printf("RESTORE SCREEN: scrbuf at 0x%08x\n", scrbuf);

  if (config.console_video) {
    v_printf("restore cancelled for console_video\n");
    return;
  }

  sadr = SCREEN_ADR(SCREEN);
  oa = 7;
  p = sadr;
  for (y = 0; y < LI; y++) {
    dostputs(tgoto(cm, 0, y), 1, outcbuf);
    for (x = 0; x < CO; x++) {
      c = *(unsigned char *) p;
      if ((a = ((unsigned char *) p)[1]) != oa) {
#ifndef OLD_RESTORE
	/* do fore/back-ground colors */
	if (!(a & 7) || (a & 0x70))
	  dostputs(mr, 1, outcbuf);
	else
	  dostputs(me, 1, outcbuf);

	/* do high intensity */
	if (a & 0x8)
	  dostputs(md, 1, outcbuf);
	else if (oa & 0x8) {
	  dostputs(me, 1, outcbuf);
	  if (!(a & 7) || (a & 0x70))
	    dostputs(mr, 1, outcbuf);
	}

	/* do underline/blink */
	if (a & 0x80)
	  dostputs(so, 1, outcbuf);
	else if (oa & 0x80)
	  dostputs(se, 1, outcbuf);

	oa = a;			/* save old attr as current */
#else
	if ((a & 7) == 0)
	  dostputs(mr, 1, outcbuf);
	else
	  dostputs(me, 1, outcbuf);
	if ((a & 0x88))
	  dostputs(md, 1, outcbuf);
	oa = a;
#endif
      }
      CHOUT(trans[c] ? trans[c] : '_');
      p++;
    }
  }

  dostputs(me, 1, outcbuf);
  CHFLUSH;
  poscur(XPOS(SCREEN), YPOS(SCREEN));
}

#endif /* CHUNKY_RESTORE */
#endif /* USE_NCURSES */

u_short microsoft_port_check = 0;

int
inb(int port)
{
  /* for scanseq */
#define NEWCODE      1
#define BREAKCODE    2

  static unsigned int cga_r = 0;
  static unsigned int tmp = 0;

  port &= 0xffff;
  if (port_readable(port))
    return (read_port(port) & 0xFF);
#if 1
  if (config.chipset && port > 0x3b3 && port < 0x3df)
    return (video_port_in(port));
#endif

  switch (port) {
  case 0x60:
    /* #define new8042 */
#ifndef new8042
    k_printf("direct 8042 about to read1: 0x%02x\n", lastscan);
    parent_nextscan();
    if (keys_ready)
      microsoft_port_check = 0;
    k_printf("direct 8042 read1: 0x%02x microsoft=%d\n", lastscan, microsoft_port_check);
    /*	      tmp=lastscan;
	      lastscan=0; */
    if (microsoft_port_check)
      return microsoft_port_check;
    else
      return lastscan;
#else
    /* this code can't send non-ascii scancodes like alt/ctrl/scrollock,
       nor keyups, etc. */
    if (new_scanseq == BREAKCODE) {
      k_printf("KBD: doing keyup for 0x%02x->0x%02x\n",
	       new_lastscan, new_lastscan | 0x80);
      new_scanseq = NEWCODE;
      return new_lastscan | 0x80;
    }

    if (!CReadKeyboard(&tmpkeycode, NOWAIT)) {
      k_printf("failed direct 8042 read\n");
      return 0;
    }
    else {
      new_lastscan = tmpkeycode >> 8;
      new_scanseq = BREAKCODE;
      k_printf("KBD: direct 8042 read: 0x%02x\n", new_lastscan);
      return new_lastscan;
    }
    break;
#endif
  case 0x64:{
      parent_nextscan();
      tmp = 0x1c | (keys_ready || microsoft_port_check ? 1 : 0);	/* low bit set = sc ready */
      /* lastscan=0; */
      k_printf("direct 8042 0x64 status check: 0x%02x keys_ready=%d, microsoft=%d\n", tmp, keys_ready, microsoft_port_check);
      return tmp;
    }

  case 0x61:
    k_printf("inb [0x61] =  0x%02x (8255 chip)\n", port61);
    return port61;

  case 0x70:
  case 0x71:
    return cmos_read(port);

  case 0x40:
    i_printf("inb [0x40] = 0x%02x  1st timer inb\n",
	     pit.CNTR0 -= 20);
    return pit.CNTR0;
  case 0x41:
    i_printf("inb [0x41] = 0x%02x  2nd timer inb\n",
	     pit.CNTR1--);
    return pit.CNTR1;
  case 0x42:
    i_printf("inb [0x42] = 0x%02x  3rd timer inb\n",
	     pit.CNTR2--);
    return pit.CNTR2;
  case 0x2f8:			/* serial port */
  case 0x2f9:
  case 0x2fa:
  case 0x2fb:
  case 0x2fc:
  case 0x2fd:
  case 0x2fe:
  case 0x2ff:
    if (com[0].base_port == 0x2f8)
      return (do_serial_in(0, port));
    if (com[1].base_port == 0x2f8)
      return (do_serial_in(1, port));
    i_printf("Ser inb [0x%x] = 0x%02x\n",
	     port, (int) (REG(eax) & 0xFF));
    return (0);
  case 0x3ba:
  case 0x3da:
    /* graphic status - many programs will use this port to sync with
     * the vert & horz retrace so as not to cause CGA snow */
    return (cga_r ^= 1) ? 0xcf : 0xc6;
  case 0x3bc:
    i_printf("printer port inb [0x3bc] = 0\n");
    return 0;
  case 0x3db:			/* light pen strobe reset */
    return 0;
  case 0x3f8:
  case 0x3f9:
  case 0x3fa:
  case 0x3fb:
  case 0x3fc:
  case 0x3fd:
  case 0x3fe:
  case 0x3ff:			/* serial port */
    if (com[0].base_port == 0x3f8)
      return (do_serial_in(0, port));
    if (com[1].base_port == 0x3f8)
      return (do_serial_in(1, port));
    i_printf("Ser inb [0x%x] = 0x%02x\n",
	     port, (int) (REG(eax) & 0xFF));
    return (0);

  default:{
      int num;

      /* The diamond bug */
      if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
	iopl(3);
	tmp = port_in(port);
	iopl(0);
	i_printf(" Diamond inb [0x%x] = 0x%x\n", port, tmp);
	return (tmp);
      }

      i_printf("default inb [0x%x] = 0x%02x\n",
	       port, (int) (REG(eax) & 0xFF));
      return 0;
    }
  }
  return 0;
}

void
outb(int port, int byte)
{
  static int timer_beep = 0;
  static int lastport = 0;
  int num;

  port &= 0xffff;
  byte &= 0xff;

  if (port_writeable(port)) {
    write_port(byte, port);
    return;
  }

#if 1
  if (config.chipset && port > 0x3b3 && port < 0x3df)
    video_port_out(byte, port);
#endif

  /* The diamond bug */
  if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
    iopl(3);
    port_out(byte, port);
    iopl(0);
    i_printf(" Diamond outb [0x%x] = 0x%x\n", port, byte);
    return;
  }

  switch (port) {
  case 0x20:
    outb20 = 1;
    k_printf("OUTB resetting hard int! byte=%x\n", byte);
    break;
  case 0x60:
    k_printf("keyboard 0x60 outb = 0x%x\n", byte);
    microsoft_port_check = 1;
    if (byte < 0xf0) {
      microsoft_port_check = 0xfe;
    }
    else {
      microsoft_port_check = 0xfa;
    }
    break;
  case 0x64:
    k_printf("keyboard 0x64 outb = 0x%x\n", byte);
    break;
  case 0x61:
    port61 = byte & 0x0f;
    k_printf("8255 0x61 outb = 0x%x\n", byte);
    if (((byte & 3) == 3) && (timer_beep == 1) &&
	(config.speaker == SPKR_EMULATED)) {
      i_printf("beep!\n");
      fprintf(stderr, "\007");
      timer_beep = 0;
    }
    else {
      timer_beep = 1;
    }
    break;
  case 0x70:
  case 0x71:
    cmos_write(port, byte);
    break;
  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
    /*      i_printf("timer outb 0x%02x\n", byte); */
    if ((port == 0x42) && (lastport == 0x42)) {
      if ((timer_beep == 1) &&
	  (config.speaker == SPKR_EMULATED)) {
	fprintf(stderr, "\007");
	timer_beep = 0;
      }
      else {
	timer_beep = 1;
      }
    }
    break;
  case 0x2f8:			/* serial port */
  case 0x2f9:
  case 0x2fa:
  case 0x2fb:
  case 0x2fc:
  case 0x2fd:
  case 0x2fe:
  case 0x2ff:
    if (com[0].base_port == 0x2f8) {
      do_serial_out(0, port, byte);
      break;
    }
    if (com[1].base_port == 0x2f8) {
      do_serial_out(1, port, byte);
      break;
    }
    i_printf("Ser outb [0x%x] = 0x%02x\n",
	     port, (int) (REG(eax) & 0xFF));
    break;
  case 0x3f8:			/* serial port */
  case 0x3f9:
  case 0x3fa:
  case 0x3fb:
  case 0x3fc:
  case 0x3fd:
  case 0x3fe:
  case 0x3ff:
    if (com[0].base_port == 0x3f8) {
      do_serial_out(0, port, byte);
      break;
    }
    if (com[1].base_port == 0x3f8) {
      do_serial_out(1, port, byte);
      break;
    }
    i_printf("Ser outb [0x%x] = 0x%02x\n",
	     port, (int) (REG(eax) & 0xFF));
    break;
  default:
    i_printf("outb [0x%x] 0x%02x\n", port, byte);
  }

  lastport = port;
}

void
config_init(void)
{
  int b;

  configuration = 0;

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, and the
   * configured number of floppy disks
   */
  CONF_NFLOP(configuration, config.fdisks);
  CONF_NSER(configuration, config.num_ser);
  CONF_NLPT(configuration, config.num_lpt);
  if (config.mouse_flag)
    configuration |= CONF_MOUSE;/* XXX - is this PS/2 only? */

  if (config.mathco)
    configuration |= CONF_MATHCO;

  g_printf("CONFIG: 0x%04x    binary: ", configuration);
  for (b = 15; b >= 0; b--)
    dbug_printf("%s%s", (configuration & (1 << b)) ? "1" : "0",
		(b % 4) ? "" : " ");

  dbug_printf("\n");
}

void
dos_ctrl_alt_del(void)
{
  dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
  disk_close();
  clear_screen(SCREEN, 7);
  show_cursor();
  special_nowait = 0;
  p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
  boot();
}

void
dos_ctrlc(void)
{
  k_printf("DOS ctrl-c!\n");
  p_dos_str("^C\n\r");		/* print ctrl-c message */
  keybuf_clear();

  do_soft_int(0x23);
}

void
dosemu_banner(void)
{
  u_short *ssp = SEG_ADR((u_short *), ss, sp);

  *--ssp = REG(cs);
  *--ssp = REG(eip);
  REG(esp) -= 4;
  REG(cs) = Banner_SEG;
  REG(eip) = Banner_OFF;
}

#if 0
void
dbug_dumpivec(void)
{
  int i;

  for (i = 0; i < 256; i++) {
    int j;

    dbug_printf("%02x %08lx", i, ((unsigned long *) 0)[i << 1]);
    for (j = 0; j < 8; j++)
      dbug_printf(" %02x", ((unsigned char *) (BIOSSEG * 16 + 16 * i))[j]);
    dbug_printf("\n");
  }
}

#endif

void
boot()
{
  char *buffer;
  unsigned int i;
  unsigned char *ptr;
  struct disk *dp;

  switch (config.hdiskboot) {
  case 0:
    if (config.bootdisk)
      dp = &bootdisk;
    else
      dp = &disktab[0];
    break;
  case 1:
    dp = &hdisktab[0];
    break;
  case 2:
    dp = &disktab[2];
    break;
  default:
    error("ERROR: unexpected value for config.hdiskboot\n");
    leavedos(1);
  }

  config_init();
  cpu_init();
  cmos_init();

  ignore_segv++;

  disk_close();
  disk_open(dp);
  buffer = (char *) 0x7c00;

  /* fill the last page w/HLT, except leave the BIOS date & machine
   * type there if BIOS mapped in... */
  if (!config.mapped_sbios) {
    memset((char *) 0xffff0, 0xF4, 16);

    strncpy((char *) 0xffff5, "02/25/93", 8);	/* set our BIOS date */
    *(char *) 0xffffe = 0xfc;	/* model byte = IBM AT */
  }

  /* init trapped interrupts called via jump */
  for (i = 0; i < 256; i++) {
    if ((i & 0xf8) == 0x60)
      continue;			/* user interrupts */
    SETIVEC(i, BIOSSEG, 16 * i);
    ptr = (unsigned char *) (BIOSSEG * 16 + 16 * i);
    *ptr++ = 0xcd;
    *ptr++ = i;			/* 0xcd = INT */
    if ((i & 0xf8) == 8)	/* hardware interrupts */
      *ptr++ = 0xcf;		/* 0xcf = IRET */
    else {
      *ptr++ = 0xca;
      *ptr++ = 2;
      *ptr++ = 0;		/* 0xca = RETF */
    }
  }

  /* user timer tick, should be an IRET */
  *(unsigned char *) (BIOSSEG * 16 + 16 * 0x1c) = 0xcf;

  /* XMS has it's handler just after the interrupt dummy segment */
  ptr = (unsigned char *) (XMSControl_ADD);
  *ptr++ = 0xeb;		/* jmp short forward 3 */
  *ptr++ = 3;
  *ptr++ = 0x90;		/* nop */
  *ptr++ = 0x90;		/* nop */
  *ptr++ = 0x90;		/* nop */
  *ptr++ = 0xf4;		/* HLT...the current emulator trap */
  *ptr++ = INT2F_XMS_MAGIC;	/* just an info byte. reserved for later */
  *ptr++ = 0xcb;		/* FAR RET */
  /*  xms_init(); */

  /* show EMS as disabled */
  SETIVEC(0x67, 0, 0);

  /* this is the mouse handler */
  if (config.mouse_flag) {
    u_short *seg, *off;

    ptr = (unsigned char *) (Mouse_ADD);

    /* mouse routine simulates the stack frame of an int, then does a
       * "pushad" before here...so we just "popad; iret" to get back out
       */
    *ptr++ = 0xff;
    *ptr++ = 0x1e;
    *(((us *) ptr)++) = Mouse_OFF + 7;	/* uses ptr[3] as well */

    *ptr++ = 0x61;		/* popa */
    *ptr++ = 0xcf;		/* iret */

    *ptr++ = 0x27;		/* placeholder(offset) */
    *ptr++ = 0x02;		/* placeholder */
    off = (u_short *) (ptr - 2);
    *ptr++ = 0x81;		/* placeholder(segment) */
    *ptr++ = 0x1c;		/* placeholder */
    seg = (u_short *) (ptr - 2);

    /* tell the mouse driver where we are...exec add, seg, offset */
    mouse_sethandler(ptr, seg, off);
  }
#if 0
  else
    *(unsigned char *) (BIOSSEG * 16 + 16 * 0x33) = 0xcf;	/* IRET */
#endif

  ptr = (u_char *) Banner_ADD;
  *ptr++ = 0xb0;		/* mov al, 5 */
  *ptr++ = 0x05;
  *ptr++ = 0xcd;		/* int 0xe6 */
  *ptr++ = 0xe6;
  *ptr++ = 0xb2;		/* MOV DL, bootdrive */
  *ptr++ = config.hdiskboot ? 0x80 : 0;
  *ptr++ = 0xcb;		/* far ret */

  /* Welcome to an -inline- int16 routine - ask for details :-) */
  ptr = (u_char *) INT16_ADD;
  *ptr++ = 0xFB;
  *ptr++ = 0x1E;
  *ptr++ = 0x53;
  *ptr++ = 0xBB;
  *ptr++ = 0x40;
  *ptr++ = 0x00;
  *ptr++ = 0x8E;
  *ptr++ = 0xDB;
  *ptr++ = 0x0A;
  *ptr++ = 0xE4;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x00;
  *ptr++ = 0x74;
  *ptr++ = 0x2B;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x10;
  *ptr++ = 0x74;
  *ptr++ = 0x26;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x01;
  *ptr++ = 0x74;
  *ptr++ = 0x47;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x11;
  *ptr++ = 0x74;
  *ptr++ = 0x42;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x02;
  *ptr++ = 0x74;
  *ptr++ = 0x4E;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x03;
  *ptr++ = 0x74;
  *ptr++ = 0x0F;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x04;
  *ptr++ = 0x74;
  *ptr++ = 0x0A;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x05;
  *ptr++ = 0x74;
  *ptr++ = 0x56;
  *ptr++ = 0x80;
  *ptr++ = 0xFC;
  *ptr++ = 0x12;
  *ptr++ = 0x74;
  *ptr++ = 0x3A;
  *ptr++ = 0x5B;
  *ptr++ = 0x1F;
  *ptr++ = 0xCF;
  *ptr++ = 0xFA;
  *ptr++ = 0x8B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1A;
  *ptr++ = 0x00;
  *ptr++ = 0x3B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1C;
  *ptr++ = 0x00;
  *ptr++ = 0x75;
  *ptr++ = 0x03;
  *ptr++ = 0xFB;
  *ptr++ = 0xEB;
  *ptr++ = 0xF2;
  *ptr++ = 0x8B;
  *ptr++ = 0x07;
  *ptr++ = 0x43;
  *ptr++ = 0x43;
  *ptr++ = 0x89;
  *ptr++ = 0x1E;
  *ptr++ = 0x1A;
  *ptr++ = 0x00;
  *ptr++ = 0x3B;
  *ptr++ = 0x1E;
  *ptr++ = 0x82;
  *ptr++ = 0x00;
  *ptr++ = 0x75;
  *ptr++ = 0xE1;
  *ptr++ = 0x8B;
  *ptr++ = 0x1E;
  *ptr++ = 0x80;
  *ptr++ = 0x00;
  *ptr++ = 0x89;
  *ptr++ = 0x1E;
  *ptr++ = 0x1A;
  *ptr++ = 0x00;
  *ptr++ = 0xEB;
  *ptr++ = 0xD7;
  *ptr++ = 0xFA;
  *ptr++ = 0x8B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1A;
  *ptr++ = 0x00;
  *ptr++ = 0x3B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1C;
  *ptr++ = 0x00;
  *ptr++ = 0x8B;
  *ptr++ = 0x07;
  *ptr++ = 0xFB;
  *ptr++ = 0x5B;
  *ptr++ = 0x1F;
  *ptr++ = 0xCA;
  *ptr++ = 0x02;
  *ptr++ = 0x00;
  *ptr++ = 0xA0;
  *ptr++ = 0x17;
  *ptr++ = 0x00;
  *ptr++ = 0x8A;
  *ptr++ = 0x26;
  *ptr++ = 0x18;
  *ptr++ = 0x00;
  *ptr++ = 0x80;
  *ptr++ = 0xE4;
  *ptr++ = 0xF3;
  *ptr++ = 0x33;
  *ptr++ = 0xDB;
  *ptr++ = 0x8A;
  *ptr++ = 0x3E;
  *ptr++ = 0x96;
  *ptr++ = 0x00;
  *ptr++ = 0x80;
  *ptr++ = 0xE7;
  *ptr++ = 0x0C;
  *ptr++ = 0x0B;
  *ptr++ = 0xC3;
  *ptr++ = 0xEB;
  *ptr++ = 0xAF;
  *ptr++ = 0xFA;
  *ptr++ = 0x8B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1C;
  *ptr++ = 0x00;
  *ptr++ = 0x43;
  *ptr++ = 0x43;
  *ptr++ = 0x3B;
  *ptr++ = 0x1E;
  *ptr++ = 0x82;
  *ptr++ = 0x00;
  *ptr++ = 0x75;
  *ptr++ = 0x04;
  *ptr++ = 0x8B;
  *ptr++ = 0x1E;
  *ptr++ = 0x80;
  *ptr++ = 0x00;
  *ptr++ = 0x3B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1A;
  *ptr++ = 0x00;
  *ptr++ = 0x75;
  *ptr++ = 0x05;
  *ptr++ = 0xB0;
  *ptr++ = 0x01;
  *ptr++ = 0xFB;
  *ptr++ = 0xEB;
  *ptr++ = 0x93;
  *ptr++ = 0x53;
  *ptr++ = 0x8B;
  *ptr++ = 0x1E;
  *ptr++ = 0x1C;
  *ptr++ = 0x00;
  *ptr++ = 0x89;
  *ptr++ = 0x0F;
  *ptr++ = 0xB0;
  *ptr++ = 0x00;
  *ptr++ = 0x5B;
  *ptr++ = 0x89;
  *ptr++ = 0x1E;
  *ptr++ = 0x1C;
  *ptr++ = 0x00;
  *ptr++ = 0xFB;
  *ptr++ = 0xEB;
  *ptr++ = 0x82;
  SETIVEC(0x16, INT16_SEG, INT16_OFF);
  /* End of REAL MODE interrupt 16 */

  /* Welcome to an -inline- int09 routine - ask for details :-) */
  ptr = (u_char *) INT09_ADD;
  *ptr++ = 0xFA;

  *ptr++ = 0x9c;

  *ptr++ = 0x1E;
  *ptr++ = 0x53;
  *ptr++ = 0x50;

  *ptr++ = 0xE4;
  *ptr++ = 0x60;

  *ptr++ = 0x8a;
  *ptr++ = 0xe0;
  *ptr++ = 0xb0;
  *ptr++ = 0x07;
  *ptr++ = 0xcd;
  *ptr++ = 0xe6;

  *ptr++ = 0xB4;
  *ptr++ = 0x4F;
  *ptr++ = 0xF8;
  *ptr++ = 0xF5;
  *ptr++ = 0xCD;
  *ptr++ = 0x15;

  *ptr++ = 0x9c;

  *ptr++ = 0x50;
  *ptr++ = 0x5b;

  *ptr++ = 0x8A;
  *ptr++ = 0xE0;
  *ptr++ = 0xB0;
  *ptr++ = 0x06;

  *ptr++ = 0x9d;

  *ptr++ = 0xCD;
  *ptr++ = 0xE6;
  *ptr++ = 0x58;
  *ptr++ = 0x5B;
  *ptr++ = 0x1F;

  *ptr++ = 0x9d;

  *ptr++ = 0xFB;
  *ptr++ = 0xCF;
  SETIVEC(0x09, INT09_SEG, INT09_OFF);
  /* End of REAL MODE interrupt 9 */

  /* Wrapper around call to video init c000:0013 */
  ptr = (u_char *) INT10_ADD;

  *ptr++ = 0x50;

  *ptr++ = 0xb0;
  *ptr++ = 0x08;
  *ptr++ = 0xcd;
  *ptr++ = 0xe6;

  *ptr++ = 0x9a;
  *ptr++ = 0x03;
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0xc0;

  *ptr++ = 0xb0;
  *ptr++ = 0x09;
  *ptr++ = 0xcd;
  *ptr++ = 0xe6;

  *ptr++ = 0x58;
  *ptr++ = 0xcb;

  /* This is an int e7 used for FCB opens */
  ptr = (u_char *) INTE7_ADD;
  *ptr++ = 0x06;
  *ptr++ = 0x57;
  *ptr++ = 0x50;
  *ptr++ = 0xb8;		/* mov ax, 0x120c */
  *ptr++ = 0x0c;
  *ptr++ = 0x12;
  *ptr++ = 0xcd;		/* int 0x2f */
  *ptr++ = 0x2f;
  *ptr++ = 0x58;
  *ptr++ = 0x5f;
  *ptr++ = 0x07;
  *ptr = 0xcf;			/* IRET */
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  /* End of int 0xe7 for FCB opens */

  /* A call from a DPMI program to go protected will go here, a HLT */
  ptr = (u_char *) DPMI_ADD;
  *ptr++ = 0x50;		/* push ax */
  *ptr++ = 0xb4;
  *ptr++ = 0x51;
  *ptr++ = 0xCD;
  *ptr++ = 0x21;		/* Get PSP; BX = PSP */
  *ptr++ = 0x58;		/* pop ax */
  *ptr++ = 0xF4;		/* hlt */
  *ptr++ = 0xF4;		/* hlt *//* for Return from DOS Interrupt */
  *ptr = 0xcb;			/* FAR RET */

  /* set up BIOS exit routine (we have *just* enough room for this) */
  ptr = (u_char *) 0xffff0;
  *ptr++ = 0xb8;		/* mov ax, 0xffff */
  *ptr++ = 0xff;
  *ptr++ = 0xff;
  *ptr++ = 0xcd;		/* int 0xe6 */
  *ptr++ = 0xe6;

  /* set up relocated video handler (interrupt 0x42) */
  *(u_char *) 0xff065 = 0xcf;	/* IRET */

  /* TRB - initialize a helper routine for IPX in boot() */
#ifdef IPX
  if (config.ipxsup)
    InitIPXFarCallHelper();
#endif

  *(us *) 0x410 = configuration;
  *(us *) 0x413 = config.mem_size;	/* size of memory */
  VIDMODE = screen_mode;	/* screen mode */
  COLS = CO;			/* chars per line */
  ROWSM1 = LI - 1;		/* lines on screen - 1 */
  REGEN_SIZE = TEXT_SIZE;	/* XXX - size of video regen area in bytes */
  PAGE_OFFSET = 0;		/* offset of current page in buffer */

  /* The default 16-word BIOS key buffer starts at 0x41e */
  KBD_Head =			/* key buf start ofs */
    KBD_Tail =			/* key buf end ofs */
    KBD_Start = 0x1e;		/* keyboard queue start... */
  KBD_End = 0x3e;		/* ...and end offsets from 0x400 */

  keybuf_clear();

  *(us *) 0x463 = 0x3d4;	/* base port of CRTC - IMPORTANT! */
  *(char *) 0x465 = 9;		/* current 3x8 (x=b or d) value */
  *(char *) 0x472 = 0x1234;
  *(char *) 0x487 = 0x61;
  *(char *) 0x488 = 0x81;	/* video display data */

  *(char *) 0x48a = video_combo;/* video type */

  *(char *) 0x496 = 16;		/* 102-key keyboard */
  *(long *) 0x4a8 = 0;		/* pointer to video table */

  /* from Alan Cox's mods */
  /* we need somewhere for the bios equipment. */

  ptr = (u_char *) ROM_CONFIG_ADD;
  *ptr++ = 0x09;
  *ptr++ = 0x00;		/* 9 byte table */
  *ptr++ = 0xFC;		/* PC AT */
  *ptr++ = 0x01;
  *ptr++ = 0x04;		/* bios revision 4 */
  *ptr++ = 0x70;		/* no mca, no ebios, no wat, keybint,
				   rtc, slave 8259, no dma 3 */
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x00;

  if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
    error("ERROR: can't boot from %s, using harddisk\n", dp->dev_name);
    dp = hdisktab;
    if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
      error("ERROR: can't boot from hard disk\n");
      leavedos(1);
    }
  }
  disk_close();

  REG(ebx) = 0;			/* ax,bx,cx,dx,si,di,bp,fs,gs probably can be anything */
  REG(ecx) = 0;
  REG(edx) = 0;
  REG(esi) = 0;
  REG(edi) = 0;
  REG(ebp) = 0;
  REG(eax) = 0;
  REG(eip) = 0x7c00;
  REG(cs) = 0;			/* Some boot sectors require cs=0 */
  REG(esp) = 0x100;
  REG(ss) = 0x30;		/* This is the standard pc bios stack */
  REG(es) = 0;			/* standard pc es */
  REG(ds) = 0x40;		/* standard pc ds */
  REG(fs) = 0;
  REG(gs) = 0;

  update_flags(&REG(eflags));

  /* the banner helper actually get called *after* the VGA card
   * is initialized (if it is) because we set up a return chain:
   *      init_vga_card -> dosemu_banner -> 7c00:0000 (boot block)
   */

  if (config.dosbanner)
    dosemu_banner();

  if (config.vga) {
    g_printf("INITIALIZING VGA CARD BIOS!\n");
    init_vga_card();
  }

  if (exitearly) {
    dbug_printf("Leaving DOS before booting\n");
    leavedos(0);
  }
  ignore_segv--;
}

/* Once every 18 let's update leds */
u_char leds = 0;

void
int08(void)
{
#if 1
  if (++leds >= 18) {
    set_leds();
    leds = 0;
  }
#endif
  run_int(0x1c);
  /*	printf("Run 0x1c\n"); */
  outb20 = 1;
  return;
}

void
int15(void)
{
  struct timeval wait_time;
  int num;

  if (HI(ax) != 0x4f)
    NOCARRY;

  switch (HI(ax)) {
  case 0x41:			/* wait on external event */
    break;
  case 0x4f:			/* Keyboard intercept */
    HI(ax) = 0x86;
    k_printf("INT15 0x4f CARRY=%x AX=%x\n", REG(eflags) & CF, REG(eax));
    CARRY;
    /*
    if (LO(ax) & 0x80 )
      if (1 || !(LO(ax)&0x80) ){
	fprintf(stderr, "Carrying it out\n");
        CARRY;
      }
      else
	NOCARRY;
*/
    break;
  case 0x80:			/* default BIOS device open event */
    REG(eax) &= 0x00FF;
    return;
  case 0x81:
    REG(eax) &= 0x00FF;
    return;
  case 0x82:
    REG(eax) &= 0x00FF;
    return;
  case 0x83:
    h_printf("int 15h event wait:\n");
    show_regs();
    CARRY;
    return;			/* no event wait */
  case 0x84:
    CARRY;
    return;			/* no joystick */
  case 0x85:
    num = REG(eax) & 0xFF;	/* default bios handler for sysreq key */
    if (num == 0 || num == 1) {
      REG(eax) &= 0x00FF;
      return;
    }
    REG(eax) &= 0xFF00;
    REG(eax) |= 1;
    CARRY;
    return;
  case 0x86:
    /* wait...cx:dx=time in usecs */
    g_printf("doing int15 wait...ah=0x86\n");
    show_regs();
    wait_time.tv_sec = 0;
    wait_time.tv_usec = (LWORD(ecx) << 16) | LWORD(edx);
    RPT_SYSCALL(select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv));
    NOCARRY;
    return;

  case 0x87:
    if (config.xms_size)
      xms_int15();
    else {
      REG(eax) &= 0xFF;
      REG(eax) |= 0x0300;	/* say A20 gate failed - a lie but enough */
      CARRY;
    }
    return;

  case 0x88:
    if (config.xms_size) {
      xms_int15();
    }
    else {
      REG(eax) &= ~0xffff;	/* we don't have extended ram if it's not XMS */
      NOCARRY;
    }
    return;

  case 0x89:			/* enter protected mode : kind of tricky! */
    REG(eax) |= 0xFF00;		/* failed */
    CARRY;
    return;
  case 0x90:			/* no device post/wait stuff */
    CARRY;
    return;
  case 0x91:
    CARRY;
    return;
  case 0xc0:
    REG(es) = ROM_CONFIG_SEG;
    LWORD(ebx) = ROM_CONFIG_OFF;
    LO(ax) = 0;
    return;
  case 0xc1:
    CARRY;
    return;			/* no ebios area */
  case 0xc2:
    REG(eax) &= 0x00FF;
    REG(eax) |= 0x0300;		/* interface error if use a ps2 mouse */
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
  /* TRB - catch interrupt 2F to detect IPX in int2f() */
#ifdef IPX
  if (config.ipxsup) {
    if (LWORD(eax) == INT2F_DETECT_IPX) {
      return (IPXInt2FHandler());
    }
  }
#endif

  /* this is the DOS give up time slice call...*/
  if (LWORD(eax) == INT2F_IDLE_MAGIC) {
    usleep(INT2F_IDLE_USECS);
    return 1;
  }

  /* is it a redirector call ? */
  else if (HI(ax) == 0x11 && mfs_redirector())
    return 1;

  else if (HI(ax) == INT2F_XMS_MAGIC) {
    if (!config.xms_size)
      return 0;
    switch (LO(ax)) {
    case 0:			/* check for XMS */
      x_printf("Check for XMS\n");
      xms_grab_int15 = 0;
      LO(ax) = 0x80;
      break;
    case 0x10:
      x_printf("Get XMSControl address\n");
      REG(es) = XMSControl_SEG;
      LWORD(ebx) = XMSControl_OFF;
      break;
    default:
      x_printf("BAD int 0x2f XMS function:0x%02x\n", LO(ax));
    }
    return 1;
  }

#ifdef DPMI
  /* Call for getting DPMI entry point */
  else if (LWORD(eax) == 0x1687) {

    dpmi_get_entry_point();
    return 1;

  }
  /* Are we in protected mode ? */
  else if (LWORD(eax) == 0x1686) {

    if (in_dpmi)
      REG(eax) = 0;
    D_printf("DPMI 1686 returns %x\n", REG(eax));
    return 1;

  }
#endif

  return 0;
}

void
int1a(void)
{
  unsigned long ticks;
  long akt_time, elapsed;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm;

  switch (HI(ax)) {

    /* A timer read should reset the overflow flag */
  case 0:			/* read time counter */
    time(&akt_time);
    elapsed = akt_time - start_time;
    ticks = (elapsed * 182) / 10 + last_ticks;
    LO(ax) = *(u_char *) (TICK_OVERFLOW_ADDR);
    *(u_char *) (TICK_OVERFLOW_ADDR) = 0;
    LWORD(ecx) = ticks >> 16;
    LWORD(edx) = ticks & 0xffff;
    /* dbug_printf("read timer st: %ud %ud t=%d\n",
				    start_time, ticks, akt_time); */
    break;
  case 1:			/* write time counter */
    last_ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);
    set_ticks(last_ticks);
    time(&start_time);
    g_printf("set timer to %lu \n", last_ticks);
    break;
  case 2:			/* get time */
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
    tm = localtime((time_t *) & ticks);
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
    REG(eflags) &= ~CF;
    break;
  case 4:			/* get date */
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
    tm = localtime((time_t *) & ticks);
    tm->tm_year += 1900;
    tm->tm_mon++;
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
    REG(eflags) &= ~CF;
    break;
  case 3:			/* set time */
  case 5:			/* set date */
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
int
ms_dos(int nr)
{				/* returns 1 if emulated, 0 if internal handling */
  unsigned int c;

  /* int 21, ah=1,7,or 8:  return 0 for an extended keystroke, but the
   next call returns the scancode */
  /* if I press and hold a key like page down, it causes the emulator to
   exit! */

Restart:
  /* dbug_printf("DOSINT 0x%02x\n", nr); */
  /* emulate keyboard io to avoid DOS' busy wait */

  switch (nr) {

    /* define NO_FIX_STDOUT if you want faster dos screen output, at the
 * expense of being able to redirect things.  I don't seriously consider
 * leaving this in for real, but I just don't feel like taking it out
 * yet...I'd forget it if I did.
 */
#if DOS_FLUSH
  case 7:			/* read char, do not check <ctrl> C */
  case 1:			/* read and echo char */
  case 8:			/* read char */
    /* k_printf("KBD/DOS: int 21h ah=%d\n", nr); */
    disk_close();		/* DISK */
    return 0;			/* say not emulated */
    break;

  case 10:			/* read string */
    disk_close();		/* DISK */
    return 0;
    break;
#endif

  case 12:			/* clear key buffer, do int AL */
    while (CReadKeyboard(&c, NOWAIT) == 1) ;
    nr = LO(ax);
    if (nr == 0)
      break;			/* thanx to R Michael McMahon for the hint */
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

#ifndef USE_NCURSES
#define FALSE 0
#define TRUE 1
#endif

    /* lowercase and truncate to 3 letters the replacement extension */
#define ext_fix(s) { char *r=(s); \
		     while (*r) { *r=toupper(*r); r++; } \
		     if ((r - s) > 3) s[3]=0; }

    /* we trap this for two functions: simulating the EMMXXXX0
	 * device, and fudging the CONFIG.XXX and AUTOEXEC.XXX
	 * bootup files
	 */
  case DOS_HANDLE_OPEN:{
      char *ptr = SEG_ADR((char *), ds, dx);

#ifdef INTERNAL_EMS
      if (!strncmp(ptr, "EMMXXXX0", 8) && config.ems_size) {
	E_printf("EMS: opened EMM file!\n");
	LWORD(eax) = EMM_FILE_HANDLE;
	NOCARRY;
	show_regs();
	return (1);
      }
      else
#endif
      if (!strncmp(ptr, "\\CONFIG.SYS", 11) && config.emusys) {
	ext_fix(config.emusys);
	sprintf(ptr, "\\CONFIG.%-3s", config.emusys);
	d_printf("DISK: Substituted %s for CONFIG.SYS\n", ptr);
      }
      /* ignore explicitly selected drive by incrementing ptr by 1 */
      else if (!strncmp(ptr + 1, ":\\AUTOEXEC.BAT", 14) && config.emubat) {
	ext_fix(config.emubat);
	sprintf(ptr + 1, ":\\AUTOEXEC.%-3s", config.emubat);
	d_printf("DISK: Substituted %s for AUTOEXEC.BAT\n", ptr + 1);
      }

      return (0);
    }

#ifdef INTERNAL_EMS
  case DOS_HANDLE_CLOSE:
    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
      return (0);
    else {
      E_printf("EMS: closed EMM file!\n");
      NOCARRY;
      show_regs();
      return (1);
    }

  case DOS_IOCTL:
    {

      if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
	return (FALSE);

      switch (LO(ax)) {
      case IOCTL_GET_DEV_INFO:
	E_printf("EMS: dos_ioctl getdevinfo emm.\n");
	LWORD(edx) = 0x80;
	NOCARRY;
	show_regs();
	return (TRUE);
	break;
      case IOCTL_CHECK_OUTPUT_STS:
	E_printf("EMS: dos_ioctl chkoutsts emm.\n");
	LO(ax) = 0xff;
	NOCARRY;
	show_regs();
	return (TRUE);
	break;
      }
      error("ERROR: dos_ioctl shouldn't get here. XXX\n");
      return (FALSE);
    }
#endif

#ifdef DPMI
    /* DPMI must leave protected mode with an int21 4c call */
  case 0x4c:
    if (in_dpmi) {
      D_printf("Leaving DPMI with err of 0x%02x\n", LO(ax));
      in_dpmi_dos_int = 0;
      in_dpmi = 0;
    }
    return 0;
#endif

  default:
    /* taking this if/printf out will speed things up a bit...*/
    if (d.dos)
      dbug_printf(" dos interrupt 0x%02x, ax=0x%04x, bx=0x%04x\n",
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
  static int time_count = 0;

  if (i == 0x16) {

    if (time_count == 0)
      gettimeofday(&tp1, NULL);
    else {
      gettimeofday(&tp2, NULL);
      if ((tp2.tv_sec - tp1.tv_sec) * 1000000 +
	  ((int) tp2.tv_usec - (int) tp1.tv_usec) < config.hogthreshold)
	usleep(100);
    }
    time_count = (time_count + 1) % 10;
  }

#if 1
  /* XXX bootstrap cs is now 0 */
  if (!(REG(eip) && REG(cs))) {
    error("run_int: not running NULL interrupt 0x%04x handler\n", i);
    show_regs();
    return 0;
  }
#endif

  /* make sure that the pushed flags match out "virtual" flags */
  update_flags(&REG(eflags));

  if (!LWORD(esp))
    ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
  else
    ssp = SEG_ADR((us *), ss, sp);
  *--ssp = REG(eflags);
  *--ssp = REG(cs);
  *--ssp = REG(eip);
  REG(esp) -= 6;
  REG(cs) = ((us *) 0)[(i << 1) + 1];
  REG(eip) = ((us *) 0)[i << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
  REG(eflags) &= ~(TF | IF | NT);
  return 0;
}

inline int
can_revector(int i)
{
  /* here's sort of a guideline:
 * if we emulate it completely, but there is a good reason to stick
 * something in front of it, and it seems to work, by all means revector it.
 * if we emulate none of it, say yes, as that is a little bit faster.
 * if we emulate it, but things don't seem to work when it's revectored,
 * then don't let it be revectored.
 *
 */

  switch (i) {
    /* some things, like 0x29, need to be unrevectored if the vectors
       * are the DOS vectors...but revectored otherwise
       */
#if MOUSE
  case 0x33:			/* mouse...we're testing */
#endif
  case 0x21:			/* we want it first...then we'll pass it on */
  case 0x28:			/* same here */
  case 0x2a:
#undef FAST_BUT_WRONG29
#ifdef FAST_BUT_WRONG29
  case 0x29:			/* DOS fast char output... */
#endif
  case 0x2f:			/* needed for XMS, redirector, and idle interrupt */
  case 0x67:			/* EMS */
  case 0xe6:			/* for redirector and helper (was 0xfe) */
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
  case 0x15:
  case 0x25:			/* absolute disk read, calls int 13h */
  case 0x26:			/* absolute disk write, calls int 13h */
  case 0x1b:			/* ctrl-break handler */
  case 0x1c:			/* user timer tick */
  case 0x16:			/* BIOS keyboard */
  case 0x17:			/* BIOS printer */
  case 0x10:			/* BIOS video */
  case 0x13:			/* BIOS disk */
  case 0x27:			/* TSR */
  case 0x20:			/* exit program */
  case 0xfe:			/* Turbo Debugger and others... */
    return 1;
  default:
    g_printf("revectoring 0x%02x\n", i);
    return 1;
  }
}

void
do_int(int i)
{
  int highint = 0;

#if 0
  saytime("do_int");
#endif

#if 0
  dbug_printf("Do INT0x%02x, eax=0x%04x, ebx=0x%04x ss=0x%04x esp=0x%04x cs=0x%04x ip=0x%04x CF=%d \n", i, REG(eax), REG(ebx), REG(ss), REG(esp), REG(cs), REG(eip), REG(eflags) & CF);
#endif

  in_interrupt++;

  if ((REG(cs) & 0xffff) == BIOSSEG) {
    highint = 1;
  }
  else if (IS_REDIRECTED(i) && can_revector(i) /* && !IS_IRET(i) */ ) {
    run_int(i);
    return;
  }

  switch (i) {
  case 0x5:
    g_printf("BOUNDS exception\n");
    goto default_handling;
    return;
  case 0x0a:
  case 0x0b:			/* com2 interrupt */
  case 0x0c:			/* com1 interrupt */
  case 0x0d:
  case 0x0e:
  case 0x0f:
    g_printf("IRQ->interrupt %x\n", i);
    goto default_handling;
  case 0x09:			/* IRQ1, keyb data ready */
    fprintf(stderr, "IRQ->interrupt %x\n", i);
    run_int(0x09);
    return;
  case 0x08:
    int08();
    return;
    goto default_handling;
  case 0x10:			/* VIDEO */
    int10();
    return;
  case 0x11:			/* CONFIGURATION */
    LWORD(eax) = configuration;
    return;
  case 0x12:			/* MEMORY */
    LWORD(eax) = config.mem_size;
    return;
  case 0x13:			/* DISK IO */
    int13();
    return;
  case 0x14:			/* COM IO */
    int14();
    return;
  case 0x15:			/* Cassette */
    int15();
    return;
  case 0x16:			/* KEYBOARD */

#if 0
    fprintf(stderr, "Goinf IN\n");
    fprintf(stderr, "*DS:1A=0x%05x\n", *(us *) ((0x40 << 4) + 0x1a));
    fprintf(stderr, "*DS:1C=0x%05x\n", *(us *) ((0x40 << 4) + 0x1c));
    fprintf(stderr, "*DS:82=0x%05x\n", *(us *) ((0x40 << 4) + 0x82));
    fprintf(stderr, "*DS:80=0x%05x\n", *(us *) ((0x40 << 4) + 0x80));
#endif

    run_int(0x16);
    return;
  case 0x17:			/* PRINTER */
    int17();
    return;
  case 0x18:			/* BASIC */
    break;
  case 0x19:			/* LOAD SYSTEM */
    boot();
    return;
  case 0x1a:			/* CLOCK */
    int1a();
    return;
#if 0
  case 0x1b:			/* BREAK */
  case 0x1c:			/* TIMER */
  case 0x1d:			/* SCREEN INIT */
  case 0x1e:			/* DEF DISK PARMS */
  case 0x1f:			/* DEF GRAPHIC */
  case 0x20:			/* EXIT */
  case 0x27:			/* TSR */
#endif

  case 0x21:			/* MS-DOS */
    if (ms_dos(HI(ax)))
      return;
    /* else do default handling in vm86 mode */
    goto default_handling;

  case 0x28:			/* KEYBOARD BUSY LOOP */
    {
      static int first = 1;

      if (first && !config.keybint && config.console_keyb) {
	first = 0;
	/* revector int9, so dos doesn't play with the keybuffer */
	k_printf("revectoring int9 away from dos\n");
	SETIVEC(0x9, BIOSSEG, 16 * 0x8 + 2);	/* point to IRET */
      }
    }
    if (int28())
      return;
    goto default_handling;
    return;

  case 0x29:			/* FAST CONSOLE OUTPUT */
    char_out(*(char *) &REG(eax), SCREEN, ADVANCE);	/* char in AL */
    return;

  case 0x2a:			/* CRITICAL SECTION */
    goto default_handling;

  case 0x2f:			/* Multiplex */
    if (int2f())
      return;
    goto default_handling;

  case 0x33:			/* mouse */
#if MOUSE
    warning("doing dosemu's mouse interrupt!\n");
    if (config.mouse_flag)
      mouse_int();
#else
    goto default_handling;
#endif
    return;
    break;

  case 0x67:			/* EMS */
    goto default_handling;

  case 0xe6:			/* dos helper and mfs startup (was 0xfe) */
    if (dos_helper())
      return;
    goto default_handling;
    return;

  case 0xe7:			/* dos helper and mfs startup (was 0xfe) */
    SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
    run_int(0xe7);
    return;

  default:
    if (d.defint)
      dbug_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));
    /* fall through */

  default_handling:

    if (highint) {
      in_interrupt--;
      return;
    }

    if (!IS_REDIRECTED(i)) {
      g_printf("DEFIVEC: int 0x%02x  SG: 0x%04x  OF: 0x%04x\n",
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

    run_int(i);

    return;
  }
  error("\nERROR: int 0x%02x not implemented\n", i);
  show_regs();
  fatalerr = 1;
  return;
}

void
sigalrm(int sig)
{
  static int running = 0;
  static inalrm = 0;
  static int partials = 0;

  in_vm86 = 0;

  if (inalrm) {
    error("ERROR: Reentering SIGALRM!\n");
    return;
  }

  inalrm = 1;
  in_sighandler = 1;

  if (((vm86s.screen_bitmap & scrtest_bitmap && !config.console_video) ||
       (update_screen && !config.console_video)) && !running) {
    running = 1;
    restore_screen();
    vm86s.screen_bitmap = 0;
    running = 0;
  }

  setitimer(TIMER_TIME, &itv, NULL);

  /* if (config.mouse_flag) mouse_curtick(); */

  /* update the Bios Data Area timer dword if interrupts enabled */
  if (cpu.iflag)
    timer_tick();

  /* TRB - perform processing for the IPX Asynchronous Event Service */
#ifdef IPX
  if (config.ipxsup)
    AESTimerTick();
#endif

  /* If linpkt has been loaded and an Access Type call has been made */
  if (pd_sock)
    pd_receive_packet();

  /* this is severely broken */
  if (config.timers) {
    h_printf("starting timer int 8...\n");
    if (!do_hard_int(8))
      h_printf("CAN'T DO TIMER INT 8...IF CLEAR\n");
  }
  else
    error("NOT CONFIG.TIMERS\n");

  /* this is for per-second activities */
  partials++;
  if (partials == FREQ) {
    partials = 0;
    printer_tick((u_long) 0);
    if (config.fastfloppy)
      floppy_tick();
  }

  in_sighandler = 0;
  inalrm = 0;
}

void
sigchld(int sig)
{
  pid_t pid;
  int status;

  in_vm86 = 0;

  pid = wait(&status);

  if (pid == ipc_pid) {
    char *exitmethod;
    int exitnum;

    if (WIFEXITED(status)) {
      exitmethod = "exited with return value";
      exitnum = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status)) {
      exitmethod = "died from signal";
      exitnum = WTERMSIG(status);
    }
    else if (WIFSTOPPED(status)) {
      exitmethod = "was stopped by signal";
      exitnum = WSTOPSIG(status);
    }
    else {
      exitmethod = "unknown termination method";
      exitnum = status;
    }
    error("ERROR: main IPC process pid %ld %s %d! dosemu terminating...\n",
	  (long) pid, exitmethod, exitnum);
    leavedos(1);
  }
}

void
sigquit(int sig)
{
  in_vm86 = 0;
  in_sighandler = 1;

  error("ERROR: sigquit called\n");
  show_ints(0, 0x33);
  show_regs();

  ignore_segv++;
  *(unsigned char *) 0x471 = 0x80;	/* ctrl-break flag */
  ignore_segv--;

  do_soft_int(0x1b);
  in_sighandler = 0;
}

void
timint(int sig)
{
  in_vm86 = 0;
  in_sighandler = 1;

  warn("timint called: %04x:%04x -> %05x\n", ISEG(8), IOFF(8), IVEC(8));
  warn("(vec 0x1c)     %04x:%04x -> %05x\n", ISEG(0x1c), IOFF(0x1c),
       IVEC(0x1c));
  show_regs();

  do_hard_int(0x8);

  in_sighandler = 0;
}

void
open_terminal_pipe(char *path)
{
  terminal_fd = DOS_SYSCALL(open(path, O_RDWR));
  if (terminal_fd == -1) {
    terminal_pipe = 0;
    error("ERROR: open_terminal_pipe failed - cannot open %s!\n", path);
    return;
  }
  else
    terminal_pipe = 1;
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
  unsigned char flag = 1;
  const char allopts[] = "vsdDRWkpiwghxmIEc";

  /* if you add new classes of debug messages, make sure to add the
 * letter to the allopts string above so that "1" and "a" can work
 * correctly.
 */

  dbug_printf("debug flags: %s\n", s);
  while ((c = *(s++)))
    switch (c) {
    case '+':			/* begin options to turn on */
      if (!flag)
	flag = 1;
      break;
    case '-':			/* begin options to turn off */
      flag = 0;
      break;
    case 'v':			/* video */
      d.video = flag;
      break;
    case 's':			/* serial */
      d.serial = flag;
      break;
    case 'c':			/* disk */
      d.config = flag;
      break;
    case 'd':			/* disk */
      d.disk = flag;
      break;
    case 'R':			/* disk READ */
      d.read = flag;
      break;
    case 'W':			/* disk WRITE */
      d.write = flag;
      break;
    case 'k':			/* keyboard */
      d.keyb = flag;
      break;
    case 'p':			/* printer */
      d.printer = flag;
      break;
    case 'i':			/* i/o instructions (in/out) */
      d.io = flag;
      break;
    case 'w':			/* warnings */
      d.warning = flag;
      break;
    case 'g':			/* general messages */
      d.general = flag;
      break;
    case 'x':			/* XMS */
      d.xms = flag;
      break;
    case 'D':			/* DPMI */
      d.dpmi = flag;
      break;
    case 'm':			/* mouse */
      d.mouse = flag;
      break;
    case 'a':{			/* turn all on/off depending on flag */
	char *newopts = (char *) malloc(strlen(allopts) + 2);

	d.all = flag;
	newopts[0] = flag ? '+' : '-';
	newopts[1] = 0;
	strcat(newopts, allopts);
	parse_debugflags(newopts);
	free(newopts);
	break;
      }
    case 'h':			/* hardware */
      d.hardware = flag;
      break;
    case 'I':			/* IPC */
      d.IPC = flag;
      break;
    case 'E':			/* EMS */
      d.EMS = flag;
      break;
    case '0'...'9':		/* set debug level, 0 is off, 9 is most verbose */
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
void
flip_disks(char *flipstr)
{
  struct disk temp;
  int i1, i2;

  if ((strlen(flipstr) != 3) || !isdigit(flipstr[0]) || !isdigit(flipstr[2])) {
    error("ERROR: flips string (%s) incorrect format\n", flipstr);
    return;
  }

  /* shouldn't rely on the ASCII character set here :-)
   * this makes disks go as 1,2,3...
   */
  i1 = flipstr[0] - '1';
  i2 = flipstr[2] - '1';

  g_printf("FLIPPED disks %s and %s\n", disktab[i1].dev_name,
	   disktab[i2].dev_name);

  /* no range checking! bad Robert */
  temp = disktab[i1];
  disktab[i1] = disktab[i2];
  disktab[i2] = temp;
}

void
config_defaults(void)
{
  config.hdiskboot = 1;		/* default hard disk boot */

  config.mem_size = 640;
  config.ems_size = 0;
  config.xms_size = 0;
  config.dpmi_size = 0;
  config.mathco = 1;
  config.mouse_flag = 0;
  config.mapped_bios = 0;
  config.mapped_sbios = 0;
  config.vbios_file = NULL;
  config.vbios_copy = 0;
  config.console_keyb = 0;
  config.console_video = 0;
  config.fdisks = 0;
  config.hdisks = 0;
  config.bootdisk = 0;
  config.exitearly = 0;
  config.redraw_chunks = 1;
  config.hogthreshold = 20000;	/* in usecs */
  config.chipset = PLAINVGA;
  config.cardtype = CARD_VGA;
  config.fullrestore = 0;
  config.graphics = 0;
  config.gfxmemsize = 256;
  config.vga = 0;		/* this flags BIOS graphics */

  config.speaker = SPKR_EMULATED;

  config.update = 54945;
  config.freq = 18;		/* rough frequency */

  config.timers = 1;		/* deliver timer ints */
  config.keybint = 0;		/* no keyboard interrupt */

  config.num_ser = 0;
  config.num_lpt = 0;
  cpu.type = CPU_386;
  config.fastfloppy = 1;

  config.emusys = (char *) NULL;
  config.emubat = (char *) NULL;
  tmpdir = tempnam("/tmp", "dosemu");
  config.dosbanner = 1;
  config.allowvideoportaccess = 0;
}

/*
 * Called to queue a hardware interrupt - will call "callstart"
 * just before the interrupt occurs and "callend" when it finishes
*/
void
queue_hard_int(int i, void (*callstart), void (*callend))
{
  cli();

  int_queue[int_queue_end].interrupt = i;
  int_queue[int_queue_end].callstart = callstart;
  int_queue[int_queue_end].callend = callend;
  int_queue_end = (int_queue_end + 1) % IQUEUE_LEN;

  h_printf("int_queue: (%d,%d) ", int_queue_start, int_queue_end);

  i = int_queue_start;
  while (i != int_queue_end) {
    h_printf("%x ", int_queue[i].interrupt);
    i = (i + 1) % IQUEUE_LEN;
  }
  h_printf("\n");
  sti();
}

/*int int_queue_running = 0;*/

/* Called by vm86() loop to handle queueing of interrupts */
void
int_queue_run()
{

  int i;
  us *ssp;

  if (int_queue_running || int_queue_head) {
    int endval;

    /* check if current int is finished */
    if ((int) (SEG_ADR((us *), cs, ip)) != int_queue_head->int_queue_return_addr) {

      /* outb20 is our override so more than 1 int can stack here */
      if (!outb20) {
#if 0
	h_printf("Not finished! int_run = %d, int_q_hd=%x\n", int_queue_running, int_queue_head);
#endif
	return;
      }
    }
    else {

      /* if it's finished - clean up */
      /* call user cleanup function */
      if (int_queue_head->int_queue_ptr->callend)
	endval = int_queue_head->int_queue_ptr->callend(int_queue_head->int_queue_ptr->interrupt);
      /*      else endval = IQUEUE_END_NORMAL; */

      if (int_queue_head->int_queue_ptr->interrupt < 0x0f &&
	  int_queue_head->int_queue_ptr->interrupt > 0x07) {

	int_count[int_queue_head->int_queue_ptr->interrupt - 8]--;

	/* Keep queue up to latest lastscan */
	if (int_queue_head->int_queue_ptr->interrupt == 0x09) {
	  k_printf("INT09 end\n");
	  /* I more int09's to go, prepare for next scan code */
	  if (int_count[1]) {
	    k_printf("Set scanned to zero when int9 reset\n");
	    scanned = 0;
	  }
	}

	if (int_queue_head->int_queue_ptr->interrupt == 0xb ||
	    int_queue_head->int_queue_ptr->interrupt == 0xc)
	  serial_run();
	/*	printf(" Down one on %02x = %d\n", int_queue_head->int_queue_ptr->interrupt, int_count[int_queue_head->int_queue_ptr->interrupt - 8]); */
      }

      /* restore registers */
      REGS = int_queue_head->saved_regs;

      int_queue_running--;

      h_printf("int_queue: finished %x\n", int_queue_head->int_queue_return_addr);
      int_queue_temp = int_queue_head->prev;
      free(int_queue_head);
      int_queue_head = int_queue_temp;
      if (int_queue_head)
	int_queue_head->next = NULL;

      outb20 = 1;

    }

#if 0
    if (outb20 && int_queue_running) {
      h_printf("outb20 decs int_queue_running from %d\n", int_queue_running);
      int_queue_running--;
      return;
    }
#endif

  }

#if 0
  fprintf(stderr, "Queue start %d, end %d, running %d\n", int_queue_start, int_queue_end, int_queue_running);
#endif

  if (int_queue_start == int_queue_end || !(REG(eflags) & IF))
    return;

  i = int_queue[int_queue_start].interrupt;

  /* call user startup function...don't run interrupt if returns -1 */
  if (int_queue[int_queue_start].callstart)
    if (int_queue[int_queue_start].callstart(i) == -1) {
      fprintf(stderr, "Callstart NOWORK\n");
      return;
    }

  if (int_queue_head == NULL) {
    int_queue_head = int_queue_tail = (struct int_queue_list_struct *) malloc(sizeof(struct int_queue_list_struct));

    int_queue_head->prev = NULL;
    int_queue_head->next = NULL;
    int_queue_head->int_queue_ptr = &int_queue[int_queue_start];
  }
  else {
    int_queue_temp = (struct int_queue_list_struct *) malloc(sizeof(struct int_queue_list_struct));

    int_queue_head->next = int_queue_temp;
    int_queue_temp->prev = int_queue_head;
    int_queue_head = int_queue_temp;
    int_queue_head->next = NULL;
    int_queue_head->int_queue_ptr = &int_queue[int_queue_start];
  }

  int_queue_running++;

  if (int_queue_head->int_queue_ptr->interrupt < 0x0f &&
      int_queue_head->int_queue_ptr->interrupt > 0x07) {
    int_count[int_queue_head->int_queue_ptr->interrupt - 8]++;
    /*	printf(" Up one on %02x = %d\n", int_queue_head->int_queue_ptr->interrupt, int_count[int_queue_head->int_queue_ptr->interrupt - 8]); */

    if (int_queue_head->int_queue_ptr->interrupt == 0x09) {
      k_printf("Set scanned to zero when int9 set\n");

      /* If another program does a keybaord read on port 0x60, we'll know */
      scanned = 0;
    }

  }

  /* We'll allow int's to end if an outb 0x20 happens */
  outb20 = 0;

  cli();

  /* save our regs */
  int_queue_head->saved_regs = REGS;

  if (!LWORD(esp))
    ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
  else
    ssp = SEG_ADR((us *), ss, sp);

  /* push an illegal instruction onto the stack */
  *--ssp = 0xffff;

  /* this is where we're going to return to */
  int_queue_head->int_queue_return_addr = (int) ssp;

  *--ssp = REG(eflags);
  *--ssp = (int_queue_head->int_queue_return_addr >> 4);	/* the code segment of our illegal opcode */
  *--ssp = (int_queue_head->int_queue_return_addr & 0xf);	/* and the instruction pointer */
  REG(esp) -= 8;
  REG(cs) = ((us *) 0)[(i << 1) + 1];
  REG(eip) = ((us *) 0)[i << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
  REG(eflags) &= ~(TF | IF | NT);

  sti();

  h_printf("int_queue: running int %x return_addr=%x\n", i, int_queue_head->int_queue_return_addr);
  int_queue_start = (int_queue_start + 1) % IQUEUE_LEN;

  return;
}

/* load <msize> bytes of file <name> starting at offset <foffset>
 * into memory at <mstart>
 */
int
load_file(char *name, int foffset, char *mstart, int msize)
{
  int fd;

  if (strcmp(name, "/dev/kmem") == 0) {
    v_printf("kmem used for loadfile\n");
    open_kmem();
    fd = mem_fd;
  }
  else
    fd = open(name, O_RDONLY);

  DOS_SYSCALL(lseek(fd, foffset, SEEK_SET));
  RPT_SYSCALL(read(fd, mstart, msize));

  if (strcmp(name, "/dev/kmem") == 0)
    close_kmem();
  else
    close(fd);
  return 0;
}

void
emulate(int argc, char **argv)
{
  struct sigaction sa;
  int c;
  char *confname = NULL;
  struct stat statout, staterr;

  config_defaults();

  /* set queue head and tail to NULL */
  int_queue_head = int_queue_tail = NULL;

  /* initialize cli() and sti() */
  signal_init();

  iq.queued = 0;
  in_sighandler = 0;
  sync();			/* for safety */

  /* allocate screen buffer for non-console video compare speedup */
  scrbuf = malloc(CO * LI * 2);

  opterr = 0;
  confname = NULL;
  while ((c = getopt(argc, argv, "ABCf:cF:kM:D:P:VNtsgx:Km234e:")) != EOF)
    if (c == 'F')
      confname = optarg;
  parse_config(confname);

  optind = 0;
  opterr = 0;
  while ((c = getopt(argc, argv, "ABCf:cF:kM:D:P:VNtT:sgx:Km234e:")) != EOF) {
    switch (c) {
    case 'F':			/* previously parsed config file argument */
      break;
    case 'A':
      config.hdiskboot = 0;
      break;
    case 'B':
      config.hdiskboot = 2;
      break;
    case 'C':
      config.hdiskboot = 1;
      break;
    case 'f':			/* flip floppies: argument is string */
      flip_disks(optarg);
      break;
    case 'c':
      config.console_video = 1;
      break;
    case 'k':
      config.console_keyb = 1;
      break;
    case 'K':
      warn("Keyboard interrupt enabled...this is still buggy!\n");
      config.keybint = 1;
      break;
    case 'M':{
	int max_mem = config.vga ? 640 : MAX_MEM_SIZE;

	config.mem_size = atoi(optarg);
	if (config.mem_size > max_mem)
	  config.mem_size = max_mem;
	break;
      }
    case 'D':
      parse_debugflags(optarg);
      break;
    case 'P':
      if (terminal_fd == -1)
	open_terminal_pipe(optarg);
      else
	error("ERROR: terminal pipe already open\n");
      break;
    case 'V':
      g_printf("Assuming VGA video card & mapped ROM\n");
      config.vga = 1;
      config.mapped_bios = 1;
      if (config.mem_size > 640)
	config.mem_size = 640;
      break;
    case 'N':
      warn("DOS will not be started\n");
      exitearly = 1;
      break;
    case 'T':
      g_printf("Using tmpdir=%s\n", optarg);
      free(tmpdir);
      tmpdir = strdup(optarg);
      break;
    case 't':
      g_printf("doing timer emulation\n");
      config.timers = 1;
      break;
    case 's':
      g_printf("using new scrn size code\n");
      sizes = 1;
      break;
    case 'g':
#ifdef EXPERIMENTAL_GFX
      g_printf("turning graphics option on\n");
      config.graphics = 1;
#else
      error("Graphics support not compiled in!\n");
#endif
      break;

    case 'x':
      config.xms_size = atoi(optarg);
      x_printf("enabling %dK XMS memory\n", config.xms_size);
      break;

    case 'e':
      config.ems_size = atoi(optarg);
      g_printf("enabling %dK EMS memory\n", config.ems_size);
      break;

    case 'm':
      g_printf("turning MOUSE support on\n");
      config.mouse_flag = 1;
      break;

    case '2':
      g_printf("CPU set to 286\n");
      cpu.type = CPU_286;
      break;

    case '3':
      g_printf("CPU set to 386\n");
      cpu.type = CPU_386;
      break;

    case '4':
      g_printf("CPU set to 486\n");
      cpu.type = CPU_486;
      break;

    case '?':
    default:
      p_dos_str("unrecognized option: -%c\n\r", c /*optopt*/ );
      usage();
      fflush(stdout);
      fflush(stderr);
      _exit(1);
    }
  }

  fstat(STDOUT_FILENO, &statout);
  fstat(STDERR_FILENO, &staterr);
  if (staterr.st_ino == statout.st_ino) {
    if (freopen("/dev/null", "ab", stderr) == (FILE *) - 1) {
      fprintf(stdout, "ERROR: Could not redirect STDERR to /dev/null!\n");
      exit(-1);
    }
  }

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* create tmpdir */
  exchange_uids();
  mkdir(tmpdir, S_IREAD | S_IWRITE | S_IEXEC);
  exchange_uids();
  /* setup DOS memory, whether shared or not */
  memory_setup();

#if 0
  if (config.mapped_bios || config.console_video)
    open_kmem();
#endif

  if (config.mapped_bios) {
    if (config.vbios_file) {
      warn("WARN: loading VBIOS %s into mem at 0x%X (0x%X bytes)\n",
	   config.vbios_file, VBIOS_START, VBIOS_SIZE);
      load_file(config.vbios_file, 0, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else if (config.vbios_copy) {
      warn("WARN: copying VBIOS file from /dev/kmem\n");
      load_file("/dev/kmem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else {
#if 1
      warn("WARN: copying VBIOS file from /dev/kmem\n");
      load_file("/dev/kmem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
#else
      /* No direct mapping as this leaves memory busy for running more than
   one DOSEMU session */
      map_bios();
#endif
    }
  }

  /* copy graphics characters from system BIOS */
  load_file("/dev/kmem", GFX_CHARS, (char *) GFX_CHARS, GFXCHAR_SIZE);

  g_printf("EMULATE\n");

  /* we assume system call restarting... under linux 0.99pl8 and earlier,
 * this was the default.  SA_RESTART was defined in 0.99pl8 to explicitly
 * request restarting (and thus does nothing).  However, if this ever
 * changes, I want to be safe
 */
#ifndef SA_RESTART
#define SA_RESTART 0
#endif

#define SETSIG(sig, fun) \
		sa.sa_handler = (__sighandler_t) fun; \
		/* Point to the top of the stack, minus 4 just in case, and make \
		   just in case, and make it aligned  */ \
		sa.sa_restorer = \
		(void (*)()) (((unsigned int)(cstack) + sizeof(cstack) - 4) & ~3); \
		sa.sa_flags = SA_RESTART; \
		sigemptyset(&sa.sa_mask); \
		sigaddset(&sa.sa_mask, SIG_TIME); \
		dosemu_sigaction(sig, &sa, NULL);

  /* init signal handlers */

  SETSIG(SIGILL, sigill);
  SETSIG(SIG_TIME, sigalrm);
  SETSIG(SIGFPE, sigfpe);
  SETSIG(SIGTRAP, sigtrap);
  SETSIG(SIGIPC, sigipc);

  SETSIG(SIGHUP, leavedos);	/* for "graceful" shutdown */
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
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
    tm = localtime((time_t *) & ticks);
    last_ticks = (tm->tm_hour * 60 * 60 + tm->tm_min * 60 + tm->tm_sec) * 18.206;
    set_ticks(last_ticks);
  };

  disk_init();
  termioInit();
  SETSIG(SIGSEGV, sigsegv);
  hardware_init();
  video_config_init();
  if (config.vga)
    vga_initialize();
  clear_screen(SCREEN, 7);

  boot();

  /*
  boot(config.hdiskboot ? hdisktab : disktab);
*/

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
  update_screen = 1;

  /* start up the IPC process...stopped in leavedos() */
  start_dosipc();
  post_dosipc();
#if 0
  dbug_dumpivec();
#endif
  serial_init();

  for (; !fatalerr;) {
    run_vm86();
    if (!int_count[0x3] && !int_count[0x4])
      serial_run();
    int_queue_run();
  }
  error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	fatalerr, fatalerr, in_sigsegv, ignore_segv);

  sync();
  fprintf(stderr, "ARG!!!!!\n");
  leavedos(0);
}

void
video_config_init()
{
  switch (config.cardtype) {
  case CARD_MDA:
    {
      configuration |= (MDA_CONF_SCREEN_MODE);
      screen_mode = MDA_INIT_SCREEN_MODE;
      phys_text_base = MDA_PHYS_TEXT_BASE;
      virt_text_base = MDA_VIRT_TEXT_BASE;
      video_combo = MDA_VIDEO_COMBO;
      video_subsys = MDA_VIDEO_SUBSYS;
      break;
    }
  case CARD_CGA:
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      screen_mode = CGA_INIT_SCREEN_MODE;
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      video_subsys = CGA_VIDEO_SUBSYS;
      break;
    }
  case CARD_EGA:
    {
      configuration |= (EGA_CONF_SCREEN_MODE);
      screen_mode = EGA_INIT_SCREEN_MODE;
      phys_text_base = EGA_PHYS_TEXT_BASE;
      virt_text_base = EGA_VIRT_TEXT_BASE;
      video_combo = EGA_VIDEO_COMBO;
      video_subsys = EGA_VIDEO_SUBSYS;
      break;
    }
  case CARD_VGA:
    {
      configuration |= (VGA_CONF_SCREEN_MODE);
      screen_mode = VGA_INIT_SCREEN_MODE;
      phys_text_base = VGA_PHYS_TEXT_BASE;
      virt_text_base = VGA_VIRT_TEXT_BASE;
      video_combo = VGA_VIDEO_COMBO;
      video_subsys = VGA_VIDEO_SUBSYS;
      break;
    }
  default:			/* or Terminal, is this correct ? */
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      screen_mode = CGA_INIT_SCREEN_MODE;
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      video_subsys = CGA_VIDEO_SUBSYS;
      break;
    }
  }
}

void
ign_sigs(int sig)
{
  static int timerints = 0;
  static int otherints = 0;

  error("ERROR: signal %d received in leavedos()\n", sig);
  if (sig == SIG_TIME)
    timerints++;
  else
    otherints++;

#define LEAVEDOS_TIMEOUT (3 * FREQ)
#define LEAVEDOS_SIGOUT  5
  /* XXX - why do I need this? */
  if ((timerints >= LEAVEDOS_TIMEOUT) || (otherints >= LEAVEDOS_SIGOUT)) {
    error("ERROR: timed/signalled out in leavedos()\n");
    fclose(stderr);
    fclose(stdout);
    _exit(1);
  }
}

/* "graceful" shutdown */
void
leavedos(int sig)
{
  struct sigaction sa;

  in_vm86 = 0;

  /* remove tmpdir */
  rmdir(tmpdir);

  SETSIG(SIG_TIME, ign_sigs);
  SETSIG(SIGSEGV, ign_sigs);
  SETSIG(SIGILL, ign_sigs);
  SETSIG(SIGFPE, ign_sigs);
  SETSIG(SIGTRAP, ign_sigs);
  p_dos_str("\n\rDOS killed!\n\r");
  error("leavedos(%d) called - shutting down\n", sig);

  close_all_printers();

  serial_close();

  show_ints(0, 0x33);
  show_regs();
  fflush(stderr);
  fflush(stdout);

  termioClose();
  disk_close_all();

  /* close down the IPC process & kill all shared mem */
  SETSIG(SIGCHLD, SIG_DFL);

  stop_dosipc();

  _exit(0);
}

void
hardware_init(void)
{
  int i;

  /* do PIT init here */
  init_all_printers();

  /* PIC init */
  for (i = 0; i < 2; i++) {
    pics[i].OCW1 = 0;		/* no IRQ's serviced */
    pics[i].OCW2 = 0;		/* no EOI's received */
    pics[i].OCW3 = 8;		/* just marks this as OCW3 */
  }

  g_printf("Hardware initialized\n");
}

/* check the fd for data ready for reading */
int
d_ready(int fd)
{
  struct timeval w_time;
  fd_set checkset;

  w_time.tv_sec = 0;
  w_time.tv_usec = 200000;

  FD_ZERO(&checkset);
  FD_SET(fd, &checkset);

  if (RPT_SYSCALL(select(fd + 1, &checkset, NULL, NULL, &w_time)) == 1) {
    if (FD_ISSET(fd, &checkset))
      return (1);
    else
      return (0);
  }
  else
    return (0);
}

void
usage(void)
{
  fprintf(stdout, "$Header: /home/src/dosemu0.50/RCS/emu.c,v 1.45 1994/03/04 15:23:54 root Exp root $\n");
  fprintf(stdout, "usage: dos [-ABCckbVtsgxKm234e] [-D flags] [-M SIZE] [-P FILE] [-H|F #disks] [-f FLIPSTR] > doserr\n");
  fprintf(stdout, "    -A boot from first defined floppy disk (A)\n");
  fprintf(stdout, "    -B boot from second defined floppy disk (B) (#)\n");
  fprintf(stdout, "    -C boot from first defined hard disk (C)\n");
  fprintf(stdout, "    -f flip disks, argument \"X,Y\" where 1 <= X&Y <= 3\n");
  fprintf(stdout, "    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
  fprintf(stdout, "    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
  fprintf(stdout, "    -D set debug-msg mask to flags (+-)(avkdRWspwgxhi01)\n");
  fprintf(stdout, "    -M set memory size to SIZE kilobytes (!)\n");
  fprintf(stdout, "    -P copy debugging output to FILE\n");
  fprintf(stdout, "    -b map BIOS into emulator RAM (%%)\n");
  fprintf(stdout, "    -H specify number of hard disks (1 or 2)\n");
  fprintf(stdout, "    -F specify number of floppy disks (1-4)\n");
  fprintf(stdout, "    -V use BIOS-VGA video modes (!#%%)\n");
  fprintf(stdout, "    -N No boot of DOS\n");
  fprintf(stdout, "    -t try new timer code (#)\n");
  fprintf(stdout, "    -s try new screen size code (COMPLETELY BROKEN)(#)\n");
  fprintf(stdout, "    -g enable graphics modes (COMPLETELY BROKEN) (!%%#)\n");
  fprintf(stdout, "    -x SIZE enable SIZE K XMS RAM\n");
  fprintf(stdout, "    -e SIZE enable SIZE K EMS RAM\n");
  fprintf(stdout, "    -m enable mouse support (!#)\n");
  fprintf(stdout, "    -2,3,4 choose 286, 386 or 486 CPU\n");
  fprintf(stdout, "    -K Do int9 within PollKeyboard (!#)\n");
  fprintf(stdout, "\n     (!) means BE CAREFUL! READ THE DOCS FIRST!\n");
  fprintf(stdout, "     (%%) marks those options which require dos be run as root (i.e. suid)\n");
  fprintf(stdout, "     (#) marks options which do not fully work yet\n");
}

void
saytime(char *m_str)
{
  clock_t m_clock;
  struct tms l_time;
  long clktck = sysconf(_SC_CLK_TCK);

  m_clock = times(&l_time);
  fprintf(stderr, "%s %7.4f\n", m_str, m_clock / (double) clktck);

}

int
ifprintf(unsigned char flg, const char *fmt,...)
{
  va_list args;
  char buf[1025];
  int i;

  if (!flg)
    return 0;

  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);

#if 0
  fprintf(stderr, buf);
  if (terminal_pipe)
    fprintf(terminal, buf);
#else
  write(STDERR_FILENO, buf, strlen(buf));
  if (terminal_pipe)
    write(terminal_fd, buf, strlen(buf));
#endif
  return i;
}

void
p_dos_str(char *fmt,...)
{
  va_list args;
  char buf[1025], *s;
  int i;

  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);

  s = buf;
  while (*s)
    char_out(*s++, SCREEN, ADVANCE);
}

void
init_vga_card(void)
{

  u_short *ssp;

#if 0
#define ADDR 0x0000
  u_char *bios_mem, buffer[0x1000];

#endif

  if (!config.mapped_bios) {
    error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
    return;
  }
  if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
    warn("couldn't get range!\n");
  config.vga = 1;
  set_vc_screen_page(SCREEN);

#if 0
  open_kmem();
  bios_mem =
    (char *) mmap(
		   (caddr_t) (0x0),
		   (size_t) 0x1000,
		   PROT_READ,
		   MAP_SHARED /* | MAP_FIXED */ ,
		   mem_fd,
		   (off_t) (ADDR)
    );
  if ((caddr_t) bios_mem == (caddr_t) (-1)) {
    perror("OOPS");
    leavedos(0);
  }

  v_printf("Video interrupt is at %04x, mode=0x%02x\n",
	   *(unsigned int *) (bios_mem + 0x40),
	   (u_char) bios_mem[0x449]);

  memmove(buffer, bios_mem, 0x1000);
  munmap((caddr_t) bios_mem, 0x1000);

  close_kmem();
  v_printf("SEG=0x%02x, OFF=0x%02x\n", *(u_short *) (buffer + 0x42), *(u_short *) (buffer + 0x40));
  SETIVEC(0x10, *(u_short *) (buffer + 0x42), *(u_short *) (buffer + 0x40));
  memmove((caddr_t) 0x449, (caddr_t) (buffer + 0x449), 0x25);

#else
  ssp = SEG_ADR((us *), ss, sp);
  *--ssp = REG(cs);
  *--ssp = REG(eip);
  REG(esp) -= 4;
#if 0
  REG(cs) = 0xc000;
  REG(eip) = 3;
#else
  REG(cs) = INT10_SEG;
  REG(eip) = INT10_OFF;
#endif
#endif
}

void
ems_helper(void)
{
  u_char *rhptr;		/* request header pointer */

  switch (LWORD(ebx)) {
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
  error("EMS RHDR: len %d, command %d\n", *rhptr, *(u_short *) (rhptr + 2));
}

/* returns 1 if dos_helper() handles it, 0 otherwise */
int
dos_helper(void)
{
  us *ssp;

  switch (LO(ax)) {
  case 0x20:
    mfs_inte6();
    return 1;
    break;
  case 0x21:
    ems_helper();
    return 1;
    break;
  case 0x60:
    pkt_helper();
    return 1;
    break;
  case 0x22:{
      LWORD(eax) = pop_word(&REGS);
      E_printf("EMS: in 0xe6,0x22 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx));
      if (config.ems_size)
	bios_emm_fn(&REGS);
      else
	error("EMS: not running bios_em_fn!\n");
      break;
    }
#ifdef IPX
    if (config.ipxsup) {
  case 0x7a:
      /* TRB handle IPX far calls in dos_helper() */
      IPXFarCallHandler();
      break;
    }
#endif
  case 0:			/* Linux dosemu installation test */
    LWORD(eax) = 0xaa55;
    LWORD(ebx) = VERNUM;	/* major version 0.49 -> 0x0049 */
    warn("WARNING: dosemu installation check\n");
    show_regs();
    break;
  case 1:			/* SHOW_REGS */
    show_regs();
    break;
  case 2:			/* SHOW INTS, BH-BL */
    show_ints(HI(bx), LO(bx));
    break;
  case 3:			/* SET IOPERMS: bx=start, cx=range,
		   carry set for get, clear for release */
    {
      int cflag = REG(eflags) & CF ? 1 : 0;

      i_printf("I/O perms: 0x%04x 0x%04x %d\n",
	       LWORD(ebx), LWORD(ecx), cflag);
      if (set_ioperm(LWORD(ebx), LWORD(ecx), cflag)) {
	error("ERROR: SET_IOPERMS request failed!!\n");
	CARRY;			/* failure */
      }
      else {
	if (cflag)
	  warn("WARNING! DOS can now access"
	       " I/O ports 0x%04x to 0x%04x\n",
	       LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
	else
	  warn("Access to ports 0x%04x to 0x%04x clear\n",
	       LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
	NOCARRY;		/* success */
      }
    }
    break;
  case 4:			/* initialize video card */
    if (LO(bx) == 0) {
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 0))
	warn("couldn't shut off ioperms\n");
      SETIVEC(0x10, BIOSSEG, 0x10 * 0x10);	/* restore our old vector */
      config.vga = 0;
    }
    else {
      if (!config.mapped_bios) {
	error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
	return 1;
      }
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
	warn("couldn't get range!\n");
      config.vga = 1;
      set_vc_screen_page(SCREEN);
      warn("WARNING: jumping to 0c000:0003\n");
      ssp = SEG_ADR((us *), ss, sp);
      *--ssp = REG(cs);
      *--ssp = REG(eip);
      precard_eip = REG(eip);
      precard_cs = REG(cs);
      REG(esp) -= 4;
      REG(cs) = 0xc000;
      REG(eip) = 3;
      show_regs();
      card_init = 1;
    }

  case 5:			/* show banner */
    p_dos_str("\n\nLinux DOS emulator " VERSTR " $Date: 1994/03/04 15:23:54 $\n");
    p_dos_str("Last configured at %s\n", CONFIG_TIME);
    p_dos_str("on %s\n", CONFIG_HOST);
    /*      p_dos_str("maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n"); */
    p_dos_str("Bugs & Patches to James MacLean, jmaclean@fox.nstn.ns.ca\n\n");
    break;

  case 6:			/* Do inline int09 insert_into_keybuffer() */
    k_printf("Doing INT9 insert_into_keybuffer() bx=0x%04x\n", LWORD(ebx));
    insert_into_keybuffer();
    break;

  case 7:			/* Do int09 set_keyboard_bios() */
    k_printf("Doing INT9 set_keyboard_bio() ax=0x%04x\n", LWORD(eax));
    set_keyboard_bios();
    LO(ax) = HI(ax);
    break;

  case 8:
    v_printf("Starting Video initialization\n");
    if (config.allowvideoportaccess) {
      if (config.speaker != SPKR_NATIVE) {
	v_printf("Giving access to port 0x42\n");
	set_ioperm(0x42, 1, 1);
      }
      in_video = 1;
    }
    break;

  case 9:
    v_printf("Finished with Video initialization\n");
    if (config.allowvideoportaccess) {
      if (config.speaker != SPKR_NATIVE) {
	v_printf("Removing access to port 0x42\n");
	set_ioperm(0x42, 1, 0);
      }
      in_video = 0;
    }
    break;

  case 0x10:
    /* TRB - handle dynamic debug flags in dos_helper() */
    LWORD(eax) = GetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) + (_regs.edi & 0xffff)));
    break;
  case 0x11:
    LWORD(eax) = SetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) + (_regs.edi & 0xffff)));
    break;

  case 0x30:			/* set/reset use bootdisk flag */
    use_bootdisk = LO(bx) ? 1 : 0;
    break;

  case 0xff:
    if (LWORD(eax) == 0xffff) {
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
    return setreuid(geteuid(), getuid());
  else
    return 0;
}

inline uid_t
be_me()
{
  if (geteuid() == 0) {
    return setreuid(geteuid(), getuid());
    return 0;
  }
  else
    return geteuid();
}

inline uid_t
be_root()
{
  if (geteuid() != 0) {
    setreuid(geteuid(), getuid());
    return getuid();
  }
  else
    return 0;
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
  tmp = ioperm(start, size, flag);
  s_errno = errno;
#if DO_BE
  be(last_me);
  warn("IOP: was %d, now %d\n", 0, last_me);
#endif

  errno = s_errno;
  return tmp;
}

#undef EMU_C
