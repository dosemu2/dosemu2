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


/*   Win_Clib.h	1.29
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

#ifndef _WIN_CLIB_H
#define _WIN_CLIB_H

#if !defined(RC_INVOKED)
/********************************************************************
 *
 *  We need to pull in the system's definition of "random()" before
 *  we change it, so make sure we have pulled in stdlib.h and math.h
 *  by now.  (These are the normal "hiding places" for random().)
 *
 *  When including math.h, make sure _XOPEN_SOURCE is defined, in
 *  order to get M_PI and similar definitions.  However, if it wasn't
 *  already defined, do not leave it defined.
 *********************************************************************/

#include <stdlib.h>

#ifdef _XOPEN_SOURCE
#include <math.h>
#else
#define _XOPEN_SOURCE 1
#include <math.h>
#undef _XOPEN_SOURCE
#endif /* _XOPEN_SOURCE */

#endif /* RC_INVOKED */

#ifdef __cplusplus
extern "C" {
#endif

#define STR_RADIX_10 10		/* /wpshcode/shwin/fs/fscolmgr.c */
#define STR_RADIX_16 16		/* /wpshcode/shwin/fs/fscolmgr.c */

#define FP_OFF(a) (LOWORD(a))
#define FP_SEG(a) (HIWORD(a))

#undef  WFAR
#define WFAR
#undef  WHUGE
#define WHUGE
#undef  WNEAR
#define WNEAR

#undef	FAR
#define	FAR
#undef  _FAR
#define _FAR
#undef  __FAR
#define __FAR
#undef	far
#define	far
#undef	_far
#define	_far
#undef  __far
#define __far

#undef	NEAR
#define	NEAR
#undef	near
#define	near
#undef	_near
#define	_near
#undef  __near
#define __near

#undef	PASCAL
#define	PASCAL
#undef	pascal
#define	pascal
#undef	_pascal
#define	_pascal
#undef  __pascal
#define __pascal

#undef	CDECL
#define	CDECL
#undef	cdecl
#define	cdecl
#undef	_cdecl
#define	_cdecl
#undef	__cdecl
#define	__cdecl

#undef	EXPORT
#define	EXPORT
#undef	export
#define	export
#undef	_export
#define	_export
#undef	__export
#define	__export
#undef _fastcall
#define _fastcall
#undef	_segment
#define	_segment
#undef	__segment
#define	__segment unsigned long

#undef  huge
#define huge
#undef	_huge
#define	_huge
#undef  __huge
#define __huge

#undef  __loadds
#define __loadds
#undef  _loadds
#define _loadds


#ifndef	_MAX_PATH
#define	_MAX_PATH	255
#endif

#undef	BASED_CODE
#define	BASED_CODE
#undef	BASED_DEBUG
#define	BASED_DEBUG
#undef	BASED_STACK
#define	BASED_STACK

#define	_fmalloc	malloc
#define	_frealloc	realloc
#define	_ffree		free
#define	_fmemcpy	memcpy
#define _fmemset    	memset
#define	_fmemcmp	memcmp
#define	_fmemmove	memmove

#define	_fmemicmp	memcmp
#define _fstricmp	strcasecmp
#define _fstrcspn	strcspn

#define stricmp		strcasecmp
#define strnicmp	strncasecmp

#define	_fstrlen	strlen
#define	_fstrncpy	strncpy
#define _fstrcmp    	strcmp
#define	_fstrncmp	strncmp
#define	_fstrchr	strchr
#define _fstrrchr       strrchr
#define _fstrcpy        strcpy
#define _fstrcat        strcat
#define _fstrncat       strncat
#define _fstrspn	strspn
#define _fstrlwr	strlwr
#define _fstrupr	strupr
#define _fstrrev	strrev
#define _getcwd		getcwd

#define _strlwr		strlwr
#define _strupr		strupr
#define _strrev		strrev
#define _stricmp	strcasecmp
#define _strnicmp	strncasecmp
#define _strcmpi	strcasecmp

#define	_dup		dup
#define	_strdup		strdup
#define	_fstrdup	strdup

#define	_itoa		itoa
#define	ltoa		itoa
#define	_ltoa		itoa

#define	_lrotl		lrotl
#define	_lrotr		lrotr

#define _open		open
#define _close		close
#define _write		write
#define _read		read
#define _access		access

LPSTR itoa(int, LPSTR, int);
LPSTR _strdate(LPSTR);
LPSTR strlwr(LPSTR);
LPSTR strupr(LPSTR);
LPSTR strrev(LPSTR);

ULONG lrotl(ULONG, int);
ULONG lrotr(ULONG, int);

/*
 *  Remap some MS names for stat(2) flags to standard names.
 */
#if !defined(OPEN_NOREMAP_FLAGS)
#define _S_IREAD S_IREAD
#define _S_IWRITE S_IWRITE
#endif
/*
 *  Remap some MS names for open(2) and creat(2) flags to standard names.
 */
#if !defined(STAT_NOREMAP_FLAGS)

#ifndef O_TEXT
#define O_TEXT 0
#endif

#define _O_RDONLY       O_RDONLY
#define _O_WRONLY       O_WRONLY
#define _O_RDWR         O_RDWR
#define _O_APPEND       O_APPEND
#define _O_CREAT        O_CREAT
#define _O_TRUNC        O_TRUNC
#define _O_EXCL         O_EXCL
#define _O_TEXT         O_TEXT
#define _O_BINARY       0

#endif

#if !defined(TWIN_ISCSYM_DEFINED)
#define __iscsym(_c) (isalnum(_c)||((_c)==' '))
#endif /* TWIN_ISCSYM_DEFINED */

#if defined(TWIN_FPCLASS_IEEEFP_H)
/*
 *  In this case, we need to pick up the fpclass() definitions
 *  from <ieeefp.h>, and define mappings from the MS-names to
 *  the names we have.   
 */
#include <ieeefp.h>

#define _FPCLASS_SNAN   FP_SNAN       /* signaling NaN */
#define _FPCLASS_QNAN   FP_QNAN       /* quiet NaN */
#define _FPCLASS_NINF   FP_NINF       /* negative infinity */
#define _FPCLASS_NN     FP_NNORM      /* negative normal */
#define _FPCLASS_ND     FP_NDENORM    /* negative denormal */
#define _FPCLASS_NZ     FP_NZERO      /* -0 */
#define _FPCLASS_PZ     FP_PZERO      /* +0 */
#define _FPCLASS_PD     FP_PDENORM    /* positive denormal */
#define _FPCLASS_PN     FP_PNORM      /* positive normal */
#define _FPCLASS_PINF   FP_PINF       /* positive infinity */

#define _fpclass(dbl) fpclass(dbl)
#define _finite(dbl)  finite(dbl)
#endif /* TWIN_FPCLASS_IEEEFP_H */

/*
 *  Borland uses a random() that has a different interface than the
 *  Unix random().  Since the Unix random() prototype or macro is
 *  brought in via stdlib.h, and we include stdlib.h earlier in this
 *  file, we can safely provide our own replacement with #define at
 *  this time.  The actual routine is called borland_random(),
 *  and we change all occurences of "random" past this point to
 *  "borland_random".
 */
#ifdef random
#undef random
#endif
#define random borland_random
int borland_random(int num);

#ifdef __cplusplus
}
#endif

#endif /* _WIN_CLIB_H */
