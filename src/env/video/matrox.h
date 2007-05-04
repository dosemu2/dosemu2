/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/mga/mga.h,v 3.8.2.2 1997/05/25 05:06:50 dawes Exp $ */
/*
 * MGA Millennium (MGA2064W) functions
 *
 * Copyright 1996 The XFree86 Project, Inc.
 *
 * Authors
 *		Dirk Hohndel
 *			hohndel@XFree86.Org
 *		David Dawes
 *			dawes@XFree86.Org
 *
 * Changes for DOSEMU:
 *		9/97 Alberto Vignani <vignani@tin.it>
 *
 */

#ifndef MATROX_H
#define MATROX_H

/* some defines are missing in 2.0.xx */
#ifndef PCI_VENDOR_ID_MATROX
#define PCI_VENDOR_ID_MATROX		0x102B
#endif
#ifndef PCI_DEVICE_ID_MATROX_MGA_2
#define PCI_DEVICE_ID_MATROX_MGA_2	0x0518
#endif
#ifndef PCI_DEVICE_ID_MATROX_MIL
#define PCI_DEVICE_ID_MATROX_MIL	0x0519
#endif
#ifndef PCI_DEVICE_ID_MATROX_MYS
#define PCI_DEVICE_ID_MATROX_MYS	0x051A
#endif
#ifndef PCI_DEVICE_ID_MATROX_MGA_IMP
#define PCI_DEVICE_ID_MATROX_MGA_IMP	0x0d10
#endif

#define PCI_CHIP_MGA2085	0x0518
#define PCI_CHIP_MGA2064	0x0519
#define PCI_CHIP_MGA1064	0x051A

#define INREG8(addr) *(volatile u_char *)(MGAMMIOBase + (addr))
#define INREG16(addr) *(volatile u_short *)(MGAMMIOBase + (addr))
#define OUTREG8(addr, val) *(volatile u_char *)(MGAMMIOBase + (addr)) = (val)
#define OUTREG16(addr, val) *(volatile u_short *)(MGAMMIOBase + (addr)) = (val)
#define OUTREG(addr, val) *(volatile unsigned int *)(MGAMMIOBase + (addr)) = (val)

#define MGAISBUSY() (INREG8(MGAREG_Status + 2) & 0x01)
#define MGAWAITFIFO() while(INREG16(MGAREG_FIFOSTATUS) & 0x100);
#define MGAWAITFREE() while(MGAISBUSY());
#define MGAWAITFIFOSLOTS(slots) while ( ((INREG16(MGAREG_FIFOSTATUS) & 0x3f) - (slots)) < 0 );

/* MGA registers in PCI config space */
#define PCI_MGA_INDEX		0x44
#define PCI_MGA_DATA		0x48

#define RAMDAC_OFFSET		0x3c00


extern void vga_init_matrox(void);

extern void matrox_set_bank_read(u_char bank);
extern void matrox_set_bank_write(u_char bank);

extern void matrox_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void matrox_restore_ext_regs(u_char xregs[], u_short xregs16[]);

#endif		/* MATROX_H */
