#ifndef PCL_CTX_H
#define PCL_CTX_H

#include "mcontext.h"
#define GET_CTX(c) getmcontext(c)
#define SET_CTX(c) setmcontext(c)
#define SWAP_CTX(c1, c2) swapmcontext(c1, c2)
#define MAKE_CTX(c, f, n) makemcontext(c, f, n)
typedef m_ucontext_t co_core_ctx_t;

#endif
