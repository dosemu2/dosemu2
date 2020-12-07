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

#ifdef __linux__
/* kvm functions */
int init_kvm_cpu(void);
void init_kvm_monitor(void);
int kvm_vm86(struct vm86_struct *info);
int kvm_dpmi(sigcontext_t *scp);
void mprotect_kvm(int cap, dosaddr_t targ, size_t mapsize, int protect);
void mmap_kvm(int cap, void *addr, size_t mapsize, int protect);
void munmap_kvm(int cap, dosaddr_t targ, size_t mapsize);
void set_kvm_memory_regions(void);

void kvm_set_idt_default(int i);
void kvm_set_idt(int i, uint16_t sel, uint32_t offs, int is_32);

void kvm_done(void);

#else
static inline int init_kvm_cpu(void) { return -1; }
static inline void init_kvm_monitor(void) {}
static inline int kvm_vm86(struct vm86_struct *info) { return -1; }
static inline int kvm_dpmi(sigcontext_t *scp) { return -1; }
static inline void mprotect_kvm(int cap, dosaddr_t targ, size_t mapsize, int protect) {}
static inline void mmap_kvm(int cap, void *addr, size_t mapsize, int protect) {}
static inline void munmap_kvm(int cap, dosaddr_t targ, size_t mapsize) {}
static inline void set_kvm_memory_regions(void) {}
static inline void kvm_set_idt_default(int i) {}
static inline void kvm_set_idt(int i, uint16_t sel, uint32_t offs, int is_32) {}
static inline void kvm_done(void) {}
#endif

#endif
