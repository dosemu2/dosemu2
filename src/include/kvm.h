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

#ifndef KVM_H
#define KVM_H

#include "emu.h"

/* kvm functions */
int init_kvm_cpu(void);
void init_kvm_monitor(void);
int kvm_vm86(struct vm86_struct *info);
int kvm_dpmi(sigcontext_t *scp);
void mprotect_kvm(int cap, dosaddr_t targ, size_t mapsize, int protect);
void mmap_kvm(int cap, void *addr, size_t mapsize, int protect);
void set_kvm_memory_regions(void);

#endif
