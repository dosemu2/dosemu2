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
 */

#include "hsw_interp.h"
#include "mod_rm.h"

int
hsw_modrm_sib(Interp_ENV *env, int sib)
{
	switch(sib) {
		case 0: return (EAX + EAX);
		case 1: return (ECX + EAX);
		case 2: return (EDX + EAX);
		case 3: return (EBX + EAX);
		case 4: return (ESP + EAX);
		case 5: return (EBP + EAX);
		case 6: return (ESI + EAX);
		case 7: return (EDI + EAX);
		case 8: return (EAX + ECX);
		case 9: return (ECX + ECX);
		case 10: return (EDX + ECX);
		case 11: return (EBX + ECX);
		case 12: return (ESP + ECX);
		case 13: return (EBP + ECX);
		case 14: return (ESI + ECX);
		case 15: return (EDI + ECX);
		case 16: return (EAX + EDX);
		case 17: return (ECX + EDX);
		case 18: return (EDX + EDX);
		case 19: return (EBX + EDX);
		case 20: return (ESP + EDX);
		case 21: return (EBP + EDX);
		case 22: return (ESI + EDX);
		case 23: return (EDI + EDX);
		case 24: return (EAX + EBX);
		case 25: return (ECX + EBX);
		case 26: return (EDX + EBX);
		case 27: return (EBX + EBX);
		case 28: return (ESP + EBX);
		case 29: return (EBP + EBX);
		case 30: return (ESI + EBX);
		case 31: return (EDI + EBX);
		case 32: return (EAX);
		case 33: return (ECX);
		case 34: return (EDX);
		case 35: return (EBX);
		case 36: return (ESP);
		case 37: return (EBP);
		case 38: return (ESI);
		case 39: return (EDI);
		case 40: return (EAX + EBP);
		case 41: return (ECX + EBP);
		case 42: return (EDX + EBP);
		case 43: return (EBX + EBP);
		case 44: return (ESP + EBP);
		case 45: return (EBP + EBP);
		case 46: return (ESI + EBP);
		case 47: return (EDI + EBP);
		case 48: return (EAX + ESI);
		case 49: return (ECX + ESI);
		case 50: return (EDX + ESI);
		case 51: return (EBX + ESI);
		case 52: return (ESP + ESI);
		case 53: return (EBP + ESI);
		case 54: return (ESI + ESI);
		case 55: return (EDI + ESI);
		case 56: return (EAX + EDI);
		case 57: return (ECX + EDI);
		case 58: return (EDX + EDI);
		case 59: return (EBX + EDI);
		case 60: return (ESP + EDI);
		case 61: return (EBP + EDI);
		case 62: return (ESI + EDI);
		case 63: return (EDI + EDI);
		case 64: return (EAX + (EAX << 1));
		case 65: return (ECX + (EAX << 1));
		case 66: return (EDX + (EAX << 1));
		case 67: return (EBX + (EAX << 1));
		case 68: return (ESP + (EAX << 1));
		case 69: return (EBP + (EAX << 1));
		case 70: return (ESI + (EAX << 1));
		case 71: return (EDI + (EAX << 1));
		case 72: return (EAX + (ECX << 1));
		case 73: return (ECX + (ECX << 1));
		case 74: return (EDX + (ECX << 1));
		case 75: return (EBX + (ECX << 1));
		case 76: return (ESP + (ECX << 1));
		case 77: return (EBP + (ECX << 1));
		case 78: return (ESI + (ECX << 1));
		case 79: return (EDI + (ECX << 1));
		case 80: return (EAX + (EDX << 1));
		case 81: return (ECX + (EDX << 1));
		case 82: return (EDX + (EDX << 1));
		case 83: return (EBX + (EDX << 1));
		case 84: return (ESP + (EDX << 1));
		case 85: return (EBP + (EDX << 1));
		case 86: return (ESI + (EDX << 1));
		case 87: return (EDI + (EDX << 1));
		case 88: return (EAX + (EBX << 1));
		case 89: return (ECX + (EBX << 1));
		case 90: return (EDX + (EBX << 1));
		case 91: return (EBX + (EBX << 1));
		case 92: return (ESP + (EBX << 1));
		case 93: return (EBP + (EBX << 1));
		case 94: return (ESI + (EBX << 1));
		case 95: return (EDI + (EBX << 1));
		case 96: return (EAX);
		case 97: return (ECX);
		case 98: return (EDX);
		case 99: return (EBX);
		case 100: return (ESP);
		case 101: return (EBP);
		case 102: return (ESI);
		case 103: return (EDI);
		case 104: return (EAX + (EBP << 1));
		case 105: return (ECX + (EBP << 1));
		case 106: return (EDX + (EBP << 1));
		case 107: return (EBX + (EBP << 1));
		case 108: return (ESP + (EBP << 1));
		case 109: return (EBP + (EBP << 1));
		case 110: return (ESI + (EBP << 1));
		case 111: return (EDI + (EBP << 1));
		case 112: return (EAX + (ESI << 1));
		case 113: return (ECX + (ESI << 1));
		case 114: return (EDX + (ESI << 1));
		case 115: return (EBX + (ESI << 1));
		case 116: return (ESP + (ESI << 1));
		case 117: return (EBP + (ESI << 1));
		case 118: return (ESI + (ESI << 1));
		case 119: return (EDI + (ESI << 1));
		case 120: return (EAX + (EDI << 1));
		case 121: return (ECX + (EDI << 1));
		case 122: return (EDX + (EDI << 1));
		case 123: return (EBX + (EDI << 1));
		case 124: return (ESP + (EDI << 1));
		case 125: return (EBP + (EDI << 1));
		case 126: return (ESI + (EDI << 1));
		case 127: return (EDI + (EDI << 1));
		case 128: return (EAX + (EAX << 2));
		case 129: return (ECX + (EAX << 2));
		case 130: return (EDX + (EAX << 2));
		case 131: return (EBX + (EAX << 2));
		case 132: return (ESP + (EAX << 2));
		case 133: return (EBP + (EAX << 2));
		case 134: return (ESI + (EAX << 2));
		case 135: return (EDI + (EAX << 2));
		case 136: return (EAX + (ECX << 2));
		case 137: return (ECX + (ECX << 2));
		case 138: return (EDX + (ECX << 2));
		case 139: return (EBX + (ECX << 2));
		case 140: return (ESP + (ECX << 2));
		case 141: return (EBP + (ECX << 2));
		case 142: return (ESI + (ECX << 2));
		case 143: return (EDI + (ECX << 2));
		case 144: return (EAX + (EDX << 2));
		case 145: return (ECX + (EDX << 2));
		case 146: return (EDX + (EDX << 2));
		case 147: return (EBX + (EDX << 2));
		case 148: return (ESP + (EDX << 2));
		case 149: return (EBP + (EDX << 2));
		case 150: return (ESI + (EDX << 2));
		case 151: return (EDI + (EDX << 2));
		case 152: return (EAX + (EBX << 2));
		case 153: return (ECX + (EBX << 2));
		case 154: return (EDX + (EBX << 2));
		case 155: return (EBX + (EBX << 2));
		case 156: return (ESP + (EBX << 2));
		case 157: return (EBP + (EBX << 2));
		case 158: return (ESI + (EBX << 2));
		case 159: return (EDI + (EBX << 2));
		case 160: return (EAX);
		case 161: return (ECX);
		case 162: return (EDX);
		case 163: return (EBX);
		case 164: return (ESP);
		case 165: return (EBP);
		case 166: return (ESI);
		case 167: return (EDI);
		case 168: return (EAX + (EBP << 2));
		case 169: return (ECX + (EBP << 2));
		case 170: return (EDX + (EBP << 2));
		case 171: return (EBX + (EBP << 2));
		case 172: return (ESP + (EBP << 2));
		case 173: return (EBP + (EBP << 2));
		case 174: return (ESI + (EBP << 2));
		case 175: return (EDI + (EBP << 2));
		case 176: return (EAX + (ESI << 2));
		case 177: return (ECX + (ESI << 2));
		case 178: return (EDX + (ESI << 2));
		case 179: return (EBX + (ESI << 2));
		case 180: return (ESP + (ESI << 2));
		case 181: return (EBP + (ESI << 2));
		case 182: return (ESI + (ESI << 2));
		case 183: return (EDI + (ESI << 2));
		case 184: return (EAX + (EDI << 2));
		case 185: return (ECX + (EDI << 2));
		case 186: return (EDX + (EDI << 2));
		case 187: return (EBX + (EDI << 2));
		case 188: return (ESP + (EDI << 2));
		case 189: return (EBP + (EDI << 2));
		case 190: return (ESI + (EDI << 2));
		case 191: return (EDI + (EDI << 2));
		case 192: return (EAX + (EAX << 3));
		case 193: return (ECX + (EAX << 3));
		case 194: return (EDX + (EAX << 3));
		case 195: return (EBX + (EAX << 3));
		case 196: return (ESP + (EAX << 3));
		case 197: return (EBP + (EAX << 3));
		case 198: return (ESI + (EAX << 3));
		case 199: return (EDI + (EAX << 3));
		case 200: return (EAX + (ECX << 3));
		case 201: return (ECX + (ECX << 3));
		case 202: return (EDX + (ECX << 3));
		case 203: return (EBX + (ECX << 3));
		case 204: return (ESP + (ECX << 3));
		case 205: return (EBP + (ECX << 3));
		case 206: return (ESI + (ECX << 3));
		case 207: return (EDI + (ECX << 3));
		case 208: return (EAX + (EDX << 3));
		case 209: return (ECX + (EDX << 3));
		case 210: return (EDX + (EDX << 3));
		case 211: return (EBX + (EDX << 3));
		case 212: return (ESP + (EDX << 3));
		case 213: return (EBP + (EDX << 3));
		case 214: return (ESI + (EDX << 3));
		case 215: return (EDI + (EDX << 3));
		case 216: return (EAX + (EBX << 3));
		case 217: return (ECX + (EBX << 3));
		case 218: return (EDX + (EBX << 3));
		case 219: return (EBX + (EBX << 3));
		case 220: return (ESP + (EBX << 3));
		case 221: return (EBP + (EBX << 3));
		case 222: return (ESI + (EBX << 3));
		case 223: return (EDI + (EBX << 3));
		case 224: return (EAX);
		case 225: return (ECX);
		case 226: return (EDX);
		case 227: return (EBX);
		case 228: return (ESP);
		case 229: return (EBP);
		case 230: return (ESI);
		case 231: return (EDI);
		case 232: return (EAX + (EBP << 3));
		case 233: return (ECX + (EBP << 3));
		case 234: return (EDX + (EBP << 3));
		case 235: return (EBX + (EBP << 3));
		case 236: return (ESP + (EBP << 3));
		case 237: return (EBP + (EBP << 3));
		case 238: return (ESI + (EBP << 3));
		case 239: return (EDI + (EBP << 3));
		case 240: return (EAX + (ESI << 3));
		case 241: return (ECX + (ESI << 3));
		case 242: return (EDX + (ESI << 3));
		case 243: return (EBX + (ESI << 3));
		case 244: return (ESP + (ESI << 3));
		case 245: return (EBP + (ESI << 3));
		case 246: return (ESI + (ESI << 3));
		case 247: return (EDI + (ESI << 3));
		case 248: return (EAX + (EDI << 3));
		case 249: return (ECX + (EDI << 3));
		case 250: return (EDX + (EDI << 3));
		case 251: return (EBX + (EDI << 3));
		case 252: return (ESP + (EDI << 3));
		case 253: return (EBP + (EDI << 3));
		case 254: return (ESI + (EDI << 3));
		case 255: return (EDI + (EDI << 3));
	}
	return 0;	/* because of gcc warning */
}


#if defined(DOSEMU) && defined(DEBUG)
  #define return(x) ret=(x);break
  #define DEBUG_ENTRY int ret=0;
  #define DEBUG_EXIT(args...) \
	if (!IS_MODE_REG && TRACE_HIGH && (d.emu>2)) e_printf(##args); \
	return ret  /* NOTE: use 'return xx', _not_ 'return(xx)' !!! */
#else
  #define DEBUG_ENTRY
  #define DEBUG_EXIT(args...) return 0
#endif

int
hsw_modrm_16_byte(Interp_ENV *env, unsigned char *PC, Interp_VAR *interp_var)
{
	DEBUG_ENTRY
	switch(*(PC+1)) {
		case MOD_AL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
	}
	DEBUG_EXIT("------- ref [%p]=%02x\n",MEM_REF,*((unsigned char *)MEM_REF));
}


int
hsw_modrm_16_word(Interp_ENV *env, unsigned char *PC, Interp_VAR *interp_var)
{
	DEBUG_ENTRY
	switch(*(PC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
	}
	DEBUG_EXIT("------- ref [%p]=%04x\n",MEM_REF,*((unsigned short *)MEM_REF));
}


int
hsw_modrm_16_quad(Interp_ENV *env, unsigned char *PC, Interp_VAR *interp_var)
{
	DEBUG_ENTRY
	switch(*(PC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI)&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(SI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(DI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_WORD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(BX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+(signed char)*(PC+2))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((SI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((DI+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_SS)+((BP+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+((BX+FETCH_WORD((PC+2)))&0xffff);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
	}
	DEBUG_EXIT("------- ref [%p]=%08x\n",MEM_REF,*((unsigned int *)MEM_REF));
}


int
hsw_modrm_32_byte(Interp_ENV *env, unsigned char *PC, Interp_VAR *interp_var)
{
	DEBUG_ENTRY
	switch(*(PC+1)) {
		case MOD_AL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_CL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_DL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_BL_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_AH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_CH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_DH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_BH_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(2);
		case MOD_AL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(4);
		case MOD_AL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(4);
		case MOD_CL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_CL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(4);
		case MOD_DL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_DL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(4);
		case MOD_BL_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_BL_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(4);
		case MOD_AH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_AH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(4);
		case MOD_CH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_CH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(4);
		case MOD_DH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_DH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(4);
		case MOD_BH_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_BH_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(3);
		case MOD_AL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(7);
		case MOD_AL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(7);
		case MOD_CL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_CL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(7);
		case MOD_DL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_DL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(7);
		case MOD_BL_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_BL_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(7);
		case MOD_AH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_AH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(7);
		case MOD_CH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_CH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(7);
		case MOD_DH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_DH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(7);
		case MOD_BH_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_BH_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 0; return(6);
		case MOD_AL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_AL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(AL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_CL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(CL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_DL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(DL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_BL_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(BL);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_AH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(AH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_CH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(CH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_DH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(DH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AL: 
			MEM_REF = &(AL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CL: 
			MEM_REF = &(CL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DL: 
			MEM_REF = &(DL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BL: 
			MEM_REF = &(BL);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_AH: 
			MEM_REF = &(AH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_CH: 
			MEM_REF = &(CH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_DH: 
			MEM_REF = &(DH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
		case MOD_BH_EB_BH: 
			MEM_REF = &(BH);
			interp_var->reg1 = &(BH);
			IS_MODE_REG = 1; return(2);
	}
	DEBUG_EXIT("------- ref [%p]=%02x\n",MEM_REF,*((unsigned char *)MEM_REF));
}


int
hsw_modrm_32_word(Interp_ENV *env, unsigned char *PC, Interp_VAR *interp_var)
{
	DEBUG_ENTRY
	switch(*(PC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(7);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(7);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(7);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(7);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(7);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(7);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(7);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(7);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(AX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(CX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(DX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(BX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(SP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(BP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(SI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(AX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(CX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(DX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(BX);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(SP);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(BP);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(SI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(DI);
			interp_var->reg1 = (unsigned char *)&(DI);
			IS_MODE_REG = 1; return(2);
	}
	DEBUG_EXIT("------- ref [%p]=%04x\n",MEM_REF,*((unsigned short *)MEM_REF));
}


int
hsw_modrm_32_quad(Interp_ENV *env, unsigned char *PC, Interp_VAR *interp_var)
{
	DEBUG_ENTRY
	switch(*(PC+1)) {
		case MOD_AX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_CX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_DX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_BX_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_SP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_BP_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_SI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BXDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPSI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BPDI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_SI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2)));
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_DI: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(FETCH_QUAD((PC+2)));
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_imm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_DI_BX: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(2);
		case MOD_AX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(4);
		case MOD_AX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(4);
		case MOD_CX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_CX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(4);
		case MOD_DX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_DX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(4);
		case MOD_BX_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_BX_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(4);
		case MOD_SP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_SP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(4);
		case MOD_BP_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_BP_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(4);
		case MOD_SI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_SI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPSIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPDIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_SIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+(signed char)*(PC+3));
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(4);
		case MOD_DI_DIimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BPimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_DI_BXimm8: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+(signed char)*(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(3);
		case MOD_AX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(7);
		case MOD_AX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(7);
		case MOD_CX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_CX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(7);
		case MOD_DX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_DX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(7);
		case MOD_BX_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_BX_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(7);
		case MOD_SP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_SP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(7);
		case MOD_BP_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_BP_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(7);
		case MOD_SI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_SI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EAX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ECX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPSIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPDIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBX)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_SIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(hsw_modrm_sib(env,*(PC+2))+FETCH_QUAD(PC+3));
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(7);
		case MOD_DI_DIimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EBP)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BPimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(ESI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_DI_BXimm16: 
			MEM_REF = ALLOW_OVERRIDE(LONG_DS)+(EDI)+FETCH_QUAD(PC+2);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 0; return(6);
		case MOD_AX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_AX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EAX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_CX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(ECX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_DX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EDX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_BX_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EBX);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_SP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(ESP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_BP_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EBP);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_SI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(ESI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_AX: 
			MEM_REF = (unsigned char *)&(EAX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_CX: 
			MEM_REF = (unsigned char *)&(ECX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DX: 
			MEM_REF = (unsigned char *)&(EDX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BX: 
			MEM_REF = (unsigned char *)&(EBX);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SP: 
			MEM_REF = (unsigned char *)&(ESP);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_BP: 
			MEM_REF = (unsigned char *)&(EBP);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_SI: 
			MEM_REF = (unsigned char *)&(ESI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
		case MOD_DI_EW_DI: 
			MEM_REF = (unsigned char *)&(EDI);
			interp_var->reg1 = (unsigned char *)&(EDI);
			IS_MODE_REG = 1; return(2);
	}
	DEBUG_EXIT("------- ref [%p]=%08x\n",MEM_REF,*((unsigned int *)MEM_REF));
}
