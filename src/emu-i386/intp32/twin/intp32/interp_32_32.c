/* NOTE:
 * This file was modified for DOSEMU by the DOSEMU-team.
 * The original is 'Copyright 1997 Willows Software, Inc.' and generously
 * was put under the GNU Library General Public License.
 * ( for more information see http://www.willows.com/ )
 *
 * We make use of section 3 of the GNU Library General Public License
 * ('...opt to apply the terms of the ordinary GNU General Public License...'),
 * because the resulting product is an integrated part of DOSEMU and
 * can not be considered to be a 'library' in the terms of Library License.
 * The (below) original copyright notice from Willows therefore was edited
 * conforming to section 3 of the GNU Library General Public License.
 *
 * Nov. 1 1997, The DOSEMU team.
 */

/*    
	interp_32_32.c	1.11
    	Copyright 1997 Willows Software, Inc. 

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; see the file COPYING.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.


For more information about the Willows Twin Libraries.

	http://www.willows.com	

To send email to the maintainer of the Willows Twin Libraries.

	mailto:twin@willows.com 

changes for use with dosemu-0.67 1997/10/20 vignani@tin.it
changes for use with dosemu-0.99 1998/12/13 vignani@tin.it
 */

#include "emu-globv.h"

/*
#include "bitops.h"
#include "port.h"
#include "Log.h"
*/
#include "hsw_interp.h"
#include "mod_rm.h"

extern long instr_count;
extern int emu_under_X;
#ifdef EMU_STAT
#include "timers.h"
extern long InsFreq[];
#ifdef EMU_PROFILE
extern long long InsTimes[];
static int LastOp = 0x100;
static long long pfT0 = 0;
static long long pfT1 = 0;
#endif
#endif

#ifndef NO_DEBUG_MSGS
extern BYTE *ea_;
extern DWORD eav_;
#endif

BYTE *
hsw_interp_32_32(Interp_ENV *env, BYTE *P0, register BYTE *_PC, int *err, int cycmax)
{
    BYTE *mem_ref;

    PC = _PC;
    *err = 0;

next_switch:
#if defined(EMU_STAT) && defined(EMU_PROFILE)
    if (d_emu==0) pfT1 = GETTSC();
#endif
    if (!(CEmuStat&CeS_MODE_PM32)) {
      if ((cycmax--) <= 0) return (PC); /* no error, we're going back to 16/16 */
    }
    else {
      if (CEmuStat & (CeS_TRAP|CeS_DRTRAP|CeS_SIGPEND|CeS_LOCK)) {
	if (CEmuStat & CeS_LOCK)
	    CEmuStat &= ~CeS_LOCK;
	else {
	    if (CEmuStat & CeS_TRAP) {
	      if (*err) {
		if (*err==EXCP01_SSTP) CEmuStat &= ~CeS_TRAP; return (PC);
	      }
	      *err = EXCP01_SSTP;
	    }
	    else if (CEmuStat & CeS_DRTRAP) {
	      if (e_debug_check(PC)) { *err = EXCP01_SSTP; return(PC); }
	    }
	    else if ((env->flags & INTERRUPT_FLAG) && (CEmuStat & CeS_SIGPEND)) {
	      /* force exit after signal */
	      CEmuStat = (CEmuStat & ~CeS_SIGPEND) | CeS_SIGACT;
	      *err=EXCP_SIGNAL; return (PC);
	    }
	}
      }
      instr_count++;
      EMUtime += CYCperINS;
fast_switch:
      OVERR_DS = LONG_DS;
      OVERR_SS = LONG_SS;
      P0 = PC;
#ifndef NO_TRACE_MSGS
      if (d_emu) {
	e_trace(env, P0, PC, 3);
      }
#endif
#ifdef EMU_STAT
      InsFreq[*((WORD *)PC)] += 1;
#ifdef EMU_PROFILE
      if (d_emu==0) {
	if (LastOp < 0x100) {
	    InsTimes[LastOp] += (pfT1 - pfT0);
	}
	LastOp = *PC; pfT0 = GETTSC();
      }
#endif
#endif
    }

override: ;    /* single semicolon needed to attach label to */
    switch (*PC) {
/*00*/	case ADDbfrm: {
	    DWORD src1, src2, res;
#ifndef NO_DEBUG_MSGS
	    /* 00 00 00 00 00 is probably invalid code... */
	    if (*((long *)(PC+1))==0) { *err=-98; return P0; }
#endif
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } goto next_switch; 
/*01*/	case ADDwfrm: {
	    DWORD src1, src2, res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
		res = src1 + src2;
	    mPUT_QUAD(mem_ref, res);
	    SETDFLAGS(1,0);
	    } goto next_switch; 
/*02*/	case ADDbtrm: {
	    DWORD src1, src2, res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } goto next_switch; 
/*03*/	case ADDwtrm: {
	    DWORD src1, src2, res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	    *EREG1 = res = src1 + src2;
	    SETDFLAGS(1,0);
	    } goto next_switch; 
/*04*/	case ADDbia: {
	    DWORD src1, src2, res;
	    src1 = AL; src2 = *(PC+1);
	    res = src1 + src2;
	    AL = (BYTE)res;
	    SETBFLAGS(0);
	    } PC += 2; goto next_switch;
/*05*/	case ADDwia: {
	    DWORD src1, src2, res;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 + src2;
	    SETDFLAGS(1,0);
	    } PC += 5; goto next_switch;
/*06*/	case PUSHes: {
            register DWORD temp = SHORT_ES_16;
            PUSHQUAD(temp);
            } PC += 1; goto next_switch;
/*07*/	case POPes: {	/* restartable */
	    register DWORD temp;
	    temp = TOS_WORD;
	    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
		env->error_addr=temp&0xfffc; return P0; }
            POPQUAD(temp); temp &= 0xffff;
            SHORT_ES_32 = temp; }
            PC += 1; goto next_switch;
/*08*/	case ORbfrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    mem_ref = MEM_REF;
	    res = *mem_ref | *HREG1;
	    *mem_ref = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*09*/	case ORwfrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF; 
	    res = mFETCH_QUAD(mem_ref) | *EREG1;
	    mPUT_QUAD(mem_ref, res);
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*0a*/	case ORbtrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    res = *HREG1 | *MEM_REF;
	    *HREG1 = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*0b*/	case ORwtrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    res = mFETCH_QUAD(MEM_REF) | *EREG1;
	    *EREG1 = res;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*0c*/	case ORbi: {
	    DWORD res;
	    res = AL | PC[1];
	    AL = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } PC += 2; goto next_switch;
/*0d*/	case ORwi: {
	    DWORD res;
	    res = EAX | FETCH_QUAD((PC+1));
	    EAX = res;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } PC += 5; goto next_switch;
/*0e*/	case PUSHcs: {
	    register DWORD temp = SHORT_CS_16;
	    PUSHQUAD(temp);
	    } PC += 1; goto next_switch;

/*0f*/	case TwoByteESC: {
	    register DWORD temp;
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 - Extended Opcode 20 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    mPUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 1: /* STR */ {
			    /* Store Task Register */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    mPUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
/**/			    goto not_implemented;
			case 3: /* LTR */ {
			    /* Load Task Register */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    mem_ref = MEM_REF;
			    temp = mFETCH_QUAD(mem_ref);
/**/			    goto not_implemented;
			    /* hsw_task_register = temp; */
			    } goto next_switch;
			case 4: /* VERR */ {
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    temp = mFETCH_QUAD(MEM_REF) & 0xffff;
			    if (hsw_verr(temp)) SET_ZF;
				else CLEAR_ZF;
			    } goto next_switch;
			case 5: /* VERW */ {
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    temp = mFETCH_QUAD(MEM_REF) & 0xffff;
			    if (hsw_verw(temp)) SET_ZF;
				else CLEAR_ZF;
			    } goto next_switch;
			case 6: /* Illegal */
			case 7: /* Illegal */
			    goto not_permitted;
		    }
		case 0x01: /* GRP7 - Extended Opcode 21 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 1: /* SIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 2: /* LGDT */ /* Privileged */
			    /* Load Global Descriptor Table Register */
/**/			    goto not_implemented;
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    goto next_switch;
			case 3: /* LIDT */ /* Privileged */
			    /* Load Interrupt Descriptor Table Register */
/**/			    goto not_implemented;
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    goto next_switch;
			case 4: /* SMSW */ {
			    /* Store Machine Status Word */
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LIMIT field */;
			    mPUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 5: /* Illegal */
			    goto not_permitted;
			case 6: /* LMSW */ /* Privileged */
			    /* Load Machine Status Word */
/**/			    goto not_implemented;
			    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			    goto next_switch;
			case 7: /* Illegal */
			    goto not_permitted;
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
			WORD _sel;
			PC += 1; PC += hsw_modrm_32_quad(MODARGS);
			mem_ref = MEM_REF;
			_sel = mFETCH_WORD(mem_ref);
			if (IS_GDT(_sel)||((_sel>>3)>LDT_ENTRIES))
			    CLEAR_ZF;
			else {
			    temp = GetSelectorFlags(_sel);
			    if (temp) SetFlagAccessed(_sel);
			    SET_ZF;
			    *EREG1 = temp << 8;
			}
		    } goto next_switch;
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
		    WORD _sel;
		    PC += 1; PC += hsw_modrm_32_quad(MODARGS);
		    mem_ref = MEM_REF;
		    _sel = mFETCH_WORD(mem_ref);
		    if (IS_GDT(_sel)||((_sel>>3)>LDT_ENTRIES))
			CLEAR_ZF;
		    else {
		    temp= GetSelectorByteLimit(_sel);
			*EREG1 = temp;
			SET_ZF;
		    }
		    /* what do I do here??? */
		    } goto next_switch;
/* case 0x04:	LOADALL */
/* case 0x05:	LOADALL(286) - SYSCALL(K6) */
		case 0x06: /* CLTS */ /* Privileged */
		    if (CPL != 0) goto not_permitted;
		    /* Clear Task State Register */
		    CRs[0] &= ~8;
		    PC += 2; goto next_switch;
/* case 0x07:	LOADALL(386) - SYSRET(K6) etc. */
		case 0x08: /* INVD */
		    /* INValiDate cache */
		    PC += 2; goto next_switch;
		case 0x09: /* WBINVD */
		    /* Write-Back and INValiDate cache */
		    PC += 2; goto next_switch;
		case 0x0a: goto illegal_op;
		case 0x0b: goto illegal_op;	/* UD2 */
/* case 0x0d:	Code Extension 25(AMD-3D) */
/* case 0x0e:	FEMMS(K6-3D) */
		case 0x0f:	/* AMD-3D */
		    PC += 2; goto next_switch;
/* case 0x10-0x1f:	various V20/MMX instr. */
		case 0x20: /* MOVcdrd */ /* Privileged */
 		case 0x22: { /* MOVrdcd */ /* Privileged */
		    DWORD *srg; int reg;
		    if (VM86F) goto not_permitted;
		    reg = (PC[2]>>3)&7;
		    if ((reg==1)||(reg>4)) goto illegal_op;
		    if ((PC[2]&0xc0)!=0xc0) goto illegal_op;
		    switch (PC[2]&7) {
			case 0: srg = &(EAX); break;
			case 1: srg = &(ECX); break;
			case 2: srg = &(EDX); break;
			case 3: srg = &(EBX); break;
			case 6: srg = &(ESI); break;
			case 7: srg = &(EDI); break;
			default: goto illegal_op;
		    }
		    if (PC[1]&2) {	/* write to CRs */
			if (reg==0) {
			    if ((CRs[0] ^ *srg) & 1) {
				/*dbug_printf("RM/PM switch not allowed\n");*/
				FatalAppExit(0, "PROT");
			    }
			    CRs[0] = (*srg&0xe005002f)|0x10;
			}
			else CRs[reg] = *srg;
		    } else {
			*srg = CRs[reg];
		    }
		    } PC += 3; goto next_switch;
		case 0x21: /* MOVddrd */ /* Privileged */
		case 0x23: { /* MOVrddd */ /* Privileged */
		    DWORD *srg; int reg;
		    if (VM86F) goto not_permitted;
		    reg = (PC[2]>>3)&7;
		    if ((PC[2]&0xc0)!=0xc0) goto illegal_op;
		    switch (PC[2]&7) {
			case 0: srg = &(EAX); break;
			case 1: srg = &(ECX); break;
			case 2: srg = &(EDX); break;
			case 3: srg = &(EBX); break;
			case 6: srg = &(ESI); break;
			case 7: srg = &(EDI); break;
			default: goto illegal_op;
		    }
		    if (PC[1]&2) {	/* write to DRs */
			DRs[reg] = *srg;
		    } else {
			*srg = DRs[reg];
		    }
		    } PC += 3; goto next_switch;
		case 0x24: /* MOVtdrd */ /* Privileged */
		case 0x26: { /* MOVrdtd */ /* Privileged */
		    DWORD *srg; int reg;
		    if (VM86F) goto not_permitted;
		    reg = (PC[2]>>3)&7;
		    if (reg < 6) goto illegal_op;
		    reg -= 6;
		    if ((PC[2]&0xc0)!=0xc0) goto illegal_op;
		    switch (PC[2]&7) {
			case 0: srg = &(EAX); break;
			case 1: srg = &(ECX); break;
			case 2: srg = &(EDX); break;
			case 3: srg = &(EBX); break;
			case 6: srg = &(ESI); break;
			case 7: srg = &(EDI); break;
			default: goto illegal_op;
		    }
		    if (PC[1]&2) {	/* write to TRs */
			TRs[reg] = *srg;
		    } else {
			*srg = TRs[reg];
		    }
		    } PC += 3; goto next_switch;
/* case 0x27-0x2f:	various V20/MMX instr. */
		case 0x30:	/* WRMSR */
		    goto not_implemented;
		case 0x31: /* RDTSC */
		    {
			EAX = (DWORD)EMUtime;
			EDX = (DWORD)(EMUtime>>32);
			PC += 2; goto next_switch;
		    }
		case 0x32:	/* RDMSR */
		    goto not_implemented;
/* case 0x33:	RDPMC(P6) */
/* case 0x34:	SYSENTER(PII) */
/* case 0x35:	SYSEXIT(PII) */
/* case 0x40-0x4f:	CMOV(P6) */
/* case 0x50-0x5f:	various Cyrix/MMX */
#ifdef __GNUC__
		case 0x60 ... 0x6b:	/* MMX */
		case 0x6e: case 0x6f:
		case 0x74 ... 0x77:
#else
		case 0x60:	/* MMX */
		case 0x61: case 0x62: case 0x63: case 0x64:
		case 0x65: case 0x66: case 0x67: case 0x68:
		case 0x69: case 0x6a: case 0x6b:
		case 0x6e: case 0x6f:
		case 0x74: case 0x75: case 0x76: case 0x77:
#endif
/* case 0x78-0x7e:	Cyrix */
		case 0x7e: case 0x7f:
		    PC += 2; goto next_switch;
		case 0x80: /* JOimmdisp */
#ifndef NO_TRACE_MSGS
		    env->flags &= ~OVERFLOW_FLAG;
#endif
		    if (IS_OF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
#ifndef NO_TRACE_MSGS
			env->flags |= OVERFLOW_FLAG;
#endif
			goto next_switch;
		    } else PC += 6; goto next_switch;
		case 0x81: /* JNOimmdisp */
#ifndef NO_TRACE_MSGS
		    env->flags |= OVERFLOW_FLAG;
#endif
		    if (!IS_OF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
#ifndef NO_TRACE_MSGS
			env->flags &= ~OVERFLOW_FLAG;
#endif
			goto next_switch;
		    } else PC += 6; goto next_switch;
		case 0x82: /* JBimmdisp */
		    if (IS_CF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x83: /* JNBimmdisp */
		    if (!IS_CF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x84: /* JZimmdisp */
		    if (IS_ZF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x85: /* JNZimmdisp */
		    if (!IS_ZF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x86: /* JBEimmdisp */
		    if (IS_CF_SET || IS_ZF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x87: /* JNBEimmdisp */
		    if (!(IS_CF_SET || IS_ZF_SET)) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x88: /* JSimmdisp */
		    if (IS_SF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x89: /* JNSimmdisp */
		    if (!IS_SF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x8a: /* JPimmdisp */
#ifndef NO_TRACE_MSGS
		    env->flags &= ~PARITY_FLAG;
#endif
		    if (IS_PF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
#ifndef NO_TRACE_MSGS
			env->flags |= PARITY_FLAG;
#endif
			goto next_switch;
		    } else PC += 6; goto next_switch;
		case 0x8b: /* JNPimmdisp */
#ifndef NO_TRACE_MSGS
		    env->flags |= PARITY_FLAG;
#endif
		    if (!IS_PF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
#ifndef NO_TRACE_MSGS
			env->flags &= ~PARITY_FLAG;
#endif
			goto next_switch;
		    } else PC += 6; goto next_switch;
		case 0x8c: /* JLimmdisp */
		    if (IS_SF_SET ^ IS_OF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x8d: /* JNLimmdisp */
		    if (!(IS_SF_SET ^ IS_OF_SET)) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x8e: /* JLEimmdisp */
		    if ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x8f: /* JNLEimmdisp */
		    if (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) {
			PC += (6 + (int)FETCH_QUAD(PC+2));
		    } else PC += 6; goto next_switch;
		case 0x90: /* SETObrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_OF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x91: /* SETNObrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!IS_OF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x92: /* SETBbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_CF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x93: /* SETNBbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!IS_CF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x94: /* SETZbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x95: /* SETNZbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x96: /* SETBEbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x97: /* SETNBEbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0x98: /* SETSbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_SF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x99: /* SETNSbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!(IS_SF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0x9a: /* SETPbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_PF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9b: /* SETNPbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!IS_PF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9c: /* SETLbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9d: /* SETNLbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0x9e: /* SETLEbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9f: /* SETNLEbrm */ {
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    *MEM_REF = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0xa0: /* PUSHfs */ {
		    temp = SHORT_FS_16;
		    PUSHQUAD(temp);
		    } PC += 2; goto next_switch;
		case 0xa1: /* POPfs */ {
		    temp = TOS_WORD;
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
			env->error_addr=temp&0xfffc; return P0; }
		    POPQUAD(temp); temp &= 0xffff;
		    SHORT_FS_32 = temp;
		    } PC += 2; goto next_switch;
		case 0xa2: /* CPUID */ {
		      if (EAX==0) {
			EAX = 1;
			EBX = 0x756e6547;
			ECX = 0x6c65746e;
			EDX = 0x49656e69;
		      }
		      else if (EAX==1) {
			EAX = 0x052c;
			EBX = ECX = 0;
			EDX = 0x1bf;
		      }
		    } PC += 2; goto next_switch;
                case 0xa3: /* BT */ {
                    DWORD ind1;
                    long ind2;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
			temp = FETCH_EREG(mem_ref);
                        CARRY = ((int)temp >> ind2)&1;
                    } else {
                        if(ind2 >= 0) {
                            ind1 = ((ind2 >> 5) << 2);
                            mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
			    CARRY = (WORD)((temp >> ind2) & 0x1);
                        } else {
                            ind2 = -ind2 - 1;
                            ind1 = ((ind2 >> 5) + 1) << 2;
                            mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (((temp << ind2) & 0x80000000)? 1:0);
                        }
                    }
                    } goto next_switch;
		case 0xa4: /* SHLDimm */ {
		    /* Double Precision Shift Left */
		    DWORD count, res; WORD carry;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    count = *PC & 0x1f; PC++;
		    mem_ref = MEM_REF;
		    res = mFETCH_QUAD(mem_ref);
		    carry = (res >> (32 - count)) & 1;
		    res = (res << count) | (*EREG1 >> (32 - count));
		    mPUT_QUAD(mem_ref,res);
		    /* flags: set S,Z,P - OF,AF undefined */
		    PACK32_16(res,RES_16); CARRY = carry;
		    } goto next_switch;
		case 0xa5: /* SHLDcl */ {
		    /* Double Precision Shift Left by CL */
		    DWORD count, res; WORD carry;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    count = CL & 0x1f;
		    mem_ref = MEM_REF;
		    res = mFETCH_QUAD(mem_ref);
		    carry = (res >> (32 - count)) & 1;
		    res = (res << count) | (*EREG1 >> (32 - count));
		    mPUT_QUAD(mem_ref,res);
		    /* flags: set S,Z,P - OF,AF undefined */
		    PACK32_16(res,RES_16); CARRY = carry;
		    } goto next_switch;
		case 0xa6: /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
		case 0xa7: /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		goto not_implemented;
		case 0xa8: /* PUSHgs */ {
		    temp = SHORT_GS_16;
		    PUSHQUAD(temp);
		    } PC += 2; goto next_switch;
		case 0xa9: /* POPgs */ {
		    temp = TOS_WORD;
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
			env->error_addr=temp&0xfffc; return P0; }
		    POPQUAD(temp); temp &= 0xffff;
		    SHORT_GS_32 = temp;
		    } PC += 2; goto next_switch;
		case 0xaa:
		    goto illegal_op;
                case 0xab: /* BTS */ {
                    DWORD ind1;
                    long ind2; 
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
			temp = FETCH_EREG(mem_ref);
                        CARRY = ((int)temp >> ind2)&1;
                        temp |= (0x1 << ind2);
			PUT_EREG(mem_ref, temp);
                    } else {  
                        if(ind2 >= 0) {
                            ind1 = ((ind2 >> 5) << 2);
                            mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
			    CARRY = (WORD)((temp >> ind2) & 0x1);
                            temp |= (0x1 << ind2);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind2 = -ind2 - 1;
                            ind1 = ((ind2 >> 5) + 1) << 2;
                            mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (((temp << ind2) & 0x80000000)? 1:0);
                            temp |= (0x80000000 >> ind2);
                            PUT_QUAD(mem_ref,temp);
                        }     
                    }
                    } goto next_switch;
		case 0xac: /* SHRDimm */ {
		    /* Double Precision Shift Right by immediate */
		    DWORD count, res; WORD carry;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    count = *PC & 0x1f; PC++;
		    mem_ref = MEM_REF;
		    res = mFETCH_QUAD(mem_ref);
		    carry = (res >> (count - 1)) & 1;
		    res = (res >> count) | (*EREG1 << (32 - count));
		    mPUT_QUAD(mem_ref,res);
		    /* flags: set S,Z,P - OF,AF undefined */
		    PACK32_16(res,RES_16); CARRY = carry;
		    } goto next_switch;
		case 0xad: /* SHRDcl */ {
		    /* Double Precision Shift Right by CL */
		    DWORD count, res; WORD carry;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    count = CL & 0x1f;
		    mem_ref = MEM_REF;
		    res = mFETCH_QUAD(mem_ref);
		    carry = (res >> (count - 1)) & 1;
		    res = (res >> count) | (*EREG1 << (32 - count));
		    mPUT_QUAD(mem_ref,res);
		    /* flags: set S,Z,P - OF,AF undefined */
		    PACK32_16(res,RES_16); CARRY = carry;
		    } goto next_switch;
/* case 0xae:	Code Extension 24(MMX) */
                case 0xaf: /* IMULregrm */ {
			s_i64_u res, mlt;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                        res.s.sl = *EREG1;
                        res.s.sh = (res.s.sl<0? -1:0);
                        mem_ref = MEM_REF;
			mlt.s.sl = mFETCH_QUAD(mem_ref);
                        mlt.s.sh = (mlt.s.sl<0? -1:0);
			res.sd *= mlt.sd;
			*EREG1 = res.s.sl;
		    if (res.s.sl == res.sd) {  /* CF=OF=0 */
			CARRY=0; SRC1_16 = SRC2_16 = RES_16; /* 000,111 */
		    }
		    else {
			CARRY=1; SRC1_16 = SRC2_16 = ~RES_16;  /* 001,110 */
                        }
                        } goto next_switch;
/* case 0xb0-0xb1:	CMPXCHG */
		case 0xb2: /* LSS */ {
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    if (IS_MODE_REG) {
			/* Illegal */
			    goto illegal_op;
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref+4);
		    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
			env->error_addr=temp&0xfffc; return P0; }
		    *EREG1 = FETCH_QUAD(mem_ref);
		    SHORT_SS_32 = temp;
		    } goto next_switch;
                case 0xb3: /* BTR */ {
                    DWORD ind1;
                    long ind2;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0xf);
			temp = FETCH_EREG(mem_ref);
                        CARRY = ((int)temp >> ind2)&1;
                        temp &= ~(0x1 << ind2);
			PUT_EREG(mem_ref, temp);
                    } else {
                        if(ind2 >= 0) {
                            ind1 = ((ind2 >> 5) << 2);
                            mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
			    CARRY = (WORD)((temp >> ind2) & 0x1);
                            temp &= ~(0x1 << ind2);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind2 = -ind2 - 1;
                            ind1 = ((ind2 >> 5) + 1) << 2;
                            mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (((temp << ind2) & 0x80000000)? 1:0);
                            temp &= ~(0x80000000 >> ind2);
                            PUT_QUAD(mem_ref,temp);
                        }
                    }
                    } goto next_switch;
		case 0xb4: /* LFS */ {
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    if (IS_MODE_REG) {
			/* Illegal */
			    goto illegal_op;
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref+4);
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
			env->error_addr=temp&0xfffc; return P0; }
		    *EREG1 = FETCH_QUAD(mem_ref);
		    SHORT_FS_32 = temp;
		    } goto next_switch;
		case 0xb5: /* LGS */ {
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
		    if (IS_MODE_REG) {
			/* Illegal */
			    goto illegal_op;
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref+4);
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
			env->error_addr=temp&0xfffc; return P0; }
		    *EREG1 = FETCH_QUAD(mem_ref);
		    SHORT_GS_32 = temp;
		    } goto next_switch;
		case 0xb6: /* MOVZXb */ {
		    int ref = (*(PC+2)>>3)&7;
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    temp = *(BYTE *)MEM_REF;
		    switch (ref) {
		      case 0: EAX = temp; goto next_switch;
		      case 1: ECX = temp; goto next_switch;
		      case 2: EDX = temp; goto next_switch;
		      case 3: EBX = temp; goto next_switch;
		      case 4: ESP = temp; goto next_switch;
		      case 5: EBP = temp; goto next_switch;
		      case 6: ESI = temp; goto next_switch;
		      case 7: EDI = temp; goto next_switch;
		    }
		    }
		    goto bad_data;
		case 0xb7: /* MOVZXw */ {
		    int ref = (*(PC+2)>>3)&7;
		    PC++; PC += hsw_modrm_32_word(MODARGS);
		    temp = mFETCH_WORD(MEM_REF);
		    switch (ref) {
		      case 0: EAX = temp; goto next_switch;
		      case 1: ECX = temp; goto next_switch;
		      case 2: EDX = temp; goto next_switch;
		      case 3: EBX = temp; goto next_switch;
		      case 4: ESP = temp; goto next_switch;
		      case 5: EBP = temp; goto next_switch;
		      case 6: ESI = temp; goto next_switch;
		      case 7: EDI = temp; goto next_switch;
		    }
		    }
		    goto bad_data;
		case 0xb9: goto illegal_op;	/* UD2 */
		case 0xba: /* GRP8 - Code Extension 22 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* Illegal */
			case 1: /* Illegal */
			case 2: /* Illegal */
			case 3: /* Illegal */
			    goto not_permitted;
                        case 4: /* BT */ {
                            int temp1;
			    PC++; PC += hsw_modrm_32_quad(MODARGS);
                            mem_ref = MEM_REF; temp = *PC;  PC++;
			    temp1 = mFETCH_QUAD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0x1f))&1;
                    } goto next_switch;  
                        case 5: /* BTS */ {
                            int temp1;
			    PC++; PC += hsw_modrm_32_quad(MODARGS);
                            mem_ref = MEM_REF; temp = (*PC) & 0x1f;  PC++;
			    temp1 = mFETCH_QUAD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
			    mPUT_QUAD(mem_ref,temp1);
                    } goto next_switch;  
                        case 6: /* BTR */ {
                            int temp1;
			    PC++; PC += hsw_modrm_32_quad(MODARGS);
                            mem_ref = MEM_REF; temp = (*PC) & 0x1f;  PC++;
			    temp1 = mFETCH_QUAD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
			    mPUT_QUAD(mem_ref,temp1);
                    } goto next_switch;  
                        case 7: /* BTC */ {
                            int temp1;
			    PC++; PC += hsw_modrm_32_quad(MODARGS);
                            mem_ref = MEM_REF; temp = (*PC) & 0x1f;  PC++;
			    temp1 = mFETCH_QUAD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
			    mPUT_QUAD(mem_ref,temp1);
                    } goto next_switch;
                    }
                case 0xbb: /* BTC */ {
                    DWORD ind1;
                    long ind2;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
			temp = FETCH_EREG(mem_ref);
                        CARRY = ((int)temp >> ind2)&1;
                        temp ^= (0x1 << ind2);
			PUT_EREG(mem_ref, temp);
                    } else {
                        if(ind2 >= 0) {
                            ind1 = ((ind2 >> 5) << 2);
                            mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
			    CARRY = (WORD)((temp >> ind2) & 0x1);
                            temp ^= (0x1 << ind2);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind2 = -ind2 - 1;
                            ind1 = ((ind2 >> 5) +1) << 2;
                            mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (((temp << ind2) & 0x80000000)? 1:0);
                            temp ^= (0x80000000 >> ind2);
                            PUT_QUAD(mem_ref,temp);
                        }
                    }
                    } goto next_switch;
                case 0xbc: /* BSF */ {
                    int i;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    mem_ref = MEM_REF;
		    temp = mFETCH_QUAD(mem_ref);
		    if(temp) {
                    for(i=0; i<32; i++)
                        if((temp >> i) & 0x1) break;
                    *EREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } goto next_switch;
                case 0xbd: /* BSR */ {
                    int i;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    mem_ref = MEM_REF;
		    temp = mFETCH_QUAD(mem_ref);
		    if(temp) {
                    for(i=31; i>=0; i--)
                        if((temp & ( 0x1 << i))) break;
                    *EREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } goto next_switch;
                case 0xbe: /* MOVSXb */ {
                    register signed long stemp;
		    int ref = (*(PC+2)>>3)&7;
		    PC++; PC += hsw_modrm_32_byte(MODARGS);
		    stemp = *(signed char *)MEM_REF;
		    switch (ref) {
		      case 0: EAX = stemp; goto next_switch;
		      case 1: ECX = stemp; goto next_switch;
		      case 2: EDX = stemp; goto next_switch;
		      case 3: EBX = stemp; goto next_switch;
		      case 4: ESP = stemp; goto next_switch;
		      case 5: EBP = stemp; goto next_switch;
		      case 6: ESI = stemp; goto next_switch;
		      case 7: EDI = stemp; goto next_switch;
		    }
                    }
		    goto bad_data;
		case 0xbf: /* MOVSXw */ {
                    register signed long stemp;
		    int ref = (*(PC+2)>>3)&7;
		    PC++; PC += hsw_modrm_32_word(MODARGS);
		    stemp = (signed short)mFETCH_WORD(MEM_REF);
		    switch (ref) {
		      case 0: EAX = stemp; goto next_switch;
		      case 1: ECX = stemp; goto next_switch;
		      case 2: EDX = stemp; goto next_switch;
		      case 3: EBX = stemp; goto next_switch;
		      case 4: ESP = stemp; goto next_switch;
		      case 5: EBP = stemp; goto next_switch;
		      case 6: ESI = stemp; goto next_switch;
		      case 7: EDI = stemp; goto next_switch;
		    }
		    }
		    goto bad_data;
                case 0xc0: { /* XADDb */
                    int res,src1,src2;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
                    *HREG1 = src1;
                    *mem_ref = res = src1 + src2;
		    SETBFLAGS(0);
                    } goto next_switch;
		case 0xc1: { /* XADDw */
                    DWORD res,src1,src2;
		    PC++; PC += hsw_modrm_32_quad(MODARGS);
                    src2 = *EREG1; mem_ref = MEM_REF;
		    src1 = mFETCH_QUAD(mem_ref);
                        res = src1 + src2;
		    mPUT_QUAD(mem_ref, res);
                    *EREG1 = src1;
                    SETDFLAGS(1,0);
                } goto next_switch;
/* case 0xc2-0xc6:	MMX */
/* case 0xc7:	Code Extension 23 - 01=CMPXCHG8B mem */
                case 0xc8: /* BSWAPeax */ {
                    temp = EAX;
                    EAX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xc9: /* BSWAPecx */ {
                    temp = ECX;
                    ECX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xca: /* BSWAPedx */ {
                    temp = EDX;
                    EDX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xcb: /* BSWAPebx */ {
                    temp = EBX;
                    EBX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xcc: /* BSWAPesp */ {
                    temp = ESP;
                    ESP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xcd: /* BSWAPebp */ {
                    temp = EBP;
                    EBP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xce: /* BSWAPesi */ {
                    temp = ESI;
                    ESI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
                case 0xcf: /* BSWAPedi */ {
                    temp = EDI;
                    EDI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; goto next_switch;
#ifdef __GNUC__
		case 0xd1 ... 0xd3:	/* MMX */
		case 0xd5: case 0xd7: case 0xda: case 0xde:
		case 0xdf:
		case 0xe0 ... 0xe5:
		case 0xf1 ... 0xf3:
		case 0xf5 ... 0xf7:
		case 0xfc ... 0xfe:
#else
		case 0xd1:	/* MMX */
		case 0xd2: case 0xd3:
		case 0xd5: case 0xd7: case 0xda: case 0xde:
		case 0xdf:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5:
		case 0xf1: case 0xf2: case 0xf3:
		case 0xf5: case 0xf6: case 0xf7:
		case 0xfc: case 0xfd: case 0xfe:
#endif
		    PC += 2; goto next_switch;
		case 0xff:	/* WinOS2 WANTS an int06 */
		    goto illegal_op;
		default: goto illegal_op;
                }    
	    }

/*10*/	case ADCbfrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    res = src1 + src2 + (CARRY & 1);
	    *mem_ref = (BYTE)res;
	    SETBFLAGS(0);
	    } goto next_switch; 
/*11*/	case ADCwfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	    res = src1 + src2 + (CARRY & 1);
	    mPUT_QUAD(mem_ref, res);
	    SETDFLAGS(1,0);
	    } goto next_switch; 
/*12*/	case ADCbtrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    res = src1 + src2 + (CARRY & 1);
	    *mem_ref = (BYTE)res;
	    SETBFLAGS(0);
	    } goto next_switch; 
/*13*/	case ADCwtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	    res = src1 + src2 + (CARRY & 1);
	    *EREG1 = res;
	    SETDFLAGS(1,0);
	    } goto next_switch; 
/*14*/	case ADCbi: {
	    DWORD res, src1, src2;
	    src1 = AL; src2 = *(PC+1);
	    res = src1 + src2 + (CARRY & 1);
	    AL = (BYTE)res;
	    SETBFLAGS(0);
	    } PC += 2; goto next_switch;
/*15*/	case ADCwi: {
            DWORD res, src1, src2;
            src1 = EAX; 
	    src2 = FETCH_QUAD((PC+1));
	    res = src1 + src2 + (CARRY & 1);
	    EAX = res;
            SETDFLAGS(1,0);
            } PC += 5; goto next_switch;
/*16*/	case PUSHss: {
            register DWORD temp = SHORT_SS_16;
            PUSHQUAD(temp);
            } PC += 1; goto next_switch;
/*17*/	case POPss: {  
            DWORD temp, temp2;
            POPQUAD(temp2); temp = temp2 & 0xffff;
	    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
		PUSHQUAD(temp2); env->error_addr=temp&0xfffc; return P0; }
	    CEmuStat |= CeS_LOCK;
            SHORT_SS_32 = temp;
            } PC += 1; goto next_switch;
/*18*/	case SBBbfrm: {
	    int src1, src2, res;
	    int carry = CARRY & 1;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *HREG1;
	    mem_ref = MEM_REF; src1 = *mem_ref;
	    res = src1 - src2 - carry;
	    *mem_ref = (BYTE)res;
	    SETBFLAGS(1);
	    } goto next_switch; 
/*19*/	case SBBwfrm: {
	    DWORD src1, src2, res;
	    int carry = CARRY & 1;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	    res = src1 - src2 - carry;
	    mPUT_QUAD(mem_ref, res);
	    SETDFLAGS(1,1);
	    } goto next_switch; 
/*1a*/	case SBBbtrm: {
	    int src1, src2, res;
	    int carry = CARRY & 1;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *MEM_REF;
	    mem_ref = HREG1; src1 = *mem_ref;
	    res = src1 - src2 - carry;
	    *mem_ref = (BYTE)res;
	    SETBFLAGS(1);
	    } goto next_switch; 
/*1b*/	case SBBwtrm: {
	    DWORD src1, src2, res;
	    int carry = CARRY & 1;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    src2 = mFETCH_QUAD(mem_ref);
	    res = src1 - src2 - carry;
	    *EREG1 = res;
	    SETDFLAGS(1,1);
	    } goto next_switch; 
/*1c*/	case SBBbi: {
	    int src1, src2, res;
	    int carry = CARRY & 1;
	    src1 = AL; src2 = *(PC+1);
	    res = src1 - src2 - carry;
	    AL = (BYTE)res;
	    SETBFLAGS(1);
	    } PC += 2; goto next_switch;
/*1d*/	case SBBwi: {
	    DWORD src1, src2, res;
	    int carry = CARRY & 1;
            src1 = EAX;     
	    src2 = FETCH_QUAD((PC+1));
	    res = src1 - src2 - carry;
	    EAX = res;
            SETDFLAGS(1,1); 
            } PC += 5; goto next_switch;
/*1e*/	case PUSHds: {
            register DWORD temp = SHORT_DS_16;
            PUSHQUAD(temp);
            } PC += 1; goto next_switch;
/*1f*/	case POPds: {
	    DWORD temp;
	    temp = TOS_WORD;
	    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
		env->error_addr=temp&0xfffc; return P0; }
            POPQUAD(temp); temp &= 0xffff;
            SHORT_DS_32 = temp;
            } PC += 1; goto next_switch;

/*20*/	case ANDbfrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    mem_ref = MEM_REF;
	    res = *mem_ref & *HREG1;
	    *mem_ref = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*21*/	case ANDwfrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF; 
	    res = mFETCH_QUAD(mem_ref) & *EREG1;
	    mPUT_QUAD(mem_ref, res);
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*22*/	case ANDbtrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    res = *HREG1 & *MEM_REF;
	    *HREG1 = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*23*/	case ANDwtrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    res = mFETCH_QUAD(MEM_REF) & *EREG1;
	    *EREG1 = res;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*24*/	case ANDbi: {
	    DWORD res;
	    res = AL & PC[1];
	    AL = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } PC += 2; goto next_switch;
/*25*/	case ANDwi: {
	    DWORD res;
	    res = EAX & FETCH_QUAD((PC+1));
	    EAX = res;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
            } PC += 5; goto next_switch;
/*26*/	case SEGes:
	    if (!VM86F && (SHORT_ES_16 < 4 || LONG_ES==INVALID_OVR)) {
		error("General Protection Fault: zero ES\n");
		goto bad_override;
	    }
	    OVERR_DS = OVERR_SS = LONG_ES;
	    PC += 1; goto override;
/*27*/	case DAA:
#ifndef NO_TRACE_MSGS
	    env->flags &= ~AUX_CARRY_FLAG;
#endif
	    if (((AL & 0x0f) > 9 ) || (IS_AF_SET)) {
		AL += 6;
#ifndef NO_TRACE_MSGS
		env->flags |= AUX_CARRY_FLAG;
#endif
		SET_AF;
	    } else CLEAR_AF;
	    if ((AL > 0x9f) || (IS_CF_SET)) {
		AL += 0x60;
		SET_CF;
	    } /*else CLEAR_CF;*/
	    RES_8 = AL;
	    BYTE_FLAG = BYTE_OP;
	    PC += 1; goto next_switch;
/*28*/	case SUBbfrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } goto next_switch; 
/*29*/	case SUBwfrm: {
	    DWORD res, src1;
	    long src2;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
		res = src1 - src2;
	    mPUT_QUAD(mem_ref, res);
	    SETDFLAGS(1,1);
	    } goto next_switch; 
/*2a*/	case SUBbtrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } goto next_switch; 
/*2b*/	case SUBwtrm: {
	    DWORD res, src1;
	    long src2;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    src2 = mFETCH_QUAD(mem_ref);
	    *EREG1 = res = src1 - src2;
	    SETDFLAGS(1,1);
	    } goto next_switch; 
/*2c*/	case SUBbi: {
	    DWORD src1, src2; int res;
	    src1 = AL; 
	    src2 = *(PC+1);
	    AL = res = src1 - src2;
	    SETBFLAGS(1);
	    } PC += 2; goto next_switch;
/*2d*/	case SUBwi: {
            DWORD res, src1;
            long src2; 
            src1 = EAX;
            src2 = FETCH_QUAD((PC+1));
            EAX = res = src1 - src2;
            SETDFLAGS(1,1); 
            } PC += 5; goto next_switch;
/*2e*/	case SEGcs:
	    OVERR_DS = OVERR_SS = LONG_CS;
	    PC+=1; goto override;
/*2f*/	case DAS: {
	    BYTE carry = 0;
	    BYTE altmp = AL;
#ifndef NO_TRACE_MSGS
	    env->flags &= ~AUX_CARRY_FLAG;
#endif
	    if (((altmp & 0x0f) > 9) || (IS_AF_SET)) {
                AL -= 6;
		carry = CARRYB || (AL < 6);
                SET_AF;
#ifndef NO_TRACE_MSGS
		env->flags |= AUX_CARRY_FLAG;
#endif
            } else CLEAR_AF;
	    if ((altmp > 0x99) || (IS_CF_SET)) {
                AL -= 0x60;
		carry = 1;
	    }
            RES_8 = AL;
	    CARRYB = carry;
            BYTE_FLAG = BYTE_OP;
	    } PC += 1; goto next_switch;

/*30*/	case XORbfrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    mem_ref = MEM_REF;
	    res = *mem_ref ^ *HREG1;
	    *mem_ref = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*31*/	case XORwfrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF; 
	    res = mFETCH_QUAD(mem_ref) ^ *EREG1;
	    mPUT_QUAD(mem_ref, res);
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*32*/	case XORbtrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    res = *MEM_REF ^ *HREG1;
	    *HREG1 = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*33*/	case XORwtrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    res = mFETCH_QUAD(MEM_REF) ^ *EREG1;
	    *EREG1 = res;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*34*/	case XORbi: {
	    DWORD res;
	    res = AL ^ PC[1];
	    AL = (BYTE)res;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } PC += 2; goto next_switch;
/*35*/	case XORwi: {
	    DWORD res;
	    res = EAX ^ FETCH_QUAD((PC+1));
	    EAX = res;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
            } PC += 5; goto next_switch;
/*36*/	case SEGss:
	    OVERR_DS = OVERR_SS = LONG_SS;
	    PC+=1; goto override;
/*37*/	case AAA: {
	    BYTE icarry = (AL > 0xf9);
#ifndef NO_TRACE_MSGS
	    env->flags &= ~AUX_CARRY_FLAG;
#endif
	    if (((AL & 0x0f) > 9 ) || (IS_AF_SET)) {
		AL = (AL + 6) & 0x0f;
		AH = (AH + 1 + icarry);
                SET_CF;
#ifndef NO_TRACE_MSGS
		env->flags |= AUX_CARRY_FLAG;
#endif
                SET_AF;
            } else {
                CLEAR_CF;
                CLEAR_AF;
            AL &= 0x0f;
	    }
	    } PC += 1; goto next_switch;
/*38*/	case CMPbfrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } goto next_switch; 
/*39*/	case CMPwfrm: {
	    DWORD res, src1;
	    long src2;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	    res = src1 - src2;
	    SETDFLAGS(1,1);
	    } goto next_switch; 
/*3a*/	case CMPbtrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *MEM_REF; src1 = *HREG1;
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } goto next_switch; 
/*3b*/	case CMPwtrm: {
	    DWORD res, src1;
	    long src2;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    src2 = mFETCH_QUAD(mem_ref);
	    res = src1 - src2;
	    SETDFLAGS(1,1);
	    } goto next_switch; 
/*3c*/	case CMPbi: {
	    DWORD src1, src2; int res;
	    src1 = AL; 
	    src2 = *(PC+1);
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } PC += 2; goto next_switch;
/*3d*/	case CMPwi: {
            DWORD res, src1;
            long src2;
            src1 = EAX;
            src2 = FETCH_QUAD((PC+1));
            res = src1 - src2;
            SETDFLAGS(1,1); 
            } PC += 5; goto next_switch;
/*3e*/	case SEGds:
	    if (!VM86F && (SHORT_DS_16 < 4 || LONG_DS==INVALID_OVR)) {
		error("General Protection Fault: zero DS\n");
		goto bad_override;
	    }
	    OVERR_DS = OVERR_SS = LONG_DS;
	    PC+=1; goto override;
/*3f*/	case AAS: {
	    BYTE icarry = (AL < 6);
#ifndef NO_TRACE_MSGS
	    env->flags &= ~AUX_CARRY_FLAG;
#endif
	    if (((AL & 0x0f) > 9 ) || (IS_AF_SET)) {
		AL = (AL - 6) & 0x0f;
		AH = (AH - 1 - icarry);
                SET_CF;
#ifndef NO_TRACE_MSGS
		env->flags |= AUX_CARRY_FLAG;
#endif
                SET_AF;
            } else {
                CLEAR_CF;
                CLEAR_AF;
            AL &= 0x0f;
	    }
	    } PC += 1; goto next_switch;

/*40*/	case INCax: {
            DWORD res, src1, src2;
            src1 = EAX; src2 = 1;
            EAX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*41*/	case INCcx: {
            DWORD res, src1, src2;
            src1 = ECX; src2 = 1;
            ECX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*42*/	case INCdx: {
            DWORD res, src1, src2;
            src1 = EDX; src2 = 1;
            EDX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*43*/	case INCbx: {
            DWORD res, src1, src2;
            src1 = EBX; src2 = 1;
            EBX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*44*/	case INCsp: {
            DWORD res, src1, src2;
            src1 = ESP; src2 = 1;
            ESP = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*45*/	case INCbp: {
            DWORD res, src1, src2;
            src1 = EBP; src2 = 1;
            EBP = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*46*/	case INCsi: {
            DWORD res, src1, src2;
            src1 = ESI; src2 = 1;
            ESI = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*47*/	case INCdi: {
            DWORD res, src1, src2;
            src1 = EDI; src2 = 1;
            EDI = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*48*/	case DECax: {
            long res, src1, src2;
            src1 = EAX; src2 = -1;
            EAX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*49*/	case DECcx: {
            long res, src1, src2;
            src1 = ECX; src2 = -1;
            ECX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*4a*/	case DECdx: {
            long res, src1, src2;
            src1 = EDX; src2 = -1;
            EDX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*4b*/	case DECbx: {
            long res, src1, src2;
            src1 = EBX; src2 = -1;
            EBX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*4c*/	case DECsp: {
            long res, src1, src2;
            src1 = ESP; src2 = -1;
            ESP = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*4d*/	case DECbp: {
            long res, src1, src2;
            src1 = EBP; src2 = -1;
            EBP = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*4e*/	case DECsi: {
            long res, src1, src2;
            src1 = ESI; src2 = -1;
            ESI = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;
/*4f*/	case DECdi: {
            long res, src1, src2;
            src1 = EDI; src2 = -1;
            EDI = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; goto next_switch;

/*50*/	case PUSHax: {
            PUSHQUAD(EAX);
            } PC += 1; goto next_switch;
/*51*/	case PUSHcx: {  
            PUSHQUAD(ECX);
            } PC += 1; goto next_switch;
/*52*/	case PUSHdx: {  
            PUSHQUAD(EDX);
            } PC += 1; goto next_switch;
/*53*/	case PUSHbx: {  
            PUSHQUAD(EBX);
            } PC += 1; goto next_switch;
/*54*/	case PUSHsp: {  
	    register DWORD temp = ESP;
	    PUSHQUAD(temp);
            } PC += 1; goto next_switch;
/*55*/	case PUSHbp: {  
            PUSHQUAD(EBP);
            } PC += 1; goto next_switch;
/*56*/	case PUSHsi: {  
            PUSHQUAD(ESI);
            } PC += 1; goto next_switch;
/*57*/	case PUSHdi: {  
            PUSHQUAD(EDI);
            } PC += 1; goto next_switch;
/*58*/	case POPax: POPQUAD(EAX); PC += 1; goto next_switch;
/*59*/	case POPcx: POPQUAD(ECX); PC += 1; goto next_switch;
/*5a*/	case POPdx: POPQUAD(EDX); PC += 1; goto next_switch;
/*5b*/	case POPbx: POPQUAD(EBX); PC += 1; goto next_switch;
/*5c*/	case POPsp: {
	    register DWORD temp;
	    POPQUAD(temp);
	    ESP = temp;
	    } PC += 1; goto next_switch;
/*5d*/	case POPbp: POPQUAD(EBP); PC += 1; goto next_switch;
/*5e*/	case POPsi: POPQUAD(ESI); PC += 1; goto next_switch;
/*5f*/	case POPdi: POPQUAD(EDI); PC += 1; goto next_switch;

/*60*/	case PUSHA: {
	    register BYTE *sp = LONG_SS+(BIG_SS? ESP:SP);
	    sp -= 32;
	    PUT_QUAD(sp, EDI);
	    PUT_QUAD(sp+4, ESI);
	    PUT_QUAD(sp+8, EBP);
	    PUT_QUAD(sp+12, ESP);
	    PUT_QUAD(sp+16, EBX);
	    PUT_QUAD(sp+20, EDX);
	    PUT_QUAD(sp+24, ECX);
	    PUT_QUAD(sp+28, EAX);
	    if (BIG_SS) ESP=sp-LONG_SS; else SP=sp-LONG_SS;
	    } PC += 1; goto next_switch;
/*61*/	case POPA: {
	    register BYTE *sp = LONG_SS+(BIG_SS? ESP:SP);
	    EDI = FETCH_QUAD(sp);
	    ESI = FETCH_QUAD(sp+4);
	    EBP = FETCH_QUAD(sp+8);
	    EBX = FETCH_QUAD(sp+16);	/* discard ESP */
	    EDX = FETCH_QUAD(sp+20);
	    ECX = FETCH_QUAD(sp+24);
	    EAX = FETCH_QUAD(sp+28);
	    sp += 32;
	    if (BIG_SS) ESP=sp-LONG_SS; else SP=sp-LONG_SS;
	    } PC += 1; goto next_switch;
/*62*/	case BOUND:
/*63*/	case ARPL:
            goto not_implemented;
/*64*/	case SEGfs:
	    if (!VM86F && (SHORT_FS_16 < 4 || LONG_FS==INVALID_OVR)) {
		error("General Protection Fault: zero FS\n");
		goto bad_override;
	    }
	    OVERR_DS = OVERR_SS = LONG_FS;
	    PC+=1; goto override;
/*65*/	case SEGgs:
	    if (!VM86F && (SHORT_GS_16 < 4 || LONG_GS==INVALID_OVR)) {
		error("General Protection Fault: zero GS\n");
		goto bad_override;
	    }
	    OVERR_DS = OVERR_SS = LONG_GS;
	    PC+=1; goto override;
/*66*/	case OPERoverride:	/* 0x66: 16 bit operand, 32 bit addressing */
	    if (!data32) goto bad_override;
	    if (d_emu>4) e_printf0("ENTER interp_16d_32a\n");
	    data32 = 0;
	    PC = hsw_interp_16_32 (env, P0, PC+1, err);
	    data32 = 1;
	    if (*err) return (*err==EXCP_GOBACK? PC:P0);
	    goto next_switch;
/*67*/	case ADDRoverride:	/* 0x67: 32 bit operand, 16 bit addressing */
	    if (!code32) goto bad_override;
	    if (d_emu>4) e_printf0("ENTER interp_32d_16a\n");
	    code32 = 0;
	    PC = hsw_interp_32_16 (env, P0, PC+1, err);
	    code32 = 1;
	    if (*err) return (*err==EXCP_GOBACK? PC:P0);
	    goto next_switch;
/*68*/	case PUSHwi: {
	    register DWORD temp;
	    temp = FETCH_QUAD(PC+1);
	    PUSHQUAD(temp);
            } PC += 5; goto next_switch;
/*69*/	case IMULwrm: {
	    s_i64_u res, mlt;
	    PC += hsw_modrm_32_quad(MODARGS);
	    res.s.sl = FETCH_QUAD(PC);
	    res.s.sh = (res.s.sl<0? -1:0);
	    PC += 4; mem_ref = MEM_REF; 
	    mlt.s.sl = mFETCH_QUAD(mem_ref);
	    mlt.s.sh = (mlt.s.sl<0? -1:0);
	    res.sd *= mlt.sd;
	    *EREG1 = res.s.sl;
	    if (res.s.sl == res.sd) {  /* CF=OF=0 */
		CARRY=0; SRC1_16 = SRC2_16 = RES_16; /* 000,111 */
	    }
	    else {
		CARRY=1; SRC1_16 = SRC2_16 = ~RES_16;  /* 001,110 */
	    }
            } goto next_switch;
/*6a*/	case PUSHbi: {
	    register long temp = *((signed char *)(PC+1));
	    /*temp = ((temp<<24)>>24);*/
            PUSHQUAD(temp);
	    } PC += 2; goto next_switch; 
/*6b*/	case IMULbrm: {
	    s_i64_u res, mlt;
	    PC += hsw_modrm_32_quad(MODARGS);
	    res.s.sl = *(signed char *)(PC);
	    res.s.sh = (*PC & 0x80? -1:0);
	    PC += 1; mem_ref = MEM_REF; 
	    mlt.s.sl = mFETCH_QUAD(mem_ref);
	    mlt.s.sh = (mlt.s.sl<0? -1:0);
	    res.sd *= mlt.sd;
	    *EREG1 = res.s.sl;
	    if (res.s.sl == res.sd) {  /* CF=OF=0 */
		CARRY=0; SRC1_16 = SRC2_16 = RES_16; /* 000,111 */
	    }
	    else {
		CARRY=1; SRC1_16 = SRC2_16 = ~RES_16;  /* 001,110 */
	    }
	    } goto next_switch; 
/*6c*/	case INSb:
/*6d*/	case INSw:
/*6e*/	case OUTSb:
/*6f*/	case OUTSw:
	    goto not_permitted;

/*70*/	case JO:
#ifndef NO_TRACE_MSGS
	    env->flags &= ~OVERFLOW_FLAG;
#endif
	    if (IS_OF_SET) {
#ifndef NO_TRACE_MSGS
	    env->flags |= OVERFLOW_FLAG;
#endif
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*71*/	case JNO:
#ifndef NO_TRACE_MSGS
	    env->flags |= OVERFLOW_FLAG;
#endif
	    if (!IS_OF_SET) {
#ifndef NO_TRACE_MSGS
	    env->flags &= ~OVERFLOW_FLAG;
#endif
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*72*/	case JB_JNAE: if (IS_CF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*73*/	case JNB_JAE: if (!IS_CF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*74*/	case JE_JZ: if (IS_ZF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*75*/	case JNE_JNZ: if (!IS_ZF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*76*/	case JBE_JNA: if (IS_CF_SET || IS_ZF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*77*/	case JNBE_JA: if (!(IS_CF_SET || IS_ZF_SET)) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*78*/	case JS: if (IS_SF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*79*/	case JNS: if (!(IS_SF_SET)) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*7a*/	case JP_JPE:
#ifndef NO_TRACE_MSGS
	    env->flags &= ~PARITY_FLAG;
#endif
	    if (IS_PF_SET) {
#ifndef NO_TRACE_MSGS
	    env->flags |= PARITY_FLAG;
#endif
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*7b*/	case JNP_JPO:
#ifndef NO_TRACE_MSGS
	    env->flags |= PARITY_FLAG;
#endif
	    if (!IS_PF_SET) {
#ifndef NO_TRACE_MSGS
	    env->flags &= ~PARITY_FLAG;
#endif
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*7c*/	case JL_JNGE: if (IS_SF_SET ^ IS_OF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*7d*/	case JNL_JGE: if (!(IS_SF_SET ^ IS_OF_SET)) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*7e*/	case JLE_JNG: if ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;
/*7f*/	case JNLE_JG: if (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) {
	    JUMP(PC+1); goto fast_switch; 
	    } PC+=2; goto fast_switch;

/*82*/	case IMMEDbrm2:    /* out of order */
/*80*/	case IMMEDbrm: {
	    int src1, src2, res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(MODARGS);
	    src2 = *PC; PC += 1;
	    mem_ref = MEM_REF; src1 = *mem_ref;
	    switch (res) {
		case 0: /* ADD */
		    *mem_ref = res = src1 + src2;
		    SETBFLAGS(0);
		    goto next_switch;
		case 1: /* OR */
		    *mem_ref = res = src1 | src2;
		    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
		    goto next_switch;
		case 2: /* ADC */
		    res = src1 + src2 + (CARRY & 1);
		    *mem_ref = (BYTE)res;
		    SETBFLAGS(0);
		    goto next_switch;
		case 3: { /* SBB */
		    int carry = CARRY & 1;
		    res = src1 - src2 - carry;
		    *mem_ref = (BYTE)res;
		    SETBFLAGS(1);
		    } goto next_switch;
		case 4: /* AND */
		    *mem_ref = res = src1 & src2;
		    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
		    goto next_switch;
		case 5: /* SUB */
		    *mem_ref = res = src1 - src2;
		    SETBFLAGS(1);
		    goto next_switch;
		case 6: /* XOR */
		    *mem_ref = res = src1 ^ src2;
		    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    SETBFLAGS(1);
		    goto next_switch;
		default: goto not_implemented;
	    }}
/*81*/	case IMMEDwrm: {
	    DWORD src1, src2, res, temp;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
	    src2 = FETCH_QUAD(PC); PC += 4;
	    mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	    switch (temp) {
		case 0: /* ADD */
		    res = src1 + src2;
		    mPUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,0);
		    goto next_switch;
		case 1: /* OR */
		    res = src1 | src2;
		    mPUT_QUAD(mem_ref, res);
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 2: /* ADC */
		    res = src1 + src2 + (CARRY & 1);
		    mPUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,0);
		    goto next_switch;
		case 3: { /* SBB */
		    int carry = CARRY & 1;
		    res = src1 - src2 - carry;
		    mPUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,1);
		    } goto next_switch;
		case 4: /* AND */
		    res = src1 & src2;
		    mPUT_QUAD(mem_ref, res);
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 5: /* SUB */
		    res = src1 - src2;
		    mPUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,1);
		    goto next_switch;
		case 6: /* XOR */
		    res = src1 ^ src2;
		    mPUT_QUAD(mem_ref, res);
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    SETDFLAGS(1,1);
		    goto next_switch;
	      }
	    }
/*83*/	case IMMEDisrm: {
	    DWORD src1, src2, res;
	    register signed long temp;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
	    temp = *((signed char *)PC); PC += 1;
	    src2 = temp;
	    mem_ref = MEM_REF; 
	    src1 = mFETCH_QUAD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    mPUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    goto next_switch;
		case 1: /* OR */
		    res = src1 | src2;
		    mPUT_QUAD(mem_ref,res);
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 2: /* ADC */
		    res = src1 + src2 + (CARRY & 1);
		    mPUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    goto next_switch;
		case 3: { /* SBB */
		    int carry = CARRY & 1;
		    res = src1 - src2 - carry;
		    mPUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,1);
		    } goto next_switch;
		case 4: /* AND */
		    res = src1 & src2;
		    mPUT_QUAD(mem_ref,res);
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 5: /* SUB */
		    res = src1 - src2;
		    mPUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,1);
		    goto next_switch;
		case 6: /* XOR */
		    res = src1 ^ src2;
		    mPUT_QUAD(mem_ref,res);
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    SETDFLAGS(1,1);
		    goto next_switch;
	      }
	    }
/*84*/	case TESTbrm: {
	    DWORD res;
	    PC += hsw_modrm_32_byte(MODARGS);
	    res = *MEM_REF & *HREG1;
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } goto next_switch; 
/*85*/	case TESTwrm: {
	    DWORD res;
	    PC += hsw_modrm_32_quad(MODARGS);
	    res = mFETCH_QUAD(MEM_REF) & *EREG1;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } goto next_switch; 
/*86*/	case XCHGbrm: {
	    register BYTE temp;
	    PC += hsw_modrm_32_byte(MODARGS);
	    temp = *MEM_REF; *MEM_REF = *HREG1; *HREG1 = temp;
	    } goto next_switch; 
/*87*/	case XCHGwrm: {
	    register DWORD temp;
	    PC += hsw_modrm_32_quad(MODARGS);
	    temp = *EREG1;
	    *EREG1 = mFETCH_QUAD(MEM_REF);
	    mPUT_QUAD(MEM_REF, temp);
		goto next_switch; 
	    }
/*88*/	case MOVbfrm:
	    PC += hsw_modrm_32_byte(MODARGS);
	    mPUT_BYTE(MEM_REF, *HREG1); goto next_switch;
/*89*/	case MOVwfrm:
	    PC += hsw_modrm_32_quad(MODARGS);
	    mPUT_QUAD(MEM_REF, *EREG1); goto next_switch; 
/*8a*/	case MOVbtrm:
	    PC += hsw_modrm_32_byte(MODARGS);
	    *HREG1 = mFETCH_BYTE(MEM_REF); goto next_switch;
/*8b*/	case MOVwtrm:
	    PC += hsw_modrm_32_quad(MODARGS);
	    *EREG1 = mFETCH_QUAD(MEM_REF); goto next_switch;
/*8c*/	case MOVsrtrm: {
	    BYTE seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
		switch (seg_reg) {
		    case 0: /* ES */
			mPUT_WORD(MEM_REF, SHORT_ES_16);
			goto next_switch;
		    case 1: /* CS */
			mPUT_WORD(MEM_REF, SHORT_CS_16);
			goto next_switch;
		    case 2: /* SS */
			mPUT_WORD(MEM_REF, SHORT_SS_16);
			goto next_switch;
		    case 3: /* DS */
			mPUT_WORD(MEM_REF, SHORT_DS_16);
			goto next_switch;
		    case 4: /* FS */
			mPUT_WORD(MEM_REF, SHORT_FS_16);
			goto next_switch;
		    case 5: /* GS */
			mPUT_WORD(MEM_REF, SHORT_GS_16);
			goto next_switch;
		    case 6: /* Illegal */
		    case 7: /* Illegal */
		goto illegal_op; 
			/* trap this */
		}
		}
/*8d*/	case LEA:
	    OVERR_DS = OVERR_SS = 0;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) {
		goto illegal_op;
	    } else {
		/* NOTE gcc uses lea esi,[esi] (8d 76 00) as 3-byte NOP
		 * to align code on dword boundary */
		*EREG1 = (long)MEM_REF;
	    }
	    goto next_switch;
/*8e*/	case MOVsrfrm: {
	    WORD wtemp;
	    BYTE seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
	    wtemp = mFETCH_WORD(MEM_REF);
	    switch (seg_reg) {
		case 0: /* ES */
		    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,wtemp))) {
			env->error_addr=wtemp&0xfffc; return P0; }
		    SHORT_ES_16 = wtemp;
		    goto next_switch;
		case 2: /* SS */
		    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,wtemp))) {
			env->error_addr=wtemp&0xfffc; return P0; }
		    SHORT_SS_16 = wtemp;
		    goto next_switch;
		case 3: /* DS */
		    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,wtemp))) {
			env->error_addr=wtemp&0xfffc; return P0; }
		    SHORT_DS_16 = wtemp;
		    goto next_switch;
		case 4: /* FS */
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,wtemp))) {
			env->error_addr=wtemp&0xfffc; return P0; }
		    SHORT_FS_16 = wtemp;
		    goto next_switch;
		case 5: /* GS */
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,wtemp))) {
			env->error_addr=wtemp&0xfffc; return P0; }
		    SHORT_GS_16 = wtemp;
		    goto next_switch;
		case 1: /* CS */
		case 6: /* Illegal */
		case 7: /* Illegal */
		goto illegal_op; 
		    /* trap this */
		    goto next_switch;
	    }}
/*8f*/	case POPrm: {
	    register DWORD temp;
	    POPQUAD(temp);
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF;
	    PUT_QUAD(mem_ref, temp);
	    } goto next_switch;

/*90*/	case NOP:
	    do PC++; while (*PC==0x90); goto next_switch;
/*91*/	case XCHGcx: {
	    register DWORD temp = EAX;
	    EAX = ECX;
	    ECX = temp;
	    } PC += 1; goto next_switch;
/*92*/	case XCHGdx: {
	    register DWORD temp = EAX;
	    EAX = EDX;
	    EDX = temp;
	    } PC += 1; goto next_switch;
/*93*/	case XCHGbx: {
	    register DWORD temp = EAX;
	    EAX = EBX;
	    EBX = temp;
	    } PC += 1; goto next_switch;
/*94*/	case XCHGsp: {
	    register DWORD temp = EAX;
	    EAX = ESP;
	    ESP = temp;
	    } PC += 1; goto next_switch;
/*95*/	case XCHGbp: {
	    register DWORD temp = EAX;
	    EAX = EBP;
	    EBP = temp;
	    } PC += 1; goto next_switch;
/*96*/	case XCHGsi: {
	    register DWORD temp = EAX;
	    EAX = ESI;
	    ESI = temp;
	    } PC += 1; goto next_switch;
/*97*/	case XCHGdi: {
	    register DWORD temp = EAX;
	    EAX = EDI;
	    EDI = temp;
	    } PC += 1; goto next_switch;
/*98*/	case CBW: /* CWDE */
	    env->rax.x.dummy = ((AX & 0x8000) ? 0xffff : 0);
	    PC += 1; goto next_switch;
/*99*/	case CWD: /* CDQ */
            EDX = (EAX & 0x80000000) ? 0xffffffff : 0;
            PC += 1; goto next_switch;
/*9a*/	case CALLl:
	    goto not_implemented;
/*9b*/	case WAIT:
            PC += 1; goto next_switch;
/*9c*/	case PUSHF: {
	    register DWORD temp;
	    if (VM86F) goto not_permitted;
	    temp = trans_interp_flags(env) & ~(VM_FLAG|RF_FLAG);
	    PUSHQUAD(temp);
	    } PC += 1; goto next_switch;
/*9d*/	case POPF: {
	    register DWORD temp, ef2;
	    if (VM86F) goto not_permitted;
	    ef2 = env->flags & (VM_FLAG|RF_FLAG);
	    POPQUAD(temp);
	    BYTE_FLAG=0;
	    trans_flags_to_interp(env, ((temp & ~(VM_FLAG|RF_FLAG)) | ef2));
	    } PC += 1; goto next_switch;
/*9e*/	case SAHF: {
	    DWORD flags;
	    trans_interp_flags(env);
	    flags = (env->flags & 0xffffff02) | (AH & 0xd5);
	    trans_flags_to_interp(env, flags);
	    } PC += 1; goto next_switch;
/*9f*/	case LAHF: {
	    AH = (BYTE)trans_interp_flags(env);
	    } PC += 1; goto next_switch;

/*a0*/	case MOVmal: {
	    mem_ref = OVERR_DS + FETCH_QUAD((PC+1));
	    AL = *mem_ref;
	    } PC += 5; goto next_switch;
/*a1*/	case MOVmax: {
	    mem_ref = OVERR_DS + FETCH_QUAD((PC+1));
	    EAX = FETCH_QUAD(mem_ref);
	    } PC += 5; goto next_switch;
/*a2*/	case MOValm: {
	    mem_ref = OVERR_DS + FETCH_QUAD((PC+1));
	    *mem_ref = AL;
	    } PC += 5; goto next_switch;
/*a3*/	case MOVaxm: {
	    register int temp = EAX;
	    mem_ref = OVERR_DS + FETCH_QUAD((PC+1));
	    PUT_QUAD(mem_ref, temp);
	    } PC += 5; goto next_switch;
/*a4*/	case MOVSb: {
	    BYTE *src, *dest;
	    src = OVERR_DS + (ESI);
	    dest = LONG_ES + (EDI);
	    *dest = *src;
	    (env->flags & DIRECTION_FLAG)?(ESI--,EDI--):(ESI++,EDI++);
	    } PC += 1; goto next_switch;
/*a5*/	case MOVSw: {
	    BYTE *src, *dest;
	    DWORD temp;
	    src = OVERR_DS + (ESI);
	    dest = LONG_ES + (EDI);
	    temp = FETCH_QUAD(src);
	    PUT_QUAD(dest, temp);
	    (env->flags & DIRECTION_FLAG)?(ESI-=4,EDI-=4):(ESI+=4,EDI+=4);
	    } PC += 1; goto next_switch;
/*a6*/	case CMPSb: {
	    DWORD src1, src2; int res;
	    BYTE *src, *dest;
	    src = OVERR_DS + (ESI);
	    dest = LONG_ES + (EDI);
	    src1 = *src;
	    src2 = *dest;
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(ESI--,EDI--):(ESI++,EDI++);
	    SETBFLAGS(1);
	    } PC += 1; goto next_switch;
/*a7*/	case CMPSw: {
	    DWORD res, src1;
	    long src2;
	    BYTE *src, *dest;
	    src = OVERR_DS + (ESI);
	    dest = LONG_ES + (EDI);
	    src1 = FETCH_QUAD(src);
	    src2 = FETCH_QUAD(dest);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(ESI-=4,EDI-=4):(ESI+=4,EDI+=4);
	    SETDFLAGS(1,1);
	    } PC += 1; goto next_switch;
/*a8*/	case TESTbi: {
	    DWORD res;
	    res = AL & PC[1];
	    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
	    } PC += 2; goto next_switch;
/*a9*/	case TESTwi: {
	    DWORD res;
	    res = FETCH_QUAD((PC+1)) & EAX;
	    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
	    } PC += 5; goto next_switch;
/*aa*/	case STOSb:
	    LONG_ES[EDI] = AL;
	    (env->flags & DIRECTION_FLAG)?EDI--:EDI++;
	    PC += 1; goto next_switch;
/*ab*/	case STOSw:
	    PUT_QUAD(LONG_ES+EDI, EAX);
	    (env->flags & DIRECTION_FLAG)?(EDI-=4):(EDI+=4);
	    PC += 1; goto next_switch;
/*ac*/	case LODSb: {
	    BYTE *seg;
	    seg = OVERR_DS + (ESI);
	    AL = *seg;
	    (env->flags & DIRECTION_FLAG)?ESI--:ESI++;
	    } PC += 1; goto next_switch;
/*ad*/	case LODSw: {
	    BYTE *seg;
	    seg = OVERR_DS + (ESI);
	    EAX = FETCH_QUAD(seg);
	    (env->flags & DIRECTION_FLAG)?(ESI-=4):(ESI+=4);
	    } PC += 1; goto next_switch;
/*ae*/	case SCASb: {
	    DWORD src1, src2; int res;
	    src1 = AL;
	    src2 = (LONG_ES[EDI]);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?EDI--:EDI++;
	    SETBFLAGS(1);
	    } PC += 1; goto next_switch;
/*af*/	case SCASw: {
	    DWORD res, src1;
	    long src2;
	    src1 = EAX;
	    mem_ref = LONG_ES + (EDI);
	    src2 = FETCH_QUAD(mem_ref);
	    res = src1 - src2;
	    SETDFLAGS(1,1);
	    (env->flags & DIRECTION_FLAG)?(EDI-=4):(EDI+=4);
	    } PC += 1; goto next_switch;

/*b0*/	case MOVial:
	    AL = *(PC+1);
	    PC += 2; goto next_switch;
/*b1*/	case MOVicl:
	    CL = *(PC+1);
	    PC += 2; goto next_switch;
/*b2*/	case MOVidl:
	    DL = *(PC+1);
	    PC += 2; goto next_switch;
/*b3*/	case MOVibl:
	    BL = *(PC+1);
	    PC += 2; goto next_switch;
/*b4*/	case MOViah:
	    AH = *(PC+1); 
	    PC += 2; goto next_switch;
/*b5*/	case MOVich:
	    CH = *(PC+1); 
	    PC += 2; goto next_switch;
/*b6*/	case MOVidh:
	    DH = *(PC+1);
	    PC += 2; goto next_switch;
/*b7*/	case MOVibh:
	    BH = *(PC+1);
	    PC += 2; goto next_switch;
/*b8*/	case MOViax:
	    EAX = FETCH_QUAD((PC+1));
	    PC += 5; goto next_switch;
/*b9*/	case MOVicx:
	    ECX = FETCH_QUAD((PC+1));
	    PC += 5; goto next_switch;
/*ba*/	case MOVidx:
	    EDX = FETCH_QUAD((PC+1));
	    PC += 5; goto next_switch;
/*bb*/	case MOVibx:
	    EBX = FETCH_QUAD((PC+1));
	    PC += 5; goto next_switch;
/*bc*/	case MOVisp:
	    ESP = FETCH_QUAD((PC+1)); 
	    PC += 5; goto next_switch;
/*bd*/	case MOVibp:
	    EBP = FETCH_QUAD((PC+1)); 
	    PC += 5; goto next_switch;
/*be*/	case MOVisi:
	    ESI = FETCH_QUAD((PC+1));
	    PC += 5; goto next_switch;
/*bf*/	case MOVidi:
	    EDI = FETCH_QUAD((PC+1));
	    PC += 5; goto next_switch;

/*d0*/	case SHIFTb:
/*d2*/	case SHIFTbv:
/*c0*/	case SHIFTbi: {
	    int temp, count;
	    DWORD rbef, raft; BYTE opc;
	    opc = *PC;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(MODARGS);
	    if (opc==SHIFTb) count = 1;
	      else if (opc==SHIFTbv) count = ECX & 0x1f;
	        else { count = *PC & 0x1f; PC += 1; }
	    /* are we sure that for count==0 CF is not changed? */
	    if (count) {
		mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			rbef = *mem_ref;
			SRC1_8 = SRC2_8 = rbef;		/* OF: 00x or 11x */
			count &= 7;	/* rotation modulo 8 */
			raft = (rbef << count) | (rbef >> (8 - count));
			*mem_ref = RES_8 = (BYTE)raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (BYTE)(raft & 1);
			goto next_switch;
		    case 1: /* ROR */
			rbef = *mem_ref; 
			SRC1_8 = SRC2_8 = rbef;
			count &= 7;
			raft = (rbef >> count) | (rbef << (8 - count));
			*mem_ref = RES_8 = (BYTE)raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (BYTE)((raft >> 7) & 1);
			goto next_switch;
		    case 2: /* RCL */
			rbef = *mem_ref; 
			SRC1_8 = SRC2_8 = rbef;
			count &= 7;
			raft = (rbef << count) | ((rbef >> (9 - count))
			    | ((CARRY & 1) << (count - 1)));
			*mem_ref = RES_8 = (BYTE)raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (BYTE)((rbef >> (8 - count)) & 1);
			goto next_switch;
		    case 3: /* RCR */
			rbef = *mem_ref; 
			SRC1_8 = SRC2_8 = rbef;
			count &= 7;
			raft = (rbef >> count) | ((rbef << (9 - count))
			    | ((CARRY & 1) << (8 - count)));
			*mem_ref = RES_8 = (BYTE)raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (BYTE)((rbef >> (count - 1)) & 1);
			goto next_switch;
		    case 6: /* Undocumented */
		        if (opc==SHIFTbv) goto illegal_op;
			/* fall thru for SHIFTbi */
		    case 4: /* SHL/SAL */
			temp = *mem_ref;
			SRC1_8 = SRC2_8 = temp;
			CARRYB = (temp >> (8 - count)) & 1;
			temp = (temp << count);
			*mem_ref = temp;
			/* if cnt==1 OF=1 if CF after shift != MSB 1st op */
			RES_8 = temp; BYTE_FLAG = BYTE_OP;
			goto next_switch;
		    case 5: /* SHR */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			CARRYB = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			/* if cnt==1 OF=1 if MSB was changed */
			RES_8 = temp; BYTE_FLAG = BYTE_OP;
			goto next_switch;
		    case 7: /* SAR */
			temp = *(signed char *)mem_ref; 
			CARRYB = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = (BYTE)temp;
			/* if cnt==1 OF=0 */
			SRC1_8 = SRC2_8 = RES_8 = (BYTE)temp;
			BYTE_FLAG = BYTE_OP;
			goto next_switch;
		    default: goto not_implemented;
		} } else  goto next_switch;
	    }
/*d1*/	case SHIFTw:
/*d3*/	case SHIFTwv:
/*c1*/	case SHIFTwi: {
	    register long temp; int count;
	    DWORD src1, src2, res; BYTE opc;
	    opc = *PC;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF;
	    if (opc==SHIFTw) count = 1;
	      else if (opc==SHIFTwv) count = ECX & 0x1f;
	        else { count = *PC & 0x1f; PC += 1; }
	    /* are we sure that for count==0 CF is not changed? */
	    if (count) {
		mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			src1 = mFETCH_QUAD(mem_ref);
			res = (src1 << count) | (src1 >> (32 - count));
			PACK32_16(src1,SRC1_16); SRC2_16=SRC1_16;
			mPUT_QUAD(mem_ref,res);
			CARRY = (WORD)(res & 1);	/* -> BYTE_FLAG=0 */
			PACK32_16(res,RES_16);
			goto next_switch;
		    case 1: /* ROR */
			src1 = mFETCH_QUAD(mem_ref);
			res = (src1 >> count) | (src1 << (32 - count));
			PACK32_16(src1,SRC1_16); SRC2_16=SRC1_16;
			mPUT_QUAD(mem_ref,res);
			CARRY = (WORD)((res >> 31) & 1);
			PACK32_16(res,RES_16);
			goto next_switch;
		    case 2: /* RCL */
			src1 = mFETCH_QUAD(mem_ref);
			res = (src1 << count) | ((CARRY & 1) << (count - 1));
			if (count > 1) res |= (src1 >> (33 - count));
			PACK32_16(src1,SRC1_16); SRC2_16=SRC1_16;
			mPUT_QUAD(mem_ref,res);
			CARRY = (WORD)((src1 >> (32 - count)) & 1);
			PACK32_16(res,RES_16);
			goto next_switch;
		    case 3: /* RCR */
			src1 = mFETCH_QUAD(mem_ref);
			res = (src1 >> count) | ((CARRY & 1) << (32 - count));
                        if (count > 1) res |= (src1 << (33 - count));
			PACK32_16(src1,SRC1_16); SRC2_16=SRC1_16;
			mPUT_QUAD(mem_ref,res);
			CARRY = (WORD)((src1 >> (count - 1)) & 1);
			PACK32_16(res,RES_16);
			goto next_switch;
		    case 6: /* Undocumented */
		        if ((opc==SHIFTw)||(opc==SHIFTwv))
			    goto illegal_op;
			/* fall thru for SHIFTwi */
		    case 4: /* SHL/SAL */
			res = mFETCH_QUAD(mem_ref);
			src1 = src2 = res;
			CARRY = (WORD)((res >> (32-count)) & 0x1);
			res = (res << count);
			mPUT_QUAD(mem_ref,res);
			SETDFLAGS(0,0);
			goto next_switch;
		    case 5: /* SHR */
			res = mFETCH_QUAD(mem_ref);
			src1 = src2 = res;
			CARRY = (WORD)((res >> (count-1)) & 1);
			res = res >> count;
			mPUT_QUAD(mem_ref,res);
			SETDFLAGS(0,0);
			goto next_switch;
		    case 7: { /* SAR */
			signed long res1;
			res1 = mFETCH_QUAD(mem_ref);
			CARRY = (res1 >> (count-1)) & 1;
			src1 = src2 = res = (res1 >> count);
			mPUT_QUAD(mem_ref,res);
			SETDFLAGS(0,0);
			goto next_switch;
			}
		    default: goto not_implemented;
	      } } else  goto next_switch;
	    }
/*c2*/	case RETisp: {
	    DWORD ip, dr;
	    POPQUAD(ip);
	    dr = (signed short)(FETCH_WORD((PC+1)));
	    if (BIG_SS) ESP+=dr; else SP+=(WORD)dr;
	    PC = LONG_CS + ip;
	    } goto next_switch;
/*c3*/	case RET: {
	    DWORD ip;
	    POPQUAD(ip);
	    PC = LONG_CS + ip;
	    } goto next_switch;
/*c4*/	case LES: {
	    register DWORD temp;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_WORD(mem_ref+4);
	    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
		env->error_addr=temp&0xfffc; return P0; }
	    *EREG1 = FETCH_QUAD(mem_ref);
	    SHORT_ES_32 = temp;
	    } goto next_switch;
/*c5*/	case LDS: {
	    register DWORD temp;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_WORD(mem_ref+4);
	    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
		env->error_addr=temp&0xfffc; return P0; }
	    *EREG1 = FETCH_QUAD(mem_ref);
	    SHORT_DS_32 = temp;
	    } goto next_switch;
/*c6*/	case MOVbirm:
	    PC += hsw_modrm_32_byte(MODARGS);
	    *MEM_REF = *PC; PC += 1; goto next_switch;
/*c7*/	case MOVwirm: {
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF;
	    *(DWORD *)mem_ref = FETCH_QUAD(PC);
	    PC += 4; goto next_switch;
	    } 
/*c8*/	case ENTER: {
	    BYTE *sp, *bp, *ss;
	    DWORD FrameTemp;
	    BYTE level = *(PC+3) & 0x1f;
	    ss = LONG_SS;
	    if (BIG_SS) {
		sp = ss + ESP - 4;
		bp = ss + EBP;		/* Intel manual */
	    } else {
		sp = ss + SP - 4;
		bp = ss + BP;
	    }
	    PUSHQUAD(bp-ss);		/* -> sp-=4 */
	    FrameTemp = sp - ss;
	    if (level) {
		sp -= (4 + 4*level);
		while (level--) {
		    bp -= 4; 
		    PUSHQUAD(bp-ss);
		}
		PUSHQUAD(FrameTemp);
	    }
	    EBP = FrameTemp;
	    sp -= (FETCH_WORD((PC + 1)));
	    if (BIG_SS) ESP=sp-ss; else SP=sp-ss;
	    } PC += 4; goto next_switch;
/*c9*/	case LEAVE: {
	    register DWORD temp;
	    ESP = EBP;
	    POPQUAD(temp);
	    EBP = temp;
	    } PC += 1; goto next_switch;
/*ca*/	case RETlisp: { /* restartable */
	    DWORD cs, ip; int dr;
	    WORD transfer_magic;
	    POPQUAD(ip);
	    cs = TOS_WORD;
	    dr = (signed short)FETCH_WORD((PC+1));
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		PUSHQUAD(ip); env->error_addr=cs&0xfffc; return P0; }
	    POPQUAD(cs); cs &= 0xffff;
	    SHORT_CS_16 = (WORD)cs;
	    if (BIG_SS) ESP+=dr; else SP+=dr;
	    PC = ip + LONG_CS;
	    if (transfer_magic == TRANSFER_CODE32) {
		goto next_switch;
	    } else if (transfer_magic == TRANSFER_CODE16) {
		*err = EXCP_GOBACK;
		return (PC);
	    }
	    else
	      invoke_data(env);    /* TRANSFER_DATA or garbage */
	    }
/*cb*/	case RETl: { /* restartable */
	    DWORD cs, ip;
	    WORD transfer_magic;
	    POPQUAD(ip);
	    cs = TOS_WORD;
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		PUSHQUAD(ip); env->error_addr=cs&0xfffc; return P0; }
	    POPQUAD(cs); cs &= 0xffff;
	    SHORT_CS_16 = (WORD)cs;
	    PC = ip + LONG_CS;
	    if (transfer_magic == TRANSFER_CODE32) {
		goto next_switch;
	    } else if (transfer_magic == TRANSFER_CODE16) {
		*err = EXCP_GOBACK;
		return (PC);
	    }
	    else
	      invoke_data(env);    /* TRANSFER_DATA or garbage */
	    } 
/*cc*/	case INT3:		/* cc */
	    *err = EXCP03_INT3;
	    PC += 1; return PC;
/*cd*/	case INT:		/* cd */
	    *err = (PC[1]==1? EXCP01_SSTP:EXCP0D_GPF);
	    return P0;
/*ce*/	case INTO:		/* ce */
	    goto not_implemented;
/*cf*/	case IRET: { /* restartable */
	    DWORD cs, ip, flags;
	    WORD transfer_magic;
	    POPQUAD(ip);
	    cs = TOS_WORD;
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		PUSHQUAD(ip); env->error_addr=cs&0xfffc; return P0; }
	    POPQUAD(cs); cs &= 0xffff;
	    POPQUAD(flags);
	    trans_flags_to_interp(env, flags);
	    SHORT_CS_16 = (WORD)cs;
	    PC = ip + LONG_CS;
	    if (transfer_magic == TRANSFER_CODE32) {
		if (env->flags & TRAP_FLAG) {
		    return (PC);
		}
		goto next_switch;
	    } else if (transfer_magic == TRANSFER_CODE16) {
		*err = EXCP_GOBACK;
		return (PC);
	    }
	    else
	      invoke_data(env);    /* TRANSFER_DATA or garbage */
	    } 

/*d0*/	    /* see before */
/*d1*/	    /* see before */
/*d2*/	    /* see before */
/*d3*/	    /* see before */
/*d4*/	case AAM: {
	    int base = PC[1];
	    AH = AL / base;
	    RES_8 = AL = AL % base;
	    BYTE_FLAG=BYTE_OP;
	    } PC += 2; goto next_switch;
/*d5*/	case AAD: {
	    int base = PC[1];
	    AX = ((AH * base) + AL) & 0xff;
	    RES_8 = AL; /* for flags */
	    BYTE_FLAG=BYTE_OP;
	    } PC += 2; goto next_switch;
/*d6*/	case 0xd6:	/* SETALC, Undocumented */
	    AL = (CARRY & 1? 0xff: 0x00);
	    PC += 1; goto next_switch;
/*d7*/	case XLAT: {
	    mem_ref = OVERR_DS + (EBX) + (AL);
	    AL = *mem_ref; }
	    PC += 1; goto next_switch;

/*d8*/	case ESC0:
/*d9*/	case ESC1:
/*da*/	case ESC2:
/*db*/	case ESC3:
/*dc*/	case ESC4:
/*dd*/	case ESC5:
/*de*/	case ESC6:
/*df*/	case ESC7:
    {
    switch(*PC){
/*d8*/	case ESC0: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg0[funct](reg);
	    else hsw_fp87_mem0[funct](MEM_REF);
	    } break;
/*d9*/	case ESC1: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg1[funct](reg);
	    else hsw_fp87_mem1[funct](MEM_REF);
	    } break;
/*da*/	case ESC2: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg2[funct](reg);
	    else hsw_fp87_mem2[funct](MEM_REF);
	    } break;
/*db*/	case ESC3: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg3[funct](reg);
	    else hsw_fp87_mem3[funct](MEM_REF);
	    } break;
/*dc*/	case ESC4: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg4[funct](reg);
	    else hsw_fp87_mem4[funct](MEM_REF);
	    } break;
/*dd*/	case ESC5: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg5[funct](reg);
	    else hsw_fp87_mem5[funct](MEM_REF);
	    } break;
/*de*/	case ESC6: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg6[funct](reg);
	    else hsw_fp87_mem6[funct](MEM_REF);
	    } break;
/*df*/	case ESC7: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_quad(MODARGS);
	    if (IS_MODE_REG) hsw_fp87_reg7[funct](reg);
	    else hsw_fp87_mem7[funct](MEM_REF);
	    } break;
    }
#ifndef NO_TRACE_MSGS
    if (d_emu>3) e_trace_fp(&hsw_env87);
#endif
    if (*PC == WAIT) PC += 1;
    goto next_switch; }

/*e0*/	case LOOPNZ_LOOPNE: 
	    if ((--ECX != 0) && (!IS_ZF_SET)) {
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
/*e1*/	case LOOPZ_LOOPE: 
	    if ((--ECX != 0) && (IS_ZF_SET)) {
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
/*e2*/	case LOOP: 
	    if (--ECX != 0) {
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
/*e3*/	case JCXZ: 
	    if (ECX == 0) {
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
/*e4*/	case INb: {
	      BYTE a = *(PC+1);
	      /* We CAN't allow unlimited port access even when IOPL=0 -
	       * all MUST be filtered through the I/O bitmap */
	      /*if (VM86F || (CPL > IOPL)) {*/
		if (!test_bit(a, io_bitmap)) goto not_permitted;
	      AL = port_real_inb(a);
	      PC += 2; goto next_switch;
	    }
/*e5*/	case INw: {
	      BYTE a = *(PC+1);
		if ((a&3)||(!test_bit(a+3, io_bitmap))) goto not_permitted;
	      EAX = port_real_ind(a);
	      PC += 2; goto next_switch;
	    }
/*e6*/	case OUTb: {
	      BYTE a = *(PC+1);
		if (!test_bit(a, io_bitmap)) goto not_permitted;
	      port_real_outb(a, AL);
	      PC += 2; goto next_switch;
	    }
/*e7*/	case OUTw: {
	      BYTE a = *(PC+1);
		if ((a&3)||(!test_bit(a+3, io_bitmap))) goto not_permitted;
	      port_real_outw(a, AX);
	      PC += 2; goto next_switch;
	    }
/*ec*/	case INvb: {
#if defined(DOSEMU) && defined(X_SUPPORT)
		if (emu_under_X) {
		  extern BYTE VGA_emulate_inb(WORD);
		  switch(DX) {
		    case 0x3d4:	/*CRTC_INDEX*/
		    case 0x3d5:	/*CRTC_DATA*/
		    case 0x3c4:	/*SEQUENCER_INDEX*/
		    case 0x3c5:	/*SEQUENCER_DATA*/
		    case 0x3c0:	/*ATTRIBUTE_INDEX*/
		    case 0x3c1:	/*ATTRIBUTE_DATA*/
		    case 0x3c6:	/*DAC_PEL_MASK*/
		    case 0x3c7:	/*DAC_STATE*/
		    case 0x3c8:	/*DAC_WRITE_INDEX*/
		    case 0x3c9:	/*DAC_DATA*/
		    case 0x3da:	/*INPUT_STATUS_1*/
			AL = VGA_emulate_inb(DX);
			PC += 1; goto next_switch;
		    default: break;
		  }
		}
#endif
		if ((DX>0x3ff)||(!test_bit(DX, io_bitmap))) goto not_permitted;
	      AL = port_real_inb(DX);
	      PC += 1; goto next_switch;
	    }
/*ed*/	case INvw: {
		if ((DX>0x3fc)||(DX&3)||(!test_bit(DX+3, io_bitmap))) goto not_permitted;
	      EAX = port_real_ind(DX);
	      PC += 1; goto next_switch;
	    }
/*ee*/	case OUTvb: {
#if defined(DOSEMU) && defined(X_SUPPORT)
		if (emu_under_X) {
		  extern void VGA_emulate_outb(WORD,BYTE);
		  switch(DX) {
		    case 0x3d4:	/*CRTC_INDEX*/
		    case 0x3d5:	/*CRTC_DATA*/
		    case 0x3c4:	/*SEQUENCER_INDEX*/
		    case 0x3c5:	/*SEQUENCER_DATA*/
		    case 0x3c0:	/*ATTRIBUTE_INDEX*/
		    case 0x3c6:	/*DAC_PEL_MASK*/
		    case 0x3c7:	/*DAC_READ_INDEX*/
		    case 0x3c8:	/*DAC_WRITE_INDEX*/
		    case 0x3c9:	/*DAC_DATA*/
			VGA_emulate_outb(DX,AL);
			PC += 1; goto next_switch;
		    default: break;
		  }
		}
#endif
		if ((DX>0x3ff)||(!test_bit(DX, io_bitmap))) goto not_permitted;
	      port_real_outb(DX, AL);
	      PC += 1; goto next_switch;
	    }
/*ef*/	case OUTvw: {
		if ((DX>0x3fc)||(DX&3)||(!test_bit(DX+3, io_bitmap))) goto not_permitted;
	      port_real_outd(DX, EAX);
	      PC += 1; goto next_switch;
	    }

/*e8*/	case CALLd: {
	    DWORD ip = PC - LONG_CS + 5;
	    PUSHQUAD(ip);
	    PC = LONG_CS + (ip + (signed long)(FETCH_QUAD(PC+1)));
	    } goto next_switch;
/*e9*/	case JMPd: {
	    DWORD ip = PC - LONG_CS + 5;
	    PC = LONG_CS + (ip + (signed long)(FETCH_QUAD(PC+1)));
	    } goto next_switch;
/*ea*/	case JMPld: {
	    DWORD cs, ip;
	    WORD transfer_magic;
	    ip = FETCH_QUAD(PC+1);
	    cs = (FETCH_QUAD(PC+5) & 0xffff);
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		env->error_addr=cs&0xfffc; return P0; }
	    SHORT_CS_16 = (WORD)cs;
	    PC = ip + LONG_CS;
	    if (transfer_magic == TRANSFER_CODE32) {
		goto next_switch;
	    } else if (transfer_magic == TRANSFER_CODE16) {
		*err = EXCP_GOBACK;
		return (PC);
	    }
	    else {
	      invoke_data(env);    /* TRANSFER_DATA or garbage */
	    }
	    }
/*eb*/	case JMPsid: 
/**/	    if (PC[1]==0xfe) {
		FatalAppExit(0, "LOOP");
		exit(1);
	    }
	    JUMP((PC+1)); goto fast_switch;

/*f0*/	case LOCK:
	    CEmuStat |= CeS_LOCK;
	    PC += 1; goto next_switch;
	case 0xf1:    /* illegal on 8086 and 80x86 */
	    goto illegal_op;
/*f2*/	case REPNE:
/*f3*/	case REP:     /* also is REPE */
{
	    unsigned int count = ECX;
	    int longd = 4;
	    BYTE repop,test;
	    DWORD res, src1=0, oldcnt; long src2=0;

	    repop = (*PC-REPNE);	/* 0 test !=, 1 test == */
	    EMUtime += (count * CYCperINS);
	    PC += 2;
segrep:
	    switch (*(PC-1)) {
		case INSb: {
		    BYTE *dest;
		    if (count == 0) goto next_switch;
			if ((DX>0x3ff)||(!test_bit(DX, io_bitmap))) goto not_permitted;
		    instr_count += count;
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count;
			while (count--) *dest-- = port_real_inb(DX);
		    } else {
			EDI += count;
			while (count--) *dest++ = port_real_inb(DX);
		    }
		    ECX = 0; goto next_switch;
		    }
		case INSw: {
		    int lcount = count * longd;
		    if (count == 0) goto next_switch;
			if ((DX>0x3fc)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
		    if (env->flags & DIRECTION_FLAG) lcount = -lcount;
		    instr_count += count;
		    if (longd==2) {
		      WORD *dest = (WORD *)(LONG_ES + EDI);
		      if (env->flags & DIRECTION_FLAG) {
			while (count--) *dest-- = port_real_inw(DX);
		      } else {
			while (count--) *dest++ = port_real_inw(DX);
		      }
		    }
		    else {
		      DWORD *dest = (DWORD *)(LONG_ES + EDI);
		      if (env->flags & DIRECTION_FLAG) {
			while (count--) *dest-- = port_real_ind(DX);
		      } else {
			while (count--) *dest++ = port_real_ind(DX);
		      }
		    }
		    EDI += lcount; ECX = 0; goto next_switch;
		    }
		case OUTSb: {
		    BYTE *src;
		    if (count == 0) goto next_switch;
#if defined(DOSEMU) && defined(X_SUPPORT)
			if (emu_under_X) {
			  extern void VGA_emulate_outb(WORD,BYTE);
			  switch(DX) {
			    case 0x3d4:	/*CRTC_INDEX*/
			    case 0x3d5:	/*CRTC_DATA*/
			    case 0x3c4:	/*SEQUENCER_INDEX*/
			    case 0x3c5:	/*SEQUENCER_DATA*/
			    case 0x3c0:	/*ATTRIBUTE_INDEX*/
			    case 0x3c6:	/*DAC_PEL_MASK*/
			    case 0x3c7:	/*DAC_READ_INDEX*/
			    case 0x3c8:	/*DAC_WRITE_INDEX*/
			    case 0x3c9:	/*DAC_DATA*/
				src = LONG_DS + ESI;
				if (env->flags & DIRECTION_FLAG) {
				    ESI -= count;
				    while (count--) VGA_emulate_outb(DX, *src--);
				} else {
				    ESI += count;
				    while (count--) VGA_emulate_outb(DX, *src++);
				}
				ECX = 0; goto next_switch;
			    default: break;
			  }
			}
#endif
			if ((DX>0x3ff)||(!test_bit(DX, io_bitmap))) goto not_permitted;
		    instr_count += count;
		    src = LONG_DS + ESI;
		    if (env->flags & DIRECTION_FLAG) {
			ESI -= count;
			while (count--) port_real_outb(DX, *src--);
		    } else {
			ESI += count;
			while (count--) port_real_outb(DX, *src++);
		    }
		    ECX = 0; goto next_switch;
		    }
		case OUTSw: {
		    int lcount = count * longd;
		    if (count == 0) goto next_switch;
			if ((DX>0x3fc)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
		    if (env->flags & DIRECTION_FLAG) lcount = -lcount;
		    instr_count += count;
		    if (longd==2) {
		      WORD *src = (WORD *)(LONG_DS + ESI);
		      if (env->flags & DIRECTION_FLAG) {
			while (count--) port_real_outw(DX, *src--);
		      } else {
			while (count--) port_real_outw(DX, *src++);
		      }
		    }
		    else {
		      DWORD *src = (DWORD *)(LONG_DS + ESI);
		      if (env->flags & DIRECTION_FLAG) {
			while (count--) port_real_outd(DX, *src--);
		      } else {
			while (count--) port_real_outd(DX, *src++);
		      }
		    }
		    ESI += lcount; ECX = 0; goto next_switch;
		    }
		case MOVSb: {
		    BYTE *src, *dest;
		    if (count == 0) goto next_switch;
		    instr_count += count;
		    src = OVERR_DS + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; ECX = 0;
			while (count--) *dest-- = *src--;
			goto next_switch;
		    } else {
			EDI += count; ESI += count; ECX = 0;
			while (count--) *dest++ = *src++;
			goto next_switch;
		    } } 
		case MOVSw: { /* MOVSD */
		    int lcount = count * longd;
		    if (count == 0) goto next_switch;
		    instr_count += count;
		    if (env->flags & DIRECTION_FLAG) lcount = -lcount;
		    if (longd==2) {
		      WORD *src, *dest;
		      src = (WORD *)(OVERR_DS+(ESI));
		      dest = (WORD *)(LONG_ES + EDI);
		      if (lcount < 0) {
			while (count--) *dest-- = *src--;
		      } else {
			while (count--) *dest++ = *src++;
		      }
		    } else {
		      DWORD *src, *dest;
		      src = (DWORD *)(OVERR_DS+(ESI));
		      dest = (DWORD *)(LONG_ES + EDI);
		      if (lcount < 0) {
			while (count--) *dest-- = *src--;
		      } else {
			while (count--) *dest++ = *src++;
		      }
		    } 
		    EDI += lcount; ESI += lcount; ECX = 0;
		    goto next_switch;
		    }
		case CMPSb: {
		    BYTE *src, *dest, *ovr;
		    if (count == 0) goto next_switch;
		    oldcnt = count;
		    src = (ovr=OVERR_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = *src--;
			    src2 = *dest--;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
				res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				SETBFLAGS(1);
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = *src++;
			    src2 = *dest++;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
				res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				SETBFLAGS(1);
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0; EDI = dest - LONG_ES;
		    ESI = src - ovr;
		    SETBFLAGS(1);
		    } goto next_switch;
		case CMPSw: {
		    BYTE *src, *dest, *ovr;
		    if (count == 0) goto next_switch;
		    oldcnt = count;
		    src = (ovr=OVERR_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    if (longd==4) {
			      src1 = FETCH_QUAD(src);
			      src2 = FETCH_QUAD(dest);
			    }
			    else {
			      src1 = FETCH_WORD(src);
			      src2 = FETCH_WORD(dest);
			    }
			    src -= longd; dest -= longd;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
				res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    if (longd==4) {
			      src1 = FETCH_QUAD(src);
			      src2 = FETCH_QUAD(dest);
			    }
			    else {
			      src1 = FETCH_WORD(src);
			      src2 = FETCH_WORD(dest);
			    }
			    src += longd; dest += longd;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ovr;
		    if (longd==2) {
		      SETWFLAGS(1);
		    }
		    else {
		      SETDFLAGS(1,1);
		    }
		    } goto next_switch;
		case STOSb: {
		    BYTE *dest;
		    register BYTE al;
		    if (count == 0) goto next_switch;
		    instr_count += count;
		    dest = LONG_ES + EDI;
		    al = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count; ECX = 0;
			while (count--) *dest-- = al;
			goto next_switch;
		    } else {		      /* forwards */
			EDI += count; ECX = 0;
			while (count--) *dest++ = al;
			goto next_switch;
		    } } 
		case STOSw: {
		    BYTE *dest;
		    register DWORD ddat = EAX;
		    if (count == 0) goto next_switch;
		    instr_count += count;
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count * longd; ECX = 0;
			if (longd==4)
			    while (count--) {PUT_QUAD(dest,ddat); dest-=4;}
			else
			    while (count--) {PUT_WORD(dest,ddat); dest-=2;}
			goto next_switch;
		    } else {		      /* forwards */
			EDI += count * longd; ECX = 0;
			if (longd==4)
			    while (count--) {PUT_QUAD(dest,ddat); dest+=4;}
			else
			    while (count--) {PUT_WORD(dest,ddat); dest+=2;}
			goto next_switch;
		    } }
		case LODSb:
		    goto not_implemented;
		case LODSw:
		    goto not_implemented;
		case SCASb: {
		    BYTE *dest;
		    if (count == 0) goto next_switch;
		    oldcnt = count;
		    dest = LONG_ES + EDI;
		    src1 = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = *dest--;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
				res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				SETBFLAGS(1);
				if(((int)src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = *dest++;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
				res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				SETBFLAGS(1);
				if(((int)src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0; EDI = dest - LONG_ES;
		    SETBFLAGS(1);
		    if(((int)src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } goto next_switch;
		case SCASw: {
		    BYTE *dest;
		    if (count == 0) goto next_switch;
		    oldcnt = count;
		    dest = LONG_ES + EDI;
		    src1 = EAX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = (longd==4? FETCH_QUAD(dest) : FETCH_WORD(dest));
			    dest -= longd;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = (longd==4? FETCH_QUAD(dest) : FETCH_WORD(dest));
			    dest += longd;
			    test = ((int)src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				goto next_switch;
			    }
			}
		    } 
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0; EDI = dest - LONG_ES;
		    if (longd==2) {
		      SETWFLAGS(1);
		    }
		    else {
		      SETDFLAGS(1,1);
		    }
		    } goto next_switch;
/*----------IA--------------------------------*/
	        case SEGes:
		    OVERR_DS = OVERR_SS = LONG_ES;
            	    PC+=1; goto segrep;
        	case SEGcs:
		    OVERR_DS = OVERR_SS = LONG_CS;
          	    PC+=1; goto segrep;
        	case SEGss:
		    OVERR_DS = OVERR_SS = LONG_SS;
            	    PC+=1; goto segrep;
        	case SEGds:
		    OVERR_DS = OVERR_SS = LONG_DS;
            	    PC+=1; goto segrep;
        	case SEGfs:
		    OVERR_DS = OVERR_SS = LONG_FS;
            	    PC+=1; goto segrep;
        	case SEGgs:
		    OVERR_DS = OVERR_SS = LONG_GS;
            	    PC+=1; goto segrep;
		case OPERoverride:	/* 0x66 */
            	    longd = 2;
            	    PC+=1; goto segrep;
		case ADDRoverride:	/* 0x67 */
		    PC=P0-1;
		    code32 = 0;
		    if (longd==2) {	/* 0x66 0x67 */
			data32 = 0;
			PC = hsw_interp_16_16 (env, P0, PC+1, err, 1);
			data32 = 1;
		    }
		    else		/* 0x67 (0x66) */
			PC = hsw_interp_32_16 (env, P0, PC+1, err);
		    code32 = 1;
		    if (*err) return (*err==EXCP_GOBACK? PC:P0);
		    goto next_switch;
/*----------IA--------------------------------*/
		default: PC--; goto not_implemented;
	    } }

/*f4*/	case HLT:
	    goto not_permitted;
/*f5*/	case CMC:
	    CARRY ^= CARRY_FLAG;
	    PC += 1; goto next_switch;
/*f6*/	case GRP1brm: {
	    DWORD src1, src2; int res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(MODARGS);
	    mem_ref = MEM_REF; 
	    switch (res) {
		case 0: /* TEST */
		case 1: /* Undocumented */
		    res = *mem_ref & *PC; PC += 1;
		    SRC1_32 = SRC2_32 = RES_32 = BYTEDUP(res);	/* CF=OF=0 */
		    goto next_switch;
		case 2: /* NOT */
		    src1 = ~(*mem_ref);
		    *mem_ref = (BYTE)src1;
		    goto next_switch;
		case 3: /* NEG */
		    src1 = 0; src2 = *mem_ref;
		    res = (BYTE)(0x100-src2);
		    *mem_ref = (BYTE)res;
		    SETBFLAGS(1);
		    CARRYB = (src2 != 0);	/* not CARRY! */
		    goto next_switch;
		case 4: /* MUL AL */
		    src1 = *mem_ref;
		    src2 = AL;
		    res = src1 * src2;
		    AX = res;
		    if (res&0xff00) {  /* CF=OF=1 */
			CARRY=1; SRC1_16 = SRC2_16 = ~RES_16; /* 001,110 */
		    }
		    else {
			CARRY=0; SRC1_16 = SRC2_16 = RES_16;  /* 000,111 */
		    }
		    goto next_switch;
		case 5: /* IMUL AL */
		    src1 = *(signed char *)mem_ref;
		    src2 = (signed char)AL;
		    res = src1 * src2;
		    AX = res;
		    res = res >> 7;
		    if ((res==0)||(res==0xffffffff)) {  /* CF=OF=0 */
			CARRY=0; SRC1_16 = SRC2_16 = RES_16; /* 000,111 */
		    }
		    else {
			CARRY=1; SRC1_16 = SRC2_16 = ~RES_16;  /* 001,110 */
		    }
		    goto next_switch;
		case 6: /* DIV AL */
		    src1 = *mem_ref;
		    if (src1==0) goto div_by_zero;
		    res = AX;
		    AL = (BYTE)(res / src1);
		    AH = (BYTE)(res % src1);
		    goto next_switch;
		case 7: /* IDIV AX */
		    src1 = *(signed char *)mem_ref;
		    if (src1==0) goto div_by_zero;
		    res = AX;
		    AL = (BYTE)(res / src1);
		    AH = (BYTE)(res % src1);
		    goto next_switch;
	    } }
/*f7*/	case GRP1wrm: {
	    int src1, src2, res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF;
	      switch (res) {
		case 0: /* TEST */
		case 1: /* Undocumented */
		    src1 = mFETCH_QUAD(mem_ref);
		    src2 = FETCH_QUAD(PC); PC += 4;
		    res = src1 & src2;
		    PACK32_16(res,RES_16); CARRY=0; SRC1_32=SRC2_32=RES_32; /* CF=OF=0 */
		    goto next_switch;
		case 2: /* NOT */
		    src1 = mFETCH_QUAD(mem_ref);
		    src1 = ~(src1);
		    mPUT_QUAD(mem_ref, src1);
		    goto next_switch;
		case 3: /* NEG */
		    src1 = 0; /* equiv to SUB 0,src2 */
		    src2 = mFETCH_QUAD(mem_ref);
		    res = -src2;
		    mPUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,1);
		    CARRY = (src2 != 0);	/* not CARRYB! */
		    goto next_switch;
		case 4: { /* MUL EAX */
		    u_i64_u mres, mmlt;
		    mres.u.ul = EAX; mres.u.uh=0;
		    mmlt.u.ul = mFETCH_QUAD (mem_ref); mmlt.u.uh=0;
		    mres.ud *= mmlt.ud;
		    EAX = mres.u.ul;
		    EDX = mres.u.uh;
		    if (mres.u.uh) {  /* CF=OF=1 */
			CARRY=1; SRC1_16 = SRC2_16 = ~RES_16; /* 001,110 */
		    }
		    else {
			CARRY=0; SRC1_16 = SRC2_16 = RES_16;  /* 000,111 */
		    }
                    } goto next_switch;
		case 5: { /* IMUL EAX */
		    s_i64_u mres, mmlt;
		    mres.s.sl = EAX;
		    mres.s.sh = (mres.s.sl<0? -1:0);
		    mmlt.s.sl = mFETCH_QUAD (mem_ref);
		    mmlt.s.sh = (mmlt.s.sl<0? -1:0);
		    mres.sd *= mmlt.sd;
		    EAX = mres.s.sl;
		    EDX = mres.s.sh;
		    if (mres.s.sl == mres.sd) {  /* CF=OF=0 */
			CARRY=0; SRC1_16 = SRC2_16 = RES_16; /* 000,111 */
		    }
		    else {
			CARRY=1; SRC1_16 = SRC2_16 = ~RES_16;  /* 001,110 */
		    }
                    } goto next_switch;
		case 6: { /* DIV EDX+EAX */
		    u_i64_u divd;
		    DWORD temp, rem;
		    divd.u.ul = EAX; divd.u.uh = EDX;
		    temp = mFETCH_QUAD(mem_ref);
		    if (temp==0) goto div_by_zero;
		    rem = (DWORD)(divd.ud % temp);
		    divd.ud /= temp;
		    if (divd.u.uh) goto div_by_zero;
		    EAX = divd.u.ul;
		    EDX = rem;
		    } goto next_switch;
		case 7: { /* IDIV EDX+EAX */
		    s_i64_u divd;
		    signed long temp, rem;
		    divd.s.sl = EAX; divd.s.sh = EDX;
		    temp = mFETCH_QUAD(mem_ref);
		    if (temp==0) goto div_by_zero;
		    rem = (signed long)(divd.sd % temp);
		    divd.sd /= temp;
		    if (((divd.s.sh+1)&(-2))!=0) goto div_by_zero;
		    EAX = divd.s.sl;
		    EDX = rem;
		    } goto next_switch;
	      }
	      }
/*f8*/	case CLC:
	    CLEAR_CF;
	    PC += 1; goto next_switch;
/*f9*/	case STC:
	    SET_CF;
	    PC += 1; goto next_switch;
/*fa*/	case CLI:
	    goto not_permitted;
/*fb*/	case STI:
	    goto not_permitted;
/*fc*/	case CLD:
	    CLEAR_DF;
	    PC += 1; goto next_switch;
/*fd*/	case STD:
	    SET_DF;
	    PC += 1; goto next_switch;
/*fe*/	case GRP2brm: { /* only INC and DEC are legal on bytes */
	    register int temp;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(MODARGS);
	    mem_ref = MEM_REF;
	    switch (temp) {
		case 0: /* INC */
		    SRC1_8 = temp = *mem_ref;
		    *mem_ref = temp = temp + 1;
		    SRC2_8 = 1;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    goto next_switch;
		case 1: /* DEC */
		    SRC1_8 = temp = *mem_ref;
		    *mem_ref = temp = temp - 1;
		    SRC2_8 = -1;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    goto next_switch;
		case 2: /* CALL indirect */
		case 3: /* CALL long indirect */
		case 4: /* JMP indirect */
		case 5: /* JMP long indirect */
		case 6: /* PUSH */
		case 7: /* Illegal */
                    goto illegal_op; 
	    }}
/*ff*/	case GRP2wrm: {
	    register DWORD temp;
	    DWORD res, src1, src2;
	    DWORD jcs, jip, ocs=0, oip=0;
	    WORD transfer_magic;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(MODARGS);
	    mem_ref = MEM_REF;
	      switch (temp) {
		case 0: /* INC */
		    res = mFETCH_QUAD(mem_ref);
		    src1 = res; src2 = 1;
		    res++;
		    mPUT_QUAD(mem_ref, res);
	    	    SETDFLAGS(0,0);
		    goto next_switch;
		case 1: /* DEC */
		    res = mFETCH_QUAD(mem_ref);
		    src1 = res; src2 = -1;
		    res--;
		    mPUT_QUAD(mem_ref, res);
	    	    SETDFLAGS(0,0);
		    goto next_switch;
		case 2: /* CALL indirect */
		    temp = PC - LONG_CS;
		    PUSHQUAD(temp);
		    PC = LONG_CS + mFETCH_QUAD(mem_ref);
		    goto next_switch;
		case 4: /* JMP indirect */
		    PC = LONG_CS + mFETCH_QUAD(mem_ref);
		    goto next_switch;
		case 3: {  /* CALL long indirect restartable */
		    ocs = SHORT_CS_16;
		    oip = PC - LONG_CS;
		    if (VM86F) goto illegal_op;
		    }
		    /* fall through */
		case 5: {  /* JMP long indirect */
		    if (VM86F) goto illegal_op;
		    jip = FETCH_QUAD(mem_ref);
		    jcs = FETCH_WORD(mem_ref+4);
 		    transfer_magic = (WORD)GetSelectorType(jcs);
		    if (transfer_magic==TRANSFER_CODE16) {
			if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,jcs))==EXCP0B_NOSEG) {
			    env->error_addr=jcs&0xfffc; return P0; }
			if (temp==3) {
			    PUSHQUAD(ocs);
			    PUSHQUAD(oip);
			}
			SHORT_CS_16 = (WORD)jcs;
			PC = jip + LONG_CS;
			if (CEmuStat&CeS_MODE_PM32) {
			    *err = EXCP_GOBACK;
			}
			return (PC);	/* always for code16 */
		    }
		    else if (transfer_magic==TRANSFER_CODE32) {
			if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,jcs))==EXCP0B_NOSEG) {
			    env->error_addr=jcs&0xfffc; return P0; }
			if (temp==3) {
			    PUSHQUAD(ocs);
			    PUSHQUAD(oip);
			}
			SHORT_CS_16 = (WORD)jcs;
			PC = jip + LONG_CS;
			if (!(CEmuStat&CeS_MODE_PM32)) {
			    *err = EXCP_GOBACK; return (PC);
			}
			goto next_switch;
		    }
		    else
			/* unimplemented in Twin */
			invoke_data(env); /* TRANSFER_DATA or garbage */
		    }
		case 6: /* PUSH */
		    temp = mFETCH_QUAD(mem_ref);
		    PUSHQUAD(temp);
		    goto next_switch;
		case 7: /* Illegal */
                 goto illegal_op;
		    goto next_switch;
	      }
	    }
	default: goto not_implemented;
	}

not_implemented:
	d_emu=9;
	error(" 32/32 nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*P0,*(P0+1),*(P0+2),
		SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifndef NO_TRACE_MSGS
	e_trace(env, P0, P0, 3);
#endif
	FatalAppExit(0, "INSTR");
	exit(1);

bad_override:
	d_emu=9;
	error(" 32/32 bad code/data sizes at %4x:%4x long PC %x\n",
		SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifndef NO_TRACE_MSGS
	e_trace(env, P0, P0, 3);
#endif
	FatalAppExit(0, "SIZE");
	exit(1);

bad_data:
	d_emu=9;
	error(" 32/32 bad data at %4x:%4x long PC %x\n",
		SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifndef NO_TRACE_MSGS
	e_trace(env, P0, P0, 3);
#endif
	FatalAppExit(0, "OOPS");
	exit(1);

not_permitted:
#ifdef DOSEMU
	*err = EXCP0D_GPF; return P0;
#else
	FatalAppExit(0, "GPF");
	exit(1);
#endif

div_by_zero:
#ifdef DOSEMU
	*err = EXCP00_DIVZ; return P0;
#else
	FatalAppExit(0, "DIVZ");
	exit(1);
#endif

illegal_op:
	error(" 32/32 illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*P0,*(P0+1),*(P0+2), 
                SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifdef DOSEMU
	*err = EXCP06_ILLOP; return (P0);
#else
	FatalAppExit(0, "ILLEG");
	exit(1);
#endif
}
