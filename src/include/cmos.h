/* cmos.h, for DOSEMU
 *   by Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/06/12 23:15:37 $
 * $Source: /usr/src/dosemu0.60/include/RCS/cmos.h,v $
 * $Revision: 2.1 $
 * $State: Exp $
 */

#ifndef CMOS_H
#define CMOS_H

#include "extern.h"

#define CMOS_SEC	0	/* RTC time: seconds */
#define CMOS_SECALRM	1	/* Alarm time: seconds */
#define CMOS_MIN	2	/* RTC time: minutes */
#define CMOS_MINALRM	3	/* Alarm time: minute */
#define CMOS_HOUR	4	/* RTC time: hours */
#define CMOS_HOURALRM	5	/* Alarm time: hour */
#define CMOS_DOW	6	/* Day of Week */
#define CMOS_DOM	7	/* Day of month */
#define CMOS_MONTH	8	/* Month */
#define CMOS_YEAR	9	/* Year */
#define CMOS_STATUSA	0xA	/* Status A */
#define CMOS_STATUSB	0xB	/* Status B */
#define CMOS_STATUSC	0xC	/* Status C */
#define CMOS_STATUSD	0xD	/* Status D */
#define CMOS_DIAG	0xE	/* Diagnostic status byte */
#define CMOS_SHUTDOWN	0xF	/* shutdown status byte */
#define CMOS_DISKTYPE	0x10	/* floppy type byte */
#define CMOS_RESV1	0x11
#define CMOS_HDTYPE	0x12	/* drive type byte (4 bits per drive, 0 for see extwords) */
#define CMOS_RESV2	0x13
#define CMOS_EQUIPMENT	0x14	/* equipment byte */
#define CMOS_BASEMEML	0x15	/* base memory in 1k blocks */
#define CMOS_BASEMEMM	0x16
#define CMOS_EXTMEML	0x17	/* extended memory in 1k blocks */
#define CMOS_EXTMEMM	0x18
#define CMOS_HD1EXT	0x19	/* drive C: extension byte */
#define CMOS_HD2EXT	0x1a	/* drive D: extension byte */
#define CMOS_RESV3	0x1b
#define CMOS_CHKSUMM	0x2e	/* checksum over 0x10-0x20 */
#define CMOS_CHKSUML	0x2f
#define CMOS_PEXTMEML	0x30	/* extended memory over 1 MB found during POST */
#define CMOS_PEXTMEMM	0x31
#define CMOS_CENTURY	0x32
#define CMOS_INFO	0x33
#define CMOS_RESV4	0x34	/* 12 bytes reserved */

void cmos_write(int, int), cmos_init(void), cmos_reset(void);
int cmos_read(int);
int cmos_date(int);

struct CMOS {
  unsigned char subst[64];
  unsigned char flag[64];
  int address;
};

EXTERN struct CMOS cmos;

#endif
