/***********************************************
 * File: LREDIR.C
 *  Program for Linux DOSEMU disk redirector functions
 * Written: 10/29/93 by Tim Bird
 * Fixes: 1/7/95 by Tim Josling TEJ: check correct # args
 *      :                            display read/write status correctly
 * Changes: 3/19/95 by Kang-Jin Lee
 *  Removed unused variables
 *  Changes to eliminate warnings
 *  Display read/write status when redirecting drive
 *  Usage information is more verbose
 *
 * Changes: 11/27/97 Hansl Lermen
 *  new detection stuff,
 *  rewrote to use plain C and intr(), instead of asm and fiddling with
 *  (maybe used) machine registers.
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
#include <dos.h>      /* geninterrupt and MK_FP */
#include <stdio.h>    /* printf  */
#include <stdlib.h>   /* exit    */
#include <string.h>
#include "detect.h"

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

#define DEFAULT_REDIR_PARAM   0

#define DOS_HELPER_INT          0xE6


char far *
GetListOfLists(void)
{
    char far *LOL;
    struct REGPACK preg;

    preg.r_ax = DOS_GET_LIST_OF_LISTS;
    intr(0x21, &preg);
    LOL = MK_FP(preg.r_es, preg.r_bx);
    return (LOL);
}

char far *
GetSDAPointer(void)
{
    char far *SDA;
    struct REGPACK preg;

    preg.r_ax = DOS_GET_SDA_POINTER;
    intr(0x21, &preg);
    SDA = MK_FP(preg.r_ds, preg.r_si);

    return (SDA);
}

/********************************************
 * InitMFS - call Emulator to initialize MFS
 ********************************************/
/* tej - changed return type to void as nothing returned */
void InitMFS(void)
{
    char far *LOL;
    char far *SDA;
    struct REGPACK preg;

    LOL = GetListOfLists();
    SDA = GetSDAPointer();

    /* get DOS version into CX */
    preg.r_cx = _osmajor | (_osminor <<8);

    preg.r_dx = FP_OFF(LOL);
    preg.r_es = FP_SEG(LOL);
    preg.r_si = FP_OFF(SDA);
    preg.r_ds = FP_SEG(SDA);
    preg.r_bx = 0x500;
    preg.r_ax = 0x20;
    intr(DOS_HELPER_INT, &preg);
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
uint16 RedirectDevice(char *deviceStr, char *resourceStr, uint8 deviceType,
                      uint16 deviceParameter)
{
    char slashedResourceStr[MAX_RESOURCE_PATH_LENGTH];
    struct REGPACK preg;

    /* prepend the resource string with slashes */
    strcpy(slashedResourceStr, "\\\\");
    strcat(slashedResourceStr, resourceStr);

    /* should verify strings before sending them down ??? */
    preg.r_ds = FP_SEG(deviceStr);
    preg.r_si = FP_OFF(deviceStr);
    preg.r_es = FP_SEG(slashedResourceStr);
    preg.r_di = FP_OFF(slashedResourceStr);
    preg.r_cx = deviceParameter;
    preg.r_bx = deviceType;
    preg.r_ax = DOS_REDIRECT_DEVICE;
    intr(0x21, &preg);

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
uint16 GetRedirection(uint16 redirIndex, char *deviceStr, char *resourceStr,
                      uint8 * deviceType, uint16 * deviceParameter)
{
    uint16 ccode;
    uint8 deviceTypeTemp;
    char slashedResourceStr[MAX_RESOURCE_PATH_LENGTH];
    struct REGPACK preg;

    preg.r_ds = FP_SEG(deviceStr);
    preg.r_si = FP_OFF(deviceStr);
    preg.r_es = FP_SEG(slashedResourceStr);
    preg.r_di = FP_OFF(slashedResourceStr);
    preg.r_bx = redirIndex;
    preg.r_ax = DOS_GET_REDIRECTION;
    intr(0x21, &preg);

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
uint16 CancelRedirection(char *deviceStr)
{
    struct REGPACK preg;

    preg.r_ds = FP_SEG(deviceStr);
    preg.r_si = FP_OFF(deviceStr);
    preg.r_ax = DOS_CANCEL_REDIRECTION;
    intr(0x21, &preg);

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
void
ShowMyRedirections(void)
{
    int driveCount;
    uint16 redirIndex, deviceParam, ccode;
    uint8 deviceType;

    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];

    redirIndex = 0;
    driveCount = 0;

    ccode = GetRedirection(redirIndex, deviceStr, resourceStr,
                           &deviceType, &deviceParam);

    while (ccode == CC_SUCCESS) {
      /* only print disk redirections here */
      if (deviceType == REDIR_DISK_TYPE) {
        if (driveCount == 0) {
          printf("Current Drive Redirections:\n");
        }
        driveCount++;
        printf("%-2s = %-20s ", deviceStr, resourceStr);

        /* read attribute is returned in the device parameter */
        if (deviceParam & 0x80) {
          switch (deviceParam & 0x7f) { /* tej 1/7/95 */
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
      ccode = GetRedirection(redirIndex, deviceStr, resourceStr,
                             &deviceType, &deviceParam);
    }

    if (driveCount == 0) {
      printf("No drives are currently redirected to Linux.\n");
    }
}

void
DeleteDriveRedirection(char *deviceStr)
{
    uint16 ccode;

    /* convert device string to upper case */
    strupr(deviceStr);
    ccode = CancelRedirection(deviceStr);
    if (ccode) {
      printf("Error %x canceling redirection on drive %s\n",
             ccode, deviceStr);
      }
    else {
      printf("Redirection for drive %s was deleted.\n", deviceStr);
    }
}

main(int argc, char **argv)
{
    uint16 ccode;
    uint16 deviceParam;

    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];

    if (!is_dosemu()) {
      printf("This program requires DOSEMU to run, aborting\n");
      exit(1);
    }

    /* initialize the MFS, just in case the user didn't run EMUFS.SYS */
    InitMFS();

    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
      ShowMyRedirections();
      exit(0);
    }

    /* tej one parm is either error or HELP/-help etc */
    if (argc == 2) {
      printf("Usage: LREDIR [drive: LINUX\\FS\\path [R] | HELP]\n");
      printf("Redirect a drive to the Linux file system.\n\n");
      printf("LREDIR X: LINUX\\FS\\tmp\n");
      printf("  Redirect drive X: to /tmp of Linux file system for read/write\n");
      printf("  If R is specified, the drive will be read-only\n");
      printf("  ${home} represents user's home directory\n\n");
      printf("LREDIR DEL drive:\n");
      printf("  delete a drive redirection\n\n");
      printf("LREDIR\n");
      printf("  show current drive redirections\n\n");
      printf("LREDIR HELP\n");
      printf("  show this help screen\n");
      exit(0);
    }

    if (strncmpi(argv[1], KEYWORD_DEL, KEYWORD_DEL_COMPARE_LENGTH) == 0) {
      DeleteDriveRedirection(argv[2]);
      exit(0);
    }

    /* assume the command is to redirect a drive */
    /* read the drive letter and resource string */
    strcpy(deviceStr, argv[1]);
    strcpy(resourceStr, argv[2]);
    deviceParam = DEFAULT_REDIR_PARAM;

    if (argc > 3) {
      if (toupper(argv[3][0]) == 'R') {
        deviceParam = 1;
      }
    }

    /* upper-case both strings */
    strupr(deviceStr);
    strupr(resourceStr);

    /* now actually redirect the drive */
    ccode = RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE,
                           deviceParam);

    if (ccode) {
      printf("Error %x redirecting drive %s to %s\n",
             ccode, deviceStr, resourceStr);
      goto MainExit;
    }

    printf("%s = %s  attrib = ", deviceStr, resourceStr);
    switch (deviceParam) {
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
