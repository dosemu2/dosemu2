/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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
#include <limits.h>
#include <getopt.h>

#if X_GRAPHICS
#include <sys/mman.h>           /* root@sjoerd*/
#endif
#ifdef __linux__
#include <sys/vt.h>
#include "Linux/fd.h"
#include "Linux/hdreg.h"
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
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

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

#ifdef USE_SBEMU
#include "sound.h"
#endif

#include "dma.h"

static void callback_return(void);

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

#if defined(X86_EMULATOR) && defined(CPUEMU_DIRECT_IO)
  if (InCompiledCode) {
    prefix66 ^= 1; prefix67 ^= 1;	/* since we come from 32-bit code */
/**/ e_printf("vm86_GP_fault: adjust prefixes to 66=%d,67=%d\n",
	prefix66,prefix67);
  }
#endif
  switch (*csp) {
  
       /* interrupt calls after prefix: we go back to vm86 */
  case 0xcc:    /* int 3       and let it generate an */
  case 0xcd:    /* int         interrupt (don't advance eip) */
  case 0xce:    /* into */
    break;
  case 0xcf:                   /* iret */
    if (prefix67) goto op0ferr; /* iretd */
    break;
  case 0xf1:                   /* int 1 */
    LWORD(eip)++; /* emulated "undocumented" instruction */
    do_int(1);
    break;

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
#if defined(X86_EMULATOR) && defined(SKIP_EMU_VBIOS)
    if ((config.cpuemu>1) && (lina == (unsigned char *) CPUEMUI10_ADD)) {
      e_printf("EMU86: HLT at int10 end\n");
      LWORD(eip) += 1;	/* simply skip, so that we go back to emu mode */
      break;
    }
    else
#endif

    if (lina == (unsigned char *) PIC_ADD) {
      pic_iret();
    }

    else if (lina == (unsigned char *) CBACK_ADD) {
      /* we are back from a callback routine */
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
      if(pic_icount) {
    /* looks like we have failed to catch iret... */
         g_printf("HLT requested with pic_icount=%u: lina=%p!\n",
	    pic_icount, lina);
         show_regs(__FILE__, __LINE__);
      }
      pic_resched();
#if 0
      haltcount++;
      if (haltcount > MAX_HALT_COUNT)
	fatalerr = 0xf4;
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
    set_debug_level('g', 1);
    error("general protection at %p: %x\n", lina,*lina);
    show_regs(__FILE__, __LINE__);
    show_ints(0, 0x33);
    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
  }				/* end of switch() */

#ifdef TRACE_DPMI
  if (debug_level('t')==0)
#endif
  if (LWORD(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs(__FILE__, __LINE__);
  }

}
/* @@@ MOVE_END @@@ 32768 */



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

    in_vm86 = 1;
    retval = DO_VM86(&vm86s);
    in_vm86 = 0;

  /* sync the pic interrupt state with the flags && sync VIF & IF */
    if (_EFLAGS & VIF) {
      set_IF();
    } else {
      clear_IF();
    }

#if 0
    /* This will protect us from Mr.Norton's bugs */
    if (_EFLAGS & (AC|ID)) {
      _EFLAGS &= ~(AC|ID);
      if (debug_level('g')>3)
	dbug_printf("BUG: AC,ID set; flags changed to %08x\n",_EFLAGS);
    }
#endif

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

freeze_idle:
  do_periodic_stuff();

  if (dosemu_frozen) {
    static int minpoll = 0;
    if (!mhpdbg.active) {
      if (!(++minpoll & 7)) usleep(10000);
      g_printf("VM86: freeze: loop\n");
      goto freeze_idle;
    } else {
      unfreeze_dosemu();
    }
  }
}
/* @@@ MOVE_END @@@ 49152 */

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
	++pic_vm86_count;
	run_vm86();
	serial_run();
	dma_run ();
#ifdef USE_SBEMU
	run_sb(); /* Beat Karcher to this one .. 8-) - AM */
#endif
	pic_run();		/* trigger any hardware interrupts requested */
}


static int callback_level = 0;

static void callback_return(void)
{
	callback_level--;
}

/*
 * do_call_back() calls a 16-bit DOS routine with a far call.
 * NOTE: It does _not_ save any of the vm86 registers except old cs:ip !!
 *       The _caller_ has to do this.
 */
static void do_interruptible_call_back(Bit32u codefarptr, int inter)
{
	unsigned char * ssp;
	unsigned long sp;
	Bit16u oldcs, oldip;
	int level;

	if (in_dpmi && !in_dpmi_dos_int) {
		error("do_call_back() cannot call protected mode code\n");
		leavedos(25);
	}

	/* we push the address of our HLT place in the bios
	 * as return address on the stack and run vm86 mode.
	 * When the call returns and HLT causes GPF, vm86_GP_fault() calls
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
		if (fatalerr) leavedos(99);
	/*
	 * BIG WARNING: We should NOT use things like loopstep_run_vm86() here!
	 * Only the plain run_vm86() and run_dpmi() are safe (also DPMI-safe).
	 * -SS
	 */
		if (inter) /* this is essential to do BEFORE run_[vm86|dpmi]() */
			run_irqs();
		if (!in_dpmi)
			run_vm86();
		else
			run_dpmi();
        }
	/* ... and back we are */
	REG(cs) = oldcs;
	LWORD(eip) = oldip;
}

void do_call_back(Bit32u codefarptr)
{
	do_interruptible_call_back(codefarptr, 0);
}

void do_intr_call_back(int intno, int inter)
{
	unsigned char * ssp = (unsigned char *)(LWORD(ss)<<4);
	unsigned long sp = (unsigned long) LWORD(esp);
	pushw(ssp, sp, LWORD(eflags));
	LWORD(esp) = (LWORD(esp) - 2) & 0xffff;
	clear_IF();
	clear_TF();
	clear_AC();
	clear_NT();

	do_interruptible_call_back((ISEG(intno) << 16) + IOFF(intno), inter);
}
