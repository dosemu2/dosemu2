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
	@(#)DPMI.h	2.10
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

*/
 
#ifndef DPMI__h
#define DPMI__h

#include "Segment.h"
#ifdef DOSEMU
BOOL LoadSegment(UINT);
#else
#include "LoadDLL.h"
#endif	/* DOSEMU */

LPSTR GetAddress(WORD,WORD);
WORD AssignSelector(LPBYTE,WORD,BYTE,DWORD);
WORD AssignSelRange(int);
UINT TWIN_ReallocSelector(UINT,DWORD,UINT);
WORD MakeSegment(DWORD,WORD);
int GetAHSHIFT(void);
int GetAHINCR(void);

#define ASSIGNSEL(lp, sz)	AssignSelector((LPBYTE)lp,0,TRANSFER_DATA,sz)

/* Macros to access fields in LDT table		*/

#ifdef DOSEMU
#define GetSelectorAddress(w)	(vm86f? (LPBYTE)(w<<4):((LDT[w>>3].w86Flags & DF_PRESENT)? \
				LDT[w>>3].lpSelBase:(LoadSegment(w)? \
				LDT[w>>3].lpSelBase:0)))

#define SetPhysicalAddress(w,l) { if (!vm86f) LDT[w>>3].lpSelBase = (LPBYTE)l; }

#define	GetPhysicalAddress(w)	(vm86f? (LPBYTE)(w<<4):LDT[w>>3].lpSelBase)

#define GetSelectorLimit(w)	(vm86f? 0xffff:LDT[w>>3].dwSelLimit)
#define SetSelectorLimit(w,d)	{ if (!vm86f) LDT[w>>3].dwSelLimit = (DWORD)d; }

#define GetSelectorHandle(w)	(vm86f? 1:LDT[w>>3].hGlobal)
#define SetSelectorHandle(w,h)	{ if (!vm86f) LDT[w>>3].hGlobal = (HGLOBAL)h;}

#define GetSelectorFlags(w)	(vm86f? 0x00f0:LDT[w>>3].w86Flags)
#define SetSelectorFlags(w,wf)	{ if (!vm86f) LDT[w>>3].w86Flags = (WORD)wf; }

#define GetSelectorType(w)	(vm86f? 0:LDT[w>>3].bSelType)
#define SetSelectorType(w,bt)	{ if (!vm86f) LDT[w>>3].w86Flags = Sel86Flags[bt], \
				  LDT[w>>3].bSelType = (BYTE)bt; }

#define GetModuleIndex(w)	(vm86f? 0:LDT[w>>3].bModIndex)
#define SetModuleIndex(w,bi)	{ if (!vm86f) LDT[w>>3].bModIndex = (BYTE)bi; }

#else	/* DOSEMU */

#define GetSelectorAddress(w)	((LDT[w>>3].w86Flags & DF_PRESENT)? \
				LDT[w>>3].lpSelBase:(LoadSegment(w)? \
				LDT[w>>3].lpSelBase:0))

#define SetPhysicalAddress(w,l) { LDT[w>>3].lpSelBase = (LPBYTE)l; }

#define	GetPhysicalAddress(w)	LDT[w>>3].lpSelBase

#define GetSelectorLimit(w)	LDT[w>>3].dwSelLimit
#define SetSelectorLimit(w,d)	{ LDT[w>>3].dwSelLimit = (DWORD)d; }

#define GetSelectorHandle(w)	LDT[w>>3].hGlobal
#define SetSelectorHandle(w,h)	{ LDT[w>>3].hGlobal = (HGLOBAL)h;}

#define GetSelectorFlags(w)	LDT[w>>3].w86Flags
#define SetSelectorFlags(w,wf)	{ LDT[w>>3].w86Flags = (WORD)wf; }

#define GetSelectorType(w)	LDT[w>>3].bSelType
#define SetSelectorType(w,bt)	{ LDT[w>>3].w86Flags = Sel86Flags[bt]; \
				  LDT[w>>3].bSelType = (BYTE)bt; }

#define GetModuleIndex(w)	LDT[w>>3].bModIndex
#define SetModuleIndex(w,bi)	{ LDT[w>>3].bModIndex = (BYTE)bi; }
#endif	/* DOSEMU */

#define	CopySelector(w1,w2)	memcpy((LPSTR)&LDT[w1>>3], \
					(LPSTR)&LDT[w2>>3], \
					sizeof(DSCR));

BOOL DPMI_Notify(UINT,WORD);

/* Messages for DPMI_Notify() */
#define DN_ASSIGN	1
#define DN_FREE		2
#define DN_INIT		3
#define DN_MODIFY	4

/* descriptor byte 6 */
#define	DF_PAGES	0x8000
#define	DF_32		0x4000

/* descriptor byte 5 */
#define	DF_PRESENT	0x80
#define	DF_DPL		0x60
#define	DF_USER		0x10
#define	DF_CODE		0x08
#define	DF_DATA		0x00
#define	DF_EXPANDDOWN	0x04
#define	DF_CREADABLE	0x02
#define	DF_DWRITEABLE	0x02

/* Segment types */
				/* 0x00F2 */
#define ST_DATA16	DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_DWRITEABLE
				/* 0x00FA */
#define ST_CODE16	DF_PRESENT|DF_DPL|DF_USER|DF_CODE|DF_CREADABLE
				/* 0x00F6 */
#define ST_STACK16	DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_EXPANDDOWN| \
			DF_DWRITEABLE
				/* 0x80F2 */
#define ST_DATA32	DF_32|DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_DWRITEABLE
				/* 0x80FA */
#define ST_CODE32	DF_32|DF_PRESENT|DF_DPL|DF_USER|DF_CODE|DF_CREADABLE
				/* 0x80F6 */
#define ST_STACK32	DF_32|DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_EXPANDDOWN| \
			DF_DWRITEABLE

/* Segment limit -- 64K */
#define SL_DEFAULT	(LPARAM)0xFFFF

typedef struct {
	LPBYTE lpSelBase;	/* unscrambled segment base */
	DWORD  dwSelLimit;	/* unscrambled segment limit */
	HGLOBAL hGlobal;	/* segment has to be a global object */
#ifndef TWIN32
	HANDLE	hReserved;	/* has to go when HGLOBAL becomes 32-bit */
#endif
	WORD   w86Flags;	/* bytes 6 and 5 of the descriptor */
	BYTE   bSelType;	/* TRANSFER_... selector flags */
	BYTE   bModIndex;	/* index into module table */
} DSCR;

extern WORD Sel86Flags[];
extern DSCR *LDT;
extern int nLDTSize;

#endif /* DPMI__h */
