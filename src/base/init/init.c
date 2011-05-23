/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "dpmi.h"
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
#include "lowmem.h"
#include "iodev.h"
#include "priv.h"
#include "doshelpers.h"
#include "cpu-emu.h"

#include "keyb_clients.h"
#include "keyb_server.h"

#include "mapping.h"

#if 0
static inline void dbug_dumpivec(void)
{
  int i;

  for (i = 0; i < 256; i++) {
    int j;

    dbug_printf("%02x %08x", i, ((unsigned int *) 0)[i << 1]);
    for (j = 0; j < 8; j++)
      dbug_printf(" %02x", ((unsigned char *) (BIOSSEG * 16 + 16 * i))[j]);
    dbug_printf("\n");
  }
}
#endif

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
  if (((delta/1000)+1) < (1000/sysconf(_SC_CLK_TCK))) {
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
 * DANG_BEGIN_FUNCTION map_video_bios
 *
 * description:
 *  Map the video bios into main memory
 * 
 * DANG_END_FUNCTION
 */
uint32_t int_bios_area[0x500/sizeof(uint32_t)];
void map_video_bios(void)
{
  v_printf("Mapping VBIOS = %d\n",config.mapped_bios);

  if (config.mapped_bios) {
    if (config.vbios_file) {
      warn("WARN: loading VBIOS %s into mem at %#x (%#X bytes)\n",
	   config.vbios_file, VBIOS_START, VBIOS_SIZE);
      load_file(config.vbios_file, 0, LINEAR2UNIX(VBIOS_START), VBIOS_SIZE);
    }
    else if (config.vbios_copy) {
      warn("WARN: copying VBIOS from /dev/mem at %#x (%#X bytes)\n",
	   VBIOS_START, VBIOS_SIZE);
      load_file("/dev/mem", VBIOS_START, LINEAR2UNIX(VBIOS_START), VBIOS_SIZE);
    }
    else {
      warn("WARN: copying VBIOS file from /dev/mem\n");
      load_file("/dev/mem", VBIOS_START, LINEAR2UNIX(VBIOS_START), VBIOS_SIZE);
    }

    /* copy graphics characters from system BIOS */
    load_file("/dev/mem", GFX_CHARS, LINEAR2UNIX(GFX_CHARS), GFXCHAR_SIZE);

    memcheck_addtype('V', "Video BIOS");
    memcheck_reserve('V', VBIOS_START, VBIOS_SIZE);
    if (!config.vbios_post || config.chipset == VESA)
      load_file("/dev/mem", 0, (unsigned char *)int_bios_area, sizeof(int_bios_area));
  }
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
  unsigned int ptr;
  u_long n;

  n = (u_long)bios_f000_endpart1 - (u_long)bios_f000;
  ptr = SEGOFF2LINEAR(BIOSSEG, 0);
  MEMCPY_2DOS(ptr, bios_f000, n);

  n = (u_long)bios_f000_end - (u_long)bios_f000_part2;
  ptr = SEGOFF2LINEAR(BIOSSEG, ((u_long)bios_f000_part2 - (u_long)bios_f000));
  MEMCPY_2DOS(ptr, bios_f000_part2, n);
  /* Initialize the lowmem heap that resides in a custom bios */
  lowmem_heap_init();
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
  map_custom_bios();           /* map the DOSEMU bios */
  setup_interrupts();          /* setup interrupts */
  bios_setup_init();
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
  pit_init();		/* for native speaker */
  video_config_init();	/* privileged part of video init */
  keyb_priv_init();
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
  unsigned char *result;

  open_mapping(MAPPING_INIT_LOWRAM);
  g_printf ("DOS+HMA memory area being mapped in\n");
  result = alloc_mapping(MAPPING_INIT_LOWRAM, LOWMEM_SIZE + HMASIZE, 0);

  if (result != NULL)
    {
#ifdef X86_EMULATOR
    if (config.cpuemu < 3) 
#endif
    {
      int err = errno;
#ifndef X86_EMULATOR
      perror ("LOWRAM mmap");
      if (err == EPERM || err == EACCES) {
	fprintf(stderr, "Cannot map low DOS memory (the first 640k).\n"
		"You can most likely avoid this problem by running\n"
		"sysctl -w vm.mmap_min_addr=0\n"
		"as root, or by changing the vm.mmap_min_addr setting in\n"
		"/etc/sysctl.conf or a file in /etc/sysctl.d/ to 0.\n");
      }
      leavedos(99);
#else
      if (err != EPERM && err != EACCES) {
	perror ("LOWRAM mmap");
        leavedos(99);
      }
      /* switch on vm86-only JIT CPU emulation to with non-zero base */
      config.cpuemu = 3;
      init_emu_cpu();
      c_printf("CONF: JIT CPUEMU set to 3 for %d86\n", vm86s.cpu_type);
      fprintf(stderr, "Using CPU emulation because vm.mmap_min_addr > 0.\n");
      if(dbg_fd)
        fprintf(stderr, "For more information, see %s.\n", config.debugout);
#endif
    }
#ifdef X86_EMULATOR
    if (errno == EPERM || errno == EACCES) {
      /* try 1MB+64K as base (may be higher if execshield is active) */
      /* the first mmap just reserves the memory */
      result = mmap_mapping(MAPPING_LOWMEM | MAPPING_SCRATCH,
			    (void *)(LOWMEM_SIZE + HMASIZE),
			    LOWMEM_SIZE + HMASIZE,
			    PROT_READ | PROT_WRITE | PROT_EXEC, 0);
      result = alias_mapping(MAPPING_INIT_LOWRAM, result,
			     LOWMEM_SIZE + HMASIZE,
			     PROT_READ | PROT_WRITE | PROT_EXEC, lowmem_base);
    }
    if (result == MAP_FAILED) {
      perror ("LOWRAM mmap");
      leavedos(99);
    }
    warn("WARN: using non-zero memory base address %p.\n"
	 "WARN: You can use the better-tested zero based setup using\n"
	 "WARN: sysctl -w vm.mmap_min_addr=0\n"
	 "WARN: as root, or by changing the vm.mmap_min_addr setting in\n"
	 "WARN: /etc/sysctl.conf or a file in /etc/sysctl.d/ to 0.\n",
	    result);
    *(unsigned char **)&mem_base = result;
#endif
  }

  /* keep conventional memory protected as long as possible to protect
     NULL pointer dereferences */
  mprotect_mapping(MAPPING_LOWMEM, result, config.mem_size * 1024, PROT_NONE);
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
  warn("DOSEMU-%s is coming up on %s version %s %s %s\n", VERSTR,
       unames.sysname, unames.release, unames.version, unames.machine);
  warn("Compiled with GCC version %d.%d", __GNUC__, __GNUC_MINOR__);
#ifdef __GNUC_PATCHLEVEL__
  warn(".%d",__GNUC_PATCHLEVEL__);
#endif
#ifdef i386
  warn(" -m32\n");
#else
  warn(" -m64\n");
#endif
}
