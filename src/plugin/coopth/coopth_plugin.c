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


/* coopth as a plugin: the tables that let the rest of dosemu2 call it
 * without linking against it. */

#include "init.h"
#include "coopth_impl.h"

static const struct coopth_host_api host_api = {
    .init = ct_init,
    .is_threaded = ct_is_threaded,
    .set_permanent_post_handler = ct_set_permanent_post_handler,
    .set_ctx_handlers = ct_set_ctx_handlers,
    .set_sleep_handlers = ct_set_sleep_handlers,
    .set_thread_class = ct_set_thread_class,
    .set_detached = ct_set_detached,
    .unsafe_detach = ct_unsafe_detach,
    .unsafe_shutdown = ct_unsafe_shutdown,
    .push_user_data = ct_push_user_data,
    .pop_user_data = ct_pop_user_data,
    .set_udata = ct_set_udata,
    .ensure_sleeping = ct_ensure_sleeping,
    .ensure_single = ct_ensure_single,
    .attach_to_cur = ct_attach_to_cur,
    .wake_up = ct_wake_up,
    .cancel = ct_cancel,
    .done = ct_done,
    .run = ct_run,
    .run_tid = ct_run_tid,
    .dump = ct_dump,
    .set_nothread_notifier = ct_set_nothread_notifier,
    .get_thread_count = ct_get_thread_count,
};

static const struct coopth_guest_api guest_api = {
    .yield = ct_yield,
    .wait = ct_wait,
    .sleep = ct_sleep,
    .sched = ct_sched,
    .sched_cond = ct_sched_cond,
    .detach = ct_detach,
    .attach = ct_attach,
    .exit = ct_exit,
    .add_post_handler = ct_add_post_handler,
    .set_sleep_handler = ct_set_sleep_handler,
    .set_cleanup_handler = ct_set_cleanup_handler,
    .push_user_data_cur = ct_push_user_data_cur,
    .pop_user_data_cur = ct_pop_user_data_cur,
    .get_user_data_cur = ct_get_user_data_cur,
    .get_udata_cur = ct_get_udata_cur,
    .cancel_disable_cur = ct_cancel_disable_cur,
    .cancel_enable_cur = ct_cancel_enable_cur,
};

static const struct coopth_common_api common_api = {
    .get_tid = ct_get_tid,
    .leave = ct_leave,
    .abandon = ct_abandon,
};

static const struct coopth_int_api int_api = {
    .create_internal = ct_create_internal,
    .create_multi_internal = ct_create_multi_internal,
    .start_internal = ct_start_internal,
    .start_custom_internal = ct_start_custom_internal,
    .join_internal = ct_join_internal,
    .flush_internal = ct_flush_internal,
    .run_thread_internal = ct_run_thread_internal,
    .wants_sleep_internal = ct_wants_sleep_internal,
    .call_post_internal = ct_call_post_internal,
    .leave_internal = ct_leave_internal,
};

CONSTRUCTOR(static void coopth_plugin_init(void))
{
    coopth_register_api(&host_api, &guest_api, &common_api, &int_api);
}
