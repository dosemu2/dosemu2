/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

int msetenv(char *var, char *value);
int msetenv_child(char *var, char *value);
int mresize_env(int size_plus);
