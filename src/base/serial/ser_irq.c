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
 * This module is maintained by Stas Sergeev <stsp@users.sourceforge.net>
 *
 * DANG_END_MODULE
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "timers.h"
#include "pic.h"
#include "serial.h"
#include "ser_defs.h"
#include "tty_io.h"

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
void receive_engine(int num, int size)	/* Internal 16550 Receive emulation */
{
  if (com[num].MCR & UART_MCR_LOOP) return;	/* Return if loopback */

  if (RX_BUF_BYTES(num) == size && FIFO_ENABLED(num)) /* if fifo was empty */
    com[num].rx_timeout = TIMEOUT_RX;	/* set timeout counter */

  if (RX_BUF_BYTES(num)) {
    com[num].LSR |= UART_LSR_DR;		/* Set recv data ready bit */
    /* Has it gone above the receive FIFO trigger level? */
    if (!FIFO_ENABLED(num) || RX_BUF_BYTES(num) >= com[num].rx_fifo_trigger) {
      if(s3_printf) s_printf("SER%d: Func uart_fill requesting RX_INTR\n",num);
      serial_int_engine(num, RX_INTR);	/* Update interrupt status */
    }
  }

  if (FIFO_ENABLED(num) && RX_BUF_BYTES(num) && com[num].rx_timeout) {		/* Is it in FIFO mode? */
    com[num].rx_timeout--;			/* Decrement counter */
    if (!com[num].rx_timeout) {		/* Has timeout counted down? */
      com[num].IIR.flg.cti = 1;
      if(s3_printf) s_printf("SER%d: Receive timeout (%i bytes), requesting RX_INTR\n",
          num, RX_BUF_BYTES(num));
      serial_int_engine(num, RX_INTR);	/* Update interrupt status */
    }
  }
}

static void update_tx_cnt(int num)
{
  /* find out how many bytes are queued by tty - may be slow */
  int queued = serial_get_tx_queued(num);
  if (queued < 0)
    queued = 0;
  if (queued > com[num].tx_cnt)
    s_printf("SER%d: ERROR: queued=%i tx_cnt=%i\n", num, queued, com[num].tx_cnt);
  com[num].tx_cnt = queued;
}

/* This function does housekeeping for serial transmit operations.
 * Duties of this function include attempting to transmit the data out
 * of the XMIT FIFO/THR register, and checking if it's time to generate
 * a hardware interrupt (THRI).    [num = port]
 */
void transmit_engine(int num) /* Internal 16550 Transmission emulation */
{
/* how many bytes left in output queue when signalling interrupt to DOS */
#define QUEUE_THRESHOLD 14

  if (!TX_TRIGGER(num)) {
    if (!(com[num].LSR & UART_LSR_TEMT)) {
#if 1
/* the below code doesn't work right because of this slowdown patch:
 * http://lkml.indiana.edu/hypermail/linux/kernel/1210.1/01456.html
 * Disable it for now.
 *
 * ... and reenable thanks to this patch:
 * https://lkml.org/lkml/2013/5/5/49
 */
      if (com[num].tx_cnt)
        update_tx_cnt(num);
      if (!com[num].tx_cnt)
#endif
        com[num].LSR |= UART_LSR_TEMT;
    }
    return;
  }

  /* If Linux is handling flow control, then check the CTS state.
   * If the CTS state is low, then don't start new transmit interrupt!
   */
  if (com_cfg[num].system_rtscts) {
    int cts = (serial_get_msr(num) & UART_MSR_CTS);
    if (!cts)
      return;		/* Return if CTS is low */
  }

  if (com[num].tx_cnt > QUEUE_THRESHOLD)
    update_tx_cnt(num);
  if (debug_level('s') > 5)
    s_printf("SER%d: queued=%i\n", num, com[num].tx_cnt);
  if (com[num].tx_cnt > QUEUE_THRESHOLD)
    return;

  com[num].LSR |= UART_LSR_THRE;
  if (!com[num].tx_cnt)
    com[num].LSR |= UART_LSR_TEMT;
  if(s3_printf) s_printf("SER%d: Func transmit_engine requesting TX_INTR\n",num);
  serial_int_engine(num, TX_INTR);		/* Update interrupt status */
}


/* This function does housekeeping for modem status operations.
 * Duties of this function include polling the serial line for its status
 * and updating the MSR, and checking if it's time to generate a hardware
 * interrupt (MSI).    [num = port]
 */
void modstat_engine(int num)		/* Internal Modem Status processing */
{
  int newmsr, delta;

  /* Return if in loopback mode */
  if (com[num].MCR & UART_MCR_LOOP) return;

#if 0
  /* Return if it is not time to do a modem status check */
  if (com[num].ms_timer > 0) return;
  com[num].ms_timer += MS_MIN_FREQ;
#endif

  if(com_cfg[num].pseudo)
    newmsr = UART_MSR_CTS | UART_MSR_DSR | UART_MSR_DCD;
  else
    newmsr = serial_get_msr(num);
  delta = msr_compute_delta_bits(com[num].MSR, newmsr);

  com[num].MSR = (com[num].MSR & UART_MSR_DELTA) | newmsr | delta;

  if (delta) {
    if(s2_printf) s_printf("SER%d: Modem Status Change: MSR -> 0x%x\n",num,newmsr);
    if(s3_printf) s_printf("SER%d: Func modstat_engine requesting MS_INTR\n",num);
    serial_int_engine(num, MS_INTR);		/* Update interrupt status */
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
  /* Safety code to avoid receive and transmit while DLAB is set high */
  if (DLAB(num)) int_requested &= ~(RX_INTR | TX_INTR);

  if (TX_TRIGGER(num)) {
    transmit_engine(num);
  }

  if (!int_requested && !com[num].int_condition)
    return;
  com[num].int_condition |= int_requested;

  /* At this point, we don't much care which function is requested; that
   * is taken care of in the serial_int_engine.  However, if interrupts are
   * disabled, then the Interrupt Identification Register must be set.
   */

  /* See if a requested interrupt is enabled */
  if (INT_ENAB(num) && INT_REQUEST(num)) {
      if(s3_printf) s_printf("SER%d: Func pic_request intlevel=%d, int_requested=%d\n",
                 num, com[num].interrupt, int_requested);
      pic_request(com[num].interrupt);		/* Flag PIC for interrupt */
  }
  else
    if(s3_printf) s_printf("SER%d: Interrupt %d (%d) cannot be requested: enable=%d IER=0x%x\n",
        num, com[num].interrupt, com[num].int_condition, INT_ENAB(num), com[num].IER);
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
  int i, ret = 0;

  for (i = 0; i < config.num_ser; i++) {
    if (com[i].interrupt != ilevel)
      continue;
    if (INT_REQUEST(i))
      ret++;
  }

  if(s2_printf) s_printf("SER: ---BEGIN INTERRUPT---\n");

  if (!ret) {
    s_printf("SER: Interrupt Error: cancelled serial interrupt!\n");
    /* No interrupt flagged?  Then the interrupt was cancelled sometime
     * after the interrupt was flagged, but before pic_serial_run executed.
     * DANG_FIXTHIS how do we cancel a PIC interrupt, when we have come this far?
     */
  }
  return ret;
}



/**************************************************************************/
/*                         The MAIN ENGINE LOOP                           */
/**************************************************************************/

void serial_update(int num)
{
  int size = 0;

#ifdef USE_MODEMU
  if (com_cfg[num].vmodem)
    modemu_update(num);
#endif
  /* optimization: don't read() when enough data buffered */
  if (RX_BUF_BYTES(num) < com[num].rx_fifo_trigger)
    size = uart_fill(num);
  if (size > 0)
    receive_engine(num, size);		/* Receive operations */
  else if (RX_BUF_BYTES(num))
    receive_engine(num, 0);		/* Handle timeouts */
  transmit_engine(num);		/* Transmit operations */
  modstat_engine(num);  	/* Modem Status operations */
}
