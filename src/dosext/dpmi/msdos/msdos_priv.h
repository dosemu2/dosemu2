#ifndef MSDOS_PRIV_H
#define MSDOS_PRIV_H

unsigned short ConvertSegmentToDescriptor_lim(unsigned short segment,
    unsigned int limit);

u_short msdos_map_buffer(int *len);
void msdos_unmap_buffer(void);

struct XMS_call {
    far_t call;
    unsigned short seg;
    int len;
};

#endif
