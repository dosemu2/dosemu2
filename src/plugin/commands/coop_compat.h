#ifndef __COOP_COMPAT_H
#define __COOP_COMPAT_H

struct coop_com_header {
	unsigned char	jumparound[3];	/* jmp start*/
	unsigned char	mode_flags;	/* 0x103 */
	#define COM_MAGIC   0xd0c0
	unsigned short	magic;		/* 0x104 */
	unsigned short	heapstart;	/* 0x106 */
} __attribute__((packed));

#endif
