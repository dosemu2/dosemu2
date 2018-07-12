/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Console graphics interface to the (real) VESA BIOS
 *
 * Authors: Stas Sergeev, Bart Oldeman, based on svgalib's vesa driver
 *
 * Note: original vesa driver in svgalib came with no explicit copyrights
 * or license clauses.
 */


#include <stdlib.h>
#include <string.h>

#include "emu.h"
#include "utilities.h"
#include "vc.h"
#include "vga.h"
#include "dpmi.h"
#include "lowmem.h"
#include "coopth.h"
#include "vbe.h"
#include "vesa.h"
#include "mapping.h"
#include "dosemu_config.h"

static int vesa_regs_size, vesa_granularity, vesa_read_write;
static char *vesa_oemstring = NULL;
static unsigned vesa_linear_vbase;
static unsigned vesa_int10, vesa_oemid;
static unsigned vesa_version;

struct VBE_vi_vm {
  struct VBE_vi vbei;
  struct VBE_vm vbemi;
} __attribute__((packed));

#define VESA_SAVE_BITMAP 0xf /* save everything */

#define RM_STACK_SIZE 0x200
static void *rm_stack = NULL;

static void do_int10_callback(struct vm86_regs *regs)
{
  struct vm86_regs saved_regs;
  char *p;

  saved_regs = REGS;
  REGS = *regs;
  v_printf("VGA: call interrupt 0x10, ax=%#x\n", LWORD(eax));
  /* we don't want the BIOS to call the mouse helper */
  p = MK_FP32(BIOSSEG, (long)&bios_in_int10_callback - (long)bios_f000);
  *p = 1;
  do_int_call_back(0x10);
  *p = 0;
  v_printf("VGA: interrupt returned, ax=%#x\n", LWORD(eax));
  *regs = REGS;
  REGS = saved_regs;
}

/* vesa_reinit: a function to reinitialize in case the DOS VESA driver
   changes at runtime (e.g. univbe). Also called at startup */
static void vesa_reinit(void)
{
  struct vm86_regs vesa_r;
  struct VBE_vi_vm *vbe_buffer;
  struct VBE_vi *vbei;
  struct VBE_vm *vbemi;
  char *s;

  coopth_attach();
  vesa_r = REGS;
  vesa_int10 = MK_FP16(ISEG(0x10), IOFF(0x10));

  vbe_buffer = lowmem_heap_alloc(sizeof *vbe_buffer);
  vbei = &vbe_buffer->vbei;
  vesa_r.eax = 0x4f00;
  vesa_r.es = DOSEMU_LMHEAP_SEG + (DOSEMU_LMHEAP_OFFS_OF(vbei)>>4);
  vesa_r.edi = 0;
  vbei->VBESig = 0x32454256; /* "VBE2" */

  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) != 0x4f || vbei->VBESig != 0x41534556 /* "VESA" */ ) {
    v_printf("No VESA bios detected!\n");
    if (config.gfxmemsize < 0) config.gfxmemsize = 256;
    vesa_regs_size = 0;
    vesa_linear_vbase = -1;
    goto out;
  }

  /* check if the VESA BIOS has changed */
  s = MK_FP32(FP_SEG16(vbei->OEMID), FP_OFF16(vbei->OEMID));
  if (vesa_oemstring && vbei->OEMID == vesa_oemid &&
      strcmp(s, vesa_oemstring) == 0)
    goto out;
  vesa_oemid = vbei->OEMID;
  if (vesa_oemstring) free(vesa_oemstring);
  vesa_oemstring = strdup(s);

  if (config.gfxmemsize < 0) config.gfxmemsize = vbei->Memory*64;
  vesa_version = vbei->VESAVersion;

  vbemi = &vbe_buffer->vbemi;

  vesa_granularity = 64;
  vesa_read_write = 6;
  vesa_linear_vbase = -1;

  vesa_r.eax = 0x4f01;
  vesa_r.ecx = vesa_version >= 0x200 ? 0x81ff : 0x101;
  vesa_r.es = DOSEMU_LMHEAP_SEG + (DOSEMU_LMHEAP_OFFS_OF(vbe_buffer)>>4);
  vesa_r.edi = DOSEMU_LMHEAP_OFFS_OF(vbe_buffer) & 0xf;
  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) == 0x4f) {
    vesa_granularity= vbemi->WinGran;
    vesa_read_write = vbemi->WinAAttrib & 6;
    if (vesa_version >= 0x200 && (vbemi->ModeAttrib & 0x80) && config.pci_video) {
      vesa_linear_vbase = get_hardware_ram(vbemi->PhysBasePtr);
      v_printf("VESA: physical base = %x, virtual base = %x\n",
	       vbemi->PhysBasePtr, vesa_linear_vbase);
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
  lowmem_heap_free(vbe_buffer);
  v_printf("VESA: memory size = %lu, regs_size=%x\n",
	   config.gfxmemsize, vesa_regs_size);
}

/* Read and save chipset-specific registers */
static void vesa_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  struct vm86_regs vesa_r;
  void *lowmem;

  coopth_attach();
  vesa_r = REGS;
  /* if int10 changed we may have to reinitialize */
  if (MK_FP16(ISEG(0x10), IOFF(0x10)) != vesa_int10)
    vesa_reinit();
  if (vesa_regs_size == 0)
    return;
  lowmem = lowmem_heap_alloc(vesa_regs_size);
  vesa_r.eax = 0x4f04;
  vesa_r.ebx = 0;
  vesa_r.es = DOSEMU_LMHEAP_SEG + (DOSEMU_LMHEAP_OFFS_OF(lowmem)>>4);
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
  struct vm86_regs vesa_r;
  void *lowmem;
  unsigned long current_int10;

  if (xregs16[0] == 0)
    return;
  coopth_attach();
  vesa_r = REGS;
  lowmem = lowmem_heap_alloc(xregs16[0]);
  memcpy(lowmem, xregs, xregs16[0]);
  vesa_r.eax = 0x4f04;
  vesa_r.ebx = 0;
  vesa_r.es = DOSEMU_LMHEAP_SEG + (DOSEMU_LMHEAP_OFFS_OF(lowmem)>>4);
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
  struct vm86_regs vesa_r;
  coopth_attach();
  vesa_r = REGS;
  vesa_r.eax = 0x4f05;
  vesa_r.ebx = (vesa_read_write & 2) ? 0 : 1;
  vesa_r.edx = bank*64/vesa_granularity;
  do_int10_callback(&vesa_r);
}

static void vesa_setbank_write(unsigned char bank)
{
  struct vm86_regs vesa_r;
  coopth_attach();
  vesa_r = REGS;
  vesa_r.eax = 0x4f05;
  vesa_r.ebx = (vesa_read_write & 4) ? 0 : 1;
  vesa_r.edx = bank*64/vesa_granularity;
  do_int10_callback(&vesa_r);
}

void vesa_init(void)
{
  rm_stack = lowmem_heap_alloc(RM_STACK_SIZE);
  vesa_int10 = MK_FP16(ISEG(0x10), IOFF(0x10));
  vesa_reinit();
  /* This is all we need before booting. Memory info comes later */
  save_ext_regs = vesa_save_ext_regs;
  restore_ext_regs = vesa_restore_ext_regs;
  set_bank_read = vesa_setbank_read;
  set_bank_write = vesa_setbank_write;
}

unsigned vesa_get_lfb(void)
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
  vesa_r.ebx = (vesa_linear_vbase != -1) ? 0xc1ff :
    vesa_version >= 0x200 ? 0x81ff : 0x101;
  do_int10_callback(&vesa_r);
  if ((vesa_r.eax & 0xffff) != 0x4f)
    return GRAPH_BASE;
#endif
  if (vesa_linear_vbase != -1)
    return vesa_linear_vbase;
  return GRAPH_BASE;
}
