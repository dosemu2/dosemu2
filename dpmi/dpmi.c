/* 			DPMI support for DOSEMU
 *
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * First Attempted by James B. MacLean jmaclean@fox.nstn.ns.ca
 *
 * $Date: 1994/01/25 20:10:27 $
 * $Source: /home/src/dosemu0.49pl4f/dpmi/RCS/dpmi.c,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: dpmi.c,v $
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

static char RCSId[] = "$Id: dpmi.c,v 1.3 1994/01/25 20:10:27 root Exp $";
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
#include "wine.h"
#include "prototypes.h"
#include "emu.h"
#include "memory.h"
#include "dosipc.h"
#include "machcompat.h"
#include "cpu.h"
#include "dpmi.h"

extern struct CPU cpu;
extern struct config_info config;

extern print_ldt();

extern int CallToInit16(unsigned long csip, unsigned long sssp,
			unsigned short ds);
extern void CallTo32();

char *GetModuleName(struct w_files *wpnt, int index, char *buffer);
extern unsigned char ran_out;
extern char WindowsPath[256];
unsigned short WIN_StackSize;
unsigned short WIN_HeapSize;
int DLLRelay;
int PSPSelector;

struct w_files *wine_files = NULL;

static char RCSdpmi[] = "$Header: /home/src/dosemu0.49pl4f/dpmi/RCS/dpmi.c,v 1.3 1994/01/25 20:10:27 root Exp $";

#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2

static u_char ldt_in_use[255] = {0};

/* Set to 1 when running under DPMI */
u_char in_dpmi = 0;

void
dpmi_init()
{

    D_printf("Request for DPMI entry ");
    D_printf("cpu=%d, es:di=%04x:%04x\n", cpu.type, LWORD(es), LWORD(edi));
    REG(eax) = 0;

    /* 32 bit programs ar O.K. */
    LWORD(ebx) = 1;
    LO(cx) = cpu.type;

    /* Version 0.9 */
    HI(dx) = DPMI_VERSION;
    LO(dx) = DPMI_DRIVER_VERSION;

    /* Entry Address for DPMI */
    REG(es) = DPMI_SEG;
    REG(edi) = DPMI_OFF;

  return;
}

void 
dpmi_control()
{

  /* Holding spots for REGS and Return Code */
  int CS, DS, ES, SS;
  int RC;


  D_printf("DPMI limit is %x\n", config.dpmi_size);
  D_printf("EMS  limit is %x\n", config.ems_size);
  D_printf("XMS  limit is %x\n", config.xms_size);
  D_printf("Going protected with fingers crossed 32bit=%d, ES=%04x\n", LO(ax), LWORD(es));

  if ((CS = next_free()) < 0x000000ff) {
    if ( set_ldt_entry( (int)CS, (unsigned long) (LWORD(cs) << 4),
         0xffff, 0, MODIFY_LDT_CONTENTS_CODE, 0, 0 )) leavedos(65);
  }
  else {
    D_printf("CS=%x\n", CS);
    leavedos(66);
  }

  if ((SS = next_free()) < 0x000000ff) {
    if ( set_ldt_entry( (int)SS, (unsigned long) (LWORD(ss) << 4),
         0xffff, 0, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) leavedos(65);
  }
  else {
    D_printf("SS=%x\n", SS);
    leavedos(66);
  }

  if (LWORD(ss) == LWORD(ds)) {
    DS=SS;
  }  
  else
    if ((DS = next_free()) < 0x000000ff){
      if ( set_ldt_entry( (int)DS, (unsigned long) (LWORD(ds) << 4),
           0xffff, 0, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) leavedos(65);
    }
    else {
      D_printf("DS=%x\n", DS);
      leavedos(66);
    }

  if ((ES = next_free()) < 0x000000ff) {
    if ( set_ldt_entry( (int)ES, (unsigned long) (LWORD(ebx) << 4),
         0xffff, 0, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) leavedos(65);
  }
  else {
    D_printf("SS=%x\n", SS);
    leavedos(66);
  }

  print_ldt();
  in_dpmi=1;
  RC = CallToInit16(CS << 16 | LWORD(eip), SS << 16 | LWORD(esp),
      DS);
  in_dpmi=0;
  D_printf("RC=%x\n", RC);

  CARRY;
}

int
next_free(void)
{
  char buffer[0x10000];
  unsigned long *lp;
  unsigned long isfree;
  int next_ldt=0;
  if (get_ldt(buffer) < 0)
      exit(255);
  lp = (unsigned long *)buffer;
  isfree = *lp & 0x0000FFFF;
  lp++;
  isfree |= (*lp & 0x000F0000);
  while (isfree) {
    next_ldt++;
    lp++;
    isfree = *lp & 0x0000FFFF;
    lp++;
    isfree |= (*lp & 0x000F0000);
    if (next_ldt == 0xff) return 0xff;
  } 
  return next_ldt;
}

u_char
free_ldt(int ldt_entry)
{
  if ( set_ldt_entry( (int)ldt_entry, (unsigned long) 0,
           0, 0, MODIFY_LDT_CONTENTS_DATA, 0, 0 )) return 1;
  return 0;
}

void
leave_dpmi()
{
  ReturnFromRegisterFunc();
}

u_char
dpmi_test()
{
  if (in_dpmi) return 0;
  return 1;
}

#undef DPMI_C
