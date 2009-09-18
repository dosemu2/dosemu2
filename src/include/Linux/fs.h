#ifndef _LINUX_FS_H
#define _LINUX_FS_H

/* Note: _IO()/_IOR being defined in <sys/ioctl.h> */
#ifndef BLKGETSIZE
   #define BLKGETSIZE _IO(0x12,96) /* return device size */
#endif
#ifndef BLKGETSIZE64
   #define BLKGETSIZE64 _IOR(0x12,114,size_t) /* return device size in bytes */
#endif
#ifndef BLKSSZGET
   #define BLKSSZGET _IO(0x12,104)/* get block device sector size */
#endif

#endif
