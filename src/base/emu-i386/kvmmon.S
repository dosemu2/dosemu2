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

        .globl kvm_mon_vcpi_pm_jmp      # largely based on Jemm's code
kvm_mon_vcpi_pm_jmp:
        movw $0x28,%ax                  # set 0-based %ds
        mov %eax,%ds
        movw $0x30,%ax                  # some poorly written VCPI clients expect
        mov %eax,%ss                    # hiword(esp) to be cleared.

        sgdt %ss:(0x80)
        sidt %ss:(0x86)
        mov  %cr3,%eax
        mov  %eax,%ss:(0x8c)

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
        movl $0x80,%esp                 # TOS for new stack
        ljmp *0x10(%esi)                # jump to client's entry point

        .globl kvm_mon_vcpi_pmi
kvm_mon_vcpi_pmi:
        cmpw $0xde0c,%ax
        je pm_to_v86
        cmpw $0xde03,%ax
        je vcpi_de03
        cmpw $0xde04,%ax
        je vcpi_de04
        cmpw $0xde05,%ax
        je vcpi_de05
        mov $0x8f,%ah # unsupported
        lret

vcpi_de03: # Get Number of Free 4K Pages (0: let VCPI clients use XMS instead)
        xorb %ah,%ah
        xorl %edx,%edx
        lret
vcpi_de04: # Allocate a 4K page
        movb $0x88,%ah
        movl $0xffffffff,%edx
        lret
vcpi_de05: # Free a 4K page
        movb $0x8a,%ah
        movl $0xffffffff,%edx
        lret

pm_to_v86:
        cli
        mov %cs,%eax
        addl $8,%eax
        mov %eax,%ds
        mov (0x11108c),%eax # v86cr3
        addl $5*4,%esp
        pushl $0x20202                  # flags: VM=1, NT=0, IOPL=0, IF=1, TF=0, VIF=0
        subl $2*4,%esp
        mov %eax,%cr3
        lgdt (0x111080) #gdt_ptr
        movl (0x111082),%eax            # EAX == linear address of server's GDT
        andb $~2,(0x18+5)(%eax)         # clear task busy bit in TSS descriptor
        lidt (0x111086) #idt_ptr
        mov $0x20,%ax
        lldt %ax
        mov $0x18,%al #tss_sel
        clts
        ltr %ax
        iret

	.globl kvm_mon_end
kvm_mon_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
