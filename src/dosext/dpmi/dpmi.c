/* 			DPMI support for DOSEMU
 *
 * DANG_BEGIN_MODULE dpmi.c
 *
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * DANG_END_MODULE
 *
 * First Attempted by James B. MacLean macleajb@ednet.ns.ca
 *
 * $Date: 1995/05/06 16:25:48 $
 * $Source: /usr/src/dosemu0.60/dpmi/RCS/dpmi.c,v $
 * $Revision: 1.2 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
 *
 */

#define DPMI_C

#ifdef __linux__                  
#define DIRECT_DPMI_CONTEXT_SWITCH
#endif

#include "config.h"
#include "kversion.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "vm86plus.h"

#ifdef __linux__
#include <linux/unistd.h>
#include <linux/head.h>
#if KERNEL_VERSION < 2001000
  #include <linux/ldt.h>
#else
  #include <asm/ldt.h>
#endif
#include <asm/segment.h>
#include <asm/page.h>
#endif

#if 0
  /* well, I would like to get TASK_SIZE  out of the header files,
   * however, including them also defines stuff, that breaks compilation
   */
  #include <linux/sched.h>
#else
  #define TASK_SIZE     (0xC0000000UL)
#endif


#ifdef __NetBSD__
#include <signal.h>
#include <machine/segments.h>
#include <machine/sysarch.h>
#include <sys/param.h>
#define PAGE_SIZE NBPG
#define LDT_ENTRY_SIZE        8               /* 8 bytes each */
#define MODIFY_LDT_CONTENTS_DATA      0
#define MODIFY_LDT_CONTENTS_STACK     1
#define MODIFY_LDT_CONTENTS_CODE      2
#define LDT_ENTRIES                   8192            /* XXX ?? */
#endif
#include <string.h>
#include <errno.h>
#include "emu.h"
#include "memory.h"
#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif
#include "dosio.h"

#ifdef __NetBSD__
#define sigcontext_struct sigcontext  /* XXX easier this way */
#endif

#if 0
#define SHOWREGS
#endif

#if 0
#undef inline /*for gdb*/
#define inline
#endif

#include "dpmi.h"
#include "bios.h"
#include "config.h"
#include "bitops.h"
#include "pic.h"
#include "meminfo.h"
#include "int.h"
#include "serial.h"
#include "port.h"

#ifdef __NetBSD__
#define vm86_regs sigcontext
#endif
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

static INTDESC Interrupt_Table[0x100];
static INTDESC Exception_Table[0x20];
SEGDESC Segments[MAX_SELECTORS];

/* for real mode call back, DPMI function 0x303 0x304 */
static RealModeCallBack realModeCallBack[DPMI_MAX_CLIENTS][0x10];

/* For protected mode mouse call back support */
typedef struct {
    unsigned short ax;
    unsigned short cx;
    unsigned short dx;
    unsigned short si;
    unsigned short di;
    unsigned short bx;
} MouseEvent;
    
#define mouseCallBackQueueSize 6       /* hope it is enough */
static RealModeCallBack mouseCallBack; /* user\'s mouse routine */
static MouseEvent mouseCallBackQueue[mouseCallBackQueueSize];
static MouseEvent LastMouse;
static unsigned short mouseCallBackHead = 0;
static unsigned short mouseCallBackTail = 0;
static short mouseCallBackCount;
static short in_mouse_callback;

struct vm86_regs DPMI_rm_stack[DPMI_max_rec_rm_func];
int DPMI_rm_procedure_running = 0;

#define DPMI_max_rec_pm_func 16
struct sigcontext_struct DPMI_pm_stack[DPMI_max_rec_pm_func];
int DPMI_pm_procedure_running = 0;

char *ldt_buffer=0;
unsigned short LDT_ALIAS = 0;
unsigned short KSPACE_LDT_ALIAS = 0;

static char *pm_stack; /* locked protected mode stack */

static unsigned long dpmi_total_memory; /* total memory  of this session */
extern unsigned long dpmi_free_memory; /* how many bytes memory client */
				       /* can allocate */
extern dpmi_pm_block *pm_block_root[DPMI_MAX_CLIENTS];
extern unsigned long pm_block_handle_used;       /* tracking handle */

#ifndef lint
static char RCSdpmi[] = "$Header: /usr/src/dosemu0.60/dpmi/RCS/dpmi.c,v 1.2 1995/05/06 16:25:48 root Exp root $";
#endif

static int DPMIclient_is_32 = 0;
static unsigned short DPMI_private_data_segment;
unsigned short PMSTACK_SEL = 0;	/* protected mode stack selector */
unsigned long PMSTACK_ESP = 0;	/* protected mode stack descriptor */
unsigned short DPMI_SEL = 0;
extern int fatalerr;

static struct sigcontext_struct dpmi_stack_frame[DPMI_MAX_CLIENTS]; /* used to store the dpmi client registers */
static struct sigcontext_struct _emu_stack_frame;  /* used to store emulator registers */
static struct sigcontext_struct *emu_stack_frame=&_emu_stack_frame;

#ifdef __NetBSD__
#undef vm86_regs
#endif

#include "msdos.h"

#ifdef __linux__
_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)
#ifdef REQUIRES_VM86PLUS
  #define LDT_WRITE 0x11
#else
  #define LDT_WRITE 1
#endif
#endif

#ifdef __NetBSD__
/* from WINE: */
struct segment_descriptor *
make_sd(unsigned base,
	unsigned limit,
	int contents,
	int read_exec_only,
	int seg32,
	int inpgs,
	int seg_not_present,
	int usable
	)
{
        static long d[2];

        d[0] = ((base & 0x0000ffff) << 16) |
                (limit & 0x0ffff);
        d[1] = (base & 0xff000000) |
               ((base & 0x00ff0000)>>16) |
               (limit & 0xf0000) |
               (contents << 10) |
               ((read_exec_only ^ 1) << 9) |
               (seg32 << 22) |
               (inpgs << 23) |
               ((seg_not_present ^1) << 15) |
               (usable << 20) |
               0x7000;
        
        return ((struct segment_descriptor *)d);
}
#endif

inline int get_ldt(void *buffer)
{
#ifdef __linux__
  return modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
#endif
#ifdef __NetBSD__
  D_printf("fetching ldt @ %p\n", buffer);
    return i386_get_ldt(0, (union descriptor *)buffer, LDT_ENTRIES);
#endif
}

__inline__ int set_ldt_entry(int entry, unsigned long base, unsigned int limit,
	      int seg_32bit_flag, int contents, int read_only_flag,
	      int limit_in_pages_flag
#ifdef WANT_WINDOWS
, int seg_not_present, int useable
#endif
)
{
  unsigned long *lp;
#ifdef __linux__
  struct modify_ldt_ldt_s ldt_info;
  unsigned long first, last, bottom, top;
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
#if defined(REQUIRES_VM86PLUS)
  ldt_info.useable = useable;
#endif
#else
  ldt_info.seg_not_present = 0;
#endif

  last = ldt_info.limit;
  bottom = first = ldt_info.base_addr;
  if (ldt_info.limit_in_pages) {
  	last = last * PAGE_SIZE + PAGE_SIZE - 1;
  }
  if (contents == 1) {
	first += last +1;
	bottom = first;
	last = ldt_info.base_addr+65535;
	if (ldt_info.seg_32bit)
		last = first-1;
  }
  else last += first;
  top=last;
    
#define OUR_STACK 16*4096  /* for config.secure */

  if (contents == 1) {
    if (ldt_info.seg_32bit) {
      if (last < first) {
        /* Well, we have last < first, the other cases we can't handle anyway,
         * so we let modify_ldt() accept or reject.
         * We ajust the limit, so the kernel accepts it, but we leave the
         * LTD_ALIAS untouched.
         * If the base is below TASK_SIZE, we force an expand-up.
         * However, this isn't a complete fake, if the DPMI client accesses
         * outside the _real_ limit, we get a segfault.
         * However, most clients won't do, as we know from the days as
         * expand-downs weren't handled by the kernel. (Hans 961220)
         */
	top = TASK_SIZE-1;
        if (ldt_info.base_addr >= TASK_SIZE) {
		bottom =0;
		last = 1 - ldt_info.base_addr;
	}
	else {
	  /* we force an expand-up */
	  ldt_info.contents = 0;
	  if (config.secure) {
	  	last = TASK_SIZE - OUR_STACK - base;
	  	top = TASK_SIZE - OUR_STACK -1;
	  }
	  else last = TASK_SIZE - base;
	  bottom = base;
	}
        ldt_info.limit_in_pages = 1;
        last = (last - (PAGE_SIZE - 1)) / PAGE_SIZE;
        ldt_info.limit = last;
      }
    }
  }
  else if ((last < first || last >= TASK_SIZE) && ldt_info.seg_not_present == 0) {
    if (first >= TASK_SIZE) {
      ldt_info.seg_not_present = 
#ifdef WANT_WINDOWS
      seg_not_present = 
#endif
      1;
      D_printf("DPMI: WARNING: set segment[0x%04x] to NOT PRESENT\n",entry);
    } else {
      if (ldt_info.limit_in_pages)
        limit = ldt_info.limit = ((TASK_SIZE - first)>>12) - 1;
      else
        limit = ldt_info.limit = TASK_SIZE - first - 1;
      D_printf("DPMI: WARNING: reducing limit of segment[0x%04x]\n",entry);
    }
  }

  if (config.secure) {
	extern void _start();
	extern char _end;
	if ( (top > (TASK_SIZE - OUR_STACK)) /* we protect our stack */
	  || ((bottom >= ((unsigned long)&_start)) && ( bottom < (unsigned long)&_end))
	  || ((bottom < ((unsigned long)&_start)) && ( top >= (unsigned long)&_start)) ) {
		errno = EINVAL;
		return -1;
	}
  }

  if ((__retval=modify_ldt(LDT_WRITE, &ldt_info, sizeof(ldt_info))))
	return __retval;
#endif
#ifdef __NetBSD__
#ifndef WANT_WINDOWS
  int seg_not_present = 0;
  int useable = 0;
#endif
  unsigned long first, last;
  int __retval;
    struct segment_descriptor *sd;

    
  ifprintf(d.dpmi, "DPMI: entry=%x base=%x limit=%x%s %s-bit contents=%d %s"
#ifdef WANT_WINDOWS
	   "%s%s"
#endif
	   "\n",
	   entry, base, limit, limit_in_pages_flag?"-pages":"",
	   seg_32bit_flag?"32":"16",
	   contents, read_only_flag?"read-only":""
#ifdef WANT_WINDOWS
	   ,seg_not_present ? " not present" :"",
	   useable ? " usable" : ""
#endif
      );

  last = limit;
  first = base;
  if (limit_in_pages_flag) {
  	last *= PAGE_SIZE;
  	last += PAGE_SIZE-1;
  }

  last += first;
  if ((last < first || last >= TASK_SIZE) && seg_not_present == 0) {
    if (first >= TASK_SIZE) {
      seg_not_present = 1;
      D_printf("DPMI: WARNING: set segment[0x%04x] to NOT PRESENT\n",entry);
    } else {
      if (limit_in_pages_flag)
        limit = ((TASK_SIZE - first)>>12) - 1;
      else
        limit = TASK_SIZE - first - 1;
      D_printf("DPMI: WARNING: reducing limit of segment[0x%04x]\n",entry);
    }
  }

  sd = make_sd(base, limit, contents, read_only_flag, seg_32bit_flag, limit_in_pages_flag, seg_not_present, useable);
  /* easier to read in big-endian order */
  D_printf("DPMI: setldt %x %x\n",
	   ((unsigned long *)sd)[1],
	   ((unsigned long *)sd)[0]);
  if (dbg_fd) {
      fflush(dbg_fd);
      fsync(fileno(dbg_fd));
  }
  if ((__retval = i386_set_ldt(entry, (union descriptor *)sd, 1)) == -1)
      return errno;
  D_printf("DPMI: setldt succeeded\n");
#endif


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
#if defined(REQUIRES_VM86PLUS) && defined(WANT_WINDOWS)
            (ldt_info.useable << 20) |
#endif
            0x7000;
#endif
#ifdef __NetBSD__
  *lp = ((u_long *)sd)[0];
  *(lp+1) = ((u_long *)sd)[1];
#endif
  return 0;
}

static void print_ldt(void ) /* stolen from WINE */
{
  static char buffer[0x10000];
  unsigned long *lp, *lp2;
  unsigned long base_addr, limit;
  int type, i;

  if (get_ldt(buffer) < 0)
    exit(1);

  lp = (unsigned long *) buffer;

  for (i = 0; i < MAX_SELECTORS; i++, lp++) {
    /* First 32 bits of descriptor */
    base_addr = (*lp >> 16) & 0x0000FFFF;
    limit = *lp & 0x0000FFFF;
    lp++;

    /* First 32 bits of descriptor */
    base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    limit |= (*lp & 0x000F0000);
    type = (*lp >> 10) & 7;
    if ((base_addr > 0) || (limit > 0 ) || Segments[i].used) {
      if (*lp & 1000)  {
	D_printf("Entry 0x%04x: Base %08lx, Limit %05lx, Type %d\n",
	       i, base_addr, limit, type);
	D_printf("              ");
	if (*lp & 0x100)
	  D_printf("Accessed, ");
	if (!(*lp & 0x200))
	  D_printf("Read/Excecute only, ");
	if (*lp & 8000)
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
	D_printf("              %08lx %08lx\n", *(lp), *(lp-1));
      }
      else {
	D_printf("Entry 0x%04x: Base %08lx, Limit %05lx, Type %d\n",
	       i, base_addr, limit, type);
	D_printf("              SYSTEM: %08lx %08lx\n", *lp, *(lp - 1));
      }
      lp2 = (unsigned long *) &ldt_buffer[i*LDT_ENTRY_SIZE];
      D_printf("       cache: %08lx %08lx\n", (unsigned long)*(lp2+1), (unsigned long)*(lp2)); 
      D_printf("         seg: Base %08lx, Limit %05x, Type %d, Big %d\n",
	     Segments[i].base_addr, Segments[i].limit, Segments[i].type, Segments[i].is_big); 
    }
  }
}

/* client_esp return the proper value of client\'s esp, if scp != 0, */
/* get esp from scp, otherwise get esp from dpmi_stack_frame         */
unsigned long inline client_esp(struct sigcontext_struct *scp)
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
static int direct_dpmi_switch(struct sigcontext_struct *dpmi_context)
{
  register int ret;
  __asm__ volatile ("
      leal   -12(%%esp),%%esp "/* dummy, cr2,oldmask,fpstate */"
      push   %%ss
      pushl  %%esp   "/* dummy, esp_at_signal */"
      pushfl
      push   %%cs
      pushl  $dpmi_switch_return
      xorl   %0,%0   "/* preset return code with 0 */"
      push   %%eax   "/* dummy, err */"
      push   %%eax   "/* dummy, trapno */"
      pushal
      addl   $10*4,12(%%esp)   "/* adjust esp */"
      push   %%ds
      push   %%es
      push   %%fs
      push   %%gs
      movl   %%esp,"CISH_INLINE(emu_stack_frame)"

      "/* now we load the new context
          we move it on our stack, and then pop it */"

      movl   (14*4)(%1),%%eax   "/* p->eip */"
      movl   %%eax,__neweip
      movw   (15*4)(%1),%%ax   "/* p->cs */"
      movw   %%eax,__newcs
      pushl  18*4 (%1)        "/* p->ss */"
      pushl  7*4 (%1)         "/* p->esp */"    
      pushl  16*4 (%1)
      movl   $12,%%ecx
      subl   $12*4,%%esp      "/* make room on the stack */"
      cld
      movl   %%esp,%%edi
      movl   %1,%%esi
      rep; movsl
      pop    %%gs
      pop    %%fs
      pop    %%es
      pop    %%ds
      popal
      popfl
      lss    (%%esp),%%esp  "/* this is: pop ss; pop esp */"
      jmp    __dpmi_switch_jmp   

      "/* NOTE: we are putting the ljmp into .data (needing this kludge),
                because we can't write to code space (Hans) */"
      .data
  __dpmi_switch_jmp:
      .byte  0xEA    "/* ljmp */"
  __neweip:
      .long  0x12345678
  __newcs:
      .short 0xabcd
      .text
  dpmi_switch_return:
  ":"=a" (ret): "d" (dpmi_context)
  );
  return ret;
}
#endif


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
 *
 * DANG_END_REMARK
 */

  register int ret;
#ifdef DIRECT_DPMI_CONTEXT_SWITCH
  struct sigcontext_struct *scp=&dpmi_stack_frame[current_client];
  if (!(_eflags & TF)) return direct_dpmi_switch(scp);
  else {
    /* Note: we can't set TF with our speedup code */
#ifdef USE_MHPDBG
    if (mhpdbg.active) {
      static force_early=0;
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


static unsigned short AllocateDescriptors(int number_of_descriptors)
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
  for (i=0;i<number_of_descriptors;i++) {
    if (in_dpmi)
      Segments[next_ldt+i].used = in_dpmi;
    else
      Segments[next_ldt+i].used = 1;
  }
  D_printf("DPMI: Allocate descriptors started at 0x%04x\n", (next_ldt<<3) | 0x0007);
  /* dpmi spec says, the descriptor allocated should be "data" with */
  /* base and limit set to 0 */
  for (i = 0 ; i< number_of_descriptors; i++)
      if (SetSelector(((next_ldt+i)<<3) | 0x0007, 0, 0, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  return (next_ldt<<3) | 0x0007;
}

static int FreeDescriptor(unsigned short selector)
{
  unsigned short ldt_entry = selector >> 3;

  if (ldt_entry >= MAX_SELECTORS)
    return -1;

  Segments[ldt_entry].used = 0;
  Segments[ldt_entry].base_addr = 0;
  Segments[ldt_entry].limit = 0;
  Segments[ldt_entry].type = 0;
  Segments[ldt_entry].is_32 = 0;
  Segments[ldt_entry].readonly = 1;
  Segments[ldt_entry].is_big = 0;
  Segments[ldt_entry].not_present = 1;
  Segments[ldt_entry].useable = 0;
  return 0;
}

static inline int ConvertSegmentToDescriptor(unsigned short segment)
{
  unsigned long baseaddr = segment << 4;
  unsigned short selector;
  int i;
  for (i=1;i<MAX_SELECTORS;i++)
    if ((Segments[i].base_addr==baseaddr) && (Segments[i].limit==0xffff) &&
	(Segments[i].type==MODIFY_LDT_CONTENTS_DATA) && Segments[i].used)
      return (i<<3) | 0x0007;
  if (!(selector = AllocateDescriptors(1))) return 0;
  if (SetSelector(selector, baseaddr, 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  return selector;
}

static inline unsigned short GetNextSelectorIncrementValue(void)
{
  return 8;
}

static inline unsigned long GetSegmentBaseAddress(unsigned short selector)
{
  if (selector >= (MAX_SELECTORS << 3))
    return 0;
  return Segments[selector >> 3].base_addr;
}

static int SetSegmentBaseAddress(unsigned short selector, unsigned long baseaddr)
{
  unsigned short ldt_entry = selector >> 3;
  D_printf("DPMI: SetSegmentBaseAddress[0x%04x;0x%04x] 0x%08lx\n", ldt_entry, selector, baseaddr);
  if (ldt_entry >= MAX_SELECTORS)
    return -1;
  if (!Segments[ldt_entry].used)
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

static int SetSegmentLimit(unsigned short selector, unsigned int limit)
{
  unsigned short ldt_entry = selector >> 3;
  D_printf("DPMI: SetSegmentLimit[0x%04x;0x%04x] 0x%08x\n", ldt_entry, selector, limit);
  if (ldt_entry >= MAX_SELECTORS)
    return -1;
  if (!Segments[ldt_entry].used)
    return -1;
  if (limit > 0x0fffff) {
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

  if (ldt_entry >= MAX_SELECTORS)
    return -1;
  if (!Segments[ldt_entry].used)
    return -1; /* invalid value 8021 */
  if (!(type_byte & 0x10)) /* we refused system selector */
    return -1; /* invalid value 8021 */
  /* Only allow conforming Codesegments if Segment is not present */
  if ( ((type_byte>>2)&3) == 3 && ((type_byte >> 7) & 1) == 1)
    return -2; /* invalid selector 8022 */

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
  __asm__ ("
    movzwl  %%ax,%%eax
    larw %%ax,%%ax
    jz   1f
    xorl %%eax,%%eax
   1:
    shrl $8,%%eax
  ":"=a" (selector)
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
   * however, some do not. Some of those which do _not_, atleast use the
   * DPMI-GetDescriptor function, so this may solve the problem.
   *                      (Hans Lermen, July 1996)
   * DANG_END_REMARK
   */
  int typebyte=do_LAR(selector);
  if (typebyte) ((unsigned char *)(&ldt_buffer[selector & 0xfff8]))[5]=typebyte;
#endif  
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
  D_printf("DPMI: GetDescriptor[0x%04x;0x%04x]: 0x%08lx%08lx\n", selector>>3, selector, *(lp+1), *lp);
}

static int SetDescriptor(unsigned short selector, unsigned long *lp)
{
  unsigned long base_addr, limit;
  D_printf("DPMI: SetDescriptor[0x%04x;0x%04x] 0x%08lx%08lx\n", selector>>3, selector, *(lp+1), *lp);
  if (selector >= (MAX_SELECTORS << 3))
    return -1;
  if (!Segments[selector >> 3].used)
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

#ifdef SHOWREGS
  D_printf("DPMI: free=%d,\n", mi->free);
  D_printf("      swapfree=%d,\n", mi->swapfree);
  D_printf("      swaptotal=%d,\n", mi->swaptotal);
  D_printf("      total=%d,\n",mi->total);
  D_printf("      mi->free + mi->swapfree=%d\n",(unsigned int) (mi->free + mi->swapfree));
  D_printf("      mi->free + mi->swapfree/DPMI=%d\n",(unsigned int) (mi->free + mi->swapfree)/DPMI_page_size);
  D_printf("      (mi->total + mi->swaptotal)/DPMI_page_size=%d\n",
  (unsigned int) (mi->total + mi->swaptotal)/DPMI_page_size);
  D_printf("      (mi->free)/DPMI_page_size=%d\n", (unsigned int) (mi->free)/DPMI_page_size);
  D_printf("      (mi->total)/DPMI_page_size=%d\n", (unsigned int) (mi->total)/DPMI_page_size);
  D_printf("      (mi->swaptotal)/DPMI_page_size=%d\n", (unsigned int) (mi->swaptotal)/DPMI_page_size);
#endif

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

static inline void save_rm_context()
{
  if (RealModeContext_Running >= DPMI_max_rec_rm_func) {
    error("DPMI: RealModeContext_Running = 0x%4lx\n",RealModeContext_Running++);
    return;
  }
  RealModeContext_Stack[RealModeContext_Running++] = RealModeContext;
}

static inline void restore_rm_context()
{
  if (RealModeContext_Running-- > DPMI_max_rec_rm_func)
    return;
  RealModeContext = RealModeContext_Stack[RealModeContext_Running];
}


static inline void save_rm_regs()
{
  if (DPMI_rm_procedure_running >= DPMI_max_rec_rm_func) {
    error("DPMI: DPMI_rm_procedure_running = 0x%4x\n",DPMI_rm_procedure_running++);
    return;
  }
  DPMI_rm_stack[DPMI_rm_procedure_running++] = REGS;
  REG(ss) = DPMI_private_data_segment;
  REG(esp) = DPMI_rm_stack_size * DPMI_rm_procedure_running;
}

static inline void restore_rm_regs()
{
  if (DPMI_rm_procedure_running-- > DPMI_max_rec_rm_func)
    return;
  REGS = DPMI_rm_stack[DPMI_rm_procedure_running];
}

static inline void save_pm_regs()
{
  if (DPMI_pm_procedure_running >= DPMI_max_rec_pm_func) {
    error("DPMI: DPMI_pm_procedure_running = 0x%04x\n",DPMI_pm_procedure_running++);
    return;
  }
  DPMI_pm_stack[DPMI_pm_procedure_running++] = dpmi_stack_frame [current_client];
}

static inline void restore_pm_regs()
{
  if (DPMI_pm_procedure_running-- > DPMI_max_rec_pm_func)
    return;
  dpmi_stack_frame[current_client] = DPMI_pm_stack[DPMI_pm_procedure_running];
}

void fake_pm_int(void)
{
  save_rm_regs();
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  in_dpmi_dos_int = 1;
}

/* DANG_BEGIN_REMARK
 * Handling of the virtuell interruptflag is still not correct and there
 * are many open questions since DPMI specifications are unclear in this
 * point.
 * An example: If IF=1 in protected mode and real mode code is called
 * which is disabling interrupts via cli and returning to protected
 * mode, is IF then still one or zero?
 * I guess I have to think a lot about this and to write a small dpmi
 * client running under a commercial dpmi server :-).
 * DANG_END_REMARK
 */

static __inline__ void dpmi_cli()
{
  dpmi_eflags &= ~IF;
  pic_cli();
}

static __inline__ void dpmi_sti()
{
  dpmi_eflags |= IF;
  pic_sti();
}

#define CHECK_SELECTOR(x) \
{ if ( (((x) >> 3) >= MAX_SELECTORS) || (!Segments[((x) >> 3)].used) \
      || (((x) & 4) != 4) || (((x) & 0xfffc) == (DPMI_SEL & 0xfffc)) \
      || (((x) & 0xfffc ) == (PMSTACK_SEL & 0xfffc)) \
	|| (((x) & 0xfffc ) == (LDT_ALIAS & 0xfffc))) { \
      _LWORD(eax) = 0x8022; \
      _eflags |= CF; \
      break; \
    } \
}
#define CHECK_SELECTOR_ALLOC(x) \
{ if ((((x) & 4) != 4) || (((x) & 0xfffc) == (DPMI_SEL & 0xfffc)) \
      || (((x) & 0xfffc ) == (PMSTACK_SEL & 0xfffc)) \
	|| (((x) & 0xfffc ) == (LDT_ALIAS & 0xfffc))) { \
      _LWORD(eax) = 0x8022; \
      _eflags |= CF; \
      break; \
    } \
}


void do_int31(struct sigcontext_struct *scp, int inumber)
{
  _eflags &= ~CF;
  switch (inumber) {
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
    if ( _ds == _LWORD(ebx)) _ds = 0;
    if ( _es == _LWORD(ebx)) _es = 0;
    if ( _fs == _LWORD(ebx)) _fs = 0;
    if ( _gs == _LWORD(ebx)) _gs = 0;
#endif    
    break;
  case 0x0002:
    if (!(_LWORD(eax)=ConvertSegmentToDescriptor(_LWORD(ebx)))) {
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
      if ((x=SetDescriptorAccessRights(_LWORD(ebx), _ecx & (DPMIclient_is_32 ? 0xffff : 0x00ff))) !=0) {
        if (x == -2) _LWORD(eax) = 0x8021;
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
  case 0x0101:			/* Dos resize memory */
  case 0x0102:			/* Dos free memory */
    {
	unsigned char * rm_ssp;
	unsigned long rm_sp;
	save_rm_regs();
	if(inumber == 0x0100)
	    LWORD(eax) = 0x4800;
	else if (inumber == 0x0101) {
	    REG(es) = GetSegmentBaseAddress(_LWORD(edx)) >> 4;
	    LWORD(eax) = 0x4900;
	} else if (inumber == 0x0102) {
	    REG(es) = GetSegmentBaseAddress(_LWORD(edx)) >> 4;
	    LWORD(eax) = 0x4a00;
	}
	REG(eflags) = _eflags;
	REG(ebx) = _ebx;
	rm_ssp = (unsigned char *) (REG(ss) << 4);
	rm_sp = (unsigned long) LWORD(esp);
	LWORD(esp) -= 4;
	pushw(rm_ssp, rm_sp, inumber);
	pushw(rm_ssp, rm_sp, LWORD(ebx));
	REG(cs) = DPMI_SEG;
	REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos_memory);
	in_dpmi_dos_int=1;
        D_printf("DPMI: call dos memory service AX=0x%04X, BX=0x%04x, ES=0x%04x\n",
                  LWORD(eax), LWORD(ebx), REG(es));
	return (void) do_int(0x21); /* call dos for memory service */
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
    D_printf("DPMI: Getting Excp 0x%x\n", _LO(bx));
    _LWORD(ecx) = Exception_Table[_LO(bx)].selector;
    _edx = Exception_Table[_LO(bx)].offset;
    break;
  case 0x0203:	/* Set Processor Exception Handler Vector */
    D_printf("DPMI: Setting Excp 0x%x\n", _LO(bx));
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
	    if (can_revector(_LO(bx)))
	      reset_revectored(_LO(bx),&vm86s.int_revectored);
	  else
	    set_revectored(_LO(bx),&vm86s.int_revectored);
#endif
#ifdef __NetBSD__
	    if (can_revector(_LO(bx)))
	      reset_revectored(_LO(bx), vm86s.int_byuser);
	  else
	    set_revectored(_LO(bx), vm86s.int_byuser);
#endif
        default:
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

      ssp = (us *) SEL_ADR(_ss, _esp);
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
      if (inumber==0x0300) {
	REG(cs) = ((us *) 0)[(_LO(bx) << 1) + 1];
	REG(eip) = ((us *) 0)[_LO(bx) << 1];
      } else {
	REG(cs) = rmreg->cs;
	REG(eip) = (long) rmreg->ip;
      }
      if (!(rmreg->sp==0)) {
	REG(ss) = rmreg->ss;
	REG(esp) = (long) rmreg->sp;
      }
      rm_ssp = (unsigned char *) (REG(ss) << 4);
      rm_sp = (unsigned long) LWORD(esp);
      for (i=0;i<(_LWORD(ecx)); i++)
	pushw(rm_ssp, rm_sp, *(ssp+(_LWORD(ecx)) - 1 - i));
      LWORD(esp) -= 2 * (_LWORD(ecx));
      if (inumber==0x0301)
	       LWORD(esp) -= 4;
      else {
	LWORD(esp) -= 6;
	pushw(rm_ssp, rm_sp, LWORD(eflags));
	REG(eflags) &= ~(IF|TF);
      }
      pushw(rm_ssp, rm_sp, DPMI_SEG);
      pushw(rm_ssp, rm_sp, DPMI_OFF + HLT_OFF(DPMI_return_from_realmode));
      in_dpmi_dos_int=1;
    }
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
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
       unsigned i, offset;
       offset = _LWORD(edx);
       if ((_LWORD(ecx) == DPMI_SEG) &&
	   ( offset >= (unsigned short)DPMI_realmode_callback) &&
	   ( offset < (unsigned short)DPMI_realmode_callback +	0x10)) {
	 i = offset - (unsigned short)DPMI_realmode_callback;
	 realModeCallBack[current_client][i].selector = 0;
	 realModeCallBack[current_client][i].offset = 0;
	 FreeDescriptor(realModeCallBack[current_client][i].rm_ss_selector);
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
      D_printf("      malloc returns address 0x%p\n", block->base);
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
	D_printf("      realloc returns address 0x%p\n", block -> base);
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
	  D_printf("DPMI: allocate linear mem attemp for siz 0x%08lx at 0x%08lx\n",
		   _ecx, base_address);
	  D_printf("      malloc returns address 0x%p\n", block->base);
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
  case 0x0509:			/* Map convientional memory,1.0 */
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

	D_printf("DPMI: Map convientional mem for handle %ld, offset %lx at low address %lx\n", handle, offset, low_addr);
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
  case 0x0603:	/* Relock Real Mode Reagion */
    break;
  case 0x0604:	/* Get Page Size */
    _LWORD(ebx) = 0;
    _LWORD(ecx) = DPMI_page_size;
    break;
  case 0x0700:	/* Reserved,MARK PAGES AS PAGING CANDIDATES, see intr. lst */
    break;
  case 0x0701:	/* Reserved, DISCARD PAGES, see interrupt lst */
    D_printf("DPMI: undoc. func. 0x0701 called\n");
    D_printf("      BX=0x%04x, CX=0x%04x, SI=0x%04x, DI=0x%04x\n",
			_LWORD(ebx), _LWORD(ecx), _LWORD(esi), _LWORD(edi));
    break;
  case 0x0702:	/* Mark Page as Demand Paging Candidate */
  case 0x0703:	/* Discard Page Contents */
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
    break;
  case 0x0e00:	/* Get Coprocessor Status */
    _LWORD(eax) = 0x4d;
    break;
  case 0x0e01:	/* Set Coprocessor Emulation */
    break;
  default:
    _eflags |= CF;
  } /* switch */
  if (_eflags & CF)
    D_printf("DPMI: dpmi function failed, CF=1\n");
}

#ifdef DIRECT_DPMI_CONTEXT_SWITCH

static inline void copy_context(struct sigcontext_struct *d, struct sigcontext_struct *s)
{
  memcpy(d, s, ((int)(&s->eax) - (int)(s))+ sizeof(s->eax));
  d->eip = s->eip;
  *((long *)(&d->cs)) = *((long *)(&s->cs));
  d->eflags = s->eflags;
  *((long *)(&d->ss)) = *((long *)(&s->ss));
}

static inline void Return_to_dosemu_code(struct sigcontext_struct *scp, int retcode)
{
  copy_context(&dpmi_stack_frame[current_client],scp);
  copy_context(scp, emu_stack_frame);
  _eax = retcode;
}

#else

static inline void Return_to_dosemu_code(struct sigcontext_struct *scp, int retcode)
{
  memcpy(&dpmi_stack_frame[current_client], scp, sizeof(dpmi_stack_frame[0]));
  memcpy(scp, emu_stack_frame, sizeof(*emu_stack_frame));
  _eax = retcode;
}

#endif /* DIRECT_DPMI_CONTEXT_SWITCH */

static void quit_dpmi(struct sigcontext_struct *scp, unsigned short errcode)
{
  if (in_dpmi == 1) { /* Restore enviornment, must before FreeDescriptor */
    if (CURRENT_ENV_SEL)
      *(unsigned short *)(((char *)(CURRENT_PSP<<4))+0x2c) =
             (unsigned long)(GetSegmentBaseAddress(CURRENT_ENV_SEL)) >> 4;
  }
  { 
    int i;
    for (i=0;i<MAX_SELECTORS;i++) {
      if (Segments[i].used==in_dpmi)
        FreeDescriptor(i<<3);
    }
  }

  DPMIfreeAll();
  
  /* we must free ldt_buffer here, because FreeDescriptor() will */
  /* modify ldt_buffer */
  if (in_dpmi==1) {
    if (ldt_buffer) free(ldt_buffer);
    if (pm_stack) free(pm_stack);
  }
  in_dpmi_dos_int = 1;
  in_dpmi--;
  in_win31 = 0;
#ifdef DIRECT_DPMI_CONTEXT_SWITCH
  copy_context(scp, &dpmi_stack_frame[current_client]);
#else
  memcpy(scp, &dpmi_stack_frame[current_client], sizeof(dpmi_stack_frame[0]));
#endif
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  HI(ax) = 0x4c;
  LO(ax) = errcode;
  return (void) do_int(0x21);
}

static void do_dpmi_int(struct sigcontext_struct *scp, int i)
{

  if ((i == 0x2f) && (_LWORD(eax) == 0x168a))
    return do_int31(scp, 0x0a00);
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
    D_printf("DPMI: int31, ax=%04x ,ebx=%08lx ,ecx=%08lx ,edx=%08lx\n",
	     _LWORD(eax),_ebx,_ecx,_edx);
    D_printf("             edi=%08lx ,esi=%08lx\n",_edi,_esi);
    return do_int31(scp, _LWORD(eax));
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
    REG(ds) = (long) GetSegmentBaseAddress(_ds) >> 4;
    REG(es) = (long) GetSegmentBaseAddress(_es) >> 4;
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
  us *ssp;

  dpmi_cli();	/* put it here, D_printf might got interrupted */

  D_printf("DPMI: run_pm_int(0x%02x) called, in_dpmi_dos_int=0x%02x\n",i,in_dpmi_dos_int);

  if (Interrupt_Table[i].selector == DPMI_SEL) {
    if (!in_dpmi_dos_int) {
      save_rm_regs();
      REG(cs) = DPMI_SEG;
      REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
      in_dpmi_dos_int = 1;
    }
    run_int(i);
    return;
  }

  if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
    PMSTACK_ESP = client_esp(0);
  ssp = (us *) (GetSegmentBaseAddress(PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));
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
  dpmi_stack_frame[current_client].ss = PMSTACK_SEL;
  dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
  in_dpmi_dos_int = 0;
}

void run_dpmi(void)
{
   int retval;

  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */

  if (int_queue_running || in_dpmi_dos_int) {
   /* a little optimization - if we already know that next insn is a hlt
    * there's no need to lose time calling vm86() again - AV
    */
   if (*((unsigned char *)SEG_ADR((unsigned char *), cs, ip))==0xf4)
     retval=VM86_UNKNOWN;
   else {
#if 1 			/* <ESC> BUG FIXER (if 1) */
    #define OVERLOAD_THRESHOULD2  600000 /* maximum acceptable value */
    #define OVERLOAD_THRESHOULD1  238608 /* good average value */
    #define OVERLOAD_THRESHOULD0  100000 /* minum acceptable value */
    if ((pic_icount && (dpmi_eflags & VIP)) ||
          ((pic_icount || (pic_dos_time<pic_sys_time))
        && ((pic_sys_time - pic_dos_time) < OVERLOAD_THRESHOULD1)))
       REG(eflags) |= (VIP);
    else REG(eflags) &= ~(VIP);
#else
    if (pic_icount)
        REG(eflags) |= (VIP);
#endif

    in_vm86 = 1;
    retval=DO_VM86(&vm86s);
    in_vm86=0;

    if (REG(eflags)&IF) {
      if (!(dpmi_eflags&IF))
        dpmi_sti();
    } else {
      if (dpmi_eflags&IF)
        dpmi_cli();
    }
  } /* not an HLT insn */

    switch VM86_TYPE(retval) {
	case VM86_UNKNOWN:
		vm86_GP_fault();
		break;
	case VM86_STI:
#ifdef SHOWREGS
		I_printf("DPMI: Return from vm86() for timeout\n");
#endif
		pic_iret();
		break;
	case VM86_INTx:
#ifdef SHOWREGS
		D_printf("DPMI: Return from vm86() for interrupt\n");
#endif
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
#endif
		switch (VM86_ARG(retval)) {
		  case 0x1c:	/* ROM BIOS timer tick interrupt */
		  case 0x23:	/* DOS Ctrl+C interrupt */
		  case 0x24:	/* DOS critical error interrupt */
			run_pm_int(VM86_ARG(retval));
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
	if(!mhp_debug(DBG_TRAP + (VM86_ARG(retval) << 8), 0, 0))
	   do_int(VM86_ARG(retval));
	break;
#endif
#ifdef REQUIRES_VM86PLUS
	case VM86_PICRETURN:
#endif
	case VM86_SIGNAL:
		break;
	default:
		error("DPMI: unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
		fatalerr = 4;
    }
  }
  else {
    int retcode;
    if(pic_icount) dpmi_eflags |= VIP;
    retcode = dpmi_control();
#ifdef USE_MHPDBG
    if (retcode && mhpdbg.active) {
      if ((retcode ==1) || (retcode ==3)) mhp_debug(DBG_TRAP + (retcode << 8), 0, 0);
      else mhp_debug(DBG_INTxDPMI + (retcode << 8), 0, 0);
    }
#endif
  }
    handle_signals();

#ifdef USE_MHPDBG  
    if (mhpdbg.active) mhp_debug(DBG_POLL, 0, 0);
#endif

  if (iq.queued)
    do_queued_ioctl();

}

void dpmi_init()
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

  if(!in_dpmi) {
    struct meminfo *mi;

    mi = readMeminfo();
    if (mi)
      dpmi_free_memory = ((mi->free + mi->swapfree)<
			  (((unsigned long)config.dpmi)*1024))?
                   	  (mi->free + mi->swapfree):
                	  ((unsigned long)config.dpmi)*1024;
    else
     dpmi_free_memory = ((unsigned long)config.dpmi)*1024;

    /* make it page aligned */
    dpmi_free_memory = (dpmi_free_memory & 0xfffff000) +
	                              ((dpmi_free_memory & 0xfff)
				      ? DPMI_page_size : 0);
    dpmi_total_memory = dpmi_free_memory;
    
    DPMI_rm_procedure_running = 0;

    DPMIclient_is_32 = LWORD(eax) ? 1 : 0;
    DPMI_private_data_segment = REG(es);

    ldt_buffer = malloc(LDT_ENTRIES*LDT_ENTRY_SIZE);
    if (ldt_buffer == NULL) {
      error("DPMI: can't allocate memory for ldt_buffer\n");
      return;
    }

    pm_stack = malloc(DPMI_pm_stack_size);
    if (pm_stack == NULL) {
      error("DPMI: can't allocate memory for locked protected mode stack\n");
      free(ldt_buffer);
      return;
    }

    D_printf("Freeing descriptors\n");
    for (i=0;i<MAX_SELECTORS;i++) {
	FreeDescriptor(i<<3);
/*	D_printf("%d freed\n", i);*/
    }
    D_printf("Descriptors freed\n");
    if (dbg_fd) {
	fflush(dbg_fd);
	fsync(fileno(dbg_fd));
    }

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

    pm_block_handle_used = 1;
    DTA_over_1MB = 0;		/* from msdos.h */
    in_mouse_callback = 0;
    mouseCallBackCount = 0;
    mouseCallBackTail = mouseCallBackHead = 0;
/*
 * DANG_BEGIN_NEWIDEA
 * Simulate Local Descriptor Table for MS-Windows 3.1
 * must be read only, so if krnl386.exe/krnl286.exe
 * try to write to this table, we will bomb into sigsegv()
 * and and emulate direct ldt access
 * DANG_END_NEWIDEA
 */

    if (!(LDT_ALIAS = AllocateDescriptors(1))) return;
    if (SetSelector(LDT_ALIAS, (unsigned long) ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE-1, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 1, 0, 0, 0)) return;
    
    if (!(PMSTACK_SEL = AllocateDescriptors(1))) return;
    if (SetSelector(PMSTACK_SEL, (unsigned long) pm_stack, DPMI_pm_stack_size-1, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return;
    PMSTACK_ESP = DPMI_pm_stack_size;

    if (!(DPMI_SEL = AllocateDescriptors(1))) return;
    if (SetSelector(DPMI_SEL, (unsigned long) (DPMI_SEG << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0)) return;

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
      return;
  }

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

  psp = LWORD(ebx);
  LWORD(ebx) = popw(ssp, sp);

  my_ip = popw(ssp, sp);
  my_cs = popw(ssp, sp);

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
  (*(unsigned short *)& sp) -= 10;
  for (i = 0; i < 10; i++)
    D_printf("%02lx ", popb(ssp, sp));
  D_printf("-> ");
  for (i = 0; i < 10; i++)
    D_printf("%02lx ", popb(ssp, sp));
  D_printf("\n");

  if (!(CS = AllocateDescriptors(1))) return;
  if (SetSelector(CS, (unsigned long) (my_cs << 4), 0xffff, 0,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0)) return;

  if (!(SS = AllocateDescriptors(1))) return;
  if (SetSelector(SS, (unsigned long) (LWORD(ss) << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return;

  if (LWORD(ss) == LWORD(ds))
    DS=SS;
  else {
    if (!(DS = AllocateDescriptors(1))) return;
    if (SetSelector(DS, (unsigned long) (LWORD(ds) << 4), 0xffff, DPMIclient_is_32,
                    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return;
  }

  if (!(ES = AllocateDescriptors(1))) return;
  if (SetSelector(ES, (unsigned long) (psp << 4), 0x00ff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return;

  {/* convert environment pointer to a descriptor*/
     unsigned short envp, envpd;
     envp = *(unsigned short *)(((char *)(psp<<4))+0x2c);
     if (envp) {
	if(!(envpd = AllocateDescriptors(1))) return;
#if 0
	if (SetSelector(envpd, (unsigned long) (envp << 4), 0x03ff,
#else
	/* windows is accessing envp:0x0400 */
	if (SetSelector(envpd, (unsigned long) (envp << 4), 0x0ffff,
#endif
			DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return;
	*(unsigned short *)(((char *)(psp<<4))+0x2c) = envpd;

        CURRENT_ENV_SEL = envpd;
	D_printf("DPMI: env segement %X converted to descriptor %X\n",
		envp,envpd);
     } else
          CURRENT_ENV_SEL = 0;
     CURRENT_PSP = psp;
  }

  print_ldt();
  D_printf("LDT_ALIAS=%x DPMI_SEL=%x CS=%x DS=%x SS=%x ES=%x\n", LDT_ALIAS, DPMI_SEL, CS, DS, SS, ES);

  REG(esp) += 6;
  my_sp = LWORD(esp);
  NOCARRY;

  if(!in_dpmi) { /* Don't eat rm regs if already in dpmi */
    save_rm_regs();
  }

  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);

  in_dpmi++;
  in_win31 = 0;
  in_dpmi_dos_int = 0;
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

  in_sigsegv--;
  for (; (!fatalerr && in_dpmi) ;) {
    run_dpmi();
    serial_run();
    run_irqs();
    int_queue_run();
  }
  in_sigsegv++;
}

void dpmi_sigio(struct sigcontext_struct *scp)
{
  if (_cs != UCODESEL){
#if 0
    if (dpmi_eflags & IF) {
      D_printf("DPMI: return to dosemu code for handling signals\n");
      Return_to_dosemu_code(scp,0);
    } else dpmi_eflags |= VIP;
#else
/* DANG_FIXTHIS We shouldn't return to dosemu code if IF=0, but it helps - WHY? */
    D_printf("DPMI: return to dosemu code for handling signals\n");
    Return_to_dosemu_code(scp,0);
#endif
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
	       save_rm_regs();
	       REG(cs) = DPMI_SEG;
	       REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
	       REG(eflags) &= ~(IF|TF);
	       dpmi_cli();
	       in_dpmi_dos_int = 1;
	       return (void) do_int(trapno);
    default:
	       p_dos_str("DPMI: Unhandled Execption %02x - Terminating Client\n"
			 "It is likely that dosemu is unstable now and should be rebooted\n",
			 trapno);
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
#ifdef __NetBSD__
static void do_cpu_exception(struct sigcontext *scp, int code)
#endif
{
  us *ssp;
  unsigned char *csp2, *ssp2;
  int i;

  /* My log file grows to 2MB, I have to turn off dpmi debugging,
     so this log excptions even dpmi debug is off */
  unsigned char dd = d.dpmi;
  d.dpmi = 1;
  D_printf("DPMI: do_cpu_exception(0x%02lx) called\n",_trapno);
  DPMI_show_state;
  if ( _trapno == 0xe)
      D_printf("DPMI: page fault. in dosemu?\n");
#ifdef SHOWREGS
  print_ldt();
#endif
  d.dpmi = dd;
  
  if (Exception_Table[_trapno].selector == DPMI_SEL) {
    do_default_cpu_exception(scp, _trapno);
    return;
  }

  if (_ss == PMSTACK_SEL) PMSTACK_ESP = client_esp(scp);
  ssp = (us *) (GetSegmentBaseAddress(PMSTACK_SEL) +
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
  _ss = PMSTACK_SEL;
  _esp = PMSTACK_ESP;
  dpmi_cli();
  _eflags &= ~TF;
}

/*
 * DANG_BEGIN_FUNCTION dpmi_fault
 *
 * This is the brain of DPMI. All CPU exceptions are first
 * reflected (from the signal handlers) to this code.
 *
 * Exception from nonpriveleged instructions INT XX, STI, CLI, HLT
 * and from WINDOWS 3.1 are handled here.
 *
 * All here unhandled exceptions are reflected to do_cpu_exception()
 *
 * DANG_END_FUNCTION
 */

#ifdef __linux__
void dpmi_fault(struct sigcontext_struct *scp)
#endif
#ifdef __NetBSD__
void dpmi_fault(struct sigcontext *scp, int code)
#endif
{

#define LWORD32(x) (DPMIclient_is_32 ? (unsigned long) _##x : _LWORD(x))

  us *ssp;
  unsigned char *csp;

#ifdef SHOWREGS
  unsigned char *csp2, *ssp2;
  int i;
#if 1
  if (!(_cs==UCODESEL))
#endif
  {
    D_printf("DPMI: CPU Exception!\n");
    DPMI_show_state;
  }
#endif /* SHOWREGS */

#if 0
/* 
 * If we have a 16-Bit stack segment the high word of
 * esp is not zero es expected. I don't know if this
 * is a bug of the linux kernel or the intel chip - I
 * guess the bug is due to intel :-))))))
 */
if ((_ss & 7) == 7) {
  /* stack segment from ldt */
  if (Segments[_ss>>3].used) {
    if (Segments[_ss>>3].is_32==0)
      _HWORD(esp)=0;
  } else
    D_printf("DPMI: why in the hell the stack segment 0x%04x is marked as not used?\n",_ss);
}
#endif /* 1 */
  
#ifdef USE_MHPDBG
  if (mhpdbg.active) {
    if (_trapno == 3) {
       Return_to_dosemu_code(scp,3);
       return;
    }
    if (dpmi_mhp_TF && (_trapno == 1)) {
      _eflags &= ~TF;
      dpmi_mhp_TF=0;
      Return_to_dosemu_code(scp,1);
      return;
    }
  }
#endif
  if (_trapno == 13) {
    unsigned char *lina;
    Bit32u org_eip;
    int pref_seg;
    int done,is_rep,prefix66;
    
    csp = lina = (unsigned char *) SEL_ADR(_cs, _eip);
    ssp = (us *) SEL_ADR(_ss, _esp);

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
    prefix66=0;
    pref_seg=-1;

    do {
      switch (*(csp++)) {
         case 0x66:      /* operant prefix */  prefix66=1; break;
         case 0x2e:      /* CS */              pref_seg=_cs; break;
         case 0x3e:      /* DS */              pref_seg=_ds; break;
         case 0x26:      /* ES */              pref_seg=_es; break;
         case 0x36:      /* SS */              pref_seg=_ss; break;
         case 0x65:      /* GS */              pref_seg=_gs; break;
         case 0x64:      /* FS */              pref_seg=_fs; break;
         case 0xf3:      /* rep */             is_rep=1; break;
  #if 0
         case 0xf2:      /* repnz */
  #endif
         default: done=1;
      }
    } while (!done);
    csp--;
    org_eip = _eip;
    _eip += (csp-lina);



    switch (*csp++) {

    case 0xcd:			/* int xx */
#ifdef USE_MHPDBG
      if (mhpdbg.active) {
        if (dpmi_mhp_intxxtab[*csp]) {
          static int dpmi_mhp_intxx_check(struct sigcontext_struct *scp, int intno);
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
	  _eflags &= ~(TF);
	  dpmi_cli();
	}
	_cs = Interrupt_Table[*csp].selector;
	_eip = Interrupt_Table[*csp].offset;
	D_printf("DPMI: calling interrupthandler 0x%02x at 0x%04x:0x%08lx\n", *csp, _cs, _eip);
	if ((*csp == 0x2f)&&((_LWORD(eax)==
			      0xfb42)||(_LWORD(eax)==0xfb43)))
	    D_printf("DPMI: dpmiload function called, ax=0x%04x,bx=0x%04x\n"
		     ,_LWORD(eax), _LWORD(ebx));
	if ((*csp == 0x21) && (_HI(ax) == 0x4c))
	    D_printf("DPMI: dos exit called\n");
      }
      break;

    case 0xf4:			/* hlt */
      _eip += 1;

      if (_cs==UCODESEL) {
	/* HLT in dosemu code - must in dpmi_control() */
#ifdef DIRECT_DPMI_CONTEXT_SWITCH
        /* Note: when using DIRECT_DPMI_CONTEXT_SWITCH, we only come
         * here if we have set the trap-flags (TF)
         * ( needed for dosdebug only )
         */
	copy_context(emu_stack_frame, scp); /* backup the registers */
	copy_context(scp, &dpmi_stack_frame[current_client]); /* switch the stack */
#else
	memcpy(emu_stack_frame, scp, sizeof(*emu_stack_frame)); /* backup the registers */
	memcpy(scp, &dpmi_stack_frame[current_client], sizeof(dpmi_stack_frame[0])); /* switch the stack */
#endif
#if 0
	D_printf("DPMI: now jumping to dpmi client code\n");
#ifdef SHOWREGS
	DPMI_show_state;
#endif
#endif
	return;
      }
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
          D_printf("DPMI: Return from protected mode interrupt hander\n");
	  if (DPMIclient_is_32) {
	    PMSTACK_ESP = client_esp(scp) + 24;
	    _eip = *(((unsigned long *) ssp)++);
	    _cs = *ssp++;
	    ssp++;
	    _eflags = *(((unsigned long *) ssp)++);
	    _esp = *(((unsigned long *) ssp)++);
	    _ss = *ssp++;
	    ssp++;
	    in_dpmi_dos_int = (int) *(((unsigned long *) ssp)++);
	  } else {
	    PMSTACK_ESP = client_esp(scp) + 12;
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eflags) = *ssp++;
	    _LWORD(esp) = *ssp++;
	    _ss = *ssp++;
	    in_dpmi_dos_int = (int) *ssp++;
	  }
	  pic_iret();

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_exception)) {
          D_printf("DPMI: Return from client exception hander\n");
	  if (DPMIclient_is_32) {
	    PMSTACK_ESP = client_esp(scp) + 24;
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
	    PMSTACK_ESP = client_esp(scp) + 12;
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
	  
	  PMSTACK_ESP = client_esp(scp);

	  /* dpmi_stack_frame[current_client], will be saved in */
	  /* Returo_To_Dosemu */
	  restore_pm_regs();
	  *scp = dpmi_stack_frame[current_client];

	  in_dpmi_dos_int = 1;
	  dpmi_sti();

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_mouse_callback)) {
	  
	  D_printf("DPMI: Return from mouse callback procedure\n");
	  
	  if (mouseCallBackCount) {
	      unsigned short *ssp;
	      D_printf("DPMI: %d events in mouse queue\n", mouseCallBackCount);
	      /* we have queued mouse event, call it again */
	     _eax = mouseCallBackQueue[mouseCallBackHead].ax;
	     _ebx = mouseCallBackQueue[mouseCallBackHead].bx;
	     _ecx = mouseCallBackQueue[mouseCallBackHead].cx;
	     _edx = mouseCallBackQueue[mouseCallBackHead].dx;
	     _esi = mouseCallBackQueue[mouseCallBackHead].si;
	     _edi = mouseCallBackQueue[mouseCallBackHead].di;
	     _cs  = mouseCallBack.selector;
	     _eip = mouseCallBack.offset;
	     ssp = (unsigned short *) SEL_ADR(_ss, _esp);
	     if (DPMIclient_is_32) {
		 *--ssp = (us) 0;
		 *--ssp = DPMI_SEL; 
		 *(--((unsigned long *) ssp)) =
		      DPMI_OFF + HLT_OFF(DPMI_return_from_mouse_callback);
		 _esp -= 8;
	     } else {
		 *--ssp = DPMI_SEL; 
		 *--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_mouse_callback);
		 _esp -= 4;
	     }
	     mouseCallBackHead  =
		 (mouseCallBackHead+1)%mouseCallBackQueueSize;
	     mouseCallBackCount --;
	  } else {
	      /* pop up in_dpmi_dos_int */
	     if (DPMIclient_is_32) {
		 in_dpmi_dos_int = (int) *(((unsigned long *) ssp)++);
		 _esp += 4;
	     } else {
		 in_dpmi_dos_int = (int) *ssp++;
		 _esp += 2;
	     }
	     PMSTACK_ESP = client_esp(scp);
	     /* dpmi_stack_frame[current_client], will be saved in */
	     /* Returo_To_Dosemu                                   */
	     restore_pm_regs();
	     *scp = dpmi_stack_frame[current_client];
	     /*in_dpmi_dos_int = 1;*/
	     in_mouse_callback = 0;
	     /* dpmi_sti(); */
	  }
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
      dpmi_cli();
      break;
    case 0xfb:			/* sti */
      _eip += 1;
      dpmi_sti();
      break;
    case 0x6c:                    /* insb */

      /* NOTE: insb uses ES, and ES can't be overwritten by prefix */
      if (is_rep) {
        int delta = 1;
        if(_LWORD(eflags) &DF ) delta = -1;
        D_printf("DPMI: Doing REP F3 6C (rep insb) %lx bytes, DELTA %d\n",
                LWORD32(ecx),delta);
        if (DPMIclient_is_32) {
          while (_ecx)  {
             *((unsigned char *)SEL_ADR(_es,_edi)) = inb((int) _LWORD(edx));
             _edi += delta;
             _ecx--;
          }
        }
        else {
          while (_LWORD(ecx))  {
             *((unsigned char *)SEL_ADR(_es,_LWORD(edi))) = inb((int) _LWORD(edx));
             _LWORD(edi) += delta;
             _LWORD(ecx)--;
          }
        }
      }
      else {
        *((unsigned char *)SEL_ADR(_es,_edi)) = inb((int) _LWORD(edx));
        D_printf("DPMI: insb(0x%04x) value %02x\n",
  	       _LWORD(edx),*((unsigned char *)SEL_ADR(_es,_edi)));   
        if(LWORD(eflags) & DF) LWORD32(edi)--;
        else LWORD32(edi)++;
      }
      LWORD32(eip)++;
      break;
    case 0x6d:			/* [rep] insw/d */
      /* NOTE: insw/d uses ES, and ES can't be overwritten by prefix */
      if (is_rep) {
        #define ___LOCALAUX(typ,iotyp,incr,x) \
          int delta =incr; \
          if(_LWORD(eflags) & DF) delta = -incr; \
          D_printf("DPMI: rep ins%c %lx words, DELTA %d\n", x, \
                  LWORD32(ecx),delta); \
          while(LWORD32(ecx)) { \
            *(typ SEL_ADR(_es, _edi))=iotyp(_LWORD(edx)); \
            LWORD32(edi) += delta; \
            LWORD32(ecx)--; \
          }

        if (prefix66 ^ DPMIclient_is_32) {
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
          *(typ SEL_ADR(_es, _edi))=iotyp(_LWORD(edx)); \
          D_printf("DPMI: ins%c(0x%lx) value %lx \n", x, \
                  LWORD32(edx),(unsigned long)*(typ SEL_ADR(_es, _edi)) ); \
          if(_LWORD(eflags) & DF) LWORD32(edi) -= incr; \
          else LWORD32(edi) +=incr;
        
        if (prefix66 ^ DPMIclient_is_32) {
                                  /* insd */
          ___LOCALAUX((unsigned long *),ind,4,'d')
        }
        else {
                                  /* insw */
          ___LOCALAUX((unsigned short *),inw,2,'w')
        }
        #undef  ___LOCALAUX
      }
      LWORD32(eip)++;
      break;

    case 0x6e:			/* [rep] outsb */
      if (pref_seg < 0) pref_seg = _ds;
      if (is_rep) {
        int delta = 1;
        D_printf("DPMI: rep outsb\n");
        if(LWORD(eflags) & DF) delta = -1;
        while(LWORD32(ecx)) {
          outb(_LWORD(edx), *((unsigned char *)SEL_ADR(pref_seg,_esi)));
          LWORD32(esi) += delta;
          LWORD32(ecx)--;
        }
      }
      else {
        D_printf("DPMI: outsb port 0x%04x value %02x\n",
              _LWORD(edx),*((unsigned char *)SEL_ADR(pref_seg,_esi)));
        outb(_LWORD(edx), *((unsigned char *)SEL_ADR(pref_seg,_esi)));
        if(_LWORD(eflags) & DF) LWORD32(esi)--;
        else LWORD32(esi)++;
      }
      LWORD32(eip)++;
      break;

    case 0x6f:			/* [re] outsw/d */
      if (pref_seg < 0) pref_seg = _ds;
      if (is_rep) {
        #define ___LOCALAUX(typ,iotyp,incr,x) \
          int delta = incr; \
          D_printf("DPMI:  rep outs%c\n", x); \
          if(_LWORD(eflags) & DF) delta = -incr; \
          while(LWORD32(ecx)) { \
            iotyp(LWORD32(edx), *(typ SEL_ADR(pref_seg,_esi))); \
            LWORD32(esi) += delta; \
            LWORD32(ecx)--; \
          }
        
        if (prefix66 ^ DPMIclient_is_32) {
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
          D_printf("DPMI: outs%c port 0x%04x value %x\n", x, \
                  _LWORD(edx), (unsigned int) *(typ SEL_ADR(pref_seg,_esi))); \
          iotyp(_LWORD(edx), *(typ SEL_ADR(pref_seg,_esi))); \
          if(_LWORD(eflags) & DF ) LWORD32(esi) -= incr; \
          else LWORD32(esi) +=incr;
        
        if (prefix66 ^ DPMIclient_is_32) {
                                  /* outsd */
          ___LOCALAUX((unsigned long *),outd,4,'d')
        }
        else {
                                  /* outsw */
          ___LOCALAUX((unsigned short *),outw,2,'w')
        }
        #undef  ___LOCALAUX
      } 
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

    default:
      _eip = org_eip;
      if (msdos_fault(scp))
	  return;
#ifdef __linux__
      do_cpu_exception(scp);
#endif
#ifdef __NetBSD__
      do_cpu_exception(scp, code);
#endif

    } /* switch */
  } /* _trapno==13 */
  else
#ifdef __linux__
      do_cpu_exception(scp);
#endif
#ifdef __NetBSD__
      do_cpu_exception(scp, code);
#endif

  if (in_dpmi_dos_int || int_queue_running || ((dpmi_eflags&(VIP|IF))==(VIP|IF)) ) {
    dpmi_eflags &= ~VIP;
    Return_to_dosemu_code(scp,0);
  }
}

void run_pm_mouse()
{
    unsigned char press, release;
    unsigned char insert;
    unsigned short *ssp;
    
    REG(eip) += 1;            /* skip halt to point to FAR RET */
    if(!in_dpmi) {
       D_printf("DPMI: mouse Callback while no dpmi clint running\n");
       return;
    }
    /*
     * Internal mouse driver and mouse under X does not understand pic yet,
     * so to prevent stack overflow, we queue mouse callback events  here.
     * To improve performance, motion events are compacted.
     */


    /*
     * It seems that both serial and internal mouse driver are not
     * reliable to deliver mouse event. Missing events are inserted
     * here. These code should be deleted when mouse driver becomes
     * reliabe.
     */
    press = release = 0;
    insert = (LastMouse.bx & 0x7) ^ (LO(bx) & 0x7);
    if (!(LastMouse.bx & 1) && LO(ax) & 4)
	press |= 0x2;
    if (!(LastMouse.bx & 2) && LO(ax) & 0x10)
	press |= 0x8;
    if (!(LastMouse.bx & 4) && LO(ax) & 0x40)
	press |= 0x20;
    if ((LastMouse.bx & 1) && LO(ax) & 2)
	release |= 0x4;
    if ((LastMouse.bx & 2) && LO(ax) & 0x8)
	release |= 0x10;
    if ((LastMouse.bx & 4) && LO(ax) & 0x20)
	release |= 0x40;
    if (insert && LO(ax) == 1)	{ /* buttons changes but only a motion */
				  /* event, some envent is missed      */
	if (insert & 1) {
	    if (LastMouse.bx & 1)
		release |= 0x4;
	    else
		press |= 0x2;
	}
	if (insert & 2) {
	    if (LastMouse.bx & 2)
		release |= 0x10;
	    else
		press |= 0x8;
	}
	if (insert & 4) {
	    if (LastMouse.bx & 4)
		release |= 0x40;
	    else
		press |= 0x20;
	}
    }
    if (press && mouseCallBackCount < mouseCallBackQueueSize) {
	D_printf("insert missing press event 0x%02x\n", press);
	mouseCallBackQueue[mouseCallBackTail] = LastMouse;
	mouseCallBackQueue[mouseCallBackTail].ax = press;
	mouseCallBackQueue[mouseCallBackTail].bx = LWORD(ebx);
	mouseCallBackTail = (mouseCallBackTail + 1) % mouseCallBackQueueSize;
	mouseCallBackCount++;
    }
    if ((mouseCallBackCount==0) ||
	((LO(ax) & 0x1e) && mouseCallBackCount < mouseCallBackQueueSize)) {
	D_printf("mouse event AL=0x%02x\n", LO(ax));
	mouseCallBackQueue[mouseCallBackTail].ax = LWORD(eax);
	mouseCallBackQueue[mouseCallBackTail].bx = LWORD(ebx);
	mouseCallBackQueue[mouseCallBackTail].cx = LWORD(ecx);
	mouseCallBackQueue[mouseCallBackTail].dx = LWORD(edx);
	mouseCallBackQueue[mouseCallBackTail].di = LWORD(edi);
	mouseCallBackQueue[mouseCallBackTail].si = LWORD(esi);
	mouseCallBackTail = (mouseCallBackTail + 1) % mouseCallBackQueueSize;
	mouseCallBackCount++;
    }
    if (release && mouseCallBackCount < mouseCallBackQueueSize) {
	D_printf("insert missing release event 0x%02x\n", release);
	mouseCallBackQueue[mouseCallBackTail] = LastMouse;
	mouseCallBackQueue[mouseCallBackTail].ax = release;
	mouseCallBackQueue[mouseCallBackTail].bx = LWORD(ebx);
	mouseCallBackTail = (mouseCallBackTail + 1) % mouseCallBackQueueSize;
	mouseCallBackCount++;
    }
    LastMouse.ax = LWORD(eax);
    LastMouse.bx = LWORD(ebx);
    LastMouse.cx = LWORD(ecx);
    LastMouse.dx = LWORD(edx);
    LastMouse.di = LWORD(edi);
    LastMouse.si = LWORD(esi);

    if (in_mouse_callback && mouseCallBackCount == 0)
      return;

    in_mouse_callback = 1;
    save_pm_regs();
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0dd5 & REG(eflags));
    dpmi_stack_frame[current_client].eax = mouseCallBackQueue[mouseCallBackHead].ax;
    dpmi_stack_frame[current_client].ebx = mouseCallBackQueue[mouseCallBackHead].bx;
    dpmi_stack_frame[current_client].ecx = mouseCallBackQueue[mouseCallBackHead].cx;
    dpmi_stack_frame[current_client].edx = mouseCallBackQueue[mouseCallBackHead].dx;
    dpmi_stack_frame[current_client].esi = mouseCallBackQueue[mouseCallBackHead].si;
    dpmi_stack_frame[current_client].edi = mouseCallBackQueue[mouseCallBackHead].di;
    mouseCallBackHead  = (mouseCallBackHead+1)%mouseCallBackQueueSize;
    mouseCallBackCount --;
    dpmi_stack_frame[current_client].ds = ConvertSegmentToDescriptor(REG(ds));
    dpmi_stack_frame[current_client].cs = mouseCallBack.selector;
    dpmi_stack_frame[current_client].eip = mouseCallBack.offset;

    /* mouse call back rountine should return by an lret */
    /* do we need use locked pm_stack_here? */
    if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
	PMSTACK_ESP = client_esp(0);
    ssp = (us *) (GetSegmentBaseAddress(PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));
    if (DPMIclient_is_32) {
	*(--((unsigned long *) ssp)) = in_dpmi_dos_int;
	*--ssp = (us) 0;
	*--ssp = DPMI_SEL; 
	*(--((unsigned long *) ssp)) =
	     DPMI_OFF + HLT_OFF(DPMI_return_from_mouse_callback);
	PMSTACK_ESP -= 12;
    } else {
	*--ssp = in_dpmi_dos_int;
	*--ssp = DPMI_SEL; 
	*--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_mouse_callback);
	PMSTACK_ESP -= 6;
    }
    dpmi_stack_frame[current_client].ss = PMSTACK_SEL;
    dpmi_stack_frame[current_client].esp = PMSTACK_ESP;

    /*dpmi_cli();*/
    in_dpmi_dos_int = 0;

}
void dpmi_realmode_hlt(unsigned char * lina)
{

  D_printf("DPMI: realmode hlt: %p\n", lina);
  if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_dpmi_init))) {
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    LWORD(eip) += 1;	/* skip halt to point to FAR RET */
    CARRY;

    dpmi_init();

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dos))) {

    D_printf("DPMI: Return from DOS Interrupt without register translation\n");
    restore_rm_regs();
    in_dpmi_dos_int = 0;

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
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0dd5 & REG(eflags));
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
    restore_rm_context();

    D_printf("DPMI: Return from Real Mode Procedure\n");
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
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

    restore_rm_regs();
    in_dpmi_dos_int = 0;

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dos_memory))) {
    unsigned char  *rm_ss;
    unsigned long rm_sp;
    us orig_inumber;
    us orig_bx;
    char error;
    
    rm_ss = (unsigned char *) (REG(ss) << 4);
    rm_sp = (unsigned long) LWORD(esp);
      
    orig_bx = popw(rm_ss, rm_sp);
    orig_inumber = popw(rm_ss, rm_sp);
    D_printf("DPMI: Return from dos memory service, CARRY=%d,ES=0x%04x\n",
	     LWORD(eflags) & CF, REG(es));

    in_dpmi_dos_int = 0;
    if(LWORD(eflags) & CF) {	/* error */
	dpmi_stack_frame[current_client].eax = REG(eax);/* get error code */
	if (orig_inumber != 0x101)
	    dpmi_stack_frame[current_client].ebx = REG(ebx);/* max para aval */
	dpmi_stack_frame[current_client].eflags |= CF;
	restore_rm_regs();
	return;
    }

    error = 0;
    if (orig_inumber == 0x0100)
	dpmi_stack_frame[current_client].eax = LWORD(eax);
    if (orig_inumber == 0x0100) {
	/* allocate desciptor for it */
	unsigned long length = 0x10*orig_bx;
	unsigned long base;
	unsigned short begin_selector;
	int i;
	
	base = LWORD(eax) << 4;

	begin_selector = AllocateDescriptors(length/0x10000 +
						((length%0x10000) ? 1 : 0));
	if (!begin_selector) {
	    error = 1;
	    goto done;
	}
	dpmi_stack_frame[current_client].edx = begin_selector;
	SetSegmentBaseAddress(begin_selector, base);
	if ( SetSegmentLimit(begin_selector, length)) {
	    error = 1;
	    goto done;
	}
	if (length < 0x10000)
	    goto done;
	i = 1;
	length -= 0x10000;
	while (length > 0xffff) {
	    if (SetSelector(begin_selector + (i<<3), base+i*0x10000,
			    0xffff, DPMIclient_is_32,
			    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) {
		error =1;
		goto done;
	    }
	    length -= 0x10000;
	    i++;
	}
	if (length)
	    if (SetSelector(begin_selector + (i<<3), base+i*0x10000,
			    length, DPMIclient_is_32,
			    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
		error = 1;
    }
done:
    if (error)
	dpmi_stack_frame[current_client].eflags |= CF;
    else
	dpmi_stack_frame[current_client].eflags  &= ~CF;
    restore_rm_regs();
    in_dpmi_dos_int = 0;
  } else if ((lina>=(unsigned char *)(DPMI_ADD + HLT_OFF(DPMI_realmode_callback))) &&
	     (lina <(unsigned char *)(DPMI_ADD + HLT_OFF(DPMI_realmode_callback)+0x10)) ) {
    int num;
    unsigned short *ssp;
    struct RealModeCallStructure *rmreg;

    num = (int)(lina) - DPMI_ADD-HLT_OFF(DPMI_realmode_callback);
    rmreg = realModeCallBack[current_client][num].rmreg;

    D_printf("DPMI: Real Mode Callback for #%i address\n", num);

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

    save_pm_regs();

    /* the realmode callback procedure will return by an iret */
    dpmi_stack_frame[current_client].eflags =  REG(eflags)&(~(VM|IF|TF));
    if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
	PMSTACK_ESP = client_esp(0);
    ssp = (us *) (GetSegmentBaseAddress(PMSTACK_SEL) +
		(DPMIclient_is_32 ? PMSTACK_ESP : (PMSTACK_ESP&0xffff)));
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
    dpmi_stack_frame[current_client].ss = PMSTACK_SEL;
    dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
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
      run_pm_mouse();
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
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0cd5 & REG(eflags));
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

  } else
    error("DPMI: unhandled HLT: lina=%p!\n", lina);
}

#ifdef USE_MHPDBG   /* dosdebug support */

int dpmi_mhp_regs(void)
{
  struct sigcontext_struct *scp;
  if (!in_dpmi) return 0;
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
  if (typebyte) ((unsigned char *)(&ldt_buffer[selector & 0xfff8]))[5]=typebyte;
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
  if (!in_dpmi) return 0;
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
  if (!in_dpmi) return;
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

#undef DPMI_C
