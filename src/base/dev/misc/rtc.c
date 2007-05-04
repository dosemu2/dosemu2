/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * SIDOC_BEGIN_MODULE
 *
 * Description: CMOS handling and RTC emulation
 * 
 * Maintainers: Alberto Vignani (vignani@tin.it)
 *
 * SIDOC_END_MODULE
 *
 */

#include <time.h>
#include <sys/time.h>

#include "emu.h"
#include "cmos.h"
#include "disks.h"
#include "timers.h"
#include "int.h"
#include "iodev.h"

long   sys_base_ticks = 0;
long   usr_delta_ticks = 0;
unsigned long   last_ticks = 0;
static unsigned long long q_ticks_m = 0;


static int rtc_get_rate(Bit8u div)
{
  if (!div)
    return 0;
  if (div < 3)
    div += 7;
  return (65536 >> div);
}

void rtc_run(void)
{
  static hitimer_t last_time = -1;
  int rate;
  hitimer_t ticks_m, cur_time = GETusTIME(0);
  if (last_time == -1 || last_time > cur_time) {
    last_time = cur_time;
    return;
  }
  rate = rtc_get_rate(GET_CMOS(CMOS_STATUSA) & 0x0f);
  ticks_m = (cur_time - last_time) * rate;
  q_ticks_m += ticks_m;
  last_time = cur_time;
  if (debug_level('h') > 8)
    h_printf("RTC: A=%hhx B=%hhx C=%hhx rate=%i queued=%lli added=%lli\n",
	GET_CMOS(CMOS_STATUSA), GET_CMOS(CMOS_STATUSB), GET_CMOS(CMOS_STATUSC),
	rate, (long long)q_ticks_m, (long long)ticks_m);
  if (q_ticks_m >= 1000000) {
    Bit8u old_c = GET_CMOS(CMOS_STATUSC);
    SET_CMOS(CMOS_STATUSC, old_c | 0x40);
    if ((GET_CMOS(CMOS_STATUSB) & 0x40) && !(GET_CMOS(CMOS_STATUSC) & 0x80)) {
      SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC) | 0x80);
      if (debug_level('h') > 7)
        h_printf("RTC: periodic IRQ, queued=%lli, added=%lli\n",
	    (long long)q_ticks_m, (long long)ticks_m);
      pic_request(PIC_IRQ8);
    }
    if (!(old_c & 0x40))
      q_ticks_m -= 1000000;
  }
}

Bit8u rtc_read(Bit8u reg)
{
  Bit8u ret = GET_CMOS(reg);

  switch (reg) {
  case CMOS_SEC:
  case CMOS_SECALRM:
  case CMOS_MIN:
  case CMOS_MINALRM:
  case CMOS_DOW:
  case CMOS_DOM:
  case CMOS_MONTH:
  case CMOS_YEAR:
  case CMOS_CENTURY:
    /* Note - the inline function BCD() in cmos.h will check bit 2 of
     * status reg B for proper output format */
    ret = BCD(ret);
    break;

  case CMOS_HOUR:		/* RTC hour...bit 1 of 0xb set=24 hour mode, clear 12 hour */
  case CMOS_HOURALRM:
    if (!(GET_CMOS(CMOS_STATUSB) & 2)) {	/* 12-hour mode */
      if (ret == 0)
	ret = 12;
      else if (ret > 12)
	ret -= 12;
    }
    ret = BCD(ret);
    break;

  case CMOS_STATUSC:
    if (debug_level('h') > 8)
      h_printf("RTC: Read C=%hhx\n", ret);
    SET_CMOS(CMOS_STATUSC, 0);
    pic_untrigger(PIC_IRQ8);
    rtc_run();
    break;
  }

  return ret;
}

void rtc_write(Bit8u reg, Bit8u byte)
{
  switch (reg) {
    case CMOS_SEC:
    case CMOS_MIN:
    case CMOS_HOUR:
    case CMOS_SECALRM:
    case CMOS_MINALRM:
    case CMOS_HOURALRM:
    case CMOS_DOW:
    case CMOS_DOM:
    case CMOS_MONTH:
    case CMOS_YEAR:
    case CMOS_CENTURY:
      SET_CMOS(reg, BIN(byte));
      break;

    /* b7=r/o and unused
     * b4-6=always 010 (AT standard 32.768kHz)
     * b0-3=rate [65536/2^v], default 6, min 3, 0=disable
     */
    case CMOS_STATUSA:
      h_printf("RTC: Write %hhx to A\n", byte);
      SET_CMOS(reg, byte & 0x7f);
      break;

    case CMOS_STATUSB:
      h_printf("RTC: Write %hhx to B\n", byte);
      SET_CMOS(reg, byte);
      break;

    case CMOS_STATUSC:
    case CMOS_STATUSD:
      h_printf("RTC: attempt to write %hhx to %hhx\n", byte, reg);
      break;

    default:
      SET_CMOS(reg, byte);
  }
  q_ticks_m = 0;
}

static void rtc_alarm_check (void)
{
  static u_char last_sec = 0xff; /* any invalid value to begin with */
  u_char h0,m0,s0,h,m,s;

  s0=GET_CMOS(CMOS_SEC);
  m0=GET_CMOS(CMOS_MIN);
  h0=GET_CMOS(CMOS_HOUR);

  /* this is a test for equality - we can't just call a lib time
   * function because a second could be skipped */
  h = GET_CMOS(CMOS_HOURALRM);
  if (h>=0xc0 || h0==h) {      /* Eric: all c0-ff are don't care */
    m = GET_CMOS(CMOS_MINALRM);
    if (m>=0xc0 || m0==m) {
      s = GET_CMOS(CMOS_SECALRM);
      if (s>=0xc0 || (s0==s && s0!=last_sec)) {
        last_sec = s0;
        h_printf("RTC: got alarm at %02d:%02d:%02d\n",h,m,s);
        SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC) | 0x20);
        if ((GET_CMOS(CMOS_STATUSB) & 0x20) && !(GET_CMOS(CMOS_STATUSC) & 0x80)) {
	  SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC) | 0x80);
          h_printf("RTC: alarm IRQ\n");
	  pic_request(PIC_IRQ8);
        }
      }
    }
  }
}


void rtc_update (void)	/* called every 1s from SIGALRM */
{
  u_char h0,m0,s0,D0,M0,Y0,C0,days;
  static const u_char dpm[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (GET_CMOS(CMOS_STATUSA) & 0x80)
    error("RTC: A/UIP set on update\n");

  if (GET_CMOS(CMOS_STATUSB) & 0x80) {
    h_printf("RTC updates inhibited\n");
    return;
  }

  SET_CMOS(CMOS_STATUSA, GET_CMOS(CMOS_STATUSA)|0x80);
  s0=GET_CMOS(CMOS_SEC);
  m0=GET_CMOS(CMOS_MIN);
  h0=GET_CMOS(CMOS_HOUR);
  D0=GET_CMOS(CMOS_DOM);
  M0=GET_CMOS(CMOS_MONTH);
  Y0=GET_CMOS(CMOS_YEAR);
  C0=GET_CMOS(CMOS_CENTURY);

  if ((++s0)>59) {
    s0=0;
    if ((++m0)>59) {
      m0=0;
      if ((++h0)>23) {
       h0=0;

       /* Compute days in current month. */
       if (M0>12 || M0<1) M0=1;    /* Error! */
       days = dpm[M0];
       if ((Y0&3) == 0 && M0 == 2) days++; /* Leap year & Feb */

       if (++D0>days) {
        D0=1;
        if (++M0>12) {
         M0=1;
         if (++Y0>99) {
          Y0=0;
          ++C0;   /* Only 19->20 transition realistic. */
          SET_CMOS(CMOS_CENTURY, C0);
         }
         SET_CMOS(CMOS_YEAR, Y0);
        }
        SET_CMOS(CMOS_MONTH, M0);
       }
       SET_CMOS(CMOS_DOM, D0);

       /* As well as day-of-month, do day-of-week */
       days=GET_CMOS(CMOS_DOW);
       days++;
       if(days > 7 || days < 1) days=1;
       SET_CMOS(CMOS_DOW, days);
      }
      SET_CMOS(CMOS_HOUR, h0);
    }
    SET_CMOS(CMOS_MIN, m0);
  }
  SET_CMOS(CMOS_SEC, s0);

  h_printf("RTC: cmos update %02d:%02d:%02d, B=%#x\n",h0,m0,s0,GET_CMOS(CMOS_STATUSB));

  rtc_alarm_check();

  /* per-second IRQ */
  SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC) | 0x10);
  if ((GET_CMOS(CMOS_STATUSB) & 0x10) && !(GET_CMOS(CMOS_STATUSC) & 0x80)) {
    SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC) | 0x80);
    h_printf("RTC: update IRQ\n");
    pic_request(PIC_IRQ8);
  }

  SET_CMOS(CMOS_STATUSA, GET_CMOS(CMOS_STATUSA)&~0x80);
}

/*
 * Initialise the RTC. Key work is now in get_linux_ticks() where we initialise
 * all of the RTC time/date fields, and only zero the alarm stuff here.
 */
void rtc_init (void)
{
  usr_delta_ticks = 0;
  last_ticks = sys_base_ticks = get_linux_ticks(1, NULL);

  SET_CMOS(CMOS_HOURALRM, 0);
  SET_CMOS(CMOS_MINALRM,  0);
  SET_CMOS(CMOS_SECALRM,  0);
}


/* ========================================================================= */

/* > get_linux_ticks.c
 *
 * 1.03 arb Fri Jul 22 19:13:03 BST 2005 - removed config options for logging
 *      suspicious time jumps and using re-entrant-safe calls.
 * 1.02 arb Fri Aug  6 13:08:21 BST 2004 - tidied up ifdefs
 * 1.01 arb Mon Jun 14 17:50:00 BST 2004 - added timelog
 *
 * Copyright (c) 2004 Paul S. Crawford, All Rights Reserved
 *
 */

/*
 * DANG_BEGIN_MODULE
 *
 * Description: Convert LINUX time to DOS / RTC time.
 *
 * Maintainers: Paul Crawford
 *
 * REMARK
 * This reads the LINUX time and converts it to the DOS ticks per day
 * value, and if necessary sets the RTC registers to match. It attempts
 * to put all LINUX->DOS time conversion to one function. Used for RTC init
 * and for the INT-1A time calls.
 * /REMARK
 * DANG_END_MODULE
 *
 */
/*
 * Configuration:
 * Define USE_PIT_TICK_RATE to use the PIT tick rate.
 */
#undef  USE_PIT_TICK_RATE

/*
 * Safety check to make sure 'freedos' is not supplied with a tick value greater than one
 * day (max = 86399.99 seconds) with its not quite correct 1193180 PIC rate (should be
 * 1193182 I gather). Correct value should be 1573042, so a difference of about 0.15s
 * just before midnight.
 *
 */

#define FREEDOS_TICKS_IN_A_DAY 1573039

/*
 * This function attempts to place all code for converting LINUX system time to DOS 'ticks'
 * and/or the CMOS RTC time in one place. As DOS is based on local time (stupid, I know) it
 * converts the LINUX/UNIX linear UTC time_t in to local time values and from these it computes
 * the corresponding number of DOS ~18.2Hz interrupt 'ticks' in the current day. If you want/need
 * UTC in DOS then start dosemu with a TZ=GMT0 environment variable.
 *
 * As the DOS notion of time is based on interrupts per day, and a day counter when this
 * overflows at midnight, this function can also supply the roll-over flag based on the day
 * having changed since the last time it was called WITH THE FLAG REQUESTED. This allows it
 * to be used for both the INT-1A AH=0 function (get ticks) and the AH=2 or 4 (get RTC time/date)
 * without confusing the once per day roll-over count.
 *
 * In addition, should the LINUX time go backwards over midnight (very unfortunate occurance, but
 * it is just possible if ntpdate was used to get things correct at once, rather then the ntm deamon
 * running to slew time to correctness slowly) it will hold the DOS notion of time at 00:00:00
 * until the LINUX time crosses the midnight boundary again, but without producing two day counts.
 *
 * The calling arguments are:
 *
 *	set_cmos		: Non-zero to initialise the CMOS RTC date/time fields. Otherwise RTC unchanged.
 *
 *	day_rollover	: Non-NULL pointer if checking for the day crossing for INT-1A AH=0 use.
 *
 * The return value is the DOS 'ticks' from 00:00:00 for this day.
 * An error results in -1 retun (e.g. from problems converting time, perhaps bad TZ environment).
 *
 * NOTE: A check in the freedos source shows a *slightly* different value for the nominal tick count
 * spanning one day, compared to the PIT_TICK_RATE value in timers.h (which is correct, I think). So
 * if you compile with the freedos values for 100% time conversion compatability with it, that is fine
 * but if you compile with the 'correct' PIT_TICK_RATE value, it limits it near midnight so freedos
 * never gets a value corresponding equal to, or more than, 24 hours.
 *
 * NOTE: When setting the RTC it will do this in local time, with any "daylight saving" shift applied,
 * but it makes no attempt to set the DST flag in the RTC. That is a horrible idea anyway, and I do
 * not see this as being a big problem for anyone.
 *
 * Finally, if time matters for anything important - set it to UTC and set TZ=GMT0 !
 *
 */
unsigned long get_linux_ticks(int set_cmos, int *day_rollover)
{
	static long last_ds70 = 0;	/* For day changing test. */

	long ds70, isec;
	long long tt;	/* Needs > 32 bits for intermediate product! */
	static long long last_lint = 0;
	static struct timeval prev_tv;
	long long lint;
	struct timeval tv;
	int year;
	struct tm *tm;

	gettimeofday(&tv, NULL);
	
	lint = ((long long)(unsigned long)tv.tv_sec) * 1000;
	lint += tv.tv_usec / 1000;
				
	last_lint = lint;
	prev_tv = tv;

	tm = localtime(&tv.tv_sec);
	if(tm == NULL) return -1;

	year = 1900 + tm->tm_year;
	isec = tm->tm_sec + 60 * (tm->tm_min + 60 * tm->tm_hour);	/* Seconds in day. */
	ds70 = tm->tm_yday + (year-1970)*365L + (year-1969)/4;		/* Days from 1970 */
	if(last_ds70 == 0) last_ds70 = ds70;

	/* Compute time of day in 1/100 second units, then to DOS ticks. */
	tt = isec * 100 + (tv.tv_usec / 10000);

#if USE_PIT_TICK_RATE
	tt = (tt * PIT_TICK_RATE) / 6553600;	/* Correct as per LINUX code, 44-bit intermediate product.*/
	if(tt > FREEDOS_TICKS_IN_A_DAY) tt = FREEDOS_TICKS_IN_A_DAY;
#else
	tt = (tt * 59659) / 327680;				/* Use same magic numbers as freedos: ke2033/kernel/sysclk.c */
#endif

	if(day_rollover != NULL)
	{
		/* Here we deal with INT-1A AH=0 calls. */
		long day_diff = ds70 - last_ds70;
		*day_rollover = 0;

		if(day_diff < 0)
		{
			/* Day change has us go back over midnight - keep 00:00:00 on same day until time caught up */
			tt = 0;
		}
		else if(day_diff > 0)
		{
			/* Some DOS assume incrementing flag, others only set. Here we return days passed. */
			if(day_diff > 255) day_diff = 255;
			*day_rollover = (int)day_diff;
			last_ds70 = ds70;	/* Re-set day only if flag requested & forward time. */
		}
	}

	if(set_cmos)
	{
		/* Initialise the date settings of the RTC (CMOS clock) */
		int cent;
		SET_CMOS(CMOS_HOUR, tm->tm_hour);
		SET_CMOS(CMOS_MIN, tm->tm_min);
		SET_CMOS(CMOS_SEC, tm->tm_sec);

		SET_CMOS(CMOS_DOW, tm->tm_wday + 1);
		SET_CMOS(CMOS_DOM, tm->tm_mday);
		SET_CMOS(CMOS_MONTH, tm->tm_mon + 1);

		cent = year/100;
		SET_CMOS(CMOS_YEAR, year - 100*cent);	/* mod 100 */
		SET_CMOS(CMOS_CENTURY, cent);
		/* Really - we don't want to consider daylight saving in the RTC! */
	}

	return (unsigned long)tt;
}
