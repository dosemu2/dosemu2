/*
 * $Id: netbsd_disk.h,v 1.1 1995/03/10 03:11:23 jtk Exp $
 */

#ifndef _NETBSD_DISK_H_
#define  _NETBSD_DISK_H_

#define partition nbsd_partition	/* avoid conflict with dosemu XXX */

#include <sys/disklabel.h>
#undef partition

#include <machine/ioctl_fd.h>

#endif
