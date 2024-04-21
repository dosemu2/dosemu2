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
 * Purpose: flipping priviledges
 *
 * Authors:
 *   Loosely based on dosemu1's priv.c by Hans Lermen, Eric Biederman
 *   and Bart Oldeman.
 *
 *   Rewritten for dosemu2 by @stsp
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#include "emu.h"
#include "priv.h"
#include "dosemu_config.h"
#include "utilities.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

/* Some handy information to have around */
static uid_t uid,euid;
static gid_t gid,egid;
static uid_t cur_euid;
static gid_t cur_egid;
static int suid, sgid;

static int skip_priv_setting = 0;

int can_do_root_stuff;
int under_root_login;
int using_sudo;
int current_iopl;

#define PRIVS_ARE_ON (euid == cur_euid)
#define PRIVS_ARE_OFF (uid == cur_euid)

static int _priv_on(void)
{
  if (seteuid(euid)) {
    error("Cannot turn privs on!\n");
    return 0;
  }
  cur_euid = euid;
  if (setegid(egid)) {
    error("Cannot turn privs on!\n");
    return 0;
  }
  cur_egid = egid;
  return 1;
}

static int _priv_off(void)
{
  if (seteuid(uid)) {
    error("Cannot turn privs off!\n");
    return 0;
  }
  cur_euid = uid;
  if (setegid(gid)) {
    error("Cannot turn privs off!\n");
    return 0;
  }
  cur_egid = gid;
  return 1;
}

int real_enter_priv_on(void)
{
  if (skip_priv_setting) return 1;
  assert(PRIVS_ARE_OFF);
  return _priv_on();
}

int real_leave_priv_setting(void)
{
  if (skip_priv_setting) return 1;
  assert(PRIVS_ARE_ON);
  return _priv_off();
}

int priv_iopl(int pl)
{
#ifdef HAVE_SYS_IO_H
  int ret;
  assert(PRIVS_ARE_OFF);
  _priv_on();
  ret = iopl(pl);
  _priv_off();
#ifdef X86_EMULATOR
  if (config.cpu_vm == CPUVM_EMU) e_priv_iopl(pl);
#endif
  if (ret == 0)
    current_iopl = pl;
  return ret;
#else
  return -1;
#endif
}

uid_t get_orig_uid(void)
{
  return uid;
}

gid_t get_orig_gid(void)
{
  return gid;
}

uid_t get_suid(void)
{
  assert(suid);
  return euid;
}

gid_t get_sgid(void)
{
  assert(sgid);
  return egid;
}

int priv_drop(void)
{
  if (skip_priv_setting)
    return 1;
  assert(PRIVS_ARE_OFF);
  /* We set the same values as they are now.
   * The trick is that if the first arg != -1 then saved-euid is reset.
   * This allows to avoid the use of non-standard setresuid(). */
  if (setreuid(uid, cur_euid) || setregid(gid, cur_egid)) {
    error("Cannot drop root uid or gid!\n");
    return 0;
  }
  /* Now check that saved-euids are actually reset: privs should fail. */
  if (seteuid(euid) == 0 || setegid(egid) == 0) {
    error("privs were not dropped\n");
    leavedos(3);
    return 0;
  }
  skip_priv_setting = 1;
  if (uid) can_do_root_stuff = 0;
  return 1;
}

void priv_drop_total(void)
{
  if (suid) {
    seteuid(euid);
    if (setreuid(euid, euid) != 0)
      error("Cannot drop suid: %s\n", strerror(errno));
    /* make sure privs were dropped */
    if (seteuid(uid) == 0) {
      error("suid: privs were not dropped\n");
      leavedos(3);
      return;
    }
    suid++;
  }
  if (sgid) {
    setegid(egid);
    if (setregid(egid, egid) != 0)
      error("Cannot drop sgid: %s\n", strerror(errno));
    /* make sure privs were dropped */
    if (setegid(gid) == 0) {
      error("sgid: privs were not dropped\n");
      leavedos(3);
      return;
    }
    sgid++;
  }
}

int running_suid_orig(void)
{
  if (!suid)
    return 0;
  assert(suid == 1);
  return 1;
}

int running_suid_changed(void)
{
  if (!suid)
    return 0;
  assert(suid == 2);
  return 1;
}

void priv_init(void)
{
  int err;
  const char *sh = getenv("SUDO_HOME"); // theoretical future var
  const char *h = getenv("HOME");
  uid = getuid();
  /* suid bit only sets euid & suid but not uid, sudo sets all 3 */
  if (!uid) under_root_login = 1;
  euid = cur_euid = geteuid();
  if (!euid) can_do_root_stuff = 1;
  if (!uid && !euid) skip_priv_setting = 1;
  gid = getgid();
  egid = cur_egid = getegid();

  /* must store the /proc/self/exe symlink contents before dropping
     privs! */
  dosemu_proc_self_exe = readlink_malloc("/proc/self/exe");
  /* For Fedora we must also save a file descriptor to /proc/self/maps */
  dosemu_proc_self_maps_fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);

  if (euid && uid && euid != uid) {
    dbug_printf("suid %i detected\n", euid);
    suid++;
    err = seteuid(uid);
    assert(!err);
  }
  if (egid && gid && egid != gid) {
    dbug_printf("sgid %i detected\n", egid);
    sgid++;
    err = setegid(gid);
    assert(!err);
  }

  if (!sh)
    sh = getenv("DOSEMU_SUDO_HOME");
  /* see if -E was used */
  if (under_root_login && sh && h && strcmp(sh, h) == 0) {
    /* check for sudo and set to original user */
    char *s = getenv("SUDO_GID");
    if (s) {
      gid = atoi(s);
      if (gid) {
        setregid(gid, egid);
      }
    }
    s = getenv("SUDO_UID");
    if (s) {
      uid = atoi(s);
      if (uid) {
        skip_priv_setting = under_root_login = 0;
	using_sudo = 1;
        setreuid(uid, euid);
      }
    }
  }

  if (!can_do_root_stuff)
    skip_priv_setting = 1;

  if (!skip_priv_setting) _priv_off();
}
