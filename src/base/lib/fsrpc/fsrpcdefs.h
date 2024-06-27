#ifndef FSRPCDEFS_H
#define FSRPCDEFS_H

#include "fssvc.h"  // for setattr_cb

void fsrpc_svc_run(void);

int fsrpc_srv_init(int tr_fd, int fd, setattr_t setattr_cb,
    getattr_t getattr_cb);

#endif
