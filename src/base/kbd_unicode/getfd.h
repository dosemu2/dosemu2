/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */
#ifdef __linux__
#include "keyboard/keymaps.h"

extern int open_console(void);
extern int getfd(void);
int read_kbd_table(struct keytable_entry *kt,
			  struct keytable_entry *altkt);
#else
static inline int open_console(void) { return -1; }
static inline int getfd(void) { return -1; }
static inline int read_kbd_table(struct keytable_entry *kt,
			  struct keytable_entry *altkt) { return -1; }
#endif
