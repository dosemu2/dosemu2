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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: EMS frame manipulation routines
 *
 * Author: Stas Sergeev, code ported from pmdapi project.
 *
 */
#include <string.h>
#include <assert.h>
#include <dpmi_api.h>
#include "msdos_priv.h"
#include "emm_msdos.h"

#define ALLOCATE_PAGES          0x43
#define SAVE_PAGE_MAP           0x47
#define RESTORE_PAGE_MAP        0x48
#define MAP_UNMAP_MULTIPLE      0x50
#define GET_MPA_ARRAY           0x58

#define EMM_INT                 0x67

int emm_allocate_handle(cpuctx_t *scp, int is_32, int pages_needed)
{
  __dpmi_regs regs = {0};
  regs.h.ah = ALLOCATE_PAGES;
  regs.x.bx = pages_needed;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, EMM_INT, &regs);
  if (regs.h.ah)
    return -1;
  return regs.x.dx;
}

int emm_save_handle_state(cpuctx_t *scp, int is_32, int handle)
{
  __dpmi_regs regs = {0};
  regs.h.ah = SAVE_PAGE_MAP;
  regs.x.dx = handle;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, EMM_INT, &regs);
  if (regs.h.ah)
    return -1;
  return 0;
}

int emm_restore_handle_state(cpuctx_t *scp, int is_32, int handle)
{
  __dpmi_regs regs = {0};
  regs.h.ah = RESTORE_PAGE_MAP;
  regs.x.dx = handle;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, EMM_INT, &regs);
  if (regs.h.ah)
    return -1;
  return 0;
}

int emm_map_unmap_multi(cpuctx_t *scp, int is_32, const u_short *array,
    int handle, int map_len)
{
  uint16_t buf_seg = get_scratch_seg();
  void *rmaddr = SEG2UNIX(buf_seg);
  __dpmi_regs regs = {0};
  int arr_len = sizeof(u_short) * 2 * map_len;

  assert(arr_len <= 16);
  regs.h.ah = MAP_UNMAP_MULTIPLE;
  regs.h.al = 0;
  regs.x.dx = handle;
  regs.x.cx = map_len;
  memcpy(rmaddr, array, arr_len);
  regs.x.ds = buf_seg;
  regs.x.si = 0;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, EMM_INT, &regs);
  if (regs.h.ah)
    return -1;
  return 0;
}

int emm_get_mpa_len(cpuctx_t *scp, int is_32)
{
  __dpmi_regs regs = {0};
  __dpmi_raddr vec = {0};

  _dpmi_get_real_mode_interrupt_vector(scp, is_32, EMM_INT, &vec);
  if (!vec.segment && !vec.offset16)  // avoid crash
    return -1;
  regs.h.ah = GET_MPA_ARRAY;
  regs.h.al = 1;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, EMM_INT, &regs);
  if (regs.h.ah)
    return -1;
  return regs.x.cx;
}

int emm_get_mpa_array(cpuctx_t *scp, int is_32,
    struct emm_phys_page_desc *array, int max_len)
{
  uint16_t buf_seg = get_scratch_seg();
  void *rmaddr = SEG2UNIX(buf_seg);
  __dpmi_regs regs = {0};
  regs.h.ah = GET_MPA_ARRAY;
  regs.h.al = 0;
  regs.x.es = buf_seg;
  regs.x.di = 0;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, EMM_INT, &regs);
  if (regs.h.ah || regs.x.cx > max_len)
    return -1;
  memcpy(array, rmaddr, regs.x.cx * sizeof(array[0]));
  return regs.x.cx;
}
