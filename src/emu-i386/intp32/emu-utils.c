/* NOTE:
 * This file was modified for DOSEMU by the DOSEMU-team.
 * The original is 'Copyright 1997 Willows Software, Inc.' and generously
 * was put under the GNU Library General Public License.
 * ( for more information see http://www.willows.com/ )
 *
 * We make use of section 3 of the GNU Library General Public License
 * ('...opt to apply the terms of the ordinary GNU General Public License...'),
 * because the resulting product is an integrated part of DOSEMU and
 * can not be considered to be a 'library' in the terms of Library License.
 * The (below) original copyright notice from Willows therefore was edited
 * conforming to section 3 of the GNU Library General Public License.
 *
 * Nov. 1 1997, The DOSEMU team.
 */


/*    
	utils.c	2.20
    	Copyright 1997 Willows Software, Inc. 

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; see the file COPYING.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.


For more information about the Willows Twin Libraries.

	http://www.willows.com	

To send email to the maintainer of the Willows Twin Libraries.

	mailto:twin@willows.com 

changes for use with dosemu-0.67 1997/10/20 vignani@mbox.vol.it
 */
#include "config.h"

#ifdef X86_EMULATOR
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hsw_interp.h"

#undef CARRY_FLAG	/* well-coordinated work */
#undef ZERO_FLAG
#include "kerndef.h"
#include "BinTypes.h"
#include "DPMI.h"

#include "utils.h"
#include "Log.h"
#include "ctype.h"

#include "port.h"
 
DSCR *LDT;

ENV *envp_global;	/* the current global pointer to the x86 environment */

DSCR baseldt[8] =
{
	{ NULL, 0, 0,
#ifndef TWIN32
	 0,
#endif
	 0, 0, 0 }
};

extern long instr_count;


void WINAPI
FatalAppExit(UINT wAction,LPCSTR lpText)
{
    if (lpText) dbug_printf("%s\n", lpText);
    exit(0);
}

BOOL
LoadSegment(UINT uiSel)
{
#if 0
    UINT uiSegNum;
    MODULEINFO *modinfo;

#ifdef X386
    if (uiSel == native_cs || uiSel == native_ds)
	return FALSE;
#endif

    if (GetPhysicalAddress(uiSel) != (LPBYTE)-1)
#endif
	return FALSE;	/* segment is already loaded or invalid */
#if 0
    if (!(modinfo = lpModuleTable[GetModuleIndex(uiSel)])) {
	ERRSTR((LF_ERROR," ... failed to find module\n"));
	DebugBreak();
	return FALSE;
    }

    uiSegNum = (uiSel >>3) - modinfo->wSelBase + 1;

    return LoadDuplicateSegment(uiSel, uiSegNum, modinfo);
#endif
}

REGISTER PortIO(DWORD dwPort, DWORD dwData, UINT wOpSize, BOOL bWrite)
{
	if (bWrite) {
		switch(wOpSize) {
			case 8:  port_real_outb((ioport_t)dwPort, dwData);
				 break;
			case 16: port_real_outw((ioport_t)dwPort, dwData);
				 break;
			case 32: port_real_outd((ioport_t)dwPort, dwData);
				 break;
		}
	}
	else {
		switch(wOpSize) {
			case 8:  return port_real_inb((ioport_t)dwPort);
			case 16: return port_real_inw((ioport_t)dwPort);
			case 32: return port_real_ind((ioport_t)dwPort);
		}
	}
	return 0;
}

/* debug support */

extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *,
                     unsigned int, int);

static char *e_emu_disasm(Interp_ENV *env, Interp_VAR *interp_var,
	unsigned char *org)
{
   static unsigned char buf[256];
   unsigned char frmtbuf[256];
   int rc;
   int i;
   unsigned char *p;
   unsigned int seg;
   unsigned int refseg;
   unsigned int ref;

   seg = SHORT_CS_16;
   refseg = seg;

   rc = dis_8086((unsigned long)org, org, frmtbuf, 0, &refseg, &ref, 0, 1);

   p = buf + sprintf(buf,"%08lx: ",(long)org);
   for (i=0; i<rc && i<8; i++) {
           p += sprintf(p, "%02x", *(org+i));
   }
   sprintf(p,"%20s", " ");
   sprintf(buf+28, "%04x:%04x %s", seg, org-LONG_CS, frmtbuf);

   return buf;
}

static char *e_print_cpuemu_regs(Interp_ENV *env)
{
	static char buf[1024];
	unsigned short *sp;
	int i, tsp;
	char *p;

	*buf = 0;
	if (d.emu>3) {
	  p = buf;
	  p += sprintf(p,"\teax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n",
		e_EAX,e_EBX,e_ECX,e_EDX);
	  p += sprintf(p,"\tesi=%08lx edi=%08lx ebp=%08lx esp=%08lx\n",
		e_ESI,e_EDI,e_EBP,e_ESP);
	  p += sprintf(p,"\teip=%08lx  cs=%04x      ds=%04x      es=%04x\n",
		e_EIP,e_CS,e_DS,e_ES);
	  p += sprintf(p,"\t fs=%04x      gs=%04x      ss=%04x     flg=%08lx\n",
		e_FS,e_GS,e_SS,e_EFLAGS);
	  tsp = e_SP;
	  p += sprintf(p,"\tstk=");
	  for (i=0; (i<10)&&(!vm86f||(tsp<0x10000)); i++) {
	    sp = (short *)((e_SS<<4)+tsp);
	    p += sprintf(p," %04x",*sp);
	    tsp += 2;
	  }
	  for (; i<10; i++) p+=sprintf(p," ****");
	  p+=sprintf(p,"\n");
	}
	return buf;
}

static char *e_print_internal_regs(Interp_VAR *v)
{
	static char buf[1024];
	char *p;

	*buf = 0;
	if (d.emu>4) {
	  p = buf;
	  p += sprintf(p,"\tFL: %08lx (%04x %04x) (%02x %02x %02x %02x)\n",
		v->flags_czsp.res,
		v->flags_czsp.word16.res16,v->flags_czsp.word16.carry,
		v->flags_czsp.byte.parity16,v->flags_czsp.byte.res8,
		v->flags_czsp.byte.carryb,v->flags_czsp.byte.byte_op);
	  p += sprintf(p,"\tS1: %08lx (%04x ****) (%02x %02x ** **)\n",
		v->src1.longword,
		v->src1.word16.word,
		v->src1.byte.aux,v->src1.byte.byte);
	  p += sprintf(p,"\tS2: %08lx (%04x ****) (%02x %02x ** **)\n",
		v->src2.longword,
		v->src2.word16.word,
		v->src2.byte.aux,v->src2.byte.byte);
	}
	return buf;
}

void e_debug (Interp_ENV *env, unsigned char *P0, unsigned char *PC,
  	Interp_VAR *interp_var)
{
  if ((d.emu>2) && TRACE_HIGH && (P0==PC)) {
    e_printf("%ld\n%s%s    %s\n", instr_count, e_print_cpuemu_regs(env),
	e_print_internal_regs(interp_var), e_emu_disasm(env,interp_var,PC));
  }
}

#endif	/* X86_EMULATOR */
