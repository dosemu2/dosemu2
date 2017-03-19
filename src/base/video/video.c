
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

#include "config.h"
#include "emu.h"
#include "int.h"
#include "bios.h"
#include "port.h"
#include "memory.h"
#include "render.h"
#include "vgaemu.h"
#include "vc.h"
#include "mapping.h"
#include "utilities.h"
#include "pci.h"
#include "keyb_clients.h"
#include "video.h"

struct video_system *Video;
#define MAX_VID_CLIENTS 16
static struct video_system *vid_clients[MAX_VID_CLIENTS];
static int num_vid_clients;
int video_initialized;

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
  NULL,		/* late_init */
  NULL,		/* early_close */
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

    if (!on_console()) return 0;	// not using KMS under X
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

static void init_video_none(void)
{
    c_printf("VID: Video set to Video_none\n");
    config.cardtype = CARD_NONE;
    config.X = config.console_video = config.mapped_bios = config.vga = 0;
    Video=&Video_none;
    config.term = 1;
    config.dumb_video = 1;
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
  if (!config.term && config.cardtype != CARD_NONE && using_kms())
  {
    config.vga = config.console_video = config.mapped_bios = config.pci_video = 0;
    warn("KMS detected: using SDL mode.\n");
#ifdef SDL_SUPPORT
    load_plugin("sdl");
    Video = video_get("sdl");
    if (Video) {
      config.X = 1;
      config.sdl = 1;
    }
#endif
  }

#ifdef USE_CONSOLE_PLUGIN
  if (config.console_video || config.console_keyb == KEYB_RAW)
    load_plugin("console");
#endif
  /* figure out which video front end we are to use */
  if (no_real_terminal() || config.dumb_video || config.cardtype == CARD_NONE) {
    init_video_none();
  }
  else if (config.X) {
    /* already initialized */
  }
  else if (config.vga) {
    c_printf("VID: Video set to Video_graphics\n");
    Video = video_get("graphics");
  }
  else if (config.console_video) {
    if (config.cardtype == CARD_MDA)
      {
	c_printf("VID: Video set to Video_hgc\n");
	Video = video_get("hgc");
      }
    else
      {
	c_printf("VID: Video set to Video_console\n");
	Video = video_get("console");
      }
  }

  if (Video && Video->priv_init) {
    int err = Video->priv_init();          /* call the specific init routine */
    if (err) {
      warn("VID: priv initialization failed for %s\n", Video->name);
      Video = NULL;
    }
  }
  return 0;
}

void video_early_close(void) {
  if (!video_initialized)
    return;
  v_printf("VID: video_early_close() called\n");
  if (Video && Video->early_close) {
    Video->early_close();
    v_printf("VID: video_close()->Video->early_close() called\n");
  }
  /* Note: update_screen() may be called up to video_close().
   * Hope that plugins that implement early_close(), do NOT implement
   * update_screen() at the same time. */
}

void video_close(void) {
  if (!video_initialized)
    return;
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

static void do_reserve_vmem(dosaddr_t base, int len)
{
  if (config.vga)
    register_hardware_ram('v', base, len);
  memcheck_reserve('v', base, len);
}

static void reserve_video_memory(void)
{
  if (config.umb_b0 && !config.dualmon) {
    if (!config.umb_a0)
      do_reserve_vmem(GRAPH_BASE, GRAPH_SIZE);
    do_reserve_vmem(VGA_PHYS_TEXT_BASE, VGA_TEXT_SIZE);
  } else {
    if (!config.umb_a0)
      do_reserve_vmem(VMEM_BASE, VMEM_SIZE);
    else
      do_reserve_vmem(VMEM_BASE + 0x10000, VMEM_SIZE - 0x10000);
  }
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

void video_config_init(void)
{
  video_init();
  reserve_video_memory();
}

static void init_video_term(void)
{
#ifdef USE_SLANG
  load_plugin("term");
  Video = video_get("term");
  if (!Video) {
    init_video_none();
  } else {
    config.term = 1;
    c_printf("VID: Video set to Video_term\n");
  }
#else
  init_video_none();
#endif
}

void video_post_init(void)
{
  int err = 0;

  switch (config.cardtype) {
  case CARD_MDA:
    {
      bios_configuration |= (MDA_CONF_SCREEN_MODE);
      video_combo = MDA_VIDEO_COMBO;
      break;
    }
  case CARD_CGA:
    {
      bios_configuration |= (CGA_CONF_SCREEN_MODE);
      video_combo = CGA_VIDEO_COMBO;
      break;
    }
  case CARD_EGA:
    {
      bios_configuration |= (EGA_CONF_SCREEN_MODE);
      video_combo = EGA_VIDEO_COMBO;
      break;
    }
  case CARD_VGA:
    {
      bios_configuration |= (VGA_CONF_SCREEN_MODE);
      video_combo = VGA_VIDEO_COMBO;
      break;
    }
  default:			/* or Terminal, is this correct ? */
    {
      bios_configuration |= (CGA_CONF_SCREEN_MODE);
      video_combo = CGA_VIDEO_COMBO;
      break;
    }
  }

  if (Video && Video->init) {
    c_printf("VID: initializing video %s\n", Video->name);
    err = Video->init();
    if (err)
      warn("VID: initialization failed for %s\n", Video->name);
  }
  if (!Video || err) {
    if (config.sdl) {
      /* silly fall-back from SDL to X or slang.
       * Can work because X/slang do not have priv_init */
      config.sdl = 0;
      if (using_kms()) {
        init_video_term();
        if (Video) {
          err = Video->init();
          if (err) {
            error("Unable to initialize SDL and terminal video\n");
            leavedos(3);
          }
        }
      }
#ifdef X_SUPPORT
      else {
        load_plugin("X");
        Video = video_get("X");
        if (Video) {
          err = Video->init();
          if (err) {
            error("Unable to initialize X and SDL video\n");
            leavedos(3);
          }
          config.X = 1;
          config.mouse.type = MOUSE_X;
          c_printf("VID: Video set to Video_X\n");
        }
      }
#endif
    } else {
      init_video_term();
      if (Video) {
        err = Video->init();
        if (err)
          Video = NULL;
      }
    }
  }
  if (!Video) {
    error("Unable to initialize video subsystem\n");
    leavedos(3);
  }

  if (!config.vga) {
    vga_emu_pre_init();
    render_init();
  }
}

void video_late_init(void)
{
  if (Video && Video->late_init)
    Video->late_init();
}

int on_console(void)
{
#ifdef __linux__
    struct stat chkbuf;
    int major, minor;

    if (console_fd == -2)
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
       console_fd = STDIN_FILENO;
       return 1;
    }
#endif
    return 0;
}

void
vt_activate(int num)
{
    struct video_system *v = video_get("console");
    if (v && v->vt_activate)
	v->vt_activate(num);
    else
	error("VID: Console plugin unavailable\n");
}
