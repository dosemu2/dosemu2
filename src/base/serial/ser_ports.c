/* DANG_BEGIN_MODULE
 *
 * REMARK
 * ser_ports.c: Serial ports for DOSEMU: Software emulated 16550 UART!
 * Please read the README.serial file in this directory for more info!
 *
 * Copyright (C) 1995 by Mark Rejhon
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This module is maintained by Stas Sergeev <stsp@users.sourceforge.net>
 *
 * /REMARK
 * DANG_END_MODULE
 */

/**************************** DECLARATIONS *******************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "emu.h"
#include "ser_defs.h"
#include "tty_io.h"


/*************************************************************************/
/*                 MISCELLANOUS serial support functions                 */
/*************************************************************************/

/* This is a function used for translating a bit to a different bit,
 * depending on a bit inside an integer 'testval'.  It will return 'outbit'
 * if the bit 'inbit' is set in 'testval'.
 */
inline int convert_bit(int testval, int inbit, int outbit)
{
  if (testval & inbit)
    return outbit;
  else
    return 0;
}


/* This function slides the contents of the receive buffer to the
 * bottom of the buffer.  A sliding buffer is used instead of
 * a circular buffer because this way, a single read() can easily
 * put data straight into our internal receive buffer!
 */
void rx_buffer_slide(int num)
{
  if (com[num].rx_buf_start == 0)
    return;
  /* Move existing chars in receive buffer to the start of buffer */
  memmove(com[num].rx_buf, com[num].rx_buf + com[num].rx_buf_start,
    RX_BUF_BYTES(num));

  /* Update start and end pointers in buffer */
  com[num].rx_buf_end -= com[num].rx_buf_start;
  com[num].rx_buf_start = 0;
}

static void clear_int_cond(int num, u_char val)
{
  com[num].int_condition &= ~val;
  /* reset IIR too, to recalculate later */
  com[num].IIR.mask = 0;
}

static void recalc_IIR(int num)
{
  int tmp;
#if 0
  /* disabled being too expensive... */
  serial_update(num);
#endif
  tmp = INT_REQUEST(num);
  if (!tmp)
    com[num].IIR.mask = 0;
  else if (tmp & LS_INTR)
    com[num].IIR.mask = LS_INTR;
  else if (tmp & RX_INTR)
    com[num].IIR.mask = RX_INTR;
  else if (tmp & TX_INTR)
    com[num].IIR.mask = TX_INTR;
  else if (tmp & MS_INTR)
    com[num].IIR.mask = MS_INTR;
}

static u_char get_IIR_val(int num)
{
  u_char val = com[num].IIR.flags << 3;
  switch (com[num].IIR.mask) {
  case 0:
    val |= UART_IIR_NO_INT;
    break;
  case LS_INTR:
    val |= UART_IIR_RLSI;
    break;
  case RX_INTR:
    val |= UART_IIR_RDI;
    break;
  case TX_INTR:
    val |= UART_IIR_THRI;
    break;
  case MS_INTR:
    val |= UART_IIR_MSI;
    break;
  }
  return val;
}

/* This function clears the specified XMIT and/or RCVR FIFO's.  This is
 * done on initialization, and when changing from 16450 to 16550 mode, and
 * vice versa.  [num = port, fifo = flags to indicate which fifos to clear]
 */
void uart_clear_fifo(int num, int fifo)
{
  /* DANG_FIXTHIS Should clearing UART cause THRE int if it's enabled? */

  if(s1_printf) s_printf("SER%d: Clear FIFO.\n",num);

  /* Clear the receive FIFO */
  if (fifo & UART_FCR_CLEAR_RCVR) {
    /* Preserve THR empty state, clear error bits and recv data ready bit */
    com[num].LSR &= ~(UART_LSR_ERR | UART_LSR_DR);
    com[num].rx_buf_start = 0;		/* Beginning of rec FIFO queue */
    com[num].rx_buf_end = 0;		/* End of rec FIFO queue */
    com[num].IIR.flg.cti = 0;		/* clear timeout */
    com[num].rx_timeout = 0;		/* Receive intr already occured */
    clear_int_cond(num, LS_INTR | RX_INTR);  /* Clear LS/RX conds */
    rx_buffer_dump(num);		/* Clear receive buffer */
  }

  /* Clear the transmit FIFO */
  if (fifo & UART_FCR_CLEAR_XMIT) {
    /* Preserve recv data ready bit and error bits, and set THR empty */
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
    clear_int_cond(num, TX_INTR);	/* Clear TX int condition */
    tx_buffer_dump(num);		/* Clear transmit buffer */
  }
}


/*************************************************************************/
/*                      RECEIVE handling functions                       */
/*************************************************************************/

/* This function returns a received character.
 * The character comes from the RBR register (or FIFO), which would have
 * been previously filled by the uart_fill routine (which does the
 * actual reads from the serial line) or loopback code.  This function
 * usually runs through the do_serial_in routine.    [num = port]
 */
static int get_rx(int num)
{
  int val;
  com[num].rx_timeout = 0;		/* Reset timeout counter */
  com[num].IIR.flg.cti = 0;

  /* if no data, try to get some */
  if (!RX_BUF_BYTES(num)) {
    int size = uart_fill(num);
    if (size > 0)
      receive_engine(num, size);
  }
  /* if still no data, go out */
  if (!RX_BUF_BYTES(num)) {
    if (com[num].LSR & UART_LSR_DR) {
      error("COM%i: DR set but buffer empty\n", num);
      com[num].LSR &= ~UART_LSR_DR;
    }
    return 0;
  }

  /* Get byte from internal receive queue */
  val = com[num].rx_buf[com[num].rx_buf_start++];
  /* Clear data waiting status and interrupt condition flag */
  clear_int_cond(num, RX_INTR);
  /* and see if more to read */
  receive_engine(num, 0);

  if (!RX_BUF_BYTES(num))
    com[num].LSR &= ~UART_LSR_DR;

  return val;		/* Return received byte */
}


/*************************************************************************/
/*                    MODEM STATUS handling functions                    */
/*************************************************************************/




/* The following computes the MSR delta bits.  It is done by computing
 * the difference between two MSR values (oldmsr and newmsr) by xor'ing
 * bits 4-7 of them.   The exception is the RI (ring) bit which is a
 * trailing edge bit (the RI trailing edge bit is set on only when RI
 * goes from on to off).  [oldmsr = old value, newmsr = new value]
 */
int msr_compute_delta_bits(int oldmsr, int newmsr)
{
  int delta;

  /* Compute difference bits, and restrict RI bit to trailing-edge */
  delta = (oldmsr ^ newmsr) & ~(newmsr & UART_MSR_RI);

  /* Shift bits to lowest 4 bits and mask other bits except delta bits */
  delta = (delta >> 4) & UART_MSR_DELTA;

  return delta;
}


/* This function returns the value in the MSR (Modem Status Register)
 * and does the expected UART operation whenever the MSR is read.
 * [num = port]
 */
static int
get_msr(int num)
{
  int val;

  modstat_engine(num);				/* Get the fresh MSR status */
  val = com[num].MSR;				/* Save old MSR value */
  com[num].MSR &= UART_MSR_STAT; 		/* Clear delta bits */
  clear_int_cond(num, MS_INTR);		/* MSI condition satisfied */

  return val;					/* Return MSR value */
}


/*************************************************************************/
/*                     LINE STATUS handling functions                    */
/*************************************************************************/

/* This function returns the value in the LSR (Line Status Register)
 * and does the expected UART operation whenever the LSR is read.
 * [num = port]
 */
static int
get_lsr(int num)
{
  int val;

  val = com[num].LSR;			/* Save old LSR value */
  clear_int_cond(num, LS_INTR);	/* RLSI int condition satisfied */
  com[num].LSR &= ~UART_LSR_ERR;	/* Clear error bits */

  return (val);                         /* Return LSR value */
}


/*************************************************************************/
/*                      TRANSMIT handling functions                      */
/*************************************************************************/

/* This function transmits a character.  This function is called mainly
 * through do_serial_out, when the Transmit Register is written to.
 * The end result is that the character is put into the THR or the
 * transmit FIFO. (or receive fifo if in Loopback test mode)
 * [num = port, val = character to transmit]
 */
static void put_tx(int num, char val)
{
  int rtrn;
#if 0
  /* Update the transmit timer */
  com[num].tx_timer += com[num].tx_char_time;
#endif
  clear_int_cond(num, TX_INTR);	/* TX interrupt condition satisifed */

  /* Loop-back writes.  Parity is currently not calculated.  No
   * UART diagnostics programs including COMTEST.EXE, that I tried,
   * complained about parity problems during loopback tests anyway!
   * Even some real UART clones don't calculate parity during loopback.
   */
  if (com[num].MCR & UART_MCR_LOOP) {
    com[num].rx_timeout = 0;		/* Reset timeout counter */
    switch (com[num].LCR & UART_LCR_WLEN8) {	/* Word size adjustment */
    case UART_LCR_WLEN7:  val &= 0x7f;  break;
    case UART_LCR_WLEN6:  val &= 0x3f;  break;
    case UART_LCR_WLEN5:  val &= 0x1f;  break;
    }
    if (FIFO_ENABLED(num)) {		/* Is it in FIFO mode? */
      /* Is the FIFO full? */
      if (RX_BUF_BYTES(num) >= com[num].rx_fifo_size) {
        if(s3_printf) s_printf("SER%d: Func put_tx loopback overrun requesting LS_INTR\n",num);
        com[num].LSR |= UART_LSR_OE;		/* Indicate overrun error */
        serial_int_engine(num, LS_INTR);	/* Update interrupt status */
      }
      else { /* FIFO not full */
        /* Put char into recv FIFO */
        com[num].rx_buf[com[num].rx_buf_end] = val;
        com[num].rx_buf_end++;

	/* If the buffer touches the top, slide chars to bottom of buffer */
        if ((com[num].rx_buf_end - 1) >= RX_BUFFER_SIZE) rx_buffer_slide(num);
        /* Is it the past the receive FIFO trigger level? */
        if (RX_BUF_BYTES(num) >= com[num].rx_fifo_trigger) {
          com[num].rx_timeout = 0;
          if(s3_printf) s_printf("SER%d: Func put_tx loopback requesting RX_INTR\n",num);
          serial_int_engine(num, RX_INTR);	/* Update interrupt status */
        }
      }
      com[num].LSR |= UART_LSR_DR;	/* Flag Data Ready bit */
    }
    else {				/* FIFOs not enabled */
      com[num].rx_buf[com[num].rx_buf_end] = val;  /* Overwrite old byte */
      com[num].rx_buf_end++;
      if (com[num].LSR & UART_LSR_DR) {		/* Was data waiting? */
        com[num].LSR |= UART_LSR_OE;		/* Indicate overrun error */
        if(s3_printf) s_printf("SER%d: Func put_tx loopback overrun requesting LS_INTR\n",num);
        serial_int_engine(num, LS_INTR);	/* Update interrupt status */
      }
      else {
        com[num].LSR |= UART_LSR_DR; 		/* Flag Data Ready bit */
        com[num].rx_timeout = 0;
        if(s3_printf) s_printf("SER%d: Func put_tx loopback requesting RX_INTR\n",num);
        serial_int_engine(num, RX_INTR);	/* Update interrupt status */
      }
    }
    return;
  }
  /* Else, not in loopback mode */

  if (!FIFO_ENABLED(num) && !(com[num].LSR & UART_LSR_THRE)) {
    s_printf("SER%d: ERROR: TX overrun\n", num);
    /* no indication bit for this??? */
    return;
  }

  rtrn = serial_write(num, &val, 1);
  if (rtrn != 1) {				/* Did transmit fail? */
    s_printf("SER%d: write failed! %s\n", num, strerror(errno)); 		/* Set overflow flag */
  } else {
    com[num].LSR &= ~(UART_LSR_THRE | UART_LSR_TEMT);		/* THR full */
    com[num].tx_cnt++;
  }

  transmit_engine(num);
}


/*************************************************************************/
/*            Miscallenous UART REGISTER handling functions              */
/*************************************************************************/

/* This function handles writes to the FCR (FIFO Control Register).
 * [num = port, val = new value to write to FCR]
 */
static void
put_fcr(int num, int val)
{
  val &= 0xcf;				/* bits 4,5 are reserved. */
  /* Bits 1-6 are only programmed when bit 0 is set (FIFO enabled) */
  if (val & UART_FCR_ENABLE_FIFO) {
    /* fifos are reset when we change from 16450 to 16550 mode.*/
    if (!(com[num].FCReg & UART_FCR_ENABLE_FIFO))
      uart_clear_fifo(num, UART_FCR_CLEAR_CMD);
    else if (val & UART_FCR_CLEAR_CMD)
      /* Commands to reset either of the two fifos.  The clear-FIFO bits
       * are disposed right after the FIFO's are cleared, and are not saved.
       */
      uart_clear_fifo(num, val & UART_FCR_CLEAR_CMD);

    /* Various flags to indicate that fifos are enabled. */
    com[num].FCReg = (val & ~UART_FCR_TRIGGER_14) | UART_FCR_ENABLE_FIFO;
    com[num].IIR.flg.fifo_enable = IIR_FIFO_ENABLE;

    /* Don't need to emulate RXRDY,TXRDY pins, used for bus-mastering. */
    if (val & UART_FCR_DMA_SELECT) {
      if(s1_printf) s_printf("SER%d: FCR: Attempt to change RXRDY & TXRDY pin modes\n",num);
    }

    /* Assign special variable for trigger value for easy access. */
    switch (val & UART_FCR_TRIGGER_14) {
    case UART_FCR_TRIGGER_1:   com[num].rx_fifo_trigger = 1;	break;
    case UART_FCR_TRIGGER_4:   com[num].rx_fifo_trigger = 4;	break;
    case UART_FCR_TRIGGER_8:   com[num].rx_fifo_trigger = 8;	break;
    case UART_FCR_TRIGGER_14:  com[num].rx_fifo_trigger = 14;	break;
    }
    if(s2_printf) s_printf("SER%d: FCR: Trigger Level is %d\n",num,com[num].rx_fifo_trigger);
  }
  else {				/* FIFO are disabled */
    /* If uart was set in 16550 mode, this will reset it back to 16450 mode.
     * If it already was in 16450 mode, there is no harm done.
     */
    com[num].IIR.flg.fifo_enable = 0;		/* Disabled FIFO flag */
    com[num].FCReg &= (~UART_FCR_ENABLE_FIFO);	/* Flag FIFO as disabled */
    uart_clear_fifo(num, UART_FCR_CLEAR_CMD);	/* Clear FIFO */
  }
}


/* This function handles writes to the LCR (Line Control Register).
 * [num = port, val = new value to write to LCR]
 */
static void
put_lcr(int num, int val)
{
  int changed = com[num].LCR ^ val;    /* bitmask of changed bits */

  com[num].LCR = val;                  /* Set new LCR value */

  if (val & UART_LCR_DLAB) {		/* Is Baudrate Divisor Latch set? */
    s_printf("SER%d: LCR = 0x%x, DLAB high.\n", num, val);
  }
  else {
    s_printf("SER%d: LCR = 0x%x, DLAB low.\n", num, val);
  }

  if (changed & UART_LCR_SBC)
    serial_brkctl(num, !!(val & UART_LCR_SBC));

  /* obviously the writes to LCR (except BREAK state) would
   * invalidate the rx fifo. We clear tx too. */
  if ((changed & ~UART_LCR_SBC) && !DLAB(num)) {
    uart_clear_fifo(num, UART_FCR_CLEAR_CMD);
    ser_termios(num);			/* Sets new line settings */
  }
}


/* This function handles writes to the MCR (Modem Control Register).
 * [num = port, val = new value to write to MCR]
 */
static void
put_mcr(int num, int val)
{
  int newmsr, delta;
  int changed;
  changed = com[num].MCR ^ val;			/* Bitmask of changed bits */
  com[num].MCR = val & UART_MCR_VALID;		/* Set valid bits for MCR */

  if (val & UART_MCR_LOOP) {		/* Is Loopback Mode set? */
    /* If loopback just enabled, clear FIFO and turn off DTR & RTS on line */
    if (changed & UART_MCR_LOOP) {		/* Was loopback just set? */
      if (FIFO_ENABLED(num))
        uart_clear_fifo(num,UART_FCR_CLEAR_CMD);	/* Clear FIFO's */
      serial_rts(num, 0);
      serial_dtr(num, 0);
    }

    /* During a UART Loopback test these bits are, Write(Out) => Read(In)
     * MCR Bit 1 RTS => MSR Bit 4 CTS,	MCR Bit 2 OUT1 => MSR Bit 6 RI
     * MCR Bit 0 DTR => MSR Bit 5 DSR,	MCR Bit 3 OUT2 => MSR Bit 7 DCD
     */
    newmsr  = convert_bit(val, UART_MCR_RTS, UART_MSR_CTS);
    newmsr |= convert_bit(val, UART_MCR_DTR, UART_MSR_DSR);
    newmsr |= convert_bit(val, UART_MCR_OUT1, UART_MSR_RI);
    newmsr |= convert_bit(val, UART_MCR_OUT2, UART_MSR_DCD);

    /* Compute delta bits of MSR */
    delta = msr_compute_delta_bits(com[num].MSR, newmsr);

    /* Update the MSR and its delta bits, not erasing old delta bits!! */
    com[num].MSR = (com[num].MSR & UART_MSR_DELTA) | delta | newmsr;

    /* Set the MSI interrupt flag if loopback changed the modem status */
    if(s3_printf) s_printf("SER%d: Func put_mcr loopback requesting MS_INTR\n",num);
    if (delta) serial_int_engine(num, MS_INTR);    /* Update interrupt status */

  }
  else {				/* It's not in Loopback Mode */
    /* Was loopback mode just turned off now?  Then reset FIFO */
    if (changed & UART_MCR_LOOP) {
      if (FIFO_ENABLED(num))
        uart_clear_fifo(num,UART_FCR_CLEAR_CMD);
    }

    /* Set interrupt enable flag according to OUT2 bit in MCR */
    if (INT_ENAB(num)) {
      if(s3_printf) s_printf("SER%d: Update interrupt status after MCR update\n",num);
    }

    /* Force RTS & DTR reinitialization if the loopback state has changed */
    if (UART_MCR_LOOP) changed |= UART_MCR_RTS | UART_MCR_DTR;

    /* Update DTR setting on serial device only if DTR state changed */
    if (changed & UART_MCR_DTR) {
      if(s1_printf) s_printf("SER%d: MCR: DTR -> %d\n",num,(val & UART_MCR_DTR));
      serial_dtr(num, !!(val & UART_MCR_DTR));
    }

    /* Update RTS setting on serial device only if RTS state changed */
    if ((changed & UART_MCR_RTS) && !com_cfg[num].system_rtscts) {
      if(s1_printf) s_printf("SER%d: MCR: RTS -> %d\n",num,(val & UART_MCR_RTS));
      serial_rts(num, !!(val & UART_MCR_RTS));
    }
  }
}


/* This function handles writes to the LSR (Line Status Register).
 * [num = port, val = new value to write to LSR]
 */
static void
put_lsr(int num, int val)
{
  int int_type = 0;

  com[num].LSR = val & 0x1F;			/* Bits 6,7,8 are ignored */

  if (val & UART_LSR_ERR)                       /* Any error bits set? */
    int_type |= LS_INTR;			/* Flag RLSI interrupt */
  else
    clear_int_cond(num, LS_INTR);         /* Unflag RLSI condition */

  if (val & UART_LSR_DR)                        /* Is data ready bit set? */
    int_type |= RX_INTR;			/* Flag RDI interrupt */
  else
    clear_int_cond(num, RX_INTR);         /* Unflag RDI condition */

  if (val & UART_LSR_THRE) {
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;       /* Set THRE state */
    int_type |= TX_INTR;			/* Flag TX interrupt */
  }
  else {
    com[num].LSR &= ~(UART_LSR_THRE | UART_LSR_TEMT);   /* Clear THRE state */
    clear_int_cond(num, TX_INTR);         /* Unflag THRI condition */
  }

  if (int_type) {
    /* Update interrupt status */
    if(s3_printf) s_printf("SER%d: Func put_lsr caused int_type = %d\n",num,int_type);
    serial_int_engine(num, int_type);
  }
  /* need to sync back DR */
  receive_engine(num, 0);
}


/* This function handles writes to the MSR (Modem Status Register).
 * [num = port, val = new value to write to MSR]
 */
static inline void
put_msr(int num, int val)
{
  /* Update MSR register */
  com[num].MSR = (com[num].MSR & UART_MSR_STAT) | (val & UART_MSR_DELTA);

  /* Update interrupt status */
  if (com[num].MSR & UART_MSR_DELTA) {
    if(s3_printf) s_printf("SER%d: Func put_msr requesting MS_INTR\n",num);
    serial_int_engine(num, MS_INTR);
  }
}


/*************************************************************************/
/*            PORT I/O to UART registers: INPUT AND OUTPUT               */
/*************************************************************************/

/* DANG_BEGIN_FUNCTION do_serial_in
 * The following function returns a value from an I/O port.  The port
 * is an I/O address such as 0x3F8 (the base port address of COM1).
 * There are 8 I/O addresses for each serial port which ranges from
 * the base port (ie 0x3F8) to the base port plus seven (ie 0x3FF).
 * [num = abritary port number for serial line, address = I/O port address]
 * DANG_END_FUNCTION
 */
int
do_serial_in(int num, ioport_t address)
{
  int val;

  /* delayed open happens here */
  if (!com[num].opened)
    com[num].opened = ser_open(num);
  if (com[num].opened <= 0)
    return 0;

  switch (address - com_cfg[num].base_port) {
  case UART_RX:		/* Read from Received Byte Register */
/*case UART_DLL:*/      /* or Read from Baudrate Divisor Latch LSB */
    if (DLAB(num)) {	/* Is DLAB set? */
      val = com[num].dll;	/* Then read Divisor Latch LSB */
      if(s1_printf) s_printf("SER%d: Read Divisor LSB = 0x%x\n",num,val);
    }
    else {
      val = get_rx(num);	/* Else, read Received Byte Register */
      if(s2_printf) s_printf("SER%d: Receive 0x%x\n",num,val);
    }
    break;

  case UART_IER:	/* Read from Interrupt Enable Register */
/*case UART_DLM:*/      /* or Read from Baudrate Divisor Latch MSB */
    if (DLAB(num)) {	/* Is DLAB set? */
      val = com[num].dlm;	/* Then read Divisor Latch MSB */
      if(s1_printf) s_printf("SER%d: Read Divisor MSB = 0x%x\n",num,val);
    }
    else {
      val = com[num].IER;	/* Else, read Interrupt Enable Register */
      if(s1_printf) s_printf("SER%d: Read IER = 0x%x\n",num,val);
    }
    break;

  case UART_IIR:	/* Read from Interrupt Identification Register */
    if (!com[num].IIR.mask)
      recalc_IIR(num);
    val = get_IIR_val(num);
    if (val & UART_IIR_THRI) {
      if(s2_printf) s_printf("SER%d: Read IIR = 0x%x (THRI now cleared)\n",num,val);
      clear_int_cond(num, TX_INTR);	/* Unflag TX int condition */
    }
    else {
      if(s2_printf) s_printf("SER%d: Read IIR = 0x%x\n",num,val);
    }
    break;

  case UART_LCR:	/* Read from Line Control Register */
    val = com[num].LCR;
    if(s2_printf) s_printf("SER%d: Read LCR = 0x%x\n",num,val);
    break;

  case UART_MCR:	/* Read from Modem Control Register */
    val = com[num].MCR;
    if(s2_printf) s_printf("SER%d: Read MCR = 0x%x\n",num,val);
    break;

  case UART_LSR:	/* Read from Line Status Register */
    val = get_lsr(num);
    if(s2_printf) s_printf("SER%d: Read LSR = 0x%x\n",num,val);
    break;

  case UART_MSR:	/* Read from Modem Status Register */
    val = get_msr(num);
    if(s2_printf) s_printf("SER%d: Read MSR = 0x%x\n",num,val);
    break;

  case UART_SCR:   	/* Read from Scratch Register */
    val = com[num].SCR;
    if(s2_printf) s_printf("SER%d: Read SCR = 0x%x\n",num,val);
    break;

  default:		/* The following code should never execute. */
    s_printf("ERROR: Port read 0x%x out of bounds for serial port %d\n",
    	address,num);
    val = 0;
    break;
  }

  return val;
}


/* DANG_BEGIN_FUNCTION do_serial_out
 * The following function writes a value to an I/O port.  The port
 * is an I/O address such as 0x3F8 (the base port address of COM1).
 * [num = abritary port number for serial line, address = I/O port address,
 * val = value to write to I/O port address]
 * DANG_END_FUNCTION
 */
int
do_serial_out(int num, ioport_t address, int val)
{

  /* delayed open happens here */
  if (!com[num].opened)
    com[num].opened = ser_open(num);
  if (com[num].opened <= 0)
    return 0;

  switch (address - com_cfg[num].base_port) {
  case UART_TX:		/* Write to Transmit Holding Register */
/*case UART_DLL:*/	/* or write to Baudrate Divisor Latch LSB */
    if (DLAB(num)) {	/* If DLAB set, */
      com[num].dll = val;	/* then write to Divisor Latch LSB */
      if(s2_printf) s_printf("SER%d: Divisor LSB = 0x%02x\n", num, val);
    }
    else {
      if (s2_printf) {
        if (com[num].MCR & UART_MCR_LOOP)
          s_printf("SER%d: Transmit 0x%x Loopback\n",num,val);
        else
          s_printf("SER%d: Transmit 0x%x\n",num,val);
      }
      put_tx(num, val);		/* else, Transmit character (write to THR) */
    }
    break;

  case UART_IER:	/* Write to Interrupt Enable Register */
/*case UART_DLM:*/	/* or write to Baudrate Divisor Latch MSB */
    if (DLAB(num)) {	/* If DLAB set, */
      com[num].dlm = val;	/* then write to Divisor Latch MSB */
      if(s2_printf) s_printf("SER%d: Divisor MSB = 0x%x\n", num, val);
    }
    else {			/* Else, write to Interrupt Enable Register */
      int tflg = 0;
      if ( !(com[num].IER & UART_IER_THRI) && (val & UART_IER_THRI) ) {
        /* Flag to allow THRI if enable THRE went from state 0 -> 1 */
        tflg = 1;
        com[num].int_condition |= TX_INTR;	// is this needed?
      }
      com[num].IER = (val & UART_IER_VALID);	/* Write to IER */
      if(s1_printf) s_printf("SER%d: Write IER = 0x%x\n", num, val);
      if (tflg)
        serial_int_engine(num, 0);
    }
    break;

  case UART_FCR:	/* Write to FIFO Control Register */
    if(s1_printf) s_printf("SER%d: FCR = 0x%x -> 0x%x\n", num,
        com[num].FCReg, val);
    put_fcr(num, val);
    break;

  case UART_LCR: 	/* Write to Line Control Register */
    /* The debug message is in the ser_termios routine via put_lcr */
    put_lcr(num, val);
    break;

  case UART_MCR:	/* Write to Modem Control Register */
    if(s1_printf) s_printf("SER%d: MCR = 0x%x -> 0x%x\n", num,
        com[num].MCR, val);
    put_mcr(num, val);
    break;

  case UART_LSR:	/* Write to Line Status Register */
    if(s1_printf) s_printf("SER%d: LSR = 0x%x -> 0x%x\n", num,
        com[num].LSR, val);
    put_lsr(num, val);		/* writeable only to lower 6 bits */
    break;

  case UART_MSR:	/* Write to Modem Status Register */
    if(s1_printf) s_printf("SER%d: MSR = 0x%x -> 0x%x\n", num,
        com[num].MSR, val);
    put_msr(num, val);		/* writeable only to lower 4 bits */
    break;

  case UART_SCR:	/* Write to Scratch Register */
    if(s1_printf) s_printf("SER%d: SCR = 0x%x -> 0x%x\n", num,
        com[num].SCR, val);
    com[num].SCR = val;
    break;

  default:		/* The following should never execute */
    s_printf("ERROR: Port write 0x%x out of bounds for serial port %d\n",
             address,num);
    break;
  }

  serial_int_engine(num, 0);		/* Update interrupt status */

  return 0;
}
