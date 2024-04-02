#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <sys/capability.h>
#include "dosemu_debug.h"
#include "ns.h"

int ns_init(void)
{
    uid_t uid;
    uid_t gid;
    int err;
    FILE *f;

    uid = getuid();
    gid = getgid();
    err = unshare(CLONE_NEWNS | CLONE_NEWUSER);
    if (err) {
        perror("unshare()");
        return -1;
    }

    f = fopen("/proc/self/uid_map", "w");
    if (!f) {
        perror("fopen(/proc/self/uid_map)");
        return -1;
    }
    err = fprintf(f, "%d %d 1\n", uid, uid);
    fclose(f);
    if (err <= 0) {
        error("can't write to /proc/self/uid_map\n");
        return -1;
    }

    f = fopen("/proc/self/setgroups", "w");
    if (!f) {
        perror("fopen(/proc/self/setgroups)");
        return -1;
    }
    err = fprintf(f, "deny\n");
    fclose(f);
    if (err <= 0) {
        error("can't write to /proc/self/setgroups\n");
        return -1;
    }

    f = fopen("/proc/self/gid_map", "w");
    if (!f) {
        perror("fopen(/proc/self/gid_map)");
        return -1;
    }
    err = fprintf(f, "%d %d 1\n", gid, gid);
    fclose(f);
    if (err <= 0) {
        error("can't write to /proc/self/gid_map\n");
        return -1;
    }

    return 0;
}

int ns_priv_drop(void)
{
    int rc;
    cap_t cap;

    cap = cap_init();
    if (!cap)
        return -1;
    cap_clear(cap);
    rc = cap_set_proc(cap);
    cap_free(cap);
    return rc;
}
