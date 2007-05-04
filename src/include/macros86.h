/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef __MACROS86_H_
#define __MACROS86_H_

/* NOTE: this is run through GCC with -traditional
 *       so, keep the hash (#) at column 1 !!!
 */
#ifndef __ASM__
#  define __ASM__
#endif
#include "config.h"

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

#if GASCODE16
#  define CODE32 addr32
#  define INDIRECT_PTR(x) *x
#else
#  define CODE32 addr32;
#  define INDIRECT_PTR(x) x
#endif /* GASCODE16 */

/* NOTE: we need the following only for prefixing string instructions
 *       (such as lodsb, lodsw, lodsl, e.t.c.) and if GASCODE16 == 0
 *       In all other cases prefixing can be imbedded in the operant itself
 */
#ifndef SEGES
#define SEGES .byte	0x26;
#endif
#define SEGCS .byte	0x2e;
#define SEGSS .byte	0x36;
#define SEGDS .byte	0x3e;
#define SEGFS .byte	0x64;
#define SEGGS .byte	0x65;


#endif
