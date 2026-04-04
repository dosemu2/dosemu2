#ifndef SRPC_UTIL_H
#define SRPC_UTIL_H

typedef int (*init_cb_t)(const char *, int, void *);
SearpcClient *clnt_init(int *sock_rx, init_cb_t init_cb,
        void *init_arg, int (*svc_ex)(void),
        void (*ex_cb)(void *), const char *svc_name, pid_t *r_pid);
int searpc_async_recv(SearpcClient *clnt);

#define _CHECK_RPC(st, c) do { \
    if (st) { \
        c \
        bad_rpc(__FUNCTION__, st->message); \
        g_error_free(st); \
        return -1; \
    } \
} while (0)

#define CHECK_RPC(st) _CHECK_RPC(st,)
//#define CHECK_RPC_LOCKED(st) _CHECK_RPC(st, pthread_mutex_unlock(&rpc_mtx);)

#endif
