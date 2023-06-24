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


static Bit8u port92h_io_read(ioport_t port, void *arg)
{
  Bit8u ret = 0;
  if (a20)
    ret |= CONTROL_A20GATE_MASK;
  return ret;
}

static void port92h_io_write(ioport_t port, Bit8u val, void *arg)
{
  int enA20 = (val & CONTROL_A20GATE_MASK) ? 1 : 0;
  if (val & CONTROL_RESET_MASK) cpu_reset();
  set_a20(enA20);
}

static Bit8u picext_io_read(ioport_t port, void *arg)
{
  Bit8u val = 0xff;

  switch (port) {
  case PIC0_VECBASE_PORT:
    val = pic0_get_base();
    break;
  case PIC1_VECBASE_PORT:
    val = pic1_get_base();
    break;
  }
  return val;
}

void chipset_init(void)
{
  emu_iodev_t io_dev = {};

  io_dev.read_portb = port92h_io_read;
  io_dev.write_portb = port92h_io_write;
  io_dev.start_addr = 0x92;
  io_dev.end_addr = 0x92;
  io_dev.handler_name = "Chipset Control Port A";
  port_register_handler(io_dev, 0);

  memset(&io_dev, 0, sizeof(io_dev));
  io_dev.read_portb = picext_io_read;
  io_dev.start_addr = PIC0_EXTPORT_START;
  io_dev.end_addr = PIC0_EXTPORT_START + PICx_EXT_PORTS - 1;
  io_dev.handler_name = "PIC0 extensions";
  port_register_handler(io_dev, 0);

  memset(&io_dev, 0, sizeof(io_dev));
  io_dev.read_portb = picext_io_read;
  io_dev.start_addr = PIC1_EXTPORT_START;
  io_dev.end_addr = PIC1_EXTPORT_START + PICx_EXT_PORTS - 1;
  io_dev.handler_name = "PIC1 extensions";
  port_register_handler(io_dev, 0);
}
