/*
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Derived from cirrus.h */

extern void vga_init_svgalib(void);

#ifdef SVGALIB_C
/* these prototypes are from vga.h, but this file cannot be included,
    due to a conflict with svgalib's vga.h */
#include "extern.h"
EXTERN void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
EXTERN void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
EXTERN void (*set_bank_read) (unsigned char bank);
EXTERN void (*set_bank_write) (unsigned char bank);
#endif

extern void svgalib_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void svgalib_restore_ext_regs(u_char xregs[], u_short xregs16[]);
extern void svgalib_setbank(unsigned char bank);

