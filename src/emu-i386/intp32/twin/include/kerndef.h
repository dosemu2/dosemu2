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


/*************************************************************************
	@(#)kerndef.h	2.30
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

**************************************************************************/
 
#ifndef kerndef__h
#define kerndef__h

#include <stddef.h>

#define	WIN32API

/* macros to help C */
#define	EQUALS	==
#define	NEQ	!=

#ifndef DOSEMU

/* Window field offsets for GetWindowLong() and GetWindowWord() */
#ifndef	TWIN32
#define GWW_HWNDFOCUS   (-36)
#define GWL_DRVDATA	(-44)
#define	GWW_HMENU	(-48)
#define	GWW_HWNDMENU	(-52)
#define	GWW_HDC		(-56)
#define GWL_UPDATE	(-60)
#define	GWW_HSYSMENU	(-64)
#define	GWW_HWNDHZSCROLL (-68)
#define	GWW_HWNDVTSCROLL (-72)
#else
#define GWL_HWNDFOCUS   (-36)
#define GWL_DRVDATA	(-44)
#define	GWL_HMENU	(-48)
#define	GWL_HWNDMENU	(-52)
#define	GWL_HDC		(-56)
#define GWL_UPDATE	(-60)
#define	GWL_HSYSMENU	(-64)
#define	GWL_HWNDHZSCROLL (-68)
#define	GWL_HWNDVTSCROLL (-72)
#endif

/* macros to access these fields (see windowsx.h) */
#ifndef	TWIN32
#define	GetDialogFocus(hDlg)	((HWND)GetWindowWord(hDlg,GWW_HWNDFOCUS))
#define	SetDialogFocus(hDlg,hWnd) ((HWND)SetWindowWord(hDlg,GWW_HWNDFOCUS,(WORD)hWnd))
#define	GetWindowMenu(hWnd)	((HMENU)GetWindowWord(hWnd,GWW_HMENU))
#define	SetWindowMenu(hWnd,hMenu) ((HMENU)SetWindowWord(hWnd,GWW_HMENU,(WORD)hMenu))
#define	GetWindowFrame(hWnd)	((HWND)GetWindowWord(hWnd,GWW_HWNDMENU))
#define	GetOwnDC(hWnd)		((HDC)GetWindowWord(hWnd,GWW_HDC))
#define	GetWindowSysMenu(hWnd)	((HMENU)GetWindowWord(hWnd,GWW_HSYSMENU))
#define	SetWindowSysMenu(hWnd,hSysMenu) ((HMENU)SetWindowWord(hWnd,GWW_HSYSMENU,(WORD)hSysMenu))
#define	GetWindowHScroll(hWnd)	((HWND)GetWindowWord(hWnd,GWW_HWNDHZSCROLL))
#define	GetWindowVScroll(hWnd)	((HWND)GetWindowWord(hWnd,GWW_HWNDVTSCROLL))
#else
#define	GetDialogFocus(hDlg)	((HWND)GetWindowLong(hDlg,GWL_HWNDFOCUS))
#define	SetDialogFocus(hDlg,hWnd) ((HWND)SetWindowLong(hDlg,GWL_HWNDFOCUS,(LONG)hWnd))
#define	GetWindowMenu(hWnd)	((HMENU)GetWindowLong(hWnd,GWL_HMENU))
#define	SetWindowMenu(hWnd,hMenu) ((HMENU)SetWindowLong(hWnd,GWL_HMENU,(LONG)hMenu))
#define	GetWindowFrame(hWnd)	((HWND)GetWindowLong(hWnd,GWL_HWNDMENU))
#define	GetOwnDC(hWnd)		((HDC)GetWindowLong(hWnd,GWL_HDC))
#define	GetWindowSysMenu(hWnd)	((HMENU)GetWindowLong(hWnd,GWL_HSYSMENU))
#define	SetWindowSysMenu(hWnd,hSysMenu) ((HMENU)SetWindowLong(hWnd,GWL_HSYSMENU,(LONG)hSysMenu))
#define	GetWindowHScroll(hWnd)	((HWND)GetWindowLong(hWnd,GWL_HWNDHZSCROLL))
#define	GetWindowVScroll(hWnd)	((HWND)GetWindowLong(hWnd,GWL_HWNDVTSCROLL))
#endif

/* our frame class name */
#define	TWIN_FRAMECLASS	"TWINFrame"
/* our system scrollbar class name */
#define	TWIN_SYSSCROLLCLASS "TWINSysScroll"
/* MSWin dialog class name */
#define	TWIN_DIALOGCLASS "#32770"

/* Class field offsets for GetClassLong() and GetClassWord() */
#ifndef	TWIN32
#define	GCW_HDC		(-40)
#define	GCL_BINTONAT	(-44)
#define	GCL_NATTOBIN	(-48)
#else
#define	GCL_HDC		(-40)
#define	GCL_BINTONAT	(-44)
#define	GCL_NATTOBIN	(-48)
#endif

#ifndef	TWIN32
#define	GetClassDC(hWnd)	((HDC)GetClassWord(hWnd,GCW_HDC))
#define	SetClassDC(hWnd,hDC)	((HDC)SetClassWord(hWnd,GCW_HDC,(WORD)hDC))
#else
#define	GetClassDC(hWnd)	((HDC)GetClassLong(hWnd,GCL_HDC))
#define	SetClassDC(hWnd,hDC)	((HDC)SetClassLong(hWnd,GCL_HDC,(LONG)hDC))
#endif

/* Private class style bit */
#define	CS_SYSTEMGLOBAL	0x8000

/* indexes in the atmGlobalLookup table to compare class names */
#define	LOOKUP_ROOTWCLASS	0
#define	LOOKUP_BUTTON		1
#define	LOOKUP_COMBOBOX		2
#define	LOOKUP_EDIT		3
#define	LOOKUP_LISTBOX		4
#define	LOOKUP_COMBOLBOX	5
#define	LOOKUP_MDICLIENT	6
#define	LOOKUP_SCROLLBAR	7
#define	LOOKUP_STATIC		8
#define	LOOKUP_FRAME		9
#define	LOOKUP_DIALOGCLASS	10
#define	LOOKUP_TRACKPOPUP	11
#define	LOOKUP_ICONTITLE	12
#define	LOOKUP_MENULBOX		13
#define	LOOKUP_SYSSCROLL	14

DECLARE_HANDLE32(HCLASS32);

WORD SetClassHandleWord(HCLASS32,int,WORD);
WORD GetClassHandleWord(HCLASS32,int);
LONG SetClassHandleLong(HCLASS32,int,LONG);
LONG GetClassHandleLong(HCLASS32,int);

#ifdef	STRICT
typedef BOOL (CALLBACK *CLASSENUMPROC)(HCLASS32, LPWNDCLASS, LPARAM);
#else
typedef FARPROC CLASSENUMPROC;
#endif

BOOL EnumClasses(UINT, CLASSENUMPROC, LPARAM);

#define	ECF_SYSGLOBAL	0x8000
#define	ECF_APPGLOBAL	0x0002
#define	ECF_APPLOCAL	0x0001
#define	ECF_ALLCLASSES	ECF_SYSGLOBAL|ECF_APPGLOBAL|ECF_APPLOCAL

/* Undocumented messages */
#define WM_PAINTICON		0x0026
#define WM_ISACTIVEICON		0x0035
#define	WM_SYNCPAINT		0x0088
#define WM_SYSTIMER		0x0118

/* undocumented functions */
DWORD InquireSystem(WORD,WORD,BOOL);
WORD Get80x87SaveSize(void);

WORD    InitApp(HINSTANCE);
WORD    WaitEvent(HANDLE);
DWORD	SetDCOrg(HDC,int,int);
BOOL	FastWindowFrame(HDC, LPRECT, int, int, DWORD);
UINT WINAPI SetSystemTimer(HWND,UINT,UINT,TIMERPROC);
BOOL WINAPI UserKillSystemTimer(HWND,UINT);

int SelectVisRgn(HDC, HRGN);
HRGN InquireVisRgn(HDC);
int IntersectVisRect(HDC, int, int, int, int);
int OffsetVisRgn(HDC, int, int);

/* Private window extended style bit */
#define	WS_EX_SAVEBITS	0x80000000
#define	WS_EX_NOCAPTURE	0x40000000
#define WS_EX_POPUPMENU 0x20000000

/* Private listbox style bit */
#define	LBS_COMBOLBOX	0x8000
#define	LBS_PRELOADED	0x4000

#define BS_PUSHBOX      0x0aL

#if (WINVER >= 0x030a)
#define COLOR_ENDCOLORS COLOR_BTNHIGHLIGHT
#else
#define COLOR_ENDCOLORS COLOR_BTNTEXT
#endif

/* Additional messages to the LISTBOX system class */
#define	LB_GETINDENTS		(WM_USER+40)
#define	LB_SETINDENTS		(WM_USER+41)
#define	LB_GETITEMINDENTS	(WM_USER+42)
#define	LB_SETITEMINDENTS	(WM_USER+43)
#define	LB_GETITEMBITMAPS	(WM_USER+44)
#define	LB_SETITEMBITMAPS	(WM_USER+45)
#define	LB_ADDITEM		(WM_USER+46)

/* Additional style bit to the STATIC system class */
#define	SS_VCENTER	0x40

/* Additional style to the SCROLLBAR class */
#define	SBS_SYSTEM	0x0080

/* additional action bit for InternalInvalidateWindows/InternalPaintWindows */
#define	RDW_PAINT	0x1000
#define	RDW_NOPAINT	0x2000

/* Messages to the SCROLLBAR system class */
#define	SBM_GETSCROLLPOS	(WM_USER+0)
#define	SBM_SETSCROLLPOS	(WM_USER+1)
#define	SBM_GETSCROLLRANGE	(WM_USER+2)
#define	SBM_SETSCROLLRANGE	(WM_USER+3)

/* Private GetWindow functions */
#define	GW_HWNDNEXTSIB		GW_HWNDNEXT
#define	GW_HWNDPREVSIB		GW_HWNDPREV
#define	GW_HWNDNEXTTREE		0x0010
#define	GW_HWNDPREVTREE		0x0011
#define	GW_HWNDNEXTGROUP	0x0012
#define	GW_HWNDPREVGROUP	0x0013

/* Private InternalFindWindow functions */
#define	IFW_NAME	0x0000
#define	IFW_PRIVATE	0x0001

/* Private InternalFocus functions */
#define	IFC_GETFOCUS	0x0000
#define	IFC_SETFOCUS	0x0001
#define	IFC_DRIVERFOCUS	0x0002

/* Additional stock objects */
#define	SYSTEM_BITMAP		SYSTEM_FIXED_FONT+3

/* Internal window flags */
/* These are the state flags */

#define WFTRACKPOPUPACTIVE	0x80000000L
#define WFRESTOREMAXIMIZED      0x40000000L
#define WFCHILDMAXIMIZED        0x20000000L
#define WFBUTTONDOWN		0x10000000L
#define WFDRAGGING		0x08000000L
#define	WFUPDATELOCKED		0x04000000L	/* LockUpdateWindow */

#define WFFRAMEON               0x00800000L
#define WFNONCPAINT             0x00400000L
#define	WFBKERASE		0x00200000L
#define	WFNCDIRTY		0x00100000L
#define	WFDIRTY			0x00080000L
#define	WFCHILDDIRTY		0x00040000L
#define WFINPAINT		0x00020000L

/* These are the X-related state flags */
#define	WFMAPPINGPENDING	0x00004000L
#define	WFMAPPED		0x00002000L
#define	WFVISIBILITY		0x00000800L

/* These are the "style-shortcuts" */
#define WFHIDDENPOPUP           0x00000200L

/* This flag says that window has been created by the dialog manager */
#define	WFDIALOGWINDOW		0x00000080L

/* This flag says that this window uses DefMDIClientProc */
#define	WFMDICLIENT		0x00000040L
/* This flag says that this window uses DefMDIChildProc */
#define	WFMDICHILD		0x00000020L
/* This flag says that this window is dead and should not receive messages */
#define WFDEAD			0x00000010L

/* Message queue status flags */
#define	QFPAINT			0x00000001L
#define	QFTIMER			0x00000002L
#define	QFINSEND		0x00000004L

/* QueueFlags functions */
#define QF_SET		1
#define QF_CLR		2
#define QF_TST		4

/* Function codes for Set/Get*Word/Long functions */
#define WND_SET		1
#define WND_GET		0
#define WND_OR		2
#define WND_AND		3
#define WND_XOR		4
#define WND_TEST	5

/* ComparePrivateData return codes */
#define	MATCH_NULL	0
#define	MATCH_CLIENT	1
#define	MATCH_FRAME	2
#define	MATCH_HSCROLL	3
#define	MATCH_VSCROLL	4

/************************************************************************/
/*		additional constant for windows.h 			*/
/************************************************************************/
#define SWP_DRIVER 	0x8000

/*************** keyboard messages **************************************/

/* context flags */
#define ALTKEY		1
#define SHIFTKEY	2
#define CONTROLKEY	4

LPSTR CoreMalloc(unsigned int);
void CoreFree(LPSTR);
LPSTR CoreRealloc(LPSTR, unsigned int);

/* Microsoft C library functions */
void _splitpath(LPCSTR,LPSTR,LPSTR,LPSTR,LPSTR);
void _makepath(LPSTR,LPCSTR,LPCSTR,LPCSTR,LPCSTR);
char * _fullpath(LPSTR,LPCSTR,size_t);

LRESULT DefROOTProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefBUTTONProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefEDITProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefCOMBOBOXProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefLISTBOXProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefCOMBOLBOXProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefMDICLIENTProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefSCROLLBARProc(HWND, UINT, WPARAM, LPARAM);
LRESULT DefSTATICProc(HWND, UINT, WPARAM, LPARAM);
LRESULT MenuBarProc(HWND, UINT, WPARAM, LPARAM);
LRESULT FrameProc(HWND, UINT, WPARAM, LPARAM);

/* 32-bit internal menu handle */
DECLARE_HANDLE32(HMENU32);
HGLOBAL GlobalHandle32(LPCVOID);

#endif	/* DOSEMU */

/* generic function pointer for conversion routines */
typedef DWORD (* LONGPROC)();
typedef UINT (CALLBACK *LPFNHOOK)(HWND,UINT,WPARAM,LPARAM); 

/* private message for the binary world */
#define WM_CONVERT	(UINT)-1

typedef DWORD BINADDR;

#define INT_86      2
#define UINT_86     2
#define WORD_86     2
#define HANDLE_86   2
#define DWORD_86    4
#define LONG_86     4
#define LP_86       4
#define RET_86	    4

#endif /* kerndef__h */

