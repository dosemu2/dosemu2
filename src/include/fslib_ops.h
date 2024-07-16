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
#ifndef FSLIB_OPS_H
#define FSLIB_OPS_H

#include <sys/stat.h>  // for struct stat
#include <sys/statvfs.h>  // for struct statvfs
#include "fssvc.h"  // for setattr_t, getattr_t

struct fslib_ops {
  int (*add_path)(const char *path);
  int (*add_path_list)(const char *list);
  int (*open)(int mfs_idx, const char *path, int flags);
  int (*create)(int mfs_idx, const char *path, int flags, mode_t mode);
  int (*unlink)(int mfs_idx, const char *path);
  int (*getxattr)(int mfs_idx, const char *path);
  int (*setxattr)(int mfs_idx, const char *path, int attr);
  int (*statvfs)(int mfs_idx, const char *path, struct statvfs *sb);
  int (*mkdir)(int mfs_idx, const char *path, mode_t mode);
  int (*rmdir)(int mfs_idx, const char *path);
  int (*stat)(int mfs_idx, const char *path, struct stat *sb);
  int (*rename)(int id1, const char *oldpath, int id2,
      const char *newpath);
  int (*access)(int mfs_idx, const char *path, int mode);
  int (*utime)(int mfs_idx, const char *fpath, time_t atime, time_t mtime);
  int (*init)(plist_idx_t plist_idx, setattr_t setattr_cb,
      getattr_t getattr_cb);
  int (*seal)(void);
  int (*exit)(void);
  int (*path_ok)(int idx, const char *path);
};

void fslib_register_ops(const struct fslib_ops *ops);

#endif
