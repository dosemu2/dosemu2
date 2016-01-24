#ifndef PCL_CTX_H
#define PCL_CTX_H

#include <ucontext.h>
#define GET_CTX(c) getcontext(c)
#define SET_CTX(c) setcontext(c)
#define SWAP_CTX(c1, c2) swapcontext(c1, c2)
#define MAKE_CTX(c, f, n) makecontext(c, f, n)
typedef ucontext_t co_core_ctx_t;

#endif
