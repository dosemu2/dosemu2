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
	interp_main.c	1.31
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
#include <math.h>
#ifdef linux
#include <asm/bitops.h>
#endif
#include "Log.h"
#include "hsw_interp.h"
#include "mod_rm.h"

extern REGISTER PortIO(DWORD, DWORD, UINT, BOOL);
extern void LogProcName(WORD, WORD, WORD);

extern ENV87 hsw_env87;

#ifdef DOSEMU
BOOL vm86f = 1;
#  define fprintf our_fprintf
#  define our_fprintf(stream, args...) error(##args)
#endif
BOOL data32=0, code32=0;

#ifdef DEBUG
#include <string.h>
char * decode(int opcode, int modrm);
extern char *getenv();
int print = 0;
int small_print = 0;
int stack_print = 0;
int segment_print = 0;
int float_print = 0;
int op32_print = 0;
int ad32_print = 0;
long start_count= -1;
long end_count= -1;
int granularity = 1;
#ifdef DOSEMU
unsigned long loop_PC = 0;
extern long instr_count;
#else
int instr_count = 0;
#endif
int print_initialized = 0;
int dbx_cs = 0;
int dbx_ip = 0;
int dbx_stop_count = 0;
void dbx_stop() {
    dbx_stop_count++;
}
#endif

void
debuggerbreak(void)
{
    fprintf(stderr,"debuggerbreak: exiting\n");
    exit(1);
}

void
trans_flags_to_interp(Interp_ENV *env, Interp_VAR *interp_var, unsigned int flags);
unsigned int
trans_interp_flags(Interp_ENV *env, Interp_VAR *interp_var);

int opcode_table[256];
char unknown_msg[] = "unknown opcode %02x at %04x:%04x\n";
char illegal_msg[] = "illegal opcode %02x at %04x:%04x\n";
char unsupp_msg[]  = "unsupported opcode %02x at %04x:%04x\n";
unsigned char parity[256] = 
    {PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    PARITY_FLAG, 0, 0, PARITY_FLAG, 0, PARITY_FLAG, PARITY_FLAG, 0,
    0, PARITY_FLAG, PARITY_FLAG, 0, PARITY_FLAG, 0, 0, PARITY_FLAG};

extern void hsw_fp87_00m(), hsw_fp87_01m(), hsw_fp87_02m(), hsw_fp87_03m();
extern void hsw_fp87_04m(), hsw_fp87_05m(), hsw_fp87_06m(), hsw_fp87_07m();
extern void hsw_fp87_10m(), hsw_fp87_11m(), hsw_fp87_12m(), hsw_fp87_13m();
extern void hsw_fp87_14m(), hsw_fp87_15m(), hsw_fp87_16m(), hsw_fp87_17m();
extern void hsw_fp87_20m(), hsw_fp87_21m(), hsw_fp87_22m(), hsw_fp87_23m();
extern void hsw_fp87_24m(), hsw_fp87_25m(), hsw_fp87_26m(), hsw_fp87_27m();
extern void hsw_fp87_30m(), hsw_fp87_31m(), hsw_fp87_32m(), hsw_fp87_33m();
extern void hsw_fp87_34m(), hsw_fp87_35m(), hsw_fp87_36m(), hsw_fp87_37m();
extern void hsw_fp87_40m(), hsw_fp87_41m(), hsw_fp87_42m(), hsw_fp87_43m();
extern void hsw_fp87_44m(), hsw_fp87_45m(), hsw_fp87_46m(), hsw_fp87_47m();
extern void hsw_fp87_50m(), hsw_fp87_51m(), hsw_fp87_52m(), hsw_fp87_53m();
extern void hsw_fp87_54m(), hsw_fp87_55m(), hsw_fp87_56m(), hsw_fp87_57m();
extern void hsw_fp87_60m(), hsw_fp87_61m(), hsw_fp87_62m(), hsw_fp87_63m();
extern void hsw_fp87_64m(), hsw_fp87_65m(), hsw_fp87_66m(), hsw_fp87_67m();
extern void hsw_fp87_70m(), hsw_fp87_71m(), hsw_fp87_72m(), hsw_fp87_73m();
extern void hsw_fp87_74m(), hsw_fp87_75m(), hsw_fp87_76m(), hsw_fp87_77m();
extern void hsw_fp87_00r(), hsw_fp87_01r(), hsw_fp87_02r(), hsw_fp87_03r();
extern void hsw_fp87_04r(), hsw_fp87_05r(), hsw_fp87_06r(), hsw_fp87_07r();
extern void hsw_fp87_10r(), hsw_fp87_11r(), hsw_fp87_12r(), hsw_fp87_13r();
extern void hsw_fp87_14r(), hsw_fp87_15r(), hsw_fp87_16r(), hsw_fp87_17r();
extern void hsw_fp87_20r(), hsw_fp87_21r(), hsw_fp87_22r(), hsw_fp87_23r();
extern void hsw_fp87_24r(), hsw_fp87_25r(), hsw_fp87_26r(), hsw_fp87_27r();
extern void hsw_fp87_30r(), hsw_fp87_31r(), hsw_fp87_32r(), hsw_fp87_33r();
extern void hsw_fp87_34r(), hsw_fp87_35r(), hsw_fp87_36r(), hsw_fp87_37r();
extern void hsw_fp87_40r(), hsw_fp87_41r(), hsw_fp87_42r(), hsw_fp87_43r();
extern void hsw_fp87_44r(), hsw_fp87_45r(), hsw_fp87_46r(), hsw_fp87_47r();
extern void hsw_fp87_50r(), hsw_fp87_51r(), hsw_fp87_52r(), hsw_fp87_53r();
extern void hsw_fp87_54r(), hsw_fp87_55r(), hsw_fp87_56r(), hsw_fp87_57r();
extern void hsw_fp87_60r(), hsw_fp87_61r(), hsw_fp87_62r(), hsw_fp87_63r();
extern void hsw_fp87_64r(), hsw_fp87_65r(), hsw_fp87_66r(), hsw_fp87_67r();
extern void hsw_fp87_70r(), hsw_fp87_71r(), hsw_fp87_72r(), hsw_fp87_73r();
extern void hsw_fp87_74r(), hsw_fp87_75r(), hsw_fp87_76r(), hsw_fp87_77r();

FUNCT_PTR hsw_fp87_mem0[8] = {hsw_fp87_00m, hsw_fp87_01m, hsw_fp87_02m,
			      hsw_fp87_03m, hsw_fp87_04m, hsw_fp87_05m,
			      hsw_fp87_06m, hsw_fp87_07m};
FUNCT_PTR hsw_fp87_mem1[8] = {hsw_fp87_10m, hsw_fp87_11m, hsw_fp87_12m,
			      hsw_fp87_13m, hsw_fp87_14m, hsw_fp87_15m,
			      hsw_fp87_16m, hsw_fp87_17m};
FUNCT_PTR hsw_fp87_mem2[8] = {hsw_fp87_20m, hsw_fp87_21m, hsw_fp87_22m,
			      hsw_fp87_23m, hsw_fp87_24m, hsw_fp87_25m,
			      hsw_fp87_26m, hsw_fp87_27m};
FUNCT_PTR hsw_fp87_mem3[8] = {hsw_fp87_30m, hsw_fp87_31m, hsw_fp87_32m,
			      hsw_fp87_33m, hsw_fp87_34m, hsw_fp87_35m,
			      hsw_fp87_36m, hsw_fp87_37m};
FUNCT_PTR hsw_fp87_mem4[8] = {hsw_fp87_40m, hsw_fp87_41m, hsw_fp87_42m,
			      hsw_fp87_43m, hsw_fp87_44m, hsw_fp87_45m,
			      hsw_fp87_46m, hsw_fp87_47m};
FUNCT_PTR hsw_fp87_mem5[8] = {hsw_fp87_50m, hsw_fp87_51m, hsw_fp87_52m,
			      hsw_fp87_53m, hsw_fp87_54m, hsw_fp87_55m,
			      hsw_fp87_56m, hsw_fp87_57m};
FUNCT_PTR hsw_fp87_mem6[8] = {hsw_fp87_60m, hsw_fp87_61m, hsw_fp87_62m,
			      hsw_fp87_63m, hsw_fp87_64m, hsw_fp87_65m,
			      hsw_fp87_66m, hsw_fp87_67m};
FUNCT_PTR hsw_fp87_mem7[8] = {hsw_fp87_70m, hsw_fp87_71m, hsw_fp87_72m,
			      hsw_fp87_73m, hsw_fp87_74m, hsw_fp87_75m,
			      hsw_fp87_76m, hsw_fp87_77m};
FUNCT_PTR hsw_fp87_reg0[8] = {hsw_fp87_00r, hsw_fp87_01r, hsw_fp87_02r,
			      hsw_fp87_03r, hsw_fp87_04r, hsw_fp87_05r,
			      hsw_fp87_06r, hsw_fp87_07r};
FUNCT_PTR hsw_fp87_reg1[8] = {hsw_fp87_10r, hsw_fp87_11r, hsw_fp87_12r,
			      hsw_fp87_13r, hsw_fp87_14r, hsw_fp87_15r,
			      hsw_fp87_16r, hsw_fp87_17r};
FUNCT_PTR hsw_fp87_reg2[8] = {hsw_fp87_20r, hsw_fp87_21r, hsw_fp87_22r,
			      hsw_fp87_23r, hsw_fp87_24r, hsw_fp87_25r,
			      hsw_fp87_26r, hsw_fp87_27r};
FUNCT_PTR hsw_fp87_reg3[8] = {hsw_fp87_30r, hsw_fp87_31r, hsw_fp87_32r,
			      hsw_fp87_33r, hsw_fp87_34r, hsw_fp87_35r,
			      hsw_fp87_36r, hsw_fp87_37r};
FUNCT_PTR hsw_fp87_reg4[8] = {hsw_fp87_40r, hsw_fp87_41r, hsw_fp87_42r,
			      hsw_fp87_43r, hsw_fp87_44r, hsw_fp87_45r,
			      hsw_fp87_46r, hsw_fp87_47r};
FUNCT_PTR hsw_fp87_reg5[8] = {hsw_fp87_50r, hsw_fp87_51r, hsw_fp87_52r,
			      hsw_fp87_53r, hsw_fp87_54r, hsw_fp87_55r,
			      hsw_fp87_56r, hsw_fp87_57r};
FUNCT_PTR hsw_fp87_reg6[8] = {hsw_fp87_60r, hsw_fp87_61r, hsw_fp87_62r,
			      hsw_fp87_63r, hsw_fp87_64r, hsw_fp87_65r,
			      hsw_fp87_66r, hsw_fp87_67r};
FUNCT_PTR hsw_fp87_reg7[8] = {hsw_fp87_70r, hsw_fp87_71r, hsw_fp87_72r,
			      hsw_fp87_73r, hsw_fp87_74r, hsw_fp87_75r,
			      hsw_fp87_76r, hsw_fp87_77r};

void
invoke_data (Interp_ENV *envp)
{
    char buf[0x100];

    sprintf(buf,"ERROR: data selector in CS\n cs:ip %x/%x\ncalled from %x:%x",
		HIWORD(envp->trans_addr),LOWORD(envp->trans_addr),
		HIWORD(envp->return_addr),LOWORD(envp->return_addr));
    FatalAppExit(0,buf);
}

unsigned char *
  hsw_interp_16_16 (Interp_ENV *env, unsigned char *P0, unsigned char *PC,
  	Interp_VAR *interp_var, int *err)
{
  *err = 0;
#if defined(DOSEMU) && defined(DEBUG)
  loop_PC = 0;
#endif
next_switch:
#if defined(DOSEMU) && defined(DEBUG)
  instr_count++;
#endif
  OVERRIDE = INVALID_OVR;
  P0 = PC;
override: ;    /* single semicolon needed to attach label to */
#ifdef DEBUG
#ifdef DOSEMU
    if ((long)PC > loop_PC) { e_debug(env, P0, PC, interp_var); loop_PC = 0; }
#else
    if((instr_count++)==start_count)print=1;
    if(instr_count==end_count)print=0;
    if (dbx_cs)
      if ((dbx_cs == SHORT_CS_16) && (dbx_ip == (PC - LONG_CS))) {
	dbx_stop();
      }

    if(print && (!(instr_count % granularity))){
	if(small_print)
	    printf("%d %04x:%04x %02x %02x\n", instr_count, SHORT_CS_16, PC-(LONG_CS),*PC, *(PC+1));
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
	    /*      address   opcode     ax   bx   cx   dx   si   di  bp  sp*/
	    printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS),*PC, *(PC+1), AX, BX, CX, DX, SI, DI, BP, SP, decode(*PC, *(PC+1)), instr_count);
	fflush(stdout);
    }
#endif	/* DOSEMU */
#endif

    switch (*PC) {
	case ADDbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
#ifdef DOSEMU
	    /* 00 00 00 is probably invalid code... */
	    if ((*(PC+1)==0)&&(*(PC+2)==0)) { *err=-98; return P0; }
#endif
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } goto next_switch; 
	case ADDwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case ADDbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } goto next_switch; 
	case ADDwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 + src2;
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } goto next_switch; 
	case ADDbia: {
	    unsigned int res; unsigned char src1, src2;
	    src1 = AL; src2 = *(PC+1);
	    AL = res = src1 + src2;
	    RES_32 = res << 8; SRC1_8 = src1; SRC2_8 = src2; BYTE_FLAG = BYTE_OP;
	    } PC += 2; goto next_switch;
	case ADDwia: {
	    unsigned int res, src2; unsigned short src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 + src2;
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 3; goto next_switch;
	case PUSHes: {
	    unsigned short temp = SHORT_ES_16;
	    PUSHWORD(temp); 
	    } PC += 1; goto next_switch;
	case POPes: {
	    unsigned int temp;
	    POPWORD(temp);
	    SET_SEGREG(LONG_ES,temp);
	    SHORT_ES_32 = temp; }
	    PC += 1; goto next_switch;
	case ORbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
          BYTE_FLAG = BYTE_OP;
	    } goto next_switch; 
	case ORwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case ORbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 | src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
          BYTE_FLAG = BYTE_OP;
	    } goto next_switch; 
	case ORwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 | src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } goto next_switch; 
	case ORbi: {
	    unsigned int res = AL | *(PC+1);
	    SRC1_16 = SRC2_16 = RES_32 = res << 8; BYTE_FLAG = BYTE_OP;    
	    AL = res;
	    } PC += 2; goto next_switch;
	case ORwi: {
	    unsigned int res, src2; unsigned short src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 | src2;
	    SRC1_16 = SRC2_16 = RES_32 = res;
	    } PC += 3; goto next_switch;
	case PUSHcs: {
	    unsigned short temp = SHORT_CS_16;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case TwoByteESC: {
	    switch (*(PC+1)) {
		case 0x00: /* GRP6 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SLDT */ {
			    /* Store Local Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be LDT selector */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } goto next_switch;
			case 1: /* STR */ {
			    /* Store Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0 /* should be Task Register */;
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } goto next_switch;
			case 2: /* LLDT */ /* Privileged */
			    /* Load Local Descriptor Table Register */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC +1 + hsw_modrm_16_word(env,PC + 1,interp_var);
			    goto next_switch;
			case 3: /* LTR */ {
			    /* Load Task Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) temp = FETCH_XREG(mem_ref);
			    else temp = FETCH_WORD(mem_ref);
		goto not_implemented;
			    }
			case 4: /* VERR */ {
			    int temp, _sel; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
			    else _sel = FETCH_WORD(mem_ref);
			    if((_sel &0x7) != 0x7)
				RES_16 = 0;
			    else {
				temp = (GetSelectorFlags(_sel)) & 0xffff;
				RES_16 = (!(temp & DF_CODE) || (temp & DF_CREADABLE))?1:0;
			    }
			    } goto next_switch;
			case 5: /* VERW */ {
			    int temp, _sel; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
			    else _sel = FETCH_WORD(mem_ref);
			    if((_sel &0x7) != 0x7)
				RES_16 = 0;
			    else {
				temp = (GetSelectorFlags(_sel)) & 0xffff;
				RES_16 = (!(temp & DF_CODE) && (temp & DF_DWRITEABLE))?1:0;
			    }
			    } goto next_switch;
			case 6: /* Illegal */
			case 7: /* Illegal */
			    goto illegal_op;
		    }
		case 0x01: /* GRP7 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* SGDT */ {
			    /* Store Global Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_WORD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 1: /* SIDT */ {
			    /* Store Interrupt Descriptor Table Register */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
			    temp = 0; /* should be LIMIT field */;
			    PUT_WORD(mem_ref,temp);
			    temp = 0; /* should be BASE field (4 bytes) */
			    PUT_QUAD(mem_ref,temp);
			    } goto next_switch;
			case 2: /* LGDT */ /* Privileged */
			    /* Load Global Descriptor Table Register */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC+1+hsw_modrm_16_word(env,PC + 1,interp_var);
		goto not_implemented;
			case 3: /* LIDT */ /* Privileged */
			    /* Load Interrupt Descriptor Table Register */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC+1+hsw_modrm_16_word(env,PC + 1,interp_var);
			    goto next_switch;
			case 4: /* SMSW */ {
			    /* Store Machine Status Word */
			    int temp; unsigned char *mem_ref;
			    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
			    mem_ref = MEM_REF;
#ifdef DOSEMU
			    temp = 0x13;	/* PM, FPU */
#else
			    temp = 0 /* should be LIMIT field */;
#endif
			    if (IS_MODE_REG) *(unsigned short *)mem_ref = temp;
			    else {PUT_WORD(mem_ref,temp);}
			    } goto next_switch;
			case 5: /* Illegal */
			    goto illegal_op;
			case 6: /* LMSW */ /* Privileged */
			    /* Load Machine Status Word */
#ifdef DOSEMU
			    if (vm86f) goto not_permitted;
#endif
			    PC = PC+1+hsw_modrm_16_word(env,PC + 1,interp_var);
		goto not_implemented;
			case 7: /* Illegal */
			    goto illegal_op;
		    }
		case 0x02: /* LAR */ {
		    /* Load Access Rights Byte */
		    int temp; unsigned char *mem_ref;
			WORD _sel;
		    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
		    mem_ref = MEM_REF;
	 	    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
		if((_sel &0x7) != 0x7)
			RES_16 = 1;
		else {
		temp = (GetSelectorFlags(_sel) << 8) & 0xffff;
			RES_16 = 0;
			*XREG1 = temp;
		}
		    /* what do I do here??? */
		    } goto next_switch;
		case 0x03: /* LSL */ {
		    /* Load Segment Limit */
		    int temp; unsigned char *mem_ref;
			WORD _sel;
		    PC += 1; PC += hsw_modrm_16_word(env,PC,interp_var);
		    mem_ref = MEM_REF;
                    if (IS_MODE_REG) _sel = FETCH_XREG(mem_ref);
                    else _sel = FETCH_WORD(mem_ref);
 		if((_sel & 0x7) != 0x7)
			RES_16 = 1;
		else {
		temp= (WORD)GetSelectorLimit(_sel);
			*XREG1 = temp;
			RES_16 = 0;
		}
		    /* what do I do here??? */
		    } goto next_switch;
		case 0x06: /* CLTS */ /* Privileged */
		    /* Clear Task State Register */
#ifdef DOSEMU
		    if (vm86f) goto not_permitted;
#endif
		    PC += 2; goto next_switch;
		case 0x08: /* INVD */
		    /* INValiDate cache */
		    PC += 2; goto next_switch;
		case 0x09: /* WBINVD */
		    /* Write-Back and INValiDate cache */
		    PC += 2; goto next_switch;
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
		    if (IS_OF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x81: /* JNOimmdisp */
		    if (!IS_OF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x82: /* JBimmdisp */
		    if (IS_CF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x83: /* JNBimmdisp */
		    if (!IS_CF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x84: /* JZimmdisp */
		    if (IS_ZF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x85: /* JNZimmdisp */
		    if (!IS_ZF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x86: /* JBEimmdisp */
		    if (IS_CF_SET || IS_ZF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x87: /* JNBEimmdisp */
		    if (!(IS_CF_SET || IS_ZF_SET)) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x88: /* JSimmdisp */
		    if (IS_SF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x89: /* JNSimmdisp */
		    if (!IS_SF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x8a: /* JPimmdisp */
		    if (IS_PF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x8b: /* JNPimmdisp */
		    if (!IS_PF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x8c: /* JLimmdisp */
		    if (IS_SF_SET ^ IS_OF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x8d: /* JNLimmdisp */
		    if (!(IS_SF_SET ^ IS_OF_SET)) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x8e: /* JLEimmdisp */
		    if ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x8f: /* JNLEimmdisp */
		    if (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) {
			int temp = FETCH_WORD(PC+2);
			PC += (4 + ((temp << 16)>>16));
			goto next_switch;
		    } PC += 4; goto next_switch;
		case 0x90: /* SETObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_OF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x91: /* SETNObrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_OF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x92: /* SETBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x93: /* SETNBbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_CF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x94: /* SETZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x95: /* SETNZbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x96: /* SETBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_CF_SET || IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x97: /* SETNBEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_CF_SET || IS_ZF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0x98: /* SETSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x99: /* SETNSbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0x9a: /* SETPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_PF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9b: /* SETNPbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!IS_PF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9c: /* SETLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (IS_SF_SET ^ IS_OF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9d: /* SETNLbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0x9e: /* SETLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) ? 1 : 0;
		    } goto next_switch;
		case 0x9f: /* SETNLEbrm */ {
		    unsigned char *mem_ref;
		    PC = PC + 1 + hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    *mem_ref = (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) ? 1 : 0;
		    } goto next_switch;
		case 0xa0: /* PUSHfs */ {
		    unsigned short temp = SHORT_FS_16;
		    PUSHWORD(temp);
		    } PC += 2; goto next_switch;
		case 0xa1: /* POPfs */ {
		    unsigned int temp;
		    POPWORD(temp);
		    SET_SEGREG(LONG_FS,temp);
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
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xa4: /* SHLDimm */ {
		    /* Double Prescision Shift Left */
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		     } goto next_switch;
		case 0xa5: /* SHLDcl */ {
		    /* Double Prescision Shift Left by CL */
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		     } goto next_switch;
		case 0xa6:{ /* CMPXCHGb */	/* NOT IMPLEMENTED !!!!!! */
           	    unsigned int src1, src2;
		    int res;
		    unsigned char *mem_ref;
            	    PC++; PC += hsw_modrm_16_byte(env,PC,interp_var);
            	    src2 = *HREG1; mem_ref = MEM_REF;
		    src1 = *mem_ref;
            	    res = src1 - AL;
		    if(res) AL = src1;
		    else *mem_ref = src2;
            	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
            	    SRC1_8 = src1; SRC2_8 = -AL;
		goto not_implemented;
                    }
		case 0xa7:{ /* CMPXCHGw */	/* NOT IMPLEMENTED !!!!!! */
		    int res;
		    unsigned long src1, src2;
		    unsigned char *mem_ref;
		    PC++; PC += hsw_modrm_16_word(env,PC,interp_var);
		    src2 = *XREG1; mem_ref = MEM_REF; 
		    if (IS_MODE_REG) {
		    src1 = FETCH_XREG(mem_ref);
		    } else {
		    src1 = FETCH_WORD(mem_ref);
            	    }
		    res = src1 - AX;
		    if(res) AX = src1;
		    else { if(IS_MODE_REG)
			    PUT_XREG(mem_ref,src2);
			   else
			    PUT_WORD(mem_ref,src2);
			 }
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = -AX;
		goto not_implemented;
		    }
		case 0xa8: /* PUSHgs */ {
		    unsigned short temp = SHORT_GS_16;
		    PUSHWORD(temp);
		    } PC += 2; goto next_switch;
		case 0xa9: /* POPgs */ {
		    unsigned int temp;
		    POPWORD(temp);
		    SET_SEGREG(LONG_GS,temp);
		    SHORT_GS_32 = temp;
		    } PC += 2; goto next_switch;
		case 0xaa:
		    goto illegal_op;
                case 0xab: /* BTS */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind; 
                    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xac: /* SHRDimm */ {
		    /* Double Precision Shift Right by immediate */
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF; temp1 = *XREG1;
		    count = *PC & 0xf; PC++;
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
		    SRC1_16 = SRC2_16 = temp << 1;
		    } goto next_switch;
		case 0xad: /* SHRDcl */ {
		    /* Double Precision Shift Right by CL */
		    unsigned char *mem_ref; int count, temp, temp1;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    SRC1_16 = SRC2_16 = temp << 1;
		    } goto next_switch;
		case 0xaf: /* IMULregrm */ {
                        int res, src1, src2; unsigned char *mem_ref;
                        PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
                        src1 = *XREG1; mem_ref = MEM_REF;
                        if (IS_MODE_REG)
                            src2 = FETCH_XREG(mem_ref);
                        else
                            src2 = FETCH_WORD(mem_ref);
                        res = src1 * src2;
                        *XREG1 = res; res = (res >> 16);
                        SRC1_16 = SRC2_16 = 0;
                        RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
                        } goto next_switch;
		case 0xb2: /* LSS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
                case 0xb3: /* BTR */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xb4: /* LFS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xb5: /* LGS */ {
		    int temp; unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xb6: /* MOVZXb */ {
		    DWORD temp; 
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
		    temp = *(char *)MEM_REF;
		    *XREG1 = (WORD)temp;
		    } goto next_switch;
		case 0xb7: /* MOVZXw */ {
                  unsigned int temp;
                  unsigned char *mem_ref;
		  PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
                  mem_ref = MEM_REF;
                  if(IS_MODE_REG)
                      temp = FETCH_XREG(mem_ref);
                  else
                      temp = FETCH_WORD(mem_ref);
                  *XREG1 = (WORD)temp;
		  } goto next_switch;
		case 0xba: /* GRP8 */
		    switch ((*(PC+2)>>3)&7) {
			case 0: /* Illegal */
			case 1: /* Illegal */
			case 2: /* Illegal */
			case 3: /* Illegal */
				goto illegal_op;
			case 4: /* BT */ {
               		    unsigned char *mem_ref; int temp,temp1;
               		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
                	    mem_ref = MEM_REF; temp = *PC;  PC++;
                	    if (IS_MODE_REG)
				temp1 = *(unsigned short *)mem_ref;
                	    else
				temp1 = FETCH_WORD(mem_ref);
                            CARRY = ((int)temp1>>(int)( temp & 0xf))&1;
                    	    } goto next_switch;
			case 5: /* BTS */ {
               		    unsigned char *mem_ref; int temp,temp1;
               		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
                    	    } goto next_switch;
			case 6: /* BTR */ {
               		    unsigned char *mem_ref; int temp,temp1;
               		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
                    	    } goto next_switch;
			case 7: /* BTC */ {
               		    unsigned char *mem_ref; int temp,temp1;
               		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
                    	    } goto next_switch;
			}
                case 0xbb: /* BTC */ {
                    unsigned char *mem_ref; DWORD temp, ind1;
                    long ind;
                    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xbc: /* BSF */ {
		    DWORD temp, i;
		    unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
		    mem_ref = MEM_REF;
		    if(IS_MODE_REG)
			temp = *(unsigned short *)mem_ref;
		    else
			temp = FETCH_WORD(mem_ref);
		    if(temp) {
		    for(i=0; i<16; i++)
			if((temp >> i) & 0x1) break ;
		    *XREG1 = i; RES_16 = 1;
		    } else RES_16 = 0;
		    } goto next_switch;
		case 0xbd: /* BSR */ {
		    DWORD temp;
		    int i;
		    unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
		    } goto next_switch;
		case 0xbe: /* MOVSXb */ {
		    signed long temp; 
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
		    temp = *(signed char *)MEM_REF; 
		    *XREG1 = (WORD)temp;
		    } goto next_switch;
		case 0xbf: { /* MOVSXw */
                    signed long temp;
                    unsigned char *mem_ref;
		    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
                    mem_ref = MEM_REF;
                    if(IS_MODE_REG)
                        temp = FETCH_XREG(mem_ref);
                    else
                        temp = FETCH_WORD(mem_ref);
		    temp = ((temp<<16)>>16);
                    *XREG1 = (WORD)temp;
		    } goto next_switch;
                case 0xc0: { /* XADDb */
                    int res,src1,src2; unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_16_byte(env,PC+1,interp_var);
                    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
                    *HREG1 = src1;
                    *mem_ref = res = src1 + src2;
                    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
                    SRC1_8 = src1; SRC2_8 = src2;
                    } goto next_switch;
                case 0xc1: { /* XADDw */
                    int res,src1,src2; unsigned char *mem_ref;
                    PC = PC+1+hsw_modrm_16_word(env,PC+1,interp_var);
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
                    } goto next_switch;
                case 0xc8: /* BSWAPeax */ {
                    unsigned long temp = EAX;
                    EAX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
		case 0xc9: /* BSWAPecx */ {
                    unsigned long temp = ECX;
                    ECX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
                case 0xca: /* BSWAPedx */ {
                    unsigned long temp = EDX;
                    EDX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
		case 0xcb: /* BSWAPebx */ {
                    unsigned long temp = EBX;
                    EBX = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
                case 0xcc: /* BSWAPesp */ {
                    unsigned long temp = ESP;
                    ESP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
                case 0xcd: /* BSWAPebp */ {
                    unsigned long temp = EBP;
                    EBP = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
                case 0xce: /* BSWAPesi */ {
                    unsigned long temp = ESI;
                    ESI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
                case 0xcf: /* BSWAPedi */ {
                    unsigned long temp = EDI;
                    EDI = (temp << 24) | (temp >> 24) |
                        ((temp << 8) & 0xff0000) | ((temp >> 8)& 0xff00);
		    } PC += 2; goto next_switch;
		}
	    }
	case ADCbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } goto next_switch; 
	case ADCwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case ADCbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 + src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = src2;
	    } goto next_switch; 
	case ADCwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    src2 = src2 + (CARRY & 1);
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 + src2;
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } goto next_switch; 
	case ADCbi: {
	    unsigned int res, src2;
	    unsigned char src1;
	    src1 = AL; 
	    src2 = *(PC+1) + (CARRY & 1);
	    AL = res = src1 + src2;
	    RES_32=res<<8;SRC1_8=src1;SRC2_8=src2;BYTE_FLAG=BYTE_OP;
	    } PC += 2; goto next_switch;
	case ADCwi: {
	    unsigned int res,  src2; unsigned short src1;
	    src1 = AX; 
	    src2 = (FETCH_WORD((PC+1))) + (CARRY & 1);
	    AX = res = src1 + src2;
	    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 3; goto next_switch;
	case PUSHss: {
	    unsigned short temp = SHORT_SS_16;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case POPss: {
	    unsigned int temp;
	    unsigned char *old_ss, *new_ss;
	    old_ss = LONG_SS;
	    POPWORD(temp);
	    SET_SEGREG(new_ss,temp);
	    SHORT_SS_32 = temp;
	    LONG_SS = new_ss;
	    } PC += 1; goto next_switch;
	case SBBbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } goto next_switch; 
	case SBBwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case SBBbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    src2 = src2 + (CARRY & 1);
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } goto next_switch; 
	case SBBwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case SBBbi: {
	    unsigned int res, src2;
	    unsigned char src1;
	    src1 = AL; 
	    src2 = *(PC+1) + (CARRY & 1);
	    AL = res = src1 - src2;
	    RES_32=res<<8; SRC1_8=src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); BYTE_FLAG=BYTE_OP;
	    } PC += 2; goto next_switch;
	case SBBwi: {
	    unsigned int res, src1, src2;
	    src1 = AX; 
	    src2 = (FETCH_WORD((PC+1))) + (CARRY & 1);
	    AX = res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 3; goto next_switch;
	case PUSHds: {
	    unsigned short temp = SHORT_DS_16;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case POPds:
	    POPWORD(SHORT_DS_16);
	    SET_SEGREG(LONG_DS,SHORT_DS_16);
	    PC += 1; goto next_switch;
	case ANDbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } goto next_switch; 
	case ANDwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case ANDbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 & src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } goto next_switch; 
	case ANDwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 & src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } goto next_switch; 
	case ANDbi: {
	    unsigned int res = AL & *(PC+1);
	    SRC1_16 = SRC2_16 = RES_32 = res << 8; BYTE_FLAG = BYTE_OP;    
	    AL = res;
	    } PC += 2; goto next_switch;
	case ANDwi: {
	    unsigned int res, src2; unsigned short src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 & src2;
	    SRC1_16 = SRC2_16 = RES_32 = res;
	    } PC += 3; goto next_switch;
	case SEGes:
#ifdef DOSEMU
	    if (!vm86f && (SHORT_ES_16 == 0 || LONG_ES == (unsigned char *) -1)) {
#else
	    if (SHORT_ES_16 == 0 || LONG_ES == (unsigned char *)-1) {
#endif
		char outbuf[80];
		sprintf(outbuf,
			"General Protection Fault: CS:IP %x:%x zero ES\n",
			SHORT_CS_16,PC-(LONG_CS));
		FatalAppExit(0,outbuf);
	    }
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
	    PC += 1; goto next_switch;
	case SUBbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } goto next_switch; 
	case SUBwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case SUBbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } goto next_switch; 
	case SUBwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } goto next_switch; 
	case SUBbi: {
	    unsigned int res;
	    unsigned char src1, src2;
	    src1 = AL; 
	    src2 = *(PC+1);
	    AL = res = src1 - src2;
	    RES_32=res<<8;SRC1_8=src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);BYTE_FLAG=BYTE_OP;
	    } PC += 2; goto next_switch;
	case SUBwi: {
	    unsigned int res, src2; unsigned short src1;
	    src1 = AX; 
	    src2 = FETCH_WORD((PC+1));
	    AX = res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 3; goto next_switch;
	case SEGcs:
	    OVERRIDE = LONG_CS;
	    PC+=1; goto override;
	case DAS:
	    if (((unsigned int)( AX & 0x0f ) > 9 ) || (IS_AF_SET)) {
		AL -= 6;
		SET_AF
	    } else CLEAR_AF
	    if (((AL & 0xf0) > 0x90) || (IS_CF_SET)) {
		AL -= 0x60;
		SET_CF;
	    } else CLEAR_CF;
            RES_8 = AL;
            BYTE_FLAG = BYTE_OP;
	    PC += 1; goto next_switch;
	case XORbfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; mem_ref = MEM_REF; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } goto next_switch; 
	case XORwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch; 
	case XORbtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; mem_ref = HREG1; src1 = *mem_ref;
	    *mem_ref = res = src1 ^ src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } goto next_switch; 
	case XORwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    *XREG1 = res = src1 ^ src2;
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } goto next_switch; 
	case XORbi: {
	    unsigned char res = AL ^ *(PC+1);
	    SRC1_16 = SRC2_16 = RES_32 = res << 8; BYTE_FLAG = BYTE_OP;    
	    AL = res;
	    } PC += 2; goto next_switch;
	case XORwi: {
	    unsigned int res, src2; unsigned short src1;
	    src1 = AX; src2 = FETCH_WORD((PC+1));
	    AX = res = src1 ^ src2;
	    SRC1_16 = SRC2_16 = RES_32 = res;
	    } PC += 3; goto next_switch;
	case SEGss:
	    OVERRIDE = LONG_SS;
	    PC+=1; goto override;
	case AAA:
	    if (((unsigned int)( AX & 0x0f ) > 9 ) || (IS_AF_SET)) {
		AL += 6;
		AH += 1; 
		SET_CF;
		SET_AF
	        AL &= 0x0f;
	    } else {
		CLEAR_CF;
		CLEAR_AF
	    }
	    PC += 1; goto next_switch;
	case CMPbfrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } goto next_switch; 
	case CMPwfrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
	    } else {
		src1 = FETCH_WORD(mem_ref);
	    }
	    res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } goto next_switch; 
	case CMPbtrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *MEM_REF; src1 = *HREG1;
	    res = src1 - src2;
	    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
	    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
	    } goto next_switch; 
	case CMPwtrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src1 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src2 = FETCH_XREG(mem_ref);
	    } else {
		src2 = FETCH_WORD(mem_ref);
	    }
	    res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } goto next_switch; 
	case CMPbi: {
	    unsigned int res, src2;
	    unsigned char src1;
	    src1 = AL; 
	    src2 = *(PC+1);
	    res = src1 - src2;
	    RES_32 = res << 8; SRC1_8 = src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); BYTE_FLAG = BYTE_OP;
	    } PC += 2; goto next_switch;
	case CMPwi: {
	    unsigned int res, src2; unsigned short src1;
	    src1 = AX; 
	    src2 = FETCH_WORD((PC+1));
	    res = src1 - src2;
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 3; goto next_switch;
	case SEGds:
	    OVERRIDE = LONG_DS;
	    PC+=1; goto override;
	case AAS:
	    if (((unsigned int)( AX & 0x0f ) > 9 ) || (IS_AF_SET)) {
		AL -= 6;
		AH -= 1;
		SET_CF;
		SET_AF
	        AL &= 0x0f;
	    } else {
		CLEAR_CF;
		CLEAR_AF
	    }
	    PC += 1; goto next_switch;
	case INCax: {
	    unsigned int res, src1;
	    src1 = AX; 
	    AX = res = src1 + 1;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = 1;
	    } PC += 1; goto next_switch;
	case INCcx: {
	    unsigned int res, src1, src2;
	    src1 = CX; src2 = 1;
	    CX = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;
	case INCdx: {
	    unsigned int res, src1, src2;
	    src1 = DX; src2 = 1;
	    DX = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;
	case INCbx: {
	    unsigned int res, src1, src2;
	    src1 = BX; src2 = 1;
	    BX = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;
	case INCsp: {
	    unsigned int res, src1, src2;
	    src1 = SP; src2 = 1;
	    SP = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;
	case INCbp: {
	    unsigned int res, src1, src2;
	    src1 = BP; src2 = 1;
	    res = src1 + src2;
	    BP = RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;
	case INCsi: {
	    unsigned int res, src1, src2;
	    src1 = SI; src2 = 1;
	    SI = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;
	case INCdi: {
	    unsigned int res, src1, src2;
	    src1 = DI; src2 = 1;
	    DI = res = src1 + src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = src2;
	    } PC += 1; goto next_switch;

	case DECax: {
	    int res, src1, src2;
	    src1 = AX; src2 = 1;
	    AX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECcx: {
	    int res, src1, src2;
	    src1 = CX; src2 = 1;
	    CX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECdx: {
	    int res, src1, src2;
	    src1 = DX; src2 = 1;
	    DX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECbx: {
	    int res, src1, src2;
	    src1 = BX; src2 = 1;
	    BX = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECsp: {
	    int res, src1, src2;
	    src1 = SP; src2 = 1;
	    SP = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECbp: {
	    int res, src1, src2;
	    src1 = BP; src2 = 1;
	    BP = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECsi: {
	    int res, src1, src2;
	    src1 = SI; src2 = 1;
	    SI = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;
	case DECdi: {
	    int res, src1, src2;
	    src1 = DI; src2 = 1;
	    DI = res = src1 - src2;
	    RES_16 = res; SRC1_16 = src1; SRC2_16 = -src2;
	    } PC += 1; goto next_switch;

	case PUSHax: {
	    unsigned short temp = AX;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHcx: {
	    unsigned short temp = CX;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHdx: {
	    unsigned short temp = DX;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHbx: {
	    unsigned short temp = BX;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHsp: {
	    unsigned short temp = SP;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHbp: {
	    unsigned short temp = BP;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHsi: {
	    unsigned short temp = SI;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case PUSHdi: {
	    unsigned short temp = DI;
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case POPax: POPWORD(AX); PC += 1; goto next_switch;
	case POPcx: POPWORD(CX); PC += 1; goto next_switch;
	case POPdx: POPWORD(DX); PC += 1; goto next_switch;
	case POPbx: POPWORD(BX); PC += 1; goto next_switch;
	case POPsp: POPWORD(SP); PC += 1; goto next_switch;
	case POPbp: POPWORD(BP); PC += 1; goto next_switch;
	case POPsi: POPWORD(SI); PC += 1; goto next_switch;
	case POPdi: POPWORD(DI); PC += 1; goto next_switch;
	case PUSHA: {
	    unsigned int temp;
	    unsigned short tempsp = SP;
	    temp = AX; PUSHWORD(temp);
	    temp = CX; PUSHWORD(temp);
	    temp = DX; PUSHWORD(temp);
	    temp = BX; PUSHWORD(temp);
	    PUSHWORD(tempsp);
	    tempsp = BP;
	    PUSHWORD(tempsp);
	    temp = SI; PUSHWORD(temp);
	    temp = DI; PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case POPA: {
	    unsigned short temp;
	    POPWORD(DI);
	    POPWORD(SI);
	    POPWORD(temp);
	    BP = temp;
	    SP += 2;
	    POPWORD(BX);
	    POPWORD(DX);
	    POPWORD(CX);
	    POPWORD(AX);
	    } PC += 1; goto next_switch;
	case BOUND: 
	case ARPL:
	    PC += 1; goto next_switch;    
	case SEGfs:
#ifdef DOSEMU
	    if (!vm86f && (SHORT_FS_16 == 0 || LONG_FS == (unsigned char *) -1)) {
#else
	    if (SHORT_FS_16 == 0 || LONG_FS == (unsigned char *)-1) {
#endif
		char outbuf[80];
		sprintf(outbuf,
			"General Protection Fault: CS:IP %x:%x zero FS\n",
			SHORT_CS_16,PC-(LONG_CS));
		FatalAppExit(0,outbuf);
	    }
	    OVERRIDE = LONG_FS;
	    PC+=1; goto override;
	case SEGgs:
#ifdef DOSEMU
	    if (!vm86f && (SHORT_GS_16 == 0 || LONG_GS == (unsigned char *) -1)) {
#else
	    if (SHORT_GS_16 == 0 || LONG_GS == (unsigned char *)-1) {
#endif
		char outbuf[80];
		sprintf(outbuf,
			"General Protection Fault: CS:IP %x:%x zero GS\n",
			SHORT_CS_16,PC-(LONG_CS));
		FatalAppExit(0,outbuf);
	    }
	    OVERRIDE = LONG_GS;
	    PC+=1; goto override;
	case OPERoverride:	/* 0x66: 32 bit operand, 16 bit addressing */
	    if (data32) return (PC+1);
	    PC = hsw_interp_32_16 (env, P0, PC+1, interp_var, err);
	    if (*err) return P0;
	    goto next_switch;
	case ADDRoverride:	/* 0x67: 16 bit operand, 32 bit addressing */
	    if (code32) return (PC+1);
	    PC = hsw_interp_16_32 (env, P0, PC+1, interp_var, err);
	    if (*err) return P0;
	    goto next_switch;
	case PUSHwi: {
	    unsigned char *sp = LONG_SS + SP;
	    *(sp - 1) = *(PC + 2);
	    *(sp - 2) = *(PC + 1);
	    SP -= 2;
	    } PC += 3; goto next_switch;
	case IMULwrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = FETCH_WORD(PC); PC += 2; mem_ref = MEM_REF; 
	    if (IS_MODE_REG)
		src1 = FETCH_XREG(mem_ref);
	    else 
		src1 = FETCH_WORD(mem_ref);
	    src1 = ((src1<<16)>>16);
	    src2 = ((src2<<16)>>16);
	    res = src1 * src2;
	    *XREG1 = res; res = res >> 16;
	    SRC1_16 = SRC2_16 = 0;
	    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
	    } goto next_switch; 
	case PUSHbi: {
	    unsigned char *sp = LONG_SS + SP;
	    short temp = (signed char)*(PC+1);
	    *(sp - 2) = temp;
	    *(sp - 1) = temp >> 8;
	    SP -= 2;
	    } PC +=2; goto next_switch;
	case IMULbrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *(signed char *)(PC); PC += 1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG)
		src1 = FETCH_XREG(mem_ref);
	    else
		src1 = FETCH_WORD(mem_ref);
	    src1 = ((src1<<16)>>16);
	    res = src1 * src2;
	    *XREG1 = res; res = res >> 16;
	    SRC1_16 = SRC2_16 = 0;
	    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
	    } goto next_switch; 
	case INSb:
	case INSw:
	case OUTSb:
	case OUTSw:
#ifdef DOSEMU
	    if (vm86f) goto not_permitted;
#endif
            goto not_implemented;
	case JO: if (IS_OF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNO: if (!IS_OF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JB_JNAE: if (IS_CF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNB_JAE: if (!IS_CF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JE_JZ: if (IS_ZF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNE_JNZ: if (!IS_ZF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JBE_JNA: if (IS_CF_SET || IS_ZF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNBE_JA: if (!(IS_CF_SET || IS_ZF_SET)) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JS: if (IS_SF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNS: if (!(IS_SF_SET)) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JP_JPE: if (IS_PF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNP_JPO: if (!IS_PF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JL_JNGE: if (IS_SF_SET ^ IS_OF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNL_JGE: if (!(IS_SF_SET ^ IS_OF_SET)) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JLE_JNG: if ((IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case JNLE_JG: if (!(IS_SF_SET ^ IS_OF_SET) && !(IS_ZF_SET)) {
	    JUMP(PC+1); goto next_switch; 
	    } PC+=2; goto next_switch;
	case IMMEDbrm2:    /* out of order */
	case IMMEDbrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *PC; PC += 1;
	    mem_ref = MEM_REF; src1 = *mem_ref;
	    switch (res) {
		case 0: /* ADD */
		    *mem_ref = res = src1 + src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = src2;
		    goto next_switch;
		case 1: /* OR */
		    *mem_ref = res = src1 | src2;
		    SRC1_8 = res; SRC2_8 = res;
		    RES_32 = res << 8; 
		    BYTE_FLAG=BYTE_OP;
		    goto next_switch;
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *mem_ref = res = src1 + src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = src2;
		    goto next_switch;
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *mem_ref = res = src1 - src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    goto next_switch;
		case 4: /* AND */
		    *mem_ref = res = src1 & src2;
		    SRC1_8 = res; SRC2_8 = res;
		    RES_32 = res << 8; 
		    BYTE_FLAG=BYTE_OP;
		    goto next_switch;
		case 5: /* SUB */
		    *mem_ref = res = src1 - src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    goto next_switch;
		case 6: /* XOR */
		    *mem_ref = res = src1 ^ src2;
		    SRC1_8 = res; SRC2_8 = res;
		    RES_32 = res << 8; 
		    BYTE_FLAG=BYTE_OP;
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res << 8; BYTE_FLAG = BYTE_OP;
		    SRC1_8 = src1; SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    goto next_switch;
	    }}
	case IMMEDwrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = FETCH_WORD(PC); PC += 2;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_XREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    goto next_switch;
		case 1: /* OR */
		    *(unsigned short *)mem_ref = res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    goto next_switch;
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
		case 4: /* AND */
		    *(unsigned short *)mem_ref = res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 5: /* SUB */
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
		case 6: /* XOR */
		    *(unsigned short *)mem_ref = res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_WORD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 1: /* OR */
		    res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 4: /* AND */
		    res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 5: /* SUB */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 6: /* XOR */
		    res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
	      }
	    }}
	case IMMEDisrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = (signed char)*(PC); PC += 1;
	    src2 = src2 & 0xffff;
	    mem_ref = MEM_REF; 
	    if (IS_MODE_REG) { /* register is operand */
	      src1 = FETCH_XREG(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    goto next_switch;
		case 1: /* OR */
		    *(unsigned short *)mem_ref = res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    goto next_switch;
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
		case 4: /* AND */
		    *(unsigned short *)mem_ref = res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 5: /* SUB */
		    *(unsigned short *)mem_ref = res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
		case 6: /* XOR */
		    *(unsigned short *)mem_ref = res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
	      }
	    } else { /* memory is operand */
	      src1 = FETCH_WORD(mem_ref);
	      switch (res) {
		case 0: /* ADD */
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 1: /* OR */
		    res = src1 | src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 2: /* ADC */
		    src2 = src2 + (CARRY & 1);
		    res = src1 + src2;
		    RES_32 = res; SRC1_16 = src1; SRC2_16 = src2;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 3: /* SBB */
		    src2 = src2 + (CARRY & 1);
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 4: /* AND */
		    res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 5: /* SUB */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 6: /* XOR */
		    res = src1 ^ src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    *mem_ref = res; *(mem_ref + 1) = res >> 8;
		    goto next_switch;
		case 7: /* CMP */
		    res = src1 - src2;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    goto next_switch;
	      }
	    }}
	case TESTbrm: {
	    int res, src1, src2;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    src2 = *HREG1; src1 = *MEM_REF;
	    res = src1 & src2;
	    SRC1_8 = res; SRC2_8 = res;
	    RES_32 = res << 8;
	    BYTE_FLAG=BYTE_OP;
	    } goto next_switch; 
	case TESTwrm: {
	    int res, src1, src2; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    src2 = *XREG1; mem_ref = MEM_REF; 
	    if (IS_MODE_REG) {
		src1 = FETCH_XREG(mem_ref);
		res = src1 & src2;
	    } else {
		src1 = FETCH_WORD(mem_ref);
		res = src1 & src2;
	    }
	    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
	    } goto next_switch; 
	case XCHGbrm: {
	    unsigned char temp;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    temp = *MEM_REF; *MEM_REF = *HREG1; *HREG1 = temp;
	    } goto next_switch; 
	case XCHGwrm: {
	    unsigned short temp; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		temp = FETCH_XREG(mem_ref);
		*(unsigned short *)mem_ref = *XREG1;
		*XREG1 = temp;
		goto next_switch; 
	    } else {
		unsigned short temp1 = FETCH_WORD(mem_ref);
		temp = *XREG1; *XREG1 = temp1;
		*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
		goto next_switch; 
	    }
	    }

	case MOVbfrm:
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    *MEM_REF = *HREG1; goto next_switch;
	case MOVwfrm:
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		*(unsigned short *)MEM_REF = *XREG1;
		goto next_switch; 
	    } else {
		unsigned char *mem_ref;
		unsigned short temp = *XREG1;
		mem_ref = MEM_REF;
		*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
		goto next_switch; 
	    }
	case MOVbtrm:
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    *HREG1 = *MEM_REF; goto next_switch;
	case MOVwtrm: {
	    switch (*(PC+1)) {
		case MOD_AX_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_CX_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DX_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BX_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SP_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_BP_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_SI_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_BXSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_BXDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_BPSI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_BPDI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_SI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(SI);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_DI: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(DI);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_DI_imm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BX: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+(BX);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=2; goto next_switch;
		case MOD_AX_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_CX_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DX_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BX_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SP_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_BP_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_SI_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_BXSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_BXDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_BPSIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_BPDIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_SIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_DIimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_BPimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_DI_BXimm8: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=3; goto next_switch;
		case MOD_AX_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    AX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_CX_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    CX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DX_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    DX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BX_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    BX = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SP_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    SP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_BP_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    BP = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_SI_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    SI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BXSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BXDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BPSIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BPDIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_SIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_DIimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BPimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_DI_BXimm16: {
		    unsigned char *mem_ref;
		    mem_ref = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
		    DI = FETCH_WORD(mem_ref);
		    } PC+=4; goto next_switch;
		case MOD_AX_EW_AX:
		    PC+=2; goto next_switch;
		case MOD_AX_EW_CX:
		    AX = CX;
		    PC+=2; goto next_switch;
		case MOD_AX_EW_DX:
		    AX = DX;
		    PC+=2; goto next_switch;
		case MOD_AX_EW_BX:
		    AX = BX;
		    PC+=2; goto next_switch;
		case MOD_AX_EW_SP:
		    AX = SP;
		    PC+=2; goto next_switch;
		case MOD_AX_EW_BP:
		    AX = BP;
		    PC+=2; goto next_switch;
		case MOD_AX_EW_SI:
		    AX = SI;
		    PC+=2; goto next_switch;
		case MOD_AX_EW_DI:
		    AX = DI;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_AX:
		    CX = AX;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_CX:
		    PC+=2; goto next_switch;
		case MOD_CX_EW_DX:
		    CX = DX;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_BX:
		    CX = BX;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_SP:
		    CX = SP;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_BP:
		    CX = BP;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_SI:
		    CX = SI;
		    PC+=2; goto next_switch;
		case MOD_CX_EW_DI:
		    CX = DI;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_AX:
		    DX = AX;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_CX:
		    DX = CX;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_DX:
		    PC+=2; goto next_switch;
		case MOD_DX_EW_BX:
		    DX = BX;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_SP:
		    DX = SP;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_BP:
		    DX = BP;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_SI:
		    DX = SI;
		    PC+=2; goto next_switch;
		case MOD_DX_EW_DI:
		    DX = DI;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_AX:
		    BX = AX;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_CX:
		    BX = CX;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_DX:
		    BX = DX;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_BX:
		    PC+=2; goto next_switch;
		case MOD_BX_EW_SP:
		    BX = SP;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_BP:
		    BX = BP;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_SI:
		    BX = SI;
		    PC+=2; goto next_switch;
		case MOD_BX_EW_DI:
		    BX = DI;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_AX:
		    SP = AX;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_CX:
		    SP = CX;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_DX:
		    SP = DX;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_BX:
		    SP = BX;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_SP:
		    PC+=2; goto next_switch;
		case MOD_SP_EW_BP:
		    SP = BP;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_SI:
		    SP = SI;
		    PC+=2; goto next_switch;
		case MOD_SP_EW_DI:
		    SP = DI;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_AX:
		    BP = AX;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_CX:
		    BP = CX;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_DX:
		    BP = DX;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_BX:
		    BP = BX;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_SP:
		    BP = SP;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_BP:
		    PC+=2; goto next_switch;
		case MOD_BP_EW_SI:
		    BP = SI;
		    PC+=2; goto next_switch;
		case MOD_BP_EW_DI:
		    BP = DI;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_AX:
		    SI = AX;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_CX:
		    SI = CX;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_DX:
		    SI = DX;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_BX:
		    SI = BX;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_SP:
		    SI = SP;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_BP:
		    SI = BP;
		    PC+=2; goto next_switch;
		case MOD_SI_EW_SI:
		    PC+=2; goto next_switch;
		case MOD_SI_EW_DI:
		    SI = DI;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_AX:
		    DI = AX;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_CX:
		    DI = CX;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_DX:
		    DI = DX;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_BX:
		    DI = BX;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_SP:
		    DI = SP;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_BP:
		    DI = BP;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_SI:
		    DI = SI;
		    PC+=2; goto next_switch;
		case MOD_DI_EW_DI:
		    PC+=2; goto next_switch;
	    }
	    } goto next_switch;
	case MOVsrtrm: {
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) {
		switch (seg_reg) {
		    case 0: /* ES */
			*(unsigned short *)MEM_REF = SHORT_ES_16;
			goto next_switch;
		    case 1: /* CS */
			*(unsigned short *)MEM_REF = SHORT_CS_16;
			goto next_switch;
		    case 2: /* SS */
			*(unsigned short *)MEM_REF = SHORT_SS_16;
			goto next_switch;
		    case 3: /* DS */
			*(unsigned short *)MEM_REF = SHORT_DS_16;
			goto next_switch;
		    case 4: /* FS */
			*(unsigned short *)MEM_REF = SHORT_FS_16;
			goto next_switch;
		    case 5: /* GS */
			*(unsigned short *)MEM_REF = SHORT_GS_16;
			goto next_switch;
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
			goto next_switch;
		    case 1: /* CS */
			temp = SHORT_CS_16;
			*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			goto next_switch;
		    case 2: /* SS */
			temp = SHORT_SS_16;
			*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			goto next_switch;
		    case 3: /* DS */
			temp = SHORT_DS_16;
			*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			goto next_switch;
		    case 4: /* FS */
			temp = SHORT_FS_16;
			*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			goto next_switch;
		    case 5: /* GS */
			temp = SHORT_GS_16;
			*mem_ref = temp; *(mem_ref + 1) = temp >> 8;
			goto next_switch;
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
		    case 0: temp = BX + SI; break;
		    case 1: temp = BX + DI; break;
		    case 2: temp = BP + SI; break;
		    case 3: temp = BP + DI; break;
		    case 4: temp = SI; break;
		    case 5: temp = DI; break;
		    case 6: PC += 2; temp = FETCH_WORD(PC); break;
		    case 7: temp = BX; break;
		  } /*end switch */ PC += 2; break;
		case 1:
		  switch (modrm & 7) {
		    case 0: temp = BX + SI + *(signed char *)(PC+2); break;
		    case 1: temp = BX + DI + *(signed char *)(PC+2); break;
		    case 2: temp = BP + SI + *(signed char *)(PC+2); break;
		    case 3: temp = BP + DI + *(signed char *)(PC+2); break;
		    case 4: temp = SI + *(signed char *)(PC+2); break;
		    case 5: temp = DI + *(signed char *)(PC+2); break;
		    case 6: temp = BP + *(signed char *)(PC+2); break;
		    case 7: temp = BX + *(signed char *)(PC+2); break;
		  } /*end switch */ PC += 3; break;
		case 2:
		  switch (modrm & 7) {
		    case 0: temp = BX + SI + FETCH_WORD(PC+2); break;
		    case 1: temp = BX + DI + FETCH_WORD(PC+2); break;
		    case 2: temp = BP + SI + FETCH_WORD(PC+2); break;
		    case 3: temp = BP + DI + FETCH_WORD(PC+2); break;
		    case 4: temp = SI + FETCH_WORD(PC+2); break;
		    case 5: temp = DI + FETCH_WORD(PC+2); break;
		    case 6: temp = BP + FETCH_WORD(PC+2); break;
		    case 7: temp = BX + FETCH_WORD(PC+2); break;
		  } /*end switch */ PC += 4; break;
		case 3:
		  switch (modrm & 7) {
		    case 0: temp = AX; break;
		    case 1: temp = CX; break;
		    case 2: temp = DX; break;
		    case 3: temp = BX; break;
		    case 4: temp = SP; break;
		    case 5: temp = BP; break;
		    case 6: temp = SI; break;
		    case 7: temp = DI; break;
		  } /*end switch */ PC += 2; break;
	    } /* end switch */
	    switch ((modrm >> 3) & 7) {
		case 0: AX = temp; goto next_switch;
		case 1: CX = temp; goto next_switch;
		case 2: DX = temp; goto next_switch;
		case 3: BX = temp; goto next_switch;
		case 4: SP = temp; goto next_switch;
		case 5: BP = temp; goto next_switch;
		case 6: SI = temp; goto next_switch;
		case 7: DI = temp; goto next_switch;
	    } /* end switch */ }
	case MOVsrfrm: {
	    unsigned short temp;
	    unsigned char seg_reg = (*(PC + 1) >> 3) & 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
		    goto next_switch;
		case 1: /* CS */
		    SHORT_CS_16 = temp;
		    SET_SEGREG(LONG_CS,temp);
		    goto next_switch;
		case 2: /* SS */
		    SHORT_SS_16 = temp;
		    SET_SEGREG(LONG_SS,temp);
		    goto next_switch;
		case 3: /* DS */
		    SHORT_DS_16 = temp;
		    SET_SEGREG(LONG_DS,temp);
		    goto next_switch;
		case 4: /* FS */
		    SHORT_FS_16 = temp;
		    SET_SEGREG(LONG_FS,temp);
		    goto next_switch;
		case 5: /* GS */
		    SHORT_GS_16 = temp;
		    SET_SEGREG(LONG_GS,temp);
		    goto next_switch;
		case 6: /* Illegal */
		case 7: /* Illegal */
		    goto illegal_op;
		    /* trap this */
	    }}
	case POPrm: {
	    unsigned char *mem_ref, *sp;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    mem_ref = MEM_REF; sp = LONG_SS + SP;
	    *mem_ref = *sp; *(mem_ref+1) = *(sp+1);
	    SP += 2;
	    } goto next_switch;

	case NOP:
	    PC += 1; goto next_switch;
	case XCHGcx: {
	    unsigned short temp = AX;
	    AX = CX;
	    CX = temp;
	    } PC += 1; goto next_switch;
	case XCHGdx: {
	    unsigned short temp = AX;
	    AX = DX;
	    DX = temp;
	    } PC += 1; goto next_switch;
	case XCHGbx: {
	    unsigned short temp = AX;
	    AX = BX;
	    BX = temp;
	    } PC += 1; goto next_switch;
	case XCHGsp: {
	    unsigned short temp = AX;
	    AX = SP;
	    SP = temp;
	    } PC += 1; goto next_switch;
	case XCHGbp: {
	    unsigned short temp = AX;
	    AX = BP;
	    BP = temp;
	    } PC += 1; goto next_switch;
	case XCHGsi: {
	    unsigned short temp = AX;
	    AX = SI;
	    SI = temp;
	    } PC += 1; goto next_switch;
	case XCHGdi: {
	    unsigned short temp = AX;
	    AX = DI;
	    DI = temp;
	    } PC += 1; goto next_switch;
	case CBW:
	    AH = (AL & 0x80) ? 0xff : 0;
	    PC += 1; goto next_switch;
	case CWD:
	    DX = (AX & 0x8000) ? 0xffff : 0;
	    PC += 1; goto next_switch;
	case CALLl: {
	    unsigned int cs = SHORT_CS_16;
	    unsigned int ip = PC - LONG_CS + 5;
	    unsigned short transfer_magic;
	    PUSHWORD(cs);
	    PUSHWORD(ip);
	    env->return_addr = (cs << 16)|ip;
	    ip = (FETCH_WORD(PC+1) & 0xffff);
	    cs = FETCH_WORD(PC+3);
	    transfer_magic = (WORD)GetSelectorType(cs);
#ifdef DOSEMU
	    if (vm86f || (transfer_magic == TRANSFER_CODE16)) {
#else
	    if (transfer_magic == TRANSFER_CODE16) {
#endif
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		goto next_switch;
	    }
#ifdef DOSEMU
	    if (vm86f || (transfer_magic == TRANSFER_CALLBACK) || 
#else
	    if ((transfer_magic == TRANSFER_CALLBACK) || 
#endif
	    		(transfer_magic == TRANSFER_BINARY))  {
		LONGPROC conv,targ;
		SEGIMAGE *lpSegImage = &((SEGIMAGE *)
	    		(*(long *)(SELECTOR_PADDRESS(cs))))[ip>>3];
		targ = (LONGPROC)lpSegImage->targ;
		conv = (LONGPROC)lpSegImage->conv;
		EBP = (long)LONG_SS + EBP;
		ESP = (long)LONG_SS + ESP;
		trans_interp_flags(env, interp_var);    
#ifdef DOSEMU
		if (vm86f||(transfer_magic == TRANSFER_CALLBACK)) {
#else
		if (transfer_magic == TRANSFER_CALLBACK) {
#endif
		    env->trans_addr = (BINADDR)MAKELONG(ip,cs);
#ifdef	TRACE
		LogProcName(cs,ip,1);
#else
#ifdef DEBUG
		    LOGSTR((LF_DEBUG,"do_ext: %s %x:%x\n",
			GetProcName(cs,ip>>3),
			env->return_addr >> 16,
			(env->return_addr & 0xffff)-5));
#endif
#endif	/* TRACE */
		}
#ifdef DEBUG
		else    /* TRANSFER_BINARY */
		    LOGSTR((LF_DEBUG,
			    "do_ext: calling binary thunk %x:%x\n",cs,ip));
#endif
		(conv)(env,targ);
#ifdef	TRACE
		LogProcName(AX,DX,0);
#endif
		SHORT_CS_16 = cs = env->return_addr >> 16;
		ip = env->return_addr & 0xffff;
		SET_SEGREG(LONG_CS,cs);
		SET_SEGREG(LONG_DS,SHORT_DS_16);
		SET_SEGREG(LONG_ES,SHORT_ES_16);
		SET_SEGREG(LONG_SS,SHORT_SS_16);
		EBP = EBP - (long)LONG_SS;
		ESP = ESP - (long)LONG_SS;
		trans_flags_to_interp(env, interp_var, env->flags);
		PC = LONG_CS + ip; goto next_switch;
	    }
	    if (transfer_magic == TRANSFER_RETURN) {
		SHORT_CS_16 = cs;
		env->return_addr = (cs << 16) | ip;
#ifndef DOSEMU
		trans_interp_flags(env, interp_var);    
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
#endif
		return PC;
	    }
	    invoke_data(env);    /* TRANSFER_DATA or garbage */
        }
	case WAIT:
	    PC += 1; goto next_switch;
	case PUSHF: {
	    unsigned int temp;
#ifdef DOSEMU
	    if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK))
		goto not_permitted;
#endif
	    temp =     trans_interp_flags(env, interp_var);    
	    PUSHWORD(temp);
	    } PC += 1; goto next_switch;
	case POPF: {
	    unsigned int temp;
#ifdef DOSEMU
	    if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK))
		goto not_permitted;
#endif
	    POPWORD(temp);
	    trans_flags_to_interp(env, interp_var, temp);
	    } PC += 1; goto next_switch;
	case SAHF: {
	    trans_flags_to_interp(env, interp_var, AH);
	    } PC += 1; goto next_switch;
	case LAHF:
	    AH = trans_interp_flags(env, interp_var);    
	    PC += 1; goto next_switch;
	case MOVmal: {
	    unsigned char *mem_ref;;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    AL = *mem_ref;
	    } PC += 3; goto next_switch;
	case MOVmax: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    AX = FETCH_WORD(mem_ref);
	    } PC += 3; goto next_switch;
	case MOValm: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    *mem_ref = AL;
	    } PC += 3; goto next_switch;
	case MOVaxm: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + FETCH_WORD((PC+1));
	    *mem_ref = AL;
	    *(mem_ref + 1) = AH;
	    } PC += 3; goto next_switch;
	case MOVSb: {
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    *dest = *src;
	    (env->flags & DIRECTION_FLAG)?(SI--,DI--):(SI++,DI++);
	    } PC += 1; goto next_switch;
	case MOVSw: {
	    unsigned char *src, *dest;
	    int temp;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    temp = FETCH_WORD(src);
	    PUT_WORD(dest, temp);
	    (env->flags & DIRECTION_FLAG)?(SI-=2,DI-=2):(SI+=2,DI+=2);
	    } PC += 1; goto next_switch;
	case CMPSb: {
	    unsigned int res, src1, src2;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    src1 = *src;
	    src2 = *dest;
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(SI--,DI--):(SI++,DI++);
	    RES_32 = res << 8; SRC1_8 = src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); BYTE_FLAG = BYTE_OP;
	    } PC += 1; goto next_switch;
	case CMPSw: {
	    unsigned int res, src1, src2;
	    unsigned char *src, *dest;
	    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    dest = LONG_ES + (DI);
	    src1 = FETCH_WORD(src);
	    src2 = FETCH_WORD(dest);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(SI-=2,DI-=2):(SI+=2,DI+=2);
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 1; goto next_switch;
	case TESTbi: {
	    unsigned int res = AL & (*(PC+1));
	    SRC1_32 = SRC2_32 = RES_32 = res << 8; BYTE_FLAG = BYTE_OP;    
	    } PC += 2; goto next_switch;
	case TESTwi: {
	    unsigned int res;
	    res = FETCH_WORD((PC+1));
	    SRC1_32 = SRC2_32 = RES_32 = AX & res;
	    } PC += 3; goto next_switch;
	case STOSb:
	    LONG_ES[DI] = AL;
	    (env->flags & DIRECTION_FLAG)?DI--:DI++;
	    PC += 1; goto next_switch;
	case STOSw:
	    LONG_ES[DI] = AL;
	    LONG_ES[DI+1] = AH;
	    (env->flags & DIRECTION_FLAG)?(DI-=2):(DI+=2);
	    PC += 1; goto next_switch;
	case LODSb: {
	    unsigned char *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    AL = *seg;
	    (env->flags & DIRECTION_FLAG)?SI--:SI++;
	    } PC += 1; goto next_switch;
	case LODSw: {
	    unsigned char *seg;
	    seg = ALLOW_OVERRIDE(LONG_DS) + (SI);
	    AX = FETCH_WORD(seg);
	    (env->flags & DIRECTION_FLAG)?(SI-=2):(SI+=2);
	    } PC += 1; goto next_switch;
	case SCASb: {
	    unsigned int res, src1, src2;
	    src1 = AL;
	    src2 = (LONG_ES[DI]);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?DI--:DI++;
	    RES_32 = res << 8; SRC1_8 = src1;
	    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); BYTE_FLAG = BYTE_OP;
	    } PC += 1; goto next_switch;
	case SCASw: {
	    unsigned int res, src1, src2;
	    unsigned char *mem_ref;
	    src1 = AX;
	    mem_ref = LONG_ES + (DI);
	    src2 = FETCH_WORD(mem_ref);
	    res = src1 - src2;
	    (env->flags & DIRECTION_FLAG)?(DI-=2):(DI+=2);
	    RES_32 = res; SRC1_16 = src1;
	    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
	    } PC += 1; goto next_switch;
	case MOVial:
	    AL = *(PC+1);
	     PC += 2; goto next_switch;
	case MOVicl:
	    CL = *(PC+1);
	    PC += 2; goto next_switch;
	case MOVidl:
	    DL = *(PC+1);
	    PC += 2; goto next_switch;
	case MOVibl:
	    BL = *(PC+1);
	    PC += 2; goto next_switch;
	case MOViah:
	    AH = *(PC+1);
	    PC += 2; goto next_switch;
	case MOVich:
	    CH = *(PC+1);
	    PC += 2; goto next_switch;
	case MOVidh:
	    DH = *(PC+1);
	    PC += 2; goto next_switch;
	case MOVibh:
	    BH = *(PC+1);
	    PC += 2; goto next_switch;
	case MOViax:
	    AX = FETCH_WORD((PC+1));
	    PC += 3; goto next_switch;
	case MOVicx:
	    CX = FETCH_WORD((PC+1));
	    PC += 3; goto next_switch;
	case MOVidx:
	    DX = FETCH_WORD((PC+1));
	    PC += 3; goto next_switch;
	case MOVibx:
	    BX = FETCH_WORD((PC+1));
	    PC += 3; goto next_switch;
	case MOVisp:
	    SP = FETCH_WORD((PC+1)); 
	    PC += 3; goto next_switch;
	case MOVibp:
	    BP = FETCH_WORD((PC+1)); 
	    PC += 3; goto next_switch;
	case MOVisi:
	    SI = FETCH_WORD((PC+1));
	    PC += 3; goto next_switch;
	case MOVidi:
	    DI = FETCH_WORD((PC+1));
	    PC += 3; goto next_switch;
	case SHIFTbi: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
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
			goto next_switch;
		    case 5: /* SHR */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
			goto next_switch;
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
			goto next_switch;
		} } else  goto next_switch;
	    }
	case SHIFTwi: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	/*		temp = FETCH_WORD(mem_ref); */
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
			goto next_switch;
		    case 5: /* SHR */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			goto next_switch;
		    case 6: /* Illegal */
			goto illegal_op;
		    case 7: /* SAR */
			temp = *(signed short *)reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			goto next_switch;
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
			goto next_switch;
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			PUT_WORD(mem_ref,temp);
			goto next_switch;
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
			goto next_switch;
	      } } } else  goto next_switch;
	    }
	case RETisp: {
	    unsigned int ip;
	    POPWORD(ip);
	    SP += (signed short)(FETCH_WORD((PC+1)));
	    PC = LONG_CS + ip;
	    } goto next_switch;
	case RET: {
	    unsigned int ip;
	    POPWORD(ip);
	    PC = LONG_CS + ip;
	    } goto next_switch;
	case LES: {
	    int temp; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch;
	case LDS: {
	    int temp; unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
	    } goto next_switch;
	case MOVbirm:
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    *MEM_REF = *PC; PC += 1; goto next_switch;
	case MOVwirm: {
	    /*int temp;*/ unsigned char *mem_ref;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		*(unsigned short *)mem_ref = FETCH_WORD(PC);
		PC += 2; goto next_switch;
	    } else {
		*mem_ref = *PC; *(mem_ref+1)= *(PC+1);
		PC += 2; goto next_switch;
	    } } 
	case ENTER: {
	    unsigned char *sp, *bp, *ss;
	    unsigned int temp;
	    unsigned char level = *(PC+3) & 0x1f;
	    sp = LONG_SS + SP;
	    bp = LONG_SS + BP;
	    ss = LONG_SS;
	    temp = bp - ss;
	    sp -= 2;
	    *sp = temp; *(sp+1) = temp >> 8;
	    temp = sp - ss;
	    if (level) {
		while (level--) {
		    bp -= 2; sp -= 2;
		    *sp = *bp; *(sp+1) = *(bp+1);
		}
		*(sp - 2) = temp; *(sp - 1) = temp >> 8;
	    }
	    BP = temp;
	    sp = ss + (temp - (FETCH_WORD((PC + 1))));
	    SP = sp - LONG_SS;
	    } PC += 4; goto next_switch;
	case LEAVE: {   /* 0xc9 */
	    unsigned short temp;
	    SP = BP;
	    POPWORD(temp);
	    BP = temp;
	    } PC += 1; goto next_switch;
	case RETlisp: {
	    unsigned int cs, ip;
	    unsigned char *sp = LONG_SS + SP;
	    ip = (FETCH_WORD(sp) & 0xffff);
	    cs = FETCH_WORD(sp+2);
	    SP = ((sp + 4) + FETCH_WORD((PC+1))) - LONG_SS;
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		goto next_switch;
	    } else {
		env->return_addr = ip | (cs << 16);
#ifndef DOSEMU
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
#endif
		return PC;
	    }
	    }
	case RETl: {
	    unsigned int cs, ip;
	    unsigned char *sp = LONG_SS + SP;
	    ip = (FETCH_WORD(sp) & 0xffff);
	    cs = FETCH_WORD(sp+2);
	    SP += 4;
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		goto next_switch;
	    } else {
		env->return_addr = ip | (cs << 16);
#ifndef DOSEMU
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
#endif
		return PC;
	    }
	    } 
	case INT3:
#ifdef DOSEMU
	    *err=EXCP03_INT3; return P0;
#else
	    fprintf(stderr,unsupp_msg,*PC,SHORT_CS_16,PC-LONG_CS);
	    PC += 1; goto next_switch;
#endif
	case INT:
#ifdef DOSEMU
	    *err=EXCP0D_GPF; return P0;
#else
	    { unsigned int temp, cs, ip = (unsigned int)(PC - LONG_CS);
	    PUSHWORD(ip);
	    cs = SHORT_CS_16;
	    PUSHWORD(cs);
	    env->return_addr = (cs << 16)|ip;
	    temp =     trans_interp_flags(env, interp_var);    
	    PUSHWORD(temp);
	    EBP = EBP + (long)LONG_SS;
	    ESP = ESP + (long)LONG_SS;
	    INT_handler((unsigned int)(*(PC+1)),(ENV *)env);
	    EBP = EBP - (long)LONG_SS;
	    ESP = ESP - (long)LONG_SS;
	    trans_flags_to_interp(env, interp_var, env->flags);
	    } PC += 2; goto next_switch;
#endif
	case INTO:
#ifdef DOSEMU
	    *err=EXCP04_INTO; return P0;
#else
	    fprintf(stderr,unsupp_msg,*PC,SHORT_CS_16,PC-LONG_CS);
	    PC += 1; goto next_switch;
#endif
	case IRET:
#ifdef DOSEMU
	    if (vm86f) goto not_permitted;
	    else
#endif
	    {
	    unsigned int cs, ip, flags;
	    unsigned char *sp = LONG_SS + SP;
	    ip = *(sp) | (*(sp + 1) >> 8);
	    cs = *(sp + 2) | (*(sp + 3) >> 8);
	    flags = *(sp + 4) | (*(sp + 5) >> 8);
	    SP += 6;
	    trans_flags_to_interp(env, interp_var, flags);
	    if (GetSelectorType(cs) == TRANSFER_CODE16) {
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		goto next_switch;
	    } else {
		env->return_addr = ip | (cs << 16);
#ifndef DOSEMU
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
#endif
		return PC;
	    }
	    } 
	case SHIFTb: {
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
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
		    goto next_switch;
		case 5: /* SHR */
		    temp = *mem_ref; 
		    SRC1_8 = SRC2_8 = temp;
		    CARRY = temp & 1;
		    temp = (temp >> 1);
		    *mem_ref = temp;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    goto next_switch;
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
		    goto next_switch;
		} } 
	case SHIFTw: {
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) {
		unsigned short *reg = (unsigned short *)MEM_REF;
		switch (temp) {
		    case 0: /* ROL */
			temp = *reg;
/*			temp = FETCH_WORD(mem_ref); */
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
			goto next_switch;
		    case 5: /* SHR */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			*reg = temp;
			goto next_switch;
		    case 6: /* Illegal */
                	goto illegal_op; 
		    case 7: /* SAR */
			temp = *(signed short *)reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			*reg = temp;
			goto next_switch;
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
			goto next_switch;
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = temp & 1;
			temp = (temp >> 1);
			RES_16 = temp;
			PUT_WORD(mem_ref, temp);
			goto next_switch;
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
			goto next_switch;
	      } }
	    }
	case SHIFTbv: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
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
			goto next_switch;
		    case 5: /* SHR */
			temp = *mem_ref; 
			SRC1_8 = SRC2_8 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			*mem_ref = temp;
			RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
			goto next_switch;
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
			goto next_switch;
		} } else  goto next_switch;
	    }
	case SHIFTwv: {
	    int temp, count; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
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
/*			temp = FETCH_WORD(mem_ref); */
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
			goto next_switch;
		    case 5: /* SHR */
			temp = *reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			goto next_switch;
		    case 6: /* Illegal */
                	goto illegal_op; 
		    case 7: /* SAR */
			temp = *(signed short *)reg;
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			*reg = temp;
			goto next_switch;
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
			goto next_switch;
		    case 5: /* SHR */
			temp = FETCH_WORD(mem_ref); 
			SRC1_16 = SRC2_16 = temp;
			CARRY = (temp >> (count-1)) & 1;
			temp = (temp >> count);
			RES_16 = temp;
			PUT_WORD(mem_ref, temp);
			goto next_switch;
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
			goto next_switch;
	      } } } else  goto next_switch;
	    }
	case AAM: {
	    unsigned int al, ax;
	    al = AL;
	    ax = ((al / 10) << 8) + (al % 10);
	    AX = ax;
	    RES_8 = AL; /* for flags */
	    BYTE_FLAG=BYTE_OP;
	    } PC += 1; goto next_switch;
	case AAD: {
	    unsigned int al, ax, ah;
	    al = AL; ah = AH;
	    ax = (0xff & ((ah * 10) + al));
	    AX = ax;
	    RES_8 = AL; /* for flags */
	    BYTE_FLAG=BYTE_OP;
	    } PC += 1; goto next_switch;
	case 0xd6:    /* illegal on 8086 and 80x86*/
	    goto illegal_op;
	case XLAT: {
	    unsigned char *mem_ref;
	    mem_ref = ALLOW_OVERRIDE(LONG_DS) + (BX) + (AL);
	    AL = *mem_ref; }
	    PC += 1; goto next_switch;
#ifdef DEBUG
   case ESC0:
   case ESC1:
   case ESC2:
   case ESC3:
   case ESC4:
   case ESC5:
   case ESC6:
   case ESC7:
   { int ifpr;
#ifdef DOSEMU
	e_printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %ld\n",
		SHORT_CS_16, PC - (LONG_CS), *PC, *(PC + 1), AX, BX, CX, DX, SI, DI, BP, SP, decode (*PC, *(PC + 1)), instr_count);
	for (ifpr = 0; ifpr < 4; ifpr++)
	  e_printf(" %18.8g ", hsw_env87.fpregs[ifpr]);
	e_printf("\n");
	for (ifpr = 4; ifpr < 8; ifpr++)
	  e_printf(" %18.8g ", hsw_env87.fpregs[ifpr]);
	e_printf("\n");
	e_printf(" sw cw tag %4x %4x %4x\n", hsw_env87.fpus, hsw_env87.fpuc, hsw_env87.fpstt);
#else
    if(float_print){
            /*      address   opcode     ax   bx   cx   dx   si   di  bp  sp*/
            if(!(print && (!(instr_count % granularity))))printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS),*PC, *(PC+1), AX, BX, CX, DX, SI, DI, BP, SP, decode(*PC, *(PC+1)), instr_count);
	for(ifpr=0;ifpr<4;ifpr++) printf(" %18.8g ",hsw_env87.fpregs[ifpr]); printf("\n");
	for(ifpr=4;ifpr<8;ifpr++) printf(" %18.8g ",hsw_env87.fpregs[ifpr]); printf("\n");
	printf(" sw cw tag %4x %4x %4x\n",hsw_env87.fpus,hsw_env87.fpuc,hsw_env87.fpstt);
        fflush(stdout);
    }
#endif
   }
   switch(*PC){
#endif	/* DEBUG */
	case ESC0: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg0[funct](reg);
	    else hsw_fp87_mem0[funct](MEM_REF);
	    } goto next_switch;
	case ESC1: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg1[funct](reg);
	    else hsw_fp87_mem1[funct](MEM_REF);
	    } goto next_switch;
	case ESC2: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg2[funct](reg);
	    else hsw_fp87_mem2[funct](MEM_REF);
	    } goto next_switch;
	case ESC3: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg3[funct](reg);
	    else hsw_fp87_mem3[funct](MEM_REF);
	    } goto next_switch;
	case ESC4: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg4[funct](reg);
	    else hsw_fp87_mem4[funct](MEM_REF);
	    } goto next_switch;
	case ESC5: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg5[funct](reg);
	    else hsw_fp87_mem5[funct](MEM_REF);
	    } goto next_switch;
	case ESC6: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg6[funct](reg);
	    else hsw_fp87_mem6[funct](MEM_REF);
	    } goto next_switch;
	case ESC7: {
	    int reg = (*(PC+1) & 7);
	    unsigned int funct = (unsigned int)(*(PC+1) & 0x38) >> 3;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    if (IS_MODE_REG) hsw_fp87_reg7[funct](reg);
	    else hsw_fp87_mem7[funct](MEM_REF);
	    } goto next_switch;
#ifdef DEBUG
    }
#endif
	case LOOPNZ_LOOPNE: 
	    if ((--CX != 0) && (!IS_ZF_SET)) {
#if defined(DOSEMU) && defined(DEBUG)
		loop_PC = (long)PC;
#endif
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
	case LOOPZ_LOOPE: 
	    if ((--CX != 0) && (IS_ZF_SET)) {
#if defined(DOSEMU) && defined(DEBUG)
		loop_PC = (long)PC;
#endif
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
	case LOOP: 
	    if (--CX != 0) {
#if defined(DOSEMU) && defined(DEBUG)
		loop_PC = (long)PC;
#endif
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;
	case JCXZ: 
	    if (CX == 0) {
		JUMP((PC+1)); goto next_switch;
	    } PC += 2; goto next_switch;

	case INb: {
#ifdef DOSEMU
	      DWORD a = *(PC+1);
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if (!test_bit(a, io_bitmap)) goto not_permitted;
	      }
#endif
	      AL = PortIO((DWORD)*(++PC), 0, 8, FALSE);
	    }
	    goto io_switch;
	case INw: {
#ifdef DOSEMU
	      DWORD a = *(PC+1);
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((a&1)||(!test_bit(a+1, io_bitmap))) goto not_permitted;
	      }
#endif
	      AX = PortIO((DWORD)*(++PC), 0, 16, FALSE);
	    }
	    goto io_switch;
	case OUTb: {
#ifdef DOSEMU
	      DWORD a = *(PC+1);
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if (!test_bit(a, io_bitmap)) goto not_permitted;
	      }
#endif
	      PortIO((DWORD)*(++PC), AL, 8, TRUE);
	    }
	    goto io_switch;
	case OUTw: {
#ifdef DOSEMU
	      DWORD a = *(PC+1);
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((a&1)||(!test_bit(a+1, io_bitmap))) goto not_permitted;
	      }
#endif
	      PortIO((DWORD)*(++PC), AX, 16, TRUE);
	    }
	    goto io_switch;
	case INvb: {
#ifdef DOSEMU
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((DX>0x3ff)||(!test_bit(DX, io_bitmap))) goto not_permitted;
	      }
#endif
	      AL = PortIO(DX, 0, 8, FALSE);
	    }
	    goto io_switch;
	case INvw: {
#ifdef DOSEMU
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((DX>0x3ff)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
	      }
#endif
	      AX = PortIO(DX, 0, 16, FALSE);
	    }
	    goto io_switch;
	case OUTvb: {
#ifdef DOSEMU
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((DX>0x3ff)||(!test_bit(DX, io_bitmap))) goto not_permitted;
	      }
#endif
	      PortIO(DX, AL, 8, TRUE);
	    }
	    goto io_switch;
	case OUTvw: {
#ifdef DOSEMU
	      if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK)) {
		if ((DX>0x3ff)||(DX&1)||(!test_bit(DX+1, io_bitmap))) goto not_permitted;
	      }
#endif
	      PortIO(DX, AX, 16, TRUE);
	    }
io_switch:
	    PC += 1;
	    goto next_switch;

	case CALLd: {
	    unsigned int ip = PC - LONG_CS + 3;
	    PUSHWORD(ip);
	    PC = LONG_CS + ((ip + (signed short)FETCH_WORD((PC+1))) & 0xffff);
	    } goto next_switch;
	case JMPd: {
	    unsigned int ip = PC - LONG_CS + 3;
	    PC = LONG_CS + ((ip + (signed short)FETCH_WORD((PC+1))) & 0xffff);
	    } goto next_switch;
	case JMPld: {
	    unsigned int cs, ip;
	    unsigned short transfer_magic;
	    ip = FETCH_WORD(PC+1);
	    cs = (FETCH_WORD(PC+3) & 0xffff);
	    SHORT_CS_16 = cs;
	    transfer_magic = (WORD)GetSelectorType(cs);
#ifdef DOSEMU
	    if (vm86f || (transfer_magic == TRANSFER_CODE16)) {
#else
	    if (transfer_magic == TRANSFER_CODE16) {
#endif
		SHORT_CS_16 = cs;
		SET_SEGREG(LONG_CS,cs);
		PC = ip + LONG_CS;
		goto next_switch;
	    }
#ifdef DOSEMU
	    if (vm86f || (transfer_magic == TRANSFER_CALLBACK) || 
#else
	    if ((transfer_magic == TRANSFER_CALLBACK) || 
#endif
	    		(transfer_magic == TRANSFER_BINARY))  {
		LONGPROC conv,targ;
		SEGIMAGE *lpSegImage = &((SEGIMAGE *)
	    		(*(long *)(SELECTOR_PADDRESS(cs))))[ip>>3];
		targ = (LONGPROC)lpSegImage->targ;
		conv = (LONGPROC)lpSegImage->conv;
		EBP = (long)LONG_SS + EBP;
		ESP = (long)LONG_SS + ESP;
		trans_interp_flags(env, interp_var);   
#ifdef DOSEMU
		if (vm86f||(transfer_magic == TRANSFER_CALLBACK))
#else
		if (transfer_magic == TRANSFER_CALLBACK)
#endif
		    env->trans_addr = (BINADDR)MAKELONG(ip,cs);
#ifdef	TRACE
		LogProcName(cs,ip,1);
#else
#ifdef DEBUG
#ifdef DOSEMU
		if (vm86f||(transfer_magic == TRANSFER_CALLBACK))
#else
		if (transfer_magic == TRANSFER_CALLBACK)
#endif
		    LOGSTR((LF_DEBUG,
			    "do_ext: jump to %s\n", GetProcName(cs,ip>>3)));
		else    /* TRANSFER_BINARY */
		    LOGSTR((LF_DEBUG,
			    "do_ext: jumping to binary thunk %x:%x\n",cs,ip));
#endif
#endif	/* TRACE */
		(conv)(env,targ);
#ifdef	TRACE
		LogProcName(AX,DX,0);
#endif
                SHORT_CS_16 = cs = env->return_addr >> 16;
                ip = env->return_addr & 0xffff;
		SET_SEGREG(LONG_CS,SHORT_CS_16);
		SET_SEGREG(LONG_DS,SHORT_DS_16);
		SET_SEGREG(LONG_ES,SHORT_ES_16);
		SET_SEGREG(LONG_SS,SHORT_SS_16);
                EBP = EBP - (long)LONG_SS;
                ESP = ESP - (long)LONG_SS;
                trans_flags_to_interp(env, interp_var, env->flags);
                PC = LONG_CS + ip; goto next_switch;
	    }
	    if (transfer_magic == TRANSFER_RETURN) {
		env->return_addr = (cs << 16) | ip;
#ifndef DOSEMU
		trans_interp_flags(env, interp_var);
		EBP = EBP + (long)LONG_SS;
		ESP = ESP + (long)LONG_SS;
#endif
		return PC;
	    }
	    invoke_data(env);    /* TRANSFER_DATA or garbage */
	}
	case JMPsid: 
	JUMP((PC+1)); goto next_switch;
	case LOCK:
	    PC += 1; goto next_switch;
	case 0xf1:    /* illegal on 8086 and 80x86 */
	    goto illegal_op;
	case REPNE: {
	    int count = CX;

#ifdef DEBUG
#ifdef DOSEMU
	instr_count += count;
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
printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2), AX, BX, CX, DX, SI, DI, BP, SP, decode(*(PC+1), *(PC+2)), instr_count);
fflush(stdout);
}
#endif	/* DOSEMU */
#endif	/* DEBUG */

	    PC += 2;
segrepne:
	    switch (*(PC-1)){
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
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) {
			DI -= count; SI -= count; CX = 0;
			while (count--) *dest-- = *src--;
			goto next_switch;
		    } else {
			DI += count; SI += count; CX = 0;
			while (count--) *dest++ = *src++;
			goto next_switch;
		    } } 
		case MOVSw: {
		    unsigned char *src, *dest;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    count = count << 1;
		    if (env->flags & DIRECTION_FLAG) {
			DI -= count; SI -= count; CX = 0;
			while (count){
			    *(dest+1) = *(src+1);
			    *dest = *src;
			    dest -= 2; src -= 2; count -= 2;
			} goto next_switch;
		    } else {
			DI += count; SI += count; CX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    dest += 2; src += 2; count -= 2;
			} goto next_switch;
		    } }
		case CMPSb: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0, src2=0;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = *src--;
			    src2 = *dest--;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
				BYTE_FLAG = BYTE_OP;
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = *src++;
			    src2 = *dest++;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
				BYTE_FLAG = BYTE_OP;
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0; DI = dest - LONG_ES;
		    SI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res << 8; SRC1_8 = src1; 
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
		    } goto next_switch;
		case CMPSw: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0;
		    int src2=0;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src -= 2; dest -=2;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1;
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				RES_32 = res;
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src += 2; dest +=2;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1;
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				RES_32 = res;
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0; DI = dest - LONG_ES;
		    SI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    } goto next_switch;
		case STOSb: {
		    unsigned char al;
		    unsigned char *dest;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    al = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count; CX = 0;
			while (count--) *dest-- = al;
			goto next_switch;
		    } else {		      /* forwards */
			DI += count; CX = 0;
			while (count--) *dest++ = al;
			goto next_switch;
		    } } 
		case STOSw: {
		    unsigned char al, ah;
		    unsigned char *dest;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    al = AL; ah = AH;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest -= 2;
			} goto next_switch;
		    } else {		      /* forwards */
			DI += count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest += 2;
			} goto next_switch;
		    } }
		case LODSb: {
		    unsigned char *src;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			AL = *(src - count); SI -= count;
			CX = 0; goto next_switch;
		    } else {		      /* forwards */
			AL = *(src + count); SI += count;
			CX = 0; goto next_switch;
		    } } 
		case LODSw: {
		    unsigned char *src;
		    count = count << 1;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; SI -= count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; goto next_switch;
		    } else {		      /* forwards */
			src = src + count; SI += count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; goto next_switch;
		    } }
		case SCASb: {
		    unsigned int res, src1;
		    int src2;
		    unsigned char *dest;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    src1 = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = *dest;
			    dest -=1;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				RES_32 = res << 8; SRC1_8 = src1;
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
				BYTE_FLAG = BYTE_OP;
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
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
				DI = dest - LONG_ES;
				RES_32 = res << 8; SRC1_8 = src1;
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
				BYTE_FLAG = BYTE_OP;
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0; DI = dest - LONG_ES;
		    RES_32 = res << 8; SRC1_8 = src1;
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
		    BYTE_FLAG = BYTE_OP;
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } goto next_switch;
		case SCASw: {
		    unsigned char *dest;
		    unsigned int res, src1;
		    int src2;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    src1 = AX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest -=2;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest +=2;
			    if (src1 != src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    } 
		    res = src1 - src2;
		    CX = 0; DI = dest - LONG_ES;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } goto next_switch;
/*------------------IA-----------------------------------*/
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
/*------------------IA------------------------------------*/
		default: PC--; goto next_switch;
	    } }

	case REP: {     /* also is REPE */
	    int count = CX;

#ifdef DEBUG
#ifdef DOSEMU
	instr_count += count;
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
printf("%04x:%04x %02x %02x %04x %04x %04x %04x %04x %04x %04x %04x %s %d\n", SHORT_CS_16, PC-(LONG_CS)+1,*(PC+1), *(PC+2), AX, BX, CX, DX, SI, DI, BP, SP, decode(*(PC+1), *(PC+2)), instr_count);
fflush(stdout);
}
#endif	/* DOSEMU */
#endif	/* DEBUG */

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
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) {
			DI -= count; SI -= count; CX = 0;
			while (count--) *dest-- = *src--;
			goto next_switch;
		    } else {
			DI += count; SI += count; CX = 0;
			while (count--) *dest++ = *src++;
			goto next_switch;
		    } } 
		case MOVSw: {
		    unsigned char *src, *dest;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    count = count << 1;
		    if (env->flags & DIRECTION_FLAG) {
			DI -= count; SI -= count; CX = 0;
			while (count){
			    *(dest+1) = *(src+1);
			    *dest = *src;
			    dest -= 2; src -= 2; count -= 2;
			} goto next_switch;
		    } else {
			DI += count; SI += count; CX = 0;
			while (count){
			    *dest = *src;
			    *(dest+1) = *(src+1);
			    dest += 2; src += 2; count -= 2;
			} goto next_switch;
		    } }
		case CMPSb: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0, src2=0;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = *src--;
			    src2 = *dest--;
			    if (src1 == src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
				goto next_switch;
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
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				RES_32 = res << 8; SRC1_8 = src1; 
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
				goto next_switch;
			    }
			}
		    }
		    CX = 0; DI = dest - LONG_ES;
		    res = src1 - src2;
		    SI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res << 8; SRC1_8 = src1; 
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2);
		    BYTE_FLAG = BYTE_OP;
		    } goto next_switch;
		case CMPSw: {
		    unsigned char *src, *dest;
		    unsigned int res, src1=0;
		    int src2=0;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    dest = LONG_ES + DI;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src1 = FETCH_WORD(src);
			    src2 = FETCH_WORD(dest);
			    src -= 2; dest -=2;
			    if (src1 == src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1; RES_32 = res; 
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1; 
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				goto next_switch;
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
				DI = dest - LONG_ES;
				SI = src - ALLOW_OVERRIDE(LONG_DS);
				SRC1_16 = src1; 
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
				goto next_switch;
			    }
			}
		    }
		    CX = 0; DI = dest - LONG_ES;
		    res = src1 - src2;
		    SI = src - ALLOW_OVERRIDE(LONG_DS);
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
		    } goto next_switch;
		case STOSb: {
		    unsigned char al;
		    unsigned char *dest;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    al = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count; CX = 0;
			while (count--) *dest-- = al;
			goto next_switch;
		    } else {		      /* forwards */
			DI += count; CX = 0;
			while (count--) *dest++ = al;
			goto next_switch;
		    } } 
		case STOSw: {
		    unsigned char al, ah;
		    unsigned char *dest;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    al = AL; ah = AH;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			DI -= count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest -= 2;
			} goto next_switch;
		    } else {		      /* forwards */
			DI += count<<1; CX = 0;
			while (count--) {
			    *dest = al;
			    *(dest+1) = ah;
			    dest += 2;
			} goto next_switch;
		    } }
		case LODSb: {
		    unsigned char *src;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			AL = *(src - count); SI -= count;
			CX = 0; goto next_switch;
		    } else {		      /* forwards */
			AL = *(src + count); SI += count;
			CX = 0; goto next_switch;
		    } } 
		case LODSw: {
		    unsigned char *src;
		    count = count << 1;
		    if (count == 0) goto next_switch;
		    src = ALLOW_OVERRIDE(LONG_DS) + (SI);
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			src = src - count; SI -= count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; goto next_switch;
		    } else {		      /* forwards */
			src = src + count; SI += count;
			AL = *(src); 
			AH = *(src + 1);
			CX = 0; goto next_switch;
		    } }
		case SCASb: {
		    unsigned int res, src1;
		    int src2;
		    unsigned char *dest;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    src1 = AL;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = *dest;
			    dest -=1;
			    if (src1 == src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				RES_32 = res << 8; SRC1_8 = src1;
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
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
				DI = dest - LONG_ES;
				RES_32 = res << 8; SRC1_8 = src1;
				SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
				BYTE_FLAG = BYTE_OP;
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    }
		    res = src1 - src2;
		    CX = 0; DI = dest - LONG_ES;
		    RES_32 = res << 8; SRC1_8 = src1;
		    SRC2_8 = (((src2 & 0xff)== 0x80)? 0:-src2); 
		    BYTE_FLAG = BYTE_OP;
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } goto next_switch;
		case SCASw: {
		    unsigned char *dest;
		    unsigned int res, src1;
		    int src2;
		    if (count == 0) goto next_switch;
		    dest = LONG_ES + DI;
		    src1 = AX;
		    if (env->flags & DIRECTION_FLAG) { /* backwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest -=2;
			    if (src1 == src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1; 
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    } else {		      /* forwards */
			while (count) {
			    src2 = FETCH_WORD(dest);
			    dest +=2;
			    if (src1 == src2) count--;
			    else {
				res = src1 - src2;
				CX = count - 1;
				DI = dest - LONG_ES;
				RES_32 = res; SRC1_16 = src1;
				SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                                if((src1 & 0xf) < src2) SET_AF
                                else CLEAR_AF
				goto next_switch;
			    }
			}
		    } 
		    res = src1 - src2;
		    CX = 0; DI = dest - LONG_ES;
		    RES_32 = res; SRC1_16 = src1;
		    SRC2_16 = (((src2 & 0xffff)== 0x8000)? 0:-src2);
                    if((src1 & 0xf) < src2) SET_AF
                    else CLEAR_AF
		    } goto next_switch;
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
		default: PC--; goto next_switch;
	    } }

	case HLT:
#ifdef DOSEMU
	    goto not_permitted;
#else
	    EBP = EBP + (long)LONG_SS;
	    ESP = ESP + (long)LONG_SS;
	    return PC;
#endif
	case CMC:
	    interp_var->flags_czsp.word16.carry ^= CARRY_FLAG;
	    PC += 1; goto next_switch;
	case GRP1brm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    mem_ref = MEM_REF; 
	    switch (res) {
		case 0: /* TEST */
		    src1 = *mem_ref;
		    src2 = *PC; PC += 1;
		    RES_32 = (src1 & src2) << 8;
		    SRC1_8 = SRC2_8 = RES_8;
		    BYTE_FLAG = BYTE_OP;
		    goto next_switch;
		case 1: /* TEST (Is this Illegal?) */
                    goto illegal_op; 
		case 2: /* NOT */
		    src1 = ~(*mem_ref);
		    *mem_ref = src1;
		    goto next_switch;
		case 3: /* NEG */
		    src1 = -(*mem_ref);
		    *mem_ref = src1;
		    RES_32 = src1 << 8;
                    SRC1_16 = 0;
                    SRC2_16 = RES_16;
		    BYTE_FLAG = BYTE_OP;
		    goto next_switch;
		case 4: /* MUL AL */
		    src1 = *mem_ref;
		    src2 = AL;
		    res = src1 * src2;
		    AX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = (res>>8)?-1:0;
		    goto next_switch;
		case 5: /* IMUL AL */
		    src1 = *(signed char *)mem_ref;
		    src2 = (signed char)AL;
		    res = src1 * src2;
		    AX = res;
		    SRC1_16 = SRC2_16 = 0;
		    res = res >> 8;
		    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
		    goto next_switch;
		case 6: /* DIV AL */
		    src1 = *mem_ref;
		    res = AX;
		    AL = res / src1;
		    AH = res % src1;
		    goto next_switch;
		case 7: /* IDIV AX */
		    src1 = *(signed char *)mem_ref;
		    res = AX;
		    AL = res / src1;
		    AH = res % src1;
		    goto next_switch;
	    } }
	case GRP1wrm: {
	    int src1, src2, res; unsigned char *mem_ref;
	    res = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	    if (IS_MODE_REG) { /* register is operand */
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_XREG(mem_ref);
		    src2 = FETCH_WORD(PC); PC += 2;
		    res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 1: /* TEST (Is this Illegal?) */
		goto illegal_op;
		case 2: /* NOT */
		    src1 = FETCH_XREG(mem_ref);
		    src1 = ~(src1);
		    *(unsigned short *)mem_ref = src1;
		    goto next_switch;
		case 3: /* NEG */
		    src1 = FETCH_XREG(mem_ref);
		    src1 = -src1;
		    SRC1_16 = 0;
		    *(unsigned short *)mem_ref = src1; SRC2_16 = src1;
		    RES_32 = src1;
		    goto next_switch;
		case 4: /* MUL AX */
		    src1 = FETCH_XREG(mem_ref);
		    src2 = AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = (res)?-1:0;
		    goto next_switch;
		case 5: /* IMUL AX */
		    src1 = *(signed short *)mem_ref;
		    src2 = (signed short)AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
		    goto next_switch;
		case 6: /* DIV DX+AX */
		    src1 = FETCH_XREG(mem_ref);
		    if (src1) {
			res = AX | (DX<<16);
			AX = res / src1;
			DX = res % src1;
		    }
		    else
			AX = DX = 0;
		    goto next_switch;
		case 7: /* IDIV DX+AX */
		    src1 = *(signed short *)mem_ref;
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    goto next_switch;
	      }
	    } else {
	      switch (res) {
		case 0: /* TEST */
		    src1 = FETCH_WORD(mem_ref);
		    src2 = FETCH_WORD(PC); PC += 2;
		    res = src1 & src2;
		    RES_32 = res; SRC1_16 = res; SRC2_16 = res;
		    goto next_switch;
		case 1: /* TEST (Is this Illegal?) */
                    goto illegal_op; 
		case 2: /* NOT */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ~(src1);
		    PUT_WORD(mem_ref, src1);
		    goto next_switch;
		case 3: /* NEG */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = -(src1);
		    PUT_WORD(mem_ref, src1);
		    SRC1_16 = 0; SRC2_16 = src1;
		    RES_32 = src1;
		    goto next_switch;
		case 4: /* MUL AX */
		    src1 = FETCH_WORD(mem_ref);
		    src2 = AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = (res)?-1:0;
		    goto next_switch;
		case 5: /* IMUL AX */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ((src1<<16)>>16);
		    src2 = (signed short)AX;
		    res = src1 * src2; AX = res;
		    res = res >> 16;
		    DX = res;
		    SRC1_16 = SRC2_16 = 0;
		    RES_32 = ((res==0)||(res==0xffffffff))?0:-1;
		    goto next_switch;
		case 6: /* DIV DX+AX */
		    src1 = FETCH_WORD(mem_ref);
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    goto next_switch;
		case 7: /* IDIV DX+AX */
		    src1 = FETCH_WORD(mem_ref);
		    src1 = ((src1<<16)>>16);
		    res = AX | (DX<<16);
		    AX = res / src1;
		    DX = res % src1;
		    goto next_switch;
	      } }
	      }

	case CLC:
	    CLEAR_CF;
	    PC += 1; goto next_switch;
	case STC:
	    SET_CF;
	    PC += 1; goto next_switch;
	case CLI:
#ifdef DOSEMU
	    if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK))
		goto not_permitted;
	    env->flags &= ~INTERRUPT_FLAG;
#endif
	    PC += 1; goto next_switch;
	case STI:
#ifdef DOSEMU
	    if (vm86f && ((env->flags&IOPL_FLAG_MASK)<IOPL_FLAG_MASK))
		goto not_permitted;
	    env->flags |= INTERRUPT_FLAG;
#endif
	    PC += 1; goto next_switch;
	case CLD:
	    CLEAR_DF;
	    PC += 1; goto next_switch;
	case STD:
	    SET_DF;
	    PC += 1; goto next_switch;
	case GRP2brm: { /* only INC and DEC are legal on bytes */
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_byte(env,PC,interp_var);
	    switch (temp) {
		case 0: /* INC */
		    mem_ref = MEM_REF;
		    SRC1_8 = temp = *mem_ref;
		    *mem_ref = temp = temp + 1;
		    SRC2_8 = 1;
		    RES_16 = temp << 8; BYTE_FLAG = BYTE_OP;
		    goto next_switch;
		case 1: /* DEC */
		    mem_ref = MEM_REF;
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

	case GRP2wrm: {
	    int temp; unsigned char *mem_ref;
	    temp = (*(PC+1)>>3)& 0x7;
	    PC += hsw_modrm_16_word(env,PC,interp_var);
	    mem_ref = MEM_REF;
	      switch (temp) {
		case 0: /* INC */
	          if (IS_MODE_REG) { /* register is operand */
		    temp = FETCH_XREG(mem_ref);
		    SRC1_16 = temp; SRC2_16 = 1;
		    *(unsigned short *)mem_ref = RES_16 = temp + 1;
		  }
		  else {
		    temp = FETCH_WORD(mem_ref);
		    SRC1_16 = temp; SRC2_16 = 1;
		    RES_16 = temp = temp + 1;
		    PUT_WORD(mem_ref, temp);
		  }
		    goto next_switch;
		case 1: /* DEC */
	          if (IS_MODE_REG) { /* register is operand */
		    temp = FETCH_XREG(mem_ref);
		    SRC1_16 = temp; SRC2_16 = -1;
		    *(unsigned short *)mem_ref = RES_16 = temp - 1;
		  }
		  else {
		    temp = FETCH_WORD(mem_ref);
		    SRC1_16 = temp; SRC2_16 = -1;
		    RES_16 = temp = temp - 1;
		    PUT_WORD(mem_ref, temp);
		  }
		    goto next_switch;
		case 2: /* CALL near indirect */
		    temp = PC - LONG_CS;
		    PUSHWORD(temp);
	          if (IS_MODE_REG) /* register is operand */
		    PC = LONG_CS + FETCH_XREG(mem_ref);
		  else
		    PC = LONG_CS + FETCH_WORD(mem_ref);
		    goto next_switch;
		case 4: /* JMP near indirect */
	          if (IS_MODE_REG) /* register is operand */
		    PC = LONG_CS + FETCH_XREG(mem_ref);
		  else
		    PC = LONG_CS + FETCH_WORD(mem_ref);
		    goto next_switch;
		case 3: { /* CALL long indirect */
		    unsigned int cs = SHORT_CS_16;
		    unsigned int ip = PC - LONG_CS;
		    PUSHWORD(cs);
		    PUSHWORD(ip);
		    env->return_addr = (cs << 16)|ip;
		    }
		    /* fall through */
		case 5:  { /* JMP long indirect */
		    unsigned int cs, ip;
		    unsigned short transfer_magic;
		    ip = FETCH_WORD(mem_ref);
		    cs = FETCH_WORD(mem_ref+2);
		    transfer_magic = (WORD)GetSelectorType(cs);
#ifdef DOSEMU
		    if (vm86f||(transfer_magic == TRANSFER_CODE16)) {
#else
		    if (transfer_magic == TRANSFER_CODE16) {
#endif
			SHORT_CS_16 = cs;
			SET_SEGREG(LONG_CS,cs);
			PC = ip + LONG_CS;
			goto next_switch;
		    }
#ifdef DOSEMU
		    if (vm86f || (transfer_magic == TRANSFER_CALLBACK) ||
#else
		    if ((transfer_magic == TRANSFER_CALLBACK) ||
#endif
				(transfer_magic == TRANSFER_BINARY))  {
			LONGPROC conv,targ;
			SEGIMAGE *lpSegImage = &((SEGIMAGE *)
				(*(long *)(SELECTOR_PADDRESS(cs))))[ip>>3];
			EBP = (long)LONG_SS + EBP;
			ESP = (long)LONG_SS + ESP;
			trans_interp_flags(env, interp_var);    
			targ = (LONGPROC)lpSegImage->targ;
			conv = (LONGPROC)lpSegImage->conv;
#ifdef DOSEMU
			if (vm86f||(transfer_magic == TRANSFER_CALLBACK))
#else
			if (transfer_magic == TRANSFER_CALLBACK)
#endif
		    	    env->trans_addr = (BINADDR)MAKELONG(ip,cs);
#ifdef	TRACE
			LogProcName(cs,ip,1);
#else
#ifdef DEBUG
#ifdef DOSEMU
			if (vm86f||(transfer_magic == TRANSFER_CALLBACK))
#else
			if (transfer_magic == TRANSFER_CALLBACK)
#endif
			    LOGSTR((LF_DEBUG,"do_ext: %s\n", 
						GetProcName(cs,ip>>3)));
			else    /* TRANSFER_BINARY */
			    LOGSTR((LF_DEBUG,
				"do_ext: calling binary thunk %x:%x\n",cs,ip));
#endif
#endif	/* TRACE */
			(conv)(env,targ);
#ifdef	TRACE
			LogProcName(AX,DX,0);
#endif
			SHORT_CS_16 = cs = env->return_addr >> 16;
			ip = env->return_addr & 0xffff;
			SET_SEGREG(LONG_CS,SHORT_CS_16);
			SET_SEGREG(LONG_DS,SHORT_DS_16);
			SET_SEGREG(LONG_ES,SHORT_ES_16);
			SET_SEGREG(LONG_SS,SHORT_SS_16);
			EBP = EBP - (long)LONG_SS;
			ESP = ESP - (long)LONG_SS;
			trans_flags_to_interp(env, interp_var, env->flags);
			PC = LONG_CS + ip; goto next_switch;
		    }
#ifdef DOSEMU
		    if (vm86f||(transfer_magic == TRANSFER_RETURN)) {
#else
		    if (transfer_magic == TRANSFER_RETURN) {
#endif
			SHORT_CS_16 = cs;
			env->return_addr = (cs << 16) | ip;
#ifndef DOSEMU
			trans_interp_flags(env, interp_var);    
			EBP = EBP + (long)LONG_SS;
			ESP = ESP + (long)LONG_SS;
#endif
			return PC;
		    }
		    invoke_data(env);    /* TRANSFER_DATA or garbage */
		}
		case 6: /* PUSH */
	          if (IS_MODE_REG) /* register is operand */
		    temp = FETCH_XREG(mem_ref);
		  else
		    temp = FETCH_WORD(mem_ref);
		    PUSHWORD(temp);
		    goto next_switch;
		case 7: /* Illegal */
		    goto illegal_op;
	      }
	    }
    } /* end of the switch statement */

    not_implemented:
	fprintf(stderr," nonimplemented instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2),
		SHORT_CS_16,PC-LONG_CS,(int)PC);
#ifdef DOSEMU
	*err=EXCP06_ILLOP; return P0;
#else
	exit(1);
#endif

    not_permitted:
#ifdef DOSEMU
	*err=EXCP0D_GPF; return P0;
#endif
    illegal_op:
	fprintf(stderr," illegal instruction %2x %2x %2x at %4x:%4x long PC %x\n",*PC,*(PC+1),*(PC+2), 
                SHORT_CS_16,PC-LONG_CS,(int)PC);
#ifdef DOSEMU
	*err=EXCP06_ILLOP; return P0;
#else
        exit(1); 
    next_switch_16_32:;    /* 16 bit operand, 32 bit addressing switch */
    /* we only want to handle the mod/rm instructions in this switch */
    switch (*PC) {
	default: goto next_switch;
    } /* end of the switch statement */
#endif

    log_rot: { /* logical rotation don't change SF ZF AF PF flags */
	DWORD flags;
	flags = env->flags;
	if(IS_CF_SET) flags |= CARRY_FLAG;
	else flags &= ~CARRY_FLAG;
	if(IS_OF_SET) flags |= OVERFLOW_FLAG;
	else flags &= ~OVERFLOW_FLAG;
	trans_flags_to_interp(env,interp_var, flags);
	} goto next_switch;
#ifndef DOSEMU
    next_switch_32_16:;    /* 32 bit operand, 16 bit addressing switch */
    /* only handle the "word" size intructions in this switch */
    switch (*PC) {
	default: goto next_switch;
    } /* end of the switch statement */

    next_switch_32_32:;    /* 32 bit operand, 32 bit addressing switch */
    /* we only want to handle the mod/rm instructions in this switch */
    switch (*PC) {
	default: goto next_switch_32_16;
    } /* end of the switch statement */
#endif
}


/**************************************************************************
    The interpreter is actually called with a BINADDR as its second
    parameter, but we will re-use this register as our PC instead,
    since that is all we really will use the passed address for
    anyway. mfh
**************************************************************************/
#ifdef DOSEMU

int
invoke_code16 (Interp_ENV *env, int vf)
{
  Interp_VAR interp_variables;
  Interp_VAR *interp_var = &interp_variables;
  unsigned char *PC;
  int err = 0;

  vm86f = (vf!=0);
  code32 = data32 = 0;
  SHORT_CS_16 = (unsigned int) env->trans_addr >> 16;

  SET_SEGREG (LONG_CS, SHORT_CS_16);
  SET_SEGREG (LONG_DS, SHORT_DS_16);
  SET_SEGREG (LONG_ES, SHORT_ES_16);
  SET_SEGREG (LONG_SS, SHORT_SS_16);
  SET_SEGREG (LONG_FS, SHORT_FS_16);
  SET_SEGREG (LONG_GS, SHORT_GS_16);
  OVERRIDE = INVALID_OVR;
  PC = (unsigned char *) LONG_CS + LOWORD (env->trans_addr);
  trans_flags_to_interp (env, interp_var, env->flags);

  PC = hsw_interp_16_16 (env, PC, PC, interp_var, &err);

  if (err) env->return_addr = (SHORT_CS_16 << 16) | (PC-LONG_CS);
  trans_interp_flags (env, interp_var);
  return err;
}

#else	/* !DOSEMU */

void
invoke_code16(Interp_ENV *env)
{
    Interp_VAR interp_variables;
    Interp_VAR *interp_var = &interp_variables;
    unsigned char *PC;
    int err = 0;
#ifdef DEBUG
    if (!print_initialized) {
	char *ch;

	print_initialized = 1;
	if ( ch = getenv("HSW_START")) {
	    if(strstr(ch,"0x") == ch)
		sscanf(ch+2, "%x", &start_count);
	    else
		sscanf(ch, "%d", &start_count);
	    printf("Starting instruction for print is: %d\n", start_count);
	}
	if ( ch = getenv("HSW_END")) {
	    if(strstr(ch,"0x") == ch)
		sscanf(ch+2, "%x", &end_count);
	    else
		sscanf(ch, "%d", &end_count);
	    printf("  Ending instruction for print is: %d\n", end_count);
	}
	if ( ch = getenv("HSW_GRAN")) {
	    if(strstr(ch,"0x") == ch)
		sscanf(ch+2, "%x", &granularity);
	    else
		sscanf(ch, "%d", &granularity);
	    printf("   Granularity for print is: %d\n", granularity);
	}
	if ( ch = getenv("HSW_SEGMENT_PRINT")) {
	    segment_print = 1;
	}
	if ( ch = getenv("HSW_STACK_PRINT")) {
	    stack_print = 1;
	}
	if (ch = getenv("HSW_OP32")) op32_print = 1;
	if (ch = getenv("HSW_AD32")) ad32_print = 1;
	if ( ch = getenv("HSW_SHORT_PRINT")) {
	    small_print = 1;
	}
	if ( ch = getenv("HSW_FLOAT_PRINT")) float_print = 1;
	if ( ch = getenv("HSW_DBX_CS")) {
	    if(strstr(ch,"0x") == ch)
		sscanf(ch+2, "%x", &dbx_cs);
	    else
		sscanf(ch, "%d", &dbx_cs);
	    dbx_cs = dbx_cs & 0xffff;
	    printf("dbx_stop CS value is: %04x\n", dbx_cs);
	}
	if ( ch = getenv("HSW_DBX_IP")) {
	    if(strstr(ch,"0x") == ch)
		sscanf(ch+2, "%x", &dbx_ip);
	    else
		sscanf(ch, "%d", &dbx_ip);
	    dbx_ip = dbx_ip & 0xffff;
	    printf("dbx_stop IP value is: %04x\n", dbx_ip);
	}
    }
#endif
    code32 = data32 = 0;
    SHORT_CS_16 = (unsigned int)env->trans_addr >> 16;

    SET_SEGREG(LONG_CS,SHORT_CS_16);
    SET_SEGREG(LONG_DS,SHORT_DS_16);
    SET_SEGREG(LONG_ES,SHORT_ES_16);
    SET_SEGREG(LONG_SS,SHORT_SS_16);
    SET_SEGREG(LONG_FS,SHORT_FS_16);
    SET_SEGREG(LONG_GS,SHORT_GS_16);
    OVERRIDE = INVALID_OVR;
    PC = (unsigned char *)LONG_CS + LOWORD(env->trans_addr);
    EBP = EBP - (unsigned long)LONG_SS;
    ESP = ESP - (unsigned long)LONG_SS;

    PC = hsw_interp_16_16 (env, PC, PC, interp_var, &err);
}

#endif	/* DOSEMU */

unsigned int
trans_interp_flags(Interp_ENV *env, Interp_VAR *interp_var)
{
    unsigned int flags;

    /* turn off flag bits that we update here */
#ifdef DOSEMU
    flags = env->flags & 0x3f7700;
#else
    flags = env->flags & (DIRECTION_FLAG | INTERRUPT_FLAG);
#endif

    /* byte operation */
    flags |= ((BYTE_FLAG == BYTE_OP)? BYTE_FL:0);

    /* or in Carry flag */
    flags |= interp_var->flags_czsp.word16.carry & CARRY_FLAG;

    /* or in Zero flag */
    flags |= ((RES_16 == 0)?ZERO_FLAG:0);

    /* or in Sign flag */
    flags |= (interp_var->flags_czsp.byte.res8 & SIGN_FLAG);

    /* or in Overflow flag */
    flags |= ((int)(((RES_16)^(~((SRC1_16)^(SRC2_16)))))>>4)&OVERFLOW_FLAG;

    /* or in Parity flag */
    flags |= parity[((interp_var->flags_czsp.byte.byte_op==BYTE_OP)
	    ?interp_var->flags_czsp.byte.res8:interp_var->flags_czsp.byte.parity16)];

    /* or in Aux Carry flag */
    flags |= (int)((interp_var->flags_czsp.byte.byte_op==BYTE_OP)?
	(((SRC1_8 & 0xf) + (SRC2_8 & 0xf)) & AUX_CARRY_FLAG):
	(((SRC1_16 & 0xf) + (SRC2_16 & 0xf)) & AUX_CARRY_FLAG));

    /* put flags into the env structure */
    env->flags = flags;

    /* return flags */
    return(flags);
}

void
trans_flags_to_interp(Interp_ENV *env, Interp_VAR *interp_var, unsigned int flags)
{
	if(flags & BYTE_FL) { BYTE_FLAG = BYTE_OP;
 	flags ^=BYTE_FL;}
    env->flags = flags;

    /* deal with carry */
    interp_var->flags_czsp.word16.carry = flags & CARRY_FLAG;

    /* deal with parity */
	if(BYTE_FLAG == BYTE_OP)
        PAR_16 = (flags & PARITY_FLAG)? 0:1;
	else RES_8 = (flags & PARITY_FLAG)? 0:1;

    /* deal with zero */
    RES_16 = (flags & ZERO_FLAG)?0:0x100;

    /* deal with sign */
    RES_8 = (flags & SIGN_FLAG) | RES_8;

    /* deal with overflow */
    if (flags & OVERFLOW_FLAG){
	if (flags & SIGN_FLAG) {
	    SRC1_8 = SRC2_8 = 0;
	} else {
	    SRC1_8 = SRC2_8 = 0xff;
	}
    } else {
	SRC1_8 = 0; SRC2_8 = 0xff;
    }


    /* deal with aux carry */
	if(BYTE_FLAG ==BYTE_OP) {
     SRC2_8 |= (flags & AUX_CARRY_FLAG)?0x8:0; 
     SRC1_8 |= (flags & AUX_CARRY_FLAG)?0x8:0;
	} else {
     SRC2_16 |= (flags & AUX_CARRY_FLAG)?0x8:0; 
     SRC1_16 |= (flags & AUX_CARRY_FLAG)?0x8:0;
	}
}

#ifndef DOSEMU
void print_table()
{
    int i,j;
    for (i = 0; i < 256; i += 16) {
	for (j = 0; j < 8; j++, printf(" ")) {
	    if (opcode_table[i+j] > 0xffff)
		printf("+%03x", (opcode_table[i+j]>>16) & 0xfff);
	    else
		printf("%04x", opcode_table[i+j]);
	}
	printf("-");
	for (j = 8; j < 16; j++, printf(" ")) {
	    if (opcode_table[i+j] > 0xffff)
		printf("+%03x", (opcode_table[i+j]>>16) & 0xfff);
	    else
		printf("%04x", opcode_table[i+j]);
	}
	printf("\n\n");
    }
}
#endif

#ifdef DEBUG
char *inst[256] = {
	"ADDbfrm", "ADDwfrm", "ADDbtrm", "ADDwtrm", "ADDbia", "ADDwia",
	"PUSHes", "POPes", "ORbfrm", "ORwfrm", "ORbtrm", "ORwtrm", "ORbi",
	"ORwi", "PUSHcs", "TwoByteEscape", "ADCbfrm", "ADCwfrm", "ADCbtrm", 
	"ADCwtrm",
	"ADCbi", "ADCwi", "PUSHss", "POPss", "SBBbfrm", "SBBwfrm", "SBBbtrm",
	"SBBwtrm", "SBBbi", "SBBwi", "PUSHds", "POPds", "ANDbfrm", "ANDwfrm",
	"ANDbtrm", "ANDwtrm", "ANDbi", "ANDwi", "SEGes", "DAA", "SUBbfrm",
	"SUBwfrm", "SUBbtrm", "SUBwtrm", "SUBbi", "SUBwi", "SEGcs", "DAS",
	"XORbfrm", "XORwfrm", "XORbtrm", "XORwtrm", "XORbi", "XORwi", "SEGss",
	"AAA", "CMPbfrm", "CMPwfrm", "CMPbtrm", "CMPwtrm", "CMPbi", "CMPwi",
	"SEGds", "AAS", "INCax", "INCcx", "INCdx", "INCbx", "INCsp", "INCbp",
	"INCsi", "INCdi", "DECax", "DECcx", "DECdx", "DECbx", "DECsp", "DECbp",
	"DECsi", "DECdi", "PUSHax", "PUSHcx", "PUSHdx", "PUSHbx", "PUSHsp",
	"PUSHbp", "PUSHsi", "PUSHdi", "POPax", "POPcx", "POPdx", "POPbx",
	"POPsp", "POPbp", "POPsi", "POPdi", "PUSHA", "POPA", "BOUND", "ARPL",
	"SEGfs", "SEGgs", "OPER32", "ADDR32", "PUSHwi", "IMULwrm",
	"PUSHbi", "IMULbrm", "INSb", "INSw", "OUTSb", "OUTSw", "JO", "JNO",
	"JB_JNAE", "JNB_JAE", "JE_JZ", "JNE_JNZ", "JBE_JNA", "JNBE_JA",
	"JS", "JNS", "JP_JPE", "JNP_JPO", "JL_JNGE", "JNL_JGE", "JLE_JNG",
	"JNLE_JG", "IMMEDbrm", "IMMEDwrm", "IMMEDbrm2", "IMMEDisrm", "TESTbrm",
	"TESTwrm", "XCHGbrm", "XCHGwrm", "MOVbfrm", "MOVwfrm", "MOVbtrm",
	"MOVwtrm", "MOVsrtrm", "LEA", "MOVsrfrm", "POPrm", "NOP", "XCHGcx",
	"XCHGdx", "XCHGbx", "XCHGsp", "XCHGbp", "XCHGsi", "XCHGdi", "CBW",
	"CWD", "CALLl", "WAIT", "PUSHF", "POPF", "SAHF", "LAHF", "MOVmal",
	"MOVmax", "MOValm", "MOVaxm", "MOVSb", "MOVSw", "CMPSb", "CMPSw",
	"TESTbi", "TESTwi", "STOSb", "STOSw", "LODSb", "LODSw", "SCASb",
	"SCASw", "MOVial", "MOVicl", "MOVidl", "MOVibl", "MOViah", "MOVich",
	"MOVidh", "MOVibh", "MOViax", "MOVicx", "MOVidx", "MOVibx", "MOVisp",
	"MOVibp", "MOVisi", "MOVidi", "SHIFTbi", "SHIFTwi", "RETisp", "RET",
	"LES", "LDS", "MOVbirm", "MOVwirm", "ENTER", "LEAVE", "RETlisp", "RETl",
	"INT3", "INT", "INTO", "IRET", "SHIFTb", "SHIFTw", "SHIFTbv", "SHIFTwv",
	"AAM", "AAD", "ILLEGAL", "XLAT", "ESC0", "ESC1", "ESC2", "ESC3", "ESC4",
	"ESC5", "ESC6", "ESC7", "LOOPNZ_LOOPNE", "LOOPZ_LOOPE", "LOOP", "JCXZ",
	"INb", "INw", "OUTb", "OUTw", "CALLd", "JMPd", "JMPld", "JMPsid",
	"INvb", "INvw", "OUTvb", "OUTvw", "LOCK", "ILLEGAL", "REPNE", "REP",
	"HLT", "CMC", "GRP1brm", "GRP1wrm", "CLC", "STC", "CLI", "STI", "CLD",
	"STD", "GRP2brm", "GRP2wrm", };

char *
decode(opcode, modrm)
int opcode, modrm;
{

    return(inst[opcode]);

}
#endif /* DEBUG */	

