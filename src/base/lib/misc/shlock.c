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
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include "shlock.h"

#define LOCK_DIR "/tmp"
#define LOCK_PFX "LCK.."

struct shlck {
  char *fspec;
  char *dir;
  int excl;
  int fd;
};

static int fd_is_ok(int dir_fd, const char *dspec)
{
  int rc = access(dspec, R_OK);
  if (rc == 0) {
    struct stat sb1, sb2;
    fstat(dir_fd, &sb1);
    stat(dspec, &sb2);
    if (sb1.st_ino == sb2.st_ino)
      return 1;
  }
  return 0;
}

void *shlock_open(const char *dir, const char *name, int excl, int block)
{
  struct shlck *ret;
  char *fspec, *dspec;
  int fd, dir_fd, rc;
  int flg = block ? 0 : LOCK_NB;

  rc = asprintf(&dspec, LOCK_DIR "/%s", dir);
  if (rc == -1) {
    perror("asprintf()");
    return NULL;
  }
  while (1) {
    rc = mkdir(dspec, 0775);
    if (rc == -1 && errno != EEXIST) {
      perror("mkdir()");
      goto err_dspec;
    }
    dir_fd = open(dspec, O_RDONLY | O_DIRECTORY);
    if (dir_fd == -1) {
      perror("open(dir)");
      goto err_dspec;
    }
    /* get directory lock to avoid races */
    rc = flock(dir_fd, LOCK_EX);
    if (rc == -1) {
      perror("flock(dir)");
      goto err_clodir;
    }
    /* check if someone else removed dir in a process */
    if (fd_is_ok(dir_fd, dspec))
      break;
    /* race happened, retry */
    close(dir_fd);
  }

  rc = asprintf(&fspec, "%s/" LOCK_PFX "%s", dspec, name);
  if (rc == -1) {
    perror("asprintf()");
    goto err_clodir;
  }
  fd = open(fspec, O_RDWR | O_CREAT | O_CLOEXEC, 0660);
  if (fd == -1) {
    perror("open()");
    goto err_free;
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
  /* release dir lock */
  close(dir_fd);

  ret = malloc(sizeof(*ret));
  ret->fspec = fspec;
  ret->dir = dspec;
  ret->excl = excl;
  ret->fd = fd;
  return ret;

err_close:
  close(fd);
err_free:
  free(fspec);
err_clodir:
  close(dir_fd);
err_dspec:
  free(dspec);
  return NULL;
}

int shlock_close(void *handle)
{
  struct shlck *s = handle;
  int rc, ret = 0;
  int dir_fd;

  /* drop file lock first, to avoid AB-BA deadlock with dir lock */
  rc = flock(s->fd, LOCK_UN);
  if (rc == -1) {
    perror("flock(UN)");
    return -1;
  }
  dir_fd = open(s->dir, O_RDONLY | O_DIRECTORY);
  if (dir_fd == -1) {
    perror("open(dir)");
    return -1;
  }
  /* grab directory lock */
  rc = flock(dir_fd, LOCK_EX);
  if (rc == -1) {
    perror("flock(dir)");
    goto err_clodir;
  }
  /* if dir or file removed, just close fds */
  if (!fd_is_ok(dir_fd, s->dir) || !fd_is_ok(s->fd, s->fspec))
    rc = -1;
  if (rc == 0)
    rc = flock(s->fd, LOCK_EX | LOCK_NB);
  if (rc == 0) {
    DIR *d;
    struct dirent *de;
    int found = 0;

    /* we are the last owner, delete the lock file */
    unlink(s->fspec);
    ret++;

    d = fdopendir(dir_fd);
    assert(d);
    while ((de = readdir(d))) {
      if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
        found++;
        break;
      }
    }
    if (!found) {
      /* we are the last user of the dir as well */
      rmdir(s->dir);
      ret++;
    }
    /* release dir lock */
    closedir(d);
  } else {
    /* release dir lock */
    close(dir_fd);
  }
  close(s->fd);

  free(s->fspec);
  free(s->dir);
  free(s);
  return ret;

err_clodir:
  close(dir_fd);
  return -1;
}
