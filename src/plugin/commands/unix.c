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


#include "emu.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "memory.h"
#include "doshelpers.h"
#include "dos2linux.h"
#include "builtins.h"
#include "redirect.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"

#include "msetenv.h"
#include "unix.h"

#define FP_OFF(x) FP_OFF32(x)
#define FP_SEG(x) FP_SEG32(x)

#define CAN_EXECUTE_DOS 1
enum { EXEC_LINUX_PATH, EXEC_LITERAL, EXEC_CHOICE };

static int usage (void);
static int send_command (char **argv);
#if CAN_EXECUTE_DOS
static int do_execute_dos (int argc, char **argv, int CommandStyle);
#endif
static int do_set_dosenv (int agrc, char **argv);


int unix_main(int argc, char **argv)
{

  if (argc == 1 ||
      (argc == 2 && !strcmp (argv[1], "/?"))) {
    return usage();
  }

  if (*argv[1] == '-') {
    /* Got a switch */
    switch ((argv[1])[1]) {
#if CAN_EXECUTE_DOS
    case 'e':
    case 'E':
      /* EXECUTE dos program or command if program does not exist */
      if (unix_e_welcome) {
	show_welcome_screen();
	unix_e_welcome = 0;
      }
      return do_execute_dos (argc-2, argv+2, EXEC_CHOICE);
    case 'r':
    case 'R':
      /* EXECUTE dos command*/
      return do_execute_dos (argc-2, argv+2, EXEC_LITERAL);
    case 'c':
    case 'C':
      /* EXECUTE dos program*/
      return do_execute_dos (argc-2, argv+2, EXEC_LINUX_PATH);
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
  com_printf ("Usage: UNIX [FLAG COMMAND]\n\n");
#if CAN_EXECUTE_DOS
  com_printf ("UNIX -r [ENVVAR]\n");
  com_printf ("  Execute the DOS command given in the Linux environment variable \"ENVVAR\".\n");
  com_printf ("  If not given, use the argument to the -E flag of DOSEMU\n\n");
  com_printf ("UNIX -c [ENVVAR]\n");
  com_printf ("  Execute the DOS program whose Linux path is given in the Linux environment\n");
  com_printf ("  variable \"ENVVAR\".");
  com_printf (" If not given, use the argument to the -E flag of DOSEMU\n\n");
  com_printf ("UNIX -e [ENVVAR]\n");
  com_printf ("  Invokes \"UNIX -c\" if the argument appears to refer to a");
  com_printf (" Linux path (ends in\n  .com, .exe or .bat, and exists or");
  com_printf (" contains slashes) or \"UNIX -r\" otherwise.\n\n");
#endif
  com_printf ("UNIX -s ENVVAR\n");
  com_printf ("  Set the DOS environment to the Linux environment variable \"ENVVAR\".\n\n");
  com_printf ("UNIX command [arg1 ...]\n");
  com_printf ("  Execute the Linux command with the arguments given.\n\n");
  com_printf ("UNIX\n");
  com_printf ("UNIX /?\n");
  com_printf ("  show this help screen\n");

  return (1);
}


static int send_command(char **argv)
{
    char command_line[256];

    command_line[0] = 0;
    argv++;

    while(*argv)
    {
        strcat(command_line, *argv);
        strcat(command_line, " ");
        argv++;
    }
#if 0
    com_printf("Effective commandline: %s\n", command_line);
#endif
    run_unix_command(command_line);
    return(0);
}

#if CAN_EXECUTE_DOS


static int is_progname_char (unsigned char c)
{
  /*
   * No "." and "/" especially as "blah..exe" or "blah/.exe" are unlikely
   * programs.
   * NOTE: ' is always allowed! +,;=[] are only allowed in LFNs.
   */
  return c >= 0x20 && !strchr("*?./\\\":|<>,",c);
}

static int likely_linux_path (const char *linux_path)
{
  char linux_path_resolved [PATH_MAX];
  struct stat s;
  size_t len = strlen(linux_path);

  j_printf ("likely_linux_path('%s')?\n", linux_path);

  if (len >= 5) {
    const char *n = linux_path + len - 5;

    if (is_progname_char (*n) &&
	/* \.(bat|com|exe) */
	(strncasecmp (n+1, ".bat", 4) == 0 ||
	 strncasecmp (n+1, ".com", 4) == 0 ||
	 strncasecmp (n+1, ".exe", 4) == 0)) {

      if (strchr (linux_path, '/')) {
	j_printf ("\tyes - contains /\n");
	return 1;
      }

      if (realpath (linux_path, linux_path_resolved) &&
	  !stat (linux_path_resolved, &s) && S_ISREG (s.st_mode)) {
	j_printf ("\tyes - existing file\n");
	return 1;
      }
    }
  }

  j_printf ("\tno\n");
  return 0;
}

/*
 * Given a <linux_path>, change to its corresponding drive and directory
 * in DOS (redirecting a new drive if necessary).  The DOS command and any
 * DOS options (like "/a/b /c") are returned through <linux_path>.
 * The DOS options may be passed in dos_opts as well.
 *
 * <*CommandStyle> must be EXEC_CHOICE or EXEC_LINUX_PATH on entry.
 * It will be EXEC_LITERAL or EXEC_LINUX_PATH on exit.  If it becomes
 * EXEC_LITERAL, <linux_path> will not be modified.
 * returned through <linux_path>.
 *
 * Returns 0 on success, nonzero on failure.
 */
static int setupDOSCommand (int *CommandStyle, char *linux_path, char *dos_opts)
{
  char linux_path_resolved[PATH_MAX];
  char dos_path [MAX_PATH_LENGTH];
  int drive;

  char *b;

  int is_probably_linux_path;

  if (*CommandStyle != EXEC_LINUX_PATH && *CommandStyle != EXEC_CHOICE) {
     error ("should not be in setupDOSCommand\n");
     return 1;
  }

  is_probably_linux_path = likely_linux_path(linux_path);

  if (*CommandStyle == EXEC_CHOICE) {
     if (is_probably_linux_path) {
       /*
        * It _looks like_ a linux path.
        *
        * If it doesn't exist, it is probably a typo rather than an
        * intention to EXEC_LITERAL.  By Stas Sergeev design, realpath()
        * below will cause the UNIX command - but not DOSEMU - to abort
        * with an error.  In contrast, EXEC_LITERAL always aborts/quits
        * DOSEMU after executing a bogus command.
        */
       *CommandStyle = EXEC_LINUX_PATH;
     } else {
       *CommandStyle = EXEC_LITERAL;
       return 0;
     }
  } else if (*CommandStyle == EXEC_LINUX_PATH) {
     if (!is_probably_linux_path) {
       /*
        * I doubt the program will actually run but we still try since:
        *
        * 1. the user said to interpret it as a linux path
        * 2. our heuristic is not perfect
        */
     }
  }

  if (!realpath (linux_path, linux_path_resolved)) {
      com_fprintf (com_stderr, "ERROR: %s: %s\n", strerror(errno), linux_path);
      return (1);
  }
  
  drive = find_drive (linux_path_resolved);
  if (drive < 0) {
    drive = -drive;
    
    if (drive >= 26) {
      if (drive == 26) {
        com_fprintf (com_stderr,
                     "ERROR: Cannot find a free DOS drive to use for LREDIR\n");
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
      com_fprintf (com_stderr,
                   "ERROR: Could not redirect %c: to /\n", drive + 'A');
      return (1);
    }
  }
  
  
  /* switch to the drive */
  j_printf ("Switching to drive %i (%c:)\n", drive, drive + 'A');
  com_dossetdrive (drive);
  if (com_dosgetdrive () != drive) {
    com_fprintf (com_stderr, "ERROR: Could not change to %c:\n", drive + 'A');
     
    if (com_dossetdrive (com_dosgetdrive ()) < 26)
      com_fprintf (com_stderr, "Try 'LASTDRIVE=Z' in CONFIG.SYS.\n");

    return (1);
  }

  
  make_unmake_dos_mangled_path (dos_path, linux_path_resolved,
                                drive, 1/*alias*/);
  j_printf ("DOS path: '%s' (from linux '%s')\n",
            dos_path, linux_path_resolved);

  /* switch to the directory */
  if (strlen (dos_path) >= 3 && (b = strrchr (dos_path, '\\')) != NULL) {
    char *dos_dir, *slash_ptr = dos_path + 2;
    int err;
    b++;
    dos_dir = strndup(slash_ptr, b - slash_ptr);
    
    j_printf ("Changing to directory '%s'\n", dos_dir);
    err = com_dossetcurrentdir (dos_dir);
    if (err) {
      com_fprintf (com_stderr,
                   "ERROR: Could not change to directory: %s\n",
                   dos_dir);
      free(dos_dir);
      return (1);
    }
    free(dos_dir);
  } else {
    com_fprintf (com_stderr, "INTERNAL ERROR: no backslash in DOS path\n");
    return (1);
  }
  

  /* return the 8.3 EXE name */
  strcpy (linux_path/*arg as return value*/, b);

  j_printf ("DOS cmd='%s'\n", linux_path);

  /* and append any dos options */
  if (dos_opts && *dos_opts) {
    strncat (linux_path, dos_opts, PATH_MAX - strlen (linux_path) - 1);
    linux_path [PATH_MAX - 1] = 0;
    j_printf ("\tAppended options '%s' to give DOS cmd '%s'\n",
              dos_opts, linux_path);
  }

  return (0);
}

static int do_execute_dos (int argc, char **argv, int CommandStyle)
{
  char cmd[PATH_MAX];
  int ret, terminate;
  char *options = NULL;


  /*
   * Get unprocessed command.
   */

  if (argc == 0) {
    options = misc_e6_options();
    ret = misc_e6_commandline(cmd);
  } else {
    strcpy (cmd, argv[0]);
    ret = misc_e6_envvar(cmd);
  }

  if (ret) {
    /* empty string, assume we had to exec -E and this wasn't given
     * ( may have 'unix -e' in your autoexec.bat )
     */
    return (1);
  }
  /*
   * If linux path (either EXEC_LINUX_PATH or setupDOSCommand() determines
   * it is), cmd be set to the appropriate DOS command.
   */

  /* Mutates CommandStyle, cmd. */
  if ((CommandStyle == EXEC_LINUX_PATH || CommandStyle == EXEC_CHOICE) &&
      setupDOSCommand (&CommandStyle, cmd, options)) {
        return (1);
  }

  /*
   * Decide whether we need to quit after running the DOS command.
   */

  terminate = misc_e6_need_terminate();
#if 0
   /* FIXTHIS: we must not terminate if the DOS path doesn't exist,
    * so we should check its existance, but it is difficult. */
  if (CommandStyle == EXEC_LITERAL /* && DOS path doesn't exist */) {
    terminate = 0;
  }
#endif

  /*
   * Execute DOS command.
   */

  if (*cmd == '\0')
    return (1);


  com_printf ("About to Execute : %s\n", cmd);
  config.quiet = 0;
  if (com_system (cmd, terminate)) {
    /* SYSTEM failed ... */
    com_fprintf (com_stderr, "SYSTEM failed ....(%d)\n", com_errno);
    return (1);
  }


  return (0);
}
#endif  /*CAN_EXECUTE_DOS*/

static int do_set_dosenv (int argc, char **argv)
{
  char data[256];

  if (argc == 0) return usage();

  strcpy (data, argv[0]);

  if (! misc_e6_envvar(data)) {
    if (msetenv(argv[0],data))
      return (0);
  }
  return (1);
}
