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


/*  Log.h	2.11
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

#ifndef Log__h
#define Log__h	

#include "windows.h"

void logstr(unsigned long flg, ...);

#ifdef TRACE

#define ERRSTR(x)	logstr x
#define APISTR(x)	logstr x
#define LOGSTR(x)	logstr x

#else

#define	ERRSTR(x)	logstr x
#define	APISTR(x)	
#define	LOGSTR(x)	

#endif

/*****************************************************

	LOGSTR 

*****************************************************/

/********************************************************************
 *
 * 	There are 5 classes of debugging/tracing available, the type is
 *	passed as the first parameter to LOGSTR. The first class will be
 *	sent to stdout, or stderr, while the others will be logged to the
 *	'logging' file, as defined by WIN_OPENLOG.
 *	
 * 	LF_DBGMASK
 *	0	Generic
 *		LF_LOG	   to stdout
 *		LF_DEBUG   to stderr
 *
 *	LF_LOGMASK
 *	1	Logging, 
 *		LF_WARNING	warning messages
 *		LF_ERROR	error messages
 *
 *	LF_APIMASK
 *	2	Logging of WINAPI call/returns
 *		LF_APICALL	an api is called	
 *		LF_APIRET	an api returns
 *		LF_APIFAIL	an api failed
 *		LF_API		an api message
 *		LF_APISTUB	an api not implemented
 *
 *	LF_BINMASK
 *	3	Logging of binary interface, dos calls, and interrupts
 *		LF_BINCALL	from windows binary
 *		LF_BINRET	back to windows binary
 *		LF_INTCALL	from interrupt
 *		LF_INTRET	back from interrupt
 *		LF_INT86	generic interrupt
 *
 *	LF_MSGMASK
 *	4	Logging of message interface
 *		LF_MSGCALL	message sent
 *		LF_MSGRET	message results
 *
 ********************************************************************/

/*
 * generic logging
 *
 * usage:
 *	logstr(LF_LOG,"This is a generic message.\n");
 * 
 * note:
 *	each of the subclasses may be individually selected,
 *	so that you see all, or none of a generic class
 */

#define LF_CONSOLE	MAKELONG(1,0)		/* selected with WD_CONSOLE */
#define LF_VERBOSE	MAKELONG(2,0)		/* selected with WD_VERBOSE */
#define LF_SYSTEM 	MAKELONG(4,0)		/* selected with WD_SYSTEM  */
#define LF_LOG		MAKELONG(8,0)		/* selected with WD_LOGGING */

/*
 * generic error or warning
 *
 * usage:
 *	logstr(LF_WARNING,"Resource %s not found.\n",lpstr);
 *	logstr(LF_ERROR,"WindowCreate() FAILED.\n");
 *
 * note:
 */
#define LF_WARNING	MAKELONG(0,1)
#define LF_ERROR	MAKELONG(1,1)
#define LF_DEBUG	MAKELONG(2,1)

/*
 * api call and return 
 *
 * usage:
 *	
 *	APISTR((LF_APICALL,"IsWindow(HWND=%x)\n",hWnd));
 *
 *	if(error) {
 *		APISTR((LF_APIFAIL,"IsWindow: returns BOOL %d\n",FALSE));
 *	 	return FALSE;
 *	}
 *
 *	APISTR((LF_APIRET,"IsWindow: returns BOOL %d\n",TRUE));
 *	return TRUE;
 *
 * note:
 *	APICALL increments the API call count,
 *	APIFAIL, APIRET decrement the API call count.
 *
 *	logging can be turned on for up to a certain call count,
 *	no logging, or logging all api calls, using WIN_LOGAPI
 *	WIN_LOGAPI:
 *		-1	all api's logged
 *		0	no api's logged
 *		n	api call count < n
 *	to log only calls made by application, set to 1
 *	to log api's called by api's called by application, set to 2
 *
 *	all logging for these will have API, and either CALL, RET, or FAIL
 *	if WIN_LOGFLAG & 8, 
 */
#define LF_APICALL	MAKELONG(0,2)
#define LF_APIRET	MAKELONG(1,2)
#define LF_APIFAIL	MAKELONG(2,2)
#define LF_API		MAKELONG(4,2)
#define LF_APISTUB	MAKELONG(8,2)

/*
 * bin logging
 *
 */
#define LF_BINCALL	MAKELONG(0,3)
#define LF_BINRET	MAKELONG(1,3)
#define LF_INTCALL	MAKELONG(2,3)
#define LF_INTRET	MAKELONG(4,3)
#define LF_INT86	MAKELONG(8,3)


/*
 * msg logging
 *
 */
#define LF_MSGCALL	MAKELONG(0,4)
#define LF_MSGRET	MAKELONG(1,4)

#endif
