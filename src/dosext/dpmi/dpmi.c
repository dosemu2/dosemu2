/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 			DPMI support for DOSEMU
 *
 * DANG_BEGIN_MODULE dpmi.c
 *
 * REMARK
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * First Attempted by James B. MacLean macleajb@ednet.ns.ca
 *
 */

#define DPMI_C

#ifdef __linux__                  
#define DIRECT_DPMI_CONTEXT_SWITCH
#ifdef DIRECT_DPMI_CONTEXT_SWITCH
  #define COPY_CONTEXT_USE_ASM
#endif
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "vm86plus.h"

#ifdef __linux__
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "emu-ldt.h"
#endif

#ifdef USE_SBEMU
#include "sound.h"
#endif

#include <string.h>
#include <errno.h>
#include "emu.h"
#include "memory.h"
#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif
#include "dosio.h"

#if 0
#define SHOWREGS
#endif

#if 0
#undef inline /*for gdb*/
#define inline
#endif

#include "dpmi.h"
#include "msdos.h"
#include "bios.h"
#include "config.h"
#include "bitops.h"
#include "pic.h"
#include "meminfo.h"
#include "int.h"
#include "serial.h"
#include "port.h"
#include "dma.h"
#include "timers.h"
#include "userhook.h"

#if X_GRAPHICS
#include "vgaemu.h"
#endif

#ifndef PAGE_SIZE
/* FIXME: we really need to define those missing PAGE_SIZEs globally :-( */
#define PAGE_SIZE       4096  
#endif

int dpmi_eflags = 0;

unsigned long RealModeContext;
/*
 * With this stack nested PM programs like DJ200's make/compiler work fine
 * (maybe Borkland's protected mode make/compiler too).
 * For do_int31
 * 	0x0300:	Simulate Real Mode Interrupt
 * 	0x0301:	Call Real Mode Procedure With Far Return Frame
 * 	0x0302:	Call Real Mode Procedure With Iret Frame
 */
unsigned long RealModeContext_Stack[DPMI_max_rec_rm_func];
unsigned long RealModeContext_Running = 0;

INTDESC Interrupt_Table[0x100];
static INTDESC Exception_Table[0x20];
SEGDESC Segments[MAX_SELECTORS];

/* for real mode call back, DPMI function 0x303 0x304 */
static RealModeCallBack realModeCallBack[DPMI_MAX_CLIENTS][0x10];

#define CLI_BLACKLIST_LEN 128
static unsigned char * cli_blacklist[CLI_BLACKLIST_LEN];
static unsigned char * current_cli;
static int cli_blacklisted = 0;
static int find_cli_in_blacklist(unsigned char *);
static int dpmi_mhp_intxx_check(struct sigcontext_struct *scp, int intno);

RealModeCallBack mouseCallBack; /* user\'s mouse routine */

struct vm86_regs DPMI_rm_stack[DPMI_max_rec_rm_func];
int DPMI_rm_procedure_running = 0;

#define DPMI_max_rec_pm_func 16
struct sigcontext_struct DPMI_pm_stack[DPMI_max_rec_pm_func];
int DPMI_pm_procedure_running = 0;

char *ldt_buffer=0;
unsigned short LDT_ALIAS = 0;
unsigned short KSPACE_LDT_ALIAS = 0;

static char *pm_stack; /* locked protected mode stack */
static int in_dpmi_pm_stack = 0; /* locked protected mode stack in use */

static unsigned long dpmi_total_memory; /* total memory  of this session */

int DPMIclient_is_32 = 0;
unsigned short DPMI_private_data_segment;
unsigned short PMSTACK_SEL = 0;	/* protected mode stack selector */
unsigned long PMSTACK_ESP = 0;	/* protected mode stack descriptor */
unsigned short DPMI_SEL = 0;

struct sigcontext_struct dpmi_stack_frame[DPMI_MAX_CLIENTS]; /* used to store the dpmi client registers */
static struct sigcontext_struct _emu_stack_frame;  /* used to store emulator registers */
static struct sigcontext_struct *emu_stack_frame = &_emu_stack_frame;

#define CHECK_SELECTOR(x) \
{ if (!ValidAndUsedSelector(x) || SystemSelector(x)) { \
      _LWORD(eax) = 0x8022; \
      _eflags |= CF; \
      break; \
    } \
}

#define CHECK_SELECTOR_ALLOC(x) \
{ if (!ValidSelector(x) || SystemSelector(x)) { \
      _LWORD(eax) = 0x8022; \
      _eflags |= CF; \
      break; \
    } \
}

#ifdef __linux__
_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)
#endif

static inline int get_ldt(void *buffer)
{
#ifdef __linux__
#ifdef X86_EMULATOR
  if (config.cpuemu>1)
	return emu_modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
  else
#endif
  return modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
#endif
}

static int set_ldt_entry(int entry, unsigned long base, unsigned int limit,
	      int seg_32bit_flag, int contents, int read_only_flag,
	      int limit_in_pages_flag
#ifdef WANT_WINDOWS
, int seg_not_present, int useable
#endif
)
{
  unsigned long *lp;
/* --------------------- linux --------------------- */
#ifdef __linux__
  struct modify_ldt_ldt_s ldt_info;
  int __retval;
  ldt_info.entry_number = entry;
  ldt_info.base_addr = base;
  ldt_info.limit = limit;
  ldt_info.seg_32bit = seg_32bit_flag;
  ldt_info.contents = contents;
  ldt_info.read_exec_only = read_only_flag;
  ldt_info.limit_in_pages = limit_in_pages_flag;
#ifdef WANT_WINDOWS
  ldt_info.seg_not_present = seg_not_present;
  ldt_info.useable = useable;
#else
  ldt_info.seg_not_present = 0;
#endif

#ifdef X86_EMULATOR
  if (config.cpuemu>1)
	__retval = emu_modify_ldt(LDT_WRITE, &ldt_info, sizeof(ldt_info));
  else
#endif
  __retval = modify_ldt(LDT_WRITE, &ldt_info, sizeof(ldt_info));
  if (__retval)
	return __retval;
#endif /* __linux__ */


/*
 * DANG_BEGIN_REMARK
 * 
 * We are caching ldt here for speed reasons and for Windows 3.1.
 * I would love to have an readonly ldt-alias (located in the first
 * 16MByte for use with 16-Bit descriptors (WIN-LDT)). This is on my
 * wish list for the kernel hackers (Linus mainly) :-))))))).
 *
 * DANG_END_REMARK
 */

  lp = (unsigned long *) &ldt_buffer[entry*LDT_ENTRY_SIZE];

  if (base == 0 && limit == 0 &&
      contents == 0 && read_only_flag == 1 &&
      seg_32bit_flag == 0 && limit_in_pages_flag == 0
#ifdef WANT_WINDOWS
      && seg_not_present == 1 && useable == 0
#endif
      ) {
	*lp = 0;
	*(lp+1) = 0;
	return 0;
  }
#ifdef __linux__
  *lp =     ((ldt_info.base_addr & 0x0000ffff) << 16) |
            (ldt_info.limit & 0x0ffff);
  *(lp+1) = (ldt_info.base_addr & 0xff000000) |
            ((ldt_info.base_addr & 0x00ff0000)>>16) |
            (ldt_info.limit & 0xf0000) |
            (ldt_info.contents << 10) |
            ((ldt_info.read_exec_only ^ 1) << 9) |
            (ldt_info.seg_32bit << 22) |
            (ldt_info.limit_in_pages << 23) |
            ((ldt_info.seg_not_present ^1) << 15) |
#if defined(WANT_WINDOWS)
            (ldt_info.useable << 20) |
#endif
            0x7000;
#endif
  return 0;
}

static void _print_dt(char *buffer, int nsel, int isldt) /* stolen from WINE */
{
  static char *cdsdescs[] = {
	"RO data", "RW data/upstack", "RO data", "RW data/dnstack",
	"FO nonconf code", "FR nonconf code", "FO conf code", "FR conf code"
  };
  static char *sysdescs[] = {
	"reserved", "avail 16bit TSS", "LDT" ,"busy 16bit TSS",
	"16bit call gate", "task gate", "16bit int gate", "16bit trap gate",
	"reserved", "avail 32bit TSS", "reserved" ,"busy 32bit TSS",
	"32bit call gate", "reserved", "32bit int gate", "32bit trap gate"
  };
  unsigned long *lp;
  unsigned long base_addr, limit;
  int type, i;

  lp = (unsigned long *) buffer;

  for (i = 0; i < nsel; i++, lp++) {
    /* First 32 bits of descriptor */
    base_addr = (*lp >> 16) & 0x0000FFFF;
    limit = *lp & 0x0000FFFF;
    lp++;

    /* First 32 bits of descriptor */
    base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    limit |= (*lp & 0x000F0000);
    type = (*lp >> 8) & 15;
    if ((base_addr > 0) || (limit > 0 ) || Segments[i].used) {
      if (*lp & 0x1000)  {
	D_printf("Entry 0x%04x: Base %08lx, Limit %05lx, Type %d (desc %#x)\n",
	       i, base_addr, limit, (type&7), (i<<3)|(isldt?7:0));
	D_printf("              ");
	if (*lp & 0x100)
	  D_printf("Accessed, ");
	if (!(*lp & 0x200))
	  D_printf("Read/Execute only, ");
	if (*lp & 0x8000)
	  D_printf("Present, ");
	if (*lp & 0x100000)
	  D_printf("User, ");
	if (*lp & 0x200000)
	  D_printf("X, ");
	if (*lp & 0x400000)
	  D_printf("32, ");
	else
	  D_printf("16, ");
	if (*lp & 0x800000)
	  D_printf("page limit, ");
	else
	  D_printf("byte limit, ");
	D_printf("\n");
	D_printf("              %08lx %08lx [%s]\n", *(lp), *(lp-1),
		cdsdescs[type>>1]);
      }
      else {
	D_printf("Entry 0x%04x: Base %08lx, Limit %05lx, Type %d (desc %#x)\n",
	       i, base_addr, limit, type, (i<<3)|(isldt?7:0));
	D_printf("              SYSTEM: %08lx %08lx [%s]\n", *lp, *(lp-1),
		sysdescs[type]);
      }
      if (isldt) {
	unsigned long *lp2;
	lp2 = (unsigned long *) &ldt_buffer[i*LDT_ENTRY_SIZE];
	D_printf("       cache: %08lx %08lx\n", (unsigned long)*(lp2+1), (unsigned long)*(lp2)); 
	D_printf("         seg: Base %08lx, Limit %05x, Type %d, Big %d\n",
	     Segments[i].base_addr, Segments[i].limit, Segments[i].type, Segments[i].is_big); 
      }
    }
  }
}

static void print_ldt(void)
{
  static char buffer[0x10000];

  if (get_ldt(buffer) < 0)
    leavedos(0x544c);

  _print_dt(buffer, MAX_SELECTORS, 1);
}

/* client_esp return the proper value of client\'s esp, if scp != 0, */
/* get esp from scp, otherwise get esp from dpmi_stack_frame         */
static inline unsigned long client_esp(struct sigcontext_struct *scp)
{
    if (scp) {
	if( Segments[_ss >> 3].is_32)
	    return _esp;
	else
	    return (_esp)&0xffff;
    } else {
	if( Segments[dpmi_stack_frame[current_client].ss >> 3].is_32)
	    return dpmi_stack_frame[current_client].esp;
	else
	    return dpmi_stack_frame[current_client].esp&0xffff;
    }
}


#ifdef DIRECT_DPMI_CONTEXT_SWITCH
/* --------------------------------------------------------------
 *-15	context->gs, __gsh	copied, then popped
 *-14	context->fs, __fsh
 *-13	context->es, __esh
 *-12	context->ds, __dsh
 *-11	context->edi
 *-10	context->esi
 *-09	context->ebp
 *-08	context->esp
 *-07	context->ebx
 *-06	context->edx
 *-05	context->ecx
 *-04	context->eax
 *-03	context->eflags	(cs,eip are written into dpmi_switch_jmp)
 *-02	context->esp
 *-01	context->ss
 * --------------------------------------------------------------
 * 00	unsigned short gs, __gsh;	00 -88 -> emu_stack_frame
 * 01	unsigned short fs, __fsh;	04 -84
 * 02	unsigned short es, __esh;	08 -80
 * 03	unsigned short ds, __dsh;	12 -76
 * 04	unsigned long edi;		16 -72 ---------\
 * 05	unsigned long esi;		20 -68		|
 * 06	unsigned long ebp;		24 -64		|
 *    esp contains &edi when pushed; we adjust it to point at eip below
 * 07	unsigned long esp;		28 -60		|
 * 08	unsigned long ebx;		32 -56		|
 * 09	unsigned long edx;		36 -52		|
 * 10	unsigned long ecx;		40 -48		|
 * 11	unsigned long eax;		44 -44		|(+40)
 *							|
 * 12	unsigned long trapno;		48 -40  zeroed  |
 * 13	unsigned long err;		52 -36  zeroed  |
 * 14	unsigned long eip;		56 -32  dpmi_switch_return
 * 15	unsigned short cs, __csh;	60 -28
 * 16	unsigned long eflags;		64 -24
 *
 * 17	unsigned long esp_at_signal;	68 -20  ==esp
 * 18	unsigned short ss, __ssh;	72 -16
 * 19	struct _fpstate * fpstate;	76 -12  dirty
 * 20	unsigned long oldmask;		80 -08  dirty
 * 21	unsigned long cr2;		84 -04  dirty
 * --------------------------------------------------------------
 */
static int direct_dpmi_switch(struct sigcontext_struct *dpmi_context)
{
  register int ret;

  CheckSelectors(dpmi_context);

  __asm__ volatile (
"      subl   $12,%%esp\n"		/* dummy, cr2,oldmask,fpstate */
"      push   %%ss\n"
"      pushl  %%esp\n"			/* dummy, esp_at_signal */
"      pushfl\n"
"      push   %%cs\n"
"      pushl  $dpmi_switch_return\n"
"      xorl   %0,%0\n"			/* preset return code with 0 */
"      push   %%eax\n"			/* dummy, err */
"      push   %%eax\n"			/* dummy, trapno */
"      pusha\n"
"      addl   $10*4,12(%%esp)\n"	/* adjust esp */
"      push   %%ds\n"
"      push   %%es\n"
"      push   %%fs\n"
"      push   %%gs\n"
"      movl   %%esp,"CISH_INLINE(emu_stack_frame)"\n"

      /* now we load the new context
          we move it on our stack, and then pop it */

"      movl   (14*4)(%1),%%eax\n"	/* p->eip */
"      movl   %%eax,__neweip\n"
"      movw   (15*4)(%1),%%ax\n"	/* p->cs */
"      movw   %%ax,__newcs\n"
"      pushl  18*4 (%1)\n"		/* p->ss */
"      pushl  7*4 (%1)\n"		/* p->esp */
"      pushl  16*4 (%1)\n"		/* p->eflags */
"      movl   $12,%%ecx\n"
"      subl   $12*4,%%esp\n"		/* make room on the stack */
"      cld\n"
"      movl   %%esp,%%edi\n"
"      movl   %1,%%esi\n"
"      rep; movsl\n"
"      pop    %%gs\n"
"      pop    %%fs\n"
"      pop    %%es\n"
"      pop    %%ds\n"
"      popa\n"
"      popfl\n"
"      lss    (%%esp),%%esp\n"		/* this is: pop ss; pop esp */
"      jmp    __dpmi_switch_jmp\n"

      /* NOTE: we are putting the ljmp into .data (needing this kludge),
                because we can't write to code space (Hans) */
"      .data\n"
"  __dpmi_switch_jmp:\n"
"      .byte  0xEA\n"			/* ljmp */
"  __neweip:\n"
"      .long  0x12345678\n"
"  __newcs:\n"
"      .short 0xabcd\n"
"      .text\n"
"  dpmi_switch_return:" \
  :"=a" (ret): "d" (dpmi_context)
  );
  return ret;
}
#endif

/* ======================================================================== */
/*
 * DANG_BEGIN_FUNCTION dpmi_control
 *
 * This function is similar to the vm86() syscall in the kernel and
 * switches to dpmi code.
 *
 * DANG_END_FUNCTION
 */

static int dpmi_control(void)
{
/*
 * DANG_BEGIN_REMARK
 * 
 * DPMI is designed such that the stack change needs a task switch.
 * We are doing it via an SIGSEGV - instead of one task switch we have
 * now four :-(.
 * Arrgh this is the point where I should start to include DPMI stuff
 * in the kernel, but then we could include the rest of dosemu too.
 * Would Linus love this? I don't :-((((.
 * Anyway I would love to see first a working DPMI port, maybe we
 * will later (with version 0.9 or similar :-)) start with it to
 * get a really fast dos emulator...............
 *
 * NOTE: Using DIRECT_DPMI_CONTEXT_SWITCH we avoid these 4  taskswitches
 *       actually doing 0. We don't need a 'physical' taskswitch
 *       (not having different TSS for us and DPMI), we only need a complete
 *       register (context) replacement. For back-switching, however, we need
 *       the sigcontext technique, so we build a proper sigcontext structure
 *       even for 'hand made taskswitch'. (Hans Lermen, June 1996)
 *
 * dpmi_control is called only from dpmi_run when in_dpmi_dos_int==0
 *
 * DANG_END_REMARK
 */

/* STANDARD SWITCH example
 *
 * run_dpmi() -> dpmi_control (cs==UCODESEL)
 *			xor %eax,%eax
 *			hlt
 *		 -> dpmi_fault: save scp to emu_stack_frame
 *	[C>S>E]			move client frame to scp
 *				return -> jump to DPMI code
 *		========== into client code =================
 *		 -> dpmi_fault (cs==DPMI_SEL)
 *				perform fault action (e.g. I/O) on scp
 *		========== into client code =================
 *		 -> dpmi_fault with return to dosemu code:
 *				save scp to client frame
 *	[E>S>C]			move emu_stack_frame to scp
 *				return
 *	         dpmi_control <-
 *			return %eax
 * run_dpmi() <-
 *
 * DIRECT SWITCH example
 *
 * run_dpmi() -> dpmi_control (cs==UCODESEL) -> direct_dpmi_switch
 *				*** there's no scp ***
 *				create new emu_stack_frame ON STACK
 *				push client frame
 *				pop and jump to DPMI code (client cs:eip)
 *		========== into client code =================
 *		 -> dpmi_fault (cs==DPMI_SEL)
 *				perform fault action (e.g. I/O) on scp
 *		========== into client code =================
 *		 -> dpmi_fault with return to dosemu code:
 *				save scp to client frame
 *	[E>S>C]			move emu_stack_frame to scp
 *				return
 *			dpmi_switch_return <-
 *	         dpmi_control <-
 *			return %eax
 * run_dpmi() <-
 *
 */

  register int ret;
#ifdef DIRECT_DPMI_CONTEXT_SWITCH
  struct sigcontext_struct *scp=&dpmi_stack_frame[current_client];
#ifdef TRACE_DPMI
  if (debug_level('t')) _eflags |= TF;
#endif
  if (!(_eflags & TF)) {
	if (debug_level('M')>6) {
	  D_printf("DPMI SWITCH to %08lx, esp=%08lx\n",(long)SEL_ADR(_cs,_eip),_esp);
	}
	return direct_dpmi_switch(scp);
  }
  else {
    /* Note: we can't set TF with our speedup code */
#ifdef USE_MHPDBG
    if (mhpdbg.active) {
      static int force_early=0;
      usleep(1); /* NOTE: We need a syscall (maybe any) to force scheduling.
                  *       ( ... don't know why ... )
                  *       If we do not, the below kludge doesn't work
                  *       and we may loose 1 single step after an INTx.
                  */
      if (*((unsigned char *)SEL_ADR(_cs,_eip))==0xcd) force_early=1;
      else if (force_early) {
        force_early=0;
        return 1; /* we are simulating SIGTRAP after INTx */
      }
    }
#endif
    emu_stack_frame=&_emu_stack_frame;
    asm("xorl %0,%0; hlt":"=a" (ret));
    return ret;
  }
#else
  asm("xorl %0,%0; hlt":"=a" (ret));
  return ret;
#endif
}

void dpmi_get_entry_point(void)
{
    D_printf("Request for DPMI entry\n");

    if (!config.dpmi)
      return;

    REG(eax) = 0; /* no error */

    /* 32 bit programs are O.K. */
    LWORD(ebx) = 1;
    LO(cx) = vm86s.cpu_type;

    /* Version 0.9 */
    HI(dx) = DPMI_VERSION;
    LO(dx) = DPMI_DRIVER_VERSION;

    /* Entry Address for DPMI */
    REG(es) = DPMI_SEG;
    REG(edi) = DPMI_OFF;

    /* private data */
    if (in_dpmi)
      LWORD(esi) = 0;
    else
      LWORD(esi) = DPMI_private_paragraphs + 0x1008;

    D_printf("DPMI entry returned\n");
}

static int SetSelector(unsigned short selector, unsigned long base_addr, unsigned int limit,
                       unsigned char is_32, unsigned char type, unsigned char readonly,
                       unsigned char is_big, unsigned char seg_not_present, unsigned char useable)
{
  int ldt_entry = selector >> 3;
  if (set_ldt_entry(ldt_entry, base_addr, limit, is_32, type, readonly, is_big
#ifdef WANT_WINDOWS
  , seg_not_present, useable
#endif
  )) {
    D_printf("DPMI: set_ldt_entry() failed\n");
    return -1;
  }
  D_printf("DPMI: SetSelector: 0x%04x base=0x%lx limit=0x%x big=%i\n",
    selector, base_addr, limit, is_big);
  Segments[ldt_entry].base_addr = base_addr;
  Segments[ldt_entry].limit = limit;
  Segments[ldt_entry].type = type;
  Segments[ldt_entry].is_32 = is_32;
  Segments[ldt_entry].readonly = readonly;
  Segments[ldt_entry].is_big = is_big;
  Segments[ldt_entry].not_present = seg_not_present;
  Segments[ldt_entry].useable = useable;
  if (in_dpmi)
    Segments[ldt_entry].used = in_dpmi;
  else
    Segments[ldt_entry].used = 1;
  return 0;
} 

static unsigned short AllocateDescriptorsAt(unsigned short selector,
    int number_of_descriptors)
{
  int ldt_entry = selector >> 3, i;

  if (ldt_entry <= 0x10) return 0;
  if (ldt_entry == MAX_SELECTORS-number_of_descriptors+1) return 0;
  for (i=0;i<number_of_descriptors;i++)
    if (Segments[ldt_entry+i].used) return 0;

  for (i=0;i<number_of_descriptors;i++) {
    if (in_dpmi)
      Segments[ldt_entry+i].used = in_dpmi;
    else {
      error("AllocateDescriptor() called while in_dpmi=0\n");
      Segments[ldt_entry+i].used = 1;
    }
  }
  D_printf("DPMI: Allocate %d descriptors started at 0x%04x\n",
	number_of_descriptors, (ldt_entry<<3) | 0x0007);
  /* dpmi spec says, the descriptor allocated should be "data" with */
  /* base and limit set to 0 */
  for (i = 0; i < number_of_descriptors; i++)
      if (SetSelector(((ldt_entry+i)<<3) | 0x0007, 0, 0, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  return (ldt_entry<<3) | 0x0007;
}

unsigned short AllocateDescriptors(int number_of_descriptors)
{
  int next_ldt=0x10, i;		/* first 0x10 descriptors are reserved */
  unsigned char isfree=1;
  while (isfree) {
    next_ldt++;
    isfree=0;
    for (i=0;i<number_of_descriptors;i++)
      isfree |= Segments[next_ldt+i].used;
    if (next_ldt == MAX_SELECTORS-number_of_descriptors+1) return 0;
  }
  return AllocateDescriptorsAt((next_ldt<<3) | 0x0007, number_of_descriptors);
}

static void FreeSegRegs(struct sigcontext_struct *scp, unsigned short selector)
{
    if ((_ds | 7) == (selector | 7)) _ds = 0;
    if ((_es | 7) == (selector | 7)) _es = 0;
    if ((_fs | 7) == (selector | 7)) _fs = 0;
    if ((_gs | 7) == (selector | 7)) _gs = 0;
}

int FreeDescriptor(unsigned short selector)
{
  unsigned short ldt_entry = selector >> 3;

  if (ldt_entry >= MAX_SELECTORS)
    return -1;

  D_printf("DPMI: Free descriptor, selector=0x%x\n", selector);
  Segments[ldt_entry].used = 0;
  Segments[ldt_entry].base_addr = 0;
  Segments[ldt_entry].limit = 0;
  Segments[ldt_entry].type = 0;
  Segments[ldt_entry].is_32 = 0;
  Segments[ldt_entry].readonly = 1;
  Segments[ldt_entry].is_big = 0;
  Segments[ldt_entry].not_present = 1;
  Segments[ldt_entry].useable = 0;
#if 1
  /* We should be in sync with the real LDT  --Alberto 981209 */
  return set_ldt_entry(ldt_entry, 0, 0, 0, 0, 1, 0
  #ifdef WANT_WINDOWS
    , 1, 0
  #endif
  );
#else
  return 0;
#endif
}

static void FreeAllDescriptors(void)
{
    int i;
    for (i=0;i<MAX_SELECTORS;i++) {
      if (Segments[i].used==in_dpmi)
        FreeDescriptor(i<<3);
    }
}

static int ConvertSegmentToDescriptor32(unsigned short segment, int limit_is_32)
{
  unsigned long baseaddr = segment << 4;
  unsigned long limit = limit_is_32 ? 0xffffffff : 0xffff;
  unsigned short selector;
  int i;
  D_printf("DPMI: convert seg %#x to desc\n", segment);
  for (i=1;i<MAX_SELECTORS;i++)
    if ((Segments[i].base_addr==baseaddr) && (Segments[i].limit==limit) &&
	(Segments[i].type==MODIFY_LDT_CONTENTS_DATA) && Segments[i].used)
      return (i<<3) | 0x0007;
  D_printf("DPMI: SEG at base=%#lx not found, allocate a new one\n", baseaddr);
  if (!(selector = AllocateDescriptors(1))) return 0;
  if (SetSelector(selector, baseaddr, limit, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  return selector;
}

int ConvertSegmentToDescriptor(unsigned short segment)
{
  return ConvertSegmentToDescriptor32(segment, DPMIclient_is_32);
}

static int ConvertSegmentToDescriptor16(unsigned short segment)
{
  return ConvertSegmentToDescriptor32(segment, 0);
}

static inline unsigned short GetNextSelectorIncrementValue(void)
{
  return 8;
}

static int SystemSelector(unsigned short selector)
{
  if ((((selector) & 0xfffc) == (DPMI_SEL & 0xfffc)) ||
     (((selector) & 0xfffc ) == (PMSTACK_SEL & 0xfffc)) ||
     (((selector) & 0xfffc ) == (LDT_ALIAS & 0xfffc)))
    return 1;
  return 0;
}

int ValidSelector(unsigned short selector)
{
  if (selector && ((selector >> 3) < MAX_SELECTORS) && (selector & 4) == 4)
    return 1;
  return 0;
}

int ValidAndUsedSelector(unsigned short selector)
{
  if (ValidSelector(selector) && Segments[selector >> 3].used)
    return 1;
  return 0;
}

void CheckSelectors(struct sigcontext_struct *scp)
{
/* NONCONFORMING-CODE-SEGMENT:
   RPL of destination selector must be <= CPL ELSE #GP(selector);
   Descriptor DPL must be = CPL ELSE #GP(selector);
   Segment must be present ELSE # NP(selector);
   Instruction pointer must be within code-segment limit ELSE #GP(0);
*/
  if (!ValidAndUsedSelector(_cs) || Segments[_cs >> 3].not_present ||
    Segments[_cs >> 3].type != MODIFY_LDT_CONTENTS_CODE) {
    error("CS selector invalid: 0x%04X, type=%x np=%i\n",
      _cs, Segments[_cs >> 3].type, Segments[_cs >> 3].not_present);
    leavedos(36);
  }

/*
IF SS is loaded;
THEN
   IF selector is null THEN #GP(0);
FI;
   Selector index must be within its descriptor table limits else
      #GP(selector);
   Selector's RPL must equal CPL else #GP(selector);
AR byte must indicate a writable data segment else #GP(selector);
   DPL in the AR byte must equal CPL else #GP(selector);
   Segment must be marked present else #SS(selector);
   Load SS with selector;
   Load SS with descriptor.
FI;
*/
  if (!ValidAndUsedSelector(_ss) || ((_ss & 3) != 3) || /* writable??? */
     Segments[_ss >> 3].not_present ||
     (Segments[_ss >> 3].type != MODIFY_LDT_CONTENTS_STACK &&
      Segments[_ss >> 3].type != MODIFY_LDT_CONTENTS_DATA)) {
    error("SS selector invalid: 0x%04X, type=%x np=%i\n",
      _ss, Segments[_ss >> 3].type, Segments[_ss >> 3].not_present);
    leavedos(36);
  }

/*
IF DS, ES, FS or GS is loaded with non-null selector;
THEN
   Selector index must be within its descriptor table limits
      else #GP(selector);
   AR byte must indicate data or readable code segment else
      #GP(selector);
   IF data or nonconforming code segment
   THEN both the RPL and the CPL must be less than or equal to DPL in
      AR byte;
   ELSE #GP(selector);
   FI;
   Segment must be marked present else #NP(selector);
   Load segment register with selector;
   Load segment register with descriptor;
FI;
IF DS, ES, FS or GS is loaded with a null selector;
THEN
   Load segment register with selector;
   Clear descriptor valid bit;
FI;
*/
  if (_ds && (!ValidAndUsedSelector(_ds) || Segments[_ds >> 3].not_present)) {
    error("DS selector invalid: 0x%04X, type=%x np=%i\n",
      _ds, Segments[_ds >> 3].type, Segments[_ds >> 3].not_present);
    leavedos(36);
  }
  if (_es && (!ValidAndUsedSelector(_es) || Segments[_es >> 3].not_present)) {
    error("ES selector invalid: 0x%04X, type=%x np=%i\n",
      _es, Segments[_es >> 3].type, Segments[_es >> 3].not_present);
    leavedos(36);
  }
  if (_fs && (!ValidAndUsedSelector(_fs) || Segments[_fs >> 3].not_present)) {
    error("FS selector invalid: 0x%04X, type=%x np=%i\n",
      _fs, Segments[_fs >> 3].type, Segments[_fs >> 3].not_present);
    leavedos(36);
  }
  if (_gs && (!ValidAndUsedSelector(_gs) || Segments[_gs >> 3].not_present)) {
    error("GS selector invalid: 0x%04X, type=%x np=%i\n",
      _gs, Segments[_gs >> 3].type, Segments[_gs >> 3].not_present);
    leavedos(36);
  }
}

unsigned long GetSegmentBaseAddress(unsigned short selector)
{
  if (!ValidAndUsedSelector(selector))
    return 0;
  return Segments[selector >> 3].base_addr;
}

/* needed in env/video/vesa.c */
unsigned long dpmi_GetSegmentBaseAddress(unsigned short selector)
{
  return GetSegmentBaseAddress(selector);
}

unsigned long GetSegmentLimit(unsigned short selector)
{
  if (!ValidAndUsedSelector(selector))
    return 0;
  return Segments[selector >> 3].limit;
}

int SetSegmentBaseAddress(unsigned short selector, unsigned long baseaddr)
{
  unsigned short ldt_entry = selector >> 3;
  D_printf("DPMI: SetSegmentBaseAddress[0x%04x;0x%04x] 0x%08lx\n", ldt_entry, selector, baseaddr);
  if (!ValidAndUsedSelector((ldt_entry << 3) | 7))
    return -1;
  Segments[ldt_entry].base_addr = baseaddr;
  return set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, Segments[ldt_entry].readonly,
	Segments[ldt_entry].is_big
#ifdef WANT_WINDOWS
	, Segments[ldt_entry].not_present, Segments[ldt_entry].useable
#endif
);
}

int SetSegmentLimit(unsigned short selector, unsigned int limit)
{
  unsigned short ldt_entry = selector >> 3;
  D_printf("DPMI: SetSegmentLimit[0x%04x;0x%04x] 0x%08x\n", ldt_entry, selector, limit);
  if (!ValidAndUsedSelector((ldt_entry << 3) | 7))
    return -1;
  if (limit > 0x0fffff) {
    /* "Segment limits greater than 1M must have the low 12 bits set" */
    if (~limit & 0xfff) return -1;
    Segments[ldt_entry].limit = (limit>>12) & 0xfffff;
    Segments[ldt_entry].is_big = 1;
  } else {
    Segments[ldt_entry].limit = limit;
    Segments[ldt_entry].is_big = 0;
  }
  return set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, Segments[ldt_entry].readonly,
	Segments[ldt_entry].is_big
#ifdef WANT_WINDOWS
	, Segments[ldt_entry].not_present, Segments[ldt_entry].useable
#endif
);
}

static int SetDescriptorAccessRights(unsigned short selector, unsigned short type_byte)
{
  unsigned short ldt_entry = selector >> 3;
  D_printf("DPMI: SetDescriptorAccessRights[0x%04x;0x%04x] 0x%04x\n", ldt_entry, selector, type_byte);
  if (!ValidAndUsedSelector((ldt_entry << 3) | 7))
    return -1; /* invalid value 8021 */
  if (!(type_byte & 0x10)) /* we refused system selector */
    return -1; /* invalid value 8021 */
  /* Only allow conforming Codesegments if Segment is not present */
  if ( ((type_byte>>2)&3) == 3 && ((type_byte >> 7) & 1) == 1)
    return -2; /* invalid selector 8022 */
  if ((type_byte & 0x0d)==4)
    D_printf("DPMI: warning: expand-down stack segment\n");

  Segments[ldt_entry].type = (type_byte >> 2) & 3;
  Segments[ldt_entry].is_32 = (type_byte >> 14) & 1;

  /* This is a bug in some clients, hence we only allow
     to set the big bit if the limit is not yet set */
#if 1
  if (Segments[ldt_entry].limit == 0)
#endif
    Segments[ldt_entry].is_big = (type_byte >> 15) & 1;

  Segments[ldt_entry].readonly = ((type_byte >> 1) & 1) ? 0 : 1;
  Segments[ldt_entry].not_present = ((type_byte >> 7) & 1) ? 0 : 1;
  Segments[ldt_entry].useable = (type_byte >> 12) & 1;
  return set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr, Segments[ldt_entry].limit,
			Segments[ldt_entry].is_32, Segments[ldt_entry].type,
			Segments[ldt_entry].readonly, Segments[ldt_entry].is_big
#ifdef WANT_WINDOWS
			, Segments[ldt_entry].not_present, Segments[ldt_entry].useable
#endif
);
}

static unsigned short CreateCSAlias(unsigned short selector)
{
  us ds_selector;
  us cs_ldt= selector >> 3;

  ds_selector = AllocateDescriptors(1);
  if (SetSelector(ds_selector, Segments[cs_ldt].base_addr, Segments[cs_ldt].limit,
			Segments[cs_ldt].is_32, MODIFY_LDT_CONTENTS_DATA,
			Segments[cs_ldt].readonly, Segments[cs_ldt].is_big,
			Segments[cs_ldt].not_present, Segments[cs_ldt].useable))
    return 0;
  return ds_selector;
}

static int inline do_LAR(us selector)
{
#ifdef X86_EMULATOR
  if (config.cpuemu>1)
    return emu_do_LAR(selector);
  else
#endif
  __asm__ volatile(" \
    movzwl  %%ax,%%eax\n \
    larw %%ax,%%ax\n \
    jz   1f\n \
    xorl %%eax,%%eax\n"
   "1:   shrl $8,%%eax" \
   :"=a" (selector)
  );
  return selector;
}

static void GetDescriptor(us selector, unsigned long *lp)
{
#if 0    
  modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE);
#else
  /* DANG_BEGIN_REMARK
   * Hopefully the below LAR can serve as a replacement for the KERNEL_LDT,
   * which we are abandoning now. Especially the 'accessed-bit' will get
   * updated in the ldt-cache with the code below.
   * Most DPMI-clients fortunately _are_ using LAR also to get this info,
   * however, some do not. Some of those which do _not_, at least use the
   * DPMI-GetDescriptor function, so this may solve the problem.
   *                      (Hans Lermen, July 1996)
   * DANG_END_REMARK
   */
  int typebyte=do_LAR(selector);
  if (typebyte) {
	D_printf("DPMI: change type only in local selector\n");
	((unsigned char *)(&ldt_buffer[selector & 0xfff8]))[5]=typebyte;
  }
#endif  
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
  D_printf("DPMI: GetDescriptor[0x%04x;0x%04x]: 0x%08lx%08lx\n", selector>>3, selector, *(lp+1), *lp);
}

static int SetDescriptor(unsigned short selector, unsigned long *lp)
{
  unsigned long base_addr, limit;
  D_printf("DPMI: SetDescriptor[0x%04x;0x%04x] 0x%08lx%08lx\n", selector>>3, selector, *(lp+1), *lp);
  if (!ValidAndUsedSelector(selector))
    return -1; /* invalid value 8021 */
  base_addr = (*lp >> 16) & 0x0000FFFF;
  limit = *lp & 0x0000FFFF;
  lp++;
  base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
  limit |= (*lp & 0x000F0000);

  return SetSelector(selector, base_addr, limit, (*lp >> 22) & 1,
			(*lp >> 10) & 3, ((*lp >> 9) & 1) ^1,
			(*lp >> 23) & 1, ((*lp >> 15) & 1) ^1, (*lp >> 20) & 1);
}

static int AllocateSpecificDescriptor(us selector)
{
  int ldt_entry = selector >> 3;
  if (ldt_entry >= MAX_SELECTORS)
    return -1;
  if (Segments[ldt_entry].used)
    return -1;
  /* dpmi spec says, the descriptor allocated should be "data" with */
  /* base and limit set to 0 */
  if (SetSelector((ldt_entry << 3) | 0x0007, 0, 0, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return -1;
  if (in_dpmi)
    Segments[ldt_entry].used = in_dpmi;
  else
    Segments[ldt_entry].used = 1;
  return 0;
}

static  void GetFreeMemoryInformation(unsigned int *lp)
{
  struct meminfo *mi = readMeminfo();

  if (debug_level('M') > 4) {
    D_printf("DPMI: GetFreeMemoryInformation at %08lx\n",(long)lp);
    D_printf("      free=%lx(%x)\n", dpmi_free_memory, mi->free);
    D_printf("      swapfree=%x\n", mi->swapfree);
    D_printf("      swaptotal=%x\n", mi->swaptotal);
    D_printf("      total=%x\n",mi->total);
    D_printf("      mi->free + mi->swapfree=%d\n",(unsigned int) (mi->free + mi->swapfree));
    D_printf("      mi->free + mi->swapfree/DPMI=%d\n",(unsigned int) (mi->free + mi->swapfree)/DPMI_page_size);
    D_printf("      (mi->total + mi->swaptotal)/DPMI_page_size=%d\n",
    (unsigned int) (mi->total + mi->swaptotal)/DPMI_page_size);
    D_printf("      (mi->free)/DPMI_page_size=%d\n", (unsigned int) (mi->free)/DPMI_page_size);
    D_printf("      (mi->total)/DPMI_page_size=%d\n", (unsigned int) (mi->total)/DPMI_page_size);
    D_printf("      (mi->swaptotal)/DPMI_page_size=%d\n", (unsigned int) (mi->swaptotal)/DPMI_page_size);
  }
  /*00h*/	*lp = dpmi_free_memory;
  /*04h*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*08h*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*0ch*/	*++lp = dpmi_total_memory/DPMI_page_size;
  /*10h*/	*++lp = dpmi_free_memory/DPMI_page_size;;
  /*14h*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*18h*/	*++lp = (unsigned int) (mi->total)/DPMI_page_size;
  /*1ch*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*20h*/	*++lp = (unsigned int) (mi->swaptotal)/DPMI_page_size;
  /*24h*/	*++lp = 0xffffffff;
  /*28h*/	*++lp = 0xffffffff;
  /*2ch*/	*++lp = 0xffffffff;
}

static inline void copy_context(struct sigcontext_struct *d, struct sigcontext_struct *s)
{
#ifdef DIRECT_DPMI_CONTEXT_SWITCH
/* --------------------------------------------------------------
 * 00	unsigned short gs, __gsh;	COPIED
 * 01	unsigned short fs, __fsh;	COPIED
 * 02	unsigned short es, __esh;	COPIED
 * 03	unsigned short ds, __dsh;	COPIED
 * 04	unsigned long edi;		COPIED
 * 05	unsigned long esi;		COPIED
 * 06	unsigned long ebp;		COPIED
 * 07	unsigned long esp;		COPIED
 * 08	unsigned long ebx;		COPIED
 * 09	unsigned long edx;		COPIED
 * 10	unsigned long ecx;		COPIED
 * 11	unsigned long eax;		COPIED
 *
 * 12	unsigned long trapno;		NOT COPIED
 * 13	unsigned long err;		NOT COPIED
 * 14	unsigned long eip;		COPIED
 * 15	unsigned short cs, __csh;	COPIED
 * 16	unsigned long eflags;		COPIED
 *
 * 17	unsigned long esp_at_signal;	NOT COPIED
 * 18	unsigned short ss, __ssh;	COPIED
 * 19	struct _fpstate * fpstate;	NOT COPIED
 * 20	unsigned long oldmask;		NOT COPIED
 * 21	unsigned long cr2;		NOT COPIED
 * -------------------------------------------------------------- */

 #ifdef COPY_CONTEXT_USE_ASM
  #ifdef ASM_PEDANTIC
  unsigned long int d0, d1, d2;
  #endif
  __asm__ __volatile__ (" \
	cld\n \
	rep; movsl \
  "
  #ifndef ASM_PEDANTIC
   :: "S" (s), "D" (d), "c" ((((int)(&s->eax) - (int)(s))+ sizeof(s->eax)) >> 2)
   : "cx", "si", "di" );
  #else
   :"=&c" (d0), "=&D" (d1), "=&S" (d2)
   :"0" ((((int)(&s->eax) - (int)(s))+ sizeof(s->eax)) >> 2), "1" (d), "2" (s)
   :"memory");
  #endif
 #else
  memcpy(d, s, ((int)(&s->eax) - (int)(s))+ sizeof(s->eax));
 #endif
  d->eip = s->eip;
  *((long *)(&d->cs)) = *((long *)(&s->cs));
  d->eflags = s->eflags;
  *((long *)(&d->ss)) = *((long *)(&s->ss));
#else /* not DIRECT_DPMI_CONTEXT_SWITCH */
  memcpy(d, s, sizeof(struct sigcontext_struct));
#endif
}

static void Return_to_dosemu_code(struct sigcontext_struct *scp, int retcode)
{
#ifdef X86_EMULATOR
 if (config.cpuemu<2) {	/* 0=off 1=on-inactive 2=active 3=on-first time */
#endif
  if (in_dpmi) {
    copy_context(&dpmi_stack_frame[current_client],scp);
  }
  copy_context(scp, emu_stack_frame);
  _eax = retcode;
#ifdef X86_EMULATOR
 }
 else {
    if (retcode)
	D_printf("DPMI: return %x to dosemu code\n", retcode);
    D_printf("DPMI: in_dpmi_dos_int=%d dpmi_eflags=%x\n",in_dpmi_dos_int,
	dpmi_eflags);
    emu_dpmi_retcode = retcode;
 }
#endif
}

void indirect_dpmi_switch(struct sigcontext_struct *scp)
{
    copy_context(emu_stack_frame, scp);
    CheckSelectors(&dpmi_stack_frame[current_client]);
    copy_context(scp, &dpmi_stack_frame[current_client]);
}

static void save_rm_context(void)
{
  if (RealModeContext_Running >= DPMI_max_rec_rm_func) {
    error("DPMI: RealModeContext_Running = 0x%lx\n",RealModeContext_Running);
    leavedos(25);
  }
  RealModeContext_Stack[RealModeContext_Running++] = RealModeContext;
}

static void restore_rm_context(void)
{
  if (RealModeContext_Running > DPMI_max_rec_rm_func ||
    RealModeContext_Running < 1) {
    error("DPMI: RealModeContext_Running = 0x%lx\n",RealModeContext_Running);
    leavedos(25);
  }
  RealModeContext = RealModeContext_Stack[--RealModeContext_Running];
}


static void save_rm_regs(void)
{
  if (DPMI_rm_procedure_running >= DPMI_max_rec_rm_func) {
    error("DPMI: DPMI_rm_procedure_running = 0x%x\n",DPMI_rm_procedure_running);
    leavedos(25);
  }
  DPMI_rm_stack[DPMI_rm_procedure_running++] = REGS;
  REG(ss) = DPMI_private_data_segment;
  REG(esp) = DPMI_rm_stack_size * DPMI_rm_procedure_running;
}

static void restore_rm_regs(void)
{
  if (DPMI_rm_procedure_running > DPMI_max_rec_rm_func ||
    DPMI_rm_procedure_running < 1) {
    error("DPMI: DPMI_rm_procedure_running = 0x%x\n",DPMI_rm_procedure_running);
    leavedos(25);
  }
  REGS = DPMI_rm_stack[--DPMI_rm_procedure_running];
}

void save_pm_regs(struct sigcontext_struct *scp)
{
  if (DPMI_pm_procedure_running >= DPMI_max_rec_pm_func) {
    error("DPMI: DPMI_pm_procedure_running = 0x%x\n",DPMI_pm_procedure_running);
    leavedos(25);
  }
  copy_context(&DPMI_pm_stack[DPMI_pm_procedure_running++], scp);
}

void restore_pm_regs(struct sigcontext_struct *scp)
{
  if (DPMI_pm_procedure_running > DPMI_max_rec_pm_func ||
    DPMI_pm_procedure_running < 1) {
    error("DPMI: DPMI_pm_procedure_running = 0x%x\n",DPMI_pm_procedure_running);
    leavedos(25);
  }
  copy_context(scp, &DPMI_pm_stack[--DPMI_pm_procedure_running]);
}

void fake_pm_int(void)
{
  D_printf("DPMI: fake_pm_int() called, in_dpmi_dos_int=0x%02x\n",in_dpmi_dos_int);
  save_rm_regs();
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  in_dpmi_dos_int = 1;
}

static void get_ext_API(struct sigcontext_struct *scp)
{
      char *ptr = (char *) (GetSegmentBaseAddress(_ds) + (DPMIclient_is_32 ? _esi : _LWORD(esi)));
      D_printf("DPMI: GetVendorAPIEntryPoint: %s\n", ptr);
#ifdef WANT_WINDOWS
	if ((!strcmp("WINOS2", ptr))||(!strcmp("MS-DOS", ptr)))
      {
        if(_LWORD(eax) == 0x168a)
	  _LO(ax) = 0;
	_es = DPMI_SEL;
	_edi = DPMI_OFF + HLT_OFF(DPMI_API_extension);
      } else
#endif
	if ((!strcmp("PHARLAP.HWINT_SUPPORT", ptr))||(!strcmp("PHARLAP.CE_SUPPORT", ptr)))
      {
        if(_LWORD(eax) == 0x168a)
	  _LO(ax) = 0;
	_LWORD(eax) = 0;
      } else
      {
	if (!(_LWORD(eax)==0x168a))
	  _eax = 0x8001;
	_eflags |= CF;
      }
}

static int ResizeDescriptorBlock(struct sigcontext_struct *scp,
 unsigned short begin_selector, unsigned long length)
{
    unsigned short num_descs, old_num_descs;
    unsigned long old_length, base;
    int i;

    if (!ValidAndUsedSelector(begin_selector)) return 0;
    base = GetSegmentBaseAddress(begin_selector);
    old_length = GetSegmentLimit(begin_selector) + 1;
    old_num_descs = (old_length ? (DPMIclient_is_32 ? 1 : (old_length/0x10000 +
				((old_length%0x10000) ? 1 : 0))) : 0);
    num_descs = (length ? (DPMIclient_is_32 ? 1 : (length/0x10000 +
				((length%0x10000) ? 1 : 0))) : 0);

    if (num_descs > old_num_descs) {
	/* grow block. This can fail so do this before anything else. */
	if (! AllocateDescriptorsAt(begin_selector + (old_num_descs<<3),
	    num_descs - old_num_descs)) {
	    _LWORD(eax) = 0x8011;
	    _LWORD(ebx) = old_length >> 4;
	    D_printf("DPMI: Unable to allocate %i descriptors starting at 0x%x\n",
		num_descs - old_num_descs, begin_selector + (old_num_descs<<3));
	    return 0;
	}
	/* last descriptor has a valid base, lets adjust the limit */
	if (SetSegmentLimit(begin_selector + ((old_num_descs-1)<<3), 0xffff))
	    return 0;
        /* init all the newly allocated descs, including the last one */
	for (i = old_num_descs; i < num_descs; i++) {
	    if (SetSelector(begin_selector + (i<<3), base+i*0x10000,
		    0xffff, DPMIclient_is_32,
		    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
	}
    }
    if (old_num_descs > num_descs) {
	/* shrink block */
	for (i = num_descs; i < old_num_descs; i++) {
	    FreeDescriptor(begin_selector + (i<<3));
	    FreeSegRegs(scp, begin_selector + (i<<3));
	}
    }

    if (num_descs > 0) {
	/* adjust the limit of the leading descriptor */
	if (SetSegmentLimit(begin_selector, length-1)) return 0;
    }
    if (num_descs > 1) {
	/* now adjust the last descriptor. Base is already set. */
	if (SetSegmentLimit(begin_selector + ((num_descs-1)<<3),
	    (length%0x10000 ? (length%0x10000)-1 : 0xffff))) return 0;
    }

    return 1;
}

static void do_int31(struct sigcontext_struct *scp)
{
#ifdef X86_EMULATOR
  if (debug_level('M')) {
    D_printf("DPMI: int31, ax=%04x, ebx=%08lx, ecx=%08lx, edx=%08lx\n",
	_LWORD(eax),_ebx,_ecx,_edx);
    D_printf("        edi=%08lx, esi=%08lx, ebp=%08lx, esp=%08lx\n",
	_edi,_esi,_ebp,_esp);
    D_printf("        cs=%04x, ds=%04x, ss=%04x, es=%04x, fs=%04x, gs=%04x\n",
	_cs,_ds,_ss,_es,_fs,_gs);
  }
#endif

  _eflags &= ~CF;
  switch (_LWORD(eax)) {
  case 0x0000:
    if (!(_LWORD(eax) = AllocateDescriptors(_LWORD(ecx)))) {
      _LWORD(eax) = 0x8011;
      _eflags |= CF;
    }
    break;
  case 0x0001:
    CHECK_SELECTOR(_LWORD(ebx));
    if (FreeDescriptor(_LWORD(ebx))){
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
    }
#if 1    
    /* do it dpmi 1.00 host\'s way */
    FreeSegRegs(scp, _LWORD(ebx));
#endif    
    break;
  case 0x0002:
    if (!(_LWORD(eax)=ConvertSegmentToDescriptor16(_LWORD(ebx)))) {
      _LWORD(eax) = 0x8011;
      _eflags |= CF;
    }
    break;
  case 0x0003:
    _LWORD(eax) = GetNextSelectorIncrementValue();
    break;
  case 0x0004:	/* Reserved lock selector, see interrupt list */
  case 0x0005:	/* Reserved unlock selector, see interrupt list */
    break;
  case 0x0006:
    CHECK_SELECTOR(_LWORD(ebx));
    {
      unsigned long baddress = GetSegmentBaseAddress(_LWORD(ebx));
      _LWORD(edx) = (us)(baddress & 0xffff);
      _LWORD(ecx) = (us)(baddress >> 16);
    }
    break;
  case 0x0007:
    CHECK_SELECTOR(_LWORD(ebx));
    if (SetSegmentBaseAddress(_LWORD(ebx), (_LWORD(ecx))<<16 | (_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    break;
  case 0x0008:
    CHECK_SELECTOR(_LWORD(ebx));
    if (SetSegmentLimit(_LWORD(ebx), ((unsigned long)(_LWORD(ecx))<<16) | (_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    break;
  case 0x0009:
    CHECK_SELECTOR(_LWORD(ebx));
    { int x;
      if ((x=SetDescriptorAccessRights(_LWORD(ebx), _LWORD(ecx))) !=0) {
        if (x == -1) _LWORD(eax) = 0x8021;
        else if (x == -2) _LWORD(eax) = 0x8022;
        else _LWORD(eax) = 0x8025;
        _eflags |= CF;
      }
    }
    break;
  case 0x000a:
    CHECK_SELECTOR(_LWORD(ebx));
    if (!(_LWORD(eax) = CreateCSAlias(_LWORD(ebx)))) {
       _LWORD(eax) = 0x8011;
      _eflags |= CF;
    }
    break;
  case 0x000b:
    GetDescriptor(_LWORD(ebx),
		(unsigned long *) (GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : _LWORD(edi)) ) );
    break;
  case 0x000c:
    CHECK_SELECTOR_ALLOC(_LWORD(ebx));
    if (SetDescriptor(_LWORD(ebx),
		      (unsigned long *) (GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : _LWORD(edi)) ) )) {
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
    }
    break;
  case 0x000d:
    if (AllocateSpecificDescriptor(_LWORD(ebx))) {
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
    }
    break;
  case 0x0100:			/* Dos allocate memory */
  case 0x0101:			/* Dos free memory */
  case 0x0102:			/* Dos resize memory */
    {
        D_printf("DPMI: call DOS memory service AX=0x%04X, BX=0x%04x, DX=0x%04x\n",
                  _LWORD(eax), _LWORD(ebx), _LWORD(edx));

	save_rm_regs();
	REG(eflags) = _eflags;
	REG(ebx) = _LWORD(ebx);
	REG(es) = GetSegmentBaseAddress(_LWORD(edx)) >> 4;

	switch(_LWORD(eax)) {
	  case 0x0100: {
	    unsigned short begin_selector, num_descs;
	    unsigned long length;
	    int i;

	    length = _LWORD(ebx) << 4;
	    num_descs = (length ? (DPMIclient_is_32 ? 1 : (length/0x10000 +
					((length%0x10000) ? 1 : 0))) : 0);
	    if (!num_descs) goto err;

	    if (!(begin_selector = AllocateDescriptors(num_descs))) goto err;
	    _LWORD(edx) = begin_selector;

	    if (SetSelector(begin_selector, 0, length-1, DPMIclient_is_32,
			    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;
	    for(i = 1; i < num_descs - 1; i++) {
		if (SetSelector(begin_selector + (i<<3), 0, 0xffff,
		    DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
			goto err;
	    }
	    if (num_descs > 1) {
		if (SetSelector(begin_selector + ((num_descs-1)<<3), 0,
			    (length%0x10000 ? (length%0x10000)-1 : 0xffff),
			    DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
		    goto err;
	    }

	    LWORD(eax) = 0x4800;
	  }
	  break;

	  case 0x0101:
	    if (!ResizeDescriptorBlock(scp, _LWORD(edx), 0))
		goto err;

	    LWORD(eax) = 0x4900;
	    break;

	  case 0x0102: {
	    unsigned long old_length;

	    if (!ValidAndUsedSelector(_LWORD(edx))) goto err;
	    old_length = GetSegmentLimit(_LWORD(edx)) + 1;
	    if (!ResizeDescriptorBlock(scp, _LWORD(edx), _LWORD(ebx) << 4))
		goto err;
	    _LWORD(ebx) = old_length >> 4; /* in case of a failure */

	    LWORD(eax) = 0x4a00;
	  }
	  break;
	}

	REG(cs) = DPMI_SEG;
	REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos_memory);
	in_dpmi_dos_int=1;
	return do_int(0x21); /* call dos for memory service */

err:
        D_printf("DPMI: DOS memory service failed\n");
	_eflags |= CF;
	restore_rm_regs();
	in_dpmi_dos_int = 0;
    }
    break;
  case 0x0200:	/* Get Real Mode Interrupt Vector */
    _LWORD(ecx) = ((us *) 0)[(_LO(bx) << 1) + 1];
    _LWORD(edx) = ((us *) 0)[_LO(bx) << 1];
    break;
  case 0x0201:	/* Set Real Mode Interrupt Vector */
    ((us *) 0)[(_LO(bx) << 1) + 1] = _LWORD(ecx);
    ((us *) 0)[_LO(bx) << 1] = (us) _LWORD(edx);
    break;
  case 0x0202:	/* Get Processor Exception Handler Vector */
    _LWORD(ecx) = Exception_Table[_LO(bx)].selector;
    _edx = Exception_Table[_LO(bx)].offset;
    D_printf("DPMI: Getting Excp %#x = %#x:%#lx\n", _LO(bx),_LWORD(ecx),_edx);
    break;
  case 0x0203:	/* Set Processor Exception Handler Vector */
    D_printf("DPMI: Setting Excp %#x = %#x:%#lx\n", _LO(bx),_LWORD(ecx),_edx);
    Exception_Table[_LO(bx)].selector = _LWORD(ecx);
    Exception_Table[_LO(bx)].offset = (DPMIclient_is_32 ? _edx : _LWORD(edx));
    break;
  case 0x0204:	/* Get Protected Mode Interrupt vector */
    _LWORD(ecx) = Interrupt_Table[_LO(bx)].selector;
    _edx = Interrupt_Table[_LO(bx)].offset;
    D_printf("DPMI: Get Prot. vec. bx=%x sel=%x, off=%lx\n", _LO(bx), _LWORD(ecx), _edx);
    break;
  case 0x0205:	/* Set Protected Mode Interrupt vector */
    Interrupt_Table[_LO(bx)].selector = _LWORD(ecx);
    Interrupt_Table[_LO(bx)].offset = (DPMIclient_is_32 ? _edx : _LWORD(edx));
    if (in_dpmi==1) { /* current_client==0 */
      switch (_LO(bx)) {
        case 0x1c:	/* ROM BIOS timer tick interrupt */
        case 0x23:	/* DOS Ctrl+C interrupt */
        case 0x24:	/* DOS critical error interrupt */
	  if ((Interrupt_Table[_LO(bx)].selector==DPMI_SEL) &&
		(Interrupt_Table[_LO(bx)].offset==DPMI_OFF + HLT_OFF(DPMI_interrupt) + _LO(bx)))
#ifdef __linux__
	    { if (can_revector(_LO(bx))==NO_REVECT)
	      reset_revectored(_LO(bx),&vm86s.int_revectored); }
	  else
	    set_revectored(_LO(bx),&vm86s.int_revectored);
#endif
	break;
/*        default: break; */
      }
    }
    D_printf("DPMI: Put Prot. vec. bx=%x sel=%x, off=%lx\n", _LO(bx),
      _LWORD(ecx), Interrupt_Table[_LO(bx)].offset);
    break;
  case 0x0300:	/* Simulate Real Mode Interrupt */
  case 0x0301:	/* Call Real Mode Procedure With Far Return Frame */
  case 0x0302:	/* Call Real Mode Procedure With Iret Frame */
    save_rm_regs();
    save_rm_context();
    RealModeContext = GetSegmentBaseAddress(_es) + (DPMIclient_is_32 ? _edi : _LWORD(edi));
    {
      struct RealModeCallStructure *rmreg = (struct RealModeCallStructure *) RealModeContext;
      us *ssp;
      unsigned char *rm_ssp;
      unsigned long rm_sp;
      int i;
#ifdef X86_EMULATOR
      int tmp;
      unsigned char *tmp_ssp;
#endif

      D_printf("DPMI: RealModeCallStructure at %#x\n",(int)RealModeContext);
      ssp = (us *) SEL_ADR(_ss, _esp);
      REG(edi) = rmreg->edi;
      REG(esi) = rmreg->esi;
      REG(ebp) = rmreg->ebp;
      REG(ebx) = rmreg->ebx;
      REG(edx) = rmreg->edx;
      REG(ecx) = rmreg->ecx;
      REG(eax) = rmreg->eax;
      REG(eflags) = (long) rmreg->flags | dpmi_mhp_TF;
      REG(es) = rmreg->es;
      REG(ds) = rmreg->ds;
      REG(fs) = rmreg->fs;
      REG(gs) = rmreg->gs;
      if (!(rmreg->sp==0)) {
	REG(ss) = rmreg->ss;
	REG(esp) = (long) rmreg->sp;
      }
      rm_ssp = (unsigned char *) (REG(ss) << 4);
      rm_sp = (unsigned long) LWORD(esp);
#ifdef X86_EMULATOR
      tmp_ssp = rm_ssp+rm_sp;
      tmp = E_MUNPROT_STACK(tmp_ssp);
#endif
      for (i=0;i<(_LWORD(ecx)); i++)
	pushw(rm_ssp, rm_sp, *(ssp+(_LWORD(ecx)) - 1 - i));
#ifdef X86_EMULATOR
      if (tmp) E_MPROT_STACK(tmp_ssp);
#endif
      LWORD(esp) -= 2 * (_LWORD(ecx));
      in_dpmi_dos_int=1;
      REG(cs) = DPMI_SEG;
      LWORD(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_realmode);
      switch (_LWORD(eax)) {
        case 0x0300:
          if (_LO(bx)==0x21)
            D_printf("DPMI: int 0x21 fn %04x\n",LWORD(eax));
	  do_int(_LO(bx));
	  break;
        case 0x0301:
	  fake_call_to(rmreg->cs, rmreg->ip);
	  break;
        case 0x0302:
	  fake_int_to(rmreg->cs, rmreg->ip);
	  break;
      }

/* --------------------------------------------------- 0x300:
     RM |  FC90C   |
	| dpmi_seg |
	|  flags   |
	| cx words |
   --------------------------------------------------- */
    }
#ifdef SHOWREGS
    if (debug_level('e')==0) {
      set_debug_level('g', debug_level('g') + 1);
      show_regs(__FILE__, __LINE__);
      set_debug_level('g', debug_level('g') - 1);
    }
#endif
    break;
  case 0x0303:	/* Allocate realmode call back address */
    {
       int i;
       for (i=0; i< 0x10; i++)
	 if ((realModeCallBack[current_client][i].selector == 0)&&
	     (realModeCallBack[current_client][i].offset == 0))
	    break;
       if ( i>= 0x10) {
	 D_printf("DPMI: Allocate real mode call back address failed.\n");
	 _eflags |= CF;
	 break;
       }
       if (!(realModeCallBack[current_client][i].rm_ss_selector
	     = AllocateDescriptors(1))) {
	 D_printf("DPMI: Allocate real mode call back address failed.\n");
	 _eflags |= CF;
	 break;
       }
	   
       realModeCallBack[current_client][i].selector = _ds;
       realModeCallBack[current_client][i].offset =
                                 (DPMIclient_is_32 ? _esi:_LWORD(esi)); 
       realModeCallBack[current_client][i].rmreg_selector = _es;
       realModeCallBack[current_client][i].rmreg_offset =
	                         (DPMIclient_is_32 ? _edi : _LWORD(edi));
       realModeCallBack[current_client][i].rmreg =
	                  (struct RealModeCallStructure *)
	                         (GetSegmentBaseAddress(_es) +
	                         (DPMIclient_is_32 ? _edi : _LWORD(edi)));
       _LWORD(ecx) = DPMI_SEG;
       _LWORD(edx) = DPMI_OFF + HLT_OFF(DPMI_realmode_callback)+i;
       D_printf("DPMI: Allocate realmode callback for 0x%0x4:0x%08lx use #%i callback address\n",
		realModeCallBack[current_client][i].selector,
		realModeCallBack[current_client][i].offset,i);
    }
    break;
  case 0x0304: /* free realmode call back address */
    {
       unsigned short offset, rcbase;
       rcbase = DPMI_OFF + HLT_OFF(DPMI_realmode_callback);
       offset = _LWORD(edx) - rcbase;
       if ((_LWORD(ecx) == DPMI_SEG) && (offset < 0x10)) {
	 realModeCallBack[current_client][offset].selector = 0;
	 realModeCallBack[current_client][offset].offset = 0;
	 FreeDescriptor(realModeCallBack[current_client][offset].rm_ss_selector);
       } else
	 _eflags |= CF;
    }
    break;
  case 0x0305:	/* Get State Save/Restore Adresses */
      _LWORD(ebx) = DPMI_SEG;
      _LWORD(ecx) = DPMI_OFF + HLT_OFF(DPMI_save_restore);
      _LWORD(esi) = DPMI_SEL;
      _edi = DPMI_OFF + HLT_OFF(DPMI_save_restore);
      _LWORD(eax) = 60;		/* size to hold all registers */
    break;
  case 0x0306:	/* Get Raw Mode Switch Adresses */
      _LWORD(ebx) = DPMI_SEG;
      _LWORD(ecx) = DPMI_OFF + HLT_OFF(DPMI_raw_mode_switch);
      _LWORD(esi) = DPMI_SEL;
      _edi = DPMI_OFF + HLT_OFF(DPMI_raw_mode_switch);
    break;
  case 0x0400:	/* Get Version */
    _LWORD(eax) = DPMI_VERSION << 8 | DPMI_DRIVER_VERSION;
    _LO(bx) = 5;
    _LO(cx) = vm86s.cpu_type;
    _LWORD(edx) = 0x0870; /* PIC base imaster/slave interrupt */
    break;
  case 0x0401:			/* Get DPMI Capabilities 1.0 */
      {
	  char *buf = (char *)SEL_ADR(_es, _edi);
	  /* report our capabilities include, exception */
	  /* restartability, conventional memory mapping, demand zero */
	  /* fill, write-protect client, write-protect host, is this */
	  /* right? */
	  _LWORD(eax) = 0x7a;
	  _LWORD(ecx) = 0;
	  _LWORD(edx) = 0;
	  *buf = DPMI_VERSION;
	  *(buf+1) = DPMI_DRIVER_VERSION;
	  sprintf(buf+2, "Linux DOSEMU Version %d.%d.%f\n", VERSION,
		  SUBLEVEL, PATCHLEVEL);
      }
	  
  case 0x0500:
    GetFreeMemoryInformation( (unsigned int *)
	(GetSegmentBaseAddress(_es) + (DPMIclient_is_32 ? _edi : _LWORD(edi))));
    break;
  case 0x0501:	/* Allocate Memory Block */
    { 
      dpmi_pm_block *block;
      unsigned long mem_required = (_LWORD(ebx))<<16 | (_LWORD(ecx));

      if ((block = DPMImalloc(mem_required)) == NULL) {
	D_printf("DPMI: client allocate memory failed.\n");
	_eflags |= CF;
	break;
      }
      D_printf("DPMI: malloc attempt for siz 0x%08x\n", (_LWORD(ebx))<<16 | (_LWORD(ecx)));
      D_printf("      malloc returns address %p\n", block->base);
      D_printf("                using handle 0x%08lx\n",block->handle);
      _LWORD(edi) = (block -> handle)&0xffff;
      _LWORD(esi) = ((block -> handle) >> 16) & 0xffff;
      _LWORD(ecx) = (unsigned long)block -> base & 0xffff;
      _LWORD(ebx) = ((unsigned long)block -> base >> 16) & 0xffff;
    }
    break;
  case 0x0502:	/* Free Memory Block */
    {
	D_printf(" DPMI: Free Mem Blk. for handle %08x\n",(_LWORD(esi))<<16 | (_LWORD(edi)));

        if(DPMIfree((_LWORD(esi))<<16 | (_LWORD(edi)))) {
	    D_printf("DPMI: client attempt to free a non exist handle\n");
	    _eflags |= CF;
	    break;
	}
    }
    break;
  case 0x0503:	/* Resize Memory Block */
    {
	unsigned long newsize, handle;
	dpmi_pm_block *block;
	handle = (_LWORD(esi))<<16 | (_LWORD(edi));
	newsize = (_LWORD(ebx)<<16 | _LWORD(ecx));
	D_printf("DPMI: Realloc to size %x\n", (_LWORD(ebx)<<16 | _LWORD(ecx)));
	D_printf("DPMI: For Mem Blk. for handle   0x%08lx\n", handle);

	if((block = DPMIrealloc(handle, newsize)) == NULL) {
	    D_printf("DPMI: client resize memory failed\n");
	    _eflags |= CF;
	    break;
	}
	D_printf("DPMI: realloc attempt for siz 0x%08lx\n", newsize);
	D_printf("      realloc returns address %p\n", block->base);
	_LWORD(ecx) = (unsigned long)(block->base) & 0xffff;
	_LWORD(ebx) = ((unsigned long)(block->base) >> 16) & 0xffff;
    }
    break;
  case 0x0504:			/* Allocate linear mem, 1.0 */
      {
	  unsigned long base_address = _ebx;
	  dpmi_pm_block *block;
	  unsigned long length = _ecx;
	  if (!length) {
	      _eflags |= CF;
	      _LWORD(eax) = 0x8021;
	      break;
	  }
	  if (base_address & 0xfff) {
	      _eflags |= CF;
	      _LWORD(eax) = 0x8025; /* not page aligned */
	      break;
	  }
	  if (!base_address)
	      block = DPMImalloc(length);
	  else
	      block = DPMImallocFixed(base_address, length);
	  if (block == NULL) {
	      if (!base_address)
		  _LWORD(eax) = 0x8013;	/* mem not available */
	      else
		  _LWORD(eax) = 0x8012;	/* linear mem not avail. */
	      _eflags |= CF;
	      break;
	  }
	  _ebx = (unsigned long)block -> base;
	  _esi = block -> handle;
	  D_printf("DPMI: allocate linear mem attempt for siz 0x%08lx at 0x%08lx\n",
		   _ecx, base_address);
	  D_printf("      malloc returns address %p\n", block->base);
	  D_printf("                using handle 0x%08lx\n",block->handle);
	  break;
      }
  case 0x0505:			/* Resize memory block, 1.0 */
    {
	unsigned long newsize, handle;
	dpmi_pm_block *block;
	unsigned short *sel_array;
	unsigned long old_base, old_len;
	
/* JES Unsure of ASSUMING sel_array OK to be nit to NULL */
	sel_array = NULL;
	handle = _esi;
	newsize = _ecx;

	if (!newsize) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8021; /* invalid value */
	    break;
	}

	D_printf("DPMI: Resize linear mem to size %lx\n", newsize);
	D_printf("DPMI: For Mem Blk. for handle   0x%08lx\n", handle);
	block = lookup_pm_block(handle);
	if(block == NULL || block -> handle != handle) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8023; /* invalid handle */
	    break;
	}
	old_base = (unsigned long)block -> base;
	old_len = block -> size;
	
	if(_edx & 0x2) {		/* update descriptor required */
	    sel_array = (unsigned short *)(GetSegmentBaseAddress(_es) +_ebx);
	    D_printf("DPMI: update descriptor required\n");
	    dpmi_cli();
	}
	if((block = DPMIrealloc(handle, newsize)) == NULL) {
	    _LWORD(eax) = 0x8012;
	    if(_edx & 0x2)
		dpmi_sti();
	    _eflags |= CF;
	    break;
	}
	_ebx = (unsigned long)block->base;
	_esi = block->handle;
	if(_edx & 0x2)	{	/* update descriptor required */
	    int i;
	    unsigned short sel;
	    unsigned long sel_base;
	    for(i=0; i< _edi; i++) {
		sel = sel_array[i];
		if (Segments[sel >> 3].used == 0)
		    continue;
		if (Segments[sel >> 3].type & MODIFY_LDT_CONTENTS_STACK) {
		    if(Segments[sel >> 3].is_big)
			sel_base = Segments[sel>>3].base_addr +
			    Segments[sel>>3].limit*DPMI_page_size-1;
		    else
			sel_base = Segments[sel>>3].base_addr + Segments[sel>>3].limit - 1;
		}else
		    sel_base = Segments[sel>>3].base_addr;
		if ((old_base <= sel_base) &&
		    ( sel_base < (old_base+old_len)))
		    SetSegmentBaseAddress(sel,
					  Segments[sel>>3].base_addr +
					  (unsigned long)block->base -
					  old_base);
	    }
	    dpmi_sti();
	}
    }
    break;
  case 0x0509:			/* Map conventional memory,1.0 */
    {
	unsigned long low_addr, handle, offset;
	dpmi_pm_block *block;
	
	handle = _esi;
	low_addr = _edx;
	offset = _ebx;
	
	if (low_addr > 0xf0000) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8003; /* system integrity */
	    break;
	}
	
	if ((low_addr & 0xfff) || (offset & 0xfff)) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8025; /* invalid linear address */
	    break;
	}

	D_printf("DPMI: Map conventional mem for handle %ld, offset %lx at low address %lx\n", handle, offset, low_addr);
	block = lookup_pm_block(handle);
	if(block == NULL || block -> handle != handle) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8023; /* invalid handle */
	    break;
	}
	if (DPMIMapConventionalMemory(block, offset, low_addr, _ecx))
	{
	    _LWORD(eax) = 0x8001;
	    _eflags |= CF;
	    break;
	}
    }
    break;
  case 0x050a:	/* Get mem block and base. 10 */
    {
	unsigned long handle;
	dpmi_pm_block *block;
	handle = (_LWORD(esi))<<16 | (_LWORD(edi));

	if((block = lookup_pm_block(handle)) == NULL) {
	    _LWORD(eax) = 0x8023;
	    _eflags |= CF;
	    break;
	}

	_LWORD(edi) = (block -> handle)&0xffff;
	_LWORD(esi) = ((block -> handle) >> 16) & 0xffff;
	_LWORD(ecx) = (unsigned long)block -> base & 0xffff;
	_LWORD(ebx) = ((unsigned long)block -> base >> 16) & 0xffff;
    }
    break;
  case 0x0600:	/* Lock Linear Region */
  case 0x0601:	/* Unlock Linear Region */
  case 0x0602:	/* Mark Real Mode Region as Pageable */
  case 0x0603:	/* Relock Real Mode Region */
    break;
  case 0x0604:	/* Get Page Size */
    _LWORD(ebx) = 0;
    _LWORD(ecx) = DPMI_page_size;
    break;
  case 0x0701:	/* Reserved, DISCARD PAGES, see interrupt lst */
    D_printf("DPMI: undoc. func. 0x0701 called\n");
    D_printf("      BX=0x%04x, CX=0x%04x, SI=0x%04x, DI=0x%04x\n",
			_LWORD(ebx), _LWORD(ecx), _LWORD(esi), _LWORD(edi));
    break;
  case 0x0900:	/* Get and Disable Virtual Interrupt State */
    _LO(ax) = (dpmi_eflags & IF) ? 1 : 0;
    dpmi_cli();
    break;
  case 0x0901:	/* Get and Enable Virtual Interrupt State */
    _LO(ax) = (dpmi_eflags & IF) ? 1 : 0;
    dpmi_sti();
    break;
  case 0x0902:	/* Get Virtual Interrupt State */
    _LO(ax) = (dpmi_eflags & IF) ? 1 : 0;
    break;
  case 0x0a00:	/* Get Vendor Specific API Entry Point */
    get_ext_API(scp);
    break;
  case 0x0b00:	/* Set Debug Breakpoint ->bx=handle(!CF) */
    {
      D_printf("DPMI: Set breakpoint type %x size %x at %04x%04x\n",
	_HI(dx),_LO(dx),_LWORD(ebx),_LWORD(ecx));
#ifdef X86_EMULATOR
      if (config.cpuemu>1) {
	e_dpmi_b0x(0,scp);
      } else
#endif
      {_LWORD(eax) = 0x8016;	/* n.i. */
	_eflags |= CF; }
    }
    break;
  case 0x0b01:	/* Clear Debug Breakpoint, bx=handle */
    {
      D_printf("DPMI: Clear breakpoint %x\n",_LWORD(ebx));
#ifdef X86_EMULATOR
      if (config.cpuemu>1) {
	e_dpmi_b0x(1,scp);
      } else
#endif
      { _LWORD(eax) = 0x8023;	/* n.i. */
	_eflags |= CF; }
    }
    break;
  case 0x0b02:	/* Get Debug Breakpoint State, bx=handle->ax=state(!CF) */
    {
      D_printf("DPMI: Breakpoint %x state\n",_LWORD(ebx));
#ifdef X86_EMULATOR
      if (config.cpuemu>1) {
	e_dpmi_b0x(2,scp);
      } else
#endif
      { _LWORD(eax) = 0x8023;	/* n.i. */
	_eflags |= CF; }
    }
    break;
  case 0x0b03:	/* Reset Debug Breakpoint, bx=handle */
    {
      D_printf("DPMI: Reset breakpoint %x\n",_LWORD(ebx));
#ifdef X86_EMULATOR
      if (config.cpuemu>1) {
	e_dpmi_b0x(3,scp);
      } else
#endif
      { _LWORD(eax) = 0x8023;	/* n.i. */
	_eflags |= CF; }
    }
    break;

  case 0x0e00:	/* Get Coprocessor Status */
    _LWORD(eax) = 0x4d;
    break;
  case 0x0e01:	/* Set Coprocessor Emulation */
    break;

  case 0x0506:			/* ??? */
  case 0x0507:			/* ??? */
  case 0x0508:			/* ??? */

  case 0x0700:	/* Reserved,MARK PAGES AS PAGING CANDIDATES, see intr. lst */
  case 0x0702:	/* Mark Page as Demand Paging Candidate */
  case 0x0703:	/* Discard Page Contents */
    D_printf("DPMI: unimplemented int31 func %#x\n",_LWORD(eax));
    break;

#if X_GRAPHICS
  case 0x0800: {
      unsigned addr, size, lfb;

      lfb = vga.mem.lfb_base_page << 12;
      addr = (_LWORD(ebx)) << 16 | (_LWORD(ecx));
      size = (_LWORD(esi)) << 16 | (_LWORD(edi));

      if((lfb && lfb == addr) || (addr == 0xa0000 && size == 0x10000)) {
        D_printf("DPMI: getting linear frame buffer at 0x%x, size 0x%x\n", addr, size);
        break;		/* physical == linear address in this case */
      }
    }
#endif

  default:
    D_printf("DPMI: unimplemented int31 func %#x\n",_LWORD(eax));
    _eflags |= CF;
  } /* switch */
  if (_eflags & CF)
    D_printf("DPMI: dpmi function failed, CF=1\n");
}

static void quit_dpmi(struct sigcontext_struct *scp, unsigned short errcode)
{
  if (in_dpmi == 1) { /* Restore environment, must before FreeDescriptor */
    if (CURRENT_ENV_SEL)
      *(unsigned short *)(((char *)(CURRENT_PSP<<4))+0x2c) =
             (unsigned long)(GetSegmentBaseAddress(CURRENT_ENV_SEL)) >> 4;
  }
  else
    D_printf("DPMI: warning: in_dpmi=%d\n",in_dpmi);

  FreeAllDescriptors();
  DPMIfreeAll();
  
  /* we must free ldt_buffer here, because FreeDescriptor() will */
  /* modify ldt_buffer */
  if (in_dpmi==1) {
    if (ldt_buffer) free(ldt_buffer);
    if (pm_stack) free(pm_stack);
  }
  cli_blacklisted = 0;
  in_dpmi_dos_int = 1;
  in_dpmi_pm_stack = 0;
  in_dpmi--;
  in_win31 = 0;
  if(pic_icount) {
    D_printf("DPMI: Warning: trying to leave DPMI when pic_icount=%li\n",
	pic_icount);
    pic_resched();
  }

  if (in_dpmi) {
    copy_context(scp, &dpmi_stack_frame[current_client]);
  }

  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  HI(ax) = 0x4c;
  LO(ax) = errcode;
  return (void) do_int(0x21);
}

static void do_dpmi_int(struct sigcontext_struct *scp, int i)
{

  if ((i == 0x2f) && (_LWORD(eax) == 0x168a))
    return get_ext_API(scp);
  if ((i == 0x2f) && (_LWORD(eax) == 0x1686)) {
    _eax = 0;
    D_printf("DPMI: CPU mode check in protected mode.\n");
    return;
  }
  if ((i == 0x21) && (_HI(ax) == 0x4c)) {
    D_printf("DPMI: leaving DPMI with error code 0x%02x\n",_LO(ax));
    quit_dpmi(scp, _LO(ax));
  } else if (i == 0x31) {
    switch (_LWORD(eax)) {
      case 0x0700:
      case 0x0701: {
        static int once=1;
        if (once) once=0;
        else {
          _eflags &= ~CF;
          return;
        }
      }
    }
    return do_int31(scp);
  } else {
    save_rm_regs();
    REG(eflags) = _eflags;
    REG(eax) = _eax;
    REG(ebx) = _ebx;
    REG(ecx) = _ecx;
    REG(edx) = _edx;
    REG(esi) = _esi;
    REG(edi) = _edi;
    REG(ebp) = _ebp;
    REG(cs) = DPMI_SEG;
    REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint) + i;
    if(msdos_pre_extender(scp, i)) {
	    restore_rm_regs();
	    return;
    }
    in_dpmi_dos_int = 1;
    D_printf("DPMI: calling real mode interrupt 0x%02x, ax=0x%04x\n",i,LWORD(eax));
    return (void) do_int(i);
  }
}

/* DANG_BEGIN_FUNCTION run_pm_int
 *
 * This routine is used for running protected mode hardware
 * interrupts and software interrupts 0x1c, 0x23 and 0x24.
 * run_pm_int() switches to the locked protected mode stack
 * and calls the handler. If no handler is installed the
 * real mode interrupt routine is called.
 *
 * DANG_END_FUNCTION
 */

void run_pm_int(int i)
{
  unsigned short CLIENT_PMSTACK_SEL;
  us *ssp;

  D_printf("DPMI: run_pm_int(0x%02x) called, in_dpmi_dos_int=0x%02x\n",i,in_dpmi_dos_int);

  if (Interrupt_Table[i].selector == DPMI_SEL) {

    D_printf("DPMI: Calling real mode handler for int 0x%02x\n", i);

    if (!in_dpmi_dos_int) {
      save_rm_regs();
      REG(cs) = DPMI_SEG;
      REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
      in_dpmi_dos_int = 1;
    }
    do_int(i);
    return;
  }

  if (!in_dpmi_pm_stack) {
    D_printf("DPMI: Switching to locked stack\n");
    CLIENT_PMSTACK_SEL = PMSTACK_SEL;
    if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
      error("DPMI: run_pm_int: App is working on host\'s PM locked stack, expect troubles!\n");
  }
  else {
    D_printf("DPMI: Not switching to locked stack, in_dpmi_pm_stack=%d\n",
      in_dpmi_pm_stack);
    CLIENT_PMSTACK_SEL = dpmi_stack_frame[current_client].ss;
  }

  if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL || in_dpmi_pm_stack)
    PMSTACK_ESP = client_esp(0);
  else
    PMSTACK_ESP = DPMI_pm_stack_size;

  if (PMSTACK_ESP < 100) {
      error("PM stack overflowed: in_dpmi_pm_stack=%i\n", in_dpmi_pm_stack);
      leavedos(25);
  }

  ssp = (us *) (GetSegmentBaseAddress(CLIENT_PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));

  D_printf("DPMI: Calling protected mode handler for int 0x%02x\n", i);
/* ---------------------------------------------------
	| 000FC925 | <- ssp here, executes pm int
	| dpmi_sel |
	|  eflags  |
	|   eip    | <- ssp at fc925 -> hlt, see line 2709
	|    cs    |
	|  eflags  |
	|   esp    |
	|    ss    |
	| i_d_d_i  |
   --------------------------------------------------- */
  if (DPMIclient_is_32) {
    *(--((unsigned long *) ssp)) = (unsigned long) in_dpmi_dos_int;
    *--ssp = (us) 0;
    *--ssp = dpmi_stack_frame[current_client].ss;
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].esp;
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].eflags;
    *--ssp = (us) 0;
    *--ssp = dpmi_stack_frame[current_client].cs; 
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].eip;
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].eflags;
    *--ssp = (us) 0;
    *--ssp = DPMI_SEL; 
    *(--((unsigned long *) ssp)) = DPMI_OFF + HLT_OFF(DPMI_return_from_pm);
    PMSTACK_ESP -= 36;
  } else {
    *--ssp = (unsigned short) in_dpmi_dos_int;
    *--ssp = dpmi_stack_frame[current_client].ss;
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].esp;
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].eflags;
    *--ssp = dpmi_stack_frame[current_client].cs; 
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].eip;
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].eflags;
    *--ssp = DPMI_SEL; 
    *--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_pm);
    PMSTACK_ESP -= 18;
  }
  dpmi_stack_frame[current_client].cs = Interrupt_Table[i].selector;
  dpmi_stack_frame[current_client].eip = Interrupt_Table[i].offset;
  dpmi_stack_frame[current_client].ss = CLIENT_PMSTACK_SEL;
  dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
  in_dpmi_pm_stack++;
  in_dpmi_dos_int = 0;
  dpmi_cli();
  dpmi_stack_frame[current_client].eflags &= ~(TF | NT);
}

void run_dpmi(void)
{
   int retval;
   unsigned char *csp;

  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */

  if (in_dpmi_dos_int) {

   csp = SEG_ADR((unsigned char *), cs, ip);

   /* a little optimization - if we already know that next insn is a hlt
    * and we are going to repeat it (same address as before)
    * there's no need to lose time calling vm86() again - AV
    */
   if (*csp == 0xf4) {
     if (debug_level('M')>3) D_printf("DPMI: skip 0xf4 at %p\n", csp);
     retval=VM86_UNKNOWN;
     /* kernel keeps IF always set */
     REG(eflags) |= IF;
   }
   else {
    if (
#ifdef TRACE_DPMI
	((debug_level('t')==0)||((REG(cs)!=0x70)&&(REG(eip)!=0x5b0)))&&
#endif
	(debug_level('M')>2)) {
	D_printf ("DPMI: do_vm86,  %04x:%04lx %08lx %08lx %08x\n", REG(cs),
		REG(eip), REG(esp), REG(eflags), dpmi_eflags);
    }
    in_vm86 = 1;
    retval=DO_VM86(&vm86s);
    in_vm86=0;

#if 0
    /* This will let foxpro run with cpu>=486 */
    if (_EFLAGS & (AC|ID)) {
      _EFLAGS &= ~(AC|ID);
      if (debug_level('M')>3)
	D_printf("BUG: AC,ID set; flags changed to %08x\n",_EFLAGS);
    }
#endif

    if (
#ifdef TRACE_DPMI
	(retval!=1)&&
#endif
	(debug_level('M')>3)) {
	D_printf ("DPMI: ret_vm86, %04x:%04lx %08lx %08lx %08x ret=%#x\n",
		REG(cs), REG(eip), REG(esp), REG(eflags), dpmi_eflags, retval);
#ifdef TRACE_DPMI
	D_printf ("ax=%04x bx=%04x ss=%04x sp=%04x bp=%04x\n"
	      		 "           cx=%04x dx=%04x ds=%04x cs=%04x ip=%04x\n"
	      		 "           si=%04x di=%04x es=%04x flg=%08x\n",
			_AX, _BX, _SS, _SP, _BP, _CX, _DX, _DS, _CS, _IP,
			_SI, _DI, _ES, _EFLAGS);
#endif
    }
  } /* not an HLT insn */

    if (REG(eflags)&IF) {
      if (!(dpmi_eflags&IF))
        dpmi_sti();
    } else {
#ifdef X86_EMULATOR
      if (config.cpuemu<2) D_printf("DPMI: strange...IF clear, why?\n");
#endif
      if (dpmi_eflags&IF)
        dpmi_cli();
    }

    switch VM86_TYPE(retval) {
	case VM86_UNKNOWN:
		vm86_GP_fault();
		break;
	case VM86_STI:
#ifdef X86_EMULATOR
		D_printf("DPMI: Return from vm86() for timeout\n");
#endif
		pic_iret();
		break;
	case VM86_INTx:
#ifdef X86_EMULATOR
		D_printf("DPMI: Return from vm86() for interrupt\n");
#endif
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
#endif
    D_printf("DPMI: retval=%x %x:%x\n", VM86_ARG(retval), _CS, _IP);
		switch (VM86_ARG(retval)) {
		  case 0x1c:	/* ROM BIOS timer tick interrupt */
		  case 0x23:	/* DOS Ctrl+C interrupt */
		  case 0x24:	/* DOS critical error interrupt */
			if (SEGOFF2LINEAR(BIOSSEG, INT_OFF(VM86_ARG(retval))) !=
 			    SEGOFF2LINEAR(_CS, _IP) -2)
			  run_pm_int(VM86_ARG(retval));
			else
			  do_int(VM86_ARG(retval));
			break;
		  default:
#ifdef USE_MHPDBG
			mhp_debug(DBG_INTx + (VM86_ARG(retval) << 8), 0, 0);
#endif
			do_int(VM86_ARG(retval));
		}
		break;
#ifdef USE_MHPDBG
	case VM86_TRAP:
#ifdef X86_EMULATOR
		D_printf("DPMI: Return from vm86() for trap\n");
#endif
		if (!mhp_debug(DBG_TRAP + (VM86_ARG(retval) << 8), 0, 0))
#ifdef TRACE_DPMI
		   if ((debug_level('t')==0)||(VM86_ARG(retval)!=1))
#endif
		   do_int(VM86_ARG(retval));
		break;
#endif
	case VM86_PICRETURN:
	case VM86_SIGNAL:
		break;
	default:
		error("DPMI: unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
		fatalerr = 4;
    }
  }
  else {
    int retcode;
    retcode = (
#ifdef X86_EMULATOR
	config.cpuemu>1?
	e_dpmi(&dpmi_stack_frame[current_client]) :
#endif
	dpmi_control());
#ifdef USE_MHPDBG
    if (retcode && mhpdbg.active) {
      if ((retcode ==1) || (retcode ==3)) mhp_debug(DBG_TRAP + (retcode << 8), 0, 0);
      else mhp_debug(DBG_INTxDPMI + (retcode << 8), 0, 0);
    }
#endif
  }

freeze_idle:
    handle_signals();

    /* catch user hooks here */
    if (uhook_fdin != -1) uhook_poll();

    /* here we include the hooks to possible plug-ins */
    #define VM86_RETURN_VALUE retval
    #include "plugin_poll.h"
    #undef VM86_RETURN_VALUE

#ifdef USE_MHPDBG  
    if (mhpdbg.active) mhp_debug(DBG_POLL, 0, 0);
#endif

  if (iq.queued)
    do_queued_ioctl();

  if (dosemu_frozen) {
    static int minpoll = 0;
    if (!mhpdbg.active) {
      if (!(++minpoll & 7)) usleep(10000);
      g_printf("DPMI: freeze: signal loop\n");
      goto freeze_idle;
    } else {
      unfreeze_dosemu();
    }
  }
}

static void dpmi_init(void)
{
  /* Holding spots for REGS and Return Code */
  unsigned short CS, DS, ES, SS;
  unsigned char *ssp;
  unsigned long sp;
  unsigned int my_ip, my_cs, my_sp, psp, i;
  unsigned char *cp;

  CARRY;

  if (!config.dpmi)
    return;

  D_printf("DPMI: initializing\n");
  if (in_dpmi>=DPMI_MAX_CLIENTS) {
    p_dos_str("Sorry, only %d DPMI clients supported under DOSEMU :-(\n", DPMI_MAX_CLIENTS);
    return;
  }

  in_dpmi++;

  if(in_dpmi == 1) {
    struct meminfo *mi;

#ifdef X86_EMULATOR
    FLUSH_TREE;
#endif

 /* DANG_FIXTHIS Should we really care for the Memory info? 
    After the next task switch everything may have changed substantially
    bon@elektron.ikp.physik.th-darmstadt.de 2/16/97 */
    mi = readMeminfo();
    if (mi) {
      unsigned long maxmem = mi->free + mi->swapfree + mi->buffers + mi->cached;
      dpmi_free_memory = (maxmem <
			  (((unsigned long)config.dpmi)*1024))?
                   	  maxmem:
                	  ((unsigned long)config.dpmi)*1024;
    }
    else
     dpmi_free_memory = ((unsigned long)config.dpmi)*1024;

    /* make it page aligned */
    dpmi_free_memory = (dpmi_free_memory & 0xfffff000) +
	                              ((dpmi_free_memory & 0xfff)
				      ? DPMI_page_size : 0);
    dpmi_total_memory = dpmi_free_memory;
    D_printf("DPMI: dpmi_free_memory available 0x%lx\n",dpmi_free_memory); 
    
    DPMI_rm_procedure_running = 0;

    DPMIclient_is_32 = LWORD(eax) ? 1 : 0;
    DPMI_private_data_segment = REG(es);

    ldt_buffer = malloc(LDT_ENTRIES*LDT_ENTRY_SIZE);
    if (ldt_buffer == NULL) {
      error("DPMI: can't allocate memory for ldt_buffer\n");
      goto err;
    }

    pm_stack = malloc(DPMI_pm_stack_size);
    if (pm_stack == NULL) {
      error("DPMI: can't allocate memory for locked protected mode stack\n");
      free(ldt_buffer);
      goto err;
    }

    D_printf("Freeing descriptors\n");
    { int dd=debug_level('M'); set_debug_level('M', 0); /* don't be unnecessarily verbose */
      for (i=0;i<MAX_SELECTORS;i++) {
	  FreeDescriptor(i<<3);
      }
      set_debug_level('M', dd);
    }
    D_printf("Descriptors freed\n");

#if 0
    /* all selectors are used */
    memset(ldt_buffer,0xff,LDT_ENTRIES*LDT_ENTRY_SIZE);
#else
    /* Note: when get_ldt() is called _before_ any descriptor allocation
     * (e.g. when no modify_ldt(WRITE, ...) was called), then we get
     * _not_ the whole LDT, but only the DEFAULT static one.
     * So better to clear the ldt_buffer with ZERO
     */
    memset(ldt_buffer,0,LDT_ENTRIES*LDT_ENTRY_SIZE);
#endif
    get_ldt(ldt_buffer);
    flush_log();

    pm_block_handle_used = 1;
    USER_DTA_SEL = 0;		/* from msdos.h */
    USER_PSP_SEL = 0;		/* from msdos.h */
/*
 * DANG_BEGIN_NEWIDEA
 * Simulate Local Descriptor Table for MS-Windows 3.1
 * must be read only, so if krnl386.exe/krnl286.exe
 * try to write to this table, we will bomb into sigsegv()
 * and and emulate direct ldt access
 * DANG_END_NEWIDEA
 */

    if (!(LDT_ALIAS = AllocateDescriptors(1))) goto err;
    if (SetSelector(LDT_ALIAS, (unsigned long) ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE-1, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 1, 0, 0, 0)) goto err;
    
    if (!(PMSTACK_SEL = AllocateDescriptors(1))) goto err;
    if (SetSelector(PMSTACK_SEL, (unsigned long) pm_stack, DPMI_pm_stack_size-1, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;

    if (!(DPMI_SEL = AllocateDescriptors(1))) goto err;
    if (SetSelector(DPMI_SEL, (unsigned long) (DPMI_SEG << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0)) goto err;

    for (i=0;i<0x100;i++) {
      Interrupt_Table[i].offset = DPMI_OFF + HLT_OFF(DPMI_interrupt) + i;
      Interrupt_Table[i].selector = DPMI_SEL;
    }
    for (i=0;i<0x20;i++) {
      Exception_Table[i].offset = DPMI_OFF + HLT_OFF(DPMI_exception) + i;
      Exception_Table[i].selector = DPMI_SEL;
    }

    dpmi_eflags = IF;
    
  } else {
    if (DPMIclient_is_32 != (LWORD(eax) ? 1 : 0))
      goto err;
  }

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

  psp = LWORD(ebx);
  LWORD(ebx) = popw(ssp, sp);

  my_ip = popw(ssp, sp);
  my_cs = popw(ssp, sp);

  if (debug_level('M')) {
    cp = (unsigned char *) ((my_cs << 4) +  my_ip);

    D_printf("Going protected with fingers crossed\n"
		"32bit=%d, CS=%04x SS=%04x DS=%04x PSP=%04x ip=%04x sp=%04lx\n",
		LO(ax), my_cs, LWORD(ss), LWORD(ds), psp, my_ip, REG(esp));
  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
    D_printf("OPS  : ");
    cp -= 10;
    for (i = 0; i < 10; i++)
      D_printf("%02x ", *cp++);
    D_printf("-> ");
    for (i = 0; i < 10; i++)
      D_printf("%02x ", *cp++);
    D_printf("\n");

    D_printf("STACK: ");
    sp = (sp & 0xffff0000) | (((sp & 0xffff) - 10 ) & 0xffff);
    for (i = 0; i < 10; i++)
      D_printf("%02lx ", popb(ssp, sp));
    D_printf("-> ");
    for (i = 0; i < 10; i++)
      D_printf("%02lx ", popb(ssp, sp));
    D_printf("\n");
    flush_log();
  }

  if (!(CS = AllocateDescriptors(1))) goto err;
  if (SetSelector(CS, (unsigned long) (my_cs << 4), 0xffff, 0,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0)) goto err;

  if (!(SS = AllocateDescriptors(1))) goto err;
  if (SetSelector(SS, (unsigned long) (LWORD(ss) << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;

  if (LWORD(ss) == LWORD(ds))
    DS=SS;
  else {
    if (!(DS = AllocateDescriptors(1))) goto err;
    if (SetSelector(DS, (unsigned long) (LWORD(ds) << 4), 0xffff, DPMIclient_is_32,
                    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;
  }

  if (!(ES = AllocateDescriptors(1))) goto err;
  if (SetSelector(ES, (unsigned long) (psp << 4), 0x00ff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;

  {/* convert environment pointer to a descriptor*/
     unsigned short envp, envpd;
     envp = *(unsigned short *)(((char *)(psp<<4))+0x2c);
     if (envp) {
	if(!(envpd = AllocateDescriptors(1))) goto err;
#if 0
	if (SetSelector(envpd, (unsigned long) (envp << 4), 0x03ff,
#else
	/* windows is accessing envp:0x0400 */
	if (SetSelector(envpd, (unsigned long) (envp << 4), 0x0ffff,
#endif
		DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;
	*(unsigned short *)(((char *)(psp<<4))+0x2c) = envpd;

        CURRENT_ENV_SEL = envpd;
	D_printf("DPMI: env segment %#x converted to descriptor %#x\n",
		envp,envpd);
     } else
          CURRENT_ENV_SEL = 0;
     CURRENT_PSP = psp;
  }

  if (debug_level('M')) {
    print_ldt();
    D_printf("LDT_ALIAS=%x DPMI_SEL=%x CS=%x DS=%x SS=%x ES=%x\n", LDT_ALIAS, DPMI_SEL, CS, DS, SS, ES);
  }

  REG(esp) += 6;
  my_sp = LWORD(esp);
  NOCARRY;

  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);

  /* set int 23 to "iret" so that DOS doesn't terminate the program
     behind our back */
  SETIVEC(0x23, BIOSSEG, INT_OFF(0x68));

  in_win31 = 0;
  in_dpmi_dos_int = 0;
  if(pic_icount) {
    D_printf("DPMI: Warning: trying to enter DPMI when pic_icount=%li\n",
	pic_icount);
    pic_resched();
  }
  pm_block_root[current_client] = 0;
  memset((void *)(&realModeCallBack[current_client][0]), 0,
	 sizeof(RealModeCallBack)*0x10);
  dpmi_stack_frame[current_client].eip	= my_ip;
  dpmi_stack_frame[current_client].cs	= CS;
  dpmi_stack_frame[current_client].esp	= my_sp;
  dpmi_stack_frame[current_client].ss	= SS;
  dpmi_stack_frame[current_client].ds	= DS;
  dpmi_stack_frame[current_client].es	= ES;
  dpmi_stack_frame[current_client].fs	= 0;
  dpmi_stack_frame[current_client].gs	= 0;
  dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0cd5 & REG(eflags));
  dpmi_stack_frame[current_client].eax = REG(eax);
  dpmi_stack_frame[current_client].ebx = REG(ebx);
  dpmi_stack_frame[current_client].ecx = REG(ecx);
  dpmi_stack_frame[current_client].edx = REG(edx);
  dpmi_stack_frame[current_client].esi = REG(esi);
  dpmi_stack_frame[current_client].edi = REG(edi);
  dpmi_stack_frame[current_client].ebp = REG(ebp);

  if (in_dpmi>1) return; /* return immediately to the main loop */

  for (; (!fatalerr && in_dpmi) ;) {
    if (debug_level('M')>6) {
#ifdef TRACE_DPMI
	if (debug_level('t')==0)
#endif
	D_printf("------ DPMI: dpmi loop ---------------------\n");
    }
    run_dpmi();
    serial_run();
    dma_run();
#ifdef USE_SBEMU
    run_sb(); /* Suggested Karcher */
#endif
    pic_run();
  }
  if (debug_level('M')>6) D_printf("DPMI: end dpmi loop\n");
  return;

err:
  FreeAllDescriptors();
  in_dpmi--;
}

void dpmi_sigio(struct sigcontext_struct *scp)
{
#ifdef X86_EMULATOR
  if (in_dpmi_emu || (_cs != UCODESEL)) {
#else
  if (_cs != UCODESEL){
#endif
/* DANG_FIXTHIS We shouldn't return to dosemu code if IF=0, but it helps - WHY? */
/*
   Because IF is not set by popf and because dosemu have to do some background
   job (like DMA transfer) regardless whether IF is set or not.
*/
    D_printf("DPMI: return to dosemu code for handling signals\n");
    Return_to_dosemu_code(scp,0);
  }
}

/*
 * DANG_BEGIN_FUNCTION do_default_cpu_exception
 *
 * This is the default CPU exception handler.
 * Exceptions 0, 1, 2, 3, 4, 5 and 7 are reflected
 * to real mode. All other exceptions are terminating the client
 * (and may be dosemu too :-)).
 *
 * DANG_END_FUNCTION
 */

static  void do_default_cpu_exception(struct sigcontext_struct *scp, int trapno)
{ 
  switch (trapno) {
    case 0x00: /* divide_error */
    case 0x01: /* debug */
    case 0x03: /* int3 */
    case 0x04: /* overflow */
    case 0x05: /* bounds */
    case 0x07: /* device_not_available */
#ifdef TRACE_DPMI
	       if (debug_level('t') && (trapno==1)) {
	         if (debug_level('t')>1)
			dbug_printf("\n%s",e_scp_disasm(scp,1));
		 return;
	       }
#endif
	       D_printf("DPMI: do_default_cpu_exception %d at %#x:%#x\n",
		 trapno, (int)_cs, (int)_eip);
	       save_rm_regs();
	       REG(cs) = DPMI_SEG;
	       REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
	       in_dpmi_dos_int = 1;
	       return (void) do_int(trapno);
    default:
	       p_dos_str("DPMI: Unhandled Exception %02x - Terminating Client\n"
			 "It is likely that dosemu is unstable now and should be rebooted\n",
			 trapno);
	       flush_log();
	       quit_dpmi(scp, 0xff);
  }
}

/*
 * DANG_BEGIN_FUNCTION do_cpu_exception
 *
 * This routine switches to the locked protected mode stack,
 * disables interrupts and calls the DPMI client exception handler.
 * If no handler is installed the default handler is called.
 *
 * DANG_END_FUNCTION
 */

#ifdef __linux__
static void do_cpu_exception(struct sigcontext_struct *scp)
#endif
{
  unsigned short CLIENT_PMSTACK_SEL;
  us *ssp;

#ifdef DPMI_DEBUG
  /* My log file grows to 2MB, I have to turn off dpmi debugging,
     so this log exceptions even if dpmi debug is off */
  unsigned char dd = debug_level('M');
  set_debug_level('M', 1);
#endif

  mhp_intercept("\nCPU Exception occured, invoking dosdebug\n\n", "+9M");

#ifdef TRACE_DPMI
  if (debug_level('t') && (_trapno == 1)) {
    do_default_cpu_exception(scp, _trapno);
    return;
  }
#endif

  D_printf("DPMI: do_cpu_exception(0x%02lx) at %#x:%#x\n",_trapno,
  	(int)_cs, (int)_eip);
  if (_trapno == 0xe) {
      set_debug_level('M', 9);
      error("DPMI: page fault. in dosemu?\n");
      /* why should we let dpmi continue after this point and crash
       * the system? */
      flush_log();
      DPMI_show_state(scp);
      leavedos(0x5046);
  }

  if ((_trapno != 0xe)
#ifdef X86_EMULATOR
      || debug_level('e')
#endif
     )
    { DPMI_show_state(scp); }
#ifdef SHOWREGS
  print_ldt();
#endif
  if ((_trapno == 0xe)
#ifdef X86_EMULATOR
	&& (config.cpuemu==0)
#endif
     )
    leavedos(98);
#ifdef DPMI_DEBUG
  set_debug_level('M', dd);
#endif
  
  if (Exception_Table[_trapno].selector == DPMI_SEL) {
    do_default_cpu_exception(scp, _trapno);
    return;
  }

  if (!in_dpmi_pm_stack) {
    D_printf("DPMI: Switching to locked stack\n");
    CLIENT_PMSTACK_SEL = PMSTACK_SEL;
    if (_ss == PMSTACK_SEL)
      error("DPMI: do_cpu_exception: App is working on host\'s PM locked stack, expect troubles!\n");
  }
  else {
    D_printf("DPMI: Not switching to locked stack, in_dpmi_pm_stack=%d\n",
      in_dpmi_pm_stack);
    CLIENT_PMSTACK_SEL = _ss;
  }

  if (_ss == PMSTACK_SEL || in_dpmi_pm_stack) 
    PMSTACK_ESP = client_esp(scp);
  else
    PMSTACK_ESP = DPMI_pm_stack_size;

  if (PMSTACK_ESP < 100) {
      error("PM stack overflowed: in_dpmi_pm_stack=%i\n", in_dpmi_pm_stack);
      leavedos(25);
  }

  ssp = (us *) (GetSegmentBaseAddress(CLIENT_PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));

  if (DPMIclient_is_32) {
    *--ssp = (us) 0;
    *--ssp = _ss;
    *(--((unsigned long *) ssp)) = _esp;
    *(--((unsigned long *) ssp)) = (_eflags&~IF)|(dpmi_eflags&IF);
    *--ssp = (us) 0;
    *--ssp = _cs;
    *(--((unsigned long *) ssp)) = _eip;
    *(--((unsigned long *) ssp)) = _err;
    *--ssp = (us) 0;
    *--ssp = DPMI_SEL; 
    *(--((unsigned long *) ssp)) = DPMI_OFF + HLT_OFF(DPMI_return_from_exception);
    PMSTACK_ESP -= 32;
  } else {
    *--ssp = _ss;
    *--ssp = (unsigned short) _esp;
    *--ssp = (unsigned short) ((_eflags&~IF)|(dpmi_eflags&IF));
    *--ssp = _cs; 
    *--ssp = (unsigned short) _eip;
    *--ssp = (unsigned short) _err;
    *--ssp = DPMI_SEL; 
    *--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_exception);
    PMSTACK_ESP -= 16;
  }
  _cs = Exception_Table[_trapno].selector;
  _eip = Exception_Table[_trapno].offset;
  D_printf("DPMI: Exception Table jump to %04x:%08lx\n",_cs,_eip);
  _ss = CLIENT_PMSTACK_SEL;
  _esp = PMSTACK_ESP;
  in_dpmi_pm_stack++;
  dpmi_cli();
  _eflags &= ~TF;
}

/*
 * DANG_BEGIN_FUNCTION dpmi_fault
 *
 * This is the brain of DPMI. All CPU exceptions are first
 * reflected (from the signal handlers) to this code.
 *
 * Exception from nonprivileged instructions INT XX, STI, CLI, HLT
 * and from WINDOWS 3.1 are handled here.
 *
 * All here unhandled exceptions are reflected to do_cpu_exception()
 *
 * Note for cpu-emu: exceptions generated from the emulator are handled
 *  here. 'Real' system exceptions (e.g. from an emulator fault) are
 *  redirected to emu_dpmi_fault() in fullemu mode
 *
 * DANG_END_FUNCTION
 */

#ifdef __linux__
void dpmi_fault(struct sigcontext_struct *scp)
#endif
{

#define LWORD32(x) (DPMIclient_is_32 ? (unsigned long) _##x : _LWORD(x))
#define _LWECX	   (DPMIclient_is_32 ^ prefix67 ? _ecx : _LWORD(ecx))

  us *ssp;
  unsigned char *csp, *lina;

#if 0
/* 
 * If we have a 16-Bit stack segment the high word of esp is not always
 * zero as expected, esp. in winos2
 */
if ((_ss & 4) == 4) {
  /* stack segment from ldt */
  if (Segments[_ss>>3].used) {
    if (Segments[_ss>>3].is_32==0)
      _HWORD(esp)=0;
  } else
    D_printf("DPMI: why in the hell the stack segment 0x%04x is marked as not used?\n",_ss);
}
#endif

  csp = lina = (unsigned char *) SEL_ADR(_cs, _eip);
  /* should we use _sp instead for 16-bit stack segments? */
  ssp = (us *) SEL_ADR(_ss, _esp);
  
#ifdef USE_MHPDBG
  if (mhpdbg.active) {
    if (_trapno == 3) {
       Return_to_dosemu_code(scp,3);
       return;
    }
    if (dpmi_mhp_TF && (_trapno == 1)) {
      _eflags &= ~TF;
      switch (csp[-1]) {
        case 0x9c:	/* pushf */
	  ssp[0] &= ~TF;
	  break;
        case 0x9f:	/* lahf */
	  _eax &= ~(TF << 8);
	  break;
      }
      dpmi_mhp_TF=0;
      Return_to_dosemu_code(scp,1);
      return;
    }
  }
#endif
  if (_trapno == 13) {
    Bit32u org_eip;
    int pref_seg;
    int done,is_rep,prefix66,prefix67;
    
    /* DANG_BEGIN_REMARK
     * Here we handle all prefixes prior switching to the appropriate routines
     * The exception CS:EIP will point to the first prefix that effects the
     * the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
     * So we collect all prefixes and remember them.
     * - Hans Lermen
     * DANG_END_REMARK
     */

    done=0;
    is_rep=0;
    prefix66=prefix67=0;
    pref_seg=-1;

    do {
      switch (*(csp++)) {
         case 0x66:      /* operand prefix */  prefix66=1; break;
         case 0x67:      /* address prefix */  prefix67=1; break;
         case 0x2e:      /* CS */              pref_seg=_cs; break;
         case 0x3e:      /* DS */              pref_seg=_ds; break;
         case 0x26:      /* ES */              pref_seg=_es; break;
         case 0x36:      /* SS */              pref_seg=_ss; break;
         case 0x65:      /* GS */              pref_seg=_gs; break;
         case 0x64:      /* FS */              pref_seg=_fs; break;
         case 0xf2:      /* repnz */
         case 0xf3:      /* rep */             is_rep=1; break;
         default: done=1;
      }
    } while (!done);
    csp--;
    org_eip = _eip;
    _eip += (csp-lina);

#ifdef X86_EMULATOR
    if (config.cpuemu>1) {
	/* trick, because dpmi_fault must return void */
	_trapno = *csp;
#ifdef CPUEMU_DIRECT_IO
	if (InCompiledCode && !DPMIclient_is_32) {
	    prefix66 ^= 1; prefix67 ^= 1; /* since we come from 32-bit code */
/**/ e_printf("dpmi_fault: adjust prefixes to 66=%d,67=%d\n",
		prefix66,prefix67);
	}
#endif
    }
#endif

    switch (*csp++) {

    case 0xcd:			/* int xx */
#ifdef USE_MHPDBG
      if (mhpdbg.active) {
        if (dpmi_mhp_intxxtab[*csp]) {
          int ret=dpmi_mhp_intxx_check(scp, *csp);
          if (ret) {
            Return_to_dosemu_code(scp,ret);
            return;
          }
        }
      }
#endif
      /* Bypass the int instruction */
      _eip += 2;
      if (Interrupt_Table[*csp].selector == DPMI_SEL)
	do_dpmi_int(scp, *csp);
      else {
        us cs2 = _cs;
        unsigned long eip2 = _eip;
	if (DPMIclient_is_32) {
	  *(--((unsigned long *) ssp)) = _eflags;
	  *--ssp = (us) 0;
	  *--ssp = _cs;
	  *(--((unsigned long *) ssp)) = _eip;
	  _esp -= 12;
	} else {
	  *--ssp = _LWORD(eflags);
	  *--ssp = _cs;
	  *--ssp = _LWORD(eip);
	  _esp -= 6;
	}
	if (*csp<=7) {
	  dpmi_cli();
	}
	_eflags &= ~(TF | NT);
	_cs = Interrupt_Table[*csp].selector;
	_eip = Interrupt_Table[*csp].offset;
	D_printf("DPMI: call inthandler %#02x(%#04x) at %#04x:%#08lx\n\t\tret=%#04x:%#08lx\n",
		*csp, _LWORD(eax), _cs, _eip, cs2, eip2);
	if ((*csp == 0x2f)&&((_LWORD(eax)==
			      0xfb42)||(_LWORD(eax)==0xfb43)))
	    D_printf("DPMI: dpmiload function called, ax=0x%04x,bx=0x%04x\n"
		     ,_LWORD(eax), _LWORD(ebx));
	if ((*csp == 0x21) && (_HI(ax) == 0x4c))
	    D_printf("DPMI: DOS exit called\n");
      }
      break;

    case 0xf4:			/* hlt */
      _eip += 1;
      if (_cs==DPMI_SEL) {
	if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_raw_mode_switch)) {
	  D_printf("DPMI: switching from protected to real mode\n");
	  REG(ds) = (long) _LWORD(eax);
	  REG(es) = (long) _LWORD(ecx);
	  REG(ss) = (long) _LWORD(edx);
	  REG(esp) = (long) _LWORD(ebx);
	  REG(cs) = (long) _LWORD(esi);
	  REG(eip) = (long) _LWORD(edi);
	  REG(ebp) = _ebp;
	  in_dpmi_dos_int = 1;

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_save_restore)) {
	  unsigned long *buffer = (unsigned long *) SEL_ADR(_es, _edi);
	  if (_LO(ax)==0) {
            D_printf("DPMI: save real mode registers\n");
	    *buffer++ = REG(eax);
	    *buffer++ = REG(ebx);
	    *buffer++ = REG(ecx);
	    *buffer++ = REG(edx);
	    *buffer++ = REG(esi);
	    *buffer++ = REG(edi);
	    *buffer++ = REG(esp);
	    *buffer++ = REG(ebp);
	    *buffer++ = REG(eip);
	    *buffer++ = REG(cs);
	    *buffer++ = REG(ds);
	    *buffer++ = REG(ss);
	    *buffer++ = REG(es);
	    *buffer++ = REG(fs);
	    *buffer++ = REG(gs);  
	  } else {
            D_printf("DPMI: restore real mode registers\n");
	    REG(eax) = *buffer++;
	    REG(ebx) = *buffer++;
	    REG(ecx) = *buffer++;
	    REG(edx) = *buffer++;
	    REG(esi) = *buffer++;
	    REG(edi) = *buffer++;
	    REG(esp) = *buffer++;
	    REG(ebp) = *buffer++;
	    REG(eip) = *buffer++;
	    REG(cs) =  *buffer++;
	    REG(ds) =  *buffer++;
	    REG(ss) =  *buffer++;
	    REG(es) =  *buffer++;
            REG(fs) =  *buffer++;
	    REG(gs) =  *buffer++;
          }/* _eip point to FAR RET */

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_API_extension)) {
          D_printf("DPMI: extension API call: 0x%04x\n", _LWORD(eax));
          if (_LWORD(eax) == 0x0100) {
            _eax = LDT_ALIAS;  /* simulate direct ldt access */
	    _eflags &= ~CF;
	    in_win31 = 1;
	  } else
            _eflags |= CF;

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_pm)) {
          D_printf("DPMI: Return from protected mode interrupt handler\n");
	  if (in_dpmi_pm_stack) {
	    in_dpmi_pm_stack--;
	    if (!in_dpmi_pm_stack && _ss != PMSTACK_SEL) {
	      error("DPMI: Client's PM Stack corrupted during interrupt handling!\n");
//	      leavedos(91);
	    }
	  }
/* ---------------------------------------------------
	|(000FC925)|
	|(dpmi_sel)|
	|( eflags )|
	|   eip    | <- ssp here
	|    cs    |
	|  eflags  |
	|   esp    |
	|    ss    |
	| i_d_d_i  |
   --------------------------------------------------- */
	  if (DPMIclient_is_32) {
	    _eip = *(((unsigned long *) ssp)++);
	    _cs = *ssp++;
	    ssp++;
	    _eflags = *(((unsigned long *) ssp)++);
	    _esp = *(((unsigned long *) ssp)++);
	    _ss = *ssp++;
	    ssp++;
	    in_dpmi_dos_int = (int) *(((unsigned long *) ssp)++);
	  } else {
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eflags) = *ssp++;
	    _LWORD(esp) = *ssp++;
	    _ss = *ssp++;
	    in_dpmi_dos_int = (int) *ssp++;
	  }
/* Posibilities:
 * 1. If in_dpmi_dos_int==0, it is hardware interrupt: software pm ints are
 *    handled not here. pic_iret() must be called in this case.
 * 2. If in_dpmi_dos_int==1 and interrupt is from hardware, pic_iret()
 *    will success here because realmode cs:ip were not modified in pm
 *    handler and still points to hlt. We can just skip this hlt or do_vm86()
 *    will do it for us, but pic_iret will be called with current cs:ip anyway.
 *    So calling pic_iret() here and skipping hlt in this case is also an
 *    optimization.
 * 3. If in_dpmi_dos_int==1 and int is software (0x1c, 0x23 or 0x24), then
 *    cs:ip is not on hlt now and pic_iret() will just return.
 *
 *
 * -- Stas Sergeev
 */
	  if (in_dpmi_dos_int) {
	     if (*SEG_ADR((unsigned char *), cs, ip) == 0xf4) {
		if (debug_level('M') > 3) D_printf("DPMI: returned to RM from hardware "
		    "interrupt at %p, skip hlt at %04x:%04lx\n",
		    lina, REG(cs), REG(eip));
	     }
	     else {
		if (debug_level('M') > 3) D_printf("DPMI: returned to RM from software "
		    "interrupt at %p, we are at %04x:%04lx\n",
		    lina, REG(cs), REG(eip));
	     }
	  } else {
	    if (debug_level('M') > 3) D_printf("DPMI: returned to PM from hardware "
		"interrupt at %p\n", lina);
	  }
	  pic_iret();

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_exception)) {
          D_printf("DPMI: Return from client exception handler\n");
	  if (in_dpmi_pm_stack) {
	    in_dpmi_pm_stack--;
	    if (!in_dpmi_pm_stack && _ss != PMSTACK_SEL) {
	      error("DPMI: Client's PM Stack corrupted during exception handling!\n");
//	      leavedos(91);
	    }
	  }

	  if (DPMIclient_is_32) {
	    /* poping error code */
	    ((unsigned long *) ssp)++;
	    _eip = *(((unsigned long *) ssp)++);
	    _cs = *ssp++;
	    ssp++;
	    _eflags = *(((unsigned long *) ssp)++);
	    _esp = *(((unsigned long *) ssp)++);
	    _ss = *ssp++;
	    ssp++;
	  } else {
	    /* poping error code */
	    ssp++;
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eflags) = *ssp++;
	    _LWORD(esp) = *ssp++;
	    _ss = *ssp++;
	  }
	  if (_eflags & IF)
	    dpmi_sti();

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_rm_callback)) {
	  
	  struct RealModeCallStructure *rmreg;

	  D_printf("DPMI: Return from client realmode callback procedure\n");

	  rmreg = (struct RealModeCallStructure *)(GetSegmentBaseAddress(_es)
		                + (DPMIclient_is_32 ? _edi : _LWORD(edi)));

	  if (in_dpmi_pm_stack) {
	    in_dpmi_pm_stack--;
	    if (!in_dpmi_pm_stack && _ss != PMSTACK_SEL) {
	      error("DPMI: Client's PM Stack corrupted during the realmode callback!\n");
//	      leavedos(91);
	    }
	  }

	  REG(edi) = rmreg->edi;
	  REG(esi) = rmreg->esi;
	  REG(ebp) = rmreg->ebp;
	  REG(ebx) = rmreg->ebx;
	  REG(edx) = rmreg->edx;
	  REG(ecx) = rmreg->ecx;
	  REG(eax) = rmreg->eax;
	  REG(eflags) = (long) rmreg->flags;
	  REG(es) = rmreg->es;
	  REG(ds) = rmreg->ds;
	  REG(fs) = rmreg->fs;
	  REG(gs) = rmreg->gs;
	  REG(cs) = rmreg->cs;
	  REG(eip) = (long) rmreg->ip;
	  REG(ss) = rmreg->ss;
	  REG(esp) = (long) rmreg->sp;
	  
	  restore_pm_regs(scp);
	  in_dpmi_dos_int = 1;
	  dpmi_sti();

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_mouse_callback)) {

	  D_printf("DPMI: Return from mouse callback\n");

	  if (in_dpmi_pm_stack) {
	    in_dpmi_pm_stack--;
	    if (!in_dpmi_pm_stack && _ss != PMSTACK_SEL) {
	      error("DPMI: Client's PM Stack corrupted during mouse callback!\n");
//	      leavedos(91);
	    }
	  }

	  restore_pm_regs(scp);
	  in_dpmi_dos_int = 1;
	  dpmi_sti();
	} else if ((_eip>=DPMI_OFF+1+HLT_OFF(DPMI_exception)) && (_eip<=DPMI_OFF+32+HLT_OFF(DPMI_exception))) {
	  int excp = _eip-1-DPMI_OFF-HLT_OFF(DPMI_exception);
	  D_printf("DPMI: default exception handler 0x%02x called\n",excp);
	  do_default_cpu_exception(scp, excp);

	} else if ((_eip>=DPMI_OFF+1+HLT_OFF(DPMI_interrupt)) && (_eip<=DPMI_OFF+256+HLT_OFF(DPMI_interrupt))) {
	  int intr = _eip-1-DPMI_OFF-HLT_OFF(DPMI_interrupt);
	  D_printf("DPMI: default protected mode interrupthandler 0x%02x called\n",intr);
	  if (DPMIclient_is_32) {
	    _eip = *(((unsigned long *) ssp)++);
	    _cs = *ssp++;
	    ssp++;
	    _eflags = *((unsigned long *) ssp);
	    _esp += 12;
	  } else {
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eflags) = *ssp;
	    _esp += 6;
	  }
	  do_dpmi_int(scp, intr);

	} else
	  return;
      } else			/* in client\'s code, set back eip */
	_eip -= 1;
      break;
    case 0xfa:			/* cli */
      _eip += 1;
      /*
       * are we trapped in a deadly loop?
       */
      if ((csp[0] == 0xeb) && (csp[1] == 0xfe)) {
	dbug_printf("OUCH! deadly loop, cannot continue");
	leavedos(97);
      }
      if (find_cli_in_blacklist(lina)) {
        D_printf("DPMI: Ignoring blacklisted cli\n");
	break;
      }
      current_cli = lina;
      dpmi_cli();
      if (!is_cli)
	is_cli = 1;
      break;
    case 0xfb:			/* sti */
      _eip += 1;
      dpmi_sti();
      break;

    case 0x6c:                    /* [rep] insb */
      /* NOTE: insb uses ES, and ES can't be overwritten by prefix */
      if (DPMIclient_is_32)
	_edi += port_rep_inb(_LWORD(edx), (Bit8u *)SEL_ADR(_es,_edi),
	        _LWORD(eflags)&DF, (is_rep?_LWECX:1));
      else
	_LWORD(edi) += port_rep_inb(_LWORD(edx), (Bit8u *)SEL_ADR(_es,_LWORD(edi)),
        	_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      if (is_rep) _LWECX = 0;
      LWORD32(eip)++;
      break;

    case 0x6d:			/* [rep] insw/d */
      /* NOTE: insw/d uses ES, and ES can't be overwritten by prefix */
      if (prefix66) {
	if (DPMIclient_is_32)
	  _edi += port_rep_inw(_LWORD(edx), (Bit16u *)SEL_ADR(_es,_edi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	else
	  _LWORD(edi) += port_rep_ind(_LWORD(edx), (Bit32u *)SEL_ADR(_es,_LWORD(edi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      else {
	if (DPMIclient_is_32)
	  _edi += port_rep_ind(_LWORD(edx), (Bit32u *)SEL_ADR(_es,_edi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	else
	  _LWORD(edi) += port_rep_inw(_LWORD(edx), (Bit16u *)SEL_ADR(_es,_LWORD(edi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      if (is_rep) _LWECX = 0;
      LWORD32(eip)++;
      break;

    case 0x6e:			/* [rep] outsb */
      if (pref_seg < 0) pref_seg = _ds;
      if (DPMIclient_is_32)
	_esi += port_rep_outb(_LWORD(edx), (Bit8u *)SEL_ADR(pref_seg,_esi),
	        _LWORD(eflags)&DF, (is_rep?_LWECX:1));
      else
	_LWORD(esi) += port_rep_outb(_LWORD(edx), (Bit8u *)SEL_ADR(pref_seg,_LWORD(esi)),
	        _LWORD(eflags)&DF, (is_rep?_LWECX:1));
      if (is_rep) _LWECX = 0;
      LWORD32(eip)++;
      break;

    case 0x6f:			/* [rep] outsw/d */
      if (pref_seg < 0) pref_seg = _ds;
      if (prefix66) {
        if (DPMIclient_is_32)
	  _esi += port_rep_outw(_LWORD(edx), (Bit16u *)SEL_ADR(pref_seg,_esi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
        else
	  _LWORD(esi) += port_rep_outd(_LWORD(edx), (Bit32u *)SEL_ADR(pref_seg,_LWORD(esi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      else {
        if (DPMIclient_is_32)
	  _esi += port_rep_outd(_LWORD(edx), (Bit32u *)SEL_ADR(pref_seg,_esi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
        else
	  _LWORD(esi) += port_rep_outw(_LWORD(edx), (Bit16u *)SEL_ADR(pref_seg,_LWORD(esi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      } 
      if (is_rep) _LWECX = 0;
      LWORD32(eip)++;
      break;

    case 0xe5:			/* inw xx, ind xx */
      if (prefix66 ^ DPMIclient_is_32) _eax = ind((int) csp[0]);
      else _LWORD(eax) = inw((int) csp[0]);
      LWORD32(eip) += 2;
      break;
    case 0xe4:			/* inb xx */
      _LWORD(eax) &= ~0xff;
      _LWORD(eax) |= inb((int) csp[0]);
      LWORD32(eip) += 2;
      break;
    case 0xed:			/* inw dx */
      if (prefix66 ^ DPMIclient_is_32) _eax = ind(_LWORD(edx));
      else _LWORD(eax) = inw(_LWORD(edx));
      LWORD32(eip)++;
      break;
    case 0xec:			/* inb dx */
      _LWORD(eax) &= ~0xff;
      _LWORD(eax) |= inb(_LWORD(edx));
      LWORD32(eip) += 1;
      break;
    case 0xe7:			/* outw xx */
      if (prefix66 ^ DPMIclient_is_32) outd((int)csp[0], _eax);
      else outw((int)csp[0], _LWORD(eax));
      LWORD32(eip) += 2;
      break;
    case 0xe6:			/* outb xx */
      outb((int) csp[0], _LO(ax));
      LWORD32(eip) += 2;
      break;
    case 0xef:			/* outw dx */
      if (prefix66 ^ DPMIclient_is_32) outd(_LWORD(edx), _eax);
      else outw(_LWORD(edx), _LWORD(eax));
      LWORD32(eip) += 1;
      break;
    case 0xee:			/* outb dx */
      outb(_LWORD(edx), _LO(ax));
      LWORD32(eip) += 1;
      break;

    case 0x0f:
      if (cpu_trap_0f(csp-1, scp)) break;
      /* fall thru */

    default:
      _eip = org_eip;
      if (msdos_fault(scp))
	  return;
#ifdef __linux__
#ifdef X86_EMULATOR
      /* the other side of the trick */
      _trapno = 13;
#endif
      do_cpu_exception(scp);
#endif

    } /* switch */
  } /* _trapno==13 */
  else
#ifdef __linux__
      do_cpu_exception(scp);
#endif

  if (pic_irr & ~(pic_isr | pic_imr)) dpmi_eflags |= VIP;
  if (in_dpmi_dos_int || (dpmi_eflags & VIP)) {
    dpmi_eflags &= ~VIP;
    Return_to_dosemu_code(scp,0);
  }
}


void dpmi_realmode_hlt(unsigned char * lina)
{
#ifdef TRACE_DPMI
  if ((debug_level('t')==0)||((int)lina!=0xfc80a))
#endif
  D_printf("DPMI: realmode hlt: %p\n", lina);
  if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_dpmi_init))) {
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    LWORD(eip) += 1;	/* skip halt to point to FAR RET */
    CARRY;

    dpmi_init();

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dos))) {

#ifdef TRACE_DPMI
    if ((debug_level('t')==0)||((int)lina!=0xfc80a))
#endif
    D_printf("DPMI: Return from DOS Interrupt without register translation\n");
    restore_rm_regs();
    in_dpmi_dos_int = 0;
/* we are here at return from:
 * - realmode hardware interrupt handler
 * - realmode mouse callback (fake_pm_int)
 * - default realmode CPU exception handler
 * - other client's termination point
 * As internal mouse driver now uses PIC, all the above cases are apropriate
 * to call pic_iret(). Exception handler does not require pic_iret, but being
 * called without an active interrupt handler, pic_iret() does nothing, and
 * CPU exceptions should not happen inside the interrupt handler under normal
 * conditions.
 * Calling pic_iret() on other client termination is good idea, I even revector
 * int 21h/4ch for this.
 *
 * -- Stas Sergeev
 */
    pic_iret();

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dos_exec))) {

    D_printf("DPMI: Return from DOS exec\n");
    msdos_post_exec();

    restore_rm_regs();
    in_dpmi_dos_int = 0;

  } else if ((lina>=(unsigned char *)(DPMI_ADD + HLT_OFF(DPMI_return_from_dosint))) &&
	     (lina <(unsigned char *)(DPMI_ADD + HLT_OFF(DPMI_return_from_dosint)+256)) ) {
    int intr = (int)(lina) - DPMI_ADD-HLT_OFF(DPMI_return_from_dosint);

    D_printf("DPMI: Return from DOS Interrupt 0x%02x\n",intr);

    /* DANG_FIXTHIS we should not change registers for hardware interrupts */
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0dd5 & REG(eflags)) |
      dpmi_mhp_TF;
    dpmi_stack_frame[current_client].eax = REG(eax);
    dpmi_stack_frame[current_client].ebx = REG(ebx);
    dpmi_stack_frame[current_client].ecx = REG(ecx);
    dpmi_stack_frame[current_client].edx = REG(edx);
    dpmi_stack_frame[current_client].esi = REG(esi);
    dpmi_stack_frame[current_client].edi = REG(edi);
    dpmi_stack_frame[current_client].ebp = REG(ebp);

    msdos_post_extender(intr);

    restore_rm_regs();
    in_dpmi_dos_int = 0;

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_realmode))) {
    struct RealModeCallStructure *rmreg = (struct RealModeCallStructure *) RealModeContext;
#ifdef X86_EMULATOR
    int tmp;
#endif
    restore_rm_context();

    D_printf("DPMI: Return from Real Mode Procedure\n");
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
#endif
#ifdef X86_EMULATOR
    tmp = E_MUNPROT_STACK(rmreg);
#endif
    rmreg->edi = REG(edi);
    rmreg->esi = REG(esi);
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
#ifdef X86_EMULATOR
    if (tmp) E_MPROT_STACK(rmreg);
#endif

    restore_rm_regs();
    in_dpmi_dos_int = 0;

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dos_memory))) {
    unsigned long length, base;
    unsigned short begin_selector, num_descs;
    int i;
    
    D_printf("DPMI: Return from DOS memory service, CARRY=%d,ES=0x%04x\n",
	     LWORD(eflags) & CF, REG(es));

    in_dpmi_dos_int = 0;

    begin_selector = LO_WORD(dpmi_stack_frame[current_client].edx);

    dpmi_stack_frame[current_client].eflags &= ~CF;
    if(LWORD(eflags) & CF) {	/* error */
	switch (LO_WORD(dpmi_stack_frame[current_client].eax)) {
	  case 0x100:
	    ResizeDescriptorBlock(&dpmi_stack_frame[current_client],
		begin_selector, 0);
	    break;

	  case 0x102:
	    if (!ResizeDescriptorBlock(&dpmi_stack_frame[current_client],
		begin_selector, LO_WORD(dpmi_stack_frame[current_client].ebx) << 4))
		error("Unable to resize descriptor block\n");
	}
	if (LO_WORD(dpmi_stack_frame[current_client].eax) != 0x101)
	    dpmi_stack_frame[current_client].ebx = REG(ebx);/* max para aval */
	dpmi_stack_frame[current_client].eax = REG(eax);/* get error code */
	dpmi_stack_frame[current_client].eflags |= CF;
	goto done;
    }

    base = 0;
    length = 0;
    switch (LO_WORD(dpmi_stack_frame[current_client].eax)) {
      case 0x0100:	/* allocate block */
	dpmi_stack_frame[current_client].eax = LWORD(eax);
	base = LWORD(eax) << 4;
	length = GetSegmentLimit(begin_selector) + 1;
	dpmi_stack_frame[current_client].ebx = length >> 4;
	num_descs = (length ? (DPMIclient_is_32 ? 1 : (length/0x10000 +
					((length%0x10000) ? 1 : 0))) : 0);
	for (i = 0; i < num_descs; i++) {
	    if (SetSegmentBaseAddress(begin_selector + (i<<3), base+i*0x10000))
		error("Set segment base failed\n");
	}
	break;

      case 0x0101:	/* free block */
	break;
      
      case 0x0102:	/* resize block */
        if (ValidAndUsedSelector(begin_selector)) {
	  length = GetSegmentLimit(begin_selector) + 1;
	}
	dpmi_stack_frame[current_client].ebx = length >> 4;
	break;
    }

done:
    restore_rm_regs();
  } else if ((lina>=(unsigned char *)(DPMI_ADD + HLT_OFF(DPMI_realmode_callback))) &&
	     (lina <(unsigned char *)(DPMI_ADD + HLT_OFF(DPMI_realmode_callback)+0x10)) ) {
#ifdef X86_EMULATOR
    int tmp;
#endif
    int num;
    unsigned short *ssp;
    unsigned short CLIENT_PMSTACK_SEL;
    struct RealModeCallStructure *rmreg;

    num = (int)(lina) - DPMI_ADD-HLT_OFF(DPMI_realmode_callback);
    rmreg = realModeCallBack[current_client][num].rmreg;

    D_printf("DPMI: Real Mode Callback for #%i address\n", num);

#ifdef X86_EMULATOR
    tmp = E_MUNPROT_STACK(rmreg);
#endif
    rmreg->edi = REG(edi);
    rmreg->esi = REG(esi);
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
    rmreg->cs = REG(cs);
    rmreg->ip = LWORD(eip);
    rmreg->ss = REG(ss);
    rmreg->sp = LWORD(esp);
#ifdef X86_EMULATOR
    if (tmp) E_MPROT_STACK(rmreg);
#endif
    save_pm_regs(&dpmi_stack_frame[current_client]);

    /* the realmode callback procedure will return by an iret */
    /* WARNING - realmode flags can contain the dreadful NT flag which
     * will produce an exception 10 as soon as we return from the
     * callback! */
    dpmi_stack_frame[current_client].eflags =  REG(eflags)&(~(AC|VM|IF|TF|NT));

    if (!in_dpmi_pm_stack) {
      D_printf("DPMI: Switching to locked stack\n");
      CLIENT_PMSTACK_SEL = PMSTACK_SEL;
      if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
        error("DPMI: rm_callback: App is working on host\'s PM locked stack, expect troubles!\n");
    }
    else {
      D_printf("DPMI: Not switching to locked stack, in_dpmi_pm_stack=%d\n",
        in_dpmi_pm_stack);
      CLIENT_PMSTACK_SEL = dpmi_stack_frame[current_client].ss;
    }

    if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL || in_dpmi_pm_stack)
      PMSTACK_ESP = client_esp(0);
    else
      PMSTACK_ESP = DPMI_pm_stack_size;

    if (PMSTACK_ESP < 100) {
      error("PM stack overflowed: in_dpmi_pm_stack=%i\n", in_dpmi_pm_stack);
      leavedos(25);
    }

    ssp = (us *) (GetSegmentBaseAddress(CLIENT_PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));
/* ---------------------------------------------------
	| 000FC927 | <- ssp here
	| dpmi_sel |
	|  eflags  |
   --------------------------------------------------- */
    if (DPMIclient_is_32) {
	*(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].eflags;
	*--ssp = (us) 0;
	*--ssp = DPMI_SEL; 
	*(--((unsigned long *) ssp)) = DPMI_OFF + HLT_OFF(DPMI_return_from_rm_callback);
	PMSTACK_ESP -= 12;
    } else {
	*--ssp = (unsigned short) dpmi_stack_frame[current_client].eflags;
	*--ssp = DPMI_SEL; 
	*--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_rm_callback);
	PMSTACK_ESP -= 6;
    }
    dpmi_stack_frame[current_client].cs =
	realModeCallBack[current_client][num].selector;
    dpmi_stack_frame[current_client].eip =
	realModeCallBack[current_client][num].offset;
    dpmi_stack_frame[current_client].ss = CLIENT_PMSTACK_SEL;
    dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
    in_dpmi_pm_stack++;
    SetSelector(realModeCallBack[current_client][num].rm_ss_selector,
		(REG(ss)<<4), 0xffff, DPMIclient_is_32,
		MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0);
    dpmi_stack_frame[current_client].ds =
	realModeCallBack[current_client][num].rm_ss_selector;
    dpmi_stack_frame[current_client].esi = REG(esp);
    dpmi_stack_frame[current_client].es =
	realModeCallBack[current_client][num].rmreg_selector;
    dpmi_stack_frame[current_client].edi=
	realModeCallBack[current_client][num].rmreg_offset;

    dpmi_cli();
    in_dpmi_dos_int = 0;

  } else if (lina ==(unsigned char *)(DPMI_ADD +
				      HLT_OFF(DPMI_mouse_callback))) {
    unsigned short *ssp;
    unsigned short CLIENT_PMSTACK_SEL;

    REG(eip) += 1;            /* skip halt to point to FAR RET */
    D_printf("DPMI: starting mouse callback\n");
    save_pm_regs(&dpmi_stack_frame[current_client]);
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0dd5 & REG(eflags)) |
      dpmi_mhp_TF;
    dpmi_stack_frame[current_client].eax = REG(eax);
    dpmi_stack_frame[current_client].ebx = REG(ebx);
    dpmi_stack_frame[current_client].ecx = REG(ecx);
    dpmi_stack_frame[current_client].edx = REG(edx);
    dpmi_stack_frame[current_client].esi = REG(esi);
    dpmi_stack_frame[current_client].edi = REG(edi);
    dpmi_stack_frame[current_client].ds = ConvertSegmentToDescriptor(REG(ds));
    dpmi_stack_frame[current_client].cs = mouseCallBack.selector;
    dpmi_stack_frame[current_client].eip = mouseCallBack.offset;

    if (!in_dpmi_pm_stack) {
      D_printf("DPMI: Switching to locked stack\n");
      CLIENT_PMSTACK_SEL = PMSTACK_SEL;
      if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
        error("DPMI: run_pm_mouse: App is working on host\'s PM locked stack, expect troubles!\n");
    }
    else {
      D_printf("DPMI: Not switching to locked stack, in_dpmi_pm_stack=%d\n",
        in_dpmi_pm_stack);
      CLIENT_PMSTACK_SEL = dpmi_stack_frame[current_client].ss;
    }

    if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL || in_dpmi_pm_stack)
      PMSTACK_ESP = client_esp(0);
    else
      PMSTACK_ESP = DPMI_pm_stack_size;

    if (PMSTACK_ESP < 100) {
      error("PM stack overflowed: in_dpmi_pm_stack=%i\n", in_dpmi_pm_stack);
      leavedos(25);
    }

    ssp = (us *) (GetSegmentBaseAddress(CLIENT_PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));

    if (DPMIclient_is_32) {
	*--ssp = (us) 0;
	*--ssp = DPMI_SEL; 
	*(--((unsigned long *) ssp)) =
	     DPMI_OFF + HLT_OFF(DPMI_return_from_mouse_callback);
	PMSTACK_ESP -= 8;
    } else {
	*--ssp = DPMI_SEL; 
	*--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_mouse_callback);
	PMSTACK_ESP -= 4;
    }
    dpmi_stack_frame[current_client].ss = CLIENT_PMSTACK_SEL;
    dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
    in_dpmi_pm_stack++;
    dpmi_cli();
    in_dpmi_dos_int = 0;
  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_raw_mode_switch))) {
    D_printf("DPMI: switching from real to protected mode\n");
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
#endif
    in_dpmi_dos_int = 0;
    if (DPMIclient_is_32) {
      dpmi_stack_frame[current_client].eip = REG(edi);
      dpmi_stack_frame[current_client].esp = REG(ebx);
    } else {
      dpmi_stack_frame[current_client].eip = LWORD(edi);
      dpmi_stack_frame[current_client].esp = LWORD(ebx);
    }
    dpmi_stack_frame[current_client].cs	 = LWORD(esi);
    dpmi_stack_frame[current_client].ss	 = LWORD(edx);
    dpmi_stack_frame[current_client].ds	 = LWORD(eax);
    dpmi_stack_frame[current_client].es	 = LWORD(ecx);
    dpmi_stack_frame[current_client].fs	 = 0;
    dpmi_stack_frame[current_client].gs	 = 0;
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0cd5 & REG(eflags)) |
      dpmi_mhp_TF;
    dpmi_stack_frame[current_client].eax = 0;
    dpmi_stack_frame[current_client].ebx = 0;
    dpmi_stack_frame[current_client].ecx = 0;
    dpmi_stack_frame[current_client].edx = 0;
    dpmi_stack_frame[current_client].esi = 0;
    dpmi_stack_frame[current_client].edi = 0;
    dpmi_stack_frame[current_client].ebp = REG(ebp);

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_save_restore))) {
    unsigned long *buffer = SEG_ADR((unsigned long *),es,di);
    if (LO(ax)==0) {
      D_printf("DPMI: save protected mode registers\n");
      *buffer++ = dpmi_stack_frame[current_client].eax;
      *buffer++ = dpmi_stack_frame[current_client].ebx;
      *buffer++ = dpmi_stack_frame[current_client].ecx;
      *buffer++ = dpmi_stack_frame[current_client].edx;
      *buffer++ = dpmi_stack_frame[current_client].esi;
      *buffer++ = dpmi_stack_frame[current_client].edi;
      *buffer++ = dpmi_stack_frame[current_client].esp;
      *buffer++ = dpmi_stack_frame[current_client].ebp;
      *buffer++ = dpmi_stack_frame[current_client].eip;
      *buffer++ = dpmi_stack_frame[current_client].cs;
      *buffer++ = dpmi_stack_frame[current_client].ds;
      *buffer++ = dpmi_stack_frame[current_client].ss;
      *buffer++ = dpmi_stack_frame[current_client].es;
      *buffer++ = dpmi_stack_frame[current_client].fs;
      *buffer++ = dpmi_stack_frame[current_client].gs;  
    } else {
      D_printf("DPMI: restore protect mode registers\n");
      dpmi_stack_frame[current_client].eax = *buffer++;
      dpmi_stack_frame[current_client].ebx = *buffer++;
      dpmi_stack_frame[current_client].ecx = *buffer++;
      dpmi_stack_frame[current_client].edx = *buffer++;
      dpmi_stack_frame[current_client].esi = *buffer++;
      dpmi_stack_frame[current_client].edi = *buffer++;
      dpmi_stack_frame[current_client].esp = *buffer++;
      dpmi_stack_frame[current_client].ebp = *buffer++;
      dpmi_stack_frame[current_client].eip = *buffer++;
      dpmi_stack_frame[current_client].cs =  *buffer++;
      dpmi_stack_frame[current_client].ds =  *buffer++;
      dpmi_stack_frame[current_client].ss =  *buffer++;
      dpmi_stack_frame[current_client].es =  *buffer++;
      dpmi_stack_frame[current_client].fs =  *buffer++;
      dpmi_stack_frame[current_client].gs =  *buffer++;
    }
    REG(eip) += 1;            /* skip halt to point to FAR RET */

  } else {
    if(pic_icount)
      D_printf("DPMI: unhandled HLT: lina=%p pic_icount=%li\n",
        lina, pic_icount);
    pic_resched();
  }
}

#ifdef USE_MHPDBG   /* dosdebug support */

int dpmi_mhp_regs(void)
{
  struct sigcontext_struct *scp;
  if (!in_dpmi || in_dpmi_dos_int) return 0;
  scp=&dpmi_stack_frame[current_client];
  mhp_printf("\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx eflags: %08lx",
     _eax, _ebx, _ecx, _edx, _eflags);
  mhp_printf("\nESI: %08lx EDI: %08lx EBP: %08lx", _esi, _edi, _ebp);
  mhp_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n", _ds, _es, _fs, _gs);
  mhp_printf(" CS:EIP= %04x:%08x SS:ESP= %04x:%08x\n", _cs, _eip, _ss, _esp);
  return 1;
}

void dpmi_mhp_getcseip(unsigned int *seg, unsigned int *off)
{
  *seg = dpmi_stack_frame[current_client].cs;
  *off = dpmi_stack_frame[current_client].eip;
}

void dpmi_mhp_modify_eip(int delta)
{
  dpmi_stack_frame[current_client].eip +=delta;
}

void dpmi_mhp_getssesp(unsigned int *seg, unsigned int *off)
{
  *seg = dpmi_stack_frame[current_client].ss;
  *off = dpmi_stack_frame[current_client].esp;
}

int dpmi_mhp_get_selector_size(int sel)
{
  return (Segments[sel >> 3].is_32);
}

int dpmi_mhp_getcsdefault(void)
{
  return dpmi_mhp_get_selector_size(dpmi_stack_frame[current_client].cs);
}

void dpmi_mhp_GetDescriptor(unsigned short selector, unsigned long *lp)
{
  int typebyte=do_LAR(selector);
  if (typebyte) {
	((unsigned char *)(&ldt_buffer[selector & 0xfff8]))[5]=typebyte;
#ifdef X86_EMULATOR
	if (config.cpuemu>1)
	  emu_mhp_SetTypebyte (selector, typebyte);
#endif
  }
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
}

int dpmi_mhp_getselbase(unsigned short selector)
{
  unsigned long d[2];
  dpmi_mhp_GetDescriptor(selector, d);
  return (d[0] >> 16) | ((d[1] & 0xff) <<16) | (d[1] & 0xff000000);
}

enum {
   _SSr, _CSr, _DSr, _ESr, _FSr, _GSr,
   _AXr, _BXr, _CXr, _DXr, _SIr, _DIr, _BPr, _SPr, _IPr, _FLr,
  _EAXr,_EBXr,_ECXr,_EDXr,_ESIr,_EDIr,_EBPr,_ESPr,_EIPr
};

unsigned long dpmi_mhp_getreg(int regnum)
{
  struct sigcontext_struct *scp;
  if (!in_dpmi || in_dpmi_dos_int) return 0;
  scp=&dpmi_stack_frame[current_client];
  switch (regnum) {
    case _SSr: return _ss;
    case _CSr: return _cs;
    case _DSr: return _ds;
    case _ESr: return _es;
    case _FSr: return _fs;
    case _GSr: return _gs;
    case _AXr: return _eax;
    case _BXr: return _ebx;
    case _CXr: return _ecx;
    case _DXr: return _edx;
    case _SIr: return _esi;
    case _DIr: return _edi;
    case _BPr: return _ebp;
    case _SPr: return _esp;
    case _IPr: return _eip;
    case _FLr: return _eflags;
    case _EAXr: return _eax;
    case _EBXr: return _ebx;
    case _ECXr: return _ecx;
    case _EDXr: return _edx;
    case _ESIr: return _esi;
    case _EDIr: return _edi;
    case _EBPr: return _ebp;
    case _ESPr: return _esp;
    case _EIPr: return _eip;
  }
  return -1;
}

void dpmi_mhp_setreg(int regnum, unsigned long val)
{
  struct sigcontext_struct *scp;
  if (!in_dpmi || in_dpmi_dos_int) return;
  scp=&dpmi_stack_frame[current_client];
  switch (regnum) {
    case _SSr: _ss = val; break;
    case _CSr: _cs = val; break;
    case _DSr: _ds = val; break;
    case _ESr: _es = val; break;
    case _FSr: _fs = val; break;
    case _GSr: _gs = val; break;
    case _AXr: _eax = val; break;
    case _BXr: _ebx = val; break;
    case _CXr: _ecx = val; break;
    case _DXr: _edx = val; break;
    case _SIr: _esi = val; break;
    case _DIr: _edi = val; break;
    case _BPr: _ebp = val; break;
    case _SPr: _esp = val; break;
    case _IPr: _eip = val; break;
    case _FLr: _eflags = (_eflags & ~0x0cd5) | (val & 0x0cd5); break;
    case _EAXr: _eax = val; break;
    case _EBXr: _ebx = val; break;
    case _ECXr: _ecx = val; break;
    case _EDXr: _edx = val; break;
    case _ESIr: _esi = val; break;
    case _EDIr: _edi = val; break;
    case _EBPr: _ebp = val; break;
    case _ESPr: _esp = val; break;
    case _EIPr: _eip = val; break;
  }
}

static int dpmi_mhp_intxx_pending(struct sigcontext_struct *scp, int intno)
{
  if (dpmi_mhp_intxxtab[intno] & 1) {
    if (dpmi_mhp_intxxtab[intno] & 0x80) {
      if (mhp_getaxlist_value( (_eax & 0xFFFF) | (intno<<16), -1) <0) return -1;
    }
    return 1;
  }
  return 0;
}

static int dpmi_mhp_intxx_check(struct sigcontext_struct *scp, int intno)
{
  switch (dpmi_mhp_intxx_pending(scp,intno)) {
    case -1: return 0;
    case 1:
      dpmi_mhp_intxxtab[intno] &=~1;
      return 0x100+intno;
    default:
      if (!(dpmi_mhp_intxxtab[intno] & 2)) dpmi_mhp_intxxtab[intno] |=3;
      return 0;
  }
}

int dpmi_mhp_setTF(int on)
{
  struct sigcontext_struct *scp;
  if (!in_dpmi) return 0;
  scp=&dpmi_stack_frame[current_client];
  if (on) _eflags |=TF;
  else _eflags &=~TF;
  dpmi_mhp_TF = _eflags & TF;
  return 1;
}


#endif /* dosdebug support */

void add_cli_to_blacklist(void)
{
  if (*current_cli != 0xfa) {
    error("DPMI: add_cli_to_blacklist() called with no cli at %p (0x%x)!\n",
      current_cli, *current_cli);
    return;
  }
  if (cli_blacklisted < CLI_BLACKLIST_LEN) {
    if (debug_level('M') > 5)
      D_printf("DPMI: adding cli to blacklist: lina=%p\n", current_cli);
    cli_blacklist[cli_blacklisted++] = current_cli;
  }
  else
    D_printf("DPMI: Warning: cli blacklist is full!\n");
}

static int find_cli_in_blacklist(unsigned char * cur_cli)
{
int i;
  if (debug_level('M') > 8)
    D_printf("DPMI: searching blacklist (%d elements) for cli (lina=%p)\n",
      cli_blacklisted, cur_cli);
  for (i=0; i<cli_blacklisted; i++) {
    if (cli_blacklist[i] == cur_cli)
      return 1;
  }
  return 0;
}

#undef DPMI_C
