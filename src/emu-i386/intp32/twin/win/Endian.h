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
	@(#)Endian.h	2.3
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
 
#ifndef Endian__h
#define Endian__h


/* macros to access non-aligned data */
#ifdef	LATER
#define	GETWORD(p) (WORD)(((BYTE *)p)[0]+(((BYTE *)p)[1]*0x100))
#define PUTWORD(p,w) { WORD t = (WORD)w; LPBYTE b = (LPBYTE)p; \
			*(b) = (BYTE)(t % 0x100); \
		       *(b+1) = (BYTE)(t / 0x100); }

#define PUTDWORD(p,dw) { WORD h = HIWORD((DWORD)dw); \
			WORD l = LOWORD((DWORD)dw); \
			LPBYTE b = (LPBYTE)p; \
			*(b++) = (BYTE)(l % 0x100); \
			*(b++) = (BYTE)(l / 0x100); \
			*(b++) = (BYTE)(h % 0x100); \
			*(b++) = (BYTE)(h / 0x100); }
#define GETDWORD(p) ((DWORD)((GETWORD((LPBYTE)(p)+2) * 0x10000) \
			     + GETWORD((LPBYTE)p)))
#else
#define	GETWORD(p) (WORD)(((LPBYTE)(p))[0]+((WORD)(((LPBYTE)(p))[1])<<8))
#define PUTWORD(p,w) { *((LPBYTE)(p)) = (BYTE)((WORD)(w) % 0x100); \
		       *((LPBYTE)(p) + 1) = (BYTE)((WORD)(w)>>8); }

#define PUTDWORD(p,dw) { PUTWORD((LPBYTE)(p),LOWORD((DWORD)(dw))); \
			PUTWORD((LPBYTE)(p)+2,HIWORD((DWORD)(dw))); }
#define GETDWORD(p) ((DWORD)((DWORD)(GETWORD((LPBYTE)(p)+2)<<16) \
			     + GETWORD((LPBYTE)(p))))
#endif

#define	GETSHORT(p)	(short)GETWORD(p)

#endif /* Endian__h */
