/***********************************************
 * speed.c
 * Written: 12/27/95 by Kang-Jin Lee
 *  - Read/write "hogthreshold" from inside Dosemu
 ***********************************************/


#include <dos.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include "emulib.h"


void usage(void)
{
    printf("\n");
    printf("speed [threshold]\n\n");
    printf("threshold: hogthreshold value, 0 to 255\n\n");
    printf("Example: speed 30\n");
    printf("Sets hoghthreshold to 30.\n\n");
    printf("When no argument given, it reports current hogthreshold setting.\n\n");
    printf("Dosemu can give up CPU cycles when it is idle. Threshold is used to\n");
    printf("control how friendly Dosemu should be.\n\n");
    printf("For lower CPU load try\n\n");
    printf("    i486:  0.25 x CPU clock\n");
    printf("    i586:  0.17 x CPU clock\n\n\n");
    printf("If you found a value that suits your needs, consider to set it in\n");
    printf("/etc/dosemu.conf, then you don't have to use this program to set\n");
    printf("the threshold.\n");
    exit (1);
}


int main(int argc, char *argv[])
{
    unsigned int hogthreshold_value;

    if (check_emu() == 0)
    {
        printf("Dosemu not detected.\n");
        printf("This program is intended for Dosemu only.\n");
        return (0);
    }

    if (argc > 2)
        usage();

    if (argc == 2)
    {
        if (sscanf(argv[1], "%u", &hogthreshold_value) == 1)
		{
			_AX = 0x12;
			_BX = hogthreshold_value;
			geninterrupt(DOS_HELPER_INT);
		}
        else
            usage();
    }

    _AX = 0x28;
    geninterrupt(DOS_HELPER_INT);
    printf("Current hogthreshold value = %u\n", _BX);
    return (0);
}
