/* $XConsortium: mga_driver.c /main/12 1996/10/28 05:13:26 kaleb $ */
/*
 * MGA Millennium (MGA2064W) with Ti3026 RAMDAC driver v.1.1
 *
 * This is almost empty at the moment.
 *
 */

#include <stdio.h>
#include <unistd.h>

#include "emu.h"        /* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "matrox.h"

void vga_init_matrox(void)
{
  ioperm(0xcf8, 8, 1);
}

