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


/********************************************************************
	@(#)WinConfig.h	1.12 Twin configuration file management.
    	Copyright 1997 Willows Software, Inc. 

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




For more information about the Willows Twin Libraries.

	http://www.willows.com	

To send email to the maintainer of the Willows Twin Libraries.

	mailto:twin@willows.com 

********************************************************************/
 
 
char *GetTwinFilename(void);
char *GetTwinString(int, char *,int );
unsigned long GetTwinInt(int);
char *GetEnv(const char *env);


/* WinConfig Sections definitions */
#define WCS_BOOT        0
#define WCS_WINDOWS     1
#define WCS_XDOS        2
#define WCS_COMMDLG     3
#define WCS_ENV      	4

/* WinConfig Parameter definitions */
#define WCP_DISPLAY     1
#define WCP_FATAL	2
#define WCP_CONTROL	3
#define	WCP_WINDOWS	4
#define	WCP_TEMP	5
#define	WCP_OPENPATH	6
#define	WCP_TASKING	7
#define	WCP_DBLCLICK	8

#define	WCP_FONTFACE	 10
#define	WCP_FONTSIZE	 11
#define	WCP_FONTBOLD	 12
#define WCP_ICONFONTFACE 13
#define WCP_ICONFONTSIZE 14
#define WCP_MENUSIZE	 15
#define WCP_MENUBOLD	 16
#define WCP_DLGSIZE	 17
#define WCP_DLGBOLD	 18

#define	WCP_DOSMODE	20
#define WCP_MEMORY	21
#define WCP_EXTENDED	22

#define WCP_DOSDRIVES		30
#define WCP_DRIVELETTERS	31
#define WCP_PATHSASDRIVES	32

typedef struct twinrc {
	int     parameter;
	int	opcode;
	int     section;
	char   *lpszkeyname;
	char   *lpszdefault;
	char   *lpszenviron;
	char   *lpszstring;
} TWINRC, *LPTWINRC;
