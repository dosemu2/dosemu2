/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* DANG_BEGIN_MODULE
 * 
 * REMARK
 * ser_irq.c: Serial interrupt services for DOSEMU
 * Please read the README.serial file in this directory for more info!
 * 
 * Copyright (C) 1995 by Mark Rejhon
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * /REMARK
 * This module is maintained by Mark Rejhon at these Email addresses:
 *      marky@magmacom.com
 *      ag115@freenet.carleton.ca
 *
 * DANG_END_MODULE
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "config.h"
#include "emu.h"
#include "timers.h"
#include "pic.h"
#include "serial.h"
#include "ser_defs.h"

/**************************************************************************/
/*                          The SERIAL ENGINES                            */
/**************************************************************************/

/* This function updates the timer counters for several serial events 
 * which includes: Transmission so that data gets written to the serial 
 * device at a constant rate, without buffer overflows.  Receive, so that
 * the read() system call is not done more often than needed.  Modem
 * Status check, so that it is not done more often than needed.
 *
 * In short, the serial timer ensures that the serial emulation runs
 * more smoothly, and does not put a heavy load on the system.
 */
/* NOTE: with the async notifications we dont need that any more.
 * Disabled  -- stsp
 */
#if 0
void serial_timer_update(void)
{
  static hitimer_t oldtp = 0;	/* Timer value from last call */
  hitimer_t tp;			/* Current timer value */
  unsigned long elapsed;	/* No of 115200ths seconds elapsed */
  int i;

  /* Get system time.  PLEASE DONT CHANGE THIS LINE, unless you can 
   * _guarantee_ that the substitute/stored timer value _is_ up to date 
   * at _this_ instant!  (i.e: vm86s exit time did not not work well)
   */
  tp = GETusTIME(0);
  if (oldtp==0)	oldtp=tp;
  /* compute the number of 115200ths of seconds since last timer update */
  elapsed = T64DIV((tp-oldtp),8.680555556);

  /* Save the old timer values for next time */
  oldtp = tp;

  /* Update all the timers */
  for (i = 0; i < config.num_ser; i++) {
    /* Countdown before next character will be transmitted */
    if (com[i].tx_timer > elapsed)
      com[i].tx_timer -= elapsed;
    else
      com[i].tx_timer = 0;

    /* Countdown to next Modem Status check */
    if (com[i].ms_timer > elapsed)
      com[i].ms_timer -= elapsed;
    else
      com[i].ms_timer = 0;

    /* Countdown before next read() of the serial port */
    if (com[i].rx_timer > elapsed)
      com[i].rx_timer -= elapsed;
    else
      com[i].rx_timer = 0;
  }
}
#endif

/* This function does housekeeping for serial receive operations.  Duties 
 * of this function include keeping the UART FIFO/RBR register filled,
 * and checking if it's time to generate a hardware interrupt (RDI).
 * [num = port]
 */
void receive_engine(int num)	/* Internal 16550 Receive emulation */ 
{
  if (com[num].MCR & UART_MCR_LOOP) return;	/* Return if loopback */

  uart_fill(num);

  if (com[num].LSR & UART_LSR_DR) {	/* Is data waiting? */
    if (com[num].fifo_enable) {		/* Is it in FIFO mode? */
      if (com[num].rx_timeout) {		/* Has get_rx run since int? */
        com[num].rx_timeout--;			/* Decrement counter */
        if (!com[num].rx_timeout) {		/* Has timeout counted down? */
          com[num].LSRqueued |= UART_LSR_DR;
          if(s3_printf) s_printf("SER%d: Func receive_engine requesting RX_INTR\n",num);
          serial_int_engine(num, RX_INTR);	/* Update interrupt status */
        }
      }
    }
    else { 				/* Not in FIFO mode */
      if (com[num].rx_timeout) {		/* Has get_rx run since int? */
        com[num].LSRqueued |= UART_LSR_DR;
        com[num].rx_timeout = 0;		/* Reset timeout counter */
        if(s3_printf) s_printf("SER%d: Func receive_engine requesting RX_INTR\n",num);
        serial_int_engine(num, RX_INTR);	/* Update interrupt status */
      }
    }
  }
}


/* This function does housekeeping for serial transmit operations.
 * Duties of this function include attempting to transmit the data out
 * of the XMIT FIFO/THR register, and checking if it's time to generate
 * a hardware interrupt (THRI).    [num = port]
 */
void transmit_engine(int num) /* Internal 16550 Transmission emulation */
{
/* how many bytes left in output queue when signalling interrupt to DOS */
#define QUEUE_THRESHOLD 2

  int rtrn, control, queued;
#if 0
  /* Give system time to transmit */
  /* Disabled timer stuff, not needed any more  -- stsp */
  if (com[num].tx_timer > TX_BUF_THRESHOLD) return;
#endif
  /* If Linux is handling flow control, then check the CTS state.
   * If the CTS state is low, then don't start new transmit interrupt!
   */
  if (com[num].system_rtscts) {
    ioctl(com[num].fd, TIOCMGET, &control);	/* WARNING: Non re-entrant! */
    if (!(control & TIOCM_CTS)) return;		/* Return if CTS is low */
  }

  /* find out how many bytes are queued by tty */
  rtrn = ioctl(com[num].fd, TIOCOUTQ, &queued);
  if (rtrn == -1)
    queued = 0;
  if (debug_level('s') > 5)
    s_printf("SER%d: ret=%i queue=%i\n", num, rtrn, queued);
  
  if (com[num].fifo_enable) {  /* Is FIFO enabled? */
    if (com[num].tx_overflow){
      if(RPT_SYSCALL(write(com[num].fd,
        &com[num].tx_buf[com[num].tx_buf_start], 1))!=1) {
        return;  /* write error */
      } else {
        /* char was written */
        com[num].tx_buf_start++;
        /* Squeeze char into FIFO */
	tx_buffer_slide(num);
        com[num].tx_buf[com[num].tx_buf_end] = com[num].TX ;
        /* Update FIFO queue pointers */
        com[num].tx_buf_end++;
	if (com[num].tx_buf_end > TX_BUFFER_SIZE) {
	  error("SER%d: BUG: transmit buffer overflowed, couldn't happen...\n", num);
	}
        com[num].tx_overflow = 0;                  /* Exit overflow state */
      }
    }
    /* Clear as much of the transmit FIFO as possible! */
    if (TX_BUF_BYTES(num)) {
      rtrn = RPT_SYSCALL(write(com[num].fd,
        &com[num].tx_buf[com[num].tx_buf_start], TX_BUF_BYTES(num)));
      if (rtrn <= 0) return;				/* Exit Loop if fail */
      com[num].tx_buf_start += rtrn;
      tx_buffer_slide(num);
      return;		/* return and give the system time to transfer */
    }
    /* Is FIFO empty, and is it time to trigger an xmit int? */
    if (!TX_BUF_BYTES(num) && queued <= QUEUE_THRESHOLD && com[num].tx_trigger) {
      com[num].tx_trigger = 0;
      com[num].LSRqueued |= UART_LSR_TEMT | UART_LSR_THRE;
      if(s3_printf) s_printf("SER%d: Func transmit_engine requesting TX_INTR\n",num);
      serial_int_engine(num, TX_INTR);		/* Update interrupt status */
    }
  }
  else {					/* Not in FIFO mode */
    if (com[num].tx_overflow) {        /* Is it in overflow state? */
      rtrn = RPT_SYSCALL(write(com[num].fd, &com[num].TX, 1));  /* Write port */
      if (rtrn == 1)                               /* Did it succeed? */
        com[num].tx_overflow = 0;                  /* Exit overflow state */
      return;
    }
    if (com[num].tx_trigger && queued <= QUEUE_THRESHOLD) {	/* Is it time to trigger int */
      com[num].tx_trigger = 0;
      com[num].LSRqueued |= UART_LSR_TEMT | UART_LSR_THRE;
      if(s3_printf) s_printf("SER%d: Func transmit_engine requesting TX_INTR\n",num);
      serial_int_engine(num, TX_INTR);		/* Update interrupt status */
    }  
  }
}


/* This function does housekeeping for modem status operations.
 * Duties of this function include polling the serial line for its status
 * and updating the MSR, and checking if it's time to generate a hardware
 * interrupt (MSI).    [num = port]
 */
void modstat_engine(int num)		/* Internal Modem Status processing */ 
{
  int control;
  int newmsr, delta;

  /* Return if in loopback mode */
  if (com[num].MCR & UART_MCR_LOOP) return;

#if 0
  /* Return if it is not time to do a modem status check */  
  if (com[num].ms_timer > 0) return;
  com[num].ms_timer += MS_MIN_FREQ;
#endif

  if(com[num].pseudo) {
    newmsr = UART_MSR_CTS | UART_MSR_DSR | UART_MSR_DCD;
  } else {
    ioctl(com[num].fd, TIOCMGET, &control);	/* WARNING: Non re-entrant! */
    newmsr = convert_bit(control, TIOCM_CTS, UART_MSR_CTS) |
             convert_bit(control, TIOCM_DSR, UART_MSR_DSR) |
             convert_bit(control, TIOCM_RNG, UART_MSR_RI) |
             convert_bit(control, TIOCM_CAR, UART_MSR_DCD);
  }
           
  delta = msr_compute_delta_bits(com[num].MSR, newmsr);
  
  com[num].MSRqueued = (com[num].MSRqueued & UART_MSR_DELTA) | newmsr | delta;

  if (delta) {
    if(s2_printf) s_printf("SER%d: Modem Status Change: MSR -> 0x%x\n",num,newmsr);
    if(s3_printf) s_printf("SER%d: Func modstat_engine requesting MS_INTR\n",num);
    serial_int_engine(num, MS_INTR);		/* Update interrupt status */
  }
  else if ( !(com[num].MSRqueued & UART_MSR_DELTA)) {
    /* No interrupt is going to occur, keep MSR updated including
     * leading-edge RI signal which does not cause an interrupt 
     */
    com[num].MSR = com[num].MSRqueued;
  }
}

/* There is no need for a Line Status Engine function right now since there
 * is not much justification for one, since all the error bits that causes
 * interrupt, are handled by Linux and are difficult for this serial code
 * to detect.  When easy non-blocking break signal handling is introduced,
 * there will be justification for a simple Line Status housekeeping
 * function whose purpose is to detect an error condition (mainly a
 * break signal sent from the remote) and generate a hardware interrupt
 * on its occurance (RLSI).
 *
 * DANG_BEGIN_REMARK
 * Linux code hackers: How do I detect a break signal without having
 * to rely on Linux signals?  Can I peek a 'break state bit'?
 * Also, how do I 'turn on' and 'turn off' the break state, via
 * an ioctl() or tcsetattr(), rather than using POSIX tcsendbrk()?
 * DANG_END_REMARK
 */
#if 0
void linestat_engine(int num)           /* Internal Line Status processing */
{
  s3_printf("SER%d: Func linestat_engine setting LS_INTR\n",num);
  serial_int_engine(num, LS_INTR);              /* Update interrupt status */
}
#endif

/* This function updates the IIR for Line Status.  [num = port] */
static inline void flag_IIR_linestat(int num)
{
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_RLSI;
}


/* This function updates the IIR for Receive.  [num = port] */
static inline void flag_IIR_receive(int num)
{
  /* Update the IIR for RDI status */
  if (!com[num].fifo_enable)
    com[num].IIR = UART_IIR_RDI;
  else if (RX_BUF_BYTES(num) >= com[num].rx_fifo_trigger)
    com[num].IIR = UART_IIR_FIFO | (com[num].IIR & UART_IIR_CTI) | UART_IIR_RDI;  else
    com[num].IIR = UART_IIR_FIFO | UART_IIR_CTI;
}

/* This function updates the IIR for Transmit.  [num = port] */
static inline void flag_IIR_transmit(int num)
{
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_THRI;
}


/* This function updates the IIR for Modem Status.  [num = port] */
static inline void flag_IIR_modstat(int num)
{
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_MSI;
}


/* This function updates the IIR for No Interrupt.  [num = port] */
static inline void flag_IIR_noint(int num)
{
  com[num].IIR = UART_IIR_NO_INT;
}


/* This function updates the queued Modem Status and Line Status registers.
 * This also updates interrupt-condition flags used to identify a potential
 * condition for the occurance of a serial interrupt.  [num = port]
 */
static inline int check_and_update_uart_status(int num)
{
  /* Check and update queued Line Status condition */
  if (com[num].LSRqueued & UART_LSR_ERR) {
    com[num].LSR |= (com[num].LSRqueued & UART_LSR_ERR);
    com[num].LSRqueued &= ~UART_LSR_ERR;
    com[num].int_condition |= LS_INTR;
  }
  else if (! (com[num].LSR & UART_LSR_ERR)) {
    com[num].int_condition &= ~LS_INTR;
  }  

  /* Check and updated queued Receive condition. */
  /* Safety check, avoid receive interrupt while DLAB is set high */
  if ((com[num].LSRqueued & UART_LSR_DR) && !com[num].DLAB) {
    com[num].rx_timeout = 0;
    com[num].LSR |= UART_LSR_DR;
    com[num].LSRqueued &= ~UART_LSR_DR;
    com[num].int_condition |= RX_INTR;
  }
  else if ( !(com[num].LSR & UART_LSR_DR)) {
    com[num].int_condition &= ~RX_INTR;
  }
  
  /* Check and update queued Transmit condition */
  /* Safety check, avoid transmit interrupt while DLAB is set high */
  if ((com[num].LSRqueued & UART_LSR_THRE) && !com[num].DLAB) {
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
    com[num].LSRqueued &= ~(UART_LSR_TEMT | UART_LSR_THRE);
    com[num].int_condition |= TX_INTR;
  }
  else if ( !(com[num].LSR & UART_LSR_THRE)) {
    com[num].int_condition &= ~TX_INTR;
  }
  
  /* Check and update queued Modem Status condition */
  if (com[num].MSRqueued & UART_MSR_DELTA) {
    com[num].MSR = (com[num].MSR & UART_MSR_DELTA) | com[num].MSRqueued;
    com[num].MSRqueued &= ~(UART_MSR_DELTA);
    com[num].int_condition |= MS_INTR;
  } 
  else if ( !(com[num].MSR & UART_MSR_DELTA)) {
    com[num].int_condition &= ~MS_INTR;
  }
  return 1;
}


/* DANG_BEGIN_FUNCTION serial_int_engine
 * 
 * This function is the serial interrupts scheduler.  Its purpose is to
 * update interrupt status and/or invoke a requested serial interrupt.
 * If interrupts are not enabled, the Interrupt Identification Register 
 * is still updated and the function returns.  See pic_serial_run() below
 * it is executed right at the instant the interrupt is actually invoked.
 *
 * Since it is not possible to run the interrupt on the spot, it triggers
 * the interrupt via the pic_request() function (which is in pic.c)
 * and sets a flag that an interrupt is going to be occur soon.
 *
 * Please read pic_serial_run() for more information about interrupts.
 * [num = port, int_requested = the requested serial interrupt]
 * 
 * DANG_END_FUNCTION
 */
void serial_int_engine(int num, int int_requested)
{
  u_char tmp;
  
  /* Safety code to avoid receive and transmit while DLAB is set high */
  if (com[num].DLAB) int_requested &= ~(RX_INTR | TX_INTR);

  if (com[num].tx_trigger) {
    com[num].tx_timer = 0;
    transmit_engine(num);
  }
  
  check_and_update_uart_status(num);

  /* reset the served requests */
  com[num].int_request &= (com[num].int_condition & com[num].IER);
   
  /* Update the IIR status immediately */
  tmp = (com[num].int_condition & com[num].IER);

  if(s3_printf) s_printf("SER%d: tmp=%d int_cond=%d int_req=%d int=%d fifo=%i\n",
    num, tmp, com[num].int_condition, com[num].int_request, int_requested,
    com[num].fifo_enable);
  if (int_requested && !(int_requested & com[num].int_condition))
    s_printf("SER%d: Warning: int_condition is not set for the requested interrupt!\n", num);

  if (!tmp)
    flag_IIR_noint(num);		/* No interrupt */
  else if (tmp & LS_INTR)
    flag_IIR_linestat(num);		/* Line Status */
  else if (tmp & RX_INTR)
    flag_IIR_receive(num);		/* Receive */
  else if (tmp & TX_INTR)
    flag_IIR_transmit(num);		/* Transmit */
  else if (tmp & MS_INTR)
    flag_IIR_modstat(num);		/* Modem Status */

  if (com[num].int_request) {
    if(int_requested) s_printf("SER%d: Interrupt for another event (%d) is already requested\n",
	num, com[num].int_request);
    return;
  }

  if (!int_requested) {
    if (!tmp) {
      /* just an IIR update */
      return;
    }
    else {
      if(s3_printf) s_printf("SER%d: Requesting serial interrupt upon IIR update\n",
        num);
      int_requested = tmp;
    }
  }

  /* At this point, we don't much care which function is requested; that  
   * is taken care of in the serial_int_engine.  However, if interrupts are
   * disabled, then the Interrupt Identification Register must be set.
   */

  /* See if a requested interrupt is enabled */
  if (com[num].int_enab && com[num].interrupt && (int_requested & com[num].IER)) {
    
      if(s3_printf) s_printf("SER%d: Func pic_request intlevel=%d, int_requested=%d\n", 
                 num, com[num].interrupt, int_requested);
      pic_request(com[num].interrupt);		/* Flag PIC for interrupt */
  }
  else
    if(s3_printf) s_printf("SER%d: Interrupt %d (%d) cannot be requested: enable=%d IER=0x%x\n", 
        num, com[num].interrupt, int_requested, com[num].int_enab, com[num].IER);
}  


/* DANG_BEGIN_FUNCTION pic_serial_run 
 *
 * This function is called by the priority iunterrupt controller when a
 * serial interrupt occurs.  It executes the highest priority serial
 * interrupt for that port. (Priority order is: RLSI, RDI, THRI, MSI)
 *
 * Because it is theoretically possible for things to change between the
 * interrupt trigger and the actual interrupt, some checks must be 
 * repeated.
 *
 * DANG_END_FUNCTION
 */
int pic_serial_run(int ilevel)
{
/*  static u_char*/ int tmp;
/*  static u_char*/ int num;

  num = irq_source_num[ilevel];

  /* Update the queued Modem Status and Line Status values. */
  check_and_update_uart_status(num);

  /* Make sure requested interrupt is still enabled, then run it. 
   * These checks are in priority order.
   *
   * The UART MSR and LSR status is queued until interrupt time whenever 
   * possible, to minimize the chance of 'cancelling' an interrupt 
   * condition is requested via pic_request, but before the interrupt 
   * actually runs.
   *
   * However, there is enough latency between pic_request() and
   * the execution of the actual interrupt code, such that there are 
   * still cases where the UART emulation can be tricked to pic_request()
   * an interrupt, and then the interrupt condition is satisfied *before*
   * the interrupt code even executes!
   */
   
  tmp = com[num].int_condition & com[num].IER;
  
  if(s2_printf) s_printf("SER%d: ---BEGIN INTERRUPT--- int_condition = %02x\n", num,
             com[num].int_condition);

  /* Execute any pre-interrupt code that may be necessary */
  if (!tmp) {
    flag_IIR_noint(num);		/* No interrupt */
    com[num].int_request = 0;
  }
  else if (tmp & LS_INTR) {
    flag_IIR_linestat(num);		/* Line Status */
    com[num].int_request = LS_INTR;
  }
  else if (tmp & RX_INTR) {
    flag_IIR_receive(num);		/* Receive */
    com[num].int_request = RX_INTR;
  }
  else if (tmp & TX_INTR) {
    flag_IIR_transmit(num);		/* Transmit */
    com[num].int_request = TX_INTR;
  }
  else if (tmp & MS_INTR) {
    flag_IIR_modstat(num);		/* Modem Status */
    com[num].int_request = MS_INTR;
  }

  if (com[num].int_request)
    return 1;	/* run IRQ */
  else {				/* What?  We can't be this far! */
    s_printf("SER%d: Interrupt Error: cancelled serial interrupt!\n",num);  
    /* No interrupt flagged?  Then the interrupt was cancelled sometime
     * after the interrupt was flagged, but before pic_serial_run executed.
     * DANG_FIXTHIS how do we cancel a PIC interrupt, when we have come this far?
     */
  }
  return 0;
}



/**************************************************************************/
/*                         The MAIN ENGINE LOOP                           */
/**************************************************************************/

/* DANG_BEGIN_FUNCTION serial_run 
 *
 * This is the main housekeeping function, which should be called about
 * 20 to 100 times per second.  The more frequent, the better, up to 
 * a certain point.   However, it should be self-compensating if it
 * executes 10 times or even 1000 times per second.   Serial performance
 * increases with frequency of execution of serial_run.
 *
 * Serial mouse performance becomes more smooth if the time between 
 * calls to serial_run are smaller.
 *
 * DANG_END_FUNCTION
 */
void
serial_run(void)	
{
  int i;
#if 0
  /* Update the internal serial timers */
  serial_timer_update();
#endif
  /* Do the necessary interrupt checksing in a logically efficient manner.  
   * All the engines have built-in code to prevent loading the
   * system if they are called 100x's per second.
   */
  for (i = 0; i < config.num_ser; i++) {
    if (com[i].fd < 0) continue;
    receive_engine(i);		/* Receive operations */
    transmit_engine(i);		/* Transmit operations */
    modstat_engine(i);  	/* Modem Status operations */
  }
  return;
}
