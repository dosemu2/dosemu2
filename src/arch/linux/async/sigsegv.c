/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif /* USE_MHPDBG */
#include "debug.h"

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "emu.h"
#include "int.h"

#include "video.h"
#include "vgaemu.h" /* root@zaphod */

#include "dpmi.h"
#include "cpu-emu.h"
#include "dosemu_config.h"


/* Function prototypes */
void print_exception_info(struct sigcontext_struct *scp);



/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, struct sigcontext_struct);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scanned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */

int dosemu_fault1(
#ifdef __linux__
int signal, struct sigcontext_struct *scp
#endif /* __linux__ */
)
{
#if 0
  _eflags &= ~(AC|ID);
  REG(eflags) &= ~(AC|ID);
#endif

  if (fault_cnt > 1) {
    error("Fault handler re-entered! signal=%i _trapno=0x%lX\n",
      signal, _trapno);
    if (!in_vm86 && _cs == getsegment(cs)) {
      /* TODO - we can start gdb here */
      /* start_gdb() */
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
    in_vm86 = 0;
    switch (_trapno) {
      case 0x00: /* divide_error */
      case 0x01: /* debug */
      case 0x03: /* int3 */
      case 0x04: /* overflow */
      case 0x05: /* bounds */
      case 0x07: /* device_not_available */
#ifdef TRACE_DPMI
		 if (_trapno==1) {
	           t_printf("\n%s",e_scp_disasm(scp,0));
		 }
#endif
		 do_int(_trapno);
		 return 0;

      case 0x10: /* coprocessor error */
		 pic_request(PIC_IRQ13); /* this is the 386 way of signalling this */
		 return 0;

      case 0x11: /* alignment check */
		 /* we are now safe; nevertheless, fall into the default
		  * case and exit dosemu, as an AC fault in vm86 is(?) a
		  * catastrophic failure.
		  */
		 goto sgleave;

      case 0x06: /* invalid_op */
		{
		 unsigned char *csp;
		 dbug_printf("SIGILL while in vm86(): %04x:%04x\n", REG(cs), LWORD(eip));
		 if (config.vga && REG(cs) == config.vbios_seg) {
		   if (!config.vbios_post)
		     error("Fault in VBIOS code, try setting $_vbios_post=(1)\n");
		   else
		     error("Fault in VBIOS code, try running xdosemu under X\n");
		   goto sgleave;
		 }
#if 0
		 show_regs(__FILE__, __LINE__);
#endif /* 0 */
 		 csp = SEG_ADR((unsigned char *), cs, ip);
		 /* this one is for CPU detection programs
		  * actually we should check if int0x06 has been
		  * hooked by the pgm and redirected to it */
#if 1
		 if (IS_REDIRECTED(0x06)) {
#else
		 if (csp[0]==0x0f) {
#endif
		   do_int(_trapno);
		   return 0;
		 }
 		 /* Some db commands start with 2e (use cs segment) 
		    and thus is accounted for here */
 		 if (csp[0] == 0x2e) {
 		   csp++;
 		   LWORD(eip)++;
		   goto sgleave;
 		 }
 		 if (csp[0] == 0xf0) {
 		   dbug_printf("ERROR: LOCK prefix not permitted!\n");
 		   LWORD(eip)++;
 		   return 0;
 		 }
		 goto sgleave;
		}
      /* We want to protect the video memory and the VGA BIOS */
      case 0x0e:
                if(Video->update_screen)
                  {
                    if(VGA_EMU_FAULT(scp,code,0)==True)
                      return 0;
                  }
                /* fall into default case if not X */


      default:	
sgleave:
#if 0
		 error("unexpected CPU exception 0x%02lx errorcode: 0x%08lx while in vm86()\n"
	  	"eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
	  	"cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n", _trapno,
		_err,
	  	_rip, _rsp, _eflags, _cs, _ds, _es, _ss);


		 print_exception_info(scp);
#else
		 error("unexpected CPU exception 0x%02lx err=0x%08lx cr2=%08lx while in vm86 (DOS)\n",
	  	 _trapno, _err, _cr2);
		{
		  int auxg = debug_level('g');
		  FILE *aux = dbg_fd;
		  flush_log();  /* important! else we flush to stderr */
		  dbg_fd = stderr;
		  set_debug_level('g',1);
		  show_regs(__FILE__, __LINE__);
		  set_debug_level('g', auxg);
		  flush_log();
		  dbg_fd = aux;
		}
#endif

 		 show_regs(__FILE__, __LINE__);
		 if (debug_level('n'))		/* XXX */
		     abort();
		 flush_log();
 		 leavedos(4);
    }
  }
#define VGA_ACCESS_HACK 1
#if VGA_ACCESS_HACK
  if(_trapno==0x0e && Video->update_screen && _cs==getsegment(cs)) {
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
    v_printf("BUG: dosemu touched protected video mem, but trying to recover\n");
    if(VGA_EMU_FAULT(scp,code,1)==True)
      return 0;
  }
#endif

  if (in_dpmi) {
    /* At first let's find out where we came from */
    if (_cs==getsegment(cs)) {
      /* Fault in dosemu code */
#ifdef __i386__
      /* Now see if it is HLT */
      if (indirect_dpmi_switch(scp)) {
	/* Well, must come from dpmi_control() */
        /* Note: when using DIRECT_DPMI_CONTEXT_SWITCH, we only come
         * here if we have set the trap-flags (TF)
         * ( needed for dosdebug only )
         */
	return 0;
      }
#endif
      { /* No, not HLT, too bad :( */
	error("Fault in dosemu code, in_dpmi=%i\n", in_dpmi);
        /* TODO - we can start gdb here */
        /* start_gdb() */

	/* Going to die from here */
	goto bad;	/* well, this goto is unnecessary but I like gotos:) */
      }
    } /*_cs==getsegment(cs)*/
    else {
    /* Not in dosemu code */

    int retcode;
    if (_trapno == 0x10) {
      g_printf("coprocessor exception, calling IRQ13\n");
      pic_request(PIC_IRQ13);
      dpmi_return(scp);
      return -1;
    }

    /* If this is an exception 0x11, we have to ignore it. The reason is that
     * under real DOS the AM bit of CR0 is not set.
     * Also clear the AC flag to prevent it from re-occuring.
     */
     if (_trapno == 0x11) {
       g_printf("Exception 0x11 occured, clearing AC\n");
       _eflags &= ~AC;
       return 0;
     }

      if(_trapno==0x0e && Video->update_screen) {
        if(VGA_EMU_FAULT(scp,code,1)==True) {
          return dpmi_check_return(scp);
	}
      }

      /* dpmi_fault() will handle that */
      retcode = dpmi_fault(scp);
      if (retcode) {
        /* context was switched to dosemu's, return ASAP */
        return retcode;
      }

      if (CheckSelectors(scp, 0) == 0) {
        dpmi_return(scp);
	return -1;
      }
      /* now we are safe */
      return 0;
    }
  } /*in_dpmi*/

bad:
/* All recovery attempts failed, going to die :( */

#if 0
  csp = (char *) _eip;
  /* This was added temporarily as most illegal sigsegv's are attempt
     to call Linux int routines */
  if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85)) {
#else
  {
#endif /* 0 */
    error("cpu exception in dosemu code outside of %s!\n"
	  "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	  "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
	  (in_dpmi ? "DPMI client" : "VM86()"),
	  _trapno, _err, _cr2,
	  _rip, _rsp, _eflags, _cs, _ds, _es, _ss);

    gdb_debug();

    print_exception_info(scp);

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

    /* display the 10 bytes before and after CS:EIP.  the -> points
     * to the byte at address CS:EIP
     */
    {
    unsigned char *csp;
    int i;

    dbug_printf("OOPS : ");
    csp = (unsigned char *) _rip - 10;
    for (i = 0; i < 10; i++)
      dbug_printf("%02x ", *csp++);
    dbug_printf("-> ");
    for (i = 0; i < 10; i++)
      dbug_printf("%02x ", *csp++);
    dbug_printf("\n");
 
    show_regs(__FILE__, __LINE__);

    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
    }
  }
}

static void dosemu_fault0(int signal, struct sigcontext_struct *scp)
{
  int retcode;
  fault_cnt++;
  if (fault_cnt > 2) {
   /*
    * At this point we already tried leavedos(). Now try _exit()
    * and NOT exit(3), because glibc is probably malfunctions if
    * we are here.
    */
    _exit(255);
  }

  init_handler(scp);

  if (kernel_version_code < 0x20600+14 && kernel_version_code >= 0x20500) {
    sigset_t set;

    /* this emulates SA_NODEFER, so that we can double fault.
       SA_NODEFER only works as documented in Linux kernels >= 2.6.14.
       In 2.4 and earlier kernels double faulting is always possible.
    */
    sigemptyset(&set);
    sigaddset(&set, signal);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
  }

#if defined(__x86_64__) || defined(X86_EMULATOR)
  if (fault_cnt > 1 && _trapno == 0xe && getsegment(cs) == _cs) {
#ifdef __x86_64__
    /* see comment in signal.c, fix_fs_gs_base() */
    unsigned char *rip = (unsigned char *)_rip;
    /* page fault for fs: or gs: */
    if (*rip == 0x64 || *rip == 0x65) {
      if (check_fix_fs_gs_base(*rip)) {
	fault_cnt--;
	return;
      }
    }
#endif
#ifdef X86_EMULATOR
    /* it may be necessary to fix up a page fault in the DPMI fault handling
       code for $_cpu_emu = "vm86". This really shouldn't happen but not all
       cases have been fixed yet */
    if (config.cpuemu == 3 && !CONFIG_CPUSIM && in_dpmi && !in_dpmi_dos_int &&
	e_emu_fault(scp)) {
      fault_cnt--;
      return;
    }
#endif
  }
#endif

  if (debug_level('g')>7)
    g_printf("Entering fault handler, signal=%i _trapno=0x%X\n",
      signal, _trapno);

  retcode = dosemu_fault1 (signal, scp);

  if (debug_level('g')>8)
    g_printf("Returning from the fault handler\n");
  if(retcode)
    _eax = retcode;
  dpmi_iret_setup(scp);
  fault_cnt--;
}

#ifdef __linux__
#ifdef __x86_64__
void dosemu_fault(int signal, siginfo_t *si, void *uc)
{
  dosemu_fault0(signal, (struct sigcontext_struct *)
		&((ucontext_t *)uc)->uc_mcontext);
}
#else
void dosemu_fault(int signal, struct sigcontext_struct context)
{
  dosemu_fault0(signal, &context);
}
#endif
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
void print_exception_info(struct sigcontext_struct *scp)
{
  int i;
  unsigned char *csp;

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


    case 6:
      error("@Invalid opcode\n");
      error("@Opcodes: ");
      csp = (unsigned char *) _rip - 10;
      for (i = 0; i < 10; i++)
	error("@%02x ", *csp++);
      error("@-> ");
      for (i = 0; i < 10; i++)
	error("@%02x ", *csp++);
      error("@\n");    
      break;


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

      error("@ selector: 0x%04x\n", ((_err >> 3) & 0x1fff ));

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

      error("@ selector: 0x%04x\n", ((_err >> 3) & 0x1fff ));

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

      error("@ selector: 0x%04x\n", ((_err >> 3) & 0x1fff ));

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
      struct _fpstate *p = scp->fpstate;
      int i, n;
      unsigned short sw;
      error ("@Coprocessor Error:\n");
#ifdef __x86_64__
      error ("@cwd=%04x swd=%04x ftw=%04x\n", p->cwd, p->swd, p->ftw);
      error ("@cs:rip=%04x:%08lx ds:data=%04x:%08lx\n",	_cs,p->rip,_ds,p->rdp);
      sw = p->swd;
#else
      error ("@cw=%04x sw=%04x tag=%04x\n",
	*((unsigned short *)&(p->cw)),*((unsigned short *)&(p->sw)),
	*((unsigned short *)&(p->tag)));
      error ("@cs:eip=%04x:%08lx ds:data=%04x:%08lx\n",
	*((unsigned short *)&(p->cssel)),p->ipoff,
	*((unsigned short *)&(p->datasel)),p->dataoff);
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
	unsigned long long *r = (unsigned long long *)&(p->_st[i].significand);
	unsigned short *e = (unsigned short *)&(p->_st[i].exponent);
	error ("@fpr[%d] = %04x:%016Lx\n",n,*e,*r);
	n = (n+1) & 7;
      }
      } break;

    default:
      error("@Unknown exception\n");
      break;
    }
}
