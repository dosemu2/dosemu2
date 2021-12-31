/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "keyboard/keymaps.h"

extern int open_console(void);
extern int getfd(void);
int read_kbd_table(struct keytable_entry *kt,
			  struct keytable_entry *altkt);
