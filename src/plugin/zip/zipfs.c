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
  int ovl_id;           // -1 unless the entry only exists in the overlay
  off_t size;           // as DOS sees it, overlay included
  off_t arc_size;       // as the archive has it, 0 for an overlay entry
  time_t mtime;
  int attr;
  int is_dir;
  int refs;             // open handles pointing here
  int gone;             // unlinked, and only the handles keep it alive
};

struct zipfs {
  zip_t *za;
  char *root;           // archive path, with a trailing slash
  int root_len;
  off_t arc_size;
  uint64_t arc_id;      // what the archive's entries hash to
  struct zip_node *tree;
  char *ovl;            // overlay dir for this archive, with a trailing slash
  int ovl_made;         // the dir is known to exist
  int id_fd;            // the stamp, held shared while this mount lives
  int log_fd;           // the directory log, -1 until there is one
  int next_id;          // the next overlay id to hand out
};

/* a half-open range of the entry that the chunk file owns */
struct extent {
  off_t off;
  off_t end;
};

struct ext_map {
  struct extent *v;
  int n;
  int cap;
  int fd;               // the map file, -1 until there is one
  off_t consumed;       // how much of it is already in v[]
  off_t size;           // the entry's length now
  off_t arc_valid;      // how much of the archive's copy still counts
};

struct zip_file {
  vfs_file_t vfile;     // must be first
  struct zipfs *zfs;
  struct zip_node *node;
  off_t pos;
  unsigned char *data;  // inflated contents, NULL when stored
  zip_file_t *zfp;      // used instead of data for stored entries
  int chunk_fd;         // the entry's chunk file, -1 until a lock needs it
  struct ext_map map;   // what of the entry the chunk file owns
  int writable;         // opened for writing
};

/*
 * A directory is copied out when it is opened rather than walked as it
 * is read: DOS keeps a search open across calls, and whatever it does
 * in between - deleting the entry it just found, most of all - must not
 * be able to pull the tree out from under the search.
 */
struct zip_dir {
  vfs_dir_t vdir;       // must be first
  struct stat sb;       // the directory itself, as it was
  char **names;
  int n;
  int pos;
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
  n->ovl_id = -1;
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
        node->size = node->arc_size = st.size;
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

/* the name is wanted relative to the archive, as the log stores it */
static const char *rel_path(vfs_fs_t *fs, const char *path)
{
  struct zipfs *zfs = fs->priv;

  if (strncmp(path, zfs->root, zfs->root_len) != 0)
    return NULL;
  return path + zfs->root_len;
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

/*
 * What the overlay belongs to.
 *
 * A chunk file is named by the entry's index in the archive, and the
 * overlay directory is named after the archive's path. Neither says
 * anything about the archive's contents, so if the file at that path is
 * replaced - rebuilt, swapped for another release - an overlay written
 * for the old one would be laid over entries it knows nothing about:
 * index 1 is still index 1, but it is a different file now, and DOS
 * would read the old overlay's bytes spliced over the new entry's.
 *
 * So the archive is fingerprinted over what identifies its entries, and
 * the fingerprint is kept beside the overlay. Names, sizes and CRCs are
 * what the central directory already has, so this costs one pass over
 * entries that are about to be walked anyway. A rebuild that produces
 * the same entries hashes the same, which is what we want: the overlay
 * is still about the same files.
 */
static uint64_t arc_fingerprint(zip_t *za)
{
  uint64_t h = 14695981039346656037ULL;
  zip_int64_t n = zip_get_num_entries(za, 0);
  zip_int64_t i;

  for (i = 0; i < n; i++) {
    struct zip_stat st;
    const char *name;
    int j;

    if (zip_stat_index(za, i, 0, &st) != 0)
      continue;
    name = st.valid & ZIP_STAT_NAME ? st.name : "";
    for (j = 0; name[j]; j++) {
      h ^= (unsigned char)name[j];
      h *= 1099511628211ULL;
    }
    h ^= (uint64_t)st.size;
    h *= 1099511628211ULL;
    h ^= (uint64_t)st.crc;
    h *= 1099511628211ULL;
  }
  h ^= (uint64_t)n;
  h *= 1099511628211ULL;
  return h;
}

/* the fingerprint file beside the chunks */
static char *ovl_id_name(struct zipfs *zfs)
{
  char *ret;

  if (!zfs->ovl || asprintf(&ret, "%sid", zfs->ovl) == -1)
    return NULL;
  return ret;
}

/*
 * Whether an overlay that is already there was written for this
 * archive. An overlay from a different one is not ours to read and not
 * ours to throw away either - it is the only copy of what was written
 * through it - so the mount fails and says where it is.
 */
static int ovl_id_ok(struct zipfs *zfs)
{
  char buf[32];
  char *path = ovl_id_name(zfs);
  uint64_t had;
  int fd, n, ret = 1;

  if (!path)
    return 1;
  fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1)
    goto out;
  n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0)
    goto out;
  buf[n] = '\0';
  had = strtoull(buf, NULL, 16);
  if (had == zfs->arc_id)
    goto out;
  error("zip: %s holds an overlay for a different archive; "
      "move it away to use this one\n", zfs->ovl);
  ret = 0;
out:
  free(path);
  return ret;
}

/* stamped when the directory is made, so a later mount can tell */
static void ovl_id_write(struct zipfs *zfs)
{
  char *path = ovl_id_name(zfs);
  char buf[32];
  int fd, len;

  if (!path)
    return;
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    free(path);
    return;
  }
  len = snprintf(buf, sizeof(buf), "%016" PRIx64 "\n", zfs->arc_id);
  if (write(fd, buf, len) != len)
    error("zip: cannot stamp %s: %s\n", path, strerror(errno));
  close(fd);
  free(path);
}

/*
 * The stamp doubles as the overlay's presence lock: every mount holds it
 * shared for as long as it lives, so a mount that can take it exclusive
 * knows it is the only one left and may tidy up behind everybody.
 */
static void ovl_id_hold(struct zipfs *zfs)
{
  char *path;

  if (zfs->id_fd != -1)
    return;
  path = ovl_id_name(zfs);
  if (!path)
    return;
  zfs->id_fd = open(path, O_RDONLY | O_CLOEXEC);
  if (zfs->id_fd != -1 && flock(zfs->id_fd, LOCK_SH | LOCK_NB) == -1) {
    /* somebody has it exclusive, i.e. is tidying up right now; without
     * the shared lock we simply do not tidy up ourselves later */
    close(zfs->id_fd);
    zfs->id_fd = -1;
  }
  free(path);
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
  ovl_id_write(zfs);
  ovl_id_hold(zfs);
  ret = 0;
out:
  if (ret)
    error("zip: cannot create overlay dir %s: %s\n", p, strerror(errno));
  free(p);
  return ret;
}

/*
 * Extent map.
 *
 * The chunk file is sparse, so a hole in it reads as zeros and is
 * indistinguishable from zeros that DOS actually wrote. SEEK_DATA does
 * not settle it either: it is allowed to report a written range as a
 * hole, which is the dangerous direction here - we would serve the
 * archive's bytes over data DOS had written. So what the overlay owns
 * is recorded explicitly, in a file beside the chunk file.
 *
 * The record is a pair of 64 bit little endian numbers, offset and
 * length, appended and never rewritten. The data goes to the chunk
 * file before its record goes to the map, so a crash in between loses
 * the write and falls back to the archive, rather than serving a range
 * the chunk file never received. A torn record at the tail is dropped
 * on load for the same reason.
 */
#define EXT_REC_LEN 16
/* in a length, says the record is a truncation to its offset instead */
#define EXT_TRUNC (1ULL << 63)

static void put64(unsigned char *p, uint64_t v)
{
  int i;

  for (i = 0; i < 8; i++)
    p[i] = (v >> (i * 8)) & 0xff;
}

static uint64_t get64(const unsigned char *p)
{
  uint64_t v = 0;
  int i;

  for (i = 0; i < 8; i++)
    v |= (uint64_t)p[i] << (i * 8);
  return v;
}

/* insert [off, end), merging into whatever it touches */
static int map_add(struct ext_map *m, off_t off, off_t end)
{
  int i, j;

  if (end <= off)
    return 0;
  /* swallow every extent that the new one reaches */
  for (i = 0; i < m->n; i++) {
    if (m->v[i].end < off)
      continue;
    if (m->v[i].off > end)
      break;
    if (m->v[i].off < off)
      off = m->v[i].off;
    if (m->v[i].end > end)
      end = m->v[i].end;
    for (j = i; j < m->n; j++) {
      if (m->v[j].off > end)
        break;
      if (m->v[j].end > end)
        end = m->v[j].end;
    }
    memmove(&m->v[i + 1], &m->v[j], (m->n - j) * sizeof(m->v[0]));
    m->n = i + 1 + (m->n - j);
    m->v[i].off = off;
    m->v[i].end = end;
    return 0;
  }
  if (m->n == m->cap) {
    int cap = m->cap ? m->cap * 2 : 8;
    struct extent *v = realloc(m->v, cap * sizeof(*v));

    if (!v)
      return -1;
    m->v = v;
    m->cap = cap;
  }
  memmove(&m->v[i + 1], &m->v[i], (m->n - i) * sizeof(m->v[0]));
  m->v[i].off = off;
  m->v[i].end = end;
  m->n++;
  return 0;
}

/*
 * Cut the entry to len. What the overlay owns above that is dropped,
 * and so is the archive's copy above it, permanently: growing the
 * entry again must not bring the old bytes back, it must read as
 * zeros, which is what DOS gets when it seeks past the end and writes.
 */
static void map_truncate(struct ext_map *m, off_t len)
{
  int i;

  for (i = 0; i < m->n; i++) {
    if (m->v[i].off >= len)
      break;
    if (m->v[i].end > len)
      m->v[i].end = len;
  }
  m->n = i;
  if (len < m->arc_valid)
    m->arc_valid = len;
  m->size = len;
}

/* take in whatever has been appended since the last look, ours or not */
static void map_load(struct ext_map *m)
{
  unsigned char rec[EXT_REC_LEN];
  struct stat sb;
  off_t whole;

  if (m->fd == -1 || fstat(m->fd, &sb) == -1)
    return;
  whole = sb.st_size - sb.st_size % EXT_REC_LEN;
  while (m->consumed < whole) {
    if (pread(m->fd, rec, sizeof(rec), m->consumed) != sizeof(rec))
      return;
    uint64_t raw = get64(rec + 8);
    off_t off = get64(rec);
    off_t len = raw & ~EXT_TRUNC;

    m->consumed += sizeof(rec);
    /* the file is ours, but a truncated or scribbled record must not
     * be able to make us read outside the entry */
    if (off < 0)
      continue;
    if (raw & EXT_TRUNC) {
      map_truncate(m, off);
      continue;
    }
    if (len <= 0 || off + len < off)
      continue;
    map_add(m, off, off + len);
    if (off + len > m->size)
      m->size = off + len;
  }
}

/*
 * The record goes out only after the data is in the chunk file, so
 * that a crash between the two loses the write rather than pointing
 * the map at a range the chunk file never received.
 */
static int map_write_rec(struct ext_map *m, off_t off, uint64_t len)
{
  unsigned char rec[EXT_REC_LEN];
  int ret = -1;

  if (m->fd == -1)
    return -1;
  put64(rec, off);
  put64(rec + 8, len);
  /*
   * Take in whatever is there before adding to it. The file is opened
   * O_APPEND and shared, so anything another writer added since the
   * last look sits in front of our record; counting our own record
   * past it would leave theirs unread for good, and we would go on
   * serving the archive's bytes over a range they had written. The
   * lock is on the map file, which carries no region locks - those go
   * on the chunk file - so it serialises nothing but this.
   */
  if (flock(m->fd, LOCK_EX) == -1)
    return -1;
  map_load(m);
  if (write(m->fd, rec, sizeof(rec)) == sizeof(rec)) {
    m->consumed += sizeof(rec);
    ret = 0;
  }
  flock(m->fd, LOCK_UN);
  return ret;
}

/* is [off, end) already the overlay's, whole and in one piece? */
static int map_covers(const struct ext_map *m, off_t off, off_t end)
{
  int i;

  for (i = 0; i < m->n; i++) {
    if (m->v[i].end < end)
      continue;
    return m->v[i].off <= off;
  }
  return 0;
}

/*
 * A record says the overlay owns a range, nothing more: the data
 * itself is already in the chunk file. So a write into a range the
 * map has been told about needs no second record, and DOS programs
 * rewrite the same record of the same file over and over. Without
 * this the map file grows by 16 bytes per write forever, while the
 * entry it describes never changes shape.
 */
static int map_append(struct ext_map *m, off_t off, off_t len)
{
  if (!map_covers(m, off, off + len) &&
      map_write_rec(m, off, len) != 0)
    return -1;
  if (off + len > m->size)
    m->size = off + len;
  return map_add(m, off, off + len);
}

static int map_append_trunc(struct ext_map *m, off_t len)
{
  if (map_write_rec(m, len, EXT_TRUNC) != 0)
    return -1;
  map_truncate(m, len);
  return 0;
}

/*
 * Directory log.
 *
 * The archive says what entries there are; this says what has changed
 * since. One record per change, appended and never rewritten, replayed
 * over the tree when the archive is mounted:
 *
 *   op        '+' a file that is not in the archive, 'd' a directory,
 *             '-' a name that is gone, 'r' a name that moved,
 *             'a' a DOS attribute, 't' a modification time
 *   id        which chunk file holds it, for '+' and 'd'; the value
 *             itself for 'a' and 't'
 *   name      the entry's path inside the archive, not terminated.
 *             'r' carries the old path, a NUL, then the new one
 *
 * Replaying in order is what makes it work: creating, deleting and
 * creating again is three records and ends up right, and no record
 * ever has to be found and edited. A record that does not parse ends
 * the replay, since anything after it has lost its place.
 *
 * The replay happens at mount and nowhere else, on purpose. The extent
 * maps are read again whenever they are looked at, so two instances do
 * see each other's writes to entries that already exist; names are the
 * exception, and a create or a delete by one is not seen by the other
 * until it mounts the archive afresh. Reading the log as we go would
 * mean the tree changing under an operation that has already taken a
 * node out of it - a delete arriving between the lookup in
 * zip_fs_unlink() and its own log record would free that node under
 * the caller - so it needs the node's lifetime rethought first, and
 * that is more than this wants to be.
 */
#define LOG_NAME "dir.log"
#define LOG_HDR_LEN 8
#define LOG_MAX_NAME 4096
/*
 * A modification time goes in as seconds since 1980, which is where
 * DOS timestamps start. They end in 2107, and that whole span fits the
 * record's 32 bits, which a time_t stops doing in 2038.
 */
#define LOG_EPOCH 315532800
#define LOG_MTIME_MAX 0xffffffffLL

static int log_open(struct zipfs *zfs, int create)
{
  char *path;
  int flags = O_RDWR | O_CLOEXEC | O_APPEND | (create ? O_CREAT : 0);

  if (zfs->log_fd != -1)
    return zfs->log_fd;
  if (create && ovl_make_dir(zfs) != 0)
    return -1;
  if (asprintf(&path, "%s" LOG_NAME, zfs->ovl) == -1)
    return -1;
  zfs->log_fd = open(path, flags, S_IRUSR | S_IWUSR);
  if (zfs->log_fd == -1 && (create || errno != ENOENT))
    error("zip: cannot open %s: %s\n", path, strerror(errno));
  free(path);
  return zfs->log_fd;
}

/* name2 is only for 'r', and goes after the first name and a NUL */
static int log_append(struct zipfs *zfs, int op, unsigned id,
    const char *name, const char *name2)
{
  unsigned char *rec;
  int len1 = strlen(name);
  int len = len1 + (name2 ? 1 + strlen(name2) : 0);
  int ret;

  if (len > LOG_MAX_NAME) {
    errno = ENAMETOOLONG;
    return -1;
  }
  if (log_open(zfs, 1) == -1)
    return -1;
  rec = malloc(LOG_HDR_LEN + len);
  if (!rec)
    return -1;
  rec[0] = op;
  rec[1] = 0;
  rec[2] = len & 0xff;
  rec[3] = (len >> 8) & 0xff;
  rec[4] = id & 0xff;
  rec[5] = (id >> 8) & 0xff;
  rec[6] = (id >> 16) & 0xff;
  rec[7] = (id >> 24) & 0xff;
  memcpy(rec + LOG_HDR_LEN, name, len1);
  if (name2) {
    rec[LOG_HDR_LEN + len1] = '\0';
    memcpy(rec + LOG_HDR_LEN + len1 + 1, name2, len - len1 - 1);
  }
  /*
   * A full file system does not refuse the write, it takes what fits:
   * a record of a full-length name comes back 4088 bytes of 4104. The
   * record is lost either way and the caller is told so, but the part
   * that did land would sit in front of everything appended later, and
   * log_replay() stops at the first record it cannot read whole. So
   * leaving it there loses the entire rest of the log at the next
   * mount, not just this record. Put the file back the length it had.
   * The lock is what makes that safe to do: another mount appending
   * between our write and the truncate would lose its record instead.
   */
  if (flock(zfs->log_fd, LOCK_EX) == -1) {
    free(rec);
    return -1;
  }
  ret = write(zfs->log_fd, rec, LOG_HDR_LEN + len);
  if (ret > 0 && ret != LOG_HDR_LEN + len) {
    off_t end = lseek(zfs->log_fd, 0, SEEK_CUR);

    /* O_APPEND leaves the offset just past what went in */
    if (end >= ret && ftruncate(zfs->log_fd, end - ret) != 0)
      error("zip: cannot undo a short log write: %s\n", strerror(errno));
  }
  flock(zfs->log_fd, LOCK_UN);
  free(rec);
  return ret == LOG_HDR_LEN + len ? 0 : -1;
}

/* drop a node from its parent's list, leaving the node itself whole */
static void node_detach(struct zip_node *n)
{
  struct zip_node **pp;

  if (!n->parent)
    return;
  for (pp = &n->parent->child; *pp; pp = &(*pp)->next) {
    if (*pp == n) {
      *pp = n->next;
      break;
    }
  }
  n->next = NULL;
}

/* removes what a node left in the overlay; defined below chunk_name */
static void ovl_drop(struct zipfs *zfs, struct zip_node *n);

/*
 * Drop a node from the tree. DOS lets a program delete a file it has
 * open, and goes on using the handle afterwards, so a node that still
 * has handles on it is only cut loose here and freed by the last one.
 * Whatever it kept in the overlay goes with it, but not before: an open
 * handle still writes to the chunk file, which is what DOS expects.
 */
static void node_unlink(struct zipfs *zfs, struct zip_node *n)
{
  if (!n->parent)
    return;
  node_detach(n);
  n->parent = NULL;
  n->gone = 1;
  if (!n->refs) {
    ovl_drop(zfs, n);
    node_free(n);
  }
}

static void node_put(struct zipfs *zfs, struct zip_node *n)
{
  if (--n->refs == 0 && n->gone) {
    ovl_drop(zfs, n);
    node_free(n);
  }
}

/* the path a node has now, which is what the log records it under */
static char *node_path(struct zip_node *n)
{
  struct zip_node *p;
  char *ret, *q;
  int len = 0;

  for (p = n; p && p->parent; p = p->parent)
    len += strlen(p->name) + 1;
  if (!len)
    return NULL;
  ret = malloc(len);
  if (!ret)
    return NULL;
  q = ret + len;
  *--q = '\0';
  for (p = n; p && p->parent; p = p->parent) {
    int l = strlen(p->name);

    q -= l;
    memcpy(q, p->name, l);
    if (q != ret)
      *--q = '/';
  }
  return ret;
}

/*
 * Moves a node, and with it its whole subtree, to another name. What
 * the overlay holds for an entry is named by the entry's index in the
 * archive or by the id the log gave it, and the path takes no part in
 * either, so a rename never has to touch the data.
 */
static void node_rename(struct zipfs *zfs, const char *from, const char *to)
{
  struct zip_node *n = node_walk(zfs, from, 0);
  struct zip_node *dir, *old, *up;
  const char *base = strrchr(to, '/');
  char *name, *prefix;

  if (!n || !n->parent)
    return;
  prefix = strndup(to, base ? base - to : 0);
  if (!prefix)
    return;
  if (base)
    base++;
  else
    base = to;
  dir = *base ? node_walk(zfs, prefix, 1) : NULL;
  free(prefix);
  if (!dir || !dir->is_dir)
    return;
  /* a directory can not be moved inside itself */
  for (up = dir; up; up = up->parent) {
    if (up == n)
      return;
  }
  name = strdup(base);
  if (!name)
    return;
  old = node_find(dir, base, strlen(base));
  if (old && old != n)
    node_unlink(zfs, old);
  node_detach(n);
  free(n->name);
  n->name = name;
  n->parent = dir;
  n->next = dir->child;
  dir->child = n;
}

/*
 * A DOS attribute is one byte, and whether an entry is a directory is
 * not DOS's to change, so that bit comes from the tree either way.
 */
static int attr_for(struct zip_node *n, unsigned attr)
{
  return (attr & 0xff & ~ZIP_ATTR_DIR) | (n->is_dir ? ZIP_ATTR_DIR : 0);
}

static void log_apply(struct zipfs *zfs, int op, unsigned id,
    const char *name)
{
  struct zip_node *n;

  if (op == '-' || op == 'a' || op == 't') {
    n = node_walk(zfs, name, 0);
    if (!n)
      return;
    if (op == 'a') {
      n->attr = attr_for(n, id);
      return;
    }
    if (op == 't') {
      n->mtime = LOG_EPOCH + (time_t)id;
      return;
    }
    if (n->parent)
      node_unlink(zfs, n);
    return;
  }
  /* only the ops that make an entry take an id out of the sequence */
  if ((int)id >= zfs->next_id)
    zfs->next_id = id + 1;
  /* the parents are made as directories of the archive would be */
  n = node_walk(zfs, name, 1);
  if (!n)
    return;
  n->ovl_id = id;
  n->idx = -1;
  n->size = n->arc_size = 0;
  n->mtime = time(NULL);
  if (op == 'd') {
    n->is_dir = 1;
    n->attr = ZIP_ATTR_DIR;
  } else {
    n->is_dir = 0;
    n->attr = 0;
  }
}

static void log_replay(struct zipfs *zfs)
{
  unsigned char hdr[LOG_HDR_LEN];
  char name[LOG_MAX_NAME + 1];
  off_t pos = 0;

  if (log_open(zfs, 0) == -1)
    return;
  while (pread(zfs->log_fd, hdr, sizeof(hdr), pos) == sizeof(hdr)) {
    int len = hdr[2] | (hdr[3] << 8);
    unsigned id = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) |
        ((unsigned)hdr[7] << 24);

    if (len < 1 || len > LOG_MAX_NAME)
      break;
    if (!hdr[0] || !strchr("+-drat", hdr[0]) || hdr[1])
      break;
    pos += sizeof(hdr);
    if (pread(zfs->log_fd, name, len, pos) != len)
      break;
    pos += len;
    name[len] = '\0';
    if (hdr[0] == 'r') {
      char *sep = memchr(name, '\0', len);

      /* both names have to be there for the record to mean anything */
      if (!sep || sep == name || sep == name + len - 1)
        break;
      node_rename(zfs, name, sep + 1);
      continue;
    }
    log_apply(zfs, hdr[0], id, name);
  }
}

/*
 * An entry of the archive is named by its index, which is unique there
 * and needs no escaping; one that only exists in the overlay by the id
 * the directory log gave it, kept apart by the prefix.
 */
static int chunk_name(struct zipfs *zfs, struct zip_node *n, const char *suff,
    char **ret)
{
  if (n->ovl_id >= 0)
    return asprintf(ret, "%sn%08x%s", zfs->ovl, n->ovl_id, suff) == -1 ?
        -1 : 0;
  return asprintf(ret, "%s%08llx%s", zfs->ovl, (unsigned long long)n->idx,
      suff) == -1 ? -1 : 0;
}

/* one file of a node's overlay, gone if it was ever there */
static void ovl_drop_one(struct zipfs *zfs, struct zip_node *n,
    const char *suff)
{
  char *path;

  if (chunk_name(zfs, n, suff, &path) != 0)
    return;
  if (unlink(path) != 0 && errno != ENOENT)
    error("zip: cannot remove %s: %s\n", path, strerror(errno));
  free(path);
}

/*
 * What a node leaves behind in the overlay once it is really gone. A
 * deleted directory takes its subtree with it, the same way node_free()
 * does, or the chunk files below it would never be reached again: the
 * log records the directory, not what was under it.
 */
static void ovl_drop(struct zipfs *zfs, struct zip_node *n)
{
  struct zip_node *c;

  for (c = n->child; c; c = c->next)
    ovl_drop(zfs, c);
  if (n->is_dir || (n->idx < 0 && n->ovl_id < 0))
    return;
  ovl_drop_one(zfs, n, "");
  ovl_drop_one(zfs, n, ".map");
}

/*
 * The entry's index is what names the chunk file: it is unique within
 * the archive and needs no escaping, unlike the entry's path. The map
 * file sits beside it under the same name.
 *
 * With create clear this only adopts a chunk file that is already
 * there, which is what reading wants: a file nobody has locked or
 * written has no overlay, and reading it should not make one.
 */
static int ovl_chunk_fd(struct zip_file *zf, int create)
{
  struct zipfs *zfs = zf->zfs;
  char *path;
  int flags = O_RDWR | O_CLOEXEC | (create ? O_CREAT : 0);
  int fd;

  if (zf->chunk_fd != -1)
    return zf->chunk_fd;
  if (zf->node->idx < 0 && zf->node->ovl_id < 0)
    return -1;
  if (create && ovl_make_dir(zfs) != 0)
    return -1;
  if (chunk_name(zfs, zf->node, "", &path) != 0)
    return -1;
  fd = open(path, flags, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    if (create || errno != ENOENT)
      error("zip: cannot open chunk file %s: %s\n", path, strerror(errno));
    free(path);
    return -1;
  }
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
      free(path);
      return -1;
    }
  }
  zf->chunk_fd = fd;

  free(path);
  /* O_APPEND, so that a record from another instance can not land in
   * the middle of ours */
  if (chunk_name(zfs, zf->node, ".map", &path) == 0) {
    zf->map.fd = open(path, O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC,
        S_IRUSR | S_IWUSR);
    if (zf->map.fd == -1)
      error("zip: cannot open map file %s: %s\n", path, strerror(errno));
    free(path);
  }
  map_load(&zf->map);
  return fd;
}

/* picks up what another instance has appended since we last looked */
static void ovl_refresh(struct zip_file *zf)
{
  if (zf->chunk_fd == -1)
    ovl_chunk_fd(zf, 0);
  map_load(&zf->map);
}

static off_t ovl_size(struct zip_file *zf)
{
  return zf->map.size;
}

static int zf_close(vfs_file_t *file)
{
  struct zip_file *zf = (struct zip_file *)file;

  node_put(zf->zfs, zf->node);
  if (zf->zfp)
    zip_fclose(zf->zfp);
  if (zf->chunk_fd != -1)
    close(zf->chunk_fd);
  if (zf->map.fd != -1)
    close(zf->map.fd);
  free(zf->map.v);
  free(zf->data);
  free(zf);
  return 0;
}

/*
 * Lay whatever the overlay owns over what the archive gave us. Doing
 * it in this order rather than picking a source per range keeps the
 * archive read one contiguous operation, which is what the inflate
 * path wants, and the extents are few.
 */
static int read_overlay(struct zip_file *zf, void *buf, off_t off,
    size_t count)
{
  int i;

  for (i = 0; i < zf->map.n; i++) {
    off_t s = zf->map.v[i].off;
    off_t e = zf->map.v[i].end;

    if (e <= off)
      continue;
    if (s >= off + (off_t)count)
      break;
    if (s < off)
      s = off;
    if (e > off + (off_t)count)
      e = off + count;
    if (pread(zf->chunk_fd, (char *)buf + (s - off), e - s, s) != e - s) {
      error("zip: short read of chunk file: %s\n", strerror(errno));
      return -1;
    }
  }
  return 0;
}

static ssize_t zf_read(vfs_file_t *file, void *buf, size_t count)
{
  struct zip_file *zf = (struct zip_file *)file;
  off_t arc_left = zf->map.arc_valid - zf->pos;
  off_t left = ovl_size(zf) - zf->pos;
  ssize_t ret;

  if (left <= 0)
    return 0;
  if ((off_t)count > left)
    count = left;
  /* past the part of the archive's copy that still counts there is
   * only the overlay, and what it does not own reads as zeros */
  if (arc_left < 0)
    arc_left = 0;
  if ((off_t)count > arc_left)
    memset((char *)buf + arc_left, 0, count - arc_left);
  if (arc_left > 0) {
    size_t acnt = count < (size_t)arc_left ? count : (size_t)arc_left;

    if (zf->data) {
      memcpy(buf, zf->data + zf->pos, acnt);  // arc_valid bounds this
    } else {
      /* stored entry, read through the archive */
      if (zip_fseek(zf->zfp, zf->pos, SEEK_SET) < 0) {
        errno = EIO;
        return -1;
      }
      ret = zip_fread(zf->zfp, buf, acnt);
      if (ret < 0) {
        errno = EIO;
        return -1;
      }
      /* a short read from the archive is not short for DOS: the rest
       * is the overlay's to fill, or zeros */
      if ((size_t)ret < acnt)
        memset((char *)buf + ret, 0, acnt - ret);
    }
  }
  if (zf->map.n && read_overlay(zf, buf, zf->pos, count) != 0) {
    errno = EIO;
    return -1;
  }
  zf->pos += count;
  return count;
}

/*
 * A write goes into the chunk file at the offset DOS asked for and
 * nowhere else. Nothing underneath has to be read, let alone inflated:
 * the extent is recorded as the overlay's, and from then on reads take
 * it from here. That is what unaligned extents buy - the archive is
 * never touched on the way.
 */
static ssize_t zf_write(vfs_file_t *file, const void *buf, size_t count)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd;
  ssize_t ret;

  if (!zf->writable) {
    errno = EBADF;
    return -1;
  }
  if (!count)
    return 0;
  fd = ovl_chunk_fd(zf, 1);
  if (fd == -1) {
    errno = EROFS;
    return -1;
  }
  ret = pwrite(fd, buf, count, zf->pos);
  if (ret < 0)
    return -1;
  if (map_append(&zf->map, zf->pos, ret) != 0) {
    /* the bytes are in the chunk file but nothing will look at them,
     * so the write did not happen as far as anyone can tell */
    error("zip: cannot record write of %s\n", zf->node->name);
    errno = EIO;
    return -1;
  }
  zf->pos += ret;
  if (zf->map.size > zf->node->size)
    zf->node->size = zf->map.size;
  return ret;
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
    pos = ovl_size(zf) + offset;
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
  sb->st_size = ovl_size(zf);
  return 0;
}

static int zf_ftruncate(vfs_file_t *file, off_t length)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd;

  if (!zf->writable) {
    errno = EBADF;
    return -1;
  }
  if (length == ovl_size(zf))
    return 0;
  fd = ovl_chunk_fd(zf, 1);
  if (fd == -1) {
    errno = EROFS;
    return -1;
  }
  /*
   * Shrinking records first and cuts after: a crash in between leaves
   * a chunk file longer than the map says, which is harmless, where
   * the other order would leave the map pointing past its end.
   * Growing is the write case and goes the usual way round.
   */
  if (length < ovl_size(zf)) {
    if (map_append_trunc(&zf->map, length) != 0)
      return -1;
    zf->node->size = length;
    return ftruncate(fd, length);
  }
  if (ftruncate(fd, length) == -1)
    return -1;
  /* the grown part is the overlay's now, and reads as zeros */
  if (map_append(&zf->map, ovl_size(zf), length - ovl_size(zf)) != 0)
    return -1;
  zf->node->size = length;
  return 0;
}

static int zf_fsync(vfs_file_t *file)
{
  struct zip_file *zf = (struct zip_file *)file;

  if (zf->chunk_fd == -1)
    return 0;
  if (zf->map.fd != -1 && fsync(zf->map.fd) == -1)
    return -1;
  return fsync(zf->chunk_fd);
}

static int node_set_attr(struct zipfs *zfs, struct zip_node *n,
    const char *rel, int attr)
{
  attr = attr_for(n, attr);
  /* the log only has to say what the tree looks like, and a record
   * setting what is already set says nothing */
  if (attr == n->attr)
    return 0;
  if (log_append(zfs, 'a', attr, rel, NULL) != 0)
    return -1;
  n->attr = attr;
  return 0;
}

static int zf_get_dos_attr(vfs_file_t *file, int mode)
{
  struct zip_file *zf = (struct zip_file *)file;

  return zf->node->attr;
}

/* the create path sets the attribute on the handle it just got */
static int zf_set_dos_attr(vfs_file_t *file, int attr)
{
  struct zip_file *zf = (struct zip_file *)file;
  char *rel = node_path(zf->node);
  int ret;

  if (!rel) {
    errno = ENOENT;
    return -1;
  }
  ret = node_set_attr(zf->zfs, zf->node, rel, attr);
  free(rel);
  return ret;
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
/*
 * This one is the mutex the redirector wraps around reading and then
 * changing a region lock, and it is taken on every DOS read. With no
 * chunk file there are no region locks on this entry to serialise, so
 * it creates nothing; the first real lock creates the file, and from
 * then on there is something to take.
 */
static int zf_flock(vfs_file_t *file, int op)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd = ovl_chunk_fd(zf, 0);

  if (fd == -1)
    return 0;
  /* taking the lock is where another instance's writes become ours to
   * see, which is the same point at which a DOS program expects them */
  if (op & LOCK_UN)
    return flock(fd, op);
  if (flock(fd, op) != 0)
    return -1;
  map_load(&zf->map);
  return 0;
}

static int zf_setlk(vfs_file_t *file, struct flock *fl)
{
  struct zip_file *zf = (struct zip_file *)file;
  /* dropping a lock we never took needs no file to drop it on */
  int fd = ovl_chunk_fd(zf, fl->l_type != F_UNLCK);

  if (fd == -1)
    return 0;
  return fcntl(fd, F_OFD_SETLK, fl);
}

/*
 * Asking does not create anything: with no chunk file nobody holds a
 * lock on this entry, so the answer is already known. A lock taken
 * between the question and the answer would be missed, but taking one
 * goes to the kernel on the chunk file, and that is where a race
 * between two lockers is actually decided.
 *
 * It matters because every DOS read asks, and a drive whose files are
 * only read should leave nothing behind.
 */
static int zf_getlk(vfs_file_t *file, struct flock *fl)
{
  struct zip_file *zf = (struct zip_file *)file;
  int fd = ovl_chunk_fd(zf, 0);

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
  struct zip_dir *zd = (struct zip_dir *)dir;
  int i;

  for (i = 0; i < zd->n; i++)
    free(zd->names[i]);
  free(zd->names);
  free(zd);
  return 0;
}

static int zd_readdir(vfs_dir_t *dir, struct vfs_dirent *de)
{
  struct zip_dir *zd = (struct zip_dir *)dir;

  if (zd->pos >= zd->n)
    return -1;
  de->d_name = de->d_long_name = zd->names[zd->pos++];
  return 0;
}

static int zd_fstatdir(vfs_dir_t *dir, struct stat *sb)
{
  struct zip_dir *zd = (struct zip_dir *)dir;

  *sb = zd->sb;
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
  zf = calloc(1, sizeof(*zf));
  if (!zf)
    return NULL;
  zf->vfile.ops = &zip_file_ops;
  zf->vfile.fd = -1;
  zf->zfs = zfs;
  zf->node = n;
  n->refs++;
  zf->chunk_fd = -1;
  zf->map.fd = -1;
  zf->map.size = n->size;
  zf->map.arc_valid = n->arc_size;
  zf->writable = (flags & O_ACCMODE) != O_RDONLY;
  /* an earlier run, or another instance, may already own part of this
   * entry; reading has to see that without creating anything */
  ovl_refresh(zf);
  if ((flags & O_TRUNC) && zf->writable && zf_ftruncate(&zf->vfile, 0) != 0) {
    int err = errno;

    zf_close(&zf->vfile);
    errno = err;
    return NULL;
  }

  /* an entry that only exists in the overlay has no archive side at
   * all, and the read path already serves that */
  if (n->idx < 0)
    return &zf->vfile;

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
  zf->data = malloc(n->arc_size ? n->arc_size : 1);
  if (!zf->data) {
    zf_close(&zf->vfile);
    return NULL;
  }
  if (n->arc_size) {
    zip_file_t *zfp = zip_fopen_index(zfs->za, n->idx, 0);
    zip_int64_t rd;

    if (!zfp) {
      zf_close(&zf->vfile);
      errno = EIO;
      return NULL;
    }
    rd = zip_fread(zfp, zf->data, n->arc_size);
    zip_fclose(zfp);
    if (rd != (zip_int64_t)n->arc_size) {
      error("zip: short read of %s\n", path);
      zf_close(&zf->vfile);
      errno = EIO;
      return NULL;
    }
  }
  return &zf->vfile;
}

/*
 * The usual DOS way of rewriting a file is to create over it, so this
 * is the same as opening an existing entry and truncating it. A name
 * that is not in the archive still can not be made: the overlay has
 * nowhere to put an entry of its own yet.
 */
static vfs_file_t *zip_fs_creat(vfs_fs_t *fs, const char *path, int flags,
    mode_t mode)
{
  struct zipfs *zfs = fs->priv;

  if (!lookup(fs, path)) {
    const char *rel = rel_path(fs, path);

    if (!rel) {
      errno = ENOENT;
      return NULL;
    }
    if (log_append(zfs, '+', zfs->next_id, rel, NULL) != 0)
      return NULL;
    log_apply(zfs, '+', zfs->next_id, rel);
    if (!lookup(fs, path)) {
      errno = EIO;
      return NULL;
    }
  }
  return zip_fs_open(fs, path, (flags & ~O_CREAT) | O_RDWR | O_TRUNC);
}

static int zip_fs_ro(void)
{
  errno = EROFS;
  return -1;
}

static int zip_fs_unlink(vfs_fs_t *fs, const char *path)
{
  struct zipfs *zfs = fs->priv;
  struct zip_node *n = lookup(fs, path);
  const char *rel = rel_path(fs, path);

  if (!n || !rel || !n->parent) {
    errno = ENOENT;
    return -1;
  }
  if (n->is_dir) {
    errno = EISDIR;
    return -1;
  }
  if (log_append(zfs, '-', 0, rel, NULL) != 0)
    return -1;
  node_unlink(zfs, n);
  return 0;
}

static int zip_fs_mkdir(vfs_fs_t *fs, const char *path, mode_t mode)
{
  struct zipfs *zfs = fs->priv;
  const char *rel = rel_path(fs, path);

  if (!rel) {
    errno = ENOENT;
    return -1;
  }
  if (lookup(fs, path)) {
    errno = EEXIST;
    return -1;
  }
  if (log_append(zfs, 'd', zfs->next_id, rel, NULL) != 0)
    return -1;
  log_apply(zfs, 'd', zfs->next_id, rel);
  return 0;
}

static int zip_fs_rmdir(vfs_fs_t *fs, const char *path)
{
  struct zipfs *zfs = fs->priv;
  struct zip_node *n = lookup(fs, path);
  const char *rel = rel_path(fs, path);

  if (!n || !rel || !n->parent) {
    errno = ENOENT;
    return -1;
  }
  if (!n->is_dir) {
    errno = ENOTDIR;
    return -1;
  }
  if (n->child) {
    errno = ENOTEMPTY;
    return -1;
  }
  if (log_append(zfs, '-', 0, rel, NULL) != 0)
    return -1;
  node_unlink(zfs, n);
  return 0;
}

/*
 * Only the tree and the log move; the chunk file and its map stay put,
 * since neither is named after the path. A name outside this archive
 * means the data itself has to move, which only the caller can do.
 */
static int zip_fs_rename(vfs_fs_t *fs, const char *oldpath, const char *newpath)
{
  struct zipfs *zfs = fs->priv;
  struct zip_node *n = lookup(fs, oldpath);
  const char *from = rel_path(fs, oldpath);
  const char *to = rel_path(fs, newpath);
  struct zip_node *old, *dir;
  const char *base;
  char *prefix;

  if (!n || !from || !n->parent) {
    errno = ENOENT;
    return -1;
  }
  if (!to) {
    errno = EXDEV;
    return -1;
  }
  if (!*to) {
    errno = EISDIR;
    return -1;
  }
  /* the case of a name is the one thing a rename may change in place */
  old = lookup(fs, newpath);
  if (old && old != n) {
    errno = EEXIST;
    return -1;
  }
  /* a rename does not make the directories on the way to the new name */
  base = strrchr(to, '/');
  prefix = strndup(to, base ? base - to : 0);
  if (!prefix)
    return -1;
  dir = node_walk(zfs, prefix, 0);
  free(prefix);
  if (!dir || !dir->is_dir) {
    errno = ENOENT;
    return -1;
  }
  if (log_append(zfs, 'r', 0, from, to) != 0)
    return -1;
  node_rename(zfs, from, to);
  return 0;
}

/*
 * DOS asks for the modification time when it closes a file it wrote,
 * so this is the ordinary end of a write, not just an explicit touch.
 * The access time is not kept: nothing on a DOS drive reports one.
 */
static int zip_fs_utime(vfs_fs_t *fs, const char *path, time_t atime,
    time_t mtime)
{
  struct zipfs *zfs = fs->priv;
  struct zip_node *n = lookup(fs, path);
  const char *rel = rel_path(fs, path);
  long long secs;

  if (!n || !rel || !n->parent) {
    errno = ENOENT;
    return -1;
  }
  /* a time DOS cannot name in the first place is pinned to the range
   * it can, rather than refused */
  secs = (long long)mtime - LOG_EPOCH;
  if (secs < 0)
    secs = 0;
  if (secs > LOG_MTIME_MAX)
    secs = LOG_MTIME_MAX;
  if (LOG_EPOCH + (time_t)secs == n->mtime)
    return 0;
  if (log_append(zfs, 't', secs, rel, NULL) != 0)
    return -1;
  n->mtime = LOG_EPOCH + (time_t)secs;
  return 0;
}

static int zip_fs_setxattr(vfs_fs_t *fs, const char *path, int attr)
{
  return zip_fs_ro();
}

static int zip_fs_set_dos_attr(vfs_fs_t *fs, const char *path, int attr)
{
  struct zip_node *n = lookup(fs, path);
  const char *rel = rel_path(fs, path);

  if (!n || !rel || !n->parent) {
    errno = ENOENT;
    return -1;
  }
  return node_set_attr(fs->priv, n, rel, attr);
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
  /* an existing entry can be written, because the overlay takes it;
   * creating and deleting still can not be done */
  return 0;
}

/*
 * Writes land in the overlay, so the space that matters is the space
 * the overlay has. Falling back to the archive's own size keeps the
 * drive looking full rather than infinite if that can not be had.
 */
static int zip_fs_statvfs(vfs_fs_t *fs, const char *path, struct statvfs *sb)
{
  struct zipfs *zfs = fs->priv;
  struct statvfs host;

  memset(sb, 0, sizeof(*sb));
  sb->f_bsize = sb->f_frsize = 512;
  sb->f_namemax = 255;
  if (dosemu_tmpdir && statvfs(dosemu_tmpdir, &host) == 0 && host.f_frsize) {
    double scale = (double)host.f_frsize / 512;

    sb->f_blocks = host.f_blocks * scale;
    sb->f_bfree = sb->f_bavail = host.f_bavail * scale;
  } else {
    sb->f_blocks = (zfs->arc_size + 511) / 512;
  }
  return 0;
}

static vfs_dir_t *zip_fs_opendir(vfs_fs_t *fs, const char *path)
{
  struct zip_node *n = lookup(fs, path);
  struct zip_node *c;
  struct zip_dir *zd;
  int cnt = 0;

  if (!n) {
    errno = ENOENT;
    return NULL;
  }
  if (!n->is_dir) {
    errno = ENOTDIR;
    return NULL;
  }
  for (c = n->child; c; c = c->next)
    cnt++;
  zd = calloc(1, sizeof(*zd));
  if (!zd)
    return NULL;
  zd->names = calloc(cnt ? cnt : 1, sizeof(*zd->names));
  if (!zd->names) {
    free(zd);
    return NULL;
  }
  for (c = n->child; c; c = c->next) {
    zd->names[zd->n] = strdup(c->name);
    if (!zd->names[zd->n]) {
      zd_closedir(&zd->vdir);
      return NULL;
    }
    zd->n++;
  }
  zd->vdir.ops = &zip_dir_ops;
  zd->vdir.fd = -1;
  zd->vdir.has_sfn = 0;
  fill_stat(fs->priv, n, &zd->sb);
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

/*
 * A stat has no open file to ask, and the size DOS must see is the one
 * the overlay ended up with, not the archive's. Rather than look for a
 * map beside every entry, read the overlay directory once at mount:
 * only entries that were locked or written have one.
 */
static struct zip_node *node_by_id(struct zip_node *n, zip_int64_t idx,
    int ovl_id)
{
  struct zip_node *c;

  if (ovl_id >= 0 ? n->ovl_id == ovl_id : (n->ovl_id < 0 && n->idx == idx))
    return n;
  for (c = n->child; c; c = c->next) {
    struct zip_node *r = node_by_id(c, idx, ovl_id);

    if (r)
      return r;
  }
  return NULL;
}

static void sizes_from_overlay(struct zipfs *zfs)
{
  struct dirent *de;
  char *dir;
  DIR *d;

  if (!zfs->ovl)
    return;
  dir = strdup(zfs->ovl);
  if (!dir)
    return;
  dir[strlen(dir) - 1] = '\0';  // mkdir and opendir want no trailing slash
  d = opendir(dir);
  free(dir);
  if (!d)
    return;
  while ((de = readdir(d))) {
    struct ext_map m = { .fd = -1 };
    struct zip_node *n;
    zip_int64_t idx = -1;
    int ovl_id = -1;
    char *end;
    char *path;
    const char *p = de->d_name;
    int len = strlen(p);

    if (len < 5 || strcmp(p + len - 4, ".map") != 0)
      continue;
    errno = 0;
    if (*p == 'n')
      ovl_id = strtol(p + 1, &end, 16);
    else
      idx = strtoll(p, &end, 16);
    if (errno || end != p + len - 4)
      continue;
    n = node_by_id(zfs->tree, idx, ovl_id);
    if (!n || n->is_dir)
      continue;
    if (asprintf(&path, "%s%s", zfs->ovl, p) == -1)
      continue;
    m.fd = open(path, O_RDONLY | O_CLOEXEC);
    free(path);
    if (m.fd == -1)
      continue;
    m.size = n->size;
    m.arc_valid = n->arc_size;
    map_load(&m);
    n->size = m.size;
    close(m.fd);
    free(m.v);
  }
  closedir(d);
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
  zfs->log_fd = -1;
  zfs->id_fd = -1;

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

  zfs->arc_id = arc_fingerprint(zfs->za);
  ovl_id_hold(zfs);
  if (!ovl_id_ok(zfs)) {
    node_free(zfs->tree);
    zip_close(zfs->za);
    if (zfs->id_fd != -1)
      close(zfs->id_fd);         // and with it the shared lock it holds
    free(zfs->ovl);
    free(zfs->root);
    free(zfs);
    return -1;
  }

  /* whatever an earlier run created or deleted, on top of the archive */
  log_replay(zfs);
  /* and whatever it wrote, so that a listing shows the size DOS would
   * read rather than the archive's */
  sizes_from_overlay(zfs);

  fs->ops = &zip_fs_ops;
  fs->priv = zfs;
  return 0;
}

/* the file's length, or -1 if it is not there */
static off_t ovl_len(const char *dir, const char *name)
{
  struct stat sb;
  char *path;
  off_t ret = -1;

  if (asprintf(&path, "%s%s", dir, name) == -1)
    return -1;
  if (stat(path, &sb) == 0)
    ret = sb.st_size;
  free(path);
  return ret;
}

static void ovl_unlink(const char *dir, const char *name)
{
  char *path;

  if (asprintf(&path, "%s%s", dir, name) == -1)
    return;
  if (unlink(path) == -1 && errno != ENOENT)
    error("zip: cannot remove %s: %s\n", path, strerror(errno));
  free(path);
}

/*
 * What the overlay has to show for itself once nobody is using it.
 *
 * A chunk file is made whenever an entry needs a host file to carry
 * kernel locks, whether or not anything is ever written to it, so an
 * archive that was only read from - but locked, as DOS programs do -
 * used to leave a sparse file the size of the entry behind for good. A
 * chunk whose map is empty owns no byte of its entry and is exactly
 * that: nothing to lose by removing it.
 *
 * Only the last mount does this, which is what the shared stamp lock
 * establishes. Another instance may have the same chunk file open to
 * lock against us, and removing it under them would leave the two of
 * them locking different inodes and neither of them wiser.
 */
static void ovl_prune(struct zipfs *zfs)
{
  struct dirent *de;
  char *dir;
  DIR *d;
  int left = 0;

  if (!zfs->ovl)
    return;
  d = opendir(zfs->ovl);
  if (!d)
    return;
  while ((de = readdir(d))) {
    const char *n = de->d_name;
    int len = strlen(n);
    char *map;

    if (!strcmp(n, ".") || !strcmp(n, ".."))
      continue;
    if (!strcmp(n, "id") || !strcmp(n, LOG_NAME))
      continue;
    if (len > 4 && !strcmp(n + len - 4, ".map"))
      continue;                 // dealt with along with its chunk
    if (asprintf(&map, "%s.map", n) == -1)
      continue;
    if (ovl_len(zfs->ovl, map) > 0)
      left++;                   // the overlay owns part of this entry
    else {
      ovl_unlink(zfs->ovl, map);
      ovl_unlink(zfs->ovl, n);
    }
    free(map);
  }
  closedir(d);

  /* and the bookkeeping itself, once it is all that is left and it
   * records nothing either */
  if (left || ovl_len(zfs->ovl, LOG_NAME) > 0)
    return;
  ovl_unlink(zfs->ovl, LOG_NAME);
  ovl_unlink(zfs->ovl, "id");
  dir = strdup(zfs->ovl);
  if (dir) {
    dir[strlen(dir) - 1] = '\0';       // rmdir wants no trailing slash
    rmdir(dir);
    free(dir);
  }
}

static void zip_umount(vfs_fs_t *fs)
{
  struct zipfs *zfs = fs->priv;

  if (!zfs)
    return;
  if (zfs->tree)
    node_free(zfs->tree);
  zip_close(zfs->za);
  if (zfs->log_fd != -1)
    close(zfs->log_fd);
  /* granted only if no other mount holds the stamp, i.e. we are last */
  if (zfs->id_fd != -1) {
    if (flock(zfs->id_fd, LOCK_EX | LOCK_NB) == 0)
      ovl_prune(zfs);
    close(zfs->id_fd);
  }
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
