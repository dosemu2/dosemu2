/*
 * global initializers for libao.
 */

#include <ao/ao.h>
#include "emu.h"
#include "ao_init.h"

static int initialized;

static void ao_done(void)
{
  ao_shutdown();
}

void ao_init(void)
{
  if (initialized)
    return;
  initialized = 1;
  ao_initialize();
  register_exit_handler(ao_done);
}
