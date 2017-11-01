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

#include "init.h"
#include "builtins.h"
#include "utilities.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"
#include "msetenv.h"
#include "system.h"

#define CAN_EXECUTE_DOS 1

static int usage (void);
#if CAN_EXECUTE_DOS
static int do_execute_dos(int argc, char **argv);
static int do_execute_linux(int argc, char **argv);
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
      return do_execute_dos(argc-2, argv+2);
    case 'c':
      /* Execute the DOS program whose Linux path is given in the Linux environment */
      return do_execute_linux(argc-2, argv+2);
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
static int setupDOSCommand(const char *linux_path, char *r_drv)
{
  char dos_path [MAX_PATH_LENGTH];
  int drive;
  int err;
  char *dos_dir;

  drive = find_free_drive();
  if (drive < 0) {
    com_fprintf (com_stderr,
                     "ERROR: Cannot find a free DOS drive to use for LREDIR\n");
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

  /* switch to the drive */
  j_printf ("Switching to drive %i (%c:)\n", drive, drive + 'A');
  com_dossetdrive (drive);
  if (com_dosgetdrive () != drive) {
    com_fprintf (com_stderr, "ERROR: Could not change to %c:\n", drive + 'A');
    if (com_dossetdrive (com_dosgetdrive ()) < 26)
      com_fprintf (com_stderr, "Try 'LASTDRIVE=Z' in CONFIG.SYS.\n");
    return (1);
  }

  make_unmake_dos_mangled_path (dos_path, linux_path, drive, 1/*alias*/);
  j_printf ("DOS path: '%s' (from linux '%s')\n", dos_path, linux_path);

  /* switch to the directory */
  if (strlen (dos_path) < 3) {
    com_fprintf (com_stderr, "INTERNAL ERROR: DOS path %s invalid\n", dos_path);
    return (1);
  }
  dos_dir = dos_path + 2;
  j_printf ("Changing to directory '%s'\n", dos_dir);
  err = com_dossetcurrentdir (dos_dir);
  if (err) {
    com_fprintf (com_stderr,
                   "ERROR: Could not change to directory: %s\n",
                   dos_dir);
    return (1);
  }

  if (r_drv)
    *r_drv = drive + 'A';
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

static int do_execute_dos(int argc, char **argv)
{
  const char *cmd;

  if (!argc)
    return 1;
  cmd = getenv(argv[0]);
  if (!cmd)
    return (1);
  return do_system(cmd, 0);
}

static int do_execute_linux(int argc, char **argv)
{
  const char *cmd;
  char buf[PATH_MAX];
  char *p;

  if (!argc)
    return 1;
  cmd = getenv(argv[0]);
  if (!cmd)
    return (1);
  strlcpy(buf, cmd, sizeof(buf));
  p = strrchr(buf, '/');
  if (!p)
    return 1;
  *p = 0;
  p++;
  if (setupDOSCommand(buf, NULL))
    return 1;
  return do_system(p, 0);
}

static void do_parse_vars(char *str, char drv)
{
  char *p, *p0, *p1;

  /* find env=val patterns in option and call msetenv() on them */
  while (1) {
    if (!(p = strchr(str, '=')))
      return;
    if (p == str || p[1] == 0)
      return;
    *p = 0;
    p0 = strrchr(str, ' ');
    if (!p0)
      p0 = str;
    else
      p0++;
    p++;
    p1 = strchr(p, ' ');
    if (p1)
      *p1 = 0;
    if (drv && strncmp(p, "%D", 2) == 0) {
      p[0] = drv;
      p[1] = ':';
    }
    com_printf("setenv %s=%s\n", p0, p);
    msetenv(p0, p);
    if (!p1)
      break;
    p1++;
    str = p1;
  }
}

static int do_prepare_exec(int argc, char **argv, char *r_drv)
{
  *r_drv = 0;
  if (config.unix_path) {
    if (!config.dos_cmd) {
	/* system_scrub() should validate this, cant be here */
	com_printf("ERROR: config.dos_cmd not set\n");
	return 1;
    }
    if (setupDOSCommand(config.unix_path, r_drv))
      return 1;
  } else {
    if (!config.dos_cmd)
      return 0;		// nothing to execute
    com_printf("ERROR: config.unix_path not set\n");
    return 1;
  }

  return 2;
}

static int do_execute_cmdline(int argc, char **argv)
{
  char *vars, drv;
  int ret;

  ret = do_prepare_exec(argc, argv, &drv);
  vars = misc_e6_options();
  if (vars)
    do_parse_vars(vars, drv);
  if (ret == 2)
    ret = do_system(config.dos_cmd, config.exit_on_cmd);
  return ret;
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

static void system_scrub(void)
{
  char *u_path;

  if (config.unix_path) {
    u_path = malloc(PATH_MAX);
    if (!realpath(config.unix_path, u_path))
      goto err;
    config.unix_path = u_path;
    if (!config.dos_cmd) {
      char *p = strrchr(u_path, '/');
      if (!p)
        goto err;
      config.dos_cmd = p + 1;
      *p = 0;
    }
  } else if (config.dos_cmd) {
    u_path = malloc(PATH_MAX);
    if (!getcwd(u_path, PATH_MAX))
      goto err;;
    config.unix_path = u_path;
  }
  return;

err:
  free(u_path);
  config.unix_path = NULL;
}

CONSTRUCTOR(static void init(void))
{
  register_config_scrub(system_scrub);
}
