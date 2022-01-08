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
#include "mcontext.h"
#include "pcl.h"
#include "pcl_private.h"
#include "pcl_ctx.h"

#if WANT_UCONTEXT
static int ctx_get_context(struct s_co_ctx *ctx)
{
	return getcontext((ucontext_t *)ctx->cc);
}

static int ctx_set_context(struct s_co_ctx *ctx)
{
	return setcontext((ucontext_t *)ctx->cc);
}

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
	.get_context = ctx_get_context,
	.set_context = ctx_set_context,
	.swap_context = ctx_swap_context,
};
#endif

static int mctx_get_context(struct s_co_ctx *ctx)
{
	return getmcontext((m_ucontext_t *)ctx->cc);
}

static int mctx_set_context(struct s_co_ctx *ctx)
{
	return setmcontext((m_ucontext_t *)ctx->cc);
}

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
	.get_context = mctx_get_context,
	.set_context = mctx_set_context,
	.swap_context = mctx_swap_context,
};

static struct pcl_ctx_ops *ops_arr[] = {
	/*[PCL_C_MC] = */&mctx_ops,
#if WANT_UCONTEXT
	/*[PCL_C_UC] = */&ctx_ops,
#endif
};

int ctx_init(enum CoBackend b, struct pcl_ctx_ops **ops)
{
	if (b >= PCL_C_MAX)
		return -1;
	assert(ops_arr[b]);
	*ops = ops_arr[b];
	return 0;
}

int ctx_sizeof(enum CoBackend b)
{
	switch (b) {
	case PCL_C_MC:
		return sizeof(m_ucontext_t);
#if WANT_UCONTEXT
	case PCL_C_UC:
		return sizeof(ucontext_t);
#endif
	default:
		return -1;
	}
}
