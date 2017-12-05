/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: Run DOS commands specified by dosemu2 cmdline.
 * Also make-style environment passing with different substitution rules.
 *
 * Author: Stas Sergeev.
 * Reused some code snippets from unix.c of dosemu1, written by
 * Clarence Dang.
 *
 */


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
#include "redirect.h"
#include "msetenv.h"
#include "system.h"

#define CAN_EXECUTE_DOS 1

static int usage (void);
#if CAN_EXECUTE_DOS
static int do_execute_dos(int argc, char **argv);
static int do_execute_linux(int argc, char **argv);
static int do_execute_cmdline(int argc, char **argv, int parent);
#endif
static int do_set_dosenv (int agrc, char **argv);
static void do_parse_vars(const char *str, char drv, int parent);

static char e_drv;

int system_main(int argc, char **argv)
{
  char c;
  int is_e = 0, is_p = 0;
  const char *getopt_string = "ercsp";

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
      is_e = 1;
      break;
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

    case 'p':
      is_p = 1;
      break;

    default:
      break;
    }
  }

  if (is_p && !is_e) {
    char *vars = misc_e6_options();
    if (vars)
      do_parse_vars(vars, e_drv, 1);
    return 0;
  }
  if (is_e)
    return do_execute_cmdline(argc-2, argv+2, is_p);

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
  com_printf ("SYSTEM -p\n");
  com_printf ("  Set DOS environment variables passed via dosemu command line\n\n");
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
static int setupDOSCommand(const char *linux_path, int n_up, char *r_drv)
{
#define MAX_RESOURCE_PATH_LENGTH   128
  char dos_path [MAX_PATH_LENGTH];
  char resourceStr[MAX_RESOURCE_PATH_LENGTH];
  int drive;
  int err;
  int i;
  char *dos_dir;
  char *path1, *p;
  char drvStr[3];

  drive = find_free_drive();
  if (drive < 0) {
    com_fprintf (com_stderr,
                     "ERROR: Cannot find a free DOS drive to use for LREDIR\n");
    return (1);
  }

  snprintf(drvStr, sizeof drvStr, "%c:", 'A' + drive);

  path1 = strdup(linux_path);
  i = n_up;
  while (i--) {
    p = strrchr(path1, '/');
    if (!p) {
      free(path1);
      error("Path \"%s\" does not contain %i components\n", linux_path, n_up);
      return 1;
    }
    *p = 0;
  }
  if (!path1[0])
    strcpy(path1, "/");
  g_printf("Redirecting %s to %s\n", drvStr, path1);
  snprintf(resourceStr, sizeof(resourceStr), LINUX_RESOURCE "%s", path1);

  err = com_RedirectDevice(drvStr, resourceStr, REDIR_DISK_TYPE, 0 /* rw */);
  free(path1);
  if (err) {
    com_fprintf(com_stderr, "ERROR: Could not redirect %s to /\n", drvStr);
    return (1);
  }

  /* switch to the drive */
  g_printf ("Switching to drive %i (%s)\n", drive, drvStr);
  com_dossetdrive (drive);
  if (com_dosgetdrive () != drive) {
    com_fprintf (com_stderr, "ERROR: Could not change to %s\n", drvStr);
    if (com_dossetdrive (com_dosgetdrive ()) < 26)
      com_fprintf (com_stderr, "Try 'LASTDRIVE=Z' in CONFIG.SYS.\n");
    return (1);
  }

  err = make_unmake_dos_mangled_path(dos_path, linux_path, drive, 1/*mangle*/);
  if (err) {
    com_fprintf(com_stderr, "INTERNAL ERROR: path %s not resolved\n",
        linux_path);
    return 1;
  }
  g_printf ("DOS path: '%s' (from linux '%s')\n", dos_path, linux_path);

  /* switch to the directory */
  if (strlen(dos_path) < 3) {
    com_fprintf(com_stderr, "INTERNAL ERROR: DOS path %s invalid\n", dos_path);
    return 1;
  }
  dos_dir = dos_path + 2;
  g_printf ("Changing to directory '%s'\n", dos_dir);
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
  if (setupDOSCommand(buf, 0, NULL))
    return 1;
  return do_system(p, 0);
}

static void _do_parse_vars(char *str, char drv, int parent)
{
  char *p, *p0, *p1, *sub;
  char buf[MAX_RESOURCE_PATH_LENGTH];
  char buf2[MAX_RESOURCE_PATH_LENGTH];

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
    /* %D expands to drive letter */
    if (drv) {
      while ((sub = strstr(p, "%D"))) {
        sub[0] = drv;
        sub[1] = ':';
      }
    }
    /* %P means only at parent env */
    if (p0 && strncmp(p0, "%P", 2) == 0) {
      if (parent) {
        strcpy(buf2, p0 + 2);
        p0 = buf2;
      } else {
        p0 = NULL;
      }
    }
    /* %C means only at child env */
    if (p0 && strncmp(p0, "%C", 2) == 0) {
      if (!parent) {
        strcpy(buf2, p0 + 2);
        p0 = buf2;
      } else {
        p0 = NULL;
      }
    }
    if (p0) {
      /* %O expands to old value */
      if ((sub = strstr(p, "%O"))) {
        char *old = parent ? mgetenv(p0) : mgetenv_child(p0);
        int offs = sub - p;
        if (old)
          snprintf(buf, sizeof(buf), "%.*s%s%s", offs, p, old, p + offs + 2);
        else
          snprintf(buf, sizeof(buf), "%.*s%s", offs, p, p + offs + 2);
        p = buf;
      }

      com_printf("setenv %s=%s\n", p0, p);
      if (parent)
        msetenv(p0, p);
      else
        msetenv_child(p0, p);
    }
    if (!p1)
      break;
    p1++;
    str = p1;
  }
}

static void do_parse_vars(const char *str, char drv, int parent)
{
  char *s = strdup(str);
  _do_parse_vars(s, drv, parent);
  free(s);
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
    if (setupDOSCommand(config.unix_path, config.cdup, r_drv))
      return 1;
  } else {
    if (!config.dos_cmd)
      return 0;		// nothing to execute
    com_printf("ERROR: config.unix_path not set\n");
    return 1;
  }

  return 2;
}

static int do_execute_cmdline(int argc, char **argv, int parent)
{
  char *vars, drv;
  int ret;

  ret = do_prepare_exec(argc, argv, &drv);
  vars = misc_e6_options();
  if (vars) {
    mresize_env(strlen(vars));
    do_parse_vars(vars, drv, 0);
    if (parent)
      do_parse_vars(vars, drv, 1);
    e_drv = drv;	// store for later -p
  }
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
      return 1;
  }
  return 0;
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
