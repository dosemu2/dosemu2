/***********************************************
 * emulib.c
 * Written: 12/27/95 by Kang-Jin Lee
 * - Misc functions for Dosemu DOS binaries
 ***********************************************/


#include <dos.h>
#include <string.h>
#include "emulib.h"


int check_date(void)
{
    int i;
    unsigned char far *pos;
    unsigned char bios_date[8];

    pos = MK_FP(0xF000, 0xFFF5);

    for (i = 0; i < 8; i++)
         bios_date[i] = pos[i];

    if (strncmp(bios_date, DOSEMU_BIOS_DATE, 8) == 0)
         return (1);
    else
         return (0);
}


/* returns non-zero version number if DOSEMU is loaded */
int check_emu(void)
{
    unsigned int version, sublevel;

    if (check_date() == 0)
         return (0);

    _AL = DOS_HELPER_DOSEMU_CHECK;
    geninterrupt(DOS_HELPER_INT);

    /* check for signature in AX */
    if (_AX == 0xaa55)
    {
         version = _BX / 0x100;
         sublevel = _BX % 0x100;
         return (version * 100 + sublevel);
    }
    else
         return (0);
}
