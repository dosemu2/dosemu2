/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _EMU_HLT_H
#define _EMU_HLT_H

#ifdef USE_HLT_CODE  /* currently not used at all */

#if 0
#include "emu_defs.h"
#endif
#include "bios.h"

/*
 * maximum number of halt handlers.
 * you can increase this to anything below 256 since an 8-bit handle
 * is used for each device
 */
#define MAX_HLT_HANDLERS 10

typedef void (* emu_hlt_func)(Bit32u offs);

typedef struct {
  emu_hlt_func  func;
  const char   *name;
  Bit32u        start_addr;
  Bit32u        end_addr;
  } emu_hlt_t;

extern void hlt_init(void);
extern int  hlt_register_handler(emu_hlt_t handler);
extern void hlt_handle(void);

#endif /* USE_HLT_CODE */

#endif /* _EMU_HLT_H */
