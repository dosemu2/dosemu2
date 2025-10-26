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

#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "emu86.h"
#include "codegen.h"
#include "vgaemu.h"
#include "port.h"
#include "emudpmi.h"
#include "mhpdbg.h"
#include "video.h"
#include "utilities.h"

#if PROFILE
int EmuSignals = 0;
#endif

/* this is probably unsafe with cpatch */
#define SPEC_PREJIT 0

static pthread_cond_t run_cnd = PTHREAD_COND_INITIALIZER;
static int prejit_running;
static pthread_mutex_t run_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t prejit_thr;
static sem_t prejit_sem;
static void *prejit_thread(void *arg);
#if SPEC_PREJIT
static int can_speculate(void);
static void prejit_run(TNode *G);
#else
#define can_speculate() 0
#endif
static TNode *prejit_G;
static unsigned short prejit_cs;
#if PROFILE
int SpecPrejits;
#endif

/* countdown to exit after handling VGAEMU faults, reset by
   planar VGA reads and writes */
static int interp_inst_emu_count;

static int ArOpsR[] =
	{ O_ADD_R, O_OR_R, O_ADC_R, O_SBB_R, O_AND_R, O_SUB_R, O_XOR_R, O_CMP_R };
static int ArOpsFR[] =
	{ O_ADD_FR, O_OR_FR, O_ADC_FR, O_SBB_FR, O_AND_FR, O_SUB_FR, O_XOR_FR, O_CMP_FR};
static char R1Tab_b[8] =
	{ Ofs_AL, Ofs_CL, Ofs_DL, Ofs_BL, Ofs_AH, Ofs_CH, Ofs_DH, Ofs_BH };
static char R1Tab_l[14] =
	{ Ofs_EAX, Ofs_ECX, Ofs_EDX, Ofs_EBX, Ofs_ESP, Ofs_EBP, Ofs_ESI, Ofs_EDI,
	  Ofs_ES,  Ofs_CS,  Ofs_SS,  Ofs_DS,  Ofs_FS,  Ofs_GS };

#define SEL_B_X(r)		R1Tab_b[(r)]

#define INC_WL_PCA(m,i)	PC=(PC+(i)+BT24(BitADDR16, m))
#define INC_WL_PC(m,i)	PC=(PC+(i)+BT24(BitDATA16, m))

/*
 * close any pending instruction in the code cache.
 * P0 is the start of current code cache.
 * PC is the address of the next instruction after the sequence stops.
 * The compiled code is then moved into the tree and can be picked up
 * by the next DoExec.
 */
static TNode *DoClose(unsigned int PC, unsigned int Interp_LONG_CS, int mode,
		int flags)
{
	unsigned int P0 = InstrMeta[0].npc;

	/* If the code doesn't terminate with a jump/loop instruction
	 * it still lacks the tail code; add it here */
	IMeta *GL = &InstrMeta[CurrIMeta];
	if (GL->gen[GL->ngen-1].op < JMP_TAILCODE) {
		Gen(JMP_TAILCODE, mode, PC);
	}

	assert(PC > P0);
	if (e_querymark(P0, PC - P0)) {
	    InvalidateNodeRange(P0, PC - P0, NULL);
	    if (!e_querymprotrange_full(P0, PC - P0)) {
		unsigned int abeg, aend;

		/* in case of prejit, the page content could
		 * be altered, so we better re-jit */
		if (flags & (F_PREJ | F_SPRJ))
			return NULL;
		abeg = P0 & _PAGE_MASK;
		aend = PC & _PAGE_MASK;
		/* re-populate cache */
		for (; abeg <= aend; abeg += PAGE_SIZE)
			Fetch(abeg);
	    }
	}
	return Close(PC, Interp_LONG_CS, mode, flags);
}

static inline unsigned int UNPREFIX(unsigned int m)
{
	return (m&~(DATA16|ADDR16))|((m&MBIGCS)?0:(DATA16|ADDR16));
}


/////////////////////////////////////////////////////////////////////////////
//
// jmp		b8 j j j j 5a c3
//	link	e9 l l l l -- --
// jcc  	7x 06 b8 a a a a 5a c3 b8 b b b b 5a c3
//	link	7x 06 e9 l l l l -- -- e9 l l l l -- --
//

static unsigned int JumpGen(unsigned int P2, unsigned int Interp_LONG_CS,
			    int mode, int opc, int pskip)
{
#if !defined(SINGLESTEP)
	unsigned int P1;
#endif
	int dsp;
	unsigned int d_t, d_nt, j_t, j_nt;
	unsigned int PC;

	/* pskip points to start of next instruction
	 * dsp is the displacement relative to this jump instruction,
	 * some cases:
	 *	eb 00	dsp=2	jmp to next inst (dsp==pskip)
	 *	eb ff	dsp=1	illegal or tricky
	 *	eb fe	dsp=0	loop forever
	 */
	switch(opc) {
	case RET: case RETisp: case JMPi: case CALLi:
	case RETl: case RETlisp: case JMPli: case CALLli:
	case IRET: case INT: // indirect jumps
		dsp = 0;
		j_t = 0;
		d_t = 0;
		break;
	case JMPld: case CALLl: // far jmp/call
		d_t = DataFetchWL_U(mode, P2+1);
		if (REALADDR()) {
			InstrMeta[CurrIMeta].flags |= F_LJMP;
			j_t = SEGOFF2LINEAR(FetchW(P2 + pskip - 2), d_t);
			dsp = j_t - P2;
		} else {
			j_t = 0;
			dsp = 0;
		}
		break;
	default:
		dsp = pskip;
		if (pskip == 2)	// short branch (byte)
			dsp += (signed char)Fetch(P2+1);
		else		// long branch (word/long)
			dsp += (int)DataFetchWL_S(mode,
				P2+pskip-BT24(BitDATA16,mode));

		/* displacement for taken branch */
		d_t = P2 - Interp_LONG_CS + dsp;
		if (mode&DATA16) d_t &= 0xffff;

		/* jump address for taken branch */
		j_t = d_t + Interp_LONG_CS;
		break;
	}

	/* displacement for not taken branch */
	d_nt = P2 - Interp_LONG_CS + pskip;
	/* d_nt is allowed to be exactly 0x10000 for
	   unconditional jumps; if the jump is conditional
	   and not taken it'll GPF at the bottom of InterpOne.
	   If it's higher do an early return so it can GPF there
	   asap. OperandSize-based masking should only happen for the
	   *taken* branch (see Intel manuals!)
	*/
	if (REALADDR() && d_nt > 0x10000)
		return P2 + pskip;

	/* jump address for not taken branch, usually next instruction */
	j_nt = d_nt + Interp_LONG_CS;
	assert(j_nt > P2);
	PC = j_nt;

#if !defined(SINGLESTEP)
	P1 = P2 + pskip;
#endif
	switch(opc) {
	case JO ... JNLE_JG:
	case JCXZ:
		if (dsp < 0) mode |= CKSIGN;
		/* is there a jump after the condition? if yes, simplify */
#if !defined(SINGLESTEP)
		if (!(mode & MSSTP)) {
		  if (Fetch(P1)==JMPsid) {	/* eb xx */
		    int dsp2 = (signed char)Fetch(P1+1) + 2;
	    	    if (dsp2 < 0) mode |= CKSIGN;
		    d_nt = P1 - Interp_LONG_CS + dsp2;
		    if (mode&DATA16) d_nt &= 0xffff;
		    j_nt = d_nt + Interp_LONG_CS;
		    if (debug_level('e')>1)
			e_printf("JMPs (%02x,%d) at %08x after Jcc: t=%08x nt=%08x\n",
				 Fetch(P1),dsp2,P1,j_t,j_nt);
		  }
		  else if (Fetch(P1)==JMPd) {	/* e9 xxxx{xxxx} */
		    int skp2 = BT24(BitDATA16,mode) + 1;
		    int dsp2 = skp2 + (int)DataFetchWL_S(mode, P1+1);
	    	    if (dsp2 < 0) mode |= CKSIGN;
		    d_nt = P1 - Interp_LONG_CS + dsp2;
		    if (mode&DATA16) d_nt &= 0xffff;
		    j_nt = d_nt + Interp_LONG_CS;
		    if (debug_level('e')>1)
			e_printf("JMPl (%02x,%d) at %08x after Jcc: t=%08x nt=%08x\n",
				 Fetch(P1),dsp2,P1,j_t,j_nt);
		  }
		}
#endif
		/* backwards jump limited to 256 bytes */
		if ((dsp > -256) && (dsp < pskip)) {
		    if (dsp >= 0) {
			// dsp>0 && dsp<pskip: jumps in the jmp itself
			if (dsp > 0)
			    e_printf("### self jmp=%x dsp=%d pskip=%d\n",opc,dsp,pskip);
			else // dsp==0
			    /* strange but possible, very tight loop with an external
			     * condition changing a flag */
			    e_printf("### dsp=0 jmp=%x pskip=%d\n",opc,pskip);
		    }
		    Gen(JB_LINK, mode, opc);
		    Gen(JMP_LINK, mode, j_nt, InstrMeta[0].npc);
		    Gen(JMP_LINK, mode|CKSIGN, j_t, InstrMeta[0].npc);
		}
		else {
		    if (dsp == pskip) {
			e_printf("### jmp %x 00\n",opc);
		    }
		    /* forward jump or backward jump >=256 bytes */
		    Gen(JF_LINK, mode, opc);
		    Gen(JMP_LINK, mode, j_nt, InstrMeta[0].npc);
		    Gen(JMP_LINK, mode&~CKSIGN, j_t, InstrMeta[0].npc);
		}
		break;
	case JMPld: {   /* uncond jmp far */
		unsigned short jcs = FetchW(P2 + pskip - 2);
		Gen(L_IMM_R1, mode|DATA16, jcs);
		if (REALADDR()) {
		    AddrGen(A_SR_SH4, mode, Ofs_CS, Ofs_XCS);
		}
		else {
		    AddrGen(A_SR_PROT, mode, Ofs_CS, P2);
		    /* transfer to new PC
		       (new cs base dynamic, so indirect jmp) */
		    Gen(L_IMM_R1, mode, d_t);
		    Gen(JMP_INDIRECT, mode);
		    break;
		}
	}
	/* no break */
	case JMPsid: case JMPd:   /* uncond jmp */
		if (dsp == 0) {      // eb fe
		    dbug_printf("!Possible forever loop!\n");
		    InstrMeta[CurrIMeta].flags |= F_SLFJ;
		}
		if (dsp <= 0) mode |= CKSIGN;
		Gen(JMP_LINK, mode, j_t, InstrMeta[0].npc);
		break;
	case CALLl: {   /* call far */
		unsigned short jcs = FetchW(P2 + pskip - 2);
		Gen(L_IMM_R1, mode|DATA16, jcs);
		if (REALADDR())
		    AddrGen(A_SR_SH4, mode, Ofs_CS, Ofs_XCS);
		else
		    /* check if new cs is valid, load if so */
		    AddrGen(A_SR_PROT, mode, Ofs_CS, P2);
		/* ok, now push old cs (returned by A_SR_*):eip */
		Gen(O_PUSH, mode);
		if (!REALADDR()) {
		    Gen(O_PUSHI, mode, d_nt);
		    /* transfer to new PC (indirect jmp) */
		    Gen(L_IMM_R1, mode, d_t);
		    Gen(JMP_INDIRECT, mode);
		    break;
		}
	}
	/* no break for realaddr call */
	case CALLd:    /* call, unfortunately also uses JMP_LINK */
		Gen(O_PUSHI, mode, d_nt);
		Gen(JMP_LINK, mode, j_t, InstrMeta[0].npc);
		break;
	case LOOP: case LOOPZ_LOOPE: case LOOPNZ_LOOPNE:
		Gen(JLOOP_LINK, mode, opc);
		/* CKSIGN is likely not needed for loops */
		Gen(JMP_LINK, mode, j_nt, InstrMeta[0].npc);
		Gen(JMP_LINK, mode, j_t, InstrMeta[0].npc);
		break;
	case RETl: case RETlisp: case IRET: // far ret, indirect
	case JMPli: case CALLli: case INT: // far jmp/call, indirect
	case RET: case RETisp: case JMPi: case CALLi: // ret, indirect
		Gen(JMP_INDIRECT, mode);
		break;
	default: dbug_printf("JumpGen: unknown condition\n");
		break;
	}

	return PC;
}

#if SPEC_PREJIT
static int can_speculate(void)
{
	IMeta *GL = &InstrMeta[CurrIMeta];
	IGen *IG = &GL->gen[GL->ngen-1];
	int opc = IG->op;
	/* With uncond JMP or RET nothing to speculate. */
	if (opc != JMP_LINK)
		return 0;
	/* we need two JMP_LINKs (conditional jump),
	   or PUSHI followed by JMP_LINK (call) */
	if (GL->ngen <= 1)
		return 0;
	opc = (IG-1)->op;
	if (opc != O_PUSHI && opc != JMP_LINK)
		return 0;
	if (debug_level('e')) {
		unsigned short ocs = TheCPU.cs;
		unsigned int P2 = GL->npc;
		char *ds = e_emu_disasm(EMU_BASE32(P2),IG->mode&MBIGCS,ocs);
		e_printf("prejit after  %s\n", ds);
	}
	return F_SPEC;
}
#endif

static unsigned int ExceptionGen(unsigned int PC, int basemode, int trapno,
	unsigned int scp_err, unsigned int P0)
{
	Gen(L_IMM, basemode, Ofs_ERR, trapno);
	if (trapno == EXCP0D_GPF)
		Gen(L_IMM, basemode, Ofs_SCP_ERR, scp_err);
	if (trapno != EXCP03_INT3 && trapno != EXCP04_INTO)
		Gen(JMP_TAILCODE, basemode, P0); // fault: use current instr
	else
		Gen(JMP_TAILCODE, basemode, PC); // trap: use next instr
	return PC;
}

/////////////////////////////////////////////////////////////////////////////

static TNode *_Interp86(unsigned int PC, unsigned int Interp_LONG_CS,
			unsigned short ocs, int basemode, int flags);
static void HandleEmuSignals(void);

static unsigned int FindExecCode(unsigned int PC)
{
	TNode *G;
	unsigned LastXKey = PC;

	/* for a sequence to be found, it must begin with
	 * an allowable opcode. Look into table.
	 * NOTE - this while can loop forever and stop
	 * any signal processing. Jumps are defined as
	 * a 'descheduling point' for checking signals.
	 */
	while (1) {
		if (EFLAGS & TF)
			CEmuStat |= CeS_TRAP;
		if (CEmuStat & (CeS_TRAP|CeS_STI))
			TheCPU.mode |= MSSTP;
		else
			TheCPU.mode &= ~MSSTP;
		G = FindTree(PC);
		if (G) {
			if (!GoodNode(G)) {
				InvalidateNodeRange(G->key, G->seqlen, NULL);
				G = NULL;
			}
			else if (debug_level('e')>2)
				e_printf("** Found compiled code at %08x\n",PC);
		}
		else if (e_querymark(PC, 1)) {
			/* slow path */
			InvalidateNodeRange(PC, 1, NULL);
		}
		/* future proofing: _Interp86() can't fail now in non-prejit mode
		   but if it ever does, retry */
		while (!G) {
			if (CEmuStat & (CeS_PREJIT_RM | CeS_PREJIT_PM)) {
				TheCPU.err = EXCP_GOBACK;
				return PC;
			}
#if 0
			/* this obviously can't happen with current code, but
			 * slows down execution under debug a lot */
			if (debug_level('e') && e_querymark(PC, 1))
				error("simx86: code nodes clashed at %x\n", PC);
#endif
			if (debug_level('e')>=9)
				dbug_printf("\n%s",e_print_regs(LONG_CS));
			G = _Interp86(PC, LONG_CS, TheCPU.cs, TheCPU.mode, 0);
		}
		if (debug_level('e') &&
				/* check for codemarks inconsistency */
				e_querymark_all(G->key, G->seqlen) == 0) {
			int i, j;
			error("no mark at %x (%i)\n",
					G->key,
					e_querymark(G->key, G->seqlen));
			j = -1;
			for (i = 0; i < G->seqlen; i++) {
				int mrk = e_querymark(G->key + i, 1);
				error("@%i ", mrk);
				if (!mrk && j == -1)
					j = i;
			}
			error("@\n");
			if (j != -1)
				error("@corrupted at %x\n", G->key + j);
		}
		/* ---- this is the MAIN EXECUTE point ---- */
		unsigned short seqflg = G->flags;
		NodesExecd++;
#if PROFILE
		TotalNodesExecd++;
		if (seqflg & F_PREJ)
			PrejitNodesExecd++;
#endif
		assert(G->seqlen);
#if SPEC_PREJIT
		if (seqflg & F_SPEC)
			prejit_run(G);
#endif
		PC = DoExec(G, &LastXKey);
		// G is unreliable (maybe deleted) past this point!
#if SPEC_PREJIT
		if (seqflg & F_SPEC)
			prejit_sync();
#endif
		if (seqflg & F_LEAV)
			TheCPU.err = EXCP_EMULEAVE;

		if (TheCPU.err) return PC;
		if (CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND|CeS_RPIC))
			HandleEmuSignals();
		if (TheCPU.err) return PC;
	}
	return PC;
}

static void HandleEmuSignals(void)
{
#if PROFILE
	if (debug_level('e')) EmuSignals++;
#endif
	if (CEmuStat & CeS_TRAP) {
		/* force exit for single step trap */
		if (!TheCPU.err)
			TheCPU.err = EXCP01_SSTP;
	}
	//else if (CEmuStat & CeS_DRTRAP) {
	//	if (e_debug_check(PC)) {
	//		TheCPU.err = EXCP01_SSTP;
	//	}
	//}
	else if (CEmuStat & CeS_SIGPEND) {
		/* force exit after signal */
		CEmuStat = (CEmuStat & ~CeS_SIGPEND) | CeS_SIGACT;
		TheCPU.err=EXCP_SIGNAL;
	}
	else if (CEmuStat & CeS_RPIC) {
		/* force exit for PIC */
		CEmuStat &= ~CeS_RPIC;
		if (EFLAGS & EFLAGS_IF)
			TheCPU.err=EXCP_PICSIGNAL;
	}
	/* clear optional exit conditions */
	CEmuStat &= ~CeS_TRAP;
	if (TheCPU.err)
		CEmuStat &= ~(CeS_SIGPEND | CeS_RPIC);
}

static unsigned int InterpOne(unsigned int PC, unsigned int Interp_LONG_CS,
			      unsigned short ocs, int basemode);

void Interp86(void)
{
    unsigned int PC;

    TheCPU.err = 0;

    if (PROTMODE() && setjmp(jmp_env)) {
      /* long jump to here from simulated page fault
         via Gen_sim or Sim_helper, with different cs:eip */
      return;
    }

    CEmuStat &= ~(CeS_TRAP|CeS_STI);

    PC = LONG_CS + TheCPU.eip;
    assert(CurrIMeta < 0);
    PC = FindExecCode(PC);
    assert(CurrIMeta < 0);
    TheCPU.eip = PC - LONG_CS;
}

/* safety gap should be definitely longer than 1 instruction to not
 * overwrite something unintentionally */
#define SAFE_PRJ_GAP 16

static int interp_post(unsigned int PC, unsigned int Interp_LONG_CS,
		       const int mode, int flags)
{
		int gap = (flags & F_SPRJ) ? SAFE_PRJ_GAP : 1;
		assert (CurrIMeta>=0);

		if (CEmuStat & CeS_INSTREMUx(PROTMODE())) {
			if (debug_level('e')>1)
				dbug_printf("CeS_INSTREMU, count=%d\n",
					    interp_inst_emu_count);
			if (interp_inst_emu_count == 0 ||
			    --interp_inst_emu_count == 0) {
				if ((CEmuStat & CeS_INSTREMU_PM) &&
							config.dpmi_remote &&
							vga.inst_emu) {
					instr_emu_sim_reset_count();
				} else {
					flags |= F_LEAV;
				}
			}
		}

#ifndef SINGLEBLOCK
		IMeta *GL = &InstrMeta[CurrIMeta];
		if ((mode & MSSTP) ||
		    (flags & F_LEAV) ||
		    GL->gen[GL->ngen-1].op >= JMP_TAILCODE ||
		    e_querymark(PC, gap))
#endif
		{
			if (!(flags & F_SPRJ))
				/* don't do recursive speculation ! */
				flags |= can_speculate();
			InstrMeta[0].flags |= flags;
			return 1;
		}

		return 0;
}

static void sprj_deep(TNode *G, unsigned PC, unsigned int Interp_LONG_CS,
		unsigned short ocs, int basemode, int flags)
{
#ifdef SPEC_PREJIT
	int i = 0;

	while ((G->unlinked_jmp_targets & TARGET_T) && !(G->flags & (F_LJMP|F_LEAV))) {
		TNode *oldG = G;
		PC = G->clink_t.target;
		if (e_querymark(PC, SAFE_PRJ_GAP)) break;
		do {
			PC = InterpOne(PC, Interp_LONG_CS, ocs, basemode);
		} while (!interp_post(PC, Interp_LONG_CS, basemode, flags));
		G = DoClose(PC, Interp_LONG_CS, basemode, flags);
		i++;
		NodesPrejitted++;
		NodeLinker(oldG, G);
	}
	if (debug_level('e') && i)
		dbug_printf("Linked %d TARGET_T nodes in advance\n", i);
#endif
}

static TNode *_Interp86(unsigned int PC, unsigned int Interp_LONG_CS,
			unsigned short ocs, int basemode, int flags)
{
	do {
		PC = InterpOne(PC, Interp_LONG_CS, ocs, basemode);
	} while (!interp_post(PC, Interp_LONG_CS, basemode, flags));
	return DoClose(PC, Interp_LONG_CS, basemode, flags);
}

static unsigned int InterpOne(unsigned int PC, unsigned int Interp_LONG_CS,
			      unsigned short ocs, int basemode)
{
	unsigned int P0 = PC;
	unsigned char opc;
	int _mode = basemode;
	/* WARNING - these are signed char offsets, NOT pointers! */
	signed char OVERR_DS = Ofs_XDS;
	signed char OVERR_SS = Ofs_XSS;

	if (debug_level('e')>2) {
		char *ds;
		ds = e_emu_disasm(EMU_BASE32(PC),TheCPU.mode&MBIGCS,ocs);
		e_printf("  %s\n", ds);
	}

	if (!NewIMeta(P0)) {
		if (debug_level('e')>2)
			e_printf("============ Tab full:cannot close sequence\n");

		// close previous instruction with tail code and try again later
		Gen(JMP_TAILCODE, basemode, P0);
		return P0;
	}

override:
	switch ((opc=Fetch(PC))) {
/*28*/	case SUBbfrm:
/*2a*/	case SUBbtrm:
/*30*/	case XORbfrm:
/*32*/	case XORbtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_CLEAR, _mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3a; else goto intop28;
/*08*/	case ORbfrm:
/*0a*/	case ORbtrm:
/*20*/	case ANDbfrm:
/*22*/	case ANDbtrm:
/*84*/	case TESTbrm:
		        { int m = _mode | MBYTE;
			if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_TEST, m, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc != TESTbrm) {
			    if (opc & 2) goto intop3a; else goto intop28;
			}
			PC += ModRM(opc, PC, m|MLOAD);	// al=[rm]
			Gen(O_AND_R, m, REG1);		// op  al,[ebx+reg]
			}
			break;
/*18*/	case SBBbfrm:
/*1a*/	case SBBbtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, _mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3a;
/*00*/	case ADDbfrm:
/*10*/	case ADCbfrm:
/*38*/	case CMPbfrm:
intop28:		{ int m = _mode | MBYTE;
			PC += ModRM(opc, PC, m);	// DI=mem
			if (REG3) {
			    int op = ArOpsFR[D_MO(opc)];
			    Gen(L_REG, m, REG1);	// mov al,[ebx+reg]
			    Gen(op, m, REG3);		// op [ebx+rmreg],al	rmreg=rmreg op reg
			}
			else {
			    int op = ArOpsR[D_MO(opc)];
			    Gen(L_DI_R1, m);		// mov al,[edi]
			    Gen(op, m, REG1);		// op  al,[ebx+reg]
			    if (opc!=CMPbfrm) {
				Gen(S_DI, m);		// mov [edi],al		mem=mem op reg
			    }
			} }
			break;
/*02*/	case ADDbtrm:
/*12*/	case ADCbtrm:
/*3a*/	case CMPbtrm:
intop3a:		{ int m = _mode | MBYTE;
			int op = ArOpsFR[D_MO(opc)];
			PC += ModRM(opc, PC, m|MLOAD);	// al=[rm]
			Gen(op, m, REG1);		// op [ebx+reg], al
			}
			break;
/*29*/	case SUBwfrm:
/*2b*/	case SUBwtrm:
/*31*/	case XORwfrm:
/*33*/	case XORwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_CLEAR, _mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3b; else goto intop29;
/*09*/	case ORwfrm:
/*0b*/	case ORwtrm:
/*21*/	case ANDwfrm:
/*23*/	case ANDwtrm:
/*85*/	case TESTwrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_TEST, _mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc != TESTwrm) {
			    if (opc & 2) goto intop3b; else goto intop29;
			}
			PC += ModRM(opc, PC, _mode|MLOAD);	// (e)ax=[rm]
			Gen(O_AND_R, _mode, REG1);	// op  (e)ax,[ebx+reg]
			break;
/*19*/	case SBBwfrm:
/*1b*/	case SBBwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, _mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3b;
/*01*/	case ADDwfrm:
/*11*/	case ADCwfrm:
/*39*/	case CMPwfrm:
intop29:		PC += ModRM(opc, PC, _mode);	// DI=mem
			if (REG3) {
			    int op = ArOpsFR[D_MO(opc)];
			    Gen(L_REG, _mode, REG1);	// mov (e)ax,[ebx+reg]
			    Gen(op, _mode, REG3);	// op [ebx+rmreg],(e)ax	rmreg=rmreg op reg
			}
			else {
			    int op = ArOpsR[D_MO(opc)];
			    Gen(L_DI_R1, _mode);		// mov (e)ax,[edi]
			    Gen(op, _mode, REG1);	// op  (e)ax,[ebx+reg]
			    if (opc!=CMPwfrm) {
				Gen(S_DI, _mode);	// mov [edi],(e)ax	mem=mem op reg
			    }
			}
			break;
/*03*/	case ADDwtrm:
/*13*/	case ADCwtrm:
/*3b*/	case CMPwtrm:
intop3b:		{ int op = ArOpsFR[D_MO(opc)];
			PC += ModRM(opc, PC, _mode|MLOAD);	// (e)ax=[rm]
			Gen(op, _mode, REG1);		// op [ebx+reg], (e)ax
			}
			break;
/*a8*/	case TESTbi: {
			int m = _mode | MBYTE;
			Gen(L_IMM_R1, m, Fetch(PC+1)); PC+=2;	// mov al,#imm
			Gen(O_AND_R, m, Ofs_AL);		// op al,[ebx+reg]
		        }
			break;
/*04*/	case ADDbia:
/*0c*/	case ORbi:
/*14*/	case ADCbi:
/*24*/	case ANDbi:
/*1c*/	case SBBbi:
/*2c*/	case SUBbi:
/*34*/	case XORbi:
/*3c*/	case CMPbi: {
			int m = _mode | MBYTE;
			int op = ArOpsFR[D_MO(opc)];
			// op [ebx+Ofs_EAX],#imm
			Gen(op, m|IMMED, Ofs_EAX, Fetch(PC+1)); PC+=2;
			}
			break;
/*a9*/	case TESTwi:
			Gen(L_IMM_R1, _mode|IMMED, DataFetchWL_U(_mode,PC+1));
			INC_WL_PC(_mode,1);
			Gen(O_AND_R, _mode, Ofs_EAX);
			break;
/*05*/	case ADDwia:
/*0d*/	case ORwi:
/*15*/	case ADCwi:
/*1d*/	case SBBwi:
/*25*/	case ANDwi:
/*2d*/	case SUBwi:
/*35*/	case XORwi:
/*3d*/	case CMPwi: {
			int m = _mode;
			int op = ArOpsFR[D_MO(opc)];
			// op [ebx+Ofs_EAX],#imm
			Gen(op, m|IMMED, Ofs_EAX, DataFetchWL_U(_mode,PC+1));
			INC_WL_PC(_mode,1);
			}
			break;
/*69*/	case IMULwrm:
			PC += ModRM(opc, PC, _mode|MLOAD); // mov (e)ax,[rm]
			Gen(O_IMUL,_mode|IMMED,DataFetchWL_S(_mode,PC),REG1);
			INC_WL_PC(_mode, 0);
			break;
/*6b*/	case IMULbrm:
			PC += ModRM(opc, PC, _mode|MLOAD); // mov (e)ax,[rm]
			Gen(O_IMUL,_mode|MBYTE|IMMED,(signed char)Fetch(PC),REG1);
			PC++;
			break;

/*9c*/	case PUSHF:
			PC++;
			if (V86MODE() && (IOPL<3)) {
			    /* virtual-8086 monitor */
			    if (!(TheCPU.cr[4] & CR4_VME))
				goto not_permitted;	/* GPF */
			    Gen(O_SIM, _mode, opc, 0, P0);
			    Gen(O_PUSH, _mode);
			}
			else {
				Gen(O_PUSH2F, _mode);
			}
			break;
/*9e*/	case SAHF:
			Gen(O_SLAHF, _mode, 1);
			PC++; break;
/*9f*/	case LAHF:
			Gen(O_SLAHF, _mode, 0);
			PC++; break;

/*27*/	case DAA:
			Gen(O_OPAX, _mode, 1, DAA); PC++; break;
/*2f*/	case DAS:
			Gen(O_OPAX, _mode, 1, DAS); PC++; break;
/*37*/	case AAA:
			Gen(O_OPAX, _mode, 1, AAA); PC++; break;
/*3f*/	case AAS:
			Gen(O_OPAX, _mode, 1, AAS); PC++; break;
/*d4*/	case AAM:
			Gen(O_OPAX, _mode, 2, AAM, Fetch(PC+1)); PC+=2; break;
/*d5*/	case AAD:
			Gen(O_OPAX, _mode, 2, AAD, Fetch(PC+1)); PC+=2; break;

/*d6*/	case 0xd6:	/* Undocumented */
			e_printf("Undocumented op 0xd6\n");
			Gen(O_SETCC, _mode, 2);
			Gen(O_NEG, _mode|MBYTE);
			Gen(S_REG, _mode|MBYTE, Ofs_AL);
			PC++; break;
/*62*/	case BOUND:
			if (Fetch(PC+1) >= 0xc0) {
			    PC += 2; goto not_permitted;
			}
			PC += ModRM(opc, PC, _mode);
			Gen(L_REG, _mode, REG1);
			Gen(O_SIM, _mode, opc, REG3, P0);
			break;
/*63*/	case ARPL:
			PC += ModRM(opc, PC, _mode);
			Gen(L_REG, _mode|DATA16, REG1);
			Gen(O_SIM, _mode, opc, REG3, P0);
			break;
/*d7*/	case XLAT:
			Gen(O_XLAT, _mode, OVERR_DS);
			Gen(L_DI_R1, _mode|MBYTE);
			Gen(S_REG, _mode|MBYTE, Ofs_AL);
			PC++; break;
/*98*/	case CBW:
			Gen(O_CBWD, _mode|MBYTE); PC++; break;
/*99*/	case CWD:
			Gen(O_CBWD, _mode); PC++; break;

/*07*/	case POPes:	if (REALADDR()) {
			    Gen(O_POP, _mode);
			    AddrGen(A_SR_SH4, _mode, Ofs_ES, Ofs_XES);
			} else { /* restartable */
			    Gen(O_POP1, _mode);
			    Gen(O_POP2, _mode|MNOREG);
			    /* same principle applies as for POPrm: this
			       segment load may fault, above pops into
			       temporary storage without adjusting (E)SP */
			    AddrGen(A_SR_PROT, _mode, Ofs_ES, P0);
			    Gen(O_POP3, _mode);
			}
			PC++;
			break;
/*17*/	case POPss:	if (REALADDR()) {
			    Gen(O_POP, _mode);
			    AddrGen(A_SR_SH4, _mode, Ofs_SS, Ofs_XSS);
			} else { /* restartable */
			    Gen(O_POP1, _mode);
			    Gen(O_POP2, _mode|MNOREG);
			    AddrGen(A_SR_PROT, _mode, Ofs_SS, P0);
			    Gen(O_POP3, _mode);
			}
			InstrMeta[CurrIMeta].flags |= F_INHI;
			PC++;
			break;
/*1f*/	case POPds:	if (REALADDR()) {
			    Gen(O_POP, _mode);
			    AddrGen(A_SR_SH4, _mode, Ofs_DS, Ofs_XDS);
			} else { /* restartable */
			    Gen(O_POP1, _mode);
			    Gen(O_POP2, _mode|MNOREG);
			    AddrGen(A_SR_PROT, _mode, Ofs_DS, P0);
			    Gen(O_POP3, _mode);
			}
			PC++;
			break;

/*26*/	case SEGes:
			OVERR_DS = OVERR_SS = Ofs_XES; PC++; goto override;
/*2e*/	case SEGcs:		/* CS is already checked */
			OVERR_DS = OVERR_SS = Ofs_XCS; PC++; goto override;
/*36*/	case SEGss:		/* SS is already checked */
			OVERR_DS = OVERR_SS = Ofs_XSS; PC++; goto override;
/*3e*/	case SEGds:
			OVERR_DS = OVERR_SS = Ofs_XDS; PC++; goto override;
/*64*/	case SEGfs:
			OVERR_DS = OVERR_SS = Ofs_XFS; PC++; goto override;
/*65*/	case SEGgs:
			OVERR_DS = OVERR_SS = Ofs_XGS; PC++; goto override;
/*66*/	case OPERoverride:	/* 0x66: 32 bit operand, 16 bit addressing */
			_mode = (_mode & ~DATA16) | (~basemode & DATA16);
			if (debug_level('e')>4)
				e_printf("OPERoverride: new _mode %04x\n",_mode);
			PC++; goto override;
/*67*/	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
			_mode = (_mode & ~ADDR16) | (~basemode & ADDR16);
			if (debug_level('e')>4)
				e_printf("ADDRoverride: new _mode %04x\n",_mode);
			PC++; goto override;

/*f0*/	case LOCK: {	int i = 1; unsigned char op;
			do {
				op = Fetch(PC+i);
				i++;
			} while (op == SEGcs || op == SEGss || op == SEGds ||
				 op == SEGes || op == SEGfs || op == SEGgs ||
				 op == OPERoverride || op == ADDRoverride);
			/* LOCK is allowed on BTS, BTR, BTC, XCHG, ADD...XOR (but not CMP),
			   INC, DEC, NOT, NEG, XADD -- just ignore LOCK for now... */
			if (op == 0x0f) {
				op = Fetch(PC+i);   /* BTS/BTR/BTC/XADD/
						       CMPXCHG* */
			  	if (op == 0xab || op == 0xb3 || op == 0xbb ||
				    op == 0xc0 || op == 0xc1 ||
				    op == 0xb0 || op == 0xb1 ||
				    (op == 0xc7 && D_MO(Fetch(PC+i+1)) == 1) ||
				    op == 0xba /* GRP8 - Code Extension 22 */
				    ) {
					PC++; goto override;
				}
			} else if (op >= 0xf6 && op < 0xf8) { /*NOT/NEG*/
				op = Fetch(PC+i);
				if ((op & 0x30) == 0x10) {
					PC++; goto override;
				}
			} else if (op >= 0xfe) { /*INC/DEC*/
				op = Fetch(PC+i);
				if ((op & 0x30) == 0x00) {
					PC++; goto override;
				}
			}
			else if ((op < 0x38 && (op & 0x8) < 6) || /*ADD..XOR*/
			    (op >= 0x40 && op < 0x50) || /*INC/DEC*/
			    (op >= 0x91 && op < 0x98) ||
			    op == 0x86 || op == 0x87) { /*XCHG, not NOP*/
				PC++; goto override;
			}
			else if (op >= 0x80 && op <= 0x83) { /*ADD..XOR*/
				op = Fetch(PC+i);
				if ((op & 0x38) < 0x38) {
					PC++; goto override;
				}
			}
			PC += i+1; goto illegal_op;
			}
/*40*/	case INCax:
/*41*/	case INCcx:
/*42*/	case INCdx:
/*43*/	case INCbx:
/*44*/	case INCsp:
/*45*/	case INCbp:
/*46*/	case INCsi:
/*47*/	case INCdi:
			Gen(O_INC_R, _mode, R1Tab_l[D_LO(opc)]); PC++; break;
/*48*/	case DECax:
/*49*/	case DECcx:
/*4a*/	case DECdx:
/*4b*/	case DECbx:
/*4c*/	case DECsp:
/*4d*/	case DECbp:
/*4e*/	case DECsi:
/*4f*/	case DECdi:
			Gen(O_DEC_R, _mode, R1Tab_l[D_LO(opc)]); PC++; break;

/*06*/	case PUSHes:
/*0e*/	case PUSHcs:
/*16*/	case PUSHss:
/*1e*/	case PUSHds:
/*50*/	case PUSHax:
/*51*/	case PUSHcx:
/*52*/	case PUSHdx:
/*53*/	case PUSHbx:
/*54*/	case PUSHsp:
/*55*/	case PUSHbp:
/*56*/	case PUSHsi:
/*57*/	case PUSHdi:	opc = OpIsPush[opc];
#ifndef SINGLESTEP
			if (!(_mode & MSSTP)) {
			int m = _mode;		// enter with prefix
			int cnt = 2;
			int is_66;
			Gen(O_PUSH1, m);
			/* optimized multiple register push */
			while (1) {
			    int op;
			    if (opc > 8 && !(_mode & DATA16)) // 32-bit segreg
				m |= SEGREG;
			    Gen(O_PUSH2, m, R1Tab_l[opc-1]);
			    PC++;
			    opc = OpIsPush[op = Fetch(PC)];
			    is_66 = (op == 0x66);
			    if (++cnt >= NUMGENS || (!opc && !is_66) ||
				    e_querymark(PC, 1 + is_66))
				break;
			    m &= ~(DATA16|SEGREG);
			    if (is_66) {	// prefix 0x66
				m |= (~basemode & DATA16);
				if ((opc=OpIsPush[(op = Fetch(PC+1))])!=0) PC++;
				else break;
			    }
			    else {
				m |= (basemode & DATA16);
			    }
			    if (op == PUSHsp) {
				/* reload SP before pushing it */
				Gen(O_PUSH3, m);
			    }
			}
			Gen(O_PUSH3, m); } else
#endif
			{
			if (opc > 8 && !(_mode & DATA16)) { // 32-bit segreg
			    Gen(L_REG, _mode|DATA16, R1Tab_l[opc-1]);
			    Gen(L_ZXAX, _mode);
			} else
			    Gen(L_REG, _mode, R1Tab_l[opc-1]);
			Gen(O_PUSH, _mode); PC++;
			}
			break;
/*68*/	case PUSHwi:
			Gen(O_PUSHI, _mode, DataFetchWL_U(_mode,(PC+1)));
			INC_WL_PC(_mode,1);
			break;
/*6a*/	case PUSHbi:
			Gen(O_PUSHI, _mode, (signed char)Fetch(PC+1)); PC+=2; break;
/*60*/	case PUSHA:
			/* push order: eax ecx edx ebx esp ebp esi edi */
			Gen(O_PUSH1, _mode);
			Gen(O_PUSH2, _mode, Ofs_EAX);
			Gen(O_PUSH2, _mode, Ofs_ECX);
			Gen(O_PUSH2, _mode, Ofs_EDX);
			Gen(O_PUSH2, _mode, Ofs_EBX);
			Gen(O_PUSH2, _mode, Ofs_ESP);
			Gen(O_PUSH2, _mode, Ofs_EBP);
			Gen(O_PUSH2, _mode, Ofs_ESI);
			Gen(O_PUSH2, _mode, Ofs_EDI);
			Gen(O_PUSH3, _mode); PC++; break;
/*61*/	case POPA:
			Gen(O_POP1, _mode);
			Gen(O_POP2, _mode, Ofs_EDI);
			Gen(O_POP2, _mode, Ofs_ESI);
			Gen(O_POP2, _mode, Ofs_EBP);
			Gen(O_POP2, _mode, Ofs_ESP); // overwritten in O_POP3
			Gen(O_POP2, _mode, Ofs_EBX);
			Gen(O_POP2, _mode, Ofs_EDX);
			Gen(O_POP2, _mode, Ofs_ECX);
			Gen(O_POP2, _mode, Ofs_EAX);
			Gen(O_POP3, _mode); PC++; break;

/*58*/	case POPax:
/*59*/	case POPcx:
/*5a*/	case POPdx:
/*5b*/	case POPbx:
/*5c*/	case POPsp:
/*5d*/	case POPbp:
/*5e*/	case POPsi:
/*5f*/	case POPdi:
#ifndef SINGLESTEP
			if (!(_mode & MSSTP)) {
			int m = _mode;
			int cnt = 2;
			Gen(O_POP1, m);
			do {
				opc = Fetch(PC);
				Gen(O_POP2, m, R1Tab_l[D_LO(opc)]);
				m = UNPREFIX(m);
				PC++;
				/* for pop sp reload stack pointer */
				if (opc == POPsp)
					Gen(O_POP1, m);
			} while (++cnt < NUMGENS && (Fetch(PC)&0xf8)==0x58 &&
					!e_querymark(PC, 1));
			if (opc!=POPsp) Gen(O_POP3, m);
			} else
#endif
			{
			Gen(O_POP, _mode);
			Gen(S_REG, _mode, R1Tab_l[D_LO(opc)]); PC++;
			}
			break;
/*8f*/	case POPrm:
			// now calculate address. This way when using %esp
			//	as index we use the value AFTER the pop
			PC += ModRM(opc, PC, _mode|MPOPRM);
			if (REG3) {
				// pop data into temporary storage and adjust esp
				Gen(O_POP, _mode);
				// store data
				Gen(S_REG, _mode, REG3);
			} else {
				// read data into temporary storage
				Gen(O_POP1, _mode);
				Gen(O_POP2, _mode|MNOREG);
				// store data
				// S_DI may fault, in which case the instruction
				// may need to be restarted with the original
				// value of ESP!
				Gen(S_DI, _mode);	// mov [edi],{e}ax
				Gen(O_POP3, _mode);
			}
			break;

/*70*/	case JO:
/*71*/	case JNO:
/*72*/	case JB_JNAE:
/*73*/	case JNB_JAE:
/*74*/	case JE_JZ:
/*75*/	case JNE_JNZ:
/*76*/	case JBE_JNA:
/*77*/	case JNBE_JA:
/*78*/	case JS:
/*79*/	case JNS:
/*7a*/	case JP_JPE:
/*7b*/	case JNP_JPO:
/*7c*/	case JL_JNGE:
/*7d*/	case JNL_JGE:
/*7e*/	case JLE_JNG:
/*7f*/	case JNLE_JG:
/*eb*/	case JMPsid:
/*e0*/	case LOOPNZ_LOOPNE:
/*e1*/	case LOOPZ_LOOPE:
/*e2*/	case LOOP:
/*e3*/	case JCXZ:
			PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 2);
			break;

/*82*/	case IMMEDbrm2:		// add mem8,signed imm8: no AND,OR,XOR
/*80*/	case IMMEDbrm: {
			int m = _mode | MBYTE;
			int op = D_MO(Fetch(PC+1));
			PC += ModRM(opc, PC, m);
			if (REG3) {
				op = ArOpsFR[op];
				// op [ebx+reg],#imm
				Gen(op, m|IMMED, REG3, Fetch(PC));
			}
			else {
				op = ArOpsR[op];
				Gen(L_DI_R1, m);	// mov al,[edi]
				Gen(op, m|IMMED, Fetch(PC));	// op al,#imm
				if (op!=O_CMP_R)
					Gen(S_DI, m);	// mov [edi],al	mem=mem op #imm
			}
			PC++; }
			break;
/*81*/	case IMMEDwrm: {
			int op = D_MO(Fetch(PC+1));
			PC += ModRM(opc, PC, _mode);
			if (REG3) {
				op = ArOpsFR[op];
				// op [ebx+reg],#imm
				Gen(op, _mode|IMMED, REG3, DataFetchWL_U(_mode,PC));
			}
			else {
				op = ArOpsR[op];
				Gen(L_DI_R1, _mode);	// mov ax,[edi]
				Gen(op, _mode|IMMED, DataFetchWL_U(_mode,PC)); // op ax,#imm
				if (op!=O_CMP_R)
					Gen(S_DI, _mode);// mov [edi],ax	mem=mem op #imm
			}
			INC_WL_PC(_mode,0);
			}
			break;
/*83*/	case IMMEDisrm: {
			int op = D_MO(Fetch(PC+1));
			long v;
			PC += ModRM(opc, PC, _mode);
			v = (signed char)Fetch(PC);
			if (REG3) {
				op = ArOpsFR[op];
				// op [ebx+reg],#imm
				Gen(op, _mode|IMMED, REG3, v);
			}
			else {
				op = ArOpsR[op];
				Gen(L_DI_R1, _mode);	// mov ax,[edi]
				Gen(op, _mode|IMMED, v);		// op ax,#imm
				if (op != O_CMP_R)
					Gen(S_DI, _mode);// mov [edi],ax	mem=mem op #imm
			}
			PC++; }
			break;
/*86*/	case XCHGbrm:
			if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(L_NOP, _mode); PC+=2;
			}
			else {
			    PC += ModRM(opc, PC, _mode|MBYTE|MLOAD);// al=[rm]
			    if (REG3) {
				Gen(O_XCHG, _mode|MBYTE, REG1);
				Gen(S_REG, _mode|MBYTE, REG3);
			    }
			    else {
				Gen(O_XCHG, _mode|MBYTE, REG1);
				Gen(S_DI, _mode|MBYTE);
			    }
			}
			break;
/*87*/	case XCHGwrm:
			if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(L_NOP, _mode); PC+=2;
			}
			else {
			    PC += ModRM(opc, PC, _mode|MLOAD);	// (e)ax=[rm]
			    if (REG3) {
				Gen(O_XCHG, _mode, REG1);
				Gen(S_REG, _mode, REG3);
			    }
			    else {
				Gen(O_XCHG, _mode, REG1);
				Gen(S_DI, _mode);
			    }
			}
			break;
/*88*/	case MOVbfrm:
			if (ModGetReg1(PC, MBYTE)==3) {
			    Gen(L_REG2REG, MBYTE, REG1, REG3); PC+=2;
			} else {
			    Gen(L_REG, _mode|MBYTE, REG1);
			    PC += ModRM(opc, PC, _mode|MBYTE|MSTORE); // [rm]=al
			}
			break;
/*89*/	case MOVwfrm:
			if (ModGetReg1(PC, _mode)==3) {
			    Gen(L_REG2REG, _mode, REG1, REG3); PC+=2;
			} else {
			    Gen(L_REG, _mode, REG1);
			    PC += ModRM(opc, PC, _mode|MSTORE); // [rm]=(e)ax
			}
			break;
/*8a*/	case MOVbtrm:
			if (ModGetReg1(PC, MBYTE)==3) {
			    Gen(L_REG2REG, MBYTE, REG3, REG1); PC+=2;
			} else {
			    PC += ModRM(opc, PC, _mode|MBYTE|MLOAD); // al=[rm]
			    Gen(S_REG, _mode|MBYTE, REG1);
			}
			break;
/*8b*/	case MOVwtrm:
			if (ModGetReg1(PC, _mode)==3) {
			    Gen(L_REG2REG, _mode, REG3, REG1); PC+=2;
			} else {
			    PC += ModRM(opc, PC, _mode|MLOAD); // (e)ax=[rm]
			    Gen(S_REG, _mode, REG1);
			}
			break;
/*8c*/	case MOVsrtrm:
			PC += ModRM(opc, PC, _mode|SEGREG);
			Gen(L_REG, _mode|DATA16, REG1);
			if (REG3) {
			    if (!(_mode & DATA16))
				Gen(L_ZXAX, _mode);
			    Gen(S_REG, _mode, REG3);
			} else
			    Gen(S_DI, _mode|DATA16);
			break;
/*8d*/	case LEA:
			if (Fetch(PC+1) >= 0xc0) {
			    PC += 2; goto illegal_op;
			}
			PC += ModRM(opc, PC, _mode|MLEA);
			Gen(S_DI_R, _mode, REG1);
			break;

/*c4*/	case LES:
			if (Fetch(PC+1) >= 0xc0) {
			    PC += 2; goto illegal_op;
			}
			PC += ModRM(opc, PC, _mode);
			Gen(L_LXS2, _mode);
			if (REALADDR()) {
			    AddrGen(A_SR_SH4, _mode, Ofs_ES, Ofs_XES);
			}
			else {
			    AddrGen(A_SR_PROT, _mode, Ofs_ES, P0);
			}
			Gen(L_LXS1, _mode, REG1);
			break;
/*c5*/	case LDS:
			if (Fetch(PC+1) >= 0xc0) {
			    PC += 2; goto illegal_op;
			}
			PC += ModRM(opc, PC, _mode);
			Gen(L_LXS2, _mode);
			if (REALADDR()) {
			    AddrGen(A_SR_SH4, _mode, Ofs_DS, Ofs_XDS);
			}
			else {
			    AddrGen(A_SR_PROT, _mode, Ofs_DS, P0);
			}
			Gen(L_LXS1, _mode, REG1);
			break;
/*8e*/	case MOVsrfrm:
			PC += ModRM(opc, PC, _mode|SEGREG|DATA16|MLOAD);
			if (REG1 == Ofs_CS) {
			    goto illegal_op;
			}
			if (REALADDR()) {
			    AddrGen(A_SR_SH4, _mode, REG1, e_ofsseg(REG1));
			}
			else {
			    AddrGen(A_SR_PROT, _mode, REG1, P0);
			}
			if (REG1 == Ofs_SS)
			    InstrMeta[CurrIMeta].flags |= F_INHI;
			break;

/*9b*/	case oWAIT:
/*90*/	case NOP:	//if (!IsCodeInBuf()) Gen(L_NOP, _mode);
			Gen(L_NOP, _mode);
			PC++;
			if (!(_mode & MSSTP))
			    while (Fetch(PC)==NOP) PC++;
			break;
/*91*/	case XCHGcx:
/*92*/	case XCHGdx:
/*93*/	case XCHGbx:
/*94*/	case XCHGsp:
/*95*/	case XCHGbp:
/*96*/	case XCHGsi:
/*97*/	case XCHGdi:
			Gen(O_XCHG_R, _mode, Ofs_EAX, R1Tab_l[D_LO(opc)]);
			PC++; break;

/*a0*/	case MOVmal:
			AddrGen(A_DI_0, _mode|IMMED, OVERR_DS, AddrFetchWL_U(_mode,PC+1));
			Gen(L_DI_R1, _mode|MBYTE);
			Gen(S_REG, _mode|MBYTE, Ofs_AL);
			INC_WL_PCA(_mode,1);
			break;
/*a1*/	case MOVmax:
			AddrGen(A_DI_0, _mode|IMMED, OVERR_DS, AddrFetchWL_U(_mode,PC+1));
			Gen(L_DI_R1, _mode);
			Gen(S_REG, _mode, Ofs_EAX);
			INC_WL_PCA(_mode,1);
			break;
/*a2*/	case MOValm:
			AddrGen(A_DI_0, _mode|IMMED, OVERR_DS, AddrFetchWL_U(_mode,PC+1));
			Gen(L_REG, _mode|MBYTE, Ofs_AL);
			Gen(S_DI, _mode|MBYTE);
			INC_WL_PCA(_mode,1);
			break;
/*a3*/	case MOVaxm:
			AddrGen(A_DI_0, _mode|IMMED, OVERR_DS, AddrFetchWL_U(_mode,PC+1));
			Gen(L_REG, _mode, Ofs_EAX);
			Gen(S_DI, _mode);
			INC_WL_PCA(_mode,1);
			break;

/*a4*/	case MOVSb: {	int m = _mode|(MBYTE|MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC, OVERR_DS);
			Gen(S_DI, m);
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++;
			} break;
/*a5*/	case MOVSw: {	int m = _mode|(MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC, OVERR_DS);
			Gen(S_DI, m);
			PC++;
			Gen(O_MOVS_SavA, m, OVERR_DS);
#ifndef SINGLESTEP
			/* optimize common sequence MOVSw..MOVSw..MOVSb */
			if (!(_mode & MSSTP)) {
				int cnt = 3;
				m = UNPREFIX(m);
				while (++cnt < NUMGENS && Fetch(PC) == MOVSw &&
						!e_querymark(PC, 1)) {
					Gen(O_MOVS_SetA, m&~MOVSDST, OVERR_DS);
					Gen(L_DI_R1, m);
					Gen(O_MOVS_SetA, m&~MOVSSRC, OVERR_DS);
					Gen(S_DI, m);
					PC++;
					Gen(O_MOVS_SavA, m, OVERR_DS);
				}
				if (Fetch(PC) == MOVSb && !e_querymark(PC, 1)) {
					m |= MBYTE;
					Gen(O_MOVS_SetA, m&~MOVSDST, OVERR_DS);
					Gen(L_DI_R1, m);
					Gen(O_MOVS_SetA, m&~MOVSSRC, OVERR_DS);
					Gen(S_DI, m);
					PC++;
					Gen(O_MOVS_SavA, m, OVERR_DS);
				}
			}
#endif
			} break;
/*a6*/	case CMPSb: {	int m = _mode|(MBYTE|MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC, OVERR_DS);
			Gen(O_MOVS_CmpD, m);
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; } break;
/*a7*/	case CMPSw: {	int m = _mode|(MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC, OVERR_DS);
			Gen(O_MOVS_CmpD, m);
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; } break;
/*aa*/	case STOSb: {	int m = _mode|(MBYTE|MOVSDST);
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_REG, m, Ofs_AL);
			Gen(S_DI, m);
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; } break;
/*ab*/	case STOSw: {	int m = _mode|MOVSDST;
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_REG, m, Ofs_EAX);
			Gen(S_DI, m); PC++;
			Gen(O_MOVS_SavA, m, OVERR_DS);
#ifndef SINGLESTEP
			if (!(_mode & MSSTP)) {
			    int cnt = 3;
			    m = UNPREFIX(m);
			    while (++cnt < NUMGENS && Fetch(PC) == STOSw &&
					!e_querymark(PC, 1)) {
				Gen(O_MOVS_SetA, m, OVERR_DS);
				Gen(S_DI, m);
				Gen(O_MOVS_SavA, m, OVERR_DS);
				PC++;
			    }
			}
#endif
			} break;
/*ac*/	case LODSb: {	int m = _mode|(MBYTE|MOVSSRC);
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(S_REG, m, Ofs_AL); PC++;
#ifndef SINGLESTEP
			/* optimize common sequence LODSb-STOSb */
			if (!(_mode & MSSTP) && Fetch(PC) == STOSb &&
					!e_querymark(PC, 1)) {
				Gen(O_MOVS_SetA, (m&ADDR16)|MOVSDST, OVERR_DS);
				Gen(S_DI, m);
				m |= MOVSDST;
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, m, OVERR_DS);
			} break;
/*ad*/	case LODSw: {	int m = _mode|MOVSSRC;
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(S_REG, m, Ofs_EAX); PC++;
#ifndef SINGLESTEP
			/* optimize common sequence LODSw-STOSw */
			if (!(_mode & MSSTP) && Fetch(PC) == STOSw &&
					!e_querymark(PC, 1)) {
				Gen(O_MOVS_SetA, (m&ADDR16)|MOVSDST, OVERR_DS);
				Gen(S_DI, m);
				m |= MOVSDST;
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, m, OVERR_DS);
			} break;
/*ae*/	case SCASb: {	int m = _mode|(MBYTE|MOVSDST);
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_DI_R1, m);		// mov al,[edi]
			Gen(O_CMP_FR, m, Ofs_AL);	// cmp [ebx+reg],al
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; } break;
/*af*/	case SCASw: {	int m = _mode|MOVSDST;
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_DI_R1, m);		// mov ax,[edi]
			Gen(O_CMP_FR, m, Ofs_EAX);	// cmp [ebx+reg],ax
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; } break;

/*b0*/	case MOVial:
/*b1*/	case MOVicl:
/*b2*/	case MOVidl:
/*b3*/	case MOVibl:
/*b4*/	case MOViah:
/*b5*/	case MOVich:
/*b6*/	case MOVidh:
/*b7*/	case MOVibh:
			Gen(L_IMM, _mode|MBYTE, SEL_B_X(D_LO(opc)), Fetch(PC+1));
			PC += 2; break;
/*b8*/	case MOViax:
/*b9*/	case MOVicx:
/*ba*/	case MOVidx:
/*bb*/	case MOVibx:
/*bc*/	case MOVisp:
/*bd*/	case MOVibp:
/*be*/	case MOVisi:
/*bf*/	case MOVidi:
			Gen(L_IMM, _mode, R1Tab_l[D_LO(opc)], DataFetchWL_U(_mode,PC+1));
			INC_WL_PC(_mode, 1); break;

/*d0*/	case SHIFTb:
/*d2*/	case SHIFTbv:
/*c0*/	case SHIFTbi: {
			int m = _mode | MBYTE;
			unsigned char count = 0;
			PC += ModRM(opc, PC, m|MLOAD);
			if (opc==SHIFTb) { m |= IMMED; count = 1; }
			else if (opc==SHIFTbi) {
				m |= IMMED; count = Fetch(PC)&0x1f; PC++;
			}
			switch (REG1) {
			case Ofs_AL:	/*0*/	// ROL
				Gen(O_ROL, m, count);
				break;
			case Ofs_CL:	/*1*/	// ROR
				Gen(O_ROR, m, count);
				break;
			case Ofs_DL:	/*2*/	// RCL
				Gen(O_RCL, m, count);
				break;
			case Ofs_BL:	/*3*/	// RCR
				Gen(O_RCR, m, count);
				break;
			case Ofs_DH:	/*6*/	// undoc
				if (opc==SHIFTbv) {
					goto illegal_op;
				}
				break;
			case Ofs_AH:	/*4*/	// SHL,SAL
				Gen(O_SHL, m, count);
				break;
			case Ofs_CH:	/*5*/	// SHR
				Gen(O_SHR, m, count);
				break;
			case Ofs_BH:	/*7*/	// SAR
				Gen(O_SAR, m, count);
				break;
			}
			if (REG3)
				Gen(S_REG, m, REG3);
			else
				Gen(S_DI, m);
		}
		break;
/*d1*/	case SHIFTw:
/*d3*/	case SHIFTwv:
/*c1*/	case SHIFTwi: {
			int m = _mode;
			unsigned char count = 0;
			PC += ModRM(opc, PC, m|MLOAD);
			if (opc==SHIFTw) { m |= IMMED; count = 1; }
			else if (opc==SHIFTwi) {
				m |= IMMED; count = Fetch(PC)&0x1f; PC++;
			}
			switch (REG1) {
			case Ofs_AX:	/*0*/	// ROL
				Gen(O_ROL, m, count);
				break;
			case Ofs_CX:	/*1*/	// ROR
				Gen(O_ROR, m, count);
				break;
			case Ofs_DX:	/*2*/	// RCL
				Gen(O_RCL, m, count);
				break;
			case Ofs_BX:	/*3*/	// RCR
				Gen(O_RCR, m, count);
				break;
			case Ofs_SI:	/*6*/	// undoc
				if ((opc==SHIFTw)||(opc==SHIFTwv)) {
					goto illegal_op;
				}
			case Ofs_SP:	/*4*/	// SHL,SAL
				Gen(O_SHL, m, count);
				break;
			case Ofs_BP:	/*5*/	// SHR
				Gen(O_SHR, m, count);
				break;
			case Ofs_DI:	/*7*/	// SAR
				Gen(O_SAR, m, count);
				break;
			}
			if (REG3)
				Gen(S_REG, m, REG3);
			else
				Gen(S_DI, m);
		}
		break;

/*e9*/	case JMPd:
/*e8*/	case CALLd:
		    PC = JumpGen(PC, Interp_LONG_CS, _mode, opc,
				 1 + BT24(BitDATA16,_mode));
		    break;

/*9a*/	case CALLl:
/*ea*/	case JMPld: {
		    int len = 3 + BT24(BitDATA16,_mode);
		    PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, len);
		    if (debug_level('e')>2) {
			dosaddr_t oip = PC - ocs;
			unsigned short ncs = FetchW(PC-2);
			dosaddr_t nip = DataFetchWL_U(_mode,PC-2-
						      BT24(BitDATA16,_mode));
			if (opc==CALLl)
			    e_printf("CALL_FAR: ret=%04x:%08x\n  calling:	   %04x:%08x\n",
				     ocs,oip,ncs,nip);
			else
			    e_printf("JMP_FAR: %04x:%08x\n",ncs,nip);
		    }
		    }
		    break;

/*c2*/	case RETisp: {
			int dr = (signed short)FetchW(PC+1);
			Gen(O_POP, _mode|MRETISP, dr);
			PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 3);
			if (debug_level('e')>2)
				e_printf("RET: ret=%08x inc_sp=%d\n",PC-Interp_LONG_CS,dr);
			}
			break;
/*c3*/	case RET:
			Gen(O_POP, _mode);
			PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 1);
			if (debug_level('e')>2) e_printf("RET: ret=%08x\n",PC-Interp_LONG_CS);
			break;
/*c6*/	case MOVbirm:
			PC += ModRM(opc, PC, _mode|MBYTE);
			if (REG3)
			    Gen(L_IMM, _mode|MBYTE, REG3, Fetch(PC));
			else
			    Gen(S_DI_IMM, _mode|MBYTE, Fetch(PC));
			PC++; break;
/*c7*/	case MOVwirm:
			PC += ModRM(opc, PC, _mode);
			if (REG3)
			    Gen(L_IMM, _mode, REG3, DataFetchWL_U(_mode,PC));
			else
			    Gen(S_DI_IMM, _mode, DataFetchWL_U(_mode,PC));
			INC_WL_PC(_mode,0);
			break;
/*c8*/	case ENTER: {
			int allocsize = FetchW(PC+1);
			int level = Fetch(PC+3) & 0x1f;
			Gen(L_REG, _mode, Ofs_EBP);
			Gen(O_PUSH, _mode);
			if (level >= 1) {
				Gen(L_REG, _mode, Ofs_ESP);
				if (level >= 2)
					Gen(O_SIM, _mode, opc, level, P0);
				Gen(O_PUSH, _mode);
				Gen(S_REG, _mode, Ofs_EBP);
			}
			else {
				Gen(L_REG2REG, _mode, Ofs_ESP, Ofs_EBP);
			}
			// subtract AllocSize from ESP via
			// "lea -allocsize(%esp), %esp"
			if (allocsize) {
				AddrGen(A_DI_1,
					_mode|MLEA|((_mode&DATA16)?ADDR16:0)|IMMED,
					0, -allocsize, Ofs_ESP);
				Gen(S_DI_R, _mode, Ofs_ESP);
			}
			PC += 4; }
			break;
/*c9*/	case LEAVE:
			Gen(O_LEAVE, _mode); PC++;
			break;
/*ca*/	case RETlisp: {	/* restartable */
			/* pop from stack without adjusting esp before A_SR_* */
			int dr = (signed short)FetchW(PC+1);
			if (REALADDR()) {
				Gen(O_POP1, _mode);
				Gen(O_POP2, _mode, Ofs_EIP);
				Gen(O_POP2, _mode|MNOREG|MRETISP, dr);
				AddrGen(A_SR_SH4, _mode, Ofs_CS, Ofs_XCS);
				Gen(O_POP3, _mode);
				Gen(L_REG, _mode, Ofs_EIP);
			}
			else {
				Gen(O_SIM, _mode, opc, dr, P0);
			}
			PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 3);
			if (debug_level('e')>2)
				e_printf("RET_%d: ret=%08x\n",dr,TheCPU.eip);
			}
			break;
/*cc*/	case INT3:
			e_printf("Interrupt 03\n");
			PC = P0 = ExceptionGen(PC+1, _mode, EXCP03_INT3, 0, P0);
			break;
/*ce*/	case INTO:
			PC++;
			Gen(O_SIM, _mode, opc, 0, PC);
			break;
/*cd*/	case INT: {
			int inum = Fetch(PC+1);
#ifdef ASM_DUMP
			fprintf(aLog,"%08x:\t\tint %02x\n", P0, inum);
#endif
			if (V86MODE() && (TheCPU.cr[4] & CR4_VME)) {
				if (IOPL == 3) {
					Gen(O_INT, _mode, inum, P0);
					Gen(O_PUSH2F, _mode);
					Gen(O_SETFL, _mode, INT);
				} else {
					Gen(O_INT, _mode, inum, P0);
					Gen(O_SIM, _mode, opc, inum, P0);
					Gen(O_PUSH, _mode);
				}
				Gen(L_REG, _mode, Ofs_CS);
				Gen(O_PUSH, _mode);
				Gen(O_PUSHI, _mode, PC + 2 - Interp_LONG_CS);
				Gen(L_LXS2, _mode);
				AddrGen(A_SR_SH4, _mode, Ofs_CS, Ofs_XCS);
				Gen(L_DI_R1, _mode);
				PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 2);
				if (debug_level('e')>1)
					dbug_printf("EMU86: directly called int %#x ax=%#x at %#x:%#x\n",
						    inum, TheCPU.eax, ocs, PC - Interp_LONG_CS);
				break;
			}
			// V86: always #GP(0) if revectored or without VME
			if (V86MODE() && !(TheCPU.cr[4] & CR4_VME) && IOPL<3) {
				PC += 2; goto not_permitted;
			}
			/* protected _mode INT or revectored V86 with IOPL<3 */
			switch(inum) {
			case 0x03:
			    if (PROTMODE())
				PC = P0 = ExceptionGen(PC+2, _mode,
						       EXCP03_INT3, 0, P0);
			    break;
			case 0x04:
			    if (PROTMODE())
				PC = P0 = ExceptionGen(PC+2, _mode,
						       EXCP04_INTO, 0, P0);
			    break;
			default:
			    if (debug_level('e')>1)
				e_printf("!!! Not permitted int %x\n",inum);
			    PC = P0 = ExceptionGen(PC+2, _mode, EXCP0D_GPF,
						   (inum << 3) | 2, P0);
			    break;
			}
			break;
		}

/*cb*/	case RETl:
			/* pop from stack without adjusting esp before A_SR_* */
			if (REALADDR()) {
				Gen(O_POP1, _mode);
				Gen(O_POP2, _mode, Ofs_EIP);
				Gen(O_POP2, _mode|MNOREG);
				AddrGen(A_SR_SH4, _mode, Ofs_CS, Ofs_XCS);
				Gen(O_POP3, _mode);
				Gen(L_REG, _mode, Ofs_EIP);
			}
			else {
				Gen(O_SIM, _mode, opc, 0, P0);
			}
			PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 1);
			break;

/*cf*/	case IRET:	/* restartable */
			if (V86MODE() && IOPL!=3 && !(TheCPU.cr[4] & CR4_VME)) {
			    PC++; goto not_permitted;	/* GPF */
			}
			Gen(O_SIM, _mode, opc, 0, P0);
			PC = JumpGen(PC, Interp_LONG_CS, _mode, opc, 1);
			break;
/*9d*/	case POPF:
			PC++;
			if (V86MODE() && IOPL!=3 && !(TheCPU.cr[4] & CR4_VME)) {
			    goto not_permitted;	/* GPF */
			}
			Gen(O_POP, _mode);
			Gen(O_SIM, _mode, opc, 0, PC);
			break;

/*f2*/	case REPNE:
/*f3*/	case REP: /* also is REPE */ {
			unsigned char repop; int repmod, realrepmod;
			PC++;
			repmod = _mode | (opc==REPNE? MREPNE:MREP);
repag0:
			realrepmod = repmod;
			if ((_mode & MSSTP) &&
			    ((repmod & ADDR16) ? rCX : rECX) > 0) {
				/* use LOOP for REP below with TF set, because
				   we must trap for every string ins */
				repmod = repmod & ~(MREPNE|MREP);
			}
			repop = Fetch(PC);
			switch (repop) {
				case INSb:
				case INSw:
				case OUTSb:
				case OUTSw:
					PC++; goto not_permitted;
				case LODSb:
					repmod |= (MBYTE|MOVSSRC);
					Gen(O_MOVS_SetA, repmod, OVERR_DS);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_LodD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
					}
					Gen(S_REG, repmod, Ofs_AL);
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case LODSw:
					repmod |= MOVSSRC;
					Gen(O_MOVS_SetA, repmod, OVERR_DS);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_LodD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
					}
					Gen(S_REG, repmod, Ofs_AX);
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case MOVSb:
					repmod |= (MBYTE|MOVSSRC|MOVSDST);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod, OVERR_DS);
						Gen(O_MOVS_MovD, repmod);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST, OVERR_DS);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC, OVERR_DS);
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case MOVSw:
					repmod |= (MOVSSRC|MOVSDST);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod, OVERR_DS);
						Gen(O_MOVS_MovD, repmod);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST, OVERR_DS);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC, OVERR_DS);
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case CMPSb:
					repmod |= (MBYTE|MOVSSRC|MOVSDST|MREPCOND);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod, OVERR_DS);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST, OVERR_DS);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC, OVERR_DS);
					}
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case CMPSw:
					repmod |= (MOVSSRC|MOVSDST|MREPCOND);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod, OVERR_DS);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST, OVERR_DS);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC, OVERR_DS);
					}
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case STOSb:
					repmod |= (MBYTE|MOVSDST);
					Gen(O_MOVS_SetA, repmod, OVERR_DS);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_StoD, repmod);
					}
					else {
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case STOSw:
					repmod |= MOVSDST;
					Gen(O_MOVS_SetA, repmod, OVERR_DS);
					Gen(L_REG, repmod, Ofs_EAX);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_StoD, repmod);
					}
					else {
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case SCASb:
					repmod |= (MBYTE|MOVSDST|MREPCOND);
					Gen(O_MOVS_SetA, repmod, OVERR_DS);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_ScaD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
						Gen(O_CMP_FR, repmod, Ofs_AL);
					}
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case SCASw:
					repmod |= MOVSDST|MREPCOND;
					Gen(O_MOVS_SetA, repmod, OVERR_DS);
					Gen(L_REG, repmod, Ofs_EAX);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_ScaD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
						Gen(O_CMP_FR, repmod, Ofs_AL);
					}
					Gen(O_MOVS_SavA, repmod, OVERR_DS);
					PC++; break;
				case SEGes:
					OVERR_DS = OVERR_SS = Ofs_XES; PC++; goto repag0;
				case SEGcs:
					OVERR_DS = OVERR_SS = Ofs_XCS; PC++; goto repag0;
				case SEGss:
					OVERR_DS = OVERR_SS = Ofs_XSS; PC++; goto repag0;
				case SEGds:
					OVERR_DS = OVERR_SS = Ofs_XDS; PC++; goto repag0;
				case SEGfs:
					OVERR_DS = OVERR_SS = Ofs_XFS; PC++; goto repag0;
				case SEGgs:
					OVERR_DS = OVERR_SS = Ofs_XGS; PC++; goto repag0;
				case OPERoverride:
					repmod = (repmod & ~DATA16) | (~basemode & DATA16);
					if (debug_level('e')>4)
					    e_printf("OPERoverride: new _mode %04x\n",repmod);
					PC++; goto repag0;
				case ADDRoverride:
					repmod = (repmod & ~ADDR16) | (~basemode & ADDR16);
					if (debug_level('e')>4)
					    e_printf("ADDRoverride: new _mode %04x\n",repmod);
					PC++; goto repag0;
			}
			if ((_mode & MSSTP) && !(repmod & (MREP|MREPNE))) {
				/* with TF set, we use LOOP instead of REP so we can stop at
				   every iteration */
				int op = LOOP;
				if (repmod & MREPCOND)
					op = (realrepmod&MREP) ? LOOPZ_LOOPE : LOOPNZ_LOOPNE;
				Gen(JLOOP_LINK, _mode, op);
				Gen(JMP_LINK, _mode, PC, InstrMeta[0].npc);
				Gen(JMP_LINK, _mode, P0, InstrMeta[0].npc);
				/* code will be flushed immediately
				   in interp_post because TF is set */
				break;
			} }
			break;
/*f4*/	case HLT:
			PC++; goto not_permitted;
/*f5*/	case CMC:	PC++;
			Gen(O_SETFL, _mode, CMC);
			break;
/*f6*/	case GRP1brm: {
			PC += ModRM(opc, PC, _mode|MBYTE|MLOAD);	// al=[rm]
			switch(REG1) {
			case Ofs_AL:	/*0*/	/* TEST */
			case Ofs_CL:	/*1*/	/* undocumented */
				Gen(O_AND_R, _mode|MBYTE|IMMED, Fetch(PC));	// op al,#imm
				PC++;
				break;
			case Ofs_DL:	/*2*/	/* NOT */
				Gen(O_NOT, _mode|MBYTE);			// not al
				if (REG3)
					Gen(S_REG, _mode|MBYTE, REG3);
				else
					Gen(S_DI, _mode|MBYTE);
				break;
			case Ofs_BL:	/*3*/	/* NEG */
				Gen(O_NEG, _mode|MBYTE);			// neg al
				if (REG3)
					Gen(S_REG, _mode|MBYTE, REG3);
				else
					Gen(S_DI, _mode|MBYTE);
				break;
			case Ofs_AH:	/*4*/	/* MUL AL */
				Gen(O_MUL, _mode|MBYTE);			// al*[edi]->AX unsigned
				break;
			case Ofs_CH:	/*5*/	/* IMUL AL */
				Gen(O_IMUL, _mode|MBYTE);		// al*[edi]->AX signed
				break;
			case Ofs_DH:	/*6*/	/* DIV AL */
				Gen(O_DIV, _mode|MBYTE, P0 - Interp_LONG_CS);			// ah:al/[edi]->AH:AL unsigned
				break;
			case Ofs_BH:	/*7*/	/* IDIV AL */
				Gen(O_IDIV, _mode|MBYTE, P0 - Interp_LONG_CS);		// ah:al/[edi]->AH:AL signed
				break;
			} }
			break;
/*f7*/	case GRP1wrm: {
			PC += ModRM(opc, PC, _mode|MLOAD);	// (e)ax=[rm]
			switch(REG1) {
			case Ofs_AX:	/*0*/	/* TEST */
			case Ofs_CX:	/*1*/	/* undocumented */
				Gen(O_AND_R, _mode|IMMED, DataFetchWL_U(_mode,PC));	// op al,#imm
				INC_WL_PC(_mode,0);
				break;
			case Ofs_DX:	/*2*/	/* NOT */
				Gen(O_NOT, _mode);			// not (e)ax
				if (REG3)
					Gen(S_REG, _mode, REG3);
				else
					Gen(S_DI, _mode);
				break;
			case Ofs_BX:	/*3*/	/* NEG */
				Gen(O_NEG, _mode);			// neg (e)ax
				if (REG3)
					Gen(S_REG, _mode, REG3);
				else
					Gen(S_DI, _mode);
				break;
			case Ofs_SP:	/*4*/	/* MUL AX */
				Gen(O_MUL, _mode);			// (e)ax*[edi]->(E)DX:(E)AX unsigned
				break;
			case Ofs_BP:	/*5*/	/* IMUL AX */
				Gen(O_IMUL, _mode);			// (e)ax*[edi]->(E)DX:(E)AX signed
				break;
			case Ofs_SI:	/*6*/	/* DIV AX+DX */
				Gen(O_DIV, _mode, P0 - Interp_LONG_CS);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX unsigned
				break;
			case Ofs_DI:	/*7*/	/* IDIV AX+DX */
				Gen(O_IDIV, _mode, P0 - Interp_LONG_CS);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX signed
				break;
			} }
			break;
/*f8*/	case CLC:	PC++;
			Gen(O_SETFL, _mode, CLC);
			break;
/*f9*/	case STC:	PC++;
			Gen(O_SETFL, _mode, STC);
			break;
/*fa*/	case CLI:
			PC++;
			if (REALMODE() || (CPL <= IOPL) || (IOPL==3)) {
				Gen(O_SETFL, _mode, CLI);
			}
			else {
			    if ((V86MODE() && !(TheCPU.cr[4] & CR4_VME)) ||
				(!V86MODE() && !(TheCPU.cr[4] & CR4_PVI)))
				goto not_permitted;	/* GPF */
			    Gen(O_SIM, _mode, opc, 0, P0);
			}
			break;
/*fb*/	case STI:
			PC++;
			if (debug_level('e')>2 && V86MODE())
				e_printf("Virtual VM86 STI\n");
			if (!(REALMODE() || (CPL <= IOPL) || (IOPL==3)) &&
			    ((V86MODE() && !(TheCPU.cr[4] & CR4_VME)) ||
			     (!V86MODE() && !(TheCPU.cr[4] & CR4_PVI))))
				goto not_permitted;	/* GPF */
			Gen(O_SIM, _mode, opc, 0, PC);
			/* real mode inhibits after STI as well but
			   we've always relied on trapping behaviour with vm86 */
			if (!V86MODE())
				InstrMeta[CurrIMeta].flags |= F_INHI;
			break;
/*fc*/	case CLD:	PC++;
			Gen(O_SETFL, _mode, CLD);
			break;
/*fd*/	case STD:	PC++;
			Gen(O_SETFL, _mode, STD);
			break;
/*fe*/	case GRP2brm:	/* only INC and DEC are legal on bytes */
			ModGetReg1(PC, _mode);
			switch(REG1) {
			case Ofs_AL:	/*0*/
				if (Fetch(PC+1) >= 0xc0) {
					Gen(O_INC_R, _mode|MBYTE,
					    R1Tab_b[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, _mode|MBYTE|MLOAD);//al=[rm]
				Gen(O_INC, _mode|MBYTE);
				Gen(S_DI, _mode|MBYTE);
				break;
			case Ofs_CL:	/*1*/
				if (Fetch(PC+1) >= 0xc8) {
					Gen(O_DEC_R, _mode|MBYTE,
					    R1Tab_b[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, _mode|MBYTE|MLOAD);//al=[rm]
				Gen(O_DEC, _mode|MBYTE);
				Gen(S_DI, _mode|MBYTE);
				break;
			default:
				PC += 2; goto illegal_op;
			}
			break;
/*ff*/	case GRP2wrm:
			ModGetReg1(PC, _mode);
			switch (REG1) {
			case Ofs_AX:	/*0*/
				/* it's not worth optimizing for registers
				   here (single byte DEC/INC exist), but do
				   it for consistency anyway.
				*/
				if (Fetch(PC+1) >= 0xc0) {
					Gen(O_INC_R, _mode,
					    R1Tab_l[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_INC, _mode);
				Gen(S_DI, _mode);
				break;
			case Ofs_CX:	/*1*/
				if (Fetch(PC+1) >= 0xc8) {
					Gen(O_DEC_R, _mode,
					    R1Tab_l[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_DEC, _mode);
				Gen(S_DI, _mode);
				break;
			case Ofs_SP:	/*4*/	 // JMP near indirect
				PC = JumpGen(PC, Interp_LONG_CS, _mode, (opc<<8)|REG1,
					ModRM(opc, PC, _mode|NOFLDR|MLOAD));
				break;
			case Ofs_DX: {	/*2*/	 // CALL near indirect
				/* don't use MLOAD as O_PUSHI clobbers eax */
				int len = ModRM(opc, PC, _mode|NOFLDR);
				dosaddr_t ret = PC + len - Interp_LONG_CS;
				Gen(O_PUSHI, _mode, ret);
				if (REG3)
					Gen(L_REG, _mode, REG3);
				else
					Gen(L_DI_R1, _mode);
				PC = JumpGen(PC, Interp_LONG_CS, _mode,
					     (opc<<8)|REG1, len);
				if (debug_level('e')>2)
					e_printf("CALL indirect: ret=%08x\n\tcalling: %08x\n",
						 ret,PC-Interp_LONG_CS);
				}
				break;
			case Ofs_BX:	/*3*/	 // CALL long indirect restartable
			case Ofs_BP:	/*5*/	 // JMP long indirect restartable
				if (Fetch(PC+1) >= 0xc0) {
					PC += 2; goto illegal_op;
				}
				{
					dosaddr_t oip = 0;
					int len = ModRM(opc, PC, _mode|NOFLDR);
					Gen(L_LXS2, _mode);
					if (REALADDR())
					    AddrGen(A_SR_SH4, _mode, Ofs_CS, Ofs_XCS);
					else
					    AddrGen(A_SR_PROT, _mode, Ofs_CS, P0);
					if (REG1==Ofs_BX) {
					    /* ok, now push old cs:eip */
					    oip = PC + len - Interp_LONG_CS;
					    Gen(O_PUSH, _mode);
					    Gen(O_PUSHI, _mode, oip);
					}
					Gen(L_DI_R1, _mode);
					PC = JumpGen(PC, Interp_LONG_CS, _mode,
						     (opc<<8)|REG1, len);
					if (debug_level('e')>2) {
					    if (REG1==Ofs_BX)
						e_printf("CALL_FAR indirect: ret=%04x:%08x\n",
							 ocs,oip);
					}
				}
				break;
			case Ofs_SI:	/*6*/	 // PUSH
				PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_PUSH, _mode); break;	// push [rm]
				break;
			default:
				PC += 2; goto illegal_op;
			}
			break;

/*6c*/	case INSb:
/*6d*/	case INSw: {	int m = _mode|MOVSDST;
			if (opc == INSb) m |= MBYTE;
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(O_SIM, m, opc, 0, P0);
			Gen(S_DI, m);
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; }
			break;
/*6e*/	case OUTSb:
/*6f*/	case OUTSw: {	int m = _mode|MOVSSRC;
			if (opc == OUTSb) m |= MBYTE;
			Gen(O_MOVS_SetA, m, OVERR_DS);
			Gen(L_DI_R1, m);
			Gen(O_SIM, m, opc, 0, P0);
			Gen(O_MOVS_SavA, m, OVERR_DS);
			PC++; }
			break;
/*ec*/	case INvb:
/*ed*/	case INvw:
/*ee*/	case OUTvb:
/*ef*/	case OUTvw:
			Gen(O_SIM, _mode, opc, 0, P0);
			PC++;
			break;
/*e4*/	case INb:
/*e5*/	case INw:
/*e6*/	case OUTb:
/*e7*/	case OUTw:
			Gen(O_SIM, _mode, opc, Fetch(PC+1), P0);
			PC += 2;
			break;

/*d8*/	case ESC0:
/*d9*/	case ESC1:
/*da*/	case ESC2:
/*db*/	case ESC3:
/*dc*/	case ESC4:
/*dd*/	case ESC5:
/*de*/	case ESC6:
/*df*/	case ESC7:  {
			unsigned char b=Fetch(PC+1);
			// extended opcode
			// 1101.1ooo xxeeerrr -> eeeooo,rrr
			// D8 -> 00,08,10,18...38
			// DF -> 07,0f,17,1f...3f
			int exop = (b & 0x38) | (opc & 7);
			if ((b&0xc0)==0xc0) {
				exop |= 0x40;
				PC += 2;
			}
			else {
				PC += ModRM(opc, PC, _mode|NOFLDR);
			}
			b &= 7;
			if (Fp87_illegal_op(exop, b)) {
				goto illegal_op;
			}
			if ((exop&0xeb)==0x21)
			    Gen(O_SIM, _mode, opc, exop, P0);
			else
			    Gen(O_FOP, _mode, exop, b);
			}
			break;

/*0f*/	case TwoByteESC: {
			unsigned char opc2 = Fetch(PC+1);
			switch (opc2) {
//
			case 0x00: { /* GRP6 - Extended Opcode 20 */
				unsigned char opm = D_MO(Fetch(PC+2));
				switch (opm) {
				case 0: /* SLDT */
				case 1: /* STR: Store Task Register */
				    if (!PROTMODE()) {
					PC += 3; goto illegal_op;
				    }
				    PC++; PC += ModRM(opc, PC, _mode);
				    Gen(O_SIM, _mode, 0x100, opm, P0);
				    if (REG3)
					Gen(S_REG, _mode, REG3);
				    else
					Gen(S_DI, _mode);
				    break;
				case 2: /* LLDT */
				    /* Load Local Descriptor Table Register */
				case 3: /* LTR */
				    /* Load Task Register */
				    PC += 3; goto not_permitted;
				case 4: /* VERR */
				case 5: /* VERW */
				    if (!PROTMODE()) {
					PC += 3; goto illegal_op;
				    }
				    PC++; PC += ModRM(opc, PC, _mode|DATA16|MLOAD);
				    Gen(O_SIM, _mode, 0x100, opm, P0);
				    break;
				case 6: /* JMP indirect to IA64 code */
				case 7: /* Illegal */
				    PC += 3; goto illegal_op;
				} }
				break;
			case 0x01: { /* GRP7 - Extended Opcode 21 */
				unsigned char opm = D_MO(Fetch(PC+2));
				switch (opm) {
				case 0: /* SGDT */
				    /* Store Global Descriptor Table Register */
				    PC++; PC += ModRM(opc, PC, _mode|DATA16|MSTORE);
				    error("SGDT not implemented\n");
				    break;
				case 1: /* SIDT */
				    /* Store Interrupt Descriptor Table Register */
				    PC++; PC += ModRM(opc, PC, _mode|DATA16|MSTORE);
				    error("SIDT not implemented\n");
				    break;
				case 2: /* LGDT */ /* PM privileged AND real _mode */
				    /* Load Global Descriptor Table Register */
				case 3: /* LIDT */ /* PM privileged AND real _mode */
				    /* Load Interrupt Descriptor Table Register */
				    PC += 3; goto not_permitted;
				case 4: /* SMSW, 80286 compatibility */
				    /* Store Machine Status Word */
				    Gen(L_CR0, _mode);
				    PC++; PC += ModRM(opc, PC, _mode|DATA16|MSTORE);
				    break;
				case 5: /* Illegal */
				case 6: /* LMSW, 80286 compatibility, Privileged */
				    /* Load Machine Status Word */
				case 7: /* Illegal */
				    PC += 3; goto illegal_op;
				} }
				break;

			case 0x02:   /* LAR */ /* Load Access Rights Byte */
			case 0x03:   /* LSL */ /* Load Segment Limit */
				if (REALMODE()) {
				    PC += 3; goto illegal_op;
				}
				PC++; PC += ModRM(opc, PC, _mode|DATA16|MLOAD);
				Gen(O_SIM, _mode, 0x100+opc2, REG1, P0);
				break;

			/* case 0x04:	LOADALL */
			/* case 0x05:	LOADALL(286) - SYSCALL(K6) */
			case 0x06: /* CLTS */ /* Privileged */
				/* Clear Task State Register */
				PC += 2;
				if (CPL != 0) goto not_permitted;
				Gen(O_SIM, _mode, 0x100+opc2, 0, P0);
				break;
			/* case 0x07:	LOADALL(386) - SYSRET(K6) etc. */
			case 0x08: /* INVD */
				/* INValiDate cache */
			case 0x09: /* WBINVD */
				/* Write-Back and INValiDate cache */
				PC += 2; break;
			/* case 0x0a: */
			/* case 0x0b: UD2 */
			/* case 0x0d:	Code Extension 25(AMD-3D) */
			/* case 0x0e:	FEMMS(K6-3D) */
			/* case 0x0f:	AMD-3D */
			/* case 0x10-0x1f:	various V20/MMX instr. */
			case 0x20:   /* MOVcdrd */ /* Privileged */
			case 0x22:   /* MOVrdcd */ /* Privileged */
			case 0x21:   /* MOVddrd */ /* Privileged */
			case 0x23:   /* MOVrddd */ /* Privileged */
			case 0x24:   /* MOVtdrd */ /* Privileged */
			case 0x26: { /* MOVrdtd */ /* Privileged */
				int reg; unsigned char b;
				if (V86MODE()) {
				    PC += 3; goto not_permitted;
				}
				b = Fetch(PC+2);
				reg = D_MO(b);
				/* D_HO(b) (mod field) ignored */
				if (((opc2&4) && (reg<6)) ||
				    (!(opc2&5) && ((reg==1)||(reg>4)))) {
				    PC += 3; goto illegal_op;
				}
				b = D_LO(b);
				if (opc2&2)
				    Gen(L_REG, _mode&~DATA16, b);
				Gen(O_SIM, _mode, 0x100+opc2, reg, P0);
				if (!(opc2&2))
				    Gen(S_REG, _mode&~DATA16, b);
		    		} PC += 3; break;
			case 0x30: /* WRMSR */
			    PC += 2; goto not_permitted;

			case 0x31: /* RDTSC */
				Gen(O_RDTSC, _mode);
				PC+=2; break;
			case 0x32: /* RDMSR */
			    PC += 2; goto not_permitted;

			/* case 0x33:	RDPMC(P6) */
			/* case 0x34:	SYSENTER(PII) */
			/* case 0x35:	SYSEXIT(PII) */
			/* case 0x40-0x4f:	CMOV(P6) */
			/* case 0x50-0x5f:	various Cyrix/MMX */
			/* case 0x60 ... 0x6b:	MMX */
			/* case 0x6e: case 0x6f: */
			/* case 0x74 ... 0x77: */
			/* case 0x78-0x7e:	Cyrix */
			/* case 0x7e: case 0x7f: */
//
			case JOimmdisp:		/*80*/
			case JNOimmdisp:	/*81*/
			case JBimmdisp:		/*82*/
			case JNBimmdisp:	/*83*/
			case JZimmdisp:		/*84*/
			case JNZimmdisp:	/*85*/
			case JBEimmdisp:	/*86*/
			case JNBEimmdisp:	/*87*/
			case JSimmdisp:		/*88*/
			case JNSimmdisp:	/*89*/
			case JPimmdisp:		/*8a*/
			case JNPimmdisp:	/*8b*/
			case JLimmdisp:		/*8c*/
			case JNLimmdisp:	/*8d*/
			case JLEimmdisp:	/*8e*/
			case JNLEimmdisp:	/*8f*/
				{
				  PC = JumpGen(PC, Interp_LONG_CS, _mode,
					       JO+(opc2-JOimmdisp),
					       2 + BT24(BitDATA16,_mode));
				}
				break;
///
			case SETObrm:		/*90*/
			case SETNObrm:		/*91*/
			case SETBbrm:		/*92*/
			case SETNBbrm:		/*93*/
			case SETZbrm:		/*94*/
			case SETNZbrm:		/*95*/
			case SETBEbrm:		/*96*/
			case SETNBEbrm:		/*97*/
			case SETSbrm:		/*98*/
			case SETNSbrm:		/*99*/
			case SETPbrm:		/*9a*/
			case SETNPbrm:		/*9b*/
			case SETLbrm:		/*9c*/
			case SETNLbrm:		/*9d*/
			case SETLEbrm:		/*9e*/
			case SETNLEbrm:		/*9f*/
				Gen(O_SETCC, _mode, (opc2&0x0f));
				PC++; PC += ModRM(opc, PC, _mode|MBYTE|MSTORE);
				break;
///
			case 0xa0: /* PUSHfs */
				Gen(L_REG, _mode, Ofs_FS);
				Gen(O_PUSH, _mode); PC+=2;
				break;
			case 0xa1: /* POPfs */
				if (REALADDR()) {
				    Gen(O_POP, _mode);
				    AddrGen(A_SR_SH4, _mode, Ofs_FS, Ofs_XFS);
				} else { /* restartable */
				    Gen(O_POP1, _mode);
				    Gen(O_POP2, _mode|MNOREG);
				    AddrGen(A_SR_PROT, _mode, Ofs_FS, P0);
				    Gen(O_POP3, _mode);
				}
				PC+=2;
				break;
///
			case 0xa2: /* CPUID */
				Gen(O_SIM, _mode, 0x100+opc2, 0, P0);
				PC+=2; break;

			case 0xa3: /* BT */
			case 0xab: /* BTS */
			case 0xb3: /* BTR */
			case 0xbb: /* BTC */
				PC++; PC += ModRM(opc, PC, _mode);
				if (REG3) {
				    Gen(L_REG, _mode, REG3);
				}
				Gen(O_BITOP, _mode | (REG3 ? RM_REG : 0),
				    (opc2-0xa0), REG1);
				if (opc2 != 0xa3) {
				    if (REG3)
					Gen(S_REG, _mode, REG3);
				}
				break;
			case 0xbc: /* BSF */
			case 0xbd: /* BSR */
				PC++; PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_BITOP, _mode, (opc2-0xa0), REG1);
				break;
			case 0xba: { /* GRP8 - Code Extension 22 */
				unsigned char opm = (Fetch(PC+2))&0x38;
				switch (opm) {
				case 0x00: /* Illegal */
				case 0x08: /* Illegal */
				case 0x10: /* Illegal */
				case 0x18: /* Illegal */
				    PC += 3; goto illegal_op;
				case 0x20: /* BT imm8 */
				case 0x28: /* BTS imm8 */
				case 0x30: /* BTR imm8 */
				case 0x38: /* BTC imm8 */
					PC++; PC += ModRM(opc, PC, _mode|MLOAD);
					Gen(O_BITOP, _mode, opm, Fetch(PC));
					if (opm != 0x20) {
					    if (REG3) {
						Gen(S_REG, _mode, REG3);
					    }
					    else {
						Gen(S_DI, _mode);
					    }
					}
					PC++;
					break;
				} }
				break;

			case 0xa4: /* SHLDimm */
			    /* Double Precision Shift Left by IMMED */
			case 0xa5: /* SHLDcl */
			    /* Double Precision Shift Left by CL */
			case 0xac: /* SHRDimm */
			    /* Double Precision Shift Left by IMMED */
			case 0xad: /* SHRDcl */
			    /* Double Precision Shift Left by CL */
				PC++; PC += ModRM(opc, PC, _mode|MLOAD);
				if (opc2&1) {
					Gen(O_SHFD, _mode, (opc2&8), REG1);
				}
				else {
					Gen(O_SHFD, _mode|IMMED, (opc2&8), REG1, Fetch(PC));
					PC++;
				}
				if (REG3)
					Gen(S_REG, _mode, REG3);
				else
					Gen(S_DI, _mode);
				break;

			/* case 0xa6: CMPXCHGb (486 STEP A only) */
			/* case 0xa7: CMPXCHGw (486 STEP A only) */
///
			case 0xa8: /* PUSHgs */
				Gen(L_REG, _mode, Ofs_GS);
				Gen(O_PUSH, _mode); PC+=2;
				break;
			case 0xa9: /* POPgs */
				if (REALADDR()) {
				    Gen(O_POP, _mode);
				    AddrGen(A_SR_SH4, _mode, Ofs_GS, Ofs_XGS);
				} else { /* restartable */
				    Gen(O_POP1, _mode);
				    Gen(O_POP2, _mode|MNOREG);
				    AddrGen(A_SR_PROT, _mode, Ofs_GS, P0);
				    Gen(O_POP3, _mode);
				}
				PC+=2;
				break;
			/* case 0xaa: */
			/* case 0xae:	Code Extension 24(MMX) */
			case 0xaf: /* IMULregrm */
				PC++; PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_IMUL, _mode|MEMADR, REG1);	// reg*[edi]->reg signed
				break;
			case 0xb0:		/* CMPXCHGb */
				PC++; PC += ModRM(opc, PC, _mode|MBYTE|MLOAD);
				Gen(O_CMPXCHG, _mode | MBYTE, REG1);
				if (REG3)
				    Gen(S_REG, _mode | MBYTE, REG3);
				else
				    Gen(S_DI, _mode | MBYTE);
				break;
			case 0xb1:		/* CMPXCHGw */
				PC++; PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_CMPXCHG, _mode, REG1);
				if (REG3)
				    Gen(S_REG, _mode, REG3);
				else
				    Gen(S_DI, _mode);
				break;
///
			case 0xb2: /* LSS */
				if (Fetch(PC+2) >= 0xc0) {
				    PC += 3; goto illegal_op;
				}
				PC++; PC += ModRM(opc, PC, _mode);
				Gen(L_LXS2, _mode);
				if (REALADDR()) {
				    AddrGen(A_SR_SH4, _mode, Ofs_SS, Ofs_XSS);
				}
				else {
				    AddrGen(A_SR_PROT, _mode, Ofs_SS, P0);
				}
				Gen(L_LXS1, _mode, REG1);
				break;
			case 0xb4: /* LFS */
				if (Fetch(PC+2) >= 0xc0) {
				    PC += 3; goto illegal_op;
				}
				PC++; PC += ModRM(opc, PC, _mode);
				Gen(L_LXS2, _mode);
				if (REALADDR()) {
				    AddrGen(A_SR_SH4, _mode, Ofs_FS, Ofs_XFS);
				}
				else {
				    AddrGen(A_SR_PROT, _mode, Ofs_FS, P0);
				}
				Gen(L_LXS1, _mode, REG1);
				break;
			case 0xb5: /* LGS */
				if (Fetch(PC+2) >= 0xc0) {
				    PC += 3; goto illegal_op;
				}
				PC++; PC += ModRM(opc, PC, _mode);
				Gen(L_LXS2, _mode);
				if (REALADDR()) {
				    AddrGen(A_SR_SH4, _mode, Ofs_GS, Ofs_XGS);
				}
				else {
				    AddrGen(A_SR_PROT, _mode, Ofs_GS, P0);
				}
				Gen(L_LXS1, _mode, REG1);
				break;
			case 0xb6: /* MOVZXb */
				PC++; PC += ModRM(opc, PC, _mode|MBYTX|MLOAD);
				Gen(L_MOVZS, _mode|MBYTX, 0, REG1);
				break;
			case 0xb7: /* MOVZXw */
				PC++; PC += ModRM(opc, PC, _mode|DATA16|MLOAD);
				Gen(L_MOVZS, _mode, 0, REG1);
				break;
			case 0xbe: /* MOVSXb */
				PC++; PC += ModRM(opc, PC, _mode|MBYTX|MLOAD);
				Gen(L_MOVZS, _mode|MBYTX, 1, REG1);
				break;
			case 0xbf: /* MOVSXw */
				PC++; PC += ModRM(opc, PC, _mode|DATA16|MLOAD);
				Gen(L_MOVZS, _mode, 1, REG1);
				break;
///
			/* case 0xb8:      JMP absolute to IA64 code */
			/* case 0xb9: UD1 */
			case 0xc0: /* XADDb */
				PC++; PC += ModRM(opc, PC, _mode|MBYTE|MLOAD);
				Gen(O_XCHG, _mode | MBYTE, REG1);
				Gen(O_ADD_R, _mode | MBYTE, REG1);
				if (REG3)
				    Gen(S_REG, _mode|MBYTE, REG3);
				else
				    Gen(S_DI, _mode|MBYTE);
				break;
			case 0xc1: /* XADDw */
				PC++; PC += ModRM(opc, PC, _mode|MLOAD);
				Gen(O_XCHG, _mode, REG1);
				Gen(O_ADD_R, _mode, REG1);
				if (REG3)
				    Gen(S_REG, _mode, REG3);
				else
				    Gen(S_DI, _mode);
				break;

			/* case 0xc2-0xc6:	MMX */
			case 0xc7: { /*	Code Extension 23 - 01=CMPXCHG8B mem */
				unsigned char modrm;
				modrm = Fetch(PC+2);
				if (D_MO(modrm) != 1 || D_HO(modrm) == 3) {
					PC += 3; goto illegal_op;
				}
				PC++; PC += ModRM(opc, PC, _mode);
				Gen(O_SIM, _mode, 0x1c7, 1, P0);
				break;
				}

			case 0xc8: /* BSWAPeax */
			case 0xc9: /* BSWAPecx */
			case 0xca: /* BSWAPedx */
			case 0xcb: /* BSWAPebx */
			case 0xcc: /* BSWAPesp */
			case 0xcd: /* BSWAPebp */
			case 0xce: /* BSWAPesi */
			case 0xcf: /* BSWAPedi */
				if (!(_mode&DATA16)) {
					Gen(O_BSWAP, 0, R1Tab_l[D_LO(opc2)]);
				} /* else undefined */
				PC+=2; break;
			/* case 0xd1 ... 0xd3:     MMX
			case 0xd5: case 0xd7: case 0xda: case 0xde:
			case 0xdf:
			case 0xe0 ... 0xe5:
			case 0xf1 ... 0xf3:
			case 0xf5 ... 0xf7:
			case 0xfc ... 0xfe: */
///
			/* case 0xff: UD0, used by Windows */
			/* XXX there is a small difference between
			   UD0, UD1 and UD2: UD0 & UD1 should decode
			   modrm and cause #PF or #GP if they straddle the
			   page or segment limit, instead of #UD */
			default: /* MMX etc */
			    PC += 2; goto illegal_op;
			} }
			break;

/*xx*/	default:
			PC++; goto illegal_op;
		}

		/* check segment boundaries. TODO for prot _mode */
		if (REALADDR() && (PC - Interp_LONG_CS > 0xffff)) {
			e_printf("PC out of bounds, %x\n", PC - Interp_LONG_CS);
			goto not_permitted;
		}
	return PC;

not_permitted:
	if (debug_level('e')>1) e_printf("!!! Not permitted %02x\n",opc);
	return ExceptionGen(PC, _mode, EXCP0D_GPF, 0, P0);
illegal_op:
	dbug_printf("!!! Illegal op %02x %02x %02x\n",Fetch(P0),
		    Fetch(P0+1),Fetch(P0+2));
	return ExceptionGen(PC, _mode, EXCP06_ILLOP, 0, P0);
}

/* reset for VGA reads and writes */
void instr_emu_sim_reset_count(void)
{
	if (!(CEmuStat & CeS_INSTREMU))
		return;
	interp_inst_emu_count = VGA_EMU_INST_EMU_COUNT;
}

static void _PreJit86(unsigned int PC, unsigned int Interp_LONG_CS,
		      unsigned short ocs, int basemode, int flags)
{
	TNode * G = _Interp86(PC, Interp_LONG_CS, ocs, basemode, flags);
	if (G && (flags & F_SPRJ)) {
		NodesPrejitted++;
		sprj_deep(G, PC, Interp_LONG_CS, ocs, basemode, flags);
	}
}

void PreJit86(unsigned int PC, int basemode)
{
	if (e_querymark(PC, 1))
		return;
	_PreJit86(PC, LONG_CS, TheCPU.cs, basemode, F_PREJ);
	e_mdrop();
}

static void *prejit_thread(void *arg)
{
  while (1) {
    sem_wait(&prejit_sem);
    TNode *G = __atomic_load_n(&prejit_G, __ATOMIC_RELAXED);
    unsigned short ocs = __atomic_load_n(&prejit_cs, __ATOMIC_RELAXED);
    _PreJit86(G->key + G->seqlen, G->cs, ocs, G->mode, F_PREJ|F_SPRJ);
    pthread_mutex_lock(&run_mtx);
    prejit_running = 0;
    pthread_mutex_unlock(&run_mtx);
    pthread_cond_signal(&run_cnd);
  }
  return NULL;
}

#if SPEC_PREJIT
static void prejit_run(TNode *G)
{
  unsigned int PC = G->key + G->seqlen;
  if (debug_level('e')) {
    char *ds;
    unsigned short ocs = TheCPU.cs;
    unsigned int basemode = G->mode;
    ds = e_emu_disasm(EMU_BASE32(PC),basemode&MBIGCS,ocs);
    e_printf("prejit at  %s\n", ds);
  }
  if (e_querymark(PC, SAFE_PRJ_GAP))
    return;
#if PROFILE
  SpecPrejits++;
#endif
  __atomic_store_n(&prejit_G, G, __ATOMIC_RELAXED);
  __atomic_store_n(&prejit_cs, TheCPU.cs, __ATOMIC_RELAXED);
  pthread_mutex_lock(&run_mtx);
  assert(!prejit_running);
  prejit_running = 1;
  pthread_mutex_unlock(&run_mtx);
  sem_post(&prejit_sem);
}
#endif

void prejit_sync(void)
{
  pthread_mutex_lock(&run_mtx);
  while (prejit_running)
    cond_wait(&run_cnd, &run_mtx);
  pthread_mutex_unlock(&run_mtx);
}

void prejit_init(void)
{
  sem_init(&prejit_sem, 0, 0);
  pthread_create(&prejit_thr, NULL, prejit_thread, NULL);
}

void prejit_done(void)
{
  pthread_cancel(prejit_thr);
  pthread_join(prejit_thr, NULL);
  sem_destroy(&prejit_sem);
}
