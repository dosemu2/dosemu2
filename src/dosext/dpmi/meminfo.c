/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Linux meminfo, using /proc - addapted for dosemu
 * Copyright (C) 1993 Kenneth Osterberg, 1994 Lutz Molgedey
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include "meminfo.h"
#include "emu.h"

/**************************************************/
static int getvalue_by_key(char *buf, char *key)
{
  char *p = strstr(buf,key);
  int val;

  if (p) {
    if (sscanf(p, "%*[^ ]%d",&val) == 1) return val;
  }
  error("Unknown format on /proc/meminfo\n");
  leavedos(5);
  return -1; /* just to make GCC happy */
}


struct meminfo *readMeminfo(void) {
/* Read /proc/meminfo and return the new values
 *
 * linux <  2.1.41 has part A + B
 * linux >= 2.1.41 has only part B
 * linux >= 2.5.x does not have the "MemShared" value
 * for compatibility reasons we only use Part B
 *
 * Part A:
        total:    used:    free:  shared: buffers:  cached:
Mem:  64622592 61927424  2695168 41209856  7151616 19976192
Swap: 70184960    57344 70127616
 *
 * Part B:
MemTotal:     63108 kB
MemFree:       2632 kB
MemShared:    40244 kB
Buffers:       6984 kB
Cached:       19508 kB
SwapTotal:    68540 kB
SwapFree:     68484 kB
 *
 */
  static struct meminfo mem = { 0 };
#ifdef __linux__
  char buf[4097];
  int cnt;

  if (mem.meminfofd <= 0) {
    mem.meminfofd = open("/proc/meminfo", O_RDONLY);
    if (mem.meminfofd <= 0) {
      error("Cannot open /proc/meminfo\n");
      leavedos(5);
    }
  }
  else
    lseek(mem.meminfofd, 0, SEEK_SET);

  cnt = read(mem.meminfofd, buf, 4096);
  if (cnt <= 0) {
    error("Read failure on /proc/meminfo");
    leavedos(5);
  }
  buf[cnt] = '\0';
  mem.total = getvalue_by_key(buf, "MemTotal:") <<10;
  mem.free = getvalue_by_key(buf, "MemFree:") <<10;
  mem.used = mem.total - mem.free;
  mem.buffers = getvalue_by_key(buf, "Buffers:") <<10;
  mem.cached = getvalue_by_key(buf, "Cached:") <<10;
  mem.swaptotal = getvalue_by_key(buf, "SwapTotal:") <<10;
  mem.swapfree = getvalue_by_key(buf, "SwapFree:") <<10;
  mem.swapused = mem.swaptotal - mem.swapfree;
  mem.totmegs = (mem.total >>20) +1;
  mem.swapmegs = (mem.swaptotal>>20) +1;
  mem.bytes_per_tick = mem.total >> 8;
  mem.swapbytes_per_tick = mem.swaptotal >> 8;
#endif
  return(&mem);
}
/**************************************************/
