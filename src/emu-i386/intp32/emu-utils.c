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
#include "config.h"

#ifdef X86_EMULATOR
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TWIN
#include "hsw_interp.h"

#undef CARRY_FLAG	/* well-coordinated work */
#undef ZERO_FLAG
#include "kerndef.h"
#include "BinTypes.h"

#include "utils.h"
#include "Log.h"
#include <ctype.h>
#include "emu-ldt.h"
#include "port.h"

#define FP_DISPHEX
 
static int isEMUon = 0;

extern Interp_ENV *envp_global;
extern long instr_count;
#ifdef DOSEMU
extern int in_dpmi, in_dpmi_emu;
extern void leavedos(int) NORETURN;
#endif

void WINAPI
FatalAppExit(UINT wAction,LPCSTR lpText)
{
    extern void print_ldt(void);
    if (lpText) {
    	fprintf(stderr, "%s\n", lpText);
    	dbug_printf("FATAL in CPUEMU: %s\n", lpText);
    }
    if (in_dpmi) {
	d.dpmi=1; in_dpmi_emu=0; print_ldt();
    }
#ifdef DOSEMU
    leavedos(173);
#else
    exit(0);
#endif
}


void e_priv_iopl(int pl)
{
    pl &= 3;
    envp_global->flags = (envp_global->flags & ~IOPL_FLAG_MASK) |
	(pl << 12);
    e_printf("eIOPL: set IOPL to %d, flags=%#lx\n",pl,envp_global->flags);
}


extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *,
                     unsigned int, int);

static char *e_emu_disasm(Interp_ENV *env, Interp_VAR *interp_var,
	unsigned char *org, int is32)
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

static char *e_print_cpuemu_regs(Interp_ENV *env, Interp_VAR *interp_var,
	int is32)
{
	static char buf[1024];
	void *sp;
	int i, tsp;
	char *p;

	*buf = 0;
	if (d.emu>3) {
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
	  if (vm86f) {
	    tsp = e_SP;
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

static char *e_print_internal_regs(Interp_VAR *v)
{
	static char buf[1024];
	char *p;

	*buf = 0;
	if (d.emu>6) {
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

void e_debug_fp(ENV87 *ef)
{
	int i, ifpr;
	ifpr = ef->fpstt;
	for (i=0; i<8; i++) {
#ifdef FP_DISPHEX
	  { void *q = (void *)&(ef->fpregs[ifpr]);
	    e_printf("FP%d\t%04x %016Lx\n", ifpr, ((unsigned short *)q)[4],
	    *((unsigned long long *)q));
	  }
#else
	  e_printf("FP%d\t%18.8Lf\n", ifpr, ef->fpregs[ifpr]);
#endif
	  ifpr = (ifpr+1) & 7;
	}
	e_printf(" sw=%04x cw=%04x\n", ef->fpus, ef->fpuc);
}


void e_debug (Interp_ENV *env, unsigned char *P0, unsigned char *PC,
  	Interp_VAR *interp_var, int is32)
{
	if ((d.emu>2) && TRACE_HIGH && (P0==PC)) {
	    e_printf("%ld\n%s%s    %s\n", instr_count,
	    	e_print_cpuemu_regs(env,interp_var,is32),
		e_print_internal_regs(interp_var),
		e_emu_disasm(env,interp_var,PC,is32));
	    isEMUon = 1;
	}
}

#endif	/* X86_EMULATOR */
