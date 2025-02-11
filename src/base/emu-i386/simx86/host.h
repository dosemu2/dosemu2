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

uint8_t sim_read_byte(dosaddr_t x);
uint16_t sim_read_word(dosaddr_t x);
uint32_t sim_read_dword(dosaddr_t x);
uint64_t sim_read_qword(dosaddr_t x);
void sim_write_byte(dosaddr_t x, uint8_t y);
void sim_write_word(dosaddr_t x, uint16_t y);
void sim_write_dword(dosaddr_t x, uint32_t y);
void sim_write_qword(dosaddr_t x, uint64_t y);

uint8_t jit_fetch_byte(dosaddr_t x);
uint16_t jit_fetch_word(dosaddr_t x);
uint32_t jit_fetch_dword(dosaddr_t x);

/////////////////////////////////////////////////////////////////////////////

extern uint8_t (*Fetch)(dosaddr_t a);
extern uint16_t (*FetchW)(dosaddr_t a);
extern uint32_t (*FetchL)(dosaddr_t a);
#define DataFetchWL_U(m,a) ((m)&DATA16? FetchW(a):FetchL(a))
#define DataFetchWL_S(m,a) ((m)&DATA16? (short)FetchW(a):(int)FetchL(a))
#define AddrFetchWL_U(m,a) ((m)&ADDR16? FetchW(a):FetchL(a))
#define AddrFetchWL_S(m,a) ((m)&ADDR16? (short)FetchW(a):(int)FetchL(a))

#define GetDWord(a)	sim_read_word(a)
#define GetDLong(a)	sim_read_dword(a)
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
