/*
 * DANG_BEGIN_MODULE
 *
 * Description: Timer emulation for DOSEMU.
 * 
 * Maintainers: Scott Bucholz
 *              J. Lawrence Stephan
 *
 * This is the timer emulation for DOSEMU.  It emulates the Programmable
 * Interval Timer (PIT), and also handles IRQ0 interrupt events.
 * A lot of animation and video game software are dependant on this module
 * for high frequency timer interrupts (IRQ0).
 *
 * COPYRIGHT: There is some old Mach code in here.  Please be aware of
 * their copyright in mfs.c
 * 
 * This code will actually generate 18.2 interrupts/second.  It will even
 * happily attempt to generate faster clocks, right up to the point where
 * it chokes.  Since the absolute best case timing we can get out of Linux
 * is 100Hz, figure that anything approaching or exceeding that isn't going
 * to work well.  (The code will attempt to generate up to 10Khz interrupts
 * per second at the moment.  Too bad that would probably overflow all
 * internal queues really fast. :)
 *
 * DANG_END_MODULE
 *
 * $Date: 1995/02/25 22:37:51 $
 * $Source: /home/src/dosemu0.60/dosemu/RCS/timers.c,v $
 * $Revision: 2.4 $
 * $State: Exp $
 *
 * $Log: timers.c,v $
 * Revision 2.4  1995/02/25  22:37:51  root
 * *** empty log message ***
 *
 * Revision 2.3  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.3  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.2  1994/09/22  23:51:57  root
 * Prep for pre53_21.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.6  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.5  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.4  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.3  1993/11/29  00:05:32  root
 * *** empty log message ***
 *
 * Revision 1.2  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.7  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.6  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.5  1993/03/02  03:06:42  root
 * somewhere between 0.48pl1 and 0.49 (with IPC).  added virtual IOPL
 * and AC support (for 386/486 tests), -3 and -4 flags for choosing.
 * Split dosemu into 2 processes; the child select()s on the keyboard,
 * and signals the parent when a key is received (also sends it on a
 * UNIX domain socket...this might not work well for non-console keyb).
 *
 * Revision 1.4  1993/02/24  11:33:24  root
 * some general cleanups, fixed the key-repeat bug.
 *
 * Revision 1.3  1993/02/18  19:35:58  root
 * just added newline so diff wouldn't barf
 *
 * Revision 1.2  1993/02/16  00:21:29  root
 * DOSEMU 0.48 DISTRIBUTION
 *
 * Revision 1.1  1993/02/13  23:37:20  root
 * Initial revision
 *
 */

#include <linux/time.h>
#include "emu.h"
#include "timers.h"
#include "int.h"
#ifdef NEW_PIC
#include "../timer/pic.h"
#endif

#define CLOCK_TICK_RATE   1193180     /* underlying clock rate in HZ */

static int timer0_read_latch_value = 0;
static int timer0_read_latch_value_orig = 0;
static int timer0_latched_value = 0;
static int timer0_new_value = 0;
static boolean_t latched_counter_msb = FALSE;

static u_long timer_div;          /* used by timer int code */
static struct timeval clock0;     /* start time of port 0 timer */
static u_long ticks_accum;        /* For timer_tick function, 100usec ticks */

/*
 * DANG_BEGIN_FUNCTION set_timer_latch0
 *
 * description:
 *  DOS programs latch values into port 0x40 to change the timer interrupt
 *  rate.  This procedure calculates the values to attemp to generate the
 *  requested interrupt rate.
 *
 * DANG_END_FUNCTION
 */
static void set_timer_latch0(u_long latch_value)
{
  ticks_accum   = 0;
  gettimeofday(&clock0, NULL);

  if (latch_value == 0)
    latch_value = 0x10000;
  timer_div     = (latch_value * 10000) / CLOCK_TICK_RATE;
  if (timer_div == 0)
    timer_div = 1;

  i_printf("timer_interrupt_rate requested %.3g Hz, granted %.3g Hz\n",
           CLOCK_TICK_RATE / (double)latch_value, 10000.0 / timer_div);
}

/*
 * DANG_BEGIN_FUNCTION initialize_timers
 *
 * description:  ensure the 0x40 port timer is initially set correctly
 *
 * DANG_END_FUNCTION
 */
void initialize_timers(void)
{
   set_timer_latch0(0x10000);
}

/*
 * DANG_BEGIN_FUNCTION initialize_timers
 *
 * description:
 *  Every time we get a TIMER signal from Linux, this procedure is called.
 *  It checks to see if we should queue a timer interrupt based on the
 *  current values.
 *
 * DANG_END_FUNCTION
 */
void timer_tick(void)
{
  struct timeval tp;
  u_long time_curr;
  static u_long time_old = 0;       /* Preserve value for next call */

  if (!config.timers) {
    h_printf("NOT CONFIG.TIMERS\n");
    return;
  }

  /* Get system time */
  gettimeofday(&tp, NULL);

  /* compute the number of 100usecs since we started */
  time_curr  = (tp.tv_sec - clock0.tv_sec) * 10000;
  time_curr += (tp.tv_usec - clock0.tv_usec) / 100;
  
  /* Reset old timer value to 0 if time_curr wrapped around back to 0 */
  if (time_curr < time_old) time_old = 0;
  
  /* Compute number of 100usec ticks since the last time this function ran */
  ticks_accum += (time_curr - time_old);
  pit.CNTR2 -= ticks_accum;
  
  /* Save old value of the timer */
  time_old = time_curr;

  /* Add any missing timer events to the queue */
  while (ticks_accum > timer_div) {
    ticks_accum -= timer_div;
    /* h_printf("starting timer int 8...\n"); */ /* Slows down game testing */
#ifndef NEW_PIC
    if (!do_hard_int(8)) h_printf("CAN'T DO TIMER INT 8...IF CLEAR\n");
#else
#if NEW_PIC==2
    /* This line will be obsolete with the new serial PIC coming soon */
    age_transmit_queues();
#endif
    pic_request(PIC_IRQ0);
#endif
  }
}

void set_ticks(unsigned long new)
{
  unsigned long *ticks = BIOS_TICK_ADDR;
  unsigned char *overflow = TICK_OVERFLOW_ADDR;

  ignore_segv++;
  *ticks = new;
  *overflow = 0;
  ignore_segv--;
  /* warn("TIMER: update value of %d\n", (40 / (1000000 / UPDATE))); */
}

int do_40(boolean_t in_out, int val)
{
  int ret;

  if (in_out) {  /* true => in, false => out */
    if (timer0_latched_value == 0) {
      timer0_latched_value = timer_div*850;
    }
    switch (timer0_read_latch_value) {
      case 0x30:
        ret = timer0_latched_value&0xf;
	timer0_read_latch_value = 0x20;
	break;
      case 0x10:
	ret = timer0_latched_value&0xf;
	timer0_read_latch_value = 0x0;
	break;
      case 0x20:
	ret = (timer0_latched_value&0xf0)>>8;
	timer0_read_latch_value = 0x0;
	timer0_latched_value = 0;
	break;
      case 0x0:
	if (!latched_counter_msb) {
	  struct timeval tp;
	  u_long count;
	  gettimeofday(&tp, NULL);

	  count = (tp.tv_sec - clock0.tv_sec) * CLOCK_TICK_RATE +
	          ((tp.tv_usec - clock0.tv_usec) * 1193) / 1000;

	  timer0_latched_value = 0x10000 - count;

	  ret = timer0_latched_value & 0xff;
	} else {
	  ret = (timer0_latched_value >> 8) & 0xff;
	  timer0_latched_value = 0;
	}
	latched_counter_msb = !latched_counter_msb;
	break;
      }
    i_printf("PORT: do_40, in = 0x%x\n",ret);
  } else {
    switch (timer0_read_latch_value) {
      case 0x0:
      case 0x30:
        timer0_new_value = val;
	timer0_read_latch_value = 0x20;
	break;
      case 0x10:
	timer0_new_value = val;
	timer0_read_latch_value = 0x0;
	break;
      case 0x20:
	timer0_new_value |= (val<<8);
	timer0_read_latch_value = 0x0;
	break;
      }
    i_printf("PORT: do_40, out = 0x%x\n",val);
    if (timer0_read_latch_value == 0)
      set_timer_latch0(timer0_new_value);
  }
  return(ret);
}

#if 0
/* return value of port -- what happens when output?  Stack trash? */
int do_42(in_out, val)
	boolean_t in_out;
	int val;
{
	int ret;
	if (in_out) {  /* true => in, false => out */
		ioperm(0x42,1,1);
		ret = port_in(0x42);
		ioperm(0x42,1,0);
		i_printf( "PORT: do_42, in = 0x%x\n",ret);
	} else {
		ioperm(0x42,1,1);
		port_out(val, 0x42);
		ioperm(0x42,1,0);
		i_printf( "PORT: do_42, out = 0x%x\n",val);
	}
	return(ret);
}

#endif

int do_43(boolean_t in_out, int val)
{
  int ret;
	
  if (in_out) {  /* true => in, false => out */
    i_printf( "PORT: do_43, in = 0x%x\n",ret);
  } else {
    i_printf( "PORT: do_43, out = 0x%x\n",val);
    switch(val>>6) {
    case 0:
      timer0_read_latch_value = (val&0x30);
      timer0_read_latch_value_orig = (val&0x30);
      if (timer0_read_latch_value == 0) {
	latched_counter_msb = FALSE;
      }
      break;
    case 2:
#if 0
      ioperm(0x43,1,1);
      port_out(val, 0x43);
      ioperm(0x43,1,0);
#else
      safe_port_out_byte(0x43, val);
#endif
      i_printf( "PORT: Really do_43\n");
      break;
    }
  }
  return(ret);
}
