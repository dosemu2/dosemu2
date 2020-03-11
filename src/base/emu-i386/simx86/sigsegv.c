/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
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
#include "utilities.h"
#include "emu86.h"
#include "codegen-arch.h"
#include "trees.h"
#include "dpmi.h"

#include "video.h"
#include "bios.h"
#include "memory.h"
#include "priv.h"
#include "mapping.h"

/* ======================================================================= */

unsigned e_VgaRead(unsigned char *a, int mode)
{
  unsigned u;
  dosaddr_t addr = DOSADDR_REL(a);
  if (mode&(MBYTE|MBYTX))
    u = vga_read(addr);
  else {
    u = vga_read_word(addr);
    if (!(mode&DATA16))
      u |= (unsigned)vga_read_word(addr+2) << 16;
  }
#ifdef DEBUG_VGA
  e_printf("eVGAEmuFault: VGA read at %08x = %08x mode %x\n",addr,u,mode);
#endif
  return u;
}

void e_VgaWrite(unsigned char *a, unsigned u, int mode)
{
  dosaddr_t addr = DOSADDR_REL(a);
#ifdef DEBUG_VGA
  e_printf("eVGAEmuFault: VGA write %08x at %08x mode %x\n",u,addr,mode);
#endif
  if (mode&MBYTE) {
    vga_write(addr, u);
    return;
  }
  vga_write_word(addr, u);
  if (mode&DATA16) return;
  vga_write_word(addr+2, u>>16);
}

void e_VgaMovs(sigcontext_t *scp, char op, int w16, int dp)
{
  unsigned int rep = (op&2? _ecx : 1);

#ifdef DEBUG_VGA
  e_printf("eVGAEmuFault: Movs ESI=%08x EDI=%08x ECX=%08x\n",_esi,_edi,rep);
#endif
  if (_err&2) {		/* writing from mem or VGA to VGA */
	if ((unsigned)(_esi-TheCPU.mem_base-vga.mem.bank_base)<vga.mem.bank_len) op |= 4;
	if (op&1) {		/* byte move */
	    if (op&4) goto vga2vgab;
	    while (rep--) {
		e_VgaWrite(LINP(_edi),READ_BYTE(DOSADDR_REL(LINP(_rsi))),MBYTE);
		_esi+=dp,_edi+=dp;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else if (w16&1) {	/* word move */
	    if (op&4) goto vga2vgaw;
	    while (rep--) {
		e_VgaWrite(LINP(_edi),READ_WORD(DOSADDR_REL(LINP(_rsi))),DATA16);
		_esi+=dp,_edi+=dp;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else {			/* long move */
	    dp *= 2;
	    if (op&4) goto vga2vgal;
	    while (rep--) {
		e_VgaWrite(LINP(_edi),READ_DWORD(DOSADDR_REL(LINP(_rsi))),DATA32);
		_esi+=dp,_edi+=dp;
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
  }
  else {		/* reading from VGA to mem or VGA */
	if ((unsigned)(_edi-TheCPU.mem_base-vga.mem.bank_base)<vga.mem.bank_len) op |= 4;
	if (op&1) {		/* byte move */
	    if (op&4) {		/* vga2vga */
vga2vgab:
	        while (rep--) {
		  e_VgaWrite(LINP(_edi),e_VgaRead(LINP(_esi),MBYTE),MBYTE);
		  _esi+=dp,_edi+=dp;
	        }
	    }
	    else {
		m_munprotect(DOSADDR_REL(LINP(_rdi)) - ((dp < 0) ?
			(rep - 1) : 0), rep, LINP(_rip));
		while (rep--) {
		    WRITE_BYTE(DOSADDR_REL(LINP(_rdi)), e_VgaRead(LINP(_esi),MBYTE));
		    _esi+=dp,_edi+=dp;
		}
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else if (w16&1) {	/* word move */
	    if (op&4) {		/* vga2vga */
vga2vgaw:
	        while (rep--) {
		  e_VgaWrite(LINP(_edi),e_VgaRead(LINP(_esi),DATA16),DATA16);
		  _esi+=dp,_edi+=dp;
	        }
	    }
	    else {
		m_munprotect(DOSADDR_REL(LINP(_rdi)) - ((dp < 0) ?
			(rep * 2 - 2) : 0), rep * 2, LINP(_rip));
		while (rep--) {
		    WRITE_WORD(DOSADDR_REL(LINP(_rdi)), e_VgaRead(LINP(_esi),DATA16));
		    _esi+=dp,_edi+=dp;
		}
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
	else {			/* long move */
	    dp *= 2;
	    if (op&4) {		/* vga2vga */
vga2vgal:
	        while (rep--) {
		  e_VgaWrite(LINP(_edi),e_VgaRead(LINP(_esi),DATA32),DATA32);
		  _esi+=dp,_edi+=dp;
	        }
	    }
	    else {
		m_munprotect(DOSADDR_REL(LINP(_rdi)) - ((dp < 0) ?
			(rep * 4 - 4) : 0), rep * 4, LINP(_rip));
		while (rep--) {
		    WRITE_DWORD(DOSADDR_REL(LINP(_rdi)), e_VgaRead(LINP(_esi),DATA32));
		    _esi+=dp,_edi+=dp;
		}
	    }
	    if (op&2) _ecx = 0;
	    return;
	}
  }
}

#if 1
static int jitx86_instr_len(const unsigned char *rip)
{
  const unsigned char *p = rip;

  if (*p==0xf3) p++; /* rep */
  if (*p==0x66) p++; /* word */

  /* moves */
  if (*p >= 0x88 && *p <= 0x8b)
    return p - rip + 2;
  /* string moves */
  if (*p >= 0xa4 && *p <= 0xad)
    return p - rip + 1;
  return 0;
}

int e_vgaemu_fault(sigcontext_t *scp, unsigned page_fault)
{
  int i, j;
  unsigned vga_page = 0, u=0;

  for (i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    j = page_fault - vga.mem.map[i].base_page;
    if (j >= 0 && j < vga.mem.map[i].pages) {
      vga_page = j + vga.mem.map[i].first_page;
#ifdef DEBUG_VGA
      dbug_printf("eVGAEmuFault: found vga_page %x map %d\n",vga_page,i);
#endif
      break;
    }
  }

  if (i == VGAEMU_MAX_MAPPINGS) {
    if ((unsigned)((page_fault << 12) - vga.mem.graph_base) <
	vga.mem.graph_size) {	/* unmapped VGA area */
#ifdef HOST_ARCH_X86
      if (!CONFIG_CPUSIM) {
	u = jitx86_instr_len((unsigned char *)_rip);
	_rip += u;
      }
#endif
      if (u==0) {
        e_printf("eVGAEmuFault: unknown instruction, page at 0x%05x now writable\n", page_fault << 12);
        vga_emu_protect_page(page_fault, 2);
/**/	leavedos_main(0x5640);
      }
      return 1;
    }
    else if (page_fault >= 0xc0 && page_fault < (0xc0 + vgaemu_bios.pages)) {	/* ROM area */
#ifdef HOST_ARCH_X86
      if (!CONFIG_CPUSIM) {
	u = jitx86_instr_len((unsigned char *)_rip);
	_rip += u;
      }
#endif
      if (u==0 || (_err&2)==0) {
        e_printf("eVGAEmuFault: unknown instruction, converting ROM to RAM at 0x%05x\n", page_fault << 12);
        vga_emu_protect_page(page_fault, 2);
/**/	leavedos_main(0x5641);
      }
      return 1;
    }
    else {
      if (debug_level('e')>1)
	dbug_printf("eVGAEmuFault: unhandled page fault (not in range)\n");
      return 0;
    }
  }

  if (vga_page < vga.mem.pages) {
    unsigned char *p;
    unsigned long cxrep;
    int w16;
    if (!vga.inst_emu) {
      /* Normal: make the display page writeable after marking it dirty */
      dosemu_error("simx86: should not be here\n");
      vga_emu_adjust_protection(vga_page, page_fault, VGA_PROT_RW, 1);
      return 1;
    }

/**/  e_printf("eVGAEmuFault: trying %08x, a=%08"PRI_RG"\n",*((int *)_rip),_rdi);

    /* try CPatch, and if that fails, the exceptionally expensive route */
    if (Cpatch(scp))
      return 1;

    p = (unsigned char *)_rip;
    if (*p==0x66) w16=1,p++; else w16=0;

    /* Decode the faulting instruction.
     * Hopefully, since the compiled code contains a well-defined subset
     * of the many possibilities for writing a memory location, this
     * decoder can be kept quite small. It is possible, however, that
     * someone accesses the VGA memory with a shift, or a bit set, and
     * this will cause the cpuemu to fail.
     */
    switch (*p) {
/*88*/	case MOVbfrm:
		if ((_err&2)==0) goto badrw;
		if (p[1]!=0x07) goto unimp;
		e_VgaWrite(LINP(_edi),_eax,MBYTE);
		_rip = (long)(p+2); break;
/*89*/	case MOVwfrm:
		if ((_err&2)==0) goto badrw;
		if (p[1]!=0x07) goto unimp;
		e_VgaWrite(LINP(_edi),_eax,(w16? DATA16:DATA32));
		_rip = (long)(p+2); break;
/*8a*/	case MOVbtrm:
		if (_err&2) goto badrw;
		if (p[1]==0x07)
		    _LO(ax) = e_VgaRead(LINP(_edi),MBYTE);
		else if (p[1]==0x17)
		    _LO(dx) = e_VgaRead(LINP(_edi),MBYTE);
		else goto unimp;
		_rip = (long)(p+2); break;
/*8b*/	case MOVwtrm:
		if (_err&2) goto badrw;
		if (p[1]!=0x07) goto unimp;
		if (w16)
			LO_WORD(_eax) = e_VgaRead(LINP(_edi),DATA16);
		else
			_eax = e_VgaRead(LINP(_edi),DATA32);
		_rip = (long)(p+2); break;
/*f2*/	case REPNE:
/*f3*/	case REP: {
		int repmod;
		int d = (_eflags & EFLAGS_DF? -1:1);
		if (p[1]==0x66) w16=1,p++;
		switch(p[1]) {
	/*aa*/	case STOSb:
		    if ((_err&2)==0) goto badrw;
		    cxrep = _ecx;
		    while (cxrep--) {
			e_VgaWrite(LINP(_edi),_eax,MBYTE);
			_edi+=d;
		    }
		    _ecx = 0;
		    break;
	/*a4*/	case MOVSb:
		    e_VgaMovs(scp, 3, 0, d);
		    break;
	/*ab*/	case STOSw:
		    if ((_err&2)==0) goto badrw;
		    if (w16) {
		      d *= 2;
		      cxrep = _ecx;
		      while (cxrep--) {
			e_VgaWrite(LINP(_edi),_eax,DATA16);
			_edi+=d;
		      }
		      _ecx = 0;
		    }
		    else {
		      d *= 4;
		      cxrep = _ecx;
		      while (cxrep--) {
			e_VgaWrite(LINP(_edi),_eax,DATA32);
			_edi+=d;
		      }
		      _ecx = 0;
		    }
		    break;
	/*a5*/	case MOVSw:
		    e_VgaMovs(scp, 2, w16, d*2);
		    break;
	/*a6*/	case CMPSb:
		    repmod = MBYTE;
		    goto REPCMPS_common;
	/*a7*/	case CMPSw:
		    repmod = w16 ? DATA16 : 0;
		REPCMPS_common:
		    repmod |= MOVSSRC|MOVSDST|MREPCOND|
		      (p[0]==REPNE? MREPNE:MREP);
		    AR1.d = _edi;
		    AR2.d = _esi;
		    TR1.d = _ecx;
		    Gen_sim(O_MOVS_CmpD, repmod);
		    FlagSync_All();
		    _edi = AR1.d;
		    _esi = AR2.d;
		    _ecx = TR1.d;
		    _eflags = (_eflags & ~EFLAGS_CC) | (EFLAGS & EFLAGS_CC);
		    break;
	/*ae*/	case SCASb:
		    repmod = MBYTE;
		    goto REPSCAS_common;
	/*af*/	case SCASw:
		    repmod = w16 ? DATA16 : 0;
		REPSCAS_common:
		    repmod |= MOVSDST|MREPCOND|(p[0]==REPNE? MREPNE:MREP);
		    AR1.d = _edi;
		    DR1.d = _eax;
		    TR1.d = _ecx;
		    Gen_sim(O_MOVS_ScaD, repmod);
		    FlagSync_All();
		    _edi = AR1.d;
		    _ecx = TR1.d;
		    _eflags = (_eflags & ~EFLAGS_CC) | (EFLAGS & EFLAGS_CC);
		    break;
		default:
		    goto unimp;
		}
		_rip = (long)(p+2); }
		break;
	default:
		goto unimp;
    }
/**/  e_printf("eVGAEmuFault: new eip=%08"PRI_RG"\n",_rip);
    vgaemu_dirty_page(vga_page, 1);
  }
  return 1;

unimp:
  error("eVGAEmuFault: unimplemented decode instr at %08"PRI_RG": %08x\n",
	_rip, *((int *)_rip));
  leavedos_from_sig(0x5643);
  return 0;
badrw:
  error("eVGAEmuFault: bad R/W CR2 bits at %08"PRI_RG": %08x\n",
	_rip, _err);
  leavedos_from_sig(0x5643);
  return 0;
}
#endif

/* ======================================================================= */
/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, sigcontext_t);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scanned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */

#define GetSegmentBaseAddress(s)	GetSegmentBase(s)

/* this function is called from dosemu_fault */
int e_emu_pagefault(sigcontext_t *scp, int pmode)
{
    if (CONFIG_CPUSIM) {
	/* in cpusim mode we do not fault for vgaemu */
	if (in_dpmi_emu) {
	    /* reflect DPMI page fault back to a DOSEMU crash or
	       DPMI exception;
	       vm86 faults cannot happen in sim mode so then we die.
	     */
	    TheCPU.err = EXCP0E_PAGE;
	    TheCPU.scp_err = _err;
	    TheCPU.cr2 = _cr2;
	    fault_cnt--;
	    siglongjmp(jmp_env, 0);
	}
    } else if (InCompiledCode) {
	/* in vga.inst_emu mode, vga_emu_fault() can handle
	 * only faults from DOS code, and here we are with
	 * the fault from jit-compiled code. But in !inst_emu
	 * mode vga_emu_fault() just unprotects. */
	dosaddr_t cr2 = DOSADDR_REL(LINP(_cr2));
	if ((!vga.inst_emu && vga_emu_fault(cr2, _err, scp) == True) ||
	    e_vgaemu_fault(scp, cr2 >> 12) == 1)
	    return 1;

#ifdef HOST_ARCH_X86
	if (e_handle_pagefault(scp))
	    return 1;
#endif
	TheCPU.scp_err = _err;
	/* save eip, eflags, and do a "ret" out of compiled code */
	TheCPU.err = EXCP0E_PAGE;
	_eax = FindPC((unsigned char *)_rip);
	e_printf("FindPC: found %x\n",_eax);
	_edx = *(long *)_rsp; // flags
	_rsp += sizeof(long);
	TheCPU.cr2 = _cr2;
	_rip = *(long *)_rsp;
	_rsp += sizeof(long);
	return 1;
    }
    return 0;
}

/* ======================================================================= */
