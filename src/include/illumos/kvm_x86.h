/*
 * GPL HEADER START
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * GPL HEADER END
 *
 * Copyright 2011 various Linux Kernel contributors.
 * Copyright 2018 Joyent, Inc.
 */

#ifndef __KVM_X86_H
#define	__KVM_X86_H

/* See <sys/kvm.h> for an explanation of why this is necessary */
#ifndef __GNUC__
#error "The KVM Header files require GNU C extensions for compatibility."
#endif

#include <sys/types.h>

#define	KVM_NR_INTERRUPTS 256

/* for KVM_GET_IRQCHIP and KVM_SET_IRQCHIP */
typedef struct kvm_pic_state {
	uint8_t last_irr;	/* edge detection */
	uint8_t irr;		/* interrupt request register */
	uint8_t imr;		/* interrupt mask register */
	uint8_t isr;		/* interrupt service register */
	uint8_t priority_add;	/* highest irq priority */
	uint8_t irq_base;
	uint8_t read_reg_select;
	uint8_t poll;
	uint8_t special_mask;
	uint8_t init_state;
	uint8_t auto_eoi;
	uint8_t rotate_on_auto_eoi;
	uint8_t special_fully_nested_mode;
	uint8_t init4;		/* true if 4 byte init */
	uint8_t elcr;		/* PIIX edge/trigger selection */
	uint8_t elcr_mask;
} kvm_pic_state_t;

#define	KVM_IOAPIC_NUM_PINS  24
typedef struct kvm_ioapic_state {
	uint64_t base_address;
	uint32_t ioregsel;
	uint32_t id;
	uint32_t irr;
	uint32_t pad;
	union {
		uint64_t bits;
		struct {
			uint8_t vector;
			uint8_t delivery_mode:3;
			uint8_t dest_mode:1;
			uint8_t delivery_status:1;
			uint8_t polarity:1;
			uint8_t remote_irr:1;
			uint8_t trig_mode:1;
			uint8_t mask:1;
			uint8_t reserve:7;
			uint8_t reserved[4];
			uint8_t dest_id;
		} fields;
	} redirtbl[KVM_IOAPIC_NUM_PINS];
} kvm_ioapic_state_t;

#define	KVM_IRQCHIP_PIC_MASTER	0
#define	KVM_IRQCHIP_PIC_SLAVE	1
#define	KVM_IRQCHIP_IOAPIC	2
#define	KVM_NR_IRQCHIPS		3

/* for KVM_GET_REGS and KVM_SET_REGS */
typedef struct kvm_regs {
	/* out (KVM_GET_REGS) / in (KVM_SET_REGS) */
	uint64_t rax, rbx, rcx, rdx;
	uint64_t rsi, rdi, rsp, rbp;
	uint64_t r8,  r9,  r10, r11;
	uint64_t r12, r13, r14, r15;
	uint64_t rip, rflags;
} kvm_regs_t;

/* for KVM_GET_LAPIC and KVM_SET_LAPIC */
#define	KVM_APIC_REG_SIZE 0x400
typedef struct kvm_lapic_state {
	char regs[KVM_APIC_REG_SIZE];
} kvm_lapic_state_t;

typedef struct kvm_segment {
	uint64_t base;
	uint32_t limit;
	unsigned short selector;
	unsigned char  type;
	unsigned char  present, dpl, db, s, l, g, avl;
	unsigned char  unusable;
	unsigned char  padding;
} kvm_segment_t;

typedef struct kvm_dtable {
	uint64_t base;
	unsigned short limit;
	unsigned short padding[3];
} kvm_dtable_t;

/* for KVM_GET_SREGS and KVM_SET_SREGS */
typedef struct kvm_sregs {
	/* out (KVM_GET_SREGS) / in (KVM_SET_SREGS) */
	struct kvm_segment cs, ds, es, fs, gs, ss;
	struct kvm_segment tr, ldt;
	struct kvm_dtable gdt, idt;
	uint64_t cr0, cr2, cr3, cr4, cr8;
	uint64_t efer;
	uint64_t apic_base;
	unsigned long interrupt_bitmap[(KVM_NR_INTERRUPTS + (64-1)) / 64];
} kvm_sregs_t;

/* for KVM_GET_FPU and KVM_SET_FPU */
typedef struct kvm_fpu {
	unsigned char  fpr[8][16];
	unsigned short fcw;
	unsigned short fsw;
	unsigned char  ftwx;  /* in fxsave format */
	unsigned char  pad1;
	unsigned short last_opcode;
	uint64_t last_ip;
	uint64_t last_dp;
	unsigned char  xmm[16][16];
	uint32_t mxcsr;
	uint32_t pad2;
} kvm_fpu_t;

typedef struct kvm_msr_entry {
	uint32_t index;
	uint32_t reserved;
	uint64_t data;
} kvm_msr_entry_t;

/* for KVM_GET_MSRS and KVM_SET_MSRS */
typedef struct kvm_msrs {
	uint32_t nmsrs; /* number of msrs in entries */
	uint32_t pad;

	struct kvm_msr_entry entries[100];
} kvm_msrs_t;

/* for KVM_GET_MSR_INDEX_LIST */
typedef struct kvm_msr_list {
	uint32_t nmsrs; /* number of msrs in entries */
	uint32_t indices[1];
} kvm_msr_list_t;

typedef struct kvm_cpuid_entry {
	uint32_t function;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t padding;
} kvm_cpuid_entry_t;

/* for KVM_SET_CPUID */
typedef struct kvm_cpuid {
	uint32_t nent;
	uint32_t padding;
	struct kvm_cpuid_entry entries[100];
} kvm_cpuid_t;

typedef struct kvm_cpuid_entry2 {
	uint32_t function;
	uint32_t index;
	uint32_t flags;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t padding[3];
} kvm_cpuid_entry2_t;


#define	KVM_CPUID_FLAG_SIGNIFCANT_INDEX 1
#define	KVM_CPUID_FLAG_STATEFUL_FUNC    2
#define	KVM_CPUID_FLAG_STATE_READ_NEXT  4

/* for KVM_SET_CPUID2 */
typedef struct kvm_cpuid2 {
	uint32_t nent;
	uint32_t padding;
	struct kvm_cpuid_entry2 entries[0];
} kvm_cpuid2_t;

/* for KVM_GET_PIT and KVM_SET_PIT */
typedef struct kvm_pit_channel_state {
	uint32_t count; /* can be 65536 */
	uint16_t latched_count;
	uint8_t count_latched;
	uint8_t status_latched;
	uint8_t status;
	uint8_t read_state;
	uint8_t write_state;
	uint8_t write_latch;
	uint8_t rw_mode;
	uint8_t mode;
	uint8_t bcd;
	uint8_t gate;
	int64_t count_load_time;
} kvm_pit_channel_state_t;

typedef struct kvm_debug_exit_arch {
	uint32_t exception;
	uint32_t pad;
	uint64_t pc;
	uint64_t dr6;
	uint64_t dr7;
} kvm_debug_exit_arch_t;

#define	KVM_GUESTDBG_USE_SW_BP		0x00010000
#define	KVM_GUESTDBG_USE_HW_BP		0x00020000
#define	KVM_GUESTDBG_INJECT_DB		0x00040000
#define	KVM_GUESTDBG_INJECT_BP		0x00080000

/* for KVM_SET_GUEST_DEBUG */
typedef struct kvm_guest_debug_arch {
	uint64_t debugreg[8];
} kvm_guest_debug_arch_t;


typedef struct kvm_pit_state {
	struct kvm_pit_channel_state channels[3];
} kvm_pit_state_t;

#define	KVM_PIT_FLAGS_HPET_LEGACY  0x00000001

typedef struct kvm_pit_state2 {
	struct kvm_pit_channel_state channels[3];
	uint32_t flags;
	uint32_t reserved[9];
} kvm_pit_state2_t;

typedef struct kvm_reinject_control {
	uint8_t pit_reinject;
	uint8_t reserved[31];
} kvm_reinject_control_t;

/* When set in flags, include corresponding fields on KVM_SET_VCPU_EVENTS */
#define	KVM_VCPUEVENT_VALID_NMI_PENDING	0x00000001
#define	KVM_VCPUEVENT_VALID_SIPI_VECTOR	0x00000002

/* for KVM_GET/SET_VCPU_EVENTS */
typedef struct kvm_vcpu_events {
	struct {
		unsigned char injected;
		unsigned char nr;
		unsigned char has_error_code;
		unsigned char pad;
		uint32_t error_code;
	} exception;
	struct {
		unsigned char injected;
		unsigned char nr;
		unsigned char soft;
		unsigned char pad;
	} interrupt;
	struct {
		unsigned char injected;
		unsigned char pending;
		unsigned char masked;
		unsigned char pad;
	} nmi;
	uint32_t sipi_vector;
	uint32_t flags;
	uint32_t reserved[10];
} kvm_vcpu_events_t;

/*
 * The following should provide an optimization barrier.
 * If the system does reorder loads and stores, this needs to be changed.
 */
#ifdef _KERNEL
#define	smp_wmb()   __asm__ __volatile__("" ::: "memory")
#define	smp_rmb()   __asm__ __volatile__("" ::: "memory")
#endif

#endif /* __KVM_X86_H */
