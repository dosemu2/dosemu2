#ifndef BOOT_H
#define BOOT_H

int fdpp_boot(far_t *plt, int plt_len, const void *krnl, int len,
        uint16_t seg, uint16_t off, int khigh,
        uint16_t heap_seg, int heap, int hhigh, unsigned char *boot_sec,
        uint16_t bpseg);
int fdpp_boot_xtra_space(void);

#endif
