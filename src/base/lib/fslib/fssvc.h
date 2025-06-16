#ifndef FSSVC_H
#define FSSVC_H

typedef int (*setattr_t)(const char *path, int attr);
typedef int (*getattr_t)(const char *path);
typedef int (*plist_idx_t)(const char *clist, const char *path);

#endif
