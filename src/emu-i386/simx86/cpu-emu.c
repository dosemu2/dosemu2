/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include "config.h"

#ifdef X86_EMULATOR
#include <stdlib.h>
#include <string.h>		/* for memset */
#include <setjmp.h>
#include <sys/time.h>
#include "emu.h"
#include "vm86plus.h"
#include "timers.h"
#include "pic.h"
#include "cpu-emu.h"
#include "emu86.h"
#include "codegen-arch.h"
#include "dpmi.h"
#include "dis8086.h"

#define e_set_flags(X,new,mask) \
	((X) = ((X) & ~(mask)) | ((new) & (mask)))

/*
 *  ID VIP VIF AC VM RF 0 NT IOPL OF DF IF TF SF ZF 0 AF 0 PF 1 CF
 *                                 1  1  0  1  1  1 0  1 0  1 0  1
 */
#define SAFE_MASK	(0xDD5)		/* ~(reserved flags plus IF) */
#define notSAFE_MASK	(~SAFE_MASK&0x3fffff)
#define RETURN_MASK	(0xDFF)		/* ~IF */

/* ======================================================================= */

#ifdef PROFILE
hitimer_t AddTime, SearchTime, ExecTime, CleanupTime; // for debug
hitimer_t GenTime, LinkTime;
#endif

hitimer_t TotalTime;
static int iniflag = 0;
static int vm86only = 0;

hitimer_t sigEMUtime = 0;
static hitimer_t lastEMUsig = 0;
static unsigned long sigEMUdelta = 0;
int eTimeCorrect;

/* This needs to be merged someday with 'mode' */
volatile int CEmuStat = 0;

int IsV86Emu = 1;
int IsDpmiEmu = 1;

/* This keeps the delta time in CPU clocks between SIGALRMs and
 * SIGPROFs, i.e. the time spent by the system outside of dosemu,
 * which should be considered in time stretching */
int e_sigpa_count;

int in_vm86_emu = 0;
int in_dpmi_emu = 0;

SynCPU	TheCPU;

int Running = 0;
volatile int InCompiledCode = 0;

unsigned long trans_addr, return_addr;	// PC

#ifdef DEBUG_TREE
FILE *tLog = NULL;
#endif
#ifdef ASM_DUMP
FILE *aLog = NULL;
#endif

/*
 * --------------------------------------------------------------
 * x86 sigcontext:
 * 00	unsigned short gs, __gsh;
 * 01	unsigned short fs, __fsh;
 * 02	unsigned short es, __esh;
 * 03	unsigned short ds, __dsh;
 * 04	unsigned long edi;
 * 05	unsigned long esi;
 * 06	unsigned long ebp;
 * 07	unsigned long esp;
 * 08	unsigned long ebx;
 * 09	unsigned long edx;
 * 10	unsigned long ecx;
 * 11	unsigned long eax;
 * 12	unsigned long trapno;
 * 13	unsigned long err;
 * 14	unsigned long eip;
 * 15	unsigned short cs, __csh;
 * 16	unsigned long eflags;
 * 17	unsigned long esp_at_signal;
 * 18	unsigned short ss, __ssh;
 * 19	struct _fpstate * fpstate;
 * 20	unsigned long oldmask;
 * 21	unsigned long cr2;
 *
 * x86 vm86plus regs:
 * 00	long ebx;
 * 01	long ecx;
 * 02	long edx;
 * 03	long esi;
 * 04	long edi;
 * 05	long ebp;
 * 06	long eax;
 * 07	long __null_ds;
 * 08	long __null_es;
 * 09	long __null_fs;
 * 10	long __null_gs;
 * 11	long orig_eax;
 * 12	long eip;
 * 13	unsigned short cs, __csh;
 * 14	long eflags;
 * 15	long esp;
 * 16	unsigned short ss, __ssh;
 * 17	unsigned short es, __esh;
 * 18	unsigned short ds, __dsh;
 * 19	unsigned short fs, __fsh;
 * 20	unsigned short gs, __gsh;
 * --------------------------------------------------------------
 */
struct sigcontext_struct e_scp; /* initialized to 0 */

unsigned long io_bitmap[IO_BITMAP_SIZE+1];

/* ======================================================================= */

unsigned long eTSSMASK = 0;
#define VFLAGS	(*(unsigned short *)&TheCPU.veflags)

void e_priv_iopl(int pl)
{
    pl &= 3;
    TheCPU.eflags = (TheCPU.eflags & ~EFLAGS_IOPL) |
	(pl << 12);
    e_printf("eIOPL: set IOPL to %d, flags=%#x\n",pl,TheCPU.eflags);
}

void InvalidateSegs(void)
{
    CS_DTR.Attrib=0;
    SS_DTR.Attrib=0;
    DS_DTR.Attrib=0;
    ES_DTR.Attrib=0;
    FS_DTR.Attrib=0;
    GS_DTR.Attrib=0;
}

/* ======================================================================= */


static char ehextab[] = "0123456789abcdef";

/* WARNING - do not convert spaces to tabs! */
#define ERB_LLEN	0x39
#define ERB_LEFTM	5
#define ERB_L1		0x00
#define ERB_L2		(ERB_L1+ERB_LLEN)
#define ERB_L3		(ERB_L1+2*ERB_LLEN)
#define ERB_L4		(ERB_L1+3*ERB_LLEN)
#define ERB_L5		(ERB_L1+4*ERB_LLEN)
#define ERB_L6		(ERB_L5+0x37)
static char eregbuf[] =
/*00*/	"\teax=00000000 ebx=00000000 ecx=00000000 edx=00000000    \n"
/*39*/	"\tesi=00000000 edi=00000000 ebp=00000000 esp=00000000    \n"
/*72*/	"\t vf=00000000  cs=0000      ds=0000      es=0000        \n"
/*ab*/	"\t fs=0000      gs=0000      ss=0000     flg=00000000    \n"
/*e4*/  "\tstk=0000 0000 0000 0000 0000 0000 0000 0000 0000 0000\n";
/*11b*/

static inline void exprintl(unsigned long val,char *bf,unsigned int pos)
{
	char *p=bf+pos+7;
	unsigned long v	= val;
#ifdef __x86_64__
	v &= 0xffffffffffff;
	if (v > 0xffffffff) p += 4;
#endif
	while (v) { *p-- = ehextab[v&15]; v>>=4; }
}

static inline void exprintw(unsigned short val,char *bf,unsigned int pos)
{
	char *p=bf+pos+3;
	unsigned long v = val;
	while (v) { *p-- = ehextab[v&15]; v>>=4; }
}

char *e_print_regs(void)
{
	static char buf[300];
	char *p = buf;
	char *q = eregbuf;

	while (*q) *p++ = *q++; *p=0;
	exprintl(rEAX,buf,(ERB_L1+ERB_LEFTM));
	exprintl(rEBX,buf,(ERB_L1+ERB_LEFTM)+13);
	exprintl(rECX,buf,(ERB_L1+ERB_LEFTM)+26);
	exprintl(rEDX,buf,(ERB_L1+ERB_LEFTM)+39);
	exprintl(rESI,buf,(ERB_L2+ERB_LEFTM));
	exprintl(rEDI,buf,(ERB_L2+ERB_LEFTM)+13);
	exprintl(rEBP,buf,(ERB_L2+ERB_LEFTM)+26);
	exprintl(rESP,buf,(ERB_L2+ERB_LEFTM)+39);
	if (TheCPU.eflags&EFLAGS_VM)
		exprintl(TheCPU.veflags,buf,(ERB_L3+ERB_LEFTM));
	else
		exprintl(get_vFLAGS(TheCPU.eflags),buf,(ERB_L3+ERB_LEFTM));
	exprintw(TheCPU.cs,buf,(ERB_L3+ERB_LEFTM)+13);
	exprintw(TheCPU.ds,buf,(ERB_L3+ERB_LEFTM)+26);
	exprintw(TheCPU.es,buf,(ERB_L3+ERB_LEFTM)+39);
	exprintw(TheCPU.fs,buf,(ERB_L4+ERB_LEFTM));
	exprintw(TheCPU.gs,buf,(ERB_L4+ERB_LEFTM)+13);
	exprintw(TheCPU.ss,buf,(ERB_L4+ERB_LEFTM)+26);
	exprintl(TheCPU.eflags,buf,(ERB_L4+ERB_LEFTM)+39);
	if (debug_level('e')>4) {
		int i;
		unsigned st = LONG_SS+TheCPU.esp;
		if (st < 0x1100000 ||
		    (st > config.dpmi_base &&
		     st <= config.dpmi_base + config.dpmi * 1024)) {
			unsigned short *stk = (unsigned short *)(uintptr_t)st;
			for (i=(ERB_L5+ERB_LEFTM); i<(ERB_L6-2); i+=5) {
			   exprintw(*stk++,buf,i);
			}
		}
	}
	else
		buf[ERB_L5]=0;
	return buf;
}

#if MAX_SELECTORS != 8192
#error MAX_SELECTORS needs to be 8192
#endif

#define GetSegmentBaseAddress(s)	((unsigned long)Segments[(s) >> 3].base_addr)
#define IsSegment32(s)			(Segments[(s) >> 3].is_32)

char *e_print_scp_regs(struct sigcontext_struct *scp, int pmode)
{
	static char buf[300];
	char *p = buf;
	char *q = eregbuf;
	unsigned short *stk;
	int i;

	while (*q) *p++ = *q++; *p=0;
	exprintl(_rax,buf,(ERB_L1+ERB_LEFTM));
	exprintl(_rbx,buf,(ERB_L1+ERB_LEFTM)+13);
	exprintl(_rcx,buf,(ERB_L1+ERB_LEFTM)+26);
	exprintl(_rdx,buf,(ERB_L1+ERB_LEFTM)+39);
	exprintl(_rsi,buf,(ERB_L2+ERB_LEFTM));
	exprintl(_rdi,buf,(ERB_L2+ERB_LEFTM)+13);
	exprintl(_rbp,buf,(ERB_L2+ERB_LEFTM)+26);
	exprintl(_rsp,buf,(ERB_L2+ERB_LEFTM)+39);
	if (pmode & 1)
		exprintl(get_vFLAGS(TheCPU.eflags),buf,(ERB_L3+ERB_LEFTM));
	else
		exprintl(TheCPU.veflags,buf,(ERB_L3+ERB_LEFTM));
	exprintw(_cs,buf,(ERB_L3+ERB_LEFTM)+13);
	exprintw(_ds,buf,(ERB_L3+ERB_LEFTM)+26);
	exprintw(_es,buf,(ERB_L3+ERB_LEFTM)+39);
	exprintw(_fs,buf,(ERB_L4+ERB_LEFTM));
	exprintw(_gs,buf,(ERB_L4+ERB_LEFTM)+13);
	exprintw(_ss,buf,(ERB_L4+ERB_LEFTM)+26);
	exprintl(_eflags,buf,(ERB_L4+ERB_LEFTM)+39);
	if (pmode & 2) {
		buf[(ERB_L4+ERB_LEFTM)+47] = 0;
	}
	else {
	    if (pmode & 1) {
	        if (Segments[_ss>>3].is_32)
		    stk = (unsigned short *)(GetSegmentBaseAddress(_ss)+_esp);
	        else
		    stk = (unsigned short *)(GetSegmentBaseAddress(_ss)+_LWORD(esp));
	    }
	    else
		stk = MK_FP32(_ss,_LWORD(esp));
	    for (i=(ERB_L5+ERB_LEFTM); i<(ERB_L6-2); i+=5) {
		exprintw(*stk++,buf,i);
	    }
	}
	return buf;
}


char *e_emu_disasm(unsigned char *org, int is32)
{
   static char buf[512];
   static char frmtbuf[256];
   int rc;
   int i;
   char *p, *p1;
   unsigned char *code;
   unsigned int org2;
   unsigned int refseg;
   unsigned int ref;

   refseg = TheCPU.cs;
   org2 = org-(unsigned char *)(uintptr_t)LONG_CS;
   code = org;

   rc = dis_8086(code, frmtbuf, is32, &ref, refseg * 16);

   p = buf + sprintf(buf,"%08lx: ",(long)org);
   for (i=0; i<rc && i<8; i++) {
	p += sprintf(p, "%02x", code[i]);
   }
   sprintf(p,"%20s", " ");
   p1 = buf + 28;
   p = p1 + sprintf(p1, "%04x:%04x %s", TheCPU.cs, org2, frmtbuf);

   return buf;
}

#ifdef TRACE_DPMI
char *e_scp_disasm(struct sigcontext_struct *scp, int pmode)
{
   static char insrep = 0;
   static unsigned char buf[1024];
   static unsigned char frmtbuf[256];
   static unsigned long lasta = 0;
   int rc;
   int i;
   unsigned char *p, *pb, *org, *org2, *csp2;
   unsigned int seg;
   unsigned int refseg;
   unsigned int ref;

   *buf = 0;
   seg = scp->cs;
   refseg = seg;
   if (!((_cs) & 0x0004)) {
      csp2 = org = (unsigned char *)scp->eip;
   }
   else {
      csp2 = NULL;
      if (scp->cs <= 0xffff)
         csp2 = (unsigned char *)(GetSegmentBaseAddress(scp->cs));
      org  = (unsigned char *)(csp2 + scp->eip);
   }
   if ((long)org==lasta) { insrep=1; return buf; } /* skip 'rep xxx' steps */
   lasta = (long)org; insrep = 0;

   rc = dis_8086(org, frmtbuf, pmode&&IsSegment32(scp->cs),
   	&ref, (pmode? (int)csp2 : refseg * 16));

   pb = buf;
   org2 = org;
   while ((*org2&0xfc)==0x64) org2++;	/* skip most prefixes */
   if ((debug_level('t')>3)||(InterOps[*org2]&2))
	pb += sprintf(pb,"%s",e_print_scp_regs(scp,pmode));

   p = pb + sprintf(pb,"    %08lx: ",(long)org);
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", *(org+i));
   }
   sprintf(p,"%20s", " ");
   sprintf(pb+28, "%04x:%04lx %s\n", scp->cs, scp->eip, frmtbuf);

   return buf;
}
#endif


char *e_trace_fp(void)
{
	static char buf[512];
	int i, ifpr;
	char *p;
	double *FPRSTT = &TheCPU.fpregs[TheCPU.fpstt];

	ifpr = TheCPU.fpstt&7;
	p = buf;
	for (i=0; i<8; i++) {
	  double *q = &TheCPU.fpregs[ifpr];
	  char buf2[32];
#ifdef FP_DISPHEX
	  sprintf(buf2,"\t%016Lx", *((long long *)q));
#else
	  *buf2=0;
#endif
#ifdef FPU_TAGS
	  switch ((TheCPU.fptag >> (ifpr<<1)) & 3) {
	    case 0: case 1:
#endif
		p += sprintf(p,"\tFp%d\t%16.8f%s\n\t", ifpr, *q, buf2);
#ifdef FPU_TAGS
		break;
	    case 2: p += sprintf(p,"\tFp%d\tNaN/Inf%s\n\t", ifpr, buf2);
	    	break;
	    case 3: p += sprintf(p,"\tFp%d\t****%s\n\t", ifpr, buf2);
	    	break;
	  }
#endif
	  ifpr = (ifpr+1) & 7;
	}
	p += sprintf(p,"\tst=%d(%08lx) sw=%04x cw=%04x tag=%04x\n",
		TheCPU.fpstt, (long)FPRSTT, TheCPU.fpus, TheCPU.fpuc, TheCPU.fptag);
	return buf;
}

void GCPrint(unsigned char *cp, unsigned char *cbase, int len)
{
	int i;
	while (len) {
		dbug_printf(">>> %08tx:",cp-cbase);
		for (i=0; (i<16) && len; i++,len--) dbug_printf(" %02x",*cp++);
		dbug_printf("\n");
	}
}


char *showreg(signed char r)
{
	static const char *s4[] = {
		"ZERO","  GS","  FS","  ES","  DS"," EDI"," ESI"," EBP",
		" ESP"," EBX"," EDX"," ECX"," EAX","TPNO","SCPE"," EIP",
		"  CS","EFLG","ESPS","  SS","FPSP","OLDM"," CR2"," SR1",
		" DR1"," XR1"," SIG","VEFL"," ERR","TIML","TIMH","SMSK",
		"  ni","CR2S","CR_0","CR_1","CR_2","CR_3","CR_4","MODE",
		" XGS","GSc1","GSc2"," XFS","FSc1","FSc2"," XES","ESc1",
		"ESc2"," XDS","DSc1","DSc2"," XCS","CSc1","CSc2"," XSS",
		"SSc1","SSc2","FPRG","FPCS","FPST","FNI0","FNI1","FNI2"
	};
	static char m1[32];
	static int i = 0;
	char *p;
	unsigned int ix = r;
	i = (i+8) & 0x18; p = m1+i;	// for side effects in printf
	if ((ix&0xff)==0xff) { *p=0; return p; }
	ix = (ix>>2)&0x3f;
	*((int *)p) = *((int *)(s4[ix]));
	if ((r&3)==0) p[4]=0; else {
		p[4]='+'; p[5]=(r&3)+'0'; p[6]=0;
	}
	return p;
}

char *showmode(unsigned int m)
{
	static char m0[28] = "ADBIRSGLrnMNXbsd....CoOi";
	static char m1[28];
	int i,j;
	m &= 0xf0ffff; j = 24;
	for (i=0; (i<24)&&(m!=0); i++) { if (m&1) m1[--j]=m0[i]; m>>=1; }
	m1[24]=0;
	return m1+j;
}


/* ======================================================================= */
/*
 * Register movements between the dosemu REGS and the emulator cpu
 * A - Real and VM86 mode
 */

/*
 * Enter emulator in VM86 mode (sys_vm86)
 */
static void Reg2Cpu (int mode)
{
  unsigned long flg;
 /*
  * Enter VM86
  * Copy the dosemu flags (in vm86s) into our veflags, which are the
  * equivalent of the VEFLAGS in /linux/arch/i386/kernel/vm86.c
  */
  eVEFLAGS = vm86s.regs.eflags | RF;

 /* From now on we'll work on the cpuemu eflags (BUT vm86s eflags can be
  * changed asynchronously by signals) */
  TheCPU.eflags = vm86s.regs.eflags & SAFE_MASK;
  /* get the protected mode flags. Note that RF and VM are cleared
   * by pushfd (but not by ints and traps). Equivalent to regs32->eflags
   * in vm86.c */
  flg = getflags();
  TheCPU.eflags |= (flg & notSAFE_MASK); // which VIP do we get here?
  TheCPU.eflags |= (VM | RF);	// RF is cosmetic...

  if (config.cpuemu==2) {
    /* a vm86 call switch has been detected.
       Setup flags for the 1st time. */
    config.cpuemu=4-vm86only;
  }

  if (debug_level('e')>1) e_printf("Reg2Cpu> vm86=%08lx dpm=%08x emu=%08x evf=%08x\n",
	vm86s.regs.eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,TheCPU.veflags);
  TheCPU.eax     = vm86s.regs.eax;	/* 2c -> 18 */
  TheCPU.ebx     = vm86s.regs.ebx;	/* 20 -> 00 */
  TheCPU.ecx     = vm86s.regs.ecx;	/* 28 -> 04 */
  TheCPU.edx     = vm86s.regs.edx;	/* 24 -> 08 */
  TheCPU.esi     = vm86s.regs.esi;	/* 14 -> 0c */
  TheCPU.edi     = vm86s.regs.edi;	/* 10 -> 10 */
  TheCPU.ebp     = vm86s.regs.ebp;	/* 18 -> 14 */
  TheCPU.esp     = vm86s.regs.esp;	/* 1c -> 3c */
  TheCPU.err     = 0;
  TheCPU.eip     = vm86s.regs.eip&0xffff;

  SetSegReal(vm86s.regs.cs,Ofs_CS);
  SetSegReal(vm86s.regs.ss,Ofs_SS);
  SetSegReal(vm86s.regs.ds,Ofs_DS);
  SetSegReal(vm86s.regs.es,Ofs_ES);
  SetSegReal(vm86s.regs.fs,Ofs_FS);
  SetSegReal(vm86s.regs.gs,Ofs_GS);
  trans_addr     = LONG_CS + TheCPU.eip;

  if (debug_level('e')>1) {
	if (debug_level('e')==3) e_printf("Reg2Cpu< vm86=%08lx dpm=%08x emu=%08x evf=%08x\n%s\n",
		vm86s.regs.eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,TheCPU.veflags,
		e_print_regs());
	else e_printf("Reg2Cpu< vm86=%08lx dpm=%08x emu=%08x evf=%08x\n",
		vm86s.regs.eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,TheCPU.veflags);
  }
}

/*
 * Exit emulator in VM86 mode and return to dosemu (return_to_32bit)
 */
static void Cpu2Reg (void)
{
  int mask;
  if (debug_level('e')>1) e_printf("Cpu2Reg> vm86=%08lx dpm=%08x emu=%08x evf=%08x\n",
	vm86s.regs.eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,TheCPU.veflags);
  vm86s.regs.eax = TheCPU.eax;
  vm86s.regs.ebx = TheCPU.ebx;
  vm86s.regs.ecx = TheCPU.ecx;
  vm86s.regs.edx = TheCPU.edx;
  vm86s.regs.esi = TheCPU.esi;
  vm86s.regs.edi = TheCPU.edi;
  vm86s.regs.ebp = TheCPU.ebp;
  vm86s.regs.esp = TheCPU.esp;
  vm86s.regs.ds  = TheCPU.ds;
  vm86s.regs.es  = TheCPU.es;
  vm86s.regs.ss  = TheCPU.ss;
  vm86s.regs.fs  = TheCPU.fs;
  vm86s.regs.gs  = TheCPU.gs;
  vm86s.regs.cs  = TheCPU.cs;
  vm86s.regs.eip = return_addr - LONG_CS;
  /*
   * move (VIF|TSSMASK) flags from VEFLAGS to eflags; resync vm86s eflags
   * from the emulated ones.
   * The cpuemu should not change VIP, the good one is always in vm86s.
   */
  mask = VIF | eTSSMASK;
  vm86s.regs.eflags = (vm86s.regs.eflags & VIP) | 
  			(eVEFLAGS & mask) | (TheCPU.eflags & ~(mask|VIP));

  if (debug_level('e')>1) e_printf("Cpu2Reg< vm86=%08lx dpm=%08x emu=%08x evf=%08x\n",
	vm86s.regs.eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,TheCPU.veflags);
}


/* ======================================================================= */

static void Scp2Cpu (struct sigcontext_struct *scp)
{
#ifdef __x86_64__
  TheCPU.eax = _eax;
  TheCPU.ebx = _ebx;
  TheCPU.ecx = _ecx;
  TheCPU.edx = _edx;
  TheCPU.esi = _esi;
  TheCPU.edi = _edi;
  TheCPU.ebp = _ebp;
  TheCPU.esp = _esp;

  TheCPU.eip = _eip;
  TheCPU.eflags = _eflags;

  TheCPU.cs = _cs;
  TheCPU.fs = _fs;
  TheCPU.gs = _gs;

  TheCPU.ds = _ds;
  TheCPU.es = _es;

  TheCPU.scp_err = _err;
#else
  memcpy(&TheCPU.gs,scp,offsetof(struct sigcontext_struct,esp_at_signal));
#endif
  TheCPU.ss = _ss;
  TheCPU.cr2 = _cr2;
}

/*
 * Return back from fault handling to VM86
 */
static void Scp2CpuR (struct sigcontext_struct *scp)
{
  if (debug_level('e')>1) e_printf("Scp2CpuR> scp=%08lx dpm=%08x fl=%08x vf=%08x\n",
	_eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,eVEFLAGS);
  Scp2Cpu(scp);
  TheCPU.err = 0;

  if (in_dpmi) {	// vm86 during dpmi active
	e_printf("** Scp2Cpu VM in_dpmi\n");
  }
  TheCPU.eflags = (_eflags&(eTSSMASK|0x10ed5)) | 0x20002;
  trans_addr = ((_cs<<4) + _eip);

  if (debug_level('e')>1) e_printf("Scp2CpuR< scp=%08lx dpm=%08x fl=%08x vf=%08x\n",
	_eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,eVEFLAGS);
}

/*
 * Build a sigcontext structure to enter fault handling from VM86 or DPMI
 */
static void Cpu2Scp (struct sigcontext_struct *scp, int trapno)
{
  if (debug_level('e')>1) e_printf("Cpu2Scp> scp=%08lx dpm=%08x fl=%08x vf=%08x\n",
	_eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,eVEFLAGS);

  /* setup stack context from cpu registers */
#ifdef __x86_64__
  _eax = TheCPU.eax;
  _ebx = TheCPU.ebx;
  _ecx = TheCPU.ecx;
  _edx = TheCPU.edx;
  _esi = TheCPU.esi;
  _edi = TheCPU.edi;
  _ebp = TheCPU.ebp;
  _esp = TheCPU.esp;

  _eip = TheCPU.eip;
  _eflags = TheCPU.eflags;

  _cs = TheCPU.cs;
  _fs = TheCPU.fs;
  _gs = TheCPU.gs;

  _ds = TheCPU.ds;
  _es = TheCPU.es;

  _err = TheCPU.scp_err;
#else
  memcpy(scp,&TheCPU.gs,offsetof(struct sigcontext_struct,esp_at_signal));
#endif
  _ss = TheCPU.ss;
  _cr2 = TheCPU.cr2;
  _trapno = trapno;
  /* Error code format:
   * b31-b16 = 0 (undef)
   * b15-b0  = selector, where b2=LDT/GDT, b1=IDT, b0=EXT
   * (b0-b1 are currently unimplemented here)
   */
  if (!TheCPU.err) _err = 0;		//???

  if (in_dpmi) {
    _cs = TheCPU.cs;
    _eip = return_addr - LONG_CS;
    /* push running flags - same as eflags, RF is cosmetic */
    _eflags = (TheCPU.eflags & (eTSSMASK|0xfd5)) | 0x10002;
  }
  else {
    unsigned long mask;
    _cs  = return_addr >> 16;
    _eip = return_addr & 0xffff;
    /* rebuild running flags */
    mask = VIF | eTSSMASK;
    vm86s.regs.eflags = (vm86s.regs.eflags & VIP) | 
  			(eVEFLAGS & mask) | (TheCPU.eflags & ~(mask|VIP));
    _eflags = vm86s.regs.eflags & ~VM;
  }
  if (debug_level('e')>1) e_printf("Cpu2Scp< scp=%08lx vm86=%08lx dpm=%08x fl=%08x vf=%08x\n",
	_eflags,vm86s.regs.eflags,get_vFLAGS(TheCPU.eflags),TheCPU.eflags,eVEFLAGS);
}


/* ======================================================================= */
/*
 * Register movements between the dosemu REGS and the emulator cpu
 * B - DPMI and PM mode
 */

/*
 * Enter emulator in DPMI mode (context_switch)
 */
static int Scp2CpuD (struct sigcontext_struct *scp)
{
  char big; int mode=0, amask, oldfl;

  /* copy registers from current dpmi client to our cpu */
  oldfl = TheCPU.eflags & ~(EFLAGS_VM|EFLAGS_RF); /* to preserve IOPL */
  Scp2Cpu(scp);

  mode |= ADDR16;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_CS,&big,TheCPU.cs);
  if (TheCPU.err) goto erseg;
  if (big) mode=0; else mode |= DATA16;

  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_DS,&big,TheCPU.ds);
  if (TheCPU.err) goto erseg;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_SS,&big,TheCPU.ss);
  if (TheCPU.err) goto erseg;
  TheCPU.StackMask = (big? 0xffffffff : 0x0000ffff);
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_ES,&big,TheCPU.es);
  if (TheCPU.err) goto erseg;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_FS,&big,TheCPU.fs);
  if (TheCPU.err) goto erseg;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_GS,&big,TheCPU.gs);
erseg:
  /* push scp flags, pop eflags - this clears RF,VM */
  amask = (CPL==0? 0:EFLAGS_IOPL_MASK) | (CPL<=IOPL? 0:EFLAGS_IF) |
    (EFLAGS_VM|EFLAGS_RF) | 2;
  TheCPU.eflags = (oldfl & amask) | ((_eflags&(eTSSMASK|0xfd7))&~amask);

  trans_addr = LONG_CS + _eip;
  if (debug_level('e')>1) {
	if (debug_level('e')==3) e_printf("Scp2CpuD%s: %08lx -> %08x\n\tIP=%08x:%08x\n%s\n",
			(TheCPU.err? " ERR":""),
			_eflags, TheCPU.eflags, LONG_CS, _eip,
			e_print_regs());
	else e_printf("Scp2CpuD%s: %08lx -> %08x\n",
			(TheCPU.err? " ERR":""), _eflags, TheCPU.eflags);
  }
  return mode;
}


/* ======================================================================= */


void init_emu_cpu (void)
{
  eTimeCorrect = 0;		// full backtime stretch
#ifdef HOST_ARCH_X86
  if (!CONFIG_CPUSIM)
    eTimeCorrect = 1;		// 1/2 backtime stretch
#endif
  if (config.cpuemu == 3)
    vm86only = 1;
  memset(&TheCPU, 0, sizeof(SynCPU));
  TheCPU.cr[0] = 0x13;	/* valid bits: 0xe005003f */
  TheCPU.dr[4] = 0xffff1ff0;
  TheCPU.dr[5] = 0x400;
  TheCPU.dr[6] = 0xffff1ff0;
  TheCPU.dr[7] = 0x400;
  TheCPU.GDTR.Limit = TheCPU.IDTR.Limit = TheCPU.LDTR.Limit = TheCPU.TR.Limit = 0xffff;
  TheCPU.cs_cache.BoundL = 0x400;
  TheCPU.cs_cache.BoundH = 0x10ffff;
  TheCPU.ss_cache.BoundL = 0x100;
  TheCPU.ss_cache.BoundH = 0x10ffff;
  TheCPU.ds_cache.BoundL = 0;
  TheCPU.ds_cache.BoundH = 0x10ffff;
  TheCPU.es_cache.BoundL = 0;
  TheCPU.es_cache.BoundH = 0x10ffff;
  TheCPU.fs_cache.BoundL = 0;
  TheCPU.fs_cache.BoundH = 0x10ffff;
  TheCPU.gs_cache.BoundL = 0;
  TheCPU.gs_cache.BoundH = 0x10ffff;

  Reg2Cpu(ADDR16|DATA16);
  TheCPU.StackMask = 0x0000ffff;
  init_emu_npu();

  switch (vm86s.cpu_type) {
	case CPU_286:
		eTSSMASK = 0;
		break;
	case CPU_386:
		eTSSMASK = NT_MASK | IOPL_MASK;
		break;
	case CPU_486:
		eTSSMASK = AC_MASK | NT_MASK | IOPL_MASK;
		break;
	default:
		eTSSMASK = ID_MASK | AC_MASK | NT_MASK | IOPL_MASK;
		break;
  }
  e_printf("EMU86: tss mask=%08lx\n", eTSSMASK);
  InitGen();
  Running = 1;
}

/*
 * Under cpuemu, SIGALRM is redirected here. We need a source of
 * asynchronous signals because without it any badly-behaved pgm
 * can stop us forever.
 */
int e_gen_sigalrm(struct sigcontext_struct *scp)
{
	if(config.cpuemu < 2)
	    return 1;

	/* here we come from the kernel with cs==UCODESEL, as
	 * the passed context is that of dosemu, NOT that of the
	 * emulated CPU! */

	e_sigpa_count += sigEMUdelta;
	if (!in_vm86_emu && !in_dpmi_emu) {	/* if into dosemu itself */
	    TheCPU.EMUtime = GETTSC();		/* resync emulator time  */
	    sigEMUtime = TheCPU.EMUtime;	/* and generate signal   */
	}
	else {
	    TheCPU.sigalrm_pending = 0x101;	/* decremented by loops  */
	}
	if (TheCPU.EMUtime >= sigEMUtime) {
		lastEMUsig = TheCPU.EMUtime;
		sigEMUtime += sigEMUdelta;
		/* we can't call sigalrm() because of the way the
		 * context parameter is passed. */
		return 1;	/* -> signal_save */
	}
	/* here we return back to dosemu */
	return 0;
}

static void e_gen_sigprof(struct sigcontext_struct *scp)
{
	e_sigpa_count -= sigEMUdelta;
	TheCPU.sigprof_pending += 1;
}


void enter_cpu_emu(void)
{
	struct itimerval itv;
	unsigned int realdelta = config.update / TIMER_DIVISOR;

	if (config.realcpu < CPU_586) {
	  fprintf(stderr,"Cannot execute CPUEMU without TSC counter\n");
	  leavedos(0);
	}
	config.cpuemu=2;	/* for saving CPU flags */
	GDT = NULL; IDT = NULL;
	/* use the cached LDT used by dpmi (w/o GDT) */
	if (LDT==NULL) {
		LDT = (Descriptor *)ldt_buffer;
		e_printf("LDT allocated at %08lx\n",(long)LDT);
		TheCPU.LDTR.Base = (long)LDT;
		TheCPU.LDTR.Limit = 0xffff;
	}
#ifdef SKIP_EMU_VBIOS
	if ((ISEG(0x10)==INT10_WATCHER_SEG)&&(IOFF(0x10)==INT10_WATCHER_OFF))
		IOFF(0x10)=CPUEMU_WATCHER_OFF;
#endif
#ifdef HOST_ARCH_X86
	TheCPU.unprotect_stub = stub_rep;
	TheCPU.stub_wri_8 = stub_wri_8;
	TheCPU.stub_wri_16 = stub_wri_16;
	TheCPU.stub_wri_32 = stub_wri_32;
	TheCPU.stub_stk_16 = stub_stk_16;
	TheCPU.stub_stk_32 = stub_stk_32;
	TheCPU.stub_movsb = stub_movsb;
	TheCPU.stub_movsw = stub_movsw;
	TheCPU.stub_movsl = stub_movsl;
	TheCPU.stub_stosb = stub_stosb;
	TheCPU.stub_stosw = stub_stosw;
	TheCPU.stub_stosl = stub_stosl;
#endif

	TheCPU.EMUtime = GETTSC();
	sigEMUdelta = realdelta*config.CPUSpeedInMhz;
	sigEMUtime = TheCPU.EMUtime + sigEMUdelta;
	TotalTime = 0;
#ifdef PROFILE
	SearchTime = AddTime = ExecTime = CleanupTime =
	GenTime = LinkTime = 0;
#endif
	e_printf("EMU86: delta alrm=%d speed=%d\n",realdelta,config.CPUSpeedInMhz);
	e_sigpa_count = 0;

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = realdelta;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = realdelta;
	e_printf("TIME: using %d usec for updating PROF timer\n", realdelta);
	setitimer(ITIMER_PROF, &itv, NULL);
	registersig(SIGPROF, e_gen_sigprof);

#ifdef DEBUG_TREE
	tLog = fopen(DEBUG_TREE_FILE,"w");
#endif
#ifdef ASM_DUMP
	aLog = fopen(ASM_DUMP_FILE,"w");
#endif
	dbug_printf("======================= ENTER CPU-EMU ===============\n");
	flush_log();
	iniflag = 1;
}

void leave_cpu_emu(void)
{
	struct itimerval itv;

	if (config.cpuemu > 1) {
		iniflag = 0;
		config.cpuemu=1;
#ifdef SKIP_EMU_VBIOS
		if (IOFF(0x10)==CPUEMU_WATCHER_OFF)
			IOFF(0x10)=INT10_WATCHER_OFF;
#endif
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = 0;
		e_printf("TIME: disabling PROF timer\n");
		setitimer(ITIMER_PROF, &itv, NULL);
		registersig(SIGPROF, NULL);

		EndGen();
#ifdef DEBUG_TREE
		fclose(tLog); tLog = NULL;
#endif
#ifdef ASM_DUMP
		fclose(aLog); aLog = NULL;
#endif
		mprot_end();

		LDT = NULL; GDT = NULL; IDT = NULL;
		dbug_printf("======================= LEAVE CPU-EMU ===============\n");
#ifdef PROFILE
		dbug_printf("Total cpuemu time %16lld us (incl.trace)\n",(long long)TotalTime/config.CPUSpeedInMhz);
		dbug_printf("Total codgen time %16lld us\n",(long long)GenTime/config.CPUSpeedInMhz);
		dbug_printf("Total linker time %16lld us\n",(long long)LinkTime/config.CPUSpeedInMhz);
		dbug_printf("Total exec   time %16lld us (incl.faults)\n",(long long)ExecTime/config.CPUSpeedInMhz);
		dbug_printf("Total insert time %16lld us\n",(long long)AddTime/config.CPUSpeedInMhz);
		dbug_printf("Total search time %16lld us\n",(long long)SearchTime/config.CPUSpeedInMhz);
		dbug_printf("Total clean  time %16lld us\n",(long long)CleanupTime/config.CPUSpeedInMhz);
		dbug_printf("Max tree nodes    %16d\n",MaxNodes);
		dbug_printf("Max node size     %16d\n",MaxNodeSize);
		dbug_printf("Max tree depth    %16d\n",MaxDepth);
		dbug_printf("Nodes parsed      %16d\n",TotalNodesParsed);
		dbug_printf("Find misses       %16d\n",NodesNotFound);
		dbug_printf("Nodes executed    %16d\n",TotalNodesExecd);
		if (TotalNodesExecd) {
		  unsigned long long k;
		  k = ((long long)NodesFound * 100UL) / (long long)TotalNodesExecd;
		  dbug_printf("Find hits         %16d (%lld%%)\n",NodesFound,k);
		  k = ((long long)NodesFastFound * 100UL) / (long long)TotalNodesExecd;
		  dbug_printf("Find last hits    %16d (%lld%%)\n",NodesFastFound,k);
		}
		dbug_printf("Page faults       %16d\n",PageFaults);
		dbug_printf("Signals received  %16d\n",EmuSignals);
		dbug_printf("Tree cleanups     %16d\n",TreeCleanups);
#endif
		/*re*/init_emu_cpu();
	}
	flush_log();
}


/* =======================================================================
 * kernel-level vm86 handling - this has been mostly moved into interp.c
 * from /linux/arch/i386/kernel/vm86.c
 * original code by Linus Torvalds and later enhancements by
 * Lutz Molgedey and Hans Lermen.
 */
static inline unsigned long e_get_vflags(void)
{
	unsigned long flags = REG(eflags) & RETURN_MASK;

	if (eVEFLAGS & VIF_MASK)
		flags |= IF_MASK;
	return flags | (eVEFLAGS & eTSSMASK);
}

static inline int e_revectored(int nr, struct revectored_struct * bitmap)
{
	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (nr)
		:"m" (*bitmap),"r" (nr));
	return nr;
}

static void e_should_clean_tree(int i)
{
#ifdef HOST_ARCH_X86
	if ((i==0x21) && ((HI(ax)==0x4b)||(HI(ax)==0x4c)) &&
	    !CONFIG_CPUSIM) {
	    FLUSH_TREE;
	}
#endif
}

static int e_do_int(int i, unsigned char * ssp, unsigned long sp)
{
	unsigned int *intr_ptr, segoffs;

	if (_CS == BIOSSEG)
		goto cannot_handle;
	if (e_revectored(i, &vm86s.int_revectored))
		goto cannot_handle;
	if (i==0x21 && e_revectored(_AH,&vm86s.int21_revectored))
		goto cannot_handle;
	intr_ptr = MK_FP32(0, i << 2);
	segoffs = *intr_ptr;
	if ((segoffs >> 16) == BIOSSEG)
		goto cannot_handle;
	pushw(ssp, sp, e_get_vflags());
	pushw(ssp, sp, _CS);
	pushw(ssp, sp, _IP);
	_CS = segoffs >> 16;
	_SP -= 6;
	_IP = segoffs & 0xffff;
	REG(eflags) &= ~(TF|RF|AC);
	eVEFLAGS &= ~VIF_MASK;

	/* see config.c: int21 goes here when debug_level('D')==0 ... */
	if (i!=0x16) {
	    e_printf("EMU86: directly calling int %#x ax=%#x at %#x:%#x\n",
		i, _AX, _CS, _IP);
	    e_should_clean_tree(i);
	}
	return -1;

cannot_handle:
	e_printf("EMU86: calling revectored int %#x\n", i);
	e_should_clean_tree(i);
	return (VM86_INTx + (i << 8));
}


static int handle_vm86_trap(int *error_code, int trapno)
{
	if ( (trapno==3) || (trapno==1) )
		return (VM86_TRAP + (trapno << 8));
	e_do_int(trapno, SEG2LINEAR(_SS), _SP);
	return -1;
}


static int handle_vm86_fault(int *error_code)
{
	unsigned char *csp, *ssp, op;
	unsigned long ip, sp;

	csp = SEG2LINEAR(_CS);
	ssp = SEG2LINEAR(_SS);
	sp = _SP;
	ip = _IP;
	op = popb(csp, ip);
	if (debug_level('e')>1) e_printf("EMU86: vm86 fault %#x at %#x:%#x\n",
		op, _CS, _IP);

	/* int xx */
	if (op==0xcd) {
	        int intno=popb(csp, ip);
		_IP += 2;
		if (vm86s.vm86plus.vm86dbg_active) {
			if ( (1 << (intno &7)) & vm86s.vm86plus.vm86dbg_intxxtab[intno >> 3] ) {
				return (VM86_INTx + (intno << 8));
			}
		}
		return e_do_int(intno, ssp, sp);
	}
	else
		return VM86_UNKNOWN;
}

/*
 * =======================================================================
 */
static char *retdescs[] =
{
	"VM86_SIGNAL","VM86_UNKNOWN","VM86_INTx","VM86_STI",
	"VM86_PICRET","???","VM86_TRAP","???"
};

int e_vm86(void)
{
  hitimer_t tt0;
  int xval,retval,mode;
#ifdef SKIP_VM86_TRACE
  int demusav;
#endif
  int errcode;

#ifdef __i386__
  /* skip emulation of video BIOS, as it is too much timing-dependent */
  if ((!IsV86Emu) || (config.cpuemu<2)
#ifdef SKIP_EMU_VBIOS
   || ((REG(cs)&0xf000)==config.vbios_seg)
#endif
   ) {
	s_munprotect(0);
	InvalidateSegs();
	return true_vm86(&vm86s);
  }
#endif
  if (iniflag==0) enter_cpu_emu();

  tt0 = GETTSC();
  e_sigpa_count = 0;
  mode = ADDR16|DATA16; TheCPU.StackMask = 0x0000ffff;
  TheCPU.EMUtime = GETTSC();
#ifdef SKIP_VM86_TRACE
  demusav=debug_level('e'); if (debug_level('e')) set_debug_level('e', 1);
#endif
//  if (lastEMUsig && (debug_level('e')>1))
//    e_printf("EMU86: last sig at %lld, curr=%lld, next=%lld\n",lastEMUsig>>16,
//    	TheCPU.EMUtime>>16,sigEMUtime>>16);

 /* This emulates VM86_ENTER */
  /* ------ OUTER LOOP: exit for code >=0 and return to dosemu code */
  do {
    Reg2Cpu(mode);
    if (CONFIG_CPUSIM) {
      RFL.valid = V_INVALID;
    }
    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
    do {
      /* enter VM86 mode */
      in_vm86_emu = 1;
      e_printf("INTERP: enter=%08lx\n",trans_addr);
      return_addr = (long)Interp86((char *)trans_addr, mode);
      e_printf("INTERP: exit=%08lx err=%d\n",return_addr,TheCPU.err-1);
      xval = TheCPU.err;
      in_vm86_emu = 0;
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("EMU86: error %d\n", -xval);
        in_vm86=0;
        leavedos(1);
      }
      trans_addr = return_addr;
    }
    while (xval==0);
    /* ---- INNER LOOP -- exit for exception ---------------------- */
    if (CONFIG_CPUSIM)
      FlagSync_All();

    Cpu2Reg();
    if (debug_level('e')>1) e_printf("---------------------\n\t   EMU86: EXCP %#x\n", xval-1);

    retval = -1;
    E_TIME_STRETCH;

    if (xval==EXCP_SIGNAL) {	/* coming here for async interruptions */
	if (CEmuStat & (CeS_SIGPEND|CeS_SIGACT))
		{ CEmuStat &= ~(CeS_SIGPEND|CeS_SIGACT); retval=VM86_SIGNAL; }
    }
    else if (xval==EXCP_PICSIGNAL) {
        retval = VM86_PICRETURN;
    }
    else if (xval==EXCP_STISIGNAL) {
        retval = VM86_STI;
    }
    else if (xval==EXCP_GOBACK) {
        retval = 0;
    }
    else {
	switch (xval) {
	    case EXCP0D_GPF: {	/* to kernel vm86 */
		retval=handle_vm86_fault(&errcode);	/* kernel level */
#ifdef SKIP_EMU_VBIOS
		/* are we into the VBIOS? If so, exit and reenter e_vm86 */
		if ((REG(cs)&0xf000)==config.vbios_seg) {
		    if (retval<0) retval=0;  /* force exit even if handled */
		}
#endif
	      }
	      break;
	    case EXCP01_SSTP:
	    case EXCP03_INT3: {	/* to kernel vm86 */
		retval=handle_vm86_trap(&errcode,xval-1); /* kernel level */
	      }
	      break;
	    default: {
		struct sigcontext_struct scp;
		Cpu2Scp (&scp, xval-1);
		/* CALLBACK */
		TotalTime += (GETTSC() - tt0);
		dosemu_fault1(xval-1, &scp);
		tt0 = GETTSC();
		Scp2CpuR (&scp);
		in_vm86 = 1;
		retval = -1;	/* reenter vm86 emu */
	    }
	}
    }
  }
  while (retval < 0);
  /* ------ OUTER LOOP -- exit to user level ---------------------- */

  LastXNode = NULL;
  e_printf("EMU86: retval=%s\n", retdescs[retval&7]);

#ifdef SKIP_VM86_TRACE
  set_debug_level('e',demusav);
#endif
  TotalTime += (GETTSC() - tt0);
  /* this time should become >0 only when dosemu was descheduled
   * during a vm86/dpmi call. It will be necessary to subtract this
   * value from ZeroBase to account for the elapsed time, maybe. */
  e_printf("Sys timers d=%d\n",e_sigpa_count);
  return retval;
}

/* ======================================================================= */


int e_dpmi(struct sigcontext_struct *scp)
{
  hitimer_t tt0;
  int xval,retval,mode;
  Descriptor *dt;

  if (iniflag==0) enter_cpu_emu();

  tt0 = GETTSC();
  e_sigpa_count = 0;
  TheCPU.EMUtime = GETTSC();
  /* make clear we are in PM now */
  TheCPU.cr[0] |= 1;

  if (debug_level('e')>2) {
	long swa = (long)(DTgetSelBase(_cs)+_eip);
	D_printf("EMU86: DPMI enter at %08lx\n",swa);
  }
//  if (lastEMUsig)
//    e_printf("DPM86: last sig at %lld, curr=%lld, next=%lld\n",lastEMUsig>>16,
//    	TheCPU.EMUtime>>16,sigEMUtime>>16);

  /* ------ OUTER LOOP: exit for code >=0 and return to dosemu code */
  do {
    TheCPU.err = 0;
    mode = Scp2CpuD (scp);
    if (CONFIG_CPUSIM)
      RFL.valid = V_INVALID;
    if (TheCPU.err) {
        error("DPM86: segment error %d\n", TheCPU.err);
        leavedos(0);
    }
    dt = (TheCPU.cs&4? LDT:GDT);

    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
    do {
      /* switch to DPMI process */
      in_dpmi_emu = 1;
      e_printf("INTERP: enter=%08lx mode=%04x\n",trans_addr,mode);
      return_addr = (long)Interp86((char *)trans_addr, mode);
      e_printf("INTERP: exit=%08lx err=%d\n",return_addr,TheCPU.err-1);
      xval = TheCPU.err;
      in_dpmi_emu = 0;
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("DPM86: error %d\n", -xval);
        leavedos(0);
      }
      trans_addr = return_addr;
    }
    while (xval==0);
    /* ---- INNER LOOP -- exit for exception ---------------------- */
    if (CONFIG_CPUSIM)
      FlagSync_All();

    if (debug_level('e')>1) e_printf("DPM86: EXCP %#x eflags=%08lx\n",
	xval-1, REG(eflags));

    TheCPU.eflags &= ~TF;	/* is it right? */
    Cpu2Scp (scp, xval-1);

    retval = -1;
    E_TIME_STRETCH;

    if ((xval==EXCP_SIGNAL) || (xval==EXCP_PICSIGNAL) || (xval==EXCP_STISIGNAL)) {
	if (debug_level('e')>2) e_printf("DPMI sigpending = %d\n",signal_pending);
	if (signal_pending) {
	    retval=0;
	}
    }
    else if (xval==EXCP_GOBACK) {
        retval = 0;
    }
    else {
	int emu_dpmi_retcode;
	TotalTime += (GETTSC() - tt0);
	emu_dpmi_retcode = dpmi_fault(scp);
	tt0 = GETTSC();
	if (emu_dpmi_retcode != 0) {
	    retval=emu_dpmi_retcode; emu_dpmi_retcode = 0;
	    if (retval == -1)
		retval = 0;
	}
    }
  }
  while (retval < 0);
  /* ------ OUTER LOOP -- exit to user level ---------------------- */

  LastXNode = NULL;
  e_printf("DPM86: retval=%#x\n", retval);

  TotalTime += (GETTSC() - tt0);
  e_printf("Sys timers d=%d\n",e_sigpa_count);
  return retval;
}


/* ======================================================================= */
/* file: src/cwsdpmi/exphdlr.c */

void e_dpmi_b0x(int op,struct sigcontext_struct *scp)
{
  switch (op) {
    case 0: {	/* set */
	int enabled = TheCPU.dr[7];
	int i;
	for (i=0; i<4; i++) {
	  if ((~enabled >> (i*2)) & 3) {
	    unsigned mask;
	    TheCPU.dr[i] = (_LWORD(ebx) << 16) | _LWORD(ecx);
	    TheCPU.dr[7] |= (3 << (i*2));
	    mask = _HI(dx) & 3; if (mask==2) mask++;
	    mask |= ((_LO(dx)-1) << 2) & 0x0c;
	    TheCPU.dr[7] |= mask << (i*4 + 16);
	    _LWORD(ebx) = i;
	    _eflags &= ~CF;
	    break;
	  }
	}
      }
      break;
    case 1: {	/* clear */
        int i = _LWORD(ebx) & 3;
        TheCPU.dr[6] &= ~(1 << i);
        TheCPU.dr[7] &= ~(3 << (i*2));
        TheCPU.dr[7] &= ~(15 << (i*4+16));
	_eflags &= ~CF;
	break;
      }
    case 2: {	/* get */
        int i = _LWORD(ebx) & 3;
        _LWORD(eax) = (TheCPU.dr[6] >> i);
	_eflags &= ~CF;
	break;
      }
    case 3: {	/* reset */
        int i = _LWORD(ebx) & 3;
        TheCPU.dr[6] &= ~(1 << i);
	_eflags &= ~CF;
	break;
      }
    default:
	_LWORD(eax) = 0x8016;
	_eflags |= CF;
	break;
  }
  if (TheCPU.dr[7] & 0xff) CEmuStat|=CeS_DRTRAP; else CEmuStat&=~CeS_DRTRAP;
/*
  e_printf("DR0=%08lx DR1=%08lx DR2=%08lx DR3=%08lx\n",DRs[0],DRs[1],DRs[2],DRs[3]);
  e_printf("DR6=%08lx DR7=%08lx\n",DRs[6],DRs[7]);
*/
}


int e_debug_check(unsigned char *PC)
{
    register unsigned long d7 = TheCPU.dr[7];

    if (d7&0x03) {
	if (d7&0x30000) return 0;	/* only execute(00) bkp */
	if ((long)PC==TheCPU.dr[0]) {
	    e_printf("DBRK: DR0 hit at %p\n",PC);
	    TheCPU.dr[6] |= 1;
	    return 1;
	}
    }
    if (d7&0x0c) {
	if (d7&0x300000) return 0;
	if ((long)PC==TheCPU.dr[1]) {
	    e_printf("DBRK: DR1 hit at %p\n",PC);
	    TheCPU.dr[6] |= 2;
	    return 1;
	}
    }
    if (d7&0x30) {
	if (d7&0x3000000) return 0;
	if ((long)PC==TheCPU.dr[2]) {
	    e_printf("DBRK: DR2 hit at %p\n",PC);
	    TheCPU.dr[6] |= 4;
	    return 1;
	}
    }
    if (d7&0xc0) {
	if (d7&0x30000000) return 0;
	if ((long)PC==TheCPU.dr[3]) {
	    e_printf("DBRK: DR3 hit at %p\n",PC);
	    TheCPU.dr[6] |= 8;
	    return 1;
	}
    }
    return 0;
}


/* ======================================================================= */

#endif	/* X86_EMULATOR */
