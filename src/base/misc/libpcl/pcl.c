/* This code is taken from libpcl library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/*
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi
 *
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#define WANT_ASAN 1
#ifdef __clang__
#define HAS_FEATURE(x) __has_feature(x)
#else
#define HAS_FEATURE(x) 0
#endif
#if (defined(__SANITIZE_ADDRESS__) || HAS_FEATURE(address_sanitizer)) && WANT_ASAN
#define USE_ASAN 1
#else
#define USE_ASAN 0
#endif
#if USE_ASAN
#include <sanitizer/asan_interface.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "pcl.h"
#include "pcl_private.h"
#include "pcl_ctx.h"

static cothread_ctx *co_get_thread_ctx(coroutine *co);

static void co_switch_context(co_base *octx, co_base *nctx)
{
#if USE_ASAN
	void *fake_stack_save = NULL;
	__sanitizer_start_switch_fiber(octx->exited ? NULL : &fake_stack_save,
	               nctx->stack, nctx->stack_size);
	nctx->tmp = fake_stack_save;
#endif
	if (octx->ctx.ops->swap_context(&octx->ctx, nctx->ctx.cc) < 0) {
		fprintf(stderr, "[PCL] Context switch failed\n");
		exit(1);
	}
#if USE_ASAN
	__sanitizer_finish_switch_fiber(fake_stack_save, NULL, NULL);
#endif
}


static void co_runner(coroutine *co)
{
	cothread_ctx *tctx;

#if USE_ASAN
	__sanitizer_finish_switch_fiber(co->tmp, NULL, NULL);
#endif
	tctx = co_get_thread_ctx(co);
	co->restarget = co->caller;
	co->func(co->data);
	co_exit(tctx);
	abort();
}

static coroutine *do_co_create(void (*func)(void *), void *data, void *stack,
		int size, int corosize)
{
	int alloc = 0;
	coroutine *co;

	if ((size &= ~(sizeof(long) - 1)) < CO_MIN_SIZE)
		return NULL;
	if (stack == NULL) {
		size = CO_STK_COROSIZE(size + corosize);
		stack = malloc(size);
		if (stack == NULL)
			return NULL;
		alloc = size;
	}
	co = stack;
	co->stack = (char *) stack + CO_STK_COROSIZE(corosize);
	co->stack_size = size;
	co->alloc = alloc;
	co->func = func;
	co->data = data;
	co->exited = 0;

	return co;
}

coroutine_t co_create(cohandle_t handle, void (*func)(void *), void *data,
		void *stack, int size)
{
	coroutine *co;
	cothread_ctx *tctx = (cothread_ctx *)handle;

	co = do_co_create(func, data, stack, size, tctx->ctx_sizeof);
	if (!co)
		return NULL;
	co->ctx = tctx->co_main.ctx;
	co->ctx.cc = co->stk;
	co->ctx_main = tctx;
	if (co->ctx.ops->create_context(&co->ctx, co_runner, co, co->stack,
			size - CO_STK_COROSIZE(tctx->ctx_sizeof)) < 0) {
		if (co->alloc)
			free(co);
		return NULL;
	}

	return (coroutine_t) co;
}

void co_delete(coroutine_t coro)
{
	coroutine *co = (coroutine *) coro;
	cothread_ctx *tctx = co_get_thread_ctx(co);

	if ((co_base *)co == tctx->co_curr) {
		fprintf(stderr, "[PCL] Cannot delete itself: curr=%p\n",
			tctx->co_curr);
		exit(1);
	}
	if (co->alloc)
		free(co);
}

void co_call(coroutine_t coro)
{
	coroutine *co = (coroutine *) coro;
	cothread_ctx *tctx = co_get_thread_ctx(co);
	co_base *oldco = tctx->co_curr;

	co->caller = tctx->co_curr;
	tctx->co_curr = co;

	co_switch_context(oldco, co);

	if (co->exited)
		co_delete(co);
}

void co_resume(cohandle_t handle)
{
	cothread_ctx *tctx = (cothread_ctx *)handle;

	co_call(tctx->co_curr->restarget);
	tctx->co_curr->restarget = tctx->co_curr->caller;
}

void co_exit(cohandle_t handle)
{
	cothread_ctx *tctx = (cothread_ctx *)handle;
	co_base *newco = tctx->co_curr->restarget, *co = tctx->co_curr;

	co->exited = 1;
	tctx->co_curr = newco;

	co_switch_context(co, newco);
}

coroutine_t co_current(cohandle_t handle)
{
	cothread_ctx *tctx = (cothread_ctx *)handle;

	return (coroutine_t) tctx->co_curr;
}

void *co_get_data(coroutine_t coro)
{
	coroutine *co = (coroutine *) coro;

	return co->data;
}

void *co_set_data(coroutine_t coro, void *data)
{
	coroutine *co = (coroutine *) coro;
	void *odata;

	odata = co->data;
	co->data = data;

	return odata;
}

/*
 * Simple case, the single thread one ...
 */

static void do_co_init(cothread_ctx *tctx)
{
	tctx->co_main.ctx.cc = tctx->stk0;
	tctx->co_main.ctx_main = tctx;
	tctx->co_main.exited = 0;
	tctx->co_curr = &tctx->co_main;
}

cohandle_t co_thread_init(enum CoBackend b)
{
	int sz = ctx_sizeof(b);
	cothread_ctx *tctx = malloc(sizeof(cothread_ctx) + CO_STK_ALIGN(sz));

	do_co_init(tctx);
	ctx_init(b, &tctx->co_main.ctx.ops);
	tctx->ctx_sizeof = sz;
	return tctx;
}

void co_thread_cleanup(cohandle_t handle)
{
	cothread_ctx *tctx = (cothread_ctx *)handle;

	free(tctx);
}

static cothread_ctx *co_get_thread_ctx(coroutine *co)
{
	return co->ctx_main;
}
