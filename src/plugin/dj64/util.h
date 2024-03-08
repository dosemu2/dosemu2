#ifndef UTIL_H
#define UTIL_H

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_MASK
#define PAGE_MASK	(~(PAGE_SIZE-1))
#endif

long _long_read(int file, char *buf, unsigned long offset,
    unsigned long size);

#endif
