#ifndef DOSIPC_H
#define DOSIPC_H

#include <sys/ipc.h>
#include <sys/shm.h>

void memory_setup(void), set_a20(int);

/* do, while necessary because if { ... }; precludes an else, while
 * do { ... } while(); does not. ugly */
extern void HMA_MAP(int); 
#ifdef NEW_PIC
extern void do_irq1(void);
#endif

extern inline void set_keyboard_bios();
extern inline void insert_into_keybuffer();

#endif /* DOSIPC_H */
