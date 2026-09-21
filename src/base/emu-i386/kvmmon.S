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
 *  IDT handlers for the V86 monitor using KVM
 *  These just push the exception number and state, and then execute
 *  a common HLT which is trapped by KVM.
 */

	.section .text
	.code32

	.global _start
_start:  // dummy
	.globl kvm_mon_start
kvm_mon_start:

        i = 0
        .rept 0x100
        .if i == 8 || (i >= 0xa && i <= 0xe)
        /* these exceptions already pushed an error code
           (exception 0x11 (alignment) and 0x1e (security) cannot occur) */
        nop
        nop
        .else
        /* push fake error code for consistent stack */
        pushl $0
        .endif
        movw $i,2(%esp)
        jmp kvm_mon_main
        i = i + 1
        .fill kvm_mon_start+32*i-., 1, 0x90
        .endr

kvm_mon_main:
        push %gs
        push %fs
        push %es
        push %ds
        push %eax
        push %ebp
        push %edi
        push %esi
        push %edx
        push %ecx
        push %ebx
        /* cr3: mark as 0 to not reload by default */
        pushl $0
        mov %cr2,%eax
        push %eax
#if 0
        subl $0x200,%esp
        fxsave (%esp)
#endif
	.globl kvm_mon_hlt
kvm_mon_hlt:
        hlt
#if 0
        fxrstor (%esp)
        addl $0x200,%esp
#endif
        pop %eax
        pop %eax
        or %eax,%eax
        jz skip_cr3
        /* flush TLB by reloading cr3 if nonzero (set by mprotect_kvm()) */
        mov %eax,%cr3
skip_cr3:
        pop %ebx
        pop %ecx
        pop %edx
        pop %esi
        pop %edi
        pop %ebp
        pop %eax
        pop %ds
        pop %es
        pop %fs
        pop %gs
        add $0x4,%esp
        iret

/* The VCPI half, largely Jemm's code, from Bart Oldeman's PR #1880.
   It runs from the page a client maps at VCPI_CODE, with its scratch
   data (saved GDTR/IDTR/CR3 and a stack) on the page above. */
        VCPI_DATA = 0x111000
        SAVED_GDTR = 0x80
        SAVED_IDTR = 0x86
        SAVED_CR3 = 0x8c
        /* nonzero while a client owns the CPU; kvm_vcpi_pm_switch() sets
           it, the way back clears it.  Inferring this from monitor->regs
           does not work: the way back does not go through them. */
        VCPI_ACTIVE = 0x90
        MON_TSS_SEL = 0x18
        MON_LDT_SEL = 0x20
        VCPI_DS_SEL = 0x28
        VCPI_SS_SEL = 0x30

        .globl kvm_mon_vcpi_pm_jmp
kvm_mon_vcpi_pm_jmp:
        movw $VCPI_DS_SEL,%ax           # 0-based %ds
        mov %eax,%ds
        movw $VCPI_SS_SEL,%ax           # some poorly written VCPI clients
        mov %eax,%ss                    # expect hiword(esp) to be cleared

        sgdt %ss:(SAVED_GDTR)
        sidt %ss:(SAVED_IDTR)
        mov  %cr3,%eax
        mov  %eax,%ss:(SAVED_CR3)

        movl (%esi),%eax                # set client's context *first*
        movl %eax,%cr3
        movl 0x08(%esi),%eax            # set up client's IDT
        lidt (%eax)
        movl 0x04(%esi),%eax            # set up client's GDT
        movzwl 0x0e(%esi),%esp          # client's TSS selector
        lgdt (%eax)
        movl 2(%eax),%eax               # EAX == linear address of client's GDT
        andb $~2,%ds:5(%esp,%eax)       # clear task busy bit in TSS descriptor
        ltr %sp                         # set up client's TSS
        lldt 0x0c(%esi)                 # set up client's LDT
        movl $SAVED_GDTR,%esp           # TOS for new stack
        ljmp *0x10(%esi)                # jump to client's entry point

        .globl kvm_mon_vcpi_pmi
kvm_mon_vcpi_pmi:
        cmpw $0xde0c,%ax
        je pm_to_v86
        cmpw $0xde03,%ax
        jb vcpi_unsupported
        cmpw $0xde05,%ax
        ja vcpi_unsupported
	.globl kvm_mon_vcpi_hlt
kvm_mon_vcpi_hlt:
        hlt
        lret

vcpi_unsupported:
        mov $0x8f,%ah                   # unsupported
        lret

pm_to_v86:
        cli
        mov %cs,%eax
        addl $8,%eax                    # our 0-based DS follows our CS
        mov %eax,%ds
        movb $0,(VCPI_DATA+VCPI_ACTIVE) # the client is done with the CPU
        mov (VCPI_DATA+SAVED_CR3),%eax
        addl $5*4,%esp
        pushl $0x20202                  # VM=1, NT=0, IOPL=0, IF=1, TF=0, VIF=0
        subl $2*4,%esp
        mov %eax,%cr3
        lgdt (VCPI_DATA+SAVED_GDTR)
        movl (VCPI_DATA+SAVED_GDTR+2),%eax  # linear address of our own GDT
        andb $~2,(MON_TSS_SEL+5)(%eax)  # clear task busy bit in TSS descriptor
        lidt (VCPI_DATA+SAVED_IDTR)
        mov $MON_LDT_SEL,%ax
        lldt %ax
        mov $MON_TSS_SEL,%al
        clts
        ltr %ax
        iret

	.globl kvm_mon_end
kvm_mon_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
