/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************
 * emulib.h
 * Written: 12/27/95 by Kang-Jin Lee
 * - Misc functions and defines for Dosemu DOS binaries
 ***********************************************/


#define DOS_HELPER_INT           0xE6
#define DOS_HELPER_DOSEMU_CHECK  0x00
#define DOSEMU_BIOS_DATE         "02/25/93"


/* Return non-zero value if Dosemu detected */
int check_emu(void);
