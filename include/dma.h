/* DANG_BEGIN_MODULE
 *
 * include/dma.h defines the dma_struct, the type which holds all the
 * information needed by the dma controller.
 *
 * DANG_END_MODULE
 */



typedef struct {
  unsigned char page;               /* The high byte to use always for this ch */
  unsigned int cur_addr;            /* This is the low word of the current address. */
  unsigned int cur_count;           /* The number of bytes - 1 to transfer */
  unsigned int base_addr;           /* The address  and count to load when the */
  unsigned int base_count;          /* transfer is finished */
  unsigned char mode;               /* The mode for the channel */
  unsigned char request;            /* = 4 when dma requested by software */
  unsigned char mask;               /* = 4 to disable ch */
  unsigned char tc;                 /* = 4 when terminal count is reached */
  /* the preceding three will always be either 0 or 4 */

  /* The following aren't registers in the real thing but we need them in our
     psuedo-dma controller.  If you are writing code for a dma device, you
     need to fill in all these fields yourself. */
  int fd;                           /* The file descriptor to use for this ch */
  int tc_irq, dreq_irq;             /* The irq level to call when the transfer
                                       is over and the dreq_count becomes 0.
                                       This must be an interrupt level defined
                                       in ../include/pic.h.  Or use -1 to
                                       disable */
  int dreq;                         /* One of the following: */
#define DREQ_OFF 0
#define DREQ_ON  1
#define DREQ_COUNTED 2
  unsigned long int dreq_count;     /* max bytes to transfer when dreq = 2 */
  } dma_ch_struct;



extern dma_ch_struct dma_ch[7];     /* This is the same as dma_ch : array[0..7]
                                       of dma_ch_struct in Pascal, right??? */


/* Constants follow representing the values for various registers */

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

/* I've decided there's no need to support this, but I left it anyway to
   document what's going on. */

/* Mode register (1 per ch) */
/* We'll start with the low bits and move toward the high ones */

/* The low two bits are the ch selectors */
#define DMA_CH_SELECT         0x03   

/* After that comes the direction of the transfer */
#define DMA_WRITE             0x04  /* This is the write mode */
#define DMA_READ              0x08  /* This is the read mode */
#define DMA_VERIFY            0x00  /* Verify mode, not supported */
#define DMA_INVALID           0x0C  /* Invalid on the real thing */

/* The next bit causes it to automatically go on to a different address
when it's finished */
#define DMA_AUTO_INIT         0x10

/* The next bit is set to decrement the address, otherwise it's incremented */
#define DMA_ADDR_DEC          0x20

/* The mode ie how much to transfer.  Note that this is ignored. */
#define DMA_DEMAND_MODE       0x00
#define DMA_SINGLE_MODE       0x40
#define DMA_BLOCK_MODE        0x80
#define DMA_CASCADE_MODE      0xC0


/* There are several other write registers.  Generally they seem to reuse
DMA_CH_SELECT, and the following: */
#define DMA_SINGLE_BIT        0x04

void dma_write(unsigned int addr, unsigned char value);
unsigned char dma_read(unsigned int addr);
void dma_trans();

