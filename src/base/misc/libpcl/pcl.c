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

#include <stdio.h>
#include <stdlib.h>
#include "pcl_private.h"
#include "pcl_ctx.h"
#include "pcl.h"

static cothread_ctx *co_get_thread_ctx(co_ctx_t *ctx);

static void co_switch_context(co_ctx_t *octx, co_ctx_t *nctx)
{
	cothread_ctx *tctx = co_get_thread_ctx(octx);

	if (octx->swap_context(octx, nctx->cc) < 0) {
		fprintf(stderr, "[PCL] Context switch failed: curr=%p\n",
			tctx->co_curr);
		exit(1);
	}
}


static void co_runner(co_ctx_t *ctx)
{
	cothread_ctx *tctx = co_get_thread_ctx(ctx);
	coroutine *co = tctx->co_curr;

	co->restarget = co->caller;
	co->func(co->data);
	co_exit(tctx);
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
	co->alloc = alloc;
	co->func = func;
	co->data = data;
	co->exited = 0;
	co->ctx.cc = co->stk;

	return co;
}

coroutine_t co_create(void (*func)(void *), void *data, void *stack, int size)
{
	coroutine *co;
	int cs = ctx_sizeof();

	co = do_co_create(func, data, stack, size, cs);
	if (!co)
		return NULL;
	if (ctx_create_context(&co->ctx, co_runner, &co->ctx, co->stack, size - CO_STK_COROSIZE(cs)) < 0) {
		if (co->alloc)
			free(co);
		return NULL;
	}

	return (coroutine_t) co;
}

coroutine_t m_co_create(void (*func)(void *), void *data, void *stack, int size)
{
	coroutine *co;
	int cs = mctx_sizeof();

	co = do_co_create(func, data, stack, size, cs);
	if (!co)
		return NULL;
	if (mctx_create_context(&co->ctx, co_runner, &co->ctx, co->stack, size - CO_STK_COROSIZE(cs)) < 0) {
		if (co->alloc)
			free(co);
		return NULL;
	}

	return (coroutine_t) co;
}

void co_delete(coroutine_t coro)
{
	coroutine *co = (coroutine *) coro;
	cothread_ctx *tctx = co_get_thread_ctx(&co->ctx);

	if (co == tctx->co_curr) {
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
	cothread_ctx *tctx = co_get_thread_ctx(&co->ctx);
	coroutine *oldco = tctx->co_curr;

	co->caller = tctx->co_curr;
	tctx->co_curr = co;

	co_switch_context(&oldco->ctx, &co->ctx);

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
	coroutine *newco = tctx->co_curr->restarget, *co = tctx->co_curr;

	co->exited = 1;
	tctx->co_curr = newco;

	co_switch_context(&co->ctx, &newco->ctx);
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

cohandle_t co_thread_init(void)
{
	cothread_ctx *tctx = ctx_get_global_ctx();

	tctx->co_main.ctx.cc = tctx->stk0;
	ctx_init(&tctx->co_main.ctx);
	return tctx;
}

cohandle_t mco_thread_init(void)
{
	cothread_ctx *tctx = mctx_get_global_ctx();

	tctx->co_main.ctx.cc = tctx->stk0;
	mctx_init(&tctx->co_main.ctx);
	return tctx;
}

void co_thread_cleanup(void)
{
}

static cothread_ctx *co_get_thread_ctx(co_ctx_t *ctx)
{
	return ctx->get_global_ctx();
}
