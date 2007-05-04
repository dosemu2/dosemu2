/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/************************************************
 * unix.c
 * Run unix commands from inside Dosemu
 *
 * Written by ???
 *
 ************************************************/


#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "detect.h"


void usage (void);
int send_command (char **argv);
void do_execute_dos (int argc, char **argv);
void do_set_dosenv (int agrc, char **argv);


int main(int argc, char **argv)
{

  if (argc == 1) {
    usage();
  }

  if (!is_dosemu()) {
    printf ("This program requires DOSEMU to run, aborting\n");
    exit (-1);
  }

  if (*argv[1] == '-') {
    /* Got a switch */
    switch ((argv[1])[1]) {
    case 'e':
    case 'E':
      /* EXECUTE dos command*/
      do_execute_dos (argc-2, argv+2);
      break;
    case 's':
    case 'S':
      /* SETENV */
      do_set_dosenv (argc-2, argv+2);
      break;
    default:
      usage();
    }
  }

  send_command (argv);

  return (0);
}


void usage (void)
{
  printf ("Usage: UNIX [FLAG COMMAND]\n");
  printf ("Run Linux commands from DOSEMU\n\n");
  printf ("UNIX -e [ENVVAR]\n");
  printf ("  Execute the DOS command given in the Linux environment variable \"ENVVAR\"\n");
  printf ("  and then exit DOSEMU.\n");
  printf ("  If not given, use the argument to the -E flag of DOSEMU\n\n");
  printf ("UNIX -s ENVVAR\n");
  printf ("  Set the DOS environment to the Linux environment variable \"ENVVAR\".\n\n");
  printf ("UNIX command [arg1 ...]\n");
  printf ("  Execute the Linux command with the arguments given.\n\n");
  printf ("UNIX\n");
  printf ("  show this help screen\n\n\n");
  printf ("Note: Use UNIX only to run Linux commands that terminates without user\n");
  printf ("      interaction. Otherwise it will start and wait forever!\n");

  exit(-1);
}


int send_command(char **argv)
{
    static char command_line[256] = "";
    struct REGPACK preg;

    argv++;

    while(*argv)
    {
	strcat(command_line, *argv);
	strcat(command_line, " ");
	argv++;
    }

    printf("Effective commandline: %s\n", command_line);

    preg.r_ax = 0x50;
    preg.r_dx = FP_OFF(&command_line);
    preg.r_es = FP_SEG(&command_line);
    intr(0xe6, &preg);

    return(0);
}


void do_execute_dos (int argc, char **argv)
{
  char data[256];
  struct REGPACK preg;

  if (argc == 0) {
    data[0] = '\0';
    preg.r_ax = 0x51;
  } else {
    strcpy (data, argv[0]);
    preg.r_ax = 0x52;
  }

  /* Store the string address in the registers */
  preg.r_dx = FP_OFF(&data);
  preg.r_es = FP_SEG(&data);

  intr(0xe6, &preg);

  if (! preg.r_ax) {
    /* SUCCESSFUL */

    printf ("About to Execute : %s\n", data);

#ifdef GIBBER
	execvp (data, argv);   /* Search path - and replace ourselves ... */

    /* - unsuccesful exec ... */
    puts ("EXEC failed.\n");
#endif /* GIBBER */

	if (system (data)) {
		/* SYSTEM failed ... */
		fprintf (stderr, "SYSTEM failed ....(%d)", errno);
		exit (-1);
	}
	/* bye bye! */
        if (data[strlen(data)+1] == '\0') {
	  preg.r_ax = 0xffff;
	  intr(0xe6, &preg);
        }

	exit (0);
  }
  else {
    /* UNSUCCESSFUL */
    /* empty string, assume we had to exec -E and this wasn't given
     * ( may have 'unix -e' in your autoexec.bat )
     */
    exit (1);
  }

}


void do_set_dosenv (int argc, char **argv)
{
  struct REGPACK preg;
  char data[256];

  if (argc == 0) usage();

  strcpy (data, argv[0]);

  preg.r_ax = 0x52;

  /* Store the string address in the registers */
  preg.r_dx = FP_OFF(&data);
  preg.r_es = FP_SEG(&data);

  intr(0xe6, &preg);

  if (! preg.r_ax) {
    if (msetenv(argv[0],data))
    	exit(0);
  }
  exit (1);
}
