
/* currently this file contains only global video vars and
   initialization/cleanup code.
*/

#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "emu.h"
#include "bios.h"
#include "memory.h"
#include "video.h"

void open_kmem();      /* from vc.c */


int video_mode = 0;		/* Init Screen Mode in emu.c     */

int video_page=0;
int char_blink=1;
int font_height=16;
int co=80, li=25;

ushort *screen_adr;       /* pointer to current video mem page */
ushort *prev_screen;      /* currently displayed page (was scrbuf) */

struct video_system *Video = NULL;

int cursor_col=0, cursor_row=0;
ushort cursor_shape=0x0E0F;

unsigned int screen_mask;  /* bit mask for testing vm86s.screen_bitmap */    


int video_init()
{
  /* figure out which video front end we are to use */
  
  if (config.graphics) {
     /* something to do here? */
  }
  else if (config.console_video)
     Video=&Video_console;
#ifdef X_SUPPORT
  else if (config.X)
     Video=&Video_X;
#endif
  else 
     Video=&Video_term;       /* ansi or ncurses */

  Video->init();              /* call the specific init routine */

  termioInit();            /* kludge! */

  if (!Video->is_mapped) {
     /* allocate screen buffer for non-console video compare speedup */
     prev_screen = (ushort *)malloc(CO * LI * 2);
     if (prev_screen==NULL) {
        error("could not malloc prev_screen\n");
        leavedos(99);
     }
     v_printf("SCREEN saves at: %p of %d size\n", prev_screen, CO * LI * 2);
     vm86s.flags |= VM86_SCREEN_BITMAP;
  }
  
  if (config.vga) vga_initialize();
  clear_screen(video_page, 7);

  return 0;
}

void video_close() {
  if (Video && Video->close) Video->close();
  termioClose();          /* kludge! */
}

/* load <msize> bytes of file <name> starting at offset <foffset>
 * into memory at <mstart>
 */
int
load_file(char *name, int foffset, char *mstart, int msize)
{
  int fd;

  if (strcmp(name, "/dev/kmem") == 0) {
    v_printf("kmem used for loadfile\n");
    open_kmem();
    fd = mem_fd;
  }
  else
    fd = open(name, O_RDONLY);

  DOS_SYSCALL(lseek(fd, foffset, SEEK_SET));
  RPT_SYSCALL(read(fd, mstart, msize));

  if (strcmp(name, "/dev/kmem") == 0)
    close_kmem();
  else
    close(fd);
  return 0;
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
  bios_current_screen_page = 0x0;	/* Current Screen Page */
  video_page = 0;
  screen_mask = 1 << (((int)phys_text_base-0xA0000)/4096);
  screen_adr = SCREEN_ADR(0);
  bios_cursor_shape = (configuration&MDA_CONF_SCREEN_MODE)?0x0A0B:0x0607;
  
  /* This is needed in the video stuff. Grabbed from boot(). */
  if ((configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE)
    bios_video_port = 0x3b4;	/* base port of CRTC - IMPORTANT! */
  else
    bios_video_port = 0x3d4;	/* base port of CRTC - IMPORTANT! */

  if (config.mapped_bios) {
    if (config.vbios_file) {
      warn("WARN: loading VBIOS %s into mem at 0x%X (0x%X bytes)\n",
	   config.vbios_file, VBIOS_START, VBIOS_SIZE);
      load_file(config.vbios_file, 0, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else if (config.vbios_copy) {
      warn("WARN: copying VBIOS from /dev/kmem at 0x%X (0x%X bytes)\n",
	   VBIOS_START, VBIOS_SIZE);
      load_file("/dev/kmem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else {
      warn("WARN: copying VBIOS file from /dev/kmem\n");
      load_file("/dev/kmem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
    }
  }

  /* copy graphics characters from system BIOS */
  load_file("/dev/kmem", GFX_CHARS, (char *) GFX_CHARS, GFXCHAR_SIZE);

}
