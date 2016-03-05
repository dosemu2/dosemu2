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

#ifndef PCL_CTX_H
#define PCL_CTX_H

int ctx_init(co_ctx_t *ctx);
int mctx_init(co_ctx_t *ctx);
int ctx_sizeof(void);
int mctx_sizeof(void);
cothread_ctx *ctx_get_global_ctx(void);
cothread_ctx *mctx_get_global_ctx(void);

#endif
