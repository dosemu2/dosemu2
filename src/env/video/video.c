/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


/* currently this file contains only global video vars and
   initialization/cleanup code.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "config.h"
#include "emu.h"
#include "bios.h"
#include "port.h"
#include "memory.h"
#include "video.h"
#if 0
#include "hgc.h"
#endif
#include "termio.h"
#include "vc.h"
#include "mapping.h"
#include "vga.h"

struct video_system *Video = NULL;

/* I put Video_none here because I don't want to make a special file for such 
 * a simple task -- Goga
 */
static int i_empty_void (void) {return 0;}
static void v_empty_void (void) {}
static int i_empty_3int (int type, int xsize, int ysize) {return 0;}

struct video_system Video_none = {
  0,		/* is_mapped */
  i_empty_void,	/* init */
  v_empty_void,	/* close */
  i_empty_3int,	/* setmode */
  i_empty_void,	/* update_screen */
  v_empty_void	/* update_cursor */
};

/* 
 * DANG_BEGIN_FUNCTION video_init
 *
 * description:
 *  Set pointer to correct structure of functions to initialize, close,
 *  etc... video routines.
 *
 * DANG_END_FUNCTION
 */
static int video_init(void)
{
  /* figure out which video front end we are to use */
  
  if (config.vga) {
     v_printf("VID: Video set to Video_graphics\n");
     Video=&Video_graphics;
  }
  else if (config.console_video) {
     if (config.cardtype == CARD_MDA)
       {
	 v_printf("VID: Video set to Video_hgc\n");
	 Video = &Video_hgc;
       }
     else
       {
	 v_printf("VID: Video set to Video_console\n");
	 Video=&Video_console;
       }
  }
#ifdef X_SUPPORT
  else if (config.X) {
     v_printf("VID: Video set to Video_X\n");
     Video=&Video_X;
  }
#endif
  else if (config.cardtype == CARD_NONE) {
     v_printf("VID: Video set to Video_none");
     Video=&Video_none;
  }
  else {
     v_printf("VID: Video set to Video_term\n");
     Video=&Video_term;       /* ansi or ncurses */
  }

#if USE_DUALMON
  init_dualmon();
#endif

  Video->init();              /* call the specific init routine */

  if (!Video->is_mapped) {
     /* allocate screen buffer for non-console video compare speedup */
     int co_ = co;
     if (co_ < MAX_COLUMNS) co_ = MAX_COLUMNS; /* sanity check */
     prev_screen = (ushort *)malloc(co_ * MAX_LINES * 2);
     if (prev_screen==NULL) {
        error("could not malloc prev_screen\n");
        leavedos(99);
     }
     v_printf("SCREEN saves at: %p of %d size\n", prev_screen, co * li * 2);
/* 
 * DANG_BEGIN_REMARK
 * Here the sleeping lion will be awoken and eat much of CPU time !!!
 *
 * The result of setting VM86_SCREEN_BITMAP (at state of Linux 1.1.56):
 *   Each vm86 call will set 32 pages of video mem RD-only
 *    (there may be 1000000 per second)
 *   Write access to RD-only page results in page-fault (mm/memory.c),
 *   which will set a bit in current->screen_bitmap and calls do_wp_page()
 *   which does __get_free_page(GFP_KERNEL) but frees it immediatly, 
 *   because copy-on-write is not neccessary and sets RD/WR for the page.
 *   (this could happen 32000000 per second, if the CPU were fast enough)
 * It would be better to get the DIRTY-bit directly from the page table,
 * isn't it?  A special syscall in emumodule could do this.
 * DANG_END_REMARK
 */
#if VIDEO_CHECK_DIRTY
     if (!config_dualmon) {
       vm86s.flags |= VM86_SCREEN_BITMAP;
     }
#endif

  }
  
  clear_screen(video_page, 7);

  return 0;
}

void
scr_state_init(void){
  scr_state.vt_allow = 0;
  scr_state.vt_requested = 0;
  scr_state.mapped = 0;
  scr_state.pageno = 0;
  scr_state.virt_address = PAGE_ADDR(0);
  /* Assume the screen is initially mapped. */
  scr_state.current = 1;
}

void video_close(void) {
  v_printf("VID: video_close() called\n");
  if (Video && Video->close) {
    Video->close();
    v_printf("VID: video_close()->Video->close() called\n");
  }
  if (config.console_video /* && config.vga */) {
    v_printf("VID: video_close():clear console video\n");
    clear_console_video();
  }
}

/* load <msize> bytes of file <name> starting at offset <foffset>
 * into memory at <mstart>
 */
int
load_file(char *name, int foffset, char *mstart, int msize)
{
  int fd;

  if (strcmp(name, "/dev/mem") == 0) {
    v_printf("kmem used for loadfile\n");
    open_kmem();
    fd = mem_fd;
  }
  else
    fd = open(name, O_RDONLY);

  DOS_SYSCALL(lseek(fd, foffset, SEEK_SET));
  RPT_SYSCALL(read(fd, mstart, msize));

  if (strcmp(name, "/dev/mem") == 0)
    close_kmem();
  else
    close(fd);
  return 0;
}

static inline void
reserve_video_memory(void)
{
  memcheck_addtype('v', "Video memory");

/* 
 * DANG_BEGIN_REMARK
 * reserve_video_memory()
 *
 * This procedure is trying to eke out all the UMB blocks possible to
 * maximize your memory under DOSEMU.  If you know about dual monitor
 * setups, you can contribute by putting in the correct graphics page
 * address values.
 * DANG_END_REMARK
 */

#if USE_DUALMON
  if (!config.max_umb || config.dualmon) {
#else
  if (!config.max_umb) {
#endif
    if (config.dualmon)
      c_printf("CONF: Unable to maximize UMB's due to dual monitor setup\n");
    if (config.mem_size > 640) {
      int addr_start = config.mem_size * 1024;
      memcheck_reserve('v', addr_start, 0xC0000 - addr_start);
    }
    else
      memcheck_reserve('v', GRAPH_BASE, GRAPH_SIZE);
  } else {
    int graph_base, graph_size;

    /* Okay, the usual procedure would be to reserve 128K of memory for
       video unconditionally.  Clearly this is insane.  If you're running
       in an x-term or across the network, you're wasting memory. */

    c_printf("CONF: Trying to set minimum video memory to maximize UMB's\n");

    if (!config.console_video) {
      graph_base = 0xB0000;
      graph_size = 64*1024;
      c_printf("CONF: remote session.  Assuming %uKB video memory @ 0x%5.5X\n",
	       graph_size/1024, graph_base);
    }
    else {
      switch (config.cardtype) {
      case CARD_MDA:
	graph_base = 0xB0000;
	graph_size = 64*1024;
	c_printf("CONF: MDA video card w/%uKB video memory @ 0x%5.5X\n",
		 graph_size/1024, graph_base);
	break;
      case CARD_CGA:
	graph_base = 0xB8000;
	graph_size = 32*1024;
	c_printf("CONF: CGA video card w/%uKB video memory @ 0x%5.5X\n",
		 graph_size/1024, graph_base);
	break;
      case CARD_EGA:
	graph_base = 0xB0000;
	graph_size = 64*1024;
	c_printf("CONF: EGA video card w/%uKB video memory @ 0x%5.5X\n",
		 graph_size/1024, graph_base);
	break;
      case CARD_VGA:
	graph_base = 0xA0000;
	graph_size = 128*1024;
	c_printf("CONF: VGA video card w/%uKB video memory @ 0x%5.5X\n",
		 graph_size/1024, graph_base);
	break;
      default:
	graph_base = 0xA0000;
	graph_size = 128*1024;
	c_printf("CONF: default video, guessing %uKB video memory @ 0x%5.5X\n",
		 graph_size/1024, graph_base);
	break;
      }
    }

    memcheck_reserve('v', graph_base, graph_size);
  }
}

void 
set_video_bios_size(void){
  WRITE_WORD(BIOS_SCREEN_COLUMNS, co);     /* chars per line */
  WRITE_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1, li - 1); /* lines on screen - 1 */
  WRITE_WORD(BIOS_VIDEO_MEMORY_USED, TEXT_SIZE);   /* size of video regen area in bytes */
}

void
gettermcap(int i)
{
  struct winsize ws;		/* buffer for TIOCSWINSZ */

  li = LI;
  co = CO;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) >= 0) {
    li = ws.ws_row;
    co = ws.ws_col;
  }

  if (co > MAX_COLUMNS || li > MAX_LINES) {
    error("Screen size is too large: %dx%d, max is %dx%d\n",
      co, li, MAX_COLUMNS, MAX_LINES);
    leavedos(0x63);
  }

  if (li == 0 || co == 0) {
    error("unknown window sizes li=%d  co=%d, setting to 80x25\n", li, co);
    li = LI;
    co = CO;
  }
  else
    v_printf("VID: Setting windows size to li=%d, co=%d\n", li, co);
  get_screen_size();
  set_video_bios_size();
}

void
video_config_init(void) {
  gettermcap(0);

  switch (config.cardtype) {
  case CARD_MDA:
    {
      configuration |= (MDA_CONF_SCREEN_MODE);
      video_mode = MDA_INIT_SCREEN_MODE;
      phys_text_base = MDA_PHYS_TEXT_BASE;
      virt_text_base = MDA_VIRT_TEXT_BASE;
      video_combo = MDA_VIDEO_COMBO;
      video_subsys = MDA_VIDEO_SUBSYS;
      break;
    }
  case CARD_CGA:
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      video_mode = CGA_INIT_SCREEN_MODE;
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      video_subsys = CGA_VIDEO_SUBSYS;
      break;
    }
  case CARD_EGA:
    {
      configuration |= (EGA_CONF_SCREEN_MODE);
      video_mode = EGA_INIT_SCREEN_MODE;
      phys_text_base = EGA_PHYS_TEXT_BASE;
      virt_text_base = EGA_VIRT_TEXT_BASE;
      video_combo = EGA_VIDEO_COMBO;
      video_subsys = EGA_VIDEO_SUBSYS;
      break;
    }
  case CARD_VGA:
    {
      configuration |= (VGA_CONF_SCREEN_MODE);
      video_mode = VGA_INIT_SCREEN_MODE;
      phys_text_base = VGA_PHYS_TEXT_BASE;
      virt_text_base = VGA_VIRT_TEXT_BASE;
      video_combo = VGA_VIDEO_COMBO;
      video_subsys = VGA_VIDEO_SUBSYS;
      break;
    }
  default:			/* or Terminal, is this correct ? */
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      video_mode = CGA_INIT_SCREEN_MODE;
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      video_subsys = CGA_VIDEO_SUBSYS;
      break;
    }
  }
  if (!config.console) {
     /* NOTE: BIG FAT WARNING !!!
      *       without this you will reproduceable KILL LINUX
      *       (seen with Linux-2.0.28)            ^^^^^^^^^^
      *       This happens in xterm, not console and not xdos.
      *       I was unable to trace it down, because directly at
      *       startup you get a black screen and no logs are left
      *       once you repaired your files system :(
      *       Though this is a userspace bug, it should not
      *       kill the kernel IMHO. -- Hans
      */
     v_printf("VID: not running on console - resetting to terminal mode\n");
     config.console_video=0;
     scr_state.console_no = 0;
     config.console_keyb = 0;
     config.console_video = 0;
     config.mapped_bios = 0;
     config.vga = 0;
     config.console = 0;
     if (config.speaker == SPKR_NATIVE)
        config.speaker = SPKR_EMULATED;
  }
  video_page = 0;
  screen_mask = 1 << (((int)phys_text_base-0xA0000)/4096);
  screen_adr = SCREEN_ADR(0);
  if (!config.vga) {
    WRITE_BYTE(BIOS_CURRENT_SCREEN_PAGE, 0x0);	/* Current Screen Page */
    WRITE_WORD(BIOS_CURSOR_SHAPE, (configuration&MDA_CONF_SCREEN_MODE)?0x0A0B:0x0607);
    
    /* This is needed in the video stuff. Grabbed from boot(). */
    if ((configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE)
      WRITE_WORD(BIOS_VIDEO_PORT, 0x3b4);	/* base port of CRTC - IMPORTANT! */
    else
      WRITE_WORD(BIOS_VIDEO_PORT, 0x3d4);	/* base port of CRTC - IMPORTANT! */
    
    WRITE_BYTE(BIOS_VDU_CONTROL, 9);	/* current 3x8 (x=b or d) value */
    
    WRITE_BYTE(BIOS_VIDEO_MODE, video_mode); /* video mode */
    set_video_bios_size();
    WRITE_WORD(BIOS_VIDEO_MEMORY_ADDRESS, 0);/* offset of current page in buffer */
    
    WRITE_WORD(BIOS_FONT_HEIGHT, 16);
    
    /* XXX - these are the values for VGA color!
       should reflect the real display hardware. */
    WRITE_BYTE(BIOS_VIDEO_INFO_0, 0x60);
    WRITE_BYTE(BIOS_VIDEO_INFO_1, 0xF9);
    WRITE_BYTE(BIOS_VIDEO_INFO_2, 0x51);
    WRITE_BYTE(BIOS_VIDEO_COMBO, video_combo);
    
    WRITE_DWORD(BIOS_VIDEO_SAVEPTR, 0);		/* pointer to video table */
  }
    
  if (config.console_video) {
    set_process_control();
    set_console_video();
  }

  video_init();

  reserve_video_memory();
}
#define graphics_init vga_initialize
#define graphics_close NULL
#define graphics_setmode NULL

struct video_system Video_graphics = {
   1,                /* is_mapped */
   graphics_init,
   graphics_close,
   graphics_setmode,
   NULL,             /* update_screen */
   NULL
};

