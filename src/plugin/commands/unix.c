/*
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "builtins.h"

#include "msetenv.h"
#include "unix.h"

#define printf  com_printf
#define fprintf com_fprintf
#undef stderr
#define stderr  com_stderr
#define puts	com_puts
#define system	com_system
#define errno	com_errno
#define FP_OFF(x) FP_OFF32(x)
#define FP_SEG(x) FP_SEG32(x)

#define CAN_EXECUTE_DOS 1

static int usage (void);
static int send_command (char **argv);
#if CAN_EXECUTE_DOS
static int do_execute_dos (int argc, char **argv);
#endif
static int do_set_dosenv (int agrc, char **argv);


int unix_main(int argc, char **argv)
{

  if (argc == 1) {
    return usage();
  }


  if (*argv[1] == '-') {
    /* Got a switch */
    switch ((argv[1])[1]) {
#if CAN_EXECUTE_DOS
    case 'e':
    case 'E':
      /* EXECUTE dos command*/
      return do_execute_dos (argc-2, argv+2);
#endif
    case 's':
    case 'S':
      /* SETENV */
      return do_set_dosenv (argc-2, argv+2);
    default:
      return usage();
    }
  }

  return send_command (argv);

}


static int usage (void)
{
  printf ("Usage: UNIX [FLAG COMMAND]\n");
  printf ("Run Linux commands from DOSEMU\n\n");
#if CAN_EXECUTE_DOS
  printf ("UNIX -e [ENVVAR]\n");
  printf ("  Execute the DOS command given in the Linux environment variable \"ENVVAR\"\n");
  printf ("  and then exit DOSEMU.\n");
  printf ("  If not given, use the argument to the -E flag of DOSEMU\n\n");
#endif
  printf ("UNIX -s ENVVAR\n");
  printf ("  Set the DOS environment to the Linux environment variable \"ENVVAR\".\n\n");
  printf ("UNIX command [arg1 ...]\n");
  printf ("  Execute the Linux command with the arguments given.\n\n");
  printf ("UNIX\n");
  printf ("  show this help screen\n\n\n");
  printf ("Note: Use UNIX only to run Linux commands that terminates without user\n");
  printf ("      interaction. Otherwise it will start and wait forever!\n");

  return (1);
}


static int send_command(char **argv)
{
    char *command_line = lowmem_alloc(256);
    struct REGPACK preg = REGPACK_INIT;

    command_line[0] = 0;
    argv++;

    while(*argv)
    {
	strcat(command_line, *argv);
	strcat(command_line, " ");
	argv++;
    }

    printf("Effective commandline: %s\n", command_line);

    preg.r_ax = DOS_HELPER_RUN_UNIX;
    preg.r_dx = FP_OFF(command_line);
    preg.r_es = FP_SEG(command_line);
    dos_helper_r(&preg);

    return(0);
}

#if CAN_EXECUTE_DOS
static int do_execute_dos (int argc, char **argv)
{
  char *data = lowmem_alloc(256);
  struct REGPACK preg = REGPACK_INIT;

  if (argc == 0) {
    data[0] = '\0';
    preg.r_ax = DOS_HELPER_GET_USER_COMMAND;
  } else {
    strcpy (data, argv[0]);
    preg.r_ax = DOS_HELPER_GET_UNIX_ENV;
  }

  /* Store the string address in the registers */
  preg.r_dx = FP_OFF(data);
  preg.r_es = FP_SEG(data);

  dos_helper_r(&preg);

  if (! preg.r_ax) {
    /* SUCCESSFUL */

    printf ("About to Execute : %s\n", data);


	if (system (data)) {
		/* SYSTEM failed ... */
		fprintf (stderr, "SYSTEM failed ....(%d)\n", errno);
		return (1);
	}
 
	return (0);
  }
  else {
    /* UNSUCCESSFUL */
    /* empty string, assume we had to exec -E and this wasn't given
     * ( may have 'unix -e' in your autoexec.bat )
     */
    return (1);
  }
}
#endif

static int do_set_dosenv (int argc, char **argv)
{
  struct REGPACK preg = REGPACK_INIT;
  char *data = lowmem_alloc(256);

  if (argc == 0) return usage();

  strcpy (data, argv[0]);

  preg.r_ax = DOS_HELPER_GET_UNIX_ENV;

  /* Store the string address in the registers */
  preg.r_dx = FP_OFF(data);
  preg.r_es = FP_SEG(data);

  dos_helper_r(&preg);

  if (! preg.r_ax) {
    if (msetenv(argv[0],data))
    	return (0);
  }
  return (1);
}
