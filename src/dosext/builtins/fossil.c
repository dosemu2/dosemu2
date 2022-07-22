/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>

#include "builtins.h"
#include "doshelpers.h"
#include "fossil.h"

#define printf com_printf
#define fprintf com_fprintf
#undef stderr
#define stderr com_stderr
#define intr com_intr

int fossil_main(int argc, char **argv)
{
  struct REGPACK r = REGPACK_INIT;

  // get old interrupt vector to pass to Dosemu
  r.r_ax = 0x3514;
  intr(0x21, &r);		// ES:BX now has old ISR addr

  // Check if we are good to load
  r.r_ax = (DOS_HELPER_SERIAL_HELPER | (DOS_SUBHELPER_SERIAL_FOSSIL_INIT << 8)) & 0xffff;
  // ES:BX still set
  r.r_cx = DOS_VERSION_SERIAL_FOSSIL;
  intr(DOS_HELPER_INT, &r);	// DS:DX set by helper

  // set new interrupt vector (helper returned the ISR address in DS:DX)
  if (!(r.r_flags & 1)) { // carry clear
    r.r_ax = 0x2514;
    // DS:DX still set
    intr(0x21, &r);

    fprintf(stderr, "dosemu FOSSIL emulator: installed.\n");
    return 0;
  }

  switch (r.r_bx) {
    case DOS_ERROR_SERIAL_ALREADY_INSTALLED:
      fprintf(stderr, "dosemu FOSSIL emulator: already installed.\n");
      return 1;

    case DOS_ERROR_SERIAL_CONFIG_DISABLED:
      fprintf(stderr, "dosemu FOSSIL emulator: disabled in config.\n");
      return 2;

    case DOS_ERROR_SERIAL_FOSSIL_VERSION:
      fprintf(stderr, "dosemu FOSSIL emulator: version mismatch, update FOSSIL.COM.\n");
      return 2;

    default:
      fprintf(stderr, "dosemu FOSSIL emulator: unknown error.\n");
      return 2;
  }
}
