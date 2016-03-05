#ifndef PCL_CTX_H
#define PCL_CTX_H

int ctx_create_context(co_ctx_t *ctx, void *func, void *arg, char *stkbase,
		long stksiz);
int mctx_create_context(co_ctx_t *ctx, void *func, void *arg, char *stkbase,
		long stksiz);
int ctx_init(co_ctx_t *ctx);
int mctx_init(co_ctx_t *ctx);
int ctx_sizeof(void);
int mctx_sizeof(void);
cothread_ctx *ctx_get_global_ctx(void);
cothread_ctx *mctx_get_global_ctx(void);

#endif
