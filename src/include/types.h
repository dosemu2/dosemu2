/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef EMU_TYPES_H
#define EMU_TYPES_H

/* will include bitypes.h, linux/posix_types.h and asm/types.h */
#include <sys/types.h>
#if GLIBC_VERSION_CODE >= 2000
#  include "Asm/types.h"
#endif

#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#ifndef True
#define True  TRUE
#endif
#ifndef False
#define False FALSE
#endif

#ifndef DOSEMU_BOOL_IS_CHAR
typedef unsigned char boolean;
#define DOSEMU_BOOL_IS_CHAR
#endif

typedef u_int64_t hitimer_t;

typedef union {
  u_int64_t td;
  struct { u_int32_t tl,th; } t;
} u_int64_u;

typedef union {
  int64_t td;
  struct { int32_t tl,th; } t;
} int64_u;

typedef u_int64_u hitimer_u;

typedef unsigned char      Boolean;
typedef u_int8_t           Bit8u;   /* type of 8 bit unsigned quantity */
typedef   int8_t           Bit8s;   /* type of 8 bit signed quantity */
typedef u_int16_t          Bit16u;  /* type of 16 bit unsigned quantity */
typedef   int16_t          Bit16s;  /* type of 16 bit signed quantity */
typedef u_int32_t          Bit32u;  /* type of 32 bit unsigned quantity */
typedef   int32_t          Bit32s;  /* type of 32 bit signed quantity */
typedef u_int64_t          Bit64u;  /* type of 64 bit unsigned quantity */
typedef   int64_t          Bit64s;  /* type of 64 bit signed quantity */

/* Temporarily defined to allow dosemu to compile with glibc2 */
#if (GLIBC_VERSION_CODE >= 2000) && !defined(_I386_TYPES_H)
typedef u_int8_t	__u8;
typedef u_int16_t	__u16;
typedef u_int32_t	__u32;
#endif

typedef Bit32u ioport_t;	/* for compatibility */

#if GLIBC_VERSION_CODE >= 2000
#include <stddef.h> /* for ptrdiff_t & friends */
#endif

#endif /* EMU_TYPES_H */
