#define SIGSEGV_C 1

/* 
 * $Date: 1994/04/30 01:05:16 $
 * $Source: /home/src/dosemu0.60/RCS/sigsegv.c,v $
 * $Revision: 1.11 $
 * $State: Exp $
 *
 * $Log: sigsegv.c,v $
 * Revision 1.11  1994/04/30  01:05:16  root
 * Clean Up.
 *
 * Revision 1.10  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.9  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.8  1994/04/20  23:43:35  root
 * pre51_8 out the door.
 *
 * Revision 1.7  1994/04/18  20:57:34  root
 * Checkin prior to Jochen's latest patches.
 *
 * Revision 1.6  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.5  1994/04/13  00:07:09  root
 * Lutz's patches
 *
 * Revision 1.4  1994/04/09  18:41:52  root
 * Prior to Lutz's kernel enhancements.
 *
 * Revision 1.3  1994/04/07  00:18:41  root
 * Pack up for pre52_4.
 *
 * Revision 1.2  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.1  1994/03/24  22:40:31  root
 * Initial revision
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>

#include "emu.h"

#include "bios.h"
#include "mouse.h"
#include "serial.h"
#include "xms.h"
#include "timers.h"
#include "cmos.h"

#include "memory.h"
#include "cpu.h"
#include <linux/mm.h>
#include "termio.h"
#include "config.h"
#include "port.h"

/* Needed for DIAMOND define */
#include "video.h"

#ifdef DPMI
#include "dpmi/dpmi.h"
#endif

extern u_char in_sigsegv, in_sighandler, ignore_segv;
extern int fatalerr;

int in_vm86 = 0;
unsigned long RealModeContext;

extern struct config_info config;

#ifdef DPMI
extern int int_queue_running;
extern void dpmi_init(void);
extern void do_dpmi_int(int);
extern print_ldt();
#endif

#include "int.h"
#include "ports.h"

/* this is the brain, the privileged instruction handler.
 * see cpu.h for the SIGSTACK definition (new in Linux 0.99pl10)
 */

struct sigcontext_struct *scp;

inline void vm86_sigsegv()
{
  u_short *ssp = SEG_ADR((us *), ss, sp);

  unsigned char *csp, *lina;
  static int haltcount = 0;
  int i;

#define MAX_HALT_COUNT 3

#if 0
    csp = SEG_ADR((unsigned char *), cs, ip);
    k_printf("SIGSEGV cs=0x%04x ip=0x%04lx *csp=0x%02x->0x%02x 0x%02x 0x%02x\n", LWORD(cs), REG(eip), csp[-1], csp[0], csp[1], csp[2]);
/*
    show_regs();
*/
#endif

  if (ignore_segv) {
    error("ERROR: sigsegv ignored!\n");
    return;
  }

  if (in_sigsegv)
    error("ERROR: in_sigsegv=%d!\n", in_sigsegv);

  in_vm86 = 0;
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
  in_sighandler = 0;

  if (LWORD(eflags) & TF) {
    g_printf("SIGSEGV received while TF is set\n");
    show_regs();
  }

  csp = lina = SEG_ADR((unsigned char *), cs, ip);

  /* fprintf(stderr, "CSP in cpu is 0x%04x\n", *csp); */

  switch (*csp) {

  case 0x62:			/* bound */
    error("ERROR: BOUND instruction");
    show_regs();
    LWORD(eip) += 2;		/* check this! */
    do_int(5);
    break;

  case 0x6c:			/* insb */
  case 0x6d:			/* insw */
  case 0x6e:			/* outsb */
  case 0x6f:			/* outsw */
    error("ERROR: IN/OUT SB/SW: 0x%02x\n", *csp);
    LWORD(eip)++;
    break;

  case 0xe5:			/* inw xx */
    LWORD(eax) = inw((int) csp[1]);
    LWORD(eip) += 2;
    break;
  case 0xe4:			/* inb xx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    LWORD(eip) += 2;
    break;

  case 0xed:			/* inw dx */
    LWORD(eax) = inw(LWORD(edx));
    LWORD(eip) += 1;
    break;
  case 0xec:			/* inb dx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb(LWORD(edx));
    LWORD(eip) += 1;
    break;

  case 0xe7:			/* outw xx */
    outb((int) csp[1], LO(ax));
    outb((int) csp[1] + 1, HI(ax));
    LWORD(eip) += 2;
    break;
  case 0xe6:			/* outb xx */
    outb((int) csp[1], LO(ax));
    LWORD(eip) += 2;
    break;

  case 0xef:			/* outw dx */
    outb(REG(edx), LO(ax));
    outb(REG(edx) + 1, HI(ax));
    LWORD(eip) += 1;
    break;
  case 0xee:			/* outb dx */
    outb(LWORD(edx), LO(ax));
    LWORD(eip) += 1;
    break;

    /* Emulate REP F3 6F */
  case 0xf3:
    fprintf(stderr, "Checking for REP F3 6F\n");
    if ((csp[1] & 0xff) == 0x6f) {
      u_char *si = SEG_ADR((unsigned char *), ds, si);

      fprintf(stderr, "Doing REP F3 6F\n");
      show_regs();
      if (config.chipset == DIAMOND && (LWORD(edx) >= 0x23c0) && (LWORD(edx) <= 0x23cf))
	iopl(3);
      while (LWORD(ecx)) {
	fprintf(stderr, "1- LWORD(ecx)=0x%08x, *si=0x%04x, LWORD(edx)=0x%08x\n", LWORD(ecx), *si, LWORD(edx));
	outb(LWORD(edx), *si);
	si++;
	fprintf(stderr, "2- LWORD(ecx)=0x%08x, *si=0x%04x, LWORD(edx)=0x%08x\n", LWORD(ecx), *si, LWORD(edx) + 1);
	outb(LWORD(edx) + 1, *si);
	si++;
	LWORD(ecx)--;
      }
      if (config.chipset == DIAMOND && (LWORD(edx) >= 0x23c0) && (LWORD(edx) <= 0x23cf))
	iopl(0);
      fprintf(stderr, "Finished REP F3 6F\n");
      LWORD(eip)+= 2;
      LWORD(eflags) |= CF;
      break;
    }
    else
      fprintf(stderr, "Nope CSP[1] = 0x%04x\n", csp[1]);

  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
    if (lina == (unsigned char *) XMSTrap_ADD) {
      LWORD(eip) += 2;		/* skip halt and info byte to point to FAR RET */
      xms_control();
    }
#ifdef DPMI
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    else if (lina == (unsigned char *) (DPMI_ADD + 6)) {
      LWORD(eip) += 1;		/* skip halt to point to FAR RET */
      CARRY;
      if (!in_dpmi)
	dpmi_init();
    }
    else if (lina == (unsigned char *) (DPMI_ADD + 8)) {
      D_printf("DPMI: Return from DOS Interrupt\n");
      if (!((DPMI_SavedDOS_ss) & 0x0004)) {
	/* GTD */
	D_printf("ss in GTD: this should never happen\n");
	ssp = (us *) DPMI_SavedDOS_esp;
      }
      else {
	/* LDT */
	if (Segments[DPMI_SavedDOS_ss>>3].is_32)
	  ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + DPMI_SavedDOS_esp);
	else
	  ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + (DPMI_SavedDOS_esp & 0xffff));
      }
      {
	struct pm86 *dpmir = (struct pm86 *) ssp;
	dpmir->eflags &= ~0xdd5;
	dpmir->eflags |= REG(eflags) & 0xdd5;
	dpmir->eax = REG(eax);
	dpmir->ebx = REG(ebx);
	dpmir->ecx = REG(ecx);
	dpmir->edx = REG(edx);
	dpmir->esi = REG(esi);
	dpmir->edi = REG(edi);
	dpmir->ebp = REG(ebp);
      }
      in_dpmi_dos_int = 0;
    }
    else if (lina == (unsigned char *) (DPMI_ADD + 9)) {
      struct RealModeCallStructure *rmreg = (struct RealModeCallStructure *) RealModeContext;

      D_printf("DPMI: Return from Real Mode Procedure\n");

      rmreg->edi = REG(edi);
      rmreg->edi = REG(esi);
      rmreg->ebp = REG(ebp);
      rmreg->ebx = REG(ebx);
      rmreg->edx = REG(edx);
      rmreg->ecx = REG(ecx);
      rmreg->eax = REG(eax);
      rmreg->flags = LWORD(eflags);
      rmreg->es = REG(es);
      rmreg->ds = REG(ds);
      rmreg->fs = REG(fs);
      rmreg->gs = REG(gs);

      in_dpmi_dos_int = 0;
    }
    else if (lina == (unsigned char *) (DPMI_ADD + 10)) {
      D_printf("DPMI: switching from real to protected mode\n");
      show_regs();
      in_dpmi_dos_int = 0;
      if (DPMIclient_is_32)
	CallToInit(REG(edi), LWORD(esi), REG(ebx), LWORD(edx), LWORD(eax), LWORD(ecx));
      else
	CallToInit(LWORD(edi), LWORD(esi), LWORD(ebx), LWORD(edx), LWORD(eax), LWORD(ecx));
      if (!((DPMI_SavedDOS_ss) & 0x0004)) {
	/* GTD */
	D_printf("ss in GTD: this should never happen\n");
	ssp = (us *) DPMI_SavedDOS_esp;
      }
      else {
	/* LDT */
	if (Segments[DPMI_SavedDOS_ss>>3].is_32)
	  ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + DPMI_SavedDOS_esp);
	else
	  ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + (DPMI_SavedDOS_esp & 0xffff));
      }
      {
        struct pm86 *dpmir = (struct pm86 *) ssp;
	dpmir->eax = dpmir->ebx = dpmir->ecx = dpmir->edx = dpmir->esi = dpmir->edi = 0;
	dpmir->ebp = REG(ebp);
      }
    }
    else if (lina == (unsigned char *) (DPMI_ADD + 11)) {
      D_printf("DPMI: save/restore protected mode registers\n");
	/* must be implemented here */
      REG(eip) += 1;            /* skip halt to point to FAR RET */
    }
#endif
    else {
#if 0
      error("HLT requested: lina=%p!\n", lina);
      show_regs();
      haltcount++;
      if (haltcount > MAX_HALT_COUNT)
	fatalerr = 0xf4;
#endif
      LWORD(eip) += 1;
    }
    break;

  case 0xf0:			/* lock */
  default:
    /* er, why don't we advance eip here, and
	 why does it work??  */
    error("general protection %x\n", *csp);
    show_regs();
    show_ints(0, 0x33);
    error("ERROR: SIGSEGV, protected insn...exiting!\n");
    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
    _exit(1000);
  }				/* end of switch() */

  if (LWORD(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs();
  }

  in_sigsegv--;
  in_sighandler = 0;
}

void 
sigsegv(int signal, struct sigcontext_struct context)
{
  us *ssp;
  unsigned char *csp, *lina;
#ifdef DPMI
  unsigned char *csp2, *ssp2;
#endif
  static int haltcount = 0;
  int i;

scp = &context;

#ifdef DPMI
  if (!in_dpmi) {
#else
  {
#endif
    csp = (char *) (_eip);

    /* This has been added temporarily as most illegal sigsegv's are attempt
   to call Linux int routines */

#if 0
    if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85)) {
#endif
      error("ERROR: NON-VM86 sigsegv!\n"
	    "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
	    "cs: 0x%04lx  ds: 0x%04lx  es: 0x%04lx  ss: 0x%04lx\n",
	    (long)_eip, (long)_esp, (long)_eflags, (long)_cs, (long)_ds, (long)_es, (long)_ss);

      dbug_printf("  VFLAGS(b): ");
      for (i = (1 << 17); i; i >>= 1)
	dbug_printf((_eflags & i) ? "1" : "0");
      dbug_printf("\n");

      dbug_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx"
		  "  VFLAGS(h): %08lx\n",
		  _eax, _ebx, _ecx, _edx, _eflags);
      dbug_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
		  _esi, _edi, _ebp);
      dbug_printf("CS: %04lx  DS: %04lx  ES: %04lx  FS: %04lx  GS: %04lx\n",
		  (long)_cs, (long)_ds, (long)_es, (long)_fs, (long)_gs);

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
      PFLAG(VIF);
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
      dbug_printf("OPS  : ");
      csp = (unsigned char *) _eip - 10;
      for (i = 0; i < 10; i++)
	dbug_printf("%02x ", *csp++);
      dbug_printf("-> ");
      for (i = 0; i < 10; i++)
	dbug_printf("%02x ", *csp++);
      dbug_printf("\n");

#if 0
    }
#endif
  }

#ifdef DPMI
  if (in_dpmi) {
#if 1
    D_printf("DPMI: Protected Mode Interrupt!\n");
    DPMI_show_state;
#endif /* 1 */
    if (!((_cs) & 0x0004)) {
      /* GTD */
      D_printf("cs in GTD: this should never happen\n");
      csp = (char *) _eip;
    } else {
      /* LDT */
      csp = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip );
    }
    if (!((_ss) & 0x0004)) {
      /* GTD */
      D_printf("ss in GTD: this should never happen\n");
      ssp = (us *) _esp;
    } else {
      /* LDT */
      if (Segments[_ss>>3].is_32)
	ssp = (us *) (GetSegmentBaseAddress(_ss) + _esp );
      else
	ssp = (us *) (GetSegmentBaseAddress(_ss) + _LWORD(esp) );
    }

    switch (*csp++) {

    case 0xcd:			/* int xx */
      /* Bypass the int instruction */
      _eip += 2;
      if (Interrupt_Table[*csp].selector == DPMI_SEL)
	do_dpmi_int(*csp);
      else {
	/* hope that 32Bit Clients don't use initial (16Bit) CS for interrupthandler */
	if (DPMIclient_is_32) {
	  *(--((unsigned long *) ssp)) = _eflags;
	  *--ssp = (us) 0;
	  *--ssp = _cs;
	  *(--((unsigned long *) ssp)) = _eip;
	  _esp -= 12;
	} else {
	  *--ssp = _LWORD(efl);
	  *--ssp = _cs;
	  *--ssp = _LWORD(eip);
	  _esp -= 6;
	}
	_cs = Interrupt_Table[*csp].selector;
	_eip = Interrupt_Table[*csp].offset;
	D_printf("DPMI: calling interrupthandler 0x%02x at 0x%04x:0x%08lx\n", *csp, _cs, _eip);
      }
      if (in_dpmi_dos_int || int_queue_running) {
        *--ssp = (us) 0;
        *--ssp = (us) _cs;
        *(--((unsigned long *) ssp)) = _eip;
        _esp -= 8;
        _cs = UCODESEL;
        _eip = (unsigned long) ReturnFrom_dpmi_control;
      }

      break;

    case 0xf4:			/* hlt */
      _eip += 1;
      if (_cs==DPMI_SEL)
	if (_eip==DPMI_OFF+11) {
	  D_printf("DPMI: switching from protected to real mode\n");
	  REG(ds) = (long) _LWORD(eax);
	  REG(es) = (long) _LWORD(ecx);
	  REG(ss) = (long) _LWORD(edx);
	  REG(esp) = (long) _LWORD(ebx);
	  REG(cs) = (long) _LWORD(esi);
	  REG(eip) = (long) _LWORD(edi);
	  REG(ebp) = _ebp;
	  in_dpmi_dos_int = 1;
	  _cs = UCODESEL;
	  _eip = (unsigned long) ReturnFrom_dpmi_control;
        } else if (_eip==DPMI_OFF+12) {
	  struct vm86_regs *buffer;
	  if (!((_es) & 0x0004)) {
	    /* GTD */
	    buffer = (struct vm86_regs *) _edi;
	  }
	  else {
	    /* LDT */
	    if (Segments[_es>>3].is_32)
	      buffer = (struct vm86_regs *) (GetSegmentBaseAddress(_es) + _edi);
	    else
	      buffer = (struct vm86_regs *) (GetSegmentBaseAddress(_es) + _LWORD(edi));
	  }
	  if (_LO(ax)==0) {
            D_printf("DPMI: save real mode registers\n");
	    memcpy(buffer, &vm86s, sizeof(struct vm86_regs));
	  } else {
            D_printf("DPMI: restore real mode registers\n");
	    memcpy(&vm86s, buffer, sizeof(struct vm86_regs));
	   }
          /* _eip point to FAR RET */
        } else if (_eip==DPMI_OFF+14) {
          D_printf("DPMI: extension API call: 0x%04x\n", _LWORD(eax));
          if (_LWORD(eax) == 0x0100) {
            _eax = LDT_ALIAS;  /* simulate direct ldt access */
            *ssp += (us) 7; /* jump over writeaccess test of the LDT in krnl386.exe! */
	  } else
            _eflags |= CF;
        } else if (_eip==DPMI_OFF+15) {
	  D_printf("DPMI: default exception handler called\n");
	  goto leavedpmi;
	} else if (_eip==DPMI_OFF+16) {
	  D_printf("DPMI: default protected mode interrupthandler called\n");
	  /* hope that 32Bit Clients don't use initial (16Bit) CS for interrupthandler */
	  if (DPMIclient_is_32) {
	    _eflags = *(((unsigned long *) ssp)++);
	    ssp++;
	    _cs = *ssp++;
	    _eip = *((unsigned long *) ssp);
	    _esp += 12;
	  } else {
	    _LWORD(efl) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eip) = *ssp;
	    _esp += 6;
	  }
	  if (!((_cs) & 0x0004)) {
	    /* GTD */
	    D_printf("cs in GTD: this should never happen\n");
	    csp2 = (char *) _eip - 1;
	  } else {
	    /* LDT */
	    csp2 = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip - 1);
	  }
	  do_dpmi_int(*csp2);
	}
      else
	return;
      break;

      case 0xfa:                        /* cli */
	REG(eflags) &= ~VIF;
        _eip += 1;
        break;
 
      case 0xfb:                        /* sti */
	REG(eflags) |= VIF;
        _eip += 1;
        break;

    default:
      csp--;
      if ((*(csp-2)==0xcd) && (*(csp-1)==0x80)) {
	error("DPMI: error in syscall (int 0x80) at %4x:%8lx\n", (us)_cs, (long)_eip - 2);
        leavedos(fatalerr);	/* shouldn't return */
        _exit(1000);
      } 
      /* Simulate direct LDT access for MS-Windows 3.1 */
      if (win31ldt(csp))
      return;

      error("DPMI: Unexpected Protected Mode segfault %x\n", *csp);
leavedpmi:
#if 1
      p_dos_str("DPMI: Unexpected Protected Mode segfault %02lx at %04lx:%08lx\n",
              (long)*csp, (long)_cs, (long)_eip);
      p_dos_str("Terminating Client\n");
      REG(eax) = 0x4cff;
      REG(cs) = DPMI_SEG;
      REG(eip) = DPMI_OFF + 8;
      print_ldt();
      in_dpmi_dos_int = 1;
      do_int(0x21);
#else
      leavedos(fatalerr);	/* shouldn't return */
      _exit(1000);
#endif
    }				/* end of switch() */
    return;
  }
#endif /* DPMI */
}

#undef SIGSEGV_C
