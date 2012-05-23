/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif /* USE_MHPDBG */

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */

#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#if X_GRAPHICS
#include <sys/mman.h>           /* root@sjoerd*/
#endif /* X_GRAPHICS */

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
#if X_GRAPHICS
#include "vgaemu.h" /* root@zaphod */
#endif /* X_GRAPHICS */

#include "pic.h"

#include "dpmi.h"

#ifdef USING_NET
#include "ipx.h"
#endif /* USING_NET */

/* Needed for DIAMOND define */
#include "vc.h"

/* #include "sound.h" */

#include "dma.h"


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

void 
dosemu_fault1(
#ifdef __linux__
int signal, struct sigcontext_struct *scp
#endif /* __linux__ */
)
{
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

      case 0x10: /* coprocessor error */
		 pic_request(PIC_IRQ13); /* this is the 386 way of signalling this */
		 return;

      case 0x11: /* alignment check */
		 /*
		  * FIRST thing to do - to avoid being trapped into int0x11
		  * forever, we must clear AC before doing anything else!
		  */
		 __asm__ __volatile__ ("
			pushfl
			popl	%%eax
			andl	$0xfffbffff,%%eax
			pushl	%%eax
			popfl"
			: : : "%eax");
		 /* we are now safe; nevertheless, fall into the default
		  * case and exit dosemu, as an AC fault in vm86 is(?) a
		  * catastrophic failure.
		  */
		 goto sgleave;

      case 0x06: /* invalid_op */
		 dbug_printf("SIGILL while in vm86()\n");
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
		    return (void) do_int(_trapno);
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
 		   return;
 		 }
		 goto sgleave;
#if X_GRAPHICS
      /* We want to protect the video memory and the VGA BIOS */
      case 0x0e:
                if(config.X)
                  {
                    if(VGA_EMU_FAULT(scp,code)==True)
                      return;
                  }
                /* fall into default case if not X */

#endif /* X_GRAPHICS */

      default:	
sgleave:
#if 0
		 error("unexpected CPU exception 0x%02lx errorcode: 0x%08lx while in vm86()\n"
	  	"eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
	  	"cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n", _trapno,
		_err,
	  	_eip, _esp, _eflags, _cs, _ds, _es, _ss);


		 print_exception_info(scp);
#else
		 error("unexpected CPU exception 0x%02lx errorcode: 0x%08lx while in vm86 (DOS)\n",
	  	 _trapno, _err);
		{
		  extern FILE *dbg_fd;
		  int auxg = d.general;
		  FILE *aux = dbg_fd;
		  flush_log();  /* important! else we flush to stderr */
		  dbg_fd = stderr;
		  d.general =1;
		  show_regs(__FILE__, __LINE__);
		  d.general = auxg;
		  flush_log();
		  dbg_fd = aux;
		}
#endif

 		 show_regs(__FILE__, __LINE__);
		 if (d.network)		/* XXX */
		     abort();
 		 leavedos(4);
    }
  }


#if X_GRAPHICS				/* only for debug ?*/ /*root@sjoerd*/
/* The reason it comes here instead of inside_VM86 is
 * char_out() in ./video/int10.c (and the memory is protected).
 * We want to protect the video memory and the VGA BIOS.
 * This function is called from the following functions:
 * dosemu/utilities.c:     char_out(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
 * video/int10.c:    char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
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
        if(VGA_EMU_FAULT(scp,code)==True)
          return;
      }
#endif /* X_GRAPHICS */


  if (in_dpmi)
#ifdef __linux__
    return dpmi_fault(scp);
#endif /* __linux__ */

  csp = (char *) _eip;

  /* This has been added temporarily as most illegal sigsegv's are attempt
     to call Linux int routines */

#if 0
  if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85)) {
#else
  {
#endif /* 0 */
    error("cpu exception in dosemu code outside of VM86()!\n"
	  "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	  "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
	  _trapno, _err, _cr2,
	  _eip, _esp, _eflags, _cs, _ds, _es, _ss);

    print_exception_info(scp);

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

#ifdef __linux__
void dosemu_fault(int signal, struct sigcontext_struct context)
{
    dosemu_fault1 (signal, &context);
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
      csp = (unsigned char *) _eip - 10;
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
	error("@insufficient privelege\n");
      else
	error("@non-available page\n");
      break;

   case 0x10:
      error ("Coprocessor Error:\n");
      break;

    default:
      error("@Unknown exception\n");
      break;
    }
}
