/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
#include "pci.h"
#include "dosemu_config.h"

static void *vesa_pm_code;
static int vesa_regs_size, vesa_granularity, vesa_read_write;
static char *vesa_oemstring;
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
  vesa_r.es = (unsigned int)vbe_buffer >> 4;
  vesa_r.edi = 0;
  VBE_viVBESig = 0x32454256; /* "VBE2" */

  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) != 0x4f || VBE_viVBESig != 0x41534556 /* "VESA" */ ) {
    v_printf("No VESA bios detected!\n");
    config.gfxmemsize = 256;
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

  config.gfxmemsize = VBE_viMemory*64;
  vesa_version = VBE_viVESAVersion;

  if (vesa_version >= 0x200) {
    /* Get pm interface, used for a faster setbank */
    unsigned short int *pm_info, *pm_list;
    vesa_r.eax = 0x4f0a;
    vesa_r.ebx = 0;
    do_int10_callback(&vesa_r);
    pm_info = MK_FP32(vesa_r.es, vesa_r.edi & 0xffff);
    vesa_pm_code = (char *)pm_info + pm_info[0];

    pm_list = (unsigned short *)((char *)pm_info + pm_info[3]);
    if (pm_list != pm_info) {
      while(*pm_list != 0xffff) {
	if(*pm_list >= 0x400 && kernel_version_code < 0x20608)
	  /* can't use slow ports in the main DOSEMU code */
	  vesa_pm_code = NULL;
	pm_list++;
      }
      if(pm_list[1] != 0xffff) {
	v_printf("VESA: no support for MMIO selectors\n");
	vesa_pm_code = NULL;
      }
    }
    v_printf("VESA: protected mode bank switch code at %p\n", vesa_pm_code);
  }

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
      size_t vesa_linear_base = VBE_vmPhysBasePtr;
      int i;
      pciRec *pcirec = pcibios_find_class(PCI_CLASS_DISPLAY_VGA << 8, 0);
      if (pcirec) for (i = 0; i < 7; i++) {
	if (pcirec->region[i].type == PCI_BASE_ADDRESS_SPACE_MEMORY &&
	    pcirec->region[i].base <= vesa_linear_base &&
	    vesa_linear_base < pcirec->region[i].base + pcirec->region[i].size ) {
	  vesa_linear_vbase = pcirec->region[i].vbase +
	    vesa_linear_base - pcirec->region[i].base;
	  v_printf("VESA: physical base = %x, virtual base = %x\n",
		   vesa_linear_base, vesa_linear_vbase);
	  break;
	}
      }
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

static inline void call_vesa_pm_code(unsigned long ebx, unsigned long edx)
{
  v_printf("VESA: calling protected mode bankswitch code\n");
  __asm__ __volatile__(
		       "call *(%%edi)"
		       : /* no return value */
		       : "a" (0x4f05),
		       "b" (ebx),
		       "d" (edx),
		       "D" (&vesa_pm_code));
}

static void vesa_setbank_read(unsigned char bank)
{
  vesa_r.eax = 0x4f05;
  vesa_r.ebx = (vesa_read_write & 2) ? 0 : 1;
  vesa_r.edx = bank*64/vesa_granularity;
  if (vesa_pm_code)
    call_vesa_pm_code(vesa_r.ebx, vesa_r.edx);
  else
    do_int10_callback(&vesa_r);
}

static void vesa_setbank_write(unsigned char bank)
{
  vesa_r.eax = 0x4f05;
  vesa_r.ebx = (vesa_read_write & 4) ? 0 : 1;
  vesa_r.edx = bank*64/vesa_granularity;
  if (vesa_pm_code)
    call_vesa_pm_code(vesa_r.ebx, vesa_r.edx);
  else
    do_int10_callback(&vesa_r);
}

void vesa_init(void)
{
  /* If there's a DOS TSR in real memory (say, univbe followed by loadlin)
     then don't call int10 here yet */
  vesa_int10 = MK_FP16(ISEG(0x10), IOFF(0x10));
  if (FP_SEG32(IVEC(0x10)) < config.vbios_seg ||
      FP_SEG32(IVEC(0x10)) >= config.vbios_seg + (config.vbios_size >> 4))
    v_printf("VESA: int10 is not in the BIOS (DOS TSR?): giving up for now\n");
  else
    vesa_reinit();
  /* This is all we need before booting. Memory info comes later */
  save_ext_regs = vesa_save_ext_regs;
  restore_ext_regs = vesa_restore_ext_regs;
  set_bank_read = vesa_setbank_read;
  set_bank_write = vesa_setbank_write;  
}

char *vesa_get_lfb(void)
{
  vesa_r.eax = 0x4f02;
  /* 0x81ff | 0x4000 is the special "all memory access mode" + LFB */
  vesa_r.ebx = vesa_linear_vbase ? 0xc1ff :
    vesa_version >= 0x200 ? 0x81ff : 0x101;
  do_int10_callback(&vesa_r);
  if (vesa_linear_vbase)
    return (char *)vesa_linear_vbase;
  return (char *)GRAPH_BASE;
}
