/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>
#include <sys/io.h>
#include "emu.h"
#include "priv.h"
#include "dosemu_config.h"
#include "mapping.h"
#include "utilities.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

#if 0
#define PRIV_TESTING
#endif

/* Some handy information to have around */
static uid_t uid,euid;
static gid_t gid,egid;
static uid_t cur_uid, cur_euid;
static gid_t cur_gid, cur_egid;

static int skip_priv_setting = 0;


#ifdef __linux__

#define PRIVS_ARE_ON (euid == cur_euid)
#define PRIVS_ARE_OFF (uid == cur_euid)
#define PRIVS_WERE_ON(privs) (pop_priv(privs))


static __inline__  void push_priv(saved_priv_status *privs)
{
  if (!privs || *privs != PRIV_MAGIC) {
    error("Aiiiee... not in-sync saved priv status on push_priv\n");
    leavedos(99);
  }
  *privs = PRIVS_ARE_ON;
#ifdef PRIV_TESTING
  c_printf("PRIV: pushing %d privs_ptr=%p\n", *privs, privs);
#endif
}

static __inline__  int pop_priv(saved_priv_status *privs)
{
  int ret;
  if (!privs || *privs == PRIV_MAGIC) {
    error("Aiiiee... not in-sync saved priv status on pop_priv\n");
    leavedos(99);
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: poping %d privs_ptr=%p\n", *privs, privs);
#endif
  ret = (int)*privs;
  *privs = PRIV_MAGIC;
  return ret;
}

static __inline__ int _priv_on(void) {
  if (PRIVS_ARE_OFF) {  /* make sure the privs need to be changed */
#ifdef PRIV_TESTING
      c_printf("PRIV: on-in %d\n", cur_euid);
#endif
      if (setreuid(uid,euid)) {
         error("Cannot turn privs on!\n");
         return 0;
      }
      cur_uid = uid;
      cur_euid = euid;
      if (setregid(gid,egid)) {
	  error("Cannot turn privs on!\n");
	  return 0;
      }
      cur_gid = gid;
      cur_egid = egid;
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: on-ex %d\n", cur_euid);
#endif
  return 1;
}

static __inline__ int _priv_off(void) {
  if (PRIVS_ARE_ON) {  /* make sure the privs need to be changed */
#ifdef PRIV_TESTING
      c_printf("PRIV: off-in %d\n", cur_euid);
#endif
      if (setreuid(euid,uid)) {
	error("Cannot turn privs off!\n");
	return 0;
      }
      cur_uid = euid;
      cur_euid = uid;
      if (setregid(egid,gid)) {
	error("Cannot turn privs off!\n");
	return 0;
      }
      cur_gid = egid;
      cur_egid = gid;
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: off-ex %d\n", cur_euid);
#endif
  return 1;
}

int real_enter_priv_on(saved_priv_status *privs)
{
  if (skip_priv_setting) return 1;
  push_priv(privs);
  return _priv_on();
}
  
int real_enter_priv_off(saved_priv_status *privs)
{
  if (skip_priv_setting) return 1;
  push_priv(privs);
  return _priv_off();
}
  
int real_leave_priv_setting(saved_priv_status *privs)
{
  if (skip_priv_setting) return 1;
  if (PRIVS_WERE_ON(privs)) return _priv_on();
  return _priv_off();
}
  
int priv_iopl(int pl)
{
  int ret;
  if (PRIVS_ARE_OFF) {
    _priv_on();
    ret = iopl(pl);
    _priv_off();
  }
  else ret = iopl(pl);
#ifdef X86_EMULATOR
  if (config.cpuemu) e_priv_iopl(pl);
#endif
  if (ret == 0)
    current_iopl = pl;
  return ret;
}

uid_t get_cur_uid(void)
{
  return cur_uid;
}

uid_t get_cur_euid(void)
{
  return cur_euid;
}

gid_t get_cur_egid(void)
{
  return cur_egid;
}

uid_t get_orig_uid(void)
{
  return uid;
}

uid_t get_orig_euid(void)
{
  return euid;
}

gid_t get_orig_gid(void)
{
  return gid;
}

int priv_drop(void)
{
  if (setreuid(uid,uid) || setregid(gid,gid))
    {
      error("Cannot drop root uid or gid!\n");
      return 0;
    }
  cur_euid = euid = uid;
  cur_egid = egid = gid;
  skip_priv_setting = 1;
  if (uid) can_do_root_stuff = 0;
  return 1;
}
#endif /* __linux__ */

#define MAXGROUPS  20
static gid_t *groups;
static int num_groups = 0;


int is_in_groups(gid_t gid)
{
  int i;
  for (i=0; i<num_groups; i++) {
    if (gid == groups[i]) return 1;
  }
  return 0;
}


void priv_init(void)
{
  uid  = cur_uid  = getuid();
  if (!uid) under_root_login =1;
  euid = cur_euid = geteuid();
  if (!euid) can_do_root_stuff = 1;
  if (!uid) skip_priv_setting = 1;
  gid  = cur_gid  = getgid();
  egid = cur_egid = getegid();

  /* must store the /proc/self/exe symlink contents before dropping
     privs! */
  dosemu_proc_self_exe = readlink_malloc("/proc/self/exe");
  /* For Fedora we must also save a file descriptor to /proc/self/maps */
  dosemu_proc_self_maps_fd = open("/proc/self/maps", O_RDONLY);
  if (under_root_login)
  {
    /* check for sudo and set to original user */
    char *s = getenv("SUDO_GID");
    if (s) {
      gid = cur_gid = atoi(s);
      if (gid) {
        setregid(gid, egid);
      }
    }
    s = getenv("SUDO_UID");
    if (s) {
      uid = cur_uid = atoi(s);
      if (uid) {
        skip_priv_setting = under_root_login = 0;
	using_sudo = 1;
	s = getenv("SUDO_USER");
	if (s) {
	  initgroups(s, gid);
	  setenv("USER", s, 1);
	}
        setreuid(uid, euid);
      }
    }
  }
  
  if (!can_do_root_stuff)
    {
      skip_priv_setting = 1;
    }

  num_groups = getgroups(0,0);
  groups = malloc(num_groups * sizeof(gid_t));
  getgroups(num_groups,groups);

  if (!skip_priv_setting) _priv_off();
}

