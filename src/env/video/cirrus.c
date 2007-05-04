/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * video/cirrus.c
 * $XConsortium: cir_driver.c,v 1.5 95/01/23 15:35:14 kaleb Exp $ *
 * $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/cirrus/cir_driver.c,v 3.17 1995/06/02 11:19:47 dawes Exp $ *
 *
 * This file contains support for VGA-cards with a Cirrus chip.
 *
 */

#define CIRRUS_C

#include <stdio.h>
#include <unistd.h>

#include "emu.h"        /* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "cirrus.h"

static int vgaIOBase = 0x3D0;

static int cirrusChip;
static int cirrusChipRevision;
/*static int cirrusBusType;*/
static int cirrus_memsize = 0;
static int cirrus_8514_base = BASE_8514_1;

#if 0
static char cirrusUseBLTEngine = 0;
static char cirrusUseMMIO = 0;
static char cirrusMMIOFlag = 0;
static unsigned char *cirrusMMIOBase = NULL;
static char cirrusUseLinear = 0;
static char cirrusFavourBLT = 0;
static char cirrusAvoidImageBLT = 0;
static int cirrusDRAMBandwidth;
static int cirrusDRAMBandwidthLimit;
static int cirrusReprogrammedMCLK = 0;
#endif

#define CLAVGA2_ID  0x06
#define CLGD5420_ID 0x22
#define CLGD5422_ID 0x23
#define CLGD5424_ID 0x25
#define CLGD5426_ID 0x24
#define CLGD5428_ID 0x26
#define CLGD5429_ID 0x27
#define CLGD6205_ID 0x02
#define CLGD6215_ID 0x22
#define CLGD6225_ID 0x32
#define CLGD6235_ID 0x06
#define CLGD5434_OLD_ID 0x29
#define CLGD5434_ID 0x2A
#define CLGD5430_ID 0x28
#define CLGD5436_ID 0x2B

#define Is_62x5(x)  ((x) >= CLGD6205 && (x) <= CLGD6235)

#define Has_HWCursor(x) (((x) >= CLGD5422 && (x) <= CLGD5429) || \
    (x) == CLGD5430 || (x) == CLGD5434 || (x) == CLGD5436)

/*      Some information on the accelerated features of the 5426,
   derived from the Databook. [from svgalib]

   port index

   Addresses have 21 bits (2Mb of memory).
   0x3ce,       0x28    bits 0-7 of the destination address
   0x3ce,  0x29 bits 8-15
   0x3ce,       0x2a    bits 16-20

   0x3ce,       0x2c    bits 0-7 of the source address
   0x3ce,       0x2d    bits 8-15
   0x3ce,       0x2e    bits 16-20

   Maximum pitch is 4095.
   0x3ce,       0x24    bits 0-7 of the destination pitch (screen width)
   0x3ce,       0x25    bits 8-11

   0x3ce,       0x26    bits 0-7 of the source pitch (screen width)
   0x3ce,       0x27    bits 8-11

   Maximum width is 2047.
   0x3ce,       0x20    bits 0-7 of the box width - 1
   0x3ce,       0x21    bits 8-10

   Maximum height is 1023.
   0x3ce,       0x22    bits 0-7 of the box height - 1
   0x3ce,       0x23    bits 8-9

   0x3ce,       0x30    BLT mode
   bit 0: direction (0 = down, 1 = up)
   bit 1: destination
   bit 2: source (0 = video memory, 1 = system memory)
   bit 3: enable transparency compare
   bit 4: 16-bit color expand/transparency
   bit 6: 8x8 pattern copy
   bit 7: enable color expand

   0x31 BLT status
   bit 0: busy
   bit 1: start operation (1)/suspend (0)
   bit 2: reset
   bit 3: set while blit busy/suspended

   0x32 BLT raster operation
   0x00 black
   0x01 white
   0x0d copy source
   0xd0 copy inverted source
   0x0b invert destination
   0x05 logical AND
   0x6d logical OR (paint)
   0x59 XOR

   0x34 BLT transparent color
   0x35 high byte

   0x3ce,  0x00 background color (for color expansion)
   0x3ce,  0x01 foreground color
   0x3ce,  0x10 high byte of background color for 16-bit pixels
   0x3ce,  0x11 high byte of foreground color

   0x3ce,       0x0b    bit 1: enable BY8 addressing
   0x3c4,       0x02    8-bit plane mask for BY8 (corresponds to 8 pixels)
   (write mode 1, 4, 5)
   0x3c5,  0x05 bits 0-2: VGA write mode
   extended write mode 4: write up to 8 pixels in
   foreground color (BY8)
   extended write mode 5: write 8 pixels in foreground/
   background color (BY8)
   This may also work in normal non-BY8 packed-pixel mode.

   When doing blits from system memory to video memory, pixel data
   can apparently be written to any video address in 16-bit words, with
   the each scanline padded to 4-byte alignment. This is handy because
   the chip handles line transitions and alignment automatically (and
   can do, for example, masking).

   The pattern copy requires an 8x8 pattern (64 pixels) at the source
   address in video memory, and fills a box with specified size and 
   destination address with the pattern. This is in fact the way to do
   solid fills.

   mode                 pattern
   Color Expansion              8 bytes (monochrome bitmap)
   8-bit pixels         64 bytes
   16-bit pixels                128 bytes

 */

static inline void wrinx(const ioport_t port, const int index, const u_char value)
{
	port_out(index, port);
	port_out(value, port+1);
}

static inline int rdinx(const ioport_t port, const int index)
{
	port_out(index, port);
	return (port_in(port+1) & 0xff);
}

#define out_crt(i,v)	wrinx(CRT_I,(i),(v))
#define in_crt(i)	rdinx(CRT_I,(i))

/*
 * cirrusEnterLeave -- 
 *      enable/disable io-mapping
 */
static void cirrusEnterLeave(char enter)
{
  static unsigned char temp;

  if (enter)
       {
	/* Are we Mono or Color? */
       vgaIOBase = (port_in(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;

       port_out(0x06,0x3C4);
       port_out(0x12,0x3C5);	 /* unlock cirrus special */
				 /* Put the Vert. Retrace End Reg in temp */

       port_out(0x11, vgaIOBase+4); temp = port_in(vgaIOBase + 5);

				/* Put it back with PR bit set to 0 */
				/* This unprotects the 0-7 CRTC regs so */
				/* they can be modified, i.e. we can set */
				/* the timing. */

       port_out(temp & 0x7F, vgaIOBase+5);

    }
  else
       {
       port_out(0x06,0x3C4);
       port_out(0x0f,0x3C5);	 /*relock cirrus special */
    }
}


/*
 * cirrusProbe -- 
 *      check up whether a cirrus based board is installed
 */
static int cirrusProbe(void)
{  
/*     int cirrusClockNo; */
     unsigned char lockreg,IdentVal;
     unsigned char id, rev, partstatus;
     unsigned char temp;
     unsigned char old;

     old = rdinx(0x3c4, 0x06);
     cirrusEnterLeave(1); /* Make the timing regs writable */
	  
     /* Kited the following from the Cirrus */
     /* Databook */
     /* If it's a Cirrus at all, we should be */
     /* able to read back the lock register */
     /* we wrote in cirrusEnterLeave() */
	  
     port_out(0x06,0x3C4);
     lockreg = port_in(0x3C5);
	  
     /* Ok, if it's not 0x12, we're not a Cirrus542X or 62x5. */
     if (lockreg != 0x12)
       {
         wrinx(0x3c4, 0x06, old);
         cirrusEnterLeave(0);
         return(0);
       }
	  
     /* OK, it's a Cirrus. Now, what kind of */
     /* Cirrus? We read in the ident reg, */
     /* CRTC index 27 */
	  
     port_out(0x27, vgaIOBase+4); IdentVal = port_in(vgaIOBase+5);
	  
     cirrusChip = -1;
     id  = (IdentVal & 0xFc) >> 2;
     rev = (IdentVal & 0x03);

     port_out(0x25, vgaIOBase+4); partstatus = port_in(vgaIOBase+5);
     cirrusChipRevision = 0x00;

     switch( id )
       {
	     case CLGD5420_ID:
	       cirrusChip = CLGD5420;	/* 5420 or 5402 */
	       /* Check for CL-GD5420-75QC-B */
	       /* It has a Hidden-DAC register. */
	       port_out(0x00, 0x3C6);
	       port_out(0xff, 0x3C6);
	       port_in(0x3C6); port_in(0x3c6); port_in(0x3C6); port_in(0x3C6);
	       if (port_in(0x3C6) != 0xFF)
	           cirrusChipRevision = 0x01;	/* 5420-75QC-B */
	       break;
	     case CLGD5422_ID:
	       cirrusChip = CLGD5422;
	       break;
	     case CLGD5424_ID:
	       cirrusChip = CLGD5424;
	       break;
	     case CLGD5426_ID:
	       cirrusChip = CLGD5426;
	       break;
	     case CLGD5428_ID:
	       cirrusChip = CLGD5428;
	       break;
	     case CLGD5429_ID:
	       if (partstatus >= 0x67)
	           cirrusChipRevision = 0x01;	/* >= Rev. B, fixes BLT */
	       cirrusChip = CLGD5429;
	       break;

	     /* 
	      * LCD driver chips...  the +1 options are because
	      * these chips have one more bit of chip rev level
	      */
	     case CLGD6205_ID:
	     case CLGD6205_ID + 1:
	       cirrusChip = CLGD6205;
	       break;
	     case CLGD6225_ID:
	     case CLGD6225_ID + 1:
	       cirrusChip = CLGD6225;
	       break;
	     case CLGD6235_ID:
	     case CLGD6235_ID + 1:
	       cirrusChip = CLGD6235;
	       break;

	     /* 'Alpine' family. */
	     case CLGD5434_ID:
	       if ((partstatus & 0xC0) == 0xC0) {
	          /*
	           * Better than Rev. ~D/E/F.
	           * Handles 60 MHz MCLK and 135 MHz VCLK.
	           */
	          cirrusChipRevision = 0x01;
/*	          cirrusClockLimit[CLGD5434] = 135100; */
	       }
	       else
	       if (partstatus == 0x8E)
	       	  /* Intermediate revision, supports 135 MHz VCLK. */
/*	          cirrusClockLimit[CLGD5434] = 135100; */
	       cirrusChip = CLGD5434;
	       break;

	     case CLGD5430_ID:
	       cirrusChip = CLGD5430;
	       break;

	     case CLGD5436_ID:
	       cirrusChip = CLGD5436;
	       break;

	     case CLGD5434_OLD_ID:

	     default:
	       v_printf("Unknown Cirrus chipset: type 0x%02x, rev %d\n", id, rev);
	       if (id == CLGD5434_OLD_ID)
	          v_printf("Old pre-production ID for clgd5434\n");
	       cirrusEnterLeave(0);
	       return(0);
	       break;
       }
	  
     if (cirrusChip == CLGD5430 || cirrusChip == CLGD5434 ||
         cirrusChip == CLGD5436) {
         /* Write sane value to Display Compression Control */
         /* Register, which may be corrupted by pvga1 driver */
         /* probe. */
         port_out(0x0f, 0x3ce);
         temp = port_in(0x3cf) & 0xc0;
         port_out(temp, 0x3cf);
       }
     
     /* OK, we are a Cirrus */
     v_printf("Cirrus chipset: type 0x%02x, rev %d\n", id, rev);

#if 0
     if (vgaBitsPerPixel == 16 &&
     (Is_62x5(cirrusChip) || cirrusChip == CLGD5420)) {
         v_printf("%s %s: %s: Cirrus 62x5 and 5420 chipsets not supported "
             "in 16bpp mode\n",
             XCONFIG_PROBED, vga256InfoRec.name, vga256InfoRec.chipset);
         CIRRUS.ChipHas16bpp = 0;
     }
#endif

     if (!cirrus_memsize) 
	  {
	  if (Is_62x5(cirrusChip)) 
	       {
	       /* 
		* According to Ed Strauss at Cirrus, the 62x5 has 512k.
		* That's it.  Period.
		*/
	         cirrus_memsize = 512;
	       }
	  else 
	  if (HAVE543X()) {
	  	/* The scratch register method does not work on the 543x. */
	  	/* Use the DRAM bandwidth bit and the DRAM bank switching */
	  	/* bit to figure out the amount of memory. */
	  	unsigned char SRF;
	  	cirrus_memsize = 512;
	  	port_out(0x0f, 0x3c4);
	  	SRF = port_in(0x3c5);
	  	if (SRF & 0x10)
	  		/* 32-bit DRAM bus. */
	  		cirrus_memsize *= 2;
	  	if ((SRF & 0x18) == 0x18)
	  		/* 64-bit DRAM data bus width; assume 2MB. */
	  		/* Also indicates 2MB memory on the 5430. */
	  		cirrus_memsize *= 2;
	  	if (cirrusChip != CLGD5430 && (SRF & 0x80))
	  		/* If DRAM bank switching is enabled, there */
	  		/* must be twice as much memory installed. */
	  		/* (4MB on the 5434) */
	  		cirrus_memsize *= 2;
	  }
	  else
	       {
	       unsigned char memreg;

		/* Thanks to Brad Hackenson at Cirrus for */
		/* this bit of undocumented black art....*/
	       port_out(0x0a, 0x3C4);
	       memreg = port_in(0x3C5);
	  
	       switch( (memreg & 0x18) >> 3 )
		    {
		  case 0:
		    cirrus_memsize = 256; break;
		  case 1:
		    cirrus_memsize = 512; break;
		  case 2:
		    cirrus_memsize = 1024; break;
		  case 3:
		    cirrus_memsize = 2048; break;
		    }

	       if (cirrusChip >= CLGD5422 && cirrusChip <= CLGD5429 &&
		    cirrus_memsize < 512) {
	       		/* Invalid amount for 542x -- scratch register may */
	       		/* not be set by some BIOSes. */
		  	unsigned char SRF;
		  	cirrus_memsize = 512;
	  		port_out(0x0f, 0x3c4);
		  	SRF = port_in(0x3c5);
		  	if (SRF & 0x10)
		  		/* 32-bit DRAM bus. */
		  		cirrus_memsize *= 2;
		  	if ((SRF & 0x18) == 0x18)
		  		/* 2MB memory on the 5426/8/9 (not sure). */
		  		cirrus_memsize *= 2;
		  	}
	       }
	  }

#if 0
     if (vgaBitsPerPixel == 32 &&
     ((cirrusChip != CLGD5434 && cirrusChip != CLGD5436) ||
     vga256InfoRec.videoRam < 2048)) {
         ErrorF("%s %s: %s: Only clgd5434 with 2048K supports 32bpp\n",
             XCONFIG_PROBED, vga256InfoRec.name, vga256InfoRec.chipset);
         CIRRUS.ChipHas32bpp = 0;
     }

     /* 
      * Banking granularity is 16k for the 5426, 5428 or 5429
      * when allowing access to 2MB, and 4k otherwise 
      */
     if (vga256InfoRec.videoRam > 1024)
          {
          CIRRUS.ChipSetRead = cirrusSetRead2MB;
          CIRRUS.ChipSetWrite = cirrusSetWrite2MB;
          CIRRUS.ChipSetReadWrite = cirrusSetReadWrite2MB;
	  cirrusBankShift = 8;
          }
/* ... */
     vga256InfoRec.bankedMono = 1;
     vga256InfoRec.maxClock = cirrusClockLimit[cirrusChip];
     /* Initialize option flags allowed for this driver */
     OFLG_SET(OPTION_NOACCEL, &CIRRUS.ChipOptionFlags);
     OFLG_SET(OPTION_PROBE_CLKS, &CIRRUS.ChipOptionFlags);
     OFLG_SET(OPTION_LINEAR, &CIRRUS.ChipOptionFlags);
     if ((cirrusChip >= CLGD5424 && cirrusChip <= CLGD5429) || HAVE543X()) {
         OFLG_SET(OPTION_SLOW_DRAM, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_MED_DRAM, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_FAST_DRAM, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_FIFO_CONSERV, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_FIFO_AGGRESSIVE, &CIRRUS.ChipOptionFlags);
     }
     if ((cirrusChip >= CLGD5426 && cirrusChip <= CLGD5429) || HAVE543X()) {
         OFLG_SET(OPTION_NO_2MB_BANKSEL, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_NO_BITBLT, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_FAVOUR_BITBLT, &CIRRUS.ChipOptionFlags);
         OFLG_SET(OPTION_NO_IMAGEBLT, &CIRRUS.ChipOptionFlags);
     }

     /* <scooper>
      *	The Hardware cursor, if the chip is capable, can be turned off using
      * the "sw_cursor" option.
      */

     if (Has_HWCursor(cirrusChip)) {
        OFLG_SET(OPTION_SW_CURSOR, &CIRRUS.ChipOptionFlags);
     }
#endif

     v_printf("Cirrus base address: 0x%x\n", cirrus_8514_base);
     v_printf("Cirrus memory size : %d kbyte\n", cirrus_memsize);

     if (config.gfxmemsize < 0) config.gfxmemsize = cirrus_memsize;
     v_8514_base = cirrus_8514_base;

     return(1);
}


/*
 * cirrusRestore -- 
 *      restore a video mode
 */
void cirrus_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
/*  00 unsigned char GR9;		 Graphics Offset1 */
/*  01 unsigned char GRA;		 Graphics Offset2 */
/*  02 unsigned char GRB;		 Graphics Extensions Control */
/*  03 unsigned char SR7;		 Extended Sequencer */
/*  04 unsigned char SRE;		 VCLK Numerator */
/*  05 unsigned char SRF;		 DRAM Control */
/*  06 unsigned char SR10;		 Graphics Cursor X Position [7:0]         */
/*  07 unsigned char SR10E;	 	 Graphics Cursor X Position [7:5] | 10000 */
/*  08 unsigned char SR11;		 Graphics Cursor Y Position [7:0]         */
/*  09 unsigned char SR11E;	 	 Graphics Cursor Y Position [7:5] | 10001 */
/*  10 unsigned char SR12;		 Graphics Cursor Attributes Register */
/*  11 unsigned char SR13;		 Graphics Cursor Pattern Address */
/*  12 unsigned char SR1E;		 VCLK Denominator */
/*  13 unsigned char CR19;		 Interlace End */
/*  14 unsigned char CR1A;		 Miscellaneous Control */
/*  15 unsigned char CR1B;		 Extended Display Control */
/*  16 unsigned char CR1D;		 Overlay Extended Control Register */
/*  17 unsigned char HIDDENDAC;	 Hidden DAC register */
/*  18 DACcolourRec  FOREGROUND;    Hidden DAC cursor foreground colour */
/*  19 DACcolourRec  BACKGROUND;    Hidden DAC cursor background colour */

  emu_video_retrace_off();
  port_out_w(0x0009, 0x3CE);	/* select bank 0 */
  port_out_w(0x000a, 0x3CE);

  port_out(0x0f, 0x3C4);	/* Restoring this registers avoids */
  port_out(xregs[5], 0x3C5);	/* textmode corruption on 2Mb cards. */

  port_out(0x07, 0x3C4);	/* This will disable linear addressing */
  port_out(xregs[3], 0x3C5);	/* if enabled. */

#if 0
  if (vgaBitsPerPixel != 8) {
      /*
       * Write to DAC. This is very delicate, and the it can lock up
       * the bus if not done carefully. The access count for the DAC
       * register can be such that the first write accesses either the
       * VGA LUT pixel mask register or the Hidden DAC register.
       */
      port_out(0x3c6, 0x00);	/* Reset access count. */
      port_out(0x3c6, 0xff);	/* Write 0xff to pixel mask. */
      port_in(0x3c6); port_in(0x3c6); port_in(0x3c6); port_in(0x3c6);
      port_out(0x3c6, restore->HIDDENDAC);
  }
#endif

  port_out_w(0x0100, 0x3C4);		/* disable timing sequencer */

  port_out(0x09, 0x3CE); port_out(xregs[0], 0x3CF);
  port_out(0x0a, 0x3CE); port_out(xregs[1], 0x3CF);
  port_out(0x0b, 0x3CE); port_out(xregs[2], 0x3CF);

  if (HAVE543X()) {
       port_out(0x0f, 0x3ce); port_out(xregs[20], 0x3cf);
  }

  port_out(0x0e, 0x3C4); port_out(xregs[4], 0x3C5);

  if (Has_HWCursor(cirrusChip))
    {
      /* Restore the hardware cursor */
      port_out (0x13, 0x3C4);
      port_out (xregs[11], 0x3C5);
      
      port_out (xregs[7], 0x3C4);
      port_out (xregs[6], 0x3C5);
      
      port_out (xregs[9], 0x3C4);
      port_out (xregs[8], 0x3C5);

      port_out (0x12, 0x3C4);
      port_out (xregs[10], 0x3C5);
    }

  if ((cirrusChip >= CLGD5424 && cirrusChip <= CLGD5429) || HAVE543X())
       {
       /* Restore the Performance Tuning Register on these chips only. */
       port_out(0x16, 0x3C4); port_out(xregs[21], 0x3C5);
       }

  if (HAVE543X()) {
       port_out(0x17, 0x3c4); port_out(xregs[22], 0x3c5);
  }

  port_out(0x1e, 0x3C4); port_out(xregs[12], 0x3C5);

  if ((cirrusChip >= CLGD5424 && cirrusChip <= CLGD5429) || HAVE543X()) {
      port_out(0x1f, 0x3c4);	/* MCLK register */
      port_out(xregs[23], 0x3c5);
  }

  port_out(0x19, vgaIOBase+4); port_out(xregs[13], vgaIOBase+5);
  port_out(0x1a, vgaIOBase+4); port_out(xregs[14], vgaIOBase+5);
  port_out(0x1b, vgaIOBase+4); port_out(xregs[15], vgaIOBase+5);

  if (cirrusChip == CLGD5434 || cirrusChip == CLGD5436) {
      port_out(0x1d, vgaIOBase+4);
      port_out(xregs[16], vgaIOBase+5);
  }
  emu_video_retrace_on();
}

/*
 * cirrusSave -- 
 *      save the current video mode
 */
void cirrus_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  unsigned char             temp1, temp2;
  
  emu_video_retrace_off();
  vgaIOBase = (port_in(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;

  port_out(0x09, 0x3CE);
  temp1 = port_in(0x3CF);
  port_out(0x00, 0x3CF);	/* select bank 0 */
  port_out(0x0a, 0x3CE);
  temp2 = port_in(0x3CF);
  port_out(0x00, 0x3CF);	/* select bank 0 */

/*  00 unsigned char GR9;		 Graphics Offset1 */
/*  01 unsigned char GRA;		 Graphics Offset2 */
/*  02 unsigned char GRB;		 Graphics Extensions Control */
/*  03 unsigned char SR7;		 Extended Sequencer */
/*  04 unsigned char SRE;		 VCLK Numerator */
/*  05 unsigned char SRF;		 DRAM Control */
/*  06 unsigned char SR10;		 Graphics Cursor X Position [7:0]         */
/*  07 unsigned char SR10E;	 	 Graphics Cursor X Position [7:5] | 10000 */
/*  08 unsigned char SR11;		 Graphics Cursor Y Position [7:0]         */
/*  09 unsigned char SR11E;	 	 Graphics Cursor Y Position [7:5] | 10001 */
/*  10 unsigned char SR12;		 Graphics Cursor Attributes Register */
/*  11 unsigned char SR13;		 Graphics Cursor Pattern Address */
/*  12 unsigned char SR1E;		 VCLK Denominator */
/*  13 unsigned char CR19;		 Interlace End */
/*  14 unsigned char CR1A;		 Miscellaneous Control */
/*  15 unsigned char CR1B;		 Extended Display Control */
/*  16 unsigned char CR1D;		 Overlay Extended Control Register */
/*  17 unsigned char HIDDENDAC;	 Hidden DAC register */
/*  18 DACcolourRec  FOREGROUND;    Hidden DAC cursor foreground colour */
/*  19 DACcolourRec  BACKGROUND;    Hidden DAC cursor background colour */

  xregs[0] = temp1;
  xregs[1] = temp2;

  port_out(0x0b, 0x3CE);		
  xregs[2] = port_in(0x3CF); 

  if (HAVE543X()) {
      port_out(0x0f, 0x3ce); xregs[20] = port_in(0x3cf);
  }

  port_out(0x07, 0x3C4); xregs[3] = port_in(0x3C5);
  port_out(0x0e, 0x3C4); xregs[4] = port_in(0x3C5);
  port_out(0x0f, 0x3C4); xregs[5] = port_in(0x3C5);

  if (Has_HWCursor(cirrusChip))
    {
      /* Hardware cursor */
      port_out (0x10, 0x3C4);
      xregs[7] = port_in (0x3C4);
      xregs[6] = port_in (0x3C5);

      port_out (0x11, 0x3C4);
      xregs[9] = port_in (0x3C4);
      xregs[8] = port_in (0x3C5);
  
      port_out (0x12, 0x3C4);
      xregs[10] = port_in (0x3C5);
  
      port_out (0x13, 0x3C4);
      xregs[11] = port_in (0x3C5);
    }  

  if ((cirrusChip >= CLGD5424 && cirrusChip <= CLGD5429) || HAVE543X()) 
       {
       /* Save the Performance Tuning Register on these chips only. */
        port_out(0x16, 0x3C4); xregs[21] = port_in(0x3C5);
       }

  if (HAVE543X())
       {
       port_out(0x17, 0x3c4); xregs[22] = port_in(0x3c5);
       }

  port_out(0x1e, 0x3C4); xregs[12] = port_in(0x3C5);

  if ((cirrusChip >= CLGD5424 && cirrusChip <= CLGD5429) || HAVE543X()) {
      port_out(0x1f, 0x3c4);		/* Save the MCLK register. */
      xregs[23] = port_in(0x3c5);
  }

  port_out(0x19, vgaIOBase+4); xregs[13] = port_in(vgaIOBase+5);
  port_out(0x1a, vgaIOBase+4); xregs[14] = port_in(vgaIOBase+5);
  port_out(0x1b, vgaIOBase+4); xregs[15] = port_in(vgaIOBase+5);

  if (cirrusChip == CLGD5434 || cirrusChip == CLGD5436) {
      port_out(0x1d, vgaIOBase+4);
      xregs[16] = port_in(vgaIOBase+5);
  }

#if 0
  if (vgaBitsPerPixel != 8) {
      port_out(0x3c6, 0x00);	/* Reset access count. */
      port_out(0x3c6, 0xff);	/* Write 0xff to pixel mask. */
      port_in(0x3c6); port_in(0x3c6); port_in(0x3c6); port_in(0x3c6);
      save->HIDDENDAC = port_in(0x3c6);
  }
#endif

  emu_video_retrace_on();
}


void vga_init_cirrus(void)
{
  if (cirrusProbe()) {
	save_ext_regs = cirrus_save_ext_regs;
	restore_ext_regs = cirrus_restore_ext_regs;
#if 0
	set_bank_read = cirrus_set_bank;
	set_bank_write = cirrus_set_bank;
	ext_video_port_in = cirrus_ext_video_port_in;
	ext_video_port_out = cirrus_ext_video_port_out;
#endif
  }
}

#undef CIRRUS_C
