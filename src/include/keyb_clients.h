/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


#ifndef _EMU_KEYB_CLNT_H
#define _EMU_KEYB_CLNT_H

#include "emu.h"
#include "keyboard.h"


/* ASCII 0x20..0x7E */
extern const Bit8u ascii_keys[];
extern const u_char latin_to_dos[];
extern const u_char latin1_to_dos[];
extern const u_char latin2_to_dos[];

int keyb_client_init();
void keyb_client_close();
void keyb_client_run();

int paste_text(const char *text, int len);

/* For the current sigio handler, this still has to be defined here. */
EXTERN int kbd_fd INIT(-1);

struct keyboard_client {
  const char *name;
  int    (*init)(void);
  void   (*reset)(void);
  void   (*close)(void);
  void   (*run)(void);         /* check if keys are ready and process them */
};

EXTERN struct keyboard_client *Keyboard INIT(NULL);
extern struct keyboard_client Keyboard_raw;
extern struct keyboard_client Keyboard_slang;
extern struct keyboard_client Keyboard_X;

#if 0
extern int raw_keyboard_init(void);
extern void raw_keyboard_close(void);
extern void do_raw_getkeys(void);

extern int slang_keyb_init(void);
extern void slang_keyb_close(void);
extern void do_slang_getkeys(void);
#endif

extern void  dos_slang_redraw(void);
extern void  dos_slang_suspend(void);
extern void  dos_slang_smart_set_mono(void);

#endif	/* _EMU_KEYB_CLNT_H */

