/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * SIDOC_BEGIN_MODULE
 *
 * Description: PCI configuration space access
 * (only configuration mechanism 1 is fully implemented so far)
 * used by the console video drivers and the PCI BIOS
 * 
 * SIDOC_END_MODULE
 */

#include "config.h"
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
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

/*
 * So far only config type 1 is supported. Return 0 if
 * no PCI is present or PCI config type != 1.
 */
struct pci_funcs *pci_check_conf(void)
{
    unsigned long val;

    if (!config.pci && access("/proc/bus/pci", R_OK) == 0)
      return &pci_proc;

    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }
    port_real_outd(PCI_CONF_ADDR,PCI_EN);
    val = port_real_ind(PCI_CONF_ADDR);
    port_real_outd(PCI_CONF_ADDR, 0);
    priv_iopl(0);
    if (val == PCI_EN)
	return &pci_cfg1;
    else
	return NULL;
}

/* only called from pci bios init */
static int pci_read_header_cfg1 (unsigned char bus, unsigned char device,
				 unsigned char fn, unsigned long *buf)
{
  int i;
  unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
                      PCI_EN;


  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return 0;
  }
  /* Get only first 64 bytes: See src/linux/drivers/pci/proc.c for
     why. They are not joking. My NCR810 crashes the machine on read
     of register 0xd8 */
  for (i=0; i<16; i++) {
	port_real_outd (PCI_CONF_ADDR, bx|(i<<2));
	buf[i] = port_real_ind (PCI_CONF_DATA);
  }
  port_real_outd (PCI_CONF_ADDR, 0);
  priv_iopl(0);
  return 0;
}

static unsigned long pci_read_cfg1 (unsigned char bus, unsigned char device,
				    unsigned char fn, unsigned long reg)
{
  unsigned long val;
  unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
                      PCI_EN;

  if (can_do_root_stuff) {
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }
    port_real_outd (PCI_CONF_ADDR, bx | (reg & ~3));
    val = port_real_ind (PCI_CONF_DATA);
    port_real_outd (PCI_CONF_ADDR, 0);
    priv_iopl(0);
  } else {
    std_port_outd (PCI_CONF_ADDR, bx | (reg & ~3));
    val = std_port_ind (PCI_CONF_DATA);
    std_port_outd (PCI_CONF_ADDR, 0);
  }
  return val;
}

static void pci_write_cfg1 (unsigned char bus, unsigned char device,
			    unsigned char fn, unsigned long reg, unsigned long val)
{
  unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
                      PCI_EN;

  if (can_do_root_stuff) {
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return;
    }
    port_real_outd (PCI_CONF_ADDR, bx | (reg & ~3));
    port_real_outd (PCI_CONF_DATA, val);
    port_real_outd (PCI_CONF_ADDR, 0);
    priv_iopl(0);
  } else {
    std_port_outd (PCI_CONF_ADDR, bx | (reg & ~3));
    std_port_outd (PCI_CONF_DATA, val);
    std_port_outd (PCI_CONF_ADDR, 0);
  }
}

/* only called from pci bios init */
static int pci_check_device_present_cfg1(unsigned char bus, unsigned char device,
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
    port_real_outd (PCI_CONF_ADDR, 0);
    priv_iopl(0);

    if (val == 0xFFFFFFFF)
	return 0 ;
    else
	return 1;
}

/* only called from pci bios init */
static int pci_read_header_cfg2 (unsigned char bus, unsigned char device,
				 unsigned char fn, unsigned long *buf)
{
  int i;
  
  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return 0;
  }
  port_real_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
  port_real_outb(PCI_MODE2_FORWARD_REG, bus);
  for (i=0; i<16; i++) {
	buf[i] = port_real_ind (0xc000 | (device << 8) | (i << 2));
  }
  port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
  priv_iopl(0);
  return 0;
}

static unsigned long pci_read_cfg2 (unsigned char bus, unsigned char device,
				    unsigned char fn, unsigned long num)
{
  unsigned long val;
  
  if (can_do_root_stuff) {
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }
    port_real_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
    port_real_outb(PCI_MODE2_FORWARD_REG, bus);
    val = port_real_ind (0xc000 | (device << 8) | num);
    port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
    priv_iopl(0);
  } else {
    std_port_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
    std_port_outb(PCI_MODE2_FORWARD_REG, bus);
    val = std_port_ind (0xc000 | (device << 8) | num);
    std_port_outb(PCI_MODE2_ENABLE_REG, 0x00);
  }
  return val;
}

static void pci_write_cfg2 (unsigned char bus, unsigned char device,
			    unsigned char fn, unsigned long num, unsigned long val)
{
  if (can_do_root_stuff) {
    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return;
    }
    port_real_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
    port_real_outb(PCI_MODE2_FORWARD_REG, bus);
    port_real_outd (0xc000 | (device << 8) | num, val);
    port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
    priv_iopl(0);
  } else {
    std_port_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
    std_port_outb(PCI_MODE2_FORWARD_REG, bus);
    std_port_outd (0xc000 | (device << 8) | num, val);
    std_port_outb(PCI_MODE2_ENABLE_REG, 0x00);
  }
}

/* only called from pci bios init */
static int pci_check_device_present_cfg2(unsigned char bus, unsigned char device,
					 unsigned char fn)
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

static char proc_pci_name_buf[] = "/proc/bus/pci/00/00.0";

static void proc_pci_set_name_buf(unsigned char bus, unsigned char device,
	unsigned char fn)
{
  sprintf(proc_pci_name_buf + 14, "%02x/%02x.%d", bus, device, fn);
}

/* only called from pci bios init */
static int pci_read_header_proc (unsigned char bus, unsigned char device,
	unsigned char fn, unsigned long *buf)
{
  int fd;

  proc_pci_set_name_buf(bus, device, fn);
  Z_printf("PCI: reading %s\n", proc_pci_name_buf);
  fd = open(proc_pci_name_buf, O_RDONLY);
  if (fd == -1) {
    error("can't open %s: %s\n", proc_pci_name_buf, strerror(errno));
    return 0;
  }

  /* Get only first 64 bytes: See src/linux/drivers/pci/proc.c for
     why. They are not joking. My NCR810 crashes the machine on read
     of register 0xd8 */
  
  read(fd, buf, 64);
  close(fd);
  return 0;
}

static unsigned long pci_read_proc (unsigned char bus, unsigned char device,
				    unsigned char fn, unsigned long reg)
{
  int fd;
  unsigned long val;

  proc_pci_set_name_buf(bus, device, fn);
  sprintf(proc_pci_name_buf, "/proc/bus/pci/%02x/%02x.%d", bus, device, fn);
  Z_printf("PCI: reading reg %ld from %s\n", reg, proc_pci_name_buf);
  fd = open(proc_pci_name_buf, O_RDONLY);
  if (fd == -1) {
    error("can't open %s: %s\n", proc_pci_name_buf, strerror(errno));
    return 0;
  }
  pread(fd, &val, sizeof(val), reg);
  close(fd);
  return val;
}

static void pci_write_proc (unsigned char bus, unsigned char device,
			    unsigned char fn, unsigned long reg, unsigned long val)
{
  int fd;

  PRIV_SAVE_AREA
  proc_pci_set_name_buf(bus, device, fn);
  Z_printf("PCI: writing reg %ld in %s\n", reg, proc_pci_name_buf);
  enter_priv_on();
  fd = open(proc_pci_name_buf, O_WRONLY);
  leave_priv_setting();
  if (fd == -1) {
    error("can't open %s: %s\n", proc_pci_name_buf, strerror(errno));
    return;
  }
  pwrite(fd, &val, sizeof(val), reg);
  close(fd);
}

/* only called from pci bios init */
static int pci_check_device_present_proc(unsigned char bus, unsigned char device,
					 unsigned char fn)
{
  sprintf(proc_pci_name_buf, "/proc/bus/pci/%02x/%02x.%d", bus, device, fn);
  Z_printf("PCI: checking access to %s\n", proc_pci_name_buf);
  return !access(proc_pci_name_buf, R_OK);
}

struct pci_funcs pci_cfg1 = {
  pci_read_cfg1,
  pci_write_cfg1,
  pci_read_header_cfg1,
  pci_check_device_present_cfg1
};

struct pci_funcs pci_cfg2 = {
  pci_read_cfg2,
  pci_write_cfg2,
  pci_read_header_cfg2,
  pci_check_device_present_cfg2
};

struct pci_funcs pci_proc = {
  pci_read_proc,
  pci_write_proc,
  pci_read_header_proc,
  pci_check_device_present_proc
};

static Bit8u pci_port_inb(ioport_t port)
{
    if (port != 0xcf9)
	return std_port_inb(port);
    return 0;
}

static void pci_port_outb(ioport_t port, Bit8u byte)
{
    /* don't allow DOSEMU to reset the CPU */
    if (port != 0xcf9)
	std_port_outb(port, byte);
}

/* SIDOC_BEGIN_FUNCTION pci_setup
 *
 * Register standard PCI ports 0xcf8-0xcff
 *
 * SIDOC_END_FUNCTION
 */
int pci_setup (void)
{
  emu_iodev_t io_device;

  if (config.pci) {
    pcibios_init();
    /* register PCI ports */
    io_device.read_portb = pci_port_inb;
    io_device.write_portb = pci_port_outb;
    io_device.read_portw = std_port_inw;
    io_device.write_portw = std_port_outw;
    io_device.read_portd = std_port_ind;
    io_device.write_portd = std_port_outd;
    io_device.irq = EMU_NO_IRQ;
    io_device.fd = -1;
  
    io_device.handler_name = "PCI Config";
    io_device.start_addr = PCI_CONF_ADDR;
    io_device.end_addr = PCI_CONF_DATA+3;
    port_register_handler(io_device, 0);
  }
  return 0;
}

/* basic r/o PCI emulation, enough for VGA-style classes */

/* sets the pci record if the bdf changes */
static pciRec *set_pcirec(unsigned short bdf)
{
  /* cached pci record */
  static pciRec *pcirec;
  pciRec *pci = pcirec;

  if (pci == NULL || bdf != pci->bdf) {
    pci = pcibios_find_bdf(bdf);
    if(pci != NULL)
      pcirec = pci;
  }
  return pci;
}

static unsigned long
emureadPciCfg1(unsigned char bus, unsigned char device,
	       unsigned char fn, unsigned long num)
{
  unsigned long val;
  unsigned short bdf;
  pciRec *pci;

  bdf = (bus << 8) | (device << 3) | fn;
  pci = set_pcirec(bdf);
  num &= 0xfc;
  if (pci == NULL)
    return 0xffffffff;
#if 0
  if (num >= 0x40)
    return readPciCfg1(reg);
#endif
  val = pci->header[num >> 2];
  Z_printf("PCIEMU: reading 0x%lx from %#lx\n",val,num);
  return val;
}

static void
emuwritePciCfg1(unsigned char bus, unsigned char device,
		unsigned char fn, unsigned long num,
		unsigned long val)
{
  unsigned short bdf;
  pciRec *pci;

  bdf = (bus << 8) | (device << 3) | fn;
  pci = set_pcirec(bdf);
  num &= 0xfc;
  if (pci == NULL)
    return;
#if 0
  if (num >= 0x40)
    writePciCfg1(reg, val);
#endif
  if ((pci->header[3] & 0x007f0000) == 0) {
    if (num >= PCI_BASE_ADDRESS_0 && num <= PCI_BASE_ADDRESS_5)
      val &= pci->region[num - PCI_BASE_ADDRESS_0].rawsize;
    if (num == PCI_ROM_ADDRESS) 
      val &= pci->region[6].rawsize;
  }
  Z_printf("PCIEMU: writing 0x%lx to %#lx\n",val,num);
  pci->header[num >> 2] = val;
}

static unsigned long current_pci_reg;

static Bit8u pciemu_port_inb(ioport_t port)
{
  /* 0xcf8 -- 0xcfb as bytes or words don't access sub-parts;
     for instance writing to 0xcf9 as a byte may reset the CPU */
  if (port == 0xcf9) /* TURBO/RESET control register */
    return 0;
  if (port < PCI_CONF_DATA)
    return 0xff;
  return readPci(current_pci_reg) >> ((port & 0x3) << 3);
}

static void pciemu_port_outb(ioport_t port, Bit8u byte)
{
  unsigned long val;
  int shift;
  
  if (port < PCI_CONF_DATA)
    return;
  val = readPci(current_pci_reg);
  shift = (port & 0x3) << 3;
  val &= ~(unsigned long)(0xff << shift);
  val |= byte << shift;
  writePci(current_pci_reg, val);
}

static Bit16u pciemu_port_inw(ioport_t port)
{
  if (port < PCI_CONF_DATA)
    return 0xffff;
  return readPci(current_pci_reg) >> ((port & 0x2) << 3);
}

static void pciemu_port_outw(ioport_t port, Bit16u value)
{
  unsigned long val;
  int shift;

  if (port < PCI_CONF_DATA)
    return;
  shift = (port & 0x2) << 3;
  val = readPci(current_pci_reg);
  val &= ~(unsigned long)(0xffff << shift);
  val |= value << shift;
  writePci(current_pci_reg, val);
}

static Bit32u pciemu_port_ind(ioport_t port)
{
  if (port == PCI_CONF_DATA)
    return readPci(current_pci_reg);
  if (port == PCI_CONF_ADDR)
    return current_pci_reg;
  return 0xffffffff;
}

static void pciemu_port_outd(ioport_t port, Bit32u value)
{
  if (port == PCI_CONF_ADDR)
    current_pci_reg = value;
  else if (port == PCI_CONF_DATA)
    writePci(current_pci_reg, value);
}

/* set up emulated r/o PCI config space for the given class */
pciRec *pciemu_setup(unsigned long class)
{
  static int pciemu_initialized = 0;
  pciRec *pci;

  if (!pciemu_initialized)
    pcibios_init();
  pci = pcibios_find_class(class, 0);
  if (pci == NULL)
    return pci;
  pci->enabled = 1;
  if (!pciemu_initialized) {
    emu_iodev_t io_device;

    /* register PCI ports */
    io_device.read_portb = pciemu_port_inb;
    io_device.write_portb = pciemu_port_outb;
    io_device.read_portw = pciemu_port_inw;
    io_device.write_portw = pciemu_port_outw;
    io_device.read_portd = pciemu_port_ind;
    io_device.write_portd = pciemu_port_outd;
    io_device.irq = EMU_NO_IRQ;
    io_device.fd = -1;

    io_device.handler_name = "PCI Emulated Config";
    io_device.start_addr = PCI_CONF_ADDR;
    io_device.end_addr = PCI_CONF_DATA+3;
    port_register_handler(io_device, 0);

    pciConfigType->read = emureadPciCfg1;
    pciConfigType->write = emuwritePciCfg1;

    pciemu_initialized = 1;
  }
  return pci;
}
