/* DANG_BEGIN_MODULE
 * 
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
 *      mdrejhon@magi.com
 *      ag115@freenet.carleton.ca
 *
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
#include "config.h" 
#include "emu.h"
#include "serial.h"
#include "ser_defs.h"

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
#define read_char(num)        do_serial_in(num, com[num].base_port)
#define read_LCR(num)         do_serial_in(num, com[num].base_port + 3)
#define read_LSR(num)         do_serial_in(num, com[num].base_port + 5)
#define read_MSR(num)         do_serial_in(num, com[num].base_port + 6)
#define write_char(num, byte) do_serial_out(num, com[num].base_port, byte)
#define write_DLL(num, byte)  do_serial_out(num, com[num].base_port, byte)
#define write_DLM(num, byte)  do_serial_out(num, com[num].base_port + 1, byte)
#define write_LCR(num, byte)  do_serial_out(num, com[num].base_port + 3, byte)
#define write_MCR(num, byte)  do_serial_out(num, com[num].base_port + 4, byte)


/* The following function sets the speed of the serial port.
 * num is the index, and speed is a baudrate divisor value.
 */
void set_speed(int num, int speed)
{
  write_DLL(num, speed & 0xFF);		/* Write baudrate divisor LSB */
  write_DLM(num, (speed >> 8) & 0xFF);	/* Write baudrate divisor MSB */
}


/**************************************************************************/
/*                         BIOS INTERRUPT 0x14                            */
/**************************************************************************/

/* DANG_BEGIN_FUNCTION int14
 * The following function exeuctes a BIOS interrupt 0x14 function.
 * This code by Mark Rejhon replaced some very buggy, old int14 interface
 * a while back.  Tese routines are not flawless since it does not wait
 * for a character during receive, and this may confuse some programs.
 * DANG_END_FUNCTION
 */
void int14(u_char ii)
{
  static int num;
  static int temp;
  
  /* Translate the requested COM port number in the DL register, into
   * the necessary arbritary port number system used throughout this module.
   */
  for(num = 0; num < config.num_ser; num++)
    if (com[num].real_comport == (LO(dx)+1)) break;

  if (num >= config.num_ser) return;	/* Exit if not on supported port */

  /* If FOSSIL is active, call it! */
  if (com[num].fossil_active)
  {
    fossil_int14(num);
    return;
  }

  switch (HI(ax)) {
  case 0:		/* Initialize serial port. */
    s_printf("SER%d: INT14 0x0: Initialize port %d, AL=0x%x\n", 
	num, LO(dx), LO(ax));

    /* Read the LCR register */
    temp = read_LCR(num);
    
    /* The following sets character size, parity, and stopbits */
    temp = (temp & ~UART_LCR_PARA) | (LO(ax) & UART_LCR_PARA);

    /* Raise DTR and RTS on the Modem Control Register */
    write_MCR(num, 0x3);
    
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

    uart_fill(num);			/* Fill UART with received data */
    HI(ax) = read_LSR(num);		/* Read Line Status (LSR) into AH */
    LO(ax) = read_MSR(num);		/* Read Modem Status (MSR) into AL */

    s_printf("SER%d: INT14 0x0: Return with AL=0x%x AH=0x%x\n", 
	num, LO(ax), HI(ax));
    break;
    
  /* Write character function */
  case 1:
    s_printf("SER%d: INT14 0x1: Write char 0x%x\n",num,LO(ax));
    write_char(num, LO(ax));		/* Transmit character */
    HI(ax) = read_LSR(num);		/* Read Line Status (LSR) into AH */
    LO(ax) = read_MSR(num);		/* Read Modem Status (MSR) into AL */
    break;
    
  /* Read character function */
  case 2:
    uart_fill(num);			/* Fill UART with received data */
    if (com[num].LSR & UART_LSR_DR) {	/* Was a character received? */
      LO(ax) = read_char(num);		/* Read character */
      HI(ax) = read_LSR(num) & ~0x80;	/* Character was received */
      s_printf("SER%d: INT14 0x2: Read char 0x%x\n",num,LO(ax));
    }
    else {
      HI(ax) = read_LSR(num) | 0x80;	/* Timeout */
      s_printf("SER%d: INT14 0x2: Read char timeout.\n",num);
    }
    break;
    
  /* Port Status request function. */
  case 3:
    uart_fill(num);			/* Fill UART with received data */
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
    s_printf("SER%d: INT14 0x%x: Unsupported interrupt on port %d\n",
              num,HI(ax),LO(dx));
    return;
  }
}
