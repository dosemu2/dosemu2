/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * PCI BIOS support for dosemu
 *
 * (C) Copyright 1999, Egbert Eich
 */
#include <unistd.h>
#include <stdlib.h>
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
				     
typedef struct _pciRec {
    unsigned short bdf;
    unsigned short vendor;
    unsigned short device;
    unsigned long class;
    struct _pciRec *next;
} pciRec, *pciPtr;

/* variables get initialized by pcibios_init() */
static pciPtr pciList = NULL;
static int pciConfigType = 0;
static int lastBus = 0;

/* functions used by pci_bios() */
static unsigned short findDevice(unsigned short device,
				 unsigned short vendor, int num);
static unsigned short findClass(unsigned long class,  int num);
static int interpretCfgSpace(unsigned long *pciheader,unsigned long *pcibuses,
			     int busidx, unsigned char dev,
			     unsigned char func);

static unsigned long readPciCfg1(unsigned long reg);
static void writePciCfg1(unsigned long reg, unsigned long val);
static unsigned long readPciCfg2(unsigned long reg);
static void writePciCfg2(unsigned long reg, unsigned long val);

static unsigned long (*readPci)(unsigned long reg) = readPciCfg1;
static void (*writePci)(unsigned long reg, unsigned long val) = writePciCfg1;


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
    
    switch (LWORD(eax)) {
    case PCIBIOS_PCI_BIOS_PRESENT:
	Z_printf("PCIBIOS: pcibios present\n");
	if (!pciConfigType)
	    set_CF();
	else {
	    HI(ax) = 0;
	    clear_CF();
	}
	if (pciConfigType == 1)
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
    unsigned long pcibuses[16];
    unsigned long pciheader[64];
    int busidx = 0;
    int idx = 0;
    int func = 0;
    int cardnum;

    pcibuses[0] = 0;
    
    if (!config.pci) return 0;
    Z_printf("PCI enabled\n");
    
    pciConfigType = pci_check_conf();
    Z_printf("PCI config type %i\n",pciConfigType);

    if (pciConfigType == 1) {
	do {
	    for (cardnum = 0; cardnum < MAX_DEV_PER_VENDOR_CFG1; cardnum++) {
		func = 0;
		do {
		    
		    if (!pci_check_device_present_cfg1(pcibuses[busidx],
						  cardnum,func))
			break;
		    pci_read_header_cfg1(pcibuses[busidx],cardnum,func,
					 pciheader);
		    func = interpretCfgSpace(pciheader,pcibuses,busidx,
					     cardnum,func);
		    if (idx++ > MAX_PCI_DEVICES)
			continue;
		} while (func < 8);
	    }
	} while (++busidx <= numbus);
    } else if (pciConfigType == 2) {
	writePci = writePciCfg2;
	readPci = readPciCfg2;
	pci_read_header = pci_read_header_cfg2;
	
	do {
	    for (cardnum = 0xC0; cardnum < 0xD0; cardnum++) {
		    if (!pci_check_device_present_cfg2(pcibuses[busidx],
						  cardnum))
			break;
		    pci_read_header_cfg2(pcibuses[busidx],cardnum,0,pciheader);
		    interpretCfgSpace(pciheader,pcibuses,busidx,
					     cardnum,func);
		    if (idx++ > MAX_PCI_DEVICES)
			continue;
	    }
	} while (++busidx <= numbus);
    }
    
    
    if (pciConfigType) {
	lastBus = numbus;
	return 1;
    } else
	return 0;
}

static unsigned short
findDevice(unsigned short device, unsigned short vendor, int num)
{
    pciPtr pci = pciList;

    Z_printf("PCIBIOS: find device %d id=%04x vend=%04x", num,device,vendor);
    while (pci) {
	if ((pci->vendor == vendor) && (pci->device == device)) {
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

static unsigned short
findClass(unsigned long class,  int num)
{
    pciPtr pci = pciList;

    Z_printf("pcibios find device %d class %lx\n", num, class);
    while (pci) {
	if ((pci->class & 0xFFFFFF) == (class & 0xFFFFFF)) {
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

static unsigned long
readPciCfg1(unsigned long reg)
{
    unsigned long val;
    
    std_port_outd(PCI_CONF_ADDR, reg);
    val = std_port_ind(PCI_CONF_DATA);
    std_port_outd(PCI_CONF_ADDR, 0);
    Z_printf("PCIBIOS: reading 0x%lx from 0x%lx\n",val,reg);
    return val;
}

static void
writePciCfg1(unsigned long reg, unsigned long val)
{
    Z_printf("PCIBIOS writing: 0x%lx to 0x%lx\n",val,reg);
    std_port_outd(PCI_CONF_ADDR, reg);
    std_port_outd(PCI_CONF_DATA,val);
    std_port_outd(PCI_CONF_ADDR, 0);
}

static unsigned long
readPciCfg2(unsigned long reg)
{
    unsigned long val;
    
    unsigned char bus = (reg >> 16) & 0xff;
    unsigned char dev = (reg >> 11) & 0x1f;
    unsigned char num = reg & 0xff;
    
    std_port_outb(PCI_MODE2_ENABLE_REG, 0xF1);
    std_port_outb(PCI_MODE2_FORWARD_REG, bus); 
    val = std_port_ind((dev << 8) + num);
    std_port_outb(PCI_MODE2_ENABLE_REG, 0x00);
    Z_printf("PCIBIOS: reading 0x%lx from 0x%lx\n",val,reg);
    return val;
}

static void
writePciCfg2(unsigned long reg, unsigned long val)
{
    unsigned char bus = (reg >> 16) & 0xff;
    unsigned char dev = (reg >> 11) & 0x1f;
    unsigned char num = reg & 0xff;

    Z_printf("PCIBIOS writing: 0x%lx to 0x%lx\n",val,reg);
    std_port_outb(PCI_MODE2_ENABLE_REG, 0xF1);
    std_port_outb(PCI_MODE2_FORWARD_REG, bus); 
    std_port_outd((dev << 8) + num,val);
    std_port_outb(PCI_MODE2_ENABLE_REG, 0x00);
}

static void
write_dword(unsigned short loc,unsigned short reg,unsigned long val)
{
    unsigned long reg32 = loc << 8 | (reg & 0xfc) | PCI_EN;
    writePci(reg32, val);
}

static void
write_word(unsigned short loc,unsigned short reg,unsigned short word)
{
    unsigned long val;
    unsigned long reg32 = loc << 8 | (reg & 0xfc) | PCI_EN;
    int shift = reg & 0x2;

    val = readPci(reg32);
    val &= ~(unsigned long)(0xffff << (shift << 3));
    val |= word << (shift << 3);
    writePci(reg32, val);

}

static void
write_byte(unsigned short loc,unsigned short reg,unsigned char byte)
{
    unsigned long val;
    unsigned long reg32 = loc << 8 | (reg & 0xfc) | PCI_EN;
    int shift = reg & 0x3;

    val = readPci(reg32);
    val &= ~(unsigned long)(0xff << shift);
    val |= byte << (shift << 3);
    writePci(reg32, val);
}

static unsigned long
read_dword(unsigned short loc,unsigned short reg)
{
    unsigned long val;
    unsigned long reg32 = loc << 8 | (reg & 0xfc) | PCI_EN;

    val = readPci(reg32);
    return val;
}

static unsigned short
read_word(unsigned short loc,unsigned short reg)
{
    unsigned short val;
    int shift = reg & 0x2;
    unsigned long reg32 = loc << 8 | (reg & 0xfc) | PCI_EN;

    val = (readPci(reg32) >> (shift << 3)) & 0xffff;
    return val;
}

static unsigned char
read_byte(unsigned short loc,unsigned short reg)
{
    unsigned char val;
    int shift = reg & 0x3;
    unsigned long reg32 = loc << 8 | (reg & 0xfc) | PCI_EN;

    val = (readPci(reg32) >> (shift << 3)) & 0xff;
    return val;
}



static int
interpretCfgSpace(unsigned long *pciheader,unsigned long *pcibuses,int busidx,
		   unsigned char dev, unsigned char func)
{
    int tmp;
    
    pciPtr pciTmp = (pciPtr)malloc(sizeof(pciRec));
    pciTmp->next = pciList;
    pciList = pciTmp;
    pciTmp->bdf = pcibuses[busidx] << 8 | dev << 3 | func;
    pciTmp->vendor = pciheader[0] & 0xffff;
    pciTmp->device = pciheader[0] >> 16;
    pciTmp->class = pciheader[0x02] >> 8;
    if (PCI_BRIDGE_CLASS(pciTmp->class))  {
	if (PCI_BRIDGE_PCI_CLASS(pciTmp->class)) { /*PCI-PCI*/
	    Z_printf("PCI-PCI bridge:\n");
	    tmp = (pciheader[0x6] >> 8) & 0xff;
	    if (tmp > 0)
		pcibuses[++numbus] = tmp; /* secondary bus */
	} else if (PCI_BRIDGE_HOST_CLASS(pciTmp->class)) {
	    Z_printf("PCI-HOST bridge:\n");
	    if (++hostbridges > 1) {/* HOST-PCI bridges*/
		numbus++;
		pcibuses[numbus] = numbus;
	    }
	}
    }
    Z_printf("bus:%li dev:%i func:%i vend:0x%x dev:0x%x"
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

