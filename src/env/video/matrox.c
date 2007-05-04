/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* $XConsortium: mga_driver.c /main/12 1996/10/28 05:13:26 kaleb $ */
/*
 * MGA Millennium (MGA2064W) with Ti3026 RAMDAC driver v.1.1
 *
 * Copyright 1996 The XFree86 Project, Inc.
 *
 * The driver is written without any chip documentation. All extended ports
 * and registers come from tracing the VESA-ROM functions.
 * The BitBlt Engine comes from tracing the windows BitBlt function.
 *
 * Author:	Radoslaw Kapitan, Tarnow, Poland
 *			kapitan@student.uci.agh.edu.pl
 *		original source
 *
 * Changes for DOSEMU:
 *		9/97 Alberto Vignani <vignani@tin.it>
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "emu.h"        /* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "pci.h"
#include "matrox.h"
#include "mapping.h"

static struct MGAextreg {
	unsigned char ExtVga[6];
} MGAReg;
    
/*
 * Driver data structures.
 */
static unsigned int pciconf[64];

static char MGAIdent[64] = "";
static int MGAchipset;
static int MGA_memsize = 0;
static int MGA_8514_base = BASE_8514_1;
static int vgaIOBase = 0x3D0;
static unsigned char *MGAMMIOBase = NULL;
/*static unsigned char *MGALinearBase = NULL;*/


static void *MapVidMem (unsigned long addr, int size)
{
  alloc_mapping(MAPPING_VIDEO | MAPPING_KMEM, size, addr);
  return (char *)mmap_mapping(MAPPING_VIDEO | MAPPING_KMEM,
	(caddr_t)-1, size, PROT_READ|PROT_WRITE|PROT_EXEC, addr);
}

static int UnMapVidMem (void *base, int size)
{
  return munmap_mapping(MAPPING_VIDEO, base, size);
}


/*
 * vgaProtect --
 *	Protect VGA registers and memory from corruption during loads.
 */
static void
vgaProtect(Boolean on)
{
  unsigned char tmp;

    if (on) {
      /*
       * Turn off screen and disable sequencer.
       */
      port_real_outb(0x3C4, 0x01);
      tmp = port_real_inb(0x3C5);

      port_real_outw(0x3C4, ((tmp | 0x20) << 8) | 0x01);	/* disable the display */

      tmp = port_real_inb(vgaIOBase + 0x0A);
      port_real_outb(0x3C0, 0x00);		/* enable pallete access */
    }
    else {
      /*
       * Reenable sequencer, then turn on screen.
       */
      port_real_outb(0x3C4, 0x01);
      tmp = port_real_inb(0x3C5);

      port_real_outw(0x3C4, ((tmp & 0xDF) << 8) | 0x01);	/* reenable display */

      tmp = port_real_inb(vgaIOBase + 0x0A);
      port_real_outb(0x3C0, 0x20);		/* disable pallete access */
    }
}


/*
 * MGACountRAM --
 *
 * Counts amount of installed RAM 
 */
static int
MGACountRam(unsigned long linbase)
{
  if (linbase) {
	volatile unsigned char* base;
	unsigned char tmp, tmp3, tmp5;

	base = MapVidMem(linbase, 8192 * 1024);
	
	/* turn MGA mode on - enable linear frame buffer (CRTCEXT3) */
	port_real_outb(0x3DE, 3);
	tmp = port_real_inb(0x3DF);
	port_real_outb(0x3DF, tmp | 0x80);
	
	/* write, read and compare method */
	base[0x500000] = 0x55;
	base[0x300000] = 0x33;
	base[0x100000] = 0x11;
	tmp5 = base[0x500000];
	tmp3 = base[0x300000];

	/* restore CRTCEXT3 state */
	port_real_outb(0x3DE, 3);
	port_real_outb(0x3DF, tmp);
	
	UnMapVidMem((char *)base, 8192 * 1024);
	
	if(tmp5 == 0x55)
		return 8192;
	if(tmp3 == 0x33)
		return 4096;
  }
  return 2048;
}


/*
 * MGAEnterLeave --
 *
 * This function is called when the virtual terminal on which the server
 * is running is entered or left, as well as when the server starts up
 * and is shut down.	Its function is to obtain and relinquish I/O 
 * permissions for the SVGA device.	 This includes unlocking access to
 * any registers that may be protected on the chipset, and locking those
 * registers again on exit.
 */
static void 
MGAEnterLeave(Boolean enter)
{
	unsigned char temp;

	if (enter)
	{
		vgaIOBase = (port_real_inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
		/* Unprotect CRTC[0-7] */
		port_real_outb(vgaIOBase + 4, 0x11);
		temp = port_real_inb(vgaIOBase + 5);
		port_real_outb(vgaIOBase + 5, temp & 0x7F);
	}
	else
	{
		/* Protect CRTC[0-7] */
		port_real_outb(vgaIOBase + 4, 0x11);
		temp = port_real_inb(vgaIOBase + 5);
		port_real_outb(vgaIOBase + 5, (temp & 0x7F) | 0x80);
	}
}


/*
 * MGAProbe --
 *
 * This is the function that makes a yes/no decision about whether or not
 * a chipset supported by this driver is present or not.
 * Since we have not access to the PCI device info we trust the config
 * file and the kernel...
 */
static Boolean matroxProbe(void)
{
  FILE *fp;
  unsigned int bus, device, fn;
  int i, ok;

  ok = 0;

  fp=fopen("/proc/pci","r");
  if (fp) {
    char Line[100], *p;
    while (fgets(Line,sizeof(Line),fp)) {
      if (sscanf(Line," Bus %d, device %d, function %d",&bus,&device,&fn)==3) {
	fgets(Line,sizeof(Line),fp);
	if ((p=strstr(Line,"Matrox"))) {
	  ok=1;
	  break;
	}
      }
    }
    fclose(fp);
  }
  if (!ok) return FALSE;

  /* this is more a demo than a real useful thing - all the work is
   * done by the VGA BIOS anyway
   */
  pciConfigType->read_header ((unsigned char)bus, (unsigned char)device,
	(unsigned char)fn, pciconf);

  MGAMMIOBase = MapVidMem(pciconf[4]&~0xfff, 0x4000);
  v_printf("MGA: mmap() %#x at %p\n", pciconf[4], MGAMMIOBase);
  if (MGAMMIOBase == MAP_FAILED) return(FALSE);
  for (i=0; i<64; i+=4) {
    v_printf("PCI: %08x %08x %08x %08x\n", pciconf[i], pciconf[i+1],
	 pciconf[i+2], pciconf[i+3]);
  }

  if ((pciconf[0]&0xffff)==PCI_VENDOR_ID_MATROX) {
    switch (pciconf[0]>>16) {
      case PCI_DEVICE_ID_MATROX_MIL:
            MGAchipset=PCI_CHIP_MGA2064;
            strcpy(MGAIdent, "Matrox Millennium");
	    v_printf("MGA %s chipset mga2064w\n",MGAIdent);
            break;
      case PCI_DEVICE_ID_MATROX_MYS:
            MGAchipset=PCI_CHIP_MGA1064;
            strcpy(MGAIdent, "Matrox Mystique");
	    v_printf("MGA %s chipset mga1064w\n",MGAIdent);
            break;
      default:
	    v_printf("MGA unknown chipset\n");
	    return FALSE;
    }
  }

  /* enable IO ports, etc. */
  MGAEnterLeave(TRUE);

  MGA_memsize = MGACountRam(pciconf[5]&~0xfff);
	
  v_printf("MGA base address: 0x%x\n", MGA_8514_base);

  if (config.gfxmemsize < 0)
	config.gfxmemsize = MGA_memsize;
  v_printf("MGA memory size : %ld kbyte\n", config.gfxmemsize);
  v_8514_base = MGA_8514_base;

  UnMapVidMem(MGAMMIOBase, 0x4000);

  return(TRUE);
}



/*
 * MGA3026Restore -- for mga2064 with ti3026
 *
 * This function restores a video mode.	 It basically writes out all of
 * the registers that have previously been saved in the vgaMGARec data 
 * structure.
 */
static void
MGA3026Restore(void)
{
  int i;
  struct MGAextreg *restore = &MGAReg;

  /*
   * Code is needed to get things back to bank zero.
   */
  for (i = 0; i < 6; i++)
	port_real_outw(0x3DE, (restore->ExtVga[i] << 8) | i);

}

/*
 * MGARestore --
 *
 * This function restores a video mode.	 It basically writes out all of
 * the registers that have previously been saved in the vgaMGARec data 
 * structure.
 */
void matrox_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
  emu_video_retrace_off();
  vgaProtect(TRUE);
	
  switch (MGAchipset)
  {
    case PCI_CHIP_MGA2064:
	MGA3026Restore();
	break;
#if 0
    case PCI_CHIP_MGA1064:
	MGA1064Restore();
	break;
#endif
  }

  vgaProtect(FALSE);
  emu_video_retrace_on();
}

/*
 * MGA3026Save -- for mga2064 with ti3026
 *
 * This function saves the video state.	 It reads all of the SVGA registers
 * into the vgaMGARec data structure.
 */
static void
MGA3026Save(void)
{
  int i;
  struct MGAextreg *save = &MGAReg;
	
  /*
   * Code is needed to get back to bank zero.
   */
  port_real_outw(0x3DE, 0x0004);
	
  /*
   * The port I/O code necessary to read in the extended registers 
   * into the fields of the vgaMGARec structure.
   */
  for (i = 0; i < 6; i++)
  {
	port_real_outb(0x3DE, i);
	save->ExtVga[i] = port_real_inb(0x3DF);
  }

}

/*
 * MGASave --
 *
 * This function saves the video state.	 It reads all of the SVGA registers
 * into the vgaMGARec data structure.
 */
void matrox_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  emu_video_retrace_off();
  switch (MGAchipset)
  {
    case PCI_CHIP_MGA2064: MGA3026Save(); break;
#if 0
    case PCI_CHIP_MGA1064: MGA1064Save(); break;
#endif
  }
  emu_video_retrace_on();
}


void vga_init_matrox(void)
{
  if (config.pci_video && matroxProbe()) {

        if (MGAchipset==PCI_CHIP_MGA2064) {
	  save_ext_regs = matrox_save_ext_regs;
	  restore_ext_regs = matrox_restore_ext_regs;
	}
#if 0
	set_bank_read = matrox_set_bank;
	set_bank_write = matrox_set_bank;
#endif
  }
}

