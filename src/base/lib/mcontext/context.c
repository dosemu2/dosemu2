/* This code is taken from libtask library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#include <ucontext.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "mcontext.h"

void makemcontext(m_ucontext_t *ucp, void (*func)(void*), void *arg)
{
	uintptr_t *sp;

	sp = ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size/sizeof(*ucp->uc_stack.ss_sp);
	sp = (void*)((uintptr_t)sp - (uintptr_t)sp%16); /* 16-align for OS X */
#ifdef __i386__
	sp -= 3;	// alignment
	*--sp = (uintptr_t)arg;
#else
	ucp->uc_mcontext.mc_rdi = (uintptr_t)arg;
#endif
	*--sp = 0;	/* return address */
#ifdef __i386__
	ucp->uc_mcontext.mc_eip = (uintptr_t)func;
	ucp->uc_mcontext.mc_esp = (uintptr_t)sp;
#else
	ucp->uc_mcontext.mc_rip = (uintptr_t)func;
	ucp->uc_mcontext.mc_rsp = (uintptr_t)sp;
#endif
}

int swapmcontext(m_ucontext_t *oucp, const m_ucontext_t *ucp)
{
	if(getmcontext(oucp) == 0)
		setmcontext(ucp);
	return 0;
}

int getmcontext(struct m_ucontext *u)
{
	memset(&u->uc_mcontext, 0, sizeof u->uc_mcontext);
	return _getmcontext(&u->uc_mcontext);
}
