/*
 * $Date: 1995/04/08 22:30:40 $
 * $Source: /usr/src/dosemu0.60/dosemu/RCS/sigsegv.c,v $
 * $Revision: 2.20 $
 * $State: Exp $
 *
 * $Log: sigsegv.c,v $
 * Revision 2.20  1995/04/08  22:30:40  root
 * Release dosemu0.60.0
 *
 * Revision 2.20  1995/04/08  22:30:40  root
 * Release dosemu0.60.0
 *
 * Revision 2.19  1995/02/26  00:54:47  root
 * *** empty log message ***
 *
 * Revision 2.18  1995/02/25  22:37:52  root
 * *** empty log message ***
 *
 * Revision 2.17  1995/02/25  21:52:39  root
 * *** empty log message ***
 *
 * Revision 2.16  1995/02/05  16:52:03  root
 * Prep for Scotts patches.
 *
 * Revision 2.15  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.14  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.13  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.12  1994/10/03  00:24:25  root
 * Checkin prior to pre53_25.tgz
 *
 * Revision 2.11  1994/09/28  00:55:59  root
 * Prep for pre53_23.tgz
 *
 * Revision 2.10  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.9  1994/09/23  01:29:36  root
 * Prep for pre53_21.
 *
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

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */
#ifdef X_SUPPORT
#define XG
#endif

static char rcsid[]="$Id: sigsegv.c,v 2.20 1995/04/08 22:30:40 root Exp $";

#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>
#ifdef XG
#include <sys/mman.h>           /* root@sjoerd*/
#endif
#ifdef __NetBSD__
#include <setjmp.h>
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

#include "video.h"
#ifdef XG
#include "vgaemu.h" /* root@zaphod */
#endif

#include "pic.h"

#include "dpmi.h"

#ifdef USING_NET
#include "ipx.h"
#endif

/* Needed for DIAMOND define */
#include "vc.h"

#include "sound.h"

#include "dma.h"





#ifdef __NetBSD__
#include <machine/segments.h>

extern sigjmp_buf handlerbuf;

void dosemu_fault1(int, int, struct sigcontext *);

void
dosemu_fault(int signal, int code, struct sigcontext *scp)
{
    register unsigned short dsel;
    unsigned long retaddr;
    int jmp = 0;
    if (scp->sc_eflags & PSL_VM) {
	vm86s.substr.regs.vmsc = *scp;
	jmp = 1;
    }

    if (!in_dpmi) {
	asm("pushl %%ds; popl %0" : "=r" (dsel));
	if (dsel & SEL_LDT) {
	    error("ds in LDT!\n");
	    abort();
	}
	asm("pushl %%es; popl %0" : "=r" (dsel));
	if (dsel & SEL_LDT) {
	    error("es in LDT!\n");
	    abort();
	}
	asm("movl %%fs,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("fs in LDT!\n");
	    abort();
	}
	asm("movl %%gs,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("gs in LDT!\n");
	    abort();
	}
	asm("movl %%ss,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("ss in LDT!\n");
	    abort();
	}
	asm("movl %%cs,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("cs in LDT!\n");
	    abort();
	}
    }
    dosemu_fault1(signal, code, scp);
    if (jmp)
	siglongjmp(handlerbuf, VM86_SIGNAL | 0x80000000);
    return;
}
#endif

#ifdef __linux__
#define dosemu_fault1 dosemu_fault
#endif

/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, struct sigcontext_struct);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scaned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */

void 
dosemu_fault1(
#ifdef __linux__
int signal, struct sigcontext_struct context
#endif
#ifdef __NetBSD__
int signal, int code, struct sigcontext *scp
#endif
)
{
#ifdef __linux__
  struct sigcontext_struct *scp = &context;
#endif
  unsigned char *csp;
#if 0
  unsigned char c;
#endif
  int i;

  if (in_vm86) {
    in_vm86 = 0;
#if 0
    write(2, "fault1 sig ", 11);
    c = signal + '0';
    write(2, &c, 1);
    write(2, " code ", 6);
    c = code + '0';
    write(2, &c, 1);
    write(2, " scp ", 5);
    for (i = 7; i >= 0; i--) {
	c = (((long)scp >> (i*4)) & 0xf);
	if (c > 9)
	    c = c + 'a' - 10;
	else
	    c = c + '0';
	write(2, &c, 1);
    }
    write(2, "\n", 1);
#endif
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
#if 0
		 show_regs(__FILE__, __LINE__);
#endif
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
#ifdef XG
      /* We want to protect the video memory and the VGA BIOS */
      case 0x0e:
                if(_trapno!=0x06)   /* original code (case 0x06) fall trough */
                                    /* so we have to do it this way. Maybe */
                                    /* it's better that we just put a break */
                                    /* at 0x06... Maybe we have to rewrite */
                                    /* this... (root@zaphod) */
                if(config.X)
                  {
                    if(vga_emu_fault(&context)==True)
                      return;
                  }

#endif

      default:	error("ERROR: unexpected CPU exception 0x%02lx errorcode: 0x%08lx while in vm86()\n"
	  	"eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
	  	"cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n", _trapno,
		_err,
	  	_eip, _esp, _eflags, _cs, _ds, _es, _ss);
#if 0
		perror("dosemu_fault:");
#endif
 		 show_regs(__FILE__, __LINE__);
		 if (d.network)		/* XXX */
		     abort();
 		 leavedos(4);
    }
  }


#ifdef XG       /* only for debug ?*/ /*root@sjoerd*/
/* The reason it comes here instead of inside_VM86 is
 * char_out() in ./video/int10.c (and the memory is protected).
 * This function is called from the following functions:
 * dosemu/utilities.c:     char_out(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
 * video/int10.c:    char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
 *
 */

  if(_trapno==0x0e)
    if(config.X)
      {
/*
        printf("ERROR: cpu exception in dosemu code outside of VM86()!\n"
               "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
               "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
               "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
               _trapno, scp->err, scp->cr2,
               _eip, _esp, _eflags, _cs, _ds, _es, _ss);
*/
        if(vga_emu_fault(&context)==True)
          return;
      }
#endif


  if (in_dpmi)
#ifdef __linux__
    return dpmi_fault(scp);
#endif
#ifdef __NetBSD__
    return dpmi_fault(scp, code);
#endif

  csp = (char *) _eip;

  /* This has been added temporarily as most illegal sigsegv's are attempt
     to call Linux int routines */

#if 0
  if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85)) {
#else
  {
#endif
    error("ERROR: cpu exception in dosemu code outside of VM86()!\n"
	  "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	  "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
	  _trapno, _err, _cr2,
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

    show_regs(__FILE__, __LINE__);

    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
  }
}
