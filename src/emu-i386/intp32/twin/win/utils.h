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
	@(#)utils.h	2.5
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
 

#ifndef Utils__h
#define Utils__h


#define LM_SETFD	0
#define LM_SETMASK	1
#define LM_SETFILE	2
#define LM_CLOSELOG	3
#define LM_SETEXT	4

#define LM_WINDOWS	0x00000001 /* window management 	       */
#define LM_DRIVER	0x00000002 /* low-level driver in general      */
#define LM_GDI		0x00000004 /* all of GDI		       */
#define LM_FONT		0x00000008 /* font handling		       */
#define LM_RESOURCE	0x00000010 /* resource management	       */
#define LM_MENU		0x00000020 /* menu APIs and event handling     */
#define LM_CONTROL	0x00000040 /* standard controls		       */
#define LM_MESSAGE	0x00000080 /* messaging system		       */
#define LM_CLASSES	0x00000100 /* classes subsystem		       */
#define LM_OBJECTS	0x00000200 /* object management		       */
#define LM_DIALOGS	0x00000400 /* dialog manager/common dialogs    */
#define LM_SYSTEM	0x00000800 /* system stuff (timers,filesys etc.)*/
#define LM_APPLICATION  0x00001000 /* to use inside source ports       */
#define	LM_SHARED	0x00002000 /* clipboard 'n stuff	       */
#define LM_GRAPHICS	0x00004000 /* graphics call thru mdevsw...     */
#define LM_MEMORY	0x00008000 /* references to memory allocation  */
#define LM_EXTERNAL	0x00010000 /* external hook (X11)	       */
#define LM_KEYS		0x00020000 /* keystroke interface	       */
#define	LM_INTERFACE	0x00040000 /* binary interface layer	       */
#define	LM_OLE		0x00080000 /* OLE APIs			       */
#define	LM_XDOS		0x00100000 /* virtual DOS subsystem	       */
#define	LM_NONCLIENT	0x00200000 /* non-client events (frame)	       */
#define	LM_MDI		0x00400000 /* MDI-related code		       */
#define	LM_DDE		0x00800000 /* DDE messages, DDEML	       */
#define	LM_REGIONS	0x01000000 /* regions logic		       */
#define	LM_SHELL	0x02000000 /* shell functions		       */
#define	LM_PGH		0x04000000 /* DRIVER / GDI / EXTERNAL / GRAPHICS  */
#define	LM_PSH		0x08000000 /* DRIVER / GDI / EXTERNAL / GRAPHICS  */
#define	LM_PWH		0x10000000 /* DRIVER / GDI / EXTERNAL / GRAPHICS  */
#define LM_POSTSCRIPT	0x20000000 /* pscript.drv                         */
#define LM_ALL		0x40000000

#endif /* Utils__h */
