#include "config.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

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
#include <sys/wait.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <limits.h>
#ifndef __NetBSD__
#include <getopt.h>
#endif
#include <assert.h>


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
#include <linux/vt.h>
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

#ifdef bon
extern void     vm86_GP_fault();
#endif

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
  int done,is_rep,is_32bit;

#if 0
  u_short *ssp;
  static int haltcount = 0;
#endif

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
    error("ERROR: sigsegv ignored!\n");
    return;
  }

  if (in_sigsegv)
    error("ERROR: in_sigsegv=%d!\n", in_sigsegv);

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
   * The exception CS:EIP will point to the first prefix that effects the
   * the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
   * So we collect all prefixes and remember them.
   * - Hans Lermen
   * DANG_END_REMARK
   */

  #define __SEG_ADR(type, seg, reg)  type((seg << 4) + LWORD(e##reg))
  done=0;
  is_rep=0;
  is_32bit=0;
  pref_seg=-1;

  do {
    switch (*(csp++)) {
       case 0x66:      /* operand prefix */  is_32bit=1; break;
       case 0x2e:      /* CS */              pref_seg=REG(cs); break;
       case 0x3e:      /* DS */              pref_seg=REG(ds); break;
       case 0x26:      /* ES */              pref_seg=REG(es); break;
       case 0x36:      /* SS */              pref_seg=REG(ss); break;
       case 0x65:      /* GS */              pref_seg=REG(gs); break;
       case 0x64:      /* FS */              pref_seg=REG(fs); break;
       case 0xf3:      /* rep */             is_rep=1; break;
#if 0
       case 0xf2:      /* repnz */
#endif
       default: done=1;
    }
  } while (!done);
  csp--;   
  org_eip = REG(eip);
  LWORD(eip) += (csp-lina);

  switch (*csp) {

  case 0x6c:                    /* insb */
    /* NOTE: ES can't be overwritten */
    if (is_rep) {
      int delta = 1;
      if(LWORD(eflags) &DF ) delta = -1;
      i_printf("Doing REP F3 6C (rep insb) %04x bytes, DELTA %d\n",
              LWORD(ecx),delta);
      while (LWORD(ecx))  {
        *(SEG_ADR((unsigned char *),es,di)) = inb((int) LWORD(edx));
        LWORD(edi) += delta;
        LWORD(ecx)--;
      }
    }
    else {
      *(SEG_ADR((unsigned char *),es,di)) = inb((int) LWORD(edx));
      i_printf("insb(0x%04x) value %02x\n",
              LWORD(edx),*(SEG_ADR((unsigned char *),es,di)));   
      if(LWORD(eflags) & DF) LWORD(edi)--;
      else LWORD(edi)++;
    }
    LWORD(eip)++;
    break;

  case 0x6d:			/* (rep) insw / insd */
    /* NOTE: ES can't be overwritten */
    if (is_rep) {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        int delta =incr; \
        if(LWORD(eflags) & DF) delta = -incr; \
        i_printf("rep ins%c %04x words, DELTA %d\n", x, \
                LWORD(ecx),delta); \
        while(LWORD(ecx)) { \
          *(SEG_ADR(typ,es,di))=iotyp(LWORD(edx)); \
          LWORD(edi) += delta; \
          LWORD(ecx)--; \
        }

      if (is_32bit) {
                                /* rep insd */
        ___LOCALAUX((unsigned long *),ind,4,'d')
      }
      else {
                                /* rep insw */
        ___LOCALAUX((unsigned short *),inw,2,'w')
      }
      #undef  ___LOCALAUX
    }
    else {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        *(SEG_ADR(typ,es,di)) =iotyp((int) LWORD(edx)); \
        i_printf("ins%c(0x%04x) value %04x \n", x, \
                LWORD(edx),*(SEG_ADR((unsigned short *),es,di))); \
        if(LWORD(eflags) & DF) LWORD(edi) -= incr; \
        else LWORD(edi) +=incr;
      
      if (is_32bit) {
                                /* insd */
        ___LOCALAUX((unsigned long *),ind,4,'d')
      }
      else {
                                /* insw */
        ___LOCALAUX((unsigned short *),inw,2,'w')
      }
      #undef  ___LOCALAUX
    }
    LWORD(eip)++;
    break;
      

  case 0x6e:			/* (rep) outsb */
    if (pref_seg < 0) pref_seg = LWORD(ds);
    if (is_rep) {
      int delta = 1;
      i_printf("untested: rep outsb\n");
      if(LWORD(eflags) & DF) delta = -1;
      while(LWORD(ecx)) {
        outb(LWORD(edx), *(__SEG_ADR((unsigned char *),pref_seg,si)));
        LWORD(esi) += delta;
        LWORD(ecx)--;
      }
    }
    else {
      i_printf("untested: outsb port 0x%04x value %02x\n",
              LWORD(edx),*(__SEG_ADR((unsigned char *),pref_seg,si)));
      outb(LWORD(edx), *(__SEG_ADR((unsigned char *),pref_seg,si)));
      if(LWORD(eflags) & DF) LWORD(esi)--;
      else LWORD(esi)++;
    }
    LWORD(eip)++;
    break;


  case 0x6f:			/* (rep) outsw / outsd */
    if (pref_seg < 0) pref_seg = LWORD(ds);
    if (is_rep) {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        int delta = incr; \
        i_printf("untested: rep outs%c\n", x); \
        if(LWORD(eflags) & DF) delta = -incr; \
        while(LWORD(ecx)) { \
          iotyp(LWORD(edx), *(__SEG_ADR(typ,pref_seg,si))); \
          LWORD(esi) += delta; \
          LWORD(ecx)--; \
        }
      
      if (is_32bit) {
                                /* rep outsd */
        ___LOCALAUX((unsigned long *),outd,4,'d')
      }
      else {
                                /* rep outsw */
        ___LOCALAUX((unsigned short *),outw,2,'w')
      }
      #undef  ___LOCALAUX
    }
    else {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        i_printf("untested: outs%c port 0x%04x value %04x\n", x, \
                LWORD(edx), (unsigned int) *(__SEG_ADR(typ,pref_seg,si))); \
        iotyp(LWORD(edx), *(__SEG_ADR(typ,pref_seg,si))); \
        if(LWORD(eflags) & DF ) LWORD(esi) -= incr; \
        else LWORD(esi) +=incr;
      
      if (is_32bit) {
                                /* outsd */
        ___LOCALAUX((unsigned long *),outd,4,'d')
      }
      else {
                                /* outsw */
        ___LOCALAUX((unsigned short *),outw,2,'w')
      }
      #undef  ___LOCALAUX
    } 
    LWORD(eip)++;
    break;

  case 0xe5:			/* inw xx, ind xx */
    if (is_32bit) REG(eax) = ind((int) csp[1]);
    else LWORD(eax) = inw((int) csp[1]);
    LWORD(eip) += 2;
    break;
  case 0xe4:			/* inb xx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    LWORD(eip) += 2;
    break;

  case 0xed:			/* inw dx, ind dx */
    if (is_32bit) REG(eax) = ind(LWORD(edx));
    else LWORD(eax) = inw(LWORD(edx));
    LWORD(eip) += 1;
    break;
  case 0xec:			/* inb dx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb(LWORD(edx));
    LWORD(eip) += 1;
    break;

  case 0xe7:			/* outw xx */
    if (is_32bit) outd((int)csp[1], REG(eax));
    else outw((int)csp[1], LWORD(eax));
    LWORD(eip) += 2;
    break;
  case 0xe6:			/* outb xx */
    outb((int) csp[1], LO(ax));
    LWORD(eip) += 2;
    break;

  case 0xef:			/* outw dx */
    if (is_32bit) outd(REG(edx), REG(eax));
    else outw(REG(edx), REG(eax));
    LWORD(eip) += 1;
    break;
  case 0xee:			/* outb dx */
    outb(LWORD(edx), LO(ax));
    LWORD(eip) += 1;
    break;

  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
       /* set VIF (only if necessary) */
    if (REG(eflags) & IF_MASK) REG(eflags) |= VIF_MASK;
          /* return with STI if VIP was set from run_dpmi; this happens
           * if pic_count is >0 and the VIP flag in dpmi_eflags was on
           */
    if ((lina == (unsigned char *) PIC_ADD) || (pic_icount && (REG(eflags) & VIP_MASK))) {
      pic_iret();
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

  case 0x0f: /* RDE hack */
    if (csp[1] == 0x06)
      /*
	 CLTS - ignore
      */
      { LWORD(eip)+=2;
	break;
      }
      
   if (csp[1] == 0x20)
      /*
    mov ???,cr?? - ignore
      */
      { if (csp[2] == 0xC0) REG(eax)=0;  /* mov eax,cr0 */
        LWORD(eip)+=3;
   break;
      }

  case 0xf0:			/* lock */
  default:
    if (is_rep) fprintf(stderr, "Nope REP F3,CSP = 0x%04x\n", csp[0]);
    /* er, why don't we advance eip here, and
	 why does it work??  */
    REG(eip) = org_eip;
#ifdef USE_MHPDBG
    mhp_debug(DBG_GPF, 0, 0);
#endif
    error("general protection at %p: %x\n", lina,*lina);
    show_regs(__FILE__, __LINE__);
    show_ints(0, 0x33);
    error("ERROR: SIGSEGV, protected insn...exiting!\n");
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

#if 0
#define PFLAG(f)  if (REG(eflags)&(f)) k_printf(#f" ")

  k_printf("FLAGS BEFORE: ");
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
  PFLAG(VIF);
  PFLAG(VIP);
  k_printf(" IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));
#endif

    retval = DO_VM86(&vm86s);

#if 0
  k_printf("FLAGS AFTER: ");
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
  PFLAG(VIF);
  PFLAG(VIP);
  k_printf(" IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));
#endif

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
    if (REG(eflags) & VIF_MASK) {
	if (pic_iflag)
	    pic_sti();
    } else {
	if (!pic_iflag)
	    pic_cli();		/* pic_iflag=0 => enabled */
    }
}
/* @@@ MOVE_END @@@ 49152 */



