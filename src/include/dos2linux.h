/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* dos2linux.h
 * 
 * Function prototypes for the DOSEMU/LINUX interface
 *
 */

#ifndef DOS2LINUX_H
#define DOS2LINUX_H

#include "cpu.h"

struct MCB {
	char id;			/* 0 */
	unsigned short owner_psp;	/* 1 */
	unsigned short size;		/* 3 */
	char align8[3];			/* 5 */
	char name[8];			/* 8 */
} __attribute__((packed));

struct PSP {
	unsigned short	opint20;	/* 0x00 */
	unsigned short	memend_frame;	/* 0x02 */
	unsigned char	dos_reserved4;	/* 0x04 */
	unsigned char	cpm_function_entry[0xa-0x5];	/* 0x05 */
	FAR_PTR		int22_copy;	/* 0x0a */
	FAR_PTR		int23_copy;	/* 0x0e */
	FAR_PTR		int24_copy;	/* 0x12 */
	unsigned short	parent_psp;	/* 0x16 */
	unsigned char	file_handles[20];	/* 0x18 */
	unsigned short	envir_frame;	/* 0x2c */
	FAR_PTR		system_stack;	/* 0x2e */
	unsigned short	max_open_files;	/* 0x32 */
	FAR_PTR		file_handles_ptr;	/* 0x34 */
	unsigned char	dos_reserved38[0x50-0x38];	/* 0x38 */
	unsigned char	high_language_dos_call[0x53-0x50];	/* 0x50 */
	unsigned char	dos_reserved53[0x5c-0x53];	/* 0x53 */
	unsigned char	FCB1[0x6c-0x5c];	/* 0x5c */
	unsigned char	FCB2[0x80-0x6c];	/* 0x6c */
	unsigned char	cmdline_len;		/* 0x80 */
	unsigned char	cmdline[0x100-0x81];	/* 0x81 */
} __attribute__((packed));

struct lowstring {
	unsigned char len;
	char s[0];
} __attribute__((packed));

extern int misc_e6_envvar (char *str);

extern int misc_e6_commandline (char *str);
extern void misc_e6_store_command (char *str, int terminate);

extern void run_unix_command (char *buffer);
extern int run_system_command(char *buffer);

#endif /* DOS2LINUX_H */
