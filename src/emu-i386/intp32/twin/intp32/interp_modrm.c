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
	interp_modrm.c	1.4
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

#include "emu-globv.h"
#include "hsw_interp.h"
#include "mod_rm.h"

static int
hsw_modrm_sibd(Interp_ENV *env, int sib, unsigned char *base)
{
	long v = FETCH_QUAD(base);
	switch(sib) {
		/* DS: d32 + (index<<0) */
		case 0x05: return (v + EAX);
		case 0x0d: return (v + ECX);
		case 0x15: return (v + EDX);
		case 0x1d: return (v + EBX);
		case 0x25: return (v);
		case 0x2d: return (v + EBP);
		case 0x35: return (v + ESI);
		case 0x3d: return (v + EDI);
		/* DS: d32 + (index<<1) */
		case 0x45: return (v + (EAX << 1));
		case 0x4d: return (v + (ECX << 1));
		case 0x55: return (v + (EDX << 1));
		case 0x5d: return (v + (EBX << 1));
		case 0x6d: return (v + (EBP << 1));
		case 0x75: return (v + (ESI << 1));
		case 0x7d: return (v + (EDI << 1));
		/* DS: d32 + (index<<2) */
		case 0x85: return (v + (EAX << 2));
		case 0x8d: return (v + (ECX << 2));
		case 0x95: return (v + (EDX << 2));
		case 0x9d: return (v + (EBX << 2));
		case 0xad: return (v + (EBP << 2));
		case 0xb5: return (v + (ESI << 2));
		case 0xbd: return (v + (EDI << 2));
		/* DS: d32 + (index<<3) */
		case 0xc5: return (v + (EAX << 3));
		case 0xcd: return (v + (ECX << 3));
		case 0xd5: return (v + (EDX << 3));
		case 0xdd: return (v + (EBX << 3));
		case 0xed: return (v + (EBP << 3));
		case 0xf5: return (v + (ESI << 3));
		case 0xfd: return (v + (EDI << 3));
		default:   FatalAppExit(0, "SIBD"); return 1;
	}
	return 0;	/* because of gcc warning */
}

static int
hsw_modrm_sib(Interp_ENV *env, int sib, unsigned char **ovr)
{
	*ovr = LONG_DS;		/* default */
	switch(sib) {
		case 0x00: return (EAX + EAX);
		case 0x01: return (ECX + EAX);
		case 0x02: return (EDX + EAX);
		case 0x03: return (EBX + EAX);
		case 0x04: *ovr = LONG_SS; return (ESP + EAX);
		case 0x05: *ovr = LONG_SS; return (EBP + EAX);
		case 0x06: return (ESI + EAX);
		case 0x07: return (EDI + EAX);
		case 0x08: return (EAX + ECX);
		case 0x09: return (ECX + ECX);
		case 0x0a: return (EDX + ECX);
		case 0x0b: return (EBX + ECX);
		case 0x0c: *ovr = LONG_SS; return (ESP + ECX);
		case 0x0d: *ovr = LONG_SS; return (EBP + ECX);
		case 0x0e: return (ESI + ECX);
		case 0x0f: return (EDI + ECX);
		case 0x10: return (EAX + EDX);
		case 0x11: return (ECX + EDX);
		case 0x12: return (EDX + EDX);
		case 0x13: return (EBX + EDX);
		case 0x14: *ovr = LONG_SS; return (ESP + EDX);
		case 0x15: *ovr = LONG_SS; return (EBP + EDX);
		case 0x16: return (ESI + EDX);
		case 0x17: return (EDI + EDX);
		case 0x18: return (EAX + EBX);
		case 0x19: return (ECX + EBX);
		case 0x1a: return (EDX + EBX);
		case 0x1b: return (EBX + EBX);
		case 0x1c: *ovr = LONG_SS; return (ESP + EBX);
		case 0x1d: *ovr = LONG_SS; return (EBP + EBX);
		case 0x1e: return (ESI + EBX);
		case 0x1f: return (EDI + EBX);
		case 0x20: return (EAX);
		case 0x21: return (ECX);
		case 0x22: return (EDX);
		case 0x23: return (EBX);
		case 0x24: *ovr = LONG_SS; return (ESP);
		case 0x25: *ovr = LONG_SS; return (EBP);
		case 0x26: return (ESI);
		case 0x27: return (EDI);
		case 0x28: return (EAX + EBP);
		case 0x29: return (ECX + EBP);
		case 0x2a: return (EDX + EBP);
		case 0x2b: return (EBX + EBP);
		case 0x2c: *ovr = LONG_SS; return (ESP + EBP);
		case 0x2d: *ovr = LONG_SS; return (EBP + EBP);
		case 0x2e: return (ESI + EBP);
		case 0x2f: return (EDI + EBP);
		case 0x30: return (EAX + ESI);
		case 0x31: return (ECX + ESI);
		case 0x32: return (EDX + ESI);
		case 0x33: return (EBX + ESI);
		case 0x34: *ovr = LONG_SS; return (ESP + ESI);
		case 0x35: *ovr = LONG_SS; return (EBP + ESI);
		case 0x36: return (ESI + ESI);
		case 0x37: return (EDI + ESI);
		case 0x38: return (EAX + EDI);
		case 0x39: return (ECX + EDI);
		case 0x3a: return (EDX + EDI);
		case 0x3b: return (EBX + EDI);
		case 0x3c: *ovr = LONG_SS; return (ESP + EDI);
		case 0x3d: *ovr = LONG_SS; return (EBP + EDI);
		case 0x3e: return (ESI + EDI);
		case 0x3f: return (EDI + EDI);
		case 0x40: return (EAX + (EAX << 1));
		case 0x41: return (ECX + (EAX << 1));
		case 0x42: return (EDX + (EAX << 1));
		case 0x43: return (EBX + (EAX << 1));
		case 0x44: *ovr = LONG_SS; return (ESP + (EAX << 1));
		case 0x45: *ovr = LONG_SS; return (EBP + (EAX << 1));
		case 0x46: return (ESI + (EAX << 1));
		case 0x47: return (EDI + (EAX << 1));
		case 0x48: return (EAX + (ECX << 1));
		case 0x49: return (ECX + (ECX << 1));
		case 0x4a: return (EDX + (ECX << 1));
		case 0x4b: return (EBX + (ECX << 1));
		case 0x4c: *ovr = LONG_SS; return (ESP + (ECX << 1));
		case 0x4d: *ovr = LONG_SS; return (EBP + (ECX << 1));
		case 0x4e: return (ESI + (ECX << 1));
		case 0x4f: return (EDI + (ECX << 1));
		case 0x50: return (EAX + (EDX << 1));
		case 0x51: return (ECX + (EDX << 1));
		case 0x52: return (EDX + (EDX << 1));
		case 0x53: return (EBX + (EDX << 1));
		case 0x54: *ovr = LONG_SS; return (ESP + (EDX << 1));
		case 0x55: *ovr = LONG_SS; return (EBP + (EDX << 1));
		case 0x56: return (ESI + (EDX << 1));
		case 0x57: return (EDI + (EDX << 1));
		case 0x58: return (EAX + (EBX << 1));
		case 0x59: return (ECX + (EBX << 1));
		case 0x5a: return (EDX + (EBX << 1));
		case 0x5b: return (EBX + (EBX << 1));
		case 0x5c: *ovr = LONG_SS; return (ESP + (EBX << 1));
		case 0x5d: *ovr = LONG_SS; return (EBP + (EBX << 1));
		case 0x5e: return (ESI + (EBX << 1));
		case 0x5f: return (EDI + (EBX << 1));
/* 01-100-xxx undefined */
		case 0x68: return (EAX + (EBP << 1));
		case 0x69: return (ECX + (EBP << 1));
		case 0x6a: return (EDX + (EBP << 1));
		case 0x6b: return (EBX + (EBP << 1));
		case 0x6c: *ovr = LONG_SS; return (ESP + (EBP << 1));
		case 0x6d: *ovr = LONG_SS; return (EBP + (EBP << 1));
		case 0x6e: return (ESI + (EBP << 1));
		case 0x6f: return (EDI + (EBP << 1));
		case 0x70: return (EAX + (ESI << 1));
		case 0x71: return (ECX + (ESI << 1));
		case 0x72: return (EDX + (ESI << 1));
		case 0x73: return (EBX + (ESI << 1));
		case 0x74: *ovr = LONG_SS; return (ESP + (ESI << 1));
		case 0x75: *ovr = LONG_SS; return (EBP + (ESI << 1));
		case 0x76: return (ESI + (ESI << 1));
		case 0x77: return (EDI + (ESI << 1));
		case 0x78: return (EAX + (EDI << 1));
		case 0x79: return (ECX + (EDI << 1));
		case 0x7a: return (EDX + (EDI << 1));
		case 0x7b: return (EBX + (EDI << 1));
		case 0x7c: *ovr = LONG_SS; return (ESP + (EDI << 1));
		case 0x7d: *ovr = LONG_SS; return (EBP + (EDI << 1));
		case 0x7e: return (ESI + (EDI << 1));
		case 0x7f: return (EDI + (EDI << 1));
		case 0x80: return (EAX + (EAX << 2));
		case 0x81: return (ECX + (EAX << 2));
		case 0x82: return (EDX + (EAX << 2));
		case 0x83: return (EBX + (EAX << 2));
		case 0x84: *ovr = LONG_SS; return (ESP + (EAX << 2));
		case 0x85: *ovr = LONG_SS; return (EBP + (EAX << 2));
		case 0x86: return (ESI + (EAX << 2));
		case 0x87: return (EDI + (EAX << 2));
		case 0x88: return (EAX + (ECX << 2));
		case 0x89: return (ECX + (ECX << 2));
		case 0x8a: return (EDX + (ECX << 2));
		case 0x8b: return (EBX + (ECX << 2));
		case 0x8c: *ovr = LONG_SS; return (ESP + (ECX << 2));
		case 0x8d: *ovr = LONG_SS; return (EBP + (ECX << 2));
		case 0x8e: return (ESI + (ECX << 2));
		case 0x8f: return (EDI + (ECX << 2));
		case 0x90: return (EAX + (EDX << 2));
		case 0x91: return (ECX + (EDX << 2));
		case 0x92: return (EDX + (EDX << 2));
		case 0x93: return (EBX + (EDX << 2));
		case 0x94: *ovr = LONG_SS; return (ESP + (EDX << 2));
		case 0x95: *ovr = LONG_SS; return (EBP + (EDX << 2));
		case 0x96: return (ESI + (EDX << 2));
		case 0x97: return (EDI + (EDX << 2));
		case 0x98: return (EAX + (EBX << 2));
		case 0x99: return (ECX + (EBX << 2));
		case 0x9a: return (EDX + (EBX << 2));
		case 0x9b: return (EBX + (EBX << 2));
		case 0x9c: *ovr = LONG_SS; return (ESP + (EBX << 2));
		case 0x9d: *ovr = LONG_SS; return (EBP + (EBX << 2));
		case 0x9e: return (ESI + (EBX << 2));
		case 0x9f: return (EDI + (EBX << 2));
/* 10-100-xxx undefined */
		case 0xa8: return (EAX + (EBP << 2));
		case 0xa9: return (ECX + (EBP << 2));
		case 0xaa: return (EDX + (EBP << 2));
		case 0xab: return (EBX + (EBP << 2));
		case 0xac: *ovr = LONG_SS; return (ESP + (EBP << 2));
		case 0xad: *ovr = LONG_SS; return (EBP + (EBP << 2));
		case 0xae: return (ESI + (EBP << 2));
		case 0xaf: return (EDI + (EBP << 2));
		case 0xb0: return (EAX + (ESI << 2));
		case 0xb1: return (ECX + (ESI << 2));
		case 0xb2: return (EDX + (ESI << 2));
		case 0xb3: return (EBX + (ESI << 2));
		case 0xb4: *ovr = LONG_SS; return (ESP + (ESI << 2));
		case 0xb5: *ovr = LONG_SS; return (EBP + (ESI << 2));
		case 0xb6: return (ESI + (ESI << 2));
		case 0xb7: return (EDI + (ESI << 2));
		case 0xb8: return (EAX + (EDI << 2));
		case 0xb9: return (ECX + (EDI << 2));
		case 0xba: return (EDX + (EDI << 2));
		case 0xbb: return (EBX + (EDI << 2));
		case 0xbc: *ovr = LONG_SS; return (ESP + (EDI << 2));
		case 0xbd: *ovr = LONG_SS; return (EBP + (EDI << 2));
		case 0xbe: return (ESI + (EDI << 2));
		case 0xbf: return (EDI + (EDI << 2));
		case 0xc0: return (EAX + (EAX << 3));
		case 0xc1: return (ECX + (EAX << 3));
		case 0xc2: return (EDX + (EAX << 3));
		case 0xc3: return (EBX + (EAX << 3));
		case 0xc4: *ovr = LONG_SS; return (ESP + (EAX << 3));
		case 0xc5: *ovr = LONG_SS; return (EBP + (EAX << 3));
		case 0xc6: return (ESI + (EAX << 3));
		case 0xc7: return (EDI + (EAX << 3));
		case 0xc8: return (EAX + (ECX << 3));
		case 0xc9: return (ECX + (ECX << 3));
		case 0xca: return (EDX + (ECX << 3));
		case 0xcb: return (EBX + (ECX << 3));
		case 0xcc: *ovr = LONG_SS; return (ESP + (ECX << 3));
		case 0xcd: *ovr = LONG_SS; return (EBP + (ECX << 3));
		case 0xce: return (ESI + (ECX << 3));
		case 0xcf: return (EDI + (ECX << 3));
		case 0xd0: return (EAX + (EDX << 3));
		case 0xd1: return (ECX + (EDX << 3));
		case 0xd2: return (EDX + (EDX << 3));
		case 0xd3: return (EBX + (EDX << 3));
		case 0xd4: *ovr = LONG_SS; return (ESP + (EDX << 3));
		case 0xd5: *ovr = LONG_SS; return (EBP + (EDX << 3));
		case 0xd6: return (ESI + (EDX << 3));
		case 0xd7: return (EDI + (EDX << 3));
		case 0xd8: return (EAX + (EBX << 3));
		case 0xd9: return (ECX + (EBX << 3));
		case 0xda: return (EDX + (EBX << 3));
		case 0xdb: return (EBX + (EBX << 3));
		case 0xdc: *ovr = LONG_SS; return (ESP + (EBX << 3));
		case 0xdd: *ovr = LONG_SS; return (EBP + (EBX << 3));
		case 0xde: return (ESI + (EBX << 3));
		case 0xdf: return (EDI + (EBX << 3));
/* 11-100-xxx undefined */
		case 0xe8: return (EAX + (EBP << 3));
		case 0xe9: return (ECX + (EBP << 3));
		case 0xea: return (EDX + (EBP << 3));
		case 0xeb: return (EBX + (EBP << 3));
		case 0xec: *ovr = LONG_SS; return (ESP + (EBP << 3));
		case 0xed: *ovr = LONG_SS; return (EBP + (EBP << 3));
		case 0xee: return (ESI + (EBP << 3));
		case 0xef: return (EDI + (EBP << 3));
		case 0xf0: return (EAX + (ESI << 3));
		case 0xf1: return (ECX + (ESI << 3));
		case 0xf2: return (EDX + (ESI << 3));
		case 0xf3: return (EBX + (ESI << 3));
		case 0xf4: *ovr = LONG_SS; return (ESP + (ESI << 3));
		case 0xf5: *ovr = LONG_SS; return (EBP + (ESI << 3));
		case 0xf6: return (ESI + (ESI << 3));
		case 0xf7: return (EDI + (ESI << 3));
		case 0xf8: return (EAX + (EDI << 3));
		case 0xf9: return (ECX + (EDI << 3));
		case 0xfa: return (EDX + (EDI << 3));
		case 0xfb: return (EBX + (EDI << 3));
		case 0xfc: *ovr = LONG_SS; return (ESP + (EDI << 3));
		case 0xfd: *ovr = LONG_SS; return (EBP + (EDI << 3));
		case 0xfe: return (ESI + (EDI << 3));
		case 0xff: return (EDI + (EDI << 3));
		default:   FatalAppExit(0, "SIB"); return 1;
	}
	return 0;	/* because of gcc warning */
}


#if !defined(NO_DEBUG_MSGS) && defined(MODRM_DEBUG)
/* WARNING! */
#define return(x) ret=(x);break
#define DEBUG_ENTRY int ret=0;

#define DEBUG_EXIT(args...) \
	if (d_emu>4) e_printf(##args); \
	return ret  /* NOTE: use 'return xx', _not_ 'return(xx)' !!! */

#else
  #define DEBUG_ENTRY
  #define DEBUG_EXIT
#endif

int
hsw_modrm_16_byte(Interp_ENV *env, unsigned char *mPC)
{
	DEBUG_ENTRY
	switch(*(mPC+1)) {
		case MOD_AL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
	}
#ifdef MODRM_DEBUG
	DEBUG_EXIT("------- [EA=%p]=%02x\n",MEM_REF,*((unsigned char *)MEM_REF));
#else
	return 0;
#endif
}


int
hsw_modrm_16_word(Interp_ENV *env, unsigned char *mPC)
{
	DEBUG_ENTRY
	switch(*(mPC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
	}
#ifdef MODRM_DEBUG
	DEBUG_EXIT("------- [EA=%p]=%04x\n",MEM_REF,*((unsigned short *)MEM_REF));
#else
	return 0;
#endif
}


int
hsw_modrm_16_quad(Interp_ENV *env, unsigned char *mPC)
{
	DEBUG_ENTRY
	switch(*(mPC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((mPC+2)));
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(mPC+2))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((mPC+2)))&0xffff);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
	}
#ifdef MODRM_DEBUG
	DEBUG_EXIT("------- [EA=%p]=%08x\n",MEM_REF,*((unsigned int *)MEM_REF));
#else
	return 0;
#endif
}


int
hsw_modrm_32_byte(Interp_ENV *env, unsigned char *mPC)
{
	unsigned char *ovr;
	int dsp;
	DEBUG_ENTRY
	switch(*(mPC+1)) {
		case MOD_AL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_AL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_CL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_DL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_BL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_AH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_CH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_DH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_BH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(7);
		case MOD_AL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(7);
		case MOD_CL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(7);
		case MOD_DL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(7);
		case MOD_BL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(7);
		case MOD_AH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(7);
		case MOD_CH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(7);
		case MOD_DH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(7);
		case MOD_BH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AL: 
			MEM_REF = &(AL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CL: 
			MEM_REF = &(CL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DL: 
			MEM_REF = &(DL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BL: 
			MEM_REF = &(BL);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AH: 
			MEM_REF = &(AH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CH: 
			MEM_REF = &(CH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DH: 
			MEM_REF = &(DH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BH: 
			MEM_REF = &(BH);
			env->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
	}
#ifdef MODRM_DEBUG
	DEBUG_EXIT("------- [EA=%p]=%02x\n",MEM_REF,*((unsigned char *)MEM_REF));
#else
	return 0;
#endif
}


int
hsw_modrm_32_word(Interp_ENV *env, unsigned char *mPC)
{
	unsigned char *ovr;
	int dsp;
	DEBUG_ENTRY
	switch(*(mPC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(7);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(7);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(7);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(7);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(7);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(7);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(7);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(7);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			env->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
	}
#ifdef MODRM_DEBUG
	DEBUG_EXIT("------- [EA=%p]=%04x\n",MEM_REF,*((unsigned short *)MEM_REF));
#else
	return 0;
#endif
}


int
hsw_modrm_32_quad(Interp_ENV *env, unsigned char *mPC)
{
	unsigned char *ovr;
	int dsp;
	DEBUG_ENTRY
	switch(*(mPC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: {
			unsigned char sib = *(mPC+2);
			int incpc = 3;
			if ((sib&7)==5) {
			  dsp = hsw_modrm_sibd(env,sib,(mPC+3));
			  ovr = LONG_DS; incpc=7;
			}
			else
			  dsp = hsw_modrm_sib(env,sib,&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(incpc); }
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((mPC+2)));
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+(signed char)*(mPC+3));
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(7);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(7);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(7);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(7);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(7);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(7);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(7);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_SIimm16: 
			dsp = hsw_modrm_sib(env,*(mPC+2),&ovr);
			MEM_REF = ALLOW_OVERRIDE(ovr) + (dsp+FETCH_QUAD(mPC+3));
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(7);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+(EBP)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(mPC+2);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			env->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
	}
#ifdef MODRM_DEBUG
	DEBUG_EXIT("------- [EA=%p]=%08x\n",MEM_REF,*((unsigned int *)MEM_REF));
#else
	return 0;
#endif
}
