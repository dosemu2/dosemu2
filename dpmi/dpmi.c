/* 			DPMI support for DOSEMU
 *
 * DANG_BEGIN_MODULE dpmi.c
 *
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * DANG_END_MODULE
 *
 * First Attempted by James B. MacLean jmaclean@fox.nstn.ns.ca
 *
 * $Date: 1995/01/14 15:31:41 $
 * $Source: /home/src/dosemu0.60/dpmi/RCS/dpmi.c,v $
 * $Revision: 2.9 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
 * Revision 2.9  1995/01/14  15:31:41  root
 * New Year checkin.
 *
 * Revision 2.8  1994/10/14  18:02:48  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.7  1994/08/05  22:31:46  root
 * Prep dir pre53_10.
 *
 * Revision 2.6  1994/07/26  01:13:20  root
 * prep for pre53_6.
 *
 * Revision 2.5  1994/07/09  14:30:45  root
 * prep for pre53_3.
 *
 * Revision 2.4  1994/06/27  02:17:03  root
 * Prep for pre53
 *
 * Revision 2.3  1994/06/24  14:52:04  root
 * Lutz's patches against DPMI
 *
 * Revision 2.2  1994/06/17  00:14:41  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:18:03  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:18:03  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.18  1994/05/26  23:17:03  root
 * Prep. for pre51_21.
 *
 * Revision 1.17  1994/05/24  01:24:25  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.16  1994/05/21  23:40:51  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.15  1994/04/30  01:07:41  root
 * Lutz's Latest.
 *
 * Revision 1.14  1994/04/27  23:58:51  root
 * Lutz's patches.
 *
 * Revision 1.12  1994/04/16  01:33:52  root
 * Lutz's updates.
 *
 * Revision 1.11  1994/04/13  00:11:50  root
 * Lutz's patches
 *
 * Revision 1.10  1994/04/09  18:47:20  root
 * Lutz's latest.
 *
 * Revision 1.9  1994/04/07  00:19:50  root
 * Lutz's latest.
 *
 * Revision 1.8  1994/03/23  23:26:31  root
 * Diffs from Lutz
 *
 * Revision 1.7  1994/03/15  01:38:57  root
 * DPMI updates
 *
 * Revision 1.6  1994/03/10  23:53:36  root
 * Lutz patches for DPMI
 *
 * Revision 1.5  1994/03/04  00:03:08  root
 * Lutz does DPMI :-)
 *
 * Revision 1.4  1994/02/21  20:30:25  root
 * Dpmi updates.
 *
 * Revision 1.3  1994/01/25  20:10:27  root
 * Added set_ldt_entry() work.
 *
 * Revision 1.2  1994/01/20  21:18:50  root
 * Indent. More preliminaries on DPMI.
 *
 * Revision 1.1  1994/01/19  18:23:49  root
 * Initial revision
 *
 */

#define DPMI_C

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <linux/head.h>
#include <linux/ldt.h>
#include "kversion.h"
#if KERNEL_VERSION < 1001067
#include <linux/segment.h>
#else
#include <asm/segment.h>
#endif
#include <string.h>
#include <errno.h>
#include "emu.h"
#include "memory.h"
#include "dosio.h"
#include "machcompat.h"
#include "cpu.h"

#if 0
#define SHOWREGS
#endif
#include "dpmi.h"
#include "bios.h"
#include "config.h"
#include "../timer/bitops.h"
#include "../timer/pic.h"
#include "meminfo.h"
#include "int.h"

extern struct config_info config;
unsigned long RealModeContext;

static INTDESC Interrupt_Table[0x100];
static INTDESC Exception_Table[0x20];
SEGDESC Segments[MAX_SELECTORS];

struct vm86_regs DPMI_rm_stack[DPMI_rm_stack_size];
int DPMI_rm_procedure_running = 0;

static char *ldt_buffer;
static char *pm_stack; /* locked protected mode stack */

static char RCSdpmi[] = "$Header: /home/src/dosemu0.60/dpmi/RCS/dpmi.c,v 2.9 1995/01/14 15:31:41 root Exp $";

int in_dpmi = 0;		/* Set to 1 when running under DPMI */
static u_char DPMIclient_is_32 = 0;
static us DPMI_private_data_segment;
int in_dpmi_dos_int = 0;
int in_dpmi_pm_int = 0;
int in_dpmi_pm_ex = 0;
int in_dpmi_sw_to_pm_ex = 0;
us PMSTACK_SEL = 0;		/* protected mode stack selector */
unsigned long PMSTACK_ESP = 0;	/* protected mode stack descriptor */
us DPMI_SEL = 0;
us LDT_ALIAS = 0;
extern int fatalerr;

static struct sigcontext_struct dpmi_stack_frame[5]; /* used to store the dpmi client registers */
static struct sigcontext_struct emu_stack_frame;  /* used to store emulator registers */
/* used to store emulator  registers when calling client\'s exception handler    */
static struct sigcontext_struct exception_stack_frame;
static int current_client=0;

_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)

inline int get_ldt(void *buffer)
{
  return modify_ldt(0, buffer, 32 * sizeof(struct modify_ldt_ldt_s));
}

inline int set_ldt_entry(int entry, unsigned long base, unsigned int limit,
	      int seg_32bit_flag, int contents, int read_only_flag,
	      int limit_in_pages_flag)
{
  struct modify_ldt_ldt_s ldt_info;
  unsigned long *lp;
  int __retval;

  ldt_info.entry_number = entry;
  ldt_info.base_addr = base;
  ldt_info.limit = limit;
  ldt_info.seg_32bit = seg_32bit_flag;
  ldt_info.contents = contents;
  ldt_info.read_exec_only = read_only_flag;
  ldt_info.limit_in_pages = limit_in_pages_flag;
  ldt_info.seg_not_present = 0;

  if (__retval=modify_ldt(1, &ldt_info, sizeof(ldt_info)))
    return __retval;

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

  lp = (unsigned long *) &ldt_buffer[ldt_info.entry_number*LDT_ENTRY_SIZE];
  if (ldt_info.base_addr == 0 && ldt_info.limit == 0) {
    *lp = 0;
    *(lp+1) = 0;
    return 0;
  }
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
            0x7000;
  return 0;
}

static void print_ldt(void ) /* stolen from WINE */
{
	/* wow -- why static  */
  static char buffer[0x10000];
  unsigned long *lp;
  unsigned long base_addr, limit;
  int type, dpl, i;

  if (get_ldt(buffer) < 0)
    exit(1);

  lp = (unsigned long *) buffer;

  for (i = 0; i < 4096; i++, lp++) {
    /* First 32 bits of descriptor */
    base_addr = (*lp >> 16) & 0x0000FFFF;
    limit = *lp & 0x0000FFFF;
    lp++;

    /* First 32 bits of descriptor */
    base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    limit |= (*lp & 0x000F0000);
    type = (*lp >> 10) & 7;
    dpl = (*lp >> 13) & 3;
    if ((base_addr > 0) || (limit > 0 ))
      if (*lp & 1000)  {
	D_printf("Entry %2d: Base %08.8x, Limit %05.5x, DPL %d, Type %d\n",
	       i, base_addr, limit, dpl, type);
	D_printf("          ");
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
	D_printf("          %08.8x %08.8x\n", *(lp), *(lp - 1));
      }
      else {
	D_printf("Entry %2d: Base %08.8x, Limit %05.5x, DPL %d, Type %d\n",
	       i, base_addr, limit, dpl, type);
	D_printf("          SYSTEM: %08.8x %08.8x\n", *lp, *(lp - 1));
      }
  }
}

/*
 * DANG_BEGIN_FUNCTION dpmi_control
 *
 * This function is similar to the vm86() syscall in the kernel and
 * switches to dpmi code.
 *
 * DANG_END_FUNCTION
 */

static void dpmi_control(void)
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
 * DANG_END_REMARK
 */

  asm("hlt");
}

void dpmi_get_entry_point(void)
{
    D_printf("Request for DPMI entry\n");

    if (!config.dpmi_size)
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
    LWORD(esi) = DPMI_private_paragraphs;

}

static inline int SetSelector(unsigned short selector, unsigned long base_addr, unsigned long limit,
                       unsigned char is_32, unsigned char type, unsigned char readonly,
                       unsigned char is_big)
{
  int ldt_entry = selector >> 3;
  if (set_ldt_entry(ldt_entry, base_addr, limit, is_32, type, readonly, is_big))
    return -1;
  if (base_addr==0 && limit==0){
    Segments[ldt_entry].base_addr = 0;
    Segments[ldt_entry].limit = 0;
    Segments[ldt_entry].type = 0;
    Segments[ldt_entry].is_32 = 0;
    Segments[ldt_entry].readonly = 0;
    Segments[ldt_entry].is_big = 0;
    Segments[ldt_entry].used = 0;
  } else {
    Segments[ldt_entry].base_addr = base_addr;
    Segments[ldt_entry].limit = limit;
    Segments[ldt_entry].type = type;
    Segments[ldt_entry].is_32 = is_32;
    Segments[ldt_entry].readonly = readonly;
    Segments[ldt_entry].is_big = is_big;
    Segments[ldt_entry].used = 1;
  }
  return 0;
} 

static inline unsigned short AllocateDescriptors(int number_of_descriptors)
{
  int next_ldt=0, i;
  unsigned char isfree=1;
  while (isfree) {
    next_ldt++;
    isfree=0;
    for (i=0;i<number_of_descriptors;i++)
      isfree |= Segments[next_ldt+i].used;
    if (next_ldt == MAX_SELECTORS-number_of_descriptors+1) return 0;
  }
  for (i=0;i<number_of_descriptors;i++)
    Segments[next_ldt+i].used = 1;
  D_printf("DPMI: Allocate descriptors started at 0x%04x\n", (next_ldt<<3) | 0x0007);
  return (next_ldt<<3) | 0x0007;
}

static inline int FreeDescriptor(unsigned short selector)
{
  return SetSelector(selector, 0, 0, 0, 0, 0, 0);
}

static inline unsigned short ConvertSegmentToDescriptor(unsigned short segment)
{
  unsigned long baseaddr = segment << 4;
  us selector;
  int i;
  for (i=1;i<MAX_SELECTORS;i++)
    if ((Segments[i].base_addr==baseaddr) && (Segments[i].limit==0xffff) &&
	(Segments[i].type==MODIFY_LDT_CONTENTS_DATA) && Segments[i].used)
      return (i<<3) | 0x0007;
  if (!(selector = AllocateDescriptors(1))) return 0;
  if (SetSelector(selector, baseaddr, 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0)) return 0;
  return selector;
}

static inline unsigned short GetNextSelectorIncrementValue(void)
{
  return 8;
}

static inline unsigned long GetSegmentBaseAddress(unsigned short selector)
{
  return Segments[selector >> 3].base_addr;
}

inline unsigned char SetSegmentBaseAddress(unsigned short selector, unsigned long baseaddr)
{
  unsigned short ldt_entry = selector >> 3;
  if (!Segments[ldt_entry].used)
    return -1;
  Segments[ldt_entry].base_addr = baseaddr;
  return set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, Segments[ldt_entry].readonly, Segments[ldt_entry].is_big);
}

static inline unsigned char SetSegmentLimit(unsigned short selector, unsigned long limit)
{
  unsigned short ldt_entry = selector >> 3;
  if (!Segments[ldt_entry].used)
    return -1;
  if ( ((limit & 0x0fff) == 0x0fff) && DPMIclient_is_32 ) {
    Segments[ldt_entry].limit = limit>>12;
    Segments[ldt_entry].is_big = 1;
  } else {
    Segments[ldt_entry].limit = limit;
    Segments[ldt_entry].is_big = 0;
  }

  return set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, Segments[ldt_entry].readonly, Segments[ldt_entry].is_big);
}

static inline unsigned char SetDescriptorAccessRights(unsigned short selector, unsigned short type_byte)
{
  unsigned short ldt_entry = selector >> 3;
  if (!Segments[ldt_entry].used)
    return -1;
  Segments[ldt_entry].type = (type_byte >> 2) & 7;
  Segments[ldt_entry].is_32 = (type_byte >> 14) & 1;
  Segments[ldt_entry].is_big = (type_byte >> 15) & 1;
  Segments[ldt_entry].readonly = ((type_byte >> 1) & 1) ? 0 : 1;
  return set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr, Segments[ldt_entry].limit,
			Segments[ldt_entry].is_32, Segments[ldt_entry].type,
			Segments[ldt_entry].readonly, Segments[ldt_entry].is_big);
}

static inline unsigned short CreateCSAlias(unsigned short selector)
{
  us ds_selector;
  us cs_ldt= selector >> 3;

  ds_selector = AllocateDescriptors(1);
  if (SetSelector(ds_selector, Segments[cs_ldt].base_addr, Segments[cs_ldt].limit,
			Segments[cs_ldt].is_32, MODIFY_LDT_CONTENTS_DATA,
			0, Segments[cs_ldt].is_big))
    return 0;
  return ds_selector;
}

static inline void GetDescriptor(us selector, unsigned long *lp)
{
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
}

static inline unsigned char SetDescriptor(unsigned short selector, unsigned long *lp)
{
  unsigned long base_addr, limit;
  D_printf("DPMI: SetDescriptor[0x%04lx] 0x%08lx%08lx\n", selector>>3, *(lp+1), *lp);
  if (selector >= (MAX_SELECTORS << 3))
    return -1;
  base_addr = (*lp >> 16) & 0x0000FFFF;
  limit = *lp & 0x0000FFFF;
  lp++;
  base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
  limit |= (*lp & 0x000F0000);
  return SetSelector(selector, base_addr, limit, (*lp >> 22) & 1,
			(*lp >> 10) & 7, ((*lp >> 9) & 1) ? 0 : 1, (*lp >> 23) & 1);
}

/* why not int? */
static inline unsigned char AllocateSpecificDescriptor(us selector)
{
  int ldt_entry = selector >> 3;
  if (Segments[ldt_entry].used)
    return 1;
  Segments[ldt_entry].used = 1;
  return 0;
}

static inline void GetFreeMemoryInformation(unsigned int *lp)
{
  struct meminfo *mi = readMeminfo();

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

  /*00h*/	*lp = (unsigned int) (mi->free);
#if 1
  /*04h*/	*++lp = (unsigned int) (mi->free + mi->swapfree)/DPMI_page_size;
  /*08h*/	*++lp = (unsigned int) (mi->free + mi->swapfree)/DPMI_page_size;
  /*0ch*/	*++lp = (unsigned int) (mi->total + mi->swaptotal)/DPMI_page_size;
  /*10h*/	*++lp = (unsigned int) (mi->free + mi->swapfree)/DPMI_page_size;
  /*14h*/	*++lp = (unsigned int) (mi->free + mi->swapfree)/DPMI_page_size;
  /*18h*/	*++lp = (unsigned int) (mi->total)/DPMI_page_size;
  /*1ch*/	*++lp = (unsigned int) (mi->free)/DPMI_page_size;
  /*20h*/	*++lp = (unsigned int) (mi->swaptotal)/DPMI_page_size;
#else
  /*04h*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
  /*08h*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
  /*0ch*/	*++lp = (unsigned int) (mi->free / 2 )/DPMI_page_size;
  /*10h*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
  /*14h*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
  /*18h*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
  /*1ch*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
  /*20h*/	*++lp = (unsigned int) (mi->free / 2)/DPMI_page_size;
#endif
  /*24h*/	*++lp = 0xffffffff;
  /*28h*/	*++lp = 0xffffffff;
  /*2ch*/	*++lp = 0xffffffff;
}

/* #define WIN31 */

#ifdef WIN31
/* Simulate direct LDT access for MS-Windows 3.1 */
u_char win31ldt(struct sigcontext_struct *scp, unsigned char *csp)
{
  if ((_es==LDT_ALIAS) && *csp==0x26) {
    csp++;
    if ((*csp==0xc7)&&(*(csp+1)==0x04)) {   /* MOV  Word Ptr ES:[SI],#word */
      ldt_buffer[_LWORD(esi)] = *(csp+2);
      ldt_buffer[_LWORD(esi)+1] = *(csp+3);
      _eip +=5;
      SetDescriptor(_LWORD(esi), (unsigned long *)&ldt_buffer[_LWORD(esi)&0xfff8]);
      return 1;
    }
    if ((*csp==0xc7)&&(*(csp+1)==0x05)) {   /* MOV  Word Ptr ES:[DI],#word */
      ldt_buffer[_LWORD(edi)] = *(csp+2);
      ldt_buffer[_LWORD(edi)+1] = *(csp+3);
      _eip +=5;
      SetDescriptor(_LWORD(edi), (unsigned long *)&ldt_buffer[_LWORD(edi)&0xfff8]);
      return 1;
    }
    if ((*csp==0xc7)&&(*(csp+1)==0x44)) {   /* MOV  Word Ptr ES:[SI+#byte],#word */
      ldt_buffer[_LWORD(esi)+*(csp+2)] = *(csp+3);
      ldt_buffer[_LWORD(esi)+1+*(csp+2)] = *(csp+4);
      _eip +=6;
      SetDescriptor(_LWORD(esi)+*(csp+2),
				(unsigned long *)&ldt_buffer[(_LWORD(esi)+*(csp+2))&0xfff8]);
      return 1;
    }
    if ((*csp==0x89)&&(*(csp+1)==0x35)) {   /* MOV  ES:[DI],SI */
      ldt_buffer[_LWORD(esi)] = _LO(si);
      ldt_buffer[_LWORD(esi)+1] = _HI(si);
      _eip +=3;
      SetDescriptor(_LWORD(esi), (unsigned long *)&ldt_buffer[_LWORD(esi)&0xfff8]);
      return 1;
    }
  }
  return 0;
}
#endif /* WIN31 */

static inline void save_rm_regs()
{
  DPMI_rm_stack[DPMI_rm_procedure_running++] = REGS;
}

static inline void restore_rm_regs()
{
  REGS = DPMI_rm_stack[--DPMI_rm_procedure_running];
}


#include <sys/mman.h>
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
    if ((_ds==_LWORD(ebx))||(_es==_LWORD(ebx))||(_ss==_LWORD(ebx))||(_fs==_LWORD(ebx))||(_gs==_LWORD(ebx))) {
      /* selector in use - can't free */
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
    } else
    if (FreeDescriptor(_LWORD(ebx))) {
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
    }
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
  case 0x0006:
    {
      unsigned long baddress = GetSegmentBaseAddress(_LWORD(ebx));
      _LWORD(edx) = (us)(baddress & 0xffff);
      _LWORD(ecx) = (us)(baddress >> 16);
    }
    break;
  case 0x0007:
    if (SetSegmentBaseAddress(_LWORD(ebx), (_LWORD(ecx))<<16 | (_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    D_printf("DPMI: SetSegmentBaseAddress successfull\n");
    break;
  case 0x0008:
    if (SetSegmentLimit(_LWORD(ebx), (_LWORD(ecx))<<16 |(_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    D_printf("DPMI: SetSegmentLimit successfull\n");
    break;
  case 0x0009:
    if (SetDescriptorAccessRights(_LWORD(ebx), _ecx & (DPMIclient_is_32 ? 0xffff : 0x00ff)))
      _eflags |= CF;
    D_printf("DPMI: SetDescriptorAccessRights successfull\n");
    break;
  case 0x000a:
    if (!(_LWORD(eax) = CreateCSAlias(_LWORD(ebx))))
      _eflags |= CF;
    break;
  case 0x000b:
    GetDescriptor(_LWORD(ebx),
		(unsigned long *) (GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : _LWORD(edi)) ) );
    break;
  case 0x000c:
    if (SetDescriptor(_LWORD(ebx),
		(unsigned long *) (GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : _LWORD(edi)) ) ))
      _eflags |= CF;
    break;
  case 0x000d:
    if (AllocateSpecificDescriptor(_LWORD(ebx)))
      _eflags |= CF;
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
    break;
  case 0x0203:	/* Set Processor Exception Handler Vector */
    Exception_Table[_LO(bx)].selector = _LWORD(ecx);
    Exception_Table[_LO(bx)].offset = (DPMIclient_is_32 ? _edx : _LWORD(edx));
    break;
  case 0x0204:	/* Get Protected Mode Interrupt vector */
    _LWORD(ecx) = Interrupt_Table[_LO(bx)].selector;
    _edx = Interrupt_Table[_LO(bx)].offset;
    D_printf("DPMI: Get Prot. vec. bx=%x int.tab=%x sel=%x, off=%x\n", _LO(bx), Interrupt_Table[_LO(bx)].selector,
      Interrupt_Table[_LO(bx)].selector, Interrupt_Table[_LO(bx)].offset);
    break;
  case 0x0205:	/* Set Protected Mode Interrupt vector */
    Interrupt_Table[_LO(bx)].selector = _LWORD(ecx);
    Interrupt_Table[_LO(bx)].offset = (DPMIclient_is_32 ? _edx : _LWORD(edx));
    switch (_LO(bx)) {
      case 0x1c:	/* ROM BIOS timer tick interrupt */
      case 0x23:	/* DOS Ctrl+C interrupt */
      case 0x24:	/* DOS critical error interrupt */
	if ((Interrupt_Table[_LO(bx)].selector==DPMI_SEL) &&
		(Interrupt_Table[_LO(bx)].offset==DPMI_OFF + HLT_OFF(DPMI_interrupt) + _LO(bx)))
	  if (can_revector(_LO(bx)))
	    reset_revectored(_LO(bx),&vm86s.int_revectored);
	else
	  set_revectored(_LO(bx),&vm86s.int_revectored);
      default:
    }
    D_printf("DPMI: Put Prot. vec. bx=%x int.tab=%x sel=%x, off=%x\n", _LO(bx), Interrupt_Table[_LO(bx)].selector,
      Interrupt_Table[_LO(bx)].selector, Interrupt_Table[_LO(bx)].offset);
    break;
  case 0x0300:	/* Simulate Real Mode Interrupt */
  case 0x0301:	/* Call Real Mode Procedure With Far Return Frame */
  case 0x0302:	/* Call Real Mode Procedure With Iret Frame */
    save_rm_regs();
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
#if 0      
      if (REG(eflags)&IF) REG(eflags)|=VIF;
#endif      
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
      if ((rmreg->sp==0) /*&& (rmreg->ss==0)*/) {
	if (REG(ss)!=DPMI_private_data_segment) {
	  REG(ss) = DPMI_private_data_segment;
	  REG(esp) = 0x010 * DPMI_private_paragraphs;
	}
      } else {
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
#if 0
	REG(eflags) &= ~(IF|TF|VIF);
#else
	REG(eflags) &= ~(IF|TF);
#endif
      }
      pushw(rm_ssp, rm_sp, DPMI_SEG);
      pushw(rm_ssp, rm_sp, DPMI_OFF + HLT_OFF(DPMI_return_from_realmode));
      in_dpmi_dos_int=1;
    }
#ifdef SHOWREGS
    show_regs(__FILE__, __LINE__);
#endif
    D_printf("DPMI: from pm 0x%04X:0x%08x call rm proc at 0x%04x:0x%04x\n",
               _cs,_eip, REG(cs), REG(eip));
    break;
  case 0x0305:	/* Get State Save/Restore Adresses */
      _LWORD(ebx) = DPMI_SEG;
      _LWORD(ecx) = DPMI_OFF + HLT_OFF(DPMI_save_restore);
      _LWORD(esi) = DPMI_SEL;
      _edi = DPMI_OFF + HLT_OFF(DPMI_save_restore);
      _LWORD(eax) = sizeof(struct vm86_regs);
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
			/* needs some update with Larry's code? */
    break;
  case 0x0500:
    GetFreeMemoryInformation( (unsigned int *)
	(GetSegmentBaseAddress(_es) + (DPMIclient_is_32 ? _edi : _LWORD(edi))));
    break;
  case 0x0501:	/* Allocate Memory Block */
    { unsigned long *ptr;
#if 1
      ptr = malloc( (_LWORD(ebx))<<16 | (_LWORD(ecx)) );
#else
      ptr = malloc( 500000 );
#endif
    D_printf("DPMI: malloc attempt for siz 0x%08lx\n", (_LWORD(ebx))<<16 | (_LWORD(ecx)));
    D_printf("      malloc returns address 0x%08lx\n", ptr);
      if (ptr==NULL)
	_eflags |= CF;
      _LWORD(edi) = (unsigned long) ptr & 0xffff;
      _LWORD(esi) = ((unsigned long) ptr >> 16) & 0xffff;
    }
    _LWORD(ebx) = _LWORD(esi);
    _LWORD(ecx) = _LWORD(edi);
    D_printf("                     edi     0x%08lx\n", _LWORD(edi));
    D_printf("                     esi     0x%08lx\n", _LWORD(esi));
    break;
  case 0x0502:	/* Free Memory Block */
    D_printf(" DPMI: Free Mem Blk. esi     0x%08lx\n", _LWORD(esi));
    D_printf("                     edi     0x%08lx\n", _LWORD(edi));
    free((void *)( (_LWORD(esi))<<16 | (_LWORD(edi)) ));
    break;
  case 0x0503:	/* Resize Memory Block */
    { unsigned long *ptr;
      D_printf("DPMI: Realloc to size %x\n", (_LWORD(ebx)<<16 | _LWORD(ecx)));
      D_printf("DPMI: Free Mem Blk. esi     0x%08lx\n", _LWORD(esi));
      D_printf("                    edi     0x%08lx\n", _LWORD(edi));
      ptr = realloc((_LWORD(esi))<<16 | (_LWORD(edi)),
	  (_LWORD(ebx)<<16 | _LWORD(ecx)));
      D_printf("DPMI: malloc attempt for siz 0x%08lx\n", (_LWORD(ebx))<<16 | (_LWORD(ecx)));
      D_printf("      malloc returns address 0x%08lx\n", ptr);
      if (ptr==NULL) {
	_eflags |= CF;
	_LWORD(eax) = 0x8013;
        D_printf("DPMI: malloc failed\n");
      }
      _LWORD(edi) = (unsigned long) ptr & 0xffff;
      _LWORD(esi) = ((unsigned long) ptr >> 16) & 0xffff;
      D_printf("DPMI: Realloc done\n");
    }
    _LWORD(ebx) = _LWORD(esi);
    _LWORD(ecx) = _LWORD(edi);
    break;
  case 0x0600:	/* Lock Linear Region */
  case 0x0601:	/* Unlock Linear Region */
  case 0x0602:	/* Mark Real Mode Region as Pageable */
  case 0x0603:	/* Relock Real Mode Reagion */
   break;
  case 0x0604:	/* Get Page Size */
    _LWORD(ebx) = 0;
    _LWORD(ecx) = 0x1000; /* 4 KByte */
    break;
  case 0x0702:	/* Mark Page as Demand Paging Candidate */
  case 0x0703:	/* Discard Page Contents */
   break;
  case 0x0900:	/* Get and Disable Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & VIF) ? 1 : 0;
    REG(eflags) &= ~VIF;
    pic_cli();
    break;
  case 0x0901:	/* Get and Enable Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & VIF) ? 1 : 0;
    REG(eflags) |= VIF;
    pic_sti();
    break;
  case 0x0902:	/* Get Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & VIF) ? 1 : 0;
    break;
  case 0x0a00:	/* Get Vendor Specific API Entry Point */
    {
      char *ptr = (char *) (GetSegmentBaseAddress(_ds) + (DPMIclient_is_32 ? _esi : _LWORD(esi)));
      D_printf("DPMI: GetVendorAPIEntryPoint: %s\n", ptr);
#if WIN31
      if ((!strcmp("WINOS2", ptr))||(!strcmp("MS-DOS", ptr)))
#else
      if (!strcmp("WINOS2", ptr))
#endif
      {
	_LWORD(eax) = 0;
	_es = DPMI_SEL;
	_edi = DPMI_OFF + HLT_OFF(DPMI_API_extension);
      } else {
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
  }
}

inline void quit_dpmi(unsigned short errcode)
{
  if (ldt_buffer) free(ldt_buffer);
  if (pm_stack) free(pm_stack);
/* DANG_FIXTHIS don't free protected mode stack if DPMI client terminates from exception handler */
  if (!--current_client) {
    in_dpmi = 0;
  }
  in_dpmi_dos_int = 1;
#if 0
  save_rm_regs();
#else
  restore_rm_regs();
#endif
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);
  HI(ax) = 0x4c;
  LO(ax) = errcode;
  return (void) do_int(0x21);
}

inline void do_dpmi_int(struct sigcontext_struct *scp, int i)
{
  us *ssp;

  if ((i == 0x2f) && (_LWORD(eax) == 0x168a))
    return do_int31(scp, 0x0a00);
  if ((i == 0x21) && (_HI(ax) == 0x4c)) {
    D_printf("DPMI: leaving DPMI with error code 0x%02x\n",_LO(ax));
    quit_dpmi(_LO(ax));
  }
  else if ((i == 0x21) && ((_HI(ax) == 0x51) || (_HI(ax) == 0x62))) {/* get psp */
    D_printf("DPMI: client get current PSP\n");
    _LWORD(ebx) = dpmi_stack_frame[current_client].es;
    return;
  }
  else if (i == 0x31) {
    D_printf("DPMI: int31, ax=%04x\n", _LWORD(eax));
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
    REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);
    in_dpmi_dos_int = 1;
    D_printf("DPMI: calling real mode interrupt 0x%02x,eax = %X\n",i,_eax);
    return (void) do_int(i);
  }
}

/* DANG_BEGIN_FUNCTION run_pm_int
 *
 * This routine is used for running protected mode hardware
 * interrupts and software interrupts 0x1c, 0x23 and 0x24.
 * run_pm_int() switches to the locked protected mode stack
 * and calls the handler.
 *
 * This needs some speed optimize if no handler is installed
 *
 * DANG_END_FUNCTION
 */

inline void run_pm_int(int i)
{
  us *ssp;
  D_printf("DPMI: run_pm_int(0x%02x) called, in_dpmi_dos_int=0x%02x\n",i,in_dpmi_dos_int);

  if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL) PMSTACK_ESP = dpmi_stack_frame[current_client].esp;
  ssp = (us *) SEL_ADR(PMSTACK_SEL, PMSTACK_ESP);
  if (DPMIclient_is_32) {
    *--ssp = (us) 0;
    *--ssp = dpmi_stack_frame[current_client].ss;
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].esp;
    *(--((unsigned long *) ssp)) = (unsigned long) in_dpmi_dos_int;
    *--ssp = (us) 0;
    *--ssp = dpmi_stack_frame[current_client].cs; 
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].eip;
    *(--((unsigned long *) ssp)) = dpmi_stack_frame[current_client].eflags;
    *--ssp = (us) 0;
    *--ssp = DPMI_SEL; 
    *(--((unsigned long *) ssp)) = DPMI_OFF + HLT_OFF(DPMI_return_from_pm);
    PMSTACK_ESP -= 32;
  } else {
    *--ssp = dpmi_stack_frame[current_client].ss;
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].esp;
    *--ssp = (unsigned short) in_dpmi_dos_int;
    *--ssp = dpmi_stack_frame[current_client].cs; 
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].eip;
    *--ssp = (unsigned short) dpmi_stack_frame[current_client].eflags;
    *--ssp = DPMI_SEL; 
    *--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_pm);
    PMSTACK_ESP -= 16;
  }
  dpmi_stack_frame[current_client].cs = Interrupt_Table[i].selector;
  dpmi_stack_frame[current_client].eip = Interrupt_Table[i].offset;
  dpmi_stack_frame[current_client].ss = PMSTACK_SEL;
  dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
  in_dpmi_dos_int = 0;
  in_dpmi_pm_int = 1;
  dpmi_control();
}

inline void run_dpmi(void)
{
  static int retval;
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */

  if (int_queue_running || in_dpmi_dos_int) {
    in_vm86 = 1;
    D_printf("DPMI: going to vm86 mode at 0x%04x:0x%08x\n",
	     REG(cs), REG(eip));
    switch VM86_TYPE({retval=vm86(&vm86s); in_vm86=0; retval;}) {
	case VM86_UNKNOWN:
		vm86_GP_fault();
		break;
	case VM86_STI:
		I_printf("Return from vm86() for timeout\n");
		REG(eflags) &=~VIP;
		break;
	case VM86_INTx:
		D_printf("Return from vm86() for interrupt %X\n",VM86_ARG(retval));
    show_regs(__FILE__, __LINE__);
		switch (VM86_ARG(retval)) {
		  case 0x1c:	/* ROM BIOS timer tick interrupt */
		  case 0x23:	/* DOS Ctrl+C interrupt */
		  case 0x24:	/* DOS critical error interrupt */
#if 1
			run_pm_int(VM86_ARG(retval));
#else
			d_printf("DPMI: interrupt 0x%02x should reflected to protected mode\n"
				 "      - arrgh not yet inplemented!\n", VM86_ARG(retval));
			do_int(VM86_ARG(retval));
#endif
			break;
		  default:
			do_int(VM86_ARG(retval));
		}
		break;
	case VM86_SIGNAL:
		break;
	default:
		error("unknown return value from vm86()=%x,%d-%x\n", VM86_TYPE(retval), VM86_TYPE(retval), VM86_ARG(retval));
		fatalerr = 4;
    }
  }
  else {
    dpmi_control();
  }
    handle_signals();

  if (iq.queued)
    do_queued_ioctl();
}

/* DANG_BEGIN_FUNCTION run_pm_exception
 *
 * This routine is used for running protected mode hardware
 * interrupts and software exceptions
 * run_pm_exception() switches to the locked protected mode stack
 * and calls the handler.
 *
 * This needs some speed optimize if no handler is installed
 *
 * DANG_END_FUNCTION
 */

inline void run_pm_exception(int i, struct sigcontext_struct *scp)
{
  us *ssp;
  D_printf("DPMI: run_pm_exception(0x%02x) called, in_dpmi_dos_int=0x%02x\n",i,in_dpmi_dos_int);

  memcpy(&exception_stack_frame, &dpmi_stack_frame[current_client],
	 sizeof(struct sigcontext_struct));  
  memcpy(&dpmi_stack_frame[current_client], scp ,
	 sizeof(struct sigcontext_struct)); 
  if (dpmi_stack_frame[current_client].ss == PMSTACK_SEL)
      PMSTACK_ESP = dpmi_stack_frame[current_client].esp;
  ssp = (us *) SEL_ADR(PMSTACK_SEL, PMSTACK_ESP);
  if (DPMIclient_is_32) {
    *(--((int *) ssp)) = in_dpmi_dos_int;
    *--ssp = (us) 0;
    *--ssp = _ss;
    *(--((unsigned long *) ssp)) = _esp;
    *(--((unsigned long *) ssp)) = _eflags;
    *--ssp = (us) 0;
    *--ssp = _cs;
    *(--((unsigned long *) ssp)) = _eip;
    *(--((unsigned long *) ssp)) = scp -> err;
    *--ssp = (us) 0;
    *--ssp = DPMI_SEL; 
    *(--((unsigned long *) ssp)) = DPMI_OFF + HLT_OFF(DPMI_return_from_pm_eh);
    PMSTACK_ESP -= 36;
  } else {
    *(--((int *) ssp)) = in_dpmi_dos_int;
    *--ssp = _ss;
    *--ssp = _LWORD(esp);
    *--ssp = _LWORD(eflags);
    *--ssp = _cs;
    *--ssp = _LWORD(eip);
    *--ssp = (unsigned short) scp->err;
    *--ssp = DPMI_SEL; 
    *--ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_pm_eh);
    PMSTACK_ESP -= 20;
  }
  dpmi_stack_frame[current_client].cs = Exception_Table[i].selector;
  dpmi_stack_frame[current_client].eip = Exception_Table[i].offset;
  dpmi_stack_frame[current_client].ss = PMSTACK_SEL;
  dpmi_stack_frame[current_client].esp = PMSTACK_ESP;
  in_dpmi_dos_int = 0;
  in_dpmi_pm_ex = 1;
  in_dpmi_sw_to_pm_ex = 1;
  for (; (!fatalerr && in_dpmi_pm_ex) ;) {
    dpmi_control();
    handle_signals();
/*  trigger any hardware interrupts requested */
/*  These MUST be reflected to protected mode first */
    if (1 /*!in_dpmi_dos_int*/) {
      run_irqs();
      serial_run();
      int_queue_run();
    }
  }
  error("DPMI pm ex: shouldn't be here\n");
}

void dpmi_init()
{
  /* Holding spots for REGS and Return Code */
  us CS, DS, ES, SS;
  unsigned char *ssp;
  unsigned long sp;
  int my_ip, my_cs, my_sp, i;
  unsigned char *cp;

  current_client++;

  CARRY;

  DPMI_rm_procedure_running = 0;

  DPMIclient_is_32 = LWORD(eax) ? 1 : 0;
  DPMI_private_data_segment = REG(es);

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);
      
  my_ip = popw(ssp, sp);
  my_cs = popw(ssp, sp);

  cp = (unsigned char *) ((my_cs << 4) +  my_ip);

  D_printf("Going protected with fingers crossed\n"
		"32bit=%d, CS=%04x SS=%04x DS=%04x PSP=%04x ip=%04x sp=%04x\n",
		LO(ax), my_cs, LWORD(ss), LWORD(ds), LWORD(ebx), my_ip, REG(esp));
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
    D_printf("%02x ", popb(ssp, sp));
  D_printf("-> ");
  for (i = 0; i < 10; i++)
    D_printf("%02x ", popb(ssp, sp));
  D_printf("\n");

  if (!config.dpmi_size)
    return;

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

#if 0
  /* should be in dpmi_quit() */
  for (i=0;i<MAX_SELECTORS;i++) FreeDescriptor(i<<3);
#endif

  /* all selectors are used */
  memset(ldt_buffer,0xff,LDT_ENTRIES*LDT_ENTRY_SIZE);
  modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE);

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
                  MODIFY_LDT_CONTENTS_DATA, 1, 0)) return;

  if (!(PMSTACK_SEL = AllocateDescriptors(1))) return;
  if (SetSelector(PMSTACK_SEL, (unsigned long) pm_stack, DPMI_pm_stack_size-1, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0)) return;
  PMSTACK_ESP = DPMI_pm_stack_size;

  if (!(DPMI_SEL = AllocateDescriptors(1))) return;
  if (SetSelector(DPMI_SEL, (unsigned long) (DPMI_SEG << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0 )) return;

  for (i=0;i<0x100;i++) {
    Interrupt_Table[i].offset = DPMI_OFF + HLT_OFF(DPMI_interrupt) + i;
    Interrupt_Table[i].selector = DPMI_SEL;
  }
  for (i=0;i<0x20;i++) {
    Exception_Table[i].offset = DPMI_OFF + HLT_OFF(DPMI_exception) + i;
    Exception_Table[i].selector = DPMI_SEL;
  }

  if (!(CS = AllocateDescriptors(1))) return;
  if (SetSelector(CS, (unsigned long) (my_cs << 4), 0xffff, 0,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0 )) return;

  if (!(SS = AllocateDescriptors(1))) return;
  if (SetSelector(SS, (unsigned long) (LWORD(ss) << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return;

  if (LWORD(ss) == LWORD(ds))
    DS=SS;
  else {
    if (!(DS = AllocateDescriptors(1))) return;
    if (SetSelector(DS, (unsigned long) (LWORD(ds) << 4), 0xffff, DPMIclient_is_32,
                    MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return;
  }

  if (!(ES = AllocateDescriptors(1))) return;
  if (SetSelector(ES, (unsigned long) (LWORD(ebx) << 4), 0x00ff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return;

{/* convert environment pointer to a descriptor*/
      unsigned short envp, envpd;
      envp = *(unsigned short *)(((char *)(LWORD(ebx)<<4))+0x2c);
      if(!(envpd = AllocateDescriptors(1))) return;
      if (SetSelector(envpd, (unsigned long) (envp << 4), 0x03ff,
		      DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0,
		      0 )) return;
      *(unsigned short *)(((char *)(LWORD(ebx)<<4))+0x2c) = envpd;
      D_printf("DPMI: env segement %X converted to descriptor %X\n", envp,envpd);
  }

  print_ldt();
  D_printf("LDT_ALIAS=%x DPMI_SEL=%x CS=%x DS=%x SS=%x ES=%x\n", LDT_ALIAS, DPMI_SEL, CS, DS, SS, ES);

  REG(esp) += 4;
  my_sp = LWORD(esp);
  NOCARRY;

  save_rm_regs();

  REG(ss) = DPMI_private_data_segment;
  REG(esp) = 0x010 * DPMI_private_paragraphs;
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);

  in_dpmi = 1;
  in_dpmi_dos_int = 0;
  in_sigsegv--;
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

  for (; (!fatalerr && in_dpmi) ;) {
    run_dpmi();
/*  trigger any hardware interrupts requested */
/*  These MUST be reflected to protected mode first */
    if (1 /* !in_dpmi_dos_int */) {
      run_irqs();
      serial_run();
      int_queue_run();
    }
  }
  in_sigsegv++;
}

static inline void Return_to_dosemu_code(struct sigcontext_struct *scp)
{
  memcpy(&dpmi_stack_frame[current_client], scp, sizeof(struct sigcontext_struct));
  memcpy(scp, &emu_stack_frame, sizeof(struct sigcontext_struct));
}

void dpmi_sigio(struct sigcontext_struct *scp)
{
  us *ssp;

  if (_cs != UCODESEL){
    D_printf("DPMI: signal at 0x%04x:0x%08x\n", _cs, _eip);
    if (!in_dpmi_pm_ex)
	Return_to_dosemu_code(scp);
    D_printf("DPMI: switch to 0x%04x:0x%08x to handler signal\n", _cs, _eip);
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

inline void do_default_cpu_exception(struct sigcontext_struct *scp, int trapno)
{ 
  save_rm_regs();
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);
  in_dpmi_dos_int = 1;
  switch (trapno) {
    case 0x00: /* divide_error */
    case 0x01: /* debug */
    case 0x03: /* int3 */
    case 0x04: /* overflow */
    case 0x05: /* bounds */
    case 0x07: /* device_not_available */
    case 0x0c: 
	      return (void) do_dpmi_int(scp, trapno);
    default:
	       p_dos_str("DPMI: Unhandled Execption %02x - Terminating Client\n"
			 "It is likely that dosemu is unstable now and should be rebooted\n",
			 trapno);
	       quit_dpmi(0xff);
  }
}

static DPMI_show_state1(struct sigcontext_struct *scp)
{
    u_char *csp2, *ssp2;
    int i;
    D_printf("eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n" \
	     "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n" \
	     "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x  fs: 0x%04x  gs: 0x%04x\n",
	     _eip, _esp, _eflags, _trapno, scp->err, scp->cr2, _cs, _ds, _es, _ss, _fs, _gs);
    D_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx\n",
	     _eax, _ebx, _ecx, _edx);
    D_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
	     _esi, _edi, _ebp);
    /* display the 10 bytes before and after CS:EIP.  the -> points
     * to the byte at address CS:EIP
     */ \
    if (!((_cs) & 0x0004)) {
      /* GTD */
      csp2 = (unsigned char *) _eip - 10;
    } 
    else { 
      /* LDT */ 
      csp2 = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip) - 10; 
    } 
    D_printf("OPS  : ");
    for (i = 0; i < 10; i++) 
      D_printf("%02x ", *csp2++); 
    D_printf("-> "); 
    for (i = 0; i < 10; i++) 
      D_printf("%02x ", *csp2++); 
    D_printf("\n"); 
    if (!((_ss) & 0x0004)) { 
      /* GTD */ 
      ssp2 = (unsigned char *) _esp - 10; 
    } 
    else { 
      /* LDT */ 
      if (Segments[_ss>>3].is_32) 
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _esp ) - 10; 
      else 
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _LWORD(esp) ) - 10; 
    } 
    D_printf("STACK: "); 
    for (i = 0; i < 10; i++) 
      D_printf("%02x ", *ssp2++); 
    D_printf("-> "); 
    for (i = 0; i < 10; i++) 
      D_printf("%02x ", *ssp2++); 
    D_printf("\n");
}

/*
 * DANG_BEGIN_FUNCTION do_cpu_exception
 *
 * This calls the DPMI client exception handler. If none is
 * installed do_default_cpu_exception() is called.
 *
 * DANG_END_FUNCTION
 */

inline void do_cpu_exception(struct sigcontext_struct *scp)
{
/*
 * DANG_FIXTHIS not yet implemented 
 */
  error("DPMI: Unhandled Execption 0x%02x\n", _trapno);
  print_ldt();
  DPMI_show_state1(scp);
  if ((_cs != UCODESEL)&&(Exception_Table[_trapno].selector!=DPMI_SEL)) {
      D_printf("DPMI call user\'s exception handler at %X:%X\n",
	       Exception_Table[_trapno].selector,
	       Exception_Table[_trapno].offset);
      return  run_pm_exception(_trapno, scp);
  }
  else
      do_default_cpu_exception(scp,_trapno);
}

inline void * segment_to_address(unsigned short seg, unsigned reg)
{
    unsigned long __res;
    if (!((seg) & 0x0004)) { 
	/* GTD */ \
	__res = (unsigned long) reg; 
    } else { 
	/* LDT */
	if (Segments[seg>>3].is_32)
	    __res = (unsigned long) (GetSegmentBaseAddress(seg) + reg );
	else 
	    __res = (unsigned long) (GetSegmentBaseAddress(seg)
				     + *((unsigned short *)&(reg)) );
    }
    return (void *)__res;
}
/* this is used like: SEG_ADR((char *), _ss, _esp) */
#undef SEG_ADR
#define SEG_ADR(type, seg, reg) (type(segment_to_address(seg,reg)))

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

void dpmi_fault(struct sigcontext_struct *scp)
{
  us *ssp;
  unsigned char *csp;
  unsigned char *csp2, *ssp2;
  int i;

#if 1
#if 1
  if (!(_cs==UCODESEL))
#endif
  {
    D_printf("DPMI: CPU Exception at 0x%04x:0x%08x!\n", _cs, _eip);
    DPMI_show_state;
  }
#endif /* 1 */
  
  if (_trapno == 13) {
    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    ssp = (us *) SEL_ADR(_ss, _esp);

    switch (*csp++) {

    case 0xcd:			/* int xx */
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
	  REG(eflags) &= ~(IF|VIF);
	}
	_cs = Interrupt_Table[*csp].selector;
	_eip = Interrupt_Table[*csp].offset;
	D_printf("DPMI: calling interrupthandler 0x%02x at 0x%04x:0x%08lx\n", *csp, _cs, _eip);
      }
      break;

    case 0xf4:			/* hlt */
      _eip += 1;
/*      D_printf("DPMI: hlt at %X:%X\n", _cs,_eip); */
      if (_cs==UCODESEL || in_dpmi_pm_int || in_dpmi_sw_to_pm_ex) {
	/* HLT in dosemu code - must in dpmi_control() */
	if (in_dpmi_sw_to_pm_ex) {
	    memcpy(scp, &dpmi_stack_frame[current_client], sizeof(struct sigcontext_struct)); /* switch the stack */
	    D_printf("DPMI: jump to user's ex handler at 0x%04x:0x%08x\n",
		     _cs, _eip);
	    in_dpmi_sw_to_pm_ex =0;
	} else {
	    memcpy(&emu_stack_frame, scp, sizeof(struct sigcontext_struct)); /* backup the registers */
	    memcpy(scp, &dpmi_stack_frame[current_client], sizeof(struct sigcontext_struct)); /* switch the stack */
	    D_printf("DPMI: now jumping to dpmi client code at 0x%04x:0x%08x\n",
		     _cs, _eip);
	    DPMI_show_state;
	    in_dpmi_pm_int=0;
	}
	return;
      }
      if (_cs==DPMI_SEL)
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
	  struct vm86_regs *buffer = (struct vm86_regs *) SEL_ADR(_es, _edi);
	  if (_LO(ax)==0) {
            D_printf("DPMI: save real mode registers\n");
	    memcpy(buffer, &vm86s, sizeof(struct vm86_regs));
	  } else {
            D_printf("DPMI: restore real mode registers\n");
	    memcpy(&vm86s, buffer, sizeof(struct vm86_regs));
	   }
          /* _eip point to FAR RET */

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_API_extension)) {
          D_printf("DPMI: extension API call: 0x%04x\n", _LWORD(eax));
          if (_LWORD(eax) == 0x0100) {
            _eax = LDT_ALIAS;  /* simulate direct ldt access */
#ifdef WIN31
            *ssp += (us) 7; /* jump over writeaccess test of the LDT in krnl386.exe! */
#endif
	  } else
            _eflags |= CF;

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_pm)) {
          D_printf("DPMI: Return from protected mode interrupt hander\n");
	  if (DPMIclient_is_32) {
	    PMSTACK_ESP = _esp + 20;
	    _eip = *(((unsigned long *) ssp)++);
	    _cs = *ssp++;
	    ssp++;
	    in_dpmi_dos_int = (int) *(((unsigned long *) ssp)++);
	    _esp = *(((unsigned long *) ssp)++);
	    _ss = *ssp++;
	    ssp++;
	  } else {
	    PMSTACK_ESP = _esp + 10;
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    in_dpmi_dos_int = (int) *ssp++;
	    _LWORD(esp) = *ssp++;
	    _ss = *ssp++;
	  }
	  pic_iret();
	  D_printf("DPMI: Return from protected mode int. back to 0x%04x:0x%08x\n", _cs, _eip);
        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_return_from_pm_eh)) {
          D_printf("DPMI: Return from protected mode exception hander\n");
	  if (DPMIclient_is_32) {
	    PMSTACK_ESP = _esp + 28;
	    ssp++; ssp++;	/* pop error code */
	    _eip = *(((unsigned long *) ssp)++);
	    _cs = *ssp++;
	    ssp++;
	    _eflags = (int) *(((unsigned long *) ssp)++);
	    _esp = *(((unsigned long *) ssp)++);
	    _ss = *ssp++;
	    ssp++;
	    in_dpmi_dos_int = (int) *ssp++;
	  } else {
	    PMSTACK_ESP = _esp + 16;
	    ssp++;		/* pop error code */
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eflags) = (int) *ssp++;
	    _LWORD(esp) = *ssp++;
	    _ss = *ssp++;
	    in_dpmi_dos_int = (int) *ssp++;
	  }
	  DPMI_show_state1(scp);
	  memcpy(&dpmi_stack_frame[current_client], &exception_stack_frame,
		 sizeof(struct sigcontext_struct));
	  in_dpmi_pm_ex = 0;
	  return;
	} else if ((_eip>=DPMI_OFF+1+HLT_OFF(DPMI_exception)) && (_eip<=DPMI_OFF+32+HLT_OFF(DPMI_exception))) {
	  /*
	   * DANG_FIXTHIS not yet implemented 
	   */
	  int excp=_eip-1-DPMI_OFF-HLT_OFF(DPMI_exception);
	  D_printf("DPMI: default exception handler called for 0x%x\n", excp);
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
        break;
    case 0x6c:                    /* insb */
      *(SEG_ADR((unsigned char *),_es,_edi)) = inb((int) _LWORD(edx));
      D_printf("DPMI: insb(0x%04x) value %02x\n",
	       _LWORD(edx),*(SEG_ADR((unsigned char *),_es,_edi)));   
      if(LWORD(eflags) & DF) _LWORD(edi)--;
      else _LWORD(edi)++;
      _eip++;
      break;

    case 0x6d:			/* insw */
      *(SEG_ADR((unsigned short *),_es,_edi)) =inw((int) _LWORD(edx));
      D_printf("insw(0x%04x) value %04x \n",
	       _LWORD(edx),*(SEG_ADR((unsigned short *),_es,_edi)));
      if(_LWORD(eflags) & DF) _LWORD(edi) -= 2;
      else _LWORD(edi) +=2;
      _eip++;
      break;
      

    case 0x6e:			/* outsb */
      fprintf(stderr,"untested: outsb port 0x%04x value %02x\n",
            _LWORD(edx),*(SEG_ADR((unsigned char *),_ds,_esi)));
      outb(_LWORD(edx), *(SEG_ADR((unsigned char *),_ds,_esi)));
      if(_LWORD(eflags) & DF) _LWORD(esi)--;
      else _LWORD(esi)++;
      _eip++;
      break;


    case 0x6f:			/* outsw */
      fprintf(stderr,"untested: outsw port 0x%04x value %04x\n",
	      _LWORD(edx), *(SEG_ADR((unsigned short *),_ds,_esi)));
      outw(_LWORD(edx), *(SEG_ADR((unsigned short *),_ds,_esi)));
      if(_LWORD(eflags) & DF ) _LWORD(esi) -= 2;
      else _LWORD(esi) +=2; 
      _eip++;
      break;

    case 0xe5:			/* inw xx */
      _LWORD(eax) = inw((int) csp[0]);
      _eip += 2;
      break;
    case 0xe4:			/* inb xx */
      _LWORD(eax) &= ~0xff;
      _LWORD(eax) |= inb((int) csp[0]);
      _eip += 2;
      break;

    case 0xed:			/* inw dx */
      _LWORD(eax) = inw(_LWORD(edx));
      _eip++;
      break;
    case 0xec:			/* inb dx */
      _LWORD(eax) &= ~0xff;
      _LWORD(eax) |= inb(_LWORD(edx));
      _eip += 1;
      break;

    case 0xe7:			/* outw xx */
      outb((int) csp[0], _LO(ax));
      outb((int) csp[0] + 1, _HI(ax));
      _eip += 2;
      break;
    case 0xe6:			/* outb xx */
      outb((int) csp[0], _LO(ax));
      _eip += 2;
      break;

    case 0xef:			/* outw dx */
      outb(_edx, _LO(ax));
      outb(_edx + 1, _HI(ax));
      _eip += 1;
      break;
    case 0xee:			/* outb dx */
      outb(_LWORD(edx), _LO(ax));
      _eip += 1;
      break;

    case 0xf3:                    /* rep */
	switch(csp[0] & 0xff) {                
        case 0x6c: {             /* rep insb */
	    int delta = 1;
	    if(_LWORD(eflags) &DF ) delta = -1;
	    D_printf("Doing REP F3 6C (rep insb) %04x bytes, DELTA %d\n",
		     _LWORD(ecx),delta);
	    while (_LWORD(ecx))  {
		*(SEG_ADR((unsigned char *),_es,_edi)) = inb((int) _LWORD(edx));
		_LWORD(edi) += delta;
		_LWORD(ecx)--;
	    }
	    _eip+=2;
	    break;
	}
	case  0x6d: {           /* rep insw */
	    int delta =2;
	    if(_LWORD(eflags) & DF) delta = -2;
	    i_printf("REP F3 6D (rep insw) %04x words, DELTA %d\n",
		     _LWORD(ecx),delta);
	    while(_LWORD(ecx)) {
		*(SEG_ADR((unsigned short *),_es,_edi))=inw(_LWORD(edx));
		_LWORD(edi) += delta;
		_LWORD(ecx)--;
	    }
	    _eip +=2;
	    break;
	}
	case 0x6e: {           /* rep outsb */
	    int delta = 1;
	    fprintf(stderr,"untested: rep outsb\n");
	    if(_LWORD(eflags) & DF) delta = -1;
	    while(_LWORD(ecx)) {
		outb(_LWORD(edx), *(SEG_ADR((unsigned char *),_ds,_esi)));
		_esi += delta;
		_LWORD(ecx)--;
	    }
	    _eip +=2;
	    break;
	}
	case 0x6f: { 
	    int delta = 2;
	    fprintf(stderr,"untested: rep outsw\n");
	    if(_LWORD(eflags) & DF) delta = -2;
	    while(_LWORD(ecx)) {
		outw(_LWORD(edx), *(SEG_ADR((unsigned short *),_ds,_esi)));
		_esi += delta;
		_LWORD(ecx)--;
	    }
	    _eip+=2;
	    break;
	}
	default:
	    fprintf(stderr, "Nope REP F3,CSP[1] = 0x%04x\n", csp[1]);
	}
      break;        

    default:
#ifdef WIN31
      /* Simulate direct LDT access for MS-Windows 3.1 */
      if (win31ldt(scp, --csp))
      return;
#endif

      do_cpu_exception(scp);

    } /* switch */
  } /* _trapno==13 */
  else
    do_cpu_exception(scp);

  if (in_dpmi_dos_int || int_queue_running)
    Return_to_dosemu_code(scp);
}

void dpmi_realmode_hlt(unsigned char * lina)
{
  unsigned short *ssp;

  if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_dpmi_init))) {
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    LWORD(eip) += 1;	/* skip halt to point to FAR RET */
    CARRY;
#if 0
    if (!in_dpmi)
#endif
      dpmi_init();

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dosint))) {

    D_printf("DPMI: Return from DOS Interrupt\n");
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0dd5 & REG(eflags));
    dpmi_stack_frame[current_client].eax = REG(eax);
    dpmi_stack_frame[current_client].ebx = REG(ebx);
    dpmi_stack_frame[current_client].ecx = REG(ecx);
    dpmi_stack_frame[current_client].edx = REG(edx);
    dpmi_stack_frame[current_client].esi = REG(esi);
    dpmi_stack_frame[current_client].edi = REG(edi);
    dpmi_stack_frame[current_client].ebp = REG(ebp);

    restore_rm_regs();
    in_dpmi_dos_int = 0;
    dpmi_control();

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_realmode))) {
    struct RealModeCallStructure *rmreg = (struct RealModeCallStructure *) RealModeContext;

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
    dpmi_control();

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
    D_printf("DPMI: save/restore protected mode registers\n");
    /* must be implemented here */
    REG(eip) += 1;            /* skip halt to point to FAR RET */

  } else
    error("DPMI: unhandled HLT: lina=%p!\n", lina);
}
#undef DPMI_C
