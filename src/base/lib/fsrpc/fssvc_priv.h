#ifndef FSSVC_PRIV_H
#define FSSVC_PRIV_H

#include <fcntl.h>  // for mode_t
#include <time.h>   // for time_t
#include "fssvc.h"

int fssvc_init(plist_idx_t plist_idx, setattr_t settatr_cb,
    getattr_t getattr_cb);
int fssvc_add_path(const char *path);
int fssvc_add_path_ex(const char *path);
int fssvc_add_path_list(const char *list);
int fssvc_seal(void);
int fssvc_open(int id, const char *path, int flags);
int fssvc_creat(int id, const char *path, int flags, mode_t mode);
int fssvc_unlink(int id, const char *path);
int fssvc_setxattr(int id, const char *path, int attr);
int fssvc_getxattr(int id, const char *path);
int fssvc_rename(int id1, const char *path1, int id2, const char *path2);
int fssvc_mkdir(int id, const char *path, mode_t mode);
int fssvc_rmdir(int id, const char *path);
int fssvc_utime(int id, const char *path, time_t atime, time_t mtime);
int fssvc_path_ok(int id, const char *path);
int fssvc_exit(void);

#endif
