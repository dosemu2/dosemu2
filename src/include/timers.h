/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef DOSEMU_TIMERS_H
#define DOSEMU_TIMERS_H

#include "config.h"
#include "extern.h"
#include "types.h"

/* A 'magical' constant used to force the result we want, in this case
 * getting 100Hz out of SIGALRM_call()
 */
#define TIMER_DIVISOR   6

extern void timer_tick(void);
extern Bit8u pit_inp(ioport_t);
extern void pit_outp(ioport_t, Bit8u);

/* The divisor used to time faster-than-1sec activities (floppy,printer,
 * IPX) - see signal.c
 */
#define PARTIALS	5

#define BIOS_TICK_ADDR		(void *)0x46c
#define TICK_OVERFLOW_ADDR	(void *)0x470

/* underlying clock rate in HZ */
#define PIT_TICK_RATE		1193182
#define PIT_TICK_RATE_k		1193.182
#define PIT_TICK_RATE_M		1.193182
#define PIT_TICK_RATE_f		0.193182

/* number of 18.2Hz ticks = 86400*PIT_TICK_RATE/65536 = 1573042.6756 */
#define TICKS_IN_A_DAY		1573042

#define PIT_TIMERS      4            /* 4 timers (w/opt. at 0x44)   */

/* --------------------------------------------------------------------- */

#define JIFFIE_TIME		(1000000/sysconf(_SC_CLK_TCK))

/* this specifies how many microseconds int 0x2f, ax=0x1680, will usleep().
 * we don't really have a "give up time slice" primitive, but something
 * like this works okay...thanks to Andrew Tridgell.
 * same for int 0x15, ax=0x1000 (TopView/DESQview).
 */
#define INT2F_IDLE_USECS	(JIFFIE_TIME*8)
#define INT15_IDLE_USECS	(JIFFIE_TIME*8)
#define INT28_IDLE_USECS	(JIFFIE_TIME/2)
void reset_idle(int val);
void alarm_idle(void);
void trigger_idle(void);
int idle(int threshold1, int threshold, int threshold2, int usec, const char *who);

/* --------------------------------------------------------------------- */
/*	New unified timing macros with/without Pentium rdtsc - AV 8/97	 */
/* --------------------------------------------------------------------- */
/*
 * replace 64/32 divide instructions with 64*32 multiply
 * how it works: we multiply by f = (1/x)<<32 = (1<<32)/x
 *	input = tH:tL, k
 *		tL*k ----->      edx:eax(discard)
 *		tH*k ----->  edx:eax
 *		sum	     -------
 *	output =	     edx:eax
 *
 * all operations unsigned, it really shines on K6 where mul=3 cycles!
 *
 * GCC also generates 2 mul instructions, no need to use asm here (BO).
 */
static __inline__ hitimer_t _mul64x32_(hitimer_t v, unsigned long f)
{
	return (unsigned long)(v >> 32) * (unsigned long long)f + 
	  (((unsigned long)(v & 0xffffffff) * (unsigned long long)f) >> 32);
}

#define LLF_US		(unsigned long long)0x100000000LL
#define LLF_TICKS	(unsigned long long)(LLF_US*PIT_TICK_RATE_M)

/* warning - 'k' must be a constant here! (but can also be a float) */
#define T64DIV(t,k)	_mul64x32_((t),(LLF_US/k))
#define TICKtoMS(t)	({unsigned long _m=T64DIV(t,PIT_TICK_RATE_k); _m;})

#define CPUtoTICK(t)	_mul64x32_((t),config.cpu_tick_spd)
/* t*1.193 = t+t*0.193 */
#define UStoTICK(t)	({ hitimer_t _l=(t); \
			_l+_mul64x32_(_l,(LLF_US*PIT_TICK_RATE_f)); })

#define TICKtoUS(t)	({unsigned long _m=T64DIV(t,PIT_TICK_RATE_M); _m;})

/* --------------------------------------------------------------------- */

extern void update_cputime_TSCBase(void);
extern hitimer_u ZeroTimeBase;
extern hitimer_t t_vretrace;

EXTERN hitimer_t (*GETcpuTIME)(void) INIT(0);

static inline hitimer_t GETTSC(void) {
	hitimer_t d;
#ifdef __x86_64__
	unsigned int lo, hi;
	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	d = lo | ((unsigned long)hi << 32);
#else
	__asm__ __volatile__ ("rdtsc" : "=A" (d));
#endif
	return d;
}

#define CPUtoUS() _mul64x32_(GETTSC(), config.cpu_spd)

static inline hitimer_t TSCtoUS(register hitimer_t t) {
	return _mul64x32_(t, config.cpu_spd);
}

/* 1 us granularity */
extern hitimer_t GETusTIME(int sc);
extern hitimer_t GETusSYSTIME(void);

/* 838 ns granularity */
extern hitimer_t GETtickTIME(int sc);

int stop_cputime (int);
int restart_cputime (int);
extern int cpu_time_stop;	/* for dosdebug */
int bogospeed(unsigned long *spus, unsigned long *sptick);

void freeze_dosemu_manual(void);
void freeze_dosemu(void);
void unfreeze_dosemu(void);
extern int dosemu_frozen;
extern int dosemu_user_froze;

/* --------------------------------------------------------------------- */

extern Bit8u pit_control_inp(ioport_t);
extern void pit_control_outp(ioport_t port, Bit8u val);
extern void initialize_timers(void);
extern void get_time_init(void);
extern void do_sound(Bit16u period);

#endif /* DOSEMU_TIMERS_H */
