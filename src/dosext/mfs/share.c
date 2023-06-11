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
#include "shlock.h"
#include "share.h"

#define SHLOCK_DIR "dosemu2_sh"
#define EXLOCK_DIR "dosemu2_ex"

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

static char *prepare_shlock_name(const char *fname)
{
    char *p;
    char *nm = strdup(fname);
    while ((p = strchr(nm, '/')))
        *p = '\\';
    return nm;
}

static void *apply_shlock(const char *fname)
{
    char *nm = prepare_shlock_name(fname);
    void *ret = shlock_open(SHLOCK_DIR, nm, 0);
    free(nm);
    return ret;
}

static void *apply_exlock(const char *fname)
{
    char *nm = prepare_shlock_name(fname);
    void *ret = shlock_open(EXLOCK_DIR, nm, 1);
    free(nm);
    return ret;
}

static int is_locked_shlock(const char *name)
{
    char *nm = prepare_shlock_name(name);
    /* try to create exlock in a shlock dir */
    void *exlock = shlock_open(SHLOCK_DIR, nm, 1);
    free(nm);
    if (!exlock)
        return 1;
    /* we are called under another exlock, so no races if we drop the lock
     * or if it failed to be created */
    shlock_close(exlock);
    return 0;
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

static int file_is_opened(const char *name)
{
    int lck = is_locked_shlock(name);
    if (lck)
        return 1;  // locked means already opened
    return access(name, F_OK);
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

static int do_mfs_open(struct file_fd *f, const char *fname,
        int flags, int share_mode, int *r_err)
{
    int fd, err;
    void *shlock;
    void *exlock;
    int is_writable = (flags == O_WRONLY || flags == O_RDWR);
    int is_readable = (flags == O_RDONLY || flags == O_RDWR);
    /* XXX: force in READ mode, as needed by our share emulation w OFD locks */
    int flags2 = (flags == O_WRONLY ? O_RDWR : flags);

    *r_err = ACCESS_DENIED;
    exlock = apply_exlock(fname);
    if (!exlock)
        return -1;
    fd = open(fname, flags2 | O_CLOEXEC);
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
    shlock = apply_shlock(fname);
    if (!shlock) {
        *r_err = SHARING_VIOLATION;
        goto err2;
    }
    shlock_close(exlock);

    f->fd = fd;
    f->shlock = shlock;
    f->share_mode = share_mode;
    f->psp = sda_cur_psp(sda);
    f->is_writable = is_writable;
    f->read_allowed = is_readable;
    return 0;

err2:
    close(fd);
err:
    shlock_close(exlock);
    return -1;
}

struct file_fd *mfs_open(const char *name, int flags,
        int share_mode, int *r_err)
{
    struct file_fd *f;
    int err;

    f = do_claim_fd(name);
    if (!f)
        return NULL;
    err = do_mfs_open(f, name, flags, share_mode, r_err);
    if (err) {
        free(f->name);
        f->name = NULL;
        return NULL;
    }
    return f;
}

static int do_mfs_creat(struct file_fd *f, const char *fname, mode_t mode)
{
    int fd, err;
    void *shlock;
    void *exlock;

    exlock = apply_exlock(fname);
    if (!exlock)
        return -1;
    fd = open(fname, O_RDWR | O_CLOEXEC | O_CREAT | O_TRUNC, mode);
    if (fd == -1)
        goto err;
    /* set compat mode */
    err = open_compat(fd);
    if (err)
        goto err2;
    shlock = apply_shlock(fname);
    if (!shlock)
        goto err2;
    shlock_close(exlock);

    f->fd = fd;
    f->shlock = shlock;
    f->share_mode = 0;
    f->psp = sda_cur_psp(sda);
    f->is_writable = 1;
    f->read_allowed = 1;
    return 0;

err2:
    unlink(fname);
    close(fd);
err:
    shlock_close(exlock);
    return -1;
}

struct file_fd *mfs_creat(const char *name, mode_t mode)
{
    struct file_fd *f;
    int err;

    f = do_claim_fd(name);
    if (!f)
        return NULL;
    err = do_mfs_creat(f, name, mode);
    if (err) {
        free(f->name);
        f->name = NULL;
        return NULL;
    }
    return f;
}

static int do_mfs_unlink(const char *fname, int force)
{
    int rc;
    void *exlock;

    exlock = apply_exlock(fname);
    if (!exlock)
        return -1;
    rc = file_is_opened(fname);
    switch (rc) {
        case -1:
            shlock_close(exlock);
            return FILE_NOT_FOUND;
        case 0:
            break;
        case 1:
            if (!force) {
                shlock_close(exlock);
                return ACCESS_DENIED;
            }
    }
    rc = unlink(fname);
    shlock_close(exlock);
    if (rc)
        return FILE_NOT_FOUND;
    return 0;
}

int mfs_unlink(char *name)
{
    struct file_fd *f;

    f = do_find_fd(name);
    if (f && (f->share_mode || f->psp != sda_cur_psp(sda)))
        return ACCESS_DENIED;
    return do_mfs_unlink(name, f && !f->share_mode &&
        f->psp == sda_cur_psp(sda));
}

static int do_mfs_setattr(const char *fname, int attr, int force)
{
    int rc;
    void *exlock;

    exlock = apply_exlock(fname);
    if (!exlock)
        return -1;
    rc = file_is_opened(fname);
    switch (rc) {
        case -1:
            shlock_close(exlock);
            return FILE_NOT_FOUND;
        case 0:
            break;
        case 1:
            if (!force) {
                shlock_close(exlock);
                return ACCESS_DENIED;
            }
    }
    rc = set_dos_xattr(fname, attr);
    shlock_close(exlock);
    return rc;
}

int mfs_setattr(char *name, int attr)
{
    struct file_fd *f;

    f = do_find_fd(name);
    if (f && (f->share_mode || f->psp != sda_cur_psp(sda)))
        return ACCESS_DENIED;
    return do_mfs_setattr(name, attr, f && !f->share_mode &&
        f->psp == sda_cur_psp(sda));
}

static int do_mfs_rename(const char *fname, const char *fname2, int force)
{
    int rc;
    void *exlock;
    void *exlock2;

    exlock = apply_exlock(fname);
    if (!exlock)
        return -1;
    rc = file_is_opened(fname);
    switch (rc) {
        case -1:
            shlock_close(exlock);
            return FILE_NOT_FOUND;
        case 0:
            break;
        case 1:
            if (!force) {
                shlock_close(exlock);
                return ACCESS_DENIED;
            }
    }

    exlock2 = apply_exlock(fname2);
    if (!exlock2)
        goto err2;
    rc = file_is_opened(fname2);
    if (rc != -1) {
        /* dest file exists, do not overwrite */
        shlock_close(exlock2);
        shlock_close(exlock);
        return ACCESS_DENIED;
    }

    rc = rename(fname, fname2);
    shlock_close(exlock2);
    shlock_close(exlock);
    if (rc) {
        perror("rename()");
        return FILE_NOT_FOUND;
    }
    return 0;

err2:
    shlock_close(exlock);
    return ACCESS_DENIED;
}

int mfs_rename(char *name, char *name2)
{
    struct file_fd *f;

    f = do_find_fd(name);
    if (f && (f->share_mode || f->psp != sda_cur_psp(sda)))
        return ACCESS_DENIED;
    return do_mfs_rename(name, name2, f && !f->share_mode &&
        f->psp == sda_cur_psp(sda));
}

void mfs_close(struct file_fd *f)
{
    close(f->fd);
    shlock_close(f->shlock);
    free(f->name);
    f->name = NULL;
}
