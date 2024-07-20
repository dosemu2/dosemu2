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
#include "fslib_ops.h"

#define MAX_PATHS 26
static char *def_drives[MAX_PATHS];
static char *plist;
static int num_def_drives;
static char *paths_ex[MAX_PATHS];
static int num_paths_ex;
static int sealed;
static plist_idx_t plist_idx_cb;
static setattr_t do_setattr;
static getattr_t do_getattr;

static int add_path(const char *path)
{
  int len;

  assert(!sealed);
  assert(num_def_drives < MAX_PATHS);
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
  return num_def_drives++;
}

static int add_path_ex(const char *path)
{
    int len;

    assert(num_paths_ex < MAX_PATHS);
    assert(!sealed);
    len = strlen(path);
    assert(len > 0);
    paths_ex[num_paths_ex++] = strdup(path);
    return 0;
}

static int add_path_list(const char *clist)
{
    assert(!sealed);
    plist = strdup(clist);
    return 0;
}

static int path_ok(int idx, const char *path)
{
  int len;

  assert(sealed);
  if (idx < 0) {
    int i;
    for (i = 0; i < num_paths_ex; i++) {
      if (strcmp(path, paths_ex[i]) == 0)
        return 1;
    }
    return 0;
  }
  if (idx >= num_def_drives)
    return (plist && plist_idx_cb(plist, path) + num_def_drives == idx);
  assert(def_drives[idx]);
  len = strlen(def_drives[idx]);
  assert(len && def_drives[idx][len - 1] == '/');
  if (strlen(path) == len - 1)
    len--;  // no trailing slash
  if (strncmp(path, def_drives[idx], len) != 0)
    return 0;
  return 1;
}

static int open_file(int mfs_idx, const char *path, int flags)
{
  assert(path_ok(mfs_idx, path));
  return open(path, flags);
}

static int create_file(int mfs_idx, const char *path, int flags, mode_t mode)
{
  assert(path_ok(mfs_idx, path));
  return open(path, flags, mode);
}

static int unlink_file(int mfs_idx, const char *path)
{
  assert(path_ok(mfs_idx, path));
  return unlink(path);
}

static int setxattr_file(int mfs_idx, const char *path, int attr)
{
  assert(path_ok(mfs_idx, path));
  return do_setattr(path, attr);
}

static int getxattr_file(int mfs_idx, const char *path)
{
  assert(path_ok(mfs_idx, path));
  return do_getattr(path);
}

static int rename_file(int idx1, const char *oldpath, int idx2,
    const char *newpath)
{
  assert(path_ok(idx1, oldpath));
  assert(path_ok(idx2, newpath));
  return rename(oldpath, newpath);
}

static int do_mkdir(int mfs_idx, const char *path, mode_t mode)
{
  assert(path_ok(mfs_idx, path));
  return mkdir(path, mode);
}

static int do_rmdir(int mfs_idx, const char *path)
{
  assert(path_ok(mfs_idx, path));
  return rmdir(path);
}

static int stat_file(int mfs_idx, const char *path, struct stat *sb)
{
  assert(path_ok(mfs_idx, path));
  return stat(path, sb);
}

static int do_statvfs(int mfs_idx, const char *path, struct statvfs *sb)
{
  assert(path_ok(mfs_idx, path));
  return statvfs(path, sb);
}

static int do_access(int mfs_idx, const char *path, int mode)
{
  assert(path_ok(mfs_idx, path));
  return access(path, mode);
}

static int do_utime(int mfs_idx, const char *fpath, time_t atime, time_t mtime)
{
  struct utimbuf ut = { .actime = atime, .modtime = mtime };

  assert(path_ok(mfs_idx, fpath));
  return utime(fpath, &ut);
}

static int do_init(plist_idx_t plist_idx, setattr_t setattr_cb,
    getattr_t getattr_cb)
{
  plist_idx_cb = plist_idx;
  do_setattr = setattr_cb;
  do_getattr = getattr_cb;
  return 0;
}

static int fslocal_seal(void)
{
  assert(!sealed);
  sealed++;
  return num_def_drives;
}

static int fslocal_done(void)
{
  int i;
  for (i = 0; i < num_def_drives; i++)
    free(def_drives[i]);
  free(plist);
  return 0;
}

static int fslocal_path_ok(int idx, const char *path)
{
  return path_ok(idx, path);
}

static const struct fslib_ops fslops = {
  .add_path = add_path,
  .add_path_ex = add_path_ex,
  .add_path_list = add_path_list,
  .open = open_file,
  .create = create_file,
  .unlink = unlink_file,
  .setxattr = setxattr_file,
  .getxattr = getxattr_file,
  .rename = rename_file,
  .mkdir = do_mkdir,
  .rmdir = do_rmdir,
  .stat = stat_file,
  .statvfs = do_statvfs,
  .access = do_access,
  .utime = do_utime,
  .init = do_init,
  .seal = fslocal_seal,
  .exit = fslocal_done,
  .path_ok = fslocal_path_ok,
  .name = "local",
  .flags = FSFLG_NOSUID,
};

void fslocal_init(void)
{
  fslib_register_ops(&fslops);
}
