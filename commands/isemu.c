/***********************************************
 * Isemu.c
 * Written: 03/14/95 by Kang-Jin Lee
 *  - Detects and shows Dosemu major version number
 *
 * Modified 04/03/95
 *  - works only with Dosemu 0.53.58+
 *  - Detects and shows Dosemu version, subversion
 *    and patchlevel
 *  - returns errorlevel = VERSION * 100 + SUBLEVEL
 *    Dosemu-1.2.4 returns errorlevel 102
 ***********************************************/

#include <dos.h>    /* geninterrupt */
#include <stdio.h>  /* printf       */

#define DOS_HELPER_INT           0xE6
#define DOS_HELPER_DOSEMU_CHECK  0x00

/* returns non-zero version number if DOSEMU is loaded */
main(void)
{
    unsigned int version, sublevel, patchlevel;

    _AL = DOS_HELPER_DOSEMU_CHECK;
    geninterrupt(DOS_HELPER_INT);

    /* check for signature in AX */
    if (_AX == 0xaa55) {
         version = _BX / 0x100;
         sublevel = _BX % 0x100;
         patchlevel = _CX;
         printf("Detected: Dosemu %d.%d.%d\n", version, sublevel, patchlevel);
         return (version * 100 + sublevel);
    }
    else {
         printf("Dosemu not detected\n");
         return (0);
    }
}
