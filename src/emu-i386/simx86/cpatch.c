/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2006 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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

#if GCC_VERSION_CODE >= 3003
#ifdef __i386__
#define asmlinkage static __attribute__((used)) __attribute__((cdecl))
#else
#define asmlinkage static __attribute__((used))
#endif
#else
#define asmlinkage static __attribute__((unused))
#endif

int s_munprotect(caddr_t addr)
{
	if (debug_level('e')>3) e_printf("\tS_MUNPROT %08lx\n",(long)addr);
	return e_check_munprotect(addr);
}

int s_mprotect(caddr_t addr)
{
	if (debug_level('e')>3) e_printf("\tS_MPROT   %08lx\n",(long)addr);
	return e_mprotect(addr,0);
}

#ifdef HOST_ARCH_X86

static int m_mprotect(caddr_t addr)
{
	if (debug_level('e')>3)
	    e_printf("\tM_MPROT   %08lx\n",(long)addr);
	return e_mprotect(addr,0);
}

/*
 * Return address of the stub function is passed into eip
 */
static int m_munprotect(caddr_t addr, long eip)
{
	if (debug_level('e')>3) e_printf("\tM_MUNPROT %08lx:%08lx [%08x]\n",
		(long)addr,eip,*((int *)(eip-3)));
	/* verify that data, not code, has been hit */
	if (!e_querymark(addr))
	    return e_check_munprotect(addr);
	/* Oops.. we hit code, maybe the stub was set up before that
	 * code was parsed. Ok, undo the patch and clear that code */
	e_printf("CODE %08lx hit in DATA %08lx patch\n",(long)addr,eip);
/*	if (UnCpatch((void *)(eip-3))) leavedos(0); */
	InvalidateSingleNode((long)addr, eip);
	return e_check_munprotect(addr);
}

asmlinkage int r_munprotect(caddr_t addr, long len, unsigned char *eip)
{

	if (*eip == 0x66)
		len *= 2;
	else if (*eip & 1)
		len *= 4;
	if (EFLAGS & EFLAGS_DF) addr -= len;
	if (debug_level('e')>3)
	    e_printf("\tR_MUNPROT %08lx:%08lx %s\n",
		(long)addr,(long)addr+len,(EFLAGS&EFLAGS_DF?"back":"fwd"));
	InvalidateNodePage((long)addr,len,0,NULL);
	e_resetpagemarks(addr,len);
	e_munprotect(addr,len);
	return 0;
}

/* ======================================================================= */

asmlinkage void stk_16(caddr_t addr, Bit16u value)
{
	int ret = s_munprotect(addr);
	WRITE_WORD(addr, value);
	if (ret & 1)
		s_mprotect(addr);
}

asmlinkage void stk_32(caddr_t addr, Bit32u value)
{
	int ret = s_munprotect(addr);
	WRITE_DWORD(addr, value);
	if (ret & 1)
		s_mprotect(addr);
}

asmlinkage void wri_8(caddr_t addr, Bit8u value, long eip)
{
	int ret;
	ret = m_munprotect(addr, eip);
	WRITE_BYTE(addr, value);
	if (ret & 1)
		m_mprotect(addr);
}

asmlinkage void wri_16(caddr_t addr, Bit16u value, long eip)
{
	int ret;
	ret = m_munprotect(addr, eip);
	WRITE_WORD(addr, value);
	if (ret & 1)
		m_mprotect(addr);
}

asmlinkage void wri_32(caddr_t addr, Bit32u value, long eip)
{
	int ret;
	ret = m_munprotect(addr, eip);
	WRITE_DWORD(addr, value);
	if (ret & 1)
		m_mprotect(addr);
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
"		pushl	(%esp)\n"	/* return addr = patch point+5 */ \
"		pushl	%eax\n"		/* value to write */ \
"		pushl	%edi\n"		/* addr where to write */ \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters */ \
"		ret\n"

#define STUB_MOVS(cfunc,letter) \
"		pushl	(%esp)\n"	/* return addr = patch point+5 */ \
"		lods"#letter"\n"	/* fetch value to write   */ \
"		pushl	%eax\n"		/* value to write         */ \
"		pushl	%edi\n"		/* push fault address     */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		cld\n" \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters      */ \
"		ret\n"

#define STUB_STOS(cfunc,letter) \
"		pushl	(%esp)\n"	/* return addr = patch point+5 */ \
"		pushl	%eax\n"		/* value to write         */ \
"		pushl	%edi\n"		/* push fault address     */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		cld\n" \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters      */ \
"		ret\n"

asm (
".text\n.globl stub_rep__\n"
"stub_rep__:	jecxz	1f\n"		/* zero move, nothing to do */
"		pushl	%eax\n"		/* save regs */
"		pushl	%ecx\n"
"		pushfl\n"		/* push flags for DF */
"		pushl	12(%esp)\n"	/* push return address */
"		pushl	%ecx\n"		/* push count */
"		pushl	%edi\n"		/* push base address */
"		cld\n"
"		call	r_munprotect\n"
"		addl	$12,%esp\n"	/* remove parameters */
"		popfl\n"		/* real CPU flags back */
"		popl	%ecx\n"		/* restore regs */
"		popl	%eax\n"		
"1:		ret\n"
);

asm (
		".text\n"
"stub_movsb__: "STUB_MOVS(wri_8,b)
"stub_movsw__: "STUB_MOVS(wri_16,w)
"stub_movsl__: "STUB_MOVS(wri_32,l)
"stub_stosb__: "STUB_STOS(wri_8,b)
"stub_stosw__: "STUB_STOS(wri_16,w)
"stub_stosl__: "STUB_STOS(wri_32,l)
);

void stub_movsb (void) asm ("stub_movsb__" );
void stub_movsw (void) asm ("stub_movsw__" );
void stub_movsl (void) asm ("stub_movsl__" );
void stub_stosb (void) asm ("stub_stosb__" );
void stub_stosw (void) asm ("stub_stosw__" );
void stub_stosl (void) asm ("stub_stosl__" );

/* ======================================================================= */

#define JSRPATCHOLD(p,N) \
	*p++=0xe8;*((int *)(p))=(long)((unsigned char *)N-((p)+4))

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
"		movq	(%rsp),%rdx\n"	/* return addr = patch point+5 */ \
"		pushq	%rdi\n"		/* save regs */ \
"		movl	%eax,%esi\n"	/* value to write */ \
					/* pass addr where to write in %rdi */\
"		call	"#cfunc"\n" \
"		popq	%rdi\n"		/* restore regs */ \
"		ret\n"

asm (
".text\n.globl stub_rep__\n"
"stub_rep__:	jrcxz	1f\n"		/* zero move, nothing to do */
"		movq	(%rsp),%rdx\n"  /* pass return address */
"		pushq	%rax\n"		/* save regs */
"		pushq	%rcx\n"
"		pushq	%rdi\n"
"		pushq	%rsi\n"
"		pushfq\n"		/* push flags for DF */
"		movl	%ecx,%esi\n"	/* pass count */
					/* pass base address in %rdi */
"		cld\n"
"		call	r_munprotect\n"
"		popfq\n"		/* real CPU flags back */
"		popq	%rsi\n"		/* restore regs */
"		popq	%rdi\n"
"		popq	%rcx\n"
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
);

/* call N(%ebx) */
#define JSRPATCH(p,N) *((short *)(p))=0x53ff;p[2]=N;

/*
 * enters here only from a fault
 */
int Cpatch(struct sigcontext_struct *scp)
{
    unsigned char *p;
    int w16;
    unsigned int v;
    unsigned char *eip = (unsigned char *)_rip;

    p = eip;
    if (*p==0xf3 && p[-1] == 0x90 && p[-2] == 0x90) {	// rep movs, rep stos
	if (debug_level('e')>1) e_printf("### REP movs/stos patch at %08lx\n",(long)eip);
	p-=2;
	G2M(0xff,0x13,p); /* call (%ebx) */
	_rip -= 2; /* make sure call (%ebx) is performed the first time */
	return 1;
    }

    if (*p==0x66) w16=1,p++; else w16=0;
    v = *((int *)p) & 0xffffff;
    if (v==0x0e0489) {		// stack: never fail
	// mov %%{e}ax,(%%esi,%%ecx,1)
	// we have a sequence:	66 89 04 0e
	//		or	89 04 0e
	if (debug_level('e')>1) e_printf("### Stack patch at %08lx\n",(long)eip);
	if (w16) {
	    p--; JSRPATCH(p,Ofs_stub_stk_16); p[3] = 0x90;
	}
	else {
	    JSRPATCH(p,Ofs_stub_stk_32);
	}
	return 1;
    }
    if (v==0x900788) {		// movb %%al,(%%edi)
	// we have a sequence:	88 07 90
	if (debug_level('e')>1) e_printf("### Byte write patch at %08lx\n",(long)eip);
	JSRPATCH(p,Ofs_stub_wri_8);
	return 1;
    }
    if ((v&0xffff)==0x0789) {	// mov %%{e}ax,(%%edi)
	// we have a sequence:	89 07 90
	//		or	66 89 07
	if (debug_level('e')>1) e_printf("### Word/Long write patch at %08lx\n",(long)eip);
	if (w16) {
	    p--; JSRPATCH(p,Ofs_stub_wri_16);
	}
	else {
	    JSRPATCH(p,Ofs_stub_wri_32);
	}
	return 1;
    }
#ifdef __i386__
    if (v==0x909090a5) {	// movsw
	if (debug_level('e')>1) e_printf("### movs{wl} patch at %08lx\n",(long)eip);
	if (w16) {
	    p--; JSRPATCHOLD(p,&stub_movsw);
	}
	else {
	    JSRPATCHOLD(p,&stub_movsl);
	}
	return 1;
    }
    if (v==0x909090a4) {	// movsb
	if (debug_level('e')>1) e_printf("### movsb patch at %08lx\n",(long)eip);
	    JSRPATCHOLD(p,&stub_movsb);
	return 1;
    }
    if (v==0x909090ab) {	// stosw
	if (debug_level('e')>1) e_printf("### stos{wl} patch at %08lx\n",(long)eip);
	if (w16) {
	    p--; JSRPATCHOLD(p,&stub_stosw);
	}
	else {
	    JSRPATCHOLD(p,&stub_stosl);
	}
	return 1;
    }
    if (v==0x909090aa) {	// stosb
	if (debug_level('e')>1) e_printf("### stosb patch at %08lx\n",(long)eip);
	JSRPATCHOLD(p,&stub_stosb);
	return 1;
    }
#endif
    if (debug_level('e')>1) e_printf("### Patch unimplemented: %08x\n",*((int *)p));
    return 0;
}


int UnCpatch(unsigned char *eip)
{
    register signed char *p;
    p = eip;

    if (*eip != 0xe8 && *eip != 0xff) return 1;
    e_printf("UnCpatch   at %08lx was %02x%02x%02x%02x%02x\n",(long)eip,
	eip[0],eip[1],eip[2],eip[3],eip[4]);

#ifdef __i386__
    if (*eip == 0xe8) {
    long subad = *((long *)(eip+1)) + ((long)eip+5);
    if (subad == (long)&stub_movsb) {
	*((long *)p) = 0x909090a4; p[4] = 0x90;
    }
    else if (subad == (long)&stub_movsw) {
	*p++ = 0x66; *((long *)p) = 0x909090a5;
    }
    else if (subad == (long)&stub_movsl) {
	*((long *)p) = 0x909090a5; p[4] = 0x90;
    }
    else if (subad == (long)&stub_stosb) {
	*((long *)p) = 0x909090aa; p[4] = 0x90;
    }
    else if (subad == (long)&stub_stosw) {
	*p++ = 0x66; *((long *)p) = 0x909090ab;
    }
    else if (subad == (long)&stub_stosl) {
	*((long *)p) = 0x909090ab; p[4] = 0x90;
    }
    else return 1;
    } else /* p[0] == 0xff */
#endif
    if (p[1] == 0x13) {
	p[0] = p[1] = 0x90;
    }
    else if (p[1] == 0x53) {
	if (p[2] == Ofs_stub_wri_8) {
	    *((short *)p) = 0x0788; p[2] = 0x90;
	}
	else if (p[2] == Ofs_stub_wri_16) {
	    *p++ = 0x66; *((short *)p) = 0x0789;
	}
	else if (p[2] == Ofs_stub_wri_32) {
	    *((short *)p) = 0x0789; p[2] = 0x90;
	}
	else if (p[2] == Ofs_stub_stk_16) {
	    *((int *)p) = 0x0e048966;
	}
	else if (p[2] == Ofs_stub_stk_32) {
	    *((short *)p) = 0x0489; p[2] = 0x0e;
	}
	else return 1;
    }
    else return 1;
    e_printf("UnCpatched at %08lx  is %02x%02x%02x%02x%02x\n",(long)eip,
	eip[0],eip[1],eip[2],eip[3],eip[4]);
    return 0;
}

#endif	//HOST_ARCH_X86

/* ======================================================================= */

