/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * Console graphics interface to the (real) VESA BIOS
 *
 * Authors: Stas Sergeev, Bart Oldeman, based on svgalib's vesa driver
 */


#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "emu.h"
#include "vc.h"
#include "vga.h"
#include "lowmem.h"
#include "vbe.h"
#include "vesa.h"
#include "mapping.h"
#include "dosemu_config.h"

static int vesa_regs_size, vesa_granularity, vesa_read_write;
static char *vesa_oemstring = NULL;
static size_t vesa_linear_vbase;
static unsigned long vesa_int10, vesa_oemid;
static unsigned vesa_version;
static struct vm86_regs vesa_r;

#define VESA_SAVE_BITMAP 0xf /* save everything */
#define VBE_BUFFER vbe_buffer

/* vesa_reinit: a function to reinitialize in case the DOS VESA driver
   changes at runtime (e.g. univbe). Also called at startup */
static void vesa_reinit(void)
{
  unsigned char *vbe_buffer, *info_buffer, *s;

  vesa_int10 = MK_FP16(ISEG(0x10), IOFF(0x10));

  vbe_buffer = info_buffer = lowmem_heap_alloc(VBE_viSize+VBE_vmSize);
  vesa_r.eax = 0x4f00;
  vesa_r.es = FP_SEG32(vbe_buffer);
  vesa_r.edi = 0;
  VBE_viVBESig = 0x32454256; /* "VBE2" */

  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) != 0x4f || VBE_viVBESig != 0x41534556 /* "VESA" */ ) {
    v_printf("No VESA bios detected!\n");
    if (config.gfxmemsize < 0) config.gfxmemsize = 256;
    vesa_regs_size = 0;
    vesa_linear_vbase = 0;
    goto out;
  }

  /* check if the VESA BIOS has changed */
  s = MK_FP32(FP_SEG16(VBE_viOEMID), FP_OFF16(VBE_viOEMID));
  if (vesa_oemstring && VBE_viOEMID == vesa_oemid &&
      strcmp(s, vesa_oemstring) == 0)
    goto out;
  vesa_oemid = VBE_viOEMID;
  if (vesa_oemstring) free(vesa_oemstring);
  vesa_oemstring = strdup(s);

  if (config.gfxmemsize < 0) config.gfxmemsize = VBE_viMemory*64;
  vesa_version = VBE_viVESAVersion;

  vbe_buffer += VBE_viSize;
  memset(&vesa_r, 0, sizeof(vesa_r));

  vesa_granularity = 64;
  vesa_read_write = 6;
  vesa_linear_vbase = 0;

  vesa_r.eax = 0x4f01;
  vesa_r.ecx = vesa_version >= 0x200 ? 0x81ff : 0x101;
  vesa_r.es = (size_t)vbe_buffer >> 4;
  vesa_r.edi = (size_t)vbe_buffer & 0xf;
  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) == 0x4f) {
    vesa_granularity= VBE_vmWinGran;
    vesa_read_write = VBE_vmWinAAttrib & 6;
    if (vesa_version >= 0x200 && (VBE_vmModeAttrib & 0x80) && config.pci_video) {
      vesa_linear_vbase = (size_t)get_hardware_ram(VBE_vmPhysBasePtr);
      v_printf("VESA: physical base = %x, virtual base = %zx\n",
	       VBE_vmPhysBasePtr, vesa_linear_vbase);
    }
  } else {
    v_printf("VESA: Can't get mode info\n");
  }
  /* if not reported then guess */
  if (vesa_granularity == 0) vesa_granularity = 64;

  vesa_regs_size = 0;
  vesa_r.eax = 0x4f04;
  vesa_r.edx = 0;
  vesa_r.ecx = VESA_SAVE_BITMAP;
  vesa_r.ebx = 0;
  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) == 0x4f)
    vesa_regs_size = vesa_r.ebx * 64;

 out:
  lowmem_heap_free(info_buffer);
  v_printf("VESA: memory size = %lu, regs_size=%x\n",
	   config.gfxmemsize, vesa_regs_size);
}

/* Read and save chipset-specific registers */
static void vesa_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  void *lowmem;

  /* if int10 changed we may have to reinitialize */
  if (MK_FP16(ISEG(0x10), IOFF(0x10)) != vesa_int10)
    vesa_reinit();
  if (vesa_regs_size == 0)
    return;
  lowmem = lowmem_heap_alloc(vesa_regs_size);
  vesa_r.eax = 0x4f04;
  vesa_r.ebx = 0;
  vesa_r.es = FP_SEG32(lowmem);
  vesa_r.edx = 1;
  vesa_r.ecx = VESA_SAVE_BITMAP;
  do_int10_callback(&vesa_r);
  /* abuse xregs16 to store some important info: size and int10 vector */
  xregs16[0] = vesa_regs_size;
  xregs16[1] = IOFF(0x10);
  xregs16[2] = ISEG(0x10);
  memcpy(xregs, lowmem, vesa_regs_size);  
  lowmem_heap_free(lowmem);
}

/* Restore and write chipset-specific registers */
static void vesa_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
  void *lowmem;
  unsigned long current_int10;

  if (xregs16[0] == 0)
    return;
  lowmem = lowmem_heap_alloc(xregs16[0]);
  memcpy(lowmem, xregs, xregs16[0]);  
  vesa_r.eax = 0x4f04;
  vesa_r.ebx = 0;
  vesa_r.es = FP_SEG32(lowmem);
  vesa_r.edx = 2;
  vesa_r.ecx = VESA_SAVE_BITMAP;
  /* swap int10 vectors with the original int10 vector as it was
     when the registers were saved to avoid conflicts with univbe */
  current_int10 = MK_FP16(ISEG(0x10), IOFF(0x10));
  SETIVEC(0x10, xregs16[2], xregs16[1]);
  do_int10_callback(&vesa_r);
  SETIVEC(0x10, FP_SEG16(current_int10), FP_OFF16(current_int10));
  lowmem_heap_free(lowmem);
}

/* setbank read/write functions: these are only called by
   the save/restore routines if no LFB is available.
   An alternative to int10/ax=4f0x here is to call protected
   mode routines, but they are harder to set up (would
   have to go via a DPMI style setup to handle cli/sti) */

static void vesa_setbank_read(unsigned char bank)
{
  vesa_r.eax = 0x4f05;
  vesa_r.ebx = (vesa_read_write & 2) ? 0 : 1;
  vesa_r.edx = bank*64/vesa_granularity;
  do_int10_callback(&vesa_r);
}

static void vesa_setbank_write(unsigned char bank)
{
  vesa_r.eax = 0x4f05;
  vesa_r.ebx = (vesa_read_write & 4) ? 0 : 1;
  vesa_r.edx = bank*64/vesa_granularity;
  do_int10_callback(&vesa_r);
}

void vesa_init(void)
{
  vesa_int10 = MK_FP16(ISEG(0x10), IOFF(0x10));
  vesa_reinit();
  /* This is all we need before booting. Memory info comes later */
  save_ext_regs = vesa_save_ext_regs;
  restore_ext_regs = vesa_restore_ext_regs;
  set_bank_read = vesa_setbank_read;
  set_bank_write = vesa_setbank_write;  
}

char *vesa_get_lfb(void)
{
  /*
   * The below trick doesn't seem to work on my Matrox G550 -
   * looks like it ignores the bit15 and clears the video memory.
   * We don't really need that special mode after all, so
   * it is disabled.
   */
#if 0
  vesa_r.eax = 0x4f02;
  /* 0x81ff | 0x4000 is the special "all memory access mode" + LFB */
  vesa_r.ebx = vesa_linear_vbase ? 0xc1ff :
    vesa_version >= 0x200 ? 0x81ff : 0x101;
  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) != 0x4f)
    return (char *)GRAPH_BASE;
#endif
  if (vesa_linear_vbase)
    return (char *)vesa_linear_vbase;
  return (char *)GRAPH_BASE;
}
