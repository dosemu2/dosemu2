/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This is file plugin_config.h for use within the src/plugin/<name>/
 *
 *
 * It should contain public prototypes for the hook routines
 * (init, close, inte6, ioselect and poll)
 * and config.h - like configuration statements
 *
 */

extern void demo_plugin_init(void);
extern int demo_plugin_inte6(void);
extern void demo_plugin_close(void);
extern int my_plugin_need_poll;
extern void my_plugin_poll(int);
extern int my_plugin_fd;
extern void my_plugin_ioselect(void);
