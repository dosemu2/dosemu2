/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */
#include <features.h>
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
#include <sys/wait.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#ifndef __NetBSD__
#include <getopt.h>
#endif

#if X_GRAPHICS
#include <sys/mman.h>           /* root@sjoerd*/
#endif
#ifdef __NetBSD__
#include <setjmp.h>
#include <signal.h>
#include <machine/pcvt_ioctl.h>
#include "netbsd_vm86.h"
#endif
#ifdef __linux__
#if GLIBC_VERSION_CODE >= 2000
#include <sys/vt.h>
#else
#include <linux/vt.h>
#endif
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <sys/vm86.h>
#include <syscall.h>
#endif

#include "emu.h"
#include "bios.h"
#include "mouse.h"
#include "serial.h"
#include "xms.h"
#include "timers.h"
#include "cmos.h"
#include "memory.h"
#include "termio.h"
#include "config.h"
#include "port.h"
#include "int.h"
#include "hgc.h"
#include "dosio.h"
#include "disks.h"
#include "ipx.h"                /* TRB - add support for ipx */
#include "keymaps.h"
#include "bitops.h"
#include "cpu-emu.h"

#include "video.h"
#if X_GRAPHICS
#include "vgaemu.h" /* root@zaphod */
#endif

#include "pic.h"

#include "dpmi.h"

#ifdef USING_NET
#include "ipx.h"
#endif

/* Needed for DIAMOND define */
#include "vc.h"

#include "dma.h"

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

void vm86_GP_fault(void)
{

  unsigned char *csp, *lina;
  Bit32u org_eip;
  int pref_seg;
  int done,is_rep,prefix66,prefix67;

#if 0
  u_short *ssp;
  static int haltcount = 0;
#endif

#define LWECX	    (prefix66 ^ prefix67 ? REG(ecx) : LWORD(ecx))
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

  if (ignore_segv) {
    error("sigsegv ignored!\n");
    return;
  }

  if (in_sigsegv)
    error("in_sigsegv=%d!\n", in_sigsegv);

  /* in_vm86 = 0; */
  in_sigsegv++;

  /* DANG_BEGIN_REMARK 
   * In a properly functioning emulator :-), sigsegv's will never come
   * while in a non-reentrant system call (ioctl, select, etc).  Therefore,
   * there's really no reason to worry about them, so I say that I'm NOT
   * in a signal handler (I might make this a little clearer later, to
   * show that the purpose of in_sighandler is to stop non-reentrant system
   * calls from being reentered.
   * I reiterate: sigsegv's should only happen when I'm running the vm86
   * system call, so I really shouldn't be in a non-reentrant system call
   * (except maybe vm86)
   * - Robert Sanders
   * DANG_END_REMARK
   */
  in_sighandler = 0;

  if (LWORD(eflags) & TF) {
    g_printf("SIGSEGV received while TF is set\n");
    show_regs(__FILE__, __LINE__);
  }

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

  #define __SEG_ADR(type, seg, reg)  type((seg << 4) + LWORD(e##reg))
  done=0;
  is_rep=0;
  prefix66=prefix67=0;
  pref_seg=-1;

  do {
    switch (*(csp++)) {
       case 0x66:      /* operand prefix */  prefix66=1; break;
       case 0x67:      /* address prefix */  prefix67=1; break;
       case 0x2e:      /* CS */              pref_seg=REG(cs); break;
       case 0x3e:      /* DS */              pref_seg=REG(ds); break;
       case 0x26:      /* ES */              pref_seg=REG(es); break;
       case 0x36:      /* SS */              pref_seg=REG(ss); break;
       case 0x65:      /* GS */              pref_seg=REG(gs); break;
       case 0x64:      /* FS */              pref_seg=REG(fs); break;
       case 0xf2:      /* repnz */
       case 0xf3:      /* rep */             is_rep=1; break;
       default: done=1;
    }
  } while (!done);
  csp--;   
  org_eip = REG(eip);
  LWORD(eip) += (csp-lina);

  switch (*csp) {

  case 0x6c:                    /* insb */
    /* NOTE: ES can't be overwritten; prefixes 66,67 should use esi,edi,ecx
     * but is anyone using extended regs in real mode? */
    /* WARNING: no test for DI wrapping! */
    LWORD(edi) += port_rep_inb(LWORD(edx), SEG_ADR((Bit8u *),es,di),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    if (is_rep) LWECX = 0;
    LWORD(eip)++;
    break;

  case 0x6d:			/* (rep) insw / insd */
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
    if (is_rep) LWECX = 0;
    LWORD(eip)++;
    break;
      

  case 0x6e:			/* (rep) outsb */
    if (pref_seg < 0) pref_seg = LWORD(ds);
    /* WARNING: no test for _SI wrapping! */
    LWORD(esi) += port_rep_outb(LWORD(edx), __SEG_ADR((Bit8u *),pref_seg,si),
	LWORD(eflags)&DF, (is_rep? LWECX:1));
    if (is_rep) LWECX = 0;
    LWORD(eip)++;
    break;


  case 0x6f:			/* (rep) outsw / outsd */
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
    if (is_rep) LWECX = 0;
    LWORD(eip)++;
    break;

  case 0xe5:			/* inw xx, ind xx */
    if (prefix66) REG(eax) = ind((int) csp[1]);
    else LWORD(eax) = inw((int) csp[1]);
    LWORD(eip) += 2;
    break;
  case 0xe4:			/* inb xx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    LWORD(eip) += 2;
    break;

  case 0xed:			/* inw dx, ind dx */
    if (prefix66) REG(eax) = ind(LWORD(edx));
    else LWORD(eax) = inw(LWORD(edx));
    LWORD(eip) += 1;
    break;
  case 0xec:			/* inb dx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb(LWORD(edx));
    LWORD(eip) += 1;
    break;

  case 0xe7:			/* outw xx */
    if (prefix66) outd((int)csp[1], REG(eax));
    else outw((int)csp[1], LWORD(eax));
    LWORD(eip) += 2;
    break;
  case 0xe6:			/* outb xx */
    outb((int) csp[1], LO(ax));
    LWORD(eip) += 2;
    break;

  case 0xef:			/* outw dx */
    if (prefix66) outd(REG(edx), REG(eax));
    else outw(LWORD(edx), REG(eax));
    LWORD(eip) += 1;
    break;
  case 0xee:			/* outb dx */
    outb(LWORD(edx), LO(ax));
    LWORD(eip) += 1;
    break;

  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
#ifndef USE_NEW_INT
       /* set VIF (only if necessary) */
    if (REG(eflags) & IF_MASK) REG(eflags) |= VIF_MASK;
#endif /* not USE_NEW_INT */
#if defined(X86_EMULATOR) && defined(SKIP_EMU_VBIOS)
    if ((config.cpuemu>1) && (lina == (unsigned char *) CPUEMUI10_ADD)) {
      e_printf("EMU86: HLT at int10 end\n");
      LWORD(eip) += 1;	/* simply skip, so that we go back to emu mode */
      break;
    }
    else
#endif
          /* return with STI if VIP was set from run_dpmi; this happens
           * if pic_count is >0 and the VIP flag in dpmi_eflags was on
           */
    if ((lina == (unsigned char *) PIC_ADD) || (pic_icount && (REG(eflags) & VIP_MASK))) {
      pic_iret();
    }

    else if (lina == (unsigned char *) CBACK_ADD) {
      /* we are back from a callback routine */
      static void callback_return(void);
      callback_return();
    }

    else if (lina == (unsigned char *) XMSTrap_ADD) {
      LWORD(eip) += 2;  /* skip halt and info byte to point to FAR RET */
      xms_control();
    }

    else if ((lina >=(unsigned char *)DPMI_ADD) &&
	(lina <(unsigned char *)(DPMI_ADD+(unsigned long)DPMI_dummy_end-(unsigned long)DPMI_dummy_start)))
    {
      dpmi_realmode_hlt(lina);
      break;
    }

    else {
#if 1
      error("HLT requested: lina=%p!\n", lina);
      show_regs(__FILE__, __LINE__);
#if 0
      haltcount++;
      if (haltcount > MAX_HALT_COUNT)
	fatalerr = 0xf4;
#endif
#endif
      LWORD(eip) += 1;
    }
    break;

  case 0x0f: /* was: RDE hack, now handled in cpu.c */
    if (!cpu_trap_0f(csp, NULL)) goto op0ferr;
    break;

  case 0xf0:			/* lock */
  default:
    if (is_rep) fprintf(stderr, "Nope REP F3,CSP = 0x%04x\n", csp[0]);
    /* er, why don't we advance eip here, and
	 why does it work??  */
op0ferr:
    REG(eip) = org_eip;
#ifdef USE_MHPDBG
    mhp_debug(DBG_GPF, 0, 0);
#endif
    d.general=1;
    error("general protection at %p: %x\n", lina,*lina);
    show_regs(__FILE__, __LINE__);
    show_ints(0, 0x33);
    error("SIGSEGV, protected insn...exiting!\n");
    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
  }				/* end of switch() */

  if (LWORD(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs(__FILE__, __LINE__);
  }

  in_sigsegv--;
  in_sighandler = 0;
}
/* @@@ MOVE_END @@@ 32768 */



/*  */
/* run_vm86,NetBSD:vm86,vm86_return @@@  49152 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu.c --> src/emu-i386/do_vm86.c  */
#ifdef __NetBSD__

sigjmp_buf handlerbuf;

int
vm86(register struct vm86_struct *vm86p)
{
    int retval;

    retval = sigsetjmp(handlerbuf, 1);
    if (retval == 0) {
	i386_vm86(vm86p);	/* never returns, except via signal */
	I_printf("Return from i386_vm86()??\n");
	vm86_GP_fault();
	abort();			/* WTF? */
    }
    return retval & ~0x80000000;
}

void
vm86_return(int sig, int code, struct sigcontext *scp)
{
    vm86s.substr.regs.vmsc = *scp;	/* copy vm86 registers first... */
    siglongjmp(handlerbuf, code | 0x80000000); /* then simulate return */
}
#endif

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
void
run_vm86(void)
{
    int      retval;
    /*
     * always invoke vm86() with this call.  all the messy stuff will be
     * in here.
     */

/*    dma_trans(); */
    dma_run ();

    /* FIXME: is this the optimal place for this??????? */

    in_vm86 = 1;
#if 1 /* <ESC> BUG FIXER (if 1) */
    #define OVERLOAD_THRESHOULD2  600000 /* maximum acceptable value */
    #define OVERLOAD_THRESHOULD1  238608 /* good average value */
    #define OVERLOAD_THRESHOULD0  100000 /* minum acceptable value */
    if ((pic_icount || (pic_dos_time<pic_sys_time))
        && ((pic_sys_time - pic_dos_time) < OVERLOAD_THRESHOULD1) )
       REG(eflags) |= (VIP);
    else REG(eflags) &= ~(VIP);
#else
    if (pic_icount||pic_dos_time<pic_sys_time)
	REG(eflags) |= (VIP);
#endif
    /* FIXME: this needs to be clarified and rewritten */

    if (
#ifdef X86_EMULATOR
	(d.emu>1)||
#endif
	(d.general>3)) {
	dbug_printf ("DO_VM86,  cs=%04x:%04x ss=%04x:%04x f=%08x\n",
		_CS, _EIP, _SS, _SP, _EFLAGS);
	if (d.general>8)
	    dbug_printf ("ax=%04x bx=%04x ss=%04x sp=%04x bp=%04x\n"
	      		 "           cx=%04x dx=%04x ds=%04x cs=%04x ip=%04x\n"
	      		 "           si=%04x di=%04x es=%04x flg=%08x\n",
			_AX, _BX, _SS, _SP, _BP, _CX, _DX, _DS, _CS, _IP,
			_SI, _DI, _ES, _EFLAGS);
    }

    retval = DO_VM86(&vm86s);

#ifdef USE_NEW_INT
  /* sync the pic interrupt state with the flags && sync VIF & IF */
    if (_EFLAGS & VIF) {
      set_IF();
    } else {
      clear_IF();
    }
#endif /* USE_NEW_INT */

    /* This will protect us from Mr.Norton's bugs */
    if (_EFLAGS & (AC|ID)) {
      g_printf("BUG: flags changed to %08x\n",_EFLAGS);
      _EFLAGS &= ~(AC|ID);
    }
    if (
#ifdef X86_EMULATOR
	(d.emu>1)||
#endif
	(d.general>3)) {
	dbug_printf ("RET_VM86, cs=%04x:%04x ss=%04x:%04x f=%08x ret=0x%x\n",
		_CS, _EIP, _SS, _SP, _EFLAGS, retval);
	if (d.general>8)
	    dbug_printf ("ax=%04x bx=%04x ss=%04x sp=%04x bp=%04x\n"
	      		 "           cx=%04x dx=%04x ds=%04x cs=%04x ip=%04x\n"
	      		 "           si=%04x di=%04x es=%04x flg=%08x\n",
			_AX, _BX, _SS, _SP, _BP, _CX, _DX, _DS, _CS, _IP,
			_SI, _DI, _ES, _EFLAGS);
    }

    in_vm86 = 0;
    switch VM86_TYPE
	(retval) {
    case VM86_UNKNOWN:
	vm86_GP_fault();
	break;
    case VM86_STI:
	I_printf("Return from vm86() for STI\n");
	pic_iret();
	break;
    case VM86_INTx:
#ifdef USE_MHPDBG
	mhp_debug(DBG_INTx + (VM86_ARG(retval) << 8), 0, 0);
#endif
	do_int(VM86_ARG(retval));
	break;
#ifdef USE_MHPDBG
    case VM86_TRAP:
	if(!mhp_debug(DBG_TRAP + (VM86_ARG(retval) << 8), 0, 0))
	   do_int(VM86_ARG(retval));
	break;
#endif
#ifdef REQUIRES_VM86PLUS
    case VM86_PICRETURN:
        I_printf("Return for FORCE_PIC\n");
        break;
#endif
    case VM86_SIGNAL:
	I_printf("Return for SIGNAL\n");
	break;
    default:
	error("unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
	fatalerr = 4;
	}

    handle_signals();

    { /* catch user hooks here */
    	extern int uhook_fdin;
    	extern void uhook_poll(void);
    	if (uhook_fdin != -1) uhook_poll();
    }

    /* here we include the hooks to possible plug-ins */
    #define VM86_RETURN_VALUE retval
    #include "plugin_poll.h"
    #undef VM86_RETURN_VALUE


#ifdef USE_MHPDBG  
    if (mhpdbg.active) mhp_debug(DBG_POLL, 0, 0);
#endif
    /*
     * This is here because ioctl() is non-reentrant, and signal handlers
     * may have to use ioctl().  This results in a possible (probable)
     * time lag of indeterminate length (and a bad return value). Ah, life
     * isn't perfect.
     * 
     * I really need to clean up the queue functions to use real queues.
     */
    if (iq.queued)
	do_queued_ioctl();
    /* update the pic to reflect IEF */
#ifndef USE_NEW_INT
    if (REG(eflags) & VIF_MASK) {
	if (pic_iflag)
	    pic_sti();
    } else {
	if (!pic_iflag)
	    pic_cli();		/* pic_iflag=0 => enabled */
    }
#endif /* not USE_NEW_INT */
}
/* @@@ MOVE_END @@@ 49152 */


static callback_level = 0;

static void callback_return(void)
{
	callback_level--;
}

/*
 * do_call_back() calls a 16-bit DOS routine with a far call.
 * NOTE: It does _not_ save any of the vm86 registers except old cs:ip !!
 *       The _caller_ has to do this.
 *
 * FIXME: This does not yet handle DPMI stuff, it should not be called
 *        while in_dpmi is active.
 */
void do_call_back(Bit32u codefarptr)
{
	unsigned char * ssp;
	unsigned long sp;
	Bit16u oldcs, oldip;
	int level;

	/* we push the address of our HLT place in the bios
	 * as return address on the stack and run vm86 mode.
	 * Hence we get awaoken by sigsegv, whih in return calls
	 * callback_return() above, which then decreases callback_level
	 * ... and then we return from here
	 */
	ssp = (unsigned char *)(LWORD(ss)<<4);
	sp = (unsigned long) LWORD(esp);
	pushw(ssp, sp, CBACK_SEG);	/* push our return cs:ip */
	pushw(ssp, sp, CBACK_OFF);
	LWORD(esp) = (LWORD(esp) - 4) & 0xffff;
	oldcs = REG(cs);		/* save old cs:ip */
	oldip = LWORD(eip);
	REG(cs) = codefarptr >>16;	/* far jump to the vm86(DOS) routine */
	LWORD(eip) = codefarptr & 0xffff;

        level = callback_level++;
        while (callback_level > level) {
		run_vm86();
		serial_run();
        }
	/* ... and back we are */
	REG(cs) = oldcs;
	LWORD(eip) = oldip;
}

