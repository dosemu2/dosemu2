#include <unistd.h>
#include "emu.h"
#include "priv.h"

#if 0
#define PRIV_TESTING
#endif

/* Some handy information to have around */
static uid_t uid,euid;
static gid_t gid,egid;
static uid_t cur_uid, cur_euid;
static gid_t cur_gid, cur_egid;

#ifdef __NetBSD__
/* NOTE: Sorry, but the NetBSD stuff is broken currently,
         ... John Kohl is the man to help us with this ;-) */
int internal_priv_on(void)
{
    if (seteuid(euid) || setegid(egid)) {
	error("Cannot enable privs!\n");
	return (0);
    }
    return (1);
}

int internal_priv_off(void)
{
  if (seteuid(getuid()) || setegid(getgid())) {
    error("Cannot disable privs!\n");
    return (0);
  }
  return (1);
}
#endif /* __NetBSD__ */

#ifdef __linux__

#define PRIVS_ARE_ON (euid == cur_euid)
#define PRIVS_ARE_OFF (uid == cur_euid)
#define PRIVS_WERE_ON (pop_priv())

#define PRIV_STACK_SIZE 32
static int priv_stack[PRIV_STACK_SIZE+1];
static int privsp=0;

static __inline__  void push_priv(void)
{
  if (privsp >= PRIV_STACK_SIZE) {
    error("Aiiiee... overflow on privilege stack\n");
    leavedos(99);
  }
  priv_stack[privsp++] = PRIVS_ARE_ON;
#ifdef PRIV_TESTING
  c_printf("PRIV: pushing %d sp=%d\n", priv_stack[privsp-1], privsp);
#endif
}

static __inline__  int pop_priv(void)
{
  if (privsp <= 0) {
    error("Aiiiee... underflow on privilege stack\n");
    leavedos(99);
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: poping %d sp=%d\n", priv_stack[privsp-1], privsp);
#endif
  return priv_stack[--privsp];
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

int enter_priv_on(void)
{
  if (under_root_login) return 1;
  push_priv();
  return _priv_on();
}
  
int enter_priv_off(void)
{
  if (under_root_login) return 1;
  push_priv();
  return _priv_off();
}
  
int leave_priv_setting(void)
{
  if (under_root_login) return 1;
  if (PRIVS_WERE_ON) return _priv_on();
  return _priv_off();
}
  
int priv_iopl(int pl)
{
  if (PRIVS_ARE_OFF) {
    int ret;
    _priv_on();
    ret = iopl(pl);
    _priv_off();
    return ret;
  }
  return iopl(pl);
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

/* BIG NOTE:
 * The below 'priv_drop' should only be called from forked child processes !!
 */
int priv_drop(void)
{
  if (setreuid(uid,uid) || setregid(gid,gid))
    {
      error("Cannot drop root uid or gid!\n");
      return 0;
    }
  return 1;
}
#endif /* __linux__ */


void priv_init(void)
{
  uid  = cur_uid  = getuid();
  if (!uid) under_root_login =1;
  euid = cur_euid = geteuid();
  gid  = cur_gid  = getgid();
  egid = cur_egid = getegid();

  if ((euid != 0) && (uid == euid))
    {
      /* Nothing else is set up yet so don't try anything fancy */
      fprintf(stderr,"I must be setuid root (or run by root) to run!\n");
      exit(1);
    }
#if 0 /* we defined it as a macro, because it _alway_ is '1'
       * (the checks for it will be optimized away by GCC )
       */
  i_am_root = 1;
#endif

#ifndef RUN_AS_ROOT
  _priv_off();
#endif
}
