/* this is for the XMS support */
#ifndef XMS_H
#define XMS_H

#define XMSControl_SEG  0xe000
#define XMSControl_OFF  0x1000
#define XMSControl_ADD  ((XMSControl_SEG << 4)+XMSControl_OFF)
#define XMSTrap_ADD     ((XMSControl_SEG << 4)+XMSControl_OFF+5)

#define XMS_MAGIC      		0x0043
#define XMS_VERSION    		0x0300  /* version 3.00 */
#define XMS_DRIVER_VERSION	0x0002  /* my driver version 0.01 */

#define NUM_HANDLES     32
#define FIRST_HANDLE    1

#define PARAGRAPH       16		/* bytes in a paragraph */

struct EMM {
  unsigned long  int  Length;
  unsigned short int  SourceHandle;
  unsigned long  int  SourceOffset;
  unsigned short int  DestHandle;
  unsigned long  int  DestOffset;
};

struct Handle {
  unsigned short int  num;
  unsigned char      *addr;
  unsigned long       size;
  int valid;
  int lockcount;
};

struct UMB {
  unsigned short segment, size;
  int used;
};

void xms_control(void),
     xms_int15(void),
     xms_init(void);

#endif /* XMS_H */
