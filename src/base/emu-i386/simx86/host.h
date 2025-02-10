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

#ifndef _EMU86_HOST_H
#define _EMU86_HOST_H

#include "dos2linux.h"
#define read_byte(x) do_read_byte((x), emu_pagefault_handler)
#define read_word(x) do_read_word((x), emu_pagefault_handler)
#define read_dword(x) do_read_dword((x), emu_pagefault_handler)
#define read_qword(x) do_read_qword((x), emu_pagefault_handler)
#define write_byte(x,y) do_write_byte((x), (y), emu_pagefault_handler)
#define write_word(x,y) do_write_word((x), (y), emu_pagefault_handler)
#define write_dword(x,y) do_write_dword((x), (y), emu_pagefault_handler)
#define write_qword(x,y) do_write_qword((x), (y), emu_pagefault_handler)

/////////////////////////////////////////////////////////////////////////////

#define Fetch(a)	read_byte(a)
#define FetchW(a)	read_word(a)
#define FetchL(a)	read_dword(a)
#define DataFetchWL_U(m,a) ((m)&DATA16? FetchW(a):FetchL(a))
#define DataFetchWL_S(m,a) ((m)&DATA16? (short)FetchW(a):(int)FetchL(a))
#define AddrFetchWL_U(m,a) ((m)&ADDR16? FetchW(a):FetchL(a))
#define AddrFetchWL_S(m,a) ((m)&ADDR16? (short)FetchW(a):(int)FetchL(a))
#define GetDWord(a)	read_word(a)
#define GetDLong(a)	read_dword(a)
#define DataGetWL_U(m,a) ((m)&DATA16? GetDWord(a):GetDLong(a))
#define DataGetWL_S(m,a) ((m)&DATA16? (short)GetDWord(a):(int)GetDLong(a))

#if defined(HOST_ARCH_X86) && !defined(HAVE___FLOAT80)
typedef long double __float80;
#undef __SIZEOF_FLOAT80__
#define __SIZEOF_FLOAT80__ sizeof(__float80)
#define HAVE___FLOAT80 1
#endif

#if !defined(HOST_ARCH_X86) && !defined(HAVE__FLOAT128)
typedef long double _Float128;
#endif

/////////////////////////////////////////////////////////////////////////////

#endif
