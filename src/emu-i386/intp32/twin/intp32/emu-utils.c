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

#undef TRACE_STACK

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
extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *,
                     unsigned int, int);


static char *e_emu_disasm(Interp_ENV *env, unsigned char *org, int is32)
{
   static unsigned char buf[256];
   static unsigned char frmtbuf[256];
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

static char ehextab[16] = "0123456789abcdef";

static char eregbuf[] =
/*00*/	"\teax=00000000 ebx=00000000 ecx=00000000 edx=00000000\n"
/*35*/	"\tesi=00000000 edi=00000000 ebp=00000000 esp=00000000\n"
/*6a*/	"\teip=00000000  cs=0000      ds=0000      es=0000    \n"
/*9f*/	"\t fs=0000      gs=0000      ss=0000     flg=00000000\n";

static inline void exprintl(unsigned long val,char *buf,int pos)
{
	char *p=buf+pos+7;
	unsigned long v = val;
	while (v) { *p-- = ehextab[v&15]; v>>=4; }
}

static inline void exprintw(unsigned short val,char *buf,int pos)
{
	char *p=buf+pos+3;
	unsigned short v = val;
	while (v) { *p-- = ehextab[v&15]; v>>=4; }
}

static char *e_print_cpuemu_regs(Interp_ENV *env, int is32)
{
	static char buf[512];
#ifdef TRACE_STACK
	void *sp;
	int i, tsp;
#endif
	char *p;

	*buf = 0;
	if (d_emu>3) {
	  long lflags;
	  strcpy(buf,eregbuf);
	  exprintl(env->rax.e,buf,0x05); /* look 'Ma... magic numbers! */
	  exprintl(env->rbx.e,buf,0x12);
	  exprintl(env->rcx.e,buf,0x1f);
	  exprintl(env->rdx.e,buf,0x2c);
	  exprintl(env->rsi.esi,buf,0x3a);
	  exprintl(env->rdi.edi,buf,0x47);
	  exprintl(env->rbp.ebp,buf,0x54);
	  exprintl(env->rsp.esp,buf,0x61);
	  exprintl(env->trans_addr,buf,0x6f);
	  exprintw(env->cs.cs,buf,0x7c);
	  exprintw(env->ds.ds,buf,0x89);
	  exprintw(env->es.es,buf,0x96);
	  exprintw(env->fs.fs,buf,0xa4);
	  exprintw(env->gs.gs,buf,0xb1);
	  exprintw(env->ss.ss,buf,0xbe);
	  lflags = (env->flags&0x3ff73e)|(CARRY&1)|(RES_8&0x80)|(IS_ZF_SET<<6)|
	  	(IS_OF_SET<<11);
	  exprintl(lflags,buf,0xcb);
	  p = buf+0xd4;
#ifdef TRACE_STACK
	  /* dangerous! stack address has to be checked before access */
	  if (VM86F) {
	    tsp = env->rsp.sp.sp;
	    if ((int)tsp & 1)
	      p += sprintf(p," -odd-  stk=");
	    else
	      p += sprintf(p,"\tstk=");
	    for (i=0; (i<10)&&(tsp<0x10000); i++) {
	      sp = (void *)((env->ss.ss<<4)+tsp);
	      p += sprintf(p," %04x",*((unsigned short *)sp));
	      tsp += 2;
	    }
	    for (; i<10; i++) p+=sprintf(p," ****");
	    p+=sprintf(p,"\n");
	  }
	  else {
	    sp = (void *)(LONG_SS + (BIG_SS? env->rsp.esp:env->rsp.sp.sp));
	    p += sprintf(p,"\tstk=");
	    if ((unsigned long)sp > (unsigned long)GetSelectorAddrMax(env->ss.ss)) {
	        p += sprintf(p,"%#lx out of limit\n",(long)sp);
	    	return buf;
	    }
	    if (code32) {
	      sp = (void *)(((unsigned long)sp)&(-2));	/* align, should be -4 */
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
#endif
	}
	return buf;
}

static char *e_print_internal_regs(Interp_ENV *env)
{
	Interp_ENV *v=env;
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
	    p += sprintf(p,"\tS2: %08lx                    %02x         OVRds=%08x\n",
		v->src2.longword,
		v->src2.byte.byte,
		(int)v->seg_regs[6]);
	  }
	  else {
	    p += sprintf(p,"\tS1: %08lx       %04x                    lSS=%08x\n",
		v->src1.longword,
		v->src1.word16.word,
		(int)v->seg_regs[3]);
	    p += sprintf(p,"\tS2: %08lx       %04x                    OVRds=%08x\n",
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

#ifndef DOSEMU
/* This is for Twin - for dosemu version look into dpmi.c */

void print_ldt (void)
{
  DSCR *lp;
  unsigned long base_addr, limit;
  unsigned int type, flags, i;

  lp = LDT;

  for (i = 0; i < MAX_SELECTORS; i++, lp++) {
    /* First 32 bits of descriptor */
    base_addr = (unsigned long)lp->lpSelBase;
    limit = lp->dwSelLimit;
    type = lp->bSelType;
    flags = lp->w86Flags;

    if ((base_addr>0) && (type>0) && (type<10)) {
      if (flags & 0x10)  {
	e_printf("Entry 0x%04x: Base %08lx, Limit %05lx, Type %d (desc %#x)\n",
	       i, base_addr, limit, type, (i<<3)|7);
	e_printf("              ");
	if (flags & 0x1)
	  e_printf("Accessed, ");
	if (!(flags & 0x2))
	  e_printf("Read/Execute only, ");
	if (flags & 0x80)
	  e_printf("Present, ");
	if (flags & 0x1000)
	  e_printf("User, ");
	if (flags & 0x2000)
	  e_printf("X, ");
	if (flags & 0x4000)
	  e_printf("32, ");
	else
	  e_printf("16, ");
	if (flags & 0x8000)
	  e_printf("page limit, ");
	else
	  e_printf("byte limit, ");
	e_printf("\n");
      }
      else {
	e_printf("Entry 0x%04x: Base %08lx, Limit %05lx, Type %d (flg %#x)\n",
	       i, base_addr, limit, type, flags);
      }
    }
    else {	/* invalid entry */
      if (i >= INITLDTSIZE) break;
    }
  }
}
#endif

#endif	/* TRACE_MSGS */
