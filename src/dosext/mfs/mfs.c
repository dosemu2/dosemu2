/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * This is the file redirector code for DOSEMU. It was built on the Mach
 * DOS redirector and as such continues that copyright as well in 
 * addition the GNU copyright. This redirector uses the 
 * DOS int2f fnx 11 calls to give running DOS programs access to any 
 * Unix mounted drives that permissions exist for.
 *
 * /REMARK
 * DANG_END_MODULE
 * DANG_FIXTHIS We probably should use llseek here for file > 2 GBytes
 *
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
 *
 * Purpose:
 *	V86 DOS disk emulation routines
 *

Work started by Tim Bird (tbird@novell.com) 28th October 1993

	
	added support for CONTROL_REDIRECTION in dos_fs_redirect.
	removed my_drive, and made drive arrays 0-based instead of
	first_drive based.


This module is a port of the redirector handling in the mach dos
emulator to the linux dos emulator. Started by
Andrew.Tridgell@anu.edu.au on 30th March 1993

version 1.4 12th May 1993

      added a cds_flags check in select_drive so the pointers can
      be recalculated when the cds moves. This means you can use the
      emufs drive immediately after it installs, even using it to load
      other device drivers in config.sys

      fixed the volume label bug, so now volume labels are enabled by default.
      We really should do something more useful with them.

      ran "indent" on mfs.c so emacs wouldn't choke in C mode

      did a quick fix for nested finds so they work if the inner find
      didn't finish.  I don't think it's quite right yet, probably
      find_first/next need to be completely rewritten to get perfect
      behaviour, especially for programs that may try to hack directly with
      the search template/directory entry during a find.

      added Roberts patches so the root directory gets printed when initialised

version 1.3 9th may 1993

      completely revamped find_file() so it can match mixed case names.
      This doesn't seem to have cost as much in speed as I thought
      it would. This actually involved changes to quite a few routines for
      it to work properly.

      added a check for the dos root passed to init_drive - you now get
      a "server not responding" error with a bad path - this is still not good
      but's it's better than just accepting it.

version 1.2 8th may 1993

      made some changes to get_dir() which make the redirector MUCH faster.
      It no longer stat's every file when searching for a file. This
      is most noticible when you have a long dos path.

      also added profiling. This is supported by profile.c and profile.h.

version 1.2 7th may 1993

      more changes from Stephen Tweedie to fix the dos root length
      stuff, hopefully it will now work equally well with DRDOS and MSDOS.

      also change the way the calculate_dos_pointers() works - it's
      now done drive by drive which should be better

      also fixed for MS-DOS 6. It turned out that the problem was that
      an earlier speculative dos 6 compatability patch I did managed to get
      lost in all the 0.49v? ungrades. re-applying it fixed the problem.
      *grumble*. While I was doing that I fixed up non-cds filename finding
      (which should never be used anyway as we have a valid cds), and added
      a preliminary qualify_filename (which is currently turned off
      as it's broken)

version 1.1 1st May 1993

      incorporated patches from Stephen Tweedie to
      fix things for DRDOS and to make the file attribute handling
      a lot saner. (thanks Stephen!)

      The main DRDOS change is to handle the fact that filenames appear
      to be in <DRIVE><PATH> format rather than <DRIVE>:\<PATH>. Hopefully
      the changes will not affect the operation under MS-DOS

version 1.0 17th April 1993
changes from mach dos emulator version 2.5:

   - changed SEND_FROM_EOF to SEEK_FROM_EOF and added code (untested!)
   - added better attribute mapping and set attribute capability
   - remove volume label to prevent "dir e:" bug
   - made get_attribute return file size
   - added multiple drive capability via multiple .sys files in config.sys
   - fixed so compatible with devlod so drives can be added on the fly
   - allow unix directory to be chosen with command line in config.sys
   - changed dos side (.asm) code to only use 1 .sys file (removed
     need for mfsini.exe)
   - ported linux.asm to use as86 syntax (Robert Sanders did this - thanks!)
   - added read-only option on drives
   - totally revamped drive selection to match Ralf's interrupt list
   - made reads and writes faster by not doing 4096 byte chunks
   - masked sigalrm in read and write to prevent crash under NFS
   - gave delete_file it's own hlist to prevent clashes with find_next

TODO:
   - fix volume labels
   - get filename qualification working properly
   - anything else???

*/

/* Modified by O.V.Zhirov at July 1998    */

#include "config.h"

#if defined(__linux__)
#define DOSEMU 1		/* this is a port to dosemu */
#endif

#if !DOSEMU
#include "base.h"
#include "bios.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <utime.h>
#include <wchar.h>

#if !DOSEMU
#include <mach/message.h>
#include <mach/exception.h>

#include "dos.h"
#else
#include <dirent.h>
#include <string.h>
#include <wctype.h>
#include "int.h"
#include "mfs.h"
#include "lfn.h"
#include "dos2linux.h"
/* For passing through GetRedirection Status */
#include "memory.h"
#include "redirect.h"
#include "mangle.h"
#include "utilities.h"
#include "cpu-emu.h"
#endif

#ifdef __linux__
/* we need to use the kernel dirent structure for the VFAT ioctls */
struct kernel_dirent {
  long		  d_ino;
  long		  d_off;
  unsigned short  d_reclen;
  char		  d_name[256]; /* We must not include limits.h! */
};
#define VFAT_IOCTL_READDIR_BOTH	 _IOR('r', 1, struct kernel_dirent [2])
#define VFAT_IOCTL_READDIR_SHORT _IOR('r', 2, struct kernel_dirent [2])
/* vfat_ioctl to use is short for int2f/ax=11xx, both for int21/ax=71xx */
static long vfat_ioctl = VFAT_IOCTL_READDIR_BOTH;
#define FAT_IOCTL_GET_ATTRIBUTES _IOR('r', 0x10, uint32_t)
#define FAT_IOCTL_SET_ATTRIBUTES _IOW('r', 0x11, uint32_t)
#ifndef MSDOS_SUPER_MAGIC
#define MSDOS_SUPER_MAGIC     0x4d44
#endif
#endif

/* these universal globals defined here (externed in dos.h) */
boolean_t mach_fs_enabled = FALSE;

#define INSTALLATION_CHECK	0x0
#define	REMOVE_DIRECTORY	0x1
#define	REMOVE_DIRECTORY_2	0x2
#define	MAKE_DIRECTORY		0x3
#define	MAKE_DIRECTORY_2	0x4
#define	SET_CURRENT_DIRECTORY	0x5
#define	CLOSE_FILE		0x6
#define	COMMIT_FILE		0x7
#define	READ_FILE		0x8
#define	WRITE_FILE		0x9
#define	LOCK_FILE_REGION	0xa
#define	UNLOCK_FILE_REGION	0xb
#define	GET_DISK_SPACE		0xc
#define	SET_FILE_ATTRIBUTES	0xe
#define	GET_FILE_ATTRIBUTES	0xf
#define RENAME_FILE		0x11
#define	DELETE_FILE		0x13
#define	OPEN_EXISTING_FILE	0x16
#define	CREATE_TRUNCATE_FILE	0x17
#define	CREATE_TRUNCATE_NO_CDS	0x18
#define	FIND_FIRST_NO_CDS	0x19
#define	FIND_FIRST		0x1b
#define	FIND_NEXT		0x1c
#define	CLOSE_ALL		0x1d
#define	CONTROL_REDIRECT	0x1e
#define PRINTER_SETUP		0x1f
#define	FLUSH_ALL_DISK_BUFFERS	0x20
#define	SEEK_FROM_EOF		0x21
#define	PROCESS_TERMINATED	0x22
#define	QUALIFY_FILENAME	0x23
/*#define TURN_OFF_PRINTER	0x24 */
#define MULTIPURPOSE_OPEN	0x2e	/* Used in DOS 4.0+ */
#define PRINTER_MODE  		0x25	/* Used in DOS 3.1+ */
#define EXTENDED_ATTRIBUTES	0x2d	/* Used in DOS 4.x */

#define EOS		'\0'
#define	SLASH		'/'
#define BACKSLASH	'\\'

#define SFT_MASK 0x0060
#define SFT_FSHARED 0x8000
#define SFT_FDEVICE 0x0080
#define SFT_FEOF 0x0040

struct file_fd
{
  char *name;
  int fd;
};

/* Need to know how many drives are redirected */
static u_char redirected_drives = 0;
struct drive_info drives[MAX_DRIVE];

static int calculate_drive_pointers(int);
static boolean_t dos_fs_dev(state_t *);
static boolean_t compare(char *, char *, char *, char *);
static int dos_fs_redirect(state_t *);
static int is_long_path(const char *s);
static void path_to_ufs(char *ufs, size_t ufs_offset, const char *path,
                        int PreserveEnvVar, int lowercase);
static boolean_t dos_would_allow(char *fpath, const char *op, boolean_t equal);

static boolean_t drives_initialized = FALSE;

static struct file_fd open_files[256];
static u_char first_free_drive = 0;
static int num_drives = 0;
static int process_mask = 0;

lol_t lol = NULL;
static far_t cdsfarptr;
cds_t cds_base;
sda_t sda;

static int dos_major;
static int dos_minor;

/* initialize 'em to 3.1 to 3.3 */

int sdb_drive_letter_off = 0x0;
int sdb_template_name_off = 0x1;
int sdb_template_ext_off = 0x9;
int sdb_attribute_off = 0xc;
int sdb_dir_entry_off = 0xd;
int sdb_p_cluster_off = 0xf;
int sdb_file_name_off = 0x15;
int sdb_file_ext_off = 0x1d;
int sdb_file_attr_off = 0x20;
int sdb_file_time_off = 0x2b;
int sdb_file_date_off = 0x2d;
int sdb_file_st_cluster_off = 0x2f;
int sdb_file_size_off = 0x31;

int sft_handle_cnt_off = 0x0;
int sft_open_mode_off = 0x2;
int sft_attribute_byte_off = 0x4;
int sft_device_info_off = 0x5;
int sft_dev_drive_ptr_off = 0x7;
int sft_fd_off = 0xb;
int sft_start_cluster_off = 0xb;
int sft_time_off = 0xd;
int sft_date_off = 0xf;
int sft_size_off = 0x11;
int sft_position_off = 0x15;
int sft_rel_cluster_off = 0x19;
int sft_abs_cluster_off = 0x1b;
int sft_directory_sector_off = 0x1d;
int sft_directory_entry_off = 0x1f;
int sft_name_off = 0x20;
int sft_ext_off = 0x28;

int cds_record_size = 0x51;
int cds_current_path_off = 0x0;
int cds_flags_off = 0x43;
int cds_DBP_pointer_off = 0x45;
int cds_cur_cluster_off = 0x49;
int cds_rootlen_off = 0x4f;

int sda_current_dta_off = 0xc;
int sda_cur_psp_off = 0x10;
int sda_cur_drive_off = 0x16;
int sda_filename1_off = 0x92;
int sda_filename2_off = 0x112;
int sda_sdb_off = 0x192;
int sda_cds_off = 0x26c;
int sda_search_attribute_off = 0x23a;
int sda_open_mode_off = 0x23b;
int sda_rename_source_off = 0x2b8;
int sda_user_stack_off = 0x250;

int lol_dpbfarptr_off = 0;
int lol_cdsfarptr_off = 0x16;
int lol_last_drive_off = 0x21;
int lol_nuldev_off = 0x22;
int lol_njoined_off = 0x34;

/*
 * These offsets only meaningful for DOS 4 or greater:
 */
int sda_ext_act_off = 0x2dd;
int sda_ext_attr_off = 0x2df;
int sda_ext_mode_off = 0x2e1;

/* here are the functions used to interface dosemu with the mach
   dos redirector code */

static int cds_drive(cds_t cds)
{
  ptrdiff_t cds_offset = cds - cds_base;
  int drive = cds_offset / cds_record_size;
  
  if (drive >= 0 && drive < MAX_DRIVE && cds_offset % cds_record_size == 0)
    return drive;
  else
    return -1;
}

/* Try and work out if the current command is for any of my drives */
static int
select_drive(state_t *state)
{
  int dd;
  boolean_t found = 0;
  boolean_t check_cds = FALSE;
  boolean_t check_dpb = FALSE;
  boolean_t check_esdi_cds = FALSE;
  boolean_t check_sda_ffn = FALSE;
  boolean_t check_always = FALSE;
  boolean_t check_dssi_fn = FALSE;
  boolean_t cds_changed = FALSE;

  cds_t sda_cds = sda_cds(sda);
  cds_t esdi_cds = (cds_t) Addr(state, es, edi);
  sft_t sft = (u_char *) Addr(state, es, edi);
  int fn = LOW(state->eax);

  cdsfarptr = lol_cdsfarptr(lol);
  cds_base = MK_FP32(cdsfarptr.segment, cdsfarptr.offset);

  Debug0((dbg_fd, "selecting drive fn=%x sda_cds=%p\n",
	  fn, (void *) sda_cds));

  switch (fn) {
  case INSTALLATION_CHECK:	/* 0x0 */
  case CONTROL_REDIRECT:	/* 0x1e */
    check_always = TRUE;
    break;
  case QUALIFY_FILENAME:	/* 0x23 */
    check_dssi_fn = TRUE;
    break;
  case REMOVE_DIRECTORY:	/* 0x1 */
  case REMOVE_DIRECTORY_2:	/* 0x2 */
  case MAKE_DIRECTORY:		/* 0x3 */
  case MAKE_DIRECTORY_2:	/* 0x4 */
  case SET_CURRENT_DIRECTORY:	/* 0x5 */
  case SET_FILE_ATTRIBUTES:	/* 0xe */
  case GET_FILE_ATTRIBUTES:	/* 0xf */
  case RENAME_FILE:		/* 0x11 */
  case DELETE_FILE:		/* 0x13 */
  case CREATE_TRUNCATE_FILE:	/* 0x17 */
  case FIND_FIRST:		/* 0x1b */
    check_cds = TRUE;
    break;

  case CLOSE_FILE:		/* 0x6 */
  case COMMIT_FILE:		/* 0x7 */
  case READ_FILE:		/* 0x8 */
  case WRITE_FILE:		/* 0x9 */
  case LOCK_FILE_REGION:	/* 0xa */
  case UNLOCK_FILE_REGION:	/* 0xb */
  case SEEK_FROM_EOF:		/* 0x21 */
    check_dpb = TRUE;
    break;

  case GET_DISK_SPACE:		/* 0xc */
    check_esdi_cds = TRUE;
    break;

  case OPEN_EXISTING_FILE:	/* 0x16 */
  case MULTIPURPOSE_OPEN:	/* 0x2e */
  case FIND_FIRST_NO_CDS:	/* 0x19 */
  case CREATE_TRUNCATE_NO_CDS:	/* 0x18 */
    check_sda_ffn = TRUE;
    break;

  default:
    check_cds = TRUE;
    break;
   
 /* The rest are unknown - assume check_cds */
 /*
	case CREATE_TRUNCATE_NO_DIR	0x18
	case FIND_NEXT		0x1c
	case CLOSE_ALL		0x1d
	case FLUSH_ALL_DISK_BUFFERS	0x20
	case PROCESS_TERMINATED	0x22
	case UNDOCUMENTED_FUNCTION_2	0x25
 */
  }

  /* re-init the cds stuff for any drive that I think is mine, but
	where the cds flags seem to be unset. This allows me to reclaim a
 	drive in the strange and unnatural case where the cds has moved. */
  for (dd = 0; dd < num_drives; dd++)
    if (drives[dd].root && (cds_flags(drive_cds(dd)) &
			  (CDS_FLAG_REMOTE | CDS_FLAG_READY)) !=
	(CDS_FLAG_REMOTE | CDS_FLAG_READY)) {
      calculate_drive_pointers(dd);
      cds_changed = TRUE;
    }
  /* try to convert any fatfs drives that did not fit in the CDS before */
  if (cds_changed)
    redirect_devices();

  if (check_always)
    found = 1;

  if (!found && check_cds) {
    char *fn1 = sda_filename1(sda);
    if (fn != SET_CURRENT_DIRECTORY &&
	strncasecmp(fn1, LINUX_RESOURCE, strlen(LINUX_RESOURCE)) == 0)
      dd = MAX_DRIVE - 1;
    else
      dd = cds_drive(sda_cds);
    if (dd >= 0 && drives[dd].root)
      found = 1;
  }

  if (!found && check_esdi_cds) {
    dd = cds_drive(esdi_cds);
    if (dd >= 0 && drives[dd].root)
      found = 1;
  }

  if (!found && check_dpb) {
    dd = sft_device_info(sft) & 0x0d1f;
        /* 11/20/95 by RGPP:
          Some Novell applications seem to trash the sft_device_info
          in such a way that it looks like an existing DOSEMU re-
          directed drive.  This condition seems to be detectable as
          a non-zero value in the second four bits of the entry.
	  2002/01/08: apparently PTSDOS doesn't like bit 9 though...
	*/
    if (dd == 0 && (sft_device_info(sft) & 0x8000))
      dd = MAX_DRIVE - 1;
    if (dd >= 0 && dd < MAX_DRIVE && drives[dd].root) {
      found = 1;
    }
  }

  if (!found && check_sda_ffn) {
    char *fn1 = sda_filename1(sda);

    if (strncasecmp(fn1, LINUX_RESOURCE, strlen(LINUX_RESOURCE)) == 0) {
      found = 1;
      dd = MAX_DRIVE - 1;
    }

    if (!found)
      dd = toupper(fn1[0]) - 'A';
    if (dd >= 0 && dd < MAX_DRIVE && drives[dd].root) {
      /* removed ':' check so DRDOS would be happy,
	     there is a slight worry about possible device name
	     troubles with this - will PRN ever be interpreted as P:\RN ? */
      found = 1;
    }
  }

  if (!found && check_dssi_fn) {
    char *name = (char *) Addr(state, ds, esi);
    Debug0((dbg_fd, "FNX=%.15s\n", name));
    if (strncasecmp(name, LINUX_RESOURCE, strlen(LINUX_RESOURCE)) == 0) {
      dd = MAX_DRIVE - 1;
    } else if (name[1] == ':') {
      dd = toupper(name[0]) - 'A';
    } else {
      dd = sda_cur_drive(sda);
    }
    if (dd >= 0 && dd < MAX_DRIVE && drives[dd].root)
      found = 1;
  }

  /* for find next we will check the drive letter in the
     findfirst block which is in the DTA */
  if (!found && fn == FIND_NEXT) {
    u_char *dta = sda_current_dta(sda);
    dd = (*dta & ~0x80);
    if (dd >= 0 && dd < MAX_DRIVE && drives[dd].root)
      found = 1;
  }

  if (!found) {
    Debug0((dbg_fd, "no drive selected fn=%x\n", fn));
    if (fn == 0x1f) {
      SETWORD(&(state->ebx), WORD(state->ebx) - redirected_drives);
      Debug0((dbg_fd, "Passing %d to PRINTER SETUP anyway\n",
	      (int) WORD(state->ebx)));
    }
    return (-1);
  }

  Debug0((dbg_fd, "selected drive %d: %s\n", dd, drives[dd].root));
  return (dd);
}

boolean_t is_hidden(char *fname)
{
  char *p = strrchr(fname,'/');
  if (p) fname = p+1;
  return(fname[0] == '.' && strcmp(fname,"..") && fname[1]);
}

static int file_on_fat(const char *name)
{
  struct statfs buf;
  return statfs(name, &buf) == 0 && buf.f_type == MSDOS_SUPER_MAGIC;
}

int get_dos_attr(const char *fname,int mode,boolean_t hidden)
{
  int attr = 0;

  if (fname && file_on_fat(fname) && (S_ISREG(mode) || S_ISDIR(mode))) {
    int fd = open(fname, O_RDONLY);
    if (fd != -1) {
      int res = ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &attr);
      close(fd);
      if (res == 0)
	return attr;
    }
  }

  if (S_ISDIR(mode) && !S_ISCHR(mode) && !S_ISBLK(mode))
    attr |= DIRECTORY;
  if (!(mode & S_IWRITE))
    attr |= READ_ONLY_FILE;
  if (!(mode & S_IEXEC))
    attr |= ARCHIVE_NEEDED;
  if (hidden)
    attr |= HIDDEN_FILE;
  return (attr);
}

int get_unix_attr(int mode, int attr)
{
	enum { S_IWRITEA = S_IWUSR | S_IWGRP | S_IWOTH };

#if 0
#define S_IWRITEA (S_IWUSR | S_IWGRP | S_IWOTH)
#endif
  mode &= ~(S_IFDIR | S_IWRITE | S_IEXEC);
  if (attr & DIRECTORY)
    mode |= S_IFDIR;
  if (!(attr & READ_ONLY_FILE))
    mode |= S_IWRITEA;
  if (!(attr & ARCHIVE_NEEDED))
    mode |= S_IEXEC;
  /* Apply current umask to rwx_group and rwx_other permissions */
  mode &= ~(process_mask & (S_IRWXG | S_IRWXO));
  /* Don't set WG or WO unless corresponding Rx is set! */
  mode &= ~(((mode & S_IROTH) ? 0 : S_IWOTH) |
	    ((mode & S_IRGRP) ? 0 : S_IWGRP));
  return (mode);
}

int set_fat_attr(int fd, int attr)
{
  return ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &attr);
}

int set_dos_attr(char *fpath, int mode, int attr)
{
  int res, fd, newmode;

  fd = -1;
  if (fpath && file_on_fat(fpath) && (S_ISREG(mode) || S_ISDIR(mode)))
    fd = open(fpath, O_RDONLY);
  if (fd != -1) {
    res = set_fat_attr(fd, attr);
    if (res && errno != ENOTTY) {
      int oldattr = 1;
      ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &oldattr);
      if (dos_would_allow(fpath, "FAT_IOCTL_SET_ATTRIBUTES", attr == oldattr))
	res = 0;
      close(fd);
      return res;
    }
    close(fd);
    if (res == 0)
      return res;
  }

  newmode = get_unix_attr(mode, attr);
  if (chmod(fpath, newmode) != 0 &&
      !dos_would_allow(fpath, "chmod", newmode == mode))
    return -1;
  return 0;
}

int dos_utime(char *fpath, struct utimbuf *ut)
{
  if (utime(fpath,ut) == 0)
    return 0;
  /* output a warning if we can't do this */
  if (dos_would_allow(fpath, "utime", 0))
    return 0;
  return -1;
}

static int
get_disk_space(char *cwd, int *free, int *total)
{
  struct statfs fsbuf;

  if (statfs(cwd, &fsbuf) >= 0) {
    *free = (fsbuf.f_bsize / 512) * fsbuf.f_bavail;
    *total = (fsbuf.f_bsize / 512) * fsbuf.f_blocks;
    return (1);
  }
  else
    return (0);
}

static void
init_all_drives(void)
{
  int dd;

  if (!drives_initialized) {
    Debug0((dbg_fd, "Inside initialization\n"));
    drives_initialized = TRUE;
    init_all_DOS_tables();
    for (dd = 0; dd < MAX_DRIVE; dd++) {
      drives[dd].root = NULL;
      drives[dd].root_len = 0;
      drives[dd].read_only = FALSE;
    }
    /* special processing for UNC "drive" */
    drives[MAX_DRIVE - 1].root = "";
    process_mask = umask(0);
    umask(process_mask);
  }
}

/*
  *  this routine takes care of things
  * like getting environment variables
  * and such.  \ is used as an escape character.
  * \\ -> \
  * \${VAR} -> value of environment variable VAR
  * \T -> current tmp directory
  *
  */
static void
get_unix_path(char *new_path, const char *path)
{
  char str[PATH_MAX];
  char var[256];
  const char *orig_path = path;
  char *val;
  char *tmp_str;
  int i;
  int esc;

  i = 0;
  esc = 0;
  for (; *path != 0; path++) {
    if (esc) {
      switch (*path) {
      case '\\':		/* really meant a \ */
	str[i++] = '\\';
	break;
      case 'T':		/* the name of the temporary directory */
	tmp_str = getenv("TMPDIR");
	if (!tmp_str) tmp_str = getenv("TMP");
	if (!tmp_str) tmp_str = "/tmp";
	strncpy(&str[i], tmp_str, PATH_MAX - 2 - i);
	str[PATH_MAX - 2] = 0;
	i = strlen(str);
	break;
      case '$':		/* substitute an environment variable. */
	path++;
	if (*path != '{') {
	  var[0] = *path;
	  var[1] = 0;
	}
	else {
	  for (tmp_str = var; *path != 0 && tmp_str != &var[255]; tmp_str++) {
	    path++;
	    if (*path == '}')
	      break;
	    *tmp_str = *path;
	  }
	  *tmp_str = 0;
	  val = getenv(var);
	  if (val == NULL) {
	    Debug0((dbg_fd,
		    "In path=%s, Environment Variable $%s is not set.\n",
		    orig_path, var));
	    break;
	  }
	  strncpy(&str[i], val, PATH_MAX - 2 - i);
	  str[PATH_MAX - 2] = 0;
	  i = strlen(str);
	  esc = 0;
	  break;
	}
      }
    }
    else {
      if (*path == '\\') {
	esc = 1;
      }
      else {
	str[i++] = *path;
      }
    }
    if (i >= PATH_MAX - 2) {
      i = PATH_MAX - 2;
      break;
    }
  }
  if (i == 0 || str[i - 1] != '/')
    str[i++] = '/';
  str[i] = 0;
  strcpy(new_path, str);
  Debug0((dbg_fd,
	  "unix_path returning %s from %s.\n",
	  new_path, orig_path));
  return;
}

static int
init_drive(int dd, char *path, int options)
{
  struct stat st;
  char *new_path;
  int new_len;

  new_path = malloc(PATH_MAX + 1);
  if (new_path == NULL) {
    Debug0((dbg_fd,
	    "Out of memory in path %s.\n",
	    path));
    return ((int) NULL);
  }
  get_unix_path(new_path, path);
  new_len = strlen(new_path);
  Debug0((dbg_fd, "new_path=%s\n", new_path));
  Debug0((dbg_fd, "next_aval %d path %s opts %d root %s length %d\n",
	  dd, path, options, new_path, new_len));

  /* now a kludge to find the true name of the path */
  if (new_len != 1) {
    new_path[new_len - 1] = 0;
    drives[dd].root_len = 1;
    drives[dd].root = strdup("/");
    if (!find_file(new_path, &st, dd)) {
      error("MFS: couldn't find root path %s\n", new_path);
      free(new_path);
      return (0);
    }
    if (!(st.st_mode & S_IFDIR)) {
      error("MFS: root path is not a directory %s\n", new_path);
      free(new_path);
      return (0);
    }
    new_path[new_len - 1] = '/';
  }

  drives[dd].root = new_path;
  drives[dd].root_len = new_len;
  if (num_drives <= dd)
    num_drives = dd + 1;
  drives[dd].read_only = options;
  Debug0((dbg_fd, "initialised drive %d as %s with access of %s\n", dd, drives[dd].root,
	  drives[dd].read_only ? "READ_ONLY" : "READ_WRITE"));
  if (options >= 2 && options <= 5)
    register_cdrom(dd, options - 1);
#if 0  
  calculate_drive_pointers (dd);
#endif
  return (1);
}

/***************************
 * mfs_redirector - perform redirector emulation for int 2f, ah=11
 * on entry - nothing
 * on exit - returns non-zero if call was handled
 * 	returns 0 if call was not handled, and should be passed on.
 * notes:
 ***************************/
int
mfs_redirector(void)
{
  int ret;

  vfat_ioctl = VFAT_IOCTL_READDIR_SHORT;
  sigalarm_block(1);
  ret = dos_fs_redirect(&REGS);
  sigalarm_block(0);
  vfat_ioctl = VFAT_IOCTL_READDIR_BOTH;

  switch (ret) {
  case FALSE:
    Debug0((dbg_fd, "dos_fs_redirect failed\n"));
    REG(eflags) |= CF;
    return 1;
  case TRUE:
    Debug0((dbg_fd, "Finished dos_fs_redirect\n"));
    REG(eflags) &= ~CF;
    return 1;
  case UNCHANGED:
    return 1;
  case REDIRECT:
    return 0;
  }

  return 0;
}

int
mfs_inte6(void)
{
  return mfs_helper(&REGS);
}

int
mfs_helper(state_t *regs)
{
  boolean_t result;

  sigalarm_block(1);
  result = dos_fs_dev(regs);
  sigalarm_block(0);
  return (result);
}

/* include a few necessary functions from dos_disk.c in the mach
   code as well. input: 8.3 filename, output name + ext
   validity of filename was already checked by name_convert.
*/
void extract_filename(const char *filename, char *name, char *ext)
{
  char *dot_pos = strchr(filename, '.');
  size_t slen;

  if (dot_pos) {
    slen = dot_pos - filename;
  } else {
    slen = strlen(filename);
  }

  memcpy(name, filename, slen);
  memset(name + slen, ' ', 8 - slen);

  if (slen == 0) {
    /* '.' or '..' */
    memset(ext, ' ', 3);
    name[0] = '.';
    if (filename[1] == '.')
      name[1] = '.';
    return;
  }

  slen = 0;
  if (dot_pos) {
    dot_pos++;
    slen = strlen(dot_pos);
    memcpy(ext, dot_pos, slen);
  }
  memset(ext + slen, ' ', 3 - slen);
}

static struct dir_list *make_dir_list(int n)
{
  struct dir_list *dir_list = malloc(sizeof(*dir_list));
  dir_list->size = n;
  dir_list->nr_entries = 0;
  dir_list->de = malloc(n * sizeof(dir_list->de[0]));
  return dir_list;
}

static void enlarge_dir_list(struct dir_list *dir_list, int n)
{
  dir_list->size = n;
  dir_list->de = realloc(dir_list->de, n * sizeof(dir_list->de[0]));
}

static struct dir_ent *make_entry(struct dir_list *dir_list)
{
/* DANG_FIXTHIS returned size of struct dir_ent seems wrong at 28 bytes. */
/* DANG_BEGIN_REMARK
 * The msdos_dir_ent structure has much more than 28 bytes. 
 * Is this significant?
 * DANG_END_REMARK
 */
  struct dir_ent *entry;

  if (dir_list->nr_entries == 0xffff) {
    Debug0((dbg_fd, "DOS cannot handle more than 65535 files in one directory\n"));
    error("DOS cannot handle more than 65535 files in one directory\n");
    /* dos limit -- can't get beyond here */
    return NULL;
  }
  if (dir_list->nr_entries >= dir_list->size)
    enlarge_dir_list(dir_list, dir_list->size * 2);
  entry = &dir_list->de[dir_list->nr_entries];
  dir_list->nr_entries++;
  entry->hidden = FALSE;
  entry->long_path = FALSE;
  return entry;
}

/* construct (lowercase) unix name from (uppercase) DOS mname and DOS mext */
static void dos83_to_ufs(char *name, const char *mname, const char *mext)
{
  char filename[8+1+3+1];
  size_t len;

  len = 8;
  while (mname[len - 1] == ' ')
    len--;
  memcpy(filename, mname, len);
  filename[len++] = '.';
  memcpy(filename + len, mext, 3);
  len += 3;
  while (filename[len - 1] == ' ')
    len--;
  while (filename[len - 1] == '.')
    len--;
  filename[len] = '\0';
  path_to_ufs(name, 0, filename, 0, 1);
}

/* check if name/filename exists as such if it does not contain wildcards */
static boolean_t exists(const char *name, const char *filename,
                        struct stat *st, int drive)
{
  char fullname[strlen(name) + 1 + NAME_MAX + 1];
  snprintf(fullname, sizeof(fullname), "%s/%s", name, filename);
  Debug0((dbg_fd, "exists() result = %s\n", fullname));
  return find_file(fullname, st, drive);
}

static void fill_entry(struct dir_ent *entry, const char *name, int drive)
{
  int slen;
  char *sptr;
  char buf[PATH_MAX];
  struct stat sbuf;

  entry->hidden = is_hidden(entry->d_name);

  strcpy(buf, name);
  slen = strlen(buf);
  sptr = buf + slen + 1;
  buf[slen] = '/';
  strcpy(sptr, entry->d_name);

  if (!find_file(buf, &sbuf, drive)) {
    Debug0((dbg_fd, "Can't findfile %s\n", buf));
    entry->mode = S_IFREG;
    entry->size = 0;
    entry->time = 0;
    entry->attr = get_dos_attr(NULL,entry->mode,entry->hidden);
  }
  else {
    entry->mode = sbuf.st_mode;
    entry->size = sbuf.st_size;
    entry->time = sbuf.st_mtime;
    entry->attr = get_dos_attr(buf,entry->mode,entry->hidden);
  }
} 

/* converts d_name to DOS 8:3 and compares with the wildcard */
static boolean_t convert_compare(char *d_name, char *fname, char *fext,
				 char *mname, char *mext, boolean_t in_root)
{
  char tmpname[NAME_MAX + 1];
  size_t namlen;
  boolean_t maybe_mangled;

  maybe_mangled = (mname[5] == '~' || mname[5] == '?');

  name_ufs_to_dos(tmpname, d_name);
  if (!name_convert(tmpname, maybe_mangled))
    return FALSE;

  namlen = strlen(tmpname);

  if (tmpname[0] == '.') {
    if (namlen > 2)
      return FALSE;
    if (in_root)
      return FALSE;
    if ((namlen == 2) &&
	(tmpname[1] != '.'))
      return FALSE;
  }
  strupperDOS(tmpname);
  extract_filename(tmpname, fname, fext);
  return compare(fname, fext, mname, mext);
}

/* get directory;
   name = UNIX directory name
   mname = DOS (uppercase) name to match (can have wildcards)
   mext = DOS (uppercase) extension to match (can have wildcards)
   drive = drive on which the directory lives
*/
static struct dir_list *get_dir(char *name, char *mname, char *mext, int drive)
{
  struct mfs_dir *cur_dir;
  struct mfs_dirent *cur_ent;
  struct dir_list *dir_list;
  struct dir_ent *entry;
  struct stat sbuf;
  char buf[256];
  char fname[8];
  char fext[3];

  if(is_dos_device(name) || !find_file(name, &sbuf, drive))
    return NULL;

  if ((cur_dir = dos_opendir(name)) == NULL) {
    Debug0((dbg_fd, "get_dir(): couldn't open '%s' errno = %s\n", name, strerror(errno)));
    return (NULL);
  }

  Debug0((dbg_fd, "get_dir() opened '%s'\n", name));

  dir_list = NULL;

/* DANG_BEGIN_REMARK
 * 
 * Added compares to device so that newer versions of Foxpro which test directories
 * using xx\yy\device perform closer to whats DOS does.
 * 
 * DANG_END_REMARK
 */
  if (mname && is_dos_device(mname)) {
    dir_list = make_dir_list(1);
    entry = make_entry(dir_list);

    memcpy(entry->name, mname, 8);
    memset(entry->ext, ' ', 3);
    dos83_to_ufs(entry->d_name, entry->name, entry->ext);
    entry->mode = S_IFREG;
    entry->size = 0;
    entry->time = time(NULL);
    entry->attr = get_dos_attr(NULL,entry->mode,entry->hidden);

    dos_closedir(cur_dir);
    return (dir_list);
  }
  /* for efficiency we don't read everything if there are no wildcards */
  else if (mname && !memchr(mname, '?', 8) && !memchr(mext, '?', 3))
  {
    dos83_to_ufs(buf, mname, mext);
    if (exists(name, buf, &sbuf, drive))
    {
      Debug0((dbg_fd, "filename exists, %s %.8s%.3s\r\n", name, mname, mext));
      dir_list = make_dir_list(1);
      entry = make_entry(dir_list);

      memcpy(entry->name, mname, 8);
      memcpy(entry->ext, mext, 3);
      strcpy(entry->d_name, buf);
      entry->mode = sbuf.st_mode;
      entry->size = sbuf.st_size;
      entry->time = sbuf.st_mtime;
      entry->attr = get_dos_attr(buf,entry->mode,entry->hidden);
    }
    dos_closedir(cur_dir);
    return (dir_list);      
  }
  else {
    boolean_t is_root = (strlen(name) == drives[drive].root_len);
    while ((cur_ent = dos_readdir(cur_dir))) {
      if (mname) {
	sigset_t mask;

	/* this while loop can take a _long_ time ... better avoid signal queue
	 *overflows AV
	 */
	sigpending(&mask);
	if (sigismember(&mask, SIGALRM)) {
	  sigalarm_block(0);
	  /* the pending sigalrm must be delivered now */
	  sigalarm_block(1);
	}
	handle_signals();

	Debug0((dbg_fd, "get_dir(): `%s' \n", cur_ent->d_name));
      
	/* this is the expensive part, done much later in findnext 
	   if mname == NULL */
	if (!convert_compare(cur_ent->d_name, fname, fext,
			      mname, mext, is_root))
	  continue;
      }

      if (dir_list == NULL) {
	dir_list = make_dir_list(20);
      }
      entry = make_entry(dir_list);

      strcpy(entry->d_name, cur_ent->d_name);
      /* only fill these values if we don't search everything */
      if (mname) {
	memcpy(entry->name, fname, 8);
	memcpy(entry->ext, fext, 3);
	fill_entry(entry, name, drive);
      }
    }
  }
  dos_closedir(cur_dir);
  return (dir_list);
}

/*
 * Another useless specialized parsing routine!
 * Assumes that a legal string is passed in.
 */
static void auspr(const char *filestring, char *name, char *ext)
{
  const char *bs_pos;
  char *star_pos;

  Debug1((dbg_fd, "auspr '%s'\n", filestring));
  bs_pos=strrchr(filestring, '\\');
  if (bs_pos == NULL)
    bs_pos = filestring;
  else
    bs_pos++;
  filestring = bs_pos;

  extract_filename(filestring, name, ext);
  /* convert any * wildcards (from DRDOS 5.0) into ? */
  star_pos = memchr(name, '*', 8);
  if (star_pos)
    memset(star_pos, '?', name + 8 - star_pos);
  star_pos = memchr(ext, '*', 3);
  if (star_pos)
    memset(star_pos, '?', ext + 3 - star_pos);

  Debug0((dbg_fd,"auspr(%s,%.8s,%.3s)\n",filestring,name,ext));
}

static void
init_dos_offsets(int ver)
{
  Debug0((dbg_fd, "dos_fs: using dos version = %d.\n", ver));
  switch (ver) {
  case DOSVER_31_33:
    {
      sdb_drive_letter_off = 0x0;
      sdb_template_name_off = 0x1;
      sdb_template_ext_off = 0x9;
      sdb_attribute_off = 0xc;
      sdb_dir_entry_off = 0xd;
      sdb_p_cluster_off = 0xf;
      sdb_file_name_off = 0x15;
      sdb_file_ext_off = 0x1d;
      sdb_file_attr_off = 0x20;
      sdb_file_time_off = 0x2b;
      sdb_file_date_off = 0x2d;
      sdb_file_st_cluster_off = 0x2f;
      sdb_file_size_off = 0x31;

      sft_handle_cnt_off = 0x0;
      sft_open_mode_off = 0x2;
      sft_attribute_byte_off = 0x4;
      sft_device_info_off = 0x5;
      sft_dev_drive_ptr_off = 0x7;
      sft_fd_off = 0xb;
      sft_start_cluster_off = 0xb;
      sft_time_off = 0xd;
      sft_date_off = 0xf;
      sft_size_off = 0x11;
      sft_position_off = 0x15;
      sft_rel_cluster_off = 0x19;
      sft_abs_cluster_off = 0x1b;
      sft_directory_sector_off = 0x1d;
      sft_directory_entry_off = 0x1f;
      sft_name_off = 0x20;
      sft_ext_off = 0x28;

      cds_record_size = 0x51;
      cds_current_path_off = 0x0;
      cds_flags_off = 0x43;
      cds_rootlen_off = 0x4f;

      sda_current_dta_off = 0xc;
      sda_cur_psp_off = 0x10;
      sda_cur_drive_off = 0x16;
      sda_filename1_off = 0x92;
      sda_filename2_off = 0x112;
      sda_sdb_off = 0x192;
      sda_cds_off = 0x26c;
      sda_search_attribute_off = 0x23a;
      sda_open_mode_off = 0x23b;
      sda_rename_source_off = 0x2b8;
      sda_user_stack_off = 0x250;

      lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;
      lol_nuldev_off = 0x22;      
      lol_njoined_off = 0x34;
      break;
    }
  case DOSVER_50:
  case DOSVER_41:
    {
      sdb_drive_letter_off = 0x0;
      sdb_template_name_off = 0x1;
      sdb_template_ext_off = 0x9;
      sdb_attribute_off = 0xc;
      sdb_dir_entry_off = 0xd;
      sdb_p_cluster_off = 0xf;
      sdb_file_name_off = 0x15;
      sdb_file_ext_off = 0x1d;
      sdb_file_attr_off = 0x20;
      sdb_file_time_off = 0x2b;
      sdb_file_date_off = 0x2d;
      sdb_file_st_cluster_off = 0x2f;
      sdb_file_size_off = 0x31;

      /* same */ sft_handle_cnt_off = 0x0;
      sft_open_mode_off = 0x2;
      sft_attribute_byte_off = 0x4;
      sft_device_info_off = 0x5;
      sft_dev_drive_ptr_off = 0x7;
      sft_fd_off = 0xb;
      sft_start_cluster_off = 0xb;
      sft_time_off = 0xd;
      sft_date_off = 0xf;
      sft_size_off = 0x11;
      sft_position_off = 0x15;
      sft_rel_cluster_off = 0x19;
      sft_abs_cluster_off = 0x1b;
      sft_directory_sector_off = 0x1d;
      sft_directory_entry_off = 0x1f;
      sft_name_off = 0x20;
      sft_ext_off = 0x28;

      /* done */ cds_record_size = 0x58;
      cds_current_path_off = 0x0;
      cds_flags_off = 0x43;
      cds_rootlen_off = 0x4f;

      /* done */ sda_current_dta_off = 0xc;
      sda_cur_psp_off = 0x10;
      sda_cur_drive_off = 0x16;
      sda_filename1_off = 0x9e;
      sda_filename2_off = 0x11e;
      sda_sdb_off = 0x19e;
      sda_cds_off = 0x282;
      sda_search_attribute_off = 0x24d;
      sda_open_mode_off = 0x24e;
      sda_ext_act_off = 0x2dd;
      sda_ext_attr_off = 0x2df;
      sda_ext_mode_off = 0x2e1;
      sda_rename_source_off = 0x300;
      sda_user_stack_off = 0x264;

      /* same */ lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;
      lol_nuldev_off = 0x22;      
      lol_njoined_off = 0x34;

      break;
    }
    /* at the moment dos 6 is the same as dos 5,
	 anyone care to fix these for dos 6 ?? */
  case DOSVER_60:
  default:
    {
      sdb_drive_letter_off = 0x0;
      sdb_template_name_off = 0x1;
      sdb_template_ext_off = 0x9;
      sdb_attribute_off = 0xc;
      sdb_dir_entry_off = 0xd;
      sdb_p_cluster_off = 0xf;
      sdb_file_name_off = 0x15;
      sdb_file_ext_off = 0x1d;
      sdb_file_attr_off = 0x20;
      sdb_file_time_off = 0x2b;
      sdb_file_date_off = 0x2d;
      sdb_file_st_cluster_off = 0x2f;
      sdb_file_size_off = 0x31;

      /* same */ sft_handle_cnt_off = 0x0;
      sft_open_mode_off = 0x2;
      sft_attribute_byte_off = 0x4;
      sft_device_info_off = 0x5;
      sft_dev_drive_ptr_off = 0x7;
      sft_fd_off = 0xb;
      sft_start_cluster_off = 0xb;
      sft_time_off = 0xd;
      sft_date_off = 0xf;
      sft_size_off = 0x11;
      sft_position_off = 0x15;
      sft_rel_cluster_off = 0x19;
      sft_abs_cluster_off = 0x1b;
      sft_directory_sector_off = 0x1d;
      sft_directory_entry_off = 0x1f;
      sft_name_off = 0x20;
      sft_ext_off = 0x28;

      /* done */ cds_record_size = 0x58;
      cds_current_path_off = 0x0;
      cds_flags_off = 0x43;
      cds_rootlen_off = 0x4f;

      /* done */ sda_current_dta_off = 0xc;
      sda_cur_psp_off = 0x10;
      sda_cur_drive_off = 0x16;
      sda_filename1_off = 0x9e;
      sda_filename2_off = 0x11e;
      sda_sdb_off = 0x19e;
      sda_cds_off = 0x282;
      sda_search_attribute_off = 0x24d;
      sda_open_mode_off = 0x24e;
      sda_ext_act_off = 0x2dd;
      sda_ext_attr_off = 0x2df;
      sda_ext_mode_off = 0x2e1;
      sda_rename_source_off = 0x300;
      sda_user_stack_off = 0x264;

      /* same */ lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;
      lol_nuldev_off = 0x22;      
      lol_njoined_off = 0x34;

      break;
    }
  }
}

struct mfs_dir *dos_opendir(const char *name)
{
  struct mfs_dir *dir;
  int fd = -1;
  DIR *d = NULL;
  struct kernel_dirent de[2];

  if (file_on_fat(name)) {
    fd = open(name, O_RDONLY|O_DIRECTORY);
    if (fd == -1)
      return NULL;
    de[0].d_name[1] = '.';
    if (ioctl(fd, vfat_ioctl, (long)&de) != -1 &&
	/* Bug in kernels <= 2.6.21.1 (ioctl32 on x86-64) */
	de[0].d_name[1] == '\0') {
      lseek(fd, 0, SEEK_SET);
    } else {
      close(fd);
      fd = -1;
    }
  }
  if (fd == -1) {
    /* not a VFAT filesystem or other problems */
    d = opendir(name);
    if (d == NULL)
      return NULL;
  }
  dir = malloc(sizeof *dir);
  dir->fd = fd;
  dir->dir = d;
  dir->nr = 0;
  return (dir);
}

struct mfs_dirent *dos_readdir(struct mfs_dir *dir)
{
  if (dir->nr <= 1) {
    dir->de.d_name = dir->de.d_long_name = dir->nr ? ".." : ".";
  } else do {
    if (dir->dir) {
      struct direct *de = (struct direct *) readdir(dir->dir);
      if (de == NULL)
	return NULL;
      dir->de.d_name = dir->de.d_long_name = de->d_name;
    } else {
      static struct kernel_dirent de[2];
      int ret = (int)RPT_SYSCALL(ioctl(dir->fd, vfat_ioctl, (long)&de));
      if (ret == -1 || de[0].d_reclen == 0)
	return NULL;
      dir->de.d_name = de[0].d_name;
      dir->de.d_long_name = de[1].d_name;
      if (dir->de.d_long_name[0] == '\0' ||
	  vfat_ioctl == VFAT_IOCTL_READDIR_SHORT) {
        dir->de.d_long_name = dir->de.d_name;
      }
    }
  } while (strcmp(dir->de.d_name, ".") == 0 ||
	   strcmp(dir->de.d_name, "..") == 0);
  dir->nr++;
  return (&dir->de);
}

int dos_closedir(struct mfs_dir *dir)
{
  int ret;

  if (dir->dir)
    ret = closedir(dir->dir);
  else
    ret = close(dir->fd);
  free(dir);
  return (ret);
}

static inline int
dos_flush(int fd)
{
  int ret;

  ret = RPT_SYSCALL(fsync(fd));

  return (ret);
}

static int
calculate_drive_pointers(int dd)
{
  far_t cdsfarptr;
  char *cwd;
  cds_t cds_base, cds;

  if (!lol)
    return (0);
  if (!drives[dd].root)
    return (0);

  cdsfarptr = lol_cdsfarptr(lol);
  cds_base = MK_FP32(cdsfarptr.segment, cdsfarptr.offset);

  cds = drive_cds(dd);

  /* if it's already done then don't bother */
  if ((cds_flags(cds) & (CDS_FLAG_REMOTE | CDS_FLAG_READY)) ==
      (CDS_FLAG_REMOTE | CDS_FLAG_READY))
    return (1);

  Debug0((dbg_fd, "Calculated DOS Information for %d:\n", dd));
  Debug0((dbg_fd, "  cwd=%20s\n", cds_current_path(cds)));
  Debug0((dbg_fd, "  cds flags =%x\n", cds_flags(cds)));
  Debug0((dbg_fd, "  cdsfar = %x, %x\n", cdsfarptr.segment,
	  cdsfarptr.offset));

  cds_flags(cds) |= (CDS_FLAG_REMOTE | CDS_FLAG_READY | CDS_FLAG_NOTNET);

  cwd = cds_current_path(cds);
  sprintf(cwd, "%c:\\", 'A' + dd);
  cds_rootlen(cds) = strlen(cwd) - 1;
  Debug0((dbg_fd, "cds_current_path=%s\n", cwd));
  return (1);
}

static boolean_t
dos_fs_dev(state_t *state)
{
  u_char drive_to_redirect;
  int dos_ver;

  Debug0((dbg_fd, "emufs operation: 0x%08lx\n", state->ebx));

  if (WORD(state->ebx) == 0x500) {
    init_all_drives();
    mach_fs_enabled = TRUE;

    lol = (lol_t) Addr(state, es, edx);
    sda = (sda_t) Addr(state, ds, esi);
    dos_major = LOW(state->ecx);
    dos_minor = HIGH(state->ecx);
    if (running_DosC)
        Debug0((dbg_fd, "dos_fs: running DosC build 0x%x\n", running_DosC));
    Debug0((dbg_fd, "dos_fs: dos_major:minor = 0x%d:%d.\n",
	    dos_major, dos_minor));
    Debug0((dbg_fd, "lol=%p\n", (void *) lol));
    Debug0((dbg_fd, "sda=%p\n", (void *) sda));
    if ((dos_major == 3) && (dos_minor > 9) && (dos_minor <= 31)) {
      dos_ver = DOSVER_31_33;
    }
    else if ((dos_major == 4) && (dos_minor >= 0) && (dos_minor <= 1)) {
      dos_ver = DOSVER_41;
    }
    else if ((dos_major == 5) && (dos_minor >= 0)) {
      dos_ver = DOSVER_50;
    }
    else if ((dos_major >= 6) && (dos_minor >= 0)) {
      dos_ver = DOSVER_60;
    }
    else {
      dos_ver = 0;
    }
	/* we need to fake a 4.1 SDA, because the DosC structure _has_
	 * 4.1 layout of the SDA though reporting DOS 3.31 compatibility.
	 *                             --Hans 990703
	 */
    if (running_DosC) dos_ver = DOSVER_41;
    init_dos_offsets(dos_ver);
    SETWORD(&(state->eax), 1);
  }

  if (WORD(state->ebx) == 0) {
    u_char *ptr;

    ptr = (u_char *) Addr_8086(state->es, state->edi) + 22;

    drive_to_redirect = *ptr;
    /* if we've never set our first free drive, set it now, and */
    /* initialize our drive tables */
    if (first_free_drive == 0) {
      Debug0((dbg_fd, "Initializing all drives\n"));
      first_free_drive = drive_to_redirect;
      init_all_drives();
    }

    if (drive_to_redirect - (int) first_free_drive < 0) {
      SETWORD(&(state->eax), 0);
      Debug0((dbg_fd, "Invalid drive - maybe increase LASTDRIVE= in config.sys?\n"));
      return (UNCHANGED);
    }

    *(ptr - 9) = 1;
    Debug0((dbg_fd, "first_free_drive = %d\n", first_free_drive));
    {
      u_short *seg = (u_short *) (ptr - 2);
      u_short *ofs = (u_short *) (ptr - 4);
      char *clineptr = MK_FP32(*seg, *ofs);
      char *dirnameptr = MK_FP32(state->ds, state->esi);
      char cline[256];
      char *t;
      int i = 0;
      int opt;

      while (*clineptr != '\n' && *clineptr != '\r')
	cline[i++] = *(clineptr++);
      cline[i] = 0;

      t = strtok(cline, " \n\r\t");
      if (t) {
	t = strtok(NULL, " \n\r\t");
      }

      opt = 0;
      if (t) {
	char *p = strtok(NULL, " \n\r\t");
	opt = (p && (toupper(p[0]) == 'R'));
      }
      if (!init_drive(drive_to_redirect, t, opt)) {
	SETWORD(&(state->eax), 0);
	return (UNCHANGED);
      }

      if (strncmp(dirnameptr - 10, "directory ", 10) == 0) {
	*dirnameptr = 0;
	strncpy(dirnameptr, drives[drive_to_redirect].root, 128);
	strcat(dirnameptr, "\n\r$");
      }
      else
	Debug0((dbg_fd, "WARNING! old version of emufs.sys!\n"));
    }

    mach_fs_enabled = TRUE;

    /*
     * So that machfs.sys v1.1+ will know that
     * we're running Mach too.
     */
    SETWORD(&(state->eax), 1);

    return (UNCHANGED);
  }

  return (UNCHANGED);
}

void time_to_dos(time_t clock, u_short *date, u_short *time)
{
  struct tm *tm;

  tm = localtime(&clock);

  *date = ((((tm->tm_year - 80) & 0x7f) << 9) |
	   (((tm->tm_mon + 1) & 0xf) << 5) |
	   (tm->tm_mday & 0x1f));

  *time = (((tm->tm_hour & 0x1f) << 11) |
	   ((tm->tm_min & 0x3f) << 5) |
	   ((tm->tm_sec>>1) & 0x1f));
}

time_t time_to_unix(u_short dos_date, u_short dos_time) 
{
   struct tm T;
   T.tm_sec  = (dos_time & 0x1f) << 1;    dos_time >>= 5;
   T.tm_min  = dos_time & 0x3f;           dos_time >>= 6;
   T.tm_hour = dos_time;
   T.tm_mday = dos_date & 0x1f;           dos_date >>= 5;
   T.tm_mon  = (dos_date & 0x0f) - 1;     dos_date >>= 4;
   T.tm_year = dos_date + 80;
   T.tm_isdst = -1;

   /* tm_wday, tm_yday are ignored by mktime() */

   /* OOPS! it's not true for tm_isdst! (at least with libc 4.6.27) -- peak */
   /* autodetect dst */
   T.tm_isdst = -1;

   return mktime(&T);
}

static void
path_to_ufs(char *ufs, size_t ufs_offset, const char *path, int PreserveEnvVar,
            int lowercase)
{
  char ch;
  int inenv = 0;

  mbstate_t unix_state;
  memset(&unix_state, 0, sizeof unix_state);

  if (ufs_offset < PATH_MAX) do {
    ch = *path++;
    if (ufs_offset == PATH_MAX - 1)
      ch = EOS;
    switch (ch) {
    case BACKSLASH:
      /* Check for environment variable */
      if (PreserveEnvVar && path[0] == '$' && path[1] == '{')
	inenv = 1;
      else
	ch = SLASH;
      /* fall through */
    case EOS:  
    case SLASH:
    case '.':
      /* remove trailing spaces for SFNs */
      if (lowercase) while(ufs_offset > 0 && ufs[ufs_offset - 1] == ' ')
        ufs_offset--;
      break;
    case '}':
      inenv = 0;
      break;
    default:
      break;
    }
    if (ch != EOS) {
      size_t result;
      wchar_t symbol = dos_to_unicode_table[(unsigned char)ch];
      if (lowercase && !inenv)
        symbol = towlower(symbol);
      result = wcrtomb(&ufs[ufs_offset], symbol, &unix_state);
      if (result == -1)
        ufs[ufs_offset++] = '?';
      else
        ufs_offset += result;
    } else
      ufs[ufs_offset++] = ch;
  } while(ch != EOS);

  Debug0((dbg_fd, "dos_gen: path_to_ufs '%s'\n", ufs));
}

int build_ufs_path_(char *ufs, const char *path, int drive, int lowercase)
{
  int i;

  Debug0((dbg_fd, "dos_fs: build_ufs_path for DOS path '%s'\n", path));

  strcpy(ufs, drives[drive].root);
  /* Skip over leading <drive>:\ in the path */
  if (path[1]==':')
    path += cds_rootlen(drive_cds(drive));
  
  /* strip \\linux\fs if present */
  if (strncasecmp(path, LINUX_RESOURCE, strlen(LINUX_RESOURCE)) == 0) {
    size_t len;

    path_to_ufs(ufs, 0, &path[strlen(LINUX_RESOURCE)], 1, lowercase);
    get_unix_path(ufs, ufs);
    len = strlen(ufs);
    if (len > 1 && ufs[len - 1] == SLASH)
      ufs[len - 1] = EOS;
    return TRUE;
  }

  Debug0((dbg_fd,"dos_gen: ufs '%s', path '%s', l=%d\n", ufs, path,
          drives[drive].root_len));
  
  path_to_ufs(ufs, drives[drive].root_len, path, 0, lowercase);
  
  /* remove any double slashes */
  i = 0;
  while (ufs[i]) {
    if (ufs[i] == '/' && ufs[i+1] == '/')
      strcpy(&ufs[i], &ufs[i+1]);
    else
      i++;
  }
  Debug0((dbg_fd, "dos_fs: build_ufs_path result is '%s'\n", ufs));
  return TRUE;
}

static inline int build_ufs_path(char *ufs, const char *path, int drive)
{
  return build_ufs_path_(ufs, path, drive, 1);
}

/*
 * scan a directory for a matching filename
 */
static boolean_t
scan_dir(char *path, char *name, int drive)
{
  struct mfs_dir *cur_dir;
  struct mfs_dirent *cur_ent;
  int maybe_mangled, is_8_3;
  char dosname[strlen(name)+1];

  /* handle null paths */
  if (*path == 0)
    path = "/";

  Debug0((dbg_fd,"scan_dir(%s,%s)\n",path,name));

  /* check if name is an LFN or not; if it's 8.3
     then dosname will contain the uppercased name, and
     otherwise the name in the DOS character set */
  is_8_3 = maybe_mangled = 0;
  name_ufs_to_dos(dosname, name);
  if (name_convert(dosname, 0)) {
    is_8_3 = 1;
    /* check if the name is, perhaps, mangled. If not then we don't
       need to mangle the readdir result as it can't be the same */
    maybe_mangled = is_mangled(dosname);
  }

  /* ignore . and .. in root directories */
  if (dosname[0] == '.' && strlen(path) == drives[drive].root_len &&
      (dosname[1] == '\0' || strcmp(dosname, "..") == 0))
    return (FALSE);

  /* open the directory */
  if ((cur_dir = dos_opendir(path)) == NULL) {
    Debug0((dbg_fd, "scan_dir(): failed to open dir: %s\n", path));
    return (FALSE);
  }

  strupperDOS(dosname);

  /* now scan for matching names */
  while ((cur_ent = dos_readdir(cur_dir))) {
    char tmpname[NAME_MAX + 1];

    if (!name_ufs_to_dos(tmpname,cur_ent->d_long_name) && !is_8_3)
      continue;

    if (is_8_3 && !name_convert(tmpname,maybe_mangled))
      continue;

    /* tmpname now contains the readdir name in the DOS character set */
    if (!strequalDOS(tmpname, dosname)) {
      if (!maybe_mangled || cur_ent->d_long_name == cur_ent->d_name)
	continue;

      /* check if the long name variety of the current name
	 can be represented in DOS; otherwise it is mangled.
	 only used for the LFN code on VFAT partitions.
      */
      if (name_ufs_to_dos(tmpname,cur_ent->d_long_name))
	continue;
      name_convert(tmpname,MANGLE);
      if (!strequalDOS(tmpname, dosname))
	continue;
    }

    Debug0((dbg_fd, "scan_dir found %s\n",cur_ent->d_name));

    /* we've found the file, change it's name and return */
    strcpy(name, cur_ent->d_name);
    dos_closedir(cur_dir);
    return (TRUE);
  }

  dos_closedir(cur_dir);

  if (MANGLE && is_mangled(name))
    check_mangled_stack(name,NULL);

  Debug0((dbg_fd, "scan_dir gave %s FALSE\n",name));

  return (FALSE);
}

/*
 * a new find_file that will do complete upper/lower case matching for the
 * whole path
 */
boolean_t find_file(char *fpath, struct stat * st, int drive)
{
  char *slash1, *slash2;

  Debug0((dbg_fd, "find file %s\n", fpath));

  if (is_dos_device(fpath)) {
    struct stat st;
    char *s = strrchr(fpath, '/');
    int path_exists = 0;
      
    /* check if path exists */
    if (s != NULL) {
      *s = '\0';
      path_exists = (!stat(fpath, &st) && S_ISDIR(st.st_mode));
      *s = '/';
    }
    Debug0((dbg_fd, "device exists  = %d\n", s == NULL || path_exists));
    return (s == NULL || path_exists);
  }
  
  /* first see if the path exists as is */
  if (stat(fpath, st) == 0) {
    Debug0((dbg_fd, "file exists as is\n"));
    return (TRUE);
  }

  /* if it isn't an absolute path then we're in trouble */
  if (*fpath != '/') {
    error("MFS: non-absolute path in find_file: %s\n", fpath);
    return (FALSE);
  }

  /* slash1 will point to the beginning of the section we're looking
     at, and slash2 will point at the end */
  slash1 = fpath + drives[drive].root_len - 1;

  /* now match each part of the path name separately, trying the names
     as is first, then tring to scan the directory for matching names */
  while (slash1) {
    slash2 = strchr(slash1 + 1, '/');
    if (slash2)
      *slash2 = 0;
    if (stat(fpath, st) == 0) {
      /* the file exists as is */
      if (st->st_mode & S_IFDIR || !slash2) {
	if (slash2)
	  *slash2 = '/';
	slash1 = slash2;
	continue;
      }

      Debug0((dbg_fd, "find_file(): not a directory: %s\n", fpath));
      if (slash2)
	*slash2 = '/';
      return (FALSE);
    }
    else {
      char remainder[PATH_MAX];
      *slash1 = 0;
      if (slash2) {
	remainder[0] = '/';
	strcpy(remainder+1,slash2+1);
      }
      if (!scan_dir(fpath, slash1 + 1, drive)) {
	*slash1 = '/';
	Debug0((dbg_fd, "find_file(): no match: %s\n", fpath));
	if (slash2)
	  strcat(slash1+1,remainder);
	return (FALSE);
      }
      else {
        *slash1 = '/';
        if (slash2)
	  strcat(slash1+1,remainder);
        slash1 = strchr(slash1+1,'/');
      }
    }
  }

  /* we've found the file - now stat it */
  if (stat(fpath, st) != 0) {
    Debug0((dbg_fd, "find_file(): can't stat %s\n", fpath));
    return (FALSE);
  }

  Debug0((dbg_fd, "found file %s\n", fpath));
  return (TRUE);
}

static boolean_t
compare(char *fname, char *fext, char *mname, char *mext)
{
  int i;

  Debug0((dbg_fd, "dos_gen: compare '%.8s'.'%.3s' to '%.8s'.'%.3s'\n",
	  mname, mext, fname, fext));
  /* match name first */
  for (i = 0; i < 8; i++) {
    if (mname[i] == '?') {
      continue;
    }
    if (mname[i] != fname[i]) {
      return (FALSE);
    }
  }
  /* if got here then name matches */
  if (is_dos_device(mname))
    return (TRUE);
  
  /* match ext next */
  for (i = 0; i < 3; i++) {
    if (mext[i] == '?') {
      continue;
    }
    if (mext[i] != fext[i]) {
      return (FALSE);
    }
  }
  return (TRUE);
}

/* Set the long_path flag for every directory on a dir_ent list.
   Called on FIND_FIRST when we are in a directory with a long
   pathname. The potentially dangerous subdirectories can then be
   handled properly.*/
static void set_long_path_on_dirs(struct dir_list *dir_list)
{
  int i;
  struct dir_ent *list = &dir_list->de[0];
  for (i = 0; i < dir_list->nr_entries; i++) {
    if (S_ISDIR(list->mode)) {
      list->long_path = TRUE;
    }
    list++;
  }
}

static int
is_long_path(const char* path)
{
  char *p = strrchr(path, BACKSLASH);
  return (p != NULL)
    && (p-path > MAX_PATH_LENGTH-8);
}

#define HLIST_STACK_SIZE 256

struct stack_entry
{
  struct dir_list *hlist;
  unsigned psp;
  int seq;
  int duplicates;
  unsigned char *fpath;      
};

#define HLIST_WATCH_CNT 64	/* if more than HWC hlist positions then ... */

static struct
{
  int tos;                      /* top of stack */
  int seq;                      /* sequence number */
  /* 
   * = 1 : watching
   * = 0 : findnext in progress without watching
   */
  int watch;
  struct stack_entry stack[HLIST_STACK_SIZE];
} hlists;

static void free_list(struct stack_entry *se, boolean_t force)
{
  struct dir_list *list;

  if (se->duplicates && !force) {
    se->duplicates--;
    return;
  }

  free(se->fpath);
  se->fpath = NULL;

  list = se->hlist;  
  if (list == NULL)
    return;

  free(list->de);
  free(list);
  se->hlist = NULL;
}

static inline int hlist_push(struct dir_list *hlist, unsigned psp, char *fpath)
{
  struct stack_entry *se;
  static unsigned prev_psp = 0;

  Debug0((dbg_fd, "hlist_push: %d hlist=%p PSP=%d path=%s\n", hlists.tos, hlist, psp, fpath));

  if (fpath[0]) for (se = hlists.stack; se < &hlists.stack[hlists.tos]; se++) {
    if (se->hlist && se->fpath && strcmp(se->fpath, fpath) == 0) {
      Debug0((dbg_fd, "hlist_push: found duplicate\n"));
      /* if the list is owned by the current PSp then we must not
         destroy it after the last findnext. If the list is owned
         by a different PSP then it is never destroyed by the current
         PSP so we should not mark it duplicate.
         We then replace the duplicate with a fresh directory list;
         this does not conflict with how DOS works on real FAT drives,
         where a "findnext" looks directly in the directory.
      */
      if (se->psp == psp)
        se->duplicates++;
      free_list(se, TRUE);
      goto exit;
    }
  }

  if (psp != prev_psp) {
    Debug0((dbg_fd, "hlist_push_new_psp: prev_psp=%d psp=%d\n", prev_psp, psp));
    prev_psp = psp;
    hlists.watch = 0;	/* reset watch for new PSP */
  }
  else {
    /*
     * we're looking for a gap, which is a result from a deletion of
     * a previous broken(?) findfirst/findnext. --ms
     */
    for (se = hlists.stack; se < &hlists.stack[hlists.tos]; se++) {
      if (se->hlist == NULL) {
        Debug0((dbg_fd, "hlist_push gap=%td\n", se - hlists.stack));
        se->duplicates = 0;
        se->psp = psp;
        goto exit;
      }
    }
  }
  
  if (hlists.tos >= HLIST_STACK_SIZE) {
    Debug0((dbg_fd, "hlist_push: past maximum stack\n"));
    error("MFS: hlist_push: past maximum stack\n");
    return -1;
  }

  se = &hlists.stack[hlists.tos++];
  se->duplicates = 0;
  se->psp = psp;
 exit:
  se->hlist = hlist;
  se->fpath = strdup(fpath);
  return se - hlists.stack;;
}

/* 
 * DOS allows more than one open (not finished) findfirst/findnext!
 * But repeated findfirst with more than HLIST_WATCH_CNT hlist positions 
 * is an indicator for possible broken findfirsts/findnexts.
 * We are looking for more than HLIST_WATCH_CNT hlist positions,
 * these are candidates to watch for deletion. --ms
 */
static inline void hlist_set_watch(unsigned psp)
{
  struct stack_entry *se;
  int cnt = 0;

  if (hlists.watch) return; /* watching in progress */
          
  se = &hlists.stack[hlists.tos];
  for (se = hlists.stack; se < &hlists.stack[hlists.tos]; se++) {
    if ((se->psp == psp) && (++cnt > HLIST_WATCH_CNT)) {
      /* we set all findfirst of these PSP onto the watchlist */
      hlists.watch = 1;	/* watching on */
      Debug0((dbg_fd, "watch hlist_stack for PSP=%d\n", psp));
      return;
    }
  }
}

static inline void hlist_pop(int indx, unsigned psp)
{
  struct stack_entry *se = &hlists.stack[indx];
  Debug0((dbg_fd, "hlist_pop: %d popping=%d PSP=%d\n", hlists.tos, indx, psp));
  if (hlists.tos <= indx)
    return;
  if (se->psp != psp) {
    Debug0((dbg_fd, "hlist_pop: psp mismatch\n"));
    return;
  }
  if (se->duplicates) {
    Debug0((dbg_fd, "hlist_pop: don't pop duplicates\n"));
    se->duplicates--;
    return;
  }
  if (se->hlist != NULL) {
    Debug0((dbg_fd, "hlist_pop: popped list not empty?!\n"));
  }
  free_list(se, FALSE);
  
  for (se = &hlists.stack[hlists.tos-1];
       se >= hlists.stack && se->hlist == NULL;
       --se);
  hlists.tos = se - hlists.stack + 1;

  Debug0((dbg_fd, "hlist_pop: %d poped=%d PSP=%d\n",
		 hlists.tos, indx, psp));
}

static inline void hlist_pop_psp(unsigned psp)
{
  struct stack_entry *se;
  int new_tos = hlists.tos;
  Debug0((dbg_fd, "hlist_pop_psp: PSP=%d\n", psp));

  hlists.watch = 0;	/* reset, we give the previous PSP a new chance --ms */

  for (se = hlists.stack; se < &hlists.stack[hlists.tos]; se++) {
    if (se->psp == psp && se->hlist != NULL) {
      Debug0((dbg_fd, "hlist_pop_psp: deleting hlist=%p\n", se->hlist));
      free_list(se, TRUE);
    }
    if (se->hlist != NULL) {
      new_tos = se - hlists.stack + 1;
    }            
  }
  hlists.tos = new_tos;
}


static inline void hlist_watch_pop(unsigned psp)
{
  int act_seq = hlists.seq;
  struct stack_entry *se_del = NULL;
  struct stack_entry *se;

  /*
   * We delete simple the oldest hlist of the current PSP.
   * This means not simple a circuit buffer but a heap for the current PSP!
   * The oldest hlist is this one which is longest time
   * not touched for a find_next.
   * We never delete a hlist from a parent PSP!!!
   * We cancel only the oldest hlist of the current PSP.
   * --ms
   */

  for (se = hlists.stack; se < &hlists.stack[hlists.tos]; se++) {
    if (se->psp != psp)
      continue;

    if (se->seq < 0) {
      se_del = NULL;	/* we have found a gap, it is not neccessary ... */
      break;
    }

    if (hlists.watch && (se->seq > 0) && (se->seq < act_seq)) {
      se_del = se;
      act_seq = se->seq;
    }
  }
  if (se_del != NULL) {
    se = se_del;
    Debug0((dbg_fd, "hlist_watch_pop: deleting ind=%td hlist=%p\n",
						se_del-hlists.stack,se->hlist));
    free_list(se, TRUE);
    se->seq = -1; /* done */
  }

  /* shrinking hlists.stack.hlist if is possible
   */
  se = &hlists.stack[hlists.tos];
  while (se > hlists.stack) {
    se--;
    if (se->hlist != NULL) {
      se++;
      break;
    }
    Debug0((dbg_fd, "hlist_watch_pop: shrinking stack_top=%td\n",
						se - hlists.stack));
    hlists.watch = 0;	/* reset watch */
  }
  hlists.tos = se - hlists.stack;
}

#if 0
static void
debug_dump_sft(char handle)
{
  u_short *ptr;
  u_char *sptr;
  int sftn;

  ptr = (u_short *) (FARPTR((far_t *) (lol + 0x4)));

  Debug0((dbg_fd, "SFT: han = 0x%x, sftptr = %p\n",
	  handle, (void *) ptr));

  /* Assume 3.1 or 3.3 Dos */
  sftn = handle;
  while (TRUE) {
    if ((*ptr == 0xffff) && (ptr[2] < sftn)) {
      Debug0((dbg_fd, "handle invalid.\n"));
      break;
    }
    if (ptr[2] > sftn) {
      sptr = (u_char *) & ptr[3];
      while (sftn--)
	sptr += 0x35;		/* dos 3.1 3.3 */
      Debug0((dbg_fd, "handle_count = %x\n",
	      sft_handle_cnt(sptr)));
      Debug0((dbg_fd, "open_mode = %x\n",
	      sft_open_mode(sptr)));
      Debug0((dbg_fd, "attribute byte = %x\n",
	      sft_attribute_byte(sptr)));
      Debug0((dbg_fd, "device_info = %x\n",
	      sft_device_info(sptr)));
      Debug0((dbg_fd, "dev_drive_ptr = %lx\n",
	      sft_dev_drive_ptr(sptr)));
      Debug0((dbg_fd, "starting cluster = %x\n",
	      sft_start_cluster(sptr)));
      Debug0((dbg_fd, "file time = %x\n",
	      sft_time(sptr)));
      Debug0((dbg_fd, "file date = %x\n",
	      sft_date(sptr)));
      Debug0((dbg_fd, "file size = %lx\n",
	      sft_size(sptr)));
      Debug0((dbg_fd, "pos = %lx\n",
	      sft_position(sptr)));
      Debug0((dbg_fd, "rel cluster = %x\n",
	      sft_rel_cluster(sptr)));
      Debug0((dbg_fd, "abs cluster = %x\n",
	      sft_abs_cluster(sptr)));
      Debug0((dbg_fd, "dir sector = %x\n",
	      sft_directory_sector(sptr)));
      Debug0((dbg_fd, "dir entry = %x\n",
	      sft_directory_entry(sptr)));
      Debug0((dbg_fd, "name = %.8s\n",
	      sft_name(sptr)));
      Debug0((dbg_fd, "ext = %.3s\n",
	      sft_ext(sptr)));
      return;
    }
    sftn -= ptr[2];
    ptr = (u_short *) Addr_8086(ptr[1], ptr[0]);
  }
}
#endif

/* convert forward slashes to back slashes for DOS */

static void
path_to_dos(char *path)
{
  char *s;

  for (s = path; (s = strchr(s, '/')) != NULL; ++s)
    *s = '\\';
}

static int
GetRedirection(state_t *state, u_short index)
{
  int dd;
  u_short returnBX;		/* see notes below */
  u_short returnCX;
  char *resourceName;
  char *deviceName;
  u_short *userStack;

  /* Set number of redirected drives to 0 prior to getting new
	   Count */
  /* BH has device status 0=valid */
  /* BL has the device type - 3 for printer, 4 for disk */
  /* CX is supposed to be used to return the stored redirection parameter */
  /* I'm going to cheat and return a read-only flag in it */
  Debug0((dbg_fd, "GetRedirection, index=%d\n", index));
  for (dd = 0; dd < num_drives; dd++) {
    if (drives[dd].root) {
      if (index == 0) {
	/* return information for this drive */
	Debug0((dbg_fd, "redirection root =%s\n", drives[dd].root));
	deviceName = (u_char *) Addr(state, ds, esi);
	deviceName[0] = 'A' + dd;
	deviceName[1] = ':';
	deviceName[2] = EOS;
	resourceName = (u_char *) Addr(state, es, edi);
	strcpy(resourceName, LINUX_RESOURCE);
	strcat(resourceName, drives[dd].root);
	path_to_dos(resourceName);
	Debug0((dbg_fd, "resource name =%s\n", resourceName));
	Debug0((dbg_fd, "device name =%s\n", deviceName));
	userStack = (u_short *) sda_user_stack(sda);

	/* have to return BX, and CX on the user return stack */
	/* return a "valid" disk redirection */
	returnBX = 4;		/*BH=0, BL=4 */

	/* set the high bit of the return CL so that */
	/* NetWare shell doesn't get confused */
	returnCX = drives[dd].read_only | 0x80;

	Debug0((dbg_fd, "GetRedirection "
		"user stack=%p, CX=%x\n",
		(void *) userStack, returnCX));
	userStack[1] = returnBX;
	userStack[2] = returnCX;
	/* XXXTRB - should set session number in returnBP if */
	/* we are doing an extended getredirection */
	return (TRUE);
      }
      else {
	/* count down until the index is exhausted */
	index--;
      }
    }
  }
  if (IS_REDIRECTED(0x2f)) {
    redirected_drives = WORD(state->ebx) - index;
    SETWORD(&(state->ebx), index);
    Debug0((dbg_fd, "GetRedirect passing index of %d, Total redirected=%d\n", index, redirected_drives));
    return (REDIRECT);
  }

  SETWORD(&(state->eax), NO_MORE_FILES);
  return (FALSE);
}

/*****************************
 * GetRedirectionRoot - get the root on the Linux fs of a redirected drive
 * on entry:
 * on exit:
 *   Returns 0 on success, otherwise some error code.
 * notes:
 *   This function is used internally by DOSEMU (for userhooks)
 *   Take care of freeing resourceName after calling this
 *****************************/
int
GetRedirectionRoot(int dsk, char **resourceName,int *ro_flag)
{
  if (!drives[dsk].root) return 1;
  *resourceName = malloc(PATH_MAX + 1);
  if (*resourceName == NULL) return 1;
  strcpy(*resourceName, drives[dsk].root );
  *ro_flag=drives[dsk].read_only;
  return 0;

}

/*****************************
 * RedirectDisk - redirect a disk to the Linux file system
 * on entry:
 * on exit:
 *   Returns 0 on success, otherwise some error code.
 * notes:
 *   This function is used internally by DOSEMU, in contrast to
 *   RedirectDevice(), which must be called from DOS.
 *****************************/
int
RedirectDisk(int dsk, char *resourceName, int ro_flag)
{
  char path[256];
  unsigned char *p;
  int i;
  cds_t cds;

  *path = 0;

  Debug0((dbg_fd, "RedirectDisk %c: to %s\n", dsk + 'A', resourceName));

  cdsfarptr = lol_cdsfarptr(lol);
  cds_base = MK_FP32(cdsfarptr.segment, cdsfarptr.offset);

  /* see if drive is in range of valid drives */
  if(dsk < 0 || dsk > lol_last_drive(lol)) return 1;

  cds = drive_cds(dsk);

  /* see if drive is already redirected */
  if(cds_flags(cds) & CDS_FLAG_REMOTE) return 2;

  /* see if drive is currently substituted */
  if(cds_flags(cds) & CDS_FLAG_SUBST) return 3;

  path_to_ufs(path, 0, &resourceName[strlen(LINUX_RESOURCE)], 1, 0);

  i = init_drive(dsk, path, ro_flag) == 0 ? 4 : 0;

  if(!i) {
    /* Make drive known to DOS.
     *
     * Actually DOS will, given the chance, fill in this field by itself.
     * However, the command line parse function (int 0x21, ah=0x29) will not.
     * -- sw
     */
    p = drive_cds(dsk);
    p[cds_flags_off] = 0x80;
    p[cds_flags_off + 1] = 0xc0;
  }

#if 0
  {
    unsigned char *p = drive_cds(dsk);
    unsigned char c;
    int i, j;

    for(j = 0; j < 0x6; j++) {
      ds_printf("%05x ", j * 0x10 + (unsigned) p);
      for(i = 0; i < 0x10; i++) ds_printf(" %02x", p[i + 0x10*j]);
      ds_printf("  ");
      for(i = 0; i < 0x10; i++) {
        c = p[i + 0x10*j];
        ds_printf("%c", c >= 0x20 && c < 0x7f ? c : '.');
      }
      ds_printf("\n");
    }
  }
#endif

  return i;
}

/*****************************
 * RedirectDevice - redirect a drive to the Linux file system
 * on entry:
 *		cds_base should be set
 * on exit:
 * notes:
 *****************************/
static int
RedirectDevice(state_t * state)
{
  char *resourceName;
  char *deviceName;
  char path[256];
  int drive;
  cds_t cds;

  /* first, see if this is our resource to be redirected */
  resourceName = (u_char *) Addr(state, es, edi);
  deviceName = (u_char *) Addr(state, ds, esi);
  path[0] = 0;

  Debug0((dbg_fd, "RedirectDevice %s to %s\n", deviceName, resourceName));
  if (strncmp(resourceName, LINUX_RESOURCE,
	      strlen(LINUX_RESOURCE)) != 0) {
    /* pass call on to next redirector, if any */
    return (REDIRECT);
  }
  /* see what device is to be redirected */
  /* we only support disk redirection right now */
  if (LOW(state->ebx) != 4 || deviceName[1] != ':') {
    SETWORD(&(state->eax), FUNC_NUM_IVALID);
    return (FALSE);
  }
  drive = toupper(deviceName[0]) - 'A';

  /* see if drive is in range of valid drives */
  if (drive < 0 || drive > lol_last_drive(lol)) {
    SETWORD(&(state->eax), DISK_DRIVE_INVALID);
    return (FALSE);
  }
  cds = drive_cds(drive);
  /* see if drive is already redirected */
  if (cds_flags(cds) & CDS_FLAG_REMOTE) {
    SETWORD(&(state->eax), DUPLICATE_REDIR);
    return (FALSE);
  }
  /* see if drive is currently substituted */
  if (cds_flags(cds) & CDS_FLAG_SUBST) {
    SETWORD(&(state->eax), DUPLICATE_REDIR);
    return (FALSE);
  }

  path_to_ufs(path, 0, &resourceName[strlen(LINUX_RESOURCE)], 1, 0);

  /* if low bit of CX is set, then set for read only access */
  Debug0((dbg_fd, "read-only/cdrom attribute = %d\n",
	  (int) (state->ecx & 7)));
  if (init_drive(drive, path, state->ecx & 7) == 0) {
    SETWORD(&(state->eax), NETWORK_NAME_NOT_FOUND);
    return (FALSE);
  }
  else {
    return (TRUE);
  }
}

int ResetRedirection(int dsk)
{
  /* Do we own this drive? */
  if(drives[dsk].root == NULL) return 2;

  /* first, clean up my information */
  free(drives[dsk].root);
  drives[dsk].root = NULL;
  drives[dsk].root_len = 0;
  drives[dsk].read_only = FALSE;
  unregister_cdrom(dsk);
  return 0;
}

static void RemoveRedirection(int drive)
{
  char *path;
  far_t DBPptr;
  cds_t cds = drive_cds(drive);

  /* reset information in the CDS for this drive */
  cds_flags(cds) = 0;		/* default to a "not ready" drive */

  path = cds_current_path(cds);
  /* set the current path for the drive */
  path[0] = drive + 'A';
  path[1] = ':';
  path[2] = '\\';
  path[3] = EOS;
  cds_rootlen(cds) = CDS_DEFAULT_ROOT_LEN;
  cds_cur_cluster(cds) = 0;	/* reset us at the root of the drive */

  /* see if there is a physical drive behind this redirection */
  DBPptr = cds_DBP_pointer(cds);
  if (DBPptr.offset | DBPptr.segment) {
    /* if DBP_pointer is non-NULL, set the drive status to ready */
    cds_flags(cds) = CDS_FLAG_READY;
  }
}

/*****************************
 * CancelDiskRedirection - cancel a drive redirection
 * on entry:
 * on exit:
 *   Returns 0 on success, otherwise some error code.
 * notes:
 *   This function is used internally by DOSEMU, in contrast to
 *   CancelRedirection(), which must be called from DOS.
 *****************************/
int
CancelDiskRedirection(int dsk)
{
  Debug0((dbg_fd, "CancelDiskRedirection on %c:\n", dsk + 'A'));

  cdsfarptr = lol_cdsfarptr(lol);
  cds_base = MK_FP32(cdsfarptr.segment, cdsfarptr.offset);

  /* see if drive is in range of valid drives */
  if(dsk < 0 || dsk > lol_last_drive(lol)) return 1;

  if (ResetRedirection(dsk) != 0)
    return 2;
  RemoveRedirection(dsk);
  return 0;
}

/*****************************
 * CancelRedirection - cancel a drive redirection
 * on entry:
 *		cds_base should be set
 * on exit:
 * notes:
 *****************************/
static int
CancelRedirection(state_t * state)
{
  char *deviceName;
  int drive;

  /* first, see if this is one of our current redirections */
  deviceName = (u_char *) Addr(state, ds, esi);

  Debug0((dbg_fd, "CancelRedirection on %s\n", deviceName));
  if (deviceName[1] != ':') {
    /* we only handle drive redirections, pass it through */
    return (REDIRECT);
  }
  drive = toupper(deviceName[0]) - 'A';

  /* see if drive is in range of valid drives */
  if (drive < 0 || drive > lol_last_drive(lol)) {
    SETWORD(&(state->eax), DISK_DRIVE_INVALID);
    return (FALSE);
  }
  if (ResetRedirection(drive) != 0)
    /* we don't own this drive, pass it through to next redirector */
    return (REDIRECT);

  RemoveRedirection(drive);

  Debug0((dbg_fd, "CancelRedirection on %s completed\n", deviceName));
  return (TRUE);
}

static int lock_file_region(int fd, int cmd, struct flock *fl, long long start, unsigned long len)
{
  fl->l_whence = SEEK_SET;
  fl->l_pid = 0;
  /* first handle magic file lock value */
  if (start == 0x100000000LL && config.full_file_locks) {
    start = len = 0;
  }
#ifdef F_GETLK64
  if (cmd == F_SETLK64 || cmd == F_GETLK64) {
    struct flock64 fl64;
    int result;
    Debug0((dbg_fd, "Large file locking start=%llx, len=%lx\n", start, len));
    fl64.l_type = fl->l_type;
    fl64.l_whence = fl->l_whence;
    fl64.l_pid = fl->l_pid;
    fl64.l_start = start;
    fl64.l_len = len;
    result = fcntl( fd, cmd, &fl64 );
    fl->l_type = fl64.l_type;
    fl->l_start = (long) fl64.l_start;
    fl->l_len = (long) fl64.l_len;
    return result;
  }
#endif
  if (start == 0x100000000LL)
    start = 0x7fffffff;
  fl->l_start = start;
  fl->l_len = len;
  return fcntl( fd, cmd, fl );
}

static boolean_t
share(int fd, int mode, int drive, sft_t sft)
{
  /*
   * Return whether FD doesn't break any sharing rules.  FD was opened for
   * writing if WRITING and for reading otherwise.
   *
   * Written by Maxim Ruchko, and moved into a separate function by Nick
   * Duffek <nsd@bbc.com>.  Here are Maxim's original comments:
   *
   * IMHO, to handle sharing modes at this moment,
   * it's impossible to know wether an other process already
   * has been opened this file in shared mode.
   * But DOS programs (FoxPro 2.6 for example) need ACCESS_DENIED
   * as return code _at_ _this_ _point_, if they are opening
   * the file exclusively and the file has been opened elsewhere.
   * I place a lock in a predefined place at max possible lock length
   * in order to emulate the exclusive lock feature of DOS.
   * This lock is 'invisible' to DOS programs because the code
   * (extracted from the Samba project) in mfs lock requires that the
   * handler wrapps the locks below or equal 0x3fffffff (mask=0xC0000000)
   * So, 0x3fffffff + 0x3fffffff = 0x7ffffffe 
   * and 0x7fffffff is my start position.  --Maxim Ruchko
   */
  struct flock fl;
  int ret;
  int share_mode = ( sft_open_mode( sft ) >> 4 ) & 0x7;
  fl.l_type = F_WRLCK;
  /* see whatever locks are possible */

#ifdef F_GETLK64
  ret = lock_file_region( fd, F_GETLK64, &fl, 0x100000000LL, 1 );
  if ( ret == -1 && errno == EINVAL )
#endif
    ret = lock_file_region( fd, F_GETLK, &fl, 0x100000000LL, 1 );
  if ( ret == -1 ) {
  /* work around Linux's NFS locking problems (June 1999) -- sw */
    static unsigned char u[26] = { 0, };
    if(drive < 26) {
      if(!u[drive])
        fprintf(stderr,
                "SHAREing doesn't work on drive %c: (probably NFS volume?)\n",
                drive + 'A'
          );
      u[drive] = 1;
    }
    return (TRUE);
  }

  /* end NFS fix */

  /* file is already locked? then do not even open */
  /* a Unix read lock prevents writing;
     a Unix write lock prevents reading and writing,
     but for DOS compatibility we allow reading for write locks */
  if ((fl.l_type == F_RDLCK && mode != O_RDONLY) ||
      (fl.l_type == F_WRLCK && mode != O_WRONLY))
    return FALSE;
  
  switch ( share_mode ) {
    /* this is a little heuristic and does not completely
       match to DOS behaviour. That would require tracking
       how existing fd's are opened and comparing st_dev
       and st_ino fields
       from Ralf Brown's interrupt list:
       (Table 01403)
Values of DOS 2-6.22 file sharing behavior:
          |     Second and subsequent Opens
 First    |Compat  Deny   Deny   Deny   Deny
 Open     |        All    Write  Read   None
          |R W RW R W RW R W RW R W RW R W RW
 - - - - -| - - - - - - - - - - - - - - - - -
 Compat R |Y Y Y  N N N  1 N N  N N N  1 N N
        W |Y Y Y  N N N  N N N  N N N  N N N
        RW|Y Y Y  N N N  N N N  N N N  N N N
 - - - - -|
 Deny   R |C C C  N N N  N N N  N N N  N N N
 All    W |C C C  N N N  N N N  N N N  N N N
        RW|C C C  N N N  N N N  N N N  N N N
 - - - - -|
 Deny   R |2 C C  N N N  Y N N  N N N  Y N N
 Write  W |C C C  N N N  N N N  Y N N  Y N N
        RW|C C C  N N N  N N N  N N N  Y N N
 - - - - -|
 Deny   R |C C C  N N N  N Y N  N N N  N Y N
 Read   W |C C C  N N N  N N N  N Y N  N Y N
        RW|C C C  N N N  N N N  N N N  N Y N
 - - - - -|
 Deny   R |2 C C  N N N  Y Y Y  N N N  Y Y Y
 None   W |C C C  N N N  N N N  Y Y Y  Y Y Y
        RW|C C C  N N N  N N N  N N N  Y Y Y

        our sharing behaviour:
 Compat R |Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
        W |Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
        RW|Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
 - - - - -|
 Deny   R |Y N N  N N N  Y N N  N N N  Y N N
 All    W |N N N  N N N  N N N  N Y N  N Y N
        RW|N N N  N N N  N N N  N Y N  N Y N
 - - - - -|
 Deny   R |Y N N  N N N  Y N N  N N N  Y N N
 Write  W |Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
        RW|Y N N  N N N  Y N N  N N N  Y N N
 - - - - -|
 Deny   R |Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
 Read   W |N N N  N N N  N N N  N Y N  N Y N
        RW|N N N  N N N  N N N  N Y N  N Y N
 - - - - -|
 Deny   R |Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
 None   W |Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y
        RW|Y Y Y  Y Y Y  Y Y Y  Y Y Y  Y Y Y

        Legend: Y = open succeeds, N = open fails with error code 05h
        C = open fails, INT 24 generated
        1 = open succeeds if file read-only, else fails with error code
        2 = open succeeds if file read-only, else fails with INT 24
    */
  case COMPAT_MODE:
    if (fl.l_type == F_WRLCK) return FALSE;
  case DENY_NONE:
    return TRUE;                   /* do not set locks at all */
  case DENY_WRITE:
    if (fl.l_type == F_WRLCK) return FALSE;
    if (mode == O_WRONLY) return TRUE; /* only apply read locks */
    fl.l_type = F_RDLCK;
    break;
  case DENY_READ:
    if (fl.l_type == F_RDLCK) return FALSE;
    if (mode == O_RDONLY) return TRUE; /* only apply write locks */
    fl.l_type = F_WRLCK;
    break;
  case DENY_ALL:
    if (fl.l_type == F_WRLCK || fl.l_type == F_RDLCK) return FALSE;
    fl.l_type = mode == O_RDONLY ? F_RDLCK : F_WRLCK;
    break;
  case FCB_MODE:
    if ((sft_open_mode(sft) & 0x8000) && (fl.l_type != F_WRLCK)) return TRUE;
    /* else fall through */
  default:
    Debug0((dbg_fd, "internal SHARE: unknown sharing mode %x\n",
	    share_mode));
    return FALSE;
    break;
  }
#ifdef F_SETLK64
  ret = lock_file_region( fd, F_SETLK64, &fl, 0x100000000LL, 1 );
  if ( ret == -1 && errno == EINVAL )
#endif
    lock_file_region( fd, F_SETLK, &fl, 0x100000000LL, 1 );
  Debug0((dbg_fd,
    "internal SHARE: locking: drive %c:, fd %d, type %d whence %d pid %d\n",
     drive + 'A', fd, fl.l_type, fl.l_whence, fl.l_pid
  ));

  return (TRUE);
}

/* returns pointer to the basename of fpath */
static char *getbasename(char *fpath)
{
  char *bs_pos = strrchr(fpath, '/');
  if (bs_pos == NULL) {
    strcpy(fpath, "/");
    bs_pos = fpath;
  }
  return bs_pos + 1;
}

/* finds unix directory name for fpath (case insensitive,
   possibly unmangled) */
static void find_dir(char *fpath, int drive)
{
  struct stat st;
  char *bs_pos, *buf;
  
  bs_pos = getbasename(fpath);
  if (bs_pos == fpath + 1)
    return;
  bs_pos--;
  buf = strdup(bs_pos);
  *bs_pos = EOS;
  find_file(fpath, &st, drive);
  strcat(fpath, buf);
  free(buf);
}

static void open_device(unsigned long devptr, char *fname, sft_t sft)
{
  unsigned char *dev =
    (unsigned char *)(((devptr & 0xffff0000) >> 12) + (devptr & 0xffff));
  memcpy(sft_name(sft), fname, 8);
  memset(sft_ext(sft), ' ', 3);
  sft_dev_drive_ptr(sft) = devptr;
  sft_directory_entry(sft) = 0;
  sft_directory_sector(sft) = 0;
  sft_attribute_byte(sft) = 0x40;
  sft_device_info(sft) =
    (((dev[4] | dev[5] << 8) & ~SFT_MASK) & ~SFT_FSHARED)
    | SFT_FDEVICE | SFT_FEOF;
  time_to_dos(time(NULL), &sft_date(sft), &sft_time(sft));
  sft_size(sft) = 0;
  sft_position(sft) = 0;
}

/* In writable Linux directories it is possible to have uids not equal
   to your own one. In that case Linux denies any chmod or utime,
   but DOS really expects any attribute/time set to succeed. We'll fake it
   with a warning */
static boolean_t dos_would_allow(char *fpath, const char *op, boolean_t equal)
{
  char *slash;
  if (errno != EPERM)
    return FALSE;

  slash = strrchr(fpath, '/');
  if (slash) *slash = '\0';
  if (slash == fpath)
    fpath = "/";
  if (access(fpath, W_OK) != 0)
    return FALSE;
  if (slash) *slash = '/';

  /* no need to warn if there was nothing to do */
  if (equal)
    return TRUE;

  warn("MFS: Ignoring request for %s(\"%s\") (%s), where DOS expects "
          "it to succeed.\n"
	   "MFS: If you are using the FAT file system, try to set the \"uid\" "
	   "mount option to your own uid or use \"quiet\".\n",
       op, fpath, strerror(errno));
  return TRUE;
}

static boolean_t find_again(boolean_t firstfind, int drive, char *fpath,
			    struct dir_list *hlist, state_t *state, sdb_t sdb)
{
  boolean_t is_root;
  u_char attr;
  int hlist_index = sdb_p_cluster(sdb);
  struct dir_ent *de;

  attr = sdb_attribute(sdb);
  is_root = (strlen(fpath) == drives[drive].root_len);

  while (hlist != NULL && sdb_dir_entry(sdb) < hlist->nr_entries) {

    de = &hlist->de[sdb_dir_entry(sdb)];

    sdb_dir_entry(sdb)++;

    if (!convert_compare(de->d_name, de->name, de->ext,
			 sdb_template_name(sdb), sdb_template_ext(sdb), is_root))
      continue;

    Debug0((dbg_fd, "find_again entered with %.8s.%.3s\n", de->name, de->ext));
    fill_entry(de, fpath, drive);
    sdb_file_attr(sdb) = de->attr;

    if (de->mode & S_IFDIR) {
      Debug0((dbg_fd, "Directory ---> YES 0x%x\n", de->mode));
      if (!(attr & DIRECTORY)) {
	continue;
      }
      if (de->long_path 
	  && strncmp(de->name, ".       ", 8)
	  && strncmp(de->name, "..      ", 8)) {
	/* Path is long, so we do not allow subdirectories
	   here. Instead return the entry as a regular file.
	*/
	sdb_file_attr(sdb) &= ~DIRECTORY;
	de->size = 0; /* fake empty file */
      }
    }
    time_to_dos(de->time,
		&sdb_file_date(sdb),
		&sdb_file_time(sdb));
    sdb_file_size(sdb) = de->size;
    strncpy(sdb_file_name(sdb), de->name, 8);
    strncpy(sdb_file_ext(sdb), de->ext, 3);

    Debug0((dbg_fd, "'%.8s'.'%.3s' hlist=%d\n",
	    sdb_file_name(sdb),
	    sdb_file_ext(sdb), hlist_index));

    if (sdb_dir_entry(sdb) >= hlist->nr_entries)
      hlist_pop(hlist_index, sda_cur_psp(sda));
    return (TRUE);
  }

  /* no matches or empty directory */
  Debug0((dbg_fd, "No more matches\n"));
#if 0 /* Hardly any directory is really empty (there are always some vol.labels,
         `.', or `..'), and NO_MORE_FILES is more convenient for this case.
         Moreover, Volkov Commander doesn't like FILE_NOT_FOUND to be returned
         as a result of findfirst, and keeps annoying me due to it. */
  if (firstfind)
    SETWORD(&(state->eax), FILE_NOT_FOUND);
  else
#endif
    SETWORD(&(state->eax), NO_MORE_FILES);
  return (FALSE);
}

void get_volume_label(char *fname, char *fext, char *lfn, int drive)
{
  char *label, *root, *p;
  char cdrom_label[32];
  Debug0((dbg_fd, "DO LABEL!!\n"));

  if (get_volume_label_cdrom(drive, cdrom_label)) {
    if (lfn) {
      strcpy(lfn, cdrom_label);
      return;
    }
    label = strdup(cdrom_label);
  } else {
    p = drives[drive].root;
    root = strdup(p);
    if (root[strlen(root) - 1] == '/' && strlen(root) > 1)
      root[strlen(root) - 1] = '\0';

    if (lfn) {
      snprintf(lfn, 260, "%s", root);
      free(root);
      return;
    }

    label = (char *) malloc(8 + 3 + 1);
    label[0] = '\0';

    if (strlen(label) + strlen(root) <= 8 + 3) {
      strcat(label, root);
    }
    else {
      strcat(label, root + strlen(root) - (8 + 3 - strlen(label)));
    }
    free(root);
  }
  p = label + strlen(label);
  if (p < label + 8 + 3)
    memset(p, ' ', label + 8 + 3 - p);

  memcpy(fname, label, 8);
  memcpy(fext, label + 8, 3);
  free(label);
}

int dos_rmdir(const char *filename1, int drive, int lfn)
{
  struct stat st;
  char fpath[PATH_MAX];

  Debug0((dbg_fd, "Remove Directory %s\n", filename1));
  if (drives[drive].read_only)
    return ACCESS_DENIED;
  build_ufs_path_(fpath, filename1, drive, !lfn);
  if (find_file(fpath, &st, drive) && !is_dos_device(fpath)) {
    if (rmdir(fpath) != 0) {
      Debug0((dbg_fd, "failed to remove directory %s\n", fpath));
      return ACCESS_DENIED;
    }
  }
  else {
    Debug0((dbg_fd, "couldn't find directory %s\n", fpath));
    return PATH_NOT_FOUND;
  }
  return 0;
}

int dos_mkdir(const char *filename1, int drive, int lfn)
{
  struct stat st;
  char fpath[PATH_MAX];

  Debug0((dbg_fd, "Make Directory %s\n", filename1));
  if (drives[drive].read_only || (!lfn && is_long_path(filename1)))
    return ACCESS_DENIED;
  build_ufs_path_(fpath, filename1, drive, !lfn);
  if (find_file(fpath, &st, drive) || is_dos_device(fpath)) {
    Debug0((dbg_fd, "make failed already dir or file '%s'\n",
	    fpath));
    return ACCESS_DENIED;
  }
  if (mkdir(fpath, 0775) != 0) {
    find_dir(fpath, drive);
    Debug0((dbg_fd, "trying '%s'\n", fpath));
    if (mkdir(fpath, 0755) != 0) {
      Debug0((dbg_fd, "make directory failed '%s'\n",
	      fpath));
      return PATH_NOT_FOUND;
    }
  }
  return 0;
}

int dos_rename(const char *filename1, const char *filename2, int drive, int lfn)
{
  struct stat st;
  char fpath[PATH_MAX];
  char buf[PATH_MAX];

  Debug0((dbg_fd, "Rename file fn1=%s fn2=%s\n", filename1, filename2));
  if (drives[drive].read_only)
    return ACCESS_DENIED;
  build_ufs_path_(fpath, filename2, drive, !lfn);
  if (find_file(fpath, &st, drive) || is_dos_device(fpath)) {
    Debug0((dbg_fd,"Rename, %s already exists\n", fpath));
    return ACCESS_DENIED;
  }
  find_dir(fpath, drive);

  build_ufs_path_(buf, filename1, drive, !lfn);
  if (!find_file(buf, &st, drive) || is_dos_device(buf)) {
    Debug0((dbg_fd, "Rename '%s' error.\n", buf));
    return PATH_NOT_FOUND;
  }

  if (rename(buf, fpath) != 0)
    return PATH_NOT_FOUND;

  Debug0((dbg_fd, "Rename file %s to %s\n", buf, fpath));
  return 0;
}

static int
dos_fs_redirect(state_t *state)
{
  char *filename1;
  char *filename2;
  char *dta;
  long s_pos=0;
  unsigned long devptr;
  u_char attr;
  u_char subfunc;
  u_short dos_mode, unix_mode;
  u_short FCBcall = 0;
  u_char create_file=0;
  int fd, drive;
  int cnt;
  int ret = REDIRECT;
  cds_t my_cds;
  sft_t sft;
  sdb_t sdb;
  char *bs_pos;
  char fname[8];
  char fext[3];
  char fpath[PATH_MAX];
  struct stat st;
  boolean_t long_path;
  struct dir_list *hlist;
  int hlist_index;
#if 0
  static char last_find_name[8] = "";
  static char last_find_ext[3] = "";
  static u_char last_find_dir = 0;
  static u_char last_find_drive = 0;
  char *resourceName;
  char *deviceName;
#endif

 dos_mode = 0;
 
  if (!mach_fs_enabled)
    return (REDIRECT);

  my_cds = sda_cds(sda);

  sft = LINEAR2UNIX(Addr(state, es, edi));

  Debug0((dbg_fd, "Entering dos_fs_redirect, FN=%02X\n",(int)LOW(state->eax)));

  drive = select_drive(state);
  if (drive == -1)
    return (REDIRECT);

  filename1 = sda_filename1(sda);
  filename2 = sda_filename2(sda);
  sdb = sda_sdb(sda);
  dta = sda_current_dta(sda);

#if 0
  Debug0((dbg_fd, "CDS current path: %s\n", cds_current_path(cds)));
  Debug0((dbg_fd, "Filename1 %s\n", filename1));
  Debug0((dbg_fd, "Filename2 %s\n", filename2));
  Debug0((dbg_fd, "sft %x\n", sft));
  Debug0((dbg_fd, "dta %x\n", dta));
  fflush(NULL);
#endif

  switch (LOW(state->eax)) {
  case INSTALLATION_CHECK:	/* 0x00 */
    Debug0((dbg_fd, "Installation check\n"));
    SETLOW(&(state->eax), 0xFF);
    return (TRUE);
  case REMOVE_DIRECTORY:	/* 0x01 */
  case REMOVE_DIRECTORY_2:	/* 0x02 */
  case MAKE_DIRECTORY:		/* 0x03 */
  case MAKE_DIRECTORY_2:	/* 0x04 */
    ret = (LOW(state->eax) >= MAKE_DIRECTORY ? dos_mkdir : dos_rmdir)
      (filename1, drive, 0);
    if (ret) {
      SETWORD(&(state->eax), ret);
      return (FALSE);
    }
    return (TRUE);
  case SET_CURRENT_DIRECTORY:	/* 0x05 */
    Debug0((dbg_fd, "set directory to: %s\n", filename1));
    if (is_long_path(filename1)) {
      /* Path is too long, so we block access */
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      return (FALSE);
    }
    build_ufs_path(fpath, filename1, drive);
    Debug0((dbg_fd, "set directory to ufs path: %s\n", fpath));

    /* Try the given path */
    if (!find_file(fpath, &st, drive) || is_dos_device(fpath)) {
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      return (FALSE);
    }
    if (!(st.st_mode & S_IFDIR)) {
      SETWORD(&(state->eax), PATH_NOT_FOUND);
      Debug0((dbg_fd, "Set Directory %s not found\n", fpath));
      return (FALSE);
    }
    /* strip trailing backslash in the filename */
    {
      size_t len = strlen(filename1);
      if (len > 3 && filename1[len-1] == '\\') {
        filename1[len-1] = '\0';
      }
    }
    Debug0((dbg_fd, "New CWD is %s\n", filename1));
    return (TRUE);
  case CLOSE_FILE:		/* 0x06 */
    cnt = sft_fd(sft);
    filename1 = open_files[cnt].name;
    fd = open_files[cnt].fd;
    Debug0((dbg_fd, "Close file %x (%s)\n", fd, filename1));
    Debug0((dbg_fd, "Handle cnt %d\n",
	    sft_handle_cnt(sft)));
    sft_handle_cnt(sft)--;
    if (sft_handle_cnt(sft) > 0) {
      Debug0((dbg_fd, "Still more handles\n"));
      return (TRUE);
    }
    else if (filename1 == NULL || close(fd) != 0) {
      Debug0((dbg_fd, "Close file fails\n"));
      if (filename1 != NULL) {
        free(filename1);
        open_files[cnt].name = NULL;
      }
      return (FALSE);
    }
    else {
      Debug0((dbg_fd, "Close file succeeds\n"));

      /* if bit 14 in device_info is set, dos requests to set the file 
         date/time on closing. R.Brown states this incorrectly (inverted).
      */
         
      if (sft_device_info(sft) & 0x4000) {  
         struct utimbuf ut;
         u_short dos_date=sft_date(sft),dos_time=sft_time(sft);
         
         Debug0((dbg_fd,"close: setting file date=%04x time=%04x [<- dos format]\n",
                 dos_date,dos_time));
         ut.actime=ut.modtime=time_to_unix(dos_date,dos_time);

         if (filename1!=NULL && *filename1)
	   dos_utime(filename1, &ut);
      }
      else {
         Debug0((dbg_fd,"close: not setting file date/time\n"));
      }
      if (filename1 != NULL) {
        free(filename1);
        open_files[cnt].name = NULL;
      }
      return (TRUE);
    }
  case READ_FILE:
    {				/* 0x08 */
      int return_val;
      int itisnow;

      cnt = WORD(state->ecx);
      if (open_files[sft_fd(sft)].name == NULL) {
        SETWORD(&(state->eax), ACCESS_DENIED);
        return (FALSE);
      }
      fd = open_files[sft_fd(sft)].fd;
      Debug0((dbg_fd, "Read file fd=%x, dta=%p, cnt=%d\n",
	      fd, (void *) dta, cnt));
      Debug0((dbg_fd, "Read file pos = %d\n",
	      sft_position(sft)));
      Debug0((dbg_fd, "Handle cnt %d\n",
	      sft_handle_cnt(sft)));
      itisnow = lseek(fd, sft_position(sft), SEEK_SET);
      if (itisnow < 0 && errno != ESPIPE) {
	SETWORD(&(state->ecx), 0);
	return (TRUE);
      }
      Debug0((dbg_fd, "Actual pos %d\n",
	      itisnow));

      ret = dos_read(fd, dta, cnt);

      Debug0((dbg_fd, "Read returned : %d\n",
	      ret));
      if (ret < 0) {
	Debug0((dbg_fd, "ERROR IS: %s\n", strerror(errno)));
	return (FALSE);
      }
      else if (ret < cnt) {
	SETWORD(&(state->ecx), ret);
	return_val = TRUE;
      }
      else {
	SETWORD(&(state->ecx), cnt);
	return_val = TRUE;
      }
      sft_position(sft) += ret;
      sft_abs_cluster(sft) = 0x174a;	/* XXX a test */
/*      Debug0((dbg_fd, "File data %02x %02x %02x\n",
	      dta[0], dta[1], dta[2])); */
      Debug0((dbg_fd, "Read file pos after = %d\n",
	      sft_position(sft)));
      return (return_val);
    }
  case WRITE_FILE:		/* 0x09 */
    if (open_files[sft_fd(sft)].name == NULL || drives[drive].read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    
    cnt = WORD(state->ecx);
    fd = open_files[sft_fd(sft)].fd;
    Debug0((dbg_fd, "Write file fd=%x count=%x sft_mode=%x\n", fd, cnt, sft_open_mode(sft)));

    /* According to U DOS 2, any write with a (cnt)=CX=0 should truncate fd to
   sft_size , do to how ftruncate works, I'll only do an ftruncate
   if the file's size is greater than the current file position. */

    if (!cnt && sft_size(sft) > sft_position(sft)) {
      Debug0((dbg_fd, "Applying O_TRUNC at %x\n", (int)s_pos));
      if (ftruncate(fd, (off_t) sft_position(sft))) {
	Debug0((dbg_fd, "O_TRUNC failed\n"));
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
      sft_size(sft) = sft_position(sft);
    }

    if (us_debug_level > Debug_Level_0) {
      s_pos = lseek(fd, sft_position(sft), SEEK_SET);
      if (s_pos < 0 && errno != ESPIPE) {
	SETWORD(&(state->ecx), 0);
	return (TRUE);
      }
    }
    Debug0((dbg_fd, "Handle cnt %d\n",
	    sft_handle_cnt(sft)));
    Debug0((dbg_fd, "sft_size = %x, sft_pos = %x, dta = %p, cnt = %x\n", (int)sft_size(sft), (int)sft_position(sft), (void *) dta, (int)cnt));
    if (us_debug_level > Debug_Level_0) {
      ret = dos_write(fd, dta, cnt);
      if ((ret + s_pos) > sft_size(sft)) {
	sft_size(sft) = ret + s_pos;
        if (ret == 0) {
          /* physically extend the file -- ftruncate() does not
             extend on all filesystems */
          lseek(fd, -1, SEEK_CUR);
          dos_write(fd, "", 1);
        }
      }
    }
    Debug0((dbg_fd, "write operation done,ret=%x\n", ret));
    if (us_debug_level > Debug_Level_0)
      if (ret < 0) {
	Debug0((dbg_fd, "Write Failed : %s\n", strerror(errno)));
	return (FALSE);
      }
    Debug0((dbg_fd, "sft_position=%u, Sft_size=%u\n",
	    sft_position(sft), sft_size(sft)));
    SETWORD(&(state->ecx), ret);
    sft_position(sft) += ret;
    sft_abs_cluster(sft) = 0x174a;	/* XXX a test */
    if (us_debug_level > Debug_Level_0)
      return (TRUE);
  case GET_DISK_SPACE:
    {				/* 0x0c */
#ifdef USE_DF_AND_AFS_STUFF
      unsigned int free, tot;

      Debug0((dbg_fd, "Get Disk Space\n"));
      build_ufs_path(fpath, cds_current_path(drive_cds(drive)), drive);

      if (find_file(fpath, &st, drive)) {
	if (get_disk_space(fpath, &free, &tot)) {
	  /* return unit = 512-byte blocks @ 1 spc, std for floppy */
	  int spc = 1;
	  int bps = 512;

	  while ((spc<32) && ((tot > 65535) || (free > 65535))) {
	    /* we're on an HD here! */
	    if (bps==512) {
	      bps = 1024;	/* try 1024-byte sectors first */
	    }
	    else
	      spc *= 2;		/* try bigger clusters, up to 32 */
	    free /= 2;
	    tot  /= 2;
	  }
	  /* report no more than 32*1024*64K = 2G, even if some
	     DOS version 7 can see more */
	  if (tot>65535) tot=65535;
	  if (free>65535) free=65535;

	  /* Ralf Brown says: AH=media ID byte - can we let it at 0 here? */
	  SETWORD(&(state->eax), spc);
	  SETWORD(&(state->edx), free);
	  SETWORD(&(state->ecx), bps);
	  SETWORD(&(state->ebx), tot);
	  Debug0((dbg_fd, "free=%d, tot=%d, bps=%d, spc=%d\n",
		  free, tot, bps, spc));

	  return (TRUE);
	}
	else {
	  Debug0((dbg_fd, "no ret gds\n"));
	}
      }
#endif /* USE_DF_AND_AFS_STUFF */
      break;
    }
  case SET_FILE_ATTRIBUTES:	/* 0x0e */
    {
      u_short att = *(u_short *) Addr(state, ss, esp);

      Debug0((dbg_fd, "Set File Attributes %s 0%o\n", filename1, att));
      if (drives[drive].read_only || is_long_path(filename1)) {
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }

      build_ufs_path(fpath, filename1, drive);
      Debug0((dbg_fd, "Set attr: '%s' --> 0%o\n", fpath, att));
      if (!find_file(fpath, &st, drive) || is_dos_device(fpath)) {
	SETWORD(&(state->eax), FILE_NOT_FOUND);
	return (FALSE);
      }
      if (set_dos_attr(fpath, st.st_mode, att) != 0) {
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
    }
    return (TRUE);

    break;
  case GET_FILE_ATTRIBUTES:	/* 0x0f */
    Debug0((dbg_fd, "Get File Attributes %s\n", filename1));
    build_ufs_path(fpath, filename1, drive);
    if (!find_file(fpath, &st, drive) || is_dos_device(fpath)) {
      Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }

    attr = get_dos_attr(fpath,st.st_mode,is_hidden(fpath));
    if (is_long_path(filename1)) {
      /* turn off directory attr for directories with long path */
      attr &= ~DIRECTORY;
    }

    SETWORD(&(state->eax), attr);
    state->ebx = st.st_size >> 16;
    state->edi = MASK16(st.st_size);
    return (TRUE);
  case RENAME_FILE:		/* 0x11 */
    ret = dos_rename(filename1, filename2, drive, 0);
    if (ret) {
      SETWORD(&(state->eax), ret);
      return (FALSE);
    }
    return (TRUE);    
  case DELETE_FILE:		/* 0x13 */
    {
      struct dir_list *dir_list = NULL;
      int errcode = 0;
      int i;
      struct dir_ent *de;

      Debug0((dbg_fd, "Delete file %s\n", filename1));
      if (drives[drive].read_only) {
	SETWORD(&(state->eax), ACCESS_DENIED);
	return (FALSE);
      }
      build_ufs_path(fpath, filename1, drive);
      if (is_dos_device(fpath))
      {
	SETWORD(&(state->eax), FILE_NOT_FOUND);
	return (FALSE);
      }
      
      auspr(filename1, fname, fext);

      bs_pos = getbasename(fpath);
      *bs_pos = '\0';
      dir_list = get_dir(fpath, fname, fext, drive);

      if (dir_list == NULL) {
	build_ufs_path(fpath, filename1, drive);
	if (!find_file(fpath, &st, drive)) {
	  SETWORD(&(state->eax), FILE_NOT_FOUND);
	  return (FALSE);
	}
        if (access(fpath, W_OK) == -1) {
          SETWORD(&(state->eax), ACCESS_DENIED);
          return (FALSE);
        }
        if (unlink(fpath) != 0) {
          Debug0((dbg_fd, "Delete failed(%s) %s\n", strerror(errno), fpath));
          if (errno == EACCES) {
            SETWORD(&(state->eax), ACCESS_DENIED);
          } else {
            SETWORD(&(state->eax), FILE_NOT_FOUND);
          }
	  return (FALSE);
	}
        Debug0((dbg_fd, "Deleted %s\n", fpath));
	return (TRUE);
      }

      cnt = strlen(fpath);
      fpath[cnt++] = SLASH;
      de = &dir_list->de[0];
      for(i = 0; i < dir_list->nr_entries; i++, de++) {
	if ((de->mode & S_IFMT) == S_IFREG) {
	  strcpy(fpath + cnt, de->d_name);
	  if (find_file(fpath, &st, drive)) {
            if (access(fpath, W_OK) == -1) {
              errcode = EACCES;
            } else {
              errcode = unlink(fpath) ? errno : 0;
            }
            if (errcode != 0) {
              Debug0((dbg_fd, "Delete failed(%s) %s\n", strerror(errcode), fpath));
            } else {
              Debug0((dbg_fd, "Deleted %s\n", fpath));
            }
	  }
	}
        if (errcode != 0) {
          if (errcode == EACCES) {
            SETWORD(&(state->eax), ACCESS_DENIED);
          } else {
            SETWORD(&(state->eax), FILE_NOT_FOUND);
          }
          free(dir_list->de);
          free(dir_list);
          return (FALSE);
        }
      }
      free(dir_list->de);
      free(dir_list);
      return (TRUE);
    }

    case OPEN_EXISTING_FILE:	/* 0x16 */
	/* according to the appendix in undoc dos 2 the top word on the
       stack holds the open mode.  Other than the definition in the
       appendix, I can find nothing else which supports this
       statement. */

    /* get the high byte */
    dos_mode = *(u_char *) (Addr(state, ss, esp) + 1);
    dos_mode <<= 8;

    /* and the low one (isn't there a way to do this with one Addr ??) */
    dos_mode |= *(u_char *)Addr(state, ss, esp);

    /* check for a high bit set indicating an FCB call */
    FCBcall = sft_open_mode(sft) & 0x8000;

    Debug0((dbg_fd, "(mode = 0x%04x)\n", dos_mode));
    Debug0((dbg_fd, "(sft_open_mode = 0x%04x)\n", sft_open_mode(sft)));

    if (FCBcall) {
	sft_open_mode(sft) |= 0x00f0;
    }
    else {
       /* Keeping sharing modes in sft also, --Maxim Ruchko */
	sft_open_mode(sft) = dos_mode & 0xFF;
    }
    dos_mode &= 0xF;
	
	/*
	   This method is ALSO in undoc dos.  They have the command
	   defined differently in two different places.  The important
	   thing is that this doesn't work under dr-dos 6.0

    attr = *(u_short *) Addr(state, ss, esp) */


    Debug0((dbg_fd, "Open existing file %s\n", filename1));

  do_open_existing:
    if (drives[drive].read_only && dos_mode != READ_ACC) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    build_ufs_path(fpath, filename1, drive);
    auspr(filename1, fname, fext);
    if (!find_file(fpath, &st, drive)) {
      Debug0((dbg_fd, "open failed: '%s'\n", fpath));
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }
    devptr = is_dos_device(fpath);
    if (devptr) {
      open_device (devptr, fname, sft);
      Debug0((dbg_fd, "device open succeeds: '%s'\n", fpath));
      return (TRUE);
    }
    if (st.st_mode & S_IFDIR) {
      Debug0((dbg_fd, "S_IFDIR: '%s'\n", fpath));
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }
    attr = get_dos_attr(fpath,st.st_mode,is_hidden(fpath));
    if (dos_mode == READ_ACC) {
      unix_mode = O_RDONLY;
    }
    else if (dos_mode == WRITE_ACC) {
      unix_mode = O_WRONLY;
    }
    else if (dos_mode == READ_WRITE_ACC) {
      unix_mode = O_RDWR;
    }
    else if (dos_mode == 0x40) { /* what's this mode ?? */
      unix_mode = O_RDWR;
    }
    else {
      Debug0((dbg_fd, "Illegal access_mode 0x%x\n", dos_mode));
      unix_mode = O_RDONLY;
    }
    if (drives[drive].read_only && unix_mode != O_RDONLY) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }

    if ((fd = open(fpath, unix_mode )) < 0) {
      Debug0((dbg_fd, "access denied:'%s'\n", fpath));
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    if (!share(fd, unix_mode & O_ACCMODE, drive, sft)) {
      close( fd );
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }

    memcpy(sft_name(sft), fname, 8);
    memcpy(sft_ext(sft), fext, 3);

#if 0
    if (FCBcall)
      sft_open_mode(sft) |= 0x00F0;
#endif

    /* store the name for FILE_CLOSE */
    sft_directory_entry(sft) = 0;
    sft_directory_sector(sft) = 0;
    sft_attribute_byte(sft) = attr;
    sft_device_info(sft) = (drive & 0x1f) + (0x8040);
    time_to_dos(st.st_mtime,
		&sft_date(sft), &sft_time(sft));
    sft_size(sft) = st.st_size;
    sft_position(sft) = 0;
    for (cnt = 0; cnt < 255; cnt++)
    {
      if (open_files[cnt].name == NULL) {
        open_files[cnt].name = strdup(fpath);
        open_files[cnt].fd = fd;
        sft_fd(sft) = cnt;
        break;
      }
    }
    if (cnt == 255)
    {
      error("Panic: too many open files\n");
      leavedos(1);
    }
    Debug0((dbg_fd, "open succeeds: '%s' fd = 0x%x\n", fpath, fd));
    Debug0((dbg_fd, "Size : %ld\n", (long) st.st_size));

    /* If FCB open requested, we need to call int2f 0x120c */
    if (FCBcall) {
      Debug0((dbg_fd, "FCB Open calling int2f 0x120c\n"));
      fake_int_to(INTE7_SEG, INTE7_OFF);
    }

    return (TRUE);
  case CREATE_TRUNCATE_NO_CDS:	/* 0x18 */
  case CREATE_TRUNCATE_FILE:	/* 0x17 */

    FCBcall = sft_open_mode(sft) & 0x8000;
    Debug0((dbg_fd, "FCBcall=0x%x\n", FCBcall));

    /* 01 in high byte = create new, 00 s just create truncate */
    create_file = *(u_char *) (Addr(state, ss, esp) + 1);

    attr = *(u_short *) Addr(state, ss, esp);
    Debug0((dbg_fd, "CHECK attr=0x%x, create=0x%x\n", attr, create_file));

    /* make it a byte - we thus ignore the new bit */
    attr &= 0xFF;
    if (attr & DIRECTORY) return(REDIRECT);

    Debug0((dbg_fd, "Create truncate file %s attr=%x\n", filename1, attr));

  do_create_truncate:
    if (drives[drive].read_only) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }
    build_ufs_path(fpath, filename1, drive);
    auspr(filename1, fname, fext);
    if (find_file(fpath, &st, drive)) {
      devptr = is_dos_device(fpath);
      if (devptr) {
        open_device (devptr, fname, sft);
        Debug0((dbg_fd, "device open succeeds: '%s'\n", fpath));
        return (TRUE);
      }
      Debug0((dbg_fd, "st.st_mode = 0x%02x, handles=%d\n", st.st_mode, sft_handle_cnt(sft)));
      if ( /* !(st.st_mode & S_IFREG) || */ create_file) {
	SETWORD(&(state->eax), FILE_ALREADY_EXISTS);
	Debug0((dbg_fd, "File exists '%s'\n", fpath));
	return (FALSE);
      }
    }

    if ((fd = open(fpath, (O_RDWR | O_CREAT),
		   get_unix_attr(0664, attr))) < 0) {
      find_dir(fpath, drive);
      Debug0((dbg_fd, "trying '%s'\n", fpath));
      if ((fd = open(fpath, (O_RDWR | O_CREAT),
		     get_unix_attr(0664, attr))) < 0) {
	Debug0((dbg_fd, "can't open %s: %s (%d)\n",
		fpath, strerror(errno), errno));
#if 1
	SETWORD(&(state->eax), FILE_NOT_FOUND);
#else
	SETWORD(&(state->eax), ACCESS_DENIED);
#endif
	return (FALSE);
      }
    }
    set_fat_attr(fd, attr);

    if (!share(fd, O_RDWR, drive, sft) || ftruncate(fd, 0) != 0) {
      Debug0((dbg_fd, "unable to truncate %s: %s (%d)\n",
	      fpath, strerror(errno), errno));
      close(fd);
      SETWORD(&(state->eax), ACCESS_DENIED);
      return FALSE;
    }

    memcpy(sft_name(sft), fname, 8);
    memcpy(sft_ext(sft), fext, 3);

    /* This caused a bug with temporary files so they couldn't be read,
   they were made write-only */
#if 0
    sft_open_mode(sft) = 0x1;
#else
    if (FCBcall)
      sft_open_mode(sft) |= 0x00f0;
    else
  #if 0
      sft_open_mode(sft) = 0x3; /* write only */
  #else
      sft_open_mode(sft) = 0x2; /* read/write */
  #endif
#endif
    sft_directory_entry(sft) = 0;
    sft_directory_sector(sft) = 0;
    sft_attribute_byte(sft) = attr;
    sft_device_info(sft) = (drive & 0x1f) + (0x8040);
    time_to_dos(st.st_mtime, &sft_date(sft),
		&sft_time(sft));

    /* file size starts at 0 bytes */
    sft_size(sft) = 0;
    sft_position(sft) = 0;
    for (cnt = 0; cnt < 255; cnt++)
    {
      if (open_files[cnt].name == NULL) {
        open_files[cnt].name = strdup(fpath);
        open_files[cnt].fd = fd;
        sft_fd(sft) = cnt;
        break;
      }
    }
    if (cnt == 255)
    {
      error("Panic: too many open files\n");
      leavedos(1);
    }
    Debug0((dbg_fd, "create succeeds: '%s' fd = 0x%x\n", fpath, fd));
    Debug0((dbg_fd, "size = 0x%x\n", sft_size(sft)));

    /* If FCB open requested, we need to call int2f 0x120c */
    if (FCBcall) {
      Debug0((dbg_fd, "FCB Open calling int2f 0x120c\n"));
      fake_int_to(INTE7_SEG, INTE7_OFF);
    }
    return (TRUE);

  case FIND_FIRST_NO_CDS:	/* 0x19 */
  case FIND_FIRST:		/* 0x1b */

    attr = sda_search_attribute(sda);

    Debug0((dbg_fd, "findfirst %s attr=%x\n", filename1, attr));

    /* 
     * we examine the hlists.stack.hlist for broken find_firsts/find_nexts. --ms
     */
    hlist_watch_pop(sda_cur_psp(sda));

    /* check if path is long */
    long_path = is_long_path(filename1);

    if (!build_ufs_path(fpath, filename1, drive)) {
      sdb_p_cluster(sdb) = 0xffff; /* no findnext */
      sdb_file_attr(sdb) = 0;
      time_to_dos(time(NULL), &sdb_file_date(sdb), &sdb_file_time(sdb));
      sdb_file_size(sdb) = 0;
      memset(sdb_file_name(sdb), ' ', 8);
      memcpy(sdb_file_name(sdb), fpath, strlen(fpath));
      memset(sdb_file_ext(sdb), ' ', 3);
      return (TRUE);
    }

    auspr(filename1, fname, fext);
    memcpy(sdb_template_name(sdb), fname, 8);
    memcpy(sdb_template_ext(sdb), fext, 3);
    sdb_attribute(sdb) = attr;
    sdb_drive_letter(sdb) = 0x80 + drive;
    sdb_p_cluster(sdb) = 0xffff;  /* correct value later */

    Debug0((dbg_fd, "Find first %8.8s.%3.3s\n",
	    (char *) sdb_template_name(sdb),
	    (char *) sdb_template_ext(sdb)));


    if (((attr & (VOLUME_LABEL|DIRECTORY)) == VOLUME_LABEL) &&
	strncmp(sdb_template_name(sdb), "????????", 8) == 0 &&
	strncmp(sdb_template_ext(sdb), "???", 3) == 0) {
      get_volume_label(fname, fext, NULL, drive);
      memcpy(sdb_file_name(sdb), fname, 8);
      memcpy(sdb_file_ext(sdb), fext, 3);
      sdb_file_attr(sdb) = VOLUME_LABEL;
      sdb_dir_entry(sdb) = 0x0;

      /* We fill the hlist for labels not here,
       * we do it a few lines later. --ms
       */
    }

    bs_pos = getbasename(fpath);
    *bs_pos = '\0';
    /* for efficiency we don't read everything if there are no wildcards */
    if (!memchr(sdb_template_name(sdb), '?', 8) &&
        !memchr(sdb_template_ext(sdb), '?', 3)) {
      hlist = get_dir(fpath, sdb_template_name(sdb),
                      sdb_template_ext(sdb), drive);
      bs_pos = NULL;
    }
    else
      hlist = get_dir(fpath, NULL, NULL, drive);
    if (hlist==NULL)  {
      SETWORD(&(state->eax), NO_MORE_FILES);
      return (FALSE);
    }
    if (long_path) {
      set_long_path_on_dirs(hlist);
    }
    hlist_index = hlist_push(hlist, sda_cur_psp(sda), bs_pos ? fpath : "");
    sdb_dir_entry(sdb) = 0;
    sdb_p_cluster(sdb) = hlist_index;

    hlists.stack[hlist_index].seq = ++hlists.seq; /* new watch stamp --ms */
    hlist_set_watch(sda_cur_psp(sda));

    /*
     * This is the right place to leave this stuff for volume labels. --ms
     */
    if (((attr & (VOLUME_LABEL|DIRECTORY)) == VOLUME_LABEL) &&
        strncmp(sdb_template_name(sdb), "????????", 8) == 0 &&
        strncmp(sdb_template_ext(sdb), "???", 3) == 0) {
      Debug0((dbg_fd, "DONE LABEL!!\n"));

      return (TRUE);
    }
    return find_again(1, drive, fpath, hlist, state, sdb);

  case FIND_NEXT:		/* 0x1c */
    hlist_index = sdb_p_cluster(sdb);
    hlist = NULL;

    /* 
     * if watched find_next in progress, refresh sequence number. --ms
     */
    Debug0((dbg_fd, "Find next hlist_index=%d\n",hlist_index));

    if (hlist_index < hlists.tos) {
      if (hlists.stack[hlist_index].seq > 0)
        hlists.stack[hlist_index].seq = hlists.seq; 

      Debug0((dbg_fd, "Find next seq=%d\n",hlists.stack[hlist_index].seq));

      strcpy(fpath, hlists.stack[hlist_index].fpath);
      hlist = hlists.stack[hlist_index].hlist;
    }

    Debug0((dbg_fd, "Find next %8.8s.%3.3s, pointer->hlist=%p\n",
	    (char *) sdb_template_name(sdb),
	    (char *) sdb_template_ext(sdb), hlist));
    return find_again(0, drive, fpath, hlist, state, sdb);
  case CLOSE_ALL:		/* 0x1d */
    Debug0((dbg_fd, "Close All\n"));
    break;
  case FLUSH_ALL_DISK_BUFFERS:	/* 0x20 */
    Debug0((dbg_fd, "Flush Disk Buffers\n"));
    return TRUE;
  case SEEK_FROM_EOF:		/* 0x21 */
    {
      int offset = (state->ecx << 16) + WORD(state->edx);
      if (open_files[sft_fd(sft)].name == NULL) {
        SETWORD(&(state->eax), ACCESS_DENIED);
        return (FALSE);
      }
      fd = open_files[sft_fd(sft)].fd;
      Debug0((dbg_fd, "Seek From EOF fd=%x ofs=%d\n",
	      fd, offset));
      if (offset > 0)
	offset = -offset;
      offset = lseek(fd, offset, SEEK_END);
      Debug0((dbg_fd, "Seek returns fs=%d ofs=%d\n",
	      fd, offset));
      if (offset != -1) {
	sft_position(sft) = offset;
	SETWORD(&(state->edx), offset >> 16);
	SETWORD(&(state->eax), WORD(offset));
	return (TRUE);
      }
      else {
	SETWORD(&(state->eax), SEEK_ERROR);
	return (FALSE);
      }
    }
    break;
  case QUALIFY_FILENAME:	/* 0x23 */
    /* do nothing here */
    break;
  case LOCK_FILE_REGION:	/* 0x0a */
	/* The following code only apply to DOS 4.0 and later */
	/* It manage both LOCK and UNLOCK */
	/* I don't know how to find out from here which DOS is running */
	{
		int is_lock = !(state->ebx & 1);
		int fd = open_files[sft_fd(sft)].fd;
		int ret;
		struct LOCKREC{
			unsigned long offset,size;
		} *pt = (struct LOCKREC*) Addr (state,ds,edx);
		struct flock larg;
		unsigned long mask = 0xC0000000;

                if (open_files[sft_fd(sft)].name == NULL) {
                  SETWORD(&(state->eax), ACCESS_DENIED);
                  return (FALSE);
                }

		Debug0((dbg_fd, "lock requested, fd=%d, is_lock=%d, start=%lx, len=%lx\n",
			fd, is_lock, pt->offset, pt->size));

		if (pt->size > 0 && pt->offset + (pt->size - 1) < pt->offset) {
			Debug0((dbg_fd, "offset+size too large, lock failed.\n"));
			SETWORD(&(state->eax), ACCESS_DENIED);
			return FALSE;
		}

#if 1   /* The kernel can't place F_WRLCK on files opened read-only and
         * FoxPro fails. IMHO the right solution is:         --Maxim Ruchko */
		larg.l_type = is_lock ? (
			    ( sft_open_mode( sft ) & 0x7 ) ? F_WRLCK : F_RDLCK
			) : F_UNLCK;
#elif 0 /* fix for foxpro problems with lredired drives from
         * Sergey Suleimanov <solt@atibank.astrakhan.su>.
         * Needs more testing --Hans 98/01/30 */
		larg.l_type = is_lock ? F_RDLCK : F_UNLCK;
#else
		larg.l_type = is_lock ? F_WRLCK : F_UNLCK;
#endif
		larg.l_start = pt->offset;
		larg.l_len = pt->size;

		/*
			This is a superdooper patch extract from the Samba project
			We have no idea why this is there but it seem to work
			So a program running in DOSEMU will successfully cooperate
			with the same program running on a remote station and using
			samba to access the same database.

			After doing some experiment with a Clipper program
			(database driver from SuccessWare (SAX)), we have wittness
			unexpected large offset for small database. From this we
			assume that some DOS program are playing game with lock/unlock.
		*/

		/* make sure the count is reasonable, we might kill the lockd otherwise */
		larg.l_len &= ~mask;

		/* the offset is often strange - remove 2 of its bits if either of
			the top two bits are set. Shift the top ones by two bits. This
			still allows OLE2 apps to operate, but should stop lockd from
			dieing */
		if ((larg.l_start & mask) != 0)
			larg.l_start = (larg.l_start & ~mask) | ((larg.l_start & mask) >> 2);

#ifdef F_SETLK64
		ret = lock_file_region (fd,F_SETLK64,&larg,pt->offset,pt->size);
		if (ret == -1 && errno == EINVAL)
#endif
			ret = lock_file_region (fd,F_SETLK,&larg,larg.l_start,larg.l_len);
		Debug0((dbg_fd, "lock fd=%x rc=%x type=%x whence=%x start=%lx, len=%lx\n",
			fd, ret, larg.l_type, larg.l_whence, larg.l_start,larg.l_len));
		if (ret != -1) return TRUE; /* no error */
		ret = (errno == EAGAIN) ? FILE_LOCK_VIOLATION :
		      (errno == ENOLCK) ? SHARING_BUF_EXCEEDED :
		      ACCESS_DENIED;
		SETWORD(&(state->eax), ret); 
		return FALSE;
	}
    break;
  case UNLOCK_FILE_REGION:	/* 0x0b */
    Debug0((dbg_fd, "Unlock file region\n"));
    break;
  case PROCESS_TERMINATED:	/* 0x22*/
    Debug0((dbg_fd, "Process terminated PSP=%d\n", state->ds));
    hlist_pop_psp(state->ds);
    if (config.lfn) close_dirhandles(state->ds);
    return (REDIRECT);
  case CONTROL_REDIRECT:	/* 0x1e */
    /* get low word of parameter, should be one of 2, 3, 4, 5 */
    subfunc = LOW(*(u_short *) Addr(state, ss, esp));
    Debug0((dbg_fd, "Control redirect, subfunction %d\n",
	    subfunc));
    switch (subfunc) {
      /* XXXTRB - need to support redirection index pass-thru */
    case GET_REDIRECTION:
    case EXTENDED_GET_REDIRECTION:
      return (GetRedirection(state, WORD(state->ebx)));
      break;
    case REDIRECT_DEVICE:
      return (RedirectDevice(state));
      break;
    case CANCEL_REDIRECTION:
      return (CancelRedirection(state));
      break;
    default:
      SETWORD(&(state->eax), FUNC_NUM_IVALID);
      return (FALSE);
      break;
    }
    break;
  case COMMIT_FILE:		/* 0x07 */
    Debug0((dbg_fd, "Commit\n"));
    if (open_files[sft_fd(sft)].name == NULL) {
      SETWORD(&(state->eax), ACCESS_DENIED);
      return (FALSE);
    }    
    fd = open_files[sft_fd(sft)].fd;
    return (dos_flush(fd) == 0);
    break;
  case MULTIPURPOSE_OPEN:
    {
      boolean_t file_exists;
      u_short action = sda_ext_act(sda);
	  u_short mode;
	  
      mode = sda_ext_mode(sda) & 0x7f;
      attr = *(u_short *) Addr(state, ss, esp);
      Debug0((dbg_fd, "Multipurpose open file: %s\n", filename1));
      Debug0((dbg_fd, "Mode, action, attr = %x, %x, %x\n",
	      mode, action, attr));
      
      build_ufs_path(fpath, filename1, drive);
      file_exists = find_file(fpath, &st, drive);
      if (file_exists && is_dos_device(fpath))
        goto do_open_existing;

      if (((action & 0x10) == 0) && !file_exists) {
	/* Fail if file does not exist */
	SETWORD(&(state->eax), FILE_NOT_FOUND);
	return (FALSE);
      }

      if (((action & 0xf) == 0) && file_exists) {
	/* Fail if file does exist */
	SETWORD(&(state->eax), FILE_ALREADY_EXISTS);
	return (FALSE);
      }

      if (((action & 0xf) == 1) && file_exists) {
	/* Open if does exist */
	SETWORD(&(state->ecx), 0x1);
	dos_mode = mode & 0xF;
	goto do_open_existing;
      }

      if (((action & 0xf) == 2) && file_exists) {
	/* Replace if file exists */
	SETWORD(&(state->ecx), 0x3);
	goto do_create_truncate;
      }

      if (((action & 0x10) != 0) && !file_exists) {
	/* Replace if file exists */
	SETWORD(&(state->ecx), 0x2);
	goto do_create_truncate;
      }

      Debug0((dbg_fd, "Multiopen failed: 0x%02x\n",
	      (int) LOW(state->eax)));
      /* Fail if file does exist */
      SETWORD(&(state->eax), FILE_NOT_FOUND);
      return (FALSE);
    }
  case EXTENDED_ATTRIBUTES:{
      switch (LOW(state->ebx)) {
      case 2:			/* get extended attributes */
      case 3:			/* get extended attribute properties */
	if (WORD(state->ecx) >= 2) {
	  *(short *) (Addr(state, es, edi)) = 0;
	  SETWORD(&(state->ecx), 0x2);
	}
      case 4:			/* set extended attributes */
	return (TRUE);
      }
      return (FALSE);
    }
  case PRINTER_MODE:{
      Debug0((dbg_fd, "Printer Mode: %02x\n",(int) LOW(state->eax)));
      SETLOW(&(state->edx), 1);
      return (TRUE);
    }
#ifdef UNDOCUMENTED_FUNCTION_2
  case UNDOCUMENTED_FUNCTION_2:
    Debug0((dbg_fd, "Undocumented function: %02x\n",
	    (int) LOW(state->eax)));
    return (TRUE);
#endif
  case PRINTER_SETUP:
    SETWORD(&(state->ebx), WORD(state->ebx) - redirected_drives);
    Debug0((dbg_fd, "Passing %d to PRINTER SETUP CALL\n",
	    (int) WORD(state->ebx)));
    return (REDIRECT);
  default:
    Debug0((dbg_fd, "Default for undocumented function: %02x\n",
	    (int) LOW(state->eax)));
    return (REDIRECT);
  }
  return (ret);
}
