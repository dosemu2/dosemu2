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
#define _CO_STK_ALIGN 256
#define CO_STK_ALIGN(x) (((x) + _CO_STK_ALIGN - 1) & ~(_CO_STK_ALIGN - 1))
#define CO_STK_COROSIZE(x) CO_STK_ALIGN((x) + sizeof(coroutine))
#define CO_MIN_SIZE (4 * 1024)

struct s_co_ctx;
struct pcl_ctx_ops {
	int (*create_context)(struct s_co_ctx *ctx, void (*func)(void*),
		void *arg, char *stkbase, long stksiz);
	int (*get_context)(struct s_co_ctx *ctx);
	int (*set_context)(struct s_co_ctx *ctx);
	int (*swap_context)(struct s_co_ctx *ctx1, void *ctx2);
};

typedef struct s_co_ctx {
	void *cc;
	struct pcl_ctx_ops *ops;
} co_ctx_t;

typedef struct s_co_base {
	co_ctx_t ctx;
	struct s_co_base *caller;
	struct s_co_base *restarget;
	struct s_cothread_ctx *ctx_main;
	void *tmp;
	int stack_size;
	char *stack;
	int exited:1;
} co_base;

typedef
#ifdef __cplusplus
struct s_coroutine : public co_base {
#else
struct s_coroutine {
	co_base;
#endif
	int alloc;
	void (*func)(void *);
	void *data;
	char stk[0];
} coroutine;

typedef struct s_cothread_ctx {
	co_base co_main;
	co_base *co_curr;
	int ctx_sizeof;
	char stk0[0];
} cothread_ctx;

#endif
