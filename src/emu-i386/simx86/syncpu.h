/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
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

#ifndef _EMU86_SYNCPU_H
#define _EMU86_SYNCPU_H

/***************************************************************************/

#include "host.h"
#include "protmode.h"
#include <limits.h>

#if ULONG_MAX > 0xffffffffUL
#define PADDING32BIT(n)
#else
#define PADDING32BIT(n) unsigned int padding##n;
#endif

typedef struct {
/* offsets are 8-bit signed */
#define FIELD0		unprotect_stub	/* field of SynCPU at offset 00 */
/* ------------------------------------------------ */
/*80*/  double   *fpregs;
/*84*/  PADDING32BIT(1)
/*88*/	unsigned long long EMUtime;
/*90*/	SDTR gs_cache;
/*9c*/	SDTR fs_cache;
/*a8*/	SDTR es_cache;
/*b4*/	SDTR ds_cache;
/*c0*/	SDTR cs_cache;
/*cc*/	SDTR ss_cache;
/* ------------------------------------------------ */
/*d8*/  void (*stub_stk_16)(void);
/*dc*/  PADDING32BIT(2)
/*e0*/  void (*stub_stk_32)(void);
/*e4*/  PADDING32BIT(3)
/*e8*/  void (*stub_wri_8)(void);
/*ec*/  PADDING32BIT(4)
/*f0*/  void (*stub_wri_16)(void);
/*f4*/  PADDING32BIT(5)
/*f8*/  void (*stub_wri_32)(void);
/*fc*/  PADDING32BIT(6)
/* ------------------------------------------------ */
/*00*/  void (*unprotect_stub)(void); /* must be at 0 for call (%ebx) */
/*04*/  PADDING32BIT(7)
/*08*/	unsigned int rzero;
/*0c*/	unsigned short gs, __gsh;
/*10*/	unsigned short fs, __fsh;
/*14*/	unsigned short es, __esh;
/*18*/	unsigned short ds, __dsh;
/*1c*/	unsigned int edi;
/*20*/	unsigned int esi;
/*24*/	unsigned int ebp;
/*28*/	unsigned int esp;
/*2c*/	unsigned int ebx;
/*30*/	unsigned int edx;
/*34*/	unsigned int ecx;
/*38*/	unsigned int eax;
/*3c*/	unsigned int trapno;
/*40*/	unsigned int scp_err;
/*44*/	unsigned int eip;
/*48*/	unsigned short cs, __csh;
/*4c*/	unsigned int eflags;
/* ----end of i386 sigcontext 1:1 correspondence--- */
/*50*/	unsigned short ss, __ssh;
/*54*/	unsigned int cr2;
/* ------------------------------------------------ */
/*58*/	unsigned short fpuc, fpus;
/*5c*/	unsigned short fpstt, fptag;
/*60*/	unsigned int _fni[3];
/* ------------------------------------------------ */
/*6c*/	unsigned short sigalrm_pending, sigprof_pending;
/*70*/		 int err;
/*74*/	unsigned int StackMask;
/*78*/	unsigned int  mode;
/*7c*/	unsigned int cr[5]; /* only cr[0] is used in compiled code */
/* ------------------------------------------------ */
/*90*/	unsigned int tr[2];

	unsigned int sreg1;
	unsigned int dreg1;
	unsigned int xreg1;
	unsigned int veflags;

/*
 * DR0-3 = linear address of breakpoint 0-3
 * DR4=5 = reserved
 * DR6	b0-b3 = BP active
 *	b13   = BD
 *	b14   = BS
 *	b15   = BT
 * DR7	b0-b1 = G:L bp#0
 *	b2-b3 = G:L bp#1
 *	b4-b5 = G:L bp#2
 *	b6-b7 = G:L bp#3
 *	b8-b9 = GE:LE
 *	b13   = GD
 *	b16-19= LLRW bp#0	LL=00(1),01(2),11(4)
 *	b20-23= LLRW bp#1	RW=00(x),01(w),11(rw)
 *	b24-27= LLRW bp#2
 *	b28-31= LLRW bp#3
 */
	unsigned int dr[8];
	unsigned long mem_ref;
/* CPU register: base(32) limit(16) */
	DTR  GDTR;
/* CPU register: base(32) limit(16) */
	DTR  IDTR;
/* CPU register: sel(16) base(32) limit(16) attr(8) */
	unsigned short LDT_SEL;
	DTR  LDTR;
/* CPU register: sel(16) base(32) limit(16) attr(8) */
	unsigned short TR_SEL;
	DTR  TR;

	void (*stub_stosb)(void);
	void (*stub_stosw)(void);
	void (*stub_stosl)(void);
	void (*stub_movsb)(void);
	void (*stub_movsw)(void);
	void (*stub_movsl)(void);
} SynCPU;

extern SynCPU TheCPU;

#ifndef offsetof
  #define offsetof(stru,member) ( (unsigned long) (&((stru *)0)->member) )
#endif

#define SCBASE		offsetof(SynCPU,FIELD0)

#define CPUOFFS(o)	(((char *)&(TheCPU.FIELD0))+(o))

#define CPUBYTE(o)	*((unsigned char *)CPUOFFS(o))
#define CPUWORD(o)	*((unsigned short *)CPUOFFS(o))
#define CPULONG(o)	*((unsigned int *)CPUOFFS(o))

#define rEAX		TheCPU.eax
#define Ofs_EAX		(char)(offsetof(SynCPU,eax)-SCBASE)
#define rECX		TheCPU.ecx
#define Ofs_ECX		(char)(offsetof(SynCPU,ecx)-SCBASE)
#define rEDX		TheCPU.edx
#define Ofs_EDX		(char)(offsetof(SynCPU,edx)-SCBASE)
#define rEBX		TheCPU.ebx
#define Ofs_EBX		(char)(offsetof(SynCPU,ebx)-SCBASE)
#define rESP		TheCPU.esp
#define Ofs_ESP		(char)(offsetof(SynCPU,esp)-SCBASE)
#define rEBP		TheCPU.ebp
#define Ofs_EBP		(char)(offsetof(SynCPU,ebp)-SCBASE)
#define rESI		TheCPU.esi
#define Ofs_ESI		(char)(offsetof(SynCPU,esi)-SCBASE)
#define rEDI		TheCPU.edi
#define Ofs_EDI		(char)(offsetof(SynCPU,edi)-SCBASE)

#define Ofs_CS		(char)(offsetof(SynCPU,cs)-SCBASE)
#define Ofs_DS		(char)(offsetof(SynCPU,ds)-SCBASE)
#define Ofs_ES		(char)(offsetof(SynCPU,es)-SCBASE)
#define Ofs_SS		(char)(offsetof(SynCPU,ss)-SCBASE)
#define Ofs_FS		(char)(offsetof(SynCPU,fs)-SCBASE)
#define Ofs_GS		(char)(offsetof(SynCPU,gs)-SCBASE)
#define Ofs_EFLAGS	(char)(offsetof(SynCPU,eflags)-SCBASE)
#define Ofs_ERR		(char)(offsetof(SynCPU,err)-SCBASE)
#define Ofs_CR0		(char)(offsetof(SynCPU,cr[0])-SCBASE)
#define Ofs_CR2		(char)(offsetof(SynCPU,cr2)-SCBASE)
#define Ofs_STACKM	(char)(offsetof(SynCPU,StackMask)-SCBASE)
#define Ofs_ETIME	(char)(offsetof(SynCPU,EMUtime)-SCBASE)
#define Ofs_RZERO	(char)(offsetof(SynCPU,rzero)-SCBASE)
#define Ofs_SIGAPEND	(char)(offsetof(SynCPU,sigalrm_pending)-SCBASE)
#define Ofs_SIGFPEND	(char)(offsetof(SynCPU,sigprof_pending)-SCBASE)

#define Ofs_FPR		(char)(offsetof(SynCPU,fpregs)-SCBASE)
#define Ofs_FPSTT	(char)(offsetof(SynCPU,fpstt)-SCBASE)
#define Ofs_FPUS	(char)(offsetof(SynCPU,fpus)-SCBASE)
#define Ofs_FPUC	(char)(offsetof(SynCPU,fpuc)-SCBASE)
#define Ofs_FPTAG	(char)(offsetof(SynCPU,fptag)-SCBASE)

// 'Base' is 1st field of xs_cache
#define Ofs_XDS		(char)(offsetof(SynCPU,ds_cache)-SCBASE)
#define Ofs_XSS		(char)(offsetof(SynCPU,ss_cache)-SCBASE)
#define Ofs_XES		(char)(offsetof(SynCPU,es_cache)-SCBASE)
#define Ofs_XCS		(char)(offsetof(SynCPU,cs_cache)-SCBASE)
#define Ofs_XFS		(char)(offsetof(SynCPU,fs_cache)-SCBASE)
#define Ofs_XGS		(char)(offsetof(SynCPU,gs_cache)-SCBASE)

#define Ofs_stub_wri_8	(char)(offsetof(SynCPU,stub_wri_8)-SCBASE)
#define Ofs_stub_wri_16	(char)(offsetof(SynCPU,stub_wri_16)-SCBASE)
#define Ofs_stub_wri_32	(char)(offsetof(SynCPU,stub_wri_32)-SCBASE)
#define Ofs_stub_stk_16	(char)(offsetof(SynCPU,stub_stk_16)-SCBASE)
#define Ofs_stub_stk_32	(char)(offsetof(SynCPU,stub_stk_32)-SCBASE)
#define Ofs_stub_movsb	(int)(offsetof(SynCPU,stub_movsb)-SCBASE)
#define Ofs_stub_movsw	(int)(offsetof(SynCPU,stub_movsw)-SCBASE)
#define Ofs_stub_movsl	(int)(offsetof(SynCPU,stub_movsl)-SCBASE)
#define Ofs_stub_stosb	(int)(offsetof(SynCPU,stub_stosb)-SCBASE)
#define Ofs_stub_stosw	(int)(offsetof(SynCPU,stub_stosw)-SCBASE)
#define Ofs_stub_stosl	(int)(offsetof(SynCPU,stub_stosl)-SCBASE)

#define rAX		*((unsigned short *)&rEAX)
#define Ofs_AX		(Ofs_EAX)
#define Ofs_AXH		(Ofs_EAX+2)
#define rAL		((unsigned char *)&rEAX)[0]
#define Ofs_AL		(Ofs_EAX)
#define rAH		((unsigned char *)&rEAX)[1]
#define Ofs_AH		(Ofs_EAX+1)
#define rCX		*((unsigned short *)&rECX)
#define Ofs_CX		(Ofs_ECX)
#define rCL		((unsigned char *)&rECX)[0]
#define Ofs_CL		(Ofs_ECX)
#define rCH		((unsigned char *)&rECX)[1]
#define Ofs_CH		(Ofs_ECX+1)
#define rDX		*((unsigned short *)&rEDX)
#define Ofs_DX		(Ofs_EDX)
#define rDL		((unsigned char *)&rEDX)[0]
#define Ofs_DL		(Ofs_EDX)
#define rDH		((unsigned char *)&rEDX)[1]
#define Ofs_DH		(Ofs_EDX+1)
#define rBX		*((unsigned short *)&rEBX)
#define Ofs_BX		(Ofs_EBX)
#define rBL		((unsigned char *)&rEBX)[0]
#define Ofs_BL		(Ofs_EBX)
#define rBH		((unsigned char *)&rEBX)[1]
#define Ofs_BH		(Ofs_EBX+1)
#define rSP		*((unsigned short *)&rESP)
#define Ofs_SP		(Ofs_ESP)
#define rBP		*((unsigned short *)&rEBP)
#define Ofs_BP		(Ofs_EBP)
#define rSI		*((unsigned short *)&rESI)
#define Ofs_SI		(Ofs_ESI)
#define rDI		*((unsigned short *)&rEDI)
#define Ofs_DI		(Ofs_EDI)
#define Ofs_FLAGS	(Ofs_EFLAGS)
#define Ofs_FLAGSL	(Ofs_EFLAGS)

#define REG1		TheCPU.sreg1
#define REG3		TheCPU.dreg1
#define SBASE		TheCPU.xreg1
#define SIGAPEND	TheCPU.sigalrm_pending
#define SIGFPEND	TheCPU.sigprof_pending
#define MEMREF		TheCPU.mem_ref
#define EFLAGS		TheCPU.eflags
#define FLAGS		*((unsigned short *)&(TheCPU.eflags))
#define eVEFLAGS	TheCPU.veflags
#define FPX		TheCPU.fpstt

#define CS_DTR		TheCPU.cs_cache
#define DS_DTR		TheCPU.ds_cache
#define ES_DTR		TheCPU.es_cache
#define SS_DTR		TheCPU.ss_cache
#define FS_DTR		TheCPU.fs_cache
#define GS_DTR		TheCPU.gs_cache

#define LONG_CS		TheCPU.cs_cache.BoundL
#define LONG_DS		TheCPU.ds_cache.BoundL
#define LONG_ES		TheCPU.es_cache.BoundL
#define LONG_SS		TheCPU.ss_cache.BoundL
#define LONG_FS		TheCPU.fs_cache.BoundL
#define LONG_GS		TheCPU.gs_cache.BoundL

extern char OVERR_DS, OVERR_SS;

#endif
