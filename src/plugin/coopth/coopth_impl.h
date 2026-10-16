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


/* coopth's own entry points, behind the tables it registers with the
 * core. The names the rest of dosemu2 uses are the stubs in
 * coopth_api.c and coopth_guest_api.c, so these cannot carry them.
 * The nine vm86-flavoured host entry points (coopth_create*,
 * coopth_start and the *_vm86 ones) are not here: the vm86 backend
 * implements them in the core, on top of the internal API below. */

#ifndef COOPTH_IMPL_H
#define COOPTH_IMPL_H

#include "coopth_ops.h"

/* host */
void ct_init(void);
int ct_is_threaded(void);
int ct_set_permanent_post_handler(int tid, coopth_hndl_t func);
int ct_set_ctx_handlers(int tid, coopth_hndl_t pre, coopth_hndl_t post, void *arg);
int ct_set_sleep_handlers(int tid, coopth_sleep_hndl_t pre, coopth_hndl_t post);
int ct_set_thread_class(int tid, int cls);
int ct_set_detached(int tid);
int ct_unsafe_detach(int tid, const char *who);
void ct_unsafe_shutdown(void);
void ct_push_user_data(int tid, void *udata);
void *ct_pop_user_data(int tid);
void ct_set_udata(int tid, void *udata);
void ct_ensure_sleeping(int tid);
void ct_ensure_single(int tid);
void ct_attach_to_cur(int tid);
void ct_wake_up(int tid);
void ct_cancel(int tid);
void ct_done(void);
void ct_run(void);
void ct_run_tid(int tid);
void ct_dump(int all);
void ct_set_nothread_notifier(void (*notifier)(int));
int ct_get_thread_count(int cls);

/* guest */
int ct_yield(void);
int ct_wait(void);
int ct_sleep(void);
int ct_sched(void);
int ct_sched_cond(void);
void ct_detach(void);
void ct_attach(void);
void ct_exit(void);
int ct_add_post_handler(coopth_func_t func, void *arg);
int ct_set_sleep_handler(coopth_func_t func, void *arg);
int ct_set_cleanup_handler(coopth_func_t func, void *arg);
void ct_push_user_data_cur(void *udata);
void *ct_pop_user_data_cur(void);
void *ct_get_user_data_cur(void);
void *ct_get_udata_cur(void);
void ct_cancel_disable_cur(void);
void ct_cancel_enable_cur(void);

/* common */
int ct_get_tid(void);
void ct_leave(void);
void ct_abandon(void);

/* be */
int ct_create_internal(const char *name, coopth_func_t func, const struct coopth_be_ops *ops);
int ct_create_multi_internal(const char *name, int len, coopth_func_t func, const struct coopth_be_ops *ops);
struct cstart_ret ct_start_internal(int tid, void *arg, void (*retf)(int tid, int idx));
int ct_start_custom_internal(int tid, void *arg);
void ct_join_internal(int tid, void (*helper)(void));
int ct_flush_internal(unsigned id, void (*helper)(void));
struct crun_ret ct_run_thread_internal(int tid);
int ct_wants_sleep_internal(unsigned id);
void ct_call_post_internal(int tid);
void ct_leave_internal(void);

#endif
