#include "extern.h"
#include "types.h"

EXTERN u_char keyb_io_read(u_int);
EXTERN void keyb_io_write(u_int port, u_char byte);

EXTERN void keyb_init(void);
EXTERN void keyb_reset(void);
