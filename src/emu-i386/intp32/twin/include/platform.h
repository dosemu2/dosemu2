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


/*  platform.h	1.25
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


/*
 *  This file must be included by every module in our library, either
 *  directly, or indirectly via windows.h.  The contents of this file
 *  are the following types of things:
 *
 *    Platform-specific files that must be included in most/all modules.
 *    Platform-specific capability definitions (TWIN_*) that drive
 *        conditionally compiled code within the library.
 *
 *
 *  The following are the available defines that can be made for each 
 *  platform to enable/disable certain behaviour.  This overall list
 *  must be kept up to date.  If a new capability is added, check all
 *  platforms to see if the given entity should be defined for that
 *  case.
 *
 *  --------
 *  TWIN_FPCLASS_IEEEFP_H 	[OPTIONAL]
 *
 *	Used to control how the MS functions fpclass() and finite()
 *	are defined, as well as supporting macros.  Currently, the
 *      only allowed method is through the <ieeefp.h> file. 
 *  
 * 	This macro is used in the Win_Clib.h file, which is included
 *	by any application using windows.h
 *
 *  --------
 *  TWIN_ISCSYM_DEFINED		[OPTIONAL]
 *
 *      Set this if the platform already defines __iscsym().
 *
 * 	This macro is used in the Win_Clib.h file, which is included
 *	by any application using windows.h
 *
 *  --------
 *  TWIN_BOOL_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines BOOL (and the definition is
 *	compatible with the Windows definition).
 *
 * 	This macro is used in the windows.h file. 
 *
 *  --------
 *  TWIN_SHORT_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines SHORT (and the definition is
 *	compatible with the Windows definition).
 *
 * 	This macro is used in the windows.h file. 
 *
 *  --------
 *  TWIN_USHORT_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines USHORT (and the definition is
 *	compatible with the Windows definition).
 *
 * 	This macro is used in the windows.h file. 
 *
 *  --------
 *  TWIN_INT_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines INT (and the definition is
 *	compatible with the Windows definition).
 *
 * 	This macro is used in the windows.h file. 
 *
 *  --------
 *  TWIN_UINT_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines UINT (and the definition is
 *	compatible with the Windows definition).
 *
 * 	This macro is used in the windows.h file. 
 *
 *  --------
 *  TWIN_LONG_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines LONG (and the definition is
 *	compatible with the Windows definition).
 *
 * 	This macro is used in the windows.h file. 
 *
 *  --------
 *  TWIN_ULONG_DEFINED		[OPTIONAL]
 *
 *	Set this if the platform already defines ULONG (and the definition is
 *	compatible with the Windows definition).
 *
 *  --------
 *  TWIN_EMPTY_MODTABLE		[OPTIONAL]
 *
 *	Set this if the platform requires an empty modtable, for
 *	instance if the application does not require binary support.
 *	This decreases the memory requirements by not having the table
 *	mapping the binary interface to the native interface.
 *
 *  --------
 *  TWIN_EMPTY_LSD_PRINTER		[OPTIONAL]
 *
 *	Set this if the platform requires an empty lsd printer table, if
 *	the application does not intend to support printing.
 *
 *  --------
 *  TWIN_RUNTIME_DRVTAB		[OPTIONAL]
 *
 *	Set this if the platform requires the DrvTab entries to be set
 *      at runtime.  It's ok to set this even if it's not really needed.
 *	This fixes a problem where a table of function pointers defined
 *	at compile time are incorrect at runtime.  A workaround allows
 *	us to set the table entries at runtime with their correct values.
 *
 *  --------
 *  TWIN_HASSTATFS		[OPTIONAL]
 *  TWIN_HASSTATVFS		[OPTIONAL]
 *
 *	Set one or the other to determine if the operating system supports
 * 	statfs(), or vfsstat().  This is used in the XDOS emulation package
 *      to determine the size of a given filesystem, for DOS INT21 FN36.
 *
 *	Will assume existence of "sys/vms.h" if the above are not defined.
 *
 *  --------
 *  TWIN_HASDLFCN		
 *	
 *	Set if the system has the dlfcn.h include file, needed to support
 *	the dlopen/dladdr functions.
 *
 *  --------
 *  TWIN_USESYSCONF		
 *  TWIN_USETIMES
 *	
 * 	Set TWIN_USESYSCONF if you must use the sysconf() function to set the
 *	time, or TWIN_USETIMES to use the times() function.  Default is 
 *	TWIN_USETIMES.  The macro is only used in the pal/x11/DrvEvents.c
 *	file, which includes the <times.h> file.
 *
 *  --------
 *  TWIN_PLATFORM
 *	
 *	Set to the system name.
 *
 *	Defaults to "Unknown"
 *
 *  --------
 *  TWIN_DRVINIT
 *
 *	Set if .init function needs to be called manually,
 *	rather than automatically by ld.so
 *
 *  --------
 *  TWINRCFILE
 *	
 *	Set to the name of the users runtime configuration file. On unix, this
 *	would be .twinrc, in the users home directory.
 *
 *	Defaults to "twinrc", if not set.
 *
 *  --------
 *
 */

/*****************************************************************/

#ifndef platform__h
#define platform__h

#if defined(linux)
#define ELF
#define TWIN_HASDLFCN	1
#define TWIN_USESYSCONF 1

#define SYNC_CODE_DATA
#define TWIN_PLATFORM	"Linux 2.0.30"
#define TWIN_GCCINIT    1

#endif

#if defined(solaris)
#define TWIN_RUNTIME_DRVTAB 1
#define TWIN_HASSTATFS      1
#define TWIN_HASDLFCN       1
#define TWIN_USESYSCONF     1
#define BSD_COMP
#define TWIN_DRVINIT        1
#endif

#if defined(macintosh)
#include <stdlib.h>
#endif

#if defined(irix)
#define TWIN_HASSTATVFS
#define TWIN_HASDLFCN
#define TWIN_USESSYSCONF
#endif

#ifdef __VMS
#include <stdlib.h>
#define  unlink(x) delete(x)
#if __VMS_VER < 70000000
 int strcasecmp(const char *, const char *);
 int strncasecmp(const char *, const char *,size_t);
#define TWINRCFILE "twinrc.dat"
#define TWIN_USETIMES
#else
#define TWIN_USESYSCONF 1
#endif
#define TWIN_PLATFORM	"VMS"
#endif

#ifdef osf
#define TWIN_USESYSCONF
#define TWIN_HASSTATVFS
#define TWIN_PLATFORM	"OSF"
#endif

/*****************************************************************/

#define TWIN_SLASHSTRING	"/"
#define TWIN_SLASHCHAR	 	'/'
#define _EXTERN_ extern

/********************************************************************
 * 
 * catch everything NOT yet defined
 *
 *******************************************************************/

#ifndef TWIN_PLATFORM
#define TWIN_PLATFORM	"Unknown"
#endif


#ifndef TWIN_USESYSCONF
#define TWIN_USETIMES 1
#endif

#ifndef TWINRCFILE
#define TWINRCFILE ".twinrc"
#endif
/*****************************************************************/

#define DRVTAB	1

#if 0
#define TWIN_DLADDR     1		/* use dladdrtosym      */
#define TWIN_DLSYMS	1		/* use dladdr() */
#endif

#ifndef DOSEMU
#if @TWIN_BIN16@ 
#define TWIN_BIN16
#endif
#if @TWIN_BIN32@ 
#define TWIN_BIN32
#endif


#if  @TWIN_MALLOC@
#define WINMALLOC_CHECK 	1
#endif
#endif	/* DOSEMU */

#endif  /* platform__h */
