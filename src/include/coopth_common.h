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

/* what both sides of the coopth API need: the thread function type, the
 * invalid tid, and the few entry points that are callable from the host
 * as well as from inside a thread. The latter are the only ones that do
 * not fall on one side of the split; each of them tests
 * _coopth_is_in_thread_nowarn() and behaves differently depending on the
 * answer, so splitting them means giving the two behaviours two names. */

#ifndef COOPTH_COMMON_H
#define COOPTH_COMMON_H

#include <stdint.h>

#define COOPTH_TID_INVALID (-1)

typedef void (*coopth_func_t)(void *arg);

int coopth_get_tid(void);
void coopth_leave(void);
void coopth_abandon(void);

#endif
