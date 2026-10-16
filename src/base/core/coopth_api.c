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


/* host-side stubs: they forward to the coopth plugin through the tables
 * it registers. This file stays in libdosemu2.so when the core is split
 * out into libd2core.so. */

#include <assert.h>
#include "coopth_ops.h"

const struct coopth_host_api *coopth_host;
const struct coopth_guest_api *coopth_guest;
const struct coopth_common_api *coopth_common;
const struct coopth_int_api *coopth_int;

void coopth_register_api(const struct coopth_host_api *host,
	const struct coopth_guest_api *guest,
	const struct coopth_common_api *common,
	const struct coopth_int_api *intr)
{
    assert(!coopth_host);
    coopth_host = host;
    coopth_guest = guest;
    coopth_common = common;
    coopth_int = intr;
}

void coopth_init(void)
{
    coopth_host->init();
}

int coopth_is_threaded(void)
{
    return coopth_host->is_threaded();
}

int coopth_set_permanent_post_handler(int tid, coopth_hndl_t func)
{
    return coopth_host->set_permanent_post_handler(tid, func);
}

int coopth_set_ctx_handlers(int tid, coopth_hndl_t pre, coopth_hndl_t post, void *arg)
{
    return coopth_host->set_ctx_handlers(tid, pre, post, arg);
}

int coopth_set_sleep_handlers(int tid, coopth_sleep_hndl_t pre, coopth_hndl_t post)
{
    return coopth_host->set_sleep_handlers(tid, pre, post);
}

int coopth_set_thread_class(int tid, int cls)
{
    return coopth_host->set_thread_class(tid, cls);
}

int coopth_set_detached(int tid)
{
    return coopth_host->set_detached(tid);
}

int coopth_unsafe_detach(int tid, const char *who)
{
    return coopth_host->unsafe_detach(tid, who);
}

void coopth_unsafe_shutdown(void)
{
    coopth_host->unsafe_shutdown();
}

void coopth_push_user_data(int tid, void *udata)
{
    coopth_host->push_user_data(tid, udata);
}

void *coopth_pop_user_data(int tid)
{
    return coopth_host->pop_user_data(tid);
}

void coopth_set_udata(int tid, void *udata)
{
    coopth_host->set_udata(tid, udata);
}

void coopth_ensure_sleeping(int tid)
{
    coopth_host->ensure_sleeping(tid);
}

void coopth_ensure_single(int tid)
{
    coopth_host->ensure_single(tid);
}

void coopth_attach_to_cur(int tid)
{
    coopth_host->attach_to_cur(tid);
}

void coopth_wake_up(int tid)
{
    coopth_host->wake_up(tid);
}

void coopth_cancel(int tid)
{
    coopth_host->cancel(tid);
}

void coopth_done(void)
{
    coopth_host->done();
}

void coopth_run(void)
{
    coopth_host->run();
}

void coopth_run_tid(int tid)
{
    coopth_host->run_tid(tid);
}

void coopth_dump(int all)
{
    coopth_host->dump(all);
}

void coopth_set_nothread_notifier(void (*notifier)(int))
{
    coopth_host->set_nothread_notifier(notifier);
}

int coopth_get_thread_count(int cls)
{
    return coopth_host->get_thread_count(cls);
}

int coopth_get_tid(void)
{
    return coopth_common->get_tid();
}

void coopth_leave(void)
{
    coopth_common->leave();
}

void coopth_abandon(void)
{
    coopth_common->abandon();
}

int coopth_create_internal(const char *name, coopth_func_t func, const struct coopth_be_ops *ops)
{
    return coopth_int->create_internal(name, func, ops);
}

int coopth_create_multi_internal(const char *name, int len, coopth_func_t func, const struct coopth_be_ops *ops)
{
    return coopth_int->create_multi_internal(name, len, func, ops);
}

struct cstart_ret coopth_start_internal(int tid, void *arg, void (*retf)(int tid, int idx))
{
    return coopth_int->start_internal(tid, arg, retf);
}

int coopth_start_custom_internal(int tid, void *arg)
{
    return coopth_int->start_custom_internal(tid, arg);
}

void coopth_join_internal(int tid, void (*helper)(void))
{
    coopth_int->join_internal(tid, helper);
}

int coopth_flush_internal(unsigned id, void (*helper)(void))
{
    return coopth_int->flush_internal(id, helper);
}

struct crun_ret coopth_run_thread_internal(int tid)
{
    return coopth_int->run_thread_internal(tid);
}

int coopth_wants_sleep_internal(unsigned id)
{
    return coopth_int->wants_sleep_internal(id);
}

void coopth_call_post_internal(int tid)
{
    coopth_int->call_post_internal(tid);
}

void coopth_leave_internal(void)
{
    coopth_int->leave_internal();
}
