/* stripped down from official jdmacros to use glibc functions */

#ifndef _JD_MACROS_H_
#define _JD_MACROS_H_

#define SLMEMSET memset
#define SLMEMCHR memchr
#define SLMEMCPY memcpy
#define SLMEMCMP memcmp
#define SLfree free
#define SLmalloc malloc
#define SLcalloc calloc
#define SLREALLOC realloc
#define HAVE_VSNPRINTF 1
#define HAVE_SNPRINTF 1
#define HAVE_GETGID 1
#define HAVE_GETEGID 1
#define HAVE_GETEUID 1
#define HAVE_GETUID 1
#define SLANG_POSIX_SIGNALS 1
#define _SLANG_SOURCE 1

#endif				       /* _JD_MACROS_H_ */
