#ifndef _EMU_KEYB_H
#define _EMU_KEYB_H

#ifndef NEW_KBD_CODE
#error keyb_server.h is for new keyboard code only !
#endif


/* this file is included from base/bios/bios.S */
#ifndef __ASM__

#include "keyboard.h"


/* definitions for keyb_8042.c  (8042 keyboard controller emulation)
 * these are needed in some other modules, e.g. ports.c
 */

Bit8u keyb_io_read(Bit32u port);
void keyb_io_write(Bit32u port, Bit8u value);
void keyb_8042_init(void);
void keyb_8042_reset(void);

/* definitions for serv_backend.c */

extern Bit16u bios_buffer;
extern t_shiftstate shiftstate_buffer;

extern Boolean port60_ready;
extern Bit8u port60_buffer;
extern Boolean int9_running;

void output_byte_8042(Bit8u value);
void int_check_queue();
void copy_shift_state(Bit16u shift);

static inline Bit16u get_bios_key(void) {
   Bit16u b;
   b=bios_buffer;
   bios_buffer=0;
   port60_ready=0;
   copy_shift_state(shiftstate_buffer);
   return b;
}


int keyb_queuelevel();
void clear_queue();
void write_queue(Bit16u bios_key, t_shiftstate shift, Bit32u raw);
void read_queue(Bit16u *bios_key, t_shiftstate *shift, t_rawkeycode *raw);

void backend_run();
void backend_reset();

#endif  /* not __ASM__ */

/* physical scancodes deviating from keysyms */
#define _SCN_PRTSCR		0xe037 
#define _SCN_PAUSE_MAKE		0xe11d45
#define _SCN_PAUSE_BREAK	0xe19dc5

#define IS_MAKE(scancode)       !((scancode)&0x80)

#define SP_PAUSE	0xff01
#define SP_BREAK	0xff02
#define SP_PRTSCR	0xff03
#define SP_SYSRQ_MAKE	0xff04
#define SP_SYSRQ_BREAK	0xff05

#define IS_SPECIAL(s) (((s)&0xff00) == 0xff00)


#endif /* _EMU_KEYB_H */
