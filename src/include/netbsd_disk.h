/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _NETBSD_DISK_H_
#define  _NETBSD_DISK_H_

#define partition nbsd_partition	/* avoid conflict with dosemu XXX */

#include <sys/disklabel.h>
#undef partition

#include <machine/ioctl_fd.h>

#endif
