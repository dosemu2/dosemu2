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


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "builtins.h"
#include "redirect.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"

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
static int do_execute_dos (int argc, char **argv, int isLiteralCommand);
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
      return do_execute_dos (argc-2, argv+2, 1/*literal DOS cmd*/);
    case 'c':
    case 'C':
      /* EXECUTE dos program*/
      return do_execute_dos (argc-2, argv+2, 0/*not literal DOS cmd - is Linux path*/);
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
  printf ("  Execute the DOS command given in the Linux environment variable \"ENVVAR\".\n");
  printf ("  If not given, use the argument to the -E flag of DOSEMU\n\n");
  printf ("UNIX -c [ENVVAR]\n");
  printf ("  Execute the DOS program whose Linux path is given in the Linux environment\n");
  printf ("  variable \"ENVVAR\".\n");
  printf ("  If not given, use the argument to the -E flag of DOSEMU\n\n");
#endif
  printf ("UNIX -s ENVVAR\n");
  printf ("  Set the DOS environment to the Linux environment variable \"ENVVAR\".\n\n");
  printf ("UNIX command [arg1 ...]\n");
  printf ("  Execute the Linux command with the arguments given.\n\n");
  printf ("UNIX\n");
  printf ("  show this help screen\n\n");
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

static char *makeEndInBackslash (char *s)
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
static int findDrive (char *linux_path_resolved)
{
  int freeDrive = -26;

  j_printf ("findDrive (linux_path='%s')\n", linux_path_resolved);

  int drive;
  for (drive = 0; drive < 26; drive++) {
    char *drive_linux_root = NULL;
    int drive_ro;
    char drive_linux_root_resolved [PATH_MAX];

    if (GetRedirectionRoot (drive, &drive_linux_root, &drive_ro) == 0/*success*/) {
      if (!realpath (drive_linux_root, drive_linux_root_resolved)) {
        fprintf (stderr, "ERROR: Cannot canonicalize drive root path (%s)\n", strerror (errno));
        return -27;
      }

      /* make sure drive root ends in / */
      makeEndInBackslash (drive_linux_root_resolved);
      
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
      if (drive >= 2 && freeDrive == -26) {
        freeDrive = -drive;
      }
    }
  }

  j_printf ("findDrive() returning free drive: %i\n", -freeDrive);
  return freeDrive;
}

/*
 * Given a <linux_path>, change to its corresponding drive and directory
 * in DOS (redirecting a new drive if neccessary).  The DOS command is
 * returned through <linux_path>.
 *
 * Returns 0 on success, nonzero on failure.
 */
static int setupDOSCommand (char *linux_path)
{
  char linux_path_resolved[PATH_MAX];
  char dos_path [MAX_PATH_LENGTH];
  int drive;
 
  char *b;

  
  if (!realpath (linux_path, linux_path_resolved)) {
    fprintf (stderr, "ERROR: Cannot canonicalize path (%s)\n", strerror (errno));
    return (1);
  }

  
  drive = findDrive (linux_path_resolved);
  if (drive < 0) {
    drive = -drive;
    
    if (drive >= 26) {
      if (drive == 26) {
        fprintf (stderr, "ERROR: Cannot find a free DOS drive to use for LREDIR\n");
      }
      
      return (1);
    }

    /* try mounting / on this DOS drive
       (this won't work nice with "long" Linux paths > approx. 66
        but at least programs that try to access ..\DATA\BLAH.DAT
        will work)
     */

    j_printf ("Redirecting %c: to /\n", drive + 'A');
    if (RedirectDisk (drive, LINUX_RESOURCE "/", 0/*rw*/) != 0/*success*/) {
      fprintf (stderr, "ERROR: Could not redirect %c: to /\n", drive + 'A');
      return (1);
    }
  }
  
  
  /* switch to the drive */
  j_printf ("Switching to drive %i (%c:)\n", drive, drive + 'A');
  com_dossetdrive (drive);
  if (com_dosgetdrive () != drive) {
    fprintf (stderr, "ERROR: Could not change to %c:\n", drive + 'A');
     
    if (com_dossetdrive (com_dosgetdrive ()) < 26)
      fprintf (stderr, "Try 'LASTDRIVE=Z' in CONFIG.SYS.\n");

    return (1);
  }

  
  make_unmake_dos_mangled_path (dos_path, linux_path_resolved,
                                drive, 1/*alias*/);
  j_printf ("DOS path: '%s' (from linux '%s')\n",
            dos_path, linux_path_resolved);

  
  /* switch to the directory */
  if (strlen (dos_path) >= 3 && (b = strrchr (dos_path, '\\')) != NULL) {
    char *dos_dir = dos_path + 2;
    *b++ = 0;
    
    j_printf ("Changing to directory '%s'\n", dos_dir);
    if (com_dossetcurrentdir (dos_dir)) {
      fprintf (stderr, "ERROR: Could not change to directory: %s\n", dos_dir);
      return (1);
    }
  } else {
    fprintf (stderr, "INTERNAL ERROR: no backslash in DOS path\n");
    return (1);
  }
  

  /* return the 8.3 EXE name */
  strcpy (linux_path/*arg as return value*/, b);
  
  
  return (0);
}

static int do_execute_dos (int argc, char **argv, int isLiteralCommand)
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

    if (!isLiteralCommand && setupDOSCommand (data))
        return (1);
    
    if (*data) {
      printf ("About to Execute : %s\n", data);

      if (system (data)) {
        /* SYSTEM failed ... */
        fprintf (stderr, "SYSTEM failed ....(%d)\n", errno);
        return (1);
      }
 
      return (0);
    } else {
      /* empty string */
      return (1);
    }
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
