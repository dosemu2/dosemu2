#include <unistd.h>
#include "priv.h"
#include "emu.h"

/* Some handy information to have around */
static uid_t uid,euid;
static gid_t gid,egid;

#ifdef __NetBSD__
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
int internal_priv_on(void)
{
  if (uid == geteuid())  /* make sure the privs need to be changed */
    {
      if (setreuid(uid,euid) || setregid(gid,egid))
	{
	  error("Cannont turn privs on!\n");
	  return 0;
	}
    }
  return 1;
}
  
int internal_priv_off(void)
{
  if (euid == geteuid())  /* make sure the privs need to be changed */
    {
      if (setreuid(euid,uid) || setregid(egid,gid))
	{
	  error("Cannont turn privs off!\n");
	  return 0;
	}
    }
  return 1;
}

int priv_drop(void)
{
  if (setreuid(uid,uid) || setreuid(gid,gid))
    {
      error("Cannot drop root uid or gid!\n");
      return 0;
    }
  return 1;
}
#endif /* __linux__ */


void priv_init(void)
{
  uid  = getuid();
  euid = geteuid();
  gid  = getgid();
  egid = getegid();

  if ((euid != 0) && (uid == euid))
    {
      /* Nothing else is set up yet so don't try anything fancy */
      fprintf(stderr,"I must be setuid root (or run by root) to run!\n");
      exit(1);
    }
  i_am_root = 1;
  warn("I am root\n");  /* traditional warning */
  priv_default();
}
