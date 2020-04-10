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
    if (!vga.inst_emu) {
      /* Normal: make the display page writeable after marking it dirty */
      dosemu_error("simx86: should not be here\n");
      return False;
    }

/**/  e_printf("eVGAEmuFault: trying %08x, a=%08"PRI_RG"\n",*((int *)_rip),_rdi);

    /* try CPatch, which should not fail */
    if (Cpatch(scp))
      return 1;
  }

  error("eVGAEmuFault: unimplemented decode instr at %08"PRI_RG": %08x\n",
	_rip, *((int *)_rip));
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
    if (InCompiledCode) {
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
	/* use CPatch for LDT page faults, which should not fail */
	if (msdos_ldt_access((unsigned char *)_cr2) && Cpatch(scp))
	    return 1;
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
