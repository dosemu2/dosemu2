/*
 * 
 * DANG_BEGIN_MODULE
 *
 * This file contains a simple system for passing information through to
 * DOSEMU from the Linux side. It does not allow dynamic message passing,
 * but it intended to provide useful information for the DOS user.
 * 
 * As such, the current set of implemented commands are :
 * GET_USER_ENVVAR and GET_COMMAND
 *
 * These are made available to the DOSEMU by using the DOS_HELPER interrupt
 * (Currently 0xE6) and writing a string into a location passed to this 
 * interrupt handler using the registers. In the case of GET_USER_ENVVAR the
 * string also contains the name of the environment variable to interrogate.
 * (The string is overwritten with the reply).
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 * DANG_END_CHANGELOG
 */

/* JAMES - Can you edit the CHANGELOG into place for me please ???!! 8-) */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* #include "dos2linux.h" */
#include "../include/emu.h"

#define GET_USER_ENVVAR      0x52
#define GET_USER_COMMAND     0x51
#define EXEC_USER_COMMAND    0x50

#define MAX_DOS_COMMAND_LEN  256

char misc_dos_command[MAX_DOS_COMMAND_LEN + 1];

int misc_e6_envvar (char *str)
{
  char *tmp;

  g_printf ("Environment Variable Check : %s", str);

  tmp = getenv (str);

  if (tmp == NULL)
  {
    str[0] = '\0';

    g_printf (" is undefined\n");

    return 1;
  } else {
    strcpy (str, tmp);

    g_printf (" is %s\n", str);

    return 0;
  }

  /* doesn't get this far */
}


int misc_e6_commandline (char *str)
{

  g_printf ("Command Line Check : ");

  if (misc_dos_command == NULL)
  {
    str[0] = '\0';

    g_printf ("%s\n", str);

    return 1;
  } else {
    strcpy (str, misc_dos_command);

    g_printf ("%s\n", str);

    return 0;
  }

  /* doesn't get this far */
}


void misc_e6_store_command (char *str)
{
  strcpy (misc_dos_command, str);

  g_printf ("Storing Command : %s\n", misc_dos_command);
}


#ifdef FORK_DEBUG
#warning USING FORK DEBUG
/* system to debug through forks...
 * If used, setenv FORKDEBUG to stop child on fork call
 */
static int fork_debug(void)
{
	int retval;

	retval = fork();

	if(retval == 0) {
		/* child -- maybe stop */
		if(getenv("FORKDEBUG")) {
			printf("stopping %d\n", getpid());
			raise(SIGSTOP);
		}
	}
	return retval;
}
#define fork	fork_debug
#endif


/*
 * 2/9/1995, Erik Mouw (j.a.k.mouw@et.tudelft.nl):
 *
 * DANG_BEGIN_FUNCTION run_unix_command
 *
 * arguments: string with command to execute
 *
 * returns: nothing
 *
 * description:
 *  Runs a command and prints the (stdout) output on the dosemu screen.
 *
 * DANG_END_FUNCTION
 *
 *
 * This function forks a child process (don't worry, it doesn't take 
 * much memory, read the fork manpage) and creates a pipe between the 
 * parent and the child. The child redirects stdout to the write side 
 * of the pipe and executes the command, the parents reads the read 
 * side of the pipe and prints the command output on the dosemu screen. 
 * system() is used to execute the command because this function uses a 
 * shell (/bin/sh -c command), so nice things like this work: 
 *
 * C:\>unix ls -CF /usr/src/dosemu/video/*.[ch]
 *
 * Even output redirection works, but you have to quote the command in 
 * dosemu:
 *
 * C:\>unix "ps aux | grep dos > /msdos/c/unixcmd.txt"
 *
 * DOS output redirection doesn't work, p_dos_str() uses direct video 
 * memory access (I think).
 *
 * Be prepared to kill the child from a telnet session or a terminal 
 * on /dev/ttyS? when you start an interactive command!
 *
 */
void run_unix_command(char *buffer)
{
      /* DANG_FIXME Calls to "system" are unsafe when setuid .... - AM */

#ifdef SIMPLE_FORK
	/* unix command is in a null terminate buffer pointed to by ES:DX. */
      system(buffer);

#else
    int p[2];
    int pid, status, retval;
    char buf;
    
    /* create a pipe */
    if(pipe(p)!=0)
    {
        g_printf("run_unix_command(): pipe() failed\n");
        return;
    }
    
    /* fork child */
    pid=fork();
    if(pid==-1) /* failed */
    {
        g_printf("run_unix_command(): fork() failed\n");
        return;
    }
    else if(pid==0) /* child */
    {
        close(p[0]);		/* close read side of the pipe */
        close(1);		/* close stdout */
        if(dup(p[1])!=1)	/* copy write side of the pipe to stdout */
        {
            /* hmm, I wonder if the next line works ok... */
            g_printf("run_unix_command() (child): dup() failed\n");
            _exit(-1);
        }
            
        retval=system(buffer);	/* execute command */
        close(p[1]);		/* close write side of the pipe */
        _exit(retval);
    }
    else /* parent */
    {
        close(p[1]);		/* close write side of the pipe */
        
        /* read bytes until an error occurs */
        /* no big buffer here, because speed is not important */
        /* if speed *is* important, switch to another virtual console! */
        while(read(p[0], &buf, 1)==1)
        {
            p_dos_str("%c", buf);	/* print character */
        }
        
        /* kill the child (to be sure (s)he (?) is really dead) */
        if(kill(pid, SIGTERM)!=0)
            kill(pid, SIGKILL);
            
        close(p[0]);		/* close read side of the pipe */
        waitpid(pid, &status, WUNTRACED);
        /* print child exitcode. not perfect */
        g_printf("run_unix_command() (parent): child exit code: %i\n",
            WEXITSTATUS(&status));
    }
#endif
}
