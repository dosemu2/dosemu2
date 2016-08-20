#define USE_MODEMU 1

#ifndef __ASSEMBLER__

char *modemu_init(int num);
void modemu_done(int num);
void modemu_update(int num);

#endif
