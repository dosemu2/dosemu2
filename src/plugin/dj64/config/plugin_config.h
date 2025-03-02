#ifndef USE_DJDEV64
#define USE_DJDEV64 1

struct pm_regs;
struct djdev64_ops {
    int (*open)(const char *path, unsigned short flags);
    void (*close)(int handle);
    unsigned (*call)(int handle);
    unsigned (*ctrl)(int handle);
    unsigned (*stub)(void);
    int (*exec)(const char *path, int handle, int libid, unsigned flags);
    int (*elfopen)(const char *path, unsigned short flags);
    int (*memfd)(const char *path);
    unsigned (*run64)(void);
};

void register_djdev64(const struct djdev64_ops *ops);

#endif
