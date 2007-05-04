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
      warn("WARN: loading VBIOS %s into mem at %p (%#X bytes)\n",
	   config.vbios_file, VBIOS_START, VBIOS_SIZE);
      load_file(config.vbios_file, 0, (char *) VBIOS_START, VBIOS_SIZE);
    }
    else if (config.vbios_copy) {
      warn("WARN: copying VBIOS from /dev/mem at %p (%#X bytes)\n",
	   VBIOS_START, VBIOS_SIZE);
      load_file("/dev/mem", (off_t)VBIOS_START, VBIOS_START, VBIOS_SIZE);
    }
    else {
      warn("WARN: copying VBIOS file from /dev/mem\n");
      load_file("/dev/mem", (off_t)VBIOS_START, VBIOS_START, VBIOS_SIZE);
    }

    /* copy graphics characters from system BIOS */
    load_file("/dev/mem", GFX_CHARS, (char *) GFX_CHARS, GFXCHAR_SIZE);

    memcheck_addtype('V', "Video BIOS");
    memcheck_reserve('V', (uintptr_t)VBIOS_START, VBIOS_SIZE);
    if (!config.vbios_post || config.chipset == VESA)
      load_file("/dev/mem", 0, (char *)int_bios_area, sizeof(int_bios_area));
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
  u_char *ptr;
  u_long n;

  n = (u_long)bios_f000_endpart1 - (u_long)bios_f000;
  ptr = (u_char *) (BIOSSEG << 4);
  memcpy(ptr, bios_f000, n);

  n = (u_long)bios_f000_end - (u_long)bios_f000_part2;
  ptr = (u_char *) (BIOSSEG << 4) + ((u_long)bios_f000_part2 - (u_long)bios_f000);
  memcpy(ptr, bios_f000_part2, n);
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
  char *result;

  open_mapping(MAPPING_INIT_LOWRAM);
  g_printf ("DOS+HMA memory area being mapped in\n");
  result = alloc_mapping(MAPPING_INIT_LOWRAM, LOWMEM_SIZE + HMASIZE, 0);
  /* keep conventional memory unmapped as long as possible to protect
     NULL pointer dereferences */
  munmap_mapping(MAPPING_LOWMEM, 0, config.mem_size * 1024);

  if (result != NULL)
    {
      perror ("LOWRAM mmap");
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
