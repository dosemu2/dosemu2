#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#ifndef BLKGETSIZE
   /* Note: _IO() being defined in <sys/ioctl.h> */
   #define BLKGETSIZE _IO(0x12,96) /* return device size */
#endif

#endif
