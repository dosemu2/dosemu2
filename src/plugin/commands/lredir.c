/*
 * Written: 10/29/93 by Tim Bird (original code)
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************
 * File: LREDIR.C
 *  Program for Linux DOSEMU disk redirector functions
 * Fixes: 1/7/95 by Tim Josling TEJ: check correct # args
 *      :                            display read/write status correctly
 * Changes: 3/19/95 by Kang-Jin Lee
 *  Removed unused variables
 *  Changes to eliminate warnings
 *  Display read/write status when redirecting drive
 *  Usage information is more verbose
 *
 * Changes: 11/27/97 Hans Lermen
 *  new detection stuff,
 *  rewrote to use plain C and intr(), instead of asm and fiddling with
 *  (maybe used) machine registers.
 *
 * Changes: 990703 Hans Lermen
 *  Checking for DosC and exiting with error, if not redirector capable.
 *  Printing DOS errors in clear text.
 *
 * Changes: 010211 Bart Oldeman
 *   Make it possible to "reredirect": LREDIR drive filepath
 *   can overwrite the direction for the drive.
 *
 * Changes: 20010402 Hans Lermen
 *   Ported to buildin_apps, compiled directly into dosemu
 *
 *
 * NOTES:
 *  LREDIR supports the following commands:
 *  LREDIR drive filepath
 *    redirects the indicated drive to the specified linux filepath
 *    drive = the drive letter followed by a colon (ex. 'E:')
 *    filepath has linux, fs and a file path (ex. 'LINUX\FS\USR\SRC')
 *  LREDIR DEL drive
 *    cancels redirection of the indicated drive
 *  LREDIR
 *    shows drives that are currently redirected (mapped)
 *  LREDIR HELP or LREDIR ?
 *    show usage information for LREDIR
 ***********************************************/


#include <ctype.h>    /* toupper */
#include <stdio.h>    /* printf  */
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "msetenv.h"
#include "doshelpers.h"
#include "utilities.h"
#include "lredir.h"
#include "builtins.h"
#include "disks.h"

#define printf	com_printf
#define	intr	com_intr
#define	strncmpi strncasecmp
#define FP_OFF(x) FP_OFF32(x)
#define FP_SEG(x) FP_SEG32(x)

typedef unsigned char uint8;
typedef unsigned int uint16;

#define CARRY_FLAG    1 /* carry bit in flags register */
#define CC_SUCCESS    0

#define DOS_GET_LIST_OF_LISTS  0x5200
#define DOS_GET_SDA_POINTER    0x5D06
#define DOS_GET_REDIRECTION    0x5F02
#define DOS_REDIRECT_DEVICE    0x5F03
#define DOS_CANCEL_REDIRECTION 0x5F04

#define MAX_RESOURCE_STRING_LENGTH  36  /* 16 + 16 + 3 for slashes + 1 for NULL */
#define MAX_RESOURCE_PATH_LENGTH   128  /* added to support Linux paths */
#define MAX_DEVICE_STRING_LENGTH     5  /* enough for printer strings */

#define REDIR_PRINTER_TYPE    3
#define REDIR_DISK_TYPE       4

#define READ_ONLY_DRIVE_ATTRIBUTE 1  /* same as NetWare Lite */

#define KEYWORD_DEL   "DELETE"
#define KEYWORD_DEL_COMPARE_LENGTH  3

#define KEYWORD_HELP   "HELP"
#define KEYWORD_HELP_COMPARE_LENGTH  4

#define DEFAULT_REDIR_PARAM   0

#include "doserror.h"

static FAR_PTR /* char far * */
GetListOfLists(void)
{
    FAR_PTR LOL;
    struct REGPACK preg = REGPACK_INIT;

    preg.r_ax = DOS_GET_LIST_OF_LISTS;
    intr(0x21, &preg);
    LOL = MK_FP(preg.r_es, preg.r_bx);
    return (LOL);
}

static FAR_PTR /* char far * */
GetSDAPointer(void)
{
    FAR_PTR SDA;
    struct REGPACK preg = REGPACK_INIT;

    preg.r_ax = DOS_GET_SDA_POINTER;
    intr(0x21, &preg);
    SDA = MK_FP(preg.r_ds, preg.r_si);

    return (SDA);
}

/********************************************
 * InitMFS - call Emulator to initialize MFS
 ********************************************/
/* tej - changed return type to void as nothing returned */
static void InitMFS(void)
{
    FAR_PTR LOL;
    FAR_PTR SDA;
    unsigned char _osmajor;
    unsigned char _osminor;
    state_t preg;

    LOL = GetListOfLists();
    SDA = GetSDAPointer();

    /* now get the DOS version */
    HI(ax) = 0x30;
    call_msdos();
    _osmajor = LO(ax);
    _osminor = HI(ax);

    /* get DOS version into CX */
    preg.ecx = _osmajor | (_osminor <<8);

    preg.edx = FP_OFF16(LOL);
    preg.es = FP_SEG16(LOL);
    preg.esi = FP_OFF16(SDA);
    preg.ds = FP_SEG16(SDA);
    preg.ebx = 0x500;
    mfs_helper(&preg);
}

/********************************************
 * RedirectDevice - redirect a device to a remote resource
 * ON ENTRY:
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 *  resourceStr has a string with the server and name of resource
 *    (ex. 'TIM\TOOLS')
 *  deviceType indicates the type of device being redirected
 *    3 = printer, 4 = disk
 *  deviceParameter is a value to be saved with this redirection
 *  which will be returned on GetRedirectionList
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful,
 *  otherwise returns the DOS error code
 * NOTES:
 *  deviceParameter is used in DOSEMU to return the drive attribute
 *  It is not actually saved and returned as specified by the redirector
 *  specification.  This type of usage is common among commercial redirectors.
 ********************************************/
static uint16 RedirectDevice(char *deviceStr, char *resourceStr, uint8 deviceType,
                      uint16 deviceParameter)
{
    char slashedResourceStr[MAX_RESOURCE_PATH_LENGTH];
    struct REGPACK preg = REGPACK_INIT;
    char *dStr, *sStr;

    /* prepend the resource string with slashes */
    strcpy(slashedResourceStr, "\\\\");
    strcat(slashedResourceStr, resourceStr);

    /* should verify strings before sending them down ??? */
    dStr = com_strdup(deviceStr);
    preg.r_ds = FP_SEG(dStr);
    preg.r_si = FP_OFF(dStr);
    sStr = com_strdup(slashedResourceStr);
    preg.r_es = FP_SEG(sStr);
    preg.r_di = FP_OFF(sStr);
    preg.r_cx = deviceParameter;
    preg.r_bx = deviceType;
    preg.r_ax = DOS_REDIRECT_DEVICE;
    intr(0x21, &preg);
    com_strfree(dStr);
    com_strfree(sStr);

    if (preg.r_flags & CARRY_FLAG) {
      return (preg.r_ax);
    }
    else {
      return (CC_SUCCESS);
    }
}

/********************************************
 * GetRedirection - get next entry from list of redirected devices
 * ON ENTRY:
 *  redirIndex has the index of the next device to return
 *    this should start at 0, and be incremented between calls
 *    to retrieve all elements of the redirection list
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful, and
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 *  resourceStr has a string with the server and name of resource
 *    (ex. 'TIM\TOOLS')
 *  deviceType indicates the type of device which was redirected
 *    3 = printer, 4 = disk
 *  deviceParameter has my rights to this resource
 * NOTES:
 *
 ********************************************/
static uint16 GetRedirection(uint16 redirIndex, char *deviceStr, char *resourceStr,
                      uint8 * deviceType, uint16 * deviceParameter)
{
    uint16 ccode;
    uint8 deviceTypeTemp;
    char slashedResourceStr[MAX_RESOURCE_PATH_LENGTH];
    struct REGPACK preg = REGPACK_INIT;
    char *dStr, *sStr;

    dStr = lowmem_alloc(16);
    preg.r_ds = FP_SEG(dStr);
    preg.r_si = FP_OFF(dStr);
    sStr = lowmem_alloc(128);
    preg.r_es = FP_SEG(sStr);
    preg.r_di = FP_OFF(sStr);
    preg.r_bx = redirIndex;
    preg.r_ax = DOS_GET_REDIRECTION;
    intr(0x21, &preg);
    strcpy(deviceStr,dStr);
    lowmem_free(dStr, 16);
    strcpy(slashedResourceStr,sStr);
    lowmem_free(sStr, 128);

    ccode = preg.r_ax;
    deviceTypeTemp = preg.r_bx & 0xff;       /* save device type before C ruins it */
    *deviceType = deviceTypeTemp;
    *deviceParameter = preg.r_cx;

    /* copy back unslashed portion of resource string */
    if (preg.r_flags & CARRY_FLAG) {
      return (ccode);
    }
    else {
      /* eat the leading slashes */
      strcpy(resourceStr, slashedResourceStr + 2);
      return (CC_SUCCESS);
    }
}

/********************************************
 * CancelRedirection - delete a device mapped to a remote resource
 * ON ENTRY:
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful,
 *  otherwise returns the DOS error code
 * NOTES:
 *
 ********************************************/
static uint16 CancelRedirection(char *deviceStr)
{
    struct REGPACK preg = REGPACK_INIT;
    char *dStr;

    dStr = com_strdup(deviceStr);
    preg.r_ds = FP_SEG(dStr);
    preg.r_si = FP_OFF(dStr);
    preg.r_ax = DOS_CANCEL_REDIRECTION;
    intr(0x21, &preg);
    com_strfree(dStr);

    if (preg.r_flags & CARRY_FLAG) {
      return (preg.r_ax);
    }
    else {
      return (CC_SUCCESS);
    }
}

/*************************************
 * ShowMyRedirections
 *  show my current drive redirections
 * NOTES:
 *  I show the read-only attribute for each drive
 *    which is returned in deviceParam.
 *************************************/
static void
ShowMyRedirections(void)
{
    int driveCount;
    uint16 redirIndex, deviceParam, ccode;
    uint8 deviceType;

    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];

    redirIndex = 0;
    driveCount = 0;

    while ((ccode = GetRedirection(redirIndex, deviceStr, resourceStr,
                           &deviceType, &deviceParam)) == CC_SUCCESS) {
      /* only print disk redirections here */
      if (deviceType == REDIR_DISK_TYPE) {
        if (driveCount == 0) {
          printf("Current Drive Redirections:\n");
        }
        driveCount++;
        printf("%-2s = %-20s ", deviceStr, resourceStr);

        /* read attribute is returned in the device parameter */
        if (deviceParam & 0x80) {
	  if ((deviceParam & 0x7f) > 1)
	    printf("CDROM:%d ", (deviceParam & 0x7f) - 1);
          switch ((deviceParam & 0x7f) != 0) {
            case READ_ONLY_DRIVE_ATTRIBUTE:
              printf("attrib = READ ONLY\n");
              break;
            default:
              printf("attrib = READ/WRITE\n");
              break;
          }
        }
      }

      redirIndex++;
    }

    if (driveCount == 0) {
      printf("No drives are currently redirected to Linux.\n");
    }
}

static void
DeleteDriveRedirection(char *deviceStr)
{
    uint16 ccode;

    /* convert device string to upper case */
    strupr(deviceStr);
    ccode = CancelRedirection(deviceStr);
    if (ccode) {
      printf("Error %x (%s)\ncanceling redirection on drive %s\n",
             ccode, decode_DOS_error(ccode), deviceStr);
      }
    else {
      printf("Redirection for drive %s was deleted.\n", deviceStr);
    }
}

static int FindRedirectionByDevice(char *deviceStr, char *resourceStr)
{
    uint16 redirIndex = 0, deviceParam, ccode;
    uint8 deviceType;
    char dStr[MAX_DEVICE_STRING_LENGTH];
    char dStrSrc[MAX_DEVICE_STRING_LENGTH];

    snprintf(dStrSrc, MAX_DEVICE_STRING_LENGTH, "%s", deviceStr);
    strupr(dStrSrc);
    while ((ccode = GetRedirection(redirIndex, dStr, resourceStr,
                           &deviceType, &deviceParam)) == CC_SUCCESS) {
      if (strcmp(dStrSrc, dStr) == 0)
        break;
      redirIndex++;
    }

    return ccode;
}

static int FindFATRedirectionByDevice(char *deviceStr, char *resourceStr)
{
    struct DINFO *di;
    char *dir;
    fatfs_t *f;
    unsigned long serial;
    if (!(di = (struct DINFO *)lowmem_alloc(sizeof(struct DINFO))))
	return 0;
    LWORD(eax) = 0x6900;
    LWORD(ebx) = toupper(deviceStr[0]) - 'A' + 1;
    REG(ds) = FP_SEG32(di);
    LWORD(edx) = FP_OFF32(di);
    call_msdos();
    if (REG(eflags) & CF) {
	lowmem_free((void *)di, sizeof(struct DINFO));
	printf("error retrieving serial, %#x\n", LWORD(eax));
	return -1;
    }
    serial = READ_DWORD(&di->serial);
    lowmem_free((void *)di, sizeof(struct DINFO));
    f = get_fat_fs_by_serial(serial);
    if (!f || !(dir = f->dir)) {
	printf("error identifying FAT volume\n");
	return -1;
    }
    strcpy(resourceStr, "LINUX\\FS");
    strcat(resourceStr, dir);
    return CC_SUCCESS;
}

/********************************************
 * Check wether we are running DosC (FreeDos)
 * and check wether this version can cope with redirection
 * ON ENTRY:
 *  nothing
 * ON EXIT:
 *  returns 0 if not running DosC
 *  otherwise returns the DosC 'build' number
 *
 ********************************************/
/* no longer used -- Bart */
#if 0
static uint16 CheckForDosc(void)
{
    struct REGPACK preg = REGPACK_INIT;

    preg.r_ax = 0xdddc;
    dos_helper_r(&preg);

    if (preg.r_ax == 0xdddc) {
      return 0;
    }
    else {
      return (preg.r_bx);
    }
}
#endif

int lredir_main(int argc, char **argv)
{
    uint16 ccode;
    uint16 deviceParam;
    int carg;
#if 0
    unsigned long dversion;
#endif

    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];


    /* initialize the MFS, just in case the user didn't run EMUFS.SYS */
    InitMFS();

    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
      ShowMyRedirections();
      return(0);
    }

    /* tej one parm is either error or HELP/-help etc */
    if (argc == 2 && strncmpi(argv[1], KEYWORD_HELP, KEYWORD_HELP_COMPARE_LENGTH) == 0) {
      printf("Usage: LREDIR [[drive:] LINUX\\FS\\path [R] | [C [n]] | HELP]\n");
      printf("Redirect a drive to the Linux file system.\n\n");
      printf("LREDIR X: LINUX\\FS\\tmp\n");
      printf("  Redirect drive X: to /tmp of Linux file system for read/write\n");
      printf("  If R is specified, the drive will be read-only\n");
      printf("  If C is specified, (read-only) CDROM n is used (n=1 by default)\n");
      printf("  ${home} represents user's home directory\n\n");
      printf("  If drive is not specified, the next available drive will be used.");
      printf("LREDIR X: Y:\n");
      printf("  Redirect drive X: to where the drive Y: is redirected.\n");
      printf("  If F is specified, the path for Y: is taken from its emulated "
    	     "FAT volume.\n\n");
      printf("LREDIR DEL drive:\n");
      printf("  delete a drive redirection\n\n");
      printf("LREDIR\n");
      printf("  show current drive redirections\n\n");
      printf("LREDIR HELP\n");
      printf("  show this help screen\n");
      return(0);
    }

    if (strncmpi(argv[1], KEYWORD_DEL, KEYWORD_DEL_COMPARE_LENGTH) == 0) {    
      DeleteDriveRedirection(argv[2]);
      return(0);
    }

    /* assume the command is to redirect a drive */
    /* read the drive letter and resource string */
    carg = 3;
    if (argc > 2 && argv[2][1] == ':') {
      /* lredir c: d: */
      strcpy(deviceStr, argv[1]);
      if ((argc > 3 && toupper(argv[3][0]) == 'F') ||
    	((ccode = FindRedirectionByDevice(argv[2], resourceStr)) != CC_SUCCESS)) {
        if ((ccode = FindFATRedirectionByDevice(argv[2], resourceStr)) != CC_SUCCESS) {
          printf("Error: unable to find redirection for drive %s\n", argv[2]);
	  goto MainExit;
	}
      }
    } else {
      if (argc > 1 && argv[1][1] != ':') {
	int nextDrive;
        strcpy(resourceStr, argv[1]);
	nextDrive = find_drive(resourceStr);
	if (nextDrive == -26) {
		printf("Cannot redirect (maybe no drives available).");
		return(0);
	} else if (nextDrive == -27) {
		printf("Cannot canonicalize drive root path.\n");
		return(0);
	}
        deviceStr[0] = -nextDrive + 'A';
	deviceStr[1] = ':';
	deviceStr[2] = '\0';
	carg = 2;
      } else if (argc > 2) {
        strcpy(deviceStr, argv[1]);
        strcpy(resourceStr, argv[2]);
      }
    }
    deviceParam = DEFAULT_REDIR_PARAM;

    if (argc > carg) {
      if (toupper(argv[carg][0]) == 'R') {
        deviceParam = 1;
      }
      if (toupper(argv[carg][0]) == 'C') {
	int cdrom = 1;
	if (argc > carg+1 && argv[carg+1][0] >= '1' && argv[carg+1][0] <= '4')
	  cdrom = argv[carg+1][0] - '0';
        deviceParam = 1 + cdrom;
      }
    }

    /* upper-case both strings */
    strupr(deviceStr);
    strupr(resourceStr);

    /* now actually redirect the drive */
    ccode = RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE,
                           deviceParam);

    /* duplicate redirection: try to reredirect */
    if (ccode == 0x55) {
      DeleteDriveRedirection(deviceStr);     
      ccode = RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE,
                             deviceParam);
    }

    if (ccode) {
      printf("Error %x (%s)\nwhile redirecting drive %s to %s\n",
             ccode, decode_DOS_error(ccode), deviceStr, resourceStr);
      goto MainExit;
    }
    if ((argc <= 2 || argv[2][1] != ':') && argc >= 2 && argv[1][1] != ':')
	msetenv("DOSEMU_LASTREDIR", deviceStr);

    printf("%s = %s", deviceStr, resourceStr);
    if (deviceParam > 1)
      printf(" CDROM:%d", deviceParam - 1);
    printf(" attrib = ");
    switch (deviceParam != 0) {
      case READ_ONLY_DRIVE_ATTRIBUTE:
        printf("READ ONLY\n");
        break;
      default:
        printf("READ/WRITE\n");
        break;
    }

MainExit:
    return (ccode);
}
