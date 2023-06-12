#ifndef RLOCKS_H
#define RLOCKS_H

#if HAVE_DECL_F_OFD_SETLK

int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr, int mlemu_fd);
int region_lock_offs(int fd, long long start, unsigned long len,
    int wr, const char *mlemu);
void region_unlock_offs(int fd);

#else

static inline int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr, int mlemu_fd)
{
    return 0;
}

static inline int region_lock_offs(int fd, long long start, unsigned long len,
    int wr, const char *mlemu)
{
    return len;
}

static inline void region_unlock_offs(int fd)
{
}

#endif

#endif
