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
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
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
#include "emudpmi.h"
#include "../dosext/dpmi/msdos/msdos_ldt.h"

#include "video.h"
#include "bios.h"
#include "memory.h"
#include "priv.h"
#include "mapping.h"

/* ======================================================================= */

void e_VgaMovs(unsigned char **rdi, unsigned char **rsi, unsigned int rep,
	       int dp, unsigned int access)
{
  dosaddr_t edi = DOSADDR_REL(*rdi);
  dosaddr_t esi = DOSADDR_REL(*rsi);

#ifdef DEBUG_VGA
  e_printf("eVGAEmuFault: Movs ESI=%08x EDI=%08x ECX=%08x\n",esi,edi,rep);
#endif
  switch (access) {
    case 1:		/* reading from VGA to mem */
	switch (abs(dp)) {
	case 1: /* byte move */
	    while (rep--) {
		WRITE_BYTE(edi, vga_read(esi));
		esi+=dp,edi+=dp;
	    }
	    break;
	case 2: /* word move */
	    while (rep--) {
		WRITE_WORD(edi, vga_read_word(esi));
		esi+=dp,edi+=dp;
	    }
	    break;
	case 4: /* long move */
	    while (rep--) {
		WRITE_DWORD(edi, vga_read_dword(esi));
		esi+=dp,edi+=dp;
	    }
	    break;
	}
	break;
    case 2:		/* writing from mem to VGA */
	switch (abs(dp)) {
	case 1: /* byte move */
	    while (rep--) {
		vga_write(edi,READ_BYTE(esi));
		esi+=dp,edi+=dp;
	    }
	    break;
	case 2: /* word move */
	    while (rep--) {
		vga_write_word(edi,READ_WORD(esi));
		esi+=dp,edi+=dp;
	    }
	    break;
	case 4: /* long move */
	    while (rep--) {
		vga_write_dword(edi,READ_DWORD(esi));
		esi+=dp,edi+=dp;
	    }
	    break;
	}
	break;
    case 3:		/* VGA to VGA */
	switch (abs(dp)) {
	case 1: /* byte move */
	        while (rep--) {
		  vga_write(edi,vga_read(esi));
		  esi+=dp,edi+=dp;
	        }
		break;
	case 2: /* word move */
	        while (rep--) {
		  vga_write_word(edi,vga_read_word(esi));
		  esi+=dp,edi+=dp;
	        }
		break;
	case 4: /* long move */
	        while (rep--) {
		  vga_write_dword(edi,vga_read_dword(esi));
		  esi+=dp,edi+=dp;
	        }
		break;
	}
	break;
  }
  *rsi = MEM_BASE32(esi);
  *rdi = MEM_BASE32(edi);
}

#if 1
static int jitx86_instr_len(const unsigned char *rip)
{
  const unsigned char *p = rip;
  int len;

  if (*p==0xf3) p++; /* rep */
  if (*p==0x66) p++; /* word */
  len = p - rip + 2;

  /* moves */
  if (*p >= 0x88 && *p <= 0x8b) {
    int disp32_still_possible = 0;
    switch (p[1] & 0xc0) { /* decode modifier */
    case 0:    /* disp32 possible */
      switch (p[1] & 0x07) { /* decode rm */
      case 5: /* disp32 actually here */
        len += 4;
        break;
      case 4: /* disp32 is still possible in SIB byte */
        disp32_still_possible = 1;
        break;
      }
      break;
    case 0x40: /* have disp8 */
      len++;
      break;
    case 0x80: /* have disp32 */
      len += 4;
      break;
    }
    if ((p[1] & 0xc0) < 0xc0 /* SIB byte possible */
        && (p[1] & 0x07) == 4) { /* SIB actually here */
      len++;
      if (disp32_still_possible && (p[2] & 0x07) == 5) /* disp32 used by SIB */
        len += 4;
    }
    return len;
  }
  /* string moves */
  if (*p >= 0xa4 && *p <= 0xad)
    return p - rip + 1;
  return 0;
}

static int e_vgaemu_fault(sigcontext_t *scp, unsigned page_fault)
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
      u = jitx86_instr_len((unsigned char *)_scp_rip);
      _scp_rip += u;
      if (u==0) {
        e_printf("eVGAEmuFault: unknown instruction, page at 0x%05x now writable\n", page_fault << 12);
        vga_emu_protect_page(page_fault, 2);
/**/	leavedos_main(0x5640);
      }
      return 1;
    }
    else if (memcheck_is_rom(page_fault << PAGE_SHIFT)) {	/* ROM area */
      u = jitx86_instr_len((unsigned char *)_scp_rip);
      _scp_rip += u;
      if (u==0 || (_scp_err&2)==0) {
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
/**/  e_printf("eVGAEmuFault: trying %08x\n",*((int *)_scp_rip));
    /* try CPatch, which should not fail */
    if (Cpatch(scp))
      return 1;
  }

  error("eVGAEmuFault: unimplemented decode instr at %08"PRI_RG": %08x\n",
	_scp_rip, *((int *)_scp_rip));
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
static int e_emu_pagefault(sigcontext_t *scp, int pmode)
{
    if (InCompiledCode) {
	dosaddr_t cr2 = DOSADDR_REL(LINP(_scp_cr2));
	if (e_vgaemu_fault(scp, cr2 >> 12) == 1)
	    return 1;

#ifdef HOST_ARCH_X86
	if (e_handle_pagefault(cr2, _scp_err, scp))
	    return 1;
#endif
	/* use CPatch for LDT page faults, which should not fail */
	if (msdos_ldt_access(cr2) && Cpatch(scp))
	    return 1;
	TheCPU.scp_err = _scp_err;
	/* save eip, eflags, and do a "ret" out of compiled code */
	TheCPU.err = EXCP0E_PAGE;
	_scp_eax = FindPC((unsigned char *)_scp_rip);
	e_printf("FindPC: found %x\n",_scp_eax);
	_scp_edx = *(long *)_scp_rsp; // flags
	_scp_rsp += sizeof(long);
	TheCPU.cr2 = cr2;
	_scp_rip = *(long *)_scp_rsp;
	_scp_rsp += sizeof(long);
	return 1;
    }
    return 0;
}

int e_emu_fault(sigcontext_t *scp, int in_vm86)
{
    /* Possibilities:
     * 1. Compiled code touches VGA prot
     * 2. Compiled code touches cpuemu prot
     * 3. Compiled code touches DPMI prot
     * 4. reserved, was "fullsim code touches DPMI prot", but fullsim
     *    no longer faults since commit 70ca014459
     * 5. dosemu code touches cpuemu prot (bug)
     * Compiled code means dpmi-jit, otherwise vm86 not here.
     */
    if (_scp_trapno == 0x0e) {
      /* cases 1, 2, 3 */
      if ((in_vm86 || EMU_DPMI()) && e_emu_pagefault(scp, !in_vm86))
        return 1;
      /* case 5, any jit, bug */
      if (e_handle_pagefault(DOSADDR_REL(LINP(_scp_cr2)), _scp_err, scp)) {
        dosemu_error("touched jit-protected page%s\n",
                     in_vm86 ? " in vm86-emu" : "");
        return 1;
      }
    } else if ((in_vm86 || EMU_DPMI()) && e_handle_fault(scp)) {
      /* compiled code can cause fault (usually DE, Divide Exception) */
      return 1;
    }
    return 0;
}

/* ======================================================================= */
