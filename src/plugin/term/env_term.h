/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

extern const char *DOSemu_Keyboard_Keymap_Prompt;
extern int DOSemu_Terminal_Scroll;
extern int DOSemu_Slang_Show_Help;
int term_init(void);
void term_close(void);
int using_xterm(void);
void xtermmouse_get_event (Bit8u **kbp, int *kb_count);
