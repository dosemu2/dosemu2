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
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include <unistd.h>

#include "init.h"
#include "int.h"
#include "builtins.h"
#include "utilities.h"
#include "../../dosext/mfs/lfn.h"
#include "redirect.h"
#include "msetenv.h"
#include "system.h"

static int usage (void);
static int do_execute_dos(int argc, char **argv);
static int do_execute_cmdline(int parent);
static int do_set_dosenv (int agrc, char **argv);
static void do_parse_vars(const char *str, char drv, int parent);

static char e_drv;
static int vars_parsed;
static int *drv_num_p;

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
    case 'e':
      /* Execute the DOS command given in dosemu command line with -E or -K */
      is_e = 1;
      break;
    case 'r':
      /* Execute the DOS command given in the Linux environment variable */
      return do_execute_dos(argc-2, argv+2);
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
    return do_execute_cmdline(is_p);

  return usage();
}


static int usage (void)
{
  com_printf ("Usage: SYSTEM [FLAG COMMAND]\n\n");
  com_printf ("SYSTEM -r ENVVAR\n");
  com_printf ("  Execute the DOS command given in the Linux environment variable \"ENVVAR\".\n\n");
  com_printf ("SYSTEM -e\n");
  com_printf ("  Execute the DOS command given in dosemu command line with -E or -K.\n\n");
  com_printf ("SYSTEM -s ENVVAR [DOSVAR]\n");
  com_printf ("  Set the DOS environment to the Linux environment variable \"ENVVAR\".\n\n");
  com_printf ("SYSTEM -p\n");
  com_printf ("  Set DOS environment variables passed via dosemu command line\n\n");
  com_printf ("SYSTEM\n");
  com_printf ("SYSTEM /?\n");
  com_printf ("  show this help screen\n");

  return (1);
}

static int setupDOSCommand(const char *dos_path, char *r_drv)
{
  int drive = *drv_num_p;
  char drvStr[2];

  if (drive < 0) {
    com_fprintf(com_stderr, "ERROR: Cannot find a drive\n");
    return (1);
  }
  if (dos_path && dos_path[0])
    msetenv("DOSEMU_SYS_DIR", dos_path);
  drvStr[0] = drive + 'A';
  drvStr[1] = '\0';
  msetenv("DOSEMU_SYS_DRV", drvStr);
  if (r_drv)
    *r_drv = drvStr[0];
  return (0);
}

static int do_system(const char *cmd, int terminate)
{
  com_printf ("About to Execute : %s\n", cmd);
  config.tty_stderr = 0;
  if (terminate)
    msetenv("DOSEMU_EXIT", "1");
  msetenv("DOSEMU_SYS_CMD", cmd);
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

static void _do_parse_vars(char *str, char drv, int parent)
{
  char *p, *p0, *p1, *sub;
#define MAX_RESOURCE_PATH_LENGTH 128
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
    while ((sub = strstr(p, "%D"))) {
      if (!drv) {
        error("cannot expand %%D, no drive found\n");
        leavedos(32);
      }
      sub[0] = drv;
      sub[1] = ':';
    }
    /* %P means only at parent env */
    if (p0 && strncmp(p0, "%P", 2) == 0) {
      if (parent) {
        strlcpy(buf2, p0 + 2, sizeof(buf2));
        p0 = buf2;
      } else {
        p0 = NULL;
      }
    }
    /* %C means only at child env */
    if (p0 && strncmp(p0, "%C", 2) == 0) {
      if (!parent) {
        strlcpy(buf2, p0 + 2, sizeof(buf2));
        p0 = buf2;
      } else {
        p0 = NULL;
      }
    }
    if (p0) {
      /* %O expands to old value */
      if ((sub = strstr(p, "%O"))) {
        char *old = mgetenv(p0);
        int offs = sub - p;
        if (old)
          snprintf(buf, sizeof(buf), "%.*s%s%s", offs, p, old, p + offs + 2);
        else
          snprintf(buf, sizeof(buf), "%.*s%s", offs, p, p + offs + 2);
        p = buf;
      }

//      com_printf("setenv %s=%s\n", p0, p);
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

static int do_execute_cmdline(int parent)
{
  char *vars, drv = e_drv;
  int ret = 0, first = 0;

  if (!e_drv && config.unix_path) {
    int err = setupDOSCommand(config.dos_path, &drv);
    if (err)
      return err;
    e_drv = drv;  // store for later -p
    first = 1;
  }
  vars = misc_e6_options();
  if (vars) {
    uint16_t ppsp = com_parent_psp_seg();
    /* if we have a parent then we are not command.com (hack) */
    if (ppsp && ppsp != com_psp_seg()) {
      if (parent && first) {
        do_parse_vars(vars, drv, 1);
        vars_parsed = 1;
      }
      if (!vars_parsed || first) {
        mresize_env(strlen(vars));
        do_parse_vars(vars, drv, 0);
      }
    } else if (first) {
      do_parse_vars(vars, drv, 0);
      vars_parsed = 1;
    }
  }
  if (config.dos_cmd)
    ret = do_system(config.dos_cmd, config.exit_on_cmd);
  return ret;
}

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
  if (!config.unix_path)
    return;
  if (config.unix_path[0] != '/') {
    /* omitted unix path means current dir */
    const char *u = config.unix_path[0] ? config.unix_path : ".";
    char *u_path = malloc(PATH_MAX);
    if (!realpath(u, u_path)) {
      free(u_path);
      goto err;
    }
    free(config.unix_path);
    config.unix_path = u_path;
  }
  if (!config.dos_cmd) {
    char *p;
    if (!exists_dir(config.unix_path)) {
      /* hack to support full path in -K */
      if (!exists_file(config.unix_path))
        goto err;
      p = strrchr(config.unix_path, '/');
      if (!p)
        goto err;
      config.dos_cmd = p + 1;
      *p = 0;
    }
  }
  drv_num_p = add_extra_drive(config.unix_path, 0, 0);
  if (!drv_num_p)
    goto err;
  return;

err:
  free(config.unix_path);
  config.unix_path = NULL;
}

CONSTRUCTOR(static void init(void))
{
  register_config_scrub(system_scrub);
}
