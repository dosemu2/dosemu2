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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "memory.h"
#include "emu.h"
#include "hma.h"
#include "mapping.h"

#define HMAAREA (u_char *)0x100000

void HMA_MAP(int HMA)
{
  caddr_t ipc_return;
  /* Note: MAPPING_HMA is magic, dont be confused by src==dst==HMAAREA here */
  u_char *src = HMA ? HMAAREA : 0;
  x_printf("Entering HMA_MAP with HMA=%d\n", HMA);

  if (munmap_mapping(MAPPING_HMA, HMAAREA, HMASIZE) < 0) {
    x_printf("HMA: Detaching HMAAREA unsuccessful: %s\n", strerror(errno));
    leavedos(48);
  }
  x_printf("HMA: detached at %p\n", HMAAREA);

  ipc_return = mmap_mapping(MAPPING_HMA, HMAAREA, HMASIZE,
    PROT_READ | PROT_WRITE | PROT_EXEC, src);
  if ((int)ipc_return == -1) {
    x_printf("HMA: Mapping HMA to HMAAREA %p unsuccessful: %s\n",
	       HMAAREA, strerror(errno));
    leavedos(47);
  }
  x_printf("HMA: mapped to %p\n", ipc_return);
}

void
set_a20(int enableHMA)
{
  if (a20 == enableHMA) {
    g_printf("WARNING: redundant %s of A20!\n", enableHMA ? "enabling" :
	  "disabling");
    return;
  }

  /* to turn the A20 on, one must unmap the "wrapped" low page, and
   * map in the real HMA memory. to turn it off, one must unmap the HMA
   * and make FFFF:xxxx addresses wrap to the low page.
   */
  HMA_MAP(enableHMA);

  a20 = enableHMA;
}

void HMA_init(void)
{
  /* initially, no HMA */
  HMA_MAP(0);
  a20 = 0;
}


void
hma_exit(void)
{
}
