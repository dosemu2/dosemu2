/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <sys/utsname.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "config.h"
#include "bios.h"
#include "int.h"
#include "timers.h"
#include "termio.h"
#include "video.h"
#include "vc.h"
#include "mouse.h"
#include "port.h"
#include "joystick.h"
#ifdef USING_NET
#include "pktdrvr.h"
#include "ipx.h"
#endif
#include "bitops.h"
#include "pic.h"
#include "cmos.h"
#include "dma.h"
#include "xms.h"
#include "shared.h"
#include "iodev.h"
#include "priv.h"
#include "doshelpers.h"
#include "speaker.h"

#include "keyb_clients.h"
#include "keyb_server.h"

#include "mapping.h"


#if 0
static inline void dbug_dumpivec(void)
{
  int i;

  for (i = 0; i < 256; i++) {
    int j;

    dbug_printf("%02x %08lx", i, ((unsigned long *) 0)[i << 1]);
    for (j = 0; j < 8; j++)
      dbug_printf(" %02x", ((unsigned char *) (BIOSSEG * 16 + 16 * i))[j]);
    dbug_printf("\n");
  }
}
#endif

/*
 * DANG_BEGIN_FUNCTION dosemu_banner
 *
 * description:
 *  Setup the call stack to draw the dosemu banner
 * 
 * DANG_END_FUNCTION
 */
static void
dosemu_banner(void)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, LWORD(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 4;
  LWORD(cs) = Banner_SEG;
  LWORD(eip) = Banner_OFF;
}

/*
 * DANG_BEGIN_FUNCTION stdio_init
 *
 * description:
 *  Initialize stdio, open debugging output file if user specified one
 * 
 * DANG_END_FUNCTION
 */
void stdio_init(void)
{
  setbuf(stdout, NULL);

  if(dbg_fd) {
    warn("DBG_FD already set\n");
    return;
  }

  if(config.debugout)
  {
    dbg_fd=fopen(config.debugout,"w");
    if(!dbg_fd) {
      error("can't open \"%s\" for writing debug file\n",
	      config.debugout);
      exit(1);
    }
    free(config.debugout);
    config.debugout = NULL;
  }
  else
  {
    dbg_fd=0;
    warn("No debug output file specified, debugging information will not be printed");
  }
  sync();  /* for safety */
}

/*
 * DANG_BEGIN_FUNCTION time_setting_init
 *
 * description:
 *  Beats me
 * 
 * DANG_END_FUNCTION
 */
void time_setting_init(void)
{
  initialize_timers();
}

/*
 * DANG_BEGIN_FUNCTION timer_interrupt_init
 *
 * description:
 *  Tells the OS to send us periodic timer messages
 * 
 * DANG_END_FUNCTION
 */
void timer_interrupt_init(void)
{
  struct itimerval itv;
  int delta;

  delta = (config.update / TIMER_DIVISOR);
  /* Check that the kernel actually supports such a frequency - we
   * can't go faster than jiffies with setitimer()
   */
  if (((delta/1000)+1) < (1000/HZ)) {
    c_printf("TIME: FREQ too fast, using defaults\n");
    config.update = 54925; config.freq = 18;
    delta = 54925 / TIMER_DIVISOR;
  }

  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = delta;
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = delta;
  c_printf("TIME: using %d usec for updating ALRM timer\n", delta);

  setitimer(ITIMER_REAL, &itv, NULL);
}

/*
 * DANG_BEGIN_FUNCTION hardware_setup
 *
 * description:
 *  Initialize any leftover hardware. 
 * 
 * DANG_END_FUNCTION
 */
void hardware_setup(void)
{
  /* PIC init */
  pic_seti(PIC_IRQ0, timer_int_engine, 0, NULL);  /* do_irq0 in pic.c */
  pic_unmaski(PIC_IRQ0);
  pic_request(PIC_IRQ0);  /* start timer */
  pic_seti(PIC_IRQ1, do_irq1, 0, NULL); /* do_irq1 in dosio.c   */
  pic_unmaski(PIC_IRQ1);
  pic_seti(PIC_IRQ8, rtc_int8, 0, NULL);
  pic_unmaski(PIC_IRQ8);
  if (mouse_is_ps2()) {
    pic_seti(PIC_IMOUSE, DOSEMUMouseEvents, 0, do_mouse_irq);
    pic_unmaski(PIC_IMOUSE);
  }
#ifdef USING_NET
#ifdef IPX
  pic_seti(PIC_IPX, do_irq, 0, IPXCallRel);
  pic_unmaski(PIC_IPX);
#endif
  pic_seti(PIC_NET, pkt_check_receive_quick, 0, NULL);
  pic_unmaski(PIC_NET);
#endif
  
  /* DMA Init */
  /* dma_init(); - Currently in dev_list */

  g_printf("PIC,mouse,IPX initialized\n");
}

/*
 * DANG_BEGIN_FUNCTION map_video_bios
 *
 * description:
 *  Map the video bios into main memory
 * 
 * DANG_END_FUNCTION
 */
void map_video_bios(void)
{
  uint32_t int_area[256];
  uint16_t seg, off;
  int i;

  v_printf("Mapping VBIOS = %d\n",config.mapped_bios);

  if (config.mapped_bios) {
    if (config.vbios_file) {
      warn("WARN: loading VBIOS %s into mem at 0x%X (0x%X bytes)\n",
	   config.vbios_file, VBIOS_START, VBIOS_SIZE);
      load_file(config.vbios_file, 0, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else if (config.vbios_copy) {
      warn("WARN: copying VBIOS from /dev/mem at 0x%X (0x%X bytes)\n",
	   VBIOS_START, VBIOS_SIZE);
      load_file("/dev/mem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else {
      warn("WARN: copying VBIOS file from /dev/mem\n");
      load_file("/dev/mem", VBIOS_START, (char *) VBIOS_START, VBIOS_SIZE);
    }

    /* copy graphics characters from system BIOS */
    load_file("/dev/mem", GFX_CHARS, (char *) GFX_CHARS, GFXCHAR_SIZE);

    memcheck_addtype('V', "Video BIOS");
    memcheck_reserve('V', VBIOS_START, VBIOS_SIZE);
    video_ints[0x1f] = 1;
    video_ints[0x43] = 1;    
    if (!config.vbios_post) {
      load_file("/dev/mem", 0, (char *)int_area, sizeof(int_area));
      for (i = 0; i < 256; i++) {
        seg = int_area[i] >> 16;
        off = int_area[i] & 0xffff;
        g_printf("int0x%x was 0x%04x:0x%04x\n", i, seg, off);
        if (seg == VBIOS_START >> 4) {
          g_printf("Setting int0x%x to 0x%04x:0x%04x\n", i, seg, off);
          SETIVEC(i, seg, off);
          video_ints[i] = 1;
        } else video_ints[i] = 0;
      }
      g_printf("Now initialising 0x40:49-66, 84-8a and a8-ab\n");
      load_file("/dev/mem", (0x40 << 4) + 0x49, (char *)(0x40 << 4) + 0x49, 0x67-0x49);
      load_file("/dev/mem", (0x40 << 4) + 0x84, (char *)(0x40 << 4) + 0x84, 0x8b-0x84);
      load_file("/dev/mem", (0x40 << 4) + 0xa8, (char *)(0x40 << 4) + 0xa8, 0xac-0xa8);
    }
  }
}

/*
 * DANG_BEGIN_FUNCTION map_hardware_ram
 *
 * description:
 *  Initialize the hardware direct-mapped pages
 * 
 * DANG_END_FUNCTION
 */
void map_hardware_ram(void)
{
  int i, j;
  unsigned int addr, size;

  if (!config.must_spare_hardware_ram)
    return;
  if (!can_do_root_stuff) {
    fprintf(stderr, "can't use hardware ram in low feature (non-suid root) DOSEMU\n");
    return;
  }
  i = 0;
  do {
    if (config.hardware_pages[i++]) {
      j = i - 1;
      while (config.hardware_pages[i])
	i++;		/* NOTE: last byte is always ZERO */
      addr = HARDWARE_RAM_START + (j << 12);
      size = (i - j) << 12;
      if ((int)mmap_mapping(MAPPING_INIT_HWRAM | MAPPING_KMEM, (caddr_t) addr,
		size, PROT_READ | PROT_WRITE, (caddr_t) addr) == -1) {
	error("mmap error in map_hardware_ram %s\n", strerror (errno));
	return;
      }
      g_printf("mapped hardware ram at 0x%05x .. 0x%05x\n", addr, addr+size-1);
    }
  } while (i < sizeof (config.hardware_pages) - 1);
}

/*
 * DANG_BEGIN_FUNCTION map_custom_bios
 *
 * description:
 *  Setup the dosemu amazing custom BIOS, quietly overwriting anything
 *  was copied there before. Do not overwrite graphic fonts!
 * 
 * DANG_END_FUNCTION
 */
static inline void map_custom_bios(void)
{
  u_char *ptr;
  u_long n;

  n = (u_long)bios_f000_endpart1 - (u_long)bios_f000;
  ptr = (u_char *) (BIOSSEG << 4);
  memcpy(ptr, bios_f000, n);

  n = (u_long)bios_f000_end - (u_long)bios_f000_part2;
  ptr = (u_char *) (BIOSSEG << 4) + ((u_long)bios_f000_part2 - (u_long)bios_f000);
  memcpy(ptr, bios_f000_part2, n);
}

/* 
 * DANG_BEGIN_FUNCTION bios_mem_setup
 * 
 * description:
 *  Set up all memory areas as would be present on a typical i86 during
 * the boot phase.
 *
 * DANG_END_FUNCTION
 */
static inline void bios_mem_setup(void)
{
  int b;

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, game card and the
   * configured number of floppy disks
   */
  CONF_NFLOP(configuration, config.fdisks);
  CONF_NSER(configuration, config.num_ser);
  CONF_NLPT(configuration, config.num_lpt);
  if (mouse_is_ps2())
    configuration |= CONF_MOUSE;

  /* 
   * real BIOS' don't seem to set this bit but setting it can't do any harm
   * and improves joystick detection for the game, Alley Cat, anyway :)
   */
  if (joy_exist ()) configuration |= CONF_GAME;

  if (config.mathco)
    configuration |= CONF_MATHCO;

  g_printf("CONFIG: 0x%04x    binary: ", configuration);
  for (b = 15; b >= 0; b--)
    g_printf("%s%s", (configuration & (1 << b)) ? "1" : "0", (b%4) ? "" : " ");
  g_printf("\n");

  bios_configuration = configuration;
  bios_memory_size   = config.mem_size;	/* size of memory */

}

/* 
 * DANG_BEGIN_FUNCTION memory_init
 * 
 * description:
 *  Set up all memory areas as would be present on a typical i86 during
 * the boot phase.
 *
 * DANG_END_FUNCTION
 */
void memory_init(void)
{
  static int first_call = 1;
  /* first_call is a truly awful hack to keep some of these procedures from
     being called twice.  It should be fixed sometime. */

  map_custom_bios();           /* map the DOSEMU bios */
  setup_interrupts();          /* setup interrupts */

  {
    /* update boot drive in Banner-code */
    u_char *ptr;

    ptr = (u_char *)((BIOSSEG << 4) + ((long)bios_f000_bootdrive - (long)bios_f000));
    *ptr = config.hdiskboot ? 0x80 : 0;
  }

#ifdef IPX
  if (config.ipxsup)
    InitIPXFarCallHelper();    /* TRB - initialize IPX in boot() */
#endif

#ifdef USING_NET
  if (config.pktdrv)
    pkt_init(0x60);              /* Install the new packet driver interface */
#endif

  bios_mem_setup();            /* setup values in BIOS area */

  /* 
   * The banner helper actually gets called *after* the VGA card
   * is initialized (if it is) because we set up a return chain:
   *      init_vga_card -> dosemu_banner -> 7c00:0000 (boot block)
   */

  if (config.dosbanner)
    dosemu_banner();

  if (config.vga) {
    g_printf("INITIALIZING VGA CARD BIOS!\n");
    init_vga_card();
  }

  if (first_call) {
    ems_init();                /* initialize ems */
    xms_init();                /* initialize xms */
    shared_memory_init();
  }
  first_call = 0;
}

/* 
 * DANG_BEGIN_FUNCTION device_init
 * 
 * description:
 *  Calls all initialization routines for devices (keyboard, video, serial,
 *    disks, etc.)
 *
 * DANG_END_FUNCTION
 */
void device_init(void)
{
  /* check whether we are running on the console */
  check_console();

  scr_state_init();
#ifdef HAVE_KEYBOARD_V1
  if (!keyb_server_init()) {
    error("can't init keyboard server\n");
    config.exitearly = 1;
  }
  if (!keyb_client_init()) {
    error("can't open keyboard client\n");
    config.exitearly = 1;
  }
#endif
   
  if (!config.vga)
    config.vbios_post = 0;

  video_config_init();
  if (config.console && (config.speaker == SPKR_EMULATED)) {
    register_speaker((void *)console_fd,
		     console_speaker_on, console_speaker_off);
  }
  iodev_init();
  printer_init();
  disk_init();
}

/* 
 * DANG_BEGIN_FUNCTION low_mem_init
 * 
 * description:
 *  Initializes the lower 1Meg via mmap & sets up the HMA region
 *
 * DANG_END_FUNCTION
 */
void low_mem_init(void)
{
  char *result;

  g_printf ("DOS memory area being mapped in\n");
  result = mmap_mapping(MAPPING_INIT_LOWRAM | MAPPING_SCRATCH, 0,
  		0x100000, PROT_EXEC | PROT_READ | PROT_WRITE, 0);
  if (result != NULL)
    {
      perror ("anonymous mmap");
      config.exitearly = 1;
    }
}

/*
 * DANG_BEGIN_FUNCTION version_init
 * 
 * description:
 *  Find version of OS running and set necessary global parms.
 *
 * DANG_END_FUNCTION
 */
void version_init(void) {
  struct utsname unames;
  char version[80];

  uname((struct utsname *)&unames);
#ifdef __linux__
  strcpy(version,unames.release);
  running_kversion = atoi(strtok(version,".")) *1000000;
  running_kversion += atoi(strtok(NULL,".")) *1000;
  running_kversion += atoi(strtok(NULL,"."));
#endif
  

#if 0 /* hmm, the below _allway_ has been hit in the past,
       * because: unames.release[2] > 1 always is true
       * (unames.release is a string like "2.0.28") --Hans
       */
  if (unames.release[0] > 0 ) {
    if ((unames.release[2] == 1  && unames.release[3] > 1 ) ||
         unames.release[2] > 1 ) {
    }
  }
#endif

   
}

void print_version(void)
{
  struct utsname unames;
    
  uname((struct utsname *)&unames);
  warn("DOSEMU-%s is coming up on %s version %s\n", VERSTR, unames.sysname, unames.release);
}
