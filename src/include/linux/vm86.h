/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Kludge for wrong <sys/vm86.h> in older libc headers:
 * in case of missing <linux/vm86.h> GCC falls back to this file
 * (we have -Isrc/include in all makefiles).
 * But NOTE: If the kernel has been upgraded by patch, then <linux/vm86.h>
 *           will be existing _and_ have ZERO size !
 *           In this case DELETE <linux/vm86.h>.
 */
#include <features.h>
#if GLIBC_VERSION_CODE >= 2000
#include <sys/vm86.h>
#else
#include <asm/vm86.h>
#endif
