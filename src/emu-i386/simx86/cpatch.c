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

static long _pr_temp, _edi_temp;

asmlinkage void stk_16(caddr_t addr, Bit16u value)
{
	int ret = s_munprotect(addr);
	WRITE_WORD(addr, value);
	if (ret & 1)
		s_mprotect(addr);
}

/*
 * stack on entry:
 *	esp+00	return address
 *	esp+04	eflags
 */
asmlinkage void stub_stk_16(void)
{
	__asm__ __volatile__ (" \
		leal	(%%esi,%%ecx,1),%%edi\n \
		pushal\n \
		pushl	%%eax\n \
		pushl	%%edi\n \
		call	stk_16\n \
		addl	$8,%%esp\n \
		popal\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stk_32(caddr_t addr, Bit32u value)
{
	int ret = s_munprotect(addr);
	WRITE_DWORD(addr, value);
	if (ret & 1)
		s_mprotect(addr);
}

asmlinkage void stub_stk_32(void)
{
	__asm__ __volatile__ (" \
		leal	(%%esi,%%ecx,1),%%edi\n \
		pushal\n \
		pushl	%%eax\n \
		pushl	%%edi\n \
		call	stk_32\n \
		addl	$8,%%esp\n \
		popal\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
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

asmlinkage void stub_wri_8(void)
{
	__asm__ __volatile__ (
"		pushl	%%ebx\n"	/* save regs */
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"	/* return addr = patch point+5 */
#else
"		pushl	4(%%esp)\n"	/* return addr = patch point+5 */
#endif
"		pushl	%%eax\n"	/* value to write */
"		pushl	%%edi\n"	/* addr where to write */
"		call	wri_8\n"
"		addl	$12,%%esp\n"	/* remove parameters */
"		popl	%%ebx\n"	/* restore regs */
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
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

asmlinkage void stub_wri_16(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	4(%%esp)\n"
#endif
"		pushl	%%eax\n \
		pushl	%%edi\n \
		call	wri_16\n \
		addl	$12,%%esp\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
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

asmlinkage void stub_wri_32(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	4(%%esp)\n"
#endif
"		pushl	%%eax\n \
		pushl	%%edi\n \
		call	wri_32\n \
		addl	$12,%%esp\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_movsb(void)
{
	__asm__ __volatile__ (
"		pushl	%%ebx\n"	/* save regs              */
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"	/* push eflags from stack */
#else
"		pushl	8(%%esp)\n"	/* push eflags from stack */
#endif
"		popfl\n"		/* get eflags (DF)        */
"		pushfl\n"		/* and push back          */
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"	/* push return address    */
#else
"		pushl	8(%%esp)\n"	/* push return address    */
#endif
"		lodsb\n"		/* fetch value to write   */
"		pushl	%%eax\n"	/* value to write         */
"		pushl	%%edi\n"	/* push fault address     */
"		scasb\n"		/* adjust edi depends:DF  */
"		call	wri_8\n"
"		addl	$12,%%esp\n"	/* remove parameters      */
"		popfl\n"		/* get eflags             */
"		popl	%%ebx\n"	/* restore regs           */
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_rep_movsb(void)
{
	__asm__ __volatile__ (
"		jecxz	1f\n"		/* zero move, nothing to do */
"		pushal\n"		/* save regs */
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"	/* push eflags from stack */
#else
"		pushl	36(%%esp)\n"	/* save eflags from stack */
#endif
"		pushl	0x44(%%ebx)\n"	/* eflags from TheCPU (Ofs_EFLAGS) */
"		pushl	%%ecx\n"	/* push count */
"		pushl	%%edi\n"	/* push base address */
"		call	r_munprotect\n"
"		addl	$12,%%esp\n"	/* remove parameters */
"		popfl\n"		/* real CPU flags back */
"		popal\n"		/* restore regs */
"		rep; movsb\n"		/* perform op */
#ifdef _DEBUG
"1:		nop"
#else
"1:		ret"
#endif
#if GCC_VERSION_CODE >= 2096
		: : : "%esp" );		/* BUG!! */
#else
		: : );
#endif
}

asmlinkage void stub_movsw(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		popfl\n \
		pushfl\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		lodsw\n \
		pushl	%%eax\n \
		pushl	%%edi\n \
		scasw\n \
		call	wri_16\n \
		addl	$12,%%esp\n \
		popfl\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_rep_movsw(void)
{
	__asm__ __volatile__ (" \
		jecxz	1f\n \
		pushal\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	36(%%esp)\n"
#endif
"		shll	$1,%%ecx\n \
		pushl	0x44(%%ebx)\n \
		pushl	%%ecx\n \
		pushl	%%edi\n \
		call	r_munprotect\n \
		addl	$12,%%esp\n \
		popfl\n \
		popal\n \
		rep; movsw\n"
#ifdef _DEBUG
"1:		nop"
#else
"1:		ret"
#endif
#if GCC_VERSION_CODE >= 2096
		: : : "%esp" );
#else
		: : );
#endif
}

asmlinkage void stub_movsl(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		popfl\n \
		pushfl\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		lodsl\n \
		pushl	%%eax\n \
		pushl	%%edi\n \
		scasl\n \
		call	wri_32\n \
		addl	$12,%%esp\n \
		popfl\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_rep_movsl(void)
{
	__asm__ __volatile__ (" \
		jecxz	1f\n \
		pushal\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	36(%%esp)\n"
#endif
"		shll	$2,%%ecx\n \
		pushl	0x44(%%ebx)\n \
		pushl	%%ecx\n \
		pushl	%%edi\n \
		call	r_munprotect\n \
		addl	$12,%%esp\n \
		popfl\n \
		popal\n \
		rep; movsl\n"
#ifdef _DEBUG
"1:		nop"
#else
"1:		ret"
#endif
#if GCC_VERSION_CODE >= 2096
		: : : "%esp" );
#else
		: : );
#endif
}

asmlinkage void stub_stosb(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	4(%%esp)\n"
#endif
"		pushl	%%eax\n \
		pushl	%%edi\n \
		call	wri_8\n \
		addl	$12,%%esp\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		popfl\n \
		scasb\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_rep_stosb(void)
{
	__asm__ __volatile__ (" \
		jecxz	1f\n \
		pushal\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	36(%%esp)\n"
#endif
"		pushl	0x44(%%ebx)\n \
		pushl	%%ecx\n \
		pushl	%%edi\n \
		call	r_munprotect\n \
		addl	$12,%%esp\n \
		popfl\n \
		popal\n \
		rep; stosb\n"
#ifdef _DEBUG
"1:		nop"
#else
"1:		ret"
#endif
#if GCC_VERSION_CODE >= 2096
		: : : "%esp" );
#else
		: : );
#endif
}

asmlinkage void stub_stosw(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	4(%%esp)\n"
#endif
"		pushl	%%eax\n \
		pushl	%%edi\n \
		call	wri_16\n \
		addl	$12,%%esp\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		popfl\n \
		scasw\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_rep_stosw(void)
{
	__asm__ __volatile__ (" \
		jecxz	1f\n \
		pushal\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	36(%%esp)\n"
#endif
"		shll	$1,%%ecx\n \
		pushl	0x44(%%ebx)\n \
		pushl	%%ecx\n \
		pushl	%%edi\n \
		call	r_munprotect\n \
		addl	$12,%%esp\n \
		popfl\n \
		popal\n \
		rep; stosw\n"
#ifdef _DEBUG
"1:		nop"
#else
"1:		ret"
#endif
#if GCC_VERSION_CODE >= 2096
		: : : "%esp" );
#else
		: : );
#endif
}

asmlinkage void stub_stosl(void)
{
	__asm__ __volatile__ (" \
		pushl	%%ebx\n"
#ifdef _DEBUG
"		pushl	4(%%ebp)\n"
#else
"		pushl	4(%%esp)\n"
#endif
"		pushl	%%eax\n \
		pushl	%%edi\n \
		call	wri_32\n \
		addl	$12,%%esp\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	8(%%esp)\n"
#endif
"		popfl\n \
		scasl\n \
		popl	%%ebx\n"
#ifdef _DEBUG
"		nop"
#else
"		ret"
#endif
		: : );
}

asmlinkage void stub_rep_stosl(void)
{
	__asm__ __volatile__ (" \
		jecxz	1f\n \
		pushal\n"
#ifdef _DEBUG
"		pushl	8(%%ebp)\n"
#else
"		pushl	36(%%esp)\n"
#endif
"		shll	$2,%%ecx\n \
		pushl	0x44(%%ebx)\n \
		pushl	%%ecx\n \
		pushl	%%edi\n \
		call	r_munprotect\n \
		addl	$12,%%esp\n \
		popfl\n \
		popal\n \
		rep; stosl\n"
#ifdef _DEBUG
"1:		nop"
#else
"1:		ret"
#endif
#if GCC_VERSION_CODE >= 2096
		: : : "%esp" );
#else
		: : );
#endif
}


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

