/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef MEMINFO_H
#define MEMINFO_H

struct meminfo {
  int meminfofd;
  int total;
  int used;
  int free;
  int shared;
  int buffers;
  int cached;
  int swaptotal;
  int swapused;
  int swapfree;
  int totmegs;
  int swapmegs;
  int bytes_per_tick;
  int swapbytes_per_tick;
};

struct meminfo *readMeminfo(void);

#endif
