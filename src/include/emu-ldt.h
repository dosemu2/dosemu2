/*
 * emu-ldt.h
 *
 * Definitions of structures used with the modify_ldt system call.
 */

#ifndef _EMU_LDT_H
#define _EMU_LDT_H

#if 1
#if LX_KERNEL_VERSION < 2001000
  #include <linux/ldt.h>
#else
  #include <asm/ldt.h>
#endif
#else
/* Maximum number of LDT entries supported. */
#define LDT_ENTRIES	8192
/* The size of each LDT entry. */
#define LDT_ENTRY_SIZE	8
#endif

#define MAX_SELECTORS	LDT_ENTRIES

/*
 *  00-15	limit 15-0
 *  16-39	base  23-0
 *  40-43	type
 *  44		DT
 *  45-46	DPL
 *  47		P
 *  48-51	limit 19-16
 *  52		vf
 *  53		r
 *  54		DB
 *  55		G
 *  56-63	base 31-24
 *
 * limit mask 000f0000.0000ffff
 * base  mask ff0000ff.ffff0000
 * flags mask 00f0ff00.00000000
 *
 */


#endif
