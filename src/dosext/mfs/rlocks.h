#ifndef RLOCKS_H
#define RLOCKS_H

#if HAVE_DECL_F_OFD_SETLK

#ifdef __linux__
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
  #ifdef WARN_UNDISABLED_WA
    #warning Not disabling FUNLCK_WA, update your kernel
  #endif
  #define FUNLCK_WA 1
#else
  #ifdef DISABLE_SYSTEM_WA
    #define FUNLCK_WA 0
  #else
    #define FUNLCK_WA 1
  #endif
#endif
#else
  #define FUNLCK_WA 1
#endif

#if FUNLCK_WA
void open_mlemu(int *r_fds);
void close_mlemu(int *fds);
#else
static inline void open_mlemu(int *r_fds)
{
}

static inline void close_mlemu(int *fds)
{
}
#endif

int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr, int mlemu_fd);
int region_lock_offs(int fd, long long start, unsigned long len,
    int wr);
void region_unlock_offs(int fd);
int region_is_fully_owned(int fd, long long start, unsigned long len, int wr,
    int mlemu_fd2);

#else

static inline void open_mlemu(int *r_fds)
{
}

static inline void close_mlemu(int *fds)
{
}

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
