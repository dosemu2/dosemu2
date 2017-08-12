#ifndef __MACROS86_H_
#define __MACROS86_H_

#ifndef __ASM__
#  define __ASM__
#endif

.macro SIM_INT n,rtnlabel
        pushw %ds
        pushw $0
        popw %ds
        pushf
        pushw %cs
        pushw $\rtnlabel-bios_f000
        pushw (4*\n)+2
        pushw (4*\n)
        lret
        \rtnlabel: popw %ds
.endm

.macro FILL_OPCODE x,v
.rept \x
\v
.endr
.endm

#define FILL_BYTE(x,v) FILL_OPCODE x, .byte v
#define FILL_DWORD(x,v) FILL_OPCODE x, .int v
#define FILL_WORD(x,v) FILL_OPCODE x, .word v
#define FILL_SHORT(x,v) FILL_OPCODE x, .short v
#define FILL_LONG(x,v) FILL_OPCODE x, .long v

#endif
