#ifndef USE_DJDEV64
#define USE_DJDEV64 1

struct pm_regs;
struct djdev64_ops {
    int (*open)(const char *path);
    void (*close)(int handle);
    unsigned *call;
    unsigned *ctrl;
};

void register_djdev64(const struct djdev64_ops *ops);

#endif
