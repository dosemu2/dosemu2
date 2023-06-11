/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: open share modes support.
 *
 * Author: @stsp
 *
 */
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/file.h>
#include "emu.h"
#include "dosemu_debug.h"
#include "dos2linux.h"
#include "mfs.h"
#include "xattr.h"
#include "share.h"

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

static void downgrade_dir_lock(int dir_fd, off_t start)
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
        error("MFS: read lock failed, %s\n", strerror(errno));
    flock(dir_fd, LOCK_UN);
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
    if (err)
        error("MFS: exclusive lock failed: %s\n", strerror(errno));
    return dir_fd;
}

struct file_fd *do_claim_fd(const char *name)
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
        /* check for dangling symlink */
        if (fstatat(dir_fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0)
            return 0;
        *r_err = FILE_NOT_FOUND;
        return -1;
    }
    fl.l_type = F_WRLCK;
    fl.l_start = st.st_ino;
    fl.l_len = 1;
    lock_get(dir_fd, &fl);
    return (fl.l_type == F_UNLCK ? 0 : 1);
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

static int do_mfs_open(struct file_fd *f, const char *dname,
        const char *fname, int flags, struct stat *st, int share_mode,
        int *r_err)
{
    int fd, dir_fd, err;
    int is_writable = (flags == O_WRONLY || flags == O_RDWR);
    int is_readable = (flags == O_RDONLY || flags == O_RDWR);
    /* XXX: force in READ mode, as needed by our share emulation w OFD locks */
    int flags2 = (flags == O_WRONLY ? O_RDWR : flags);

    *r_err = ACCESS_DENIED;
    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    fd = openat(dir_fd, fname, flags2 | O_CLOEXEC);
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
    downgrade_dir_lock(dir_fd, st->st_ino);

    f->st = *st;
    f->fd = fd;
    f->dir_fd = dir_fd;
    f->share_mode = share_mode;
    f->psp = sda_cur_psp(sda);
    f->is_writable = is_writable;
    f->read_allowed = is_readable;
    return 0;

err2:
    close(fd);
err:
    close(dir_fd);
    return -1;
}

struct file_fd *mfs_open(char *name, int flags, struct stat *st,
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

static int do_mfs_creat(struct file_fd *f, const char *dname,
        const char *fname, mode_t mode)
{
    int fd, dir_fd, err;

    dir_fd = do_open_dir(dname);
    if (dir_fd == -1)
        return -1;
    fd = openat(dir_fd, fname, O_RDWR | O_CLOEXEC | O_CREAT | O_TRUNC, mode);
    if (fd == -1)
        goto err;
    /* set compat mode */
    err = open_compat(fd);
    if (err)
        goto err2;
    err = fstat(fd, &f->st);
    if (err)
        goto err2;
    downgrade_dir_lock(dir_fd, f->st.st_ino);

    f->fd = fd;
    f->dir_fd = dir_fd;
    f->share_mode = 0;
    f->psp = sda_cur_psp(sda);
    f->is_writable = 1;
    f->read_allowed = 1;
    return 0;

err2:
    unlinkat(dir_fd, fname, 0);
    close(fd);
err:
    close(dir_fd);
    return -1;
}

struct file_fd *mfs_creat(char *name, mode_t mode)
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

static int do_mfs_unlink(const char *dname, const char *fname, int force)
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
            if (!force) {
                close(dir_fd);
                return ACCESS_DENIED;
            }
    }
    err = unlinkat(dir_fd, fname, 0);
    close(dir_fd);
    if (err)
        return FILE_NOT_FOUND;
    return 0;
}

int mfs_unlink(char *name)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    int err;
    if (!slash)
        return FILE_NOT_FOUND;
    f = do_find_fd(name);
    if (f && (f->share_mode || f->psp != sda_cur_psp(sda)))
        return ACCESS_DENIED;
    fname = slash + 1;
    *slash = '\0';
    err = do_mfs_unlink(name, fname, f && !f->share_mode &&
        f->psp == sda_cur_psp(sda));
    *slash = '/';
    return err;
}

static int do_mfs_setattr(const char *dname, const char *fname,
        const char *fullname, int attr, int force)
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
            if (!force) {
                close(dir_fd);
                return ACCESS_DENIED;
            }
    }
    err = set_dos_xattr(fullname, attr);
    close(dir_fd);
    return err;
}

int mfs_setattr(char *name, int attr)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    char *fullname;
    int err;
    if (!slash)
        return FILE_NOT_FOUND;
    f = do_find_fd(name);
    if (f && (f->share_mode || f->psp != sda_cur_psp(sda)))
        return ACCESS_DENIED;
    fname = slash + 1;
    fullname = strdup(name);
    *slash = '\0';
    err = do_mfs_setattr(name, fname, fullname, attr, f && !f->share_mode &&
        f->psp == sda_cur_psp(sda));
    *slash = '/';
    free(fullname);
    return err;
}

static int do_mfs_rename(const char *dname, const char *fname,
        const char *fname2, int force)
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
            if (!force) {
                close(dir_fd);
                return ACCESS_DENIED;
            }
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

int mfs_rename(char *name, char *name2)
{
    char *slash = strrchr(name, '/');
    struct file_fd *f;
    char *fname;
    int err;
    if (!slash)
        return FILE_NOT_FOUND;
    f = do_find_fd(name);
    if (f && (f->share_mode || f->psp != sda_cur_psp(sda)))
        return ACCESS_DENIED;
    fname = slash + 1;
    *slash = '\0';
    err = do_mfs_rename(name, fname, name2, f && !f->share_mode &&
        f->psp == sda_cur_psp(sda));
    *slash = '/';
    return err;
}

void mfs_close(struct file_fd *f)
{
    close(f->fd);
    close(f->dir_fd);
    free(f->name);
    f->name = NULL;
}
