/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _EMU_KEYB_H
#define _EMU_KEYB_H

#include "config.h"

#define PAUSE_MASK      0x0008

/* this file is included from base/bios/bios.S */
#ifndef __ASM__

#include "keyboard.h"

/* definitions for keyb_8042.c  (8042 keyboard controller emulation)
 * these are needed in some other modules, e.g. ports.c
 */

Bit8u keyb_io_read(ioport_t port);
void keyb_io_write(ioport_t port, Bit8u value);
void keyb_8042_init(void);
void keyb_8042_reset(void);

/* definitions for serv_backend.c */

extern Boolean port60_ready;
extern Bit8u port60_buffer;
extern Boolean int9_running;

void output_byte_8042(Bit8u value);
void int_check_queue(void);
void copy_shift_state(t_shiftstate shift);

Bit16u get_bios_key(t_rawkeycode raw);

struct keyboard_queue {
	int head;
	int tail;
	int size;
	t_rawkeycode *queue;
};
extern struct keyboard_queue keyb_queue;

int queue_level(struct keyboard_queue *q);
void clear_queue(struct keyboard_queue *q);
void write_queue(struct keyboard_queue *q, t_rawkeycode raw);
t_rawkeycode read_queue(struct keyboard_queue *q);

void clear_bios_keybuf(void);

int keyb_server_init(void);
int keyb_server_reset(void);
void keyb_server_close(void);
void keyb_server_run(void);

void backend_run(void);
void backend_reset(void);

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
