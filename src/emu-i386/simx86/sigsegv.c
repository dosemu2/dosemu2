/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2000 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "emu86.h"
#include "codegen.h"
#include "dpmi.h"

#include "video.h"
#include "bios.h"
#include "memory.h"
#include "vgaemu.h"
#include "priv.h"
#include "mapping.h"

extern int TrapVgaOn;
extern unsigned long e_vga_base, e_vga_end;

/* ======================================================================= */

#define SKIP_BOUND	// info only, for the moment


#ifdef USE_BOUND
int e_decode_bound_excp (unsigned char *csp, struct sigcontext_struct *scp)
{
  unsigned char modrm,sib;
  long l, r, *bp = NULL;

  modrm = csp[1];
  dbug_printf("BOUND exception at %#04x:%#08lx: %02x %02x%02x%02x%02x\n",
	_cs,_eip, modrm, csp[2], csp[3], csp[4], csp[5]);
  if (*csp != BOUND) return 2;	// goto blank screen & reset

  /* bound does not occur in 16-bit code */
  r = ((long *)&(scp->edi))[7-D_MO(modrm)];
  l = 2;
  switch (D_HO(modrm)) {	// some decoding of range ptr (else CS)
    case 0:
	switch (D_LO(modrm)) {
	    case 4: l++; sib=csp[2]; if (D_LO(sib)==5) l+=4; break;
	    case 5: l+=4; bp = (long *)(*((long *)(csp+2))); break;
	    default: break;
	} break;
    case 2: l+=3;
    case 1: l++;
	if (D_LO(modrm)==4) l++;
	break;
    case 3:	// register
	bp = (long *)(((long *)&(scp->edi))[D_LO(modrm)]);
	break;
  }
  if (bp)
    e_printf("BOUND excp for %08lx in (%08lx..%08lx)\n",r,bp[0],bp[1]);
  else
    e_printf("BOUND excp for %08lx in default CS:(%08lx..%08lx)\n",r,
	CS_DTR.BoundL,CS_DTR.BoundH);
#ifdef SKIP_BOUND
  _eip += l;
  return 0;
#else
  return 1;
#endif
}
#endif

/* ======================================================================= */

unsigned e_VgaRead(unsigned addr, int mode)
{
  unsigned u=0;
  if (vga.inst_emu) {
    addr -= e_vga_base;
    ((unsigned char *) &u)[0] = Logical_VGA_read(addr);
    if (mode&MBYTE) return u;
    ((unsigned char *) &u)[1] = Logical_VGA_read(addr+1);
    if (!(mode&DATA16)) {
      ((unsigned char *) &u)[2] = Logical_VGA_read(addr+2);
      ((unsigned char *) &u)[3] = Logical_VGA_read(addr+3);
    }
//  e_printf("eVgaEmuFault: VGA read at %08lx = %08x mode %x\n",addr,u,mode);
    return u;
  }
  else {
    if (mode&MBYTE) return *((unsigned char *)addr);
      else if (mode&DATA16) return *((unsigned short *)addr);
        else return *((unsigned long *)addr);
  }
}

void e_VgaWrite(unsigned addr, unsigned u, int mode)
{
  if (vga.inst_emu) {
//  e_printf("eVgaEmuFault: VGA write %08x at %08lx mode %x\n",u,addr,mode);
    addr -= e_vga_base;
    Logical_VGA_write(addr, u);
    if (mode&MBYTE) return;
    Logical_VGA_write(addr+1, u>>8);
    if (mode&DATA16) return;
    Logical_VGA_write(addr+2, u>>16);
    Logical_VGA_write(addr+3, u>>24);
  }
  else {
    if (mode&MBYTE) *((unsigned char *)addr) = (unsigned char)u;
      else if (mode&DATA16) *((unsigned short *)addr) = (unsigned short)u;
        else *((unsigned long *)addr) = u;
  }
}

static void e_VgaMovs(struct sigcontext_struct *scp, char op, int w16, int d)
{
  unsigned long rep = (op&2? _ecx : 1);

  if (_err&2) {		/* writing from mem or VGA to VGA */
	if ((_esi>=e_vga_base)&&(_esi<e_vga_end)) op |= 4;
	if (op&1) {		/* byte move */
	    if (op&4) goto vga2vgab;
	    while (rep--) {
		e_VgaWrite(_edi,*((char *)_esi),MBYTE);
		_esi+=d,_edi+=d;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else if (w16&1) {	/* word move */
	    if (op&4) goto vga2vgaw;
	    while (rep--) {
		e_VgaWrite(_edi,*((short *)_esi),DATA16);
		_esi+=d,_edi+=d;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else {			/* long move */
	    d *= 2;
	    if (op&4) goto vga2vgal;
	    while (rep--) {
		e_VgaWrite(_edi,*((long *)_esi),DATA32);
		_esi+=d,_edi+=d;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
  }
  else {		/* reading from VGA to mem or VGA */
	if ((_edi>=e_vga_base)&&(_edi<e_vga_end)) op |= 4;
	if (op&1) {		/* byte move */
	    if (op&4) {		/* vga2vga */
vga2vgab:
	        while (rep--) {
		  e_VgaWrite(_edi,e_VgaRead(_esi,MBYTE),MBYTE);
		  _esi+=d,_edi+=d;
	        }
	    }
	    else while (rep--) {
		*((char *)_edi) = e_VgaRead(_esi,MBYTE);
		_esi+=d,_edi+=d;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else if (w16&1) {	/* word move */
	    if (op&4) {		/* vga2vga */
vga2vgaw:
	        while (rep--) {
		  e_VgaWrite(_edi,e_VgaRead(_esi,DATA16),DATA16);
		  _esi+=d,_edi+=d;
	        }
	    }
	    else while (rep--) {
		*((short *)_edi) = e_VgaRead(_esi,DATA16);
		_esi+=d,_edi+=d;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else {			/* long move */
	    d *= 2;
	    if (op&4) {		/* vga2vga */
vga2vgal:
	        while (rep--) {
		  e_VgaWrite(_edi,e_VgaRead(_esi,DATA32),DATA32);
		  _esi+=d,_edi+=d;
	        }
	    }
	    else while (rep--) {
		*((long *)_edi) = e_VgaRead(_esi,DATA32);
		_esi+=d,_edi+=d;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
  }
}


int e_vgaemu_fault(struct sigcontext_struct *scp, unsigned page_fault)
{
  int i, j;
  unsigned vga_page = 0, u=0;

  e_vga_base = vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12;
  e_vga_end =  e_vga_base + (vga.mem.map[VGAEMU_MAP_BANK_MODE].pages << 12);

  for (i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    j = page_fault - vga.mem.map[i].base_page;
    if (j >= 0 && j < vga.mem.map[i].pages) {
      vga_page = j + vga.mem.map[i].first_page;
//  dbug_printf("eVgaEmuFault: found vga_page %x map %d\n",vga_page,i);
      break;
    }
  }

  if (i == VGAEMU_MAX_MAPPINGS) {
    if (page_fault >= 0xa0 && page_fault < 0xc0) {	/* unmapped VGA area */
//      u = instr_len(SEG_ADR((unsigned char *), cs, ip));
//      LWORD(eip) += u;
      if (in_dpmi || (u==0)) {
        e_printf("eVgaEmuFault: unknown instruction, page at 0x%05x now writable\n", page_fault << 12);
        vga_emu_protect_page(page_fault, 2);
      }
/**/	leavedos(0x5640);
      return 1;
    }
    else if (page_fault >= 0xc0 && page_fault < (0xc0 + vgaemu_bios.pages)) {	/* ROM area */
//      u = instr_len(SEG_ADR((unsigned char *), cs, ip));
//      LWORD(eip) += u;
      if (in_dpmi || (u==0)) {
        e_printf("eVgaEmuFault: unknown instruction, converting ROM to RAM at 0x%05x\n", page_fault << 12);
        vga_emu_protect_page(page_fault, 2);
      }
/**/	leavedos(0x5641);
      return 1;
    }
    else {
      e_printf("eVgaEmuFault: unhandled page fault (not in range)\n");
      return 0;
    }
  }
 
  if (vga_page < vga.mem.pages) {
    unsigned char *p;
    unsigned long cxrep;
    int w16;

    vga.mem.dirty_map[vga_page] = 1;

    if (!vga.inst_emu) {
      /* Normal: make the display page writeable after marking it dirty */
      vga_emu_adjust_protection(vga_page, page_fault);
      return 1;
    }  

/**/  e_printf("eVgaEmuFault: trying %08lx, a=%08lx\n",*((long *)_eip),_edi);

    p = (unsigned char *)_eip;
    if (*p==0x66) w16=1,p++; else w16=0;

    switch (*p) {
	case 0x88:	// write byte
		if ((_err&2)==0) goto badrw;
		if (p[1]!=0x07) goto unimp;
		e_VgaWrite(_edi,_eax,MBYTE);
		_eip = (long)(p+2); break;
	case 0x89:	// write word
		if ((_err&2)==0) goto badrw;
		if (p[1]!=0x07) goto unimp;
		e_VgaWrite(_edi,_eax,(w16? DATA16:DATA32));
		_eip = (long)(p+2); break;
	case 0x8a:	// read byte
		if (_err&2) goto badrw;
		if (p[1]==0x07)
		    *((unsigned char *)&_eax) = e_VgaRead(_edi,MBYTE);
		else if (p[1]==0x17)
		    *((unsigned char *)&_edx) = e_VgaRead(_edi,MBYTE);
		else goto unimp;
		_eip = (long)(p+2); break;
	case 0x8b:	// read word
		if (_err&2) goto badrw;
		if (p[1]!=0x07) goto unimp;
		if (w16)
			*((unsigned short *)&_eax) = e_VgaRead(_edi,DATA16);
		else
			_eax = e_VgaRead(_edi,DATA32);
		_eip = (long)(p+2); break;
	case 0xa4: {	// MOVsb
		int d = (_eflags & EFLAGS_DF? -1:1);
		e_VgaMovs(scp, 0, 0, d);
		_eip = (long)(p+1); } break;
	case 0xa5: {	// MOVsw
		int d = (_eflags & EFLAGS_DF? -1:1);
		e_VgaMovs(scp, 1, w16, d*2);
		_eip = (long)(p+1); } break;
	case 0xaa: {	// STOsb
		int d = (_eflags & EFLAGS_DF? -1:1);
		if ((_err&2)==0) goto badrw;
		e_VgaWrite(_edi,_eax,MBYTE);
		_edi+=d;
		_eip = (long)(p+1); } break;
	case 0xab: {	// STOsw
		int d = (_eflags & EFLAGS_DF? -4:4);
		if ((_err&2)==0) goto badrw;
		if (w16) d>>=1;
		e_VgaWrite(_edi,_eax,(w16? DATA16:DATA32)); _edi+=d;
		_eip = (long)(p+1); } break;
	case 0xac: {	// LODsb
		int d = (_eflags & EFLAGS_DF? -1:1);
		if (_err&2) goto badrw;
		*((char *)&_eax) = e_VgaRead(_esi,MBYTE);
		_esi+=d;
		_eip = (long)(p+1); } break;
	case 0xad: {	// LODsw
		int d = (_eflags & EFLAGS_DF? -4:4);
		if (_err&2) goto badrw;
		if (w16) {
		    d >>= 1;
		    *((short *)&_eax) = e_VgaRead(_esi,DATA16);
		}
		else
		    _eax = e_VgaRead(_esi,DATA32);
		_esi+=d;
		_eip = (long)(p+1); } break;
	case 0xf3: {
		int d = (_eflags & EFLAGS_DF? -1:1);
		if (p[1]==0x66) w16=1,p++;
		if (p[1]==0xaa) {
		    if ((_err&2)==0) goto badrw;
		    cxrep = _ecx;
		    while (cxrep--) {
			e_VgaWrite(_edi,_eax,MBYTE);
			_edi+=d;
		    }
		    _ecx = 0;
		}
		else if (p[1]==0xa4) {
		    e_VgaMovs(scp, 2, 0, d);
		}
		else if (p[1]==0xab) {
		    if ((_err&2)==0) goto badrw;
		    if (w16) {
		      d *= 2;
		      cxrep = _ecx;
		      while (cxrep--) {
			e_VgaWrite(_edi,_eax,DATA16);
			_edi+=d;
		      }
		      _ecx = 0;
		    }
		    else {
		      d *= 4;
		      cxrep = _ecx;
		      while (cxrep--) {
			e_VgaWrite(_edi,_eax,DATA32);
			_edi+=d;
		      }
		      _ecx = 0;
		    }
		}
		else if (p[1]==0xa5) {
		    e_VgaMovs(scp, 3, w16, d*2);
		}
		else goto unimp;
		_eip = (long)(p+2); }
		break;
	default:
/**/  		leavedos(0x5644);
    }
/**/  e_printf("eVgaEmuFault: new eip=%08lx\n",(long)_eip);
  }
  return 1;

unimp:
  error("eVgaEmuFault: unimplemented decode instr at %08lx: %08lx\n",
	(long)_eip, *((long *)_eip));
  leavedos(0x5643);
badrw:
  error("eVgaEmuFault: bad R/W CR2 bits at %08lx: %08lx\n",
	(long)_eip, _err);
  leavedos(0x5643);
}

/* ======================================================================= */
/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, struct sigcontext_struct);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scanned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */
extern void dosemu_fault1(int signal, struct sigcontext_struct *scp);

#define GetSegmentBaseAddress(s)	(((s) >= (MAX_SELECTORS << 3))? 0 :\
					Segments[(s) >> 3].base_addr)

void e_emu_fault1(int signal, struct sigcontext_struct *scp)
{

#ifdef USE_BOUND
  if (_trapno==0x05) {
	unsigned char *csp;
	if (in_dpmi)
	    csp = (unsigned char *)(GetSegmentBaseAddress(_cs)+_eip);
	else
	    csp = (unsigned char *)_eip;
	
	if (!e_decode_bound_excp(csp, scp)) {
		return;
	}
  }
#endif

  if (d.emu>1) {
    dbug_printf("==============================================================\n");
    dbug_printf("CPU exception 0x%02lx err=0x%08lx cr2=%08lx eip=%08lx\n",
	  	 _trapno, _err, _cr2, _eip);
    dbug_printf("==============================================================\n");

  /*
   * Ok, we were executing a compiled sequence.
   * Too bad we can't backtrack - we have no hardware to do it ;-)
   * Then, either
   * 1) we were executing that sequence for the first time. Its code
   *	is still in the generate buffer, and we know the location
   *	of every instruction.
   * 2) we were executing a previously compiled sequence which didn't
   *	fault before. Its code comes from the collector tree, and we
   *	lost the details of every single instruction.
   * We can look at the XECFND flag to discriminate this.
   */

    dbug_printf("Host CPU mode=%04x\n%s\n",
	InCompiledCode,e_print_scp_regs(scp,(in_dpmi?3:2)));
    dbug_printf("Emul CPU mode=%04x cr2=%08lx\n%s\n",
	TheCPU.mode&0xffff,TheCPU.cr2,e_print_regs());
  }

  /*
   * We are probably asked to stop the execution and run some int
   * set up by the program... so it's a bad idea to just return back.
   * Implemented solution:
   *	for every "sensitive" instruction we store the current PC in
   *	some place, let's say TheCPU.cr2. As we get the exception, we
   *	use this address to return back to the interpreter loop, skipping
   *	the rest of the sequence, and possibly with an error code in
   *	TheCPU.err.
   */

#if X_GRAPHICS
  if (config.X && (_trapno==0x0e)) {
      unsigned pf = (unsigned)_cr2 >> 12;
      if ((pf & 0xfffe0) == 0xa0) {
      	if (!TrapVgaOn) {
//		e_vga_base = vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12;
//		e_vga_end = e_vga_base + (vga.mem.map[VGAEMU_MAP_BANK_MODE].pages << 12);
// e_printf("eVgaEmu: set base=%lx end=%lx\n",e_vga_base,e_vga_end);
		TrapVgaOn = 1;
	}
	if (e_vgaemu_fault(scp,pf) == 1) return;
	goto verybad;
      }
      goto cont0e;
  }
#endif /* X_GRAPHICS */

  if (_trapno==0x0d) {
	(void)dosemu_fault1(signal, scp);
	return;
  }
  else if (_trapno==0x0e) {
        /* bit 0 = 1	page protect
         * bit 1 = 1	writing
         * bit 2 = 1	user mode
         * bit 3 = 0	no reserved bit err
         */
cont0e:
	if ((int)_cr2 < 0) {
		error("Accessing reserved memory at %08lx\n"
		      "\tMaybe a null segment register\n",_cr2);
		goto verybad;
	}
	if ((_err&0x0f)==0x07) {
		if (mprotect((void *)(_cr2&~(PAGE_SIZE-1)),PAGE_SIZE,
			PROT_READ|PROT_WRITE)) {
			perror("munprotect");
			leavedos(0x999);
		}
		InvalidateTreePaged((unsigned char *)_cr2, 0);
		return;
	}
  }
  else if (_trapno==0x00) {
	if (InCompiledCode) {
		TheCPU.err = EXCP00_DIVZ;
		*((unsigned long *)(TailCode+3)) = TheCPU.cr2;
		_eip = (long)TailCode;
		return;
	}
  }
  (void)dosemu_fault1(signal, scp);
  return;

verybad:
  /*
   * Too bad, we have to give up.
   */
  dbug_printf("Unhandled CPUEMU exception %02lx\n", _trapno);
  flush_log();
  if (in_vm86) in_vm86=0;	/* otherwise leavedos() complains */
  fatalerr = 5;
  leavedos(fatalerr);		/* shouldn't return */
}

/* ======================================================================= */

void e_emu_fault(int signal, struct sigcontext_struct context)
{
    e_emu_fault1 (signal, &context);
}


/* ======================================================================= */
