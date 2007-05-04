/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* DANG_BEGIN_MODULE
 * 
 * REMARK
 * fossil.c: FOSSIL serial driver emulator for dosemu.
 * 
 * Copyright (C) 1995 by Pasi Eronen.
 * Portions Copyright (C) 1995 by Mark Rejhon
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * /REMARK
 * DANG_END_MODULE
 */

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include "config.h" 
#include "emu.h"
#include "serial.h"
#include "ser_defs.h"

/* These macros are shortcuts to access various serial port registers
 *   read_char		Read character
 *   read_LCR		Read Line Control Register
 *   write_char		Transmit character
 *   write_DLL		Write Baudrate Divisor Latch LSB value
 *   write_DLM		Write Baudrate Divisor Latch MSB value
 *   write_FCR		Write FIFO Control Register
 *   write_LCR		Write Line Control Register
 *   write_MCR		Write Modem Control Register
 */
#define read_reg(num, offset) do_serial_in((num), com[(num)].base_port + (offset))
#define read_char(num)        read_reg((num), UART_RX)
#define read_LCR(num)         read_reg((num), UART_LCR)
#define write_reg(num, offset, byte) do_serial_out((num), com[(num)].base_port + (offset), (byte))
#define write_char(num, byte) write_reg((num), UART_TX, (byte))
#define write_DLL(num, byte)  write_reg((num), UART_DLL, (byte))
#define write_DLM(num, byte)  write_reg((num), UART_DLM, (byte))
#define write_FCR(num, byte)  write_reg((num), UART_FCR, (byte))
#define write_LCR(num, byte)  write_reg((num), UART_LCR, (byte))
#define write_MCR(num, byte)  write_reg((num), UART_MCR, (byte))

/* Get the LSR/MSR status bits in FOSSIL format. Since we don't care about 
 * the delta/interrupt bits anyway, reading the com[] structure is 
 * faster than calling do_serial_in. 
 */
#define FOSSIL_GET_STATUS(num) \
  (((com[(num)].LSR & 0x63) << 8) | (com[(num)].MSR & 0x80) | 0x08)

/* Some FOSSIL constants. */
#define FOSSIL_MAGIC 0x1954
#define FOSSIL_REVISION 5
#define FOSSIL_MAX_FUNCTION 0x1b

static void fossil_check_info(void);

/* These are the address of the FOSSIL id string, which is located
 * in FOSSIL.COM. These are set by serial_helper when FOSSIL.COM
 * is installed.
 */
static unsigned short fossil_id_offset, fossil_id_segment;

/* This flag indicates that the DOS part of the emulation, FOSSIL.COM,
 * is loaded. This module does nothing as long as this flag is false,
 * so other (DOS-based) FOSSIL drivers may be used. 
 */
static boolean fossil_tsr_installed = FALSE;


/**************************************************************************/
/*                         FOSSIL INTERRUPT 0x14                          */
/**************************************************************************/

/* This function handles the FOSSIL calls. It's called by int14.c
 * if the com[num].fossil_active flag is true, and always for 
 * function 0x04 (initialize FOSSIL driver).
 */
void fossil_int14(int num)
{
  switch (HI(ax)) {
  /* Initialize serial port. */
  case 0x00:
  {
    int lcr;
    int divisors[] = { DIV_19200, DIV_38400, DIV_300, DIV_600, DIV_1200, 
      DIV_2400, DIV_4800, DIV_9600 };
      
    s_printf("SER%d: FOSSIL 0x00: Initialize port %d, AL=0x%02x\n",
      num, LO(dx), LO(ax));

    /* Read the LCR register and set character size, parity and stopbits */
    lcr = read_LCR(num);
    lcr = (lcr & ~UART_LCR_PARA) | (LO(ax) & UART_LCR_PARA);

    /* Raise DTR and RTS */
    write_MCR(num, com[num].MCR | UART_MCR_DTR | UART_MCR_RTS);
    
    /* Set DLAB bit, set Baudrate Divisor Latch values, and clear DLAB. */
    write_LCR(num, lcr | UART_LCR_DLAB);
    write_DLL(num, divisors[LO(ax) >> 5] & 0xff);
    write_DLM(num, divisors[LO(ax) >> 5] >> 8);
    write_LCR(num, lcr & ~UART_LCR_DLAB);

    uart_fill(num);			/* Fill UART with received data */
    LWORD(eax) = FOSSIL_GET_STATUS(num);
    s_printf("SER%d: FOSSIL 0x00: Return with AL=0x%02x AH=0x%02x\n",
      num, LO(ax), HI(ax));
    break;
  }
    
  /* Write character (should be with wait) */
  case 0x01:
    /* DANG_FIXTHIS This really should be write-with-wait. */
    write_char(num, LO(ax));
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x01: Write char 0x%02x\n", num, LO(ax));
    #endif
    LWORD(eax) = FOSSIL_GET_STATUS(num);
    break;
    
  /* Read character (should be with wait) */
  case 0x02:
    uart_fill(num);			/* Fill UART with received data */
    if (com[num].LSR & UART_LSR_DR) {	/* Was a character received? */
      LO(ax) = read_char(num);
      HI(ax) = 0;
      #if SER_DEBUG_FOSSIL_RW
        s_printf("SER%d: FOSSIL 0x02: Read char 0x%02x\n", num, LO(ax));
      #endif
    }
    else {
      /* This is supposed to be read-with-wait, but I since most programs don't
       * use this function without checking for character available, I think
       * it doesn't matter. 
       */
      LO(ax) = 0;
      HI(ax) = 0x80;
      s_printf("SER%d: FOSSIL 0x02: Read with wait failed!\n", num);
    }
    break;
    
  /* Get port status. */
  case 0x03:
    uart_fill(num);			/* Fill UART with received data */
    LWORD(eax) = FOSSIL_GET_STATUS(num);
    #if SER_DEBUG_FOSSIL_STATUS   
      s_printf("SER%d: FOSSIL 0x03: Port Status, AH=0x%02x AL=0x%02x\n",
        num, HI(ax), LO(ax));
    #endif	       
    break;
    
  /* Initialize FOSSIL driver. */
  case 0x04:
    /* Do nothing if TSR isn't installed. */
    if (!fossil_tsr_installed)	     
      return;
    com[num].fossil_active = TRUE;
    LWORD(eax) = FOSSIL_MAGIC;
    HI(bx) = FOSSIL_REVISION;
    LO(bx) = FOSSIL_MAX_FUNCTION;
    /* Raise DTR */
    write_MCR(num, com[num].MCR | UART_MCR_DTR);
    /* Enable FIFOs. I'm not sure if the changing of FIFO size really
     * affects anything, but it seems to work :-). 
     */
    write_FCR(num, UART_FCR_ENABLE_FIFO|UART_FCR_TRIGGER_14);
    uart_clear_fifo(num, UART_FCR_CLEAR_CMD);
    com[num].rx_fifo_size = RX_BUFFER_SIZE/2;
    /* Initialize FOSSIL driver info buffer. This is used by the 
     * function 0x1b (Get driver info). 
     */
    com[num].fossil_info[0] = 19;       /* Structure size */
    com[num].fossil_info[1] = 0;
    com[num].fossil_info[2] = FOSSIL_REVISION;
    com[num].fossil_info[3] = 0;        /* Driver revision (not used) */
    com[num].fossil_info[4] = fossil_id_offset & 0xff;
    com[num].fossil_info[5] = fossil_id_offset >> 8;
    com[num].fossil_info[6] = fossil_id_segment & 0xff;
    com[num].fossil_info[7] = fossil_id_segment >> 8;
    com[num].fossil_info[8] = RX_BUFFER_SIZE & 0xff;
    com[num].fossil_info[9] = RX_BUFFER_SIZE >> 8;
    com[num].fossil_info[12] = TX_BUFFER_SIZE & 0xff;
    com[num].fossil_info[13] = TX_BUFFER_SIZE >> 8;
    com[num].fossil_info[16] = 80;	/* Screen width */
    com[num].fossil_info[17] = 25;	/* Screen height */
    com[num].fossil_info[18] = 0;       /* Bps rate (not used) */
    s_printf("SER%d: FOSSIL 0x04: Emulation activated\n",num);
    break;
    
  /* FOSSIL shutdown. */  
  case 0x05:
    com[num].fossil_active = FALSE;
    /* Note: the FIFO values aren't restored. Hopefully nobody notices... */
    s_printf("SER%d: FOSSIL 0x05: Emulation deactivated\n", num);
    break;
    
  /* Lower/raise DTR. */  
  case 0x06:
    write_MCR(num, (com[num].MCR & ~UART_MCR_DTR) | (LO(ax) ? UART_MCR_DTR : 0));
    s_printf("SER%d: FOSSIL 0x06: DTR set to %d\n", num, LO(ax));
    break;
  
  /* Purge output buffer */
  case 0x09:
    uart_clear_fifo(num, UART_FCR_CLEAR_XMIT);
    s_printf("SER%d: FOSSIL 0x09: Purge output buffer\n", num);
    break;

  /* Purge input buffer */
  case 0x0a:
    uart_clear_fifo(num, UART_FCR_CLEAR_RCVR);
    s_printf("SER%d: FOSSIL 0x0a: Purge input buffer\n", num);
    break;

  /* Write character (without wait) */
  case 0x0b:
    if (!com[num].tx_overflow){
      write_char(num, LO(ax));
      LWORD(eax)= 1;
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x0b: Write char 0x%02x, return AX=%d\n", num, LO(ax), 1);
    #endif
    } else {
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x0b: Write char 0x%02x, return AX=%d\n", num, LO(ax), !com[num].tx_overflow);
    #endif
    LWORD(eax) = !com[num].tx_overflow;
    }
    break;

  /* Block read */
  case 0x18:
  {
    unsigned char *p = SEG_ADR((unsigned char *), es, di);
    int n = 0, len = LWORD(ecx);
    while (n < len) {
      /* Don't call uart_fill if it isn't necessary. */
      if (!(com[num].LSR & UART_LSR_DR))
        uart_fill(num);			/* Fill UART with received data */
      if (com[num].LSR & UART_LSR_DR) 	/* Was a character received? */
        p[n++] = read_char(num);       	/* Read character */
      else
        break;
    }
    LWORD(eax) = n;
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x18: Block read, %d/%d bytes\n", num, n, len);
    #endif
    break;
  }

  /* Block write */
  case 0x19:
  {
    unsigned char *p = SEG_ADR((unsigned char *), es, di);
    int n = 0, len = LWORD(ecx);
    while (n < len) {
      if (com[num].tx_overflow)
        break;
      write_char(num, p[n++]);
    }
    LWORD(eax) = n;
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x19: Block write, %d/%d bytes\n", num, n, len);
    #endif  
    break;
  }
  
  /* Get FOSSIL driver info. */
  case 0x1b:
  {
    unsigned char *p = SEG_ADR((unsigned char *), es, di);
    int ifree = RX_BUFFER_SIZE-RX_BUF_BYTES(num),
      ofree = TX_BUFFER_SIZE-TX_BUF_BYTES(num),
      bufsize = (LWORD(ecx) <= 19) ? LWORD(ecx) : 19;
    if (LO(dx) == 0xff)
    {
      fossil_check_info();
      return;
    }
    /* Fill in some values that aren't constant. */
    com[num].fossil_info[10] = ifree & 0xff;
    com[num].fossil_info[11] = ifree >> 8;
    com[num].fossil_info[14] = ofree & 0xff;
    com[num].fossil_info[15] = ofree >> 8;
    /* Copy data to user area. */
    memcpy(p, com[num].fossil_info, bufsize);
    LWORD(eax)=bufsize;
    #if SER_DEBUG_FOSSIL_STATUS
      s_printf("SER%d: FOSSIL 0x1b: Driver info, i=%d/%d, o=%d/%d, AX=%d\n", num, ifree, RX_BUFFER_SIZE, ofree, TX_BUFFER_SIZE, bufsize);
    #endif    
    break;
  }

  /* Unimplemented functions. Some of these could be implemented quite easily,
   * but most programs (at least the ones I use) don't use use them. 
   */

  /* Get timer tick information. */
  case 0x07:
  /* Flush output buffer. */
  case 0x08:
  /* Non-destructive read (peek). */
  case 0x0c:
  /* Keyboard read (without wait). */
  case 0x0d:
  /* Keyboard read (with wait). */
  case 0x0e:
  /* Set flow control. */
  case 0x0f:
  /* Enable/disable Ctrl-C/Ctrl-K checking and transmitter. */
  case 0x10:
  /* Set cursor location. */
  case 0x11:
  /* Get cursor location. */
  case 0x12:
  /* Write character to screen (with ANSI support). */
  case 0x13:
  /* Enable/disable DCD watchdog. */
  case 0x14:
  /* Write character to screen (using BIOS). */
  case 0x15:
  /* Add/delete function from timer tick chain. */ 
  case 0x16:
  /* Reboot system. */
  case 0x17:
  /* Break begin/end. */
  case 0x1a:
  /* Install external application function. */
  case 0x7e:
  /* Remove external application function. */
  case 0x7f:
    s_printf("SER%d: FOSSIL 0x%02x: Function not implemented!\n", num, HI(ax));
    break;
    
  /* This runs if nothing handles the FOSSIL function. */
  default:
    s_printf("SER%d: FOSSIL 0x%02x: Unknown function!\n", num, HI(ax));
  }
}


/**************************************************************************/
/*                         SERIAL HELPER FUNCTION                         */
/**************************************************************************/

/* The DOS part of FOSSIL emulator, FOSSIL.COM, uses this call to activate
 * the dosemu part of the emulation. 
 */
void serial_helper(void)
{
  switch(HI(ax))
  {
  /* TSR installation check. */
  case 0:
    LWORD(eax) = fossil_tsr_installed;
    s_printf("SER: FOSSIL helper 0: TSR installation check, AX=%d\n", fossil_tsr_installed);
    break;
  
  /* TSR install. */
  case 1:
    fossil_tsr_installed = TRUE;
    fossil_id_segment = LWORD(es);
    fossil_id_offset = LWORD(edi);
    s_printf("SER: FOSSIL helper 1: TSR install, ES:DI=%04x:%04x\n", fossil_id_segment, fossil_id_offset);
    break;
  
  default:
    s_printf("SER: FOSSIL helper 0x%02x: Unknown function!\n", HI(ax));
  }
}

static void fossil_check_info()
{
if (fossil_tsr_installed)
  {
    unsigned char *p = SEG_ADR((unsigned char *), es, di);
    u_char def_fossil_info[19]; /* FOSSIL driver info buffer */
    int bufsize = (LWORD(ecx) <= 19) ? LWORD(ecx) : 19;
    def_fossil_info[0] = 19;       /* Structure size */
    def_fossil_info[1] = 0;
    def_fossil_info[2] = FOSSIL_REVISION;
    def_fossil_info[3] = 0;        /* Driver revision (not used) */
    def_fossil_info[4] = fossil_id_offset & 0xff;
    def_fossil_info[5] = fossil_id_offset >> 8;
    def_fossil_info[6] = fossil_id_segment & 0xff;
    def_fossil_info[7] = fossil_id_segment >> 8;
    def_fossil_info[8] = RX_BUFFER_SIZE & 0xff;
    def_fossil_info[9] = RX_BUFFER_SIZE >> 8;
    def_fossil_info[12] = TX_BUFFER_SIZE & 0xff;
    def_fossil_info[13] = TX_BUFFER_SIZE >> 8;
    def_fossil_info[16] = 80;   /* Screen width */
    def_fossil_info[17] = 25;   /* Screen height */
    def_fossil_info[18] = 0;       /* Bps rate (not used) */
    /* Copy data to user area. */
    memcpy(p, def_fossil_info, bufsize);
    LWORD(eax)=bufsize;
    #if SER_DEBUG_FOSSIL_STATUS
      s_printf("SER%d: FOSSIL 0x1b: Driver info, i=%d/%d, o=%d/%d, AX=%d\n", 
      		num, ifree, RX_BUFFER_SIZE, ofree, TX_BUFFER_SIZE, bufsize);
    #endif
    return;
  }
}

