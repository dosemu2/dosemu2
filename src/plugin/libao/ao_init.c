/*
 * global initializers for libao.
 * Note: libao is so lame to require this.
 */

#include <ao/ao.h>
#include "init.h"

CONSTRUCTOR(static void ao_init(void))
{
  ao_initialize();
}

DESTRUCTOR(static void ao_done(void))
{
  ao_shutdown();
}
