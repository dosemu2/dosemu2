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
 * This code will actually generate 18.2 DOS interrupts/second (the code
 * here itself will be triggered about 100 times per second). It will even
 * happily attempt to generate faster clocks, right up to the point where
 * it chokes.  Since the absolute best case timing we can get out of Linux
 * is 100Hz, figure that anything approaching or exceeding that isn't going
 * to work well.  (The code will attempt to generate up to 10Khz interrupts
 * per second at the moment.  Too bad that would probably overflow all
 * internal queues really fast. :)
 *
 * Speaker emulation, now including port 61h, is also in here. [rz]
 *
 * DANG_END_MODULE
 *
 */

#include <sys/time.h>
#include <linux/kd.h>
#include "config.h"
#include "emu.h"
#include "port.h"
#include "timers.h"
#include "iodev.h"
#include "port.h"
#include "int.h"
#include "pic.h"
#include "dpmi.h"

#ifdef NEW_KBD_CODE
#include "keyb_server.h"
#endif


#ifndef MONOTON_MICRO_TIMING
#define CLOCK_TICK_RATE   1193180     /* underlying clock rate in HZ */
#else  /* MONOTON_MICRO_TIMING */
extern long   pic_itime[33];
#endif /* MONOTON_MICRO_TIMING */

pit_latch_struct pit[PIT_TIMERS];   /* values of 3 PIT counters */

static u_long timer_div;          /* used by timer int code */
static u_long ticks_accum;        /* For timer_tick function, 100usec ticks */

#ifdef NEW_KBD_CODE
static Bit8u port61 = 0x0e;
#else
extern int    port61;
#endif


/*
 * DANG_BEGIN_FUNCTION initialize_timers
 *
 * description:  
 * ensure the 0x40 port timer is initially set correctly
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
#ifndef MONOTON_MICRO_TIMING
  timer_div     = (pit[0].cntr * 10000) / CLOCK_TICK_RATE;
#else /* MONOTON_MICRO_TIMING */
  timer_div     = (pit[0].cntr * 10000) / PIT_TICK_RATE;
#endif /* MONOTON_MICRO_TIMING */

  timer_tick();  /* a starting tick! */

  port61 = 0x0e;
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


#ifdef NEW_KBD_CODE
/* this does the EMULATED mode speaker emulation */
void do_sound(Bit16u period) {
  if ((port61 & 3) == 3 && (pit[2].mode == 2 || pit[2].mode == 3)) {
     if (config.console && !config.X) {
        i_printf("SPEAKER: on, period=%d\n",pit[2].write_latch);
        do_ioctl(console_fd,KIOCSOUND,pit[2].write_latch & 0xffff);
     }
     else {
        i_printf("SPEAKER: Dumb emulated beep\n");
        putchar('\007');
     }
  }
  else if (config.console && !config.X) {
     i_printf("SPEAKER: sound OFF!\n");
     do_ioctl(console_fd,KIOCSOUND,0);
  }
}
#endif

 
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
#ifndef MONOTON_MICRO_TIMING
	ticks = (cur_time.tv_sec - pit[latch].time.tv_sec) * CLOCK_TICK_RATE +
	        ((cur_time.tv_usec - pit[latch].time.tv_usec) * 1193) / 1000;
#else /* MONOTON_MICRO_TIMING */
	ticks = (cur_time.tv_sec - pit[latch].time.tv_sec) * PIT_TICK_RATE +
	        PIT_MS2TICKS(cur_time.tv_usec - pit[latch].time.tv_usec);
#endif /* MONOTON_MICRO_TIMING */

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
#ifndef MONOTON_MICRO_TIMING
      ticks = pit[latch].cntr - (pic_dos_time % pit[latch].cntr);
      pit[latch].read_latch = pit[latch].cntr - ticks % pit[latch].cntr;
#else /* MONOTON_MICRO_TIMING */
      gettimeofday(&cur_time, NULL);
      pic_sys_time = cur_time.tv_sec*PIT_TICK_RATE 
	+ PIT_MS2TICKS(cur_time.tv_usec);
      pic_sys_time += (pic_sys_time == NEVER);
      if(latch == 0) {
	ticks = pic_itime[PIC_IRQ0] - pic_sys_time;
	if(((int)ticks) < 0) {
	  pic_request(PIC_IRQ0);
	  run_irqs();
	  ticks = pic_itime[PIC_IRQ0] - pic_sys_time;
	}
      } else {
	ticks = pit[latch].cntr - (pic_sys_time % pit[latch].cntr);
      }
      pit[latch].read_latch = ticks;
#endif /* MONOTON_MICRO_TIMING */
      break;

    case 0x03:  /* mode 3,7 -- countdown by 2(?), interrupt, reload */
    case 0x07:
      gettimeofday(&cur_time, NULL);
#ifndef MONOTON_MICRO_TIMING
      /* fancy calculations to avoid overflow */
      ticks = (cur_time.tv_sec - pit[latch].time.tv_sec) % pit[latch].cntr;
      ticks = ticks * (CLOCK_TICK_RATE % pit[latch].cntr) +
	      ((cur_time.tv_usec - pit[latch].time.tv_usec) * 1193) / 1000;
      pit[latch].read_latch = pit[latch].cntr - (2*ticks) % pit[latch].cntr;
#else /* MONOTON_MICRO_TIMING */
      pic_sys_time = cur_time.tv_sec*PIT_TICK_RATE 
	+ PIT_MS2TICKS(cur_time.tv_usec);
      pic_sys_time += (pic_sys_time == NEVER);

      if(latch == 0) {
	ticks = pic_itime[PIC_IRQ0] - pic_sys_time;
	if(((int)ticks) < 0) {
	  pic_request(PIC_IRQ0);
	  run_irqs();
	  ticks = pic_itime[PIC_IRQ0] - pic_sys_time;
	}
      } else {
	ticks = pit[latch].cntr - (pic_sys_time % pit[latch].cntr);
      }
      pit[latch].read_latch = (2*ticks) % pit[latch].cntr;
#endif /* MONOTON_MICRO_TIMING */
      break;
  }
#if 0 /* for debugging */
  i_printf("pit_latch:  latched value 0x%4.4x for timer %d\n",
	   pit[latch].read_latch, latch);
#endif
}

Bit8u pit_inp(Bit32u port)
{
  int ret = 0;
  port -= 0x40;

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
      if (pit[port].mode & 0x80) pit[port].mode &= 7;
        else
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

void pit_outp(Bit32u port, Bit8u val)
{

  port -= 0x40;
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
#ifndef MONOTON_MICRO_TIMING
      timer_div     = (pit[0].cntr * 10000) / CLOCK_TICK_RATE;
#else /* MONOTON_MICRO_TIMING */
      timer_div     = (pit[0].cntr * 10000) / PIT_TICK_RATE;
#endif /* MONOTON_MICRO_TIMING */
      if (timer_div == 0)
	timer_div = 1;
#if 0
      i_printf("timer_interrupt_rate requested %.3g Hz, granted %.3g Hz\n",
#ifndef MONOTON_MICRO_TIMING
	       CLOCK_TICK_RATE/(double)pit[0].cntr, 10000.0/timer_div);
#else /* MONOTON_MICRO_TIMING */
	       PIT_TICK_RATE/(double)pit[0].cntr, 10000.0/timer_div);
#endif /* MONOTON_MICRO_TIMING */
#endif
    }
    else if (port == 2 && config.speaker == SPKR_EMULATED) {
#ifdef NEW_KBD_CODE
      do_sound(pit[2].write_latch & 0xffff);
#else
      if ((port61 & 3) == 3) {
	i_printf("PORT: Emulated beep!");
	putchar('\007');
      }
#endif
    }
  }
}

Bit8u pit_control_inp(Bit32u port)
{
  return 0;
}

void pit_control_outp(Bit32u port, Bit8u val)
{
  int latch = (val >> 6) & 0x03;

#if 0
  i_printf("PORT: outp(0x43, 0x%x)\n",val);
#endif

  /* Timer commands (00-BF):
   *  bit 6-7 = Timer (0,1,2)
   *		3 is not a timer but a special read-back command
   *
   *    bit 4-5 = command 00=latch 01=LSB mode 10=MSB mode 11=16bit mode
   *    bit 1-3 = mode selection
   *                    mode 0(000) 1(001) 2(010 or 110) 3(011 or 111)
   *                         4(100) 5(101)
   *			6(110) and 7(111) undefined?
   *    bit 0   = binary(0), BCD(1)
   *
   *  0xh	counter latch timer 0
   *  1xh	timer 0 LSB mode
   *			modes: 0,2,3,4 - 1,5 not applicable
   *  2xh	timer 0 MSB mode
   *			modes: 0,2,3,4 - 1,5 not applicable
   *  3xh	timer 0 16bit mode
   *			modes: 0,2,4 - 3 typical - 1,5 not applicable
   *  modes 1,5 applicable only to timer 2 [van Gilluwe,1994]
   */
  switch (latch) {
    case 2:
      if (config.speaker == SPKR_NATIVE) {
        safe_port_out_byte(0x43, val);
	break;
      }
      /* nobreak; */
    case 0:
    case 1:
#ifdef MONOTON_MICRO_TIMING
      pit[latch].mode        = (val >> 1) & 0x07;
      if ((val & 4)==0) {      /* modes 0,1,4,5 */
      /* set the time base for the counter - safety code for programs
       * which use a non-periodical mode without reloading the counter
       */
       gettimeofday(&pit[latch].time, NULL);
      }
#endif
      if ((val & 0x30) == 0)
	pit_latch(latch);
      else {
	pit[latch].read_state  = (val >> 4) & 0x03;
	pit[latch].write_state = (val >> 4) & 0x03;
#ifndef MONOTON_MICRO_TIMING
	pit[latch].mode        = (val >> 1) & 0x07;
#endif
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
#if 0  /* =0, if old engine */
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
#else /* new engine */
void timer_int_engine(void)
{
/* Try to terminate previous int8 routine before scheduling
 * another one. Without this one dope crashes (DPMI_rm_procedure_
 * running increases up to a stack fault)
 */
 if (in_dpmi) {
   while (in_dpmi_pm_int==8) run_dpmi();
 }
 pic_sched(PIC_IRQ0,pit[0].cntr);
 do_irq();
}

#ifdef NEW_KBD_CODE
/* reads/writes to the speaker control port (0x61)
 */
Bit8u spkr_io_read(Bit32u port) {
   if (port==0x61)  {
      if (config.speaker == SPKR_NATIVE)
         return port_safe_inb(0x61);
      else
         return port61;
   }
   return 0xff;
}

void spkr_io_write(Bit32u port, Bit8u value) {
   if (port==0x61) {
      switch (config.speaker) {
       case SPKR_NATIVE:
          port_safe_outb(0x61, value & 0x03);
          break;
      
       case SPKR_EMULATED:
	  port61 = value & 0x0f;
          do_sound(pit[2].write_latch & 0xffff);
	  break;

       case SPKR_OFF:
          port61 = value & 0x0f;
          break;
      }
   }
}
#endif

void pit_init(void)
{
  emu_iodev_t  io_device;

  /* 8254 PIT (Programmable Interval Timer) */
  io_device.read_portb   = pit_inp;
  io_device.write_portb  = pit_outp;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.handler_name = "8254 PIT";
  io_device.start_addr   = 0x0040;
  io_device.end_addr     = 0x0042;
  io_device.irq          = 0;
  port_register_handler(io_device);

  io_device.read_portb   = pit_control_inp;
  io_device.write_portb  = pit_control_outp;
  io_device.start_addr   = 0x0043;
  io_device.end_addr     = 0x0043;
  port_register_handler(io_device);

#if 0
  io_device.start_addr   = 0x0047;
  io_device.end_addr     = 0x0047;
  port_register_handler(io_device);
#endif

#ifdef NEW_KBD_CODE /* This code conflicts with the old keyboard code! 16 Sept 96*/
  io_device.read_portb   = spkr_io_read;
  io_device.write_portb  = spkr_io_write;
  io_device.handler_name = "Speaker port";
  io_device.start_addr   = 0x0061;
  io_device.end_addr     = 0x0061;
  io_device.irq          = EMU_NO_IRQ;
  port_register_handler(io_device);
#endif
}

void pit_reset(void)
{
  struct timeval cur_time;

  gettimeofday(&cur_time, NULL);

  pit[0].mode        = 3;
  pit[0].cntr        = 0x10000;
  pit[0].time        = cur_time;
  pit[0].read_latch  = 0xffffffff;
  pit[0].write_latch = 0;
  pit[0].read_state  = 3;
  pit[0].write_state = 3;

  pit[1].mode        = 2;
  pit[1].cntr        = 18;
  pit[1].time        = cur_time;
  pit[1].read_latch  = 0xffffffff;
  pit[1].write_latch = 18;
  pit[1].read_state  = 3;
  pit[1].write_state = 3;

  pit[2].mode        = 0;
  pit[2].cntr        = 0x10000;
  pit[2].time        = cur_time;
  pit[2].read_latch  = 0xffffffff;
  pit[2].write_latch = 0;
  pit[2].read_state  = 3;
  pit[2].write_state = 3;

  pit[3].mode        = 0;
  pit[3].cntr        = 0x10000;
  pit[3].time        = cur_time;
  pit[3].read_latch  = 0xffffffff;
  pit[3].write_latch = 0;
  pit[3].read_state  = 3;
  pit[3].write_state = 3;

#if 0
  timer_handle = timer_create(pit_timer_func, NULL, pit_timer_usecs(0x10000));
#endif
#ifdef NEW_KBD_CODE
  port61 = 0;
  do_ioctl(console_fd,KIOCSOUND,0);    /* sound off */
#endif
}
#endif /* new engine */
