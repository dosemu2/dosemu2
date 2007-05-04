/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* Derived from cirrus.h */

extern void vga_init_svgalib(void);

extern void svgalib_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void svgalib_restore_ext_regs(u_char xregs[], u_short xregs16[]);
extern void svgalib_setbank(unsigned char bank);

