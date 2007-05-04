/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef DMA_H
#define DMA_H

void dma_init(void);
void dma_reset(void);
void dma_new_init(void);
void dma_new_reset(void);

enum { DMA_NO_DACK, DMA_DACK };
int dma_pulse_DRQ(int ch, Bit8u *buf);

/* 8237 DMA controllers */
#define IO_DMA1_BASE	0x00	/* 8 bit slave DMA, channels 0..3 */
#define IO_DMA2_BASE	0xC0	/* 16 bit master DMA, ch 4(=slave input)..7 */

/* DMA controller registers */
#define DMA1_CMD_REG		0x08	/* command register (w) */
#define DMA1_STAT_REG		0x08	/* status register (r) */
#define DMA1_REQ_REG            0x09    /* request register (w) */
#define DMA1_MASK_REG		0x0A	/* single-channel mask (w) */
#define DMA1_MODE_REG		0x0B	/* mode register (w) */
#define DMA1_CLEAR_FF_REG	0x0C	/* clear pointer flip-flop (w) */
#define DMA1_TEMP_REG           0x0D    /* Temporary Register (r) */
#define DMA1_RESET_REG		0x0D	/* Master Clear (w) */
#define DMA1_CLR_MASK_REG       0x0E    /* Clear Mask */
#define DMA1_MASK_ALL_REG       0x0F    /* all-channels mask (w) */

#define DMA2_CMD_REG		0xD0	/* command register (w) */
#define DMA2_STAT_REG		0xD0	/* status register (r) */
#define DMA2_REQ_REG            0xD2    /* request register (w) */
#define DMA2_MASK_REG		0xD4	/* single-channel mask (w) */
#define DMA2_MODE_REG		0xD6	/* mode register (w) */
#define DMA2_CLEAR_FF_REG	0xD8	/* clear pointer flip-flop (w) */
#define DMA2_TEMP_REG           0xDA    /* Temporary Register (r) */
#define DMA2_RESET_REG		0xDA	/* Master Clear (w) */
#define DMA2_CLR_MASK_REG       0xDC    /* Clear Mask */
#define DMA2_MASK_ALL_REG       0xDE    /* all-channels mask (w) */

#define DMA1_ADDR_1              0x00    /* DMA address registers */
#define DMA1_ADDR_2              0x02
#define DMA1_ADDR_3              0x04
#define DMA1_ADDR_4              0x06
#define DMA2_ADDR_1              0xC0
#define DMA2_ADDR_2              0xC4
#define DMA2_ADDR_3              0xC8
#define DMA2_ADDR_4              0xCC

#define DMA1_CNT_1               0x01    /* DMA count registers */
#define DMA1_CNT_2               0x03
#define DMA1_CNT_3               0x05
#define DMA1_CNT_4               0x07
#define DMA2_CNT_1               0xC2
#define DMA2_CNT_2               0xC6
#define DMA2_CNT_3               0xCA
#define DMA2_CNT_4               0xCE

#define DMA1_PAGE_1              0x87    /* DMA page registers */
#define DMA1_PAGE_2              0x83
#define DMA1_PAGE_3              0x81
#define DMA1_PAGE_4              0x82
#define DMA2_PAGE_1              0x8F
#define DMA2_PAGE_2              0x8B
#define DMA2_PAGE_3              0x89
#define DMA2_PAGE_4              0x8A

/* old DMA definitions follow */

EXTERN uint8_t is_dma;     /* Active DMA channels mask */

void dma_controller(void);

/* This is the correct way to run the dma controller */

#define dma_run()  if (is_dma && config.sound == 1) dma_controller()


int dma_test_DACK(int channel);
int dma_test_DREQ(int channel);
int dma_test_eop(int channel);
void dma_drop_eop(int channel);
void dma_assert_eop(int channel);
void dma_drop_DREQ(int channel);
void dma_drop_DACK(int channel);
void dma_assert_DREQ(int channel);
void dma_assert_DACK(int channel);
inline int dma_get_block_size (int channel);
inline int dma_units_left (int channel); /* units are bytes or words */
inline int dma_get_transfer_size (int channel);


#define DMA_HANDLER_READ   1
#define DMA_HANDLER_WRITE  2

#define DMA_HANDLER_DONE   100

#define DMA_HANDLER_OK     1
#define DMA_HANDLER_NOT_OK 0

#define DMA_HANDLER_ERROR  -1

#define MAX_DMA_TRANSFERSIZE 512

void dma_install_handler(int ch, size_t(*read_handler)(void *, size_t),
    size_t(*write_handler)(void *, size_t), void(*DACK_handler)(void),
    void(*EOP_handler)(void));

/* [Re-]Sets the preferred transfer size - Karcher */
void dma_set_transfer_size (int ch, long int size);

/* From <asm/dma.h> */

#define DMA_ADDR_0              0x00    /* DMA address registers */
#define DMA_ADDR_1              0x02
#define DMA_ADDR_2              0x04
#define DMA_ADDR_3              0x06
#define DMA_ADDR_4              0xC0
#define DMA_ADDR_5              0xC4
#define DMA_ADDR_6              0xC8
#define DMA_ADDR_7              0xCC

#define DMA_CNT_0               0x01    /* DMA count registers */
#define DMA_CNT_1               0x03
#define DMA_CNT_2               0x05
#define DMA_CNT_3               0x07
#define DMA_CNT_4               0xC2
#define DMA_CNT_5               0xC6
#define DMA_CNT_6               0xCA
#define DMA_CNT_7               0xCE

#define DMA_PAGE_0              0x87    /* DMA page registers */
#define DMA_PAGE_1              0x83
#define DMA_PAGE_2              0x81
#define DMA_PAGE_3              0x82
#define DMA_PAGE_4              0x8F
#define DMA_PAGE_5              0x8B
#define DMA_PAGE_6              0x89
#define DMA_PAGE_7              0x8A

/* These from Joels dma.h */
/* Values for command register */
#define DMA_MEM_TO_MEM_ENABLE 0x01  /* Not supported */
#define DMA_CH0_ADDR_HOLD     0x02  /* Not supported */
#define DMA_DISABLE           0x04  /* Set to turn off the whole controller */
#define DMA_COMPRESSED_TIMING 0x08  /* Not supported; related to bus timing */
#define DMA_ROTATING_PRIORITY 0x10  /* Set to make the priorty of chs rotate */
#define DMA_EXTENDED_WRITE    0x20  /* Not supported; related to bus timing */
#define DMA_DREQ_LOW          0x40  /* Not supported; hardware */
#define DMA_DACK_HIGH         0x80  /* Not supported; hardware */
/* Let's summarize that: these bits fall into three categories:
1)  Memory to memory transfer, which doesn't seem to be used in the PC
    environment
2)  Various hardware details related to how the controller talks to other
    hardware.  Our psuedo-hardware makes calls to Linux to take care of this,
    so it's irrelevant
3)  Possibly useful bits: shut off the controller and change priorty rules.
    Probably not useful.
I guess a more concise summary would be that this register is system wide
and probably is unimportant since dosemu uses its own bios, not the pc's */


/* Mode register (1 per ch) */
/* We'll start with the low bits and move toward the high ones */

/* The low two bits are the ch selectors */
#define DMA_CH_SELECT         0x03   

/* After that comes the direction of the transfer */
#define DMA_WRITE             0x04  /* This is the write mode */
#define DMA_READ              0x08  /* This is the read mode */
#define DMA_VERIFY            0x00  /* Verify mode, not supported */
#define DMA_INVALID           0x0C  /* Invalid on the real thing */

#define DMA_DIR_MASK          0x0C

/* The next bit causes it to automatically repeat when it's finished */
#define DMA_AUTO_INIT         0x10

/* The next bit is set to decrement the address, otherwise it's incremented */
#define DMA_ADDR_DEC          0x20

/* The mode ie how much to transfer. */
#define DMA_DEMAND_MODE       0x00
#define DMA_SINGLE_MODE       0x40
#define DMA_BLOCK_MODE        0x80
#define DMA_CASCADE_MODE      0xC0

#define DMA_MODE_MASK         0xC0

/* There are several other write registers.  Generally they seem to reuse
DMA_CH_SELECT, and the following: */
#define DMA_SELECT_BIT        0x04

#endif /* DMA_H */
