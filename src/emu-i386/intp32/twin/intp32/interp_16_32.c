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
	interp_16_32.c	1.13
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

#include "Log.h"
#include "bitops.h"
#include "port.h"
#include "hsw_interp.h"
#include "mod_rm.h"

extern long instr_count;
extern int in_dpmi_emu;
extern int emu_under_X;
#ifdef EMU_STAT
extern long InsFreq[];
#endif


#ifdef EMU_GLOBAL_VAR
extern Interp_VAR interp_variables;
extern Interp_ENV dosemu_env;
#define	env		(&dosemu_env)
#define interp_var	(&interp_variables)
BYTE *
hsw_interp_16_32 (register Interp_ENV *env1, BYTE *P0,
	register BYTE *PC, Interp_VAR *interpvar1, int *err)
{
#else
BYTE *
hsw_interp_16_32 (register Interp_ENV *env, BYTE *P0,
	register BYTE *PC, Interp_VAR *interp_var, int *err)
{
#endif
#ifdef DEBUG
    if (d.emu) e_debug(env, P0, PC, interp_var, 1);
#endif
    *err = 0;
override:
#ifdef EMU_STAT
    InsFreq[*PC] += 1;
#endif
    switch (*PC) {
/*00*/	case ADDbfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*01*/	case ADDwfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 + src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 + src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(0);
	    } return (PC); 
/*02*/	case ADDbtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*03*/	case ADDwtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 + src2;
	    SETWFLAGS(0);
	    } return (PC); 
/*04*/	case ADDbia: return (PC);
/*05*/	case ADDwia: {
	    DWORD res, src2; WORD src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 + src2;
	    SETWFLAGS(0);
	    } PC += 3; return (PC);
/*06*/	case PUSHes: {
	    WORD temp = SHORT_ES_16;
	    PUSHWORD(temp); 
	    } PC += 1; return (PC);
/*07*/	case POPes: {
	    DWORD temp;
	    POPWORD(temp);
	    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_ES_32 = temp; }
	    PC += 1; return (PC);
/*08*/	case ORbfrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*09*/	case ORwfrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 | src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 | src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(0);
	    } return (PC); 
/*0a*/	case ORbtrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*0b*/	case ORwtrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 | src2;
	    SETWFLAGS(0);
	    } return (PC); 
/*0c*/	case ORbi: return (PC);
/*0d*/	case ORwi: {
	    DWORD res, src2; WORD src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 | src2;
	    SETWFLAGS(0);
	    } PC += 3; return (PC);
/*0e*/	case PUSHcs: {
	    WORD temp = SHORT_CS_16;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*0f*/	case TwoByteESC: {
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    if (IS_MODE_REG) *(WORD *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } return (PC);
			case 1: /* STR */ {
			    /* Store Task Register */
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    if (IS_MODE_REG) *(WORD *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } return (PC);
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
			    PC = PC +1 + hsw_modrm_32_word(env,PC + 1,interp_var);
			    return (PC);
			case 3: /* LTR */ {
			    /* Load Task Register */
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_XREG(mem_ref);
			    else temp = FETCH_WORD(mem_ref);
		goto not_implemented;
			    /* hsw_task_register = temp; */
			    } 
			case 4: /* VERR */ {
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_XREG(mem_ref);
			    else temp = FETCH_WORD(mem_ref);
		goto not_implemented;
			    /* if (hsw_verr(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } 
			case 5: /* VERW */ {
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_XREG(mem_ref);
			    else temp = FETCH_WORD(mem_ref);
		goto not_implemented;
			    /* if (hsw_verw(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } 
			case 6: /* Illegal */
			case 7: /* Illegal */
		goto illegal_op;
			default: goto not_implemented;
		    }
		case 0x01: /* GRP7 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    e_printf("16/32 SGDT %#x\n",(int)mem_ref);
			    temp = 0; /* should be LIMIT field */;
			    PUT_WORD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref+2,temp);
			    } return (PC);
			case 1: /* SIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    e_printf("16/32 SIDT %#x\n",(int)mem_ref);
			    temp = 0; /* should be LIMIT field */;
			    PUT_WORD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref+2,temp);
			    } return (PC);
			case 2: /* LGDT */ /* Privileged */
			    /* Load Global Descriptor Table Register */
			    PC = PC+1+hsw_modrm_32_word(env,PC + 1,interp_var);
			    goto not_implemented;
			case 3: /* LIDT */ /* Privileged */
			    /* Load Interrupt Descriptor Table Register */
			    PC = PC+1+hsw_modrm_32_word(env,PC + 1,interp_var);
		goto not_implemented;
			case 4: /* SMSW */ {
			    /* Store Machine Status Word */
			    int temp; BYTE *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    e_printf("16/32 SMSW %#x\n",(int)mem_ref);
			    temp = 0 /* should be LIMIT field */;
			    if (IS_MODE_REG) *(WORD *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } return (PC);
			case 5: /* Illegal */
		goto illegal_op; 
			case 6: /* LMSW */ /* Privileged */
			    /* Load Machine Status Word */
			    PC = PC+1+hsw_modrm_32_word(env,PC + 1,interp_var);
		goto not_implemented;
			case 7: /* Illegal */
		goto illegal_op; 
			default: goto not_implemented;
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
		    int temp; BYTE *mem_ref;
			WORD _sel;
		    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
		    mem_ref = MEM_REF;
	 	    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
		if (IS_GDT(_sel)||((_sel>>3)>LDT_ENTRIES))
			RES_16 = 1;
		else {
		    temp = (GetSelectorFlags(_sel) & 0xff) << 8;
		    if (temp) SetFlagAccessed(_sel);
                        PUT_XREG(mem_ref,temp);
			RES_16 = 0;
		}
		    } return(PC); 
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
                    int temp; BYTE *mem_ref;
                        WORD _sel;
                    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
                    mem_ref = MEM_REF;
                    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
		if (IS_GDT(_sel)||((_sel>>3)>LDT_ENTRIES))
                        RES_16 = 1;
                else {
                temp= (WORD)GetSelectorByteLimit(_sel);
                        PUT_XREG(mem_ref,temp);
                        RES_16 = 0;
                }
                    /* what do I do here??? */
		    } return(PC); 
		case 0x06: /* CLTS */ /* Privileged */
		    /* Clear Task State Register */
		    PC += 2; return (PC);
		case 0x08: /* INVD */
		    /* INValiDate cache */
		    PC += 2; return (PC);
		case 0x09: /* WBINVD */
		    /* Write-Back and INValiDate cache */
		    PC += 2; return (PC);
		case 0x20: /* MOVcdrd */ /* Privileged */
		goto not_implemented;

		case 0x21: /* MOVddrd */ /* Privileged */
		goto not_implemented;

		case 0x22: /* MOVrdcd */ /* Privileged */
		goto not_implemented;
		
		case 0x23: /* MOVrddd */ /* Privileged */
		goto not_implemented;
	
		case 0x24: /* MOVtdrd */ /* Privileged */
		goto not_implemented;

		case 0x26: /* MOVrdtd */ /* Privileged */
		goto not_implemented;

		case 0x80: /* JOimmdisp */
		    if (IS_OF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x81: /* JNOimmdisp */
		    if (!IS_OF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x82: /* JBimmdisp */
		    if (IS_CF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x83: /* JNBimmdisp */
		    if (!IS_CF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x84: /* JZimmdisp */
		    if (IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x85: /* JNZimmdisp */
		    if (!IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x86: /* JBEimmdisp */
		    if (IS_CF_SET || IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x87: /* JNBEimmdisp */
		    if (!(IS_CF_SET || IS_ZF_SET)) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x88: /* JSimmdisp */
		    if (IS_SF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x89: /* JNSimmdisp */
		    if (!IS_SF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x8a: /* JPimmdisp */
		    if (IS_PF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x8b: /* JNPimmdisp */
		    if (!IS_PF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x8c: /* JLimmdisp */
		    if (IS_SF_SET ^ IS_OF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x8d: /* JNLimmdisp */
		    if (!(IS_SF_SET ^ IS_OF_SET)) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x8e: /* JLEimmdisp */
		    if ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x8f: /* JNLEimmdisp */
		    if (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) {
			int temp = FETCH_QUAD(PC+2);
			PC += (6 + temp);
			return (PC);
		    } PC += 6; return (PC);
		case 0x90: /* SETObrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x91: /* SETNObrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x92: /* SETBbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x93: /* SETNBbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x94: /* SETZbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x95: /* SETNZbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x96: /* SETBEbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x97: /* SETNBEbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x98: /* SETSbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET) ? 1 : 0;
		    } return (PC);
		case 0x99: /* SETNSbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9a: /* SETPbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9b: /* SETNPbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9c: /* SETLbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9d: /* SETNLbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9e: /* SETLEbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9f: /* SETNLEbrm */ {
		    BYTE *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0xa0: /* PUSHfs */ {
		    WORD temp = SHORT_FS_16;
		    PUSHWORD(temp);
		    } PC += 2; return (PC);
		case 0xa1: /* POPfs */ {
		    DWORD temp;
		    POPWORD(temp);
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_FS_32 = temp;
		    } PC += 2; return (PC);
                case 0xa3: /* BT */ {
                    BYTE *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *XREG1;
             	    ind = ((ind << 16) >> 16);
                    if (IS_MODE_REG) {
                        ind = (ind & 0xf);
                        temp = *(WORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                    } else {
                        if(ind >= 0) {
                            ind1 = (ind >> 4) << 1;
                            mem_ref += ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (temp >> ind) & 0x1;
                        } else {
                            ind = -ind - 1;
                            ind1 = ((ind >> 4) + 1) << 1;
                            mem_ref -= ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (((temp << ind) & 0x8000)? 1:0);
                        }
                    }          
                    } return(PC);
		case 0xa4: /* SHLDimm */ {
		    /* Double Precision Shift Left */
		    BYTE *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = *PC & 0xf; PC++;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    *(WORD *)mem_ref = temp;
			} else {
			    temp = FETCH_WORD(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    PUT_WORD(mem_ref,temp);
			}
			RES_32 = temp;
			SRC1_16 = SRC2_16 = temp >> 1;
			CARRY = temp & 1;
		    } return (PC);
		case 0xa5: /* SHLDcl */ {
		    /* Double Prescision Shift Left by CL */
		    BYTE *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = CX & 0xf;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    *(WORD *)mem_ref = temp;
			} else {
			    temp = FETCH_WORD(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    PUT_WORD(mem_ref,temp);
			}
			RES_32 = temp;
			SRC1_16 = SRC2_16 = temp >> 1;
			CARRY = temp & 1;
		    } return (PC);
		case 0xa6: /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
		case 0xa7: /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		goto not_implemented;
		case 0xa8: /* PUSHgs */ {
		    WORD temp = SHORT_GS_16;
		    PUSHWORD(temp);
		    } PC += 2; return (PC);
		case 0xa9: /* POPgs */ {
		    DWORD temp;
		    POPWORD(temp);
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_GS_32 = temp;
		    } PC += 2; return (PC);
                case 0xab: /* BTS */ {
                    BYTE *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *XREG1;
                    ind = ((ind << 16) >> 16);
                    if (IS_MODE_REG) {
                        ind = (ind & 0xf);
                        temp = *(WORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                        temp |= (0x1 << ind);
                        *(WORD *)mem_ref = temp;
                    } else {
                        if(ind >= 0) {
                            ind1 = ((ind >> 4) << 1);
                            mem_ref += ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (temp >> ind) & 0x1;
                            temp |= (0x1 << ind);
                            PUT_WORD(mem_ref,temp);
                        } else {
                            ind = -ind - 1;
                            ind1 = ((ind >> 4) + 1) << 1;
                            mem_ref -= ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (((temp << ind) & 0x8000)? 1:0);
                            temp |= (0x8000 >> ind);
                            PUT_WORD(mem_ref,temp);
                        }
                    }
                    } return(PC);
		case 0xac: /* SHRDimm */ {
		    /* Double Precision Shift Right by immediate */
		    BYTE *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = FETCH_WORD(PC) & 0xf; PC += 2;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (16 - count);
			    temp |= temp1;
			    *(WORD *)mem_ref = temp;
			} else {
			    temp = FETCH_WORD(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (16 - count);
			    temp |= temp1;
			    PUT_WORD(mem_ref,temp);
			}
                        RES_16 = temp;
                        SRC1_16 = SRC2_16 = temp <<1;
		    } return (PC);
		case 0xad: /* SHRDcl */ {
		    /* Double Precision Shift Right by CL */
		    BYTE *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = CX & 0xf;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (16 - count);
			    temp |= temp1;
			    *(WORD *)mem_ref = temp;
			} else {
			    temp = FETCH_WORD(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (16 - count);
			    temp |= temp1;
			    PUT_WORD(mem_ref,temp);
			}
                        RES_16 = temp;
                        SRC1_16 = SRC2_16 = temp <<1;
		    } return (PC);
                case 0xaf: { /* IMULregrm */
                        int res, src1, src2; BYTE *mem_ref;
                        PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                        src1 = *XREG1; mem_ref = MEM_REF;
                        if (IS_MODE_REG)
                            src2 = FETCH_XREG(mem_ref);
                        else
                            src2 = FETCH_WORD(mem_ref);
                        res = src1 * src2;
                        *XREG1 = res; res = (res >> 16);
                        SRC1_16 = SRC2_16 = 0;
                        RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
                        } return(PC);
		case 0xb2: /* LSS */ {
		    int temp; BYTE *mem_ref;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref);
		    *XREG1 = temp;
		    temp = FETCH_WORD(mem_ref+2);
		    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_SS_32 = temp;
		    } return (PC);
                case 0xb3: /* BTR */ {
                    BYTE *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *XREG1;
                    ind = ((ind << 16) >> 16);
                    if (IS_MODE_REG) {
                        ind = (ind & 0xf);
                        temp = *(WORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                        temp &= ~(0x1 << ind);
                        *(WORD *)mem_ref = temp;
                    } else {
                        if(ind >= 0) {
                            ind1 = ((ind >> 4) << 1);
                            mem_ref += ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (temp >> ind) & 0x1;
                            temp &= ~(0x1 << ind);
                            PUT_WORD(mem_ref,temp);
                        } else {
                            ind = -ind - 1;
                            ind1 = ((ind >> 4) + 1) << 1;
                            mem_ref -= ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (((temp << ind) & 0x8000)? 1:0);
                            temp &= ~(0x8000 >> ind);
                            PUT_WORD(mem_ref,temp);
                        }
                    }
                    } return(PC);
		case 0xb4: /* LFS */ {
		    int temp; BYTE *mem_ref;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref);
		    *XREG1 = temp;
		    temp = FETCH_WORD(mem_ref+2);
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_FS_32 = temp;
		    } return (PC);
		case 0xb5: /* LGS */ {
		    int temp; BYTE *mem_ref;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref);
		    *XREG1 = temp;
		    temp = FETCH_WORD(mem_ref+2);
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
		    	env->error_addr=temp; return P0; }
		    SHORT_GS_32 = temp;
		    } return (PC);
		case 0xb6: /* MOVZXb */ {
		    DWORD temp;
		    int ref = (*(PC+2)>>3)&7;
		    PC = PC+1+hsw_modrm_32_byte(env,PC+1,interp_var);
		    temp = *(BYTE *)MEM_REF;
		    switch (ref) {
		      case 0: AX = temp; return (PC);
		      case 1: CX = temp; return (PC);
		      case 2: DX = temp; return (PC);
		      case 3: BX = temp; return (PC);
		      case 4: SP = temp; return (PC);
		      case 5: BP = temp; return (PC);
		      case 6: SI = temp; return (PC);
		      case 7: DI = temp; return (PC);
		    }
		    }
		case 0xb7: { /* MOVZXw */
             	    DWORD temp;
                    BYTE *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = FETCH_XREG(mem_ref);
                    else
                        temp = FETCH_WORD(mem_ref);
                    *XREG1 = (WORD)temp;
		    } return(PC);
		case 0xba: /* GRP8 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* Illegal */
			case 1: /* Illegal */
			case 2: /* Illegal */
			case 3: /* Illegal */
		goto illegal_op; 
                        case 4: /* BT */ {
                            BYTE *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = *PC;  PC++;
                            if (IS_MODE_REG)
                                temp1 = *(WORD *)mem_ref;
                            else   
                                temp1 = FETCH_WORD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0xf))&1;
                    } return(PC);
                        case 5: /* BTS */ {
                            BYTE *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = (*PC) & 0xf;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(WORD *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
                                *(WORD *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_WORD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
                                PUT_WORD(mem_ref,temp1);
			    }
                    } return(PC);
                        case 6: /* BTR */ {
                            BYTE *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = (*PC) & 0xf;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(WORD *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
                                *(WORD *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_WORD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
                                PUT_WORD(mem_ref,temp1);
			    }
                    } return(PC);
                        case 7: /* BTC */ {
                            BYTE *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = (*PC) & 0xf;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(WORD *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
                                *(WORD *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_WORD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
                                PUT_WORD(mem_ref,temp1);
			    }
                    } return(PC);
                    }
                case 0xbb: /* BTC */ {
                    BYTE *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *XREG1;
                    ind = ((ind << 16) >> 16);
                    if (IS_MODE_REG) {
                        ind = (ind & 0xf);
                        temp = *(WORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                        temp ^= (0x1 << ind);
                        *(WORD *)mem_ref = temp;
                    } else {
                        if(ind >= 0) {
                            ind1 = ((ind >> 4) << 1);
                            mem_ref += ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (temp >> ind) & 0x1;
                            temp ^= (0x1 << ind);
                            PUT_WORD(mem_ref,temp);
                        } else {
                            ind = -ind - 1;
                            ind1 = ((ind >> 4) + 1) << 1;
                            mem_ref += ind1;
                            temp = FETCH_WORD(mem_ref);
                            ind = ind & 0xf;
                            CARRY = (((temp << ind) & 0x8000)? 1:0);
                            temp ^= (0x8000 >> ind);
                            PUT_WORD(mem_ref,temp);
                        }
                    }
                    } return(PC);
                case 0xbc: /* BSF */ {
                    DWORD temp, i;
		    BYTE *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = *(WORD *)mem_ref;
                    else
                        temp = FETCH_WORD(mem_ref);
		    if(temp) {
                    for(i=0; i<16; i++)
                        if((temp >> i) & 0x1) break;
                    *XREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } return(PC);
                case 0xbd: /* BSR */ {
                    DWORD temp; int i;
		    BYTE *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = *(WORD *)mem_ref;
                    else
                        temp = FETCH_WORD(mem_ref);
		    if(temp) { 
                    for(i=15; i>=0; i--)
                        if((temp & ( 0x1 << i))) break;
                    *XREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } return(PC);
		case 0xbe: /* MOVSXb */ {
                    signed long temp;
		    int ref = (*(PC+2)>>3)&7;
		    PC = PC+1+hsw_modrm_32_byte(env,PC+1,interp_var);
		    temp = *(signed char *)MEM_REF;
		    switch (ref) {
		      case 0: AX = ((temp<<24)>>24); return (PC);
		      case 1: CX = ((temp<<24)>>24); return (PC);
		      case 2: DX = ((temp<<24)>>24); return (PC);
		      case 3: BX = ((temp<<24)>>24); return (PC);
		      case 4: SP = ((temp<<24)>>24); return (PC);
		      case 5: BP = ((temp<<24)>>24); return (PC);
		      case 6: SI = ((temp<<24)>>24); return (PC);
		      case 7: DI = ((temp<<24)>>24); return (PC);
		    }
		    }
		case 0xbf: { /* MOVSXw */
		    signed long temp; 
                    BYTE *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = FETCH_XREG(mem_ref);
                    else
                        temp = FETCH_WORD(mem_ref);
                    *XREG1 = (WORD)temp;
		    } return(PC); 
                case 0xc0: { /* XADDb */
                    int res,src1,src2; BYTE *mem_ref;
                    PC = PC+1+hsw_modrm_32_byte(env,PC+1,interp_var);
                    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
                    *HREG1 = src1;
                    *mem_ref = res = src1 + src2;
                    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
                    SRC1_8 = src1; SRC2_8 = src2;
                    } return(PC);
                case 0xc1: { /* XADDw */
                    int res,src1,src2; BYTE *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    src2 = *XREG1; mem_ref = MEM_REF;
                    if (IS_MODE_REG) {
                        src1 = FETCH_XREG(mem_ref);
                        res = src1 + src2;
                        PUT_XREG(mem_ref, res);
                    } else {
                        src1 = FETCH_WORD(mem_ref);
                        res = src1 + src2;
                        PUT_WORD(mem_ref, res);
                    }
                    *XREG1 = src1;
                    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
                    } return(PC);
		default: goto not_implemented;
		}
	    }

/*10*/	case ADCbfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*11*/	case ADCwfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 + src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 + src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(0);
	    } return (PC); 
/*12*/	case ADCbtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*13*/	case ADCwtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 + src2;
	    SETWFLAGS(0);
	    } return (PC); 
/*14*/	case ADCbi: return (PC);
/*15*/	case ADCwi: {
	    DWORD res, src2; WORD src1;
	    src1 = AX; 
	    src2 = (FETCH_WORD((PC+1))) + (CARRY & 1);
	    AX = res = src1 + src2;
	    SETWFLAGS(0);
	    } PC += 3; return (PC);
/*16*/	case PUSHss: {
	    WORD temp = SHORT_SS_16;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*17*/	case POPss: {
	    DWORD temp;
	    POPWORD(temp);
	    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_SS_32 = temp;
	    } PC += 1; return (PC);
/*18*/	case SBBbfrm: {
	    DWORD src1, src2; int res; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*19*/	case SBBwfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 - src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 - src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(1);
	    } return (PC); 
/*1a*/	case SBBbtrm: {
	    DWORD src1, src2; int res; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*1b*/	case SBBwtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    src2 = src2 + (CARRY & 1);
	    *XREG1 = res = src1 - src2;
	    SETWFLAGS(1);
	    } return (PC); 
/*1c*/	case SBBbi: return (PC);
/*1d*/	case SBBwi: {
	    DWORD res, src1, src2;
	    src1 = AX; 
	    src2 = (FETCH_WORD((PC+1))) + (CARRY & 1);
	    AX = res = src1 - src2;
	    SETWFLAGS(1);
	    } PC += 3; return (PC);
/*1e*/	case PUSHds: {
	    WORD temp = SHORT_DS_16;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*1f*/	case POPds: {
	    WORD temp;
	    POPWORD(temp);
	    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_DS_32 = temp;
	    } PC += 1; return (PC);

/*20*/	case ANDbfrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*21*/	case ANDwfrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 & src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 & src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(0);
	    } return (PC); 
/*22*/	case ANDbtrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*23*/	case ANDwtrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 & src2;
	    SETWFLAGS(0);
	    } return (PC); 
/*24*/	case ANDbi:
/*25*/	case ANDwi: {
            int res, src1, src2;
            src1 = AX; src2 = FETCH_WORD((PC+1));
            AX = res = src1 & src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
            } PC += 3; return (PC);
/*26*/	case SEGes:
	    OVERRIDE = LONG_ES;
	    PC += 1; goto override;
/*27*/	case DAA: return (PC);
/*28*/	case SUBbfrm: {
	    DWORD src1, src2; int res; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*29*/	case SUBwfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 - src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 - src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(1);
	    } return (PC); 
/*2a*/	case SUBbtrm: {
	    DWORD src1, src2; int res; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*2b*/	case SUBwtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 - src2;
	    SETWFLAGS(1);
	    } return (PC); 
/*2c*/	case SUBbi: return (PC);
/*2d*/	case SUBwi: {
	    DWORD res, src2; WORD src1;
	    src1 = AX; 
	    src2 = FETCH_WORD((PC+1));
	    AX = res = src1 - src2;
	    SETWFLAGS(1);
	    } PC += 3; return (PC);
/*2e*/	case SEGcs:
	    OVERRIDE = LONG_CS;
	    PC+=1; goto override;
/*2f*/	case DAS: return (PC);

/*30*/	case XORbfrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*31*/	case XORwfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 ^ src2;
		PUT_XREG(mem_ref, res);
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 ^ src2;
		PUT_WORD(mem_ref, res);
	    }
	    SETWFLAGS(0);
	    } return (PC); 
/*32*/	case XORbtrm: {
	    DWORD res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*33*/	case XORwtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 ^ src2;
	    SETWFLAGS(0);
	    } return (PC); 
/*34*/	case XORbi: return (PC);
/*35*/	case XORwi: {
	    DWORD res, src2; WORD src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 ^ src2;
	    SETWFLAGS(0);
	    } PC += 3; return (PC);
/*36*/	case SEGss:
	    OVERRIDE = LONG_SS;
	    PC+=1; goto override;
/*37*/	case AAA: return (PC);
/*38*/	case CMPbfrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*39*/	case CMPwfrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    res = src1 - src2;
	    SETWFLAGS(1);
	    } return (PC); 
/*3a*/	case CMPbtrm: {
	    DWORD src1, src2; int res;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; src1 = *HREG1;
	    res = src1 - src2;
	    SETBFLAGS(1);
	    } return (PC); 
/*3b*/	case CMPwtrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    res = src1 - src2;
	    SETWFLAGS(1);
	    } return (PC); 
/*3c*/	case CMPbi: return (PC);
/*3d*/	case CMPwi: {
            int res, src1;
            long src2;
            src1 = AX;
            src2 = FETCH_WORD((PC+1));
            res = src1 - src2;
            SETWFLAGS(1);
            } PC += 3; return (PC);
/*3e*/	case SEGds:
	    OVERRIDE = LONG_DS;
	    PC+=1; goto override;
/*3f*/	case AAS: return (PC);

/*40*/	case INCax: {
	    DWORD res, src1;
	    src1 = AX; 
	    AX = res = src1 + 1;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = 1;
	    } PC += 1; return (PC);
/*41*/	case INCcx: {
	    DWORD res, src1, src2;
	    src1 = CX; src2 = 1;
	    CX = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*42*/	case INCdx: {
	    DWORD res, src1, src2;
	    src1 = DX; src2 = 1;
	    DX = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*43*/	case INCbx: {
	    DWORD res, src1, src2;
	    src1 = BX; src2 = 1;
	    BX = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*44*/	case INCsp: {
	    DWORD res, src1, src2;
	    src1 = SP; src2 = 1;
	    SP = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*45*/	case INCbp: {
	    DWORD res, src1, src2;
	    src1 = BP; src2 = 1;
	    res = src1 + src2;
	    BP = RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*46*/	case INCsi: {
	    DWORD res, src1, src2;
	    src1 = SI; src2 = 1;
	    SI = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*47*/	case INCdi: {
	    DWORD res, src1, src2;
	    src1 = DI; src2 = 1;
	    DI = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; return (PC);
/*48*/	case DECax: {
	    int res, src1, src2;
	    src1 = AX; src2 = 1;
	    AX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*49*/	case DECcx: {
	    int res, src1, src2;
	    src1 = CX; src2 = 1;
	    CX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*4a*/	case DECdx: {
	    int res, src1, src2;
	    src1 = DX; src2 = 1;
	    DX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*4b*/	case DECbx: {
	    int res, src1, src2;
	    src1 = BX; src2 = 1;
	    BX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*4c*/	case DECsp: {
	    int res, src1, src2;
	    src1 = SP; src2 = 1;
	    SP = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*4d*/	case DECbp: {
	    int res, src1, src2;
	    src1 = BP; src2 = 1;
	    BP = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*4e*/	case DECsi: {
	    int res, src1, src2;
	    src1 = SI; src2 = 1;
	    SI = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);
/*4f*/	case DECdi: {
	    int res, src1, src2;
	    src1 = DI; src2 = 1;
	    DI = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; return (PC);

/*50*/	case PUSHax: {
	    WORD temp = AX;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*51*/	case PUSHcx: {
	    WORD temp = CX;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*52*/	case PUSHdx: {
	    WORD temp = DX;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*53*/	case PUSHbx: {
	    WORD temp = BX;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*54*/	case PUSHsp: {
	    WORD temp = SP;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*55*/	case PUSHbp: {
	    WORD temp = BP;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*56*/	case PUSHsi: {
	    WORD temp = SI;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*57*/	case PUSHdi: {
	    WORD temp = DI;
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*58*/	case POPax: POPWORD(AX); PC += 1; return (PC);
/*59*/	case POPcx: POPWORD(CX); PC += 1; return (PC);
/*5a*/	case POPdx: POPWORD(DX); PC += 1; return (PC);
/*5b*/	case POPbx: POPWORD(BX); PC += 1; return (PC);
/*5c*/	case POPsp: {
	    DWORD temp;
	    POPWORD(temp);
	    SP = temp;
	    } PC += 1; return (PC);
/*5d*/	case POPbp: POPWORD(BP); PC += 1; return (PC);
/*5e*/	case POPsi: POPWORD(SI); PC += 1; return (PC);
/*5f*/	case POPdi: POPWORD(DI); PC += 1; return (PC);

/*60*/	case PUSHA: {
	    DWORD temp;
	    WORD tempsp = SP;
	    temp = AX; PUSHWORD(temp);
	    temp = CX; PUSHWORD(temp);
	    temp = DX; PUSHWORD(temp);
	    temp = BX; PUSHWORD(temp);
	    PUSHWORD(tempsp);
	    tempsp = BP;
	    PUSHWORD(tempsp);
	    temp = SI; PUSHWORD(temp);
	    temp = DI; PUSHWORD(temp);
	    } PC += 1; return (PC);
/*61*/	case POPA: {
	    WORD temp;
	    POPWORD(DI);
	    POPWORD(SI);
	    POPWORD(temp);
	    BP = temp;
	    SP += 2;
	    POPWORD(BX);
	    POPWORD(DX);
	    POPWORD(CX);
	    POPWORD(AX);
	    } PC += 1; return (PC);
/*62*/	case BOUND: 
/*63*/	case ARPL:
	    goto not_implemented;
/*64*/	case SEGfs:
	    OVERRIDE = LONG_FS;
	    PC+=1; goto override;
/*65*/	case SEGgs:
	    OVERRIDE = LONG_GS;
	    PC+=1; goto override;
/*66*/	case OPERoverride:	/* 0x66: 32 bit operand, 16 bit addressing */
	    if (!data32) {
		if (d.emu>4) e_printf("ENTER interp_32d_32a\n");
		data32 = 1;
		PC = hsw_interp_32_32 (env, P0, PC+1, interp_var, err, 1);
		data32 = 0;
		return (PC);
	    }
	    PC += 1;
	    goto override;
/*67*/	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
	    if (code32) {
		if (d.emu>4) e_printf("ENTER interp_16d_16a\n");
		code32 = 0;
		PC = hsw_interp_16_16 (env, P0, PC+1, interp_var, err, 1);
		code32 = 1;
		return (PC);
	    }
	    PC += 1;
	    goto override;
/*68*/	case PUSHwi: {
	    WORD temp;
	    temp = FETCH_WORD(PC+1);
	    PUSHWORD(temp);
	    } PC += 3; return (PC);
/*69*/	case IMULwrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = FETCH_WORD(PC); PC += 2; mem_ref = MEM_REF; 
	    if (IS_MODE_REG)
		src1 = FETCH_XREG(mem_ref);
	    else 
		src1 = FETCH_WORD(mem_ref);
            src1 = ((src1<<16)>>16);
            src2 = ((src2<<16)>>16);
	    res = src1 * src2;
	    *XREG1 = res; res = (res >> 16);
	    SRC1_16 = SRC2_16 = 0;
	    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
	    } return (PC); 
/*6a*/	case PUSHbi: return (PC);
/*6b*/	case IMULbrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *(signed char *)(PC); PC += 1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG)
		src1 = FETCH_XREG(mem_ref);
	    else
		src1 = FETCH_WORD(mem_ref);
            src1 = ((src1<<16)>>16);
	    res = src1 * src2;
	    *XREG1 = res; res = res >> 16;
	    SRC1_16 = SRC2_16 = 0;
	    RES_32 = ((res==0)||(res==0xffff))?0:-1;
	    BYTE_FLAG=BYTE_OP;
	    } return (PC); 
/*6c*/	case INSb:
/*6d*/	case INSw:
/*6e*/	case OUTSb:
/*6f*/	case OUTSw:
            goto not_implemented;

/*82*/	case IMMEDbrm2:    /* out of order */
/*80*/	case IMMEDbrm: {
	    int src1, src2, res; BYTE *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
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
	    int src1, src2, res; BYTE *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = FETCH_WORD(PC); PC += 2;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_XREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETWFLAGS(1);
		    return (PC);
		default: goto not_implemented;
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_WORD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETWFLAGS(1);
		    return (PC);
		default: goto not_implemented;
	      }
	    }}
/*83*/	case IMMEDisrm: {
	    int src1, src2, res; BYTE *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = (signed char)*(PC); PC += 1;
	    src2 = src2 & 0xffff;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_XREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETWFLAGS(1);
		    return (PC);
		default: goto not_implemented;
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_WORD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(1);
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_WORD(mem_ref, res);
		    SETWFLAGS(0);
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    SETWFLAGS(1);
		    return (PC);
		default: goto not_implemented;
	      }
	    }}
/*84*/	case TESTbrm: {
	    DWORD res, src1, src2;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 & src2;
	    SETBFLAGS(0);
	    } return (PC); 
/*85*/	case TESTwrm: {
	    int res, src1, src2; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 & src2;
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 & src2;
	    }
	    SETWFLAGS(0);
	    } return (PC); 
/*86*/	case XCHGbrm: {
	    BYTE temp;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    temp = *MEM_REF; *MEM_REF = *HREG1; *HREG1 = temp;
	    } return (PC); 
/*87*/	case XCHGwrm: {
	    WORD temp; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		temp = FETCH_XREG(mem_ref);
		*(WORD *)mem_ref = *XREG1;
		*XREG1 = temp;
		return (PC); 
	    } else {
		WORD temp1 = FETCH_WORD(mem_ref);
		temp = *XREG1; *XREG1 = temp1;
                *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
		return (PC); 
	    }
	    }

/*88*/	case MOVbfrm:
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    *MEM_REF = *HREG1; return (PC);
/*89*/	case MOVwfrm:
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		*(WORD *)MEM_REF = *XREG1;
		return (PC); 
	    } else {
		BYTE *mem_ref;
		WORD temp = *XREG1;
		mem_ref = MEM_REF;
                *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
		return (PC); 
	    }
/*8a*/	case MOVbtrm:
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    *HREG1 = *MEM_REF; return (PC);
/*8b*/	case MOVwtrm: {
	    BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    *XREG1 = FETCH_WORD(mem_ref);
	    } return (PC);
/*8c*/	case MOVsrtrm: {
	    BYTE seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
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
		goto illegal_op; 
			/* trap this */
		    default: goto not_implemented;
		}
	    } else {
		int temp;
		BYTE *mem_ref = MEM_REF;
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
		goto illegal_op; 
			/* trap this */
		    default: goto not_implemented;
		}
	    }}
/*8d*/	case LEA:
	    OVERRIDE = 0;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		goto illegal_op;
	    } else {
		*XREG1 = ((long)MEM_REF) & 0xffff;
	    }
	    return (PC);
/*8e*/	case MOVsrfrm: {
	    WORD temp;
	    BYTE seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		temp = *(WORD *)MEM_REF;
	    } else {
		BYTE *mem_ref = MEM_REF;
		temp = FETCH_WORD(mem_ref);
	    }
	    switch (seg_reg) {
		case 0: /* ES */
		    SHORT_ES_16 = temp;
		    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 1: /* CS */
		    SHORT_CS_16 = temp;
		    if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 2: /* SS */
		    SHORT_SS_16 = temp;
		    if ((*err = SET_SEGREG(LONG_SS,BIG_SS,MK_SS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 3: /* DS */
		    SHORT_DS_16 = temp;
		    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 4: /* FS */
		    SHORT_FS_16 = temp;
		    if ((*err = SET_SEGREG(LONG_FS,BIG_FS,MK_FS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 5: /* GS */
		    SHORT_GS_16 = temp;
		    if ((*err = SET_SEGREG(LONG_GS,BIG_GS,MK_GS,temp))) {
		    	env->error_addr=temp; return P0; }
		    return (PC);
		case 6: /* Illegal */
		case 7: /* Illegal */
		goto illegal_op; 
		    /* trap this */
		default: goto not_implemented;
	    }}
/*8f*/	case POPrm: {
	    BYTE *mem_ref;
	    WORD temp;
	    POPWORD(temp);
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    PUT_WORD(mem_ref, temp);
	    } return (PC);

/*90*/	case NOP:
	    PC += 1; return (PC);
/*91*/	case XCHGcx: {
	    WORD temp = AX;
	    AX = CX;
	    CX = temp;
	    } PC += 1; return (PC);
/*92*/	case XCHGdx: {
	    WORD temp = AX;
	    AX = DX;
	    DX = temp;
	    } PC += 1; return (PC);
/*93*/	case XCHGbx: {
	    WORD temp = AX;
	    AX = BX;
	    BX = temp;
	    } PC += 1; return (PC);
/*94*/	case XCHGsp: {
	    WORD temp = AX;
	    AX = SP;
	    SP = temp;
	    } PC += 1; return (PC);
/*95*/	case XCHGbp: {
	    WORD temp = AX;
	    AX = BP;
	    BP = temp;
	    } PC += 1; return (PC);
/*96*/	case XCHGsi: {
	    WORD temp = AX;
	    AX = SI;
	    SI = temp;
	    } PC += 1; return (PC);
/*97*/	case XCHGdi: {
	    WORD temp = AX;
	    AX = DI;
	    DI = temp;
	    } PC += 1; return (PC);
/*98*/	case CBW:
	    AH = (AL & 0x80) ? 0xff : 0;
	    PC += 1; return (PC);
/*99*/	case CWD:
	    DX = (AX & 0x8000) ? 0xffff : 0;
	    PC += 1; return (PC);
/*9a*/	case CALLl: goto not_implemented;
/*9b*/	case WAIT:
	    PC += 1; return (PC);
/*9c*/	case PUSHF: {
	    DWORD temp;
	    if (vm86f) goto not_permitted;
	    temp = trans_interp_flags(env, interp_var);    
	    PUSHWORD(temp);
	    } PC += 1; return (PC);
/*9d*/	case POPF: {
	    DWORD temp;
	    if (vm86f) goto not_permitted;
	    POPWORD(temp);
	    trans_flags_to_interp(env, interp_var, temp);
	    } PC += 1; return (PC);
/*9e*/	case SAHF: return (PC);
/*9f*/	case LAHF: return (PC);

/*a0*/	case MOVmal: {
	    BYTE *mem_ref;;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    AL = *mem_ref;
	    } PC += 5; return (PC);
/*a1*/	case MOVmax: {
	    BYTE *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    AX = FETCH_WORD(mem_ref);
	    } PC += 5; return (PC);
/*a2*/	case MOValm: {
	    BYTE *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    *mem_ref = AL;
	    } PC += 5; return (PC);
/*a3*/	case MOVaxm: {
	    BYTE *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
            *mem_ref = AL;
            *(mem_ref + 1) = AH;
	    } PC += 5; return (PC);
/*a4*/	case MOVSb: {
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    ValidateAddr(dest, SHORT_ES_16);
	    *dest = *src;
	    (env->flags & DIRECTION_FLAG)?(ESI--,EDI--):(ESI++,EDI++);
	    } PC += 1; return (PC);
/*a5*/	case MOVSw: {
	    int temp;
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    ValidateAddr(dest, SHORT_ES_16);
	    temp = FETCH_WORD(src);
	    PUT_WORD(dest, temp);
	    (env->flags & DIRECTION_FLAG)?(ESI-=2,EDI-=2):(ESI+=2,EDI+=2);
	    } PC += 1; return (PC);
/*a6*/	case CMPSb: {
	    DWORD res, src1, src2;
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    ValidateAddr(dest, SHORT_ES_16);
	    src1 = *src;
	    src2 = *dest;
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(ESI--,EDI--):(ESI++,EDI++);
	    SETBFLAGS(1);
	    } PC += 1; return (PC);
/*a7*/	case CMPSw: {
	    DWORD res, src1, src2;
	    BYTE *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    ValidateAddr(dest, SHORT_ES_16);
	    src1 = FETCH_WORD(src);
	    src2 = FETCH_WORD(dest);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(ESI-=2,EDI-=2):(ESI+=2,EDI+=2);
	    SETWFLAGS(1);
	    } PC += 1; return (PC);
/*a8*/	case TESTbi: return (PC);
/*a9*/	case TESTwi: {
	    DWORD res;
	    res = FETCH_WORD((PC+1));
	    SRC1_32 = SRC2_32 = RES_32 = AX & res;
	    } PC += 3; return (PC);
/*aa*/	case STOSb:
	    LONG_ES[EDI] = AL;
	    (env->flags & DIRECTION_FLAG)?EDI--:EDI++;
	    PC += 1; return (PC);
/*ab*/	case STOSw:
	    LONG_ES[EDI] = AL;
	    LONG_ES[EDI+1] = AH;
	    (env->flags & DIRECTION_FLAG)?(EDI-=2):(EDI+=2);
	    PC += 1; return (PC);
/*ac*/	case LODSb: {
	    BYTE *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    AL = *seg;
	    (env->flags & DIRECTION_FLAG)?ESI--:ESI++;
	    } PC += 1; return (PC);
/*ad*/	case LODSw: {
	    BYTE *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    AX = FETCH_WORD(seg);
	    (env->flags & DIRECTION_FLAG)?(ESI-=2):(ESI+=2);
	    } PC += 1; return (PC);
/*ae*/	case SCASb: {
	    DWORD res, src1, src2;
	    src1 = AL;
	    src2 = (LONG_ES[EDI]);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?EDI--:EDI++;
	    SETBFLAGS(1);
	    } PC += 1; return (PC);
/*af*/	case SCASw: {
	    DWORD res, src1, src2;
	    BYTE *mem_ref;
	    src1 = AX;
	    mem_ref = LONG_ES + (EDI);
	    src2 = FETCH_WORD(mem_ref);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(EDI-=2):(EDI+=2);
	    SETWFLAGS(1);
	    } PC += 1; return (PC);

/*b0*/	case MOVial: return (PC);
/*b1*/	case MOVicl: return (PC);
/*b2*/	case MOVidl: return (PC);
/*b3*/	case MOVibl: return (PC);
/*b4*/	case MOViah: return (PC);
/*b5*/	case MOVich: return (PC);
/*b6*/	case MOVidh: return (PC);
/*b7*/	case MOVibh: return (PC);
/*b8*/	case MOViax:
	    AX = FETCH_WORD((PC+1));
	    PC += 3; return (PC);
/*b9*/	case MOVicx:
	    CX = FETCH_WORD((PC+1));
	    PC += 3; return (PC);
/*ba*/	case MOVidx:
	    DX = FETCH_WORD((PC+1));
	    PC += 3; return (PC);
/*bb*/	case MOVibx:
	    BX = FETCH_WORD((PC+1));
	    PC += 3; return (PC);
/*bc*/	case MOVisp:
	    SP = FETCH_WORD((PC+1)); 
	    PC += 3; return (PC);
/*bd*/	case MOVibp:
	    BP = FETCH_WORD((PC+1)); 
	    PC += 3; return (PC);
/*be*/	case MOVisi:
	    SI = FETCH_WORD((PC+1));
	    PC += 3; return (PC);
/*bf*/	case MOVidi:
	    DI = FETCH_WORD((PC+1));
	    PC += 3; return (PC);

/*d0*/	case SHIFTb:
/*d2*/	case SHIFTbv:
/*c0*/	case SHIFTbi: {
	    int temp, count;
	    DWORD rbef, raft; BYTE *mem_ref, opc;
	    opc = *PC;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    if (opc==SHIFTb) count = 1;
	      else if (opc==SHIFTbv) count = CX & 0x1f;
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
	    int temp, count;
	    DWORD rbef, raft; BYTE *mem_ref, opc;
	    opc = *PC;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (opc==SHIFTw) count = 1;
	      else if (opc==SHIFTwv) count = CX & 0x1f;
	        else { count = *PC & 0x1f; PC += 1; }
	    /* are we sure that for count==0 CF is not changed? */
	    if (count) {
	      if (IS_MODE_REG) {
		WORD *reg = (WORD *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			rbef = *reg;
			count &= 0xf;	/* rotation modulo 16 */
			raft = (rbef << count) | (rbef >> (16 - count));
			*reg = raft;
			CARRY = raft & 1;	/* -> BYTE_FLAG=0 */
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16; /* will produce OF */
			return (PC);
		    case 1: /* ROR */
			rbef = *reg; 
			count &= 0xf;
			raft = (rbef >> count) | (rbef << (16 - count));
			*reg = raft;
			CARRY = (raft >> 15) & 1;
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 2: /* RCL */
			rbef = *reg; 
			count &= 0xf;
			raft = (rbef << count) | ((rbef >> (17 - count))
			    | ((CARRY & 1) << (count - 1)));
			*reg = raft;
			CARRY = (rbef >> (16 - count)) & 1;
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 3: /* RCR */
			rbef = *reg;
			count &= 0xf;
			raft = (rbef >> count) | ((rbef << (17 - count))
			    | ((CARRY & 1) << (16 - count)));
			*reg = raft;
			CARRY = (rbef >> (count - 1)) & 1;
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 4: /* SHL/SAL */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (16 - count)) & 1;
			temp = (temp << count);
			*reg = temp;
			RES_16 = temp;
			return (PC);
		    case 5: /* SHR */
			temp = *reg; 
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*reg = temp;
			RES_16 = temp;
			return (PC);
		    case 6: /* Illegal */
			goto illegal_op;
		    case 7: /* SAR */
			temp = *(signed short *)reg; 
			temp = ((temp << 16) >> 16);
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*reg = temp;
			RES_16 = temp;
			return (PC);
		    default: goto not_implemented;
		}
	      } else {
		BYTE *mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			rbef = FETCH_WORD(mem_ref);
			count &= 0xf;	/* rotation modulo 16 */
			raft = (rbef << count) | (rbef >> (16 - count));
			PUT_WORD(mem_ref,raft);
			CARRY = raft & 1;	/* -> BYTE_FLAG=0 */
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16; /* will produce OF */
			return (PC);
		    case 1: /* ROR */
			rbef = FETCH_WORD(mem_ref);
			count &= 0xf;
			raft = (rbef >> count) | (rbef << (16 - count));
			PUT_WORD(mem_ref,raft);
			CARRY = (raft >> 15) & 1;
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 2: /* RCL */
			rbef = FETCH_WORD(mem_ref);
			count &= 0xf;
			raft = (rbef << count) | ((rbef >> (17 - count))
			    | ((CARRY & 1) << (count - 1)));
			PUT_WORD(mem_ref,raft);
			CARRY = (rbef >> (16 - count)) & 1;
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 3: /* RCR */
			rbef = FETCH_WORD(mem_ref);
			count &= 0xf;
			raft = (rbef >> count) | ((rbef << (17 - count))
			    | ((CARRY & 1) << (16 - count)));
			PUT_WORD(mem_ref,raft);
			CARRY = (rbef >> (count - 1)) & 1;
			if ((count==1) && ((rbef^raft)&0x8000))
			    SRC1_16 = SRC2_16 = ~RES_16;
			return (PC);
		    case 4: /* SHL/SAL */
			temp = FETCH_WORD(mem_ref);
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (16 - count)) & 1;
			temp = (temp << count);
			PUT_WORD(mem_ref,temp);
			RES_16 = temp;
			return (PC);
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			PUT_WORD(mem_ref,temp);
			RES_16 = temp;
			return (PC);
		    case 6: /* Illegal */
			goto illegal_op;
		    case 7: /* SAR */
			temp = FETCH_WORD(mem_ref); 
			temp = ((temp << 16) >> 16);
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			PUT_WORD(mem_ref,temp);
			RES_16 = temp;
			return (PC);
		    default: goto not_implemented;
	      } } } else  return (PC);
	    }
/*c2*/	case RETisp:
	    goto not_implemented;
/*c3*/	case RET:
	    goto not_implemented;
/*c4*/	case LES: {
	    int temp; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_WORD(mem_ref);
	    *XREG1 = temp;
	    temp = FETCH_WORD(mem_ref+2);
	    if ((*err = SET_SEGREG(LONG_ES,BIG_ES,MK_ES,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_ES_32 = temp;
	    } return (PC);
/*c5*/	case LDS: {
	    int temp; BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_WORD(mem_ref);
	    *XREG1 = temp;
	    temp = FETCH_WORD(mem_ref+2);
	    if ((*err = SET_SEGREG(LONG_DS,BIG_DS,MK_DS,temp))) {
	    	env->error_addr=temp; return P0; }
	    SHORT_DS_32 = temp;
	    } return (PC);
/*c6*/	case MOVbirm:
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    *MEM_REF = *PC; PC += 1; return (PC);
/*c7*/	case MOVwirm: {
	    /*int temp;*/ BYTE *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		*(WORD *)mem_ref = FETCH_WORD(PC);
		PC += 2; return (PC);
	    } else {
		*mem_ref = *PC; *(mem_ref+1)= *(PC+1);
		PC += 2; return (PC);
	    } } 
/*cb*/	case RETl: /* restartable */
	    if (vm86f) goto not_permitted;
	    else {
		DWORD cs, ip;
		WORD transfer_magic;
		POPWORD(ip);
		cs = TOS_WORD;
		transfer_magic = (WORD)GetSelectorType(cs);
		if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		    PUSHWORD(ip); env->error_addr=cs; return P0; }
		POPWORD(cs);
		SHORT_CS_16 = cs;
		PC = ip + LONG_CS;
		if (transfer_magic == TRANSFER_CODE32) {
		    return (PC);
		} else
		if (transfer_magic == TRANSFER_CODE16) {
		    *err = EXCP_GOBACK;
		    return (PC);
		} else
		    invoke_data(env);    /* TRANSFER_DATA or garbage */
	    } 
/*c8*/	case ENTER:
/*c9*/	case LEAVE:
/*ca*/	case RETlisp:
/*cc*/	case INT3:
/*cd*/	case INT:
/*ce*/	case INTO:
	    goto not_implemented;
/*cf*/	case IRET: /* restartable */
	    if (vm86f) goto not_permitted;
	    else {
		DWORD cs, ip, flags;
		WORD transfer_magic;
		POPWORD(ip);
		cs = TOS_WORD;
		transfer_magic = (WORD)GetSelectorType(cs);
		if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,cs))==EXCP0B_NOSEG) {
		    PUSHWORD(ip); env->error_addr=cs; return P0; }
		POPWORD(cs);
		POPWORD(flags);
		trans_flags_to_interp(env, interp_var, flags);
		SHORT_CS_16 = cs;
		PC = ip + LONG_CS;
		if (transfer_magic == TRANSFER_CODE32) {
		    return (PC);
		} else
		if (transfer_magic == TRANSFER_CODE16) {
		    *err = EXCP_GOBACK;
		    return (PC);
		} else
		    invoke_data(env);    /* TRANSFER_DATA or garbage */
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
	    BYTE *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + (EBX) + (AL);
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
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg0[funct](reg);
	    else hsw_fp87_mem0[funct](MEM_REF);
	    } break;
/*d9*/	case ESC1: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg1[funct](reg);
	    else hsw_fp87_mem1[funct](MEM_REF);
	    } break;
/*da*/	case ESC2: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg2[funct](reg);
	    else hsw_fp87_mem2[funct](MEM_REF);
	    } break;
/*db*/	case ESC3: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg3[funct](reg);
	    else hsw_fp87_mem3[funct](MEM_REF);
	    } break;
/*dc*/	case ESC4: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg4[funct](reg);
	    else hsw_fp87_mem4[funct](MEM_REF);
	    } break;
/*dd*/	case ESC5: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg5[funct](reg);
	    else hsw_fp87_mem5[funct](MEM_REF);
	    } break;
/*de*/	case ESC6: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg6[funct](reg);
	    else hsw_fp87_mem6[funct](MEM_REF);
	    } break;
/*df*/	case ESC7: {
	    int reg = (*(PC+1) & 7);
	    DWORD funct = (DWORD)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
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
	    if ((--ECX != 0) && (!IS_ZF_SET)) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e1*/	case LOOPZ_LOOPE: 
	    if ((--ECX != 0) && (IS_ZF_SET)) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e2*/	case LOOP: 
	    if (--ECX != 0) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e3*/	case JCXZ: 
	    if (ECX == 0) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
/*e5*/	case INw: {
	      BYTE a = *(PC+1);
	      if (vm86f || (CPL > IOPL)) {
		if ((a&1)||(!test_bit(a+1, io_bitmap))) goto not_permitted;
	      }
	      AX = port_real_inw(a);
	      PC += 2; return (PC);
	    }
/*e7*/	case OUTw: {
	      BYTE a = *(PC+1);
	      if (vm86f || (CPL > IOPL)) {
		if ((a&1)||(!test_bit(a+1, io_bitmap))) goto not_permitted;
	      }
	      port_real_outw(a, AX);
	      PC += 2; return (PC);
	    }
/*ed*/	case INvw: {
	      if (vm86f || (CPL > IOPL)) {
		if ((DX>0x3fe)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
	      }
	      AX = port_real_inw(DX);
	      PC += 1; return (PC);
	    }
/*ef*/	case OUTvw: {
	      if (vm86f || (CPL > IOPL)) {
#ifdef X_SUPPORT
		if (emu_under_X) {
		  extern void VGA_emulate_outb(WORD,BYTE);
		  switch(DX) {
		    case 0x3d4:	/*CRTC_INDEX*/
		    case 0x3c4:	/*SEQUENCER_INDEX*/
		    case 0x3c6:	/*DAC_PEL_MASK*/
		    case 0x3c8:	/*DAC_WRITE_INDEX*/
			VGA_emulate_outb(DX,AL);
			VGA_emulate_outb(DX+1,AH);
			PC += 1; return (PC);
		    default: break;
		  }
		}
#endif
		if ((DX>0x3fe)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
	      }
	      port_real_outw(DX, AX);
	      PC += 1; return (PC);
	    }

/*e8*/	case CALLd:
/*e9*/	case JMPd:
/*ea*/	case JMPld:
/*eb*/	case JMPsid: 
	    goto not_implemented;

/*f0*/	case LOCK:
	    PC += 1; return (PC);
	case 0xf1:    /* illegal on 8086 and 80x86 */
	    goto illegal_op;

/*f2*/	case REPNE:
/*f3*/	case REP:     /* also is REPE */
	{
	    unsigned int count = ECX;
	    int longd = 2;
	    BYTE repop,test;

	    repop = (*PC-REPNE);
	    PC += 2;
segrep:
	    switch (*(PC-1)) {
		case INSb:
		case INSw:
		case OUTSb:
		case OUTSw:
		    if (vm86f) goto not_permitted;
		    goto not_implemented;
		case MOVSb: {
		    BYTE *src, *dest;
		    if (count == 0) return (PC);
		    instr_count += count;
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; ECX = 0;
			while (count--) *dest-- = *src--;
			return (PC);
		    } else {
			EDI += count; ESI += count; ECX = 0;
			while (count--) *dest++ = *src++;
			return (PC);
		    } } 
		case MOVSw: {
		    int lcount = count * longd;
		    if (count == 0) return (PC);
		    instr_count += count;
		    if (env->flags & DIRECTION_FLAG) lcount = -lcount;
		    if (longd==2) {
		      WORD *src, *dest;
		      src = (WORD *)(ALLOW_OVERRIDE(LONG_DS)+(ESI));
		      dest = (WORD *)(LONG_ES + EDI);
		      ValidateAddr((void *)dest, SHORT_ES_16);
		      if (lcount < 0) {
			while (count--) *dest-- = *src--;
		      } else {
			while (count--) *dest++ = *src++;
		      }
		    } else {
		      DWORD *src, *dest;
		      src = (DWORD *)(ALLOW_OVERRIDE(LONG_DS)+(ESI));
		      dest = (DWORD *)(LONG_ES + EDI);
		      ValidateAddr((void *)dest, SHORT_ES_16);
		      if (lcount < 0) {
			while (count--) *dest-- = *src--;
		      } else {
			while (count--) *dest++ = *src++;
		      }
		    } 
		    EDI += lcount; ESI += lcount; ECX = 0;
		    return (PC);
		    }
		case CMPSb: {
		    BYTE *src, *dest, *ovr;
		    DWORD res, src1=0, src2=0, oldcnt;
		    if (count == 0) return (PC);
		    oldcnt = count;
		    src = (ovr=ALLOW_OVERRIDE(LONG_DS)) + (ESI);
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = *src--;
			    src2 = *dest--;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
				BYTE_FLAG = BYTE_OP;
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = *src++;
			    src2 = *dest++;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			        res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0; EDI = dest - LONG_ES;
		    ESI = src - ovr;
		    RES_32 = res << 8; SRC1_8 = src1; 
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
		    } return (PC);
		case CMPSw: {
		    BYTE *src, *dest, *ovr;
		    DWORD res, src1=0, oldcnt;
		    int src2=0;
		    if (count == 0) return (PC);
		    oldcnt = count;
		    src = (ovr=ALLOW_OVERRIDE(LONG_DS)) + (ESI);
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
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
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				if (longd==2) {
				  SRC1_16 = src1; RES_32 = res;
				  SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
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
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ovr;
				if (longd==2) {
				  SRC1_16 = src1; RES_32 = res;
				  SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
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
		    ECX = 0; EDI = dest - LONG_ES;
		    ESI = src - ovr;
		    if (longd==2) {
		      RES_32 = res; SRC1_16 = src1;
		      SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    }
		    else {
		      SETDFLAGS(1,1);
		    }
		    } return (PC);
		case STOSb: {
		    BYTE al;
		    BYTE *dest;
		    if (count == 0) return (PC);
		    instr_count += count;
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
		    al = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count; ECX = 0;
			while (count--) *dest-- = al;
			return (PC);
		    } else {		      /* forwards */
			EDI += count; ECX = 0;
			while (count--) *dest++ = al;
			return (PC);
		    } } 
		case STOSw: {
		    BYTE *dest;
		    if (count == 0) return (PC);
		    instr_count += count;
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count * longd; ECX = 0;
			while (count--) {
			    if (longd==4) {PUT_QUAD(dest, EAX);}
			      else {PUT_WORD(dest, AX);}
			    dest -= longd;
			} return (PC);
		    } else {		      /* forwards */
			EDI += count * longd; ECX = 0;
			while (count--) {
			    if (longd==4) {PUT_QUAD(dest, EAX);}
			      else {PUT_WORD(dest, AX);}
			    dest += longd;
			} return (PC);
		    } }
		case LODSb: {
		    BYTE *src;
		    if (count == 0) return (PC);
		    instr_count += count;
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			AL = *(src - count); ESI -= count;
			ECX = 0; return (PC);
		    } else {		      /* forwards */
			AL = *(src + count); ESI += count;
			ECX = 0; return (PC);
		    } } 
		case LODSw: {
		    BYTE *src;
		    if (count == 0) return (PC);
		    instr_count += count;
		    count = count * longd;
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; ESI -= count;
			if (longd==4) EAX = FETCH_QUAD(src);
			  else AX = FETCH_WORD(src);
			ECX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; ESI += count;
			if (longd==4) EAX = FETCH_QUAD(src);
			  else AX = FETCH_WORD(src);
			ECX = 0; return (PC);
		    } }
		case SCASb: {
		    DWORD res, src1;
		    int src2, oldcnt;
		    BYTE *dest;
		    if (count == 0) return (PC);
		    oldcnt = count;
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
		    src1 = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = *dest;
			    dest -=1;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			    	res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				RES_32 = res << 8; SRC1_8 = src1;
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
                         	if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = *dest;
			    dest +=1;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			    	res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				RES_32 = res << 8; SRC1_8 = src1;
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
				BYTE_FLAG = BYTE_OP;
                         	if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0; EDI = dest - LONG_ES;
		    RES_32 = res << 8; SRC1_8 = src1;
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } return (PC);
		case SCASw: {
		    BYTE *dest;
		    DWORD res, src1, oldcnt;
		    int src2;
		    if (count == 0) return (PC);
		    oldcnt = count;
		    dest = LONG_ES + EDI;
		    ValidateAddr(dest, SHORT_ES_16);
		    src1 = AX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = (longd==4? FETCH_QUAD(dest) : FETCH_WORD(dest));
			    dest -= longd;
			    test = (src1 != src2) ^ repop;
			    if (test) count--;
			    else {
			    	res = src1 - src2;
				instr_count += (oldcnt-count);
				ECX = count - 1;
				EDI = dest - LONG_ES;
				if (longd==2) {
				  RES_32 = res; SRC1_16 = src1;
				  SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				}
				else {
				  SETDFLAGS(1,1);
				}
                         	if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
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
				ECX = count - 1;
				EDI = dest - LONG_ES;
				if (longd==2) {
				  RES_32 = res; SRC1_16 = src1;
				  SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				}
				else {
				  SETDFLAGS(1,1);
				}
                         	if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    instr_count += count;
		    ECX = 0; EDI = dest - LONG_ES;
		    if (longd==2) {
		      RES_32 = res; SRC1_16 = src1;
		      SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    }
		    else {
		      SETDFLAGS(1,1);
		    }
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
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
		case OPERoverride:	/* 0x66 if we come from 16:16 */
            	    longd = 4;
		case ADDRoverride:	/* ignore 0x67 */
            	    PC+=1; goto segrep;
/*----------IA--------------------------------*/
		default: PC--; goto not_implemented;
/*		default: PC--; return (PC); */
	    } }
/*f4*/	case HLT:
	    goto not_implemented;
/*f5*/	case CMC: return (PC);

/*f6*/	case GRP1brm: {
	    int src, src1, src2, res; BYTE *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    mem_ref = MEM_REF; 
	    switch (res) {
		case 0: /* TEST */
		    src1 = *mem_ref;
		    src2 = *PC; PC += 1;
		    RES_32 = (src1 & src2) << 8;
		    SRC1_8 = SRC2_8 = RES_8;
		    BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 1: /* TEST (Is this Illegal?) */
		goto illegal_op;
		case 2: /* NOT */
		    src1 = ~(*mem_ref);
		    *mem_ref = src1;
		    return (PC);
		case 3: /* NEG */
		    src = *mem_ref;
		    src1 = (((src & 0xff) == 0x80)? 0:-src);
		    *mem_ref = src1;
		    RES_32 = src1 << 8;
                    SRC1_16 = 0;
                    SRC2_16 = RES_16;
		    BYTE_FLAG = BYTE_OP;
		    CARRY = (src != 0);
		    return (PC);
		case 4: /* MUL AL */
		    src1 = *mem_ref;
		    src2 = AL;
		    res = src1 * src2;
		    AX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = (res>>8)?-1:0;
		    return (PC);
		case 5: /* IMUL AL */
		    src1 = *(signed char *)mem_ref;
		    src2 = (signed char)AL;
		    res = src1 * src2;
		    AX = res;
		    SRC1_16 = SRC2_16 = 0;
		    res = res >> 8;
		    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
		    return (PC);
		case 6: /* DIV AL */
		    src1 = *mem_ref;
		    if (src1==0) goto div_by_zero;
		    res = AX;
		    AL = res / src1;
		    AH = res % src1;
		    return (PC);
		case 7: /* IDIV AX */
		    src1 = *(signed char *)mem_ref;
		    if (src1==0) goto div_by_zero;
		    res = AX;
		    AL = res / src1;
		    AH = res % src1;
		    return (PC);
		default: goto not_implemented;
	    } }
/*f7*/	case GRP1wrm: {
	    int src, src1, src2, res; BYTE *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_XREG(mem_ref);
		    src2 = FETCH_WORD(PC); PC += 2;
		    SRC1_16 = SRC2_16 = RES_32 = src1 & src2;
		case 1: /* TEST (Is this Illegal?) */
		goto illegal_op; 
		case 2: /* NOT */
		    src1 = FETCH_XREG(mem_ref);
		    src1 = ~(src1);
		    *(WORD *)mem_ref = src1;
		    return (PC);
		case 3: /* NEG */
		    src = FETCH_XREG(mem_ref);
		    src1 = (((src & 0xffff) == 0x8000)? 0:-src);
		    *(WORD *)mem_ref = src1;
		    RES_32 = src1;
		    SRC1_16 = 0;
		    SRC2_16 = src1;
		    CARRY = (src != 0);
		    return (PC);
		case 4: /* MUL AX */
		    src1 = FETCH_XREG(mem_ref);
		    src2 = AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = (res)?-1:0;
		    return (PC);
		case 5: /* IMUL AX */
		    src1 = *(signed short *)mem_ref;
		    src2 = (signed short)AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
		    return (PC);
		case 6: /* DIV DX+AX */
		    src1 = FETCH_XREG(mem_ref);
		    if (src1==0) goto div_by_zero;
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
		case 7: /* IDIV DX+AX */
		    src1 = *(signed short *)mem_ref;
		    if (src1==0) goto div_by_zero;
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
		default: goto not_implemented;
	      }
	    } else {
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_WORD(mem_ref);
		    src2 = FETCH_WORD(PC); PC += 2;
		    RES_32 = src1 & src2;
		    SRC1_16 = SRC2_16 = RES_16;
		    return(PC);
		case 1: /* TEST (Is this Illegal?) */
		goto illegal_op; 
		case 2: /* NOT */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ~(src1);
		    PUT_WORD(mem_ref, src1);
		    return (PC);
		case 3: /* NEG */
		    src = FETCH_WORD(mem_ref);
		    src1 = (((src & 0xffff) == 0x8000)? 0:-src);
		    PUT_WORD(mem_ref, src1);
		    RES_32 = src1;
		    SRC1_16 = 0;
		    SRC2_16 = src1;
		    CARRY = (src != 0);
		    return (PC);
		case 4: /* MUL AX */
		    src1 = FETCH_WORD(mem_ref);
		    src2 = AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = (res)?-1:0;
		    return (PC);
		case 5: /* IMUL AX */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ((src1<<16)>>16);
		    src2 = (signed short)AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
		    return (PC);
		case 6: /* DIV DX+AX */
		    src1 = FETCH_WORD(mem_ref);
		    if (src1==0) goto div_by_zero;
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
		case 7: /* IDIV DX+AX */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ((src1<<16)>>16);
		    if (src1==0) goto div_by_zero;
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
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
	    int temp; BYTE *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    switch (temp) {
		case 0: /* INC */
		    mem_ref = MEM_REF;
		    SRC1_8 = temp = *mem_ref;
		    *mem_ref = temp = temp + 1;
		    SRC2_8 = 1;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 1: /* DEC */
		    mem_ref = MEM_REF;
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
		default: goto not_implemented;
	    }}
/*ff*/	case GRP2wrm: {
	    int temp; BYTE *mem_ref;
	    DWORD jcs, jip, ocs=0, oip=0;
	    WORD transfer_magic;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (temp) {
		case 0: /* INC */
		    temp = FETCH_XREG(mem_ref);
		    SRC1_16 = temp; SRC2_16 = 1;
		    *(WORD *)mem_ref = RES_16 = temp + 1;
		    return (PC);
		case 1: /* DEC */
		    temp = FETCH_XREG(mem_ref);
		    SRC1_16 = temp; SRC2_16 = -1;
		    *(WORD *)mem_ref = RES_16 = temp - 1;
		    return (PC);
		case 2: /* CALL near indirect */
		    temp = PC - LONG_CS;
		    PUSHWORD(temp);
		    PC = LONG_CS + FETCH_XREG(mem_ref);
		    return (PC);
		case 4: /* JMP near indirect */
		    PC = LONG_CS + FETCH_XREG(mem_ref);
		    return (PC);
		case 3: /* CALL long indirect */
		    goto jff03m;
		case 5: /* JMP long indirect */
		    goto jff05m;
		case 6: /* PUSH */
		    temp = FETCH_XREG(mem_ref);
		    PUSHWORD(temp);
		    return (PC);
		case 7: /* Illegal */
		goto illegal_op; 
		default: goto not_implemented;
	      }
	    } else {
	      switch (temp) {
		case 0: /* INC */
		    temp = FETCH_WORD(mem_ref);
		    SRC1_16 = temp; SRC2_16 = 1;
		    RES_16 = temp = temp + 1;
		    PUT_WORD(mem_ref, temp);
		    return (PC);
		case 1: /* DEC */
		    temp = FETCH_WORD(mem_ref);
		    SRC1_16 = temp; SRC2_16 = -1;
		    RES_16 = temp = temp - 1;
		    PUT_WORD(mem_ref, temp);
		    return (PC);
		case 2: /* CALL near indirect */
		    temp = PC - LONG_CS;
		    PUSHWORD(temp);
		    PC = LONG_CS + FETCH_WORD(mem_ref);
		    return (PC);
		case 4: /* JMP near indirect */
		    PC = LONG_CS + FETCH_WORD(mem_ref);
		    return(PC);
		case 3: /* CALL long indirect restartable */
jff03m:		    ocs = SHORT_CS_16;
		    oip = PC - LONG_CS;
		    /* fall through */
		case 5: { /* JMP long indirect restartable */
jff05m:		    jip = FETCH_WORD(mem_ref);
		    jcs = FETCH_WORD(mem_ref+2);
		    if (!vm86f) {
			transfer_magic = (WORD)GetSelectorType(jcs);
		    }
		    if (vm86f || (transfer_magic==TRANSFER_CODE16)) {
			if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,jcs))==EXCP0B_NOSEG) {
		    	    env->error_addr=jcs; return P0; }
			if (temp==3) {
			    PUSHWORD(ocs);
			    PUSHWORD(oip);
			}
			SHORT_CS_16 = jcs;
			PC = jip + LONG_CS;
			if (in_dpmi_emu==32) {
			    *err = EXCP_GOBACK;
			}
			return (PC);
		    }
		    else if (transfer_magic==TRANSFER_CODE32) {
			if ((*err = SET_SEGREG(LONG_CS,BIG_CS,MK_CS,jcs))==EXCP0B_NOSEG) {
		    	    env->error_addr=jcs; return P0; }
			if (temp==3) {
			    PUSHWORD(ocs);
			    PUSHWORD(oip);
			}
			SHORT_CS_16 = jcs;
			PC = jip + LONG_CS;
			if (in_dpmi_emu==16) {
			    *err = EXCP_GOBACK;
			}
			return (PC);
		    }
		    else
			invoke_data(env);   
		}
		case 6: /* PUSH */
		    temp = FETCH_WORD(mem_ref);
		    PUSHWORD(temp);
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
	error(" 16/32 nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*P0,*(P0+1),*(P0+2),
		SHORT_CS_16,P0-LONG_CS,(int)P0);
#ifdef DEBUG
	e_debug(env, P0, P0, interp_var, 1);
#endif
	FatalAppExit(0, "INSTR");
	exit(1);

not_permitted:
	*err = EXCP0D_GPF; return P0;

div_by_zero:
	*err = EXCP00_DIVZ; return P0;

illegal_op:
	error(" 16/32 illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*P0,*(P0+1),*(P0+2), 
                SHORT_CS_16,P0-LONG_CS,(int)P0);
	*err = EXCP06_ILLOP; return (P0);
}
