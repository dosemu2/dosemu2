/* 
 ******** OLD_DMA_CODE COPYRIGHT
 * dma.c--dma controller emulater--Joel N. Weber II
 * Copyright (C) 1995  Joel N. Weber II
 * See the file README.dma in this directory for more information
 ********
 *
 * DANG_BEGIN_MODULE dma.c
 * This is a DMA controller emulation. It is not complete, and only implements
 * the lower half of the controller. (8-bit channels)
 *
 * maintainer:
 * Alistair MacDonald <alistair@slitesys.demon.co.uk>
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_REMARK
 * **** WARNING ****
 * This Code _HAS_ changed.
 * DANG_END_REMARK
 *
 * modified 11/05/95 by Michael Beck
 *  some minor bugs fixed in dma_trans() and other places (Old Code - AM)
 */

#include "emu.h" /* for h_printf */

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "dma.h"
#include "pic.h"
#include "port.h"

#ifndef OLD_DMA_CODE
#include <unistd.h>

typedef struct {
  Bit8u lsb;
  Bit8u msb;
} msblsb_t;

typedef union {
  Bit8u data[2];
  msblsb_t bits;
  Bit16u value;
} multi_t;

typedef struct {
  int     wfd;           /* Write part of pipe */
  int     rfd;           /* Read part of pipe */
  int     dack;          /* DACK */
  int     run;           /* Running */
  int     eop;           /* End Of Process */
  int     size;          /* Preferred Transfer Size */
  void    (* handler)(int);  /* Handler Function */
} internal_t;

typedef struct {
  multi_t address[4];    /* Address for channel */
  multi_t length[4];     /* Length of transfer */
  Bit8u   status;        /* Status of Controller */
  Bit8u   ch_config[4];  /* Configuration of channel */
  Bit8u   ch_mode[4];    /* Mode of the channel */
  Bit8u   page[4];       /* Page address for channel */
  int     ff;            /* High/Low order bit Flip/Flop */
  int     current_ch;    /* Which is being referred to */

/* Emulation Specific */
  internal_t i[4];       /* Internals */
} dma_t;

static  dma_t dma[2];    /* DMA controllers */

/*
 * DANG_BEGIN_REMARK
 *
 * The Emulated DMA channels are provided by using files and writes.
 * This means that they are easy to track.
 * It might cause problems when attempting to interface to the REAL DMA
 * controller. (Necessary to talk to hardware which uses DMA.)
 *
 * Note that DMA controller 2 uses word granular addressing and controller 1
 * uses byte granular address ... this simplifies the code !
 *
 * DANG_END_REMARK
 */

#define DMA1  0
#define DMA2  1

Bit8u dma_io_read(Bit32u port);
void dma_io_write(Bit32u port, Bit8u value);

inline void dma_toggle_ff(int dma_c);
inline void dma_write_mask (int dma_c, Bit8u value);
inline void dma_write_count (int dma_c, int channel, Bit8u value);
inline void dma_write_addr (int dma_c, int channel, Bit8u value);
inline Bit8u dma_read_count (int dma_c, int channel);
inline Bit8u dma_read_addr (int dma_c, int channel);
inline Bit32u *create_addr (Bit8u page, Bit8u addr_msb, Bit8u addr_lsb, 
			   int dma_c);

int dma_do_read (int controller, int channel, Bit32u *target_addr);
int dma_do_write (int controller, int channel, Bit32u *target_addr);


/* PUBLIC CODE */

void dma_assert_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Asserting DACK on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].dack = 1;
}

void dma_assert_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Asserting DREQ on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch + 4);

  dma[controller].status |= mask;
}

void dma_drop_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Dropping DACK on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].dack = 0;
}

void dma_drop_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Dropping DREQ on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch +4);
  dma[controller].status &= ~ mask;
}

void dma_assert_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Asserting EOP on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].eop = 1;
}

void dma_drop_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Dropping EOP on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  dma[controller].i[ch].eop = 0;
}

int dma_test_DACK(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Testing DACK on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  return dma[controller].i[ch].dack;
}

int dma_test_DREQ(int channel)
{
  int controller, ch;
  Bit8u mask;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Testing DREQ on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  mask = 1 << (ch + 4);

  return dma[controller].status & mask;
}

int dma_test_eop(int channel)
{
  int controller, ch;

#ifdef EXCESSIVE_DEBUG
  h_printf ("DMA: Testing EOP on channel %d\n", channel);
#endif /* EXCESSIVE_DEBUG */

  controller = channel & 4;
  ch = channel & DMA_CH_SELECT;

  return dma[controller].i[ch].eop;
}




/* PRIVATE CODE */

inline Bit32u *create_addr (Bit8u page, Bit8u addr_msb, Bit8u addr_lsb, 
			   int dma_c)
{
  if (dma_c == DMA1)
    /* 64K page */
    return (Bit32u *) ((page & 0x15) << 16) + (addr_msb << 8) + addr_lsb;
  else
    /* 128K page */
    return (Bit32u *) (page << 17) + (addr_msb << 9) + (addr_lsb << 1);
}



inline void dma_toggle_ff(int dma_c)
{
  dma[dma_c].ff = !dma[dma_c].ff;
}


inline Bit8u dma_read_addr (int dma_c, int channel)
{
  Bit8u r;

  r = dma[dma_c].address[channel].data[dma[dma_c].ff];

  h_printf ("DMA: Read %u from Channel %d Address (Byte %d)\n", r,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);

  return r;
}

inline Bit8u dma_read_count (int dma_c, int channel)
{
  Bit8u r;

  r = dma[dma_c].length[channel].data[dma[dma_c].ff];

  h_printf ("DMA: Read %u from Channel %d Length (Byte %d)\n", r,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);

  return r;
}


Bit8u dma_io_read(Bit32u port)
{
  Bit8u r;

  r = 0;

  switch (port) 
  {
  case DMA_ADDR_0:
    r = dma_read_addr(DMA1, 0);
    break;
  case DMA_ADDR_1:
    r = dma_read_addr(DMA1, 1);
    break;
  case DMA_ADDR_2:
    r = dma_read_addr(DMA1, 2);
    break;
  case DMA_ADDR_3:
    r = dma_read_addr(DMA1, 3);
    break;
  case DMA_ADDR_4:
    r = dma_read_addr(DMA2, 0);
    break;
  case DMA_ADDR_5:
    r = dma_read_addr(DMA2, 1);
    break;
  case DMA_ADDR_6:
    r = dma_read_addr(DMA2, 2);
    break;
  case DMA_ADDR_7:
    r = dma_read_addr(DMA2, 3);
    break;

  case DMA_CNT_0:
    r = dma_read_count(DMA1, 0);
    break;
  case DMA_CNT_1:
    r = dma_read_count(DMA1, 1);
    break;
  case DMA_CNT_2:
    r = dma_read_count(DMA1, 2);
    break;
  case DMA_CNT_3:
    r = dma_read_count(DMA1, 3);
    break;
  case DMA_CNT_4:
    r = dma_read_count(DMA2, 0);
    break;
  case DMA_CNT_5:
    r = dma_read_count(DMA2, 1);
    break;
  case DMA_CNT_6:
    r = dma_read_count(DMA2, 2);
    break;
  case DMA_CNT_7:
    r = dma_read_count(DMA2, 3);
    break;

  case DMA1_STAT_REG:
    r = dma[DMA1].status;
    h_printf ("DMA: Read %u from DMA1 Status\n", r);
    break;
  case DMA2_STAT_REG:
    r = dma[DMA2].status;
    h_printf ("DMA: Read %u from DMA2 Status\n", r);
    break;

  default:
    h_printf ("DMA: Unhandled Read on 0x%04lx\n", port);
  }

  return r;
}


inline void dma_write_addr (int dma_c, int channel, Bit8u value)
{
  dma[dma_c].address[channel].data[dma[dma_c].ff] = value;

  h_printf ("DMA: Wrote %u into Channel %d Address (Byte %d)\n", value,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);
}

inline void dma_write_count (int dma_c, int channel, Bit8u value)
{
  dma[dma_c].length[channel].data[dma[dma_c].ff] = value;

  h_printf ("DMA: Wrote %u into Channel %d Length (Byte %d)\n", value,
	    (dma_c *4) + channel, dma[dma_c].ff);

  dma_toggle_ff (dma_c);
}

inline void dma_write_mask (int dma_c, Bit8u value)
{
    if (value & DMA_SELECT_BIT)
    {
      dma[dma_c].current_ch = (int) value & 3;
      h_printf ("DMA: Channel %u selected.\n", 
		(dma_c *4) + dma[dma_c].current_ch);
    }
    else
    {
      h_printf ("DMA: Channel %u deselected.\n", 
		(dma_c *4) + dma[dma_c].current_ch);

      is_dma |= (1 << dma[dma_c].current_ch);

      dma[dma_c].current_ch = -1;
    }
}




void dma_io_write(Bit32u port, Bit8u value)
{
  Bit8u ch;

  switch (port) 
  {
  case DMA_ADDR_0:
    dma_write_addr (DMA1, 0, value);
    break;
  case DMA_ADDR_1:
    dma_write_addr (DMA1, 1, value);
    break;
  case DMA_ADDR_2:
    dma_write_addr (DMA1, 2, value);
    break;
  case DMA_ADDR_3:
    dma_write_addr (DMA1, 3, value);
    break;
  case DMA_ADDR_4:
    dma_write_addr (DMA2, 0, value);
    h_printf ("DMA: That last write was a little dubious ....\n");
    break;
  case DMA_ADDR_5:
    dma_write_addr (DMA2, 1, value);
    break;
  case DMA_ADDR_6:
    dma_write_addr (DMA2, 2, value);
    break;
  case DMA_ADDR_7:
    dma_write_addr (DMA2, 3, value);
    break;
  case DMA_CNT_0:
    dma_write_count (DMA1, 0, value);
    break;
  case DMA_CNT_1:
    dma_write_count (DMA1, 1, value);
    break;
  case DMA_CNT_2:
    dma_write_count (DMA1, 2, value);
    break;
  case DMA_CNT_3:
    dma_write_count (DMA1, 3, value);
    break;
  case DMA_CNT_4:
    dma_write_count (DMA2, 0, value);
    break;
  case DMA_CNT_5:
    dma_write_count (DMA2, 1, value);
    break;
  case DMA_CNT_6:
    dma_write_count (DMA2, 2, value);
    break;
  case DMA_CNT_7:
    dma_write_count (DMA2, 3, value);
    break;
  case DMA_PAGE_0:
    dma[DMA1].page[0] = value;
    h_printf ("DMA: Write %u to Channel0 (Page)\n", value);
    break;
  case DMA_PAGE_1:
    dma[DMA1].page[1] = value;
    h_printf ("DMA: Write %u to Channel1 (Page)\n", value);
    break;
  case DMA_PAGE_2:
    dma[DMA1].page[2] = value;
    h_printf ("DMA: Write %u to Channel2 (Page)\n", value);
    break;
  case DMA_PAGE_3:
    dma[DMA1].page[3] = value;
    h_printf ("DMA: Write %u to Channel3 (Page)\n", value);
    break;
  /* DMA_PAGE_4 is not useable - its the cascade channel ... */
  case DMA_PAGE_5:
    dma[DMA2].page[5] = value;
    h_printf ("DMA: Write %u to Channel5 (Page)\n", value);
    break;
  case DMA_PAGE_6:
    dma[DMA2].page[6] = value;
    h_printf ("DMA: Write %u to Channel6 (Page)\n", value);
    break;
  case DMA_PAGE_7:
    dma[DMA2].page[7] = value;
    h_printf ("DMA: Write %u to Channel7 (Page)\n", value);
    break;

  case DMA1_MASK_REG:
    dma_write_mask(DMA1, value);
    break;
  case DMA2_MASK_REG:
    dma_write_mask(DMA2, value);
    break;

  case DMA1_MODE_REG:
    ch = value & DMA_CH_SELECT;
    dma[DMA1].ch_mode[ch] = value - ch;
    h_printf ("DMA: Write 0x%x to Channel %u mode\n", value - ch, ch);
    break;
  case DMA2_MODE_REG:
    ch = value & DMA_CH_SELECT;
    dma[DMA2].ch_mode[ch] = value - ch;
    h_printf ("DMA: Write 0x%x to Channel %u mode\n", value - ch, ch + 4);
    break;

  case DMA1_CMD_REG:
    dma[DMA1].ch_config[dma[DMA1].current_ch] = value;;
    h_printf ("DMA: Write 0x%x to DMA1 Command\n", value);
    break;
  case DMA2_CMD_REG:
    dma[DMA2].ch_config[dma[DMA2].current_ch] = value;;
    h_printf ("DMA: Write 0x%x to DMA2 Command\n", value);
    break;

  case DMA1_CLEAR_FF_REG:
    h_printf ("DMA: Clearing DMA1 Output FF\n");
    dma[DMA1].ff = value & 1;  /* Kernel implies this, then ignores it */
    break;
  case DMA2_CLEAR_FF_REG:
    h_printf ("DMA: Clearing DMA2 Output FF\n");
    dma[DMA2].ff = value & 1;  /* Kernel implies this, then ignores it */
    break;

  default:
    h_printf ("DMA: Unhandled Write on 0x%04lx\n", port);
  }
}



void dma_install_handler (int ch, int wfd, int rfd, void (* handler) (int), 
			  int size)
{
  int channel, dma_c;

  h_printf ("DMA: Installing DMA Handler on channel %d [%d bytes/call]\n", 
	    ch, size);

  dma_c = ch & 4;
  channel = ch & DMA_CH_SELECT;

  if (dma[dma_c].i[channel].wfd != -1)
    close (dma[dma_c].i[channel].wfd);
  dma[dma_c].i[channel].wfd = wfd;

  if (dma[dma_c].i[channel].rfd != -1)
    close (dma[dma_c].i[channel].rfd);
  dma[dma_c].i[channel].rfd = rfd;

  dma[dma_c].i[channel].handler = handler;

  dma[dma_c].i[channel].size = size;
}



/*
 * This is the brains of the operation ....
 */

void dma_controller (void)
{
  Bit8u test;
  Bit32u *target_addr;
  int controller, channel, ch;
  int tmp_pipe[2];
  int amount_done = 0;

  for (test = 1, controller = DMA1, channel = 0, ch = 0; 
       test != 0; 
       test = test << 1, channel++, ch++)
  {
    if (channel == 4)
    {
      controller = DMA2;
      channel = 0;
    }

    /* Process the channel only if it has been deselected */
    if ((is_dma & test) && (dma[controller].current_ch != channel))
    {
      /* Time to process this DMA channel */
      h_printf ("DMA: processing controller %d, channel %d\n", controller +1, 
		ch);

      /* Have we started using it yet ? */
      if ((dma[controller].i[channel].wfd == -1) 
	  && (dma[controller].i[channel].rfd == -1))
      {
	/* Neither part is open - lets set up the pipe */
	h_printf ("DMA: Initialising transfer for channel %d, controller %d\n",
		  ch, controller +1);

	if (pipe(tmp_pipe))
	{
	  /* Failed to create the pipe */
	  h_printf ("DMA: Failed to intialise transfer for channel %d on controller %d\n", 
		    ch, controller + 1);

	  /* FIXME: Lets hope that it times out ! */
	  is_dma &= ~ test;
	  return;
	}

	dma[controller].i[channel].wfd = tmp_pipe[0];
	dma[controller].i[channel].rfd = tmp_pipe[1];

	dma_assert_DACK (ch);  /* Force DACK - makes the logic easier */
;
	/* 
	 * DANG_BEGIN_REMARK
	 * I think that DREQ should only be set on auto-init if we are the 
	 * reading portion.
	 * DANG_END_REMARK
	 */

/*	if (dma[controller].ch_mode[channel] & (DMA_AUTO_INIT | DMA_READ))
	  dma_assert_DREQ (ch);
	else
*/	  dma_drop_DREQ (ch);

      }

      /* Should now have a valid set up */
      
      /* 
       * Telling the DMA controller to READ, means that you want to read
       * from the address, so we actually need to write !
       */

      target_addr = create_addr (dma[controller].page[channel],
				 dma[controller].address[channel].bits.msb,
				 dma[controller].address[channel].bits.lsb,
				 controller);

      switch (dma[controller].ch_mode[channel] & DMA_DIR_MASK)
      {
      case DMA_WRITE:
	switch (dma[controller].ch_mode[channel] & DMA_MODE_MASK)
	{
	case DMA_DEMAND_MODE:
	  if (dma_test_DACK(ch))
	    dma_drop_DACK(ch);
	  else
	    return;

	  if (! dma[controller].length[channel].value) 
	  { 
	    /* Transfer is complete */ 
	    dma[controller].status |= (1 << channel);
/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1; 
*/	    is_dma &= ~test ;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	    return; 
	  }

	  if (dma_test_eop(ch))
	  {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);
/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1;
*/	    is_dma &= ~ test;
	    return;
	  }

	  if (dma_test_DREQ(ch))
	  {
	    dma[controller].i[channel].run = 1; /* TRIVIAL */

	    if (dma[controller].length[channel].value
		< dma[controller].i[channel].size)
	      dma[controller].i[channel].size 
		= dma[controller].length[channel].value;

 	    amount_done = dma_do_read(controller, channel, target_addr);

	    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC)
	      dma[controller].address[channel].value 
		-= amount_done;
	    else
	      dma[controller].address[channel].value
		+= amount_done;

	    dma[controller].length[channel].value 
	      -= amount_done;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_WRITE);
	  }

	  break;

	case DMA_SINGLE_MODE:
	  if (! dma[controller].length[channel].value)
	  {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);
/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1;
*/	    is_dma &= ~test ;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	    return;
	  }

	  if (((!dma[controller].i[channel].run) && dma_test_DREQ(ch))
	      || (dma_test_DREQ(ch) && dma_test_DACK(ch)))
	  {
	    if (! dma[controller].i[channel].run)
	      dma[controller].i[channel].run = 1;
	    else
	      dma_drop_DACK(ch);

	    if (dma[controller].length[channel].value
		< dma[controller].i[channel].size)
	      dma[controller].i[channel].size 
		= dma[controller].length[channel].value;

 	    amount_done = dma_do_read(controller, channel, target_addr);

	    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC)
	      dma[controller].address[channel].value 
		-= amount_done;
	    else
	      dma[controller].address[channel].value 
		+= amount_done;

	    dma[controller].length[channel].value 
	      -= amount_done;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_WRITE);
	  }

 	  break;

	case DMA_BLOCK_MODE:
	  if (dma_test_DACK(ch))
	    dma_drop_DACK(ch);
	  else
	    return;

	  if (!dma[controller].length[channel].value)
	  {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);
/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1;
*/	    is_dma &= ~test ;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	    return;
	  }

	  if (dma_test_eop(ch))
	  {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);
/*	    close (dma[controller].i[channel].wfd);
	    dma[controller].i[channel].wfd = -1;
*/	    is_dma &= ~ test;
	    return;
	  }

	  if ((! dma[controller].i[channel].run) && (dma_test_DREQ(ch)))
	    dma[controller].i[channel].run = 1; /* Important ! */

	  if (dma[controller].i[channel].run)
	  {
	    if (dma[controller].length[channel].value
		< dma[controller].i[channel].size)
	      dma[controller].i[channel].size 
		= dma[controller].length[channel].value;

 	    amount_done = dma_do_read(controller, channel, target_addr);

	    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC)
	      dma[controller].address[channel].value 
		-= amount_done;
	    else
	      dma[controller].address[channel].value 
		+= amount_done;

	    dma[controller].length[channel].value 
	      -= amount_done;
	    
	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_WRITE);

	  }
	  break;

	case DMA_CASCADE_MODE:
	  /* Not Supported */
	  h_printf ("DMA: Attempt to use unsupported CASCADE mode\n");
/*	  close (dma[controller].i[channel].wfd);
	  dma[controller].i[channel].wfd = -1;
*/	  is_dma &= ~ test;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	  break;
	};
	break;

      case DMA_READ:

	switch (dma[controller].ch_mode[channel] & DMA_MODE_MASK)
	{
	case DMA_DEMAND_MODE:
	  if (dma_test_DACK(ch))
	    dma_drop_DACK(ch);
	  else
	    return;

	  if (! dma[controller].length[channel].value)
	  {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);
/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
*/	    is_dma &= ~test ;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	    return;
	  }

	  if (dma_test_eop(ch))
	  {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);
/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
*/	    is_dma &= ~ test;
	    return;
	  }

	  if (dma_test_DREQ(ch))
	  {
	    dma[controller].i[channel].run = 1; /* TRIVIAL */

	    if (dma[controller].length[channel].value
		< dma[controller].i[channel].size)
	      dma[controller].i[channel].size 
		= dma[controller].length[channel].value;

 	    amount_done = dma_do_write(controller, channel, target_addr);

	    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC)
	      dma[controller].address[channel].value 
		-= amount_done;
	    else
	      dma[controller].address[channel].value 
		+= amount_done;

	    dma[controller].length[channel].value 
	      -= amount_done;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_READ);

	  }

	  break;

	case DMA_SINGLE_MODE:
	  h_printf ("DMA: Single Mode Read - length %d\n", dma[controller].length[channel].value);
	  if (! dma[controller].length[channel].value)
	  {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);
/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
*/	    is_dma &= ~test ;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	    return;
	  }

	  if ((!dma[controller].i[channel].run && dma_test_DREQ(ch))
	      || (dma_test_DREQ(ch) && dma_test_DACK(ch)))
	  {
	    if (! dma[controller].i[channel].run)
	      dma[controller].i[channel].run = 1;
	    else
	      dma_drop_DACK(ch);

	    if (dma[controller].length[channel].value
		< dma[controller].i[channel].size)
	      dma[controller].i[channel].size 
		= dma[controller].length[channel].value;

 	    amount_done = dma_do_write(controller, channel, target_addr);

	    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC)
	      dma[controller].address[channel].value 
		-= amount_done;
	    else
	      dma[controller].address[channel].value 
		+= amount_done;

	    dma[controller].length[channel].value 
	      -= amount_done;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_READ);
	  }

 	  break;

	case DMA_BLOCK_MODE:
	  if (dma_test_DACK(ch))
	    dma_drop_DACK(ch);
	  else
	    return;

	  if (!dma[controller].length[channel].value)
	  {
	    /* Transfer is complete */
	    dma[controller].status |= (1 << channel);
/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
*/	    is_dma &= ~test ;

	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	    return;
	  }

	  if (dma_test_eop(ch))
	  {
	    /* Stop the Transfer */
	    dma[controller].status &= (1 << channel);
/*	    close (dma[controller].i[channel].rfd);
	    dma[controller].i[channel].rfd = -1;
*/	    is_dma &= ~ test;
	    return;
	  }

	  if ((! dma[controller].i[channel].run) && (dma_test_DREQ(ch)))
	    dma[controller].i[channel].run = 1; /* Essential ! */

	  if (dma[controller].i[channel].run)
	  {
	    if (dma[controller].length[channel].value
		< dma[controller].i[channel].size)
	      dma[controller].i[channel].size 
		= dma[controller].length[channel].value;

 	    amount_done = dma_do_write(controller, channel, target_addr);

	    if (dma[controller].ch_mode[channel] & DMA_ADDR_DEC)
	      dma[controller].address[channel].value 
		-= amount_done;
	    else
	      dma[controller].address[channel].value 
		+= amount_done;

	    dma[controller].length[channel].value 
	      -= amount_done;
	    
	    if (dma[controller].i[channel].handler != NULL)
	      dma[controller].i[channel].handler (DMA_HANDLER_READ);
	  }
	  break;

	case DMA_CASCADE_MODE:
	  /* Not Supported */
	  h_printf ("DMA: Attempt to use unsupported CASCADE mode\n");
/*	  close (dma[controller].i[channel].rfd);
	  dma[controller].i[channel].rfd = -1;
*/	  is_dma &= ~ test;

	  if (dma[controller].i[channel].handler != NULL)
	    dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	  break;
	};
	break;

      case DMA_VERIFY:
	/* Not Supported */
 	h_printf ("DMA: Attempt to use unsupported VERIFY direction by controller %d, channel %d\n",
		  controller +1, channel);
	close (dma[controller].i[channel].rfd);
	dma[controller].i[channel].rfd = -1;
	close (dma[controller].i[channel].rfd);
	dma[controller].i[channel].rfd = -1;
	is_dma &= ~ test;

	if (dma[controller].i[channel].handler != NULL)
	  dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	break;

      case DMA_INVALID:
	/* Not Supported */
 	h_printf ("DMA: Attempt to use INVALID direction by controller %d, channel %d\n",
		  controller +1, channel);
	close (dma[controller].i[channel].rfd);
	dma[controller].i[channel].rfd = -1;
	close (dma[controller].i[channel].rfd);
	dma[controller].i[channel].rfd = -1;
	is_dma &= ~ test;

	if (dma[controller].i[channel].handler != NULL)
	  dma[controller].i[channel].handler (DMA_HANDLER_DONE);

	break;
	
      };
    }
  }
}


int dma_do_write (int controller, int channel, Bit32u *target_addr)
{
  size_t ret_val;

  ret_val = write(dma[controller].i[channel].rfd, target_addr,
		  dma[controller].i[channel].size << controller);

  if (ret_val == -1) {
    h_printf ("DMA: Error in READ on Channel %d of controller %d (%s)\n", 
	      channel, controller, strerror(errno));
    return 0;
  }
  else {
    return ret_val;
  }
}

int dma_do_read (int controller, int channel, Bit32u *target_addr)
{
  size_t ret_val;

  ret_val = read(dma[controller].i[channel].wfd, target_addr,
		  dma[controller].i[channel].size << controller);

  if (ret_val == -1) {
    h_printf ("DMA: Error in WRITE on Channel %d of controller %d (%s)\n", 
	      channel, controller, strerror(errno));  
    return 0;
  }
  else {
    return ret_val;
  }
}

void dma_init(void)
{
  emu_iodev_t  io_device;
  int i, j;
  
  /* 8237 DMA controller */
  io_device.read_portb   = dma_io_read;
  io_device.write_portb  = dma_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.irq          = EMU_NO_IRQ;

  /*
   * XT Controller 
   */
  io_device.start_addr   = 0x0000;
  io_device.end_addr     = 0x000F;
  io_device.handler_name = "DMA - XT Controller";
  port_register_handler(io_device);

  /*
   * Page Registers (XT)
   */
  io_device.start_addr   = 0x0081;
  io_device.end_addr     = 0x0087;
  io_device.handler_name = "DMA - XT Pages";
  port_register_handler(io_device);

  /*
   * AT Controller
   */
  io_device.start_addr   = 0x00C0;
  io_device.end_addr     = 0x00DE;
  io_device.handler_name = "DMA - AT Controller";
  port_register_handler(io_device);

  /*
   * Page Registers (AT)
   */
  io_device.start_addr   = 0x0089;
  io_device.end_addr     = 0x008A;
  io_device.handler_name = "DMA - AT Pages";
  port_register_handler(io_device);

  for (i = 0; i < 2; i++)
  {
    for (j = 0 ; j < 5; j++)
    {
      dma[i].address[j].value = 0;
      dma[i].length[j].value = 0;
      dma[i].ch_config[j] = 0;
      dma[i].page[j] = 0;

      dma[i].i[j].wfd = -1;
      dma[i].i[j].rfd = -1;      	

      dma[i].i[j].size = 1;

      dma[i].i[j].run = 0;
      dma[i].i[j].handler = NULL;
    }

    dma[i].status = 0;
    dma[i].ff = 0;
    dma[i].current_ch = 0;
  }

  is_dma = 0;

  h_printf ("DMA: DMA Controller initialiased - 8 & 16 bit modes\n");
}
  

void dma_reset(void)
{
  int i, j;
  for (i = 0; i < 2; i++)
  {
    for (j = 0 ; j < 4; j++)
    {
      dma[i].address[j].value = 0;
      dma[i].length[j].value = 0;
      dma[i].ch_config[j] = 0;
      dma[i].page[j] = 0;

      if (dma[i].i[j].wfd != -1) 
      {
	h_printf ("DMA: Closing Write File for Controller %d Channel %d\n",
		  i, j);
	close (dma[i].i[j].wfd);
      }
      if (dma[i].i[j].rfd != -1)
      {
	h_printf ("DMA: Closing Read File for Controller %d Channel %d\n",
		  i, j);
	close (dma[i].i[j].rfd);
      }

      dma[i].i[j].wfd = -1;
      dma[i].i[j].rfd = -1;      	

      dma[i].i[j].size = 1;

      dma[i].i[j].run = 0;
      dma[i].i[j].handler = NULL;
    }

    dma[i].status = 0;
    dma[i].ff = 0;
    dma[i].current_ch = 0;
  }

  is_dma = 0;

  h_printf ("DMA: DMA Controller Reset - 8 & 16 bit modes\n");

}

#else /* OLD_DMA_CODE */

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
#endif /* OLD_DMA_CODE */



