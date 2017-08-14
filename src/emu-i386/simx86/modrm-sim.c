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

#include "emu86.h"
#include "codegen-sim.h"

/*
 * Why this one? It is only a duplicate of modrm-gen but it interpretes
 * addresses instead of compiling them. After some headaches I decided
 * that it was better to have a duplicate than to parse,compile,execute,
 * find the node,delete it etc.
 *
 */

/////////////////////////////////////////////////////////////////////////////

int ModRMSim(unsigned int PC, int mode)
{
	int l;
	void (*AddrGen_save)(int op, int mode, ...);

	AddrGen_save = AddrGen;
	AddrGen = AddrGen_sim;
	l = _ModRM(0, PC, mode);
	AddrGen = AddrGen_save;
	TheCPU.mem_ref = AR1.d - TheCPU.mem_base;
	return l;
}

