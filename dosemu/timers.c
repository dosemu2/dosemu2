/*
 * DANG_BEGIN_MODULE
 *
 * Description: Timer emulation for DOSEMU.
 * 
 * Maintainers: J. Lawrence Stephan
 *              Scott Buchholz
 *
 * This is the timer emulation for DOSEMU.  It emulates the Programmable
 * Interval Timer (PIT), and also handles IRQ0 interrupt events.
 * A lot of animation and video game software are dependant on this module
 * for high frequency timer interrupts (IRQ0).
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
 * $Date: 1995/05/06 16:25:30 $
 * $Source: /usr/src/dosemu0.60/dosemu/RCS/timers.c,v $
 * $Revision: 2.6 $
 * $State: Exp $
 *
 * $Log: timers.c,v $
 * Revision 2.6  1995/05/06  16:25:30  root
 * Prep for 0.60.2.
 *
 * Revision 2.5  1995/04/08  22:30:40  root
 * Release dosemu0.60.0
 *
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

#ifdef __NetBSD__
#include <sys/time.h>
#endif
#ifdef __linux__
#include <linux/time.h>
#endif
#include "emu.h"
#include "timers.h"
#include "int.h"
#include "pic.h"

#define CLOCK_TICK_RATE   1193180     /* underlying clock rate in HZ */

typedef struct {
  short          read_state;
  short          write_state;
  int            mode;
  int            read_latch;
  int            write_latch;
  long           cntr;
  struct timeval time;
} pit_latch_struct;

static pit_latch_struct pit[3];   /* values of 3 PIT counters */

static u_long timer_div;          /* used by timer int code */
static u_long ticks_accum;        /* For timer_tick function, 100usec ticks */

static int    port61 = 0x0e;      /* Speaker control */

/*
 * DANG_BEGIN_FUNCTION initialize_timers
 *
 * description:  ensure the 0x40 port timer is initially set correctly
 *
 * DANG_END_FUNCTION
 */
void initialize_timers(void)
{
  struct timeval cur_time;

  gettimeofday(&cur_time, NULL);

  pit[0].mode        = 3;
  pit[0].cntr        = 0x10000;
  pit[0].time        = cur_time;
  pit[0].read_latch  = -1;
  pit[0].write_latch = 0;
  pit[0].read_state  = 3;
  pit[0].write_state = 3;

  pit[1].mode        = 2;
  pit[1].cntr        = 18;
  pit[1].time        = cur_time;
  pit[1].read_latch  = -1;
  pit[1].write_latch = 18;
  pit[1].read_state  = 3;
  pit[1].write_state = 3;

  pit[2].mode        = 0;
  pit[2].cntr        = -1;
  pit[2].time        = cur_time;
  pit[2].read_latch  = -1;
  pit[2].write_latch = 0;
  pit[2].read_state  = 3;
  pit[2].write_state = 3;

  ticks_accum   = 0;
  timer_div     = (pit[0].cntr * 10000) / CLOCK_TICK_RATE;
}

/*
 * DANG_BEGIN_FUNCTION timer_tick
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
  time_curr  = (tp.tv_sec - pit[0].time.tv_sec) * 10000;
  time_curr += (tp.tv_usec - pit[0].time.tv_usec) / 100;
  
  /* Reset old timer value to 0 if time_curr wrapped around back to 0 */
  if (time_curr < time_old) time_old = 0;
  
  /* Compute number of 100usec ticks since the last time this function ran */
  ticks_accum += (time_curr - time_old);
  
  /* Save old value of the timer */
  time_old = time_curr;
  
  /* test for stuck interrupts, trigger any scheduled interrupts */
  pic_watch(&tp);
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

static void pit_latch(int latch)
{
  struct timeval cur_time;
  u_long         ticks;

  /* check for special 'read latch status' mode */
  if (pit[latch].mode & 0x80) {
    pit[latch].mode = pit[latch].mode & 0x07;
    /*
     * Latch status:
     *   bit 7   = state of OUT pin (uncomputed here -- always 0)
     *   bit 6   = null count flag (1 == no cntr set, 0 == cntr available)
     *   bit 4-5 = read latch format
     *   bit 1-3 = read latch mode
     *   bit 0   = BCD flag (1 == BCD, 0 == 16-bit -- always 0)
     */
    pit[latch].read_latch = (pit[latch].read_state << 4) |
                            (pit[latch].mode << 1);
    if (pit[latch].cntr == -1)
      pit[latch].read_latch |= 0x40;
    return;
  }

  switch (pit[latch].mode) {
    default:    /* just in case... */
    case 0x00:  /* mode 0   -- countdown, interrupt, wait*/
    case 0x01:  /* mode 1   -- countdown, wait */
    case 0x04:  /* mode 4   -- countdown, wait */
    case 0x05:  /* mode 5   -- countdown, wait */
      if (pit[latch].cntr != -1) {
	gettimeofday(&cur_time, NULL);
	ticks = (cur_time.tv_sec - pit[latch].time.tv_sec) * CLOCK_TICK_RATE +
	        ((cur_time.tv_usec - pit[latch].time.tv_usec) * 1193) / 1000;

	if (ticks > pit[latch].cntr) {
	  pit[latch].cntr = -1;
	  pit[latch].read_latch = pit[latch].write_latch;
	} else {
	  pit[latch].read_latch = pit[latch].cntr - ticks;
	}
      } else {
	pit[latch].read_latch = pit[latch].write_latch;
      }
      break;

    case 0x02:  /* mode 2,6 -- countdown, reload */
    case 0x06:
#if 0
      gettimeofday(&cur_time, NULL);
      /* fancy calculations to avoid overflow */
      ticks = (cur_time.tv_sec - pit[latch].time.tv_sec) % pit[latch].cntr;
      ticks = ticks * (CLOCK_TICK_RATE % pit[latch].cntr) +
	      ((cur_time.tv_usec - pit[latch].time.tv_usec) * 1193) / 1000;
#endif
      ticks = pit[latch].cntr - (pic_dos_time % pit[latch].cntr);
      pit[latch].read_latch = pit[latch].cntr - ticks % pit[latch].cntr;
      break;

    case 0x03:  /* mode 3,7 -- countdown by 2(?), interrupt, reload */
    case 0x07:
      gettimeofday(&cur_time, NULL);
      /* fancy calculations to avoid overflow */
      ticks = (cur_time.tv_sec - pit[latch].time.tv_sec) % pit[latch].cntr;
      ticks = ticks * (CLOCK_TICK_RATE % pit[latch].cntr) +
	      ((cur_time.tv_usec - pit[latch].time.tv_usec) * 1193) / 1000;
      pit[latch].read_latch = pit[latch].cntr - (2*ticks) % pit[latch].cntr;
      break;
  }
#if 0 /* for debugging */
  i_printf("pit_latch:  latched value 0x%4.4x for timer %d\n",
	   pit[latch].read_latch, latch);
#endif
}

int pit_inp(int port)
{
  int ret = 0;

  if (port == 2 && config.speaker == SPKR_NATIVE) {
    return safe_port_in_byte(0x42);
  }

  if (port == 1)
    i_printf("PORT:  someone is reading the CMOS refresh time?!?");

  if (pit[port].read_latch == -1)
    pit_latch(port);

  switch (pit[port].read_state) {
    case 0: /* read MSB & return to state 3 */
      ret = (pit[port].read_latch >> 8) & 0xff;
      pit[port].read_state = 3;
      pit[port].read_latch = -1;
      break;
    case 3: /* read LSB followed by MSB */
      ret = (pit[port].read_latch & 0xff);
      pit[port].read_state = 0;
      break;
    case 1: /* read MSB */
      ret = (pit[port].read_latch >> 8) & 0xff;
      pit[port].read_latch = -1;
      break;
    case 2: /* read LSB */
      ret = (pit[port].read_latch & 0xff);
      pit[port].read_latch = -1;
      break;
  }
#if 0
  i_printf("PORT: pit_inp(0x%x) = 0x%x\n", port+0x40, ret);
#endif
  return ret;
}

void pit_outp(int port, int val)
{
  static int timer_beep;

  if (port == 1)
    i_printf("PORT: someone is writing the CMOS refresh time?!?");
  else if (port == 2 && config.speaker == SPKR_NATIVE) {
    safe_port_out_byte(0x42, val);
    return;
  }

  switch (pit[port].write_state) {
    case 0:
      pit[port].write_latch = pit[port].write_latch | ((val & 0xff) << 8);
      pit[port].write_state = 3;
      break;
    case 3:
      pit[port].write_latch = val & 0xff;
      pit[port].write_state = 0;
      break;
    case 1:
      pit[port].write_latch = val & 0xff;
      break;
    case 2:
      pit[port].write_latch = (val & 0xff) << 8;
      break;
    }
#if 0
  i_printf("PORT: pit_outp(0x%x, 0x%x)\n", port+0x40, val);
#endif

  if (pit[port].write_state != 0) {
    if (pit[port].write_latch == 0)
      pit[port].cntr = 0x10000;
    else
      pit[port].cntr = pit[port].write_latch;

    gettimeofday(&pit[port].time, NULL);

    if (port == 0) {
      ticks_accum   = 0;
      timer_div     = (pit[0].cntr * 10000) / CLOCK_TICK_RATE;
      if (timer_div == 0)
	timer_div = 1;
#if 0
      i_printf("timer_interrupt_rate requested %.3g Hz, granted %.3g Hz\n",
	       CLOCK_TICK_RATE/(double)pit[0].cntr, 10000.0/timer_div);
#endif
    }
    else if (port == 2 && config.speaker == SPKR_EMULATED) {
      if ((port61 & 3) == 3) {
	i_printf("PORT: Emulated beep!");
	putchar('\007');
      }
    }
  }
}

int pit_control_inp()
{
  return 0;
}

void pit_control_outp(int val)
{
  int latch = (val >> 6) & 0x03;

#if 0
  i_printf("PORT: outp(43, 0x%x)\n",val);
#endif

  switch (latch) {
    case 2:
      if (config.speaker == SPKR_NATIVE) {
        safe_port_out_byte(0x43, val);
	break;
      }
      /* nobreak; */
    case 0:
    case 1:
      if ((val & 0x30) == 0)
	pit_latch(latch);
      else {
	pit[latch].read_state  = (val >> 4) & 0x03;
	pit[latch].write_state = (val >> 4) & 0x03;
	pit[latch].mode        = (val >> 1) & 0x07;
      }
#if 0
      i_printf("PORT: writing outp(0x43, 0x%x)\n", val);
#endif
      break;
    case 3:
      /* I think this code is more or less correct */
      if ((val & 0x20) == 0) {        /* latch counts? */
	if (val & 0x02) pit_latch(0);
	if (val & 0x04) pit_latch(1);
	if (val & 0x08) pit_latch(2);
      }
      else if ((val & 0x10) == 0) {   /* latch status words? */
	if (val & 0x02) pit[0].mode |= 0x80;
	if (val & 0x04) pit[1].mode |= 0x80;
	if (val & 0x08) pit[2].mode |= 0x80;
      }
      break;
  }
}

int read_port61()
{
  if (config.speaker == SPKR_NATIVE)
    port61 = safe_port_in_byte(0x61);

  return port61;
}

void write_port61(int byte)
{
  port61 = byte & 0x0f;

  switch (config.speaker) {
    case SPKR_NATIVE:
      safe_port_out_byte(0x61, byte & 0x03);
      break;

    case SPKR_EMULATED:
      if (((byte & 3) == 3) && (pit[2].mode == 2 || pit[2].mode == 3)) {
	i_printf("PORT: emulated beep!\n");
	putchar('\007');
      }
      break;
  }
}

/* DANG_BEGIN_FUNCTION timer_int_engine
 *
 * This is experimental TIMER-IRQ CHAIN code!
 * This is a function to determine whether it is time to invoke a
 * new timer irq 0 event.  Normally it is 18 times a second, but
 * many video games set it to 100 times per second or more.  Since
 * the kernel cannot keep an accurate timer interrupt, the job of this
 * routine is to perform a chained timer irq 0 right after the previous
 * timer irq 0.  This routine should, ideally, be called right after
 * the end of a timer irq, if possible.
 *
 * This would speed up high frequency timer interrupts if this code
 * can be converted into an assembly macro equivalent!
 *
 * PLEASE NOTE
 *
 * This code has been replaced by interrupt scheduling code in pic.
 * The result is that we simply call pic_sched and run the dos interrupt.
 * If the new code causes no problems, I'll revise this section permanently. 
 *
 * DANG_END_FUNCTION
 */
#if 0
void timer_int_engine(void)
{
  static continuous = 0;

  if (ticks_accum < timer_div)
    continuous = 0;                     /* Reset overflow guard */
  else {

    /* Overflow guard so that there is no more than 50 irqs in a chain */
    continuous++;
    
    if (continuous > 50) 
      continuous = 0;                   /* Reset overflow guard */
    else {
        pic_request(PIC_IRQ0);            /* Start a timer irq */
    }

    /* Decrement the ticks accumulator */
    ticks_accum -= timer_div;
    
    /* Prevent more than 250 ms of extra timer ticks from building up.
     * This ensures that if timer_int_engine get called less frequently
     * than every 250ms, the code won't choke on an overload of timer 
     * interrupts.
     */
    if (ticks_accum > 250) ticks_accum -= 250;
  }
}
#else
void timer_int_engine(void)
{
 do_irq();
 pic_sched(PIC_IRQ0,pit[0].cntr);
}

#endif
