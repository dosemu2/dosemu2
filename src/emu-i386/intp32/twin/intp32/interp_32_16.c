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
 */

#include <stdio.h>
#ifdef linux
#include <asm/bitops.h>
#endif
#include "Log.h"
#include "hsw_interp.h"
#include "mod_rm.h"
#include <math.h>

#ifdef DEBUG
#include <string.h>
extern char * decode(int opcode, int modrm);
extern char *getenv();
extern int print;
extern int small_print;
extern int stack_print;
extern int op32_print;
extern int segment_print;
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
#define C2P32	4294967296.0
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
  trans_interp_flags (Interp_ENV *env, Interp_VAR *interp_var);
extern void
  trans_flags_to_interp (Interp_ENV *env, Interp_VAR *interp_var, unsigned int flags);

extern REGISTER PortIO (DWORD, DWORD, UINT, BOOL);

#ifdef DOSEMU
#  define fprintf our_fprintf
#  define our_fprintf(stream, args...) error(##args)
#endif

unsigned char *
hsw_interp_32_16 (Interp_ENV *env, unsigned char *P0, unsigned char *PC,
	Interp_VAR *interp_var, int *err)
{
#ifdef DEBUG
    e_debug(env, P0, PC, interp_var);
#ifdef DOSEMU
#else
    if(op32_print){
         printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS),*PC, *(PC+1), AX, BX, CX, DX, SI, DI, BP, SP, decode(*PC, *(PC+1)), instr_count);
	fflush(stdout);
	}
#endif
#endif
    *err = 0;
    override:
    switch (*PC) {
	case ADDwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    SETDFLAGS;
	    } return (PC); 
	case ADDwtrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 + src2;
	    SETDFLAGS;
	    } return (PC); 
	case ADDwia: {
	    DWORD res, src1, src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 + src2;
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case PUSHes: {
	    unsigned long temp = SHORT_ES_32;
	    PUSHQUAD(temp); 
	    } PC += 1; return (PC);
	case POPes: {
	    unsigned long temp;
	    POPQUAD(temp);
	    SET_SEGREG(LONG_ES,temp);
	    SHORT_ES_32 = temp; }
	    PC += 1; return (PC);
	case ORwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    SETDFLAGS;
	    } return (PC); 
	case ORwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 | src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } return (PC); 
	case ORwi: {
	    DWORD res, src1,src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 | src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case PUSHcs: {
	    unsigned long temp = SHORT_CS_32;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case TwoByteESC: {
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    if (IS_MODE_REG) *(unsigned long *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 1: /* STR */ {
			    /* Store Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    if (IS_MODE_REG) *(unsigned long *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC +1 + hsw_modrm_16_quad(env,PC + 1,interp_var);
			    return (PC);
			case 3: /* LTR */ {
			    /* Load Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
			    /* hsw_task_register = temp; */
			    } return (PC);
			case 4: /* VERR */ {
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
		goto not_implemented;
			    /* if (hsw_verr(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } return (PC);
			case 5: /* VERW */ {
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
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
		    }
		case 0x01: /* GRP7 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 1: /* ESIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 2: /* LGDT */ /* Privileged */
			    /* Load Global Descriptor Table Register */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC+1+hsw_modrm_16_quad(env,PC + 1,interp_var);
		goto not_implemented;
			    return (PC);
			case 3: /* LIDT */ /* Privileged */
			    /* Load Interrupt Descriptor Table Register */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC+1+hsw_modrm_16_quad(env,PC + 1,interp_var);
		goto not_implemented;
			    return (PC);
			case 4: /* SMSW */ {
			    /* Store Machine Status Word */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LIMIT field */;
			    if (IS_MODE_REG) *(unsigned long *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 5: /* Illegal */
		goto illegal_op;
			case 6: /* LMSW */ /* Privileged */
			    /* Load Machine Status Word */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    goto not_implemented;
			    PC = PC+1+hsw_modrm_16_quad(env,PC + 1,interp_var);
			    return (PC);
			case 7: /* Illegal */
		goto illegal_op;
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
		    /*int temp;*/ unsigned char *mem_ref;
		    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
		    mem_ref = MEM_REF;
		goto not_implemented;
		    /* what do I do here??? */
		    } return (PC);
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
		    /*int temp;*/ unsigned char *mem_ref;
		    PC += 1; PC += hsw_modrm_16_quad(env,PC,interp_var);
		    mem_ref = MEM_REF;
		goto not_implemented;
		    /* what do I do here??? */
		    } return (PC);
		case 0x06: /* CLTS */ /* Privileged */
		    /* Clear Task State Register */
#ifdef DOSEMU
		    if (vm86f) goto not_permitted;
#endif
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
#ifdef DOSEMU
		    if (vm86f) goto not_permitted;
#endif
		    goto not_implemented;
		case 0x80: /* JOimmdisp */
/*		    if (IS_OF_SET) {
			unsigned long temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x81: /* JNOimmdisp */
/*		    if (!IS_OF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x82: /* JBimmdisp */
/*		    if (IS_CF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x83: /* JNBimmdisp */
/*		    if (!IS_CF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x84: /* JZimmdisp */
/*		    if (IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x85: /* JNZimmdisp */
/*		    if (!IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x86: /* JBEimmdisp */
/*		    if (IS_CF_SET || IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x87: /* JNBEimmdisp */
/*		    if (!(IS_CF_SET || IS_ZF_SET)) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x88: /* JSimmdisp */
/*		    if (IS_SF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x89: /* JNSimmdisp */
/*		    if (!IS_SF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */ 
		case 0x8a: /* JPimmdisp */
/*		    if (IS_PF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x8b: /* JNPimmdisp */
/*		    if (!IS_PF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x8c: /* JLimmdisp */
/*		    if (IS_SF_SET ^ IS_OF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x8d: /* JNLimmdisp */
/*		    if (!(IS_SF_SET ^ IS_OF_SET)) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x8e: /* JLEimmdisp */
/*		    if ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		case 0x8f: /* JNLEimmdisp */
/*		    if (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) {
			int temp = FETCH_QUAD(PC+2);
			PC += (4 + temp);
			return (PC);
		    } PC += 4; return (PC); */
		goto illegal_op;
		case 0x90: /* SETObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x91: /* SETNObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x92: /* SETBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x93: /* SETNBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x94: /* SETZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x95: /* SETNZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x96: /* SETBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x97: /* SETNBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x98: /* SETSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET) ? 1 : 0;
		    } return (PC);
		case 0x99: /* SETNSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9a: /* SETPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9b: /* SETNPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9c: /* SETLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9d: /* SETNLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9e: /* SETLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9f: /* SETNLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0xa0: /* PUSHfs */ {
		    unsigned long temp = SHORT_FS_32;
		    PUSHQUAD(temp);
		    } PC += 2; return (PC);
		case 0xa1: /* POPfs */ {
		    unsigned long temp;
		    POPQUAD(temp);
 		    SET_SEGREG(LONG_FS,temp);
		    SHORT_FS_32 = temp;
		    } PC += 2; return (PC);
                case 0xa3: /* BT */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *EREG1;
                    if (IS_MODE_REG) {
                        ind = (ind & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                    } else {
                        if(ind >= 0) {
			    ind1 = ((ind >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (temp >> ind) & 0x1;
                        } else {
                            ind = -ind - 1;
			    ind1 = ((ind >> 5) + 1) << 2;
			    mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (((temp << ind) & 0x80000000)? 1:0);
                        }
		    }
                    } return(PC);
		case 0xa4: /* SHLDimm */ {
		    /* Double Prescision Shift Left */
		    unsigned char *mem_ref;
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
		    SETDFLAGS;
		    } return (PC);
		case 0xa5: /* SHLDcl */ {
		    /* Double Prescision Shift Left by CL */
		    unsigned char *mem_ref;
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
		    SETDFLAGS;
		    } return (PC);
		case 0xa6: /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
		case 0xa7: /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		goto not_implemented;
		case 0xa8: /* PUSHgs */ {
		    unsigned long temp = SHORT_GS_32;
		    PUSHQUAD(temp);
		    } PC += 2; return (PC);
		case 0xa9: /* POPgs */ {
		    unsigned long temp;
		    POPQUAD(temp);
		    SET_SEGREG(LONG_GS,temp);
		    SHORT_GS_32 = temp;
		    } PC += 2; return (PC);
                case 0xab: /* BTS */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *EREG1;
                    if (IS_MODE_REG) {
                        ind = (ind & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                        temp |= (0x1 << ind);
                        *(DWORD *)mem_ref = temp;
                    } else {
                        if(ind >= 0) {
			    ind1 = ((ind >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (temp >> ind) & 0x1;
                            temp |= (0x1 << ind);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind = -ind - 1;
			    ind1 = ((ind >> 5) + 1) << 2;
			    mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (((temp << ind) & 0x80000000)? 1:0);
                            temp |= (0x80000000 >> ind);
                            PUT_QUAD(mem_ref,temp);
                        }     
		    }
                    } return(PC);
		case 0xac: /* SHRDimm */ {
		    /* Double Precision Shift Right by immediate */
		    unsigned char *mem_ref, carry;
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = *PC & 0x1f; PC++;
			if (IS_MODE_REG) {
			    temp = FETCH_EREG(mem_ref);
			    carry = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    *(DWORD *)mem_ref = temp;
			} else {
			    temp = FETCH_QUAD(mem_ref);
			    carry = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    PUT_QUAD(mem_ref,temp);
			}
		    res = temp; src1 = src2 = temp << 1;
		    SETDFLAGS;
		    CARRY = carry;
		    } return (PC);
		case 0xad: /* SHRDcl */ {
		    /* Double Precision Shift Right by CL */
		    unsigned char *mem_ref, carry;
		    DWORD count, temp, temp1;
		    DWORD res, src1, src2;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = ECX & 0x1f;
			if (IS_MODE_REG) {
			    temp = FETCH_EREG(mem_ref);
			    carry = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    *(DWORD *)mem_ref = temp;
			} else {
			    temp = FETCH_QUAD(mem_ref);
			    carry = (temp >> (count - 1)) & 1;
			    temp = temp >> count;
			    temp1 = temp1 << (32 - count);
			    temp |= temp1;
			    PUT_QUAD(mem_ref,temp);
			}
		    res = temp; src1 = src2 = temp << 1;
		    SETDFLAGS;
		    CARRY = carry;
		    } return (PC);
		case 0xaf: { /* IMULregrm */
#ifdef DOSEMU
		    int64_t res, mlt;
		    unsigned char *mem_ref;
		    unsigned long himr;
		    PC = PC + 1 + hsw_modrm_16_quad (env, PC + 1, interp_var);
		    res = *EREG1; mem_ref = MEM_REF;
		    if (IS_MODE_REG)
			mlt = FETCH_EREG(mem_ref);
		    else
			mlt = FETCH_QUAD(mem_ref);
		    res *= mlt;
		    *EREG1 = (u_int32_t)res;
		    himr = (u_int32_t)((res<0? -res:res)>>32);
#else
                    MULT resm, src1m, src2m, mulr[4];
		    int sg1=1, sg2=1;
		    unsigned char *mem_ref;
		    unsigned long himr;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
                    src1m.longw = *EREG1; mem_ref = MEM_REF;
		    if(IS_MODE_REG)
               		src2m.longw = FETCH_EREG(mem_ref);
		    else
			src2m.longw = FETCH_QUAD(mem_ref);
		    if(src1m.longw<0) { src1m.longw = -src1m.longw; sg1 = -1; }
		    if(src2m.longw<0) { src2m.longw = -src2m.longw; sg2 = -1; }
		    mulr[0].longw = src1m.word.jnr * src2m.word.jnr;
		    mulr[1].longw = src1m.word.jnr * src2m.word.snjr;
		    mulr[2].longw = src1m.word.snjr * src2m.word.jnr;
		    mulr[3].longw = src1m.word.snjr * src2m.word.snjr;
		    src1m.longw = mulr[0].word.snjr + mulr[1].word.jnr + mulr[2].word.jnr;
		    himr = src2m.longw = mulr[1].word.snjr + mulr[2].word.snjr + mulr[3].longw + src1m.word.snjr;
		    resm.word.jnr = mulr[0].word.jnr;
		    resm.word.snjr = src1m.word.jnr;
		    *EREG1 = ((sg1==sg2)? resm.longw: -resm.longw);
#endif
		    RES_32 = 0;
		    if(himr) { SRC1_16 = SRC2_16 = 0x8000;
			CARRY = 1; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 0xb2: /* LSS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op;
			return (PC);
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_QUAD(mem_ref);
		    *EREG1 = temp;
		    temp = FETCH_QUAD(mem_ref+2);
		    SHORT_SS_32 = temp;
		    SET_SEGREG(LONG_SS,temp);
		    } return (PC);
                case 0xb3: /* BTR */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *EREG1;
                    if (IS_MODE_REG) {
                        ind = (ind & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                        temp &= ~(0x1 << ind);
                        *(DWORD *)mem_ref = temp;
                    } else {
                        if(ind >= 0) {
			    ind1 = ((ind >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (temp >> ind) & 0x1;
                            temp &= ~(0x1 << ind);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind = -ind - 1;
			    ind1 = ((ind >> 5) + 1) << 2;
			    mem_ref -= ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (((temp << ind) & 0x80000000)? 1:0);
                            temp &= ~(0x80000000 >> ind);
                            PUT_QUAD(mem_ref,temp);
                        }
		    }
                    } return(PC);
		case 0xb4: /* LFS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op;
			return (PC);
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_QUAD(mem_ref);
		    *EREG1 = temp;
		    temp = FETCH_QUAD(mem_ref+2);
		    SHORT_FS_32 = temp;
		    SET_SEGREG(LONG_FS,temp);
		    } return (PC);
		case 0xb5: /* LGS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op;
		    }
		    mem_ref = MEM_REF;
		    temp = FETCH_QUAD(mem_ref);
		    *EREG1 = temp;
		    temp = FETCH_QUAD(mem_ref+2);
		    SHORT_GS_32 = temp;
		    SET_SEGREG(LONG_GS,temp);
		    } return (PC);
		case 0xb6: /* MOVZXb */ {
		    DWORD temp;  
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
		    *EREG1 = (DWORD)(temp & 0xff);
		    } return (PC);
		case 0xb7: /* MOVZXw */ {
			DWORD temp; 
			unsigned char *mem_ref;
			PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
                            mem_ref = MEM_REF; temp = *PC;  PC++;
                            if (IS_MODE_REG)
                                temp1 = *(DWORD *)mem_ref;
                            else
                                temp1 = FETCH_QUAD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0x1f))&1;
                    	    } return(PC);
                        case 5: /* BTS */ {
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *EREG1;
                    if (IS_MODE_REG) {
                        ind = (ind & 0x1f);
                        temp = *(DWORD *)mem_ref;
                        CARRY = ((int)temp >> ind)&1;
                        temp ^= (0x1 << ind);
                        *(DWORD *)mem_ref = temp;
                    } else {
                        if(ind >= 0) {
			    ind1 = ((ind >> 5) << 2);
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (temp >> ind) & 0x1;
                            temp ^= (0x1 << ind);
                            PUT_QUAD(mem_ref,temp);
                        } else {
                            ind = -ind - 1;
			    ind1 = ((ind >> 5) + 1) << 2;
			    mem_ref += ind1;
                            temp = FETCH_QUAD(mem_ref);
                            ind = ind & 0x1f;
                            CARRY = (((temp << ind) & 0x80000000)? 1:0);
                            temp ^= (0x80000000 >> ind);
                            PUT_QUAD(mem_ref,temp);
                        }
		    }
                    } return(PC);
                case 0xbc: /* BSF */ {
                    DWORD temp, i;
		    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
		    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
		    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
		    *EREG1 = ((temp << 24) >> 24);
		    } return (PC);
		case 0xbf: /* MOVSXw */ {
                    signed long temp;
                    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
                    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_16_quad(env,PC+1,interp_var);
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
                    SETDFLAGS;
                    } return(PC);
                case 0xc8: /* BSWAPeax */ {
                    unsigned long temp = EAX;
                    EAX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xc9: /* BSWAPecx */ {
                    unsigned long temp = ECX;
                    ECX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xca: /* BSWAPedx */ {
                    unsigned long temp = EDX;
                    EDX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcb: /* BSWAPebx */ {
                    unsigned long temp = EBX;
                    EBX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcc: /* BSWAPesp */ {
                    unsigned long temp = ESP;
                    ESP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcd: /* BSWAPebp */ {
                    unsigned long temp = EBP;
                    EBP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xce: /* BSWAPesi */ {
                    unsigned long temp = ESI;
                    ESI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
                    } PC += 2; return(PC);
                case 0xcf: /* BSWAPedi */ {
                    unsigned long temp = EDI;
                    EDI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
           	    } PC += 2; return(PC);
                }
            }
	case ADCwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    SETDFLAGS;
	    } return (PC); 
	case ADCwtrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 + src2;
	    SETDFLAGS;
	    } return (PC); 
	case ADCwi: {
	    DWORD res, src1, src2;
	    src1 = EAX; 
	    src2 = (FETCH_QUAD((PC+1))) + (CARRY & 1);
	    EAX = res = src1 + src2;
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case PUSHss: {
	    unsigned long temp = SHORT_SS_32;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case POPss: {
	    unsigned long temp;
	    unsigned char *old_ss, *new_ss;
	    old_ss = LONG_SS;
	    POPQUAD(temp);
	    SET_SEGREG(new_ss,temp);
	    SHORT_SS_32 = temp;
	    LONG_SS = new_ss;
	    } PC += 1; return (PC);
	case SBBwfrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case SBBwtrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    signed long src2;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    src2 = src2 + (CARRY & 1);
	    *EREG1 = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case SBBwi: {
	    DWORD res, src1;
	    signed long src2;
	    src1 = EAX; 
	    src2 = (FETCH_QUAD((PC+1))) + (CARRY & 1);
	    EAX = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case PUSHds: {
	    unsigned short temp = SHORT_DS_32;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case POPds:
	    POPQUAD(SHORT_DS_32);
	    SET_SEGREG(LONG_DS,SHORT_DS_32);
	    PC += 1; return (PC);
	case ANDwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    SETDFLAGS;
	    } return (PC); 
	case ANDwtrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 & src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } return (PC); 
	case ANDwi: {
	    DWORD res, src1, src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 & src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case SEGes:
	    OVERRIDE = LONG_ES;
	    PC+=1; goto override;
        case DAA:
            if (((unsigned int)( AL & 0x0f ) > 9 ) || (IS_AF_SET)) {
                AL += 6;
                SET_AF
            } else CLEAR_AF
            if (((AL & 0xf0) > 0x90) || (IS_CF_SET)) {
                AL += 0x60;
                SET_CF;
            } else CLEAR_CF;
            RES_8 = AL;
            BYTE_FLAG = BYTE_OP;
            PC += 1; return(PC);
	case SUBwfrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case SUBwtrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case SUBwi: {
	    DWORD res, src1;
	    long src2;
	    src1 = EAX; 
	    src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case SEGcs:
	    OVERRIDE = LONG_CS;
	    PC+=1; goto override;
	case DAS:
	    if (((unsigned int)( EAX & 0x0f ) > 9 ) || (IS_AF_SET)) {
		AL -= 6;
		SET_AF;
	    } else CLEAR_AF;
	    if (((AL & 0xf0) > 0x90) || (IS_CF_SET)) {
		AL -= 0x60;
		SET_CF;
	    } else CLEAR_CF;
	    PC += 1; return (PC);
	case XORwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
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
	    SETDFLAGS;
	    } return (PC); 
	case XORwtrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 ^ src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } return (PC); 
	case XORwi: {
	    DWORD res, src1, src2;
	    src1 = EAX; src2 = FETCH_QUAD((PC+1));
	    EAX = res = src1 ^ src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case SEGss:
	    OVERRIDE = LONG_SS;
	    PC+=1; goto override;
	case AAA:
	    if (((unsigned int)( EAX & 0x0f ) > 9 ) || (IS_AF_SET)) {
		AL += 6;
		AH += 1; 
		SET_CF;
		SET_AF;
	        AL &= 0x0f;
	    } else {
		CLEAR_CF;
		CLEAR_AF;
	    }
	    PC += 1; return (PC);
	case CMPwfrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case CMPwtrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case CMPwi: {
	    DWORD res, src1;
	    long src2;
	    src1 = EAX; 
	    src2 = FETCH_QUAD((PC+1));
	    res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case SEGds:
	    OVERRIDE = LONG_DS;
	    PC+=1; goto override;
	case AAS:
	    if (((unsigned int)( EAX & 0x0f ) > 9 ) || (IS_AF_SET)) {
		AL -= 6;
		AH -= 1;
		SET_CF;
		SET_AF;
	        AL &= 0x0f;
	    } else {
		CLEAR_CF;
		CLEAR_AF;
	    }
	    PC += 1; return (PC);
	case INCax: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = EAX; 
	    EAX = res = src1 + 1;
	    src2 = 1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCcx: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = ECX; src2 = 1;
	    ECX = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCdx: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = EDX; src2 = 1;
	    EDX = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCbx: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = EBX; src2 = 1;
	    EBX = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCsp: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = ESP; src2 = 1;
	    ESP = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCbp: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = EBP; src2 = 1;
	    EBP = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCsi: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = ESI; src2 = 1;
	    ESI = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case INCdi: {
	    unsigned int res, src1, src2;
	    int carry;
	    src1 = EDI; src2 = 1;
	    EDI = res = src1 + src2;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);

	case DECax: {
	    long res, src1, src2;
	    int carry;
	    src1 = EAX; src2 = 1;
	    EAX = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECcx: {
	    long res, src1, src2;
	    int carry;
	    src1 = ECX; src2 = 1;
	    ECX = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECdx: {
	    long res, src1, src2;
	    int carry;
	    src1 = EDX; src2 = 1;
	    EDX = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECbx: {
	    long res, src1, src2;
	    int carry;
	    src1 = EBX; src2 = 1;
	    EBX = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECsp: {
	    long res, src1, src2;
	    int carry;
	    src1 = ESP; src2 = 1;
	    ESP = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECbp: {
	    long res, src1, src2;
	    int carry;
	    src1 = EBP; src2 = 1;
	    EBP = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECsi: {
	    long res, src1, src2;
	    int carry;
	    src1 = ESI; src2 = 1;
	    ESI = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);
	case DECdi: {
	    long res, src1, src2;
	    int carry;
	    src1 = EDI; src2 = 1;
	    EDI = res = src1 - src2;
	    src2 = ((src2==0x80000000)? 0:-src2);
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
	    } PC += 1; return (PC);

	case PUSHax: {
	    unsigned long temp = EAX;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHcx: {
	    unsigned long temp = ECX;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHdx: {
	    unsigned long temp = EDX;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHbx: {
	    unsigned long temp = EBX;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHsp: {
	    unsigned long temp = ESP;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHbp: {
	    unsigned long temp = EBP;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHsi: {
	    unsigned long temp = ESI;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case PUSHdi: {
	    unsigned long temp = EDI;
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case POPax: POPQUAD(EAX); PC += 1; return (PC);
	case POPcx: POPQUAD(ECX); PC += 1; return (PC);
	case POPdx: POPQUAD(EDX); PC += 1; return (PC);
	case POPbx: POPQUAD(EBX); PC += 1; return (PC);
	case POPsp: POPQUAD(ESP); PC += 1; return (PC);
	case POPbp: POPQUAD(EBP); PC += 1; return (PC);
	case POPsi: POPQUAD(ESI); PC += 1; return (PC);
	case POPdi: POPQUAD(EDI); PC += 1; return (PC);
	case PUSHA: {
	    unsigned long temp;
	    unsigned long tempsp = ESP;
	    temp = EAX; PUSHQUAD(temp);
	    temp = ECX; PUSHQUAD(temp);
	    temp = EDX; PUSHQUAD(temp);
	    temp = EBX; PUSHQUAD(temp);
	    PUSHQUAD(tempsp);
	    tempsp = EBP;
	    PUSHQUAD(tempsp);
	    temp = ESI; PUSHQUAD(temp);
	    temp = EDI; PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case POPA: {
	    unsigned long temp;
	    POPQUAD(EDI);
	    POPQUAD(ESI);
	    POPQUAD(temp);
	    EBP = temp;
	    ESP += 4;
	    POPQUAD(EBX);
	    POPQUAD(EDX);
	    POPQUAD(ECX);
	    POPQUAD(EAX);
	    } PC += 1; return (PC);
	case BOUND: 
	case ARPL:
	    PC += 1; return (PC);    
	case SEGfs:
	    OVERRIDE = LONG_FS;
	    PC+=1; goto override;
	case SEGgs:
	    OVERRIDE = LONG_GS;
	    PC+=1; goto override;
	case OPERoverride:	/* 0x66: 32 bit operand, 16 bit addressing */
	    if (data32)
		return (hsw_interp_16_16 (env, P0, PC+1, interp_var, err));
	    PC += 1;
	    goto override;
	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
	    if (!code32)
		return (hsw_interp_32_32 (env, P0, PC+1, interp_var, err));
	    PC += 1;
	    goto override;
	case PUSHwi: {
	    unsigned char *sp = LONG_SS + SP;
	    *(sp - 1) = *(PC + 4);
	    *(sp - 2) = *(PC + 3);
	    *(sp - 3) = *(PC + 2);
	    *(sp - 4) = *(PC + 1);
	    SP -= 4;
	    } PC += 5; return (PC);
        case IMULwrm: {
#ifdef DOSEMU
	    int64_t res, mlt;
	    unsigned char *mem_ref;
	    unsigned long himr;
	    PC += hsw_modrm_16_quad (env, PC, interp_var);
	    res = *EREG1; mem_ref = MEM_REF;
	    if (IS_MODE_REG)
		mlt = FETCH_EREG(mem_ref);
	    else
		mlt = FETCH_QUAD(mem_ref);
	    res *= mlt;
	    *EREG1 = (u_int32_t)res;
	    himr = (u_int32_t)((res<0? -res:res)>>32);
#else
            MULT resm, src1m, src2m, mulr[4];
            unsigned char *mem_ref, sg1=1, sg2=1;
	    unsigned long himr;
            PC += hsw_modrm_16_quad(env,PC,interp_var);
            src2m.longw = FETCH_QUAD(PC); PC += 4; mem_ref = MEM_REF;
            if (IS_MODE_REG)
                 src1m.longw = FETCH_EREG(mem_ref);
            else
                 src1m.longw = FETCH_QUAD(mem_ref);
            if(src1m.longw<0) { src1m.longw = -src1m.longw; sg1 = -1; }
            if(src2m.longw<0) { src2m.longw = -src2m.longw; sg2 = -1; }
            mulr[0].longw = src1m.word.jnr * src2m.word.jnr;
            mulr[1].longw = src1m.word.jnr * src2m.word.snjr;
            mulr[2].longw = src1m.word.snjr * src2m.word.jnr;
            mulr[3].longw = src1m.word.snjr * src2m.word.snjr;
            src1m.longw = mulr[0].word.snjr + mulr[1].word.jnr + mulr[2].word.jnr;
            himr = src2m.longw = mulr[1].word.snjr + mulr[2].word.snjr + mulr[3].longw + src1m.word.snjr;
            resm.word.jnr = mulr[0].word.jnr;
            resm.word.snjr = src1m.word.jnr;
            *EREG1 = ((sg1==sg2)? resm.longw: -resm.longw);
#endif
            RES_32 = 0;
            if (himr) { SRC1_16 = SRC2_16 = 0x8000;
                CARRY = 1; }
            else SRC1_16 = SRC2_16 = 0;
            } return(PC);
	case PUSHbi: {
	    unsigned char *sp = LONG_SS + SP;
	    long temp = (signed char)*(PC+1);
	    *(sp - 4) = temp;
	    temp = temp >> 8;
	    *(sp - 3) = temp;
	    *(sp - 2) = temp;
	    *(sp - 1) = temp;
	    SP -= 4;
	    } PC +=2; return (PC);
	case IMMEDwrm: {
	    DWORD src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = FETCH_QUAD(PC); PC += 4;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_EREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS;
		    return (PC);
		case 1: /* OR */
		    *(DWORD *)mem_ref = res = src1 | src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 4: /* AND */
		    *(DWORD *)mem_ref = res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 5: /* SUB */
		    *(DWORD *)mem_ref = res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 6: /* XOR */
		    *(DWORD *)mem_ref = res = src1 ^ src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_QUAD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS;
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    PUT_QUAD(mem_ref, res);
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    PUT_QUAD(mem_ref, res);
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    PUT_QUAD(mem_ref, res);
		    src1 = src2 =res;
		    SETDFLAGS;
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
		    PUT_QUAD(mem_ref, res);
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
		    PUT_QUAD(mem_ref, res);
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
	      }
	    }}
	case IMMEDisrm: {
	    DWORD src1, src2, res; unsigned char *mem_ref;
	    signed long temp;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    temp = *PC; PC += 1;
	    temp = ((temp<<24)>>24);
	    src2 = temp;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_EREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS;
		    return (PC);
		case 1: /* OR */
		    *(DWORD *)mem_ref = res = src1 | src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 + src2;
		    SETDFLAGS;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(DWORD *)mem_ref = res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 4: /* AND */
		    *(DWORD *)mem_ref = res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 5: /* SUB */
		    *(DWORD *)mem_ref = res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 6: /* XOR */
		    *(DWORD *)mem_ref = res = src1 ^ src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
		    src2 = - src2;
		    SETDFLAGS;
		    return (PC);
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_QUAD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 1: /* OR */
		    res = src1 | src2;
		    src1 = src2 = res;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 4: /* AND */
		    res = src1 & src2;
		    src1 = src2 = res;
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 5: /* SUB */
		    res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
	      }
	    }}
	case TESTwrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
		res = src1 & src2;
	    } else {
		src1 = FETCH_QUAD(mem_ref);
		res = src1 & src2;
	    }
	    src1 = src2 = res;
	    SETDFLAGS;
	    } return (PC); 
	case XCHGwrm: {
	    unsigned long temp; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		temp = FETCH_EREG(mem_ref);
		*(unsigned long *)mem_ref = *EREG1;
		*EREG1 = temp;
		return (PC); 
	    } else {
		unsigned long temp1 = FETCH_QUAD(mem_ref);
		temp = *EREG1; *EREG1 = temp1;
		PUT_QUAD(mem_ref, temp);
		return (PC); 
	    }
	    }

	case MOVwfrm:
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		*(unsigned long *)MEM_REF = *EREG1;
		return (PC); 
	    } else {
		unsigned long temp = *EREG1;
		unsigned char *mem_ref;
		mem_ref = MEM_REF;
		PUT_QUAD(mem_ref, temp);
		return (PC); 
	    }
	case MOVwtrm: {
	    unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if(IS_MODE_REG) 
		*EREG1 = *(DWORD *)mem_ref;
	    else
		*EREG1 = FETCH_QUAD(mem_ref);
	    } return (PC);
	case MOVsrtrm: {
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		switch (seg_reg) {
		    case 0: /* ES */
			*(unsigned long *)MEM_REF = SHORT_ES_32;
			return (PC);
		    case 1: /* CS */
			*(unsigned long *)MEM_REF = SHORT_CS_32;
			return (PC);
		    case 2: /* SS */
			*(unsigned long *)MEM_REF = SHORT_SS_32;
			return (PC);
		    case 3: /* DS */
			*(unsigned long *)MEM_REF = SHORT_DS_32;
			return (PC);
		    case 4: /* FS */
			*(unsigned long *)MEM_REF = SHORT_FS_32;
			return (PC);
		    case 5: /* GS */
			*(unsigned long *)MEM_REF = SHORT_GS_32;
			return (PC);
		    case 6: /* Illegal */
		    case 7: /* Illegal */
			/* trap this */
		goto illegal_op;
		}
	    } else {
		DWORD temp;
		unsigned char *mem_ref = MEM_REF;
		switch (seg_reg) {
		    case 0: /* ES */
			temp = SHORT_ES_32;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 1: /* CS */
			temp = SHORT_CS_32;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 2: /* SS */
			temp = SHORT_SS_32;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 3: /* DS */
			temp = SHORT_DS_32;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 4: /* FS */
			temp = SHORT_FS_32;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 5: /* GS */
			temp = SHORT_GS_32;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 6: /* Illegal */
		    case 7: /* Illegal */
			/* trap this */
		goto illegal_op;
		}
	    }}
	case LEA: {
	    int temp=0, modrm;
	    modrm = *(PC+1);
	    switch (modrm >> 6) {
		case 0:
		  switch (modrm & 7) {
		    case 0: temp = EBX + ESI; break;
		    case 1: temp = EBX + EDI; break;
		    case 2: temp = EBP + ESI; break;
		    case 3: temp = EBP + EDI; break;
		    case 4: temp = ESI; break;
		    case 5: temp = EDI; break;
		    case 6: PC += 2; temp = FETCH_QUAD(PC); PC += 2; break; 
		    case 7: temp = EBX; break;
		  } /*end switch */ PC += 2; break;
		case 1:
		  switch (modrm & 7) {
		    case 0: temp = EBX + ESI + *(signed char *)(PC+2); break;
		    case 1: temp = EBX + EDI + *(signed char *)(PC+2); break;
		    case 2: temp = EBP + ESI + *(signed char *)(PC+2); break;
		    case 3: temp = EBP + EDI + *(signed char *)(PC+2); break;
		    case 4: temp = ESI + *(signed char *)(PC+2); break;
		    case 5: temp = EDI + *(signed char *)(PC+2); break;
		    case 6: temp = EBP + *(signed char *)(PC+2); break;
		    case 7: temp = EBX + *(signed char *)(PC+2); break;
		  } /*end switch */ PC += 3; break;
		case 2:
		  switch (modrm & 7) {
		    case 0: temp = EBX + ESI + FETCH_WORD(PC+2); break;
		    case 1: temp = EBX + EDI + FETCH_WORD(PC+2); break;
		    case 2: temp = EBP + ESI + FETCH_WORD(PC+2); break;
		    case 3: temp = EBP + EDI + FETCH_WORD(PC+2); break;
		    case 4: temp = ESI + FETCH_WORD(PC+2); break;
		    case 5: temp = EDI + FETCH_WORD(PC+2); break;
		    case 6: temp = EBP + FETCH_WORD(PC+2); break;
		    case 7: temp = EBX + FETCH_WORD(PC+2); break;
		  } /*end switch */ PC += 4; break;
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
		case 0: EAX = temp; return (PC);
		case 1: ECX = temp; return (PC);
		case 2: EDX = temp; return (PC);
		case 3: EBX = temp; return (PC);
		case 4: ESP = temp; return (PC);
		case 5: EBP = temp; return (PC);
		case 6: ESI = temp; return (PC);
		case 7: EDI = temp; return (PC);
	    } /* end switch */ }
	case MOVsrfrm: {
	    unsigned long temp;
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		temp = *(unsigned long *)MEM_REF;
	    } else {
		unsigned char *mem_ref = MEM_REF;
		temp = FETCH_QUAD(mem_ref);
	    }
	    switch (seg_reg) {
		case 0: /* ES */
		    SHORT_ES_32 = temp;
		    SET_SEGREG(LONG_ES,temp);
		    return (PC);
		case 1: /* CS */
		    SHORT_CS_32 = temp;
		    SET_SEGREG(LONG_CS,temp);
		    return (PC);
		case 2: /* SS */
		    SHORT_SS_32 = temp;
		    SET_SEGREG(LONG_SS,temp);
		    return (PC);
		case 3: /* DS */
		    SHORT_DS_32 = temp;
		    SET_SEGREG(LONG_DS,temp);
		    return (PC);
		case 4: /* FS */
		    SHORT_FS_32 = temp;
		    SET_SEGREG(LONG_FS,temp);
		    return (PC);
		case 5: /* GS */
		    SHORT_GS_32 = temp;
		    SET_SEGREG(LONG_GS,temp);
		    return (PC);
		case 6: /* Illegal */
		case 7: /* Illegal */
		    /* trap this */
		goto illegal_op;
	    }}
	case POPrm: {
	    unsigned char *mem_ref, *sp;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF; sp = LONG_SS + SP;
	    *mem_ref = *sp;
	    *(mem_ref+1) = *(sp+1);
	    *(mem_ref+2) = *(sp+2);
	    *(mem_ref+3) = *(sp+3);
	    SP += 4;
	    } return (PC);

	case NOP:
	    PC += 1; return (PC);
	case XCHGcx: {
	    unsigned long temp = EAX;
	    EAX = ECX;
	    ECX = temp;
	    } PC += 1; return (PC);
	case XCHGdx: {
	    unsigned long temp = EAX;
	    EAX = EDX;
	    EDX = temp;
	    } PC += 1; return (PC);
	case XCHGbx: {
	    unsigned long temp = EAX;
	    EAX = EBX;
	    EBX = temp;
	    } PC += 1; return (PC);
	case XCHGsp: {
	    unsigned long temp = EAX;
	    EAX = ESP;
	    ESP = temp;
	    } PC += 1; return (PC);
	case XCHGbp: {
	    unsigned long temp = EAX;
	    EAX = EBP;
	    EBP = temp;
	    } PC += 1; return (PC);
	case XCHGsi: {
	    unsigned long temp = EAX;
	    EAX = ESI;
	    ESI = temp;
	    } PC += 1; return (PC);
	case XCHGdi: {
	    unsigned long temp = EAX;
	    EAX = EDI;
	    EDI = temp;
	    } PC += 1; return (PC);
	case CBW: /* CWDE */
	    env->rax.x.dummy = ((AX & 0x8000) ? 0xffff : 0);
	    PC += 1; return (PC);
	case CWD: /* CDQ */
            EDX = (EAX & 0x80000000) ? 0xffffffff : 0;
            PC += 1; return(PC);
	    return(PC);
	case CALLl: {
/*	    unsigned int cs = SHORT_CS_16;
	    unsigned int ip = PC - LONG_CS + 5;
	    unsigned short transfer_magic;
	    PUSHQUAD(cs);
	    PUSHQUAD(ip);
	    env->return_addr = (cs << 16)|ip;
	    ip = FETCH_QUAD(PC+1);
	    cs = FETCH_QUAD(PC+3);
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
		SET_SEGREG(LONG_CS,SHORT_CS_16);
		SET_SEGREG(LONG_DS,SHORT_DS_16);
		SET_SEGREG(LONG_ES,SHORT_ES_16);
		SET_SEGREG(LONG_SS,SHORT_SS_16);
		EBP = EBP - (long)LONG_SS;
		ESP = ESP - (long)LONG_SS;
		PC += 5; return (PC);
	    }
	    if (transfer_magic == TRANSFER_RETURN) {
		SHORT_CS_16 = cs;
		env->return_addr = (cs << 16) | ip;
		trans_interp_flags(env, interp_var);    
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
		return;
	    }
	    invoke_data(env);    ------- TRANSFER_DATA or garbage 
	*/ }
	    fprintf(stderr," CALLl opsize = 32 \n");
	    goto not_implemented;
	case PUSHF: {
	    DWORD temp;
#ifdef DOSEMU
	    if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK))
		goto not_permitted;
#endif
	    temp =     trans_interp_flags(env, interp_var);    
	    PUSHQUAD(temp);
	    } PC += 1; return (PC);
	case POPF: {
	    unsigned long temp;
#ifdef DOSEMU
	    if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK))
		goto not_permitted;
#endif
	    POPQUAD(temp);
	    trans_flags_to_interp(env, interp_var, temp);
	    } PC += 1; return (PC);
	case MOVmal: 
	    return(PC);
	case MOVmax: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    EAX = FETCH_QUAD(mem_ref);
	    } PC += 3; return (PC);
	case MOValm: 
	    return(PC);
	case MOVaxm: {
	    unsigned char *mem_ref;
	    DWORD temp = EAX;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    PUT_QUAD(mem_ref, temp);
	    } PC += 3; return (PC);
	case MOVSw: {
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    *dest = *src;
	    *(dest+1) = *(src+1);
	    *(dest+2) = *(src+2);
	    *(dest+3) = *(src+3);
	    (env->flags & DIRECTION_FLAG)?(SI-=4,DI-=4):(SI+=4,DI+=4);
	    } PC += 1; return (PC);
	case CMPSw: {
	    DWORD res, src1, src2;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    src1 = FETCH_QUAD(src);
	    src2 = FETCH_QUAD(dest);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(SI-=4,DI-=4):(SI+=4,DI+=4);
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } PC += 1; return (PC);
	case TESTwi: {
	    DWORD res, src1, src2;
	    src1 = FETCH_QUAD((PC+1));
	    src2 = EAX; res = src1 & src2;
	    src1 = src2 = res;
	    SETDFLAGS;
	    } PC += 5; return (PC);
	case STOSw: /* STOSD */
	    LONG_ES[DI] = AL;
	    LONG_ES[DI+1] = AH;
	    LONG_ES[DI+2] = (EAX >> 16) & 0xff;
	    LONG_ES[DI+3] = EAX >> 24;
	    (env->flags & DIRECTION_FLAG)?(DI-=4):(DI+=4);
	    PC += 1; return (PC);
	case LODSw: {
	    unsigned char *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    EAX = FETCH_QUAD(seg);
	    (env->flags & DIRECTION_FLAG)?(SI-=4):(SI+=4);
	    } PC += 1; return (PC);
	case SCASw: {
	    DWORD res, src1;
	    long src2;
	    unsigned char *mem_ref;
	    src1 = EAX;
	    mem_ref = LONG_ES + (DI);
	    src2 = FETCH_QUAD(mem_ref);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(DI-=4):(DI+=4);
	    src2 = ((src2==0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } PC += 1; return (PC);
	case MOViax:
	    EAX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
	case MOVicx:
	    ECX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
	case MOVidx:
	    EDX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
	case MOVibx:
	    EBX = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
	case MOVisp:
	    ESP = FETCH_QUAD((PC+1)); 
	    PC += 5; return (PC);
	case MOVibp:
	    EBP = FETCH_QUAD((PC+1)); 
	    PC += 5; return (PC);
	case MOVisi:
	    ESI = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
	case MOVidi:
	    EDI = FETCH_QUAD((PC+1));
	    PC += 5; return (PC);
	case SHIFTwi: {
	    int count; unsigned char *mem_ref;
	    int temp;
	    DWORD res, src1, src2;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    count = *PC & 0x1f;  
	    PC += 1;
	    if (count) {
	      if (IS_MODE_REG) {
		unsigned long *reg = (unsigned long *)MEM_REF;
		switch (res) {
		    case 0: /* ROL */
			res = *reg;
	/*		temp = FETCH_QUAD(mem_ref); */
			src1 = src2 = res;
			res = (res << count) | (res >> (32 - count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1; /* CF */
			goto log_rot;
		    case 1: /* ROR */
			res = *reg;
			src1 = src2 = res;
			res = (res >> count) | (res << (32-count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1;
			goto log_rot;
		    case 2: /* RCL */
			temp = CARRY;
			res = *reg;
			src1 = src2 = res;
			res = (res << count) | (res >> (33 - count)
			    | (temp & 1) << (count - 1));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = ((res>>(32-count)) & 0x1);
			goto log_rot;
		    case 3: /* RCR */
			temp = CARRY;
			res = *reg;
			src1 = src2 = res;
			res = (res >> count) | (res << (33 - count)
			    | (temp & 1) << (32 - count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = (res>>(count-1)) & 0x1;
			goto log_rot;
		    case 4: /* SHL/SAL */
			res = *reg;
			src1 = src2 = res;
			res = (res << count);
			*reg = res;
			CARRY = (res >> (32-count)) & 0x1;
			return (PC);
		    case 5: /* SHR */
			res = *reg;
			src1 = src2 = res;
			res = res >> count;
			*reg = res;
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op;
		    case 7: /* SAR */
			res = *(signed long *)reg;
			src1 = src2 = res;
			res = (res >> count);
			*reg = res;
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		}
	      } else {
		unsigned char *mem_ref = MEM_REF;
		switch (res) {
		    case 0: /* ROL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count) | (res >> (32 - count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1; /* CF */
			goto log_rot;
		    case 1: /* ROR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res >> count) | (res << (32-count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1;
			goto log_rot;
		    case 2: /* RCL */
			temp = CARRY;
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count) | (res >> (33 - count)
			    | (temp & 1) << (count - 1));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = ((res>>(32-count)) & 0x1);
			goto log_rot;
		    case 3: /* RCR */
			temp = CARRY;
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res >> count) | (res << (33 - count)
			    | (temp & 1) << (32 - count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = (res>>(count-1)) & 0x1;
			goto log_rot;
		    case 4: /* SHL/SAL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count);
			PUT_QUAD(mem_ref,res);
			CARRY = (res >> (32-count)) & 0x1;
			return (PC);
		    case 5: /* SHR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = res >> count;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op;
		    case 7: { /* SAR */
			signed long res1;
			res1 = FETCH_QUAD(mem_ref);
			src1 = src2 = res1;
			res1 = (res1 >> count);
			res = res1;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
			}
		    }
	      }  } 
	      else  return (PC);
	    }
/*	case RETisp: {
	    unsigned long ip;
	    POPQUAD(ip);
	    ESP += (signed short)(FETCH_WORD((PC+1)));
	    PC = LONG_CS + ip;
	    } return (PC);
	case RET: {
	    unsigned long ip;
	    POPQUAD(ip);
	    PC = LONG_CS + ip;
	    } return (PC);
*/
	case LES: {
	    DWORD temp; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op;
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_QUAD(mem_ref);
	    *EREG1 = temp;
	    temp = FETCH_QUAD(mem_ref+2);
	    SHORT_ES_32 = temp;
	    SET_SEGREG(LONG_ES,temp);
	    } return (PC);
	case LDS: {
	    int temp; unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op;
		return (PC);
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_QUAD(mem_ref);
	    *EREG1 = temp;
	    temp = FETCH_QUAD(mem_ref+2);
	    SHORT_DS_32 = temp;
	    SET_SEGREG(LONG_DS,temp);
	    } return (PC);
	case MOVwirm: {
	    /*int temp;*/ unsigned char *mem_ref;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		*(unsigned long *)mem_ref = FETCH_QUAD(PC);
		PC += 4; return (PC);
	    } else {
		*mem_ref = *PC;
		*(mem_ref+1)= *(PC+1);
		*(mem_ref+2)= *(PC+2);
		*(mem_ref+3)= *(PC+3);
		PC += 4; return (PC);
	    } } 
	case ENTER: {
	    unsigned char *sp, *bp, *ss;
	    unsigned long temp;
	    unsigned char level = *(PC+3) & 0x1f;
	    sp = LONG_SS + ESP;
	    bp = LONG_SS + EBP;
	    ss = LONG_SS;
	/*  push(EBP) */
	    temp = bp - ss;
	    *(sp-4) = temp & 0xff;
	    *(sp-3) = (temp >> 8) & 0xff;
	    *(sp-2) = (temp >> 16) & 0xff;
	    *(sp-1) = (temp >> 24) & 0xff;
	    sp -= 4;
	    temp = sp - ss; /* frame-ptr */
	    if(level){
	      while (level--) {
	/* push(EBP) */
		*(sp-4) = *(bp-4); *(sp-3) = *(bp-3);
		*(sp-2) = *(bp-2); *(sp-1) = *(bp-1);
		bp -= 4; sp -= 4;
	      }
	/* push(frame-ptr) */
	    *(sp - 4) = temp & 0xff; *(sp - 3) = (temp >> 8) & 0xff;
	    *(sp - 2) = (temp >> 16) & 0xff; *(sp - 1) = (temp >> 24) & 0xff;
	    }
	    EBP = temp;
	    sp = ss + (temp - (FETCH_WORD((PC + 1))));
	    ESP = sp - LONG_SS;
	    } PC += 4; return (PC);
	case LEAVE: {   /* 0xc9 */
	    unsigned long temp;
	    ESP = EBP;
	    POPQUAD(temp);
	    EBP = temp;
	    } PC += 1; return (PC);
	case RETlisp: /* {
	    unsigned int cs, ip;
	    unsigned char *sp = LONG_SS + ESP;
	    ip = FETCH_QUAD(sp);
	    cs = FETCH_QUAD(sp+4);
	    ESP = ((sp + 8) + FETCH_WORD((PC+1))) - LONG_SS;
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		return (PC);
	    }  else {
		env->return_addr = ip | (cs << 16);
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
		return(0);
	    }
	    } */
	    fprintf(stderr," RETl opsize = 32 \n");
	    goto not_implemented;
	case RETl: /* {
	    unsigned int cs, ip;
	    unsigned char *sp = LONG_SS + ESP;
	    ip = FETCH_QUAD(sp);
	    cs = FETCH_QUAD(sp+2);
	    ESP += 8;
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		return (PC);
	    } else {
		env->return_addr = ip | (cs << 16);
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
		return(0);
	    }
	    } */
	    fprintf(stderr," RETl opsize = 32 \n");
	    goto not_implemented; 
	case INT3: 
	    fprintf(stderr," INT3l opsize = 32 \n");
	    goto not_implemented;
	case INT: /* {
	    unsigned int temp, cs, ip = (unsigned int)(PC - LONG_CS);
	    PUSHQUAD(ip);
	    cs = SHORT_CS_16;
	    PUSHQUAD(cs);
	    env->return_addr = (cs << 16)|ip;
	    temp =     trans_interp_flags(env, interp_var);    
	    PUSHQUAD(temp);
	    EBP = EBP + (long)LONG_SS;
	    ESP = ESP + (long)LONG_SS;
	    INT_handler((unsigned int)(*(PC+1)),env);
	    EBP = EBP - (long)LONG_SS;
	    ESP = ESP - (long)LONG_SS;
	    trans_flags_to_interp(env, interp_var, env->flags);
	    } PC += 2; return (PC);
	    */
	fprintf(stderr," INTl opsize = 32 \n");
	goto not_implemented;
	case INTO:
	fprintf(stderr," INTOl opsize = 32 \n");
	goto not_implemented;
	case IRET: /* {
	    unsigned int cs, ip, flags;
	    unsigned char *sp = LONG_SS + ESP;
	    ip = *(sp) | (*(sp + 1) >> 8);
	    cs = *(sp + 2) | (*(sp + 3) >> 8);
	    flags = *(sp + 4) | (*(sp + 5) >> 8);
	    ESP += 6;
	    trans_flags_to_interp(env, interp_var, flags);
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		return (PC);
	    } else {
		env->return_addr = ip | (cs << 16);
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
		return;
	    }
	    } */
	fprintf(stderr," IRETl opsize = 32 \n");
	goto not_implemented;
	case SHIFTw: {
	    int count=1; unsigned char *mem_ref;
	    int temp;
	    DWORD res, src1, src2;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	      if (IS_MODE_REG) {
		unsigned long *reg = (unsigned long *)MEM_REF;
		switch (res) {
		    case 0: /* ROL */
			res = *reg;
	/*		temp = FETCH_QUAD(mem_ref); */
			src1 = src2 = res;
			res = (res << count) | (res >> (32 - count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1; /* CF */
			goto log_rot;
		    case 1: /* ROR */
			res = *reg;
			src1 = src2 = res;
			res = (res >> count) | (res << (32-count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1;
			goto log_rot;
		    case 2: /* RCL */
			temp = CARRY;
			res = *reg;
			src1 = src2 = res;
			res = (res << count) | (res >> (33 - count)
			    | (temp & 1) << (count - 1));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = ((res>>(32-count)) & 0x1);
			goto log_rot;
		    case 3: /* RCR */
			temp = CARRY;
			res = *reg;
			src1 = src2 = res;
			res = (res >> count) | (res << (33 - count)
			    | (temp & 1) << (32 - count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = (res>>(count-1)) & 0x1;
			goto log_rot;
		    case 4: /* SHL/SAL */
			res = *reg;
			src1 = src2 = res;
			res = (res << count);
			*reg = res;
			CARRY = (res >> (32-count)) & 0x1;
			return(PC);
		    case 5: /* SHR */
			res = *reg;
			src1 = src2 = res;
			res = res >> count;
			*reg = res;
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return(PC);
		    case 6: /* Illegal */
		goto illegal_op;
		    case 7: /* SAR */
			res = *(signed long *)reg;
			src1 = src2 = res;
			res = (res >> count);
			*reg = res;
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		}
	      } else {
		unsigned char *mem_ref = MEM_REF;
		switch (res) {
		    case 0: /* ROL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count) | (res >> (32 - count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1; /* CF */
			goto log_rot;
		    case 1: /* ROR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res >> count) | (res << (32-count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1;
			goto log_rot;
		    case 2: /* RCL */
			temp = CARRY;
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count) | (res >> (33 - count)
			    | (temp & 1) << (count - 1));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = ((res>>(32-count)) & 0x1);
			goto log_rot;
		    case 3: /* RCR */
			temp = CARRY;
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res >> count) | (res << (33 - count)
			    | (temp & 1) << (32 - count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = (res>>(count-1)) & 0x1;
			goto log_rot;
		    case 4: /* SHL/SAL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count);
			PUT_QUAD(mem_ref,res);
			CARRY = (res >> (32-count)) & 0x1;
			return (PC);
		    case 5: /* SHR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = res >> count;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op;
		    case 7: { /* SAR */
			signed long res1;
			res1 = FETCH_QUAD(mem_ref);
			src1 = src2 = res1;
			res1 = (res1 >> count);
			res = res1;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
			}
		    }
		}
	    }
	case SHIFTwv: {
	    int count; unsigned char *mem_ref;
	    int temp;
	    DWORD res, src1, src2;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    count = ECX & 0x1f;
	    if(count) {
	      if (IS_MODE_REG) {
		unsigned long *reg = (unsigned long *)MEM_REF;
		switch (res) {
		    case 0: /* ROL */
			res = *reg;
	/*		temp = FETCH_QUAD(mem_ref); */
			src1 = src2 = res;
			res = (res << count) | (res >> (32 - count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1; /* CF */
			goto log_rot;
		    case 1: /* ROR */
			res = *reg;
			src1 = src2 = res;
			res = (res >> count) | (res << (32-count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1;
			goto log_rot;
		    case 2: /* RCL */
			temp = CARRY;
			res = *reg;
			src1 = src2 = res;
			res = (res << count) | (res >> (33 - count)
			    | (temp & 1) << (count - 1));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = ((res>>(32-count)) & 0x1);
			goto log_rot;
		    case 3: /* RCR */
			temp = CARRY;
			res = *reg;
			src1 = src2 = res;
			res = (res >> count) | (res << (33 - count)
			    | (temp & 1) << (32 - count));
			*reg = res;
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = (res>>(count-1)) & 0x1;
			goto log_rot;
		    case 4: /* SHL/SAL */
			res = *reg;
			src1 = src2 = res;
			res = (res << count);
			*reg = res;
			CARRY = (res >> (32-count)) & 0x1;
			return (PC);
		    case 5: /* SHR */
			res = *reg;
			src1 = src2 = res;
			res = res >> count;
			*reg = res;
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op;
		    case 7: /* SAR */
			res = *(signed long *)reg;
			src1 = src2 = res;
			res = (res >> count);
			*reg = res;
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		}
	      } else {
		unsigned char *mem_ref = MEM_REF;
		switch (res) {
		    case 0: /* ROL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count) | (res >> (32 - count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1; /* CF */
			goto log_rot;
		    case 1: /* ROR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res >> count) | (res << (32-count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = res & 0x1;
			goto log_rot;
		    case 2: /* RCL */
			temp = CARRY;
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count) | (res >> (33 - count)
			    | (temp & 1) << (count - 1));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = ((res>>(32-count)) & 0x1);
			goto log_rot;
		    case 3: /* RCR */
			temp = CARRY;
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res >> count) | (res << (33 - count)
			    | (temp & 1) << (32 - count));
			PUT_QUAD(mem_ref,res);
			trans_interp_flags(env,interp_var);
			SETDFLAGS;
			CARRY = (res>>(count-1)) & 0x1;
			goto log_rot;
		    case 4: /* SHL/SAL */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = (res << count);
			PUT_QUAD(mem_ref,res);
			CARRY = (res >> (32-count)) & 0x1;
			return (PC);
		    case 5: /* SHR */
			res = FETCH_QUAD(mem_ref);
			src1 = src2 = res;
			res = res >> count;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
		    case 6: /* Illegal */
		goto illegal_op;
		    case 7: { /* SAR */
			signed long res1;
			res1 = FETCH_QUAD(mem_ref);
			src1 = src2 = res1;
			res1 = (res1 >> count);
			res = res1;
			PUT_QUAD(mem_ref,res);
			SETDFLAGS;
			CARRY = (res >> (count-1)) & 1;
			return (PC);
			}
	      } } } else  return (PC);
	    }
	case 0xd6:    /* illegal on 8086 and 80x86*/
#ifdef DOSEMU
	    goto illegal_op;
#else
	    fprintf(stderr,illegal_msg,PC,SHORT_CS_16,PC-LONG_CS);
	    exit(1);
	    PC += 1; return (PC);    
#endif
	case ESC0: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg0[funct](MEM_REF);
	    else hsw_fp87_mem0[funct](MEM_REF);
	    } return (PC);
	case ESC1: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg1[funct](MEM_REF);
	    else hsw_fp87_mem1[funct](MEM_REF);
	    } return (PC);
	case ESC2: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg2[funct](MEM_REF);
	    else hsw_fp87_mem2[funct](MEM_REF);
	    } return (PC);
	case ESC3: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg3[funct](MEM_REF);
	    else hsw_fp87_mem3[funct](MEM_REF);
	    } return (PC);
	case ESC4: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg4[funct](MEM_REF);
	    else hsw_fp87_mem4[funct](MEM_REF);
	    } return (PC);
	case ESC5: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg5[funct](MEM_REF);
	    else hsw_fp87_mem5[funct](MEM_REF);
	    } return (PC);
	case ESC6: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg6[funct](MEM_REF);
	    else hsw_fp87_mem6[funct](MEM_REF);
	    } return (PC);
	case ESC7: {
	    int funct = (*(PC+1) & 0x38);
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg7[funct](MEM_REF);
	    else hsw_fp87_mem7[funct](MEM_REF);
	    } return (PC);

	case LOOPNZ_LOOPNE: 
	    if ((--ECX != 0) && (!IS_ZF_SET)) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
	case LOOPZ_LOOPE: 
	    if ((--ECX != 0) && (IS_ZF_SET)) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
	case LOOP: 
	    if (--ECX != 0) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
	case JCXZ: 
	    if (ECX == 0) {
		JUMP((PC+1)); return (PC);
	    } PC += 2; return (PC);
#ifdef DOSEMU
	case INw: {
	      DWORD a = *(PC+1);
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((a&1)||(!test_bit(a+1, io_bitmap))) goto not_permitted;
	      }
	      AX = PortIO ((DWORD) *(++PC), 0, 32, FALSE);
	    }
	    PC += 1;
	    return (PC);
	case OUTw: {
	      DWORD a = *(PC+1);
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((a&1)||(!test_bit(a+1, io_bitmap))) goto not_permitted;
	      }
	      PortIO ((DWORD) *(++PC), AX, 32, TRUE);
	    }
	    PC += 1;
	    return (PC);
	case INvw: {
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((DX>0x3ff)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
	      }
	      AX = PortIO (DX, 0, 32, FALSE);
	    }
	    PC += 1;
	    return (PC);
	case OUTvw: {
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((DX>0x3ff)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
	      }
	      PortIO (DX, AX, 32, TRUE);
	    }
	    PC += 1;
	    return (PC);
#endif 
	case CALLd: /* {
	    unsigned int ip = PC - LONG_CS + 3;
	    PUSHQUAD(ip);
	    PC = LONG_CS + (ip + (signed short)(FETCH_QUAD((PC+1)))&0xffff);
	    } return (PC); */ goto illegal_op;
	case JMPd: /* {
	    unsigned int ip = PC - LONG_CS + 3;
	    PC = LONG_CS + (ip + (signed short)(FETCH_QUAD((PC+1)))&0xffff);
	    } return (PC); */ goto illegal_op;
	case JMPld: /* {
	    unsigned int cs, ip;
	    ip = FETCH_QUAD(PC+1);
	    cs = FETCH_QUAD(PC+3);
	    SHORT_CS_16 = cs;
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		return (PC);
	    }
	    env->return_addr = (cs << 16) | ip;
	    trans_interp_flags(env, interp_var);
	    EBP = EBP + (long)LONG_SS;
	    ESP = ESP + (long)LONG_SS;
	    } return; */ goto illegal_op;
	case JMPsid: 
	    JUMP((PC+1)); return (PC);
	case REPNE: {
	    int count = ECX;

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
printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2), EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, decode(*(PC+1), *(PC+2)), instr_count);
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
		case MOVSw: { /* MOVSD */
		    unsigned char *src, *dest;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    count = count << 2;
		    if (env->flags & DIRECTION_FLAG) {
			DI -= count; SI -= count; ECX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
            		    *(dest+2) = *(src+2);
            		    *(dest+3) = *(src+3);
			    dest -= 4; src -= 4; count -= 4;
			} return (PC);
		    } else {
			DI += count; SI += count; ECX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
            		    *(dest+2) = *(src+2);
            		    *(dest+3) = *(src+3);
			    dest += 4; src += 4; count -= 4;
			} return (PC);
		    } }
		case CMPSw: {
		    unsigned char *src, *dest;
		    unsigned long res, src1=0;
		    long src2=0;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_QUAD(src);
			    src2 = FETCH_QUAD(dest);
			    src -= 4; dest -=4;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = FETCH_QUAD(src);
			    src2 = FETCH_QUAD(dest);
			    src += 4; dest += 4;
			    if (src1 != src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1; RES_32 = res; 
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    ECX = 0;
		    DI = dest - LONG_ES;
		    SI = src - ALLOW_OVERRIDE(LONG_DS);
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    } return (PC);
		case STOSw: {
		    unsigned char al, ah,eal,eah;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + DI;
		    al = AL; ah = AH;
		    eal = (EAX >> 16) & 0xff;
		    eah = EAX >> 24;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count<<2; ECX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    *(dest+2) = eal;
			    *(dest+3) = eah;
			    dest -= 4;
			} return (PC);
		    } else {		      /* forwards */
			DI += count<<2; ECX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    *(dest+2) = eal;
			    *(dest+3) = eah;
			    dest += 4;
			} return (PC);
		    } }
		case LODSw: {
		    unsigned char *src;
		    DWORD temp;
		    count = count << 2;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; SI -= count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; SI += count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } }
		case SCASw: {
		    unsigned char *dest;
		    unsigned long res, src1;
		    long src2;
		    if (count == 0) return (PC);
		    dest = LONG_ES + DI;
		    src1 = EAX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_QUAD(dest);
			    dest -=4;
			    if (src1 != src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = FETCH_QUAD(dest);
			    dest -=4;
			    if (src1 != src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    ECX = 0; DI = dest - LONG_ES;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
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
	    int count = ECX;

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
printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2), EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, decode(*(PC+1), *(PC+2)), instr_count);
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
		case MOVSw: {
		    unsigned char *src, *dest;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    count = count << 2;
		    if (env->flags & DIRECTION_FLAG) {
			DI -= count; SI -= count; ECX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    *(dest+2) = *(src+2);
			    *(dest+3) = *(src+3);
			    dest -= 4; src -= 4; count -= 4;
			} return (PC);
		    } else {
			DI += count; SI += count; ECX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    *(dest+2) = *(src+2);
			    *(dest+3) = *(src+3);
			    dest += 4; src += 4; count -= 4;
			} return (PC);
		    } }
		case CMPSw: {
		    unsigned char *src, *dest;
		    unsigned long res, src1=0;
		    long src2=0;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_QUAD(src);
			    src2 = FETCH_QUAD(dest);
			    src -= 4; dest -=4;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = FETCH_QUAD(src);
			    src2 = FETCH_QUAD(dest);
			    src += 4; dest +=4;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    ECX = 0;
		    DI = dest - LONG_ES;
		    SI = src - ALLOW_OVERRIDE(LONG_DS);
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
		    } return (PC);
		case STOSw: {
		    unsigned char al, ah, eal, eah;
		    unsigned char *dest;
		    if (count == 0) return (PC);
		    dest = LONG_ES + DI;
		    al = AL; ah = AH;
		    eal = (EAX >> 16) & 0xff;
		    eah = EAX >> 24;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count<<1; ECX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    *(dest+2) = eal;
			    *(dest+3) = eah;
			    dest -= 4;
			} return (PC);
		    } else {		      /* forwards */
			DI += count<<1; ECX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    *(dest+2) = eal;
			    *(dest+3) = eah;
			    dest += 4;
			} return (PC);
		    } }
		case LODSw: {
		    unsigned char *src;
		    DWORD temp;
		    count = count << 2;
		    if (count == 0) return (PC);
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; SI -= count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; SI += count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } }
		case SCASw: {
		    unsigned char *dest;
		    unsigned long res, src1;
		    long src2;
		    if (count == 0) return (PC);
		    dest = LONG_ES + DI;
		    src1 = EAX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_QUAD(dest);
			    dest -=4;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = FETCH_QUAD(dest);
			    dest -=4;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				DI = dest - LONG_ES;
	    			src2 = ((src2==0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    ECX = 0; DI = dest - LONG_ES;
	    	    src2 = ((src2==0x80000000)? 0:-src2);
		    SETDFLAGS;
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

	case CMC:
	    interp_var->flags_czsp.word16.carry ^= CARRY_FLAG;
	    PC += 1; return (PC);
	case GRP1wrm: {
	    DWORD src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_EREG(mem_ref);
		    src2 = FETCH_QUAD(PC); PC += 4;
		    res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS;
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
		    SETDFLAGS;
		    } return (PC);
		case 4: { /* MUL EAX */
#ifdef DOSEMU
		    u_int64_t res = (u_int64_t)EAX;
		    res *= (u_int64_t)FETCH_EREG (mem_ref);
		    EAX = (u_int32_t)res;
		    EDX = (u_int32_t)(res>>32);
#else
                    MULT resm, src1m, src2m, mulr[4];
                    src1m.longw = EAX;
                    src2m.longw = FETCH_EREG(mem_ref);
		    mulr[0].longw = src1m.word.jnr * src2m.word.jnr;
		    mulr[1].longw = src1m.word.jnr * src2m.word.snjr;
		    mulr[2].longw = src1m.word.snjr * src2m.word.jnr;
		    mulr[3].longw = src1m.word.snjr * src2m.word.snjr;
		    src1m.longw = mulr[0].word.snjr + mulr[1].word.jnr + mulr[2].word.jnr; /* src1.jnr - high byte of low word */
		    src2m.longw = mulr[1].word.snjr + mulr[2].word.snjr + mulr[3].longw + src1m.word.snjr; /* src2 - high word */
		    resm.word.jnr = mulr[0].word.jnr;
		    resm.word.snjr = src1m.word.jnr;
		    EAX = resm.longw;
		    EDX = src2m.longw;
#endif
		    RES_32 = 0;
		    if (EDX) { SRC1_16 = SRC2_16 = 0x8000;
			CARRY = 1; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 5: { /* IMUL EAX */
#ifdef DOSEMU
		    int64_t res = (int64_t)EAX;
		    res *= (int64_t)FETCH_EREG (mem_ref);
		    EAX = (u_int32_t)res;
		    EDX = (u_int32_t)(res>>32);
#else
                    MULT resm, src1m, src2m, mulr[4];
		    int sg1=1, sg2=1;
                    src1m.longw = EAX;
                    src2m.longw = FETCH_EREG(mem_ref);
		    if(src1m.longw<0) { src1m.longw = -src1m.longw; sg1 = -1; }
		    if(src2m.longw<0) { src2m.longw = -src2m.longw; sg2 = -1; }
		    mulr[0].longw = src1m.word.jnr * src2m.word.jnr;
		    mulr[1].longw = src1m.word.jnr * src2m.word.snjr;
		    mulr[2].longw = src1m.word.snjr * src2m.word.jnr;
		    mulr[3].longw = src1m.word.snjr * src2m.word.snjr;
		    src1m.longw = mulr[0].word.snjr + mulr[1].word.jnr + mulr[2].word.jnr;
		    src2m.longw = mulr[1].word.snjr + mulr[2].word.snjr + mulr[3].longw + src1m.word.snjr;
		    resm.word.jnr = mulr[0].word.jnr;
		    resm.word.snjr = src1m.word.jnr;
		    EAX = resm.longw;
		    EDX = src2m.longw;
		    if(sg1 != sg2){
			EAX = ~EAX;
			sg1 = EAX & 0x80000000;
			EAX ++;
			sg2 = EAX & 0x80000000;
			EDX = ~EDX;
			EDX += (sg1 & (~sg2))>>31; /* carry from eax */
		    }
#endif
		    RES_32 = 0;
		    if((EDX == 0) || (EDX == 0xffffffff)) { SRC1_16 = SRC2_16 = 0x8000;
			CARRY = 1; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 6: { /* EDIV EDX+EAX */
		    double dividend,quotent;
		    src1 = FETCH_EREG(mem_ref);
		    if(EDX){
			dividend = C2P32*EDX+EAX;
			quotent = dividend / src1;
			EAX = floor(quotent);
			EDX = floor(dividend - (EAX * src1));
		    } else {
		        res = EAX;
		        EAX = res / src1;
		        EDX = res % src1;
		    }
		    } return (PC);
		case 7: { /* IEDIV EDX+EAX */
		    double dividend,quotent;
		    long srcs;
		    int sg,sg1=1,sg2=1;
		    srcs = FETCH_EREG(mem_ref);
		    if(srcs & 0x80000000){
			sg2 = -1;
			srcs = -srcs;
		    }
		    if(EDX){
			if(EDX & 0x80000000){
			    EAX = ~EAX;
			    sg1 = EAX & 0x80000000;
			    EAX ++;
			    sg = EAX & 0x80000000;
			    EDX = ~EDX;
			    EDX += (sg1 & (~sg))>>31; /* carry from eax */
			    sg1 = -1;
			}
			dividend = C2P32*EDX+EAX;
			quotent = dividend / srcs;
			EAX = floor(quotent);
			EDX = floor(dividend - (EAX * srcs));
			if(sg1 != sg2) EAX =-EAX;
			if(sg1<0) EDX = -EDX;
		    }
		    res = EAX;
		    EAX = res / srcs;
		    EDX = res % srcs;
		    } return (PC);
	      }
	    } else {
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_QUAD(mem_ref);
		    src2 = FETCH_QUAD(PC); PC += 4;
		    res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS;
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
		    SETDFLAGS;
		    } return (PC);
		case 4: { /* MUL EAX */
#ifdef DOSEMU
		    u_int64_t res = (u_int64_t)EAX;
		    res *= (u_int64_t)FETCH_QUAD (mem_ref);
		    EAX = (u_int32_t)res;
		    EDX = (u_int32_t)(res>>32);
#else
                    MULT resm, src1m, src2m, mulr[4];
                    src1m.longw = EAX;
                    src2m.longw = FETCH_QUAD(mem_ref);
		    mulr[0].longw = src1m.word.jnr * src2m.word.jnr;
		    mulr[1].longw = src1m.word.jnr * src2m.word.snjr;
		    mulr[2].longw = src1m.word.snjr * src2m.word.jnr;
		    mulr[3].longw = src1m.word.snjr * src2m.word.snjr;
		    src1m.longw = mulr[0].word.snjr + mulr[1].word.jnr + mulr[2].word.jnr; /* src1.jnr - high byte of low word */
		    src2m.longw = mulr[1].word.snjr + mulr[2].word.snjr + mulr[3].longw + src1m.word.snjr; /* src2 - high word */
		    resm.word.jnr = mulr[0].word.jnr;
		    resm.word.snjr = src1m.word.jnr;
		    EAX = resm.longw;
		    EDX = src2m.longw;
#endif
		    RES_32 = 0;
		    if (EDX) { SRC1_16 = SRC2_16 = 0x8000;
			CARRY = 1; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 5: { /* IMUL EAX */
#ifdef DOSEMU
		    int64_t res = (int64_t)EAX;
		    res *= (int64_t)FETCH_QUAD (mem_ref);
		    EAX = (u_int32_t)res;
		    EDX = (u_int32_t)(res>>32);
#else
                    MULT resm, src1m, src2m, mulr[4];
		    int sg1=1, sg2=1;
                    src1m.longw = EAX;
                    src2m.longw = FETCH_QUAD(mem_ref);
		    if(src1m.longw<0) { src1m.longw = -src1m.longw; sg1 = -1; }
		    if(src2m.longw<0) { src2m.longw = -src2m.longw; sg2 = -1; }
		    mulr[0].longw = src1m.word.jnr * src2m.word.jnr;
		    mulr[1].longw = src1m.word.jnr * src2m.word.snjr;
		    mulr[2].longw = src1m.word.snjr * src2m.word.jnr;
		    mulr[3].longw = src1m.word.snjr * src2m.word.snjr;
		    src1m.longw = mulr[0].word.snjr + mulr[1].word.jnr + mulr[2].word.jnr;
		    src2m.longw = mulr[1].word.snjr + mulr[2].word.snjr + mulr[3].longw + src1m.word.snjr;
		    resm.word.jnr = mulr[0].word.jnr;
		    resm.word.snjr = src1m.word.jnr;
		    EAX = resm.longw;
		    EDX = src2m.longw;
		    if(sg1 != sg2){
			EAX = ~EAX;
			sg1 = EAX & 0x80000000;
			EAX ++;
			sg2 = EAX & 0x80000000;
			EDX = ~EDX;
			EDX += (sg1 & (~sg2))>>31; /* carry from eax */
		    }
#endif
		    RES_32 = 0;
		    if((EDX == 0) || (EDX == 0xffffffff)) { SRC1_16 = SRC2_16 = 0x8000;
			CARRY = 1; }
		    else SRC1_16 = SRC2_16 = 0;
                    } return(PC);
		case 6: { /* EDIV EDX+EAX */
		    double dividend,quotent;
		    src1 = FETCH_QUAD(mem_ref);
		    if(EDX){
			dividend = C2P32*EDX+EAX;
			quotent = dividend / src1;
			EAX = floor(quotent);
			EDX = floor(dividend - (EAX * src1));
		    } else {
		        res = EAX;
		        EAX = res / src1;
		        EDX = res % src1;
		    }
		    } return (PC);
		case 7: { /* IEDIV EDX+EAX */
		    double dividend,quotent;
		    long srcs;
		    int sg,sg1=1,sg2=1;
		    srcs = FETCH_QUAD(mem_ref);
		    if(srcs & 0x80000000){
			sg2 = -1;
			srcs = -srcs;
		    }
		    if(EDX){
			if(EDX & 0x80000000){
			    EAX = ~EAX;
			    sg1 = EAX & 0x80000000;
			    EAX ++;
			    sg = EAX & 0x80000000;
			    EDX = ~EDX;
			    EDX += (sg1 & (~sg))>>31; /* carry from eax */
			    sg1 = -1;
			}
			dividend = C2P32*EDX+EAX;
			quotent = dividend / srcs;
			EAX = floor(quotent);
			EDX = floor(dividend - (EAX * srcs));
			if(sg1 != sg2) EAX =-EAX;
			if(sg1<0) EDX = -EDX;
		    }
		    res = EAX;
		    EAX = res / srcs;
		    EDX = res % srcs;
		    } return (PC);
	      } }
	      }
	case GRP2wrm: {
	    DWORD temp; unsigned char *mem_ref;
	    int carry;
	    DWORD res, src1, src2;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (temp) {
		case 0: /* INC */
		    res = FETCH_EREG(mem_ref);
		    src1 = res;
		    src2 = 1;
		    res ++;
		    *(DWORD *)mem_ref = res;
	    	    carry = CARRY;
	    	    SETDFLAGS;
	    	    CARRY = carry;
		    return (PC);
		case 1: /* DEC */
		    res = FETCH_EREG(mem_ref);
		    src1 = res; src2 = -1;
		    res --;
		    *(DWORD *)mem_ref = res;
	    	    carry = CARRY;
	    	    SETDFLAGS;
	    	    CARRY = carry;
		    return (PC);
		case 2: /* CALL indirect */ /*
		    temp = PC - LONG_CS;
		    PUSHQUAD(temp);
		    PC = LONG_CS + FETCH_EREG(mem_ref);
		    return (PC); */ goto illegal_op;
		case 3: /* CALL long indirect */ /* Illegal */
                 goto illegal_op;
		case 4: /* JMP indirect */ /*
		    PC = LONG_CS + FETCH_EREG(mem_ref);
		    return (PC); */ goto illegal_op;
		case 5: /* JMP long indirect */ /* Illegal */
                 goto illegal_op;
		case 6: /* PUSH */
		    temp = FETCH_EREG(mem_ref);
		    PUSHQUAD(temp);
		    return (PC);
		case 7: /* Illegal */
                 goto illegal_op;
		    return (PC);
	      }
	    } else {
	      switch (temp) {
		case 0: /* INC */
		    res = FETCH_QUAD(mem_ref);
		    src1 = res; src2 = 1;
		    res++;
		    PUT_QUAD(mem_ref, res);
	    	    carry = CARRY;
	    	    SETDFLAGS;
	    	    CARRY = carry;
		    return (PC);
		case 1: /* DEC */
		    res = FETCH_QUAD(mem_ref);
		    src1 = res; src2 = -1;
		    src1--;
		    PUT_QUAD(mem_ref, res);
		    SETDFLAGS;
	    	    carry = CARRY;
	    	    SETDFLAGS;
	    	    CARRY = carry;
		    return (PC);
		case 2: /* CALL indirect */
/*		    temp = PC - LONG_CS;
		    PUSHQUAD(temp);
		    temp = FETCH_QUAD(mem_ref);
		    PC = LONG_CS + temp;
		    return (PC); */
		fprintf(stderr," CALL indir opsize = 32\n");
		goto not_implemented;
		case 3: /* {  CALL long indirect 
		    unsigned int cs = SHORT_CS_16;
		    unsigned int ip = PC - LONG_CS;
		    PUSHQUAD(cs);
		    PUSHQUAD(ip);
		    env->return_addr = (cs << 16)|ip;
		    } */
		fprintf(stderr," CALL indir long opsize = 32\n");
		goto not_implemented;
		case 5: /* {  JMP long indirect 
		    unsigned int cs, ip;
		    unsigned short transfer_magic;
		    ip = FETCH_QUAD(mem_ref);
		    cs = FETCH_QUAD(mem_ref+2);
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
---------ifdef DEBUG
		if (transfer_magic == TRANSFER_CALLBACK)
		    LOGSTR((LF_DEBUG,
			"do_ext: %s\n", GetProcName(cs,ip>>3)));
		else    ------- TRANSFER_BINARY 
		    LOGSTR((LF_DEBUG,
			"do_ext: calling binary thunk %x:%x\n",cs,ip));
---------endif
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
		    invoke_data(env);    -------- TRANSFER_DATA or garbage 
		    } */
		    fprintf(stderr," JMPl indirect opsize = 32\n");
		    goto not_implemented;
		case 4: /* JMP indirect */
		    temp = FETCH_QUAD(mem_ref);
		    PC = LONG_CS + temp;
		    return (PC);
		case 6: /* PUSH */
		    temp = FETCH_QUAD(mem_ref);
		    PUSHQUAD(temp);
		    return (PC);
		case 7: /* Illegal */
                 goto illegal_op;
		    return (PC);
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
	fprintf(stderr," 32/16 nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2),
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
	fprintf(stderr," 32/16 illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2), 
                SHORT_CS_16,PC-LONG_CS,(int)PC);
#ifdef DOSEMU
	*err = EXCP06_ILLOP; return (P0);
#else
        exit(1); 
#endif
}
