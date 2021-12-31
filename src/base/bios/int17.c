/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* This is in implementation of int 17, the printer service interrupt
   It is based on Bochs' implementation, and purely communicates
   with the BIOS data area and the printer ports */

#include "emu.h"
#include "bios.h"
#include "port.h"
#include "lpt.h"
#include "timers.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

int int17(void)
{
  int timeout, val8;
  ioport_t addr;

  CARRY;
  if (_DX >= NUM_LPTS)
    return 1;
  addr = READ_WORD(BIOS_ADDRESS_LPT1 + _DX * 2);
  if (addr == 0)
    return 1;
  timeout = READ_BYTE(BIOS_LPT1_TIMEOUT + _DX) << 8;

  reset_idle(2);

  switch(_AH)
  {
  case 0:
    port_outb(addr, _AL);
    val8 = port_inb(addr + 2);
    port_outb(addr + 2, val8 | 0x01); // send strobe
    port_outb(addr + 2, val8 & ~0x01);
    while (((val8 = port_inb(addr + 1)) & LPT_STAT_NOT_ACK) && timeout)
      timeout--;
    break;
  case 1:
    val8 = port_inb(addr + 2);
    port_outb(addr + 2, val8 & ~0x04); // send init
    port_outb(addr + 2, val8 | 0x04);
    val8 = port_inb(addr + 1);
    break;
  case 2:
    val8 = port_inb(addr + 1);
    break;
  default:
    return 1;
  }
  /* Note: RBIL seems to be wrong on Busy bit: Table P0658 states it
   * as "busy" and Table 00631 as "not busy". But, according to SeaBIOS
   * sources, there is no int17 inversion on Busy bit (the inversion
   * mask is 0x48 and Busy is 0x80), so Table P0658 should have stated
   * it as "not busy" too. The inversion happens instead in the LPT hardware:
   * http://digteh.ru/PC/LPT/
   */
  _AH = val8 ^ (LPT_STAT_NOT_ACK | LPT_STAT_NOIOERR);
  /* mask out "unused" bits. Needs to get rid of the no-IRQ flag, which
   * is obviously always 1. SeaBIOS doesn't do this, but PRINTFIX.COM does.
   * Anderson Vulczak <andi@andi.com.br> says:
   * ---
   * I had to modify bios file 17 in order to clipper detect the printer
   * as READY, it was not detecting it as function "isprinter" of clipper
   * reads directly from bios (i found on xharbour sources) if the bios 17
   * function will return a 0x90 value.
   * ---
   * So lets not force people to use PRINTFIX.COM.
   */
  _AH &= ~0x06;
  if (!timeout) _AH |= LPT_STAT_TIMEOUT;
  NOCARRY;

  return 1;
}

void
printer_mem_setup(void)
{
  int i;
  for (i = 0; i < NUM_LPTS; i++) {
    /* set the port address for each printer in bios */
    WRITE_WORD(BIOS_ADDRESS_LPT1 + i * 2, get_lpt_base(i));
    WRITE_BYTE(BIOS_LPT1_TIMEOUT + i, 20);
  }
}
