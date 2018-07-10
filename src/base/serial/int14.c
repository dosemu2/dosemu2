/* DANG_BEGIN_MODULE
 *
 * REMARK
 * int14.c: Serial BIOS services for DOSEMU.
 * Please read the README.serial file in this directory for more info!
 *
 * Copyright (C) 1995 by Mark Rejhon
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This module is maintained by Mark Rejhon at these Email addresses:
 *      marky@magmacom.com
 *      ag115@freenet.carleton.ca
 *
 * /REMARK
 * DANG_END_MODULE
 */

/* DANG_BEGIN_NEWIDEA
 * If any of you coders are ambitious, try thinking of the following:
 * - Converting this into inline assembler and use direct port access
 * DANG_END_NEWIDEA
 */

/* Standard includes, just to be on the safe side. */
#include <stdio.h>
#include <termios.h>

/* Other includes that may be needed for this serial code */
#include "emu.h"
#include "serial.h"
#include "ser_defs.h"
#include "timers.h"
#include "coopth.h"

/* These macros are shortcuts to access various serial port registers:
 *   read_char		Read character
 *   read_LCR		Read Line Control Register
 *   read_LSR		Read Line Status Register
 *   read_MSR		Read Modem Status Register
 *   write_char		Transmit character
 *   write_DLL		Write Baudrate Divisor Latch LSB value
 *   write_DLM		Write Baudrate Divisor Latch MSB value
 *   write_LCR		Write Line Control Register
 *   write_MCR		Write Modem Control Register
 */
#define read_char(num)        do_serial_in(num, com_cfg[num].base_port)
#define read_LCR(num)         do_serial_in(num, com_cfg[num].base_port + 3)
#define read_MCR(num)         do_serial_in(num, com_cfg[num].base_port + 4)
#define read_LSR(num)         do_serial_in(num, com_cfg[num].base_port + 5)
#define read_MSR(num)         do_serial_in(num, com_cfg[num].base_port + 6)
#define write_char(num, byte) do_serial_out(num, com_cfg[num].base_port, byte)
#define write_DLL(num, byte)  do_serial_out(num, com_cfg[num].base_port, byte)
#define write_DLM(num, byte)  do_serial_out(num, com_cfg[num].base_port + 1, byte)
#define write_LCR(num, byte)  do_serial_out(num, com_cfg[num].base_port + 3, byte)
#define write_MCR(num, byte)  do_serial_out(num, com_cfg[num].base_port + 4, byte)


/* The following function sets the speed of the serial port.
 * num is the index, and speed is a baudrate divisor value.
 */
static void set_speed(int num, int speed)
{
  write_DLL(num, speed & 0xFF);		/* Write baudrate divisor LSB */
  write_DLM(num, (speed >> 8) & 0xFF);	/* Write baudrate divisor MSB */
}


/**************************************************************************/
/*                         BIOS INTERRUPT 0x14                            */
/**************************************************************************/

/* DANG_BEGIN_FUNCTION int14
 * The following function executes a BIOS interrupt 0x14 function.
 * This code by Mark Rejhon replaced some very buggy, old int14 interface
 * a while back.  These routines are not flawless since it does not wait
 * for a character during receive, and this may confuse some programs.
 * DANG_END_FUNCTION
 */
int int14(void)
{
  int num;
  int temp;

  /* Translate the requested COM port number in the DL register, into
   * the necessary arbritary port number system used throughout this module.
   */
  for(num = 0; num < config.num_ser; num++)
    if (com_cfg[num].real_comport == (LO(dx)+1)) break;

  if (num >= config.num_ser) return 1;	/* Exit if not on supported port */

  switch (HI(ax)) {
  case 0:		/* Initialize serial port. */
    if (com[num].fossil_active) {
      fossil_int14(num);
      break;
    }

    s_printf("SER%d: INT14 0x0: Initialize port %d, AL=0x%x\n",
	num, LO(dx), LO(ax));

    /* Read the LCR register */
    temp = read_LCR(num);

    /* The following sets character size, parity, and stopbits */
    temp = (temp & ~UART_LCR_PARA) | (LO(ax) & UART_LCR_PARA);

    /* Set DLAB bit, in order to set the baudrate */
    write_LCR(num, temp | 0x80);

    /* Write the Baudrate Divisor Latch values */
    switch (LO(ax) & 0xE0) {
    case 0x00:   set_speed(num, DIV_110);   break;
    case 0x20:   set_speed(num, DIV_150);   break;
    case 0x40:   set_speed(num, DIV_300);   break;
    case 0x60:   set_speed(num, DIV_600);   break;
    case 0x80:   set_speed(num, DIV_1200);  break;
    case 0xa0:   set_speed(num, DIV_2400);  break;
    case 0xc0:   set_speed(num, DIV_4800);  break;
    case 0xe0:   set_speed(num, DIV_9600);  break;
    }

    /* Lower DLAB bit */
    write_LCR(num, temp & ~0x80);

    /* Note that SeaBIOS does not touch MCR... why do we? */
    temp = read_MCR(num) & UART_MCR_VALID;
    temp &= ~UART_MCR_LOOP;
    /* Raise DTR and RTS on the Modem Control Register */
    temp |= UART_MCR_DTR | UART_MCR_RTS;
    write_MCR(num, temp);

    HI(ax) = read_LSR(num);		/* Read Line Status (LSR) into AH */
    LO(ax) = read_MSR(num);		/* Read Modem Status (MSR) into AL */

    s_printf("SER%d: INT14 0x0: Return with AL=0x%x AH=0x%x\n",
	num, LO(ax), HI(ax));
    break;

  /* Write character with wait */
  case 1: {
    const int timeout = 10;
    const int scale = 0x10000;
    int i = 1;
    hitimer_t end_time = GETtickTIME(0) + timeout * scale;
    s_printf("SER%d: INT14 0x1: Write char 0x%x\n",num,LO(ax));
#if 1
    while (GETtickTIME(0) < end_time) {
      if (read_LSR(num) & UART_LSR_THRE)
        break;
      s_printf("SER%d: INT14 0x1: Wait for xmit, %i\n", num, i);
      i++;
      set_IF();
      coopth_wait();
      clear_IF();
    }
#endif
    if (FIFO_ENABLED(num) || (read_LSR(num) & UART_LSR_THRE)) {
      write_char(num, LO(ax));		/* Transmit character */
      HI(ax) = read_LSR(num) & ~0x80;	/* Character was sent */
    } else {
      s_printf("SER%d: INT14 TX overrun\n",num);
      HI(ax) = read_LSR(num) | 0x80;	/* return error */
    }
    break;
  }

  /* Read character function */
  case 2: {
    const int timeout = 10;
    const int scale = 0x10000;
    int i = 1;
    hitimer_t end_time = GETtickTIME(0) + timeout * scale;
    if (com[num].fossil_active) {
      fossil_int14(num);
      break;
    }

    s_printf("SER%d: INT14 0x2: Read char\n", num);
#if 1
    while (GETtickTIME(0) < end_time) {
      if (read_LSR(num) & UART_LSR_DR)
        break;
      s_printf("SER%d: INT14 0x2: Wait for recv, %i\n", num, i);
      i++;
      set_IF();
      coopth_wait();
      clear_IF();
    }
#endif
    if ((read_LSR(num) & UART_LSR_DR) && (read_MSR(num) & UART_MSR_DSR)) {
      LO(ax) = read_char(num);		/* Read character */
      HI(ax) = read_LSR(num) & ~0x80;	/* Character was received */
      s_printf("SER%d: INT14 0x2: Read char 0x%x\n",num,LO(ax));
    }
    else {
      HI(ax) = read_LSR(num) | 0x80;	/* Timeout */
      s_printf("SER%d: INT14 0x2: Read char timeout.\n",num);
    }
    break;
  }

  /* Port Status request function. */
  case 3:
    HI(ax) = read_LSR(num);		/* Read Line Status (LSR) into AH */
    LO(ax) = read_MSR(num);		/* Read Modem Status (MSR) into AL */
    s_printf("SER%d: INT14 0x3: Port Status, AH=0x%x AL=0x%x\n",
               num,HI(ax),LO(ax));
    break;

  /* FOSSIL initialize. */
  case 4:
    fossil_int14(num);
    break;

  /* This runs if nothing handles the int14 function */
  default:
    if (com[num].fossil_active) {
      fossil_int14(num);
      break;
    }

    s_printf("SER%d: INT14 0x%x: Unsupported interrupt on port %d\n",
              num,HI(ax),LO(dx));
    break;
  }
  return 1;
}

void serial_mem_setup(void)
{
  int num;
  /* Write serial port information into BIOS data area 0040:0000
   * This is for DOS and many programs to recognize ports automatically
   */
  for (num = 0; num < config.num_ser; num++) {
    if ((com_cfg[num].real_comport >= 1) && (com_cfg[num].real_comport <= 4)) {
      WRITE_WORD(0x400 + (com_cfg[num].real_comport-1)*2, com_cfg[num].base_port);

      /* Debugging to determine whether memory location was written properly */
      s_printf("SER%d: BIOS memory location %p has value of %#x\n", num,
	       ((u_short *) (0x400) + (com_cfg[num].real_comport-1))
	       ,READ_WORD(0x400 + 2*(com_cfg[num].real_comport-1)));
    }
  }
}
