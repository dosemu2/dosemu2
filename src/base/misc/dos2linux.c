/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
 *	Revision 1.16  2006/01/28 09:45:34  bartoldeman
 *	Move disclaimer and install prompts from dos_post_boot to banner code,
 *	using BIOS calls.
 *
 *	Revision 1.15  2005/12/31 06:23:11  bartoldeman
 *	Mostly from Clarence: unix.com patches in dosemu-exec-friendly.diff,
 *	so that "dosemu keen1.exe" works again.
 *	But do not use heuristics that split options from commands, by making
 *	quotes of filenames that contain spaces compulsory.
 *	
 *	Revision 1.14  2005/12/18 07:58:44  bartoldeman
 *	Put some basic checks into dos_read/dos_write, and memmove_dos2dos to
 *	avoid touching the protected video memory.
 *	Fixes #1379806 (gw stopped to work under X).
 *	
 *	Revision 1.13  2005/11/29 18:24:31  stsp
 *	
 *	call_msdos() must switch to real mode before calling DOS, if needed.
 *	
 *	Revision 1.12  2005/11/29 10:25:04  bartoldeman
 *	Adjust video.c init so that -dumb does not pop up an X window.
 *	Make -dumb quiet until the command is executed if a command is given. So
 *	dosemu -dumb dir
 *	gives a directory listing and nothing else.
 *	
 *	Revision 1.11  2005/11/21 18:12:51  stsp
 *	
 *	- Map the DOS<-->unix STDOUT/STDERR properly for unix.com.
 *	- Improved the DOS console reading for unix.com
 *	
 *	Revision 1.10  2005/11/19 00:54:40  stsp
 *	
 *	Make unix.com "keyboard-aware" (FR #1360156). It works like a tty
 *	in a -icanon mode.
 *	system() replaced with execlp() to make it possible to emulate ^C
 *	with SIGINT.
 *	Also reworked the output-grabbing again - it was still skipping an
 *	output sometimes.
 *	
 *	Revision 1.9  2005/11/18 21:47:56  stsp
 *	
 *	Make unix.com to use DOS/stdout instead of the direct video mem access,
 *	so that the file redirection to work.
 *	
 *	Revision 1.8  2005/11/02 04:52:55  stsp
 *	
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
#include <stdarg.h>

#include "config.h"
#include "emu.h"
#include "cpu-emu.h"
#include "int.h"
#include "dpmi.h"
#include "timers.h"
#include "video.h"
#include "lowmem.h"
#include "utilities.h"
#include "dos2linux.h" 
#include "vgaemu.h"
#include "keyb_server.h"

#include "redirect.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"

#define com_stderr      2

#ifndef max
#define max(a,b)       ((a)>(b)? (a):(b))
#endif

#define GET_USER_ENVVAR      0x52
#define GET_USER_COMMAND     0x51
#define EXEC_USER_COMMAND    0x50

#define MAX_DOS_COMMAND_LEN  256

static char *misc_dos_command = NULL;
static char *misc_dos_options = NULL;
static int need_terminate = 0;
int com_errno;

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

char *misc_e6_options(void)
{
  return misc_dos_options;
}

int misc_e6_need_terminate(void)
{
  return need_terminate;
}

void misc_e6_store_command (char *str, int terminate)
{
  size_t slen = strlen(str), olen = 0;
  if (slen > MAX_DOS_COMMAND_LEN) {
    error("DOS command line too long, exiting");
    leavedos(1);
  }
  if (misc_dos_command == NULL) {
    misc_dos_command = strdup(str);
    need_terminate = terminate;
    if (terminate) config.quiet = 1;

    g_printf ("Storing Command : %s\n", misc_dos_command);
    return;
  }
  /* any later arguments are collected as DOS options */
  if (misc_dos_options)
    olen = strlen(misc_dos_options);
  misc_dos_options = realloc(misc_dos_options, olen + slen + 2);
  misc_dos_options[olen] = ' ';
  memcpy(misc_dos_options + olen + 1, str, slen + 1);
  if (strlen(misc_dos_command) + olen + slen + 2 > MAX_DOS_COMMAND_LEN) {
    error("DOS command line too long, exiting");
    leavedos(1);
  }
  g_printf ("Storing Options : %s\n", misc_dos_options);
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


static char *make_end_in_backslash (char *s)
{
  int len = strlen (s);
  if (len && s [len - 1] != '/')
    strcpy (s + len, "/");
  return s;
}


/*
 * Return the drive from which <linux_path_resolved> is accessible.
 * If there is no such redirection, it returns the next available drive * -1.
 * If there are no available drives (from >= 2 aka "C:"), it returns -26.
 * If an error occurs, it returns -27.
 *
 * In addition, if a drive is available, <linux_path_resolved> is modified
 * to have the drive's uncannonicalized linux root as its prefix.  This is
 * necessary as make_unmake_dos_mangled_path() will not work with a resolved
 * path if the drive was LREDIR'ed by the user to a unresolved path.
 */
int find_drive (char *linux_path_resolved)
{
  int free_drive = -26;
  int drive;

  j_printf ("find_drive (linux_path='%s')\n", linux_path_resolved);

  for (drive = 0; drive < 26; drive++) {
    char *drive_linux_root = NULL;
    int drive_ro;
    char drive_linux_root_resolved [PATH_MAX];

    if (GetRedirectionRoot (drive, &drive_linux_root, &drive_ro) == 0/*success*/) {
      if (!realpath (drive_linux_root, drive_linux_root_resolved)) {
        com_fprintf (com_stderr,
                     "ERROR: %s.  Cannot canonicalize drive root path.\n",
                     strerror (errno));
        return -27;
      }

      /* make sure drive root ends in / */
      make_end_in_backslash (drive_linux_root_resolved);
      
      j_printf ("CMP: drive=%i drive_linux_root='%s' (resolved='%s')\n",
                 drive, drive_linux_root, drive_linux_root_resolved);

      /* TODO: handle case insensitive filesystems (e.g. VFAT)
       *     - can we just strlwr() both paths before comparing them? */
      if (strstr (linux_path_resolved, drive_linux_root_resolved) == linux_path_resolved) {
        char old_linux_path_resolved [PATH_MAX];
        
        j_printf ("\tFound drive!\n");
        strcpy (old_linux_path_resolved, linux_path_resolved);
        snprintf (linux_path_resolved, PATH_MAX, "%s%s",
                  drive_linux_root/*unresolved*/,
                  old_linux_path_resolved + strlen (drive_linux_root_resolved));
        j_printf ("\t\tModified root; linux path='%s'\n", linux_path_resolved);
        
        free (drive_linux_root);
        return drive;
      }
      
      free (drive_linux_root);
    } else {
      if (drive >= 2 && free_drive == -26) {
        free_drive = -drive;
      }
    }
  }

  j_printf ("find_drive() returning free drive: %i\n", -free_drive);
  return free_drive;
}


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
 */
void run_unix_command(char *buffer)
{
    /* unix command is in a null terminate buffer pointed to by ES:DX. */

    /* IMPORTANT NOTE: euid=user uid=root (not the other way around!) */

    int out[2], err[2], in[2];
    int pid, status, retval, mxs, rd;
    char buf[128];
    fd_set rfds;
    struct timeval tv;

    if (buffer==NULL) return;
    g_printf("UNIX: run '%s'\n",buffer);

    /* create a pipe... */
    if(pipe(out) || pipe(in) || pipe(err))
    {
        g_printf("run_unix_command(): pipe(p) failed\n");
        return;
    }
    
	/* fork child */
	switch ((pid = fork())) {
	case -1: /* failed */
    	    g_printf("run_unix_command(): fork() failed\n");
    	    return;
	case 0: /* child */
    	    close(out[0]);		/* close read side of the stdout pipe */
    	    close(err[0]);		/* and the stderr pipe */
	    close(in[1]);
	    dup2(in[0], 0);
    	    dup2(out[1], 1);	/* copy write side of the pipe to stdout */
    	    dup2(err[1], 2);	/* copy write side of the pipe to stderr */
        
	    priv_drop();
        
    	    retval = execlp("/bin/sh", "/bin/sh", "-c", buffer, NULL);	/* execute command */
	    error("exec /bin/sh failed\n");
    	    exit(retval);
	    break;
        }

        /* parent */
        close(out[1]);		/* close write side of the pipe */
        close(err[1]);		/* and stderr pipe */
        close(in[0]);
        
        /* read bytes until an error occurs or child exits
         * no big buffer here, because speed is not important
         * if speed *is* important, switch to another virtual console!
         * If both stdout and stderr produce output, we should
         * decide what to do (print only the stderr?)
         */
        mxs = max(out[0], err[0]) + 1;

        for (;;) {		/* nice eternal loop */
		int done = 0;

		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		FD_ZERO(&rfds);
		FD_SET(out[0], &rfds);
		FD_SET(err[0], &rfds);
		retval = select (mxs, &rfds, NULL, NULL, &tv);

		if (retval > 0) {
			int nr;
			/* one of the pipes has data, or EOF */
			if (FD_ISSET(out[0], &rfds)) {
				nr = read(out[0], buf, sizeof(buf));
				if (nr > 0) {
					buf[nr] = 0;
					com_fprintf(STDOUT_FILENO, "%s", buf);
				}
				if (nr == 0)
					done++;
			}
			if (FD_ISSET(err[0], &rfds)) {
				nr = read(err[0], buf, sizeof(buf));
				if (nr > 0) {
					buf[nr] = 0;
					com_fprintf(STDERR_FILENO, "%s", buf);
				}
				if (nr == 0)
					done++;
			}
		}
		if (done >= 2)		// if both pipes closed - return
			break;

		if ((rd = com_dosreadcon(buf, sizeof(buf)))) {
			char *ctrc = NULL;
			if ((ctrc = memchr(buf, 3, rd)))
				*ctrc = 0;	// avoid fancy char on screen
			buf[rd] = 0;
			if ((rd = strlen(buf))) {
				char *OxD;
				/* unix process doesn't like \r */
				while ((OxD = memchr(buf, '\r', rd)))
					*OxD = '\n';
				/* emulate echo :( */
				com_puts(buf); // this returns just-skipped \r's
				/* write to unix process */
				write(in[1], buf, rd);
			}
			/* emulate ^C */
			if (ctrc)
				kill(pid, SIGINT);
		}

		handle_signals();
	}

        /* kill the child (to be sure (s)he (?) is really dead) */
        if(kill(pid, SIGTERM)!=0)
            kill(pid, SIGKILL);
        waitpid(pid, &status, 0);

        close(out[0]);		/* close read side of the stdout pipe */
        close(err[0]);		/* and the stderr pipe */
        close(in[1]);
        
        /* print child exitcode. not perfect */
        g_printf("run_unix_command() (parent): child exit code: %i\n",
            WEXITSTATUS(status));
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

  g_printf("change_config: item = %d, buffer = %p\n", item, buf);

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

void memmove_dos2dos(void *dest, const void *src, size_t n)
{
  /* XXX GW (Game Wizard Pro) does this.
     TODO: worry about overlaps; could be a little cleaner
     using the memcheck.c mechanism */
  if (vga.inst_emu && (size_t)src >= 0xa0000 && (size_t)src < 0xc0000)
    memcpy_from_vga(dest, src, n);
  else if (vga.inst_emu && (size_t)dest >= 0xa0000 && (size_t)dest < 0xc0000)
    memcpy_to_vga(dest, src, n);
  else
    MEMMOVE_DOS2DOS(dest, src, n);
}

int dos_read(int fd, char *data, int cnt)
{
  int ret;
  /* GW also reads or writes directly from a file to protected video memory. */
  if (vga.inst_emu && (size_t)data >= 0xa0000 && (size_t)data < 0xc0000) {
    char buf[cnt];
    ret = RPT_SYSCALL(read(fd, buf, cnt));
    if (ret >= 0)
      memcpy_to_vga(data, buf, ret);
  }
  else
    ret = RPT_SYSCALL(read(fd, LINEAR2UNIX(data), cnt));
  if (ret > 0)
	e_invalidate(data, ret);
  return (ret);
}

int dos_write(int fd, char *data, int cnt)
{
  int ret;
  char buf[cnt];
  if (vga.inst_emu && (size_t)data >= 0xa0000 && (size_t)data < 0xc0000) {
    memcpy_from_vga(buf, data, cnt);
    data = buf;
  }
  ret = RPT_SYSCALL(write(fd, data, cnt));
  g_printf("Wrote %10.10s\n", data);
  return (ret);
}

#define BUF_SIZE 1024
int com_vsprintf(char *str, char *format, va_list ap)
{
	char *s = str;
	int i, size;
	char scratch[BUF_SIZE];

	size = vsnprintf(scratch, BUF_SIZE, format, ap);
	for (i=0; i < size; i++) {
		if (scratch[i] == '\n') *s++ = '\r';
		*s++ = scratch[i];
	}
	*s = 0;
	return s - str;
}

int com_sprintf(char *str, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vsprintf(str, format, ap);
	va_end(ap);
	return ret;
}

int com_vfprintf(int dosfilefd, char *format, va_list ap)
{
	int size;
	char scratch2[BUF_SIZE];

	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_doswrite(dosfilefd, scratch2, size);
}

int com_vprintf(char *format, va_list ap)
{
	return com_vfprintf(1, format, ap);
}

int com_fprintf(int dosfilefd, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(dosfilefd, format, ap);
	va_end(ap);
	return ret;
}

int com_printf(char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(1, format, ap);
	va_end(ap);
	return ret;
}

int com_puts(char *s)
{
	return com_printf("%s", s);
}

char *skip_white_and_delim(char *s, int delim)
{
	while (*s && isspace(*s)) s++;
	if (*s == delim) s++;
	while (*s && isspace(*s)) s++;
	return s;
}

void call_msdos(void)
{
	if(in_dpmi && !in_dpmi_dos_int)
		fake_pm_int();
	do_intr_call_back(0x21);
}

int com_doswrite(int dosfilefd, char *buf32, u_short size)
{
	char *s;
	u_short int23_seg, int23_off;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	memcpy(s, buf32, size);
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = FP_SEG32(s);
	LWORD(edx) = FP_OFF32(s);
	LWORD(eax) = 0x4000;	/* write handle */
	/* write() can be interrupted with ^C. Therefore we set int0x23 here
	 * so that even in this case it will return to the proper place. */
	int23_seg = ISEG(0x23);
	int23_off = IOFF(0x23);
	SETIVEC(0x23, CBACK_SEG, CBACK_OFF);
	call_msdos();	/* call MSDOS */
	SETIVEC(0x23, int23_seg, int23_off);	/* restore 0x23 ASAP */
	lowmem_heap_free(s);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return  LWORD(eax);
}

int com_dosread(int dosfilefd, char *buf32, u_short size)
{
	char *s;
	u_short int23_seg, int23_off;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = FP_SEG32(s);
	LWORD(edx) = FP_OFF32(s);
	LWORD(eax) = 0x3f00;
	/* read() can be interrupted with ^C, esp. when it reads from a
	 * console. Therefore we set int0x23 here so that even in this
	 * case it will return to the proper place. */
	int23_seg = ISEG(0x23);
	int23_off = IOFF(0x23);
	SETIVEC(0x23, CBACK_SEG, CBACK_OFF);
	call_msdos();	/* call MSDOS */
	SETIVEC(0x23, int23_seg, int23_off);	/* restore 0x23 ASAP */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		lowmem_heap_free(s);
		return -1;
	}
	memcpy(buf32, s, min(size, LWORD(eax)));
	lowmem_heap_free(s);
	return LWORD(eax);
}

int com_dosreadcon(char *buf32, u_short size)
{
	u_short rd = 0;

	if (!size) return 0;
	while (rd < size) {
		LWORD(eax) = 0x600;
		LO(dx) = 0xff;
		call_msdos();
		if (LWORD(eflags) & ZF)
			break;
		buf32[rd++] = LO(ax);
	}
	return rd;
}

int com_biosgetch(void)
{
	do {
		HI(ax) = 1;
		do_intr_call_back(0x16);
		handle_signals();
		keyb_server_run();
	} while (LWORD(eflags) & ZF);
	HI(ax) = 0;
	do_intr_call_back(0x16);
	return LO(ax);
}

int com_biosread(char *buf32, u_short size)
{
	u_short rd = 0;
	int ch;

	if (!size) return 0;
	while (rd < size) {
		ch = com_biosgetch();
		if (ch == '\b') {
			if (rd > 0) {
				p_dos_str("\b \b");
				rd--;
			}
			continue;
		}
		if (ch != '\r')
			buf32[rd++] = ch;
		if (iscntrl(ch))
			break;
		p_dos_str("%c", LO(ax));
	}
	p_dos_str("\n");
	return rd;
}
