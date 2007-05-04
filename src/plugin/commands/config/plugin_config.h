/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This is file plugin_config.h for use within the src/plugin/<name>/
 *
 * It should contain public prototypes for the hook routines
 * (init, close, inte6, ioselect and poll)
 * and config.h - like configuration statements
 *
 */

/* Increment this when the interface changes */
#define BUILTINS_PLUGIN_VERSION     2

#define DOS_HELPER_COMMANDS         0xc0
#define DOS_HELPER_COMMANDS_DONE    0xc1

#ifndef __ASSEMBLER__
extern void commands_plugin_init(void);
extern int commands_plugin_inte6(void);
extern int commands_plugin_inte6_done(void);
extern void commands_plugin_close(void);
#endif
