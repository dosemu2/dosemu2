/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * SIDOC_BEGIN_MODULE
 *
 * Description: CMOS handling routines
 * 
 * Originally by Robert Sanders, gt8134b@prism.gatech.edu
 * New CMOS code by vignani@mail.tin.it 1997-98
 *
 * SIDOC_END_MODULE
 *
 */

#include <time.h>
#include <sys/time.h>

#include "config.h"
#include "emu.h"
#include "port.h"
#include "iodev.h"
#include "disks.h"


#define PEXTMEM_SIZE (EXTMEM_SIZE + HMASIZE)


static int
cmos_chksum(void)
{
  int i, sum = 0;

  /* return the checksum over bytes 0x10-0x20. These are static values,
   * so no need to call cmos_read()
   */

  for (i = 0x10; i < 0x21; i++)
    sum += GET_CMOS(i);
  return sum;
}

Bit8u cmos_read(ioport_t port)
{
  unsigned char holder = 0;

  if (port != 0x71)
    return 0xff;

  switch (cmos.address) {
    case 0 ... 0x0d:
      holder = rtc_read(cmos.address);
      break;

    case CMOS_CHKSUML:
      holder = cmos_chksum() & 0xff;
      break;

    case CMOS_CHKSUMM:
      holder = cmos_chksum() >> 8;
      break;

    default:
      holder = GET_CMOS(cmos.address);
      if (!cmos.flag[cmos.address])
        h_printf("CMOS: unknown CMOS read 0x%x\n", cmos.address);
  }

  h_printf("CMOS: read addr 0x%02x = 0x%02x\n", cmos.address, holder);
  return holder;
}

void cmos_write(ioport_t port, Bit8u byte)
{
  if (port == 0x70)
    cmos.address = byte & ~0xc0;/* get true address */
  else {
    h_printf("CMOS: set address 0x%02x to 0x%02x\n", cmos.address, byte);
    switch (cmos.address) {
      case 0 ... 0x0d:
        rtc_write(cmos.address, byte);
	break;

      default:
        SET_CMOS(cmos.address, byte);
    }
  }
}

void cmos_init(void)
{
  emu_iodev_t  io_device;
  int i;

  /* CMOS RAM & RTC */
  io_device.read_portb   = cmos_read;
  io_device.write_portb  = cmos_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "CMOS RAM";
  io_device.start_addr   = 0x0070;
  io_device.end_addr     = 0x0071;
  io_device.irq          = EMU_NO_IRQ;
  io_device.fd           = -1;
  port_register_handler(io_device, 0);

  for (i = 0; i < 64; i++)
    cmos.subst[i] = cmos.flag[i] = 0;

  rtc_init();

  /* CMOS floppies...is this correct? */
  SET_CMOS(CMOS_DISKTYPE, 
	(config.fdisks ? (disktab[0].default_cmos << 4) : 0) |
	((config.fdisks > 1) ? disktab[1].default_cmos & 0xf : 0));

  /* CMOS equipment byte..top 2 bits are 01 for 2 drives, 00 for 1
   * bit 1 is 1 for math coprocessor installed
   * bit 0 is 1 for floppies installed, 0 for none */

  cmos.subst[0x14] = ((config.fdisks ? config.fdisks - 1 : 0) << 5) +
        (config.fdisks ? 1 : 0);
  if (config.mathco)
    cmos.subst[0x14] |= 2;
  cmos.flag[0x14] = 1;

  /* CMOS hard disks...type 47 for both. */
  SET_CMOS(CMOS_HDTYPE, (config.hdisks ? 0xf0 : 0) +
        ((config.hdisks - 1) ? 0xf : 0));
  SET_CMOS(CMOS_HD1EXT, 47);
  if (config.hdisks == 2)
    SET_CMOS(CMOS_HD2EXT, 47);
  else
    SET_CMOS(CMOS_HD2EXT, 0);

  /* this is the CMOS status */
  SET_CMOS(CMOS_STATUSA, 0x26);
  /* default id BCD,24h,no DST */
  SET_CMOS(CMOS_STATUSB, 2);

  /* 0xc and 0xd are read only */
  SET_CMOS(CMOS_STATUSC, 0);
  SET_CMOS(CMOS_STATUSD, 0x80);

  SET_CMOS(CMOS_DIAG, 0);

  /* memory counts */
  SET_CMOS(CMOS_BASEMEML, config.mem_size & 0xff);      /* base mem LSB */
  SET_CMOS(CMOS_BASEMEMM, config.mem_size >> 8);        /* base mem MSB */

  SET_CMOS(CMOS_EXTMEML, (EXTMEM_SIZE + HMASIZE) & 0xff);
  SET_CMOS(CMOS_EXTMEMM, (EXTMEM_SIZE + HMASIZE) >> 8);

  SET_CMOS(CMOS_PEXTMEML, PEXTMEM_SIZE & 0xff);
  SET_CMOS(CMOS_PEXTMEMM, PEXTMEM_SIZE >> 8);

  /* say protected mode test 7 passed (?) */
  SET_CMOS(CMOS_SHUTDOWN, 6);

  /* information flags...my CMOS returns this */
  SET_CMOS(CMOS_INFO, 0xe1);

  g_printf("CMOS initialized\n");
}

void cmos_reset(void)
{
}
