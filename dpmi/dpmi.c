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
 * $Date: 1995/05/06 16:25:48 $
 * $Source: /usr/src/dosemu0.60/dpmi/RCS/dpmi.c,v $
 * $Revision: 1.2 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
 * Revision 1.2  1995/05/06  16:25:48  root
 * Prep for 0.60.2.
 *
 * Revision 1.1  1995/04/08  22:31:40  root
 * Initial revision
 *
 * Revision 2.11  1995/02/25  21:54:02  root
 * *** empty log message ***
 *
 * Revision 2.11  1995/02/25  21:54:02  root
 * *** empty log message ***
 *
 * Revision 2.10  1995/02/05  16:54:12  root
 * *** empty log message ***
 *
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

#if defined(REQUIRES_EMUMODULE) && defined(WANT_WINDOWS)
  #include "ldt.h" /* NOTE we have a patched version in dosemu/include */
#else
  #include <linux/ldt.h>
#endif

#include "kversion.h"
#include <asm/segment.h>
#include <string.h>
#include <errno.h>
#include <asm/page.h>
#include "emu.h"
#include "memory.h"
#include "dosio.h"
#include "machcompat.h"
#include "cpu.h"

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

unsigned long RealModeContext;

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

static char *ldt_buffer;
unsigned short LDT_ALIAS = 0;

static char *pm_stack; /* locked protected mode stack */

extern unsigned long dpmi_free_memory; /* how many bytes memory client */
				       /* can allocate */
extern dpmi_pm_block *pm_block_root[DPMI_MAX_CLIENTS];
extern unsigned long pm_block_handle_used;       /* tracking handle */

static char RCSdpmi[] = "$Header: /usr/src/dosemu0.60/dpmi/RCS/dpmi.c,v 1.2 1995/05/06 16:25:48 root Exp root $";

int in_dpmi = 0;		/* Set to 1 when running under DPMI */
int in_win31 = 0;		/* Set to 1 when running Windows 3.1 */

int dpmi_eflags = 0;		/* used for virtuell interruptflag and pending interrupts */
static int DPMIclient_is_32 = 0;
static unsigned short DPMI_private_data_segment;
int in_dpmi_dos_int = 0;
unsigned short PMSTACK_SEL = 0;	/* protected mode stack selector */
unsigned long PMSTACK_ESP = 0;	/* protected mode stack descriptor */
unsigned short DPMI_SEL = 0;
extern int fatalerr;

static struct sigcontext_struct dpmi_stack_frame[DPMI_MAX_CLIENTS]; /* used to store the dpmi client registers */
static struct sigcontext_struct emu_stack_frame;  /* used to store emulator registers */

/* define CLIENT_USE_GDT_40, to work around some bugy client */
/* (like  dos4gw :=(. ) want to use gdt 0x40 to access bios area */
#define CLIENT_USE_GDT_40

#include "msdos.h"

_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)

inline int get_ldt(void *buffer)
{
  return modify_ldt(0, buffer, 32 * sizeof(struct modify_ldt_ldt_s));
}

__inline__ int set_ldt_entry(int entry, unsigned long base, unsigned int limit,
	      int seg_32bit_flag, int contents, int read_only_flag,
	      int limit_in_pages_flag
#ifdef WANT_WINDOWS
, int seg_not_present, int useable
#endif
)
{
  struct modify_ldt_ldt_s ldt_info;
  unsigned long *lp;
  unsigned long base2, limit2;
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

  limit2 = ldt_info.limit;
  base2 = ldt_info.base_addr;
  if (ldt_info.limit_in_pages) {
  	limit2 *= PAGE_SIZE;
  	limit2 += PAGE_SIZE-1;
  }

  limit2 += base2;
  if ((limit2 < base2 || limit2 >= 0xC0000000) && ldt_info.seg_not_present == 0) {
    if (base2 >= 0xC0000000) {
      ldt_info.seg_not_present = 1;
      D_printf("DPMI: WARNING: set segment[0x%04x] to NOT PRESENT\n",entry);
    } else {
      if (ldt_info.limit_in_pages)
        ldt_info.limit = ((0xC0000000 - base2)>>12) - 1;
      else
        ldt_info.limit = 0xC0000000 - base2 - 1;
      D_printf("DPMI: WARNING: reducing limit of segment[0x%04x]\n",entry);
    }
  }

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

  if (ldt_info.base_addr == 0 && ldt_info.limit == 0 &&
      ldt_info.contents == 0 && ldt_info.read_exec_only == 1 &&
      ldt_info.seg_32bit == 0 && ldt_info.limit_in_pages == 0
#ifdef WANT_WINDOWS
      && ldt_info.seg_not_present == 1 && ldt_info.useable == 0
#endif
      ) {
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
#ifdef WANT_WINDOWS
            (ldt_info.useable << 20) |
#endif
            0x7000;

  return 0;
}

static void print_ldt(void ) /* stolen from WINE */
{
	/* wow -- why static  */
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
	D_printf("Entry 0x%04x: Base %08.8x, Limit %05.5x, Type %d\n",
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
	D_printf("              %08.8x %08.8x\n", *(lp), *(lp-1));
      }
      else {
	D_printf("Entry 0x%04x: Base %08.8x, Limit %05.5x, Type %d\n",
	       i, base_addr, limit, type);
	D_printf("              SYSTEM: %08.8x %08.8x\n", *lp, *(lp - 1));
      }
      lp2 = (unsigned long *) &ldt_buffer[i*LDT_ENTRY_SIZE];
      D_printf("       cache: %08.8x %08.8x\n", *(lp2+1), *(lp2)); 
      D_printf("         seg: Base %08.8x, Limit %05.5x, Type %d, Big %d\n",
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
    unsigned short old_es = REG(es);
    
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
  unsigned long *lp;
  struct modify_ldt_ldt_s ldt_info;

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

  lp = (unsigned long *) &ldt_buffer[ldt_entry*LDT_ENTRY_SIZE];
  *lp = 0;
  *(lp+1) = 0;

  /* WinOS2 depends on freeed descrpitor really free */
  memset((void *)&ldt_info, 0, sizeof(ldt_info));
  ldt_info.entry_number = ldt_entry;
#ifdef WANT_WINDOWS
  ldt_info.read_exec_only = 1;
  ldt_info.seg_not_present = 1;
#endif  
  return modify_ldt(1, &ldt_info, sizeof(ldt_info));

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
  D_printf("DPMI: SetSegmentBaseAddress[0x%04lx;0x%04lx] 0x%08lx\n", ldt_entry, selector, baseaddr);
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
  D_printf("DPMI: SetSegmentLimit[0x%04lx;0x%04lx] 0x%08lx\n", ldt_entry, selector, limit);
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
  D_printf("DPMI: SetDescriptorAccessRights[0x%04lx;0x%04lx] 0x%04lx\n", ldt_entry, selector, type_byte);

  if (ldt_entry >= MAX_SELECTORS)
    return -1;
  if (!Segments[ldt_entry].used)
    return -1; /* invalid value 8021 */
  /* Only allow conforming Codesegments if Segment is not present */
  if ( ((type_byte>>2)&3) == 3 && ((type_byte >> 7) & 1) == 1)
    return -1; /* invalid selector 8022 */

  Segments[ldt_entry].type = (type_byte >> 2) & 7;
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
			0, Segments[cs_ldt].is_big, 0, 0))
    return 0;
  return ds_selector;
}

static void GetDescriptor(us selector, unsigned long *lp)
{
#if 0
  modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE);
#endif
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
  D_printf("DPMI: GetDescriptor[0x%04lx;0x%04lx]: 0x%08lx%08lx\n", selector>>3, selector, *(lp+1), *lp);
}

static int SetDescriptor(unsigned short selector, unsigned long *lp)
{
  unsigned long base_addr, limit;
  D_printf("DPMI: SetDescriptor[0x%04lx;0x%04lx] 0x%08lx%08lx\n", selector>>3, selector, *(lp+1), *lp);
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
			(*lp >> 10) & 7, ((*lp >> 9) & 1) ^1,
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
  /*0ch*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*10h*/	*++lp = 0xffffffff;
  /*14h*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*18h*/	*++lp = (unsigned int) (mi->total)/DPMI_page_size;
  /*1ch*/	*++lp = dpmi_free_memory/DPMI_page_size;
  /*20h*/	*++lp = (unsigned int) (mi->swaptotal)/DPMI_page_size;
  /*24h*/	*++lp = 0xffffffff;
  /*28h*/	*++lp = 0xffffffff;
  /*2ch*/	*++lp = 0xffffffff;
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
{ if ((((x) & 4) != 4) || (((x) & 0xfffc) == (DPMI_SEL & 0xfffc)) \
      || (((x) & 0xfffc ) == (PMSTACK_SEL & 0xfffc)) \
	|| (((x) & 0xfffc ) == (LDT_ALIAS & 0xfffc))) { \
      _LWORD(eax) = 0x8011; \
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
      _LWORD(eax) = 0x8011;
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
    if (SetDescriptorAccessRights(_LWORD(ebx), _ecx & (DPMIclient_is_32 ? 0xffff : 0x00ff))) {
      _eflags |= CF;
    }
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
    CHECK_SELECTOR(_LWORD(ebx));
    if (SetDescriptor(_LWORD(ebx),
		      (unsigned long *) (GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : _LWORD(edi)) ) ))
      _eflags |= CF;
    break;
  case 0x000d:
    if (AllocateSpecificDescriptor(_LWORD(ebx)))
      _eflags |= CF;
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
    D_printf("DPMI: Get Prot. vec. bx=%x sel=%x, off=%x\n", _LO(bx), _LWORD(ecx), _edx);
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
	    if (can_revector(_LO(bx)))
	      reset_revectored(_LO(bx),&vm86s.int_revectored);
	  else
	    set_revectored(_LO(bx),&vm86s.int_revectored);
        default:
      }
    }
    D_printf("DPMI: Put Prot. vec. bx=%x sel=%x, off=%x\n", _LO(bx),
      _LWORD(ecx), Interrupt_Table[_LO(bx)].offset);
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
       D_printf("DPMI: Allocate realmode callback for 0x%0x4:0x%08x use #%i callback address\n",
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
      D_printf("DPMI: malloc attempt for siz 0x%08lx\n", (_LWORD(ebx))<<16 | (_LWORD(ecx)));
      D_printf("      malloc returns address 0x%08lx\n", block->base);
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
	D_printf("      realloc returns address 0x%08lx\n", block -> base);
	_LWORD(ecx) = (unsigned long)(block->base) & 0xffff;
	_LWORD(ebx) = ((unsigned long)(block->base) >> 16) & 0xffff;
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
  memcpy(scp, &dpmi_stack_frame[current_client], sizeof(struct sigcontext_struct));
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  HI(ax) = 0x4c;
  LO(ax) = errcode;
  return (void) do_int(0x21);
}

static void do_dpmi_int(struct sigcontext_struct *scp, int i)
{
  us *ssp;

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
   static int retval;
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */

  if (int_queue_running || in_dpmi_dos_int) {
    if(pic_icount)
      REG(eflags) |= VIP;

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
			do_int(VM86_ARG(retval));
		}
		break;
#ifdef USE_VM86PLUS
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
    if(pic_icount)
      dpmi_eflags |= VIP;
    dpmi_control();
  }
    handle_signals();

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

    for (i=0;i<MAX_SELECTORS;i++) FreeDescriptor(i<<3);

    /* all selectors are used */
    memset(ldt_buffer,0xff,LDT_ENTRIES*LDT_ENTRY_SIZE);
    modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE);

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
		"32bit=%d, CS=%04x SS=%04x DS=%04x PSP=%04x ip=%04x sp=%04x\n",
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
    D_printf("%02x ", popb(ssp, sp));
  D_printf("-> ");
  for (i = 0; i < 10; i++)
    D_printf("%02x ", popb(ssp, sp));
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

  save_rm_regs();

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

static inline void Return_to_dosemu_code(struct sigcontext_struct *scp)
{
  memcpy(&dpmi_stack_frame[current_client], scp, sizeof(struct sigcontext_struct));
  memcpy(scp, &emu_stack_frame, sizeof(struct sigcontext_struct));
}

void dpmi_sigio(struct sigcontext_struct *scp)
{
  if (_cs != UCODESEL){
#if 0
    if (dpmi_eflags & IF) {
      D_printf("DPMI: return to dosemu code for handling signals\n");
      Return_to_dosemu_code(scp);
    } else dpmi_eflags |= VIP;
#else
/* DANG_FIXTHIS We shouldn't return to dosemu code if IF=0, but it helps - WHY? */
    D_printf("DPMI: return to dosemu code for handling signals\n");
    Return_to_dosemu_code(scp);
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

static void do_cpu_exception(struct sigcontext_struct *scp)
{
  us *ssp;
  unsigned char *csp2, *ssp2;
  int i;

  /* My log file grows to 2MB, I have to turn off dpmi debugging,
     so this log excptions even dpmi debug is off */
  unsigned char dd = d.dpmi;
/*  d.dpmi = 1;*/
  D_printf("DPMI: do_cpu_exception(0x%02x) called\n",_trapno);
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

void dpmi_fault(struct sigcontext_struct *scp)
{
  us *ssp;
  unsigned char *csp;
  unsigned char *csp2, *ssp2;
  int i;

#ifdef SHOWREGS
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
	memcpy(&emu_stack_frame, scp, sizeof(struct sigcontext_struct)); /* backup the registers */
	memcpy(scp, &dpmi_stack_frame[current_client], sizeof(struct sigcontext_struct)); /* switch the stack */
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
      *((unsigned char *)SEL_ADR(_es,_edi)) = inb((int) _LWORD(edx));
      D_printf("DPMI: insb(0x%04x) value %02x\n",
	       _LWORD(edx),*((unsigned char *)SEL_ADR(_es,_edi)));   
      if(LWORD(eflags) & DF) _LWORD(edi)--;
      else _LWORD(edi)++;
      _eip++;
      break;
    case 0x6d:			/* insw */
      *((unsigned short *)SEL_ADR(_es,_edi)) = inw((int) _LWORD(edx));
      D_printf("DPMI: insw(0x%04x) value %04x \n",
	       _LWORD(edx),*((unsigned short *)SEL_ADR(_es,_edi)));
      if(_LWORD(eflags) & DF) _LWORD(edi) -= 2;
      else _LWORD(edi) +=2;
      _eip++;
      break;
    case 0x6e:			/* outsb */
      D_printf("DPMI: untested: outsb port 0x%04x value %02x\n",
            _LWORD(edx),*((unsigned char *)SEL_ADR(_ds,_esi)));
      outb(_LWORD(edx), *((unsigned char *)SEL_ADR(_ds,_esi)));
      if(_LWORD(eflags) & DF) _LWORD(esi)--;
      else _LWORD(esi)++;
      _eip++;
      break;
    case 0x6f:			/* outsw */
      D_printf("DPMI: untested: outsw port 0x%04x value %04x\n",
	      _LWORD(edx), *((unsigned short *)SEL_ADR(_ds,_esi)));
      outw(_LWORD(edx), *((unsigned short *)SEL_ADR(_ds,_esi)));
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
	    D_printf("DPMI: Doing REP F3 6C (rep insb) %04x bytes, DELTA %d\n",
		     _LWORD(ecx),delta);
	    while (_LWORD(ecx))  {
		*((unsigned char *)SEL_ADR(_es,_edi)) = inb((int) _LWORD(edx));
		_LWORD(edi) += delta;
		_LWORD(ecx)--;
	    }
	    _eip+=2;
	    break;
	}
	case  0x6d: {           /* rep insw */
	    int delta =2;
	    if(_LWORD(eflags) & DF) delta = -2;
	    D_printf("DPMI: REP F3 6D (rep insw) %04x words, DELTA %d\n",
		     _LWORD(ecx),delta);
	    while(_LWORD(ecx)) {
		*((unsigned short *)SEL_ADR(_es,_edi))=inw(_LWORD(edx));
		_LWORD(edi) += delta;
		_LWORD(ecx)--;
	    }
	    _eip +=2;
	    break;
	}
	case 0x6e: {           /* rep outsb */
	    int delta = 1;
	    D_printf("DPMI: untested: rep outsb\n");
	    if(_LWORD(eflags) & DF) delta = -1;
	    while(_LWORD(ecx)) {
		outb(_LWORD(edx), *((unsigned char *)SEL_ADR(_ds,_esi)));
		_esi += delta;
		_LWORD(ecx)--;
	    }
	    _eip +=2;
	    break;
	}
	case 0x6f: { 
	    int delta = 2;
	    D_printf("DPMI: untested: rep outsw\n");
	    if(_LWORD(eflags) & DF) delta = -2;
	    while(_LWORD(ecx)) {
		outw(_LWORD(edx), *((unsigned short *)SEL_ADR(_ds,_esi)));
		_esi += delta;
		_LWORD(ecx)--;
	    }
	    _eip+=2;
	    break;
	}
	default:
	    D_printf("DPMI: Nope REP F3,CSP[1] = 0x%04x\n", csp[1]);
	    do_cpu_exception(scp);
      }
      break;        
    default:

      if (msdos_fault(scp))
	  return;
      do_cpu_exception(scp);

    } /* switch */
  } /* _trapno==13 */
  else
    do_cpu_exception(scp);

  if (in_dpmi_dos_int || int_queue_running || ((dpmi_eflags&(VIP|IF))==(VIP|IF)) ) {
    dpmi_eflags &= ~VIP;
    Return_to_dosemu_code(scp);
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
  unsigned short *ssp;

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
#undef DPMI_C
