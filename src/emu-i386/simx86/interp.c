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

#include <stddef.h>
#include <string.h>
#include "emu86.h"
#include "codegen-arch.h"
#include "port.h"
#include "dpmi.h"

unsigned char *P0;
#ifdef PROFILE
int EmuSignals = 0;
#endif

static int NewNode = 1;
static int basemode = 0;

static int ArOpsR[] =
	{ O_ADD_R, O_OR_R, O_ADC_R, O_SBB_R, O_AND_R, O_SUB_R, O_XOR_R, O_CMP_R };
static int ArOpsFR[] =
	{      -1,     -1,      -1, O_SBB_FR,     -1, O_SUB_FR,     -1, O_CMP_FR};
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
#define	CODE_FLUSH() 	if (CONFIG_CPUSIM || CurrIMeta>0) {\
			  unsigned char *P2 = CloseAndExec(P0, NULL, mode, __LINE__);\
			  if (TheCPU.err) return P2;\
			  if (!CONFIG_CPUSIM && P2 != P0) { PC=P2; continue; }\
			} NewNode=0

#define UNPREFIX(m)	((m)&~(DATA16|ADDR16))|(basemode&(DATA16|ADDR16))

#if 1	// VIF kernel patch
#define EFLAGS_IFK	(EFLAGS_IF|EFLAGS_VIF)
#else
#define EFLAGS_IFK	EFLAGS_IF
#endif

/////////////////////////////////////////////////////////////////////////////


static int MAKESEG(int mode, int ofs, unsigned short sv)
{
	SDTR tseg, *segc;
	int e;
	char big;

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
		if (big) basemode &= ~(ADDR16|DATA16);
		else basemode |= (ADDR16|DATA16);
		if (debug_level('e')>1) e_printf("MAKESEG CS: big=%d basemode=%04x\n",big&1,basemode);
	}
	if (ofs==Ofs_SS) {
		TheCPU.StackMask = (big? 0xffffffff : 0x0000ffff);
		if (debug_level('e')>1) e_printf("MAKESEG SS: big=%d basemode=%04x\n",big&1,basemode);
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
// jmp		b8 j j j j c3
//	link	e9 l l l l --
// jcc  	7x 06 b8 a a a a c3 b8 b b b b c3
//	link	7x 06 e9 l l l l -- e9 l l l l --
//

static unsigned char *JumpGen(unsigned char *P2, int mode, int cond,
			      int btype)
{
	unsigned char *P1;
	int taken=0, tailjmp=0;
	int dsp, cxv, gim, rc;
	int pskip;
	unsigned long d_t, d_nt, j_t, j_nt;

	/* pskip points to start of next instruction
	 * dsp is the displacement relative to this jump instruction,
	 * some cases:
	 *	eb 00	dsp=2	jmp to next inst (dsp==pskip)
	 *	eb ff	dsp=1	illegal or tricky
	 *	eb fe	dsp=0	loop forever
	 */
	if ((btype&5)==0) {	// short branch (byte)
		pskip = 2;
		dsp = pskip + (signed char)Fetch(P2+1);
	}
	else if ((btype&5)==4) {	// jmp (word/long)
		pskip = 1 + BT24(BitADDR16,mode);
		dsp = pskip + (int)AddrFetchWL_S(mode, P2+1);
	}
	else {		// long branch (word/long)
		pskip = 2 + BT24(BitADDR16,mode);
		dsp = pskip + (int)AddrFetchWL_S(mode, P2+2);
	}

	/* displacement for taken branch */
	d_t  = ((long)P2 - LONG_CS) + dsp;
	/* displacement for not taken branch */
	d_nt = ((long)P2 - LONG_CS) + pskip;
	if (mode&DATA16) { d_t &= 0xffff; d_nt &= 0xffff; }

	/* jump address for taken branch */
	j_t  = d_t  + LONG_CS;
	/* jump address for not taken branch, usually next instruction */
	j_nt = d_nt + LONG_CS;

#ifdef HOST_ARCH_X86
#if !defined(SINGLESTEP)&&!defined(SINGLEBLOCK)
	if (!UseLinker || CONFIG_CPUSIM || (EFLAGS & TF))
#endif
#endif
	    goto jgnolink;

	gim = 0;
	switch(cond) {
	case 0x00 ... 0x0f:
	case 0x31:
		P1 = P2 + pskip;
		/* is there a jump after the condition? if yes, simplify */
		if (Fetch(P1)==JMPsid) {	/* eb xx */
		    int dsp2 = (signed char)Fetch(P1+1) + 2;
	    	    if (dsp2 < 0) mode |= CKSIGN;
		    d_nt = ((long)P1 - LONG_CS) + dsp2;
		    if (mode&ADDR16) d_nt &= 0xffff;
		    j_nt = d_nt + LONG_CS;
	    	    e_printf("JMPs (%02x,%d) at %08lx after Jcc: t=%08lx nt=%08lx\n",
			P1[0],dsp2,(long)P1,j_t,j_nt);
		}
		else if (Fetch(P1)==JMPd) {	/* e9 xxxx{xxxx} */
		    int skp2 = BT24(BitADDR16,mode) + 1;
		    int dsp2 = skp2 + (int)AddrFetchWL_S(mode, P1+1);
	    	    if (dsp2 < 0) mode |= CKSIGN;
		    d_nt = ((long)P1 - LONG_CS) + dsp2;
		    if (mode&ADDR16) d_nt &= 0xffff;
		    j_nt = d_nt + LONG_CS;
	    	    e_printf("JMPl (%02x,%d) at %08lx after Jcc: t=%08lx nt=%08lx\n",
			P1[0],dsp2,(long)P1,j_t,j_nt);
		}

		/* backwards jump limited to 256 bytes */
		if ((dsp > -256) && (dsp < 0)) {
		    Gen(JB_LINK, mode, cond, P2, j_t, j_nt, &InstrMeta[0].clink);
		    gim = 1;
		}
		else if (dsp) {
		    /* forward jump or backward jump >=256 bytes */
		    if ((dsp < 0) || (dsp > pskip)) {
			Gen(JF_LINK, mode, cond, P2, j_t, j_nt, &InstrMeta[0].clink);
			gim = 1;
		    }
		    else if (dsp==pskip) {
			e_printf("### jmp %x 00\n",cond);
			TheCPU.mode |= SKIPOP;
			goto notakejmp;
		    }
		    else {	// dsp>0 && dsp<pskip: jumps in the jmp itself
			TheCPU.err = -103;
			dbug_printf("### err 103 jmp=%x dsp=%d pskip=%d\n",cond,dsp,pskip);
			return NULL;
		    }
		}
		else {	// dsp==0
		    /* strange but possible, very tight loop with an external
		     * condition changing a flag */
		    e_printf("### dsp=0 jmp=%x pskip=%d\n",cond,pskip);
		    break;
		}
		tailjmp=1;
		break;
	case 0x10:
		if (dsp==0) {	// eb fe
		    dbug_printf("!Forever loop!\n");
		    leavedos(0xebfe);
		}
#ifdef HOST_ARCH_X86
#ifdef NOJUMPS
		if (!CONFIG_CPUSIM &&
		    (((long)P2 ^ j_t) & PAGE_MASK)==0) {	// same page
		    e_printf("** JMP: ignored\n");
		    TheCPU.mode |= SKIPOP;
		    goto takejmp;
		}
#endif
#endif
	case 0x11:
		Gen(JMP_LINK, mode, cond, j_t, d_nt, &InstrMeta[0].clink);
		gim = 1;
		tailjmp=1;
		break;
	case 0x20: case 0x24: case 0x25:
		if (dsp == 0) {
		    	// ndiags: shorten delay loops (e2 fe)
			e_printf("### loop %x 0xfe\n",cond);
			TheCPU.mode |= SKIPOP;
			if (mode&ADDR16) rCX=0; else rECX=0;
			goto notakejmp;
		}
		else {
			Gen(JLOOP_LINK, mode, cond, j_t, j_nt, &InstrMeta[0].clink);
			gim = 1;
			tailjmp=1;
		}
		break;
	default: dbug_printf("JumpGen: unknown condition\n");
		break;
	}

	if (gim)
	    (void)NewIMeta(P2, mode, &rc);

jgnolink:
	/* we just generated a jump, so the returned eip (P1) is
	 * (almost) always different from P2.
	 */
	P1 = CloseAndExec(P2, NULL, mode, __LINE__); NewNode=0;
	if (TheCPU.err) return NULL;
	if (tailjmp) return P1;

	/* evaluate cond at RUNTIME after exec'ing */
	switch(cond) {
	case 0x00:
		if (CONFIG_CPUSIM) FlagSync_O();
		taken = IS_OF_SET; break;
	case 0x01:
		if (CONFIG_CPUSIM) FlagSync_O();
		taken = !IS_OF_SET; break;
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
		if (CONFIG_CPUSIM) FlagSync_AP();
		taken = IS_PF_SET; break;
	case 0x0b:
		e_printf("!!! JPclr\n");
		if (CONFIG_CPUSIM) FlagSync_AP();
		taken = !IS_PF_SET; break;
	case 0x0c:
		if (CONFIG_CPUSIM) FlagSync_O();
		taken = IS_SF_SET ^ IS_OF_SET; break;
	case 0x0d:
		if (CONFIG_CPUSIM) FlagSync_O();
		taken = !(IS_SF_SET ^ IS_OF_SET); break;
	case 0x0e:
		if (CONFIG_CPUSIM) FlagSync_O();
		taken = (IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET; break;
	case 0x0f:
		if (CONFIG_CPUSIM) FlagSync_O();
		taken = !(IS_SF_SET ^ IS_OF_SET) && !IS_ZF_SET; break;
	case 0x10: taken = 1; break;
	case 0x11: {
			PUSH(mode, &d_nt);
			if (debug_level('e')>2) e_printf("CALL: ret=%08lx\n",d_nt);
			taken = 1;
		   } break;
	case 0x20:	// LOOP
		   cxv = (mode&ADDR16? --rCX : --rECX);
		   taken = (cxv != 0); break;
	case 0x24:	// LOOPZ
		   cxv = (mode&ADDR16? --rCX : --rECX);
		   taken = (cxv != 0) && IS_ZF_SET; break;
	case 0x25:	// LOOPNZ
		   cxv = (mode&ADDR16? --rCX : --rECX);
		   taken = (cxv != 0) && !IS_ZF_SET; break;
	case 0x31:	// JCXZ
		   cxv = (mode&ADDR16? rCX : rECX);
		   taken = (cxv == 0); break;
	}
	NewNode = 0;
	if (taken) {
		if (dsp==0) {	// infinite loop
		    if ((cond&0xf0)==0x20) {	// loops
		    	// ndiags: shorten delay loops (e2 fe)
			if (mode&ADDR16) rCX=0; else rECX=0;
			return (unsigned char *)j_nt;
		    }
		    TheCPU.err = -103;
		    return NULL;
		}
		if (debug_level('e')>2) e_printf("** Jump taken to %08lx\n",(long)j_t);
#ifdef HOST_ARCH_X86
takejmp:
#endif
		TheCPU.eip = d_t;
		return (unsigned char *)j_t;
	}
notakejmp:
	TheCPU.eip = d_nt;
	return (unsigned char *)j_nt;
}


/////////////////////////////////////////////////////////////////////////////


unsigned char *Interp86(unsigned char *PC, int mod0)
{
	unsigned char opc;
	unsigned int temp;
	register int mode;
#ifdef HOST_ARCH_X86
	TNode *G;
#endif

	NewNode = 0;
	basemode = mod0;
	TheCPU.err = 0;
	CEmuStat &= ~CeS_TRAP;

	while (Running) {
		OVERR_DS = Ofs_XDS;
		OVERR_SS = Ofs_XSS;
		P0 = PC;	// P0 changes on instruction boundaries
		TheCPU.mode = mode = basemode;

		if (!NewNode) {
#if !defined(SINGLESTEP)&&!defined(SINGLEBLOCK)&&defined(HOST_ARCH_X86)
		    if (!CONFIG_CPUSIM && !(EFLAGS & TF)) {
			/* for a sequence to be found, it must begin with
			 * an allowable opcode. Look into table.
			 * NOTE - this while can loop forever and stop
			 * any signal processing. Jumps are defined as
			 * a 'descheduling point' for checking signals.
			 */
			temp = 100;	/* safety count */
			while (temp && ((InterOps[Fetch(PC)]&1)==0) && (G=FindTree((long)PC)) != NULL) {
				if (debug_level('e')>2)
					e_printf("** Found compiled code at %08lx\n",(long)(PC));
				if (CurrIMeta>0) {		// open code?
					if (debug_level('e')>2)
						e_printf("============ Closing open sequence at %08lx\n",(long)(PC));
					PC = CloseAndExec(PC, NULL, mode, __LINE__);
					if (TheCPU.err) return PC;
				}
				/* ---- this is the MAIN EXECUTE point ---- */
				{ int m = mode | (G->flags<<16) | XECFND;
				  unsigned char *tmp = CloseAndExec(NULL, G, m, __LINE__);
				  if (!tmp) goto bad_return;
				  P0 = PC = tmp; }
				if (TheCPU.err) return PC;
				/* on jumps, exit to process signals */
				if (InterOps[Fetch(PC)]&0x80) break;
				/* if all fails, stop infinite loops here */
				temp--;
			}
		    }
#endif
			if (CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND|CeS_LOCK|CeS_RPIC|CeS_STI)) {
#ifdef PROFILE
				EmuSignals++;
#endif
				if (CEmuStat & CeS_LOCK)
					CEmuStat &= ~CeS_LOCK;
				else {
					if (CEmuStat & CeS_TRAP) {
						/* force exit for single step trap */
						if (!TheCPU.err)
							TheCPU.err = EXCP01_SSTP;
						return PC;
					}
					//else if (CEmuStat & CeS_DRTRAP) {
					//	if (e_debug_check(PC)) {
					//		TheCPU.err = EXCP01_SSTP;
					//		return PC;
					//	}
					//}
					else if (CEmuStat & CeS_SIGPEND) {
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
				}
			}
			CEmuStat &= ~CeS_TRAP;
			if (EFLAGS & TF) {
				CEmuStat |= CeS_TRAP;
			}
		}
// ------ temp ------- debug ------------------------
		if ((PC==NULL)||(*((int *)PC)==0)) {
			set_debug_level('e',9);
			dbug_printf("\n%s\nFetch %08x at %p mode %x\n",
				e_print_regs(),*((int *)PC),PC,mode);
			TheCPU.err = -99; return PC;
		}
// ------ temp ------- debug ------------------------
		NewNode = 1;

override:
		switch ((opc=Fetch(PC))) {
/*28*/	case SUBbfrm:
/*30*/	case XORbfrm:
/*32*/	case XORbtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_CLEAR, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			} goto intop28;
/*08*/	case ORbfrm:
/*0a*/	case ORbtrm:
/*20*/	case ANDbfrm:
/*22*/	case ANDbtrm:
/*84*/	case TESTbrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_TEST, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			} goto intop28;
/*18*/	case SBBbfrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
/*00*/	case ADDbfrm:
/*02*/	case ADDbtrm:
/*10*/	case ADCbfrm:
/*12*/	case ADCbtrm:
/*38*/	case CMPbfrm:
intop28:		{ int m = mode | MBYTE;
			int op = (opc==TESTbrm? O_AND_R:ArOpsR[D_MO(opc)]);
			PC += ModRM(opc, PC, m);	// SI=reg DI=mem
			    Gen(L_DI_R1, m);		// mov al,[edi]
			    Gen(op, m, REG1);		// op  al,[ebx+reg]
			    if ((opc!=CMPbfrm)&&(opc!=TESTbrm)) {
				if (opc & 2)
					Gen(S_REG, m, REG1);	// mov [ebx+reg],al		reg=mem op reg
				else
					Gen(S_DI, m);		// mov [edi],al			mem=mem op reg
			    }
			}
			break; 
/*2a*/	case SUBbtrm:	if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(O_CLEAR, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			} goto intop3a;
/*1a*/	case SBBbtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, mode|MBYTE, R1Tab_b[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
/*3a*/	case CMPbtrm:
intop3a:		{ int m = mode | MBYTE;
			int op = ArOpsFR[D_MO(opc)];
			PC += ModRM(opc, PC, m);	// SI=reg DI=mem
			Gen(L_DI_R1, m);		// mov al,[edi]
			Gen(op, m, REG1);		// op [ebx+reg], al
			// reg=reg op mem
			}
			break; 
/*29*/	case SUBwfrm:
/*31*/	case XORwfrm:
/*33*/	case XORwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_CLEAR, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			} goto intop29;
/*09*/	case ORwfrm:
/*0b*/	case ORwtrm:
/*21*/	case ANDwfrm:
/*23*/	case ANDwtrm:
/*85*/	case TESTwrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_TEST, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			} goto intop29;
/*19*/	case SBBwfrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
/*01*/	case ADDwfrm:
/*03*/	case ADDwtrm:
/*11*/	case ADCwfrm:
/*13*/	case ADCwtrm:
/*39*/	case CMPwfrm:
intop29:		{ int op = (opc==TESTwrm? O_AND_R:ArOpsR[D_MO(opc)]);
			PC += ModRM(opc, PC, mode);	// SI=reg DI=mem
			    Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
			    Gen(op, mode, REG1);		// op  (e)ax,[ebx+reg]
			    if ((opc!=CMPwfrm)&&(opc!=TESTwrm)) {
				if (opc & 2)
					Gen(S_REG, mode, REG1);	// mov [ebx+reg],(e)ax	reg=mem op reg
				else
					Gen(S_DI, mode);	// mov [edi],(e)ax	mem=mem op reg
			    }
			}
			break; 
/*2b*/	case SUBwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(O_CLEAR, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			} goto intop3b;
/*1b*/	case SBBwtrm:	if (RmIsReg[Fetch(PC+1)]&2) {	// same reg
			    Gen(O_SBSELF, mode, R1Tab_l[Fetch(PC+1)&7]);
			    PC+=2; break;
			}
/*3b*/	case CMPwtrm:
intop3b:		{ int op = ArOpsFR[D_MO(opc)];
			PC += ModRM(opc, PC, mode);	// SI=reg DI=mem
			Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
			Gen(op, mode, REG1);		// op [ebx+reg], (e)ax
			// reg=reg op mem
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
				Gen(S_REG, m, Ofs_AL);	// mov [ebx+reg],al
			}
			break; 
/*1c*/	case SBBbi:
/*2c*/	case SUBbi:
/*3c*/	case CMPbi: {
			int m = mode | MBYTE;
			int op = ArOpsFR[D_MO(opc)];
			// op [ebx+Ofs_EAX],#imm
			Gen(op, m|IMMED, Fetch(PC+1)); PC+=2;
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
			Gen(op, mode, Ofs_EAX);
			if (opc != TESTwi)
				Gen(S_REG, mode, Ofs_EAX);
			}
			break; 
/*1d*/	case SBBwi:
/*2d*/	case SUBwi:
/*3d*/	case CMPwi: {
			int m = mode;
			int op = ArOpsFR[D_MO(opc)];
			// op [ebx+Ofs_EAX],#imm
			Gen(op, m|IMMED, DataFetchWL_U(mode,PC+1));
			INC_WL_PC(mode,1);
			}
			break; 
/*69*/	case IMULwrm:
			PC += ModRM(opc, PC, mode);
			Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
			Gen(O_IMUL,mode|IMMED,DataFetchWL_S(mode,PC),REG1);
			INC_WL_PC(mode, 0);
			break;
/*6b*/	case IMULbrm:
			PC += ModRM(opc, PC, mode);
			Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
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
			    temp = EFLAGS & 0xdff;
			    if (eVEFLAGS & VIF) temp |= EFLAGS_IF;
			    temp |= ((eVEFLAGS & (eTSSMASK|VIF)) |
				(vm86s.regs.eflags&VIP));  // this one FYI
			    PUSH(mode, &temp);
			    if (debug_level('e')>1)
				e_printf("Pushed flags %08x fl=%08x vf=%08x\n",
		    			temp,EFLAGS,eVEFLAGS);
checkpic:		    if (vm86s.vm86plus.force_return_for_pic &&
				    (eVEFLAGS & EFLAGS_IFK)) {
				if (debug_level('e')>1)
				    e_printf("Return for PIC fl=%08x vf=%08x\n",
		    			EFLAGS,eVEFLAGS);
				TheCPU.err=EXCP_PICSIGNAL;
				return PC+1;
		    	    }
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
/*62*/	case BOUND:
			CODE_FLUSH();
			goto not_implemented;
/*63*/	case ARPL:
			CODE_FLUSH();
			goto not_implemented;

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
			    Gen(O_POP, mode, Ofs_SS);
			    AddrGen(A_SR_SH4, mode, Ofs_SS, Ofs_XSS);
			} else { /* restartable */
			    unsigned short sv = 0;
			    CODE_FLUSH();
			    TOS_WORD(mode, &sv);
			    TheCPU.err = MAKESEG(mode, Ofs_SS, sv);
			    if (TheCPU.err) return P0;
			    POP_ONLY(mode);
			    TheCPU.ss = sv;
			}
			CEmuStat |= CeS_LOCK;
			PC++;
			break;
/*1f*/	case POPds:	if (REALADDR()) {
			    Gen(O_POP, mode, Ofs_DS);
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
/*57*/	case PUSHdi:	opc = OpIsPush[opc];	// 1..12, 0x66=13
#ifndef SINGLESTEP
			if (!(EFLAGS & TF))
			{ int m = mode;		// enter with prefix
			Gen(O_PUSH1, m);
			/* optimized multiple register push */
			do {
			    Gen(O_PUSH2, m, R1Tab_l[opc-1]);
			    PC++; opc = OpIsPush[Fetch(PC)];
			    m &= ~DATA16;
			    if (opc==13) {	// prefix 0x66
				m |= (~basemode & DATA16);
				if ((opc=OpIsPush[Fetch(PC+1)])!=0) PC++;
			    }
			    else {
				m |= (basemode & DATA16);
			    }
			} while (opc);
			Gen(O_PUSH3, m); break; }
#endif
			Gen(O_PUSH, mode, R1Tab_l[opc-1]); PC++;
			break;
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
/*5f*/	case POPdi:
#ifndef SINGLESTEP
			if (!(EFLAGS & TF)) {
			int m = mode;
			Gen(O_POP1, m);
			do {
				Gen(O_POP2, m, R1Tab_l[D_LO(Fetch(PC))]);
				m = UNPREFIX(m);
				PC++;
			} while ((Fetch(PC)&0xf8)==0x58);
			if (opc!=POPsp) Gen(O_POP3, m); break; }
#endif
			Gen(O_POP, mode, R1Tab_l[D_LO(opc)]); PC++;
			break;
/*8f*/	case POPrm:
			// pop data into temporary storage and adjust esp
			Gen(O_POP, mode|MEMADR);
			// now calculate address. This way when using %esp
			//	as index we use the value AFTER the pop
			PC += ModRM(opc, PC, mode);
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
/*7f*/	case JNLE_JG:   {
			  unsigned char *p2 = JumpGen(PC, mode, (opc-JO), 0);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*eb*/	case JMPsid:    {
			  unsigned char *p2 = JumpGen(PC, mode, 0x10, 0);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;

/*e0*/	case LOOPNZ_LOOPNE: {
			  unsigned char *p2 = JumpGen(PC, mode, 0x25, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*e1*/	case LOOPZ_LOOPE: {
			  unsigned char *p2 = JumpGen(PC, mode, 0x24, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*e2*/	case LOOP:	{
			  unsigned char *p2 = JumpGen(PC, mode, 0x20, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;
/*e3*/	case JCXZ:	{
			  unsigned char *p2 = JumpGen(PC, mode, 0x31, 2);
			  if (TheCPU.err) return PC; else if (p2) PC = p2;
			}
			break;

/*82*/	case IMMEDbrm2:		// add mem8,signed imm8: no AND,OR,XOR
/*80*/	case IMMEDbrm: {
			int m = mode | MBYTE;
			int op = ArOpsR[D_MO(Fetch(PC+1))];
			PC += ModRM(opc, PC, m);	// DI=mem
			Gen(L_DI_R1, m);		// mov al,[edi]
			Gen(op, m|IMMED, Fetch(PC));	// op al,#imm
			if (op!=O_CMP_R)
				Gen(S_DI, m);		// mov [edi],al	mem=mem op #imm
			PC++; }
			break;
/*81*/	case IMMEDwrm: {
			int op = ArOpsR[D_MO(Fetch(PC+1))];
			PC += ModRM(opc, PC, mode);	// DI=mem
			Gen(L_DI_R1, mode);		// mov ax,[edi]
			Gen(op, mode|IMMED, DataFetchWL_U(mode,PC)); // op ax,#imm
			if (op!=O_CMP_R)
				Gen(S_DI, mode);	// mov [edi],ax	mem=mem op #imm
			INC_WL_PC(mode,0);
			}
			break;
/*83*/	case IMMEDisrm: {
			int op = ArOpsR[D_MO(Fetch(PC+1))];
			long v;
			PC += ModRM(opc, PC, mode);	// DI=mem
			v = (signed char)Fetch(PC);
			Gen(L_DI_R1, mode);		// mov ax,[edi]
			Gen(op, mode|IMMED, v);		// op ax,#imm
			if (op != O_CMP_R)
				Gen(S_DI, mode);	// mov [edi],ax	mem=mem op #imm
			PC++; }
			break;
/*86*/	case XCHGbrm:
			if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(L_NOP, mode); PC+=2;
			}
			else {
			    PC += ModRM(opc, PC, mode|MBYTE);	// EDI=mem
			    Gen(L_DI_R1, mode|MBYTE);
			    Gen(O_XCHG, mode|MBYTE, REG1);
			    Gen(S_DI, mode|MBYTE);
			}
			break;
/*87*/	case XCHGwrm:
			if (RmIsReg[Fetch(PC+1)]&2) {
			    Gen(L_NOP, mode); PC+=2;
			}
			else {
			    PC += ModRM(opc, PC, mode);		// EDI=mem
			    Gen(L_DI_R1, mode);
			    Gen(O_XCHG, mode, REG1);
			    Gen(S_DI, mode);
			}
			break;
/*88*/	case MOVbfrm:
			if (ModGetReg1(PC, MBYTE)==3) {
			    Gen(L_REG2REG, MBYTE, REG1, REG3); PC+=2;
			} else {
			    PC += ModRM(opc, PC, mode|MBYTE);	// mem=reg
			    Gen(L_REG, mode|MBYTE, REG1);
			    Gen(S_DI, mode|MBYTE);
			}
			break; 
/*89*/	case MOVwfrm:
			if (ModGetReg1(PC, mode)==3) {
			    Gen(L_REG2REG, mode, REG1, REG3); PC+=2;
			} else {
			    PC += ModRM(opc, PC, mode);
			    Gen(L_REG, mode, REG1);
			    Gen(S_DI, mode);
			}
			break; 
/*8a*/	case MOVbtrm:
			if (ModGetReg1(PC, MBYTE)==3) {
			    Gen(L_REG2REG, MBYTE, REG3, REG1); PC+=2;
			} else {
			    PC += ModRM(opc, PC, mode|MBYTE);	// reg=mem
			    Gen(L_DI_R1, mode|MBYTE);
			    Gen(S_REG, mode|MBYTE, REG1);
			}
			break; 
/*8b*/	case MOVwtrm:
			if (ModGetReg1(PC, mode)==3) {
			    Gen(L_REG2REG, mode, REG3, REG1); PC+=2;
			} else {
			    PC += ModRM(opc, PC, mode);
			    Gen(L_DI_R1, mode);
			    Gen(S_REG, mode, REG1);
			}
			break; 
/*8c*/	case MOVsrtrm:
			PC += ModRM(opc, PC, mode|SEGREG);
			Gen(L_REG, mode|DATA16, REG1);
			if (REG1==Ofs_SS) CEmuStat |= CeS_LOCK;
			//Gen(L_ZXAX, mode);
			Gen(S_DI, mode|DATA16);
			break; 
/*8d*/	case LEA:
			PC += ModRM(opc, PC, mode|MLEA);
			Gen(S_DI_R, mode, REG1);
			break; 

/*c4*/	case LES:
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
			    PC += ModRM(opc, PC, mode|SEGREG);
			    Gen(L_DI_R1, mode|DATA16);
			    Gen(S_REG, mode|DATA16, REG1);
			    AddrGen(A_SR_SH4, mode, REG1, e_ofsseg[REG1>>2]);
			}
			else {
			    unsigned short sv = 0;
			    CODE_FLUSH();
			    PC += ModRMSim(PC, mode|SEGREG);
			    sv = GetDWord(TheCPU.mem_ref);
			    TheCPU.err = MAKESEG(mode, REG1, sv);
			    if (TheCPU.err) return P0;
			    switch (REG1) {
				case Ofs_DS: TheCPU.ds=sv; break;
				case Ofs_SS: TheCPU.ss=sv;
				    CEmuStat |= CeS_LOCK;
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
			Gen(O_MOVS_SetA, m);
			Gen(O_MOVS_MovD, m);
			Gen(O_MOVS_SavA, m);
			PC++;
			} break;
/*a5*/	case MOVSw: {	int m = mode|(MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m);
#ifndef SINGLESTEP
			/* optimize common sequence MOVSw..MOVSw..MOVSb */
			if (!(EFLAGS & TF)) {
				do {
					Gen(O_MOVS_MovD, m);
					m = UNPREFIX(m);
					PC++;
				} while (Fetch(PC) == MOVSw);
				if (Fetch(PC) == MOVSb) {
					Gen(O_MOVS_MovD, m|MBYTE);
					PC++;
				}
			} else
#endif
			{
				Gen(O_MOVS_MovD, m); PC++;
			}
			Gen(O_MOVS_SavA, m);
			} break;
/*a6*/	case CMPSb: {	int m = mode|(MBYTE|MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m);
			Gen(O_MOVS_CmpD, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*a7*/	case CMPSw: {	int m = mode|(MOVSSRC|MOVSDST);
			Gen(O_MOVS_SetA, m);
			Gen(O_MOVS_CmpD, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*aa*/	case STOSb: {	int m = mode|(MBYTE|MOVSDST);
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, m, Ofs_AL);
			Gen(O_MOVS_StoD, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*ab*/	case STOSw: {	int m = mode|MOVSDST;
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, m, Ofs_EAX);
#ifndef SINGLESTEP
			if (!(EFLAGS & TF)) do {
				Gen(O_MOVS_StoD, m);
				m = UNPREFIX(m);
				PC++;
			} while (Fetch(PC) == STOSw); else
#endif
			{
				Gen(O_MOVS_StoD, m); PC++;
			}
			Gen(O_MOVS_SavA, m);
			} break;
/*ac*/	case LODSb: {	int m = mode|(MBYTE|MOVSSRC);
			Gen(O_MOVS_SetA, m);
			Gen(O_MOVS_LodD, m);
			Gen(S_REG, m, Ofs_AL); PC++;
#ifndef SINGLESTEP
			/* optimize common sequence LODSb-STOSb */
			if (!(EFLAGS & TF) && Fetch(PC) == STOSb) {
				Gen(O_MOVS_SetA, (m&ADDR16)|MOVSDST);
				Gen(O_MOVS_StoD, m);
				m |= MOVSDST;
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, m);
			} break;
/*ad*/	case LODSw: {	int m = mode|MOVSSRC;
			Gen(O_MOVS_SetA, m);
			Gen(O_MOVS_LodD, m);
			Gen(S_REG, m, Ofs_EAX); PC++;
#ifndef SINGLESTEP
			/* optimize common sequence LODSw-STOSw */
			if (!(EFLAGS & TF) && Fetch(PC) == STOSw) {
				Gen(O_MOVS_SetA, (m&ADDR16)|MOVSDST);
				Gen(O_MOVS_StoD, m);
				m |= MOVSDST;
				PC++;
			}
#endif
			Gen(O_MOVS_SavA, m);
			} break;
/*ae*/	case SCASb: {	int m = mode|(MBYTE|MOVSDST);
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, m, Ofs_AL);
			Gen(O_MOVS_ScaD, m);
			Gen(O_MOVS_SavA, m);
			PC++; } break;
/*af*/	case SCASw: {	int m = mode|MOVSDST;
			Gen(O_MOVS_SetA, m);
			Gen(L_REG, m, Ofs_EAX);
			Gen(O_MOVS_ScaD, m);
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
			PC += ModRM(opc, PC, m);
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
		}
		break;
/*d1*/	case SHIFTw:
/*d3*/	case SHIFTwv:
/*c1*/	case SHIFTwi: {
			int m = mode;
			unsigned char count = 0;
			PC += ModRM(opc, PC, m);
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
		}
		break;

/*e9*/	case JMPd: {
			unsigned char *p2 = JumpGen(PC, mode, 0x10, 4);
			if (TheCPU.err) return PC; else if (p2) PC = p2;
		   }
		   break;
/*e8*/	case CALLd: {
			unsigned char *p2 = JumpGen(PC, mode, 0x11, 4);
			if (TheCPU.err) return PC; else if (p2) PC = p2;
		   }
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
			    if (debug_level('e')>2)
				e_printf("CALL_FAR: ret=%04x:%08lx\n  calling:      %04x:%08lx\n",
					ocs,oip,jcs,jip);
			}
			else {
			    if (debug_level('e')>2)
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
			if (debug_level('e')>2)
				e_printf("RET: ret=%08x inc_sp=%d\n",TheCPU.eip,dr);
			temp = rESP + dr;
			rESP = (temp&TheCPU.StackMask) | (rESP&~TheCPU.StackMask);
			PC = (unsigned char *)(uintptr_t)(LONG_CS + TheCPU.eip); }
			break;
/*c3*/	case RET:
			CODE_FLUSH();
			POP(mode, &TheCPU.eip);
			if (debug_level('e')>2) e_printf("RET: ret=%08x\n",TheCPU.eip);
			PC = (unsigned char *)(uintptr_t)(LONG_CS + TheCPU.eip);
			break;
/*c6*/	case MOVbirm:
			PC += ModRM(opc, PC, mode|MBYTE);
			Gen(S_DI_IMM, mode|MBYTE, Fetch(PC));
			PC++; break;
/*c7*/	case MOVwirm:
			PC += ModRM(opc, PC, mode);
			Gen(S_DI_IMM, mode, DataFetchWL_U(mode,PC));
			INC_WL_PC(mode,0);
			break;
/*c8*/	case ENTER: {
			unsigned int sp, bp, frm;
			int level, ds;
			CODE_FLUSH();
			level = Fetch(PC+3) & 0x1f;
			ds = BT24(BitDATA16, mode);
			sp = LONG_SS + ((rESP - ds) & TheCPU.StackMask);
			bp = LONG_SS + (rEBP & TheCPU.StackMask);
			PUSH(mode, &rEBP);
			frm = sp - LONG_SS;
			if (level) {
				sp -= ds*level;
				while (--level) {
					bp -= ds;
					PUSH(mode, (void *)(uintptr_t)bp);
				}
				PUSH(mode, &frm);
			}
			if (mode&DATA16) rBP = frm; else rEBP = frm;
			sp -= FetchW(PC+1);
			temp = sp - LONG_SS;
			rESP = (temp&TheCPU.StackMask) | (rESP&~TheCPU.StackMask);
			PC += 4; }
			break;
/*c9*/	case LEAVE:
			Gen(O_LEAVE, mode); PC++;
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
			if (debug_level('e')>2)
				e_printf("RET_%ld: ret=%08x\n",dr,TheCPU.eip);
			PC = (unsigned char *)(uintptr_t)(LONG_CS + TheCPU.eip);
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
			e_printf("Overflow interrupt 04\n");
			TheCPU.err=EXCP04_INTO;
			return P0;
/*cd*/	case INT:
			CODE_FLUSH();
#ifdef ASM_DUMP
			fprintf(aLog,"%08lx:\t\tint %02x\n",(long)P0,Fetch(PC+1));
#endif
			if (Fetch(PC+1)==0x31) InvalidateSegs();
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
			PC = (unsigned char *)(uintptr_t)(LONG_CS + TheCPU.eip);
			if (opc==RETl) {
			    if (debug_level('e')>1)
				e_printf("RET_FAR: ret=%04lx:%08x\n",sv,TheCPU.eip);
			    break;	/* un-fall */
			}
			if (debug_level('e')>1) {
				e_printf("IRET: ret=%04lx:%08x\n",sv,TheCPU.eip);
			}
			temp=0; POP(m, &temp);
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
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x->{r=%08x v=%08x}\n",temp,EFLAGS,get_vFLAGS(EFLAGS));
			}
			if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
			} break;

/*9d*/	case POPF: {
			CODE_FLUSH();
			temp=0; POP(mode, &temp);
			if (V86MODE()) {
stack_return_from_vm86:
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x fl=%08x vf=%08x\n",
					temp,EFLAGS,eVEFLAGS);
			    if (IOPL==3) {	/* Intel manual */
				if (mode & DATA16)
				    FLAGS = temp;	/* oh,really? */
				else
				    EFLAGS = (temp&~0x1b3000) | (EFLAGS&0x1b3000);
			    }
			    else {
				/* virtual-8086 monitor */
				if (vm86s.vm86plus.vm86dbg_active &&
				    vm86s.vm86plus.vm86dbg_TFpendig) {
				    temp |= TF;
				}
				/* move TSSMASK from pop{e}flags to V{E}FLAGS */
				eVEFLAGS = (eVEFLAGS & ~eTSSMASK) | (temp & eTSSMASK);
				/* move 0xdd5 from pop{e}flags to regs->eflags */
				EFLAGS = (EFLAGS & ~0xdd5) | (temp & 0xdd5);
				if (temp & EFLAGS_IF) {
				    eVEFLAGS |= EFLAGS_VIF;
		    		    if (vm86s.regs.eflags & VIP) {
					if (debug_level('e')>1)
					    e_printf("Return for STI fl=%08x vf=%08x\n",
			    			EFLAGS,eVEFLAGS);
					TheCPU.err=EXCP_STISIGNAL;
					return PC + (opc==POPF);
				    }
				}
				if (vm86s.vm86plus.force_return_for_pic &&
					(eVEFLAGS & EFLAGS_IFK)) {
				    if (debug_level('e')>1)
					e_printf("Return for PIC fl=%08x vf=%08x\n",
		    			    EFLAGS,eVEFLAGS);
				    TheCPU.err=EXCP_PICSIGNAL;
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
			    if (in_dpmi) {
				if (temp & EFLAGS_IF)
				    set_IF();
				else {
				    clear_IF();
				}
			    }
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x->{r=%08x v=%08x}\n",temp,EFLAGS,_EFLAGS);
			}
			if (CONFIG_CPUSIM) RFL.valid = V_INVALID;
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
					CODE_FLUSH();
					goto not_permitted;
				case LODSb:
					repmod |= (MBYTE|MOVSSRC);
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_LodD, repmod);
					Gen(S_REG, repmod, Ofs_AL);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case LODSw:
					repmod |= MOVSSRC;
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_LodD, repmod);
					Gen(S_REG, repmod, Ofs_AX);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case MOVSb:
					repmod |= (MBYTE|MOVSSRC|MOVSDST);
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_MovD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case MOVSw:
					repmod |= (MOVSSRC|MOVSDST);
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_MovD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case CMPSb:
					repmod |= (MBYTE|MOVSSRC|MOVSDST|MREPCOND);
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case CMPSw:
					repmod |= (MOVSSRC|MOVSDST|MREPCOND);
					Gen(O_MOVS_SetA, repmod);
					Gen(O_MOVS_CmpD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case STOSb:
					repmod |= (MBYTE|MOVSDST);
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					Gen(O_MOVS_StoD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case STOSw:
					repmod |= MOVSDST;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, Ofs_EAX);
					Gen(O_MOVS_StoD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case SCASb:
					repmod |= (MBYTE|MOVSDST|MREPCOND);
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod|MBYTE, Ofs_AL);
					Gen(O_MOVS_ScaD, repmod);
					Gen(O_MOVS_SavA, repmod);
					PC++; break;
				case SCASw:
					repmod |= MOVSDST|MREPCOND;
					Gen(O_MOVS_SetA, repmod);
					Gen(L_REG, repmod, Ofs_EAX);
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
					if (debug_level('e')>4)
					    e_printf("OPERoverride: new mode %04x\n",repmod);
					PC++; goto repag0;
				case ADDRoverride:
					repmod = (repmod & ~ADDR16) | (~basemode & ADDR16);
					if (debug_level('e')>4)
					    e_printf("ADDRoverride: new mode %04x\n",repmod);
					PC++; goto repag0;
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
			PC += ModRM(opc, PC, mode|MBYTE);	// EDI=mem
			Gen(L_DI_R1, mode|MBYTE);		// mov al,[edi]
			switch(REG1) {
			case Ofs_AL:	/*0*/	/* TEST */
			case Ofs_CL:	/*1*/	/* undocumented */
				Gen(O_AND_R, mode|MBYTE|IMMED, Fetch(PC));	// op al,#imm
				PC++;
				break;
			case Ofs_DL:	/*2*/	/* NOT */
				Gen(O_NOT, mode|MBYTE);			// not al
				Gen(S_DI, mode|MBYTE);
				break;
			case Ofs_BL:	/*3*/	/* NEG */
				Gen(O_NEG, mode|MBYTE);			// neg al
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
				break;
			case Ofs_BH:	/*7*/	/* IDIV AL */
				Gen(O_IDIV, mode|MBYTE, P0);		// ah:al/[edi]->AH:AL signed
				break;
			} }
			break;
/*f7*/	case GRP1wrm: {
			PC += ModRM(opc, PC, mode);			// EDI=mem
			Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
			switch(REG1) {
			case Ofs_AX:	/*0*/	/* TEST */
			case Ofs_CX:	/*1*/	/* undocumented */
				Gen(O_AND_R, mode|IMMED, DataFetchWL_U(mode,PC));	// op al,#imm
				INC_WL_PC(mode,0);
				break;
			case Ofs_DX:	/*2*/	/* NOT */
				Gen(O_NOT, mode);			// not (e)ax
				Gen(S_DI, mode);
				break;
			case Ofs_BX:	/*3*/	/* NEG */
				Gen(O_NEG, mode);			// neg (e)ax
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
				break;
			case Ofs_DI:	/*7*/	/* IDIV AX+DX */
				Gen(O_IDIV, mode, P0);		// (e)ax:(e)dx/[edi]->(E)AX:(E)DX signed
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
			CODE_FLUSH();
			if (REALMODE() || (CPL <= IOPL) || (IOPL==3)) {
				EFLAGS &= ~EFLAGS_IF;
			}
			else {
			    /* virtual-8086 monitor */
			    if (V86MODE()) {
				if (debug_level('e')>2) e_printf("Virtual VM86 CLI\n");
				eVEFLAGS &= ~EFLAGS_VIF;
				goto checkpic;
			    }
			    else if (in_dpmi) {
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
				eVEFLAGS |= EFLAGS_VIF;
				if (vm86s.regs.eflags & VIP) {
				    if (debug_level('e')>1)
					e_printf("Return for STI fl=%08x vf=%08x\n",
			    		    EFLAGS,eVEFLAGS);
				    TheCPU.err=EXCP_STISIGNAL;
				    return PC+1;
				}
			}
			else {
			    if (REALMODE() || (CPL <= IOPL) || (IOPL==3)) {
				EFLAGS |= EFLAGS_IF;
			    }
			    else if (in_dpmi) {
				if (debug_level('e')>2) e_printf("Virtual DPMI STI\n");
				set_IF();
			    }
			    else
				goto not_permitted;	/* GPF */
			}
			PC++;
			break;
/*fc*/	case CLD:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1))
				EFLAGS &= ~EFLAGS_DF;
			else
				Gen(O_SETFL, mode, CLD);
			break;
/*fd*/	case STD:	PC++;
			if ((CurrIMeta<0)&&(InterOps[Fetch(PC)]&1))
				EFLAGS |= EFLAGS_DF;
			else
				Gen(O_SETFL, mode, STD);
			break;
/*fe*/	case GRP2brm:	/* only INC and DEC are legal on bytes */
			ModGetReg1(PC, mode);
			switch(REG1) {
			case Ofs_AL:	/*0*/
				if (PC[1] >= 0xc0) {
					Gen(O_INC_R, mode|MBYTE,
					    R1Tab_b[PC[1] & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, mode|MBYTE); // EDI=mem
				Gen(L_DI_R1, mode|MBYTE);
				Gen(O_INC, mode|MBYTE);
				Gen(S_DI, mode|MBYTE);
				break;
			case Ofs_CL:	/*1*/
				if (PC[1] >= 0xc8) {
					Gen(O_DEC_R, mode|MBYTE,
					    R1Tab_b[PC[1] & 7]);
					PC += 2;
					break;
				}
				PC += ModRM(opc, PC, mode|MBYTE); // EDI=mem
				Gen(L_DI_R1, mode|MBYTE);
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
				   here (single byte DEC/INC exist) */
				PC += ModRM(opc, PC, mode);
				Gen(L_DI_R1, mode);
				Gen(O_INC, mode);
				Gen(S_DI, mode);
				break;
			case Ofs_CX:	/*1*/
				PC += ModRM(opc, PC, mode);
				Gen(L_DI_R1, mode);
				Gen(O_DEC, mode);
				Gen(S_DI, mode);
				break;
			case Ofs_SP:	/*4*/	 // JMP near indirect
			case Ofs_DX: {	/*2*/	 // CALL near indirect
					int dp;
					CODE_FLUSH();
					PC += ModRMSim(PC, mode|NOFLDR);
					TheCPU.eip = (long)PC - LONG_CS;
					dp = DataGetWL_U(mode, TheCPU.mem_ref);
					if (REG1==Ofs_DX) { 
						PUSH(mode, &TheCPU.eip);
						if (debug_level('e')>2)
							e_printf("CALL indirect: ret=%08x\n\tcalling: %08x\n",
								TheCPU.eip,dp);
					}
					PC = (unsigned char *)(uintptr_t)(LONG_CS + dp);
				}
				break;
			case Ofs_BX:	/*3*/	 // CALL long indirect restartable
			case Ofs_BP: {	/*5*/	 // JMP long indirect restartable
					unsigned short ocs,jcs;
					unsigned long oip,xcs,jip=0;
					CODE_FLUSH();
					PC += ModRMSim(PC, mode|NOFLDR);
					TheCPU.eip = (long)PC - LONG_CS;
					/* get new cs:ip */
					jip = DataGetWL_U(mode, TheCPU.mem_ref);
					jcs = GetDWord(TheCPU.mem_ref+BT24(BitDATA16,mode));
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
					    if (debug_level('e')>2)
						e_printf("CALL_FAR indirect: ret=%04x:%08lx\n\tcalling: %04x:%08lx\n",
							ocs,oip,jcs,jip);
					}
					else {
					    if (debug_level('e')>2)
						e_printf("JMP_FAR indirect: %04x:%08lx\n",jcs,jip);
					}
					TheCPU.eip = jip;
					PC = (unsigned char *)(uintptr_t)(LONG_CS + jip);
#ifdef SKIP_EMU_VBIOS
					if ((jcs&0xf000)==config.vbios_seg) {
					    /* return the new PC after the jump */
					    TheCPU.err = EXCP_GOBACK; return PC;
					}
#endif
				}
				break;
			case Ofs_SI:	/*6*/	 // PUSH
				PC += ModRM(opc, PC, mode);
				Gen(O_PUSH, mode|MEMADR); break;	// push [mem]
				break;
			default:
				CODE_FLUSH();
				goto illegal_op;
			}
			break;

/*6c*/	case INSb: {
			unsigned short a;
			unsigned long rd;
			CODE_FLUSH();
			a = rDX;
			rd = (mode&ADDR16? rDI:rEDI);
			if (test_ioperm(a))
				((char *)(uintptr_t)LONG_ES)[rd] = port_real_inb(a);
			else
				((char *)(uintptr_t)LONG_ES)[rd] = port_inb(a);
			if (EFLAGS & EFLAGS_DF) rd--; else rd++;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			PC++; } break;
/*ec*/	case INvb: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (config.X && (a>=0x3c0) && (a<=0x3df)) {
			  switch(a&0x1f) {
			    // [012.456789a.c.ef....45....a.....]
			    case 0x00:	/*ATTRIBUTE_INDEX*/
			    case 0x01:	/*ATTRIBUTE_DATA*/
			    case 0x02:	/*INPUT_STATUS_0*/
			    case 0x04:	/*SEQUENCER_INDEX*/
			    case 0x05:	/*SEQUENCER_DATA*/
			    case 0x06:	/*DAC_PEL_MASK*/
			    case 0x07:	/*DAC_STATE*/
			    case 0x08:	/*DAC_WRITE_INDEX*/
			    case 0x09:	/*DAC_DATA*/
			    case 0x0a:	/*FEATURE_CONTROL_R*/
			    case 0x0c:	/*MISC_OUTPUT_R*/
			    case 0x0e:	/*GFX_INDEX*/
			    case 0x0f:	/*GFX_DATA*/
			    case 0x14:	/*CRTC_INDEX*/
			    case 0x15:	/*CRTC_DATA*/
			    case 0x1a:	/*INPUT_STATUS_1*/
				rAL = VGA_emulate_inb(a);
				break;
			    default: dbug_printf("not emulated EC %x\n",a);
				leavedos(0);
		  	  }
	  		  PC++; break;
			}
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
				while (((rAL=port_real_inb(a))&8)==0);
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
				//while ((((rAL=port_real_inb(a))&8)!=0) && rcx)
				//    rcx--;
				//if (tp==0) set_ioperm(a,1,0);
				//if (mode&DATA16) rCX=rcx; else rECX=rcx;
				//PC+=5; break;
			    //}
			    else if (c1==0xfbe108a8) {
				// e1 fb = loop while VR==0
				unsigned int rcx = mode&DATA16? rCX:rECX;
				if (tp==0) set_ioperm(a,1,1);
				while ((((rAL=port_real_inb(a))&8)==0) && rcx)
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
					while (((rAL=port_real_inb(a))&amk)==0);
				    else
					while (((rAL=port_real_inb(a))&amk)!=0);
				    if (tp==0) set_ioperm(a,1,0);
				}
				else
				    rAL = (PC[5]&1? amk:0);
				PC+=9; break;
			    }
			}
#endif
			if (test_ioperm(a)) {
#ifdef CPUEMU_DIRECT_IO
				Gen(O_INPDX, mode|MBYTE); NewNode=1;
#else
				rAL = port_real_inb(a);
#endif
			}
			else
				rAL = port_inb(a);
			}
			PC++; break;
/*e4*/	case INb: {
			unsigned short a;
			CODE_FLUSH();
			E_TIME_STRETCH;		// for PIT
			/* there's no reason to compile this, as most of
			 * the ports under 0x100 are emulated by dosemu */
			a = Fetch(PC+1);
			if (test_ioperm(a)) rAL = port_real_inb(a);
				else rAL = port_inb(a);
			PC += 2; } break;
/*6d*/	case INSw: {
			unsigned long rd;
			void *p;
			int dp;
			CODE_FLUSH();
			rd = (mode&ADDR16? rDI:rEDI);
			p = (void *)(LONG_ES+rd);
			if (mode&DATA16) {
				*((short *)p) = port_inw(rDX); dp=2;
			}
			else {
				*((int *)p) = port_ind(rDX); dp=4;
			}
			if (EFLAGS & EFLAGS_DF) rd-=dp; else rd+=dp;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			PC++; } break;
/*ed*/	case INvw: {
			CODE_FLUSH();
			if (config.X && (rDX>=0x3c0) && (rDX<=0x3de)) {
			    dbug_printf("X: INW,IND %x in VGA space\n",rDX);
			    leavedos(0);
			}
			if (mode&DATA16) rAX = port_inw(rDX);
			else rEAX = port_ind(rDX);
			} PC++; break;
/*e5*/	case INw: {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if (mode&DATA16) rAX = port_inw(a); else rEAX = port_ind(a);
			PC += 2; } break;

/*6e*/	case OUTSb: {
			unsigned short a;
			unsigned long rs;
			int iop;
			CODE_FLUSH();
			a = rDX;
			rs = (mode&ADDR16? rSI:rESI);
			iop = test_ioperm(a);
			do {
			    if (iop)
				port_real_outb(a,((char *)(uintptr_t)LONG_DS)[rs]);
			    else
				port_outb(a,((char *)(uintptr_t)LONG_DS)[rs]);
			    if (EFLAGS & EFLAGS_DF) rs--; else rs++;
			    PC++;
			} while (Fetch(PC)==OUTSb);
			if (mode&ADDR16) rSI=rs; else rESI=rs;
			} break;
/*ee*/	case OUTvb: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (config.X && (a>=0x3c0) && (a<=0x3df)) {
			  switch(a&0x1f) {
			    // [0.2.456789....e.....45...9a.....]
			    case 0x00:	/*ATTRIBUTE_INDEX*/
			    case 0x02:	/*MISC_OUTPUT_W*/
			    case 0x04:	/*SEQUENCER_INDEX*/
			    case 0x05:	/*SEQUENCER_DATA*/
			    case 0x06:	/*DAC_PEL_MASK*/
			    case 0x07:	/*DAC_READ_INDEX*/
			    case 0x08:	/*DAC_WRITE_INDEX*/
			    case 0x09:	/*DAC_DATA*/
			    case 0x0e:	/*GFX_INDEX*/
			    case 0x0f:	/*GFX_DATA*/
			    case 0x14:	/*CRTC_INDEX*/
			    case 0x15:	/*CRTC_DATA*/
			    case 0x19:  /*COLOR_SELECT*/
			    case 0x1a:  /*FEATURE_CONTROL_W*/
				VGA_emulate_outb(a,rAL);
				break;
			    default: dbug_printf("not emulated EE %x\n",a);
				leavedos(0);
			  }
	  		  PC++; break;
			}
			if (test_ioperm(a)) {
#ifdef CPUEMU_DIRECT_IO
				Gen(O_OUTPDX, mode|MBYTE); NewNode=1;
#else
				port_real_outb(a,rAL);
#endif
			}
			else
				port_outb(a,rAL);
			}
			PC++; break;
/*e6*/	case OUTb:  {
			unsigned short a;
			CODE_FLUSH();
			a = Fetch(PC+1);
			if ((a&0xfc)==0x40) E_TIME_STRETCH;	// for PIT
			/* there's no reason to compile this, as most of
			 * the ports under 0x100 are emulated by dosemu */
			if (test_ioperm(a)) port_real_outb(a,rAL);
				else port_outb(a,rAL);
			PC += 2; } break;
/*6f*/	case OUTSw:
			CODE_FLUSH();
			goto not_implemented;
/*ef*/	case OUTvw: {
			unsigned short a;
			CODE_FLUSH();
			a = rDX;
			if (config.X && (a>=0x3c0) && (a<=0x3de)) {
			  if (mode&DATA16) {
			    switch(a&0x1f) {
			      // [....456789..........45..........]
			      case 0x04:	/*SEQUENCER_INDEX*/
			      case 0x06:	/*DAC_PEL_MASK*/
			      case 0x08:	/*DAC_WRITE_INDEX*/
			      case 0x0e:	/*GFX_INDEX*/
			      case 0x14:	/*CRTC_INDEX*/
				VGA_emulate_outw(a,rAX);
				break;
			      default: dbug_printf("not emulated EF %x\n",a);
				leavedos(0);
			    }
			  }
			  else {
			    dbug_printf("X: OUTD %x in VGA space\n",a);
			    leavedos(0);
			  }
	  		  PC++; break;
			}
			if (test_ioperm(a)) {
#ifdef CPUEMU_DIRECT_IO
			    Gen(O_OUTPDX, mode); NewNode=1;
#else
			    if (mode&DATA16) port_real_outw(a,rAX); else port_real_outd(a,rEAX);
#endif
			}
			else {
			    if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
			} }
			PC++; break;

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
			int sim = 0;
			if ((b&0xc0)==0xc0) {
				exop |= 0x40;
				if (exop==0x63) {
					CODE_FLUSH(); sim=1;
				}
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
			TheCPU.mode |= M_FPOP;
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
				case 1: /* STR */
				    /* Store Task Register */
				case 2: /* LLDT */
				    /* Load Local Descriptor Table Register */
				case 3: /* LTR */
				    /* Load Task Register */
				case 4: { /* VERR */
				    unsigned short sv; int tmp;
				    CODE_FLUSH();
				    if (REALMODE()) goto illegal_op;
				    PC += ModRMSim(PC+1, mode) + 1;
				    sv = GetDWord(TheCPU.mem_ref);
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
				    sv = GetDWord(TheCPU.mem_ref);
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
				case 1: /* SIDT */
				    /* Store Interrupt Descriptor Table Register */
				case 2: /* LGDT */ /* PM privileged AND real mode */
				    /* Load Global Descriptor Table Register */
				case 3: /* LIDT */ /* PM privileged AND real mode */
				    /* Load Interrupt Descriptor Table Register */
				case 4: /* SMSW, 80286 compatibility */
				    /* Store Machine Status Word */
				    PC++; PC += ModRM(opc, PC, mode);
				    Gen(L_CR0, mode);
				    Gen(S_DI, mode|DATA16);
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
				sv = GetDWord(TheCPU.mem_ref);
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
				  unsigned char *p2 = JumpGen(PC, mode, (opc2-0x80), 1);
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
				PC++; PC += ModRM(opc, PC, mode|MBYTE);
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
			case 0xbc: /* BSF */
			case 0xbd: /* BSR */
				PC++; PC += ModRM(opc, PC, mode);
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
				case 0x20: /* BT */
				case 0x28: /* BTS */
				case 0x30: /* BTR */
				case 0x38: /* BTC */
					PC++; PC += ModRM(opc, PC, mode);
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
				PC++; PC += ModRM(opc, PC, mode);
				if (opc2&1) {
					Gen(O_SHFD, mode, (opc2&8), REG1);
				}
				else {
					Gen(O_SHFD, mode|IMMED, (opc2&8), REG1, Fetch(PC));
					PC++;
				}
				break;

			case 0xa6: /* CMPXCHGb */
			case 0xa7: /* CMPXCHGw */
			    CODE_FLUSH();
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
				PC++; PC += ModRM(opc, PC, mode);
				Gen(L_DI_R1, mode);		// mov (e)ax,[edi]
				Gen(O_IMUL, mode|MEMADR, REG1);	// reg*[edi]->reg signed
				break;
			case 0xb0:
			case 0xb1:		/* CMPXCHG */
			    CODE_FLUSH();
			    goto not_implemented;
///
			case 0xb2: /* LSS */
				if (REALADDR()) {
				    PC++; PC += ModRM(opc, PC, mode);
				    Gen(L_LXS1, mode, REG1);
				    Gen(L_LXS2, mode, Ofs_SS, Ofs_XSS);
				}
				else {
				    unsigned short sv = 0;
				    unsigned long rv;
				    CODE_FLUSH();
				    CEmuStat |= CeS_LOCK;
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
				PC++; PC += ModRM(opc, PC, mode|MBYTX);
				Gen(L_MOVZS, mode|MBYTX, 0, REG1);
				break;
			case 0xb7: /* MOVZXw */
				PC++; PC += ModRM(opc, PC, mode);
				Gen(L_MOVZS, mode, 0, REG1);
				break;
			case 0xbe: /* MOVSXb */
				PC++; PC += ModRM(opc, PC, mode|MBYTX);
				Gen(L_MOVZS, mode|MBYTX, 1, REG1);
				break;
			case 0xbf: /* MOVSXw */
				PC++; PC += ModRM(opc, PC, mode);
				Gen(L_MOVZS, mode, 1, REG1);
				break;
///
			case 0xb8:	/* JMP absolute to IA64 code */
			case 0xb9:
			    CODE_FLUSH();
			    goto illegal_op;	/* UD2 */
			case 0xc0: /* XADDb */
			case 0xc1: /* XADDw */
			/* case 0xc2-0xc6:	MMX */
			/* case 0xc7:	Code Extension 23 - 01=CMPXCHG8B mem */
			    CODE_FLUSH();
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
			if (!(TheCPU.mode&SKIPOP))
				(void)NewIMeta(P0, TheCPU.mode, &rc);
#ifdef HOST_ARCH_X86
			if (!CONFIG_CPUSIM && rc < 0) {	// metadata table full
				if (debug_level('e')>2)
					e_printf("============ Tab full:cannot close sequence\n");
				leavedos(0x9000);
			}
#endif
		}
		else
		{
		    if (debug_level('e')>2)
			e_printf("\n%s",e_print_regs());
		    TheCPU.EMUtime += FAKE_INS_TIME;
		}
#ifdef ASM_DUMP
		{
#else
		if (debug_level('e')>2) {
#endif
		    char *ds = e_emu_disasm(P0,(~basemode&3));
#ifdef ASM_DUMP
		    fprintf(aLog,"%s\n",ds);
#endif
		    if (debug_level('e')>2) e_printf("  %s\n", ds);
		}

		P0 = PC;
#ifndef SINGLESTEP
		if (!(CEmuStat & CeS_TRAP)) continue;
#endif
		CloseAndExec(P0, NULL, mode, __LINE__);
		e_printf("\n%s",e_print_regs());
		NewNode = 0;
	}
	return 0;

not_implemented:
	dbug_printf("!!! Unimplemented %02x %02x %02x at %p\n",opc,PC[1],PC[2],PC);
	TheCPU.err = -2; return PC;
#ifdef HOST_ARCH_X86
bad_return:
	dbug_printf("!!! Bad code return\n");
	TheCPU.err = -3; return PC;
#endif
not_permitted:
	if (debug_level('e')>1) e_printf("!!! Not permitted %02x\n",opc);
	TheCPU.err = EXCP0D_GPF; return PC;
//div_by_zero:
//	dbug_printf("!!! Div by 0 %02x\n",opc);
//	TheCPU.err = -6; return PC;
illegal_op:
	dbug_printf("!!! Illegal op %02x %02x %02x\n",opc,PC[1],PC[2]);
	TheCPU.err = EXCP06_ILLOP; return PC;
}

