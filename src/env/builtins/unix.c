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


#include "emu.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "memory.h"
#include "doshelpers.h"
#include "dos2linux.h"
#include "builtins.h"
#include "redirect.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"

#include "system.h"
#include "unix.h"

#define SYSTEM_COM_COMPAT 1

static int usage (void);
static int send_command (int argc, char **argv);

int unix_main(int argc, char **argv)
{
  char s[256];
  char c;
  const char *getopt_string = "ercsd:w";

  if (argc == 1 ||
      (argc == 2 && !strcmp (argv[1], "/?"))) {
    return usage();
  }

  optind = 0;		// glibc wants this to reser parser state
  while ((c = getopt(argc, argv, getopt_string)) != EOF) {
    /* Got a switch */
    switch (c) {
#if SYSTEM_COM_COMPAT
    case 'e':
    case 'r':
    case 'c':
    case 's':
      com_printf("UNIX: option %s is deprecated, use system.com instead\n",
	    argv[1]);
      return system_main(argc, argv);
#endif
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
  if (optind < argc)
    return send_command(argc - optind, argv + optind);

  return 0;
}


static int usage (void)
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

  return (1);
}


static int send_command(int argc, char **argv)
{
    char command_line[256];
    int i;

    command_line[0] = 0;

    for (i = 0; i < argc; i++)
    {
        strcat(command_line, argv[i]);
        strcat(command_line, " ");
    }
#if 0
    com_printf("Effective commandline: %s\n", command_line);
#endif
    return run_unix_command(command_line);
}
