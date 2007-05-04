/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* dos emulator, Matthias Lautner */
#ifndef TERMIO_H
#define TERMIO_H
/* Extensions by Robert Sanders, 1992-93
 *
 *
 */

/*
 * These are the defines for the int that is dosemu's kbd flagger (kbd_flags)
 * These just happen to coincide with the format of the GET EXTENDED SHIFT
 * STATE of int16h/ah=12h, and the low byte to GET SHIFT STATE int16h,ah=02h
 *
 * A lot of the RAW VC dosemu stuff was copied from the Linux kernel (0.98pl6)
 * I hope Linux dosen't mind
 *
 * Robert Sanders 12/12/92
 */

#include "extern.h"


struct screen_stat {
  int console_no,		/* our console number */
   vt_allow,			/* whether to allow VC switches */
   vt_requested;		/* whether one was requested in forbidden state */

  int current;			/* boolean: is our VC current? */

  int curadd;			/* row*80 + col */
  int dcurgeom;			/* msb: start, lsb: end */
  int lcurgeom;			/* msb: start, lsb: end */

  int mapped,			/* whether currently mapped */
   pageno;			/* current mapped text page # */

  int dorigin;			/* origin in DOS */
  int lorigin;

  caddr_t virt_address;		/* current map address in DOS memory */
  off_t phys_address;		/* current map address in /dev/mem memory */

  int old_modecr, new_modecr;
};

#endif /* TERMIO_H */
