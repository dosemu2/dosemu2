/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2000 Alberto Vignani, FIAT Research Center
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
#include "emu.h"
#include "timers.h"
#include "pic.h"
#include "cpu-emu.h"
#include "emu86.h"
#include "codegen-x86.h"
#include "dpmi.h"

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

hitimer_t TotalTime, AddTime, SearchTime, ExecTime, CleanupTime; // for debug
static int iniflag = 0;

hitimer_t sigEMUtime = 0;
static hitimer_t lastEMUsig = 0;
static unsigned long sigEMUdelta = 0;
static int last_emuretrace;

/* This needs to be merged someday with 'mode' */
int CEmuStat = 0;

int JumpOpt = 3;	/* b0=back b1=fwd */
unsigned long JumpOptLim = 0x1000;

/* Another signal pending flag... this one is used by the compiled
 * back jumps to avoid looping forever */
volatile sig_atomic_t e_signal_pending = 0;

/* This keeps the delta time in CPU clocks between SIGALRMs and
 * SIGPROFs, i.e. the time spent by the system outside of dosemu,
 * which should be considered in time stretching */
int e_sigpa_count;

int emu_dpmi_retcode = -1;
int in_dpmi_emu = 0;

SynCPU	TheCPU;

int Running = 0;
int InCompiledCode = 0;

unsigned long trans_addr, return_addr;	// PC

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
struct sigcontext_struct e_scp = {
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,
	0,0,0,0,0,0,0,0,0
};

extern void dosemu_fault1(int signal, struct sigcontext_struct *scp);
extern void e_sigalrm(struct sigcontext_struct *scp);

unsigned long io_bitmap[IO_BITMAP_SIZE+1];

/* ======================================================================= */

unsigned long eTSSMASK = 0;
#define VFLAGS	(*(unsigned short *)&TheCPU.veflags)

#undef pushb
#undef pushw
#undef pushl
#define pushb(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %2,0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define pushw(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %h2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define pushl(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %h2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %h2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#undef popb
#undef popw
#undef popl
#define popb(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popw(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb 0(%1,%0),%h2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popl(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb 0(%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb 0(%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base)); \
__res; })


void e_priv_iopl(int pl)
{
    pl &= 3;
    TheCPU.eflags = (TheCPU.eflags & ~EFLAGS_IOPL) |
	(pl << 12);
    e_printf("eIOPL: set IOPL to %d, flags=%#lx\n",pl,TheCPU.eflags);
}


/* ======================================================================= */


static char ehextab[] = "0123456789abcdef";

/* WARNING - do not convert spaces to tabs! */
#define ERB_LLEN	0x35
#define ERB_LEFTM	5
#define ERB_L1		0x00
#define ERB_L2		(ERB_L1+ERB_LLEN)
#define ERB_L3		(ERB_L1+2*ERB_LLEN)
#define ERB_L4		(ERB_L1+3*ERB_LLEN)
#define ERB_L5		(ERB_L1+4*ERB_LLEN)
#define ERB_L6		(ERB_L5+0x37)
static char eregbuf[] =
/*00*/	"\teax=00000000 ebx=00000000 ecx=00000000 edx=00000000\n"
/*35*/	"\tesi=00000000 edi=00000000 ebp=00000000 esp=00000000\n"
/*6a*/	"\t vf=00000000  cs=0000      ds=0000      es=0000    \n"
/*9f*/	"\t fs=0000      gs=0000      ss=0000     flg=00000000\n"
/*d4*/  "\tstk=0000 0000 0000 0000 0000 0000 0000 0000 0000 0000\n";
/*10b*/

static inline void exprintl(unsigned long val,char *bf,unsigned int pos)
{
	char *p=bf+pos+7;
	unsigned long v	= val;
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
		exprintl(dpmi_eflags,buf,(ERB_L3+ERB_LEFTM));
	exprintw(TheCPU.cs,buf,(ERB_L3+ERB_LEFTM)+13);
	exprintw(TheCPU.ds,buf,(ERB_L3+ERB_LEFTM)+26);
	exprintw(TheCPU.es,buf,(ERB_L3+ERB_LEFTM)+39);
	exprintw(TheCPU.fs,buf,(ERB_L4+ERB_LEFTM));
	exprintw(TheCPU.gs,buf,(ERB_L4+ERB_LEFTM)+13);
	exprintw(TheCPU.ss,buf,(ERB_L4+ERB_LEFTM)+26);
	exprintl(TheCPU.eflags,buf,(ERB_L4+ERB_LEFTM)+39);
	if ((d.emu>6) && (TheCPU.mode & DSPSTK)) {
		int i;
		unsigned short *stk = (unsigned short *)(LONG_SS+TheCPU.esp);
		for (i=(ERB_L5+ERB_LEFTM); i<(ERB_L6-2); i+=5) {
		   exprintw(*stk++,buf,i);
		}
	}
	else
		buf[ERB_L5]=0;
	return buf;
}

#define GetSegmentBaseAddress(s)	(((s) >= (MAX_SELECTORS << 3))? 0 :\
					Segments[(s) >> 3].base_addr)
#define IsSegment32(s)			(((s) >= (MAX_SELECTORS << 3))? 0 :\
					Segments[(s) >> 3].is_32)

char *e_print_scp_regs(struct sigcontext_struct *scp, int pmode)
{
	static char buf[300];
	char *p = buf;
	char *q = eregbuf;
	unsigned short *stk;
	int i;

	while (*q) *p++ = *q++; *p=0;
	exprintl(_eax,buf,(ERB_L1+ERB_LEFTM));
	exprintl(_ebx,buf,(ERB_L1+ERB_LEFTM)+13);
	exprintl(_ecx,buf,(ERB_L1+ERB_LEFTM)+26);
	exprintl(_edx,buf,(ERB_L1+ERB_LEFTM)+39);
	exprintl(_esi,buf,(ERB_L2+ERB_LEFTM));
	exprintl(_edi,buf,(ERB_L2+ERB_LEFTM)+13);
	exprintl(_ebp,buf,(ERB_L2+ERB_LEFTM)+26);
	exprintl(_esp,buf,(ERB_L2+ERB_LEFTM)+39);
	if (pmode & 1)
		exprintl(dpmi_eflags,buf,(ERB_L3+ERB_LEFTM));
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
		stk = (unsigned short *)((_ss<<4)+_LWORD(esp));
	    for (i=(ERB_L5+ERB_LEFTM); i<(ERB_L6-2); i+=5) {
		exprintw(*stk++,buf,i);
	    }
	}
	return buf;
}


extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *,
                     unsigned int, int);

char *e_emu_disasm(unsigned char *org, int is32)
{
   static char buf[512];
   static char frmtbuf[256];
   int rc;
   int i;
   char *p, *p1;
   unsigned char *code, *org2;
   unsigned int refseg;
   unsigned int ref;

   refseg = TheCPU.cs;
   org2 = org-LONG_CS;
   code = org;

   rc = dis_8086((unsigned long)org, code, frmtbuf, is32, &refseg, &ref, 0, 1);

   p = buf + sprintf(buf,"%08lx: ",(long)org);
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", code[i]);
   }
   sprintf(p,"%20s", " ");
   p1 = buf + 28;
   p = p1 + sprintf(p1, "%04x:%04x %s", TheCPU.cs, (int)org2, frmtbuf);

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
      csp2 = (unsigned char *)(GetSegmentBaseAddress(scp->cs));
      org  = (unsigned char *)(csp2 + scp->eip);
   }
   if ((long)org==lasta) { insrep=1; return buf; } /* skip 'rep xxx' steps */
   lasta = (long)org; insrep = 0;

   rc = dis_8086((unsigned long)org, org, frmtbuf, (pmode&&IsSegment32(scp->cs)? 3:0),
   	&refseg, &ref, (pmode? (int)csp2 : 0), 1);

   pb = buf;
   org2 = org;
   while ((*org2&0xfc)==0x64) org2++;	/* skip most prefixes */
   if ((d.dpmit>3)||(InterOps[*org2]&2))
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

#undef FP_DISPHEX

char *e_trace_fp(void)
{
	static char buf[512];
	int i, ifpr;
	char *p;
	extern double *FPRSTT;

	ifpr = TheCPU.fpstt&7;
	p = buf;
	for (i=0; i<8; i++) {
#ifdef FP_DISPHEX
	  { unsigned long long *q = (unsigned long long *)&(TheCPU.fpregs[ifpr]);
	    p += sprintf(p,"\tFP%d\t%016Lx\n\t", ifpr, *q);
	  }
#else
#ifdef FPU_TAGS
	  switch ((TheCPU.fptag >> (ifpr<<1)) & 3) {
	    case 0: case 1:
#endif
		p += sprintf(p,"\tFP%d\t%16.8f\n\t", ifpr, TheCPU.fpregs[ifpr]);
#ifdef FPU_TAGS
		break;
	    case 2: p += sprintf(p,"\tFP%d\tNaN/Inf\n\t", ifpr);
	    	break;
	    case 3: p += sprintf(p,"\tFP%d\t****\n\t", ifpr);
	    	break;
	  }
#endif
#endif
	  ifpr = (ifpr+1) & 7;
	}
	p += sprintf(p,"\tst=%d(%08lx) sw=%04x cw=%04x tag=%04x\n",
		TheCPU.fpstt, (long)FPRSTT, TheCPU.fpus, TheCPU.fpuc, TheCPU.fptag);
	return buf;
}


/* ======================================================================= */
/*
 * Register movements between the dosemu REGS and the emulator cpu
 * A - Real and VM86 mode
 */
static void Reg2Cpu (struct vm86plus_struct *info, int mode)
{
  char big;	// dummy
  unsigned long flg;
 /*
  * Copy the dosemu flags (in vm86s) into our veflags, which are the
  * equivalent of the VEFLAGS in /linux/arch/i386/kernel/vm86.c
  * We'll move back our flags into vm86s when exiting this function.
  */
  TheCPU.veflags = info->regs.eflags;
  info->regs.eflags &= SAFE_MASK;
  /* get the protected mode flags. Note that RF and VM are cleared
   * by pushfd (but not by ints and traps) */
  __asm__ __volatile__ ("
	pushfl
	popl	%0"
	: "=m"(flg) : : "memory");
  info->regs.eflags |= (flg & notSAFE_MASK);
  info->regs.eflags |= EFLAGS_VM;

  if (config.cpuemu>2) {
    /* a vm86 call switch has been detected.
       Setup flags for the 1st time. */
    config.cpuemu=2;
  }

  if (d.emu>1) e_printf("Reg2Cpu> vm86=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	info->regs.eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
  TheCPU.eax     = info->regs.eax;	/* 2c -> 18 */
  TheCPU.ebx     = info->regs.ebx;	/* 20 -> 00 */
  TheCPU.ecx     = info->regs.ecx;	/* 28 -> 04 */
  TheCPU.edx     = info->regs.edx;	/* 24 -> 08 */
  TheCPU.esi     = info->regs.esi;	/* 14 -> 0c */
  TheCPU.edi     = info->regs.edi;	/* 10 -> 10 */
  TheCPU.ebp     = info->regs.ebp;	/* 18 -> 14 */
  TheCPU.esp     = info->regs.esp;	/* 1c -> 3c */
  TheCPU.cs      = info->regs.cs;	/* 3c -> 34 */
  TheCPU.ds      = info->regs.ds;	/* 0c -> 48 */
  TheCPU.es      = info->regs.es;	/* 08 -> 44 */
  TheCPU.ss      = info->regs.ss;	/* 48 -> 40 */
  TheCPU.fs      = info->regs.fs;	/* 04 -> 4c */
  TheCPU.gs      = info->regs.gs;	/* 00 -> 50 */
  TheCPU.err     = 0;
  TheCPU.eip     = info->regs.eip&0xffff;
  TheCPU.eflags  = info->regs.eflags;	/* 40 -> 38 */

  (void)SET_SEGREG(CS_DTR,big,Ofs_CS,TheCPU.cs);
  (void)SET_SEGREG(DS_DTR,big,Ofs_DS,TheCPU.ds);
  (void)SET_SEGREG(ES_DTR,big,Ofs_ES,TheCPU.es);
  (void)SET_SEGREG(SS_DTR,big,Ofs_SS,TheCPU.ss);
  (void)SET_SEGREG(FS_DTR,big,Ofs_FS,TheCPU.fs);
  (void)SET_SEGREG(GS_DTR,big,Ofs_GS,TheCPU.gs);
  trans_addr     = LONG_CS + TheCPU.eip;

  if (d.emu>1) {
	if (d.emu==3) e_printf("Reg2Cpu< vm86=%08lx dpm=%08x emu=%08lx evf=%08lx\n%s\n",
		info->regs.eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags,
		e_print_regs());
	else e_printf("Reg2Cpu< vm86=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
		info->regs.eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
  }
}


static void Cpu2Reg (struct vm86plus_struct *info)
{
  if (d.emu>1) e_printf("Cpu2Reg> vm86=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	info->regs.eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
  info->regs.eax = TheCPU.eax;
  info->regs.ebx = TheCPU.ebx;
  info->regs.ecx = TheCPU.ecx;
  info->regs.edx = TheCPU.edx;
  info->regs.esi = TheCPU.esi;
  info->regs.edi = TheCPU.edi;
  info->regs.ebp = TheCPU.ebp;
  info->regs.esp = TheCPU.esp;
  info->regs.ds  = TheCPU.ds;
  info->regs.es  = TheCPU.es;
  info->regs.ss  = TheCPU.ss;
  info->regs.fs  = TheCPU.fs;
  info->regs.gs  = TheCPU.gs;
  info->regs.cs  = TheCPU.cs;
  info->regs.eip = return_addr - LONG_CS;
  /*
   * The emulator only processes the low 16 bits of eflags. OF,PF and
   * AF are not passed back, as dosemu doesn't care about them. RF is
   * forced only for compatibility with the kernel syscall; it is not used.
   * VIP and VIF are reflected to the emu flags but belong to the eflags.
   */
  info->regs.eflags = (info->regs.eflags & (VIP|VIF)) |
  		      (TheCPU.eflags & (eTSSMASK|0x20ed5)) | 0x10002;

  if (d.emu>1) e_printf("Cpu2Reg< vm86=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	info->regs.eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
}


static void Scp2CpuR (struct sigcontext_struct *scp)
{
  if (d.emu>1) e_printf("Scp2CpuR> scp=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	scp->eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
  __memcpy(&TheCPU.FIELD0,scp,sizeof(struct sigcontext_struct));
  TheCPU.err = 0;

  if (in_dpmi) {	// vm86 during dpmi active
	e_printf("** Scp2Cpu VM in_dpmi\n");
  }
  TheCPU.eflags = (scp->eflags&(eTSSMASK|0x10ed5)) | 0x20002;
  trans_addr = ((scp->cs<<4) + scp->eip);

  if (d.emu>1) e_printf("Scp2CpuR< scp=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	scp->eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
}


static void Cpu2Scp (struct sigcontext_struct *scp,
	int trapno)
{
  if (d.emu>1) e_printf("Cpu2Scp> scp=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	scp->eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
  __memcpy(scp,&TheCPU.FIELD0,sizeof(struct sigcontext_struct));
  scp->trapno = trapno;
  /* Error code format:
   * b31-b16 = 0 (undef)
   * b15-b0  = selector, where b2=LDT/GDT, b1=IDT, b0=EXT
   * (b0-b1 are currently unimplemented here)
   */
  if (!TheCPU.err) scp->err = 0;		//???
  if (in_dpmi) {
    scp->cs = TheCPU.cs;
    scp->eip = return_addr - LONG_CS;
    scp->eflags = (dpmi_eflags & (VIP|VIF|IF)) |
  		      (TheCPU.eflags & (eTSSMASK|0xed5)) | 0x10002;
  }
  else {
    scp->cs  = return_addr >> 16;
    scp->eip = return_addr & 0xffff;
    scp->eflags = (vm86s.regs.eflags & (VIP|VIF)) |
  		      (TheCPU.eflags & (eTSSMASK|0xed5)) | 0x20002;
  }
  if (d.emu>1) e_printf("Cpu2Scp< scp=%08lx vm86=%08lx dpm=%08x emu=%08lx evf=%08lx\n",
	scp->eflags,vm86s.regs.eflags,dpmi_eflags,TheCPU.eflags,TheCPU.veflags);
}


/* ======================================================================= */

/*
 * Register movements between the dosemu REGS and the emulator cpu
 * B - DPMI and PM mode
 */
static int Scp2CpuD (struct sigcontext_struct *scp)
{
  char big; int mode=0;
  __memcpy(&TheCPU.FIELD0,scp,sizeof(struct sigcontext_struct));
  TheCPU.veflags = scp->eflags&~EFLAGS_VM;

  mode |= ADDR16;	/* just a default */
  TheCPU.err = SET_SEGREG(CS_DTR,big,Ofs_CS,TheCPU.cs);
  if (big) mode=0; else mode |= DATA16;
  if (!TheCPU.err) {
    TheCPU.err = SET_SEGREG(DS_DTR,big,Ofs_DS,TheCPU.ds);
    if (!TheCPU.err) {
      SET_SEGREG(SS_DTR,big,Ofs_SS,TheCPU.ss);
      TheCPU.StackMask = (big? 0xffffffff : 0x0000ffff);
      if (!TheCPU.err) {
	SET_SEGREG(ES_DTR,big,Ofs_ES,TheCPU.es);
	SET_SEGREG(FS_DTR,big,Ofs_FS,TheCPU.fs);
	SET_SEGREG(GS_DTR,big,Ofs_GS,TheCPU.gs);
      }
    }
  }
  TheCPU.eflags = (scp->eflags&(eTSSMASK|0x10ed5)) | 2;
  trans_addr = LONG_CS + scp->eip;
  if (d.emu>1) {
	if (d.emu==3) e_printf("Scp2CpuD: %08lx -> %08lx\n\tIP=%08lx:%08lx\n%s\n",
			scp->eflags, TheCPU.eflags, LONG_CS, scp->eip,
			e_print_regs());
	else e_printf("Scp2CpuD: %08lx -> %08lx\n",scp->eflags, TheCPU.eflags);
  }
  return mode;
}


/* ======================================================================= */


void init_emu_cpu (void)
{
  extern void init_emu_npu();

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

  Reg2Cpu(&vm86s,ADDR16|DATA16);
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
static void e_gen_sigalrm(int sig, struct sigcontext_struct context)
{
	/* here we come from the kernel with cs==UCODESEL, as
	 * the passed context is that of dosemu, NOT that of the
	 * emulated CPU! */

	e_signal_pending |= 1;
	e_sigpa_count += sigEMUdelta;
	if (!in_vm86 && !in_dpmi) {
	    TheCPU.EMUtime = GETTSC();
	}
	if (TheCPU.EMUtime >= sigEMUtime) {
		lastEMUsig = TheCPU.EMUtime;
		sigEMUtime += sigEMUdelta;
		/* we can't call sigalrm() because of the way the
		 * context parameter is passed. */
		e_sigalrm(&context);	/* -> signal_save */
	}
	/* here we return back to dosemu */
}

static void e_gen_sigprof(int sig, struct sigcontext_struct context)
{
	e_sigpa_count -= sigEMUdelta;
}


Descriptor *alloc_LDT = NULL;

void enter_cpu_emu(void)
{
	struct itimerval itv;
	struct sigaction sa;

	if (config.rdtsc==0) {
	  fprintf(stderr,"Cannot execute CPUEMU without TSC counter\n");
	  leavedos(0);
	}
	config.cpuemu=3;	/* for saving CPU flags */
	emu_dpmi_retcode = -1;
	GDT = NULL; IDT = NULL;
	/* allocate the LDT used by dpmi (w/o GDT) */
	if (LDT==NULL) {
		alloc_LDT = LDT = (Descriptor *)calloc(LGDT_ENTRIES, sizeof(Descriptor));
		e_printf("LDT allocated at %08lx\n",(long)LDT);
		TheCPU.LDTR.Base = (long)LDT;
		TheCPU.LDTR.Limit = 0xffff;
	}
#ifdef SKIP_EMU_VBIOS
	if ((ISEG(0x10)==INT10_WATCHER_SEG)&&(IOFF(0x10)==INT10_WATCHER_OFF))
		IOFF(0x10)=CPUEMU_WATCHER_OFF;
#endif
	e_printf("EMU86: turning emuretrace ON\n");
	last_emuretrace = config.emuretrace;
	config.emuretrace = 2;
	JumpOpt = 3;
	JumpOptLim = 0x1000;

	e_printf("EMU86: switching SIGALRMs\n");
	TheCPU.EMUtime = GETTSC();
	sigEMUdelta = config.realdelta*config.CPUSpeedInMhz;
	sigEMUtime = TheCPU.EMUtime + sigEMUdelta;
	TotalTime = SearchTime = AddTime = ExecTime = CleanupTime = 0;
	e_printf("EMU86: delta alrm=%d speed=%d\n",config.realdelta,config.CPUSpeedInMhz);
	e_sigpa_count = 0;
	SETSIG(SIGALRM, e_gen_sigalrm);

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = config.realdelta;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = config.realdelta;
	e_printf("TIME: using %d usec for updating PROF timer\n", config.realdelta);
	setitimer(ITIMER_PROF, &itv, NULL);
	SETSIG(SIGPROF, e_gen_sigprof);
	NEWSETSIG(SIGFPE, e_emu_fault);
	NEWSETSIG(SIGSEGV, e_emu_fault);

	dbug_printf("======================= ENTER CPU-EMU ===============\n");
	flush_log();
	iniflag = 1;
}

void leave_cpu_emu(void)
{
	struct itimerval itv;
	struct sigaction sa;
	extern int MaxDepth, MaxNodes, MaxNodeSize;

	if (config.cpuemu > 1) {
		config.cpuemu=1;
#ifdef SKIP_EMU_VBIOS
		if (IOFF(0x10)==CPUEMU_WATCHER_OFF)
			IOFF(0x10)=INT10_WATCHER_OFF;
#endif
		e_printf("EMU86: turning emuretrace OFF\n");
		config.emuretrace = last_emuretrace;

		NEWSETSIG(SIGFPE, dosemu_fault);
		NEWSETSIG(SIGSEGV, dosemu_fault);
		e_printf("EMU86: switching SIGALRMs\n");
		NEWSETQSIG(SIGALRM, sigalrm);

		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = 0;
		e_printf("TIME: disabling PROF timer\n");
		setitimer(ITIMER_PROF, &itv, NULL);
		SETSIG(SIGPROF, SIG_IGN);

		EndGen();
		mprot_end();

		if (alloc_LDT!=NULL) { free(alloc_LDT); alloc_LDT=LDT=NULL; }
		GDT = NULL; IDT = NULL;
		dbug_printf("======================= LEAVE CPU-EMU ===============\n");
		dbug_printf("Total cpuemu time %16Ld us\n",TotalTime/config.CPUSpeedInMhz);
		dbug_printf("Total exec   time %16Ld us\n",ExecTime/config.CPUSpeedInMhz);
		dbug_printf("Total insert time %16Ld us\n",AddTime/config.CPUSpeedInMhz);
		dbug_printf("Total search time %16Ld us\n",SearchTime/config.CPUSpeedInMhz);
		dbug_printf("Total clean  time %16Ld us\n",CleanupTime/config.CPUSpeedInMhz);
		dbug_printf("Max tree nodes    %16d\n",MaxNodes);
		dbug_printf("Max node size     %16d\n",MaxNodeSize);
		dbug_printf("Max tree depth    %16d\n",MaxDepth);

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

	if (TheCPU.veflags & VIF_MASK)
		flags |= IF_MASK;
	return flags | (TheCPU.veflags & eTSSMASK);
}

static inline int e_revectored(int nr, struct revectored_struct * bitmap)
{
	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (nr)
		:"m" (*bitmap),"r" (nr));
	return nr;
}

static int e_do_int(int i, unsigned char * ssp, unsigned long sp)
{
	unsigned long *intr_ptr, segoffs;

	if (_CS == BIOSSEG)
		goto cannot_handle;
	if (e_revectored(i, &vm86s.int_revectored))
		goto cannot_handle;
	if (i==0x21 && e_revectored(_AH,&vm86s.int21_revectored))
		goto cannot_handle;
	intr_ptr = (unsigned long *) (i << 2);
	segoffs = *intr_ptr;
	if ((segoffs >> 16) == BIOSSEG)
		goto cannot_handle;
	pushw(ssp, sp, e_get_vflags());
	pushw(ssp, sp, _CS);
	pushw(ssp, sp, _IP);
	_CS = segoffs >> 16;
	_SP -= 6;
	_IP = segoffs & 0xffff;
	REG(eflags) &= ~TF_MASK;
	TheCPU.veflags &= ~VIF_MASK;
	if ((i!=0x16)&&((d.emu>1)||(d.dos))) {
		dbug_printf("EMU86: directly calling int %#x ax=%#x at %#x:%#x\n", i, _AX, _CS, _IP);
	}
	return -1;

cannot_handle:
	if ((d.emu>1)||(d.dos))
		dbug_printf("EMU86: calling revectored int %#x\n", i);
	return (VM86_INTx + (i << 8));
}


static int handle_vm86_trap(long *error_code, int trapno)
{
	if ( (trapno==3) || (trapno==1) )
		return (VM86_TRAP + (trapno << 8));
	e_do_int(trapno, (unsigned char *) (_SS << 4), _SP);
	return -1;
}


static int handle_vm86_fault(long *error_code)
{
	unsigned char *csp, *ssp, op;
	unsigned long ip, sp;

	csp = (unsigned char *) (_CS << 4);
	ssp = (unsigned char *) (_SS << 4);
	sp = _SP;
	ip = _IP;
	op = popb(csp, ip);
	if (d.emu>1) e_printf("EMU86: vm86 fault %#x at %#x:%#x\n",
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
  long errcode;

  /* skip emulation of video BIOS, as it is too much timing-dependent */
  if ((config.cpuemu<2)
#ifdef SKIP_EMU_VBIOS
   || ((REG(cs)&0xf000)==config.vbios_seg)
#endif
   ) {
	return TRUE_VM86(&vm86s);
  }
/**/ if (iniflag==0) enter_cpu_emu();

  tt0 = GETTSC();
  e_sigpa_count = 0;
  mode = ADDR16|DATA16; TheCPU.StackMask = 0x0000ffff;
  TheCPU.EMUtime = GETTSC();
#ifdef SKIP_VM86_TRACE
  demusav=d.emu; if (d.emu) d.emu=1;
#endif
//  if (lastEMUsig && (d.emu>1))
//    e_printf("EMU86: last sig at %Ld, curr=%Ld, next=%Ld\n",lastEMUsig>>16,
//    	TheCPU.EMUtime>>16,sigEMUtime>>16);

 /* This emulates VM86_ENTER */
  /* ------ OUTER LOOP: exit for code >=0 and return to dosemu code */
  do {
    Reg2Cpu (&vm86s, mode);

    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
    do {
      /* enter VM86 mode */
      e_printf("INTERP: enter=%08lx\n",trans_addr);
      return_addr = (long)Interp86((char *)trans_addr, mode);
      e_printf("INTERP: exit=%08lx err=%ld\n",return_addr,TheCPU.err-1);
      xval = TheCPU.err;
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("EMU86: error %d\n", -xval);
        in_vm86=0;
        leavedos(0);
      }
      trans_addr = return_addr;
    }
    while (xval==0);
    /* ---- INNER LOOP -- exit for exception ---------------------- */

    Cpu2Reg (&vm86s);
    if (d.emu>1) e_printf("---------------------\n\t   EMU86: EXCP %#x\n", xval-1);

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

  /* In the kernel, this always clears VIP, as it is never in VEFLAGS.
   * But on return from syscall a signal can be caught, and signal_save()
   * will set VIP again. This explains why VIP can be set on return from
   * VM86().
   * In this emulator, if a signal was trapped VIP is already set
   * in eflags, and will be kept by the following operation.
   */
  e_set_flags(REG(eflags), TheCPU.veflags, VIF_MASK | eTSSMASK);

  e_printf("EMU86: retval=%s\n", retdescs[retval&7]);

#ifdef SKIP_VM86_TRACE
  d.emu=demusav;
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

/**/ if (iniflag==0) enter_cpu_emu();

  tt0 = GETTSC();
  e_sigpa_count = 0;
  TheCPU.EMUtime = GETTSC();
  /* make clear we are in PM now */
  TheCPU.cr[0] |= 1;

  if (d.emu>2) {
	long swa = (long)(DTgetSelBase(_cs)+_eip);
	D_printf("DPMI SWITCH to %08lx\n",swa);
  }
//  if (lastEMUsig)
//    e_printf("DPM86: last sig at %Ld, curr=%Ld, next=%Ld\n",lastEMUsig>>16,
//    	TheCPU.EMUtime>>16,sigEMUtime>>16);

  /* ------ OUTER LOOP: exit for code >=0 and return to dosemu code */
  do {
    TheCPU.err = 0;
    mode = Scp2CpuD (scp);
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
      e_printf("INTERP: exit=%08lx err=%ld\n",return_addr,TheCPU.err-1);
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

    if (d.emu>1) e_printf("DPM86: EXCP %#x eflags=%08lx\n",
	xval-1, REG(eflags));

    TheCPU.eflags &= ~TF;	/* is it right? */
    Cpu2Scp (scp, xval-1);

    retval = -1;
    E_TIME_STRETCH;

    if ((xval==EXCP_SIGNAL) || (xval==EXCP_PICSIGNAL) || (xval==EXCP_STISIGNAL)) {
/**/ e_printf("DPMI retcode = %d\n",emu_dpmi_retcode);
	if (emu_dpmi_retcode >= 0) {
	    retval=emu_dpmi_retcode; emu_dpmi_retcode = -1;
	}
    }
    else if (xval==EXCP_GOBACK) {
        retval = 0;
    }
    else {
	TotalTime += (GETTSC() - tt0);
	dpmi_fault(scp);
	tt0 = GETTSC();
	if (emu_dpmi_retcode >= 0) {
		retval=emu_dpmi_retcode; emu_dpmi_retcode = -1;
	}
	else {
	  switch (scp->trapno) {
	    case 0x6c: case 0x6d: case 0x6e: case 0x6f:
	    case 0xe4: case 0xe5: case 0xe6: case 0xe7:
	    case 0xec: case 0xed: case 0xee: case 0xef:
	    case 0xfa: case 0xfb: retval = -1; break;
	    default: retval = 0; break;
	  }
	}
	scp->trapno = 0;
    }
  }
  while (retval < 0);
  /* ------ OUTER LOOP -- exit to user level ---------------------- */

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
