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
#ifndef SHARE_H
#define SHARE_H

struct file_fd;

struct file_fd *do_claim_fd(const char *name);
struct file_fd *mfs_creat(int mfs_idx, const char *name, mode_t mode);
struct file_fd *mfs_open(int mfs_idx, const char *name, int flags,
        int share_mode, int *r_err);
int mfs_unlink(const char *name);
int mfs_setattr(const char *name, int attr);
int mfs_rename(const char *name, const char *name2);
void mfs_close(struct file_fd *f);

#endif
