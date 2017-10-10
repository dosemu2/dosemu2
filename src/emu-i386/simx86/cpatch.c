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

#ifdef __i386__
#define asmlinkage static __attribute__((used)) __attribute__((cdecl))
#else
#define asmlinkage static __attribute__((used))
#endif

#ifdef HOST_ARCH_X86

/*
 * Return address of the stub function is passed into eip
 */
static void m_munprotect(unsigned int addr, unsigned int len, unsigned char *eip)
{
	if (debug_level('e')>3) e_printf("\tM_MUNPROT %08x:%p [%08x]\n",
		addr,eip,*((int *)(eip-3)));
	/* if only data in aliased low memory is hit, nothing to do */
	if (LINEAR2UNIX(addr) != MEM_BASE32(addr) && !e_querymark(addr, len))
		return;
	/* Always unprotect and clear all code in the pages
	 * for either DPMI data or code.
	 * Maybe the stub was set up before that code was parsed.
	 * Clear that code */
	if (debug_level('e')>1 && e_querymark(addr, len))
	    e_printf("CODE %08x hit in DATA %p patch\n",addr,eip);
/*	if (UnCpatch((void *)(eip-3))) leavedos_main(0); */
	InvalidateNodePage(addr,len,eip,NULL);
	e_resetpagemarks(addr,len);
	e_munprotect(addr,len);
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
		if (vga_access(source, addr)) {
			if (EFLAGS & EFLAGS_DF)
				vga_memcpy(addr - len + size,
					   source - len + size, len);
			else
				vga_memcpy(addr, source, len);
			ecx = 0;
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
done:
		if (EFLAGS & EFLAGS_DF) source -= len;
		else source += len;
		stack->esi = MEM_BASE32(source);
	}
	else { /* stos */
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
	if (EFLAGS & EFLAGS_DF) addr -= len;
	else addr += len;
	stack->edi = MEM_BASE32(addr);
	stack->ecx = ecx;
	InCompiledCode++;
}

/* ======================================================================= */

asmlinkage void stk_16(unsigned char *paddr, Bit16u value)
{
	dosaddr_t addr;

	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	e_invalidate(addr, 2);
	WRITE_WORD(addr, value);
	InCompiledCode++;
}

asmlinkage void stk_32(unsigned char *paddr, Bit32u value)
{
	dosaddr_t addr;

	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	e_invalidate(addr, 4);
	WRITE_DWORD(addr, value);
	InCompiledCode++;
}

asmlinkage void wri_8(unsigned char *paddr, Bit8u value, unsigned char *eip)
{
	dosaddr_t addr;
	Bit8u *p;

	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	m_munprotect(addr, 1, eip);
	p = LINEAR2UNIX(addr);
	InCompiledCode++;
	/* there is a slight chance that this stub hits VGA memory.
	   For that case there is a simple instruction decoder but
	   we must use mov %al,(%edi) (%rdi for x86_64) */
	asm("movb %1,(%2)" : "=m"(*p) : "a"(value), "D"(p));
}

asmlinkage void wri_16(unsigned char *paddr, Bit16u value, unsigned char *eip)
{
	dosaddr_t addr;
	Bit16u *p;

	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	m_munprotect(addr, 2, eip);
	p = LINEAR2UNIX(addr);
	InCompiledCode++;
	asm("movw %1,(%2)" : "=m"(*p) : "a"(value), "D"(p));
}

asmlinkage void wri_32(unsigned char *paddr, Bit32u value, unsigned char *eip)
{
	dosaddr_t addr;
	Bit32u *p;

	assert(InCompiledCode);
	InCompiledCode--;
	addr = DOSADDR_REL(paddr);
	m_munprotect(addr, 4, eip);
	p = LINEAR2UNIX(addr);
	InCompiledCode++;
	asm("movl %1,(%2)" : "=m"(*p) : "a"(value), "D"(p));
}

#ifdef __i386__

/*
 * stack on entry:
 *	esp+00	return address
 */

#define STUB_STK(cfunc) \
"		leal	(%esi,%ecx,1),%edi\n \
		pushal\n \
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
"		addl	$12,%esp\n"	/* remove parameters */ \
"		ret\n"

#define STUB_MOVS(cfunc,letter) \
"		pushl	(%esp)\n"	/* return addr = patch point+6 */ \
"		lods"#letter"\n"	/* fetch value to write   */ \
"		pushl	%eax\n"		/* value to write         */ \
"		pushl	%edi\n"		/* push fault address     */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		cld\n" \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters      */ \
"		ret\n"

#define STUB_STOS(cfunc,letter) \
"		pushl	%eax\n"		/* save eax (consecutive stosws) */ \
"		pushl	4(%esp)\n"	/* return addr = patch point+6 */ \
"		pushl	%eax\n"		/* value to write         */ \
"		pushl	%edi\n"		/* push fault address     */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		cld\n" \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters      */ \
"		popl	%eax\n" 	/* restore eax */ \
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
"		leal	(%rsi,%rcx,1),%edi\n" \
"		pushq	%rax\n"		/* save regs */ \
"		pushq	%rcx\n" \
"		pushq	%rdx\n" \
"		pushq	%rdi\n" \
"		pushq	%rsi\n" \
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
"		movl	%eax,%esi\n"	/* value to write */ \
					/* pass addr where to write in %rdi */\
"		call	"#cfunc"\n" \
"		popq	%rdi\n"		/* restore regs */ \
"		ret\n"

#define STUB_MOVS(cfunc,letter) \
"		movq	(%rsp),%rdx\n"	/* return addr = patch point+6 */ \
"		movl	%edi,%ecx\n"	/* save edi to pass */ \
"		lods"#letter"\n"	/* fetch value to write   */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		pushq	%rdi\n"		/* save regs */ \
"		pushq	%rsi\n" \
"		pushq	%rax\n"		/* not needed, but stack alignment */ \
"		movl	%eax,%esi\n"	/* value to write         */ \
"		movl	%ecx,%edi\n"	/* pass fault address in %rdi */ \
"		cld\n" \
"		call	"#cfunc"\n" \
"		popq	%rax\n"		/* restore regs */ \
"		popq	%rsi\n" \
"		popq	%rdi\n" \
"		ret\n"

#define STUB_STOS(cfunc,letter) \
"		movq	(%rsp),%rdx\n"	/* return addr = patch point+6 */ \
"		movl	%edi,%ecx\n"	/* save edi to pass */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		pushq	%rdi\n"		/* save regs */ \
"		pushq	%rsi\n"		/* rax is saved for consecutive */ \
"		pushq	%rax\n"		/* merged stosws & stack alignment */ \
"		movl	%eax,%esi\n"	/* value to write         */ \
"		movl	%ecx,%edi\n"	/* pass fault address in %rdi */ \
"		cld\n" \
"		call	"#cfunc"\n" \
"		popq	%rax\n"		/* restore regs */ \
"		popq	%rsi\n" \
"		popq	%rdi\n"	\
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
"stub_movsb__: .globl stub_movsb__\n "STUB_MOVS(wri_8,b)
"stub_movsw__: .globl stub_movsw__\n "STUB_MOVS(wri_16,w)
"stub_movsl__: .globl stub_movsl__\n "STUB_MOVS(wri_32,l)
"stub_stosb__: .globl stub_stosb__\n "STUB_STOS(wri_8,b)
"stub_stosw__: .globl stub_stosw__\n "STUB_STOS(wri_16,w)
"stub_stosl__: .globl stub_stosl__\n "STUB_STOS(wri_32,l)
);

/* call N(%ebx) */
#define JSRPATCH(p,N) *((short *)(p))=0x53ff;p[2]=N;
#define JSRPATCHL(p,N) *((short *)(p))=0x93ff; *((int *)((p)+2))=N;

/*
 * enters here only from a fault
 */
int Cpatch(struct sigcontext *scp)
{
    unsigned char *p;
    int w16;
    unsigned int v;
    unsigned char *eip = (unsigned char *)_rip;

    p = eip;
    if (*p==0xf3 && p[-1] == 0x90 && p[-2] == 0x90) {	// rep movs, rep stos
	if (debug_level('e')>1) e_printf("### REP movs/stos patch at %p\n",eip);
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
    if (v==0x9090a5) {	// movsw
	if (debug_level('e')>1) e_printf("### movs{wl} patch at %p\n",eip);
	if (w16) {
	    p--; JSRPATCHL(p,Ofs_stub_movsw);
	}
	else {
	    JSRPATCHL(p,Ofs_stub_movsl);
	}
	return 1;
    }
    if (v==0x9090a4) {	// movsb
	if (debug_level('e')>1) e_printf("### movsb patch at %p\n",eip);
	    JSRPATCHL(p,Ofs_stub_movsb);
	return 1;
    }
    if (v==0x9090ab) {	// stosw
	if (debug_level('e')>1) e_printf("### stos{wl} patch at %p\n",eip);
	if (w16) {
	    p--; JSRPATCHL(p,Ofs_stub_stosw);
	}
	else {
	    JSRPATCHL(p,Ofs_stub_stosl);
	}
	return 1;
    }
    if (v==0x9090aa) {	// stosb
	if (debug_level('e')>1) e_printf("### stosb patch at %p\n",eip);
	JSRPATCHL(p,Ofs_stub_stosb);
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

    if (eip[1] == 0x93) {
	int *p2 = (int *)(p+2);
	if (*p2 == Ofs_stub_movsb) {
	     // movsb; nop; nop; nop; nop; cld
	     *((short *)p) = 0x90a4; *p2 = 0xfc909090;
	}
	else if (*p2 == Ofs_stub_movsw) {
	     *((short *)p) = 0xa566; *p2 = 0x90909090;
	}
	else if (*p2 == Ofs_stub_movsl) {
	     *((short *)p) = 0x90a5; *p2 = 0xfc909090;
	}
	else if (*p2 == Ofs_stub_stosb) {
	     *((short *)p) = 0x90aa; *p2 = 0xfc909090;
	}
	else if (*p2 == Ofs_stub_stosw) {
	     *((short *)p) = 0xab66; *p2 = 0x90909090;
	}
	else if (*p2 == Ofs_stub_stosl) {
	     *((short *)p) = 0x90ab; *p2 = 0xfc909090;
	}
	else return 1;
    }
    else if (p[1] == 0x13) {
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

