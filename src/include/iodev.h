/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _EMU_IODEV_H
#define _EMU_IODEV_H

#include <features.h>
#if GLIBC_VERSION_CODE >= 2000
#include <sys/time.h>
#else
#include <linux/time.h>
#endif
#if 0
#include "emu_defs.h"
#include "keyb.h"
#endif
#include "port.h"
#include "timers.h"

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

typedef struct {
  Bit16u         read_state;
  Bit16u         write_state;
  Bit8u          mode, outpin;
  Bit32u         read_latch;
  Bit16u         write_latch;
  Bit32s         cntr;
  hitimer_u	 time;
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
extern void  rtc_update (void);

/*******************************************************************
 * CMOS support                                                    *
 *******************************************************************/

#include "cmos.h"

extern unsigned long last_ticks;		/* for int1a */
extern long  sys_base_ticks;		/* signed, could go negative */
extern long  usr_delta_ticks;


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
