/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * 
 * DANG_BEGIN_MODULE
 * REMARK
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
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 *	$Log$
 *	Revision 1.1  2003/06/23 00:02:07  bartoldeman
 *	Initial revision
 *
 *
 * DANG_END_CHANGELOG
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "dos2linux.h" 
#include "emu.h"
#include "priv.h"
#include "pic.h"
#include "int.h"
#include "vc.h"

#ifndef max
#define max(a,b)       ((a)>(b)? (a):(b))
#endif

#define GET_USER_ENVVAR      0x52
#define GET_USER_COMMAND     0x51
#define EXEC_USER_COMMAND    0x50

#define MAX_DOS_COMMAND_LEN  256

static char *misc_dos_command = NULL;

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
    size_t len = strlen(misc_dos_command);      

    /* also store info about whether or not to terminate */
    memcpy (str, misc_dos_command, len + 2);
    g_printf ("%s\n", str);

    return 0;
  }

  /* doesn't get this far */
}


void misc_e6_store_command (char *str, int terminate)
{
  size_t len = strlen(str) + 2;
  if (len > MAX_DOS_COMMAND_LEN) {
    error("DOS command line too long, exiting");
    leavedos(1);
  }
  misc_dos_command = malloc(len);
  strcpy(misc_dos_command, str);
  /* store info about whether or not to terminate */
  misc_dos_command[len-1] = terminate ? '\0' : '\1';

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
 *  - initial version
 * 3/10/1995, Erik Mouw (j.a.k.mouw@et.tudelft.nl):
 *  - fixed security hole
 *  - stderr output also to dosemu screen
 * 2/27/1997, Alberto Vignani (vignani@mail.tin.it):
 *  - make it work (tested with 'ls -lR /' and 'updatedb &')
 *
 * DANG_BEGIN_FUNCTION run_unix_command
 *
 * description:
 *  Runs a command and prints the (stdout and stderr) output on the dosemu 
 *  screen.
 *
 * return: nothing
 *
 * arguments: 
 *   buffer - string with command to execute
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
 * C:\>unix ls -CF /usr/src/dosemu/video / *.[ch]
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
    /* unix command is in a null terminate buffer pointed to by ES:DX. */

    /* IMPORTANT NOTE: euid=user uid=root (not the other way around!) */

    int p[2];
    int q[2];
    int pid, status;
    char si_buf[128], se_buf[128];

    if (buffer==NULL) return;
    g_printf("UNIX: run '%s'\n",buffer);

    /* create a pipe... */
    if(pipe(p)!=0)
    {
        g_printf("run_unix_command(): pipe(p) failed\n");
        return;
    }
    
    /* ...or two */
    if(pipe(q)!=0)
    {
        g_printf("run_unix_command(): pipe(q) failed\n");
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
	int retval;

        close(p[0]);		/* close read side of the stdout pipe */
        close(q[0]);		/* and the stderr pipe */
        close(1);		/* close stdout */
        close(2);		/* close stderr */
        if(dup(p[1])!=1)	/* copy write side of the pipe to stdout */
        {
            /* hmm, I wonder if the next line works ok... */
            g_printf("run_unix_command() (child): dup(p) failed\n");
            _exit(-1);
        }
        
        if(dup(q[1])!=2)	/* copy write side of the pipe to stderr */
        {
            g_printf("run_unix_command() (child): dup(q) failed\n");
            _exit(-1);
        }
        
        /* DOSEMU runs setuid(root). go back to the real uid/gid for
         * safety reasons.
         *
         * NOTE: euid=user uid=root!  -Steven P. Crain
         */
	priv_drop();
        
        retval=system(buffer);	/* execute command */
        close(p[1]);		/* close write side of the stdout pipe */
        close(q[1]);		/* and stderr pipe */
        _exit(retval);
    }
    else /* parent */
    {
        fd_set rfds;
        struct timeval tv;
        int mxs, retval = 0;

        close(p[1]);		/* close write side of the pipe */
        close(q[1]);		/* and stderr pipe */
        
        /* read bytes until an error occurs or child exits
         * no big buffer here, because speed is not important
         * if speed *is* important, switch to another virtual console!
         * If both stdout and stderr produce output, we should
         * decide what to do (print only the stderr?)
         */
	FD_ZERO(&rfds);
	FD_SET(p[0], &rfds);
	FD_SET(q[0], &rfds);
	mxs = max(p[0], q[0]) + 1;

	for (;;) {		/* nice eternal loop */
		int nr;

		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		retval = select (mxs, &rfds, NULL, NULL, &tv);

		if (retval > 0) {
			/* one of the pipes has data, or EOF */
			if (FD_ISSET(p[0], &rfds)) {
				nr = read(p[0], si_buf, 80);
				if (nr <= 0) break;
				si_buf[nr] = 0;
				p_dos_str("%s", si_buf);
			}
			if (FD_ISSET(q[0], &rfds)) {
				nr = read(q[0], se_buf, 80);
				if (nr <= 0) break;
				se_buf[nr] = 0;
				p_dos_str("%s", se_buf);
			}
		}
		else {
			/* return for timeout or signal
			 * we'll lose a LOT of SIGALRMs here but
			 * this is not critical; instead, we could
			 * find a way to process other signals
			 * (e.g. currently we can't stop the child!)
			 */
			handle_signals();
			if (iq.queued)
				do_queued_ioctl();
			/*
			 * if the child doesn't send anything to the
			 * pipes, we check here for termination
			 */
			if (waitpid(pid, &status, WNOHANG)==pid) break;
		}
	}
 
        
        /* kill the child (to be sure (s)he (?) is really dead) */
        if(kill(pid, SIGTERM)!=0)
            kill(pid, SIGKILL);
            
        close(p[0]);		/* close read side of the stdout pipe */
        close(q[0]);		/* and the stderr pipe */
        
        /* anti-zombie code */
        waitpid(pid, &status, WUNTRACED);
        
        /* print child exitcode. not perfect */
        g_printf("run_unix_command() (parent): child exit code: %i\n",
            WEXITSTATUS(status));
    }
}


/*
 * This runs system() in a forked environment with all priviledges off
 * while scheduling vital DOSEMU functionality in the background.
 */
int run_system_command(char *buffer)
{
    int pid, status;

    if (!buffer || !buffer[0]) return 1;

    /* fork child */
    pid=fork();
    if (pid == -1) {
        /* failed */
        g_printf("run_system_command(): fork() failed\n");
        return 1;
    }
    else if (pid == 0) {
	/* child */
	int retval;
	priv_drop(); /* we drop _all_ priviledges */
        retval=system(buffer);	/* execute command */
        _exit(retval);
    }
    else {
	/* parent */
        while (waitpid(pid, &status, WNOHANG) != pid) {
            handle_signals();
            if (iq.queued)
                  do_queued_ioctl();
            usleep(10000);
        }
        return WEXITSTATUS(status);
    }
}
