/*
 * emu-ldt.h
 *
 * Definitions of structures used with the modify_ldt system call.
 */

#ifndef _EMU_LDT_H
#define _EMU_LDT_H

#ifndef TWIN
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

#if defined(X86_EMULATOR)||defined(TWIN)

typedef struct {
	unsigned char *lpSelBase;	/* unscrambled segment base */
	unsigned long dwSelLimit;	/* unscrambled segment limit */
	void *hGlobal;		/* segment has to be a global object */
#ifndef TWIN32
	void *hReserved;	/* has to go when HGLOBAL becomes 32-bit */
#endif
	unsigned short w86Flags;	/* bytes 6 and 5 of the descriptor */
	unsigned char bSelType;		/* TRANSFER_... selector flags */
	unsigned char bModIndex;	/* index into module table */
} DSCR;

extern DSCR LDT[LDT_ENTRIES];

int emu_modify_ldt(int func, void *ptr, unsigned long bytecount);

#endif

#endif
