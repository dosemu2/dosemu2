#include <fdpp/thunks.h>
#include "emu.h"
#include "init.h"

static struct dl_ops ops = {
    .ccall = FdThunkCall,
};

CONSTRUCTOR(static void init(void))
{
    register_dl_ops(&ops);
}
