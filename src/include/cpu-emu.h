#ifndef DOSEMU_CPUEMU_H
#define DOSEMU_CPUEMU_H

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32

extern unsigned long	io_bitmap[IO_BITMAP_SIZE+1];

#if 1
#define  DONT_START_EMU
#endif

#if 0
#define TRACE_HIGH	(SHORT_CS_16!=0xc000)
#endif
#if 1
#define TRACE_HIGH	((SHORT_CS_16<0xa000)|| \
			 ((SHORT_CS_16>=0xe000)&&(SHORT_CS_16<=0xf800)))
#endif

#if 0
#define VT_EMU_ONLY
#endif

#endif	/*DOSEMU_CPUEMU_H*/
