/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
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

#ifndef _EMU86_HOST_H
#define _EMU86_HOST_H

#if defined(ppc)||defined(__ppc)||defined(__ppc__)
/* NO PAGING! */
/*
 *  $Id$
 */
/* alas, egcs sounds like it has a bug in this code that doesn't use the
   inline asm correctly, and can cause file corruption. */
static __inline__ unsigned short ppc_pswap2(long addr)
{
	unsigned val;
	__asm__ __volatile__ ("lhbrx %0,0,%1" : "=r" (val) :
		 "r" ((unsigned short *)addr), "m" (*(unsigned short *)addr));
	return val;
}

static __inline__ void ppc_dswap2(long addr, unsigned short val)
{
	__asm__ __volatile__ ("sthbrx %1,0,%2" : "=m" (*(unsigned short *)addr) :
		 "r" (val), "r" ((unsigned short *)addr));
}

static __inline__ unsigned long ppc_pswap4(long addr)
{
	unsigned val;
	__asm__ __volatile__ ("lwbrx %0,0,%1" : "=r" (val) :
		 "r" ((unsigned long *)addr), "m" (*(unsigned long *)addr));
	return val;
}

static __inline__ unsigned long long ppc_pswap8(long addr)
{
	union {	unsigned long long lq; struct {unsigned long ll,lh;} lw; } val;
	__asm__ __volatile__ (" \
		lwbrx %0,0,%2\n \
		addi  %2,%2,4\n \
		lwbrx %1,0,%2" \
		: "=r" (val.lw.lh), "=r" (val.lw.ll)
		: "r" ((unsigned long *)addr), "m" (*(unsigned long *)addr) );
	return val.lq;
}

static __inline__ void ppc_dswap4(long addr, unsigned long val)
{
	__asm__ __volatile__ ("stwbrx %1,0,%2" : "=m" (*(unsigned long *)addr) :
		 "r" (val), "r" ((unsigned long *)addr));
}

static __inline__ void ppc_dswap8(long addr, unsigned long long val)
{
	union { unsigned long long lq; struct {unsigned long lh,ll;} lw; } v;
	v.lq = val;
	__asm__ __volatile__ (" \
		stwbrx %1,0,%3\n \
		addi   %3,%3,4\n \
		stwbrx %2,0,%3" \
		: "=m" (*(unsigned long *)addr)
		: "r" (v.lw.ll), "r" (v.lw.lh), "r" ((unsigned long *)addr) );
}

#endif		/* ppc */

/////////////////////////////////////////////////////////////////////////////

#if defined(i386)||defined(__i386)||defined(__i386__)
#ifdef USE_BOUND
/* `Fetch` is for CODE reads, `Get`/`Put` is for DATA.
 *  WARNING - BOUND uses SIGNED limits!! */
#define Fetch(a)	({ \
	register long p = (long)(a);\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	*((unsigned char *)p); })
#define FetchW(a)	({ \
	register long p = (long)(a)+1;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	*((unsigned short *)(a)); })
#define FetchL(a)	({ \
	register long p = (long)(a)+3;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	*((unsigned long *)(a)); })

#define DataFetchWL_U(m,a)	({ \
	register unsigned f = ((m)&DATA16? 1:3);\
	register long p = (long)(a)+f;\
	register long res;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	__asm__ ("xorl	%0,%0\n\
		shr	$2,%1\n\
		jc	1f\n\
		.byte	0x66\n\
1:		movl	(%2),%0"\
		: "=&r"(res) : "r"(f), "g"(a) : "memory" ); res; })

#define DataFetchWL_S(m,a)	({ \
	register unsigned f = ((m)&DATA16? 1:3);\
	register long p = (long)(a)+f;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	(f&2? *((long *)(a)):*((short *)(a))); })

#define AddrFetchWL_U(m,a)	({ \
	register unsigned f = ((m)&ADDR16? 1:3);\
	register long p = (long)(a)+f;\
	register long res;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	__asm__ ("xorl	%0,%0\n\
		shr	$2,%1\n\
		jc	1f\n\
		.byte	0x66\n\
1:		movl	(%2),%0"\
		: "=&r"(res) : "r"(f), "g"(a) : "memory" ); res; })

#define AddrFetchWL_S(m,a)	({ \
	register unsigned f = ((m)&ADDR16? 1:3);\
	register long p = (long)(a)+f;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	(f&2? *((long *)(a)):*((short *)(a))); })
#else
#define Fetch(a)	*((unsigned char *)(a))
#define FetchW(a)	*((unsigned short *)(a))
#define FetchL(a)	*((unsigned long *)(a))
#define DataFetchWL_U(m,a) ((m)&DATA16? FetchW(a):FetchL(a))
#define DataFetchWL_S(m,a) ((m)&DATA16? (short)FetchW(a):(long)FetchL(a))
#define AddrFetchWL_U(m,a) ((m)&ADDR16? FetchW(a):FetchL(a))
#define AddrFetchWL_S(m,a) ((m)&ADDR16? (short)FetchW(a):(long)FetchL(a))
#endif
#define GetDWord(a)	*((unsigned short *)(a))
#define GetDLong(a)	*((unsigned long *)(a))
#define DataGetWL_U(m,a) ((m)&DATA16? GetDWord(a):GetDLong(a))
#define DataGetWL_S(m,a) ((m)&DATA16? (short)GetDWord(a):(long)GetDLong(a))
#endif

#if defined(ppc)||defined(__ppc)||defined(__ppc__)
#define Fetch(a)	*((unsigned char *)(a))
#define FetchW(a)	ppc_pswap2((long)(a))
#define FetchL(a)	ppc_pswap4((long)(a))
#define DataFetchWL_U(m,a) ((m)&DATA16? FetchW(a):FetchL(a))
#define DataFetchWL_S(m,a) ((m)&DATA16? (short)FetchW(a):(long)FetchL(a))
#define AddrFetchWL_U(m,a) ((m)&ADDR16? FetchW(a):FetchL(a))
#define AddrFetchWL_S(m,a) ((m)&ADDR16? (short)FetchW(a):(long)FetchL(a))

#define GetDWord(a)	ppc_pswap2((long)(a))
#define GetDLong(a)	ppc_pswap4((long)(a))
#define DataGetWL_U(m,a) ((m)&DATA16? GetDWord(a):GetDLong(a))
#define DataGetWL_S(m,a) ((m)&DATA16? (short)GetDWord(a):(long)GetDLong(a))
#endif

#if 0
/* general-purpose */
//static inline unsigned short pswap2(long a) {
//	register unsigned char *p = (unsigned char *)a;
//	return p[0] | (p[1]<<8);
//}
//
//static inline unsigned short dswap2(unsigned short w) {
//	register unsigned char *p = (unsigned char *)&w;
//	return p[0] | (p[1]<<8);
//}
//
//static inline unsigned long pswap4(long a) {
//	register unsigned char *p = (unsigned char *)a;
//	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
//}
//
//static inline unsigned long dswap4(unsigned long l) {
//	register unsigned char *p = (unsigned char *)&l;
//	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
//}
//#define GetDWord(a)		pswap2((long)a)
//#define GetDLong(a)		pswap4((long)a)
//#define Fetch(p)		*(p)
#endif


/////////////////////////////////////////////////////////////////////////////

#endif
