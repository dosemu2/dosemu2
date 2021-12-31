/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include "emu86.h"
#include "trees.h"
#include "codegen-arch.h"
#include "cpatch.h"

#ifdef HOST_ARCH_X86

static int in_cpatch;

/*
 * Return address of the stub function is passed into eip
 */
void m_munprotect(unsigned int addr, unsigned int len, unsigned char *eip)
{
	if (debug_level('e')>1) {
		if (debug_level('e')>3)
			e_printf("\tM_MUNPROT %08x:%p\n", addr,eip);
		if (e_querymark(addr, len))
			e_printf("CODE %08x hit in DATA %p patch\n",addr,eip);
	}
	/* if only data in aliased low memory is hit, nothing to do */
	if (LINEAR2UNIX(addr) != MEM_BASE32(addr)) {
		if (e_querymark(addr, len))
			// no need to invalidate the whole page here,
			// as the page does not need to be unprotected
			InvalidateNodeRange(addr,len,eip);
		return;
	}
	/* Always unprotect and clear all code in the pages
	 * for DPMI data and code
	 * Maybe the stub was set up before that code was parsed.
	 * Clear that code */
/*	if (UnCpatch((void *)(eip-3))) leavedos_main(0); */
	len = PAGE_ALIGN(addr+len-1) - (addr & PAGE_MASK);
	addr &= PAGE_MASK;
	InvalidateNodeRange(addr,len,eip);
}

#define repmovs(std,letter,cld)			       \
	asm volatile(#std" ; rep ; movs"#letter ";" #cld"\n\t" \
		     : "=&c" (ecx), "=&D" (edi), "=&S" (esi)   \
		     : "0" (ecx), "1" (edi), "2" (esi) \
		     : "memory")

#define repstos(std,letter,cld)			       \
	asm volatile(#std" ; rep ; stos"#letter ";" #cld"\n\t" \
		     : "=&c" (ecx), "=&D" (edi) \
		     : "a" (eax), "0" (ecx), "1" (edi) \
		     : "memory")

struct rep_stack {
	unsigned char *esi, *edi;
	unsigned long ecx, eflags, edx, eax;
#ifdef __x86_64__
	unsigned long eax_pad;
#endif
	unsigned char *eip;
} __attribute__((packed));


asmlinkage void rep_movs_stos(struct rep_stack *stack)
{
	unsigned char *paddr = stack->edi;
	unsigned int ecx = stack->ecx;
	unsigned char *eip = stack->eip;
	dosaddr_t addr;
	unsigned int len = ecx;
	unsigned char *edi;
	unsigned char op;
	unsigned int size;

	in_cpatch++;
	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	if (*eip == 0xf3) /* skip rep */
		eip++;
	op = eip[0];
	size = 1;
	if (*eip == 0x66) {
		size = 2;
		op = eip[1];
	}
	else if (*eip & 1)
		size = 4;
	len *= size;
	m_munprotect(addr - ((EFLAGS & EFLAGS_DF) ? (len - size) : 0),
		     len, eip);
	edi = LINEAR2UNIX(addr);
	if ((op & 0xfe) == 0xa4) { /* movs */
		dosaddr_t source = DOSADDR_REL(stack->esi);
		unsigned char *esi;
		unsigned int v = vga_access(source, addr);
		if (v) {
			int df = ((EFLAGS & EFLAGS_DF) ? -size:size);
			e_VgaMovs(&stack->edi, &stack->esi, ecx, df, v);
			stack->ecx = 0;
			goto done;
		}
		esi = LINEAR2UNIX(source);
		if (ecx == len) {
			if (EFLAGS & EFLAGS_DF) repmovs(std,b,cld);
			else repmovs(,b,);
		}
		else if (ecx*2 == len) {
			if (EFLAGS & EFLAGS_DF) repmovs(std,w,cld);
			else repmovs(,w,);
		}
		else {
			if (EFLAGS & EFLAGS_DF) repmovs(std,l,cld);
			else repmovs(,l,);
		}
		if (EFLAGS & EFLAGS_DF) source -= len;
		else source += len;
		stack->esi = MEM_BASE32(source);
	}
	else if ((op & 0xfe) == 0xaa) { /* stos */
		unsigned int eax = stack->eax;
		if (ecx == len) {
			if (vga_write_access(addr)) {
				if (EFLAGS & EFLAGS_DF)
					vga_memset(addr - len + 1, eax, ecx);
				else
					vga_memset(addr, eax, ecx);
				ecx = 0;
			}
			else if (EFLAGS & EFLAGS_DF) repstos(std,b,cld);
			else repstos(,b,);
		}
		else if (ecx*2 == len) {
			if (vga_write_access(addr)) {
				if (EFLAGS & EFLAGS_DF)
					vga_memsetw(addr - len + 2, eax, ecx);
				else
					vga_memsetw(addr, eax, ecx);
				ecx = 0;
			}
			else if (EFLAGS & EFLAGS_DF) repstos(std,w,cld);
			else repstos(,w,);
		}
		else {
			if (vga_write_access(addr)) {
				if (EFLAGS & EFLAGS_DF)
					vga_memsetl(addr - len + 4, eax, ecx);
				else
					vga_memsetl(addr, eax, ecx);
				ecx = 0;
			}
			else if (EFLAGS & EFLAGS_DF) repstos(std,l,cld);
			else repstos(,l,);
		}
	}
	else if ((op & 0xf6) == 0xa6) { /* cmps/scas */
		int repmod = (size == 1 ? MBYTE : size == 2 ? DATA16 : 0);
		AR1.d = DOSADDR_REL(stack->edi);
		TR1.d = stack->ecx;
		repmod |= MOVSDST|MREPCOND|(eip[-1]==REPNE? MREPNE:MREP);
		if ((op & 0xf6) == 0xa6) { /* cmps */
			repmod |= MOVSSRC;
			AR2.d = DOSADDR_REL(stack->esi);
			Gen_sim(O_MOVS_CmpD, repmod);
			stack->esi = MEM_BASE32(AR2.d);
		}
		else { /* scas */
			DR1.d = stack->eax;
			Gen_sim(O_MOVS_ScaD, repmod);
		}
		FlagSync_All();
		stack->edi = MEM_BASE32(AR1.d);
		stack->ecx = TR1.d;
		stack->eflags = (stack->eflags & ~EFLAGS_CC) |
			(EFLAGS & EFLAGS_CC);
		goto done;
	}
	if (EFLAGS & EFLAGS_DF) addr -= len;
	else addr += len;
	stack->edi = MEM_BASE32(addr);
	stack->ecx = ecx;
done:
	InCompiledCode++;
	in_cpatch--;
}

/* ======================================================================= */

asmlinkage void stk_16(unsigned char *paddr, Bit16u value)
{
	dosaddr_t addr;

	in_cpatch++;
	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	e_invalidate(addr, 2);
	WRITE_WORD(addr, value);
	InCompiledCode++;
	in_cpatch--;
}

asmlinkage void stk_32(unsigned char *paddr, Bit32u value)
{
	dosaddr_t addr;

	in_cpatch++;
	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	e_invalidate(addr, 4);
	WRITE_DWORD(addr, value);
	InCompiledCode++;
	in_cpatch--;
}

asmlinkage void wri_8(unsigned char *paddr, Bit8u value, unsigned char *eip)
{
	dosaddr_t addr;

	in_cpatch++;
	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	m_munprotect(addr, 1, eip);
	InCompiledCode++;
	if (!emu_ldt_write(paddr, value, 1)) {
		if (vga_write_access(addr))
			vga_write(addr, value);
		else
			WRITE_BYTE(addr,value);
	}
	in_cpatch--;
}

asmlinkage void wri_16(unsigned char *paddr, Bit16u value, unsigned char *eip)
{
	dosaddr_t addr;

	in_cpatch++;
	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	m_munprotect(addr, 2, eip);
	InCompiledCode++;
	if (!emu_ldt_write(paddr, value, 2)) {
		if (vga_write_access(addr))
			vga_write_word(addr, value);
		else
			WRITE_WORD(addr,value);
	}
	in_cpatch--;
}

asmlinkage void wri_32(unsigned char *paddr, Bit32u value, unsigned char *eip)
{
	dosaddr_t addr;

	in_cpatch++;
	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	m_munprotect(addr, 4, eip);
	InCompiledCode++;
	if (!emu_ldt_write(paddr, value, 4)) {
		if (vga_write_access(addr))
			vga_write_dword(addr, value);
		else
			WRITE_DWORD(addr,value);
	}
	in_cpatch--;
}

asmlinkage Bit8u read_8(unsigned char *paddr)
{
	dosaddr_t addr = DOSADDR_REL(paddr);
	return vga_read_access(addr) ? vga_read(addr) : READ_BYTE(addr);
}

asmlinkage Bit16u read_16(unsigned char *paddr)
{
	dosaddr_t addr = DOSADDR_REL(paddr);
	return vga_read_access(addr) ? vga_read_word(addr) : READ_WORD(addr);
}

asmlinkage Bit32u read_32(unsigned char *paddr)
{
	dosaddr_t addr = DOSADDR_REL(paddr);
	return vga_read_access(addr) ? vga_read_dword(addr) : READ_DWORD(addr);
}

#ifdef __i386__

/*
 * stack on entry:
 *	esp+00	return address
 */

#define STUB_STK(cfunc) \
"		pushal\n \
		leal	(%esi,%ecx,1),%edi\n \
		pushl	%eax\n \
		pushl	%edi\n \
		call	"#cfunc"\n \
		addl	$8,%esp\n \
		popal\n \
		ret\n"


#define STUB_WRI(cfunc) \
"		pushl	(%esp)\n"	/* return addr = patch point+3 */ \
"		pushl	%eax\n"		/* value to write */ \
"		pushl	%edi\n"		/* addr where to write */ \
"		call	"#cfunc"\n" \
"		popl	%edi\n"		/* restore addr */ \
"		popl	%eax\n"		/* restore eax */ \
"		addl	$4,%esp\n"	/* remove parameters */ \
"		ret\n"

#define STUB_READ(cfunc) \
"		pushl	%edi\n"		/* addr where to read */ \
"		call	"#cfunc"\n" \
"		addl	$4,%esp\n"	/* remove parameters */ \
"		ret\n"

asm (
".text\n.globl stub_rep__\n"
"stub_rep__:	jecxz	1f\n"		/* zero move, nothing to do */
"		pushl	%eax\n"		/* save regs */
"		pushl	%edx\n"		/* edx used in 16bit overrun emulation, save too */
"		pushfl\n"		/* push flags for DF */
"		pushl	%ecx\n"		/* push count */
"		pushl	%edi\n"		/* push dest address */
"		pushl	%esi\n"		/* push source address */
"		pushl	%esp\n"		/* push stack */
"		cld\n"
"		call	rep_movs_stos\n"
"		addl	$4,%esp\n"	/* remove stack parameter */
"		popl	%esi\n"		/* obtain changed source address */
"		popl	%edi\n"		/* obtain changed dest address */
"		popl	%ecx\n"		/* obtain changed count */
"		popfl\n"		/* real CPU flags back */
"		popl	%edx\n"
"		popl	%eax\n"
"1:		ret\n"
);

/* ======================================================================= */

#else //__x86_64__

#define STUB_STK(cfunc) \
"		pushq	%rax\n"		/* save regs */ \
"		pushq	%rcx\n" \
"		pushq	%rdx\n" \
"		pushq	%rdi\n" \
"		pushq	%rsi\n" \
"		leal	(%rsi,%rcx,1),%edi\n" \
"		movl	%eax,%esi\n" \
					/* pass base address in %rdi */ \
"		call	"#cfunc"\n" \
"		popq	%rsi\n"		/* restore regs */ \
"		popq	%rdi\n" \
"		popq	%rdx\n" \
"		popq	%rcx\n" \
"		popq	%rax\n"	\
"		ret\n"


#define STUB_WRI(cfunc) \
"		movq	(%rsp),%rdx\n"	/* return addr = patch point+3 */ \
"		pushq	%rdi\n"		/* save regs */ \
"		pushq	%rsi\n"	\
"		pushq	%rax\n"	\
"		movl	%eax,%esi\n"	/* value to write */ \
					/* pass addr where to write in %rdi */\
"		call	"#cfunc"\n" \
"		popq	%rax\n"		/* restore regs */ \
"		popq	%rsi\n" \
"		popq	%rdi\n"	\
"		ret\n"

#define STUB_READ(cfunc) \
"		pushq	%rdi\n"		/* save regs */ \
"		call	"#cfunc"\n" \
"		popq	%rdi\n"		/* restore regs */ \
"		ret\n"

asm (
".text\n.globl stub_rep__\n"
"stub_rep__:	jrcxz	1f\n"		/* zero move, nothing to do */
"		pushq	%rax\n"		/* save regs */
"		pushq	%rax\n"		/* save rax twice for 16-alignment */
"		pushq	%rdx\n"
"		pushfq\n"		/* push flags for DF */
"		pushq	%rcx\n"
"		pushq	%rdi\n"
"		pushq	%rsi\n"
"		movq	%rsp,%rdi\n"	/* pass stack address in %rdi */
"		cld\n"
"		call	rep_movs_stos\n"
"		popq	%rsi\n"		/* restore regs */
"		popq	%rdi\n"
"		popq	%rcx\n"
"		popfq\n"		/* real CPU flags back */
"		popq	%rdx\n"
"		popq	%rax\n"
"		popq	%rax\n"
"1:		ret\n"
);

#endif

asm (
		".text\n"
"stub_stk_16__:.globl stub_stk_16__\n"STUB_STK(stk_16)
"stub_stk_32__:.globl stub_stk_32__\n"STUB_STK(stk_32)
"stub_wri_8__: .globl stub_wri_8__\n "STUB_WRI(wri_8)
"stub_wri_16__:.globl stub_wri_16__\n"STUB_WRI(wri_16)
"stub_wri_32__:.globl stub_wri_32__\n"STUB_WRI(wri_32)
"stub_read_8__: .globl stub_read_8__\n "STUB_READ(read_8)
"stub_read_16__:.globl stub_read_16__\n"STUB_READ(read_16)
"stub_read_32__:.globl stub_read_32__\n"STUB_READ(read_32)
);

/* call N(%ebx) */
#define JSRPATCH(p,N) *((short *)(p))=0x53ff;p[2]=N;
#define JSRPATCHL(p,N) *((short *)(p))=0x93ff; *((int *)((p)+2))=N;

/*
 * enters here only from a fault
 */
int Cpatch(sigcontext_t *scp)
{
    unsigned char *p;
    int w16;
    unsigned int v;
    unsigned char *eip = (unsigned char *)_rip;

    if (in_cpatch)
	return 0;

    p = eip;
    if ((*p==0xf3 || *p==0xf2) && p[-1] == 0x90 && p[-2] == 0x90) {
	// rep movs, rep stos, rep lods, rep scas, rep cmps
	if (debug_level('e')>1) e_printf("### REP patch at %p\n",eip);
	p-=2;
	G2M(0xff,0x13,p); /* call (%ebx) */
	_rip -= 2; /* make sure call (%ebx) is performed the first time */
	return 1;
    }

    if (*p==0x66) w16=1,p++; else w16=0;
    v = *((int *)p) & 0xffffff;
    while (v==0x0e0489) {		// stack: never fail
	// mov %%{e}ax,(%%esi,%%ecx,1)
	// we have a sequence:	66 89 04 0e
	//		or	89 04 0e
	if (debug_level('e')>1) e_printf("### Stack patch at %p\n",p);
	if (w16) {
	    p--; JSRPATCH(p,Ofs_stub_stk_16); p[3] = 0x90; p+=4;
	}
	else {
	    JSRPATCH(p,Ofs_stub_stk_32); p+=3;
	}
	/* check for optimized multiple register push */
	if (p[0]==0x89) return 1; //O_PUSH3
	p += 9;
	if (p[0]==0xff) return 1; // already JSRPATCH'ed
	if (*p==0x66) w16=1,p++; else w16=0;
	v = *((int *)p) & 0xffffff;
	/* extra check: should not fail */
	if (v!=0x0e0489) {
	    dbug_printf("CPUEMU: stack patch failure, fix source code!\n");
	    return 1;
	}
    }
    if (v==0x900788) {		// movb %%al,(%%edi)
	// we have a sequence:	88 07 90
	if (debug_level('e')>1) e_printf("### Byte write patch at %p\n",eip);
	JSRPATCH(p,Ofs_stub_wri_8);
	return 1;
    }
    if ((v&0xffff)==0x0789) {	// mov %%{e}ax,(%%edi)
	// we have a sequence:	89 07 90
	//		or	66 89 07
	if (debug_level('e')>1) e_printf("### Word/Long write patch at %p\n",eip);
	if (w16) {
	    p--; JSRPATCH(p,Ofs_stub_wri_16);
	}
	else {
	    JSRPATCH(p,Ofs_stub_wri_32);
	}
	return 1;
    }
    if (v==0x90078a) {		// movb (%%edi),%%al
	// we have a sequence:	8a 07 90 90 90 90
	if (debug_level('e')>1) e_printf("### Byte read patch at %p\n",eip);
	JSRPATCHL(p,Ofs_stub_read_8);
	return 1;
    }
    if ((v&0xffff)==0x078b) {	// mov %%{e}ax,(%%edi)
	// we have a sequence:	8b 07 90 90 90 90
	//		or	66 8b 07 90 90 90
	if (debug_level('e')>1) e_printf("### Word/Long read patch at %p\n",eip);
	if (w16) {
	    p--; JSRPATCHL(p,Ofs_stub_read_16);
	}
	else {
	    JSRPATCHL(p,Ofs_stub_read_32);
	}
	return 1;
    }
    if (debug_level('e')>1) e_printf("### Patch unimplemented: %08x\n",*((int *)p));
    return 0;
}


int UnCpatch(unsigned char *eip)
{
    register unsigned char *p;
    p = eip;

    if (*eip != 0xff) return 1;
    e_printf("UnCpatch   at %p was %02x%02x%02x%02x%02x\n",eip,
	eip[0],eip[1],eip[2],eip[3],eip[4]);

    if (p[1] == 0x13) {
	p[0] = p[1] = 0x90;
    }
    else if (p[1] == 0x53) {
	if ((unsigned char)p[2] == Ofs_stub_wri_8) {
	    *((short *)p) = 0x0788; p[2] = 0x90;
	}
	else if ((unsigned char)p[2] == Ofs_stub_wri_16) {
	    *p++ = 0x66; *((short *)p) = 0x0789;
	}
	else if ((unsigned char)p[2] == Ofs_stub_wri_32) {
	    *((short *)p) = 0x0789; p[2] = 0x90;
	}
	else if ((unsigned char)p[2] == Ofs_stub_stk_16) {
	    *((int *)p) = 0x0e048966;
	}
	else if ((unsigned char)p[2] == Ofs_stub_stk_32) {
	    *((short *)p) = 0x0489; p[2] = 0x0e;
	}
	else return 1;
    }
    else return 1;
    e_printf("UnCpatched at %p  is %02x%02x%02x%02x%02x\n",eip,
	eip[0],eip[1],eip[2],eip[3],eip[4]);
    return 0;
}

#endif	//HOST_ARCH_X86

/* ======================================================================= */

