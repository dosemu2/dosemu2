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
	interp_32_16.c	1.15
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

changes for use with dosemu-0.67 1997/10/20 vignani@mbox.vol.it
changes for use with dosemu-0.99 1998/12/13 vignani@mbox.vol.it
 */

#if defined(X386) && defined(__i386__)
#include "emu-globv.h"
#endif

#include "bitops.h"
#include "port.h"
#include "Log.h"
#include "hsw_interp.h"
#include "mod_rm.h"

extern long instr_count;
#ifdef EMU_STAT
extern long InsFreq[];
#endif


#ifdef EMU_GLOBAL_VAR
extern Interp_ENV dosemu_env;
#define	env		(&dosemu_env)
BYTE *
hsw_interp_32_16 (register Interp_ENV *env1, BYTE *P0,
	register BYTE *_PC, int *err)
#else
BYTE *
hsw_interp_32_16 (register Interp_ENV *env, BYTE *P0,
	register BYTE *_PC, int *err)
#endif
{
    BYTE *mem_ref;

    PC = _PC;
    *err = 0;
#ifdef DEBUG
    if (d.emu) e_debug(env, P0, PC, 2);
#endif

override:
#ifdef EMU_STAT
    InsFreq[*PC] += 1;
#endif
    switch (*PC) {
/*00*/	case ADDbfrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*01*/	case ADDwfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 + src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 + src2;
		PUT_QUAD(mem_ref, res);
	    }
	    SETDFLAGS(1,0);
	    } return (PC); 
/*02*/	case ADDbtrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*03*/	case ADDwtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 + src2;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*04*/	case ADDbia: return (PC);
/*05*/	case ADDwia: {
	    DWORD res, src1, src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 + src2;
	    SETDFLAGS(1,0);
	    } PC += 5; return (PC);
/*06*/	case PUSHes: {
	    DWORD temp = SHORT_ES_32;
	    PUSHQUAD(temp); 
	    } PC += 1; return (PC);
/*07*/	case POPes: {	/* restartable */
	    DWORD temp;
	    temp = TOS_WORD;
	    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
	    	env->error_addr=temp; return P0; }
	    POPQUAD(temp); temp &= 0xffff;
	    SHORT_ES_32 = temp; }
	    PC += 1; return (PC);
/*08*/	case ORbfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF;
	    src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*09*/	case ORwfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 | src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 | src2;
		PUT_QUAD(mem_ref, res);
	    }
	    src1 = src2 =res;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*0a*/	case ORbtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*0b*/	case ORwtrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 | src2;
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*0c*/	case ORbi: return (PC);
/*0d*/	case ORwi: {
	    DWORD res, src1,src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 | src2;
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } PC += 5; return (PC);
/*0e*/	case PUSHcs: {
	    DWORD temp = SHORT_CS_32;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);

/*0f*/	case TwoByteESC: {
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    if (IS_MODE_REG) *(DWORD *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 1: /* STR */ {
			    /* Store Task Register */
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    if (IS_MODE_REG) *(DWORD *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
			    if (VM86F) goto not_permitted;
			    PC = PC +1 + hsw_modrm_16_quad(env,PC + 1);
			    return (PC);
			case 3: /* LTR */ {
			    /* Load Task Register */
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
			    /* hsw_task_register = temp; */
			    } return (PC);
			case 4: /* VERR */ {
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
		goto not_implemented;
			    /* if (hsw_verr(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } return (PC);
			case 5: /* VERW */ {
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
		goto not_implemented;
			    /* if (hsw_verw(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } return (PC);
			case 6: /* Illegal */
			case 7: /* Illegal */
		    goto illegal_op;
			default: goto not_implemented;
		    }
		case 0x01: /* GRP7 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 1: /* ESIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 2: /* LGDT */ /* Privileged */
			    /* Load Global Descriptor Table Register */
			    if (VM86F) goto not_permitted;
			    PC = PC+1+hsw_modrm_16_quad(env,PC + 1);
		goto not_implemented;
			    return (PC);
			case 3: /* LIDT */ /* Privileged */
			    /* Load Interrupt Descriptor Table Register */
			    if (VM86F) goto not_permitted;
			    PC = PC+1+hsw_modrm_16_quad(env,PC + 1);
		goto not_implemented;
			    return (PC);
			case 4: /* SMSW */ {
			    /* Store Machine Status Word */
			    int temp;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LIMIT field */;
			    if (IS_MODE_REG) *(DWORD *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 5: /* Illegal */
		goto illegal_op;
			case 6: /* LMSW */ /* Privileged */
			    /* Load Machine Status Word */
			    if (VM86F) goto not_permitted;
			    goto not_implemented;
			    PC = PC+1+hsw_modrm_16_quad(env,PC + 1);
			    return (PC);
			case 7: /* Illegal */
		goto illegal_op;
			default: goto not_implemented;
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
			int temp;
			WORD _sel;
			if (VM86F) goto not_permitted;
			PC += 1; PC += hsw_modrm_16_quad(env,PC);
			mem_ref = MEM_REF;
			if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
			    else _sel = FETCH_WORD(mem_ref);
			if (IS_GDT(_sel)||((_sel>>3)>LDT_ENTRIES))
			    RES_32 = 1;
			else {
			    temp = GetSelectorFlags(_sel);
			    if (temp) SetFlagAccessed(_sel);
			    RES_32 = 0;
			    *EREG1 = temp << 8;
			}
		    } return (PC);
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
		    int temp;
		    WORD _sel;
		    if (VM86F) goto not_permitted;
		    PC += 1; PC += hsw_modrm_16_quad(env,PC);
		    mem_ref = MEM_REF;
                    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
		    if (IS_GDT(_sel)||((_sel>>3)>LDT_ENTRIES))
			RES_32 = 1;	/* -> ZF=0 */
		    else {
		    temp= GetSelectorByteLimit(_sel);
			*EREG1 = temp;
			RES_32 = 0;	/* -> ZF=1 */
		    }
		    /* what do I do here??? */
		    } return (PC);
		case 0x06: /* CLTS */ /* Privileged */
		    /* Clear Task State Register */
		    if (VM86F) goto not_permitted;
		    PC += 2; return (PC);
		case 0x08: /* INVD */
		    /* INValiDate cache */
		    PC += 2; return (PC);
		case 0x09: /* WBINVD */
		    /* Write-Back and INValiDate cache */
		    PC += 2; return (PC);
		case 0x20: /* MOVcdrd */ /* Privileged */
		case 0x21: /* MOVddrd */ /* Privileged */
		case 0x22: /* MOVrdcd */ /* Privileged */
		case 0x23: /* MOVrddd */ /* Privileged */
		case 0x24: /* MOVtdrd */ /* Privileged */
		case 0x26: /* MOVrdtd */ /* Privileged */
		    if (VM86F) goto not_permitted;
		    goto not_implemented;
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
			goto not_implemented;
		case 0x90: /* SETObrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x91: /* SETNObrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x92: /* SETBbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x93: /* SETNBbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x94: /* SETZbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x95: /* SETNZbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x96: /* SETBEbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x97: /* SETNBEbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x98: /* SETSbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET) ? 1 : 0;
		    } return (PC);
		case 0x99: /* SETNSbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9a: /* SETPbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9b: /* SETNPbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9c: /* SETLbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9d: /* SETNLbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9e: /* SETLEbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9f: /* SETNLEbrm */ {
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0xa0: /* PUSHfs */ {
		    DWORD temp = SHORT_FS_32;
		    PUSHQUAD(temp);
		    } PC += 2; return (PC);
		case 0xa1: /* POPfs */ {
		    DWORD temp;
		    temp = TOS_WORD;
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
		    	env->error_addr=temp; return P0; }
		    POPQUAD(temp); temp &= 0xffff;
		    SHORT_FS_32 = temp;
		    } PC += 2; return (PC);
                case 0xa3: /* BT */ {
                    DWORD temp, ind1;
                    long ind2;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind2)&1;
                    } else {
                        if(ind2 >= 0) {
			    ind1 = ((ind2 >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (temp >> ind2) & 0x1;
                        } else {
                            ind2 = -ind2 - 1;
			    ind1 = ((ind2 >> 5) + 1) << 2;
			    mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (((temp << ind2) & 0x80000000)? 1:0);
                        }
		    }
                    } return(PC);
		case 0xa4: /* SHLDimm */ {
		    /* Double Precision Shift Left */
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = *PC & 0x1f; PC++;
			if (IS_MODE_REG) {
			    temp = FETCH_EREG(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (32 - count);
			    temp |= temp1;
			    *(DWORD *)mem_ref = temp;
			} else {
			    temp = FETCH_QUAD(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (32 - count);
			    temp |= temp1;
			    PUT_QUAD(mem_ref,temp);
			}
		    res = temp; src1 = src2 = temp >> 1;
		    SETDFLAGS(1,0); CARRY = temp & 1;
		    } return (PC);
		case 0xa5: /* SHLDcl */ {
		    /* Double Precision Shift Left by CL */
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = ECX & 0x1f;
			if (IS_MODE_REG) {
			    temp = FETCH_EREG(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (32 - count);
			    temp |= temp1;
			    *(DWORD *)mem_ref = temp;
			} else {
			    temp = FETCH_QUAD(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (32 - count);
			    temp |= temp1;
			    PUT_QUAD(mem_ref,temp);
			}
		    res = temp; src1 = src2 = temp >> 1;
		    SETDFLAGS(1,0); CARRY = temp & 1;
		    } return (PC);
		case 0xa6: /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
		case 0xa7: /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		goto not_implemented;
		case 0xa8: /* PUSHgs */ {
		    DWORD temp = SHORT_GS_32;
		    PUSHQUAD(temp);
		    } PC += 2; return (PC);
		case 0xa9: /* POPgs */ {
		    DWORD temp;
		    temp = TOS_WORD;
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
		    	env->error_addr=temp; return P0; }
		    POPQUAD(temp); temp &= 0xffff;
		    SHORT_GS_32 = temp;
		    } PC += 2; return (PC);
                case 0xab: /* BTS */ {
                    DWORD temp, ind1;
                    long ind2;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind2)&1;
                        temp |= (0x1 << ind2);
                        *(DWORD *)mem_ref = temp;
                    } else {
                        if(ind2 >= 0) {
			    ind1 = ((ind2 >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (temp >> ind2) & 0x1;
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
                    } return(PC);
		case 0xac: /* SHRDimm */ {
		    /* Double Precision Shift Right by immediate */
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = *PC & 0x1f; PC++;
			if (IS_MODE_REG) {
			    temp = FETCH_EREG(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    *(DWORD *)mem_ref = temp;
			} else {
			    temp = FETCH_QUAD(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    PUT_QUAD(mem_ref,temp);
			}
		    res = temp; src1 = src2 = temp << 1;
		    SETDFLAGS(0,0);
		    } return (PC);
		case 0xad: /* SHRDcl */ {
		    /* Double Precision Shift Right by CL */
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = ECX & 0x1f;
			if (IS_MODE_REG) {
			    temp = FETCH_EREG(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    *(DWORD *)mem_ref = temp;
			} else {
			    temp = FETCH_QUAD(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    PUT_QUAD(mem_ref,temp);
			}
		    res = temp; src1 = src2 = temp << 1;
		    SETDFLAGS(0,0);
		    } return (PC);
		case 0xaf: { /* IMULregrm */
		    s_i64_u res, mlt;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    res.s.sl = *EREG1;
                    res.s.sh = (res.s.sl<0? -1:0);
                    mem_ref = MEM_REF;
                    if (IS_MODE_REG)
                        mlt.s.sl = FETCH_EREG(mem_ref);
                    else
                        mlt.s.sl = FETCH_QUAD(mem_ref);
                    mlt.s.sh = (mlt.s.sl<0? -1:0);
		    res.sd *= mlt.sd;
		    *EREG1 = res.s.sl;
                    RES_32 = 0;
                    if (((res.s.sh+1)&(-2))!=0) {
                        SRC1_16 = SRC2_16 = 0x8000; SET_CF;
                    }
                    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 0xb2: /* LSS */ {
		    int temp;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op;
			return (PC);
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_QUAD(mem_ref);
		    *EREG1 = temp;
		    temp = FETCH_WORD(mem_ref+4);
		    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_SS_32 = temp;
		    } return (PC);
                case 0xb3: /* BTR */ {
                    DWORD temp, ind1;
                    long ind2;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind2)&1;
                        temp &= ~(0x1 << ind2);
                        *(DWORD *)mem_ref = temp;
                    } else {
                        if(ind2 >= 0) {
			    ind1 = ((ind2 >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (temp >> ind2) & 0x1;
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
                    } return(PC);
		case 0xb4: /* LFS */ {
		    int temp;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op;
			return (PC);
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_QUAD(mem_ref);
		    *EREG1 = temp;
		    temp = FETCH_WORD(mem_ref+4);
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_FS_32 = temp;
		    } return (PC);
		case 0xb5: /* LGS */ {
		    int temp;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op;
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_QUAD(mem_ref);
		    *EREG1 = temp;
		    temp = FETCH_WORD(mem_ref+4);
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_GS_32 = temp;
		    } return (PC);
		case 0xb6: /* MOVZXb */ {
		    DWORD temp;
		    int ref = (*(PC+2)>>3)&7;
		    PC = PC+1+hsw_modrm_16_byte(env,PC+1);
		    temp = *(BYTE *)MEM_REF;
		    switch (ref) {
		      case 0: EAX = temp; return (PC);
		      case 1: ECX = temp; return (PC);
		      case 2: EDX = temp; return (PC);
		      case 3: EBX = temp; return (PC);
		      case 4: ESP = temp; return (PC);
		      case 5: EBP = temp; return (PC);
		      case 6: ESI = temp; return (PC);
		      case 7: EDI = temp; return (PC);
		    }
		    }
		case 0xb7: /* MOVZXw */ {
			DWORD temp; 
			PC = PC+1+hsw_modrm_16_quad(env,PC+1);
			mem_ref = MEM_REF;
			if(IS_MODE_REG)
		 	    temp = FETCH_EREG(mem_ref);
			else
			    temp = FETCH_QUAD(mem_ref);
			*EREG1 = (DWORD)(temp & 0xffff);
			} return (PC);
		case 0xba: /* GRP8 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* Illegal */
			case 1: /* Illegal */
			case 2: /* Illegal */
			case 3: /* Illegal */
		goto illegal_op;
                        case 4: /* BT */ {
                            int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                            mem_ref = MEM_REF; temp = *PC;  PC++;
                            if (IS_MODE_REG)
                                temp1 = *(DWORD *)mem_ref;
                            else
                                temp1 = FETCH_QUAD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0x1f))&1;
                    	    } return(PC);
                        case 5: /* BTS */ {
                            int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                            mem_ref = MEM_REF; temp = (*PC) & 0x1f;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(DWORD *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
                                *(DWORD *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_QUAD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
                                PUT_QUAD(mem_ref,temp1);
			    }
                    	    } return(PC);
                        case 6: /* BTR */ {
                            int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                            mem_ref = MEM_REF; temp = (*PC) & 0x1f;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(DWORD *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
                                *(DWORD *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_QUAD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
                                PUT_QUAD(mem_ref,temp1);
			    }
                    	    } return(PC);
                        case 7: /* BTC */ {
                            int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                            mem_ref = MEM_REF; temp = (*PC) & 0x1f;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(DWORD *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
                                *(DWORD *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_QUAD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
                                PUT_QUAD(mem_ref,temp1);
			    }
                        } return(PC);
                    }
                case 0xbb: /* BTC */ {
                    DWORD temp, ind1;
                    long ind2;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF; ind2 = *EREG1;
                    if (IS_MODE_REG) {
                        ind2 = (ind2 & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind2)&1;
                        temp ^= (0x1 << ind2);
                        *(DWORD *)mem_ref = temp;
                    } else {
                        if(ind2 >= 0) {
			    ind1 = ((ind2 >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (temp >> ind2) & 0x1;
                            temp ^= (0x1 << ind2);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind2 = -ind2 - 1;
			    ind1 = ((ind2 >> 5) + 1) << 2;
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind2 = ind2 & 0x1f;
                            CARRY = (((temp << ind2) & 0x80000000)? 1:0);
                            temp ^= (0x80000000 >> ind2);
                            PUT_QUAD(mem_ref,temp);
                        }
		    }
                    } return(PC);
                case 0xbc: /* BSF */ {
                    DWORD temp, i;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = *(DWORD *)mem_ref;
                    else
                        temp = FETCH_QUAD(mem_ref);
		    if(temp) {
                    for(i=0; i<32; i++)
                        if((temp >> i) & 0x1) break;
                    *EREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } return(PC);
                case 0xbd: /* BSR */ {
                    DWORD temp; 
                    int  i;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = *(DWORD *)mem_ref;
                    else
                        temp = FETCH_QUAD(mem_ref);
		    if(temp) {
                    for(i=31; i>=0; i--)
                        if((temp & ( 0x1 << i))) break;
                    *EREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } return(PC);
		case 0xbe: /* MOVSXb */ {
                    signed long temp;
		    int ref = (*(PC+2)>>3)&7;
		    PC = PC+1+hsw_modrm_16_byte(env,PC+1);
		    temp = *(signed char *)MEM_REF;
		    switch (ref) {
		      case 0: EAX = ((temp<<24)>>24); return (PC);
		      case 1: ECX = ((temp<<24)>>24); return (PC);
		      case 2: EDX = ((temp<<24)>>24); return (PC);
		      case 3: EBX = ((temp<<24)>>24); return (PC);
		      case 4: ESP = ((temp<<24)>>24); return (PC);
		      case 5: EBP = ((temp<<24)>>24); return (PC);
		      case 6: ESI = ((temp<<24)>>24); return (PC);
		      case 7: EDI = ((temp<<24)>>24); return (PC);
		    }
		    }
		case 0xbf: /* MOVSXw */ {
                    signed long temp;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = FETCH_EREG(mem_ref);
                    else
                        temp = FETCH_QUAD(mem_ref);
                    temp = ((temp<<16)>>16);
                    *EREG1 = temp;
                    } return(PC);
                case 0xc1: { /* XADDw */
                    DWORD res,src1,src2;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1);
                    src2 = *EREG1; mem_ref = MEM_REF;
                    if (IS_MODE_REG) {
                        src1 = FETCH_EREG(mem_ref);
                        res = src1 + src2;
                        PUT_EREG(mem_ref, res);
                    } else {
                        src1 = FETCH_QUAD(mem_ref);
                        res = src1 + src2;
                        PUT_QUAD(mem_ref, res);
                    }
                    *EREG1 = src1;
                    SETDFLAGS(1,0);
                    } return(PC);
                case 0xc8: /* BSWAPeax */ {
                    DWORD temp = EAX;
                    EAX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xc9: /* BSWAPecx */ {
                    DWORD temp = ECX;
                    ECX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xca: /* BSWAPedx */ {
                    DWORD temp = EDX;
                    EDX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcb: /* BSWAPebx */ {
                    DWORD temp = EBX;
                    EBX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcc: /* BSWAPesp */ {
                    DWORD temp = ESP;
                    ESP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcd: /* BSWAPebp */ {
                    DWORD temp = EBP;
                    EBP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xce: /* BSWAPesi */ {
                    DWORD temp = ESI;
                    ESI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcf: /* BSWAPedi */ {
                    DWORD temp = EDI;
                    EDI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
           	    } PC += 2; return(PC);
		default: goto not_implemented;
                }
            }

/*10*/	case ADCbfrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 += (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*11*/	case ADCwfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 + src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 + src2;
		PUT_QUAD(mem_ref, res);
	    }
	    SETDFLAGS(1,0);
	    } return (PC); 
/*12*/	case ADCbtrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 += (CARRY & 1);
	    *mem_ref = res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*13*/	case ADCwtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 + src2;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*14*/	case ADCbi: return (PC);
/*15*/	case ADCwi: {
	    DWORD res, src1, src2;
	    src1 = EAX; 
	    src2 = (FETCH_QUAD((PC+1))) + (CARRY & 1);
	    EAX = res = src1 + src2;
	    SETDFLAGS(1,0);
	    } PC += 5; return (PC);
/*16*/	case PUSHss: {
	    DWORD temp = SHORT_SS_32;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
/*17*/	case POPss: {
	    DWORD temp, temp2;
	    POPQUAD(temp2); temp = temp2 & 0xffff;
	    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
	    	PUSHQUAD(temp2); env->error_addr=temp; return P0; }
	    SHORT_SS_32 = temp;
	    } PC += 1; return (PC);
/*18*/	case SBBbfrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*19*/	case SBBwfrm: {
	    DWORD res, src1;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 - src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 - src2;
		PUT_QUAD(mem_ref, res);
	    }
	    SETDFLAGS(1,1);
	    } return (PC); 
/*1a*/	case SBBbtrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*1b*/	case SBBwtrm: {
	    DWORD res, src1;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    src2 = src2 + (CARRY & 1);
	    *EREG1 = res = src1 - src2;
	    SETDFLAGS(1,1);
	    } return (PC); 
/*1c*/	case SBBbi: return (PC);
/*1d*/	case SBBwi: {
	    DWORD res, src1;
	    signed long src2;
	    src1 = EAX; 
	    src2 = (FETCH_QUAD((PC+1))) + (CARRY & 1);
	    EAX = res = src1 - src2;
	    SETDFLAGS(1,1);
	    } PC += 5; return (PC);
/*1e*/	case PUSHds: {
	    WORD temp = SHORT_DS_32;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
/*1f*/	case POPds: {
	    DWORD temp;
	    temp = TOS_WORD;
	    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
	    	env->error_addr=temp; return P0; }
	    POPQUAD(temp); temp &= 0xffff;
	    SHORT_DS_32 = temp;
	    } PC += 1; return (PC);

/*20*/	case ANDbfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*21*/	case ANDwfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 & src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 & src2;
		PUT_QUAD(mem_ref, res);
	    }
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*22*/	case ANDbtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*23*/	case ANDwtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 & src2;
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*24*/	case ANDbi: return (PC);
/*25*/	case ANDwi: {
	    DWORD res, src1, src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 & src2;
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } PC += 5; return (PC);
/*26*/	case SEGes:
	    if (!VM86F && (SHORT_ES_16 < 4 || LONG_ES==INVALID_OVR)) {
		e_printf("General Protection Fault: zero ES\n");
		goto bad_override;
	    }
	    OVERRIDE = LONG_ES;
	    PC += 1; goto override;
/*27*/	case DAA: return (PC);
/*28*/	case SUBbfrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*29*/	case SUBwfrm: {
	    DWORD res, src1;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 - src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 - src2;
		PUT_QUAD(mem_ref, res);
	    }
	    SETDFLAGS(1,1);
	    } return (PC); 
/*2a*/	case SUBbtrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*2b*/	case SUBwtrm: {
	    DWORD res, src1;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 - src2;
	    SETDFLAGS(1,1);
	    } return (PC); 
/*2c*/	case SUBbi: return (PC);
/*2d*/	case SUBwi: {
	    DWORD res, src1;
	    signed long src2;
	    src1 = EAX; 
	    src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 - src2;
	    SETDFLAGS(1,1);
	    } PC += 5; return (PC);
/*2e*/	case SEGcs:
	    OVERRIDE = LONG_CS;
	    PC+=1; goto override;
/*2f*/	case DAS: return (PC);

/*30*/	case XORbfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*31*/	case XORwfrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 ^ src2;
		PUT_EREG(mem_ref, res);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 ^ src2;
		PUT_QUAD(mem_ref, res);
	    }
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*32*/	case XORbtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*33*/	case XORwtrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 ^ src2;
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } return (PC); 
/*34*/	case XORbi: return (PC);
/*35*/	case XORwi: {
	    DWORD res, src1, src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 ^ src2;
	    src1 = src2 = res;
	    SETDFLAGS(1,0);
	    } PC += 5; return (PC);
/*36*/	case SEGss:
	    OVERRIDE = LONG_SS;
	    PC+=1; goto override;
/*37*/	case AAA: return (PC);
/*38*/	case CMPbfrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*39*/	case CMPwfrm: {
	    DWORD res, src1;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    res = src1 - src2;
	    SETDFLAGS(1,1);
	    } return (PC); 
/*3a*/	case CMPbtrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *MEM_REF; src1 = *HREG1;
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*3b*/	case CMPwtrm: {
	    DWORD res, src1;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    res = src1 - src2;
	    SETDFLAGS(1,1);
	    } return (PC); 
/*3c*/	case CMPbi: return (PC);
/*3d*/	case CMPwi: {
	    DWORD res, src1;
	    signed long src2;
	    src1 = EAX; 
	    src2 = FETCH_QUAD((PC+1));
	    res = src1 - src2;
	    SETDFLAGS(1,1);
	    } PC += 5; return (PC);
/*3e*/	case SEGds:
	    if (!VM86F && (SHORT_DS_16 < 4 || LONG_DS==INVALID_OVR)) {
		e_printf("General Protection Fault: zero DS\n");
		goto bad_override;
	    }
	    OVERRIDE = LONG_DS;
	    PC+=1; goto override;
/*3f*/	case AAS: return (PC);

/*40*/	case INCax: {
            DWORD res, src1, src2;
            src1 = EAX; src2 = 1;
            EAX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*41*/	case INCcx: {
            DWORD res, src1, src2;
            src1 = ECX; src2 = 1;
            ECX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*42*/	case INCdx: {
            DWORD res, src1, src2;
            src1 = EDX; src2 = 1;
            EDX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*43*/	case INCbx: {
            DWORD res, src1, src2;
            src1 = EBX; src2 = 1;
            EBX = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*44*/	case INCsp: {
            DWORD res, src1, src2;
            src1 = ESP; src2 = 1;
            ESP = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*45*/	case INCbp: {
            DWORD res, src1, src2;
            src1 = EBP; src2 = 1;
            EBP = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*46*/	case INCsi: {
            DWORD res, src1, src2;
            src1 = ESI; src2 = 1;
            ESI = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*47*/	case INCdi: {
            DWORD res, src1, src2;
            src1 = EDI; src2 = 1;
            EDI = res = src1 + 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*48*/	case DECax: {
            long res, src1, src2;
            src1 = EAX; src2 = -1;
            EAX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*49*/	case DECcx: {
            long res, src1, src2;
            src1 = ECX; src2 = -1;
            ECX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*4a*/	case DECdx: {
            long res, src1, src2;
            src1 = EDX; src2 = -1;
            EDX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*4b*/	case DECbx: {
            long res, src1, src2;
            src1 = EBX; src2 = -1;
            EBX = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*4c*/	case DECsp: {
            long res, src1, src2;
            src1 = ESP; src2 = -1;
            ESP = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*4d*/	case DECbp: {
            long res, src1, src2;
            src1 = EBP; src2 = -1;
            EBP = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*4e*/	case DECsi: {
            long res, src1, src2;
            src1 = ESI; src2 = -1;
            ESI = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);
/*4f*/	case DECdi: {
            long res, src1, src2;
            src1 = EDI; src2 = -1;
            EDI = res = src1 - 1;
	    SETDFLAGS(0,0);
            } PC += 1; return (PC);

/*50*/	case PUSHax: {
            PUSHQUAD(EAX);
            } PC += 1; return (PC);
/*51*/	case PUSHcx: {  
            PUSHQUAD(ECX);
            } PC += 1; return (PC);
/*52*/	case PUSHdx: {  
            PUSHQUAD(EDX);
            } PC += 1; return (PC);
/*53*/	case PUSHbx: {  
            PUSHQUAD(EBX);
            } PC += 1; return (PC);
/*54*/	case PUSHsp: {  
            PUSHQUAD(ESP);
            } PC += 1; return (PC);
/*55*/	case PUSHbp: {  
            PUSHQUAD(EBP);
            } PC += 1; return (PC);
/*56*/	case PUSHsi: {  
            PUSHQUAD(ESI);
            } PC += 1; return (PC);
/*57*/	case PUSHdi: {  
            PUSHQUAD(EDI);
            } PC += 1; return (PC);
/*58*/	case POPax: POPQUAD(EAX); PC += 1; return (PC);
/*59*/	case POPcx: POPQUAD(ECX); PC += 1; return (PC);
/*5a*/	case POPdx: POPQUAD(EDX); PC += 1; return (PC);
/*5b*/	case POPbx: POPQUAD(EBX); PC += 1; return (PC);
/*5c*/	case POPsp: {
	    register DWORD temp;
	    POPQUAD(temp);
	    ESP = temp;
	    } PC += 1; return (PC);
/*5d*/	case POPbp: POPQUAD(EBP); PC += 1; return (PC);
/*5e*/	case POPsi: POPQUAD(ESI); PC += 1; return (PC);
/*5f*/	case POPdi: POPQUAD(EDI); PC += 1; return (PC);

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
	    } PC += 1; return (PC);
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
	    } PC += 1; return (PC);
/*62*/	case BOUND:
/*63*/	case ARPL:
            goto not_implemented;
/*64*/	case SEGfs:
	    if (!VM86F && (SHORT_FS_16 < 4 || LONG_FS==INVALID_OVR)) {
		e_printf("General Protection Fault: zero FS\n");
		goto bad_override;
	    }
	    OVERRIDE = LONG_FS;
	    PC+=1; goto override;
/*65*/	case SEGgs:
	    if (!VM86F && (SHORT_GS_16 < 4 || LONG_GS==INVALID_OVR)) {
		e_printf("General Protection Fault: zero GS\n");
		goto bad_override;
	    }
	    OVERRIDE = LONG_GS;
	    PC+=1; goto override;
/*66*/	case OPERoverride:	/* 0x66: 32 bit operand, 16 bit addressing */
	    if (data32) {
		if (d.emu>4) e_printf("ENTER interp_16d_16a\n");
		data32 = 0;
		PC = hsw_interp_16_16 (env, P0, PC+1, err, 1);
		data32 = 1;
		return (PC);
	    }
	    PC += 1;
	    goto override;
/*67*/	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
	    if (!code32) {
		if (d.emu>4) e_printf("ENTER interp_32d_32a\n");
		code32 = 1;
		PC = hsw_interp_32_32 (env, P0, PC+1, err, 1);
		code32 = 0;
		return (PC);
	    }
	    PC += 1;
	    goto override;
/*68*/	case PUSHwi: {
	    DWORD temp;
	    temp = FETCH_QUAD(PC+1);
	    PUSHQUAD(temp);
	    } PC += 5; return (PC);
/*69*/	case IMULwrm: {
	    s_i64_u res, mlt;
	    PC += hsw_modrm_16_quad(env,PC);
	    res.s.sl = FETCH_QUAD(PC);
	    res.s.sh = (res.s.sl<0? -1:0);
	    PC += 4; mem_ref = MEM_REF; 
	    if (IS_MODE_REG)
		mlt.s.sl = FETCH_EREG(mem_ref);
	    else
		mlt.s.sl = FETCH_QUAD(mem_ref);
	    mlt.s.sh = (mlt.s.sl<0? -1:0);
	    res.sd *= mlt.sd;
	    *EREG1 = res.s.sl;
	    RES_32 = 0;
            if (((res.s.sh+1)&(-2))!=0) {
		SRC1_16 = SRC2_16 = 0x8000; SET_CF;
	    }
            else SRC1_16 = SRC2_16 = 0;
            } return(PC);
/*6a*/	case PUSHbi: {
	    long temp = (signed char)*(PC+1);
	    temp = ((temp<<24)>>24);
	    PUSHQUAD(temp);
	    } PC +=2; return (PC);
/*6b*/	case IMULbrm: {
	    s_i64_u res, mlt;
	    PC += hsw_modrm_16_quad(env,PC);
	    res.s.sl = *(signed char *)(PC);
	    res.s.sh = (*PC & 0x80? -1:0);
	    PC += 1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG)
		mlt.s.sl = FETCH_EREG(mem_ref);
	    else
		mlt.s.sl = FETCH_QUAD(mem_ref);
	    mlt.s.sh = (mlt.s.sl<0? -1:0);
	    res.sd *= mlt.sd;
	    *EREG1 = res.s.sl;
	    RES_32 = 0;
            if (((res.s.sh+1)&(-2))!=0) {
		SRC1_16 = SRC2_16 = 0x8000; SET_CF;
	    }
            else SRC1_16 = SRC2_16 = 0;
	    } return (PC); 
/*6c*/	case INSb:
/*6d*/	case INSw:
/*6e*/	case OUTSb:
/*6f*/	case OUTSw:
	    goto not_permitted;

/*82*/	case IMMEDbrm2:    /* out of order */
/*80*/	case IMMEDbrm: {
	    DWORD src1, src2; int res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *PC; PC += 1;
	    mem_ref = MEM_REF; src1 = *mem_ref;
	    switch (res) {
		case 0: /* ADD */
		    *mem_ref = res = src1 + src2;
		    SETBFLAGS(0);
		    return (PC);
		case 1: /* OR */
		    *mem_ref = res = src1 | src2;
		    SETBFLAGS(0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *mem_ref = res = src1 + src2;
		    SETBFLAGS(0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *mem_ref = res = src1 - src2;
		    SETBFLAGS(1);
		    return (PC);
		case 4: /* AND */
		    *mem_ref = res = src1 & src2;
		    SETBFLAGS(0);
		    return (PC);
		case 5: /* SUB */
		    *mem_ref = res = src1 - src2;
		    SETBFLAGS(1);
		    return (PC);
		case 6: /* XOR */
		    *mem_ref = res = src1 ^ src2;
		    SETBFLAGS(0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETBFLAGS(1);
		    return (PC);
		default: goto not_implemented;
	    }}
/*81*/	case IMMEDwrm: {
	    DWORD src1, src2, res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = FETCH_QUAD(PC); PC += 4;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_EREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS(1,0);
		    return (PC);
		case 1: /* OR */
		    *(DWORD *)mem_ref = res = src1 | src2;
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS(1,0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		case 4: /* AND */
		    *(DWORD *)mem_ref = res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 5: /* SUB */
		    *(DWORD *)mem_ref = res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		case 6: /* XOR */
		    *(DWORD *)mem_ref = res = src1 ^ src2;
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		default: goto not_implemented;
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_QUAD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    PUT_QUAD(mem_ref, res);
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,1);
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    PUT_QUAD(mem_ref, res);
		    src1 = src2 =res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS(1,1);
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_QUAD(mem_ref, res);
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		default: goto not_implemented;
	      }
	    }}
/*83*/	case IMMEDisrm: {
	    DWORD src1, src2, res;
	    signed long temp;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    temp = *PC; PC += 1;
	    temp = ((temp<<24)>>24);
	    src2 = temp;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_EREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS(1,0);
		    return (PC);
		case 1: /* OR */
		    *(DWORD *)mem_ref = res = src1 | src2;
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS(1,0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		case 4: /* AND */
		    *(DWORD *)mem_ref = res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 5: /* SUB */
		    *(DWORD *)mem_ref = res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		case 6: /* XOR */
		    *(DWORD *)mem_ref = res = src1 ^ src2;
		    src1 = src2 = res;
		    SETDFLAGS(1,0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		default: goto not_implemented;
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_QUAD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    src1 = src2 = res;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,1);
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    src1 = src2 = res;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,1);
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS(1,0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETDFLAGS(1,1);
		    return (PC);
		default: goto not_implemented;
	      }
	    }}
/*84*/	case TESTbrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*85*/	case TESTwrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_16_quad(env,PC);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 & src2;
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 & src2;
	    }
	    src1 = src2 = res;
	    CLEAR_CF; SETDFLAGS(0,0);
	    } return (PC); 
/*86*/	case XCHGbrm: {
	    BYTE temp;
	    PC += hsw_modrm_16_byte(env,PC);
	    temp = *MEM_REF; *MEM_REF = *HREG1; *HREG1 = temp;
	    } return (PC); 
/*87*/	case XCHGwrm: {
	    DWORD temp;
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		temp = FETCH_EREG(mem_ref);
		*(DWORD *)mem_ref = *EREG1;
		*EREG1 = temp;
		return (PC); 
	    } else {
		DWORD temp1 = FETCH_QUAD(mem_ref);
		temp = *EREG1; *EREG1 = temp1;
		PUT_QUAD(mem_ref, temp);
		return (PC); 
	    }
	    }
/*88*/	case MOVbfrm:
	    PC += hsw_modrm_16_byte(env,PC);
	    *MEM_REF = *HREG1; return (PC);
/*89*/	case MOVwfrm:
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) {
		*(DWORD *)MEM_REF = *EREG1;
		return (PC); 
	    } else {
		DWORD temp = *EREG1;
		mem_ref = MEM_REF;
		PUT_QUAD(mem_ref, temp);
		return (PC); 
	    }
/*8a*/	case MOVbtrm:
	    PC += hsw_modrm_16_byte(env,PC);
	    *HREG1 = *MEM_REF; return (PC);
/*8b*/	case MOVwtrm: {
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    if(IS_MODE_REG) 
		*EREG1 = *(DWORD *)mem_ref;
	    else
		*EREG1 = FETCH_QUAD(mem_ref);
	    } return (PC);
/*8c*/	case MOVsrtrm: {
	    BYTE seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) {
		switch (seg_reg) {
		    case 0: /* ES */
			*(WORD *)MEM_REF = SHORT_ES_16;
			return (PC);
		    case 1: /* CS */
			*(WORD *)MEM_REF = SHORT_CS_16;
			return (PC);
		    case 2: /* SS */
			*(WORD *)MEM_REF = SHORT_SS_16;
			return (PC);
		    case 3: /* DS */
			*(WORD *)MEM_REF = SHORT_DS_16;
			return (PC);
		    case 4: /* FS */
			*(WORD *)MEM_REF = SHORT_FS_16;
			return (PC);
		    case 5: /* GS */
			*(WORD *)MEM_REF = SHORT_GS_16;
			return (PC);
		    case 6: /* Illegal */
		    case 7: /* Illegal */
			/* trap this */
		goto illegal_op;
		    default: goto not_implemented;
		}
	    } else {
		int temp;
		mem_ref = MEM_REF;
		switch (seg_reg) {
		    case 0: /* ES */
			temp = SHORT_ES_16;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 1: /* CS */
			temp = SHORT_CS_16;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 2: /* SS */
			temp = SHORT_SS_16;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 3: /* DS */
			temp = SHORT_DS_16;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 4: /* FS */
			temp = SHORT_FS_16;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 5: /* GS */
			temp = SHORT_GS_16;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 6: /* Illegal */
		    case 7: /* Illegal */
			/* trap this */
		goto illegal_op;
		    default: goto not_implemented;
		}
	    }}
/*8d*/	case LEA:
	    OVERRIDE = 0;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) {
		goto illegal_op;
	    } else {
		*EREG1 = (long)MEM_REF;
	    }
	    return (PC);
/*8e*/	case MOVsrfrm: {
	    DWORD temp;
	    BYTE seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) {
		temp = *(WORD *)MEM_REF;
	    } else {
		mem_ref = MEM_REF;
		temp = FETCH_WORD(mem_ref);
	    }
	    switch (seg_reg) {
		case 0: /* ES */
		    SHORT_ES_32 = temp;
		    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 1: /* CS */
		    SHORT_CS_32 = temp;
		    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 2: /* SS */
		    SHORT_SS_32 = temp;
		    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 3: /* DS */
		    SHORT_DS_32 = temp;
		    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 4: /* FS */
		    SHORT_FS_32 = temp;
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 5: /* GS */
		    SHORT_GS_32 = temp;
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 6: /* Illegal */
		case 7: /* Illegal */
		    /* trap this */
		goto illegal_op;
		default: goto not_implemented;
	    }}
/*8f*/	case POPrm: {
	    DWORD temp;
	    POPQUAD(temp);
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    PUT_QUAD(mem_ref, temp);
	    } return (PC);

/*90*/	case NOP:
	    PC += 1; return (PC);
/*91*/	case XCHGcx: {
	    DWORD temp = EAX;
	    EAX = ECX;
	    ECX = temp;
	    } PC += 1; return (PC);
/*92*/	case XCHGdx: {
	    DWORD temp = EAX;
	    EAX = EDX;
	    EDX = temp;
	    } PC += 1; return (PC);
/*93*/	case XCHGbx: {
	    DWORD temp = EAX;
	    EAX = EBX;
	    EBX = temp;
	    } PC += 1; return (PC);
/*94*/	case XCHGsp: {
	    DWORD temp = EAX;
	    EAX = ESP;
	    ESP = temp;
	    } PC += 1; return (PC);
/*95*/	case XCHGbp: {
	    DWORD temp = EAX;
	    EAX = EBP;
	    EBP = temp;
	    } PC += 1; return (PC);
/*96*/	case XCHGsi: {
	    DWORD temp = EAX;
	    EAX = ESI;
	    ESI = temp;
	    } PC += 1; return (PC);
/*97*/	case XCHGdi: {
	    DWORD temp = EAX;
	    EAX = EDI;
	    EDI = temp;
	    } PC += 1; return (PC);
/*98*/	case CBW: /* CWDE */
	    env->rax.x.dummy = ((AX & 0x8000) ? 0xffff : 0);
	    PC += 1; return (PC);
/*99*/	case CWD: /* CDQ */
            EDX = (EAX & 0x80000000) ? 0xffffffff : 0;
            PC += 1; return(PC);
/*9a*/	case CALLl: {
#if 0
	    DWORD cs = SHORT_CS_16;
	    DWORD ip = PC - LONG_CS + 5;
	    WORD transfer_magic;
	    PUSHQUAD(cs);
	    PUSHQUAD(ip);
	    env->return_addr = (cs << 16)|ip;
	    ip = FETCH_QUAD(PC+1);
	    cs = FETCH_QUAD(PC+3);
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if (transfer_magic == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,BIG_CS,cs);
		PC = ip + LONG_CS;
		return (PC);
	    }
	    if ((transfer_magic == TRANSFER_CALLBACK) ||
			(transfer_magic == TRANSFER_BINARY))  {
		LONGPROC conv,targ;
		SEGIMAGE *lpSegImage = &((SEGIMAGE *)
			(*(long *)(SELECTOR_PADDRESS(cs))))[ip>>3];
		targ = (LONGPROC)lpSegImage->targ;
		conv = (LONGPROC)lpSegImage->conv;
		EBP = (long)LONG_SS + EBP;
		ESP = (long)LONG_SS + ESP;
-------ifdef DEBUG
		if (transfer_magic == TRANSFER_CALLBACK)
		    LOGSTR((LF_DEBUG,
			"do_ext: %s\n", GetProcName(cs,ip>>3)));
		else    ------ TRANSFER_BINARY 
		    LOGSTR((LF_DEBUG,
			"do_ext: calling binary thunk %x:%x\n",cs,ip));
--------endif
		(conv)(env,targ);
		SET_SEGREG(LONG_CS,BIG_CS,SHORT_CS_16);
		SET_SEGREG(LONG_DS,BIG_DS,SHORT_DS_16);
		SET_SEGREG(LONG_ES,BIG_ES,SHORT_ES_16);
		SET_SEGREG(LONG_SS,BIG_SS,SHORT_SS_16);
		EBP = EBP - (long)LONG_SS;
		ESP = ESP - (long)LONG_SS;
		PC += 5; return (PC);
	    }
	    if (transfer_magic == TRANSFER_RETURN) {
		SHORT_CS_16 = cs;
		env->return_addr = (cs << 16) | ip;
		trans_interp_flags(env);    
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
		return;
	    }
	    invoke_data(env);    ------- TRANSFER_DATA or garbage 
#endif
	   }
	    e_printf(" CALLl opsize = 32 \n");
	    goto not_implemented;
/*9b*/	case WAIT:
            PC += 1; return (PC);
/*9c*/	case PUSHF: {
	    DWORD temp;
	    if (VM86F) goto not_permitted;
	    temp = trans_interp_flags(env);    
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
/*9d*/	case POPF: {
	    DWORD temp;
	    if (VM86F) goto not_permitted;
	    POPQUAD(temp);
	    trans_flags_to_interp(env, temp);
	    } PC += 1; return (PC);
/*9e*/	case SAHF: return (PC);
/*9f*/	case LAHF: return (PC);

/*a0*/	case MOVmal: {
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    AL = *mem_ref;
	    } PC += 3; return (PC);
/*a1*/	case MOVmax: {
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    EAX = FETCH_QUAD(mem_ref);
	    } PC += 3; return (PC);
/*a2*/	case MOValm: {
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    *mem_ref = AL;
	    } PC += 3; return (PC);
/*a3*/	case MOVaxm: {
	    DWORD temp = EAX;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    PUT_QUAD(mem_ref, temp);
	    } PC += 3; return (PC);
/*a4*/	case MOVSb: {
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    *dest = *src;
	    (env->flags & DIRECTION_FLAG)?(SI--,DI--):(SI++,DI++);
	    } PC += 1; return (PC);
/*a5*/	case MOVSw: {
	    BYTE *src, *dest;
	    DWORD temp;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    temp = FETCH_QUAD(src);
	    PUT_QUAD(dest, temp);
	    (env->flags & DIRECTION_FLAG)?(SI-=4,DI-=4):(SI+=4,DI+=4);
	    } PC += 1; return (PC);
/*a6*/	case CMPSb: {
	    DWORD src1, src2; int res;
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    src1 = *src;
	    src2 = *dest;
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(SI--,DI--):(SI++,DI++);
	    SETBFLAGS(1);
	    } PC += 1; return (PC);
/*a7*/	case CMPSw: {
	    DWORD res, src1;
	    signed long src2;
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    src1 = FETCH_QUAD(src);
	    src2 = FETCH_QUAD(dest);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(SI-=4,DI-=4):(SI+=4,DI+=4);
	    SETDFLAGS(1,1);
	    } PC += 1; return (PC);
/*a8*/	case TESTbi: return (PC);
/*a9*/	case TESTwi: {
	    DWORD res, src1, src2;
	    src1 = FETCH_QUAD((PC+1));
	    src2 = EAX; res = src1 & src2;
	    src1 = src2 = res;
	    CLEAR_CF; SETDFLAGS(0,0);
	    } PC += 5; return (PC);
/*aa*/	case STOSb:
	    LONG_ES[DI] = AL;
	    (env->flags & DIRECTION_FLAG)?DI--:DI++;
	    PC += 1; return (PC);
/*ab*/	case STOSw: /* STOSD */
	    PUT_QUAD(LONG_ES+DI, EAX);
	    (env->flags & DIRECTION_FLAG)?(DI-=4):(DI+=4);
	    PC += 1; return (PC);
/*ac*/	case LODSb: {
	    BYTE *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    AL = *seg;
	    (env->flags & DIRECTION_FLAG)?SI--:SI++;
	    } PC += 1; return (PC);
/*ad*/	case LODSw: {
	    BYTE *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    EAX = FETCH_QUAD(seg);
	    (env->flags & DIRECTION_FLAG)?(SI-=4):(SI+=4);
	    } PC += 1; return (PC);
/*ae*/	case SCASb: {
	    DWORD src1, src2; int res;
	    src1 = AL;
	    src2 = (LONG_ES[DI]);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?DI--:DI++;
	    SETBFLAGS(1);
	    } PC += 1; return (PC);
/*af*/	case SCASw: {
	    DWORD res, src1;
	    signed long src2;
	    src1 = EAX;
	    mem_ref = LONG_ES + (DI);
	    src2 = FETCH_QUAD(mem_ref);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(DI-=4):(DI+=4);
	    SETDFLAGS(1,1);
	    } PC += 1; return (PC);

/*b0*/	case MOVial:
/*b1*/	case MOVicl:
/*b2*/	case MOVidl:
/*b3*/	case MOVibl:
/*b4*/	case MOViah:
/*b5*/	case MOVich:
/*b6*/	case MOVidh:
/*b7*/	case MOVibh: return (PC);
/*b8*/	case MOViax:
	    EAX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
/*b9*/	case MOVicx:
	    ECX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
/*ba*/	case MOVidx:
	    EDX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
/*bb*/	case MOVibx:
	    EBX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
/*bc*/	case MOVisp:
	    ESP = FETCH_QUAD((PC+1)); 
	    PC += 5; return (PC);
/*bd*/	case MOVibp:
	    EBP = FETCH_QUAD((PC+1)); 
	    PC += 5; return (PC);
/*be*/	case MOVisi:
	    ESI = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
/*bf*/	case MOVidi:
	    EDI = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);

/*d0*/	case SHIFTb:
/*d2*/	case SHIFTbv:
/*c0*/	case SHIFTbi: {
	    int temp, count;
	    DWORD rbef, raft; BYTE opc;
	    opc = *PC;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC);
	    if (opc==SHIFTb) count = 1;
	      else if (opc==SHIFTbv) count = ECX & 0x1f;
	        else { count = *PC & 0x1f; PC += 1; }
	    /* are we sure that for count==0 CF is not changed? */
	    if (count) {
		mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			rbef = *mem_ref;
			count &= 7;	/* rotation modulo 8 */
			raft = (rbef << count) | (rbef >> (8 - count));
			*mem_ref = raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = raft & 1;
			if ((count==1) && ((rbef^raft)&0x80))
			    SRC1_8 = SRC2_8 = ~RES_8; /* will produce OF */
			return (PC);
		    case 1: /* ROR */
			rbef = *mem_ref; 
			count &= 7;
			raft = (rbef >> count) | (rbef << (8 - count));
			*mem_ref = raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (raft >> 7) & 1;
			if ((count==1) && ((rbef^raft)&0x80))
			    SRC1_8 = SRC2_8 = ~RES_8;
			return (PC);
		    case 2: /* RCL */
			rbef = *mem_ref; 
			count &= 7;
			raft = (rbef << count) | ((rbef >> (9 - count))
			    | ((CARRY & 1) << (count - 1)));
			*mem_ref = raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (rbef >> (8 - count)) & 1;
			if ((count==1) && ((rbef^raft)&0x80))
			    SRC1_8 = SRC2_8 = ~RES_8;
			return (PC);
		    case 3: /* RCR */
			rbef = *mem_ref; 
			count &= 7;
			raft = (rbef >> count) | ((rbef << (9 - count))
			    | ((CARRY & 1) << (8 - count)));
			*mem_ref = raft;
			BYTE_FLAG = BYTE_OP;
			CARRYB = (rbef >> (count - 1)) & 1;
			if ((count==1) && ((rbef^raft)&0x80))
			    SRC1_8 = SRC2_8 = ~RES_8;
			return (PC);
		    case 4: /* SHL/SAL */
			temp = *mem_ref;
			SRC1_8 = SRC2_8 = temp;
			CARRYB = (temp >> (8 - count)) & 1;
			temp = (temp << count);
			*mem_ref = temp;
			RES_8 = temp; BYTE_FLAG = BYTE_OP;
			return (PC);
		    case 5: /* SHR */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			CARRYB = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_8 = temp; BYTE_FLAG = BYTE_OP;
			return (PC);
		    case 6: /* Illegal */
			goto illegal_op;
		    case 7: /* SAR */
			temp = *mem_ref; 
			temp = ((temp << 24) >> 24);
			SRC1_8 = SRC2_8 = temp;
			CARRYB = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_8 = temp; BYTE_FLAG = BYTE_OP;
			return (PC);
		    default: goto not_implemented;
		} } else  return (PC);
	    }
/*d1*/	case SHIFTw:
/*d3*/	case SHIFTwv:
/*c1*/	case SHIFTwi: {
	    long temp, count; signed long res1;
	    DWORD src1, src2, res; BYTE opc;
	    opc = *PC;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    if (opc==SHIFTw) count = 1;
	      else if (opc==SHIFTwv) count = ECX & 0x1f;
	        else { count = *PC & 0x1f; PC += 1; }
	    /* are we sure that for count==0 CF is not changed? */
	    if (count) {
	      if (IS_MODE_REG) {
		DWORD *reg = (DWORD *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			src1 = *reg;
			res = (src1 << count) | (src1 >> (32 - count));
			*reg = res;
			CARRY = res & 1;	/* -> BYTE_FLAG=0 */
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16; /* will produce OF */
			return (PC);
		    case 1: /* ROR */
			src1 = *reg; 
			res = (src1 >> count) | (src1 << (32 - count));
			*reg = res;
			CARRY = (res >> 31) & 1;
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 2: /* RCL */
			src1 = *reg; 
			res = (src1 << count) | ((CARRY & 1) << (count - 1));
			if (count > 1) res |= (src1 >> (33 - count));
			*reg = res;
			CARRY = (src1 >> (32 - count)) & 1;
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 3: /* RCR */
			src1 = *reg;
			res = (src1 >> count) | ((CARRY & 1) << (32 - count));
                        if (count > 1) res |= (src1 << (33 - count));
			*reg = res;
			CARRY = (src1 >> (count - 1)) & 1;
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 4: /* SHL/SAL */
			res = *reg;
			src1 = src2 = res;
			CARRY = (res >> (32-count)) & 0x1;
			res = (res << count);
			*reg = res;
			SETDFLAGS(0,0);
			return (PC);
		    case 5: /* SHR */
			res = *reg;
			src1 = src2 = res;
			CARRY = (res >> (count-1)) & 1;
			res = res >> count;
			*reg = res;
			SETDFLAGS(0,0);
			return (PC);
		    case 6: /* Illegal */
			goto illegal_op;
		    case 7: { /* SAR */
			res1 = *(signed long *)reg;
			src1 = src2 = res1;
			CARRY = (res1 >> (count-1)) & 1;
			res = (res1 >> count);
			*reg = res;
			SETDFLAGS(0,0);
			return (PC);
			}
		    default: goto not_implemented;
		}
	      } else {
		mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			src1 = FETCH_QUAD(mem_ref);
			res = (src1 << count) | (src1 >> (32 - count));
			PUT_QUAD(mem_ref,res);
			CARRY = res & 1;	/* -> BYTE_FLAG=0 */
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16; /* will produce OF */
			return (PC);
		    case 1: /* ROR */
			src1 = FETCH_QUAD(mem_ref);
			res = (src1 >> count) | (src1 << (32 - count));
			PUT_QUAD(mem_ref,res);
			CARRY = (res >> 31) & 1;
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 2: /* RCL */
			src1 = FETCH_QUAD(mem_ref);
			res = (src1 << count) | ((CARRY & 1) << (count - 1));
			if (count > 1) res |= (src1 >> (33 - count));
			PUT_QUAD(mem_ref,res);
			CARRY = (src1 >> (32 - count)) & 1;
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 3: /* RCR */
			src1 = FETCH_QUAD(mem_ref);
			res = (src1 >> count) | ((CARRY & 1) << (32 - count));
                        if (count > 1) res |= (src1 << (33 - count));
			PUT_QUAD(mem_ref,res);
			CARRY = (src1 >> (count - 1)) & 1;
			if ((count==1) && ((src1^res)&0x80000000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 4: /* SHL/SAL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			CARRY = (res >> (32-count)) & 0x1;
			res = (res << count);
			PUT_QUAD(mem_ref,res);
			SETDFLAGS(0,0);
			return (PC);
		    case 5: /* SHR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			CARRY = (res >> (count-1)) & 1;
			res = res >> count;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS(0,0);
			return (PC);
		    case 6: /* Illegal */
			goto illegal_op;
		    case 7: { /* SAR */
			res1 = FETCH_QUAD(mem_ref);
			src1 = src2 = res1;
			CARRY = (res1 >> (count-1)) & 1;
			res = (res1 >> count);
			PUT_QUAD(mem_ref,res);
			SETDFLAGS(0,0);
			return (PC);
			}
		    default: goto not_implemented;
	      } } } else  return (PC);
	    }
/*c2*/	case RETisp: /*{
	    DWORD ip;
	    POPQUAD(ip);
	    ESP += (signed short)(FETCH_WORD((PC+1)));
	    PC = LONG_CS + ip;
	    } return (PC); */ goto not_implemented;
/*c3*/	case RET: /*{
	    DWORD ip;
	    POPQUAD(ip);
	    PC = LONG_CS + ip;
	    } return (PC); */ goto not_implemented;
/*c4*/	case LES: {
	    DWORD temp;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op;
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_QUAD(mem_ref);
	    *EREG1 = temp;
	    temp = FETCH_WORD(mem_ref+4);
	    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_ES_32 = temp;
	    } return (PC);
/*c5*/	case LDS: {
	    int temp;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op;
		return (PC);
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_QUAD(mem_ref);
	    *EREG1 = temp;
	    temp = FETCH_WORD(mem_ref+4);
	    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_DS_32 = temp;
	    } return (PC);
/*c6*/	case MOVbirm:
	    PC += hsw_modrm_16_byte(env,PC);
	    *MEM_REF = *PC; PC += 1; return (PC);
/*c7*/	case MOVwirm: {
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		*(DWORD *)mem_ref = FETCH_QUAD(PC);
		PC += 4; return (PC);
	    } else {
		*mem_ref = *PC;
		*(mem_ref+1)= *(PC+1);
		*(mem_ref+2)= *(PC+2);
		*(mem_ref+3)= *(PC+3);
		PC += 4; return (PC);
	    } } 
/*c8*/	case ENTER: {
	    goto not_implemented;
	    } PC += 4; return (PC);
/*c9*/	case LEAVE: {   /* 0xc9 */
	    DWORD temp;
	    ESP = EBP;
	    POPQUAD(temp);
	    EBP = temp;
	    } PC += 1; return (PC);
/*ca*/	case RETlisp: { /* restartable */
	    DWORD cs, ip; int dr;
	    WORD transfer_magic;
	    if (VM86F) goto not_implemented;
	    POPQUAD(ip);
	    cs = TOS_WORD;
	    dr = (signed short)FETCH_WORD((PC+1));
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		PUSHQUAD(ip); env->error_addr=cs; return P0; }
	    POPQUAD(cs); cs &= 0xffff;
	    SHORT_CS_16 = cs;
	    if (BIG_SS) ESP+=dr; else SP+=dr;
	    PC = ip + LONG_CS;
	    if (transfer_magic == TRANSFER_CODE16) {
		return (PC);
	    } else if (transfer_magic == TRANSFER_CODE32) {
		*err = EXCP_GOBACK;
		return (PC);
	    }
	    else
	      invoke_data(env);    /* TRANSFER_DATA or garbage */
	    }
/*cb*/	case RETl: { /* restartable */
	    DWORD cs, ip;
	    WORD transfer_magic;
	    if (VM86F) goto not_implemented;
	    POPQUAD(ip);
	    cs = TOS_WORD;
	    transfer_magic = (WORD)GetSelectorType(cs);
	    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		PUSHQUAD(ip); env->error_addr=cs; return P0; }
	    POPQUAD(cs); cs &= 0xffff;
	    SHORT_CS_16 = cs;
	    PC = ip + LONG_CS;
	    if (transfer_magic == TRANSFER_CODE16) {
		return (PC);
	    } else if (transfer_magic == TRANSFER_CODE32) {
		*err = EXCP_GOBACK;
		return (PC);
	    }
	    else
	      invoke_data(env);    /* TRANSFER_DATA or garbage */
	    } 
/*cc*/	case INT3: 
	    e_printf(" INT3l opsize = 32 \n");
	    goto not_implemented;
/*cd*/	case INT: /* {
	    DWORD temp, cs, ip = (DWORD)(PC - LONG_CS);
	    PUSHQUAD(ip);
	    cs = SHORT_CS_16;
	    PUSHQUAD(cs);
	    env->return_addr = (cs << 16)|ip;
	    temp =     trans_interp_flags(env);    
	    PUSHQUAD(temp);
	    EBP = EBP + (long)LONG_SS;
	    ESP = ESP + (long)LONG_SS;
	    INT_handler((DWORD)(*(PC+1)),env);
	    EBP = EBP - (long)LONG_SS;
	    ESP = ESP - (long)LONG_SS;
	    trans_flags_to_interp(env, env->flags);
	    } PC += 2; return (PC);
	    */
	e_printf(" INTl opsize = 32 \n");
	goto not_implemented;
/*ce*/	case INTO:
	e_printf(" INTOl opsize = 32 \n");
	goto not_implemented;
/*cf*/	case IRET: { /* restartable */
	    if (VM86F) goto not_permitted;
	    else {
		DWORD cs, ip, flags;
		WORD transfer_magic;
		POPQUAD(ip);
		cs = TOS_WORD;
		transfer_magic = (WORD)GetSelectorType(cs);
		if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		    PUSHQUAD(ip); env->error_addr=cs; return P0; }
		POPQUAD(cs); cs &= 0xffff;
		POPQUAD(flags);
		trans_flags_to_interp(env, flags);
		SHORT_CS_16 = cs;
		PC = ip + LONG_CS;
		if (transfer_magic == TRANSFER_CODE16) {
		    return (PC);
		} else if (transfer_magic == TRANSFER_CODE32) {
		    *err = EXCP_GOBACK;
		    return (PC);
		}
		else
		    invoke_data(env);    /* TRANSFER_DATA or garbage */
		} 
	    }

/*d0*/	    /* see before */
/*d1*/	    /* see before */
/*d2*/	    /* see before */
/*d3*/	    /* see before */
/*d4*/	case AAM: return (PC);
/*d5*/	case AAD: return (PC);
	case 0xd6:    /* illegal on 8086 and 80x86*/
	    goto illegal_op;
/*d7*/	case XLAT: {
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + (BX) + (AL);
	    AL = *mem_ref; }
	    PC += 1; return (PC);

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
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg0[funct](reg);
	    else hsw_fp87_mem0[funct](MEM_REF);
	    } break;
/*d9*/	case ESC1: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg1[funct](reg);
	    else hsw_fp87_mem1[funct](MEM_REF);
	    } break;
/*da*/	case ESC2: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg2[funct](reg);
	    else hsw_fp87_mem2[funct](MEM_REF);
	    } break;
/*db*/	case ESC3: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg3[funct](reg);
	    else hsw_fp87_mem3[funct](MEM_REF);
	    } break;
/*dc*/	case ESC4: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg4[funct](reg);
	    else hsw_fp87_mem4[funct](MEM_REF);
	    } break;
/*dd*/	case ESC5: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg5[funct](reg);
	    else hsw_fp87_mem5[funct](MEM_REF);
	    } break;
/*de*/	case ESC6: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg6[funct](reg);
	    else hsw_fp87_mem6[funct](MEM_REF);
	    } break;
/*df*/	case ESC7: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_quad(env,PC);
	    if (IS_MODE_REG) hsw_fp87_reg7[funct](reg);
	    else hsw_fp87_mem7[funct](MEM_REF);
	    } break;
    }
#ifdef DEBUG
    if (d.emu>3) e_debug_fp(&hsw_env87);
#endif
    if (*PC == WAIT) PC += 1;
    return (PC); }

/*e0*/	case LOOPNZ_LOOPNE: 
	    if ((--CX != 0) && (!IS_ZF_SET)) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e1*/	case LOOPZ_LOOPE: 
	    if ((--CX != 0) && (IS_ZF_SET)) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e2*/	case LOOP: 
	    if (--CX != 0) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e3*/	case JCXZ: 
	    if (CX == 0) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e5*/	case INw: {
	      BYTE a = *(PC+1);
	      if (VM86F || (CPL > IOPL)) {
		if ((a&3)||(!test_bit(a+3, io_bitmap))) goto not_permitted;
	      }
	      EAX = port_real_ind(a);
	      PC += 2; return (PC);
	    }
/*e7*/	case OUTw: {
	      BYTE a = *(PC+1);
	      if (VM86F || (CPL > IOPL)) {
		if ((a&3)||(!test_bit(a+3, io_bitmap))) goto not_permitted;
	      }
	      port_real_outw(a, AX);
	      PC += 2; return (PC);
	    }
/*ed*/	case INvw: {
	      if (VM86F || (CPL > IOPL)) {
		if ((DX>0x3fc)||(DX&3)||(!test_bit(DX+3, io_bitmap))) goto not_permitted;
	      }
	      EAX = port_real_ind(DX);
	      PC += 1; return (PC);
	    }
/*ef*/	case OUTvw: {
	      if (VM86F || (CPL > IOPL)) {
		if ((DX>0x3fc)||(DX&3)||(!test_bit(DX+3, io_bitmap))) goto not_permitted;
	      }
	      port_real_outd(DX, EAX);
	      PC += 1; return (PC);
	    }

/*e8*/	case CALLd: /* {
	    DWORD ip = PC - LONG_CS + 3;
	    PUSHQUAD(ip);
	    PC = LONG_CS + (ip + (signed short)(FETCH_QUAD((PC+1)))&0xffff);
	    } return (PC); */ goto not_implemented;
/*e9*/	case JMPd: /* {
	    DWORD ip = PC - LONG_CS + 3;
	    PC = LONG_CS + (ip + (signed short)(FETCH_QUAD((PC+1)))&0xffff);
	    } return (PC); */ goto not_implemented;
/*ea*/	case JMPld: /* {
	    DWORD cs, ip;
	    ip = FETCH_QUAD(PC+1);
	    cs = FETCH_WORD(PC+3);
	    SHORT_CS_16 = cs;
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		    env->error_addr=cs; return P0; }
		PC = ip + LONG_CS;
		return (PC);
	    }
	    env->return_addr = (cs << 16) | ip;
	    trans_interp_flags(env);
	    EBP = EBP + (long)LONG_SS;
	    ESP = ESP + (long)LONG_SS;
	    } return; */ goto not_implemented;
/*eb*/	case JMPsid: 
	    JUMP((PC+1)); return (PC);

/*f0*/	case LOCK:
	    PC += 1; return (PC);
	case 0xf1:    /* illegal on 8086 and 80x86 */
	    goto illegal_op;
/*f2*/	case REPNE:
/*f3*/	case REP:     /* also is REPE */
{
	    unsigned int count = CX;
	    int longd = 4;
	    BYTE repop,test;
	    DWORD res, src1=0, oldcnt; long src2=0;

	    repop = (*PC-REPNE);	/* 0 test !=, 1 test == */
	    PC += 2;
segrep:
	    switch (*(PC-1)) {
		case INSb:
		case INSw:
		case OUTSb:
		case OUTSw:
		    if (VM86F) goto not_permitted;
		    goto not_implemented;
		case MOVSw: { /* MOVSD */
		    int lcount = count * longd;
		    if (count == 0) return (PC);
		    instr_count += count;
		    if (env->flags & DIRECTION_FLAG) lcount = -lcount;
		    if (longd==2) {
		      WORD *src, *dest;
		      src = (WORD *)(ALLOW_OVERRIDE(LONG_DS)+(SI));
		      dest = (WORD *)(LONG_ES + DI);
		      if (lcount < 0) {
			while (count--) *dest-- = *src--;
		      } else {
			while (count--) *dest++ = *src++;
		      }
		    } else {
		      DWORD *src, *dest;
		      src = (DWORD *)(ALLOW_OVERRIDE(LONG_DS)+(SI));
		      dest = (DWORD *)(LONG_ES + DI);
		      if (lcount < 0) {
			while (count--) *dest-- = *src--;
		      } else {
			while (count--) *dest++ = *src++;
		      }
		    } 
		    DI += lcount; SI += lcount; CX = 0;
		    return (PC);
		    }
		case CMPSw: {
		    BYTE *src, *dest, *ovr;
		    if (count == 0) return (PC);
		    oldcnt = count;
		    src = (ovr=ALLOW_OVERRIDE(LONG_DS)) + (SI);
		    dest = LONG_ES + DI;
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
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
				res = src1 - src2;
				instr_count += (oldcnt-count);
				CX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ovr;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				return (PC);
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
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				CX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ovr;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    instr_count += count;
		    CX = 0;
		    DI = dest - LONG_ES;
		    SI = src - ovr;
		    if (longd==2) {
		      SETWFLAGS(1);
		    }
		    else {
		      SETDFLAGS(1,1);
		    }
		    } return (PC);
		case STOSw: {
		    BYTE *dest;
		    if (count == 0) return (PC);
		    instr_count += count;
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count * longd; ECX = 0;
			while (count--) {
			    if (longd==4) {PUT_QUAD(dest, EAX);}
			      else {PUT_WORD(dest, AX);}
			    dest -= longd;
			} return (PC);
		    } else {		      /* forwards */
			DI += count * longd; ECX = 0;
			while (count--) {
			    if (longd==4) {PUT_QUAD(dest, EAX);}
			      else {PUT_WORD(dest, AX);}
			    dest += longd;
			} return (PC);
		    } }
		case LODSw: {
		    BYTE *src;
		    count = count * longd;
		    if (count == 0) return (PC);
		    instr_count += count;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; SI -= count;
			if (longd==4) EAX = FETCH_QUAD(src);
			  else AX = FETCH_WORD(src);
			CX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; SI += count;
			if (longd==4) EAX = FETCH_QUAD(src);
			  else AX = FETCH_WORD(src);
			CX = 0; return (PC);
		    } }
		case SCASw: {
		    BYTE *dest;
		    if (count == 0) return (PC);
		    oldcnt = count;
		    dest = LONG_ES + DI;
		    src1 = EAX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = (longd==4? FETCH_QUAD(dest) : FETCH_WORD(dest));
			    dest -= longd;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				CX = count - 1;
				DI = dest - LONG_ES;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = (longd==4? FETCH_QUAD(dest) : FETCH_WORD(dest));
			    dest += longd;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				CX = count - 1;
				DI = dest - LONG_ES;
				if (longd==2) {
				  SETWFLAGS(1);
				}
				else {
				  SETDFLAGS(1,1);
				}
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    instr_count += count;
		    CX = 0; DI = dest - LONG_ES;
		    if (longd==2) {
		      SETWFLAGS(1);
		    }
		    else {
		      SETDFLAGS(1,1);
		    }
		    } return (PC);
/*----------IA--------------------------------*/
	        case SEGes:
       		    OVERRIDE = LONG_ES;
            	    PC+=1; goto segrep;
        	case SEGcs:
            	    OVERRIDE = LONG_CS;
          	    PC+=1; goto segrep;
        	case SEGss:
            	    OVERRIDE = LONG_SS;
            	    PC+=1; goto segrep;
        	case SEGds:
            	    OVERRIDE = LONG_DS;
            	    PC+=1; goto segrep;
        	case SEGfs:
            	    OVERRIDE = LONG_FS;
            	    PC+=1; goto segrep;
        	case SEGgs:
            	    OVERRIDE = LONG_GS;
            	    PC+=1; goto segrep;
		case OPERoverride:	/* 0x66 if we come from 32:32 */
            	    longd = 2;
		case ADDRoverride:	/* ignore 0x67 */
            	    PC+=1; goto segrep;
/*----------IA--------------------------------*/
		default: PC--; goto not_implemented;
	    } }
/*f4*/	case HLT:
	    goto not_implemented;
/*f5*/	case CMC: return (PC);
/*f6*/	case GRP1brm: {
	    DWORD src, src1, src2; int res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC);
	    mem_ref = MEM_REF; 
	    switch (res) {
		case 0: /* TEST */
		    src1 = *mem_ref;
		    src2 = *PC; PC += 1;
		    res = src1 & src2;
		    SETBFLAGS(0);
		    return (PC);
		case 1: /* TEST (Is this Illegal?) */
                    goto illegal_op; 
		case 2: /* NOT */
		    src1 = ~(*mem_ref);
		    *mem_ref = src1;
		    return (PC);
		case 3: /* NEG */
		    src = *mem_ref;
		    *mem_ref = res = src2 = -src;
		    src1 = 0;
		    SETBFLAGS(0);
		    CARRYB = (src != 0);
		    return (PC);
		case 4: /* MUL AL */
		case 5: /* IMUL AL */
		case 6: /* DIV AL */
		case 7: /* IDIV AX */
		    goto not_implemented;
	    } }
/*f7*/	case GRP1wrm: {
	    DWORD src1, src2, res;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_EREG(mem_ref);
		    src2 = FETCH_QUAD(PC); PC += 4;
		    res = src1 & src2;
		    src1 = src2 = res;
		    CLEAR_CF; SETDFLAGS(0,0);
		    return(PC);
		case 1: /* TEST (Is this Illegal?) */
                 goto illegal_op;
		case 2: /* NOT */
		    src1 = FETCH_EREG(mem_ref);
		    src1 = ~(src1);
		    *(DWORD *)mem_ref = src1;
		    return (PC);
		case 3: { /* NEG */
		    long src;
		    src = FETCH_EREG(mem_ref);
	    	    src2 = ((src==0x80000000)? 0:-src);
		    *(DWORD *)mem_ref = src2;
		    res = src2; src1 = 0;
		    SETDFLAGS(1,0);
		    CARRY = (src != 0);
		    } return (PC);
		case 4: { /* MUL EAX */
		    u_i64_u mres, mmlt;
		    mres.u.ul = EAX; mres.u.uh=0;
		    mmlt.u.ul = FETCH_EREG (mem_ref); mmlt.u.uh=0;
		    mres.ud *= mmlt.ud;
		    EAX = mres.u.ul;
		    EDX = mres.u.uh;
		    RES_32 = 0;
		    if (EDX) { SRC1_16 = SRC2_16 = 0x8000; SET_CF; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 5: { /* IMUL EAX */
		    s_i64_u mres, mmlt;
		    mres.s.sl = EAX;
		    mres.s.sh = (mres.s.sl<0? -1:0);
		    mmlt.s.sl = FETCH_EREG (mem_ref);
		    mmlt.s.sh = (mmlt.s.sl<0? -1:0);
		    mres.sd *= mmlt.sd;
		    EAX = mres.s.sl;
		    EDX = mres.s.sh;
		    RES_32 = 0;
		    if (((mres.s.sh+1)&(-2))!=0) {
			SRC1_16 = SRC2_16 = 0x8000; SET_CF;
		    }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 6: { /* DIV EDX+EAX */
		    u_i64_u divd;
		    DWORD temp, rem;
		    divd.u.ul = EAX; divd.u.uh = EDX;
		    temp = FETCH_EREG(mem_ref);
		    if (temp==0) goto div_by_zero;
		    rem = divd.ud % temp;
		    divd.ud /= temp;
		    if (divd.u.uh) goto div_by_zero;
		    EAX = divd.u.ul;
		    EDX = rem;
		    } return (PC);
		case 7: { /* IDIV EDX+EAX */
		    s_i64_u divd;
		    signed long temp, rem;
		    divd.s.sl = EAX; divd.s.sh = EDX;
		    temp = FETCH_EREG(mem_ref);
		    if (temp==0) goto div_by_zero;
		    rem = divd.sd % temp;
		    divd.sd /= temp;
		    if (((divd.s.sh+1)&(-2))!=0) goto div_by_zero;
		    EAX = divd.s.sl;
		    EDX = rem;
		    } return (PC);
	      }
	    } else {
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_QUAD(mem_ref);
		    src2 = FETCH_QUAD(PC); PC += 4;
		    res = src1 & src2;
		    src1 = src2 = res;
		    CLEAR_CF; SETDFLAGS(0,0);
		    return(PC);
		case 1: /* TEST (Is this Illegal?) */
                 goto illegal_op;
		case 2: /* NOT */
		    src1 = FETCH_QUAD(mem_ref);
		    src1 = ~(src1);
		    *(DWORD *)mem_ref = src1;
		    return (PC);
		case 3: { /* NEG */
		    long src;
		    src = FETCH_QUAD(mem_ref);
	    	    src2 = ((src==0x80000000)? 0:-src);
		    *(DWORD *)mem_ref = src2;
		    res = src2; src1 = 0;
		    SETDFLAGS(1,0);
		    CARRY = (src != 0);
		    } return (PC);
		case 4: { /* MUL EAX */
		    u_i64_u mres, mmlt;
		    mres.u.ul = EAX; mres.u.uh=0;
		    mmlt.u.ul = FETCH_QUAD (mem_ref); mmlt.u.uh=0;
		    mres.ud *= mmlt.ud;
		    EAX = mres.u.ul;
		    EDX = mres.u.uh;
		    RES_32 = 0;
		    if (EDX) { SRC1_16 = SRC2_16 = 0x8000; SET_CF; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 5: { /* IMUL EAX */
		    s_i64_u mres, mmlt;
		    mres.s.sl = EAX;
		    mres.s.sh = (mres.s.sl<0? -1:0);
		    mmlt.s.sl = FETCH_QUAD (mem_ref);
		    mmlt.s.sh = (mmlt.s.sl<0? -1:0);
		    mres.sd *= mmlt.sd;
		    EAX = mres.s.sl;
		    EDX = mres.s.sh;
		    RES_32 = 0;
		    if (((mres.s.sh+1)&(-2))!=0) {
			SRC1_16 = SRC2_16 = 0x8000; SET_CF;
		    }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 6: { /* DIV EDX+EAX */
		    u_i64_u divd;
		    DWORD temp, rem;
		    divd.u.ul = EAX; divd.u.uh = EDX;
		    temp = FETCH_QUAD(mem_ref);
		    if (temp==0) goto div_by_zero;
		    rem = divd.ud % temp;
		    divd.ud /= temp;
		    if (divd.u.uh) goto div_by_zero;
		    EAX = divd.u.ul;
		    EDX = rem;
		    } return (PC);
		case 7: { /* IDIV EDX+EAX */
		    s_i64_u divd;
		    signed long temp, rem;
		    divd.s.sl = EAX; divd.s.sh = EDX;
		    temp = FETCH_QUAD(mem_ref);
		    if (temp==0) goto div_by_zero;
		    rem = divd.sd % temp;
		    divd.sd /= temp;
		    if (((divd.s.sh+1)&(-2))!=0) goto div_by_zero;
		    EAX = divd.s.sl;
		    EDX = rem;
		    } return (PC);
		default: goto not_implemented;
	      } }
	      }
/*f8*/	case CLC: return (PC);
/*f9*/	case STC: return (PC);
/*fa*/	case CLI:
	    goto not_implemented;
/*fb*/	case STI:
	    goto not_implemented;
/*fc*/	case CLD: return (PC);
/*fd*/	case STD: return (PC);
/*fe*/	case GRP2brm: { /* only INC and DEC are legal on bytes */
	    int temp;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC);
	    mem_ref = MEM_REF;
	    switch (temp) {
		case 0: /* INC */
		    SRC1_8 = temp = *mem_ref;
		    *mem_ref = temp = temp + 1;
		    SRC2_8 = 1;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 1: /* DEC */
		    SRC1_8 = temp = *mem_ref;
		    *mem_ref = temp = temp - 1;
		    SRC2_8 = -1;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 2: /* CALL indirect */
		case 3: /* CALL long indirect */
		case 4: /* JMP indirect */
		case 5: /* JMP long indirect */
		case 6: /* PUSH */
		case 7: /* Illegal */
                    goto illegal_op; 
	    }}
/*ff*/	case GRP2wrm: {
	    DWORD temp;
	    DWORD res, src1, src2;
	    DWORD jcs, jip, ocs=0, oip=0;
	    WORD transfer_magic;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (temp) {
		case 0: /* INC */
		    res = FETCH_EREG(mem_ref);
		    src1 = res; src2 = 1;
		    res++;
		    *(DWORD *)mem_ref = res;
	    	    SETDFLAGS(0,0);
		    return (PC);
		case 1: /* DEC */
		    res = FETCH_EREG(mem_ref);
		    src1 = res; src2 = -1;
		    res--;
		    *(DWORD *)mem_ref = res;
	    	    SETDFLAGS(0,0);
		    return (PC);
		case 2: /* CALL indirect */
		    temp = PC - LONG_CS;
		    PUSHQUAD(temp);
		    PC = LONG_CS + FETCH_EREG(mem_ref);
		    return (PC);
		case 4: /* JMP indirect */
		    PC = LONG_CS + FETCH_EREG(mem_ref);
		    return (PC);
		case 3: /* CALL long indirect */
		    goto jff03m;
		case 5: /* JMP long indirect */
		    goto jff05m;
		case 6: /* PUSH */
		    temp = FETCH_EREG(mem_ref);
		    PUSHQUAD(temp);
		    return (PC);
		case 7: /* Illegal */
                 goto illegal_op;
		    return (PC);
		default: goto not_implemented;
	      }
	    } else {
	      switch (temp) {
		case 0: /* INC */
		    res = FETCH_QUAD(mem_ref);
		    src1 = res; src2 = 1;
		    res++;
		    PUT_QUAD(mem_ref, res);
	    	    SETDFLAGS(0,0);
		    return (PC);
		case 1: /* DEC */
		    res = FETCH_QUAD(mem_ref);
		    src1 = res; src2 = -1;
		    res--;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS(0,0);
		    return (PC);
		case 2: /* CALL indirect */
		    temp = PC - LONG_CS;
		    PUSHQUAD(temp);
		    temp = FETCH_QUAD(mem_ref);
		    PC = LONG_CS + temp;
		    return (PC);
		case 4: /* JMP indirect */
		    PC = LONG_CS + FETCH_QUAD(mem_ref);
		    return (PC);
		case 3: {  /* CALL long indirect */
jff03m:		    ocs = SHORT_CS_16;
		    oip = PC - LONG_CS;
		    if (VM86F) goto illegal_op;
		    }
		    /* fall through */
		case 5: {  /* JMP long indirect */
jff05m:		    if (VM86F) goto illegal_op;
		    jip = FETCH_QUAD(mem_ref);
		    jcs = FETCH_WORD(mem_ref+4);
		    transfer_magic = (WORD)GetSelectorType(jcs);
		    if (transfer_magic==TRANSFER_CODE16) {
			if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,jcs))==EXCP0B_NOSEG) {
			    env->error_addr=jcs; return P0; }
			if (temp==3) {
			    PUSHQUAD(ocs);
			    PUSHQUAD(oip);
			}
			SHORT_CS_16 = jcs;
			PC = jip + LONG_CS;
			if (CEmuStat&CeS_MODE_PM32) {
			    *err = EXCP_GOBACK;
			}
			return (PC);
		    }
		    else if (transfer_magic==TRANSFER_CODE32) {
			if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,jcs))==EXCP0B_NOSEG) {
			    env->error_addr=jcs; return P0; }
			if (temp==3) {
			    PUSHQUAD(ocs);
			    PUSHQUAD(oip);
			}
			SHORT_CS_16 = jcs;
			PC = jip + LONG_CS;
			if (!(CEmuStat&CeS_MODE_PM32)) {
			    *err = EXCP_GOBACK;
			}
			return (PC);
		    }
		    else
			invoke_data(env); /* TRANSFER_DATA or garbage */
		    }
		case 6: /* PUSH */
		    temp = FETCH_QUAD(mem_ref);
		    PUSHQUAD(temp);
		    return (PC);
		case 7: /* Illegal */
                 goto illegal_op;
		default: goto not_implemented;
	      }
	    } }
    default: goto not_implemented;
    }

not_implemented:
	d.emu=9;
	e_printf(" 32/16 nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*P0,*(P0+1),*(P0+2),
		SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifdef DEBUG
	e_debug(env, P0, P0, 2);
#endif
	FatalAppExit(0, "INSTR");
	exit(1);

bad_override:
	d.emu=9;
	e_printf(" 32/16 bad code/data sizes at %4x:%4x long PC %x\n",
		SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifdef DEBUG
	e_debug(env, P0, P0, 0);
#endif
	FatalAppExit(0, "SIZE");
	exit(1);

not_permitted:
	*err = EXCP0D_GPF; return P0;

div_by_zero:
	*err = EXCP00_DIVZ; return P0;

illegal_op:
	e_printf(" 32/16 illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*P0,*(P0+1),*(P0+2), 
                SHORT_CS_16,P0-LONG_CS,(int)P0);
	*err = EXCP06_ILLOP; return (P0);
}
