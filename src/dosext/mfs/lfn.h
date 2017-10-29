/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

void make_unmake_dos_mangled_path(char *dest, const char *fpath,
                                         int current_drive, int alias);
void close_dirhandles(unsigned psp);
