/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DOSEMU_CPUEMU_H
#define DOSEMU_CPUEMU_H

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32

extern unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
extern void e_priv_iopl(int);

/* generally DOS uses high memory from 0x9xxx to 0x9fff - please fine
 * tune for your specific version, or set it to 0xa000 to trace into DOS
 */
#define DOS_HI_LIMIT	0x9500

/* ------------------------------------------------------------------------
 * if this is defined, cpuemu will only start when explicitly activated
 * by int0xe6. Technically: if we want to trace DOS boot we undefine this
 * and config.cpuemu will be set at 3 from the start - else it is set at 1
 */
#if 1
#define DONT_START_EMU
#endif

/* define this to skip emulation through video BIOS. This speeds up a lot
 * the cpuemu and avoids possible time-dependent code in the VBIOS. It has
 * a drawback: when we jump into REAL vm86 in the VBIOS, we never know
 * where we will exit back to the cpuemu. Oh, well, speed is what matters...
 */
#if 1
#define	SKIP_EMU_VBIOS
#endif

/*
 * other settings for intp32:
 *  EMU_GLOBAL_VAR - let's try to use absolute memory references to free
 *	some more registers. Cpuemu will run a bit slower but we should
 *	avoid gcc register allocation problems.
 */
#if 0
#define	EMU_GLOBAL_VAR
#endif

/*
 * Reserve a specific register globally for the CPU interpreter's
 * Program Counter. This is a __i386__ only thing and reserves EBP
 * so don't remove -fomit-frame-pointer from the CFLAGS !!
 */
#if 1
#define USE_GLOBAL_EBP
#endif
  

/*  EMU_STAT - collect opcode information and display sorted stat of
 *	opcode frequency when exiting cpuemu
 *  EMU_PROFILE (only if EMU_STAT defined) - get info about mean exec
 *	time of opcodes
 */
#if 0
#define	EMU_STAT
#define EMU_PROFILE
#endif

/*  DONT_DEBUG_BOOT (in emu.h) - if you set that, debug messages are
 *	skipped until the cpuemu is activated by int0xe6, so you can
 *	avoid the long DOS intro when using high debug values.
 *  CIRCULAR_LOGBUFFER (utilities.c) - another way to reduce the size of
 *	the debug logfile by keeping only the last n lines (n=~20000)
 */
#if 0
#define	CIRCULAR_LOGBUFFER
#endif

/* ----------------------------------------------------------------------- */

/* cycles/instr is an empirical estimate of how my K6 system
 * really performs, including CPU, caches and main memory */
#define CYCperINS	4

/* no one will ever use selectors so big... so this will trap only real
 * mode addresses...
 */
#define TRACE_HIGH	((SHORT_CS_16<DOS_HI_LIMIT)||(SHORT_CS_16>=0xd000))

/* Cpuemu status register - pack as much info as possible here, so to
 * use a single test to check if we have to go further or not */
#define CeS_SIGPEND	0x01	/* signal pending mask */
#define CeS_SIGACT	0x02	/* signal active mask */
#define CeS_MODE_RM	0x04	/* true real mode */
#define CeS_MODE_VM86	0x08	/* standard VM86 */
#define CeS_MODE_PM16	0x10	/* 16-bit protected */
#define CeS_MODE_PM32	0x20	/* 32-bit protected */
#define CeS_MODE_DPMI	0x40	/* DPMI active */
#define CeS_MODE_MASK	0x7c
#define CeS_LOCK	0x800	/* cycle lock (pop ss; pop sp et sim.) */
#define CeS_TRAP	0x1000	/* INT01 Sstep active */
#define CeS_DRTRAP	0x2000	/* Debug Registers active */

extern int CEmuStat;

#define VM86F		(CEmuStat & CeS_MODE_VM86)
#define MODE_VM86	CeS_MODE_VM86
#define MODE_DPMI16	(CeS_MODE_PM16|CeS_MODE_DPMI)
#define MODE_DPMI32	(CeS_MODE_PM32|CeS_MODE_DPMI)

/* these registers are defined in cpu.c */
extern unsigned long CRs[5];
extern unsigned long DRs[8];
extern unsigned long TRs[2];

#endif	/*DOSEMU_CPUEMU_H*/
