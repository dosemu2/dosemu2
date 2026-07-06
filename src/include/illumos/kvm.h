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
 * Copyright 2011 Joyent, Inc. All Rights Reserved.
 */

#ifndef __KVM_H
#define	__KVM_H

/*
 * The userland / kernel interface was initially defined by the Linux KVM
 * project. As a part of our efforts to port it, it's important to maintain
 * compatibility with the portions of that interface that we implement. A side
 * effect of this is that we require GNU extensions to C. Rather than let a
 * consumer go crazy trying to understand and track down odd compiler errors, we
 * explicitly note that this file is not ISO C.
 */
#ifndef __GNUC__
#error "The KVM Header files require GNU C extensions for compatibility."
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioccom.h>
#include "kvm_x86.h"

#define	KVM_API_VERSION 12   /* same as linux (for qemu compatability...) */

/* for KVM_CREATE_MEMORY_REGION */
typedef struct kvm_memory_region {
	uint32_t slot;
	uint32_t flags;
	uint64_t guest_phys_addr;
	uint64_t memory_size; /* bytes */
} kvm_memory_region_t;

/* for KVM_SET_USER_MEMORY_REGION */
typedef struct kvm_userspace_memory_region {
	uint32_t slot;
	uint32_t flags;
	uint64_t guest_phys_addr;
	uint64_t memory_size; /* bytes */
	uint64_t userspace_addr; /* start of the userspace allocated memory */
} kvm_userspace_memory_region_t;

/* for kvm_memory_region::flags */
#define	KVM_MEM_LOG_DIRTY_PAGES		1UL
#define	KVM_MEMSLOT_INVALID		(1UL << 1)

/* for KVM_IRQ_LINE */
typedef struct kvm_irq_level {
	/*
	 * ACPI gsi notion of irq.
	 * For IA-64 (APIC model) IOAPIC0: irq 0-23; IOAPIC1: irq 24-47..
	 * For X86 (standard AT mode) PIC0/1: irq 0-15. IOAPIC0: 0-23..
	 */
	union {
		uint32_t irq;
		int32_t status;
	};
	uint32_t level;
} kvm_irq_level_t;

typedef struct kvm_irqchip {
	uint32_t chip_id;
	uint32_t pad;
	union {
		char dummy[512];  /* reserving space */
		struct kvm_pic_state pic;
		struct kvm_ioapic_state ioapic;
	} chip;
} kvm_irqchip_t;

/* for KVM_CREATE_PIT2 */
typedef struct kvm_pit_config {
	uint32_t flags;
	uint32_t pad[15];
} kvm_pit_config_t;

#define	KVM_PIT_SPEAKER_DUMMY		1

#define	KVM_EXIT_UNKNOWN		0
#define	KVM_EXIT_EXCEPTION		1
#define	KVM_EXIT_IO			2
#define	KVM_EXIT_HYPERCALL		3
#define	KVM_EXIT_DEBUG			4
#define	KVM_EXIT_HLT			5
#define	KVM_EXIT_MMIO			6
#define	KVM_EXIT_IRQ_WINDOW_OPEN	7
#define	KVM_EXIT_SHUTDOWN		8
#define	KVM_EXIT_FAIL_ENTRY		9
#define	KVM_EXIT_INTR			10
#define	KVM_EXIT_SET_TPR		11
#define	KVM_EXIT_TPR_ACCESS		12
#define	KVM_EXIT_S390_SIEIC		13
#define	KVM_EXIT_S390_RESET		14
#define	KVM_EXIT_DCR			15
#define	KVM_EXIT_NMI			16
#define	KVM_EXIT_INTERNAL_ERROR		17

/* For KVM_EXIT_INTERNAL_ERROR */
#define	KVM_INTERNAL_ERROR_EMULATION	1
#define	KVM_INTERNAL_ERROR_SIMUL_EX	2

/* for KVM_RUN, returned by mmap(vcpu_fd, offset=0) */
typedef struct kvm_run {
	/* in */
	unsigned char request_interrupt_window;
	unsigned char padding1[7];

	/* out */
	uint32_t exit_reason;
	unsigned char ready_for_interrupt_injection;
	unsigned char if_flag;
	unsigned char padding2[2];

	/* in (pre_kvm_run), out (post_kvm_run) */
	uint64_t cr8;
	uint64_t apic_base;

	union {
		/* KVM_EXIT_UNKNOWN */
		struct {
			uint64_t hardware_exit_reason;
		} hw;
		/* KVM_EXIT_FAIL_ENTRY */
		struct {
			uint64_t hardware_entry_failure_reason;
		} fail_entry;
		/* KVM_EXIT_EXCEPTION */
		struct {
			uint32_t exception;
			uint32_t error_code;
		} ex;
		/* KVM_EXIT_IO */
		struct {
#define	KVM_EXIT_IO_IN  0
#define	KVM_EXIT_IO_OUT 1
			unsigned char direction;
			unsigned char size; /* bytes */
			unsigned short port;
			uint32_t count;
			uint64_t data_offset; /* relative to kvm_run start */
		} io;
		struct {
			struct kvm_debug_exit_arch arch;
		} debug;
		/* KVM_EXIT_MMIO */
		struct {
			uint64_t phys_addr;
			unsigned char  data[8];
			uint32_t len;
			unsigned char  is_write;
		} mmio;
		/* KVM_EXIT_HYPERCALL */
		struct {
			uint64_t nr;
			uint64_t args[6];
			uint64_t ret;
			uint32_t longmode;
			uint32_t pad;
		} hypercall;
		/* KVM_EXIT_TPR_ACCESS */
		struct {
			uint64_t rip;
			uint32_t is_write;
			uint32_t pad;
		} tpr_access;
		/* KVM_EXIT_DCR */
		struct {
			uint32_t dcrn;
			uint32_t data;
			unsigned char  is_write;
		} dcr;
		struct {
			uint32_t suberror;
			/* Available with KVM_CAP_INTERNAL_ERROR_DATA: */
			uint32_t ndata;
			uint64_t data[16];
		} internal;
		/* Fix the size of the union. */
		char padding[256];
	};
} kvm_run_t;

typedef struct kvm_coalesced_mmio_zone {
	uint64_t addr;
	uint32_t size;
	uint32_t pad;
} kvm_coalesced_mmio_zone_t;

typedef struct kvm_coalesced_mmio {
	uint64_t phys_addr;
	uint32_t len;
	uint32_t pad;
	unsigned char  data[8];
} kvm_coalesced_mmio_t;

typedef struct kvm_coalesced_mmio_ring {
	uint32_t first, last;
	struct kvm_coalesced_mmio coalesced_mmio[1];
} kvm_coalesced_mmio_ring_t;

#define	KVM_COALESCED_MMIO_MAX \
	((PAGESIZE - sizeof (struct kvm_coalesced_mmio_ring)) / \
	sizeof (struct kvm_coalesced_mmio))

/* for KVM_INTERRUPT */
typedef struct kvm_interrupt {
	/* in */
	uint32_t irq;
} kvm_interrupt_t;

/* for KVM_GET_DIRTY_LOG */
typedef struct kvm_dirty_log {
	uint32_t slot;
	uint32_t padding1;
	union {
		void  *dirty_bitmap; /* one bit per page */
		uint64_t padding2;
	};
} kvm_dirty_log_t;

/* for KVM_SET_SIGNAL_MASK */
typedef struct kvm_signal_mask {
	uint32_t len;
	uint8_t sigset[1];
} kvm_signal_mask_t;

/* for KVM_TPR_ACCESS_REPORTING */
typedef struct kvm_tpr_access_ctl {
	uint32_t enabled;
	uint32_t flags;
	uint32_t reserved[8];
} kvm_tpr_access_ctl_t;

/* for KVM_SET_VAPIC_ADDR */
typedef struct kvm_vapic_addr {
	uint64_t vapic_addr;
} kvm_vapic_addr_t;

/* for KVM_SET_MP_STATE */
#define	KVM_MP_STATE_RUNNABLE		0
#define	KVM_MP_STATE_UNINITIALIZED	1
#define	KVM_MP_STATE_INIT_RECEIVED	2
#define	KVM_MP_STATE_HALTED		3
#define	KVM_MP_STATE_SIPI_RECEIVED	4

typedef struct kvm_mp_state {
	uint32_t mp_state;
} kvm_mp_state_t;

/* for KVM_SET_GUEST_DEBUG */

#define	KVM_GUESTDBG_ENABLE		0x00000001
#define	KVM_GUESTDBG_SINGLESTEP		0x00000002

typedef struct kvm_guest_debug {
	uint32_t control;
	uint32_t pad;
	struct kvm_guest_debug_arch arch;
} kvm_guest_debug_t;

/* ioctl commands */

#define	KVMIO 0xAE

/*
 * ioctls for /dev/kvm fds:
 */
#define	KVM_GET_API_VERSION	_IO(KVMIO,   0x00)
#define	KVM_CREATE_VM		_IO(KVMIO,   0x01) /* returns a VM fd */
#define	KVM_GET_MSR_INDEX_LIST	_IOWR(KVMIO, 0x02, struct kvm_msr_list)
#define	KVM_CLONE		_IO(KVMIO,   0x20)

/*
 * Check if a kvm extension is available.  Argument is extension number,
 * return is 1 (yes) or 0 (no, sorry).
 */
#define	KVM_CHECK_EXTENSION	_IO(KVMIO,   0x03)

/*
 * Get size for mmap(vcpu_fd)
 */
#define	KVM_GET_VCPU_MMAP_SIZE	_IO(KVMIO,   0x04) /* in bytes */
#define	KVM_GET_SUPPORTED_CPUID	_IOWR(KVMIO, 0x05, struct kvm_cpuid2)

/*
 * Extension capability list.
 */
#define	KVM_CAP_IRQCHIP				0
#define	KVM_CAP_HLT				1
#define	KVM_CAP_MMU_SHADOW_CACHE_CONTROL	2
#define	KVM_CAP_USER_MEMORY			3
#define	KVM_CAP_SET_TSS_ADDR			4
#define	KVM_CAP_VAPIC				6
#define	KVM_CAP_EXT_CPUID			7
#define	KVM_CAP_CLOCKSOURCE			8
#define	KVM_CAP_NR_VCPUS			9
#define	KVM_CAP_NR_MEMSLOTS			10
#define	KVM_CAP_PIT				11
#define	KVM_CAP_NOP_IO_DELAY			12
#define	KVM_CAP_PV_MMU				13
#define	KVM_CAP_MP_STATE			14
#define	KVM_CAP_COALESCED_MMIO			15
#define	KVM_CAP_SYNC_MMU			16

#ifdef __KVM_HAVE_DEVICE_ASSIGNMENT
#define	KVM_CAP_DEVICE_ASSIGNMENT		17
#endif

#define	KVM_CAP_IOMMU				18

#ifdef __KVM_HAVE_MSI
#define	KVM_CAP_DEVICE_MSI			20
#endif

/* Bug in KVM_SET_USER_MEMORY_REGION fixed: */
#define	KVM_CAP_DESTROY_MEMORY_REGION_WORKS	21

#define	KVM_CAP_USER_NMI			22

#ifdef __KVM_HAVE_GUEST_DEBUG
#define	KVM_CAP_SET_GUEST_DEBUG			23
#endif
#define	KVM_CAP_REINJECT_CONTROL		24
#define	KVM_CAP_IRQ_ROUTING			25
#define	KVM_CAP_IRQ_INJECT_STATUS		26
#ifdef __KVM_HAVE_DEVICE_ASSIGNMENT
#define	KVM_CAP_DEVICE_DEASSIGNMENT		27
#endif
#ifdef __KVM_HAVE_MSIX
#define	KVM_CAP_DEVICE_MSIX			28
#endif
#define	KVM_CAP_ASSIGN_DEV_IRQ			29
/* Another bug in KVM_SET_USER_MEMORY_REGION fixed: */
#define	KVM_CAP_JOIN_MEMORY_REGIONS_WORKS	30
#define	KVM_CAP_MCE				31
#define	KVM_CAP_PIT2				33
#define	KVM_CAP_SET_BOOT_CPU_ID			34
#define	KVM_CAP_PIT_STATE2			35
#define	KVM_CAP_IOEVENTFD			36
#define	KVM_CAP_SET_IDENTITY_MAP_ADDR		37
#define	KVM_CAP_XEN_HVM				38
#define	KVM_CAP_ADJUST_CLOCK			39
#define	KVM_CAP_INTERNAL_ERROR_DATA		40
#define	KVM_CAP_VCPU_EVENTS			41
#define	KVM_CAP_S390_PSW			42
#define	KVM_CAP_PPC_SEGSTATE			43
#define	KVM_CAP_HYPERV				44
#define	KVM_CAP_HYPERV_VAPIC			45
#define	KVM_CAP_HYPERV_SPIN			46
#define	KVM_CAP_PCI_SEGMENT			47
#define	KVM_CAP_X86_ROBUST_SINGLESTEP		51

#ifdef KVM_CAP_IRQ_ROUTING
typedef struct kvm_irq_routing_irqchip {
	uint32_t irqchip;
	uint32_t pin;
} kvm_irq_routing_irqchip_t;

typedef struct kvm_irq_routing_msi {
	uint32_t address_lo;
	uint32_t address_hi;
	uint32_t data;
	uint32_t pad;
} kvm_irq_routing_msi_t;

/* gsi routing entry types */
#define	KVM_IRQ_ROUTING_IRQCHIP 1
#define	KVM_IRQ_ROUTING_MSI 2

typedef struct kvm_irq_routing_entry {
	uint32_t gsi;
	uint32_t type;
	uint32_t flags;
	uint32_t pad;
	union {
		struct kvm_irq_routing_irqchip irqchip;
		struct kvm_irq_routing_msi msi;
		uint32_t pad[8];
	} u;
} kvm_irq_routing_entry_t;

typedef struct kvm_irq_routing {
	uint32_t nr;
	uint32_t flags;
	struct kvm_irq_routing_entry entries[1];
} kvm_irq_routing_t;

#endif /* KVM_CAP_IRQ_ROUTING */

#ifdef KVM_CAP_MCE
/* x86 MCE */
typedef struct kvm_x86_mce {
	uint64_t status;
	uint64_t addr;
	uint64_t misc;
	uint64_t mcg_status;
	uint8_t bank;
	uint8_t pad1[7];
	uint64_t pad2[3];
} kvm_x86_mce_t;
#endif /* KVM_CAP_MCE */

typedef struct kvm_clock_data {
	uint64_t clock;
	uint32_t flags;
	uint32_t pad[9];
} kvm_clock_data_t;

/*
 * ioctls for VM fds
 */

/*
 * KVM_CREATE_VCPU receives as a parameter the vcpu slot, and returns
 * a vcpu fd.
 */
#define	KVM_CREATE_VCPU		_IO(KVMIO,   0x41)
#define	KVM_GET_DIRTY_LOG	_IOW(KVMIO,  0x42, struct kvm_dirty_log)
#define	KVM_SET_NR_MMU_PAGES	_IO(KVMIO,   0x44)
#define	KVM_GET_NR_MMU_PAGES	_IO(KVMIO,   0x45)
#define	KVM_SET_USER_MEMORY_REGION _IOW(KVMIO, 0x46, \
					    struct kvm_userspace_memory_region)

#define	KVM_SET_TSS_ADDR	_IO(KVMIO,   0x47)
#define	KVM_SET_IDENTITY_MAP_ADDR _IOW(KVMIO,  0x48, uint64_t)

/* Device model IOC */
#define	KVM_CREATE_IRQCHIP	_IO(KVMIO,   0x60)
#define	KVM_IRQ_LINE		_IOW(KVMIO,  0x61, struct kvm_irq_level)
#define	KVM_GET_IRQCHIP		_IOWR(KVMIO, 0x62, struct kvm_irqchip)
#define	KVM_SET_IRQCHIP		_IOR(KVMIO,  0x63, struct kvm_irqchip)
#define	KVM_CREATE_PIT		_IO(KVMIO,   0x64)
#define	KVM_GET_PIT		_IOWR(KVMIO, 0x65, struct kvm_pit_state)
#define	KVM_SET_PIT		_IOR(KVMIO,  0x66, struct kvm_pit_state)
#define	KVM_IRQ_LINE_STATUS	_IOWR(KVMIO, 0x67, struct kvm_irq_level)

#define	KVM_REGISTER_COALESCED_MMIO _IOW(KVMIO,  0x67, \
					    struct kvm_coalesced_mmio_zone)
#define	KVM_UNREGISTER_COALESCED_MMIO _IOW(KVMIO,  0x68, \
					    struct kvm_coalesced_mmio_zone)
#define	KVM_SET_GSI_ROUTING	_IOW(KVMIO,  0x6a, struct kvm_irq_routing)
#define	KVM_REINJECT_CONTROL	_IO(KVMIO,   0x71)
#define	KVM_CREATE_PIT2		_IOW(KVMIO,  0x77, struct kvm_pit_config)
#define	KVM_SET_BOOT_CPU_ID	_IO(KVMIO,   0x78)
#define	KVM_SET_CLOCK		_IOW(KVMIO,  0x7b, struct kvm_clock_data)
#define	KVM_GET_CLOCK		_IOR(KVMIO,  0x7c, struct kvm_clock_data)
/* Available with KVM_CAP_PIT_STATE2 */
#define	KVM_GET_PIT2		_IOR(KVMIO,  0x9f, struct kvm_pit_state2)
#define	KVM_SET_PIT2		_IOW(KVMIO,  0xa0, struct kvm_pit_state2)

/*
 * ioctls for vcpu fds
 */
#define	KVM_RUN			_IO(KVMIO,   0x80)
#define	KVM_GET_REGS		_IOR(KVMIO,  0x81, struct kvm_regs)
#define	KVM_SET_REGS		_IOW(KVMIO,  0x82, struct kvm_regs)
#define	KVM_GET_SREGS		_IOR(KVMIO,  0x83, struct kvm_sregs)
#define	KVM_SET_SREGS		_IOW(KVMIO,  0x84, struct kvm_sregs)
#define	KVM_INTERRUPT		_IOW(KVMIO,  0x86, struct kvm_interrupt)
#define	KVM_GET_MSRS		_IOWR(KVMIO, 0x88, struct kvm_msrs)
#define	KVM_SET_MSRS		_IOW(KVMIO,  0x89, struct kvm_msrs)
#define	KVM_SET_CPUID		_IOW(KVMIO,  0x8a, struct kvm_cpuid)
#define	KVM_SET_SIGNAL_MASK	_IOW(KVMIO,  0x8b, struct kvm_signal_mask)
#define	KVM_GET_FPU		_IOR(KVMIO,  0x8c, struct kvm_fpu)
#define	KVM_SET_FPU		_IOW(KVMIO,  0x8d, struct kvm_fpu)
#define	KVM_GET_LAPIC		_IOR(KVMIO,  0x8e, struct kvm_lapic_state)
#define	KVM_SET_LAPIC		_IOW(KVMIO,  0x8f, struct kvm_lapic_state)
#define	KVM_SET_CPUID2		_IOW(KVMIO,  0x90, struct kvm_cpuid2)
#define	KVM_GET_CPUID2		_IOWR(KVMIO, 0x91, struct kvm_cpuid2)
/* Available with KVM_CAP_VAPIC */
#define	KVM_TPR_ACCESS_REPORTING _IOWR(KVMIO, 0x92, struct kvm_tpr_access_ctl)
/* Available with KVM_CAP_VAPIC */
#define	KVM_SET_VAPIC_ADDR	_IOW(KVMIO,  0x93, struct kvm_vapic_addr)
#define	KVM_GET_MP_STATE	_IOR(KVMIO,  0x98, struct kvm_mp_state)
#define	KVM_SET_MP_STATE	_IOW(KVMIO,  0x99, struct kvm_mp_state)
/* Available with KVM_CAP_NMI */
#define	KVM_NMI			_IO(KVMIO,   0x9a)
/* MCE for x86 */
#define	KVM_X86_SETUP_MCE	_IOW(KVMIO,  0x9c, uint64_t)
#define	KVM_X86_GET_MCE_CAP_SUPPORTED _IOR(KVMIO,  0x9d, uint64_t)
#define	KVM_X86_SET_MCE		_IOW(KVMIO,  0x9e, struct kvm_x86_mce)
/* Available with KVM_CAP_VCPU_EVENTS */
#define	KVM_GET_VCPU_EVENTS	_IOR(KVMIO,  0x9f, struct kvm_vcpu_events)
#define	KVM_SET_VCPU_EVENTS	_IOW(KVMIO,  0xa0, struct kvm_vcpu_events)

#endif /* __KVM_H */
