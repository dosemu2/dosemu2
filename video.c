/* video.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/02/09 20:10:24 $
 * $Source: /home/src/dosemu0.49pl4g/RCS/video.c,v $
 * $Revision: 1.17 $
 * $State: Exp $
 *
 * Revision 1.3  1993/10/03  21:38:22  root
 * Checking the LegendClockSelect stuff ARGH !
 *
 * Revision 1.2  1993/09/30  21:45:24  root
 * Another extensive overhaul of ET4000 stuff
 *
 * Revision 1.1  1993/09/30  21:15:39  root
 * Initial revision
 *
 * Revision 1.5  1993/09/18  20:19:35  root
 * Added CRT 0x34, SEQ 0x06 and general alteration of ET4000 save/restore etc..
 *
 * Revision 1.4  1993/09/10  20:06:23  root
 * Increased Printing of ETX  Regs
 *
 * Revision 1.3  1993/09/06  22:24:35  root
 * Added et4000 regs to dump_video_regs
 *
 * Revision 1.2  1993/09/06  17:42:55  root
 * Added quick alpha switch via registers
 *
 * Revision 1.1  1993/09/06  17:20:38  root
 * Initial revision
 *
 * Revision 1.6  1993/09/05  15:32:13  root
 * *** empty log message ***
 *
 * Revision 1.5  1993/09/05  15:29:39  root
 * Added et4000 extended reg setting on bank read/write. Added some comments.[D[D[D[D[D[D[D[D[D[D
 *
 * Revision 1.4  1993/09/05  15:18:04  root
 * Banks are back in order, atleast for Trident :-), Why are they not
 * using page bit inverted as documented is beyond me....
 *
 * Revision 1.3  1993/09/05  12:30:54  root
 * Fixed protection violation (bad printf)
 *
 * Revision 1.2  1993/08/29  22:58:43  root
 * Bank modes are out of order
 *
 * Revision 1.1  1993/08/29  20:56:05  root
 * Initial revision
 *
 * Revision 1.2  1993/07/14  04:30:35  rsanders
 * changed some printf()s to v_printf()s.
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.4  1993/05/20  05:19:48  root
 * added background text writing in mode 3
 *
 * Revision 1.3  1993/05/16  22:45:33  root
 * fixed int10h ah=9/ah=0xa problems.  also fixed screen page support.
 * still has some bugs with non-console video updating.
 *
 * Revision 1.2  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.1  1993/04/05  17:25:13  root
 * Initial revision
 *
 */
#define VIDEO_C 1

#define VGA_SAVE_PAGE0

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <termio.h>
#include <sys/time.h>
#include <termcap.h>
#include <sys/mman.h>
#include <linux/mm.h>
#include <signal.h>
#include <sys/stat.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "termio.h"
#include "video.h"
#include "mouse.h"
#include "dosipc.h"
#include "modes.h"
#include "machcompat.h"

extern struct config_info config;

void dump_video_regs(void);
u_char trident_ext_video_port_in(int port);
void trident_ext_video_port_out(u_char value, int port);
u_char et4000_ext_video_port_in(int port);
void et4000_ext_video_port_out(u_char value, int port);
void dump_video();
void dump_video_linux();
u_char video_type = PLAINVGA;

struct video_save_struct linux_regs, dosemu_regs;
void (*save_ext_regs) ();
void (*restore_ext_regs) ();
void (*set_bank_read) (u_char bank);
void (*set_bank_write) (u_char bank);
void (*ext_video_port_out) (u_char value, int port);

u_char(*ext_video_port_in) (int port);
int CRT_I, CRT_D, IS1_R, FCR_W, color_text;

#define MAXDOSINTS 032
#define MAXMODES 34

u_char permissions;
u_char video_initialized = 0;

/* flags from dos.c */

struct screen_stat scr_state;	/* main screen status variables */
void release_vt(), acquire_vt();
void get_video_ram(), open_kmem(), set_console_video();
extern int mem_fd, kbd_fd, ioc_fd;

int gfx_mode = TEXT;
int max_page = 7;		/* number of highest vid page - 1*/
int screen_mode = INIT_SCREEN_MODE;

#define SETSIG(sa, sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);

inline void
forbid_switch()
{
  scr_state.vt_allow = 0;
}

inline void
allow_switch()
{
  scr_state.vt_allow = 1;
  if (scr_state.vt_requested) {
    v_printf("VID: clearing old vt request\n");
    release_vt(0);
  }
}

void
parent_close_mouse(void)
{
  ipc_send2child(DMSG_MCLOSE);
  child_close_mouse();
}

void
parent_open_mouse(void)
{
  ipc_send2child(DMSG_MOPEN);
  child_open_mouse();
}

void
acquire_vt(int sig)
{
  int kb_mode;
  struct sigaction siga;
  void *oldalrm;

  forbid_switch();

  oldalrm = signal(SIGALRM, SIG_IGN);
  SETSIG(siga, SIG_ACQUIRE, acquire_vt);

  if (ioctl(ioc_fd, VT_RELDISP, VT_ACKACQ))	/* switch acknowledged */
    v_printf("VT_RELDISP failed (or was queued)!\n");

  if (config.console_video) {
    get_video_ram(WAIT);
    set_dos_video();
    /*      if (config.vga) dos_unpause(); */
  }

  SETSIG(siga, SIGALRM, oldalrm);
  scr_state.current = 1;

  parent_open_mouse();

  allow_switch();
}

void
get_permissions()
{
  DOS_SYSCALL(set_ioperm(0x3b0, 0x3df - 0x3b0 + 1, 1));
}

void
giveup_permissions()
{
  DOS_SYSCALL(set_ioperm(0x3b0, 0x3df - 0x3b0 + 1, 0));
}

set_dos_video()
{
  if (!config.vga)
    return;

  v_printf("Setting DOS video: gfx_mode: %d\n", gfx_mode);

  /* jesx */
  /* After all that fun up there, get permissions and save/restore states */
  if (video_initialized) {
    v_printf("Acquiring vt, restoring dosemu_regs\n");
    get_perm();
    dump_video();
    restore_vga_state(&dosemu_regs);
  }

}

void
set_linux_video()
{
  if (!config.vga)
    return;

  /* jesx */
  /* After all that fun  rel perms and save/restore states */
  if (video_initialized) {
    v_printf("Storing dosemu_regs, Releasing vt mode=%02x\n", *(u_char *) 0x449);
    dosemu_regs.video_mode = *(u_char *) 0x449;
    save_vga_state(&dosemu_regs);
    dump_video();
    if (linux_regs.mem != (u_char) NULL) {
      v_printf("Restoring linux_regs, Releasing vt\n");
      dump_video_linux();
      restore_vga_state(&linux_regs);
    }
    release_perm();
  }
}

void
release_vt(int sig)
{
  struct sigaction siga;

  SETSIG(siga, SIG_RELEASE, release_vt);

  parent_close_mouse();

  if (!scr_state.vt_allow) {
    v_printf("disallowed vt switch!\n");
    scr_state.vt_requested = 1;
    return;
  }

  if (config.console_video) {
    set_linux_video();
    put_video_ram();
    /*      if (config.vga) dos_pause(); */
  }

  scr_state.current = 0;	/* our console is no longer current */
  if (do_ioctl(ioc_fd, VT_RELDISP, 1))	/* switch ok by me */
    v_printf("VT_RELDISP failed!\n");
}

int
wait_vc_active(void)
{
  if (ioctl(ioc_fd, VT_WAITACTIVE, scr_state.console_no) < 0) {
    error("ERROR: VT_WAITACTIVE for %d gave %d: %s\n", scr_state.console_no,
	  errno, strerror(errno));
    return -1;
  }
  else
    return 0;
}

/* allows remapping even if memory is mapped in...this is useful, as it
 * remembers where the video mem *was* mapped, unmaps from that, and then
 * remaps it to where the text page number says it should be
 */
void
get_video_ram(int waitflag)
{
  char *graph_mem;
  void (*oldhandler) ();
  int tmp;
  struct sigaction siga;
  char *unmappedbuf = NULL;

  char *sbase;
  size_t ssize;

  char *textbuf = NULL, *vgabuf = NULL;

  v_printf("get_video_ram STARTED\n");
  if (config.vga) {
    ssize = GRAPH_SIZE;
    sbase = (char *) GRAPH_BASE;
  }
  else {
    ssize = TEXT_SIZE;
    sbase = PAGE_ADDR(SCREEN);
  }

  if (waitflag == WAIT) {
    config.console_video = 0;
    v_printf("VID: get_video_ram WAITING\n");
    /* XXX - wait until our console is current (mixed signal functions) */
    oldhandler = signal(SIG_ACQUIRE, SIG_IGN);
    do {
      if (!wait_vc_active())
	break;
    }
    while (errno == EINTR);
    SETSIG(siga, SIG_ACQUIRE, oldhandler);
    config.console_video = 1;
  }

  if (config.vga) {
    if (VIDMODE == 3 && SCREEN < 8) {
      textbuf = malloc(TEXT_SIZE * 8);
      memcpy(textbuf, PAGE_ADDR(0), TEXT_SIZE * 8);
    }

    if (scr_state.mapped) {
      vgabuf = malloc(GRAPH_SIZE);
      memcpy(vgabuf, (caddr_t) GRAPH_BASE, GRAPH_SIZE);
      munmap((caddr_t) GRAPH_BASE, GRAPH_SIZE);
      memcpy((caddr_t) GRAPH_BASE, vgabuf, GRAPH_SIZE);
    }
  }
  else {
    textbuf = malloc(TEXT_SIZE);
    memcpy(textbuf, scr_state.virt_address, TEXT_SIZE);

    if (scr_state.mapped) {
      munmap(scr_state.virt_address, TEXT_SIZE);
      memcpy(scr_state.virt_address, textbuf, TEXT_SIZE);
    }
  }
  scr_state.mapped = 0;

  if (config.vga) {
    if (VIDMODE == 3) {
      if (dosemu_regs.mem && textbuf)
	memcpy(dosemu_regs.mem, textbuf, TEXT_SIZE * 8);
      /*      else error("ERROR: no dosemu_regs.mem!\n"); */
    }
    fprintf(stderr, "mapping GRAPH_BASE\n");
    open_kmem();
    graph_mem = (char *) mmap((caddr_t) GRAPH_BASE,
			      (size_t) (GRAPH_SIZE),
			      PROT_READ | PROT_WRITE,
			      MAP_SHARED | MAP_FIXED,
			      mem_fd,
			      GRAPH_BASE);

    close_kmem();
    /* the code below is done by the video save/restore code */
    get_permissions();
  }
  else {
    /* this is used for page switching */
    if (PAGE_ADDR(SCREEN) != scr_state.virt_address)
      memcpy(textbuf, PAGE_ADDR(SCREEN), TEXT_SIZE);

    fprintf(stderr, "mapping PAGE_ADDR\n");
    open_kmem();
    graph_mem = (char *) mmap((caddr_t) PAGE_ADDR(SCREEN),
			      TEXT_SIZE,
			      PROT_READ | PROT_WRITE,
			      MAP_SHARED | MAP_FIXED,
			      mem_fd,
			      PHYS_TEXT_BASE);

    close_kmem();
    if ((long) graph_mem < 0) {
      error("ERROR: mmap error in get_video_ram (text)\n");
      return;
    }
    else
      v_printf("CONSOLE VIDEO address: %p %p %p\n", (void *) graph_mem,
	       (void *) PHYS_TEXT_BASE, (void *) PAGE_ADDR(SCREEN));

    get_permissions();
    /* copy contents of page onto video RAM */
    memcpy((caddr_t) PAGE_ADDR(SCREEN), textbuf, TEXT_SIZE);
  }

  if (vgabuf)
    free(vgabuf);
  if (textbuf)
    free(textbuf);

  scr_state.pageno = SCREEN;
  scr_state.virt_address = PAGE_ADDR(SCREEN);
  scr_state.mapped = 1;
}

put_video_ram(void)
{
  char *putbuf = (char *) malloc(TEXT_SIZE);

  if (scr_state.mapped) {
    v_printf("put_video_ram called\n");

    if (config.vga) {
      munmap((caddr_t) GRAPH_BASE, GRAPH_SIZE);
      if (dosemu_regs.mem && VIDMODE == 3 && SCREEN < 8)
	memcpy((caddr_t) PAGE_ADDR(0), dosemu_regs.mem, TEXT_SIZE * 8);
    }
    else {
      memcpy(putbuf, scr_state.virt_address, TEXT_SIZE);
      munmap(scr_state.virt_address, TEXT_SIZE);
      memcpy(scr_state.virt_address, putbuf, TEXT_SIZE);
    }

    giveup_permissions();
    scr_state.mapped = 0;
  }
  else
    warn("VID: put_video-ram but not mapped!\n");

  if (putbuf)
    free(putbuf);
  v_printf("put_video_ram completed\n");
}

/* this puts the VC under process control */
set_process_control()
{
  struct vt_mode vt_mode;
  struct sigaction siga;

  vt_mode.mode = VT_PROCESS;
  vt_mode.waitv = 0;
  vt_mode.relsig = SIG_RELEASE;
  vt_mode.acqsig = SIG_ACQUIRE;
  vt_mode.frsig = 0;

  scr_state.vt_requested = 0;	/* a switch has not been attempted yet */
  allow_switch();

  SETSIG(siga, SIG_RELEASE, release_vt);
  SETSIG(siga, SIG_ACQUIRE, acquire_vt);

  if (do_ioctl(ioc_fd, VT_SETMODE, (int) &vt_mode))
    v_printf("initial VT_SETMODE failed!\n");
}

clear_process_control()
{
  struct vt_mode vt_mode;
  struct sigaction siga;

  vt_mode.mode = VT_AUTO;
  ioctl(ioc_fd, VT_SETMODE, (int) &vt_mode);
  SETSIG(siga, SIG_RELEASE, SIG_IGN);
  SETSIG(siga, SIG_ACQUIRE, SIG_IGN);
}

u_char kmem_open_count = 0;

void
open_kmem()
{
  /* as I understad it, /dev/kmem is the kernel's view of memory,
     * and /dev/mem is the identity-mapped (i.e. physical addressed)
     * memory. Currently under Linux, both are the same.
     */

  kmem_open_count++;

  if (mem_fd != -1 ) return;
  if ((mem_fd = open("/dev/kmem", O_RDWR)) < 0) {
    error("ERROR: can't open /dev/kmem: errno=%d, %s \n",
	  errno, strerror(errno));
      leavedos(0);
      return;
  }
  v_printf("Kmem opened successfully\n");
}

close_kmem()
{

  if (kmem_open_count) {
    kmem_open_count--;
    if (kmem_open_count) return;
    close( mem_fd);
    mem_fd = -1;
    v_printf("Kmem closed successfully\n");
  }
}

void
set_console_video()
{
  int i;

  /* clear Linux's (unmapped) screen */
  tputs(cl, 1, outch);
  scr_state.mapped = 0;

  allow_switch();

  if (config.vga)
    ioctl(ioc_fd, KDSETMODE, KD_GRAPHICS);

  /* warning! this must come first! the VT_ACTIVATES which some below
     * cause set_dos_video() and set_linux_video() to use the modecr
     * settings.  We have to first find them here.
     */
  if (config.vga) {
    int permtest;

    permtest = set_ioperm(0x3d4, 2, 1);	/* get 0x3d4 and 0x3d5 */
    permtest |= set_ioperm(0x3da, 1, 1);
    permtest |= set_ioperm(0x3c0, 2, 1);	/* get 0x3c0 and 0x3c1 */
  }

  /* XXX - get this working correctly! */
#define OLD_SET_CONSOLE 1
#ifdef OLD_SET_CONSOLE
  get_video_ram(WAIT);
  scr_state.mapped = 1;
#endif
  if (vc_active()) {
    int other_no = (scr_state.console_no == 1 ? 2 : 1);

    v_printf("VID: we're active, waiting...\n");
#ifndef OLD_SET_CONSOLE
    get_video_ram(WAIT);
    scr_state.mapped = 1;
#endif
    if (!config.vga) {
      ioctl(ioc_fd, VT_ACTIVATE, other_no);
      ioctl(ioc_fd, VT_ACTIVATE, scr_state.console_no);
    }
  }
  else
    v_printf("VID: not active, going on\n");

  allow_switch();
  clear_screen(0, 7);
}

clear_console_video()
{

  set_linux_video();
  put_video_ram();		/* unmap the screen */

  /* XXX - must be current console! */

  if (!config.vga)
    show_cursor();		/* restore the cursor */
  else {
    ioctl(ioc_fd, KIOCSOUND, 0);/* turn off any sound */
  }
  if (config.vga)
    ioctl(ioc_fd, KDSETMODE, KD_TEXT);
}

void
map_bios(void)
{
  char *video_bios_mem, *system_bios_mem;

  /* assume VGA BIOS size of 32k here and in bios_emm.c */
  open_kmem();
  video_bios_mem =
    (char *) mmap(
		   (caddr_t) VBIOS_START,
		   VBIOS_SIZE,
		   PROT_READ | PROT_EXEC,
		   MAP_PRIVATE | MAP_FIXED,
		   mem_fd,
		   0xc0000
    );
  close_kmem();

  if ((long) video_bios_mem < 0) {
    error("ERROR: mmap error in map_bios %s\n", strerror(errno));
    return;
  }
  else
    g_printf("VIDEO BIOS address: %p\n", (void *) video_bios_mem);

#if MAP_SYSTEM_BIOS

  open_kmem();
  system_bios_mem =
    (char *) mmap(
		   (caddr_t) 0xf0000,
		   64 * 1024,
		   PROT_READ | PROT_EXEC,
		   MAP_PRIVATE | MAP_FIXED,
		   mem_fd,
		   0xf0000
    );
  close_kmem();

  if ((long) system_bios_mem < 0) {
    error("ERROR: mmap error in map_bios %s\n", strerror(errno));
    return;
  }
  else
    g_printf("SYSTEM BIOS address: 0x%x\n", system_bios_mem);
#endif
}

int
vc_active(void)
{				/* return 1 if our VC is active */
  struct vt_stat vtstat;

  error("VC_ACTIVE!\n");
  ioctl(ioc_fd, VT_GETSTATE, &vtstat);
  error("VC_ACTIVE: ours: %d, active: %d\n", scr_state.console_no, vtstat.v_active);
  return ((vtstat.v_active == scr_state.console_no));
}

void
set_vc_screen_page(int page)
{
  SCREEN = page;

  /* okay, if we have the current console, and video ram is mapped.
   * this has to be "atomic," or there is a "race condition": the
   * user may change consoles between our check and our remapping
   */
  forbid_switch();
  if (scr_state.mapped)		/* should I check for foreground VC instead? */
    get_video_ram(WAIT);
  allow_switch();
}

hide_cursor()
{
#ifdef USE_NCURSES
  /* Can't figure out how to hide cursor with ncurses yet */
  return;
#else
  /* the Linux terminal driver hide_cursor string is
   * "\033[?25l" - put it in as the "vi" termcap in /etc/termcap
   */
  tputs(vi, 1, outch);
#endif
}

show_cursor()
{
#ifdef USE_NCURSES
  return;
#else
  /* the Linux terminal driver show_cursor string is
   * "\033[?25h" - put it in as the "ve" termcap in /etc/termcap
   */
  tputs(ve, 1, outch);
#endif
}

void
int10(void)
{
  /* some code here is copied from Alan Cox ***************/
  int x, y, i, tmp;
  unsigned int s;
  static int gfx_flag = 0;
  char tmpchar;
  char c, m;
  us *sm;

  if (d.video >= 3)
    dbug_printf("int10 AH=%02x AL=%02x\n", HI(ax), LO(ax));

  NOCARRY;
  switch (HI(ax)) {
  case 0x0:			/* define mode */
    v_printf("define mode: 0x%x\n", LO(ax));
    switch (LO(ax)) {
    case 2:
    case 3:
      v_printf("text mode from %d to %d\n", screen_mode, LO(ax));
      max_page = 7;
      screen_mode = LO(ax);
      gfx_flag = 0;		/* we're in a text mode now */
      gfx_mode = TEXT;
      /* mode change clears screen unless bit7 of AL set */
      if (!(LO(ax) & 0x80))
	clear_screen(SCREEN, 7);
      NOCARRY;
      break;
    default:
      v_printf("undefined video mode 0x%x\n", LO(ax));
      CARRY;
      return;
    }

    /* put the mode in the BIOS Data Area */
    VIDMODE = screen_mode;
    break;

  case 0x1:			/* define cursor shape */
    v_printf("define cursor: 0x%04x\n", LWORD(ecx));
    /* 0x20 is the cursor no blink/off bit */
    /*	if (HI(cx) & 0x20) */
    if (REG(ecx) == 0x2000)
      hide_cursor();
    else
      show_cursor();
    CARRY;
    break;

  case 0x2:			/* set cursor pos */
    s = HI(bx);
    x = LO(dx);
    y = HI(dx);
    if (s > 7) {
      error("ERROR: video error (setcur/s>7: %d)\n", s);
      CARRY;
      return;
    }
    if (x >= CO || y >= LI)
      break;

    XPOS(s) = x;
    YPOS(s) = y;

    if (s == SCREEN)
      poscur(x, y);
    break;

  case 0x3:			/* get cursor pos */
    s = HI(bx);
    if (s > 7) {
      error("ERROR: video error(0x3 s>7: %d)\n", s);
      CARRY;
      return;
    }
    REG(edx) = (YPOS(s) << 8) | XPOS(s);
    break;

  case 0x5:
    {				/* change page */
      s = LO(ax);
      error("VID: change page from %d to %d!\n", SCREEN, s);
      if (s <= max_page) {
	if (!config.console_video) {
	  scrtest_bitmap = 1 << (24 + s);
	  vm86s.screen_bitmap = -1;
	  update_screen = 1;
	}
	else
	  set_vc_screen_page(s);
      }
      else {
	error("ERROR: video error: set bad screen %d\n", s);
	CARRY;
      }

      SCREEN = s;
      PAGE_OFFSET = TEXT_SIZE * s;

      poscur(XPOS(SCREEN), YPOS(SCREEN));
      break;
    }

  case 0x6:			/* scroll up */
    v_printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!config.console_video)
      vm86s.screen_bitmap = -1;
    break;

  case 0x7:			/* scroll down */
    v_printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!config.console_video)
      vm86s.screen_bitmap = -1;
    break;

  case 0x8:			/* read character at x,y + attr */
    s = HI(bx);
    if (s > max_page) {
      error("ERROR: read char from bad page %d\n", s);
      CARRY;
      break;
    }
    sm = SCREEN_ADR(s);
    REG(eax) = sm[CO * YPOS(s) + XPOS(s)];
    break;

    /* these two put literal character codes into memory, and do
       * not scroll or move the cursor...
       * the difference is that 0xA ignores color for text modes
       */
  case 0x9:
  case 0xA:
    {
      u_short *sadr;
      u_char attr = LO(bx);

      s = HI(bx);
      sadr = SCREEN_ADR(s) + YPOS(s) * CO + XPOS(s);
      x = *(us *) & REG(ecx);
      c = LO(ax);

      /* XXX - need to make sure this doesn't overrun video memory!
       * we don't really know how large video memory is yet (I haven't
       * abstracted things yet) so we'll just leave it for the
       * Great Video Housecleaning foretold in the TODO file of the
       * ancients
       */
      if (HI(ax) == 9)
	for (i = 0; i < x; i++)
	  *(sadr++) = c | (attr << 8);
      else
	for (i = 0; i < x; i++) {	/* leave attribute as it is */
	  *sadr &= 0xff00;
	  *(sadr++) |= c;
	}

      if (!config.console_video)
	update_screen = 1;

      break;
    }

  case 0xe:			/* print char */
    char_out(*(char *) &REG(eax), SCREEN, ADVANCE);	/* char in AL */
    break;

  case 0x0f:			/* get screen mode */
    REG(eax) = (CO << 8) | screen_mode;
    v_printf("get screen mode: 0x%04x s=%d\n", LWORD(eax), SCREEN);
    HI(bx) = SCREEN;
    break;

#if 0
  case 0xb:			/* palette */
  case 0xc:			/* set dot */
  case 0xd:			/* get dot */
    break;
#endif

  case 0x4:			/* get light pen */
    error("ERROR: video error(no light pen)\n");
    CARRY;
    return;

  case 0x1a:			/* get display combo */
    if (LO(ax) == 0) {
      v_printf("get display combo!\n");
      LO(ax) = 0x1a;		/* valid function=0x1a */
      LO(bx) = VID_COMBO;	/* active display */
      HI(bx) = 0;		/* no inactive display */
    }
    else {
      v_printf("set display combo not supported\n");
    }
    break;

  case 0x12:			/* video subsystem config */
    v_printf("get video subsystem config ax=0x%04x bx=0x%04x\n",
	     LWORD(eax), LWORD(ebx));
    switch (LO(bx)) {
    case 0x10:
      HI(bx) = VID_SUBSYS;
      /* this breaks qedit! (any but 0x10) */
      /* LO(bx)=3;  */
      v_printf("video subsystem 0x10 BX=0x%04x\n", LWORD(ebx));
      REG(ecx) = 0x0809;
      break;
    case 0x20:
      v_printf("select alternate printscreen\n");
      break;
    case 0x30:
      if (LO(ax) == 0)
	tmp = 200;
      else if (LO(ax) == 1)
	tmp = 350;
      else
	tmp = 400;
      v_printf("select textmode scanlines: %d", tmp);
      LO(ax) = 12;		/* ok */
      break;
    case 0x32:			/* enable/disable cpu access to cpu */
      if (LO(ax) == 0)
	v_printf("disable cpu access to video!\n");
      else
	v_printf("enable cpu access to video!\n");
      break;
    default:
      error("ERROR: unrecognized video subsys config!!\n");
      show_regs();
    }
    break;

  case 0xfe:			/* get shadow buffer..return unchanged */
  case 0xff:			/* update shadow buffer...do nothing */
    break;

  case 0x11:			/* ega character generator */
  case 0x10:			/* ega palette */
  case 0x4f:			/* vesa interrupt */

  default:
    error("new unknown video int 0x%x\n", LWORD(eax));
    CARRY;
    break;
  }
}

int
vga_screenoff()
{
  v_printf("vga_screenoff\n");
  /* synchronous reset on */
  port_out(0x00, SEQ_I);
  port_out(0x01, SEQ_D);

  /* turn off screen for faster VGA memory acces */
  port_out(0x01, SEQ_I);
  port_out(port_in(SEQ_D) | 0x20, SEQ_D);

  /* synchronous reset off */
  port_out(0x00, SEQ_I);
  port_out(0x03, SEQ_D);

  return 0;
}

int
vga_screenon()
{
  v_printf("vga_screenon\n");

  /* synchronous reset on */
  port_out(0x00, SEQ_I);
  port_out(0x01, SEQ_D);

  /* turn screen back on */
  port_out(0x01, SEQ_I);
  port_out(port_in(SEQ_D) & 0xDF, SEQ_D);

  /* synchronous reset off */
  port_out(0x00, SEQ_I);
  port_out(0x03, SEQ_D);

  return 0;
}

int
vga_setpalvec(int start, int num, u_char * pal)
{
  int i, j;

  /* select palette register */
  port_out(start, PEL_IW);

  for (j = 0; j < num; j++) {
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
  }

  /* Upon Restore Videos, also restore current Palette REgs */
  /*  port_out(dosemu_regs.regs[PELIW], PEL_IW);
  port_out(dosemu_regs.regs[PELIR], PEL_IR); */

  return j;
}

int
vga_getpalvec(int start, int num, u_char * pal)
{
  int i, j;

  /* Save Palette Regs */
  /* dosemu_regs.regs[PELIW]=port_in(PEL_IW);
  dosemu_regs.regs[PELIR]=port_in(PEL_IR); */

  /* select palette register */
  port_out(start, PEL_IR);

  for (j = 0; j < num; j++) {
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
    for (i = 0; i < 10; i++) ;	/* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
  }

  /* Put Palette regs back */
  /* port_out(dosemu_regs.regs[PELIW], PEL_IW);
  port_out(dosemu_regs.regs[PELIR], PEL_IR); */
}

/* get ioperms to allow havoc to occur */
int
get_perm()
{
  permissions = permissions + 1;
  if (permissions > 1) {
    return;
  }
  /* get I/O permissions for VGA registers */
  if (ioperm(0x3b0, 0x3df - 0x3b0 + 1, 1)) {
    v_printf("VGA: can't get I/O permissions \n");
    exit(-1);
  }

  /* color or monochrome text emulation? */
  color_text = port_in(MIS_R) & 0x01;

  /* chose registers for color/monochrome emulation */
  if (color_text) {
    CRT_I = CRT_IC;
    CRT_D = CRT_DC;
    IS1_R = IS1_RC;
    FCR_W = FCR_WC;
  }
  else {
    CRT_I = CRT_IM;
    CRT_D = CRT_DM;
    IS1_R = IS1_RM;
    FCR_W = FCR_WM;
  }
  v_printf("Permission allowed\n");
}

/* Stop io to card */
int
release_perm()
{
  if (permissions > 0) {
    permissions = permissions - 1;
    if (permissions > 0) {
      return;
    }
    /* release I/O permissions for VGA registers */
    if (ioperm(0x3b0, 0x3df - 0x3b0 + 1, 0)) {
      v_printf("VGA: can't release I/O permissions \n");
      leavedos(-1);
    }
    v_printf("Permission disallowed\n");
  }
  else
    v_printf("Permissions already at 0\n");

}

int
set_regs(u_char regs[])
{
  int i;
  int x;

  /* port_out(dosemu_regs.regs[FCR], FCR_W); */
  /* update misc output register */
  port_out(regs[MIS], MIS_W);

  /* write sequencer registers */
  /* synchronous reset on */
  port_out(0x00, SEQ_I);
  port_out(0x01, SEQ_D);
  port_out(0x01, SEQ_I);
  port_out(regs[SEQ + 1] | 0x20, SEQ_D);
  /* synchronous reset off */
  port_out(0x00, SEQ_I);
  port_out(0x03, SEQ_D);
  for (i = 2; i < SEQ_C; i++) {
    port_out(i, SEQ_I);
    port_out(regs[SEQ + i], SEQ_D);
  }

  /* deprotect CRT registers 0-7 */
  port_out(0x11, CRT_I);
  port_out(port_in(CRT_D) & 0x7F, CRT_D);

  /* write CRT registers */
  for (i = 0; i < CRT_C; i++) {
    port_out(i, CRT_I);
    port_out(regs[CRT + i], CRT_D);
  }

  /* write graphics controller registers */
  for (i = 0; i < GRA_C; i++) {
    port_out(i, GRA_I);
    port_out(regs[GRA + i], GRA_D);
  }

  /* write attribute controller registers */
  for (i = 0; i < ATT_C; i++) {
    port_in(IS1_R);		/* reset flip-flop */
    port_out(i, ATT_IW);
    port_out(regs[ATT + i], ATT_IW);
  }

  /* port_out(dosemu_regs.regs[CRTI], CRT_I);
port_out(dosemu_regs.regs[GRAI], GRA_I);
port_out(dosemu_regs.regs[SEQI], SEQ_I); */
  /* v_printf("CRTI=0x%02x\n",dosemu_regs.regs[CRTI]);
v_printf("GRAI=0x%02x\n",dosemu_regs.regs[GRAI]);
v_printf("SEQI=0x%02x\n",dosemu_regs.regs[SEQI]); */
  return (0);
}

/* Prepare to do business with the EGA/VGA card */
inline void
disable_vga_card()
{
  /* enable video */
  port_in(IS1_R);
  port_out(0x00, ATT_IW);
}

/* Finish doing business with the EGA/VGA card */
inline void
enable_vga_card()
{
  port_in(IS1_R);
  port_out(0x20, ATT_IW);
  /* disable video */
}

/* Reset Attribute registers */
inline void
reset_att()
{
  port_in(IS1_R);
}

/* Store current EGA/VGA regs */
int
store_vga_regs(char regs[])
{
  int i, j;

  /* Start with INDEXS */
  regs[CRTI] = port_in(CRT_I);
  regs[GRAI] = port_in(GRA_I);
  regs[SEQI] = port_in(SEQ_I);
  regs[FCR] = port_in(FCR_R);
  regs[ISR1] = port_in(IS1_R) | 0x09;
  regs[ISR0] = port_in(IS0_R);

  /* save VGA registers */
  for (i = 0; i < CRT_C; i++) {
    port_out(i, CRT_I);
    regs[CRT + i] = port_in(CRT_D);
  }

  for (i = 0; i < ATT_C; i++) {
    port_in(IS1_R);
    port_out(i, ATT_IW);
    regs[ATT + i] = port_in(ATT_R);
  }
  for (i = 0; i < GRA_C; i++) {
    port_out(i, GRA_I);
    regs[GRA + i] = port_in(GRA_D);
  }
  for (i = 1; i < SEQ_C; i++) {
    port_out(i, SEQ_I);
    regs[SEQ + i] = port_in(SEQ_D);
  }
  regs[SEQ + 1] = regs[SEQ + 1] | 0x20;

  regs[MIS] = port_in(MIS_R);

  port_out(regs[CRTI], CRT_I);
  port_out(regs[GRAI], GRA_I);
  port_out(regs[SEQI], SEQ_I);
  v_printf("Store regs complete!\n");
}

/* Store EGA/VGA display planes (4) */
void
store_vga_mem(u_char * mem, u_char mem_size[], u_char banks)
{

  int p[4], bsize, position;
  u_char cbank, plane, counter;

  p[0] = mem_size[0] * 1024;
  p[1] = mem_size[1] * 1024;
  p[2] = mem_size[2] * 1024;
  p[3] = mem_size[3] * 1024;
  bsize = p[0] + p[1] + p[2] + p[3];
  dump_video_regs();
  if (config.chipset == ET4000) {
    /*
 * The following is from the X files
 * we need this here , cause we MUST disable the ROM SYNC feature
*/
    u_char temp1, temp2;

    port_out(0x34, CRT_I);
    temp1 = port_in(CRT_D);
    port_out(temp1 & 0x0F, CRT_D);
    temp2 = port_in(0x3cd);
    port_out(0x00, 0x3cd);
  }
  if (!config.chipset || banks == 1) {
    set_regs((u_char *) vregs);
    position = 0;
    for (plane = 0; plane < 4; plane++) {

      /* Store planes */
      port_out(0x04, GRA_I);
      port_out(plane, GRA_D);

      memcpy((caddr_t) (mem + position), (caddr_t) GRAPH_BASE, p[plane]);
      position = position + p[plane];
    }
    v_printf("READ Bank=%d, plane=0x%02x, mem=%08x\n", banks, plane, *(int *) GRAPH_BASE);
  }
  else {
    for (cbank = 0; cbank < banks; cbank++) {
      position = 0;
      if (cbank < 10) {
	for (plane = 0; plane < 4; plane++) {
	  set_bank_read((cbank * 4) + plane);
	  memcpy((caddr_t) (mem + (bsize * cbank) + position), (caddr_t) GRAPH_BASE, p[plane]);
	  position = position + p[plane];
	  for (counter = 0; counter < 80; counter++) {
	    v_printf("%c", *(u_char *) (GRAPH_BASE + counter));
	  }
	  v_printf("\n");
	  for (counter = 0; counter < 80; counter++)
	    v_printf("0x%02x ", *(u_char *) (GRAPH_BASE + counter));
	  v_printf("\n");
	  v_printf("BANK READ Bank=%d, plane=0x%02x, mem=%08x\n", cbank, plane, *(int *) GRAPH_BASE);
	}
      }
    }
  }
  v_printf("GRAPH_BASE to mem complete!\n");
  if (config.chipset == ET4000) {
    port_out(0x00, 0x3cd);
  }
}

/* Restore EGA/VGA display planes (4) */
void
restore_vga_mem(u_char * mem, u_char mem_size[], u_char banks)
{
  int p[4], bsize, position, plane;
  u_char cbank, counter;

  p[0] = mem_size[0] * 1024;
  p[1] = mem_size[1] * 1024;
  p[2] = mem_size[2] * 1024;
  p[3] = mem_size[3] * 1024;
  bsize = p[0] + p[1] + p[2] + p[3];

  dump_video_regs();
  if (config.chipset == ET4000)
    port_out(0x00, 0x3cd);
  if (!config.chipset || banks == 1) {
    set_regs((u_char *) vregs);
    position = 0;
    for (plane = 0; plane < 4; plane++) {

      /* disable Set/Reset Register */
      port_out(0x01, GRA_I);
      port_out(0x00, GRA_D);

      /* Store planes */
      port_out(0x02, SEQ_I);
      port_out(1 << plane, SEQ_D);

      memcpy((caddr_t) GRAPH_BASE, (caddr_t) (mem + position), p[plane]);
      position = position + p[plane];
    }
    v_printf("WRITE Bank=%d, plane=0x%02x, mem=%08x\n", banks, plane, *(caddr_t) mem);
  }
  else {
    plane = 0;
    for (cbank = 0; cbank < banks; cbank++) {
      position = 0;
      if (cbank < 10) {
	for (plane = 0; plane < 4; plane++) {
	  set_bank_write((cbank * 4) + plane);
	  memcpy((caddr_t) GRAPH_BASE, (caddr_t) (mem + (bsize * cbank) + position), p[plane]);
	  position = position + p[plane];
	  for (counter = 0; counter < 20; counter++)
	    v_printf("0x%02x ", *(u_char *) (mem + (bsize * cbank) + position + counter));
	  v_printf("\n");
	}
      }
    }
    v_printf("BANK WRITE Bank=%d, plane=0x%02x, mem=%08x\n", cbank, plane, *(int *) (mem + (bsize * cbank)));
  }
  v_printf("mem to GRAPH_BASE complete!\n");
}

/* Restore EGA/VGA regs */
int
restore_vga_regs(char regs[], u_char xregs[])
{
  set_regs(regs);
  restore_ext_regs(xregs);
  v_printf("Restore completed!\n");
}

/* Save all necessary info to allow switching vt's */
void
save_vga_state(struct video_save_struct *save_regs)
{

  v_printf("Saving data for %s\n", save_regs->video_name);
  vga_screenoff();
  disable_vga_card();
  store_vga_regs(save_regs->regs);
  save_ext_regs(save_regs->xregs);
  v_printf("ALPHA mode save being attempted\n");
  port_out(0x06, GRA_I);
  if (!(port_in(GRA_D) & 0x01)) {
    save_regs->save_mem_size[0] = 8;
    save_regs->save_mem_size[1] = 8;
    save_regs->save_mem_size[2] = 8;
    save_regs->save_mem_size[3] = 8;
    save_regs->banks = 1;
    v_printf("ALPHA mode save being performed\n");
  }
  else
    switch (save_regs->video_mode) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 0x51:
      save_regs->save_mem_size[0] = 64;
      save_regs->save_mem_size[1] = 64;
      save_regs->save_mem_size[2] = 64;
      save_regs->save_mem_size[3] = 64;
      save_regs->banks = 1;
      break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
      save_regs->save_mem_size[0] = 64;
      save_regs->save_mem_size[1] = 64;
      save_regs->save_mem_size[2] = 64;
      save_regs->save_mem_size[3] = 64;
      save_regs->banks = 1;
      break;
    default:
      save_regs->save_mem_size[0] = 64;
      save_regs->save_mem_size[1] = 64;
      save_regs->save_mem_size[2] = 64;
      save_regs->save_mem_size[3] = 64;
      save_regs->banks =
	(config.gfxmemsize + 255) / 256;
      break;
    }
  v_printf("Mode  == %d\n", save_regs->video_mode);
  v_printf("Banks == %d\n", save_regs->banks);
  if (!save_regs->mem)
    save_regs->mem = malloc((save_regs->save_mem_size[0] +
			     save_regs->save_mem_size[1] +
			     save_regs->save_mem_size[2] +
			     save_regs->save_mem_size[3]) * 1024 *
			    save_regs->banks
      );

  store_vga_mem(save_regs->mem, save_regs->save_mem_size, save_regs->banks);
  vga_getpalvec(0, 256, save_regs->pal);
  restore_vga_regs(save_regs->regs, save_regs->xregs);
  enable_vga_card();

  v_printf("Store_vga_state complete\n");
}

/* Restore all necessary info to allow switching back to vt */
void
restore_vga_state(struct video_save_struct *save_regs)
{

  v_printf("Restoring data for %s\n", save_regs->video_name);
  vga_screenoff();
  disable_vga_card();
  restore_vga_regs(save_regs->regs, save_regs->xregs);
  restore_vga_mem(save_regs->mem, save_regs->save_mem_size, save_regs->banks);
  if (save_regs->release_video) {
    free(save_regs->mem);
    save_regs->mem = NULL;
  }
  vga_setpalvec(0, 256, save_regs->pal);
  restore_vga_regs(save_regs->regs, save_regs->xregs);
  v_printf("Permissions=%d\n", permissions);
  enable_vga_card();
  vga_screenon();

  v_printf("Restore_vga_state complete\n");
}

/* These are dummy calls */
void
save_ext_regs_dummy(int regs)
{
  return;
}

void
restore_ext_regs_dummy(int regs)
{
  return;
}

void
set_bank_read_dummy(u_char bank)
{
  return;
}

void
set_bank_write_dummy(u_char bank)
{
  return;
}

u_char
dummy_ext_video_port_in(int port)
{
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void
dummy_ext_video_port_out(u_char value, int port)
{
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

void
trident_set_old_regs()
{
  port_out(0x0b, SEQ_I);
  port_out(0x00, SEQ_D);
  return;
}

void
trident_set_new_regs()
{
  u_char dummy;

  port_out(0x0b, SEQ_I);
  port_out(0x00, SEQ_D);
  dummy = port_in(SEQ_D);
  return;
}

void
trident_allow_svga()
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  dummy = port_in(SEQ_D);
  v_printf("Dummy then = 0x%02x\n", dummy);
  port_out(((dummy & ~0x20) | 0x10), SEQ_D);
  dummy = port_in(SEQ_D);
  v_printf("Dummy  now = 0x%02x\n", dummy);
  trident_set_new_regs();
  return;
}

void
trident_disallow_svga()
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  dummy = port_in(SEQ_D);
  port_out(((dummy | 0x20) & ~0x10), SEQ_D);
  trident_set_new_regs();
  v_printf("Disallow SVGA Modes = 0x%02x\n", dummy & ~0x10);
  return;
}

u_char
trident_get_svga()
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  dummy = port_in(SEQ_D) & 0x10;
  trident_set_new_regs();
  v_printf("Trident get SVGA, dummy = 0x%02x\n", dummy);
  return (dummy);
}

/* These set banks only work on 256k boundaries */
void
trident_set_bank_read(u_char bank)
{
  if (trident_get_svga()) {
    trident_allow_svga();
    port_out(0x0e, SEQ_I);
    port_out(bank, SEQ_D);
  }
  else {
    port_out(0x0e, SEQ_I);
    port_out(0x02, SEQ_D);
  }
  v_printf("Trident set bank read = 0x%02x\n", bank);
  return;
}

void
trident_set_bank_write(u_char bank)
{
  if (trident_get_svga()) {
    trident_allow_svga();
    port_out(0x0e, SEQ_I);
    port_out(bank, SEQ_D);
  }
  else {
    port_out(0x0e, SEQ_I);
    port_out(0x02, SEQ_D);
  }
  v_printf("Trident set bank write= 0x%02x\n", bank);
  return;
}

void
trident_save_ext_regs(u_char xregs[])
{
  port_out(0x0c, SEQ_I);
  xregs[0] = port_in(SEQ_D) & 0xff;
  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  xregs[1] = port_in(SEQ_D) & 0xff;
  port_out(0x0e, SEQ_I);
  xregs[2] = port_in(SEQ_D) & 0xff;
  trident_set_new_regs();
  port_out(0x0d, SEQ_I);
  xregs[3] = port_in(SEQ_D) & 0xff;
  port_out(0x0e, SEQ_I);
  xregs[4] = port_in(SEQ_D) & 0xff;
  port_out(0x0f, SEQ_I);
  xregs[5] = port_in(SEQ_D) & 0xff;
  port_out(0x1e, CRT_I);
  xregs[6] = port_in(CRT_D) & 0xff;
  port_out(0x1f, CRT_I);
  xregs[7] = port_in(CRT_D) & 0xff;
  port_out(0x0f, GRA_I);
  xregs[8] = port_in(GRA_D) & 0xff;
  return;
}

void
trident_restore_ext_regs(u_char xregs[])
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  port_out(xregs[1], SEQ_D);
  port_out(0x0e, SEQ_I);
  port_out(xregs[2], SEQ_D);
  trident_set_new_regs();
  port_out(0x0e, SEQ_I);
  port_out(0x82, SEQ_D);
  port_out(0x0c, SEQ_I);
  port_out(xregs[0], SEQ_D);
  v_printf("Check for change in 0x%02x\n", port_in(SEQ_D));
  port_out(0x0d, SEQ_I);
  port_out(xregs[3], SEQ_D);
  port_out(0x0e, SEQ_I);
  port_out(xregs[4] ^ 0x02, SEQ_D);
  port_out(0x0f, SEQ_I);
  port_out(xregs[5], SEQ_D);
  port_out(0x1e, CRT_I);
  port_out(xregs[6], CRT_D);
  port_out(0x1f, CRT_I);
  port_out(xregs[7], CRT_D);
  port_out(0x0f, GRA_I);
  port_out(xregs[8], GRA_D);
  port_out(0x0c, SEQ_I);
  port_out(xregs[0], SEQ_D);
  return;
}

/* These are trident Specific calls to get at banks */
vga_init_trident()
{
  port_out(0x0b, SEQ_I);
  port_out(0x00, SEQ_D);
  if (port_in(SEQ_D) >= 0x03) {
    save_ext_regs = trident_save_ext_regs;
    restore_ext_regs = trident_restore_ext_regs;
    set_bank_read = trident_set_bank_read;
    set_bank_write = trident_set_bank_write;
    ext_video_port_in = trident_ext_video_port_in;
    ext_video_port_out = trident_ext_video_port_out;
    v_printf("Trident is 8900+\n");
  }
  else {
    save_ext_regs = trident_save_ext_regs;
    restore_ext_regs = trident_restore_ext_regs;
    set_bank_read = trident_set_bank_read;
    set_bank_write = trident_set_bank_write;
    ext_video_port_in = trident_ext_video_port_in;
    ext_video_port_out = trident_ext_video_port_out;
    v_printf("Trident is 8800\n");
  }
  return;
}

/* THese are et4000 specific functions */
void
et4000_save_ext_regs(u_char xregs[])
{
  xregs[12] = port_in(0x3bf) & 0xff;
  xregs[11] = port_in(0x3d8) & 0xff;
  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  xregs[0] = port_in(0x3cd) & 0xff;
  xregs[18] = port_in(MIS_R);
  port_out(0x24, CRT_I);
  xregs[1] = port_in(CRT_D) & 0xff;
  port_out(0x32, CRT_I);
  xregs[2] = port_in(CRT_D) & 0xff;
  port_out(0x34, CRT_I);
  xregs[15] = port_in(CRT_D) & 0xff;
  port_out(xregs[15] & 0x0f, CRT_D);
  port_out(0x00, 0x3cd);
  port_out(0x33, CRT_I);
  xregs[3] = port_in(CRT_D) & 0xff;
  port_out(0x35, CRT_I);
  xregs[4] = port_in(CRT_D) & 0xff;
  port_out(0x36, CRT_I);
  xregs[5] = port_in(CRT_D) & 0xff;
  port_out(0x37, CRT_I);
  xregs[6] = port_in(CRT_D) & 0xff;
  port_out(0x3f, CRT_I);
  xregs[7] = port_in(CRT_D) & 0xff;
  port_out(0x6, SEQ_I);
  xregs[16] = port_in(SEQ_D) & 0xff;
  port_out(0x7, SEQ_I);
  xregs[8] = port_in(SEQ_D) & 0xff;
  port_in(IS1_R);
  port_out(0x36, ATT_IW);
  xregs[9] = port_in(ATT_R) & 0xff;
  port_out(xregs[9], ATT_IW);
  xregs[17] = port_in(0x3c3) & 0xff;
  xregs[10] = port_in(0x3cb) & 0xff;
  port_out(0xd, GRA_I);
  xregs[13] = port_in(GRA_D) & 0xff;
  port_out(0xe, GRA_I);
  xregs[14] = port_in(GRA_D) & 0xff;
}

void
et4000_restore_ext_regs(u_char xregs[])
{
  u_char temp;

  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  port_out(0x00, 0x3cd);
  port_out(0x6, SEQ_I);
  port_out(xregs[16], SEQ_D);
  port_out(0x7, SEQ_I);
  port_out(xregs[8], SEQ_D);
  port_in(IS1_R);
  port_out(0x36, ATT_IW);
  port_out(xregs[9], ATT_IW);
  port_out(0x24, CRT_I);
  port_out(xregs[1], CRT_D);
  port_out(0x32, CRT_I);
  port_out(xregs[2], CRT_D);
  port_out(0x33, CRT_I);
  port_out(xregs[3], CRT_D);
  port_out(0xe3, MIS_W);
  port_out(0x34, CRT_I);
  /* First time through leave as is. Second time, remove the #define JESX 1 line */
#define JESX 1
#ifdef JESX
  port_out(0x34, CRT_I);
  port_out(xregs[15], CRT_D);
  port_out(xregs[18], MIS_W);
#else
  temp = port_in(MIS_R);
  port_out((temp & 0xf3) | ((xregs[15] & 0x10) >> 1) | (xregs[15] & 0x04), MIS_W);
  port_out(0x34, CRT_I);
  port_out(0x00, CRT_D);
  port_out(0x02, CRT_D);
  port_out((xregs[15] & 0x08) >> 2, CRT_D);
  port_out(xregs[18], MIS_W);
#endif
  port_out(0x35, CRT_I);
  port_out(xregs[4], CRT_D);
  port_out(xregs[17], 0x3c3);
  port_out(0x36, CRT_I);
  port_out(xregs[5], CRT_D);
  port_out(0x37, CRT_I);
  port_out(xregs[6], CRT_D);
  port_out(0x3f, CRT_I);
  port_out(xregs[7], CRT_D);
  port_out(xregs[10], 0x3cb);
  port_out(0xd, GRA_I);
  port_out(xregs[13], GRA_D);
  port_out(0xe, GRA_I);
  port_out(xregs[14], GRA_D);
  port_out(xregs[0], 0x3cd);
  port_out(xregs[12], 0x3bf);
  port_out(xregs[11], 0x3d8);
  port_out(0x0300, 0x3c4);
}

void
et4000_set_bank_read(u_char bank)
{
  u_char dummy;

  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  dummy = port_in(0x3cd) & 0x0f;
  port_out(dummy | (bank << 4), 0x3cd);
}

void
et4000_set_bank_write(u_char bank)
{
  u_char dummy;

  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  dummy = port_in(0x3cd) & 0xf0;
  port_out(dummy | (bank), 0x3cd);
}

vga_init_et4000()
{
  save_ext_regs = et4000_save_ext_regs;
  restore_ext_regs = et4000_restore_ext_regs;
  set_bank_read = et4000_set_bank_read;
  set_bank_write = et4000_set_bank_write;
  ext_video_port_in = et4000_ext_video_port_in;
  ext_video_port_out = et4000_ext_video_port_out;
  return;
}

void
vga_initialize(void)
{
  linux_regs.mem = NULL;
  dosemu_regs.mem = NULL;
  get_perm();

  switch (config.chipset) {
  case TRIDENT:
    vga_init_trident();
    v_printf("Trident CARD in use\n");
    break;
  case ET4000:
    vga_init_et4000();
    v_printf("ET4000 CARD in use\n");
    break;
  default:
    save_ext_regs = save_ext_regs_dummy;
    restore_ext_regs = restore_ext_regs_dummy;
    set_bank_read = set_bank_read_dummy;
    set_bank_write = set_bank_write_dummy;
    ext_video_port_in = dummy_ext_video_port_in;
    ext_video_port_out = dummy_ext_video_port_out;
    v_printf("Unspecific VIDEO selected = 0x%04x\n", config.chipset);
  }

  linux_regs.video_name = "Linux Regs";
  linux_regs.save_mem_size[0] = 8;
  linux_regs.save_mem_size[1] = 8;
  linux_regs.save_mem_size[2] = 8;
  linux_regs.save_mem_size[3] = 8;
  linux_regs.banks = 1;
  linux_regs.video_mode = 3;
  linux_regs.release_video = 0;

  dosemu_regs.video_name = "Dosemu Regs";
  dosemu_regs.save_mem_size[0] = 8;
  dosemu_regs.save_mem_size[1] = 8;
  dosemu_regs.save_mem_size[2] = 8;
  dosemu_regs.save_mem_size[3] = 8;
  dosemu_regs.banks = (config.gfxmemsize + 255) / 256;
  dosemu_regs.video_mode = 3;
  dosemu_regs.regs[CRTI] = 0;
  dosemu_regs.regs[SEQI] = 0;
  dosemu_regs.regs[GRAI] = 0;

  /* don't release it; we're trying text mode restoration */
  dosemu_regs.release_video = 1;

  v_printf("VGA: mem size %d, banks %d\n", config.gfxmemsize,
	   dosemu_regs.banks);

  save_vga_state(&linux_regs);
  vga_screenon();
  memset((caddr_t) linux_regs.mem, ' ', 8 * 1024);
  dump_video_linux();
  video_initialized = 1;
  /* release_perm(); */
}

/* Attempt to virtualize calls to video ports */

static u_char att_d_index = 0;
static isr_read = 0, crt_ok = 0;

u_char
video_port_in(int port)
{
  /* v_printf("Video read on port 0x%04x.\n",port); */
  switch (port) {
  case CRT_IC:
  case CRT_IM:
    v_printf("Read Index CRTC 0x%02x\n", dosemu_regs.regs[CRTI]);
    return (dosemu_regs.regs[CRTI]);
  case CRT_DC:
  case CRT_DM:
    if (dosemu_regs.regs[CRTI] <= 0x18) {
      v_printf("Read Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs.regs[CRTI], dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]]);
      return (dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]]);
    }
    else
      return (ext_video_port_in(port));
    break;
  case GRA_I:
    v_printf("Read Index GRAI 0x%02x\n", GRAI);
    return (dosemu_regs.regs[GRAI]);
  case GRA_D:
    if (dosemu_regs.regs[GRAI] < 0x0a) {
      v_printf("Read Data at GRA  Index 0x%02x = 0x%02x \n", dosemu_regs.regs[GRAI], dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]]);
      return (dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]]);
    }
    else
      return (ext_video_port_in(port));
    break;
  case SEQ_I:
    v_printf("Read Index SEQI 0x%02x\n", SEQI);
    return (dosemu_regs.regs[SEQI]);
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] < 0x07) {
      v_printf("Read Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs.regs[SEQI], dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]]);
      return (dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]]);
    }
    else
      return (ext_video_port_in(port));
    break;
  case ATT_IW:
    v_printf("Read Index ATTIW 0x%02x\n", att_d_index);
    return (att_d_index);
  case ATT_R:
    if (att_d_index > 20)
      return (ext_video_port_in(port));
    else {
      v_printf("Read Index ATTR 0x%02x\n", dosemu_regs.regs[ATT + att_d_index]);
      return (dosemu_regs.regs[ATT + att_d_index]);
    }
    break;
  case IS1_RC:
  case IS1_RM:
    v_printf("Read ISR1_R 0x%02x\n", dosemu_regs.regs[ISR1]);
    isr_read = 1;		/* Next ATT write will be to the index */
    return (dosemu_regs.regs[ISR1]);
  case MIS_R:
    v_printf("Read MIS_R 0x%02x\n", dosemu_regs.regs[MIS]);
    return (dosemu_regs.regs[MIS]);
  case IS0_R:
    v_printf("Read ISR0_R 0x%02x\n", dosemu_regs.regs[ISR0]);
    return (dosemu_regs.regs[ISR0]);
  case PEL_IW:
    v_printf("Read PELIW 0x%02x\n", dosemu_regs.regs[PELIW] / 3);
    return (dosemu_regs.regs[PELIW] / 3);
  case PEL_IR:
    v_printf("Read PELIR 0x%02x\n", dosemu_regs.regs[PELIR] / 3);
    return (dosemu_regs.regs[PELIR] / 3);
  case PEL_D:
    v_printf("Read PELIR Data 0x%02x\n", dosemu_regs.pal[dosemu_regs.regs[PELIR]]);
    return (dosemu_regs.pal[dosemu_regs.regs[PELIR]++]);
  case PEL_M:
    v_printf("Read PELM  Data 0x%02x\n", dosemu_regs.regs[PELM] == 0 ? 0xff : dosemu_regs.regs[PELM]);
    return (dosemu_regs.regs[PELM] == 0 ? 0xff : dosemu_regs.regs[PELM]);
  default:
    return (ext_video_port_in(port));
  }
}

void
video_port_out(u_char value, int port)
{
  /* v_printf("Video write on port 0x%04x,byte 0x%04x.\n",port, value); */
  switch (port) {
  case CRT_IC:
  case CRT_IM:
    v_printf("Set Index CRTC 0x%02x\n", value);
    dosemu_regs.regs[CRTI] = value;
    break;
  case CRT_DC:
  case CRT_DM:
    if (dosemu_regs.regs[CRTI] <= 0x18) {
      v_printf("Write Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]] = value;
    }
    else
      ext_video_port_out(value, port);
    break;
  case GRA_I:
    v_printf("Set Index GRAI 0x%02x\n", value);
    dosemu_regs.regs[GRAI] = value;
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] < 0x0a) {
      v_printf("Write Data at GRAI Index 0x%02x = 0x%02x \n", dosemu_regs.regs[GRAI], value);
      dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]] = value;
    }
    else
      ext_video_port_out(value, port);
    break;
  case SEQ_I:
    v_printf("Set Index SEQI 0x%02x\n", value);
    dosemu_regs.regs[SEQI] = value;
    break;
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] <= 0x05) {
      v_printf("Write Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs.regs[SEQI], value);
      dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]] = value;
    }
    else
      ext_video_port_out(value, port);
    break;
  case ATT_IW:
    if (isr_read) {
      v_printf("Set Index ATTI 0x%02x\n", value & 0x1f);
      att_d_index = value & 0x1f;
      isr_read = 0;
    }
    else {
      isr_read = 1;
      if (att_d_index > 20)
	ext_video_port_out(value, port);
      else {
	v_printf("Write Data at ATT Index 0x%02x = 0x%02x \n", att_d_index, value);
	dosemu_regs.regs[ATT + att_d_index] = value;
      }
    }
    break;
  case MIS_W:
    v_printf("Set MISW 0x%02x\n", value);
    dosemu_regs.regs[MIS] = value;
    break;
  case PEL_IW:
    v_printf("Set PELIW  to 0x%02x\n", value);
    dosemu_regs.regs[PELIW] = value * 3;
    break;
  case PEL_IR:
    v_printf("Set PELIR to 0x%02x\n", value);
    dosemu_regs.regs[PELIR] = value * 3;
    break;
  case PEL_D:
    v_printf("Put PEL_D[0x%02x] = 0x%02x\n", dosemu_regs.regs[PELIW], value);
    dosemu_regs.pal[dosemu_regs.regs[PELIW]] = value;
    dosemu_regs.regs[PELIW] += 1;
    break;
  case PEL_M:
    v_printf("Set PEL_M to 0x%02x\n", value);
    dosemu_regs.regs[PELM] = value;
    break;
  case GR1_P:
    v_printf("Set GR1_P to 0x%02x\n", value);
    dosemu_regs.regs[GR1P] = value;
    break;
  case GR2_P:
    v_printf("Set GR2_P to 0x%02x\n", value);
    dosemu_regs.regs[GR2P] = value;
    break;
  default:
    ext_video_port_out(value, port);
  }
  return;
}

static trident_old_regs = 0;

u_char
trident_ext_video_port_in(int port)
{
  switch (port) {
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x0b) {
      trident_old_regs = !trident_old_regs;
      v_printf("old=1/new=0 = 0x%02x\n", trident_old_regs);
      return (0x04);
    }
    if (trident_old_regs) {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("OLD Read on SEQI 0x0d got 0x%02x\n", dosemu_regs.xregs[1]);
	return (dosemu_regs.xregs[1]);
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("OLD Read on SEQI 0x0e got 0x%02x\n", dosemu_regs.xregs[2]);
	return (dosemu_regs.xregs[2]);
      }
    }
    else {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("NEW Read on SEQI 0x0d got 0x%02x\n", dosemu_regs.xregs[3]);
	return (dosemu_regs.xregs[3]);
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("NEW Read on SEQI 0x0e got 0x%02x\n", dosemu_regs.xregs[4]);
	return (dosemu_regs.xregs[4]);
      }
    }
    if (dosemu_regs.regs[SEQI] == 0x0f) {
      v_printf("Read on SEQI 0x0f got 0x%02x\n", dosemu_regs.xregs[5]);
      return (dosemu_regs.xregs[5]);
    }
    if (dosemu_regs.regs[SEQI] == 0x0c) {
      v_printf("Read on SEQI 0x0c got 0x%02x\n", dosemu_regs.xregs[0]);
      return (dosemu_regs.xregs[0]);
    }
    break;
  case CRT_DC:
  case CRT_DM:
    if (dosemu_regs.regs[CRTI] == 0x1e) {
      v_printf("Read on CRT_D 0x1e got 0x%02x\n", dosemu_regs.xregs[6]);
      return (dosemu_regs.xregs[6]);
    }
    else if (dosemu_regs.regs[CRTI] == 0x1f) {
      v_printf("Read on CRT_D 0x1f got 0x%02x\n", dosemu_regs.xregs[7]);
      return (dosemu_regs.xregs[7]);
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0f) {
      v_printf("Read on GRA_D 0x0f got 0x%02x\n", dosemu_regs.xregs[8]);
      return (dosemu_regs.xregs[8]);
    }
    break;
  }
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void
trident_ext_video_port_out(u_char value, int port)
{
  switch (port) {
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x0b) {
      trident_old_regs = 1;
      v_printf("Old_regs reset to OLD = 0x%02x\n", trident_old_regs);
      return;
    }
    if (trident_old_regs) {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("OLD Write on SEQ_D 0x0d sent 0x%02x\n", value);
	dosemu_regs.xregs[1] = value;
	return;
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("OLD Write to SEQI at 0x0e put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[2]);
	return;
	dosemu_regs.xregs[2] = value;
	return;
      }
    }
    else {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("NEW Write to SEQI at 0x0d put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[3]);
	dosemu_regs.xregs[3] = value;
	return;
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("NEW Write to SEQI at 0x0e put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[4]);
	dosemu_regs.xregs[4] = value;
	return;
      }
    }
    if (dosemu_regs.regs[SEQI] == 0x0c) {
      v_printf("Write to SEQI at 0x0c put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[0]);
      dosemu_regs.xregs[0] = value;
      return;
    }
    if (dosemu_regs.regs[SEQI] == 0x0f) {
      v_printf("Write to SEQI at 0x0f put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[5]);
      dosemu_regs.xregs[5] = value;
      return;
    }
    break;
  case CRT_DC:
  case CRT_DM:
    if (dosemu_regs.regs[CRTI] == 0x1e) {
      v_printf("Write to CRTI at 0x1e put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[6]);
      dosemu_regs.xregs[6] = value;
      return;
    }
    else if (dosemu_regs.regs[CRTI] == 0x1f) {
      v_printf("Write to CRTI at 0x1f put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[7]);
      dosemu_regs.xregs[7] = value;
      return;
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0f) {
      v_printf("Write to GRAD at 0x0f put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[8]);
      return;
    }
    break;
  }
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

u_char
et4000_ext_video_port_in(int port)
{
  switch (port) {
  case 0x3c3:
    v_printf("et4000 read of port 0x3c3 = 0x%02x\n", dosemu_regs.xregs[17]);
    return (dosemu_regs.xregs[17]);
  case 0x3bf:
    v_printf("et4000 read of port 0x3bf = 0x%02x\n", dosemu_regs.xregs[12]);
    return (dosemu_regs.xregs[12]);
  case 0x3cb:
    v_printf("et4000 read of port 0x3cb = 0x%02x\n", dosemu_regs.xregs[10]);
    return (dosemu_regs.xregs[10]);
  case 0x3cd:
    v_printf("et4000 read of port 0x3cd = 0x%02x\n", dosemu_regs.xregs[0]);
    return (dosemu_regs.xregs[0]);
  case 0x3d8:
    v_printf("et4000 read of port 0x3d8 = 0x%02x\n", dosemu_regs.xregs[11]);
    return (dosemu_regs.xregs[11]);
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x07) {
      v_printf("et4000 SEQ_D read at 0x07 of 0x%02x\n", dosemu_regs.xregs[8]);
      return (dosemu_regs.xregs[8]);
    }
    else if (dosemu_regs.regs[SEQI] == 0x06) {
      v_printf("et4000 SEQ_D read at 0x06 of 0x%02x\n", dosemu_regs.xregs[16]);
      return (dosemu_regs.xregs[16]);
    }
    break;
  case CRT_DC:
  case CRT_DM:
    switch (dosemu_regs.regs[CRTI]) {
    case 0x24:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[1]);
    case 0x32:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[2]);
      return (dosemu_regs.xregs[2]);
    case 0x33:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[3]);
      return (dosemu_regs.xregs[3]);
    case 0x34:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[15]);
      return (dosemu_regs.xregs[15]);
    case 0x35:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[4]);
      return (dosemu_regs.xregs[4]);
    case 0x36:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[5]);
      return (dosemu_regs.xregs[5]);
    case 0x37:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[6]);
      return (dosemu_regs.xregs[6]);
    case 0x3f:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[7]);
      return (dosemu_regs.xregs[7]);
    default:
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0d) {
      v_printf("ExtRead on GRA_D 0x0d got 0x%02x\n", dosemu_regs.xregs[13]);
      return (dosemu_regs.xregs[13]);
    }
    else if (dosemu_regs.regs[GRAI] == 0x0e) {
      v_printf("ExtRead on GRA_D 0x0e got 0x%02x\n", dosemu_regs.xregs[14]);
      return (dosemu_regs.xregs[14]);
    }
    break;
  case ATT_R:
    if (att_d_index == 0x16) {
      v_printf("ExtRead on ATT_R 0x16 got 0x%02x\n", dosemu_regs.xregs[9]);
      return (dosemu_regs.xregs[9]);
    }
    break;
  }
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void
et4000_ext_video_port_out(u_char value, int port)
{
  switch (port) {
  case 0x3c3:
    v_printf("et4000 Ext write on port 0x3c3 = 0x%02x\n", value);
    dosemu_regs.xregs[17] = value;
    return;
  case 0x3bf:
    v_printf("et4000 Ext write on port 0x3bf = 0x%02x\n", value);
    dosemu_regs.xregs[12] = value;
    return;
  case 0x3cb:
    v_printf("et4000 Ext write on port 0x3cb = 0x%02x\n", value);
    dosemu_regs.xregs[10] = value;
    return;
  case 0x3cd:
    v_printf("et4000 Ext write on port 0x3cd = 0x%02x\n", value);
    dosemu_regs.xregs[0] = value;
    return;
  case 0x3d8:
    v_printf("et4000 Ext write on port 0x3d8 = 0x%02x\n", value);
    dosemu_regs.xregs[11] = value;
    return;
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x07) {
      v_printf("et4000 SEQ_D Ext write at 0x07 of 0x%02x\n", value);
      dosemu_regs.xregs[8] = value;
      return;
    }
    else if (dosemu_regs.regs[SEQI] == 0x06) {
      v_printf("et4000 SEQ_D Ext write at 0x06 of 0x%02x\n", value);
      dosemu_regs.xregs[16] = value;
      return;
    }
    break;
  case CRT_DC:
  case CRT_DM:
    switch (dosemu_regs.regs[CRTI]) {
    case 0x24:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[1] = value;
      return;
    case 0x32:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[2] = value;
      return;
    case 0x33:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[3] = value;
      return;
    case 0x34:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[15] = value;
      return;
    case 0x35:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[4] = value;
      return;
    case 0x36:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[5] = value;
      return;
    case 0x37:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[6] = value;
      return;
    case 0x3f:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[7] = value;
      return;
    default:
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0d) {
      v_printf("ExtWrite on GRA_D 0x0d of 0x%02x\n", value);
      dosemu_regs.xregs[13] = value;
      return;
    }
    else if (dosemu_regs.regs[GRAI] == 0x0e) {
      v_printf("ExtWrite on GRA_D 0x0e of 0x%02x\n", value);
      dosemu_regs.xregs[14] = value;
      return;
    }
    break;
  case ATT_IW:
    if (att_d_index == 0x16) {
      v_printf("ExtWrite on ATT_IW 0x16 of 0x%02x\n", value);
      dosemu_regs.xregs[9] = value;
      return;
    }
    break;
  default:
    break;
  }
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

/* Generally I will request a video mode change switch quickly, and have the
   actuall registers printed out,as well as the dump of what dump_video
   received and fix the differences */

/* Dump what's in the dosemu_regs */
void
dump_video(void)
{
  char i;

  v_printf("/* BIOS mode 0x%02X */\n", dosemu_regs.video_mode);
  v_printf("static char regs[60] = {\n  ");
  for (i = 0; i < 12; i++)
    v_printf("0x%02X,", dosemu_regs.regs[CRT + i]);
  v_printf("\n  ");
  for (i = 12; i < CRT_C; i++)
    v_printf("0x%02X,", dosemu_regs.regs[CRT + i]);
  v_printf("\n  ");
  for (i = 0; i < 12; i++)
    v_printf("0x%02X,", dosemu_regs.regs[ATT + i]);
  v_printf("\n  ");
  for (i = 12; i < ATT_C; i++)
    v_printf("0x%02X,", dosemu_regs.regs[ATT + i]);
  v_printf("\n  ");
  for (i = 0; i < GRA_C; i++)
    v_printf("0x%02X,", dosemu_regs.regs[GRA + i]);
  v_printf("\n  ");
  for (i = 0; i < SEQ_C; i++)
    v_printf("0x%02X,", dosemu_regs.regs[SEQ + i]);
  v_printf("\n  ");
  v_printf("0x%02X", dosemu_regs.regs[MIS]);
  v_printf("\n};\n");
  v_printf("Extended Regs/if applicable:\n");
  for (i = 0; i < 17; i++)
    v_printf("0x%02X,", dosemu_regs.xregs[i]);
  v_printf("0x%02X,", dosemu_regs.xregs[17]);
  v_printf("\n");
}

/* Dump what's in the linux_regs */
void
dump_video_linux(void)
{
  char i;

  v_printf("/* BIOS mode 0x%02X */\n", linux_regs.video_mode);
  v_printf("static char regs[60] = {\n  ");
  for (i = 0; i < 12; i++)
    v_printf("0x%02X,", linux_regs.regs[CRT + i]);
  v_printf("\n  ");
  for (i = 12; i < CRT_C; i++)
    v_printf("0x%02X,", linux_regs.regs[CRT + i]);
  v_printf("\n  ");
  for (i = 0; i < 12; i++)
    v_printf("0x%02X,", linux_regs.regs[ATT + i]);
  v_printf("\n  ");
  for (i = 12; i < ATT_C; i++)
    v_printf("0x%02X,", linux_regs.regs[ATT + i]);
  v_printf("\n  ");
  for (i = 0; i < GRA_C; i++)
    v_printf("0x%02X,", linux_regs.regs[GRA + i]);
  v_printf("\n  ");
  for (i = 0; i < SEQ_C; i++)
    v_printf("0x%02X,", linux_regs.regs[SEQ + i]);
  v_printf("\n  ");
  v_printf("0x%02X", linux_regs.regs[MIS]);
  v_printf("\n};\n");
  v_printf("Extended Regs/if applicable:\n");
  for (i = 0; i < 17; i++)
    v_printf("0x%02X,", linux_regs.xregs[i]);
  v_printf("0x%02X", linux_regs.xregs[17]);
  v_printf("\n");
}

/* Store current actuall EGA/VGA regs */
void
dump_video_regs()
{
  int i;

  /* save VGA registers */
  v_printf("CRT=\n");
  for (i = 0; i < CRT_C; i++) {
    port_out(i, CRT_I);
    v_printf("0x%02x ", (u_char) port_in(CRT_D));
  }
  v_printf("\n");
  v_printf("ATT=\n");
  for (i = 0; i < ATT_C; i++) {
    port_in(IS1_R);
    port_out(i, ATT_IW);
    v_printf("0x%02x ", (u_char) port_in(ATT_R));
  }
  v_printf("\n");
  v_printf("GRA=\n");
  for (i = 0; i < GRA_C; i++) {
    port_out(i, GRA_I);
    v_printf("0x%02x ", (u_char) port_in(GRA_D));
  }
  v_printf("\n");
  v_printf("SEQ=\n");
  for (i = 0; i < SEQ_C; i++) {
    port_out(i, SEQ_I);
    v_printf("0x%02x ", (u_char) port_in(SEQ_D));
  }
  v_printf("\n");
  v_printf("MIS=0x%02x\n", (u_char) port_in(MIS_R));

  /* et4000 regs should be added here .... :-) */
  if (config.chipset == TRIDENT) {
    trident_set_old_regs();
    port_out(0x0c, SEQ_I);
    v_printf("0C=0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x0d, SEQ_I);
    v_printf("0D=0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x0e, SEQ_I);
    v_printf("0E=0x%02x\n", (u_char) port_in(SEQ_D));
    trident_set_new_regs();
    port_out(0x0d, SEQ_I);
    v_printf("0D=0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x0e, SEQ_I);
    v_printf("0E=0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x0f, SEQ_I);
    v_printf("0F=0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x1e, CRT_I);
    v_printf("CRT 1E=0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x1f, CRT_I);
    v_printf("CRT 1F=0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x0f, GRA_I);
    v_printf("GRA 0F=0x%02x\n", (u_char) port_in(GRA_D));
  }
  else if (config.chipset == ET4000) {
    v_printf("ET4000 port 0x3c3=0x%02x\n", (u_char) port_in(0x3c3));
    v_printf("ET4000 port 0x3cd=0x%02x\n", (u_char) port_in(0x3cd));
    port_out(0x24, CRT_I);
    v_printf("ET4000 CRT 0x24  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x32, CRT_I);
    v_printf("ET4000 CRT 0x32  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x33, CRT_I);
    v_printf("ET4000 CRT 0x33  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x34, CRT_I);
    v_printf("ET4000 CRT 0x34  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x35, CRT_I);
    v_printf("ET4000 CRT 0x35  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x36, CRT_I);
    v_printf("ET4000 CRT 0x36  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x37, CRT_I);
    v_printf("ET4000 CRT 0x37  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x3f, CRT_I);
    v_printf("ET4000 CRT 0x3f  =0x%02x\n", (u_char) port_in(CRT_D));
    port_out(0x6, SEQ_I);
    v_printf("ET4000 SEQ 0x06  =0x%02x\n", (u_char) port_in(SEQ_D));
    port_out(0x7, SEQ_I);
    v_printf("ET4000 SEQ 0x07  =0x%02x\n", (u_char) port_in(SEQ_D));
    port_in(IS1_R);
    port_out(0x16, ATT_IW);
    v_printf("ET4000 ATT 0x16  =0x%02x\n", (u_char) port_in(ATT_R));
    v_printf("ET4000 port 0x3cb=0x%02x\n", (u_char) port_in(0x3cb));
    v_printf("ET4000 port 0x3d8=0x%02x\n", (u_char) port_in(0x3d8));
    v_printf("ET4000 port 0x3bf=0x%02x\n", (u_char) port_in(0x3bf));
    port_out(0xd, GRA_I);
    v_printf("ET4000 GRA 0x0d  =0x%02x\n", (u_char) port_in(GRA_D));
    port_out(0xe, GRA_I);
    v_printf("ET4000 GRA 0x0e  =0x%02x\n", (u_char) port_in(GRA_D));
  }
}

#undef VIDEO_C
