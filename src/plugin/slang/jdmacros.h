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

#endif				       /* _JD_MACROS_H_ */
