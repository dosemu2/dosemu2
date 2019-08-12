/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************
 * File: DOSDBG.C
 *  Program for DOSEMU
 * Written: 12/15/93 by Tim Bird
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


#include <stdio.h>    /* printf                 */
#include <stdlib.h>
#include <string.h>

#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "builtins.h"

#include "dosdbg.h"

#define printf  com_printf
#define FP_OFF(x) DOSEMU_LMHEAP_OFFS_OF(x)
#define FP_SEG(x) DOSEMU_LMHEAP_SEG


typedef unsigned char uint8;
typedef unsigned int uint16;

#define CARRY_FLAG    1  /* carry bit in flags register */
#define CC_SUCCESS    0

#define MAX_DEBUG_STRING_LENGTH   100


static void Usage(void)
{
    printf("Usage: DOSDBG [string|HELP]\n");
    printf("If no string is specified, then DOSDBG will show the current debug settings.\n");
    printf("  If HELP is specified, then this screen is displayed.  Otherwise <string>\n");
    printf("  is parsed and used to change the current debug settings.\n\n");
    printf("<string> can contain letters, and the '+' and '-' characters.\n");
    printf("  Letters denote specific message classes, except for 'a' which is\n");
    printf("  shorthand for all classes\n\n");
    printf("Any classes following a '+', up until the end of string or a '-',\n");
    printf("  will be turned on.  Likewise, any classes following a '-', to the\n");
    printf("  end of string or a '+' will be turned off.\n\n");
    printf("The character 'a' acts like a string of all possible debugging classes,\n");
    printf("  so \"-a\" turns all message off, and \"+a-RW\" would turn all messages\n");
    printf("  on except for disk Read and Write messages.");
}

static uint16 SetDebugString(char *debugStr)
{
    return SetDebugFlagsHelper(debugStr);
}

static void ShowDebugString(void)
{
  char s[1024];

  printf("Current debug message class settings:\n");

  if (!GetDebugInfoHelper(s, sizeof s))
    printf("Warning: output truncated!\n");

  printf("%s\n", s);
}

static uint16 ParseAndSetDebugString(char *userDebugStr)
{
    uint16 ccode;

    char debugStr[MAX_DEBUG_STRING_LENGTH];

    int i, j, k;
    char cls, value;

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
         cls = userDebugStr[i];

         /* change sense of value if required */
         if (cls == '+' || cls == '-' || (cls >= '0' && cls <= '9')) {
              value = cls;
              continue;
         }

         if (cls == 'a') {
              strcpy(debugStr, debugAll);
              j = strlen(debugStr);
              for (k = 0; k < j; k += 2)
             debugStr[k] = value;
              continue;
         }

    debugStr[j++] = value;
    debugStr[j++] = cls;

    }

    debugStr[j] = 0;

    ccode = SetDebugString(debugStr);

    if (ccode == 0) {
         printf("Debug settings were adjusted...\n");
         ShowDebugString();
    }

    return (ccode);

}


int dosdbg_main(int argc, char **argv)
{
    uint16 ccode;


    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
         ShowDebugString();
         return (0);
    }

    if (strequalDOS(argv[1], "HELP") || argv[1][0] == '?') {
         Usage();
         return (0);
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

    return (ccode);

}
