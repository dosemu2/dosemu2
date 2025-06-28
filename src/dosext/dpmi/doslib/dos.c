/*
 *  Copyright (C) 2024  stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../emudpmi.h"
#include "../dpmisel.h"
#include "../dpmi_api.h"
#include "coopth.h"
#include "dos.h"

unsigned _dos_open(const char *pathname, unsigned mode, int *handle)
{
  cpuctx_t *scp = dpmi_get_scp();
  __dpmi_regs regs = {};
  int len = strlen(pathname) + 1;
  unsigned ret = 0;
  int is_32 = dpmi_is_32();
  int sel;
  int para = (len >> 4) + 1;
  int seg;

  if (len > 65536 - 16)
    return -1;
  seg = _dpmi_allocate_dos_memory(scp, is_32, para, &sel);
  if (seg == -1)
    return -1;

  regs.d.eax = 0x3d00 | (mode & 0xff);
  regs.d.ecx = 0;
  regs.x.ds = seg;
  regs.d.edx = 0;
  MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0), pathname, len);
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    ret = regs.d.eax;
  else
    *handle = regs.d.eax;
  _dpmi_free_dos_memory(scp, is_32, sel);

  return ret;
}

unsigned _dos_read(int handle, void *buffer, unsigned count, unsigned *numread)
{
  cpuctx_t *scp = dpmi_get_scp();
  __dpmi_regs regs = {};
  unsigned ret = 0;
  int is_32 = dpmi_is_32();
  int sel;
  int para = (count >> 4) + 1;
  int seg;

  if (count > 65536 - 16)
    return -1;
  seg = _dpmi_allocate_dos_memory(scp, is_32, para, &sel);
  if (seg == -1)
    return -1;

  regs.d.eax = 0x3f00;
  regs.d.ebx = handle;
  regs.d.ecx = count;
  regs.x.ds = seg;
  regs.d.edx = 0;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    ret = regs.d.eax;
  else {
    *numread = regs.d.eax;
    MEMCPY_2UNIX(buffer, SEGOFF2LINEAR(seg, 0), regs.d.eax);
  }
  _dpmi_free_dos_memory(scp, is_32, sel);

  return ret;
}

unsigned _dos_write(int handle, const void *buffer, unsigned count, unsigned *numwrt)
{
  cpuctx_t *scp = dpmi_get_scp();
  __dpmi_regs regs = {};
  unsigned ret = 0;
  int is_32 = dpmi_is_32();
  int sel;
  int para = (count >> 4) + 1;
  int seg;

  if (count > 65536 - 16)
    return -1;
  seg = _dpmi_allocate_dos_memory(scp, is_32, para, &sel);
  if (seg == -1)
    return -1;

  regs.d.eax = 0x4000;
  regs.d.ebx = handle;
  regs.d.ecx = count;
  regs.x.ds = seg;
  regs.d.edx = 0;
  MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0), buffer, count);
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    ret = regs.d.eax;
  else
    *numwrt = regs.d.eax;
  _dpmi_free_dos_memory(scp, is_32, sel);

  return ret;
}

unsigned long _dos_seek(int handle, unsigned long offset, int origin)
{
  cpuctx_t *scp = dpmi_get_scp();
  __dpmi_regs regs = {};
  unsigned ret = 0;
  int is_32 = dpmi_is_32();

  regs.d.eax = 0x4200 | (origin & 3);
  regs.d.ebx = handle;
  regs.d.ecx = offset >> 16;
  regs.d.edx = offset & 0xffff;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    ret = -1;
  else
    ret = (regs.d.edx << 16) | (regs.d.eax & 0xffff);

  return ret;
}

int _dos_close(int handle)
{
  cpuctx_t *scp = dpmi_get_scp();
  __dpmi_regs regs = {};
  unsigned ret = 0;
  int is_32 = dpmi_is_32();

  regs.d.eax = 0x3e00;
  regs.d.ebx = handle;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    ret = regs.d.eax;

  return ret;
}

int _dos_link_umb(int on)
{
  cpuctx_t *scp = dpmi_get_scp();
  __dpmi_regs regs = {};
  int is_32 = dpmi_is_32();

  regs.d.eax = 0x5803;
  regs.d.ebx = on;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    return regs.d.eax;
  regs.d.eax = 0x5801;
  regs.d.ebx = on ? 0x80 : 0;
  _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &regs);
  if (regs.x.flags & CF)
    return regs.d.eax;

  return 0;
}
