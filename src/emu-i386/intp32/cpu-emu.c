/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
    CPU-EMU a Intel 80x86 cpu emulator
    Copyright (C) 1997,1998 Alberto Vignani, FIAT Research Center

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


Additional copyright notes:

 1. The emulator uses (modified) parts of the Twin library from
    Willows Software, Inc.
    ( For more information on Twin see http://www.willows.com )

 2. The kernel-level vm86 handling was taken out of the Linux kernel
    (linux/arch/i386/kernel/vm86.c). This code originaly was written by
    Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.

*/


#include "config.h"

#ifdef X86_EMULATOR
#include <stdlib.h>
#include <setjmp.h>
#include "emu.h"
#include "timers.h"
#include "pic.h"
#define DOSEMU_TYPESONLY	/* needed by the following: */
#include "hsw_interp.h"
#include "cpu-emu.h"
#include "dpmi.h"

#undef NEVER
#define NEVER	(0x7fffffffffffffffULL)

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

hitimer_t EMUtime = 0;
extern hitimer_u ZeroTimeBase;

static hitimer_t sigEMUtime = 0;
static hitimer_t lastEMUsig = 0;
static unsigned long sigEMUdelta = 0;
static int last_emuretrace;
static int in_e_dpmi = 0;
int    CEmuStat = 0;

Interp_ENV dosemu_env;
Interp_ENV *envp_global; /* the current global pointer to the x86 environment */

DSCR baseldt[8] =
{
	{ NULL, 0, 0,
#ifndef TWIN32
	 0,
#endif
	 0, 0, 0 }
};

long instr_count;
int emu_dpmi_retcode = -1;
int emu_under_X = 0;

#ifdef EMU_STAT
long InsFreq[256];
#ifdef EMU_PROFILE
long long InsTimes[256];
#endif
#endif
/*
 * --------------------------------------------------------------
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

unsigned long eVEFLAGS;
unsigned long eTSSMASK = 0;
#define VFLAGS	(*(unsigned short *)&eVEFLAGS)

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


/* ======================================================================= */
/*
 * Register movements between the dosemu REGS and the emulator ENV
 */
static void Reg2Env (struct vm86plus_struct *info, Interp_ENV *env)
{
  env->rax.e   = info->regs.eax;
  env->rbx.e   = info->regs.ebx;
  env->rcx.e   = info->regs.ecx;
  env->rdx.e   = info->regs.edx;
  env->rsi.esi = info->regs.esi;
  env->rdi.edi = info->regs.edi;
  env->rbp.ebp = info->regs.ebp;
  env->rsp.esp = info->regs.esp;
  env->cs.cs   = info->regs.cs;
  env->ds.ds   = info->regs.ds;
  env->es.es   = info->regs.es;
  env->ss.ss   = info->regs.ss;
  env->fs.fs   = info->regs.fs;
  env->gs.gs   = info->regs.gs;
  env->trans_addr = (info->regs.cs<<16 | (info->regs.eip&0xffff));

  if (config.cpuemu>2) {
  /* a vm86 call switch has been detected. Save all the flags from
   * the real CPU into the emulator state, less the reserved bits */
    env->flags = (info->regs.eflags&0x3f77d5) | 0x20002;
    config.cpuemu=2;
  }
  else {
  /*
   * the flags calculated by the emulator can be a bit different from
   * those of a real CPU, because flags processing is done in a 'lazy'
   * way. Restore BYTE_FLAG(0x8000) from the previous emu flags.
   */
    env->flags = ((env->flags&0x8000) | (info->regs.eflags&0x3d7fd5)) | 0x20002;
  }
  if (d.emu>1) e_printf("EMU86: REG(eflags)=%08lx -> env.flags=%08lx\n",
  	info->regs.eflags, env->flags);
}

static void Env2Reg (Interp_ENV *env, struct vm86plus_struct *info)
{
  info->regs.eax = env->rax.e;
  info->regs.ebx = env->rbx.e;
  info->regs.ecx = env->rcx.e;
  info->regs.edx = env->rdx.e;
  info->regs.esi = env->rsi.esi;
  info->regs.edi = env->rdi.edi;
  info->regs.ebp = env->rbp.ebp;
  info->regs.esp = env->rsp.esp;
  info->regs.ds  = env->ds.ds;
  info->regs.es  = env->es.es;
  info->regs.ss  = env->ss.ss;
  info->regs.fs  = env->fs.fs;
  info->regs.gs  = env->gs.gs;
  info->regs.cs  = env->return_addr >> 16;
  info->regs.eip = env->return_addr & 0xffff;
  /*
   * The emulator only processes the low 16 bits of eflags. OF,PF and
   * AF are not passed back, as dosemu doesn't care about them. RF is
   * forced only for compatibility with the kernel syscall; it is not used.
   * VIP and VIF are reflected to the emu flags but belong to the eflags.
   * BYTE_FLAG(0x8000) remains in the emu flags and is not passed back.
   */
  info->regs.eflags = (info->regs.eflags & (VIP|VIF)) |
  		      (env->flags & 0x247fd5) | 0x10002;  /* VM off */
}

static void Scp2Env (struct sigcontext_struct *scp, Interp_ENV *env)
{
  env->rax.e   = scp->eax;
  env->rbx.e   = scp->ebx;
  env->rcx.e   = scp->ecx;
  env->rdx.e   = scp->edx;
  env->rsi.esi = scp->esi;
  env->rdi.edi = scp->edi;
  env->rbp.ebp = scp->ebp;
  env->rsp.esp = scp->esp;
  env->cs.cs   = scp->cs;
  env->ds.ds   = scp->ds;
  env->es.es   = scp->es;
  env->ss.ss   = scp->ss;
  env->fs.fs   = scp->fs;
  env->gs.gs   = scp->gs;
  env->error_addr = 0;

  if (in_dpmi) {
    env->flags = ((env->flags&0x8000) | (scp->eflags&0x3d7fd5)) | 2;
    env->trans_addr = scp->eip;
  }
  else {
    env->flags = ((env->flags&0x8000) | (scp->eflags&0x3d7fd5)) | 0x20002;
    env->trans_addr = ((scp->cs<<16) + scp->eip);
/*
    if (d.emu>1) e_printf("EMU86: scp.eflags=%08lx -> env.flags=%08lx\n",
  	scp->eflags, env->flags);
*/
  }
}

#if 0
static void Scp2Reg (struct sigcontext_struct *scp,
		struct vm86plus_struct *info)
{
  info->regs.eax = scp->eax;
  info->regs.ebx = scp->ebx;
  info->regs.ecx = scp->ecx;
  info->regs.edx = scp->edx;
  info->regs.esi = scp->esi;
  info->regs.edi = scp->edi;
  info->regs.ebp = scp->ebp;
  info->regs.esp = scp->esp;
  info->regs.cs  = scp->cs;
  info->regs.ds  = scp->ds;
  info->regs.es  = scp->es;
  info->regs.ss  = scp->ss;
  info->regs.fs  = scp->fs;
  info->regs.gs  = scp->gs;
  info->regs.eip = scp->eip;
  info->regs.eflags = (info->regs.eflags & (VIP|VIF)) |
		(scp->eflags&0x247fd5) | 0x10002;  /* VM off */
}
#endif

static void Env2Scp (Interp_ENV *env, struct sigcontext_struct *scp,
	int trapno)
{
  scp->eax = env->rax.e;
  scp->ebx = env->rbx.e;
  scp->ecx = env->rcx.e;
  scp->edx = env->rdx.e;
  scp->esi = env->rsi.esi;
  scp->edi = env->rdi.edi;
  scp->ebp = env->rbp.ebp;
  scp->esp = env->rsp.esp;
  scp->ds  = env->ds.ds;
  scp->es  = env->es.es;
  scp->ss  = env->ss.ss;
  scp->fs  = env->fs.fs;
  scp->gs  = env->gs.gs;
  scp->trapno = trapno;
  /* Error code format:
   * b31-b16 = 0 (undef)
   * b15-b0  = selector, where b2=LDT/GDT, b1=IDT, b0=EXT
   * (b0-b1 are currently unimplemented here)
   */
  scp->err = env->error_addr;

  if (in_dpmi) {
    scp->cs = env->cs.cs;
    scp->eip = env->return_addr;
    scp->eflags = (dpmi_eflags & (VIP|VIF)) |
  		      (env->flags & 0x247fd5) | 0x10202;
  }
  else {
    scp->cs  = env->return_addr >> 16;
    scp->eip = env->return_addr & 0xffff;
    scp->eflags = (vm86s.regs.eflags & (VIP|VIF)) |
  		      (env->flags & 0x247fd5) | 0x30002;
  }
}

/* ======================================================================= */


static void emu_DPMI_show_state (struct sigcontext_struct *scp)
{
    e_printf("----------------------------------------------\n");
    e_printf("eip:%08lx  esp:%08lx  eflags:%08lx\n"
	     "           trapno:%02lx  errorcode:%08lx  cr2: %08lx\n"
	     "           cs:%04x  ds:%04x  es:%04x  ss:%04x  fs:%04x  gs:%04x\n",
	     _eip, _esp, _eflags, _trapno, _err, _cr2, _cs, _ds, _es, _ss, _fs, _gs);
    e_printf("EAX:%08lx  EBX:%08lx  ECX:%08lx  EDX:%08lx\n",
	     _eax, _ebx, _ecx, _edx);
    e_printf("ESI:%08lx  EDI:%08lx  EBP:%08lx  ESP:%08lx\n",
	     _esi, _edi, _ebp, _esp);
    e_printf("----------------------------------------------\n");
}


/* ======================================================================= */

void init_cpu (void)
{
  extern void init_npu(void);

#ifdef EMU_STAT
  memset(InsFreq,0,sizeof(InsFreq));
#endif
  memset(&dosemu_env, 0, sizeof(Interp_ENV));
  envp_global = &dosemu_env;

  envp_global->flags = IF;
  Reg2Env(&vm86s,envp_global);

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
  init_npu();
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

	if (!in_vm86 && !in_e_dpmi) {
	    EMUtime = GETusTIME(0)*config.emuspeed;
	}
	if (EMUtime >= sigEMUtime) {
		lastEMUsig = EMUtime;
		sigEMUtime += sigEMUdelta;
		/* we can't call sigalrm() because of the way the
		 * context parameter is passed. */
		e_sigalrm(&context);	/* -> signal_save */
	}
	/* here we return back to dosemu */
}

/**/static void e_sigxcpu(int sig, struct sigcontext_struct context)
/**/{
/**/	d.emu=4;
/**/}

void enter_cpu_emu(void)
{
	struct sigaction sa;

	if (config.rdtsc==0) {
	  fprintf(stderr,"Cannot execute CPUEMU without TSC counter\n");
	  leavedos(0);
	}
	config.cpuemu=3;	/* for saving CPU flags */
	emu_dpmi_retcode = -1;
#ifdef X_SUPPORT
	emu_under_X = (config.X != 0);
#endif
#ifdef SKIP_EMU_VBIOS
	if ((ISEG(0x10)==INT10_WATCHER_SEG)&&(IOFF(0x10)==INT10_WATCHER_OFF))
		IOFF(0x10)=CPUEMU_WATCHER_OFF;
#endif
	e_printf("EMU86: turning emuretrace ON\n");
	last_emuretrace = config.emuretrace;
	config.emuretrace = 2;

	e_printf("EMU86: switching SIGALRMs\n");
	EMUtime = GETusTIME(0)*config.emuspeed;
	sigEMUdelta = config.realdelta*config.emuspeed;
	sigEMUtime = EMUtime + sigEMUdelta;
	e_printf("EMU86: base time=%Ld delta alrm=%ld speed=%d\n",EMUtime,
		sigEMUdelta, config.emuspeed);
	SETSIG(SIG_TIME, e_gen_sigalrm);
/**/	SETSIG(SIGXCPU, e_sigxcpu);
	flush_log();
	dbug_printf("======================= ENTER CPU-EMU ===============\n");
}

void leave_cpu_emu(void)
{
	struct sigaction sa;
#ifdef EMU_STAT
	struct _eops {
	  unsigned char op;
	  unsigned long n;
#ifdef EMU_PROFILE
	  long long time;
#endif
	} eops[256];
	int i;
	int ecmp(const void *a, const void *b)
	  { return ((struct _eops *)b)->n - ((struct _eops *)a)->n; }
#endif
	config.cpuemu=1;
#ifdef SKIP_EMU_VBIOS
	if (IOFF(0x10)==CPUEMU_WATCHER_OFF)
		IOFF(0x10)=INT10_WATCHER_OFF;
#endif
	e_printf("EMU86: turning emuretrace OFF\n");
	config.emuretrace = last_emuretrace;

	e_printf("EMU86: switching SIGALRMs\n");
	NEWSETQSIG(SIG_TIME, sigalrm);
/**/	SETSIG(SIGXCPU, SIG_IGN);
	dbug_printf("======================= LEAVE CPU-EMU ===============\n");

#ifdef EMU_STAT
	if (d.emu==0) d.emu=1;
	for (i=0; i<256; i++) {
	  eops[i].op=i; eops[i].n=InsFreq[i];
#ifdef EMU_PROFILE
	  eops[i].time = (InsFreq[i]? InsTimes[i]/InsFreq[i] : 0);
#endif
	}
	qsort(eops,256,sizeof(struct _eops),ecmp);
	{ int j=0;
	  for (i=0; i<64; i++) {
	    e_printf(" [%02x]%10ld [%02x]%10ld [%02x]%10ld [%02x]%10ld\n",
		eops[j].op,eops[j].n,eops[j+1].op,eops[j+1].n,
		eops[j+2].op,eops[j+2].n,eops[j+3].op,eops[j+3].n);
#ifdef EMU_PROFILE
	    e_printf("   %12Ld   %12Ld   %12Ld   %12Ld\n",
		eops[j].time,eops[j+1].time,eops[j+2].time,eops[j+3].time);
#endif
	    j += 4;
	  }
	}
#endif
	flush_log();
}

/* =======================================================================
 * kernel-level vm86 handling
 * from /linux/arch/i386/kernel/vm86.c
 * original code by Linus Torvalds and later enhancements by
 * Lutz Molgedey and Hans Lermen.
 * adapted to the dosemu/willows CPU emulator by AV 9/1997-12/1998
 * all added bugs are (C) AV 1997/98 :-)
 */
static inline int e_set_IF(void)
{
	eVEFLAGS |= VIF_MASK;
	if (eVEFLAGS & VIP_MASK) return VM86_STI;
	return -1;
}

static inline void e_clear_IF(void)
{
	eVEFLAGS &= ~VIF_MASK;
}

static inline void e_clear_TF(void)
{
	REG(eflags) &= ~TF_MASK;
}

static inline int e_set_vflags_long(unsigned long eflags)
{
	e_set_flags(eVEFLAGS, eflags, eTSSMASK);
	e_set_flags(REG(eflags), eflags, SAFE_MASK);
	if (eflags & IF_MASK) return e_set_IF();
	return -1;
}

static inline int e_set_vflags_short(unsigned short flags)
{
	e_set_flags(VFLAGS, flags, eTSSMASK);
	e_set_flags(REG(eflags), flags, SAFE_MASK);
	if (flags & IF_MASK) return e_set_IF();
	return -1;
}

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
	e_clear_TF();
	e_clear_IF();
	if ((i!=0x16)&&((d.emu>1)||(d.dos)))
		dbug_printf("EMU86: directly calling int %#x ax=%#x at %#x:%#x\n", i, _AX, _CS, _IP);
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
	int e;

#define VM86_FAULT_RETURN \
	if (vm86s.vm86plus.force_return_for_pic  && (eVEFLAGS & IF_MASK)) { \
		return VM86_PICRETURN; } \
	if (CEmuStat & (CeS_SIGPEND|CeS_SIGACT)) \
		{ CEmuStat &= ~(CeS_SIGPEND|CeS_SIGACT); return VM86_SIGNAL; } \
	return -1;
	                                   
	csp = (unsigned char *) (_CS << 4);
	ssp = (unsigned char *) (_SS << 4);
	sp = _SP;
	ip = _IP;
	op = popb(csp, ip);
	if (d.emu>1) e_printf("EMU86: vm86 fault %#x at %#x:%#x cnt=%ld\n",
		op, _CS, _IP, instr_count);

	switch (op) {

	/* operand size override */
	case 0x66:
		switch (popb(csp, ip)) {

		/* pushfd */
		case 0x9c:
			_SP -= 4;
			_IP += 2;
			pushl(ssp, sp, e_get_vflags());
			VM86_FAULT_RETURN;

		/* popfd */
		case 0x9d:
			_SP += 4;
			_IP += 2;
			if ((e=e_set_vflags_long(popl(ssp, sp))) >= 0)
				return e;
			VM86_FAULT_RETURN;

		/* iretd */
		case 0xcf:
			_SP += 12;
			_IP = (unsigned short)popl(ssp, sp);
			_CS = (unsigned short)popl(ssp, sp);
			if ((e=e_set_vflags_long(popl(ssp, sp))) >= 0)
				return e;
			VM86_FAULT_RETURN;
		/* need this to avoid a fallthrough */
		default:
			return VM86_UNKNOWN;
		}

	/* pushf */
	case 0x9c:
		_SP -= 2;
		_IP++;
		pushw(ssp, sp, e_get_vflags());
		VM86_FAULT_RETURN;

	/* popf */
	case 0x9d:
		_SP += 2;
		_IP++;
		if ((e=e_set_vflags_short(popw(ssp, sp))) >= 0)
			return e;
		VM86_FAULT_RETURN;

	/* int xx */
	case 0xcd: {
	        int intno=popb(csp, ip);
		_IP += 2;
		if (vm86s.vm86plus.vm86dbg_active) {
			if ( (1 << (intno &7)) & vm86s.vm86plus.vm86dbg_intxxtab[intno >> 3] ) {
				return (VM86_INTx + (intno << 8));
			}
		}
		return e_do_int(intno, ssp, sp);
	}

	/* iret */
	case 0xcf:
		_SP += 6;
		_IP = popw(ssp, sp);
		_CS = popw(ssp, sp);
		if ((e=e_set_vflags_short(popw(ssp, sp))) >= 0)
			return e;
		VM86_FAULT_RETURN;
	/* cli */
	case 0xfa:
		_IP++;
		e_clear_IF();
		VM86_FAULT_RETURN;

	/* sti */
	/*
	 * Damn. This is incorrect: the 'sti' instruction should actually
	 * enable interrupts after the /next/ instruction. Not good.
	 *
	 * Probably needs some horsing around with the TF flag. Aiee..
	 */
	case 0xfb:
		_IP++;
		if ((e=e_set_IF()) >= 0) return e;
		VM86_FAULT_RETURN;

	default:
		return VM86_UNKNOWN;
	}
}

/*
 * =======================================================================
 */
extern int invoke_code16(Interp_ENV *, int, int);
extern int invoke_code32(Interp_ENV *, int);

static char *retdescs[] =
{
	"VM86_SIGNAL","VM86_UNKNOWN","VM86_INTx","VM86_STI",
	"VM86_PICRET","???","VM86_TRAP","???"
};

int e_vm86(void)
{
  hitimer_t entrytime;
  long long detime;
  int xval,retval;
  long errcode, totalcyc;
  unsigned long pmflags;

  /* skip emulation of video BIOS, as it is too much timing-dependent */
  if ((config.cpuemu<2)
#ifdef SKIP_EMU_VBIOS
   || ((REG(cs)&0xf000)==config.vbios_seg)
#endif
   ) {
	return TRUE_VM86(&vm86s);
  }

  /* get the protected mode flags. Note that RF and VM are cleared
   * by pushfd (but not by ints and traps) */
  __asm__ __volatile__ ("
	pushfl
	popl	%0"
	: "=m"(pmflags)
	:
	: "memory");

  detime = 0;
  totalcyc = 0;
  EMUtime = (entrytime=GETusTIME(0))*config.emuspeed;	/* stretched time */
  if (lastEMUsig && (d.emu>1))
    e_printf("EMU86: last sig at %Ld, curr=%Ld, next=%Ld\n",lastEMUsig>>16,
    	EMUtime>>16,sigEMUtime>>16);

 /* This emulates VM86_ENTER only */
 /*
  * The eflags register is also special: we cannot trust that the user
  * has set it up safely, so this makes sure interrupt etc flags are
  * inherited from protected mode.
  */
  eVEFLAGS = REG(eflags);
  REG(eflags) = (REG(eflags) & SAFE_MASK) | (pmflags & notSAFE_MASK) | VM;

  /* ------ OUTER LOOP: exit for code >=0 and return to dosemu code */
  do {
    Reg2Env (&vm86s, envp_global);
    instr_count = 0;

    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
    do {
      long icyc;
      /* enter VM86 mode */
      in_vm86_emu = 1;
      xval = invoke_code16(envp_global, 1, -1);
      in_vm86_emu = 0;
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("EMU86: error %d\n", -xval);
        in_vm86=0;
        leavedos(0);
      }
      envp_global->trans_addr = envp_global->return_addr;
      icyc = instr_count * CYCperINS;
      totalcyc += icyc;
    }
    while (xval==0);
    /* ---- INNER LOOP -- exit for exception ---------------------- */

    if (d.emu>1) e_printf("EMU86: EXCP %#x cyc=%ld env.flags %08lx, REG(eflags) %08lx\n",
	xval-1, totalcyc, envp_global->flags, REG(eflags));

    Env2Reg (envp_global, &vm86s);
    retval = -1;

    if (xval==EXCP_SIGNAL) {	/* coming here for async interruptions */
	if (CEmuStat & (CeS_SIGPEND|CeS_SIGACT))
		{ CEmuStat &= ~(CeS_SIGPEND|CeS_SIGACT); retval=VM86_SIGNAL; } \
    }
    else if (xval!=EXCP_GOBACK) {
	/*
	 * this is very bad - what we calculate here is the time
	 * spent in the emulator PLUS the time spent in other
	 * processes and system activities unrelated to dosemu!!
	 */
	detime = (GETusTIME(0) - entrytime);	/* real us elapsed */
	detime -= (totalcyc / config.emuspeed); if (detime < 0) detime = 0;
	totalcyc = 0;
	ZeroTimeBase.td += detime;	/* stretch system and emu time */
#ifdef DBG_TIME
	if (d.emu>1) e_printf("\tSHIFT base time by %Ld us\n", detime);
#endif
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
		Env2Scp (envp_global, &scp, xval-1);
		/* we go out of vm86 here */
		dosemu_fault1(xval-1, &scp);
		Scp2Env (&scp, envp_global);
		in_vm86 = 1;
		retval = -1;	/* reenter vm86 emu */
	    }
	}
	if (retval < 0) {
	   EMUtime = (entrytime=GETusTIME(0))*config.emuspeed;
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
  e_set_flags(REG(eflags), eVEFLAGS, VIF_MASK | eTSSMASK);

  e_printf("EMU86: retval=%s\n", retdescs[retval&7]);

  if (totalcyc) {
    detime = (GETusTIME(0) - entrytime);
    detime -= (totalcyc / config.emuspeed); if (detime < 0) detime = 0;
    ZeroTimeBase.td += detime;	/* stretch system and emu time */
#ifdef DBG_TIME
    if (d.emu>1) e_printf("\tdTIME = %Ld cyc\n", detime);
#endif
  }

  return retval;
}

/* ======================================================================= */


int e_dpmi(struct sigcontext_struct *scp)
{
  hitimer_t entrytime;
  long long detime;
  int xval,retval;
  long totalcyc;

  in_e_dpmi = 1;
  detime = 0;
  totalcyc = 0;
  EMUtime = (entrytime=GETusTIME(0))*config.emuspeed;	/* stretched time */

  if (d.dpmi>6) {
	long swa = (long)(LDTgetSelBase(_cs)+_eip);
	D_printf("DPMI SWITCH to %08lx\n",swa);
  }
  if (d.emu>8) emu_DPMI_show_state(scp);
  if (lastEMUsig)
    e_printf("DPM86: last sig at %Ld, curr=%Ld, next=%Ld\n",lastEMUsig>>16,
    	EMUtime>>16,sigEMUtime>>16);

  /* ------ OUTER LOOP: exit for code >=0 and return to dosemu code */
  do {
    Scp2Env (scp, envp_global);
    instr_count = 0;

    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
    do {
      long icyc;
      /* switch to DPMI process */
      in_dpmi_emu = 1;
      CEmuStat &= ~CeS_MODE_MASK;
      xval = (LDT[scp->cs>>3].w86Flags & DF_32 ?
      	(CEmuStat|=MODE_DPMI16, invoke_code32(envp_global, -1)) :
      	(CEmuStat|=MODE_DPMI32, invoke_code16(envp_global, 0, -1))
      );
      in_dpmi_emu = 0;
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("DPM86: error %d\n", -xval);
        leavedos(0);
      }
      envp_global->trans_addr = envp_global->return_addr;
      icyc = instr_count * CYCperINS;
      totalcyc += icyc;
    }
    while (xval==0);
    /* ---- INNER LOOP -- exit for exception ---------------------- */

    if (d.emu>1) e_printf("DPM86: EXCP %#x cyc=%ld eflags=%08lx\n",
	xval-1, totalcyc, REG(eflags));

    envp_global->flags &= ~TF;	/* is it right? */
    Env2Scp (envp_global, scp, xval-1);
    retval = -1;

    if (xval==EXCP_SIGNAL) {
	if (emu_dpmi_retcode >= 0) {
	    retval=emu_dpmi_retcode; emu_dpmi_retcode = -1;
	}
    }
    else if (xval!=EXCP_GOBACK) {
	/*
	 * this is very bad - what we calculate here is the time
	 * spent in the emulator PLUS the time spent in other
	 * processes and system activities unrelated to dosemu!!
	 */
	detime = (GETusTIME(0) - entrytime);	/* real us elapsed */
	detime -= (totalcyc / config.emuspeed); if (detime < 0) detime = 0;
	totalcyc = 0;
	ZeroTimeBase.td += detime;	/* stretch system and emu time */
#ifdef DBG_TIME
	if (d.emu>1) e_printf("\tSHIFT base time by %Ld us\n", detime);
#endif
	dpmi_fault(scp);
	if (emu_dpmi_retcode >= 0) {
		retval=emu_dpmi_retcode; emu_dpmi_retcode = -1;
	}
	else switch (scp->trapno) {
	  case 0x6c: case 0x6d: case 0x6e: case 0x6f:
	  case 0xe4: case 0xe5: case 0xe6: case 0xe7:
	  case 0xec: case 0xed: case 0xee: case 0xef:
	  case 0xfa: case 0xfb: retval = -1; break;
	  default: retval = 0; break;
	}
	scp->trapno = 0;
	if (retval < 0) {
	    EMUtime = (entrytime=GETusTIME(0))*config.emuspeed;
	}
    }
  }
  while (retval < 0);
  /* ------ OUTER LOOP -- exit to user level ---------------------- */

  e_printf("DPM86: retval=%#x\n", retval);

  if (totalcyc) {
    /* should never come here */
    detime = (GETusTIME(0) - entrytime);
    detime -= (totalcyc / config.emuspeed); if (detime < 0) detime = 0;
    ZeroTimeBase.td += detime;	/* stretch system and emu time */
#ifdef DBG_TIME
    if (d.emu>1) e_printf("\tdTIME = %Ld cyc\n", detime);
#endif
  }
  in_e_dpmi = 0;
  return retval;
}


/* ======================================================================= */
/* file: src/cwsdpmi/exphdlr.c */

void e_dpmi_b0x(int op,struct sigcontext_struct *scp)
{
  switch (op) {
    case 0: {	/* set */
	int enabled = DRs[7];
	int i;
	for (i=0; i<4; i++) {
	  if ((~enabled >> (i*2)) & 3) {
	    unsigned mask;
	    DRs[i] = (_LWORD(ebx) << 16) | _LWORD(ecx);
	    DRs[7] |= (3 << (i*2));
	    mask = _HI(dx) & 3; if (mask==2) mask++;
	    mask |= ((_LO(dx)-1) << 2) & 0x0c;
	    DRs[7] |= mask << (i*4 + 16);
	    _LWORD(ebx) = i;
	    _eflags &= ~CF;
	    break;
	  }
	}
      }
      break;
    case 1: {	/* clear */
        int i = _LWORD(ebx) & 3;
        DRs[6] &= ~(1 << i);
        DRs[7] &= ~(3 << (i*2));
        DRs[7] &= ~(15 << (i*4+16));
	_eflags &= ~CF;
	break;
      }
    case 2: {	/* get */
        int i = _LWORD(ebx) & 3;
        _LWORD(eax) = (DRs[6] >> i);
	_eflags &= ~CF;
	break;
      }
    case 3: {	/* reset */
        int i = _LWORD(ebx) & 3;
        DRs[6] &= ~(1 << i);
	_eflags &= ~CF;
	break;
      }
    default:
	_LWORD(eax) = 0x8016;
	_eflags |= CF;
	break;
  }
  if (DRs[7] & 0xff) CEmuStat|=CeS_DRTRAP; else CEmuStat&=~CeS_DRTRAP;
/*
  e_printf("DR0=%08lx DR1=%08lx DR2=%08lx DR3=%08lx\n",DRs[0],DRs[1],DRs[2],DRs[3]);
  e_printf("DR6=%08lx DR7=%08lx\n",DRs[6],DRs[7]);
*/
}


int e_debug_check(unsigned char *PC)
{
    register unsigned long d7 = DRs[7];

    if (d7&0x03) {
	if (d7&0x30000) return 0;	/* only execute(00) bkp */
	if ((long)PC==DRs[0]) {
	    e_printf("DBRK: DR0 hit at %p\n",PC);
	    DRs[6] |= 1;
	    return 1;
	}
    }
    if (d7&0x0c) {
	if (d7&0x300000) return 0;
	if ((long)PC==DRs[1]) {
	    e_printf("DBRK: DR1 hit at %p\n",PC);
	    DRs[6] |= 2;
	    return 1;
	}
    }
    if (d7&0x30) {
	if (d7&0x3000000) return 0;
	if ((long)PC==DRs[2]) {
	    e_printf("DBRK: DR2 hit at %p\n",PC);
	    DRs[6] |= 4;
	    return 1;
	}
    }
    if (d7&0xc0) {
	if (d7&0x30000000) return 0;
	if ((long)PC==DRs[3]) {
	    e_printf("DBRK: DR3 hit at %p\n",PC);
	    DRs[6] |= 8;
	    return 1;
	}
    }
    return 0;
}


/* ======================================================================= */

#endif	/* X86_EMULATOR */
