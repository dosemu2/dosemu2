/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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


#define EXTMEM_SIZE ((config.xms_size>config.ems_size)?config.xms_size : \
		     config.ems_size)
#define PEXTMEM_SIZE (((config.xms_size>config.ems_size)?config.xms_size : \
		      config.ems_size)/1024)


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

Bit8u
cmos_read(ioport_t port)
{
  unsigned char holder = 0;

  LOCK_CMOS;
  switch (cmos.address) {
  case CMOS_SEC:
  case CMOS_MIN:
  case CMOS_HOUR:
  case CMOS_DOW:		/* day of week */
  case CMOS_DOM:		/* day of month */
  case CMOS_MONTH:
  case CMOS_YEAR:
  case CMOS_CENTURY:
    holder = cmos_date(cmos.address); goto quit;

  case CMOS_CHKSUML:
    holder = cmos_chksum() & 0xff; goto quit;

  case CMOS_CHKSUMM:
    holder = cmos_chksum() >> 8; goto quit;
  }

  /* date functions return, so hereafter all values should be those set
   * either at boot time or changed by DOS programs...
   */

  if (cmos.flag[cmos.address]) {/* this reg has been written to */
    holder = GET_CMOS(cmos.address);
    h_printf("CMOS: read cmos_subst = 0x%02x\n", holder);
  }
#ifdef DANGEROUS_CMOS
  else if (!set_ioperm(0x70, 2, 1)) {
    h_printf("CMOS: really reading 0x%x!\n", cmos.address);
    port_real_outb(0x70, (cmos.address & ~0xc0));
    holder = port_real_inb(0x71);
    set_ioperm(0x70, 2, 0);
  }
#endif
  else {
    h_printf("CMOS: unknown CMOS read 0x%x\n", cmos.address);
    holder = GET_CMOS(cmos.address);
  }

#if defined(NEW_PIC)
  if (cmos.address==CMOS_STATUSB)		/* safety code */
    if ((holder&0x70)==0) pic_untrigger(PIC_IRQ8);
#endif

quit:
  UNLOCK_CMOS;
  h_printf("CMOS: read addr 0x%02x = 0x%02x\n", cmos.address, holder);
  return holder;
}

void
cmos_write(ioport_t port, Bit8u byte)
{
  LOCK_CMOS;
  if (port == 0x70)
    cmos.address = byte & ~0xc0;/* get true address */
  else {
    if ((cmos.address != 0xc) && (cmos.address != 0xd)) {
      h_printf("CMOS: set address 0x%02x to 0x%02x\n", cmos.address, byte);
      switch (cmos.address) {
	case CMOS_SEC:
	case CMOS_MIN:
	case CMOS_HOUR:
	case CMOS_SECALRM:
	case CMOS_MINALRM:
	case CMOS_HOURALRM:
	  byte = BIN(byte);
	  break;
	/* b7=r/o and unused
	 * b4-6=always 010 (AT standard 32.768kHz)
	 * b0-3=rate [65536/2^v], default 6, min 3, 0=disable
	 */
	case CMOS_STATUSA:
	  if ((byte&0x70)!=0x20) dbug_printf("RTC: error clkin set\n");
	  byte &= 0x0f;
	  if ((byte>0)&&(byte<3)) byte=3;
	  byte |= 0x20;
	  break;
	/* b7=set update cycle, 1=disable
	 * b6=enable periodical int
	 * b5=enable alarm int
	 * b4=enable update int
	 * b3=square wave out
	 * ========= source: Motorola data sheets for MC146818
	 * b2=data mode, 0=BCD(default) 1=bin
	 * b1=time mode, 0=12h 1=24h(default)
	 * b0=DST,       0=disabled(default), 1=enabled
	 *    NOTE: DST happens on last Sunday in April/October (obsolete)
	 */
	case CMOS_STATUSB:
	  byte = byte&0xf7; /* why someone should want to enable DST??? */
	  break;
      }
      SET_CMOS(cmos.address, byte);
    }
    else
      h_printf("CMOS: write to ref 0x%x blocked\n", cmos.address);
  }
  UNLOCK_CMOS;
}

void cmos_init(void)
{
  emu_iodev_t  io_device;

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
}

void cmos_reset(void)
{
  int i;

  LOCK_CMOS;
  for (i = 0; i < 64; i++)
    cmos.subst[i] = cmos.flag[i] = 0;

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

  SET_CMOS(CMOS_EXTMEML, EXTMEM_SIZE & 0xff);
  SET_CMOS(CMOS_EXTMEMM, EXTMEM_SIZE >> 8);

  SET_CMOS(CMOS_PEXTMEML, PEXTMEM_SIZE & 0xff);
  SET_CMOS(CMOS_PEXTMEMM, PEXTMEM_SIZE >> 8);

  /* say protected mode test 7 passed (?) */
  SET_CMOS(CMOS_SHUTDOWN, 6);

  /* information flags...my CMOS returns this */
  SET_CMOS(CMOS_INFO, 0xe1);

  rtc_init();
  UNLOCK_CMOS;

  g_printf("CMOS initialized\n");
}
