/*
 *  Copyright (C) 2026  @stsp, generated with Jules AI
 *  https://jules.google.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * VFS (Virtual File System) implementation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <linux/msdos_fs.h>
#endif
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <assert.h>
#include "emu.h"
#include "dosemu_debug.h"
#include "fslib/fslib.h"
#include "vfs.h"

/*
 * Default POSIX backend implementation
 */

#ifdef __linux__
/*
 * FAT knows the DOS attributes natively, so prefer them over the xattr
 * dosemu keeps for filesystems that do not.
 */
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

static int posix_file_close(vfs_file_t *file)
{
  int rc = 0;
  if (file) {
    if (file->fd != -1)
      rc = close(file->fd);
    free(file);
  }
  return rc;
}

static ssize_t posix_file_read(vfs_file_t *file, void *buf, size_t count)
{
  if (!file)
    return -1;
  return read(file->fd, buf, count);
}

static ssize_t posix_file_write(vfs_file_t *file, const void *buf, size_t count)
{
  if (!file)
    return -1;
  return write(file->fd, buf, count);
}

static off_t posix_file_lseek(vfs_file_t *file, off_t offset, int whence)
{
  if (!file)
    return -1;
  return lseek(file->fd, offset, whence);
}

static int posix_file_fstat(vfs_file_t *file, struct stat *sb)
{
  if (!file)
    return -1;
  return fstat(file->fd, sb);
}

static int posix_file_ftruncate(vfs_file_t *file, off_t length)
{
  if (!file)
    return -1;
  return ftruncate(file->fd, length);
}

static int posix_file_fsync(vfs_file_t *file)
{
  if (!file)
    return -1;
  return fsync(file->fd);
}

static int posix_file_get_async_fd(vfs_file_t *file, void *handle)
{
  if (file)
    return file->fd;
  return mfs_async_getfd(handle);
}

static int posix_file_flock(vfs_file_t *file, int op)
{
  if (!file)
    return -1;
  return flock(file->fd, op);
}

#if HAVE_DECL_F_OFD_SETLK
static int posix_file_setlk(vfs_file_t *file, struct flock *fl)
{
  if (!file)
    return -1;
  return fcntl(file->fd, F_OFD_SETLK, fl);
}

static int posix_file_getlk(vfs_file_t *file, struct flock *fl)
{
  if (!file)
    return -1;
  return fcntl(file->fd, F_OFD_GETLK, fl);
}
#endif

static int posix_file_get_dos_attr(vfs_file_t *file, int mode)
{
#ifdef __linux__
  int attr;

  if (file && fd_on_fat(file->fd) && (S_ISREG(mode) || S_ISDIR(mode)) &&
      ioctl(file->fd, FAT_IOCTL_GET_ATTRIBUTES, &attr) == 0)
    return attr;
#endif
  errno = ENOSYS;
  return -1;
}

static int posix_file_set_dos_attr(vfs_file_t *file, int attr)
{
#ifdef __linux__
  if (file && fd_on_fat(file->fd))
    return ioctl(file->fd, FAT_IOCTL_SET_ATTRIBUTES, &attr);
#endif
  errno = ENOSYS;
  return -1;
}

static const struct vfs_file_ops posix_file_ops = {
  .close = posix_file_close,
  .read = posix_file_read,
  .write = posix_file_write,
  .lseek = posix_file_lseek,
  .fstat = posix_file_fstat,
  .ftruncate = posix_file_ftruncate,
  .fsync = posix_file_fsync,
  .get_async_fd = posix_file_get_async_fd,
  .flock = posix_file_flock,
#if HAVE_DECL_F_OFD_SETLK
  .setlk = posix_file_setlk,
  .getlk = posix_file_getlk,
#endif
  .get_dos_attr = posix_file_get_dos_attr,
  .set_dos_attr = posix_file_set_dos_attr,
};

vfs_file_t *vfs_file_wrap_posix(int fd)
{
  vfs_file_t *file;
  if (fd == -1)
    return NULL;
  file = malloc(sizeof(*file));
  if (!file) {
    close(fd);
    return NULL;
  }
  file->ops = &posix_file_ops;
  file->fd = fd;
  return file;
}

/*
 * The FAT readdir ioctl hands out the 8.3 and the long name in one go,
 * which is what the redirector wants, so prefer it when we have it.
 * Which of the two ioctls to use depends on the DOS call being served;
 * the flag moved here together with the code that reads it.
 */
#ifdef __linux__
static long vfat_ioctl = VFAT_IOCTL_READDIR_BOTH;
#endif

void vfs_set_short_names(int on)
{
#ifdef __linux__
  vfat_ioctl = on ? VFAT_IOCTL_READDIR_SHORT : VFAT_IOCTL_READDIR_BOTH;
#endif
}

struct posix_dir {
  vfs_dir_t vdir;  // must be first
#ifdef __linux__
  struct __fat_dirent fat_de[2];
#endif
};

static int posix_dir_closedir(vfs_dir_t *dir)
{
  int rc = 0;
  if (dir) {
    if (dir->d)
      rc = closedir(dir->d);
    else if (dir->fd != -1)
      rc = close(dir->fd);
    free(dir);
  }
  return rc;
}

static int posix_dir_readdir(vfs_dir_t *dir, struct vfs_dirent *de)
{
  struct dirent *d;

  if (!dir)
    return -1;
#ifdef __linux__
  if (!dir->d) {
    struct posix_dir *pd = (struct posix_dir *)dir;
    struct __fat_dirent *fd = pd->fat_de;

    if (RPT_SYSCALL(ioctl(dir->fd, vfat_ioctl, fd)) == -1 ||
        fd[0].d_reclen == 0)
      return -1;
    de->d_name = fd[0].d_name;
    de->d_long_name = fd[1].d_name;
    if (de->d_long_name[0] == '\0' || vfat_ioctl == VFAT_IOCTL_READDIR_SHORT)
      de->d_long_name = de->d_name;
    return 0;
  }
#endif
  d = readdir(dir->d);
  if (!d)
    return -1;
  de->d_name = de->d_long_name = d->d_name;
  return 0;
}

static int posix_dir_fstatdir(vfs_dir_t *dir, struct stat *statbuf)
{
  if (!dir || dir->fd == -1)
    return -1;
  return fstat(dir->fd, statbuf);
}

static int posix_dir_fstatat(vfs_dir_t *dir, const char *pathname, struct stat *statbuf, int flags)
{
  if (!dir || dir->fd == -1)
    return -1;
  return fstatat(dir->fd, pathname, statbuf, flags);
}

static int posix_dir_dirfd(vfs_dir_t *dir)
{
  if (!dir)
    return -1;
  if (dir->d)
    return dirfd(dir->d);
  return dir->fd;
}

static const struct vfs_dir_ops posix_dir_ops = {
  .closedir = posix_dir_closedir,
  .readdir = posix_dir_readdir,
  .fstatdir = posix_dir_fstatdir,
  .fstatat = posix_dir_fstatat,
  .dirfd = posix_dir_dirfd,
};

static vfs_dir_t *vfs_dir_wrap_posix(DIR *d, int fd)
{
  struct posix_dir *pd;
  vfs_dir_t *dir;
  if (!d && fd == -1)
    return NULL;
  pd = malloc(sizeof(*pd));
  if (!pd) {
    if (d)
      closedir(d);
    else if (fd != -1)
      close(fd);
    return NULL;
  }
  dir = &pd->vdir;
  dir->ops = &posix_dir_ops;
  dir->d = d;
  dir->fd = fd;
  dir->has_sfn = 0;
  return dir;
}

static vfs_file_t *posix_fs_open(vfs_fs_t *fs, const char *path, int flags)
{
  int fd = mfs_open_file(fs->mfs_idx, path, flags);
  return vfs_file_wrap_posix(fd);
}

static vfs_file_t *posix_fs_creat(vfs_fs_t *fs, const char *path, int flags, mode_t mode)
{
  int fd = mfs_create_file(fs->mfs_idx, path, flags, mode);
  return vfs_file_wrap_posix(fd);
}

static int posix_fs_unlink(vfs_fs_t *fs, const char *path)
{
  return mfs_unlink_file(fs->mfs_idx, path);
}

static int posix_fs_getxattr(vfs_fs_t *fs, const char *path)
{
  return mfs_getxattr_file(fs->mfs_idx, path);
}

static int posix_fs_setxattr(vfs_fs_t *fs, const char *path, int attr)
{
  return mfs_setxattr_file(fs->mfs_idx, path, attr);
}

static int posix_fs_statvfs(vfs_fs_t *fs, const char *path, struct statvfs *sb)
{
  return do_mfs_statvfs(fs->mfs_idx, path, sb);
}

static int posix_fs_mkdir(vfs_fs_t *fs, const char *path, mode_t mode)
{
  return mfs_mkdir(fs->mfs_idx, path, mode);
}

static int posix_fs_rmdir(vfs_fs_t *fs, const char *path)
{
  return mfs_rmdir(fs->mfs_idx, path);
}

static int posix_fs_stat(vfs_fs_t *fs, const char *path, struct stat *sb)
{
  return mfs_stat_file(fs->mfs_idx, path, sb);
}

static int posix_fs_rename(vfs_fs_t *fs, const char *oldpath, const char *newpath)
{
  return mfs_rename_file(fs->mfs_idx, oldpath, newpath);
}

static int posix_fs_access(vfs_fs_t *fs, const char *path, int mode)
{
  return mfs_access(fs->mfs_idx, path, mode);
}

static int posix_fs_utime(vfs_fs_t *fs, const char *fpath, time_t atime, time_t mtime)
{
  return mfs_utime(fs->mfs_idx, fpath, atime, mtime);
}

static void *posix_fs_open_async(vfs_fs_t *fs, const char *path, int flags)
{
  return mfs_open_async(fs->mfs_idx, path, flags);
}

static vfs_dir_t *posix_fs_opendir(vfs_fs_t *fs, const char *path)
{
  int dfd;
  DIR *d;

#ifdef __linux__
  /* on FAT take the ioctl route, which also gives us the 8.3 names */
  if (file_on_fat(path)) {
    struct __fat_dirent de[2];

    dfd = mfs_open_file(fs->mfs_idx, path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd == -1)
      return NULL;
    if (ioctl(dfd, vfat_ioctl, de) != -1) {
      vfs_dir_t *dir;

      lseek(dfd, 0, SEEK_SET);
      dir = vfs_dir_wrap_posix(NULL, dfd);
      if (dir)
        dir->has_sfn = 1;
      return dir;
    }
    close(dfd);
  }
#endif
  dfd = mfs_open_file(fs->mfs_idx, path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dfd == -1)
    return NULL;
  d = fdopendir(dfd);
  if (!d) {
    close(dfd);
    return NULL;
  }
  return vfs_dir_wrap_posix(d, dfd);
}

static int posix_fs_get_dos_attr(vfs_fs_t *fs, const char *path, int mode)
{
#ifdef __linux__
  if (path && file_on_fat(path) && (S_ISREG(mode) || S_ISDIR(mode))) {
    int fd = mfs_open_file(fs->mfs_idx, path, O_RDONLY);

    if (fd != -1) {
      int attr;
      int res = ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &attr);

      close(fd);
      if (res == 0)
        return attr;
    }
  }
#endif
  errno = ENOSYS;
  return -1;
}

static int posix_fs_set_dos_attr(vfs_fs_t *fs, const char *path, int attr)
{
#ifdef __linux__
  if (path && file_on_fat(path)) {
    int fd = mfs_open_file(fs->mfs_idx, path, O_RDONLY);

    if (fd != -1) {
      int res = ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &attr);

      close(fd);
      return res;
    }
  }
#endif
  errno = ENOSYS;
  return -1;
}

static const struct vfs_fs_ops posix_fs_ops = {
  .open = posix_fs_open,
  .creat = posix_fs_creat,
  .unlink = posix_fs_unlink,
  .getxattr = posix_fs_getxattr,
  .setxattr = posix_fs_setxattr,
  .statvfs = posix_fs_statvfs,
  .mkdir = posix_fs_mkdir,
  .rmdir = posix_fs_rmdir,
  .stat = posix_fs_stat,
  .rename = posix_fs_rename,
  .access = posix_fs_access,
  .utime = posix_fs_utime,
  .open_async = posix_fs_open_async,
  .opendir = posix_fs_opendir,
  .get_dos_attr = posix_fs_get_dos_attr,
  .set_dos_attr = posix_fs_set_dos_attr,
};

static vfs_fs_t fs_instances[128];

vfs_fs_t *vfs_get_fs(int mfs_idx)
{
  if (mfs_idx <= 0 || mfs_idx >= 128)
    return NULL;
  fs_instances[mfs_idx].ops = &posix_fs_ops;
  fs_instances[mfs_idx].mfs_idx = mfs_idx;
  return &fs_instances[mfs_idx];
}

vfs_file_t *vfs_open(vfs_fs_t *fs, const char *path, int flags)
{
  if (!fs || !fs->ops || !fs->ops->open)
    return NULL;
  return fs->ops->open(fs, path, flags);
}

vfs_file_t *vfs_creat(vfs_fs_t *fs, const char *path, int flags, mode_t mode)
{
  if (!fs || !fs->ops || !fs->ops->creat)
    return NULL;
  return fs->ops->creat(fs, path, flags, mode);
}

int vfs_unlink(vfs_fs_t *fs, const char *path)
{
  if (!fs || !fs->ops || !fs->ops->unlink)
    return -1;
  return fs->ops->unlink(fs, path);
}

int vfs_getxattr(vfs_fs_t *fs, const char *path)
{
  if (!fs || !fs->ops || !fs->ops->getxattr)
    return -1;
  return fs->ops->getxattr(fs, path);
}

int vfs_setxattr(vfs_fs_t *fs, const char *path, int attr)
{
  if (!fs || !fs->ops || !fs->ops->setxattr)
    return -1;
  return fs->ops->setxattr(fs, path, attr);
}

int vfs_statvfs(vfs_fs_t *fs, const char *path, struct statvfs *sb)
{
  if (!fs || !fs->ops || !fs->ops->statvfs)
    return -1;
  return fs->ops->statvfs(fs, path, sb);
}

int vfs_mkdir(vfs_fs_t *fs, const char *path, mode_t mode)
{
  if (!fs || !fs->ops || !fs->ops->mkdir)
    return -1;
  return fs->ops->mkdir(fs, path, mode);
}

int vfs_rmdir(vfs_fs_t *fs, const char *path)
{
  if (!fs || !fs->ops || !fs->ops->rmdir)
    return -1;
  return fs->ops->rmdir(fs, path);
}

int vfs_stat(vfs_fs_t *fs, const char *path, struct stat *sb)
{
  if (!fs || !fs->ops || !fs->ops->stat)
    return -1;
  return fs->ops->stat(fs, path, sb);
}

int vfs_rename(vfs_fs_t *fs, const char *oldpath, const char *newpath)
{
  if (!fs || !fs->ops || !fs->ops->rename)
    return -1;
  return fs->ops->rename(fs, oldpath, newpath);
}

int vfs_access(vfs_fs_t *fs, const char *path, int mode)
{
  if (!fs || !fs->ops || !fs->ops->access)
    return -1;
  return fs->ops->access(fs, path, mode);
}

int vfs_utime(vfs_fs_t *fs, const char *fpath, time_t atime, time_t mtime)
{
  if (!fs || !fs->ops || !fs->ops->utime)
    return -1;
  return fs->ops->utime(fs, fpath, atime, mtime);
}

void *vfs_open_async(vfs_fs_t *fs, const char *path, int flags)
{
  if (!fs || !fs->ops || !fs->ops->open_async)
    return NULL;
  return fs->ops->open_async(fs, path, flags);
}

vfs_dir_t *vfs_opendir(vfs_fs_t *fs, const char *path)
{
  if (!fs || !fs->ops || !fs->ops->opendir)
    return NULL;
  return fs->ops->opendir(fs, path);
}

int vfs_get_dos_attr(vfs_fs_t *fs, const char *path, int mode)
{
  if (!fs || !fs->ops || !fs->ops->get_dos_attr) {
    errno = ENOSYS;
    return -1;
  }
  return fs->ops->get_dos_attr(fs, path, mode);
}

int vfs_set_dos_attr(vfs_fs_t *fs, const char *path, int attr)
{
  if (!fs || !fs->ops || !fs->ops->set_dos_attr) {
    errno = ENOSYS;
    return -1;
  }
  return fs->ops->set_dos_attr(fs, path, attr);
}

int vfs_fget_dos_attr(vfs_file_t *file, int mode)
{
  if (!file || !file->ops || !file->ops->get_dos_attr) {
    errno = ENOSYS;
    return -1;
  }
  return file->ops->get_dos_attr(file, mode);
}

int vfs_fset_dos_attr(vfs_file_t *file, int attr)
{
  if (!file || !file->ops || !file->ops->set_dos_attr) {
    errno = ENOSYS;
    return -1;
  }
  return file->ops->set_dos_attr(file, attr);
}

int vfs_close(vfs_file_t *file)
{
  if (!file || !file->ops || !file->ops->close)
    return -1;
  return file->ops->close(file);
}

ssize_t vfs_read(vfs_file_t *file, void *buf, size_t count)
{
  if (!file || !file->ops || !file->ops->read)
    return -1;
  return file->ops->read(file, buf, count);
}

ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t count)
{
  if (!file || !file->ops || !file->ops->write)
    return -1;
  return file->ops->write(file, buf, count);
}

off_t vfs_lseek(vfs_file_t *file, off_t offset, int whence)
{
  if (!file || !file->ops || !file->ops->lseek)
    return -1;
  return file->ops->lseek(file, offset, whence);
}

int vfs_fstat(vfs_file_t *file, struct stat *sb)
{
  if (!file || !file->ops || !file->ops->fstat)
    return -1;
  return file->ops->fstat(file, sb);
}

int vfs_ftruncate(vfs_file_t *file, off_t length)
{
  if (!file || !file->ops || !file->ops->ftruncate)
    return -1;
  return file->ops->ftruncate(file, length);
}

int vfs_fsync(vfs_file_t *file)
{
  if (!file || !file->ops || !file->ops->fsync)
    return -1;
  return file->ops->fsync(file);
}

int vfs_get_async_fd(vfs_file_t *file, void *handle)
{
  if (!file || !file->ops || !file->ops->get_async_fd)
    return -1;
  return file->ops->get_async_fd(file, handle);
}

int vfs_flock(vfs_file_t *file, int op)
{
  if (!file || !file->ops || !file->ops->flock)
    return -1;
  return file->ops->flock(file, op);
}

int vfs_setlk(vfs_file_t *file, struct flock *fl)
{
  if (!file || !file->ops || !file->ops->setlk)
    return -1;
  return file->ops->setlk(file, fl);
}

int vfs_getlk(vfs_file_t *file, struct flock *fl)
{
  if (!file || !file->ops || !file->ops->getlk)
    return -1;
  return file->ops->getlk(file, fl);
}

int vfs_closedir(vfs_dir_t *dir)
{
  if (!dir || !dir->ops || !dir->ops->closedir)
    return -1;
  return dir->ops->closedir(dir);
}

int vfs_readdir(vfs_dir_t *dir, struct vfs_dirent *de)
{
  if (!dir || !dir->ops || !dir->ops->readdir)
    return -1;
  return dir->ops->readdir(dir, de);
}

int vfs_dir_has_sfn(vfs_dir_t *dir)
{
  return dir && dir->has_sfn;
}

int vfs_fstatdir(vfs_dir_t *dir, struct stat *statbuf)
{
  if (!dir || !dir->ops || !dir->ops->fstatdir)
    return -1;
  return dir->ops->fstatdir(dir, statbuf);
}

int vfs_fstatat(vfs_dir_t *dir, const char *pathname, struct stat *statbuf, int flags)
{
  if (!dir || !dir->ops || !dir->ops->fstatat)
    return -1;
  return dir->ops->fstatat(dir, pathname, statbuf, flags);
}

int vfs_dirfd(vfs_dir_t *dir)
{
  if (!dir || !dir->ops || !dir->ops->dirfd)
    return -1;
  return dir->ops->dirfd(dir);
}
