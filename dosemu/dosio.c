#if 0
#define NCU
#endif
/*
 * Robert Sanders, started 3/1/93
 *
 * $Date: 1995/05/06 16:25:30 $
 * $Source: /usr/src/dosemu0.60/dosemu/RCS/dosio.c,v $
 * $Revision: 2.17 $
 * $State: Exp $
 *
 * $Log: dosio.c,v $
 * Revision 2.17  1995/05/06  16:25:30  root
 * Prep for 0.60.2.
 *
 * Revision 2.16  1995/04/08  22:30:40  root
 * Release dosemu0.60.0
 *
 * Revision 2.15  1995/02/25  21:53:11  root
 * *** empty log message ***
 *
 * Revision 2.15  1995/02/25  21:53:11  root
 * *** empty log message ***
 *
 * Revision 2.14  1995/02/05  16:52:03  root
 * Prep for Scotts patches.
 *
 * Revision 2.13  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.12  1994/11/06  02:35:24  root
 * Testing co -M.
 *
 * Revision 2.11  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.10  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.9  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.8  1994/09/22  23:51:57  root
 * Prep for pre53_21.
 *
 * Revision 2.7  1994/09/20  01:53:26  root
 * Prep for pre53_21.
 *
 * Revision 2.6  1994/09/11  01:01:23  root
 * Prep for pre53_19.
 *
 * Revision 2.5  1994/08/14  02:52:04  root
 * Rain's latest CLEANUP and MOUSE for X additions.
 *
 * Revision 2.4  1994/08/11  01:11:34  root
 * Added check to NOT add release key for ascii's with high byte 0x00.
 *
 * Revision 2.3  1994/08/05  22:29:31  root
 * Prep dir pre53_10.
 *
 * Revision 2.2  1994/07/26  01:12:20  root
 * prep for pre53_6.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.25  1994/05/30  00:08:20  root
 * Prep for pre51_22 and temp kludge fix for dir a: error.
 *
 * Revision 1.24  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.23  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.22  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.21  1994/05/18  00:15:51  root
 * pre15_17.
 *
 * Revision 1.20  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.19  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.18  1994/05/04  22:16:00  root
 * Patches by Alan to mouse subsystem.
 *
 * Revision 1.17  1994/04/30  22:12:30  root
 * Prep for pre51_11.
 *
 * Revision 1.16  1994/04/27  21:34:15  root
 * Jochen's Latest.
 *
 * Revision 1.15  1994/04/20  23:43:35  root
 * pre51_8 out the door.
 *
 * Revision 1.14  1994/04/20  21:05:01  root
 * Prep for Rob's patches to linpkt...
 *
 * Revision 1.13  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.12  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.11  1994/04/13  00:07:09  root
 * Modifications to io_select().
 *
 * Revision 1.10  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 1.9  1994/04/07  00:18:41  root
 * Pack up for pre52_4.
 *
 * Revision 1.8  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
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
 * here's a packet driver to compliment Tim_R_Bird@Novell.COM's IPX work.
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
#define DBGTIME(x) {\
                        struct timeval tv;\
                        gettimeofday(&tv,NULL);\
                        fprintf(stderr,"%c %06d:%06d\n",x,(int)tv.tv_sec,(int)tv.tv_usec);\
                   }

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifndef __NetBSD__
#include <linux/if_ether.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <termios.h>			/* for mouse.h */
#ifdef __NetBSD__
#include <sys/mman.h>
#else
#define MAP_ANON MAP_ANONYMOUS
extern caddr_t mmap __P ((caddr_t __addr, size_t __len,
			  int __prot, int __flags, int __fd, off_t __off));
extern int munmap __P ((caddr_t __addr, size_t __len));
#include <linux/mman.h>
#endif

#include "memory.h"
#include "emu.h"
#include "termio.h"
#include "dosio.h"
#include "mouse.h"
#include "int.h"
#ifdef USE_MHPDBG
  #include "mhpdbg.h"
#endif

#include "pic.h"
#include "bitops.h"

extern void DOSEMUMouseEvents(void);

extern void xms_init(void);
extern void video_memory_setup(void);
extern void dump_kbuffer(void);
extern int_count[];
extern int in_readkeyboard, keybint;
#ifdef SIG
extern SillyG_t *SillyG;
#endif

#define PAGE_SIZE	4096

static u_long secno = 0;

inline void process_interrupt(SillyG_t *sg);

/* my test shared memory IDs */
static struct {
  int 			/* normal mem if idt not attached at 0x100000 */
  hmastate;		/* 1 if HMA mapped in, 0 if idt mapped in */
}
sharedmem;

#define HMASIZE (64*1024 - 16)
#define HMAAREA (u_char *)0x100000
#ifdef __NetBSD__
#define SHM_REMAP 0			/* XXX? */
#endif

/* Used for all IPC HMA activity */
static u_char *HMAkeepalive; /* Use this to keep shm_hma_alive */
static int shm_hma_id;
static int shm_wrap_id;
static int shm_video_id;
static caddr_t ipc_return;

void HMA_MAP(int HMA)
{

  priv_on();
  E_printf("Entering HMA_MAP with HMA=%d\n", HMA);
  if (shmdt(HMAAREA) < 0) {
    E_printf("HMA: Detaching HMAAREA unsuccessful: %s\n", strerror(errno));
    leavedos(48);
  }
  E_printf("HMA: detached at %p\n", HMAAREA);
  if (HMA){
    if ((ipc_return = (caddr_t) shmat(shm_hma_id, HMAAREA, SHM_REMAP )) == (caddr_t) 0xffffffff) {
      E_printf("HMA: Mapping HMA id %x to HMAAREA %p unsuccessful: %s\n",
	       shm_hma_id, HMAAREA, strerror(errno));
      leavedos(47);
    }
    E_printf("HMA: mapped id %x to %p\n", shm_hma_id, ipc_return);
  }
  else {
    if ((ipc_return = (caddr_t) shmat(shm_wrap_id, HMAAREA, 0 )) == (caddr_t) 0xffffffff) {
      E_printf("HMA: Mapping WRAP to HMAAREA unsuccessful: %s\n", strerror(errno));
      leavedos(47);
    }
    E_printf("HMA: mapped id %x to %p\n", shm_wrap_id, ipc_return);
  }
  priv_off();
}

void
set_a20(int enableHMA)
{
  if (sharedmem.hmastate == enableHMA)
    g_printf("WARNING: redundant %s of A20!\n", enableHMA ? "enabling" :
	  "disabling");

  /* to turn the A20 on, one must unmap the "wrapped" low page, and
   * map in the real HMA memory. to turn it off, one must unmap the HMA
   * and make FFFF:xxxx addresses wrap to the low page.
   */
  HMA_MAP(enableHMA);

  sharedmem.hmastate = enableHMA;
}

void HMA_init(void)
{
  /* initially, no HMA */
  HMAkeepalive = malloc(HMASIZE); /* This is used only so that shmdt stays going */
  sharedmem.hmastate = 0;

  if ((shm_hma_id = shmget(IPC_PRIVATE, HMASIZE, 0755)) < 0) {
    E_printf("HMA: Initial HMA mapping unsuccessful: %s\n", strerror(errno));
    E_printf("HMA: Do you have IPC in the kernel?\n");

    leavedos(43);
  }
  if ((shm_wrap_id = shmget(IPC_PRIVATE, HMASIZE, 0755)) < 0) {
    E_printf("HMA: Initial WRAP at 0x0 mapping unsuccessful: %s\n", strerror(errno));
    leavedos(43);
  }

#ifdef NCU
/* Here is an IPC shm area for looking at DOS's video area */
  if ((shm_video_id = shmget(IPC_PRIVATE, GRAPH_SIZE, 0755)) < 0) {
    E_printf("VIDEO: Initial IPC GET mapping unsuccessful: %s\n", strerror(errno));
    leavedos(43);
  }
  else
    v_printf("VIDEO: SHM_VIDEO_ID = 0x%x\n", shm_video_id);
#endif

  /* attach regions: page 0 (idt) at address 0 (must be specified as 1 and
   * SHM_RND, and must specify new SHM_REMAP flag to overwrite existing
   * memory (cannot munmap() it!) and code space.
   */

  if ((ipc_return = (caddr_t) shmat(shm_wrap_id, (u_char *)0x1, SHM_REMAP|SHM_RND)) == (caddr_t) 0xffffffff) {
    E_printf("HMA: Mapping to 0 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  E_printf("HMA: mapped id %x to %p\n", shm_wrap_id, ipc_return);
  if ((ipc_return = (caddr_t) shmat(shm_wrap_id, HMAAREA, SHM_REMAP)) == (caddr_t) 0xffffffff) {
    E_printf("HMA: Adding mapping to 0x100000 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  E_printf("HMA: mapped id %x to %p\n", shm_wrap_id, ipc_return);
  if ((ipc_return = (caddr_t) shmat(shm_hma_id, HMAkeepalive, SHM_REMAP)) == (caddr_t) 0xffffffff) {
    E_printf("HMA: Adding mapping to HMAkeepalive unsuccessful: %s\n", strerror(errno));
    leavedos(45);
  }
  E_printf("HMA: mapped id %x to %p\n", shm_hma_id, ipc_return);
#ifdef NCU
  if ((ipc_return = (caddr_t) shmat(shm_video_id, (caddr_t)0xb0000, SHM_REMAP)) == (caddr_t) 0xffffffff) {
    E_printf("VIDEO: Adding mapping to video memory unsuccessful: %s\n", strerror(errno));
    leavedos(45);
  }
  else
    v_printf("VIDEO: Video attached %x to %p\n", shm_video_id, ipc_return);
#endif
#ifdef __linux__
  /* On other systems, you'll have to delete the segments in hma_exit... */
  if (shmctl(shm_hma_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl HMA unsuccessful: %s\n", strerror(errno));
  }
  if (shmctl(shm_wrap_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl WRAP unsuccessful: %s\n", strerror(errno));
  }
#ifdef NCU
  if (shmctl(shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("VIDEO: Shmctl VIDEO unsuccessful: %s\n", strerror(errno));
  }
#endif
#endif
}

#if defined(SIG) && defined(REQUIRES_EMUMODULE)
static int SillyG_pendind_irq_bits=0;
int SillyG_do_irq(void)
{
  int irq=10, ret;
  ret = do_irq();
  SillyG_pendind_irq_bits &= ~(1 << irq);
  get_and_reset_irq(irq);
  return ret;
}
#endif


void
io_select(fd_set fds)
{
  static int selrtn;
  static struct timeval tvptr;

  tvptr.tv_sec=0L;
  tvptr.tv_usec=0L;

#if defined(SIG) && defined(REQUIRES_EMUMODULE)
  if (SillyG) {
    int irq_bits =get_irq_bits() & ~SillyG_pendind_irq_bits;
    if (irq_bits) {
      SillyG_t *sg=SillyG;
      while (sg->fd) {
        if (irq_bits & (1 << sg->irq)) {
          SillyG_pendind_irq_bits |= 1 << sg->irq;
          h_printf("SIG: We have an interrupt\n");
          process_interrupt(sg);
        }
        sg++;
      }
    }
  }
#endif

  while ( ((selrtn = select(25, &fds, NULL, NULL, &tvptr)) == -1)
        && (errno == EINTR)) {
    tvptr.tv_sec=0L;
    tvptr.tv_usec=0L;
    g_printf("WARNING: interrupted io_select: %s\n", strerror(errno));
  }

  switch (selrtn) {
    case 0:			/* none ready, nothing to do :-) */
      return;
      break;

    case -1:			/* error (not EINTR) */
      error("ERROR: bad io_select: %s\n", strerror(errno));
      break;

    default:			/* has at least 1 descriptor ready */

#if defined(SIG) && (!defined(REQUIRES_EMUMODULE))
      if (SillyG) {
        SillyG_t *sg=SillyG;
        while (sg->fd) {
	  if (FD_ISSET(sg->fd, &fds)) {
	    h_printf("SIG: We have an interrupt\n");
	    process_interrupt(sg);
	  }
	  sg++;
	}
      }
#endif
      if ((mice->intdrv || mice->type == MOUSE_PS2) && mice->fd >= 0)
	if (FD_ISSET(mice->fd, &fds)) {
		m_printf("MOUSE: We have data\n");
	  pic_request(PIC_IMOUSE);
	}
      if (FD_ISSET(kbd_fd, &fds)) {
	getKeys();
      }
#ifdef USE_MHPDBG
      if (mhpdbg.fdin != -1) if (FD_ISSET(mhpdbg.fdin, &fds)) mhp_input();
#endif
      /* XXX */
#if 0
      fflush(stdout);
#endif
      break;
    }
#if 0
#ifdef USING_NET
    pic_request(16);
#endif
#endif

}

inline void
process_interrupt(SillyG_t *sg)
{
  u_int chr;
  int irq;

  if ((irq = sg->irq) != 0) {
    h_printf("INTERRUPT: 0x%02x\n", irq);
    pic_request(pic_irq_list[irq]);
  }
}

void
hma_exit(void)
{
#ifdef __NetBSD__
  if (shmctl(shm_hma_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl HMA unsuccessful: %s\n", strerror(errno));
  }
  if (shmctl(shm_wrap_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl WRAP unsuccessful: %s\n", strerror(errno));
  }
#ifdef NCU
  if (shmctl(shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("VIDEO: Shmctl VIDEO unsuccessful: %s\n", strerror(errno));
  }
#endif
#endif
}
