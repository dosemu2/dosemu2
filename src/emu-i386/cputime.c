/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "emu.h"
#include "video.h"
#include "timers.h"
#include "pic.h"
#include "int.h"
#include "speaker.h"
#include "dosemu_config.h"

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
static hitimer_u ZeroTSCBase = { 0 };
static hitimer_t C4Base = 0;
static hitimer_t LastTimeRead = 0;
static hitimer_t StopTimeBase = 0;
int cpu_time_stop = 0;

/*
 * RAWcpuTIME points to one of these functions, and returns the
 * absolute CPU time
 */
static hitimer_t rawC4time(void)
{
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv;
  gettimeofday(&tv, NULL);	/* took 30us on a P5-150 in 1997, now
  				 * takes 1-2us on a K6-300 under 2.1.126 */
  /* unsigned is necessary to tell gcc not to sign extend the tv_ fields */
  return ((hitimer_t)((unsigned)tv.tv_sec) * 1000000 + (unsigned)tv.tv_usec);
#else
#error Cannot get time
#endif
}

static hitimer_t rawP5time(void)
{
  return CPUtoUS();
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

/* this routine is called from the sigalrm handler to
   update the TSC base */
void update_cputime_TSCBase(void)
{
  if (cpu_time_stop) return;
  C4Base = getC4time();
  ZeroTSCBase.td = GETTSC();
}

static hitimer_t getP5time(void)
{
  if (cpu_time_stop) return LastTimeRead;
  return TSCtoUS(GETTSC() - ZeroTSCBase.td) + C4Base;
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


void get_time_init (void)
{
  ZeroTimeBase.td = rawC4time();
  if ((config.realcpu > CPU_486) && config.rdtsc) {
    /* we are here if: a 586/686 was detected at startup, we are not
     * on a SMP machine and the user didn't say 'rdtsc off'. But
     * we could have an emulated CPU < 586, this doesn't affect timing
     * but only flags processing (& other features) */
    RAWcpuTIME = rawP5time;		/* in usecs */
    GETcpuTIME = getP5time;		/* in usecs */
    ZeroTSCBase.td = GETTSC();
    g_printf("TIMER: using pentium timing\n");
  }
  else {
    /* we are here on all other cases: real CPU < 586, SMP machines,
     * 'rdtsc off' into config file */
    RAWcpuTIME = rawC4time;		/* in usecs */
    GETcpuTIME = getC4time;		/* in usecs */
    if (config.realcpu) {
      if (kernel_version_code < 0x2017e)
        g_printf("TIMER: using gettimeofday\n");
      else
        g_printf("TIMER: using new gettimeofday with microsecond resolution\n");
    }
  }
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
  if (ZeroTSCBase.td)
    update_cputime_TSCBase();
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


/* --------------------------------------------------------------------- */

static int getmhz(void)
{
	struct timeval tv1,tv2;
	hitimer_t a,b;
        unsigned long a0, a1, b0, b1;

	gettimeofday(&tv1, NULL);
	__asm__ __volatile__ ("rdtsc"
		:"=a" (a0),
		 "=d" (a1));
/*	for (j=0; j<10000000; j++);*/	/* 500ms on a P5-100 */
	usleep(50000);
	gettimeofday(&tv2, NULL);
	__asm__ __volatile__ ("rdtsc"
		:"=a" (b0),
		 "=d" (b1));
	b = (((hitimer_t)b1 << 32) | b0) - (((hitimer_t)a1 << 32) | a0);
	a = (tv2.tv_sec*1000000 + tv2.tv_usec) - 
	    (tv1.tv_sec*1000000 + tv1.tv_usec);
	return (int)((b*4096)/a);
}

/*
 * bogospeed is called when a CPU >486 is detected at startup, or when
 * the user specifies -5 or -6 on the command line (provided (s)he HAS
 * a >486 CPU). The name comes from the previous use of BogoMIPS from
 * /proc/cpuinfo; the value returned must NOT be bogus...
 */
int bogospeed(unsigned long *spus, unsigned long *sptick)
{
	boolean first=1;
	int mlt, dvs;

	if (config.realcpu < CPU_586) {
		fprintf(stderr,"You can't access 586 features on CPU=%d\n",
			config.realcpu);
		exit(1);
	}
	if (!first) return 0;

	mlt = 4096; dvs = getmhz();

	/* speed division factor to get 1us from CPU clocks - for
	 * details on fast division see timers.h */
	*spus = (LLF_US*mlt)/dvs;

	/* speed division factor to get 838ns from CPU clocks */
	*sptick = (LLF_TICKS*mlt)/dvs;

	config.CPUSpeedInMhz = dvs/mlt;
	fprintf (stderr,"CPU speed set to %d MHz\n",(dvs/mlt));
/*	fprintf (stderr,"CPU speed factors %ld,%ld\n",*spus,*sptick); */
	first = 0;
	return 0;
}

/* idle functions to let hogthreshold do its work .... */
static int trigger1 = 0;
void reset_idle(int val)
{
  val *= config.hogthreshold;
  if (-val < trigger1)
    trigger1 = -val;
}

void alarm_idle(void)
{
  if (trigger1 < 0)
    trigger1++;
}

void trigger_idle(void)
{
  if (trigger1 >= 0)
    trigger1++;
}

/* "strong" idle callers will have threshold1 = 0 so only the
   inner loop applies. Heuristic idlers (int16/ah=1, mouse)
   need the two loops -- the outer loop can be reset using
   reset_idle */
int idle(int threshold1, int threshold, int threshold2, int usec, const char *who)
{
  static int trigger = 0;
  do_periodic_stuff();
  if (config.hogthreshold && CAN_SLEEP()) {
    if(trigger1 >= config.hogthreshold * threshold1) {
      if (trigger++ > (config.hogthreshold - 1) * threshold + threshold2) {
	sigset_t mask;
	sigemptyset(&mask);
	sigsuspend(&mask);
	trigger = 0;
      }
      if (trigger1 > 0)
	trigger1--;
      if (trigger == 0)
	return 1;
    }
  }
  return 0;
}
