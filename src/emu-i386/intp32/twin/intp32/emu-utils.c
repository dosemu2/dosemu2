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
	utils.c	2.20
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

#include "emu-globv.h"
#ifdef DOSEMU
#include "config.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hsw_interp.h"

#undef CARRY_FLAG	/* well-coordinated work */
#undef ZERO_FLAG

#include <ctype.h>
#include "emu-ldt.h"

#undef	FP_DISPHEX
 
static int isEMUon = 0;

extern Interp_ENV *envp_global;
extern long instr_count;
#ifdef DOSEMU
extern int in_dpmi, in_dpmi_emu;
extern void leavedos(int) NORETURN;
#else
#define in_dpmi	0
#endif

#ifdef DOSEMU
void
FatalAppExit(UINT wAction, LPCSTR lpText)
{
    extern void print_ldt(void);
    if (lpText) {
    	fprintf(stderr, "%s\n", lpText);
    	dbug_printf("FATAL in CPUEMU: %s\n", lpText);
    }
    if (in_dpmi) {
	d.dpmi=1; in_dpmi_emu=0; print_ldt();
    }
    leavedos(173);
}
#endif


void e_priv_iopl(int pl)
{
    pl &= 3;
    envp_global->flags = (envp_global->flags & ~IOPL_FLAG_MASK) |
	(pl << 12);
    e_printf("eIOPL: set IOPL to %d, flags=%#lx\n",pl,envp_global->flags);
}


#ifndef NO_TRACE_MSGS
#ifdef DOSEMU
extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *,
                     unsigned int, int);
#else

int opcode_table[256];

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

int dis_8086(unsigned int org, const unsigned char *code,
                     unsigned char *outbuf, int def_size,
		     unsigned int *refseg, unsigned int *refoff,
                     unsigned int refsegbase, int nlines)
{
/* TBD - better check dosemu::src/arch/linux/debugger/dis8086.c */
  return 1;
}
#endif

static char *e_emu_disasm(Interp_ENV *env, unsigned char *org, int is32)
{
   static unsigned char buf[256];
   unsigned char frmtbuf[256];
   int rc;
   int i;
   unsigned char *p;
   unsigned int seg;
   unsigned int refseg;
   unsigned int ref;

   seg = SHORT_CS_16;
   refseg = seg;

   rc = dis_8086((unsigned long)org, org, frmtbuf, is32, &refseg, &ref,
   	(in_dpmi? (int)LONG_CS : 0), 1);

   p = buf + sprintf(buf,"%08lx: ",(long)org);
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", *(org+i));
   }
   sprintf(p,"%20s", " ");
   sprintf(buf+28, "%04x:%04x %s", seg, org-LONG_CS, frmtbuf);

   return buf;
}

#ifdef VERBOSE_FLAGS
char *Fnb0[16] = {
	"....","...C","....","...C",".P..",".P.C",".P..",".P.C",
	"....","...C","....","...C",".P..",".P.C",".P..",".P.C"
};

char *Fnb1[16] = {
	"....","...A","....","...A",".Z..",".Z.A",".Z..",".Z.A",
	"S...","S..A","S...","S..A","SZ..","SZ.A","SZ..","SZ.A"
};

char *Fnb2[16] = {
	"....","...T","..I.","..IT",".D..",".D.T",".DI.",".DIT",
	"O...","O..T","O.I.","O.IT","OD..","OD.T","ODI.","ODIT"
};

char *Fnb4[16] = {
	"....","...R","..V.","..VR",".A..",".A.R",".AV.",".AVR",
	"F...","F..R","F.V.","F.VR","FA..","FA.R","FAV.","FAVR"
};

char *Fnb5[4] = {
	"....","...P","..I.","..IP"
};
#endif

static char *e_print_cpuemu_regs(Interp_ENV *env, int is32)
{
	static char buf[1024];
	void *sp;
	int i, tsp;
	char *p;

	*buf = 0;
	if (d_emu>3) {
	  long lflags;
	  p = buf;
	  p += sprintf(p,"\teax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n",
		e_EAX,e_EBX,e_ECX,e_EDX);
	  p += sprintf(p,"\tesi=%08lx edi=%08lx ebp=%08lx esp=%08lx\n",
		e_ESI,e_EDI,e_EBP,e_ESP);
	  p += sprintf(p,"\teip=%08lx  cs=%04x      ds=%04x      es=%04x\n",
		e_EIP,e_CS,e_DS,e_ES);
	  lflags = (e_EFLAGS&0x3fff3e)|(CARRY&1)|(RES_8&0x80)|((RES_16==0)<<6);
	  p += sprintf(p,"\t fs=%04x      gs=%04x      ss=%04x     ",
		e_FS,e_GS,e_SS);
#ifdef VERBOSE_FLAGS
	  p += sprintf(p,"flg=%s%s..11%s%s%s\n",Fnb5[(lflags>>20)&3],
	  	Fnb4[(lflags>>16)&15],Fnb2[(lflags>>8)&15],
	  	Fnb1[(lflags>>4)&15],Fnb0[lflags&15]);
#else
	  p += sprintf(p,"flg=%08lx\n",lflags);
#endif
	  if (VM86F) {
	    tsp = e_SP;
	    if ((int)tsp & 1)
	      p += sprintf(p," -odd-  stk=");
	    else
	      p += sprintf(p,"\tstk=");
	    for (i=0; (i<10)&&(tsp<0x10000); i++) {
	      sp = (void *)((e_SS<<4)+tsp);
	      p += sprintf(p," %04x",*((unsigned short *)sp));
	      tsp += 2;
	    }
	    for (; i<10; i++) p+=sprintf(p," ****");
	    p+=sprintf(p,"\n");
	  }
	  else {
	    sp = (void *)(LONG_SS + (BIG_SS? e_ESP:e_SP));
	    p += sprintf(p,"\tstk=");
	    if ((unsigned long)sp > (unsigned long)GetSelectorAddrMax(e_SS)) {
	        p += sprintf(p,"%#lx out of limit\n",(long)sp);
	    	return buf;
	    }
	    if (code32) {
	      sp = (void *)(((unsigned long)sp)&(-4));	/* align */
	      for (i=0; i<6; i++) {
	        p += sprintf(p," %08lx",*((unsigned long *)sp)++);
	      }
	      for (; i<6; i++) p+=sprintf(p," ********");
	    }
	    else {
	      sp = (void *)(((unsigned long)sp)&(-2));	/* align */
	      for (i=0; i<10; i++) {
	        p += sprintf(p," %04x",*((unsigned short *)sp)++);
	      }
	      for (; i<10; i++) p+=sprintf(p," ****");
	    }
	    p+=sprintf(p,"\n");
	  }
	}
	return buf;
}

static char *e_print_internal_regs(Interp_ENV *v)
{
	static char buf[1024];
	char *p;

	*buf = 0;
	if (d_emu>6) {
	  p = buf;
	  p += sprintf(p,"\t    --RS32-- |-CY- RS16| |BY CB R8 P16|    lCS=%08x\n",
		(int)v->seg_regs[0]);
	  if (v->flags_czsp.byte.byte_op==BYTE_OP)
	    p += sprintf(p,"\tFL: %08lx              %02x %02x %02x %02x      lDS=%08x\n",
		v->flags_czsp.res,
		v->flags_czsp.byte.byte_op,v->flags_czsp.byte.carryb,
		v->flags_czsp.byte.res8,v->flags_czsp.byte.parity16,
		(int)v->seg_regs[1]);
	  else
	    p += sprintf(p,"\tFL: %08lx  %04x %04x                    lDS=%08x\n",
		v->flags_czsp.res,
		v->flags_czsp.word16.carry,v->flags_czsp.word16.res16,
		(int)v->seg_regs[1]);
	  p += sprintf(p,"\t    --SR32-- |---- SR16| |-- -- S8 AUX|    lES=%08x\n",
		(int)v->seg_regs[2]);
	  if (v->flags_czsp.byte.byte_op==BYTE_OP) {
	    p += sprintf(p,"\tS1: %08lx                    %02x         lSS=%08x\n",
		v->src1.longword,
		v->src1.byte.byte,
		(int)v->seg_regs[3]);
	    p += sprintf(p,"\tS2: %08lx                    %02x         OVR=%08x\n",
		v->src2.longword,
		v->src2.byte.byte,
		(int)v->seg_regs[6]);
	  }
	  else {
	    p += sprintf(p,"\tS1: %08lx       %04x                    lSS=%08x\n",
		v->src1.longword,
		v->src1.word16.word,
		(int)v->seg_regs[3]);
	    p += sprintf(p,"\tS2: %08lx       %04x                    OVR=%08x\n",
		v->src2.longword,
		v->src2.word16.word,
		(int)v->seg_regs[6]);
	  }
	}
	return buf;
}

void e_trace_fp(ENV87 *ef)
{
	int i, ifpr, fpus;

	ifpr = ef->fpstt&7;
	fpus = (ef->fpus & (~0x3800)) | (ifpr<<11);
	for (i=0; i<8; i++) {
#ifdef FP_DISPHEX
	  { void *q = (void *)&(ef->fpregs[ifpr]);
	    e_printf("FP%d\t%04x %016Lx\n", ifpr, ((unsigned short *)q)[4],
	    *((unsigned long long *)q));
	  }
#else
	  switch ((ef->fptag >> (ifpr<<1)) & 3) {
	    case 0: case 1:
		e_printf("FP%d\t%18.8Lf\n", ifpr, ef->fpregs[ifpr]);
		break;
	    case 2: e_printf("FP%d\tNaN/Inf\n", ifpr);
	    	break;
	    case 3: e_printf("FP%d\t****\n", ifpr);
	    	break;
	  }
#endif
	  ifpr = (ifpr+1) & 7;
	}
	e_printf(" sw=%04x cw=%04x tag=%04x\n", fpus, ef->fpuc, ef->fptag);
}


void e_trace (Interp_ENV *env, unsigned char *dP0, unsigned char *dPC,
  	int is32)
{
	if ((d_emu>2) && TRACE_HIGH && (dP0==dPC)) {
	    e_printf("%ld\n%s%s    %s\n", instr_count,
		((((*dPC&0xf8)==0xd8) || (CEmuStat & CeS_LOCK)) ?
		    "" :
		    e_print_cpuemu_regs(env,is32)),
		e_print_internal_regs(env),
		e_emu_disasm(env,dPC,is32));
	    isEMUon = 1;
	}
}

#endif	/* TRACE_MSGS */
