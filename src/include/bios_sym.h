#ifndef BIOS_SYM_H
#define BIOS_SYM_H

struct bios_symbol_entry {
  unsigned short off;
  const char *name;
};
extern struct bios_symbol_entry bios_symbol[];
extern int bios_symbol_num;

#endif
