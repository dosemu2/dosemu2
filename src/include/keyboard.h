#include "extern.h"
#include "types.h"

EXTERN Bit8u keyb_io_read(Bit32u);
EXTERN void keyb_io_write(Bit32u port, Bit8u byte);

EXTERN void keyb_init(void);
EXTERN void keyb_reset(void);
