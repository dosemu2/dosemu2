/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DOSEMU_PCI_H
#define DOSEMU_PCI_H

#include <sys/pci.h>

#define PCIBIOS_PCI_FUNCTION_ID 	0xb1
#define PCIBIOS_PCI_BIOS_PRESENT 	0xb101
#define PCIBIOS_FIND_PCI_DEVICE		0xb102
#define PCIBIOS_FIND_PCI_CLASS_CODE	0xb103
#define PCIBIOS_GENERATE_SPECIAL_CYCLE	0xb106
#define PCIBIOS_READ_CONFIG_BYTE	0xb108
#define PCIBIOS_READ_CONFIG_WORD	0xb109
#define PCIBIOS_READ_CONFIG_DWORD	0xb10a
#define PCIBIOS_WRITE_CONFIG_BYTE	0xb10b
#define PCIBIOS_WRITE_CONFIG_WORD	0xb10c
#define PCIBIOS_WRITE_CONFIG_DWORD	0xb10d
#define PCIBIOS_GET_IRQ_ROUTING_OPTIONS 0xb10e
#define PCIBIOS_SET_PCI_IRQ             0xb10f

#define PCIBIOS_STAT_SUCCESSFUL     0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED  0x81
#define PCIBIOS_BAD_VENDOR_ID       0x83
#define PCIBIOS_DEVICE_NOT_FOUND    0x86
#define PCIBIOS_BAD_REGISTER_NUMBER 0x87
#define PCIBIOS_SET_FAILED          0x88
#define PCIBIOS_BUFFER_TOO_SMALL    0x89

/* BIOS32 signature: "_32_" */
#define BIOS32_SIGNATURE	(('_' << 0) + ('3' << 8) + ('2' << 16) + ('_' << 24))

/* PCI signature: "PCI " */
#define PCI_SIGNATURE		(('P' << 0) + ('C' << 8) + ('I' << 16) + (' ' << 24))

/* PCI service signature: "$PCI" */
#define PCI_SERVICE		(('$' << 0) + ('P' << 8) + ('C' << 16) + ('I' << 24))

/* Configuration mechanism 1 - two 32-bit I/O ports */

#define PCI_CONF_ADDR	0xcf8
#define PCI_CONF_DATA	0xcfc
#define PCI_EN 0x80000000

#define PCI_MODE2_FORWARD_REG 0xCFA
#define PCI_MODE2_ENABLE_REG 0xCF8

void pci_bios(void);
int pcibios_init(void);

int pci_check_conf(void);

int pci_check_device_present_cfg1(unsigned char bus, unsigned char device,
			     unsigned char fn);

int pci_read_header_cfg1 (unsigned char bus, unsigned char device,
	unsigned char fn, unsigned long *buf);

int pci_check_device_present_cfg2(unsigned char bus, unsigned char device);

int pci_read_header_cfg2 (unsigned char bus, unsigned char device,
			  unsigned char fn, unsigned long *buf);

extern int (*pci_read_header) (unsigned char bus, unsigned char device,
			       unsigned char fn, unsigned long *buf);
int pci_setup (void);

#endif /* DOSEMU_PCI_H */
