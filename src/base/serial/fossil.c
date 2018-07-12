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
#include <assert.h>

#include "emu.h"
#include "hlt.h"
#include "int.h"
#include "port.h"
#include "coopth.h"
#include "utilities.h"
#include "serial.h"
#include "ser_defs.h"

#include "doshelpers.h"

/* These macros are shortcuts to access various serial port registers
 *   read_char		Read character
 *   read_LCR		Read Line Control Register
 *   read_MCR		Read Modem Control Register
 *   write_char		Transmit character
 *   write_DLL		Write Baudrate Divisor Latch LSB value
 *   write_DLM		Write Baudrate Divisor Latch MSB value
 *   write_FCR		Write FIFO Control Register
 *   write_LCR		Write Line Control Register
 *   write_MCR		Write Modem Control Register
 */
#define read_reg(num, offset) do_serial_in((num), com_cfg[(num)].base_port + (offset))
#define read_char(num)        read_reg((num), UART_RX)
#define read_LCR(num)         read_reg((num), UART_LCR)
#define read_MCR(num)         read_reg((num), UART_MCR)
#define read_LSR(num)         read_reg((num), UART_LSR)
#define read_IIR(num)         read_reg((num), UART_IIR)
#define write_reg(num, offset, byte) do_serial_out((num), com_cfg[(num)].base_port + (offset), (byte))
#define write_char(num, byte) write_reg((num), UART_TX, (byte))
#define write_DLL(num, byte)  write_reg((num), UART_DLL, (byte))
#define write_DLM(num, byte)  write_reg((num), UART_DLM, (byte))
#define write_FCR(num, byte)  write_reg((num), UART_FCR, (byte))
#define write_LCR(num, byte)  write_reg((num), UART_LCR, (byte))
#define write_MCR(num, byte)  write_reg((num), UART_MCR, (byte))
#define write_IER(num, byte)  write_reg((num), UART_IER, (byte))

/* Get the LSR/MSR status bits in FOSSIL format. Since we don't care about
 * the delta/interrupt bits anyway, reading the com[] structure is
 * faster than calling do_serial_in.
 */
#define FOSSIL_GET_STATUS(num) \
  (((com[(num)].LSR & 0x63) << 8) | (com[(num)].MSR & 0x80) | 0x08)

/* Some FOSSIL constants. */
#define FOSSIL_MAGIC 0x1954
#define FOSSIL_REVISION 5
#define FOSSIL_MAX_FUNCTION 0x1f

#define FOSSIL_RX_BUFFER_SIZE RX_BUFFER_SIZE
#define FOSSIL_RX_BUF_BYTES(num) RX_BUF_BYTES(num)
#define FOSSIL_TX_BUFFER_SIZE 64
#define FOSSIL_TX_BUF_BYTES(num) min(TX_BUF_BYTES(num), FOSSIL_TX_BUFFER_SIZE)

/* These are the address of the FOSSIL id string, which is located
 * in FOSSIL.COM. These are set by serial_helper when FOSSIL.COM
 * is installed.
 */
static unsigned short fossil_id_offset, fossil_id_segment;

/* This flag indicates that the DOS part of the emulation, FOSSIL.COM,
 * is loaded. This module does nothing as long as this flag is false,
 * so other (DOS-based) FOSSIL drivers may be used.
 */
boolean fossil_tsr_installed = FALSE;

static u_short irq_hlt;
static void fossil_irq(Bit16u idx, void *arg);

static void fossil_init(void)
{
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
  hlt_hdlr.name       = "fossil isr";
  hlt_hdlr.func       = fossil_irq;
  irq_hlt = hlt_register_handler(hlt_hdlr);

  fossil_tsr_installed = TRUE;
  fossil_id_segment = SREG(es);
  fossil_id_offset = LWORD(edi);
}

static void fossil_irq(Bit16u idx, void *arg)
{
  int i;
  uint8_t iir, lsr;
  s_printf("FOSSIL: got irq\n");
  for (i = 0; i < config.num_ser; i++) {
    if (!com[i].opened)
      continue;
    iir = read_IIR(i);
    switch (iir & UART_IIR_CND_MASK) {
    case UART_IIR_NO_INT:
      break;
    case UART_IIR_RDI:
      write_IER(i, 0);
      lsr = read_LSR(i);
      if (lsr & UART_LSR_DR) {
        if (com[i].fossil_blkrd_tid != COOPTH_TID_INVALID) {
	  coopth_wake_up(com[i].fossil_blkrd_tid);
	  com[i].fossil_blkrd_tid = COOPTH_TID_INVALID;
	} else {
	  error("FOSSIL: tid not set!\n");
	}
      }
      break;
    default:
      s_printf("FOSSIL: unexpected interrupt cond %#x\n", iir);
      break;
    }
  }

  do_eoi_iret();
}

/**************************************************************************/
/*                         FOSSIL INTERRUPT 0x14                          */
/**************************************************************************/

/* This function handles the FOSSIL calls. It's called by int14.c
 * if the com[num].fossil_active flag is true, and always for
 * function 0x04 (initialize FOSSIL driver).
 */
void fossil_int14(int num)
{
  uint8_t req = HI(ax);

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

    LWORD(eax) = FOSSIL_GET_STATUS(num);
    s_printf("SER%d: FOSSIL 0x00: Return with AL=0x%02x AH=0x%02x\n",
      num, LO(ax), HI(ax));
    break;
  }

  /* Read character (should be with wait) */
  case 0x02:
    if (com[num].fossil_blkrd_tid != COOPTH_TID_INVALID) {
	/* the previous read was probably interrupted by ^C... */
	s_printf("SER%d: FOSSIL 0x02: Read with wait interrupted??\n", num);
	coopth_ensure_sleeping(com[num].fossil_blkrd_tid);
	/* to avoid resouce leakage, we just kill it the unsafe way */
	coopth_cancel(com[num].fossil_blkrd_tid);
	coopth_unsafe_detach(com[num].fossil_blkrd_tid);
	com[num].fossil_blkrd_tid = COOPTH_TID_INVALID;
	write_IER(num, 0);
    }

    while (!(com[num].LSR & UART_LSR_DR)) {	/* Was a character received? */
    #if SER_DEBUG_FOSSIL_RW
        s_printf("SER%d: FOSSIL 0x02: Read char with wait\n", num);
    #endif
	write_IER(num, UART_IER_RDI);
	set_IF();
	com[num].fossil_blkrd_tid = coopth_get_tid();
	coopth_sleep();
	assert(com[num].fossil_blkrd_tid == COOPTH_TID_INVALID);
	clear_IF();
    }
    LO(ax) = read_char(num);
    HI(ax) = 0;
    #if SER_DEBUG_FOSSIL_RW
        s_printf("SER%d: FOSSIL 0x02: Read char 0x%02x\n", num, LO(ax));
    #endif
    break;

  /* Initialize FOSSIL driver. */
  case 0x04:
  case 0x1c: {
    uint8_t imr, imr1;
    fossil_info_t *fi;

    assert(sizeof(*fi) == 19);

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
    /* init IRQs, set disabled */
    write_MCR(num, com[num].MCR | UART_MCR_OUT2);
    write_IER(num, 0);
    com[num].ivec.segment = ISEG(com[num].interrupt);
    com[num].ivec.offset = IOFF(com[num].interrupt);
    SETIVEC(com[num].interrupt, BIOS_HLT_BLK_SEG, irq_hlt);
    imr = imr1 = port_inb(0x21);
    imr &= ~(1 << com_cfg[num].irq);
    if (imr != imr1)
	port_outb(0x21, imr);
    com[num].fossil_blkrd_tid = COOPTH_TID_INVALID;

    /* Initialize FOSSIL driver info. This is used by the function 0x1b
     * (Get driver info).
     */

    fi = &com[num].fossil_info;
    fi->size = sizeof(*fi);
    fi->frev = FOSSIL_REVISION;
    fi->irev = 0;         // Driver revision (not used)
    fi->id_offset = fossil_id_offset;
    fi->id_segment = fossil_id_segment;
    fi->rx_bufsize = FOSSIL_RX_BUFFER_SIZE;
    fi->tx_bufsize = FOSSIL_TX_BUFFER_SIZE;
    fi->scrn_width = 80;
    fi->scrn_height = 25;
    fi->bps = 0;          // Bps rate (not used)

    s_printf("SER%d: FOSSIL 0x%02x: Emulation activated\n", num, req);
    break;
  }

  /* FOSSIL shutdown. */
  case 0x05:
  case 0x1d: {
    uint8_t imr;
    if (!com[num].fossil_active)
      break;
    imr = port_inb(0x21);
    imr |= (1 << com_cfg[num].irq);
    port_outb(0x21, imr);
    SETIVEC(com[num].interrupt, com[num].ivec.segment, com[num].ivec.offset);
    com[num].fossil_active = FALSE;
    /* Note: the FIFO values aren't restored. Hopefully nobody notices... */
    s_printf("SER%d: FOSSIL 0x%02x: Emulation deactivated\n", num, req);
    break;
  }

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
    /* hope fifo doesn't overflow */
    if (FIFO_ENABLED(num) || (com[num].LSR & UART_LSR_THRE)) {
      write_char(num, LO(ax));
      LWORD(eax) = 1;
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x0b: Write char 0x%02x, return AX=%d\n", num, LO(ax), 1);
    #endif
    } else {
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x0b: Write char 0x%02x, return AX=0\n", num, LO(ax));
    #endif
      LWORD(eax) = 0;
    }
    break;

  /* Block read */
  case 0x18:
  {
    unsigned char *p = SEG_ADR((unsigned char *), es, di);
    int n = 0, len = LWORD(ecx);
    while (n < len) {
#if 0
      /* do we need wait here? */
      if (!(com[num].LSR & UART_LSR_DR)) {
        set_IF();
        coopth_wait();
        clear_IF();
      }
#endif
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
    int n, len = LWORD(ecx);
    for (n = 0; n < len; n++) {
      if (!FIFO_ENABLED(num) && !(com[num].LSR & UART_LSR_THRE))
        break;
      write_char(num, p[n]);
    }
    LWORD(eax) = n;
    #if SER_DEBUG_FOSSIL_RW
      s_printf("SER%d: FOSSIL 0x19: Block write, %d/%d bytes\n", num, n, len);
    #endif
    break;
  }

  /* Get FOSSIL driver info. */
  case 0x1b: {
    unsigned char *p = SEG_ADR((unsigned char *), es, di);
    int bufsize;
    fossil_info_t fossil_info, *fi;

    if (!fossil_tsr_installed) {
      LWORD(eax) = 0;       // Perhaps the only way to indicate no valid data
      return;
    }

    bufsize = (LWORD(ecx) < sizeof(*fi)) ? LWORD(ecx) : sizeof(*fi);

    if (LO(dx) == 0xff) {
      fi = &fossil_info;

      fi->size = sizeof(*fi);
      fi->frev = FOSSIL_REVISION;
      fi->irev = 0;         // Driver revision (not used)
      fi->id_offset = fossil_id_offset;
      fi->id_segment = fossil_id_segment;
      fi->rx_bufsize = FOSSIL_RX_BUFFER_SIZE;
      fi->rx_remaining = 0; // Not valid in non port specific context
      fi->tx_bufsize = FOSSIL_TX_BUFFER_SIZE;
      fi->tx_remaining = 0; // Not valid in non port specific context
      fi->scrn_width = 80;
      fi->scrn_height = 25;
      fi->bps = 0;          // Bps rate (not used)

#if SER_DEBUG_FOSSIL_STATUS
      s_printf("SER: FOSSIL 0x1b: Driver info, ibuf=%d, obuf=%d, "
               "idaddr=%04x:%04x, AX=%d\n",
               FOSSIL_RX_BUFFER_SIZE, FOSSIL_TX_BUFFER_SIZE, fossil_id_segment,
               fossil_id_offset, bufsize);
#endif
    } else {
      fi = &com[num].fossil_info;

      /* Fill in some values that aren't constant. */
      fi->rx_remaining = FOSSIL_RX_BUFFER_SIZE - FOSSIL_RX_BUF_BYTES(num);
      fi->tx_remaining = FOSSIL_TX_BUFFER_SIZE - FOSSIL_TX_BUF_BYTES(num);

#if SER_DEBUG_FOSSIL_STATUS
      s_printf("SER%d: FOSSIL 0x1b: Driver info, i=%d/%d, o=%d/%d, AX=%d\n",
               num, fi->rx_remaining, FOSSIL_RX_BUFFER_SIZE, fi->tx_remaining,
               FOSSIL_TX_BUFFER_SIZE, bufsize);
#endif
    }

    /* Copy data to user area. */
    memcpy(p, fi, bufsize);
    LWORD(eax) = bufsize;
    break;
  }

  /* Function 1Eh - Extended line control initialization.
   *
   * This function is intended to exactly emulate the PS/2's BIOS INT
   * 14 services, function 4.
   */

  case 0x1e: {
    int lcr;
    uint8_t mbits;
    int divisors[] = {DIV_110,  DIV_150,  DIV_300,  DIV_600,  DIV_1200,
                      DIV_2400, DIV_4800, DIV_9600, DIV_19200};
    uint8_t pbits[] = {0b00000000, 0b00001000, 0b00011000};

    s_printf("SER%d: FOSSIL 0x1e: Extended initialize port %d\n", num, LO(dx));

    mbits = 0;

    // break AL
    if (LO(ax) == 1)
      s_printf("SER%d: FOSSIL 0x1e: Unhandled Ctrl break set request\n", num);

    // parity BH
    if (HI(bx) < 3) // None, Odd, Even parity
      mbits |= pbits[HI(bx)];
    else
      s_printf("SER%d: FOSSIL 0x1e: Unhandled Mark or Space parity request\n",
               num);

    // stop BL
    if (LO(bx) == 1)
      mbits |= (1 << 2);

    // data CH
    if (HI(cx) < 4)
      mbits |= HI(cx);

    /* Read the LCR register and set character size, parity and stopbits */
    lcr = read_LCR(num);
    lcr = (lcr & ~UART_LCR_PARA) | (mbits & UART_LCR_PARA);

    /* Raise DTR and RTS */
    write_MCR(num, com[num].MCR | UART_MCR_DTR | UART_MCR_RTS);

    // speed CL
    if (LO(cx) < 9) {
      /* Set DLAB bit, in order to set the baudrate */
      write_LCR(num, lcr | 0x80);

      /* Write the Baudrate Divisor Latch values */
      write_DLL(num, divisors[LO(cx)] & 0xFF); /* LSB */
      write_DLM(num, divisors[LO(cx)] >> 8);   /* MSB */

      /* Lower DLAB bit */
      write_LCR(num, lcr & ~0x80);
    } else {
      s_printf("SER%d: FOSSIL 0x1e: Out of range speed request %d\n",
               num, LO(cx));
    }

    LWORD(eax) = FOSSIL_GET_STATUS(num);
    s_printf("SER%d: FOSSIL 0x1e: Return with AL=0x%02x AH=0x%02x\n", num,
             LO(ax), HI(ax));
    break;
  }

  /* Function 1Fh - Extended comm port control
   *
   * This function is intended to exactly emulate the PS/2's BIOS INT
   * 14 services, function 5.
   */

  case 0x1f: {
    uint8_t mcr;

    if (LO(ax) == 1) {
      mcr = LO(bx) & UART_MCR_VALID; // Mask off the reserved bits
      mcr |= (1 << 3);               // X00 will not allow communications
                                     // interrupts to be disabled, so do likewise.
      write_MCR(num, mcr);
      s_printf("SER%d: FOSSIL 0x1f: Write MCR (0x%02x)\n", num, mcr);
    } else {
      LO(bx) = read_MCR(num);
      s_printf("SER%d: FOSSIL 0x1f: Read MCR (0x%02x)\n", num, LO(bx));
    }

    LWORD(eax) = FOSSIL_GET_STATUS(num);
    s_printf("SER%d: FOSSIL 0x1f: Return with AL=0x%02x AH=0x%02x\n", num,
             LO(ax), HI(ax));
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
  switch (HI(ax)) {
    /* TSR installation check. */
    case DOS_SUBHELPER_SERIAL_TSR_CHECK:
      LWORD(eax) = fossil_tsr_installed;
      s_printf("SER: FOSSIL helper 0: TSR installation check, AX=%d\n",
               fossil_tsr_installed);
      break;

    /* TSR install. */
    case DOS_SUBHELPER_SERIAL_TSR_INSTALL:
      if (fossil_tsr_installed) {
        LWORD(ebx) = DOS_ERROR_SERIAL_ALREADY_INSTALLED;
        CARRY;
        break;
      }

      if (config.num_ser == 0) {
        LWORD(ebx) = DOS_ERROR_SERIAL_CONFIG_DISABLED;
        CARRY;
        break;
      }

      fossil_init();
      LWORD(ebx) = FOSSIL_MAX_FUNCTION;
      NOCARRY;
      s_printf("SER: FOSSIL helper 1: TSR install, ES:DI=%04x:%04x\n",
               SREG(es), LWORD(edi));
      break;

    default:
      s_printf("SER: FOSSIL helper 0x%02x: Unknown function!\n", HI(ax));
  }
}
