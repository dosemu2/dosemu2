/*
 * video/vga.c - This file contains function for VGA-cards only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#ifdef __linux__
#include <sys/kd.h>
#include <sys/vt.h>
#include <sys/ioctl.h>
#endif

#include "bios.h"
#include "emu.h"
#include "init.h"
#include "int.h"
#include "coopth.h"
#include "port.h"
#include "memory.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "timers.h"
#include "vbe.h"
#include "pci.h"
#include "mapping.h"
#include "utilities.h"
#include "sig.h"
#ifdef USE_SVGALIB
#include <dlfcn.h>
#include "../svgalib/svgalib.h"
#endif

#define PLANE_SIZE (64*1024)

static int vga_init(void);
static int vga_post_init(void);
static struct video_system *Video_console;

/* Here are the REGS values for valid dos int10 call */

static unsigned char vregs[60] =
{
/* BIOS mode 0x12 */
  0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
  0x3C, 0x3D, 0x3E, 0x3F, 0x01, 0x00, 0x0F, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0F, 0xFF,
  0x03, 0x21, 0x0F, 0x00, 0x06,
  0xE3
};

void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
void (*set_bank_read) (unsigned char bank);
void (*set_bank_write) (unsigned char bank);
void (*ext_video_port_out) (ioport_t port, unsigned char value);
u_char(*ext_video_port_in) (ioport_t port);

/* These are dummy calls */
static void save_ext_regs_dummy(u_char xregs[], u_short xregs16[])
{
  return;
}

static void restore_ext_regs_dummy(u_char xregs[], u_short xregs16[])
{
  return;
}

static void set_bank_read_dummy(u_char bank)
{
  return;
}

static void set_bank_write_dummy(u_char bank)
{
  return;
}

static u_char dummy_ext_video_port_in(ioport_t port)
{
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

static void dummy_ext_video_port_out(ioport_t port, u_char value)
{
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

static void dosemu_vga_screenoff(void)
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
}

static void dosemu_vga_screenon(void)
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
}

static int dosemu_vga_setpalvec(int start, int num, u_char * pal)
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

static void dosemu_vga_getpalvec(int start, int num, u_char * pal)
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

/* Prepare to do business with the EGA/VGA card */
static inline void disable_vga_card(void)
{
  /* enable video */
  emu_video_retrace_off();
  port_in(IS1_R);
  port_out(0x00, ATT_IW);
}

/* Finish doing business with the EGA/VGA card */
static inline void enable_vga_card(void)
{
  emu_video_retrace_off();
  port_in(IS1_R);
  port_out(0x20, ATT_IW);
  emu_video_retrace_on();
  /* disable video */
}

int emu_video_retrace_on(void)
{
  return (config.emuretrace>1? set_ioperm(0x3da,1,0):0);
}

int emu_video_retrace_off(void)
{
  return (config.emuretrace>1? set_ioperm(0x3c0,1,1),set_ioperm(0x3da,1,1):0);
}

/* Store current EGA/VGA regs */
static void store_vga_regs(u_char regs[])
{
  int i;

  emu_video_retrace_off();
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
  emu_video_retrace_on();
}

static sem_t cpy_sem;
static pthread_t cpy_thr;
struct vmem_chunk {
  u_char *mem;
  unsigned vmem;
  size_t len;
  int to_vid;
  int ctid;
};
static struct vmem_chunk vmem_chunk_thr;

static void vmemcpy_done(void *arg)
{
  int tid = (long)arg;
  coopth_wake_up(tid);
}

static void *vmemcpy_thread(void *arg)
{
  struct vmem_chunk *vmc = arg;
  while (1) {
    sem_wait(&cpy_sem);
    if (vmc->to_vid)
      MEMCPY_2DOS(vmc->vmem, vmc->mem, vmc->len);
    else
      MEMCPY_2UNIX(vmc->mem, vmc->vmem, vmc->len);
    add_thread_callback(vmemcpy_done, (void*)(long)vmc->ctid, "vmemcpy");
  }
  return NULL;
}

static void sleep_cb(void *arg)
{
  sem_t *sem = arg;
  sem_post(sem);
}

/* Store EGA/VGA display planes (4) */
static void store_vga_mem(u_char * mem, int banks)
{
  int cbank, plane, planar;
  unsigned vmem = GRAPH_BASE;
  int iflg;

  if (config.chipset == VESA && banks > 1)
    vmem = vesa_get_lfb();
  planar = 1;
  if (vmem != GRAPH_BASE) {
    planar = 0;
    vmem -= PLANE_SIZE;
  } else if (banks > 1) {
    port_out(0x4, SEQ_I);
    /* check whether we are using packed or planar mode;
       for standard VGA modes we set 640x480x16 colors  */
    if (port_in(SEQ_D) & 8) planar = 0;
  } else {
    set_regs((u_char *) vregs, 1);
  }
  for (cbank = 0; cbank < banks; cbank++) {
    if (planar && banks > 1) set_bank_read(cbank);
    for (plane = 0; plane < 4; plane++) {
      if (planar) {
        /* Store planes */
	port_out(0x04, GRA_I);
        port_out(plane, GRA_D);
      } else if (vmem == GRAPH_BASE)
	set_bank_read(cbank * 4 + plane);
      else
	vmem += PLANE_SIZE;

      /* reading video memory can be very slow: 16384MB takes
	 1.5 seconds here using a linear frame buffer. So we'll
	 have lots of SIGALRMs coming by. Another solution to
	 this problem would be to use a thread --Bart */
      /* SOLVED: with coopthreads_v2 we can do that in a separate
       * pthread's thread, here's how: --stsp */
      vmem_chunk_thr.mem = mem;
      vmem_chunk_thr.vmem = vmem;
      vmem_chunk_thr.len = PLANE_SIZE;
      vmem_chunk_thr.to_vid = 0;
      vmem_chunk_thr.ctid = coopth_get_tid();
      coopth_set_sleep_handler(sleep_cb, &cpy_sem);
      iflg = isset_IF();
      if (!iflg)
        set_IF();
      coopth_sleep();
      if (!iflg)
        clear_IF();
      /* end of magic: chunk copied */

      v_printf("BANK READ Bank=%d, plane=0x%02x, mem=%08x\n", cbank, plane, READ_DWORD(vmem));
      mem += PLANE_SIZE;
    }
  }
  v_printf("GRAPH_BASE to mem complete!\n");
}

/* Restore EGA/VGA display planes (4) */
static void restore_vga_mem(u_char * mem, int banks)
{
  int plane, cbank, planar;
  unsigned vmem = GRAPH_BASE;
  int iflg;

  if (config.chipset == VESA && banks > 1)
    vmem = vesa_get_lfb();
  planar = 1;
  if (vmem != GRAPH_BASE) {
    planar = 0;
    vmem -= PLANE_SIZE;
  } else if (banks > 1) {
    port_out(0x4, SEQ_I);
    /* check whether we are using packed or planar mode;
       for standard VGA modes we set 640x480x16 colors  */
    if (port_in(SEQ_D) & 8) planar = 0;
  } else {
    set_regs((u_char *) vregs, 1);
  }
  if (planar) {
      /* disable Set/Reset Register */
      port_out(0x01, GRA_I);
      port_out(0x00, GRA_D);
  }
  for (cbank = 0; cbank < banks; cbank++) {
    if (planar && banks > 1) set_bank_write(cbank);
    for (plane = 0; plane < 4; plane++) {
      if (planar) {
        /* Store planes */
        port_out(0x02, SEQ_I);
        port_out(1 << plane, SEQ_D);
      } else if (vmem == GRAPH_BASE)
	set_bank_write(cbank * 4 + plane);
      else
	vmem += PLANE_SIZE;

      vmem_chunk_thr.mem = mem;
      vmem_chunk_thr.vmem = vmem;
      vmem_chunk_thr.len = PLANE_SIZE;
      vmem_chunk_thr.to_vid = 1;
      vmem_chunk_thr.ctid = coopth_get_tid();
      coopth_set_sleep_handler(sleep_cb, &cpy_sem);
      iflg = isset_IF();
      if (!iflg)
        set_IF();
      coopth_sleep();
      if (!iflg)
        clear_IF();
      /* end of magic: chunk copied */

      v_printf("BANK WRITE Bank=%d, plane=0x%02x, mem=%08x\n", cbank, plane, *(int *)mem);
      mem += PLANE_SIZE;
    }
  }
  v_printf("mem to GRAPH_BASE complete!\n");
}

/* Restore EGA/VGA regs */
static void restore_vga_regs(u_char regs[], u_char xregs[], u_short xregs16[])
{
  restore_ext_regs(xregs, xregs16);
  set_regs(regs, 0);
  v_printf("Restore_vga_regs completed!\n");
}

/* Save all necessary info to allow switching vt's */
void save_vga_state(struct video_save_struct *save_regs)
{

  v_printf("Saving data for %s\n", save_regs->video_name);
  port_enter_critical_section(__func__);
  dosemu_vga_screenoff();
  disable_vga_card();
  store_vga_regs(save_regs->regs);
  save_ext_regs(save_regs->xregs, save_regs->xregs16);
  v_printf("ALPHA mode save being attempted\n");
  save_regs->banks = 1;
  port_out(0x06, GRA_I);
  if (!(port_in(GRA_D) & 0x01)) {
    /* text mode */
    v_printf("ALPHA mode save being performed\n");
  }
  else if (save_regs->video_mode > 0x13 && config.chipset && save_regs != &linux_regs)
        /*not standard VGA modes*/      /* not plainvga */
    save_regs->banks = (config.gfxmemsize + (4 * PLANE_SIZE / 1024) - 1) /
      (4 * PLANE_SIZE / 1024);
  v_printf("Mode  == %d\n", save_regs->video_mode);
  v_printf("Banks == %d\n", save_regs->banks);
  if (save_regs->banks) {
    if (!save_regs->mem) {
      save_regs->mem = malloc(save_regs->banks * 4 * PLANE_SIZE);
    }

    store_vga_mem(save_regs->mem, save_regs->banks);
  }
  dosemu_vga_getpalvec(0, 256, save_regs->pal);
  restore_vga_regs(save_regs->regs, save_regs->xregs, save_regs->xregs16);
  restore_ext_regs(save_regs->xregs, save_regs->xregs16);
  enable_vga_card();
  dosemu_vga_screenon();
  port_leave_critical_section();

  v_printf("Store_vga_state complete\n");
}

/* Restore all necessary info to allow switching back to vt */
void restore_vga_state(struct video_save_struct *save_regs)
{

  v_printf("Restoring data for %s\n", save_regs->video_name);
  port_enter_critical_section(__func__);
  /* my Matrox G550 seem to completely ignore the bit15, so
   * lets disable the below trick. Are there any cards that
   * really need this? */
#if 0
  /* do a BIOS setmode to the original mode before restoring registers.
     This helps if we don't restore all registers ourselves or if the
     VESA BIOS is buggy */
  if (config.chipset == PLAINVGA || config.chipset == VESA) {
    char bios_data_area[0x100];
    int mode = save_regs->video_mode;
    if (config.chipset == VESA && !config.gfxmemsize)
      mode |= 0x8000;		/* preserve video memory */
    memcpy(bios_data_area, (void *)BIOS_DATA_SEG, 0x100);
    do_int10_setmode(mode);
    memcpy((void *)BIOS_DATA_SEG, bios_data_area, 0x100);
  }
#endif
  dosemu_vga_screenoff();
  disable_vga_card();
  restore_vga_regs(save_regs->regs, save_regs->xregs, save_regs->xregs16);
  restore_ext_regs(save_regs->xregs, save_regs->xregs16);
  if (save_regs->banks)
    restore_vga_mem(save_regs->mem, save_regs->banks);
  if (save_regs->release_video) {
    v_printf("Releasing video memory\n");
    free(save_regs->mem);
    save_regs->mem = NULL;
  }
  dosemu_vga_setpalvec(0, 256, save_regs->pal);
  restore_vga_regs(save_regs->regs, save_regs->xregs, save_regs->xregs16);
  restore_ext_regs(save_regs->xregs, save_regs->xregs16);
  v_printf("Permissions=%d\n", permissions);
  enable_vga_card();
  dosemu_vga_screenon();
  port_leave_critical_section();

  v_printf("Restore_vga_state complete\n");
}

static void pcivga_init(void)
{
  unsigned long base, size;
  int i;
  pciRec *pcirec;

  if (!config.pci)
    /* set up emulated r/o PCI config space, enough
       for VGA BIOSes to use */
    pcirec = pciemu_setup(PCI_CLASS_DISPLAY_VGA << 8);
  else
    pcirec = pcibios_find_class(PCI_CLASS_DISPLAY_VGA << 8, 0);
  if (!pcirec) {
    /* only set pci_video=0 if no PCI is available. Otherwise
       it's set by default */
    config.pci_video = 0;
    return;
  }
  v_printf("PCIVGA: found PCI device, bdf=%#x\n", pcirec->bdf);
  for (i = 0; i < 6; i++) {
    base = pcirec->region[i].base;
    if (!base)
      continue;
    size = pcirec->region[i].size;
    if (pcirec->region[i].type == PCI_BASE_ADDRESS_SPACE_IO) {
      emu_iodev_t io_device;
      v_printf("PCIVGA: found IO region at %#lx [%#lx]\n", base, size);

      /* register PCI VGA ports */
      io_device.irq = EMU_NO_IRQ;
      io_device.fd = -1;
      io_device.handler_name = "std port io";
      io_device.start_addr = base;
      io_device.end_addr = base + size;
      port_register_handler(io_device, PORT_FAST);
    } else if (base >= LOWMEM_SIZE + HMASIZE) {
      v_printf("PCIVGA: found MEM region at %#lx [%#lx]\n", base, size + 1);
      register_hardware_ram('v', base, size + 1);
    }
  }
}

static int vga_ioperm(unsigned base, int len)
{
  emu_iodev_t io_device;
  int err;
  err = set_ioperm(base, len, 1);
  if (err)
    error("ioperm() %x,%i failed\n", base, len);
  /* even if ioperm failed, we register handler that will forward
   * the requests to portserver */
  io_device.irq = EMU_NO_IRQ;
  io_device.fd = -1;
  io_device.handler_name = "std port io";
  io_device.start_addr = base;
  io_device.end_addr = base + len - 1;
  return port_register_handler(io_device, PORT_FAST);
}

static void set_console_video(void)
{
  /* warning! this must come first! the VT_ACTIVATES which some below
     * cause set_dos_video() and set_linux_video() to use the modecr
     * settings.  We have to first find them here.
     */
  int permtest = 0;

  if (config.mapped_bios) {
	permtest |= vga_ioperm(0x3b4, 0x3bc - 0x3b4 + 1);
	permtest |= vga_ioperm(0x3c0, 0x3df - 0x3c0 + 1);
  }
  permtest |= vga_ioperm(0x3bf, 1);
}

static int vga_initialize(void)
{
  Video_console = video_get("console");
  if (!Video_console) {
    error("console video plugin unavailable\n");
    return -1;
  }
  set_console_video();

  linux_regs.mem = NULL;
  dosemu_regs.mem = NULL;
  get_perm();

  /* defaults to override */
  save_ext_regs = save_ext_regs_dummy;
  restore_ext_regs = restore_ext_regs_dummy;
  set_bank_read = set_bank_read_dummy;
  set_bank_write = set_bank_write_dummy;
  ext_video_port_in = dummy_ext_video_port_in;
  ext_video_port_out = dummy_ext_video_port_out;

  if (config.pci_video)
    pcivga_init();

  switch (config.chipset) {
  case PLAINVGA:
    v_printf("Plain VGA in use\n");
    /* no special init here */
    break;
  case SVGALIB:
#ifdef USE_SVGALIB
  {
    void *handle = load_plugin("svgalib");
    void (*init_svgalib)(void);
    if (!handle) {
	error("svgalib unavailable\n");
	config.exitearly = 1;
	break;
    }
    init_svgalib = dlsym(handle, "vga_init_svgalib");
    init_svgalib();
    v_printf("svgalib handles the graphics\n");
  }
#else
    error("svgalib support is not compiled in, \"plainvga\" will be used.\n");
#endif
    break;
  case VESA:
    v_printf("Using the VESA BIOS for save/restore\n");
    /* init done later */
    break;

  default:
    v_printf("Unspecific VIDEO selected = 0x%04x\n", config.chipset);
  }

  linux_regs.video_name = "Linux Regs";
  /* the mode at /dev/mem:0x449 will be correct for both vgacon and vesafb.
     Other fbs and X can often restore themselves */
  load_file("/dev/mem", 0x449, &linux_regs.video_mode, 1);
  linux_regs.release_video = 0;

  dosemu_regs.video_name = "Dosemu Regs";
  dosemu_regs.video_mode = 3;
  dosemu_regs.regs[CRTI] = 0;
  dosemu_regs.regs[SEQI] = 0;
  dosemu_regs.regs[GRAI] = 0;

  /* don't release it; we're trying text mode restoration */
  dosemu_regs.release_video = 1;

  return 0;
}

static void vga_early_close(void)
{
  Video_console->early_close();
}

static void vga_close(void)
{
  Video_console->close();
  /* if the Linux console uses fbcon we can force
     a complete text redraw by doing round-trip
     vc switches; otherwise (vgacon) it doesn't hurt */
  if(!config.detach) {
    int arg;
    ioctl(console_fd, VT_OPENQRY, &arg);
    vt_activate(arg);
    vt_activate(scr_state.console_no);
    ioctl(console_fd, VT_DISALLOCATE, arg);
  }
  ioctl(console_fd, KIOCSOUND, 0);	/* turn off any sound */

  pthread_cancel(cpy_thr);
  pthread_join(cpy_thr, NULL);
  sem_destroy(&cpy_sem);
}

static void vga_vt_activate(int num)
{
  Video_console->vt_activate(num);
}

static struct video_system Video_graphics = {
   vga_initialize,
   vga_init,
   vga_post_init,
   vga_early_close,
   vga_close,
   NULL,
   NULL,             /* update_screen */
   NULL,
   NULL,
   .name = "graphics",
   vga_vt_activate,
};

/* init_vga_card - Initialize a VGA-card */
static int vga_init(void)
{
  vc_init();
  sem_init(&cpy_sem, 0, 0);
  pthread_create(&cpy_thr, NULL, vmemcpy_thread, &vmem_chunk_thr);
  pthread_setname_np(cpy_thr, "dosemu: vga");
  return 0;
}

static int vga_post_init(void)
{
  /* this function initialises vc switch routines */
  Video_console->late_init();

  if (!config.mapped_bios) {
    error("CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
    leavedos(23);
  }

  g_printf("INITIALIZING VGA CARD BIOS!\n");

  /* If there's a DOS TSR in real memory (say, univbe followed by loadlin)
     then don't call int10 here yet */
  if (!config.vbios_post) {
    unsigned int addr = SEGOFF2LINEAR(FP_SEG16(int_bios_area[0x10]),
				      FP_OFF16(int_bios_area[0x10]));
    if (addr < VBIOS_START || addr >= VBIOS_START + VBIOS_SIZE) {
      error("VGA: int10 is not in the BIOS (loadlin used?)\n"
	    "Try the vga_reset utility of svgalib or set $_vbios_post=(1) "
	    " in dosemu.conf\n");
      leavedos(23);
    }
  }

  if (config.chipset == VESA) {
    port_enter_critical_section(__func__);
    vesa_init();
    port_leave_critical_section();
  }

  /* fall back to 256K if not autodetected at this stage */
  if (config.gfxmemsize < 0) config.gfxmemsize = 256;
  v_printf("VGA: mem size %ld\n", config.gfxmemsize);

  save_vga_state(&linux_regs);

  config.vga = 1;
  set_vc_screen_page();
  return 0;
}

CONSTRUCTOR(static void init(void))
{
   register_video_client(&Video_graphics);
}

/* End of video/vga.c */
