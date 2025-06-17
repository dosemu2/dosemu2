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

/*
 * Purpose: context handling backends for libpcl
 *
 * Author: Stas Sergeev
 */

#include <ucontext.h>
#include <assert.h>
#include "misc/pcontext.h"
#include "sig.h"
#ifdef MCONTEXT
#include "mcontext/mcontext.h"
#endif
#include "pcl.h"
#include "pcl_private.h"
#include "pcl_ctx.h"

static void ctx_init_context_dummy(void *ctx)
{
}

static void ctx_free_context_dummy(void *ctx)
{
}

#if WANT_UCONTEXT
static int ctx_swap_context(struct s_co_ctx *ctx1, void *ctx2)
{
	return swapcontext((ucontext_t *)ctx1->cc, ctx2);
}

static int ctx_create_context(co_ctx_t *ctx, void (*func)(void*), void *arg,
		char *stkbase, long stksiz)
{
	ucontext_t *cc = (ucontext_t *)ctx->cc;

	if (getcontext(cc))
		return -1;
	cc->uc_link = NULL;
	cc->uc_stack.ss_sp = stkbase;
	cc->uc_stack.ss_size = stksiz - sizeof(long);
	cc->uc_stack.ss_flags = 0;
	makecontext(cc, (void*)func, 1, arg);

	return 0;
}

static struct pcl_ctx_ops ctx_ops = {
	.create_context = ctx_create_context,
	.init_context = ctx_init_context_dummy,
	.free_context = ctx_free_context_dummy,
	.swap_context = ctx_swap_context,
};
#endif

#ifdef MCONTEXT
static int mctx_swap_context(struct s_co_ctx *ctx1, void *ctx2)
{
	return swapmcontext((m_ucontext_t *)ctx1->cc, ctx2);
}

static int mctx_create_context(co_ctx_t *ctx, void (*func)(void*), void *arg,
		char *stkbase, long stksiz)
{
	m_ucontext_t *cc = (m_ucontext_t *)ctx->cc;

	if (getmcontext(cc))
		return -1;
	cc->uc_link = NULL;
	cc->uc_stack.ss_sp = stkbase;
	cc->uc_stack.ss_size = stksiz - sizeof(long);
	cc->uc_stack.ss_flags = 0;
	makemcontext(cc, func, arg);

	return 0;
}

static struct pcl_ctx_ops mctx_ops = {
	.create_context = mctx_create_context,
	.init_context = ctx_init_context_dummy,
	.free_context = ctx_free_context_dummy,
	.swap_context = mctx_swap_context,
};
#endif

static int ptctx_swap_context(struct s_co_ctx *ctx1, void *ctx2)
{
	return swappcontext(ctx1->cc, ctx2);
}

static void pctx_pre(void *arg)
{
	if (sig_threads_wa)
		signal_block_async_nosig(arg);
}

static void pctx_post(void *arg)
{
	if (sig_threads_wa)
		sigprocmask(SIG_SETMASK, arg, NULL);
}

static int ptctx_create_context(co_ctx_t *ctx, void (*func)(void*), void *arg,
		char *stkbase, long stksiz)
{
	sigset_t oset;

	if (getpcontext(ctx->cc))
		return -1;
	makepcontext(ctx->cc, func, arg, pctx_pre, pctx_post, &oset);
	return 0;
}

static void ptctx_init_context(void *ctx)
{
	getpcontext(ctx);
}

static void ptctx_free_context(void *ctx)
{
	freepcontext(ctx);
}

static struct pcl_ctx_ops ptctx_ops = {
	.create_context = ptctx_create_context,
	.init_context = ptctx_init_context,
	.free_context = ptctx_free_context,
	.swap_context = ptctx_swap_context,
};

static struct pcl_ctx_ops *ops_arr[] = {
#ifdef MCONTEXT
	/*[PCL_C_MC] = */&mctx_ops,
#else
	NULL,
#endif
#if WANT_UCONTEXT
	/*[PCL_C_UC] = */&ctx_ops,
#else
	NULL,
#endif
	/*[PCL_C_PTH] = */&ptctx_ops,
};

int ctx_init(enum CoBackend b, struct s_co_ctx *ctx)
{
	if (b >= PCL_C_MAX)
		return -1;
	assert(ops_arr[b]);
	ctx->ops = ops_arr[b];
	ctx->ops->init_context(ctx->cc);
	return 0;
}

int ctx_sizeof(enum CoBackend b)
{
	if (!ops_arr[b])
		return 0;
	switch (b) {
#ifdef MCONTEXT
	case PCL_C_MC:
		return sizeof(m_ucontext_t);
#endif
#if WANT_UCONTEXT
	case PCL_C_UC:
		return sizeof(ucontext_t);
#endif
	case PCL_C_PTH:
		return sizeof(pcontext_t);
	default:
		return -1;
	}
}
