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
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include "meminfo.h"
#include "emu.h"

/**************************************************/
struct meminfo *readMeminfo(void) {
/* Read /proc/meminfo and return the new values */
  static struct meminfo mem = { 0 };
  char buf[4097];
  char *tok, *nextline;
  int cnt;

  if (mem.meminfofd <= 0)
    mem.meminfofd = open("/proc/meminfo", O_RDONLY);
  else
    lseek(mem.meminfofd, 0, SEEK_SET);
  if (mem.meminfofd <= 0) {
    error("Cannot open /proc/meminfo\n");
    return;
  }
  cnt = read(mem.meminfofd, buf, 4096);
  if (cnt <= 0) {
    error("Read failure on /proc/meminfo");
    return;
  }
  buf[cnt] = '\0';

/* Skip header line */
  if ((tok = strtok(buf, "\r\n")) == NULL) {
BadFormat:
    error("Unknown format on /proc/meminfo\n");
    return;
  }
  
/* Read meminfo */
  if ((tok = strtok(NULL, "\r\n")) == NULL)
    goto BadFormat;
  nextline = tok + strlen(tok) + 1;

  tok = strtok(tok, " \t\r\n");
  if ((tok == NULL) || strcmp(tok, "Mem:"))
    goto BadFormat;

  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.total = atoi(tok);
  else
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.used = atoi(tok);
  else
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.free = atoi(tok);
  else
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.shared = atoi(tok);
  else
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.buffers = atoi(tok);
  else
    goto BadFormat;

/* Read swap information */
  tok = strtok(nextline, " \t\r\n");
  if ((tok == NULL) || strcmp(tok, "Swap:"))
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.swaptotal = atoi(tok);
  else
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.swapused = atoi(tok);
  else
    goto BadFormat;
  if ((tok = strtok(NULL, " \t\r\n")) != NULL)
    mem.swapfree = atoi(tok);
  else
    goto BadFormat;

  mem.totmegs = (mem.total / (1024 * 1024)) + 1;
  mem.swapmegs = (mem.swaptotal / (1024 * 1024)) + 1;
  mem.bytes_per_tick = mem.total >> 8; 
  mem.swapbytes_per_tick = mem.swaptotal >> 8; 

  return(&mem);
}
/**************************************************/
