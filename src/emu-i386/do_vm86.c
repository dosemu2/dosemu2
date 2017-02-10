#include "config.h"
#include "mhpdbg.h"

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <fenv.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>

#include "emu.h"
#ifdef __linux__
#include "sys_vm86.h"
#endif
#include "bios.h"
#include "mouse.h"
#include "serial.h"
#include "xms.h"
#include "timers.h"
#include "cmos.h"
#include "memory.h"
#include "config.h"
#include "port.h"
#include "int.h"
#include "disks.h"
#include "ipx.h"                /* TRB - add support for ipx */
#include "keymaps.h"
#include "keyb_server.h"
#include "bitops.h"
#include "coopth.h"
#include "utilities.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "kvm.h"

#include "video.h"

#include "pic.h"
#include "dpmi.h"
#include "hlt.h"
#include "ipx.h"
#include "vgaemu.h"
#include "sig.h"

int vm86_fault(struct sigcontext *scp)
{
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
      dbug_printf("SIGILL while in vm86(): %04x:%04x\n", SREG(cs), LWORD(eip));
      if (config.vga && SREG(cs) == config.vbios_seg) {
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
      if (IS_REDIRECTED(0x06))
#else
      if (csp[0]==0x0f)
#endif
      {
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
    error("unexpected CPU exception 0x%02x err=0x%08lx cr2=%08lx while in vm86 (DOS)\n",
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
    flush_log();
    leavedos_from_sig(4);
  }
  return 0; /* keeps GCC happy */
}

/*  */
/* vm86_GP_fault @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/arch/linux/async/sigsegv.c --> src/emu-i386/do_vm86.c  */
/*
 * DANG_BEGIN_FUNCTION vm86_GP_fault
 *
 * description:
 * All from the kernel unhandled general protection faults from V86 mode
 * are handled here. This are mainly port IO and the HLT instruction.
 *
 * DANG_END_FUNCTION
 */

static int handle_GP_fault(void)
{
  unsigned char *csp, *lina;
  int pref_seg;
  int done,is_rep,prefix66,prefix67;

#if 0
  u_short *ssp;
  static int haltcount = 0;
#endif

#define LWECX	    (prefix66 ^ prefix67 ? REG(ecx) : LWORD(ecx))
#define setLWECX(x) {if (prefix66 ^ prefix67) REG(ecx)=(x); else LWORD(ecx) = (x);}
#define MAX_HALT_COUNT 3

#if 0
    csp = SEG_ADR((unsigned char *), cs, ip);
    ssp = SEG_ADR((us *), ss, sp);
    if ((*csp&0xfd)==0xec) {	/* inb, outb */
    	i_printf("VM86_GP_FAULT at %08lx, cod=%02x %02x*%02x %02x %02x %02x\n"
		 "                 stk=%04x %04x %04x %04x\n",
		 (long)csp,
		 csp[-2], csp[-1], csp[0], csp[1], csp[2], csp[3],
		 ssp[0], ssp[1], ssp[2], ssp[3]);
    }
#endif

  csp = lina = SEG_ADR((unsigned char *), cs, ip);

  /* fprintf(stderr, "CSP in cpu is 0x%04x\n", *csp); */


  /* DANG_BEGIN_REMARK
   * Here we handle all prefixes prior switching to the appropriate routines
   * The exception CS:EIP will point to the first prefix that effects
   * the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
   * So we collect all prefixes and remember them.
   * - Hans Lermen
   * DANG_END_REMARK
   */

  #define __SEG_ADR(type, seg, reg)  type(MK_FP32(seg, LWORD(e##reg)))
  done=0;
  is_rep=0;
  prefix66=prefix67=0;
  pref_seg=-1;

  do {
    switch (*(csp++)) {
       case 0x66:      /* operand prefix */  prefix66=1; break;
       case 0x67:      /* address prefix */  prefix67=1; break;
       case 0x2e:      /* CS */              pref_seg=SREG(cs); break;
       case 0x3e:      /* DS */              pref_seg=SREG(ds); break;
       case 0x26:      /* ES */              pref_seg=SREG(es); break;
       case 0x36:      /* SS */              pref_seg=SREG(ss); break;
       case 0x65:      /* GS */              pref_seg=SREG(gs); break;
       case 0x64:      /* FS */              pref_seg=SREG(fs); break;
       case 0xf2:      /* repnz */
       case 0xf3:      /* rep */             is_rep=1; break;
       default: done=1;
    }
  } while (!done);
  csp--;

#if defined(X86_EMULATOR) && defined(CPUEMU_DIRECT_IO)
  if (InCompiledCode) {
    prefix66 ^= 1; prefix67 ^= 1;	/* since we come from 32-bit code */
/**/ e_printf("vm86_GP_fault: adjust prefixes to 66=%d,67=%d\n",
	prefix66,prefix67);
  }
#endif
  switch (*csp) {

  case 0xf1:                   /* int 1 */
    LWORD(eip)++; /* emulated "undocumented" instruction */
    do_int(1);
    break;

  case 0x6c:                    /* insb */
    LWORD(eip)++;
    /* NOTE: ES can't be overwritten; prefixes 66,67 should use esi,edi,ecx
     * but is anyone using extended regs in real mode? */
    /* WARNING: no test for DI wrapping! */
    LWORD(edi) += port_rep_inb(LWORD(edx), SEG_ADR((Bit8u *),es,di),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    if (is_rep) setLWECX(0);
    break;

  case 0x6d:			/* (rep) insw / insd */
    LWORD(eip)++;
    /* NOTE: ES can't be overwritten */
    /* WARNING: no test for _DI wrapping! */
    if (prefix66) {
      LWORD(edi) += port_rep_ind(LWORD(edx), SEG_ADR((Bit32u *),es,di),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    }
    else {
      LWORD(edi) += port_rep_inw(LWORD(edx), SEG_ADR((Bit16u *),es,di),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    }
    if (is_rep) setLWECX(0);
    break;

  case 0x6e:			/* (rep) outsb */
    LWORD(eip)++;
    if (pref_seg < 0) pref_seg = LWORD(ds);
    /* WARNING: no test for _SI wrapping! */
    LWORD(esi) += port_rep_outb(LWORD(edx), __SEG_ADR((Bit8u *),pref_seg,si),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    if (is_rep) setLWECX(0);
    break;

  case 0x6f:			/* (rep) outsw / outsd */
    LWORD(eip)++;
    if (pref_seg < 0) pref_seg = LWORD(ds);
    /* WARNING: no test for _SI wrapping! */
    if (prefix66) {
      LWORD(esi) += port_rep_outd(LWORD(edx), __SEG_ADR((Bit32u *),pref_seg,si),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    }
    else {
      LWORD(esi) += port_rep_outw(LWORD(edx), __SEG_ADR((Bit16u *),pref_seg,si),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    }
    if (is_rep) setLWECX(0);
    break;

  case 0xe5:			/* inw xx, ind xx */
    LWORD(eip) += 2;
    if (prefix66) REG(eax) = ind((int) csp[1]);
    else LWORD(eax) = inw((int) csp[1]);
    break;

  case 0xe4:			/* inb xx */
    LWORD(eip) += 2;
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    break;

  case 0xed:			/* inw dx, ind dx */
    LWORD(eip)++;
    if (prefix66) REG(eax) = ind(LWORD(edx));
    else LWORD(eax) = inw(LWORD(edx));
    break;

  case 0xec:			/* inb dx */
    LWORD(eip)++;
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb(LWORD(edx));
    break;

  case 0xe7:			/* outw xx */
    LWORD(eip) += 2;
    if (prefix66) outd((int)csp[1], REG(eax));
    else outw((int)csp[1], LWORD(eax));
    break;

  case 0xe6:			/* outb xx */
    LWORD(eip) += 2;
    outb((int) csp[1], LO(ax));
    break;

  case 0xef:			/* outw dx, outd dx */
    LWORD(eip)++;
    if (prefix66) outd(LWORD(edx), REG(eax));
    else outw(LWORD(edx), REG(eax));
    break;

  case 0xee:			/* outb dx */
    LWORD(eip)++;
    outb(LWORD(edx), LO(ax));
    break;

  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
    hlt_handle();
    break;

  case 0x0f: /* was: RDE hack, now handled in cpu.c */
    if (!cpu_trap_0f(csp, NULL))
      return 0;
    break;

  case 0xf0:			/* lock */
  default:
    return 0;
  }

  LWORD(eip) += (csp-lina);
  return 1;
}

static void vm86_GP_fault(void)
{
    unsigned char *lina;
    if (handle_GP_fault())
	return;
    lina = SEG_ADR((unsigned char *), cs, ip);
#ifdef USE_MHPDBG
    mhp_debug(DBG_GPF, 0, 0);
#endif
    set_debug_level('g', 1);
    error("general protection at %p: %x\n", lina,*lina);
    show_regs(__FILE__, __LINE__);
    show_ints(0, 0x33);
    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
}
/* @@@ MOVE_END @@@ 32768 */

static int handle_GP_hlt(void)
{
  unsigned char *csp;
  csp = SEG_ADR((unsigned char *), cs, ip);
  if (*csp == 0xf4)
    return hlt_handle();
  return HLT_RET_NONE;
}

#ifdef __i386__
static int true_vm86(union vm86_union *x)
{
    int ret;
    uint32_t old_flags = REG(eflags);

#if 0
    ret = vm86(&x->vm86ps);
#else
    /* need to use vm86_plus for now as otherwise dosdebug doesn't work */
    ret = vm86_plus(VM86_ENTER, &x->vm86compat);
#endif
    /* kernel has a nasty habit of clearing VIP.
     * TODO: check kernel version */
    REG(eflags) |= (old_flags & VIP);
    return ret;
}
#endif

static int do_vm86(union vm86_union *x)
{
    if (config.cpu_vm == CPUVM_KVM)
	return kvm_vm86(&x->vm86ps);
#ifdef __i386__
#ifdef X86_EMULATOR
    if (config.cpu_vm == CPUVM_EMU)
	return e_vm86();
#endif
    return true_vm86(x);
#else
    return e_vm86();
#endif
}

static void _do_vm86(void)
{
    int retval, vtype, dret;

    if (isset_IF() && isset_VIP()) {
	error("both IF and VIP set\n");
	clear_VIP();
    }
    loadfpstate(vm86_fpu_state);
    in_vm86 = 1;
again:
    retval = do_vm86(&vm86u);
    vtype = VM86_TYPE(retval);
    /* optimize VM86_STI case that can return with ints disabled
     * if VIP is set */
    if (vtype == VM86_STI) {
	if (!isset_IF())
	    goto again;
    }
    in_vm86 = 0;
    savefpstate(vm86_fpu_state);
    /* there is no real need to save and restore the FPU state of the
       emulator itself: savefpstate (fnsave) also resets the current FPU
       state using fninit/ldmxcsr which is good enough for calling FPU-using
       routines.
    */
    feenableexcept(FE_DIVBYZERO | FE_OVERFLOW);

    if (
#ifdef X86_EMULATOR
	(debug_level('e')>1)||
#endif
	(debug_level('g')>3)) {
	dbug_printf ("RET_VM86, cs=%04x:%04x ss=%04x:%04x f=%08x ret=0x%x\n",
		_CS, _EIP, _SS, _SP, _EFLAGS, retval);
	if (debug_level('g')>8)
	    dbug_printf ("ax=%04x bx=%04x ss=%04x sp=%04x bp=%04x\n"
	      		 "           cx=%04x dx=%04x ds=%04x cs=%04x ip=%04x\n"
	      		 "           si=%04x di=%04x es=%04x flg=%08x\n",
			_AX, _BX, _SS, _SP, _BP, _CX, _DX, _DS, _CS, _IP,
			_SI, _DI, _ES, _EFLAGS);
    }

    switch (vtype) {
    case VM86_UNKNOWN:
	vm86_GP_fault();
	break;
    case VM86_STI:
	I_printf("Return from vm86() for STI\n");
	break;
    case VM86_INTx:
#ifdef USE_MHPDBG
	dret = mhp_debug(DBG_INTx + (VM86_ARG(retval) << 8), 1, 0);
	if (!dret)
#endif
	    do_int(VM86_ARG(retval));
	break;
#ifdef USE_MHPDBG
    case VM86_TRAP:
	if(!mhp_debug(DBG_TRAP + (VM86_ARG(retval) << 8), 0, 0))
	   do_int(VM86_ARG(retval));
	break;
#endif
    case VM86_PICRETURN:
        I_printf("Return for FORCE_PIC\n");
        break;
    case VM86_SIGNAL:
	I_printf("Return for SIGNAL\n");
	break;
    default:
	error("unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
	fatalerr = 4;
    }
}

/*
 * DANG_BEGIN_FUNCTION run_vm86
 *
 * description:
 * Here is where DOSEMU runs VM86 mode with the vm86() call
 * which also has the registers that it will be called with. It will stop
 * vm86 mode for many reasons, like trying to execute an interrupt, doing
 * port I/O to ports not opened for I/O, etc ...
 *
 * DANG_END_FUNCTION
 */
static void run_vm86(void)
{
    int retval, cnt;

    if (
#ifdef X86_EMULATOR
	(debug_level('e')>1)||
#endif
	(debug_level('g')>3)) {
	dbug_printf ("DO_VM86,  cs=%04x:%04x ss=%04x:%04x f=%08x\n",
		_CS, _EIP, _SS, _SP, _EFLAGS);
	if (debug_level('g')>8)
	    dbug_printf ("ax=%04x bx=%04x ss=%04x sp=%04x bp=%04x\n"
	      		 "           cx=%04x dx=%04x ds=%04x cs=%04x ip=%04x\n"
	      		 "           si=%04x di=%04x es=%04x flg=%08x\n",
			_AX, _BX, _SS, _SP, _BP, _CX, _DX, _DS, _CS, _IP,
			_SI, _DI, _ES, _EFLAGS);
    }

    cnt = 0;
    while ((retval = handle_GP_hlt())) {
	cnt++;
	if (debug_level('g')>3) {
	    g_printf("DO_VM86: premature fault handled, %i\n", cnt);
	    g_printf("RET_VM86, cs=%04x:%04x ss=%04x:%04x f=%08x\n",
		_CS, _EIP, _SS, _SP, _EFLAGS);
	}
	if (in_dpmi_pm())
	    return;
	if (isset_IF() && isset_VIP())
	    return;
	if (signal_pending())
	    return;
	/* if thread wants some sleep, we can't fuck it in a busy loop */
	if (coopth_wants_sleep())
	    return;
	/* some subsystems doesn't want this optimization loop as well */
	if (retval == HLT_RET_SPECIAL)
	    return;
	if (retval != HLT_RET_NORMAL)
	    break;

	if (debug_level('g') > 3) {
	  dbug_printf ("DO_VM86,  cs=%04x:%04x ss=%04x:%04x f=%08x\n",
		_CS, _EIP, _SS, _SP, _EFLAGS);
	  if (debug_level('g')>8)
	    dbug_printf ("ax=%04x bx=%04x ss=%04x sp=%04x bp=%04x\n"
	      		 "           cx=%04x dx=%04x ds=%04x cs=%04x ip=%04x\n"
	      		 "           si=%04x di=%04x es=%04x flg=%08x\n",
			_AX, _BX, _SS, _SP, _BP, _CX, _DX, _DS, _CS, _IP,
			_SI, _DI, _ES, _EFLAGS);
	}
    }
#ifdef USE_MHPDBG
    if (mhpdbg.active)
	mhp_debug(DBG_PRE_VM86, 0, 0);
#endif

    _do_vm86();
}

/* same as run_vm86(), but avoids any looping in handling GPFs */
void vm86_helper(void)
{
  assert(!in_dpmi_pm());
  _do_vm86();
  handle_signals();
  coopth_run();
}

/*
 * DANG_BEGIN_FUNCTION loopstep_run_vm86
 *
 * description:
 * Here we collect all stuff, that has to be executed within
 * _one_ pass (step) of a loop containing run_vm86().
 * DANG_END_FUNCTION
 */
void loopstep_run_vm86(void)
{
    uncache_time();
    if (!dosemu_frozen && !in_dpmi_pm() && !signal_pending())
	run_vm86();
    do_periodic_stuff();
    hardware_run();
    pic_run();		/* trigger any hardware interrupts requested */
}


static int callback_level;
#define MAX_CBKS 256
static far_t callback_rets[MAX_CBKS];
Bit16u CBACK_OFF;

static void callback_return(Bit16u off2, void *arg)
{
    far_t ret;
    assert(callback_level > 0);
    ret = callback_rets[callback_level - 1];
    SREG(cs) = ret.segment;
    LWORD(eip) = ret.offset;
}

/*
 * do_call_back() calls a 16-bit DOS routine with a far call.
 * NOTE: It does _not_ save any of the vm86 registers except old cs:ip !!
 *       The _caller_ has to do this.
 */
static void __do_call_back(Bit16u cs, Bit16u ip, int intr)
{
	far_t *ret;

	if (fault_cnt && !in_leavedos) {
		error("do_call_back() executed within the signal context!\n");
		leavedos(25);
	}

	/* save return address - dont use DOS stack for that :( */
	assert(callback_level < MAX_CBKS);
	ret = &callback_rets[callback_level];
	ret->segment = SREG(cs);
	ret->offset = LWORD(eip);
	SREG(cs) = CBACK_SEG;
	LWORD(eip) = CBACK_OFF;

	if (intr)
		fake_int_to(cs, ip); /* far jump to the vm86(DOS) routine */
	else
		fake_call_to(cs, ip); /* far jump to the vm86(DOS) routine */

	callback_level++;
	/* switch to DOS code */
	coopth_sched();
	callback_level--;
}

void do_call_back(Bit16u cs, Bit16u ip)
{
    __do_call_back(cs, ip, 0);
}

void do_int_call_back(int intno)
{
    __do_call_back(ISEG(intno), IOFF(intno), 1);
}

int vm86_init(void)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name = "do_call_back";
    hlt_hdlr.func = callback_return;
    CBACK_OFF = hlt_register_handler(hlt_hdlr);
    return 0;
}
