/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
    CPU-EMU a Intel 80x86 cpu emulator
    Copyright (C) 1997 Alberto Vignani, FIAT Research Center

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

/* 4 cycles/instr is an empirical estimate of how my K6 system
 * really performs, including CPU, caches and main memory */
#define CYCperINS	4

#undef NEVER
#define NEVER	(0x7fffffffffffffffULL)

#define e_set_flags(X,new,mask) \
	((X) = ((X) & ~(mask)) | ((new) & (mask)))

#define SAFE_MASK	(0xDD5)
#define notSAFE_MASK	(~SAFE_MASK&0x3fffff)
#define RETURN_MASK	(0xDFF)

/* ======================================================================= */

hitimer_t EMUtime = 0;
extern hitimer_u ZeroTimeBase;

hitimer_t dbgEMUstart = 0;
int dbgEMUlevel = 0;
unsigned long dbgEMUbreak = 0;

static hitimer_t sigEMUtime = 0;
static unsigned long sigEMUdelta = 0;
int e_sig_pending = 0;

Interp_ENV dosemu_env;
extern Interp_ENV *envp_global;

long instr_count;

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

extern void e_sigalrm(void);

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
    env->flags   = info->regs.eflags&0x3f77f5;
    config.cpuemu=2;
  }
  else {
  /*
   * the flags calculated by the emulator can be quite different from
   * those of a real CPU, because flags processing is done in a
   * different way. As dosemu is not interested in PF,AF,ZF,SF flags,
   * keep the previous emulator values. Also, the emulator can use
   * reserved bits, keep them unchanged.
   */
    env->flags   = (env->flags&0x88fe) | (info->regs.eflags&0x3f7701);
  }
  if (d.emu>2) e_printf("EMU86: userflags %08lx -> cpuflags=%08lx\n",
  	info->regs.eflags, env->flags);
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
  env->trans_addr = ((scp->cs<<16) + scp->eip);

  if (config.cpuemu>2) {
    env->flags   = scp->eflags&0x3f77f5;
    config.cpuemu=2;
  }
  else {
    env->flags   = (env->flags&0x88fe) | (scp->eflags&0x3f7701);
  }
  if (d.emu>2) e_printf("EMU86: userflags %08lx -> cpuflags=%08lx\n",
  	scp->eflags, env->flags);
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
   */
  info->regs.eflags = (info->regs.eflags&0x180000) |
  		      (env->flags & 0x2677c1) | 0x10002;
}

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
  scp->cs  = env->return_addr >> 16;
  scp->eip = env->return_addr & 0xffff;
  scp->eflags = (vm86s.regs.eflags&0x180000) |	/* ???? */
  		      (env->flags & 0x2677c1) | 0x10002;
}

/* ======================================================================= */

void init_cpu (void)
{
  char *p;
  long v,v2;

  envp_global = &dosemu_env;
  memset (io_bitmap, 0, sizeof(io_bitmap));

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

  dbgEMUstart = NEVER;
  dbgEMUlevel = 3;
  dbgEMUbreak = 0;
  if ((p=getenv("EMU_START"))!=NULL) {
	if (sscanf(p,"%ld",&v)==1) {
		if (v>=0) dbgEMUstart=((hitimer_t)v*1000*config.emuspeed);
	}
  }
  if ((p=getenv("EMU_LEVEL"))!=NULL) {
	if (sscanf(p,"%ld",&v)==1) {
		if (v>=0) dbgEMUlevel=v;
	}
  }
  if ((p=getenv("EMU_BREAK"))!=NULL) {
	if (sscanf(p,"%lx:%lx",&v,&v2)==2) {
		dbgEMUbreak = (v<<16)|(v2&0xffff);
	}
  }
}

void enter_cpu_emu(void)
{
	struct itimerval itv;

	config.cpuemu=3;	/* for saving CPU flags */
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 0;
	setitimer(TIMER_TIME, &itv, NULL);
	dbug_printf("======================= ENTER CPU-EMU ===============\n");
}

void leave_cpu_emu(void)
{
	struct itimerval itv;

	config.cpuemu=1;
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = config.realdelta;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = config.realdelta;
	setitimer(TIMER_TIME, &itv, NULL);
	dbug_printf("======================= LEAVE CPU-EMU ===============\n");
}

/* =======================================================================
 * kernel-level vm86 handling
 * from /linux/arch/i386/kernel/vm86.c
 * original code by Linus Torvalds and later enhancements by
 * Lutz Molgedey and Hans Lermen.
 * adapted to the dosemu/willows CPU emulator by AV 9/1997
 * all added bugs are (C) AV 1997 :-)
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

/* emulates only SIGALRM; the other asynchronous signals are caught
 * as usual */
void e_gen_signals(void)
{
	if (!in_vm86) {
		/* this is called, e.g. from sleep() */
		EMUtime = GETusTIME(0)*config.emuspeed;
	}
	if ((long)(EMUtime - sigEMUtime) > sigEMUdelta) {
		sigEMUtime += sigEMUdelta;
		e_sigalrm();	/* -> signal_save */
	}
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
	if (e_sig_pending) { e_sig_pending=0; return VM86_SIGNAL; }
	return -1;

cannot_handle:
	if ((d.emu>1)||(d.dos))
		dbug_printf("EMU86: calling revectored int %#x\n", i);
	return (VM86_INTx + (i << 8));
}

static int handle_vm86_fault(long *error_code)
{
	unsigned char *csp, *ssp, op;
	unsigned long ip, sp;
	int e;

#define VM86_FAULT_RETURN \
	if (vm86s.vm86plus.force_return_for_pic  && (eVEFLAGS & IF_MASK)) { \
		return VM86_PICRETURN; } \
	if (d.emu>2) e_printf("EMU86: VE=%08lx F=%08lx\n",eVEFLAGS,REG(eflags)); \
	if (e_sig_pending) { e_sig_pending=0; return VM86_SIGNAL; } \
	return -1;
	                                   
	csp = (unsigned char *) (_CS << 4);
	ssp = (unsigned char *) (_SS << 4);
	sp = _SP;
	ip = _IP;
	op = popb(csp, ip);
	if (d.emu>1) e_printf("E_VM86: vm86 fault %#x at %#x:%#x cnt=%ld\n",
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
extern int invoke_code16(Interp_ENV *, int);

int e_vm86(void)
{
  static int first = 1;
  hitimer_t entrytime, detime;
  int xval,retval;
  long errcode, totalcyc;
  unsigned long pmflags;

  if (config.cpuemu<2) {
	first = 1;
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

  entrytime = GETusTIME(0);	/* real time at vm86 entry, in us */
  EMUtime = entrytime*config.emuspeed;
  if (first) {
	sigEMUtime = EMUtime;
	sigEMUdelta = config.realdelta*config.emuspeed;
	c_printf("EMU86: base time=%Ld delta alrm=%ld speed=%d\n",EMUtime,
		sigEMUdelta, config.emuspeed);
	first=0;
  }
  totalcyc = 0;

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

    /* ---- INNER LOOP: exit with error or code>0 (vm86 fault) ---- */
    do {
      if (dbgEMUlevel && ((EMUtime > dbgEMUstart) ||
	         (dbgEMUbreak==envp_global->trans_addr)))
      	d.emu=dbgEMUlevel;
      /* enter VM86 mode */
      instr_count = 0;
      xval = invoke_code16(envp_global, 1);
      /* 0 if ok, else exception code+1 or negative if dosemu err */
      if (xval < 0) {
        error("E_VM86: error %d\n", -xval);
        in_vm86=0;
        leavedos(0);
      }
      envp_global->trans_addr = envp_global->return_addr;
      EMUtime += (instr_count*CYCperINS);
      totalcyc += (instr_count*CYCperINS);
    }
    while (xval==0);
    /* ---- INNER LOOP -- exit for exception ---------------------- */

    if (d.emu>2) e_printf("EMU86: EXCP %#x cpuflags %08lx -> userflags=%08lx\n",
	xval-1, envp_global->flags, REG(eflags));

    if (xval==EXCP0D_GPF) {	/* to kernel vm86 */
        Env2Reg (envp_global, &vm86s);
	retval=handle_vm86_fault(&errcode);	/* kernel level */
	e_gen_signals();
    }
    else {	/* signal, to dosemu_fault() - this doesn't yet work */
	Env2Scp (envp_global, &e_scp, xval-1);
        dosemu_fault1 (xval-1, &e_scp);
	Scp2Env (&e_scp, envp_global);
        retval = -1;
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

  e_printf("EMU86: retval=%#x\n", retval);

  detime = GETusTIME(0)-entrytime;	/* delta real time at vm86 exit */
  ZeroTimeBase.td += (detime - (totalcyc/config.emuspeed));

  return retval;
}

/* ======================================================================= */

int e_dpmi(void)
{
  hitimer_t entrytime;

  if (config.cpuemu<2) {
	return 0;
  }

  entrytime = GETusTIME(0);	/* real time at vm86 entry, in us */
  EMUtime = entrytime*config.emuspeed;

  e_gen_signals();

  return 0;
}

/* ======================================================================= */

#endif	/* X86_EMULATOR */
