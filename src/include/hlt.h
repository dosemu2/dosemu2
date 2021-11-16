#ifndef _EMU_HLT_H
#define _EMU_HLT_H

#if 0
#include "emu_defs.h"
#endif
#include "bios.h"

typedef void (*emu_hlt_func)(Bit16u offs, void *arg, void *arg2);
#define HLT_ARG(n) void*_dummy,void*n

typedef struct emu_hlt_s {
  emu_hlt_func  func;
  const char   *name;
  int           len;
  void         *arg;
  int           ret;
} emu_hlt_t;

#define HLT_INITIALIZER { NULL, NULL, 1, NULL, HLT_RET_NORMAL }

enum { HLT_RET_NONE, HLT_RET_FAIL, HLT_RET_NORMAL, HLT_RET_SPECIAL };

extern void *hlt_init(int size);
extern Bit16u hlt_register_handler(void *arg, emu_hlt_t handler);
extern int hlt_unregister_handler(void *arg, Bit16u start_addr);
extern int hlt_handle(void *arg, Bit16u offs, void *arg2);

#endif /* _EMU_HLT_H */
