/*
 * (C) Copyright 2013 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "emu.h"
#include "port.h"
#include "hma.h"
#include "chipset.h"

#define CONTROL_RESET_MASK   1
#define CONTROL_A20GATE_MASK 2


static Bit8u port92h_io_read(ioport_t port)
{
  Bit8u ret = 0;
  if (a20)
    ret |= CONTROL_A20GATE_MASK;
  return ret;
}

static void port92h_io_write(ioport_t port, Bit8u val)
{
  int enA20 = (val & CONTROL_A20GATE_MASK) ? 1 : 0;
  if (val & CONTROL_RESET_MASK) cpu_reset();
  set_a20(enA20);
}

void chipset_init(void)
{
  emu_iodev_t io_dev;

  io_dev.read_portb = port92h_io_read;
  io_dev.write_portb = port92h_io_write;
  io_dev.read_portw = NULL;
  io_dev.write_portw = NULL;
  io_dev.read_portd = NULL;
  io_dev.write_portd = NULL;
  io_dev.start_addr = 0x92;
  io_dev.end_addr = 0x92;
  io_dev.handler_name = "Chipset Control Port A";
  port_register_handler(io_dev, 0);
}
