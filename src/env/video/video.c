/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
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
#include "int.h"
#include "bios.h"
#include "port.h"
#include "memory.h"
#include "video.h"
#include "vgaemu.h"
#include "termio.h"
#include "vc.h"
#include "mapping.h"
#include "vga.h"
#include "utilities.h"

struct video_system *Video = NULL;

/* I put Video_none here because I don't want to make a special file for such 
 * a simple task -- Goga
 */
static int i_empty_void (void) {return 0;}
static void v_empty_void (void) {}
static int i_empty_3int (int type, int xsize, int ysize) {return 0;}
static int video_combo;

static int video_none_init(void)
{
  vga_emu_init(0, NULL);
  vga_emu_setmode(video_mode, CO, LI);
  return 0;
}

struct video_system Video_none = {
  i_empty_void,	/* priv_init */
  video_none_init,	/* init */
  v_empty_void,	/* close */
  i_empty_3int,	/* setmode */
  NULL,	        /* update_screen */
  v_empty_void,	/* update_cursor */
  NULL,         /* change_config */
  NULL          /* handle_events */
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
  
  if (Video) {
     /* already initialized */
  }
  else if (config.vga) {
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
  else if (config.cardtype == CARD_NONE) {
     v_printf("VID: Video set to Video_none");
     Video=&Video_none;
  }
  else if (!load_plugin("term")) {
     error("Terminal (S-Lang library) support not compiled in.\n"
           "Install slang-devel and recompile, use xdosemu or console "
           "dosemu (needs root) instead.\n");
     /* too early to call leavedos */
     exit(1);
  }

#if USE_DUALMON
  init_dualmon();
#endif

  if (Video->priv_init)
      Video->priv_init();          /* call the specific init routine */

  if (Video->update_screen) {
     /* allocate screen buffer for non-console video compare speedup */
     prev_screen = (ushort *)malloc(MAX_COLUMNS * MAX_LINES * sizeof(ushort));
     if (prev_screen==NULL) {
        error("could not malloc prev_screen\n");
        leavedos(99);
     }
     v_printf("SCREEN saves at: %p of %d size\n", prev_screen, MAX_COLUMNS * MAX_LINES * sizeof(ushort));
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
  
  return 0;
}

void
scr_state_init(void){
  scr_state.vt_allow = 0;
  scr_state.vt_requested = 0;
  scr_state.mapped = 0;
  scr_state.pageno = 0;
  scr_state.virt_address = (void *)virt_text_base;
  /* Assume the screen is initially mapped. */
  scr_state.current = 1;
}

void video_close(void) {
  v_printf("VID: video_close() called\n");
  if (Video && Video->close) {
    Video->close();
    v_printf("VID: video_close()->Video->close() called\n");
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
  int graph_base = GRAPH_BASE, graph_size = GRAPH_SIZE;

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
      graph_base = addr_start;
      graph_size = 0xC0000 - addr_start;
    }
  } else {

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

  }
  register_hardware_ram(0, graph_base, graph_size);
  memcheck_reserve('v', graph_base, graph_size);
}

void
gettermcap(int i, int *co, int *li)
{
  struct winsize ws;		/* buffer for TIOCSWINSZ */

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) >= 0) {
    *li = ws.ws_row;
    *co = ws.ws_col;
  }

  if (*co > MAX_COLUMNS || *li > MAX_LINES) {
    error("Screen size is too large: %dx%d, max is %dx%d\n",
      *co, *li, MAX_COLUMNS, MAX_LINES);
    leavedos(0x63);
  }

  if (*li == 0 || *co == 0) {
    error("unknown window sizes li=%d  co=%d, setting to 80x25\n", *li, *co);
    *li = LI;
    *co = CO;
  }
  else
    v_printf("VID: Setting windows size to li=%d, co=%d\n", *li, *co);
}

void video_mem_setup(void)
{
  int co, li;

  WRITE_BYTE(BIOS_CURRENT_SCREEN_PAGE, 0);
  WRITE_BYTE(BIOS_VIDEO_MODE, video_mode);

  fake_call_to(INT10_SEG, config.vbios_post ? INT10_OFF : INT10_POSTLESS_OFF);

  if (config.vga)
    /* the real bios will set all this ... */
    return;

  li = LI;
  co = CO;
  if (!config.X)
    gettermcap(0, &co, &li);

  WRITE_WORD(BIOS_SCREEN_COLUMNS, co);     /* chars per line */
  WRITE_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1, li - 1); /* lines on screen - 1 */
  WRITE_WORD(BIOS_VIDEO_MEMORY_USED, TEXT_SIZE(co,li));   /* size of video regen area in bytes */

  WRITE_WORD(BIOS_CURSOR_SHAPE, (configuration&MDA_CONF_SCREEN_MODE)?0x0A0B:0x0607);
    
  /* This is needed in the video stuff. Grabbed from boot(). */
  if ((configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE)
    WRITE_WORD(BIOS_VIDEO_PORT, 0x3b4);	/* base port of CRTC - IMPORTANT! */
  else
    WRITE_WORD(BIOS_VIDEO_PORT, 0x3d4);	/* base port of CRTC - IMPORTANT! */
    
  WRITE_BYTE(BIOS_VDU_CONTROL, 9);	/* current 3x8 (x=b or d) value */
    
  WRITE_WORD(BIOS_VIDEO_MEMORY_ADDRESS, 0);/* offset of current page in buffer */
    
  WRITE_WORD(BIOS_FONT_HEIGHT, 16);
    
  /* XXX - these are the values for VGA color!
     should reflect the real display hardware. */
  WRITE_BYTE(BIOS_VIDEO_INFO_0, 0x60);
  WRITE_BYTE(BIOS_VIDEO_INFO_1, 0xF9);
  WRITE_BYTE(BIOS_VIDEO_INFO_2, 0x51);
  WRITE_BYTE(BIOS_VIDEO_COMBO, video_combo);
    
  WRITE_DWORD(BIOS_VIDEO_SAVEPTR, 0);		/* pointer to video table */

  set_video_mode(video_mode);
}

void
video_config_init(void) {
  switch (config.cardtype) {
  case CARD_MDA:
    {
      configuration |= (MDA_CONF_SCREEN_MODE);
      video_mode = MDA_INIT_SCREEN_MODE;
      phys_text_base = MDA_PHYS_TEXT_BASE;
      virt_text_base = MDA_VIRT_TEXT_BASE;
      video_combo = MDA_VIDEO_COMBO;
      break;
    }
  case CARD_CGA:
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      video_mode = CGA_INIT_SCREEN_MODE;
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      break;
    }
  case CARD_EGA:
    {
      configuration |= (EGA_CONF_SCREEN_MODE);
      video_mode = EGA_INIT_SCREEN_MODE;
      phys_text_base = EGA_PHYS_TEXT_BASE;
      virt_text_base = EGA_VIRT_TEXT_BASE;
      video_combo = EGA_VIDEO_COMBO;
      break;
    }
  case CARD_VGA:
    {
      configuration |= (VGA_CONF_SCREEN_MODE);
      video_mode = VGA_INIT_SCREEN_MODE;
      phys_text_base = VGA_PHYS_TEXT_BASE;
      virt_text_base = VGA_VIRT_TEXT_BASE;
      video_combo = VGA_VIDEO_COMBO;
      break;
    }
  default:			/* or Terminal, is this correct ? */
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      video_mode = CGA_INIT_SCREEN_MODE;
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
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
  screen_mask = 1 << (((int)phys_text_base-0xA0000)/4096);

  video_init();

  reserve_video_memory();
}

void video_post_init(void)
{
  if (Video && Video->init) Video->init();
}
