/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
adapted from dos.h in the mach dos emulator for the linux dosemu dos
emulator.
Andrew.Tridgell@anu.edu.au 30th March 1993
*/

#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>

/* definitions to make mach emu code compatible with dosemu */
#include "emu.h"

#if 0
typedef unsigned char boolean_t;
#endif

#define direct dirent
#ifdef __linux__
#define d_namlen d_reclen
#endif

#ifndef MAX_DRIVE
#define MAX_DRIVE 33
#endif

#define USE_DF_AND_AFS_STUFF

#ifdef __linux__
#define VOLUMELABEL "Linux"
#endif

#define LINUX_RESOURCE "\\\\LINUX\\FS"

#define FALSE 0
#define TRUE 1
#define UNCHANGED 2
#define REDIRECT 3

#define us_debug_level 10
#define Debug_Level_0 0
#define dbg_fd -1

#define d_Stub(arg1, s, a...)   d_printf("MFS: "s, ##a)
#define Debug0(args)		d_Stub args
#define Debug1(args)		d_Stub args

#if 0
typedef struct vm86_regs state_t;
#endif

#define Addr_8086(x,y)	(( ((x) & 0xffff) << 4) + ((y) & 0xffff))
#define Addr(s,x,y)	Addr_8086(((s)->x), ((s)->y))
#define MASK8(x)	((x) & 0xff)
#define MASK16(x)	((x) & 0xffff)
#define HIGH(x)		MASK8((unsigned long)(x) >> 8)
#define LOW(x)		MASK8((unsigned long)(x))
#undef WORD
#define WORD(x)		MASK16((unsigned long)(x))
#define SETHIGH(x,y) 	(*(x) = (*(x) & ~0xff00) | ((MASK8(y))<<8))
#define SETLOW(x,y) 	(*(x) = (*(x) & ~0xff) | (MASK8(y)))
#define SETWORD(x,y)	(*(x) = (*(x) & ~0xffff) | (MASK16(y)))

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *
 * Purpose:
 *	V86 DOS disk emulation header file
 *
 * HISTORY:
 *
 * Log: mfs.h,v
 * Revision 1.1  2003/06/23 00:02:08  bartoldeman
 * Initial revision
 *
 * Revision 1.2  1995/05/23  06:04:49  root
 * fix for redirector open existing file function
 *
 * Revision 2.3  1995/04/08  22:33:34  root
 * Release dosemu0.60.0
 *
 * Revision 2.3  1995/04/08  22:33:34  root
 * Release dosemu0.60.0
 *
 * Revision 2.2  1995/02/25  22:38:15  root
 * *** empty log message ***
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.7  1994/03/13  01:07:31  root
 * Poor attempt to optimize.
 *
 * Revision 1.6  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout.
 *
 * Revision 1.5  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.4  1994/01/19  17:51:14  root
 * Added CDS_FLAG_NOTNET = 0x80 for mfs.c
 *
 * Revision 1.3  1993/12/22  11:45:36  root
 * Fixes for ftruncate
 *
 * Revision 1.2  1993/11/17  22:29:33  root
 * *** empty log message ***
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.3  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.2  1993/04/07  21:04:26  root
 * big move
 *
 * Revision 1.1  1993/04/05  17:25:13  root
 * Initial revision
 *
 * Revision 2.3  91/12/06  15:29:23  grm
 * 	Redefined sda_cur_psp, and added psp_parent_psp.  The
 * 	psp_parent_psp was used to find out what the psp of the parent of
 * 	the command.com program was.  It seems that it is undefined.
 * 	[91/12/06            grm]
 *
 * Revision 2.2  91/12/05  16:42:08  grm
 * 	Added sft_rel and _abs_cluster macros.  Used to debug the
 * 	MS-Write network drive problem.
 * 	[91/12/04            grm]
 * 	Added constants for Dos 4+
 * 	[91/07/16  17:47:20  grm]
 *
 * 	Changed to allow for usage with Dos v4.01 and 5.00.
 * 	[91/06/28  18:53:42  grm]
 *
 * 	New Copyright
 * 	[91/05/28  15:12:28  grm]
 *
 * 	Added structures for the dos_general routines.
 * 	[91/04/30  13:43:58  grm]
 *
 * 	Structures and macros for the dos_fs.c
 * 	network redirector interface.
 * 	[91/04/30  13:36:42  grm]
 *
 * 	Name changed from dos_general.h to dos.h.
 * 	Added external declarations for dos_foo.c files.
 * 	[91/03/01  14:40:55  grm]
 *
 * 	Type works.  Interrim changes.
 * 	[91/02/11  18:22:47  grm]
 *
 * 	Fancy dir.
 * 	[91/02/06  16:59:01  grm]
 *
 * 	Created.
 * 	[91/02/06  14:29:39  grm]
 *
 */

#include <sys/types.h>

/*
 * Dos error codes
 */
/* MS-DOS version 2 error codes */
#define FUNC_NUM_IVALID		0x01
#define FILE_NOT_FOUND		0x02
#define PATH_NOT_FOUND		0x03
#define TOO_MANY_OPEN_FILES	0x04
#define ACCESS_DENIED		0x05
#define HANDLE_INVALID		0x06
#define MEM_CB_DEST		0x07
#define INSUF_MEM		0x08
#define MEM_BLK_ADDR_IVALID	0x09
#define ENV_INVALID		0x0a
#define FORMAT_INVALID		0x0b
#define ACCESS_CODE_INVALID	0x0c
#define DATA_INVALID		0x0d
#define UNKNOWN_UNIT		0x0e
#define DISK_DRIVE_INVALID	0x0f
#define ATT_REM_CUR_DIR		0x10
#define NOT_SAME_DEV		0x11
#define NO_MORE_FILES		0x12
/* mappings to critical-error codes */
#define WRITE_PROT_DISK		0x13
#define UNKNOWN_UNIT_CERR	0x14
#define DRIVE_NOT_READY		0x15
#define UNKNOWN_COMMAND		0x16
#define DATA_ERROR_CRC		0x17
#define BAD_REQ_STRUCT_LEN	0x18
#define SEEK_ERROR		0x19
#define UNKNOWN_MEDIA_TYPE	0x1a
#define SECTOR_NOT_FOUND	0x1b
#define PRINTER_OUT_OF_PAPER	0x1c
#define WRITE_FAULT		0x1d
#define READ_FAULT		0x1e
#define GENERAL_FAILURE		0x1f

/* MS-DOS version 3 and later extended error codes */
#define SHARING_VIOLATION	0x20
#define FILE_LOCK_VIOLATION	0x21
#define DISK_CHANGE_INVALID	0x22
#define FCB_UNAVAILABLE		0x23
#define SHARING_BUF_EXCEEDED	0x24

#define NETWORK_NAME_NOT_FOUND	0x35

#define FILE_ALREADY_EXISTS	0x50

#define DUPLICATE_REDIR		0x55

/* Something seems to depend on this structure being no more than 32
   bytes, otherwise dosemu crashes. Why? /MB */
struct dir_ent {
  char name[8];			/* dos name and ext */
  char ext[3];
  char d_name[256];             /* unix name as in readdir */
  u_short mode;			/* unix st_mode value */
  u_short hidden;
  u_short long_path;            /* directory has long path */
  int size;			/* size of file */
  time_t time;			/* st_mtime */
  int attr;
};

struct dir_list {
  int nr_entries;
  int size;
  struct dir_ent *de;
};

struct dos_name {
  char name[8];
  char ext[3];
};

struct mfs_dirent
{
  char *d_name;
  char *d_long_name;
};

struct mfs_dir
{
  DIR *dir;
  struct mfs_dirent de;
  int fd;
  unsigned int nr;
};

#define FAR(x) (Addr_8086(x.segment, x.offset))
#define FARPTR(x) (Addr_8086((x)->segment, (x)->offset))

typedef u_short *psp_t;

#define PSPPTR(x) (Addr_8086(x, 0))

struct drive_info 
{
  char *root;
  int root_len;
  boolean_t read_only;
};
extern struct drive_info drives[MAX_DRIVE];

/* dos attribute byte flags */
#define REGULAR_FILE 	0x00
#define READ_ONLY_FILE	0x01
#define HIDDEN_FILE	0x02
#define SYSTEM_FILE	0x04
#define VOLUME_LABEL	0x08
#define DIRECTORY	0x10
#define ARCHIVE_NEEDED	0x20

/* dos access mode constants */
#define READ_ACC	0x00
#define WRITE_ACC	0x01
#define READ_WRITE_ACC	0x02

#define COMPAT_MODE	0x00
#define DENY_ALL	0x01
#define DENY_WRITE	0x02
#define DENY_READ	0x03
#define DENY_NONE	0x04
#define FCB_MODE	0x07

#define CHILD_INHERIT	0x00
#define NO_INHERIT	0x01

#define A_DRIVE		0x01
#define B_DRIVE		0x02
#define C_DRIVE		0x03
#define D_DRIVE		0x04

#define GET_REDIRECTION	2
#define REDIRECT_DEVICE 3
#define CANCEL_REDIRECTION 4
#define EXTENDED_GET_REDIRECTION 5


/* #define MAX_PATH_LENGTH 57 */
/* 2001/01/05 Manfred Scherer
 * With the value 57 I can create pathlength until 54.
 * In native DOS I can create pathlength until 63.
 * With the value 66 it should be possible to create
 * paths with length 63.
 * I've tested it on my own system, and I found the value 66
 * is right for me.
 */

#define MAX_PATH_LENGTH 66


extern int build_ufs_path_(char *ufs, const char *path, int drive,
                           int lowercase);
extern boolean_t find_file(char *fpath, struct stat *st, int drive);
extern boolean_t is_hidden(char *fname);
extern int get_dos_attr(const char *fname,int mode,boolean_t hidden);
extern int set_fat_attr(int fd,int attr);
extern int set_dos_attr(char *fname,int mode,int attr);
extern int dos_utime(char *fpath, struct utimbuf *ut);
extern int get_unix_attr(int mode, int attr);
extern void time_to_dos(time_t clock, u_short *date, u_short *time);
extern time_t time_to_unix(u_short dos_date, u_short dos_time);
extern void extract_filename(const char *filestring0, char *name, char *ext);
extern struct mfs_dir *dos_opendir(const char *name);
extern struct mfs_dirent *dos_readdir(struct mfs_dir *);
extern int dos_closedir(struct mfs_dir *dir);
extern void get_volume_label(char *fname, char *fext, char *lfn, int drive);
extern int dos_rename(const char *filename1, const char *filename2, int drive, int lfn);
extern int dos_mkdir(const char *filename, int drive, int lfn);
extern int dos_rmdir(const char *filename, int drive, int lfn);

extern void register_cdrom(int drive, int device);
extern void unregister_cdrom(int drive);
extern int get_volume_label_cdrom(int drive, char *name);
