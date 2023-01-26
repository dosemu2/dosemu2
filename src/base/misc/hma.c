/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
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
#include "bios.h"
#include "utilities.h"
#include "dos2linux.h"
#include "cpu-emu.h"

#define HMAAREA 0x100000

int a20;

static void HMA_MAP(int HMA)
{
  int ret;
  /* destroy simx86 memory protections first */
  e_invalidate_full(HMAAREA, HMASIZE);
  /* Note: MAPPING_HMA is magic, don't be confused by src==dst==HMAAREA here */
  off_t src = HMA ? HMAAREA : 0;
  x_printf("Entering HMA_MAP with HMA=%d\n", HMA);
  ret = alias_mapping(MAPPING_HMA, HMAAREA, HMASIZE,
    PROT_READ | PROT_WRITE | PROT_EXEC, LOWMEM(src));
  if (ret == -1) {
    x_printf("HMA: Mapping HMA to HMAAREA %#x unsuccessful: %s\n",
	       HMAAREA, strerror(errno));
    leavedos(47);
  }
  x_printf("HMA: mapped\n");
}

void set_a20(int enableHMA)
{
  if (!config.hma)
    return;
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
  int ret = alias_mapping(MAPPING_HMA, HMAAREA, HMASIZE,
    PROT_READ | PROT_WRITE | PROT_EXEC, LOWMEM(0));
  if (ret == -1) {
    error("HMA: Mapping HMA to HMAAREA %#x unsuccessful: %s\n",
	       HMAAREA, strerror(errno));
    config.exitearly = 1;
    return;
  }
  a20 = 0;
  memcheck_addtype('H', "HMA");
  memcheck_reserve('H', LOWMEM_SIZE, HMASIZE);
}


void
hma_exit(void)
{
}
