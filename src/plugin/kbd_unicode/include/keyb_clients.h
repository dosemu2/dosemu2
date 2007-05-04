/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */


#ifndef _EMU_KEYB_CLNT_H
#define _EMU_KEYB_CLNT_H

#include "config.h"

#include "emu.h"
#include "keyboard.h"
#include "translate.h"

int keyb_client_init(void);
void keyb_client_reset(void);
void keyb_client_close(void);
void keyb_client_run(void);
void keyb_client_set_leds(t_modifiers modifiers);

int paste_text(const char *text, int len);

/* this should really go somewhere else ... */
void handle_slang_keys(Boolean make, t_keysym key);

/* For the current sigio handler, this still has to be defined here. */
EXTERN int kbd_fd INIT(-1);

struct keyboard_client {
  const char *name;
  int    (*probe)(void);
  int    (*init)(void);
  void   (*reset)(void);
  void   (*close)(void);
  void   (*run)(void);         /* check if keys are ready and process them */
  void   (*set_leds)(t_modifiers modifiers);
  void   (*handle_keys)(Boolean make, t_keysym key);
  struct keyboard_client *next;
};

void register_keyboard_client(struct keyboard_client *keyboard);

EXTERN struct keyboard_client *Keyboard INIT(NULL);
extern struct keyboard_client Keyboard_raw;
extern struct keyboard_client Keyboard_slang;
extern struct keyboard_client Keyboard_none;

extern void  dos_slang_redraw(void);
extern void  dos_slang_suspend(void);
extern void  dos_slang_smart_set_mono(void);

#endif	/* _EMU_KEYB_CLNT_H */

