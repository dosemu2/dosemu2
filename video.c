/* video.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/05/16 22:45:33 $
 * $Source: /usr/src/dos/RCS/video.c,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: video.c,v $
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
#include <stdarg.h>
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

#include "emu.h"
#include "memory.h"
#include "termio.h"
#include "dosvga.h"
#include "mouse.h"
#include "dosipc.h"
#include "modes.h"
#include "config.h"
#include "machcompat.h"

extern struct config_info config;

/* jesx */

u_char video_type = PLAINVGA;

struct video_save_struct linux_regs, dosemu_regs;
void (* save_ext_regs)();
void (* restore_ext_regs)();
void (* set_bank_read)(u_char bank);
void (* set_bank_write)(u_char bank);
int CRT_I, CRT_D, IS1_R, color_text;

#define MAXDOSINTS 032
#define MAXMODES 34

u_char permissions, dostoregs[0xff];
u_char video_initialized=0;

/* flags from dos.c */

struct screen_stat scr_state;   /* main screen status variables */
void release_vt(), acquire_vt();
extern int mem_fd, kbd_fd, ioc_fd;

int gfx_mode = TEXT;
int max_page = 7;		/* number of highest vid page - 1*/
int screen_mode=INIT_SCREEN_MODE;

#define SETSIG(sa, sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);

inline void
forbid_switch()
{
  scr_state.vt_allow=0;
}

inline void
allow_switch()
{
  scr_state.vt_allow=1;
  if (scr_state.vt_requested)
    {
      v_printf("VID: clearing old vt request\n");
      release_vt(0);
    }
}


void
parent_close_mouse(void)
{
  ipc_send2child(DMSG_MCLOSE);
}

void
parent_open_mouse(void)
{
  ipc_send2child(DMSG_MOPEN);
}

      
void 
acquire_vt(int sig)
{
  int kb_mode;
  struct sigaction siga;
  void *oldalrm;

  forbid_switch();

  oldalrm=signal(SIGALRM, SIG_IGN);
  SETSIG(siga, SIG_ACQUIRE, acquire_vt);

  if (ioctl(ioc_fd, VT_RELDISP, VT_ACKACQ)) /* switch acknowledged */
    v_printf("VT_RELDISP failed (or was queued)!\n");

  if (config.console_video)
    {
      get_video_ram(WAIT);
      set_dos_video();
/*      if (config.vga) dos_unpause(); */
    }

  SETSIG(siga, SIGALRM, oldalrm);
  scr_state.current=1;

/*  parent_open_mouse(); */

  allow_switch();
}


void
get_permissions()
{
  DOS_SYSCALL( set_ioperm(0x3b0,0x3df-0x3b0+1, 1) ); 
}

void
giveup_permissions()
{
  DOS_SYSCALL( set_ioperm(0x3b0,0x3df-0x3b0+1, 0) ); 
}


set_dos_video()
{
  if (!config.vga) return;

  v_printf("Setting DOS video: gfx_mode: %d modecr = 0x%x\n", gfx_mode);

/* jesx */
/* After all that fun up there, get permissions and save/restore states */
  if (video_initialized) 
    {
      get_perm();
   /* if (dosemu_regs.mem!=(u_char)0x0)
      save_vga_state(&linux_regs); */
      restore_vga_state(&dosemu_regs);
    }

}


void
set_linux_video()
{
  if (!config.vga) return;

  /* jesx */
  /* After all that fun  rel perms and save/restore states */
  if (video_initialized)
    {
      get_perm();
      dosemu_regs.video_mode=*(u_char *)0x449;
      save_vga_state(&dosemu_regs);
      if (linux_regs.mem != (u_char)0x0)
	restore_vga_state(&linux_regs);
      release_perm();
    }
}


void release_vt(int sig)
{
  struct sigaction siga;

  SETSIG(siga, SIG_RELEASE, release_vt);

/*  parent_close_mouse(); */

  if (! scr_state.vt_allow)
    {
      v_printf("disallowed vt switch!\n");
      scr_state.vt_requested=1;
      return;
    }

  if (config.console_video)
    {
      set_linux_video();
      put_video_ram();
/*      if (config.vga) dos_pause(); */
    }

  scr_state.current = 0;  /* our console is no longer current */

  if (do_ioctl(ioc_fd, VT_RELDISP, 1))       /* switch ok by me */
    v_printf("VT_RELDISP failed!\n");
}


int 
wait_vc_active(void)
{
  if (ioctl(ioc_fd, VT_WAITACTIVE, scr_state.console_no) < 0)
    {
      error("ERROR: VT_WAITACTIVE for %d gave %d: %s\n", scr_state.console_no,
	    errno, strerror(errno));
      return -1;
    }
  else return 0;
}


/* allows remapping even if memory is mapped in...this is useful, as it
 * remembers where the video mem *was* mapped, unmaps from that, and then
 * remaps it to where the text page number says it should be
 */
get_video_ram(int waitflag)
{
  char *graph_mem;
  void (*oldhandler)();
  int tmp;
  struct sigaction siga;
  char *unmappedbuf=NULL;

  char *sbase;
  size_t ssize;

  char *textbuf=NULL, *vgabuf=NULL;

  if (config.vga) { ssize = GRAPH_SIZE; sbase = (char *)GRAPH_BASE; }
  else { ssize = TEXT_SIZE; sbase = PAGE_ADDR(SCREEN); }

  if (waitflag == WAIT)
    {
      config.console_video=0;
      v_printf("VID: get_video_ram WAITING\n");
      /* XXX - wait until our console is current (mixed signal functions) */
      oldhandler=signal(SIG_ACQUIRE, SIG_IGN);
      do  { if ( !wait_vc_active() ) break; } while (errno == EINTR);
      SETSIG(siga, SIG_ACQUIRE, oldhandler);
      config.console_video=1;
    }


  if (config.vga) {
    if (VIDMODE == 3 && SCREEN < 8) {
      textbuf = malloc(TEXT_SIZE * 8);
      memcpy (textbuf, PAGE_ADDR(0), TEXT_SIZE * 8);
    }

    if (scr_state.mapped) {
      vgabuf = malloc(GRAPH_SIZE);
      memcpy( vgabuf, (caddr_t)GRAPH_BASE, GRAPH_SIZE );
      munmap( (caddr_t)GRAPH_BASE, GRAPH_SIZE );
      memcpy( (caddr_t)GRAPH_BASE, vgabuf, GRAPH_SIZE );
    }
  }
  else {
    textbuf = malloc(TEXT_SIZE);
    memcpy( textbuf, scr_state.virt_address, TEXT_SIZE );

    if (scr_state.mapped) {
      munmap( scr_state.virt_address, TEXT_SIZE );
      memcpy( scr_state.virt_address, textbuf, TEXT_SIZE );
    }
  }
  scr_state.mapped=0;

  if (config.vga) {
    if (VIDMODE == 3) {
      warn("WARNING: doing VGA mode text save\n");
      if (dosemu_regs.mem && textbuf) 
	memcpy (dosemu_regs.mem, textbuf, TEXT_SIZE * 8);
      else error("ERROR: no dosemu_regs.mem!\n");
    }
    graph_mem = (char *)mmap((caddr_t)GRAPH_BASE,
			     (size_t)GRAPH_SIZE,
			     PROT_READ|PROT_WRITE,
			     MAP_SHARED|MAP_FIXED,
			     mem_fd, 
			     GRAPH_BASE);

    /* the code below is done by the video save/restore code */
    get_permissions();
  }
  else
  {
    /* this is used for page switching */
    if (PAGE_ADDR(SCREEN) != scr_state.virt_address)
      memcpy( textbuf, PAGE_ADDR(SCREEN), TEXT_SIZE);

    graph_mem = (char *)mmap((caddr_t)PAGE_ADDR(SCREEN), 
			     TEXT_SIZE,
			     PROT_READ|PROT_WRITE,
			     MAP_SHARED|MAP_FIXED,
			     mem_fd, 
			     PHYS_TEXT_BASE);
    
    if ((long)graph_mem < 0) {
      error("ERROR: mmap error in get_video_ram (text)\n");
      return (1);
    }
    else v_printf("CONSOLE VIDEO address: 0x%x 0x%x 0x%x\n", graph_mem,
		  PHYS_TEXT_BASE, PAGE_ADDR(SCREEN));

    /* copy contents of page onto video RAM */
    memcpy((caddr_t)PAGE_ADDR(SCREEN), textbuf, TEXT_SIZE);
  }
  
  if (vgabuf) free(vgabuf);
  if (textbuf) free(textbuf);
  
  scr_state.pageno=SCREEN;
  scr_state.virt_address=PAGE_ADDR(SCREEN);
  scr_state.mapped=1;
}


put_video_ram(void)
{
  char *putbuf = (char *)malloc(TEXT_SIZE);

  if (scr_state.mapped)
    {
      v_printf("put_video_ram called\n"); 

      if (config.vga) {
	munmap((caddr_t)GRAPH_BASE, GRAPH_SIZE);
	if (dosemu_regs.mem && VIDMODE == 3 && SCREEN < 8)
	  memcpy((caddr_t)PAGE_ADDR(0), dosemu_regs.mem, TEXT_SIZE * 8);
      }
      else {
	memcpy(putbuf, scr_state.virt_address, TEXT_SIZE);
	munmap(scr_state.virt_address, TEXT_SIZE);
	memcpy(scr_state.virt_address, putbuf, TEXT_SIZE);
      }

      giveup_permissions();
      scr_state.mapped=0;
    }
  else warn("VID: put_video-ram but not mapped!\n");

  if (putbuf) free(putbuf);
}


/* this puts the VC under process control */
set_process_control()
{
  struct vt_mode vt_mode;
  struct sigaction siga;

  vt_mode.mode = VT_PROCESS;
  vt_mode.waitv=0;
  vt_mode.relsig=SIG_RELEASE;
  vt_mode.acqsig=SIG_ACQUIRE;
  vt_mode.frsig=0;

  scr_state.vt_requested=0;    /* a switch has not been attempted yet */  
  allow_switch();

  SETSIG(siga, SIG_RELEASE, release_vt);
  SETSIG(siga, SIG_ACQUIRE, acquire_vt);

  if (do_ioctl(ioc_fd, VT_SETMODE, (int)&vt_mode))
    v_printf("initial VT_SETMODE failed!\n");
}


clear_process_control()
{
 struct vt_mode vt_mode;
 struct sigaction siga;

 vt_mode.mode = VT_AUTO;
 ioctl(ioc_fd, VT_SETMODE, (int)&vt_mode);
 SETSIG(siga, SIG_RELEASE, SIG_IGN);
 SETSIG(siga, SIG_ACQUIRE, SIG_IGN);
}


open_kmem()
{
    /* as I understad it, /dev/kmem is the kernel's view of memory,
     * and /dev/mem is the identity-mapped (i.e. physical addressed)
     * memory. Currently under Linux, both are the same.
     */
    if ((mem_fd = open("/dev/mem", O_RDWR) ) < 0) {
	error("ERROR: can't open /dev/mem: errno=%d, %s \n", 
	      errno, strerror(errno));
	return (-1);
    }
}


set_console_video()
{
    int i;

    /* clear Linux's (unmapped) screen */
    tputs(cl, 1, outc);
    v_printf("set_console_video called\n");
    scr_state.mapped=0;

    allow_switch();

/*    if (config.vga) ioctl(ioc_fd, KDSETMODE, KD_GRAPHICS);  */

    /* warning! this must come first! the VT_ACTIVATES which some below
     * cause set_dos_video() and set_linux_video() to use the modecr
     * settings.  We have to first find them here.
     */
    if (config.vga)
      {
	int permtest;

	permtest = set_ioperm(0x3d4, 2, 1);  /* get 0x3d4 and 0x3d5 */
	permtest |= set_ioperm(0x3da, 1, 1);
	permtest |= set_ioperm(0x3c0, 2, 1);  /* get 0x3c0 and 0x3c1 */
      }

/* XXX - get this working correctly! */
#define OLD_SET_CONSOLE 1
#ifdef OLD_SET_CONSOLE
    get_video_ram(WAIT);
    scr_state.mapped=1;
#endif
    if (vc_active()) 
      {
	int other_no=(scr_state.console_no == 1 ? 2 : 1);

	v_printf("VID: we're active, waiting...\n");
#ifndef OLD_SET_CONSOLE
	get_video_ram(WAIT);
	scr_state.mapped=1;
#endif
	ioctl(ioc_fd, VT_ACTIVATE, other_no);
	ioctl(ioc_fd, VT_ACTIVATE, scr_state.console_no);
      }
    else v_printf("VID: not active, going on\n");

    allow_switch();

    clear_screen(0,0);
}


clear_console_video()
{
  error("ENTERING clear_console_video()\n");

/*  if (config.vga) ioctl(ioc_fd, KDSETMODE, KD_TEXT);  */

  set_linux_video();
  put_video_ram();		/* unmap the screen */

  /* XXX - must be current console! */

  if (!config.vga) show_cursor();		/* restore the cursor */
  else {
    ioctl(ioc_fd, KIOCSOUND, 0);   /* turn off any sound */
  }

#if 0
  if (config.vga)
    {
      int sfd;

      warn("WARNING: writing ram!\n");
      sfd=open("/etc/dosemu/vram",O_RDWR|O_CREAT);
      RPT_SYSCALL( write( sfd, dosemu_regs.mem, 32 * 1024 ) );
      close(sfd);
    }
#endif

  error("LEAVING clear_console_video()\n");
}


void map_bios(void)
{
  char *video_bios_mem, *system_bios_mem;

/* assume VGA BIOS size of 32k here and in bios_emm.c */
  video_bios_mem =
    (char *)mmap(
		 (caddr_t)VBIOS_START, 
		 VBIOS_SIZE,
		 PROT_READ|PROT_EXEC,
		 MAP_PRIVATE|MAP_FIXED,
		 mem_fd, 
		 0xc0000
		 );
  
  if ((long)video_bios_mem < 0) {
    error("ERROR: mmap error in map_bios\n");
    return;
  }
  else g_printf("VIDEO BIOS address: 0x%x\n", video_bios_mem);

#if MAP_SYSTEM_BIOS

  system_bios_mem =
    (char *)mmap(
		 (caddr_t)0xf0000, 
		 64*1024,
		 PROT_READ|PROT_EXEC,
		 MAP_PRIVATE|MAP_FIXED,
		 mem_fd, 
		 0xf0000
		 );
  
  if ((long)system_bios_mem < 0) {
    error("ERROR: mmap error in map_bios\n");
    return;
  }
  else g_printf("SYSTEM BIOS address: 0x%x\n", system_bios_mem);
#endif
}


int 
vc_active(void)  /* return 1 if our VC is active */
{
  struct vt_stat vtstat;

  error("VC_ACTIVE!\n");
  ioctl(ioc_fd, VT_GETSTATE, &vtstat);
  error("VC_ACTIVE: ours: %d, active: %d\n", scr_state.console_no, vtstat.v_active);
  return ((vtstat.v_active == scr_state.console_no));
}


void 
set_vc_screen_page(int page)
{
  SCREEN=page;

  /* okay, if we have the current console, and video ram is mapped.
   * this has to be "atomic," or there is a "race condition": the
   * user may change consoles between our check and our remapping
   */
  forbid_switch();
  if (scr_state.mapped) /* should I check for foreground VC instead? */
    get_video_ram(WAIT);
  allow_switch();
}


hide_cursor()
{
  /* the Linux terminal driver hide_cursor string is
   * "\033[?25l" - put it in as the "vi" termcap in /etc/termcap
   */
  tputs(vi, 1, outc);
}

show_cursor()
{
  /* the Linux terminal driver show_cursor string is
   * "\033[?25h" - put it in as the "ve" termcap in /etc/termcap
   */
  tputs(ve, 1, outc);
}

void int10(void)
{
/* some code here is copied from Alan Cox ***************/
  int x, y, i, tmp;
  unsigned int s;
  static int gfx_flag=0;
  char tmpchar;
  char c, m;
  us *sm;

  if (d.video >= 3)
    dbug_printf("int10 AH=%02x AL=%02x\n", HI(ax), LO(ax));

  NOCARRY;
  switch(HI(ax))
    {
    case 0x0: /* define mode */
      v_printf("define mode: 0x%x\n", LO(ax));
      switch (LO(ax))
	{
	case 2:
	case 3:
	  v_printf("text mode from %d to %d\n", screen_mode, LO(ax));
	  max_page=7;
	  screen_mode = LO(ax);
	  gfx_flag=0;  /* we're in a text mode now */
	  gfx_mode = TEXT;
	  /* mode change clears screen unless bit7 of AL set */
	  if (! (LO(ax) & 0x80)) clear_screen(SCREEN,0);
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

  case 0x1: /* define cursor shape */
    v_printf("define cursor: 0x%x\n", _regs.ecx);
      /* 0x20 is the cursor no blink/off bit */
      /*	if (HI(cx) & 0x20) */
      if (_regs.ecx == 0x2000) 
	hide_cursor();
      else show_cursor();
    CARRY;
    break;

  case 0x2: /* set cursor pos */
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

  case 0x3: /* get cursor pos */
    s = HI(bx);
    if (s > 7) {
      error("ERROR: video error(0x3 s>7: %d)\n",s);
      CARRY;
      return;
    }
    _regs.edx = (YPOS(s) << 8) | XPOS(s);
    break;

  case 0x5: { /* change page */ 
    s = LO(ax);
    error("VID: change page from %d to %d!\n", SCREEN, s);
    if (s <= max_page) {
      if (!config.console_video)
	{
	  scrtest_bitmap = 1 << (24 + s);
	  vm86s.screen_bitmap = -1;
	  update_screen=1;
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

  case 0x6: /* scroll up */
    v_printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!config.console_video) vm86s.screen_bitmap = -1;
    break;

  case 0x7: /* scroll down */
    v_printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!config.console_video) vm86s.screen_bitmap = -1;
    break;
    
  case 0x8: /* read character at x,y + attr */ 
    s=HI(bx);
    if(s>max_page)
      {
	error("ERROR: read char from bad page %d\n", s);
	CARRY;
	break;
      }
    sm=SCREEN_ADR(s);
    _regs.eax=sm[CO*YPOS(s)+XPOS(s)];
    break;

      /* these two put literal character codes into memory, and do
       * not scroll or move the cursor...
       * the difference is that 0xA ignores color for text modes 
       */
    case 0x9:
    case 0xA: {
      u_short *sadr;
      u_char attr = LO(bx);

      s = HI(bx);
      sadr = SCREEN_ADR(s) + YPOS(s)*CO + XPOS(s);
      x = *(us *)&_regs.ecx;
      c = LO(ax);

      /* XXX - need to make sure this doesn't overrun video memory!
       * we don't really know how large video memory is yet (I haven't
       * abstracted things yet) so we'll just leave it for the
       * Great Video Housecleaning foretold in the TODO file of the
       * ancients
       */
      if (HI(ax) == 9)
	for (i=0; i<x; i++) *(sadr++) = c | (attr << 8);
      else
	for (i=0; i<x; i++)   /* leave attribute as it is */
	  { 
	    *sadr &= 0xff00;
	    *(sadr++) |= c;
	  }
      
      if (!config.console_video) update_screen=1;
      
      break;
    }

    case 0xe: /* print char */ 
      char_out(*(char *)&_regs.eax, SCREEN, ADVANCE);    /* char in AL */
      break;

    case 0x0f: /* get screen mode */
      _regs.eax = (CO << 8) | screen_mode;
      v_printf("get screen mode: 0x%04x s=%d\n", _regs.eax, SCREEN);
      HI(bx) = SCREEN;
      break;

#if 0
  case 0xb: /* palette */
  case 0xc: /* set dot */
  case 0xd: /* get dot */
    break;
#endif

  case 0x4: /* get light pen */
    error("ERROR: video error(no light pen)\n");
    CARRY;
    return;

  case 0x1a: /* get display combo */
    if (LO(ax) == 0)
      {
	v_printf("get display combo!\n");
	LO(ax) = 0x1a;          /* valid function=0x1a */
	LO(bx) = VID_COMBO;     /* active display */
	HI(bx) = 0;             /* no inactive display */
      }
    else {
      v_printf("set display combo not supported\n");
    }
    break;

  case 0x12: /* video subsystem config */
    v_printf("get video subsystem config ax=0x%04x bx=0x%04x\n",
	     _regs.eax, _regs.ebx);
    switch (LO(bx))
      {
      case 0x10:
	HI(bx)=VID_SUBSYS;
	/* this breaks qedit! (any but 0x10) */
	/* LO(bx)=3;  */
	v_printf("video subsystem 0x10 BX=0x%04x\n", _regs.ebx);
	_regs.ecx=0x0809;

	break;
      case 0x20:
	v_printf("select alternate printscreen\n");
	break;
      case 0x30:
	if (LO(ax) == 0) tmp=200;
	else if(LO(ax) == 1) tmp=350;
	else tmp=400;
	v_printf("select textmode scanlines: %d", tmp);
	LO(ax)=12;   /* ok */
	break;
      case 0x32:  /* enable/disable cpu access to cpu */
	if (LO(ax) == 0)
	  v_printf("disable cpu access to video!\n");
	else v_printf("enable cpu access to video!\n");
	break;
      default:
	error("ERROR: unrecognized video subsys config!!\n");
	show_regs();
      }
    break;

  case 0xfe: /* get shadow buffer..return unchanged */
  case 0xff: /* update shadow buffer...do nothing */
    break;

  case 0x10: /* ega palette */
  case 0x11: /* ega character generator */
  case 0x4f: /* vesa interrupt */

  default:
    error("new unknown video int 0x%x\n", _regs.eax);
    CARRY;
    break;
  }
}


int vga_screenoff()
{
  /* turn off screen for faster VGA memory acces */
  port_out(0x01, SEQ_I);     
  port_out(port_in(SEQ_D)|0x20, SEQ_D);   

  return 0;
}

int vga_screenon()
{
  /* turn screen back on */
  port_out(0x01, SEQ_I);     
  port_out(port_in(SEQ_D)&0xDF, SEQ_D);   

  return 0;
}


int vga_setpalvec(int start, int num, u_char *pal)
{
  int i, j;

  /* select palette register */
  port_out(start, PEL_IW);

  for(j = 0; j < num; j++) {
    for(i = 0; i < 10; i++) ;   /* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
    for(i = 0; i < 10; i++) ;   /* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
    for(i = 0; i < 10; i++) ;   /* delay (minimum 240ns) */
    port_out(*(pal++), PEL_D);
  }

  return j;
}


int vga_getpalvec(int start, int num, u_char *pal)
{
  int i, j;

  /* select palette register */
  port_out(start, PEL_IR);

  for(j = 0; j < num; j++) {
    for(i = 0; i < 10; i++) ;   /* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
    for(i = 0; i < 10; i++) ;   /* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
    for(i = 0; i < 10; i++) ;   /* delay (minimum 240ns) */
    *(pal++) = (char) port_in(PEL_D);
  }
}

/* Initialize dostoregs */
/* Should be a switch statement ... Later .... */
void initialize_dostoregs(void)
{
  int x;
  for(x=0;x<MAXDOSINTS-1;x++)dostoregs[x]=MAXMODES;
  dostoregs[0]=0;
  dostoregs[1]=1;
  dostoregs[2]=2;
  dostoregs[3]=3;
  dostoregs[4]=4;
  dostoregs[5]=4;
  dostoregs[6]=33;
  dostoregs[7]=6;
  dostoregs[0xd]=7;
  dostoregs[0xe]=8;
  dostoregs[0xf]=9;
  dostoregs[0x10]=10;
  dostoregs[0x11]=11;
  dostoregs[0x12]=12;
  dostoregs[0x13]=13;
  dostoregs[0x50]=14;
  dostoregs[0x51]=15;
  dostoregs[0x52]=16;
  dostoregs[0x53]=17;
  dostoregs[0x54]=18;
  dostoregs[0x55]=19;
  dostoregs[0x56]=20;
  dostoregs[0x57]=21;
  dostoregs[0x58]=22;
  dostoregs[0x59]=23;
  dostoregs[0x5a]=24;
  dostoregs[0x5b]=25;
  dostoregs[0x5c]=26;
  dostoregs[0x5d]=27;
  dostoregs[0x5e]=28;
  dostoregs[0x5f]=29;
  dostoregs[0x60]=30;
  dostoregs[0x61]=31;
  dostoregs[0x62]=32;
}

/* get ioperms to allow havoc to occur */
int get_perm()
{
  permissions = permissions + 1;
  if (permissions > 1) return;
  /* get I/O permissions for VGA registers */
  if (ioperm(0x3b0,0x3df-0x3b0+1,1)){
    v_printf("VGA: can't get I/O permissions \n");
    exit (-1);
  }

  /* color or monochrome text emulation? */
  color_text = port_in(MIS_R)&0x01;

  /* chose registers for color/monochrome emulation */
  if (color_text) {
    CRT_I = CRT_IC;
    CRT_D = CRT_DC;
    IS1_R = IS1_RC;
  } 
  else {
    CRT_I = CRT_IM;
    CRT_D = CRT_DM;
    IS1_R = IS1_RM;
  }
 v_printf("Permission allowed\n");
}

/* Stop io to card */
int release_perm()
{
  if (permissions > 0){
    permissions = permissions - 1;
    if (permissions > 0) return;
    /* release I/O permissions for VGA registers */
    if (ioperm(0x3b0,0x3df-0x3b0+1,0)){
      v_printf("VGA: can't release I/O permissions \n");
      leavedos (-1);
    }
  v_printf("Permission disallowed\n");
  } 
  else v_printf("Permissions already at 0\n");

}

int set_regs(u_char regs[])
{
  int i;
  int x;

  /* update misc output register */
  port_out(regs[MIS], MIS_W);         

  /* synchronous reset on */
  port_out(0x00,SEQ_I); 
  port_out(0x01,SEQ_D);         

  /* write sequencer registers */
  for (i = 1; i < SEQ_C; i++) {       
    port_out(i, SEQ_I); 
    port_out(regs[SEQ+i], SEQ_D); 
  }

  /* synchronous reset off */
  port_out(0x00, SEQ_I); 
  port_out(0x03, SEQ_D);         

  /* deprotect CRT registers 0-7 */
  port_out(0x11, CRT_I);    
  port_out(port_in(CRT_D)&0x7F, CRT_D);   

  /* write CRT registers */
  for (i = 0; i < CRT_C; i++) {       
    port_out(i, CRT_I); 
    port_out(regs[CRT+i], CRT_D); 
  }

  /* write graphics controller registers */
  for (i = 0; i < GRA_C; i++) {       
    port_out(i, GRA_I); 
    port_out(regs[GRA+i], GRA_D); 
  }

  /* write attribute controller registers */
  for (i = 0; i < ATT_C; i++) {       
    port_in(IS1_R);   /* reset flip-flop */
    port_out(i, ATT_IW);
    port_out(regs[ATT+i],ATT_IW);
  }

}

/* Prepare to do business with the EGA/VGA card */
inline void disable_vga_card(){
  get_perm();
  /* enable video */
  port_in(IS1_R); 
  port_out(0x00, ATT_IW); 
}

/* Finish doing business with the EGA/VGA card */
inline void enable_vga_card(){
  port_in(IS1_R); 
  port_out(0x20, ATT_IW); 
  /* disable video */
  release_perm();
}

/* Reset Attribute registers */
inline void reset_att(){
  port_in(IS1_R); 
}

/* Store current EGA/VGA regs */
int store_vga_regs(char regs[])
{
  int i,j;

  get_perm();

  /* save VGA registers */
  for (i = 0; i < CRT_C; i++) {
    port_out(i, CRT_I); 
    regs[CRT+i] = port_in(CRT_D); 
  }
  for (i = 0; i < ATT_C; i++) {
    port_in(IS1_R);
    port_out(i, ATT_IW); 
    regs[ATT+i] = port_in(ATT_R); 
  }
  for (i = 0; i < GRA_C; i++) {
    port_out(i, GRA_I); 
    regs[GRA+i] = port_in(GRA_D); 
  }
  for (i = 0; i < SEQ_C; i++) {
    port_out(i, SEQ_I); 
    regs[SEQ+i] = port_in(SEQ_D); 
  }
  regs[MIS] = port_in(MIS_R); 

  save_ext_regs(regs);

  release_perm();
  v_printf("Store regs complete!\n");
}

/* Store EGA/VGA display planes (4) */
void 
store_vga_mem(u_char *mem, u_char mem_size[], u_char banks){

  int p1,p2,p3,p4, bsize;
  u_char cbank;

  p1=mem_size[0]*1024;
  p2=mem_size[1]*1024;
  p3=mem_size[2]*1024;
  p4=mem_size[3]*1024;
  bsize=p1+p2+p3+p4;
  
  printf("mem=0x%08x,*mem=0x%08x\n",mem,*mem);
  printf("p1=0x%08x\n",p1);
  printf("p2=0x%08x\n",p2);
  printf("p3=0x%08x\n",p3);
  printf("p4=0x%08x\n",p4);
  printf("bsize=0x%08x\n",bsize);
  
  set_regs((char *)&vregs[dostoregs[0x12]][0]);
  
  for(cbank=0;cbank<banks;cbank++){
    set_bank_read(cbank);
    
    
    /* disable Set/Reset Register */
  /*   port_out(0x01, GRA_I ); 
    port_out(0x00, GRA_D );   */

    /* Store plane 0 */
    port_out(0x04, GRA_I ); 
    port_out(0x00, GRA_D );   
    v_printf("Starting mem copy\n");
    memcpy((caddr_t)mem+(bsize*cbank), (caddr_t)GRAPH_BASE, p1 );
    
    v_printf("After 1st copy\n");
    /* Store plane 1 */
    port_out(0x04, GRA_I ); 
    port_out(0x01, GRA_D );   
    memcpy((caddr_t)(mem+(bsize*cbank)+p1), (caddr_t)GRAPH_BASE, p2);
    
    v_printf("After 2nd copy\n");
    /* Store plane 2 */
    port_out(0x04, GRA_I ); 
    port_out(0x02, GRA_D );   
    memcpy((caddr_t)(mem+(bsize*cbank)+p1+p2), (caddr_t)GRAPH_BASE, p3);
    
    v_printf("After 3rd copy\n");
    /* Store plane 3 */
    port_out(0x04, GRA_I ); 
    port_out(0x03, GRA_D );   
    memcpy((caddr_t)(mem+(bsize*cbank)+p1+p2+p3), (caddr_t)GRAPH_BASE, p4);
  }
  v_printf("GRAPH_BASE to mem complete!\n");
}

/* Restore EGA/VGA display planes (4) */
void restore_vga_mem(u_char *mem, u_char mem_size[], u_char banks)
{
  int p1,p2,p3,p4, bsize;
  u_char cbank;
 
  p1=mem_size[0]*1024;
  p2=mem_size[1]*1024;
  p3=mem_size[2]*1024;
  p4=mem_size[3]*1024;
  bsize=p1+p2+p3+p4;
 
  v_printf("p1=0x%08x\n",p1);
  v_printf("p2=0x%08x\n",p2);
  v_printf("p3=0x%08x\n",p3);
  v_printf("p4=0x%08x\n",p4);
  v_printf("bsize=0x%08x\n",bsize);

  set_regs((char *)&vregs[dostoregs[0x12]][0]);

  for(cbank=0;cbank<banks;cbank++){
  set_bank_write(cbank);

  /* disable Set/Reset Register */
  port_out(0x01, GRA_I ); 
  port_out(0x00, GRA_D );   

  /* Store plane 0 */
  port_out(0x02, SEQ_I ); 
  port_out(0x01, SEQ_D );   
  memcpy((caddr_t)GRAPH_BASE, (caddr_t)(mem+(bsize*cbank)), p1);

  /* Store plane 1 */
  port_out(0x02, SEQ_I ); 
  port_out(0x02, SEQ_D );   
  memcpy((caddr_t)GRAPH_BASE, (caddr_t)(mem+(bsize*cbank)+p1), p2);

  /* Store plane 2 */
  port_out(0x02, SEQ_I ); 
  port_out(0x04, SEQ_D );   
  memcpy((caddr_t)GRAPH_BASE, (caddr_t)(mem+(bsize*cbank)+p1+p2), p3);

  /* Store plane 3 */
  port_out(0x02, SEQ_I ); 
  port_out(0x08, SEQ_D );   
  memcpy((caddr_t)GRAPH_BASE, (caddr_t)(mem+(bsize*cbank)+p1+p2+p3), p4);
 }
 v_printf("mem to GRAPH_BASE complete!\n");
}

/* Restore EGA/VGA regs */
int restore_vga_regs(char regs[])
{
  v_printf("Restore Regs called\n");
  set_regs(regs);
  restore_ext_regs(regs); 
  v_printf("Restore completed!\n");
}

/* Save all necessary info to allow switching vt's */
void save_vga_state(struct video_save_struct *save_regs){

  disable_vga_card();
  vga_screenoff();
  store_vga_regs(save_regs->regs);
 switch(save_regs->video_mode){
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
   save_regs->save_mem_size[0]=8;
   save_regs->save_mem_size[1]=8;
   save_regs->save_mem_size[2]=8;
   save_regs->save_mem_size[3]=8;
   save_regs->banks=1;
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
   save_regs->save_mem_size[0]=32;
   save_regs->save_mem_size[1]=32;
   save_regs->save_mem_size[2]=32;
   save_regs->save_mem_size[3]=32;
   save_regs->banks=1;
   break;
  default:
   save_regs->save_mem_size[0]=64;
   save_regs->save_mem_size[1]=64;
   save_regs->save_mem_size[2]=64;
   save_regs->save_mem_size[3]=64;
   save_regs->banks=
    (config.gfxmemsize + 255) / 256;
   break;
 }
v_printf("Mode  == %d\n",save_regs->video_mode);
v_printf("Banks == %d\n",save_regs->banks);
  if (!save_regs->mem) 
    save_regs->mem=malloc((save_regs->save_mem_size[0] +
			   save_regs->save_mem_size[1] +
			   save_regs->save_mem_size[2] +
			   save_regs->save_mem_size[3]) * 1024 *
			  save_regs->banks
			  );

  store_vga_mem(save_regs->mem, save_regs->save_mem_size, save_regs->banks);
  vga_getpalvec(0,256,save_regs->pal);
  restore_vga_regs(save_regs->regs);
  vga_screenon();
  enable_vga_card();

  v_printf("Store_vga_state complete\n");
}

/* Restore all necessary info to allow switching back to vt */
void restore_vga_state(struct video_save_struct *save_regs){

  disable_vga_card();
  vga_screenoff();
  restore_ext_regs(save_regs->regs);
  restore_vga_mem(save_regs->mem, save_regs->save_mem_size, save_regs->banks);
  if(save_regs->release_video){
    free(save_regs->mem);
    save_regs->mem=NULL;
  }
  vga_setpalvec(0,256,save_regs->pal);
  restore_vga_regs(save_regs->regs);
  v_printf("Permissions=%d\n",permissions);
  vga_screenon();
  enable_vga_card();

  v_printf("Restore_vga_state complete\n");
}

 /* These are dummy calls */
void save_ext_regs_dummy(regs){
 return;
}
void restore_ext_regs_dummy(regs){
  return;
}
void set_bank_read_dummy(u_char bank){
 return;
}
void set_bank_write_dummy(u_char bank){
 return;
}

void trident_set_old_regs(){
 u_char dummy;
  port_out(0x0b, SEQ_I ); 
  port_out(0x00, SEQ_D );   
  return;
}

void trident_set_new_regs(){
u_char dummy;
  port_out(0x0b, SEQ_I ); 
  port_out(0x00, SEQ_D );   
  dummy=port_in(SEQ_D );   
  return;
}

void trident_allow_svga(){
 u_char dummy;
 trident_set_old_regs();
 port_out(0x0d, SEQ_I ); 
 dummy=port_in(SEQ_D );   
 port_out(dummy|0x20, SEQ_D );   
 trident_set_new_regs();
 printf("Allow SVGA Modes = 0x%02x\n",dummy|0x10);
 return;
}

void trident_disallow_svga(){
 u_char dummy;
 trident_set_old_regs();
 port_out(0x0d, SEQ_I ); 
 dummy=port_in(SEQ_D );   
 port_out(dummy&~0x20, SEQ_D );   
 trident_set_new_regs();
 printf("Allow SVGA Modes = 0x%02x\n",dummy&~0x10);
 return;
}

u_char trident_get_svga()
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I ); 
  dummy=port_in(SEQ_D) & 0x20;
  trident_set_new_regs();
  printf("Trident get SVGA, dummy = 0x%02x\n",dummy);
  return(!dummy);   
}

/* These set banks only work on 256k boundaries */
void 
trident_set_bank_read(u_char bank)
{
  if (trident_get_svga()){ 
    trident_allow_svga();
    port_out(0x0e, SEQ_I ); 
    port_out(bank<<2, SEQ_D );   
  }
  printf("Set bank read = 0x%02x\n", bank<<2);
  return;
}

void 
trident_set_bank_write(u_char bank)
{
  if (trident_get_svga()){
    trident_allow_svga();
    port_out(0x0e, SEQ_I ); 
    port_out((bank<<2)^0x02, SEQ_D );   
  }
  printf("Set bank write= 0x%02x\n", bank<<2^0x02);
  return;
}

void 
trident_save_ext_regs(u_char regs[])
{
  trident_set_old_regs();
  port_out(0x0d, SEQ_I ); 
  regs[60]=port_in(SEQ_D ) & 0xff;   
  port_out(0x0e, SEQ_I ); 
  regs[61]=port_in(SEQ_D ) & 0xff;   
  trident_set_new_regs();
  port_out(0x0d, SEQ_I ); 
  regs[62]=port_in(SEQ_D ) & 0xff;   
  port_out(0x0e, SEQ_I ); 
  regs[63]=port_in(SEQ_D ) & 0xff;   
  port_out(0x0f, SEQ_I );
  regs[64]=port_in(SEQ_D ) & 0xff;
  port_out(0x1e, CRT_I ); 
  regs[65]=port_in(CRT_D ) & 0xff;   
  port_out(0x1f, CRT_I ); 
  regs[66]=port_in(CRT_D ) & 0xff;   
  v_printf("Save EXT trid REGS = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
  regs[60],regs[61], regs[62], regs[63], regs[64], regs[65], regs[66]);
  return;
}

void 
trident_restore_ext_regs(u_char regs[])
{
 u_char dummy;

 trident_set_old_regs();
 port_out(0x0d, SEQ_I ); 
 port_out(regs[60], SEQ_D );   
 port_out(0x0e, SEQ_I );
 port_out(regs[61]^0x02, SEQ_D );   
 trident_set_new_regs();
 port_out(0x0d, SEQ_I ); 
 port_out(regs[62], SEQ_D );   
 port_out(0x0e, SEQ_I ); 
 port_out(regs[63]^0x02, SEQ_D );   
 port_out(0x0f, SEQ_I ); 
 port_out(regs[64], SEQ_D );   
 port_out(0x1e, CRT_I ); 
 port_out(regs[65], CRT_D );
 port_out(0x1f, CRT_I ); 
 port_out(regs[66], CRT_D );
 v_printf("Restore EXT trid REGS\n");
 return;
}

/* These are trident Specific calls to get at banks */
vga_init_trident(){
 save_ext_regs=trident_save_ext_regs;
 restore_ext_regs=trident_restore_ext_regs;
 set_bank_read=trident_set_bank_read;
 set_bank_write=trident_set_bank_write;
 return;
}


/* THese are et4000 specific functions */
void 
et4000_save_ext_regs(u_char regs[])
{
 regs[60]=port_in(0x3cd) & 0xff;   
 port_out(0x24, CRT_I ); 
 regs[61]=port_in(CRT_D) & 0xff;   
 port_out(0x32, CRT_I ); 
 regs[62]=port_in(CRT_D) & 0xff;   
 port_out(0x33, CRT_I ); 
 regs[63]=port_in(CRT_D) & 0xff;   
 port_out(0x35, CRT_I ); 
 regs[64]=port_in( CRT_D) & 0xff;   
 port_out(0x36, CRT_I ); 
 regs[65]=port_in( CRT_D) & 0xff;   
 port_out(0x37, CRT_I ); 
 regs[66]=port_in( CRT_D) & 0xff;
 port_out(0x3f, CRT_I ); 
 regs[67]=port_in( CRT_D) & 0xff;
 port_out(0x7, SEQ_I ); 
 regs[68]=port_in( SEQ_D) & 0xff;   
 port_in( IS1_R);   
 port_out(0x16, ATT_IW ); 
 regs[69]=port_in( ATT_R) & 0xff;
 regs[70]=port_in( 0x3cb) & 0xff;
 regs[71]=port_in( 0x3d8) & 0xff;
 regs[72]=port_in( 0x3bf) & 0xff;
 port_out(0xd, GRA_I ); 
 regs[73]=port_in( GRA_D) & 0xff;
 port_out(0xe, GRA_I ); 
 regs[74]=port_in( GRA_D) & 0xff;
}

void 
et4000_restore_ext_regs(u_char regs[])
{
 port_out(regs[60], 0x3cd ); 
 port_out(0x24, CRT_I ); 
 port_out(regs[61], CRT_D ); 
 port_out(0x32, CRT_I ); 
 port_out(regs[62], CRT_D ); 
 port_out(0x33, CRT_I ); 
 port_out(regs[63], CRT_D ); 
 port_out(0x35, CRT_I ); 
 port_out(regs[64], CRT_D ); 
 port_out(0x36, CRT_I ); 
 port_out(regs[65], CRT_D ); 
 port_out(0x37, CRT_I ); 
 port_out(regs[66], CRT_D );
 port_out(0x3f, CRT_I ); 
 port_out(regs[67], CRT_D );
 port_out(0x7, SEQ_I ); 
 port_out(regs[68], SEQ_D ); 
 port_in( IS1_R);   
 port_out(0x16, ATT_IW ); 
 port_out(regs[69], ATT_IW );
 port_out(regs[70], 0x3cb);
 port_out(regs[71], 0x3d8);
 port_out(regs[72], 0x3bf);
 port_out(0xd, GRA_I ); 
 port_out(regs[73], GRA_D);
 port_out(0xe, GRA_I ); 
 port_out(regs[74], GRA_D);
}

void 
et4000_set_bank_read(u_char bank)
{
  u_char dummy;
  dummy=port_in(0x3cd) & 0x0f;   
  port_out(dummy | (bank << 6), 0x3cd ); 
}

void 
et4000_set_bank_write(u_char bank)
{
 u_char dummy;
 dummy=port_in(0x3cd) & 0xf0;   
 port_out(dummy | (bank << 2), 0x3cd ); 
}

vga_init_et4000()
{
 save_ext_regs    = et4000_save_ext_regs;
 restore_ext_regs = et4000_restore_ext_regs;
 set_bank_read    = et4000_set_bank_read;
 set_bank_write   = et4000_set_bank_write;
 return;
}

void 
vga_initialize(void)
{
  linux_regs.mem=NULL;
  dosemu_regs.mem=NULL;
  get_perm();

  initialize_dostoregs();

  switch(config.chipset) 
    {
    case TRIDENT:
      vga_init_trident();
      v_printf("Trident CARD in use\n");
      break;
    case ET4000:
      vga_init_et4000();
      v_printf("ET4000 CARD in use\n");
      break;
    default:
      save_ext_regs=save_ext_regs_dummy;
      restore_ext_regs=restore_ext_regs_dummy;
      set_bank_read=set_bank_read_dummy;
      set_bank_write=set_bank_write_dummy;
      v_printf("Unspecific VIDEO selected = 0x%04x\n", config.chipset);
    }

  linux_regs.save_mem_size[0] = 8;
  linux_regs.save_mem_size[1] = 8;
  linux_regs.save_mem_size[2] = 8;
  linux_regs.save_mem_size[3] = 8;
  linux_regs.banks=1;
  linux_regs.video_mode=3;
  linux_regs.release_video=0;

  dosemu_regs.save_mem_size[0] = 64;
  dosemu_regs.save_mem_size[1] = 64;
  dosemu_regs.save_mem_size[2] = 64;
  dosemu_regs.save_mem_size[3] = 64;
  dosemu_regs.banks = (config.gfxmemsize + 255) / 256;
  dosemu_regs.video_mode=3;
/*  dosemu_regs.release_video=1; */

  /* don't release it; we're trying text mode restoration */
  dosemu_regs.release_video=0;

  v_printf("VGA: mem size %d, banks %d\n", config.gfxmemsize,
	   dosemu_regs.banks);
  
  save_vga_state(&linux_regs);
  video_initialized=1;
  get_perm();
}
#undef VIDEO_C



