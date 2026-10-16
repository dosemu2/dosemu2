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


/* in-thread stubs: the half a thread body calls about itself. Kept in a
 * file of its own because this is what moves into libd2core.so. */

#include "coopth_ops.h"

int coopth_yield(void)
{
    return coopth_guest->yield();
}

int coopth_wait(void)
{
    return coopth_guest->wait();
}

int coopth_sleep(void)
{
    return coopth_guest->sleep();
}

int coopth_sched(void)
{
    return coopth_guest->sched();
}

int coopth_sched_cond(void)
{
    return coopth_guest->sched_cond();
}

void coopth_detach(void)
{
    coopth_guest->detach();
}

void coopth_attach(void)
{
    coopth_guest->attach();
}

void coopth_exit(void)
{
    coopth_guest->exit();
}

int coopth_add_post_handler(coopth_func_t func, void *arg)
{
    return coopth_guest->add_post_handler(func, arg);
}

int coopth_set_sleep_handler(coopth_func_t func, void *arg)
{
    return coopth_guest->set_sleep_handler(func, arg);
}

int coopth_set_cleanup_handler(coopth_func_t func, void *arg)
{
    return coopth_guest->set_cleanup_handler(func, arg);
}

void coopth_push_user_data_cur(void *udata)
{
    coopth_guest->push_user_data_cur(udata);
}

void *coopth_pop_user_data_cur(void)
{
    return coopth_guest->pop_user_data_cur();
}

void *coopth_get_user_data_cur(void)
{
    return coopth_guest->get_user_data_cur();
}

void *coopth_get_udata_cur(void)
{
    return coopth_guest->get_udata_cur();
}

void coopth_cancel_disable_cur(void)
{
    coopth_guest->cancel_disable_cur();
}

void coopth_cancel_enable_cur(void)
{
    coopth_guest->cancel_enable_cur();
}
