/* $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/mga/mga.h,v 3.8.2.2 1997/05/25 05:06:50 dawes Exp $ */
/*
 * MGA Millennium (MGA2064W) functions
 *
 * This is almost empty at the moment.
 *
 */

#ifndef MATROX_H
#define MATROX_H

#define PCI_CHIP_MGA2085	0x0518
#define PCI_CHIP_MGA2064	0x0519
#define PCI_CHIP_MGA1064	0x051A

extern void vga_init_matrox(void);

#endif		/* MATROX_H */
