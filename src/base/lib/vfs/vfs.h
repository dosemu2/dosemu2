/*
 * VFS (Virtual File System) interface
 */
#ifndef VFS_H
#define VFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
  /*
   * Two-phase open: open_async() reserves the open, then the caller
   * does its own checks and either claims the file or drops it. The
   * split exists for the privileged fs service, which needs the
   * reservation and the fd handover to be separate steps.
   */
  void *(*open_async)(vfs_fs_t *fs, const char *path, int flags);
  vfs_file_t *(*async_getfile)(vfs_fs_t *fs, void *handle);
  void (*async_cancel)(vfs_fs_t *fs, void *handle);
  vfs_dir_t *(*opendir)(vfs_fs_t *fs, const char *path);
  /*
   * Native DOS attributes, as opposed to the ones dosemu stores in an
   * xattr of its own. Return -1 with errno set to ENOSYS when the
   * backend has none, so that the caller can fall back to the xattr.
   */
  int (*get_dos_attr)(vfs_fs_t *fs, const char *path, int mode);
  int (*set_dos_attr)(vfs_fs_t *fs, const char *path, int attr);
};

struct vfs_file_ops {
  int (*close)(vfs_file_t *file);
  ssize_t (*read)(vfs_file_t *file, void *buf, size_t count);
  ssize_t (*write)(vfs_file_t *file, const void *buf, size_t count);
  off_t (*lseek)(vfs_file_t *file, off_t offset, int whence);
  int (*fstat)(vfs_file_t *file, struct stat *sb);
  int (*ftruncate)(vfs_file_t *file, off_t length);
  int (*fsync)(vfs_file_t *file);
  /* whole-file advisory lock, used to serialize region lock updates */
  int (*flock)(vfs_file_t *file, int op);
  /* OFD region locks */
  int (*setlk)(vfs_file_t *file, struct flock *fl);
  int (*getlk)(vfs_file_t *file, struct flock *fl);
  /* see the fs ops of the same name */
  int (*get_dos_attr)(vfs_file_t *file, int mode);
  int (*set_dos_attr)(vfs_file_t *file, int attr);
};

/*
 * A backend may know a short (8.3) name of its own, as FAT does. When
 * it does not, d_name and d_long_name are the same string.
 */
struct vfs_dirent {
  const char *d_name;
  const char *d_long_name;
};

struct vfs_dir_ops {
  int (*closedir)(vfs_dir_t *dir);
  int (*readdir)(vfs_dir_t *dir, struct vfs_dirent *de);
  int (*fstatdir)(vfs_dir_t *file, struct stat *sb);
  int (*fstatat)(vfs_dir_t *dir, const char *pathname, struct stat *statbuf, int flags);
};

struct vfs_backend;

struct vfs_fs {
  const struct vfs_fs_ops *ops;
  int mfs_idx;
  const struct vfs_backend *be;
  void *priv;
};

struct vfs_backend {
  const char *name;
  /* returns 1 if this backend handles the given path */
  int (*probe)(const char *path);
  /* fills in fs->ops and fs->priv, returns 0 on success */
  int (*mount)(vfs_fs_t *fs, const char *path);
  void (*umount)(vfs_fs_t *fs);
};

struct vfs_file {
  const struct vfs_file_ops *ops;
  int fd;
};

struct vfs_dir {
  const struct vfs_dir_ops *ops;
  DIR *d;
  int fd;
  /* readdir hands out the real 8.3 name, so no mangling is needed */
  int has_sfn;
};

/*
 * Whether the redirector is serving a call that has no long names,
 * i.e. int2f/11xx rather than int21/71xx.
 */
void vfs_set_short_names(int on);

void vfs_register_backend(const struct vfs_backend *be);
int vfs_bind(int mfs_idx, const char *path);
/* whether some backend claims the path, without mounting it */
int vfs_probe(const char *path);
void vfs_done(void);

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
vfs_file_t *vfs_async_getfile(vfs_fs_t *fs, void *handle);
void vfs_async_cancel(vfs_fs_t *fs, void *handle);
vfs_dir_t *vfs_opendir(vfs_fs_t *fs, const char *path);
int vfs_get_dos_attr(vfs_fs_t *fs, const char *path, int mode);
int vfs_set_dos_attr(vfs_fs_t *fs, const char *path, int attr);


int vfs_close(vfs_file_t *file);
ssize_t vfs_read(vfs_file_t *file, void *buf, size_t count);
ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t count);
off_t vfs_lseek(vfs_file_t *file, off_t offset, int whence);
int vfs_fstat(vfs_file_t *file, struct stat *sb);
int vfs_ftruncate(vfs_file_t *file, off_t length);
int vfs_fsync(vfs_file_t *file);
int vfs_flock(vfs_file_t *file, int op);
int vfs_setlk(vfs_file_t *file, struct flock *fl);
int vfs_getlk(vfs_file_t *file, struct flock *fl);
int vfs_fget_dos_attr(vfs_file_t *file, int mode);
int vfs_fset_dos_attr(vfs_file_t *file, int attr);

int vfs_closedir(vfs_dir_t *dir);
int vfs_readdir(vfs_dir_t *dir, struct vfs_dirent *de);
int vfs_dir_has_sfn(vfs_dir_t *dir);
int vfs_fstatat(vfs_dir_t *dir, const char *pathname, struct stat *statbuf, int flags);
int vfs_fstatdir(vfs_dir_t *dir, struct stat *statbuf);
/*
 * Reads the rest of the directory into a sorted array of names, each
 * malloc'd, and so is the array. Returns the number of entries, or -1.
 */
int vfs_scandir(vfs_dir_t *dir, char ***namelist,
    int (*filter)(const char *name));

#endif
