#ifndef KERNEL_VERSION
#include <linux/version.h>
#define KERNEL_VERSION  (((LINUX_VERSION_CODE >> 16)*1000000) + \
			(((LINUX_VERSION_CODE >> 8) & 255) *1000) + \
			(LINUX_VERSION_CODE & 255) )
#if (KERNEL_VERSION < 2000028) || ((KERNEL_VERSION >= 2001000) && (KERNEL_VERSION < 2001015))
#error "wrong kernel version: < 2.0.28 or 2.1.x < 2.1.15"
#endif
#endif 
