/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Robert Sanders, started 3/1/93
 *
 * Hans Lermen, moved 'mapping' to generic mapping drivers, 2000/02/02
 *
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <termios.h>			/* for mouse.h */

#include "memory.h"
#include "emu.h"
#include "termio.h"
#include "dosio.h"
#include "mouse.h"
#include "int.h"

#include "pic.h"
#include "bitops.h"
#include "mapping.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

/* my test shared memory IDs */
static struct {
  int 			/* normal mem if idt not attached at 0x100000 */
  hmastate;		/* 1 if HMA mapped in, 0 if idt mapped in */
}
sharedmem;

/* Correct HMA size is 64*1024 - 16, but IPC seems not to like this
   hence I would consider that those 16 missed bytes get swapped back
   and forth and may cause us grief - a BUG */
#define HMASIZE (64*1024)
#define HMAAREA (u_char *)0x100000

/* Used for all IPC HMA activity */
static void *shm_hma;
static void *shm_wrap;
static caddr_t ipc_return;

void HMA_MAP(int HMA)
{
  E_printf("Entering HMA_MAP with HMA=%d\n", HMA);

  if (munmap_mapping(MAPPING_HMA | MAPPING_SHM, HMAAREA, HMASIZE) < 0) {
    E_printf("HMA: Detaching HMAAREA unsuccessful: %s\n", strerror(errno));
    leavedos(48);
  }
  E_printf("HMA: detached at %p\n", HMAAREA);
  if (HMA){
    ipc_return = mmap_mapping(MAPPING_HMA | MAPPING_SHM, HMAAREA,
				HMASIZE, 0, shm_hma);
    if ((int)ipc_return == -1) {
      E_printf("HMA: Mapping HMA master %p to HMAAREA %p unsuccessful: %s\n",
	       shm_hma, HMAAREA, strerror(errno));
      leavedos(47);
    }
    E_printf("HMA: mapped %p to %p\n", shm_hma, ipc_return);
  }
  else {
    ipc_return = mmap_mapping(MAPPING_HMA | MAPPING_SHM, HMAAREA, HMASIZE, 0, shm_wrap);
    if ((int)ipc_return == -1) {
      E_printf("HMA: Mapping WRAP to HMAAREA unsuccessful: %s\n", strerror(errno));
      leavedos(47);
    }
    E_printf("HMA: mapped WRAP (%p) to %p\n", shm_wrap, ipc_return);
  }
}

void
set_a20(int enableHMA)
{
  if (sharedmem.hmastate == enableHMA)
    g_printf("WARNING: redundant %s of A20!\n", enableHMA ? "enabling" :
	  "disabling");

  /* to turn the A20 on, one must unmap the "wrapped" low page, and
   * map in the real HMA memory. to turn it off, one must unmap the HMA
   * and make FFFF:xxxx addresses wrap to the low page.
   */
  HMA_MAP(enableHMA);

  sharedmem.hmastate = enableHMA;
}

void HMA_init(void)
{
  /* initially, no HMA */
  sharedmem.hmastate = 0;

  open_mapping(MAPPING_HMA);
  shm_hma = alloc_mapping(MAPPING_HMA | MAPPING_SHM, HMASIZE, 0);
  if (!shm_hma) {
    E_printf("HMA: Initial HMA mapping unsuccessful: %s\n", strerror(errno));
    E_printf("HMA: Do you have IPC in the kernel?\n");
    leavedos(43);
  }
  shm_wrap = alloc_mapping(MAPPING_HMA | MAPPING_SHM, HMASIZE, 0);
  if (!shm_wrap) {
    E_printf("HMA: Initial WRAP at 0x0 mapping unsuccessful: %s\n", strerror(errno));
    leavedos(43);
  }


  /* attach regions: page 0 (idt) at address 0 and code space.
   */
  memcpy(shm_wrap, 0, 0x10000);
  ipc_return = mmap_mapping(MAPPING_HMA | MAPPING_SHM, (void *)0, HMASIZE, 0, shm_wrap);
  if ((int)ipc_return == -1) {
    E_printf("HMA: Mapping to 0 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  E_printf("HMA: mapped WRAP %p to %p\n", shm_wrap, ipc_return);

  ipc_return = mmap_mapping(MAPPING_HMA | MAPPING_SHM, HMAAREA, HMASIZE, 0, shm_wrap);
  if ((int)ipc_return == -1) {
    E_printf("HMA: Adding mapping to 0x100000 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  E_printf("HMA: mapped WRAP %p to %p\n", shm_wrap, ipc_return);
}


void
hma_exit(void)
{
  close_mapping(MAPPING_HMA);
}
