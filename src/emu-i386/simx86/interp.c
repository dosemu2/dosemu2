/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2000 Alberto Vignani, FIAT Research Center
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
#include "emu86.h"
#include "codegen.h"
#include "port.h"

unsigned char *P0;
static int NewNode = 1;
static int basemode = 0;
static unsigned char *locJtgt;
extern int Running;

static int ArOpsR[] =
	{ O_ADD_R, O_OR_R, O_ADC_R, O_SBB_R, O_AND_R, O_SUB_R, O_XOR_R, O_CMP_R };
static int ArOpsM[] =
	{      -1,     -1,      -1, O_SBB_M,      -1, O_SUB_M,      -1, O_CMP_M };
static char R1Tab_b[8] =
	{ Ofs_AL, Ofs_CL, Ofs_DL, Ofs_BL, Ofs_AH, Ofs_CH, Ofs_DH, Ofs_BH };
static char R1Tab_l[8] =
	{ Ofs_EAX, Ofs_ECX, Ofs_EDX, Ofs_EBX, Ofs_ESP, Ofs_EBP, Ofs_ESI, Ofs_EDI };

#define SEL_B_X(r)		R1Tab_b[(r)]

#if defined(i386)||defined(__i386)||defined(__i386__)
/* on LE machines the offsets for low byte,low word and full reg are the same */
#define SEL_WL(m,o)	(o)
#define SEL_WL_X(m,r)	R1Tab_l[(r)]
#endif
#if defined(ppc)||defined(__ppc)||defined(__ppc__)
#define SEL_WL(m,o)	((m)&DATA16? (o)+2:(o))
#define SEL_WL_X(m,r)	((m)&DATA16? R1Tab_w[(r)]:R1Tab_l[(r)])
#endif

#define INC_WL_PCA(m,i)	PC=(PC+(i)+((m)&ADDR16? sizeof(short):sizeof(long)))
#define INC_WL_PC(m,i)	PC=(PC+(i)+((m)&DATA16? sizeof(short):sizeof(long)))

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
 * current sequence. Normally the sequence ends on the same address
 * we passed to CloseAndExec, i.e. P0. But sometimes the sequence
 * can return before its end; we must trap this case and abort the
 * current instruction.
 */
#define	CODE_FLUSH()	if (IsCodeInBuf()) {\
			  unsigned char *P2 = CloseAndExec(P0, mode);\
			  if (TheCPU.err) return P0;\
			  if (P2 != P0) { PC=P2; continue; }\
			} NewNode=0

#define UNPREFIX(m)	((m)&~(DATA16|ADDR16))|(basemode&(DATA16|ADDR16))

/////////////////////////////////////////////////////////////////////////////

static int MAKESEG(int mode, int ofs, unsigned short sv)
{
	SDTR tseg, *segc;
	int e;
	char big;
	switch (ofs) {
		case Ofs_CS: segc = &CS_DTR; break;
		case Ofs_DS: segc = &DS_DTR; break;
		case Ofs_ES: segc = &ES_DTR; break;
		case Ofs_SS: segc = &SS_DTR; break;
		case Ofs_FS: segc = &FS_DTR; break;
		case Ofs_GS: segc = &GS_DTR; break;
		default: return EXCP06_ILLOP;
	}
	if (REALADDR()) {
		CPUWORD(ofs) = sv;
		segc->BoundL = ((unsigned long)sv << 4);
		segc->BoundH = segc->BoundL + 0xffff;
		e_printf("SEG %02x = %08lx:%08lx\n",ofs,
			segc->BoundL,segc->BoundH);
		return 0;	// always ok
	}
	e = SET_SEGREG(tseg, big, ofs, sv);
	/* must NOT change segreg and LONG_xx if error! */
	if (e) return e;
	CPUWORD(ofs) = sv;
	if (ofs==Ofs_CS) {
		if (big) basemode &= ~(ADDR16|DATA16);
		else basemode |= (ADDR16|DATA16);
		e_printf("MAKESEG CS: big=%d basemode=%04x\n",big&1,basemode);
	}
	if (ofs==Ofs_SS) {
		if (big) basemode &= ~STACK16; else basemode |= STACK16;
		e_printf("MAKESEG SS: big=%d basemode=%04x\n",big&1,basemode);
	}
	memcpy(segc,&tseg,sizeof(SDTR));
	return 0;
}


/////////////////////////////////////////////////////////////////////////////


unsigned char *JumpGen(unsigned char *P2, int *mode, int cond,
	int btype)
{
	unsigned char *P1;
	int taken = 0;
	int dsp, cxv;
	int pskip = (btype&1? (*mode&ADDR16? 4:6) : 2);

	if ((btype&1)==0)	// short branch (byte)
		dsp = pskip + (signed char)Fetch(P2+1);
	else		// long branch (word/long)
		dsp = pskip + (int)AddrFetchWL_S(*mode, P2+2);
#if defined(OPTIMIZE_BACK_JUMPS) && !defined(SINGLESTEP)
	/*
	 * There's a trick here. Suppose our program does
	 *	label:	cmp location,value
	 *		jcond label
	 * and location is changed by an interrupt (normally the timer):
	 * we'll never get out of the loop, unless we catch the signal
	 * into the loop itself. So what we really do here is:
	 *	label:	cmp location,value
	 *		test signal_pending,1
	 *		jeq ahead
	 *		<end code stub>
	 *		ret
	 *	ahead:	jcond label
	 */
	if ((dsp < 0) && IsCodeInBuf()) {
		IMeta *GHead = &InstrMeta[0];
		unsigned char *hp = P2 + dsp;
		if (LastIMeta && (hp >= GHead->npc)) {
			IMeta *GTgt, *H;
			e_printf("Back jump to %08x\n",(int)hp);
			locJtgt = hp;
			GTgt = NULL;
			for (H=LastIMeta; H>=GHead; --H) {
				if (H->npc == hp) { GTgt = H; break; }
			}
			TheCPU.err = 0;
			if (GTgt) {
			    if (cond==0x21)
				Gen(JCXZ_LOCAL, *mode, GTgt->addr);
			    else if (btype&2)
				Gen(JLOOP_LOCAL, *mode, cond&0x0f, GTgt->addr, P2);
			    else
				Gen(JB_LOCAL, *mode, cond, GTgt->addr, P2);
			    *mode |= M_BJMP;
			    return P2 + pskip;
			}
			else {
			    e_printf("Back jump failed, no target\n");
			    goto jgreal;
			}
		}
	}
#endif
#if defined(OPTIMIZE_FW_JUMPS) && !defined(SINGLESTEP)
	if ((dsp > 0) && (dsp<=0x20) && IsCodeInBuf() && (cond<0x10)) {
		unsigned long hp;
		hp = (long)P2 - LONG_CS + dsp;
		if (*mode&ADDR16) hp &= 0xffff;
		hp += LONG_CS;
		e_printf("Forward jump to %08x\n",(int)hp);
		locJtgt = (unsigned char *)hp;
		Gen(JF_LOCAL, *mode, cond, hp);
		*mode |= M_FJMP;
		return P2 + pskip;
	}
#endif
jgreal:
	P1 = CloseAndExec(P2, *mode); NewNode=0;
	if (TheCPU.err) return NULL;
	if (P1 != P2) return (P0 = P1);

	/* evaluate cond at RUNTIME after exec'ing */
	switch(cond) {
	case 0x00: taken = IS_OF_SET; break;
	case 0x01: taken = !IS_OF_SET; break;
	case 0x02: taken = IS_CF_SET; break;
	case 0x03: taken = !IS_CF_SET; break;
	case 0x04: taken = IS_ZF_SET; break;
	case 0x05: taken = !IS_ZF_SET; break;
	case 0x06: taken = IS_CF_SET || IS_ZF_SET; break;
	case 0x07: taken = !IS_CF_SET && !IS_ZF_SET; break;
	case 0x08: taken = IS_SF_SET; break;
	case 0x09: taken = !IS_SF_SET; break;
	case 0x0a:
		e_printf("!!! JPset\n");
		taken = IS_PF_SET; break;
	case 0x0b:
		e_printf("!!! JPclr\n");
		taken = !IS_PF_SET; break;
	case 0x0c: taken = IS_SF_SET ^ IS_OF_SET; break;
	case 0x0d: taken = !(IS_SF_SET ^ IS_OF_SET); break;
	case 0x0e: taken = (IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET; break;
	case 0x0f: taken = !(IS_SF_SET ^ IS_OF_SET) && !IS_ZF_SET; break;
	case 0x10:	// LOOP
		   cxv = (*mode&ADDR16? --rCX : --rECX);
		   taken = (cxv != 0); break;
	case 0x14:	// LOOPZ
		   cxv = (*mode&ADDR16? --rCX : --rECX);
		   taken = (cxv != 0) && IS_ZF_SET; break;
	case 0x15:	// LOOPNZ
		   cxv = (*mode&ADDR16? --rCX : --rECX);
		   taken = (cxv != 0) && !IS_ZF_SET; break;
	case 0x21:	// LOOP
		   cxv = (*mode&ADDR16? rCX : rECX);
		   taken = (cxv == 0); break;
	case 0x7b: taken = 1; break;
	}
	if (taken) {
		if (dsp==0) {	// infinite loop
		    if ((cond&0xf0)==0x10) {	// loops
		    	// ndiags: shorten delay loops (e2 fe)
			if (*mode&ADDR16) rCX=0; else rECX=0;
			return P2 + pskip;
		    }
		    TheCPU.err = -103;
		    return NULL;
		}
		TheCPU.eip = (long)P2 - LONG_CS + dsp;
		if (*mode&ADDR16) TheCPU.eip &= 0xffff;
		P2 = (unsigned char *)(LONG_CS + TheCPU.eip);
		if (d.emu>2) e_printf("** Jump taken to %08lx\n",(long)P2);
		return P2;
	}
	return P2 + pskip;
}


/////////////////////////////////////////////////////////////////////////////


unsigned char *Interp86(unsigned char *PC, int mod0)
{
	unsigned char opc;
	unsigned long temp;
	int mode;
	TNode *G;

	NewNode = 0;
	basemode = mod0;
	TheCPU.err = 0;

	while (Running) {
		OVERR_DS = Ofs_XDS;
		OVERR_SS = Ofs_XSS;
		P0 = PC;	// P0 changes on instruction boundaries
		mode = basemode;

		if (!NewNode) {
#ifndef SINGLESTEP
			/* for a sequence to be found, it must begin with
			 * an allowable opcode. Look into table */
			while (((InterOps[*PC]&1)==0) && (G=FindTree(PC)) != NULL) {
				int m;
				if (d.emu>2)
					e_printf("** (1)Found compiled code at %08lx\n",(long)(PC));
				if (IsCodeInBuf()) {		// open code?
					if (d.emu>2)
						e_printf("============ Closing open sequence at %08lx\n",(long)(PC));
					PC = CloseAndExec(PC, mode);
					if (TheCPU.err) return PC;
				}
				/* execute the code fragment found and exit
				 * with a new PC */
				m = mode | (G->flags<<16) | XECFND;
				P0 = PC = CloseAndExec(G->addr, m);
				if (TheCPU.err) return PC;
			}
#endif
			if (CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND|CeS_LOCK|CeS_RPIC)) {
				if (CEmuStat & CeS_LOCK)
					CEmuStat &= ~CeS_LOCK;
				else {
					if (CEmuStat & CeS_TRAP) {
						if (TheCPU.err) {
							if (TheCPU.err==EXCP01_SSTP)
								CEmuStat &= ~CeS_TRAP;
							return PC;
						}
						TheCPU.err = EXCP01_SSTP;
					}
					//else if (CEmuStat & CeS_DRTRAP) {
					//	if (e_debug_check(PC)) {
					//		TheCPU.err = EXCP01_SSTP;
					//		return PC;
					//	}
					//}
					else
//					if (EFLAGS & EFLAGS_IF) {
						if (CEmuStat & CeS_SIGPEND) {
							/* force exit after signal */
							CEmuStat = (CEmuStat & ~CeS_SIGPEND) | CeS_SIGACT;
							TheCPU.err=EXCP_SIGNAL;
							return PC;
						}
						else if (CEmuStat & CeS_RPIC) {
							/* force exit for PIC */
							CEmuStat &= ~CeS_RPIC;
							TheCPU.err=EXCP_PICSIGNAL;
							return PC;
						}
						else if (CEmuStat & CeS_STI) {
							/* force exit for IF set */
							CEmuStat &= ~CeS_STI;
							TheCPU.err=EXCP_STISIGNAL;
							return PC;
						}
//					}
				}
			}
		}
// ------ temp ------- debug ------------------------
		if (*((long *)PC)==0) {
			TheCPU.err = -99; return PC;
		}
// ------ temp ------- debug ------------------------
		NewNode = 1;

override:
		switch ((opc=Fetch(PC))) {
/*00*/	case ADDbfrm:
/*02*/	case ADDbtrm:
/*08*/	case ORbfrm:
/*0a*/	case ORbtrm:
/*10*/	case ADCbfrm:
/*12*/	case ADCbtrm:
/*18*/	case SBBbfrm:
/*20*/	case ANDbfrm:
/*22*/	case ANDbtrm:
/*28*/	case SUBbfrm:
/*30*/	case XORbfrm:
/*32*/	case XORbtrm:
/*38*/	case CMPbfrm:
/*84*/	case TESTbrm: {
			int m = mode | MBYTE;
			int op = (opc==TESTbrm? O_AND_R:ArOpsR[D_MO(opc)]);
			PC += ModRM(PC, m);	// SI=reg DI=mem
			Gen(L_DI_R1, m);		// mov al,[edi]
			Gen(op, m, REG1);		// op  al,[ebx+reg]
			if ((opc!=CMPbfrm)&&(opc!=TESTbrm)) {
				if (opc & 2)
					Gen(S_REG, m, REG1);	// mov [ebx+reg],al		reg=mem op reg
				else
					Gen(S_DI, m);		// mov [edi],al			mem=mem op reg
			} }
			break; 
/*1a*/	case SBBbtrm:
/*2a*/	case SUBbtrm:
/*3a*/	case CMPbtrm: {
			int m = mode | MBYTE;
			int op = ArOpsM[D_MO(opc)];
			PC += ModRM(PC, m);	// SI=reg DI=mem
			Gen(L_REG, m, REG1);		// mov al,[ebx+reg]
			Gen(op, m);			// op  al,[edi]
			if (opc != CMPbtrm)
				Gen(S_REG, m, REG1);	// mov [ebx+reg],al		reg=reg op mem
			}
			break; 
/*01*/	case ADDwfrm:
/*03*/	case ADDwtrm:
/*09*/	case ORwfrm:
/*0b*/	case ORwtrm:
/*11*/	case ADCwfrm:
/*13*/	case ADCwtrm:
/*19*/	case SBBwfrm:
/*21*/	case ANDwfrm:
/*23*/	case ANDwtrm:
/*29*/	case SUBwfrm:
/*39*/	case CMPwfrm:
/*31*/	case XORwfrm:	// sorry for xor r,r but we have to set flags
/*33*/	case XORwtrm:
/*85*/	case TESTwrm: {
			int op = (opc==TESTwrm? O_AND_R:ArOpsR[D_MO(opc)]);
			PC += ModRM(PC, mode);	// SI=reg DI=mem
			Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
			Gen(op, mode, REG1);		// op  (e)ax,[ebx+reg]
			if ((opc!=CMPwfrm)&&(opc!=TESTwrm)) {
				if (opc & 2)
					Gen(S_REG, mode, REG1);	// mov [ebx+reg],(e)ax	reg=mem op reg
				else
					Gen(S_DI, mode);	// mov [edi],(e)ax	mem=mem op reg
			} }
			break; 
/*1b*/	case SBBwtrm:
/*2b*/	case SUBwtrm:
/*3b*/	case CMPwtrm: {
			int op = ArOpsM[D_MO(opc)];
			PC += ModRM(PC, mode);	// SI=reg DI=mem
			Gen(L_REG, mode, REG1);		// mov (e)ax,[ebx+reg]
			Gen(op, mode);			// op  (e)ax,[edi]
			if (opc != CMPwtrm)
				Gen(S_REG, mode, REG1);	// mov [ebx+reg],(e)ax	reg=reg op mem
			}
			break; 
/*a8*/	case TESTbi:
/*04*/	case ADDbia:
/*0c*/	case ORbi:
/*14*/	case ADCbi:
/*24*/	case ANDbi:
/*34*/	case XORbi: {
			int m = mode | MBYTE;
			int op = (opc==TESTbi? O_AND_R:ArOpsR[D_MO(opc)]);
			Gen(L_IMM_R1, m, Fetch(PC+1)); PC+=2;	// mov al,#imm
			Gen(op, m, Ofs_AL);			// op al,[ebx+reg]
			if (opc != TESTbi)
				Gen(S_REG, m, Ofs_AL);		// mov [ebx+reg],al
			}
			break; 
/*1c*/	case SBBbi:
/*2c*/	case SUBbi:
/*3c*/	case CMPbi: {
			int m = mode | MBYTE;
			int op = ArOpsM[D_MO(opc)];
			Gen(L_REG, m, Ofs_AL);			// mov al,[ebx+reg]
			Gen(op, m|IMMED, Fetch(PC+1)); PC+=2;	// op al,#imm
			if (opc != CMPbi)
				Gen(S_REG, m, Ofs_AL);		// mov [ebx+reg],al
			}
			break; 
/*a9*/	case TESTwi:
/*05*/	case ADDwia:
/*0d*/	case ORwi:
/*15*/	case ADCwi:
/*25*/	case ANDwi:
/*35*/	case XORwi: {
			int op = (opc==TESTwi? O_AND_R:ArOpsR[D_MO(opc)]);
			Gen(L_IMM_R1, mode|IMMED, DataFetchWL_U(mode,PC+1));
			INC_WL_PC(mode,1);
			Gen(op, mode, SEL_WL(mode, Ofs_EAX));
			if (opc != TESTwi)
				Gen(S_REG, mode, SEL_WL(mode,Ofs_EAX));
			}
			break; 
/*1d*/	case SBBwi:
/*2d*/	case SUBwi:
/*3d*/	case CMPwi: {
			int m = mode;
			int op = ArOpsM[D_MO(opc)];
			Gen(L_REG, m, SEL_WL(mode,Ofs_EAX));	// mov (e)ax,[ebx+reg]
			Gen(op, m|IMMED, DataFetchWL_U(mode,PC+1));	// op (e)ax,#imm
			INC_WL_PC(mode,1);
			if (opc != CMPwi)
				Gen(S_REG, m, SEL_WL(mode,Ofs_EAX));	// mov [ebx+reg],(e)ax
			}
			break; 
/*69*/	case IMULwrm:
			PC += ModRM(PC, mode);
			Gen(O_IMUL,mode|IMMED,DataFetchWL_S(mode,PC),REG1);
			INC_WL_PC(mode, 0);
			break;
/*6b*/	case IMULbrm:
			PC += ModRM(PC, mode);
			Gen(O_IMUL,mode|MBYTE|IMMED,(signed char)Fetch(PC),REG1);
			PC++;
			break;

/*9c*/	case PUSHF: {
			CODE_FLUSH();
			if (V86MODE() /*&& (IOPL<3)*/) {
				temp =  (TheCPU.veflags&~EFLAGS_VIP) |
					(vm86s.regs.eflags&EFLAGS_VIP);
				TheCPU.eflags &= ~EFLAGS_IF;
				TheCPU.veflags = temp;
				if (temp&EFLAGS_VIF)
					TheCPU.eflags |= EFLAGS_IF;
				if ((temp&EFLAGS_IF)&&(vm86s.vm86plus.force_return_for_pic))
					CEmuStat |= CeS_RPIC;
				temp =	(TheCPU.eflags & 0xeff) |	(temp & eTSSMASK);
			}
			else
				temp = TheCPU.eflags & ~(EFLAGS_RF|EFLAGS_VM);
			PUSH(mode, &temp);
			if (d.emu>1)
				e_printf("Pushed flags %08lx->%08lx\n",TheCPU.eflags,temp);
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
#if 0
/*d6*/	case 0xd6:    /* Undocumented */
			AL = (CARRYB & 1? 0xff:0x00);
			PC += 1;
			goto not_implemented;
/*62*/	case BOUND:		/* not implemented */
			goto not_implemented;
			PC++; break;
/*63*/	case ARPL:		/* not implemented */
			goto not_implemented;
			PC++; break;
#endif
/*d7*/	case XLAT:
			Gen(O_XLAT, mode); PC++; break;
/*98*/	case CBW:
			Gen(O_CBWD, mode|MBYTE); PC++; break;
/*99*/	case CWD:
			Gen(O_CBWD, mode); PC++; break;

/*07*/	case POPes:	if (REALADDR()) {
			    Gen(O_POP, mode, Ofs_ES);
			    AddrGen(A_SR_SH4, mode, Ofs_ES, Ofs_XES);
			} else { /* restartable */
			    unsigned short sv = 0;
			    SDTR tseg;
			    unsigned char big;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = SET_SEGREG(tseg, big, Ofs_ES, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.es = sv;
			    memcpy(&ES_DTR,&tseg,sizeof(SDTR));
			}
			PC++;
			break;
/*17*/	case POPss:	if (REALADDR()) {
			    Gen(O_POP, mode, Ofs_SS);
			    AddrGen(A_SR_SH4, mode, Ofs_SS, Ofs_XSS);
			} else { /* restartable */
			    unsigned short sv = 0;
			    SDTR tseg;
			    unsigned char big;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = SET_SEGREG(tseg, big, Ofs_SS, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.ss = sv;
			    memcpy(&SS_DTR,&tseg,sizeof(SDTR));
			    if (big) basemode &= ~STACK16; else basemode |= STACK16;
			}
			CEmuStat |= CeS_LOCK;
			PC++;
			break;
/*1f*/	case POPds:	if (REALADDR()) {
			    Gen(O_POP, mode, Ofs_DS);
			    AddrGen(A_SR_SH4, mode, Ofs_DS, Ofs_XDS);
			} else { /* restartable */
			    unsigned short sv = 0;
			    SDTR tseg;
			    unsigned char big;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = SET_SEGREG(tseg, big, Ofs_DS, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.ds = sv;
			    memcpy(&DS_DTR,&tseg,sizeof(SDTR));
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
			if (d.emu>4)
				e_printf("OPERoverride: new mode %04x\n",mode);
			PC++; goto override;
/*67*/	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
			mode = (mode & ~ADDR16) | (~basemode & ADDR16);
			if (d.emu>4)
				e_printf("ADDRoverride: new mode %04x\n",mode);
			PC++; goto override;

/*40*/	case INCax:
/*41*/	case INCcx:
/*42*/	case INCdx:
/*43*/	case INCbx:
/*44*/	case INCsp:
/*45*/	case INCbp:
/*46*/	case INCsi:
/*47*/	case INCdi:
			Gen(O_INC_R, mode, SEL_WL_X(mode,D_LO(opc))); PC++; break;
/*48*/	case DECax:
/*49*/	case DECcx:
/*4a*/	case DECdx:
/*4b*/	case DECbx:
/*4c*/	case DECsp:
/*4d*/	case DECbp:
/*4e*/	case DECsi:
/*4f*/	case DECdi:
			Gen(O_DEC_R, mode, SEL_WL_X(mode,D_LO(opc))); PC++; break;

/*06*/	case PUSHes:
			Gen(O_PUSH, mode, Ofs_ES); PC+=1; break;
/*0e*/	case PUSHcs:
			Gen(O_PUSH, mode, Ofs_CS); PC+=1; break;
/*16*/	case PUSHss:
			Gen(O_PUSH, mode, Ofs_SS); PC+=1; break;
/*1e*/	case PUSHds:
			Gen(O_PUSH, mode, Ofs_DS); PC+=1; break;
/*50*/	case PUSHax:
/*51*/	case PUSHcx:
/*52*/	case PUSHdx:
/*53*/	case PUSHbx:
/*54*/	case PUSHsp:
/*55*/	case PUSHbp:
/*56*/	case PUSHsi:
/*57*/	case PUSHdi: {
#if defined(OPTIMIZE_COMMON_SEQ) && !defined(SINGLESTEP)
			int m = mode;
			Gen(O_PUSH1, m);	// tests STACK16
			do {
				Gen(O_PUSH2, m, SEL_WL_X(m,D_LO(Fetch(PC))));
				m = UNPREFIX(m);
				PC++;
			} while ((Fetch(PC)&0xf8)==0x50);
			Gen(O_PUSH3, m);	// tests STACK16
#else
			Gen(O_PUSH, mode, SEL_WL_X(mode,D_LO(opc))); PC++;
#endif
			} break;
/*68*/	case PUSHwi:
			Gen(O_PUSHI, mode, DataFetchWL_U(mode,(PC+1))); 
			INC_WL_PC(mode,1);
			break;
/*6a*/	case PUSHbi:
			Gen(O_PUSHI, mode, (signed char)Fetch(PC+1)); PC+=2; break;
/*60*/	case PUSHA:
			Gen(O_PUSHA, mode); PC++; break;
/*61*/	case POPA:
			Gen(O_POPA, mode); PC++; break;

/*58*/	case POPax:
/*59*/	case POPcx:
/*5a*/	case POPdx:
/*5b*/	case POPbx:
/*5c*/	case POPsp:
/*5d*/	case POPbp:
/*5e*/	case POPsi:
/*5f*/	case POPdi: {
#if defined(OPTIMIZE_COMMON_SEQ) && !defined(SINGLESTEP)
			int m = mode;
			Gen(O_POP1, m);
			do {
				Gen(O_POP2, m, SEL_WL_X(m,D_LO(Fetch(PC))));
				m = UNPREFIX(m);
				PC++;
			} while ((Fetch(PC)&0xf8)==0x58);
			Gen(O_POP3, m);
#else
			Gen(O_POP, mode, SEL_WL_X(mode,D_LO(opc))); PC++;
#endif
			} break;
/*8f*/	case POPrm:
			// pop data into temporary storage and adjust esp
			Gen(O_POP, mode|MEMADR);
			// now calculate address. This way when using %esp
			//	as index we use the value AFTER the pop
			PC += ModRM(PC, mode);
			// store data
			Gen(S_DI, mode);
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
/*eb*/	case JMPsid:    {
			  unsigned char *p2 = JumpGen(PC, &mode, (opc-JO), 0);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;

/*e0*/	case LOOPNZ_LOOPNE: {
			  unsigned char *p2 = JumpGen(PC, &mode, 0x15, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*e1*/	case LOOPZ_LOOPE: {
			  unsigned char *p2 = JumpGen(PC, &mode, 0x14, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*e2*/	case LOOP:	{
			  unsigned char *p2 = JumpGen(PC, &mode, 0x10, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*e3*/	case JCXZ:	{
			  unsigned char *p2 = JumpGen(PC, &mode, 0x21, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;

/*82*/	case IMMEDbrm2:		// add mem8,signed imm8: no AND,OR,XOR
/*80*/	case IMMEDbrm: {
			int m = mode | MBYTE;
			int op = ArOpsR[D_MO(Fetch(PC+1))];
			PC += ModRM(PC, m);	// DI=mem
			Gen(L_DI_R1, m);		// mov al,[edi]
			Gen(op, m|IMMED, Fetch(PC));	// op al,#imm
			if (op!=O_CMP_R)
				Gen(S_DI, m);		// mov [edi],al		mem=mem op #imm
			PC++; }
			break;
/*81*/	case IMMEDwrm: {
			int op = ArOpsR[D_MO(Fetch(PC+1))];
			PC += ModRM(PC, mode);	// DI=mem
			Gen(L_DI_R1, mode);		// mov ax,[edi]
			Gen(op, mode|IMMED, DataFetchWL_U(mode,PC)); // op ax,#imm
			if (op!=O_CMP_R)
				Gen(S_DI, mode);	// mov [edi],ax		mem=mem op #imm
			INC_WL_PC(mode,0);
			}
			break;
/*83*/	case IMMEDisrm: {
			int op = ArOpsR[D_MO(Fetch(PC+1))];
			long v;
			PC += ModRM(PC, mode);	// DI=mem
			v = (signed char)Fetch(PC);
			Gen(L_DI_R1, mode);		// mov ax,[edi]
			Gen(op, mode|IMMED, v);		// op ax,#imm
			if (op != O_CMP_R)
				Gen(S_DI, mode);	// mov [edi],ax		mem=mem op #imm
			PC++; }
			break;
/*86*/	case XCHGbrm:
			PC += ModRM(PC, mode|MBYTE);	// EDI=mem
			Gen(O_XCHG, mode|MBYTE, REG1);
			break;
/*87*/	case XCHGwrm:
			PC += ModRM(PC, mode);			// EDI=mem
			Gen(O_XCHG, mode, REG1);
			break;
/*88*/	case MOVbfrm:
			PC += ModRM(PC, mode|MBYTE);	// mem=reg
			Gen(L_REG, mode|MBYTE, REG1);
			Gen(S_DI, mode|MBYTE);
			break; 
/*89*/	case MOVwfrm:
			PC += ModRM(PC, mode);
			Gen(L_REG, mode, REG1);
			Gen(S_DI, mode);
			break; 
/*8a*/	case MOVbtrm:
			PC += ModRM(PC, mode|MBYTE);	// reg=mem
			Gen(L_DI_R1, mode|MBYTE);
			Gen(S_REG, mode|MBYTE, REG1);
			break; 
/*8b*/	case MOVwtrm:
			PC += ModRM(PC, mode);
			Gen(L_DI_R1, mode);
			Gen(S_REG, mode, REG1);
			break; 
/*8c*/	case MOVsrtrm:
			PC += ModRM(PC, mode|SEGREG);
			Gen(L_REG, mode|DATA16, REG1);
			if (REG1==Ofs_SS) CEmuStat |= CeS_LOCK;
			//Gen(L_ZXAX, mode);
			Gen(S_DI, mode|DATA16);
			break; 
/*8d*/	case LEA:
			PC += ModRM(PC, mode|MLEA);
			Gen(S_DI_R, mode, REG1);
			break; 

/*c4*/	case LES:
			if (REALADDR()) {
			    PC += ModRM(PC, mode);
			    Gen(L_LXS1, mode, REG1);
			    Gen(L_LXS2, mode, Ofs_ES, Ofs_XES);
			}
			else {
			    unsigned short sv = 0;
			    unsigned long rv;
			    SDTR tseg;
			    unsigned char big;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode);
			    rv = DataGetWL_U(mode,TheCPU.mem_ref);
			    TheCPU.mem_ref += (mode&DATA16? 2:4);
			    sv = GetDWord(TheCPU.mem_ref);
			    TheCPU.err = SET_SEGREG(tseg, big, Ofs_ES, sv);
			    if (TheCPU.err) return P0;
			    SetCPU_WL(mode, REG1, rv);
			    TheCPU.es = sv;
			    memcpy(&ES_DTR,&tseg,sizeof(SDTR));
			}
			break;
/*c5*/	case LDS:
			if (REALADDR()) {
			    PC += ModRM(PC, mode);
			    Gen(L_LXS1, mode, REG1);
			    Gen(L_LXS2, mode, Ofs_DS, Ofs_XDS);
			}
			else {
			    unsigned short sv = 0;
			    unsigned long rv;
			    SDTR tseg;
			    unsigned char big;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode);
			    rv = DataGetWL_U(mode,TheCPU.mem_ref);
			    TheCPU.mem_ref += (mode&DATA16? 2:4);
			    sv = GetDWord(TheCPU.mem_ref);
			    TheCPU.err = SET_SEGREG(tseg, big, Ofs_DS, sv);
			    if (TheCPU.err) return P0;
			    SetCPU_WL(mode, REG1, rv);
			    TheCPU.ds = sv;
			    memcpy(&DS_DTR,&tseg,sizeof(SDTR));
			}
			break;
/*8e*/	case MOVsrfrm:
			if (REALADDR()) {
			    PC += ModRM(PC, mode|SEGREG);
			    Gen(L_DI_R1, mode|DATA16);
			    Gen(S_REG, mode|DATA16, REG1);
			    AddrGen(A_SR_SH4, mode, REG1, SBASE);
			}
			else {
			    unsigned short sv = 0;
			    SDTR tseg, *chp;
			    unsigned char big;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode|SEGREG);
			    sv = GetDWord(TheCPU.mem_ref);
			    TheCPU.err = SET_SEGREG(tseg, big, REG1, sv);
			    if (TheCPU.err) return P0;
			    switch (REG1) {
				case Ofs_DS: TheCPU.ds=sv; chp=&DS_DTR; break;
				case Ofs_SS: TheCPU.ss=sv; chp=&SS_DTR;
				    if (big) basemode &= ~STACK16; else basemode |= STACK16;
				    CEmuStat |= CeS_LOCK;
				    break;
				case Ofs_ES: TheCPU.es=sv; chp=&ES_DTR; break;
				case Ofs_FS: TheCPU.fs=sv; chp=&FS_DTR; break;
				case Ofs_GS: TheCPU.gs=sv; chp=&GS_DTR; break;
				default: goto illegal_op;
			    }
			    memcpy(chp,&tseg,sizeof(SDTR));
			}
			break; 

/*9b*/	case oWAIT:
/*90*/	case NOP:
			/* sorry but it's too dangerous to compile NOPs,
			 * because of self-modifying code (like Sourcer) and
			 * other things. Better keep it as a sync point */
			CODE_FLUSH(); PC++;
			while (Fetch(PC) == NOP) PC++;
			break;
/*91*/	case XCHGcx:
/*92*/	case XCHGdx:
/*93*/	case XCHGbx:
/*94*/	case XCHGsp:
/*95*/	case XCHGbp:
/*96*/	case XCHGsi:
/*97*/	case XCHGdi:
			Gen(O_XCHG_R, mode, SEL_WL(mode,Ofs_EAX), SEL_WL_X(mode,D_LO(opc))); 
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
			Gen(S_REG, mode, SEL_WL(mode,Ofs_EAX));
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
			Gen(L_REG, mode, SEL_WL(mode,Ofs_EAX));
			Gen(S_DI, mode);
			INC_WL_PCA(mode,1);
			break;

/*a4*/	case MOVSb:
			Gen(O_MOVS_SetA, mode|MBYTE);
			Gen(O_MOVS_MovD, mode|MBYTE);
			Gen(O_MOVS_SavA, mode|MBYTE);
			PC++; break;
/*a5*/	case MOVSw: {	int m = mode;
			Gen(O_MOVS_SetA, m);
#if defined(OPTIMIZE_COMMON_SEQ) && !defined(SINGLESTEP)
			/* optimize common sequence MOVSw..MOVSw..MOVSb */
			do {
				Gen(O_MOVS_MovD, m);
				m = UNPREFIX(m);
				PC++;
			} while (Fetch(PC) == MOVSw);
			if (Fetch(PC) == MOVSb) {
				Gen(O_MOVS_MovD, m|MBYTE);
				PC++;
			}
#else
			Gen(O_MOVS_MovD, m); PC++;
#endif
			Gen(O_MOVS_SavA, m);
			} break;
/*a6*/	case CMPSb:
			Gen(O_MOVS_SetA, mode|MBYTE);
			Gen(L_REG, mode|MBYTE, Ofs_AL);
			Gen(O_MOVS_CmpD, mode|MBYTE);
			Gen(O_MOVS_SavA, mode|MBYTE);
			PC++; break;
/*a7*/	case CMPSw:
			Gen(O_MOVS_SetA, mode);
			Gen(L_REG, mode, SEL_WL(mode,Ofs_EAX));
			Gen(O_MOVS_CmpD, mode);
			Gen(O_MOVS_SavA, mode);
			PC++; break;
/*aa*/	case STOSb:
			Gen(O_MOVS_SetA, mode|MBYTE);
			Gen(L_REG, mode|MBYTE, Ofs_AL);
			Gen(O_MOVS_StoD, mode|MBYTE);
			Gen(O_MOVS_SavA, mode|MBYTE);
			PC++; break;
/*ab*/	case STOSw: {	int m = mode;
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, mode, SEL_WL(m,Ofs_EAX));
#if defined(OPTIMIZE_COMMON_SEQ) && !defined(SINGLESTEP)
			do {
				Gen(O_MOVS_StoD, m);
				m = UNPREFIX(m);
				PC++;
			} while (Fetch(PC) == STOSw);
#else
			Gen(O_MOVS_StoD, m); PC++;
#endif
			Gen(O_MOVS_SavA, m);
			} break;
/*ac*/	case LODSb:
			Gen(O_MOVS_SetA, mode|MBYTE);
			Gen(O_MOVS_LodD, mode|MBYTE);
			Gen(S_REG, mode|MBYTE, Ofs_AL); PC++;
#if defined(OPTIMIZE_COMMON_SEQ) && !defined(SINGLESTEP)
			/* optimize common sequence LODSb-STOSb */
			if (Fetch(PC) == STOSb) {
				Gen(O_MOVS_StoD, mode|MBYTE);
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, mode|MBYTE);
			break;
/*ad*/	case LODSw:
			Gen(O_MOVS_SetA, mode);
			Gen(O_MOVS_LodD, mode);
			Gen(S_REG, mode, SEL_WL(mode,Ofs_EAX)); PC++;
#if defined(OPTIMIZE_COMMON_SEQ) && !defined(SINGLESTEP)
			/* optimize common sequence LODSw-STOSw */
			if (Fetch(PC) == STOSw) {
				Gen(O_MOVS_StoD, mode);
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, mode);
			break;
/*ae*/	case SCASb:
			Gen(O_MOVS_SetA, mode|MBYTE);
			Gen(L_REG, mode|MBYTE, Ofs_AL);
			Gen(O_MOVS_ScaD, mode|MBYTE);
			Gen(O_MOVS_SavA, mode|MBYTE);
			PC++; break;
/*af*/	case SCASw:
			Gen(O_MOVS_SetA, mode);
			Gen(L_REG, mode, SEL_WL(mode,Ofs_EAX));
			Gen(O_MOVS_ScaD, mode);
			Gen(O_MOVS_SavA, mode);
			PC++; break;

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
			Gen(L_IMM, mode, SEL_WL_X(mode,D_LO(opc)), DataFetchWL_U(mode,PC+1));
			INC_WL_PC(mode, 1); break;

/*d0*/	case SHIFTb:
/*d2*/	case SHIFTbv:
/*c0*/	case SHIFTbi: {
			int m = mode | MBYTE;
			unsigned char count = 0;
			PC += ModRM(PC, m);
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
				if (opc==SHIFTbv) goto illegal_op;
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
		}
		break;
/*d1*/	case SHIFTw:
/*d3*/	case SHIFTwv:
/*c1*/	case SHIFTwi: {
			int m = mode;
			unsigned char count = 0;
			PC += ModRM(PC, m);
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
				if ((opc==SHIFTw)||(opc==SHIFTwv)) goto illegal_op;
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
		}
		break;

/*e9*/	case JMPd: 
/*e8*/	case CALLd: {
			long dp;
			int dsp;
			CODE_FLUSH();
			dp = AddrFetchWL_S(mode,PC+1);
			dsp = (mode&ADDR16? 3:5);
			TheCPU.eip = (long)PC - LONG_CS + dsp;
			if (opc==CALLd) { 
				PUSH(mode, &TheCPU.eip);
				if (d.emu>2) e_printf("CALL: ret=%08lx\n",TheCPU.eip);
			}
			TheCPU.eip += dp;
			if (mode&ADDR16) TheCPU.eip &= 0xffff;
			PC = (unsigned char *)(LONG_CS + TheCPU.eip); }
			break;

/*9a*/	case CALLl:
/*ea*/	case JMPld: {
			unsigned short jcs,ocs;
			unsigned long oip,xcs,jip=0;
			CODE_FLUSH();
			/* get new cs:ip */
			jip = AddrFetchWL_U(mode, PC+1);
			INC_WL_PCA(mode,1);
			jcs = AddrFetchWL_U(mode, PC);
			INC_WL_PCA(mode,0);
			/* check if new cs is valid, save old for error */
			ocs = TheCPU.cs;
			xcs = LONG_CS;
			TheCPU.err = MAKESEG(mode, Ofs_CS, jcs);
			if (TheCPU.err) {
				TheCPU.cs = ocs; LONG_CS = xcs;	// should not change
				return P0;
			}
			if (opc==CALLl) {
				/* ok, now push old cs:eip */
				oip = (long)PC - xcs;
				PUSH(mode, &ocs);
				PUSH(mode, &oip);
				if (d.emu>2)
					e_printf("CALL_FAR: ret=%04x:%08lx\n  calling:      %04x:%08lx\n",
						ocs,oip,jcs,jip);
			}
			else {
				if (d.emu>2)
					e_printf("JMP_FAR: %04x:%08lx\n",jcs,jip);
			}
			TheCPU.eip = jip;
			PC = (unsigned char *)(LONG_CS + jip);
#ifdef SKIP_EMU_VBIOS
			if ((jcs&0xf000)==config.vbios_seg) {
				/* return the new PC after the jump */
				TheCPU.err = EXCP_GOBACK; return PC;
			}
#endif
			}
			break;

/*c2*/	case RETisp: {
			int dr;
			CODE_FLUSH();
			dr = (signed short)FetchW(PC+1);
			POP(mode, &TheCPU.eip);
			if (d.emu>2)
				e_printf("RET: ret=%08lx inc_sp=%d\n",TheCPU.eip,dr);
			if (mode&STACK16) rSP+=dr; else rESP+=dr;
			PC = (unsigned char *)(LONG_CS + TheCPU.eip); }
			break;
/*c3*/	case RET:
			CODE_FLUSH();
			POP(mode, &TheCPU.eip);
			if (d.emu>2) e_printf("RET: ret=%08lx\n",TheCPU.eip);
			PC = (unsigned char *)(LONG_CS + TheCPU.eip);
			break;
/*c6*/	case MOVbirm:
			PC += ModRM(PC, mode|MBYTE);
			Gen(L_XREG1, mode, Fetch(PC));
			Gen(S_DI, mode|MBYTE);
			PC++; break;
/*c7*/	case MOVwirm:
			PC += ModRM(PC, mode);
			Gen(L_XREG1, mode, DataFetchWL_U(mode,PC));
			Gen(S_DI, mode);
			INC_WL_PC(mode,0);
			break;
/*c8*/	case ENTER: {
			unsigned long sp, bp, frm;
			int level, ds;
			CODE_FLUSH();
			level = Fetch(PC+3) & 0x1f;
			ds = (mode&DATA16? 2:4);
			if (mode&STACK16) {
				sp = LONG_SS + rSP - ds;
				bp = LONG_SS + rBP;
			}
			else {
				sp = LONG_SS + rESP - ds;
				bp = LONG_SS + rEBP;
			}
			PUSH(mode, &rEBP);
			frm = sp - LONG_SS;
			if (level) {
				sp -= (ds + ds*level);
				while (level--) {
					unsigned long tmp;
					bp -= ds;
					tmp = bp - LONG_SS;
					PUSH(mode, &tmp);
				}
				PUSH(mode, &frm);
			}
			if (mode&DATA16) rBP = frm; else rEBP = frm;
			sp -= FetchW(PC+1);
			if (mode&STACK16)
				rSP = sp - LONG_SS;
			else
				rESP = sp - LONG_SS;
			PC += 4; }
			break;
/*c9*/	case LEAVE: {
			unsigned long bp;
			CODE_FLUSH();
			if (mode&DATA16) rSP = rBP; else rESP = rEBP;
			POP(mode, &bp);
			if (mode&DATA16) rBP = bp; else rEBP = bp;
			PC++; }
			break;
/*ca*/	case RETlisp: { /* restartable */
			unsigned long dr, sv=0;
			CODE_FLUSH();
			NOS_WORD(mode, &sv);
			dr = AddrFetchWL_U(mode,PC+1);
			TheCPU.err = MAKESEG(mode, Ofs_CS, sv);
			if (TheCPU.err) return P0;
			TheCPU.eip=0; POP(mode, &TheCPU.eip);
			POP_ONLY(mode);
			if (d.emu>2)
				e_printf("RET_%ld: ret=%08lx\n",dr,TheCPU.eip);
			PC = (unsigned char *)(LONG_CS + TheCPU.eip);
			if (mode&STACK16) rSP+=dr; else rESP+=dr; }
			break;
/*cc*/	case INT3:
			CODE_FLUSH();
			e_printf("Interrupt 03\n");
			TheCPU.err=EXCP03_INT3; PC++;
			return PC;
/*ce*/	case INTO:
			CODE_FLUSH();
			e_printf("Overflow interrupt 04\n");
			TheCPU.err=EXCP04_INTO;
			return P0;
/*cd*/	case INT:
			CODE_FLUSH();
			goto not_permitted;
			break;

/*cb*/	case RETl:
/*cf*/	case IRET: {	/* restartable */
			long sv=0;
			int m = mode;
			CODE_FLUSH();
			NOS_WORD(m, &sv);	/* get segment */
			TheCPU.err = MAKESEG(m, Ofs_CS, sv);
			if (TheCPU.err) return P0;
			TheCPU.eip=0; POP(m, &TheCPU.eip);
			POP_ONLY(m);
			PC = (unsigned char *)(LONG_CS + TheCPU.eip);
			if (opc==RETl) {
			    if (d.emu>1)
				e_printf("RET_FAR: ret=%04lx:%08lx\n",sv,TheCPU.eip);
			    break;	/* un-fall */
			}
			if (d.emu>1) {
				e_printf("IRET: ret=%04lx:%08lx\n",sv,TheCPU.eip);
			}
			POP(m, &temp); }
			goto popf001;	// hmm, really?
/*9d*/	case POPF: {
			CODE_FLUSH();
			POP(mode, &temp);
popf001:
			if (V86MODE() /*&& (IOPL<3)*/) {
			// IOPL<3: GPF
			// IOPL==3: VM,RF,IOPL,VIP,VIF unchaged
				TheCPU.veflags &= ~(eTSSMASK|EFLAGS_VIP|EFLAGS_VIF);
				TheCPU.veflags |= ((temp & eTSSMASK) |
					(vm86s.regs.eflags&EFLAGS_VIP));
				if ((TheCPU.veflags&EFLAGS_IF)&&(vm86s.vm86plus.force_return_for_pic))
					CEmuStat |= CeS_RPIC;
				if (temp&EFLAGS_IF) {
					TheCPU.veflags |= EFLAGS_VIF;
					if (TheCPU.veflags & EFLAGS_VIP)
						CEmuStat = (CEmuStat & ~CeS_RPIC) | CeS_STI;
				}
				TheCPU.eflags &= ~0xddf;
				TheCPU.eflags |= (temp & 0xfdf);
			}
			else {
			// Rmode or CPL==0: VIP=VIF=0, VM unchanged
			// CPL>0 && CPL<=IOPL: same, IOPL unchanged
			// CPL>=IOPL: IF changed
				if (mode & DATA16)
					TheCPU.eflags = (temp&0x7dd5) | (TheCPU.eflags&0x270200) | 2;
				else
					TheCPU.eflags = (temp&0x277dd5) | (TheCPU.eflags&0x200) | 2;
				dpmi_eflags = (dpmi_eflags&~EFLAGS_IF)|(temp&EFLAGS_IF);
			}
			if (d.emu>1)
				e_printf("Popped flags %08lx->{r=%08lx v=%08lx}\n",temp,TheCPU.eflags,TheCPU.veflags);
			if (opc==POPF) PC++; }
			break;

/*f2*/	case REPNE:
/*f3*/	case REP: /* also is REPE */ {
			unsigned char repop; int repmod;
			PC++;
			repmod = mode | (opc==REPNE? MREPNE:MREP);
repag0:
			repop = Fetch(PC);
			switch (repop) {
				case INSb:
				case INSw:
				case OUTSb:
				case OUTSw:
					goto not_implemented;
					PC++; break;
				case LODSb:
				case LODSw:
					PC++; break;		// not implemented
				case MOVSb:
					repmod |= MBYTE;
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_MovD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case MOVSw:
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_MovD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case CMPSb:
					repmod |= MBYTE;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case CMPSw:
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, SEL_WL(repmod,Ofs_EAX));
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case STOSb:
					repmod |= MBYTE;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					Gen(O_MOVS_StoD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case STOSw:
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, SEL_WL(repmod,Ofs_EAX));
					Gen(O_MOVS_StoD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case SCASb:
					repmod |= MBYTE;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					Gen(O_MOVS_ScaD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case SCASw:
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, SEL_WL(repmod,Ofs_EAX));
					Gen(O_MOVS_ScaD, repmod);
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
					if (d.emu>4)
					    e_printf("OPERoverride: new mode %04x\n",repmod);
					PC++; goto repag0;
				case ADDRoverride:
					repmod = (repmod & ~ADDR16) | (~basemode & ADDR16);
					if (d.emu>4)
					    e_printf("ADDRoverride: new mode %04x\n",repmod);
					PC++; goto repag0;
			} }
			break;
/*f4*/	case HLT:
			CODE_FLUSH();
			goto not_permitted;
/*f5*/	case CMC:
			if (IsCodeBufEmpty())
				EFLAGS ^= EFLAGS_CF;
			else
				Gen(O_SETFL, mode, CMC);
			PC++;
			break;
/*f6*/	case GRP1brm: {
			PC += ModRM(PC, mode|MBYTE);	// EDI=mem
			switch(REG1) {
			case Ofs_AL:	/*0*/	/* TEST */
			case Ofs_CL:	/*1*/	/* undocumented */
				Gen(L_DI_R1, mode|MBYTE);		// mov al,[edi]
				Gen(O_AND_R, mode|MBYTE|IMMED, Fetch(PC));	// op al,#imm
				PC++;
				break;
			case Ofs_DL:	/*2*/	/* NOT */
				Gen(O_NOT, mode|MBYTE);			// not [edi]
				break;
			case Ofs_BL:	/*3*/	/* NEG */
				Gen(O_NEG, mode|MBYTE);			// neg [edi]
				break;
			case Ofs_AH:	/*4*/	/* MUL AL */
				Gen(O_MUL, mode|MBYTE);			// al*[edi]->AX unsigned
				break;
			case Ofs_CH:	/*5*/	/* IMUL AL */
				Gen(O_IMUL, mode|MBYTE);		// al*[edi]->AX signed
				break;
			case Ofs_DH:	/*6*/	/* DIV AL */
				Gen(O_DIV, mode|MBYTE);			// ah:al/[edi]->AH:AL unsigned
				break;
			case Ofs_BH:	/*7*/	/* IDIV AL */
				Gen(O_IDIV, mode|MBYTE);		// ah:al/[edi]->AH:AL signed
				break;
			} }
			break;
/*f7*/	case GRP1wrm: {
			PC += ModRM(PC, mode);			// EDI=mem
			switch(REG1) {
			case Ofs_AX:	/*0*/	/* TEST */
			case Ofs_CX:	/*1*/	/* undocumented */
				Gen(L_DI_R1, mode);		// mov al,[edi]
				Gen(O_AND_R, mode|IMMED, DataFetchWL_U(mode,PC));	// op al,#imm
				INC_WL_PC(mode,0);
				break;
			case Ofs_DX:	/*2*/	/* NOT */
				Gen(O_NOT, mode);			// not [edi]
				break;
			case Ofs_BX:	/*3*/	/* NEG */
				Gen(O_NEG, mode);			// neg [edi]
				break;
			case Ofs_SP:	/*4*/	/* MUL AX */
				Gen(O_MUL, mode);			// (e)ax*[edi]->(E)DX:(E)AX unsigned
				break;
			case Ofs_BP:	/*5*/	/* IMUL AX */
				Gen(O_IMUL, mode);			// (e)ax*[edi]->(E)DX:(E)AX signed
				break;
			case Ofs_SI:	/*6*/	/* DIV AX+DX */
				Gen(O_DIV, mode);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX unsigned
				break;
			case Ofs_DI:	/*7*/	/* IDIV AX+DX */
				Gen(O_IDIV, mode);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX signed
				break;
			} }
			break;
/*f8*/	case CLC:
			if (IsCodeBufEmpty())
				EFLAGS &= ~EFLAGS_CF;
			else
				Gen(O_SETFL, mode, CLC);
			PC++;
			break;
/*f9*/	case STC:
			if (IsCodeBufEmpty())
				EFLAGS |= EFLAGS_CF;
			else
				Gen(O_SETFL, mode, STC);
			PC++;
			break;
/*fa*/	case CLI:
			CODE_FLUSH();
			if (REALMODE() || (CPL <= IOPL)) {
				EFLAGS &= ~EFLAGS_IF;
			}
			else if (V86MODE()) {
				if (d.emu>2) e_printf("Virtual CLI\n");
				TheCPU.veflags &= ~(EFLAGS_VIP|EFLAGS_VIF);
				TheCPU.veflags |= (vm86s.regs.eflags&EFLAGS_VIP);
				if ((TheCPU.veflags&EFLAGS_IF)&&(vm86s.vm86plus.force_return_for_pic))
					CEmuStat |= CeS_RPIC;
			}
			else
				goto not_permitted;
			PC++;
			break;
/*fb*/	case STI:
			CODE_FLUSH();
			if (REALMODE() || (CPL <= IOPL)) {
				EFLAGS |= EFLAGS_IF;
			}
			else if (V86MODE()) {
				if (d.emu>2) e_printf("Virtual STI\n");
				TheCPU.veflags &= ~EFLAGS_VIP;
				TheCPU.veflags |= (EFLAGS_VIF |
					(vm86s.regs.eflags&EFLAGS_VIP));
				if ((TheCPU.veflags&EFLAGS_IF)&&(vm86s.vm86plus.force_return_for_pic))
					CEmuStat |= CeS_RPIC;
				if (TheCPU.veflags & EFLAGS_VIP)
					CEmuStat = (CEmuStat & ~CeS_RPIC) | CeS_STI;
			}
			else
				goto not_permitted;
			PC++;
			break;
/*fc*/	case CLD:
			if (IsCodeBufEmpty())
				EFLAGS &= ~EFLAGS_DF;
			else
				Gen(O_SETFL, mode, CLD);
			PC++;
			break;
/*fd*/	case STD:
			if (IsCodeBufEmpty())
				EFLAGS |= EFLAGS_DF;
			else
				Gen(O_SETFL, mode, STD);
			PC++;
			break;
/*fe*/	case GRP2brm:	/* only INC and DEC are legal on bytes */
			PC += ModRM(PC, mode|MBYTE);	// EDI=mem
			switch(REG1) {
			case Ofs_AL:	/*0*/
				Gen(O_INC, mode|MBYTE); break;
			case Ofs_CL:	/*1*/
				Gen(O_DEC, mode|MBYTE); break;
			default: goto illegal_op;
			}
			break;
/*ff*/	case GRP2wrm:
			ModGetReg1(PC, mode);
			switch (REG1) {
			case Ofs_AX:	/*0*/
				PC += ModRM(PC, mode);
				Gen(O_INC, mode); break;
			case Ofs_CX:	/*1*/
				PC += ModRM(PC, mode);
				Gen(O_DEC, mode); break;
			case Ofs_SP:	/*4*/	 // JMP near indirect
			case Ofs_DX: {	/*2*/	 // CALL near indirect
					long dp;
					CODE_FLUSH();
					PC += ModRMSim(PC, mode);
					TheCPU.eip = (long)PC - LONG_CS;
					dp = DataGetWL_U(mode, TheCPU.mem_ref);
					if (REG1==Ofs_DX) { 
						PUSH(mode, &TheCPU.eip);
						if (d.emu>2)
							e_printf("CALL indirect: ret=%08lx\n      calling:     %08lx\n",
								TheCPU.eip,dp);
					}
					PC = (unsigned char *)(LONG_CS + dp);
				}
				break;
			case Ofs_BX:	/*3*/	 // CALL long indirect restartable
			case Ofs_BP: {	/*5*/	 // JMP long indirect restartable
					unsigned short ocs,jcs;
					unsigned long oip,xcs,jip=0;
					CODE_FLUSH();
					PC += ModRMSim(PC, mode);
					TheCPU.eip = (long)PC - LONG_CS;
					/* get new cs:ip */
					jip = DataGetWL_U(mode, TheCPU.mem_ref);
					jcs = GetDWord(TheCPU.mem_ref+(mode&DATA16? 2:4));
					/* check if new cs is valid, save old for error */
					ocs = TheCPU.cs;
					xcs = LONG_CS;
					TheCPU.err = MAKESEG(mode, Ofs_CS, jcs);
					if (TheCPU.err) {
						TheCPU.cs = ocs; LONG_CS = xcs;	// should not change
						return P0;
					}
					if (REG1==Ofs_BX) {
						/* ok, now push old cs:eip */
						oip = (long)PC - xcs;
						PUSH(mode, &ocs);
						PUSH(mode, &oip);
						if (d.emu>2)
							e_printf("CALL_FAR indirect: ret=%04x:%08lx\n          calling:      %04x:%08lx\n",
								ocs,oip,jcs,jip);
					}
					else {
						if (d.emu>2)
							e_printf("JMP_FAR indirect: %04x:%08lx\n",jcs,jip);
					}
					TheCPU.eip = jip;
					PC = (unsigned char *)(LONG_CS + jip);
#ifdef SKIP_EMU_VBIOS
					if ((jcs&0xf000)==config.vbios_seg) {
						/* return the new PC after the jump */
						TheCPU.err = EXCP_GOBACK; return PC;
					}
#endif
				}
				break;
			case Ofs_SI:	/*6*/	 // PUSH
				PC += ModRM(PC, mode);
				Gen(O_PUSH, mode|MEMADR); break;	// push [mem]
				break;
			default: goto illegal_op;
			}
			break;

/*6c*/	case INSb: {
			unsigned short a;
			unsigned long rd;
			CODE_FLUSH();
			a = rDX;
			rd = (mode&ADDR16? rDI:rEDI);
			if (test_ioperm(a))
				((char *)LONG_ES)[rd] = port_real_inb(a);
			else
				((char *)LONG_ES)[rd] = port_inb(a);
			if (EFLAGS & EFLAGS_DF) rd--; else rd++;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			PC++; } break;
/*ec*/	case INvb: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (test_ioperm(a)) rAL = port_real_inb(a);
				else rAL = port_inb(a);
			PC++; } break;
/*e4*/	case INb: {
			unsigned short a;
			CODE_FLUSH();
			E_TIME_STRETCH;		// for PIT
			a = Fetch(PC+1);
			if (test_ioperm(a)) rAL = port_real_inb(a);
				else rAL = port_inb(a);
			PC += 2; } break;
/*6d*/	case INSw:
/*ed*/	case INvw: {
			CODE_FLUSH();
			if (opc==INSw) {
				unsigned long rd = (mode&ADDR16? rDI:rEDI);
				void *p = (void *)(LONG_ES+rd);
				int dp;
				if (mode&DATA16) {
					*((short *)p) = port_inw(rDX); dp=2;
				}
				else {
					*((long *)p) = port_ind(rDX); dp=4;
				}
				if (EFLAGS & EFLAGS_DF) rd-=dp; else rd+=dp;
				if (mode&ADDR16) rDI=rd; else rEDI=rd;
			}
			else {
				if (mode&DATA16) rAX = port_inw(rDX);
				else rEAX = port_ind(rDX);
			}
			PC++; } break;
/*e5*/	case INw: {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if (mode&DATA16) rAX = port_inw(a); else rEAX = port_ind(a);
			PC += 2; } break;

/*6e*/	case OUTSb: {
			unsigned short a;
			unsigned long rd;
			CODE_FLUSH();
			a = rDX;
			rd = (mode&ADDR16? rDI:rEDI);
			if (test_ioperm(a))
				port_real_outb(a,((char *)LONG_ES)[rd]);
			else
				port_outb(a,((char *)LONG_ES)[rd]);
			if (EFLAGS & EFLAGS_DF) rd--; else rd++;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			PC++; } break;
/*ee*/	case OUTvb: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (test_ioperm(a)) port_real_outb(a,rAL);
				else port_outb(a,rAL);
			PC++; } break;
/*e6*/	case OUTb:  {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if (test_ioperm(a)) port_real_outb(a,rAL);
				else port_outb(a,rAL);
			PC += 2; } break;
/*6f*/	case OUTSw:	goto not_implemented;
/*ef*/	case OUTvw: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (test_ioperm(a)) {
			    if (mode&DATA16) port_real_outw(a,rAX); else port_real_outd(a,rEAX);
			}
			else {
			    if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
			}
			PC++; } break;

/*e7*/	case OUTw:  {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if (test_ioperm(a)) {
			    if (mode&DATA16) port_real_outw(a,rAX); else port_real_outd(a,rEAX);
			}
			else {
			    if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
			}
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
			if ((b&0xc0)==0xc0) {
				exop |= 0x40;
				if (exop==0x63) {
					CODE_FLUSH();
				}
				PC += 2;
			}
			else {
				if ((exop&0xeb)==0x21) {
					CODE_FLUSH();
					PC += ModRMSim(PC, mode|NOFLDR);
					b = mode;
				}
				else {
					PC += ModRM(PC, mode|NOFLDR);
				}
			}
			b &= 7;
			if (Fp87_op(exop, b)) {
				TheCPU.err = -96; return P0;
			}
			mode |= M_FPOP;
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
				case 1: /* STR */
				    /* Store Task Register */
				case 2: /* LLDT */
				    /* Load Local Descriptor Table Register */
				case 3: /* LTR */
				    /* Load Task Register */
				case 4: /* VERR */
				case 5: /* VERW */
				case 6: /* JMP indirect to IA64 code */
				case 7: /* Illegal */
				    goto illegal_op;
				} }
				break;
			case 0x01: { /* GRP7 - Extended Opcode 21 */
				unsigned char opm = D_MO(Fetch(PC+2));
				switch (opm) {
				case 0: /* SGDT */
				    /* Store Global Descriptor Table Register */
				case 1: /* SIDT */
				    /* Store Interrupt Descriptor Table Register */
				case 2: /* LGDT */ /* PM privileged AND real mode */
				    /* Load Global Descriptor Table Register */
				case 3: /* LIDT */ /* PM privileged AND real mode */
				    /* Load Interrupt Descriptor Table Register */
				case 4: /* SMSW, 80286 compatibility */
				    /* Store Machine Status Word */
				    PC++; PC += ModRM(PC, mode);
				    Gen(L_CR0, mode);
				    Gen(S_DI, mode|DATA16);
				    break;
				case 5: /* Illegal */
				case 6: /* LMSW, 80286 compatibility, Privileged */
				    /* Load Machine Status Word */
				case 7: /* Illegal */
				    goto illegal_op;
				} }
				break;

			case 0x02:   /* LAR */ /* Load Access Rights Byte */
			case 0x03: { /* LSL */ /* Load Segment Limit */
				unsigned short sv; int tmp;
				if (REALMODE()) goto illegal_op;
				CODE_FLUSH();
				PC += ModRMSim(PC+1, mode) + 1;
				sv = GetDWord(TheCPU.mem_ref);
				if (!e_larlsl(mode, sv)) {
				    EFLAGS &= ~EFLAGS_ZF;
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
				    SetCPU_WL(mode, REG1, tmp);
				} }
				break;

			/* case 0x04:	LOADALL */
			/* case 0x05:	LOADALL(286) - SYSCALL(K6) */
			case 0x06: /* CLTS */ /* Privileged */
				/* Clear Task State Register */
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
			case 0x0b: goto illegal_op;	/* UD2 */
			/* case 0x0d:	Code Extension 25(AMD-3D) */
			/* case 0x0e:	FEMMS(K6-3D) */
			case 0x0f:	/* AMD-3D */
			/* case 0x10-0x1f:	various V20/MMX instr. */
				goto not_implemented;
			case 0x20:   /* MOVcdrd */ /* Privileged */
			case 0x22:   /* MOVrdcd */ /* Privileged */
			case 0x21:   /* MOVddrd */ /* Privileged */
			case 0x23:   /* MOVrddd */ /* Privileged */
			case 0x24:   /* MOVtdrd */ /* Privileged */
			case 0x26: { /* MOVrdtd */ /* Privileged */
				long *srg; int reg; unsigned char b,opd;
				if (REALMODE()) goto not_permitted;
				CODE_FLUSH();
				b = Fetch(PC+2);
				if (D_HO(b)!=3) goto illegal_op;
				reg = D_MO(b); b = D_LO(b);
				if ((b==4)||(b==5)) goto illegal_op;
				srg = (long *)((char *)&TheCPU + R1Tab_l[b]);
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
			    goto not_implemented;

			case 0x31: /* RDTSC */
				Gen(O_RDTSC, mode);
				PC+=2; break;
			case 0x32: /* RDMSR */
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
			    goto not_implemented;
//
			case 0x80: /* JOimmdisp */
			case 0x81: /* JNOimmdisp */
			case 0x82: /* JBimmdisp */
			case 0x83: /* JNBimmdisp */
			case 0x84: /* JZimmdisp */
			case 0x85: /* JNZimmdisp */
			case 0x86: /* JBEimmdisp */
			case 0x87: /* JNBEimmdisp */
			case 0x88: /* JSimmdisp */
			case 0x89: /* JNSimmdisp */
			case 0x8a: /* JPimmdisp */
			case 0x8b: /* JNPimmdisp */
			case 0x8c: /* JLimmdisp */
			case 0x8d: /* JNLimmdisp */
			case 0x8e: /* JLEimmdisp */
			case 0x8f: /* JNLEimmdisp */
				{
				  unsigned char *p2 = JumpGen(PC, &mode, (opc2-0x80), 1);
				  if (TheCPU.err) return PC; else if (p2) PC = p2;
				}
				break;
///
			case 0x90: /* SETObrm */
			case 0x91: /* SETNObrm */
			case 0x92: /* SETBbrm */
			case 0x93: /* SETNBbrm */
			case 0x94: /* SETZbrm */
			case 0x95: /* SETNZbrm */
			case 0x96: /* SETBEbrm */
			case 0x97: /* SETNBEbrm */
			case 0x98: /* SETSbrm */
			case 0x99: /* SETNSbrm */
			case 0x9a: /* SETPbrm */
			case 0x9b: /* SETNPbrm */
			case 0x9c: /* SETLbrm */
			case 0x9d: /* SETNLbrm */
			case 0x9e: /* SETLEbrm */
			case 0x9f: /* SETNLEbrm */
				PC++; PC += ModRM(PC, mode|MBYTE);
				Gen(O_SETCC, mode, (opc2&0x0f));
				break;
///
			case 0xa0: /* PUSHfs */
				Gen(O_PUSH, mode, Ofs_FS); PC+=2;
				break;
			case 0xa1: /* POPfs */
				if (REALADDR()) {
				    Gen(O_POP, mode, Ofs_FS);
				    AddrGen(A_SR_SH4, mode, Ofs_FS, Ofs_XFS);
				} else { /* restartable */
				    unsigned short sv = 0;
				    SDTR tseg;
				    unsigned char big;
				    CODE_FLUSH();
				    TOS_WORD(mode, &sv);
				    TheCPU.err = SET_SEGREG(tseg, big, Ofs_FS, sv);
				    if (TheCPU.err) return P0;
				    POP_ONLY(mode);
				    TheCPU.fs = sv;
				    memcpy(&FS_DTR,&tseg,sizeof(SDTR));
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
			case 0xbc: /* BSF */
			case 0xbd: /* BSR */
				PC++; PC += ModRM(PC, mode);
				Gen(O_BITOP, mode, (opc2-0xa0), REG1);
				break;
			case 0xba: { /* GRP8 - Code Extension 22 */
				unsigned char opm = (Fetch(PC+2))&0x38;
				switch (opm) {
				case 0x00: /* Illegal */
				case 0x08: /* Illegal */
				case 0x10: /* Illegal */
				case 0x18: /* Illegal */
				    goto illegal_op;
				case 0x20: /* BT */
				case 0x28: /* BTS */
				case 0x30: /* BTR */
				case 0x38: /* BTC */
					PC++; PC += ModRM(PC, mode);
					Gen(O_BITOP, mode, opm, Fetch(PC));
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
				PC++; PC += ModRM(PC, mode);
				if (opc2&1) {
					Gen(O_SHFD, mode, (opc2&8), REG1);
				}
				else {
					Gen(O_SHFD, mode|IMMED, (opc2&8), Fetch(PC), REG1);
					PC++;
				}
				break;

			case 0xa6: /* CMPXCHGb */
			case 0xa7: /* CMPXCHGw */
			    goto not_implemented;
///
			case 0xa8: /* PUSHgs */
				Gen(O_PUSH, mode, Ofs_GS); PC+=2;
				break;
			case 0xa9: /* POPgs */
				if (REALADDR()) {
				    Gen(O_POP, mode, Ofs_GS);
				    AddrGen(A_SR_SH4, mode, Ofs_GS, Ofs_XGS);
				} else { /* restartable */
				    unsigned short sv = 0;
				    SDTR tseg;
				    unsigned char big;
				    CODE_FLUSH();
				    TOS_WORD(mode, &sv);
				    TheCPU.err = SET_SEGREG(tseg, big, Ofs_GS, sv);
				    if (TheCPU.err) return P0;
				    POP_ONLY(mode);
				    TheCPU.gs = sv;
				    memcpy(&GS_DTR,&tseg,sizeof(SDTR));
				}
				PC+=2;
				break;
///
			case 0xaa:
			    goto illegal_op;
			/* case 0xae:	Code Extension 24(MMX) */
			case 0xaf: /* IMULregrm */
				PC++; PC += ModRM(PC, mode);
				Gen(O_IMUL, mode|MEMADR, REG1);	// reg*[edi]->reg signed
				break;
			case 0xb0-0xb1:		/* CMPXCHG */
			    goto not_implemented;
///
			case 0xb2: /* LSS */
				if (REALADDR()) {
				    PC++; PC += ModRM(PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_SS, Ofs_XSS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    SDTR tseg;
				    unsigned char big;
				    CODE_FLUSH();
				    CEmuStat |= CeS_LOCK;
				    PC++; PC += ModRMSim(PC, mode);
				    rv = DataGetWL_U(mode,TheCPU.mem_ref);
				    TheCPU.mem_ref += (mode&DATA16? 2:4);
				    sv = GetDWord(TheCPU.mem_ref);
				    TheCPU.err = SET_SEGREG(tseg, big, Ofs_SS, sv);
				    if (TheCPU.err) return P0;
				    SetCPU_WL(mode, REG1, rv);
				    TheCPU.ss = sv;
				    if (big) basemode &= ~STACK16; else basemode |= STACK16;
				    memcpy(&SS_DTR,&tseg,sizeof(SDTR));
				}
				break;
			case 0xb4: /* LFS */
				if (REALADDR()) {
				    PC++; PC += ModRM(PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_FS, Ofs_XFS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    SDTR tseg;
				    unsigned char big;
				    CODE_FLUSH();
				    PC++; PC += ModRMSim(PC, mode);
				    rv = DataGetWL_U(mode,TheCPU.mem_ref);
				    TheCPU.mem_ref += (mode&DATA16? 2:4);
				    sv = GetDWord(TheCPU.mem_ref);
				    TheCPU.err = SET_SEGREG(tseg, big, Ofs_FS, sv);
				    if (TheCPU.err) return P0;
				    SetCPU_WL(mode, REG1, rv);
				    TheCPU.fs = sv;
				    memcpy(&FS_DTR,&tseg,sizeof(SDTR));
				}
				break;
			case 0xb5: /* LGS */
				if (REALADDR()) {
				    PC++; PC += ModRM(PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_GS, Ofs_XGS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    SDTR tseg;
				    unsigned char big;
				    CODE_FLUSH();
				    PC++; PC += ModRMSim(PC, mode);
				    rv = DataGetWL_U(mode,TheCPU.mem_ref);
				    TheCPU.mem_ref += (mode&DATA16? 2:4);
				    sv = GetDWord(TheCPU.mem_ref);
				    TheCPU.err = SET_SEGREG(tseg, big, Ofs_GS, sv);
				    if (TheCPU.err) return P0;
				    SetCPU_WL(mode, REG1, rv);
				    TheCPU.gs = sv;
				    memcpy(&GS_DTR,&tseg,sizeof(SDTR));
				}
				break;
			case 0xb6: /* MOVZXb */
				PC++; PC += ModRM(PC, mode|MBYTX);
				Gen(L_MOVZS, mode|MBYTX, 0, REG1);
				break;
			case 0xb7: /* MOVZXw */
				PC++; PC += ModRM(PC, mode);
				Gen(L_MOVZS, mode, 0, REG1);
				break;
			case 0xbe: /* MOVSXb */
				PC++; PC += ModRM(PC, mode|MBYTX);
				Gen(L_MOVZS, mode|MBYTX, 1, REG1);
				break;
			case 0xbf: /* MOVSXw */
				PC++; PC += ModRM(PC, mode);
				Gen(L_MOVZS, mode, 1, REG1);
				break;
///
			case 0xb8:	/* JMP absolute to IA64 code */
			case 0xb9:
			    goto illegal_op;	/* UD2 */
			case 0xc0: /* XADDb */
			case 0xc1: /* XADDw */
			/* case 0xc2-0xc6:	MMX */
			/* case 0xc7:	Code Extension 23 - 01=CMPXCHG8B mem */
			    goto not_implemented;

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
			    goto not_implemented;
///
			case 0xff: /* illegal */
				CloseAndExec(PC, mode); NewNode=0;
				if (TheCPU.err) return P0;
				goto illegal_op;
			default:
				goto not_implemented;
			} }
			break;

/*xx*/	default:
			goto not_implemented;
		}

		TheCPU.mode = mode | (CEmuStat & CeS_LOCK? 0: DSPSTK);

		if (NewNode) {
			int rc;
			NewIMeta(P0, mode, &rc, (void *)locJtgt);
			if (rc < 0) {	// metadata table full
				if (d.emu>2)
					e_printf("============ Tab full:closing sequence at %08lx\n",(long)(PC));
				PC = CloseAndExec(PC, mode);
				if (TheCPU.err) return PC;
				NewNode = 0;
			}
		}
		else {
			if (d.emu>3) e_printf("\n%s",e_print_regs());
			TheCPU.EMUtime += FAKE_INS_TIME;
		}
		if (d.emu>2) {
			e_printf("  %s\n", e_emu_disasm(P0,(~basemode&3)));
		}

		P0 = PC;
#ifdef SINGLESTEP
		CloseAndExec(P0, mode);
		NewNode = 0;
#endif
	}
	return 0;

not_implemented:
	dbug_printf("!!! Unimplemented %02x %02x %02x\n",opc,PC[1],PC[2]);
	TheCPU.err = -2; return PC;
//bad_override:
//	dbug_printf("!!! Bad override %02x\n",opc);
//	TheCPU.err = -3; return PC;
//bad_data:
//	dbug_printf("!!! Bad Data %02x\n",opc);
//	TheCPU.err = -4; return PC;
not_permitted:
	if (d.emu>1) e_printf("!!! Not permitted %02x\n",opc);
	TheCPU.err = EXCP0D_GPF; return PC;
//div_by_zero:
//	dbug_printf("!!! Div by 0 %02x\n",opc);
//	TheCPU.err = -6; return PC;
illegal_op:
	dbug_printf("!!! Illegal op %02x %02x %02x\n",opc,PC[1],PC[2]);
	TheCPU.err = EXCP06_ILLOP; return PC;
}

