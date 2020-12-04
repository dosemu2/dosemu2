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

#include <stddef.h>
#include <string.h>
#include "emu86.h"
#include "codegen-arch.h"
#include "port.h"
#include "dpmi.h"
#include "mhpdbg.h"
#include "video.h"

#ifdef PROFILE
int EmuSignals = 0;
#endif

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

static __inline__ unsigned long GetCPU_WL(int m, char o)
{
	if (m&DATA16) return CPUWORD(o); else return CPULONG(o);
}

static __inline__ void SetCPU_WL(int m, char o, unsigned long v)
{
	if (m&DATA16) CPUWORD(o)=v; else CPULONG(o)=v;
}

/*
 * close any pending instruction in the code cache and execute the
 * current sequence.
 * P0 is the start of current instruction, where the sequence stops
 *	if there are no jumps at the end.
 * P2 is the address of the next instruction to execute. If different
 *	from P0, abort the current instruction and resume the parsing
 *	loop at P2.
 */
#define	CODE_FLUSH()	{ if (CONFIG_CPUSIM || CurrIMeta>0) {\
			  unsigned int P2 = CloseAndExec(P0, mode, __LINE__);\
			  if (TheCPU.err) return P2;\
			  if (!CONFIG_CPUSIM && P2 != P0) { PC=P2; continue; }\
			} NewNode=0; }

#define UNPREFIX(m)	((m)&~(DATA16|ADDR16))|(basemode&(DATA16|ADDR16))

/////////////////////////////////////////////////////////////////////////////


static int _MAKESEG(int mode, int *basemode, int ofs, unsigned short sv)
{
	SDTR tseg, *segc;
	int e;
	unsigned char big;

//	if ((ofs<0)||(ofs>=0x60)) return EXCP06_ILLOP;

	if (REALADDR()) {
		return SetSegReal(sv,ofs);
	}

	segc = (SDTR *)CPUOFFS(e_ofsseg[(ofs>>2)]);
//	if (segc==NULL) return EXCP06_ILLOP;

	memcpy(&tseg,segc,sizeof(SDTR));
	e = SetSegProt(mode&ADDR16, ofs, &big, sv);
	/* must NOT change segreg and LONG_xx if error! */
	if (e) {
		memcpy(segc,&tseg,sizeof(SDTR));
		return e;
	}
	CPUWORD(ofs) = sv;
	if (ofs==Ofs_CS) {
		if (big) *basemode &= ~(ADDR16|DATA16);
		else *basemode |= (ADDR16|DATA16);
		if (debug_level('e')>1) e_printf("MAKESEG CS: big=%d basemode=%04x\n",big&1,*basemode);
	}
	if (ofs==Ofs_SS) {
		TheCPU.StackMask = (big? 0xffffffff : 0x0000ffff);
		if (debug_level('e')>1) e_printf("MAKESEG SS: big=%d basemode=%04x\n",big&1,*basemode);
	}
	return 0;
}

#define MAKESEG(mode, ofs, sv) _MAKESEG(mode, &basemode, ofs, sv)

/////////////////////////////////////////////////////////////////////////////
//
// jmp		b8 j j j j 5a c3
//	link	e9 l l l l -- --
// jcc  	7x 06 b8 a a a a 5a c3 b8 b b b b 5a c3
//	link	7x 06 e9 l l l l -- -- e9 l l l l -- --
//

static unsigned int _JumpGen(unsigned int P2, int mode, int opc,
			      int pskip, unsigned int *r_P0)
{
#if !defined(SINGLESTEP)
	unsigned int P1;
#endif
	int dsp;
	unsigned int d_t, d_nt, j_t, j_nt;

	/* pskip points to start of next instruction
	 * dsp is the displacement relative to this jump instruction,
	 * some cases:
	 *	eb 00	dsp=2	jmp to next inst (dsp==pskip)
	 *	eb ff	dsp=1	illegal or tricky
	 *	eb fe	dsp=0	loop forever
	 */
	if ((opc>>8) == GRP2wrm || opc == INT) {	// indirect jump
		dsp = 0;
	}
	else if (opc == JMPld || opc == CALLl) { // far jmp/call
		d_t = DataFetchWL_U(mode, P2+1);
		j_t = SEGOFF2LINEAR(FetchW(P2 + pskip - 2), d_t);
		dsp = j_t - P2;
	}
	else {
		dsp = pskip;
		if (pskip == 2)	// short branch (byte)
			dsp += (signed char)Fetch(P2+1);
		else		// long branch (word/long)
			dsp += (int)DataFetchWL_S(mode,
				P2+pskip-BT24(BitDATA16,mode));

		/* displacement for taken branch */
		d_t  = P2 - LONG_CS + dsp;
		if (mode&DATA16) d_t &= 0xffff;

		/* jump address for taken branch */
		j_t  = d_t  + LONG_CS;
	}

	/* displacement for not taken branch */
	d_nt = P2 - LONG_CS + pskip;
	if (mode&DATA16) d_nt &= 0xffff;

	/* jump address for not taken branch, usually next instruction */
	j_nt = d_nt + LONG_CS;
	*r_P0 = j_nt;

#if !defined(SINGLESTEP)
	P1 = P2 + pskip;
#endif
	switch(opc) {
	case JO ... JNLE_JG:
	case JCXZ:
		if (dsp < 0) mode |= CKSIGN;
		/* is there a jump after the condition? if yes, simplify */
#if !defined(SINGLESTEP)
		if (!(EFLAGS & TF)) {
		  if (Fetch(P1)==JMPsid) {	/* eb xx */
		    int dsp2 = (signed char)Fetch(P1+1) + 2;
	    	    if (dsp2 < 0) mode |= CKSIGN;
		    d_nt = P1 - LONG_CS + dsp2;
		    if (mode&DATA16) d_nt &= 0xffff;
		    j_nt = d_nt + LONG_CS;
		    if (debug_level('e')>1)
			e_printf("JMPs (%02x,%d) at %08x after Jcc: t=%08x nt=%08x\n",
				 Fetch(P1),dsp2,P1,j_t,j_nt);
		  }
		  else if (Fetch(P1)==JMPd) {	/* e9 xxxx{xxxx} */
		    int skp2 = BT24(BitDATA16,mode) + 1;
		    int dsp2 = skp2 + (int)DataFetchWL_S(mode, P1+1);
	    	    if (dsp2 < 0) mode |= CKSIGN;
		    d_nt = P1 - LONG_CS + dsp2;
		    if (mode&DATA16) d_nt &= 0xffff;
		    j_nt = d_nt + LONG_CS;
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
		    if (CONFIG_CPUSIM)
			Gen(JB_LINK, mode, opc, P2, j_t, j_nt);
		    else
			Gen(JB_LINK, mode, opc, P2, j_t, j_nt, &InstrMeta[0].clink);
		}
		else {
		    if (dsp == pskip) {
			e_printf("### jmp %x 00\n",opc);
#if !defined(SINGLESTEP)
			if (CONFIG_CPUSIM && !(EFLAGS & TF)) {
			    TheCPU.mode |= SKIPOP;
			    TheCPU.eip = d_nt;
			    return j_nt;
			}
#endif
		    }
		    /* forward jump or backward jump >=256 bytes */
		    if (CONFIG_CPUSIM)
			Gen(JF_LINK, mode, opc, P2, j_t, j_nt);
		    else
			Gen(JF_LINK, mode, opc, P2, j_t, j_nt, &InstrMeta[0].clink);
		}
		break;
	case JMPld: {   /* uncond jmp far */
		unsigned short jcs = FetchW(P2 + pskip - 2);
		Gen(L_IMM, mode, Ofs_CS, jcs);
		AddrGen(A_SR_SH4, mode, Ofs_CS, Ofs_XCS);
	}
	/* no break */
	case JMPsid: case JMPd:   /* uncond jmp */
		if (dsp==0) {	// eb fe
		    dbug_printf("!Forever loop!\n");
		    leavedos_main(0xebfe);
		}
#if !defined(SINGLESTEP)
		if (CONFIG_CPUSIM && !(EFLAGS & TF) && opc != JMPld) {
		    if (debug_level('e')>1) dbug_printf("** JMP: ignored\n");
		    TheCPU.mode |= SKIPOP;
		    TheCPU.eip = d_t;
		    return j_t;
		}
#endif
		if (dsp < 0) mode |= CKSIGN;
		if (CONFIG_CPUSIM)
		    Gen(JMP_LINK, mode, opc, j_t, d_nt);
		else
		    Gen(JMP_LINK, mode, opc, j_t, d_nt, &InstrMeta[0].clink);
		break;
	case CALLl: {   /* call far */
		unsigned short jcs = FetchW(P2 + pskip - 2);
		Gen(L_REG, mode, Ofs_CS);
		Gen(O_PUSH, mode);
		Gen(L_IMM, mode, Ofs_CS, jcs);
		AddrGen(A_SR_SH4, mode, Ofs_CS, Ofs_XCS);
	}
	/* no break */
	case CALLd:    /* call, unfortunately also uses JMP_LINK */
		if (CONFIG_CPUSIM)
		    Gen(JMP_LINK, mode, opc, j_t, d_nt);
		else
		    Gen(JMP_LINK, mode, opc, j_t, d_nt, &InstrMeta[0].clink);
		break;
	case LOOP: case LOOPZ_LOOPE: case LOOPNZ_LOOPNE:
		if (dsp == 0) {
#if !defined(SINGLESTEP)
		    if (CONFIG_CPUSIM && !(EFLAGS & TF)) {
		    	// ndiags: shorten delay loops (e2 fe)
			e_printf("### loop %x 0xfe\n",opc);
			Gen(L_IMM, ((mode&ADDR16) ? (mode|DATA16) : mode),
			    Ofs_ECX, 0);
			TheCPU.eip = d_nt;
			return j_nt;
		    }
#endif
		}
		if (CONFIG_CPUSIM)
		    Gen(JLOOP_LINK, mode, opc, j_t, j_nt);
		else
		    Gen(JLOOP_LINK, mode, opc, j_t, j_nt, &InstrMeta[0].clink);
		break;
	case RETl: case RETlisp: case JMPli: case CALLli: // far ret, indirect
	case INT:
		Gen(S_REG, mode, Ofs_CS);
		AddrGen(A_SR_SH4, mode, Ofs_CS, Ofs_XCS);
		Gen(L_REG, mode, Ofs_EIP);
		/* fall through */
	case RET: case RETisp: case JMPi: case CALLi: // ret, indirect
		if (CONFIG_CPUSIM)
			Gen(JMP_INDIRECT, mode);
		else
			Gen(JMP_INDIRECT, mode, &InstrMeta[0].clink);
		break;
	default: dbug_printf("JumpGen: unknown condition\n");
		break;
	}

	return (unsigned)-1;
}

#define JumpGen(P2, mode, opc, pskip) ({ \
	unsigned int _P0, _P1; \
	int _rc; \
	_P1 = _JumpGen(P2, mode, opc, pskip, &_P0); \
	if (_P1 == (unsigned)-1) { \
		if (!CONFIG_CPUSIM) \
			NewIMeta(P0, &_rc); \
		_P1 = CloseAndExec(_P0, mode, __LINE__); \
		NewNode=0; \
	} \
	_P1; \
})


/////////////////////////////////////////////////////////////////////////////

#if !defined(SINGLESTEP)&&defined(HOST_ARCH_X86)
static unsigned int FindExecCode(unsigned int PC)
{
	int mode = TheCPU.mode;
	TNode *G;

	if (CurrIMeta > 0) {		// open code?
		if (debug_level('e') > 2)
			e_printf("============ Closing open sequence at %08x\n",PC);
		PC = CloseAndExec(PC, mode, __LINE__);
		if (TheCPU.err) return PC;
	}
	/* for a sequence to be found, it must begin with
	 * an allowable opcode. Look into table.
	 * NOTE - this while can loop forever and stop
	 * any signal processing. Jumps are defined as
	 * a 'descheduling point' for checking signals.
	 */
	while (!(CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND)) &&
	       (G=FindTree(PC))) {
		if (G->cs != LONG_CS) {
			/* CS mismatch can confuse relative jump/call */
			e_printf("cs mismatch at %08x: old=%x new=%x\n",
					PC, G->cs, LONG_CS);
			InvalidateNodeRange(G->seqbase, G->seqlen, NULL);
			return PC;
		}
		if (debug_level('e')>2)
			e_printf("** Found compiled code at %08x\n",PC);
		if (debug_level('e') &&
				/* check for codemarks inconsistency */
				e_querymark_all(G->seqbase, G->seqlen) == 0) {
			int i, j;
			error("no mark at %x (%i)\n",
					G->seqbase,
					e_querymark(G->seqbase, G->seqlen));
			j = -1;
			for (i = 0; i < G->seqlen; i++) {
				int mrk = e_querymark(G->seqbase + i, 1);
				error("@%i ", mrk);
				if (!mrk && j == -1)
					j = i;
			}
			error("@\n");
			if (j != -1)
				error("@corrupted at %x\n", G->seqbase + j);
		}
		/* ---- this is the MAIN EXECUTE point ---- */
		NodesExecd++;
#ifdef PROFILE
		TotalNodesExecd++;
#elif !defined(ASM_DUMP)
		/* try fast inner loop if nothing special is going on */
		if (!(CEmuStat & (CeS_INHI|CeS_MOVSS)) &&
		    !debug_level('e') && eTimeCorrect < 0 &&
		    G->cs == LONG_CS && !(G->flags & (F_FPOP|F_INHI)))
			PC = Exec_x86_fast(G);
		else
#endif
			PC = Exec_x86(G, __LINE__);
		if (G->seqlen == 0) {
			error("CPU-EMU: Zero-len code node?\n");
			break;
		}
		if (TheCPU.err) return PC;
	}
	return PC;
}
#endif

static void HandleEmuSignals(void)
{
#ifdef PROFILE
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
		TheCPU.err=EXCP_PICSIGNAL;
	}
	else if (CEmuStat & CeS_STI) {
		/* force exit for IF set */
		CEmuStat &= ~CeS_STI;
		TheCPU.err=EXCP_STISIGNAL;
	}
	/* clear optional exit conditions */
	CEmuStat &= ~CeS_TRAP;
	if (TheCPU.err)
		CEmuStat &= ~(CeS_SIGPEND | CeS_RPIC | CeS_STI);
}

static unsigned int _Interp86(unsigned int PC, int mod0);

unsigned int Interp86(unsigned int PC, int mod0)
{
    unsigned int ret = _Interp86(PC, mod0);
    TheCPU.eip = ret - LONG_CS;
    return ret;
}

static unsigned int _Interp86(unsigned int PC, int basemode)
{
	volatile unsigned int P0 = PC; /* volatile because of setjmp */
	unsigned char opc;
	unsigned short ocs = TheCPU.cs;
	unsigned int temp;
	register int mode;
	int NewNode;

	if (PROTMODE() && setjmp(jmp_env)) {
		/* long jump to here from simulated page fault */
		return P0;
	}

	NewNode = 0;
	TheCPU.err = 0;
	CEmuStat &= ~CeS_TRAP;

	while (Running) {
		OVERR_DS = Ofs_XDS;
		OVERR_SS = Ofs_XSS;
		TheCPU.mode = mode = basemode;

		if (!NewNode) {
			if (CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND|CeS_RPIC|CeS_STI)) {
				HandleEmuSignals();
				if (TheCPU.err) return PC;
			}
			if (EFLAGS & TF)
				CEmuStat |= CeS_TRAP;
		}
#ifdef HOST_ARCH_X86
		if (!CONFIG_CPUSIM && e_querymark(PC, 1)) {
			unsigned int P2 = PC;
			if (NewNode) {
				P0 = PC;
				CODE_FLUSH();
			}
#ifndef SINGLESTEP
			if (!(EFLAGS & TF)) {
				P2 = FindExecCode(PC);
				if (TheCPU.err) return P2;
				if (CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND|CeS_RPIC|CeS_STI)) {
					HandleEmuSignals();
					if (TheCPU.err) return P2;
					if (EFLAGS & TF)
						CEmuStat |= CeS_TRAP;
				}
			}
#endif
			if (P2 == PC || e_querymark(P2, 1)) {
				/* slow path */
				InvalidateNodeRange(P2, 1, NULL);
			}
			PC = P2;
		}
#if 0
		/* this obviously can't happen with current code, but
		 * slows down execution under debug a lot */
		if (debug_level('e') && !CONFIG_CPUSIM && e_querymark(PC, 1))
			error("simx86: code nodes clashed at %x\n", PC);
#endif
#endif
		P0 = PC;	// P0 changes on instruction boundaries
		NewNode = 1;
		if (debug_level('e')==9) dbug_printf("\n%s",e_print_regs());
		if (debug_level('e')>2) {
		    char *ds = e_emu_disasm(MEM_BASE32(P0),(~basemode&3),ocs);
		    ocs = TheCPU.cs;
		    if (debug_level('e')>2) e_printf("  %s\n", ds);
		}

override:
		switch ((opc=Fetch(PC))) {
/*28*/	case SUBbfrm:
/*2a*/	case SUBbtrm:
/*30*/	case XORbfrm:
/*32*/	case XORbtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_CLEAR, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3a; else goto intop28;
/*08*/	case ORbfrm:
/*0a*/	case ORbtrm:
/*20*/	case ANDbfrm:
/*22*/	case ANDbtrm:
/*84*/	case TESTbrm:
		        { int m = mode | MBYTE;
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
			    Gen(O_SBSELF, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3a;
/*00*/	case ADDbfrm:
/*10*/	case ADCbfrm:
/*38*/	case CMPbfrm:
intop28:		{ int m = mode | MBYTE;
			PC += ModRM(opc, PC, m);	// DI=mem
			if (TheCPU.mode & RM_REG) {
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
intop3a:		{ int m = mode | MBYTE;
			int op = ArOpsFR[D_MO(opc)];
			PC += ModRM(opc, PC, m|MLOAD);	// al=[rm]
			Gen(op, m, REG1);		// op [ebx+reg], al
			}
			break;
/*29*/	case SUBwfrm:
/*2b*/	case SUBwtrm:
/*31*/	case XORwfrm:
/*33*/	case XORwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_CLEAR, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3b; else goto intop29;
/*09*/	case ORwfrm:
/*0b*/	case ORwtrm:
/*21*/	case ANDwfrm:
/*23*/	case ANDwtrm:
/*85*/	case TESTwrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_TEST, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc != TESTwrm) {
			    if (opc & 2) goto intop3b; else goto intop29;
			}
			PC += ModRM(opc, PC, mode|MLOAD);	// (e)ax=[rm]
			Gen(O_AND_R, mode, REG1);	// op  (e)ax,[ebx+reg]
			break;
/*19*/	case SBBwfrm:
/*1b*/	case SBBwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
			if (opc & 2) goto intop3b;
/*01*/	case ADDwfrm:
/*11*/	case ADCwfrm:
/*39*/	case CMPwfrm:
intop29:		PC += ModRM(opc, PC, mode);	// DI=mem
			if (TheCPU.mode & RM_REG) {
			    int op = ArOpsFR[D_MO(opc)];
			    Gen(L_REG, mode, REG1);	// mov (e)ax,[ebx+reg]
			    Gen(op, mode, REG3);	// op [ebx+rmreg],(e)ax	rmreg=rmreg op reg
			}
			else {
			    int op = ArOpsR[D_MO(opc)];
			    Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
			    Gen(op, mode, REG1);	// op  (e)ax,[ebx+reg]
			    if (opc!=CMPwfrm) {
				Gen(S_DI, mode);	// mov [edi],(e)ax	mem=mem op reg
			    }
			}
			break;
/*03*/	case ADDwtrm:
/*13*/	case ADCwtrm:
/*3b*/	case CMPwtrm:
intop3b:		{ int op = ArOpsFR[D_MO(opc)];
			PC += ModRM(opc, PC, mode|MLOAD);	// (e)ax=[rm]
			Gen(op, mode, REG1);		// op [ebx+reg], (e)ax
			}
			break;
/*a8*/	case TESTbi: {
			int m = mode | MBYTE;
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
			int m = mode | MBYTE;
			int op = ArOpsFR[D_MO(opc)];
			// op [ebx+Ofs_EAX],#imm
			Gen(op, m|IMMED, Ofs_EAX, Fetch(PC+1)); PC+=2;
			}
			break;
/*a9*/	case TESTwi:
			Gen(L_IMM_R1, mode|IMMED, DataFetchWL_U(mode,PC+1));
			INC_WL_PC(mode,1);
			Gen(O_AND_R, mode, Ofs_EAX);
			break;
/*05*/	case ADDwia:
/*0d*/	case ORwi:
/*15*/	case ADCwi:
/*1d*/	case SBBwi:
/*25*/	case ANDwi:
/*2d*/	case SUBwi:
/*35*/	case XORwi:
/*3d*/	case CMPwi: {
			int m = mode;
			int op = ArOpsFR[D_MO(opc)];
			// op [ebx+Ofs_EAX],#imm
			Gen(op, m|IMMED, Ofs_EAX, DataFetchWL_U(mode,PC+1));
			INC_WL_PC(mode,1);
			}
			break;
/*69*/	case IMULwrm:
			PC += ModRM(opc, PC, mode|MLOAD); // mov (e)ax,[rm]
			Gen(O_IMUL,mode|IMMED,DataFetchWL_S(mode,PC),REG1);
			INC_WL_PC(mode, 0);
			break;
/*6b*/	case IMULbrm:
			PC += ModRM(opc, PC, mode|MLOAD); // mov (e)ax,[rm]
			Gen(O_IMUL,mode|MBYTE|IMMED,(signed char)Fetch(PC),REG1);
			PC++;
			break;

/*9c*/	case PUSHF: {
			if (V86MODE() && (IOPL<3)) {
			    if (CONFIG_CPUSIM) FlagSync_All();
#ifdef HOST_ARCH_X86
			    else CODE_FLUSH();
#endif
			    /* virtual-8086 monitor */
			    if (!(TheCPU.cr[4] & CR4_VME))
				goto not_permitted;	/* GPF */
			    temp = (EFLAGS|IOPL_MASK) & RETURN_MASK;
			    if (EFLAGS & VIF) temp |= EFLAGS_IF;
			    PUSH(mode, temp);
			    if (debug_level('e')>1)
				e_printf("Pushed flags %08x fl=%08x\n",
					temp,EFLAGS);
			}
			else {
				Gen(O_PUSH2F, mode);
			}
			PC++; }
			break;
/*9e*/	case SAHF:
			Gen(O_SLAHF, mode, 1);
			PC++; break;
/*9f*/	case LAHF:
			Gen(O_SLAHF, mode, 0);
			PC++; break;

/*27*/	case DAA:
			Gen(O_OPAX, mode, 1, DAA); PC++; break;
/*2f*/	case DAS:
			Gen(O_OPAX, mode, 1, DAS); PC++; break;
/*37*/	case AAA:
			Gen(O_OPAX, mode, 1, AAA); PC++; break;
/*3f*/	case AAS:
			Gen(O_OPAX, mode, 1, AAS); PC++; break;
/*d4*/	case AAM:
			Gen(O_OPAX, mode, 2, AAM, Fetch(PC+1)); PC+=2; break;
/*d5*/	case AAD:
			Gen(O_OPAX, mode, 2, AAD, Fetch(PC+1)); PC+=2; break;

/*d6*/	case 0xd6:	/* Undocumented */
			CODE_FLUSH();
			e_printf("Undocumented op 0xd6\n");
			rAL = (EFLAGS & EFLAGS_CF? 0xff:0x00);
			PC++; break;
/*62*/	case BOUND:    {
	  		signed int lo, hi, r;
			CODE_FLUSH();
			if (Fetch(PC+1) >= 0xc0) {
			    goto not_permitted;
			}
			PC += ModRMSim(PC, mode);
			r = GetCPU_WL(mode, REG1);
			lo = DataGetWL_S(mode,TheCPU.mem_ref);
			TheCPU.mem_ref += BT24(BitDATA16, mode);
			hi = DataGetWL_S(mode,TheCPU.mem_ref);
			if(r < lo || r > hi)
			{
				e_printf("Bound interrupt 05\n");
				TheCPU.err=EXCP05_BOUND;
				return P0;
			}
			break;
		       }
/*63*/	case ARPL:     {
			unsigned short dest, src;
			CODE_FLUSH();
			PC += ModRMSim(PC, mode);
			if (TheCPU.mode & RM_REG) {
				dest = CPUWORD(REG3);
			} else {
				dest = GetDWord(TheCPU.mem_ref);
			}
			src = GetCPU_WL(mode, REG1);
			if ((dest & 3) < (src & 3)) {
				EFLAGS |= EFLAGS_ZF;
				dest = (dest & ~3) | (src & 3);
				if (TheCPU.mode & RM_REG) {
					CPUWORD(REG3) = dest;
				} else {
					WRITE_WORD(TheCPU.mem_ref, dest);
				}
			} else {
				EFLAGS &= ~EFLAGS_ZF;
			}
			if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
			break;
		       }
/*d7*/	case XLAT:
			Gen(O_XLAT, mode);
			Gen(L_DI_R1, mode|MBYTE);
			Gen(S_REG, mode|MBYTE, Ofs_AL);
			PC++; break;
/*98*/	case CBW:
			Gen(O_CBWD, mode|MBYTE); PC++; break;
/*99*/	case CWD:
			Gen(O_CBWD, mode); PC++; break;

/*07*/	case POPes:	if (REALADDR()) {
			    Gen(O_POP, mode);
			    Gen(S_REG, mode, Ofs_ES);
			    AddrGen(A_SR_SH4, mode, Ofs_ES, Ofs_XES);
			} else { /* restartable */
			    unsigned short sv = 0;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = MAKESEG(mode, Ofs_ES, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.es = sv;
			}
			PC++;
			break;
/*17*/	case POPss:	if (REALADDR()) {
			    Gen(O_POP, mode);
			    Gen(S_REG, mode, Ofs_SS);
			    AddrGen(A_SR_SH4, mode, Ofs_SS, Ofs_XSS);
			} else { /* restartable */
			    unsigned short sv = 0;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = MAKESEG(mode, Ofs_SS, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.ss = sv;
			    CEmuStat |= CeS_MOVSS;
			}
			PC++;
			break;
/*1f*/	case POPds:	if (REALADDR()) {
			    Gen(O_POP, mode);
			    Gen(S_REG, mode, Ofs_DS);
			    AddrGen(A_SR_SH4, mode, Ofs_DS, Ofs_XDS);
			} else { /* restartable */
			    unsigned short sv = 0;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = MAKESEG(mode, Ofs_DS, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.ds = sv;
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
			mode = (mode & ~DATA16) | (~basemode & DATA16);
			if (debug_level('e')>4)
				e_printf("OPERoverride: new mode %04x\n",mode);
			PC++; goto override;
/*67*/	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
			mode = (mode & ~ADDR16) | (~basemode & ADDR16);
			if (debug_level('e')>4)
				e_printf("ADDRoverride: new mode %04x\n",mode);
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
			CODE_FLUSH();
			goto illegal_op;
			}
/*40*/	case INCax:
/*41*/	case INCcx:
/*42*/	case INCdx:
/*43*/	case INCbx:
/*44*/	case INCsp:
/*45*/	case INCbp:
/*46*/	case INCsi:
/*47*/	case INCdi:
			Gen(O_INC_R, mode, R1Tab_l[D_LO(opc)]); PC++; break;
/*48*/	case DECax:
/*49*/	case DECcx:
/*4a*/	case DECdx:
/*4b*/	case DECbx:
/*4c*/	case DECsp:
/*4d*/	case DECbp:
/*4e*/	case DECsi:
/*4f*/	case DECdi:
			Gen(O_DEC_R, mode, R1Tab_l[D_LO(opc)]); PC++; break;

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
			if (!(EFLAGS & TF)) {
			int m = mode;		// enter with prefix
			int cnt = 2;
			int is_66;
			Gen(O_PUSH1, m);
			/* optimized multiple register push */
			while (1) {
			    Gen(O_PUSH2, m, R1Tab_l[opc-1]);
			    PC++;
			    opc = OpIsPush[Fetch(PC)];
			    is_66 = (Fetch(PC) == 0x66);
			    if (++cnt >= NUMGENS || (!opc && !is_66) ||
				    e_querymark(PC, 1 + is_66))
				break;
			    m &= ~DATA16;
			    if (is_66) {	// prefix 0x66
				m |= (~basemode & DATA16);
				if ((opc=OpIsPush[Fetch(PC+1)])!=0) PC++;
				else break;
			    }
			    else {
				m |= (basemode & DATA16);
			    }
			}
			Gen(O_PUSH3, m); } else
#endif
			{
			Gen(L_REG, mode, R1Tab_l[opc-1]);
			Gen(O_PUSH, mode); PC++;
			}
			break;
/*68*/	case PUSHwi:
			Gen(O_PUSHI, mode, DataFetchWL_U(mode,(PC+1)));
			INC_WL_PC(mode,1);
			break;
/*6a*/	case PUSHbi:
			Gen(O_PUSHI, mode, (signed char)Fetch(PC+1)); PC+=2; break;
/*60*/	case PUSHA:
			/* push order: eax ecx edx ebx esp ebp esi edi */
			Gen(O_PUSH1, mode);
			Gen(O_PUSH2, mode, Ofs_EAX);
			Gen(O_PUSH2, mode, Ofs_ECX);
			Gen(O_PUSH2, mode, Ofs_EDX);
			Gen(O_PUSH2, mode, Ofs_EBX);
			Gen(O_PUSH2, mode, Ofs_ESP);
			Gen(O_PUSH2, mode, Ofs_EBP);
			Gen(O_PUSH2, mode, Ofs_ESI);
			Gen(O_PUSH2, mode, Ofs_EDI);
			Gen(O_PUSH3, mode); PC++; break;
/*61*/	case POPA:
			Gen(O_POP1, mode);
			Gen(O_POP2, mode, Ofs_EDI);
			Gen(O_POP2, mode, Ofs_ESI);
			Gen(O_POP2, mode, Ofs_EBP);
			Gen(O_POP2, mode, Ofs_ESP); // overwritten in O_POP3
			Gen(O_POP2, mode, Ofs_EBX);
			Gen(O_POP2, mode, Ofs_EDX);
			Gen(O_POP2, mode, Ofs_ECX);
			Gen(O_POP2, mode, Ofs_EAX);
			Gen(O_POP3, mode); PC++; break;

/*58*/	case POPax:
/*59*/	case POPcx:
/*5a*/	case POPdx:
/*5b*/	case POPbx:
/*5c*/	case POPsp:
/*5d*/	case POPbp:
/*5e*/	case POPsi:
/*5f*/	case POPdi:
#ifndef SINGLESTEP
			if (!(EFLAGS & TF)) {
			int m = mode;
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
			Gen(O_POP, mode);
			Gen(S_REG, mode, R1Tab_l[D_LO(opc)]); PC++;
			}
			break;
/*8f*/	case POPrm:
			// now calculate address. This way when using %esp
			//	as index we use the value AFTER the pop
			PC += ModRM(opc, PC, mode|MPOPRM);
			if (TheCPU.mode & RM_REG) {
				// pop data into temporary storage and adjust esp
				Gen(O_POP, mode);
				// store data
				Gen(S_REG, mode, REG3);
			} else {
				// read data into temporary storage
				Gen(O_POP1, mode);
				Gen(O_POP2, mode|MPOPRM, 0);
				// store data
				// S_DI may fault, in which case the instruction
				// may need to be restarted with the original
				// value of ESP!
				Gen(S_DI, mode);	// mov [edi],{e}ax
				Gen(O_POP3, mode|MPOPRM);
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
/*e3*/	case JCXZ:	{
			  PC = JumpGen(PC, mode, opc, 2);
			  if (TheCPU.err) return PC;
			}
			break;

/*82*/	case IMMEDbrm2:		// add mem8,signed imm8: no AND,OR,XOR
/*80*/	case IMMEDbrm: {
			int m = mode | MBYTE;
			int op = D_MO(Fetch(PC+1));
			PC += ModRM(opc, PC, m);
			if (TheCPU.mode & RM_REG) {
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
			PC += ModRM(opc, PC, mode);
			if (TheCPU.mode & RM_REG) {
				op = ArOpsFR[op];
				// op [ebx+reg],#imm
				Gen(op, mode|IMMED, REG3, DataFetchWL_U(mode,PC));
			}
			else {
				op = ArOpsR[op];
				Gen(L_DI_R1, mode);	// mov ax,[edi]
				Gen(op, mode|IMMED, DataFetchWL_U(mode,PC)); // op ax,#imm
				if (op!=O_CMP_R)
					Gen(S_DI, mode);// mov [edi],ax	mem=mem op #imm
			}
			INC_WL_PC(mode,0);
			}
			break;
/*83*/	case IMMEDisrm: {
			int op = D_MO(Fetch(PC+1));
			long v;
			PC += ModRM(opc, PC, mode);
			v = (signed char)Fetch(PC);
			if (TheCPU.mode & RM_REG) {
				op = ArOpsFR[op];
				// op [ebx+reg],#imm
				Gen(op, mode|IMMED, REG3, v);
			}
			else {
				op = ArOpsR[op];
				Gen(L_DI_R1, mode);	// mov ax,[edi]
				Gen(op, mode|IMMED, v);		// op ax,#imm
				if (op != O_CMP_R)
					Gen(S_DI, mode);// mov [edi],ax	mem=mem op #imm
			}
			PC++; }
			break;
/*86*/	case XCHGbrm:
			if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(L_NOP, mode); PC+=2;
			}
			else {
			    PC += ModRM(opc, PC, mode|MBYTE|MLOAD);// al=[rm]
			    if (TheCPU.mode & RM_REG) {
				Gen(O_XCHG, mode|MBYTE, REG1);
				Gen(S_REG, mode|MBYTE, REG3);
			    }
			    else {
				Gen(O_XCHG, mode|MBYTE, REG1);
				Gen(S_DI, mode|MBYTE);
			    }
			}
			break;
/*87*/	case XCHGwrm:
			if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(L_NOP, mode); PC+=2;
			}
			else {
			    PC += ModRM(opc, PC, mode|MLOAD);	// (e)ax=[rm]
			    if (TheCPU.mode & RM_REG) {
				Gen(O_XCHG, mode, REG1);
				Gen(S_REG, mode, REG3);
			    }
			    else {
				Gen(O_XCHG, mode, REG1);
				Gen(S_DI, mode);
			    }
			}
			break;
/*88*/	case MOVbfrm:
			if (ModGetReg1(PC, MBYTE)==3) {
			    Gen(L_REG2REG, MBYTE, REG1, REG3); PC+=2;
			} else {
			    Gen(L_REG, mode|MBYTE, REG1);
			    PC += ModRM(opc, PC, mode|MBYTE|MSTORE); // [rm]=al
			}
			break;
/*89*/	case MOVwfrm:
			if (ModGetReg1(PC, mode)==3) {
			    Gen(L_REG2REG, mode, REG1, REG3); PC+=2;
			} else {
			    Gen(L_REG, mode, REG1);
			    PC += ModRM(opc, PC, mode|MSTORE); // [rm]=(e)ax
			}
			break;
/*8a*/	case MOVbtrm:
			if (ModGetReg1(PC, MBYTE)==3) {
			    Gen(L_REG2REG, MBYTE, REG3, REG1); PC+=2;
			} else {
			    PC += ModRM(opc, PC, mode|MBYTE|MLOAD); // al=[rm]
			    Gen(S_REG, mode|MBYTE, REG1);
			}
			break;
/*8b*/	case MOVwtrm:
			if (ModGetReg1(PC, mode)==3) {
			    Gen(L_REG2REG, mode, REG3, REG1); PC+=2;
			} else {
			    PC += ModRM(opc, PC, mode|MLOAD); // (e)ax=[rm]
			    Gen(S_REG, mode, REG1);
			}
			break;
/*8c*/	case MOVsrtrm:
			PC += ModRM(opc, PC, mode|SEGREG);
			Gen(L_REG, mode|DATA16, REG1);
			//Gen(L_ZXAX, mode);
			if (TheCPU.mode & RM_REG)
			    Gen(S_REG, mode|DATA16, REG3);
			else
			    Gen(S_DI, mode|DATA16);
			break;
/*8d*/	case LEA:
			if (Fetch(PC+1) >= 0xc0) {
			    CODE_FLUSH();
			    goto not_permitted;
			}
			PC += ModRM(opc, PC, mode|MLEA);
			Gen(S_DI_R, mode, REG1);
			break;

/*c4*/	case LES:
			if (Fetch(PC+1) >= 0xc0) {
			    CODE_FLUSH();
			    goto not_permitted;
			}
			if (REALADDR()) {
			    PC += ModRM(opc, PC, mode);
			    Gen(L_LXS1, mode, REG1);
			    Gen(L_LXS2, mode, Ofs_ES, Ofs_XES);
			}
			else {
			    unsigned short sv = 0;
			    unsigned long rv;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode);
			    rv = DataGetWL_U(mode,TheCPU.mem_ref);
			    TheCPU.mem_ref += BT24(BitDATA16, mode);
			    sv = GetDWord(TheCPU.mem_ref);
			    TheCPU.err = MAKESEG(mode, Ofs_ES, sv);
			    if (TheCPU.err) return P0;
			    SetCPU_WL(mode, REG1, rv);
			    TheCPU.es = sv;
			}
			break;
/*c5*/	case LDS:
			if (Fetch(PC+1) >= 0xc0) {
			    CODE_FLUSH();
			    goto not_permitted;
			}
			if (REALADDR()) {
			    PC += ModRM(opc, PC, mode);
			    Gen(L_LXS1, mode, REG1);
			    Gen(L_LXS2, mode, Ofs_DS, Ofs_XDS);
			}
			else {
			    unsigned short sv = 0;
			    unsigned long rv;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode);
			    rv = DataGetWL_U(mode,TheCPU.mem_ref);
			    TheCPU.mem_ref += BT24(BitDATA16, mode);
			    sv = GetDWord(TheCPU.mem_ref);
			    TheCPU.err = MAKESEG(mode, Ofs_DS, sv);
			    if (TheCPU.err) return P0;
			    SetCPU_WL(mode, REG1, rv);
			    TheCPU.ds = sv;
			}
			break;
/*8e*/	case MOVsrfrm:
			if (REALADDR()) {
			    PC += ModRM(opc, PC, mode|SEGREG|DATA16|MLOAD);
			    Gen(S_REG, mode|DATA16, REG1);
			    AddrGen(A_SR_SH4, mode, REG1, e_ofsseg[REG1>>2]);
			}
			else {
			    unsigned short sv = 0;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode|SEGREG);
			    if (TheCPU.mode & RM_REG) {
				sv = CPUWORD(REG3);
			    } else {
				sv = GetDWord(TheCPU.mem_ref);
			    }
			    TheCPU.err = MAKESEG(mode, REG1, sv);
			    if (TheCPU.err) return P0;
			    switch (REG1) {
				case Ofs_DS: TheCPU.ds=sv; break;
				case Ofs_SS: TheCPU.ss=sv;
				    CEmuStat |= CeS_MOVSS;
				    break;
				case Ofs_ES: TheCPU.es=sv; break;
				case Ofs_FS: TheCPU.fs=sv; break;
				case Ofs_GS: TheCPU.gs=sv; break;
				default: goto illegal_op;
			    }
			}
			break;

/*9b*/	case oWAIT:
/*90*/	case NOP:	//if (!IsCodeInBuf()) Gen(L_NOP, mode);
			if (CurrIMeta<0) Gen(L_NOP, mode); else TheCPU.mode|=SKIPOP;
			PC++;
			if (!(EFLAGS & TF))
			    while (Fetch(PC)==NOP) PC++;
			break;
/*91*/	case XCHGcx:
/*92*/	case XCHGdx:
/*93*/	case XCHGbx:
/*94*/	case XCHGsp:
/*95*/	case XCHGbp:
/*96*/	case XCHGsi:
/*97*/	case XCHGdi:
			Gen(O_XCHG_R, mode, Ofs_EAX, R1Tab_l[D_LO(opc)]);
			PC++; break;

/*a0*/	case MOVmal:
			AddrGen(A_DI_0, mode|IMMED, OVERR_DS, AddrFetchWL_U(mode,PC+1));
			Gen(L_DI_R1, mode|MBYTE);
			Gen(S_REG, mode|MBYTE, Ofs_AL);
			INC_WL_PCA(mode,1);
			break;
/*a1*/	case MOVmax:
			AddrGen(A_DI_0, mode|IMMED, OVERR_DS, AddrFetchWL_U(mode,PC+1));
			Gen(L_DI_R1, mode);
			Gen(S_REG, mode, Ofs_EAX);
			INC_WL_PCA(mode,1);
			break;
/*a2*/	case MOValm:
			AddrGen(A_DI_0, mode|IMMED, OVERR_DS, AddrFetchWL_U(mode,PC+1));
			Gen(L_REG, mode|MBYTE, Ofs_AL);
			Gen(S_DI, mode|MBYTE);
			INC_WL_PCA(mode,1);
			break;
/*a3*/	case MOVaxm:
			AddrGen(A_DI_0, mode|IMMED, OVERR_DS, AddrFetchWL_U(mode,PC+1));
			Gen(L_REG, mode, Ofs_EAX);
			Gen(S_DI, mode);
			INC_WL_PCA(mode,1);
			break;

/*a4*/	case MOVSb: {	int m = mode|(MBYTE|MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC);
			Gen(S_DI, m);
			Gen(O_MOVS_SavA, m);
			PC++;
			} break;
/*a5*/	case MOVSw: {	int m = mode|(MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC);
			Gen(S_DI, m);
			PC++;
			Gen(O_MOVS_SavA, m);
#ifndef SINGLESTEP
			/* optimize common sequence MOVSw..MOVSw..MOVSb */
			if (!(EFLAGS & TF)) {
				int cnt = 3;
				m = UNPREFIX(m);
				while (++cnt < NUMGENS && Fetch(PC) == MOVSw &&
						!e_querymark(PC, 1)) {
					Gen(O_MOVS_SetA, m&~MOVSDST);
					Gen(L_DI_R1, m);
					Gen(O_MOVS_SetA, m&~MOVSSRC);
					Gen(S_DI, m);
					PC++;
					Gen(O_MOVS_SavA, m);
				}
				if (Fetch(PC) == MOVSb && !e_querymark(PC, 1)) {
					m |= MBYTE;
					Gen(O_MOVS_SetA, m&~MOVSDST);
					Gen(L_DI_R1, m);
					Gen(O_MOVS_SetA, m&~MOVSSRC);
					Gen(S_DI, m);
					PC++;
					Gen(O_MOVS_SavA, m);
				}
			}
#endif
			} break;
/*a6*/	case CMPSb: {	int m = mode|(MBYTE|MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC);
			Gen(O_MOVS_CmpD, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*a7*/	case CMPSw: {	int m = mode|(MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m&~MOVSDST);
			Gen(L_DI_R1, m);
			Gen(O_MOVS_SetA, m&~MOVSSRC);
			Gen(O_MOVS_CmpD, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*aa*/	case STOSb: {	int m = mode|(MBYTE|MOVSDST);
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, m, Ofs_AL);
			Gen(S_DI, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*ab*/	case STOSw: {	int m = mode|MOVSDST;
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, m, Ofs_EAX);
			Gen(S_DI, m); PC++;
			Gen(O_MOVS_SavA, m);
#ifndef SINGLESTEP
			if (!(EFLAGS & TF)) {
			    int cnt = 3;
			    m = UNPREFIX(m);
			    while (++cnt < NUMGENS && Fetch(PC) == STOSw &&
					!e_querymark(PC, 1)) {
				Gen(O_MOVS_SetA, m);
				Gen(S_DI, m);
				Gen(O_MOVS_SavA, m);
				PC++;
			    }
			}
#endif
			} break;
/*ac*/	case LODSb: {	int m = mode|(MBYTE|MOVSSRC);
			Gen(O_MOVS_SetA, m);
			Gen(L_DI_R1, m);
			Gen(S_REG, m, Ofs_AL); PC++;
#ifndef SINGLESTEP
			/* optimize common sequence LODSb-STOSb */
			if (!(EFLAGS & TF) && Fetch(PC) == STOSb &&
					!e_querymark(PC, 1)) {
				Gen(O_MOVS_SetA, (m&ADDR16)|MOVSDST);
				Gen(S_DI, m);
				m |= MOVSDST;
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, m);
			} break;
/*ad*/	case LODSw: {	int m = mode|MOVSSRC;
			Gen(O_MOVS_SetA, m);
			Gen(L_DI_R1, m);
			Gen(S_REG, m, Ofs_EAX); PC++;
#ifndef SINGLESTEP
			/* optimize common sequence LODSw-STOSw */
			if (!(EFLAGS & TF) && Fetch(PC) == STOSw &&
					!e_querymark(PC, 1)) {
				Gen(O_MOVS_SetA, (m&ADDR16)|MOVSDST);
				Gen(S_DI, m);
				m |= MOVSDST;
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, m);
			} break;
/*ae*/	case SCASb: {	int m = mode|(MBYTE|MOVSDST);
			Gen(O_MOVS_SetA, m);
			Gen(L_DI_R1, m);		// mov al,[edi]
			Gen(O_CMP_FR, m, Ofs_AL);	// cmp [ebx+reg],al
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*af*/	case SCASw: {	int m = mode|MOVSDST;
			Gen(O_MOVS_SetA, m);
			Gen(L_DI_R1, m);		// mov ax,[edi]
			Gen(O_CMP_FR, m, Ofs_EAX);	// cmp [ebx+reg],ax
			Gen(O_MOVS_SavA, m);
			PC++; } break;

/*b0*/	case MOVial:
/*b1*/	case MOVicl:
/*b2*/	case MOVidl:
/*b3*/	case MOVibl:
/*b4*/	case MOViah:
/*b5*/	case MOVich:
/*b6*/	case MOVidh:
/*b7*/	case MOVibh:
			Gen(L_IMM, mode|MBYTE, SEL_B_X(D_LO(opc)), Fetch(PC+1));
			PC += 2; break;
/*b8*/	case MOViax:
/*b9*/	case MOVicx:
/*ba*/	case MOVidx:
/*bb*/	case MOVibx:
/*bc*/	case MOVisp:
/*bd*/	case MOVibp:
/*be*/	case MOVisi:
/*bf*/	case MOVidi:
			Gen(L_IMM, mode, R1Tab_l[D_LO(opc)], DataFetchWL_U(mode,PC+1));
			INC_WL_PC(mode, 1); break;

/*d0*/	case SHIFTb:
/*d2*/	case SHIFTbv:
/*c0*/	case SHIFTbi: {
			int m = mode | MBYTE;
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
					CODE_FLUSH();
					goto illegal_op;
				}
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
			if (TheCPU.mode & RM_REG)
				Gen(S_REG, m, REG3);
			else
				Gen(S_DI, m);
		}
		break;
/*d1*/	case SHIFTw:
/*d3*/	case SHIFTwv:
/*c1*/	case SHIFTwi: {
			int m = mode;
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
					CODE_FLUSH();
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
			if (TheCPU.mode & RM_REG)
				Gen(S_REG, m, REG3);
			else
				Gen(S_DI, m);
		}
		break;

/*e9*/	case JMPd:
/*e8*/	case CALLd:
		    PC = JumpGen(PC, mode, opc, 1 + BT24(BitDATA16,mode));
		    if (TheCPU.err) return PC;
		    break;

/*9a*/	case CALLl:
/*ea*/	case JMPld:
		    if (REALADDR()) {
			int len = 3 + BT24(BitDATA16,mode);
			dosaddr_t oip = PC + len - LONG_CS;
			ocs = TheCPU.cs;
			PC = JumpGen(PC, mode, opc, len);
			if (debug_level('e')>2) {
			    if (opc==CALLl)
				e_printf("CALL_FAR: ret=%04x:%08x\n  calling:	   %04x:%08x\n",
					 ocs,oip,TheCPU.cs,PC-LONG_CS);
			    else
				e_printf("JMP_FAR: %04x:%08x\n",TheCPU.cs,PC-LONG_CS);
			}
			if (TheCPU.err) return PC;
		    }
		    else {
			unsigned short jcs;
			unsigned long oip,xcs,jip=0;
			CODE_FLUSH();
			/* get new cs:ip */
			jip = DataFetchWL_U(mode, PC+1);
			INC_WL_PC(mode,1);
			jcs = FetchW(PC);
			PC+=2;
			/* check if new cs is valid, save old for error */
			ocs = TheCPU.cs;
			xcs = LONG_CS;
			TheCPU.err = MAKESEG(mode, Ofs_CS, jcs);
			if (TheCPU.err) {
			    TheCPU.cs = ocs;
			    TheCPU.cs_cache.BoundL = TheCPU.mem_base + xcs;
			    // should not change
			    return P0;
			}
			if (opc==CALLl) {
			    /* ok, now push old cs:eip */
			    oip = PC - xcs;
			    PUSH(mode, ocs);
			    PUSH(mode, oip);
			    if (debug_level('e')>2)
				e_printf("CALL_FAR: ret=%04x:%08lx\n  calling:      %04x:%08lx\n",
					ocs,oip,jcs,jip);
			}
			else {
			    if (debug_level('e')>2)
				e_printf("JMP_FAR: %04x:%08lx\n",jcs,jip);
			}
			TheCPU.eip = jip;
			PC = LONG_CS + jip;
#ifdef SKIP_EMU_VBIOS
			if ((jcs&0xf000)==config.vbios_seg) {
			    /* return the new PC after the jump */
			    TheCPU.err = EXCP_GOBACK; return PC;
			}
#endif
			}
			break;

/*c2*/	case RETisp: {
			int dr = (signed short)FetchW(PC+1);
			Gen(O_POP, mode|MRETISP, dr);
			PC = JumpGen(PC, mode, opc, 3);
			if (debug_level('e')>2)
				e_printf("RET: ret=%08x inc_sp=%d\n",PC-LONG_CS,dr);
			if (TheCPU.err) return PC; }
			break;
/*c3*/	case RET:
			Gen(O_POP, mode);
			PC = JumpGen(PC, mode, opc, 1);
			if (debug_level('e')>2) e_printf("RET: ret=%08x\n",PC-LONG_CS);
			if (TheCPU.err) return PC;
			break;
/*c6*/	case MOVbirm:
			PC += ModRM(opc, PC, mode|MBYTE);
			if (TheCPU.mode & RM_REG)
			    Gen(L_IMM, mode|MBYTE, REG3, Fetch(PC));
			else
			    Gen(S_DI_IMM, mode|MBYTE, Fetch(PC));
			PC++; break;
/*c7*/	case MOVwirm:
			PC += ModRM(opc, PC, mode);
			if (TheCPU.mode & RM_REG)
			    Gen(L_IMM, mode, REG3, DataFetchWL_U(mode,PC));
			else
			    Gen(S_DI_IMM, mode, DataFetchWL_U(mode,PC));
			INC_WL_PC(mode,0);
			break;
/*c8*/	case ENTER: {
			unsigned int sp, bp, frm;
			int level, ds;
			level = Fetch(PC+3) & 0x1f;
			if (level <= 1) {
				int allocsize = FetchW(PC+1);
				Gen(L_REG, mode, Ofs_EBP);
				Gen(O_PUSH, mode);
				if (level == 1) {
					Gen(L_REG, mode, Ofs_ESP);
					Gen(O_PUSH, mode);
					Gen(S_REG, mode, Ofs_EBP);
				}
				else {
					Gen(L_REG2REG, mode, Ofs_ESP, Ofs_EBP);
				}
				// subtract AllocSize from ESP via
				// "lea -allocsize(%esp), %esp"
				if (allocsize) {
					AddrGen(A_DI_1, 0,
						mode|MLEA|((mode&DATA16)?ADDR16:0)|IMMED,
						-allocsize, Ofs_ESP);
					Gen(S_DI_R, mode, Ofs_ESP);
				}
			}
			else {
				CODE_FLUSH();
				ds = BT24(BitDATA16, mode);
				sp = LONG_SS + ((rESP - ds) & TheCPU.StackMask);
				bp = LONG_SS + (rEBP & TheCPU.StackMask);
				PUSH(mode, rEBP);
				frm = sp - LONG_SS;
				sp -= ds*level;
				while (--level) {
					bp -= ds;
					PUSH(mode, (mode&DATA16) ?
					     READ_WORD(bp) : READ_DWORD(bp));
				}
				PUSH(mode, frm);
				if (mode&DATA16) rBP = frm; else rEBP = frm;
				sp -= FetchW(PC+1);
				temp = sp - LONG_SS;
				rESP = (temp&TheCPU.StackMask) | (rESP&~TheCPU.StackMask);
			}
			PC += 4; }
			break;
/*c9*/	case LEAVE:
			Gen(O_LEAVE, mode); PC++;
			break;
/*ca*/	case RETlisp:
			if (REALADDR()) {
				int dr = (signed short)FetchW(PC+1);
				Gen(O_POP, mode);
				Gen(S_REG, mode, Ofs_EIP);
				Gen(O_POP, mode|MRETISP, dr);
				PC = JumpGen(PC, mode, opc, 3);
				if (debug_level('e')>2)
					e_printf("RET_%d: ret=%08x\n",dr,TheCPU.eip);
				if (TheCPU.err) return PC;
			}
			else { /* restartable */
			unsigned long dr;
			uint16_t sv=0;
			CODE_FLUSH();
			NOS_WORD(mode, &sv);
			dr = AddrFetchWL_U(mode,PC+1);
			TheCPU.err = MAKESEG(mode, Ofs_CS, sv);
			if (TheCPU.err) return P0;
			TheCPU.eip=0; POP(mode, &TheCPU.eip);
			POP_ONLY(mode);
			if (debug_level('e')>2)
				e_printf("RET_%ld: ret=%08x\n",dr,TheCPU.eip);
			PC = LONG_CS + TheCPU.eip;
			temp = rESP + dr;
			rESP = (temp&TheCPU.StackMask) | (rESP&~TheCPU.StackMask);
			}
			break;
/*cc*/	case INT3:
			CODE_FLUSH();
			e_printf("Interrupt 03\n");
			TheCPU.err=EXCP03_INT3; PC++;
			return PC;
/*ce*/	case INTO:
			CODE_FLUSH();
			if (CONFIG_CPUSIM) FlagSync_O();
			PC++;
			if(EFLAGS & EFLAGS_OF)
			{
				e_printf("Overflow interrupt 04\n");
				TheCPU.err=EXCP04_INTO;
				return PC;
			}
			break;
/*cd*/	case INT: {
			int inum = Fetch(PC+1);
#ifdef ASM_DUMP
			fprintf(aLog,"%08x:\t\tint %02x\n", P0, inum);
#endif
			CEmuStat &= ~CeS_TRAP;  // INT suppresses trap
			if (V86MODE() && (TheCPU.cr[4] & CR4_VME) && IOPL == 3) {
				Gen(O_INT, mode, inum, P0);
				if (TheCPU.err) return P0;
				Gen(O_PUSH2F, mode);
				Gen(L_REG, mode, Ofs_CS);
				Gen(O_PUSH, mode);
				Gen(O_PUSHI, mode, PC + 2 - LONG_CS);
				Gen(O_SETFL, mode, INT);
				Gen(L_LXS1, mode, Ofs_EIP);
				Gen(L_DI_R1, mode);
				PC = JumpGen(PC, mode, opc, 2);
				if (debug_level('e')>1)
					dbug_printf("EMU86: directly called int %#x ax=%#x at %#x:%#x\n",
						    inum, TheCPU.eax, TheCPU.cs, PC - LONG_CS);
				break;
			}
			CODE_FLUSH();
			// V86: always #GP(0) if revectored or without VME
			if (V86MODE() && !(TheCPU.cr[4] & CR4_VME) && IOPL<3)
				goto not_permitted;
			if (V86MODE() && (TheCPU.cr[4] & CR4_VME) &&
			    !is_revectored(inum, &vm86s.int_revectored)) {
				uint32_t segoffs;
				segoffs = read_dword(inum << 2);
				if (CONFIG_CPUSIM) FlagSync_All();
				temp = (EFLAGS|IOPL_MASK) & (RETURN_MASK|EFLAGS_IF);
				if (IOPL<3) {
					temp &= ~EFLAGS_IF;
					if (EFLAGS & VIF)
						temp |= EFLAGS_IF;
				}
				PUSH(mode, temp);
				PUSH(mode, TheCPU.cs);
				PUSH(mode, PC + 2 - LONG_CS);
				TheCPU.err = MAKESEG(mode, Ofs_CS, segoffs >> 16);
				if (TheCPU.err) return P0;
				TheCPU.eip = segoffs & 0xffff;
				PC = LONG_CS + TheCPU.eip;
				EFLAGS &= ~(TF|RF|AC|NT);
				EFLAGS &= ~(IOPL==3 ? EFLAGS_IF : EFLAGS_VIF);
				if (debug_level('e')>1)
					dbug_printf("EMU86: directly calling int %#x ax=%#x at %#x:%#x\n",
						    inum, _AX, _CS, _IP);
				break;
			}
			/* protected mode INT or revectored V86 with IOPL=3 */
			if (PROTMODE()) switch(inum) {
			case 0x03:
				TheCPU.err=EXCP03_INT3;
				PC += 2;
				return PC;
			case 0x04:
				TheCPU.err=EXCP04_INTO;
				PC += 2;
				return PC;
			}
			TheCPU.scp_err = (inum << 3) | 2;
			goto not_permitted;
			break;
		}

/*cb*/	case RETl:
		   if (REALADDR()) {
			Gen(O_POP, mode);
			Gen(S_REG, mode, Ofs_EIP);
			Gen(O_POP, mode);
			PC = JumpGen(PC, mode, opc, 1);
			if (debug_level('e')>1)
			    e_printf("RET_FAR: ret=%04x:%08x\n",TheCPU.cs,TheCPU.eip);
			if (TheCPU.err) return PC;
			break;	/* un-fall */
		   }
		   /* fall through */
/*cf*/	case IRET: {	/* restartable */
			uint16_t sv=0;
			int m = mode;
			CODE_FLUSH();
			NOS_WORD(m, &sv);	/* get segment */
			TheCPU.err = MAKESEG(m, Ofs_CS, sv);
			if (TheCPU.err) return P0;
			TheCPU.eip=0; POP(m, &TheCPU.eip);
			POP_ONLY(m);
			PC = LONG_CS + TheCPU.eip;
			if (opc==RETl) {
			    if (debug_level('e')>1)
				e_printf("RET_FAR: ret=%04x:%08x\n",sv,TheCPU.eip);
			    break;	/* un-fall */
			}
			if (debug_level('e')>1) {
				e_printf("IRET: ret=%04x:%08x\n",sv,TheCPU.eip);
			}
			temp=0; POP(m, &temp);
			if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
			if (REALMODE())
			    FLAGS = temp;
			else if (V86MODE()) {
			    goto stack_return_from_vm86;
			}
			else {
			    /* if (EFLAGS&EFLAGS_NT) goto task_return */
			    /* if (temp&EFLAGS_VM) goto stack_return_to_vm86 */
			    /* else stack_return */
			    int amask = (CPL==0? 0:EFLAGS_IOPL_MASK) | 2;
			    if (mode & DATA16)
				FLAGS = (FLAGS&amask) | ((temp&0x7fd7)&~amask);
			    else	/* should use eTSSMASK */
				EFLAGS = (EFLAGS&amask) |
					 ((temp&(eTSSMASK|0xfd7))&~amask);
			    TheCPU.df_increments = (EFLAGS&DF)?0xfcfeff:0x040201;
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x->{r=%08x v=%08x}\n",temp,EFLAGS,get_FLAGS(EFLAGS));
			}
			} break;

/*9d*/	case POPF: {
			CODE_FLUSH();
			temp=0; POP(mode, &temp);
			if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
			if (V86MODE()) {
			    int is_tf;
stack_return_from_vm86:
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x fl=%08x\n",
					temp,EFLAGS);
			    is_tf = !!(EFLAGS & TF);
			    if (IOPL==3) {	/* Intel manual */
				/* keep reserved bits + IOPL,VIP,VIF,VM,RF */
				if (mode & DATA16)
				    FLAGS &= ~(SAFE_MASK|EFLAGS_IF);
				else
				    EFLAGS &= ~(SAFE_MASK|EFLAGS_IF);
				EFLAGS |= temp & (SAFE_MASK|EFLAGS_IF);
			    }
			    else {
				/* virtual-8086 monitor */
				if (!(TheCPU.cr[4] & CR4_VME))
				    goto not_permitted;	/* GPF */
				/* move mask from pop{e}flags to regs->eflags */
				if (mode & DATA16)
				    FLAGS &= ~SAFE_MASK;
				else
				    EFLAGS &= ~SAFE_MASK;
				EFLAGS |= temp & SAFE_MASK;
				if (temp & EFLAGS_IF)
				    EFLAGS |= EFLAGS_VIF;
			    }
			    if (temp & EFLAGS_IF) {
				if (vm86s.regs.eflags & VIP) {
				    if (debug_level('e')>1)
					e_printf("Return for STI fl=%08x\n",
						 EFLAGS);
				    TheCPU.err = (is_tf ? EXCP01_SSTP : EXCP_STISIGNAL);
				    return PC + (opc==POPF);
				}
			    }
			}
			else {
			    int amask = (CPL==0? 0:EFLAGS_IOPL_MASK) |
			    		(CPL<=IOPL? 0:EFLAGS_IF) |
			    		(EFLAGS_VM|EFLAGS_RF) | 2;
			    if (mode & DATA16)
				FLAGS = (FLAGS&amask) | ((temp&0x7fd7)&~amask);
			    else
				EFLAGS = (EFLAGS&amask) |
					 ((temp&(eTSSMASK|0xfd7))&~amask);
			    // unused "extended PVI" since real PVI does not
			    // affect POPF
			    if (IOPL<3 && (TheCPU.cr[4]&CR4_PVI)) {
				if (temp & EFLAGS_IF)
				    set_IF();
				else {
				    clear_IF();
				}
			    }
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x->{r=%08x v=%08x}\n",temp,EFLAGS,_EFLAGS);
			}
			TheCPU.df_increments = (EFLAGS&DF)?0xfcfeff:0x040201;
			if (opc==POPF) PC++; }
			break;

/*f2*/	case REPNE:
/*f3*/	case REP: /* also is REPE */ {
			unsigned char repop; int repmod, realrepmod;
			PC++;
			repmod = mode | (opc==REPNE? MREPNE:MREP);
repag0:
			realrepmod = repmod;
			if ((EFLAGS & TF) &&
			    ((repmod & ADDR16) ? rCX : rECX) > 0) {
				/* simulate rep below with TF set, because
				   we must trap for every string ins */
				repmod = repmod & ~(MREPNE|MREP);
			}
			repop = Fetch(PC);
			switch (repop) {
				case INSb:
				case INSw:
				case OUTSb:
				case OUTSw:
					CODE_FLUSH();
					goto not_permitted;
				case LODSb:
					repmod |= (MBYTE|MOVSSRC);
					Gen(O_MOVS_SetA, repmod);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_LodD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
					}
					Gen(S_REG, repmod, Ofs_AL);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case LODSw:
					repmod |= MOVSSRC;
					Gen(O_MOVS_SetA, repmod);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_LodD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
					}
					Gen(S_REG, repmod, Ofs_AX);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case MOVSb:
					repmod |= (MBYTE|MOVSSRC|MOVSDST);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod);
						Gen(O_MOVS_MovD, repmod);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC);
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case MOVSw:
					repmod |= (MOVSSRC|MOVSDST);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod);
						Gen(O_MOVS_MovD, repmod);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC);
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case CMPSb:
					repmod |= (MBYTE|MOVSSRC|MOVSDST|MREPCOND);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC);
					}
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case CMPSw:
					repmod |= (MOVSSRC|MOVSDST|MREPCOND);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_SetA, repmod);
					}
					else {
						Gen(O_MOVS_SetA, repmod&~MOVSDST);
						Gen(L_DI_R1, repmod);
						Gen(O_MOVS_SetA, repmod&~MOVSSRC);
					}
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case STOSb:
					repmod |= (MBYTE|MOVSDST);
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_StoD, repmod);
					}
					else {
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case STOSw:
					repmod |= MOVSDST;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, Ofs_EAX);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_StoD, repmod);
					}
					else {
						Gen(S_DI, repmod);
					}
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case SCASb:
					repmod |= (MBYTE|MOVSDST|MREPCOND);
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_ScaD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
						Gen(O_CMP_FR, repmod, Ofs_AL);
					}
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case SCASw:
					repmod |= MOVSDST|MREPCOND;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, Ofs_EAX);
					if (repmod & (MREPNE|MREP)) {
						Gen(O_MOVS_ScaD, repmod);
					}
					else {
						Gen(L_DI_R1, repmod);
						Gen(O_CMP_FR, repmod, Ofs_AL);
					}
					Gen(O_MOVS_SavA, repmod);
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
					    e_printf("OPERoverride: new mode %04x\n",repmod);
					PC++; goto repag0;
				case ADDRoverride:
					repmod = (repmod & ~ADDR16) | (~basemode & ADDR16);
					if (debug_level('e')>4)
					    e_printf("ADDRoverride: new mode %04x\n",repmod);
					PC++; goto repag0;
			}
			if ((EFLAGS & TF) && !(repmod & (MREP|MREPNE))) {
				/* with TF set, we simulate REP and maybe back
				   up IP */
				int rc = 0;
#ifdef HOST_ARCH_X86
				if (!CONFIG_CPUSIM) {
					NewIMeta(P0, &rc);
					CODE_FLUSH();
					/* don't cache intermediate nodes */
					InvalidateNodeRange(P0, PC - P0, NULL);
				}
#endif
				if (CONFIG_CPUSIM) FlagSync_All();
				if (repmod & ADDR16) {
					rCX--;
					if (rCX == 0) break;
				} else {
					rECX--;
					if (rECX == 0) break;
				}
				if ((repop != CMPSb && repop != CMPSw &&
				     repop != SCASb && repop != SCASw) ||
				    ((realrepmod&MREP?1:0)==(EFLAGS&ZF?1:0))) {
					PC = P0;
					break;
				}
			} }
			break;
/*f4*/	case HLT:
			CODE_FLUSH();
			goto not_permitted;
/*f5*/	case CMC:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1))
				EFLAGS ^= EFLAGS_CF;
			else
				Gen(O_SETFL, mode, CMC);
			break;
/*f6*/	case GRP1brm: {
			PC += ModRM(opc, PC, mode|MBYTE|MLOAD);	// al=[rm]
			switch(REG1) {
			case Ofs_AL:	/*0*/	/* TEST */
			case Ofs_CL:	/*1*/	/* undocumented */
				Gen(O_AND_R, mode|MBYTE|IMMED, Fetch(PC));	// op al,#imm
				PC++;
				break;
			case Ofs_DL:	/*2*/	/* NOT */
				Gen(O_NOT, mode|MBYTE);			// not al
				if (TheCPU.mode & RM_REG)
					Gen(S_REG, mode|MBYTE, REG3);
				else
					Gen(S_DI, mode|MBYTE);
				break;
			case Ofs_BL:	/*3*/	/* NEG */
				Gen(O_NEG, mode|MBYTE);			// neg al
				if (TheCPU.mode & RM_REG)
					Gen(S_REG, mode|MBYTE, REG3);
				else
					Gen(S_DI, mode|MBYTE);
				break;
			case Ofs_AH:	/*4*/	/* MUL AL */
				Gen(O_MUL, mode|MBYTE);			// al*[edi]->AX unsigned
				break;
			case Ofs_CH:	/*5*/	/* IMUL AL */
				Gen(O_IMUL, mode|MBYTE);		// al*[edi]->AX signed
				break;
			case Ofs_DH:	/*6*/	/* DIV AL */
				Gen(O_DIV, mode|MBYTE, P0);			// ah:al/[edi]->AH:AL unsigned
				if (CONFIG_CPUSIM && TheCPU.err) return P0;
				break;
			case Ofs_BH:	/*7*/	/* IDIV AL */
				Gen(O_IDIV, mode|MBYTE, P0);		// ah:al/[edi]->AH:AL signed
				if (CONFIG_CPUSIM && TheCPU.err) return P0;
				break;
			} }
			break;
/*f7*/	case GRP1wrm: {
			PC += ModRM(opc, PC, mode|MLOAD);	// (e)ax=[rm]
			switch(REG1) {
			case Ofs_AX:	/*0*/	/* TEST */
			case Ofs_CX:	/*1*/	/* undocumented */
				Gen(O_AND_R, mode|IMMED, DataFetchWL_U(mode,PC));	// op al,#imm
				INC_WL_PC(mode,0);
				break;
			case Ofs_DX:	/*2*/	/* NOT */
				Gen(O_NOT, mode);			// not (e)ax
				if (TheCPU.mode & RM_REG)
					Gen(S_REG, mode, REG3);
				else
					Gen(S_DI, mode);
				break;
			case Ofs_BX:	/*3*/	/* NEG */
				Gen(O_NEG, mode);			// neg (e)ax
				if (TheCPU.mode & RM_REG)
					Gen(S_REG, mode, REG3);
				else
					Gen(S_DI, mode);
				break;
			case Ofs_SP:	/*4*/	/* MUL AX */
				Gen(O_MUL, mode);			// (e)ax*[edi]->(E)DX:(E)AX unsigned
				break;
			case Ofs_BP:	/*5*/	/* IMUL AX */
				Gen(O_IMUL, mode);			// (e)ax*[edi]->(E)DX:(E)AX signed
				break;
			case Ofs_SI:	/*6*/	/* DIV AX+DX */
				Gen(O_DIV, mode, P0);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX unsigned
				if (CONFIG_CPUSIM && TheCPU.err) return P0;
				break;
			case Ofs_DI:	/*7*/	/* IDIV AX+DX */
				Gen(O_IDIV, mode, P0);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX signed
				if (CONFIG_CPUSIM && TheCPU.err) return P0;
				break;
			} }
			break;
/*f8*/	case CLC:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1))
				EFLAGS &= ~EFLAGS_CF;
			else
				Gen(O_SETFL, mode, CLC);
			break;
/*f9*/	case STC:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1))
				EFLAGS |= EFLAGS_CF;
			else
				Gen(O_SETFL, mode, STC);
			break;
/*fa*/	case CLI:
			if (REALMODE() || (CPL <= IOPL) || (IOPL==3)) {
				Gen(O_SETFL, mode, CLI);
			}
			else {
			    CODE_FLUSH();
			    /* virtual-8086 monitor */
			    if (V86MODE()) {
				if (debug_level('e')>2) e_printf("Virtual VM86 CLI\n");
				if (TheCPU.cr[4] & CR4_VME)
				    EFLAGS &= ~EFLAGS_VIF;
				else
				    goto not_permitted;	/* GPF */
			    }
			    else if (TheCPU.cr[4] & CR4_PVI) {
				CODE_FLUSH();
				if (debug_level('e')>2) e_printf("Virtual DPMI CLI\n");
				clear_IF();
			    }
			    else
				goto not_permitted;	/* GPF */
			}
			PC++;
			break;
/*fb*/	case STI:
			CODE_FLUSH();
			if (V86MODE()) {    /* traps always (Intel man) */
				/* virtual-8086 monitor */
				if (debug_level('e')>2) e_printf("Virtual VM86 STI\n");
				if (IOPL==3)
				    EFLAGS |= EFLAGS_IF;
				else if (TheCPU.cr[4]&CR4_VME)
				    EFLAGS |= EFLAGS_VIF;
				else
				    goto not_permitted;	/* GPF */
				if (vm86s.regs.eflags & VIP) {
				    if (debug_level('e')>1)
					e_printf("Return for STI fl=%08x\n",
					    EFLAGS);
				    TheCPU.err=EXCP_STISIGNAL;
				    return PC+1;
				}
			}
			else {
			    if (REALMODE() || (CPL <= IOPL) || (IOPL==3)) {
				EFLAGS |= EFLAGS_IF;
			    }
			    else if (TheCPU.cr[4] & CR4_PVI) {
				if (debug_level('e')>2) e_printf("Virtual DPMI STI\n");
				set_IF();
			    }
			    else
				goto not_permitted;	/* GPF */
			}
			PC++;
			break;
/*fc*/	case CLD:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1)) {
				EFLAGS &= ~EFLAGS_DF;
				TheCPU.df_increments = 0x040201;
			}
			else
				Gen(O_SETFL, mode, CLD);
			break;
/*fd*/	case STD:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1)) {
				EFLAGS |= EFLAGS_DF;
				TheCPU.df_increments = 0xfcfeff;
			}
			else
				Gen(O_SETFL, mode, STD);
			break;
/*fe*/	case GRP2brm:	/* only INC and DEC are legal on bytes */
			ModGetReg1(PC, mode);
			switch(REG1) {
			case Ofs_AL:	/*0*/
				if (Fetch(PC+1) >= 0xc0) {
					Gen(O_INC_R, mode|MBYTE,
					    R1Tab_b[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, mode|MBYTE|MLOAD);//al=[rm]
				Gen(O_INC, mode|MBYTE);
				Gen(S_DI, mode|MBYTE);
				break;
			case Ofs_CL:	/*1*/
				if (Fetch(PC+1) >= 0xc8) {
					Gen(O_DEC_R, mode|MBYTE,
					    R1Tab_b[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, mode|MBYTE|MLOAD);//al=[rm]
				Gen(O_DEC, mode|MBYTE);
				Gen(S_DI, mode|MBYTE);
				break;
			default:
				CODE_FLUSH();
				goto illegal_op;
			}
			break;
/*ff*/	case GRP2wrm:
			ModGetReg1(PC, mode);
			switch (REG1) {
			case Ofs_AX:	/*0*/
				/* it's not worth optimizing for registers
				   here (single byte DEC/INC exist), but do
				   it for consistency anyway.
				*/
				if (Fetch(PC+1) >= 0xc0) {
					Gen(O_INC_R, mode,
					    R1Tab_l[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, mode|MLOAD);
				Gen(O_INC, mode);
				Gen(S_DI, mode);
				break;
			case Ofs_CX:	/*1*/
				if (Fetch(PC+1) >= 0xc8) {
					Gen(O_DEC_R, mode,
					    R1Tab_l[Fetch(PC+1) & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, mode|MLOAD);
				Gen(O_DEC, mode);
				Gen(S_DI, mode);
				break;
			case Ofs_SP:	/*4*/	 // JMP near indirect
				PC = JumpGen(PC, mode, (opc<<8)|REG1,
					ModRM(opc, PC, mode|NOFLDR|MLOAD));
				if (TheCPU.err) return PC;
				break;
			case Ofs_DX: {	/*2*/	 // CALL near indirect
				/* don't use MLOAD as O_PUSHI clobbers eax */
				int len = ModRM(opc, PC, mode|NOFLDR);
				dosaddr_t ret = PC + len - LONG_CS;
				Gen(O_PUSHI, mode, ret);
				if (TheCPU.mode & RM_REG)
					Gen(L_REG, mode, REG3);
				else
					Gen(L_DI_R1, mode);
				PC = JumpGen(PC, mode, (opc<<8)|REG1, len);
				if (debug_level('e')>2)
					e_printf("CALL indirect: ret=%08x\n\tcalling: %08x\n",
						 ret,PC-LONG_CS);
				if (TheCPU.err) return PC;
				}
				break;
			case Ofs_BX:	/*3*/	 // CALL long indirect restartable
			case Ofs_BP:	/*5*/	 // JMP long indirect restartable
				if (Fetch(PC+1) >= 0xc0) {
					CODE_FLUSH();
					goto illegal_op;
				}
				if (REALADDR()) {
					dosaddr_t oip = 0;
					int len = ModRM(opc, PC, mode|NOFLDR);
					if (REG1==Ofs_BX) {
					    /* ok, now push old cs:eip */
					    ocs = TheCPU.cs;
					    oip = PC + len - LONG_CS;
					    Gen(L_REG, mode, Ofs_CS);
					    Gen(O_PUSH, mode);
					    Gen(O_PUSHI, mode, oip);
					}
					Gen(L_LXS1, mode, Ofs_EIP);
					Gen(L_DI_R1, mode);
					PC = JumpGen(PC, mode, (opc<<8)|REG1, len);
					if (debug_level('e')>2) {
					    unsigned short jcs = TheCPU.cs;
					    dosaddr_t jip = PC - LONG_CS;
					    if (REG1==Ofs_BX)
						e_printf("CALL_FAR indirect: ret=%04x:%08x\n\tcalling: %04x:%08x\n",
							 ocs,oip,jcs,jip);
					    else
						e_printf("JMP_FAR indirect: %04x:%08x\n",jcs,jip);
					}
#ifdef SKIP_EMU_VBIOS
					if ((jcs&0xf000)==config.vbios_seg) {
					    /* return the new PC after the jump */
					    TheCPU.err = EXCP_GOBACK;
					}
#endif
					if (TheCPU.err) return PC;
				}
				else {
					unsigned short jcs;
					unsigned long oip,xcs,jip=0;
					CODE_FLUSH();
					PC += ModRMSim(PC, mode|NOFLDR);
					TheCPU.eip = PC - LONG_CS;
					/* get new cs:ip */
					jip = DataGetWL_U(mode, TheCPU.mem_ref);
					jcs = GetDWord(TheCPU.mem_ref+BT24(BitDATA16,mode));
					/* check if new cs is valid, save old for error */
					ocs = TheCPU.cs;
					xcs = LONG_CS;
					TheCPU.err = MAKESEG(mode, Ofs_CS, jcs);
					if (TheCPU.err) {
					    TheCPU.cs = ocs;
					    TheCPU.cs_cache.BoundL = TheCPU.mem_base + xcs;
					    // should not change
					    return P0;
					}
					if (REG1==Ofs_BX) {
					    /* ok, now push old cs:eip */
					    oip = PC - xcs;
					    PUSH(mode, ocs);
					    PUSH(mode, oip);
					    if (debug_level('e')>2)
						e_printf("CALL_FAR indirect: ret=%04x:%08lx\n\tcalling: %04x:%08lx\n",
							ocs,oip,jcs,jip);
					}
					else {
					    if (debug_level('e')>2)
						e_printf("JMP_FAR indirect: %04x:%08lx\n",jcs,jip);
					}
					TheCPU.eip = jip;
					PC = LONG_CS + jip;
				}
				break;
			case Ofs_SI:	/*6*/	 // PUSH
				PC += ModRM(opc, PC, mode|MLOAD);
				Gen(O_PUSH, mode); break;	// push [rm]
				break;
			default:
				CODE_FLUSH();
				goto illegal_op;
			}
			break;

/*6c*/	case INSb: {
			unsigned short a;
			unsigned int rd;
			CODE_FLUSH();
			a = rDX;
			if (!test_ioperm(a)) goto not_permitted;
			rd = (mode&ADDR16? rDI:rEDI);
			WRITE_BYTE(LONG_ES+rd, port_inb(a));
			if (EFLAGS & EFLAGS_DF) rd--; else rd++;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			PC++; } break;
/*ec*/	case INvb: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
#ifdef TRAP_RETRACE
			if (a==0x3da) {		// video retrace bits
			    /* bit 0 = DE  bit 3 = VR
			     * display=0 hor.retr.=1 ver.retr.=9 */
			    int c1 = *((int *)(PC+1));
			    int c2 = *((int *)(PC+5));
			    int tp = test_ioperm(a) & 1;
			    dbug_printf("IN 3DA %08lx %08lx PP=%d\n",c1,c2,tp);
			    if (c1==0xfb7408a8) {
				// 74 fb = wait for VR==1
				if (tp==0) set_ioperm(a,1,1);
				while (((rAL=port_inb(a))&8)==0);
				if (tp==0) set_ioperm(a,1,0);
				PC+=5; break;
			    }
			    //else if (c1==0xfb7508a8) {
				// 75 fb = wait for VR==0
			    //}
			    //else if (c1==0xfbe008a8) {
				// e0 fb = loop while VR==1
				//unsigned int rcx = mode&DATA16? rCX:rECX;
				//if (tp==0) set_ioperm(a,1,1);
				//while ((((rAL=port_inb(a))&8)!=0) && rcx)
				//    rcx--;
				//if (tp==0) set_ioperm(a,1,0);
				//if (mode&DATA16) rCX=rcx; else rECX=rcx;
				//PC+=5; break;
			    //}
			    else if (c1==0xfbe108a8) {
				// e1 fb = loop while VR==0
				unsigned int rcx = mode&DATA16? rCX:rECX;
				if (tp==0) set_ioperm(a,1,1);
				while ((((rAL=port_inb(a))&8)==0) && rcx)
				    rcx--;
				if (tp==0) set_ioperm(a,1,0);
				if (mode&DATA16) rCX=rcx; else rECX=rcx;
				PC+=5; break;
			    }
			    else if (((c1&0xfffff6ff)==0xc0080024) &&
				((c2&0xfffffffe)==0xf4eb0274)) {
				// 74 02 eb f4 = wait for HR,VR==0
				// 75 02 eb f4 = wait for HR,VR==1
				register unsigned char amk = Fetch(PC+1);
				if (amk==8) {
				    if (tp==0) set_ioperm(a,1,1);
				    if (PC[5]&1)
					while (((rAL=port_inb(a))&amk)==0);
				    else
					while (((rAL=port_inb(a))&amk)!=0);
				    if (tp==0) set_ioperm(a,1,0);
				}
				else
				    rAL = (PC[5]&1? amk:0);
				PC+=9; break;
			    }
			}
#endif
			if (!test_ioperm(a)) goto not_permitted;
#ifdef CPUEMU_DIRECT_IO
			Gen(O_INPDX, mode|MBYTE); NewNode=1;
#else
			rAL = port_inb(a);
#endif
			}
			PC++; break;
/*e4*/	case INb: {
			unsigned short a;
			CODE_FLUSH();
			E_TIME_STRETCH;		// for PIT
			/* there's no reason to compile this, as most of
			 * the ports under 0x100 are emulated by dosemu */
			a = Fetch(PC+1);
			if (!test_ioperm(a)) goto not_permitted;
			rAL = port_inb(a);
			PC += 2; } break;
/*6d*/	case INSw: {
			unsigned int rd;
			int dp;
			CODE_FLUSH();
			if (!test_ioperm(rDX)) goto not_permitted;
			rd = (mode&ADDR16? rDI:rEDI);
			if (mode&DATA16) {
				WRITE_WORD(LONG_ES+rd, port_inw(rDX)); dp=2;
			}
			else {
				WRITE_DWORD(LONG_ES+rd, port_ind(rDX)); dp=4;
			}
			if (EFLAGS & EFLAGS_DF) rd-=dp; else rd+=dp;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			PC++; } break;
/*ed*/	case INvw: {
			CODE_FLUSH();
			if (!test_ioperm(rDX)) goto not_permitted;
			if (mode&DATA16) rAX = port_inw(rDX);
			else rEAX = port_ind(rDX);
			} PC++; break;
/*e5*/	case INw: {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if (!test_ioperm(a)) goto not_permitted;
			if (mode&DATA16) rAX = port_inw(a);
			else rEAX = port_ind(a);
			PC += 2; } break;

/*6e*/	case OUTSb: {
			unsigned short a;
			unsigned long rs;
			CODE_FLUSH();
			a = rDX;
			if (!test_ioperm(a)) goto not_permitted;
			rs = (mode&ADDR16? rSI:rESI);
			do {
			    port_outb(a,Fetch(LONG_DS+rs));
			    if (EFLAGS & EFLAGS_DF) rs--; else rs++;
			    PC++;
			} while (Fetch(PC)==OUTSb);
			if (mode&ADDR16) rSI=rs; else rESI=rs;
			} break;
/*ee*/	case OUTvb: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (!test_ioperm(a)) goto not_permitted;
#ifdef CPUEMU_DIRECT_IO
			Gen(O_OUTPDX, mode|MBYTE); NewNode=1;
#else
			port_outb(a,rAL);
#endif
			}
			PC++; break;
/*e6*/	case OUTb:  {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if ((a&0xfc)==0x40) E_TIME_STRETCH;	// for PIT
			/* there's no reason to compile this, as most of
			 * the ports under 0x100 are emulated by dosemu */
			if (!test_ioperm(a)) goto not_permitted;
			port_outb(a,rAL);
			PC += 2; } break;
/*6f*/	case OUTSw:
			CODE_FLUSH();
			goto not_permitted;
/*ef*/	case OUTvw: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (!test_ioperm(a)) goto not_permitted;
#ifdef CPUEMU_DIRECT_IO
			Gen(O_OUTPDX, mode); NewNode=1;
#else
			if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
#endif
			}
			PC++; break;

/*e7*/	case OUTw:  {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if (!test_ioperm(a)) goto not_permitted;
			if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
			PC += 2; } break;

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
			int sim = 0;
			if ((b&0xc0)==0xc0) {
				exop |= 0x40;
				PC += 2;
			}
			else {
				if ((exop&0xeb)==0x21) {
					CODE_FLUSH();
					PC += ModRMSim(PC, mode|NOFLDR);
					b = mode; sim=1;
				}
				else {
					PC += ModRM(opc, PC, mode|NOFLDR);
				}
			}
			b &= 7;
			if (TheCPU.fpstate) {
				/* For simulator, only need to mask all
				   exceptions; for JIT, load emulated FPU state
				   into real FPU */
				if (CONFIG_CPUSIM)
					fesetenv(FE_DFL_ENV);
				else
					loadfpstate(*TheCPU.fpstate);
				TheCPU.fpstate = NULL;
			}
			if (sim) {
			    if (Fp87_op(exop,b)) { TheCPU.err = -96; return P0; }
			}
			else
			    Gen(O_FOP, mode, exop, b);
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
				    CODE_FLUSH();
				    if (REALMODE()) goto illegal_op;
				    PC += ModRMSim(PC+1, mode) + 1;
				    error("SLDT not implemented\n");
				    break;
				case 1: /* STR */
				    /* Store Task Register */
				    CODE_FLUSH();
				    if (REALMODE()) goto illegal_op;
				    PC += ModRMSim(PC+1, mode) + 1;
				    error("STR not implemented\n");
				    break;
				case 2: /* LLDT */
				    /* Load Local Descriptor Table Register */
				case 3: /* LTR */
				    /* Load Task Register */
				    CODE_FLUSH();
				    goto not_permitted;
				case 4: { /* VERR */
				    unsigned short sv; int tmp;
				    CODE_FLUSH();
				    if (REALMODE()) goto illegal_op;
				    PC += ModRMSim(PC+1, mode) + 1;
				    if (TheCPU.mode & RM_REG) {
					sv = CPUWORD(REG3);
				    } else {
					sv = GetDWord(TheCPU.mem_ref);
				    }
				    tmp = hsw_verr(sv);
				    if (tmp < 0) goto illegal_op;
				    EFLAGS &= ~EFLAGS_ZF;
				    if (tmp) EFLAGS |= EFLAGS_ZF;
				    if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
				    }
				    break;
				case 5: { /* VERW */
				    unsigned short sv; int tmp;
				    CODE_FLUSH();
				    if (REALMODE()) goto illegal_op;
				    PC += ModRMSim(PC+1, mode) + 1;
				    if (TheCPU.mode & RM_REG) {
					sv = CPUWORD(REG3);
				    } else {
					sv = GetDWord(TheCPU.mem_ref);
				    }
				    tmp = hsw_verw(sv);
				    if (tmp < 0) goto illegal_op;
				    EFLAGS &= ~EFLAGS_ZF;
				    if (tmp) EFLAGS |= EFLAGS_ZF;
				    if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
				    }
				    break;
				case 6: /* JMP indirect to IA64 code */
				case 7: /* Illegal */
				    CODE_FLUSH();
				    goto illegal_op;
				} }
				break;
			case 0x01: { /* GRP7 - Extended Opcode 21 */
				unsigned char opm = D_MO(Fetch(PC+2));
				switch (opm) {
				case 0: /* SGDT */
				    /* Store Global Descriptor Table Register */
				    PC++; PC += ModRM(opc, PC, mode|DATA16|MSTORE);
				    error("SGDT not implemented\n");
				    break;
				case 1: /* SIDT */
				    /* Store Interrupt Descriptor Table Register */
				    PC++; PC += ModRM(opc, PC, mode|DATA16|MSTORE);
				    error("SIDT not implemented\n");
				    break;
				case 2: /* LGDT */ /* PM privileged AND real mode */
				    /* Load Global Descriptor Table Register */
				case 3: /* LIDT */ /* PM privileged AND real mode */
				    /* Load Interrupt Descriptor Table Register */
				    CODE_FLUSH();
				    goto not_permitted;
				case 4: /* SMSW, 80286 compatibility */
				    /* Store Machine Status Word */
				    Gen(L_CR0, mode);
				    PC++; PC += ModRM(opc, PC, mode|DATA16|MSTORE);
				    break;
				case 5: /* Illegal */
				case 6: /* LMSW, 80286 compatibility, Privileged */
				    /* Load Machine Status Word */
				case 7: /* Illegal */
				    CODE_FLUSH();
				    goto illegal_op;
				} }
				break;

			case 0x02:   /* LAR */ /* Load Access Rights Byte */
			case 0x03: { /* LSL */ /* Load Segment Limit */
				unsigned short sv; int tmp;
				CODE_FLUSH();
				if (REALMODE()) goto illegal_op;
				PC += ModRMSim(PC+1, mode) + 1;
				if (TheCPU.mode & RM_REG) {
				    sv = CPUWORD(REG3);
				} else {
				    sv = GetDWord(TheCPU.mem_ref);
				}
				if (!e_larlsl(mode, sv)) {
				    EFLAGS &= ~EFLAGS_ZF;
				    if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
				}
				else {
				    if (opc2==0x02) {	/* LAR */
					tmp = GetSelectorFlags(sv);
					if (mode&DATA16) tmp &= 0xff;
					tmp <<= 8;
					if (tmp) SetFlagAccessed(sv);
				    }
				    else {		/* LSL */
					tmp = GetSelectorByteLimit(sv);
				    }
				    EFLAGS |= EFLAGS_ZF;
				    if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
				    SetCPU_WL(mode, REG1, tmp);
				} }
				break;

			/* case 0x04:	LOADALL */
			/* case 0x05:	LOADALL(286) - SYSCALL(K6) */
			case 0x06: /* CLTS */ /* Privileged */
				/* Clear Task State Register */
				CODE_FLUSH();
				if (CPL != 0) goto not_permitted;
				TheCPU.cr[0] &= ~8;
				PC += 2; break;
			/* case 0x07:	LOADALL(386) - SYSRET(K6) etc. */
			case 0x08: /* INVD */
				/* INValiDate cache */
			case 0x09: /* WBINVD */
				/* Write-Back and INValiDate cache */
				PC += 2; break;
			case 0x0a:
			case 0x0b:
				CODE_FLUSH();
				goto illegal_op;	/* UD2 */
			/* case 0x0d:	Code Extension 25(AMD-3D) */
			/* case 0x0e:	FEMMS(K6-3D) */
			case 0x0f:	/* AMD-3D */
			/* case 0x10-0x1f:	various V20/MMX instr. */
				CODE_FLUSH();
				goto not_implemented;
			case 0x20:   /* MOVcdrd */ /* Privileged */
			case 0x22:   /* MOVrdcd */ /* Privileged */
			case 0x21:   /* MOVddrd */ /* Privileged */
			case 0x23:   /* MOVrddd */ /* Privileged */
			case 0x24:   /* MOVtdrd */ /* Privileged */
			case 0x26: { /* MOVrdtd */ /* Privileged */
				int *srg; int reg; unsigned char b,opd;
				CODE_FLUSH();
				if (REALMODE()) goto not_permitted;
				b = Fetch(PC+2);
				if (D_HO(b)!=3) goto illegal_op;
				reg = D_MO(b); b = D_LO(b);
				if ((b==4)||(b==5)) goto illegal_op;
				srg = (int *)CPUOFFS(R1Tab_l[b]);
				opd = Fetch(PC+1)&2;
		    		if (opc2&1) {
				    if (opd) TheCPU.dr[reg] = *srg;
					else *srg = TheCPU.dr[reg];
		    		}
		    		else if (opc2&4) {
				    reg-=6; if (reg<0) goto illegal_op;
				    if (opd) TheCPU.tr[reg] = *srg;
					else *srg = TheCPU.tr[reg];
		    		}
		    		else {
				    if ((reg==1)||(reg>4)) goto illegal_op;
		    		    if (opd) {	/* write to CRs */
					if (reg==0) {
			    		    if ((TheCPU.cr[0] ^ *srg) & 1) {
						dbug_printf("RM/PM switch not allowed\n");
						TheCPU.err = -94; return P0;
					    }
			    		    TheCPU.cr[0] = (*srg&0xe005002f)|0x10;
					}
					else TheCPU.cr[reg] = *srg;
		    		    } else {
					*srg = TheCPU.cr[reg];
		    		    }
		    		}
		    		} PC += 3; break;
			case 0x30: /* WRMSR */
			    CODE_FLUSH();
			    goto not_implemented;

			case 0x31: /* RDTSC */
				Gen(O_RDTSC, mode);
				PC+=2; break;
			case 0x32: /* RDMSR */
			    CODE_FLUSH();
			    goto not_implemented;

			/* case 0x33:	RDPMC(P6) */
			/* case 0x34:	SYSENTER(PII) */
			/* case 0x35:	SYSEXIT(PII) */
			/* case 0x40-0x4f:	CMOV(P6) */
			/* case 0x50-0x5f:	various Cyrix/MMX */
			case 0x60 ... 0x6b:	/* MMX */
			case 0x6e: case 0x6f:
			case 0x74 ... 0x77:
			/* case 0x78-0x7e:	Cyrix */
			case 0x7e: case 0x7f:
			    CODE_FLUSH();
			    goto not_implemented;
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
				  PC = JumpGen(PC, mode, JO+(opc2-JOimmdisp),
					       2 + BT24(BitDATA16,mode));
				  if (TheCPU.err) return PC;
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
				Gen(O_SETCC, mode, (opc2&0x0f));
				PC++; PC += ModRM(opc, PC, mode|MBYTE|MSTORE);
				break;
///
			case 0xa0: /* PUSHfs */
				Gen(L_REG, mode, Ofs_FS);
				Gen(O_PUSH, mode); PC+=2;
				break;
			case 0xa1: /* POPfs */
				if (REALADDR()) {
				    Gen(O_POP, mode);
				    Gen(S_REG, mode, Ofs_FS);
				    AddrGen(A_SR_SH4, mode, Ofs_FS, Ofs_XFS);
				} else { /* restartable */
				    unsigned short sv = 0;
				    CODE_FLUSH();
				    TOS_WORD(mode, &sv);
				    TheCPU.err = MAKESEG(mode, Ofs_FS, sv);
				    if (TheCPU.err) return P0;
				    POP_ONLY(mode);
				    TheCPU.fs = sv;
				}
				PC+=2;
				break;
///
			case 0xa2: /* CPUID */
				CODE_FLUSH();
				if (rEAX==0) {
					rEAX = 1; rEBX = 0x756e6547;
					rECX = 0x6c65746e; rEDX = 0x49656e69;
				}
				else if (rEAX==1) {
					rEAX = 0x052c; rEBX = rECX = 0;
					rEDX = 0x1bf;
				}
				PC+=2; break;

			case 0xa3: /* BT */
			case 0xab: /* BTS */
			case 0xb3: /* BTR */
			case 0xbb: /* BTC */
				PC++; PC += ModRM(opc, PC, mode);
				if (TheCPU.mode & RM_REG) {
				    Gen(L_REG, mode, REG3);
				}
				else {
				    /* add bit offset to effective address,
				       then load and store from there */
				    Gen(O_BITOP, mode, (opc2-0xa0), REG1);
				    Gen(L_DI_R1, mode);
				}
				Gen(O_BITOP, mode|RM_REG, (opc2-0xa0), REG1);
				if (opc2 != 0xa3) {
				    if (TheCPU.mode & RM_REG)
					Gen(S_REG, mode, REG3);
				    else
					Gen(S_DI, mode);
				}
				break;
			case 0xbc: /* BSF */
			case 0xbd: /* BSR */
				PC++; PC += ModRM(opc, PC, mode|MLOAD);
				mode |= (TheCPU.mode & RM_REG);
				Gen(O_BITOP, mode, (opc2-0xa0), REG1);
				break;
			case 0xba: { /* GRP8 - Code Extension 22 */
				unsigned char opm = (Fetch(PC+2))&0x38;
				switch (opm) {
				case 0x00: /* Illegal */
				case 0x08: /* Illegal */
				case 0x10: /* Illegal */
				case 0x18: /* Illegal */
				    CODE_FLUSH();
				    goto illegal_op;
				case 0x20: /* BT imm8 */
				case 0x28: /* BTS imm8 */
				case 0x30: /* BTR imm8 */
				case 0x38: /* BTC imm8 */
					PC++; PC += ModRM(opc, PC, mode|MLOAD);
					mode |= (TheCPU.mode & RM_REG);
					Gen(O_BITOP, mode, opm, Fetch(PC));
					if (opm != 0x20) {
					    if (TheCPU.mode & RM_REG) {
						Gen(S_REG, mode, REG3);
					    }
					    else {
						Gen(S_DI, mode);
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
				PC++; PC += ModRM(opc, PC, mode|MLOAD);
				if (opc2&1) {
					Gen(O_SHFD, mode, (opc2&8), REG1);
				}
				else {
					Gen(O_SHFD, mode|IMMED, (opc2&8), REG1, Fetch(PC));
					PC++;
				}
				if (TheCPU.mode & RM_REG)
					Gen(S_REG, mode, REG3);
				else
					Gen(S_DI, mode);
				break;

			case 0xa6: /* CMPXCHGb (486 STEP A only) */
			case 0xa7: /* CMPXCHGw (486 STEP A only) */
			    CODE_FLUSH();
			    goto not_implemented;
///
			case 0xa8: /* PUSHgs */
				Gen(L_REG, mode, Ofs_GS);
				Gen(O_PUSH, mode); PC+=2;
				break;
			case 0xa9: /* POPgs */
				if (REALADDR()) {
				    Gen(O_POP, mode);
				    Gen(S_REG, mode, Ofs_GS);
				    AddrGen(A_SR_SH4, mode, Ofs_GS, Ofs_XGS);
				} else { /* restartable */
				    unsigned short sv = 0;
				    CODE_FLUSH();
				    TOS_WORD(mode, &sv);
				    TheCPU.err = MAKESEG(mode, Ofs_GS, sv);
				    if (TheCPU.err) return P0;
				    POP_ONLY(mode);
				    TheCPU.gs = sv;
				}
				PC+=2;
				break;
///
			case 0xaa:
			    CODE_FLUSH();
			    goto illegal_op;
			/* case 0xae:	Code Extension 24(MMX) */
			case 0xaf: /* IMULregrm */
				PC++; PC += ModRM(opc, PC, mode|MLOAD);
				Gen(O_IMUL, mode|MEMADR, REG1);	// reg*[edi]->reg signed
				break;
			case 0xb0:		/* CMPXCHGb */
				PC++; PC += ModRM(opc, PC, mode|MBYTE|MLOAD);
				mode |= (TheCPU.mode & RM_REG);
				Gen(O_CMPXCHG, mode | MBYTE, REG1);
				if (TheCPU.mode & RM_REG)
				    Gen(S_REG, mode | MBYTE, REG3);
				else
				    Gen(S_DI, mode | MBYTE);
				break;
			case 0xb1:		/* CMPXCHGw */
				PC++; PC += ModRM(opc, PC, mode|MLOAD);
				mode |= (TheCPU.mode & RM_REG);
				Gen(O_CMPXCHG, mode, REG1);
				if (TheCPU.mode & RM_REG)
				    Gen(S_REG, mode, REG3);
				else
				    Gen(S_DI, mode);
				break;
///
			case 0xb2: /* LSS */
				if (Fetch(PC+2) >= 0xc0) {
				    CODE_FLUSH();
				    goto not_permitted;
				}
				if (REALADDR()) {
				    PC++; PC += ModRM(opc, PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_SS, Ofs_XSS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    CODE_FLUSH();
				    PC++; PC += ModRMSim(PC, mode);
				    rv = DataGetWL_U(mode,TheCPU.mem_ref);
				    TheCPU.mem_ref += BT24(BitDATA16,mode);
				    sv = GetDWord(TheCPU.mem_ref);
				    TheCPU.err = MAKESEG(mode, Ofs_SS, sv);
				    if (TheCPU.err) return P0;
				    SetCPU_WL(mode, REG1, rv);
				    TheCPU.ss = sv;
				}
				break;
			case 0xb4: /* LFS */
				if (Fetch(PC+2) >= 0xc0) {
				    CODE_FLUSH();
				    goto not_permitted;
				}
				if (REALADDR()) {
				    PC++; PC += ModRM(opc, PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_FS, Ofs_XFS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    CODE_FLUSH();
				    PC++; PC += ModRMSim(PC, mode);
				    rv = DataGetWL_U(mode,TheCPU.mem_ref);
				    TheCPU.mem_ref += BT24(BitDATA16,mode);
				    sv = GetDWord(TheCPU.mem_ref);
				    TheCPU.err = MAKESEG(mode, Ofs_FS, sv);
				    if (TheCPU.err) return P0;
				    SetCPU_WL(mode, REG1, rv);
				    TheCPU.fs = sv;
				}
				break;
			case 0xb5: /* LGS */
				if (Fetch(PC+2) >= 0xc0) {
				    CODE_FLUSH();
				    goto not_permitted;
				}
				if (REALADDR()) {
				    PC++; PC += ModRM(opc, PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_GS, Ofs_XGS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    CODE_FLUSH();
				    PC++; PC += ModRMSim(PC, mode);
				    rv = DataGetWL_U(mode,TheCPU.mem_ref);
				    TheCPU.mem_ref += BT24(BitDATA16,mode);
				    sv = GetDWord(TheCPU.mem_ref);
				    TheCPU.err = MAKESEG(mode, Ofs_GS, sv);
				    if (TheCPU.err) return P0;
				    SetCPU_WL(mode, REG1, rv);
				    TheCPU.gs = sv;
				}
				break;
			case 0xb6: /* MOVZXb */
				PC++; PC += ModRM(opc, PC, mode|MBYTX|MLOAD);
				Gen(L_MOVZS, mode|MBYTX, 0, REG1);
				break;
			case 0xb7: /* MOVZXw */
				PC++; PC += ModRM(opc, PC, mode|DATA16|MLOAD);
				Gen(L_MOVZS, mode, 0, REG1);
				break;
			case 0xbe: /* MOVSXb */
				PC++; PC += ModRM(opc, PC, mode|MBYTX|MLOAD);
				Gen(L_MOVZS, mode|MBYTX, 1, REG1);
				break;
			case 0xbf: /* MOVSXw */
				PC++; PC += ModRM(opc, PC, mode|DATA16|MLOAD);
				Gen(L_MOVZS, mode, 1, REG1);
				break;
///
			case 0xb8:	/* JMP absolute to IA64 code */
			case 0xb9:
			    CODE_FLUSH();
			    goto illegal_op;	/* UD2 */
			case 0xc0: /* XADDb */
				PC++; PC += ModRM(opc, PC, mode|MBYTE|MLOAD);
				Gen(O_XCHG, mode | MBYTE, REG1);
				Gen(O_ADD_R, mode | MBYTE, REG1);
				if (TheCPU.mode & RM_REG)
				    Gen(S_REG, mode|MBYTE, REG3);
				else
				    Gen(S_DI, mode|MBYTE);
				break;
			case 0xc1: /* XADDw */
				PC++; PC += ModRM(opc, PC, mode|MLOAD);
				Gen(O_XCHG, mode, REG1);
				Gen(O_ADD_R, mode, REG1);
				if (TheCPU.mode & RM_REG)
				    Gen(S_REG, mode, REG3);
				else
				    Gen(S_DI, mode);
				break;

			/* case 0xc2-0xc6:	MMX */
			case 0xc7: { /*	Code Extension 23 - 01=CMPXCHG8B mem */
				uint64_t edxeax, m;
				unsigned char modrm;
				CODE_FLUSH();
				modrm = Fetch(PC+2);
				if (D_MO(modrm) != 1 || D_HO(modrm) == 3)
					goto illegal_op;
				PC++; PC += ModRMSim(PC, mode);
				edxeax = ((uint64_t)rEDX << 32) | rEAX;
				m = read_qword(TheCPU.mem_ref);
				if (edxeax == m)
				{
					EFLAGS |= EFLAGS_ZF;
					m = ((uint64_t)rECX << 32) | rEBX;
				} else {
					EFLAGS &= ~EFLAGS_ZF;
					rEDX = m >> 32;
					rEAX = m & 0xffffffff;
				}
				write_qword(TheCPU.mem_ref, m);
				if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
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
				if (!(mode&DATA16)) {
					Gen(O_BSWAP, 0, R1Tab_l[D_LO(opc2)]);
				} /* else undefined */
				PC+=2; break;
			case 0xd1 ... 0xd3:	/* MMX */
			case 0xd5: case 0xd7: case 0xda: case 0xde:
			case 0xdf:
			case 0xe0 ... 0xe5:
			case 0xf1 ... 0xf3:
			case 0xf5 ... 0xf7:
			case 0xfc ... 0xfe:
			    CODE_FLUSH();
			    goto not_implemented;
///
			case 0xff: /* illegal, used by Windows */
			    CODE_FLUSH();
			    goto illegal_op;
			default:
			    CODE_FLUSH();
			    goto not_implemented;
			} }
			break;

/*xx*/	default:
			CODE_FLUSH();
			goto not_implemented;
		}
		if (NewNode) {
			int rc=0;
			if (!CONFIG_CPUSIM && !(TheCPU.mode&SKIPOP)) {
				NewIMeta(P0, &rc);
				if (rc < 0) {
					if (debug_level('e')>2)
						e_printf("============ Tab full:cannot close sequence\n");
					CODE_FLUSH();
					NewIMeta(P0, &rc);
					NewNode = 1;
				}
			}
		}
		else
		{
		    if (eTimeCorrect >= 0)
			TheCPU.EMUtime += FAKE_INS_TIME;
		}

		/* check segment boundaries. TODO for prot mode */
		if (REALADDR() && (PC - LONG_CS > 0xffff)) {
			e_printf("PC out of bounds, %x\n", PC - LONG_CS);
			CODE_FLUSH();
			goto not_permitted;
		}

#ifdef SINGLEBLOCK
		if (!CONFIG_CPUSIM && NewNode && CurrIMeta > 0) {
			P0 = PC;
			CODE_FLUSH();
		}
#endif

		if (NewNode && (CEmuStat & CeS_TRAP)) {
			P0 = PC;
			CODE_FLUSH();
		}
		if (CEmuStat & CeS_MOVSS) {
			/* following non-compiled (sim or protected mode)
			   mov ss / pop ss only */
			if (!(CEmuStat & CeS_INHI)) {
				// directly following mov ss / pop ss
				CEmuStat |= CeS_INHI;
				CEmuStat &= ~CeS_TRAP;
			} else {
				// instruction after clear unconditionally
				// even if it's another mov ss / pop ss
				CEmuStat &= ~(CeS_INHI|CeS_MOVSS);
			}
		}
	}
	return 0;

not_implemented:
	dbug_printf("!!! Unimplemented %02x %02x %02x at %08x\n",opc,
		    Fetch(PC+1),Fetch(PC+2),PC);
	TheCPU.err = -2; return P0;
not_permitted:
	if (debug_level('e')>1) e_printf("!!! Not permitted %02x\n",opc);
	TheCPU.err = EXCP0D_GPF; return P0;
//div_by_zero:
//	dbug_printf("!!! Div by 0 %02x\n",opc);
//	TheCPU.err = -6; return P0;
illegal_op:
	dbug_printf("!!! Illegal op %02x %02x %02x\n",opc,
		    Fetch(PC+1),Fetch(PC+2));
	TheCPU.err = EXCP06_ILLOP; return P0;
}

