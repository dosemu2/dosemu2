/* memory.h - general constants/functions for memory, addresses, etc.
 *    for Linux DOS emulator, Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MEMORY_H
#define MEMORY_H

#ifndef BIOSSEG
#define BIOSSEG		0xf000
#endif

#define XMSControl_SEG  BIOSSEG
#define XMSControl_OFF  0x1000
#define XMSControl_ADD  ((XMSControl_SEG << 4)+XMSControl_OFF)
#define XMSTrap_ADD     ((XMSControl_SEG << 4)+XMSControl_OFF+5)

/* EMS origin must be at 0 */
#define EMS_SEG		(BIOSSEG+0x200)
#define EMS_OFF		0x0000
#define EMS_ADD		((EMS_SEG << 4) + EMS_OFF)

#define EMM_BASE_ADDRESS        (config.ems_frame << 4)
#define EMM_SEGMENT             (config.ems_frame)


#define Banner_SEG	BIOSSEG
#define Banner_OFF	0x3000
#define Banner_ADD	((Banner_SEG << 4) + Banner_OFF)

#define OUTB_SEG	(BIOSSEG + 0x100)
#define OUTB_OFF	0x20f0
#define OUTB_ADD	(u_short *)((OUTB_SEG << 4) + OUTB_OFF)

#define LASTSCAN_SEG	(BIOSSEG + 0x100)
#define LASTSCAN_OFF	0x20f2
#define LASTSCAN_ADD	(u_short *)((LASTSCAN_SEG << 4) + LASTSCAN_OFF)

#define IPX_SEG		BIOSSEG
#define IPX_OFF		0x3100
#define IPX_ADD		((IPX_SEG << 4) + IPX_OFF)

#define INT16_SEG	(BIOSSEG + 0x100)
#define INT16_OFF	0x3000
#define INT16_ADD	((INT16_SEG << 4) + INT16_OFF)

#define INT08_SEG	(BIOSSEG + 0x100)
#define INT08_OFF	0x4000
#define INT08_ADD	((INT08_SEG << 4) + INT08_OFF)

#if 0
#define INT09_SEG	(BIOSSEG + 0x100)
#define INT09_OFF	0xD987
#else
#define INT09_SEG	BIOSSEG
#define INT09_OFF	0xe987		/* for 100% IBM compatibility */
#endif
#define INT09_ADD	((INT09_SEG << 4) + INT09_OFF)

#define INT10_SEG	(BIOSSEG + 0x100)
#define INT10_OFF	0x4200
#define INT10_ADD	((INT10_SEG << 4) + INT10_OFF)

/* This inline interrupt is used for FCB open calls */
#define INTE7_SEG	(BIOSSEG + 0x100)
#define INTE7_OFF	0x4500
#define INTE7_ADD	((INTE7_SEG << 4) + INTE7_OFF)

#define DPMI_SEG	(BIOSSEG + 0x100)
#define DPMI_OFF	0x4800		/* need at least 512 bytes */
#define DPMI_ADD	((DPMI_SEG << 4) + DPMI_OFF)

/* The packet driver has some code in this segment which needs to be */
/* at BIOSSEG.  therefore use BIOSSEG and compensate for the offset. */
/* Memory required is about 2000 bytes, beware! */
#define PKTDRV_SEG	BIOSSEG
#define PKTDRV_OFF	(0x5100 + (0x100 << 4))
#define PKTDRV_ADD	((PKTDRV_SEG << 4) + PKTDRV_OFF)

/* For int15 0xc0 */
#define ROM_CONFIG_SEG  BIOSSEG
#define ROM_CONFIG_OFF  0xe6f5
#define ROM_CONFIG_ADD	((ROM_CONFIG_SEG << 4) + ROM_CONFIG_OFF)

#define VBIOS_START	(config.vbios_seg << 4 )
/*#define VBIOS_SIZE	(64*1024)*/
#define VBIOS_SIZE	(config.vbios_size)
#define GFX_CHARS	0xffa6e
#define GFXCHAR_SIZE	1400

/* Memory adresses for all common video adapters */

#define MDA_PHYS_TEXT_BASE  0xB0000
#define MDA_VIRT_TEXT_BASE  0xB0000

#define CGA_PHYS_TEXT_BASE  0xB8000
#define CGA_VIRT_TEXT_BASE  0xB8000

#define EGA_PHYS_TEXT_BASE  0xB8000
#define EGA_VIRT_TEXT_BASE  0xB8000

#define VGA_PHYS_TEXT_BASE  0xB8000
#define VGA_VIRT_TEXT_BASE  0xB8000

#define TEXT_SIZE	4096	/* text page size */
#define PAGE_ADDR(pg)	(caddr_t)(virt_text_base + (pg*TEXT_SIZE))
#define SCREEN_ADR(s)	((us *)(virt_text_base + (s*TEXT_SIZE)))

#define GRAPH_BASE 0xA0000
#define GRAPH_SIZE 0x20000

#define BIOS_DATA_SEG   (0x400)	/* for absolute adressing */
#define BIOS_DATA_PTR(off) ((void *) (BIOS_DATA_SEG + off))

/* Bios data area keybuffer */
#define KBD_Start (*(unsigned short *)BIOS_DATA_PTR(0x80))
#define KBD_End  (*(unsigned short *)BIOS_DATA_PTR(0x82))
#define KBD_Head (*(unsigned short *)BIOS_DATA_PTR(0x1A))
#define KBD_Tail (*(unsigned short *)BIOS_DATA_PTR(0x1C))

#define KBDFLAG_ADDR (unsigned short *)BIOS_DATA_PTR(0x17)
#define KEYFLAG_ADDR (unsigned short *)BIOS_DATA_PTR(0x96)

#endif /* MEMORY_H */
