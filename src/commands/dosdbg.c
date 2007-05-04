/*
 * Written: 12/15/93 by Tim Bird (original code)
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************
 * File: DOSDBG.C
 *  Program for DOSEMU
 *
 * NOTES:
 *  DOSDBG supports the following commands:
 *  DOSDBG
 *    shows the current state of the debug variables
 *  DOSDBG string
 *    where string contains letters and + and - which allow the
 *    various debug options to be turned on and off
 *  DOSDBG HELP or DOSDBG ?
 *    show usage information for DOSDBG ???
 *
 * Modified: 03/14/95 by Kang-Jin Lee
 *  sync'd debug flags with pre0.53.49
 *  cosmetic changes
 *  03/27/95 minor changes
 * Modified: 05/14/95
 *  sync'd debug flags with 0.60.2+
 * Modified: 08/18/95 by PeaK
 *  added support for debug levels (they're used by PIC emulation)
 * Modified: 11/05/95 by Michael Beck
 *  sync'd debug flags with 0.60.4.4
 ***********************************************/


/* comment out if dosemu is compiled without X support */
#define X_SUPPORT

#include <dos.h>      /* geninterrupt and MK_FP */
#include <stdio.h>    /* printf                 */
#include <stdlib.h>   /* exit                   */
#include <string.h>
#include "detect.h"

#define DOS_HELPER_INT              0xE6 /* The interrupt we use */
#define DOS_HELPER_GET_DEBUG_STRING 0x10
#define DOS_HELPER_SET_DEBUG_STRING 0x11


typedef unsigned char uint8;
typedef unsigned int uint16;

#define CARRY_FLAG    1  /* carry bit in flags register */
#define CC_SUCCESS    0

#define MAX_DEBUG_STRING_LENGTH   100


void
Usage(void)
{
    printf("Usage: DOSDBG [string|HELP]\n");
    printf("If no string is specified, then DOSDBG will show the current debug settings.\n");
    printf("  If HELP is specified, then this screen is displayed.  Otherwise <string>\n");
    printf("  is parsed and used to change the current debug settings.\n\n");
    printf("<string> can contain letters, and the '+' and '-' characters.\n");
    printf("  Letters denote message classes from the following list:\n\n");
    printf("\
d  disk         R  disk Reads   W  disk Writes  D  dos\n");

#ifdef X_SUPPORT

    printf("\
C  cdrom        v  video        X  X support    k  keyboard\n\
i  port I/O     s  serial       m  mouse        #  interrupt\n\
p  printer      g  general      c  config       w  warnings\n\
h  hardware     I  IPC          E  EMS          x  XMS\n\
M  DPMI         n  network      P  pktdrv       r  PIC\n\
S  sound        T  I/O-trace    e  cpu-emu\n");

#else

    printf("\
C  cdrom        v  video        k  keyboard     i  port I/O\n\
s  serial       m  mouse        #  interrupt    p  printer\n\
g  general      c  config       w  warnings     h  hardware\n\
I  IPC          E  EMS          x  XMS          M  DPMI\n\
n  network      P  pktdrv       r  PIC          S  sound\n\
T  I/O-trace    e  cpu-emu\n");

#endif

    printf("a  all (shorthand for all of the above)\n\n");
    printf("Any classes following a '+', up until the end of string or a '-',\n");
    printf("  will be turned on.  Likewise, any classes following a '-', to the\n");
    printf("  end of string or a '+' will be turned off.\n\n");
    printf("The character 'a' acts like a string of all possible debugging classes,\n");
    printf("  so \"-a\" turns all message off, and \"+a-RW\" would turn all messages\n");
    printf("  on except for disk Read and Write messages.");
}


uint16 GetDebugString(char *debugStr)
{
    struct REGPACK preg;
    preg.r_es = FP_SEG(debugStr);
    preg.r_di = FP_OFF(debugStr);
    preg.r_ax = DOS_HELPER_GET_DEBUG_STRING;
    intr(DOS_HELPER_INT, &preg);
    return (preg.r_ax);
}


uint16 SetDebugString(char *debugStr)
{
    struct REGPACK preg;
    preg.r_es = FP_SEG(debugStr);
    preg.r_di = FP_OFF(debugStr);
    preg.r_ax = DOS_HELPER_SET_DEBUG_STRING;
    intr(DOS_HELPER_INT, &preg);
    return (preg.r_ax);
}


void
printDebugClass(char class, char value)
{
    switch (class) {

         case 'd':
              printf("d  disk       ");
              break;

         case 'R':
              printf("R  disk Reads ");
              break;

         case 'W':
              printf("W  disk Writes");
              break;

         case 'D':
              printf("D  dos        ");
              break;

         case 'C':
              printf("C  cdrom      ");
              break;

         case 'v':
              printf("v  video      ");
              break;

#ifdef X_SUPPORT
         case 'X':
              printf("X  X support  ");
              break;
#endif

         case 'k':
              printf("k  keyboard   ");
              break;

         case 'i':
              printf("i  port I/O   ");
              break;

         case 'T':
              printf("T  I/O-trace  ");
              break;

         case 's':
              printf("s  serial     ");
              break;

         case 'm':
              printf("m  mouse      ");
              break;

         case '#':
              printf("#  interrupt  ");
              break;

         case 'p':
              printf("p  printer    ");
              break;

         case 'g':
              printf("g  general    ");
              break;

         case 'c':
              printf("c  config     ");
              break;

         case 'w':
              printf("w  warnings   ");
              break;

         case 'h':
              printf("h  hardware   ");
              break;

         case 'I':
              printf("I  IPC        ");
              break;

         case 'E':
              printf("E  EMS        ");
              break;

         case 'x':
              printf("x  XMS        ");
              break;

         case 'M':
              printf("M  DPMI       ");
              break;

         case 'n':
              printf("n  network    ");
              break;

         case 'P':
              printf("P  pktdrv     ");
              break;

         case 'r':
              printf("r  PIC        ");
              break;

         case 'S':
              printf("S  sound      ");
              break;

         case 'e':
              printf("e  cpu-emu    ");
              break;

         default:
              printf("%c  unknown    ", class);
              break;

    }

    switch (value) {

         case '-':
              printf(" OFF    ");
              break;

         case '+':
              printf(" ON     ");
              break;

         case '0':  /* why is GCC the only compiler allowing */
         case '1':  /* intervals in switch statement? -sigh- */
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
   case '9':
              printf(" LVL=%c  ", value);
              break;

         default:
              printf(" UNKNOWN");
              break;

    }

}


void
ShowDebugString(void)
{
    uint16 ccode;
    char debugStr[MAX_DEBUG_STRING_LENGTH];

    int i;
    char class, value;

    debugStr[0] = 0;

    ccode = GetDebugString(debugStr);

    if (ccode == CC_SUCCESS && strlen(debugStr)) {
         printf("Current debug message class settings:\n");
         for (i = 0; i < strlen(debugStr); i += 2) {
              value = debugStr[i];
              class = debugStr[i + 1];
              printDebugClass(class, value);
              if (i % 6 == 4)
                printf("\n");
              else
                printf("     "); /* shortened */
         }
    }
    else {
         if (strlen(debugStr) == 0) {
              printf("Cannot retrieve debug string from this version of DOSEMU\n");
         }
         else {
              printf("Error %04X getting debug string.\n", ccode);
         }
    }
}



uint16 ParseAndSetDebugString(char *userDebugStr)
{
    uint16 ccode;

    char debugStr[MAX_DEBUG_STRING_LENGTH];

    int i, j, k;
    char class, value;

#ifdef X_SUPPORT
    const char debugAll[] = "-d-R-W-D-C-v-X-k-i-T-s-m-#-p-g-c-w-h-I-E-x-M-n-P-r-S-e";
#else
    const char debugAll[] = "-d-R-W-D-C-v-k-i-T-s-m-#-p-g-c-w-h-I-E-x-M-n-P-r-S-e";
#endif

    /*expand the user string to a canonical form
     *by inserting a value before each class letter, and
     *by replacing 'a' with all values */
    j = 0;

    value = '+';        /* assume ON for start of string */

    for (i = 0; i < strlen(userDebugStr); i++) {
         class = userDebugStr[i];

         /* change sense of value if required */
         if (class == '+' || class == '-' || (class >= '0' && class <= '9')) {
              value = class;
              continue;
         }

         if (class == 'a') {
              strcpy(debugStr, debugAll);
              j = strlen(debugStr);
              for (k = 0; k < j; k += 2)
             debugStr[k] = value;
              continue;
         }

    debugStr[j++] = value;
    debugStr[j++] = class;

    }

    debugStr[j] = 0;

    ccode = SetDebugString(debugStr);

    if (ccode == 0) {
         printf("Debug settings were adjusted...\n");
         ShowDebugString();
    }

    return (ccode);

}


int main(int argc, char **argv)
{
    uint16 ccode;

    if (!is_dosemu()) {
         printf("This program requires DOSEMU to run, aborting\n");
         exit(1);
    }

    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
         ShowDebugString();
         exit(0);
    }

    if (strcmpi(argv[1], "HELP") == 0 || argv[1][0] == '?') {
         Usage();
         exit(0);
    }

    if (argc == 2) {
         ccode = ParseAndSetDebugString(argv[1]);
    }
    else {
         /* if wrong number of parameters, force a usage screen */
         ccode = 1;
    }

    if (ccode) {
         Usage();
    }

MainExit:
    return (ccode);

}
