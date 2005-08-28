/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
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

#if GCC_VERSION_CODE >= 3003
#define asmlinkage static __attribute__((used)) __attribute__((cdecl))
#else
#define asmlinkage static __attribute__((unused))
#endif

/* check if the address is aliased to a non protected page, and if it is,
   do not try to unprotect it */
static int check_munprotect(caddr_t addr)
{
	if (LINEAR2UNIX(addr) != addr)
		return 0;
	return e_munprotect(addr,0);
}

int s_munprotect(caddr_t addr)
{
	if (debug_level('e')>3) e_printf("\tS_MUNPROT %08lx\n",(long)addr);
	return check_munprotect(addr);
}

int s_mprotect(caddr_t addr)
{
	if (debug_level('e')>3) e_printf("\tS_MPROT   %08lx\n",(long)addr);
	return e_mprotect(addr,0);
}

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
	if (debug_level('e')>3) e_printf("\tM_MUNPROT %08lx:%08lx [%08lx]\n",
		(long)addr,eip,*((long *)(eip-5)));
#ifndef HOST_ARCH_SIM
	/* verify that data, not code, has been hit */
	if (!e_querymark(addr))
	    return check_munprotect(addr);
	/* Oops.. we hit code, maybe the stub was set up before that
	 * code was parsed. Ok, undo the patch and clear that code */
	e_printf("CODE %08lx hit in DATA %08lx patch\n",(long)addr,eip);
/*	if (UnCpatch((void *)(eip-5))) leavedos(0); */
	InvalidateSingleNode((long)addr, eip);
#endif
	return check_munprotect(addr);
}

asmlinkage int r_munprotect(caddr_t addr, long len, long flags)
{
	__asm__ ("cld");
	if (flags & EFLAGS_DF) addr -= len;
	if (debug_level('e')>3)
	    e_printf("\tR_MUNPROT %08lx:%08lx %s\n",
		(long)addr,(long)addr+len,(flags&EFLAGS_DF?"back":"fwd"));
#ifndef HOST_ARCH_SIM
	InvalidateNodePage((long)addr,len,0,NULL);
	e_resetpagemarks(addr);
#endif
	e_munprotect(addr,len);
	return 0;
}

/* ======================================================================= */

#ifndef HOST_ARCH_SIM

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
	__asm__ ("cld");
	ret = m_munprotect(addr, eip);
	WRITE_BYTE(addr, value);
	if (ret & 1)
		m_mprotect(addr);
}

asmlinkage void wri_16(caddr_t addr, Bit16u value, long eip)
{
	int ret;
	__asm__ ("cld");
	ret = m_munprotect(addr, eip);
	WRITE_WORD(addr, value);
	if (ret & 1)
		m_mprotect(addr);
}

asmlinkage void wri_32(caddr_t addr, Bit32u value, long eip)
{
	int ret;
	__asm__ ("cld");
	ret = m_munprotect(addr, eip);
	WRITE_DWORD(addr, value);
	if (ret & 1)
		m_mprotect(addr);
}

/*
 * stack on entry:
 *	esp+00	return address
 *	esp+04	eflags
 */

#define STUB_STK(cfunc) \
"		pushl	%ebp\n \
		movl	%esp, %ebp \n \
		leal	(%esi,%ecx,1),%edi\n \
		pushal\n \
		pushl	%eax\n \
		pushl	%edi\n \
		call	"#cfunc"\n \
		addl	$8,%esp\n \
		popal\n \
		popl	%ebp\n \
		ret\n"


#define STUB_WRI(cfunc) \
"		pushl	%ebp\n" \
"		movl	%esp, %ebp\n" \
"		pushl	%ebx\n"		/* save regs */ \
"		pushl	4(%ebp)\n"	/* return addr = patch point+5 */ \
"		pushl	%eax\n"		/* value to write */ \
"		pushl	%edi\n"		/* addr where to write */ \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters */ \
"		popl	%ebx\n"		/* restore regs */ \
"		popl	%ebp\n" \
"		ret\n"

#define STUB_MOVS(cfunc,letter) \
"		pushl	%ebp\n" \
"		movl	%esp, %ebp\n" \
"		pushl	%ebx\n"		/* save regs              */ \
"		pushl	8(%ebp)\n"	/* push eflags from stack */ \
"		popfl\n"		/* get eflags (DF)        */ \
"		pushfl\n"		/* and push back          */ \
"		pushl	4(%ebp)\n"	/* push return address    */ \
"		lods"#letter"\n"	/* fetch value to write   */ \
"		pushl	%eax\n"		/* value to write         */ \
"		pushl	%edi\n"		/* push fault address     */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters      */ \
"		popfl\n"		/* get eflags             */ \
"		popl	%ebx\n"		/* restore regs           */ \
"		popl	%ebp\n" \
"		ret\n"

#define STUB_STOS(cfunc,letter) \
"		pushl	%ebp\n" \
"		movl	%esp, %ebp\n" \
"		pushl	%ebx\n"		/* save regs              */ \
"		pushl	4(%ebp)\n"	/* push return address    */ \
"		pushl	%eax\n"		/* value to write         */ \
"		pushl	%edi\n"		/* push fault address     */ \
"		call	"#cfunc"\n" \
"		addl	$12,%esp\n"	/* remove parameters      */ \
"		pushl	8(%ebp)\n"	/* push eflags from stack */ \
"		popfl\n"		/* get eflags (DF)        */ \
"		scas"#letter"\n"	/* adjust edi depends:DF  */ \
"		popl	%ebx\n"		/* restore regs           */ \
"		popl	%ebp\n" \
"		ret\n"

#define STUB_REP(op,ecxshift) \
"		pushl	%ebp\n" \
"		movl	%esp, %ebp\n" \
"		jecxz	1f\n"		/* zero move, nothing to do */ \
"		pushal\n"		/* save regs */ \
"		pushl	8(%ebp)\n"	/* push eflags from stack */ \
"		shll	$"#ecxshift",%ecx\n" \
"		pushl	0x44(%ebx)\n"	/* eflags from TheCPU (Ofs_EFLAGS) */ \
"		pushl	%ecx\n"		/* push count */ \
"		pushl	%edi\n"		/* push base address */ \
"		call	r_munprotect\n" \
"		addl	$12,%esp\n"	/* remove parameters */ \
"		popfl\n"		/* real CPU flags back */ \
"		popal\n"		/* restore regs */ \
"		rep; "#op"\n"		/* perform op */ \
"1:		popl	%ebp\n" \
"		ret\n"

asm (
		".text\n"
"stub_stk_16__:"STUB_STK(stk_16)
"stub_stk_32__:"STUB_STK(stk_32)
"stub_wri_8__: "STUB_WRI(wri_8)
"stub_wri_16__:"STUB_WRI(wri_16)
"stub_wri_32__:"STUB_WRI(wri_32)
"stub_movsb__: "STUB_MOVS(wri_8,b)
"stub_movsw__: "STUB_MOVS(wri_16,w)
"stub_movsl__: "STUB_MOVS(wri_32,l)
"stub_stosb__: "STUB_STOS(wri_8,b)
"stub_stosw__: "STUB_STOS(wri_16,w)
"stub_stosl__: "STUB_STOS(wri_32,l)
"stub_rep_movsb__:"STUB_REP(movsb,0)
"stub_rep_movsw__:"STUB_REP(movsw,1)
"stub_rep_movsl__:"STUB_REP(movsl,2)
"stub_rep_stosb__:"STUB_REP(stosb,0)
"stub_rep_stosw__:"STUB_REP(stosw,1)
"stub_rep_stosl__:"STUB_REP(stosl,2)
);

void stub_stk_16(void) asm ("stub_stk_16__");
void stub_stk_32(void) asm ("stub_stk_32__");
void stub_wri_8 (void) asm ("stub_wri_8__" );
void stub_wri_16(void) asm ("stub_wri_16__");
void stub_wri_32(void) asm ("stub_wri_32__");
void stub_movsb (void) asm ("stub_movsb__" );
void stub_movsw (void) asm ("stub_movsw__" );
void stub_movsl (void) asm ("stub_movsl__" );
void stub_stosb (void) asm ("stub_stosb__" );
void stub_stosw (void) asm ("stub_stosw__" );
void stub_stosl (void) asm ("stub_stosl__" );
void stub_rep_movsb (void) asm ("stub_rep_movsb__" );
void stub_rep_movsw (void) asm ("stub_rep_movsw__" );
void stub_rep_movsl (void) asm ("stub_rep_movsl__" );
void stub_rep_stosb (void) asm ("stub_rep_stosb__" );
void stub_rep_stosw (void) asm ("stub_rep_stosw__" );
void stub_rep_stosl (void) asm ("stub_rep_stosl__" );

/* ======================================================================= */

#define JSRPATCH(p,N)	*p++=0xe8;*((long *)(p))=(long)((unsigned char *)N-((p)+4))

/*
 * enters here only from a fault
 */
int Cpatch(unsigned char *eip)
{
    unsigned char *p;
    int w16, rep;
    unsigned long v;

    p = eip;
    if (*p==0xf3) rep=1,p++; else rep=0;
    if (*p==0x66) w16=1,p++; else w16=0;
    v = *((long *)p);
    
    if (v==0x900e0489) {	// stack: never fail
	// mov %%{e}ax,(%%esi,%%ecx,1)
	// we have a sequence:	66 89 04 0e 90
	//		or	89 04 0e 90 90
	if (debug_level('e')>1) e_printf("### Stack patch at %08lx\n",(long)eip);
	if (w16) {
	    p--; JSRPATCH(p,&stub_stk_16);
	}
	else {
	    JSRPATCH(p,&stub_stk_32);
	}
	return 1;
    }
    if (v==0x90900788) {	// movb %%al,(%%edi)
	// we have a sequence:	88 07 90 90 90
	if (debug_level('e')>1) e_printf("### Byte write patch at %08lx\n",(long)eip);
	JSRPATCH(p,&stub_wri_8);
	return 1;
    }
    if (v==0x90900789) {	// mov %%{e}ax,(%%edi)
	// we have a sequence:	89 07 90 90 90
	//		or	66 89 07 90 90
	if (debug_level('e')>1) e_printf("### Word/Long write patch at %08lx\n",(long)eip);
	if (w16) {
	    p--; JSRPATCH(p,&stub_wri_16);
	}
	else {
	    JSRPATCH(p,&stub_wri_32);
	}
	return 1;
    }
    if (v==0x909090a5) {	// movsw
	if (rep) {
	if (debug_level('e')>1) e_printf("### REP movs{wl} patch at %08lx\n",(long)eip);
	    if (w16) {
		p-=2; JSRPATCH(p,&stub_rep_movsw);
	    }
	    else {
		p--; JSRPATCH(p,&stub_rep_movsl);
	    }
	}
	else {
	if (debug_level('e')>1) e_printf("### movs{wl} patch at %08lx\n",(long)eip);
	    if (w16) {
		p--; JSRPATCH(p,&stub_movsw);
	    }
	    else {
		JSRPATCH(p,&stub_movsl);
	    }
	}
	return 1;
    }
    if (v==0x909090a4) {	// movsb
	if (rep) {
	if (debug_level('e')>1) e_printf("### REP movsb patch at %08lx\n",(long)eip);
	    p--; JSRPATCH(p,&stub_rep_movsb);
	}
	else {
	if (debug_level('e')>1) e_printf("### movsb patch at %08lx\n",(long)eip);
	    JSRPATCH(p,&stub_movsb);
	}
	return 1;
    }
    if (v==0x909090ab) {	// stosw
	if (rep) {
	if (debug_level('e')>1) e_printf("### REP stos{wl} patch at %08lx\n",(long)eip);
	    if (w16) {
		p-=2; JSRPATCH(p,&stub_rep_stosw);
	    }
	    else {
		p--; JSRPATCH(p,&stub_rep_stosl);
	    }
	}
	else {
	if (debug_level('e')>1) e_printf("### stos{wl} patch at %08lx\n",(long)eip);
	    if (w16) {
		p--; JSRPATCH(p,&stub_stosw);
	    }
	    else {
		JSRPATCH(p,&stub_stosl);
	    }
	}
	return 1;
    }
    if (v==0x909090aa) {	// stosb
	if (rep) {
	if (debug_level('e')>1) e_printf("### REP stosb patch at %08lx\n",(long)eip);
	    p--; JSRPATCH(p,&stub_rep_stosb);
	}
	else {
	if (debug_level('e')>1) e_printf("### stosb patch at %08lx\n",(long)eip);
	    JSRPATCH(p,&stub_stosb);
	}
	return 1;
    }
    if (debug_level('e')>1) e_printf("### Patch unimplemented: %08lx\n",*((long *)p));
    return 0;
}


int UnCpatch(unsigned char *eip)
{
    long subad;
    register unsigned char *p = eip;

    if (*eip != 0xe8) return 1;
    e_printf("UnCpatch   at %08lx was %02x%02x%02x%02x%02x\n",(long)eip,
	eip[0],eip[1],eip[2],eip[3],eip[4]);
    subad = *((long *)(eip+1)) + ((long)eip+5);

    if (subad == (long)&stub_wri_8) {
	*((long *)p) = 0x90900788; p[4] = 0x90;
    }
    else if (subad == (long)&stub_wri_16) {
	*p++ = 0x66; *((long *)p) = 0x90900789;
    }
    else if (subad == (long)&stub_wri_32) {
	*((long *)p) = 0x90900789; p[4] = 0x90;
    }
    else if (subad == (long)&stub_movsb) {
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
    else if (subad == (long)&stub_rep_movsb) {
	*((long *)p) = 0x9090a4f3; p[4] = 0x90;
    }
    else if (subad == (long)&stub_rep_movsw) {
	*p++ = 0x66; *((long *)p) = 0x9090a5f3;
    }
    else if (subad == (long)&stub_rep_movsl) {
	*((long *)p) = 0x9090a5f3; p[4] = 0x90;
    }
    else if (subad == (long)&stub_rep_stosb) {
	*((long *)p) = 0x9090aaf3; p[4] = 0x90;
    }
    else if (subad == (long)&stub_rep_stosw) {
	*p++ = 0x66; *((long *)p) = 0x9090abf3;
    }
    else if (subad == (long)&stub_rep_stosl) {
	*((long *)p) = 0x9090abf3; p[4] = 0x90;
    }
    else if (subad == (long)&stub_stk_16) {
	*p++ = 0x66; *((long *)p) = 0x900e0489;
    }
    else if (subad == (long)&stub_stk_32) {
	*((long *)p) = 0x900e0489; p[4] = 0x90;
    }
    else return 1;
    e_printf("UnCpatched at %08lx  is %02x%02x%02x%02x%02x\n",(long)eip,
	eip[0],eip[1],eip[2],eip[3],eip[4]);
    return 0;
}

#endif	//HOST_ARCH_SIM

/* ======================================================================= */

