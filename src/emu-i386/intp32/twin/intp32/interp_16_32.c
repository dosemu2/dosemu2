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
 */

#include <stdio.h>
#include "Log.h"
#include "hsw_interp.h"
#include "mod_rm.h"

#ifdef DEBUG
#include <string.h>
extern char * decode(int opcode, int modrm);
extern char *getenv();
extern int print;
extern int small_print;
extern int stack_print;
extern int segment_print;
extern int ad32_print;
extern long start_count;
extern long end_count;
extern int granularity;
#ifdef DOSEMU
extern long instr_count;
#else
extern int instr_count;
#endif
extern int dbx_cs;
extern int dbx_ip;
extern int dbx_stop_count;
extern void dbx_stop();
#endif

extern unsigned char parity[];
extern char unknown_msg[], illegal_msg[], unsupp_msg[];
extern ENV87 hsw_env87;
extern FUNCT_PTR hsw_fp87_mem0[], hsw_fp87_mem1[], 
    hsw_fp87_mem2[], hsw_fp87_mem3[], hsw_fp87_mem4[],
    hsw_fp87_mem5[], hsw_fp87_mem6[], hsw_fp87_mem7[]; 
extern FUNCT_PTR hsw_fp87_reg0[], hsw_fp87_reg1[], 
    hsw_fp87_reg2[], hsw_fp87_reg3[], hsw_fp87_reg4[],
    hsw_fp87_reg5[], hsw_fp87_reg6[], hsw_fp87_reg7[]; 

extern unsigned int
  trans_interp_flags (Interp_ENV *env, Interp_VAR * interp_var);
extern void
  trans_flags_to_interp (Interp_ENV *env, Interp_VAR *interp_var, unsigned int flags);


unsigned char *
hsw_interp_16_32 (Interp_ENV *env, unsigned char *P0, unsigned char *PC,
	Interp_VAR *interp_var, int *err)
{
#ifdef DEBUG
#ifdef DOSEMU
    e_debug(env, P0, PC, interp_var);
#else
    if(ad32_print) { printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS),*PC, *(PC+1), AX, BX, CX, DX, SI, DI, BP, SP, decode(*PC, *(PC+1)), instr_count);
	fflush(stdout);
	}
#endif
#endif
    *err = 0;
    override:
    switch (*PC) {
	case ADDbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } return (PC); 
	case ADDwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } return (PC); 
	case ADDbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } return (PC); 
	case ADDwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 + src2;
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } return (PC); 
	case ORbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG = BYTE_OP;
	    } return (PC); 
	case ORwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case ORbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    } return (PC); 
	case ORwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 | src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case TwoByteESC: {
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } return (PC);
			case 1: /* STR */ {
			    /* Store Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } return (PC);
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
			    PC = PC +1 + hsw_modrm_32_word(env,PC + 1,interp_var);
			    return (PC);
			case 3: /* LTR */ {
			    /* Load Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_XREG(mem_ref);
			    else temp = FETCH_WORD(mem_ref);
		goto not_implemented;
			    /* hsw_task_register = temp; */
			    } 
			case 4: /* VERR */ {
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_XREG(mem_ref);
			    else temp = FETCH_WORD(mem_ref);
		goto not_implemented;
			    /* if (hsw_verr(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } 
			case 5: /* VERW */ {
			    int temp; unsigned char *mem_ref;
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
		    }
		case 0x01: /* GRP7 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_WORD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 1: /* SIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_WORD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
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
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LIMIT field */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
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
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
                    int temp; unsigned char *mem_ref;
                        WORD _sel;
                    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
                    mem_ref = MEM_REF;
                    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
                if((_sel &0x7) != 0x7)
                        RES_16 = 0;
                else {
		temp = (GetSelectorFlags(_sel) << 8) & 0xffff;
                        RES_16 = 1;
                        PUT_XREG(mem_ref,temp);
                }
		    /* what do I do here??? */
		    } return(PC); 
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
                    int temp; unsigned char *mem_ref;
                        WORD _sel;
                    PC += 1; PC += hsw_modrm_32_word(env,PC,interp_var);
                    mem_ref = MEM_REF;
                    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
                if((_sel & 0x7) != 0x7)
                        RES_16 = 0;
                else {
                temp= (WORD)GetSelectorLimit(_sel);
                        PUT_XREG(mem_ref,temp);
                        RES_16 = 1;
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
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x91: /* SETNObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x92: /* SETBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x93: /* SETNBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x94: /* SETZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x95: /* SETNZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x96: /* SETBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x97: /* SETNBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x98: /* SETSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET) ? 1 : 0;
		    } return (PC);
		case 0x99: /* SETNSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9a: /* SETPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9b: /* SETNPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9c: /* SETLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9d: /* SETNLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9e: /* SETLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9f: /* SETNLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0xa0: /* PUSHfs */ {
		    unsigned short temp = SHORT_FS_16;
		    PUSHWORD(temp);
		    } PC += 2; return (PC);
		case 0xa1: /* POPfs */ {
		    unsigned int temp;
		    POPWORD(temp);
		    SET_SEGREG(LONG_FS,temp);
		    SHORT_FS_32 = temp;
		    } PC += 2; return (PC);
                case 0xa3: /* BT */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
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
		    /* Double Prescision Shift Left */
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = *PC & 0xf; PC++;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    *(unsigned short *)mem_ref = temp;
			} else {
			    temp = FETCH_WORD(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    PUT_WORD(mem_ref,temp);
			}
			RES_32 = temp;
			SRC1_16 = SRC2_16 = temp >> 1;
		    } return (PC);
		case 0xa5: /* SHLDcl */ {
		    /* Double Prescision Shift Left by CL */
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = CX & 0xf;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    *(unsigned short *)mem_ref = temp;
			} else {
			    temp = FETCH_WORD(mem_ref);
			    temp = temp << count;
			    temp1 = temp1 >> (16 - count);
			    temp |= temp1;
			    PUT_WORD(mem_ref,temp);
			}
			RES_32 = temp;
			SRC1_16 = SRC2_16 = temp >> 1;
		    } return (PC);
		case 0xa6: /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
		case 0xa7: /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		goto not_implemented;
		case 0xa8: /* PUSHgs */ {
		    unsigned short temp = SHORT_GS_16;
		    PUSHWORD(temp);
		    } PC += 2; return (PC);
		case 0xa9: /* POPgs */ {
		    unsigned int temp;
		    POPWORD(temp);
		    SET_SEGREG(LONG_GS,temp);
		    SHORT_GS_32 = temp;
		    } PC += 2; return (PC);
                case 0xab: /* BTS */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
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
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = FETCH_WORD(PC) & 0xf; PC += 2;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (16 - count);
			    temp |= temp1;
			    *(unsigned short *)mem_ref = temp;
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
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = CX & 0xf;
			if (IS_MODE_REG) {
			    temp = FETCH_XREG(mem_ref);
			    CARRY = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (16 - count);
			    temp |= temp1;
			    *(unsigned short *)mem_ref = temp;
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
                        int res, src1, src2; unsigned char *mem_ref;
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
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref);
		    *XREG1 = temp;
		    temp = FETCH_WORD(mem_ref+2);
		    SHORT_SS_32 = temp;
		    SET_SEGREG(LONG_SS,temp);
		    } return (PC);
                case 0xb3: /* BTR */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
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
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref);
		    *XREG1 = temp;
		    temp = FETCH_WORD(mem_ref+2);
		    SHORT_FS_32 = temp;
		    SET_SEGREG(LONG_FS,temp);
		    } return (PC);
		case 0xb5: /* LGS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_WORD(mem_ref);
		    *XREG1 = temp;
		    temp = FETCH_WORD(mem_ref+2);
		    SHORT_GS_32 = temp;
		    SET_SEGREG(LONG_GS,temp);
		    } return (PC);
		case 0xb6: /* MOVZXb */ {
		    unsigned int temp; 
		    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
		    *XREG1 = (WORD)(temp & 0xff);
		    } return (PC);
		case 0xb7: { /* MOVZXw */
             	    unsigned int temp;
                    unsigned char *mem_ref;
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
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = *PC;  PC++;
                            if (IS_MODE_REG)
                                temp1 = *(unsigned short *)mem_ref;
                            else   
                                temp1 = FETCH_WORD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0xf))&1;
                    } return(PC);
                        case 5: /* BTS */ {
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = (*PC) & 0xf;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(unsigned short *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
                                *(unsigned short *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_WORD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 |= (0x1 << temp);
                                PUT_WORD(mem_ref,temp1);
			    }
                    } return(PC);
                        case 6: /* BTR */ {
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = (*PC) & 0xf;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(unsigned short *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
                                *(unsigned short *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_WORD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 &= ~(0x1 << temp);
                                PUT_WORD(mem_ref,temp1);
			    }
                    } return(PC);
                        case 7: /* BTC */ {
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = (*PC) & 0xf;  PC++;
                            if (IS_MODE_REG) {
                                temp1 = *(unsigned short *)mem_ref;
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
                                *(unsigned short *)mem_ref = temp1;
                            } else {
                                temp1 = FETCH_WORD(mem_ref);
                                CARRY = (temp1 >> temp)&1;
                                temp1 ^= (0x1 << temp);
                                PUT_WORD(mem_ref,temp1);
			    }
                    } return(PC);
                    }
                case 0xbb: /* BTC */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
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
		    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = *(unsigned short *)mem_ref;
                    else
                        temp = FETCH_WORD(mem_ref);
		    if(temp) {
                    for(i=0; i<16; i++)
                        if((temp >> i) & 0x1) break;
                    *XREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
                    } return(PC);
                case 0xbd: /* BSR */ {
                    DWORD temp, i;
		    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = *(unsigned short *)mem_ref;
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
		    PC = PC+1+hsw_modrm_32_byte(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
		    temp = ((temp << 24) >> 24);
		    *XREG1 = (WORD)temp;
		    } return (PC);
		case 0xbf: { /* MOVSXw */
		    signed long temp; 
                    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = FETCH_XREG(mem_ref);
                    else
                        temp = FETCH_WORD(mem_ref);
                    *XREG1 = (WORD)temp;
		    } return(PC); 
                case 0xc0: { /* XADDb */
                    int res,src1,src2; unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_byte(env,PC+1,interp_var);
                    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
                    *HREG1 = src1;
                    *mem_ref = res = src1 + src2;
                    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
                    SRC1_8 = src1; SRC2_8 = src2;
                    } return(PC);
                case 0xc1: { /* XADDw */
                    int res,src1,src2; unsigned char *mem_ref;
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
		}
	    }
	case ADCbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } return (PC); 
	case ADCwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } return (PC); 
	case ADCbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } return (PC); 
	case ADCwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 + src2;
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } return (PC); 
	case SBBbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } return (PC); 
	case SBBwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } return (PC); 
	case SBBbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } return (PC); 
	case SBBwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    src2 = src2 + (CARRY & 1);
	    *XREG1 = res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } return (PC); 
	case ANDbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } return (PC); 
	case ANDwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case ANDbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } return (PC); 
	case ANDwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 & src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case SEGes:
	    OVERRIDE = LONG_ES;
	    PC += 1; goto override;
	case SUBbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } return (PC); 
	case SUBwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } return (PC); 
	case SUBbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } return (PC); 
	case SUBwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } return (PC); 
	case SEGcs:
	    OVERRIDE = LONG_CS;
	    PC+=1; goto override;
	case XORbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } return (PC); 
	case XORwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case XORbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } return (PC); 
	case XORwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 ^ src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case SEGss:
	    OVERRIDE = LONG_SS;
	    PC+=1; goto override;
	case CMPbfrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } return (PC); 
	case CMPwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } return (PC); 
	case CMPbtrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *MEM_REF; src1 = *HREG1;
	    res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } return (PC); 
	case CMPwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } return (PC); 
	case SEGds:
	    OVERRIDE = LONG_DS;
	    PC+=1; goto override;

	case SEGfs:
	    OVERRIDE = LONG_FS;
	    PC+=1; goto override;
	case SEGgs:
	    OVERRIDE = LONG_GS;
	    PC+=1; goto override;
	case OPERoverride:	/* 0x66: 32 bit operand, 16 bit addressing */
	    if (!data32)
		return (hsw_interp_32_32 (env, P0, PC+1, interp_var, err));
	    PC += 1;
	    goto override;
	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
	    if (code32)
		return (hsw_interp_16_16 (env, P0, PC+1, interp_var, err));
	    PC += 1;
	    goto override;
	case IMULwrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	case IMULbrm: {
	    int res, src1, src2; unsigned char *mem_ref;
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
	case IMMEDbrm2:    /* out of order */
	case IMMEDbrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *PC; PC += 1;
	    mem_ref = MEM_REF; src1 = *mem_ref;
	    switch (res) {
		case 0: /* ADD */
		    *mem_ref = res = src1 + src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = src2;
		    return (PC);
		case 1: /* OR */
		    *mem_ref = res = src1 | src2;
		    SRC1_8 = res; SRC2_8 = res;
		    RES_32 = res << 8; 
		    BYTE_FLAG=BYTE_OP;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *mem_ref = res = src1 + src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = src2;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *mem_ref = res = src1 - src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    return (PC);
		case 4: /* AND */
		    *mem_ref = res = src1 & src2;
		    SRC1_8 = res; SRC2_8 = res;
		    RES_32 = res << 8; 
		    BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 5: /* SUB */
		    *mem_ref = res = src1 - src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    return (PC);
		case 6: /* XOR */
		    *mem_ref = res = src1 ^ src2;
		    SRC1_8 = res; SRC2_8 = res;
		    RES_32 = res << 8; 
		    BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    return (PC);
	    }}
	case IMMEDwrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = FETCH_WORD(PC); PC += 2;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_XREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    return (PC);
		case 1: /* OR */
		    *(unsigned short *)mem_ref = res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
		case 4: /* AND */
		    *(unsigned short *)mem_ref = res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    return (PC);
		case 5: /* SUB */
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
		case 6: /* XOR */
		    *(unsigned short *)mem_ref = res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_WORD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
	      }
	    }}
	case IMMEDisrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = (signed char)*(PC); PC += 1;
	    src2 = src2 & 0xffff;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_XREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    return (PC);
		case 1: /* OR */
		    *(unsigned short *)mem_ref = res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
		case 4: /* AND */
		    *(unsigned short *)mem_ref = res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    return (PC);
		case 5: /* SUB */
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
		case 6: /* XOR */
		    *(unsigned short *)mem_ref = res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_WORD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
                    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    return (PC);
	      }
	    }}
	case TESTbrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 & src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG = BYTE_OP;
	    } return (PC); 
	case TESTwrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 & src2;
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 & src2;
	    }
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } return (PC); 
	case XCHGbrm: {
	    unsigned char temp;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    temp = *MEM_REF; *MEM_REF = *HREG1; *HREG1 = temp;
	    } return (PC); 
	case XCHGwrm: {
	    unsigned short temp; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		temp = FETCH_XREG(mem_ref);
		*(unsigned short *)mem_ref = *XREG1;
		*XREG1 = temp;
		return (PC); 
	    } else {
		unsigned short temp1 = FETCH_WORD(mem_ref);
		temp = *XREG1; *XREG1 = temp1;
                *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
		return (PC); 
	    }
	    }

	case MOVbfrm:
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    *MEM_REF = *HREG1; return (PC);
	case MOVwfrm:
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		*(unsigned short *)MEM_REF = *XREG1;
		return (PC); 
	    } else {
		unsigned char *mem_ref;
		unsigned short temp = *XREG1;
		mem_ref = MEM_REF;
                *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
		return (PC); 
	    }
	case MOVbtrm:
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    *HREG1 = *MEM_REF; return (PC);
	case MOVwtrm: {
	    unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    *XREG1 = FETCH_WORD(mem_ref);
	    } return (PC);
	case MOVsrtrm: {
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		switch (seg_reg) {
		    case 0: /* ES */
			*(unsigned short *)MEM_REF = SHORT_ES_16;
			return (PC);
		    case 1: /* CS */
			*(unsigned short *)MEM_REF = SHORT_CS_16;
			return (PC);
		    case 2: /* SS */
			*(unsigned short *)MEM_REF = SHORT_SS_16;
			return (PC);
		    case 3: /* DS */
			*(unsigned short *)MEM_REF = SHORT_DS_16;
			return (PC);
		    case 4: /* FS */
			*(unsigned short *)MEM_REF = SHORT_FS_16;
			return (PC);
		    case 5: /* GS */
			*(unsigned short *)MEM_REF = SHORT_GS_16;
			return (PC);
		    case 6: /* Illegal */
		    case 7: /* Illegal */
		goto illegal_op; 
			/* trap this */
		}
	    } else {
		int temp;
		unsigned char *mem_ref = MEM_REF;
		switch (seg_reg) {
		    case 0: /* ES */
			temp = SHORT_ES_16;
                        *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			return (PC);
		    case 1: /* CS */
			temp = SHORT_CS_16;
                        *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			return (PC);
		    case 2: /* SS */
			temp = SHORT_SS_16;
                        *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			return (PC);
		    case 3: /* DS */
			temp = SHORT_DS_16;
                        *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			return (PC);
		    case 4: /* FS */
			temp = SHORT_FS_16;
                        *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			return (PC);
		    case 5: /* GS */
			temp = SHORT_GS_16;
                        *mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			return (PC);
		    case 6: /* Illegal */
		    case 7: /* Illegal */
		goto illegal_op; 
			/* trap this */
		}
	    }}
	case LEA: {
	    int temp=0, modrm;
	    modrm = *(PC+1);
	    switch (modrm >> 6) {
		case 0:
		  switch (modrm & 7) {
		    case 0: temp = EAX; break;
		    case 1: temp = ECX; break;
		    case 2: temp = EDX; break;
		    case 3: temp = EBX; break;
		    case 4: /* SIB form */ {
			int scale, base=0, index=0;
			int sib = *(PC + 2);
			scale = sib >> 6;
			switch ((sib>>3)&7) {
			    case 0: index = EAX; break;
			    case 1: index = ECX; break;
			    case 2: index = EDX; break;
			    case 3: index = EBX; break;
			    case 4: index = 0; break;
			    case 5: index = EBP; break;
			    case 6: index = ESI; break;
			    case 7: index = EDI; break;
			}
			switch (sib&7) {
			    case 0: base = EAX; break;
			    case 1: base = ECX; break;
			    case 2: base = EDX; break;
			    case 3: base = EBX; break;
			    case 4: base = ESP; break;
			    case 5: base = FETCH_QUAD(PC+3);PC+=4; break;
			    case 6: base = ESI; break;
			    case 7: base = EDI; break;
			}
			temp = (index << scale) + base;
			} break;
		    case 5: temp = FETCH_QUAD(PC+2); PC+=4; break;
		    case 6: temp = ESI; break;
		    case 7: temp = EDI; break;
		  } /*end switch */ PC += 2; break;
		case 1:
		  switch (modrm & 7) {
		    case 0: temp = EAX + *(signed char *)(PC+2); break;
		    case 1: temp = ECX + *(signed char *)(PC+2); break;
		    case 2: temp = EDX + *(signed char *)(PC+2); break;
		    case 3: temp = EBX + *(signed char *)(PC+2); break;
		    case 4: /* SIB form */ {
			int scale, base=0, index=0;
			int sib = *(PC + 2);
			scale = sib >> 6;
			switch ((sib>>3)&7) {
			    case 0: index = EAX; break;
			    case 1: index = ECX; break;
			    case 2: index = EDX; break;
			    case 3: index = EBX; break;
			    case 4: index = 0; break;
			    case 5: index = EBP; break;
			    case 6: index = ESI; break;
			    case 7: index = EDI; break;
			}
			switch (sib&7) {
			    case 0: base = EAX; break;
			    case 1: base = ECX; break;
			    case 2: base = EDX; break;
			    case 3: base = EBX; break;
			    case 4: base = ESP; break;
			    case 5: base = EBP; break;
			    case 6: base = ESI; break;
			    case 7: base = EDI; break;
			}
			temp = (index << scale) + base + *(signed char *)(PC+3);
			} break;
		    case 5: temp = EBP + *(signed char *)(PC+2); break;
		    case 6: temp = ESI + *(signed char *)(PC+2); break;
		    case 7: temp = EDI + *(signed char *)(PC+2); break;
		  } /*end switch */ PC += 3; break;
		case 2:
		  switch (modrm & 7) {
		    case 0: temp = EAX + FETCH_QUAD(PC+2); break;
		    case 1: temp = ECX + FETCH_QUAD(PC+2); break;
		    case 2: temp = EDX + FETCH_QUAD(PC+2); break;
		    case 3: temp = EBX + FETCH_QUAD(PC+2); break;
		    case 4: /* SIB form */ {
			int scale, base=0, index=0;
			int sib = *(PC + 2);
			scale = sib >> 6;
			switch ((sib>>3)&7) {
			    case 0: index = EAX; break;
			    case 1: index = ECX; break;
			    case 2: index = EDX; break;
			    case 3: index = EBX; break;
			    case 4: index = 0; break;
			    case 5: index = EBP; break;
			    case 6: index = ESI; break;
			    case 7: index = EDI; break;
			}
			switch (sib&7) {
			    case 0: base = EAX; break;
			    case 1: base = ECX; break;
			    case 2: base = EDX; break;
			    case 3: base = EBX; break;
			    case 4: base = ESP; break;
			    case 5: base = EBP; break;
			    case 6: base = ESI; break;
			    case 7: base = EDI; break;
			}
			temp = (index << scale) + base + FETCH_QUAD(PC+3);
			} break;
		    case 5: temp = EBP + FETCH_QUAD(PC+2); break;
		    case 6: temp = ESI + FETCH_QUAD(PC+2); break;
		    case 7: temp = EDI + FETCH_QUAD(PC+2); break;
		  } /*end switch */ PC += 6; break;
		case 3:
		  switch (modrm & 7) {
		    case 0: temp = EAX; break;
		    case 1: temp = ECX; break;
		    case 2: temp = EDX; break;
		    case 3: temp = EBX; break;
		    case 4: temp = ESP; break;
		    case 5: temp = EBP; break;
		    case 6: temp = ESI; break;
		    case 7: temp = EDI; break;
		  } /*end switch */ PC += 2; break;
	    } /* end switch */
	    switch ((modrm >> 3) & 7) {
		case 0: AX = temp; return (PC);
		case 1: CX = temp; return (PC);
		case 2: DX = temp; return (PC);
		case 3: BX = temp; return (PC);
		case 4: SP = temp; return (PC);
		case 5: BP = temp; return (PC);
		case 6: SI = temp; return (PC);
		case 7: DI = temp; return (PC);
	    } /* end switch */ }
	case MOVsrfrm: {
	    unsigned short temp;
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		temp = *(unsigned short *)MEM_REF;
	    } else {
		unsigned char *mem_ref = MEM_REF;
		temp = FETCH_WORD(mem_ref);
	    }
	    switch (seg_reg) {
		case 0: /* ES */
		    SHORT_ES_16 = temp;
		    SET_SEGREG(LONG_ES,temp);
		    return (PC);
		case 1: /* CS */
		    SHORT_CS_16 = temp;
		    SET_SEGREG(LONG_CS,temp);
		    return (PC);
		case 2: /* SS */
		    SHORT_SS_16 = temp;
		    SET_SEGREG(LONG_SS,temp);
		    return (PC);
		case 3: /* DS */
		    SHORT_DS_16 = temp;
		    SET_SEGREG(LONG_DS,temp);
		    return (PC);
		case 4: /* FS */
		    SHORT_FS_16 = temp;
		    SET_SEGREG(LONG_FS,temp);
		    return (PC);
		case 5: /* GS */
		    SHORT_GS_16 = temp;
		    SET_SEGREG(LONG_GS,temp);
		    return (PC);
		case 6: /* Illegal */
		case 7: /* Illegal */
		goto illegal_op; 
		    /* trap this */
	    }}
	case POPrm: {
	    unsigned char *mem_ref, *sp;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF; sp = LONG_SS + SP;
	    *mem_ref = *sp; *(mem_ref+1) = *(sp+1);
	    SP += 2;
	    } return (PC);

	case MOVmal: {
	    unsigned char *mem_ref;;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    AL = *mem_ref;
	    } PC += 5; return (PC);
	case MOVmax: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    AX = FETCH_WORD(mem_ref);
	    } PC += 5; return (PC);
	case MOValm: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    *mem_ref = AL;
	    } PC += 5; return (PC);
	case MOVaxm: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
            *mem_ref = AL;
            *(mem_ref + 1) = AH;
	    } PC += 5; return (PC);
	case MOVSb: {
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    *dest = *src;
	    (env->flags & DIRECTION_FLAG)?(ESI--,EDI--):(ESI++,EDI++);
	    } PC += 1; return (PC);
	case MOVSw: {
	    int temp;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    temp = FETCH_WORD(src);
	    PUT_WORD(dest, temp);
	    (env->flags & DIRECTION_FLAG)?(ESI-=2,EDI-=2):(ESI+=2,EDI+=2);
	    } PC += 1; return (PC);
	case CMPSb: {
	    unsigned int res, src1, src2;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    src1 = *src;
	    src2 = *dest;
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(ESI--,EDI--):(ESI++,EDI++);
	    RES_32 = res << 8; SRC1_8 = src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); BYTE_FLAG = BYTE_OP;
	    } PC += 1; return (PC);
	case CMPSw: {
	    unsigned int res, src1, src2;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    src1 = FETCH_WORD(src);
	    src2 = FETCH_WORD(dest);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(ESI-=2,EDI-=2):(ESI+=2,EDI+=2);
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 1; return (PC);
	case STOSb:
	    LONG_ES[EDI] = AL;
	    (env->flags & DIRECTION_FLAG)?EDI--:EDI++;
	    PC += 1; return (PC);
	case STOSw:
	    LONG_ES[EDI] = AL;
	    LONG_ES[EDI+1] = AH;
	    (env->flags & DIRECTION_FLAG)?(EDI-=2):(EDI+=2);
	    PC += 1; return (PC);
	case LODSb: {
	    unsigned char *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    AL = *seg;
	    (env->flags & DIRECTION_FLAG)?ESI--:ESI++;
	    } PC += 1; return (PC);
	case LODSw: {
	    unsigned char *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    AX = FETCH_WORD(seg);
	    (env->flags & DIRECTION_FLAG)?(ESI-=2):(ESI+=2);
	    } PC += 1; return (PC);
	case SCASb: {
	    unsigned int res, src1, src2;
	    src1 = AL;
	    src2 = (LONG_ES[EDI]);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?EDI--:EDI++;
	    RES_32 = res << 8; SRC1_8 = src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); BYTE_FLAG = BYTE_OP;
	    } PC += 1; return (PC);
	case SCASw: {
	    unsigned int res, src1, src2;
	    unsigned char *mem_ref;
	    src1 = AX;
	    mem_ref = LONG_ES + (EDI);
	    src2 = FETCH_WORD(mem_ref);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(EDI-=2):(EDI+=2);
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 1; return (PC);
	case SHIFTbi: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    count = *PC & 0x1f;  
	    PC += 1;
	    if (count) {
		mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp << count) | (temp >> (8 - count));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 1: /* ROR */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp >> count) | (temp << (8 - count));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 2: /* RCL */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp << count) | (temp >> (9 - count)
                            | (CARRY & 1) << (count - 1));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 3: /* RCR */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp >> count) | (temp << (9 - count)
                            | (CARRY & 1) << (8 - count));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			temp = (temp << count);
			*mem_ref = temp;
			RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
			return (PC);
		    case 5: /* SHR */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = *mem_ref; 
			temp = ((temp << 24) >> 24);
			SRC1_8 = SRC2_8 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
			return (PC);
		} } else  return (PC);
	    }
	case SHIFTwi: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    count = *PC & 0x1f;  
	    PC += 1;
	    if (count) {
	      if (IS_MODE_REG) {
		unsigned short *reg = (unsigned short *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = *reg;
                        count &=0xf;
                        trans_interp_flags(env,interp_var);
        /*              temp = FETCH_WORD(mem_ref); */
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (16 - count));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 1: /* ROR */
                        temp = *reg;
                        count &=0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (16 - count));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 2: /* RCL */
                        temp = *reg;
                        count &=0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (17 - count)
                            | (CARRY & 1) << (count - 1));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 3: /* RCR */
                        temp = *reg;
                        count &=0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (17 - count)
                            | (CARRY & 1) << (16 - count));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			temp = (temp << count);
			RES_32 = temp;
			*reg = temp;
			return (PC);
		    case 5: /* SHR */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = *(signed short *)reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			return (PC);
		}
	      } else {
		unsigned char *mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (16 - count));
                        RES_32 = temp;
                        PUT_WORD(mem_ref,temp);
                        goto log_rot;
		    case 1: /* ROR */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (16 - count));
                        RES_32 = temp;
                        PUT_WORD(mem_ref,temp);
                        goto log_rot;
		    case 2: /* RCL */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (17 - count)
                            | (CARRY & 1) << (count - 1));
                        RES_32 = temp;
                        PUT_WORD(mem_ref,temp);
                        goto log_rot;
		    case 3: /* RCR */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (17 - count)
                            | (CARRY & 1) << (16 - count));
                        RES_32 = temp;
                        PUT_WORD(mem_ref,temp);
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			temp = (temp << count);
			RES_32 = temp;
			PUT_WORD(mem_ref,temp);
			return (PC);
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			PUT_WORD(mem_ref,temp);
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = FETCH_WORD(mem_ref); 
			temp = ((temp << 24) >> 24);
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			PUT_WORD(mem_ref,temp);
			return (PC);
	      } } } else  return (PC);
	    }
	case LES: {
	    int temp; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_WORD(mem_ref);
	    *XREG1 = temp;
	    temp = FETCH_WORD(mem_ref+2);
	    SHORT_ES_32 = temp;
	    SET_SEGREG(LONG_ES,temp);
	    } return (PC);
	case LDS: {
	    int temp; unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_WORD(mem_ref);
	    *XREG1 = temp;
	    temp = FETCH_WORD(mem_ref+2);
	    SHORT_DS_32 = temp;
	    SET_SEGREG(LONG_DS,temp);
	    } return (PC);
	case MOVbirm:
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    *MEM_REF = *PC; PC += 1; return (PC);
	case MOVwirm: {
	    /*int temp;*/ unsigned char *mem_ref;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		*(unsigned short *)mem_ref = FETCH_WORD(PC);
		PC += 2; return (PC);
	    } else {
		*mem_ref = *PC; *(mem_ref+1)= *(PC+1);
		PC += 2; return (PC);
	    } } 
	case SHIFTb: {
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    switch (temp) {
		case 0: /* ROL */
                    temp = *mem_ref;
                    trans_interp_flags(env,interp_var);
                    SRC1_8 = SRC2_8 = temp;
                    temp = (temp << 1) | (temp >> 7);
                    *mem_ref = temp;
                    RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                    goto log_rot;
		case 1: /* ROR */
                    temp = *mem_ref;
                    trans_interp_flags(env,interp_var);
                    SRC1_8 = SRC2_8 = temp;
                    temp = (temp >> 1) | (temp << 7);
                    *mem_ref = temp;
                    RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                    goto log_rot;
		case 2: /* RCL */
                    temp = *mem_ref;
                    trans_interp_flags(env,interp_var);
                    SRC1_8 = SRC2_8 = temp;
                    temp = (temp << 1) | (CARRY & 1);
                    *mem_ref = temp;
                    RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                    goto log_rot;
		case 3: /* RCR */
                    temp = *mem_ref;
                    trans_interp_flags(env,interp_var);
                    SRC1_8 = SRC2_8 = temp;
                    temp = (temp >> 1) | ((CARRY & 1) << 7);
                    *mem_ref = temp;
                    RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                    goto log_rot;
		case 4: /* SHL/SAL */
		    temp = *mem_ref; 
		    SRC1_8 = SRC2_8 = temp;
		    temp = (temp << 1);
		    *mem_ref = temp;
		    RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 5: /* SHR */
		    temp = *mem_ref; 
		    SRC1_8 = SRC2_8 = temp;
		    CARRY = temp & 1;
		    temp = (temp >> 1);
		    *mem_ref = temp;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    return (PC);
		case 6: /* Illegal */
		goto illegal_op; 
		case 7: /* SAR */
		    temp = *mem_ref; 
		    temp = ((temp << 24) >> 24);
		    SRC1_8 = SRC2_8 = temp;
		    CARRY = temp & 1;
		    temp = (temp >> 1);
		    *mem_ref = temp;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    return (PC);
		} } 
	case SHIFTw: {
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		unsigned short *reg = (unsigned short *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = *reg;
/*                      temp = FETCH_WORD(mem_ref); */
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << 1) | (temp >> 15);
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 1: /* ROR */
                        temp = *reg;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> 1) | (temp << 15);
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 2: /* RCL */
                        temp = *reg;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << 1) | (CARRY & 1);
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 3: /* RCR */
                        temp = *reg;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> 1) | (temp << 16)
                            | ((CARRY & 1) << 15);
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			temp = (temp << 1);
			RES_32 = temp;
			*reg = temp;
			return (PC);
		    case 5: /* SHR */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			*reg = temp;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = *(signed short *)reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			*reg = temp;
			return (PC);
		}
	      } else {
		unsigned char *mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = FETCH_WORD(mem_ref);
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << 1) | (temp >> 15);
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 1: /* ROR */
                        temp = FETCH_WORD(mem_ref);
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> 1) | (temp << 15);
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 2: /* RCL */
                        temp = FETCH_WORD(mem_ref);
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << 1) | (CARRY & 1);
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 3: /* RCR */
                        temp = FETCH_WORD(mem_ref);
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> 1) | (temp << 16)
                            | (CARRY & 1) << 15;
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			temp = (temp << 1);
			RES_32 = temp;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = FETCH_WORD(mem_ref); 
			temp = ((temp << 24) >> 24);
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			PUT_WORD(mem_ref, temp);
			return (PC);
	      } }
	    }
	case SHIFTbv: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_byte(env,PC,interp_var);
	    count = CX & 0x1f; 
	    if (count) {
		mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp << count) | (temp >> (8 - count));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 1: /* ROR */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp >> count) | (temp << (8 - count));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 2: /* RCL */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp << count) | (temp >> (9 - count)
                            | (CARRY & 1) << (count - 1));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 3: /* RCR */
                        temp = *mem_ref;
                        count &= 7;
                        trans_interp_flags(env,interp_var);
                        SRC1_8 = SRC2_8 = temp;
                        temp = (temp >> count) | (temp << (9 - count)
                            | (CARRY & 1) << (8 - count));
                        *mem_ref = temp;
                        RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			temp = (temp << count);
			*mem_ref = temp;
			RES_32 = temp << 8; BYTE_FLAG = BYTE_OP;
			return (PC);
		    case 5: /* SHR */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = *mem_ref; 
			temp = ((temp << 24) >> 24);
			SRC1_8 = SRC2_8 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
			return (PC);
		} } else  return (PC);
	    }
	case SHIFTwv: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    count = CX & 0x1f;  
	    if (count) {
	      if (IS_MODE_REG) {
		unsigned short *reg = (unsigned short *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = *reg;
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
/*                      temp = FETCH_WORD(mem_ref); */
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (16 - count));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 1: /* ROR */
                        temp = *reg;
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (16 - count));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 2: /* RCL */
                        temp = *reg;
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (17 - count)
                            | (CARRY & 1) << (count - 1));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 3: /* RCR */
                        temp = *reg;
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (17 - count)
                            | (CARRY & 1) << (16 - count));
                        RES_32 = temp;
                        *reg = temp;
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			temp = (temp << count);
			RES_32 = temp;
			*reg = temp;
			return (PC);
		    case 5: /* SHR */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_32 = temp;
			*reg = temp;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = *(signed short *)reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			return (PC);
		}
	      } else {
		unsigned char *mem_ref = MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (16 - count));
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 1: /* ROR */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (16 - count));
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 2: /* RCL */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp << count) | (temp >> (17 - count)
                            | (CARRY & 1) << (count - 1));
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 3: /* RCR */
                        temp = FETCH_WORD(mem_ref);
                        count &= 0xf;
                        trans_interp_flags(env,interp_var);
                        SRC1_16 = SRC2_16 = temp;
                        temp = (temp >> count) | (temp << (17 - count)
                            | (CARRY & 1) << (16 - count));
                        RES_32 = temp;
                        PUT_WORD(mem_ref, temp);
                        goto log_rot;
		    case 4: /* SHL/SAL */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			temp = (temp << count);
			RES_32 = temp;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_32 = temp;
			PUT_WORD(mem_ref, temp);
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op; 
		    case 7: /* SAR */
			temp = FETCH_WORD(mem_ref); 
			temp = ((temp << 24) >> 24);
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			PUT_WORD(mem_ref, temp);
			return (PC);
	      } } } else  return (PC);
	    }
	case XLAT: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + (EBX) + (AL);
	    AL = *mem_ref; }
	    PC += 1; return (PC);

	case ESC0: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg0[funct](reg);
	    else hsw_fp87_mem0[funct](MEM_REF);
	    } return (PC);
	case ESC1: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg1[funct](reg);
	    else hsw_fp87_mem1[funct](MEM_REF);
	    } return (PC);
	case ESC2: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg2[funct](reg);
	    else hsw_fp87_mem2[funct](MEM_REF);
	    } return (PC);
	case ESC3: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg3[funct](reg);
	    else hsw_fp87_mem3[funct](MEM_REF);
	    } return (PC);
	case ESC4: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg4[funct](reg);
	    else hsw_fp87_mem4[funct](MEM_REF);
	    } return (PC);
	case ESC5: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg5[funct](reg);
	    else hsw_fp87_mem5[funct](MEM_REF);
	    } return (PC);
	case ESC6: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg6[funct](reg);
	    else hsw_fp87_mem6[funct](MEM_REF);
	    } return (PC);
	case ESC7: {
            int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg7[funct](reg);
	    else hsw_fp87_mem7[funct](MEM_REF);
	    } return (PC);

	case REPNE: {
	    int count = CX;

#ifdef DEBUG
#ifdef DOSEMU
	instr_count += count;
	e_debug(env, P0, PC, interp_var);
#else
if((instr_count++)==start_count)print=1;
if(instr_count==end_count)print=0;
    if(print && (!(instr_count % granularity))){
if(small_print)
printf("%d %04x:%04x %02x %02x\n", instr_count, SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2));
	else if (segment_print)
	    printf("%04x:%04x DS:%04x ES:%04x FS:%04x GS:%04x SS:%04x %d\n",
		SHORT_CS_16, PC-(LONG_CS), SHORT_DS_16, SHORT_ES_16, 
		SHORT_FS_16, SHORT_GS_16, SHORT_SS_16, instr_count);
	else if (stack_print) {
	    unsigned char *sp = LONG_SS + ESP;
	    printf("%04x:%04x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %d\n",
		SHORT_CS_16, PC-(LONG_CS),*(sp+1),*sp,*(sp+3),*(sp+2),*(sp+5),
		*(sp+4),*(sp+7),*(sp+6),*(sp+9),*(sp+8),*(sp+11),*(sp+10),
		*(sp+13),*(sp+12), *(sp+15),*(sp+14),instr_count);
	}
else
printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2), AX, BX, CX, DX, ESI, EDI, BP, SP, decode(*(PC+1), *(PC+2)), instr_count);
fflush(stdout);
}
#endif
#endif

	    PC += 2;
segrepne:
	    switch (*(PC-1)) {
		case INSb:
		case INSw:
		case OUTSb:
		case OUTSw:
#ifdef DOSEMU
		    if (vm86f) goto not_permitted;
		    goto not_implemented;
#else
		    fprintf(stderr,unsupp_msg,*PC,SHORT_CS_16,PC-LONG_CS);
		    exit(1);
#endif
		case MOVSb: {
		    unsigned char *src, *dest;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; CX = 0;
			while (count--) *dest-- = *src--;
			return (PC);
		    } else {
			EDI += count; ESI += count; CX = 0;
			while (count--) *dest++ = *src++;
			return (PC);
		    } } 
		case MOVSw: {
		    unsigned char *src, *dest;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    count = count << 1;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; CX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    dest -= 2; src -= 2; count -= 2;
			} return (PC);
		    } else {
			EDI += count; ESI += count; CX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    dest += 2; src += 2; count -= 2;
			} return (PC);
		    } }
		case CMPSb: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0, src2=0;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = *src--;
			    src2 = *dest--;
			    if (src1!=src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
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
			    if (src1!=src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res << 8; SRC1_8 = src1; 
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
		    } return (PC);
		case CMPSw: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0;
		    int src2=0;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src -= 2; dest -=2;
			    if (src1!=src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1;
		    		SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src += 2; dest +=2;
			    if (src1!=src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1;
		    		SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    } return (PC);
		case STOSb: {
		    unsigned char al;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    al = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count; CX = 0;
			while (count--) *dest-- = al;
			return (PC);
		    } else {		      /* forwards */
			EDI += count; CX = 0;
			while (count--) *dest++ = al;
			return (PC);
		    } } 
		case STOSw: {
		    unsigned char al, ah;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    al = AL; ah = AH;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest -= 2;
			} return (PC);
		    } else {		      /* forwards */
			EDI += count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest += 2;
			} return (PC);
		    } }
		case LODSb: {
		    unsigned char *src;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			AL = *(src - count); ESI -= count;
			CX = 0; return (PC);
		    } else {		      /* forwards */
			AL = *(src + count); ESI += count;
			CX = 0; return (PC);
		    } } 
		case LODSw: {
		    unsigned char *src;
		    count = count << 1;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; ESI -= count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; ESI += count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; return (PC);
		    } }
		case SCASb: {
		    unsigned int res, src1;
		    int src2;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    src1 = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = *dest;
			    dest -=1;
			    if (src1 != src2) count--;
			    else {
			    	res = src1 - src2;
				CX = count - 1;
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
			    if (src1 != src2) count--;
			    else {
			    	res = src1 - src2;
				CX = count - 1;
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
		    CX = 0; EDI = dest - LONG_ES;
		    RES_32 = res << 8; SRC1_8 = src1;
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } return (PC);
		case SCASw: {
		    unsigned char *dest;
		    unsigned int res, src1;
		    int src2;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    src1 = AX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest -=2;
			    if (src1 != src2) count--;
			    else {
			    	res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
		    		SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                         	if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest -=2;
			    if (src1 != src2) count--;
			    else {
			    	res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
		    		SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                         	if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    CX = 0; EDI = dest - LONG_ES;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } return (PC);
/*----------IA--------------------------------*/
	        case SEGes:
       		    OVERRIDE = LONG_ES;
            	    PC+=1; goto segrepne;
        	case SEGcs:
            	    OVERRIDE = LONG_CS;
          	    PC+=1; goto segrepne;
        	case SEGss:
            	    OVERRIDE = LONG_SS;
            	    PC+=1; goto segrepne;
        	case SEGds:
            	    OVERRIDE = LONG_DS;
            	    PC+=1; goto segrepne;
        	case SEGfs:
            	    OVERRIDE = LONG_FS;
            	    PC+=1; goto segrepne;
        	case SEGgs:
            	    OVERRIDE = LONG_GS;
            	    PC+=1; goto segrepne;
/*----------IA--------------------------------*/
		default: PC--; return (PC);
	    } }

	case REP: {     /* also is REPE */
	    int count = CX;

#ifdef DEBUG
#ifdef DOSEMU
	instr_count += count;
	e_debug(env, P0, PC, interp_var);
#else
if((instr_count++)==start_count)print=1;
if(instr_count==end_count)print=0;
    if(print && (!(instr_count % granularity))){
if(small_print)
printf("%d %04x:%04x %02x %02x\n", instr_count, SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2));
	else if (segment_print)
	    printf("%04x:%04x DS:%04x ES:%04x FS:%04x GS:%04x SS:%04x %d\n",
		SHORT_CS_16, PC-(LONG_CS), SHORT_DS_16, SHORT_ES_16, 
		SHORT_FS_16, SHORT_GS_16, SHORT_SS_16, instr_count);
	else if (stack_print) {
	    unsigned char *sp = LONG_SS + ESP;
	    printf("%04x:%04x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %d\n",
		SHORT_CS_16, PC-(LONG_CS),*(sp+1),*sp,*(sp+3),*(sp+2),*(sp+5),
		*(sp+4),*(sp+7),*(sp+6),*(sp+9),*(sp+8),*(sp+11),*(sp+10),
		*(sp+13),*(sp+12), *(sp+15),*(sp+14),instr_count);
	}
else
printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2), AX, BX, CX, DX, ESI, EDI, BP, SP, decode(*(PC+1), *(PC+2)), instr_count);
fflush(stdout);
}
#endif
#endif

	    PC += 2;
segrep:
	    switch (*(PC-1)) {
		case INSb:
		case INSw:
		case OUTSb:
		case OUTSw:
#ifdef DOSEMU
		    if (vm86f) goto not_permitted;
		    goto not_implemented;
#else
		    fprintf(stderr,unsupp_msg,*PC,SHORT_CS_16,PC-LONG_CS);
		    exit(1);
#endif
		case MOVSb: {
		    unsigned char *src, *dest;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; CX = 0;
			while (count--) *dest-- = *src--;
			return (PC);
		    } else {
			EDI += count; ESI += count; CX = 0;
			while (count--) *dest++ = *src++;
			return (PC);
		    } } 
		case MOVSw: {
		    unsigned char *src, *dest;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    count = count << 1;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; CX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    dest -= 2; src -= 2; count -= 2;
			} return (PC);
		    } else {
			EDI += count; ESI += count; CX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    dest += 2; src += 2; count -= 2;
			} return (PC);
		    } }
		case CMPSb: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0, src2=0;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = *src--;
			    src2 = *dest--;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
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
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res << 8; SRC1_8 = src1; 
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
		    } return (PC);
		case CMPSw: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0;
		    int src2=0;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src -= 2; dest -=2;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1;
	    			SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src += 2; dest +=2;
			    if (src1 == src2) count--;
	  		    else {
			        res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1;
	    			SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res; SRC1_16 = src1;
	    	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    } return (PC);
		case STOSb: {
		    unsigned char al;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    al = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count; CX = 0;
			while (count--) *dest-- = al;
			return (PC);
		    } else {		      /* forwards */
			EDI += count; CX = 0;
			while (count--) *dest++ = al;
			return (PC);
		    } } 
		case STOSw: {
		    unsigned char al, ah;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    al = AL; ah = AH;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			EDI -= count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest -= 2;
			} return (PC);
		    } else {		      /* forwards */
			EDI += count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest += 2;
			} return (PC);
		    } }
		case LODSb: {
		    unsigned char *src;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			AL = *(src - count); ESI -= count;
			CX = 0; return (PC);
		    } else {		      /* forwards */
			AL = *(src + count); ESI += count;
			CX = 0; return (PC);
		    } } 
		case LODSw: {
		    unsigned char *src;
		    count = count << 1;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; ESI -= count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; ESI += count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; return (PC);
		    } }
		case SCASb: {
		    unsigned int res, src1;
		    int src2;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    src1 = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = *dest;
			    dest -=1;
			    if (src1 == src2) count--;
			    else {
			    	res = src1 - src2;
				CX = count - 1;
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
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
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
		    CX = 0; EDI = dest - LONG_ES;
		    RES_32 = res << 8; SRC1_8 = src1;
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } return (PC);
		case SCASw: {
		    unsigned char *dest;
		    unsigned int res, src1;
		    int src2;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    src1 = AX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest -=2;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
	    			SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	                        if((src1 & 0xf) < src2) SET_AF
	                        else CLEAR_AF
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest -=2;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				CX = count - 1;
				EDI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
	    			SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	                        if((src1 & 0xf) < src2) SET_AF
	                        else CLEAR_AF
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    CX = 0; EDI = dest - LONG_ES; 
		    RES_32 = res; SRC1_16 = src1;
	    	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
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
/*----------IA--------------------------------*/
		default: PC--; return (PC);
	    } }

	case GRP1brm: {
	    int src1, src2, res; unsigned char *mem_ref;
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
		case 1: /* TEST (Is this Illegal?) */
		goto illegal_op;
		case 2: /* NOT */
		    src1 = ~(*mem_ref);
		    *mem_ref = src1;
		    return (PC);
		case 3: /* NEG */
		    src1 = *mem_ref;
		    src1 = (((src1 & 0xff) == 0x80)? 0:-src1);
		    *mem_ref = src1;
		    RES_32 = src1 << 8;
                    SRC1_16 = 0;
                    SRC2_16 = RES_16;
		    BYTE_FLAG = BYTE_OP;
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
		    res = AX;
		    AL = res / src1;
		    AH = res % src1;
		    return (PC);
		case 7: /* IDIV AX */
		    src1 = *(signed char *)mem_ref;
		    res = AX;
		    AL = res / src1;
		    AH = res % src1;
		    return (PC);
	    } }
	case GRP1wrm: {
	    int src1, src2, res; unsigned char *mem_ref;
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
		    *(unsigned short *)mem_ref = src1;
		    return (PC);
		case 3: /* NEG */
		    src1 = FETCH_XREG(mem_ref);
		    src1 = (((src1 & 0xffff) == 0x8000)? 0:-src1);
		    src1 = -(src1);
		    *(unsigned short *)mem_ref = src1;
		    RES_32 = src1;
		    SRC1_16 = 0;
		    SRC2_16 = src1;
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
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
		case 7: /* IDIV DX+AX */
		    src1 = *(signed short *)mem_ref;
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
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
		    src1 = FETCH_WORD(mem_ref);
		    src1 = (((src1 & 0xffff) == 0x8000)? 0:-src1);
		    PUT_WORD(mem_ref, src1);
		    RES_32 = src1;
		    SRC1_16 = 0;
		    SRC2_16 = src1;
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
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
		case 7: /* IDIV DX+AX */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ((src1<<16)>>16);
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    return (PC);
	      } }
	      }

	case GRP2brm: { /* only INC and DEC are legal on bytes */
	    int temp; unsigned char *mem_ref;
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
	    }}

	case GRP2wrm: {
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (temp) {
		case 0: /* INC */
		    temp = FETCH_XREG(mem_ref);
		    SRC1_16 = temp; SRC2_16 = 1;
		    *(unsigned short *)mem_ref = RES_16 = temp + 1;
		    return (PC);
		case 1: /* DEC */
		    temp = FETCH_XREG(mem_ref);
		    SRC1_16 = temp; SRC2_16 = -1;
		    *(unsigned short *)mem_ref = RES_16 = temp - 1;
		    return (PC);
		case 2: /* CALL near indirect */
		    temp = PC - LONG_CS;
		    PUSHWORD(temp);
		    PC = LONG_CS + FETCH_XREG(mem_ref);
		    return (PC);
		case 3: /* CALL long indirect */ /* Illegal */
		goto illegal_op; 
		case 4: /* JMP near indirect */
		    PC = LONG_CS + FETCH_XREG(mem_ref);
		    return (PC);
		case 5: /* JMP long indirect */ /* Illegal */
		goto illegal_op; 
		case 6: /* PUSH */
		    temp = FETCH_XREG(mem_ref);
		    PUSHWORD(temp);
		    return (PC);
		case 7: /* Illegal */
		goto illegal_op; 
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
		case 3:  /* CALL long indirect */ /* {
		    unsigned int cs = SHORT_CS_16;
		    unsigned int ip = PC - LONG_CS;
		    PUSHWORD(cs);
		    PUSHWORD(ip);
		    env->return_addr = (cs << 16)|ip;
		    } */
		case 5: /* JMP long indirect */ /* {
		    unsigned int cs, ip;
		    unsigned short transfer_magic;
		    ip = FETCH_WORD(mem_ref);
		    cs = FETCH_WORD(mem_ref+2);
		    transfer_magic = (WORD)GetSelectorType(cs);
		    if (transfer_magic == TRANSFER_CODE16) {
			SHORT_CS_16 = cs;
			SET_SEGREG(LONG_CS,cs);
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
#ifdef DEBUG
			if (transfer_magic == TRANSFER_CALLBACK)
			    LOGSTR((LF_DEBUG,
				"do_ext: jump to %s\n", GetProcName(cs,ip>>3)));
			else    ------ TRANSFER_BINARY 
			    LOGSTR((LF_DEBUG,
				"do_ext: jumping to binary thunk %x:%x\n",cs,ip));
#endif
			(conv)(env,targ);
			SET_SEGREG(LONG_CS,SHORT_CS_16);
			SET_SEGREG(LONG_DS,SHORT_DS_16);
			SET_SEGREG(LONG_ES,SHORT_ES_16);
			SET_SEGREG(LONG_SS,SHORT_SS_16);
			PC += 5; return (PC);
		    }
		    if (transfer_magic == TRANSFER_RETURN) {
			SHORT_CS_16 = cs;
			env->return_addr = (cs << 16) | ip;
			trans_interp_flags(env, interp_var);    
			return;
		    }
		    invoke_data(env);   
		} */ goto illegal_op;
		case 6: /* PUSH */
		    temp = FETCH_WORD(mem_ref);
		    PUSHWORD(temp);
		    return (PC);
		case 7: /* Illegal */
		goto illegal_op; 
	      }
	    } }
	default: return(PC);
    }

    log_rot: { /* logical rotation don't change SF ZF AF PF flags */
        DWORD flags;
        flags = env->flags;
        if(IS_CF_SET) flags |= CARRY_FLAG;
        else flags &= ~CARRY_FLAG;
        if(IS_OF_SET) flags |= OVERFLOW_FLAG;
        else flags &= ~OVERFLOW_FLAG;
        trans_flags_to_interp(env,interp_var, flags);
        } return(PC);
 
    not_implemented:
	error(" 16/32 nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2),
		SHORT_CS_16,PC-LONG_CS,(int)PC);
#ifdef DOSEMU
	*err = EXCP06_ILLOP; return (P0);
#else
	exit(1);
#endif

    not_permitted:
#ifdef DOSEMU
	*err = EXCP0D_GPF; return P0;
#endif
    illegal_op:
	error(" 16/32 illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2), 
                SHORT_CS_16,PC-LONG_CS,(int)PC);
#ifdef DOSEMU
	*err = EXCP06_ILLOP; return (P0);
#else
        exit(1); 
#endif
}
