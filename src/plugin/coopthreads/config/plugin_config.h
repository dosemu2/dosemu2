/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This is file plugin_config.h for use within the src/plugin/<name>/
 *
 * It should contain public prototypes for the hook routines
 * (init, close, inte6, ioselect and poll)
 * and config.h - like configuration statements
 *
 */

#define DOS_HELPER_COOP		0xc0

extern void coopthreads_plugin_init(void);
extern int coopthreads_plugin_inte6(void);
extern void coopthreads_plugin_close(void);
