#define SIGSEGV_C 1

/* 
 * $Date: 1994/09/11 01:01:23 $
 * $Source: /home/src/dosemu0.60/RCS/sigsegv.c,v $
 * $Revision: 2.8 $
 * $State: Exp $
 *
 * $Log: sigsegv.c,v $
 * Revision 2.8  1994/09/11  01:01:23  root
 * Prep for pre53_19.
 *
 * Revision 2.7  1994/08/14  02:52:04  root
 * Rain's latest CLEANUP and MOUSE for X additions.
 *
 * Revision 2.6  1994/08/05  22:29:31  root
 * Prep dir pre53_10.
 *
 * Revision 2.5  1994/08/01  14:26:23  root
 * Prep for pre53_7  with Markks latest, EMS patch, and Makefile changes.
 *
 * Revision 2.4  1994/07/09  14:29:43  root
 * prep for pre53_3.
 *
 * Revision 2.3  1994/06/27  02:15:58  root
 * Prep for pre53
 *
 * Revision 2.2  1994/06/24  14:51:06  root
 * Markks's patches plus.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.20  1994/06/03  00:58:55  root
 * pre51_23 prep, Daniel's fix for scrbuf malloc().
 *
 * Revision 1.19  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.18  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.17  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.16  1994/05/16  23:13:23  root
 * Prep for pre51_16.
 *
 * Revision 1.15  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.14  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.13  1994/05/04  22:16:00  root
 * Patches by Alan to mouse subsystem.
 *
 * Revision 1.12  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
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
#include "termio.h"
#include "config.h"
#include "port.h"

/* Needed for IPX support */
#include "ipx.h"

/* Needed for DIAMOND define */
#include "video/vc.h"

#ifdef DPMI
#include "dpmi/dpmi.h"
#endif

extern u_char in_sigsegv, in_sighandler, ignore_segv;

int in_vm86 = 0;

extern struct config_info config;

#include "hgc.h"

#if 0 /* 94/05/26 */
#include "int.h"
#endif
#include "ports.h"

extern inline void do_int(int);

/*
 * DANG_BEGIN_FUNCTION void vm86_GP_fault();
 *
 * All from the kernel unhandled general protection faults from V86 mode
 * are handled here. This are mainly port IO and the HLT instruction.
 *
 * DANG_END_FUNCTION
 */

inline void vm86_GP_fault()
{

  unsigned char *csp, *lina;

#if 0
  u_short *ssp = SEG_ADR((us *), ss, sp);
  static int haltcount = 0;
#endif

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

  /* in_vm86 = 0; */
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
    else if ((lina >=(unsigned char *)DPMI_ADD) &&
	(lina <(unsigned char *)(DPMI_ADD+(unsigned long)DPMI_dummy_end-(unsigned long)DPMI_dummy_start)))
    {
      dpmi_realmode_hlt(lina);
      break;
    }
#endif /* DPMI */
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
  }				/* end of switch() */

  if (LWORD(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs();
  }

  in_sigsegv--;
  in_sighandler = 0;
}

/*
 * DANG_BEGIN_FUNCTION void dosemu_fault(int, struct sigcontext_struct);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scaned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */

void 
dosemu_fault(int signal, struct sigcontext_struct context)
{
  struct sigcontext_struct *scp = &context;
  unsigned char *csp;
  int i;

  if (in_vm86) {
    in_vm86 = 0;
    switch (_trapno) {
      case 0x00: /* divide_error */
      case 0x01: /* debug */
      case 0x03: /* int3 */
      case 0x04: /* overflow */
      case 0x05: /* bounds */
      case 0x07: /* device_not_available */
		 return (void) do_int(_trapno);
      case 0x06: /* invalid_op */
		 dbug_printf("SIGILL while in vm86()\n");
		 show_regs();
 		 csp = SEG_ADR((unsigned char *), cs, ip);
 		 /* Some db commands start with 2e (use cs segment) and thus is accounted
 		    for here */
 		 if (csp[0] == 0x2e) {
 		   csp++;
 		   LWORD(eip)++;
 		 }
 		 if (csp[0] == 0xf0) {
 		   dbug_printf("ERROR: LOCK prefix not permitted!\n");
 		   LWORD(eip)++;
 		   return;
 		 }
      default:	error("ERROR: unexpected CPU exception 0x%02lx while in vm86()\n"
	  	"eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
	  	"cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n", _trapno,
	  	_eip, _esp, _eflags, _cs, _ds, _es, _ss);
		perror("YUCK");
 		 show_regs();
 		 leavedos(4);
    }
  }

#ifdef DPMI
  if (in_dpmi)
    return dpmi_fault(&context);
#endif

  csp = (char *) _eip;

  /* This has been added temporarily as most illegal sigsegv's are attempt
     to call Linux int routines */

#if 0
  if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85)) {
#else
  {
#endif
    error("ERROR: cpu exception in dosemu code!\n"
	  "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	  "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
	  _trapno, scp->err, scp->cr2,
	  _eip, _esp, _eflags, _cs, _ds, _es, _ss);

    dbug_printf("  VFLAGS(b): ");
    for (i = (1 << 17); i; i >>= 1)
      dbug_printf((_eflags & i) ? "1" : "0");
    dbug_printf("\n");

    dbug_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx"
		"  VFLAGS(h): %08lx\n",
		_eax, _ebx, _ecx, _edx, _eflags);
    dbug_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
		_esi, _edi, _ebp);
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
    dbug_printf("OPS  : ");
    csp = (unsigned char *) _eip - 10;
    for (i = 0; i < 10; i++)
      dbug_printf("%02x ", *csp++);
    dbug_printf("-> ");
    for (i = 0; i < 10; i++)
      dbug_printf("%02x ", *csp++);
    dbug_printf("\n");

    show_regs();

    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
  }
}

#undef SIGSEGV_C
