/*
 * SIDOC_BEGIN_MODULE
 *
 * System time routines
 *
 * Functions used to get a monoton, 64-bit time value either from the
 * kernel (with gettimeofday()) or from the CPU hi-res timer
 * Maintainer: vignani@mbox.vol.it (Alberto Vignani)
 * 
 * SIDOC_END_MODULE
 */

#include <stdlib.h>
#include <stdio.h>
#include <features.h>
#include <sys/time.h>
#include <unistd.h>
#include "config.h"
#include "emu.h"
#include "timers.h"
#include "pic.h"

/* --------------------------------------------------------------------- */
/*
 * SIDOC_BEGIN_REMARK
 *
 * At the heart of the timing system in dosemu >= 0.67.11 is the availability
 * of the system time as a 64-bit [type hitimer_t] monoton value.
 * Dosemu needs this time under two resolutions:
 * VERB
 *   - a MICROSECOND resolution for general timing purposes
 *   - a TICK(838ns) resolution for PIT
 * /VERB
 * On non-pentium machines, only the first one is available via the
 * kernel call gettimeofday(). On pentium, the situation is better
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
 * The 's' parameter, when !=0, activates the time correction if
 * available, thus controlling the granularity of the stretching.
 * The function GETusSYSTIME() never activates this stretching, and
 * is used only by the realtime thread-based 1-sec timer in rtc.c.
 *
 * All timing are RELATIVE and start from zero. The use of a zero-based
 * time has many advantages (last but not least, readability), and esp.
 * allows us to STOP and restart the timer during debugging. The two
 * functions stop_cputime() and restart_cputime() are used for this
 * purpose, but can have other uses too.
 *
 * SIDOC_END_REMARK
 */

static hitimer_t (*RAWcpuTIME)(void);

/* there are some reasons time must start from zero insead that from the
 * actual current time:
 * - it keeps overflows to a minimum
 * - the pentium counter (when used) has no relation to the real time
 * - DOS already uses a relative time base
 * - the debug log is MUCH more easy to read for humans (except maybe germans)
 */
static hitimer_u ZeroTimeBase = { 0 };
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
  gettimeofday(&tv, NULL);	/* takes 30us on a P5-150 */
  return ((hitimer_t)tv.tv_sec*1000000 + (hitimer_t)tv.tv_usec);
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

static hitimer_t getP5time(void)
{
  if (cpu_time_stop) return LastTimeRead;
  return CPUtoUS_Z();
}

/*
 * SIDOC_BEGIN_FUNCTION GETusTIME(sc)
 *
 * GETusTIME returns the DOS (stretched) time with 1-usec resolution
 *  using GETcpuTIME to get the implementation-dependent CPU time.
 * The 'sc' parameter controls the granularity of the stretching
 *  algorithm (see timers.h)
 *
 * SIDOC_END_FUNCTION
 */
hitimer_t GETusTIME(int sc)
{
  return GETcpuTIME();	/* no 'stretching' at the moment */
}

/*
 * SIDOC_BEGIN_FUNCTION GETtickTIME(sc)
 *
 * GETtickTIME returns the DOS (stretched) time with 838ns resolution
 *  using GETcpuTIME to get the implementation-dependent CPU time.
 * The 'sc' parameter controls the granularity of the stretching
 *  algorithm (see timers.h)
 *
 * SIDOC_END_FUNCTION
 */
hitimer_t GETtickTIME(int sc)
{
  return UStoTICK(GETcpuTIME());  /* no 'stretching' at the moment */
}

/*
 * SIDOC_BEGIN_FUNCTION GETusSYSTIME()
 *
 * GETusSYSTIME returns the DOS (unstretched) time with 1-usec resolution
 *
 * SIDOC_END_FUNCTION
 */
hitimer_t GETusSYSTIME(void)
{
	return GETcpuTIME();
}


void get_time_init (void)
{
  if ((vm86s.cpu_type>4) && config.rdtsc) {
    RAWcpuTIME = rawP5time;
    GETcpuTIME = getP5time;
  }
  else {
    RAWcpuTIME = rawC4time;
    GETcpuTIME = getC4time;
  }
  ZeroTimeBase.td = RAWcpuTIME();
}


/* --------------------------------------------------------------------- */


int stop_cputime (void)
{
  if (cpu_time_stop) return 1;
  dbug_printf("STOP TIME\n");
  StopTimeBase = RAWcpuTIME();
  LastTimeRead = StopTimeBase - ZeroTimeBase.td;
  cpu_time_stop = 1;
  return 0;
}


int restart_cputime (void)
{
  if (!cpu_time_stop) return 1;
  dbug_printf("RESTART TIME\n");
  ZeroTimeBase.td += (RAWcpuTIME() - StopTimeBase);
  cpu_time_stop = 0;
  return 0;
}


/* --------------------------------------------------------------------- */

int getmhz(void)
{
	struct timeval tv1,tv2;
	hitimer_t a,b;
	volatile int j;

	gettimeofday(&tv1, NULL);
	__asm__ __volatile__ ("rdtsc"
		:"=a" (((unsigned long*)&a)[0]),
		 "=d" (((unsigned long*)&a)[1]));
	for (j=0; j<10000000; j++);	/* 500ms on a P5-100 */
	gettimeofday(&tv2, NULL);
	__asm__ __volatile__ ("rdtsc"
		:"=a" (((unsigned long*)&b)[0]),
		 "=d" (((unsigned long*)&b)[1]));
	b -= a;
	a = (tv2.tv_sec*1000000 + tv2.tv_usec) - 
	    (tv1.tv_sec*1000000 + tv1.tv_usec);
	return (int)(b/a);
}

int bogospeed(unsigned long *spus, unsigned long *sptick)
{
	boolean first=1;
	unsigned long a;
	int v1, mlt=0, dvs=0;
	char *p;

	__asm__ __volatile__ (
		"movl $1,%%eax
		 cpuid"
		:"=d" (a)
		:
		:"%eax","%ebx","%ecx","%edx");
	if ((a & 0x10)==0) {
		fprintf(stderr,"Please use -5 on CPU >= 586\n");
		return 1;
	}
	if (!first) return 0;
	/* user-defined speed */
	if ((p=getenv("CPUSPEED"))!=NULL) {
	  if (sscanf(p,"%d",&v1)==1) {
	    if ((v1>60) && (v1<=300)) {
	      mlt = 1; dvs = v1;
	    }
	  }
	}
	if (!mlt) {
	/* last resort - do it yourself */
	  mlt = 1; dvs = getmhz();
	}
	*spus = (LLF_US*mlt)/dvs;
	*sptick = (LLF_TICKS*mlt)/dvs;
	fprintf (stderr,"CPU speed set to %d/%d MHz\n",dvs,mlt);
	first = 0;
	return 0;
}


