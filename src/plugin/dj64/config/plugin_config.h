#ifndef USE_DJDEV64
#define USE_DJDEV64 1

struct djdev64_ops {
    int (*open)(const char *path);
    int (*call)(int handle, int libid, int fn, unsigned char *sp);
    int (*ctrl)(int handle, int libid, int fn, unsigned char *sp);
    void (*close)(int handle);
};

void register_djdev64(const struct djdev64_ops *ops);

#endif
