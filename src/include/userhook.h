/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

extern int uhook_fdin;
extern void uhook_input(void);
extern void uhook_poll(void);
extern void init_uhook(char *pipes);
extern void close_uhook(void);

