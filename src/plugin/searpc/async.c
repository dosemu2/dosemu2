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
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/prctl.h>  /* Definition of PR_* constants */
#include <sys/prctl.h>
#include <searpc.h>
#include "priv.h"
#include "sig.h"
#include "utilities.h"
#include "util.h"

#ifndef NEW_SEARPC
/* work around https://github.com/haiwen/libsearpc/pull/79 */
typedef struct {
    SearpcClient *client;
    AsyncCallback callback;
    const gchar *ret_type;
    GType gtype;                /* to specify the specific gobject type
                                 if ret_type is object or objlist */
    void *cbdata;
} AsyncCallData;
#define _ASYNC_CALL_DATA_SIZEOF sizeof(AsyncCallData)
#endif

static int transport_send(void *arg, char *fcall_str,
                          size_t fcall_len, void *rpc_priv)
{
    SockTransport *sock = arg;
    ssize_t sd;

    assert(!sock->in_async);
    sock->in_async++;
    sd = send(sock->fd, fcall_str, fcall_len, MSG_DONTWAIT);
    if (sd <= 0)
        return -1;
#ifdef _ASYNC_CALL_DATA_SIZEOF
    sock->rpc_priv = malloc(_ASYNC_CALL_DATA_SIZEOF);
    memcpy(sock->rpc_priv, rpc_priv, _ASYNC_CALL_DATA_SIZEOF);
#else
    sock->rpc_priv = rpc_priv;
#endif
    return 0;
}

static int transport_recv(SockTransport *sock)
{
    ssize_t sd;
    char buf[4096];

    assert(sock->in_async == 1);
    sock->in_async--;
    sd = recv(sock->fd, buf, sizeof(buf), 0);
    if (sd <= 0)
        return -1;
    searpc_client_generic_callback(buf, sd, sock->rpc_priv, NULL);
#ifdef _ASYNC_CALL_DATA_SIZEOF
    free(sock->rpc_priv);
#endif
    sock->rpc_priv = NULL;
    return 0;
}

int searpc_async_recv(SearpcClient *clnt)
{
    return transport_recv(clnt->async_arg);
}

SearpcClient *async_clnt_init(int *sock_rx, init_cb_t init_cb,
        void *init_arg, int (*svc_ex)(void),
        void (*ex_cb)(void *), const char *svc_name, pid_t *r_pid)
{
    SearpcClient *clnt = clnt_init(sock_rx, init_cb, init_arg, svc_ex, ex_cb,
            svc_name, r_pid);
    if (!clnt)
        return NULL;
    clnt->async_send = transport_send;
    clnt->async_arg = clnt->arg;
    return clnt;
}
