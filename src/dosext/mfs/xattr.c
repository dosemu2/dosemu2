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

/*
 * Purpose: xattrs support.
 *
 * Author: @stsp
 *
 */
#include <sys/xattr.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "dosemu_debug.h"
#include "mfs.h"
#include "xattr.h"

#define XATTR_DOSATTR_NAME "user.DOSATTRIB"
#define XATTR_ATTRIBS_MASK (READ_ONLY_FILE | HIDDEN_FILE | SYSTEM_FILE | \
  ARCHIVE_NEEDED)

#ifdef __APPLE__
#define getxattr(path,name,value,size) getxattr(path,name,value,size,0,0)
#define fgetxattr(fd,name,value,size) fgetxattr(fd,name,value,size,0,0)
#define setxattr(path,name,value,size,flags) setxattr(path,name,value,size,0,flags)
#define fsetxattr(fd,name,value,size,flags) fsetxattr(fd,name,value,size,0,flags)
#endif

static int do_extr_xattr(const char *xbuf, ssize_t size, const char *name)
{
  if (size == -1) {
    int errn = errno;
    if (errn == ENODATA)
      return 0;
    error("MFS: failed to get xattrs for %s, %s\n", name, strerror(errn));
    return -1;
  }
  if (size <= 2 || strncmp(xbuf, "0x", 2) != 0)
    return -1;
  return strtol(xbuf + 2, NULL, 16) & XATTR_ATTRIBS_MASK;
}

static int xattr_str(char *xbuf, int xsize, int attr)
{
  int ret = snprintf(xbuf, xsize, "0x%x", attr & XATTR_ATTRIBS_MASK);
  assert(ret > 0);
  return (ret + 1);  // include '\0'
}

static int xattr_err(int err, const char *name)
{
  if (err) {
    error("MFS: failed to set xattrs for %s: %s\n", name, strerror(errno));
//    leavedos(5);
//    return ACCESS_DENIED;
  }
  return 0;
}

int set_dos_xattr_fd(int fd, int attr, const char *name)
{
  char xbuf[16];
  return xattr_err(fsetxattr(fd, XATTR_DOSATTR_NAME, xbuf,
      xattr_str(xbuf, sizeof(xbuf), attr), 0), name);
}

int set_dos_xattr(const char *fname, int attr)
{
  char xbuf[16];
  int err = setxattr(fname, XATTR_DOSATTR_NAME, xbuf,
      xattr_str(xbuf, sizeof(xbuf), attr), 0);
  if (err) {
    struct stat st;
    int err2 = stat(fname, &st);
    if (err2)
      return FILE_NOT_FOUND;
    if (!(st.st_mode & S_IWUSR)) {
      err = chmod(fname, st.st_mode | S_IWUSR);
      if (err)
        return ACCESS_DENIED;
      err = setxattr(fname, XATTR_DOSATTR_NAME, xbuf,
            xattr_str(xbuf, sizeof(xbuf), attr), 0);
    }
  }
  return xattr_err(err, fname);
}

int get_dos_xattr(const char *fname)
{
  char xbuf[16];
  ssize_t size = getxattr(fname, XATTR_DOSATTR_NAME, xbuf, sizeof(xbuf) - 1);
  /* some dosemus forgot \0 so we fix it up here */
  if (size > 0 && xbuf[size - 1] != '\0') {
    error("MFS: fixup xattr for %s\n", fname);
    xbuf[size++] = '\0';
    setxattr(fname, XATTR_DOSATTR_NAME, xbuf, size, XATTR_REPLACE);
  }
  return do_extr_xattr(xbuf, size, fname);
}

int get_dos_xattr_fd(int fd, const char *name)
{
  char xbuf[16];
  ssize_t size = fgetxattr(fd, XATTR_DOSATTR_NAME, xbuf, sizeof(xbuf) - 1);
  /* some dosemus forgot \0 so we fix it up here */
  if (size > 0 && xbuf[size - 1] != '\0') {
    error("MFS: fixup xattr\n");
    xbuf[size++] = '\0';
    fsetxattr(fd, XATTR_DOSATTR_NAME, xbuf, size, XATTR_REPLACE);
  }
  return do_extr_xattr(xbuf, size, name);
}

int file_is_ro(const char *fname)
{
    int attr = get_dos_xattr(fname);
    /* NOTE: do not use unix file perms for R/O as that may crash
     * some cdrom games:
     * https://github.com/dosemu2/dosemu2/issues/989
     */
    if (attr == -1)
        return 0;
    return !!(attr & READ_ONLY_FILE);
}
