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

#ifdef USE_KVM
/* kvm functions */
int init_kvm_cpu(void);
int kvm_vm86(struct vm86_struct *info);
int kvm_dpmi(cpuctx_t *scp);
void kvm_reset_to_vm86(void);
void kvm_vcpi_pm_switch(dosaddr_t addr);
dosaddr_t kvm_vcpi_get_pmi(dosaddr_t pagetable, dosaddr_t gdt, unsigned *pages);
void mprotect_kvm(int cap, dosaddr_t targ, size_t mapsize, int protect);
void mmap_kvm(int cap, void *addr, size_t mapsize, int protect, dosaddr_t targ);
void set_kvm_memory_regions(void);
void kvm_set_dirty_log(dosaddr_t base, dosaddr_t size);
void kvm_get_dirty_map(dosaddr_t base, unsigned char *bitmap);

void kvm_set_idt_default(int i);
void kvm_set_idt(int i, uint16_t sel, uint32_t offs, int is_32, int tg);

void kvm_enter(int pm);
void kvm_leave(int pm);
void kvm_update_fpu(void);
void kvm_getset_debugregs(uint32_t debugregs[8], int set);

void kvm_done(void);

#else
static inline int init_kvm_cpu(void) { return -1; }
static inline int kvm_vm86(struct vm86_struct *info) { return -1; }
static inline int kvm_dpmi(cpuctx_t *scp) { return -1; }
static inline void kvm_reset_to_vm86(void) {}
static inline void kvm_vcpi_pm_switch(dosaddr_t addr) {}
static inline dosaddr_t kvm_vcpi_get_pmi(dosaddr_t pagetable, dosaddr_t gdt, unsigned *pages) {return 0;}
static inline void mprotect_kvm(int cap, dosaddr_t targ, size_t mapsize, int protect) {}
static inline void mmap_kvm(int cap, void *addr, size_t mapsize, int protect, dosaddr_t targ) {}
static inline void munmap_kvm(int cap, dosaddr_t targ, size_t mapsize) {}
static inline void set_kvm_memory_regions(void) {}
static inline void kvm_set_dirty_log(dosaddr_t base, dosaddr_t size) {}
static inline void kvm_get_dirty_map(dosaddr_t base, unsigned char *bitmap) {}
static inline void kvm_set_idt_default(int i) {}
static inline void kvm_set_idt(int i, uint16_t sel, uint32_t offs, int is_32,
    int tg) {}
static inline void kvm_enter(int pm) {}
static inline void kvm_leave(int pm) {}
static inline void kvm_update_fpu(void) {}
static inline void kvm_getset_debugregs(uint32_t debugregs[8], int set) {}
static inline void kvm_done(void) {}
#endif

#endif
