/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* def_size carries the size bits DasmI386() takes, plus:
 *   4 - code is at a host address, to be read through the pointer
 *   8 - code is x86-64, so a 0x48 prefix is REX.W and registers are r*
 * Code of a DOS client read out of a buffer of our own is 4 without 8. */
int  dis_8086(uintptr_t, char *, int, unsigned int *, unsigned int);


