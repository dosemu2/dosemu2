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
	@(#)BinTypes.h	2.11
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
 
#ifndef BinTypes__h
#define BinTypes__h

#include <setjmp.h>

#include "kerndef.h"

typedef struct tagSHADOWSELTAB {
	HGLOBAL handle;
	WORD	wIndex;
} SHADOWSELTAB;

#define	CW_USEDEFAULT16	0x8000

typedef DWORD REGISTER;

typedef struct tagREGISTERS
{
	REGISTER ds;
	REGISTER es;
	REGISTER ss;
	REGISTER flags;
	REGISTER ax;
	REGISTER bx;
	REGISTER cx;
	REGISTER dx;
	REGISTER si;
	REGISTER di;
	REGISTER bp;
	REGISTER sp;
	REGISTER cs;
	REGISTER fs;
	REGISTER gs;
} REGISTERS;
typedef REGISTERS *LPREG;

typedef struct tagENV
{
	REGISTERS	reg;
	BINADDR	trans_addr;
	BINADDR	return_addr;
	LPVOID	machine_stack;
	struct  tagENV *prev_env;
	unsigned long active;
	unsigned long is_catch;
	unsigned long level;
	struct HSW_86_CATCHBUF *buf;
	jmp_buf		jump_buffer;
} ENV;

typedef struct tagINITT
  {
    HINSTANCE	hInstance;
    HINSTANCE	hPrevInstance;
    LPSTR	lpCmdLine;
    int		nCmdShow;
    WORD	wStackTop;
    WORD	wStackBottom;
    WORD	wHeapSize;
  } INITT;
typedef INITT *LPINITT;

#ifndef DOSEMU
WORD    InitApp(HINSTANCE);
WORD    WaitEvent(HANDLE);

LRESULT	hsw_common_nat_to_bin(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_common_bin_to_nat(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_listbox_nat_to_bin(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_listbox_bin_to_nat(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_combobox_nat_to_bin(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_combobox_bin_to_nat(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_edit_nat_to_bin(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_edit_bin_to_nat(HWND,UINT,WPARAM,LPARAM);
LRESULT	hsw_commdlg_nat_to_bin(HWND,UINT,WPARAM,LPARAM);
void	hsw_getmessage_nat_to_bin(LPBYTE,LPMSG);
void	hsw_encapsulate_nat_to_bin(LPBYTE,LPMSG);

BOOL CALLBACK	hsw_enumprops(HWND,LPCSTR,HANDLE);
void CALLBACK	hsw_timerproc(HWND,UINT,UINT,DWORD);
void CALLBACK	hsw_mmtimer(UINT,UINT,DWORD,DWORD,DWORD);
BOOL CALLBACK	hsw_wndenumproc(HWND,LPARAM);
BOOL CALLBACK	hsw_graystringproc(HDC,LPARAM,int);
int CALLBACK	hsw_fontenumproc(LOGFONT *,NEWTEXTMETRIC *,int,LPARAM);
BOOL CALLBACK   hsw_abortproc(HDC, int);
void CALLBACK	hsw_lineddaproc(int,int,LPARAM);
int CALLBACK	hsw_enumbrushproc(LOGBRUSH *,LPARAM);
int CALLBACK	hsw_enumpenproc(LOGPEN *,LPARAM);
int CALLBACK	hsw_mfenumproc(HDC,HANDLETABLE *,METARECORD *,int,LPARAM);

DWORD CALLBACK	hsw_hookproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_msgfilterproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_keyboardproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_mouseproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_journrecproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_journplbproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_callwndproc(int,WPARAM,LPARAM);
DWORD CALLBACK	hsw_cbtproc(int,WPARAM,LPARAM);

DWORD CALLBACK  hsw_oleserverrelease(LPARAM);

#undef  LocalAlloc
#undef  LocalLock
#undef  LocalReAlloc
#undef  LocalUnlock
#undef  LocalFree
#undef  LocalSize
#endif	/* DOSEMU */

#define SP ((LPBYTE)(envp->reg.sp))
#define BP ((LPBYTE)(envp->reg.bp))

#ifndef DOSEMU
#define RECT_TO_86(lp,r) { PUTWORD(lp,(WORD)r.left); \
	PUTWORD(lp+2,(WORD)r.top); \
	PUTWORD(lp+4,(WORD)r.right); \
	PUTWORD(lp+6,(WORD)r.bottom); }

#define RECT_TO_C(r,lp) { r.left = (int)((short)GETWORD(lp)); \
	r.top = (int)((short)GETWORD(lp+2)); \
	r.right = (int)((short)GETWORD(lp+4)); \
	r.bottom = (int)((short)GETWORD(lp+6)); }

#define GetPOINT(p,lp) { p.x = (int)((short)GETWORD(lp)); \
	p.y = (int)((short)GETWORD(lp+2)); }

#define PutPOINT(lp,p) { PUTWORD(lp,(WORD)p.x); \
	PUTWORD(lp+2,(WORD)p.y); }

#define wMustBeZero 0x0000
#define dwOldSSSP   0x0002
#define pLocalHeap  0x0006
#define pAtomTable  0x0008
#define pStackTop   0x000a
#define pStackMin   0x000c
#define pStackBottom    0x000e

#define BITMAPINFOHEADER_SIZE_86  40
#endif	/* DOSEMU */

#endif /* BinTypes__h */
