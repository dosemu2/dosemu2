/***********************************************
 * File: isemu.c
 *  Program for Linux Dosemu 0.53.49
 * Written: 03/14/95 by Kang-Jin Lee
 * Detects and shows Dosemu major version number
 *
 * Code shamelessly stolen from DOSDBG.C
 * written at 12/15/93 by Tim Bird
 ***********************************************/

#include <stdio.h>    /* printf       */
#include <dos.h>      /* geninterrupt */

typedef unsigned int uint16;

#define DOS_HELPER_INT        0xE6
#define DOS_HELPER_DOSEMU_CHECK   0x00

/* returns non-zero major version number if DOSEMU is loaded */
uint16 CheckForDOSEMU(void)
{
    _AL = DOS_HELPER_DOSEMU_CHECK;

    geninterrupt(DOS_HELPER_INT);

    /* check for signature in AX */
    if (_AX == 0xaa55) {
         return (_BX);
    }
    else {
         return (0);
    }
}


main(void)
{
    uint16 ccode;

    ccode = CheckForDOSEMU();

    if (ccode == 0) {
         printf("DOSEMU not detected\n");
    }
    else {
         printf("DOSEMU 0.%x detected\n", ccode);
    }

    return (ccode);

}
