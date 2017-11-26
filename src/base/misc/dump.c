#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "cpu.h"
#include "port.h"
#include "emu-ldt.h"
#include "dpmi.h"
#include "int.h"
#include "mapping.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "dis8086.h"

char *emu_disasm(unsigned int ip)
{
   static char buf[256];
   char frmtbuf[256];
   int rc, i;
   unsigned int cp;
   char *p;
   unsigned int refseg;
   unsigned int ref;

   cp = SEGOFF2LINEAR(_CS, _IP);
   refseg = SREG(cs);

   rc = dis_8086(cp, frmtbuf, 0, &ref, refseg * 16);

   p = buf;
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", READ_BYTE(cp+i));
   }
   sprintf(p,"%20s", " ");
   sprintf(buf+20, "%04x:%04x %s", SREG(cs), LWORD(eip), frmtbuf);

   return buf;
}


/*  */
/* show_regs,show_ints @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu-i386/cpu.c --> src/base/misc/dump.c  */
void show_regs(void)
{
  int i;
  unsigned int sp, cp;

  cp = SEGOFF2LINEAR(_CS, _IP);
  if (!cp) {
    dbug_printf("Ain't gonna do it, cs=0x%x, eip=0x%x\n",SREG(cs),LWORD(eip));
    return;
  }

  if (!LWORD(esp))
    sp = SEGOFF2LINEAR(_SS, _SP) + 0x8000;
  else
    sp = SEGOFF2LINEAR(_SS, _SP);

  dbug_printf("Real-mode state dump:\n");
  dbug_printf("EIP: %04x:%08x", SREG(cs), REG(eip));
  dbug_printf(" ESP: %04x:%08x", SREG(ss), REG(esp));
#if 1
  dbug_printf("  VFLAGS(b): ");
  for (i = (1 << 0x14); i > 0; i = (i >> 1)) {
    dbug_printf((vflags & i) ? "1" : "0");
    if (i & 0x10100) dbug_printf(" ");
  }
#else
  dbug_printf("         VFLAGS(b): ");
  for (i = (1 << 0x11); i > 0; i = (i >> 1))
    dbug_printf((vflags & i) ? "1" : "0");
#endif
  dbug_printf("\nEAX: %08x EBX: %08x ECX: %08x EDX: %08x VFLAGS(h): %08lx",
	      REG(eax), REG(ebx), REG(ecx), REG(edx), (unsigned long)vflags);
  dbug_printf("\nESI: %08x EDI: %08x EBP: %08x",
	      REG(esi), REG(edi), REG(ebp));
  dbug_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
	      SREG(ds), SREG(es), SREG(fs), SREG(gs));

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (REG(eflags)&(f)) dbug_printf(#f" ")

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
  PFLAG(VIF);
  PFLAG(VIP);
  dbug_printf(" IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  if (sp < 0xa0000 && sp > 10) {
	  dbug_printf("STACK: ");
	  sp -= 10;
	  for (i = 0; i < 10; i++)
		  dbug_printf("%02x ", READ_BYTE(sp++));
	  dbug_printf("-> ");
	  for (i = 0; i < 10; i++)
		  dbug_printf("%02x ", READ_BYTE(sp++));
	  dbug_printf("\n");
  }
  if (cp < 0xa0000 && cp>10) {
	  dbug_printf("OPS  : ");
	  cp -= 10;
	  for (i = 0; i < 10; i++)
		  dbug_printf("%02x ", READ_BYTE(cp++));
	  dbug_printf("-> ");
	  for (i = 0; i < 10; i++)
		  dbug_printf("%02x ", READ_BYTE(cp++));
	  dbug_printf("\n\t%s\n", emu_disasm(0));
  }
}

void
show_ints(int min, int max)
{
  int i, b;

  max = (max - min) / 3;
  for (i = 0, b = min; i <= max; i++, b += 3) {
    g_printf("%02x| %04x:%04x->%06x   ", b, ISEG(b), IOFF(b),
		IVEC(b));
    g_printf("%02x| %04x:%04x->%06x   ", b + 1, ISEG(b + 1), IOFF(b + 1),
		IVEC(b + 1));
    g_printf("%02x| %04x:%04x->%06x\n", b + 2, ISEG(b + 2), IOFF(b + 2),
		IVEC(b + 2));
  }
}

#if MAX_SELECTORS != 8192
#error MAX_SELECTORS needs to be 8192
#endif

#define IsSegment32(s)			dpmi_segment_is32(s)

void dump_state(void)
{
    struct sigcontext *scp = dpmi_get_scp();
    show_regs();
    if (scp)
        dbug_printf("\nProtected-mode state dump:\n%s\n", DPMI_show_state(scp));
}
