#ifndef BOOT_H
#define BOOT_H

int fdpp_boot(far_t plt, const void *krnl, int len, uint16_t seg,
        uint16_t heap_seg, int heap, unsigned char *boot_sec);

#endif
