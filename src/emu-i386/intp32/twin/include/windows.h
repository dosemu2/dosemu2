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


/*  windows.h -   SDK Structure definitions, and API's.
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

The maintainer of the Willows TWIN Libraries may be reached (Email) 
at the address twin@willows.com	

*/

/**********************************************************************
*
*   Symbol exclusion controls, #define any of the following to 
*    exclude the symbols:
*
*   NOAPIPROTO      Exclude API prototypes, for cross platform compatibility.
*
*   NOMINMAX          min() and max() macros
*
*   NOKERNEL        Kernel definitions and API's
*       NOLOGERROR        LogError() and related definitions
*       NOPROFILER        Profiler APIs
*       NOMEMMGR          Local and global memory management
*       NOLFILEIO         _l* file I/O routines
*       NOOPENFILE        OpenFile and related definitions
*       NORESOURCE        Resource management
*       NOATOM            Atom management
*       NOLANGUAGE        Character test routines
*       NOLSTRING         lstr* string management routines
*       NODBCS            Double-byte character set routines
*       NOKEYBOARDINFO    Keyboard driver routines
*
*   NOGDI           GDI definitions and API's
*       NOGDICAPMASKS     GDI device capability constants
*       NOCOLOR           COLOR_* color values
*       NOGDIOBJ          GDI pens, brushes, fonts
*       NODRAWTEXT        DrawText() and related definitions
*       NOTEXTMETRIC      TEXTMETRIC and related APIs
*       NOSCALABLEFONT    Truetype scalable font support
*       NOBITMAP          Bitmap support
*       NORASTEROPS       GDI Raster operation definitions
*       NOMETAFILE        Metafile support
*
*   NOUSER          USER definitions and API's
*       NOSOUND         Sound definitions and API's
*       NOCOMM          Comm driver definitions and API's
*       NODRIVERS       Installable driver definitions and API's
*       NOSYSMETRICS      GetSystemMetrics() and related SM_* definitions
*       NOSYSTEMPARAMSINFO SystemParametersInfo() and SPI_* definitions
*       NOMSG             APIs and definitions that use MSG structure
*       NOWINSTYLES       Window style definitions
*       NOWINOFFSETS      Get/SetWindowWord/Long offset definitions
*       NOSHOWWINDOW      ShowWindow and related definitions
*       NODEFERWINDOWPOS  DeferWindowPos and related definitions
*       NOVIRTUALKEYCODES VK_* virtual key codes
*       NOKEYSTATES       MK_* message key state flags
*       NOWH              SetWindowsHook and related WH_* definitions
*       NOMENUS           Menu APIs
*       NOSCROLL          Scrolling APIs and scroll bar control
*       NOCLIPBOARD       Clipboard APIs and definitions
*       NOICONS           IDI_* icon IDs
*       NOMB              MessageBox and related definitions
*       NOSYSCOMMANDS     WM_SYSCOMMAND SC_* definitions
*       NOMDI             MDI support
*       NOCTLMGR          Control management and controls
*       NOWINMESSAGES     WM_* window messages
*       NOHELP            Help support
*
*   NOCOMMDLG       COMMDLG definitions and API's
*
********************************************************************/

#ifndef windows__h
#define windows__h
#define __WINDOWS_H   /* OWL tests for this extensively */

/*
 *  Get the platform-specific information first, in case it 
 *  hasn't been included by this point.
 */
#include "platform.h"	/* platform specific */

#ifdef __cplusplus
extern "C" {    /* Force C declarations for C++ */
#endif          /* __cplusplus */

#ifdef WIN32
#ifndef TWIN32
#define TWIN32
#endif
#endif /* WIN32 */

#ifdef RC_INVOKED
/* Don't include definitions that RC.EXE can't parse */
#define NOATOM
#define NOGDI
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSOUND
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NODBCS
#define NOSYSTEMPARAMSINFO
#define NOCOMM
#define NOOEMRESOURCE
#define NOCONSOLE
#endif  /* RC_INVOKED */



/* WinPre definitions */
#define _huge
#define _far
#define __far
#define far
#define _pascal
#define _export
#define __export
#define pascal
#define _near
#define near
#define _cdecl
#define __cdecl
#define cdecl
#define	__stdcall
#define _declspec(impexp)
#define __declspec(impexp)


/* Common definitions *****************************************/
#define VOID        void
#define CONST       const

#ifndef FAR
#define FAR         _far
#endif
#ifndef NEAR
#define NEAR        _near
#endif
#ifndef PASCAL
#define PASCAL      _pascal
#endif
#ifndef CDECL
#define CDECL       _cdecl
#endif

#define WINAPI      _far _pascal    /* use this tag for APIs defined by MS */
#define TWINAPI     _far _pascal    /* use this tag for our public extensions */
#define CALLBACK    _far _pascal
#define	APIENTRY
#ifdef	TWIN32
#define	UNREFERENCED_PARAMETER(x)
#endif

#ifndef FALSE
#define FALSE       0
#define TRUE        1
#endif

#ifndef TWIN_BOOL_DEFINED
typedef int		BOOL;
#endif	/* TWIN_BOOL_DEFINED */

#ifndef TWIN_SHORT_DEFINED
typedef short int	SHORT;
#endif	/* TWIN_SHORT_DEFINED */

#ifndef TWIN_USHORT_DEFINED
typedef unsigned short	USHORT;
#endif	/* TWIN_USHORT_DEFINED */

#ifndef TWIN_INT_DEFINED
typedef int		INT;
#endif	/* TWIN_INT_DEFINED */

#ifndef TWIN_UINT_DEFINED
typedef unsigned int	UINT;
#endif	/* TWIN_UINT_DEFINED */

#ifndef TWIN_LONG_DEFINED
#ifdef STRICT
typedef signed long         LONG;
#else
#define LONG                long
#endif
#endif	/* TWIN_LONG_DEFINED */

#ifndef TWIN_ULONG_DEFINED
typedef unsigned long	ULONG;
#endif	/* TWIN_ULONG_DEFINED */

typedef unsigned char	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;

typedef	char		CHAR;
#ifndef TWIN_UCHAR_DEFINED
typedef unsigned char   UCHAR;
#endif
typedef	char		TCHAR;
typedef float		FLOAT;

typedef UINT		*PUINT;
typedef UINT NEAR	*NPUINT;
typedef UINT FAR	*LPUINT;

typedef FLOAT		*PFLOAT;
typedef FLOAT NEAR	*NPFLOAT;
typedef FLOAT FAR	*LPFLOAT;

#ifndef DOSEMU
typedef unsigned char 	boolean;
#endif
typedef unsigned char 	byte;

#ifdef  TWIN32
typedef unsigned int        WLS_UINT;
#else
typedef unsigned short      WLS_UINT;
#endif

/* Types use for passing & returning polymorphic values */
typedef WLS_UINT WPARAM;
typedef LONG LPARAM;
typedef LONG LRESULT;


/* Usefull macros ********************************************/
#define LOBYTE(w)       ((BYTE)(w))
#define HIBYTE(w)       ((BYTE)(((UINT)(w) >> 8) & 0xFF))

/* #define LOWORD(l)       ((WORD)(DWORD)(l)) */
#define LOWORD(l)       ((WORD)((DWORD)(l) & 0x0FFFF))
#define HIWORD(l)       ((WORD)((((DWORD)(l)) >> 16) & 0xFFFF))

#define MAKELONG(low, high) ((LONG)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
#define MAKEWORD(low, high) ((WORD)(((BYTE)(low)) | (((WORD)((BYTE)(high))) << 8)))

#ifndef NOMINMAX
#ifndef max
#define max(a,b)        (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)        (((a) < (b)) ? (a) : (b))
#endif
#endif  /* NOMINMAX */

#define MAKELPARAM(low, high)   ((LPARAM)MAKELONG(low, high))
#define MAKELRESULT(low, high)  ((LRESULT)MAKELONG(low, high))

#ifdef TWIN32
#define MAKEWPARAM(low, high)   ((WPARAM)MAKELONG(low, high))
#endif /* TWIN32 */

/* Pointer definitions******************************************/
#ifndef NULL
#define NULL            0
#endif

typedef char NEAR*          PSTR;
typedef char NEAR*          NPSTR;

typedef char FAR*           LPSTR;
typedef const char FAR*     LPCSTR;
typedef char FAR*           LPTSTR;
typedef const char FAR*     LPCTSTR;

typedef BYTE NEAR*          PBYTE;
typedef BYTE FAR*           LPBYTE;

typedef int NEAR*           PINT;
typedef int FAR*            LPINT;

typedef WORD NEAR*          PWORD;
typedef WORD FAR*           LPWORD;

typedef long NEAR*          PLONG;
typedef long FAR*           LPLONG;

typedef DWORD NEAR*         PDWORD;
typedef DWORD FAR*          LPDWORD;

typedef void NEAR*          PVOID;
typedef void FAR*           LPVOID;

typedef const void NEAR*    PCVOID;
typedef const void FAR*     LPCVOID;

typedef BOOL NEAR*          PBOOL;
typedef BOOL FAR*           LPBOOL;

/* Pointer macros */
#define MAKELP(sel, off)    ((void FAR*)MAKELONG((off), (sel)))
#define SELECTOROF(lp)      HIWORD(lp)
#define OFFSETOF(lp)        LOWORD(lp)

#define FIELDOFFSET(type, field)    ((int)(&((type NEAR*)1)->field)-1)


/* Handle definitions ******************************************/
/*
 *  For WIN16, we cannot use the STRICT flag to enable unique
 *  handle types, since for us the underlying pointers are 32-bit 
 *  pointers, not 16-bit pointers.
 */
#if defined(STRICT) && defined(TWIN32)
typedef void NEAR*        HANDLE;
#define DECLARE_HANDLE(name)    struct name##__ { int unused; }; \
                                typedef struct name##__ NEAR* name
#define DECLARE_HANDLE32(name)  struct name##__ { int unused; }; \
                                typedef struct name##__ FAR* name
#else
typedef WLS_UINT                    HANDLE;
#define DECLARE_HANDLE(name)    typedef WLS_UINT name
#define DECLARE_HANDLE32(name)  typedef DWORD name
#endif

typedef HANDLE*         PHANDLE;
typedef HANDLE NEAR*    SPHANDLE;
typedef HANDLE FAR*     LPHANDLE;

typedef HANDLE          HGLOBAL;
typedef HANDLE          HLOCAL;

typedef HANDLE          GLOBALHANDLE;
typedef HANDLE          LOCALHANDLE;

typedef WORD            ATOM;

#ifdef STRICT
typedef void (CALLBACK*     FARPROC)(void);
typedef void (NEAR PASCAL*  NEARPROC)(void);
#else
typedef int (CALLBACK*      FARPROC)();
typedef int (NEAR PASCAL*   NEARPROC)();
#endif
typedef int (WINAPI *PROC)();

/* Core handle declarations */
DECLARE_HANDLE(HSTR);
DECLARE_HANDLE(HTASK);
DECLARE_HANDLE(HRSRC);

DECLARE_HANDLE(HINSTANCE);
typedef HINSTANCE HMODULE;  /* HMODULEs can be used in place of HINSTANCEs */

DECLARE_HANDLE(HDC);

DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HPEN);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HWND);


/* GDI related typedefs */
typedef struct tagRECT
{
    int left;
    int top;
    int right;
    int bottom;
} RECT;
typedef RECT*      PRECT;
typedef RECT NEAR* NPRECT;
typedef RECT FAR*  LPRECT;

typedef struct _RECTL
{
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECTL;
typedef RECTL		*PRECTL;
typedef RECTL NEAR	*NPRECTL;
typedef RECTL FAR	*LPRECTL;

typedef struct tagPOINT
{
    int x;
    int y;
} POINT;
typedef POINT*       PPOINT;
typedef POINT NEAR* NPPOINT;
typedef POINT FAR*  LPPOINT;

/* #define MAKEPOINT(l)      (*((POINT FAR*)&(l))) */
POINT MAKEPOINT(DWORD);

typedef struct tagPOINTS
{
    short x;
    short y;
} POINTS;
typedef POINTS*       PPOINTS;
typedef POINTS NEAR* NPPOINTS;
typedef POINTS FAR*  LPPOINTS;

#define MAKEPOINTS(l)      (*((POINTS FAR*)&(l)))

typedef struct tagSIZE
{
    int cx;
    int cy;
} SIZE;
typedef SIZE*       PSIZE;
typedef SIZE NEAR* NPSIZE;
typedef SIZE FAR*  LPSIZE;

typedef SIZE		SIZEL;
typedef SIZE		*PSIZEL;
typedef SIZE NEAR	*NPSIZEL;
typedef SIZE FAR	*LPSIZEL;

/* System inclusions ******************************************/

/* some WIN32 definitions */
#ifdef	TWIN32
#define	GetCurrentThreadId	GetCurrentTask
#endif

#ifndef _MAX_PATH
#define _MAX_PATH   260
#endif
#ifdef MAX_PATH
#undef MAX_PATH
#endif
#define MAX_PATH	_MAX_PATH
#ifndef _MAX_DRIVE
#define _MAX_DRIVE  3
#endif
#ifndef _MAX_DIR
#define _MAX_DIR    256
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME  256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT    256
#endif

#ifndef DOSEMU
#ifndef RC_INVOKED
#include "Win_NT.h"
#include "Win_Base.h"
#endif

#ifndef NOVERSION
#include "Winver.h"
#endif

#ifndef NOCONSOLE
#include "win_con.h"
#endif

#ifndef NOKERNEL
#include "Win_Kernel.h"
#include "Win_Thread.h"
#endif

#ifndef NOGDI
#include "Win_Gdi.h"
#endif      /* NOGDI */

#ifndef NOUSER
#include "Win_User.h"
#endif      /* NOUSER */

/* Spooler status notification message */
#define WM_SPOOLERSTATUS        0x002A

#include "Win_MS.h"     /* Include any of the MS types we do not use */
#endif	/* DOSEMU */

#ifdef __cplusplus
} /* Leave 'extern "C"' mode when switching header files */
#endif

#ifndef RC_BUILD
#include "Win_Clib.h"	/* standard C library functions and definitions */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOSEMU
#include "dlgs.h"
#endif	/* DOSEMU */

#ifndef RC_INVOKED

#include "Win_Reg.h"

#ifndef DOSEMU
#ifndef NO_WINSOCK
#include "winsock.h"
#endif

#if 1
	/* PROBLEM: commdlg/Commdlg.res.c doesn't #define NOCOMMDLG
	 * and its function prototypes conflict with commdlg.h.
	 */
	/* SOLUTION: rc generates #define NOAPIPROTO before
	 * #include <windows.h>
	 */
#ifndef NO_COMMDLG
#include "commdlg.h"
#endif
#endif

#include "ddeml.h"
#include "mmsystem.h"
#if defined(TWIN32)
#include "print.h"
#endif /* TWIN32 */
#endif /* DOSEMU */
#endif /* RC_INVOKED */

#ifdef __cplusplus
}           /* End of extern "C" { */
#endif      /* __cplusplus */

/*
 *  Hook to allow an application to include something after every
 *  inclusion of windows.h.  To use this, they must define the
 *  value, and then provide the header somewhere in their include
 *  path.
 */
#if defined(OPENAPI_USE_APPLICATION_HEADER)
#include "openapi_application.h"
#endif

#endif      /* windows__h */

