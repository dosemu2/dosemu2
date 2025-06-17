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
#ifndef PGALLOC_H
#define PGALLOC_H

void *pgainit(unsigned npages);
void pgadone(void *pool);
void pgareset(void *pool);
int pgaalloc(void *pool, unsigned npages, unsigned id);
void pgafree(void *pool, unsigned page);
int pgaresize(void *pool, unsigned page, unsigned oldpages, unsigned newpages);
int pgaavail_largest(void *pool);
struct pgrm {
    int id;
    int pgoff;
};
struct pgrm pgarmap(void *pool, unsigned page);

#endif
