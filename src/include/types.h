#ifndef EMU_TYPES_H
#define EMU_TYPES_H

#include <sys/types.h>
#ifdef __linux__
#include <asm/types.h>
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/*
 * From linux asm/types.h
 * __xx is ok: it doesn't pollute the POSIX namespace. Use these in the
 * header files exported to user space
 */
#ifndef _I386_TYPES_H

typedef __signed__ char __s8;
typedef unsigned char __u8;

typedef __signed__ short __s16;
typedef unsigned short __u16;

typedef __signed__ int __s32;
typedef unsigned int __u32;

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
typedef __signed__ long long __s64;
typedef unsigned long long __u64;
#endif

#endif

typedef unsigned char      Boolean;
typedef unsigned char      Bit8u;   /* type of 8 bit unsigned quantity */
typedef char               Bit8s;   /* type of 8 bit signed quantity */
typedef unsigned short     Bit16u;  /* type of 16 bit unsigned quantity */
typedef short              Bit16s;  /* type of 16 bit signed quantity */
typedef unsigned long      Bit32u;  /* type of 32 bit unsigned quantity */
typedef long               Bit32s;  /* type of 32 bit signed quantity */
typedef unsigned long long Bit64u;  /* type of 64 bit unsigned quantity */
typedef          long long Bit64s;  /* type of 64 bit signed quantity */

#endif /* EMU_TYPES_H */
