/***********************************************
 * Isemu.c
 * Written: 03/14/95 by Kang-Jin Lee
 *  - Detects and shows Dosemu major version number
 *
 * Modified 04/03/95
 *  - Works only with Dosemu 0.53.58+
 *  - Detects and shows Dosemu version, subversion
 *    and patchlevel
 *  - Returns errorlevel = VERSION * 100 + SUBLEVEL
 *    Dosemu-1.2.4 returns errorlevel 102
 *
 * Modified 10/30/95
 *  - Checks BIOS release date before checking for
 *    Dosemu
 ***********************************************/


#include <dos.h>    /* geninterrupt */
#include <stdio.h>  /* printf       */
#include <string.h>


#define DOS_HELPER_INT           0xE6
#define DOS_HELPER_DOSEMU_CHECK  0x00
#define DOSEMU_BIOS_DATE         "02/25/93"


int check_date(void)
{
    int i;
    unsigned char far *pos;
    unsigned char b_date[8];

    pos = MK_FP(0xF000, 0xFFF5);

    for (i = 0; i < 8; i++)
         b_date[i] = pos[i];

    if (strncmp(b_date, DOSEMU_BIOS_DATE, 8) == 0)
         return (1);
    else
         return (0);
}

/* returns non-zero version number if DOSEMU is loaded */
int main(void)
{
    unsigned int version, sublevel, patchlevel;

    if (check_date() == 0)
    {
         printf("Dosemu BIOS signature not detected\n");
         return (0);
    }

    _AL = DOS_HELPER_DOSEMU_CHECK;
    geninterrupt(DOS_HELPER_INT);

    /* check for signature in AX */
    if (_AX == 0xaa55)
    {
         version = _BX / 0x100;
         sublevel = _BX % 0x100;
         patchlevel = _CX;
         printf("Detected: Dosemu %d.%d.%d\n", version, sublevel, patchlevel);
         return (version * 100 + sublevel);
    }
    else
    {
         printf("Dosemu not detected\n");
         return (0);
    }
}
