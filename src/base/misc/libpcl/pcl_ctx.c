#include <ucontext.h>
#include "mcontext.h"
#include "pcl_private.h"
#include "pcl_ctx.h"

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

cothread_ctx *ctx_get_global_ctx(void)
{
	static cothread_ctx tctx;

	if (tctx.co_curr == NULL)
		tctx.co_curr = &tctx.co_main;

	return &tctx;
}

int ctx_init(co_ctx_t *ctx)
{
	ctx->get_context = ctx_get_context;
	ctx->set_context = ctx_set_context;
	ctx->swap_context = ctx_swap_context;
	ctx->get_global_ctx = ctx_get_global_ctx;
	return 0;
}

int ctx_create_context(co_ctx_t *ctx, void *func, void *arg, char *stkbase,
		long stksiz)
{
	ucontext_t *cc = (ucontext_t *)ctx->cc;

	if (getcontext(cc))
		return -1;
	cc->uc_link = NULL;
	cc->uc_stack.ss_sp = stkbase;
	cc->uc_stack.ss_size = stksiz - sizeof(long);
	cc->uc_stack.ss_flags = 0;
	makecontext(cc, func, 1, arg);

	ctx_init(ctx);

	return 0;
}

int ctx_sizeof(void)
{
	return sizeof(ucontext_t);
}

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

cothread_ctx *mctx_get_global_ctx(void)
{
	static cothread_ctx tctx;

	if (tctx.co_curr == NULL)
		tctx.co_curr = &tctx.co_main;

	return &tctx;
}

int mctx_init(co_ctx_t *ctx)
{
	ctx->get_context = mctx_get_context;
	ctx->set_context = mctx_set_context;
	ctx->swap_context = mctx_swap_context;
	ctx->get_global_ctx = mctx_get_global_ctx;
	return 0;
}

int mctx_create_context(co_ctx_t *ctx, void *func, void *arg, char *stkbase,
		long stksiz)
{
	m_ucontext_t *cc = (m_ucontext_t *)ctx->cc;

	if (getmcontext(cc))
		return -1;
	cc->uc_link = NULL;
	cc->uc_stack.ss_sp = stkbase;
	cc->uc_stack.ss_size = stksiz - sizeof(long);
	cc->uc_stack.ss_flags = 0;
	makemcontext(cc, func, 1, arg);

	mctx_init(ctx);

	return 0;
}

int mctx_sizeof(void)
{
	return sizeof(m_ucontext_t);
}
