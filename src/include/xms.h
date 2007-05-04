/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* this is for the XMS support */
#ifndef XMS_H
#define XMS_H

#include "extern.h"

#define INT2F_XMS_MAGIC		0x0043	/* AH for all int 2f XMS calls */
#define XMS_VERSION    		0x0300	/* version 3.00 */
#define XMS_DRIVER_VERSION	0x0301	/* my driver version 3.01 */

#define NUM_HANDLES     64
#define FIRST_HANDLE    1

#define PARAGRAPH       16	/* bytes in a paragraph */

/* the NEWXMS API duplicates some functions for > 64 MB range (32-bit) */
#define OLDXMS          1
#define NEWXMS          2

struct EMM {
   unsigned long Length;
   unsigned short SourceHandle;
   unsigned long SourceOffset __attribute__((packed));
   unsigned short DestHandle __attribute__((packed));
   unsigned long DestOffset __attribute__((packed));
} ;

struct Handle {
  unsigned short int num;
  char *addr;
  unsigned long size;
  int valid;
  int lockcount;
};

struct UMB {
  unsigned short segment, size;
  int used;
};

void xms_init(void);
void xms_reset(void);
void xms_helper(void);

#endif /* XMS_H */
