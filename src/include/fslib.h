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
#ifndef FSLIB_H
#define FSLIB_H

#include <sys/stat.h>  // for struct stat
#include <sys/statvfs.h>  // for struct statvfs
#include "fssvc.h"  // for setattr_t, getattr_t

int mfs_define_drive(const char *path);
int mfs_open_file(int mfs_idx, const char *path, int flags);
int mfs_create_file(int mfs_idx, const char *path, int flags, mode_t mode);
int mfs_unlink_file(int mfs_idx, const char *path);
int mfs_getxattr_file(int mfs_idx, const char *path);
int mfs_setxattr_file(int mfs_idx, const char *path, int attr);
int do_mfs_statvfs(int mfs_idx, const char *path, struct statvfs *sb);
int mfs_mkdir(int mfs_idx, const char *path, mode_t mode);
int mfs_rmdir(int mfs_idx, const char *path);
int mfs_stat_file(int mfs_idx, const char *path, struct stat *sb);
int mfs_rename_file(int mfs_idx, const char *oldpath, const char *newpath);
int mfs_access(int mfs_idx, const char *path, int mode);
int mfs_utime(int mfs_idx, const char *fpath, time_t atime, time_t mtime);
void fslib_init(plist_idx_t plist_idx, setattr_t setattr_cb,
    getattr_t getattr_cb);
void fslib_seal(void);
void fslib_done(void);
int fslib_path_ok(int idx, const char *path);
int fslib_num_drives(void);

#endif
