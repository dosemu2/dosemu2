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
 * $Date: 1995/02/05 16:51:10 $
 * $Source: /home/src/dosemu0.60/RCS/emu.c,v $
 * $Revision: 2.34 $
 * $State: Exp $
 *
 * $Log: emu.c,v $
 * Revision 2.34  1995/02/05  16:51:10  root
 * Prep for Scotts patches.
 *
 * Revision 2.33  1995/01/19  02:20:11  root
 * Testing for Marty.
 *
 * Revision 2.32  1995/01/14  15:28:03  root
 * New Year checkin.
 *
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

#ifndef EMU_C
#define EMU_C
#endif

/*
 * DANG_BEGIN_REMARK
   DOSEMU must not work within the 1 meg DOS limit, so start of code
   is loaded at a higher address, at some time this could conflict with
   other shared libs. If DOSEMU is compiled statically (without shared
   libs), and org instruction is used to provide the jump above 1 meg.
 * DANG_END_REMARK
*/

#ifndef __ELF__
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
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <assert.h>

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
#include "hgc.h"
#include "timers.h"
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"
#include "keymaps.h"
#include "cpu.h"
#include "int.h"
#ifdef NEW_PIC
#include "timer/bitops.h"
#include "timer/pic.h"
#endif
#ifdef DPMI
#include "dpmi/dpmi.h"
#endif

extern void stdio_init(void);
extern void time_setting_init(void);
extern void tmpdir_init(void);
extern void low_mem_init(void);

extern void shared_memory_exit(void);
extern void restore_vt (unsigned short);
extern void disallocate_vt (void);
extern void keyboard_close(void);
extern void vm86_GP_fault();
extern void config_init(int argc, char **argv);

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
static int special_nowait = 0;



static int poll_io = 1;		/* polling io, default on */


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

void boot(void)
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

/* Silly Interrupt Generator Initialization/Closedown */

#ifdef SIG
SillyG_t *SillyG = 0;
static SillyG_t SillyG_[16+1];
#endif

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
static inline void SIG_init()
{
#ifdef SIG
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
#endif
}

static inline void SIG_close()
{
#ifdef SIG
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
#endif
}

static inline void emumodule_init(void)
{
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
}

static inline void
module_init(void)
{
  version_init();              /* Check the OS version */
  emumodule_init();            /* emumodule support */
  SIG_init();                  /* silly int generator support */
  memcheck_init();             /* lower 1M memory map support */
  tmpdir_init();               /* create our temporary dir */
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
#ifdef __ELF__
void main (int argc, char **argv)
#else
void emulate(int argc, char **argv)
#endif
{
  FD_ZERO(&fds_sigio);         /* initialize both fd_sets to 0 */
  FD_ZERO(&fds_no_sigio);
  vm86s.flags = 0;

  stdio_init();                /* initialize stdio & stderr */
  config_init(argc, argv);     /* parse the commands & config file(s) */
  module_init();
  low_mem_init();              /* initialize the lower 1Meg */
  time_setting_init();         /* get the startup time */
  signal_init();               /* initialize sig's & sig handlers */
  device_init();               /* initialize keyboard, disk, video, etc. */
  cpu_setup();                 /* setup the CPU */
  hardware_setup();            /* setup any hardware */
  memory_init();               /* initialize the memory contents */
  boot();                      /* read the boot sector & get moving */
  timer_interrupt_init();      /* start sending int 8h int signals */

  if (not_use_sigio)
    k_printf("Atleast 1 NON-SIGIO file handle in use.\n");
  else
    k_printf("No NON-SIGIO file handles in use.\n");
  g_printf("EMULATE\n");

  fflush(stdout);

  while(!fatalerr) {
    run_vm86();
#ifdef NEW_PIC
    run_irqs();      /*  trigger any hardware interrupts requested */
#endif
    serial_run();
#if 1
#ifdef USING_NET
    /* check for available packets on the packet driver interface */
    /* (timeout=0, so it immediately returns when none are available) */
    pkt_check_receive(0);
#endif
#endif
    int_queue_run();
  }

  error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	fatalerr, fatalerr, in_sigsegv, ignore_segv);

  sync();
  fprintf(stderr, "ARG!!!!!\n");
  leavedos(99);
}

void
dos_ctrl_alt_del(void)
{
  dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
  disk_close();
  clear_screen(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), 7);
  special_nowait = 0;
  p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
  disk_init();
  memory_init();
  cpu_setup();
  hardware_setup();
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

static void
ign_sigs(int sig) {
  static int timerints = 0;
  static int otherints = 0;

  g_printf("ERROR: signal %d received in leavedos()\n", sig);
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
#endif
  SIG_close();

  show_ints(0, 0x33);
  g_printf("calling disk_close_all\n");
  disk_close_all();
  g_printf("calling video_close\n");
  video_close();
  g_printf("calling keyboard_close\n");
  fflush(stderr);
  fflush(stdout);
  keyboard_close();

  g_printf("calling shared memory exit\n");
  shared_memory_exit();
   if (config.detach) {
     restore_vt(config.detach);
     disallocate_vt ();
   }
  exit(sig);
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
  if ( use_sigio && want_sigio ) {
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

/*
 * DANG_BEGIN_FUNCTION remove_from_io_select
 * 
 * arguments:
 *  fd - File handle to remove from select statment.
 *  used_sigio - Specifiy whether you used SIGIO (1) if it's available, or
 * 		 not (0).
 *
 * description:
 *  Remove a file handle from one of 2 select FDS_SET's depending on 
 *  whether the kernel can handle SIGIO.
 *
 * DANG_END_FUNCTION
 */
void remove_from_io_select(int new_fd, u_char used_sigio) {
  if ( use_sigio && used_sigio ) {
    int flags;
    flags = fcntl(new_fd, F_GETFL);
    fcntl(new_fd, F_SETOWN,  NULL);
    fcntl(new_fd, F_SETFL, flags & ~(use_sigio));
    FD_CLR(new_fd, &fds_sigio);
    g_printf("GEN: fd=%d removed from select SIGIO\n", new_fd);
  } else {
    FD_CLR(new_fd, &fds_no_sigio);
    g_printf("GEN: fd=%d removed from select\n", new_fd);
    not_use_sigio++;
  }
}
