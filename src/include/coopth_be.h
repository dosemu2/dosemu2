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

struct coopth_be_ops {
    int (*is_active)(int tid);
    void (*callf)(int tid, int idx);
    void (*retf)(int tid, int idx);
    int (*to_sleep)(void);
    uint64_t (*get_dbg_val)(int tid, int idx);
};

int coopth_create_internal(const char *name, const struct coopth_be_ops *ops);
int coopth_create_multi_internal(const char *name, int len,
	const struct coopth_be_ops *ops);
int coopth_start_internal(int tid, coopth_func_t func, void *arg,
	void (*callf)(int tid, int idx));
int coopth_flush_internal(void (*helper)(void));

#endif
