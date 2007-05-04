/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* cmos.h, for DOSEMU
 *   by Robert Sanders, gt8134b@prism.gatech.edu
 *
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

void cmos_write(ioport_t, Bit8u), cmos_init(void), cmos_reset(void);
Bit8u cmos_read(ioport_t);
Bit8u rtc_read(Bit8u reg);
void rtc_write(Bit8u reg, Bit8u val);
void rtc_run(void);

struct CMOS {
  Bit8u subst[64];
  Bit8u flag[64];
  int address;
};

EXTERN struct CMOS cmos;

#define SET_CMOS(byte,val)  do { cmos.subst[byte&63] = (val); cmos.flag[byte&63] = 1; } while(0)
#define GET_CMOS(byte)		 (cmos.subst[byte&63])

static __inline__ Bit8u BCD(Bit8u binval)
{
  /* bit 2 of register 0xb set=binary mode, clear=BCD mode */
  if (GET_CMOS(CMOS_STATUSB) & 4) return binval;
  if (binval > 99) return 0x99;
  return (((binval/10) << 4) | (binval%10));
}

static __inline__ Bit8u BIN(Bit8u bcdval)
{
  Bit8u h,l;
  if (((l=(bcdval&0x0f))>9)||((h=(bcdval&0xf0))>0x90)) return 0xff;
  return ((h>>1)+(h>>3)+l);	/* h*10/16+l */
}

#endif
