/*
 * ldt.h
 *
 * Definitions of structures used with the modify_ldt system call.
 */
#ifndef _LINUX_LDT_H
#define _LINUX_LDT_H

/* Maximum number of LDT entries supported. */
#define LDT_ENTRIES	8192
/* The size of each LDT entry. */
#define LDT_ENTRY_SIZE	8

struct modify_ldt_ldt_s {
	unsigned int  entry_number;
	unsigned long base_addr;
	unsigned int  limit;
	unsigned int  seg_32bit:1;
	unsigned int  contents:2;
	unsigned int  read_exec_only:1;
	unsigned int  limit_in_pages:1;
	unsigned int  seg_not_present:1;
#ifdef WANT_WINDOWS
	unsigned int  useable:1;
#endif
};

#define MODIFY_LDT_CONTENTS_DATA	0
#define MODIFY_LDT_CONTENTS_STACK	1
#define MODIFY_LDT_CONTENTS_CODE	2

extern int get_ldt(void *buffer);
extern int set_ldt_entry(int entry, unsigned long base, unsigned int limit,
			 int seg_32bit_flag, int contents, int read_only_flag,
#ifndef WANT_WINDOWS
			 int limit_in_pages_flag);
#else
			 int limit_in_pages_flag, int seg_not_present_flag, int useable);
#endif

#endif
