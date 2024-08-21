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
 * But for the more DOS-compatible behavior we inhibit read()s
 * of the readers that didn't acquire any lock (DOS locks are mandatory,
 * so locks on one FD are enough to inhibit I/O on other FDs there).
 * Unfortunately with Posix locks its quite difficult to test
 * the locks on our own FD, as locks on same FD never conflict,
 * so some emulation is emploied for that task.
 * The future kernels will support our Posix extension of using F_UNLCK
 * for testing own locks.
 *
 */
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "dosemu_debug.h"
#include "rlocks.h"

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

static int do_lock_get(int fd, struct flock *fl)
{
  fl->l_pid = 0; // needed for OFD locks
  fl->l_whence = SEEK_SET;
  return fcntl(fd, F_OFD_GETLK, fl);
}

static void lock_get(int fd, struct flock *fl)
{
  int ret = do_lock_get(fd, fl);
  if (ret) {
    error("OFD_GETLK failed, %s\n", strerror(errno));
    /* pretend nothing is locked */
    fl->l_type = F_UNLCK;
  }
}

int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr, int mlemu_fd)
{
  struct flock fl;
  int ret;

  /* make data visible before releasing the lock */
  if (!lck)
    fsync(fd);

  fl.l_type = (lck ? (wr ? F_WRLCK : F_RDLCK) : F_UNLCK);
  fl.l_start = start;
  fl.l_len = len;
  /* needs to lock against I/O operations in another process */
  ret = flock(fd, LOCK_EX);
  if (ret)
    return -1;
  ret = lock_set(fd, &fl);
#if FUNLCK_WA
  if (mlemu_fd != -1)
    lock_set(mlemu_fd, &fl);
#endif
  flock(fd, LOCK_UN);
  return ret;
}

int region_is_fully_owned(int fd, long long start, unsigned long len, int wr,
    int mlemu_fd2)
{
  struct flock fl;
  int err;

  fl.l_type = F_UNLCK;
  fl.l_start = start;
  fl.l_len = len;
  err = do_lock_get(fd, &fl);
#if FUNLCK_WA
  if (err && errno == EINVAL) {  // F_UNLCK extension unsupported
    /* check on mirror fd so rd/wr inverted */
    fl.l_type = wr ? F_RDLCK : F_WRLCK;
    fl.l_start = start;
    fl.l_len = len;
    assert(mlemu_fd2 != -1);
    lock_get(mlemu_fd2, &fl);
  } else
#endif
  if (err) {
    perror("fcntl()");
    return 0;
  }
  if (fl.l_type == F_UNLCK)
    return 0;  // not locked
  if (wr && fl.l_type == F_RDLCK) {
#if FUNLCK_WA
    assert(!err);  // can only happen with F_UNLCK method
#endif
    return 0;  // not sufficient
  }
  if (fl.l_start > start)
    return 0; // not fully locked
  if (fl.l_start + fl.l_len < start + len)
    return 0; // not fully locked
  return 1;
}

int region_lock_offs(int fd, long long start, unsigned long len, int wr)
{
  struct flock fl;
  int ret;

  /* needs to lock against lock changes in another process */
  ret = flock(fd, LOCK_EX);
  if (ret)
    return -1;
  fl.l_type = wr ? F_WRLCK : F_RDLCK;
  fl.l_start = start;
  fl.l_len = len;
  lock_get(fd, &fl);
  if (fl.l_type == F_UNLCK)
    return len;
  if (fl.l_start > start)
    return (fl.l_start - start);  // found partially unlocked region
  /* no allowed region found, unlock and return 0 */
  return flock(fd, LOCK_UN);
}

void region_unlock_offs(int fd)
{
  flock(fd, LOCK_UN);
}

#if FUNLCK_WA
void open_mlemu(int *r_fds)
{
  char *tmp = getenv("TMPDIR");
  char mltmpl[256];
  sprintf(mltmpl, "%s/dosemu2_mlemu_XXXXXX", tmp ? tmp : "/tmp");
  int fd0, fd1;
  struct flock fl;
  int err;

  r_fds[0] = r_fds[1] = -1;
  /* create 2 fds, 1 for mirroring locks and 1 for testing locks */
  fd0 = mkstemp(mltmpl);
  if (fd0 == -1) {
    perror("rlocks mkstemp()");
    return;
  }
  fd1 = open(mltmpl, O_RDONLY | O_CLOEXEC);
  unlink(mltmpl);
  if (fd1 == -1) {
    perror("open()");
    close(fd0);
    return;
  }

  /* check for the linux-6.6 locking extension */
  fl.l_type = F_UNLCK;
  fl.l_start = 0;
  fl.l_len = 1;
  err = do_lock_get(fd1, &fl);
  if (err && errno == EINVAL) {  // F_UNLCK extension unsupported, use mlemu
    r_fds[0] = fd0;
    r_fds[1] = fd1;
  } else {
    if (err)
      error("F_OFD_GETLK returned %s\n", strerror(errno));
    close(fd0);
    close(fd1);
  }
}

void close_mlemu(int *fds)
{
  if (fds[0] != -1)
    close(fds[0]);
  if (fds[1] != -1)
    close(fds[1]);
}
#endif
