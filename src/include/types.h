#ifndef _EMU_TYPES_H
#define _EMU_TYPES_H

#include <sys/types.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
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

#endif /* _EMU_TYPES_H */
