/* this is for the XMS support */
#ifndef XMS_H
#define XMS_H

#define INT2F_XMS_MAGIC		0x0043	/* AH for all int 2f XMS calls */
#define XMS_VERSION    		0x0300	/* version 3.00 */
#define XMS_DRIVER_VERSION	0x0003	/* my driver version 0.03 */

#define NUM_HANDLES     64
#define FIRST_HANDLE    1

#define PARAGRAPH       16	/* bytes in a paragraph */

/* the NEWXMS API duplicates some functions for > 64 MB range (32-bit) */
#define OLDXMS          1
#define NEWXMS          2

struct EMM {
  unsigned long int Length;
  unsigned short int SourceHandle;
  unsigned long int SourceOffset;
  unsigned short int DestHandle;
  unsigned long int DestOffset;
};

struct Handle {
  unsigned short int num;
  unsigned char *addr;
  unsigned long size;
  int valid;
  int lockcount;
};

struct UMB {
  unsigned short segment, size;
  int used;
};

void xms_control(void), xms_int15(void), xms_init(void);

#endif /* XMS_H */
