/* first baby steps for removing SYSV IPC from dosemu 0.48p1+
 *
 * Robert Sanders, started 3/1/93
 *
 * $Date: 1994/03/15 02:08:20 $
 * $Source: /home/src/dosemu0.50pl1/RCS/dosio.c,v $
 * $Revision: 1.7 $
 * $State: Exp $
 *
 * $Log: dosio.c,v $
 * Revision 1.7  1994/03/15  02:08:20  root
 * Testing
 *
 * Revision 1.6  1994/03/15  01:38:20  root
 * DPMI,serial, other changes.
 *
 * Revision 1.5  1994/03/14  00:35:44  root
 * Unsucessful speed enhancements
 *
 * Revision 1.4  1994/03/13  21:52:02  root
 * More speed testing :-(
 *
 * Revision 1.3  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.2  1994/03/10  23:52:52  root
 * Lutz DPMI patches
 *
 * Revision 1.1  1994/03/10  02:49:27  root
 * Initial revision
 *
 * Revision 1.26  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.25  1994/03/04  00:01:58  root
 * Readying for 0.50
 *
 * Revision 1.24  1994/02/21  20:28:19  root
 * Ronnie's serial patches
 *
 * Revision 1.23  1994/02/20  10:00:16  root
 * More keyboard work :-(.
 *
 * Revision 1.22  1994/02/05  21:45:55  root
 * Arranged keyboard to handle setting bios prior to calling int15 4f.
 *
 * Revision 1.21  1994/02/02  21:12:56  root
 * Bringing the pktdrvr up to speed.
 *
 * Revision 1.20  1994/02/01  20:57:31  root
 * With unlimited thanks to gorden@jegnixa.hsc.missouri.edu (Jason Gorden),
 * here's a packet driver to compliment Tim_R_Bird@Novell.COM's IPX work.[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[his packet driver  to compliment Tim_R_Bird@Novell.COM's IPX work.
 *
 * Revision 1.19  1994/02/01  19:25:49  root
 * Fix to allow multiple graphics DOS sessions with my Trident card.
 *
 * Revision 1.18  1994/01/31  22:40:18  root
 * Mouse
 *
 * Revision 1.17  1994/01/31  21:09:02  root
 * Mouse working with X.
 *
 * Revision 1.16  1994/01/31  18:44:24  root
 * Work on making mouse work
 *
 * Revision 1.15  1994/01/30  12:30:23  root
 * Attempting to get mouse to turn of when switching to another console.
 *
 * Revision 1.14  1994/01/27  19:43:54  root
 * Preparing for IPX of Tim's.
 *
 * Revision 1.13  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout.
 *
 * Revision 1.12  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.11  1994/01/19  17:51:14  root
 * More Mickey Mousing around with keyboard to get it emulating proper keyboard
 * behavior. Better, but still not correct!
 *
 * Revision 1.10  1994/01/01  17:06:19  root
 * More debug for keyboard
 *
 * Revision 1.9  1993/12/22  11:45:36  root
 * Keyboard enhancements, going to REAL int9
 *
 * Revision 1.8  1993/12/05  20:59:03  root
 * Keyboard Work.
 *
 * Revision 1.7  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.6  1993/11/29  22:44:11  root
 * More work on keyboards
 * ,
 *
 * Revision 1.5  1993/11/29  00:05:32  root
 * Overhaul Keyboard.
 *
 * Revision 1.4  1993/11/25  22:45:21  root
 * About to destroy keybaord routines.
 *
 * Revision 1.3  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
 * Revision 1.2  1993/11/17  22:29:33  root
 * Keyboard char behind patch
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.4  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.3  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.2  1993/03/04  22:35:12  root
 * put in perfect shared memory, HMA and everything.  added PROPER_STI.
 *
 * Revision 1.1  1993/03/02  03:06:42  root
 * Initial revision
 *
 */
#define DOS_IO

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "termio.h"
#include "dosio.h"

/* #define SIG 1 */

extern void bios_emm_init(void);
extern void xms_init(void);
extern void dump_kbuffer(void);
extern u_char keepkey;
extern int_count[];
extern struct config_info config;
extern int in_readkeyboard, keybint;
extern int ignore_segv;
extern struct CPU cpu;
extern int pd_sock;

#define PAGE_SIZE	4096

/* when first started, dosemu should do a start_dosipc().
 * before leaving for good, dosemu should do a stop_dosipc().
 *
 */

#ifndef SA_RESTART
#define SA_RESTART 0
#endif

/* For the packet driver */
us *cur_pd_size = (us *) 0xCFFFE;
char *cur_pd_buf;
int cpd_sock = 0;

u_long secno = 0;

inline void dokey(int);
inline void process_interrupt(int);
inline void child_packet(int);

/* my test shared memory IDs */
struct {

  int 			/* normal mem if idt not attached at 0x100000 */
   hmastate;		/* 1 if HMA mapped in, 0 if idt mapped in */
}
sharedmem;

u_char *HMAmemory;

void HMA_MAP(int HMA)
{
k_printf("Entering HMA_MAP with HMA=%d\n", HMA);
  if (HMA){
    memmove((u_char *)0x100000,(u_char *)HMAmemory , (64*1024 - 16) );
  }
  else {
    memmove((u_char *)HMAmemory, (u_char *)0x100000, (64*1024 - 16) );
    memmove((u_char *)0x100000,(u_char *)0x0 , (64*1024 - 16) );
  }
}

void
set_a20(int enableHMA)
{
  if (sharedmem.hmastate == enableHMA)
    error("ERROR: redundant %s of A20!\n", enableHMA ? "enabling" :
	  "disabling");

  /* to turn the A20 on, one must unmap the "wrapped" low page, and
   * map in the real HMA memory. to turn it off, one must unmap the HMA
   * and make FFFF:xxxx addresses wrap to the low page.
   */
  HMA_MAP(enableHMA);

  sharedmem.hmastate = enableHMA;
}

/* this is for the parent process...currently NOT compatible with
 * the -b map_bios flags!
 *
 * this is very kludgy.  don't even ask.  you don't want to know
 */

void
memory_setup(void)
{
  unsigned char *ptr;

  /* dirty all pages */
  ignore_segv++;
  for (ptr = 0; ptr < (unsigned char *) (1024 * 1024); ptr += 4096)
    *ptr = *ptr;
  ignore_segv--;

  /* initially, no HMA */
  sharedmem.hmastate = 0;
  HMAmemory=(u_char *)malloc(64*1024);

#if 1
  /* zero the DOS address space... is this really necessary? */
  memset(NULL, 0, 640 * 1024);
#endif

  /* for EMS */
  bios_emm_init();
  xms_init();
}

#define SCANQ_LEN 100
u_short scan_queue[SCANQ_LEN];
int scan_queue_start = 0;
int scan_queue_end = 0;
u_char keys_ready = 0;
u_char scanned = 0;
extern int convKey();
extern int InsKeyboard();
extern u_char move_kbd_key_flags;

fd_set fds;
void
io_select(void)
{
  int selrtn;
  struct timeval tvptr;

  tvptr.tv_sec=tvptr.tv_usec=0L;
  FD_ZERO(&fds);
  FD_SET(kbd_fd, &fds);

#ifdef SIG
  if (SillyG)
    FD_SET(SillyG, &fds);
#endif

  switch ((selrtn = select(10, &fds, NULL, NULL, &tvptr))) {
    case 0:			/* none ready, nothing to do :-) */
      return;
      break;

    case -1:			/* error (not EINTR) */
      error("ERROR: bad io_select: %s\n", strerror(errno));
      break;

    default:			/* has at least 1 descriptor ready */

#ifdef SIG
      if (SillyG)
	if (FD_ISSET(SillyG, &fds)) {
	  process_interrupt(SillyG);
	}
#endif

      if (FD_ISSET(kbd_fd, &fds)) {
	dokey(kbd_fd);
      }

      /* XXX */
#if 0
      fflush(stdout);
#endif


      break;
    }

}

void
DOS_setscan(u_short scan)
{
  k_printf("DOS got set scan %x, startq=%d, endq=%d\n", scan, scan_queue_start, scan_queue_end);
  scan_queue[scan_queue_end] = scan;
  scan_queue_end = (scan_queue_end + 1) % SCANQ_LEN;
  if (config.keybint) {
    k_printf("Hard queue\n");
    do_hard_int(9);
  }
  else {
    k_printf("NOT Hard queue\n");
    scanned = 0;
    move_kbd_key_flags = 1;
    parent_nextscan();
  }

}

static int lastchr;
int inschr;

inline void
set_keyboard_bios(void)
{

  if (config.console_keyb) {
    if (config.keybint) {
      keepkey = 1;
      inschr = convKey(HI(ax));
    }
    else
      inschr = convKey(lastscan);
  }
  else
    inschr = lastchr;
#if 0
  if ((inschr & 0xff) == 0x3 && kbd_flag(KF_CTRL)) {
    _regs.eflags &= ~0x40;
    do_int(0x1b);
  }
#endif
  k_printf("parent nextscan found inschr=0x%02x, lastchr = 0x%02x, lastscan = 0x%04x scaned=%d\n", inschr, lastchr, lastscan, scanned);
  k_printf("MOVING   key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	   *(u_char *)KEYFLAG_ADDR , *(u_char *)(KEYFLAG_ADDR +1), *(u_char *)KBDFLAG_ADDR, *(u_char *)(KBDFLAG_ADDR+1));
}

inline void
insert_into_keybuffer(void)
{
  /* int15 fn=4f will reset CF if scan key is not to be used */
  if (!config.keybint || LWORD(eflags) & CF)
    keepkey = 1;
  else
    keepkey = 0;

  if (inschr && keepkey) {
    k_printf("IPC/KBD: (child) putting key in buffer\n");
    if (InsKeyboard(inschr))
      dump_kbuffer();
    else
      error("ERROR: InsKeyboard could not put key into buffer!\n");
  }
}

inline void
parent_nextscan()
{

  k_printf("ENTERING key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	   *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);
  k_printf("scanned=%d, start=%d, end=%d\n", scanned, scan_queue_start, scan_queue_end);
  keys_ready = 0;
  if (!scanned && (scan_queue_start != scan_queue_end)) {
    scanned = 1;
    keys_ready = 1;
    lastchr = scan_queue[scan_queue_start];
    if (scan_queue_start != scan_queue_end)
      scan_queue_start = (scan_queue_start + 1) % SCANQ_LEN;
    if (!config.console_keyb) {
      lastscan = lastchr >> 8;
    }
    else {
      lastscan = lastchr;
    }
  }
  k_printf("parent nextscan lastscan = 0x%04x scaned=%d\n", lastscan, scanned);
  if (move_kbd_key_flags) {
    set_keyboard_bios();
    insert_into_keybuffer();
    move_kbd_key_flags = 0;
  }
}

inline void
dokey(int fd)
{
  getKeys();
  k_printf("After getKeys() dosipc.c\n");
}

inline void
process_interrupt(int fd)
{
  int rrtn;
  u_int chr;

  if ((rrtn = read(fd, " ", 1))) {

    h_printf("INTERRUPT: 0x%02x, fd=%x\n", rrtn, fd);
    if (rrtn < 8)
      chr = rrtn + 8;
    else
      chr = rrtn + 0x68;
    do_hard_int(chr);
  }
}
#undef DOS_IO
