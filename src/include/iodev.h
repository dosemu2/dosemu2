/*
 * $Date: $
 * $Source: $
 * $Revision: $
 * $State: $
 *
 * $Log: $
 */

#ifndef _EMU_IODEV_H
#define _EMU_IODEV_H

#include <linux/time.h>
#if 0
#include "emu_defs.h"
#include "keyb.h"
#endif
#include "port.h"

#include "lpt.h"
#include "pic.h"

/*******************************************************************
 * Global initialization routines                                  *
 *******************************************************************/

extern void iodev_init(void);
extern void iodev_reset(void);
extern void iodev_term(void);

/*******************************************************************
 * Programmable Interrupt Timer (PIT) chip                         *
 *******************************************************************/

#define PIT_TICK_RATE   1193180      /* underlying clock rate in HZ */
#define PIT_TICK_1000   1193         /* PIT tick rate / 1000        */

#define PIT_TIMERS      4            /* 4 timers (w/opt. at 0x44)   */

typedef struct {
  Bit16u         read_state;
  Bit16u         write_state;
  Bit16u         mode;
  Bit32u         read_latch;
  Bit16u         write_latch;
  Bit32u         cntr;
  struct timeval time;
} pit_latch_struct;

EXTERN pit_latch_struct pit[PIT_TIMERS];

extern void  pit_init(void);
extern void  pit_reset(void);

/*******************************************************************
 * Real Time Clock (RTC) chip                                      *
 *******************************************************************/

#define RTC_SEC       0
#define RTC_SECALRM   1
#define RTC_MIN       2
#define RTC_MINALRM   3
#define RTC_HOUR      4
#define RTC_HOURALRM  5
#define RTC_DOW       6
#define RTC_DOM       7
#define RTC_MON       8
#define RTC_YEAR      9
#define RTC_REGA      0xa
#define RTC_REGB      0xb
#define RTC_REGC      0xc
#define RTC_REGD      0xd

#define SECS_PER_MIN     60
#define SECS_PER_HOUR    3600
#define SECS_PER_DAY     86400
#define SECS_PER_MONTH   2592000     /* yes, I know, it's not right */
#define SECS_PER_YEAR    31557600    /* any trivia buffs? */

extern void  rtc_init(void);
extern void  rtc_reset(void);
extern Bit8u rtc_read(Bit32u port);
extern void  rtc_write(Bit32u port, Bit8u byte);

/*******************************************************************
 * CMOS support                                                    *
 *******************************************************************/

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

extern void  cmos_init(void);
extern void  cmos_reset(void);

/*******************************************************************
 * PIC support                                                     *
 *******************************************************************/

extern void  pic_init(void);
extern void  pic_reset(void);

/*******************************************************************
 * Dummy hardware support stubs                                    *
 *******************************************************************/

extern void  dma_init(void);
extern void  dma_reset(void);

extern void  pos_init(void);
extern void  pos_reset(void);

extern void  hdisk_init(void);
extern void  hdisk_reset(void);

extern void  floppy_init(void);
extern void  floppy_reset(void);

#endif /* _EMU_IODEV_H */
