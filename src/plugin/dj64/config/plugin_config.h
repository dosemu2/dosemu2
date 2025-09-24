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

typedef void stub_enter_t(struct pm_regs *scp,
    int argc, char *argv[], char *envp[],
    unsigned psp_sel, int ifile, int ver);

void register_elfloader(stub_enter_t *ldr);

#endif
