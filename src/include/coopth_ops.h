/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/* the coopth API as callback tables. coopth itself lives in the coopth
 * plugin and registers these; everything else reaches it through the
 * stubs in coopth_api.c and coopth_guest_api.c, so that the two halves
 * can end up in different solibs (and later in different dlmopen
 * namespaces) without direct linkage. */

#ifndef COOPTH_OPS_H
#define COOPTH_OPS_H

#include "coopth.h"
#include "coopth_guest.h"
#include "coopth_be.h"


/* the host half: coopth.h, less the nine entry points the vm86 backend
 * implements itself in the core (coopth_create, coopth_create_multi,
 * coopth_create_vm86, coopth_start, coopth_join_vm86, coopth_flush_vm86,
 * coopth_wants_sleep_vm86, coopth_set_ctx_checker_vm86 and
 * coopth_leave_vm86): those reach coopth through the internal API below,
 * so they need no table entry. */
struct coopth_host_api {
    void (*init)(void);
    int (*is_threaded)(void);
    int (*set_permanent_post_handler)(int tid, coopth_hndl_t func);
    int (*set_ctx_handlers)(int tid, coopth_hndl_t pre, coopth_hndl_t post, void *arg);
    int (*set_sleep_handlers)(int tid, coopth_sleep_hndl_t pre, coopth_hndl_t post);
    int (*set_thread_class)(int tid, int cls);
    int (*set_detached)(int tid);
    int (*unsafe_detach)(int tid, const char *who);
    void (*unsafe_shutdown)(void);
    void (*push_user_data)(int tid, void *udata);
    void * (*pop_user_data)(int tid);
    void (*set_udata)(int tid, void *udata);
    void (*ensure_sleeping)(int tid);
    void (*ensure_single)(int tid);
    void (*attach_to_cur)(int tid);
    void (*wake_up)(int tid);
    void (*cancel)(int tid);
    void (*done)(void);
    void (*run)(void);
    void (*run_tid)(int tid);
    void (*dump)(int all);
    void (*set_nothread_notifier)(void (*notifier)(int));
    int (*get_thread_count)(int cls);
};

/* the in-thread half: coopth_guest.h */
struct coopth_guest_api {
    int (*yield)(void);
    int (*wait)(void);
    int (*sleep)(void);
    int (*sched)(void);
    int (*sched_cond)(void);
    void (*detach)(void);
    void (*attach)(void);
    void (*exit)(void);
    int (*add_post_handler)(coopth_func_t func, void *arg);
    int (*set_sleep_handler)(coopth_func_t func, void *arg);
    int (*set_cleanup_handler)(coopth_func_t func, void *arg);
    void (*push_user_data_cur)(void *udata);
    void * (*pop_user_data_cur)(void);
    void * (*get_user_data_cur)(void);
    void * (*get_udata_cur)(void);
    void (*cancel_disable_cur)(void);
    void (*cancel_enable_cur)(void);
};

/* callable from both sides: coopth_common.h */
struct coopth_common_api {
    int (*get_tid)(void);
    void (*leave)(void);
    void (*abandon)(void);
};

/* what the vm86 and PM backends call: coopth_be.h */
struct coopth_int_api {
    int (*create_internal)(const char *name, coopth_func_t func, const struct coopth_be_ops *ops);
    int (*create_multi_internal)(const char *name, int len, coopth_func_t func, const struct coopth_be_ops *ops);
    struct cstart_ret (*start_internal)(int tid, void *arg, void (*retf)(int tid, int idx));
    int (*start_custom_internal)(int tid, void *arg);
    void (*join_internal)(int tid, void (*helper)(void));
    int (*flush_internal)(unsigned id, void (*helper)(void));
    struct crun_ret (*run_thread_internal)(int tid);
    int (*wants_sleep_internal)(unsigned id);
    void (*call_post_internal)(int tid);
    void (*leave_internal)(void);
};

extern const struct coopth_host_api *coopth_host;
extern const struct coopth_guest_api *coopth_guest;
extern const struct coopth_common_api *coopth_common;
extern const struct coopth_int_api *coopth_int;

void coopth_register_api(const struct coopth_host_api *host,
	const struct coopth_guest_api *guest,
	const struct coopth_common_api *common,
	const struct coopth_int_api *intr);

#endif
