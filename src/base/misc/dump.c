/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#include "emu-ldt.h"
#include "dpmi.h"
#include "int.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "dis8086.h"

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
     cp = (unsigned char *)(uintptr_t)ip;
     refseg = 0;	/* ??? */
   }

   rc = dis_8086(cp, frmtbuf, 0, &ref, refseg * 16);

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

  if (debug_level('g') == 0)
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

  g_printf("Program=%s, Line=%d\n", file, line);
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
  if ((sp < (unsigned char *)0xa0000) && (sp > (unsigned char *)10)) {
	  g_printf("STACK: ");
	  sp -= 10;
	  for (i = 0; i < 10; i++)
		  g_printf("%02x ", *sp++);
	  g_printf("-> ");
	  for (i = 0; i < 10; i++)
		  g_printf("%02x ", *sp++);
	  g_printf("\n");
  }
  if ((cp < (unsigned char *)0xa0000) && (cp>(unsigned char *)10)) {
	  g_printf("OPS  : ");
	  cp -= 10;
	  for (i = 0; i < 10; i++)
		  g_printf("%02x ", *cp++);
	  g_printf("-> ");
	  for (i = 0; i < 10; i++)
		  g_printf("%02x ", *cp++);
	  g_printf("\n\t%s\n", emu_disasm(1,0));
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

#define GetSegmentBaseAddress(s)	((unsigned long)Segments[(s) >> 3].base_addr)
#define IsSegment32(s)			(Segments[(s) >> 3].is_32)

char *DPMI_show_state(struct sigcontext_struct *scp)
{
    static char buf[4096];
    unsigned char *csp2, *ssp2;
    sprintf(buf, "eip: 0x%08x  esp: 0x%08x  eflags: 0x%08lx\n"
	     "\ttrapno: 0x%02x  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	     "\tcs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x  fs: 0x%04x  gs: 0x%04x\n",
	     _eip, _esp, _eflags, _trapno, _err, _cr2, _cs, _ds, _es, _ss, _fs, _gs);
    sprintf(buf, "%sEAX: %08x  EBX: %08x  ECX: %08x  EDX: %08x\n", buf,
	     _eax, _ebx, _ecx, _edx);
    sprintf(buf, "%sESI: %08x  EDI: %08x  EBP: %08x\n", buf,
	     _esi, _edi, _ebp);
    /* display the 10 bytes before and after CS:EIP.  the -> points
     * to the byte at address CS:EIP
     */
    if (!((_cs) & 0x0004)) {
      /* GTD */
      csp2 = (unsigned char *) _rip - 10;
    }
    else {
      /* LDT */
      csp2 = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip) - 10;
    }
    /* We have a problem here, if we get a page fault or any kind of
     * 'not present' error and then we try accessing the code/stack
     * area, we fall into another fault which likely terminates dosemu.
     */
#ifdef X86_EMULATOR
    if (!config.cpuemu || (_trapno!=0x0b && _trapno!=0x0c))
#endif
    {
      int i;
      sprintf(buf, "%sOPS  : ", buf);
      if (!(_cs & 0x0004) ||
	  (uintptr_t)csp2 < 0x110000 ||
	  ((uintptr_t)csp2 > config.dpmi_base &&
	   (uintptr_t)csp2 < config.dpmi_base + config.dpmi * 1024)) {
	for (i = 0; i < 10; i++)
	  sprintf(buf, "%s%02x ", buf, *csp2++);
	sprintf(buf, "%s-> ", buf);
	for (i = 0; i < 10; i++)
	  sprintf(buf, "%s%02x ", buf, *csp2++);
	sprintf(buf, "%s\n", buf);
      } else {
	sprintf(buf, "%sCS:EIP points to invalid memory\n", buf);
      }
      if (!((_ss) & 0x0004)) {
        /* GDT */
        ssp2 = (unsigned char *) _rsp - 10;
      }
      else {
        /* LDT */
        if (Segments[_ss>>3].is_32)
	  ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _esp ) - 10;
        else
	  ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _LWORD(esp) ) - 10;
      }
      sprintf(buf, "%sSTACK: ", buf);
      if (!(_ss & 0x0004) ||
	  (uintptr_t)ssp2 < 0x110000 ||
	  ((uintptr_t)ssp2 > config.dpmi_base &&
	   (uintptr_t)ssp2 < config.dpmi_base + config.dpmi * 1024)) {
	for (i = 0; i < 10; i++)
	  sprintf(buf, "%s%02x ", buf, *ssp2++);
	sprintf(buf, "%s-> ", buf);
	for (i = 0; i < 10; i++)
	  sprintf(buf, "%s%02x ", buf, *ssp2++);
	sprintf(buf, "%s\n", buf);
      } else {
	sprintf(buf, "%sSS:ESP points to invalid memory\n", buf);
      }
    }

    return buf;
}

/* @@@ MOVE_END @@@ 32768 */



