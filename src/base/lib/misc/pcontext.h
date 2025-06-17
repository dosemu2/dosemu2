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
#ifndef PCONTEXT_H
#define PCONTEXT_H

struct p_ucontext {
    void *sem;
    void *jmp;
    int done;
    void *done_sem;
    void (*func)(void*);
    void *arg;
};
typedef struct p_ucontext pcontext_t;

int getpcontext(pcontext_t *pct);
void makepcontext(pcontext_t *pct, void (*func)(void*), void *arg,
        void (*pre)(void*), void (*post)(void*), void *carg);
int swappcontext(pcontext_t *opct, const pcontext_t *pct);
void freepcontext(pcontext_t *pct);

#endif
