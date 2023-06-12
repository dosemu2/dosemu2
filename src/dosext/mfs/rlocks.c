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
 * Purpose: region locks support.
 *
 * Author: @stsp
 *
 * Note: DOS lock semantic doesn't distinguish between read and write
 * locks. It only have exclusive region locks. They are "mandatory",
 * therefore they inhibit both reads and writes.
 * Posix locks do not mimic that because you can't set the write lock
 * on a read-only fd (or set read lock on a write-only fd, not our case).
 * We can provide the DOS locking semantic by trying to force the write
 * mode on every open file. This may include chmod()ing and checking
 * the writability by hands. Huge hacks! We did that at some point of
 * time, but current implementation drops all hacks and provides the
 * Posix-alike semantic instead, with the "only" difference from Posix
 * being the locks are mandatory.
 *
 * Note: Since we, unlike DOS does, distinguish between reader and
 * writer locks, we also need to make sure reader locks never inhibit
 * other's read()s, rather than the "inhibit any I/O of others"
 * behavior of the DOS exclusive locks. This is because now multiple
 * readers can grab the read lock simultaneously, so all should be
 * able to read, rather than blocking each other's read()s.
 * The more DOS-compatible alternative could be to inhibit read()s
 * of the readers that didn't acquire any lock (DOS locks are mandatory,
 * so locks on one FD are enough to inhibit I/O on other FDs there),
 * but unfortunately with Posix locks its quite difficult to test
 * the locks on our own FD, as locks on same FD never conflict.
 *
 */
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "dosemu_debug.h"
#include "rlocks.h"

#define d_Stub(arg1, s, a...)   d_printf("MFS: " s, ##a)
#define Debug0(args)		d_Stub args
#define Debug1(args)		d_Stub args

static int lock_set(int fd, struct flock *fl)
{
  int ret;
  fl->l_pid = 0; // needed for OFD locks
  fl->l_whence = SEEK_SET;
  ret = fcntl(fd, F_OFD_SETLK, fl);
  if (ret) {
    int err = errno;
    if (err != EAGAIN)
      error("OFD_SETLK failed, %s\n", strerror(err));
  }
  return ret;
}

static void lock_get(int fd, struct flock *fl)
{
  int ret;
  fl->l_pid = 0; // needed for OFD locks
  fl->l_whence = SEEK_SET;
  ret = fcntl(fd, F_OFD_GETLK, fl);
  if (ret) {
    error("OFD_GETLK failed, %s\n", strerror(errno));
    /* pretend nothing is locked */
    fl->l_type = F_UNLCK;
  }
}

int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr)
{
  struct flock fl;
  int ret;

  /* make data visible before releasing the lock */
  if (!lck)
    fsync(fd);

#ifdef F_GETLK64	// 64bit locks are promoted automatically (e.g. glibc)
//  static_assert(sizeof(struct flock) == sizeof(struct flock64), "incompatible flock64");

  Debug0((dbg_fd, "Large file locking start=%llx, len=%lx\n", start, len));
#else			// 32bit locking only
#error 64bit locking not supported
#endif

  fl.l_type = (lck ? (wr ? F_WRLCK : F_RDLCK) : F_UNLCK);
  fl.l_start = start;
  fl.l_len = len;
  /* needs to lock against I/O operations in another process */
  ret = flock(fd, LOCK_EX);
  if (ret)
    return -1;
  ret = lock_set(fd, &fl);
  flock(fd, LOCK_UN);
  return ret;
}

int region_lock_offs(int fd, long long start, unsigned long len, int wr)
{
  struct flock fl;
  int ret;

  fl.l_type = wr ? F_WRLCK : F_RDLCK;
  fl.l_start = start;
  fl.l_len = len;
  /* needs to lock against lock changes in another process */
  ret = flock(fd, LOCK_EX);
  if (ret)
    return -1;
  lock_get(fd, &fl);
  if (fl.l_type == F_UNLCK)
    return len;
  return (fl.l_start - start);
}

void region_unlock_offs(int fd)
{
  flock(fd, LOCK_UN);
}
