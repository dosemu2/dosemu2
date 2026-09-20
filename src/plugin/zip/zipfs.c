/*
 *  Copyright (C) 2026  dosemu2 project
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
/*
 * Purpose: read-only vfs backend serving a zip archive as a drive.
 *
 * zip compresses every entry on its own and keeps a central directory,
 * so unlike tar.gz it gives random access at file granularity, which is
 * what a filesystem needs. There is no random access _within_ a deflated
 * entry though, so an entry is inflated once when it is opened; stored
 * entries are read straight from the archive.
 *
 * Entry names are passed through as they are: an archive made under DOS
 * flags its names as CP437, which is already what the redirector wants,
 * and one made under Unix flags them as UTF-8, same as a host filesystem.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <zip.h>
#include "emu.h"
#include "init.h"
#include "dosemu_config.h"
#include "dosemu_debug.h"
#include "fslib/fslib.h"
#include "vfs/vfs.h"

/* DOS attribute bits, as stored by a DOS-made archive */
#define ZIP_ATTR_RDONLY 0x01
#define ZIP_ATTR_DIR    0x10

struct zip_node {
  char *name;
  struct zip_node *parent;
  struct zip_node *child;
  struct zip_node *next;
  zip_int64_t idx;      // -1 for a directory we had to synthesise
  off_t size;
  time_t mtime;
  int attr;
  int is_dir;
};

struct zipfs {
  zip_t *za;
  char *root;           // archive path, with a trailing slash
  int root_len;
  off_t arc_size;
  struct zip_node *tree;
  char *ovl;            // overlay dir for this archive, with a trailing slash
  int ovl_made;         // the dir is known to exist
};

struct zip_file {
  vfs_file_t vfile;     // must be first
  struct zipfs *zfs;
  struct zip_node *node;
  off_t pos;
  unsigned char *data;  // inflated contents, NULL when stored
  zip_file_t *zfp;      // used instead of data for stored entries
  int chunk_fd;         // the entry's chunk file, -1 until a lock needs it
};

struct zip_dir {
  vfs_dir_t vdir;       // must be first
  struct zipfs *zfs;
  struct zip_node *node;
  struct zip_node *pos;
};

/*
 * Tree
 */

static struct zip_node *node_new(struct zip_node *parent, const char *name,
    int len)
{
  struct zip_node *n = calloc(1, sizeof(*n));

  if (!n)
    return NULL;
  n->name = strndup(name, len);
  if (!n->name) {
    free(n);
    return NULL;
  }
  n->idx = -1;
  n->parent = parent;
  if (parent) {
    n->next = parent->child;
    parent->child = n;
  }
  return n;
}

static void node_free(struct zip_node *n)
{
  struct zip_node *c = n->child;

  while (c) {
    struct zip_node *next = c->next;

    node_free(c);
    c = next;
  }
  free(n->name);
  free(n);
}

/* DOS is case-insensitive, zip is not */
static struct zip_node *node_find(struct zip_node *dir, const char *name,
    int len)
{
  struct zip_node *c;

  for (c = dir->child; c; c = c->next) {
    if ((int)strlen(c->name) == len && strncasecmp(c->name, name, len) == 0)
      return c;
  }
  return NULL;
}

/*
 * Walks the slash-separated path, creating the directories it passes
 * through when asked to. Archives are not required to carry entries for
 * their directories, so most of them have to be synthesised.
 */
static struct zip_node *node_walk(struct zipfs *zfs, const char *path,
    int create)
{
  struct zip_node *n = zfs->tree;
  const char *p = path;

  while (*p) {
    const char *e;
    struct zip_node *c;

    while (*p == '/')
      p++;
    if (!*p)
      break;
    e = strchr(p, '/');
    if (!e)
      e = p + strlen(p);
    c = node_find(n, p, e - p);
    if (!c) {
      if (!create)
        return NULL;
      c = node_new(n, p, e - p);
      if (!c)
        return NULL;
      c->is_dir = 1;
      c->attr = ZIP_ATTR_DIR;
    } else if (*e && !c->is_dir) {
      /* a name used as a file by one entry and as a directory by
       * another; the archive is malformed, keep the file */
      return NULL;
    }
    n = c;
    p = e;
  }
  return n;
}

static int dos_attr(zip_t *za, zip_uint64_t idx, int is_dir)
{
  zip_uint8_t opsys;
  zip_uint32_t attr;
  int ret = is_dir ? ZIP_ATTR_DIR : 0;

  if (zip_file_get_external_attributes(za, idx, 0, &opsys, &attr) != 0)
    return ret;
  switch (opsys) {
  case ZIP_OPSYS_DOS:
  case ZIP_OPSYS_WINDOWS_NTFS:
  case ZIP_OPSYS_MVS:
    /* the low byte is the DOS attribute byte already */
    return (attr & 0xff) | ret;
  default:
    /* a unix-made archive keeps st_mode in the high half */
    if (!(attr >> 16 & 0200))
      ret |= ZIP_ATTR_RDONLY;
    return ret;
  }
}

static int build_tree(struct zipfs *zfs)
{
  zip_int64_t n = zip_get_num_entries(zfs->za, 0);
  zip_int64_t i;

  zfs->tree = node_new(NULL, "", 0);
  if (!zfs->tree)
    return -1;
  zfs->tree->is_dir = 1;
  zfs->tree->attr = ZIP_ATTR_DIR;

  for (i = 0; i < n; i++) {
    const char *name = zip_get_name(zfs->za, i, ZIP_FL_ENC_RAW);
    struct zip_stat st;
    struct zip_node *node;
    int len;

    if (!name)
      continue;
    len = strlen(name);
    if (!len)
      continue;
    node = node_walk(zfs, name, 1);
    if (!node)
      return -1;
    if (name[len - 1] == '/') {  // an explicit directory entry
      node->idx = i;
      continue;
    }
    if (node->child) {
      /* a name used both as a file and as a directory */
      error("zip: %s is both a file and a directory, keeping the latter\n",
          name);
      continue;
    }
    if (node->idx != -1) {
      error("zip: duplicate entry %s, keeping the first one\n", name);
      continue;
    }
    node->is_dir = 0;
    node->idx = i;
    if (zip_stat_index(zfs->za, i, 0, &st) == 0) {
      if (st.valid & ZIP_STAT_SIZE)
        node->size = st.size;
      if (st.valid & ZIP_STAT_MTIME)
        node->mtime = st.mtime;
    }
    node->attr = dos_attr(zfs->za, i, 0);
  }
  return 0;
}

/*
 * Paths
 */

static struct zip_node *lookup(vfs_fs_t *fs, const char *path)
{
  struct zipfs *zfs = fs->priv;
  int len = strlen(path);

  /* mfs hands us host paths, which all start with the archive itself */
  if (strncmp(path, zfs->root, zfs->root_len) != 0) {
    if (len == zfs->root_len - 1 &&
        strncmp(path, zfs->root, len) == 0)
      return zfs->tree;  // the archive with no trailing slash
    return NULL;
  }
  return node_walk(zfs, path + zfs->root_len, 0);
}

static void fill_stat(struct zipfs *zfs, struct zip_node *n, struct stat *sb)
{
  memset(sb, 0, sizeof(*sb));
  sb->st_mode = n->is_dir ? (S_IFDIR | 0555) : (S_IFREG | 0444);
  sb->st_nlink = 1;
  sb->st_size = n->size;
  sb->st_mtime = sb->st_atime = sb->st_ctime = n->mtime;
  sb->st_blksize = 512;
  sb->st_blocks = (n->size + 511) / 512;
}

/*
 * File ops
 */

/*
 * Overlay
 *
 * Every archive gets a directory of its own, and inside it every entry
 * that needs one gets a chunk file: a sparse file as long as the entry,
 * so that it shares the DOS file's offset space byte for byte.
 *
 * That is what makes locking work. A region lock can be handed straight
 * to the kernel on the chunk file's own fd, and it means exactly what it
 * says, because offset N in the chunk file is offset N in the DOS file.
 * The chunk file is also where written data will land once this backend
 * accepts writes, which is why an empty one is worth creating just to
 * have something to lock.
 *
 * The directory lives under dosemu_tmpdir, which is per user rather than
 * per session, so two dosemu2 instances on the same archive meet in the
 * same chunk files and lock against each other the way they already do
 * on a plain directory.
 */
#define OVL_SUBDIR "zipovl"

/* FNV-1a, to keep two archives with the same basename apart */
static uint32_t path_hash(const char *s, int len)
{
  uint32_t h = 2166136261U;
  int i;

  for (i = 0; i < len; i++) {
    h ^= (unsigned char)s[i];
    h *= 16777619U;
  }
  return h;
}

/*
 * Named after the archive so that the directory can be recognised by
 * eye, and hashed so that two archives of the same name elsewhere do
 * not share it. Takes the path with or without a trailing slash.
 */
static char *ovl_path_for(const char *arc)
{
  const char *base;
  char *ret;
  int len = strlen(arc);
  int blen;

  while (len && arc[len - 1] == '/')
    len--;
  for (blen = 0; blen < len && arc[len - 1 - blen] != '/'; blen++)
    ;
  base = arc + len - blen;
  if (asprintf(&ret, "%s/" OVL_SUBDIR "/%.*s.%08" PRIx32 "/", dosemu_tmpdir,
      blen, base, path_hash(arc, len)) == -1)
    return NULL;
  return ret;
}

/* both levels, and the trailing slash has to go for mkdir() */
static int ovl_make_dir(struct zipfs *zfs)
{
  char *p, *sl;
  int ret = -1;

  if (zfs->ovl_made)
    return 0;
  if (!zfs->ovl)
    return -1;
  p = strdup(zfs->ovl);
  if (!p)
    return -1;
  sl = strrchr(p, '/');
  if (!sl)
    goto out;
  *sl = '\0';
  sl = strrchr(p, '/');
  if (!sl)
    goto out;
  *sl = '\0';
  if (mkdir(p, S_IRWXU) == -1 && errno != EEXIST)
    goto out;
  *sl = '/';
  if (mkdir(p, S_IRWXU) == -1 && errno != EEXIST)
    goto out;
  zfs->ovl_made = 1;
  ret = 0;
out:
  if (ret)
    error("zip: cannot create overlay dir %s: %s\n", p, strerror(errno));
  free(p);
  return ret;
}

/*
 * The entry's index is what names the chunk file: it is unique within
 * the archive and needs no escaping, unlike the entry's path.
 */
static int ovl_chunk_fd(struct zip_file *zf)
{
  struct zipfs *zfs = zf->zfs;
  char *path;
  int fd;

  if (zf->chunk_fd != -1)
    return zf->chunk_fd;
  if (zf->node->idx < 0 || ovl_make_dir(zfs) != 0)
    return -1;
  if (asprintf(&path, "%s%08llx", zfs->ovl,
      (unsigned long long)zf->node->idx) == -1)
    return -1;
  fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    error("zip: cannot open chunk file %s: %s\n", path, strerror(errno));
    free(path);
    return -1;
  }
  free(path);
  /*
   * Sparse, so it costs no blocks until something is written. Another
   * instance may have got here first and already sized it, hence the
   * stat rather than an unconditional ftruncate.
   */
  if (zf->node->size) {
    struct stat sb;

    if (fstat(fd, &sb) == 0 && sb.st_size < zf->node->size &&
        ftruncate(fd, zf->node->size) == -1) {
      error("zip: cannot size chunk file: %s\n", strerror(errno));
      close(fd);
      return -1;
    }
  }
  zf->chunk_fd = fd;
  return fd;
}

static int zf_close(vfs_file_t *file)
{
  struct zip_file *zf = (struct zip_file *)file;

  if (zf->zfp)
    zip_fclose(zf->zfp);
  if (zf->chunk_fd != -1)
    close(zf->chunk_fd);
  free(zf->data);
  free(zf);
  return 0;
}

static ssize_t zf_read(vfs_file_t *file, void *buf, size_t count)
{
  struct zip_file *zf = (struct zip_file *)file;
  off_t left = zf->node->size - zf->pos;
  ssize_t ret;

  if (left <= 0)
    return 0;
  if ((off_t)count > left)
    count = left;
  if (zf->data) {
    memcpy(buf, zf->data + zf->pos, count);
    zf->pos += count;
    return count;
  }
  /* stored entry, read through the archive */
  if (zip_fseek(zf->zfp, zf->pos, SEEK_SET) < 0) {
    errno = EIO;
    return -1;
  }
  ret = zip_fread(zf->zfp, buf, count);
  if (ret < 0) {
    errno = EIO;
    return -1;
  }
  zf->pos += ret;
  return ret;
}

static ssize_t zf_write(vfs_file_t *file, const void *buf, size_t count)
{
  errno = EROFS;
  return -1;
}

static off_t zf_lseek(vfs_file_t *file, off_t offset, int whence)
{
  struct zip_file *zf = (struct zip_file *)file;
  off_t pos;

  switch (whence) {
  case SEEK_SET:
    pos = offset;
    break;
  case SEEK_CUR:
    pos = zf->pos + offset;
    break;
  case SEEK_END:
    pos = zf->node->size + offset;
    break;
  default:
    errno = EINVAL;
    return -1;
  }
  if (pos < 0) {
    errno = EINVAL;
    return -1;
  }
  zf->pos = pos;
  return pos;
}

static int zf_fstat(vfs_file_t *file, struct stat *sb)
{
  struct zip_file *zf = (struct zip_file *)file;

  fill_stat(zf->zfs, zf->node, sb);
  return 0;
}

static int zf_ftruncate(vfs_file_t *file, off_t length)
{
  errno = EROFS;
  return -1;
}

static int zf_fsync(vfs_file_t *file)
{
  return 0;
}

static int zf_get_dos_attr(vfs_file_t *file, int mode)
{
  struct zip_file *zf = (struct zip_file *)file;

  return zf->node->attr;
}

static int zf_set_dos_attr(vfs_file_t *file, int attr)
{
  errno = EROFS;
  return -1;
}

/*
 * Locks go to the kernel on the entry's chunk file, which shares the
 * DOS file's offset space, so a byte range means the same thing on
 * both and two instances of dosemu2 lock against each other.
 *
 * When there is no chunk file to be had - a synthesised directory, a
 * full overlay dir - nothing can be locked against either, so grant the
 * lock and report the region free rather than fail the DOS call. That
 * is what this backend did for every file before it had chunk files.
 */
static int zf_flock(vfs_file_t *file, int op)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd = ovl_chunk_fd(zf);

  if (fd == -1)
    return 0;
  return flock(fd, op);
}

static int zf_setlk(vfs_file_t *file, struct flock *fl)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd = ovl_chunk_fd(zf);

  if (fd == -1)
    return 0;
  return fcntl(fd, F_OFD_SETLK, fl);
}

static int zf_getlk(vfs_file_t *file, struct flock *fl)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd = ovl_chunk_fd(zf);

  if (fd == -1) {
    fl->l_type = F_UNLCK;
    return 0;
  }
  return fcntl(fd, F_OFD_GETLK, fl);
}

static const struct vfs_file_ops zip_file_ops = {
  .close = zf_close,
  .read = zf_read,
  .write = zf_write,
  .lseek = zf_lseek,
  .fstat = zf_fstat,
  .ftruncate = zf_ftruncate,
  .fsync = zf_fsync,
  .flock = zf_flock,
  .setlk = zf_setlk,
  .getlk = zf_getlk,
  .get_dos_attr = zf_get_dos_attr,
  .set_dos_attr = zf_set_dos_attr,
};

/*
 * Dir ops
 */

static int zd_closedir(vfs_dir_t *dir)
{
  free(dir);
  return 0;
}

static int zd_readdir(vfs_dir_t *dir, struct vfs_dirent *de)
{
  struct zip_dir *zd = (struct zip_dir *)dir;

  if (!zd->pos)
    return -1;
  de->d_name = de->d_long_name = zd->pos->name;
  zd->pos = zd->pos->next;
  return 0;
}

static int zd_fstatdir(vfs_dir_t *dir, struct stat *sb)
{
  struct zip_dir *zd = (struct zip_dir *)dir;

  fill_stat(zd->zfs, zd->node, sb);
  return 0;
}

static const struct vfs_dir_ops zip_dir_ops = {
  .closedir = zd_closedir,
  .readdir = zd_readdir,
  .fstatdir = zd_fstatdir,
};

/*
 * Fs ops
 */

static vfs_file_t *zip_fs_open(vfs_fs_t *fs, const char *path, int flags)
{
  struct zipfs *zfs = fs->priv;
  struct zip_node *n = lookup(fs, path);
  struct zip_file *zf;
  struct zip_stat st;

  if (!n) {
    errno = ENOENT;
    return NULL;
  }
  if (n->is_dir) {
    errno = EISDIR;
    return NULL;
  }
  if ((flags & O_ACCMODE) != O_RDONLY) {
    errno = EROFS;
    return NULL;
  }
  zf = calloc(1, sizeof(*zf));
  if (!zf)
    return NULL;
  zf->vfile.ops = &zip_file_ops;
  zf->vfile.fd = -1;
  zf->zfs = zfs;
  zf->node = n;
  zf->chunk_fd = -1;

  if (zip_stat_index(zfs->za, n->idx, 0, &st) == 0 &&
      (st.valid & ZIP_STAT_COMP_METHOD) && st.comp_method == ZIP_CM_STORE) {
    /* stored, so the archive itself is seekable for us */
    zf->zfp = zip_fopen_index(zfs->za, n->idx, 0);
    if (zf->zfp)
      return &zf->vfile;
  }

  /*
   * Deflate has no random access, so inflate the whole entry now and
   * serve reads from it. This is the point where a chunked backing
   * store would plug in instead.
   */
  zf->data = malloc(n->size ? n->size : 1);
  if (!zf->data) {
    free(zf);
    return NULL;
  }
  if (n->size) {
    zip_file_t *zfp = zip_fopen_index(zfs->za, n->idx, 0);
    zip_int64_t rd;

    if (!zfp) {
      free(zf->data);
      free(zf);
      errno = EIO;
      return NULL;
    }
    rd = zip_fread(zfp, zf->data, n->size);
    zip_fclose(zfp);
    if (rd != (zip_int64_t)n->size) {
      error("zip: short read of %s\n", path);
      free(zf->data);
      free(zf);
      errno = EIO;
      return NULL;
    }
  }
  return &zf->vfile;
}

static vfs_file_t *zip_fs_creat(vfs_fs_t *fs, const char *path, int flags,
    mode_t mode)
{
  errno = EROFS;
  return NULL;
}

static int zip_fs_ro(void)
{
  errno = EROFS;
  return -1;
}

static int zip_fs_unlink(vfs_fs_t *fs, const char *path)
{
  return zip_fs_ro();
}

static int zip_fs_mkdir(vfs_fs_t *fs, const char *path, mode_t mode)
{
  return zip_fs_ro();
}

static int zip_fs_rmdir(vfs_fs_t *fs, const char *path)
{
  return zip_fs_ro();
}

static int zip_fs_rename(vfs_fs_t *fs, const char *oldpath, const char *newpath)
{
  return zip_fs_ro();
}

static int zip_fs_utime(vfs_fs_t *fs, const char *path, time_t atime,
    time_t mtime)
{
  return zip_fs_ro();
}

static int zip_fs_setxattr(vfs_fs_t *fs, const char *path, int attr)
{
  return zip_fs_ro();
}

static int zip_fs_set_dos_attr(vfs_fs_t *fs, const char *path, int attr)
{
  return zip_fs_ro();
}

static int zip_fs_getxattr(vfs_fs_t *fs, const char *path)
{
  errno = ENOSYS;
  return -1;
}

static int zip_fs_get_dos_attr(vfs_fs_t *fs, const char *path, int mode)
{
  struct zip_node *n = lookup(fs, path);

  if (!n) {
    errno = ENOENT;
    return -1;
  }
  return n->attr;
}

static int zip_fs_stat(vfs_fs_t *fs, const char *path, struct stat *sb)
{
  struct zip_node *n = lookup(fs, path);

  if (!n) {
    errno = ENOENT;
    return -1;
  }
  fill_stat(fs->priv, n, sb);
  return 0;
}

static int zip_fs_access(vfs_fs_t *fs, const char *path, int mode)
{
  if (!lookup(fs, path)) {
    errno = ENOENT;
    return -1;
  }
  if (mode & W_OK) {
    errno = EROFS;
    return -1;
  }
  return 0;
}

static int zip_fs_statvfs(vfs_fs_t *fs, const char *path, struct statvfs *sb)
{
  struct zipfs *zfs = fs->priv;

  memset(sb, 0, sizeof(*sb));
  sb->f_bsize = sb->f_frsize = 512;
  sb->f_blocks = (zfs->arc_size + 511) / 512;
  sb->f_flag = ST_RDONLY;
  sb->f_namemax = 255;
  return 0;
}

static vfs_dir_t *zip_fs_opendir(vfs_fs_t *fs, const char *path)
{
  struct zip_node *n = lookup(fs, path);
  struct zip_dir *zd;

  if (!n) {
    errno = ENOENT;
    return NULL;
  }
  if (!n->is_dir) {
    errno = ENOTDIR;
    return NULL;
  }
  zd = calloc(1, sizeof(*zd));
  if (!zd)
    return NULL;
  zd->vdir.ops = &zip_dir_ops;
  zd->vdir.fd = -1;
  zd->vdir.has_sfn = 0;
  zd->zfs = fs->priv;
  zd->node = n;
  zd->pos = n->child;
  return &zd->vdir;
}

static void *zip_fs_open_async(vfs_fs_t *fs, const char *path, int flags)
{
  /* nothing to defer: opening is local and cheap */
  return zip_fs_open(fs, path, flags);
}

static vfs_file_t *zip_fs_async_getfile(vfs_fs_t *fs, void *handle)
{
  return handle;
}

static void zip_fs_async_cancel(vfs_fs_t *fs, void *handle)
{
  if (handle)
    vfs_close(handle);
}

static const struct vfs_fs_ops zip_fs_ops = {
  .open = zip_fs_open,
  .creat = zip_fs_creat,
  .unlink = zip_fs_unlink,
  .getxattr = zip_fs_getxattr,
  .setxattr = zip_fs_setxattr,
  .statvfs = zip_fs_statvfs,
  .mkdir = zip_fs_mkdir,
  .rmdir = zip_fs_rmdir,
  .stat = zip_fs_stat,
  .rename = zip_fs_rename,
  .access = zip_fs_access,
  .utime = zip_fs_utime,
  .open_async = zip_fs_open_async,
  .async_getfile = zip_fs_async_getfile,
  .async_cancel = zip_fs_async_cancel,
  .opendir = zip_fs_opendir,
  .get_dos_attr = zip_fs_get_dos_attr,
  .set_dos_attr = zip_fs_set_dos_attr,
};

/*
 * Backend
 */

static int zip_probe(const char *path)
{
  int len = strlen(path);

  while (len && path[len - 1] == '/')
    len--;
  return len > 4 && strncasecmp(path + len - 4, ".zip", 4) == 0;
}

static int zip_mount(vfs_fs_t *fs, const char *path)
{
  struct zipfs *zfs;
  struct stat sb;
  int err = 0;
  int fd;
  int len;

  /* the archive is opened through fslib, so it stays inside the sandbox */
  fd = mfs_open_file(fs->mfs_idx, path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    error("zip: cannot open %s: %s\n", path, strerror(errno));
    return -1;
  }
  zfs = calloc(1, sizeof(*zfs));
  if (!zfs) {
    close(fd);
    return -1;
  }
  if (fstat(fd, &sb) == 0)
    zfs->arc_size = sb.st_size;
  /* libzip owns the fd from here on, also on failure */
  zfs->za = zip_fdopen(fd, ZIP_RDONLY, &err);
  if (!zfs->za) {
    zip_error_t ze;

    zip_error_init_with_code(&ze, err);
    error("zip: %s: %s\n", path, zip_error_strerror(&ze));
    zip_error_fini(&ze);
    close(fd);
    free(zfs);
    return -1;
  }

  len = strlen(path);
  while (len && path[len - 1] == '/')
    len--;
  zfs->root = malloc(len + 2);
  if (!zfs->root) {
    zip_close(zfs->za);
    free(zfs);
    return -1;
  }
  memcpy(zfs->root, path, len);
  zfs->root[len] = '/';
  zfs->root[len + 1] = '\0';
  zfs->root_len = len + 1;
  /* the dir itself is made on demand, so an archive nobody locks or
   * writes to leaves nothing behind */
  zfs->ovl = ovl_path_for(zfs->root);

  if (build_tree(zfs) != 0) {
    error("zip: cannot index %s\n", path);
    if (zfs->tree)
      node_free(zfs->tree);
    zip_close(zfs->za);
    free(zfs->ovl);
    free(zfs->root);
    free(zfs);
    return -1;
  }

  fs->ops = &zip_fs_ops;
  fs->priv = zfs;
  return 0;
}

static void zip_umount(vfs_fs_t *fs)
{
  struct zipfs *zfs = fs->priv;

  if (!zfs)
    return;
  if (zfs->tree)
    node_free(zfs->tree);
  zip_close(zfs->za);
  free(zfs->ovl);
  free(zfs->root);
  free(zfs);
  fs->priv = NULL;
}

static const struct vfs_backend zip_backend = {
  .name = "zip",
  .probe = zip_probe,
  .mount = zip_mount,
  .umount = zip_umount,
};

CONSTRUCTOR(static void zipfs_init(void))
{
  vfs_register_backend(&zip_backend);
}
