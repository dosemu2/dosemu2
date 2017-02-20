#include "config.h"
#include "mhpdbg.h"
#include "debug.h"

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/version.h>

#include "emu.h"
#include "utilities.h"
#include "int.h"

#include "video.h"
#include "vgaemu.h" /* root@zaphod */

#include "dpmi.h"
#include "cpu-emu.h"
#include "dosemu_config.h"
#include "sig.h"

/* Function prototypes */
void print_exception_info(struct sigcontext *scp);


/*
 * All of the functions in this module need to be declared with
 *   __attribute__((no_instrument_function))
 * so that they can safely handle signals that occur in DPMI context when
 * DOSEMU is built with the "-pg" gcc flag (which enables instrumentation for
 * gprof profiling).
 *
 * The reason for this is that mcount(), implicitly called from functions
 * instrumented with "-pg", requires access to thread-local state, and on x86,
 * TLS is implemented using the GS to refer to a segment in which the
 * thread-local variables are stored.
 *
 * However, in DPMI context, GS does not refer to this segment, and the kernel
 * does not (cannot?) restore it to do so when it invokes a signal handler, so
 * we must prevent mcount() from being called at all in this context.
 */


/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, struct sigcontext);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scanned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */
static int dosemu_fault1(int signal, struct sigcontext *scp)
{
  if (fault_cnt > 1) {
    error("Fault handler re-entered! signal=%i _trapno=0x%X\n",
      signal, _trapno);
    if (!in_vm86 && !DPMIValidSelector(_cs)) {
      gdb_debug();
      _exit(43);
    } else {
      error("BUG: Fault handler re-entered not within dosemu code! in_vm86=%i\n",
        in_vm86);
    }
    goto bad;
  }

#ifdef X86_EMULATOR
  if (config.cpuemu > 1 && e_emu_fault(scp))
    return 0;
#endif

  if (in_vm86) {
    if ( _trapno == 0x0e && VGA_EMU_FAULT(scp, code, 0) == True)
      return 0;
    return vm86_fault(scp);
  }

#define VGA_ACCESS_HACK 0
#if VGA_ACCESS_HACK
  if(_trapno==0x0e && Video->update_screen && !DPMIValidSelector(_cs)) {
/* Well, there are currently some dosemu functions that touches video memory
 * without checking the permissions. This is a VERY BIG BUG.
 * Must be fixed ASAP.
 * Known offensive functions are:
 * dosemu/utilities.c:     char_out(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
 * video/int10.c:    char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
 * EMS and XMS memory transfer functions may also touch video mem.
 *  but if only the protection needs to be adjusted (no instructions emulated)
 *  we should be able to handle it in DOSEMU
 */
    if(VGA_EMU_FAULT(scp,code,1)==True) {
      v_printf("BUG: dosemu touched protected video mem, but trying to recover\n");
      return 0;
    }
  }
#endif

//  if (in_dpmi) {
    /* At first let's find out where we came from */
    if (!DPMIValidSelector(_cs)) {
      error("Fault in dosemu code, in_dpmi=%i\n", dpmi_active());
      /* TODO - we can start gdb here */
      /* start_gdb() */

      /* Going to die from here */
      goto bad;	/* well, this goto is unnecessary but I like gotos:) */
    } /*!DPMIValidSelector(_cs)*/
    else {
      if (_trapno == 0x0e && VGA_EMU_FAULT(scp, code, 1) == True)
        return dpmi_check_return(scp);
      /* Not in dosemu code: dpmi_fault() will handle that */
      return dpmi_fault(scp);
    }
//  } /*in_dpmi*/

bad:
/* All recovery attempts failed, going to die :( */

  {
#ifdef __x86_64__
    unsigned char *fsbase, *gsbase;
#endif
    error("cpu exception in dosemu code outside of %s!\n"
	  "trapno: 0x%02x  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	  "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n"
	  "fs: 0x%04x  gs: 0x%04x\n",
	  (in_dpmi_pm() ? "DPMI client" : "VM86()"),
	  _trapno, _err, _cr2,
	  _rip, _rsp, _eflags, _cs, _ds, _es, _ss, _fs, _gs);
#ifdef __x86_64__
    dosemu_arch_prctl(ARCH_GET_FS, &fsbase);
    dosemu_arch_prctl(ARCH_GET_GS, &gsbase);
    error("@fsbase: %p gsbase: %p\n", fsbase, gsbase);
#endif
    error("@\n");

    error("Please install gdb, update dosemu from git, compile it with debug\n"
        "info and report with the contents of ~/.dosemu/boot.log at\n"
"https://github.com/stsp/dosemu2/issues\n\n"
);
    gdb_debug();

    if (DPMIValidSelector(_cs))
      print_exception_info(scp);
    if (in_vm86) {
      dbug_printf("  VFLAGS(b): ");
      {
        int i;
        for (i = (1 << 17); i; i >>= 1)
          dbug_printf((_eflags & i) ? "1" : "0");
      }
      dbug_printf("\n");
      dbug_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx"
		"  VFLAGS(h): %08lx\n",
		_rax, _rbx, _rcx, _rdx, _eflags);
      dbug_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
		_rsi, _rdi, _rbp);
      dbug_printf("CS: %04x  DS: %04x  ES: %04x  FS: %04x  GS: %04x\n",
		_cs, _ds, _es, _fs, _gs);

    /* display vflags symbolically...the #f "stringizes" the macro name */
#undef PFLAG
#define PFLAG(f)  if ((_eflags)&(f)) dbug_printf(" " #f)

      dbug_printf("FLAGS:");
      PFLAG(CF);
      PFLAG(PF);
      PFLAG(AF);
      PFLAG(ZF);
      PFLAG(SF);
      PFLAG(TF);
      PFLAG(IF);
      PFLAG(DF);
      PFLAG(OF);
      PFLAG(NT);
      PFLAG(RF);
      PFLAG(VM);
      PFLAG(AC);
      dbug_printf("  IOPL: %u\n", (unsigned) ((_eflags & IOPL_MASK) >> 12));
      show_regs(__FILE__, __LINE__);
    }
    fatalerr = 4;
    leavedos_main(fatalerr);		/* shouldn't return */
    return 0;
  }
}

/* noinline is to prevent gcc from moving TLS access around init_handler() */
__attribute__((noinline))
static void dosemu_fault0(int signal, struct sigcontext *scp)
{
  pthread_t tid;

  if (fault_cnt > 2) {
   /*
    * At this point we already tried leavedos(). Now try _exit()
    * and NOT exit(3), because glibc is probably malfunctions if
    * we are here.
    */
    _exit(255);
  }

  tid = pthread_self();
  if (!pthread_equal(tid, dosemu_pthread_self)) {
    char name[128];
    pthread_getname_np(tid, name, sizeof(name));
    dosemu_error("thread %s got signal %i\n", name, signal);
    _exit(23);
    return;
  }

  if (kernel_version_code < KERNEL_VERSION(2, 6, 14)) {
    sigset_t set;

    /* this emulates SA_NODEFER, so that we can double fault.
       SA_NODEFER only works as documented in Linux kernels >= 2.6.14.
    */
    sigemptyset(&set);
    sigaddset(&set, signal);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
  }

#ifdef X86_EMULATOR
  if (fault_cnt > 1 && _trapno == 0xe && !DPMIValidSelector(_cs)) {
    /* it may be necessary to fix up a page fault in the DPMI fault handling
       code for $_cpu_emu = "vm86". This really shouldn't happen but not all
       cases have been fixed yet */
    if (config.cpuemu == 3 && !CONFIG_CPUSIM && in_dpmi_pm() &&
	e_emu_fault(scp)) {
      return;
    }
  }
#endif

  if (debug_level('g')>7)
    g_printf("Entering fault handler, signal=%i _trapno=0x%X\n",
      signal, _trapno);

  dosemu_fault1(signal, scp);

  if (debug_level('g')>8)
    g_printf("Returning from the fault handler\n");
}

#ifdef __linux__
SIG_PROTO_PFX
void dosemu_fault(int signal, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  struct sigcontext *scp = (struct sigcontext *)&uct->uc_mcontext;
  /* need to call init_handler() before any syscall.
   * Additionally, TLS access should be done in a separate no-inline
   * function, so that gcc not to move the TLS access around init_handler(). */
  init_handler(scp, 0);
  fault_cnt++;
  dosemu_fault0(signal, scp);
  fault_cnt--;
  deinit_handler(scp, uct);
}
#endif /* __linux__ */

/*
 * DANG_BEGIN_FUNCTION print_exception_info
 *
 * Prints information about an exception: exception number, error code,
 * address, reason, etc.
 *
 * DANG_END_FUNCTION
 *
 */
void print_exception_info(struct sigcontext *scp)
{
  int i;

  switch(_trapno)
    {
    case 0:
      error("@Division by zero\n");
      break;


    case 1:
      error("@Debug exception\n");
      break;


    case 3:
      error("@Breakpoint exception (caused by INT 3 instruction)\n");
      break;


    case 4:
      error("@Overflow exception (caused by INTO instruction)\n");
      break;


    case 5:
      error("@Bound exception (caused by BOUND instruction)\n");
      break;


    case 6: {
      unsigned char *csp;
      int ps = getpagesize();
      unsigned pa = _rip & (ps - 1);
      int sub = min(pa, 10);
      int sup = min(ps - pa, 10);
      error("@Invalid opcode\n");
      error("@Opcodes: ");
      csp = (unsigned char *) _rip - sub;
      for (i = 0; i < 10 - sub; i++)
        error("@XX ");
      for (i = 0; i < sub; i++)
	error("@%02x ", *csp++);
      error("@-> ");
      for (i = 0; i < sup; i++)
	error("@%02x ", *csp++);
      for (i = 0; i < 10 - sup; i++)
        error("@XX ");
      error("@\n");
      break;
    }

    case 7:
      error("@Coprocessor exception (coprocessor not available)\n");
      /* I'd like to print some info on the EM, MP, and TS flags in CR0,
       * but I don't know where I can get that information :-(
       * Anyway, this exception should not happen... (Erik Mouw)
       */
      break;


    case 8:
      error("@Double fault\n");
      break;


    case 9:
      error("@Coprocessor segment overflow\n");
      break;


    case 0xa:
      error("@Invalid TSS\n");
      if(_err & 0x02)
	error("@IDT");
      else if(_err & 0x04)
	error("@LDT");
      else
	error("@GDT");

      error("@ selector: 0x%04lx\n", ((_err >> 3) & 0x1fff ));

      if(_err & 0x01)
	error("@Exception was not caused by DOSEMU\n");
      else
	error("@Exception was caused by DOSEMU\n");
      break;


    case 0xb:
      error("@Segment not available\n");
      /* This is the same code as case 0x0a; the compiler merges these
       * blocks, so I don't have to edit some dirty constructions to
       * generate one block of code. (Erik Mouw)
       */
      if(_err & 0x02)
	error("@IDT");
      else if(_err & 0x04)
	error("@LDT");
      else
	error("@GDT");

      error("@ selector: 0x%04lx\n", ((_err >> 3) & 0x1fff ));

      if(_err & 0x01)
	error("@Exception was not caused by DOSEMU\n");
      else
	error("@Exception was caused by DOSEMU\n");
      break;


    case 0xc:
      error("@Stack exception\n");
      break;


    case 0xd:
      error("@General protection exception\n");
      /* This is the same code as case 0x0a; the compiler merges these
       * blocks, so I don't have to edit some dirty constructions to
       * generate one block of code. (Erik Mouw)
       */
      if(_err & 0x02)
	error("@IDT");
      else if(_err & 0x04)
	error("@LDT");
      else
	error("@GDT");

      error("@ selector: 0x%04lx\n", ((_err >> 3) & 0x1fff ));

      if(_err & 0x01)
	error("@Exception was not caused by DOSEMU\n");
      else
	error("@Exception was caused by DOSEMU\n");
      break;


    case 0xe:
      error("@Page fault: ");
      if(_err & 0x02)
	error("@write");
      else
	error("@read");

      error("@ instruction to linear address: 0x%08lx\n", _cr2);

      error("@CPU was in ");
      if(_err & 0x04)
	error("@user mode\n");
      else
	error("@supervisor mode\n");

      error("@Exception was caused by ");
      if(_err & 0x01)
	error("@insufficient privilege\n");
      else
	error("@non-available page\n");
      break;

   case 0x10: {
      int i, n;
      unsigned short sw;
#ifdef __x86_64__
      fpregset_t p = ((mcontext_t *)scp)->fpregs;
      error ("@Coprocessor Error:\n");
      error ("@cwd=%04x swd=%04x ftw=%04x\n", p->cwd, p->swd, p->ftw);
      error ("@cs:rip=%04x:%08lx ds:data=%04x:%08lx\n",	_cs,p->rip,_ds,p->rdp);
      sw = p->swd;
#else
      struct _fpstate *p = scp->fpstate;
      error ("@Coprocessor Error:\n");
      error ("@cw=%04x sw=%04x tag=%04x\n",
	     ((unsigned short)(p->cw)),((unsigned short)(p->sw)),
	((unsigned short)(p->tag)));
      error ("@cs:eip=%04x:%08x ds:data=%04x:%08x\n",
	     ((unsigned short)(p->cssel)),(unsigned)p->ipoff,
	     ((unsigned short)(p->datasel)),(unsigned)p->dataoff);
      sw = p->sw;
#endif
      if ((sw&0x80)==0) error("@No error summary bit,why?\n");
      else {
	if (sw&0x20) error("@Precision\n");
	if (sw&0x10) error("@Underflow\n");
	if (sw&0x08) error("@Overflow\n");
	if (sw&0x04) error("@Divide by 0\n");
	if (sw&0x02) error("@Denormalized\n");
	if ((sw&0x41)==0x01) error("@Invalid op\n");
	  else if ((sw&0x41)==0x41) error("@Stack fault\n");
      }
      n = (sw >> 11) & 7;
      for (i=0; i<8; i++) {
	unsigned short *r = p->_st[i].significand;
	unsigned short e = p->_st[i].exponent;
	error ("@fpr[%d] = %04x:%04x%04x%04x%04x\n",n,e,r[3],r[2],r[1],r[0]);
	n = (n+1) & 7;
      }
      } break;

#ifdef __SSE__
   case 0x13: {
      int i;
      unsigned mxcsr;
#ifdef __x86_64__
      fpregset_t p = ((mcontext_t *)scp)->fpregs;
      error ("@SIMD Floating-Point Exception:\n");
      mxcsr = p->mxcsr;
      error ("@mxcsr=%08x, mxcr_mask=%08x\n",mxcsr,(unsigned)(p->mxcr_mask));
#else
      struct _fpstate *p = scp->fpstate;
      error ("@SIMD Floating-Point Exception:\n");
      mxcsr = p->mxcsr;
      error ("@mxcsr=%08x\n",mxcsr);
#endif
      if (mxcsr&0x40) error("@Denormals are zero\n");
      if (mxcsr&0x20) error("@Precision\n");
      if (mxcsr&0x10) error("@Underflow\n");
      if (mxcsr&0x08) error("@Overflow\n");
      if (mxcsr&0x04) error("@Divide by 0\n");
      if (mxcsr&0x02) error("@Denormalized\n");
      if (mxcsr&0x01) error("@Invalid op\n");
      for (i=0; i<sizeof(p->_xmm)/sizeof(p->_xmm[0]); i++)
      {
	error ("@xmm[%d] = %08x:%08x:%08x:%08x\n",i,
	      (unsigned)p->_xmm[i].element[0], (unsigned)p->_xmm[i].element[1],
	      (unsigned)p->_xmm[i].element[2], (unsigned)p->_xmm[i].element[3]);
      }
    } break;
#endif

    default:
      error("@Unknown exception\n");
      break;
    }
}
