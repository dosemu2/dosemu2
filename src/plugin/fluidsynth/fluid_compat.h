#ifndef FLUID_COMPAT_H
#define FLUID_COMPAT_H

#include <stdlib.h>

#define TRUE 1
#define FALSE 0

/**
 * Value that indicates success, used by most libfluidsynth functions.
 * @since 1.1.0
 *
 * NOTE: This was not publicly defined prior to libfluidsynth 1.1.0.  When
 * writing code which should also be compatible with older versions, something
 * like the following can be used:
 *
 * @code
 *   #include <fluidsynth.h>
 *
 *   #ifndef FLUID_OK
 *   #define FLUID_OK      (0)
 *   #define FLUID_FAILED  (-1)
 *   #endif
 * @endcode
 */
#define FLUID_OK        (0)

/**
 * Value that indicates failure, used by most libfluidsynth functions.
 * @since 1.1.0
 *
 * NOTE: See #FLUID_OK for more details.
 */
#define FLUID_FAILED    (-1)

#define FLUID_NEW(_t)                (_t*)malloc(sizeof(_t))
#define FLUID_ARRAY(_t,_n)           (_t*)malloc((_n)*sizeof(_t))
#define FLUID_FREE(_p)               free(_p)
#define FLUID_LOG(...)

#endif
