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

/*
 *  Emulate vm86() using KVM
 *  Started 2015, Bart Oldeman
 *  References: http://lwn.net/Articles/658511/
 *  plus example at http://lwn.net/Articles/658512/
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#include "kvm.h"
#include "emu.h"
#include "emu-ldt.h"
#include "vgaemu.h"
#include "mapping.h"
#include "sig.h"

#ifndef X86_EFLAGS_FIXED
#define X86_EFLAGS_FIXED 2
#endif

#define SAFE_MASK (X86_EFLAGS_CF|X86_EFLAGS_PF| \
                   X86_EFLAGS_AF|X86_EFLAGS_ZF|X86_EFLAGS_SF| \
                   X86_EFLAGS_TF|X86_EFLAGS_DF|X86_EFLAGS_OF| \
                   X86_EFLAGS_NT|X86_EFLAGS_AC|X86_EFLAGS_ID) // 0x244dd5
#define RETURN_MASK (SAFE_MASK | 0x28 | X86_EFLAGS_FIXED) // 0x244dff

extern char kvm_mon_start[];
extern char kvm_mon_hlt[];
extern char kvm_mon_end[];

/* V86 monitor structure to run code in V86 mode with VME enabled inside KVM
   This contains:
   1. a TSS with
     a. ss0:esp0 set to a stack at the top of the monitor structure
        This stack contains a copy of the vm86_regs struct.
     b. An interrupt redirect bitmap copied from info->int_revectored
     c. I/O bitmap, for now set to trap all ints. Todo: sync with ioperm()
   2. A GDT with 3 entries
     a. 0 entry
     b. selector 8: flat CS
     c. selector 0x10: flat SS
   3. An IDT with 33 (0x21) entries:
     a. 0x20 entries for all CPU exceptions
     b. a special entry at index 0x20 to interrupt the VM
   4. The stack (from 1a) above
   5. The code pointed to by the IDT entries, from kvmmon.S, on a new page
      This just pushes the exception number, error code, and all registers
      to the stack and executes the HLT instruction which is then trapped
      by KVM.
 */

#define TSS_IOPB_SIZE (65536 / 8)
#define GDT_ENTRIES 3
#undef IDT_ENTRIES
#define IDT_ENTRIES 0x21

#define PG_PRESENT 1
#define PG_RW 2
#define PG_USER 4

static struct monitor {
    Task tss;                                /* 0000 */
    /* tss.esp0                                 0004 */
    /* tss.ss0                                  0008 */
    /* tss.IOmapbaseT (word)                    0066 */
    struct revectored_struct int_revectored; /* 0068 */
    unsigned char io_bitmap[TSS_IOPB_SIZE+1];/* 0088 */
    /* TSS last byte (limit)                    2088 */
    unsigned char padding0[0x2100-sizeof(Task)
		-sizeof(struct revectored_struct)
		-(TSS_IOPB_SIZE+1)];
    Descriptor gdt[GDT_ENTRIES];             /* 2100 */
    unsigned char padding1[0x2200-0x2100
	-GDT_ENTRIES*sizeof(Descriptor)];    /* 2118 */
    Gatedesc idt[IDT_ENTRIES];               /* 2200 */
    unsigned char padding2[0x3000-0x2200
	-IDT_ENTRIES*sizeof(Gatedesc)
	-sizeof(unsigned int)
	-sizeof(struct vm86_regs)];          /* 2308 */
    unsigned int cr2;         /* Fault stack at 2FA8 */
    struct vm86_regs regs;
    /* 3000: page directory, 4000: page table */
    unsigned int pde[PAGE_SIZE/sizeof(unsigned int)];
    unsigned int pte[PAGE_SIZE/sizeof(unsigned int)];
    unsigned char code[PAGE_SIZE];           /* 5000 */
    /* 5000 IDT exception 0 code start
       500A IDT exception 1 code start
       .... ....
       5140 IDT exception 0x21 code start
       514A IDT common code start
       5164 IDT common code end
    */
} *monitor;

static struct kvm_run *run;
static int vmfd, vcpufd;

#define MAXSLOT 40
static struct kvm_userspace_memory_region maps[MAXSLOT];

/* initialize KVM virtual machine monitor */
void init_kvm_monitor(void)
{
  int ret, i, j;
  struct kvm_regs regs;
  struct kvm_sregs sregs;

  /* create monitor structure in memory */
  monitor = mmap(NULL, sizeof(*monitor), PROT_READ | PROT_WRITE,
		 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  /* Map guest memory: TSS */
  mmap_kvm(LOWMEM_SIZE + HMASIZE, monitor, sizeof(*monitor));

  memset(monitor, 0, sizeof(*monitor));
  /* trap all I/O instructions with GPF */
  memset(monitor->io_bitmap, 0xff, TSS_IOPB_SIZE+1);

  ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
  if (ret == -1) {
    perror("KVM: KVM_GET_SREGS");
    leavedos(99);
  }

  sregs.tr.base = LOWMEM_SIZE + HMASIZE;
  sregs.tr.limit = offsetof(struct monitor, io_bitmap) + TSS_IOPB_SIZE;
  sregs.tr.unusable = 0;
  sregs.tr.type = 0xb;
  sregs.tr.s = 0;
  sregs.tr.dpl = 0;
  sregs.tr.present = 1;
  sregs.tr.avl = 0;
  sregs.tr.l = 0;
  sregs.tr.db = 0;
  sregs.tr.g = 0;

  monitor->tss.esp0 = sregs.tr.base + offsetof(struct monitor, regs) +
    sizeof(monitor->regs);
  monitor->tss.ss0 = 0x10;
  monitor->tss.IOmapbase = offsetof(struct monitor, io_bitmap);

  // setup GDT
  sregs.gdt.base = sregs.tr.base + offsetof(struct monitor, gdt);
  sregs.gdt.limit = GDT_ENTRIES * sizeof(Descriptor) - 1;
  for (i=1; i<GDT_ENTRIES; i++) {
    monitor->gdt[i].limit_lo = 0xffff;
    monitor->gdt[i].type = 0xa;
    monitor->gdt[i].S = 1;
    monitor->gdt[i].present = 1;
    monitor->gdt[i].limit_hi = 0xf;
    monitor->gdt[i].DB = 1;
    monitor->gdt[i].gran = 1;
  }
  // flat data selector (0x10)
  monitor->gdt[GDT_ENTRIES-1].type = 2;

  sregs.idt.base = sregs.tr.base + offsetof(struct monitor, idt);
  sregs.idt.limit = IDT_ENTRIES * sizeof(Gatedesc)-1;
  // setup IDT
  for (i=0; i<IDT_ENTRIES; i++) {
    unsigned int offs = sregs.tr.base + offsetof(struct monitor, code) + i * 10;
    monitor->idt[i].offs_lo = offs & 0xffff;
    monitor->idt[i].offs_hi = offs >> 16;
    monitor->idt[i].seg = 0x8; // FLAT_CODE_SEL
    monitor->idt[i].type = 0xe;
    monitor->idt[i].DPL = 3;
    monitor->idt[i].present = 1;
  }
  memcpy(monitor->code, kvm_mon_start, kvm_mon_end - kvm_mon_start);

  /* setup paging */
  sregs.cr3 = sregs.tr.base + offsetof(struct monitor, pde);
  /* we need one page directory entry */
  monitor->pde[0] = (sregs.tr.base + offsetof(struct monitor, pte))
    | (PG_PRESENT | PG_RW | PG_USER);
  for (i = 0; i < (LOWMEM_SIZE + HMASIZE) / PAGE_SIZE; i++)
    monitor->pte[i] = i * PAGE_SIZE | (PG_PRESENT | PG_RW | PG_USER);
  for (j = 0; j < offsetof(struct monitor, code) / PAGE_SIZE; j++)
    monitor->pte[i+j] = (i+j) * PAGE_SIZE | PG_PRESENT | PG_RW;
  monitor->pte[i+j] = ((i+j) * PAGE_SIZE) | PG_PRESENT;

  sregs.cr0 |= X86_CR0_PE | X86_CR0_PG;
  sregs.cr4 |= X86_CR4_VME;

  /* setup registers to point to VM86 monitor */
  sregs.cs.base = 0;
  sregs.cs.limit = 0xffffffff;
  sregs.cs.selector = 0x8;
  sregs.cs.db = 1;
  sregs.cs.g = 1;

  sregs.ss.base = 0;
  sregs.ss.limit = 0xffffffff;
  sregs.ss.selector = 0x10;
  sregs.ss.db = 1;
  sregs.ss.g = 1;

  ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
  if (ret == -1) {
    perror("KVM: KVM_SET_SREGS");
    leavedos(99);
  }

  /* just after the HLT */
  regs.rip = sregs.tr.base + offsetof(struct monitor, code) +
    (kvm_mon_hlt - kvm_mon_start) + 1;
  regs.rsp = sregs.tr.base + offsetof(struct monitor, cr2);
  regs.rflags = X86_EFLAGS_FIXED;
  ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
  if (ret == -1) {
    perror("KVM: KVM_SET_REGS");
    leavedos(99);
  }

  warn("Using V86 mode inside KVM\n");
}

/* Initialize KVM and memory mappings */
int init_kvm_cpu(void)
{
  struct kvm_cpuid *cpuid;
  int kvm, ret, mmap_size;

  kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
  if (kvm == -1) {
    warn("KVM: error opening /dev/kvm: %s\n", strerror(errno));
    return 0;
  }

  vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long)0);
  if (vmfd == -1) {
    warn("KVM: KVM_CREATE_VM: %s\n", strerror(errno));
    return 0;
  }

  /* this call is only there to shut up the kernel saying
     "KVM_SET_TSS_ADDR need to be called before entering vcpu"
     this is only really needed if the vcpu is started in real mode and
     the kernel needs to emulate that using V86 mode, as is necessary
     on Nehalem and earlier Intel CPUs */
  ret = ioctl(vmfd, KVM_SET_TSS_ADDR,
	      (unsigned long)(LOWMEM_SIZE + HMASIZE + sizeof(struct monitor)));
  if (ret == -1) {
    warn("KVM: KVM_SET_TSS_ADDR: %s\n", strerror(errno));
    return 0;
  }

  vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
  if (vcpufd == -1) {
    warn("KVM: KVM_CREATE_VCPU: %s\n", strerror(errno));
    return 0;
  }

  cpuid = malloc(sizeof(*cpuid) + 2*sizeof(cpuid->entries[0]));
  cpuid->nent = 2;
  // Use the same values as in emu-i386/simx86/interp.c
  // (Pentium 133-200MHz, "GenuineIntel")
  cpuid->entries[0] = (struct kvm_cpuid_entry) { .function = 0,
    .eax = 1, .ebx = 0x756e6547, .ecx = 0x6c65746e, .edx = 0x49656e69 };
  // family 5, model 2, stepping 12, fpu vme de pse tsc msr mce cx8
  cpuid->entries[1] = (struct kvm_cpuid_entry) { .function = 1,
    .eax = 0x052c, .ebx = 0, .ecx = 0, .edx = 0x1bf };
  ret = ioctl(vcpufd, KVM_SET_CPUID, cpuid);
  free(cpuid);
  if (ret == -1) {
    warn("KVM: KVM_SET_CPUID: %s\n", strerror(errno));
    return 0;
  }

  mmap_size = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
  if (mmap_size == -1) {
    warn("KVM: KVM_GET_VCPU_MMAP_SIZE: %s\n", strerror(errno));
    return 0;
  }
  if (mmap_size < sizeof(*run)) {
    warn("KVM: KVM_GET_VCPU_MMAP_SIZE unexpectedly small\n");
    return 0;
  }
  run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
  if (run == MAP_FAILED) {
    warn("KVM: mmap vcpu: %s\n", strerror(errno));
    return 0;
  }

  return 1;
}

static void set_kvm_memory_region(struct kvm_userspace_memory_region *region)
{
  int ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, region);
  if (ret == -1) {
    perror("KVM: KVM_SET_USER_MEMORY_REGION");
    leavedos(99);
  }
}

static void mmap_kvm_no_overlap(unsigned targ, void *addr, size_t mapsize)
{
  struct kvm_userspace_memory_region *region;
  int slot;

  for (slot = 0; slot < MAXSLOT; slot++)
    if (maps[slot].memory_size == 0) break;

  if (slot == MAXSLOT) {
    perror("KVM: insufficient number of memory slots MAXSLOT");
    leavedos(99);
  }

  region = &maps[slot];
  region->slot = slot;
  region->guest_phys_addr = targ;
  region->userspace_addr = (uintptr_t)addr;
  region->memory_size = mapsize;
  set_kvm_memory_region(region);
}

static void munmap_kvm(unsigned targ, size_t mapsize)
{
  /* unmaps KVM regions from targ to targ+mapsize, taking care of overlaps */
  int slot;
  for (slot = 0; slot < MAXSLOT; slot++) {
    struct kvm_userspace_memory_region *region = &maps[slot];
    size_t sz = region->memory_size;
    unsigned gpa = region->guest_phys_addr;
    if (sz > 0 && targ + mapsize > gpa && targ < gpa + sz) {
      /* overlap: first  unmap this mapping */
      region->memory_size = 0;
      set_kvm_memory_region(region);
      /* may need to remap head or tail */
      if (gpa < targ) {
	region->memory_size = targ - gpa;
	set_kvm_memory_region(region);
      }
      if (gpa + sz > targ + mapsize) {
	mmap_kvm_no_overlap(targ + mapsize,
			    (void *)(region->userspace_addr +
				     targ + mapsize - gpa),
			    gpa + sz - (targ + mapsize));
      }
    }
  }
}

void mmap_kvm(unsigned targ, void *addr, size_t mapsize)
{
  if (targ >= LOWMEM_SIZE + HMASIZE && addr != monitor) return;
  /* with KVM we need to manually remove/shrink existing mappings */
  munmap_kvm(targ, mapsize);
  mmap_kvm_no_overlap(targ, addr, mapsize);
}

void mprotect_kvm(void *addr, size_t mapsize, int protect)
{
  size_t pagesize = sysconf(_SC_PAGESIZE);
  unsigned int start = DOSADDR_REL(addr) / pagesize;
  unsigned int end = start + mapsize / pagesize;
  unsigned int limit = (LOWMEM_SIZE + HMASIZE) / pagesize;
  unsigned int page;

  if (start >= limit || monitor == NULL) return;
  if (end > limit) end = limit;

  for (page = start; page < end; page++) {
    monitor->pte[page] &= ~(PG_PRESENT | PG_RW | PG_USER);
    if (protect & PROT_WRITE)
      monitor->pte[page] |= PG_PRESENT | PG_RW | PG_USER;
    else if (protect & PROT_READ)
      monitor->pte[page] |= PG_PRESENT | PG_USER;
  }
}

/* This function works like handle_vm86_fault in the Linux kernel,
   except:
   * since we use VME we only need to handle
     PUSHFD, POPFD, IRETD always
     POPF, IRET only if it sets TF or IF with VIP set
     STI only if VIP is set and VIF was not set
     INT only if it is revectored
   * The Linux kernel splits the CPU flags into on-CPU flags and
     flags (VFLAGS) IOPL, NT, AC, and ID that are kept on the stack.
     Here all those flags are merged into on-CPU flags, with the
     exception of IOPL. IOPL is always set to 0 on the CPU,
     and to 3 on the stack with PUSHF
*/
static int kvm_handle_vm86_fault(struct vm86_regs *regs, unsigned int cpu_type)
{
  unsigned char opcode;
  int data32 = 0, pref_done = 0;
  unsigned int csp = regs->cs << 4;
  unsigned int ssp = regs->ss << 4;
  unsigned short ip = regs->eip & 0xffff;
  unsigned short sp = regs->esp & 0xffff;
  unsigned int orig_flags = regs->eflags;
  int ret = -1;

  do {
    switch (opcode = popb(csp, ip)) {
    case 0x66:      /* 32-bit data */     data32 = 1; break;
    case 0x67:      /* 32-bit address */  break;
    case 0x2e:      /* CS */              break;
    case 0x3e:      /* DS */              break;
    case 0x26:      /* ES */              break;
    case 0x36:      /* SS */              break;
    case 0x65:      /* GS */              break;
    case 0x64:      /* FS */              break;
    case 0xf2:      /* repnz */           break;
    case 0xf3:      /* rep */             break;
    default: pref_done = 1;
    }
  } while (!pref_done);

  switch (opcode) {

  case 0x9c: { /* only pushfd faults with VME */
    unsigned int flags = regs->eflags & RETURN_MASK;
    if (regs->eflags & X86_EFLAGS_VIF)
      flags |= X86_EFLAGS_IF;
    flags |= X86_EFLAGS_IOPL;
    pushl(ssp, sp, flags);
    break;
  }

  case 0xcd: { /* int xx */
    int intno = popb(csp, ip);
    ret = VM86_INTx + (intno << 8);
    break;
  }

  case 0xcf: /* iret */
    if (data32) {
      ip = popl(ssp, sp);
      regs->cs = popl(ssp, sp);
    } else {
      ip = popw(ssp, sp);
      regs->cs = popw(ssp, sp);
    }
    /* fall through into popf */
  case 0x9d: { /* popf */
    unsigned int newflags;
    if (data32) {
      newflags = popl(ssp, sp);
      if (cpu_type >= CPU_286 && cpu_type <= CPU_486) {
	newflags &= ~X86_EFLAGS_ID;
	if (cpu_type < CPU_486)
	  newflags &= ~X86_EFLAGS_AC;
      }
      regs->eflags &= ~SAFE_MASK;
    } else {
      /* must have VIP or TF set in VME, otherwise does not trap */
      newflags = popw(ssp, sp);
      regs->eflags &= ~(SAFE_MASK & 0xffff);
    }
    regs->eflags |= newflags & SAFE_MASK;
    if (newflags & X86_EFLAGS_IF) {
      regs->eflags |= X86_EFLAGS_VIF;
      if (orig_flags & X86_EFLAGS_VIP) ret = VM86_STI;
    } else {
      regs->eflags &= ~X86_EFLAGS_VIF;
    }
    break;
  }

  case 0xfb: /* STI */
    /* must have VIP set in VME, otherwise does not trap */
    regs->eflags |= X86_EFLAGS_VIF;
    ret = VM86_STI;
    break;

  default:
    return VM86_UNKNOWN;
  }

  regs->esp = (regs->esp & 0xffff0000) | sp;
  regs->eip = (regs->eip & 0xffff0000) | ip;
  if (ret != -1)
    return ret;
  if (orig_flags & X86_EFLAGS_TF)
    return VM86_TRAP + (1 << 8);
  return ret;
}

/* Emulate vm86() using KVM */
int kvm_vm86(struct vm86_struct *info)
{
  struct vm86_regs *regs;
  int ret, vm86_ret;
  unsigned int exit_reason, trapno;

  regs = &monitor->regs;
  *regs = info->regs;
  monitor->int_revectored = info->int_revectored;

  regs->eflags &= (SAFE_MASK | X86_EFLAGS_VIF | X86_EFLAGS_VIP);
  regs->eflags |= X86_EFLAGS_FIXED | X86_EFLAGS_VM | X86_EFLAGS_IF;

  do {
    vm86_ret = -1;
    ret = ioctl(vcpufd, KVM_RUN, NULL);

    /* KVM should only exit for three reasons:
       1. KVM_EXIT_HLT: at the hlt in kvmmon.S.
       2. KVM_EXIT_INTR: (with ret==-1) after a signal. In this case we
          re-enter KVM with int 0x20 injected (if possible) so it will fault
          with KMV_EXIT_HLT.
       3. KVM_EXIT_IRQ_WINDOW_OPEN: if it is not possible to inject interrupts
          KVM is re-entered asking it to exit when interrupt injection is
          possible, then it exits with this code. This only happens if a signal
          occurs during execution of the monitor code in kvmmon.S.
    */
    exit_reason = run->exit_reason;
    if (ret == -1 && errno == EINTR) {
      exit_reason = KVM_EXIT_INTR;
    } else if (ret != 0) {
      perror("KVM: KVM_RUN");
      leavedos_main(99);
    }

    switch (exit_reason) {
    case KVM_EXIT_HLT: {
      /* __null_gs = exception number */
      /* orig_eax = error code */
      trapno = regs->__null_gs;
      vm86_ret = VM86_SIGNAL;
      if (trapno == 1 || trapno == 3)
	vm86_ret = VM86_TRAP | (trapno << 8);
      else if (trapno == 0xd)
	vm86_ret = kvm_handle_vm86_fault(regs, info->cpu_type);
      break;
    }
    case KVM_EXIT_IRQ_WINDOW_OPEN:
    case KVM_EXIT_INTR:
      run->request_interrupt_window = !run->ready_for_interrupt_injection;
      if (run->ready_for_interrupt_injection) {
	struct kvm_interrupt interrupt = (struct kvm_interrupt){.irq = 0x20};
	ret = ioctl(vcpufd, KVM_INTERRUPT, &interrupt);
	if (ret == -1) {
	  perror("KVM: KVM_INTERRUPT");
	  leavedos(99);
	}
      }
      break;
    case KVM_EXIT_FAIL_ENTRY:
      fprintf(stderr,
	      "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx\n",
	      (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
      leavedos(99);
    case KVM_EXIT_INTERNAL_ERROR:
      fprintf(stderr,
	      "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x\n", run->internal.suberror);
      leavedos(99);
    default:
      fprintf(stderr, "KVM: exit_reason = 0x%x\n", exit_reason);
      leavedos(99);
    }

  } while (vm86_ret == -1);

  info->regs = *regs;
  trapno = regs->__null_gs;
  if (vm86_ret == VM86_SIGNAL && trapno != 0x20) {
    struct sigcontext sc, *scp = &sc;
    _cr2 = (uintptr_t)MEM_BASE32(monitor->cr2);
    _trapno = trapno;
    _err = regs->orig_eax;
    if (_trapno == 0x0e && VGA_EMU_FAULT(scp, code, 0) == True)
      return vm86_ret;
    vm86_fault(scp);
  }
  return vm86_ret;
}
