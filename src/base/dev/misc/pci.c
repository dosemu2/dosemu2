/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * SIDOC_BEGIN_MODULE
 *
 * Description: PCI configuration space access
 * currently used only by the Matrox video driver
 * 
 * Maintainers: Alberto Vignani (vignani@tin.it)
 *
 * SIDOC_END_MODULE
 */

#include <errno.h>
#include <string.h>
#include "emu.h"
#include "port.h"
#include "pci.h"


/* SIDOC_BEGIN_FUNCTION pci_read_header
 *
 * Use standard 32-bit (type 1) access method to read PCI
 * configuration space data
 *
 * SIDOC_END_FUNCTION
 */
int (*pci_read_header) (unsigned char bus, unsigned char device,
			unsigned char func, unsigned long *buf) = pci_read_header_cfg1;

/*
 * So far only config type 1 is supported. Return 0 if
 * no PCI is present or PCI config type != 1.
 */
int pci_check_conf(void)
{
    unsigned long save, val;
    
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }
    save = port_real_ind(PCI_CONF_ADDR);
    port_real_outd(PCI_CONF_ADDR,PCI_EN);
    val = port_real_ind(PCI_CONF_ADDR);
    port_real_outd(PCI_CONF_ADDR, save);
    priv_iopl(0);
    if (val == PCI_EN)
	return 1;
    else
	return 0;
}


int pci_read_header_cfg1 (unsigned char bus, unsigned char device,
	unsigned char fn, unsigned long *buf)
{
  int i;
  unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
                      PCI_EN;
  

  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return 0;
  }
  for (i=0; i<64; i++) {
	port_real_outd (PCI_CONF_ADDR, bx|(i<<2));
	buf[i] = port_real_ind (PCI_CONF_DATA);
  }
  priv_iopl(0);
  return 0;
}

int pci_check_device_present_cfg1(unsigned char bus, unsigned char device,
			     unsigned char fn)
{
    unsigned long val;
    unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
	                 PCI_EN;
    
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }
    port_real_outd (PCI_CONF_ADDR, bx);
    val = port_real_ind (PCI_CONF_DATA);
    priv_iopl(0);

    if (val == 0xFFFFFFFF)
	return 0 ;
    else
	return 1;
}

int pci_read_header_cfg2 (unsigned char bus, unsigned char device,
			  unsigned char fn, unsigned long *buf)
{
  int i;
  
  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return 0;
  }
  port_real_outb(PCI_MODE2_ENABLE_REG,0xF1);
  port_real_outb(PCI_MODE2_FORWARD_REG,bus);
  for (i=0; i<64; i++) {
	buf[i] = port_real_ind ((device << 8) + i);
  }
  port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
  priv_iopl(0);
  return 0;
}

int pci_check_device_present_cfg2(unsigned char bus, unsigned char device)
{
    unsigned long val;
    
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }
    port_real_outb(PCI_MODE2_ENABLE_REG,0xF1);
    port_real_outb(PCI_MODE2_FORWARD_REG,bus);
    val = port_real_ind ((device << 8));
    port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
    priv_iopl(0);

    if (val == 0xFFFFFFFF || val == 0xf0f0f0f0)
	return 0 ;
    else
	return 1;
}

static unsigned int wcf8_pend = 0;

static void chk_pend(void)
{
    if (wcf8_pend) {
	std_port_outd(0xcf8,wcf8_pend);
	wcf8_pend=0;	/* clear at least bit 31 */
    }
}

static Bit8u pci_port_inb(ioport_t port)
{
	chk_pend();
	return std_port_inb(port);
}

static void pci_port_outb(ioport_t port, Bit8u byte)
{
	chk_pend();
	std_port_outb(port,byte);
}

static Bit16u pci_port_inw(ioport_t port)
{
	chk_pend();
	return std_port_inw(port);
}

static void pci_port_outw(ioport_t port, Bit16u value)
{
	chk_pend();
	std_port_outw(port,value);
}

static Bit32u pci_port_ind(ioport_t port)
{
	chk_pend();
	return std_port_ind(port);
}

/* SIDOC_BEGIN_FUNCTION pci_read_header
 *
 * 32-bit I/O port output on PCI ports (0xcf8=addr,0xcfc=data)
 * Optimization: trap the config writes (outd 0xcf8 with bit31=1).
 * Since this kind of access is always followed by another R/W access
 * to port 0xcfc, we can just set it as pending and merge it with the
 * following operation, saving two calls to priv_iopl().
 *
 * SIDOC_END_FUNCTION
 */
static void pci_port_outd(ioport_t port, Bit32u value)
{
	if ((port==0xcf8)&&(value&PCI_EN)&&(!wcf8_pend)) {
		wcf8_pend=value;
		if (debug_level('i')>3) i_printf("PCICFG: %08x pending\n", value);
	}
	else {
		chk_pend();
		std_port_outd(port,value);
	}
}


/* SIDOC_BEGIN_FUNCTION pci_read_header
 *
 * Register standard PCI ports 0xcf8-0xcff
 *
 * SIDOC_END_FUNCTION
 */
int pci_setup (void)
{
  emu_iodev_t io_device;

  if (config.pci) {
    /* register PCI ports */
    io_device.read_portb = pci_port_inb;
    io_device.write_portb = pci_port_outb;
    io_device.read_portw = pci_port_inw;
    io_device.write_portw = pci_port_outw;
    io_device.read_portd = pci_port_ind;
    io_device.write_portd = pci_port_outd;
    io_device.irq = EMU_NO_IRQ;
    io_device.fd = -1;
  
    io_device.handler_name = "PCI Config";
    io_device.start_addr = PCI_CONF_ADDR;
    io_device.end_addr = PCI_CONF_DATA+3;
    port_register_handler(io_device, 0);
  }
  return 0;
}
