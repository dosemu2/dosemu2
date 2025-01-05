/*
 *  Copyright (C) 2024  stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <searpc.h>
#include "test-object.h"
#include "utilities.h"
#include "emu.h"
#include "fsrpcdefs.h"
#include "util.h"
#include "fssvc.h"
#include "fssvc_priv.h"

static SearpcClient *clnt;
static int sock_rx;
static int exited;

static void svc_ex(void *arg)
{
    error("fssvc failed, exiting\n");
    exited++;
    leavedos(35);
}

struct svc_args {
    plist_idx_t plist_idx;
    setattr_t setattr_cb;
    getattr_t getattr_cb;
};

static int fssrv_init(const char *svc_name, int sock, void *arg)
{
    struct svc_args *args = arg;
    return fsrpc_srv_init(svc_name, sock, args->plist_idx, args->setattr_cb,
                          args->getattr_cb);
}

int fssvc_init(plist_idx_t plist_idx, setattr_t setattr_cb,
    getattr_t getattr_cb)
{
    struct svc_args args = { plist_idx, setattr_cb, getattr_cb };

    clnt = clnt_init(&sock_rx, fssrv_init, &args, fsrpc_exiting,
            svc_ex, "fsrpc", NULL);
    return (clnt ? 0 : -1);
}

static void bad_rpc(const char *func, const char *msg)
{
    fprintf(stderr, "RPC failure: %s: %s\n", func, msg);
    if (!exited) {
        exited++;
        leavedos(5);
    }
}

int fssvc_add_path(const char *path)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "add_path_1",
                                  &error, 1,
                                  "string", path);
    CHECK_RPC(error);
    return ret;
}

int fssvc_add_path_ex(const char *path)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "add_path_ex_1",
                                  &error, 1,
                                  "string", path);
    CHECK_RPC(error);
    return ret;
}

int fssvc_add_path_list(const char *list)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "add_path_list_1",
                                  &error, 1,
                                  "string", list);
    CHECK_RPC(error);
    return ret;
}

int fssvc_seal(void)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "seal_1", &error, 0);
    CHECK_RPC(error);
    return ret;
}

static void bad_call(int err, const char *func)
{
    dosemu_error("%s returned %s, exiting\n", func, strerror(err));
    if (!exited) {
        exited++;
        leavedos(6);
    }
}

#define CHECK_RET(_r) do { \
    TestObject *r = TEST_OBJECT(_r); \
    if ((r)->ret < 0) { \
        int _rv; \
        if ((r)->args_err) \
            bad_call((r)->args_err, __FUNCTION__); \
        else \
            errno = (r)->errn; \
        _rv = (r)->ret; \
        g_object_unref(_r); \
        return _rv; \
    } \
} while (0)

int fssvc_open(int id, const char *path, int flags)
{
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "open_1", TEST_OBJECT_TYPE,
                                     &error, 3,
                                     "int", id, "string", path,
                                     "int", flags);
    CHECK_RPC(error);
    CHECK_RET(ret);
    g_object_unref(ret);
    return recv_fd(sock_rx);
}

int fssvc_creat(int id, const char *path, int flags, mode_t mode)
{
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "creat_1", TEST_OBJECT_TYPE,
                                     &error, 4,
                                     "int", id, "string", path,
                                     "int", flags, "int", mode);
    CHECK_RPC(error);
    CHECK_RET(ret);
    g_object_unref(ret);
    return recv_fd(sock_rx);
}

#define RPC_EPILOG() \
    CHECK_RPC(error); \
    CHECK_RET(ret); \
    rv = TEST_OBJECT(ret)->ret; \
    g_object_unref(ret); \
    return rv

int fssvc_unlink(int id, const char *path)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "unlink_1", TEST_OBJECT_TYPE,
                                     &error, 2,
                                     "int", id, "string", path);
    RPC_EPILOG();
}

int fssvc_setxattr(int id, const char *path, int attr)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "setxattr_1", TEST_OBJECT_TYPE,
                                     &error, 3,
                                     "int", id, "string", path, "int", attr);
    RPC_EPILOG();
}

int fssvc_getxattr(int id, const char *path)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "getxattr_1", TEST_OBJECT_TYPE,
                                     &error, 2,
                                     "int", id, "string", path);
    RPC_EPILOG();
}

int fssvc_rename(int id1, const char *path1, int id2, const char *path2)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "rename_1", TEST_OBJECT_TYPE,
                                     &error, 4,
                                     "int", id1, "string", path1,
                                     "int", id2, "string", path2);
    RPC_EPILOG();
}

int fssvc_mkdir(int id, const char *path, mode_t mode)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "mkdir_1", TEST_OBJECT_TYPE,
                                     &error, 3,
                                     "int", id, "string", path, "int", mode);
    RPC_EPILOG();
}

int fssvc_rmdir(int id, const char *path)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "rmdir_1", TEST_OBJECT_TYPE,
                                     &error, 2,
                                     "int", id, "string", path);
    RPC_EPILOG();
}

int fssvc_utime(int id, const char *path, time_t atime, time_t mtime)
{
    int rv;
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "utime_1", TEST_OBJECT_TYPE,
                                     &error, 4,
                                     "int", id, "string", path,
                                     /* searpc passes 64bit ints via ptrs */
                                     "int64", &atime, "int64", &mtime);
    RPC_EPILOG();
}

int fssvc_path_ok(int id, const char *path)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "path_ok_1",
                                  &error, 2,
                                  "int", id, "string", path);
    CHECK_RPC(error);
    return ret;
}

int fssvc_exit(void)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "exit_1", &error, 0);
    searpc_client_free(clnt);
    if (!in_leavedos)
        CHECK_RPC(error);
    return ret;
}

int fssvc_shm_open(const char *name, int oflag, mode_t mode)
{
    GObject* ret;
    GError *error = NULL;
    ret = searpc_client_call__object(clnt, "shm_open_1", TEST_OBJECT_TYPE,
                                     &error, 3,
                                     "string", name, "int", oflag,
                                     "int", mode);
    CHECK_RPC(error);
    CHECK_RET(ret);
    g_object_unref(ret);
    return recv_fd(sock_rx);
}

int fssvc_shm_unlink(const char *name)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "shm_unlink_1",
                                  &error, 1,
                                  "string", name);
    CHECK_RPC(error);
    return ret;
}

int fssvc_set_command(int subsys, int cookie, const char *cmd)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "set_command_1",
                                  &error, 3,
                                  "int", subsys,
                                  "int", cookie,
                                  "string", cmd);
    CHECK_RPC(error);
    return ret;
}

int fssvc_popen(int subsys, int cookie, struct popen2 *file)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "popen_1",
                                  &error, 2,
                                  "int", subsys,
                                  "int", cookie);
    CHECK_RPC(error);
    if (ret < 0)
        return ret;
    file->from_child = recv_fd(sock_rx);
    file->to_child = recv_fd(sock_rx);
    return ret;
}
