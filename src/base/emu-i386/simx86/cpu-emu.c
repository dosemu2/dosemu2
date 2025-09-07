/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#ifdef X86_EMULATOR
#include <stdlib.h>
#include <string.h>		/* for memset */
#include <sys/time.h>
#include <fenv.h>
#include <pthread.h>
#include <semaphore.h>
#include "kvm.h"
#include "mhpdbg.h"
#include "cpu-emu.h"
#include "emu86.h"
#include "codegen-arch.h"
#include "emudpmi.h"
#include "misc/dis8086.h"
#include "utilities.h"
#include "sig.h"
#if PROFILE >= 2
#include "timers.h"
#endif
#ifndef HAVE___FLOAT80
#include "softfloat/softfloat.h"
#endif

#define PREJIT 1
#define PREJIT_TEST 0
/* clever trick but too expensive in practice, so disable */
#define PREJIT_EXEC 0
#define PREJIT_ASYNC 1

/* ======================================================================= */

#if PROFILE
hitimer_t AddTime, SearchTime, ExecTime, CleanupTime; // for debug
hitimer_t GenTime, LinkTime;
#endif

static pthread_cond_t run_cnd = PTHREAD_COND_INITIALIZER;
static int prejit_running;
static pthread_mutex_t run_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t prejit_thr;
static sem_t prejit_sem;
static void *prejit_thread(void *arg);

static hitimer_t TotalTime;
static void enter_cpu_emu(void);
static void do_cpuemu_enter(int pm);

/* This needs to be merged someday with 'mode' */
int CEmuStat = 0;

/* This keeps the delta time in CPU clocks between SIGALRMs and
 * SIGPROFs, i.e. the time spent by the system outside of dosemu,
 * which should be considered in time stretching */
int e_sigpa_count;

static int _in_vm86_emu;
static int _in_dpmi_emu;
/* can use RELAXED ordering here, as we are interested only in a value
 * of a variable itself, rather than any other vars written before it */
#define in_vm86_emu __atomic_load_n(&_in_vm86_emu, __ATOMIC_RELAXED)
#define in_dpmi_emu __atomic_load_n(&_in_dpmi_emu, __ATOMIC_RELAXED)
#define AW(v, n) __atomic_store_n(&_##v, n, __ATOMIC_RELAXED)
jmp_buf jmp_env;

struct _SynCPU TheCPU_struct;

int InCompiledCode;

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
 * 19	fpregset_t fpstate;
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

static void instr_sim_leave(int pmode);

uint8_t (*Fetch)(dosaddr_t a);
uint16_t (*FetchW)(dosaddr_t a);
uint32_t (*FetchL)(dosaddr_t a);

/* ======================================================================= */

unsigned long eTSSMASK = 0;

void e_priv_iopl(int pl)
{
    pl &= 3;
    TheCPU.eflags = (TheCPU.eflags & ~EFLAGS_IOPL) |
	(pl << 12);
    e_printf("eIOPL: set IOPL to %d, flags=%#x\n",pl,TheCPU.eflags);
}

void InvalidateSegs(void)
{
    CS_DTR.BoundH = CS_DTR.BoundL + SDTR_INVALID_LIMIT;
    DS_DTR.BoundH = DS_DTR.BoundL + SDTR_INVALID_LIMIT;
    ES_DTR.BoundH = ES_DTR.BoundL + SDTR_INVALID_LIMIT;
    FS_DTR.BoundH = FS_DTR.BoundL + SDTR_INVALID_LIMIT;
    GS_DTR.BoundH = GS_DTR.BoundL + SDTR_INVALID_LIMIT;
    SS_DTR.BoundH = SS_DTR.BoundL + SDTR_INVALID_LIMIT;
}

/* ======================================================================= */


static char ehextab[] = "0123456789abcdef";

/* WARNING - do not convert spaces to tabs! */
/* WARNING: the below code is written by some idiots, don't read! */
#define ERB_LLEN	0x39
#define ERB_LEFTM	5
#define ERB_L1		0x00
#define ERB_L2		(ERB_L1+ERB_LLEN)
#define ERB_L3		(ERB_L1+2*ERB_LLEN)
#define ERB_L4		(ERB_L1+3*ERB_LLEN)
#define ERB_L5		(ERB_L1+4*ERB_LLEN)
#define ERB_L6		(ERB_L5+0x23)
#define ERB_L7		(ERB_L6+0x37)
static char eregbuf[] =
/*00*/	"\teax=00000000 ebx=00000000 ecx=00000000 edx=00000000    \n"
/*39*/	"\tesi=00000000 edi=00000000 ebp=00000000 esp=00000000    \n"
/*72*/	"\t vf=00000000  cs=0000      ds=0000      es=0000        \n"
/*ab*/	"\t fs=0000      gs=0000      ss=0000     eip=00000000    \n"
/*e4*/  "\tops=00 00 00 00 00 00 00 00 00 00\n"
        "\tstk=0000 0000 0000 0000 0000 0000 0000 0000 0000 0000\n";

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

static inline void exprintb(unsigned char val,char *bf,unsigned int pos)
{
	char *p=bf+pos+1;
	unsigned long v = val;
	while (v) { *p-- = ehextab[v&15]; v>>=4; }
}

char *e_print_regs(void)
{
	static char buf[sizeof(eregbuf) + 300];
	char *p = buf;
	char *q = eregbuf;

	while (*q) *p++ = *q++;
	*p=0;
	exprintl(rEAX,buf,(ERB_L1+ERB_LEFTM));
	exprintl(rEBX,buf,(ERB_L1+ERB_LEFTM)+13);
	exprintl(rECX,buf,(ERB_L1+ERB_LEFTM)+26);
	exprintl(rEDX,buf,(ERB_L1+ERB_LEFTM)+39);
	exprintl(rESI,buf,(ERB_L2+ERB_LEFTM));
	exprintl(rEDI,buf,(ERB_L2+ERB_LEFTM)+13);
	exprintl(rEBP,buf,(ERB_L2+ERB_LEFTM)+26);
	exprintl(rESP,buf,(ERB_L2+ERB_LEFTM)+39);
	exprintl(TheCPU.eflags,buf,(ERB_L3+ERB_LEFTM));
	exprintw(TheCPU.cs,buf,(ERB_L3+ERB_LEFTM)+13);
	exprintw(TheCPU.ds,buf,(ERB_L3+ERB_LEFTM)+26);
	exprintw(TheCPU.es,buf,(ERB_L3+ERB_LEFTM)+39);
	exprintw(TheCPU.fs,buf,(ERB_L4+ERB_LEFTM));
	exprintw(TheCPU.gs,buf,(ERB_L4+ERB_LEFTM)+13);
	exprintw(TheCPU.ss,buf,(ERB_L4+ERB_LEFTM)+26);
	exprintl(TheCPU.eip,buf,(ERB_L4+ERB_LEFTM)+39);
	{
		int i;
		dosaddr_t csp = LONG_CS+TheCPU.eip;
		dosaddr_t st = LONG_SS+TheCPU.esp;
		if (csp < 0xa0000 - 256 || dpmi_is_valid_range(csp, 256)) {
			unsigned char *op = EMU_BASE32(csp);
			for (i=(ERB_L5+ERB_LEFTM); i<(ERB_L6); i+=3) {
			   exprintb(*op++,buf,i);
			}
		}
		if (st < 0xa0000 - 256 || dpmi_is_valid_range(st, 256)) {
			unsigned short *stk = (unsigned short *)EMU_BASE32(st);
			for (i=(ERB_L6+ERB_LEFTM); i<(ERB_L7-2); i+=5) {
			   exprintw(*stk++,buf,i);
			}
		}
	}
	return buf;
}

#if MAX_SELECTORS != 8192
#error MAX_SELECTORS needs to be 8192
#endif

#define GetSegmentBaseAddress(s)	GetSegmentBase(s)
#define IsSegment32(s)			dpmi_segment_is32(s)

char *e_print_scp_regs(cpuctx_t *scp, int pmode)
{
	static char buf[300];
	unsigned short *stk;
	int i, j;

	i = sprintf(buf, "RAX: %08x  RBX: %08x  RCX: %08x  RDX: %08x"
		"  VFLAGS(h): %08x\n",
		_rax, _rbx, _rcx, _rdx, _eflags);
	i += sprintf(buf + i, "RSI: %08x  RDI: %08x  RBP: %08x  RSP: %08x\n",
		_rsi, _rdi, _rbp, _rsp);
	i += sprintf(buf + i, "CS: %04x  DS: %04x  ES: %04x  FS: %04x  GS: %04x  SS: %04x\n",
		_cs, _ds, _es, _fs, _gs, _ss);

	if (pmode & 2) {
//		buf[(ERB_L4+ERB_LEFTM)+47] = 0;
	}
	else {
	    if (pmode & 1)
		stk = SEL_ADR(_ss, _esp);
	    else
		stk = MK_FP32(_ss,_LWORD(esp));
	    i += sprintf(buf + i, "Stack:");
	    for (j = 0; j < 16; j++)
		i += sprintf(buf + i, " %04hx", *stk++);
	    sprintf(buf + i, "\n");
	}
	return buf;
}


char *e_emu_disasm(unsigned char *org, int is32, unsigned int refseg)
{
   static char buf[512];
   static char frmtbuf[256];
   int rc = 0;
   int i;
   char *p = buf, *p1;
   dosaddr_t code;
   dosaddr_t org2;
   unsigned int segbase;
#ifdef USE_MHPDBG
   unsigned int ref;
#endif

   if (in_dpmi_emu)
     segbase = GetSegmentBase(refseg);
   else
     segbase = refseg * 16;
   code = EMUADDR_REL(org);
   org2 = code - segbase;
#ifdef USE_MHPDBG
   rc = dis_8086(code, frmtbuf, is32, &ref, segbase);
   p += sprintf(p, "%08x: ", code);
#endif
   for (i=0; i<rc && i<8; i++) {
	p += sprintf(p, "%02x", READ_BYTE(code+i));
   }
   sprintf(p,"%20s", " ");
   p1 = buf + 28;
   if (is32)
     p = p1 + sprintf(p1, "%04x:%08x %s", refseg, org2, frmtbuf);
   else
     p = p1 + sprintf(p1, "%04x:%04x %s", refseg, org2, frmtbuf);

   return buf;
}

#ifdef TRACE_DPMI
char *e_scp_disasm(cpuctx_t *scp, int pmode)
{
   static char insrep = 0;
   static unsigned char buf[1024];
   static unsigned char frmtbuf[256];
   static unsigned int lasta = 0;
   int rc;
   int i;
   unsigned char *p, *pb, *org2;
   dosaddr_t org, csp2;
   unsigned int refseg, seg;
   unsigned int ref;

   *buf = 0;
   seg = _cs;
   refseg = seg;
   if (!(seg & 0x0004)) {
      csp2 = org = EMUADDR_REL(LINP(_rip)); /* XXX bogus for x86_64 */
   }
   else {
      csp2 = 0;
      if (scp->cs <= 0xffff)
         csp2 = GetSegmentBase(seg);
      org = csp2 + _eip;
   }
   if (org==lasta) { insrep=1; return buf; } /* skip 'rep xxx' steps */
   lasta = org; insrep = 0;

   rc = dis_8086(org, frmtbuf, pmode&&IsSegment32(seg),
   	&ref, (pmode? csp2 : refseg * 16));

   pb = buf;
   org2 = EMU_BASE32(org);
   while ((*org2&0xfc)==0x64) org2++;	/* skip most prefixes */
   if ((debug_level('t')>3)||(InterOps[*org2]&2))
	pb += sprintf(pb,"%s",e_print_scp_regs(scp,pmode));

   p = pb + sprintf(pb,"  %08x: ",org);
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", READ_BYTE(org+i));
   }
   sprintf(p,"%20s", " ");
   if (pmode&&IsSegment32(seg))
     sprintf(pb + 30, "%04x:%08x %s\n", _cs, _eip, frmtbuf);
   else
     sprintf(pb + 30, "%04x:%04x %s\n", _cs, _eip, frmtbuf);

   return buf;
}
#endif


const char *e_trace_fp(void)
{
	static char buf[512];
	int i, ifpr;
	char *p;
	long double *FPRSTT;

	if (!TheCPU.fpregs)
	  return "";
	FPRSTT = &TheCPU.fpregs[TheCPU.fpstt];
	ifpr = TheCPU.fpstt&7;
	p = buf;
	for (i=0; i<8; i++) {
	  long double *q = &TheCPU.fpregs[ifpr];
	  char buf2[32];
#ifdef FP_DISPHEX
	  long long x;
	  memcpy(&x, q, sizeof(x));
	  sprintf(buf2,"\t%016llx", x);
#else
	  *buf2=0;
#endif
#ifdef FPU_TAGS
	  switch ((TheCPU.fptag >> (ifpr<<1)) & 3) {
	    case 0: case 1:
#endif
		p += sprintf(p,"\tFp%d\t%16.8Lf%s\n\t", ifpr, *q, buf2);
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
	p += sprintf(p,"\tst=%d(%p) sw=%04x cw=%04x tag=%04x\n",
		TheCPU.fpstt, FPRSTT, TheCPU.fpus, TheCPU.fpuc, TheCPU.fptag);
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
		"cUNP","xUNP","ZERO","  GS","  FS","  ES","  DS"," EDI",
		" ESI"," EBP"," ESP"," EBX"," EDX"," ECX"," EAX","TPNO",
		"SCPE"," EIP","  CS","EFLG","  SS"," CR2","FPCS","FPST",
		"FNI0","FNI1","FNI2"," SIG"," ERR","SMSK","MODE"," CR0",
		"FPRG","xFPR","TIML","TIMH"," GSl"," GSu"," GSo"," FSl",
		" FSu"," FSo"," ESl"," ESu"," XES"," DSl"," DSu"," DSo",
		" CSl"," CSu"," CSo"," SSl"," SSh"," SSo"," s16","xs16",
		" s32","xs32","  w8"," xw8"," w16","xw16"," w32","xw32"
	};
	static char m1[32];
	static int i = 0;
	char *p;
	unsigned int ix = r;
	i = (i+8) & 0x18; p = m1+i;	// for side effects in printf
	if ((ix&0xff)==0xff) { *p=0; return p; }
	ix = (ix>>2)&0x3f;
	*((int *)p) = *((const int *)(s4[ix]));
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
static void Reg2Cpu(struct vm86_struct *info)
{
  struct vm86_regs *regs = &info->regs;
 /*
  * Enter VM86
  */

 /* From now on we'll work on the cpuemu eflags (BUT vm86s eflags can be
  * changed asynchronously by signals)
  * Note that IOPL=3 in the emulated flags so cpuemu works directly with
    IF, not VIF */
  TheCPU.eflags = (regs->eflags & SAFE_MASK) | IOPL_MASK | EFLAGS_SET;
  if (regs->eflags & VIF)
    TheCPU.eflags |= EFLAGS_IF;
  TheCPU.eflags |= (VM | RF);	// RF is cosmetic...
  TheCPU.df_increments = (TheCPU.eflags&DF)?0xfcfeff:0x040201;

  if (debug_level('e')>1) e_printf("Reg2Cpu> vm86=%08x dpm=%08x emu=%08x\n",
	regs->eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags);
  TheCPU.eax     = regs->eax;	/* 2c -> 18 */
  TheCPU.ebx     = regs->ebx;	/* 20 -> 00 */
  TheCPU.ecx     = regs->ecx;	/* 28 -> 04 */
  TheCPU.edx     = regs->edx;	/* 24 -> 08 */
  TheCPU.esi     = regs->esi;	/* 14 -> 0c */
  TheCPU.edi     = regs->edi;	/* 10 -> 10 */
  TheCPU.ebp     = regs->ebp;	/* 18 -> 14 */
  TheCPU.esp     = regs->esp;	/* 1c -> 3c */
  TheCPU.err     = 0;
  TheCPU.eip     = regs->eip&0xffff;

  TheCPU.int_revectored = info->int_revectored;

  SetSegReal(regs->cs,Ofs_CS);
  SetSegReal(regs->ss,Ofs_SS);
  SetSegReal(regs->ds,Ofs_DS);
  SetSegReal(regs->es,Ofs_ES);
  SetSegReal(regs->fs,Ofs_FS);
  SetSegReal(regs->gs,Ofs_GS);

  /* FPU state is loaded later on demand for JIT, not used for simulator */
  TheCPU.fpstate = &vm86_fpu_state;
  LONG_CS = _LONG_CS;
  if (debug_level('e')>1) {
	if (debug_level('e')==9) e_printf("Reg2Cpu< vm86=%08x dpm=%08x emu=%08x\n%s\n",
		regs->eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags,
		e_print_regs());
	else e_printf("Reg2Cpu< vm86=%08x dpm=%08x emu=%08x\n",
		regs->eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags);
  }
}

/*
 * Exit emulator in VM86 mode and return to dosemu (return_to_32bit)
 */
static void Cpu2Reg(struct vm86_struct *info)
{
  struct vm86_regs *regs = &info->regs;
  if (debug_level('e')>1) e_printf("Cpu2Reg> vm86=%08x dpm=%08x emu=%08x\n",
	regs->eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags);
  regs->eax = TheCPU.eax;
  regs->ebx = TheCPU.ebx;
  regs->ecx = TheCPU.ecx;
  regs->edx = TheCPU.edx;
  regs->esi = TheCPU.esi;
  regs->edi = TheCPU.edi;
  regs->ebp = TheCPU.ebp;
  regs->esp = TheCPU.esp;
  regs->ds  = TheCPU.ds;
  regs->es  = TheCPU.es;
  regs->ss  = TheCPU.ss;
  regs->fs  = TheCPU.fs;
  regs->gs  = TheCPU.gs;
  regs->cs  = TheCPU.cs;
  regs->eip = TheCPU.eip;
  /*
   * move flags from EFLAGS to eflags; resync vm86s eflags
   * from the emulated ones.
   * The cpuemu should not change VIP, the good one is always in vm86s.
   */
  regs->eflags = (regs->eflags & VIP) |
			(TheCPU.eflags & ~VIP);
  if (TheCPU.eflags & EFLAGS_IF)
    regs->eflags |= VIF;
  else
    regs->eflags &= ~VIF;
  regs->eflags |= EFLAGS_IF;

  if (TheCPU.fpstate == NULL) {
    if (!CONFIG_CPUSIM)
      savefpstate(vm86_fpu_state);
    else
      fp87_save_except();
    fesetenv(&dosemu_fenv);
  }

  if (debug_level('e')>1) e_printf("Cpu2Reg< vm86=%08x dpm=%08x emu=%08x\n",
	regs->eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags);
}


/* ======================================================================= */

static void Scp2Cpu(cpuctx_t *scp)
{
  TheCPU.eax = _eax;
  TheCPU.ebx = _ebx;
  TheCPU.ecx = _ecx;
  TheCPU.edx = _edx;
  TheCPU.esi = _esi;
  TheCPU.edi = _edi;
  TheCPU.ebp = _ebp;
  TheCPU.esp = _esp;

  TheCPU.eip = _eip;
  TheCPU.eflags = _eflags | 2;

  TheCPU.scp_err = 0;
  TheCPU.cr[2] = _cr2;
  TheCPU.df_increments = (TheCPU.eflags&DF)?0xfcfeff:0x040201;

  TheCPU.fpstate = &vm86_fpu_state;
}

/*
 * Build a sigcontext structure to enter fault handling from DPMI
 */
static void Cpu2Scp(cpuctx_t *scp, int trapno)
{
  if (debug_level('e')>1) e_printf("Cpu2Scp> scp=%08x dpm=%08x fl=%08x\n",
	_eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags);

  /* setup stack context from cpu registers */
  _eax = TheCPU.eax;
  _ebx = TheCPU.ebx;
  _ecx = TheCPU.ecx;
  _edx = TheCPU.edx;
  _esi = TheCPU.esi;
  _edi = TheCPU.edi;
  _ebp = TheCPU.ebp;
  _esp = TheCPU.esp;
  _eip = TheCPU.eip;

  _cs = TheCPU.cs;
  _fs = TheCPU.fs;
  _gs = TheCPU.gs;

  _ds = TheCPU.ds;
  _es = TheCPU.es;

  _err = TheCPU.scp_err;
  _ss = TheCPU.ss;
  _cr2 = TheCPU.cr[2];
  _trapno = trapno;
  /* Error code format:
   * b31-b16 = 0 (undef)
   * b15-b0  = selector, where b2=LDT/GDT, b1=IDT, b0=EXT
   * (b0-b1 are currently unimplemented here)
   */
  if (!TheCPU.err) _err = 0;		//???
  if (TheCPU.fpstate == NULL) {
    if (!CONFIG_CPUSIM)
      savefpstate(vm86_fpu_state);
    else
      fp87_save_except();
    /* there is no real need to save and restore the FPU state of the
       emulator itself: savefpstate (fnsave) also resets the current FPU
       state using fninit; fesetenv then restores trapping of division by
       zero and overflow which is good enough for calling FPU-using
       routines.
    */
    fesetenv(&dosemu_fenv);
  }

  /* push running flags - same as eflags, RF is cosmetic */
  _eflags = (TheCPU.eflags & (eTSSMASK|0xfd5)) | 0x10002;
  if (debug_level('e')>1) e_printf("Cpu2Scp< scp=%08x dpm=%08x fl=%08x\n",
	_eflags,get_FLAGS(TheCPU.eflags),TheCPU.eflags);
}


/* ======================================================================= */
/*
 * Register movements between the dosemu REGS and the emulator cpu
 * B - DPMI and PM mode
 */

/*
 * Enter emulator in DPMI mode (context_switch)
 */
static void Scp2CpuD(cpuctx_t *scp)
{
  unsigned char big; int mode=0;

  Scp2Cpu(scp);

  /* make clear we are in PM now */
  TheCPU.cr[0] |= 1;
  mode |= ADDR16;
  InvalidateSegs(); // makes sure real mode segs aren't confused with PM sels
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_CS,&big,_cs);
  if (TheCPU.err) goto erseg;
  if (big) mode=MBIGCS; else mode |= DATA16;

  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_DS,&big,_ds);
  if (TheCPU.err) goto erseg;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_SS,&big,_ss);
  if (TheCPU.err) goto erseg;
  TheCPU.StackMask = (big? 0xffffffff : 0x0000ffff);
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_ES,&big,_es);
  if (TheCPU.err) goto erseg;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_FS,&big,_fs);
  if (TheCPU.err) goto erseg;
  TheCPU.err = SetSegProt(mode&ADDR16,Ofs_GS,&big,_gs);
erseg:
  if (debug_level('e')>1) {
	e_printf("Scp2CpuD%s: CS:IP=%08x:%08x\n%s\n",
			(TheCPU.err? " ERR":""),
			LONG_CS, _eip,
			e_print_regs());
  }
  TheCPU.mode = mode;
  LONG_CS = _LONG_CS;
}


/* ======================================================================= */


void reset_emu_cpu(void)
{
  if (CEmuStat & CeS_INSTREMU)
    instr_sim_leave(CEmuStat & CeS_INSTREMU_PM);
  TheCPU.cr[0] = 0x13;	/* valid bits: 0xe005003f */
  TheCPU.cr[4] = CR4_VME;
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
  TheCPU.StackMask = 0x0000ffff;
}

void init_emu_cpu(int cpu_type)
{
  init_emu_npu();

  switch (cpu_type) {
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
  mprot_init();
  InitGen();
  InitTrees();
  sem_init(&prejit_sem, 0, 0);
  pthread_create(&prejit_thr, NULL, prejit_thread, NULL);
  prejit_init();

  IDT = NULL;
  if (GDT==NULL) {
	/* The GDT is not really used (yet?) but some instructions
	   like verr/verw refer to it, so just allocate a 0 GDT */
	GDT = calloc(65536,1);
  }
  /* use the cached LDT used by dpmi (w/o GDT) */
  if (LDT==NULL) {
	LDT = (Descriptor *)dpmi_get_ldt_buffer();
	e_printf("LDT allocated at %p\n",LDT);
	TheCPU.LDTR.Base = (long)LDT;
	TheCPU.LDTR.Limit = 0xffff;
  }
  enter_cpu_emu();
}

/*
 * Under cpuemu, SIGALRM is redirected here. We need a source of
 * asynchronous signals because without it any badly-behaved pgm
 * can stop us forever.
 */
void e_gen_sigalrm(void)
{
	if (!in_dpmi_emu && !in_vm86_emu)
	    return;

	/* here we come from the kernel with cs==UCODESEL, as
	 * the passed context is that of dosemu, NOT that of the
	 * emulated CPU! */
	sigalrm_pending_w(1);
}

void e_gen_sigalrm_from_thread(void)
{
	__atomic_store_n(&TheCPU.sigalrm_pending, 1, __ATOMIC_RELAXED);
}

static void enter_cpu_emu(void)
{
	unsigned int realdelta = config.update / TIMER_DIVISOR;

	if (debug_level('e')) {
		TotalTime = 0;
#if PROFILE
		SearchTime = AddTime = ExecTime = CleanupTime =
		GenTime = LinkTime = 0;
#endif
		dbug_printf("EMU86: delta alrm=%d speed=%d\n",
			    realdelta,config.CPUSpeedInMhz);
	}
	e_sigpa_count = 0;

#ifdef DEBUG_TREE
	tLog = fopen(DEBUG_TREE_FILE,"w");
#endif
#ifdef ASM_DUMP
	aLog = fopen(ASM_DUMP_FILE,"w");
#endif
	dbug_printf("======================= ENTER CPU-EMU ===============\n");
	flush_log();
}

static void print_statistics(void)
{
	dbug_printf("Total cpuemu time %16lld us (incl.trace)\n",
		    (long long)TotalTime/config.CPUSpeedInMhz);
#if PROFILE
	dbug_printf("Total codgen time %16lld us\n",
		    (long long)GenTime/config.CPUSpeedInMhz);
	dbug_printf("Total linker time %16lld us\n",
		    (long long)LinkTime/config.CPUSpeedInMhz);
	dbug_printf("Total exec   time %16lld us (incl.faults)\n",
		    (long long)ExecTime/config.CPUSpeedInMhz);
	dbug_printf("Total insert time %16lld us\n",
		    (long long)AddTime/config.CPUSpeedInMhz);
	dbug_printf("Total search time %16lld us\n",
		    (long long)SearchTime/config.CPUSpeedInMhz);
	dbug_printf("Total clean  time %16lld us\n",
		    (long long)CleanupTime/config.CPUSpeedInMhz);
	dbug_printf("Max tree nodes    %16d\n",MaxNodes);
	dbug_printf("Max node size     %16d\n",MaxNodeSize);
	dbug_printf("Max tree depth    %16d\n",MaxDepth);
	dbug_printf("Nodes parsed      %16d\n",TotalNodesParsed);
	dbug_printf("Find misses       %16d\n",NodesNotFound);
	dbug_printf("Nodes executed    %16d\n",TotalNodesExecd);
	dbug_printf("Prejitted execed  %16d\n",PrejitNodesExecd);
	dbug_printf("Nodes prejitted   %16d\n",NodesPrejitted);
	dbug_printf("Speculative prej  %16d\n",SpecPrejits);
	if (TotalNodesExecd) {
		unsigned long long k;
		k = ((long long)NodesFound * 100UL) /
			(long long)TotalNodesExecd;
		dbug_printf("Find hits         %16d (%lld%%)\n",NodesFound,k);
		k = ((long long)NodesFastFound * 100UL) /
			(long long)TotalNodesExecd;
		dbug_printf("Find last hits    %16d (%lld%%)\n",
			    NodesFastFound,k);
	}
	dbug_printf("Page faults       %16d\n",PageFaults);
	dbug_printf("Signals received  %16d\n",EmuSignals);
	dbug_printf("Tree cleanups     %16d\n",TreeCleanups);
	dbug_printf("C patches applied %16d\n",CpatchTotal);
	dbug_printf("C patches removed %16d\n",UncpatchTotal);
	dbug_printf("Cpatch reps       %16d\n",CpatchReps);
	dbug_printf("Cpatch writes     %16d\n",CpatchWrites);
	dbug_printf("Cpatch stk writes %16d\n",CpatchStkWrites);
	dbug_printf("Cpatch invds      %16d\n",CpatchInvalidates);
	dbug_printf("Cache page drops  %16d\n",CPagesDropped);
	dbug_printf("Max cached pages  %16d\n",MaxCPages);
#endif
}

void leave_cpu_emu(void)
{
	if (CEmuStat & CeS_INSTREMU)
		instr_sim_leave(CEmuStat & CeS_INSTREMU_PM);
#ifdef SKIP_EMU_VBIOS
	if (IOFF(0x10)==CPUEMU_WATCHER_OFF)
		IOFF(0x10)=INT10_WATCHER_OFF;
#endif
	EndGen();
#ifdef DEBUG_TREE
	fclose(tLog); tLog = NULL;
#endif
#ifdef ASM_DUMP
	fclose(aLog); aLog = NULL;
#endif
	mprot_end();

	free(GDT);
	LDT = NULL; GDT = NULL; IDT = NULL;
	dbug_printf("======================= LEAVE CPU-EMU ===============\n");
	if (debug_level('e')) print_statistics();
	prejit_done();
	pthread_cancel(prejit_thr);
	pthread_join(prejit_thr, NULL);
	sem_destroy(&prejit_sem);
	flush_log();
}


/* =======================================================================
 * kernel-level vm86 handling - this has been mostly moved into interp.c
 * from /linux/arch/i386/kernel/vm86.c
 * original code by Linus Torvalds and later enhancements by
 * Lutz Molgedey and Hans Lermen.
 */
static int handle_vm86_fault(struct vm86_struct *info, int *error_code)
{
	struct vm86_regs *regs = &info->regs;
	unsigned int csp, ip;
	unsigned char op;

	csp = SEGOFF2LINEAR(regs->cs, 0);
	ip = regs->eip & 0xffff;
	op = popb(csp, ip);
	if (debug_level('e')>1) e_printf("EMU86: vm86 fault %#x at %#x:%#x\n",
		op, regs->cs, regs->eip & 0xffff);

	/* int xx */
	if (op==0xcd) {
	        int intno=popb(csp, ip);
		regs->eip += 2;
		if (debug_level('e')>1)
			dbug_printf("EMU86: calling revectored int %#x\n", intno);
		return VM86_INTx + (intno << 8);
	}
	else
		return VM86_UNKNOWN;
}

/*
 * =======================================================================
 */
static const char *retdescs[] =
{
	"VM86_SIGNAL","VM86_UNKNOWN","VM86_INTx","VM86_STI",
	"VM86_PICRET","???","VM86_TRAP","???"
};

#if PREJIT_TEST
static int prejit_vm86(struct vm86_struct *info);
static int prejit_dpmi(cpuctx_t *scp);
#endif

static int e_vm86_tail(struct vm86_struct *info);

int e_vm86(struct vm86_struct *info)
{
  int ret;
#if PREJIT_TEST
  prejit_vm86(info);
#endif
  sigalrm_pending_w(0);

  e_sigpa_count = 0;
#ifdef SKIP_VM86_TRACE
  demusav=debug_level('e'); if (debug_level('e')) set_debug_level('e', 1);
#endif
//  if (lastEMUsig && (debug_level('e')>1))
//    e_printf("EMU86: last sig at %lld, curr=%lld, next=%lld\n",lastEMUsig>>16,
//    	TheCPU.EMUtime>>16,sigEMUtime>>16);

 /* This emulates VM86_ENTER */
  prejit_lock();
  TheCPU.mode = ADDR16 | DATA16 | MREALA;
  TheCPU.StackMask = 0x0000ffff;
  Reg2Cpu(info);
  ret = e_vm86_tail(info);
  prejit_unlock();
  return ret;
}

static int e_vm86_tail(struct vm86_struct *info)
{
  int xval,retval;
#ifdef SKIP_VM86_TRACE
  int demusav;
#endif
  int errcode;
    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
  do {
      /* enter VM86 mode */
      AW(in_vm86_emu, 1);
      if (debug_level('e')>1)
	dbug_printf("INTERP: enter=%08x\n",LONG_CS+TheCPU.eip);
      Interp86();
      if (debug_level('e')>1)
	dbug_printf("INTERP: exit=%08x err=%d\n",LONG_CS+TheCPU.eip,TheCPU.err-1);
      xval = TheCPU.err;
      AW(in_vm86_emu, 0);
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("EMU86: error %d\n", -xval);
        in_vm86=0;
        leavedos_main(1);
        return -1;
      }
  }
  while (xval==0);
  /* ---- INNER LOOP -- exit for exception ---------------------- */

  Cpu2Reg(info);
  if (debug_level('e')>1) e_printf("---------------------\n\t   EMU86: EXCP %#x\n", xval-1);

  retval = -1;

  if (xval==EXCP_SIGNAL) {	/* coming here for async interruptions */
    if (CEmuStat & (CeS_SIGPEND|CeS_SIGACT))
		{ CEmuStat &= ~(CeS_SIGPEND|CeS_SIGACT); retval=VM86_SIGNAL; }
  } else if (xval==EXCP_PICSIGNAL) {
    retval = VM86_PICRETURN;
  } else if (xval==EXCP_STISIGNAL) {
    retval = VM86_STI;
  } else if (xval==EXCP_GOBACK) {
    retval = 0;
  } else if (xval==EXCP_EMULEAVE) {
    instr_sim_leave(0);
    retval = 0;
  } else {
	switch (xval) {
	    case EXCP0D_GPF: {	/* to kernel vm86 */
		retval=handle_vm86_fault(info, &errcode);	/* kernel level */
#ifdef SKIP_EMU_VBIOS
		/* are we into the VBIOS? If so, exit and reenter e_vm86 */
		if ((SREG(cs)&0xf000)==config.vbios_seg) {
		    if (retval<0) retval=0;  /* force exit even if handled */
		}
#endif
	      }
	      break;
	    case EXCP01_SSTP:
	    case EXCP03_INT3: {	/* to kernel vm86 */
		retval = VM86_TRAP + ((xval-1) << 8);
		break;
	      }
	    default: {
		/* FAULT, handled via signal callback */
		vm86_fault(xval-1, TheCPU.scp_err, TheCPU.cr[2]);
		retval = VM86_SIGNAL;
		break;
	    }
	}
  }

  if (debug_level('e')>1)
    e_printf("EMU86: retval=%s\n", retdescs[retval&7]);

#ifdef SKIP_VM86_TRACE
  set_debug_level('e',demusav);
#endif
  return retval;
}

/* ======================================================================= */

static int e_dpmi_tail(cpuctx_t *scp);

int e_dpmi(cpuctx_t *scp)
{
  int ret;
#if PREJIT_TEST
  prejit_dpmi(scp);
#endif
  sigalrm_pending_w(0);

  e_sigpa_count = 0;

  if (debug_level('e')>2) {
	D_printf("EMU86: DPMI enter at %08x\n",DTgetSelBase(_cs)+_eip);
  }
//  if (lastEMUsig)
//    e_printf("DPM86: last sig at %lld, curr=%lld, next=%lld\n",lastEMUsig>>16,
//    	TheCPU.EMUtime>>16,sigEMUtime>>16);

  prejit_lock();
  TheCPU.err = 0;
  Scp2CpuD(scp);
  ret = e_dpmi_tail(scp);
  prejit_unlock();
  return ret;
}

static int e_dpmi_tail(cpuctx_t *scp)
{
  int xval,retval;
  if (TheCPU.err) {
    error("DPM86: segment error %d\n", TheCPU.err);
    leavedos_main(0);
    return -1;
  }

    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
  do {
      /* switch to DPMI process */
      AW(in_dpmi_emu, 1);
      e_printf("INTERP: enter=%08x mode=%04x\n",LONG_CS+TheCPU.eip,TheCPU.mode);
      Interp86();
      e_printf("INTERP: exit=%08x err=%d\n",LONG_CS+TheCPU.eip,TheCPU.err-1);
      xval = TheCPU.err;
      AW(in_dpmi_emu, 0);
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("DPM86: error %d\n", -xval);
        error("@\n%s",e_print_regs());
        leavedos_main(0);
        return -1;
      }
  }
  while (xval==0);
  /* ---- INNER LOOP -- exit for exception ---------------------- */

  if (debug_level('e')>1) e_printf("DPM86: EXCP %#x eflags=%08x\n",
	xval-1, TheCPU.eflags);

  Cpu2Scp(scp, xval-1);

  if ((xval==EXCP_SIGNAL) || (xval==EXCP_PICSIGNAL) || (xval==EXCP_STISIGNAL)) {
    retval = DPMI_RET_DOSEMU;
  }
  else if (xval==EXCP_GOBACK) {
    retval = DPMI_RET_DOSEMU;
  } else if (xval==EXCP_EMULEAVE) {
    instr_sim_leave(1);
    retval = DPMI_RET_DOSEMU;
  } else {
    retval = DPMI_RET_FAULT;
  }
  /* ------ OUTER LOOP -- exit to user level ---------------------- */

  return retval;
}

static void *prejit_thread(void *arg)
{
  while (1) {
    sem_wait(&prejit_sem);
    PreJit86(LONG_CS + TheCPU.eip, TheCPU.mode);
    fesetenv(&dosemu_fenv);
    pthread_mutex_lock(&run_mtx);
    prejit_running = 0;
    pthread_mutex_unlock(&run_mtx);
    pthread_cond_signal(&run_cnd);
  }
  return NULL;
}

#define PREJIT_RV_OFFS 0xff

static int prejit_vm86(struct vm86_struct *info)
{
#if PREJIT_EXEC
  int kvm_left = 0;
#endif
#if PREJIT_ASYNC
  int r;
#endif
#if !PREJIT
  return 0;
#endif
  if (isset_TF())
    return 0;
#if !PREJIT_EXEC
  if (!vga.inst_emu)
    return 0;
#endif
#if PREJIT_ASYNC
  pthread_mutex_lock(&run_mtx);
  r = prejit_running;
  pthread_mutex_unlock(&run_mtx);
  if (r)
    return 0;
#else
  prejit_lock();
#endif
  TheCPU.err = 0;
  TheCPU.mode = ADDR16 | DATA16 | MREALA;
  TheCPU.StackMask = 0x0000ffff;
  Reg2Cpu(info);
#if PREJIT_EXEC
  if (e_querymark(LONG_CS + TheCPU.eip, 1)) {
    kvm_leave(0);  // invalidates dirty pages
    kvm_left = 1;
  }
  if (e_querymark(LONG_CS + TheCPU.eip, 1)) {
    int ret;
    CEmuStat |= CeS_PREJIT_RM;
    if (config.cpu_vm == CPUVM_KVM)
      do_cpuemu_enter(0);
    ret = e_vm86_tail(info);
    if (config.cpu_vm == CPUVM_KVM) {
      cpuemu_leave(0);
      kvm_enter(0);
      kvm_left = 0;
    }
    CEmuStat &= ~CeS_PREJIT_RM;
    if (TheCPU.err != EXCP_GOBACK) {
      assert(ret != PREJIT_RV_OFFS);
      return ret - PREJIT_RV_OFFS;
    }
    TheCPU.err = 0;
  }
  if (kvm_left)
    kvm_enter(0);
#endif
  pthread_mutex_lock(&run_mtx);
  prejit_running = 1;
  pthread_mutex_unlock(&run_mtx);
#if !PREJIT_ASYNC
  prejit_unlock();
#endif
  sem_post(&prejit_sem);
  return 0;
}

static int prejit_dpmi(cpuctx_t *scp)
{
#if PREJIT_EXEC
  int kvm_left = 0;
#endif
#if PREJIT_ASYNC
  int r;
#endif
#if !PREJIT
  return 0;
#endif
  if (_eflags & TF)
    return 0;
#if !PREJIT_EXEC
  if (!vga.inst_emu)
    return 0;
#endif
#if PREJIT_ASYNC
  pthread_mutex_lock(&run_mtx);
  r = prejit_running;
  pthread_mutex_unlock(&run_mtx);
  if (r)
    return 0;
#else
  prejit_lock();
#endif
  TheCPU.err = 0;
  Scp2CpuD(scp);  // don't inline as this updates TheCPU.MODE!
#if PREJIT_EXEC
  if (e_querymark(LONG_CS + TheCPU.eip, 1)) {
    kvm_leave(1);
    kvm_left = 1;
#if 0
    if (config.cpu_vm_dpmi == CPUVM_KVM &&
        (LONG_CS + TheCPU.eip >= LOWMEM_SIZE + HMASIZE))
      e_invalidate_dirty_full();
#endif
  }
  if (e_querymark(LONG_CS + TheCPU.eip, 1)) {
    int ret;
    CEmuStat |= CeS_PREJIT_PM;
    if (config.cpu_vm_dpmi == CPUVM_KVM)
      do_cpuemu_enter(1);
    ret = e_dpmi_tail(scp);
    if (config.cpu_vm_dpmi == CPUVM_KVM) {
      cpuemu_leave(1);
      kvm_enter(1);
      kvm_left = 0;
    }
    CEmuStat &= ~CeS_PREJIT_PM;
    if (TheCPU.err != EXCP_GOBACK) {
      assert(ret != PREJIT_RV_OFFS);
      return ret - PREJIT_RV_OFFS;
    }
    TheCPU.err = 0;
  }
  if (kvm_left)
    kvm_enter(1);
#endif
  pthread_mutex_lock(&run_mtx);
  prejit_running = 1;
  pthread_mutex_unlock(&run_mtx);
#if !PREJIT_ASYNC
  prejit_unlock();
#endif
  sem_post(&prejit_sem);
  return 0;
}

#ifdef USE_KVM
int kvm_vm86(struct vm86_struct *info)
{
  int rc = prejit_vm86(info);
  if (rc)
    return rc + PREJIT_RV_OFFS;
  return true_kvm_vm86(info);
}

int kvm_dpmi(cpuctx_t *scp)
{
  int rc = prejit_dpmi(scp);
  if (rc)
    return rc + PREJIT_RV_OFFS;
  return true_kvm_dpmi(scp);
}
#endif

static void load_fpu_state(void)
{
  int i;
  struct emu_fsave fs;

  if (!CONFIG_CPUSIM)
    return;
  fxsave_to_fsave(&vm86_fpu_state, &fs);
  TheCPU.fpstt = 0;
  for (i = 0; i < 8; i++) {
#ifdef HAVE___FLOAT80
    /* copy only 10 bytes out of 12, so memset first */
    memset(&TheCPU.fpregs[i], 0, sizeof(TheCPU.fpregs[i]));
    memcpy(&TheCPU.fpregs[i], &fs.st[i], sizeof(fs.st[i]));
#else
    float_status s;
    union { uint32_t u32[sizeof(floatx80)/4]; floatx80 ld; } x;
    union { float128 f; _Float128 ld; } y;

    memcpy(x.u32, &fs.st[i], sizeof(fs.st[i]));
    y.f = floatx80_to_float128(x.ld, &s);
    TheCPU.fpregs[i] = y.ld;
#endif
  }
  TheCPU.fpuc = fs.cw;
  TheCPU.fpus = fs.sw;
  TheCPU.fptag = fs.tag;
}

static void save_fpu_state(void)
{
  int i, k;
  struct emu_fsave fs = {};

  if (!CONFIG_CPUSIM)
    return;
  k = TheCPU.fpstt;
  for (i = 0; i < 8; i++) {
#ifdef HAVE___FLOAT80
    /* copy only 10 bytes out of 12 */
    memcpy(&fs.st[i], &TheCPU.fpregs[k], sizeof(fs.st[i]));
#else
    float_status s;
    union { uint32_t u32[sizeof(floatx80)/4]; floatx80 ld; } x;
    union { float128 f; _Float128 ld; } y = { .ld = TheCPU.fpregs[k] };

    x.ld = float128_to_floatx80(y.f, &s);
    memcpy(&fs.st[i], x.u32, sizeof(fs.st[i]));
#endif
    k = (k + 1) & 7;
  }
  fs.cw = TheCPU.fpuc;
  fs.sw = TheCPU.fpus;
  fs.tag = TheCPU.fptag;
  fsave_to_fxsave(&fs, &vm86_fpu_state);
}

static void do_cpuemu_enter(int pm)
{
  load_fpu_state();
}

void cpuemu_enter(int pm)
{
  int need_inv = 0;
  /* Note: KVM uses dirty logging to invalidate lowmem protections. */
  if (pm) {
    if (config.cpu_vm == CPUVM_VM86)
      need_inv++;
  } else {
    if (config.cpu_vm_dpmi == CPUVM_NATIVE)
      need_inv++;
  }
  if (need_inv)
    e_invalidate_dirty(0, LOWMEM_SIZE + HMASIZE);
  do_cpuemu_enter(pm);
}

void cpuemu_leave(int pm)
{
  if (CEmuStat & CeS_INSTREMUx(pm))
    instr_sim_leave(pm);
  save_fpu_state();
}

void cpuemu_update_fpu(void)
{
  load_fpu_state();
}

/* set special SIM mode for VGAEMU faults */
void instr_emu_sim(cpuctx_t *scp, int pmode)
{
  int be = (pmode ? config.cpu_vm_dpmi : config.cpu_vm);
  assert(!(CEmuStat & CeS_INSTREMU));
  if (be == CPUVM_KVM) {
    /* kvm_leave() invalidates lowmem cpuemu pages so lock */
    prejit_lock();
    kvm_leave(pmode);
    prejit_unlock();
  }
  CEmuStat |= CeS_INSTREMUx(pmode);
  if (pmode) {
    prejit_lock();
    e_invalidate_dirty_full();
    prejit_unlock();
  }
  instr_emu_sim_reset_count();
  do_cpuemu_enter(pmode);
}

static void instr_sim_leave(int pmode)
{
  int be = (pmode ? config.cpu_vm_dpmi : config.cpu_vm);
  assert(CEmuStat & CeS_INSTREMUx(pmode));
  CEmuStat &= ~CeS_INSTREMUx(pmode);
  cpuemu_leave(pmode);
  if (be == CPUVM_KVM)
    kvm_enter(pmode);
}

/* ======================================================================= */
/* file: src/cwsdpmi/exphdlr.c */

void e_dpmi_b0x(int op,cpuctx_t *scp)
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


int e_debug_check(unsigned int PC)
{
    unsigned long d7 = TheCPU.dr[7];

    if (d7&0x03) {
	if (d7&0x30000) return 0;	/* only execute(00) bkp */
	if (PC==TheCPU.dr[0]) {
	    e_printf("DBRK: DR0 hit at %08x\n",PC);
	    TheCPU.dr[6] |= 1;
	    return 1;
	}
    }
    if (d7&0x0c) {
	if (d7&0x300000) return 0;
	if (PC==TheCPU.dr[1]) {
	    e_printf("DBRK: DR1 hit at %08x\n",PC);
	    TheCPU.dr[6] |= 2;
	    return 1;
	}
    }
    if (d7&0x30) {
	if (d7&0x3000000) return 0;
	if (PC==TheCPU.dr[2]) {
	    e_printf("DBRK: DR2 hit at %08x\n",PC);
	    TheCPU.dr[6] |= 4;
	    return 1;
	}
    }
    if (d7&0xc0) {
	if (d7&0x30000000) return 0;
	if (PC==TheCPU.dr[3]) {
	    e_printf("DBRK: DR3 hit at %08x\n",PC);
	    TheCPU.dr[6] |= 8;
	    return 1;
	}
    }
    return 0;
}

int e_in_compiled_code(void)
{
    return InCompiledCode;
}

int in_emu_cpu(void)
{
    return (in_dpmi_emu || in_vm86_emu);
}

int EMU_V86(void)
{
    return (config.cpu_vm == CPUVM_EMU ||
	(CEmuStat & (CeS_INSTREMU_RM | CeS_PREJIT_RM)));
}

int EMU_DPMI(void)
{
    return (config.cpu_vm_dpmi == CPUVM_EMU ||
	(CEmuStat & (CeS_INSTREMU_PM | CeS_PREJIT_PM)));
}

int _CPU_VM(void)
{
    if (EMU_V86())
	return CPUVM_EMU;
    return config.cpu_vm;
}

int _CPU_VM_DPMI(void)
{
    if (EMU_DPMI())
	return CPUVM_EMU;
    return config.cpu_vm_dpmi;
}

static int lockcnt;

void prejit_lock(void)
{
    prejit_sync();
    /* No actual locking: just wait til prejit stops. */
    pthread_mutex_lock(&run_mtx);
    lockcnt++;
    while (prejit_running)
	cond_wait(&run_cnd, &run_mtx);
    pthread_mutex_unlock(&run_mtx);
}

void prejit_unlock(void)
{
    pthread_mutex_lock(&run_mtx);
    assert(lockcnt > 0);
    lockcnt--;
    pthread_mutex_unlock(&run_mtx);
}

/* ======================================================================= */

#endif	/* X86_EMULATOR */
