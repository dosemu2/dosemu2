/*
 * (C) Copyright 2013 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef CHIPSET_H
#define CHIPSET_H

void chipset_init(void);

#define PIC0_EXTPORT_START 0x560
#define PIC0_VECBASE_PORT 0x560
#define PIC1_EXTPORT_START 0x570
#define PIC1_VECBASE_PORT 0x570
#define PICx_EXT_PORTS 1

#endif
