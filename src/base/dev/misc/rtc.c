/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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


int cmos_date(int reg)
{
  unsigned char tmp;

  switch (reg) {
  case CMOS_SEC:
  case CMOS_MIN:
  case CMOS_DOW:
  case CMOS_DOM:
  case CMOS_MONTH:
  case CMOS_YEAR:
  case CMOS_CENTURY:
    /* Note - the inline function BCD() in cmos.h will check bit 2 of
     * status reg B for proper output format */
    return BCD(GET_CMOS(reg));

  case CMOS_HOUR:		/* RTC hour...bit 1 of 0xb set=24 hour mode, clear 12 hour */
    tmp = GET_CMOS(reg);	/* stored internally as bin, not BCD */
    if (!(GET_CMOS(CMOS_STATUSB) & 2)) {	/* 12-hour mode */
      if (tmp == 0)
	return BCD(12);
      else if (tmp > 12)
	return BCD(tmp-12);
    }
    return BCD(tmp);

  default:
    h_printf("CMOS: cmos_time() register 0x%02x defaulted to 0\n", reg);
    return 0;
  }

}
/* @@@ MOVE_END @@@ 32768 */


void rtc_int8(int ilevel)	/* int70 */
{
  r_printf("RTC: interrupt\n");
  do_irq(ilevel);
  LOCK_CMOS;
  SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC)&~0x80);
  UNLOCK_CMOS;
}


static void rtc_alarm_check (void)
{
  static u_char last_sec = 0xff; /* any invalid value to begin with */
  u_char a,h0,m0,s0,h,m,s;

  a = GET_CMOS(CMOS_STATUSB); if (a&0x80) return;
  r_printf("RTC: alarm check e=%#x\n",a);
  s0=GET_CMOS(CMOS_SEC);
  m0=GET_CMOS(CMOS_MIN);
  h0=GET_CMOS(CMOS_HOUR);

  if (a&0x10) goto do_alrm;

  if (a&0x20) {
    /* this is a test for equality - we can't just call a lib time
     * function because a second could be skipped */
    h = GET_CMOS(CMOS_HOURALRM);
    if (h==0xff || h0==h) {
      m = GET_CMOS(CMOS_MINALRM);
      if (m==0xff || m0==m) {
        s = GET_CMOS(CMOS_SECALRM);
        if (s==0xff || (s0==s && s0!=last_sec)) {
	  last_sec = s0;
	  r_printf("RTC: got alarm at %02d:%02d:%02d\n",h,m,s);
do_alrm:
	  SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC)|0x80);
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

  LOCK_CMOS;
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
          SET_CMOS(CMOS_CENTURY, D0);
         }
         SET_CMOS(CMOS_YEAR, D0);
        }
        SET_CMOS(CMOS_MONTH, D0);
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

  r_printf("RTC: cmos update %02d:%02d:%02d\n",h0,m0,s0);

  rtc_alarm_check();

  SET_CMOS(CMOS_STATUSA, GET_CMOS(CMOS_STATUSA)&~0x80);
  UNLOCK_CMOS;
}


static struct tm *sys_time_calib (void)
{
  struct tm *tm;
  time_t this_time;
  unsigned long long k=0;	/* gcc warning? */

  time(&this_time);
  tm = localtime(&this_time);

  /* time since last midnight, in ticks */
  k = ((tm->tm_hour * 60 + tm->tm_min) * 60 + tm->tm_sec);
  sys_base_ticks = (k * 227581)/12500;	/* * 18.206 */

  return tm;
}


void rtc_init (void)
{
  struct tm *tm;

  usr_delta_ticks = 0;
  tm = sys_time_calib();
  last_ticks = sys_base_ticks;
  h_printf("RTC: init time %02d:%02d:%02d\n",tm->tm_hour,tm->tm_min,tm->tm_sec);

  LOCK_CMOS;
  SET_CMOS(CMOS_HOUR, tm->tm_hour);
  SET_CMOS(CMOS_MIN,  tm->tm_min);
  SET_CMOS(CMOS_SEC,  tm->tm_sec);

  SET_CMOS(CMOS_HOURALRM, 0);
  SET_CMOS(CMOS_MINALRM,  0);
  SET_CMOS(CMOS_SECALRM,  0);
  UNLOCK_CMOS;
}
