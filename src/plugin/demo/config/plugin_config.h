/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
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

#ifndef __ASSEMBLER__
void sndpipe_plugin_init(void);
void sndpipe_plugin_close(void);
#endif
