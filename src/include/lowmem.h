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

#ifndef __LOWMEM_H
#define __LOWMEM_H

int lowmem_init(void);
void *lowmem_alloc(int size);
void * lowmem_alloc_aligned(int align, int size);
void lowmem_free(void *p);
void lowmem_reset(void);
extern unsigned char *dosemu_lmheap_base;
#define DOSEMU_LMHEAP_OFFS_OF(s) \
  (((unsigned char *)(s) - dosemu_lmheap_base) + DOSEMU_LMHEAP_OFF)

int get_rm_stack(Bit16u *ss_p, Bit16u *sp_p, uint64_t cookie);
uint16_t put_rm_stack(uint64_t *cookie);
void get_rm_stack_regs(struct vm86_regs *regs, struct vm86_regs *saved_regs);
void rm_stack_enter(void);
void rm_stack_leave(void);

#endif
