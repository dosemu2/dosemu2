/*
 * SIDOC_BEGIN_MODULE
 *
 * System time routines
 *
 * Functions used to get a monoton, 64-bit time value either from the
 * kernel (with gettimeofday()) or from the CPU hi-res timer
 * Maintainer: vignani@mail.tin.it (Alberto Vignani)
 *
 * SIDOC_END_MODULE
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "emu.h"
#include "video.h"
#include "timers.h"
#include "pic.h"
#include "int.h"
#include "coopth.h"
#include "speaker.h"
#include "dosemu_config.h"
#include "sig.h"

/* --------------------------------------------------------------------- */
/*
 * SIDOC_BEGIN_REMARK
 *
 * At the heart of the timing system in dosemu >= 0.67.11 is the availability
 * of the system time as a 64-bit [type hitimer_t] monoton value.
 * (a 64-bit timer on a 200MHz CPU increments by 2^48 a day).
 *
 * Dosemu needs this time under two resolutions:
 * VERB
 *   - a MICROSECOND resolution for general timing purposes
 *   - a TICK(838ns) resolution for PIT
 * /VERB
 * On non-pentium machines, only the first one is available via the
 * kernel call gettimeofday(). On the pentium and up, the situation is better
 * since we have a cheap hi-res timer on-chip, and worse since this
 * timer runs at a speed depending from the CPU clock (which we need
 * to know/measure, and could be not 100% accurate esp. if the speed is
 * a non-integer multiple of 33.3333).
 *
 * dosemu >= 0.67.11 can use both timing methods (call them 486 and pentium),
 * and switch between them in a dynamic way when configuring.
 *
 * At the first level (local to the file cputime.c) there are the
 * RAW timer functions, addressed by RAWcpuTIME(). These get the
 * actual absolute CPU time in usecs.
 *
 * At the second level, GETcpuTIME() returns the relative, zero-based
 * system time. This is where the 486/pentium switch happens.
 *
 * The third level is the actual timer interface for dosemu and is
 * made of two functions:
 * VERB
 *  - GETusTIME(s)   gives the time in usecs
 *  - GETtickTIME(s) gives the time in ticks
 * /VERB
 * The 's' parameter can be used to control secondary time functions
 * like 'time stretching' (see the READMEs).
 * The function GETusSYSTIME() never activates this stretching, and
 * is used only by the realtime thread-based 1-sec timer in rtc.c.
 *
 * All timing are RELATIVE to a base. The use of a based time allows us
 * to play more freely with time, e.g. stop and restart it during debugging,
 * stretch it, make it go at different speeds between real-time and CPU
 * emulation, etc. The base has been chosen to be zero, because it will
 * avoid overflows in calculations, produce more readable and more easily
 * comparable debug log files, and also because only int0x1a and BIOS
 * timer require knowledge of the actual time, PIT and PIC are not sensitive.
 *
 * SIDOC_END_REMARK
 */

static hitimer_t (*RAWcpuTIME)(void);

hitimer_u ZeroTimeBase = { 0 };
static hitimer_t (*GETcpuTIME)(void);
static hitimer_t LastTimeRead = 0;
static hitimer_t StopTimeBase = 0;
int cpu_time_stop = 0;
static hitimer_t cached_time;
static pthread_mutex_t ctime_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t trigger_mtx = PTHREAD_MUTEX_INITIALIZER;
static int        idle_tid;
static void idle_hlt_thr(void *arg);

static hitimer_t do_gettime(void)
{
  struct timespec tv;
#ifdef CLOCK_MONOTONIC_COURSE
  int err = clock_gettime(CLOCK_MONOTONIC_COARSE, &tv);
#else
  int err = clock_gettime(CLOCK_MONOTONIC, &tv);
#endif
  if (err) {
    error("Cannot get time!\n");
    leavedos(49);
  }
  return (tv.tv_sec * 1000000ULL + tv.tv_nsec / 1000);
}

/*
 * RAWcpuTIME points to one of these functions, and returns the
 * absolute CPU time
 */
static hitimer_t rawC4time(void)
{
  hitimer_t ctime;

  pthread_mutex_lock(&ctime_mtx);
  ctime = cached_time;
  pthread_mutex_unlock(&ctime_mtx);
  if (!ctime) {
    ctime = do_gettime();
    pthread_mutex_lock(&ctime_mtx);
    cached_time = ctime;
    pthread_mutex_unlock(&ctime_mtx);
  }
  return ctime;
}

void uncache_time(void)
{
  pthread_mutex_lock(&ctime_mtx);
  cached_time = 0;
  pthread_mutex_unlock(&ctime_mtx);
}

/*
 * SIDOC_BEGIN_FUNCTION GETcpuTIME
 *
 * GETcpuTIME is a pointer to a function which returns the
 * relative CPU time. Different methods of getting the time can
 * then be implemented, currently there are two using
 * gettimeofday() for 486 and TSC for pentium
 *
 * SIDOC_END_FUNCTION
 */
static hitimer_t getC4time(void)
{
  if (cpu_time_stop) return LastTimeRead;
  return (rawC4time() - ZeroTimeBase.td);
}

/*
 * SIDOC_BEGIN_FUNCTION GETusTIME(sc)
 *
 * GETusTIME returns the DOS time with 1-usec resolution
 *  using GETcpuTIME to get the implementation-dependent CPU time.
 * The 'sc' parameter is unused.
 *
 * SIDOC_END_FUNCTION
 */
hitimer_t GETusTIME(int sc)
{
  return GETcpuTIME();
}

/*
 * SIDOC_BEGIN_FUNCTION GETtickTIME(sc)
 *
 * GETtickTIME returns the DOS time with 838ns resolution
 *  using GETcpuTIME to get the implementation-dependent CPU time.
 * The 'sc' parameter is unused.
 *
 * SIDOC_END_FUNCTION
 */
hitimer_t GETtickTIME(int sc)
{
  return UStoTICK(GETcpuTIME());
}

/*
 * SIDOC_BEGIN_FUNCTION GETusSYSTIME()
 *
 * GETusSYSTIME returns the real CPU time with 1-usec resolution
 *
 * SIDOC_END_FUNCTION
 */
hitimer_t GETusSYSTIME(void)
{
	return GETcpuTIME();
}


void get_time_init(void)
{
  ZeroTimeBase.td = rawC4time();
  RAWcpuTIME = rawC4time;		/* in usecs */
  GETcpuTIME = getC4time;		/* in usecs */
  g_printf("TIMER: using clock_gettime(CLOCK_MONOTONIC)\n");
}

void cputime_late_init(void)
{
  idle_tid = coopth_create("hlt idle", idle_hlt_thr);
}


/* --------------------------------------------------------------------- */


int stop_cputime (int quiet)
{
  if (cpu_time_stop) return 1;
  if (!quiet) dbug_printf("STOP TIME\n");
  StopTimeBase = RAWcpuTIME();
  LastTimeRead = StopTimeBase - ZeroTimeBase.td;
  cpu_time_stop = 1;
  return 0;
}


int restart_cputime (int quiet)
{
  if (!cpu_time_stop) return 1;
  if (!quiet) dbug_printf("RESTART TIME\n");
  cpu_time_stop = 0;
  ZeroTimeBase.td += RAWcpuTIME() - StopTimeBase;
  return 0;
}

/* --------------------------------------------------------------------- */
int dosemu_frozen = 0;
int dosemu_user_froze = 0;

void freeze_dosemu_manual(void)
{
  dosemu_user_froze = 2;
  freeze_dosemu();
}

void freeze_dosemu(void)
{
  if (dosemu_frozen) return;

  stop_cputime(0);
  dosemu_frozen = 1;
  if (dosemu_user_froze) dosemu_user_froze--;
  dbug_printf("*** dosemu frozen\n");

  speaker_pause();

  if (Video && Video->change_config)
    Video->change_config (CHG_TITLE, NULL);
}

void unfreeze_dosemu(void)
{
  if (!dosemu_frozen) return;

  speaker_resume ();

  restart_cputime(0);
  dosemu_frozen = 0;
  dosemu_user_froze = 0;
  dbug_printf("*** dosemu unfrozen\n");

  if (Video && Video->change_config)
    Video->change_config (CHG_TITLE, NULL);
}

/* idle functions to let hogthreshold do its work .... */
static int trigger1 = 0;
void reset_idle(int val)
{
  val *= config.hogthreshold;
  pthread_mutex_lock(&trigger_mtx);
  if (-val < trigger1)
    trigger1 = -val;
  pthread_mutex_unlock(&trigger_mtx);
}

void alarm_idle(void)
{
  pthread_mutex_lock(&trigger_mtx);
  trigger1++;
  pthread_mutex_unlock(&trigger_mtx);
}

void trigger_idle(void)
{
  pthread_mutex_lock(&trigger_mtx);
  if (trigger1 >= 0)
    trigger1++;
  pthread_mutex_unlock(&trigger_mtx);
}

void untrigger_idle(void)
{
  pthread_mutex_lock(&trigger_mtx);
  if (trigger1 > 0)
    trigger1--;
  pthread_mutex_unlock(&trigger_mtx);
}

void dosemu_sleep(void)
{
#ifndef __EMSCRIPTEN__
  sigset_t mask;
  uncache_time();
  pthread_sigmask(SIG_SETMASK, NULL, &mask);
  sigsuspend(&mask);
#else
  uncache_time();
  usleep(10000);
#endif
}

/* "strong" idle callers will have threshold1 = 0 so only the
   inner loop applies. Heuristic idlers (int16/ah=1, mouse)
   need the two loops -- the outer loop can be reset using
   reset_idle */
static void _idle(int threshold1, int threshold, int threshold2,
    const char *who, int enable_ints)
{
  static int trigger = 0;
  int ret = 0;
  int old_if = isset_IF();
  if (config.hogthreshold && CAN_SLEEP()) {
    pthread_mutex_lock(&trigger_mtx);
    if(trigger1 >= config.hogthreshold * threshold1) {
      if (trigger++ >= (config.hogthreshold - 1) * threshold + threshold2) {
	if (debug_level('g') > 5)
	    g_printf("sleep requested by %s\n", who);
	pthread_mutex_unlock(&trigger_mtx);
	if (enable_ints && !old_if)
	    set_IF();
	coopth_wait();
	if (enable_ints && !old_if)
	    clear_IF();
	ret = 1;
	pthread_mutex_lock(&trigger_mtx);
	trigger = 0;
	if (debug_level('g') > 5)
	    g_printf("sleep ended\n");
      }
      if (trigger1 > 0)
	trigger1--;
    }
    pthread_mutex_unlock(&trigger_mtx);
  }

  if (!ret && enable_ints && !old_if)
    int_yield();
}

void idle(int threshold1, int threshold, int threshold2, const char *who)
{
  _idle(threshold1, threshold, threshold2, who, 0);
}

void idle_enable2(int threshold1, int threshold, int threshold2,
	const char *who)
{
  _idle(threshold1, threshold, threshold2, who, 1);
}

void idle_enable(int threshold, int threshold2, const char *who)
{
  _idle(0, threshold, threshold2, who, 1);
}

void int_yield(void)
{
  /* SeaBIOS does this:
   * asm volatile("sti ; nop ; rep ; nop ; cli ; cld" : : :"memory");
   */
  set_IF();
  coopth_yield();
  clear_IF();
}

static void idle_hlt_thr(void *arg)
{
  if (!isset_IF()) {
    error("cli/hlt detected, bye\n");
    leavedos(2);
    return;
  }
  idle(0, 50, 0, "hlt idle");
}

void cpu_idle(void)
{
    coopth_start(idle_tid, NULL);
}
