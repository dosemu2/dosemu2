/* This code is taken from libtask library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#ifndef MCONTEXT_H
#define MCONTEXT_H

#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct m_mcontext m_mcontext_t;
typedef struct m_ucontext m_ucontext_t;

#	if defined(__i386__)
#		include "386-ucontext.h"
#	elif defined(__x86_64__)
#		include "amd64-ucontext.h"
#	else
#		error Unsupported arch
#	endif

struct m_ucontext {
	/*
	 * Keep the order of the first two fields. Also,
	 * keep them the first two fields in the structure.
	 * This way we can have a union with struct
	 * sigcontext and ucontext_t. This allows us to
	 * support them both at the same time.
	 * note: the union is not defined, though.
	 */
	sigset_t	uc_sigmask;
	m_mcontext_t	uc_mcontext;

	struct __ucontext *uc_link;
	stack_t		uc_stack;
	int		__spare__[8];
};

extern int _getmcontext(m_mcontext_t*);
extern int _setmcontext(const m_mcontext_t*);
static inline int setmcontext(const struct m_ucontext *u)
{
	return _setmcontext(&u->uc_mcontext);
}
extern int getmcontext(struct m_ucontext *u);
extern int swapmcontext(m_ucontext_t*, const m_ucontext_t*);
extern void makemcontext(m_ucontext_t*, void(*)(void*), void*);

#ifdef __cplusplus
};
#endif

#endif
