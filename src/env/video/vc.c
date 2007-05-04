/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Here's all the calls to the code to try and properly save & restore
 * the video state between VC's and the attempts to control updates to
 * the VC whilst the user is using another. We map between the real
 * screen address and that used by DOSEMU here too.
 *
 * Attempts to use a cards own bios require the addition of the parameter
 * "graphics" to the video statement in "/etc/dosemu.conf". This will make
 * the emulator try to execute the card's initialization routine which is
 * normally located at address c000:0003. This can now be changed as an
 * option.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * vc.c - for the Linux DOS emulator
 *  Robert Sanders, gt8134b@prism.gatech.edu
 *
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>			/* needed for mouse.h */
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/vt.h>
#include <sys/kd.h>
#endif

#define INIT_C2TRAP
#define NO_VC_NO_DEBUG 0

#include "emu.h"
#include "memory.h"
#include "termio.h"
#include "mouse.h"
#include "machcompat.h"
#include "bios.h"
#include "port.h"

#include "video.h"
#include "vc.h"
#include "vga.h"
#include "s3.h"
#include "trident.h"
#include "et4000.h"
#include "priv.h"
#include "mapping.h"
#include "timers.h"
#include "vgaemu.h"
#include "dpmi.h"

static void set_dos_video (void);

static void SIGRELEASE_call (void);
static void SIGACQUIRE_call (void);

static int  color_text;

#define MAXDOSINTS 032
#define MAXMODES 34


static inline void
forbid_switch (void)
{
  scr_state.vt_allow = 0;
}

/* check whether we are running on the console; initialise
 * console_fd and scr_state.console_no
 */
int on_console(void) {


#ifdef __linux__
    struct stat chkbuf;
    int major, minor;

    if (console_fd == -2 || config.X)
	return 0;

    console_fd = -2;

    if (fstat(STDIN_FILENO, &chkbuf) != 0)
	return 0;

    major = chkbuf.st_rdev >> 8;
    minor = chkbuf.st_rdev & 0xff;

    c_printf("major = %d minor = %d\n",
	    major, minor);
    /* console major num is 4, minor 64 is the first serial line */
    if (S_ISCHR(chkbuf.st_mode) && (major == 4) && (minor < 64)) {
       scr_state.console_no = minor;
       console_fd = STDIN_FILENO;
       return 1;
    }
#endif
    return 0;
}

void
vt_activate(int con_num)
{
    ioctl(console_fd, VT_ACTIVATE, con_num);
}

void
allow_switch (void)
{
  scr_state.vt_allow = 1;
  if (scr_state.vt_requested)
    {
      v_printf ("VID: clearing old vt request\n");
      SIGRELEASE_call ();
    }
}

static inline void
SIGACQUIRE_call (void)
{
  get_video_ram (WAIT);
  set_dos_video ();
  /*      if (config.vga) dos_unpause(); */
  unfreeze_dosemu();
  unfreeze_mouse();
}

int dos_has_vt = 1;

static void
acquire_vt (struct sigcontext_struct *scp)
{
  dos_has_vt = 1;

#if NO_VC_NO_DEBUG
  if (user_vc_switch) shut_debug = 0;
#endif
  flush_log();

  v_printf ("VID: Acquiring VC\n");
  forbid_switch ();
  if (ioctl (console_fd, VT_RELDISP, VT_ACKACQ))	/* switch acknowledged */
    v_printf ("VT_RELDISP failed (or was queued)!\n");
  allow_switch ();
  SIGNAL_save (SIGACQUIRE_call);
  scr_state.current = 1;
}

static void
set_dos_video (void)
{
  if (!config.vga)
    return;

  /* jesx */
  /* After all that fun up there, get permissions and save/restore states */
  if (video_initialized)
    {
      v_printf ("Acquiring vt, restoring dosemu_regs\n");
      get_perm ();
      restore_vga_state (&dosemu_regs);
    }

}

void
set_linux_video (void)
{
  if (!config.vga)
    return;

  /* jesx */
  /* After all that fun  rel perms and save/restore states */
  if (video_initialized)
    {
      v_printf ("Storing dosemu_regs, Releasing vt mode=%02x\n", *(u_char *) 0x449);
      dosemu_regs.video_mode = *(u_char *) 0x449;
      save_vga_state (&dosemu_regs);
      if (linux_regs.mem != NULL)
	{
	  v_printf ("Restoring linux_regs, Releasing vt\n");
	  restore_vga_state (&linux_regs);
	}
    }
}

static void
SIGRELEASE_call (void)
{
  if (scr_state.current == 1)
    {
      v_printf ("VID: Releasing VC\n");
      freeze_mouse();
      if (!scr_state.vt_allow)
	{
	  v_printf ("disallowed vt switch!\n");
#if 0
	  scr_state.vt_requested = 1;
#endif
	  v_printf ("Released_vt finished!\n");
	  return;
	}

      set_linux_video ();
      if (can_do_root_stuff) 
	release_perm ();
      put_video_ram ();

      /*      if (config.vga) dos_pause(); */
      scr_state.current = 0;
      /* NOTE: if DOSEMU is not frozen then a DOS program in the background
	 is capable of changing the screen appearance, even while in another
	 console or X */
      freeze_dosemu();
    }

  scr_state.current = 0;	/* our console is no longer current */
  if (ioctl (console_fd, VT_RELDISP, 1))	/* switch ok by me */
    v_printf ("VT_RELDISP failed!\n");
  else {
   /* we don't want any debug activity while VC is not active */
    flush_log();
#if NO_VC_NO_DEBUG
    if (user_vc_switch) shut_debug = 1;
#endif
  }
}

int
wait_vc_active (void)
{
  if (ioctl (console_fd, VT_WAITACTIVE, scr_state.console_no) < 0)
    {
      if (errno != EINTR)
		{
			error("VT_WAITACTIVE for %d gave %d: %s\n", scr_state.console_no,
			     errno, strerror (errno));
	    }
      return -1;
    }
  else
    return 0;
}

static void wait_for_active_vc(void)
{
#if 0
  config.console_video = 0;
#endif
  v_printf ("VID: get_video_ram WAITING\n");
  /* XXX - wait until our console is current (mixed signal functions) */
  do {
    if (!wait_vc_active ())
      break;
    v_printf ("Keeps waiting...And\n");
  } while (errno == EINTR);
}

static void release_vt (struct sigcontext_struct *scp)
{
  dos_has_vt = 0;

  SIGNAL_save (SIGRELEASE_call);
}

static void unmap_video_ram(int copyback)
{
  char *base = (char *)GRAPH_BASE;
  size_t size = GRAPH_SIZE;
  int cap = MAPPING_VC | MAPPING_LOWMEM;

  if (!config.vga) {
    size = console_size();
    base = scr_state.virt_address;
  }
  if (copyback) cap |= MAPPING_COPYBACK;
  if (mmap_mapping(cap, base, size, PROT_READ | PROT_WRITE | PROT_EXEC, (off_t)base) != MAP_FAILED)
    scr_state.mapped = 0;
}

static void map_video_ram(void)
{ 
  void *graph_mem;
  off_t pbase = GRAPH_BASE;
  char *vbase = (char *)pbase;
  size_t ssize = GRAPH_SIZE;
  int cap = MAPPING_VC | MAPPING_KMEM;

  if (!config.vga) {
    pbase = phys_text_base;         /* physical page address    */
    vbase = scr_state.virt_address; /* new virtual page address */
    ssize = console_size();
    /* this is used for page switching */
    cap |= MAPPING_COPYBACK;
  }

  g_printf ("mapping %s\n", config.vga ? "GRAPH_BASE" : "PAGE_ADDR");

  graph_mem = mmap_mapping(cap, vbase, ssize, PROT_READ | PROT_WRITE | PROT_EXEC, pbase);

  /* the code below is done by the video save/restore code for config.vga */
  if (!config.vga) {
    if (graph_mem == MAP_FAILED) {
      if (!can_do_root_stuff && mem_fd == -1) return;
      error("mmap error in get_video_ram (text): %p, errno %d\n",
	    graph_mem, errno);
      return;
    } else
      v_printf ("CONSOLE VIDEO address: %p %#lx %p\n", graph_mem,
		pbase, vbase);
  }
  scr_state.phys_address = pbase;
  scr_state.mapped = 1;
}

void init_get_video_ram(int waitflag)
{
  size_t size = GRAPH_SIZE;
  off_t base = GRAPH_BASE;
  if (!config.vga) {
    size = console_size();
    base = phys_text_base;
  }
  if (waitflag == WAIT)
    wait_for_active_vc();
  scr_state.pageno = 0;
  scr_state.virt_address = virt_text_base;
  scr_state.phys_address = base;
  scr_state.mapped = 1;
  /* mapping is done later in map_hardware_ram */
}

/* allows remapping even if memory is mapped in...this is useful, as it
 * remembers where the video mem *was* mapped, unmaps from that, and then
 * remaps it to where the text page number says it should be
 */
void get_video_ram (int waitflag)
{
  int page;

  v_printf ("get_video_ram STARTED\n");
  if (waitflag == WAIT)
    wait_for_active_vc();

  if (!scr_state.mapped && config.vga
      && READ_BYTE(BIOS_VIDEO_MODE) == 3) {
    if (dosemu_regs.mem)
      memcpy (dosemu_regs.mem, (void *)virt_text_base, 32768);
    /* else error("ERROR: no dosemu_regs.mem!\n"); */
  }

  if (scr_state.mapped)
    unmap_video_ram(1);

  page = vga.display_start / PAGE_SIZE;
  scr_state.pageno = page;
  if (!config.vga)
    scr_state.virt_address = (void *)(virt_text_base + (page * PAGE_SIZE));
  map_video_ram();
}

void put_video_ram (void)
{
  if (scr_state.mapped) {
    v_printf ("put_video_ram called\n");
    unmap_video_ram(!config.vga);
    if (!scr_state.mapped && config.vga) {
      if (dosemu_regs.mem && READ_BYTE(BIOS_VIDEO_MODE) == 3)
	memcpy ((void*)virt_text_base, dosemu_regs.mem, 32768);
    }
  }
  else
    warn ("VID: put_video-ram but not mapped!\n");

  v_printf ("put_video_ram completed\n");
}

/* this puts the VC under process control */
void
set_process_control (void)
{
  struct vt_mode vt_mode;
  sigset_t set;

  vt_mode.mode = VT_PROCESS;
  vt_mode.waitv = 0;
  vt_mode.relsig = SIG_RELEASE;
  vt_mode.acqsig = SIG_ACQUIRE;
  vt_mode.frsig = 0;

  scr_state.vt_requested = 0;	/* a switch has not been attempted yet */
  allow_switch ();

  registersig (SIG_RELEASE, release_vt);
  registersig (SIG_ACQUIRE, acquire_vt);

  sigemptyset(&set);
  sigaddset(&set, SIG_RELEASE);
  sigaddset(&set, SIG_ACQUIRE);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  if (ioctl (console_fd, VT_SETMODE, &vt_mode))
    v_printf ("initial VT_SETMODE failed!\n");
  v_printf ("VID: Set process control\n");
}

void
clear_process_control (void)
{
  struct vt_mode vt_mode;

  vt_mode.mode = VT_AUTO;
  ioctl (console_fd, VT_SETMODE, &vt_mode);
  registersig (SIG_RELEASE, NULL);
  registersig (SIG_ACQUIRE, NULL);
}


/* why count ??? */
/* Because you do not want to open it more than once! */
static u_char kmem_open_count = 0;

void
open_kmem (void)
{
  PRIV_SAVE_AREA
  /* as I understad it, /dev/kmem is the kernel's view of memory,
     * and /dev/mem is the identity-mapped (i.e. physical addressed)
     * memory. Currently under Linux, both are the same.
     */

  kmem_open_count++;

  if (mem_fd != -1)
    return;
  enter_priv_on();
  mem_fd = open("/dev/mem", O_RDWR);
  leave_priv_setting();
  if (mem_fd < 0)
    {
      error("can't open /dev/mem: errno=%d, %s \n",
	     errno, strerror (errno));
      leavedos (0);
      return;
    }
  g_printf ("Kmem opened successfully\n");
}

void
close_kmem (void)
{

  if (kmem_open_count)
    {
      kmem_open_count--;
      if (kmem_open_count)
	return;
      close (mem_fd);
      mem_fd = -1;
      v_printf ("Kmem closed successfully\n");
    }
}

int
vc_active (void)
{				/* return 1 if our VC is active */
#ifdef __linux__
  struct vt_stat vtstat;

  g_printf ("VC_ACTIVE!\n");
  ioctl (console_fd, VT_GETSTATE, &vtstat);
  g_printf ("VC_ACTIVE: ours: %d, active: %d\n", scr_state.console_no, vtstat.v_active);
  return ((vtstat.v_active == scr_state.console_no));
#endif

}

void set_vc_screen_page (void)
{
  /* okay, if we have the current console, and video ram is mapped.
   * this has to be "atomic," or there is a "race condition": the
   * user may change consoles between our check and our remapping
   */
  forbid_switch ();
  if (scr_state.mapped)		/* should I check for foreground VC instead? */
    get_video_ram (WAIT);
  allow_switch ();
}


/* get ioperms to allow havoc to occur */
int
get_perm (void)
{
  if (permissions > 0)
    {
      return 0;
    }
  permissions++;
  if (config.vga)
    {				/* hope this will not lead to problems with ega/cga */
      /* get I/O permissions for VGA registers */
      if (set_ioperm (0x3b0, 0x3df - 0x3b0 + 1, 1))
	{
	  v_printf ("VGA: can't get I/O permissions \n");
	  exit (-1);
	}
      if ((config.chipset == S3) &&
	(set_ioperm(0x102, 1, 1) || set_ioperm(0x2ea, 4, 1))) {
	  v_printf("S3: can't get I/O permissions");
	  exit (-1);
	}
      if ((config.chipset == CIRRUS) &&
	(set_ioperm(0x102, 2, 1) || set_ioperm(0x2ea, 4, 1))) {
	  v_printf("CIRRUS: can't get I/O permissions");
	  exit (-1);
	}
      if ((config.chipset == ATI) &&
	(set_ioperm(0x102, 1, 1) || set_ioperm(0x1ce, 2, 1) || set_ioperm(0x2ec, 4, 1))) {
	  v_printf("ATI: can't get I/O permissions");
	  exit (-1);
	}
      if ((config.chipset == MATROX) &&
	(set_ioperm(0x102, 2, 1) || set_ioperm(0x2ea, 4, 1) || set_ioperm(0x3de, 2, 1))) {
	  v_printf("MATROX: can't get I/O permissions");
	  exit (-1);
	}
      if ((config.chipset == WDVGA) &&
	(set_ioperm(0x102, 2, 1) || set_ioperm(0x3de, 2, 1))) {
	  v_printf("WDVGA: can't get I/O permissions");
	  exit (-1);
	}
      /* color or monochrome text emulation? */
      color_text = port_in (MIS_R) & 0x01;

      /* chose registers for color/monochrome emulation */
      if (color_text)
	{
	  CRT_I = CRT_IC;
	  CRT_D = CRT_DC;
	  IS1_R = IS1_RC;
	  FCR_W = FCR_WC;
	}
      else
	{
	  CRT_I = CRT_IM;
	  CRT_D = CRT_DM;
	  IS1_R = IS1_RM;
	  FCR_W = FCR_WM;
	}
    }
  else if (config.console_video && (config.cardtype == CARD_MDA))
    {
      if (set_ioperm (0x3b4, 1, 1) ||
	  set_ioperm (0x3b5, 1, 1) ||
	  set_ioperm (0x3b8, 1, 1) ||
	  set_ioperm (0x3ba, 1, 1) ||
	  set_ioperm (0x3bf, 1, 1))
	{
	  v_printf ("HGC: can't get I/O permissions \n");
	  exit (-1);
	}
    }
  v_printf ("Permission allowed\n");
  return 0;
}

/* Stop io to card */
int
release_perm (void)
{
  if (permissions > 0)
    {
      permissions--;
      if (permissions > 0)
	{
	  return 0;
	}
      if (config.vga)
	{			/* hope this will not lead to problems with ega/cga */
	  /* get I/O permissions for VGA registers */
	  /* release I/O permissions for VGA registers */
	  if (set_ioperm (0x3b0, 0x3df - 0x3b0 + 1, 0))
	    {
	      v_printf ("VGA: can't release I/O permissions \n");
	      leavedos (-1);
	    }
	  if ((config.chipset == S3) &&
		(set_ioperm(0x102, 1, 0) || set_ioperm(0x2ea, 4, 0))) {
	      v_printf ("S3: can't release I/O permissions\n");
	      leavedos (-1);
	    }
	  if ((config.chipset == CIRRUS) &&
		(set_ioperm(0x102, 2, 0) || set_ioperm(0x2ea, 4, 0))) {
	      v_printf ("CIRRUS: can't release I/O permissions\n");
	      leavedos (-1);
	    }
	  if ((config.chipset == ATI) &&
		(set_ioperm(0x102, 1, 0) || set_ioperm(0x1ce, 2, 0) || set_ioperm(0x2ec, 4, 0))) {
	      v_printf ("ATI: can't release I/O permissions\n");
	      leavedos (-1);
	    }
	  if ((config.chipset == MATROX) &&
		(set_ioperm(0x102, 2, 0) || set_ioperm(0x2ea, 4, 0) || set_ioperm(0x3de, 2, 0))) {
	      v_printf ("MATROX: can't release I/O permissions\n");
	      leavedos (-1);
	    }
	  if ((config.chipset == WDVGA) &&
		(set_ioperm(0x102, 2, 0) || set_ioperm(0x3de, 2, 0))) {
	      v_printf ("WDVGA: can't release I/O permissions\n");
	      leavedos (-1);
	    }
	}
      else if (config.console_video && (config.cardtype == CARD_MDA))
	{
	  if (set_ioperm (0x3b4, 1, 0) ||
	      set_ioperm (0x3b5, 1, 0) ||
	      set_ioperm (0x3b8, 1, 0) ||
	      set_ioperm (0x3ba, 1, 0) ||
	      set_ioperm (0x3bf, 1, 0))
	    {
	      v_printf ("HGC: can't release I/O permissions \n");
	      exit (-1);
	    }
	}
      v_printf ("Permission disallowed\n");
    }
  else
    v_printf ("Permissions already at 0\n");
  return 0;

}

int
set_regs (u_char regs[], int seq_gfx_only)
{
  int i;

  emu_video_retrace_off();
  /* port_out(dosemu_regs.regs[FCR], FCR_W); */
  /* update misc output register */
  if (!seq_gfx_only)
    port_out (regs[MIS], MIS_W);

  /* write sequencer registers */
  /* synchronous reset on */
  port_out (0x00, SEQ_I);
  port_out (0x01, SEQ_D);
  port_out (0x01, SEQ_I);
  port_out (regs[SEQ + 1] | 0x20, SEQ_D);
  /* synchronous reset off */
  port_out (0x00, SEQ_I);
  port_out (0x03, SEQ_D);
  for (i = 2; i < SEQ_C; i++)
    {
      port_out (i, SEQ_I);
      port_out (regs[SEQ + i], SEQ_D);
    }

  if (!seq_gfx_only) {
    /* deprotect CRT registers 0-7 */
    port_out (0x11, CRT_I);
    port_out (port_in (CRT_D) & 0x7F, CRT_D);

    /* write CRT registers */
    for (i = 0; i < CRT_C; i++)
      {
	port_out (i, CRT_I);
	port_out (regs[CRT + i], CRT_D);
      }
  }

  /* write graphics controller registers */
  for (i = 0; i < GRA_C; i++)
    {
      port_out (i, GRA_I);
      port_out (regs[GRA + i], GRA_D);
    }

  /* write attribute controller registers */
  if (!seq_gfx_only) for (i = 0; i < ATT_C; i++)
    {
      port_in (IS1_R);		/* reset flip-flop */
      port_out (i, ATT_IW);
      port_out (regs[ATT + i], ATT_IW);
    }

  /* port_out(dosemu_regs.regs[CRTI], CRT_I);
port_out(dosemu_regs.regs[GRAI], GRA_I);
port_out(dosemu_regs.regs[SEQI], SEQ_I); */
  /* v_printf("CRTI=0x%02x\n",dosemu_regs.regs[CRTI]);
v_printf("GRAI=0x%02x\n",dosemu_regs.regs[GRAI]);
v_printf("SEQI=0x%02x\n",dosemu_regs.regs[SEQI]); */
  emu_video_retrace_on();
  return (0);
}


/* Reset Attribute registers */
static inline void
reset_att (void)
{
  emu_video_retrace_off();
  port_in (IS1_R);
  emu_video_retrace_on();
}


/* Attempt to virtualize calls to video ports */

u_char att_d_index = 0;
static int isr_read = 0;

u_char
video_port_in (ioport_t port)
{
  if (permissions)
    return port_real_inb (port);
  /* v_printf("Video read on port 0x%04x.\n",port); */
  switch (port)
    {
    case CRT_IC:
    case CRT_IM:
      v_printf ("Read Index CRTC 0x%02x\n", dosemu_regs.regs[CRTI]);
      return (dosemu_regs.regs[CRTI]);
    case CRT_DC:
    case CRT_DM:
      if (dosemu_regs.regs[CRTI] < CRT_C)
	{
	  v_printf ("Read Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs.regs[CRTI], dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]]);
	  return (dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]]);
	}
      else
	return (ext_video_port_in (port));
      break;
    case GRA_I:
      v_printf ("Read Index GRAI 0x%02x\n", GRAI);
      return (dosemu_regs.regs[GRAI]);
    case GRA_D:
      if (dosemu_regs.regs[GRAI] < GRA_C)
	{
	  v_printf ("Read Data at GRA  Index 0x%02x = 0x%02x \n", dosemu_regs.regs[GRAI], dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]]);
	  return (dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]]);
	}
      else
	return (ext_video_port_in (port));
      break;
    case SEQ_I:
      v_printf ("Read Index SEQI 0x%02x\n", SEQI);
      return (dosemu_regs.regs[SEQI]);
    case SEQ_D:
      if (dosemu_regs.regs[SEQI] < SEQ_C)
	{
	  v_printf ("Read Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs.regs[SEQI], dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]]);
	  return (dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]]);
	}
      else
	return (ext_video_port_in (port));
      break;
    case ATT_IW:
      v_printf ("Read Index ATTIW 0x%02x\n", att_d_index);
      return (att_d_index);
    case ATT_R:
      if (att_d_index > 20)
	return (ext_video_port_in (port));
      else
	{
	  v_printf ("Read Index ATTR 0x%02x\n", dosemu_regs.regs[ATT + att_d_index]);
	  return (dosemu_regs.regs[ATT + att_d_index]);
	}
      break;
    case IS1_RC:
    case IS1_RM:
      v_printf ("Read ISR1_R 0x%02x\n", dosemu_regs.regs[ISR1]);
      isr_read = 1;		/* Next ATT write will be to the index */
      return (dosemu_regs.regs[ISR1]);
    case MIS_R:
      v_printf ("Read MIS_R 0x%02x\n", dosemu_regs.regs[MIS]);
      return (dosemu_regs.regs[MIS]);
    case IS0_R:
      v_printf ("Read ISR0_R 0x%02x\n", dosemu_regs.regs[ISR0]);
      return (dosemu_regs.regs[ISR0]);
    case PEL_IW:
      v_printf ("Read PELIW 0x%02x\n", dosemu_regs.regs[PELIW] / 3);
      return (dosemu_regs.regs[PELIW] / 3);
    case PEL_IR:
      v_printf ("Read PELIR 0x%02x\n", dosemu_regs.regs[PELIR] / 3);
      return (dosemu_regs.regs[PELIR] / 3);
    case PEL_D:
      v_printf ("Read PELIR Data 0x%02x\n", dosemu_regs.pal[dosemu_regs.regs[PELIR]]);
      return (dosemu_regs.pal[dosemu_regs.regs[PELIR]++]);
    case PEL_M:
      v_printf ("Read PELM  Data 0x%02x\n", dosemu_regs.regs[PELM] == 0 ? 0xff : dosemu_regs.regs[PELM]);
      return (dosemu_regs.regs[PELM] == 0 ? 0xff : dosemu_regs.regs[PELM]);
    default:
      return (ext_video_port_in (port));
    }
}

void
video_port_out (ioport_t port, u_char value)
{
  if (permissions) {
    port_real_outb (port, value);
    return;
  }
  /* v_printf("Video write on port 0x%04x,byte 0x%04x.\n",port, value); */
  switch (port)
    {
    case CRT_IC:
    case CRT_IM:
      v_printf ("Set Index CRTC 0x%02x\n", value);
      dosemu_regs.regs[CRTI] = value;
      break;
    case CRT_DC:
    case CRT_DM:
      if (dosemu_regs.regs[CRTI] < CRT_C)
	{
	  v_printf ("Write Data at CRTC Index 0x%02x = 0x%02x \n", dosemu_regs.regs[CRTI], value);
	  dosemu_regs.regs[CRT + dosemu_regs.regs[CRTI]] = value;
	}
      else
	ext_video_port_out (port, value);
      break;
    case GRA_I:
      v_printf ("Set Index GRAI 0x%02x\n", value);
      dosemu_regs.regs[GRAI] = value;
      break;
    case GRA_D:
      if (dosemu_regs.regs[GRAI] < GRA_C)
	{
	  v_printf ("Write Data at GRAI Index 0x%02x = 0x%02x \n", dosemu_regs.regs[GRAI], value);
	  dosemu_regs.regs[GRA + dosemu_regs.regs[GRAI]] = value;
	}
      else
	ext_video_port_out (port, value);
      break;
    case SEQ_I:
      v_printf ("Set Index SEQI 0x%02x\n", value);
      dosemu_regs.regs[SEQI] = value;
      break;
    case SEQ_D:
      if (dosemu_regs.regs[SEQI] < SEQ_C)
	{
	  v_printf ("Write Data at SEQ Index 0x%02x = 0x%02x \n", dosemu_regs.regs[SEQI], value);
	  dosemu_regs.regs[SEQ + dosemu_regs.regs[SEQI]] = value;
	}
      else
	ext_video_port_out (port, value);
      break;
    case ATT_IW:
      if (isr_read)
	{
	  v_printf ("Set Index ATTI 0x%02x\n", value & 0x1f);
	  att_d_index = value & 0x1f;
	  isr_read = 0;
	}
      else
	{
	  isr_read = 1;
	  if (att_d_index > 20)
	    ext_video_port_out (port, value);
	  else
	    {
	      v_printf ("Write Data at ATT Index 0x%02x = 0x%02x \n", att_d_index, value);
	      dosemu_regs.regs[ATT + att_d_index] = value;
	    }
	}
      break;
    case MIS_W:
      v_printf ("Set MISW 0x%02x\n", value);
      dosemu_regs.regs[MIS] = value;
      break;
    case PEL_IW:
      v_printf ("Set PELIW  to 0x%02x\n", value);
      dosemu_regs.regs[PELIW] = value * 3;
      break;
    case PEL_IR:
      v_printf ("Set PELIR to 0x%02x\n", value);
      dosemu_regs.regs[PELIR] = value * 3;
      break;
    case PEL_D:
      v_printf ("Put PEL_D[0x%02x] = 0x%02x\n", dosemu_regs.regs[PELIW], value);
      dosemu_regs.pal[dosemu_regs.regs[PELIW]] = value;
      dosemu_regs.regs[PELIW] += 1;
      break;
    case PEL_M:
      v_printf ("Set PEL_M to 0x%02x\n", value);
      dosemu_regs.regs[PELM] = value;
      break;
    case GR1_P:
      v_printf ("Set GR1_P to 0x%02x\n", value);
      dosemu_regs.regs[GR1P] = value;
      break;
    case GR2_P:
      v_printf ("Set GR2_P to 0x%02x\n", value);
      dosemu_regs.regs[GR2P] = value;
      break;
    default:
      ext_video_port_out (port, value);
    }
  return;
}
