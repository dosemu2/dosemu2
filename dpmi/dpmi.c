/* 			DPMI support for DOSEMU
 *
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * First Attempted by James B. MacLean jmaclean@fox.nstn.ns.ca
 *
 * $Date: 1994/03/04 00:03:08 $
 * $Source: /home/src/dosemu0.50a/dpmi/RCS/dpmi.c,v $
 * $Revision: 1.5 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
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

static char RCSId[] = "$Id: dpmi.c,v 1.5 1994/03/04 00:03:08 root Exp root $";
static char Copyright[] = "Copyright  Robert J. Amstadt, 1993";

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
#include "dosipc.h"
#include "machcompat.h"
#include "cpu.h"
#include "dpmi.h"

extern struct CPU cpu;
extern struct config_info config;

extern print_ldt();

extern void CallToInit16(unsigned long, unsigned long,
			 unsigned short);

char *GetModuleName(struct w_files *, int index, char *buffer);
extern unsigned char ran_out;
extern char WindowsPath[256];
int PSPSelector;

struct w_files *wine_files = NULL;

static char RCSdpmi[] = "$Header: /home/src/dosemu0.50a/dpmi/RCS/dpmi.c,v 1.5 1994/03/04 00:03:08 root Exp root $";

#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2

static u_char ldt_in_use[255] =
{0};

/* Set to 1 when running under DPMI */
u_char in_dpmi = 0;
u_char DPMIclient_is_32 = 0;
int DPMI_private_data_segment;
u_char in_dpmi_dos_int = 0;

void
dpmi_get_entry_point()
{
  D_printf("Request for DPMI entry ");
  D_printf("cpu=%d, es:di=%04x:%04x\n", cpu.type, LWORD(es), LWORD(edi));
  REG(eax) = 0;			/* no error */

  /* 32 bit programs ar O.K. */
  LWORD(ebx) = 1;
  LO(cx) = cpu.type;

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
  int CS, DS, ES, SS;
  us *ssp = SEG_ADR((us *), ss, sp);
  int my_ip, my_cs, my_sp, i;
  unsigned char *cp;

  DPMIclient_is_32 = LWORD(eax) ? 1 : 0;
  DPMI_private_data_segment = REG(es);

  my_ip = *ssp++;
  my_cs = *ssp++;
  cp = (unsigned char *) ((my_cs << 4) + my_ip);

  D_printf("Going protected with fingers crossed 32bit=%d, CS=%04x SS=%04x DS=%04x PSP=%04x\n",
	   LO(ax), my_cs, LWORD(ss), LWORD(ds), LWORD(ebx));
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

  /* should be in dpim_quit() */
  for (i = 0; i < 5; i++)
    set_ldt_entry(i, 0, 0, 0, 0, 0, 0);

  if ((CS = next_free()) < 0x000000ff) {
    if (set_ldt_entry((int) CS, (unsigned long) (my_cs << 4),
		      0xffff, 0, MODIFY_LDT_CONTENTS_CODE, 0, 0))
      return;
  }
  else {
    D_printf("CS=%x\n", CS);
    return;
  }

  if ((SS = next_free()) < 0x000000ff) {
    if (set_ldt_entry((int) SS, (unsigned long) (LWORD(ss) << 4),
		  0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0))
      return;
  }
  else {
    D_printf("SS=%x\n", SS);
    return;
  }

  if (LWORD(ss) == LWORD(ds)) {
    DS = SS;
  }
  else if ((DS = next_free()) < 0x000000ff) {
    if (set_ldt_entry((int) DS, (unsigned long) (LWORD(ds) << 4),
		  0xffff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0))
      return;
  }
  else {
    D_printf("DS=%x\n", DS);
    return;
  }

  if ((ES = next_free()) < 0x000000ff) {
    if (set_ldt_entry((int) ES, (unsigned long) (LWORD(ebx) << 4),
		  0x00ff, DPMIclient_is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0))
      return;
  }
  else {
    D_printf("ES=%x\n", ES);
    return;
  }

  print_ldt();
  D_printf("CS=%x DS=%x SS=%x ES=%x\n", CS, DS, SS, ES);
  PSPSelector = ((ES << 3) | 0x0007);
  REG(esp) += 4;
  my_sp = LWORD(esp);
  NOCARRY;
  REG(ss) = DPMI_private_data_segment;
  REG(esp) = 0x010 * DPMI_private_paragraphs;
  REG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + 7;

  in_dpmi = 1;
  CallToInit16(((CS << 3) | 0x0007) << 16 | my_ip, ((SS << 3) | 0x0007) << 16 | my_sp,
	       ((DS << 3) | 0x0007));
  D_printf("ReturnFrom16\n");
}

int
next_free(void)
{
  char buffer[0x10000];
  unsigned long *lp;
  unsigned long isfree;
  int next_ldt = 0;

  if (get_ldt(buffer) < 0)
    exit(255);
  lp = (unsigned long *) buffer;
  isfree = *lp & 0x0000FFFF;
  lp++;
  isfree |= (*lp & 0x000F0000);
  while (isfree) {
    next_ldt++;
    lp++;
    isfree = *lp & 0x0000FFFF;
    lp++;
    isfree |= (*lp & 0x000F0000);
    if (next_ldt == 0xff)
      return 0xff;
  }
  return next_ldt;
}

unsigned long 
get_base_addr(int ldt_entry)
{
  char buffer[0x10000];
  unsigned long *lp;
  unsigned long base_addr;

  if (get_ldt(buffer) < 0)
    exit(1);
  lp = (unsigned long *) buffer + (ldt_entry << 1);
  /* First 32 bits of descriptor */
  base_addr = (*lp >> 16) & 0x0000FFFF;
  lp++;

  /* Second 32 bits of descriptor */
  base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);

  return base_addr;
}

#undef DPMI_C
