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
 * GETusTIME returns the DOS ('stretched') time with 1-usec resolution
 *  using GETcpuTIME to get the implementation-dependent CPU time.
 * The 'sc' parameter controls the granularity of the stretching
 *  algorithm (not yet there, see the docs)
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
 * The 'sc' parameter works like in GETusTIME.
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
  if ((config.realcpu>4) && config.rdtsc) {
    RAWcpuTIME = rawP5time;
    GETcpuTIME = getP5time;
    g_printf("TIMER: using pentium timing\n");
  }
  else {
    RAWcpuTIME = rawC4time;
    GETcpuTIME = getC4time;
    g_printf("TIMER: using gettimeofday\n");
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
	int i, rawspeed;
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
	/* how to round? standard clock speeds are usually the
	 * multiples of 30 and 33.333, plus 75. Some people
	 * use non-standard clock values (like 208) by fiddling with
	 * the motherboard jumpers: but we hope they are smart enough
	 * to read the READMEs and set their right speed in dosemu.conf.
	 * OTOH, I'm not smart enough to come up with a better
	 * algorithm  --AV
	 */
	rawspeed = (int)(b/a);
	for (i=67; i<=300; i+=33) {
	  if ((i%10)==6) i++;	/* 133,167,200... */
	  if (abs(rawspeed-i)<3) return i;
	}
	for (i=60; i<=300; i+=(i>=90? 30:15)) {
	  if (abs(rawspeed-i)<3) return i;
	}
	return rawspeed;
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
	int mlt=0, dvs=0;
	char *p;

	if (config.realcpu < CPU_586) {
		fprintf(stderr,"You can't access 586 features on CPU=%d\n",
			config.realcpu);
		exit(1);
	}
	if (!first) return 0;
#if 0
	/* user-defined speed */
	if ((p=getenv("CPUSPEED"))!=NULL) {
	  int v1;
	  if (sscanf(p,"%d",&v1)==1) {
	    if ((v1>60) && (v1<=300)) {
	      mlt = 1; dvs = v1;
	    }
	  }
	}
#endif
	if (!mlt) {
	/* last resort - do it yourself */
	  mlt = 1; dvs = getmhz();
	}

	/* speed division factor to get 1us from CPU clocks - for
	 * details on fast division see timers.h */
	*spus = (LLF_US*mlt)/dvs;

	/* speed division factor to get 838ns from CPU clocks */
	*sptick = (LLF_TICKS*mlt)/dvs;

	fprintf (stderr,"CPU speed set to %d/%d MHz\n",dvs,mlt);
	first = 0;
	return 0;
}


