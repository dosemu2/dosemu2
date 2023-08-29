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
 * Provides inter-process read/write locks and refcounting.
 * Refcounting is provided by the means of an unlock operation
 * returning an indication of "no more locks on that resource".
 *
 * This implementation provides the lock-free operations, i.e. it
 * doesn't use any locking internally. Which makes it a bit complex,
 * as we lack the atomic open-and-lock operation, unlink-from-fd
 * (funlink()/frmdir()) functions and a few other things to liverage
 * the simple lock-free implementation.
 * Also it strives to provide the race-free operations with the
 * predictable behavior in all cases, but the callers are advised
 * to use the synchronization to avoid locking and unlocking the
 * same resources simultaneously from different processes.
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
#include <glob.h>
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
  int tmp_fd;
};

static void do_gc(const char *fspec)
{
  char *pat;
  glob_t gl;
  int rc;

  asprintf(&pat, "%s/" LOCK_PFX "*", fspec);
  rc = glob(pat, GLOB_ERR | GLOB_NOSORT | GLOB_NOESCAPE, NULL, &gl);
  if (rc == 0) {
    int i;
    for (i = 0; i < gl.gl_pathc; i++) {
      int fd = open(gl.gl_pathv[i], O_RDONLY);
      if (fd == -1)
        continue;
      /* tmpfiles are not participating in locking, so we can use
       * LOCK_NB safely */
      rc = flock(fd, LOCK_SH | LOCK_NB);
      if (rc == 0)
        unlink(gl.gl_pathv[i]);
      close(fd);
    }
    globfree(&gl);
  }
  free(pat);
}

static char *fixupspec(char *fspec, const char *dir1, const char *dir2)
{
  char *ret;
  int len = strlen(dir1);
  assert(fspec[len] == '/');
  asprintf(&ret, "%s%s", dir2, fspec + len);
  return ret;
}

void *shlock_open(const char *dir, const char *name, int excl, int block)
{
  struct shlck *ret;
  char *fspec, *dspec, *tspec, *ttspec, *dtspec;
  int fd, tmp_fd, rc;
  int flg = block ? 0 : LOCK_NB;

  rc = asprintf(&dspec, LOCK_DIR "/%s", dir);
  assert(rc != -1);
  rc = asprintf(&fspec, "%s/%s", dspec, name);
  assert(rc != -1);
  rc = asprintf(&dtspec, LOCK_DIR "/%s.XXXXXX", name);
  assert(rc != -1);
  /* create tmp dir */
  mkdtemp(dtspec);
  rc = asprintf(&ttspec, "%s/" LOCK_PFX "%i_XXXXXX", dtspec, getpid());
  assert(rc != -1);

  /* create tmp file in tmp dir */
  tmp_fd = mkstemp(ttspec);
  if (tmp_fd == -1) {
    perror("mkstemp()");
    free(ttspec);
    rmdir(dtspec);
    goto err_free_d;
  }
  tspec = fixupspec(ttspec, dtspec, fspec);
  /* lock it before exposing */
  rc = flock(tmp_fd, LOCK_EX);
  if (rc == -1) {
    perror("flock(tmp)");
    free(ttspec);
    rmdir(dtspec);
    goto err_clotmp;
  }

  rc = -1;
  while (rc == -1) {
    rc = mkdir(dspec, 0775);
    if (rc == -1 && errno != EEXIST) {
      perror("mkdir()");
      unlink(ttspec);
      free(ttspec);
      rmdir(dtspec);
      goto err_clotmp;
    }
    /* Try to rename tmp dir. The idea is that fspec dir is never
     * created empty, because we rename to it from non-empty one. */
    rc = rename(dtspec, fspec);
    if (rc == -1) {
      if (errno == ENOENT)
        continue;  // race, someone removed parent dir
      /* couldn't rename the whole dir (fspec exists and not empty),
       * so try to move tmp file alone */
      rc = rename(ttspec, tspec);
      if (rc == -1) {
        if (errno == ENOENT)
          continue;  // race, someone removed parent dir
        perror("rename()");
        unlink(ttspec);
        free(ttspec);
        rmdir(dtspec);
        goto err_rmddir;
      }
      rc = rmdir(dtspec);
      if (rc == -1) {
        perror("rmdir()");
        free(ttspec);
        goto err_rmt;
      }
    }
  }

  free(ttspec);
  /* At this point our fspec directory is stable and can't disappear.
   * Try collecting stalled tmpfiles. */
  do_gc(fspec);

  fd = open(fspec, O_RDONLY | O_DIRECTORY);
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
  ret->tmp_fd = tmp_fd;
  return ret;

err_close:
  close(fd);
err_rmt:
  unlink(tspec);
  rmdir(fspec);
err_rmddir:
  rmdir(dspec);
err_clotmp:
  close(tmp_fd);
  free(tspec);
err_free_d:
  free(dtspec);
  free(fspec);
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
  rc = close(s->tmp_fd);
  if (rc == -1)
    perror("close(tmp)");
  /* At that point someone can remove and re-create our dir.
   * The creation loop is designed the way the created dir is never
   * empty, so it can't be removed here in that case.
   * Note that whoever removes our dir, returns the removal indication
   * to the caller. So if the dir doesn't exist currently (no matter
   * was it just removed, or removed then re-created then removed again,
   * and so on), then we do not return the removal indication, which
   * is fine, as that means 1 indication per 1 removal. */
  rc = rmdir(s->fspec);  // fails if still not empty
  if (rc == 0)
    ret++;
  /* close() drops OFD lock */
  rc = close(s->fd);
  if (rc == -1)
    perror("close()");
  rc = rmdir(s->dir);  // fails if still not empty
  if (rc == 0)
    ret++;

  free(s->tspec);
  free(s->fspec);
  free(s->dir);
  free(s);
  return ret;
}
