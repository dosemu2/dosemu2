#ifndef EMU_TYPES_H
#define EMU_TYPES_H

/* will include bitypes.h, linux/posix_types.h and asm/types.h */
#include <sys/types.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

typedef u_int64_t hitimer_t;

typedef union {
  hitimer_t td;
  struct { u_int32_t tl,th; } t;
} hitimer_u;

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
#if defined(__GLIBC__) && __GLIBC__ >= 2
typedef u_int8_t	__u8;
typedef u_int16_t	__u16;
typedef u_int32_t	__u32;
#endif

typedef Bit32u ioport_t;	/* for compatibility */

#if __GLIBC__ >= 2
#include <stddef.h> /* for ptrdiff_t & friends */
#endif

#endif /* EMU_TYPES_H */
