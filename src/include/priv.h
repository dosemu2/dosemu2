#ifndef __PRIV_H__
#define __PRIV_H__

#include <unistd.h>
#include "extern.h"

/*  */
/* priv_on,priv_off,exchange_uids @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/dosext/mfs/mfs.c --> src/include/priv.h  */

#ifdef __NetBSD__
EXTERN uid_t euid;
EXTERN gid_t egid;
EXTERN priv_inited INIT(0);

static __inline__ int
priv_on(void)
{
    if (seteuid(euid) || setegid(egid)) {
	error("Cannot enable privs!\n");
	return (0);
    }
    return (1);
}
static __inline__ int
priv_off(void)
{
    if (!priv_inited) {
	euid = geteuid();
	egid = getegid();
	priv_inited = 1;
    }
    if (seteuid(getuid()) || setegid(getgid())) {
	error("Cannot disable privs!\n");
	return (0);
    }
    return (1);
}
#endif

#ifdef __linux__
EXTERN Bit32u priv_rootaccess INIT(1); /* Dosemu starts with root access */
static __inline__ int priv_on(void)
{
  if (!priv_rootaccess) {
  if (setreuid(geteuid(), getuid()) ||
      setregid(getegid(), getgid())) {
    error("Cannot exchange real/effective uid or gid!\n");
    return (0);
  }
  priv_rootaccess = 1;
  return (1);
  }
  else return (1);
}

static __inline__ int priv_off(void)
{
  if (priv_rootaccess) {
  if (setreuid(geteuid(), getuid()) ||
      setregid(getegid(), getgid())) {
    error("Cannot exchange real/effective uid or gid!\n");
    return (0);
  }
  priv_rootaccess = 0;
  return (1);
  }
  else return (1);
}

static __inline__ int exchange_uids(void)
{
  if (setreuid(geteuid(), getuid()) ||
      setregid(getegid(), getgid())) {
    error("Cannot exchange real/effective uid or gid!\n");
    return (0);
  }
  return (1);
}
#endif
/* @@@ MOVE_END @@@ 32768 */


#define priv_default() priv_on()

#endif /* __PRIV_H__ */
