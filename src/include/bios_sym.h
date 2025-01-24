#ifndef BIOS_SYM_H
#define BIOS_SYM_H

#include "memory.h"

struct bios_symbol_entry {
  dosaddr_t addr;
  const char *name;
};
extern struct bios_symbol_entry bios_symbol[];
extern int bios_symbol_num;

#endif
