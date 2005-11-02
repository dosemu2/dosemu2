/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
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
 *	Revision 1.8  2005/11/02 04:52:55  stsp
 *	Fix run_unix_command (#1345102)
 *
 *	Revision 1.7  2005/09/30 22:15:59  stsp
 *	
 *	Avoid using LOWMEM for the non-const addresses.
 *	This fixes iplay (again), as it read()s directly to the EMS frame.
 *	Also, moved dos_read/write to dos2linux.c to make them globally
 *	available. And removed the __builtin_constant_p() check in LINEAR2UNIX -
 *	gcc might still be able to optimize the const addresses, but falling
 *	back for dosaddr_to_unixaddr() will happen less frequently.
 *	
 *	Revision 1.6  2005/09/05 09:47:56  bartoldeman
 *	Moved much of X_change_config to dos2linux.c. Much of it will be shared
 *	with SDL.
 *	
 *	Revision 1.5  2005/06/20 17:12:11  stsp
 *	
 *	Remove the ancient (unused) ioctl queueing code.
 *	
 *	Revision 1.4  2005/05/20 00:26:09  bartoldeman
 *	It's 2005 this year.
 *	
 *	Revision 1.3  2005/03/21 17:24:09  stsp
 *	
 *	Fixed (and re-enabled) the terminate-after-execute feature (bug #1152829).
 *	Also some minor adjustments/leftovers.
 *	
 *	Revision 1.2  2004/01/16 20:50:27  bartoldeman
 *	Happy new year!
 *	
 *	Revision 1.1.1.1  2003/06/23 00:02:07  bartoldeman
 *	Initial import (dosemu-1.1.5.2).
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
#include "cpu-emu.h"
#include "priv.h"
#include "pic.h"
#include "int.h"
#include "timers.h"
#include "vc.h"
#include "video.h"

#ifndef max
#define max(a,b)       ((a)>(b)? (a):(b))
#endif

#define GET_USER_ENVVAR      0x52
#define GET_USER_COMMAND     0x51
#define EXEC_USER_COMMAND    0x50

#define MAX_DOS_COMMAND_LEN  256

static char *misc_dos_command = NULL;
static int need_terminate = 0;

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

int misc_e6_need_terminate(void)
{
  return need_terminate;
}

void misc_e6_store_command (char *str, int terminate)
{
  if (strlen(str) > MAX_DOS_COMMAND_LEN) {
    error("DOS command line too long, exiting");
    leavedos(1);
  }
  misc_dos_command = strdup(str);
  need_terminate = terminate;

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
	mxs = max(p[0], q[0]) + 1;

	for (;;) {		/* nice eternal loop */
		int nr;

		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		FD_ZERO(&rfds);
		FD_SET(p[0], &rfds);
		FD_SET(q[0], &rfds);
		retval = select (mxs, &rfds, NULL, NULL, &tv);

		if (retval > 0) {
			/* one of the pipes has data, or EOF */
			if (FD_ISSET(p[0], &rfds)) {
				nr = read(p[0], si_buf, 80);
				if (nr > 0) {
					si_buf[nr] = 0;
					p_dos_str("%s", si_buf);
				}
			}
			if (FD_ISSET(q[0], &rfds)) {
				nr = read(q[0], se_buf, 80);
				if (nr > 0) {
					se_buf[nr] = 0;
					p_dos_str("%s", se_buf);
				}
			}
		}
		handle_signals();
		if (waitpid(pid, &status, WNOHANG)==pid)
			break;
	}
 
        
        /* kill the child (to be sure (s)he (?) is really dead) */
        if(kill(pid, SIGTERM)!=0)
            kill(pid, SIGKILL);
            
        close(p[0]);		/* close read side of the stdout pipe */
        close(q[0]);		/* and the stderr pipe */
        
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
            usleep(10000);
        }
        return WEXITSTATUS(status);
    }
}

/*
 * This function provides parts of the interface to reconfigure parts
 * of X/SDL and the VGA emulation during a DOSEMU session.
 * It is used by the xmode.exe program that comes with DOSEMU.
 */
int change_config(unsigned item, void *buf, int grab_active, int kbd_grab_active)
{
  static char title_emuname [TITLE_EMUNAME_MAXLEN] = {0};
  static char title_appname [TITLE_APPNAME_MAXLEN] = {0};
  int err = 0;

  g_printf("change_config: item = %d, buffer = 0x%x\n", item, (unsigned) buf);

  switch(item) {

    case CHG_TITLE:
      {
	/* high-level write (shows name of emulator + running app) */
	char title [TITLE_EMUNAME_MAXLEN + TITLE_APPNAME_MAXLEN + 35] = {0};
         
	/* app - DOS in a BOX */
	/* name of running application (if any) */
	if (config.X_title_show_appname && strlen (title_appname))
	  strcpy (title, title_appname);
         
	/* append name of emulator */
	if (strlen (title_emuname)) {
	    if (strlen (title)) strcat (title, " - ");
	    strcat (title, title_emuname);
	} else if (strlen (config.X_title)) {
	  if (strlen (title)) strcat (title, " - ");
	  /* foreign string, cannot trust its length to be <= TITLE_EMUNAME_MAXLEN */
	  snprintf (title + strlen (title), TITLE_EMUNAME_MAXLEN, "%s ", config.X_title);  
	}

	if (dosemu_frozen) {
	  if (strlen (title)) strcat (title, " ");

	  if (dosemu_user_froze)
	    strcat (title, "[paused - Ctrl+Alt+P] ");
	  else
	    strcat (title, "[background pause] ");
	}

	if (grab_active || kbd_grab_active) {
	  strcat(title, "[");
	  if (kbd_grab_active) {
	    strcat(title, "keyboard");
	    if (grab_active)
	      strcat(title, "+");
	  }
	  if (grab_active)
	    strcat(title, "mouse");
	  strcat(title, " grab] ");
	}

	/* now actually change the title of the Window */
	Video->change_config (CHG_TITLE, title);
      }
       break;
       
    case CHG_TITLE_EMUNAME:
      g_printf ("change_config: emu_name = %s\n", (char *) buf);
      snprintf (title_emuname, TITLE_EMUNAME_MAXLEN, "%s", ( char *) buf);
      Video->change_config (CHG_TITLE, NULL);
      break;
      
    case CHG_TITLE_APPNAME:
      g_printf ("change_config: app_name = %s\n", (char *) buf);
      snprintf (title_appname, TITLE_APPNAME_MAXLEN, "%s", (char *) buf);
      Video->change_config (CHG_TITLE, NULL);
      break;

    case CHG_TITLE_SHOW_APPNAME:
      g_printf("change_config: show_appname %i\n", *((int *) buf));
      config.X_title_show_appname = *((int *) buf);
      Video->change_config (CHG_TITLE, NULL);
      break;

    case CHG_WINSIZE:
      config.X_winsize_x = *((int *) buf);
      config.X_winsize_y = ((int *) buf)[1];
      g_printf("change_config: set initial graphics window size to %d x %d\n", config.X_winsize_x, config.X_winsize_y);
      break;

    case CHG_BACKGROUND_PAUSE:
      g_printf("change_config: background_pause %i\n", *((int *) buf));
      config.X_background_pause = *((int *) buf);
      break;

    case GET_TITLE_APPNAME:
      snprintf (buf, TITLE_APPNAME_MAXLEN, "%s", title_appname);
      break;
          
    default:
      err = 100;
  }

  return err;
}

int dos_read(int fd, char *data, int cnt)
{
  int ret;
  ret = RPT_SYSCALL(read(fd, LINEAR2UNIX(data), cnt));
  if (ret > 0)
	e_invalidate(data, ret);
  return (ret);
}

int dos_write(int fd, char *data, int cnt)
{
  int ret;
  ret = RPT_SYSCALL(write(fd, LINEAR2UNIX(data), cnt));
  g_printf("Wrote %10.10s\n", data);
  return (ret);
}
