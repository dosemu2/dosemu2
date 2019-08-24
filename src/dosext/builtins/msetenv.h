/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

int msetenv(const char *var, const char *value);
char *mgetenv(const char *var);
/* TODO: remove below funcs when comcom32 supports ae01 */
int msetenv_child(const char *var, const char *value);
int mresize_env(int size_plus);
