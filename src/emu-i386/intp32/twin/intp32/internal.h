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
 

#ifndef _CPUEMU_INTERNAL_H_
#define _CPUEMU_INTERNAL_H_

#ifdef _MSC_VER
#include <stdafx.h>
#include <stdlib.h>
#include <conio.h>
#ifdef _M_IX86
#define __i386__
#endif
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include "emu-ldt.h"

/* copied from windef.h */
#ifndef BASETYPES
#define BASETYPES
typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef unsigned short USHORT;
typedef USHORT *PUSHORT;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef char *PSZ;
#endif  /* !BASETYPES */

#define MAX_PATH          260

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef OPTIONAL
#define OPTIONAL
#endif

#undef far
#undef near
#undef pascal

#define far
#define near
#if (!defined(_MAC)) && ((_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED))
#define pascal __stdcall
#else
#define pascal
#endif

#if defined(DOSWIN32) || defined(_MAC)
#define cdecl _cdecl
#ifndef CDECL
#define CDECL _cdecl
#endif
#else
#define cdecl
#ifndef CDECL
#define CDECL
#endif
#endif

#ifdef _MAC
#define CALLBACK    PASCAL
#define WINAPI      CDECL
#define WINAPIV     CDECL
#define APIENTRY    WINAPI
#define APIPRIVATE  CDECL
#ifdef _68K_
#define PASCAL      __pascal
#else
#define PASCAL
#endif
#elif (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED)
#define CALLBACK    __stdcall
#define WINAPI      __stdcall
#define WINAPIV     __cdecl
#define APIENTRY    WINAPI
#define APIPRIVATE  __stdcall
#define PASCAL      __stdcall
#else
#define CALLBACK
#define WINAPI
#define WINAPIV
#define APIENTRY    WINAPI
#define APIPRIVATE
#define PASCAL      pascal
#endif

#undef FAR
#undef NEAR
#define FAR                 far
#define NEAR                near
#ifndef CONST
#define CONST               const
#endif

typedef	char		    CHAR;
typedef	char		    TCHAR;
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef short int	    SHORT;
typedef unsigned short      WORD;
typedef long		    LONG;
typedef float               FLOAT;

typedef char		    *PSTR;
typedef char		    *NPSTR;
typedef char		    *LPSTR;
typedef const char	    *LPCSTR;
typedef char		    *LPTSTR;
typedef const char	    *LPCTSTR;

typedef FLOAT               *PFLOAT;
typedef BOOL near           *PBOOL;
typedef BOOL far            *LPBOOL;
typedef BYTE near           *PBYTE;
typedef BYTE far            *LPBYTE;
typedef int near            *PINT;
typedef int far             *LPINT;
typedef WORD near           *PWORD;
typedef WORD far            *LPWORD;
typedef long far            *LPLONG;
typedef DWORD near          *PDWORD;
typedef DWORD far           *LPDWORD;
typedef void		    *PVOID;
typedef const void	    *PCVOID;
typedef void far            *LPVOID;
typedef CONST void far      *LPCVOID;

typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

typedef DWORD		    BINADDR;

#ifdef _MSC_VER
typedef __int64	QLONG;
typedef unsigned __int64	QWORD;
#else
typedef long long	QLONG;
typedef unsigned long long	QWORD;
#endif

#ifndef NOMINMAX

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#endif  /* NOMINMAX */

#ifdef  TWIN32
typedef unsigned int        WLS_UINT;
#else
typedef unsigned short      WLS_UINT;
#endif

/* Types use for passing & returning polymorphic values */
typedef WLS_UINT WPARAM;
typedef LONG LPARAM;
typedef LONG LRESULT;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l)           ((WORD)(l))
#define HIWORD(l)           ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))

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

/* end stealing from windef.h */

#ifdef _MSC_VER
#define _INLINE_ static __inline
#define _ASM_	__asm
#define _STATIC_ static
#define FORMAT(T,A,B)
#else
#ifdef DOSEMU
#include "dosemu_debug.h"
#define	d_emu	d.emu
#endif
#define _INLINE_ static __inline__
#define _ASM_	__asm__ __volatile__
#define _STATIC_ static
#endif

BOOL LoadSegment(UINT);

typedef struct keySEGIMAGE
{   unsigned long conv;
    unsigned long targ;
} SEGIMAGE;

/*
 *	the following is used to describe normal 16 and 32 bit code and
 *	data segments. The native world calls an interface routine to 
 *	setup the call to the code. Either through an interpreter or
 *	through direct execution we can get to the following.  The
 *	DATAxx segments are to allow simple traps to catch executing data.
 */
#define TRANSFER_CODE16		1	/* 16bit code segment */
#define TRANSFER_DATA		2	/* 16bit data segment */
#define TRANSFER_CODE32		3	/* 32bit code segment */
#define TRANSFER_DATA32		4	/* 32bit data segment */

/*
 *	this corresponds to a native code segment, which has an alias
 *	CODE16 segment for it.  On transfers to it, we make the transition
 *	from the binary world to the native world.
 */
#define TRANSFER_CALLBACK 	5	/* alias for native code segment */
#define TRANSFER_NATIVE 	6	/* a real native segment */

/*
 *	the following segments are used to map native library addresses 
 *	to thunks that can be called by the application. 
 */
#define TRANSFER_BINARY		7	/* native code segment (pseudo-code) */
#define TRANSFER_RETURN		8	/* native code return (pseudo-code)  */


/* Macros to access fields in LDT table		*/

#define LDTgetSelBase(w)	LDT[(w)>>3].lpSelBase
#define LDTsetSelBase(w,d)	LDT[(w)>>3].lpSelBase=(d)
#define LDTgetSelLimit(w)	LDT[(w)>>3].dwSelLimit
#define LDTsetSelLimit(w,d)	LDT[(w)>>3].dwSelLimit=(d)
#define LDTgetFlags(w)		LDT[(w)>>3].w86Flags
#define LDTsetFlags(w,d)	LDT[(w)>>3].w86Flags=(d)
#define GetFlagAccessed(w)	LDT[(w)>>3].w86Flags&1
#define SetFlagAccessed(w)	LDT[(w)>>3].w86Flags|=1
/* not in real LDT */
#define LDTgetGlobal(w)		LDT[(w)>>3].hGlobal
#define LDTsetGlobal(w,d)	LDT[(w)>>3].hGlobal=(d)
#define LDTgetType(w)		LDT[(w)>>3].bSelType
#define LDTsetType(w,d)		LDT[(w)>>3].bSelType=(d)
#define LDTgetIndex(w)		LDT[(w)>>3].bModIndex
#define LDTsetIndex(w,d)	LDT[(w)>>3].bModIndex=(d)

#ifdef DOSEMU

#define GetSelectorAddress(w)	(VM86F? (LPBYTE)(w<<4):\
				((LDT[w>>3].w86Flags & DF_PRESENT)? \
				LDTgetSelBase(w):0))

#define SetPhysicalAddress(w,l) { if (!VM86F) LDTsetSelBase(w,(LPBYTE)l); }

#define	GetPhysicalAddress(w)	(VM86F? (LPBYTE)(w<<4):LDTgetSelBase(w))

#define GetSelectorLimit(w)	(VM86F? 0xffff:LDTgetSelLimit(w))
#define GetSelectorByteLimit(w)	(VM86F? 0xffff:\
				 (LDTgetFlags(w)&DF_PAGES?\
				  (LDTgetSelLimit(w)<<12)|0xfff :\
				  LDTgetSelLimit(w)))
#define GetSelectorAddrMax(w)	(GetSelectorAddress(w)+GetSelectorByteLimit(w))

#define SetSelectorLimit(w,d)	{ if (!VM86F) LDTsetSelLimit(w,(DWORD)d); }

/* unused
#define GetSelectorHandle(w)	(VM86F? 1:LDTgetGlobal(w))
#define SetSelectorHandle(w,h)	{ if (!VM86F) LDTsetGlobal(w,(HGLOBAL)h);}
*/

#define GetSelectorFlags(w)	(VM86F? 0x00f0:LDTgetFlags(w))
#define SetSelectorFlags(w,wf)	{ if (!VM86F) LDTsetFlags(w,(WORD)wf); }

#define GetSelectorType(w)	(VM86F? 0:LDTgetType(w))
#define SetSelectorType(w,bt)	{ if (!VM86F) LDTsetFlags(w,Sel86Flags[bt]), \
				  LDTsetType(w,(BYTE)bt); }
/* unused
#define GetModuleIndex(w)	(VM86F? 0:LDTgetIndex(w))
#define SetModuleIndex(w,bi)	{ if (!VM86F) LDTsetIndex(w,(BYTE)bi); }
*/

#else	/* !DOSEMU */

LPSTR GetAddress(WORD,WORD);
WORD AssignSelector(LPBYTE,WORD,BYTE,DWORD);
WORD AssignSelRange(int);
UINT TWIN_ReallocSelector(UINT,DWORD,UINT);
WORD MakeSegment(DWORD,WORD);
int GetAHSHIFT(void);
int GetAHINCR(void);

#define ASSIGNSEL(lp, sz)	AssignSelector((LPBYTE)lp,0,TRANSFER_DATA,sz)

/* Macros to access fields in LDT table		*/

#define GetSelectorAddress(w)	((LDT[w>>3].w86Flags & DF_PRESENT)? \
				LDT[w>>3].lpSelBase:(LoadSegment(w)? \
				LDT[w>>3].lpSelBase:0))

#define SetPhysicalAddress(w,l) { LDT[w>>3].lpSelBase = (LPBYTE)l; }

#define	GetPhysicalAddress(w)	LDT[w>>3].lpSelBase

#define GetSelectorAddrMax(w)	(GetSelectorAddress(w)+GetSelectorByteLimit(w))

#define GetSelectorLimit(w)	LDT[w>>3].dwSelLimit
#define GetSelectorByteLimit(w)	(LDTgetFlags(w)&DF_PAGES?\
				  (LDTgetSelLimit(w)<<12)|0xfff :\
				  LDTgetSelLimit(w))
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
#define DN_EXIT		5

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
#define DF_ACCESSED	0x01

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

#define IS_GDT(sel)	(((sel)&4)==0)
#define IS_LDT(sel)	(((sel)&4)!=0)

/* Segment limit -- 64K */
#define SL_DEFAULT	(LPARAM)0xFFFF

#ifndef DOSEMU
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
extern int   nLDTSize;
#endif

#ifndef DOSEMU
#define FORMAT(T,A,B)  __attribute__((format(T,A,B)))
/* Debug stuff */
int log_printf(int, const char *,...) FORMAT(printf, 2, 3);
/* unconditional message into debug log and stderr */
void error(const char *fmt, ...);

#ifdef _MSC_VER
#define ifprintf0(flg,fmt)	do{ if (flg) log_printf(flg,fmt); }while(0)
#define ifprintf(flg,fmt,a)	do{ if (flg) log_printf(flg,fmt,##a); }while(0)
/* unconditional message into debug log */
#define dbug_printf(f,a)	ifprintf(10,f,##a)
#define e_printf0(f)     	ifprintf0(d_emu,f)
#define e_printf(f,a)     	ifprintf(d_emu,f,##a)
#else
#define ifprintf(flg,fmt,a...)	do{ if (flg) log_printf(flg,fmt,##a); }while(0)
/* unconditional message into debug log */
#define dbug_printf(f,a...)	ifprintf(10,f,##a)
#define e_printf(f,a...)     	ifprintf(d_emu,f,##a)
#endif

extern int	d_emu;
#endif	/* DOSEMU */

#define e_printf0		e_printf

#define IO_BITMAP_SIZE	32

/**/

#if defined(__i386__)&&defined(__GNUC__)

_INLINE_ void port_real_outb(WORD port, BYTE value)
{
  _ASM_ ("outb %0,%1"
		    ::"a" ((BYTE) value), "d"((WORD) port));
}

_INLINE_ BYTE port_real_inb(WORD port)
{
  BYTE _v;
  _ASM_ ("inb %1,%0"
		    :"=a" (_v):"d"((WORD) port));
  return _v;
}

_INLINE_ void port_real_outw(WORD port, WORD value)
{
  _ASM_ ("outw %0,%1" :: "a" ((WORD) value),
		"d" ((WORD) port));
}

_INLINE_ WORD port_real_inw(WORD port)
{
  WORD _v;
  _ASM_ ("inw %1,%0":"=a" (_v) : "d" ((WORD) port));
  return _v;
}

_INLINE_ void port_real_outd(WORD port, DWORD value)
{
  _ASM_ ("outl %0,%1" : : "a" (value),
		"d" ((WORD) port));
}

_INLINE_ DWORD port_real_ind(WORD port)
{
  DWORD _v;
  _ASM_ ("inl %1,%0":"=a" (_v) : "d" ((WORD) port));
  return _v;
}

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy {
    unsigned long   a[100];
};
#define ADDR (*(struct __dummy *) addr)

/*
 * Linus' stuff follows - except each __inline__ had an extern in front of
 * it
 */
_INLINE_ int
set_bit(int nr, void *addr)
{
    int             oldbit;

    _ASM_ ("btsl %2,%1\n\tsbbl %0,%0"
				 :"=r"           (oldbit), "=m"(ADDR)
				 :"r"            (nr));
    return oldbit;
}

_INLINE_ int
clear_bit(int nr, void *addr)
{
    int             oldbit;

    _ASM_ ("btrl %2,%1\n\tsbbl %0,%0"
				 :"=r"           (oldbit), "=m"(ADDR)
				 :"r"            (nr));
    return oldbit;
}

_INLINE_ int
test_bit(int nr, void *addr)
{
    int             oldbit;

    _ASM_ ("btl %2,%1\n\tsbbl %0,%0"
				 :"=r"           (oldbit)
				 :"m"            (ADDR), "r"(nr));
    return oldbit;
}

#else

#define port_real_outb(port,value)	_outp(port,value)
#define port_real_inb(port)			_inp(port)
#define port_real_outw(port,value)	_outpw(port,value)
#define port_real_inw(port)			_inpw(port)
#define port_real_outd(port,value)	_outpd(port,value)
#define port_real_ind(port)			_inpd(port)

_INLINE_ void
set_bit(int nr, void *addr)
{
	char *bp = ((char *)addr)+(nr>>3);
	*bp |= (1<<(nr&7));
}

_INLINE_ void
clear_bit(int nr, void *addr)
{
	char *bp = ((char *)addr)+(nr>>3);
	*bp &= ~(1<<(nr&7));
}

_INLINE_ int
test_bit(int nr, void *addr)
{
	char *bp = ((char *)addr)+(nr>>3);
	return ((*bp & (1<<(nr&7)))!=0);
}

#endif

#endif	/*_CPUEMU_INTERNAL_H_*/
