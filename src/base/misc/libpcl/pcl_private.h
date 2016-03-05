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

#if !defined(PCL_PRIVATE_H)
#define PCL_PRIVATE_H

#include <stdio.h>
#include <stdlib.h>

/*
 * The following value must be power of two (N^2).
 */
#define CO_STK_ALIGN 256
#define CO_STK_COROSIZE(x) (((x) + sizeof(coroutine) + CO_STK_ALIGN - 1) & ~(CO_STK_ALIGN - 1))
#define CO_MIN_SIZE (4 * 1024)

struct s_cothread_ctx;
typedef struct s_co_ctx {
	void *cc;
	int (*get_context)(struct s_co_ctx *ctx);
	int (*set_context)(struct s_co_ctx *ctx);
	int (*swap_context)(struct s_co_ctx *ctx1, void *ctx2);
	struct s_cothread_ctx *(*get_global_ctx)(void);
} co_ctx_t;

typedef struct s_coroutine {
	co_ctx_t ctx;
	int alloc;
	int exited:1;
	struct s_coroutine *caller;
	struct s_coroutine *restarget;
	void (*func)(void *);
	void *data;
	char *stack;
	char stk[0];
} coroutine;

typedef struct s_cothread_ctx {
	coroutine co_main;
	coroutine *co_curr;
	char stk0[CO_MIN_SIZE];
} cothread_ctx;

#endif
