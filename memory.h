/* memory.h - general constants/functions for memory, addresses, etc.
 *    for Linux DOS emulator, Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MEMORY_H
#define MEMORY_H

#define BIOSSEG		0xf000

#define XMSControl_SEG  BIOSSEG
#define XMSControl_OFF  0x1000
#define XMSControl_ADD  ((XMSControl_SEG << 4)+XMSControl_OFF)
#define XMSTrap_ADD     ((XMSControl_SEG << 4)+XMSControl_OFF+5)

/* EMS origin must be at 0 */
#define EMS_SEG		(BIOSSEG+0x200)
#define EMS_OFF		0x0000
#define EMS_ADD		((EMS_SEG << 4) + EMS_OFF)

#define Banner_SEG	BIOSSEG
#define Banner_OFF	0x3000
#define Banner_ADD	((Banner_SEG << 4) + Banner_OFF)

#define INT16_SEG	BIOSSEG
#define INT16_OFF	0x4000
#define INT16_ADD	((INT16_SEG << 4) + INT16_OFF)

/* raw console stuff */
#ifdef MDA_VIDEO
#define PHYS_TEXT_BASE  0xB0000
#define VIRT_TEXT_BASE	0xB0000
#else
#define PHYS_TEXT_BASE	0xB8000
#define VIRT_TEXT_BASE	0xB8000
#endif

#define VBIOS_START	0xc0000
#define VBIOS_SIZE	(64*1024)

#define GFX_CHARS	0xffa6e
#define GFXCHAR_SIZE	1400

#define TEXT_SIZE	4096         /* text page size */
#define PAGE_ADDR(pg)	(caddr_t)(VIRT_TEXT_BASE + (pg*TEXT_SIZE))
#define SCREEN_ADR(s)	(us *)(VIRT_TEXT_BASE + (s*TEXT_SIZE))

#define GRAPH_BASE 0xA0000
#define GRAPH_SIZE 0x20000

/* Bios data area 16-key (32byte) keybuffer */
#define KBDA_ADDR	(unsigned short *)0x41e
#define KBDFLAG_ADDR	(unsigned short *)0x417
#define KEYFLAG_ADDR	(unsigned short *)0x496

#endif MEMORY_H
