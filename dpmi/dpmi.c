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
 * $Date: 1994/06/27 02:17:03 $
 * $Source: /home/src/dosemu0.60/dpmi/RCS/dpmi.c,v $
 * $Revision: 2.4 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
 * Revision 2.4  1994/06/27  02:17:03  root
 * Prep for pre53
 *
 * Revision 2.3  1994/06/24  14:52:04  root
 * Lutz's patches against DPMI
 * ,
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
#include <linux/segment.h>
#include <string.h>
#include <errno.h>
#include "emu.h"
#include "memory.h"
#include "dosio.h"
#include "machcompat.h"
#include "cpu.h"
#include "dpmi.h"
#include "bios.h"
#include "config.h"

extern struct config_info config;
unsigned long RealModeContext;
extern int int_queue_running;
extern u_char in_sigsegv;

extern print_ldt();

INTDESC Interrupt_Table[0x100];
INTDESC Exception_Table[0x20];
SEGDESC Segments[MAX_SELECTORS];
static char ldt_buffer[LDT_ENTRIES*LDT_ENTRY_SIZE];

static char RCSdpmi[] = "$Header: /home/src/dosemu0.60/dpmi/RCS/dpmi.c,v 2.4 1994/06/27 02:17:03 root Exp root $";

/* Set to 1 when running under DPMI */
u_char in_dpmi = 0;
u_char in_win31 = 0;
u_char DPMIclient_is_32 = 0;
us DPMI_private_data_segment;
u_char in_dpmi_dos_int = 0;
us DPMI_SEL = 0;
us LDT_ALIAS = 0;
extern int fatalerr;

void dpmi_get_entry_point()
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

  return;
}

inline int SetSelector(unsigned short selector, unsigned long base_addr, unsigned long limit,
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

inline unsigned short AllocateDescriptors(int number_of_descriptors)
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
  return (next_ldt<<3) | 0x0007;
}

inline int FreeDescriptor(unsigned short selector)
{
  return SetSelector(selector, 0, 0, 0, 0, 0, 0);
}

inline unsigned short ConvertSegmentToDescriptor(unsigned short segment)
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

inline unsigned short GetNextSelectorIncrementValue(void)
{
  return 8;
}

inline unsigned long GetSegmentBaseAddress(unsigned short selector)
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

inline unsigned char SetSegmentLimit(unsigned short selector, unsigned long limit)
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

inline unsigned char SetDescriptorAccessRights(unsigned short selector, unsigned short type_byte)
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

inline unsigned short CreateCSAlias(unsigned short selector)
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

u_char GetDescriptor(us selector, unsigned long *lp)
{
  if (modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE) < 0)
    return 1;
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
  return 0;
}

inline unsigned char SetDescriptor(unsigned short selector, unsigned long *lp)
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

u_char AllocateSpecificDescriptor(us selector)
{
  int ldt_entry = selector >> 3;
  if (Segments[ldt_entry].used)
    return 1;
  Segments[ldt_entry].used = 1;
  return 0;
}

void GetFreeMemoryInformation(unsigned long *lp)
{
  int i;
  *lp = (unsigned long) config.dpmi_size;
  for (i=0;i<11;i++)
    *++lp = 0xffffffff;
}
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
    break;
  case 0x0008:
    if (SetSegmentLimit(_LWORD(ebx), (_LWORD(ecx))<<16 |(_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    break;
  case 0x0009:
    if (SetDescriptorAccessRights(_LWORD(ebx), _ecx & (DPMIclient_is_32 ? 0xffff : 0x00ff)))
      _eflags |= CF;
    break;
  case 0x000a:
    if (!(_LWORD(eax) = CreateCSAlias(_LWORD(ebx))))
      _eflags |= CF;
    break;
  case 0x000b:
    if (GetDescriptor(_LWORD(ebx),
		(unsigned long *) (GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : _LWORD(edi)) ) ))
      _eflags |= CF;
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
    break;
  case 0x0205:	/* Set Protected Mode Interrupt vector */
    Interrupt_Table[_LO(bx)].selector = _LWORD(ecx);
    Interrupt_Table[_LO(bx)].offset = (DPMIclient_is_32 ? _edx : _LWORD(edx));
    break;
  case 0x0300:	/* Simulate Real Mode Interrupt */
  case 0x0301:	/* Call Real Mode Procedure With Far Return Frame */
  case 0x0302:	/* Call Real Mode Procedure With Iret Frame */
    RealModeContext = GetSegmentBaseAddress(_es) + (DPMIclient_is_32 ? _edi : _LWORD(edi));
    {
      struct RealModeCallStructure *rmreg = (struct RealModeCallStructure *) RealModeContext;
      us *ssp;
      us *rm_ssp;
      int i;

      ssp = (us *) SEL_ADR(_ss, _esp);
      REG(edi) = rmreg->edi;
      REG(esi) = rmreg->edi;
      REG(ebp) = rmreg->ebp;
      REG(ebx) = rmreg->ebx;
      REG(edx) = rmreg->edx;
      REG(ecx) = rmreg->ecx;
      REG(eax) = rmreg->eax;
      LWORD(eflags) = rmreg->flags;
      REG(es) = rmreg->es;
      REG(ds) = rmreg->ds;
      REG(fs) = rmreg->fs;
      REG(gs) = rmreg->gs;
      if (inumber==0x0300) {
	REG(cs) = ((us *) 0)[(_LO(bx) << 1) + 1];
	REG(eip) = ((us *) 0)[_LO(bx) << 1];
      } else {
	REG(cs) = rmreg->cs;
	REG(eip) = (us) rmreg->ip;
      }
      if ((rmreg->sp==0) /*&& (rmreg->ss==0)*/) {
	REG(ss) = DPMI_private_data_segment;
	REG(esp) = 0x010 * DPMI_private_paragraphs;
      } else {
	LWORD(esp) = rmreg->sp;
	HWORD(esp) = 0;
	REG(ss) = rmreg->ss;
      }
      if (!LWORD(esp))
	rm_ssp = (SEG_ADR((us *), ss, sp)) + 0x8000;
      else
	rm_ssp = SEG_ADR((us *), ss, sp);
      for (i=0;i<(_LWORD(ecx)); i++)
	*--rm_ssp = *(ssp+(_LWORD(ecx)) - 1 - i);  /*pushw*/
      REG(esp) -= 2 * (_LWORD(ecx));
      if (inumber==0x0301)
	       REG(esp) -= 4;
      else {
	REG(esp) -= 6;
	*--rm_ssp = REG(eflags);
      }
      *--rm_ssp = DPMI_SEG;
      *--rm_ssp = DPMI_OFF + HLT_OFF(DPMI_return_from_realmode); 
      in_dpmi_dos_int = 1;
    }
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
    _LO(bx) = 1;
    _LO(cx) = vm86s.cpu_type;
    _LWORD(edx) = 0; /* PIC base interrupt ???????? */
    break;
  case 0x0500:
    GetFreeMemoryInformation( (unsigned long *)
	(GetSegmentBaseAddress(_es) + DPMIclient_is_32 ? _edi : _LWORD(edi)));
    break;
  case 0x0501:	/* Allocate Memory Block */
    { unsigned long *ptr;
      ptr = malloc( (_LWORD(ebx))<<16 | (_LWORD(ecx)) );
      D_printf("DPMI: malloc returns address 0x%08lx\n", ptr);
      if (ptr==NULL)
	_eflags |= CF;
      _LWORD(edi) = (unsigned long) ptr & 0xffff;
      _LWORD(esi) = ((unsigned long) ptr >> 16) & 0xffff;
    }
    _LWORD(ebx) = _LWORD(esi);
    _LWORD(ecx) = _LWORD(edi);
    break;
  case 0x0502:	/* Free Memory Block */
    free((void *)( (_LWORD(esi))<<16 | (_LWORD(edi)) ));
    break;
  case 0x0503:	/* Resize Memory Block */
    { unsigned long *ptr;
      ptr = realloc((void *)(_LWORD(esi)<<16 | _LWORD(edi)), _LWORD(ebx)<<16 | _LWORD(ecx));
      if (ptr==NULL)
	_eflags |= CF;
      _LWORD(edi) = (unsigned long) ptr & 0xffff;
      _LWORD(esi) = ((unsigned long) ptr >> 16) & 0xffff;
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
  case 0x0900:	/* Get and Disable Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & VIF) ? 1 : 0;
    REG(eflags) &= ~VIF;
    break;
  case 0x0901:	/* Get and Enable Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & VIF) ? 1 : 0;
    REG(eflags) |= VIF;
    break;
  case 0x0902:	/* Get Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & VIF) ? 1 : 0;
    break;
  case 0x0a00:	/* Get Vendor Specific API Entry Point */
    {
      char *ptr = (char *) (GetSegmentBaseAddress(_ds) + (DPMIclient_is_32 ? _esi : _LWORD(esi)));
      D_printf("DPMI: GetVendorAPIEntryPoint: %s\n", ptr);
      if (!strcmp("MS-DOS", ptr)) {
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

inline void do_dpmi_int(struct sigcontext_struct *scp, int i)
{
  us *ssp;

  if ((i == 0x2f) && (_LWORD(eax) == 0x168a))
    return do_int31(scp, 0x0a00);
  if ((i == 0x21) && (_HI(ax) == 0x4c)) {
    D_printf("DPMI: leaving DPMI with error code 0x%02x\n",_LO(ax));
    REG(eax) = _eax;
    REG(cs) = DPMI_SEG;
    REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);
    in_dpmi = 0;
    in_dpmi_dos_int = 1;
    return (void) do_int(0x21);
  } else if (i == 0x31) {
    D_printf("DPMI: int31, eax=%04lx\n", _eax);
    return do_int31(scp, _LWORD(eax));
  } else {
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
    return (void) do_int(i);
  }
}

inline void run_dpmi(void)
{
  static int retval;
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */
#if 1
  if (int_queue_running || in_dpmi_dos_int) {
#else
  if (*OUTB_ADD || in_dpmi_dos_int) {
#endif
    in_vm86 = 1;
    switch VM86_TYPE({retval=vm86(&vm86s); in_vm86=0; retval;}) {
	case VM86_UNKNOWN:
		vm86_GP_fault();
		break;
	case VM86_STI:
		I_printf("Return from vm86() for timeout\n");
		REG(eflags) &=~VIP;
		break;
	case VM86_INTx:
		do_int(VM86_ARG(retval));
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

  if (iq.queued)
    do_queued_ioctl();
}

void dpmi_init()
{
  /* Holding spots for REGS and Return Code */
  us CS, DS, ES, SS;
  us *ssp = SEG_ADR((us *), ss, sp);
  int my_ip, my_cs, my_sp, i;
  unsigned char *cp, *ssp2;

  CARRY;

  DPMIclient_is_32 = LWORD(eax) ? 1 : 0;
  DPMI_private_data_segment = REG(es);

  my_ip = *ssp++;
  my_cs = *ssp++;
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
  ssp2 = (unsigned char *)ssp - 10;
  for (i = 0; i < 10; i++)
    D_printf("%02x ", *ssp2++);
  D_printf("-> ");
  for (i = 0; i < 10; i++)
    D_printf("%02x ", *ssp2++);
  D_printf("\n");

  if (!config.dpmi_size)
    return;

  /* should be in dpmi_quit() */
  for (i=0;i<MAX_SELECTORS;i++) FreeDescriptor(i<<3);

  modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE);
  /* all other selectors are used */
  memset(&ldt_buffer[MAX_SELECTORS], 0xff, (LDT_ENTRIES-MAX_SELECTORS)*LDT_ENTRY_SIZE);

/*
 * DANG_BEGIN_NEWIDEA
 * Simulate Local Descriptor Table for MS-Windows 3.1
 * must be read only, so if krnl386.exe/krnl286.exe
 * try to write to this table, we will bomb into sigsegv()
 * and and emulate direct ldt access
 * DANG_END_NEWIDEA
 */
  if (!(LDT_ALIAS = AllocateDescriptors(1))) return;
  if (SetSelector(LDT_ALIAS, (unsigned long) ldt_buffer, 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_DATA, 1, 0)) return;

  if (!(DPMI_SEL = AllocateDescriptors(1))) return;
  if (SetSelector(DPMI_SEL, (unsigned long) (DPMI_SEG << 4), 0xffff, DPMIclient_is_32,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0 )) return;

  for (i=0;i<0x100;i++) {
    Interrupt_Table[i].offset = DPMI_OFF + HLT_OFF(DPMI_interrupt) + i;
    Interrupt_Table[i].selector = DPMI_SEL;
  }
  for (i=0;i<0x20;i++) {
    Exception_Table[i].offset = DPMI_OFF + HLT_OFF(DPMI_exception);
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

  print_ldt();
  D_printf("LDT_ALIAS=%x DPMI_SEL=%x CS=%x DS=%x SS=%x ES=%x\n", LDT_ALIAS, DPMI_SEL, CS, DS, SS, ES);

  REG(esp) += 4;
  my_sp = LWORD(esp);
  NOCARRY;
  REG(ss) = DPMI_private_data_segment;
  REG(esp) = 0x010 * DPMI_private_paragraphs;
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);

  in_dpmi = 1;
  in_dpmi_dos_int = 0;
  in_sigsegv--;
  CallToInit(my_ip, CS, my_sp, SS, DS, ES);
  ssp = (us *) SEL_ADR(DPMI_SavedDOS_ss, DPMI_SavedDOS_esp);
  {
    struct pm86 *dpmir = (struct pm86 *) ssp;
    dpmir->eflags &= ~0x00000dd5;
    dpmir->eflags |= 0x00000dd5 & REG(eflags);
    dpmir->eax = REG(eax);
    dpmir->ebx = REG(ebx);
    dpmir->ecx = REG(ecx);
    dpmir->edx = REG(edx);
    dpmir->esi = REG(esi);
    dpmir->edi = REG(edi);
    dpmir->ebp = REG(ebp);
  }
  for (; (!fatalerr && in_dpmi) ;) {
    run_dpmi();
    serial_run();
    int_queue_run();
  }
  in_sigsegv++;
}

inline void dpmi_sigalrm(struct sigcontext_struct *scp)
{
  us *ssp;

  if (_cs != UCODESEL){
    D_printf("DPMI: sigalrm\n");
    ssp = (us *) SEL_ADR(_ss, _esp);
    *--ssp = (us) 0;
    *--ssp = (us) _cs;
    *(--((unsigned long *) ssp)) = _eip;
    _esp -= 8;
    _cs = UCODESEL;
    _eip = (unsigned long) ReturnFrom_dpmi_control;
  }
}

/*
 * DANG_BEGIN_FUNCTION void dpmi_fault(struct sigcontext_struct);
 *
 * This is the brain of DPMI. All CPU exceptions are first
 * reflected (from the signal handlers) to this code.
 *
 * Exception from nonpriveleged instructions INT XX, STI, CLI, HLT
 * and from WINDOWS 3.1 are handled here.
 *
 * If installed one, the DPMI client exception handler is called first.
 * If not installed, exceptions 0, 1, 2, 3, 4, 5 and 7 are reflected
 * to real mode. All other exceptions are terminating the client
 * (and may be dosemu too :-)).
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
  D_printf("DPMI: CPU Exception!\n");
  DPMI_show_state;
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
	/* hope that 32Bit Clients don't use initial (16Bit) CS for interrupthandler */
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
	_cs = Interrupt_Table[*csp].selector;
	_eip = Interrupt_Table[*csp].offset;
	D_printf("DPMI: calling interrupthandler 0x%02x at 0x%04x:0x%08lx\n", *csp, _cs, _eip);
      }
      break;

    case 0xf4:			/* hlt */
      _eip += 1;
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
	  _cs = UCODESEL;
	  _eip = (unsigned long) ReturnFrom_dpmi_control;

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
            *ssp += (us) 7; /* jump over writeaccess test of the LDT in krnl386.exe! */
	  } else
            _eflags |= CF;

        } else if (_eip==DPMI_OFF+1+HLT_OFF(DPMI_exception)) {
	  D_printf("DPMI: default exception handler called\n");
	  goto leavedpmi;

	} else if ((_eip>=DPMI_OFF+1+HLT_OFF(DPMI_interrupt)) && (_eip<=DPMI_OFF+256+HLT_OFF(DPMI_interrupt))) {
	  D_printf("DPMI: default protected mode interrupthandler called\n");
	  i = _eip - 1 - DPMI_OFF - HLT_OFF(DPMI_interrupt);
	  /* hope that 32Bit Clients don't use initial (16Bit) CS for interrupthandler */
	  if (DPMIclient_is_32) {
	    _eflags = *(((unsigned long *) ssp)++);
	    ssp++;
	    _cs = *ssp++;
	    _eip = *((unsigned long *) ssp);
	    _esp += 12;
	  } else {
	    _LWORD(eflags) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(eip) = *ssp;
	    _esp += 6;
	  }
	  do_dpmi_int(scp, i);

        } else
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
      /* Simulate direct LDT access for MS-Windows 3.1 */
      if (win31ldt(scp, csp))
      return;

      error("DPMI: Unhandled Execption 0x%02x\n", _trapno);
leavedpmi:
      p_dos_str("DPMI: Unhandled Execption %02lx at %04x:%08lx\n"
      		"Terminating Client\n"
		"It is likely that dosemu is unstable now and should be rebooted\n",
		_trapno, _cs, _eip);
      REG(eax) = 0x4cff;
      REG(cs) = DPMI_SEG;
      REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);
      print_ldt();
      in_dpmi = 0;
      in_dpmi_dos_int = 1;
      do_int(0x21);
    } /* switch */
    if (in_dpmi_dos_int || int_queue_running) {
      *--ssp = (us) 0;
      *--ssp = (us) _cs;
      *(--((unsigned long *) ssp)) = _eip;
      _esp -= 8;
      _cs = UCODESEL;
      _eip = (unsigned long) ReturnFrom_dpmi_control;
    }
    return;
  } /* _trapno==13 */

  error("DPMI: Unhandled Execption 0x%02x\n", _trapno);
  p_dos_str("DPMI: Unhandled Execption %02lx at %04x:%08lx\n"
	    "Terminating Client\n"
	    "It is likely that dosemu is unstable now and should be rebooted\n",
	    _trapno, _cs, _eip);
  REG(eax) = 0x4cff;
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dosint);
  print_ldt();
  in_dpmi = 0;
  in_dpmi_dos_int = 1;
  do_int(0x21);
  *--ssp = (us) 0;
  *--ssp = (us) _cs;
  *(--((unsigned long *) ssp)) = _eip;
  _esp -= 8;
  _cs = UCODESEL;
  _eip = (unsigned long) ReturnFrom_dpmi_control;
}

inline void dpmi_realmode_hlt(unsigned char * lina)
{
  unsigned short *ssp;

  if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_dpmi_init))) {
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    LWORD(eip) += 1;	/* skip halt to point to FAR RET */
    CARRY;
    if (!in_dpmi)
      dpmi_init();

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_dosint))) {
    struct pm86 *dpmir = (struct pm86 *) SEL_ADR(DPMI_SavedDOS_ss, DPMI_SavedDOS_esp);

    D_printf("DPMI: Return from DOS Interrupt\n");
    dpmir->eflags &= ~0xdd5;
    dpmir->eflags |= REG(eflags) & 0xdd5;
    dpmir->eax = REG(eax);
    dpmir->ebx = REG(ebx);
    dpmir->ecx = REG(ecx);
    dpmir->edx = REG(edx);
    dpmir->esi = REG(esi);
    dpmir->edi = REG(edi);
    dpmir->ebp = REG(ebp);
    in_dpmi_dos_int = 0;

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_return_from_realmode))) {
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

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_raw_mode_switch))) {
    D_printf("DPMI: switching from real to protected mode\n");
    show_regs();
    in_dpmi_dos_int = 0;
    if (DPMIclient_is_32)
      CallToInit(REG(edi), LWORD(esi), REG(ebx), LWORD(edx), LWORD(eax), LWORD(ecx));
    else
      CallToInit(LWORD(edi), LWORD(esi), LWORD(ebx), LWORD(edx), LWORD(eax), LWORD(ecx));
    ssp = (us *) SEL_ADR(DPMI_SavedDOS_ss, DPMI_SavedDOS_esp);
    {
      struct pm86 *dpmir = (struct pm86 *) ssp;
      dpmir->eax = dpmir->ebx = dpmir->ecx = dpmir->edx = dpmir->esi = dpmir->edi = 0;
      dpmir->ebp = REG(ebp);
    }

  } else if (lina == (unsigned char *) (DPMI_ADD + HLT_OFF(DPMI_save_restore))) {
    D_printf("DPMI: save/restore protected mode registers\n");
    /* must be implemented here */
    REG(eip) += 1;            /* skip halt to point to FAR RET */

  } else
    error("DPMI: unhandled HLT: lina=%p!\n", lina);
}
#undef DPMI_C
