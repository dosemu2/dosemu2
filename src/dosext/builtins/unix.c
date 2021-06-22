/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/************************************************
 * unix.c
 * Run unix commands from inside Dosemu
 *
 * Written by ???
 *
 ************************************************/


#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dos2linux.h"
#include "system.h"
#include "unix.h"

static int usage(void);

int unix_main(int argc, char **argv)
{
  char s[256];
  char c;
  int secure = 0;
  const char *getopt_string = "+ercsd:w";

  if (argc == 1 ||
      (argc == 2 && !strcmp (argv[1], "/?"))) {
    return usage();
  }

  optind = 0;		// glibc wants this to reser parser state
  while ((c = getopt(argc, argv, getopt_string)) != EOF) {
    /* Got a switch */
    switch (c) {
    case 's':
	secure++;
	break;
    case 'd':
      if (chdir(argv[2]) != 0) {
	com_printf("Chdir failed\n");
	return 1;
      }
      break;
    case 'w':
      if (getcwd(s, sizeof(s)) == NULL) {
	com_printf("Getcwd failed\n");
	return 1;
      }
      com_printf("%s\n", s);
      break;
    default:
      return usage();
    }
  }
  if (optind < argc) {
    if (secure) {
      if (argc - optind > 1) {
        com_printf("unix: arguments not allowed when -s is specified\n");
        return 1;
      }
      return run_unix_secure(argv[optind]);
    }
    return run_unix_command(argc - optind, argv + optind);
  }

  return 0;
}


static int usage(void)
{
  com_printf ("Usage: UNIX [FLAG COMMAND]\n\n");
  com_printf ("UNIX -d dir\n");
  com_printf ("  Set unix work dir to \"dir\".\n\n");
  com_printf ("UNIX -w\n");
  com_printf ("  Get current unix work dir.\n\n");
  com_printf ("UNIX command [arg1 ...]\n");
  com_printf ("  Execute the Linux command with the arguments given.\n\n");
  com_printf ("UNIX\n");
  com_printf ("UNIX /?\n");
  com_printf ("  show this help screen\n");

  return 0;
}
