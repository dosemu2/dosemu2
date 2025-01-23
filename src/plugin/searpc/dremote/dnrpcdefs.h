#ifndef DNRPCDEFS_H
#define DNRPCDEFS_H

int dnrpc_srv_init(const char *svc_name, int fd);
int dnrpc_exiting(void);

extern char *rpc_shared_page;
#define rpc_control_storage (rpc_shared_page + PAGE_SIZE)
struct rpc_c {
    int size;
    char data[0];
};
#define rpc_control_struct ((struct rpc_c *)(rpc_control_storage - \
  sizeof(struct rpc_c)))

struct full_state {
    cpuctx_t cpu;
    emu_fpstate fpu;
};

static inline void send_state(cpuctx_t *scp)
{
    struct full_state *f = (struct full_state *)rpc_shared_page;
    f->cpu = *scp;
    f->fpu = vm86_fpu_state;
}

static inline void recv_state(cpuctx_t *scp)
{
    struct full_state *f = (struct full_state *)rpc_shared_page;
    *scp = f->cpu;
    vm86_fpu_state = f->fpu;
}

#endif
