/* CPU/V86 support for dosemu
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/03/18 23:17:51 $
 * $Source: /home/src/dosemu0.50pl1/RCS/cpu.c,v $
 * $Revision: 1.29 $
 * $State: Exp $
 *
 * $Log: cpu.c,v $
 * Revision 1.29  1994/03/18  23:17:51  root
 * Some DPMI, some prep for 0.50pl1.
 *
 * Revision 1.28  1994/03/15  01:38:20  root
 * DPMI,serial, other changes.
 *
 * Revision 1.27  1994/03/14  00:35:44  root
 * Unsucessful speed enhancements
 *
 * Revision 1.26  1994/03/13  21:52:02  root
 * More speed testing :-(
 *
 * Revision 1.25  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.24  1994/03/10  23:52:52  root
 * Lutz DPMI patches
 *
 * Revision 1.23  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.22  1994/03/04  00:01:58  root
 * Readying for 0.50
 *
 * Revision 1.21  1994/02/21  20:28:19  root
 * DPMI update by Lutz.
 *
 * Revision 1.20  1994/02/09  20:10:24  root
 * Added dosbanner config option for optionally displaying dosemu bannerinfo.
 * Added allowvideportaccess config option to deal with video ports.
 *
 * Revision 1.19  1994/02/05  21:45:55  root
 * Changed all references within ssp's from !REG to !LWORD.
 *
 * Revision 1.18  1994/01/27  19:43:54  root
 * Preparing for IPX of Tim's.
 *
 * Revision 1.17  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout.
 *
 * Revision 1.16  1994/01/20  21:14:24  root
 * Indent. Added SS overflow check to show_regs().
 *
 * Revision 1.15  1994/01/19  17:51:14  root
 * Added call in SIGILL to check if illegal op-code is just another hard
 * interrupt ending. It calls int_queue_run() and sees if such was the case.
 * Added more checks for SS being improperly addressed.
 * Added WORD macro in push/pop functions.
 * Added check for DPMI far call.
 *
 * Revision 1.14  1994/01/03  22:19:25  root
 * Fixed problem popping from stack not acking word ss.
 *
 * Revision 1.13  1994/01/01  17:06:19  root
 * Attempt to fix EMS stacking ax problem.
 *
 * Revision 1.12  1993/12/30  11:18:32  root
 * Diamond Fixes.
 *
 * Revision 1.11  1993/12/27  19:06:29  root
 * Tracking down SIGFPE errors from mft.exe divide overflow.
 *
 * Revision 1.10  1993/12/22  11:45:36  root
 * Added more debug
 *
 * Revision 1.9  1993/12/05  20:59:03  root
 * Diamond Card testing
 *
 * Revision 1.8  1993/11/30  22:21:03  root
 * Final Freeze for release pl3
 *
 * Revision 1.7  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.6  1993/11/29  22:44:11  root
 * Prepare for pl3 release
 *
 * Revision 1.5  1993/11/29  00:05:32  root
 * Add code to start on Diamond card, get rid (temporarily) of sig cd,80,85
 *
 * Revision 1.4  1993/11/25  22:45:21  root
 * About to destroy keybaord routines.
 *
 * Revision 1.3  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
 * Revision 1.2  1993/11/15  19:56:49  root
 * Fixed sp -> ssp overflow, it is a hack at this time, but it works.
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.3  1993/07/21  01:52:44  rsanders
 * implemented {push,pop}_{byte,word,long}, and push_ifs()/
 * pop_ifs() for playing with interrupt stack frames.
 *
 * Revision 1.2  1993/07/13  19:18:38  root
 * changes for using the new (0.99pl10) signal stacks
 *
 *
 */
#define CPU_C

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "memory.h"
#include "cpu.h"
#include <linux/mm.h>
#include "termio.h"
#include "emu.h"
/* Needed for DIAMOND define */
#include "video.h"
#ifdef DPMI
#include "dpmi/dpmi.h"
#endif

extern void queue_hard_int(int i, void (*), void (*));
extern inline void do_int(int);
extern int int_queue_running;
extern void int_queue_run();
extern int inb(int);
extern void outb(int, u_char);
extern void iopl(int);
extern void xms_control(void);
extern void dpmi_init(void);
extern print_ldt();

struct vm86_struct vm86s;
struct CPU cpu;

struct vec_t orig[256];		/* "original" interrupt vectors */
struct vec_t snapshot[256];	/* vectors from last snapshot */

extern u_char in_sigsegv, in_sighandler, ignore_segv;
extern int fatalerr;

int in_vm86 = 0;
unsigned long RealModeContext;

extern struct config_info config;

inline void
update_cpu(long flags)
{
  cpu.iflag = (flags & IF) ? 1 : 0;
  cpu.nt = (flags & NT) ? 1 : 0;
  cpu.ac = (flags & AC) ? 1 : 0;

  cpu.iopl = flags & IOPL_MASK;
}

inline void
update_flags(long *flags)
{
#define SETFLAG(bit)  (*flags |=  (bit))
#define CLRFLAG(bit)  (*flags &= ~(bit))

  cpu.iflag ? SETFLAG(IF) : CLRFLAG(IF);

  /* on a 386, the AC (alignment check) bit doesn't exist */
  if (cpu.type == CPU_486 && cpu.ac)
    SETFLAG(AC);
  else
    CLRFLAG(AC);

  *flags &= ~IOPL_MASK;		/* clear IOPL */

  /* the 80286 allowed one to set bit 15 of the flags register,
   * (so says HelpPC 2.1...but INFOPLUS uses the test below)
   * 80286 in real mode keeps the upper 4 bits of FLAGS zeroed.  this
   * test works with the programs I've tried, while setting bit15
   * does NOT.
   */
  if (cpu.type == CPU_286)
    *flags &= ~0xfffff000;
  else {
    *flags |= cpu.iopl;
    cpu.nt ? SETFLAG(NT) : CLRFLAG(NT);
  }
}

void
cpu_init(void)
{
  /* cpu.type set in emulate() via getopt */
  cpu.iflag = 1;
  cpu.nt = 0;
  cpu.iopl = 0;
  cpu.ac = 0;
  cpu.sti = 1;

  update_flags(&REG(eflags));

  /* make ivecs array point to low page (real mode IDT) */
  ivecs = 0;
}

void
show_regs(void)
{
  int i;
  unsigned char *sp;
  unsigned char *cp = SEG_ADR((unsigned char *), cs, ip);
  unsigned long vflags = REG(eflags);

  if (!LWORD(esp))
    sp = (SEG_ADR((u_char *), ss, sp)) + 0x8000;
  else
    sp = SEG_ADR((u_char *), ss, sp);

  /* adjust vflags with our virtual IF bit (iflag) */
  update_flags(&vflags);

  dbug_printf("\nEIP: %04x:%08lx", LWORD(cs), REG(eip));
  dbug_printf(" ESP: %04x:%08lx", LWORD(ss), REG(esp));
  dbug_printf("         VFLAGS(b): ");
  for (i = (1 << 0x11); i > 0; i = (i >> 1))
    dbug_printf((vflags & i) ? "1" : "0");

  dbug_printf("\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx VFLAGS(h): %08lx",
	      REG(eax), REG(ebx), REG(ecx), REG(edx), vflags);
  dbug_printf("\nESI: %08lx EDI: %08lx EBP: %08lx",
	      REG(esi), REG(edi), REG(ebp));
  dbug_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
	      LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs));

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (vflags&(f)) dbug_printf(#f" ")

  dbug_printf("FLAGS: ");
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
  dbug_printf(" IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  dbug_printf("STACK: ");
  sp -= 10;
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *sp++);
  dbug_printf("-> ");
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *sp++);
  dbug_printf("\n");
  dbug_printf("OPS  : ");
  cp -= 10;
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("-> ");
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("\n");
}

void
show_ints(int min, int max)
{
  int i, b;

  max = (max - min) / 3;
  for (i = 0, b = min; i <= max; i++, b += 3) {
    dbug_printf("%02x| %04x:%04x->%05x    ", b, ISEG(b), IOFF(b),
		IVEC(b));
    dbug_printf("%02x| %04x:%04x->%05x    ", b + 1, ISEG(b + 1), IOFF(b + 1),
		IVEC(b + 1));
    dbug_printf("%02x| %04x:%04x->%05x\n", b + 2, ISEG(b + 2), IOFF(b + 2),
		IVEC(b + 2));
  }
}

inline int
do_hard_int(int intno)
{
  queue_hard_int(intno, NULL, NULL);
  return (1);
}

inline int
do_soft_int(int intno)
{
  do_int(intno);
  return 1;
}

inline void
do_sti(void)
{
  cpu.sti = 1;
  REG(eflags) |= TF;
}

#ifdef DPMI
#define DPMI_show_state \
    D_printf("eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n" \
	     "cs: 0x%04lx  ds: 0x%04lx  es: 0x%04lx  ss: 0x%04lx\n", \
	     (long)_eip, (long)_esp, (long)_eflags, (long)_cs, (long)_ds, (long)_es, (long)_ss); \
    D_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx  EFLAG: %08lx\n", \
	     (long)_eax, (long)_ebx, (long)_ecx, (long)_edx, (long)_eflags); \
    D_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n", \
	     (long)_esi, (long)_edi, (long)_ebp); \
    D_printf("CS: %04lx  DS: %04lx  ES: %04lx  FS: %04lx  GS: %04lx\n", \
	     (long)_cs, (long)_ds, (long)_es, (long)_fs, (long)_gs); \
    /* display the 10 bytes before and after CS:EIP.  the -> points \
     * to the byte at address CS:EIP \
     */ \
    if (!((_cs) & 0x0004)) { \
      /* GTD */ \
      csp2 = (unsigned char *) _eip - 10; \
    } \
    else { \
      /* LDT */ \
      csp2 = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip) - 10; \
    } \
    D_printf("OPS  : "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *csp2++); \
    D_printf("-> "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *csp2++); \
    D_printf("\n"); \
    if (!((_ss) & 0x0004)) { \
      /* GTD */ \
      ssp2 = (unsigned char *) _esp - 10; \
    } \
    else { \
      /* LDT */ \
      if (Segments[_ss>>3].is_32) \
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _esp ) - 10; \
      else \
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + (_esp & 0x0000ffff) ) - 10; \
    } \
    D_printf("STACK: "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *ssp2++); \
    D_printf("-> "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *ssp2++); \
    D_printf("\n");
#endif /*DPMI*/

/* this is the brain, the privileged instruction handler.
 * see cpu.h for the SIGSTACK definition (new in Linux 0.99pl10)
 */

struct sigcontext_struct *scp;

void 
sigsegv(int signal, struct sigcontext_struct context)
{
  us *ssp;
  unsigned char *csp, *lina;
#ifdef DPMI
  unsigned char *csp2, *ssp2;
#endif
  static int haltcount = 0;
  int op32 = 0, ad32 = 0, i;

scp = &context;

#define MAX_HALT_COUNT 1

#if 0
  csp = SEG_ADR((unsigned char *), cs, ip);
  k_printf("SIGSEGV *csp=0x%02x 0x%02x->0x%02x 0x%02x\n", csp[-1], csp[0], csp[1], csp[2]);
/*
  show_regs();
*/
#endif

  if (ignore_segv) {
    error("ERROR: sigsegv ignored!\n");
    return;
  }

#ifdef DPMI
  if (!in_vm86 && !in_dpmi) {
#else
  if (!in_vm86) {
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

#if 0
    }
#endif
  }

#ifdef DPMI
  if (!in_vm86 && in_dpmi) {
#if 1
    D_printf("DPMI: Protected Mode Interrupt!\n");
    DPMI_show_state;
#endif /* 1 */
    if (!((_cs) & 0x0004)) {
      /* GTD */
      D_printf("cs in GTD: this should never happen\n");
      csp = (char *) _eip;
    }
    else {
      /* LDT */
      csp = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip );
    }

    if (!((_ss) & 0x0004)) {
      /* GTD */
      D_printf("ss in GTD: this should never happen\n");
      ssp = (us *) _esp;
    }
    else {
      /* LDT */
      if (Segments[_ss>>3].is_32)
	ssp = (us *) (GetSegmentBaseAddress(_ss) + _esp );
      else
	ssp = (us *) (GetSegmentBaseAddress(_ss) + (_esp & 0x0000ffff) );
    }

    switch (*csp++) {

    case 0xcd:			/* int xx */
      /* Bypass the int instruction */
      _eip += 2;
      if ((*csp == 0x2f) && (_LWORD(eax) == 0x168a))
	do_int31(0x0a00);
      else {
        if (*csp == 0x31) {
	  D_printf("DPMI: int31, eax=%04x\n",(int)_eax);
	  do_int31(_LWORD(eax));
        } else {
	  REG(eflags) = _eflags;
	  REG(eax) = _eax;
	  REG(ebx) = _ebx;
	  REG(ecx) = _ecx;
	  REG(edx) = _edx;
	  REG(esi) = _esi;
	  REG(edi) = _edi;
	  REG(ds) = (long) GetSegmentBaseAddress(_ds) >> 4;
	  REG(es) = (long) GetSegmentBaseAddress(_es) >> 4;
	  REG(cs) = DPMI_SEG;
	  REG(eip) = DPMI_OFF + 8;
	  in_dpmi_dos_int = 1;
	  do_int(*csp);
        }
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
	  in_dpmi_dos_int = 1;
        } else if (_eip==DPMI_OFF+12) {
          D_printf("DPMI: protected mode Save/Restore\n");
          /* _eip point to FAR RET */
        } else if (_eip==DPMI_OFF+14) {
          D_printf("DPMI: extension API call: 0x%04x\n", _LWORD(eax));
          if (_LWORD(eax) == 0x0100) {
            _eax = LDT_ALIAS;  /* simulate direct ldt access */
            *ssp += (us) 7; /* jump over writeaccess test of the LDT in krnl386.exe! */
	  } else
            _eflags |= CF;
          }
      else
	return;
      break;

      case 0xfa:                        /* cli */
        _eip += 1;
        break;
 
      case 0xfb:                        /* sti */
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

/*    D_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx\n",
	     _eax, _ebx, _ecx, _edx, _eflags);
    D_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
	     _esi, _edi, _ebp);
    D_printf("CS: %04lx  DS: %04lx  ES: %04lx  FS: %04lx  GS: %04lx\n",
	     _cs, _ds, _es, _fs, _gs); */
    if (!in_dpmi_dos_int)
      return;
    *--ssp = (us) 0;
    *--ssp = (us) _cs;
    *--ssp = (us) _HWORD(eip);
    *--ssp = (us) _LWORD(eip);
    _esp -= 8;
    _cs = UCODESEL;
    _eip = (unsigned long) ReturnFrom_dpmi_control;
    return;
  }
#endif /* DPMI */

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
    g_printf("SIGSEGV %d received\n", signal);
    show_regs();
  }

restart_segv:
  csp = lina = SEG_ADR((unsigned char *), cs, ip);

  /* fprintf(stderr, "CSP in cpu is 0x%04x\n", *csp); */

  switch (*csp) {

  case 0xcd:			/* int xx */
    LWORD(eip) += 2;
    do_int((int) *++csp);
    break;

  case 0xcf:			/* iret */
    if (!LWORD(esp))
      ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
    else
      ssp = SEG_ADR((us *), ss, sp);
    LWORD(eip) = *ssp++;
    LWORD(cs) = *ssp++;
    LWORD(eflags) = *ssp++;
    LWORD(esp) += 6;

    /* make sure our "virtual flags" correspond to the
       * popped eflags
    */
    update_cpu(REG(eflags));
    break;

  case 0x9c:			/* pushf */
    /* fetch virtual CPU flags into eflags */
    update_flags(&REG(eflags));
    if (op32) {
      unsigned long *lssp;

      op32 = 0;
      if (!LWORD(esp))
	lssp = (SEG_ADR((unsigned long *), ss, sp)) + 0x8000;
      else
	lssp = SEG_ADR((unsigned long *), ss, sp);
      *--lssp = (unsigned long) REG(eflags);
      LWORD(esp) -= 4;
      LWORD(eip) += 1;
    }
    else {
      if (!LWORD(esp))
	ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
      else
	ssp = SEG_ADR((us *), ss, sp);
      *--ssp = LWORD(eflags);
      LWORD(esp) -= 2;
      LWORD(eip) += 1;
    }
    break;

  case 0x62:			/* bound */
    error("ERROR: BOUND instruction");
    show_regs();
    LWORD(eip) += 2;		/* check this! */
    do_int(5);
    break;

  case 0x66:			/* 32-bit operand prefix */
    op32 = 1;
    LWORD(eip)++;
    goto restart_segv;
    break;
  case 0x67:			/* 32-bit address prefix */
    ad32 = 1;
    LWORD(eip)++;
    goto restart_segv;
    break;

  case 0x6c:			/* insb */
  case 0x6d:			/* insw */
  case 0x6e:			/* outsb */
  case 0x6f:			/* outsw */
    error("ERROR: IN/OUT SB/SW: 0x%02x\n", *csp);
    LWORD(eip)++;
    break;

  case 0xcc:			/* int 3 */
    LWORD(eip) += 1;
    do_int(3);
    break;

  case 0xe5:			/* inw xx */
    LWORD(eax) &= ~0xff00;
    LWORD(eax) |= inb((int) csp[1] + 1) << 8;
  case 0xe4:			/* inb xx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    LWORD(eip) += 2;
    break;

  case 0xed:			/* inw dx */
    LWORD(eax) &= ~0xff00;
    LWORD(eax) |= inb(REG(edx) + 1) << 8;
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

  case 0xfa:			/* cli */
    cpu.iflag = 0;
    LWORD(eip) += 1;
    break;
  case 0xfb:			/* sti */
#ifdef PROPER_STI
    do_sti();
#else
    cpu.iflag = 1;
#endif
    LWORD(eip) += 1;
    break;
  case 0x9d:			/* popf */
    if (op32) {
      unsigned long *lssp;

      op32 = 0;
      if (!LWORD(esp))
	lssp = (SEG_ADR((unsigned long *), ss, sp)) + 0x8000;
      else
	lssp = SEG_ADR((unsigned long *), ss, sp);
      REG(eflags) = *lssp;
      LWORD(esp) += 4;
    }
    else {
      if (!LWORD(esp))
	ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
      else
	ssp = SEG_ADR((us *), ss, sp);
      LWORD(eflags) = *ssp;
      LWORD(esp) += 2;
    }
    LWORD(eip) += 1;
    /* make sure our "virtual flags" correspond to the
       * popped eflags
       */
    update_cpu(REG(eflags));
    break;

  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
    if (lina == (unsigned char *) XMSTrap_ADD) {
      LWORD(eip) += 2;		/* skip halt and info byte to point to FAR RET */
      xms_control();
    }
#if BIOSSEG != 0xf000
    else if ((LWORD(cs) & 0xffff) == 0xf000) {
      /* if BIOSSEG = 0xf000, then jumps here will be legit */
      error("jump into BIOS %04x:%04x...simulating IRET\n",
	    LWORD(cs), LWORD(eip));
      /* simulate IRET */
      if (!LWORD(esp))
	ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
      else
	ssp = SEG_ADR((us *), ss, sp);
      LWORD(eip) = *ssp++;
      LWORD(cs) = *ssp++;
      LWORD(eflags) = *ssp++;
      LWORD(esp) += 6;
    }
#endif
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
	if (Segments[_ss>>3].is_32)
	  ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + DPMI_SavedDOS_esp);
	else
	  ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + (DPMI_SavedDOS_esp & 0xffff));
      }
      {
	struct pm86 *dpmir = (struct pm86 *) ssp;
	dpmir->eflags = REG(eflags);
	dpmir->eax = REG(eax);
	dpmir->ebx = REG(ebx);
	dpmir->ecx = REG(ecx);
	dpmir->edx = REG(edx);
	dpmir->esi = REG(esi);
	dpmir->edi = REG(edi);
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
      in_dpmi_dos_int = 0;
      CallToInit(REG(edi), LWORD(esi), REG(ebx), LWORD(edx), LWORD(eax), LWORD(ecx));
    }
    else if (lina == (unsigned char *) (DPMI_ADD + 11)) {
      D_printf("DPMI: real mode Save/Restore\n");
      REG(eip) += 1;            /* skip halt to point to FAR RET */
    }
#endif
    else {
      error("HLT requested: lina=%p!\n", lina);
      show_regs();
      haltcount++;
      if (haltcount > MAX_HALT_COUNT)
	fatalerr = 0xf4;
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

#ifndef PROPER_STI		/* do_sti() sets TF bit */
  if (LWORD(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs();
  }
#endif

  in_sigsegv--;
  in_sighandler = 0;
}

void
sigill(int sig, struct sigcontext_struct context)
{
  unsigned char *csp;
  int i, dee;
#ifdef DPMI
  struct sigcontext_struct *scp = &context;
  unsigned char *csp2, *ssp2;

  if (_cs != UCODESEL){
    D_printf("DPMI: sigill\n");
    DPMI_show_state;
  }
#endif

  if (!in_vm86)			/* test VM bit */
    error("ERROR: NON-VM86 illegal insn!\n");

  in_vm86 = 0;

  error("SIGILL %d received\n", sig);
  show_regs();
  csp = SEG_ADR((unsigned char *), cs, ip);
  /*
   O.K., we seem to be getting here because to hard ints are stacked.
   Therefore when the first is popped, the second points to instr. 0xff
   So here we run int_queue_run() again to clean up :-).
*/
  if (*csp == 0xff && int_queue_running) {
    int_queue_run();
    if (csp != SEG_ADR((unsigned char *), cs, ip)) {
      show_regs();
      fprintf(stderr, "Returning from 0xff\n");
      return;
    }
  }

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

  /* look at this with Checkit...the new code just sits in a loop */
#define OLD_MATH_CODE
#ifdef OLD_MATH_CODE
  i = (csp[0] << 8) + csp[1];	/* swapped */
  if ((i & 0xf800) != 0xd800) {	/* not FPU insn */
    error("ERROR: not an FPU instruction, real illegal opcode!\n");
#if 0
    do_int(0x10);		/* Coprocessor error */
#else
    fatalerr = 4;
#endif
    return;
  }

  /* I don't know what this code does. -Robert */
  switch (i & 0xc0) {
  case 0x00:
    if ((i & 0x7) == 0x6) {
      dee = *(short *) (csp + 2);
      REG(eip) += 4;
    }
    else {
      REG(eip) += 2;
      dee = 0;
    }
    break;
  case 0x40:
    dee = (signed) csp[2];
    REG(eip) += 3;
    break;
  case 0x80:
    dee = *(short *) (csp + 2);
    REG(eip) += 4;
    break;
  default:
    REG(eip) += 2;
    dee = 0;
  }

  warn("MATH: emulation %x d=%x\n", i, dee);

#else /* this is the new, stupid MATH-EMU code. it doesn't work. */

  if ((*csp >= 0xd8) && (*csp <= 0xdf)) {	/* the FPU insn prefix bytes */
    error("MATH: math emulation for (first 2 bytes) %02x %02x...\n",
	  *csp, *(csp + 1));

    /* this is the coprocessor-not-available int. to use this, you
       * should compile your kernel with FPU-emu support in, and you
       * should install a DOS-based FPU emulator within dosemu.
       * this is untested, and I CAN'T test it, as I have a 486.
       */
    do_int(7);
  }
#endif
}

void
sigfpe(int sig)
{
  if (!in_vm86)
    error("ERROR: NON-VM86 SIGFPE insn!\n");

  in_vm86 = 0;

  error("SIGFPE %d received\n", sig);
  show_regs();
  if (cpu.iflag)
    do_int(0);
  else
    error("ERROR: FPE happened but interrupts disabled\n");
}

void
sigtrap(int sig)
{
  in_vm86 = 0;

#ifdef PROPER_STI
  if (cpu.sti) {
    cpu.iflag = 1;
    cpu.sti = 0;
    LWORD(eflags) &= ~(TF);
    return;
  }
#endif

  if (LWORD(eflags) & TF)		/* trap flag */
    LWORD(eip)++;

  do_int(3);
}

struct port_struct {
  int start;
  int size;
  int permission;
  int ormask, andmask;
}

*ports = NULL;
int num_ports = 0;

extern struct screen_stat scr_state;
u_char video_port_io = 0;

/* find a matching entry in the ports array */
int
find_port(int port, int permission)
{
  static int last_found = 0;
  int i;

  for (i = last_found; i < num_ports; i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission)) {
      last_found = i;
      return (i);
    }

  for (i = 0; i < last_found; i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission)) {
      last_found = i;
      return (i);
    }

  /*
   for now, video_port_io flag allows special access for video regs
*/
  if (((LWORD(cs) & 0xf000) == 0xc000) && config.allowvideoportaccess && scr_state.current) {
#if 0
    v_printf("VID: checking port %x\n", port);
#endif
    if (port > 0x200 || port == 0x42) {
      video_port_io = 1;
      return 1;
    }
  }

  return (-1);
}

/* control the io permissions for ports */
int
allow_io(int start, int size, int permission, int ormask, int andmask)
{

  /* first the simplest case...however, this simplest case does
   * not apply to ports above 0x3ff--the maximum ioperm()able
   * port under Linux--so we must handle some of that ourselves.
   */
  if (permission == IO_RDWR && (ormask == 0 && andmask == 0xFFFF)) {
    if ((start + size - 1) <= 0x3ff) {
      c_printf("giving hardware access to ports 0x%x -> 0x%x\n",
	       start, start + size - 1);
      set_ioperm(start, size, 1);
      return (1);
    }

    /* allow direct I/O to all the ports that *are* below 0x3ff
       * and add the rest to our list
       */
    else if (start <= 0x3ff) {
      warn("PORT: s-s: %d %d\n", start, size);
      set_ioperm(start, 0x400 - start, 1);
      size = start + size - 0x400;
      start = 0x400;
    }
  }

  /* we'll need to add an entry */
  if (ports)
    ports = (struct port_struct *)
      realloc(ports, sizeof(struct port_struct) * (num_ports + 1));

  else
    ports = (struct port_struct *)
      malloc(sizeof(struct port_struct) * (num_ports + 1));

  num_ports++;
  if (!ports) {
    error("allocation error for ports!\n");
    num_ports = 0;
    return (0);
  }

  ports[num_ports - 1].start = start;
  ports[num_ports - 1].size = size;
  ports[num_ports - 1].permission = permission;
  ports[num_ports - 1].ormask = ormask;
  ports[num_ports - 1].andmask = andmask;

  c_printf("added port %x size=%d permission=%d ormask=%x andmask=%x\n",
	   start, size, permission, ormask, andmask);

  return (1);
}

int
port_readable(int port)
{
  return (find_port(port, IO_READ) != -1);
}

int
port_writeable(int port)
{
  return (find_port(port, IO_WRITE) != -1);
}

int
read_port(int port)
{
  int r;
  int i = find_port(port, IO_READ);

  if (i == -1)
    return (0);

  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    iopl(3);

  r = port_in(port);

  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    iopl(0);

  if (!video_port_io) {
    r &= ports[i].andmask;
    r |= ports[i].ormask;
  }
  else
    video_port_io = 0;

  h_printf("read port 0x%x gave %02x at %04x:%04x\n",
	   port, r, LWORD(cs), LWORD(eip));
  return (r);
}

int
write_port(int value, int port)
{
  int i = find_port(port, IO_WRITE);

  if (i == -1)
    return (0);

  if (!video_port_io) {
    value &= ports[i].andmask;
    value |= ports[i].ormask;
  }
  else
    video_port_io = 0;

  h_printf("write port 0x%x value %02x at %04x:%04x\n",
	   port, value, LWORD(cs), LWORD(eip));

  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    iopl(3);

  port_out(value, port);

  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    iopl(0);

  return (1);
}

char
pop_byte(struct vm86_regs *pregs)
{
  char *stack_byte = (char *) ((WORD(pregs->ss) << 4) + WORD(pregs->esp));

  pregs->esp += 1;
  return *stack_byte;
}

short
pop_word(struct vm86_regs *pregs)
{
  unsigned short *stack_word;

  if (!pregs->esp)
    stack_word = (unsigned short *) ((WORD(pregs->ss) << 4) + 0x8000);
  else
    stack_word = (unsigned short *) ((WORD(pregs->ss) << 4) + WORD(pregs->esp));
  pregs->esp += 2;
  return *stack_word;
}

long
pop_long(struct vm86_regs *pregs)
{
  long *stack_long = (long *) ((WORD(pregs->ss) << 4) + WORD(pregs->esp));

  pregs->esp += 4;
  return *stack_long;
}

void
push_byte(struct vm86_regs *pregs, char byte)
{
  pregs->esp -= 1;
  *(char *) ((WORD(pregs->ss) << 4) + WORD(pregs->esp)) = byte;
}

void
push_word(struct vm86_regs *pregs, short word)
{
  pregs->esp -= 2;
  *(short *) ((WORD(pregs->ss) << 4) + WORD(pregs->esp)) = word;
}

void
push_long(struct vm86_regs *pregs, long longword)
{
  pregs->esp -= 4;
  *(long *) ((WORD(pregs->ss) << 4) + WORD(pregs->esp)) = longword;
}

interrupt_stack_frame
pop_isf(struct vm86_regs *pregs)
{
  interrupt_stack_frame isf;

  show_regs();
  isf.eip = pop_word(pregs);
  isf.cs = pop_word(pregs);
  isf.flags = pop_word(pregs);
  error("cs: 0x%x, eip: 0x%x, flags: 0x%x\n", isf.cs, isf.eip,
	isf.flags);
  return isf;
}

void
push_isf(struct vm86_regs *pregs, interrupt_stack_frame isf)
{
  push_word(pregs, isf.flags);
  push_word(pregs, isf.cs);
  push_word(pregs, isf.eip);
}

#undef CPU_C
