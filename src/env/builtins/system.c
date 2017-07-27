/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/************************************************
 * system.c
 * Run DOS commands specified by unix env vars
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

#include "msetenv.h"
#include "system.h"

#define CAN_EXECUTE_DOS 1
enum { EXEC_LINUX_PATH, EXEC_LITERAL };

static int usage (void);
#if CAN_EXECUTE_DOS
static int do_execute_dos (int argc, char **argv, int CommandStyle);
static int do_execute_cmdline(int argc, char **argv);
#endif
static int do_set_dosenv (int agrc, char **argv);


int system_main(int argc, char **argv)
{
  char c;
  const char *getopt_string = "ercs";

  if (argc == 1 ||
      (argc == 2 && !strcmp (argv[1], "/?"))) {
    return usage();
  }

  optind = 0;		// glibc wants this to reser parser state
  while ((c = getopt(argc, argv, getopt_string)) != EOF) {
    /* Got a switch */
    switch (c) {
#if CAN_EXECUTE_DOS
    case 'e':
      /* Execute the DOS command given in dosemu command line with -E or -K */
      if (unix_e_welcome) {
	show_welcome_screen();
	unix_e_welcome = 0;
      }
      return do_execute_cmdline (argc-2, argv+2);
    case 'r':
      /* Execute the DOS command given in the Linux environment variable */
      return do_execute_dos (argc-2, argv+2, EXEC_LITERAL);
    case 'c':
      /* Execute the DOS program whose Linux path is given in the Linux environment */
      return do_execute_dos (argc-2, argv+2, EXEC_LINUX_PATH);
#endif
    case 's':
      /* SETENV from unix env */
      return do_set_dosenv (argc-2, argv+2);

    default:
      break;
    }
  }

  return usage();

}


static int usage (void)
{
  com_printf ("Usage: SYSTEM [FLAG COMMAND]\n\n");
#if CAN_EXECUTE_DOS
  com_printf ("SYSTEM -r ENVVAR\n");
  com_printf ("  Execute the DOS command given in the Linux environment variable \"ENVVAR\".\n\n");
  com_printf ("SYSTEM -c ENVVAR\n");
  com_printf ("  Execute the DOS program whose Linux path is given in the Linux environment\n");
  com_printf ("  variable \"ENVVAR\".\n\n");
  com_printf ("SYSTEM -e\n");
  com_printf ("  Execute the DOS command given in dosemu command line with -E or -K.\n\n");
#endif
  com_printf ("SYSTEM -s ENVVAR [DOSVAR]\n");
  com_printf ("  Set the DOS environment to the Linux environment variable \"ENVVAR\".\n\n");
  com_printf ("SYSTEM\n");
  com_printf ("SYSTEM /?\n");
  com_printf ("  show this help screen\n");

  return (1);
}

#if CAN_EXECUTE_DOS
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
static int setupDOSCommand (int CommandStyle, const char *linux_path,
	char *dos_opts, char *dos_cmd)
{
  char *linux_path_resolved;
  char dos_path [MAX_PATH_LENGTH];
  int drive;
  char *b;

  if (CommandStyle != EXEC_LINUX_PATH) {
     error ("should not be in setupDOSCommand\n");
     return 1;
  }

  linux_path_resolved = canonicalize_file_name(linux_path);
  if (!linux_path_resolved) {
      com_fprintf(com_stderr, "No such file: %s\n", linux_path);
      warn("No such file: %s\n", linux_path);
      if (config.exit_on_cmd)
          leavedos(48);
      return (1);
  }

  drive = find_drive (&linux_path_resolved);
  if (drive < 0) {
    drive = find_free_drive();

    if (drive < 0) {
      com_fprintf (com_stderr,
                     "ERROR: Cannot find a free DOS drive to use for LREDIR\n");
      free(linux_path_resolved);
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
      free(linux_path_resolved);
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

    free(linux_path_resolved);
    return (1);
  }


  make_unmake_dos_mangled_path (dos_path, linux_path_resolved,
                                drive, 1/*alias*/);
  j_printf ("DOS path: '%s' (from linux '%s')\n",
            dos_path, linux_path_resolved);
  free(linux_path_resolved);

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
  strcpy(dos_cmd, b);
  j_printf ("DOS cmd='%s'\n", dos_cmd);

  /* and append any dos options */
  if (dos_opts && *dos_opts) {
    strncat(dos_cmd, dos_opts, PATH_MAX - strlen(dos_cmd) - 1);
    dos_cmd[PATH_MAX - 1] = 0;
    j_printf ("\tAppended options '%s' to give DOS cmd '%s'\n",
              dos_opts, dos_cmd);
  }

  return (0);
}

static int do_execute_dos (int argc, char **argv, int CommandStyle)
{
  const char *cmd;
  char buf[PATH_MAX];
  char *options = NULL;

  if (!argc)
    return 1;
  cmd = getenv(argv[0]);
  if (!cmd)
    return (1);
  /*
   * If linux path (either EXEC_LINUX_PATH or setupDOSCommand() determines
   * it is), cmd be set to the appropriate DOS command.
   */

  /* Mutates CommandStyle, cmd. */
  if (CommandStyle == EXEC_LINUX_PATH) {
    if (setupDOSCommand(CommandStyle, cmd, options, buf))
      return 1;
    cmd = buf;
  }

  com_printf ("About to Execute : %s\n", cmd);
  config.quiet = 0;
  if (com_system (cmd, 0)) {
    /* SYSTEM failed ... */
    com_fprintf (com_stderr, "SYSTEM failed ....(%d)\n", com_errno);
    return (1);
  }


  return (0);
}

static int do_execute_cmdline(int argc, char **argv)
{
  const char *cmd;
  char buf[PATH_MAX];
  int ret, is_ux, terminate;
  char *options = NULL;

  options = misc_e6_options();
  ret = misc_e6_commandline(buf, &is_ux);
  if (ret) {
    /* empty string, assume we had to exec -E and this wasn't given
     * ( may have 'unix -e' in your autoexec.bat )
     */
    return (1);
  }
  cmd = buf;
  terminate = misc_e6_need_terminate();
  /* Mutates CommandStyle, cmd. */
  if (is_ux) {
    if (setupDOSCommand(EXEC_LINUX_PATH, cmd, options, buf))
      return 1;
  }

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
  char* data;
  char* varname;

  if (argc == 0) return usage();

  if (argc == 1) {
    varname = argv[0];
  } else {
    varname = argv[1];
  }

  data = getenv(argv[0]);
  if (data) {
    if (msetenv(varname, data))
      return (0);
  }
  return (1);
}
