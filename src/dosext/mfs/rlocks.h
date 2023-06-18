#ifndef RLOCKS_H
#define RLOCKS_H

#if HAVE_DECL_F_OFD_SETLK

int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr, int mlemu_fd);
int region_lock_offs(int fd, long long start, unsigned long len,
    int wr);
void region_unlock_offs(int fd);
int region_is_fully_owned(int fd, long long start, unsigned long len, int wr,
    int mlemu_fd2);

#else

static inline int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr, int mlemu_fd)
{
    return 0;
}

static inline int region_lock_offs(int fd, long long start, unsigned long len,
    int wr)
{
    return len;
}

static inline void region_unlock_offs(int fd)
{
}

static inline int region_is_fully_owned(int fd, long long start,
    unsigned long len, int wr, int mlemu_fd2)
{
    return 1;  // locks not implemented, allow everything
}

#endif

#endif
