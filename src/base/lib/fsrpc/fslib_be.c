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
#include <unistd.h>
#include <fcntl.h>
#include "fssvc_priv.h"
#include "fslib_ops.h"

static int stat_file(int mfs_idx, const char *path, struct stat *sb)
{
  int err;
  int fd = fssvc_open(mfs_idx, path, O_RDONLY);
  if (fd == -1)
    return -1;
  err = fstat(fd, sb);
  close(fd);
  return err;
}

static int do_statvfs(int mfs_idx, const char *path, struct statvfs *sb)
{
  int err;
  int fd = fssvc_open(mfs_idx, path, O_RDONLY);
  if (fd == -1)
    return -1;
  err = fstatvfs(fd, sb);
  close(fd);
  return err;
}

static int do_access(int mfs_idx, const char *path, int mode)
{
  int o_mode = (mode == F_OK || mode == R_OK || mode == X_OK) ?
      O_RDONLY : O_WRONLY;
  int fd = fssvc_open(mfs_idx, path, o_mode);
  if (fd == -1)
    return -1;
  close(fd);
  return 0;
}

static const struct fslib_ops fsops = {
  .add_path = fssvc_add_path,
  .add_path_ex = fssvc_add_path_ex,
  .add_path_list = fssvc_add_path_list,
  .open = fssvc_open,
  .create = fssvc_creat,
  .unlink = fssvc_unlink,
  .setxattr = fssvc_setxattr,
  .getxattr = fssvc_getxattr,
  .rename = fssvc_rename,
  .mkdir = fssvc_mkdir,
  .rmdir = fssvc_rmdir,
  .stat = stat_file,
  .statvfs = do_statvfs,
  .access = do_access,
  .utime = fssvc_utime,
  .init = fssvc_init,
  .seal = fssvc_seal,
  .exit = fssvc_exit,
  .path_ok = fssvc_path_ok,
  .name = "rpc",
};

void fsrpc_init(void)
{
  fslib_register_ops(&fsops);
}
