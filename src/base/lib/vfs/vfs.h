/*
 * VFS (Virtual File System) interface
 */
#ifndef VFS_H
#define VFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

typedef struct vfs_fs vfs_fs_t;
typedef struct vfs_file vfs_file_t;
typedef struct vfs_dir vfs_dir_t;

struct vfs_fs_ops {
  vfs_file_t *(*open)(vfs_fs_t *fs, const char *path, int flags);
  vfs_file_t *(*creat)(vfs_fs_t *fs, const char *path, int flags, mode_t mode);
  int (*unlink)(vfs_fs_t *fs, const char *path);
  int (*getxattr)(vfs_fs_t *fs, const char *path);
  int (*setxattr)(vfs_fs_t *fs, const char *path, int attr);
  int (*statvfs)(vfs_fs_t *fs, const char *path, struct statvfs *sb);
  int (*mkdir)(vfs_fs_t *fs, const char *path, mode_t mode);
  int (*rmdir)(vfs_fs_t *fs, const char *path);
  int (*stat)(vfs_fs_t *fs, const char *path, struct stat *sb);
  int (*rename)(vfs_fs_t *fs, const char *oldpath, const char *newpath);
  int (*access)(vfs_fs_t *fs, const char *path, int mode);
  int (*utime)(vfs_fs_t *fs, const char *fpath, time_t atime, time_t mtime);
  void *(*open_async)(vfs_fs_t *fs, const char *path, int flags);
  vfs_dir_t *(*opendir)(vfs_fs_t *fs, const char *path);
};

struct vfs_file_ops {
  int (*close)(vfs_file_t *file);
  ssize_t (*read)(vfs_file_t *file, void *buf, size_t count);
  ssize_t (*write)(vfs_file_t *file, const void *buf, size_t count);
  off_t (*lseek)(vfs_file_t *file, off_t offset, int whence);
  int (*fstat)(vfs_file_t *file, struct stat *sb);
  int (*ftruncate)(vfs_file_t *file, off_t length);
  int (*fsync)(vfs_file_t *file);
  int (*get_async_fd)(vfs_file_t *file, void *handle);
};

struct vfs_dir_ops {
  int (*closedir)(vfs_dir_t *dir);
  struct dirent *(*readdir)(vfs_dir_t *dir);
  int (*fstatdir)(vfs_dir_t *file, struct stat *sb);
  int (*fstatat)(vfs_dir_t *dir, const char *pathname, struct stat *statbuf, int flags);
  int (*dirfd)(vfs_dir_t *dir);
};

struct vfs_fs {
  const struct vfs_fs_ops *ops;
  int mfs_idx;
};

struct vfs_file {
  const struct vfs_file_ops *ops;
  int fd;
};

struct vfs_dir {
  const struct vfs_dir_ops *ops;
  DIR *d;
  int fd;
};

vfs_fs_t *vfs_get_fs(int mfs_idx);

vfs_file_t *vfs_open(vfs_fs_t *fs, const char *path, int flags);
vfs_file_t *vfs_creat(vfs_fs_t *fs, const char *path, int flags, mode_t mode);
int vfs_unlink(vfs_fs_t *fs, const char *path);
int vfs_getxattr(vfs_fs_t *fs, const char *path);
int vfs_setxattr(vfs_fs_t *fs, const char *path, int attr);
int vfs_statvfs(vfs_fs_t *fs, const char *path, struct statvfs *sb);
int vfs_mkdir(vfs_fs_t *fs, const char *path, mode_t mode);
int vfs_rmdir(vfs_fs_t *fs, const char *path);
int vfs_stat(vfs_fs_t *fs, const char *path, struct stat *sb);
int vfs_rename(vfs_fs_t *fs, const char *oldpath, const char *newpath);
int vfs_access(vfs_fs_t *fs, const char *path, int mode);
int vfs_utime(vfs_fs_t *fs, const char *fpath, time_t atime, time_t mtime);
void *vfs_open_async(vfs_fs_t *fs, const char *path, int flags);
vfs_dir_t *vfs_opendir(vfs_fs_t *fs, const char *path);

vfs_file_t *vfs_file_wrap_posix(int fd);
vfs_dir_t *vfs_dir_wrap_posix(DIR *d, int fd);

int vfs_close(vfs_file_t *file);
ssize_t vfs_read(vfs_file_t *file, void *buf, size_t count);
ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t count);
off_t vfs_lseek(vfs_file_t *file, off_t offset, int whence);
int vfs_fstat(vfs_file_t *file, struct stat *sb);
int vfs_ftruncate(vfs_file_t *file, off_t length);
int vfs_fsync(vfs_file_t *file);
int vfs_get_async_fd(vfs_file_t *file, void *handle);

int vfs_closedir(vfs_dir_t *dir);
struct dirent *vfs_readdir(vfs_dir_t *dir);
int vfs_fstatat(vfs_dir_t *dir, const char *pathname, struct stat *statbuf, int flags);
int vfs_fstatdir(vfs_dir_t *dir, struct stat *statbuf);
int vfs_dirfd(vfs_dir_t *dir);

#endif
