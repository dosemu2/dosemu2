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
#ifndef XATTR_H
#define XATTR_H

#ifdef HAVE_SETXATTR

int file_is_ro(const char *fname, mode_t mode);
int set_dos_xattr(const char *fname, int attr);
int get_dos_xattr(const char *fname);
int get_dos_xattr_fd(int fd, const char *name);
int set_dos_xattr_fd(int fd, int attr, const char *name);

#else

static inline int file_is_ro(const char *fname, mode_t mode)
{
    return 0;
}

static inline int set_dos_xattr(const char *fname, int attr)
{
    return 0;
}

static inline int get_dos_xattr(const char *fname)
{
    return -1;
}

static inline int get_dos_xattr_fd(int fd, const char *name)
{
    return -1;
}

static inline int set_dos_xattr_fd(int fd, int attr, const char *name)
{
    return 0;
}

#endif

#endif
