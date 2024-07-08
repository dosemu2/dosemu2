/*
 *  Copyright (C) 2024  stsp
 *
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "fssvc.h"
#include "fslib.h"

#define MAX_DRIVE 26
static char *def_drives[MAX_DRIVE];
static int num_def_drives;
static int have_fssvc;
static int sealed;
static setattr_t do_setattr;
static getattr_t do_getattr;

int mfs_define_drive(const char *path)
{
  int len;

  assert(!sealed);
  if (have_fssvc) {
    int ret = fssvc_add_path(path);
    if (ret == -1)
      return ret;
    return ret + 1;
  }
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

static int path_ok(int idx, const char *path)
{
  int len;

  assert(sealed);
  assert(!have_fssvc);
  if (idx >= num_def_drives)
    return 0;
  assert(def_drives[idx]);
  len = strlen(def_drives[idx]);
  assert(len && def_drives[idx][len - 1] == '/');
  if (strlen(path) == len - 1)
    len--;  // no trailing slash
  if (strncmp(path, def_drives[idx], len) != 0)
    return 0;
  return 1;
}

int mfs_open_file(int mfs_idx, const char *path, int flags)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return open(path, flags);
  if (have_fssvc)
    return fssvc_open(mfs_idx - 1, path, flags);
  assert(path_ok(mfs_idx - 1, path));
  return open(path, flags);
}

int mfs_create_file(int mfs_idx, const char *path, int flags, mode_t mode)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return open(path, flags, mode);
  if (have_fssvc)
    return fssvc_creat(mfs_idx - 1, path, flags, mode);
  assert(path_ok(mfs_idx - 1, path));
  return open(path, flags, mode);
}

int mfs_unlink_file(int mfs_idx, const char *path)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return unlink(path);
  if (have_fssvc)
    return fssvc_unlink(mfs_idx - 1, path);
  assert(path_ok(mfs_idx - 1, path));
  return unlink(path);
}

int mfs_setxattr_file(int mfs_idx, const char *path, int attr)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return do_setattr(path, attr);
  if (have_fssvc)
    return fssvc_setxattr(mfs_idx - 1, path, attr);
  assert(path_ok(mfs_idx - 1, path));
  return do_setattr(path, attr);
}

int mfs_getxattr_file(int mfs_idx, const char *path)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return do_getattr(path);
  if (have_fssvc)
    return fssvc_getxattr(mfs_idx - 1, path);
  assert(path_ok(mfs_idx - 1, path));
  return do_getattr(path);
}

int mfs_rename_file(int mfs_idx, const char *oldpath, const char *newpath)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return rename(oldpath, newpath);
  if (have_fssvc)
    return fssvc_rename(mfs_idx - 1, oldpath, mfs_idx - 1, newpath);
  assert(path_ok(mfs_idx - 1, oldpath));
  assert(path_ok(mfs_idx - 1, newpath));
  return rename(oldpath, newpath);
}

int mfs_mkdir(int mfs_idx, const char *path, mode_t mode)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return mkdir(path, mode);
  if (have_fssvc)
    return fssvc_mkdir(mfs_idx - 1, path, mode);
  assert(path_ok(mfs_idx - 1, path));
  return mkdir(path, mode);
}

int mfs_rmdir(int mfs_idx, const char *path)
{
  assert(mfs_idx);
  if (mfs_idx > num_def_drives)
    return rmdir(path);
  if (have_fssvc)
    return fssvc_rmdir(mfs_idx - 1, path);
  assert(path_ok(mfs_idx - 1, path));
  return rmdir(path);
}

int mfs_stat_file(int mfs_idx, const char *path, struct stat *sb)
{
  int err;
  int fd = mfs_open_file(mfs_idx, path, O_RDONLY);
  if (fd == -1)
    return -1;
  err = fstat(fd, sb);
  close(fd);
  return err;
}

int do_mfs_statvfs(int mfs_idx, const char *path, struct statvfs *sb)
{
  int err;
  int fd = mfs_open_file(mfs_idx, path, O_RDONLY);
  if (fd == -1)
    return -1;
  err = fstatvfs(fd, sb);
  close(fd);
  return err;
}

int mfs_access(int mfs_idx, const char *path, int mode)
{
  int o_mode = (mode == F_OK || mode == R_OK || mode == X_OK) ?
      O_RDONLY : O_WRONLY;
  int fd = mfs_open_file(mfs_idx, path, o_mode);
  if (fd == -1)
    return -1;
  close(fd);
  return 0;
}

int mfs_utime(int mfs_idx, const char *fpath, time_t atime, time_t mtime)
{
  struct utimbuf ut = { .actime = atime, .modtime = mtime };

  assert(mfs_idx);
  if (mfs_idx > num_def_drives) {
    return utime(fpath, &ut);
  }
  if (have_fssvc)
    return fssvc_utime(mfs_idx - 1, fpath, atime, mtime);
  assert(path_ok(mfs_idx - 1, fpath));
  return utime(fpath, &ut);
}

int fslib_init(setattr_t setattr_cb, getattr_t getattr_cb)
{
  int err = fssvc_init(setattr_cb, getattr_cb);
  if (err)
    err = 1;
  else
    have_fssvc++;
  do_setattr = setattr_cb;
  do_getattr = getattr_cb;
  return err;
}

void fslib_seal(void)
{
  assert(!sealed);
  if (have_fssvc) {
    num_def_drives = fssvc_seal();
    assert(num_def_drives != -1);
  }
  sealed++;
}

void fslib_done(void)
{
  if (have_fssvc)
    fssvc_exit();
  else {
    int i;
    for (i = 0; i < num_def_drives; i++)
      free(def_drives[i]);
  }
}

int fslib_path_ok(int idx, const char *path)
{
  if (have_fssvc)
    return fssvc_path_ok(idx, path);
  return path_ok(idx, path);
}

int fslib_num_drives(void)
{
  return num_def_drives;
}
