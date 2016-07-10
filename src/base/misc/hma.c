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

unsigned char *ext_mem_base = NULL;
int a20;

void HMA_MAP(int HMA)
{
  int ret;
  /* destroy simx86 memory protections first */
  e_invalidate_full(HMAAREA, HMASIZE);
  /* Note: MAPPING_HMA is magic, dont be confused by src==dst==HMAAREA here */
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
  if (config.ext_mem) {
    ext_mem_base = mmap_mapping(MAPPING_EXTMEM | MAPPING_SCRATCH, -1,
      EXTMEM_SIZE, PROT_READ | PROT_WRITE);
    x_printf("Ext.Mem of size 0x%x at %p\n", EXTMEM_SIZE, ext_mem_base);
    memcheck_addtype('x', "Extended memory (HMA+XMS)");
    memcheck_reserve('x', LOWMEM_SIZE, HMASIZE + EXTMEM_SIZE);
  }
}


void
hma_exit(void)
{
}

void extmem_copy(unsigned dst, unsigned src, unsigned len)
{
  unsigned slen, dlen, clen, copied = 0;
  unsigned s, d, edge = LOWMEM_SIZE + HMASIZE;
  unsigned char *ps = NULL;

  while ((clen = len - copied) > 0) {
    slen = clen;
    s = src + copied;
    if (s >= edge)
      ps = &ext_mem_base[s - edge];
    else if (s + slen > edge)
      slen = edge - s;

    dlen = clen;
    d = dst + copied;
    if (d < edge && d + dlen > edge)
      dlen = edge - d;

    clen = min(slen, dlen);
    x_printf("INT15: copy 0x%x bytes from %#x to %#x%s\n",
      clen, s, d, clen != len ? " (split)" : "");
    if (d < edge) {
      if (s < edge)
	memmove_dos2dos(d, s, clen);
      else
	memcpy_2dos(d, ps, clen);
      e_invalidate(d, clen);
    } else {
      unsigned char *pd = &ext_mem_base[d - edge];
      if (s < edge)
	memcpy_2unix(pd, s, clen);
      else
	memmove(pd, ps, clen);
    }
    copied += clen;
  }
}
