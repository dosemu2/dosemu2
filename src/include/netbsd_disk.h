#ifndef _NETBSD_DISK_H_
#define  _NETBSD_DISK_H_

#define partition nbsd_partition	/* avoid conflict with dosemu XXX */

#include <sys/disklabel.h>
#undef partition

#include <machine/ioctl_fd.h>

#endif
