#define EMU_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1994/05/26 23:15:01 $
 * $Source: /home/src/dosemu0.60/RCS/emu.c,v $
 * $Revision: 1.86 $
 * $State: Exp $
 *
 * $Log: emu.c,v $
 * Revision 1.86  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.85  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.84  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.83  1994/05/18  00:15:51  root
 * pre15_17.
 *
 * Revision 1.82  1994/05/16  23:13:23  root
 * Prep for pre51_16.
 *
 * Revision 1.81  1994/05/13  23:20:15  root
 * Pre51_15.
 *
 * Revision 1.80  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.79  1994/05/13  01:47:59  root
 * Updates 1 for DV.
 *
 * Revision 1.78  1994/05/10  23:14:44  root
 * pre51_14.
 *
 * Revision 1.77  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.76  1994/05/09  23:35:11  root
 * pre51_13.
 *
 * Revision 1.75  1994/05/05  00:16:26  root
 * Prep for pre51_12.
 *
 * Revision 1.74  1994/05/04  22:16:00  root
 * Patches by Alan to mouse subsystem.
 *
 * Revision 1.73  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
 * Revision 1.72  1994/04/30  22:12:30  root
 * Prep for pre51_11.
 *
 * Revision 1.71  1994/04/30  01:05:16  root
 * Lutz's latest 94/04/29
 *
 * Revision 1.70  1994/04/29  23:52:06  root
 * Prior to Lutz's latest 94/04/29.
 *
 * Revision 1.69  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.68  1994/04/27  21:34:15  root
 * Jochen's Latest.
 *
 * Revision 1.67  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.66  1994/04/23  20:10:38  root
 * Updated again for SP over/under flow.
 *
 * Revision 1.65  1994/04/20  23:43:35  root
 * pre51_8 out the door.
 *
 * Revision 1.64  1994/04/20  21:05:01  root
 * Prep for Rob's patches to linpkt...
 *
 * Revision 1.63  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.62  1994/04/18  20:57:34  root
 * Checkin prior to Jochen's latest patches.
 *
 * Revision 1.61  1994/04/16  14:41:41  root
 * Prep for pre51_6.
 *
 * Revision 1.60  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.59  1994/04/13  00:07:09  root
 * Multiple patches from various sources.
 *
 * Revision 1.58  1994/04/09  18:41:52  root
 * Prior to Lutz's kernel enhancements.
 *
 * Revision 1.57  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 1.56  1994/04/04  22:51:55  root
 * Patches for PS/2 mice.
 *
 * Revision 1.55  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.54  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
 * Revision 1.53  1994/03/18  23:17:51  root
 * Prep for 0.50pl1
 *
 * Revision 1.52  1994/03/15  02:08:20  root
 * Testing
 *
 * Revision 1.51  1994/03/15  01:38:20  root
 * DPMI,serial, other changes.
 *
 * Revision 1.50  1994/03/14  00:35:44  root
 * Moved int_queue_run back into static array.
 *
 * Revision 1.49  1994/03/13  21:52:02  root
 * More speed testing :-(
 *
 * Revision 1.48  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.47  1994/03/10  23:52:52  root
 * Lutz DPMI patches
 *
 * Revision 1.46  1994/03/10  02:49:27  root
 * Back to SINGLE Process.
 *
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

   make sure that this line is the first of emu.c
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
#include "dosio.h"
#include "disks.h"
#include "xms.h"
#include "timers.h"
#ifdef DPMI
#include "dpmi/dpmi.h"
#endif
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"
#include "keymaps.h"
#include "cpu.h"

#if 1				/* 94/05/12 */
#include "int10.h"
#include "int.h"
#endif

extern void getKeys(void);

extern struct pit pit;

extern inline void disk_open(struct disk *);
extern inline void vm86_sigsegv();

char *segv_stack[4096];
char *alrm_stack[4096];
char *ill_stack[4096];
char *trap_stack[4096];

int int_queue_running = 0;
inline void int_queue_run();

#define TIMER_DIVISOR 3

#define DBGTIME(x) {\
                        struct timeval tv;\
                        gettimeofday(&tv,NULL);\
                        fprintf(stderr,"%c %06d:%06d\n",x,(int)tv.tv_sec,(int)tv.tv_usec);\
                   }

/* Time structures for translating UNIX <-> DOS times */
struct timeval scr_tv;
struct itimerval itv;

long start_time;		/* Keep track of times for DOS calls */
unsigned long last_ticks;

void video_config_init(void);

/*
   Tables that hold information of currently specified storage
   devices.
*/
extern struct disk disktab[], hdisktab[];

/* Structure to hold all current configuration information */
struct config_info config;

/*
   Structure to hold current state of all VM86 registers used in
   VM86 mode
*/
extern struct vm86_struct vm86s;

/* Keep track of error that causes DOSEMU to exit */
int fatalerr;

/*
   Queue to hold all pending hard-interrupts. When an interrupt is
   placed into this queue, it can include a function to be run
   prior to the actuall interrupt being placed onto the DOS stack,
   as well as include a function to be called after the interrupt
   finishes.
*/
int int_queue_start = 0;
int int_queue_end = 0;

#if 0
#define IQUEUE_LEN 1000
struct int_queue_struct {
  int interrupt;
  int (*callstart) ();
  int (*callend) ();
}
int_queue[IQUEUE_LEN];
#endif

/*
   This is here to allow multiple hard_int's to be running concurrently.
   Needed for programs that steal INT9 away from DOSEMU.
*/
#if 0
#define NUM_INT_QUEUE 64
struct int_queue_list_struct {
  struct int_queue_struct int_queue_ptr;
  int int_queue_return_addr;
  u_char in_use;
  struct vm86_regs saved_regs;
} int_queue_head[NUM_INT_QUEUE];
#endif

int scrtest_bitmap, update_screen;	/* Flags to test if screen to be updated */
unsigned char *scrbuf;		/* the previously updated screen */

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
unsigned int configuration = 0;
void config_init(void);

/* Function to set up all memory area for DOS, as well as load boot block */
void boot(void);

extern void map_bios(void);	/* map in VIDEO bios */
extern int open_kmem();		/* Get access to physical memory */

void leavedos(int),		/* function to stop DOSEMU */
 usage(void),			/* Print parameters of DOSEMU */
 hardware_init(void);		/* Initialize info on hardware */

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

int special_nowait = 0;

struct ioctlq iq =
{0, 0, 0, 0};			/* one-entry queue :-( for ioctl's */
char *tmpdir;
u_char in_sighandler = 0;	/* so I know to not use non-reentrant
				 * syscalls like ioctl() :-( */
u_char in_ioctl = 0;
struct ioctlq curi =
{0, 0, 0, 0};

/* this is DEBUGGING code! */
int sizes = 0;

struct debug_flags d =
{0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0};

int poll_io = 1;		/* polling io, default on */
u_char ignore_segv = 0;		/* ignore sigsegv's */
u_char in_sigsegv = 0;
int terminal_pipe;
int terminal_fd = -1;

/*
   This flag will be set when doing video routines so that special
   access can be given
*/
u_char in_video = 0;

/* for use by cli() and sti() */
sigset_t oldset;

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
  static int retval;
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */
    in_vm86 = 1;
    switch VM86_TYPE(retval = vm86(&vm86s)) {
	case VM86_UNKNOWN:
		vm86_sigsegv();
		break;
	case VM86_STI:
		I_printf("Return from vm86() for timeout\n");
		REG(eflags) &= ~(VIP);
		break;
	case VM86_INTx:
		in_vm86 = 0;
		do_int(VM86_ARG(retval));
		break;
	case VM86_SIGNAL:
		break;
	default:
		error("unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
    }
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

void
config_init(void)
{
  int b;

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, and the
   * configured number of floppy disks
   */
  CONF_NFLOP(configuration, config.fdisks);
  CONF_NSER(configuration, config.num_ser);
  CONF_NLPT(configuration, config.num_lpt);
  if ((mice->type == MOUSE_PS2) || (mice->intdrv))
    configuration |= CONF_MOUSE;

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
  clear_screen(bios_current_screen_page, 7);
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
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, LWORD(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 4;
  LWORD(cs) = Banner_SEG;
  LWORD(eip) = Banner_OFF;
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
boot(void)
{
  char *buffer;
  unsigned int i;
  unsigned char *ptr;
  struct disk *dp = NULL;
  ushort *seg, *off;

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
    leavedos(15);
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
  /* Let kernel handle this, no need to return to DOSEMU */
  SETIVEC(0x1c, 0xf01c, 0);

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

  if (mice->intdrv) {
    /* this is the mouse handler */
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
  else
    *(unsigned char *) (BIOSSEG * 16 + 16 * 0x33) = 0xcf;	/* IRET */

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
  memcpy(ptr, INT16_dummy_start, (unsigned long) INT16_dummy_end - (unsigned long) INT16_dummy_start);
  SETIVEC(0x16, INT16_SEG, INT16_OFF);

  /* Welcome to an -inline- int09 routine - ask for details :-) */
  ptr = (u_char *) INT09_ADD;
  memcpy(ptr, INT09_dummy_start, (unsigned long) INT09_dummy_end - (unsigned long) INT09_dummy_start);
  SETIVEC(0x09, INT09_SEG, INT09_OFF);

  /* Welcome to an -inline- int08 */
  ptr = (u_char *) INT08_ADD;
  memcpy(ptr, INT08_dummy_start, (unsigned long) INT08_dummy_end - (unsigned long) INT08_dummy_start);
  SETIVEC(0x08, INT08_SEG, INT08_OFF);

  install_int_10_handler();	/* Install the handler for video-interrupt */

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

#ifdef DPMI
  /* A call from a DPMI program to go protected will go here, a HLT */
  ptr = (u_char *) DPMI_ADD;
  memcpy(ptr, DPMI_dummy_start, (unsigned long)DPMI_dummy_end-(unsigned long)DPMI_dummy_start);
#endif

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
  if (config.ipxsup) {
    InitIPXFarCallHelper();
  }
#endif

  /* Install the new packet driver interface */
  pkt_init(0x60);

  bios_address_lpt1 = 0x378;
  bios_address_lpt2 = 0x278;
  bios_configuration = configuration;
  bios_memory_size = config.mem_size;	/* size of memory */
  bios_video_mode = screen_mode;/* screen mode */
  bios_screen_columns = CO;	/* chars per line */
  bios_rows_on_screen_minus_1 = LI - 1;	/* lines on screen - 1 */
  bios_video_memory_used = TEXT_SIZE;	/* size of video regen area in bytes */
  bios_video_memory_address = 0;/* offset of current page in buffer */

  /* The default 16-word BIOS key buffer starts at 0x41e */
  KBD_Head =			/* key buf start ofs */
    KBD_Tail =			/* key buf end ofs */
    KBD_Start = 0x1e;		/* keyboard queue start... */
  KBD_End = 0x3e;		/* ...and end offsets from 0x400 */

  keybuf_clear();

  if ((configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE)
    bios_video_port = 0x3b4;	/* base port of CRTC - IMPORTANT! */
  else
    bios_video_port = 0x3d4;	/* base port of CRTC - IMPORTANT! */
  bios_vdu_control = 9;		/* current 3x8 (x=b or d) value */
  bios_ctrl_alt_del_flag = 0x0000;
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
      leavedos(16);
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

  if (config.exitearly) {
    dbug_printf("Leaving DOS before booting\n");
    leavedos(0);
  }

  /* Set OUTB_ADD to 1 */
  *OUTB_ADD = 1;
  *LASTSCAN_ADD = 0;
  REG(eflags) |= (IF | VIF | VIP);

  ignore_segv--;
}

void
sigalrm(int sig, struct sigcontext_struct context)
{
  static int running = 0;
  static inalrm = 0;
  static int partials = 0;
  static u_char timals = 0;

#ifdef DPMI
  if (in_dpmi && !in_vm86)
    dpmi_sigalrm(&context);
#endif /* DPMI */

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

  if (mice->intdrv)
    mouse_curtick();

  /* TRB - perform processing for the IPX Asynchronous Event Service */
#ifdef IPX
  if (config.ipxsup)
    AESTimerTick();
#endif

  /* check for available packets on the packet driver interface */
  pkt_check_receive();

  /* update the Bios Data Area timer dword if interrupts enabled */
  if (REG(eflags) & VIF)
    timer_tick();

#if 0 /* 94/05/24 Seems to cause more harm than good :-( */
  /* Deal with timer ports 0x40 - 0x42 */
  pit.CNTR0 -= 0x1fff;
  pit.CNTR1 -= 0x1fff;
  pit.CNTR2 -= 0x1fff;
#endif

  if (config.timers) {
    if (++timals == 2) {
      timals = 0;
      h_printf("starting timer int 8...\n");
      if (!do_hard_int(8))
	h_printf("CAN'T DO TIMER INT 8...IF CLEAR\n");
    }
  }
  else
    error("NOT CONFIG.TIMERS\n");

  /* Call select to see if any I/O is ready on devices */
  io_select();

  /* this is for per-second activities */
  partials++;
  if (partials == FREQ) {
    partials = 0;
#if 0 /* Try seting leds in ports.h outb 0x20 94/05/25 */
    if (config.console_keyb)
      set_leds();
#endif
    printer_tick((u_long) 0);
    if (config.fastfloppy)
      floppy_tick();
  }

  in_sighandler = 0;
  inalrm = 0;
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
      fprintf(stderr, "Unknown debug-msg mask: %c\n\r", c);
      dbug_printf("Unknown debug-msg mask: %c\n", c);
    }
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
  config.vbios_seg = 0xc000;
  config.console_keyb = 0;
  config.console_video = 0;
  config.fdisks = 0;
  config.hdisks = 0;
  config.bootdisk = 0;
  config.exitearly = 0;
  config.redraw_chunks = 1;
  config.hogthreshold = 5000;	/* in usecs */
  config.chipset = PLAINVGA;
  config.cardtype = CARD_VGA;
  config.fullrestore = 0;
  config.graphics = 0;
  config.gfxmemsize = 256;
  config.vga = 0;		/* this flags BIOS graphics */

  config.speaker = SPKR_EMULATED;

#if 0 /* This is too slow, but why? */
  config.update = 54945;
#else
  config.update = 27472;
#endif
  config.freq = 18;		/* rough frequency */

  config.timers = 1;		/* deliver timer ints */
  config.keybint = 0;		/* no keyboard interrupt */

  config.num_ser = 0;
  config.num_lpt = 0;
  vm86s.cpu_type = CPU_386;
  config.fastfloppy = 1;

  config.emusys = (char *) NULL;
  config.emubat = (char *) NULL;
  tmpdir = tempnam("/tmp", "dosemu");
  config.dosbanner = 1;
  config.allowvideoportaccess = 0;

  config.keyboard = KEYB_US;	/* What's the current keyboard  */
  config.key_map = key_map_us;	/* pointer to the keyboard-maps */
  config.shift_map = shift_map_us;	/* Here the Shilt-map           */
  config.alt_map = alt_map_us;	/* And the Alt-map              */
  config.num_table = num_table_dot;	/* Numeric keypad has a dot     */

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

  if (int_queue_start == int_queue_end)
    leavedos(56);
  sti();
}

/* Called by vm86() loop to handle queueing of interrupts */
inline void
int_queue_run()
{

  static int current_interrupt;
  static unsigned char *ssp;
  static unsigned long sp;
  static u_char vif_counter=0; /* Incase someone don't clear things */

  if (int_queue_start == int_queue_end) {
#if 0
    REG(eflags) &= ~(VIP);
#endif
    return;
  }

  if (!*OUTB_ADD) {
    if (++vif_counter & 0x08) {
      I_printf("OUTB interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("OUTB_ADD = %d , returning from int_qeueu_run()\n", *OUTB_ADD);
      return;
    }
  }

  if (!(REG(eflags) & VIF)) {
    if (++vif_counter > 0x08) {
      I_printf("VIF interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("interrupts disabled while int_qeueu_run()\n");
      return;
    }
  }

  vif_counter=0;
  current_interrupt = int_queue[int_queue_start].interrupt;

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

  if (current_interrupt == 0x09) {
    k_printf("Int9 set\n");
    /* If another program does a keybaord read on port 0x60, we'll know */
    parent_nextscan();
  }

  /* call user startup function...don't run interrupt if returns -1 */
  if (int_queue[int_queue_start].callstart) {
    if (int_queue[int_queue_start].callstart(current_interrupt) == -1) {
      fprintf(stderr, "Callstart NOWORK\n");
      return;
    }

    if (int_queue_running + 1 == NUM_INT_QUEUE)
      leavedos(55);

    int_queue_head[++int_queue_running].int_queue_ptr = int_queue[int_queue_start];
    int_queue_head[int_queue_running].in_use = 1;

    /* save our regs */
    int_queue_head[int_queue_running].saved_regs = REGS;

    cli();

    /* push an illegal instruction onto the stack */
    /*  pushw(ssp, sp, 0xffff); */
    pushw(ssp, sp, 0xe8cd);

    /* this is where we're going to return to */
    int_queue_head[int_queue_running].int_queue_return_addr = (unsigned long) ssp + sp;
    pushw(ssp, sp, vflags);
    /* the code segment of our illegal opcode */
    pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr >> 4);
    /* and the instruction pointer */
    pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr & 0xf);
    LWORD(esp) -= 8;
  }
  else {

    cli();

    pushw(ssp, sp, vflags);
    /* the code segment of our iret */
    pushw(ssp, sp, LWORD(cs));
    /* and the instruction pointer */
    pushw(ssp, sp, LWORD(eip));
    LWORD(esp) -= 6;
  }

  if (current_interrupt < 0x10)
    *OUTB_ADD = 0;
  else
    *OUTB_ADD = 1;

  LWORD(cs) = ((us *) 0)[(current_interrupt << 1) + 1];
  LWORD(eip) = ((us *) 0)[current_interrupt << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */

  REG(eflags) &= ~(VIF | TF | IF | NT);
  REG(eflags) |= VIP;

  int_queue_start = (int_queue_start + 1) % IQUEUE_LEN;
  h_printf("int_queue: running int %x if applicable, return_addr=%x\n", current_interrupt, int_queue_head[int_queue_running].int_queue_return_addr);

  sti();

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

/* Silly Interrupt Generator Initialization/Closedown */

#ifdef SIG
int SillyG = 0;

void 
SIG_init()
{
  /* Get in touch with my Silly Interupt Driver */
  if ((SillyG = open("/dev/int/12", O_RDWR)) < 1) {
    fprintf(stderr, "Not gonna touch INT you requested!\n");
    SillyG = 0;
  }
  else {
    /* Reset interupt incase it went off already */
    write(SillyG, NULL, (int) NULL);
    fprintf(stderr, "Gonna monitor the INT you requested, Return=0x%02x\n", Sill
  }
}

void
 SIG_close() {
  if (SillyG) {
    close(SillyG);
    fprintf(stderr, "Closing INT you opened!\n");
  }
}

#endif

void
 emulate(int argc, char **argv) {
  struct sigaction sa;
  int c;
  char *confname = NULL;
  struct stat statout, staterr;

  config_defaults();

  /* initialize cli() and sti() */
  signal_init();

  iq.queued = 0;
  in_sighandler = 0;
  sync();			/* for safety */

  opterr = 0;
  confname = NULL;
  while ((c = getopt(argc, argv, "ABC:cF:kM:D:P:VNtsgx:Km234e:")) != EOF)
    if (c == 'F')
      confname = optarg;
  parse_config(confname);

  if (config.exitearly)
    leavedos(0);

  optind = 0;
  opterr = 0;
  while ((c = getopt(argc, argv, "ABC:cF:kM:D:P:VNtT:sgx:Km234e:")) != EOF) {
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
      config.exitearly = 1;
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
      vm86s.cpu_type = CPU_286;
      break;

    case '3':
      g_printf("CPU set to 386\n");
      vm86s.cpu_type = CPU_386;
      break;

    case '4':
      g_printf("CPU set to 486\n");
      vm86s.cpu_type = CPU_486;
      break;

    case '?':
    default:
      fprintf(stderr, "unrecognized option: -%c\n\r", c);
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

  /* Set up hard interrupt array */
  {
    u_short counter;

    for (counter = 0; counter < NUM_INT_QUEUE; counter++) {
      int_queue_head[counter].int_queue_return_addr = 0;
      int_queue_head[counter].in_use = 0;
    }
  }

  setbuf(stdout, NULL);

  /* create tmpdir */
  exchange_uids();
  mkdir(tmpdir, S_IREAD | S_IWRITE | S_IEXEC);
  exchange_uids();

  /* allocate screen buffer for non-console video compare speedup */
  scrbuf = malloc(CO * LI * 2);
  v_printf("SCREEN saves at: %p\n", scrbuf);

  /* setup DOS memory, whether shared or not */
  memory_setup();

  video_config_init();

  /* This is needed in the video stuff. Grabbed from boot(). */
  if ((configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE)
    bios_video_port = 0x3b4;	/* base port of CRTC - IMPORTANT! */
  else
    bios_video_port = 0x3d4;	/* base port of CRTC - IMPORTANT! */

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
      warn("WARN: copying VBIOS file from /dev/kmem\n");
      load_file("/dev/kmem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
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

#define SETSIG(sig, fun)	sa.sa_handler = (SignalHandler)fun; \
					sa.sa_flags = SA_RESTART; \
					sigemptyset(&sa.sa_mask); \
					sigaddset(&sa.sa_mask, SIG_TIME); \
					sigaction(sig, &sa, NULL);

#define DPMISETSIG(sig, fun, stack) \
			sa.sa_handler = (__sighandler_t) fun; \
			/* Point to the top of the stack, minus 4 just in case, and make \
			   just in case, and make it aligned  */ \
			sa.sa_restorer = \
			(void (*)()) (((unsigned int)(stack) + sizeof(stack) - 4) & ~3); \
			sa.sa_flags = SA_RESTART; \
			sigemptyset(&sa.sa_mask); \
			sigaddset(&sa.sa_mask, SIG_TIME); \
			dosemu_sigaction(sig, &sa, NULL);

  /* init signal handlers */

  DPMISETSIG(SIGILL, sigill, ill_stack);
  DPMISETSIG(SIG_TIME, sigalrm, alrm_stack);
  SETSIG(SIGFPE, sigfpe);
  DPMISETSIG(SIGTRAP, sigtrap, trap_stack);

  SETSIG(SIGHUP, leavedos);	/* for "graceful" shutdown */
  SETSIG(SIGTERM, leavedos);
  SETSIG(SIGKILL, leavedos);
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

  mouse_init();
  disk_init();
  termioInit();
  DPMISETSIG(SIGSEGV, sigsegv, segv_stack);
  hardware_init();
  if (config.vga)
    vga_initialize();
  clear_screen(bios_current_screen_page, 7);

  boot();

  /*
  boot(config.hdiskboot ? hdisktab : disktab);
*/

  fflush(stdout);
  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = UPDATE / TIMER_DIVISOR;
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = UPDATE / TIMER_DIVISOR;
  setitimer(TIMER_TIME, &itv, NULL);
  k_printf("Used %d for update\n", UPDATE / TIMER_DIVISOR);

  if (!config.console_video)
    vm86s.flags = VM86_SCREEN_BITMAP;
  else
    vm86s.flags = 0;
  vm86s.screen_bitmap = 0;
  scrtest_bitmap = 1 << (24 + bios_current_screen_page);
  update_screen = 1;

  serial_init();
#ifdef SIG
  SIG_init();
#endif

  for (; !fatalerr;) {
    run_vm86();
    serial_run();
    int_queue_run();
  }

  error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	fatalerr, fatalerr, in_sigsegv, ignore_segv);

  sync();
  fprintf(stderr, "ARG!!!!!\n");
  leavedos(99);
}

void
 video_config_init(void) {
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
  bios_current_screen_page = 0x0;	/* Current Screen Page */
}

void
ign_sigs(int sig) {
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
 leavedos(int sig) {
  struct sigaction sa;

  in_vm86 = 0;

  /* remove tmpdir */
  rmdir(tmpdir);

  SETSIG(SIG_TIME, ign_sigs);
  SETSIG(SIGSEGV, ign_sigs);
  SETSIG(SIGILL, ign_sigs);
  SETSIG(SIGFPE, ign_sigs);
  SETSIG(SIGTRAP, ign_sigs);
  error("leavedos(%d) called - shutting down\n", sig);

  close_all_printers();

  serial_close();
  mouse_close();

#ifdef SIG
  SIG_close();
#endif

  show_ints(0, 0x33);
  show_regs();
  fflush(stderr);
  fflush(stdout);

  disk_close_all();
  termioClose();

  _exit(0);
}

void
 hardware_init(void) {
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
 d_ready(int fd) {
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
 usage(void) {
  fprintf(stdout, "$Header: /home/src/dosemu0.60/RCS/emu.c,v 1.86 1994/05/26 23:15:01 root Exp root $\n");
  fprintf(stdout, "usage: dos [-ABCckbVNtsgxKm234e] [-D flags] [-M SIZE] [-P FILE] [ -F File ] 2> dosdbg\n");
  fprintf(stdout, "    -A boot from first defined floppy disk (A)\n");
  fprintf(stdout, "    -B boot from second defined floppy disk (B) (#)\n");
  fprintf(stdout, "    -C boot from first defined hard disk (C)\n");
  fprintf(stdout, "    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
  fprintf(stdout, "    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
  fprintf(stdout, "    -D set debug-msg mask to flags (+-)(vsdDRWkpiwghxmIEc01)\n");
  fprintf(stdout, "    -M set memory size to SIZE kilobytes (!)\n");
  fprintf(stdout, "    -P copy debugging output to FILE\n");
  fprintf(stdout, "    -b map BIOS into emulator RAM (%%)\n");
  fprintf(stdout, "    -F use config-file File\n");
  fprintf(stdout, "    -V use BIOS-VGA video modes (!#%%)\n");
  fprintf(stdout, "    -N No boot of DOS\n");
  fprintf(stdout, "    -t try new timer code (#)\n");
  fprintf(stdout, "    -s try new screen size code (COMPLETELY BROKEN)(#)\n");
  fprintf(stdout, "    -g enable graphics modes (COMPLETELY BROKEN) (!%%#)\n");
  fprintf(stdout, "    -x SIZE enable SIZE K XMS RAM\n");
  fprintf(stdout, "    -e SIZE enable SIZE K EMS RAM\n");
  fprintf(stdout, "    -m enable mouse support (!#)\n");
  fprintf(stdout, "    -2,3,4 choose 286, 386 or 486 CPU\n");
  fprintf(stdout, "    -K Do int9 (!#)\n");
  fprintf(stdout, "\n     (!) means BE CAREFUL! READ THE DOCS FIRST!\n");
  fprintf(stdout, "     (%%) marks those options which require dos be run as root (i.e. suid)\n");
  fprintf(stdout, "     (#) marks options which do not fully work yet\n");
}

void
 saytime(char *m_str) {
  clock_t m_clock;
  struct tms l_time;
  long clktck = sysconf(_SC_CLK_TCK);

  m_clock = times(&l_time);
  fprintf(stderr, "%s %7.2f\n",
	  m_str, m_clock * 1.00 / clktck);

}

int
 ifprintf(unsigned char flg, const char *fmt,...) {
  va_list args;
  char buf[1025];
  int i;

  if (!flg)
    return 0;

  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);

  write(STDERR_FILENO, buf, strlen(buf));
  if (terminal_pipe)
    write(terminal_fd, buf, strlen(buf));
  return i;
}

void
 p_dos_str(char *fmt,...) {
  va_list args;
  char buf[1025], *s;
  int i;

  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);

  s = buf;
  while (*s)
    char_out(*s++, bios_current_screen_page, ADVANCE);
}

void
 ems_helper(void) {
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
 dos_helper(void) {
  switch (LO(ax)) {
  case 0x20:
    mfs_inte6();
    return 1;
    break;
  case 0x21:
    ems_helper();
    return 1;
    break;
  case 0x22:{
      unsigned char *ssp;
      unsigned long sp;

      ssp = (unsigned char *) (REG(ss) << 4);
      sp = (unsigned long) LWORD(esp);

      LWORD(eax) = popw(ssp, sp);
      LWORD(esp) += 2;
      E_printf("EMS: in 0xe6,0x22 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      if (config.ems_size)
	bios_emm_fn(&REGS);
      else
	error("EMS: not running bios_em_fn!\n");
      break;
    }
#ifdef IPX
  case 0x7a:
    if (config.ipxsup) {
      /* TRB handle IPX far calls in dos_helper() */
      IPXFarCallHandler();
    }
    break;
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
      int cflag = LWORD(eflags) & CF ? 1 : 0;

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
      unsigned char *ssp;
      unsigned long sp;

      if (!config.mapped_bios) {
	error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
	return 1;
      }
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
	warn("couldn't get range!\n");
      config.vga = 1;
      set_vc_screen_page(bios_current_screen_page);
      warn("WARNING: jumping to 0[c/e]000:0003\n");

      ssp = (unsigned char *) (REG(ss) << 4);
      sp = (unsigned long) LWORD(esp);
      pushw(ssp, sp, LWORD(cs));
      pushw(ssp, sp, LWORD(eip));
      precard_eip = LWORD(eip);
      precard_cs = LWORD(cs);
      LWORD(esp) -= 4;
      LWORD(cs) = config.vbios_seg;
      LWORD(eip) = 3;
      show_regs();
      card_init = 1;
    }

  case 5:			/* show banner */
    p_dos_str("\n\nLinux DOS emulator " VERSTR " $Date: 1994/05/26 23:15:01 $\n");
    p_dos_str("Last configured at %s\n", CONFIG_TIME);
    p_dos_str("on %s\n", CONFIG_HOST);
    /*      p_dos_str("maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n"); */
    p_dos_str("Bugs & Patches to James MacLean, jmaclean@fox.nstn.ns.ca\n\n");
#ifdef DPMI
    if (config.dpmi_size)
      p_dos_str("DPMI Version 0.9 not fully inplemented, BE CAREFUL!\n\n");
#endif
    break;

  case 6:			/* Do inline int09 insert_into_keybuffer() */
    k_printf("Doing INT9 insert_into_keybuffer() bx=0x%04x\n", LWORD(ebx));
    set_keyboard_bios();
    insert_into_keybuffer();
    break;

#if 0				/* May 17 94 */
  case 7:			/* Do int09 set_keyboard_bios() */
    k_printf("Doing INT9 set_keyboard_bio() ax=0x%04x\n", LWORD(eax));
    set_keyboard_bios();
    LO(ax) = HI(ax);
    break;
#endif

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

  case 0x16:
    /* polling keyboard - needed by INT16 bios inline rotine */
#if 1
    if (config.hogthreshold && KBD_Head == KBD_Tail) {
      static struct timeval tp1;
      static struct timeval tp2;
      static int time_count = 0;

      if (time_count == 0)
	gettimeofday(&tp1, NULL);
      else {
	gettimeofday(&tp2, NULL);
	if ((tp2.tv_sec - tp1.tv_sec) * 1000000 +
	    ((int) tp2.tv_usec - (int) tp1.tv_usec) < config.hogthreshold) {
	  usleep(100);
	}
      }
      time_count = (time_count + 1) % 10;
    }
#endif
    REG(eflags) |= VIF | IF; /* sti with return to dosemu code */
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
 be(uid_t who) {
  if (getuid() != who)
    return setreuid(geteuid(), getuid());
  else
    return 0;
}

inline uid_t
 be_me() {
  if (geteuid() == 0) {
    return setreuid(geteuid(), getuid());
    return 0;
  }
  else
    return geteuid();
}

inline uid_t
 be_root() {
  if (geteuid() != 0) {
    setreuid(geteuid(), getuid());
    return getuid();
  }
  else
    return 0;
}

int
 set_ioperm(int start, int size, int flag) {
  int tmp, s_errno;

#if DO_BE
  uid_t last_me;

#endif

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
