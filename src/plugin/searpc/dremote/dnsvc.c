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
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>
#include <searpc.h>
#include "utilities.h"
#include "mapping.h"
#include "dnative.h"
#include "plugin_config.h"
#include "init.h"
#include "emu.h"
#include "vgaemu.h"
#include "../util.h"
#include "uffd.h"
#include "dnrpcdefs.h"

static SearpcClient *clnt;
static int sock_tx;
static int exited;
char *rpc_shared_page;
#define RPC_SHARED_SIZE 0x10000  // 64K for passing LDT buffer

static int remote_mmap(void *addr, size_t length, int prot, int flags,
                       int fd, off_t offset);

static void svc_ex(void *arg)
{
    error("fssvc failed, exiting\n");
    exited++;
    leavedos(35);
}

static int dnsrv_init(const char *svc_name, int fd, void *arg)
{
    return dnrpc_srv_init(svc_name, fd);
}

static void bad_rpc(const char *func, const char *msg)
{
    fprintf(stderr, "RPC failure: %s: %s\n", func, msg);
    exit(5);
}

static int remote_mmap(void *addr, size_t length, int prot, int flags,
                       int fd, off_t offset)
{
    int ret;
    GError *error = NULL;

    if (!clnt)
        return 0;
    send_fd(sock_tx, fd);
    memcpy(rpc_shared_page, vga.mem.map, sizeof(vga.mem.map));
    ret = searpc_client_call__int(clnt, "mmap_1",
                                  &error, 5,
                                  "int64", &addr,
                                  "int64", &length, "int", prot,
                                  "int", flags, "int64", &offset);
    CHECK_RPC(error);
    uffd_reinit(addr, length);
    return ret;
}

static int remote_mprotect(void *addr, size_t length, int prot)
{
    int ret;
    GError *error = NULL;

    if (!clnt)
        return 0;
    assert(pthread_equal(dosemu_pthread_self, pthread_self()));
    ret = searpc_client_call__int(clnt, "mprotect_1",
                                  &error, 3,
                                  "int64", &addr,
                                  "int64", &length, "int", prot);
    CHECK_RPC(error);
    return ret;
}

static int remote_madvise(void *addr, size_t length, int flags)
{
    int ret;
    GError *error = NULL;

    if (!clnt)
        return 0;
    ret = searpc_client_call__int(clnt, "madvise_1",
                                  &error, 3,
                                  "int64", &addr,
                                  "int64", &length, "int", flags);
    CHECK_RPC(error);
    return ret;
}

static const struct mapping_hook mhook = {
    .mmap = remote_mmap,
    .mprotect = remote_mprotect,
    .madvise = remote_madvise,
    .wp_vga = uffd_wp,
};

static int remote_dpmi_setup(void)
{
    int ret, err;
    GError *error = NULL;

    if (clnt)
        return -1;
    rpc_shared_page = alloc_mapping(MAPPING_OTHER, RPC_SHARED_SIZE);
    assert(rpc_shared_page != MAP_FAILED);
    clnt = clnt_init(&sock_tx, dnsrv_init, NULL, dnrpc_exiting, svc_ex, "dnrpc",
            &dpmi_pid);
    if (!clnt) {
        fprintf(stderr, "failure registering RPC\n");
        return -1;
    }
    err = uffd_init(sock_tx);
    if (err) {
        int stat;
        /* signal_init() is not yet called so SIGCHLD is not delivered */
        sigchld_enable_handler(dpmi_pid, 0);
        waitpid(dpmi_pid, &stat, 0);
        searpc_client_free(clnt);
        clnt = NULL;
        return -1;
    }
    ret = searpc_client_call__int(clnt, "setup_1", &error, 0);
    CHECK_RPC(error);
    return ret;
}

static int _remote_dpmi_done(void)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "done_1", &error, 0);
    CHECK_RPC(error);
    return ret;
}

static void remote_dpmi_done(void)
{
    _remote_dpmi_done();
    searpc_client_free(clnt);
    clnt = NULL;
    free_mapping(MAPPING_OTHER, rpc_shared_page, RPC_SHARED_SIZE);
    rpc_shared_page = NULL;
}

static int _remote_dpmi_set_cpio(int base, int size)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "set_cpio_1",
                                  &error, 2,
                                  "int", base,
                                  "int", size);
    CHECK_RPC(error);
    return ret;
}

static void remote_dpmi_set_cpio(int base, int size)
{
    _remote_dpmi_set_cpio(base, size);
}

static int _remote_dpmi_set_drio(int base, int size)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "set_drio_1",
                                  &error, 2,
                                  "int", base,
                                  "int", size);
    CHECK_RPC(error);
    return ret;
}

static void remote_dpmi_set_drio(int base, int size)
{
    _remote_dpmi_set_drio(base, size);
}

static int remote_dpmi_control(cpuctx_t *scp, char *storage, int *r_size)
{
    int ret;
    GError *error = NULL;
    struct rpc_c *c;
    send_state(scp);
    in_rdpmi++;
    ret = searpc_client_call__int(clnt, "control_1", &error, 0);
    in_rdpmi--;
    CHECK_RPC(error);
    recv_state(scp);
    c = rpc_control_struct;
    *r_size = c->size;
    if (c->size)
        memcpy(storage, c->data, c->size);
    return ret;
}

static int _remote_dpmi_exit(cpuctx_t *scp)
{
    int ret;
    GError *error = NULL;
    send_state(scp);
    ret = searpc_client_call__int(clnt, "exit_1", &error, 0);
    CHECK_RPC(error);
    recv_state(scp);
    return ret;
}

static void remote_dpmi_exit(cpuctx_t *scp)
{
    _remote_dpmi_exit(scp);
}

static int remote_read_ldt(void *ptr, int bytecount, uint64_t base)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "read_ldt_1",
                                  &error, 2,
                                  "int", bytecount,
                                  "int64", &base);
    CHECK_RPC(error);
    if (ret > 0)
        memcpy(ptr, rpc_shared_page, ret);
    return ret;
}

static int remote_write_ldt(const void *ptr, int bytecount, uint64_t base)
{
    int ret;
    GError *error = NULL;
    memcpy(rpc_shared_page, ptr, bytecount);
    ret = searpc_client_call__int(clnt, "write_ldt_1",
                                  &error, 2,
                                  "int", bytecount,
                                  "int64", &base);
    CHECK_RPC(error);
    return ret;
}

static int remote_check_verr(unsigned short selector)
{
    int ret;
    GError *error = NULL;
    ret = searpc_client_call__int(clnt, "check_verr_1",
                                  &error, 1,
                                  "int", selector);
    CHECK_RPC(error);
    return ret;
}

static int remote_debug_breakpoint(int op, cpuctx_t *scp, int err)
{
    int ret;
    GError *error = NULL;
    send_state(scp);
    ret = searpc_client_call__int(clnt, "debug_breakpoint_1",
                                  &error, 2,
                                  "int", op, "int", err);
    CHECK_RPC(error);
    recv_state(scp);
    return ret;
}

static const struct dnative_ops ops = {
    remote_dpmi_setup,
    remote_dpmi_done,
    remote_dpmi_set_cpio,
    remote_dpmi_set_drio,
    remote_dpmi_control,
    remote_dpmi_exit,
    remote_read_ldt,
    remote_write_ldt,
    remote_check_verr,
    remote_debug_breakpoint,
};

CONSTRUCTOR(static void dnsvc_init(void))
{
    if (config.dpmi_remote) {
        mapping_register_hook(&mhook);
        register_dnative_ops(&ops);
    }
}
