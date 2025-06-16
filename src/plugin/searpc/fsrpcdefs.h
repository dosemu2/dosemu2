#ifndef FSRPCDEFS_H
#define FSRPCDEFS_H

#include "fslib/fssvc.h"  // for setattr_cb

int fsrpc_srv_init(const char *svc_name, int fd, plist_idx_t plist_idx,
    setattr_t setattr_cb, getattr_t getattr_cb);
int fsrpc_exiting(void);

#endif
