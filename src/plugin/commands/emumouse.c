/*
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/********************************************
 * EMUMOUSE.C
 *
 * Adjusts settings of the internal mouse driver of DOSEMU
 *
 * Written by Kang-Jin Lee (lee@tengu.in-berlin.de) at 3/20/95
 * based on MOUSE.C in dosemu0.53.51 by Alan Hourihane.
 *                                      <alanh@fairlite.demon.co.uk>
 *
 * Note:
 *   There are some incompatible changes in arguments
 *   - Arguments to switch between 2 and 3 button mouse mode has
 *     changed from m to 2 and p to 3
 *   - Mouse speed value ranges from 1 to 255 mickeys
 *   - Arguments are case insensitiv
 * Modified:
 *   - Added argument h for help screen
 ********************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "builtins.h"

#include "emumouse.h"

#define printf  com_printf

#define MHLP(rin, rout) ({rin.x.ax = DOS_HELPER_MOUSE_HELPER; \
	com_int86(DOS_HELPER_INT, &rin, &rout);})


union com_REGS regs;


/* Show help screen */
static int usage(void)
{
  printf("Usage: EMUMOUSE [option]\n");
  printf("Utility to control the internal mousedriver of DOSEMU\n\n");
  printf("Options:\n");
  printf("  r       - Reset IRET.\n");
  printf("  i       - Inquire current configuration.\n");
  printf("  2       - Select 2 button mouse mode (Microsoft).\n");
  printf("  3       - Select 3 button mouse mode (e.g. Mousesystems, PS2).\n");
  printf("  x value - Set horizontal speed.\n");
  printf("  y value - Set vertical speed.\n");
  printf("  a       - Ignore application's setting of vert/horz speed settings.\n");
  printf("  b       - Take application's settings of vert/horz speed settings.\n");
  printf("  h       - Display this screen.\n");
  printf("  Mx val  - Set minimum internal horizontal resolution.\n");
  printf("  My val  - Set minimum internal vertical resolution.\n\n");
  printf("Valid range for \"value\" is from 1 (fastest) to 255 (slowest).\n\n");
  printf("Example: EMUMOUSE r a x 10 i\n");
  printf("reset, fixed speed setting, set horizontal speed to 10 and show current\n");
  printf("  configuraton of the mouse\n\n");

  return (1);
}


/* Detect internal mouse driver of Linux */
static int detectInternalMouse(void)
{
  regs.x.bx = 0x00ff;
  MHLP(regs, regs);
  if (regs.x.ax == 0xffff) {
    printf("ERROR! Internal driver option not set, enable internal driver\n");
    printf("       in dosemu.conf ($_mouse_dev option).\n");
    return 0;
  }
  return 1;
}


int emumouse_main(int argc, char *argv[])
{
  int i, value;

  if (argc == 1)
    return usage();

  switch (argv[1][0]) {
    case '?':
    case 'H':
    case 'h':
      return usage();
      break;
    default:
      break;
  }


  if (!detectInternalMouse()) return 1;

  i = 1;

  while (i < argc)
  {
    switch(argv[i][0]) {

      case 'A':
      case 'a':
	printf("Fixed speed settings.\n");
	regs.h.cl = 0x0001;
	regs.x.bx = 0x0006;
	MHLP(regs, regs);
	break;

      case 'B':
      case 'b':
	printf("Variable speed settings.\n");
	regs.h.cl = 0x0000;
	regs.x.bx = 0x0006;
	MHLP(regs, regs);
	break;

      case 'R':
      case 'r':
	printf("Resetting iret.\n");
	regs.x.bx = 0x0000;
	MHLP(regs, regs);
	break;

      case 'I':
      case 'i':
	printf("\nCurrent mouse setting:\n");
	regs.x.bx = 0x0003;
	MHLP(regs, regs);
	if (regs.h.bh == 0x10)
	  printf("  2 button mouse mode (Microsoft)\n");
	else
	  printf("  3 button mouse mode (e.g. Mousesystems, PS2)\n");
	printf  ("  Horizontal Speed (X) - %d\n", regs.h.cl);
	printf  ("  Vertical Speed   (Y) - %d\n", regs.h.ch);
	printf  ("  Speed Setting        - %s\n\n", regs.h.dl ? "fixed" : "variable");
	regs.x.bx = 0x0007;
	MHLP(regs, regs);
	if (regs.x.ax == 0) {
	  printf  ("  Minimum Internal Horizontal Resolution - %d\n", regs.x.cx);
          printf  ("  Minimum Internal Vertical Resolution   - %d\n", regs.x.dx);
	}
	break;

      case '3':
	printf("Selecting 3 button mouse mode (e.g. Mousesystems, PS2).\n");
	regs.x.bx = 0x0002;
	MHLP(regs, regs);
	if (regs.h.al == 0xff) {
	  printf("ERROR! Cannot select 3 button mouse mode, \"emulate3buttons\" not set\n");
	  printf("       in /etc/dosemu.conf, try e.g.\n");
	  printf("       'mouse { ps2 device /dev/mouse internaldriver emulate3buttons }'\n");
	  return (1);
	}
	break;

      case '2':
	printf("Selecting 2 button mouse mode (Microsoft).\n");
	regs.x.bx = 0x0001;
	MHLP(regs, regs);
	break;

      case 'Y':
      case 'y':
	i++;
	if (i == argc) {
	  printf("ERROR! No value for \"y\" found.\n");
	  return(1);
	}
	value = atoi(argv[i]);
	printf("Selecting vertical speed to %d.\n", value);
	regs.x.bx = 0x0004;
	regs.h.cl = value;
	MHLP(regs, regs);
	if (regs.x.ax == 1) {
	  printf("ERROR! Selected speed is out of range. Unable to set speed.\n");
	  return(1);
	}
	break;

      case 'X':
      case 'x':
	i++;
	if (i == argc) {
	  printf("ERROR! No value for \"x\" found.\n");
	  return(1);
	}
	value = atoi(argv[i]);
	printf("Selecting horizontal speed to %d.\n", value);
	regs.x.bx = 0x0005;
	regs.h.cl = value;
	MHLP(regs, regs);
	if (regs.x.ax == 1) {
	  printf("ERROR! Selected speed is out of range. Unable to set speed.\n");
	  return(1);
	}
	break;

      case 'M':
      case 'm':
        switch (argv[i][1]) {
	case 'X':
	case 'x':
	  i++;
	  if (i == argc) {
	    printf("ERROR! No value for \"Mx\" found.\n");
	    return(1);
	  }
	  value = atoi(argv[i]);
	  printf("Selecting minimum horizontal resolution to %d.\n", value);
	  regs.x.bx = 0x0007;
	  MHLP(regs, regs);
	  if (regs.x.ax == 1) {
	    printf("ERROR! Setting minimum horizontal resolution not supported.\n");
	    break;
	  }
	  regs.x.bx = 0x0008;
	  regs.x.cx = value;
	  MHLP(regs, regs);
	  if (regs.x.ax == 1) {
	    printf("ERROR! Setting minimum horizontal resolution not supported.\n");
	    break;
	  }
	  break;

	case 'Y':
	case 'y':
	  i++;
	  if (i == argc) {
	    printf("ERROR! No value for \"My\" found.\n");
	    return(1);
	  }
	  value = atoi(argv[i]);
	  printf("Selecting minimum vertical resolution to %d.\n", value);
	  regs.x.bx = 0x0007;
	  MHLP(regs, regs);
	  if (regs.x.ax == 1) {
	    printf("ERROR! Setting minimum vertical resolution not supported.\n");
	    break;
	  }
	  regs.x.bx = 0x0008;
	  regs.x.dx = value;
	  MHLP(regs, regs);
	  if (regs.x.ax == 1) {
	    printf("ERROR! Setting minimum vertical resolution not supported.\n");
	    break;
	  }
	  break;

	default: 
          printf("ERROR! Unknown option \"%s\".\n\n", argv[i]);
	  return usage();
	  /* never reached */
	  break;
	}
	break;

      default:
	printf("ERROR! Unknown option \"%s\".\n\n", argv[i]);
	return usage();
	/* never reached */
	break;

    } /* switch */
    i++;
  } /* while */
  return (0);
}
