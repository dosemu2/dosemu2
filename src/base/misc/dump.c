/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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
#include "int.h"

extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *,
                     unsigned int, int);


char *emu_disasm(int sga, unsigned int ip)
{
   static unsigned char buf[256];
   unsigned char frmtbuf[256];
   int rc, i;
   unsigned char *cp;
   unsigned char *p;
   unsigned int refseg;
   unsigned int ref;

   if (sga) {
     cp = SEG_ADR((unsigned char *), cs, ip);
     refseg = REG(cs);
   }
   else {
     cp = (unsigned char *)ip;
     refseg = 0;	/* ??? */
   }

   rc = dis_8086((unsigned long)cp, cp, frmtbuf, 0, &refseg, &ref, 0, 1);

   p = buf;
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", *(cp+i));
   }
   sprintf(p,"%20s", " ");
   if (sga)
     sprintf(buf+20, "%04x:%04x %s", REG(cs), LWORD(eip), frmtbuf);
   else
     sprintf(buf+20, "%08x %s", ip, frmtbuf);

   return buf;
}


/*  */
/* show_regs,show_ints @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu-i386/cpu.c --> src/base/misc/dump.c  */
void
show_regs(char *file, int line)
{
  int i;
  unsigned char *sp;
  unsigned char *cp;

  if (!d.general)
    return;

  cp = SEG_ADR((unsigned char *), cs, ip);
  if (!cp) {
    g_printf("Ain't gonna do it, cs=0x%x, eip=0x%x\n",REG(cs),LWORD(eip));
    return;
  }

  if (!LWORD(esp))
    sp = (SEG_ADR((u_char *), ss, sp)) + 0x8000;
  else
    sp = SEG_ADR((u_char *), ss, sp);

  g_printf("\nProgram=%s, Line=%d\n", file, line);
  g_printf("EIP: %04x:%08lx", LWORD(cs), REG(eip));
  g_printf(" ESP: %04x:%08lx", LWORD(ss), REG(esp));
#if 1
  g_printf("  VFLAGS(b): ");
  for (i = (1 << 0x14); i > 0; i = (i >> 1)) {
    g_printf((vflags & i) ? "1" : "0");
    if (i & 0x10100) g_printf(" ");
  }
#else
  g_printf("         VFLAGS(b): ");
  for (i = (1 << 0x11); i > 0; i = (i >> 1))
    g_printf((vflags & i) ? "1" : "0");
#endif
  g_printf("\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx VFLAGS(h): %08lx",
	      REG(eax), REG(ebx), REG(ecx), REG(edx), (unsigned long)vflags);
  g_printf("\nESI: %08lx EDI: %08lx EBP: %08lx",
	      REG(esi), REG(edi), REG(ebp));
  g_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
	      LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs));

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (REG(eflags)&(f)) g_printf(#f" ")

  g_printf("FLAGS: ");
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
  g_printf(" IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  g_printf("STACK: ");
  sp -= 10;
  for (i = 0; i < 10; i++)
    g_printf("%02x ", *sp++);
  g_printf("-> ");
  for (i = 0; i < 10; i++)
    g_printf("%02x ", *sp++);
  g_printf("\n");
  g_printf("OPS  : ");
  cp -= 10;
  for (i = 0; i < 10; i++)
    g_printf("%02x ", *cp++);
  g_printf("-> ");
  for (i = 0; i < 10; i++)
    g_printf("%02x ", *cp++);
  g_printf("\n\t%s\n", emu_disasm(1,0));
}

void
show_ints(int min, int max)
{
  int i, b;

  max = (max - min) / 3;
  for (i = 0, b = min; i <= max; i++, b += 3) {
    g_printf("%02x| %04x:%04x->%05x    ", b, ISEG(b), IOFF(b),
		IVEC(b));
    g_printf("%02x| %04x:%04x->%05x    ", b + 1, ISEG(b + 1), IOFF(b + 1),
		IVEC(b + 1));
    g_printf("%02x| %04x:%04x->%05x\n", b + 2, ISEG(b + 2), IOFF(b + 2),
		IVEC(b + 2));
  }
}
/* @@@ MOVE_END @@@ 32768 */



