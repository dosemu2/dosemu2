/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* Derived from cirrus.h */

extern void vga_init_sis(void);

extern void sis_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void sis_restore_ext_regs(u_char xregs[], u_short xregs16[]);
extern void sis_set_bank(u_char bank);
