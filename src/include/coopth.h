/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef COOPTH_H
#define COOPTH_H

#define COOPTH_TID_INVALID (-1)

typedef void (*coopth_func_t)(void *arg);

void coopth_init(void);
int coopth_create(char *name);
int coopth_create_multi(char *name, int len);
int coopth_start(int tid, coopth_func_t func, void *arg);
int coopth_set_permanent_post_handler(int tid, coopth_func_t func, void *arg);
int coopth_set_post_handler(coopth_func_t func, void *arg);
int coopth_set_ctx_handlers(int tid, coopth_func_t pre, void *arg_pre,
	coopth_func_t post, void *arg_post);
int coopth_set_sleep_handlers(int tid, coopth_func_t pre, void *arg_pre,
	coopth_func_t post, void *arg_post);
void coopth_join(int tid, void (*helper)(void));
int coopth_flush(void (*helper)(void));
int coopth_set_detached(int tid);
int coopth_set_sleep_handler(coopth_func_t func, void *arg);
int coopth_set_cleanup_handler(coopth_func_t func, void *arg);
void coopth_set_user_data(void *udata);
void *coopth_get_user_data(int tid);
int coopth_get_tid(void);
void coopth_yield(void);
void coopth_wait(void);
void coopth_sleep(void);
void coopth_leave(void);
void coopth_attach(void);
void coopth_wake_up(int tid);
void coopth_asleep(int tid);
void coopth_cancel(int tid);
void coopth_done(void);
void coopth_run(void);

int coopth_tag_alloc(void);
void coopth_tag_set(int tag, int cookie);
void coopth_tag_clear(int tag, int cookie);
void coopth_sleep_tagged(int tag, int cookie);
int coopth_get_tid_by_tag(int tag, int cookie);

#endif
