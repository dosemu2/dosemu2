/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef COOPTH_H
#define COOPTH_H

typedef void (*coopth_func_t)(void *arg);

void coopth_init(void);
int coopth_create(char *name);
int coopth_create_multi(char *name, int len);
int coopth_start(int tid, coopth_func_t func, void *arg);
int coopth_set_post_handler(int tid, coopth_func_t func, void *arg);
int coopth_is_in_thread(void);
void coopth_wait(void);
void coopth_sleep(int *r_tid);
void coopth_wake_up(int tid);
void coopth_done(void);

#endif
