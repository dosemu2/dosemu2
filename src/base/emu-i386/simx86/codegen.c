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
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

/*
 * BACK-END for the cpuemu interpreter.
 *
 * There are two main functions here:
 *	AddrGen, which implements the AGU (Address Generation Unit).
 *		It calculates the address coming from ModRM and stores
 *		it into a well-defined register (edi in the x86 case)
 *	Gen, which does the ALU work and all the rest. There is no
 *		branch specific unit, as the branches are (in principle)
 *		all interpreted.
 * Both functions use a variable parameter approach, just to make them
 *	hard to follow ;-)
 *
 */

//

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "utilities.h"
#include "emu86.h"
#include "codegen-arch.h"

unsigned char * (*CodeGen)(unsigned char *CodePtr, unsigned char *BaseGenBuf, const IGen *IG);
unsigned (*Exec)(unsigned *mem_ref, unsigned long *flg,
		 unsigned char *ecpu, void *SeqStart,
		 unsigned short seqflg, unsigned *seqbase);

int UseLinker = 0;
hitimer_u TimeStartExec;

/////////////////////////////////////////////////////////////////////////////

/*
 * This function is only here for looking at the generated binary code
 * with objdump.
 */
static void _test_(void) __attribute__((used));

static void _test_(void)
{
	__asm__ __volatile__ (" \
		nop \
		" : : : "memory" );
}

/////////////////////////////////////////////////////////////////////////////

static int goodmemref(dosaddr_t m)
{
	if (m < 0x110000) return 1;
	if (dpmi_is_valid_range(m, 16))
		return 1;
	return 0;
}

/////////////////////////////////////////////////////////////////////////////


void InitGen(void)
{
	UseLinker = USE_LINKER;

	Fetch = jit_fetch_byte;
	FetchW = jit_fetch_word;
	FetchL = jit_fetch_dword;

#ifdef X86_JIT
	if (!CONFIG_CPUSIM)
		InitGen_x86();
	else
#endif
		InitGen_sim();
}


/////////////////////////////////////////////////////////////////////////////

/*
 * address generator unit
 * careful - do not use eax, and NEVER change any flag!
 */
void AddrGen(int op, int mode, ...)
{
	va_list	ap;
	IMeta *I;
	IGen *IG;
#if PROFILE >= 2
	hitimer_t t0 = 0;
	if (debug_level('e')) t0 = GETTSC();
#endif

	I = &InstrMeta[CurrIMeta];
	if (I->ngen >= NUMGENS) { leavedos_main(0xbac1); return; }
	IG = &(I->gen[I->ngen]);
	if (debug_level('e')>6) dbug_printf("AGEN: %3d %6x\n",op,mode);

	va_start(ap, mode);
	IG->op = op;
	IG->mode = mode;

	switch(op) {
	case A_DI_0:			// base(32), imm
	case A_DI_1: {			// base(32), {imm}, reg, {shift}
		IG->p0 = va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		if (op==A_DI_0)	break;
		IG->p2 = va_arg(ap,unsigned int);
		}
		break;
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
		unsigned char sh;
		IG->p0 = va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		IG->p2 = va_arg(ap,unsigned int);
		IG->p3 = va_arg(ap,unsigned int);
		sh = (unsigned char)(va_arg(ap,unsigned int));
		IG->p4 = sh;
		}
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
		unsigned char sh;
		IG->p0 = va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		IG->p2 = va_arg(ap,unsigned int);
		sh = (unsigned char)(va_arg(ap,unsigned int));
		IG->p3 = sh;
		}
		break;
	case A_SR_PROT:		// prot mode make base addr from seg
	case A_SR_SH4:		// real mode make base addr from seg
		IG->p0 = va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		break;
	}
	va_end(ap);
	I->ngen++;
#if PROFILE >= 2
	if (debug_level('e')) GenTime += (GETTSC() - t0);
#endif
}


void Gen(int op, int mode, ...)
{
	int rcod=0;
	va_list	ap;
	IMeta *I;
	IGen *IG;
#if PROFILE >= 2
	hitimer_t t0 = 0;
	if (debug_level('e')) t0 = GETTSC();
#endif

	I = &InstrMeta[CurrIMeta];
	if (I->ngen >= NUMGENS) leavedos_main(0xbac2);
	IG = &(I->gen[I->ngen]);
	if (debug_level('e')>6) dbug_printf("CGEN: %3d %6x\n",op,mode);

	va_start(ap, mode);
	IG->op = op;
	IG->mode = mode;

	switch(op) {
	case L_NOP:
	case L_CR0:
	case L_DI_R1:
	case L_LXS2:	/* load segment value from mem +2/4 */
	case S_DI:
	case O_NOT:
	case O_NEG:
	case O_INC:
	case O_DEC:
	case O_MUL:
	case O_CBWD:
	case O_PUSH:
	case O_PUSH1:
	case O_PUSH2F:
	case O_PUSH3:
	case O_POP1:
	case O_POP3:
	case O_LEAVE:
	case O_MOVS_MovD:
	case O_MOVS_LodD:
	case O_MOVS_StoD:
	case O_MOVS_ScaD:
	case O_MOVS_CmpD:
	case O_RDTSC:
		break;

	case L_REG:
	case S_REG:
	case S_DI_R:
	case L_LXS1:
	case O_XCHG:
	case O_CLEAR:
	case O_TEST:
	case O_SBSELF:
	case O_BSWAP:
	case O_XLAT:
	case O_MOVS_SetA:
	case O_MOVS_SavA:
	case O_CMPXCHG:
	case S_DI_IMM:
	case L_IMM_R1:
	case O_ADD_R:				// acc = acc op	reg
	case O_OR_R:
	case O_ADC_R:
	case O_SBB_R:
	case O_AND_R:
	case O_SUB_R:
	case O_XOR_R:
	case O_CMP_R:
	case O_INC_R:
	case O_DEC_R:
	case O_DIV:
	case O_IDIV:
	case O_PUSHI:
	case O_PUSH2:
	case JMP_TAILCODE:
		IG->p0 = va_arg(ap,unsigned int);
		break;

	case O_POP2:
		if (!(mode & MNOREG) || (mode & MRETISP))
			IG->p0 = va_arg(ap,unsigned int);
		break;

	case L_REG2REG:
	case O_XCHG_R:
	case L_IMM:
	case L_MOVZS:
		IG->p0 = va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		break;

	case O_ADD_FR:				// reg = reg op	acc/imm
	case O_OR_FR:
	case O_ADC_FR:
	case O_SBB_FR:
	case O_AND_FR:
	case O_SUB_FR:
	case O_XOR_FR:
	case O_CMP_FR:
		IG->p0 = va_arg(ap,unsigned int);
		if (mode & IMMED)
			IG->p1 = va_arg(ap,unsigned int);
		break;

	case O_IMUL:
		if (mode&IMMED) {
			IG->p0 = va_arg(ap,unsigned int);
			IG->p1 = va_arg(ap,unsigned int);
		}
		if (!(mode&MBYTE)) {
			if (mode&MEMADR) {
				IG->p0 = va_arg(ap,unsigned int);
			}
		}
		break;

	case O_ROL:
	case O_ROR:
	case O_RCL:
	case O_RCR:
	case O_SHL:
	case O_SHR:
	case O_SAR:
		if (mode & IMMED) {
			unsigned char sh = (unsigned char)va_arg(ap,unsigned int);
			IG->p0 = sh;
		}
		break;

	case O_OPAX: {	/* used by DAA..AAD */
			int n =	va_arg(ap,unsigned int);
			IG->p0 = n;
			// get n>0,n<3 bytes from parameter stack
			IG->p1 = va_arg(ap,unsigned int);
			if (n==2) IG->p2 = va_arg(ap,unsigned int);
		}
		break;

	case O_POP:
		if (mode & MRETISP) IG->p0 = va_arg(ap,unsigned int);
		break;


	case O_SLAHF:
		rcod = va_arg(ap,unsigned int)&1;	// 0=LAHF 1=SAHF
		IG->p0 = rcod;
		break;

	case O_SETFL:
	case O_SETCC: {
		unsigned char n = (unsigned char)va_arg(ap,unsigned int);
		IG->p0 = n;
		}
		break;

	case O_FOP:
		I->flags |= F_FPOP;
		// fall through
	case O_INT: {
		unsigned char exop = (unsigned char)va_arg(ap,unsigned int);
		IG->p0 = exop;
		IG->p1 = va_arg(ap,unsigned int);	// reg
		}
		break;

	case O_BITOP:
		IG->p0 = (unsigned char)va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		break;

	case O_SHFD:
		IG->p0 = (unsigned char)va_arg(ap,unsigned int)&8;
		IG->p1 = va_arg(ap,unsigned int);
		if (mode & IMMED) {
			unsigned char shc = (unsigned char)va_arg(ap,unsigned int)&0x1f;
			IG->p2 = shc;
		} break;

	case O_SIM:		// opc, arg, P0
		IG->p0 = va_arg(ap,unsigned int);
		IG->p1 = va_arg(ap,unsigned int);
		IG->p2 = va_arg(ap,unsigned int);
		break;

	case JMP_INDIRECT:
		break;

	case JMP_LINK:		// dspt
		IG->p0 = va_arg(ap,unsigned int);	// dspt
		IG->p1 = va_arg(ap,unsigned int);	// PC of node
		break;

	case JLOOP_LINK:
	case JF_LINK:
	case JB_LINK: {		// opc
		unsigned char opc = (unsigned char)va_arg(ap,unsigned int);
		IG->p0 = opc;
		}
		break;
	}

	va_end(ap);
	I->ngen++;
#if PROFILE >= 2
	if (debug_level('e')) GenTime += (GETTSC() - t0);
#endif
}


/////////////////////////////////////////////////////////////////////////////

static void OptimizeCode(IMeta *I)
{
	/* check if we can optimize away the first intermediate op
	   of the next instruction against the last intermediate op
	   of the present instruction */
	IMeta *I1 = I + 1;
	IGen *lastIG = &I->gen[I->ngen-1];
	IGen *firstIG = &I1->gen[0];

	if ((lastIG->mode & MOPT) && (firstIG->mode & MOPT) &&
	    ((lastIG->op == O_PUSH3 && firstIG->op == O_PUSH1) ||
	     (lastIG->op == O_POP3  && firstIG->op == O_POP1))) {
		I->ngen--;
		I1->ngen--;
		memmove(I1->gen, I1->gen+1, I1->ngen * sizeof(IGen));
		if (debug_level('e')>1)
			e_printf("Optimized op=%x at PC=%x\n",
				 firstIG->op, I1->npc);
	}
}

static unsigned char *ProduceCode(unsigned int PC, IMeta *I0)
{
	int i,j,mall_req;
	unsigned char *cp, *cp1, *BaseGenBuf, *CodePtr;
	size_t GenBufSize;

	if (debug_level('e')>1) {
	    e_printf("---------------------------------------------\n");
	    e_printf("ProduceCode: CurrIMeta=%d\n",CurrIMeta);
	}
	if (CurrIMeta < 0) leavedos_main(0xbac3);

	/* allocate the actual code buffer here; size is a worst-case
	 * estimate based on measured bytes per opcode.
	 *
	 * GenBufSize contain a first guess of the amount of space required
	 *
	 */
	GenBufSize = 0;
	for (i=0; i<=CurrIMeta; i++)
	    GenBufSize += I0[i].ngen * MAX_GEND_BYTES_PER_OP;
	mall_req = GenBufSize + 32;// 32 for tail
	BaseGenBuf = AllocGenCodeBuf(mall_req);
	/* actual code buffer starts from here */
	CodePtr = BaseGenBuf;
	I0->daddr = 0;
	if (debug_level('e')>1)
	    e_printf("CodeBuf=%p siz %zd\n",BaseGenBuf,GenBufSize);

	for (i=0; i<=CurrIMeta; i++) {
	    IMeta *I = &I0[i];
	    if (i < CurrIMeta)
		OptimizeCode(I);
	    cp = cp1 = CodePtr;
	    I->daddr = cp - BaseGenBuf;
	    for (j=0; j<I->ngen; j++) {
		IGen *IG = &I->gen[j];
		if (IG->op == JMP_LINK)
			IG->p2 = CodePtr - BaseGenBuf;
		CodePtr = CodeGen(CodePtr, BaseGenBuf, IG);
		if (CodePtr-cp1 > MAX_GEND_BYTES_PER_OP) {
		    dosemu_error("Generated code (%zd bytes) overflowed into buffer, please "
				 "increase MAX_GEND_BYTES_PER_OP=%d\n",
				 CodePtr-cp1, MAX_GEND_BYTES_PER_OP);
		    leavedos_main(0x535347);
		}
		if (debug_level('e')>7) {
		    int dg = CodePtr-cp1;
		    e_printf("PGEN(%02d,%02d) %3d %6x %2d %08x %08x %08x %08x %08x\n",
			i,j,IG->op,IG->mode,dg,
			IG->p0,IG->p1,IG->p2,IG->p3,IG->p4);
		}
		cp1 = CodePtr;
	    }
	    I->len = CodePtr - cp;
	    I0->flags |= I->flags;
	    if (debug_level('e')>3) {
		if (debug_level('e')>4) {
		    e_printf("Metadata %03d PC=%08x flags=%x(%x) ng=%d\n",
			     i,I->npc,I->flags,I0->flags,I->ngen);
		}
		GCPrint(cp, BaseGenBuf, I->len);
	    }
	}
	if (debug_level('e')>1)
	    e_printf("Size=%td guess=%zd\n",(CodePtr-BaseGenBuf),GenBufSize);
/**/ if ((CodePtr-BaseGenBuf) > GenBufSize) leavedos_main(0x535347);
	I0->seqlen  = PC - I0->npc;

	if (debug_level('e')>1)
	    e_printf("---------------------------------------------\n");

	I0->totlen = CodePtr - BaseGenBuf;

	/* shrink buffer to what is actually needed */
	mall_req = I0->totlen;
	ShrinkGenCodeBuf(BaseGenBuf, mall_req);
	if (debug_level('e')>3)
		e_printf("Seq len %#x:%#x\n",I0->seqlen,I0->totlen);

	return BaseGenBuf;
}


/////////////////////////////////////////////////////////////////////////////
/*
 * These are the functions which actually executes the generated code.
 *
 * There are two paths:
 * 1) for CloseAndExec we are ending a code generation phase, and our code
 *	is still in the CodeBuf together with all its detailed info stored
 *	in InstrMeta. First we close the sequence adding the TailCode;
 *	it, and move it to the collecting tree and clear the temporary
 *	structures. Then, in DoExec we execute the code.
 *	The PC parameter is the address in the source code of the next
 *	instruction following the end of the code block. It will be stored
 *	into the TailCode of the block.
 * 2) We are executing a sequence found in the collecting tree.
 *	DoExec is called directly.
 *	G is the node we found (possibly the start of a chain of linked
 *	code sequences).
 *
 * When the code is executed, it returns in eax the source address of the
 * next instruction to find/parse.
 *
 */

TNode *Close(unsigned int PC, unsigned int Interp_LONG_CS, int mode,
		int flags)
{
	IMeta *I0;
	TNode *G;
	unsigned char *GenCodeBuf;

	assert (CurrIMeta >= 0);

	// we're creating a new node
	I0 = &InstrMeta[0];

	if (debug_level('e')>2) {
		e_printf("==== Closing sequence at %08x\n", PC);
	}

	GenCodeBuf = ProduceCode(PC, I0);

	NodesParsed++;
#if PROFILE
	if (debug_level('e')) TotalNodesParsed++;
#endif
	G = Move2Tree(I0, GenCodeBuf);		/* when is G==NULL? */
	/* InstrMeta will be zeroed at this point */
	/* mprotect the page here; a page fault will be triggered
	 * if some other code tries to write over the page including
	 * this node */
	e_markpage(G->key, G->seqlen);
	G->cs = Interp_LONG_CS;
	G->mode = mode;
	/* check links INSIDE current node */
	if (!(mode & MSSTP)) {
		NodeLinker(G, G);
	}

	/* must be done after a_markpage() to avoid excessive m_unprots */
	if (!(flags & (F_PREJ | F_SPRJ)))
		tree_gc();
	return G;
}

#ifdef __i386__
  __attribute__((target("sse")))
#endif
static inline void prefetch(unsigned char *p)
{
	__builtin_prefetch(p);
}

static unsigned int Exec_pre(unsigned char *ecpu)
{
	unsigned long flg;

	/* get the protected mode flags. Note that RF and VM are cleared
	 * by pushfd (but not by ints and traps) */
	flg = getflags();

	/* pass TF=0, IF=1, DF=0 */
	flg = (flg & ~(EFLAGS_CC|EFLAGS_IF|EFLAGS_DF|EFLAGS_TF)) |
	       (EFLAGS & EFLAGS_CC) | EFLAGS_IF;

#ifdef __i386__
	if (config.cpuprefetcht0)
#endif
		prefetch(ecpu);

	return flg;
}

static void Exec_post(unsigned long flg, unsigned int mem_ref)
{
	EFLAGS = (EFLAGS & ~EFLAGS_CC) | (flg &	EFLAGS_CC);
	TheCPU.mem_ref = mem_ref;
}

static unsigned ExecOne(TNode *G, unsigned *mem_ref, unsigned long *flg,
		unsigned char *ecpu, unsigned int *pLastXKey)
{
	unsigned int ePC;
	/*
	 * Just before execution comes the linker stage.
	 * So the order is:
	 *	1) build code sequence in the IMeta buffer
	 *	2) move buffer to a newly allocated node in the tree
	 *	3) link the previous node to this node
	 *	4) execute it, always returning back at the end
	 * Linking: a node is linked with the next
	 * one in execution order, provided that the end source address
	 * of the preceding node matches the start source address of the
	 * following (i.e. no interpreted instructions in between).
	 *
	 * A complication here is that the previous node may already
	 * be linked, so an unlinked exit will return the start PC
	 * of the original (unlinked) block in seqbase.
	 * Unlinkable exits are flagged using ePC==LastXKey; self-links
	 * are already handled at the compile stage.
	 */
	/* check links FROM LastXNode TO current node */
	if (*pLastXKey != G->key)
		NodeLinker(FindTree(*pLastXKey), G);
	ePC = Exec(mem_ref, flg, ecpu, G->addr, G->flags, pLastXKey);
#ifdef SKIP_EMU_VBIOS
	if ((TheCPU.cs&0xf000)==config.vbios_seg && !TheCPU.err)
		TheCPU.err = EXCP_GOBACK;
#endif
	return ePC;
}

static void HandleEmuSignals(void)
{
	int e = exit_pending_xchg(0);
#if PROFILE
	if (debug_level('e')) EmuSignals++;
#endif
	//XXX need to figure out something for DRTRAP
	//if (TheCPU.dr[7] & 0xff) {
	//	if (e_debug_check(PC)) {
	//		TheCPU.err = EXCP01_SSTP;
	//	}
	//}
	if (e & exit_SIGPEND) {
		/* force exit after signal */
		TheCPU.err=EXCP_SIGNAL;
	}
	else if (e & exit_RPIC) {
		/* force exit for PIC */
		if (EFLAGS & EFLAGS_IF)
			TheCPU.err=EXCP_PICSIGNAL;
	}
	else if (e & exit_STI) {
		/* force exit for IF set */
		TheCPU.err=EXCP_STISIGNAL;
	}
}

unsigned int DoExec(TNode *G, unsigned *pLastXKey)
{
	unsigned long flg;
	unsigned char *ecpu;
	unsigned int mem_ref;
	unsigned int ePC;
	unsigned short seqflg = G->flags;
#if defined(SINGLESTEP)
        unsigned int key = G->key;
#endif

	ecpu = CPUOFFS(0);
	if (debug_level('e')>1) {
		unsigned e = exit_pending();
		if (e & exit_SIGPEND) e_printf("** SIGALRM is pending\n");
		if (e & exit_RPIC) e_printf("** PIC is pending\n");
		e_printf("==== Executing code at %p flg=%04x\n",
			G->addr,seqflg);
	}
#ifdef ASM_DUMP
	fprintf(aLog,"%p: exec\n",G->key);
#endif
#if PROFILE >= 2
	hitimer_t TimeStartExec;
	if (debug_level('e')) TimeStartExec = GETTSC();
#endif
	/* these flags need to be checked only once for the node */
	G->flags &= ~(F_SPEC|F_LEAV);
	flg = Exec_pre(ecpu);
#if !defined(ASM_DUMP) && !defined(SINGLESTEP)
	/* try fast inner loop if nothing special is going on */
	if (!(seqflg & (F_SPEC|F_LEAV)) && !debug_level('e')) {
		while (1) {
			ePC = ExecOne(G, &mem_ref, &flg, ecpu, pLastXKey);
			if (TheCPU.err || exit_pending())
				break;
			G = FindTree(ePC);
			if (!G || !GoodNode(G))
				break;
		}
	} else
#endif
		ePC = ExecOne(G, &mem_ref, &flg, ecpu, pLastXKey);
	// G is unreliable (maybe deleted) past this point!
	Exec_post(flg, mem_ref);

	if (debug_level('e')) {
#if PROFILE >= 2
	    ExecTime += GETTSC() - TimeStartExec;
#endif
	    if (debug_level('e')>1) {
		if (debug_level('e')>2 && *pLastXKey != ePC)
			e_printf("New LastXKey=%08x\n",*pLastXKey);
		e_printf("** End code, PC=%08x sig=%x\n",ePC,
		    exit_pending());
		if ((debug_level('e')>3) && (seqflg & F_FPOP)) {
		    e_printf("  %s\n", e_trace_fp());
		}
		/* DANGEROUS - can crash dosemu! */
		if ((debug_level('e')>4) && goodmemref(mem_ref)) {
		    e_printf("*mem_ref [%#08x] = %08x\n",mem_ref,
			     READ_DWORD(mem_ref));
		}
	    }
	}
	/* exit_pending at this point is non-zero if there was ANY signal,
	 * not just a SIGALRM
	 */
	if (!TheCPU.err && exit_pending()) {
		/* checking for infinite loops, flagged in JumpGen() */
		/* TheCPU.eip points to the first instruction of the last
		   executed block, except for real-mode retf, which cannot
		   cause forever loops */
		if (!(EFLAGS & (VIF|IF|TF))) {
			TNode *LastG = FindTree(LONG_CS + TheCPU.eip);
			seqflg = LastG ? LastG->flags : 0;
			if (seqflg & F_SLFJ) {
				error("!Forever loop!\n");
				leavedos_main(0xebfe);
			}
		}
		HandleEmuSignals();
	}

#if defined(SINGLESTEP)
	InvalidateNodeRange(key, 1, NULL);
	avltr_delete(key);
#endif

	return ePC;
}

#define fB(c,x) (((uint8_t *)(c))[(x) & ~_PAGE_MASK])
static inline uint16_t fW(void *c, dosaddr_t x)
{
	if (x & 1)
		return ((fB(c, x + 1) << 8) | fB(c, x));
	return (((uint16_t *)(c))[(x & ~_PAGE_MASK) >> 1]);
}
static inline uint32_t fD(void *c, dosaddr_t x)
{
	if (x & 3)
		return (((uint32_t)fW(c, x + 2) << 16) | fW(c, x));
	return (((uint32_t *)(c))[(x & ~_PAGE_MASK) >> 2]);
}

uint8_t jit_fetch_byte(dosaddr_t x)
{
	void *cache[2];

	e_fetch(x, 1, cache);
	return fB(cache[0], x);
}

uint16_t jit_fetch_word(dosaddr_t x)
{
	void *cache[2];

	if ((x & ~CGRMASK) != ((x + 1) & ~CGRMASK))
		return ((jit_fetch_byte(x + 1) << 8) | jit_fetch_byte(x));
	e_fetch(x, 2, cache);
	if ((x & _PAGE_MASK) != ((x + 1) & _PAGE_MASK))
		return ((fB(cache[1], x + 1) << 8) | fB(cache[0], x));
	return fW(cache[0], x);
}

uint32_t jit_fetch_dword(dosaddr_t x)
{
	void *cache[2];

	if ((x & ~CGRMASK) != ((x + 3) & ~CGRMASK))
		return (((uint32_t)jit_fetch_word(x + 2) << 16) |
				jit_fetch_word(x));
	e_fetch(x, 4, cache);
	if ((x & _PAGE_MASK) != ((x + 3) & _PAGE_MASK)) {
		if (!(x & 1))
			return (((uint32_t)fW(cache[1], x + 2) << 16) |
					fW(cache[0], x));
		if (x & 2)
			return (((fD(cache[1], x + 1) & 0xffffff) << 8) |
					fB(cache[0], x));
		return (((uint32_t)fB(cache[1], x + 3) << 24) |
					(fD(cache[0], x - 1) >> 8));
	}
	return fD(cache[0], x);
}

/////////////////////////////////////////////////////////////////////////////
