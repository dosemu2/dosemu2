/* dos emulator, Matthias Lautner */

#include <linux/vm86.h>

#define _regs	vm86s.regs

#define us unsigned short
#define SEG_ADR(type, seg, reg) type((int)(*(us *)&_regs.seg<<4)\
				 + *(us *)&_regs.e##reg)
#define LO(reg)		(int)(*(unsigned char *)&_regs.e##reg)
#define HI(reg)		(int)(*((unsigned char *)&_regs.e##reg +1))
#define CF 0x01
#define ZF 0x40
#define SF 0x80
#define IF 0x200

extern struct vm86_struct vm86s;
extern int error;

void show_regs(void);
int ext_fs(int, char *, char *, int);

