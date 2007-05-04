/* 
 * (C) Copyright 1999, Egbert Eich (original code)
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * PCI BIOS support for dosemu
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "pci.h"
#include "emu.h"
#include "cpu.h"
#include "port.h"

#define PCI_VERSION 0x0210  /* version 2.10 */
#define PCI_CONFIG_1 0x01 
#define PCI_CONFIG_2 0x02 
#define MAX_DEV_PER_VENDOR_CFG1  32
#define MAX_PCI_DEVICES 64
#define PCI_MULTIFUNC_DEV  0x80
#define PCI_CLASS_MASK 0xff0000
#define PCI_SUBCLASS_MASK 0xff00
#define PCI_CLASS_BRIDGE 0x060000
#define PCI_SUBCLASS_BRIDGE_PCI 0x0400
#define PCI_SUBCLASS_BRIDGE_HOST 0x0000
#define PCI_BRIDGE_CLASS(class) ((class & PCI_CLASS_MASK) == PCI_CLASS_BRIDGE)
#define PCI_BRIDGE_PCI_CLASS(class) ((class & PCI_SUBCLASS_MASK) \
                                       == PCI_SUBCLASS_BRIDGE_PCI)
#define PCI_BRIDGE_HOST_CLASS(class) ((class & PCI_SUBCLASS_MASK) \
                                       == PCI_SUBCLASS_BRIDGE_HOST)
				     
/* variables get initialized by pcibios_init() */
static pciPtr pciList = NULL;
struct pci_funcs *pciConfigType = NULL;
static int lastBus = 0;

/* functions used by pci_bios() */
static unsigned short findDevice(unsigned short device,
				 unsigned short vendor, int num);
static unsigned short findClass(unsigned long class,  int num);
static int interpretCfgSpace(unsigned int *pciheader,unsigned int *pcibuses,
			     int busidx, unsigned char dev,
			     unsigned char func);

static unsigned long readPciCfg1(unsigned long reg, int len);
static void writePciCfg1(unsigned long reg, unsigned long val, int len);
static unsigned long readPciCfg2(unsigned long reg, int len);
static void writePciCfg2(unsigned long reg, unsigned long val, int len);

static unsigned long (*readPci)(unsigned long reg, int len) = readPciCfg1;
static void (*writePci)(unsigned long reg, unsigned long val, int len) = writePciCfg1;

static void write_dword(unsigned short loc,
			unsigned short reg,unsigned long val);
static void write_word(unsigned short loc,
		       unsigned short reg,unsigned short val);
static void write_byte(unsigned short loc,
		       unsigned short reg,unsigned char val);
static unsigned long read_dword(unsigned short loc,unsigned short reg);
static unsigned short read_word(unsigned short loc,unsigned short reg);
static unsigned char read_byte(unsigned short loc,unsigned short reg);

#define SUCCESS  { clear_CF();\
                 HI(ax) = PCIBIOS_STAT_SUCCESSFUL;}
#define FAIL(reason) { set_CF();\
                     HI(ax) = reason;}

void
pci_bios(void)
{
    unsigned short val;
    
    if (!pciConfigType)
	return;

    switch (LWORD(eax)) {
    case PCIBIOS_PCI_BIOS_PRESENT:
	Z_printf("PCIBIOS: pcibios present\n");
	HI(ax) = 0;
	clear_CF();
	if (pciConfigType->name[0] != '2')
	    LO(ax) = PCI_CONFIG_1; /*  no special cylce */
	else 
	    LO(ax) = PCI_CONFIG_2; /*  no special cylce */
	LWORD(ebx) = PCI_VERSION;  /* version 2.10 */
	REG(edx) = PCI_SIGNATURE;
	LO(cx) = lastBus;
	break;
    case PCIBIOS_FIND_PCI_DEVICE:
	if (LWORD(edx) == 0xffff)
	    FAIL(PCIBIOS_BAD_VENDOR_ID)
	else if ((val = findDevice(LWORD(ecx),LWORD(edx),LWORD(esi)))
		 == 0xffff)
	    FAIL(PCIBIOS_DEVICE_NOT_FOUND)
	else {
	    LWORD(ebx) = val;
	    SUCCESS
	}
	break;
    case PCIBIOS_FIND_PCI_CLASS_CODE:
	if ((val = findClass(REG(ecx),LWORD(esi))) == 0xffff)
	    FAIL(PCIBIOS_DEVICE_NOT_FOUND)
	else {
	    LWORD(ebx) = val;
	    SUCCESS
	}
	break;
    case PCIBIOS_GENERATE_SPECIAL_CYCLE:
	FAIL(PCIBIOS_FUNC_NOT_SUPPORTED);
	break;
    case PCIBIOS_READ_CONFIG_BYTE:
	if (LWORD(edi) & 0xff00)
	    FAIL(PCIBIOS_BAD_REGISTER_NUMBER)
	LO(cx) = read_byte(LWORD(ebx),LWORD(edi));
	SUCCESS
	break;
    case PCIBIOS_READ_CONFIG_WORD:
	if (LWORD(edi) & 0xff01)
	    FAIL(PCIBIOS_BAD_REGISTER_NUMBER)
	LWORD(ecx) = read_word(LWORD(ebx),LWORD(edi));
	SUCCESS
	break;
    case PCIBIOS_READ_CONFIG_DWORD:
	if (LWORD(edi) & 0xff03)
	    FAIL(PCIBIOS_BAD_REGISTER_NUMBER)
	REG(ecx) = read_dword(LWORD(ebx),LWORD(edi));
	SUCCESS
	break;
    case PCIBIOS_WRITE_CONFIG_BYTE:
	if (LWORD(edi) & 0xff00)
	    FAIL(PCIBIOS_BAD_REGISTER_NUMBER)
	write_byte(LWORD(ebx),LWORD(edi),LO(cx));
	SUCCESS
	break;
    case PCIBIOS_WRITE_CONFIG_WORD:
	if (LWORD(edi) & 0xff01)
	    FAIL(PCIBIOS_BAD_REGISTER_NUMBER)
	write_word(LWORD(ebx),LWORD(edi),LWORD(ecx));
	SUCCESS
	break;
    case PCIBIOS_WRITE_CONFIG_DWORD:
	if (LWORD(edi) & 0xff03)
	    FAIL(PCIBIOS_BAD_REGISTER_NUMBER)
	write_dword(LWORD(ebx),LWORD(edi),REG(ecx));
	SUCCESS
	break;
    case PCIBIOS_GET_IRQ_ROUTING_OPTIONS:
	FAIL(PCIBIOS_FUNC_NOT_SUPPORTED)
	break;
    case PCIBIOS_SET_PCI_IRQ:
	FAIL(PCIBIOS_FUNC_NOT_SUPPORTED)
	break;
    }
    return;
}

static int numbus = 0;
static int hostbridges = 0;

int
pcibios_init(void)
{
    unsigned int pcibuses[16];
    unsigned int pciheader[16];
    int busidx = 0;
    int idx = 0;
    int func = 0;
    int cardnum;
    int cardnum_lo = 0;
    int cardnum_hi = MAX_DEV_PER_VENDOR_CFG1;
    int func_hi = 8;

    pcibuses[0] = 0;

    Z_printf("PCI enabled\n");
    
    pciConfigType = pci_check_conf();

    if (!pciConfigType) {
	Z_printf("Unable to access PCI config space\n");
	return 0;
    }

    Z_printf("PCI config type %s\n",pciConfigType->name);

    if (pciConfigType->name[0] == '2') {
	writePci = writePciCfg2;
	readPci = readPciCfg2;
	cardnum_lo = 0xC0;
	cardnum_hi = 0xD0;
	func_hi = 1;
    }
    do {
	for (cardnum = cardnum_lo; cardnum < cardnum_hi; cardnum++) {
	    func = 0;
	    do {
	    	if (!pciConfigType->check_device_present(pcibuses[busidx],
							 cardnum,func))
		    break;
		pciConfigType->read_header(pcibuses[busidx],cardnum,func,
					   pciheader);
		func = interpretCfgSpace(pciheader,pcibuses,busidx,
					 cardnum,func);
		if (idx++ > MAX_PCI_DEVICES)
		    continue;
	    } while (func < func_hi);
	}
    } while (++busidx <= numbus);

    lastBus = numbus;
    return 1;
}

static unsigned short
findDevice(unsigned short device, unsigned short vendor, int num)
{
    pciPtr pci = pciList;

    Z_printf("PCIBIOS: find device %d id=%04x vend=%04x", num,device,vendor);
    while (pci) {
	if ((pci->vendor == vendor) && (pci->device == device) && pci->enabled) {
	    if (num-- == 0) {
		Z_printf(" at: %04x\n",pci->bdf);
		return pci->bdf;
	    }
	}
	pci=pci->next;
    }
    Z_printf(" not found\n");
    return 0xffff;
}

pciRec *pcibios_find_class(unsigned long class,  int num)
{
    pciPtr pci = pciList;

    Z_printf("pcibios find device %d class %lx\n", num, class);
    while (pci) {
	if ((pci->class & 0xFFFFFF) == (class & 0xFFFFFF)) {
	    if (num-- == 0) {
		Z_printf(" at: %04x\n",pci->bdf);
		return pci;
	    }
	}
	pci=pci->next;
    }
    Z_printf(" not found\n");
    return NULL;
}

pciRec *pcibios_find_bdf(unsigned short bdf)
{
    pciPtr pci = pciList;

    Z_printf("pcibios find bdf %x ", bdf);
    while (pci) {
	if (pci->enabled && pci->bdf == bdf) {
	    Z_printf("class: %lx\n",pci->class);
	    return pci;
	}
	pci=pci->next;
    }
    Z_printf(" not found\n");
    return NULL;
}

static unsigned short
findClass(unsigned long class,  int num)
{
    pciPtr pci = pcibios_find_class(class, num);
    return (pci && pci->enabled) ? pci->bdf : 0xffff;
}

static unsigned long readPciCfg1 (unsigned long reg, int len)
{
    unsigned long val;

    port_outd (PCI_CONF_ADDR, reg & ~3);
    if (len == 1)
	val = port_inb (PCI_CONF_DATA + (reg & 3));
    else if (len == 2)
	val = port_inw (PCI_CONF_DATA + (reg & 2));
    else
	val = port_ind (PCI_CONF_DATA);
    port_outd (PCI_CONF_ADDR, 0);
    Z_printf("PCIBIOS: reading 0x%lx from 0x%lx, len=%d\n",val,reg,len);
    return val;
}

static void writePciCfg1 (unsigned long reg, unsigned long val, int len)
{
    Z_printf("PCIBIOS writing: 0x%lx to 0x%lx, len=%d\n",val,reg,len);
    port_outd (PCI_CONF_ADDR, reg & ~3);
    if (len == 1)
	port_outb (PCI_CONF_DATA + (reg & 3), val);
    else if (len == 2)
	port_outw (PCI_CONF_DATA + (reg & 2), val);
    else
	port_outd (PCI_CONF_DATA, val);
    port_outd (PCI_CONF_ADDR, 0);
}

static unsigned long readPciCfg2 (unsigned long reg, int len)
{
    unsigned long val;
    unsigned char bus, dev, num, fn;
  
    bus = (reg >> 16) & 0xff;
    dev = (reg >> 11) & 0x1f;
    fn  = (reg >>  8) & 7;
    num = reg & 0xff;

    port_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
    port_outb(PCI_MODE2_FORWARD_REG, bus);
    if (len == 1)
	val = port_inb (0xc000 | (dev << 8) | num);
    else if (len == 2)
	val = port_inw (0xc000 | (dev << 8) | num);
    else
	val = port_ind (0xc000 | (dev << 8) | num);
    port_outb(PCI_MODE2_ENABLE_REG, 0x00);
    Z_printf("PCIBIOS: reading 0x%lx from 0x%lx, len=%d\n",val,reg,len);
    return val;
}

static void writePciCfg2 (unsigned long reg, unsigned long val, int len)
{
    unsigned char bus, dev, num, fn;

    bus = (reg >> 16) & 0xff;
    dev = (reg >> 11) & 0x1f;
    fn  = (reg >>  8) & 7;
    num = reg & 0xff;

    Z_printf("PCIBIOS writing: 0x%lx to 0x%lx, len=%d\n",val,reg,len);
    port_outb(PCI_MODE2_ENABLE_REG, (fn << 1) | 0xF0);
    port_outb(PCI_MODE2_FORWARD_REG, bus);
    if (len == 1)
	port_outb (0xc000 | (dev << 8) | num, val);
    else if (len == 2)
	port_outw (0xc000 | (dev << 8) | num, val);
    else
	port_outd (0xc000 | (dev << 8) | num, val);
    port_outb(PCI_MODE2_ENABLE_REG, 0x00);
}

static void
write_dword(unsigned short loc,unsigned short reg,unsigned long val)
{
    writePci(loc << 8 | (reg & 0xfc) | PCI_EN, val, 4);
}

static void
write_word(unsigned short loc,unsigned short reg,unsigned short word)
{
    writePci(loc << 8 | (reg & 0xfe) | PCI_EN, word, 2);
}

static void
write_byte(unsigned short loc,unsigned short reg,unsigned char byte)
{
    writePci(loc << 8 | (reg & 0xff) | PCI_EN, byte, 1);
}

static unsigned long
read_dword(unsigned short loc,unsigned short reg)
{
    return readPci(loc << 8 | (reg & 0xfc) | PCI_EN, 4);
}

static unsigned short
read_word(unsigned short loc,unsigned short reg)
{
    return readPci(loc << 8 | (reg & 0xfe) | PCI_EN, 2);
}

static unsigned char
read_byte(unsigned short loc,unsigned short reg)
{
    return readPci(loc << 8 | (reg & 0xff) | PCI_EN, 1);
}

static int proc_bus_pci_devices_get_sizes(pciPtr pci)
{
    FILE *f;
    char buf[512];

    f = fopen("/proc/bus/pci/devices", "r");
    if (!f) {
	Z_printf("PCI: Cannot open /proc/bus/pci/devices\n");
	return 0;
    }
    while (fgets(buf, sizeof(buf)-1, f)) {
	unsigned int cnt, i, bdf, tmp;
	unsigned int lens[7];

#define F " %08x"
	cnt = sscanf(buf, "%x %x %x"  F F F F F F F  F F F F F F F,
		    &bdf, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp,
		    &lens[0], &lens[1], &lens[2], &lens[3], &lens[4], &lens[5],
		    &lens[6]);
#undef F
	if (cnt != 17) {
	    Z_printf("PCI: /proc: (read only %d items)", cnt);
	    fclose(f);
	    return 0;
	}
	if (pci->bdf == bdf)
	    for (i = 0; i < 7; i++)
		pci->region[i].rawsize = lens[i];
    }
    fclose(f);
    Z_printf("PCI: proc_bus_pci_get_sizes done\n");
    return 1;
}

static int
interpretCfgSpace(unsigned int *pciheader,unsigned int *pcibuses,int busidx,
		   unsigned char dev, unsigned char func)
{
    static char *typestr[] = { "MEM", "IO", "ROM" };
    int tmp, i;

    pciPtr pciTmp = (pciPtr)malloc(sizeof(pciRec));
    pciTmp->next = pciList;
    pciList = pciTmp;
    pciTmp->enabled = config.pci;
    pciTmp->bdf = pcibuses[busidx] << 8 | dev << 3 | func;
    pciTmp->vendor = pciheader[0] & 0xffff;
    pciTmp->device = pciheader[0] >> 16;
    pciTmp->class = pciheader[0x02] >> 8;
    if (PCI_BRIDGE_CLASS(pciTmp->class))  {
	if (PCI_BRIDGE_PCI_CLASS(pciTmp->class)) { /*PCI-PCI*/
	    Z_printf("PCI-PCI bridge:\n");
	    /* always enable for PCI emulation */
	    pciTmp->enabled = 1;
	    tmp = (pciheader[0x6] >> 8) & 0xff;
	    if (tmp > 0)
		pcibuses[++numbus] = tmp; /* secondary bus */
	} else if (PCI_BRIDGE_HOST_CLASS(pciTmp->class)) {
	    Z_printf("PCI-HOST bridge:\n");
	    /* always enable for PCI emulation */
	    pciTmp->enabled = 1;
	    if (++hostbridges > 1) {/* HOST-PCI bridges*/
		numbus++;
		pcibuses[numbus] = numbus;
	    }
	}
    }
    memcpy(pciTmp->header, pciheader, sizeof(*pciheader) * 16);
    memset(&pciTmp->region, 0, sizeof(pciTmp->region));
    if ((pciheader[3] & 0x007f0000) == 0) {
      int got_sizes = proc_bus_pci_devices_get_sizes(pciTmp);
      for (i = 0; i < 7; i++) {
	unsigned long mask, base, size, pci_val, pci_val1;
	unsigned long reg = PCI_BASE_ADDRESS_0 + (i << 2);
	int type;

	if (i == 6) reg = PCI_ROM_ADDRESS;
	pci_val = pciheader[reg/4];
	if (pci_val == 0xffffffff || pci_val == 0)
	    continue;
	if (i == 6) {
	    mask = PCI_ROM_ADDRESS_MASK;
	    type = 2;
	} else if (pci_val & PCI_BASE_ADDRESS_SPACE_IO) {
	    mask = PCI_BASE_ADDRESS_IO_MASK & 0xffff;
	    type = PCI_BASE_ADDRESS_SPACE_IO;
	} else { /* memory */
	    mask = PCI_BASE_ADDRESS_MEM_MASK;
	    type = PCI_BASE_ADDRESS_SPACE_MEMORY;
	}
	base = pci_val & mask;
	if (!base)
	    continue;
	pciTmp->region[i].base = base;
	pciTmp->region[i].type = type;
	if (!got_sizes) {
	    pciConfigType->write(pcibuses[busidx], dev, func, reg, 0xffffffff, 4);
	    pci_val1 = pciConfigType->read(pcibuses[busidx], dev, func, reg, 4);
	    pciConfigType->write(pcibuses[busidx], dev, func, reg, pci_val, 4);
	    pciTmp->region[i].rawsize = pci_val1;
	}
	if (pciTmp->region[i].rawsize == 0) {
	    pciTmp->region[i].base = 0;
	    continue;
	}
	size = pciTmp->region[i].rawsize & mask;
	size = (size & ~(size - 1)) - 1;
	Z_printf("PCI: found %s region at %#lx [%#lx] (%lx,%lx)\n",
		 typestr[type], base, size, pci_val, pciTmp->region[i].rawsize);
	pciTmp->region[i].size = size;
      }
    }

    Z_printf("bus:%i dev:%i func:%i vend:0x%x dev:0x%x"
	     " class:0x%lx bdf:0x%x\n",pcibuses[busidx],
	     dev,func,pciTmp->vendor,pciTmp->device,
	     pciTmp->class,pciTmp->bdf);
    if ((func == 0) 
	&& ((((pciheader[0x03] >> 16) & 0xff)
	     & PCI_MULTIFUNC_DEV)==0))
	func = 8;
    else
	func++;
    return func;
}

