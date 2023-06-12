#ifndef RLOCKS_H
#define RLOCKS_H

int lock_file_region(int fd, int lck, long long start,
    unsigned long len, int wr);
int region_lock_offs(int fd, long long start, unsigned long len, int wr);
void region_unlock_offs(int fd);

#endif
