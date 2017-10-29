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
 * Returns 0 on success, nonzero on failure.
 */
static int setupDOSCommand(const char *linux_path, char *dos_cmd)
{
  char *linux_path_resolved;
  char dos_path [MAX_PATH_LENGTH];
  int drive;
  int err;
  char *dos_dir;

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
  if (strlen (dos_path) < 3) {
    com_fprintf (com_stderr, "INTERNAL ERROR: DOS path %s invalid\n", dos_path);
    return (1);
  }
  dos_dir = dos_path + 2;
  if (dos_cmd) {
    char *b = strrchr(dos_dir, '\\');
    if (!b) {
      com_fprintf (com_stderr, "INTERNAL ERROR: no backslash in DOS path\n");
      return (1);
    }
    *b = 0;
    b++;
    /* return the 8.3 EXE name */
    strcpy(dos_cmd, b);
    j_printf ("DOS cmd='%s'\n", dos_cmd);
  }
  j_printf ("Changing to directory '%s'\n", dos_dir);
  err = com_dossetcurrentdir (dos_dir);
  if (err) {
    com_fprintf (com_stderr,
                   "ERROR: Could not change to directory: %s\n",
                   dos_dir);
    return (1);
  }

  return (0);
}

static int do_system(const char *cmd, int terminate)
{
  com_printf ("About to Execute : %s\n", cmd);
  config.quiet = 0;
  if (com_system(cmd, terminate)) {
    /* SYSTEM failed ... */
    com_fprintf (com_stderr, "SYSTEM failed ....(%d)\n", com_errno);
    return (1);
  }
  return (0);
}

static int do_execute_dos (int argc, char **argv, int CommandStyle)
{
  const char *cmd;
  char buf[PATH_MAX];

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
    if (setupDOSCommand(cmd, buf))
      return 1;
    cmd = buf;
  }

  return do_system(cmd, 0);
}

static void do_parse_options(char *str)
{
  char *p, *p0, *p1, *beg;

  /* find env=val patterns in option and call msetenv() on them */
  while (1) {
    if (!(p = strchr(str, '=')))
      return;
    if (p == str || p[1] == 0)
      return;
    *p = 0;
    p0 = strrchr(str, ' ');
    if (!p0) {
      beg = str;
      p0 = str;
    } else {
      beg = p0;
      p0++;
    }
    p++;
    p1 = strchr(p, ' ');
    if (p1)
      *p1 = 0;
    com_printf("setenv %s=%s\n", p0, p);
    msetenv(p0, p);
    if (p1) {
      p1++;
      memmove(p0, p1, strlen(p1) + 1);
    } else {
      *beg = 0;
      return;
    }
  }
}

static int do_execute_cmdline(int argc, char **argv)
{
  const char *cmd;
  char buf[PATH_MAX];
  char buf1[PATH_MAX];
  int ret, is_ux, terminate;
  char *options = NULL;

  options = misc_e6_options();
  if (options)
    do_parse_options(options);
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
    if (setupDOSCommand(cmd, buf))
      return 1;
  } else {
    if (!getcwd(buf1, sizeof(buf1)))
      return 1;
    if (setupDOSCommand(buf1, NULL))
      return 1;
  }
  if (options && options[0]) {
    /* options already prepended with space */
    strcat(buf, options);
  }

  return do_system(cmd, terminate);
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
