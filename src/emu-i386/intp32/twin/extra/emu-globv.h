
#ifndef _EMU_GLOBV_H
#define _EMU_GLOBV_H

/*
 * Reserve a specific register globally for the CPU interpreter's
 * Program Counter. While not the cleanest thing to do, this move
 * nevertheless increases the cpuemu speed _a_lot_.
 * Be careful to include this file only in the right places, not
 * to use other variables called 'PC', and use -fomit-frame-pointer
 * if you are going to reserve %ebp.
 *
 * Global register variables must be defined BEFORE any function
 * prototype; so this has normally to be the first included file.
 */

#include "cpu-emu.h"

#if defined(__i386__) && defined(USE_GLOBAL_EBP) && !defined(__PIC__)
register unsigned char *PC asm("%ebp");
#else
#define _PC	PC
#endif

#endif
