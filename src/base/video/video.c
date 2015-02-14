
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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <assert.h>
#ifdef __linux__
#include <sys/kd.h>
#include <sys/vt.h>
#endif

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
#include "utilities.h"
#include "pci.h"

struct video_system *Video = NULL;
#define MAX_VID_CLIENTS 16
static struct video_system *vid_clients[MAX_VID_CLIENTS];
static int num_vid_clients;

/* I put Video_none here because I don't want to make a special file for such
 * a simple task -- Goga
 */
static int i_empty_void (void) {return 0;}
static void v_empty_void (void) {}

static int video_none_init(void)
{
  vga_emu_setmode(video_mode, CO, LI);
  return 0;
}

static struct video_system Video_none = {
  i_empty_void,	/* priv_init */
  video_none_init,	/* init */
  v_empty_void,	/* close */
  NULL,		/* setmode */
  NULL,	        /* update_screen */
  NULL,         /* change_config */
  NULL,         /* handle_events */
  "none"
};

static int no_real_terminal(void)
{
  char *term = getenv("TERM");
  if (config.X)
    return 0;
  if ( term == NULL
       || !strcmp(term,"dumb")        /* most cron's have this */
       || !strcmp(term,"none")        /* ... some have this */
       || !strcmp(term,"dosemu-none") /* ... when called recursivly */
       ) {
    /*
     * We assume we are running without a terminal (e.g. in a cronjob).
     * Hence, we setup the TERM variable to "dosemu-none",
     * set a suitable TERMCAP entry ... and ignore the rest
     * the TERMCAP is needed because we still use SLang for the keyboard.
     */
    setenv("TERM", "dosemu-none", 1);
    setenv("TERMCAP",
	   "dosemu-none|for running DOSEMU without real terminal:"
	   ":am::co#80:it#8:li#25:"
	   ":ce=\\E[K:cl=\\E[H\\E[2J:cm=\\E[%i%d;%dH:do=\\E[B:ho=\\E[H:"
	   ":le=\\E[D:nd=\\E[C:ta=^I:up=\\E[A:",
	   1
	   );
    return 1;
  }
  return 0;
}

/* According to Reinhard Karcher console graphics may actually work with KMS! */
/* check whether KMS is used.
 * in that case graphics=(auto)/console=(auto) will not activate console
 * graphics.
 * code adapted from libdrm (drmCheckModesettingSupported)
 */
static int using_kms(void)
{
#ifdef __linux__
    char pci_dev_dir[1024];
    int bus, dev, func;
    DIR *sysdir;
    struct dirent *dent;
    int found = 0;
    pciRec *pcirec;

    if (!pcibios_init()) return 0;
    pcirec = pcibios_find_class(PCI_CLASS_DISPLAY_VGA << 8, 0);
    if (!pcirec) return 0;

    bus = pcirec->bdf >> 8;
    dev = (pcirec->bdf & 0xff) >> 3;
    func = pcirec->bdf & 0x7;

    sprintf(pci_dev_dir, "/sys/bus/pci/devices/0000:%02x:%02x.%d/drm",
            bus, dev, func);

    sysdir = opendir(pci_dev_dir);
    if (sysdir) {
        dent = readdir(sysdir);
        while (dent) {
            if (!strncmp(dent->d_name, "controlD", 8)) {
                found = 1;
                break;
            }
            dent = readdir(sysdir);
        }
        closedir(sysdir);
        if (found)
            return 1;
    }

    sprintf(pci_dev_dir, "/sys/bus/pci/devices/0000:%02x:%02x.%d/",
            bus, dev, func);

    sysdir = opendir(pci_dev_dir);
    if (!sysdir)
        return 0;

    dent = readdir(sysdir);
    while (dent) {
        if (!strncmp(dent->d_name, "drm:controlD", 12)) {
            found = 1;
            break;
        }
        dent = readdir(sysdir);
    }

    closedir(sysdir);
    if (found)
        return 1;

#endif
    return 0;
}

void register_video_client(struct video_system *vid)
{
    assert(num_vid_clients < MAX_VID_CLIENTS);
    vid_clients[num_vid_clients++] = vid;
    v_printf("VID: registered video client %s\n", vid->name);
}

struct video_system *video_get(const char *name)
{
    int i;
    for (i = 0; i < num_vid_clients; i++) {
	if (strcmp(vid_clients[i]->name, name) == 0)
	    return vid_clients[i];
    }
    return NULL;
}

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
  if ((config.vga == -1 || config.console_video == -1) && using_kms())
  {
    config.vga = config.console_video = config.mapped_bios = config.pci_video = 0;
    warn("KMS detected: using SDL mode.\n");
    load_plugin("sdl");
    Video = video_get("sdl");
    if (Video)
      config.X = 1;
  }

#if defined(USE_DL_PLUGINS)
  if (!config.X)
    load_plugin("console");
#endif
  /* figure out which video front end we are to use */
  if (no_real_terminal() || config.cardtype == CARD_NONE) {
     v_printf("VID: Video set to Video_none\n");
     config.cardtype = CARD_NONE;
     config.X = config.console_video = config.mapped_bios = config.vga = 0;
     Video=&Video_none;
  }
  else if (config.X) {
     /* already initialized */
  }
  else if (config.vga) {
     v_printf("VID: Video set to Video_graphics\n");
     Video = video_get("graphics");
  }
  else if (config.console_video) {
     if (config.cardtype == CARD_MDA)
       {
	 v_printf("VID: Video set to Video_hgc\n");
	 Video = video_get("hgc");
       }
     else
       {
	 v_printf("VID: Video set to Video_console\n");
	 Video = video_get("console");
       }
  }
  else {
#if defined(USE_DL_PLUGINS) && defined(USE_SLANG)
    if (!load_plugin("term")) {
     error("Terminal (S-Lang library) support not compiled in.\n"
           "Install slang-devel and recompile, use xdosemu or console "
           "dosemu (needs root) instead.\n");
     /* too early to call leavedos */
     exit(1);
    }
    v_printf("VID: Video set to Video_term\n");
    Video = video_get("term");       /* S-Lang */
#endif
  }

  if (Video->priv_init)
      Video->priv_init();          /* call the specific init routine */

  if (Video->update_screen) {
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

static void
scr_state_init(void) {
  switch (config.cardtype) {
  case CARD_MDA:
    {
      configuration |= (MDA_CONF_SCREEN_MODE);
      phys_text_base = MDA_PHYS_TEXT_BASE;
      virt_text_base = MDA_VIRT_TEXT_BASE;
      video_combo = MDA_VIDEO_COMBO;
      break;
    }
  case CARD_CGA:
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      break;
    }
  case CARD_EGA:
    {
      configuration |= (EGA_CONF_SCREEN_MODE);
      phys_text_base = EGA_PHYS_TEXT_BASE;
      virt_text_base = EGA_VIRT_TEXT_BASE;
      video_combo = EGA_VIDEO_COMBO;
      break;
    }
  case CARD_VGA:
    {
      configuration |= (VGA_CONF_SCREEN_MODE);
      phys_text_base = VGA_PHYS_TEXT_BASE;
      virt_text_base = VGA_VIRT_TEXT_BASE;
      video_combo = VGA_VIDEO_COMBO;
      break;
    }
  default:			/* or Terminal, is this correct ? */
    {
      configuration |= (CGA_CONF_SCREEN_MODE);
      phys_text_base = CGA_PHYS_TEXT_BASE;
      virt_text_base = CGA_VIRT_TEXT_BASE;
      video_combo = CGA_VIDEO_COMBO;
      break;
    }
  }
  scr_state.vt_allow = 0;
  scr_state.mapped = 0;
  scr_state.pageno = 0;
  scr_state.virt_address = virt_text_base;
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
load_file(char *name, int foffset, unsigned char *mstart, int msize)
{
  int fd;

  if (strcmp(name, "/dev/mem") == 0) {
    v_printf("kmem used for loadfile\n");
    open_kmem();
    fd = mem_fd;
  }
  else
    fd = open(name, O_RDONLY);

  (void)DOS_SYSCALL(lseek(fd, foffset, SEEK_SET));
  (void)RPT_SYSCALL(read(fd, mstart, msize));

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
  if (config.vga)
    register_hardware_ram('v', graph_base, graph_size);
  memcheck_reserve('v', graph_base, graph_size);
}

void
gettermcap(int i, int *co, int *li)
{
  struct winsize ws;		/* buffer for TIOCSWINSZ */

  *li = *co = 0;
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

void
video_config_init(void) {
  screen_mask = 1 << (((int)phys_text_base-0xA0000)/4096);

  video_init();

  reserve_video_memory();
}

void video_post_init(void)
{
  scr_state_init();
  vga_emu_pre_init();
}

void video_late_init(void)
{
  if (Video && Video->init)
    Video->init();
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
