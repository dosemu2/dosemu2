/* Extensions by Robert Sanders, 1992-93
 *
 * DANG_BEGIN_MODULE
 * 
 * Here is where DOSEMU gets booted. From emu.c external calls are made
 * to the specific I/O systems (video/keyboard/serial/etc...) to
 * initialize them. Memory is cleared/set up and the boot sector is read
 * from the boot drive. Many SIGNALS are set so that DOSEMU can 
 * exploit things like timers, I/O signals, illegal instructions, etc...
 * When every system gives the green light, vm86()
 * is called to switch into vm86 mode and start executing i86 code. 
 *
 * The vm86() function will return to DOSEMU when certain `exceptions`
 * occur as when some interrupt instructions occur (0xcd).
 *
 * The top level function emulate() is called from dos.c by way of a
 * dll entry point.
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 * $Date: 1994/12/06 03:33:50 $
 * $Source: /fs1/src/dosemu/dosemu0.53pl36/emu.c,v $
 * $Revision: 2.32 $
 * $State: Exp $
 *
 * $Log: emu.c,v $
 * Revision 2.32  1994/12/06  03:33:50  leisner
 * *** empty log message ***
 *
 * Revision 2.31  1994/11/13  00:40:45  root
 * Prep for Hans's latest.
 *
 * Revision 2.30  1994/11/06  02:35:24  root
 * Testing co -M.
 *
 * Revision 2.28  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.27  1994/10/03  00:24:25  root
 * Checkin prior to pre53_25.tgz
 *
 * Revision 2.26  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.25  1994/09/23  01:29:36  root
 * Prep for pre53_21.
 *
 * Revision 2.24  1994/09/22  23:51:57  root
 * Prep for pre53_21.
 *
 * Revision 2.23  1994/09/20  01:53:26  root
 * Prep for pre53_21.
 *
 * Revision 2.22  1994/09/11  01:01:23  root
 * Prep for pre53_19.
 *
 * Revision 2.21  1994/08/25  00:49:34  root
 * Lutz's STI patches and prep for pre53_16.
 *
 * Revision 2.20  1994/08/17  02:08:22  root
 * Mods to Rain's patches to get all modes back on the road.
 *
 * Revision 2.19  1994/08/14  02:52:04  root
 * Rain's latest CLEANUP and MOUSE for X additions.
 *
 * Revision 2.18  1994/08/09  01:49:57  root
 * Prep for pre53_11.
 *
 * Revision 2.17  1994/08/05  22:29:31  root
 * Prep dir pre53_10.
 *
 * Revision 2.16  1994/08/02  00:08:51  root
 * Markk's latest.
 *
 * Revision 2.15  1994/08/01  14:58:59  root
 * Added detach (-d) option from Karl Hakimian.
 *
 * Revision 2.14  1994/08/01  14:26:23  root
 * Prep for pre53_7  with Markks latest, EMS patch, and Makefile changes.
 *
 * Revision 2.13  1994/07/26  01:12:20  root
 * prep for pre53_6.
 *
 * Revision 2.12  1994/07/14  23:19:20  root
 * Markkk's patches.
 *
 * Revision 2.11  1994/07/09  14:29:43  root
 * prep for pre53_3.
 *
 * Revision 2.10  1994/07/05  21:59:13  root
 * NCURSES IS HERE.
 *
 * Revision 2.9  1994/07/04  23:59:23  root
 * Prep for Markkk's NCURSES patches.
 *
 * Revision 2.8  1994/06/28  22:47:46  root
 * Prep for Markk's latest.
 *
 * Revision 2.7  1994/06/27  02:15:58  root
 * Prep for pre53
 *
 * Revision 2.6  1994/06/24  14:51:06  root
 * Markks's patches plus.
 *
 * Revision 2.5  1994/06/17  00:13:32  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.4  1994/06/14  22:28:38  root
 * Prep for pre51_28.
 *
 * Revision 2.3  1994/06/14  22:00:18  root
 * Alistair's DANG inserted for the first time :-).
 *
 * Revision 2.2  1994/06/14  21:34:25  root
 * Second series of termcap patches.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.89  1994/06/05  21:17:35  root
 * Prep for pre51_24.
 *
 * Revision 1.88  1994/06/03  00:58:55  root
 * pre51_23 prep, Daniel's fix for scrbuf malloc().
 *
 * Revision 1.87  1994/05/30  00:08:20  root
 * Prep for pre51_22 and temp kludge fix for dir a: error.
 *
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
 * DANG_END_CHANGELOG
 */

/*
 * DANG_BEGIN_REMARK
   DOSEMU must not work within the 1 meg DOS limit, so start of code
   is loaded at a higher address, at some time this could conflict with
   other shared libs. If DOSEMU is compiled statically (without shared
   libs), and org instruction is used to provide the jump above 1 meg.
 * DANG_END_REMARK
*/
#ifdef STATIC
__asm__(".org 0x110000");
#endif

/*
 * DANG_BEGIN_FUNCTION jmp_emulate
 *
 * description:
 * This function allows the startup program `dos` to know how to call
 * the emulate function by way of the dll headers.
 * Always make sure that this line is the first of emu.c
 * and link emu.o as the first object file to the lib
 *
 * DANG_END_FUNCTION
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
#include <sys/stat.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <assert.h>

#include <linux/fd.h>
#include <linux/hdreg.h>
#include <sys/vm86.h>
#include <syscall.h>
#include <linux/cdrom.h>
#include <linux/config.h>
#include <linux/utsname.h>

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
#include "hgc.h"
#include "timers.h"
#ifdef NEW_PIC
#include "timer/bitops.h"
#include "timer/pic.h"
#endif
#ifdef DPMI
#include "dpmi/dpmi.h"
#endif
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"
#include "keymaps.h"
#include "cpu.h"


extern void restore_vt (unsigned short);
extern void disallocate_vt (void);
extern unsigned short detach (void);

extern int xms_grab_int15;

extern void getKeys(void);

extern struct pit pit;

extern inline void vm86_GP_fault();

/* Variables for keeping track of signals */
#define MAX_SIG_QUEUE_SIZE 50
static u_short SIGNAL_head=0; u_short SIGNAL_tail=0;
struct  SIGNAL_queue {
  struct sigcontext_struct context;
  void (* signal_handler)(void);
};
static struct SIGNAL_queue signal_queue[MAX_SIG_QUEUE_SIZE];

static void *interrupt_function[0x100];

void SIGIO_call(void);
void SIGALRM_call(void);

char *cstack[16384];

int int_queue_running = 0;
void int_queue_run();

static void default_interrupt(u_char);

#define TIMER_DIVISOR 3

/* DANG_BEGIN_FUNCTION DBGTIME 
 *
 * arguments:
 * x - character to print with time.
 *
 * description:
 *  Inline function to debug time differences between different points of
 * execution within DOSEMU. Thanks Ronnie :-).
 * Only used by developers and not expected to execute in any general 
 * releases.
 *
 * DANG_END_FUNCTION
 */
#define DBGTIME(x) {\
                        struct timeval tv;\
                        gettimeofday(&tv,NULL);\
                        fprintf(stderr,"%c %06d:%06d\n",x,(int)tv.tv_sec,(int)tv.tv_usec);\
                   }

/* Time structures for translating UNIX <-> DOS times */
static struct timeval scr_tv;
static struct itimerval itv;

static time_t start_time;		/* Keep track of times for DOS calls */
static unsigned long last_ticks;
unsigned int check_date;
extern void update_timers(void);

extern void video_config_init(void);
extern int keyboard_init(void);
extern void keyboard_close(void);


/*
   Queue to hold all pending hard-interrupts. When an interrupt is
   placed into this queue, it can include a function to be run
   prior to the actuall interrupt being placed onto the DOS stack,
   as well as include a function to be called after the interrupt
   finishes.
*/
static int int_queue_start = 0;
static int int_queue_end = 0;

#define IQUEUE_LEN 1000
static struct int_queue_struct {
  int interrupt;
  int (*callstart) ();
  int (*callend) ();
} int_queue[IQUEUE_LEN];

/*
   This is here to allow multiple hard_int's to be running concurrently.
   Needed for programs that steal INT9 away from DOSEMU.
*/
#define NUM_INT_QUEUE 64
static struct int_queue_list_struct {
  struct int_queue_struct int_queue_ptr;
  int int_queue_return_addr;
  u_char in_use;
  struct vm86_regs saved_regs;
} int_queue_head[NUM_INT_QUEUE];

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

/* Function to set up all memory areas for DOS, as well as load boot block */
static void boot(void);

extern void map_bios(void);	/* map in VIDEO bios */
extern int open_kmem();		/* Get access to physical memory */

 void usage(void),		/* Print parameters of DOSEMU */
 hardware_init(void);		/* Initialize info on hardware */

static int dos_helper(void);		/* handle int's created especially for DOSEMU */
void do_int(int);
static void setup_interrupts(void);
static void version_init(void);
void init_vga_card(void);	/* Function to set VM86 regs to run VGA initialation */

#ifndef NEW_PIC
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
#endif
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

static int poll_io = 1;		/* polling io, default on */
u_char ignore_segv = 0;		/* ignore sigsegv's */
u_char in_sigsegv = 0;
static int terminal_pipe;
static int terminal_fd = -1;

/*
   This flag will be set when doing video routines so that special
   access can be given
*/
static u_char in_video = 0;

/* for use by cli() and sti() */
sigset_t oldset;

/* Similar to the sigaction function in libc, except it leaves alone the
   restorer field
   stolen from the wine-project */

int
dosemu_sigaction(int sig, struct sigaction *new, struct sigaction *old)
{
  __asm__("int $0x80":"=a"(sig)
	  :"0"(SYS_sigaction), "b"(sig), "c"(new), "d"(old));
  if (sig >= 0)
    return 0;
  errno = -sig;
  return -1;
}

/* DANG_BEGIN_FUNCTION signal_init
 *
 * description:
 *  Initialize the signals to have NONE being blocked.
 * Currently this is NOT of much use to DOSEMU.
 *
 * DANG_END_FUNCTION
 *
 */
void
signal_init(void)
{
  sigset_t trashset;
  u_short counter;

  /* block no additional signals (i.e. get the current signal mask) */
  sigemptyset(&trashset);
  sigprocmask(SIG_BLOCK, &trashset, &oldset);
  g_printf("Initialized all signals to NOT-BLOCK\n");
}


static void
setup_low_mem (void)
{
  char *result;
  g_printf ("Low memory mapping!\n");
  result = mmap (NULL, 0x100000,
                 PROT_EXEC | PROT_READ | PROT_WRITE,
                 MAP_FIXED | MAP_PRIVATE | MAP_ANON,
                 -1, 0);
  if (result != NULL)
    {
      perror ("anonymous mmap");
      leavedos (1);
    }
}

/* 
 * DANG_BEGIN_FUNCTION cli
 *
 * description:
 *  Stop additional signals from interrupting DOSEMU.
 *
 * DANG_END_FUNCTION
 */
void
cli(void)
{
  sigset_t blockset;

  return; /* Should be OK now */
  sigfillset(&blockset);
  DOS_SYSCALL(sigprocmask(SIG_SETMASK, &blockset, &oldset));
}

/* 
 * DANG_BEGIN_FUNCTION sti
 *
 * description:
 *  Allow all signals to interrupt DOSEMU.
 *
 * DANG_END_FUNCTION
 */
void
sti(void)
{
  sigset_t blockset;

  return; /* Should be OK now */
  DOS_SYSCALL(sigprocmask(SIG_SETMASK, &oldset, &blockset));
}

/*
 * DANG_BEGIN_FUNCTION handle_signals
 *
 * description:
 *  Due to signals happening at any time, the actual work to be done 
 * because a signal occurs is done here in a serial fashion.
 *
 * The concept, should this eventualy work, is that a signal should only
 * flag that it has occurred and let DOSEMU deal with it in an orderly 
 * fashion as it executes the rest of it's code.
 *
 * DANG_END_FUNCTION
 *
 */
void handle_signals(void) {
  if ( SIGNAL_head != SIGNAL_tail ) {
    signal_queue[SIGNAL_head].signal_handler();
    SIGNAL_head = (SIGNAL_head + 1) % MAX_SIG_QUEUE_SIZE;
/* 
 * If more SIGNALS need to be dealt with, make sure we request interruption
 * by the kernel ASAP.
 */
      if (SIGNAL_head != SIGNAL_tail) {
        REG(eflags) |= VIP;
      }
  }
}

/*
 * DANG_BEGIN_FUNCTION run_vm86
 *
 * description:
 *  Here is where DOSEMU runs VM86 mode with the vm86() call which
 *  also has the registers that it will be called with. It will stop
 *  vm86 mode for many reasons, like trying to execute an interrupt,
 *  doing port I/O to ports not opened for I/O, etc ...
 *
 * DANG_END_FUNCTION
 */
void
run_vm86(void)
{
  /* FIXME: why static *? */
  static int retval;
  static u_short next_signal=0;
  /*
   * always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */
    in_vm86 = 1;
#ifdef NEW_PIC
    if(pic_icount) 
      REG(eflags) |= (VIP);
#endif
    /* FIXME: this needs to be clarified and rewritten */

    retval = vm86(&vm86s);
    in_vm86 = 0;
    switch VM86_TYPE(retval) {
	case VM86_UNKNOWN:
		vm86_GP_fault();
		break;
	case VM86_STI:
		I_printf("Return from vm86() for timeout\n");
#ifndef NEW_PIC
		REG(eflags) &= ~(VIP);
#else
          	pic_iret();
#endif
		break;
	case VM86_INTx:
		do_int(VM86_ARG(retval));
		break;
	case VM86_SIGNAL:
		I_printf("Return for SIGNAL\n");
		break;
	default:
		error("unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
		fatalerr = 4;
    }

  handle_signals();

  /* 
   * This is here because ioctl() is non-reentrant, and signal handlers
   * may have to use ioctl().  This results in a possible (probable) time
   * lag of indeterminate length (and a bad return value).
   * Ah, life isn't perfect.
   *
   * I really need to clean up the queue functions to use real queues.
   */
  if (iq.queued)
    do_queued_ioctl();
#ifdef NEW_PIC
/* update the pic to reflect IEF */
  if (REG(eflags)&IF_MASK) {
    if (pic_iflag) pic_sti();
    }
  else {
    if (!pic_iflag) pic_cli(); /* pic_iflag=0 => enabled */
    }
#endif
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
    g_printf("%s%s", (configuration & (1 << b)) ? "1" : "0",
		(b % 4) ? "" : " ");

  g_printf("\n");
}

void
dos_ctrl_alt_del(void)
{
  dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
  disk_close();
  clear_screen(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), 7);
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


/* 
 * DANG_BEGIN_FUNCTION memory_init
 * 
 * description:
 *  Set up all memory areas as would be present on a typical i86 during
 * the boot phase.
 *
 * DANG_END_FUNCTION
 *
 */
void memory_init(void) {

  unsigned int i;
  unsigned char *ptr;
  ushort *seg, *off;

  /* first get the bios-template and copy it to it's write place */
  {
    extern void bios_f000(), bios_f000_end();
    FILE *f;
    ptr = (u_char *) (BIOSSEG << 4);
    memcpy(ptr, bios_f000, (unsigned long)bios_f000_end - (unsigned long)bios_f000);
#if 0 /* for debugging only */
    f = fopen("/tmp/bios","w");
    fprintf(stderr,"opened /tmp/bios f=%p\n",f);
    fwrite(ptr,0x8000,2,f);
    fflush(f);
    fclose(f);
#endif
  }

  /* init trapped interrupts called via jump */
  for (i = 0; i < 256; i++) {
    interrupt_function[i] = default_interrupt;
    if ((i & 0xf8) == 0x60)
      continue;			/* user interrupts */
    SETIVEC(i, BIOSSEG, 16 * i);
  }

  /* Next let's set up those interrupts we look after in 
     protected mode.
  */
  setup_interrupts();

  /* Let kernel handle this, no need to return to DOSEMU */
  SETIVEC(0x1c, 0xf01c, 0);

  /* show EMS as disabled */
  SETIVEC(0x67, 0, 0);

  if (mice->intdrv) {
    /* this is the mouse handler */
    ptr = (unsigned char *) (Mouse_ADD+12);
    off = (u_short *) (Mouse_ADD + 8);
    seg = (u_short *) (Mouse_ADD + 10);
    /* tell the mouse driver where we are...exec add, seg, offset */
    mouse_sethandler(ptr, seg, off);
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
    SETIVEC(0x33, Mouse_SEG, Mouse_ROUTINE_OFF);
  }
  else
    *(unsigned char *) (BIOSSEG * 16 + 16 * 0x33) = 0xcf;	/* IRET */

  {
    /* update boot drive in Banner-code */
    extern void bios_f000_bootdrive(), bios_f000();
    ptr = (u_char *)((BIOSSEG << 4) + ((long)bios_f000_bootdrive - (long)bios_f000));
    *ptr = config.hdiskboot ? 0x80 : 0;
  }


  SETIVEC(0x16, INT16_SEG, INT16_OFF);
  SETIVEC(0x09, INT09_SEG, INT09_OFF);
  SETIVEC(0x08, INT08_SEG, INT08_OFF);

  install_int_10_handler();	/* Install the handler for video-interrupt */

  /* This is an int e7 used for FCB opens */
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  /* End of int 0xe7 for FCB opens */

  /* set up relocated video handler (interrupt 0x42) */
#if USE_DUALMON
  if (config.dualmon == 2) {
    interrupt_function[0x42] = int10;
  }
  else
#endif 
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

  /* The default 16-word BIOS key buffer starts at 0x41e */
  KBD_Head =			/* key buf start ofs */
    KBD_Tail =			/* key buf end ofs */
    KBD_Start = 0x1e;		/* keyboard queue start... */
  KBD_End = 0x3e;		/* ...and end offsets from 0x400 */

  keybuf_clear();

  bios_ctrl_alt_del_flag = 0x0000;
  *(char *) 0x496 = 16;		/* 102-key keyboard */

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

  /* Set OUTB_ADD to 1 */
  *OUTB_ADD = 1;
  *LASTSCAN_ADD = 1;
  REG(eflags) |= (IF | VIF | VIP);

  /* 
   * The banner helper actually gets called *after* the VGA card
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

}

static void
boot(void)
{
  char *buffer;
  struct disk *dp = NULL;

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
    dp = &disktab[1];
    break;
  default:
    error("ERROR: unexpected value for config.hdiskboot\n");
    leavedos(15);
  }

  ignore_segv++;

  disk_close();
  disk_open(dp);

  buffer = (char *) 0x7c00;

  if (dp->type == PARTITION) {   /* we boot partition boot record, not MBR! */
    d_printf("Booting partition boot record from part=%s....\n", dp->dev_name);
    if (RPT_SYSCALL(read(dp->fdesc, buffer, SECTOR_SIZE)) != SECTOR_SIZE) {
      error("ERROR: reading partition boot sector using partition %s.\n", dp->dev_name);
      leavedos(16);
    }
  }
  else if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
    error("ERROR: can't boot from %s, using harddisk\n", dp->dev_name);
    dp = hdisktab;
    if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
      error("ERROR: can't boot from hard disk\n");
      leavedos(16);
    }
  }
  disk_close();
  ignore_segv--;
}

void SIGALRM_call(void){

  static volatile int running = 0;
  static volatile inalrm = 0;
  static int partials = 0;
  static u_char timals = 0;
#if VIDEO_CHECK_DIRTY
  static int update_pending = 0;
#endif
  int retval;
  
#ifdef X_SUPPORT
  if (config.X) 
     X_handle_events();
#endif

  /* check for available packets on the packet driver interface */
  /* (timeout=0, so it immediately returns when none are available) */
  pkt_check_receive(0);

  /* If it is running in termcap mode, then update the screen.
   * First it sets a running flag, so as to avoid re-entrancy of 
   * update_screen while it is in use.  After update_screen is done,
   * it returns a nonzero value if there was any updates to the screen.
   * If there were any updates to the screen, then set a countdown value
   * in order to give DOSEMU more CPU time, between screen updates.
   * This increases the DOSEMU-to-termcap update efficiency greatly.
   * The countdown counter is currently at a value of 2.
   */
   
   /* This now (again) tests screen_bitmap, i.e. checks if the screen 
    * was written to at all. This doesn't seem to achieve much for now,
    * but it will be helpful when implementing X graphics.
    * It's a bit tricky, however, because previous calls of update_screen
    * might not have updated the entire screen. Therefore update_pending
    * is set to 1 if only part of the screen was updated (update_screen
    * returns 2), meaning that update_screen will in any case be called
    * next time.
    * (*** this only applies if VIDEO_CHECK_DIRTY is set, which is 
    *      currently not the default! ***)
    *
    * return vales for update_screen are now:    
    *       0 nothing changed
    *       1 changed, entire screen updated
    *       2 changed, only partially updated
    *
    * note that update_screen also updates the cursor.
    */
#ifdef X_SUPPORT
  if (config.X && config.X_blinkrate) {
     X_blink_cursor();
  }
#endif
  if (!running && !video_update_lock) {
    if (Video->update_screen 
#if VIDEO_CHECK_DIRTY
       && (update_pending || vm86s.screen_bitmap&screen_mask)
#endif
       ) 
    {
       running = -1;
       retval = Video->update_screen();
#if 0
       v_printf("update_screen returned %d\n",retval);
#endif
#ifdef X_SUPPORT
       running = retval ? (config.X?config.X_updatefreq:config.term_updatefreq) 
                        : 0;
#else
       running = retval ? config.term_updatefreq : 0;
#endif
#if VIDEO_CHECK_DIRTY
       update_pending=(retval==2);
       vm86s.screen_bitmap=0;
#endif
    }
    else if (Video->update_cursor) {
       Video->update_cursor();
    }
  }
  else if (running > 0) {
    running--;
  }
  
#if 0
  setitimer(TIMER_TIME, &itv, NULL);
#endif

  if (mice->intdrv)
    mouse_curtick();

  /* TRB - perform processing for the IPX Asynchronous Event Service */
#ifdef IPX
  if (config.ipxsup)
    AESTimerTick();
#endif

  if (++timals == TIMER_DIVISOR) {
    timals = 0;
    /* update the Bios Data Area timer dword if interrupts enabled */
/*    if (REG(eflags) & VIF)
      timer_tick();*/
    if (config.timers) {
      h_printf("starting timer int 8...\n");
#ifndef NEW_PIC
      if (!do_hard_int(8))
	h_printf("CAN'T DO TIMER INT 8...IF CLEAR\n");
#else
#if NEW_PIC==2
      age_transmit_queues();
#endif
      pic_request(PIC_IRQ0);
#endif
    }
    else
      h_printf("NOT CONFIG.TIMERS\n");
  }

  if (not_use_sigio)
    io_select(fds_no_sigio);

  /* this is for per-second activities */
  partials++;
  if (partials == FREQ) {
    partials = 0;
    printer_tick((u_long) 0);
    if (config.fastfloppy)
      floppy_tick();
  }

}

/* DANG_BEGIN_FUNCTION SIGNAL_save
 *
 * arguments:
 * context     - signal context to save.
 * signal_call - signal handling routine to be called.
 *
 * description:
 *  Save into an array structure queue the signal context of the current
 * signal as well as the function to call for dealing with this signal.
 * This is a queue because any signal may occur multiple times before
 * DOSEMU deals with it down the road.
 *
 * DANG_END_FUNCTION
 *
 */
inline void SIGNAL_save(
			void (*signal_call)() ) {
  signal_queue[SIGNAL_tail].signal_handler=signal_call;
  SIGNAL_tail = (SIGNAL_tail + 1) % MAX_SIG_QUEUE_SIZE;
  REG(eflags) |= VIP;
}

void
sigalrm(int sig, struct sigcontext_struct context)
{
#ifdef DPMI
  if (in_dpmi && !in_vm86)
    dpmi_sigalrm(&context);
#endif /* DPMI */
  SIGNAL_save(SIGALRM_call);
}

/*
 * DANG_BEGIN_FUNCTION SIGIO_call
 *
 * description:
 *  Whenever I/O occurs on devices allowing SIGIO to occur, DOSEMU
 * will be flagged to run this call which inturn checks which 
 * fd(s) was set and execute the proper routine to get the I/O
 * from that device.
 *
 * DANG_END_FUNCTION
 *
 */
void SIGIO_call(void){
  /* Call select to see if any I/O is ready on devices */
  io_select(fds_sigio);
}

void
sigio(int sig, struct sigcontext_struct context)
{
#ifdef DPMI
  if (in_dpmi && !in_vm86)
    dpmi_sigio(&context);
#endif /* DPMI */
  SIGNAL_save(SIGIO_call);
}

void
sigquit(int sig)
{
  in_vm86 = 0;
  in_sighandler = 1;

  error("ERROR: sigquit called\n");
  show_ints(0, 0x33);
  show_regs(__FILE__, __LINE__);

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
  show_regs(__FILE__, __LINE__);

#ifdef NEW_PIC
  pic_request(PIC_IRQ0);
#else
  do_hard_int(0x8);
#endif

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

void
open_Xkeyboard_pipe(char *path)
{
  keypipe = DOS_SYSCALL(open(path, O_RDWR));
  if (keypipe == -1) {
    keypipe = 0;
    error("ERROR: open_Xkeyboard_pipe failed - cannot open %s!\n", path);
    return;
  }
  return;
}

void
open_Xmouse_pipe(char *path)
{
  mousepipe = DOS_SYSCALL(open(path, O_RDWR));
  if (mousepipe == -1) {
    mousepipe = 0;
    error("ERROR: open_Xmouse_pipe failed - cannot open %s!\n", path);
    return;
  }
  return;
}

/* 
 * DANG_BEGIN_FUNCTION parse_debugflags
 *
 * arguments:
 *  s - string of options.
 * 
 * description:
 *  This part is fairly flexible...you specify the debugging flags you wish
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
 *
 * DANG_END_FUNCTION
 */
void
parse_debugflags(const char *s)
{
  char c;
  unsigned char flag = 1;
  const char allopts[] = "vsdDRWkpiwghxmIEcXP";

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
#ifdef X_SUPPORT
    case 'X':
      d.X = flag;
      break;
#endif
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
    case 'P':
      d.pd = flag;
      d.network=flag;
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

/*
 * DANG_BEGIN_FUNCTION config_defaults
 *
 * description:
 *  Set all values in the `config` structure to their default value.
 *  These will be modified by the config parser.
 *
 * DANG_END_FUNCTION
 *
 */
void
config_defaults(void)
{
  config.hdiskboot = 1;		/* default hard disk boot */

  config.mem_size = 640;
  config.ems_size = 0;
  config.ems_frame = 0xd000;
  config.xms_size = 0;
  config.dpmi_size = 0;
  config.mathco = 1;
  config.mouse_flag = 0;
  config.mapped_bios = 0;
  config.mapped_sbios = 0;
  config.vbios_file = NULL;
  config.vbios_copy = 0;
  config.vbios_seg = 0xc000;
  config.vbios_size = 0x10000;
  config.console_keyb = 0;
  config.kbd_tty = 0;
  config.console_video = 0;
  config.fdisks = 0;
  config.hdisks = 0;
  config.bootdisk = 0;
  config.exitearly = 0;
  config.term_method = METHOD_FAST;
  config.term_color = COLOR_NORMAL;
  config.term_updatelines = 25;
  config.term_updatefreq = 2;
  config.term_charset = CHARSET_LATIN;
  config.term_corner = 1;
  config.X_updatelines = 25;
  config.X_updatefreq  = 8;
  config.X_display     = NULL;     /* NULL means use DISPLAY variable */
  config.X_title       = "dosemu";
  config.X_icon_name   = "dosemu";
  config.X_blinkrate   = 8;
  config.X_keycode     = 0;
  config.X_font = "vga";
  config.usesX   = 0;
  config.X   = 0;
  config.hogthreshold = 5000;	/* in usecs */
  config.chipset = PLAINVGA;
  config.cardtype = CARD_VGA;
  config.fullrestore = 0;
  config.graphics = 0;
  config.gfxmemsize = 256;
  config.vga = 0;		/* this flags BIOS graphics */
  config.dualmon =0;

  config.speaker = SPKR_EMULATED;

#if 0 /* This is too slow, but why? */
  config.update = 54945;
#else
  config.update = 27472;
#endif
  config.freq = 18;		/* rough frequency */

  config.timers = 1;		/* deliver timer ints */
  config.keybint = 0;		/* keyboard interrupts */

  config.num_ser = 0;
  config.num_lpt = 0;
  vm86s.cpu_type = CPU_386;
  config.fastfloppy = 1;

  config.emusys = (char *) NULL;
  config.emubat = (char *) NULL;
  tmpdir = strdup(tempnam("/tmp", "dosemu"));
  config.dosbanner = 1;
  config.allowvideoportaccess = 0;

  config.keyboard = KEYB_US;	/* What's the current keyboard  */
  config.key_map = key_map_us;	/* pointer to the keyboard-maps */
  config.shift_map = shift_map_us;	/* Here the Shilt-map           */
  config.alt_map = alt_map_us;	/* And the Alt-map              */
  config.num_table = num_table_dot;	/* Numeric keypad has a dot     */
  config.detach = 0; /* Don't detach from current tty and open new VT. */

  config.sillyint = 0;
  config.must_spare_hardware_ram =0;
  memset(config.hardware_pages,0,sizeof(config.hardware_pages));
  
  mice->fd         = -1;
  mice->type       = 0;
  mice->flags      = 0;
  mice->intdrv     = 0;
  mice->cleardtr   = 0;
  mice->baudRate   = 0;
  mice->sampleRate = 0;
  mice->lastButtons= 0;
  mice->chordMiddle= 0;
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
void
int_queue_run(void)
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
g_printf("Still using int_queue_run()\n");

  if (!*OUTB_ADD) {
    if (++vif_counter > 0x08) {
      I_printf("OUTB interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("OUTB_ADD = %d , returning from int_queue_run()\n", *OUTB_ADD);
      return;
    }
  }

  if (!(REG(eflags) & VIF)) {
    if (++vif_counter > 0x08) {
      I_printf("VIF interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("interrupts disabled while int_queue_run()\n");
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
  if (int_queue[int_queue_start].callstart)
    REG(eflags) |= VIP;

  int_queue_start = (int_queue_start + 1) % IQUEUE_LEN;
  h_printf("int_queue: running int %x if applicable, return_addr=%x\n", current_interrupt, int_queue_head[int_queue_running].int_queue_return_addr);

  return;
}

/* Silly Interrupt Generator Initialization/Closedown */

#ifdef SIG
SillyG_t *SillyG = 0;
static SillyG_t SillyG_[16+1];

/* 
 * DANG_BEGIN_FUNCTION SIG_int
 *
 * description:
 *  Allow DOSEMU to be made aware when a hard interrupt occurs
 *  Requires the sig/sillyint.o driver loaded (using NEW modules package),
 *  or a kernel patch (implementing sig/int.c driver).
 *
 *  The IRQ numbers to monitor are taken from config.sillyint, each bit
 *  corresponding to one IRQ. The higher 16 bit are defining the use of SIGIO
 *
 * DANG_END_FUNCTION
 */
void 
SIG_init()
{
  /* Get in touch with my Silly Interrupt Driver */
  if (config.sillyint) {
    char devname[20];
    char prio_table[]={9,10,11,12,14,15,3,4,5,6,7};
    int i,irq,fd;
    SillyG_t *sg=SillyG_;
    for (i=0; i<sizeof(prio_table); i++) {
      irq=prio_table[i];
      if (config.sillyint & (1 << irq)) {
#ifdef REQUIRES_EMUMODULE
        if (emusyscall(EMUSYS_REQUEST_IRQ, irq)<0) {
          g_printf("Not gonna touch IRQ %d you requested!\n",irq);
        }
        else {
          g_printf("Gonna monitor the IRQ %d you requested\n", irq);
          sg->fd=-1;
#else
        sprintf(devname, "/dev/int/%d", irq);
        if ((fd = open(devname, O_RDWR)) < 1) {
          g_printf("Not gonna touch IRQ %d you requested!\n",irq);
        }
        else {
          /* Reset interupt incase it went off already */
          RPT_SYSCALL(write(fd, NULL, (int) NULL));
          g_printf("Gonna monitor the IRQ %d you requested, Return=0x%02x\n", irq ,fd);
          if (config.sillyint & (0x10000 << irq)) {
            /* Use SIGIO, this should be faster */ 
            add_to_io_select(fd, 1);
          }
          /* DANG_BEGIN_REMARK
           * At this time we have to use SIGALRM in addition to SIGIO
           * I don't (yet) know why the SIGIO signal gets lost sometimes
           * (once per minute or longer). 
           * But if it happens, we can retrigger this way over SIGALRM.
           * Normally SIGIO happens before SIGALARM, so nothing hurts.
           * (Hans)
           * DANG_END_REMARK
           */  
#if 0
          else 
#endif
          {
            /* use SIGALRM  */
            add_to_io_select(fd, 0);
          }
          sg->fd=fd;
#endif /* NOT REQUIRES_EMUMODULE */
#ifndef NEW_PIC
          (sg++)->irq=irq;
#else
    	  sg->irq = pic_irq_list[irq];
    	  g_printf("SIG %x: enabling interrupt %x\n", irq, sg->irq);
    	  pic_seti(sg->irq,do_irq,0);
    	  pic_unmaski(sg->irq);
	  sg++;
#endif
        }
      }
    }
    sg->fd=0;
    if (sg != SillyG_) SillyG=SillyG_;
  }
}

void
 SIG_close() {
  if (SillyG) {
    SillyG_t *sg=SillyG;
#ifdef REQUIRES_EMUMODULE
    while (sg->fd) {
      emusyscall(EMUSYS_FREE_IRQ,sg->irq);
      sg++;
    }
#else
    while (sg->fd) close((sg++)->fd);
#endif
    fprintf(stderr, "Closing all IRQ you opened!\n");
  }
}

#endif



/*
 * DANG_BEGIN_FUNCTION check_special_mapping
 *
 * description:
 *  This is called after all configuration stuff is done to make sure
 *  that no mapped areas are overlapping.
 *  It checks EMS, VBIOS, HARDWARE_RAM
 *  and exits with "false", if any of it overlapp. 
 * DANG_END_FUNCTION
 *
 */
static int check_special_mapping()
{
  #define _I_ ((i-0xc0000) >> 12)
  char map[(0x100000 - 0xc0000) >> 12];
  int i;
  memset(map,0,sizeof(map));
  if (config.ems_size) {
    for (i=EMM_BASE_ADDRESS; i<(EMM_BASE_ADDRESS+0x10000); i+=0x1000) map[_I_]=1;
  }
  for (i=VBIOS_START; i < (VBIOS_START+VBIOS_SIZE); i+=0x1000) {
    if (map[_I_]) return 0;
    map[_I_]=1;
  }
  if (config.must_spare_hardware_ram) {
    for (i=HARDWARE_RAM_START; i < HARDWARE_RAM_STOP; i+=0x1000) {
      if (config.hardware_pages[_I_ - ((HARDWARE_RAM_START-0xc0000)>>12)]) {
        if (map[_I_]) return 0;
      }
    }
  }
  return 1;
  #undef _I_
}




/*
 * DANG_BEGIN_FUNCTION emulate
 *
 * arguments:
 * argc - Argument count.
 * argv - Arguments.
 *
 * description:
 *  Emulate gets called from dos.c. It initializes DOSEMU to prepare it
 *  for running in vm86 mode. This involves catching signals, preparing
 *  memory, calling all the initialization functions for the I/O
 *  subsystems (video/serial/etc...), getting the boot sector instructions
 *  and calling vm86().
 *
 * DANG_END_FUNCTION
 *
 */
void
 emulate(int argc, char **argv) {
  struct sigaction sa;
  int c;
  char *confname = NULL;
  struct stat statout, staterr;
  extern void map_hardware_ram(void);

 /* DANG_BEGIN_REMARK
  * If DOSEMU starts up with stderr == stdout, then stderr gets 
  * redirected to '/dev/null'.
  * DANG_END_REMARK
  */
  fstat(STDOUT_FILENO, &statout);
  fstat(STDERR_FILENO, &staterr);
  if (staterr.st_ino == statout.st_ino) {
    if (freopen("/dev/null", "ab", stderr) == (FILE *) - 1) {
      fprintf(stdout, "ERROR: Could not redirect STDERR to /dev/null!\n");
      exit(-1);
    }
  }

  config_defaults();

  iq.queued = 0;
  in_sighandler = 0;
  sync();			/* for safety */

/*
 * DANG_BEGIN_REMARK
 *  For simpler support of X, DOSEMU can be started by a symbolic
 *  link called `xdos` which DOSEMU will use to switch into X-mode.
 * DANG_END_REMARK
 */
#ifdef X_SUPPORT
  {  char *p;
     p = strrchr(argv[0],'/');    /* parse the program name */
     p = p ? p+1 : argv[0];
    
     if (strcmp(p,"xdos")==0)
        config.X=1;               /* activate X mode if dosemu was */ 
                                  /* called as 'xdos'              */
  }
#endif
     
  opterr = 0;
  confname = NULL;
  while ((c = getopt(argc, argv, "ABCcF:kM:D:P:VNtsgx:Km234e:dXY:Z:")) != EOF) {
	  switch (c) {
	  case 'F':
		  confname = optarg;
		  break;
	  case 'd':
		  if (config.detach)
			  break;
		  config.detach = (unsigned short)detach();
		  break;
	  case 'D':
	     parse_debugflags(optarg);
	     break;
	  }
  }

  parse_config(confname);

  if (config.exitearly)
    leavedos(0);

  optind = 0;
  opterr = 0;
  while ((c = getopt(argc, argv, "ABCcF:kM:D:P:VNtT:sgx:Km234e:dXY:Z:")) != EOF) {
    switch (c) {
    case 'F':			/* previously parsed config file argument */
    case 'd':
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
    case 'X':
#ifdef X_SUPPORT
      config.X = 1;
#else
      error("X support not compiled in\n");
#endif
      break;
    case 'Y':
      open_Xkeyboard_pipe(optarg);
      config.cardtype = CARD_MDA;
      config.mapped_bios = 0;
      config.vbios_file = NULL;
      config.vbios_copy = 0;
      config.vbios_seg = 0xc000;
      config.console_video = 0;
      config.chipset = 0;
      config.fullrestore = 0;
      config.graphics = 0;
      config.vga = 0;  /* this flags BIOS graphics */
      config.usesX = 1;
      config.console_keyb = 1;
      break;
    case 'Z':
      open_Xmouse_pipe(optarg);
      config.usesX = 1;
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
  if (config.X) {
    config.console_video = config.vga = config.graphics = 0;
  }

#ifdef REQUIRES_EMUMODULE
  resolve_emusyscall();
  if (EMUSYS_AVAILABLE) {
    if (emusyscall(EMUSYS_GETVERSION,0) < EMUSYSVERSION) {
      fprintf(stderr, "emumodule not loaded or wrong version\n\r");
      fflush(stdout);
      fflush(stderr);
      _exit(1);
    }
  }
#endif

  if (!check_special_mapping()) error("ERROR: You have overlapping mappings (EMS,VBIOS,HARDWARE_RAM)\n");

  setup_low_mem();

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

  /* do time stuff - necessary for initial time setting */
  {
    struct timeval tp;
    struct timezone tzp;
    struct tm *tm;
    unsigned long ticks;

    time(&start_time);
    tm = localtime((time_t *) &start_time);
    g_printf("Set date %02d.%02d.%02d\n", tm->tm_mday, tm->tm_mon, tm->tm_year);
#if 0
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
#endif
    last_ticks = (tm->tm_hour * 60 * 60 + tm->tm_min * 60 + tm->tm_sec) * 18.206;
    check_date = tm->tm_year * 10000 + tm->tm_mon * 100 + tm->tm_mday;
    set_ticks(last_ticks);

    update_timers();

  };

  /* initialize cli() and sti() */
  signal_init();

  /* init signal handlers */

  NEWSETSIG(SIGILL, dosemu_fault);
  NEWSETQSIG(SIG_TIME, sigalrm);
  NEWSETSIG(SIGFPE, dosemu_fault);
  NEWSETSIG(SIGTRAP, dosemu_fault);

#ifdef SIGBUS /* for newer kernels */
  NEWSETSIG(SIGBUS, dosemu_fault);
#endif

  SETSIG(SIGHUP, leavedos);	/* for "graceful" shutdown */
  SETSIG(SIGTERM, leavedos);
#if 0 /* Richard Stevens says it can't be caught. It's returning an
       * error anyway
       */
  SETSIG(SIGKILL, leavedos);
#endif
  SETSIG(SIGQUIT, sigquit);
/*
  SETSIG(SIGUNUSED, timint);
*/
  NEWSETQSIG(SIGIO, sigio);
  NEWSETSIG(SIGSEGV, dosemu_fault);

  vm86s.flags=0;

  /* Initialize both fd_sets to 0 */
  FD_ZERO(&fds_sigio);
  FD_ZERO(&fds_no_sigio);

  /*
   * Check the version of the OS and set parms accordingly
   */
  version_init();

  /* 
   * Setup DOS emulated memory, HMA, EMS, XMS . Also clear all
   * low memory, so this must be called prior to setting up
   * low memory.
   */
  memory_setup();

  /* 
   * map in some ram locations we need for some adapter's memory mapped IO 
   * (those, who are defined via hardware_ram config)
   */
  map_hardware_ram();  

  serial_init();
  mouse_init();
  printer_init();

  /* 
   * Verify that Keyboard is OK as well as turn off some
   * options if not at a console
   */
  if (keyboard_init() != 0) {
    error("ERROR: can't open keyboard\n");
    leavedos(19);
  }
  if (!config.vga)
    config.allowvideoportaccess = 0;

  /* initialize some video config variables, possibly map video bios,
     get graphics chars
  */
  video_config_init();

#ifdef SIG
  SIG_init();
#endif
  disk_init();
  hardware_init();
  config_init();
  cpu_init();
  cmos_init();

  /* Setup specific memory addresses */
  memory_init();

  boot();

  if (not_use_sigio)
    k_printf("Atleast 1 NON-SIGIO file handle in use.\n");
  else
    k_printf("No NON-SIGIO file handles in use.\n");

  fflush(stdout);

  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = UPDATE / TIMER_DIVISOR;
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = UPDATE / TIMER_DIVISOR;
  k_printf("Used %d for updating timers\n", UPDATE / TIMER_DIVISOR);
  setitimer(TIMER_TIME, &itv, NULL);

  g_printf("EMULATE\n");

  while(!fatalerr) {
    run_vm86();
#ifdef CONFIG_CORRECT_STI /* defined in linux/autoconfig.h */
    if (!(REG(eflags) & STIP_MASK))
#endif
    {
#ifdef NEW_PIC
/*  trigger any hardware interrupts requested */
    run_irqs();
#endif
      serial_run();

      int_queue_run();
    }
  }

  error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	fatalerr, fatalerr, in_sigsegv, ignore_segv);

  sync();
  fprintf(stderr, "ARG!!!!!\n");
  leavedos(99);
}


static void
ign_sigs(int sig) {
  static int timerints = 0;
  static int otherints = 0;

  error("ERROR: signal %d received in leavedos()\n", sig);
  show_regs(__FILE__, __LINE__);
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

  setitimer(TIMER_TIME, NULL, NULL);
  SETSIG(SIG_TIME, ign_sigs);
  SETSIG(SIGSEGV, ign_sigs);
  SETSIG(SIGILL, ign_sigs);
  SETSIG(SIGFPE, ign_sigs);
  SETSIG(SIGTRAP, ign_sigs);
  error("leavedos(%d) called - shutting down\n", sig );

  g_printf("calling close_all_printers\n");
  close_all_printers();

  g_printf("calling serial_close\n");
  serial_close();
  g_printf("calling mouse_close\n");
  mouse_close();

#ifdef SIG
  g_printf("calling SIG_close\n");
  SIG_close();
#endif

  show_ints(0, 0x33);
  g_printf("calling disk_close_all\n");
  disk_close_all();
  g_printf("calling video_close\n");
  video_close();
  g_printf("calling keyboard_close\n");
  keyboard_close();
  fflush(stderr);
  fflush(stdout);

   if (config.detach) {
     restore_vt(config.detach);
     disallocate_vt ();
   }
  exit(sig);
}

/*
 * DANG_BEGIN_FUNCTION hardware_init
 *
 * description:
 *  Initialize any leftover hardware. 
 * 
 * DANG_END_FUNCTION
 */
void
 hardware_init(void) {
  int i;

  /* PIC init */
#ifdef NEW_PIC
   pic_seti(PIC_IRQ0, do_irq, 0);  /* only dos code to run */
   pic_unmaski(PIC_IRQ0);
   pic_seti(PIC_IRQ1, do_irq1, 0); /* do_irq1 in dosio.c   */
   pic_unmaski(PIC_IRQ1);
#else 
  for (i = 0; i < 2; i++) {
    pics[i].OCW1 = 0;		/* no IRQ's serviced */
    pics[i].OCW2 = 0;		/* no EOI's received */
    pics[i].OCW3 = 8;		/* just marks this as OCW3 */
  }
#endif

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
  fprintf(stdout, "$Header: /fs1/src/dosemu/dosemu0.53pl36/emu.c,v 2.32 1994/12/06 03:33:50 leisner Exp leisner $\n");
  fprintf(stdout, "usage: dos [-ABCckbVNtsgxKm234e] [-D flags] [-M SIZE] [-P FILE] [ -F File ] 2> dosdbg\n");
  fprintf(stdout, "    -A boot from first defined floppy disk (A)\n");
  fprintf(stdout, "    -B boot from second defined floppy disk (B) (#)\n");
  fprintf(stdout, "    -C boot from first defined hard disk (C)\n");
  fprintf(stdout, "    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
  fprintf(stdout, "    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
#ifdef X_SUPPORT
  fprintf(stdout, "    -X run in X Window (#)\n");
#endif
  fprintf(stdout, "    -X NAME use MDA direct and FIFO NAME for keyboard (only with x2dos!\n");
  fprintf(stdout, "    -Y NAME use FIFO NAME for mouse (only with x2dos!\n");
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

int
 ifprintf(unsigned char flg, const char *fmt,...) {
  va_list args;
  char buf[1025];
  int i;

#ifdef SHOW_TIME
  static int first_time = 1;
  static int show_time =  0;
#endif

  if (!flg)
    return 0;

#ifdef SHOW_TIME
  if(first_time)  {
	if(getenv("SHOWTIME"))
		show_time = 1;
	first_time = 0;
  }
#endif
	
  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);

#ifdef SHOW_TIME
  if(show_time) {
	struct timeval tv;
	int result;
	char tmpbuf[1024];
	result = gettimeofday(&tv, NULL);
	assert(0 == result);
	sprintf(tmpbuf, "%d.%d: %s", tv.tv_sec, tv.tv_usec, buf);
#else
	sprintf(buf, "%s", buf);
#endif
	
#ifdef SHOW_TIME
	strcpy(buf, tmpbuf);
  }
#endif

  write(STDERR_FILENO, buf, strlen(buf));
  if (terminal_pipe) {
    write(terminal_fd, buf, strlen(buf));
  }
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
  while (*s) char_out(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
}

int cdrom_fd = -1;

unsigned int device_status;
struct audio_status { unsigned int status;
		      unsigned char media_changed;
                      unsigned char paused_bit;
                      unsigned int last_StartSector,last_EndSector;
                      unsigned char outchan0,outchan1,outchan2,outchan3;
                      unsigned char volume0,volume1,volume2,volume3;
                    } audio_status;


#define CALC_PTR(PTR,OFFSET,RESULT_TYPE) ((RESULT_TYPE *)(PTR+OFFSET))


                    
#define MSCD_GETVOLUMESIZE_SIZE    1

#define MSCD_READ_ADRESSING       13
#define MSCD_READ_STARTSECTOR     20
#define MSCD_READ_NUMSECTORS      18

#define MSCD_SEEK_STARTSECTOR     20

#define MSCD_PLAY_ADRESSING       13
#define MSCD_PLAY_STARTSECTOR     14
#define MSCD_PLAY_NUMSECTORS      18

#define MSCD_LOCH_ADRESSING        1
#define MSCD_LOCH_LOCATION         2

#define MSCD_DISKINFO_LTN          1
#define MSCD_DISKINFO_HTN          2
#define MSCD_DISKINFO_LEADOUT      3

#define MSCD_TRACKINFO_TRACKNUM    1
#define MSCD_TRACKINFO_TRACKPOS    2
#define MSCD_TRACKINFO_CTRL        6

#define MSCD_QCHAN_CTRL            1
#define MSCD_QCHAN_TNO             2
#define MSCD_QCHAN_IND             3
#define MSCD_QCHAN_MIN             4
#define MSCD_QCHAN_SEC             5
#define MSCD_QCHAN_FRM             6
#define MSCD_QCHAN_ZERO            7
#define MSCD_QCHAN_AMIN            8
#define MSCD_QCHAN_ASEC            9
#define MSCD_QCHAN_AFRM           10

#define MSCD_AUDSTAT_PAUSED        1
#define MSCD_AUDSTAT_START         3
#define MSCD_AUDSTAT_END           7

#define MSCD_CTRL_VOLUME0          2
#define MSCD_CTRL_VOLUME1          4
#define MSCD_CTRL_VOLUME2          6
#define MSCD_CTRL_VOLUME3          8


void cdrom_reset()
{
  /* after a disk change a new read access will 
     return in error. In order to unlock this condition
     the drive must be reopend.
     Does some one knows a better way?                   */
  close (cdrom_fd); 
  cdrom_fd = open ("/dev/cdrom", O_RDONLY);
 }
#define MSCD_AUDCHAN_VOLUME0       2
#define MSCD_AUDCHAN_VOLUME1       4
#define MSCD_AUDCHAN_VOLUME2       6
#define MSCD_AUDCHAN_VOLUME3       8

                                            
static void cdrom_helper(void)
{
   unsigned char *req_buf,*transfer_buf;
   unsigned int Sector_plus_150,Sector;
   struct cdrom_msf cdrom_msf;
   struct cdrom_subchnl cdrom_subchnl;
   struct cdrom_tochdr cdrom_tochdr;
   struct cdrom_tocentry cdrom_tocentry;
   struct cdrom_volctrl cdrom_volctrl;
   int n,ioctlin;
   
                /* error ("call function %d !\n", HI(ax));

                ioctlin = 0;
                req_buf = SEG_ADR((char *), es, di);
                error ("\ndebug cdrom: \n");
                for (n = 1; n <= req_buf[0]; ++n) {
                   error ("  %3x", req_buf[n-1]);
                   if ((n % 8) == 0)
                     error ("\n");
                };
                error ("\n");
                if (req_buf[2] == 3) {
                  ioctlin = 1;
                  error ("Ein ioctlin request : ");
                  req_buf = SEG_ADR((char *), ds, si);
                  for (n = 0; n <= 9; ++n)
                     error ("  %3x", req_buf[n]);
                  error ("\n");
                }
                error ("\n");
                */
   switch (HI(ax)) {
     case 0x01: audio_status.status = 0x00000310;
                audio_status.paused_bit = 0;
                audio_status.media_changed = 0;
                audio_status.volume0 = 0xFF;
                audio_status.volume1 = 0xFF;
                audio_status.volume2 = 0;
                audio_status.volume3 = 0;
                audio_status.outchan0 = 0;
                audio_status.outchan1 = 1;
                audio_status.outchan2 = 2;
                audio_status.outchan3 = 3;
                
                cdrom_fd = open ("/dev/cdrom", O_RDONLY);
                
                if (cdrom_fd < 0) 
                  LO(ax) = 1;
                 else LO(ax) = 0;
                break;
     case 0x02: /* read long */
                req_buf = SEG_ADR((char *), es, di);
                transfer_buf = SEG_ADR((char *), ds, si);
                
                if (*CALC_PTR(req_buf,MSCD_READ_ADRESSING,u_char) == 1) {
                  cdrom_msf.cdmsf_min0   = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR+2,u_char);
                  cdrom_msf.cdmsf_sec0   = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR+1,u_char);
                  cdrom_msf.cdmsf_frame0 = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR+0,u_char);
                  Sector = cdrom_msf.cdmsf_min0*60*75+cdrom_msf.cdmsf_sec0*75
                            +cdrom_msf.cdmsf_frame0-150;
                 }                
                 else { Sector = *CALC_PTR(req_buf,MSCD_READ_STARTSECTOR,u_long);
                      }
                lseek (cdrom_fd, Sector*2048, SEEK_SET);
                if (read (cdrom_fd, transfer_buf, *CALC_PTR(req_buf,MSCD_READ_NUMSECTORS,u_short)*2048) < 0)
                  LO(ax) = 1;
                 else LO(ax) = 0;
                break; 
     case 0x03: /* seek */
                req_buf = SEG_ADR((char *), es, di);
                lseek (cdrom_fd, *CALC_PTR(req_buf,MSCD_SEEK_STARTSECTOR,u_long)*2048, SEEK_SET);
                break;
     case 0x04: /* play */
                req_buf = SEG_ADR((char *), es, di);
                if (*CALC_PTR(req_buf,MSCD_PLAY_ADRESSING,u_char) == 1) {
                  cdrom_msf.cdmsf_min0   = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR+2,u_char);
                  cdrom_msf.cdmsf_sec0   = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR+1,u_char);
                  cdrom_msf.cdmsf_frame0 = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR+0,u_char);
                  Sector_plus_150 = cdrom_msf.cdmsf_min0*60*75+cdrom_msf.cdmsf_sec0*75
                                      +cdrom_msf.cdmsf_frame0;
                  audio_status.last_StartSector = Sector_plus_150;
                 }                
                 else { Sector_plus_150 = *CALC_PTR(req_buf,MSCD_PLAY_STARTSECTOR,u_long) + 150;
                        cdrom_msf.cdmsf_min0   = (Sector_plus_150 / (60*75));
                        cdrom_msf.cdmsf_sec0   = (Sector_plus_150 % (60*75)) / 75;
                        cdrom_msf.cdmsf_frame0 = (Sector_plus_150 % (60*75)) % 75;
                        audio_status.last_StartSector = Sector_plus_150;
                      }
                Sector_plus_150 += *CALC_PTR(req_buf,MSCD_PLAY_NUMSECTORS,u_long); 
                cdrom_msf.cdmsf_min1   = (Sector_plus_150 / (60*75));
                cdrom_msf.cdmsf_sec1   = (Sector_plus_150 % (60*75)) / 75;
                cdrom_msf.cdmsf_frame1 = (Sector_plus_150 % (60*75)) % 75;

                audio_status.last_EndSector = Sector_plus_150;
                audio_status.paused_bit = 0;
                if (ioctl (cdrom_fd, CDROMPLAYMSF, &cdrom_msf)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMPLAYMSF, &cdrom_msf)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                LO(ax) = 0;
                break;                 
     case 0x05: /* pause (stop) audio */
                LO(ax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl) == 0) {
                  if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY) {
                    audio_status.last_StartSector =             
                                cdrom_subchnl.cdsc_absaddr.msf.minute*60*75
                                +cdrom_subchnl.cdsc_absaddr.msf.second*75
                                +cdrom_subchnl.cdsc_absaddr.msf.frame;
                    ioctl (cdrom_fd, CDROMPAUSE, NULL);
                    audio_status.paused_bit = 1;
                   }
                  else { audio_status.last_StartSector = 0;
                         audio_status.last_EndSector = 0;
                         audio_status.paused_bit = 0;
                       }
                 }
                 else { audio_status.last_StartSector = 0;
                        audio_status.last_EndSector = 0;
                        audio_status.paused_bit = 0;
                        audio_status.media_changed = 1;
                      }
                break;
     case 0x06: /* resume audio */
                LO(ax) = 0;
                if (audio_status.paused_bit) {
                  if (ioctl (cdrom_fd, CDROMRESUME, NULL) == 0) {
                    audio_status.paused_bit = 0;
                    HI(ax) = 1;
                  }
                 }
                else LO(ax) = 1;
                break;                
     case 0x07: /* location of head */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;   

                req_buf = SEG_ADR((char *), ds, si);
                if (*CALC_PTR(req_buf,MSCD_LOCH_ADRESSING,u_char) == 0) {
                  *CALC_PTR(req_buf,MSCD_LOCH_LOCATION,u_long)
                     = cdrom_subchnl.cdsc_absaddr.msf.minute*60*75
                            +cdrom_subchnl.cdsc_absaddr.msf.second*75
                             +cdrom_subchnl.cdsc_absaddr.msf.frame-150;                                                
                 }
                 else {/* red book adressing */
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+3,u_char) = 0;
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+2,u_char) = cdrom_subchnl.cdsc_absaddr.msf.minute;
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+1,u_char) = cdrom_subchnl.cdsc_absaddr.msf.second;
                       *CALC_PTR(req_buf,MSCD_LOCH_LOCATION+0,u_char) = cdrom_subchnl.cdsc_absaddr.msf.frame;
                      }
                break;
     case 0x08: /* return sectorsize */
                LO(ax) = 0;
                LWORD(ebx) = 2048;
                break;
     case 0x09: /* media changed */
                /* this function will be called from MSCDEX before 
                   each new disk access !                         */
                HI(ax) = 0; LO(ax) = 0; LO(bx) = 0;
                if ((audio_status.media_changed) ||
                      ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 0;
                  LO(bx) = 1; /* media has been changed */
                  if (! ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* new disk inserted */
                    cdrom_reset();
                  }
                 }
                 else /* media has not changed, check audio status */
                      if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                        HI(ax) = 1; /* audio playing in progress */
                break;
     case 0x0A: /* device status */
                HI(ax) = 0; LO(ax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl))
                    { /* no disk in drive */
                      LWORD(ebx) = audio_status.status | 0x800;
                      break;
                    }
                   else { /* disk has been changed; new disk in drive ! */
                          cdrom_reset();
                        }
                /* disk in drive */
                LWORD(ebx) = audio_status.status;
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;
                break;
     case 0x0B: /* drive reset */
                LO(ax) = 0;
                break;
     case 0x0C: /* lock/unlock door */
                if (LO(bx) == 1)
                  audio_status.status &= 0xFFFFFFFD;
                 else audio_status.status |= 0x2;
                LO(ax) = 0;
                break;
     case 0x0D: /* eject */
                LO(ax) = 0;
                if (audio_status.status & 0x02) /* drive unlocked ? */
                  if (ioctl (cdrom_fd, CDROMEJECT, NULL))
                    LO(ax) = 1;
                break;
     case 0x0E: /* close tray */
                LO(ax) = 0;
                break;
     case 0x0F: /* audio channel control */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;
 
                req_buf = SEG_ADR((char *), ds, si);
                cdrom_volctrl.channel0 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME0, u_char);
                cdrom_volctrl.channel1 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME1, u_char);
                cdrom_volctrl.channel2 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME2, u_char);
                cdrom_volctrl.channel3 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME3, u_char);
                audio_status.volume0 = cdrom_volctrl.channel0;
                audio_status.volume1 = cdrom_volctrl.channel1;
                audio_status.volume2 = cdrom_volctrl.channel2;
                audio_status.volume3 = cdrom_volctrl.channel3;
                audio_status.outchan0 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME0-1, u_char);
                audio_status.outchan1 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME1-1, u_char);
                audio_status.outchan2 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME2-1, u_char);
                audio_status.outchan3 = *CALC_PTR(req_buf, MSCD_CTRL_VOLUME3-1, u_char);
                ioctl (cdrom_fd, CDROMVOLCTRL, &cdrom_volctrl);
                break;
     case 0x10: /* audio disk info */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMREADTOCHDR, &cdrom_tochdr)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCHDR, &cdrom_tochdr)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_DISKINFO_LTN,u_char) = cdrom_tochdr.cdth_trk0;
                *CALC_PTR(req_buf,MSCD_DISKINFO_HTN,u_char) = cdrom_tochdr.cdth_trk1;
                cdrom_tocentry.cdte_track = CDROM_LEADOUT;
                cdrom_tocentry.cdte_format = CDROM_MSF;
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                  error ("Fatal cdrom error(audio disk info); read toc header succeeded but following read entry didn't\n");
                  LO(ax) = 1;
                  break;
                }
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+2,u_char) = cdrom_tocentry.cdte_addr.msf.minute;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+1,u_char) = cdrom_tocentry.cdte_addr.msf.second;
                *CALC_PTR(req_buf,MSCD_DISKINFO_LEADOUT+0,u_char) = cdrom_tocentry.cdte_addr.msf.frame;
                break;                
     case 0x11: /* track info */
                req_buf = SEG_ADR((char *), ds, si);
                cdrom_tocentry.cdte_track = *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKNUM,u_char);
                cdrom_tocentry.cdte_format = CDROM_MSF;
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+3,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+2,u_char) = cdrom_tocentry.cdte_addr.msf.minute;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+1,u_char) = cdrom_tocentry.cdte_addr.msf.second;
                *CALC_PTR(req_buf,MSCD_TRACKINFO_TRACKPOS+0,u_char) = cdrom_tocentry.cdte_addr.msf.frame;                  
                *CALC_PTR(req_buf,MSCD_TRACKINFO_CTRL,u_char) = cdrom_tocentry.cdte_ctrl << 4 | 0x20;
                LO(ax) = 0;
                break;                
     case 0x12: /* volume size */
                cdrom_tocentry.cdte_track = CDROM_LEADOUT;
                cdrom_tocentry.cdte_format = CDROM_MSF;
                if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMREADTOCENTRY, &cdrom_tocentry)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_GETVOLUMESIZE_SIZE,int) = cdrom_tocentry.cdte_addr.msf.minute*60*75
                                                                    +cdrom_tocentry.cdte_addr.msf.second*60
                                                                    +cdrom_tocentry.cdte_addr.msf.frame;
                LO(ax) = 0;
                break;                
     case 0x13: /* q channel */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;
            
                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_QCHAN_CTRL,u_char) = (cdrom_subchnl.cdsc_adr << 4) + (cdrom_subchnl.cdsc_ctrl);
                *CALC_PTR(req_buf,MSCD_QCHAN_TNO,u_char)  = cdrom_subchnl.cdsc_trk;
                *CALC_PTR(req_buf,MSCD_QCHAN_IND,u_char)  = cdrom_subchnl.cdsc_ind;
                *CALC_PTR(req_buf,MSCD_QCHAN_MIN,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.minute;
                *CALC_PTR(req_buf,MSCD_QCHAN_SEC,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.second;
                *CALC_PTR(req_buf,MSCD_QCHAN_FRM,u_char)  = cdrom_subchnl.cdsc_reladdr.msf.frame;
                *CALC_PTR(req_buf,MSCD_QCHAN_ZERO,u_char) = 0;
                *CALC_PTR(req_buf,MSCD_QCHAN_AMIN,u_char) = cdrom_subchnl.cdsc_absaddr.msf.minute;
                *CALC_PTR(req_buf,MSCD_QCHAN_ASEC,u_char) = cdrom_subchnl.cdsc_absaddr.msf.second;
                *CALC_PTR(req_buf,MSCD_QCHAN_AFRM,u_char) = cdrom_subchnl.cdsc_absaddr.msf.frame;
                break;
     case 0x14: /* audio status */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_AUDSTAT_PAUSED,u_short)= audio_status.paused_bit;
                *CALC_PTR(req_buf,MSCD_AUDSTAT_START ,u_long) = audio_status.last_StartSector;
                *CALC_PTR(req_buf,MSCD_AUDSTAT_END   ,u_long) = audio_status.last_EndSector; 
                break;
     case 0x15: /* get audio channel information */
                LWORD(eax) = 0;
                if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                  audio_status.media_changed = 1;
                  if (ioctl (cdrom_fd, CDROMSUBCHNL, &cdrom_subchnl)) {
                    /* no disk in drive */
                    LO(ax) = 1;
                    break;
                  }
                }
                if (cdrom_subchnl.cdsc_audiostatus == CDROM_AUDIO_PLAY)
                  HI(ax) = 1;

                req_buf = SEG_ADR((char *), ds, si);
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME0,u_char) = audio_status.volume0;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME1,u_char) = audio_status.volume1;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME2,u_char) = audio_status.volume2; 
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME3,u_char) = audio_status.volume3; 
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME0-1,u_char) = audio_status.outchan0;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME1-1,u_char) = audio_status.outchan1;
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME2-1,u_char) = audio_status.outchan2; 
                *CALC_PTR(req_buf,MSCD_AUDCHAN_VOLUME3-1,u_char) = audio_status.outchan3; 
                break;
     default: error ("CDROM: unknown request !\n");
   }

                /* if (ioctlin) {
                  error ("            return  : ");
                  req_buf = SEG_ADR((char *), ds, si);
                  for (n = 0; n <= 9; ++n)
                     error ("  %3x", req_buf[n]);
                  error ("\n\n");
                 }
                else ("\n");

                error ("Leave cdrom request with return status %d !\n\n", LWORD(eax));
                */
   return ;
}



static void
 ems_helper(void) {
  u_char *rhptr;		/* request header pointer */

  switch (LWORD(ebx)) {
  case 0:
    E_printf("EMS Init called!\n");
    break;
  case 3:
    E_printf("EMS IOCTL called!\n");
    break;
  case 4:
    E_printf("EMS READ called!\n");
    break;
  case 8:
    E_printf("EMS WRITE called!\n");
    break;
  case 10:
    E_printf("EMS Output Status called!\n");
    break;
  case 12:
    E_printf("EMS IOCTL-WRITE called!\n");
    break;
  case 13:
    E_printf("EMS OPENDEV called!\n");
    break;
  case 14:
    E_printf("EMS CLOSEDEV called!\n");
    break;
  case 0x20:
    E_printf("EMS INT 0x67 called!\n");
    break;
  default:
    error("UNKNOWN EMS HELPER FUNCTION %d\n", LWORD(ebx));
  }
  rhptr = SEG_ADR((u_char *), es, di);
  E_printf("EMS RHDR: len %d, command %d\n", *rhptr, *(u_short *) (rhptr + 2));
}

/* unix command is in a null terminate buffer pointed to by ES:DX.
 */
static void run_unix_command(const char *buffer)
{
      system(buffer);
}

/* returns 1 if dos_helper() handles it, 0 otherwise */
static int
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
  case 0x40:{
      E_printf("CDROM: in 0x40 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      cdrom_helper();   
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
    show_regs(__FILE__, __LINE__);
    break;
  case 1:			/* SHOW_REGS */
    show_regs(__FILE__, __LINE__);
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
      set_vc_screen_page(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
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
      show_regs(__FILE__, __LINE__);
      card_init = 1;
    }

  case 5:			/* show banner */
    p_dos_str("\n\nLinux DOS emulator " VERSTR "pl" PATCHSTR " $Date: 1994/12/06 03:33:50 $\n");
    p_dos_str("Last configured at %s\n", CONFIG_TIME);
    p_dos_str("on %s\n", CONFIG_HOST);
    /* p_dos_str("Formerly maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n"); */
    p_dos_str("Bugs, Patches & New Code to James MacLean, jmaclean@fox.nstn.ns.ca\n\n");
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
    g_printf("DBG: Get flags\n");
    break;
  case 0x11:
    g_printf("DBG: Set flags\n");
    LWORD(eax) = SetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) + (_regs.edi & 0xffff)));
    g_printf("DBG: Flags set\n");
    break;

  case 0x16:
    /* polling keyboard - needed by INT16 bios inline rotine */
#if 1
    if (config.hogthreshold && KBD_Head == KBD_Tail) {
      static struct timeval tp1;
      static struct timeval tp2;
      static int time_count = 0;

      if (time_count == 0){
	gettimeofday(&tp1, NULL);
        time_count++;
      }
      else {
        time_count++;
	gettimeofday(&tp2, NULL);
	if ((tp2.tv_sec - tp1.tv_sec) * 1000000 +
	    ((int) tp2.tv_usec - (int) tp1.tv_usec) > config.hogthreshold) {
	  usleep(100);
/*
 * Should be safe here
 */
	  handle_signals();
	  time_count = 0;
	}
      }
    }
#endif
    REG(eflags) |= VIF | IF; /* sti with return to dosemu code */
    break;

  case 0x30:			/* set/reset use bootdisk flag */
    use_bootdisk = LO(bx) ? 1 : 0;
    break;

  case 0x33:			/* set mouse vector */
    SETIVEC(0x33, Mouse_SEG, Mouse_ROUTINE_OFF);
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
    break;

  /*  run the unix command in ds:dx (a null terminated buffer) */
  case 0xfe:
     g_printf("Running Unix Command\n");
     run_unix_command(SEG_ADR((char *), es, dx));
     break;   
  case 0xff:
    if (LWORD(eax) == 0xffff) {
      /* terminate code is in bx */
      dbug_printf("DOS termination requested\n");
      p_dos_str("\n\rLeaving DOS...\n\r");
      leavedos(LO(bx));
    }
    break;

  default:
    error("ERROR: bad dos helper function: AX=0x%04x\n", LWORD(eax));
    return 0;
  }

  return 1;
}

static inline uid_t
 be(uid_t who) {
  if (getuid() != who)
    return setreuid(geteuid(), getuid());
  else
    return 0;
}

static inline uid_t
 be_me(void) {
  if (geteuid() == 0) {
    return setreuid(geteuid(), getuid());
    return 0;
  }
  else
    return geteuid();
}

static inline uid_t
 be_root(void) {
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

/*
 * DANG_BEGIN_FUNCTION version_init
 * 
 * description:
 *  Find version of OS running and set necessary global parms.
 *
 * DANG_END_FUNCTION
 */
static void version_init(void) {
  struct new_utsname unames;
  uname(&unames);
  fprintf(stderr, "DOSEMU%spl%s is coming up on %s version %s\n", VERSTR, PATCHSTR, unames.sysname, unames.release);  
  if (unames.release[0] > 0 ) {
    if ((unames.release[2] == 1  && unames.release[3] > 1 ) ||
         unames.release[2] > 1 ) {
      use_sigio=FASYNC;
    }
  }
  /* Next Check input */
  if (isatty(STDIN_FILENO)) {
    k_printf("STDIN is tty\n");
    config.kbd_tty = 0;
  } else {
    k_printf("STDIN not a tty\n");
    config.kbd_tty = 1;
  }
}

/*
 * DANG_BEGIN_FUNCTION add_to_io_select
 * 
 * arguments:
 *  fd - File handle to add to select statment.
 *  want_sigio - Specifiy whether you want SIGIO (1) if it's available, or
 * 		 not (0).
 *
 * description:
 *  Add file handle to one of 2 select FDS_SET's depending on 
 *  whether the kernel can handle SIGIO.
 *
 * DANG_END_FUNCTION
 */
void add_to_io_select(int new_fd, u_char want_sigio) {
  if (use_sigio && want_sigio) {
    int flags;
    flags = fcntl(new_fd, F_GETFL);
    fcntl(new_fd, F_SETOWN,  getpid());
    fcntl(new_fd, F_SETFL, flags | use_sigio);
    FD_SET(new_fd, &fds_sigio);
    g_printf("GEN: fd=%d gets SIGIO\n", new_fd);
  } else {
    FD_SET(new_fd, &fds_no_sigio);
    g_printf("GEN: fd=%d does not get SIGIO\n", new_fd);
    not_use_sigio++;
  }
}


static void
int08(u_char i)
{
  run_int(0x1c);
  REG(eflags) |= VIF;
  return;
}

static void
int15(u_char i)
{
  struct timeval wait_time;
  int num;

  if (HI(ax) != 0x4f)
    NOCARRY;
  REG(eflags) |= VIF;

  switch (HI(ax)) {
  case 0x10:			/* TopView/DESQview */
    switch (LO(ax))
    {
    case 0x00:			/* giveup timeslice */
      usleep(INT15_IDLE_USECS);
      return;
    }
    CARRY;
    break;
  case 0x41:			/* wait on external event */
    break;
  case 0x4f:			/* Keyboard intercept */
    HI(ax) = 0x86;
    k_printf("INT15 0x4f CARRY=%x AX=%x\n", (LWORD(eflags) & CF),LWORD(eax));
/*
    CARRY;
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
    LWORD(eax) &= 0x00FF;
    return;
  case 0x81:
    LWORD(eax) &= 0x00FF;
    return;
  case 0x82:
    LWORD(eax) &= 0x00FF;
    return;
  case 0x83:
    h_printf("int 15h event wait:\n");
    show_regs(__FILE__, __LINE__);
    CARRY;
    return;			/* no event wait */
  case 0x84:
    CARRY;
    return;			/* no joystick */
  case 0x85:
    num = LWORD(eax) & 0xFF;	/* default bios handler for sysreq key */
    if (num == 0 || num == 1) {
      LWORD(eax) &= 0x00FF;
      return;
    }
    LWORD(eax) &= 0xFF00;
    LWORD(eax) |= 1;
    CARRY;
    return;
  case 0x86:
    /* wait...cx:dx=time in usecs */
    g_printf("doing int15 wait...ah=0x86\n");
    show_regs(__FILE__, __LINE__);
    wait_time.tv_sec = 0;
    wait_time.tv_usec = (LWORD(ecx) << 16) | LWORD(edx);
    RPT_SYSCALL(select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv));
    NOCARRY;
    return;

  case 0x87:
    if (config.xms_size)
      xms_int15();
    else {
      LWORD(eax) &= 0xFF;
      LWORD(eax) |= 0x0300;	/* say A20 gate failed - a lie but enough */
      CARRY;
    }
    return;

  case 0x88:
    if (config.xms_size) {
      xms_int15();
    }
    else {
      LWORD(eax) &= ~0xffff;	/* we don't have extended ram if it's not XMS */
      NOCARRY;
    }
    return;

  case 0x89:			/* enter protected mode : kind of tricky! */
    LWORD(eax) |= 0xFF00;		/* failed */
    CARRY;
    return;
  case 0x90:			/* no device post/wait stuff */
    CARRY;
    return;
  case 0x91:
    CARRY;
    return;
  case 0xc0:
    LWORD(es) = ROM_CONFIG_SEG;
    LWORD(ebx) = ROM_CONFIG_OFF;
    LO(ax) = 0;
    return;
  case 0xc1:
    CARRY;
    return;			/* no ebios area */
  case 0xc2:
        m_printf("PS2MOUSE: Call ax=0x%04x\n", LWORD(eax));
	if (!mice->intdrv) {
          if (mice->type != MOUSE_PS2) {
                REG(eax) = 0500;        /* No ps2 mouse device handler */
                CARRY;
                return;
          }
	  else return;
	}
                
        switch (REG(eax) &= 0x00FF)
        {
                case 0x0000:                    
                        if (LO(bx)) mouse.cursor_on = 1;
                                else mouse.cursor_on = 0;
                        HI(ax) = 0;             
			NOCARRY;
                        break;
                case 0x0001:
                        HI(ax) = 0;
                        LWORD(ebx) = 0xAAAA;    /* we have a ps2 mouse */
			NOCARRY;
                        break;
		case 0x0002:			/* set sampling rate */
			HI(ax) = 0;		/* invalid function  but who cares */
			NOCARRY;
			break;
		case 0x0003:
			switch (LO(bx))
			{
			case 0x00:	
				mouse.ratio = 1;
				break;
			case 0x01:
				mouse.ratio = 2;
				break;
			case 0x02:
				mouse.ratio = 4;
				break;
			case 0x03:
				mouse.ratio = 8;
			} 
			HI(ax) = 0;
			NOCARRY;
			break;
		case 0x0004:
			HI(bx) = 0xAA;
			HI(ax) = 0;
			NOCARRY;
			break;
		case 0x0005:
			HI(ax) = 0;
			NOCARRY;
			break;
                default:
                        g_printf("PS2MOUSE: Unknown call ax=0x%04x\n", LWORD(eax));
                        CARRY;
        }
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
static int
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
    REG(eax) = 0;
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
    D_printf("DPMI 1686 returns %x\n", (int)REG(eax));
    return 1;

  }
#endif

  return 0;
}

static  void
int1a(u_char i)
{
  unsigned int test_date;
  time_t akt_time; 
  time_t time_val;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm, *tm2;

  switch (HI(ax)) {

    /* A timer read should reset the overflow flag */
  case 0:			/* read time counter */
    time(&akt_time);
    tm = localtime((time_t *) &akt_time);
    test_date = tm->tm_year * 10000 + tm->tm_mon * 100 + tm->tm_mday;
    if ( check_date != test_date ) {
      start_time = akt_time;
      check_date = tm->tm_year * 10000 + tm->tm_mon * 100 + tm->tm_mday;
      g_printf("Over 24hrs forward or backward\n");
      *(u_char *) (TICK_OVERFLOW_ADDR) += 0x1;
    }
    last_ticks = (tm->tm_hour * 60 * 60 + tm->tm_min * 60 + tm->tm_sec) * 18.206;
    LO(ax) = *(u_char *) (TICK_OVERFLOW_ADDR);
    LWORD(ecx) = (last_ticks >> 16) & 0xffff;
    LWORD(edx) = last_ticks & 0xffff;
#if 0
    g_printf("read timer st:%u ticks:%u act:%u, actdate:%d\n",
				    start_time, last_ticks, akt_time, tm->tm_mday);
#endif
    set_ticks(last_ticks);
    break;
  case 1:			/* write time counter */
    last_ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);
    set_ticks(last_ticks);
    time(&start_time);
    g_printf("set timer to %lu \n", last_ticks);
    break;
  case 2:			/* get time */
    time(&time_val);
    tm = localtime((time_t *) &time_val);
    g_printf("get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
#if 0
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
#endif
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
    time(&time_val);
    tm = localtime((time_t *) &time_val);
    tm->tm_year += 1900;
    tm->tm_mon++;
    g_printf("get date %02d.%02d.%04d\n", tm->tm_mday, tm->tm_mon, tm->tm_year);
#if 0
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
#endif
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
    /* show_regs(__FILE__, __LINE__); */
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
static int
ms_dos(int nr)
{				/* returns 1 if emulated, 0 if internal handling */

  /* int 21, ah=1,7,or 8:  return 0 for an extended keystroke, but the
   next call returns the scancode */
  /* if I press and hold a key like page down, it causes the emulator to
   exit! */

Restart:
  /* dbug_printf("DOSINT 0x%02x\n", nr); */
  /* emulate keyboard io to avoid DOS' busy wait */

  switch (nr) {

  case 12:			/* clear key buffer, do int AL */
    keybuf_clear();
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

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
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
	show_regs(__FILE__, __LINE__);
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
      show_regs(__FILE__, __LINE__);
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
	show_regs(__FILE__, __LINE__);
	return (TRUE);
	break;
      case IOCTL_CHECK_OUTPUT_STS:
	E_printf("EMS: dos_ioctl chkoutsts emm.\n");
	LO(ax) = 0xff;
	NOCARRY;
	show_regs(__FILE__, __LINE__);
	return (TRUE);
	break;
      }
      error("ERROR: dos_ioctl shouldn't get here. XXX\n");
      return (FALSE);
    }
#endif

  default:
#if 0
    /* taking this if/printf out will speed things up a bit...*/
    if (d.dos)
      dbug_printf(" dos interrupt 0x%02x, ax=0x%04x, bx=0x%04x\n",
		  nr, LWORD(eax), LWORD(ebx));
#endif
    return 0;
  }
  return 1;
}

void
run_int(int i)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  pushw(ssp, sp, LWORD(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 6;
  LWORD(cs) = ((us *) 0)[(i << 1) + 1];
  LWORD(eip) = ((us *) 0)[i << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
  REG(eflags) &= ~(VIF | TF | IF | NT);
}

int
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

  if (i < 0x21 || i > 0xe8 ) return 1;

  switch (i) {

    /* some things, like 0x29, need to be unrevectored if the vectors
       * are the DOS vectors...but revectored otherwise
       */
  case 0x21:			/* we want it first...then we'll pass it on */
  case 0x2f:			/* needed for XMS, redirector, and idle interrupt */
    return 0;
  case 0x74:			/* needed for PS/2 Mouse */
    if ((mice->type == MOUSE_PS2) || (mice->intdrv)) {
      return 0;
    }
    else return 1;
  case 0xe6:			/* for redirector and helper (was 0xfe) */
  case 0xe7:			/* for mfs FCB helper */
  case 0xe8:			/* for int_queue_run return */
    return 0;

  case 0x28:
   if (!config.keybint && config.console_keyb)
      return 0;
    else
      return 1;
    break;

  case 0x33:			/* mouse...we're testing */
    if (mice->intdrv)
      return 0;
    else
      return 1;
    break;
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
  case 0x2a:
  case 0x60:
  case 0x61:
  case 0x62:
  case 0x67:			/* EMS */
  case 0xfe:			/* Turbo Debugger and others... */
    return 1;
  default:
    g_printf("revectoring 0x%02x\n", i);
    return 1;
  }
}

/* shared, needs to be in header file */
static inline int is_revectored(int nr, struct revectored_struct * bitmap)
{
	__asm__ __volatile__("btl %2,%%fs:%1\n\tsbbl %0,%0"
		:"=r" (nr)
		:"m" (*bitmap),"r" (nr));
	return nr;
}

/*
 * DANG_BEGIN_FUNCTION DO_INT 
 *
 * description:
 * DO_INT is used to deal with interrupts returned to DOSEMU by the
 * kernel.
 *
 * DANG_END_FUNCTION
 */

void
do_int(int i)
{

  void (* caller_function)();

#if 0 /* 94/07/02 Just for reference */
  saytime("do_int");
#endif
#if 0 /* 94/07/02 Just for reference */
  k_printf("Do INT0x%02x, eax=0x%04x, ebx=0x%04x ss=0x%04x esp=0x%04x cs=0x%04x ip=0x%04x CF=%d \n", i, LWORD(eax), LWORD(ebx), LWORD(ss), LWORD(esp), LWORD(cs), LWORD(eip), (int)REG(eflags) & CF);
#endif

  if ((LWORD(cs)) != BIOSSEG && IS_REDIRECTED(i) && can_revector(i)){
    run_int(i);
    return;
  }

  caller_function = interrupt_function[i];
  caller_function(i);
  
}

static void int05(u_char i) {
    g_printf("BOUNDS exception\n");
    default_interrupt(i);
    return;
}

void int_a_b_c_d_e_f(u_char i) {
    g_printf("IRQ->interrupt %x\n", i);
    show_regs(__FILE__, __LINE__);
    default_interrupt(i);
    return;
}

/* IRQ1, keyb data ready */
static void int09(u_char i) {
    fprintf(stderr, "IRQ->interrupt %x\n", i);
    run_int(0x09);
    return;
}

/* CONFIGURATION */
static void int11(u_char i) {
    LWORD(eax) = configuration;
    return;
}

/* MEMORY */
static void int12(u_char i) {
    LWORD(eax) = config.mem_size;
    return;
}

/* KEYBOARD */
static void int16(u_char i) {
    run_int(0x16);
    return;
}

/* BASIC */
static void inline int18(u_char i) {
    k_printf("BASIC interrupt being attempted.\n");
    return;
}

/* LOAD SYSTEM */
static void inline int19(u_char i) {
    boot();
    return;
}

/* MS-DOS */
static void inline int21(u_char i) {
  if (ms_dos(HI(ax)))
    return;
  default_interrupt(i);
  return;
}

/* KEYBOARD BUSY LOOP */
static void inline int28caller(u_char i) {
  static int first = 1;
  if (first && !config.keybint && config.console_keyb) {
    first = 0;
    /* revector int9, so dos doesn't play with the keybuffer */
    k_printf("revectoring int9 away from dos\n");
    SETIVEC(0x9, BIOSSEG, 16 * 0x8 + 2);	/* point to IRET */
  }
  if (int28())
    return;
  default_interrupt(i);
  return;
}

/* FAST CONSOLE OUTPUT */
static void inline int29(u_char i) {
    /* char in AL */
    char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
    return;
}

/* Multiplex */
static void inline int2fcaller(u_char i) {
    if (int2f())
      return;
    default_interrupt(i);
    return;
}

/* mouse */
static void inline int33(u_char i) {
  if (mice->intdrv) {
    mouse_int();
    return;
  }
  else {
    k_printf("Why is this being called?\n");
    default_interrupt(i);
  }
}

/* new packet driver interface */
static void inline int_pktdrvr(u_char i) {
    if (pkt_int())
      return;
    default_interrupt(i);
    return;
}

/* dos helper and mfs startup (was 0xfe) */
static void inline inte6(u_char i) {
    if (dos_helper())
      return;
    default_interrupt(i);
    return;
}

/* mfs FCB call */
static void inline inte7(u_char i) {
    SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
    run_int(0xe7);
    return;
}

/* End function for interrupt calls from int_queue_run() */
static void inline inte8(u_char i) {
      static unsigned short *csp;
      static int x;
      csp = SEG_ADR((us *), cs, ip) - 1;
      for (x=1; x<=int_queue_running; x++) {
        /* check if any int is finished */
        if ((int)csp == int_queue_head[x].int_queue_return_addr) {
          /* if it's finished - clean up */
          /* call user cleanup function */
          if (int_queue_head[x].int_queue_ptr.callend) {
    	    int_queue_head[x].int_queue_ptr.callend(int_queue_head[x].int_queue_ptr.interrupt);
            int_queue_head[x].int_queue_ptr.callend = NULL;
	  }

          /* restore registers */
          REGS = int_queue_head[x].saved_regs;
          REG(eflags) |= VIF;

          h_printf("e8 int_queue: finished %x\n", int_queue_head[x].int_queue_return_addr);
	  *OUTB_ADD=1;
	  if (int_queue_running == x ) 
            int_queue_running--;
	  return;
        }
      }
    h_printf("e8 int_queue: shouldn't get here\n");
    show_regs(__FILE__,__LINE__);
    return;
}

/*
 * DANG_BEGIN_FUNCTION DEFAULT_INTERRUPT 
 *
 * description:
 * DEFAULT_INTERRUPT is the default interrupt service routine 
 * called when DOSEMU initializes.
 *
 * DANG_END_FUNCTION
 */

static void default_interrupt(u_char i) {
    if (d.defint)
      dbug_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));

    if (!IS_REDIRECTED(i) || (LWORD(cs) == BIOSSEG && LWORD(eip) == (i * 16 + 2))) {
      g_printf("DEFIVEC: int 0x%02x  SG: 0x%04x  OF: 0x%04x\n",
	       i, ISEG(i), IOFF(i));

/*    This is here for old SIGILL's that modify IP */
      if (i == 0x00) {
	LWORD(eip)+=2;
      }

      return;
    }

    if (IS_IRET(i)) {
      if ((i != 0x2a) && (i != 0x28))
	g_printf("just an iret 0x%02x\n", i);
      return;
    }

    run_int(i);

    return;
}

/*
 * DANG_BEGIN_FUNCTION SETUP_INTERRUPTS 
 *
 * description:
 * SETUP_INTERRUPTS is used to initialize those interrupt calls that
 * we are specifically handling in protected mode.
 *
 * DANG_END_FUNCTION
 */

static void setup_interrupts(void) {
  
  interrupt_function[5] = int05;
  interrupt_function[8] = int08;
  interrupt_function[9] = int09;
  interrupt_function[0xa] = int_a_b_c_d_e_f;
  interrupt_function[0xb] = int_a_b_c_d_e_f;
  interrupt_function[0xc] = int_a_b_c_d_e_f;
  interrupt_function[0xd] = int_a_b_c_d_e_f;
  interrupt_function[0xe] = int_a_b_c_d_e_f;
  interrupt_function[0xf] = int_a_b_c_d_e_f;
  interrupt_function[0x10] = int10;
  interrupt_function[0x11] = int11;
  interrupt_function[0x12] = int12;
  interrupt_function[0x13] = int13;
  interrupt_function[0x14] = int14;
  interrupt_function[0x15] = int15;
  interrupt_function[0x16] = int16;
  interrupt_function[0x17] = int17;
  interrupt_function[0x18] = int18;
  interrupt_function[0x19] = int19;
  interrupt_function[0x1a] = int1a;
  interrupt_function[0x21] = int21;
  interrupt_function[0x28] = int28caller;
  interrupt_function[0x29] = int29;
  interrupt_function[0x2f] = int2fcaller;
  interrupt_function[0x33] = int33;
  interrupt_function[0x60] = int_pktdrvr;
  interrupt_function[0xe6] = inte6;
  interrupt_function[0xe7] = inte7;
  interrupt_function[0xe8] = inte8;
}
