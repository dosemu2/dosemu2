/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* memory.h - general constants/functions for memory, addresses, etc.
 *    for Linux DOS emulator, Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MEMORY_H
#define MEMORY_H

#if 0
#ifdef __cplusplus
#define asmlinkage extern "C"
#else
#define asmlinkage
#endif
#endif

#ifdef __ELF__
#ifdef __STDC__
#define CISH_INLINE(x) #x
#define CISH(x) x
#else
#define CISH_INLINE(x) "x"
#define CISH(x) x
#endif
#else
# error "Sorry, a.out format is no longer supported"
#endif

/* split segment 0xf000 into two region, 0xf0000 to 0xf7fff is read-write */
/*                                       0xf8000 to 0xfffff is read-only  */
/* so put anything need read-write into BIOSSEG and anything read-only */
/* to ROMBIOSSEG  */

#ifndef BIOSSEG
#define BIOSSEG		0xf000
#endif

#define INT_OFF(i) (0xc000+(i))

#define ROM_BIOS_SELFTEST	0xe05b
#define ROM_BIOS_EXIT		0xe2b0

#define INT09_SEG	BIOSSEG
#define INT09_OFF	0xe987		/* for 100% IBM compatibility */
#define INT09_ADD	((INT09_SEG << 4) + INT09_OFF)

#define Pause_SEG	(BIOSSEG)
#define Pause_OFF	0xf110

/* The packet driver has some code in this segment which needs to be */
/* at BIOSSEG.  therefore use BIOSSEG and compensate for the offset. */
/* Memory required is about 2000 bytes, beware! */
#define PKTDRV_SEG	(BIOSSEG)
#define PKTDRV_OFF	0xf130
#define PKTDRV_ADD	((PKTDRV_SEG << 4) + PKTDRV_OFF)

#define LFN_HELPER_SEG	BIOSSEG
#define LFN_HELPER_OFF	0xf230
#define LFN_HELPER_ADD	((LFN_HELPER_SEG << 4) + LFN_HELPER_OFF)

/* don't change these for now, they're hardwired! */
#define Mouse_SEG       (BIOSSEG-1)
#define Mouse_OFF       (0xe2d0+0x10)
#define Mouse_PS2_OFF   (0xe2d8+0x10)
#define Mouse_ROUTINE_OFF  (0xe2e0+0x10)
#define Mouse_HLT_OFF   (0xe2ff+0x10)
#define Mouse_INT_OFF	(INT_OFF(0x33) + 0x10)
#define Mouse_ADD      ((Mouse_SEG << 4)+Mouse_OFF)
#define Mouse_ROUTINE  ((Mouse_SEG << 4)+Mouse_ROUTINE_OFF)
#define Mouse_HLT_ADD  ((Mouse_SEG << 4)+Mouse_HLT_OFF)

#define EOI_OFF         0xf100

/* intercept-stub for dosdebugger (catches INT21/AX=4B00 */
#define DBGload_SEG BIOSSEG
#define DBGload_OFF 0xf330

#define DOSEMU_LMHEAP_SEG  BIOSSEG
#define DOSEMU_LMHEAP_OFF  0x4000
#define DOSEMU_LMHEAP_SIZE 0x8000

#ifndef ROMBIOSSEG
#define ROMBIOSSEG	0xf800
#endif

#define IRET_SEG	ROMBIOSSEG
#define IRET_OFF	0x62cf

#define XMSControl_SEG  ROMBIOSSEG
#define XMSControl_OFF  0x4150
#define XMSControl_ADD  ((XMSControl_SEG << 4)+XMSControl_OFF)

/* EMS origin must be at 0 */
#define EMS_SEG		(ROMBIOSSEG+0x100)
#define EMS_OFF		0x0000
#define EMS_ADD		((EMS_SEG << 4) + EMS_OFF)

#define EMM_BASE_ADDRESS        ((unsigned char *)SEG2LINEAR(config.ems_frame))
#define EMM_SEGMENT             (config.ems_frame)

#define INT16_SEG	ROMBIOSSEG
#define INT16_OFF	0x682e
#define INT16_ADD	((INT16_SEG << 4) + INT16_OFF)

#define IPX_SEG		ROMBIOSSEG
#define IPX_OFF		0x6310
#define IPX_ADD		((IPX_SEG << 4) + IPX_OFF)

#define INT08_SEG	ROMBIOSSEG
#define INT08_OFF	0x7ea5
#define INT08_ADD	((INT08_SEG << 4) + INT08_OFF)

#define INT70_SEG	ROMBIOSSEG
#define INT70_OFF	0x63f0
#define INT70_ADD	((INT70_SEG << 4) + INT70_OFF)

#define INT1E_SEG	ROMBIOSSEG
#define INT1E_OFF	0x6fc7
#define INT41_SEG	ROMBIOSSEG
#define INT41_OFF	0x6401
#define INT46_SEG	ROMBIOSSEG
#define INT46_OFF	0x6420

/* int10 watcher for mouse support */
/* This was in BIOSSEG (a) so we could write old_int10,
 * when it made a difference...
 */
#define INT10_WATCHER_SEG	ROMBIOSSEG
#define INT10_WATCHER_OFF	0x6330
#ifdef X86_EMULATOR
#define CPUEMU_WATCHER_SEG	ROMBIOSSEG
#define CPUEMU_WATCHER_OFF	0x6390
#define CPUEMUI10_ADD		((CPUEMU_WATCHER_SEG << 4) +\
				  CPUEMU_WATCHER_OFF + 11)
#endif

/* This inline interrupt is used for FCB open calls */
#define INTE7_SEG	ROMBIOSSEG
#define INTE7_OFF	0x6320
#define INTE7_ADD	((INTE7_SEG << 4) + INTE7_OFF)

#define PIC_SEG       ROMBIOSSEG
#define PIC_OFF       0x47ff
#define PIC_ADD       ((PIC_SEG << 4) + PIC_OFF)

#define CBACK_SEG	ROMBIOSSEG	/* callback return to dosemu */
#define CBACK_OFF	0x63ef
#define CBACK_ADD	((CBACK_SEG << 4) + CBACK_OFF)

#define DPMI_SEG	ROMBIOSSEG
#define DPMI_OFF	0x4800		/* need at least 512 bytes */
#define DPMI_ADD	((DPMI_SEG << 4) + DPMI_OFF)

#define DOS_LONG_READ_SEG ROMBIOSSEG
#define DOS_LONG_READ_OFF 0x4B00
#define DOS_LONG_WRITE_SEG ROMBIOSSEG
#define DOS_LONG_WRITE_OFF 0x4BA0

/* For int15 0xc0 */
#define ROM_CONFIG_SEG  BIOSSEG
#define ROM_CONFIG_OFF  0xe6f5
#define ROM_CONFIG_ADD	((ROM_CONFIG_SEG << 4) + ROM_CONFIG_OFF)

/*
 * HLT block
 */
#define BIOS_HLT_BLK       0xfc000
#define BIOS_HLT_BLK_SIZE  0x00800

#define VBIOS_START	((unsigned char *)SEG2LINEAR(config.vbios_seg))
/*#define VBIOS_SIZE	(64*1024)*/
#define VBIOS_SIZE	(config.vbios_size)
#define GFX_CHARS	0xffa6e
#define GFXCHAR_SIZE	1400

/* Memory adresses for all common video adapters */

#define MDA_PHYS_TEXT_BASE  0xB0000
#define MDA_VIRT_TEXT_BASE  ((void *)0xB0000)

#define CGA_PHYS_TEXT_BASE  0xB8000
#define CGA_VIRT_TEXT_BASE  ((void *)0xB8000)

#define EGA_PHYS_TEXT_BASE  0xB8000
#define EGA_VIRT_TEXT_BASE  ((void *)0xB8000)

#define VGA_PHYS_TEXT_BASE  0xB8000
#define VGA_VIRT_TEXT_BASE  ((void *)0xB8000)

#define CO      80 /* A-typical screen width */
#define LI      25 /* Normal rows on a screen */
#define TEXT_SIZE(co,li) (((co*li*2)+4095)&(~4095)) /* 4000(4096?) text page size */

#define GRAPH_BASE 0xA0000
#define GRAPH_SIZE 0x20000

#define BIOS_DATA_SEG   (0x400)	/* for absolute adressing */
#define BIOS_DATA_PTR(off) ((void *) (BIOS_DATA_SEG + off))

/* Bios data area keybuffer */
#if 0
#define KBD_Start (*(unsigned short *)BIOS_DATA_PTR(0x80))
#define KBD_End  (*(unsigned short *)BIOS_DATA_PTR(0x82))
#define KBD_Head (*(unsigned short *)BIOS_DATA_PTR(0x1A))
#define KBD_Tail (*(unsigned short *)BIOS_DATA_PTR(0x1C))
#endif

#define KBD_START BIOS_DATA_PTR(0x80)
#define KBD_END   BIOS_DATA_PTR(0x82)
#define KBD_HEAD  BIOS_DATA_PTR(0x1A)
#define KBD_TAIL  BIOS_DATA_PTR(0x1C)

#define KBDFLAG_ADDR (unsigned short *)BIOS_DATA_PTR(0x17)
#define KEYFLAG_ADDR (unsigned short *)BIOS_DATA_PTR(0x96)

/* Correct HMA size is 64*1024 - 16, but IPC seems not to like this
   hence I would consider that those 16 missed bytes get swapped back
   and forth and may cause us grief - a BUG */
#define HMASIZE (64*1024)
#define LOWMEM_SIZE 0x100000
#define EXTMEM_SIZE ((unsigned)(config.ext_mem << 10))

#ifndef __ASSEMBLER__

#include "types.h"

/* memcheck memory conflict finder definitions */
int  memcheck_addtype(unsigned char map_char, char *name);
void memcheck_reserve(unsigned char map_char, size_t addr_start, size_t size);
void memcheck_init(void);
int  memcheck_isfree(size_t addr_start, size_t size);
int  memcheck_findhole(size_t *start_addr, size_t min_size, size_t max_size);
void memcheck_dump(void);
void memcheck_type_init(void);
extern struct system_memory_map *system_memory_map;
extern size_t system_memory_map_size;
void *dosaddr_to_unixaddr(void *addr);

/* lowmem_base points to a shared memory image of the area 0--1MB+64K.
   It does not have any holes or mapping for video RAM etc.
   The difference is that the mirror image is not read or write protected so
   DOSEMU writes will not be trapped. This allows easy interference with
   simx86, NULL page protection, and removal of the VGA protected memory
   access hack.

   It is set "const" to help GCC optimize accesses. In reality it is set only
   once, at startup
*/
extern char * const lowmem_base;

#define UNIX_READ_BYTE(addr)		(*(Bit8u *) (addr))
#define UNIX_WRITE_BYTE(addr, val)	(*(Bit8u *) (addr) = (val) )
#define UNIX_READ_WORD(addr)		(*(Bit16u *) (addr))
#define UNIX_WRITE_WORD(addr, val)	(*(Bit16u *) (addr) = (val) )
#define UNIX_READ_DWORD(addr)		(*(Bit32u *) (addr))
#define UNIX_WRITE_DWORD(addr, val)	(*(Bit32u *) (addr) = (val) )

#define LOWMEM(addr) ((void *)((uintptr_t)(addr) + lowmem_base))

#define LOWMEM_READ_BYTE(addr)		UNIX_READ_BYTE(LOWMEM(addr))
#define LOWMEM_WRITE_BYTE(addr, val)	UNIX_WRITE_BYTE(LOWMEM(addr), val)
#define LOWMEM_READ_WORD(addr)		UNIX_READ_WORD(LOWMEM(addr))
#define LOWMEM_WRITE_WORD(addr, val)	UNIX_WRITE_WORD(LOWMEM(addr), val)
#define LOWMEM_READ_DWORD(addr)		UNIX_READ_DWORD(LOWMEM(addr))
#define LOWMEM_WRITE_DWORD(addr, val)	UNIX_WRITE_DWORD(LOWMEM(addr), val)

/* generic lowmem addresses are the ones below 1Mb, that are
 * not in the EMS frame, hardware or video memory. We can _safely_
 * add lowmem_base to those. */
#define IS_GENERIC_LOWMEM_ADDR(addr) \
	 ((uintptr_t)(addr) <= 0x9fffc || \
	 ((uintptr_t)(addr) >= 0xf4000 && (uintptr_t)(addr) <= 0xffffc))

#define LINEAR2UNIX(addr) \
	(IS_GENERIC_LOWMEM_ADDR(addr) ? LOWMEM(addr) : \
	 dosaddr_to_unixaddr((void *)(uintptr_t)(addr)))

#define READ_BYTE(addr)		UNIX_READ_BYTE(LINEAR2UNIX(addr))
#define WRITE_BYTE(addr, val)	UNIX_WRITE_BYTE(LINEAR2UNIX(addr), val)
#define READ_WORD(addr)		UNIX_READ_WORD(LINEAR2UNIX(addr))
#define WRITE_WORD(addr, val)	UNIX_WRITE_WORD(LINEAR2UNIX(addr), val)
#define READ_DWORD(addr)	UNIX_READ_DWORD(LINEAR2UNIX(addr))
#define WRITE_DWORD(addr, val)	UNIX_WRITE_DWORD(LINEAR2UNIX(addr), val)

#define MEMCPY_2UNIX(unix_addr, dos_addr, n) \
	memcpy((unix_addr), LINEAR2UNIX(dos_addr), (n))

#define MEMCPY_2DOS(dos_addr, unix_addr, n) \
	memcpy(LINEAR2UNIX(dos_addr), (unix_addr), (n))

#define MEMCPY_DOS2DOS(dos_addr1, dos_addr2, n) \
	memcpy(LINEAR2UNIX(dos_addr1), LINEAR2UNIX(dos_addr2), (n))

#define MEMMOVE_DOS2DOS(dos_addr1, dos_addr2, n) \
        memmove(LINEAR2UNIX(dos_addr1), LINEAR2UNIX(dos_addr2), (n))

#define MEMCMP_DOS_VS_UNIX(dos_addr, unix_addr, n) \
	memcmp(LINEAR2UNIX(dos_addr), (unix_addr), (n))

#define MEMSET_DOS(dos_addr, val, n) \
        memset(LINEAR2UNIX(dos_addr), (val), (n))

#endif

#endif /* MEMORY_H */
