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
#include <string.h>
#include <assert.h>
#include "emu.h"
#include "fslib_ops.h"
#include "fslib.h"

static const struct fslib_ops *fssvc;
static int num_def_drives;

int mfs_define_drive(const char *path)
{
  int ret;

  ret = fssvc->add_path(path);
  if (ret == -1)
    return ret;
  return ret + 1;
}

int fslib_add_path_list(const char *list)
{
  return fssvc->add_path_list(list);
}

int fslib_add_path_ex(const char *path)
{
  return fssvc->add_path_ex(path);
}

int mfs_open_file(int mfs_idx, const char *path, int flags)
{
  return fssvc->open(mfs_idx - 1, path, flags);
}

int mfs_create_file(int mfs_idx, const char *path, int flags, mode_t mode)
{
  return fssvc->create(mfs_idx - 1, path, flags, mode);
}

int mfs_unlink_file(int mfs_idx, const char *path)
{
  return fssvc->unlink(mfs_idx - 1, path);
}

int mfs_setxattr_file(int mfs_idx, const char *path, int attr)
{
  return fssvc->setxattr(mfs_idx - 1, path, attr);
}

int mfs_getxattr_file(int mfs_idx, const char *path)
{
  return fssvc->getxattr(mfs_idx - 1, path);
}

int mfs_rename_file(int mfs_idx, const char *oldpath, const char *newpath)
{
  return fssvc->rename(mfs_idx - 1, oldpath, mfs_idx - 1, newpath);
}

int mfs_mkdir(int mfs_idx, const char *path, mode_t mode)
{
  return fssvc->mkdir(mfs_idx - 1, path, mode);
}

int mfs_rmdir(int mfs_idx, const char *path)
{
  return fssvc->rmdir(mfs_idx - 1, path);
}

int mfs_stat_file(int mfs_idx, const char *path, struct stat *sb)
{
  return fssvc->stat(mfs_idx - 1, path, sb);
}

int do_mfs_statvfs(int mfs_idx, const char *path, struct statvfs *sb)
{
  return fssvc->statvfs(mfs_idx - 1, path, sb);
}

int mfs_access(int mfs_idx, const char *path, int mode)
{
  return fssvc->access(mfs_idx - 1, path, mode);
}

int mfs_utime(int mfs_idx, const char *fpath, time_t atime, time_t mtime)
{
  return fssvc->utime(mfs_idx - 1, fpath, atime, mtime);
}

void fslib_init(plist_idx_t plist_idx, setattr_t setattr_cb,
    getattr_t getattr_cb)
{
  int err;

  fsrpc_init();
  fslocal_init();
  err = fssvc->init(plist_idx, setattr_cb, getattr_cb);
  assert(!err);
}

void fslib_seal(void)
{
  num_def_drives = fssvc->seal();
  assert(num_def_drives != -1);
}

void fslib_done(void)
{
  fssvc->exit();
}

int fslib_path_ok(int idx, const char *path)
{
  return fssvc->path_ok(idx, path);
}

int fslib_num_drives(void)
{
  return num_def_drives;
}

static const char *def_name = "rpc";

void fslib_register_ops(const struct fslib_ops *ops)
{
  const char *expect = (config.fs_backend ?: def_name);
  if (strcmp(ops->name, expect) != 0)
    return;
  assert(!fssvc);
  fssvc = ops;
}
