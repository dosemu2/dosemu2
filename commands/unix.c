#include <string.h>
#include <stdio.h>
#include <dos.h>


/* AM - Needed by TC to compile ... */
#include <stdlib.h>
#include <process.h>

int check_for_DOSEMU(void);
void usage (void);
send_command(char **argv);
void do_execute_dos (int argc, char **argv);
void do_set_dosenv (int agrc, char **argv);


int main(int argc, char **argv)
{
  if (argc == 1) {
    usage();
  }

  if (!check_for_DOSEMU ()) {
    printf ("This Command can only be used under DOSEMU.\n");
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
  printf ("  Execute the command given in the Linux environment variable \"ENVVAR\".\n");
  printf ("  If not given, use the argument to the -E flag of DOSEMU\n\n");
  printf ("UNIX command [arg1 ...]\n");
  printf ("  Execute the Linux command with the arguments given.\n\n");
  printf ("UNIX\n");
  printf ("  show this help screen\n\n\n");
  printf ("Note: Use UNIX only to run Linux commands that terminates without user\n");
  printf ("      interaction. Otherwise it will start and wait forever!\n");

  exit(-1);
}

send_command(char **argv)
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
    preg.r_dx = (unsigned int) &command_line;
    preg.r_es = _DS;
    intr(0xe6, &preg);

    return(0);
}


int check_for_DOSEMU()
{
    struct REGPACK preg;

    preg.r_ax = 0x00;
    intr(0xe6, &preg);

    if (preg.r_ax == 0xAA55) {
      /* In DOSEMU - MAJOR number in BX */
      return (preg.r_bx);
    } else {
      /* NOT in DOSEMU */
      return (0);
    }
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
  preg.r_dx = (unsigned int) &data;
  preg.r_es = _DS;

  intr(0xe6, &preg);

  if (! preg.r_ax) {
    /* SUCCESFUL */
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

	exit (0);
  } else {
    /* UNSUCESSFUL */
    puts ("FAILED to get DATA from DOSEMU.\n");

    exit (-1);
  }

}

void do_set_dosenv (int argc, char **argv)
{
  struct REGPACK preg;

  char data[256];

  puts ("This option (-s, set DOS environment) is not available at the moment.\n");
  puts ("Please try again later. Whilst you are waiting, please help yourself\n");
  puts ("to the Lemon scented napkins.\n");

  if (argc == 0) {
	data[0] = '\0';
  } else {
    strcpy (data, argv[0]);
  }

  preg.r_ax = 0x52;

  /* Store the string address in the registers */
  preg.r_dx = (unsigned int) &data;
  preg.r_es = _DS;

  intr(0xe6, &preg);

  if (! preg.r_ax) {
    /* SUCCESFUL */
    puts ("In the meantime, the environment variable would have been set to :\n");
    puts (data);

  } else {
    /* UNSUCCESFUL */
    printf ("Anyway, the lookup failed ...(%d)\n", _AX);

  }
  exit (-1);
}
