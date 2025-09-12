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

#ifndef _EMU86_SYNCPU_H
#define _EMU86_SYNCPU_H

/***************************************************************************/

#include "host.h"
#include "protmode.h"
#include <limits.h>
#include <stddef.h>

typedef struct {
/* offsets up to end_mark are 8-bit */
#define FIELD0		rzero	/* field of SynCPU at offset 00 */
/*00*/	unsigned int rzero;
/*04*/	int err2;
/*08*/	unsigned int reserved[1];
/*0c*/	unsigned int edi;
/*10*/	unsigned int esi;
/*14*/	unsigned int ebp;
/*18*/	unsigned int esp;
/*1c*/	unsigned int ebx;
/*20*/	unsigned int edx;
/*24*/	unsigned int ecx;
/*28*/	unsigned int eax;
/*2c*/	unsigned int eip;
/*30*/	unsigned int eflags;
/*34*/	unsigned short es, cs, ss, ds, fs, gs;
/*40*/	SDTR es_cache;
/*48*/	SDTR cs_cache;
/*50*/	SDTR ss_cache;
/*58*/	SDTR ds_cache;
/*60*/	SDTR fs_cache;
/*68*/	SDTR gs_cache;
/* ------------------------------------------------ */
/*70*/	unsigned short fpuc;
/* ------------------------------------------------ */
/*72*/	/*sig_atomic_t*/unsigned short sigalrm_pending;
/*74*/	unsigned int StackMask;
/*78*/ 	unsigned int df_increments; /* either 0x040201 or 0xfcfeff */
	/* begin of cr array */
/*7c*/	unsigned int cr[5]; /* only cr[0] is used in compiled code */
/* ------------------------------------------------ */
/*80*/	//unsigned int end_mark[0] = cr[1]
	unsigned int tr[2];

	unsigned int scp_err;
	int err;
	unsigned int mode;
	unsigned int sreg1;
	unsigned int dreg1;
	unsigned int xreg1;

	volatile /*sig_atomic_t*/unsigned sigprof_pending;

	unsigned short fpus;
	unsigned short fpstt, fptag;
	long double   *fpregs;

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
	unsigned int mem_ref;
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

	/* should be moved to TSS once implemented */
	struct revectored_struct int_revectored;

	/* if not NULL, points to emulated FPU state
	   if NULL, emulator uses FPU instructions, so flags that
	   dosemu needs to restore its own FPU environment. */
	emu_fpregset_t fpstate;
} SynCPU;

/* JIT uses byte offsets, make sure cr[0] is still ok */
static_assert(offsetof(SynCPU,cr[1]) == 128);

struct _SynCPU {
#ifdef X86_JIT
#define STUBS_LEN_MAX 16
	/* reserve space for max 16 stub functions used by cpatch */
	void (*stub_func[STUBS_LEN_MAX])(void);
#endif
	union {
		SynCPU s;
		unsigned char b[sizeof(SynCPU)];
		unsigned short w[sizeof(SynCPU)/2];
		unsigned int d[sizeof(SynCPU)/4];
	};
};

extern struct _SynCPU TheCPU_struct;
#define TheCPU TheCPU_struct.s

#define CPUOFFS(o)	(((unsigned char *)&(TheCPU.FIELD0))+o)

#define CPUBYTE(o)	TheCPU_struct.b[o]
#define CPUWORD(o)	TheCPU_struct.w[o/2]
#define CPULONG(o)	TheCPU_struct.d[o/4]

#define rEAX		TheCPU.eax
#define Ofs_EAX		(offsetof(SynCPU,eax))
#define rECX		TheCPU.ecx
#define Ofs_ECX		(offsetof(SynCPU,ecx))
#define rEDX		TheCPU.edx
#define Ofs_EDX		(offsetof(SynCPU,edx))
#define rEBX		TheCPU.ebx
#define Ofs_EBX		(offsetof(SynCPU,ebx))
#define rESP		TheCPU.esp
#define Ofs_ESP		(offsetof(SynCPU,esp))
#define rEBP		TheCPU.ebp
#define Ofs_EBP		(offsetof(SynCPU,ebp))
#define rESI		TheCPU.esi
#define Ofs_ESI		(offsetof(SynCPU,esi))
#define rEDI		TheCPU.edi
#define Ofs_EDI		(offsetof(SynCPU,edi))
#define Ofs_EIP		(offsetof(SynCPU,eip))

#define Ofs_CS		(offsetof(SynCPU,cs))
#define Ofs_DS		(offsetof(SynCPU,ds))
#define Ofs_ES		(offsetof(SynCPU,es))
#define Ofs_SS		(offsetof(SynCPU,ss))
#define Ofs_FS		(offsetof(SynCPU,fs))
#define Ofs_GS		(offsetof(SynCPU,gs))
#define Ofs_EFLAGS	(offsetof(SynCPU,eflags))
#define Ofs_CR0		(offsetof(SynCPU,cr[0]))
#define Ofs_STACKM	(offsetof(SynCPU,StackMask))
//#define Ofs_ETIME	(offsetof(SynCPU,EMUtime))
#define Ofs_RZERO	(offsetof(SynCPU,rzero))
#define Ofs_SIGAPEND	(offsetof(SynCPU,sigalrm_pending))
#define Ofs_DF_INCREMENTS (offsetof(SynCPU,df_increments))

#define Ofs_FPUC	(offsetof(SynCPU,fpuc))

// 'Base' is 1st field of xs_cache
#define Ofs_XDS		(offsetof(SynCPU,ds_cache))
#define Ofs_XSS		(offsetof(SynCPU,ss_cache))
#define Ofs_XES		(offsetof(SynCPU,es_cache))
#define Ofs_XCS		(offsetof(SynCPU,cs_cache))
#define Ofs_XFS		(offsetof(SynCPU,fs_cache))
#define Ofs_XGS		(offsetof(SynCPU,gs_cache))

#define Ofs_ERR		(offsetof(SynCPU,err2))
#define Ofs_int_revectored	(offsetof(SynCPU,int_revectored))

#define rAX		CPUWORD(Ofs_AX)
#define Ofs_AX		(Ofs_EAX)
#define Ofs_AXH		(Ofs_EAX+2)
#define rAL		CPUBYTE(Ofs_AL)
#define Ofs_AL		(Ofs_EAX)
#define rAH		CPUBYTE(Ofs_AH)
#define Ofs_AH		(Ofs_EAX+1)
#define rCX		CPUWORD(Ofs_CX)
#define Ofs_CX		(Ofs_ECX)
#define rCL		CPUBYTE(Ofs_CL)
#define Ofs_CL		(Ofs_ECX)
#define rCH		CPUBYTE(Ofs_CH)
#define Ofs_CH		(Ofs_ECX+1)
#define rDX		CPUWORD(Ofs_DX)
#define Ofs_DX		(Ofs_EDX)
#define rDL		CPUBYTE(Ofs_DL)
#define Ofs_DL		(Ofs_EDX)
#define rDH		CPUBYTE(Ofs_DH)
#define Ofs_DH		(Ofs_EDX+1)
#define rBX		CPUWORD(Ofs_BX)
#define Ofs_BX		(Ofs_EBX)
#define rBL		CPUBYTE(Ofs_BL)
#define Ofs_BL		(Ofs_EBX)
#define rBH		CPUBYTE(Ofs_BH)
#define Ofs_BH		(Ofs_EBX+1)
#define rSP		CPUWORD(Ofs_SP)
#define Ofs_SP		(Ofs_ESP)
#define rBP		CPUWORD(Ofs_BP)
#define Ofs_BP		(Ofs_EBP)
#define rSI		CPUWORD(Ofs_SI)
#define Ofs_SI		(Ofs_ESI)
#define rDI	        CPUWORD(Ofs_DI)
#define Ofs_DI		(Ofs_EDI)
#define Ofs_FLAGS	(Ofs_EFLAGS)
#define Ofs_FLAGSL	(Ofs_EFLAGS)

#define REG1		TheCPU.sreg1
#define REG3		TheCPU.dreg1
#define SBASE		TheCPU.xreg1
#define EFLAGS		TheCPU.eflags
#define FLAGS		CPUWORD(Ofs_EFLAGS)

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

#define sigalrm_pending() __atomic_load_n(&TheCPU.sigalrm_pending, \
  __ATOMIC_RELAXED)
#define sigalrm_pending_w(v) __atomic_store_n(&TheCPU.sigalrm_pending, v, \
  __ATOMIC_RELAXED)

#endif
