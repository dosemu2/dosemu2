#ifndef DMA_H
#define DMA_H

void dma_init(void);
void dma_reset(void);

enum { DMA_NO_DACK, DMA_DACK };
int dma_pulse_DRQ(int ch, Bit8u *buf);

#endif /* DMA_H */
