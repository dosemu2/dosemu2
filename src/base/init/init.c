/* 
 * $Id: init.c,v 1.4 1995/05/06 16:26:13 root Exp root $
 */
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#ifdef __linux__
#include <linux/config.h>
#include <linux/utsname.h>
#endif
#ifdef __NetBSD__
#include <sys/utsname.h>
#define new_utsname utsname
#endif

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "config.h"
#include "kversion.h"
#include "bios.h"
#include "int.h"
#include "timers.h"
#include "termio.h"
#include "video.h"
#include "vc.h"
#include "mouse.h"
#include "port.h"
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

#ifdef NEW_KBD_CODE
#include "keyb_clients.h"
#endif

#ifdef USING_NET
extern void pkt_check_receive_quick(void);
/* flag to activate use of pic by packet driver */
#define PICPKT 1
#endif

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
  }
  else
  {
    dbg_fd=0;
    warn("No debug output file specified, debugging information will not be printed");
  }
  sync();  /* for safety */
  setbuf(stdout, NULL);
}

/*
 * DANG_BEGIN_FUNCTION tmpdir_init
 *
 * description:
 *  Initialize the temporary directory
 * 
 * DANG_END_FUNCTION
 */
void tmpdir_init(void)
{
  /* create tmpdir */
  priv_off();
  mkdir(tmpdir, S_IREAD | S_IWRITE | S_IEXEC);
  priv_default();
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
  struct tm *tm;

  time(&start_time);
  tm = localtime((time_t *) &start_time);
  g_printf("Set date %02d.%02d.%02d\n", tm->tm_mday, tm->tm_mon+1, tm->tm_year);
  last_ticks = (tm->tm_hour * 60 * 60 + tm->tm_min * 60 + tm->tm_sec) * 19663/1080 /*18.206*/;
  check_date = tm->tm_year * 10000 + tm->tm_mon * 100 + tm->tm_mday;
  set_ticks(last_ticks);
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

  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = UPDATE / TIMER_DIVISOR;
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = UPDATE / TIMER_DIVISOR;
  k_printf("Used %d for updating timers\n", UPDATE / TIMER_DIVISOR);
  setitimer(TIMER_TIME, &itv, NULL);
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
  extern void  do_irq1(void);
  /* PIC init */
  pic_seti(PIC_IRQ0, timer_int_engine, 0);  /* do_irq0 in pic.c */
  pic_unmaski(PIC_IRQ0);
  pic_request(PIC_IRQ0);  /* start timer */
  pic_seti(PIC_IRQ1, do_irq1, 0); /* do_irq1 in dosio.c   */
  pic_unmaski(PIC_IRQ1);
  if (mice->intdrv || mice->type == MOUSE_PS2) {
    pic_seti(PIC_IMOUSE, DOSEMUMouseEvents, 0);
    pic_unmaski(PIC_IMOUSE);
  }
#ifdef USING_NET
#ifdef IPX
  pic_seti(PIC_IPX, IPXCallRel, 0);
  pic_unmaski(PIC_IPX);
#endif
#ifdef PICPKT
  pic_seti(PIC_NET, pkt_check_receive_quick, 0x61);
#else
  pic_seti(PIC_NET, pkt_check_receive_quick, 0);
#endif
  pic_unmaski(PIC_NET);
#endif
  
  /* DMA Init */
  /* dma_init(); - Currently in dev_list */

  g_printf("Hardware initialized\n");
}

/*
 * DANG_BEGIN_FUNCTION map_video_bios
 *
 * description:
 *  Map the video bios into main memory
 * 
 * DANG_END_FUNCTION
 */
static inline void map_video_bios(void)
{
  extern int load_file(char *name, int foffset, char *mstart, int msize);

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
static inline void map_hardware_ram(void)
{
  int i, j;
  unsigned int addr, size;
#ifdef __NetBSD__
  extern int errno;
#endif

  if (!config.must_spare_hardware_ram)
    return;
  open_kmem ();
  i = 0;
  do {
    if (config.hardware_pages[i++]) {
      j = i - 1;
      while (config.hardware_pages[i])
	i++;		/* NOTE: last byte is always ZERO */
      addr = HARDWARE_RAM_START + (j << 12);
      size = (i - j) << 12;
      if (mmap((caddr_t) addr, (size_t) size, PROT_READ | PROT_WRITE, 
	       MAP_SHARED | MAP_FIXED, mem_fd, addr) == (caddr_t) -1) {
	error("ERROR: mmap error in map_hardware_ram %s\n", strerror (errno));
	close_kmem();
	return;
      }
      g_printf("mapped hardware ram at 0x%05x .. 0x%05x\n", addr, addr+size-1);
    }
  } while (i < sizeof (config.hardware_pages) - 1);
  close_kmem();
}

/*
 * DANG_BEGIN_FUNCTION map_custom_bios
 *
 * description:
 *  Setup the dosemu amazing custom BIOS
 * 
 * DANG_END_FUNCTION
 */
static inline void map_custom_bios(void)
{
  extern void bios_f000(), bios_f000_end();
#if 0
  FILE   *f;
#endif
  u_char *ptr;

  ptr = (u_char *) (BIOSSEG << 4);
  memcpy(ptr, bios_f000, (u_long)bios_f000_end - (u_long)bios_f000);
  if (!config.mapped_sbios) {
    memset((char *) 0xffff0, 0xF4, 16);

    strncpy((char *) 0xffff5, "02/25/93", 8);   /* set our BIOS date */
    *(char *) 0xffffe = 0xfc;   /* model byte = IBM AT */
  }
  /* set up BIOS exit routine (we have *just* enough room for this) */
  ptr = (u_char *) 0xffff0;
  *ptr++ = 0xb8;                /* mov ax, 0xffff */
  *ptr++ = 0xff;
  *ptr++ = 0xff;
  *ptr++ = 0xcd;                /* int 0xe6 */
  *ptr++ = 0xe6;

#if 0 /* for debugging only */
  f = fopen("/tmp/bios","w");
  fprintf(stderr,"opened /tmp/bios f=%p\n",f);
  fwrite(ptr,0x8000,2,f);
  fflush(f);
  fclose(f);
#endif
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

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, and the
   * configured number of floppy disks
   */
  CONF_NFLOP(configuration, config.fdisks);
  CONF_NSER(configuration, config.num_ser);
  CONF_NLPT(configuration, config.num_lpt);
  if ((mice->type == MOUSE_PS2) || (mice->intdrv))
    configuration |= CONF_MOUSE;

  if (config.mathco)
    configuration |= CONF_MATHCO;

  g_printf("CONFIG: 0x%04x    binary: ", configuration);
  for (b = 15; b >= 0; b--)
    g_printf("%s%s", (configuration & (1 << b)) ? "1" : "0", (b%4) ? "" : " ");
  g_printf("\n");

  bios_configuration = configuration;
  bios_memory_size   = config.mem_size;	/* size of memory */

#ifndef NEW_KBD_CODE
  /* The default 16-word BIOS key buffer starts at 0x41e */
#if 0
  KBD_Head =			/* key buf start ofs */
    KBD_Tail =			/* key buf end ofs */
    KBD_Start = 0x1e;		/* keyboard queue start... */
  KBD_End = 0x3e;		/* ...and end offsets from 0x400 */
#endif
  WRITE_WORD(KBD_HEAD, 0x1e); 	/* key buf start ofs */
  WRITE_WORD(KBD_TAIL, 0x1e);	/* key buf end ofs */
  WRITE_WORD(KBD_START, 0x1e);	/* keyboard queue start... */
  WRITE_WORD(KBD_END, 0x3e);	/* ...and end offsets from 0x400 */


  keybuf_clear();

  bios_ctrl_alt_del_flag = 0x0000;
  bios_keyboard_flags2   = 16;	/* 102-key keyboard */

  *OUTB_ADD = 1;                /* Set OUTB_ADD to 1 */
  *LASTSCAN_ADD = 1;
#endif
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

#if 0
  if( first_call == 0) {
#ifdef __linux__
    /* make interrupts vector read-write */
    mprotect((void *)(BIOSSEG<<4), 0x1000, PROT_WRITE|PROT_READ|PROT_EXEC);
    /* make memory area 0xf8000 to 0xfffff read write */
    mprotect((void *)(ROMBIOSSEG<<4), 0x8000, PROT_WRITE|PROT_READ|PROT_EXEC);
#endif
  }
#endif
  map_video_bios();            /* map the video bios */
  if (first_call)
    map_hardware_ram();        /* map the direct hardware ram */
  map_custom_bios();           /* map the DOSEMU bios */
  setup_interrupts();          /* setup interrupts */

  {
    /* update boot drive in Banner-code */
    extern void bios_f000_bootdrive(), bios_f000();
    u_char *ptr;

    ptr = (u_char *)((BIOSSEG << 4) + ((long)bios_f000_bootdrive - (long)bios_f000));
    *ptr = config.hdiskboot ? 0x80 : 0;
  }

#ifdef IPX
  if (config.ipxsup)
    InitIPXFarCallHelper();    /* TRB - initialize IPX in boot() */
#endif

#ifdef USING_NET
  pkt_init(0x60);              /* Install the new packet driver interface */
#endif

  bios_mem_setup();            /* setup values in BIOS area */
  cmos_reset();

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

  if (config.exitearly) {
    dbug_printf("Leaving DOS before booting\n");
    leavedos(0);
  }

  if (first_call) {
    ems_init();                /* initialize ems */
    xms_init();                /* initialize xms */
    shared_memory_init();
#ifndef NEW_KBD_CODE
    shared_keyboard_init();
#endif
  }
  first_call = 0;
#if 0
  /* make interrupts vector read-only */
  mprotect((void *)(BIOSSEG<<4), 0x1000, PROT_READ|PROT_EXEC);
  /* make memory area 0xf8000 to 0xfffff read only */
  mprotect((void *)(ROMBIOSSEG<<4), 0x8000, PROT_READ|PROT_EXEC);
#endif
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
  port_init();

#ifdef NEW_KBD_CODE
  /* check whether we are running on the console */
  check_console();

  if (!keyb_server_init()) {
    error("ERROR: can't init keyboard server\n");
    leavedos(19);
  }
  if (!keyb_client_init()) {
    error("ERROR: can't open keyboard client\n");
    leavedos(19);
  }
#else
  if (keyboard_init() != 0) {
    error("ERROR: can't open keyboard\n");
    leavedos(19);
  }
  keyboard_flags_init();
#endif
   
  if (!config.vga)
    config.allowvideoportaccess = 0;
 
  scr_state_init();
  video_config_init();
  iodev_init();
  mouse_init();
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
  result = mmap (NULL, 0x100000,
                 PROT_EXEC | PROT_READ | PROT_WRITE,
                 MAP_FIXED | MAP_PRIVATE | MAP_ANON,
                 -1, 0);
  if (result != NULL)
    {
      perror ("anonymous mmap");
      leavedos (1);
    }

  HMA_init();
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
  struct new_utsname unames;
  char version[80];

  uname((struct utsname *)&unames);
  warn("DOSEMU-%s is coming up on %s version %s\n", VERSTR, unames.sysname, unames.release);
#ifdef __linux__
  warn("Built for %d\n", KERNEL_VERSION);
#endif
#ifdef __NetBSD__
  warn("Built for %d\n", NETBSD_VERSION);
#endif

#ifdef __linux__
  strcpy(version,unames.release);
  running_kversion = atoi(strtok(version,".")) *1000000;
  running_kversion += atoi(strtok(NULL,".")) *1000;
  running_kversion += atoi(strtok(NULL,"."));
  use_sigio=FASYNC;
#endif
  

#if 0 /* hmm, the below _allway_ has been hit in the past,
       * because: unames.release[2] > 1 always is true
       * (unames.release is a string like "2.0.28") --Hans
       */
  if (unames.release[0] > 0 ) {
    if ((unames.release[2] == 1  && unames.release[3] > 1 ) ||
         unames.release[2] > 1 ) {
      use_sigio=FASYNC;
    }
  }
#endif

#ifndef NEW_KBD_CODE
  /* Next Check input */
  if (isatty(STDIN_FILENO)) {
    k_printf("STDIN is tty\n");
    config.kbd_tty = 0;
  } else {
    k_printf("STDIN not a tty\n");
    config.kbd_tty = 1;
  }
#endif
   
}


