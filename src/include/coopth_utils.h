/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef COOPTH_UTILS_H
#define COOPTH_UTILS_H

int coopth_tag_alloc(void);
void coopth_sleep_tagged(int tag, int cookie);
void coopth_yield_tagged(int tag, int cookie);
int coopth_get_tid_by_tag(int tag, int cookie);

#endif
