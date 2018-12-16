/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

int msetenv(const char *var, const char *value);
int msetenv_child(const char *var, const char *value);
int mresize_env(int size_plus);
char *mgetenv(const char *var);
char *mgetenv_child(const char *var);
