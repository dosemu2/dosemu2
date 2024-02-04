#ifndef USE_DJDEV64
#define USE_DJDEV64 1

struct pm_regs;
struct djdev64_ops {
    int (*open)(const char *path);
    void (*close)(struct pm_regs *scp);
    unsigned *call;
    unsigned *ctrl;
};

void register_djdev64(const struct djdev64_ops *ops);

#endif
