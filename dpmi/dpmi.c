/* 			DPMI support for DOSEMU
 *
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * First Attempted by James B. MacLean jmaclean@fox.nstn.ns.ca
 *
 * $Date: 1994/04/27 23:58:51 $
 * $Source: /home/src/dosemu0.60/dpmi/RCS/dpmi.c,v $
 * $Revision: 1.14 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
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
#include "config.h"

extern struct config_info config;
extern struct sigcontext_struct *scp;
extern unsigned long RealModeContext;

extern print_ldt();

INTDESC Interrupt_Table[0x100];
INTDESC Exception_Table[0x20];
SEGDESC Segments[MAX_SELECTORS];
static char ldt_buffer[LDT_ENTRIES*LDT_ENTRY_SIZE];

static char RCSdpmi[] = "$Header: /home/src/dosemu0.60/dpmi/RCS/dpmi.c,v 1.14 1994/04/27 23:58:51 root Exp root $";

#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2

/* Set to 1 when running under DPMI */
u_char in_dpmi = 0;
u_char in_win31 = 0;
u_char DPMIclient_is_32 = 0;
us DPMI_private_data_segment;
u_char in_dpmi_dos_int = 0;
us DPMI_SEL = 0;
us LDT_ALIAS = 0;

void
dpmi_get_entry_point()
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

void 
dpmi_init()
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

  /* Simulate Local Descriptor Table for MS-Windows 3.1 */
  /* must be read only, so if krnl386.exe/krnl286.exe */
  /* try to write to this table, we will bomb into sigsegv() */
  /* and and emulate direct ldt access */
  if (LDT_ALIAS = AllocateDescriptors(1)) {
    if ( set_ldt_entry( (int)(LDT_ALIAS>>3), (unsigned long) ldt_buffer,
	 0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 1, 0 )) return;
    Segments[LDT_ALIAS>>3].base_addr = (unsigned long) ldt_buffer;
    Segments[LDT_ALIAS>>3].limit = 0xffff;
    Segments[LDT_ALIAS>>3].type = MODIFY_LDT_CONTENTS_DATA;
    Segments[LDT_ALIAS>>3].is_32 = DPMIclient_is_32;
  }
  else {
    D_printf("LDT_ALIAS=%x\n", LDT_ALIAS);
    return;
  }

  if (DPMI_SEL = AllocateDescriptors(1)) {
    if ( set_ldt_entry( (int)(DPMI_SEL>>3), (unsigned long) (DPMI_SEG << 4),
	 0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_CODE, 0, 0 )) return;
    Segments[DPMI_SEL>>3].base_addr = (unsigned long) (DPMI_SEG << 4);
    Segments[DPMI_SEL>>3].limit = 0xffff;
    Segments[DPMI_SEL>>3].type = MODIFY_LDT_CONTENTS_CODE;
    Segments[DPMI_SEL>>3].is_32 = DPMIclient_is_32;
  }
  else {
    D_printf("DPMI_SEL=%x\n", DPMI_SEL);
    return;
  }

  for (i=0;i<0x100;i++) {
    Interrupt_Table[i].offset = DPMI_OFF + 16;
    Interrupt_Table[i].selector = DPMI_SEL;
  }
  for (i=0;i<0x20;i++) {
    Exception_Table[i].offset = DPMI_OFF + 15;
    Exception_Table[i].selector = DPMI_SEL;
  }

  if (CS = AllocateDescriptors(1)) {
    if ( set_ldt_entry( (int)(CS>>3), (unsigned long) (my_cs << 4),
	 0xffff, 0, MODIFY_LDT_CONTENTS_CODE, 0, 0 )) return;
    Segments[CS>>3].base_addr = (unsigned long) (my_cs << 4);
    Segments[CS>>3].limit = 0xffff;
    Segments[CS>>3].type = MODIFY_LDT_CONTENTS_CODE;
  }
  else {
    D_printf("CS=%x\n", CS);
    return;
  }

  if (SS = AllocateDescriptors(1)) {
    if ( set_ldt_entry( (int)(SS>>3), (unsigned long) (LWORD(ss) << 4),
	 0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return;
    Segments[SS>>3].base_addr = (unsigned long) (LWORD(ss) << 4);
    Segments[SS>>3].limit = 0xffff;
    Segments[SS>>3].type = MODIFY_LDT_CONTENTS_DATA;
    Segments[SS>>3].is_32 = DPMIclient_is_32;
  }
  else {
    D_printf("SS=%x\n", SS);
    return;
  }

  if (LWORD(ss) == LWORD(ds)) {
    DS=SS;
  }
  else
    if (DS = AllocateDescriptors(1)) {
      if ( set_ldt_entry( (int)(DS>>3), (unsigned long) (LWORD(ds) << 4),
	   0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return;
      Segments[DS>>3].base_addr = (unsigned long) (LWORD(ds) << 4);
      Segments[DS>>3].limit = 0xffff;
      Segments[DS>>3].type = MODIFY_LDT_CONTENTS_DATA;
      Segments[DS>>3].is_32 = DPMIclient_is_32;
    }
    else {
      D_printf("DS=%x\n", DS);
      return;
    }

  if (ES = AllocateDescriptors(1)) {
    if ( set_ldt_entry( (int)(ES>>3), (unsigned long) (LWORD(ebx) << 4),
	 0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return;
    Segments[ES>>3].base_addr = (unsigned long) (LWORD(ebx) << 4);
    Segments[ES>>3].limit = 0xffff;
    Segments[ES>>3].type = MODIFY_LDT_CONTENTS_DATA;
    Segments[ES>>3].is_32 = DPMIclient_is_32;
  }
  else {
    D_printf("ES=%x\n", ES);
    return;
  }

  modify_ldt(0, ldt_buffer, MAX_SELECTORS*LDT_ENTRY_SIZE);
  /* all other selectors are used */
  memset(&ldt_buffer[MAX_SELECTORS], 0xff, (LDT_ENTRIES-MAX_SELECTORS)*LDT_ENTRY_SIZE);

  print_ldt();
  D_printf("LDT_ALIAS=%x DPMI_SEL=%x CS=%x DS=%x SS=%x ES=%x\n", LDT_ALIAS, DPMI_SEL, CS, DS, SS, ES);

  REG(esp) += 4;
  my_sp = LWORD(esp);
  NOCARRY;
  REG(ss) = DPMI_private_data_segment;
  REG(esp) = 0x010 * DPMI_private_paragraphs;
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + 8;

  in_dpmi=1;
  in_dpmi_dos_int = 0;
  CallToInit(my_ip, CS, my_sp, SS, DS, ES);
  if (Segments[DPMI_SavedDOS_ss>>3].is_32)
    ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + DPMI_SavedDOS_esp);
  else
    ssp = (us *) (GetSegmentBaseAddress(DPMI_SavedDOS_ss) + (DPMI_SavedDOS_esp & 0xffff));
  {
    struct pm86 *dpmir = (struct pm86 *) ssp;
    dpmir->eflags &= ~0x00000cd5;
    dpmir->eflags |= 0x00000cd5 & REG(eflags);
    dpmir->eax = REG(eax);
    dpmir->ebx = REG(ebx);
    dpmir->ecx = REG(ecx);
    dpmir->edx = REG(edx);
    dpmir->esi = REG(esi);
    dpmir->edi = REG(edi);
    dpmir->ebp = REG(ebp);
  }
}

inline void do_dpmi_int(int i)
{
  us *ssp;

  if ((i == 0x2f) && (_LWORD(eax) == 0x168a))
    do_int31(0x0a00);
  else if (i == 0x31) {
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
    REG(ebp) = _ebp;
    REG(ds) = (long) GetSegmentBaseAddress(_ds) >> 4;
    REG(es) = (long) GetSegmentBaseAddress(_es) >> 4;
    REG(cs) = DPMI_SEG;
    REG(eip) = DPMI_OFF + 8;
    in_dpmi_dos_int = 1;
    do_int(i);
  }
}

/* Simulate direct LDT access for MS-Windows 3.1 */
u_char win31ldt(unsigned char *csp)
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

void do_int31(int inumber)
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
	  ssp = (us *) (GetSegmentBaseAddress(_ss) + _LWORD(esp) );
      }
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
	*--rm_ssp = *(ssp+(_LWORD(ecx)) - 1 - i);
      REG(esp) -= 2 * (_LWORD(ecx));
      if (inumber==0x0301)
	       REG(esp) -= 4;
      else {
	REG(esp) -= 6;
	*--rm_ssp = REG(eflags);
      }
      *--rm_ssp = DPMI_SEG;
      *--rm_ssp = DPMI_OFF + 9; 
      in_dpmi_dos_int = 1;
    }
    break;
  case 0x0305:	/* Get State Save/Restore Adresses */
      _LWORD(ebx) = DPMI_SEG;
      _LWORD(ecx) = DPMI_OFF + 11;
      _LWORD(esi) = DPMI_SEL;
      _edi = DPMI_OFF + 11;
      _LWORD(eax) = sizeof(struct vm86_regs);
    break;
  case 0x0306:	/* Get Raw Mode Switch Adresses */
      _LWORD(ebx) = DPMI_SEG;
      _LWORD(ecx) = DPMI_OFF + 10;
      _LWORD(esi) = DPMI_SEL;
      _edi = DPMI_OFF + 10;
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
    _LO(ax) = (REG(eflags) & IF) ? 1 : 0;
    REG(eflags) &= ~IF;
    break;
  case 0x0901:	/* Get and Enable Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & IF) ? 1 : 0;
    REG(eflags) |= IF;
    break;
  case 0x0902:	/* Get Virtual Interrupt State */
    _LO(ax) = (REG(eflags) & IF) ? 1 : 0;
    break;
  case 0x0a00:	/* Get Vendor Specific API Entry Point */
    {
      char *ptr = (char *) (GetSegmentBaseAddress(_ds) + (DPMIclient_is_32 ? _esi : _LWORD(esi)));
      D_printf("DPMI: GetVendorAPIEntryPoint: %s\n", ptr);
      if (!strcmp("MS-DOS", ptr)) {
	_LWORD(eax) = 0;
	_es = DPMI_SEL;
	_edi = DPMI_OFF + 13;
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

us AllocateDescriptors(int number_of_descriptors)
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

int FreeDescriptor(us selector)
{
  int ldt_entry = selector >> 3;
  if ((_ds==selector)||(_es==selector)||(_ss==selector)||(_fs==selector)||(_gs==selector))
    return 1;
  Segments[ldt_entry].base_addr = 0;
  Segments[ldt_entry].limit = 0;
  Segments[ldt_entry].type = 0;
  Segments[ldt_entry].is_32 = 0;
  Segments[ldt_entry].is_big = 0;
  Segments[ldt_entry].used = 0;
  return set_ldt_entry(ldt_entry, 0, 0, 0, 0, 0, 0 );
}

us ConvertSegmentToDescriptor(us segment)
{
  unsigned long baseaddr = segment << 4;
  us selector;
  int ldt_entry;
  int i;
  for (i=1;i<MAX_SELECTORS;i++)
    if ((Segments[i].base_addr==baseaddr) && (Segments[i].limit==0xffff) &&
	(Segments[i].type==MODIFY_LDT_CONTENTS_DATA) && Segments[i].used)
      return (i<<3) | 0x0007;
  if(!(selector = AllocateDescriptors(1))) return 0;
  ldt_entry = selector >> 3;
  if ( set_ldt_entry(ldt_entry , baseaddr,
	 0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return 0;
  Segments[ldt_entry].base_addr = baseaddr;
  Segments[ldt_entry].limit = 0xffff;
  Segments[ldt_entry].type = MODIFY_LDT_CONTENTS_DATA;
  Segments[ldt_entry].is_32 = DPMIclient_is_32;
  return selector;
}

us GetNextSelectorIncrementValue(void)
{
  return 8;
}

unsigned long GetSegmentBaseAddress(us selector)
{
  return Segments[selector >> 3].base_addr;
}

u_char SetSegmentBaseAddress(us selector, unsigned long baseaddr)
{
  int ldt_entry = selector >> 3;
  if (!Segments[ldt_entry].used)
    return 1;
  Segments[ldt_entry].base_addr = baseaddr;
  if ( set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, 0, Segments[ldt_entry].is_big))
    return 1;
  return 0;
}

u_char SetSegmentLimit(us selector, unsigned long limit)
{
  int ldt_entry = selector >> 3;
  if (!Segments[ldt_entry].used)
    return 1;
  Segments[ldt_entry].limit = limit;
  Segments[ldt_entry].is_big = 0;
  if ( ((limit & 0x0fff) == 0x0fff) && DPMIclient_is_32 ) {
    Segments[ldt_entry].limit = limit>>12;
    Segments[ldt_entry].is_big = 1;
  }
  if ( set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, 0, Segments[ldt_entry].is_big))
    return 1;
  return 0;
}

u_char SetDescriptorAccessRights(us selector, us type_byte)
{
  int ldt_entry = selector >> 3;
  if (!Segments[ldt_entry].used)
    return 1;
  Segments[ldt_entry].type = (type_byte >> 2) & 7;
  Segments[ldt_entry].is_32 = (type_byte >> 14) & 1;
  Segments[ldt_entry].is_big = (type_byte >> 15) & 1;
  if ( set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, 0, Segments[ldt_entry].is_big))
    return 1;
  return 0;
}

us CreateCSAlias(us selector)
{
  us ds_selector;
  int cs_ldt= selector >> 3, ds_ldt;

  ds_ldt = (ds_selector = AllocateDescriptors(1)) >> 3;
  Segments[ds_ldt].base_addr = Segments[cs_ldt].base_addr;
  Segments[ds_ldt].limit = Segments[cs_ldt].limit;
  Segments[ds_ldt].type = MODIFY_LDT_CONTENTS_DATA;
  Segments[ds_ldt].is_32 = Segments[cs_ldt].is_32;
  /*Segments[ds_ldt].is_32 = DPMIclient_is_32;*/
  Segments[ds_ldt].is_big = Segments[cs_ldt].is_big;
  if (set_ldt_entry(ds_ldt, Segments[ds_ldt].base_addr, Segments[ds_ldt].limit, 
			Segments[ds_ldt].is_32, Segments[ds_ldt].type, 0, Segments[ds_ldt].is_big))
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

u_char SetDescriptor(us selector, unsigned long *lp)
{
  int ldt_entry = selector >> 3;
  D_printf("DPMI: SetDescriptor[0x%04lx] 0x%08lx%08lx\n", ldt_entry, *(lp+1), *lp);
  if (ldt_entry >= MAX_SELECTORS)
    return 1;
  Segments[ldt_entry].base_addr = (*lp >> 16) & 0x0000FFFF;
  Segments[ldt_entry].limit = *lp & 0x0000FFFF;
  lp++;
  Segments[ldt_entry].base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
  Segments[ldt_entry].limit |= (*lp & 0x000F0000);
  Segments[ldt_entry].type = (*lp >> 10) & 7;
  Segments[ldt_entry].is_32 = (*lp >> 22) & 1;
  Segments[ldt_entry].is_big = (*lp >> 23) & 1;
  Segments[ldt_entry].used = 1;
  if ( set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, 0, Segments[ldt_entry].is_big))
    return 1;
  /*print_ldt();*/
  return 0;
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

#undef DPMI_C
