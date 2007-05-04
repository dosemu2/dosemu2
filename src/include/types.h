/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef EMU_TYPES_H
#define EMU_TYPES_H

#include <stdint.h>
#include <sys/types.h>

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

typedef uint64_t hitimer_t;

typedef union {
  uint64_t td;
  struct { uint32_t tl,th; } t;
} u_int64_u;

typedef union {
  int64_t td;
  struct { int32_t tl,th; } t;
} int64_u;

typedef u_int64_u hitimer_u;

typedef unsigned char      Boolean;
typedef uint8_t            Bit8u;   /* type of 8 bit unsigned quantity */
typedef  int8_t            Bit8s;   /* type of 8 bit signed quantity */
typedef uint16_t           Bit16u;  /* type of 16 bit unsigned quantity */
typedef   int16_t          Bit16s;  /* type of 16 bit signed quantity */
typedef uint32_t           Bit32u;  /* type of 32 bit unsigned quantity */
typedef  int32_t           Bit32s;  /* type of 32 bit signed quantity */
typedef uint64_t           Bit64u;  /* type of 64 bit unsigned quantity */
typedef  int64_t           Bit64s;  /* type of 64 bit signed quantity */

typedef Bit32u ioport_t;	/* for compatibility */

#include <stddef.h> /* for ptrdiff_t & friends */

#endif /* EMU_TYPES_H */
