/* dma.c--dma controller emulater--Joel N. Weber II
 * Copyright (C) 1995  Joel N. Weber II
 * See the file README.dma in this directory for more information
 *
 * DANG_BEGIN_MODULE dma.c
 * This is a DMA controller emulation. It is not complete, and only implements
 * the lower half of the controller. (8-bit channels)
 * It is not maintained by the author.
 *
 * maintainer:
 * Alistair MacDonald <alistair@slitesys.demon.co.uk>
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_REMARK
 *
 * This source file will hopefully eventually be a complete emulation of
 * both DMA controllers.  However, it may not work for a while because this
 * is my first attempt at a real C program.  Also, it seems to be something
 * that's too hard for all the other dosemu people, so I could I possiblly
 * get it to work???
 *
 * Oh well.  I'll dream on.
 *
 * DANG_END_REMARK
 * 
 * DANG_BEGIN_REMARK
 * ***** WARNING *****
 *
 * This code may be changing soon. I've got an alternative driver waiting in
 * the wings. I just need some time to evaluate it. It has an alternative
 * interface for developers.
 *
 * DANG_END_REMARK
 *
 * modified 11/05/95 by Michael Beck
 *  some minor bugs fixed in dma_trans() and other places
 */

#include "emu.h" /* for h_printf */

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/soundcard.h>
#endif
#include "dma.h"
#include "pic.h"
#define DMA_0_3_BASE 0x00     /* The base address for the first dma controller */
#define DMA_4_7_BASE 0xC0     /* The base address for the second */

int dma_ff0, dma_ff1;         /* The high/low byte flip-flop for the dma
                                 controller */
                              /* Currently dma_ff1 is unused */

dma_ch_struct dma_ch[7];


inline void dma_write_addr(unsigned char ch, unsigned char value)
{
  if (dma_ff0) /* Low byte when ff0 clear, high when set */
    {dma_ch[ch].cur_addr = (dma_ch[ch].cur_addr & 0xFF) + value * 256;
     dma_ch[ch].base_addr = (dma_ch[ch].base_addr & 0xFF) + value * 256;
     dma_ff0 = !dma_ff0;}
  else {dma_ch[ch].cur_addr = (dma_ch[ch].cur_addr & 0xFF00) + value;
        dma_ch[ch].base_addr = (dma_ch[ch].base_addr & 0xFF00) + value;
        dma_ff0 = !dma_ff0;};
}

inline void dma_write_count(unsigned char ch, unsigned char value)
{
  if (dma_ff0)
    {dma_ch[ch].cur_count = (dma_ch[ch].cur_count & 0xFF) + value * 256;
     dma_ch[ch].base_count = (dma_ch[ch].base_count & 0xFF) + value * 256;
     dma_ff0 = !dma_ff0;}
  else {dma_ch[ch].cur_count = (dma_ch[ch].cur_count & 0xFF00) + value;
        dma_ch[ch].base_count = (dma_ch[ch].base_addr & 0xFF00) + value;
        dma_ff0 = !dma_ff0;};
}



void dma_write(unsigned int addr, unsigned char value)
{
  h_printf ("DMA (Write): (0x%2X <- 0x%2X)\n", addr, value);

  /* Note that addr must be the absolute port address */
  switch (addr) 
  {
  case 0x87: dma_ch[0].page = value; break;
  case 0x83: dma_ch[1].page = value; break;
  case 0x81: dma_ch[2].page = value; break;
  case 0x82: dma_ch[3].page = value; break;
  case 0x8F: dma_ch[4].page = value; break;
  case 0x8B: dma_ch[5].page = value; break;
  case 0x89: dma_ch[6].page = value; break;
  case 0x8A: dma_ch[7].page = value; break;

    
  case DMA_0_3_BASE     : dma_write_addr (0, value); break;
  case DMA_0_3_BASE +  1: dma_write_count(0, value); break;
  case DMA_0_3_BASE +  2: dma_write_addr (1, value); break;
  case DMA_0_3_BASE +  3: dma_write_count(1, value); break;
  case DMA_0_3_BASE +  4: dma_write_addr (2, value); break;
  case DMA_0_3_BASE +  5: dma_write_count(2, value); break;
  case DMA_0_3_BASE +  6: dma_write_addr (3, value); break;
  case DMA_0_3_BASE +  7: dma_write_count(3, value); break;

  case DMA_0_3_BASE +  8: break; /* Command register; should cause debuging
                                   output but I don't know how */
  case DMA_0_3_BASE +  9: dma_ch[value & DMA_CH_SELECT].request = value & DMA_SINGLE_BIT;
                          break;
  case DMA_0_3_BASE + 10: dma_ch[value & DMA_CH_SELECT].mask = value & DMA_SINGLE_BIT;
                          break;
  case DMA_0_3_BASE + 11: dma_ch[value & DMA_CH_SELECT].mode = value;
                          break;
  case DMA_0_3_BASE + 12: dma_ff0 = 0; break; /* clear flipflop */
  case DMA_0_3_BASE + 13: break; /* master clear; debug??? */
  case DMA_0_3_BASE + 14: break; /* clear mask; debug??? */
  case DMA_0_3_BASE + 15: break; /* Write all mask --debug? */



  case DMA_4_7_BASE     : dma_write_addr (4, value); break;
  case DMA_4_7_BASE +  2: dma_write_count(4, value); break;
  case DMA_4_7_BASE +  4: dma_write_addr (5, value); break;
  case DMA_4_7_BASE +  6: dma_write_count(5, value); break;
  case DMA_4_7_BASE +  8: dma_write_addr (6, value); break;
  case DMA_4_7_BASE + 10: dma_write_count(6, value); break;
  case DMA_4_7_BASE + 12: dma_write_addr (7, value); break;
  case DMA_4_7_BASE + 14: dma_write_count(7, value); break;

  case DMA_4_7_BASE + 16: break; /* Command register; should cause debuging
                                   output but I don't know how */
  case DMA_4_7_BASE + 18: dma_ch[(value & DMA_CH_SELECT) + 4].request = value & DMA_SINGLE_BIT;
                          break; /* request register */
  case DMA_4_7_BASE + 20: dma_ch[(value & DMA_CH_SELECT) + 4].mask = value & DMA_SINGLE_BIT;
                          break; /* single mask */
  case DMA_4_7_BASE + 22: dma_ch[(value & DMA_CH_SELECT) + 4].mode = value;
                          break; /* mode */
  case DMA_4_7_BASE + 24: dma_ff0 = 0; break; /* clear flipflop */
  case DMA_4_7_BASE + 26: break; /* master clear; debug??? */
  case DMA_4_7_BASE + 28: break; /* clear mask; debug??? */
  case DMA_4_7_BASE + 30: break; /* Write all mask; debugging output */
  };
}

inline unsigned char dma_read_addr(unsigned int /*addr*/ch)
{
  if ((dma_ff0 = !dma_ff0))
    {return (dma_ch[ch].cur_addr & 0xFF00) >> 8;}
  else
    {return dma_ch[ch].cur_addr & 0xFF;};
}

inline unsigned char dma_read_count(unsigned int /*addr*/ch)
{
  if ((dma_ff0 = !dma_ff0))
    {return dma_ch[ch].cur_count & 0xFF00 >> 8;}
  else
    {return dma_ch[ch].cur_count & 0xFF;};
}

inline unsigned char DREQ(unsigned char ch)
{
  switch (dma_ch[ch].dreq){
    case DREQ_OFF: return 0;
    case DREQ_ON: return 1;
    case DREQ_COUNTED: if (dma_ch[ch].cur_count)  /* said count before.. */
                         return 1;
                       else
                         return 0;
  }
  return 1;
}

inline unsigned char dma_read_status_0_3()
{
  int value = 0;
  if (dma_ch[0].tc) {value = value + 1;};
  if (dma_ch[1].tc) {value = value + 2;};
  if (dma_ch[2].tc) {value = value + 4;};
  if (dma_ch[3].tc) {value = value + 8;};
  if (DREQ(0)) value += 16;
  if (DREQ(1)) value += 32;
  if (DREQ(2)) value += 64;
  if (DREQ(3)) value += 128;
  return value;
}

inline unsigned char dma_read_status_4_7()
{
  int value = 0;
  if (dma_ch[4].tc) {value = value + 1;};
  if (dma_ch[5].tc) {value = value + 2;};
  if (dma_ch[6].tc) {value = value + 4;};
  if (dma_ch[7].tc) {value += 8;};
  return value;
}

unsigned char dma_read(unsigned int addr)
{
  h_printf ("DMA (read): (0x%2X)\n", addr);

  switch(addr)
  {
  case 0x87: return dma_ch[0].page;
  case 0x83: return dma_ch[1].page;
  case 0x81: return dma_ch[2].page;
  case 0x82: return dma_ch[3].page;
  case 0x8F: return dma_ch[4].page;
  case 0x8B: return dma_ch[5].page;
  case 0x89: return dma_ch[6].page;
  case 0x8A: return dma_ch[7].page;
  
  case DMA_0_3_BASE     : return dma_read_addr(0);
  case DMA_0_3_BASE +  1: return dma_read_count(0);
  case DMA_0_3_BASE +  2: return dma_read_addr(1);
  case DMA_0_3_BASE +  3: return dma_read_count(1);
  case DMA_0_3_BASE +  4: return dma_read_addr(2);
  case DMA_0_3_BASE +  5: return dma_read_count(2);
  case DMA_0_3_BASE +  6: return dma_read_addr(3);
  case DMA_0_3_BASE +  7: return dma_read_count(3);

  case DMA_0_3_BASE +  8: return dma_read_status_0_3();

  case DMA_4_7_BASE     : return dma_read_addr(4);
  case DMA_4_7_BASE +  2: return dma_read_count(4);
  case DMA_4_7_BASE +  4: return dma_read_addr(5);
  case DMA_4_7_BASE +  6: return dma_read_count(5);
  case DMA_4_7_BASE +  8: return dma_read_addr(6);
  case DMA_4_7_BASE + 10: return dma_read_count(6);
  case DMA_4_7_BASE + 12: return dma_read_addr(7);
  case DMA_4_7_BASE + 14: return dma_read_count(7);

  case DMA_4_7_BASE + 16: return dma_read_status_4_7();
  default: return 0;
  };
}

/*
 * DANG_BEGIN_REMARK
 * 
 * dma_trans handles the actual work of copying data to the device.  I'm not
 * sure whether it's called at a good frequency.
 *
 * DANG_END_REMARK
 */
void dma_trans()
{
  unsigned char cur_ch;
  unsigned int max_bytes, l=0;

  /*
     only the first dma-controller, as the higher channels need some
     other constants; not needed now
   */
  for (cur_ch = 0; cur_ch < 4; ++cur_ch)
  {
    if (!dma_ch[cur_ch].mask) {
    if (/* We want to transfer data */
         (dma_ch[cur_ch].dreq == DREQ_COUNTED && dma_ch[cur_ch].dreq_count)
           || (dma_ch[cur_ch].dreq == DREQ_ON)
           || (dma_ch[cur_ch].request)
       )
         { /* Okay, we have some work to do... */
           max_bytes = dma_ch[cur_ch].cur_count + 1;
           if (max_bytes > dma_ch[cur_ch].dreq_count)
	     max_bytes = dma_ch[cur_ch].dreq_count;

           if (max_bytes + dma_ch[cur_ch].cur_addr > 0x010000)
             max_bytes = 0x010000 - dma_ch[cur_ch].cur_addr;

	   h_printf("DMA%d from 0x%X 0x%X\n", cur_ch,
	            (dma_ch[cur_ch].page << 16) | dma_ch[cur_ch].cur_addr,
		    max_bytes);

           if (dma_ch[cur_ch].mode & DMA_WRITE) {
             h_printf ("DMA: Write transfer\n");
             l = write(dma_ch[cur_ch].fd,
	              (char *)((dma_ch[cur_ch].page << 16) |\
			       dma_ch[cur_ch].cur_addr),
                       max_bytes);
           }
           else if (dma_ch[cur_ch].mode & DMA_READ) {
	     h_printf("DMA: Read transfer\n");
	     l = read(dma_ch[cur_ch].fd,
                          (char *) (((dma_ch[cur_ch].page * 0x010000)
                            + dma_ch[cur_ch].cur_addr)),
                          max_bytes);
	   }
	   if (l == -1) {
	     /*
	      * I get error 4 (syscall interupted) all over the time
	      * although the bytes seems to be outputed, so it's ignored
	      * yet -- XXX FIXME
	      */
	     h_printf("ignoring device error %d\n", errno);
	     max_bytes = 0;
	   }
	   else
	     max_bytes = l;
	   h_printf("DMA: transfered %d bytes\n", max_bytes);
           dma_ch[cur_ch].cur_count -= max_bytes;
           if (dma_ch[cur_ch].cur_count == -1) {
	      dma_ch[cur_ch].tc = 1;
              if (dma_ch[cur_ch].tc_irq != -1) {
	        h_printf("DMA: TC triggers IRQ%d\n", dma_ch[cur_ch].tc_irq);
	        pic_request(dma_ch[cur_ch].tc_irq);
	      }
	   }
           if (!(dma_ch[cur_ch].dreq_count -= max_bytes)) {
             if (dma_ch[cur_ch].dreq_irq != -1) {
	       h_printf("DMA: DREQ triggers IRQ%d\n", dma_ch[cur_ch].dreq_irq);
	       pic_request(dma_ch[cur_ch].dreq_irq);
	     }
	   }
           dma_ch[cur_ch].cur_addr += max_bytes;
           if (dma_ch[cur_ch].tc && (dma_ch[cur_ch].mode & DMA_AUTO_INIT)){
             dma_ch[cur_ch].cur_addr = dma_ch[cur_ch].base_addr;
             dma_ch[cur_ch].cur_count = dma_ch[cur_ch].base_count;
             dma_ch[cur_ch].tc = 0;
           }
         }
      }	 
  }
}

void dma_init(void)
{
  int ch;

  for (ch = 0; ch < 8; ++ch) {
    dma_ch[ch].request  = 0;
    dma_ch[ch].dreq     = DREQ_OFF;
    dma_ch[ch].tc       = -1;
    dma_ch[ch].dreq_irq = -1;
    dma_ch[ch].mask     = 1;
  }
}
