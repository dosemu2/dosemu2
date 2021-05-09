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

#if defined(__linux__)
#define DOSEMU 1		/* this is a port to dosemu */
#endif

#if !DOSEMU
#include "base.h"
#include "bios.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <utime.h>
#include <wchar.h>
#include <sys/mman.h>
#include <ctype.h>
#include <stdint.h>	// types used for seek/size

#if !DOSEMU
#include <mach/message.h>
#include <mach/exception.h>

#include "dos.h"
#else
#include <dirent.h>
#include <string.h>
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include <wctype.h>
#include "emu.h"
#include "int.h"
#include "mfs.h"
#include "lfn.h"
#include "dos2linux.h"
#include "doshelpers.h"
/* For passing through GetRedirection Status */
#include "memory.h"
#include "lowmem.h"
#include "redirect.h"
#include "mangle.h"
#include "utilities.h"
#include "coopth.h"
#include "lpt.h"
#endif

#ifdef __linux__
#include <linux/msdos_fs.h>
#endif

#define Addr_8086(x,y)  MK_FP32((x),(y) & 0xffff)
#define Addr(s,x,y)     Addr_8086(((s)->x), ((s)->y))
/* vfat_ioctl to use is short for int2f/ax=11xx, both for int21/ax=71xx */
#ifdef __linux__
static long vfat_ioctl = VFAT_IOCTL_READDIR_BOTH;
#endif
/* these universal globals defined here (externed in mfs.h) */
int mfs_enabled = FALSE;

static int emufs_loaded = FALSE;

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
#define	LOCK_UNLOCK_FILE_REGION	0xa
#define	UNLOCK_FILE_REGION_OLD	0xb
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
#define TURN_OFF_PRINTER	0x24
#define PRINTER_MODE  		0x25	/* Used in DOS 3.1+ */
#define EXTENDED_ATTRIBUTES	0x2d	/* Used in DOS 4.x */
#define MULTIPURPOSE_OPEN	0x2e	/* Used in DOS 4.0+ */
#define LONG_SEEK		0x42	/* extension */
#define GET_LARGE_FILE_INFO	0xa6	/* extension */

#define EOS		'\0'
#define	SLASH		'/'
#define BACKSLASH	'\\'

#define SFT_MASK 0x0060
#define SFT_FSHARED 0x8000
#define SFT_FDEVICE 0x0080
#define SFT_FEOF 0x0040

#define DOS_SEEK_SET 0
#define DOS_SEEK_CUR 1
#define DOS_SEEK_EOF 2

enum {DRV_NOT_FOUND, DRV_FOUND, DRV_NOT_ASSOCIATED};

enum { TYPE_NONE, TYPE_DISK, TYPE_PRINTER };
struct file_fd
{
  char *name;
  int idx;
  int fd;
  int type;
  int dir_fd;
  struct stat st;
  int is_writable;
  int write_allowed;
  uint64_t seek;
  uint64_t size;
  int lock_cnt;
};

/* Need to know how many drives are redirected */
static u_char redirected_drives = 0;
static struct drive_info drives[MAX_DRIVES];

static char *def_drives[MAX_DRIVE];
static int num_def_drives;

static int dos_fs_dev(struct vm86_regs *);
static int compare(char *, char *, char *, char *);
static int dos_fs_redirect(struct vm86_regs *, char *stk);
static int is_long_path(const char *s);
static void path_to_ufs(char *ufs, size_t ufs_offset, const char *path,
                        int PreserveEnvVar, int lowercase);
static int dos_would_allow(char *fpath, const char *op, int equal);
static void RemoveRedirection(int drive, cds_t cds);
static int get_dos_xattr(const char *fname);
static int set_dos_xattr(const char *fname, int attr);

static int drives_initialized = FALSE;

#define MAX_OPENED_FILES 256
static struct file_fd open_files[MAX_OPENED_FILES];
static int num_drives = 0;

lol_t lol = 0;
sda_t sda;
static uint16_t lol_segment, lol_offset;

int lol_dpbfarptr_off, lol_cdsfarptr_off, lol_last_drive_off, lol_nuldev_off,
    lol_njoined_off;

int cds_current_path_off, cds_flags_off, cds_DPB_pointer_off,
    cds_cur_cluster_off, cds_rootlen_off, cds_record_size;

far_t get_nuldev(void)
{
    return MK_FARt(lol_segment, lol_offset + lol_nuldev_off);
}

#define cds_current_path(cds)     ((char *)&cds[cds_current_path_off])
#define cds_flags(cds)        (*(u_short *)&cds[cds_flags_off])
#define cds_DPB_pointer(cds)    (*(far_t *)&cds[cds_DPB_pointer_off])
#define cds_cur_cluster(cds)  (*(u_short *)&cds[cds_cur_cluster_off])
#define cds_rootlen(cds)      (*(u_short *)&cds[cds_rootlen_off])

#define CDS_FLAG_NOTNET 0x0080
#define CDS_FLAG_SUBST  0x1000
#define CDS_FLAG_JOIN   0x2000
#define CDS_FLAG_READY  0x4000
#define CDS_FLAG_REMOTE 0x8000

int sft_handle_cnt_off, sft_open_mode_off,sft_attribute_byte_off,
    sft_device_info_off, sft_dev_drive_ptr_off, sft_fd_off,
    sft_start_cluster_off, sft_time_off, sft_date_off, sft_size_off,
    sft_position_off, sft_rel_cluster_off, sft_abs_cluster_off,
    sft_directory_sector_off, sft_directory_entry_off, sft_name_off,
    sft_ext_off, sft_record_size;

int sda_current_dta_off, sda_cur_psp_off, sda_cur_drive_off, sda_filename1_off,
    sda_filename2_off, sda_sdb_off, sda_cds_off, sda_search_attribute_off,
    sda_open_mode_off, sda_rename_source_off, sda_user_stack_off,
    sda_ext_act_off, sda_ext_attr_off, sda_ext_mode_off;

int sdb_drive_letter_off, sdb_template_name_off, sdb_template_ext_off,
    sdb_attribute_off, sdb_dir_entry_off, sdb_p_cluster_off, sdb_file_name_off,
    sdb_file_ext_off, sdb_file_attr_off, sdb_file_time_off, sdb_file_date_off,
    sdb_file_st_cluster_off, sdb_file_size_off;

static int lock_set(int fd, struct flock *fl)
{
  int ret;
  fl->l_pid = 0; // needed for OFD locks
  fl->l_whence = SEEK_SET;
  ret = fcntl(fd, F_OFD_SETLK, fl);
  if (ret) {
    int err = errno;
    if (err != EAGAIN)
      error("OFD_SETLK failed, %s\n", strerror(err));
  }
  return ret;
}

static void lock_get(int fd, struct flock *fl)
{
  int ret;
  fl->l_pid = 0; // needed for OFD locks
  fl->l_whence = SEEK_SET;
  ret = fcntl(fd, F_OFD_GETLK, fl);
  if (ret) {
    error("OFD_GETLK failed, %s\n", strerror(errno));
    /* pretend nothing is locked */
    fl->l_type = F_UNLCK;
  }
}

static int downgrade_dir_lock(int dir_fd, int fd, off_t start)
{
    struct flock fl;
    int err;

    fl.l_type = F_RDLCK;
    fl.l_start = start;
    fl.l_len = 1;
    /* set read lock and remove exclusive lock */
    err = lock_set(dir_fd, &fl);
    /* read lock should never fail (we put no write locks) */
    if (err)
        error("MFS: read lock failed\n");
    err |= flock(dir_fd, LOCK_UN);
    return err;
}

static int do_open_dir(const char *dname)
{
    int err;
    int dir_fd = open(dname, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd == -1) {
        error("MFS: failed to open %s: %s\n", dname, strerror(errno));
        return -1;
    }
    /* lock directory (OFD) to avoid races */
    err = flock(dir_fd, LOCK_EX);
    if (err) {
        close(dir_fd);
        return -1;
    }
    return dir_fd;
}

static struct file_fd *do_claim_fd(const char *name)
{
    int i;
    struct file_fd *ret = NULL;

    for (i = 0; i < MAX_OPENED_FILES; i++) {
        struct file_fd *f = &open_files[i];
        if (!f->name) {
            f->name = strdup(name);
            f->idx = i;
            ret = f;
            break;
        }
    }
    if (!ret) {
        error("MFS: too many open files\n");
        leavedos(1);
        return NULL;
    }
    ret->dir_fd = -1;
    ret->seek = 0;
    ret->size = 0;
    return ret;
}

static struct file_fd *do_find_fd(const char *name)
{
    int i;
    struct file_fd *ret = NULL;

    for (i = 0; i < MAX_OPENED_FILES; i++) {
        struct file_fd *f = &open_files[i];
        if (f->name && strcmp(name, f->name) == 0) {
            ret = f;
            break;
        }
    }
    return ret;
}

static int file_is_opened(int dir_fd, const char *name, int *r_err)
{
    struct stat st;
    struct flock fl;
    int err;

    err = fstatat(dir_fd, name, &st, 0);
    if (err) {
        *r_err = FILE_NOT_FOUND;
        return -1;
    }
    fl.l_type = F_WRLCK;
    fl.l_start = st.st_ino;
    fl.l_len = 1;
    lock_get(dir_fd, &fl);
    return (fl.l_type == F_UNLCK ? 0 : 1);
}

static int do_mfs_creat(struct file_fd *f, const char *dname,
        const char *fname, mode_t mode)
{
    int fd, dir_fd, err;

    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    fd = openat(dir_fd, fname, O_RDWR | O_CLOEXEC | O_CREAT, mode);
    if (fd == -1)
        goto err;
    err = fstat(fd, &f->st);
    if (err)
        goto err2;
    err = downgrade_dir_lock(dir_fd, fd, f->st.st_ino);
    if (err)
        goto err2;

    f->fd = fd;
    f->dir_fd = dir_fd;
    f->write_allowed = 1;
    f->is_writable = 1;
    return 0;

err2:
    unlinkat(dir_fd, fname, 0);
    close(fd);
err:
    close(dir_fd);
    return -1;
}

static struct file_fd *mfs_creat(char *name, mode_t mode)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    int err;
    if (!slash)
        return NULL;
    f = do_claim_fd(name);
    if (!f)
        return NULL;
    fname = slash + 1;
    *slash = '\0';
    err = do_mfs_creat(f, name, fname, mode);
    *slash = '/';
    if (err) {
        free(f->name);
        f->name = NULL;
        return NULL;
    }
    return f;
}

enum { compat_lk_off = 0x100000000LL, noncompat_lk_off, denyR_lk_off,
    denyW_lk_off, R_lk_off, W_lk_off };

static int open_compat(int fd)
{
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_start = noncompat_lk_off;
    fl.l_len = 1;
    lock_get(fd, &fl);
    if (fl.l_type != F_UNLCK)
        return -1;
    fl.l_type = F_RDLCK;
    fl.l_start = compat_lk_off;
    fl.l_len = 1;
    return lock_set(fd, &fl);
}

static int open_share(int fd, int open_mode, int share_mode)
{
    int err;
    struct flock fl;
    int denyR = (share_mode == DENY_READ || share_mode == DENY_ALL);
    int denyW = (share_mode == DENY_WRITE || share_mode == DENY_ALL);
    /* inhibit compat mode */
    fl.l_type = F_WRLCK;
    fl.l_start = compat_lk_off;
    fl.l_len = 1;
    lock_get(fd, &fl);
    if (fl.l_type != F_UNLCK)
        return -1;
    if (open_mode != O_WRONLY) {
        /* read mode allowed? */
        fl.l_type = F_WRLCK;
        fl.l_start = denyR_lk_off;
        fl.l_len = 1;
        lock_get(fd, &fl);
        if (fl.l_type != F_UNLCK)
            return -1;
    }
    if (open_mode == O_WRONLY || open_mode == O_RDWR) {
        /* write mode allowed? */
        fl.l_type = F_WRLCK;
        fl.l_start = denyW_lk_off;
        fl.l_len = 1;
        lock_get(fd, &fl);
        if (fl.l_type != F_UNLCK)
            return -1;
    }
    if (denyR) {
        /* denyR allowed? */
        fl.l_type = F_WRLCK;
        fl.l_start = R_lk_off;
        fl.l_len = 1;
        lock_get(fd, &fl);
        if (fl.l_type != F_UNLCK)
            return -1;
    }
    if (denyW) {
        /* denyW allowed? */
        fl.l_type = F_WRLCK;
        fl.l_start = W_lk_off;
        fl.l_len = 1;
        lock_get(fd, &fl);
        if (fl.l_type != F_UNLCK)
            return -1;
    }

    /* all checks passed, claim our locks */
    fl.l_type = F_RDLCK;
    fl.l_start = noncompat_lk_off;
    fl.l_len = 1;
    err = lock_set(fd, &fl);
    if (err)
        return err;
    if (open_mode != O_WRONLY) {
        fl.l_type = F_RDLCK;
        fl.l_start = R_lk_off;
        fl.l_len = 1;
        err = lock_set(fd, &fl);
        if (err)
            return err;
    }
    if (open_mode == O_WRONLY || open_mode == O_RDWR) {
        fl.l_type = F_RDLCK;
        fl.l_start = W_lk_off;
        fl.l_len = 1;
        err = lock_set(fd, &fl);
        if (err)
            return err;
    }
    if (denyR) {
        fl.l_type = F_RDLCK;
        fl.l_start = denyR_lk_off;
        fl.l_len = 1;
        err = lock_set(fd, &fl);
        if (err)
            return err;
    }
    if (denyW) {
        fl.l_type = F_RDLCK;
        fl.l_start = denyW_lk_off;
        fl.l_len = 1;
        err = lock_set(fd, &fl);
        if (err)
            return err;
    }
    return 0;
}

static int file_is_ro(const char *fname, mode_t mode)
{
    int attr = get_dos_xattr(fname);
    if (attr == -1)
        return (!(mode & S_IWUSR));
    return !!(attr & READ_ONLY_FILE);
}

static int do_mfs_open(struct file_fd *f, const char *dname,
        const char *fname, int flags, struct stat *st, int share_mode,
        int *r_err)
{
    int fd, dir_fd, err;
    int is_writable = 1;
    int write_requested = (flags == O_WRONLY || flags == O_RDWR);

    *r_err = ACCESS_DENIED;
    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    /* try O_RDWR first, as needed by an OFD locks */
    fd = openat(dir_fd, fname, O_RDWR | O_CLOEXEC);
    if (fd == -1 && errno == EACCES && !write_requested) {
        /* retry with O_RDONLY, but OFD locks won't work */
        is_writable = 0;
        fd = openat(dir_fd, fname, O_RDONLY | O_CLOEXEC);
    }
    if (fd == -1)
        goto err;
    if (!share_mode)
        err = open_compat(fd);
    else
        err = open_share(fd, flags, share_mode);
    if (err) {
        *r_err = SHARING_VIOLATION;
        goto err2;
    }
    err = downgrade_dir_lock(dir_fd, fd, st->st_ino);
    if (err)
        goto err2;

    f->st = *st;
    f->fd = fd;
    f->dir_fd = dir_fd;
    assert(is_writable >= write_requested);
    f->write_allowed = write_requested;
    f->is_writable = is_writable;
    return 0;

err2:
    close(fd);
err:
    close(dir_fd);
    return -1;
}

static struct file_fd *mfs_open(char *name, int flags, struct stat *st,
        int share_mode, int *r_err)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    int err;
    if (!slash)
        return NULL;
    f = do_claim_fd(name);
    if (!f)
        return NULL;
    fname = slash + 1;
    *slash = '\0';
    err = do_mfs_open(f, name, fname, flags, st, share_mode, r_err);
    *slash = '/';
    if (err) {
        free(f->name);
        f->name = NULL;
        return NULL;
    }
    return f;
}

static int do_mfs_unlink(const char *dname, const char *fname)
{
    int dir_fd, err, rc;

    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    rc = file_is_opened(dir_fd, fname, &err);
    switch (rc) {
        case -1:
            close(dir_fd);
            return err;
        case 0:
            break;
        case 1:
            close(dir_fd);
            return ACCESS_DENIED;
    }
    err = unlinkat(dir_fd, fname, 0);
    close(dir_fd);
    if (err)
        return FILE_NOT_FOUND;
    return 0;
}

static int mfs_unlink(char *name)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    int err;
    if (!slash)
        return FILE_NOT_FOUND;
    f = do_find_fd(name);
    if (f)
        return ACCESS_DENIED;
    fname = slash + 1;
    *slash = '\0';
    err = do_mfs_unlink(name, fname);
    *slash = '/';
    return err;
}

static int do_mfs_setattr(const char *dname, const char *fname,
        const char *fullname, int attr)
{
    int dir_fd, err, rc;

    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    rc = file_is_opened(dir_fd, fname, &err);
    switch (rc) {
        case -1:
            close(dir_fd);
            return err;
        case 0:
            break;
        case 1:
            close(dir_fd);
            return ACCESS_DENIED;
    }
    err = set_dos_xattr(fullname, attr);
    close(dir_fd);
    if (err)
        return FILE_NOT_FOUND;
    return 0;
}

static int mfs_setattr(char *name, int attr)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    char *fullname;
    int err;
    if (!slash)
        return FILE_NOT_FOUND;
    f = do_find_fd(name);
    if (f)
        return ACCESS_DENIED;
    fname = slash + 1;
    fullname = strdup(name);
    *slash = '\0';
    err = do_mfs_setattr(name, fname, fullname, attr);
    *slash = '/';
    free(fullname);
    return err;
}

static int do_mfs_rename(const char *dname, const char *fname,
        const char *fname2)
{
    int dir_fd, err, rc;

    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    rc = file_is_opened(dir_fd, fname, &err);
    switch (rc) {
        case -1:
            close(dir_fd);
            return err;
        case 0:
            break;
        case 1:
            close(dir_fd);
            return ACCESS_DENIED;
    }
#ifdef HAVE_RENAMEAT2
    err = renameat2(dir_fd, fname, -1, fname2, RENAME_NOREPLACE);
    if (err && errno == EINVAL) {
      error("MFS: RENAME_NOREPLACE unsupported here\n");
      err = access(fname2, F_OK);
      if (err)  // err means no file
        err = renameat(dir_fd, fname, -1, fname2);
    }
#else
    err = access(fname2, F_OK);
    if (err)  // err means no file
        err = renameat(dir_fd, fname, -1, fname2);
#endif
    close(dir_fd);
    if (err)
        return (errno == EEXIST ? ACCESS_DENIED : FILE_NOT_FOUND);
    return 0;
}

static int mfs_rename(char *name, char *name2)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    int err;
    if (!slash)
        return FILE_NOT_FOUND;
    f = do_find_fd(name);
    if (f)
        return ACCESS_DENIED;
    fname = slash + 1;
    *slash = '\0';
    err = do_mfs_rename(name, fname, name2);
    *slash = '/';
    return err;
}

static void mfs_close(struct file_fd *f)
{
    close(f->fd);
    close(f->dir_fd);
    free(f->name);
    f->name = NULL;
}

static char *cds_flags_to_str(uint16_t flags) {
  static char s[5 * 8 + 1]; // 5 names * maxstrlen + terminator;
  int len = 0;

  s[0] = '\0';

  if (flags & CDS_FLAG_NOTNET)
    len += sprintf(s + len, "NOTNET,");
  if (flags & CDS_FLAG_SUBST)
    len += sprintf(s + len, "SUBST,");
  if (flags & CDS_FLAG_JOIN)
    len += sprintf(s + len, "JOIN,");
  if (flags & CDS_FLAG_READY)
    len += sprintf(s + len, "READY,");
  if (flags & CDS_FLAG_REMOTE)
    len += sprintf(s + len, "REMOTE,");

  if (len)
    s[len - 1] = '\0'; // trim the trailing comma

  return s;
}

static const char *redirector_op_to_str(uint8_t op) {
  const char *s[] = {
    "Installation check",
    "Remove directory",
    "Remove directory (2)",
    "Make directory",
    "Make directory (2)",
    "Set current directory",
    "Close file",
    "Commit file",
    "Read file",
    "Write file",
    "Lock file region",
    "Unlock file region",
    "Get disk space",
    "Set file attributes (IFS)",
    "Set file attributes",
    "Get file attributes",
    "Get file attributes (IFS)",
    "Rename file",
    "Rename file (IFS)",
    "Delete file",
    "Delete file (IFS)",
    "Open file (IFS)",
    "Open existing file",
    "Create truncate file",
    "Create truncate file (no CDS)",
    "Find file (no CDS)",
    "Find next (no CDS, IFS)",
    "Find file",
    "Find next",
    "Close all",
    "Control redirect",
    "Printer setup",
    "Flush all disk buffers",
    "Seek from EOF",
    "Process terminated",
    "Qualify filename",
    "Turn off printer",
    "Printer mode",
    "Printer echo on/off",
    "Unused",
    "Unused (IFS)",
    "Unused (IFS)",
    "Close all files for process (IFS)",
    "Generic IOCTL (IFS)",
    "Update CB???",
    "Extended attributes (IFS)",
    "Multipurpose open",
  };
  static_assert(sizeof(s) == sizeof(s[0]) * (MULTIPURPOSE_OPEN + 1),
                "Size of redirector operation name array suspicious");

  if (op == LONG_SEEK)
    return "Long seek (extension)";
  else if (op == GET_LARGE_FILE_INFO)
    return "Get large file info (extension)";
  else if (op > MULTIPURPOSE_OPEN)
    return "Operation out of range";
  return s[op];
}

/* here are the functions used to interface dosemu with the mach
   dos redirector code */

static int cds_drive(cds_t cds)
{
  int drive = toupperDOS(cds_current_path(cds)[0]) - 'A';

  if (drive >= 0 && drive < MAX_DRIVE)
    return drive;
  else
    return -1;
}

/*****************************
 * GetCurrentDriveInDOS
 * on entry:
 *   drv: For the result (0=A, 1=B etc)
 * on exit:
 *   Returns 1 on success, 0 on fail
 * notes:
 *   This function can be used only whilst InDOS, in essence that means from
 *   redirector int2f/11 function. It relies on being able to run int2f/12
 *   functions which need to use the DOS stack. Outside of DOS use int21/19
 *   which is much easier in any case
 *****************************/
static int GetCurrentDriveInDOS(uint8_t *drv)
{
  struct vm86_regs saved_regs = REGS;
  char *buf, *dst;
  uint8_t dd;
  int ret = 0;

  if (!(buf = lowmem_heap_alloc(2 + 128)))
    return 0;

  /* Ask DOS to canonicalize '\' which should give us 'X:\' back */
  memcpy(buf, "\\", 2);
  _DS = DOSEMU_LMHEAP_SEG;	// DS:SI -> file name to be fully qualified
  _SI = DOSEMU_LMHEAP_OFFS_OF(buf);
  dst = buf + 2;
  memset(dst, 0, 128);
  _ES = _DS;			// ES:DI -> canonical file name result
  _DI = _SI + 2;

  _AX = 0x1221;
  do_int_call_back(0x2f);

  REGS = saved_regs;

  Debug0((dbg_fd, "GetCurrentDriveInDOS() '\\' -> '%s'\n", dst));

  // Sanity checks
  if (!dst[0] || dst[1] != ':')
    goto done;
  dd = toupper(dst[0]) - 'A';
  if (dd >= 26)
    goto done;

  *drv = dd;
  ret = 1;

done:
  lowmem_heap_free(buf);
  return ret;
}

/*****************************
 * GetCDSInDOS
 * on entry:
 *   dosdrive: Dos Drive number (0=A, 1=B etc)
 *   cds: For the result
 * on exit:
 *   Returns 1 on success, 0 on fail
 * notes:
 *   This function can be used only whilst InDOS, in essence that means from
 *   redirector int2f/11 function. It relies on being able to run int2f/12
 *   functions which need to use the DOS stack.
 *****************************/
static int GetCDSInDOS(uint8_t dosdrive, cds_t *cds)
{
  unsigned int ssp, sp;
  int ret = 1;
  struct vm86_regs saved_regs = REGS;

  /* Ask DOS for the CDS */
  ssp = SEGOFF2LINEAR(_SS, 0);
  sp = _SP;
  pushw(ssp, sp, (uint16_t)dosdrive);
  _SP -= 2;
  _AX = 0x1217;
  do_int_call_back(0x2f);
  _SP += 2;

  if (isset_CF()) // drive greater than lastdrive
    ret = 0;
  else
    *cds = MK_FP32(_DS, _SI);

  REGS = saved_regs;
  return ret;
}

/* Try and work out if the current command is for any of my drives */
static int
select_drive(struct vm86_regs *state, int *drive)
{
  int dd;
  int fn = LOW(state->eax);

  Debug0((dbg_fd, "selecting drive\n"));

  switch (fn) {
  case INSTALLATION_CHECK:	/* 0x0 */
  case CLOSE_ALL:		/* 0x1d */
  case CONTROL_REDIRECT:	/* 0x1e */
  case FLUSH_ALL_DISK_BUFFERS:	/* 0x20 */
  case PROCESS_TERMINATED:	/* 0x22 */
  case PRINTER_MODE:		/* 0x25 */
    *drive = -1;
    return DRV_NOT_ASSOCIATED;	// no drive is associated with these functions

  /* src in DS:SI */
  case QUALIFY_FILENAME:	/* 0x23 */
    {
      char *name = (char *)Addr(state, ds, esi);

      Debug0((dbg_fd, "select_drive() DS:SI = '%s'\n", name));
      if (name[1] == ':') {
        dd = toupperDOS(name[0]) - 'A';
      } else if (strlen(name) == 4 && isdigit(name[3])) {
        dd = PRINTER_BASE_DRIVE + toupperDOS(name[3]) - '0' - 1;
      } else {
        dd = sda_cur_drive(sda);
      }
    }
    break;

  /* Filename & CDS in SDA */
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
    /* we don't need CDS to figure out the drive */
    /* fall through */

  /* FIlename in SDA */
  case OPEN_EXISTING_FILE:	/* 0x16 */
  case MULTIPURPOSE_OPEN:	/* 0x2e */
  case FIND_FIRST_NO_CDS:	/* 0x19 */
  case CREATE_TRUNCATE_NO_CDS:	/* 0x18 */
    {
      char *fn1 = sda_filename1(sda);

      Debug0((dbg_fd, "select_drive() sda_filename1 = '%s'\n", fn1));

      if (fn1[0] && fn1[1] == ':')
        dd = toupperDOS(fn1[0]) - 'A';
      else if (strncasecmp(fn1, LINUX_PRN_RESOURCE, strlen(LINUX_PRN_RESOURCE)) == 0)
        dd = PRINTER_BASE_DRIVE + toupperDOS(fn1[sizeof(LINUX_PRN_RESOURCE)]) - '0' - 1;
      else
        dd = MAX_DRIVES;		// this is treated as invalid
    }
    break;

  /* CDS in DPB of passed SFT */
  case CLOSE_FILE:		/* 0x6 */
  case COMMIT_FILE:		/* 0x7 */
  case READ_FILE:		/* 0x8 */
  case WRITE_FILE:		/* 0x9 */
  case LOCK_UNLOCK_FILE_REGION:	/* 0xa */
  case UNLOCK_FILE_REGION_OLD:	/* 0xb */
  case SEEK_FROM_EOF:		/* 0x21 */
  case LONG_SEEK:		/* 0x42 */
  case GET_LARGE_FILE_INFO:	/* 0xa6 */
    {
      sft_t sft = (u_char *) Addr(state, es, edi);

      dd = SFT_DRIVE(sft);
    }
    break;

  /* CDS in ES:DI */
  case GET_DISK_SPACE:		/* 0xc */
    {
      cds_t esdi_cds = (cds_t) Addr(state, es, edi);

      dd = cds_drive(esdi_cds);
    }
    break;


  case FIND_NEXT:		/* 0x1c */
    {
      /* check the drive letter in the findfirst block which is in the DTA */
      unsigned dta = sda_current_dta(sda);

      dd = (READ_BYTE(dta) & ~0x80);
    }
    break;

  /* The rest are unknown */
  default:
    Debug0((dbg_fd, "select_drive() unhandled case %x\n", fn));
    return DRV_NOT_FOUND;
  }

  if (dd < 0 || dd >= MAX_DRIVES || !drives[dd].root) {
    Debug0((dbg_fd, "no drive selected fn=%x\n", fn));
    if (fn == PRINTER_SETUP) {
      SETWORD(&state->ebx, WORD(state->ebx) - redirected_drives);
      Debug0((dbg_fd, "Passing %d to PRINTER SETUP anyway\n",
	      (int) WORD(state->ebx)));
    }
    return DRV_NOT_FOUND;
  }

  Debug0((dbg_fd, "selected drive %d: %s\n", dd, drives[dd].root));
  *drive = dd;
  return DRV_FOUND;
}

#ifdef __linux__
static int file_on_fat(const char *name)
{
  struct statfs buf;
  return statfs(name, &buf) == 0 && buf.f_type == MSDOS_SUPER_MAGIC;
}

static int fd_on_fat(int fd)
{
  struct statfs buf;
  return fstatfs(fd, &buf) == 0 && buf.f_type == MSDOS_SUPER_MAGIC;
}
#endif

#define XATTR_DOSATTR_NAME "user.DOSATTRIB"
#define XATTR_ATTRIBS_MASK (READ_ONLY_FILE | HIDDEN_FILE | SYSTEM_FILE | \
  ARCHIVE_NEEDED)

static int do_extr_xattr(const char *xbuf, ssize_t size)
{
  if (size == -1 && errno == ENOTSUP) {
    error("MFS: failed to get xattrs, unsupported!\n");
    return -1;
  }
  if (size <= 2 || strncmp(xbuf, "0x", 2) != 0)
    return -1;
  return strtol(xbuf + 2, NULL, 16) & XATTR_ATTRIBS_MASK;
}

static int get_dos_xattr(const char *fname)
{
  char xbuf[16];
  ssize_t size = getxattr(fname, XATTR_DOSATTR_NAME, xbuf, sizeof(xbuf) - 1);
  /* some dosemus forgot \0 so we fix it up here */
  if (size > 0 && xbuf[size - 1] != '\0') {
    error("MFS: fixup xattr for %s\n", fname);
    xbuf[size++] = '\0';
    setxattr(fname, XATTR_DOSATTR_NAME, xbuf, size, XATTR_REPLACE);
  }
  return do_extr_xattr(xbuf, size);
}

static int get_dos_xattr_fd(int fd)
{
  char xbuf[16];
  ssize_t size = fgetxattr(fd, XATTR_DOSATTR_NAME, xbuf, sizeof(xbuf) - 1);
  /* some dosemus forgot \0 so we fix it up here */
  if (size > 0 && xbuf[size - 1] != '\0') {
    error("MFS: fixup xattr\n");
    xbuf[size++] = '\0';
    fsetxattr(fd, XATTR_DOSATTR_NAME, xbuf, size, XATTR_REPLACE);
  }
  return do_extr_xattr(xbuf, size);
}

static int xattr_str(char *xbuf, int xsize, int attr)
{
  int ret = snprintf(xbuf, xsize, "0x%x", attr & XATTR_ATTRIBS_MASK);
  assert(ret > 0);
  return (ret + 1);  // include '\0'
}

static int xattr_err(int err)
{
  if (err) {
    error("MFS: failed to set xattrs: %s\n", strerror(errno));
    error("@Try to set $_attrs_support=(off)\n");
    leavedos(5);
  }
  return err;
}

static int set_dos_xattr(const char *fname, int attr)
{
  char xbuf[16];
  return xattr_err(setxattr(fname, XATTR_DOSATTR_NAME, xbuf,
      xattr_str(xbuf, sizeof(xbuf), attr), 0));
}

static int set_dos_xattr_fd(int fd, int attr)
{
  char xbuf[16];
  return xattr_err(fsetxattr(fd, XATTR_DOSATTR_NAME, xbuf,
      xattr_str(xbuf, sizeof(xbuf), attr), 0));
}

static int get_attr_simple(int mode)
{
  int attr = 0;
  if (!(mode & S_IWUSR))
    attr |= READ_ONLY_FILE;
  if (S_ISDIR(mode))
    attr |= DIRECTORY;
  return attr;
}

static int handle_xattr(int attr, int mode)
{
  if (attr == -1)
    return get_attr_simple(mode);
  if (S_ISDIR(mode))
    attr |= DIRECTORY;
  return attr;
}

int get_dos_attr(const char *fname, int mode)
{
  int attr;

#ifdef __linux__
  if (fname && file_on_fat(fname) && (S_ISREG(mode) || S_ISDIR(mode))) {
    int fd = open(fname, O_RDONLY);
    if (fd != -1) {
      int res = ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &attr);
      close(fd);
      if (res == 0)
	return attr;
    }
  }
#endif

  if (!config.attrs)
    return get_attr_simple(mode);

  attr = get_dos_xattr(fname);
  return handle_xattr(attr, mode);
}

int get_dos_attr_fd(int fd, int mode)
{
#ifdef __linux__
  int attr;
  if (fd_on_fat(fd) && (S_ISREG(mode) || S_ISDIR(mode)) &&
      ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &attr) == 0)
    return attr;
#endif

  if (!config.attrs)
    return get_attr_simple(mode);

  attr = get_dos_xattr_fd(fd);
  return handle_xattr(attr, mode);
}

static int get_unix_attr(int attr)
{
  int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  /* Do not make directories read-only as this has completely different
     semantics in DOS (mostly ignore) than in Unix.
     Also do not reflect the archive bit as clearing the x bit as that
     can cause inaccessible directories */
  if (attr & DIRECTORY)
    attr = DIRECTORY;
  if (attr & DIRECTORY)
    mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
  return mode;
}

#ifdef __linux__
int set_fat_attr(int fd, int attr)
{
  return ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &attr);
}
#endif

int set_dos_attr(char *fpath, int attr)
{
#ifdef __linux__
  int fd = -1;
#endif
  int res;

#ifdef __linux__
  if (fpath && file_on_fat(fpath))
    fd = open(fpath, O_RDONLY);
  if (fd != -1) {
    res = set_fat_attr(fd, attr);
    if (res && errno != ENOTTY) {
      int oldattr = 1;
      ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &oldattr);
      if (dos_would_allow(fpath, "FAT_IOCTL_SET_ATTRIBUTES", attr == oldattr))
	res = 0;
    }
    close(fd);
    return res;
  }
#endif

  if (!config.attrs)
    return 0;
  return mfs_setattr(fpath, attr);
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

static int dos_get_disk_space(const char *cwd, unsigned int *free, unsigned int *total,
		       unsigned int *spc, unsigned int *bps)
{
  struct statfs fsbuf;

  if (statfs(cwd, &fsbuf) >= 0) {
    unsigned _bps = 512, _spc = 1, _total, _free;
    /* return unit = 512-byte blocks @ 1 spc, std for floppy */
    _free = fsbuf.f_bsize * fsbuf.f_bavail / (_bps * _spc);
    _total = fsbuf.f_bsize * fsbuf.f_blocks / (_bps * _spc);

    while (_spc < 64 && _total > 65535) {
      _spc *= 2;
      _free /= 2;
      _total /= 2;
    }

    *bps = _bps;
    *spc = _spc;
    *total = _total;
    *free = _free;
    return (1);
  }
  else
    return (0);
}

/*
 * At present only the int21/7303 function is implemented so that we can
 * provide the caller with > 2GB free * space values.
 */
int mfs_fat32(void)
{
  char *src = MK_FP32(_DS, _DX);
  unsigned int dest = SEGOFF2LINEAR(_ES, _DI);
  int carry = isset_CF();
  unsigned int spc, bps, free, tot;
  int dd;

  NOCARRY;

  if (!mfs_enabled)
    goto donthandle;

  if (_AX != 0x7303)
    goto donthandle;

  d_printf("Get disk space (FAT32) '%s'\n", src);

  if (get_drive_from_path(src, &dd)) {
    if (!drives[dd].root) {
      d_printf("Error - Drive is not ours\n");
      goto donthandle;
    }
  } else if (src[0] == '\\' && src[1] == '\\') {
    d_printf("Error - UNCs unsupported\n");
    goto donthandle;
  } else {
    d_printf("Error - Invalid drive specification\n");
    goto donthandle;
  }

  if (!dos_get_disk_space(drives[dd].root, &free, &tot, &spc, &bps))
    goto donthandle;

  WRITE_DWORD(dest, 0x24);
  WRITE_DWORD(dest + 0x4, spc);
  WRITE_DWORD(dest + 0x8, bps);
  WRITE_DWORD(dest + 0xc, free);
  WRITE_DWORD(dest + 0x10, tot);
  WRITE_DWORD(dest + 0x14, free * spc);
  WRITE_DWORD(dest + 0x18, tot * spc);
  WRITE_DWORD(dest + 0x1c, free);
  WRITE_DWORD(dest + 0x20, tot);
  return 1;

donthandle:
  if (carry)
    CARRY;
  return 0;
}

static void init_one_drive(int dd)
{
  if (drives[dd].root)
    free(drives[dd].root);
  drives[dd].root = NULL;
  drives[dd].root_len = 0;
  drives[dd].options = 0;
  drives[dd].user_param = 0;
//  drives[dd].curpath[0] = '\0';
  drives[dd].saved_cds_flags = 0;
}

static void
init_all_drives(void)
{
  int dd;

  Debug0((dbg_fd, "Inside initialization\n"));
  drives_initialized = TRUE;
  for (dd = 0; dd < MAX_DRIVES; dd++)
    init_one_drive(dd);
}

void mfs_reset(void)
{
  int process_mask;

  emufs_loaded = FALSE;
  mfs_enabled = FALSE;
  init_all_drives();
  process_mask = umask(0);
  /* we need all group perms */
  process_mask &= ~S_IRWXG;
  umask(process_mask);
}

int mfs_define_drive(const char *path)
{
  int len;

  assert(num_def_drives < MAX_DRIVE);
  len = strlen(path);
  assert(len > 0);
  if (path[len - 1] == '/') {
    def_drives[num_def_drives] = strdup(path);
  } else {
    char *new_path = malloc(len + 2);
    memcpy(new_path, path, len + 1);
    new_path[len] = '/';
    new_path[len + 1] = '\0';
    def_drives[num_def_drives] = new_path;
  }
  return num_def_drives +++ 1;
}

static void init_drive(int dd, char *path, uint16_t user, uint16_t options)
{
  drives[dd].root = strdup(path);
  drives[dd].root_len = strlen(path);
  if (num_drives <= dd)
    num_drives = dd + 1;
  drives[dd].user_param = user;
  drives[dd].options = options;
#if 0
  drives[dd].curpath[0] = 'A' + dd;
  drives[dd].curpath[1] = ':';
  drives[dd].curpath[2] = '\\';
  drives[dd].curpath[3] = '\0';
#endif
  Debug0((dbg_fd, "initialised drive %d as %s with access of %s\n", dd, drives[dd].root,
	  read_only(drives[dd]) ? "READ_ONLY" : "READ_WRITE"));
  if (cdrom(drives[dd]) && cdrom(drives[dd]) <= 4)
    register_cdrom(dd, cdrom(drives[dd]));
}

/***************************
 * mfs_redirector - perform redirector emulation for int 2f, ah=11
 * on entry - nothing
 * on exit - returns non-zero if call was handled
 * 	returns 0 if call was not handled, and should be passed on.
 * notes:
 ***************************/
int mfs_redirector(struct vm86_regs *regs, char *stk, int revect)
{
  int ret;

#ifdef __linux__
  vfat_ioctl = VFAT_IOCTL_READDIR_SHORT;
#endif
  ret = dos_fs_redirect(regs, stk);
#ifdef __linux__
  vfat_ioctl = VFAT_IOCTL_READDIR_BOTH;
#endif

  switch (ret) {
  case FALSE:
    Debug0((dbg_fd, "dos_fs_redirect failed\n"));
    regs->eflags |= CF;
    return 1;
  case TRUE:
    Debug0((dbg_fd, "Finished dos_fs_redirect\n"));
    regs->eflags &= ~CF;
    return 1;
  case UNCHANGED:
    return 1;
  case REDIRECT:
    if (!revect) {
      Debug0((dbg_fd, "dos_fs_redirect unhandled, failing\n"));
      regs->eflags |= CF;
      SETWORD(&regs->eax, FORMAT_INVALID);
    }
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
mfs_helper(struct vm86_regs *regs)
{
  int result;

  result = dos_fs_dev(regs);
  return (result);
}

/* include a few necessary functions from dos_disk.c in the mach
   code as well. input: 8.3 filename, output name + ext
   validity of filename was already checked by name_convert.

   NOTE: intentionally DOES NOT null terminate name and ext
*/
void extract_filename(const char *filename, char *name, char *ext)
{
  char *dot_pos;
  size_t slen; /* length of before-the-dot part of filename */

  memset(name, ' ', 8);
  memset(ext, ' ', 3);

  if (!strcmp(filename, ".") || !strcmp(filename, "..")) {
    memcpy(name, filename, strlen(filename));
    return;
  }

  dot_pos = strchr(filename, '.');
  if (dot_pos) {
    slen = dot_pos - filename;
  } else {
    slen = strlen(filename);
  }

  memcpy(name, filename, min(8, slen));
  if (dot_pos++) {
    size_t elen = strlen(dot_pos);
    memcpy(ext, dot_pos, min(3, elen));
  }
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
#if 0
  if (dir_list->nr_entries == 0xffff) {
    Debug0((dbg_fd, "DOS cannot handle more than 65535 files in one directory\n"));
    error("DOS cannot handle more than 65535 files in one directory\n");
    /* dos limit -- can't get beyond here */
    return NULL;
  }
#endif
  if (dir_list->nr_entries >= dir_list->size)
    enlarge_dir_list(dir_list, dir_list->size * 2);
  entry = &dir_list->de[dir_list->nr_entries];
  dir_list->nr_entries++;
  entry->long_path = FALSE;
  return entry;
}

/* construct (lowercase) unix name from (uppercase) DOS mname and DOS mext */
static void dos83_to_ufs(char *name, const char *mname, const char *mext)
{
  char filename[8+1+3+1];
  size_t len;

  name[0] = 0;
  len = 8;
  while (len && mname[len - 1] == ' ')
    len--;
  if (!len)
    return;
  memcpy(filename, mname, len);
  if (filename[len - 1] != '.')
    filename[len++] = '.';
  memcpy(filename + len, mext, 3);
  len += 3;
  while (filename[len - 1] == ' ')
    len--;
  while (len && filename[len - 1] == '.')
    len--;
  if (!len)
    return;
  filename[len] = '\0';
  path_to_ufs(name, 0, filename, 0, 1);
}

/* check if name/filename exists as such if it does not contain wildcards */
static int exists(const char *name, const char *filename,
                        struct stat *st, int drive)
{
  char fullname[strlen(name) + 1 + NAME_MAX + 1];
  snprintf(fullname, sizeof(fullname), "%s/%s", name, filename);
  Debug0((dbg_fd, "exists() result = %s\n", fullname));
  return find_file(fullname, st, drives[drive].root_len, NULL);
}

static void fill_entry(struct dir_ent *entry, const char *name, int drive)
{
  char buf[PATH_MAX];
  struct stat sbuf;

  snprintf(buf, sizeof(buf), "%s/%s", name, entry->d_name);

  if (!find_file(buf, &sbuf, drives[drive].root_len, NULL)) {
    Debug0((dbg_fd, "Can't findfile %s\n", buf));
    entry->mode = S_IFREG;
    entry->size = 0;
    entry->time = 0;
    entry->attr = 0;
  } else if (is_dos_device(buf)) {
    entry->mode = S_IFREG;
    entry->size = 0;
    entry->time = time(NULL);
    entry->attr = REGULAR_FILE;
  } else {
    entry->mode = sbuf.st_mode;
    entry->size = sbuf.st_size;
    entry->time = sbuf.st_mtime;
    entry->attr = get_dos_attr(buf, entry->mode);
  }
}

/* converts d_name to DOS 8:3 and compares with the wildcard */
static int convert_compare(const char *d_name, char *fname, char *fext,
				 char *mname, char *mext, int in_root)
{
  char tmpname[NAME_MAX + 1];
  size_t namlen;
  int maybe_mangled;

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
static struct dir_list *get_dir_ff(char *name, char *mname, char *mext,
	int drive)
{
  struct mfs_dir *cur_dir;
  struct mfs_dirent *cur_ent;
  struct dir_list *dir_list;
  struct dir_ent *entry;
  char buf[256];
  char fname[8];
  char fext[3];

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
  if (is_dos_device(mname)) {
    dir_list = make_dir_list(1);
    entry = make_entry(dir_list);

    memcpy(entry->name, mname, 8);
    memset(entry->ext, ' ', 3);
    dos83_to_ufs(entry->d_name, entry->name, entry->ext);
    entry->mode = S_IFREG;
    entry->size = 0;
    entry->time = time(NULL);
    entry->attr = REGULAR_FILE;

    dos_closedir(cur_dir);
    return (dir_list);
  }
  /* for efficiency we don't read everything if there are no wildcards */
  else if (!memchr(mname, '?', 8) && !memchr(mext, '?', 3))
  {
    struct stat sbuf;

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
      entry->attr = get_dos_attr(buf, entry->mode);
    }
    dos_closedir(cur_dir);
    return (dir_list);
  }
  else {
    int is_root = (strlen(name) == drives[drive].root_len);
    while ((cur_ent = dos_readdir(cur_dir))) {
      Debug0((dbg_fd, "get_dir(): `%s' \n", cur_ent->d_name));
      if (!convert_compare(cur_ent->d_name, fname, fext, mname, mext, is_root))
	continue;
      if (dir_list == NULL)
	dir_list = make_dir_list(20);
      entry = make_entry(dir_list);
      strcpy(entry->d_name, cur_ent->d_name);
      memcpy(entry->name, fname, 8);
      memcpy(entry->ext, fext, 3);
    }
  }
  dos_closedir(cur_dir);
  return (dir_list);
}

static struct dir_list *get_dir(char *name, char *mname, char *mext, int drive)
{
  int i;
  struct dir_list *list;
  struct stat st;

  /* find_file() validates (and changes) source path */
  if (!find_file(name, &st, drives[drive].root_len, NULL)) {
    Debug0((dbg_fd, "get_dir(): find_file() returned false for '%s'\n", name));
    return NULL;
  }
  list = get_dir_ff(name, mname, mext, drive);
  if (!list)
    return NULL;
  for (i = 0; i < list->nr_entries; i++) {
    if (signal_pending())
	coopth_yield();
    fill_entry(&list->de[i], name, drive);
  }
  return list;
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
  /* support dir/dev notation */
  bs_pos=strchr(filestring, '/');
  if (bs_pos && is_dos_device(bs_pos + 1))
    filestring = bs_pos + 1;

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

static const char *redver_to_str(int ver) {
  switch (ver) {
    case REDVER_NONE:
      return "None";
    case REDVER_PC30:
      return "PC-DOS v3.0";
    case REDVER_PC31:
      return "PC-DOS v3.1";
    case REDVER_PC40:
      return "PC-DOS v4.0";
    case REDVER_CQ30:
      return "MS-DOS (Compaq) v3.0";
  }
  return "Unknown";
}

static int init_dos_offsets(int ver)
{
  lol_dpbfarptr_off = 0;

  cds_current_path_off = 0x0;
  cds_flags_off = 0x43;
  cds_DPB_pointer_off = 0x45;
  cds_cur_cluster_off = 0x49;
  cds_rootlen_off = 0x4f;

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

  /* These are only meaningful for DOS 4 or greater, so zero by default */
  sda_ext_act_off = 0;
  sda_ext_attr_off = 0;
  sda_ext_mode_off = 0;

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

  switch (ver) {
    case REDVER_PC30:
    case REDVER_PC31:

      cds_record_size = 0x51;

      if (ver == REDVER_PC30) {
        lol_cdsfarptr_off = 0x17;
        lol_last_drive_off = 0x1b;
        lol_nuldev_off = 0x28;
        lol_njoined_off = 0x00;  // Doesn't exist on v3.00

        sft_name_off = 0x21;
        sft_ext_off = 0x29;
        sft_record_size = 0x38;

        sda_current_dta_off = 0x10;

        // NOTE - this value contradicts the 'fixed' phantom.c, but has been
        // proven correct by examining dumps of sda and the resultant psp.
        sda_cur_psp_off = 0x05;

        // NOTE - not defined in phantom.c, examination of dumped sda gives
        // possible candidates as 0x09, 0x33. Subsequent dumps after lredir2
        // of e: to /tmp confirm 0x09 change value to 0x04 and 0x33 is
        // maintained at 0x02.
        sda_cur_drive_off = 0x09;
        sda_filename1_off = 0x35f;
        sda_filename2_off = 0x3df;
        sda_sdb_off = 0x45f;
        sda_search_attribute_off = 0x51e;
        sda_open_mode_off = 0x51f;

        // NOTE - not defined in phantom.c. The sda struct gives possible
        // space 0x520..0x547 and is a far ptr, so somewhere in 0x520..0x544.
        // Examination of dumped sda gives possible candidate at 0x525 but
        // is strange as it's unaligned. Subsequent testing shows that with
        // the offset at 0x525 lredir2 returns valid data for current
        // redirections, other values (0x524, 0x526) do not, so it's likely
        // correct.
        sda_user_stack_off = 0x525;
        sda_cds_off = 0x548;
        sda_rename_source_off = 0x59b;
      } else {
        lol_cdsfarptr_off = 0x16;
        lol_last_drive_off = 0x21;
        lol_nuldev_off = 0x22;
        lol_njoined_off = 0x34;

        sft_name_off = 0x20;
        sft_ext_off = 0x28;
        sft_record_size = 0x35;

        sda_current_dta_off = 0x0c;
        sda_cur_psp_off = 0x10;
        sda_cur_drive_off = 0x16;
        sda_filename1_off = 0x92;
        sda_filename2_off = 0x112;
        sda_sdb_off = 0x192;
        sda_search_attribute_off = 0x23a;
        sda_open_mode_off = 0x23b;
        sda_user_stack_off = 0x250;
        sda_cds_off = 0x26c;
        sda_rename_source_off = 0x2b8;
      }
      break;

    case REDVER_CQ30:
      cds_record_size = 0x51;

      lol_cdsfarptr_off = 0x17;
      lol_last_drive_off = 0x1b;
      lol_nuldev_off = 0x28;
      lol_njoined_off = 0x00;  // Doesn't exist on v3.00

      sft_name_off = 0x21;
      sft_ext_off = 0x29;
      sft_record_size = 0x38;

      /*
       * All of the following determined by looking at dumps of the SDA
       * between vanilla PC-DOS 3.00 and Compaq MS-DOS 3.00
       */
      sda_current_dta_off = 0x16;

      // Note - confirmed by following the value to see a valid PSP
      sda_cur_psp_off = 0x0c;

      sda_cur_drive_off = 0x20;
      sda_filename1_off = 0x187;
      sda_filename2_off = 0x207;
      sda_sdb_off = 0x287;
      sda_search_attribute_off = 0x32e;
      sda_open_mode_off = 0x32f;
      sda_user_stack_off = 0x346;
      sda_cds_off = 0x362;

      // As yet unused, will need proper offset if it is
      sda_rename_source_off = 0x0;
      break;

    case REDVER_PC40:
      cds_record_size = 0x58;

      lol_cdsfarptr_off = 0x16;
      lol_last_drive_off = 0x21;
      lol_nuldev_off = 0x22;
      lol_njoined_off = 0x34;

      sft_name_off = 0x20;
      sft_ext_off = 0x28;
      sft_record_size = 0x3b;

      sda_current_dta_off = 0xc;
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
      break;

    case REDVER_NONE:
      Debug0((dbg_fd, "No valid redirector detected, DOS unsupported\n"));
      return 0;

    default:
      Debug0((dbg_fd, "Invalid redirector version '%02x'\n", ver));
      return 0;
  }

  if (!lol_cdsfarptr(lol).segment && !lol_cdsfarptr(lol).offset) {
    Debug0((dbg_fd, "No valid CDS ptr found in LOL, DOS unsupported\n"));
    return 0;
  }

  Debug0((dbg_fd, "Using '%s' redirector\n", redver_to_str(ver)));
  return 1;
}

struct mfs_dir *dos_opendir(const char *name)
{
  struct mfs_dir *dir;
  int fd = -1;
  DIR *d = NULL;
#ifdef __linux__
  struct __fat_dirent de[2];

  if (file_on_fat(name)) {
    fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd == -1)
      return NULL;
    if (ioctl(fd, vfat_ioctl, de) != -1) {
      lseek(fd, 0, SEEK_SET);
    } else {
      close(fd);
      fd = -1;
    }
  }
#endif
  if (fd == -1) {
    int dfd;
    /* not a VFAT filesystem or other problems */
    d = opendir(name);
    if (d == NULL)
      return NULL;
    dfd = dirfd(d);
    fcntl(dfd, F_SETFD, fcntl(dfd, F_GETFD) | FD_CLOEXEC);
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
#ifdef __linux__
      static struct __fat_dirent de[2];
      int ret;

      ret = (int)RPT_SYSCALL(ioctl(dir->fd, vfat_ioctl, de));
      if (ret == -1 || de[0].d_reclen == 0)
        return NULL;

      dir->de.d_name = de[0].d_name;
      dir->de.d_long_name = de[1].d_name;
      if (dir->de.d_long_name[0] == '\0' ||
	  vfat_ioctl == VFAT_IOCTL_READDIR_SHORT) {
        dir->de.d_long_name = dir->de.d_name;
      }
#else
      return NULL;
#endif
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
SetRedirection(int dd, cds_t cds)
{
  char *cwd;

  /* if it's already done then don't bother */
  if ((cds_flags(cds) & (CDS_FLAG_REMOTE | CDS_FLAG_READY)) ==
      (CDS_FLAG_REMOTE | CDS_FLAG_READY))
    return (1);

  Debug0((dbg_fd, "Calculated DOS Information for %d:\n", dd));
  Debug0((dbg_fd, "  cwd=%20s\n", cds_current_path(cds)));
  Debug0((dbg_fd, "  cds flags = %s\n", cds_flags_to_str(cds_flags(cds))));

  WRITE_P(cds_flags(cds), cds_flags(cds) | (CDS_FLAG_REMOTE | CDS_FLAG_READY | CDS_FLAG_NOTNET));

  cwd = cds_current_path(cds);
  assert(cwd);
  sprintf(cwd, "%c:\\", 'A' + dd);
  WRITE_P(cds_rootlen(cds), strlen(cwd) - 1);
  Debug0((dbg_fd, "cds_current_path=%s\n", cwd));
  return (1);
}

static int
dos_fs_dev(struct vm86_regs *state)
{

  Debug0((dbg_fd, "emufs operation: 0x%04x\n", WORD(state->ebx)));

  NOCARRY;

  switch (LO_BYTE_d(state->ebx)) {
  case DOS_SUBHELPER_MFS_EMUFS_INIT:
    if (emufs_loaded) {
      CARRY;
      break;
    }
    emufs_loaded = TRUE;
    break;

  case DOS_SUBHELPER_MFS_REDIR_INIT: {
    int redver = HI_BYTE_d(state->ebx);
    mfs_enabled = init_dos_offsets(redver);
    Debug0((dbg_fd, "redver=%02d\n", redver));
  }
  /* no break */
  case DOS_SUBHELPER_MFS_REDIR_RESET:
    if (!mfs_enabled) {
      CARRY;
      break;
    }
    lol = SEGOFF2LINEAR(WORD(state->ecx), WORD(state->edx));
    sda = MK_FP32(WORD(state->esi), WORD(state->edi));
    Debug0((dbg_fd, "lol=%#x\n", lol));
    Debug0((dbg_fd, "sda=%p\n", sda));
    /* init some global vars */
    lol_segment = WORD(state->ecx);
    lol_offset = WORD(state->edx);
    break;

  /* Let the caller know the redirector has been initialised and that we
   * have valid CDS info */
  case DOS_SUBHELPER_MFS_REDIR_STATE:
    SETWORD(&state->eax, mfs_enabled);
    break;
  }

  return TRUE;
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
      wchar_t symbol;
      if (lowercase && !inenv)
	ch = tolowerDOS(ch);
      symbol = dos_to_unicode_table[(unsigned char)ch];
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

void build_ufs_path_(char *ufs, const char *path, int drive, int lowercase)
{
  int i;

  Debug0((dbg_fd, "dos_fs: build_ufs_path for DOS path '%s'\n", path));

  strcpy(ufs, drives[drive].root);
  /* Skip over leading <drive>: in the path */
  if (path[1]==':')
    path += 2;

  if (strncasecmp(path, LINUX_PRN_RESOURCE, strlen(LINUX_PRN_RESOURCE)) == 0) {
    sprintf(ufs, "LPT%s", &path[strlen(LINUX_PRN_RESOURCE) + 1]);
    return;
  }

  Debug0((dbg_fd,"dos_gen: ufs '%s', path '%s', l=%d\n", ufs, path,
          drives[drive].root_len));

  path_to_ufs(ufs, drives[drive].root_len, path, 0, lowercase);

  /* remove any double slashes */
  /* FIXME: is this needed?? */
  i = 0;
  while (ufs[i]) {
    if (ufs[i] == '/' && ufs[i+1] == '/')
      memmove(&ufs[i], &ufs[i+1], strlen(&ufs[i]));
    else
      i++;
  }
  Debug0((dbg_fd, "dos_fs: build_ufs_path result is '%s'\n", ufs));
}

static inline void build_ufs_path(char *ufs, const char *path, int drive)
{
  build_ufs_path_(ufs, path, drive, 1);
}

/*
 * scan a directory for a matching filename
 */
static int
scan_dir(const char *path, char *name, int root_len)
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
  if (dosname[0] == '.' && strlen(path) == root_len &&
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
int find_file(char *fpath, struct stat * st, int root_len, int *doserrno)
{
  char *slash1, *slash2;

  Debug0((dbg_fd, "find file %s\n", fpath));

  if (is_dos_device(fpath)) {
    struct stat _st;
    char *s = strrchr(fpath, '/');
    int path_exists = 0;

    /* check if path exists */
    if (s != NULL) {
      *s = '\0';
      path_exists = (!stat(fpath, &_st) && S_ISDIR(_st.st_mode));
      *s = '/';
    }
    memset(st, 0, sizeof(*st));
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
  slash1 = fpath + root_len - 1;

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
      if (doserrno) *doserrno = PATH_NOT_FOUND;
      return (FALSE);
    }
    else {
      char remainder[PATH_MAX];
      *slash1 = 0;
      if (slash2) {
	remainder[0] = '/';
	strlcpy(remainder+1, slash2+1, sizeof(remainder)-1);
      }
      if (!scan_dir(fpath, slash1 + 1, root_len)) {
	*slash1 = '/';
	Debug0((dbg_fd, "find_file(): no match: %s\n", fpath));
	if (slash2) {
	  strcat(slash1+1,remainder);
	  if (doserrno)
	    *doserrno = PATH_NOT_FOUND;
	}
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

static int
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
  char *fpath;
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

static void free_list(struct stack_entry *se, int force)
{
  struct dir_list *list;

  free(se->fpath);
  se->fpath = NULL;

  list = se->hlist;
  if (list == NULL)
    return;

  free(list->de);
  free(list);
  se->hlist = NULL;
}

static inline int hlist_push(struct dir_list *hlist, unsigned psp, const char *fpath)
{
  struct stack_entry *se;
  static unsigned prev_psp = 0;

  Debug0((dbg_fd, "hlist_push: %d hlist=%p PSP=%d path=%s\n", hlists.tos, hlist, psp, fpath));

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
  se->psp = psp;
 exit:
  se->hlist = hlist;
  se->fpath = strdup(fpath);
  return se - hlists.stack;
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

static int GetRedirection(struct vm86_regs *state, int rSize, int subfunc)
{
  u_short index = WORD(state->ebx);
  int dd;
  u_short returnBX;		/* see notes below */
  u_short returnCX;
  u_short returnDX;
  char *resourceName;
  char *deviceName;
  u_short *userStack;
  cds_t tcds;

  /* Set number of redirected drives to 0 prior to getting new count */
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

        deviceName = Addr(state, ds, esi);
        snprintf(deviceName, 16, "%c:", 'A' + dd);
        Debug0((dbg_fd, "device name =%s\n", deviceName));

        resourceName = Addr(state, es, edi);
        if (subfunc != DOS_GET_REDIRECTION_EX6) {
          snprintf(resourceName, rSize, LINUX_RESOURCE "%s", drives[dd].root);
        } else {
          snprintf(resourceName, rSize, "%s", drives[dd].root);
        }
        Debug0((dbg_fd, "resource name =%s\n", resourceName));

        /* have to return BX, and CX on the user return stack */
        /* return a "valid" disk redirection */
        returnBX = REDIR_DISK_TYPE;

        returnCX = drives[dd].user_param;
        Debug0((dbg_fd, "GetRedirection CX=%04x\n", returnCX));

        /* This is a Dosemu specific field, but RBIL states it is usually
           destroyed by a redirector, so we may use it to pass our info */
        returnDX = drives[dd].options;
        /* see if DOS believes drive is redirected */
        if (!GetCDSInDOS(dd, &tcds)) {
          Debug0((dbg_fd, "GetRedirection: can't get CDS\n"));
        } else {
          if ((cds_flags(tcds) & (CDS_FLAG_READY | CDS_FLAG_REMOTE)) != (CDS_FLAG_READY | CDS_FLAG_REMOTE))
            returnBX |= (REDIR_STATUS_DISABLED << 8);
          Debug0((dbg_fd, "GetRedirection: CDS flags are 0x%04x (%s)\n",
                 cds_flags(tcds), cds_flags_to_str(cds_flags(tcds))));
        }
        Debug0((dbg_fd, "GetRedirection DX=%04x\n", returnDX));

        userStack = (u_short *)sda_user_stack(sda);
        userStack[1] = returnBX;
        userStack[2] = returnCX;
        userStack[3] = returnDX;
        /* XXXTRB - should set session number in returnBP if */
        /* we are doing an extended getredirection */
        return TRUE;
      } else {
        /* count down until the index is exhausted */
        index--;
      }
    }
  }
#if 0
  /* if we dont own this index, pass down */
  redirected_drives = WORD(state->ebx) - index;
  SETWORD(&state->ebx, index);
  Debug0((dbg_fd, "GetRedirect passing index of %d, Total redirected=%d\n", index, redirected_drives));
  return REDIRECT;
#else
  SETWORD(&state->eax, NO_MORE_FILES);
  return FALSE;
#endif
}

static int path_list_contains(const char *clist, const char *path)
{
  char *s = NULL;
  char *p, buf[PATH_MAX];
  int found = -1;
  int i = 0;
  int plen = strlen(path);
  char *list = strdup(clist);
  const char *home = getenv("HOME");

  assert(plen && path[plen - 1] == '/');    // must end with slash
  for (p = strtok_r(list, " ", &s); p; p = strtok_r(NULL, " ", &s), i++) {
    int len;
    if (p[0] == '~' && home) {
      strlcpy(buf, home, sizeof(buf));
      if (p[1] == '/')
        strlcat(buf, p + 1, sizeof(buf));
      else if (p[1] != '\0') {
        error("invalid path %s in $_lredir_paths\n", p);
        leavedos(3);
        break;
      }
    } else {
      strlcpy(buf, p, sizeof(buf));
    }
    p = buf;
    len = strlen(p);
    if (!len || p[0] != '/' || (len > 1 && p[len - 1] == '/')) {
      error("invalid path %s in $_lredir_paths\n", p);
      leavedos(3);
      break;
    }
    if (len == 1) {
      found = i;    // single slash means any path
      break;
    }
    if (len >= plen)
      continue;
    if (path[len] == '/' && memcmp(p, path, len) == 0) {
      found = i;
      break;
    }
  }
  free(list);
  return found;
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
static int RedirectDisk(struct vm86_regs *state, int drive,
    const char *resourceName)
{
  char path[PATH_MAX];
  int idx;
  cds_t cds;
  uint16_t user = LO_WORD(state->ecx);
  u_short *userStack = (u_short *)sda_user_stack(sda);
  u_short DX = userStack[3];

  if (!GetCDSInDOS(drive, &cds) || !cds) {
    SETWORD(&state->eax, DISK_DRIVE_INVALID);
    return FALSE;
  }

  /* see if drive is already redirected */
  if (cds_flags(cds) & CDS_FLAG_REMOTE) {
    Debug0((dbg_fd, "duplicate redirection for drive %i\n", drive));
    SETWORD(&state->eax, DUPLICATE_REDIR);
    return FALSE;
  }

  strlcpy(path, resourceName, sizeof(path));
  Debug0((dbg_fd, "next_aval %d path %s opts %d\n", drive, path, DX));
  if (path[0] != '/') {
    error("MFS: invalid path %s\n", path);
    SETWORD(&state->eax, FORMAT_INVALID);
    return FALSE;
  }
  if (path[1])
    strlcat(path, "/", sizeof(path));
  /* see if drive already redirected but not in CDS, which means DISABLED  */
  if (drives[drive].root) {
    int ret;
    if (strcmp(drives[drive].root, path) == 0) {
      SetRedirection(drive, cds);
      ret = TRUE;
    } else {
      error("drive %i already has DISABLED redirection %s\n",
          drive, drives[drive].root);
      SETWORD(&state->eax, ACCESS_DENIED);
      ret = FALSE;
    }
    return ret;
  }
  /* authenticate requested path */
  idx = (DX >> REDIR_DEVICE_IDX_SHIFT) & 0x1f;
  if ((DX & 0xfe00) != REDIR_CLIENT_SIGNATURE || idx > MAX_DRIVE)
    idx = 0;
  if (idx) {
    idx--;
    if (!def_drives[idx] ||
        strncmp(def_drives[idx], path, strlen(def_drives[idx])) != 0) {
      error("redirection of %s (%i) rejected\n", path, idx);
      SETWORD(&state->eax, PATH_NOT_FOUND);
      return FALSE;
    }
  } else {
    /* index not supplied, try to find it */
    idx--;
    if (config.lredir_paths)
      idx = path_list_contains(config.lredir_paths, path);
    if (idx == -1) {
      error("redirection of %s rejected\n", path);
      error("@Add the needed path to $_lredir_paths list to allow\n");
      SETWORD(&state->eax, ACCESS_DENIED);
      return FALSE;
    }
    idx += num_def_drives;
    /* found index, tell it to the user */
    userStack[3] |= idx << REDIR_DEVICE_IDX_SHIFT;
  }
  if (access(path, R_OK | X_OK) != 0) {
    warn("MFS: couldn't find path %s\n", path);
    SETWORD(&state->eax, PATH_NOT_FOUND);
    return FALSE;
  }
  if (idx > 0x1f) {
    error("too many redirections\n");
    SETWORD(&state->eax, ACCESS_DENIED);
    return FALSE;
  }
  init_drive(drive, path, user, DX);
  drives[drive].saved_cds_flags = cds_flags(cds);
  SetRedirection(drive, cds);
  return TRUE;
}

static int EnableDiskRedirections(void)
{
  int dd;
  cds_t cds;
  int ret = FALSE;

  for (dd = 0; dd < num_drives; dd++) {
    if (!drives[dd].root || !GetCDSInDOS(dd, &cds))
      continue;
    ret = TRUE;
    if ((cds_flags(cds) & (CDS_FLAG_REMOTE | CDS_FLAG_READY)) !=
       (CDS_FLAG_REMOTE | CDS_FLAG_READY))
      SetRedirection(dd, cds);
  }
  return ret;
}

static int DisableDiskRedirections(void)
{
  int dd;
  cds_t cds;
  int ret = FALSE;

  for (dd = 0; dd < num_drives; dd++) {
    if (!drives[dd].root || !GetCDSInDOS(dd, &cds))
      continue;
    ret = TRUE;
    if (cds_flags(cds) & CDS_FLAG_REMOTE)
      RemoveRedirection(dd, cds);
  }
  return ret;
}

static int GetRedirModeDisk(void)
{
  int dd;
  cds_t cds;

  for (dd = 0; dd < num_drives; dd++) {
    if (!drives[dd].root)
      continue;
    if (!GetCDSInDOS(dd, &cds))
      return 0;
    if ((cds_flags(cds) & (CDS_FLAG_REMOTE | CDS_FLAG_READY)) !=
       (CDS_FLAG_REMOTE | CDS_FLAG_READY))
      return 0;
  }
  return 1;
}

static int GetRedirectionMode(struct vm86_regs *state)
{
  uint8_t redir_type = LOW(state->ebx);
  switch (redir_type) {
    case REDIR_PRINTER_TYPE:
      SETHIGH(&state->ebx, 1);    // printer always on
      break;
    case REDIR_DISK_TYPE:
      SETHIGH(&state->ebx, GetRedirModeDisk());
      break;
    default:
      SETHIGH(&state->ebx, 0);
      break;
  }
  return TRUE;
}

static int SetRedirectionMode(struct vm86_regs *state)
{
  uint8_t redir_type = LOW(state->ebx);
  uint8_t redir_state = HIGH(state->ebx);

  if (redir_type != REDIR_DISK_TYPE)
    return FALSE;
  if (redir_state)
    return EnableDiskRedirections();
  return DisableDiskRedirections();
}

static int RedirectPrinter(struct vm86_regs *state, const char *resourceName)
{
  uint16_t user = LO_WORD(state->ecx);
  int drive;
  const char *p;
  u_short *userStack = (u_short *)sda_user_stack(sda);
  u_short DX = userStack[3];

  if ((DX & 0xfe00) == REDIR_CLIENT_SIGNATURE && (DX & 0b111)) {
    Debug0((dbg_fd, "Readonly/cdrom printer redirection\n"));
    return FALSE;
  }

  if (strncmp(resourceName, LINUX_PRN_RESOURCE, strlen(LINUX_PRN_RESOURCE)) != 0)
    return FALSE;

  p = resourceName + strlen(LINUX_PRN_RESOURCE);
  if (p[0] != '\\' || !isdigit(p[1]))
    return FALSE;
  p++;

  drive = PRINTER_BASE_DRIVE + toupperDOS(p[1]) - '0' - 1;
  if (drive >= MAX_DRIVES || drives[drive].root)
    return FALSE;
  drives[drive].root = strdup(p);
  drives[drive].root_len = strlen(p);
  drives[drive].user_param = user;
  drives[drive].options = 0;
//  drives[drive].curpath[0] = '\0';

  return TRUE;
}

/*****************************
 * RedirectDevice - redirect a drive to the Linux file system
 * on entry:
 * 	called directly only by the redirector
 * on exit:
 * notes:
 *****************************/
static int DoRedirectDevice(struct vm86_regs *state)
{
  const char *resourceName;
  const char *deviceName;
  uint8_t drive;

  /* first, see if this is our resource to be redirected */
  resourceName = Addr(state, es, edi);
  deviceName = Addr(state, ds, esi);

  Debug0((dbg_fd, "RedirectDevice %s to %s\n", deviceName, resourceName));
  if (LOW(state->ebx) == REDIR_PRINTER_TYPE) {
    return RedirectPrinter(state, resourceName);
  }

  if (strncasecmp(resourceName, LINUX_RESOURCE, strlen(LINUX_RESOURCE)) != 0) {
    /* pass call on to next redirector, if any */
    return REDIRECT;
  }

  /* see what device is to be redirected */
  /* we only support disk redirection right now */
  if (LOW(state->ebx) != REDIR_DISK_TYPE || deviceName[1] != ':') {
    SETWORD(&state->eax, FUNC_NUM_IVALID);
    return FALSE;
  }
  drive = toupperDOS(deviceName[0]) - 'A';

  return RedirectDisk(state, drive, resourceName + strlen(LINUX_RESOURCE));
}

int ResetRedirection(int dsk)
{
  /* Do we own this drive? */
  if(drives[dsk].root == NULL) return 2;
  init_one_drive(dsk);
  unregister_cdrom(dsk);
  return 0;
}

/* See if there is a physical drive behind any redirection */
static int hasPhysical(cds_t cds)
{
  far_t DPBptr = cds_DPB_pointer(cds);

  return (DPBptr.offset != 0 || DPBptr.segment != 0);
}

static void RemoveRedirection(int drive, cds_t cds)
{
  char *path;

  /* reset information in the CDS for this drive */
  cds_flags(cds) = drives[drive].saved_cds_flags;

  path = cds_current_path(cds);
  /* set the current path for the drive */
  path[0] = drive + 'A';
  path[1] = ':';
  path[2] = '\\';
  path[3] = EOS;
  cds_rootlen(cds) = CDS_DEFAULT_ROOT_LEN;
  cds_cur_cluster(cds) = 0;	/* reset us at the root of the drive */
}

/*****************************
 * CancelRedirection - cancel a drive redirection
 * on entry:
 *          can only be called from within the redirector
 * on exit:
 * notes:
 *****************************/
static int
CancelRedirection(struct vm86_regs *state)
{
  char *deviceName;
  uint8_t drive, curdrv;
  cds_t cds;

  /* first, see if this is one of our current redirections */
  deviceName = Addr(state, ds, esi);

  Debug0((dbg_fd, "CancelRedirection on %s\n", deviceName));

  /* we only handle drive redirections, pass it through */
  if (!deviceName[0] || deviceName[1] != ':') {
    return REDIRECT;
  }
  drive = toupperDOS(deviceName[0]) - 'A';
  /* If we don't own this drive, pass it through to next redirector */
  if (!drives[drive].root)
    return REDIRECT;

  /* see if drive is in range of valid drives */
  if (!GetCDSInDOS(drive, &cds)) {
    SETWORD(&state->eax, DISK_DRIVE_INVALID);
    return FALSE;
  }

  if (!(cds_flags(cds) & CDS_FLAG_REMOTE)) {
    SETWORD(&state->eax, DISK_DRIVE_INVALID);
    return FALSE;
  }
  // Don't remove drive from under us unless we revert to FATFS
  if (!GetCurrentDriveInDOS(&curdrv)) {
    SETWORD(&state->eax, GENERAL_FAILURE);
    return FALSE;
  }
  if (drive == curdrv && !hasPhysical(cds)) {
    SETWORD(&state->eax, ATT_REM_CUR_DIR);
    return FALSE;
  }

  RemoveRedirection(drive, cds);
  if (!permanent(drives[drive]))
    ResetRedirection(drive);

  Debug0((dbg_fd, "CancelRedirection on %s completed\n", deviceName));
  return TRUE;
}

static int lock_file_region(int fd, int lck, long long start,
    unsigned long len)
{
  struct flock fl;

#ifdef F_GETLK64	// 64bit locks are promoted automatically (e.g. glibc)
  static_assert(sizeof(struct flock) == sizeof(struct flock64), "incompatible flock64");

  Debug0((dbg_fd, "Large file locking start=%llx, len=%lx\n", start, len));
#else			// 32bit locking only
#error 64bit locking not supported
#endif

  fl.l_type = (lck ? F_WRLCK : F_UNLCK);
  fl.l_start = start;
  fl.l_len = len;
  return lock_set(fd, &fl);
}

static int region_lock_offs(int fd, long long start, unsigned long len)
{
  struct flock fl;

  fl.l_type = F_WRLCK;
  fl.l_start = start;
  fl.l_len = len;
  lock_get(fd, &fl);
  if (fl.l_type == F_UNLCK)
    return len;
  return (fl.l_start - start);
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
  find_file(fpath, &st, drives[drive].root_len, NULL);
  strcat(fpath, buf);
  free(buf);
}

static void open_device(unsigned int devptr, char *fname, sft_t sft)
{
  unsigned char *dev = MK_FP32(FP_SEG16(devptr), FP_OFF16(devptr));
  memcpy(sft_name(sft), fname, 8);
  memset(sft_ext(sft), ' ', 3);
  sft_dev_drive_ptr(sft) = devptr;
  sft_directory_entry(sft) = 0;
  sft_directory_sector(sft) = 0;
  sft_attribute_byte(sft) = 0x40;
  sft_device_info(sft) =
    ((dev[4] | dev[5] << 8) & ~SFT_MASK)
    | SFT_FDEVICE | SFT_FEOF;
  time_to_dos(time(NULL), &sft_date(sft), &sft_time(sft));
  sft_size(sft) = 0;
  sft_position(sft) = 0;
}

/* In any Linux directory it is possible to have uids not equal
   to your own one. In that case Linux denies any chmod or utime,
   but DOS really expects any attribute/time set to succeed. We'll fake it
   with a warning, if the file is writable. */
static int dos_would_allow(char *fpath, const char *op, int equal)
{
  if (errno != EPERM)
    return FALSE;

  /* file not writable? */
  if (access(fpath, W_OK) != 0)
    return FALSE;

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

static int find_again(int firstfind, int drive, char *fpath,
			    struct dir_list *hlist, struct vm86_regs *state, sdb_t sdb)
{
  u_char attr;
  int hlist_index = sdb_p_cluster(sdb);
  struct dir_ent *de;

  attr = sdb_attribute(sdb);

  while (sdb_dir_entry(sdb) < hlist->nr_entries) {
    de = &hlist->de[sdb_dir_entry(sdb)];
    sdb_dir_entry(sdb)++;
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
    SETWORD(&state->eax, FILE_NOT_FOUND);
  else
#endif
    SETWORD(&state->eax, NO_MORE_FILES);
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
    if (strlen(root) <= 8 + 3) {
      strcpy(label, root);
    }
    else {
      strcpy(label, root + strlen(root) - (8 + 3));
    }
    free(root);
    while ((p = strchr(label, '.')))
      *p = ' ';
    while ((p = strchr(label, '/')))
      *p = ' ';
    while ((p = strchr(label, '-')))
      *p = ' ';
    strupperDOS(label);
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
  if (read_only(drives[drive]))
    return ACCESS_DENIED;
  build_ufs_path_(fpath, filename1, drive, !lfn);
  if (find_file(fpath, &st, drives[drive].root_len, NULL) && !is_dos_device(fpath)) {
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
  if (read_only(drives[drive]) || (!lfn && is_long_path(filename1)))
    return ACCESS_DENIED;
  build_ufs_path_(fpath, filename1, drive, !lfn);
  if (find_file(fpath, &st, drives[drive].root_len, NULL) || is_dos_device(fpath)) {
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

static int dos_rename(const char *filename1, const char *fname2, int drive)
{
  struct stat st;
  char fpath[PATH_MAX];
  char buf[PATH_MAX];
  char filename2[PATH_MAX];
  const char *cp;
  char fn[9], fe[4], *p;
  int i, j, fnl, err;

  strlcpy(filename2, fname2, sizeof(filename2));
  Debug0((dbg_fd, "Rename file fn1=%s fn2=%s\n", filename1, filename2));
  if (read_only(drives[drive]))
    return ACCESS_DENIED;
  cp = strrchr(filename1, '/');
  if (!cp)
    cp = filename1;
  else
    cp++;
  extract_filename(cp, fn, fe);
  fn[8] = 0;
  fe[3] = 0;
  fnl = strlen(fn);
  p = strrchr(filename2, '\\');
  if (!p)
    p = filename2;
  else
    p++;
  for (i = 0; p[i] && p[i] != '.' && i < 8; i++) {
    if (p[i] == '?') {
      if (i < fnl) {
        p[i] = fn[i];
      } else {
        memmove(&p[i], &p[i + 1], strlen(p) - i);
        i--;
      }
    }
  }
  if (p[i] && p[i] == '.') {
    for (j = 0, i++; p[i]; i++, j++) {
      if (p[i] == '?') {
        p[i] = fe[j];
      }
    }
  }
  build_ufs_path_(fpath, filename2, drive, 1);
  if (find_file(fpath, &st, drives[drive].root_len, NULL) || is_dos_device(fpath)) {
    Debug0((dbg_fd,"Rename, %s already exists\n", fpath));
    return ACCESS_DENIED;
  }
  find_dir(fpath, drive);

  strlcpy(buf, filename1, sizeof(buf));
  if (!find_file(buf, &st, drives[drive].root_len, NULL) || is_dos_device(buf)) {
    Debug0((dbg_fd, "Rename '%s' error.\n", buf));
    return PATH_NOT_FOUND;
  }

  if ((err = mfs_rename(buf, fpath)) != 0)
    return err;

  Debug0((dbg_fd, "Rename file %s to %s\n", buf, fpath));
  return 0;
}

int dos_rename_lfn(const char *filename1, const char *filename2, int drive)
{
  struct stat st;
  char fpath[PATH_MAX];
  char buf[PATH_MAX];
  int err;

  Debug0((dbg_fd, "Rename file fn1=%s fn2=%s\n", filename1, filename2));
  if (read_only(drives[drive]))
    return ACCESS_DENIED;
  build_ufs_path_(fpath, filename2, drive, 0);
  if (find_file(fpath, &st, drives[drive].root_len, NULL) || is_dos_device(fpath)) {
    Debug0((dbg_fd,"Rename, %s already exists\n", fpath));
    return ACCESS_DENIED;
  }
  find_dir(fpath, drive);

  build_ufs_path_(buf, filename1, drive, 0);
  if (!find_file(buf, &st, drives[drive].root_len, NULL) || is_dos_device(buf)) {
    Debug0((dbg_fd, "Rename '%s' error.\n", buf));
    return PATH_NOT_FOUND;
  }

  if ((err = mfs_rename(buf, fpath)) != 0)
    return err;

  Debug0((dbg_fd, "Rename file %s to %s\n", buf, fpath));
  return 0;
}

static u_short unix_access_mode(struct stat *st, int drive, u_short dos_mode)
{
  u_short unix_mode = 0;
  if (dos_mode == READ_ACC) {
    unix_mode = O_RDONLY;
  }
  else if (dos_mode == WRITE_ACC) {
    unix_mode = O_WRONLY;
  }
  else if (dos_mode == READ_WRITE_ACC) {
    /* for cdrom don't return error but downgrade mode to avoid open() error */
    if (cdrom(drives[drive]) || read_only(drives[drive]))
      unix_mode = O_RDONLY;
    else
      unix_mode = O_RDWR;
  }
  else if (dos_mode == 0x40) { /* what's this mode ?? */
    unix_mode = O_RDWR;
  }
  else {
    Debug0((dbg_fd, "Illegal access_mode 0x%x\n", dos_mode));
    unix_mode = O_RDONLY;
  }
  return unix_mode;
}

static void set_32bit_size_or_position(uint32_t* sftfield, uint64_t fullvalue) {
  if (fullvalue < 0x100000000) {
    *sftfield = fullvalue;
  } else {
    *sftfield = 0xFFFFffff;
  }
}

static void do_update_sft(struct file_fd *f, char *fname, char *fext,
	sft_t sft, int drive, u_char attr, u_short FCBcall, int existing)
{
    memcpy(sft_name(sft), fname, 8);
    memcpy(sft_ext(sft), fext, 3);

    if (!existing) {
      if (FCBcall)
        sft_open_mode(sft) |= 0x00f0;
      else
    #if 0
        sft_open_mode(sft) = 0x3; /* write only */
    #else
        sft_open_mode(sft) = 0x2; /* read/write */
    #endif
    }
    sft_directory_entry(sft) = 0;
    sft_directory_sector(sft) = 0;
    sft_attribute_byte(sft) = attr;
    sft_device_info(sft) = (drive & 0x1f) | 0x0940 | SFT_FSHARED;

    if (f->type == TYPE_DISK) {
      time_to_dos(f->st.st_mtime, &sft_date(sft), &sft_time(sft));
      f->size = f->st.st_size;
      set_32bit_size_or_position(&sft_size(sft), f->size);
    }
    f->seek = 0;
    sft_position(sft) = 0;
    sft_fd(sft) = f->idx;
}

static void update_seek_from_dos(uint32_t seek_from_dos, uint64_t* p_seek) {
  /*
   * This has to be done prior to read/write because DOS may seek
   * without calling the redirector. (Particularly in FreeDOS kernel
   * task.c DosComLoader to rewind to the start of an executable.
   * Its function SftSeek only calls the redirector for SEEK_EOF calls.)
   */
  if (seek_from_dos != -1) {
    *p_seek = (uint64_t)seek_from_dos;
  }
}

static int dos_fs_redirect(struct vm86_regs *state, char *stk)
{
  char *filename1;
  char *filename2;
  unsigned dta;
  off_t s_pos = 0;
  unsigned int devptr;
  u_char attr;
  u_short dos_mode, unix_mode, share_mode;
  u_short FCBcall = 0;
  u_char create_file = 0;
  int drive;
  int cnt;
  int ret = REDIRECT;
  struct file_fd *f;
  sft_t sft;
  sdb_t sdb;
  char *bs_pos;
  char fname[8];
  char fext[3];
  char fpath[PATH_MAX];
  int long_path;
  struct dir_list *hlist;
  int hlist_index;
  int doserrno = FILE_NOT_FOUND;
#if 0
  static char last_find_name[8] = "";
  static char last_find_ext[3] = "";
  static u_char last_find_dir = 0;
  static u_char last_find_drive = 0;
  char *resourceName;
  char *deviceName;
#endif

  dos_mode = 0;
  share_mode = 0;

  if (LOW(state->eax) == INSTALLATION_CHECK) {
    Debug0((dbg_fd, "Installation check\n"));
    SETLOW(&state->eax, 0xFF);
    return TRUE;
  }

  if (!mfs_enabled)
    return REDIRECT;

  sft = LINEAR2UNIX(SEGOFF2LINEAR(SREG(es), LWORD(edi)));

  Debug0((dbg_fd, "Entering dos_fs_redirect, FN=%02X, '%s'\n",
          (int)LOW(state->eax), redirector_op_to_str(LOW(state->eax))));

  if (select_drive(state, &drive) == DRV_NOT_FOUND)
    return REDIRECT;

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
#if 0
    case INSTALLATION_CHECK:	/* 0x00 */
      Debug0((dbg_fd, "Installation check\n"));
      SETLOW(&state->eax, 0xFF);
      return TRUE;
#endif
    case REMOVE_DIRECTORY:   /* 0x01 */
    case REMOVE_DIRECTORY_2: /* 0x02 */
    case MAKE_DIRECTORY:     /* 0x03 */
    case MAKE_DIRECTORY_2:   /* 0x04 */
      ret = (LOW(state->eax) >= MAKE_DIRECTORY ? dos_mkdir : dos_rmdir)(filename1, drive, 0);
      if (ret) {
        SETWORD(&state->eax, ret);
        return FALSE;
      }
      return TRUE;

    case SET_CURRENT_DIRECTORY: { /* 0x05 */
      struct stat st;
      Debug0((dbg_fd, "set directory to: %s\n", filename1));
      if (is_long_path(filename1)) {
        /* Path is too long, so we block access */
        SETWORD(&state->eax, PATH_NOT_FOUND);
        return FALSE;
      }
      build_ufs_path(fpath, filename1, drive);
      Debug0((dbg_fd, "set directory to ufs path: %s\n", fpath));

      /* Try the given path */
      if (!find_file(fpath, &st, drives[drive].root_len, NULL) || is_dos_device(fpath)) {
        SETWORD(&state->eax, PATH_NOT_FOUND);
        return FALSE;
      }
      if (!(st.st_mode & S_IFDIR)) {
        SETWORD(&state->eax, PATH_NOT_FOUND);
        Debug0((dbg_fd, "Set Directory %s not found\n", fpath));
        return FALSE;
      }
      /* strip trailing backslash in the filename */
      {
        size_t len = strlen(filename1);
        if (len > 3 && filename1[len - 1] == '\\') {
          WRITE_BYTEP((unsigned char *)&filename1[len - 1], '\0');
        }
      }
//      snprintf(drives[drive].curpath, sizeof(drives[drive].curpath), "%s", filename1);
      Debug0((dbg_fd, "New CWD is %s\n", filename1));
      return TRUE;
    }

    case CLOSE_FILE: /* 0x06 */
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];
      if (f->name == NULL) {
        Debug0((dbg_fd, "Close file %x fails\n", f->fd));
        return FALSE;
      }
      strlcpy(fpath, f->name, sizeof(fpath));
      filename1 = fpath;
      Debug0((dbg_fd, "Close file %x (%s)\n", f->fd, filename1));

      Debug0((dbg_fd, "Handle cnt %d\n", sft_handle_cnt(sft)));
      sft_handle_cnt(sft)--;
      if (sft_handle_cnt(sft) > 0) {
        Debug0((dbg_fd, "Still more handles\n"));
        return TRUE;
      }
      if (f->type == TYPE_PRINTER) {
        printer_close(f->fd);
        Debug0((dbg_fd, "printer %i closed\n", f->fd));
      } else {
        mfs_close(f);
      }

      Debug0((dbg_fd, "Close file succeeds\n"));

      /* if bit 14 in device_info is set, dos requests to set the file
         date/time on closing. R.Brown states this incorrectly (inverted).
      */

      if (sft_device_info(sft) & 0x4000) {
        struct utimbuf ut;
        u_short dos_date = sft_date(sft), dos_time = sft_time(sft);

        Debug0((dbg_fd, "close: setting file date=%04x time=%04x [<- dos format]\n",
                        dos_date, dos_time));
        ut.actime = ut.modtime = time_to_unix(dos_date, dos_time);

        if (*filename1)
          dos_utime(filename1, &ut);
      } else {
        Debug0((dbg_fd, "close: not setting file date/time\n"));
      }

      return TRUE;

    case READ_FILE: { /* 0x08 */
      int return_val;
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];

      if (f->name == NULL) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }

      update_seek_from_dos(sft_position(sft), &f->seek);
      cnt = WORD(state->ecx);
      if (cnt) {
        int cnt1 = cnt;
        if (f->seek <= 0xFFFFffff && f->seek + cnt <= 0xFFFFffff) {
          cnt1 = region_lock_offs(f->fd, f->seek, cnt);
        }
        assert(cnt1 <= cnt);
#if 1
        if (cnt1 <= 0) {  // allow partial reads even though DOS does not
#else
        if (cnt1 < cnt) {  // partial reads not allowed
#endif
          Debug0((dbg_fd, "error, region already locked\n"));
          SETWORD(&state->eax, ACCESS_DENIED);
          return FALSE;
        }
        cnt = cnt1;
      }
      Debug0((dbg_fd, "Read file fd=%d, dta=%#x, cnt=%d\n", f->fd, dta, cnt));
      Debug0((dbg_fd, "Read file pos = %"PRIu64"\n", f->seek));
      Debug0((dbg_fd, "Handle cnt %d\n", sft_handle_cnt(sft)));
      s_pos = lseek(f->fd, f->seek, SEEK_SET);
      if (s_pos < 0 && errno != ESPIPE) {
        SETWORD(&state->ecx, 0);
        return TRUE;
      }
      Debug0((dbg_fd, "Actual pos %"PRIu64"\n", (uint64_t)s_pos));

      ret = dos_read(f->fd, dta, cnt);

      Debug0((dbg_fd, "Read returned : %d\n", ret));
      if (ret < 0) {
        Debug0((dbg_fd, "ERROR IS: %s\n", strerror(errno)));
        return FALSE;
      } else if (ret < cnt) {
        SETWORD(&state->ecx, ret);
        return_val = TRUE;
      } else {
        SETWORD(&state->ecx, cnt);
        return_val = TRUE;
      }
      f->seek += ret;
      set_32bit_size_or_position(&sft_position(sft), f->seek);
      if (ret + s_pos > sft_size(sft)) {
        /* someone else enlarged the file! refresh. */
        fstat(f->fd, &f->st);
        f->size = f->st.st_size;
        set_32bit_size_or_position(&sft_size(sft), f->size);
      }
//      sft_abs_cluster(sft) = 0x174a; /* XXX a test */
      /*      Debug0((dbg_fd, "File data %02x %02x %02x\n", dta[0], dta[1], dta[2])); */
      Debug0((dbg_fd, "Read file pos (fseek) after = %"PRIu64"\n", f->seek));
      return (return_val);
    }

    case WRITE_FILE: /* 0x09 */
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];
      if (f->name == NULL || read_only(drives[drive]) || !f->write_allowed) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }

      update_seek_from_dos(sft_position(sft), &f->seek);
      cnt = WORD(state->ecx);
      Debug0((dbg_fd, "Write file fd=%d count=%x sft_mode=%x\n", f->fd, cnt, sft_open_mode(sft)));
      if (f->type == TYPE_PRINTER) {
        for (ret = 0; ret < cnt; ret++) {
          if (printer_write(f->fd, READ_BYTE(dta + ret)) != 1)
            break;
        }
        SETWORD(&state->ecx, ret);
        return TRUE;
      }

      if (!cnt) {
        Debug0((dbg_fd, "Applying O_TRUNC at %x\n", (int)s_pos));
        if (ftruncate(f->fd, (off_t)f->seek)) {
          Debug0((dbg_fd, "O_TRUNC failed\n"));
          SETWORD(&state->eax, ACCESS_DENIED);
          return FALSE;
        }
        f->size = f->seek;
        set_32bit_size_or_position(&sft_size(sft), f->size);
        SETWORD(&state->ecx, 0);
      } else {
        int cnt1 = cnt;
        if (f->seek <= 0xFFFFffff && f->seek + cnt <= 0xFFFFffff) {
          cnt1 = region_lock_offs(f->fd, f->seek, cnt);
        }
        assert(cnt1 <= cnt);
#if 1
        if (cnt1 <= 0) {  // allow partial writes even though DOS does not
#else
        if (cnt1 < cnt) {  // partial writes not allowed
#endif
          Debug0((dbg_fd, "error, region already locked\n"));
          SETWORD(&state->eax, ACCESS_DENIED);
          return FALSE;
        }
        cnt = cnt1;

        s_pos = lseek(f->fd, f->seek, SEEK_SET);
        if (s_pos < 0 && errno != ESPIPE) {
          SETWORD(&state->ecx, 0);
          return TRUE;
        }
        Debug0((dbg_fd, "Handle cnt %d\n", sft_handle_cnt(sft)));
        Debug0((dbg_fd, "fsize = %"PRIx64", fseek = %"PRIx64", dta = %#x, cnt = %x\n",
                      f->size, f->seek, dta, (int)cnt));
        ret = dos_write(f->fd, dta, cnt);
        if (ret < 0) {
          Debug0((dbg_fd, "Write Failed : %s\n", strerror(errno)));
          SETWORD(&state->eax, ACCESS_DENIED);
          return FALSE;
        }
        f->seek += ret;
        set_32bit_size_or_position(&sft_position(sft), f->seek);
        if ((ret + s_pos) > f->size) {
          f->size = ret + s_pos;
          set_32bit_size_or_position(&sft_size(sft), f->size);
        }
        Debug0((dbg_fd, "write operation done,ret=%x\n", ret));
        Debug0((dbg_fd, "fseek=%"PRIu64", fsize=%"PRIu64"\n", f->seek, f->size));
        SETWORD(&state->ecx, ret);
      }
      //    sft_abs_cluster(sft) = 0x174a;	/* XXX a test */
      /* update stat for atime/mtime */
      if (fstat(f->fd, &f->st) == 0)
        time_to_dos(f->st.st_mtime, &sft_date(sft), &sft_time(sft));
      return TRUE;

    case GET_DISK_SPACE: { /* 0x0c */
#ifdef USE_DF_AND_AFS_STUFF
      cds_t tcds = Addr(state, es, edi);
      char *name = cds_current_path(tcds);
      unsigned int free, tot, spc, bps;
      int dd;

      Debug0((dbg_fd, "Get Disk Space(INT2F/110C)\n"));

      if (get_drive_from_path(name, &dd)) {

        if (!drives[dd].root) {
          Debug0((dbg_fd, "Drive not ours\n"));
          break;
        }

        if (dos_get_disk_space(drives[dd].root, &free, &tot, &spc, &bps)) {
          u_short *userStack = (u_short *)sda_user_stack(sda);
          if (userStack[0] == 0x7303) { /* called from FAT32 function */
            while (tot > 65535 && spc < 128) {
              spc *= 2;
              free /= 2;
              tot /= 2;
            }
            while (tot > 65535 && bps < 32768) {
              bps *= 2;
              free /= 2;
              tot /= 2;
            }
          }
          /* report no more than 32*1024*64K = 2G, even if some
             DOS version 7 can see more */
          if (tot > 65535)
            tot = 65535;
          if (free > 65535)
            free = 65535;

          /* Ralf Brown says: AH=media ID byte - can we let it at 0 here? */
          SETWORD(&state->eax, spc);
          SETWORD(&state->edx, free);
          SETWORD(&state->ecx, bps);
          SETWORD(&state->ebx, tot);
          Debug0((dbg_fd, "free=%d, tot=%d, bps=%d, spc=%d\n", free, tot, bps, spc));

          return TRUE;
        }
      } else if (name[0] == '\\' && name[1] == '\\') {
        Debug0((dbg_fd, "Drive is UNC (%s)\n", name));
      } else {
        Debug0((dbg_fd, "Drive invalid (%s)\n", name));
      }
#endif /* USE_DF_AND_AFS_STUFF */
      break;
    }

    case SET_FILE_ATTRIBUTES: { /* 0x0e */
      struct stat st;
      u_short att = *(u_short *)stk;

      Debug0((dbg_fd, "Set File Attributes %s 0%o\n", filename1, att));
      if (read_only(drives[drive]) || is_long_path(filename1)) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }

      build_ufs_path(fpath, filename1, drive);
      Debug0((dbg_fd, "Set attr: '%s' --> 0%o\n", fpath, att));
      if (!find_file(fpath, &st, drives[drive].root_len, &doserrno) || is_dos_device(fpath)) {
        SETWORD(&state->eax, doserrno);
        return FALSE;
      }
      /* allow changing attrs only on files, not dirs */
      if (!S_ISREG(st.st_mode))
        return TRUE;
      if (set_dos_attr(fpath, att) != 0) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      return TRUE;
    }

    case GET_FILE_ATTRIBUTES: { /* 0x0f */
      struct stat st;
      Debug0((dbg_fd, "Get File Attributes %s\n", filename1));
      build_ufs_path(fpath, filename1, drive);
      if (!find_file(fpath, &st, drives[drive].root_len, &doserrno) || is_dos_device(fpath)) {
        Debug0((dbg_fd, "Get failed: '%s'\n", fpath));
        SETWORD(&state->eax, doserrno);
        return FALSE;
      }

      attr = get_dos_attr(fpath, st.st_mode);
      if (is_long_path(filename1)) {
        /* turn off directory attr for directories with long path */
        attr &= ~DIRECTORY;
      }

      SETWORD(&state->eax, attr);
      state->ebx = st.st_size >> 16;
      state->edi = MASK16(st.st_size);
      time_to_dos(st.st_mtime, &LO_WORD(state->edx), &LO_WORD(state->ecx));
      return TRUE;
    }

    case RENAME_FILE: { /* 0x11 */
      struct dir_list *dir_list = NULL;
      struct dir_ent *de;
      int i;

      auspr(filename1, fname, fext);
      build_ufs_path(fpath, filename1, drive);
      bs_pos = getbasename(fpath);
      *bs_pos = '\0';
      dir_list = get_dir(fpath, fname, fext, drive);
      if (!dir_list) {
        SETWORD(&state->eax, PATH_NOT_FOUND);
        return FALSE;
      }

      cnt = strlen(fpath);
      de = &dir_list->de[0];
      ret = 0;
      for (i = 0; i < dir_list->nr_entries; i++, de++) {
        strcpy(fpath + cnt, de->d_name);
        ret |= dos_rename(fpath, filename2, drive);
      }
      free(dir_list->de);
      free(dir_list);
      if (ret) {
        SETWORD(&state->eax, ret);
        return FALSE;
      }
      return TRUE;
    }

    case DELETE_FILE: { /* 0x13 */
      struct dir_list *dir_list = NULL;
      int errcode = 0;
      int i;
      struct dir_ent *de;

      Debug0((dbg_fd, "Delete file %s\n", filename1));
      if (read_only(drives[drive])) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      build_ufs_path(fpath, filename1, drive);
      if (is_dos_device(fpath)) {
        SETWORD(&state->eax, FILE_NOT_FOUND);
        return FALSE;
      }

      auspr(filename1, fname, fext);

      bs_pos = getbasename(fpath);
      *bs_pos = '\0';
      dir_list = get_dir(fpath, fname, fext, drive);

      if (dir_list == NULL) {
        struct stat st;
        build_ufs_path(fpath, filename1, drive);
        if (!find_file(fpath, &st, drives[drive].root_len, &doserrno)) {
          SETWORD(&state->eax, doserrno);
          return FALSE;
        }
#if 0
        if (access(fpath, W_OK) == -1) {
          SETWORD(&state->eax, ACCESS_DENIED);
          return FALSE;
        }
#endif
        if ((errcode = mfs_unlink(fpath)) != 0) {
          Debug0((dbg_fd, "Delete failed(%s) %s\n", strerror(errno), fpath));
          SETWORD(&state->eax, errcode);
          return FALSE;
        }
        Debug0((dbg_fd, "Deleted %s\n", fpath));
        return TRUE;
      }

      cnt = strlen(fpath);
      fpath[cnt++] = SLASH;
      de = &dir_list->de[0];
      for (i = 0; i < dir_list->nr_entries; i++, de++) {
        if ((de->mode & S_IFMT) == S_IFREG) {
          struct stat st;
          strcpy(fpath + cnt, de->d_name);
          if (find_file(fpath, &st, drives[drive].root_len, NULL)) {
            errcode = mfs_unlink(fpath);
            if (errcode != 0) {
              Debug0((dbg_fd, "Delete failed(%i) %s\n", errcode, fpath));
            } else {
              Debug0((dbg_fd, "Deleted %s\n", fpath));
            }
          }
        }
        if (errcode != 0) {
          if (errcode == EACCES) {
            SETWORD(&state->eax, ACCESS_DENIED);
          } else {
            SETWORD(&state->eax, FILE_NOT_FOUND);
          }
          free(dir_list->de);
          free(dir_list);
          return FALSE;
        }
      }
      free(dir_list->de);
      free(dir_list);
      return TRUE;
    }

    case OPEN_EXISTING_FILE: /* 0x16 */
      /* according to the appendix in undoc dos 2 the top word on the
         stack holds the open mode.  Other than the definition in the
         appendix, I can find nothing else which supports this statement. */
      dos_mode = *(u_short *)stk;
      /* check for a high bit set indicating an FCB call */
      FCBcall = sft_open_mode(sft) & 0x8000;

      Debug0((dbg_fd, "(mode = 0x%04x)\n", dos_mode));
      Debug0((dbg_fd, "(sft_open_mode = 0x%04x)\n", sft_open_mode(sft)));

      if (FCBcall) {
        sft_open_mode(sft) |= 0x00f0;
      } else {
        /* Keeping sharing modes in sft also, --Maxim Ruchko */
        sft_open_mode(sft) = dos_mode & 0xff;
      }

      /* This method is ALSO in undoc dos.  They have the command
         defined differently in two different places.  The important
         thing is that this doesn't work under dr-dos 6.0

         attr = *(u_short *) Stk_Addr(state, ss, esp, stk_offs) */

      Debug0((dbg_fd, "Open existing file %s\n", filename1));

      if (read_only(drives[drive]) && (dos_mode & 3) != READ_ACC) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      build_ufs_path(fpath, filename1, drive);

do_open_existing:
      share_mode = (dos_mode >> 4) & 7;
      dos_mode &= 0xF;

      auspr(filename1, fname, fext);
      devptr = is_dos_device(fpath);
      if (devptr) {
        open_device(devptr, fname, sft);
        Debug0((dbg_fd, "device open succeeds: '%s'\n", fpath));
        return TRUE;
      }
      if (strncasecmp(filename1, LINUX_PRN_RESOURCE, strlen(LINUX_PRN_RESOURCE)) == 0) {
        int fd;
        bs_pos = filename1 + strlen(LINUX_PRN_RESOURCE);
        if (bs_pos[0] != '\\' || !isdigit(bs_pos[1]))
          return FALSE;
        fd = bs_pos[1] - '0' - 1;
        if (printer_open(fd) != 0)
          return FALSE;
        f = do_claim_fd(fpath);
        f->fd = fd;
        f->dir_fd = -1;
        f->type = TYPE_PRINTER;
        do_update_sft(f, fname, fext, sft, drive, 0, FCBcall, 1);
      } else {
        struct stat st;
        int doserrno = FILE_NOT_FOUND;
        if (!find_file(fpath, &st, drives[drive].root_len, &doserrno)) {
          Debug0((dbg_fd, "open failed: '%s'\n", fpath));
          SETWORD(&state->eax, doserrno);
          return (FALSE);
        }
        if (st.st_mode & S_IFDIR) {
          Debug0((dbg_fd, "S_IFDIR: '%s'\n", fpath));
          SETWORD(&state->eax, FILE_NOT_FOUND);
          return (FALSE);
        }
        if (dos_mode != READ_ACC && file_is_ro(fpath, st.st_mode)) {
          SETWORD(&state->eax, ACCESS_DENIED);
          return (FALSE);
        }
        if (dos_mode == WRITE_ACC && (cdrom(drives[drive]) ||
            read_only(drives[drive]))) {
          SETWORD(&state->eax, ACCESS_DENIED);
          return (FALSE);
        }

        if (!(f = mfs_open(fpath, unix_access_mode(&st, drive, dos_mode),
            &st, share_mode, &doserrno))) {
          Debug0((dbg_fd, "access denied:'%s' (dm=%x um=%x, %x)\n", fpath,
              dos_mode, unix_mode, doserrno));
          SETWORD(&state->eax, doserrno);
          return FALSE;
        }
        f->type = TYPE_DISK;
        do_update_sft(f, fname, fext, sft, drive,
            get_dos_attr(fpath, st.st_mode), FCBcall, 1);
      }

      Debug0((dbg_fd, "open succeeds: '%s' fd = 0x%x\n", fpath, f->fd));
      Debug0((dbg_fd, "Size : %ld\n", (long)f->st.st_size));

      /* If FCB open requested, we need to call int2f 0x120c */
      if (FCBcall) {
        Debug0((dbg_fd, "FCB Open calling int2f 0x120c\n"));
        fake_call_to(FCB_HLP_SEG, FCB_HLP_OFF);
        return UNCHANGED;
      }

      return TRUE;

    case CREATE_TRUNCATE_NO_CDS: /* 0x18 */
    case CREATE_TRUNCATE_FILE:   /* 0x17 */

      FCBcall = sft_open_mode(sft) & 0x8000;
      Debug0((dbg_fd, "FCBcall=0x%x\n", FCBcall));

      /* 01 in high byte = create new, 00 s just create truncate */
      create_file = *(u_char *)(stk + 1);

      attr = *(u_short *)stk;
      Debug0((dbg_fd, "CHECK attr=0x%x, create=0x%x\n", attr, create_file));

      /* make it a byte - we thus ignore the new bit */
      attr &= 0xFF;
      if (attr & DIRECTORY)
        return REDIRECT;

      Debug0((dbg_fd, "Create truncate file %s attr=%x\n", filename1, attr));
      build_ufs_path(fpath, filename1, drive);

      {
        struct stat st;
        int file_exists = find_file(fpath, &st, drives[drive].root_len, &doserrno);
        u_short *userStack = (u_short *)sda_user_stack(sda);

        // int21 0x3c passes offset in DX, 0x6c does it in SI
        int ofs = ((userStack[0] >> 8) == 0x3c) ? 3 : 4;

        if (!file_exists && userStack[7] == BIOSSEG &&
                            userStack[ofs] == LFN_short_name) {
          /* special case: creation of LFN's using saved path
             if original DS:{DX,SI} points to BIOSSEG:LFN_short_name */
          strcpy(fpath, lfn_create_fpath);
        }
      }

do_create_truncate:
      if (read_only(drives[drive])) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      auspr(filename1, fname, fext);
      if (strncasecmp(filename1, LINUX_PRN_RESOURCE, strlen(LINUX_PRN_RESOURCE)) == 0) {
        int fd;
        bs_pos = filename1 + strlen(LINUX_PRN_RESOURCE);
        if (bs_pos[0] != '\\' || !isdigit(bs_pos[1]))
          return FALSE;
        fd = bs_pos[1] - '0' - 1;
        if (printer_open(fd) != 0) {
          error("printer %i open failure!\n", fd);
          return FALSE;
        }
        Debug0((dbg_fd, "printer open succeeds: '%s'\n", filename1));
        strcpy(fpath, filename1);
        fname[0] = 0;
        fext[0] = 0;
        f = do_claim_fd(fpath);
        f->fd = fd;
        f->dir_fd = -1;
        f->type = TYPE_PRINTER;
      } else {
        struct stat st;
        int mode;
        if (find_file(fpath, &st, drives[drive].root_len, NULL)) {
          devptr = is_dos_device(fpath);
          if (devptr) {
            open_device(devptr, fname, sft);
            Debug0((dbg_fd, "device open succeeds: '%s'\n", fpath));
            return TRUE;
          }
          Debug0((dbg_fd, "st.st_mode = 0x%02x, handles=%d\n", st.st_mode, sft_handle_cnt(sft)));
          if (/* !(st.st_mode & S_IFREG) || */ create_file) {
            SETWORD(&state->eax, FILE_ALREADY_EXISTS);
            Debug0((dbg_fd, "File exists '%s'\n", fpath));
            return FALSE;
          }
        }
        mode = get_unix_attr(attr);
        if (!(f = mfs_creat(fpath, mode))) {
          find_dir(fpath, drive);
          Debug0((dbg_fd, "trying '%s'\n", fpath));
          if (!(f = mfs_creat(fpath, mode))) {
            Debug0((dbg_fd, "can't open %s: %s (%d)\n", fpath, strerror(errno), errno));
#if 1
            SETWORD(&state->eax, PATH_NOT_FOUND);
#else
            SETWORD(&state->eax, ACCESS_DENIED);
#endif
            return FALSE;
          }
        }
        f->type = TYPE_DISK;
#ifdef __linux__
	if (file_on_fat(fpath))
          set_fat_attr(f->fd, attr);
        else
#endif
        if (config.attrs && get_attr_simple(f->st.st_mode) != attr)
          set_dos_xattr_fd(f->fd, attr);
        if (ftruncate(f->fd, 0) != 0) {
          Debug0((dbg_fd, "unable to truncate %s: %s (%d)\n", fpath, strerror(errno), errno));
          mfs_close(f);
          SETWORD(&state->eax, ACCESS_DENIED);
          return FALSE;
        }
      }

      do_update_sft(f, fname, fext, sft, drive, attr, FCBcall, 0);
      Debug0((dbg_fd, "create succeeds: '%s' fd = 0x%x\n", fpath, f->fd));
      Debug0((dbg_fd, "fsize = 0x%"PRIx64"\n", f->size));

      /* If FCB open requested, we need to call int2f 0x120c */
      if (FCBcall) {
        Debug0((dbg_fd, "FCB Open calling int2f 0x120c\n"));
        fake_call_to(FCB_HLP_SEG, FCB_HLP_OFF);
        return UNCHANGED;
      }
      return TRUE;

    case FIND_FIRST_NO_CDS: /* 0x19 */
    case FIND_FIRST:        /* 0x1b */
    {
      struct stat st;
      attr = sda_search_attribute(sda);

      Debug0((dbg_fd, "findfirst %s attr=%x\n", filename1, attr));

      /*
       * we examine the hlists.stack.hlist for broken find_firsts/find_nexts. --ms
       */
      hlist_watch_pop(sda_cur_psp(sda));

      /* check if path is long */
      long_path = is_long_path(filename1);

      build_ufs_path(fpath, filename1, drive);
      auspr(filename1, fname, fext);
      memcpy(sdb_template_name(sdb), fname, 8);
      memcpy(sdb_template_ext(sdb), fext, 3);
      sdb_attribute(sdb) = attr;
      sdb_drive_letter(sdb) = 0x80 + drive;
      sdb_p_cluster(sdb) = 0xffff; /* correct value later */

      Debug0((dbg_fd, "Find first %8.8s.%3.3s\n", sdb_template_name(sdb), sdb_template_ext(sdb)));

      if (((attr & (VOLUME_LABEL | DIRECTORY)) == VOLUME_LABEL) &&
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

      if (!find_file(fpath, &st, drives[drive].root_len, NULL)) {
        Debug0((dbg_fd, "get_dir(): find_file() returned false for '%s'\n", fpath));
        SETWORD(&state->eax, PATH_NOT_FOUND);
        return FALSE;
      }
      hlist = get_dir_ff(fpath, sdb_template_name(sdb), sdb_template_ext(sdb), drive);
      if (hlist == NULL) {
        SETWORD(&state->eax, NO_MORE_FILES);
        return FALSE;
      }

      if (long_path) {
        set_long_path_on_dirs(hlist);
      }
      hlist_index = hlist_push(hlist, sda_cur_psp(sda), fpath);
      sdb_dir_entry(sdb) = 0;
      sdb_p_cluster(sdb) = hlist_index;

      hlists.stack[hlist_index].seq = ++hlists.seq; /* new watch stamp --ms */
      hlist_set_watch(sda_cur_psp(sda));

      /*
       * This is the right place to leave this stuff for volume labels. --ms
       */
      if (((attr & (VOLUME_LABEL | DIRECTORY)) == VOLUME_LABEL) &&
          strncmp(sdb_template_name(sdb), "????????", 8) == 0 &&
          strncmp(sdb_template_ext(sdb), "???", 3) == 0) {
        Debug0((dbg_fd, "DONE LABEL!!\n"));

        return TRUE;
      }
      return find_again(1, drive, fpath, hlist, state, sdb);
    }

    case FIND_NEXT: /* 0x1c */
      hlist_index = sdb_p_cluster(sdb);
      hlist = NULL;

      /*
       * if watched find_next in progress, refresh sequence number. --ms
       */
      Debug0((dbg_fd, "Find next hlist_index=%d\n", hlist_index));

      if (hlist_index < hlists.tos) {
        if (hlists.stack[hlist_index].seq > 0)
          hlists.stack[hlist_index].seq = hlists.seq;

        Debug0((dbg_fd, "Find next seq=%d\n", hlists.stack[hlist_index].seq));
        hlist = hlists.stack[hlist_index].hlist;
      }
      if (!hlist) {
        Debug0((dbg_fd, "No more matches\n"));
        SETWORD(&state->eax, NO_MORE_FILES);
        return FALSE;
      }
      strlcpy(fpath, hlists.stack[hlist_index].fpath, sizeof(fpath));

      Debug0((dbg_fd, "Find next %8.8s.%3.3s, pointer->hlist=%p\n",
                      sdb_template_name(sdb), sdb_template_ext(sdb), hlist));
      return find_again(0, drive, fpath, hlist, state, sdb);

    case CLOSE_ALL: /* 0x1d */
      Debug0((dbg_fd, "Close All\n"));
      break;

    case FLUSH_ALL_DISK_BUFFERS: /* 0x20 */
      Debug0((dbg_fd, "Flush Disk Buffers\n"));
      return TRUE;

    case SEEK_FROM_EOF: { /* 0x21 */
      off_t offset = (int32_t)((WORD(state->ecx) << 16) | WORD(state->edx));
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];

      if (f->name == NULL) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      Debug0((dbg_fd, "Seek From EOF fd=%d ofs=%lld\n", f->fd, (long long)offset));
#if 0
      /* no need for an actual seek here. we do it before read/write */
      new_pos = lseek(f->fd, offset, SEEK_END);
#endif
      Debug0((dbg_fd, "Seek returns fd=%d ofs=%lld\n", f->fd, (long long)offset));
      if (fstat(f->fd, &f->st) == 0) {
        off_t new_pos = offset + f->st.st_size;
        /* update file size in case other process changed it */
        f->seek = new_pos;
        set_32bit_size_or_position(&sft_position(sft), f->seek);
        f->size = f->st.st_size;
        set_32bit_size_or_position(&sft_size(sft), f->size);
        SETWORD(&state->edx, sft_position(sft) >> 16);
        SETWORD(&state->eax, WORD(sft_position(sft)));
        return TRUE;
      } else {
        SETWORD(&state->eax, SEEK_ERROR);
        return FALSE;
      }

      break;
    }

    case QUALIFY_FILENAME: { /* 0x23 */
      char *name = (char *)Addr(state, ds, esi);
      char *dst = (char *)Addr(state, es, edi);
      char *slash;
      if (drive > PRINTER_BASE_DRIVE && drive < MAX_DRIVES) {
        sprintf(dst, "%s\\%s", LINUX_PRN_RESOURCE, drives[drive].root);
        Debug0((dbg_fd, "Qualify Filename: %s -> %s\n", name, dst));
        return TRUE;
      }
      if (is_dos_device(name) && (slash = strrchr(name, '\\'))) {
        /* translate to C:/DEV notation.
         * DOS will not open such devices via redirector, and
         * we would avoid the problem of opening NUL device,
         * see https://github.com/dosemu2/dosemu2/issues/1359 */
        int pos = slash - name;
        strcpy(dst, name);
        dst[pos] = '/';
        Debug0((dbg_fd, "Qualify Filename: %s -> %s\n", name, dst));
        return TRUE;
      }
      break;
    }

    case LOCK_UNLOCK_FILE_REGION: { /* 0x0a */
      /* The following code only apply to DOS 4.0 and later */
      /* It manage both LOCK and UNLOCK */
      /* I don't know how to find out from here which DOS is running */
      int is_lock = !(state->ebx & 1);
      int ret;
      struct LOCKREC {
        uint32_t offset, size;
      } *pt = (struct LOCKREC *)Addr(state, ds, edx);
      const unsigned long mask = 0xC0000000;
      off_t start;

      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];

      Debug0((dbg_fd, "lock requested, fd=%d, is_lock=%d, start=%lx, len=%lx\n",
                      f->fd, is_lock, (long)pt->offset, (long)pt->size));

      if (f->name == NULL || !f->is_writable) {
        Debug0((dbg_fd, "file not writable, lock failed.\n"));
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      if (is_lock && config.file_lock_limit &&
          f->lock_cnt >= config.file_lock_limit) {
        Debug0((dbg_fd, "lock limit reached.\n"));
        SETWORD(&state->eax, SHARING_BUF_EXCEEDED);
        return FALSE;
      }
      if (pt->size > 0 && (long long)pt->offset + pt->size > 0xffffffff) {
        Debug0((dbg_fd, "offset+size too large, lock failed.\n"));
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }

      start = pt->offset;
      /* the offset is often strange - remove 2 of its bits if either of
         the top two bits are set. Shift the top ones by two bits. This
         still allows OLE2 apps to operate, but should stop lockd from dieing */
      if ((start & mask) != 0)
        start = (start & ~mask) | ((start & mask) >> 2);

      ret = lock_file_region(f->fd, is_lock, start, pt->size & ~mask);
      if (ret == 0) {
        /* locks can be coalesced so the single unlock resets the counter */
        if (is_lock)
          f->lock_cnt++;
        else
          f->lock_cnt = 0;
        return TRUE; /* no error */
      }
      SETWORD(&state->eax, FILE_LOCK_VIOLATION);
      return FALSE;
    }

    case UNLOCK_FILE_REGION_OLD: /* 0x0b */
      Debug0((dbg_fd, "Unlock file region\n"));
      break;

    case PROCESS_TERMINATED: /* 0x22*/
      Debug0((dbg_fd, "Process terminated PSP=%d\n", state->ds));
      hlist_pop_psp(state->ds);
      if (config.lfn)
        close_dirhandles(state->ds);
      return REDIRECT;

    case CONTROL_REDIRECT: { /* 0x1e */
      u_short subfunc = *(u_short *)stk;

      Debug0((dbg_fd, "Control redirect, subfunction 0x%04x\n", subfunc));
      switch (subfunc) {
        case DOS_GET_REDIRECTION_MODE:
          return GetRedirectionMode(state);
        case DOS_SET_REDIRECTION_MODE:
          return SetRedirectionMode(state);
          /* XXXTRB - need to support redirection index pass-thru */
        case DOS_GET_REDIRECTION:
          return GetRedirection(state, 128, subfunc);
        case DOS_GET_REDIRECTION_EXT: {
          u_short *userStack = (u_short *)sda_user_stack(sda);
          u_short CX = userStack[2], DX = userStack[3];
          int isDosemu = (DX & 0xfe00) == REDIR_CLIENT_SIGNATURE;
          return GetRedirection(state, isDosemu ? CX : 128, subfunc);
        }
        case DOS_GET_REDIRECTION_EX6: {
          u_short *userStack = (u_short *)sda_user_stack(sda);
          u_short CX = userStack[2];
          return GetRedirection(state, CX, subfunc);
        }
        case DOS_REDIRECT_DEVICE:
          return DoRedirectDevice(state);
        case DOS_CANCEL_REDIRECTION:
          return CancelRedirection(state);
        default:
          SETWORD(&state->eax, FUNC_NUM_IVALID);
          return FALSE;
      }
      break;
    }

    case COMMIT_FILE: /* 0x07 */
      Debug0((dbg_fd, "Commit\n"));
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];
      if (f->name == NULL) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      return (dos_flush(f->fd) == 0);

    case MULTIPURPOSE_OPEN: {
      /* Uses DOS 4+ specific fields but is okay as this call is also so */
      int file_exists;
      struct stat st;
      u_short action = sda_ext_act(sda);

      dos_mode = sda_ext_mode(sda) & 0x7f;
      attr = *(u_short *)stk;
      Debug0((dbg_fd, "Multipurpose open file: %s\n", filename1));
      Debug0((dbg_fd, "Mode, action, attr = %x, %x, %x\n", dos_mode, action, attr));

      if (strncasecmp(filename1, LINUX_PRN_RESOURCE, strlen(LINUX_PRN_RESOURCE)) == 0)
        goto do_open_existing;

      if (read_only(drives[drive]) && dos_mode != READ_ACC) {
        SETWORD(&state->eax, ACCESS_DENIED);
        return FALSE;
      }
      build_ufs_path(fpath, filename1, drive);
      file_exists = find_file(fpath, &st, drives[drive].root_len, &doserrno);
      if (file_exists && is_dos_device(fpath))
        goto do_open_existing;

      if (((action & 0x10) == 0) && !file_exists) {
        /* Fail if file does not exist */
        SETWORD(&state->eax, doserrno);
        return FALSE;
      }

      if (((action & 0xf) == 0) && file_exists) {
        /* Fail if file does exist */
        SETWORD(&state->eax, FILE_ALREADY_EXISTS);
        return FALSE;
      }

      if (((action & 0xf) == 1) && file_exists) {
        /* Open if does exist */
        SETWORD(&state->ecx, 0x1);
        goto do_open_existing;
      }

      if (((action & 0xf) == 2) && file_exists) {
        /* Replace if file exists */
        SETWORD(&state->ecx, 0x3);
        goto do_create_truncate;
      }

      if (((action & 0x10) != 0) && !file_exists) {
        u_short *userStack = (u_short *)sda_user_stack(sda);
        if (userStack[7] == BIOSSEG && userStack[4] == LFN_short_name) {
          /* special case: creation of LFN's using saved path
             if original DS:SI points to BIOSSEG:LFN_short_name */
          strcpy(fpath, lfn_create_fpath);
        }
        /* Replace if file exists */
        SETWORD(&state->ecx, 0x2);
        goto do_create_truncate;
      }

      Debug0((dbg_fd, "Multiopen failed: 0x%02x\n", (int)LOW(state->eax)));
      /* Fail if file does exist */
      SETWORD(&state->eax, FILE_NOT_FOUND);
      return FALSE;
    }

    case EXTENDED_ATTRIBUTES: {
      switch (LOW(state->ebx)) {
        case 2: /* get extended attributes */
        case 3: /* get extended attribute properties */
          if (WORD(state->ecx) >= 2) {
            *(short *)(Addr(state, es, edi)) = 0;
            SETWORD(&state->ecx, 0x2);
          }
        case 4: /* set extended attributes */
          return TRUE;
      }
      return FALSE;
    }

    case PRINTER_MODE: {
      Debug0((dbg_fd, "Printer Mode: %02x\n", (int)LOW(state->eax)));
      SETLOW(&state->edx, 1);
      return TRUE;
    }

    case PRINTER_SETUP:
      SETWORD(&state->ebx, WORD(state->ebx) - redirected_drives);
      Debug0((dbg_fd, "Passing %d to PRINTER SETUP CALL\n", (int)WORD(state->ebx)));
      return REDIRECT;

    case LONG_SEEK: {
      uint64_t seek;
      d_printf("MFS: long seek\n");
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES) {
        d_printf("long seek: handle lookup failed (beyond table)\n");
        SETWORD(&state->eax, HANDLE_INVALID);
        return FALSE;
      }
      f = &open_files[cnt];
      if (!f->name) {
        d_printf("long seek: handle lookup failed (not open)\n");
        SETWORD(&state->eax, HANDLE_INVALID);
        return FALSE;
      }
      d_printf("found %s on fd %i\n", f->name, f->fd);
      MEMCPY_2UNIX(&seek, SEGOFF2LINEAR(SREG(ds), LWORD(edx)), sizeof seek);
      d_printf("cl=%02"PRIX8"h seek=%08"PRIX64"h "
		"sftposition=%08"PRIX32"h\n"
		"fseek=%08"PRIX64"h "
		"fsize=%08"PRIX64"h\n",
		(uint8_t)LOW(state->ecx), seek,
		(uint32_t)sft_position(sft), f->seek, f->size);
      switch (LOW(state->ecx)) {
	case DOS_SEEK_SET:
	  f->seek = seek;
	  break;
	case DOS_SEEK_CUR:
	  update_seek_from_dos(sft_position(sft), &f->seek);
	  f->seek = f->seek + seek;
	  break;
	case DOS_SEEK_EOF:
	  if (fstat(f->fd, &f->st) == 0) {
	    /* update file size in case other process changed it */
	    f->size = f->st.st_size;
	    set_32bit_size_or_position(&sft_size(sft), f->size);
	    f->seek = f->size + seek;
	    break;
	  } else {
	    SETWORD(&state->eax, SEEK_ERROR);
	    return FALSE;
	  }
	default:
	  d_printf("invalid seek origin=%02"PRIX8"h\n", (uint8_t)LOW(state->ecx));
	  SETWORD(&state->eax, FUNC_NUM_IVALID);
	  return FALSE;
      }
      d_printf("result seek=%08"PRIX64"h\n", f->seek);
      set_32bit_size_or_position(&sft_position(sft), f->seek);
      MEMCPY_2DOS(SEGOFF2LINEAR(SREG(ds), LWORD(edx)), &f->seek, sizeof(f->seek));
      SETWORD(&state->eax, 0);
      return TRUE;
    }

    case GET_LARGE_FILE_INFO: {
      /* ES:DI - SFT
       * DS:DX -> buffer for file information (see #01784)
       */
      unsigned long long wtime;
      unsigned int buffer = SEGOFF2LINEAR(_DS, _DX);

      d_printf("MFS: get large file info\n");
      cnt = sft_fd(sft);
      if (cnt >= MAX_OPENED_FILES)
          return FALSE;
      f = &open_files[cnt];
      if (!f->name) {
        d_printf("LFN: handle lookup failed\n");
        SETWORD(&state->eax, HANDLE_INVALID);
        return FALSE;
      }
      d_printf("found %s on fd %i\n", f->name, f->fd);
      /* update stat for atime/mtime */
      if (fstat(f->fd, &f->st)) {
        SETWORD(&state->eax, HANDLE_INVALID);
        return FALSE;
	}
      WRITE_DWORD(buffer, get_dos_attr_fd(f->fd, f->st.st_mode));
#define unix_to_win_time(ut) \
( \
  ((unsigned long long)ut + (369 * 365 + 89)*24*60*60ULL) * 10000000 \
)
      wtime = unix_to_win_time(f->st.st_ctime);
      WRITE_DWORD(buffer + 4, wtime);
      WRITE_DWORD(buffer + 8, wtime >> 32);
      wtime = unix_to_win_time(f->st.st_atime);
      WRITE_DWORD(buffer + 0xc, wtime);
      WRITE_DWORD(buffer + 0x10, wtime >> 32);
      wtime = unix_to_win_time(f->st.st_mtime);
      WRITE_DWORD(buffer + 0x14, wtime);
      WRITE_DWORD(buffer + 0x18, wtime >> 32);
      WRITE_DWORD(buffer + 0x1c, f->st.st_dev); /*volume serial number*/
      WRITE_DWORD(buffer + 0x20, (unsigned long long)f->st.st_size >> 32);
      WRITE_DWORD(buffer + 0x24, f->st.st_size);
      WRITE_DWORD(buffer + 0x28, f->st.st_nlink);
      /* fileid*/
      WRITE_DWORD(buffer + 0x2c, (unsigned long long)f->st.st_ino >> 32);
      WRITE_DWORD(buffer + 0x30, f->st.st_ino);
      SETWORD(&state->eax, 0);
      return TRUE;
    }

    default:
      Debug0((dbg_fd, "Default for undocumented function: %02x\n", (int)LOW(state->eax)));
      return REDIRECT;
  }

  return ret;
}
