/* CPU/V86 support for dosemu
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu 
 *
 * $Date: 1993/05/04 05:29:22 $
 * $Source: /usr/src/dos/RCS/cpu.c,v $
 * $Revision: 1.4 $
 * $State: Exp $
 *
 * $Log: cpu.c,v $
 * Revision 1.4  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.3  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.2  1993/03/04  22:35:12  root
 * put in perfect shared memory, HMA and everything.  added PROPER_STI.
 *
 */
#define CPU_C

#include "config.h"
#include "memory.h"
#include "cpu.h"
#include "emu.h"

struct vm86_struct vm86s;
struct CPU cpu;

struct vec_t orig[256];		/* "original" interrupt vectors */
struct vec_t snapshot[256];	/* vectors from last snapshot */

extern int in_sigsegv, in_sighandler, ignore_segv, in_interrupt;
extern int fatalerr;

int in_vm86=0;

inline void update_cpu(long flags)
{
  cpu.iflag = (flags&IF)  ? 1 : 0;
  cpu.nt    = (flags&NT)  ? 1 : 0;
  cpu.ac    = (flags&AC)  ? 1 : 0;

  cpu.iopl  = flags&IOPL_MASK;
}


inline void update_flags(long *flags)
{
#define SETFLAG(bit)  (*flags |=  (bit))
#define CLRFLAG(bit)  (*flags &= ~(bit))

  cpu.iflag ? SETFLAG(IF)  : CLRFLAG(IF);
  (cpu.nt && (cpu.type!=CPU_286)) ? SETFLAG(NT)  : CLRFLAG(NT);

  /* on a 386, the AC (alignment check) bit isn't */
  if (cpu.ac && (cpu.type == CPU_486)) 
    SETFLAG(AC);
  else
    CLRFLAG(AC);

  *flags &= ~IOPL_MASK;
  if (cpu.type != CPU_286) *flags |= cpu.iopl;

  /* the 80286 allowed one to set bit 15 of the flags register,
   * (so says HelpPC 2.1...but INFOPLUS uses the test below)
   * 80286 in real mode keeps the upper 4 bits of FLAGS zeroed.  this
   * test works with the programs I've tried, while setting bit15 
   * does NOT.
   */
  if (cpu.type == CPU_286) *flags &= ~0xfffff000;

#undef SETFLAG
#undef CLRFLAG
}


void cpu_init(void)
{
  /* cpu.type set in emulate() via getopt */
  cpu.iflag=1;
  cpu.nt=0;
  cpu.iopl=0;
  cpu.ac=0;
  cpu.sti=1;

  update_flags(&_regs.eflags);

  /* make ivecs array point to low page (real mode IDT) */
  ivecs=0;
}



void show_regs(void)
{
  int i;
  unsigned char *cp = SEG_ADR((unsigned char *), cs, ip);
  unsigned long vflags=_regs.eflags;

  /* adjust vflags with our virtual IF bit (iflag) */
  update_flags(&vflags);
  
  dbug_printf("\nEIP: %04x:%08x",0xffff & _regs.cs,_regs.eip);
  dbug_printf(" ESP: %04x:%08x",0xffff & _regs.ss,_regs.esp);
  dbug_printf("         VFLAGS(b): ");
  for (i=(1 << 0x11); i > 0; i=(i >> 1))
      dbug_printf( (vflags&i) ? "1" : "0");

  dbug_printf("\nEAX: %08x EBX: %08x ECX: %08x EDX: %08x VFLAGS(h): %08x",
	      _regs.eax,_regs.ebx,_regs.ecx,_regs.edx, vflags);
  dbug_printf("\nESI: %08x EDI: %08x EBP: %08x",
	      _regs.esi, _regs.edi, _regs.ebp);
  dbug_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
	      0xffff & _regs.ds,0xffff & _regs.es,
	      0xffff & _regs.fs,0xffff & _regs.gs);

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (vflags&f) dbug_printf(#f" ")

  dbug_printf("FLAGS: ");
  PFLAG(CF);  PFLAG(PF);  PFLAG(AF);  PFLAG(ZF);  
  PFLAG(SF);  PFLAG(TF);  PFLAG(IF);  PFLAG(DF);  
  PFLAG(OF);  PFLAG(NT);  PFLAG(RF);  PFLAG(VM);
  PFLAG(AC); 
  dbug_printf(" IOPL: %d\n", (vflags&IOPL_MASK) >> 12);

#undef PFLAG

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  dbug_printf("OPS: ");
  cp -= 10; 
  for (i=0; i<10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("-> ");
  for (i=0; i<10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("\n");
}


void show_ints(int min, int max)
{
  int i, b;

  max = (max-min)/3;
  for (i=0, b=min; i<=max; i++, b+=3)
    {
      dbug_printf("%02x| %04x:%04x->%05x    ",b,ISEG(b), IOFF(b),
		  IVEC(b));
      dbug_printf("%02x| %04x:%04x->%05x    ",b+1,ISEG(b+1), IOFF(b+1),
		  IVEC(b+1));
      dbug_printf("%02x| %04x:%04x->%05x\n",b+2,ISEG(b+2),IOFF(b+2),
		  IVEC(b+2));
    }
}


inline int do_hard_int(int intno) {
#if AJT
  queue_hard_int(intno,NULL,NULL);
  return(1);
#else
  if (cpu.iflag) { do_int(intno); return 1; }
  else return 0;
#endif
}

inline int do_soft_int(int intno) {
  do_int(intno); 
  return 1;
}

inline void do_sti(void)
{
  cpu.sti=1;
  _regs.eflags|=TF;
}


void sigsegv(int sig)
{
  us *ssp;
  unsigned char *csp, *lina;
  static int haltcount=0;

  int op32=0, ad32=0;

#define MAX_HALT_COUNT 1

  if (ignore_segv)
    {
      error("ERROR: sigsegv ignored!\n");
      return;
    }

  if (! (_regs.eflags & VM)) /* test VM bit */
    {
      error("ERROR: NON-VM86 sigsegv!\n");
      show_regs();
    }

  if (in_sigsegv)
    error("ERROR: in_sigsegv=%d!\n", in_sigsegv);

  in_sigsegv++;
  
  /* In a properly functioning emulator :-), sigsegv's will never come
   * while in a non-reentrant system call (ioctl, select, etc).  Therefore,
   * there's really no reason to worry about them, so I say that I'm NOT
   * in a signal handler (I might make this a little clearer later, to
   * show that the purpose of in_sighandler is to stop non-reentrant system
   * calls from being reentered.
   * I reiterate: sigsegv's should only happen when I'm running the vm86
   * system call, so I really shouldn't be in a non-reentrant system call
   * (except maybe vm86)
   */
  in_sighandler=0;

  lina=(unsigned char *)((LWORD(cs)<<4) + LWORD(eip));

  if (_regs.eflags & TF) {
    g_printf("SIGSEGV %d received\n", sig);
    show_regs(); 
  }

restart_segv:
  csp = SEG_ADR((unsigned char *), cs, ip);

  switch (*csp) {
  case 0x62: /* bound */
    error("ERROR: BOUND instruction");
    show_regs();
    _regs.eip += 2;  /* check this! */
    do_int(5); 
    break;
  case 0x66: /* 32-bit operand prefix */
    op32=1;
    _regs.eip++;
    warn("CPU: 32-bit operand prefix!");
    show_regs();
    goto restart_segv;
    break;
  case 0x67: /* 32-bit address prefix */
    ad32=1;
    _regs.eip++;
    warn("CPU: 32-bit operand prefix!");
    show_regs();
    goto restart_segv;
    break;

  case 0x6c: /* insb */
  case 0x6d: /* insw */
  case 0x6e: /* outsb */
  case 0x6f: /* outsw */
    error("ERROR: IN/OUT SB/SW: 0x%02x\n",*csp);
    _regs.eip++;
    break;

  case 0xcd: /* int xx */
    _regs.eip += 2;
    do_int((int)*++csp);
    break;
  case 0xcc: /* int 3 */
    _regs.eip += 1;
    do_int(3);
    break;

  case 0xcf: /* iret */
    ssp = SEG_ADR((us *), ss, sp);
    _regs.eip = *ssp++;
    _regs.cs = *ssp++;
    _regs.eflags = (_regs.eflags & 0xffff0000) | *ssp++;
    _regs.esp += 6;

    /* make sure our "virtual flags" correspond to the
     * popped eflags
     */
    update_cpu(_regs.eflags);

    /* decrease the interrupt depth count */
    in_interrupt--;
    break;

  case 0xe5: /* inw xx */
    _regs.eax &= ~0xff00;
    _regs.eax |= inb((int)csp[1] +1) << 8;
  case 0xe4: /* inb xx */
    _regs.eax &= ~0xff;
    _regs.eax |= inb((int)csp[1]);
    _regs.eip += 2;
    break;

  case 0xed: /* inw dx */
    _regs.eax &= ~0xff00;
    _regs.eax |= inb(_regs.edx +1) << 8;
  case 0xec: /* inb dx */
    _regs.eax &= ~0xff;
    _regs.eax |= inb(_regs.edx);
    _regs.eip += 1;
    break;

  case 0xe7: /* outw xx */
    outb((int)csp[1] +1, HI(ax));
  case 0xe6: /* outb xx */
    outb((int)csp[1], LO(ax));
    _regs.eip += 2;
    break;

  case 0xef: /* outw dx */
    outb(_regs.edx +1, HI(ax));
  case 0xee: /* outb dx */
    outb(_regs.edx, LO(ax));
    _regs.eip += 1;
    break;

  case 0xfa: /* cli */
    cpu.iflag = 0;
    _regs.eip += 1;
    break;
  case 0xfb: /* sti */
#ifdef PROPER_STI
    do_sti();
#else
    cpu.iflag = 1;
#endif
    _regs.eip += 1;
    break;

  case 0x9c: /* pushf */
    /* fetch virtual CPU flags into eflags */
    update_flags(&_regs.eflags);
    if (op32)
    {
      unsigned long *lssp;
      warn("CPU: 32-bit pushf\n\n");
      op32=0;
      lssp = SEG_ADR((unsigned long *), ss, sp);
      *--lssp = (unsigned long)_regs.eflags;
      _regs.esp -= 4;
      _regs.eip += 1;
    } else {
      ssp = SEG_ADR((us *), ss, sp);
      *--ssp = (us)_regs.eflags;
      _regs.esp -= 2;
      _regs.eip += 1;
    }
    break;

  case 0x9d: /* popf */
    if (op32)
      {
	unsigned long *lssp;
	warn("CPU: 32-bit popf\n\n");
	op32=0;
	lssp = SEG_ADR((unsigned long *), ss, sp);
	_regs.eflags = *lssp;
	_regs.esp += 4;
      } else {
	ssp = SEG_ADR((us *), ss, sp);
	_regs.eflags &= ~0xffff;
	_regs.eflags |= (u_short)*ssp;
	_regs.esp += 2;
      }
    _regs.eip += 1;
    /* make sure our "virtual flags" correspond to the
     * popped eflags
     */
    update_cpu(_regs.eflags);
    break;

  case 0xf4: /* hlt...I use it for various things, 
		like trapping direct jumps into the XMS function */
    if ((_regs.cs & 0xffff) == 0xf000) /* jump into BIOS */
      {
	error("jump into BIOS %04x:%04x...simulating IRET\n",
	      _regs.cs, LWORD(eip));
	/* simulate IRET */
	ssp = SEG_ADR((us *), ss, sp);
	_regs.eip = *ssp++;
	_regs.cs = *ssp++;
	_regs.eflags = (_regs.eflags & 0xffff0000) | *ssp++;
	_regs.esp += 6;	
    } else if (lina == (unsigned char *)0xffff0) {
        error("Jump to BIOS exit routine, flag: 0x%04x\n",
	      *(unsigned short int *)0x472);
	leavedos(1);
#ifdef XMS
    } else if (lina == (unsigned char *)XMSTrap_ADD) {
        _regs.eip+=2;  /* skip halt and info byte to point to FAR RET */
	xms_control();
#endif
    } else {
        error("HLT requested: lina=0x%06x!\n", lina);
        show_regs();
	haltcount++;
	if (haltcount > MAX_HALT_COUNT) fatalerr=0xf4;
	_regs.eip += 1;
      }
    break;
  case 0xf0: /* lock */
  default:
    /* er, why don't we advance eip here, and
       why does it work??  */
    error("general protection %x\n", *csp);
    show_regs();
    show_ints(0,0x33);
    error("ERROR: SIGSEGV, protected insn...exiting!\n");
    fatalerr = 4;
    leavedos(fatalerr);  /* shouldn't return */
    _exit(1000);
  } /* end of switch() */	

#ifndef PROPER_STI  /* do_sti() sets TF bit */
  if (_regs.eflags & TF) {
    g_printf("TF: trap done");
    show_regs(); 
  }
#endif

  in_sigsegv--;
  in_sighandler=0;
}



void sigill(int sig)
{
  unsigned char *csp;
  int i, dee;

  if (! (_regs.eflags & VM)) /* test VM bit */
    {
      error("ERROR: NON-VM86 illegal insn!\n");
    }
  error("SIGILL %d received\n", sig);
  show_regs();
  csp = SEG_ADR((unsigned char *), cs, ip);
  if (csp[0] == 0xf0)
    {
      dbug_printf("ERROR: LOCK prefix not permitted!\n");
      _regs.eip++;
      return;
    }

/* look at this with Checkit...the new code just sits in a loop */
#define OLD_MATH_CODE 
#ifdef OLD_MATH_CODE
  i = (csp[0] << 8) + csp[1]; /* swapped */
  if ((i & 0xf800) != 0xd800) { /* not FPU insn */
    error("ERROR: not an FPU instruction, real illegal opcode!\n");
    fatalerr = 4;
    return;
  }

  /* I don't know what this code does. -Robert */
  switch(i & 0xc0) {
  case 0x00:
    if ((i & 0x7) == 0x6) {
      dee = *(short *)(csp +2);
      _regs.eip += 4;
    } else {
      _regs.eip += 2;
      dee = 0;
    }
    break;
  case 0x40:
    dee = (signed)csp[2];
    _regs.eip += 3;
    break;
  case 0x80:
    dee = *(short *)(csp +2);
    _regs.eip += 4;
    break;
  default:
    _regs.eip += 2;
    dee = 0;
  }

  warn("MATH: emulation %x d=%x\n", i, dee);

#else  /* this is the new, stupid MATH-EMU code. it doesn't work. */

  if ((*csp >= 0xd8) && (*csp <= 0xdf))  /* the FPU insn prefix bytes */
    {
      error("MATH: math emulation for (first 2 bytes) %02x %02x...\n",
	    *csp, *(csp+1));

      /* this is the coprocessor-not-available int. to use this, you
       * should compile your kernel with FPU-emu support in, and you
       * should install a DOS-based FPU emulator within dosemu.
       * this is untested, and I CAN'T test it, as I have a 486.
       */
      do_int(7);  
    }
#endif
}

void sigfpe(int sig)
{
  if (! (_regs.eflags & VM)) /* test VM bit */
    {
      error("ERROR: NON-VM86 SIGFPE insn!\n");
    }
  error("SIGFPE %d received\n", sig);
  show_regs();
  if (cpu.iflag) do_int(0); 
  else error("ERROR: FPE happened but interrupts disabled\n");
}

/* this is for single stepping code */
void sigtrap(int sig)
{
#ifdef PROPER_STI
  if (cpu.sti)
    {
      cpu.iflag=1;
      cpu.sti=0;
      _regs.eflags &= ~(TF);
      return;
    }
#endif

  if (_regs.eflags & TF)  /* trap flag */
    _regs.eip++;
  
  do_int(3);
}

#undef CPU_C


