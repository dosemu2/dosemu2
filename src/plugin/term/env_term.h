/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifdef USE_RELAYTOOL
extern int SLang_Error;
extern int SLtt_Use_Blink_For_ACS;
extern int libslang_symbol_is_present(char *);
#endif

extern const char *DOSemu_Keyboard_Keymap_Prompt;
extern int DOSemu_Terminal_Scroll;
extern int DOSemu_Slang_Show_Help;
extern struct mouse_client Mouse_xterm;
int term_init(void);
void term_close(void);
int using_xterm(void);
int xtermmouse_get_event (Bit8u *kbp, int kb_count);
