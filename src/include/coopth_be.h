/*
 * (C) Copyright 2021 by stsp
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef COOPTH_BE_H
#define COOPTH_BE_H

#define COOPTH_POOL_SIZE 3000

#define MAX_COOP_RECUR_DEPTH 5
#if (COOPTH_POOL_SIZE % MAX_COOP_RECUR_DEPTH) != 0
#error COOPTH_POOL_SIZE is bad
#endif
#define MAX_COOPTHREADS (COOPTH_POOL_SIZE / MAX_COOP_RECUR_DEPTH)
#if MAX_COOPTHREADS < 512
#error COOPTH_POOL_SIZE too small
#endif

enum { COOPTH_BE_VM86, COOPTH_BE_PM };

struct coopth_be_ops {
    int (*is_active)(int tid, int idx);
    void (*callf)(int tid, int idx);
    void (*retf)(int tid, int idx);
    void (*prep)(int tid, int idx);
    int (*to_sleep)(int tid);
    void (*sleep)(void);
    uint64_t (*get_dbg_val)(int tid, int idx);
    unsigned id;
};

struct cstart_ret {
    int idx;
    int detached;
};

struct crun_ret {
    int idx;
    int term;
};

int coopth_create_internal(const char *name,
	coopth_func_t func,
	const struct coopth_be_ops *ops);
int coopth_create_multi_internal(const char *name, int len,
	coopth_func_t func,
	const struct coopth_be_ops *ops);
struct cstart_ret coopth_start_internal(int tid, void *arg,
	void (*retf)(int tid, int idx));
int coopth_start_custom_internal(int tid, void *arg);
void coopth_join_internal(int tid, void (*helper)(void));
int coopth_flush_internal(unsigned id, void (*helper)(void));
struct crun_ret coopth_run_thread_internal(int tid);
int coopth_wants_sleep_internal(unsigned id);
void coopth_call_post_internal(int tid);

#endif
