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

static void call_msdos(cpuctx_t *scp)
{
    struct pmaddr_s pma = {
	.offset = DPMI_SEL_OFF(DPMI_msdos),
	.selector = dpmi_sel(),
    };
    unsigned int *ssp = SEL_ADR(_ss, _esp);
    *--ssp = _cs;
    *--ssp = _eip;
    _esp -= 8;

    _cs = pma.selector;
    _eip = pma.offset;
    coopth_sched();
}

unsigned _dos_open(const char *pathname, unsigned mode, int *handle)
{
  cpuctx_t *scp = dpmi_get_scp();
  cpuctx_t saved_scp = *scp;
  int len = strlen(pathname) + 1;
  __dpmi_paddr nm = dapi_alloc(len);
  unsigned ret = 0;

  _eax = 0x3d00 | (mode & 0xff);
  _ecx = 0;
  _ds = nm.selector;
  _edx = nm.offset32;
  memcpy(SEL_ADR(_ds, _edx), pathname, len);
  call_msdos(scp);
  if (_eflags & CF)
    ret = _eax;
  else
    *handle = _eax;

  dapi_free(nm);
  *scp = saved_scp;
  return ret;
}

unsigned _dos_read(int handle, void *buffer, unsigned count, unsigned *numread)
{
  cpuctx_t *scp = dpmi_get_scp();
  cpuctx_t saved_scp = *scp;
  __dpmi_paddr buf = dapi_alloc(count);
  unsigned ret = 0;

  _eax = 0x3f00;
  _ebx = handle;
  _ecx = count;
  _ds = buf.selector;
  _edx = buf.offset32;
  call_msdos(scp);
  if (_eflags & CF)
    ret = _eax;
  else {
    *numread = _eax;
    memcpy(buffer, SEL_ADR(_ds, _edx), count);
  }

  dapi_free(buf);
  *scp = saved_scp;
  return ret;
}

unsigned _dos_write(int handle, const void *buffer, unsigned count, unsigned *numwrt)
{
  cpuctx_t *scp = dpmi_get_scp();
  cpuctx_t saved_scp = *scp;
  __dpmi_paddr buf = dapi_alloc(count);
  unsigned ret = 0;

  _eax = 0x4000;
  _ebx = handle;
  _ecx = count;
  _ds = buf.selector;
  _edx = buf.offset32;
  memcpy(SEL_ADR(_ds, _edx), buffer, count);
  call_msdos(scp);
  if (_eflags & CF)
    ret = _eax;
  else
    *numwrt = _eax;

  dapi_free(buf);
  *scp = saved_scp;
  return ret;
}

unsigned long _dos_seek(int handle, unsigned long offset, int origin)
{
  cpuctx_t *scp = dpmi_get_scp();
  cpuctx_t saved_scp = *scp;
  unsigned ret = 0;

  _eax = 0x4200 | (origin & 3);
  _ebx = handle;
  _ecx = offset >> 16;
  _edx = offset & 0xffff;
  call_msdos(scp);
  if (_eflags & CF)
    ret = -1;
  else
    ret = (_edx << 16) | (_eax & 0xffff);

  *scp = saved_scp;
  return ret;
}

int _dos_close(int handle)
{
  cpuctx_t *scp = dpmi_get_scp();
  cpuctx_t saved_scp = *scp;
  unsigned ret = 0;

  _eax = 0x3e00;
  _ebx = handle;
  call_msdos(scp);
  if (_eflags & CF)
    ret = _eax;

  *scp = saved_scp;
  return ret;
}

int _dos_link_umb(int on)
{
  cpuctx_t *scp = dpmi_get_scp();
  cpuctx_t saved_scp = *scp;
  unsigned ret = 0;

  _eax = 0x5803;
  _ebx = on;
  call_msdos(scp);
  if (_eflags & CF)
    ret = _eax;

  *scp = saved_scp;
  return ret;
}
