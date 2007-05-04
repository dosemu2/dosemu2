/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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


static struct pci_funcs pci_cfg1, pci_cfg2, pci_proc;

/*
 * from PCIUTILS and Linux kernel arch/i386/pci/direct.c:
 *
 * Before we decide to use direct hardware access mechanisms, we try to do some
 * trivial checks to ensure it at least _seems_ to be working -- we just test
 * whether bus 00 contains a host bridge (this is similar to checking
 * techniques used in XFree86, but ours should be more reliable since we
 * attempt to make use of direct access hints provided by the PCI BIOS).
 *
 * This should be close to trivial, but it isn't, because there are buggy
 * chipsets (yes, you guessed it, by Intel and Compaq) that have no class ID.
 */

static int
intel_sanity_check(struct pci_funcs *m)
{
  int dev;

  Z_printf("PCI: ...sanity check for mechanism %s ", m->name);
  for(dev = 0; dev < 32; dev++) {
    uint16_t class, vendor;
    class = m->read(0, dev, 0, PCI_CLASS_DEVICE, 2);
    if (class == PCI_CLASS_BRIDGE_HOST || class == PCI_CLASS_DISPLAY_VGA)
      break;
    vendor = m->read(0, dev, 0, PCI_VENDOR_ID, 2);
    if (vendor == PCI_VENDOR_ID_INTEL || vendor == PCI_VENDOR_ID_COMPAQ)
      break;
  }
  if (dev < 32) {
    Z_printf("succeeded for dev=%x\n", dev);
    return 1;
  }
  Z_printf("not succeeded\n");
  return 0;
}

/*
 * Return NULL if no PCI is present.
 */
struct pci_funcs *pci_check_conf(void)
{
    unsigned long val;
    struct pci_funcs *m;

    if (!config.pci && access("/proc/bus/pci", R_OK) == 0)
      return &pci_proc;

    if (priv_iopl(3)) {
      error("iopl(): %s\n", strerror(errno));
      return 0;
    }

    m = &pci_cfg1;
    port_real_outd(PCI_CONF_ADDR,PCI_EN);
    val = port_real_ind(PCI_CONF_ADDR);
    port_real_outd(PCI_CONF_ADDR, 0);

    if (val != PCI_EN) {
      /* from PCIUTILS: */
      /* This is ugly and tends to produce false positives. Beware. */

      port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
      port_real_outb(PCI_MODE2_FORWARD_REG, 0x00);
      m = (inb(PCI_MODE2_ENABLE_REG) == 0x00 &&
	   inb(PCI_MODE2_FORWARD_REG) == 0x00) ? &pci_cfg2 : NULL;
    }

    priv_iopl(0);
    if (m && intel_sanity_check(m))
      return m;
    return NULL;
}

static int pci_no_open(unsigned char bus, unsigned char device,
		       unsigned char fn)
{
  return -1;
}

/* only called from pci bios init */
static int pci_read_header_cfg1 (unsigned char bus, unsigned char device,
				 unsigned char fn, unsigned int *buf)
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
				    unsigned char fn, unsigned long reg, int len)
{
  unsigned long val;
  unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
                      PCI_EN;

  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return 0;
  }
  port_real_outd (PCI_CONF_ADDR, bx | (reg & ~3));
  if (len == 1)
    val = port_real_inb (PCI_CONF_DATA + (reg & 3));
  else if (len == 2)
    val = port_real_inw (PCI_CONF_DATA + (reg & 2));
  else
    val = port_real_ind (PCI_CONF_DATA);
  port_real_outd (PCI_CONF_ADDR, 0);
  priv_iopl(0);
  return val;
}

static void pci_write_cfg1 (unsigned char bus, unsigned char device,
			    unsigned char fn, unsigned long reg, unsigned long val, int len)
{
  unsigned long bx = ((fn&7)<<8) | ((device&31)<<11) | (bus<<16) |
                      PCI_EN;

  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return;
  }
  port_real_outd (PCI_CONF_ADDR, bx | (reg & ~3));
  if (len == 1)
    port_real_outb (PCI_CONF_DATA + (reg & 3), val);
  else if (len == 2)
    port_real_outw (PCI_CONF_DATA + (reg & 2), val);
  else
    port_real_outd (PCI_CONF_DATA, val);
  port_real_outd (PCI_CONF_ADDR, 0);
  priv_iopl(0);
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
				 unsigned char fn, unsigned int *buf)
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
				    unsigned char fn, unsigned long num, int len)
{
  unsigned long val;

  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return 0;
  }
  port_real_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
  port_real_outb(PCI_MODE2_FORWARD_REG, bus);
  if (len == 1)
    val = port_real_inb (0xc000 | (device << 8) | num);
  else if (len == 2)
    val = port_real_inw (0xc000 | (device << 8) | num);
  else
    val = port_real_ind (0xc000 | (device << 8) | num);
  port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
  priv_iopl(0);
  return val;
}

static void pci_write_cfg2 (unsigned char bus, unsigned char device,
			    unsigned char fn, unsigned long num, unsigned long val, int len)
{
  if (priv_iopl(3)) {
    error("iopl(): %s\n", strerror(errno));
    return;
  }
  port_real_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
  port_real_outb(PCI_MODE2_FORWARD_REG, bus);
  if (len == 1)
    port_real_outb (0xc000 | (device << 8) | num, val);
  else if (len == 2)
    port_real_outw (0xc000 | (device << 8) | num, val);
  else
    port_real_outd (0xc000 | (device << 8) | num, val);
  port_real_outb(PCI_MODE2_ENABLE_REG, 0x00);
  priv_iopl(0);
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

static int pci_open_proc(unsigned char bus, unsigned char device,
			 unsigned char fn)
{
  static char proc_pci_name_buf[] = "/proc/bus/pci/00/00.0";
  int fd;

  PRIV_SAVE_AREA
  sprintf(proc_pci_name_buf + 14, "%02x/%02x.%d", bus, device, fn);
  Z_printf("PCI: opening %s\n", proc_pci_name_buf);
  enter_priv_on();
  fd = open(proc_pci_name_buf, O_RDWR);
  leave_priv_setting();
  return fd;
}

/* only called from pci bios init */
static int pci_read_header_proc (unsigned char bus, unsigned char device,
	unsigned char fn, unsigned int *buf)
{
  int fd = pci_open_proc(bus, device, fn);
  if (fd == -1)
    return 0;

  /* Get only first 64 bytes: See src/linux/drivers/pci/proc.c for
     why. They are not joking. My NCR810 crashes the machine on read
     of register 0xd8 */
  
  read(fd, buf, 64);
  close(fd);
  return 0;
}

static unsigned long pci_read_proc (unsigned char bus, unsigned char device,
				    unsigned char fn, unsigned long reg, int len)
{
  unsigned long val;
  int fd = pci_open_proc(bus, device, fn);
  if (fd == -1)
    return 0;
  Z_printf("PCI: reading reg %ld\n", reg);
  pread(fd, &val, len, reg);
  close(fd);
  return val;
}

static void pci_write_proc (unsigned char bus, unsigned char device,
			    unsigned char fn, unsigned long reg, unsigned long val, int len)
{
  int fd = pci_open_proc(bus, device, fn);
  if (fd == -1)
    return;
  Z_printf("PCI: writing reg %ld\n", reg);
  pwrite(fd, &val, len, reg);
  close(fd);
}

/* only called from pci bios init */
static int pci_check_device_present_proc(unsigned char bus, unsigned char device,
					 unsigned char fn)
{
  int fd = pci_open_proc(bus, device, fn);
  close(fd);
  return (fd != -1);
}

static struct pci_funcs pci_cfg1 = {
  "1",
  pci_no_open,
  pci_read_cfg1,
  pci_write_cfg1,
  pci_read_header_cfg1,
  pci_check_device_present_cfg1
};

static struct pci_funcs pci_cfg2 = {
  "2",
  pci_no_open,
  pci_read_cfg2,
  pci_write_cfg2,
  pci_read_header_cfg2,
  pci_check_device_present_cfg2
};

static struct pci_funcs pci_proc = {
  "/proc",
  pci_open_proc,
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
  
    if (pciConfigType->name[0] == '1') {
      io_device.handler_name = "PCI Config Type 1";
      io_device.start_addr = PCI_CONF_ADDR;
      io_device.end_addr = PCI_CONF_DATA+3;
      port_register_handler(io_device, 0);
    } else {
      io_device.handler_name = "PCI Config Type 2";
      io_device.start_addr = PCI_MODE2_ENABLE_REG;
      io_device.end_addr = PCI_MODE2_FORWARD_REG + 1;
      port_register_handler(io_device, 0);
      io_device.start_addr = 0xc000;
      io_device.end_addr = 0xcfff;
      port_register_handler(io_device, 0);
    }
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

static unsigned long current_pci_reg;

static unsigned long pciemu_port_read(ioport_t port, int len)
{
  unsigned long val;
  unsigned short bdf;
  pciRec *pci;
  unsigned char num;

  if (!(current_pci_reg & PCI_EN))
    return 0xffffffff;

  bdf = (current_pci_reg >> 8) & 0xffff;
  pci = set_pcirec(bdf);
  if (pci == NULL)
    return 0xffffffff;
  /* values >= 0x40 have to go directly to the hardware.
     They can be dangerous so are only allowed if pci->ext_enabled
     the Linux kernel only lets us read if num < 64
     unless we're root (even if the fd was opened as root)
  */
  num = current_pci_reg & 0xff;
  if (num < 0x40) {
    val = pci->header[num >> 2];
    if (len == 1)
      val = (val >> ((port & 3) << 3)) & 0xff;
    else if (len == 2)
      val = (val >> ((port & 2) << 3)) & 0xffff;
  } else if (pci->ext_enabled) {
    pci_port_outd(PCI_CONF_ADDR, current_pci_reg);
    val = len == 1 ? std_port_inb(port) : len == 2 ? std_port_inw(port) :
      std_port_ind(port);
  } else
    val = 0xffffffff;
  Z_printf("PCIEMU: reading 0x%lx from %#x, len=%d\n",val,num,len);
  return val;
}

static void pciemu_port_write(ioport_t port, unsigned long val, int len)
{
  unsigned char num;
  unsigned short bdf;
  pciRec *pci;

  if (!(current_pci_reg & PCI_EN) || current_pci_reg == PCI_EN)
	return;

  bdf = (current_pci_reg >> 8) & 0xffff;
  pci = set_pcirec(bdf);
  if (pci == NULL)
    return;
  /* values >= 0x40 have to go directly to the hardware.
     They can be dangerous so are only allowed if pci->ext_enabled */
  num = current_pci_reg & 0xff;
  if (num < 0x40) {
    unsigned long value = pci->header[num >> 2];
    int shift = (port & (4 - len)) << 3;
    if (len == 1)
      value &= ~(unsigned long)(0xff << shift);
    else if (len == 2)
      value &= ~(unsigned long)(0xffff << shift);
    val = value | (val << shift);
    if ((pci->header[3] & 0x007f0000) == 0) {
      if (num >= PCI_BASE_ADDRESS_0 && num <= PCI_BASE_ADDRESS_5)
	val &= pci->region[num - PCI_BASE_ADDRESS_0].rawsize;
      if (num == PCI_ROM_ADDRESS) 
	val &= pci->region[6].rawsize;
    }
    pci->header[num >> 2] = val;
  } else if (pci->ext_enabled) {
    pci_port_outd(PCI_CONF_ADDR, current_pci_reg);
    if (len == 1)
      std_port_outb(port, val);
    else if (len == 2)
      std_port_outw(port, val);
    else
      std_port_outd(port, val);
  }
  Z_printf("PCIEMU: writing 0x%lx to %#x, len=%d\n",val,num,len);
}

static Bit8u pciemu_port_inb(ioport_t port)
{
  /* 0xcf8 -- 0xcfb as bytes or words don't access sub-parts;
     for instance writing to 0xcf9 as a byte may reset the CPU */
  if (port == 0xcf9) /* TURBO/RESET control register */
    return 0;
  if (port >= PCI_CONF_DATA)
    return pciemu_port_read(port, 1);
  return 0xff;
}

static void pciemu_port_outb(ioport_t port, Bit8u byte)
{
  if (port >= PCI_CONF_DATA)
    pciemu_port_write(port, byte, 1);
}

static Bit16u pciemu_port_inw(ioport_t port)
{
  if (port == PCI_CONF_DATA || port == PCI_CONF_DATA + 2)
    return pciemu_port_read(port, 2);
  return 0xffff;
}

static void pciemu_port_outw(ioport_t port, Bit16u value)
{
  if (port == PCI_CONF_DATA || port == PCI_CONF_DATA + 2)
    pciemu_port_write(port, value, 2);
}

static Bit32u pciemu_port_ind(ioport_t port)
{
  if (port == PCI_CONF_DATA)
    return pciemu_port_read(port, 4);
  if (port == PCI_CONF_ADDR)
    return current_pci_reg;
  return 0xffffffff;
}

static void pciemu_port_outd(ioport_t port, Bit32u value)
{
  if (port == PCI_CONF_ADDR)
    current_pci_reg = value & 0x80fffffc;
  else if (port == PCI_CONF_DATA)
    pciemu_port_write(port, value, 4);
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
  pci->enabled = pci->ext_enabled = 1;
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

    pciemu_initialized = 1;
  }
  return pci;
}
