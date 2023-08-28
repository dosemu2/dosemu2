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
 * shlock - shared UUCP-style lockfile library.
 *
 * Author: @stsp
 *
 */
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "shlock.h"

#define LOCK_DIR "/tmp"
#define LOCK_PFX "LCK.."

struct shlck {
  char *tspec;
  char *fspec;
  char *dir;
  int excl;
  int fd;
};

void *shlock_open(const char *dir, const char *name, int excl, int block)
{
  struct shlck *ret;
  char *fspec, *dspec, *tspec;
  int fd, tmp_fd, rc;
  int flg = block ? 0 : LOCK_NB;

  rc = asprintf(&dspec, LOCK_DIR "/%s", dir);
  if (rc == -1) {
    perror("asprintf()");
    return NULL;
  }
  rc = asprintf(&fspec, "%s/%s", dspec, name);
  if (rc == -1) {
    perror("asprintf()");
    goto err_dspec;
  }
  rc = asprintf(&tspec, "%s/" LOCK_PFX "%i_XXXXXX", fspec, getpid());
  if (rc == -1) {
    perror("asprintf()");
    goto err_fspec;
  }
  tmp_fd = -1;
  while (tmp_fd == -1) {
    rc = mkdir(dspec, 0775);
    if (rc == -1 && errno != EEXIST) {
      perror("mkdir()");
      goto err_free;
    }
    rc = mkdir(fspec, 0775);
    if (rc == -1) {
      switch (errno) {
        case EEXIST:
          break;
        case ENOENT:
          continue;  // race, someone removed parent dir
        default:
          perror("mkdir()");
          goto err_rmddir;
      }
    }
    tmp_fd = mkstemp(tspec);
    if (tmp_fd == -1) {
      if (errno == ENOENT)
        continue;  // race, someone removed parent dir
      perror("mkstemp()");
      goto err_rmfdir;
    }
  }

  fd = open(fspec, O_RDONLY | O_DIRECTORY);
  close(tmp_fd);
  if (fd == -1) {
    perror("open(dir)");
    goto err_rmt;
  }
  if (excl) {
    rc = flock(fd, LOCK_EX | flg);
    if (rc == -1)
      goto err_close;
  } else {
    rc = flock(fd, LOCK_SH | flg);
    if (rc == -1) {
      perror("flock()");
      goto err_close;
    }
  }

  ret = malloc(sizeof(*ret));
  ret->tspec = tspec;
  ret->fspec = fspec;
  ret->dir = dspec;
  ret->excl = excl;
  ret->fd = fd;
  return ret;

err_close:
  close(fd);
err_rmt:
  unlink(tspec);
err_rmfdir:
  rmdir(fspec);
err_rmddir:
  rmdir(dspec);
err_free:
  free(tspec);
err_fspec:
  free(fspec);
err_dspec:
  free(dspec);
  return NULL;
}

int shlock_close(void *handle)
{
  struct shlck *s = handle;
  int rc, ret = 0;

  rc = unlink(s->tspec);
  if (rc == -1)
    perror("unlink()");
  /* close() drops OFD lock */
  rc = close(s->fd);
  if (rc == -1)
    perror("close()");
  rc = rmdir(s->fspec);  // fails if still not empty
  if (rc == 0)
    ret++;
  rc = rmdir(s->dir);  // fails if still not empty
  if (rc == 0)
    ret++;

  free(s->tspec);
  free(s->fspec);
  free(s->dir);
  free(s);
  return ret;
}
