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
extern int op32_print;
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

#define C2P32   4294967296.0
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
  trans_interp_flags (Interp_ENV * env, Interp_VAR * interp_var);
extern void
  trans_flags_to_interp (Interp_ENV * env, Interp_VAR * interp_var, unsigned int flags);

#ifdef DOSEMU
#  define fprintf our_fprintf
#  define our_fprintf(stream, args...) error(##args)
#endif


unsigned char *
hsw_interp_32_32(Interp_ENV *env, unsigned char *P0, unsigned char *PC,
	Interp_VAR *interp_var, int *err)
{
#ifdef DEBUG
#ifdef DOSEMU
    e_debug(env, P0, PC, interp_var);
#else
    if(op32_print || ad32_print) { printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS),*PC, *(PC+1), AX, BX, CX, DX, SI, DI, BP, SP, decode(*PC, *(PC+1)), instr_count);
	fflush(stdout);
	}
#endif
#endif
    *err = 0;
    override:
    switch (*PC) {
	case ADDbfrm:
	case ADDbtrm:
	case ORbfrm:
	case ORbtrm:
	case ADCbfrm:
	case ADCbtrm:
	case SBBbfrm:
	case SBBbtrm:
	case ANDbfrm:
	case ANDbtrm:
	case SUBbfrm:
	case SUBbtrm:
	case XORbfrm:
	case XORbtrm:
	case CMPbfrm:
	case CMPbtrm:
	case IMULbrm:
	case IMMEDbrm:
	case IMMEDbrm2:
	case TESTbrm:
	case XCHGbrm:
	case MOVbfrm:
	case MOVbtrm:
	case MOVmal:
	case MOValm:
	case MOVSb:
	case CMPSb:
	case STOSb:
	case LODSb:
	case SCASb:
	case SHIFTbi:
	case MOVbirm:
	case SHIFTb:
	case SHIFTbv:
	case ESC0:
	case ESC1:
	case ESC2:
	case ESC3:
	case ESC4:
	case ESC5:
	case ESC6:
	case ESC7:
	case GRP1brm:
	case GRP2brm:
	    return (PC);
	case ADDwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 + src2;
	    SETDFLAGS;
	    } return (PC); 
	case ORwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    src1 = src2 = res;
	    SETDFLAGS;
	    } return (PC); 
	case ORwtrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	case TwoByteESC: {
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 1: /* STR */ {
			    /* Store Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
			    PC = PC +1 + hsw_modrm_32_quad(env,PC + 1,interp_var);
                goto not_implemented;
			    return (PC);
			case 3: /* LTR */ {
			    /* Load Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
                goto not_implemented;
			    /* hsw_task_register = temp; */
			    } return (PC);
			case 4: /* VERR */ {
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_EREG(mem_ref);
			    else temp = FETCH_QUAD(mem_ref);
                goto not_implemented;
			    /* if (hsw_verr(temp) SET_ZF;
			    else CLEAR_ZF; */
			    } return (PC);
			case 5: /* VERW */ {
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
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
			    PC = PC+1+hsw_modrm_32_quad(env,PC + 1,interp_var);
			    return (PC);
		    }
		case 0x01: /* GRP7 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 1: /* SIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_QUAD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } return (PC);
			case 2: /* LGDT */ /* Privileged */
			    /* Load Global Descriptor Table Register */
                goto not_implemented;
			    PC = PC+1+hsw_modrm_32_quad(env,PC + 1,interp_var);
			    return (PC);
			case 3: /* LIDT */ /* Privileged */
			    /* Load Interrupt Descriptor Table Register */
                goto not_implemented;
			    PC = PC+1+hsw_modrm_32_quad(env,PC + 1,interp_var);
			    return (PC);
			case 4: /* SMSW */ {
			    /* Store Machine Status Word */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LIMIT field */;
			    if (IS_MODE_REG) *(DWORD *)mem_ref = temp;
			    else {PUT_QUAD(mem_ref,temp);}
			    } return (PC);
			case 5: /* Illegal */
		goto illegal_op; 
			case 6: /* LMSW */ /* Privileged */
			    /* Load Machine Status Word */
                goto not_implemented;
			    PC = PC+1+hsw_modrm_32_quad(env,PC + 1,interp_var);
			    return (PC);
			case 7: /* Illegal */
		goto illegal_op; 
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
		    /*int temp;*/ unsigned char *mem_ref;
		    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
		    mem_ref = MEM_REF;
                goto not_implemented;
		    /* what do I do here??? */
		    } return (PC);
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
		    /*int temp;*/ unsigned char *mem_ref;
		    PC += 1; PC += hsw_modrm_32_quad(env,PC,interp_var);
		    mem_ref = MEM_REF;
                goto not_implemented;
		    /* what do I do here??? */
		    } return (PC);
		case 0x90: /* SETObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x91: /* SETNObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x92: /* SETBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x93: /* SETNBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_CF_SET) ? 1 : 0;
		    } return (PC);
		case 0x94: /* SETZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x95: /* SETNZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x96: /* SETBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x97: /* SETNBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x98: /* SETSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET) ? 1 : 0;
		    } return (PC);
		case 0x99: /* SETNSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9a: /* SETPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9b: /* SETNPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_PF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9c: /* SETLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9d: /* SETNLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } return (PC);
		case 0x9e: /* SETLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } return (PC);
		case 0x9f: /* SETNLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } return (PC);
		case 0xa0: /* PUSHfs */ {
		    unsigned long temp = SHORT_FS_16;
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
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
		    DWORD count, temp, temp1, res, src1, src2;
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
		    DWORD count, temp, temp1, res, src1, src2;
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = CX & 0x1f;
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
		    res = temp; src1 = src2 = temp>>1;
		    SETDFLAGS;
		    } return (PC);
		case 0xa6: /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
		case 0xa7: /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		goto not_implemented;
		case 0xa8: /* PUSHgs */ {
		    unsigned long temp = SHORT_GS_16;
		    PUSHQUAD(temp);
		    } PC += 2; return (PC);
		case 0xa9: /* POPgs */ {
		    unsigned int temp;
		    POPQUAD(temp);
		    SET_SEGREG(LONG_GS,temp);
		    SHORT_GS_32 = temp;
		    } PC += 2; return (PC);
                case 0xab: /* BTS */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind; 
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
		    DWORD count, temp, temp1, res, src1, src2;
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = *PC & 0x1f; PC ++;
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
		    res = temp; src1 = src2 = temp <<1;
		    SETDFLAGS;
		    CARRY = carry;
		    } return (PC);
		case 0xad: /* SHRDcl */ {
		    /* Double Precision Shift Right by CL */
		    unsigned char *mem_ref, carry;
		    DWORD count, temp, temp1, res, src1, src2;
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *EREG1;
		    count = CX & 0x1f;
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
                case 0xaf: /* IMULregrm */ {
#ifdef DOSEMU
			int64_t res, mlt;
                        unsigned char *mem_ref;
                        unsigned long himr;
                        PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
                        PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
                        src1m.longw = *EREG1; mem_ref = MEM_REF;
                        if (IS_MODE_REG)
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
                        if (himr) { SRC1_16 = SRC2_16 = 0x8000;
                                CARRY = 1; }
                        else SRC1_16 = SRC2_16 = 0;
                        } return(PC);
		case 0xb2: /* LSS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
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
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
                    mem_ref = MEM_REF; ind = *EREG1;
                    if (IS_MODE_REG) {
                        ind = (ind & 0xf);
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
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    if (IS_MODE_REG) {
			/* Illegal */
		goto illegal_op; 
			return (PC);
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
		    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
		    *EREG1 = (DWORD)(temp & 0xff);
		    } return (PC);
		case 0xb7: /* MOVZXw */ {
                    DWORD temp;
                    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = FETCH_EREG(mem_ref);
                    else
                        temp = FETCH_QUAD(mem_ref);
                    *EREG1 = (DWORD)(temp & 0xffff);
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
                            PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);                                   
                            mem_ref = MEM_REF; temp = *PC;  PC++;
                            if (IS_MODE_REG)
                                temp1 = *(DWORD *)mem_ref;
                            else   
                                temp1 = FETCH_QUAD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0x1f))&1;
                    } return(PC);  
                        case 5: /* BTS */ {
                            unsigned char *mem_ref; int temp,temp1;
                            PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);                                   
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
                            PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);                                   
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
                            PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
                            ind1 = ((ind >> 5) +1) << 2;
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
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
                    DWORD temp, i;
		    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
                    *EREG1 = ((temp << 24) >> 24);
                    } return (PC);
		case 0xbf: /* MOVSXw */ {
                    signed long temp;
                    unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
                    PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
            unsigned long temp = SHORT_SS_16;
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
	    long src2;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case SBBwtrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    signed long src2;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    src2 = src2 + (CARRY & 1);
	    *EREG1 = res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
        case SBBwi: {
            DWORD res, src1;
            signed long src2; 
            src1 = EAX;     
            src2 = (FETCH_QUAD((PC+1))) + (CARRY & 1);
            EAX = res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
            SETDFLAGS; 
            } PC += 5; return (PC);
        case PUSHds: {
            unsigned short temp = SHORT_DS_16;
            PUSHQUAD(temp);
            } PC += 1; return (PC);
        case POPds: 
            POPQUAD(SHORT_DS_32);
	    SET_SEGREG(LONG_DS,SHORT_DS_32);
            PC += 1; return (PC);
	case ANDwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    PC += 1; goto override;
	case SUBwfrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case SUBwtrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    src1 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    *EREG1 = res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
        case SUBwi: {
            DWORD res, src1;
            long src2; 
            src1 = EAX;
            src2 = FETCH_QUAD((PC+1));
            EAX = res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
            if ((AL > 0x9f) || (IS_CF_SET)) {
                AL -= 0x60;
                SET_CF;
            } else CLEAR_CF;
            PC += 1; return (PC);
	case XORwfrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
            } else {
                CLEAR_CF;
                CLEAR_AF;
            }
            AL &= 0x0f;
            PC += 1; return (PC);
	case CMPwfrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    src2 = *EREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_EREG(mem_ref);
	    } else {
		src1 = FETCH_QUAD(mem_ref);
	    }
	    res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
	case CMPwtrm: {
	    DWORD res, src1; unsigned char *mem_ref;
	    long src2;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    src1 = *EREG1; mem_ref = MEM_REF; 
 	    if (IS_MODE_REG) {
		src2 = FETCH_EREG(mem_ref);
	    } else {
		src2 = FETCH_QUAD(mem_ref);
	    }
	    res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    } return (PC); 
        case CMPwi: {
            DWORD res, src1;
            long src2;
            src1 = EAX;
            src2 = FETCH_QUAD((PC+1));
            res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
            } else {
                CLEAR_CF;
                CLEAR_AF;
            }
            AL &= 0x0f;
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
	    src2 = -1; 
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
        case DECcx: {
            long res, src1, src2;
	    int carry;
            src1 = ECX; src2 = 1;
            ECX = res = src1 - src2;
	    src2 = -1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
        case DECdx: {
            long res, src1, src2;
	    int carry;
            src1 = EDX; src2 = 1;
            EDX = res = src1 - src2;
	    src2 = -1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
        case DECbx: {
            long res, src1, src2;
	    int carry;
            src1 = EBX; src2 = 1;
            EBX = res = src1 - src2;
	    src2 = -1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
        case DECsp: {
            long res, src1, src2;
	    int carry;
            src1 = ESP; src2 = 1;
            ESP = res = src1 - src2;
            src2 = -1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
        case DECbp: {
            long res, src1, src2;
	    int carry;
            src1 = EBP; src2 = 1;
            EBP = res = src1 - src2;
            src2 = -1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
        case DECsi: {
            long res, src1, src2;
	    int carry;
            src1 = ESI; src2 = 1;
            ESI = res = src1 - src2;
            src2 = -1;
	    carry = CARRY;
	    SETDFLAGS;
	    CARRY = carry;
            } PC += 1; return (PC);
	case DECdi: {
            long res, src1, src2;
	    int carry;
            src1 = EDI; src2 = 1;
            EDI = res = src1 - src2;
	    src2 = -1;
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
	case OPERoverride:	/* 0x66: 16 bit operand, 32 bit addressing */
	    if (!data32) { PC += 1; goto override; }
	    PC = hsw_interp_16_32 (env, P0, PC+1, interp_var, err);
	    if (*err) return P0;
	    return (PC);
	case ADDRoverride:	/* 0x67: 32 bit operand, 16 bit addressing */
	    if (!code32) { PC += 1; goto override; }
	    PC = hsw_interp_32_16 (env, P0, PC+1, interp_var, err);
	    if (*err) return P0;
	    return (PC);
	case PUSHwi: {
            unsigned char *sp = LONG_SS + ESP;
            *(sp - 1) = *(PC + 4);
            *(sp - 2) = *(PC + 3);
            *(sp - 3) = *(PC + 2);
            *(sp - 4) = *(PC + 1);
            ESP -= 4;
            } PC += 5; return (PC);
        case IMULwrm: {
#ifdef DOSEMU
	    int64_t res, mlt;
            unsigned char *mem_ref;
            unsigned long himr;
            PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
            mlt = FETCH_QUAD(PC); PC += 4; mem_ref = MEM_REF;
            if (IS_MODE_REG)
                 res = FETCH_EREG(mem_ref);
            else
                 res = FETCH_QUAD(mem_ref);
	    res *= mlt;
	    *EREG1 = (u_int32_t)res;
	    himr = (u_int32_t)((res<0? -res:res)>>32);
#else
            MULT resm, src1m, src2m, mulr[4];
            unsigned char *mem_ref, sg1=1, sg2=1;
            unsigned long himr;
            PC = PC+1+hsw_modrm_32_quad(env,PC+1,interp_var);
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
            unsigned char *sp = LONG_SS + ESP;
            long temp = (signed char)*(PC+1);
            *(sp - 4) = temp;
            temp = temp >> 8;
            *(sp - 3) = temp;
            *(sp - 2) = temp;
            *(sp - 1) = temp;
            ESP -= 4;
	    } PC += 2; return(PC); 
	case IMMEDwrm: {
	    DWORD src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 4: /* AND */
		    *(DWORD *)mem_ref = res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 5: /* SUB */
		    *(DWORD *)mem_ref = res = src1 - src2;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 6: /* XOR */
		    *(DWORD *)mem_ref = res = src1 ^ src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
	      }
	    }}
	case IMMEDisrm: {
	    DWORD src1, src2, res; unsigned char *mem_ref;
	    signed long temp;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
		case 4: /* AND */
		    *(DWORD *)mem_ref = res = src1 & src2;
		    src1 = src2 = res;
		    SETDFLAGS;
		    return (PC);
		case 5: /* SUB */
		    *(DWORD *)mem_ref = res = src1 - src2;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 6: /* XOR */
		    res = src1 ^ src2;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    PUT_QUAD(mem_ref,res);
		    SETDFLAGS;
		    return (PC);
		case 7: /* CMP */
		    res = src1 - src2;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
		    SETDFLAGS;
		    return (PC);
	      }
	    }}
	case TESTwrm: {
	    DWORD res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		temp = FETCH_EREG(mem_ref);
		*(DWORD *)mem_ref = *EREG1;
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
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		*(DWORD *)MEM_REF = *EREG1;
		return (PC); 
	    } else {
		unsigned long temp = *EREG1;
		unsigned char *mem_ref = MEM_REF;
		PUT_QUAD(mem_ref, temp);
		return (PC); 
	    }
	case MOVwtrm: {
	    unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    *(DWORD *)EREG1 = FETCH_QUAD(mem_ref);
	    } return (PC);
	case MOVsrtrm: {
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		switch (seg_reg) {
		    case 0: /* ES */
			*(unsigned long *)MEM_REF = SHORT_ES_16;
			return (PC);
		    case 1: /* CS */
			*(unsigned long *)MEM_REF = SHORT_CS_16;
			return (PC);
		    case 2: /* SS */
			*(unsigned long *)MEM_REF = SHORT_SS_16;
			return (PC);
		    case 3: /* DS */
			*(unsigned long *)MEM_REF = SHORT_DS_16;
			return (PC);
		    case 4: /* FS */
			*(unsigned long *)MEM_REF = SHORT_FS_16;
			return (PC);
		    case 5: /* GS */
			*(unsigned long *)MEM_REF = SHORT_GS_16;
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
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 1: /* DS */
			temp = SHORT_DS_16;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 2: /* CS */
			temp = SHORT_CS_16;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 3: /* SS */
			temp = SHORT_SS_16;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 4: /* FS */
			temp = SHORT_FS_16;
			PUT_QUAD(mem_ref, temp);
			return (PC);
		    case 5: /* GS */
			temp = SHORT_GS_16;
			PUT_QUAD(mem_ref, temp);
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
	    unsigned long temp;
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		temp = *(unsigned long *)MEM_REF;
	    } else {
		unsigned char *mem_ref = MEM_REF;
		temp = FETCH_QUAD(mem_ref);
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
		    return (PC);
	    }}
	case POPrm: {
	    unsigned char *mem_ref, *sp;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    mem_ref = MEM_REF; sp = LONG_SS + SP;
	    *mem_ref = *sp;
	    *(mem_ref+1) = *(sp+1);
	    *(mem_ref+2) = *(sp+2);
	    *(mem_ref+3) = *(sp+3);
	    ESP += 4;
	    } return (PC);
	case MOVmax: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    EAX = FETCH_QUAD(mem_ref);
	    } PC += 5; return (PC);
	case MOVaxm: {
	    unsigned char *mem_ref;
	    int temp = EAX;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_QUAD((PC+1));
	    PUT_QUAD(mem_ref, temp);
	    } PC += 5; return (PC);
	case MOVSw: {
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    *dest = *src;
	    *(dest+1) = *(src+1);
	    (env->flags & DIRECTION_FLAG)?(ESI-=4,EDI-=4):(ESI+=4,EDI+=4);
	    } PC += 1; return (PC);
	case CMPSw: {
	    unsigned long res, src1;
	    long src2;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    dest = LONG_ES + (EDI);
	    src1 = FETCH_QUAD(src);
	    src2 = FETCH_QUAD(dest);
	    res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    (env->flags & DIRECTION_FLAG)?(ESI-=4,EDI-=4):(ESI+=4,EDI+=4);
	    } PC += 1; return (PC);
	case STOSw:
	    LONG_ES[EDI] = AL;
	    LONG_ES[EDI+1] = AH;
	    LONG_ES[EDI+2] = (EAX >> 16) & 0xff;
	    LONG_ES[EDI+3] = EAX >> 24;
	    (env->flags & DIRECTION_FLAG)?(EDI-=4):(EDI+=4);
	    PC += 1; return (PC);
	case LODSw: {
	    unsigned char *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (ESI);
	    EAX = FETCH_QUAD(seg);
	    (env->flags & DIRECTION_FLAG)?(ESI-=4):(ESI+=4);
	    } PC += 1; return (PC);
	case SCASw: {
	    unsigned long res, src1;
	    long src2;
	    unsigned char *mem_ref;
	    src1 = EAX;
	    mem_ref = LONG_ES + (EDI);
	    src2 = FETCH_QUAD(mem_ref);
	    res = src1 - src2;
	    src2 = ((src2 == 0x80000000)? 0:-src2);
	    SETDFLAGS;
	    (env->flags & DIRECTION_FLAG)?(EDI-=4):(EDI+=4);
	    } PC += 1; return (PC);
	case SHIFTwi: {
	    int temp=0, count; unsigned char *mem_ref;
	    DWORD res, src1, src2;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    count = *PC & 0x1f;  
	    PC += 1;
	    if (count) {
	      if (IS_MODE_REG) {
		unsigned long *reg = (unsigned long *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
                        res = *reg;
        /*              temp = FETCH_QUAD(mem_ref); */
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
			*reg = temp;
			SETDFLAGS;
			CARRY = (res >> (32-count)) & 0x1;
			return (PC);
		    case 5: /* SHR */
			res = *reg;
			src1 = src2 = res;
			res = (res >> count);
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
	case LES: {
	    DWORD temp; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    DWORD temp; unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    if (IS_MODE_REG) {
		/* Illegal */
		goto illegal_op; 
	    }
	    mem_ref = MEM_REF;
	    temp = FETCH_QUAD(mem_ref);
	    *EREG1 = temp;
	    temp = FETCH_QUAD(mem_ref+2);
	    SHORT_DS_32 = temp;
	    SET_SEGREG(LONG_DS,temp);
	    } return (PC);
	case MOVwirm: {
	    /*DWORD temp;*/ unsigned char *mem_ref;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	case SHIFTw: {
	    int count=1; unsigned char *mem_ref;
	    int temp;
	    DWORD res, src1, src2;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
	    mem_ref = MEM_REF;
	      if (IS_MODE_REG) {
		unsigned long *reg = (unsigned long *)MEM_REF;
		switch (res) {
		    case 0: /* ROL */
                        res = *reg;
        /*              temp = FETCH_QUAD(mem_ref); */
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
        /*              temp = FETCH_QUAD(mem_ref); */
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
	case XLAT: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + (EBX) + (AL);
	    AL = *mem_ref; }
	    PC += 1; return (PC);

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
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    count = count << 2;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; ECX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
            		    *(dest+2) = *(src+2);
            		    *(dest+3) = *(src+3);
			    dest -= 4; src -= 4; count -= 4;
			} return (PC);
		    } else {
			EDI += count; ESI += count; ECX = 0;
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
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_QUAD(src);
			    src2 = FETCH_QUAD(dest);
			    src -= 4; dest -=4;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
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
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    ECX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ALLOW_OVERRIDE(LONG_DS);
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
			EDI -= count<<2; ECX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    *(dest+2) = eal;
			    *(dest+3) = eah;
			    dest -= 4;
			} return (PC);
		    } else {		      /* forwards */
			EDI += count<<2; ECX = 0;
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
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; ESI -= count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; ESI += count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } }
		case SCASw: {
		    unsigned char *dest;
		    unsigned long res, src1;
		    long src2;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    src1 = EAX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_QUAD(dest);
			    dest -=4;
			    if (src1 != src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				EDI = dest - LONG_ES;
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
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
				EDI = dest - LONG_ES;
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    ECX = 0; EDI = dest - LONG_ES;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    count = count << 2;
		    if (env->flags & DIRECTION_FLAG) {
			EDI -= count; ESI -= count; ECX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    *(dest+2) = *(src+2);
			    *(dest+3) = *(src+3);
			    dest -= 4; src -= 4; count -= 4;
			} return (PC);
		    } else {
			EDI += count; ESI += count; ECX = 0;
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
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    dest = LONG_ES + EDI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_QUAD(src);
			    src2 = FETCH_QUAD(dest);
			    src -= 4; dest -=4;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
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
				EDI = dest - LONG_ES;
				ESI = src - ALLOW_OVERRIDE(LONG_DS);
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    }
		    res = src1 - src2;
		    ECX = 0;
		    EDI = dest - LONG_ES;
		    ESI = src - ALLOW_OVERRIDE(LONG_DS);
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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
			EDI -= count<<1; ECX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    *(dest+2) = eal;
			    *(dest+3) = eah;
			    dest -= 4;
			} return (PC);
		    } else {		      /* forwards */
			EDI += count<<1; ECX = 0;
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
		    src = ALLOW_OVERRIDE(LONG_DS) + (ESI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; ESI -= count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } else {		      /* forwards */
			src = src + count; ESI += count;
			temp = FETCH_QUAD(src);
			EAX = temp;
			ECX = 0; return (PC);
		    } }
		case SCASw: {
		    unsigned char *dest;
		    unsigned long res, src1;
		    long src2;
		    if (count == 0) return (PC);
		    dest = LONG_ES + EDI;
		    src1 = EAX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_QUAD(dest);
			    dest -=4;
			    if (src1 == src2) count--;
			    else {
			        res = src1 - src2;
				ECX = count - 1;
				EDI = dest - LONG_ES;
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
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
				EDI = dest - LONG_ES;
	    			src2 = ((src2 == 0x80000000)? 0:-src2);
				SETDFLAGS;
				return (PC);
			    }
			}
		    } 
		    res = src1 - src2;
		    ECX = 0; EDI = dest - LONG_ES;
	    	    src2 = ((src2 == 0x80000000)? 0:-src2);
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

	case GRP1wrm: {
	    DWORD src1=0, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
	    	    src2 = ((src == 0x80000000)? 0:-src);
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
			quotent = dividend / src1;
			EAX = floor(quotent);
			EDX = floor(dividend - (EAX * src1));
			if(sg1 != sg2) EAX =-EAX;
			if(sg1<0) EDX = -EDX;
		    }
		    res = EAX;
		    EAX = res / src1;
		    EDX = res % src1;
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
	    	    src2 = ((src == 0x80000000)? 0:-src);
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
			quotent = dividend / src1;
			EAX = floor(quotent);
			EDX = floor(dividend - (EAX * src1));
			if(sg1 != sg2) EAX =-EAX;
			if(sg1<0) EDX = -EDX;
		    }
		    res = EAX;
		    EAX = res / src1;
		    EDX = res % src1;
		    } return (PC);
	      } }
	      }

	case GRP2wrm: {
	    DWORD temp; unsigned char *mem_ref;
	    DWORD res, src1, src2;
	    int carry;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_32_quad(env,PC,interp_var);
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
		case 2: /* CALL indirect */
		    temp = PC - LONG_CS;
		    PUSHQUAD(temp);
		    PC = LONG_CS + FETCH_EREG(mem_ref);
		    return (PC);
		case 3: /* CALL long indirect */ /* Illegal */
                 goto illegal_op;
		case 4: /* JMP indirect */
		    PC = LONG_CS + FETCH_EREG(mem_ref);
		    return (PC);
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
	fprintf(stderr," 32/32 nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2),
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
	fprintf(stderr," 32/32 illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2), 
                SHORT_CS_16,PC-LONG_CS,(int)PC);
#ifdef DOSEMU
	*err = EXCP06_ILLOP; return (P0);
#else
        exit(1); 
#endif
}
